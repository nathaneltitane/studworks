#include "sw_global.h"
#include "sw_file_zip.h"
#include "sw_file.h"
#include "sw_math.h"
#include <zlib.h>
#include <time.h>

#if MAX_MEM_LEVEL >= 8
#  define DEF_MEM_LEVEL 8
#else
#  define DEF_MEM_LEVEL  MAX_MEM_LEVEL
#endif

swZipFile::swZipFile()
{
	mModified = false;
}

swZipFile::~swZipFile()
{
}

bool swZipFile::OpenRead(const QString& FileName)
{
	std::unique_ptr<swDiskFile> File(new swDiskFile(FileName));

	if (!File->Open(QIODevice::ReadOnly))
		return false;

	return OpenRead(std::move(File));
}

bool swZipFile::OpenRead(std::unique_ptr<swFile> File)
{
	mFile = std::move(File);

	if (!Open())
	{
		mFile = nullptr;
		return false;
	}

	return true;
}

bool swZipFile::OpenWrite(const QString& FileName)
{
	std::unique_ptr<swDiskFile> File(new swDiskFile(FileName));

	if (!File->Open(QIODevice::WriteOnly))
		return false;

	mFile = std::move(File);

	mNumEntries = 0;
	mCentralDirSize = 0;
	mCentralDirOffset = 0;
	mBytesBeforeZipFile = 0;
	mCentralPos = 0;

	return true;
}

bool swZipFile::AddFile(const char* FileName, const QByteArray& Data, quint32 DosDate)
{
	if (!mFile)
		return false;

	z_stream Stream = {};

	if (deflateInit2(&Stream, Z_BEST_COMPRESSION, Z_DEFLATED, -MAX_WBITS, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY) != Z_OK)
		return false;

	QByteArray Compressed(static_cast<int>(deflateBound(&Stream, static_cast<uLong>(Data.size()))), Qt::Uninitialized);

	Stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(Data.constData()));
	Stream.avail_in = static_cast<uInt>(Data.size());
	Stream.next_out = reinterpret_cast<Bytef*>(Compressed.data());
	Stream.avail_out = static_cast<uInt>(Compressed.size());

	const int Result = deflate(&Stream, Z_FINISH);
	Compressed.resize(static_cast<int>(Stream.total_out));
	deflateEnd(&Stream);

	if (Result != Z_STREAM_END)
		return false;

	swZipFileInformation FileInfo = {};
	const size_t NameLength = strlen(FileName);

	if (NameLength >= sizeof(FileInfo.file_name))
		return false;

	memcpy(FileInfo.file_name, FileName, NameLength);
	FileInfo.version = 20;
	FileInfo.version_needed = 20;
	FileInfo.compression_method = Z_DEFLATED;
	FileInfo.dosDate = DosDate;
	FileInfo.crc = crc32(0, reinterpret_cast<const Bytef*>(Data.constData()), static_cast<uInt>(Data.size()));
	FileInfo.compressed_size = Compressed.size();
	FileInfo.uncompressed_size = Data.size();
	FileInfo.size_filename = static_cast<quint16>(NameLength);
	FileInfo.offset_curfile = mFile->GetPosition();

	// local file header
	mFile->WriteU32(0x04034b50);
	mFile->WriteU16(FileInfo.version_needed);
	mFile->WriteU16(FileInfo.flag);
	mFile->WriteU16(FileInfo.compression_method);
	mFile->WriteU32(FileInfo.dosDate);
	mFile->WriteU32(FileInfo.crc);
	mFile->WriteU32(static_cast<quint32>(FileInfo.compressed_size));
	mFile->WriteU32(static_cast<quint32>(FileInfo.uncompressed_size));
	mFile->WriteU16(FileInfo.size_filename);
	mFile->WriteU16(FileInfo.size_file_extra);

	if (mFile->WriteBuffer(FileName, NameLength) != NameLength || mFile->WriteBuffer(Compressed.constData(), Compressed.size()) != static_cast<size_t>(Compressed.size()))
		return false;

	mFiles.push_back(FileInfo);
	mModified = true;

	return true;
}

