#include "sw_global.h"
#include "sw_library.h"
#include "sw_file_zip.h"
#include "sw_file.h"
#include "sw_data_part.h"
#include "sw_manager_part.h"
#include "sw_project.h"
#include "sw_string.h"

// Cache payloads use Qt compression with checksum validation.
constexpr quint32 SW_LIBRARY_CACHE_VERSION = 0x0114;

void swPartsLibrary::ReadArchiveDescriptions(const QString& OfficialFileName, const QString& UnofficialFileName)
{
	QFileInfo OfficialInfo(OfficialFileName);
	QFileInfo UnofficialInfo(UnofficialFileName);

	mArchiveCheckSum[0] = OfficialInfo.size();
	mArchiveCheckSum[1] = OfficialInfo.lastModified().toMSecsSinceEpoch();

	if (!UnofficialFileName.isEmpty())
	{
		mArchiveCheckSum[2] = UnofficialInfo.size();
		mArchiveCheckSum[3] = UnofficialInfo.lastModified().toMSecsSinceEpoch();
	}
	else
	{
		mArchiveCheckSum[2] = 0;
		mArchiveCheckSum[3] = 0;
	}

	QString IndexFileName = QFileInfo(QDir(mCachePath), QLatin1String("index")).absoluteFilePath();

	if (!LoadCacheIndex(IndexFileName))
	{
		swMemoryFile PartFile;

		for (swPartData* PartData : mPartManager->GetLibraryPartData())
		{
			mZipFiles[static_cast<int>(PartData->mZipFileType)]->ExtractFile(PartData->mZipFileIndex, PartFile, 256);
			PartFile.Seek(0, SEEK_END);
			PartFile.WriteU8(0);

			char* Src = (char*)PartFile.mBuffer + 2;
			char* Dst = PartData->m_strDescription;

			for (;;)
			{
				if (*Src != '\r' && *Src != '\n' && *Src && Dst - PartData->m_strDescription < (int)sizeof(PartData->m_strDescription) - 1)
				{
					*Dst++ = *Src++;
					continue;
				}

				*Dst = 0;
				break;
			}
		}

		ClassifyParts(DirectoryFilePaths());
		SaveArchiveCacheIndex(IndexFileName);
	}
}

