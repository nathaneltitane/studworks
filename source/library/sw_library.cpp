#include "sw_global.h"
#include "sw_library.h"
#include "sw_loader_asset.h"
#include "sw_manager_thumbnail.h"
#include "sw_file_zip.h"
#include "sw_file.h"
#include "sw_data_piece.h"
#include "sw_manager_piece.h"
#include "sw_colors.h"
#include "sw_texture.h"
#include "sw_manager_texture.h"
#include "sw_category.h"
#include "sw_application.h"
#include "sw_context.h"
#include "sw_extensions_opengl.h"
#include "sw_synthesis.h"
#include "sw_track_train.h"
#include "sw_project.h"
#include "sw_profile.h"
#include "sw_loader_mesh.h"
#include "sw_model.h"
#include "sw_view.h"
#include "sw_string.h"

// Cache payloads use Qt compression with checksum validation.
constexpr quint32 SW_LIBRARY_CACHE_VERSION = 0x0114;
constexpr qint32 SW_LIBRARY_MESH_CACHE_VERSION = 2;

enum class swLibrarySourceType
{
	Library,
	StudStyle
};

struct swLibrarySource
{
	swLibrarySource() = default;

	swLibrarySource(const swLibrarySource&) = delete;
	swLibrarySource(swLibrarySource&&) = delete;
	swLibrarySource& operator=(const swLibrarySource&) = delete;
	swLibrarySource& operator=(swLibrarySource&&) = delete;

	swLibrarySourceType Type;
	std::map<std::string, std::unique_ptr<swLibraryPrimitive>> Primitives;
};

swLibraryPrimitive::swLibraryPrimitive(QString&& FileName, const char* Name, swZipFileType ZipFileType, quint32 ZipFileIndex, bool Stud, bool StudStyle, bool SubFile)
	: mFileName(std::move(FileName))
{
	swstrcpy(mName, Name);

	mZipFileType = ZipFileType;
	mZipFileIndex = ZipFileIndex;
	mState = swPrimitiveState::NotLoaded;
	mStud = Stud;
	mStudStyle = StudStyle;
	mSubFile = SubFile;
}

swPiecesLibrary::swPiecesLibrary(swStudStyle StudStyle, bool StudCylinderColorEnabled)
	: mStudStyle(StudStyle), mStudCylinderColorEnabled(StudCylinderColorEnabled)
{
	mPieceManager = std::make_unique<swPieceManager>();
	mTextureManager = std::make_unique<swTextureManager>();
	mAssetLoader = std::unique_ptr<swAssetLoader>(new swAssetLoader(this));
	mThumbnailManager = std::unique_ptr<swThumbnailManager>(new swThumbnailManager(this));
	mLoadingMesh = std::unique_ptr<swMesh>(new swMesh);
	mLoadingMesh->CreateBox();
	QStringList cachePathList = QStandardPaths::standardLocations(QStandardPaths::CacheLocation);
	mCachePath = cachePathList.first();

	QDir Dir;
	Dir.mkpath(mCachePath);

	mBuffersDirty = false;
	mHasUnofficial = false;
}

swPiecesLibrary::~swPiecesLibrary()
{
	mThumbnailManager.reset();
	mCancelLoading.store(true);
	mAssetLoader.reset();
	Unload();
	ReleaseBuffers();
}

void swPiecesLibrary::Unload()
{
	Q_ASSERT(QThread::currentThread() == thread());

	const bool HadLoader = static_cast<bool>(mAssetLoader);

	if (HadLoader && mThumbnailManager)
		mThumbnailManager->Clear();

	mAssetLoader.reset();
	mSynthPieces.clear();

	mPieceManager->Clear();

	mSources.clear();

	mTextureManager->Clear();
	mTextureDiskEntries.clear();

	for (std::map<QString, int>& Entries : mTextureArchiveEntries)
		Entries.clear();

	mOfficialPieceCount = 0;

	for (std::unique_ptr<swZipFile>& ZipFile : mZipFiles)
		ZipFile.reset();

	if (HadLoader)
		mAssetLoader = std::unique_ptr<swAssetLoader>(new swAssetLoader(this));
}

void swPiecesLibrary::RemoveTemporaryPieceData()
{
	mPieceManager->RemoveTemporaryPieceData();
}

void swPiecesLibrary::ReleaseProjectPieceData(Project* OwnerProject)
{
	mPieceManager->ReleaseProjectPieceData(OwnerProject);
}

void swPiecesLibrary::TransferProjectPieceData(Project* Source, Project* Destination)
{
	mPieceManager->TransferProjectPieceData(Source, Destination);
}

bool swPiecesLibrary::RenameModelPieceData(swPieceData* PieceData, const char* NewName)
{
	if (!mPieceManager->RenameModelPieceData(PieceData, NewName))
		return false;

	if (PieceData->mState != swAssetState::Failed)
		ClearPieceDataLoadError(PieceData);

	return true;
}

std::string swPiecesLibrary::NormalizePieceDataName(const char* PieceName)
{
	return swPieceManager::NormalizePieceDataName(PieceName);
}

swPieceDataReference swPiecesLibrary::AcquireModelPieceData(const char* PieceName, Project* Project, bool& Reused)
{
	swPieceData* PieceData = mPieceManager->AcquireModelPieceData(PieceName, Project, Reused);

	swPieceDataReference PieceDataRef = swPieceDataReference::Adopt(PieceData, this);

	if (Reused)
		InvalidatePieceDataLoad(PieceData);

	return PieceDataRef;
}

swResult<Project*> swPiecesLibrary::LoadExternalProject(const QFileInfo& FileInfo, bool Preview, bool DeferModelMeshRequests)
{
	static thread_local std::vector<QString> LoadingPaths;
	const QString Path = FileInfo.canonicalFilePath();

	if (std::find(LoadingPaths.begin(), LoadingPaths.end(), Path) != LoadingPaths.end())
		return swUnexpected(tr("Circular reference between external model files at '%1'.").arg(Path));

	LoadingPaths.push_back(Path);

	Project* ExternalProject = new Project(Preview);
	ExternalProject->SetDeferModelMeshRequests(DeferModelMeshRequests);
	const bool Loaded = ExternalProject->Load(FileInfo.absoluteFilePath(), false);

	LoadingPaths.pop_back();

	if (!Loaded)
	{
		delete ExternalProject;
		return swUnexpected(tr("Could not load external model file '%1'.").arg(FileInfo.absoluteFilePath()));
	}

	return ExternalProject;
}

swPieceData* swPiecesLibrary::FindPieceData(const char* PieceName, Project* CurrentProject)
{
	const std::string Name = NormalizePieceDataName(PieceName);

	return mPieceManager->FindPieceData(Name, CurrentProject);
}

swPieceDataReference swPiecesLibrary::AcquirePieceData(const char* PieceName, Project* CurrentProject, bool CreateMissing, bool SearchProjectFolder)
{
	Q_ASSERT(QThread::currentThread() == thread());

	QString ProjectPath;
	QString LoadError;

	if (swPieceData* PieceData = FindPieceData(PieceName, CurrentProject))
		return swPieceDataReference(PieceData, this);

	if (CurrentProject && SearchProjectFolder && !CurrentProject->GetFileName().isEmpty())
		ProjectPath = QFileInfo(CurrentProject->GetFileName()).absolutePath();

	if (!ProjectPath.isEmpty())
	{
		QFileInfo ProjectFile = QFileInfo(ProjectPath + QDir::separator() + PieceName);

		if (ProjectFile.isFile())
		{
			const swResult<Project*> ExternalProject = LoadExternalProject(ProjectFile, CurrentProject && CurrentProject->IsPreview(), CurrentProject && CurrentProject->DefersModelMeshRequests());

			if (ExternalProject)
			{
				swPieceData* PieceData = mPieceManager->CreatePieceData(PieceName, CurrentProject);

				PieceData->CreateProject(ExternalProject.value(), PieceName);

				return swPieceDataReference(PieceData, this);
			}
			else
				LoadError = ExternalProject.error();
		}
	}

	if (CreateMissing)
	{
		swPieceData* PieceData = mPieceManager->CreatePieceData(PieceName, CurrentProject);

		PieceData->SetFailed(LoadError.isEmpty() ? tr("Could not find piece '%1'.").arg(QString::fromLatin1(PieceName)) : LoadError);

		return swPieceDataReference(PieceData, this);
	}

	return swPieceDataReference();
}

bool swPiecesLibrary::RemapProjectPieceData(swPieceData* PieceData, const QString& ProjectDirectory, bool IsPreview)
{
	const QByteArray PieceName(PieceData->mFileName);
	const QFileInfo Candidate(QDir(ProjectDirectory).absoluteFilePath(QString::fromLatin1(PieceName.constData())));

	if (PieceData->IsProject() && Candidate.isFile() &&
		QFileInfo(PieceData->GetProject()->GetFileName()).canonicalFilePath() == Candidate.canonicalFilePath())
		return false;

	if (!PieceData->IsProject() && !Candidate.isFile())
		return false;

	InvalidatePieceDataLoad(PieceData);

	if (Candidate.isFile())
	{
		const swResult<Project*> ExternalProject = LoadExternalProject(Candidate, IsPreview, true);

		if (ExternalProject)
			PieceData->CreateProject(ExternalProject.value(), PieceName.constData());
		else
		{
			PieceData->DetachContainer();
			PieceData->SetFailed(ExternalProject.error());
		}
	}
	else
	{
		PieceData->DetachContainer();
		PieceData->SetFailed(tr("Could not find external model file '%1'.").arg(Candidate.absoluteFilePath()));
	}

	NotifyAssetStatusChanged();

	return true;
}