bool swZipFile::CloseWrite()
{
	if (!mFile)
		return false;

	mCentralDirOffset = mFile->GetPosition();

	for (const swZipFileInformation& FileInfo : mFiles)
	{
		// central directory header
		mFile->WriteU32(0x02014b50);
		mFile->WriteU16(FileInfo.version);
		mFile->WriteU16(FileInfo.version_needed);
		mFile->WriteU16(FileInfo.flag);
		mFile->WriteU16(FileInfo.compression_method);
		mFile->WriteU32(FileInfo.dosDate);
		mFile->WriteU32(FileInfo.crc);
		mFile->WriteU32(static_cast<quint32>(FileInfo.compressed_size));
		mFile->WriteU32(static_cast<quint32>(FileInfo.uncompressed_size));
		mFile->WriteU16(FileInfo.size_filename);
		mFile->WriteU16(FileInfo.size_file_extra);
		mFile->WriteU16(FileInfo.size_file_comment);
		mFile->WriteU16(FileInfo.disk_num_start);
		mFile->WriteU16(FileInfo.internal_fa);
		mFile->WriteU32(FileInfo.external_fa);
		mFile->WriteU32(static_cast<quint32>(FileInfo.offset_curfile));

		if (mFile->WriteBuffer(FileInfo.file_name, FileInfo.size_filename) != FileInfo.size_filename)
			return false;
	}

	mCentralDirSize = mFile->GetPosition() - mCentralDirOffset;
	mNumEntries = mFiles.size();

	// end of central directory record
	mFile->WriteU32(0x06054b50);
	mFile->WriteU16(quint16(0));
	mFile->WriteU16(quint16(0));
	mFile->WriteU16(static_cast<quint16>(mNumEntries));
	mFile->WriteU16(static_cast<quint16>(mNumEntries));
	mFile->WriteU32(static_cast<quint32>(mCentralDirSize));
	mFile->WriteU32(static_cast<quint32>(mCentralDirOffset));
	const size_t Written = mFile->WriteBuffer("\0\0", 2);

	mFile->Close();
	mFile = nullptr;
	mModified = false;

	return Written == 2;
}

quint64 swZipFile::SearchCentralDir()
{
	quint64 SizeFile, MaxBack, BackRead, PosFound;
	constexpr int CommentBufferSize = 1024;
	quint8 buf[CommentBufferSize + 4];

	SizeFile = mFile->GetLength();
	MaxBack = swMinimum(SizeFile, 0xffffULL);
	BackRead = 4;
	PosFound = 0;

	while (BackRead < MaxBack)
	{
		quint64 ReadPos, ReadSize;

		if (BackRead + CommentBufferSize > MaxBack)
			BackRead = MaxBack;
		else
			BackRead += CommentBufferSize;
		ReadPos = SizeFile - BackRead;

		ReadSize = ((CommentBufferSize + 4) < (SizeFile - ReadPos)) ? (CommentBufferSize + 4) : (SizeFile - ReadPos);
		mFile->Seek(ReadPos, SEEK_SET);

		if (mFile->ReadBuffer(buf, (long)ReadSize) != ReadSize)
			break;

		for (int i = (int)ReadSize - 3; (i--) > 0;)
		{
			if (((*(buf+i)) == 0x50) && ((*(buf+i+1)) == 0x4b) && ((*(buf+i+2)) == 0x05) && ((*(buf+i+3)) == 0x06))
			{
				PosFound = ReadPos + i;
				break;
			}
		}

		if (PosFound != 0)
			break;
	}

	return PosFound;
}

