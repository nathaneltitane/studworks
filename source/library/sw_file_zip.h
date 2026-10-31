#pragma once

#ifdef DeleteFile
#undef DeleteFile
#endif

class swFile;

// Date/time info.
struct tm_unz
{
	// Seconds, 0 to 59.
	quint32 tm_sec;

	// Minutes, 0 to 59.
	quint32 tm_min;

	// Hours, 0 to 23.
	quint32 tm_hour;

	// Day of the month, 1 to 31.
	quint32 tm_mday;

	// Months since January, 0 to 11.
	quint32 tm_mon;

	// Years, 1980 to 2044.
	quint32 tm_year;
};

// Information about a file in the zipfile.
struct swZipFileInformation
{
	// Version made by, 2 bytes.
	quint16 version;

	// Version needed to extract, 2 bytes.
	quint16 version_needed;

	// General purpose flags, 2 bytes.
	quint16 flag;

	// Compression method, 2 bytes.
	quint16 compression_method;

	// Last modified dos date, 4 bytes.
	quint32 dosDate;

	// Crc 32, 4 bytes.
	quint32 crc;

	// Compressed size, 8 bytes.
	quint64 compressed_size;

	// Uncompressed size, 8 bytes.
	quint64 uncompressed_size;

	// File name length, 2 bytes.
	quint16 size_filename;

	// Extra field length, 2 bytes.
	quint16 size_file_extra;

	// File comment length, 2 bytes.
	quint16 size_file_comment;

	// Disk number start, 2 bytes.
	quint16 disk_num_start;

	// Internal file attributes, 2 bytes.
	quint16 internal_fa;

	// External file attributes, 4 bytes.
	quint32 external_fa;

	// Local header offset, 8 bytes.
	quint64 offset_curfile;
	char file_name[256];
	tm_unz tmu_date;

	swMemoryFile* write_buffer;
	bool deleted;
};

class swZipFile
{
public:
	swZipFile();
	~swZipFile();

	swZipFile(const swZipFile&) = delete;
	swZipFile(swZipFile&&) = delete;
	swZipFile& operator=(const swZipFile&) = delete;
	swZipFile& operator=(swZipFile&&) = delete;

	bool OpenRead(const QString& FileName);
	bool OpenRead(std::unique_ptr<swFile> File);
	bool OpenWrite(const QString& FileName);

	// Use to deflate a file into an archive opened for writing.
	bool AddFile(const char* FileName, const QByteArray& Data, quint32 DosDate);
	bool CloseWrite();

	// Returns the first case-insensitive filename match in archive order.
	std::optional<quint32> FindFileIndex(const char* FileName) const;

	bool ExtractFile(quint32 FileIndex, swMemoryFile& File, quint32 MaxLength = 0xffffffff);
	bool ExtractFile(const char* FileName, swMemoryFile& File, quint32 MaxLength = 0xffffffff);

	std::vector<swZipFileInformation> mFiles;

protected:
	bool Open();
	bool ReadCentralDir();
	quint64 SearchCentralDir();
	quint64 SearchCentralDir64();
	bool CheckFileCoherencyHeader(int FileIndex, quint32* SizeVar, quint64* OffsetLocalExtraField, quint32* SizeLocalExtraField);

	QMutex mMutex;
	std::unique_ptr<swFile> mFile;

	bool mModified;
	bool mZip64;
	quint64 mNumEntries;
	quint64 mCentralDirSize;
	quint64 mCentralDirOffset;
	quint64 mBytesBeforeZipFile;
	quint64 mCentralPos;
};