swTextureReference swPiecesLibrary::AcquireTexture(const char* TextureName, Project* CurrentProject, bool SearchProjectFolder)
{
	QString ProjectPath;

	if (SearchProjectFolder && CurrentProject)
	{
		QString FileName = CurrentProject->GetFileName();

		if (!FileName.isEmpty())
			ProjectPath = QFileInfo(FileName).absolutePath();
	}

	std::map<QString, std::shared_ptr<const swEmbeddedData>> EmbeddedData;

	if (CurrentProject)
		EmbeddedData = CurrentProject->GetEmbeddedDataLookup();

	swFileSource Source;
	return AcquireTexture(TextureName, std::vector<QString>{ ProjectPath }, EmbeddedData, Source);
}

QString swPiecesLibrary::FindProjectTextureFile(const QString& ProjectPath, const QString& TextureName)
{
	QDir Directory(ProjectPath);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
	const auto SkipEmptyParts = Qt::SkipEmptyParts;
#else
	const auto SkipEmptyParts = QString::SplitBehavior::SkipEmptyParts;
#endif
	const QStringList PathComponents = (TextureName + QLatin1String(".png")).split(QLatin1Char('/'), SkipEmptyParts);

	for (int ComponentIdx = 0; ComponentIdx < PathComponents.size(); ComponentIdx++)
	{
		const QString& Component = PathComponents[ComponentIdx];
		const bool LastComponent = ComponentIdx == PathComponents.size() - 1;

		if (Component == QLatin1String(".") || Component == QLatin1String(".."))
		{
			if (LastComponent || !Directory.cd(Component))
				return QString();
			continue;
		}

		QFileInfo Entry(Directory, Component);

		if (LastComponent ? !Entry.isFile() : !Entry.isDir())
		{
			Entry = QFileInfo();

			for (const QFileInfo& Candidate : Directory.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System))
			{
				if (Candidate.fileName().compare(Component, Qt::CaseInsensitive) == 0 &&
					(LastComponent ? Candidate.isFile() : Candidate.isDir()))
				{
					Entry = Candidate;
					break;
				}
			}
		}

		if (LastComponent)
			return Entry.isFile() ? Entry.absoluteFilePath() : QString();

		if (!Entry.isDir())
			return QString();

		Directory.setPath(Entry.absoluteFilePath());
	}

	return QString();
}

swFileSource swPiecesLibrary::ResolveTextureSource(const char* TextureName, const std::vector<QString>& SearchDirectories, const std::map<QString, std::shared_ptr<const swEmbeddedData>>& EmbeddedData, QString& ProjectPath)
{
	Q_ASSERT(QThread::currentThread() == thread());

	ProjectPath.clear();

	if (!TextureName[0] || strlen(TextureName) >= SW_TEXTURE_NAME_LENGTH)
		return swFileSource();

	const bool ArchiveLibrary = static_cast<bool>(mZipFiles[static_cast<int>(swZipFileType::Official)]);
	swFileSource Source;
	const QString Name = QString::fromLatin1(TextureName);
	const QString Candidates[] = { QLatin1String("textures/") + Name, Name };
	const QString Folders[] = { QStringLiteral("p/"), QStringLiteral("parts/"), QStringLiteral("models/"), QStringLiteral("unofficial/p/"), QStringLiteral("unofficial/parts/") };

	// Complete the prefixed search before considering any unprefixed location.
	for (const QString& Candidate : Candidates)
	{
		QString EmbeddedName = Candidate + QLatin1String(".png");
		EmbeddedName.replace(QLatin1Char('\\'), QLatin1Char('/'));
		const auto Embedded = EmbeddedData.find(QDir::cleanPath(EmbeddedName).toUpper());

		if (Embedded != EmbeddedData.end())
		{
			Source.EmbeddedData = Embedded->second;
			break;
		}

		for (const QString& Directory : SearchDirectories)
		{
			if (Directory.isEmpty())
				continue;

			Source.FilePath = FindProjectTextureFile(Directory, Candidate);

			if (!Source.FilePath.isEmpty())
			{
				ProjectPath = Directory;
				break;
			}
		}

		if (!Source.FilePath.isEmpty())
			break;

		for (const QString& Folder : Folders)
		{
			if (ArchiveLibrary)
			{
				const bool Unofficial = Folder.startsWith(QLatin1String("unofficial/"));
				const int Type = static_cast<int>(Unofficial ? swZipFileType::Unofficial : swZipFileType::Official);
				QString EntryName;

				if (Unofficial)
					EntryName = Folder.mid(11) + Candidate + QLatin1String(".png");
				else
					EntryName = QLatin1String("ldraw/") + Folder + Candidate + QLatin1String(".png");

				const std::map<QString, int>& Entries = mTextureArchiveEntries[Type];
				const auto Entry = Entries.find(QDir::cleanPath(EntryName).toUpper());

				if (Entry != Entries.end())
				{
					Source.ArchiveType = Type;
					Source.ArchiveIndex = Entry->second;
					break;
				}
			}
			else
			{
				const QString EntryName = QDir::cleanPath(Folder + Candidate + QLatin1String(".png")).toUpper();
				const auto Entry = mTextureDiskEntries.find(EntryName);

				if (Entry != mTextureDiskEntries.end())
				{
					Source.FilePath = Entry->second;
					break;
				}
			}
		}

		if (!Source.FilePath.isEmpty() || Source.ArchiveIndex != -1)
			break;
	}

	if (!Source.FilePath.isEmpty())
	{
		const QString CanonicalPath = QFileInfo(Source.FilePath).canonicalFilePath();
		Source.FilePath = CanonicalPath.isEmpty() ? QFileInfo(Source.FilePath).absoluteFilePath() : CanonicalPath;
	}

	return Source;
}

swTextureReference swPiecesLibrary::AcquireTexture(const char* TextureName, const std::vector<QString>& SearchDirectories, const std::map<QString, std::shared_ptr<const swEmbeddedData>>& EmbeddedData, swFileSource& Source)
{
	QString ProjectPath;
	Source = ResolveTextureSource(TextureName, SearchDirectories, EmbeddedData, ProjectPath);

	return swTextureReference::Adopt(mTextureManager->AcquireTexture(TextureName, Source, ProjectPath), this);
}

void swPiecesLibrary::RequestTextureLoad(swTexture* Texture, swLoadPriority LoadPriority)
{
	Q_ASSERT(QThread::currentThread() == thread());

	mAssetLoader->RequestTextureLoad(Texture, Texture->mSource, LoadPriority);
}

bool swPiecesLibrary::WaitForTexture(const swTextureReference& TextureRef)
{
	Q_ASSERT(QThread::currentThread() == thread());

	return mAssetLoader->WaitForTexture(TextureRef.Get(), TextureRef->mSource);
}

swTextureBuildResult swPiecesLibrary::BuildTextureData(const swFileSource& Source)
{
	const swResult<std::unique_ptr<swFile>> File = OpenFile(Source);

	if (!File)
	{
		swTextureBuildResult Result;
		Result.Error = swTextureLoadError::DecodeFailed;
		Result.ErrorDetails = File.error();
		return Result;
	}

	swTextureBuildResult Result = mTextureManager->BuildTextureData(*File.value());

	if (Result.DecodedImage.isNull() && Source.EmbeddedData)
		Result.ErrorDetails = tr("Could not decode embedded texture '%1'.").arg(Source.EmbeddedData->FileName);

	return Result;
}

swResult<std::unique_ptr<swFile>> swPiecesLibrary::OpenFile(const swFileSource& Source)
{
	if (Source.EmbeddedData)
	{
		const swEmbeddedData& Data = *Source.EmbeddedData;

		if (!Data.Error.isEmpty())
			return swUnexpected(Data.Error);

		if (!Data.HasValidEncoding())
			return swUnexpected(tr("Invalid Base64 data for embedded file '%1'.").arg(Data.FileName));

		const QByteArray Bytes = QByteArray::fromBase64(Data.EncodedData);
		std::unique_ptr<swMemoryFile> File(new swMemoryFile);

		if (File->WriteBuffer(Bytes.constData(), Bytes.size()) != static_cast<size_t>(Bytes.size()))
			return swUnexpected(tr("Could not read embedded file '%1'.").arg(Data.FileName));

		File->Seek(0, SEEK_SET);
		return std::unique_ptr<swFile>(std::move(File));
	}

	if (!Source.FilePath.isEmpty())
	{
		std::unique_ptr<swDiskFile> File(new swDiskFile(Source.FilePath));

		if (!File->Open(QIODevice::ReadOnly))
			return swUnexpected(tr("Could not open file '%1'.").arg(Source.FilePath));

		return std::unique_ptr<swFile>(std::move(File));
	}

	if (Source.ArchiveType < 0 || Source.ArchiveType >= static_cast<int>(swZipFileType::Count) || Source.ArchiveIndex < 0)
		return swUnexpected(tr("Invalid archive file source."));

	std::unique_ptr<swMemoryFile> File(new swMemoryFile);

	// Source archives are stable until workers are drained; extraction locks
	// the selected archive internally without serializing unrelated archives.
	const std::unique_ptr<swZipFile>& Archive = mZipFiles[Source.ArchiveType];

	if (!Archive || static_cast<size_t>(Source.ArchiveIndex) >= Archive->mFiles.size())
		return swUnexpected(tr("Could not read archive entry %1.").arg(Source.ArchiveIndex));

	if (!Archive->ExtractFile(Source.ArchiveIndex, *File))
		return swUnexpected(tr("Could not read archive file '%1'.").arg(QString::fromLatin1(Archive->mFiles[Source.ArchiveIndex].file_name)));

	File->Seek(0, SEEK_SET);
	return std::unique_ptr<swFile>(std::move(File));
}