quint64 swZipFile::SearchCentralDir64()
{
	quint64 SizeFile, MaxBack, BackRead, PosFound;
	const int CommentBufferSize = 1024;
	quint8 buf[CommentBufferSize + 4];

	SizeFile = mFile->GetLength();
	MaxBack = swMinimum(SizeFile, 0xffffULL);
	BackRead = 4;
	PosFound = 0;

	while (BackRead < MaxBack)
	{
		quint64 ReadPos, ReadSize;

		if (BackRead + CommentBufferSize > MaxBack)
			BackRead = MaxBack;
		else
			BackRead += CommentBufferSize;
		ReadPos = SizeFile - BackRead;

		ReadSize = ((CommentBufferSize + 4) < (SizeFile - ReadPos)) ? (CommentBufferSize + 4) : (SizeFile - ReadPos);
		mFile->Seek(ReadPos, SEEK_SET);

		if (mFile->ReadBuffer(buf, (long)ReadSize) != ReadSize)
			break;

		for (int i=(int)ReadSize - 3; (i--) > 0; )
		{
			if (((*(buf+i)) == 0x50) && ((*(buf+i+1)) == 0x4b) && ((*(buf+i+2)) == 0x06) && ((*(buf+i+3)) == 0x07))
			{
				PosFound = ReadPos + i;
				break;
			}
		}

		if (PosFound != 0)
			break;
	}

	if (PosFound == 0)
		return 0;

	mFile->Seek(PosFound, SEEK_SET);

	quint32 Number;
	quint64 RelativeOffset;

	// signature
	if (mFile->ReadU32(&Number, 1) != 1)
		return 0;

	// disk with the zip64 end record
	if (mFile->ReadU32(&Number, 1) != 1)
		return 0;

	if (Number != 0)
		return 0;

	// zip64 end record offset
	if (mFile->ReadU64(&RelativeOffset, 1) != 1)
		return 0;

	// total number of disks
	if (mFile->ReadU32(&Number, 1) != 1)
		return 0;

	if (Number != 0)
		return 0;

	// go to the end record
	mFile->Seek(RelativeOffset, SEEK_SET);

	// signature
	if (mFile->ReadU32(&Number, 1) != 1)
		return 0;

	if (Number != 0x06064b50)
		return 0;

	return RelativeOffset;
}

bool swZipFile::CheckFileCoherencyHeader(int FileIndex, quint32* SizeVar, quint64* OffsetLocalExtraField, quint32* SizeLocalExtraField)
{
	quint16 Number16, Flags;
	quint32 Number32, Magic;
	quint16 SizeFilename, SizeExtraField;
	const swZipFileInformation& FileInfo = mFiles[FileIndex];

	*SizeVar = 0;
	*OffsetLocalExtraField = 0;
	*SizeLocalExtraField = 0;

	mFile->Seek(FileInfo.offset_curfile + mBytesBeforeZipFile, SEEK_SET);

	if (mFile->ReadU32(&Magic, 1) != 1 || Magic != 0x04034b50)
		return false;

	if (mFile->ReadU16(&Number16, 1) != 1)
		return false;

	if (mFile->ReadU16(&Flags, 1) != 1)
		return false;

	if (mFile->ReadU16(&Number16, 1) != 1 || Number16 != FileInfo.compression_method)
		return false;

	if (FileInfo.compression_method != 0 && FileInfo.compression_method != Z_DEFLATED)
		return false;

	if (mFile->ReadU32(&Number32, 1) != 1)
		return false;

	if (mFile->ReadU32(&Number32, 1) != 1 || ((Number32 != FileInfo.crc) && ((Flags & 8)==0)))
		return false;

	if (mFile->ReadU32(&Number32, 1) != 1 || (Number32 != 0xffffffffU && (Number32 != FileInfo.compressed_size) && ((Flags & 8)==0)))
		return false;

	if (mFile->ReadU32(&Number32, 1) != 1 || (Number32 != 0xffffffffU && (Number32 != FileInfo.uncompressed_size) && ((Flags & 8)==0)))
		return false;

	if (mFile->ReadU16(&SizeFilename, 1) != 1 || SizeFilename != FileInfo.size_filename)
		return false;

	*SizeVar += SizeFilename;

	if (mFile->ReadU16(&SizeExtraField, 1) != 1)
		return false;

	*OffsetLocalExtraField= FileInfo.offset_curfile + 0x1e + SizeFilename;
	*SizeLocalExtraField = SizeExtraField;

	*SizeVar += SizeExtraField;

	return true;
}