void swPartsLibrary::ReadDirectoryDescriptions(const QFileInfoList (&FileLists)[static_cast<int>(swLibraryFolderType::Count)], bool ShowProgress)
{
	QString IndexFileName = QFileInfo(QDir(mCachePath), QLatin1String("index")).absoluteFilePath();
	swMemoryFile IndexFile;

	struct CachedDescription
	{
		swPartData::CacheData CacheData;
		quint8 FolderType;
		quint64 FileTime;
	};

	std::map<std::string, CachedDescription> CachedDescriptions;

	if (ReadCacheFile(IndexFileName, IndexFile, CacheType::Directory) && IndexFile.ReadQString() == mLibraryDir.absolutePath())
	{
		quint32 Count;

		if (IndexFile.ReadU32(&Count, 1) == 1)
		{
			for (quint32 Index = 0; Index < Count; Index++)
			{
				std::string FileName;
				CachedDescription Entry;

				if (!ReadIndexString(IndexFile, FileName) || FileName.empty() ||
					!ReadIndexString(IndexFile, Entry.CacheData.Description) || !ReadIndexString(IndexFile, Entry.CacheData.PatternParentId) ||
					IndexFile.ReadU8(&Entry.CacheData.Flags, 1) != 1 ||
					IndexFile.ReadU8(&Entry.FolderType, 1) != 1 || IndexFile.ReadU64(&Entry.FileTime, 1) != 1 ||
					Entry.FolderType >= static_cast<quint8>(swLibraryFolderType::Count) ||
					!CachedDescriptions.emplace(std::move(FileName), std::move(Entry)).second)
				{
					CachedDescriptions.clear();
					break;
				}
			}
		}
	}

	struct DescriptionJob
	{
		swPartData* PartData;
		QFileInfo FileInfo;
		swLibraryFolderType FolderType;
		bool Modified = false;
	};

	std::vector<DescriptionJob> Jobs;

	for (int FolderIdx = 0; FolderIdx < static_cast<int>(swLibraryFolderType::Count); FolderIdx++)
	{
		const QFileInfoList& FileList = FileLists[FolderIdx];

		for (int FileIdx = 0; FileIdx < FileList.size(); FileIdx++)
		{
			QByteArray FileString = FileList[FileIdx].fileName().toLatin1();
			const std::string Name = NormalizePartDataName(FileString.constData());

			if (FolderIdx > 0 && mPartManager->FindPartData(Name, nullptr))
				continue;

			swPartData* PartData = mPartManager->CreateLibraryPartData(Name, FileString.constData());

			Jobs.push_back({ PartData, FileList[FileIdx], static_cast<swLibraryFolderType>(FolderIdx) });
		}
	}

	// Only process entries retained by the normalized registry after discovery.
	Jobs.erase(std::remove_if(Jobs.begin(), Jobs.end(), [this](const DescriptionJob& Job)
	{
		return FindPartData(Job.FileInfo.fileName().toLatin1().constData(), nullptr) != Job.PartData;
	}), Jobs.end());

	QAtomicInt FilesLoaded;

	auto ReadDescriptions = [&CachedDescriptions, &FilesLoaded](DescriptionJob& Job)
	{
		FilesLoaded.ref();

		swPartData* PartData = Job.PartData;
		swDiskFile PartFile(Job.FileInfo.absoluteFilePath());
		char Line[1024];

		const auto CachedIt = CachedDescriptions.find(PartData->mFileName);
		const quint64 FileTime = Job.FileInfo.lastModified().toMSecsSinceEpoch();

		if (CachedIt != CachedDescriptions.end() && CachedIt->second.FileTime == FileTime && CachedIt->second.FolderType == static_cast<quint8>(Job.FolderType) &&
			PartData->InitializeFromCache(CachedIt->second.CacheData))
			return;

		Job.Modified = true;

		if (!PartFile.Open(QIODevice::ReadOnly) || !PartFile.ReadLine(Line, sizeof(Line)))
		{
			swstrcpy(PartData->m_strDescription, "Unknown");
			return;
		}

		const char* Src = Line + 2;
		char* Dst = PartData->m_strDescription;

		for (;;)
		{
			if (*Src != '\r' && *Src != '\n' && *Src && Dst - PartData->m_strDescription < (int)sizeof(PartData->m_strDescription) - 1)
			{
				*Dst++ = *Src++;
				continue;
			}

			*Dst = 0;
			break;
		}
	};

	QProgressDialog* ProgressDialog = new QProgressDialog(nullptr);
	ProgressDialog->setWindowFlags(ProgressDialog->windowFlags() & ~Qt::WindowCloseButtonHint);
	ProgressDialog->setWindowTitle(tr("Initializing"));
	ProgressDialog->setLabelText(tr("Loading Parts Library"));
	ProgressDialog->setMaximum(static_cast<int>(Jobs.size()));
	ProgressDialog->setMinimum(0);
	ProgressDialog->setValue(0);
	ProgressDialog->setCancelButton(nullptr);
	ProgressDialog->setAutoReset(false);
	if (ShowProgress)
		ProgressDialog->show();

	QFuture<void> LoadFuture = QtConcurrent::map(Jobs, ReadDescriptions);

	while (!LoadFuture.isFinished())
	{
		ProgressDialog->setValue(FilesLoaded);
		QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
	}

	ProgressDialog->setValue(FilesLoaded);
	QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

	ProgressDialog->deleteLater();

	// A target's change, addition or removal can invalidate otherwise unchanged aliases.
	const bool Modified = std::any_of(Jobs.begin(), Jobs.end(), [](const DescriptionJob& Job)
	{
		return Job.Modified;
	});

	if (Modified || CachedDescriptions.size() != Jobs.size())
	{
		DirectoryFilePaths FilePaths;

		for (const DescriptionJob& Job : Jobs)
			FilePaths.emplace(Job.PartData, Job.FileInfo.absoluteFilePath());

		ClassifyParts(FilePaths);

		swMemoryFile NewIndexFile;

		NewIndexFile.WriteQString(mLibraryDir.absolutePath());
		NewIndexFile.WriteU32(static_cast<quint32>(Jobs.size()));

		for (const DescriptionJob& Job : Jobs)
		{
			const swPartData* PartData = Job.PartData;
			const swPartData::CacheData CacheData = PartData->GetCacheData();

			if (!WriteIndexString(NewIndexFile, PartData->mFileName) ||
				!WriteIndexString(NewIndexFile, CacheData.Description) || !WriteIndexString(NewIndexFile, CacheData.PatternParentId))
				return;

			NewIndexFile.WriteU8(CacheData.Flags);
			NewIndexFile.WriteU8(static_cast<quint8>(Job.FolderType));
			const quint64 FileTime = Job.FileInfo.lastModified().toMSecsSinceEpoch();
			NewIndexFile.WriteU64(FileTime);
		}

		WriteCacheFile(IndexFileName, NewIndexFile, CacheType::Directory);
	}
}

