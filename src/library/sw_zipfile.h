#pragma once

#ifdef DeleteFile
#undef DeleteFile
#endif

class swFile;

// Date/time info.
struct tm_unz
{
	quint32 tm_sec;            // seconds after the minute - [0,59]
	quint32 tm_min;            // minutes after the hour - [0,59]
	quint32 tm_hour;           // hours since midnight - [0,23]
	quint32 tm_mday;           // day of the month - [1,31]
	quint32 tm_mon;            // months since January - [0,11]
	quint32 tm_year;           // years - [1980..2044]
};

// Information about a file in the zipfile.
struct swZipFileInfo
{
	quint16 version;              // version made by                 2 bytes
	quint16 version_needed;       // version needed to extract       2 bytes
	quint16 flag;                 // general purpose bit flag        2 bytes
	quint16 compression_method;   // compression method              2 bytes
	quint32 dosDate;              // last mod file date in Dos fmt   4 bytes
	quint32 crc;                  // crc-32                          4 bytes
	quint64 compressed_size;      // compressed size                 8 bytes
	quint64 uncompressed_size;    // uncompressed size               8 bytes
	quint16 size_filename;        // filename length                 2 bytes
	quint16 size_file_extra;      // extra field length              2 bytes
	quint16 size_file_comment;    // file comment length             2 bytes

	quint16 disk_num_start;       // disk number start               2 bytes
	quint16 internal_fa;          // internal file attributes        2 bytes
	quint32 external_fa;          // external file attributes        4 bytes

	quint64 offset_curfile;       // relative offset of local header 8 bytes
	char file_name[256];
	tm_unz tmu_date;

	swMemFile* write_buffer;
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

	// Returns the first case-insensitive filename match in archive order.
	std::optional<quint32> FindFileIndex(const char* FileName) const;

	bool ExtractFile(quint32 FileIndex, swMemFile& File, quint32 MaxLength = 0xffffffff);
	bool ExtractFile(const char* FileName, swMemFile& File, quint32 MaxLength = 0xffffffff);

	std::vector<swZipFileInfo> mFiles;

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