bool swZipFile::Open()
{
	quint64 NumberEntriesCD, CentralPos;

	CentralPos = SearchCentralDir64();

	if (CentralPos)
	{
		quint32 NumberDisk, NumberDiskWithCD;

		mZip64 = true;

		// skip signature, size and versions
		mFile->Seek(CentralPos + 4 + 8 + 2 + 2, SEEK_SET);

		// number of this disk
		if (mFile->ReadU32(&NumberDisk, 1) != 1)
			return false;

		// disk with the central directory
		if (mFile->ReadU32(&NumberDiskWithCD, 1) != 1)
			return false;

		// entries on this disk
		if (mFile->ReadU64(&mNumEntries, 1) != 1)
			return false;

		// total entries
		if (mFile->ReadU64(&NumberEntriesCD, 1) != 1)
			return false;

		if ((NumberEntriesCD != mNumEntries) || (NumberDiskWithCD != 0) || (NumberDisk != 0))
			return false;

		// central directory size
		if (mFile->ReadU64(&mCentralDirSize, 1) != 1)
			return false;

		// central directory offset
		if (mFile->ReadU64(&mCentralDirOffset, 1) != 1)
			return false;

//		us.gi.size_comment = 0;
	}
	else
	{
		quint16 NumberDisk, NumberDiskWithCD;
		quint16 Number16;
		quint32 Number32;

		CentralPos = SearchCentralDir();
		if (CentralPos == 0)
			return false;

		mZip64 = false;

		// skip signature
		mFile->Seek(CentralPos + 4, SEEK_SET);

		// number of this disk
		if (mFile->ReadU16(&NumberDisk, 1) != 1)
			return false;

		// disk with the central directory
		if (mFile->ReadU16(&NumberDiskWithCD, 1) != 1)
			return false;

		// entries on this disk
		if (mFile->ReadU16(&Number16, 1) != 1)
			return false;
		mNumEntries = Number16;

		// total entries
		if (mFile->ReadU16(&Number16, 1) != 1)
			return false;
		NumberEntriesCD = Number16;

		if ((NumberEntriesCD != mNumEntries) || (NumberDiskWithCD != 0) || (NumberDisk != 0))
			return false;

		// central directory size
		if (mFile->ReadU32(&Number32, 1) != 1)
			return false;
		mCentralDirSize = Number32;

		// central directory offset
		if (mFile->ReadU32(&Number32, 1) != 1)
			return false;
		mCentralDirOffset= Number32;

		// zipfile comment length.
//		if (mFile->ReadU16(&us.z_filefunc, us.filestream,&us.gi.size_comment)!=1)
//			return false;
	}

	if (CentralPos < mCentralDirOffset + mCentralDirSize)
		return false;

	mBytesBeforeZipFile = CentralPos - (mCentralDirOffset + mCentralDirSize);
	mCentralPos = CentralPos;

	return ReadCentralDir();
}