bool swPartsLibrary::ReadCacheFile(const QString& FileName, swMemoryFile& CacheFile, CacheType Type)
{
	QFile File(FileName);

	if (!File.open(QIODevice::ReadOnly))
		return false;

	quint32 CacheVersion, CacheFlags;

	if (File.read(reinterpret_cast<char*>(&CacheVersion), sizeof(CacheVersion)) != sizeof(CacheVersion) || CacheVersion != SW_LIBRARY_CACHE_VERSION)
		return false;

	if (File.read(reinterpret_cast<char*>(&CacheFlags), sizeof(CacheFlags)) != sizeof(CacheFlags) || CacheFlags != static_cast<quint32>(Type))
		return false;

	if (Type == CacheType::Archive)
	{
		qint64 CacheCheckSum[4];

		if (File.read(reinterpret_cast<char*>(CacheCheckSum), sizeof(CacheCheckSum)) != sizeof(CacheCheckSum) ||
			memcmp(CacheCheckSum, mArchiveCheckSum, sizeof(CacheCheckSum)))
			return false;
	}

	quint32 UncompressedSize;

	if (File.read(reinterpret_cast<char*>(&UncompressedSize), sizeof(UncompressedSize)) != sizeof(UncompressedSize))
		return false;

	const QByteArray Data = qUncompress(File.readAll());

	if (Data.isEmpty() || static_cast<quint64>(Data.size()) != UncompressedSize)
		return false;

	CacheFile.SetLength(Data.size());
	CacheFile.Seek(0, SEEK_SET);

	if (!CacheFile.mBuffer)
		return false;

	CacheFile.WriteBuffer(Data.constData(), Data.size());
	CacheFile.Seek(0, SEEK_SET);

	return true;
}

bool swPartsLibrary::WriteCacheFile(const QString& FileName, const swMemoryFile& CacheFile, CacheType Type)
{
	if (CacheFile.GetLength() > static_cast<size_t>(std::numeric_limits<int>::max()))
		return false;

	QFile File(FileName);

	if (!File.open(QIODevice::WriteOnly))
		return false;

	constexpr quint32 CacheVersion = SW_LIBRARY_CACHE_VERSION;
	const quint32 CacheFlags = static_cast<quint32>(Type);

	if (File.write(reinterpret_cast<const char*>(&CacheVersion), sizeof(CacheVersion)) != sizeof(CacheVersion) ||
		File.write(reinterpret_cast<const char*>(&CacheFlags), sizeof(CacheFlags)) != sizeof(CacheFlags))
		return false;

	if (Type == CacheType::Archive && File.write(reinterpret_cast<const char*>(mArchiveCheckSum), sizeof(mArchiveCheckSum)) != sizeof(mArchiveCheckSum))
		return false;

	const quint32 UncompressedSize = static_cast<quint32>(CacheFile.GetLength());

	if (File.write(reinterpret_cast<const char*>(&UncompressedSize), sizeof(UncompressedSize)) != sizeof(UncompressedSize))
		return false;

	const QByteArray CompressedData = qCompress(CacheFile.mBuffer, static_cast<int>(CacheFile.GetLength()));

	return !CompressedData.isEmpty() && File.write(CompressedData) == CompressedData.size();
}

bool swPartsLibrary::ReadIndexString(swMemoryFile& File, std::string& String)
{
	quint8 Length;

	if (File.ReadU8(&Length, 1) != 1)
		return false;

	String.resize(Length);

	return File.ReadBuffer(String.data(), Length) == Length;
}

bool swPartsLibrary::WriteIndexString(swMemoryFile& File, const std::string& String)
{
	if (String.size() > 255)
		return false;

	const quint8 Length = static_cast<quint8>(String.size());

	return File.WriteU8(&Length, 1) == 1 && File.WriteBuffer(String.data(), Length) == Length;
}

std::string swPartsLibrary::ReadAliasTarget(swFile& File)
{
	std::string Target;
	char Buffer[1024];

	while (File.ReadLine(Buffer, sizeof(Buffer)))
	{
		QString Line = QString::fromLatin1(Buffer).trimmed();

		if (Line.isEmpty())
			continue;

		QTextStream Stream(&Line, QIODevice::ReadOnly);
		int LineType;

		Stream >> LineType;

		if (Stream.status() != QTextStream::Ok)
			return std::string();

		if (LineType == 0)
			continue;

		// Aliases contain exactly one reference and no direct geometry.
		if (LineType != 1 || !Target.empty())
			return std::string();

		int Color;
		Stream >> Color;

		for (int ValueIndex = 0; ValueIndex < 12; ValueIndex++)
		{
			float Value;
			Stream >> Value;
		}

		if (Stream.status() != QTextStream::Ok)
			return std::string();

		const QByteArray Name = Stream.readAll().trimmed().toLatin1();

		if (Name.isEmpty() || Name.size() >= SW_PART_NAME_LENGTH)
			return std::string();

		Target = NormalizePartDataName(Name.constData());
	}

	return Target;
}