bool swPiecesLibrary::Load(const QString& LibraryPath, bool ShowProgress, bool OfficialLibrary)
{
	Unload();

	if (OpenArchive(LibraryPath, swZipFileType::Official))
	{
		if (OfficialLibrary)
			mOfficialPieceCount = static_cast<int>(mPieceManager->GetLibraryPieceDataCount());

		LoadColors();

		mLibraryDir = QFileInfo(LibraryPath).absoluteDir();
		QString UnofficialFileName = mLibraryDir.absoluteFilePath(QLatin1String("ldrawunf.zip"));

		if (!OpenArchive(UnofficialFileName, swZipFileType::Unofficial))
			UnofficialFileName.clear();

		ReadArchiveDescriptions(LibraryPath, UnofficialFileName);
	}
	else
	{
		mLibraryDir.setPath(LibraryPath);

		if (OpenDirectory(mLibraryDir, ShowProgress))
			LoadColors();
		else
			return false;
	}

	UpdateStudStyleSource();
	swLoadDefaultCategories();
	swSynthesisInitialize();
	swTrainTrackInformation::Initialize(this);

	return true;
}

void swPiecesLibrary::LoadColors()
{
	QString CustomColorsPath = swGetProfileString(SW_PROFILE_COLOR_CONFIGURATION);

	if (!CustomColorsPath.isEmpty())
	{
		swDiskFile ColorFile(CustomColorsPath);

		if (ColorFile.Open(QIODevice::ReadOnly) && swLoadColorFile(ColorFile, mStudStyle))
		{
			UpdateLoadingMeshColors();
			emit ColorsLoaded();
			return;
		}
	}

	if (mZipFiles[static_cast<int>(swZipFileType::Official)])
	{
		swMemoryFile ColorFile;

		if (!mZipFiles[static_cast<int>(swZipFileType::Official)]->ExtractFile("ldraw/ldconfig.ldr", ColorFile) || !swLoadColorFile(ColorFile, mStudStyle))
			swLoadDefaultColors(mStudStyle);
	}
	else
	{
		swDiskFile ColorFile(mLibraryDir.absoluteFilePath(QLatin1String("ldconfig.ldr")));

		if (!ColorFile.Open(QIODevice::ReadOnly) || !swLoadColorFile(ColorFile, mStudStyle))
		{
			ColorFile.SetFileName(mLibraryDir.absoluteFilePath(QLatin1String("LDConfig.ldr")));

			if (!ColorFile.Open(QIODevice::ReadOnly) || !swLoadColorFile(ColorFile, mStudStyle))
				swLoadDefaultColors(mStudStyle);
		}
	}

	UpdateLoadingMeshColors();
	emit ColorsLoaded();
}

void swPiecesLibrary::UpdateLoadingMeshColors()
{
	swMeshSection* Sections = mLoadingMesh->mLods[SW_MESH_LOD_HIGH].Sections;

	Sections[0].ColorIndex = gDefaultColor;
	Sections[1].ColorIndex = gEdgeColor;
}

bool swPiecesLibrary::IsStudPrimitive(const char* FileName)
{
	return memcmp(FileName, "STU", 3) == 0;
}

bool swPiecesLibrary::IsStudStylePrimitive(const char* FileName)
{
	constexpr std::array<const char*, 15> StudStylePrimitives =
	{
		"2-4STUD4.DAT", "STUD.DAT", "STUD2.DAT", "STUD2A.DAT", "STUD3.DAT", "STUD4.DAT", "STUD4A.DAT", "STUD4H.DAT",
		"8/STUD.DAT", "8/STUD2.DAT", "8/STUD2A.DAT", "8/STUD3.DAT", "8/STUD4.DAT", "8/STUD4A.DAT", "8/STUD4H.DAT"
	};

	for (const char* StudStylePrimitive : StudStylePrimitives)
		if (!strcmp(StudStylePrimitive, FileName))
			return true;

	return false;
}

qint32 swPiecesLibrary::MeshCacheSettingsKey(swStudStyle StudStyle, bool StudCylinderColorEnabled)
{
	return (SW_LIBRARY_MESH_CACHE_VERSION << 16) | (static_cast<qint32>(StudStyle) << 1) | static_cast<qint32>(StudCylinderColorEnabled);
}

void swPiecesLibrary::UpdateStudStyleSource()
{
	if (!mSources.empty() && mSources.front()->Type == swLibrarySourceType::StudStyle)
		mSources.erase(mSources.begin());

	mZipFiles[static_cast<int>(swZipFileType::StudStyle)].reset();

	if (mStudStyle == swStudStyle::Plain || (mStudStyle >= swStudStyle::HighContrast && !mStudCylinderColorEnabled))
		return;

	const QLatin1String FileNames[] =
	{
		QLatin1String(""),								  // Plain
		QLatin1String(":/resources/data/studlogo1.zip"),		  // ThinLinesLogo
		QLatin1String(":/resources/data/studlogo2.zip"),		  // OutlineLogo
		QLatin1String(":/resources/data/studlogo3.zip"),		  // SharpTopLogo
		QLatin1String(":/resources/data/studlogo4.zip"),		  // RoundedTopLogo
		QLatin1String(":/resources/data/studlogo5.zip"),		  // FlattenedLogo
		QLatin1String(":/resources/data/studslegostyle1.zip"), // HighContrast
		QLatin1String(":/resources/data/studslegostyle2.zip")  // HighContrastLogo
	};

	SW_ARRAY_SIZE_CHECK(FileNames, swStudStyle::Count);

	std::unique_ptr<swDiskFile> StudStyleFile(new swDiskFile(FileNames[static_cast<int>(mStudStyle)]));

	if (StudStyleFile->Open(QIODevice::ReadOnly))
		OpenArchive(std::move(StudStyleFile), swZipFileType::StudStyle);
}

bool swPiecesLibrary::OpenArchive(const QString& FileName, swZipFileType ZipFileType)
{
	std::unique_ptr<swDiskFile> File(new swDiskFile(FileName));

	if (!File->Open(QIODevice::ReadOnly))
		return false;

	return OpenArchive(std::move(File), ZipFileType);
}

bool swPiecesLibrary::OpenArchive(std::unique_ptr<swFile> File, swZipFileType ZipFileType)
{
	std::unique_ptr<swZipFile> ZipFile(new swZipFile());

	if (!ZipFile->OpenRead(std::move(File)))
		return false;

	mTextureArchiveEntries[static_cast<int>(ZipFileType)].clear();

	std::unique_ptr<swLibrarySource> Source(new swLibrarySource);
	Source->Type = ZipFileType != swZipFileType::StudStyle ? swLibrarySourceType::Library : swLibrarySourceType::StudStyle;

	for (quint32 FileIdx = 0; FileIdx < ZipFile->mFiles.size(); FileIdx++)
	{
		swZipFileInformation& FileInfo = ZipFile->mFiles[FileIdx];
		const std::string NormalizedName = NormalizePieceDataName(FileInfo.file_name);
		const char* Name = NormalizedName.c_str();

		if (NormalizedName.size() <= 4)
			continue;

		const char* Extension = Name + NormalizedName.size() - 4;

		if (memcmp(Extension, ".DAT", 4))
		{
			if (!memcmp(Extension, ".PNG", 4))
			{
				if (ZipFileType == swZipFileType::Official || ZipFileType == swZipFileType::Unofficial)
					mTextureArchiveEntries[static_cast<int>(ZipFileType)].emplace(QDir::cleanPath(QString::fromLatin1(Name)).toUpper(), FileIdx);
			}

			continue;
		}

		if (ZipFileType == swZipFileType::Official)
		{
			if (strncmp(Name, "LDRAW/", 6))
				continue;

			Name += 6;
		}

		if (!strncmp(Name, "PARTS/", 6))
		{
			Name += 6;

			if (strncmp(Name, "S/", 2))
			{
				swPieceData* PieceData = FindPieceData(Name, nullptr);

				// Unofficial additions must not replace registered official parts.
				if (PieceData && ZipFileType == swZipFileType::Unofficial && PieceData->mZipFileType == swZipFileType::Official)
					continue;

				if (!PieceData)
				{
					PieceData = mPieceManager->CreateLibraryPieceData(Name, FileInfo.file_name + (Name - NormalizedName.c_str()));
				}

				PieceData->SetZipFile(ZipFileType, FileIdx);
			}
			else
				Source->Primitives[Name] = std::make_unique<swLibraryPrimitive>(QString(), FileInfo.file_name + (Name - NormalizedName.c_str()), ZipFileType, FileIdx, false, false, true);
		}
		else if (!strncmp(Name, "P/", 2))
		{
			Name += 2;

			Source->Primitives[Name] = std::make_unique<swLibraryPrimitive>(QString(), FileInfo.file_name + (Name - NormalizedName.c_str()), ZipFileType, FileIdx, IsStudPrimitive(Name), IsStudStylePrimitive(Name), false);
		}
	}

	mZipFiles[static_cast<int>(ZipFileType)] = std::move(ZipFile);

	if (ZipFileType != swZipFileType::StudStyle)
		mSources.emplace_back(std::move(Source));
	else
		mSources.insert(mSources.begin(), std::move(Source));

	return true;
}

void swPiecesLibrary::ReadArchiveDescriptions(const QString& OfficialFileName, const QString& UnofficialFileName)
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
		swMemoryFile PieceFile;

		for (swPieceData* PieceData : mPieceManager->GetLibraryPieceData())
		{
			mZipFiles[static_cast<int>(PieceData->mZipFileType)]->ExtractFile(PieceData->mZipFileIndex, PieceFile, 256);
			PieceFile.Seek(0, SEEK_END);
			PieceFile.WriteU8(0);

			char* Src = (char*)PieceFile.mBuffer + 2;
			char* Dst = PieceData->m_strDescription;

			for (;;)
			{
				if (*Src != '\r' && *Src != '\n' && *Src && Dst - PieceData->m_strDescription < (int)sizeof(PieceData->m_strDescription) - 1)
				{
					*Dst++ = *Src++;
					continue;
				}

				*Dst = 0;
				break;
			}
		}

		ClassifyPieces(DirectoryFilePaths());
		SaveArchiveCacheIndex(IndexFileName);
	}
}