bool swZipFile::ReadCentralDir()
{
	quint64 PosInCentralDir = mCentralDirOffset;

	mFile->Seek(PosInCentralDir + mBytesBeforeZipFile, SEEK_SET);
	mFiles.reserve(mNumEntries);

	for (quint64 FileNum = 0; FileNum < mNumEntries; FileNum++)
	{
		quint32 Magic, Number32;
		swZipFileInformation& FileInfo = mFiles.emplace_back();
		long Seek = 0;

		FileInfo.write_buffer = nullptr;
		FileInfo.deleted = false;

		if (mFile->ReadU32(&Magic, 1) != 1 || Magic != 0x02014b50)
			return false;

		if (mFile->ReadU16(&FileInfo.version, 1) != 1)
			return false;

		if (mFile->ReadU16(&FileInfo.version_needed, 1) != 1)
			return false;

		if (mFile->ReadU16(&FileInfo.flag, 1) != 1)
			return false;

		if (mFile->ReadU16(&FileInfo.compression_method, 1) != 1)
			return false;

		if (mFile->ReadU32(&FileInfo.dosDate, 1) != 1)
			return false;

		quint32 Date = FileInfo.dosDate >> 16;
		FileInfo.tmu_date.tm_mday = (quint32)(Date & 0x1f);
		FileInfo.tmu_date.tm_mon = (quint32)((((Date) & 0x1E0) / 0x20) - 1);
		FileInfo.tmu_date.tm_year = (quint32)(((Date & 0x0FE00) / 0x0200) + 1980);

		FileInfo.tmu_date.tm_hour = (quint32)((FileInfo.dosDate & 0xF800) / 0x800);
		FileInfo.tmu_date.tm_min = (quint32)((FileInfo.dosDate & 0x7E0) / 0x20);
		FileInfo.tmu_date.tm_sec = (quint32)(2*(FileInfo.dosDate & 0x1f));

		if (mFile->ReadU32(&FileInfo.crc, 1) != 1)
			return false;

		if (mFile->ReadU32(&Number32, 1) != 1)
			return false;
		FileInfo.compressed_size = Number32;

		if (mFile->ReadU32(&Number32, 1) != 1)
			return false;
		FileInfo.uncompressed_size = Number32;

		if (mFile->ReadU16(&FileInfo.size_filename, 1) != 1)
			return false;

		if (mFile->ReadU16(&FileInfo.size_file_extra, 1) != 1)
			return false;

		if (mFile->ReadU16(&FileInfo.size_file_comment, 1) != 1)
			return false;

		if (mFile->ReadU16(&FileInfo.disk_num_start, 1) != 1)
			return false;

		if (mFile->ReadU16(&FileInfo.internal_fa, 1) != 1)
			return false;

		if (mFile->ReadU32(&FileInfo.external_fa, 1) != 1)
			return false;

		// relative offset of local header
		if (mFile->ReadU32(&Number32, 1) != 1)
			return false;
		FileInfo.offset_curfile = Number32;

		Seek += FileInfo.size_filename;

		quint32 SizeRead;
		if (FileInfo.size_filename < sizeof(FileInfo.file_name) - 1)
		{
			*(FileInfo.file_name + FileInfo.size_filename) = '\0';
			SizeRead = FileInfo.size_filename;
		}
		else
		{
			*(FileInfo.file_name + sizeof(FileInfo.file_name) - 1) = '\0';
			SizeRead = sizeof(FileInfo.file_name) - 1;
		}

		if (FileInfo.size_filename > 0)
			if (mFile->ReadBuffer(FileInfo.file_name, SizeRead) != SizeRead)
				return false;
		Seek -= SizeRead;
/*
		// read the extra field
		if ((err==UNZ_OK) && (extraField!=nullptr))
		{
			ZPOS64_T uSizeRead ;
			if (file_info.size_file_extra<extraFieldBufferSize)
				uSizeRead = file_info.size_file_extra;
			else
				uSizeRead = extraFieldBufferSize;

			if (lSeek!=0)
			{
				if (ZSEEK64(s->z_filefunc, s->filestream,lSeek,ZLIB_FILEFUNC_SEEK_CUR)==0)
					lSeek=0;
				else
					err=UNZ_ERRNO;
			}

			if ((file_info.size_file_extra>0) && (extraFieldBufferSize>0))
				if (ZREAD64(s->z_filefunc, s->filestream,extraField,(uLong)uSizeRead)!=uSizeRead)
					err=UNZ_ERRNO;

			lSeek += file_info.size_file_extra - (uLong)uSizeRead;
		}
		else
			lSeek += file_info.size_file_extra;
*/
		Seek += FileInfo.size_file_extra;

		if (FileInfo.size_file_extra != 0)
		{
			quint32 acc = 0;

			// move back before the extra field
			Seek -= FileInfo.size_file_extra;

			if (Seek != 0)
			{
				mFile->Seek(Seek, SEEK_CUR);
				Seek = 0;
			}

			while (acc < FileInfo.size_file_extra)
			{
				quint16 HeaderId, DataSize;

				if (mFile->ReadU16(&HeaderId, 1) != 1)
					return false;

				if (mFile->ReadU16(&DataSize, 1) != 1)
					return false;

				// zip64 extra fields
				if (HeaderId == 0x0001)
				{
					if (FileInfo.uncompressed_size == (quint64)(unsigned long)-1)
					{
						if (mFile->ReadU64(&FileInfo.uncompressed_size, 1) != 1)
							return false;
					}

					if (FileInfo.compressed_size == (quint64)(unsigned long)-1)
					{
						if (mFile->ReadU64(&FileInfo.compressed_size, 1) != 1)
							return false;
					}

					if (FileInfo.offset_curfile == (quint64)-1)
					{
						// relative header offset
						if (mFile->ReadU64(&FileInfo.offset_curfile, 1) != 1)
							return false;
					}

					if (FileInfo.disk_num_start == (quint16)-1)
					{
						// disk start number
						if (mFile->ReadU32(&Number32, 1) != 1)
							return false;
					}
				}
				else
				{
					mFile->Seek(DataSize, SEEK_CUR);
				}

				acc += 2 + 2 + DataSize;
			}
		}
/*
		if ((err==UNZ_OK) && (szComment!=nullptr))
		{
			uLong uSizeRead ;
			if (file_info.size_file_comment<commentBufferSize)
			{
				*(szComment+file_info.size_file_comment)='\0';
				uSizeRead = file_info.size_file_comment;
			}
			else
				uSizeRead = commentBufferSize;

			if (lSeek!=0)
			{
				if (ZSEEK64(s->z_filefunc, s->filestream,lSeek,ZLIB_FILEFUNC_SEEK_CUR)==0)
					lSeek=0;
				else
					err=UNZ_ERRNO;
			}

			if ((file_info.size_file_comment>0) && (commentBufferSize>0))
				if (ZREAD64(s->z_filefunc, s->filestream,szComment,uSizeRead)!=uSizeRead)
					err=UNZ_ERRNO;
			lSeek+=file_info.size_file_comment - uSizeRead;
		}
		else
			lSeek+=file_info.size_file_comment;
*/
		Seek += FileInfo.size_file_comment;

		mFile->Seek(Seek, SEEK_CUR);
	}

	return true;
}