void swPartsLibrary::ClassifyParts(const DirectoryFilePaths& FilePaths)
{
	const std::vector<swPartData*> Parts = mPartManager->GetLibraryPartData();
	std::unordered_map<swPartData*, swPartData*> AliasTargets;

	for (swPartData* PartData : Parts)
	{
		const std::string Name = NormalizePartDataName(PartData->mFileName);
		const std::vector<std::string> ParentIds = PartData->Classify(Name);

		if (PartData->IsAlias())
		{
			swFileSource Source;

			if (PartData->mZipFileType != swZipFileType::Count)
			{
				Source.ArchiveType = static_cast<int>(PartData->mZipFileType);
				Source.ArchiveIndex = PartData->mZipFileIndex;
			}
			else
			{
				const auto FileIt = FilePaths.find(PartData);

				if (FileIt == FilePaths.end())
				{
					AliasTargets.emplace(PartData, nullptr);
					continue;
				}

				Source.FilePath = FileIt->second;
			}

			const swResult<std::unique_ptr<swFile>> File = OpenFile(Source);
			swPartData* Target = nullptr;

			if (File)
			{
				const std::string TargetId = ReadAliasTarget(*File.value());

				if (!TargetId.empty())
					Target = FindPartData(TargetId.c_str(), nullptr);
			}

			AliasTargets.emplace(PartData, Target);
			continue;
		}

		for (const std::string& ParentId : ParentIds)
		{
			const swPartData* Parent = FindPartData(ParentId.c_str(), nullptr);

			if (Parent && Parent->IsLibraryPart())
			{
				PartData->SetPatternParentId(ParentId);
				break;
			}
		}
	}

	std::unordered_set<swPartData*> ResolvedAliases;

	for (const auto& [Alias, Target] : AliasTargets)
	{
		if (ResolvedAliases.find(Alias) != ResolvedAliases.end())
			continue;

		std::vector<swPartData*> Chain;
		swPartData* Current = Alias;
		bool Cycle = false;

		while (Current)
		{
			const auto TargetIt = AliasTargets.find(Current);

			if (TargetIt == AliasTargets.end() || ResolvedAliases.find(Current) != ResolvedAliases.end())
				break;

			if (std::find(Chain.begin(), Chain.end(), Current) != Chain.end())
			{
				Cycle = true;
				break;
			}

			Chain.push_back(Current);
			Current = TargetIt->second;
		}

		// Only the referencing alias inherits decoration from its target.
		for (auto PartIt = Chain.rbegin(); PartIt != Chain.rend(); PartIt++)
		{
			swPartData* PartData = *PartIt;
			PartData->SetAliasTarget(Cycle ? nullptr : Current);
			Current = PartData;
			ResolvedAliases.insert(PartData);
		}
	}
}

bool swPartsLibrary::LoadCacheIndex(const QString& FileName)
{
	swMemoryFile IndexFile;

	if (!ReadCacheFile(FileName, IndexFile, CacheType::Archive))
		return false;

	quint32 NumFiles;

	if (IndexFile.ReadU32(&NumFiles, 1) != 1 || NumFiles != mPartManager->GetLibraryPartDataCount())
		return false;

	for (swPartData* PartData : mPartManager->GetLibraryPartData())
	{
		swPartData::CacheData CacheData;

		if (!ReadIndexString(IndexFile, CacheData.Description) || !ReadIndexString(IndexFile, CacheData.PatternParentId) ||
			IndexFile.ReadU8(&CacheData.Flags, 1) != 1 || !PartData->InitializeFromCache(CacheData))
			return false;
	}

	return true;
}

bool swPartsLibrary::SaveArchiveCacheIndex(const QString& FileName)
{
	swMemoryFile IndexFile;

	const quint32 NumFiles = (quint32)mPartManager->GetLibraryPartDataCount();

	if (IndexFile.WriteBuffer((char*)&NumFiles, sizeof(NumFiles)) == 0)
		return false;

	for (const swPartData* PartData : mPartManager->GetLibraryPartData())
	{
		const swPartData::CacheData CacheData = PartData->GetCacheData();

		if (!WriteIndexString(IndexFile, CacheData.Description) || !WriteIndexString(IndexFile, CacheData.PatternParentId))
			return false;

		IndexFile.WriteU8(CacheData.Flags);
	}

	return WriteCacheFile(FileName, IndexFile, CacheType::Archive);
}