bool swPiecesLibrary::OpenDirectory(const QDir& LibraryDir, bool ShowProgress)
{
	mTextureDiskEntries.clear();

	const QLatin1String BaseFolders[] = { QLatin1String(""), QLatin1String("unofficial/") };
	constexpr int NumBaseFolders = SW_ARRAY_COUNT(BaseFolders);

	QFileInfoList FileLists[NumBaseFolders];

	for (unsigned int BaseFolderIdx = 0; BaseFolderIdx < NumBaseFolders; BaseFolderIdx++)
	{
		QString ParstPath = QDir(LibraryDir.absoluteFilePath(BaseFolders[BaseFolderIdx])).absoluteFilePath(QLatin1String("parts/"));
		QDir Dir = QDir(ParstPath, QLatin1String("*.dat"), QDir::SortFlags(QDir::Name | QDir::IgnoreCase), QDir::Files | QDir::Hidden | QDir::Readable);
		FileLists[BaseFolderIdx] = Dir.entryInfoList();
	}

	if (FileLists[static_cast<int>(swLibraryFolderType::Official)].isEmpty())
		return false;

	mHasUnofficial = !FileLists[static_cast<int>(swLibraryFolderType::Unofficial)].isEmpty();
	ReadDirectoryDescriptions(FileLists, ShowProgress);

	constexpr std::pair<const char*, bool> PrimitiveDirectories[] =
	{
		{ "p/", false },
		{ "parts/s/", true }
	};

	for (unsigned int BaseFolderIdx = 0; BaseFolderIdx < SW_ARRAY_COUNT(BaseFolders); BaseFolderIdx++)
	{
		std::unique_ptr<swLibrarySource> Source(new swLibrarySource);
		Source->Type = swLibrarySourceType::Library;

		QDir BaseDir(LibraryDir.absoluteFilePath(QLatin1String(BaseFolders[BaseFolderIdx])));

		for (const auto& [Path, SubFile] : PrimitiveDirectories)
		{
			QString ChildPath = BaseDir.absoluteFilePath(QLatin1String(Path));
			QDirIterator DirIterator(ChildPath, QStringList() << QLatin1String("*.dat"), QDir::Files | QDir::Hidden | QDir::Readable, QDirIterator::Subdirectories);

			while (DirIterator.hasNext())
			{
				QString FileName = DirIterator.next();
				QByteArray FileString = BaseDir.relativeFilePath(FileName).toLatin1();
				const char* RelativeName = strchr(FileString.constData(), '/') + 1;
				const std::string Name = NormalizePieceDataName(RelativeName);

				if (Name.size() <= 4 || Name.compare(Name.size() - 4, 4, ".DAT"))
					continue;

				if (BaseFolderIdx > 0 && FindPrimitive(Name.c_str()))
					continue;

				if (BaseFolderIdx == static_cast<int>(swLibraryFolderType::Unofficial))
					mHasUnofficial = true;

				Source->Primitives[Name] = std::make_unique<swLibraryPrimitive>(std::move(FileName), RelativeName, swZipFileType::Count, 0, !SubFile && IsStudPrimitive(Name.c_str()), IsStudStylePrimitive(Name.c_str()), SubFile);
			}
		}

		mSources.emplace_back(std::move(Source));

		// Index PNG paths once; UI-thread texture lookup must not enumerate library folders.
		const char* TextureDirectories[] = { "p/", "parts/", "models/" };

		for (const char* TextureDirectory : TextureDirectories)
		{
			const QString ChildPath = BaseDir.absoluteFilePath(QLatin1String(TextureDirectory));
			QDirIterator DirIterator(ChildPath, QStringList() << QLatin1String("*.png"), QDir::Files | QDir::Hidden | QDir::Readable, QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);

			while (DirIterator.hasNext())
			{
				const QString FilePath = DirIterator.next();
				const QString Name = QDir::cleanPath(LibraryDir.relativeFilePath(FilePath)).toUpper();

				mTextureDiskEntries.emplace(Name, FilePath);
			}
		}
	}

	return true;
}