std::optional<quint32> swZipFile::FindFileIndex(const char* FileName) const
{
	for (quint32 FileIdx = 0; FileIdx < mFiles.size(); FileIdx++)
	{
		const swZipFileInformation& FileInfo = mFiles[FileIdx];

		if (!qstricmp(FileInfo.file_name, FileName))
			return FileIdx;
	}

	return std::nullopt;
}

bool swZipFile::ExtractFile(const char* FileName, swMemoryFile& File, quint32 MaxLength)
{
	const std::optional<quint32> FileIndex = FindFileIndex(FileName);

	return FileIndex && ExtractFile(*FileIndex, File, MaxLength);
}

bool swZipFile::ExtractFile(quint32 FileIndex, swMemoryFile& File, quint32 MaxLength)
{
	QMutexLocker Lock(&mMutex);

	quint32 SizeVar;
	quint64 OffsetLocalExtraField;
	quint32 SizeLocalExtraField;
	const swZipFileInformation& FileInfo = mFiles[FileIndex];

	if (!CheckFileCoherencyHeader(FileIndex, &SizeVar, &OffsetLocalExtraField, &SizeLocalExtraField))
		return false;

	const int BufferSize = 16384;
	char ReadBuffer[BufferSize];
	z_stream Stream;
	quint32 Crc32;
	quint64 PosInZipfile;
	quint64 RestReadCompressed;
	quint64 RestReadUncompressed;

	Crc32 = 0;
	Stream.total_out = 0;

	if (FileInfo.compression_method == Z_DEFLATED)
	{
		Stream.zalloc = (alloc_func)0;
		Stream.zfree = (free_func)0;
		Stream.opaque = (voidpf)0;
		Stream.next_in = 0;
		Stream.avail_in = 0;

		int err = inflateInit2(&Stream, -MAX_WBITS);
		if (err != Z_OK)
			return false;
	}

	RestReadCompressed = FileInfo.compressed_size;
	RestReadUncompressed = FileInfo.uncompressed_size;
	PosInZipfile = FileInfo.offset_curfile + 0x1e + SizeVar;

	Stream.avail_in = (uInt)0;

	quint32 Length = swMinimum((quint32)FileInfo.uncompressed_size, MaxLength);
	File.SetLength(Length);
	File.Seek(0, SEEK_SET);

	Stream.next_in = (Bytef*)ReadBuffer;
	Stream.next_out = (Bytef*)File.mBuffer;
	Stream.avail_out = Length;

	quint32 Read = 0;

	while (Stream.avail_out > 0)
	{
		if ((Stream.avail_in == 0) && (RestReadCompressed > 0))
		{
			quint32 ReadThis = BufferSize;

			if (RestReadCompressed < ReadThis)
				ReadThis = (quint32)RestReadCompressed;

			if (ReadThis == 0)
				return false;

			mFile->Seek(PosInZipfile + mBytesBeforeZipFile, SEEK_SET);
			if (mFile->ReadBuffer(ReadBuffer, ReadThis) != ReadThis)
				return false;

			PosInZipfile += ReadThis;

			RestReadCompressed -= ReadThis;

			Stream.next_in = (Bytef*)ReadBuffer;
			Stream.avail_in = (uInt)ReadThis;
		}

		if (FileInfo.compression_method == 0)
		{
			quint32 DoCopy, i;

			if ((Stream.avail_in == 0) && (RestReadCompressed == 0))
				return (Read == 0) ? false : true;

			if (Stream.avail_out < Stream.avail_in)
				DoCopy = Stream.avail_out;
			else
				DoCopy = Stream.avail_in;

			for (i = 0; i < DoCopy; i++)
				*(Stream.next_out+i) = *(Stream.next_in+i);

			Crc32 = crc32(Crc32, Stream.next_out, DoCopy);
			RestReadUncompressed -= DoCopy;
			Stream.avail_in -= DoCopy;
			Stream.avail_out -= DoCopy;
			Stream.next_out += DoCopy;
			Stream.next_in += DoCopy;
			Stream.total_out += DoCopy;
			Read += DoCopy;
		}
		else
		{
			quint64 TotalOutBefore, TotalOutAfter;
			const Bytef *bufBefore;
			quint64 OutThis;
			int flush = Z_SYNC_FLUSH;

			TotalOutBefore = Stream.total_out;
			bufBefore = Stream.next_out;

			int err = inflate(&Stream,flush);

			if ((err >= 0) && (Stream.msg != nullptr))
				err = Z_DATA_ERROR;

			TotalOutAfter = Stream.total_out;
			OutThis = TotalOutAfter - TotalOutBefore;

			Crc32 = crc32(Crc32, bufBefore, (uInt)(OutThis));

			RestReadUncompressed -= OutThis;

			Read += (uInt)(TotalOutAfter - TotalOutBefore);

			if (err != Z_OK)
			{
				inflateEnd(&Stream);

				if (RestReadUncompressed == 0)
				{
					if (Crc32 != FileInfo.crc)
						return false;
				}

				if (err == Z_STREAM_END)
					return (Read == 0) ? false : true;

				return false;
			}
		}
	}

	if (FileInfo.compression_method == Z_DEFLATED)
		inflateEnd(&Stream);

	return true;
}