void swPiecesLibrary::ReadDirectoryDescriptions(const QFileInfoList (&FileLists)[static_cast<int>(swLibraryFolderType::Count)], bool ShowProgress)
{
	QString IndexFileName = QFileInfo(QDir(mCachePath), QLatin1String("index")).absoluteFilePath();
	swMemoryFile IndexFile;

	struct CachedDescription
	{
		swPieceData::CacheData CacheData;
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
		swPieceData* PieceData;
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
			const std::string Name = NormalizePieceDataName(FileString.constData());

			if (FolderIdx > 0 && mPieceManager->FindPieceData(Name, nullptr))
				continue;

			swPieceData* PieceData = mPieceManager->CreateLibraryPieceData(Name, FileString.constData());

			Jobs.push_back({ PieceData, FileList[FileIdx], static_cast<swLibraryFolderType>(FolderIdx) });
		}
	}

	// Only process entries retained by the normalized registry after discovery.
	Jobs.erase(std::remove_if(Jobs.begin(), Jobs.end(), [this](const DescriptionJob& Job)
	{
		return FindPieceData(Job.FileInfo.fileName().toLatin1().constData(), nullptr) != Job.PieceData;
	}), Jobs.end());

	QAtomicInt FilesLoaded;

	auto ReadDescriptions = [&CachedDescriptions, &FilesLoaded](DescriptionJob& Job)
	{
		FilesLoaded.ref();

		swPieceData* PieceData = Job.PieceData;
		swDiskFile PieceFile(Job.FileInfo.absoluteFilePath());
		char Line[1024];

		const auto CachedIt = CachedDescriptions.find(PieceData->mFileName);
		const quint64 FileTime = Job.FileInfo.lastModified().toMSecsSinceEpoch();

		if (CachedIt != CachedDescriptions.end() && CachedIt->second.FileTime == FileTime && CachedIt->second.FolderType == static_cast<quint8>(Job.FolderType) &&
			PieceData->InitializeFromCache(CachedIt->second.CacheData))
			return;

		Job.Modified = true;

		if (!PieceFile.Open(QIODevice::ReadOnly) || !PieceFile.ReadLine(Line, sizeof(Line)))
		{
			swstrcpy(PieceData->m_strDescription, "Unknown");
			return;
		}

		const char* Src = Line + 2;
		char* Dst = PieceData->m_strDescription;

		for (;;)
		{
			if (*Src != '\r' && *Src != '\n' && *Src && Dst - PieceData->m_strDescription < (int)sizeof(PieceData->m_strDescription) - 1)
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
	ProgressDialog->setLabelText(tr("Loading Pieces Library"));
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
			FilePaths.emplace(Job.PieceData, Job.FileInfo.absoluteFilePath());

		ClassifyPieces(FilePaths);

		swMemoryFile NewIndexFile;

		NewIndexFile.WriteQString(mLibraryDir.absolutePath());
		NewIndexFile.WriteU32(static_cast<quint32>(Jobs.size()));

		for (const DescriptionJob& Job : Jobs)
		{
			const swPieceData* PieceData = Job.PieceData;
			const swPieceData::CacheData CacheData = PieceData->GetCacheData();

			if (!WriteIndexString(NewIndexFile, PieceData->mFileName) ||
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

bool swPiecesLibrary::ReadCacheFile(const QString& FileName, swMemoryFile& CacheFile, CacheType Type)
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

bool swPiecesLibrary::WriteCacheFile(const QString& FileName, const swMemoryFile& CacheFile, CacheType Type)
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

bool swPiecesLibrary::ReadIndexString(swMemoryFile& File, std::string& String)
{
	quint8 Length;

	if (File.ReadU8(&Length, 1) != 1)
		return false;

	String.resize(Length);

	return File.ReadBuffer(String.data(), Length) == Length;
}

bool swPiecesLibrary::WriteIndexString(swMemoryFile& File, const std::string& String)
{
	if (String.size() > 255)
		return false;

	const quint8 Length = static_cast<quint8>(String.size());

	return File.WriteU8(&Length, 1) == 1 && File.WriteBuffer(String.data(), Length) == Length;
}

std::string swPiecesLibrary::ReadAliasTarget(swFile& File)
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

		if (Name.isEmpty() || Name.size() >= SW_PIECE_NAME_LENGTH)
			return std::string();

		Target = NormalizePieceDataName(Name.constData());
	}

	return Target;
}

void swPiecesLibrary::ClassifyPieces(const DirectoryFilePaths& FilePaths)
{
	const std::vector<swPieceData*> Pieces = mPieceManager->GetLibraryPieceData();
	std::unordered_map<swPieceData*, swPieceData*> AliasTargets;

	for (swPieceData* PieceData : Pieces)
	{
		const std::string Name = NormalizePieceDataName(PieceData->mFileName);
		const std::vector<std::string> ParentIds = PieceData->Classify(Name);

		if (PieceData->IsAlias())
		{
			swFileSource Source;

			if (PieceData->mZipFileType != swZipFileType::Count)
			{
				Source.ArchiveType = static_cast<int>(PieceData->mZipFileType);
				Source.ArchiveIndex = PieceData->mZipFileIndex;
			}
			else
			{
				const auto FileIt = FilePaths.find(PieceData);

				if (FileIt == FilePaths.end())
				{
					AliasTargets.emplace(PieceData, nullptr);
					continue;
				}

				Source.FilePath = FileIt->second;
			}

			const swResult<std::unique_ptr<swFile>> File = OpenFile(Source);
			swPieceData* Target = nullptr;

			if (File)
			{
				const std::string TargetId = ReadAliasTarget(*File.value());

				if (!TargetId.empty())
					Target = FindPieceData(TargetId.c_str(), nullptr);
			}

			AliasTargets.emplace(PieceData, Target);
			continue;
		}

		for (const std::string& ParentId : ParentIds)
		{
			const swPieceData* Parent = FindPieceData(ParentId.c_str(), nullptr);

			if (Parent && Parent->IsLibraryPiece())
			{
				PieceData->SetPatternParentId(ParentId);
				break;
			}
		}
	}

	std::unordered_set<swPieceData*> ResolvedAliases;

	for (const auto& [Alias, Target] : AliasTargets)
	{
		if (ResolvedAliases.find(Alias) != ResolvedAliases.end())
			continue;

		std::vector<swPieceData*> Chain;
		swPieceData* Current = Alias;
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
		for (auto PieceIt = Chain.rbegin(); PieceIt != Chain.rend(); PieceIt++)
		{
			swPieceData* PieceData = *PieceIt;
			PieceData->SetAliasTarget(Cycle ? nullptr : Current);
			Current = PieceData;
			ResolvedAliases.insert(PieceData);
		}
	}
}

bool swPiecesLibrary::LoadCacheIndex(const QString& FileName)
{
	swMemoryFile IndexFile;

	if (!ReadCacheFile(FileName, IndexFile, CacheType::Archive))
		return false;

	quint32 NumFiles;

	if (IndexFile.ReadU32(&NumFiles, 1) != 1 || NumFiles != mPieceManager->GetLibraryPieceDataCount())
		return false;

	for (swPieceData* PieceData : mPieceManager->GetLibraryPieceData())
	{
		swPieceData::CacheData CacheData;

		if (!ReadIndexString(IndexFile, CacheData.Description) || !ReadIndexString(IndexFile, CacheData.PatternParentId) ||
			IndexFile.ReadU8(&CacheData.Flags, 1) != 1 || !PieceData->InitializeFromCache(CacheData))
			return false;
	}

	return true;
}

bool swPiecesLibrary::SaveArchiveCacheIndex(const QString& FileName)
{
	swMemoryFile IndexFile;

	const quint32 NumFiles = (quint32)mPieceManager->GetLibraryPieceDataCount();

	if (IndexFile.WriteBuffer((char*)&NumFiles, sizeof(NumFiles)) == 0)
		return false;

	for (const swPieceData* PieceData : mPieceManager->GetLibraryPieceData())
	{
		const swPieceData::CacheData CacheData = PieceData->GetCacheData();

		if (!WriteIndexString(IndexFile, CacheData.Description) || !WriteIndexString(IndexFile, CacheData.PatternParentId))
			return false;

		IndexFile.WriteU8(CacheData.Flags);
	}

	return WriteCacheFile(FileName, IndexFile, CacheType::Archive);
}

void swPiecesLibrary::RequestPieceDataLoad(swPieceData* PieceData, swLoadPriority LoadPriority)
{
	Q_ASSERT(QThread::currentThread() == thread());

	if (PieceData->IsProject() || (PieceData->IsModel() && !PieceData->GetModel()->HasDirectGeometry()))
	{
		PieceData->mState = swAssetState::Loaded;
		ClearPieceDataLoadError(PieceData);
		return;
	}

	if (PieceData->mState == swAssetState::Unloaded)
	{
		PieceData->mState = swAssetState::Loading;
		ClearPieceDataLoadError(PieceData);

		const swBoundingBox& Box = GetLoadingMesh()->mBoundingBox;

		if (!PieceData->IsModel())
			PieceData->SetBoundingBox(Box.Min, Box.Max);
		else
		{
			const swBoundingBox& ModelBox = PieceData->GetBoundingBox();
			PieceData->SetBoundingBox(swMinimum(ModelBox.Min, Box.Min), swMaximum(ModelBox.Max, Box.Max));
		}
	}

	if (PieceData->IsLoading())
		mAssetLoader->RequestPieceLoad(PieceData, LoadPriority);
}

void swPiecesLibrary::PieceDataLoadCancelled(swPieceData* PieceData)
{
	Q_ASSERT(QThread::currentThread() == thread());

	PieceData->mState = swAssetState::Unloaded;
	ClearPieceDataLoadError(PieceData);
}

void swPiecesLibrary::NotifyAssetStatusChanged()
{
	emit AssetStatusChanged();
}

bool swPiecesLibrary::EnsurePieceDataAssetsReady(const swPieceDataReference& PieceData)
{
	const bool PieceReady = WaitForPieceData(PieceData);

	if (PieceData->IsModel())
		return static_cast<bool>(PieceData->GetModel()->EnsureAssetsReady()) && PieceReady;

	if (PieceData->IsProject())
	{
		const bool ProjectReady = static_cast<bool>(PieceData->GetProject()->EnsureAssetsReady());

		PieceData->GetProject()->UpdatePieceData(PieceData.Get());
		return ProjectReady && PieceReady;
	}

	return PieceReady;
}

bool swPiecesLibrary::WaitForPieceData(const swPieceDataReference& PieceDataRef)
{
	Q_ASSERT(QThread::currentThread() == thread());

	if (PieceDataRef->mState == swAssetState::Unloaded)
		RequestPieceDataLoad(PieceDataRef.Get(), swLoadPriority::Blocking);

	const bool RequestsSucceeded = mAssetLoader->WaitForPieces({ PieceDataRef.Get() });

	return RequestsSucceeded && PieceDataRef->mState == swAssetState::Loaded;
}

bool swPiecesLibrary::WaitForPieceData(const std::vector<swPieceDataReference>& PieceDataRefs, const std::function<void(int, int)>& Progress)
{
	Q_ASSERT(QThread::currentThread() == thread());

	std::vector<swPieceData*> Pieces;
	Pieces.reserve(PieceDataRefs.size());

	for (const swPieceDataReference& PieceDataRef : PieceDataRefs)
	{
		if (PieceDataRef->mState == swAssetState::Unloaded)
			RequestPieceDataLoad(PieceDataRef.Get(), swLoadPriority::Blocking);

		Pieces.push_back(PieceDataRef.Get());
	}

	const bool RequestsSucceeded = mAssetLoader->WaitForPieces(Pieces, Progress);

	for (const swPieceDataReference& PieceDataRef : PieceDataRefs)
		if (PieceDataRef->mState != swAssetState::Loaded)
			return false;

	return RequestsSucceeded;
}

void swPiecesLibrary::RequestSynthMesh(swPiece* Piece)
{
	Q_ASSERT(QThread::currentThread() == thread());

	swPieceData* PieceData = Piece->mPieceData.Get();
	std::shared_ptr<const swSynthesisInformation> SynthInfo = PieceData ? PieceData->GetSynthInfoShared() : nullptr;

	if (!SynthInfo)
	{
		CancelSynthMesh(Piece);
		return;
	}

	mSynthPieces.insert_or_assign(Piece, QString());

	mAssetLoader->RequestSynthMesh(Piece, PieceData, std::move(SynthInfo), Piece->GetControlPoints());
}

void swPiecesLibrary::CancelSynthMesh(swPiece* Piece)
{
	Q_ASSERT(QThread::currentThread() == thread());

	mSynthPieces.erase(Piece);

	if (mAssetLoader)
		mAssetLoader->CancelSynthMesh(Piece);
}

bool swPiecesLibrary::WaitForSynthMeshes(const std::vector<swPiece*>& Pieces)
{
	Q_ASSERT(QThread::currentThread() == thread());

	const bool RequestsSucceeded = mAssetLoader->WaitForSynthMeshes(Pieces);

	for (const swPiece* Piece : Pieces)
		if (Piece->mPieceData && Piece->mPieceData->GetSynthInfo() && !Piece->HasGeneratedMesh())
			return false;

	return RequestsSucceeded;
}

QString swPiecesLibrary::GetSynthMeshError(const swPiece* Piece) const
{
	Q_ASSERT(QThread::currentThread() == thread());

	const auto It = mSynthPieces.find(const_cast<swPiece*>(Piece));

	return It != mSynthPieces.end() ? It->second : QString();
}

bool swPiecesLibrary::PublishPieceDataMesh(swPieceData* PieceData, std::unique_ptr<swMesh> Mesh, QString Error)
{
	Q_ASSERT(QThread::currentThread() == thread());

	const bool Loaded = Error.isEmpty();

	if (Loaded)
	{
		if (Mesh)
			PieceData->SetMesh(Mesh.release());

		PieceData->mState = swAssetState::Loaded;
		ClearPieceDataLoadError(PieceData);
	}
	else
		PieceData->SetFailed(std::move(Error));

	ScheduleBufferRepack();

	return Loaded;
}

bool swPiecesLibrary::PublishSynthMesh(swPiece* Piece, std::unique_ptr<swMesh> Mesh, QString Error)
{
	Q_ASSERT(QThread::currentThread() == thread());

	const bool Loaded = Error.isEmpty() && Mesh;

	if (!Loaded && Error.isEmpty())
		Error = tr("Could not generate geometry for piece '%1'.").arg(QString::fromLatin1(Piece->mPieceData->mFileName));

	Piece->SetGeneratedMesh(Loaded ? Mesh.release() : nullptr);

	const auto It = mSynthPieces.find(Piece);

	if (It != mSynthPieces.end())
		It->second = Loaded ? QString() : std::move(Error);

	return Loaded;
}

void swPiecesLibrary::PublishTexture(swTexture* Texture, swTextureBuildResult Result)
{
	Q_ASSERT(QThread::currentThread() == thread());

	if (!Result.DecodedImage.isNull())
		Texture->SetImage(std::move(Result.DecodedImage), SW_TEXTURE_MIPMAPS);
	else
		Texture->SetFailed(Result.Error, std::move(Result.ErrorDetails));
}

void swPiecesLibrary::RebuildModelPieceData(swPieceData* PieceData)
{
	Q_ASSERT(QThread::currentThread() == thread());
	Q_ASSERT(PieceData->IsModel());

	InvalidatePieceDataLoad(PieceData);
	PieceData->ReleaseMesh();
	RequestPieceDataLoad(PieceData, swLoadPriority::Normal);
}

void swPiecesLibrary::InvalidatePieceDataLoad(swPieceData* PieceData)
{
	Q_ASSERT(QThread::currentThread() == thread());

	PieceData->mState = swAssetState::Unloaded;
	ClearPieceDataLoadError(PieceData);

	if (mAssetLoader)
		mAssetLoader->InvalidatePieceLoad(PieceData);
}

void swPiecesLibrary::ReleasePieceData(swPieceData* PieceData)
{
	const int Remaining = mPieceManager->ReleasePieceData(PieceData);

	if (Remaining == 1 && mAssetLoader)
		mAssetLoader->CancelPieceLoad(PieceData);
}

void swPiecesLibrary::AddPieceDataReference(swPieceData* PieceData)
{
	mPieceManager->AddPieceDataReference(PieceData);
}

void swPiecesLibrary::SetPieceDataLoadError(const swPieceData* PieceData, QString Error)
{
	mPieceManager->SetPieceDataLoadError(PieceData, std::move(Error));
}

QString swPiecesLibrary::GetPieceDataLoadError(const swPieceData* PieceData) const
{
	return mPieceManager->GetPieceDataLoadError(PieceData);
}

void swPiecesLibrary::ClearPieceDataLoadError(const swPieceData* PieceData)
{
	mPieceManager->ClearPieceDataLoadError(PieceData);
}

swPieceSourceSnapshot swPiecesLibrary::SnapshotPieceDataSource(const swPieceData* PieceData) const
{
	swPieceSourceSnapshot Source;

	Source.FileName = QString::fromLatin1(PieceData->mFileName);
	Source.LibraryDirectory = mLibraryDir.absolutePath();
	Source.CachePath = QFileInfo(QDir(mCachePath), Source.FileName).absoluteFilePath();
	Source.ZipFileType = PieceData->mZipFileType;
	Source.ZipFileIndex = PieceData->mZipFileIndex;
	Source.StudStyle = mStudStyle;
	Source.StudCylinderColorEnabled = mStudCylinderColorEnabled;

	if (PieceData->IsModel())
	{
		Source.InlineModel = true;
		Source.InlineMeshLines = PieceData->GetModel()->GetFileLines();

		const swModel* Model = PieceData->GetModel();
		Source.TextureSearchDirectories = Model->GetAssetSearchDirectories();

		if (Model->GetProject())
			Source.EmbeddedData = Model->GetProject()->GetEmbeddedDataLookup();
	}

	return Source;
}

swPieceBuildResult swPiecesLibrary::BuildPieceData(const swPieceSourceSnapshot& Source)
{
	swPieceBuildResult Result;

	if (mCancelLoading.load())
	{
		Result.Error = tr("Library loading was canceled.");
		return Result;
	}

	if (!Source.InlineModel && Source.ZipFileType != swZipFileType::Count && mZipFiles[static_cast<int>(Source.ZipFileType)])
	{
		swMemoryFile CacheData;

		if (ReadCacheFile(Source.CachePath, CacheData, CacheType::Archive))
		{
			qint32 Flags;

			if (CacheData.ReadBuffer(&Flags, sizeof(Flags)) == sizeof(Flags) && Flags == MeshCacheSettingsKey(Source.StudStyle, Source.StudCylinderColorEnabled))
			{
				Result.Mesh.reset(new swMesh);

				if (Result.Mesh->FileLoad(CacheData))
				{
					Result.FromCache = true;
					return Result;
				}

				Result.Mesh.reset();
			}
		}
	}

	swLibraryMeshData MeshData;
	swMeshLoader MeshLoader(MeshData, nullptr, swMeshLoaderFlag::Optimize | swMeshLoaderFlag::RequireAllIncludes);

	if (Source.InlineModel)
	{
		swMemoryFile PieceFile;

		for (const QString& Line : Source.InlineMeshLines)
		{
			const QByteArray Buffer = Line.toLatin1();

			PieceFile.WriteBuffer(Buffer.constData(), Buffer.size());
			PieceFile.WriteBuffer("\r\n", 2);
		}

		PieceFile.Seek(0, SEEK_SET);

		if (!MeshLoader.LoadMesh(PieceFile, SW_MESH_DATA_SHARED))
		{
			Result.Error = tr("Could not load model geometry '%1'.").arg(Source.FileName);

			if (!MeshLoader.GetError().isEmpty())
				Result.Error += QLatin1Char('\n') + MeshLoader.GetError();

			return Result;
		}

		if (MeshData.IsEmpty())
		{
			Result.EmptyGeometry = true;
			return Result;
		}
	}
	else
	{
		bool Loaded = false;

		if (Source.ZipFileType != swZipFileType::Count && mZipFiles[static_cast<int>(Source.ZipFileType)])
		{
			swFileSource FileSource;
			FileSource.ArchiveType = static_cast<int>(Source.ZipFileType);
			FileSource.ArchiveIndex = Source.ZipFileIndex;
			const swResult<std::unique_ptr<swFile>> File = OpenFile(FileSource);

			if (File)
				Loaded = MeshLoader.LoadMesh(*File.value(), SW_MESH_DATA_SHARED);
		}
		else
		{
			swFileSource FileSource;
			FileSource.FilePath = QDir(Source.LibraryDirectory).absoluteFilePath(QStringLiteral("parts/") + Source.FileName);
			const swResult<std::unique_ptr<swFile>> File = OpenFile(FileSource);

			if (File)
				Loaded = MeshLoader.LoadMesh(*File.value(), SW_MESH_DATA_SHARED);

			// Directory loading retries unofficial geometry after an open or parse failure.
			if (mHasUnofficial && !Loaded)
			{
				MeshData.Clear();

				FileSource.FilePath = QDir(Source.LibraryDirectory).absoluteFilePath(QStringLiteral("unofficial/parts/") + Source.FileName);
				const swResult<std::unique_ptr<swFile>> UnofficialFile = OpenFile(FileSource);

				if (UnofficialFile)
					Loaded = MeshLoader.LoadMesh(*UnofficialFile.value(), SW_MESH_DATA_SHARED);
			}
		}

		if (mCancelLoading.load())
			Result.Error = tr("Library loading was canceled.");
		else if (!Loaded)
		{
			Result.Error = tr("Could not load piece '%1'.").arg(Source.FileName);

			if (!MeshLoader.GetError().isEmpty())
				Result.Error += QLatin1Char('\n') + MeshLoader.GetError();
		}

		if (!Result.Error.isEmpty())
			return Result;
	}

	Result.Mesh.reset(MeshData.CreateMesh(nullptr));

	if (!Result.Mesh)
		Result.Error = tr("Could not prepare piece '%1' for rendering.").arg(Source.FileName);

	return Result;
}

void swPiecesLibrary::SaveBuiltPieceDataCache(const swPieceSourceSnapshot& Source, swMesh& Mesh)
{
	if (Source.ZipFileType == swZipFileType::Count)
		return;

	swMemoryFile MeshData;

	const qint32 Flags = MeshCacheSettingsKey(Source.StudStyle, Source.StudCylinderColorEnabled);

	if (MeshData.WriteBuffer((char*)&Flags, sizeof(Flags)) != sizeof(Flags) || !Mesh.FileSave(MeshData))
		return;

	WriteCacheFile(Source.CachePath, MeshData, CacheType::Archive);
}

swResult<std::unique_ptr<swFile>> swPiecesLibrary::OpenPrimitiveFile(const swLibraryPrimitive* Primitive)
{
	swFileSource Source;

	if (Primitive->mZipFileType != swZipFileType::Count)
	{
		Source.ArchiveType = static_cast<int>(Primitive->mZipFileType);
		Source.ArchiveIndex = Primitive->mZipFileIndex;
	}
	else
		Source.FilePath = Primitive->mFileName;

	return OpenFile(Source);
}

swResult<std::unique_ptr<swFile>> swPiecesLibrary::OpenPieceDataFile(const char* PieceName)
{
	const std::optional<swPieceManager::FileInfo> FileInfo = mPieceManager->GetPieceDataFileInfo(PieceName);

	if (mZipFiles[static_cast<int>(swZipFileType::Official)])
	{
		if (FileInfo && FileInfo->ZipFileType != swZipFileType::Count)
		{
			swFileSource Source;
			Source.ArchiveType = static_cast<int>(FileInfo->ZipFileType);
			Source.ArchiveIndex = FileInfo->ZipFileIndex;
			return OpenFile(Source);
		}

		if (!FileInfo)
		{
			for (const swZipFileType Type : { swZipFileType::Official, swZipFileType::Unofficial })
			{
				const std::unique_ptr<swZipFile>& Archive = mZipFiles[static_cast<int>(Type)];

				if (!Archive)
					continue;

				const QString Prefix = Type == swZipFileType::Official ? QStringLiteral("ldraw/") : QString();

				for (const QString& Folder : { QStringLiteral("parts/"), QStringLiteral("p/") })
				{
					const QByteArray FileName = (Prefix + Folder + QString::fromLatin1(PieceName)).toLatin1();

					const std::optional<quint32> FileIndex = Archive->FindFileIndex(FileName.constData());

					if (!FileIndex)
						continue;

					swFileSource Source;
					Source.ArchiveType = static_cast<int>(Type);
					Source.ArchiveIndex = *FileIndex;

					if (swResult<std::unique_ptr<swFile>> File = OpenFile(Source))
						return File;
				}
			}

			return swUnexpected(tr("Could not open included file '%1'.").arg(QString::fromLatin1(PieceName)));
		}
	}

	const QString Name = QString::fromLatin1(FileInfo ? FileInfo->FileName.c_str() : PieceName);
	std::vector<QString> Folders(1, QStringLiteral("parts/"));

	if (!FileInfo)
		Folders.emplace_back(QStringLiteral("p/"));

	if (mHasUnofficial)
	{
		Folders.emplace_back(QStringLiteral("unofficial/parts/"));

		if (!FileInfo)
			Folders.emplace_back(QStringLiteral("unofficial/p/"));
	}

	for (const QString& Folder : Folders)
	{
		const QString FileName = Folder + Name;
		swFileSource Source;
		Source.FilePath = mLibraryDir.absoluteFilePath(FileName);

		if (swResult<std::unique_ptr<swFile>> File = OpenFile(Source))
			return File;

#if defined(Q_OS_MACOS) || defined(Q_OS_LINUX)
		if (!FileInfo)
		{
			Source.FilePath = mLibraryDir.absoluteFilePath(FileName.toLower());

			if (swResult<std::unique_ptr<swFile>> File = OpenFile(Source))
				return File;
		}
#endif
	}

	return swUnexpected(tr("Could not open included file '%1'.").arg(QString::fromLatin1(PieceName)));
}

void swPiecesLibrary::ReleaseBuffers()
{
	mBuffersDirty = true;

	// Library-only loading can finish without creating a renderer or shared buffers.
	if (!mVertexBuffer.IsValid() && !mIndexBuffer.IsValid())
		return;

	swContext* Context = swContext::GetGlobalOffscreenContext();

	Context->MakeCurrent();
	Context->DestroyVertexBuffer(mVertexBuffer);
	Context->DestroyIndexBuffer(mIndexBuffer);
}

void swPiecesLibrary::UpdateBuffers(swContext* Context)
{
	if (!gSupportsVertexBufferObject || !mBuffersDirty)
		return;

	int VertexDataSize = 0;
	int IndexDataSize = 0;
	std::vector<swMesh*> Meshes;

	const auto AddMesh = [&VertexDataSize, &IndexDataSize, &Meshes](swPieceData* PieceData)
	{
		swMesh* Mesh = PieceData->GetMesh();

		if (!Mesh)
			return;

		if (Mesh->mVertexDataSize < 0 || Mesh->mIndexDataSize < 0 ||
			Mesh->mVertexDataSize > 16 * 1024 * 1024 || Mesh->mIndexDataSize > 16 * 1024 * 1024 ||
			(Mesh->mVertexDataSize && !Mesh->mVertexData) || (Mesh->mIndexDataSize && !Mesh->mIndexData) ||
			VertexDataSize > std::numeric_limits<int>::max() - Mesh->mVertexDataSize ||
			IndexDataSize > std::numeric_limits<int>::max() - Mesh->mIndexDataSize)
		{
			Mesh->mVertexCacheOffset = -1;
			Mesh->mIndexCacheOffset = -1;
			return;
		}

		VertexDataSize += Mesh->mVertexDataSize;
		IndexDataSize += Mesh->mIndexDataSize;

		Meshes.push_back(Mesh);
	};

	for (swPieceData* PieceData : mPieceManager->GetAllPieceData())
		AddMesh(PieceData);

	if (!VertexDataSize)
	{
		Context->DestroyVertexBuffer(mVertexBuffer);
		Context->DestroyIndexBuffer(mIndexBuffer);
		mBuffersDirty = false;

		return;
	}

	void* VertexData = malloc(VertexDataSize);
	void* IndexData = IndexDataSize ? malloc(IndexDataSize) : nullptr;

	if (!VertexData || (IndexDataSize && !IndexData))
	{
		free(VertexData);
		free(IndexData);

		return;
	}

	VertexDataSize = 0;
	IndexDataSize = 0;

	for (swMesh* Mesh : Meshes)
	{
		if (Mesh->mIndexDataSize && !IndexData)
		{
			for (swMesh* CachedMesh : Meshes)
			{
				CachedMesh->mVertexCacheOffset = -1;
				CachedMesh->mIndexCacheOffset = -1;
			}

			free(VertexData);
			free(IndexData);
			return;
		}

		Mesh->mVertexCacheOffset = VertexDataSize;
		Mesh->mIndexCacheOffset = IndexDataSize;

		if (Mesh->mVertexDataSize)
			memcpy((char*)VertexData + VertexDataSize, Mesh->mVertexData, Mesh->mVertexDataSize);

		if (Mesh->mIndexDataSize)
			memcpy((char*)IndexData + IndexDataSize, Mesh->mIndexData, Mesh->mIndexDataSize);

		VertexDataSize += Mesh->mVertexDataSize;
		IndexDataSize += Mesh->mIndexDataSize;
	}

	Context->DestroyVertexBuffer(mVertexBuffer);
	Context->DestroyIndexBuffer(mIndexBuffer);

	mVertexBuffer = Context->CreateVertexBuffer(VertexDataSize, VertexData);
	if (IndexDataSize)
		mIndexBuffer = Context->CreateIndexBuffer(IndexDataSize, IndexData);
	mBuffersDirty = false;

	free(VertexData);
	free(IndexData);
}

void swPiecesLibrary::ScheduleBufferRepack()
{
	mStreamingBuffersPending = true;

	if (mBufferRepackScheduled)
		return;

	mBufferRepackScheduled = true;

	QTimer::singleShot(120, this, [this]()
	{
		mBufferRepackScheduled = false;

		if (!mStreamingBuffersPending)
			return;

		if (mAssetLoader && mAssetLoader->HasPendingWork())
		{
			ScheduleBufferRepack();
			return;
		}

		mStreamingBuffersPending = false;
		mBuffersDirty = true;
		swView::UpdateAllViews();
	});
}

swMesh* swPiecesLibrary::GetLoadingMesh() const
{
	return mLoadingMesh.get();
}

void swPiecesLibrary::AddTextureReference(swTexture* Texture)
{
	mTextureManager->AddTextureReference(Texture);
}

void swPiecesLibrary::ReleaseTexture(swTexture* Texture)
{
	mTextureManager->ReleaseTexture(Texture);
}

void swPiecesLibrary::UpdateStudStyleAndColors(swStudStyle StudStyle, bool StudCylinderColorEnabled)
{
	Q_ASSERT(QThread::currentThread() == thread());

	if (mStudStyle == StudStyle && mStudCylinderColorEnabled == StudCylinderColorEnabled)
		return;

	// Finish running workers before changing shared primitive data and colors.
	// Queued requests resume with the new settings afterward.
	mAssetLoader->PauseQueuedWork();

	mStudStyle = StudStyle;
	mStudCylinderColorEnabled = StudCylinderColorEnabled;

	LoadColors();
	UpdateStudStyleSource();

	for (const std::unique_ptr<swLibrarySource>& Source : mSources)
	{
		for (const auto& PrimitiveIt : Source->Primitives)
		{
			swLibraryPrimitive* Primitive = PrimitiveIt.second.get();

			if (Primitive->mStudStyle || Primitive->mMeshData.mHasStyleStud)
				Primitive->Unload();
		}
	}

	const auto ReloadPiece = [this](swPieceData* PieceData)
	{
		if (PieceData->mState != swAssetState::Loaded || !PieceData->GetMesh() || !(PieceData->GetMesh()->mFlags & swMeshFlag::HasStyleStud))
			return;

		if (PieceData->IsModel())
		{
			PieceData->ReleaseMesh();
			PieceData->mState = swAssetState::Unloaded;

			RequestPieceDataLoad(PieceData, swLoadPriority::Low);
		}
		else if (!PieceData->IsProject())
		{
			PieceData->Unload();
			RequestPieceDataLoad(PieceData, swLoadPriority::Low);
		}

		mBuffersDirty = true;
	};

	for (swPieceData* PieceData : mPieceManager->GetAllPieceData())
		ReloadPiece(PieceData);

	std::vector<swPiece*> SynthPieces;
	SynthPieces.reserve(mSynthPieces.size());

	for (const auto& Entry : mSynthPieces)
		SynthPieces.push_back(Entry.first);

	for (swPiece* Piece : SynthPieces)
		Piece->RefreshGeneratedMesh();

	mAssetLoader->ResumeQueuedWork();
}

swLibraryPrimitive* swPiecesLibrary::FindPrimitive(const char* Name) const
{
	for (const std::unique_ptr<swLibrarySource>& Source : mSources)
	{
		const auto PrimitiveIt = Source->Primitives.find(Name);

		if (PrimitiveIt != Source->Primitives.end())
			return PrimitiveIt->second.get();
	}

	return nullptr;
}

swResult<void> swPiecesLibrary::LoadPrimitive(swLibraryPrimitive* Primitive)
{
	QThread* Thread = QThread::currentThread();
	swLibraryPrimitive* Parent = nullptr;

	mPrimitiveMutex.lock();

	if (ShouldCancelLoading())
	{
		mPrimitiveMutex.unlock();
		return swUnexpected(tr("Library loading was canceled."));
	}

	if (Primitive->mState == swPrimitiveState::Loaded)
	{
		mPrimitiveMutex.unlock();
		return swResult<void>();
	}

	std::vector<swLibraryPrimitive*>& Stack = mPrimitiveLoadStacks[Thread];

	if (!Stack.empty())
	{
		Parent = Stack.back();

		for (swLibraryPrimitive* Dependency = Primitive; Dependency;)
		{
			if (Dependency == Parent)
			{
				mPrimitiveMutex.unlock();
				return swUnexpected(tr("Primitive include cycle at '%1'.").arg(QString::fromLatin1(Primitive->mName)));
			}

			const auto It = mPrimitiveDependencies.find(Dependency);
			Dependency = It != mPrimitiveDependencies.end() ? It->second : nullptr;
		}

		mPrimitiveDependencies[Parent] = Primitive;
	}

	Stack.push_back(Primitive);
	mPrimitiveMutex.unlock();

	swResult<void> Result = LoadPrimitiveData(Primitive);

	mPrimitiveMutex.lock();

	if (Parent)
		mPrimitiveDependencies.erase(Parent);

	std::vector<swLibraryPrimitive*>& CompletedStack = mPrimitiveLoadStacks[Thread];
	CompletedStack.pop_back();

	if (CompletedStack.empty())
		mPrimitiveLoadStacks.erase(Thread);

	mPrimitiveMutex.unlock();

	return Result;
}

swResult<void> swPiecesLibrary::LoadPrimitiveData(swLibraryPrimitive* Primitive)
{
	mPrimitiveMutex.lock();

	if (ShouldCancelLoading())
	{
		mPrimitiveMutex.unlock();
		return swUnexpected(tr("Library loading was canceled."));
	}

	if (Primitive->mState == swPrimitiveState::NotLoaded)
		Primitive->mState = swPrimitiveState::Loading;
	else
	{
		while (Primitive->mState == swPrimitiveState::Loading)
		{
			if (ShouldCancelLoading())
			{
				mPrimitiveMutex.unlock();
				return swUnexpected(tr("Library loading was canceled."));
			}

			mPrimitiveLoaded.wait(&mPrimitiveMutex, 10);
		}

		const bool Loaded = Primitive->mState == swPrimitiveState::Loaded;
		const QString Error = Primitive->mLoadError;
		mPrimitiveMutex.unlock();

		if (Loaded)
			return swResult<void>();

		return swUnexpected(Error);
	}

	mPrimitiveMutex.unlock();

	swMeshLoader MeshLoader(Primitive->mMeshData, nullptr, swMeshLoaderFlag::Optimize | swMeshLoaderFlag::RequireAllIncludes);

	const auto LoadFailed = [this, Primitive](QString Error) -> swResult<void>
	{
		Primitive->mMeshData.Clear();

		if (Error.isEmpty())
			Error = tr("Could not load included file '%1'.").arg(QString::fromLatin1(Primitive->mName));

		QMutexLocker LoadLock(&mPrimitiveMutex);
		Primitive->mLoadError = Error;
		Primitive->mState = swPrimitiveState::NotLoaded;
		mPrimitiveLoaded.wakeAll();
		return swUnexpected(std::move(Error));
	};

	swLibraryPrimitive* LowPrimitive = nullptr;

	if (mZipFiles[static_cast<int>(swZipFileType::Official)] && Primitive->mStud && !Primitive->mStudStyle)
	{
		if (strncmp(Primitive->mName, "8/", 2)) // todo: low LOD for loose file libraries
		{
			char Name[SW_PIECE_NAME_LENGTH];
			swstrcpy(Name, "8/");
			swstrcat(Name, Primitive->mName);
			swstrupr(Name);

			LowPrimitive = FindPrimitive(Name); // todo: low primitives don't work with studlogo
		}
	}

	const swResult<std::unique_ptr<swFile>> File = OpenPrimitiveFile(Primitive);

	if (!File)
		return LoadFailed(File.error());

	if (!MeshLoader.LoadMesh(*File.value(), LowPrimitive ? SW_MESH_DATA_HIGH : SW_MESH_DATA_SHARED))
		return LoadFailed(MeshLoader.GetError());

	if (LowPrimitive)
	{
		const swResult<std::unique_ptr<swFile>> LowFile = OpenPrimitiveFile(LowPrimitive);

		if (!LowFile)
			return LoadFailed(LowFile.error());

		if (!MeshLoader.LoadMesh(*LowFile.value(), SW_MESH_DATA_LOW))
			return LoadFailed(MeshLoader.GetError());
	}

	mPrimitiveMutex.lock();
	Primitive->mLoadError.clear();
	Primitive->mState = swPrimitiveState::Loaded;
	mPrimitiveLoaded.wakeAll();
	mPrimitiveMutex.unlock();

	return swResult<void>();
}

bool swPiecesLibrary::PieceDataInCategory(swPieceData* PieceData, const char* CategoryKeywords) const
{
	if (!PieceData->IsLibraryPiece())
		return false;

	const char* PieceName;
	if (PieceData->m_strDescription[0] == '~' || PieceData->m_strDescription[0] == '_')
		PieceName = PieceData->m_strDescription + 1;
	else
		PieceName = PieceData->m_strDescription;

	return swMatchCategory(PieceName, CategoryKeywords);
}

void swPiecesLibrary::GetCategoryEntries(int CategoryIndex, bool GroupPieces, std::vector<swPieceData*>& SinglePieces, std::vector<swPieceData*>& GroupedPieces)
{
	if (CategoryIndex >= 0 && CategoryIndex < static_cast<int>(gCategories.size()))
		GetCategoryEntries(gCategories[CategoryIndex].Keywords.constData(), GroupPieces, SinglePieces, GroupedPieces);
}

void swPiecesLibrary::GetCategoryEntries(const char* CategoryKeywords, bool GroupPieces, std::vector<swPieceData*>& SinglePieces, std::vector<swPieceData*>& GroupedPieces)
{
	SinglePieces.clear();
	GroupedPieces.clear();

	for (swPieceData* PieceData : mPieceManager->GetLibraryPieceData())
	{
		if (!PieceDataInCategory(PieceData, CategoryKeywords))
			continue;

		if (!GroupPieces)
		{
			SinglePieces.emplace_back(PieceData);
			continue;
		}

		// Check if it's a patterned piece.
		if (PieceData->IsDecorated())
		{
			swPieceData* Parent = FindPieceData(PieceData->GetPatternParentId().c_str(), nullptr);

			if (Parent)
			{
				// Check if the parent was added as a single piece.
				auto ParentIt = std::find(SinglePieces.begin(), SinglePieces.end(), Parent);

				if (ParentIt != SinglePieces.end())
					SinglePieces.erase(ParentIt);

				if (std::find(GroupedPieces.begin(), GroupedPieces.end(), Parent) == GroupedPieces.end())
					GroupedPieces.emplace_back(Parent);
			}
			else
			{
				// Decorations without a plain parent remain individual entries.
				SinglePieces.emplace_back(PieceData);
			}
		}
		else
		{
			// Check if this piece has already been added to this category by one of its children.
			if (std::find(GroupedPieces.begin(), GroupedPieces.end(), PieceData) == GroupedPieces.end())
				SinglePieces.emplace_back(PieceData);
		}
	}
}

std::vector<swPieceData*> swPiecesLibrary::GetPieces() const
{
	return mPieceManager->GetLibraryPieceData();
}

std::vector<swPieceData*> swPiecesLibrary::GetVisibleTrainTrackPieces(const swTrainTrackConnectionType& ConnectionType) const
{
	std::vector<swPieceData*> Pieces;

	for (swPieceData* PieceData : mPieceManager->GetLibraryPieceData())
	{
		swTrainTrackInformation* TrainTrackInfo = PieceData->GetTrainTrackInfo();

		if (TrainTrackInfo && TrainTrackInfo->IsVisible() && TrainTrackInfo->CanConnectTo(ConnectionType, true))
			Pieces.emplace_back(PieceData);
	}

	return Pieces;
}

std::vector<swPieceData*> swPiecesLibrary::GetPiecesFromSet(const std::vector<std::string>& PieceIds) const
{
	std::vector<swPieceData*> Pieces;
	Pieces.reserve(PieceIds.size());

	for (const std::string& PieceId : PieceIds)
	{
		if (swPieceData* PieceData = mPieceManager->FindPieceData(PieceId, nullptr))
			Pieces.push_back(PieceData);
	}

	return Pieces;
}

std::string swPiecesLibrary::GetPieceId(const swPieceData* PieceData) const
{
	return mPieceManager->GetLibraryPieceDataName(PieceData);
}

bool swPiecesLibrary::LoadBuiltinPieceLibrary()
{
	std::unique_ptr<swDiskFile> File(new swDiskFile(":/resources/data/library.zip"));

	if (!File->Open(QIODevice::ReadOnly) || !OpenArchive(std::move(File), swZipFileType::Official))
		return false;

	swMemoryFile PieceFile;

	for (swPieceData* PieceData : mPieceManager->GetLibraryPieceData())
	{
		mZipFiles[static_cast<int>(PieceData->mZipFileType)]->ExtractFile(PieceData->mZipFileIndex, PieceFile, 256);
		PieceFile.Seek(0, SEEK_END);
		PieceFile.WriteU8(0);

		char* Src = (char*)PieceFile.mBuffer + 2;
		char* Dst = PieceData->m_strDescription;

		for (;;)
		{
			if (*Src != '\r' && *Src != '\n' && *Src && Dst - PieceData->m_strDescription < (int)sizeof(PieceData->m_strDescription) - 1)
			{
				*Dst++ = *Src++;
				continue;
			}

			*Dst = 0;
			break;
		}
	}

	ClassifyPieces(DirectoryFilePaths());

	swLoadDefaultColors(mStudStyle);
	UpdateLoadingMeshColors();
	UpdateStudStyleSource();
	swLoadDefaultCategories(true);
	swSynthesisInitialize();
	swTrainTrackInformation::Initialize(this);

	return true;
}
