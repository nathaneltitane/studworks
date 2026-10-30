#include "sw_global.h"
#include "sw_library.h"
#include "sw_loader_asset.h"
#include "sw_manager_thumbnail.h"
#include "sw_file_zip.h"
#include "sw_file.h"
#include "sw_data_part.h"
#include "sw_manager_part.h"
#include "sw_texture.h"
#include "sw_manager_texture.h"
#include "sw_category.h"
#include "sw_extensions_opengl.h"
#include "sw_synthesis.h"
#include "sw_track_train.h"
#include "sw_project.h"
#include "sw_model.h"
#include "sw_profile.h"
#include "sw_view.h"
#include "sw_string.h"

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

swPartsLibrary::swPartsLibrary(swStudStyle StudStyle, bool StudCylinderColorEnabled)
	: mStudStyle(StudStyle), mStudCylinderColorEnabled(StudCylinderColorEnabled)
{
	mPartManager = std::make_unique<swPartManager>();
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

swPartsLibrary::~swPartsLibrary()
{
	mThumbnailManager.reset();
	mCancelLoading.store(true);
	mAssetLoader.reset();
	Unload();
	ReleaseBuffers();
}

void swPartsLibrary::Unload()
{
	Q_ASSERT(QThread::currentThread() == thread());

	const bool HadLoader = static_cast<bool>(mAssetLoader);

	if (HadLoader && mThumbnailManager)
		mThumbnailManager->Clear();

	mAssetLoader.reset();
	mSynthParts.clear();

	mPartManager->Clear();

	mSources.clear();

	mTextureManager->Clear();
	mTextureDiskEntries.clear();

	for (std::map<QString, int>& Entries : mTextureArchiveEntries)
		Entries.clear();

	mOfficialPartCount = 0;

	for (std::unique_ptr<swZipFile>& ZipFile : mZipFiles)
		ZipFile.reset();

	if (HadLoader)
		mAssetLoader = std::unique_ptr<swAssetLoader>(new swAssetLoader(this));
}

void swPartsLibrary::RemoveTemporaryPartData()
{
	mPartManager->RemoveTemporaryPartData();
}

void swPartsLibrary::ReleaseProjectPartData(Project* OwnerProject)
{
	mPartManager->ReleaseProjectPartData(OwnerProject);
}

void swPartsLibrary::TransferProjectPartData(Project* Source, Project* Destination)
{
	mPartManager->TransferProjectPartData(Source, Destination);
}

bool swPartsLibrary::RenameModelPartData(swPartData* PartData, const char* NewName)
{
	if (!mPartManager->RenameModelPartData(PartData, NewName))
		return false;

	if (PartData->mState != swAssetState::Failed)
		ClearPartDataLoadError(PartData);

	return true;
}

std::string swPartsLibrary::NormalizePartDataName(const char* PartName)
{
	return swPartManager::NormalizePartDataName(PartName);
}

swPartDataReference swPartsLibrary::AcquireModelPartData(const char* PartName, Project* Project, bool& Reused)
{
	swPartData* PartData = mPartManager->AcquireModelPartData(PartName, Project, Reused);

	swPartDataReference PartDataRef = swPartDataReference::Adopt(PartData, this);

	if (Reused)
		InvalidatePartDataLoad(PartData);

	return PartDataRef;
}

swResult<Project*> swPartsLibrary::LoadExternalProject(const QFileInfo& FileInfo, bool Preview, bool DeferModelMeshRequests)
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

swPartData* swPartsLibrary::FindPartData(const char* PartName, Project* CurrentProject)
{
	const std::string Name = NormalizePartDataName(PartName);

	return mPartManager->FindPartData(Name, CurrentProject);
}

swPartDataReference swPartsLibrary::AcquirePartData(const char* PartName, Project* CurrentProject, bool CreateMissing, bool SearchProjectFolder)
{
	Q_ASSERT(QThread::currentThread() == thread());

	QString ProjectPath;
	QString LoadError;

	if (swPartData* PartData = FindPartData(PartName, CurrentProject))
		return swPartDataReference(PartData, this);

	if (CurrentProject && SearchProjectFolder && !CurrentProject->GetFileName().isEmpty())
		ProjectPath = QFileInfo(CurrentProject->GetFileName()).absolutePath();

	if (!ProjectPath.isEmpty())
	{
		QFileInfo ProjectFile = QFileInfo(ProjectPath + QDir::separator() + PartName);

		if (ProjectFile.isFile())
		{
			const swResult<Project*> ExternalProject = LoadExternalProject(ProjectFile, CurrentProject && CurrentProject->IsPreview(), CurrentProject && CurrentProject->DefersModelMeshRequests());

			if (ExternalProject)
			{
				swPartData* PartData = mPartManager->CreatePartData(PartName, CurrentProject);

				PartData->CreateProject(ExternalProject.value(), PartName);

				return swPartDataReference(PartData, this);
			}
			else
				LoadError = ExternalProject.error();
		}
	}

	if (CreateMissing)
	{
		swPartData* PartData = mPartManager->CreatePartData(PartName, CurrentProject);

		PartData->SetFailed(LoadError.isEmpty() ? tr("Could not find part '%1'.").arg(QString::fromLatin1(PartName)) : LoadError);

		return swPartDataReference(PartData, this);
	}

	return swPartDataReference();
}

bool swPartsLibrary::RemapProjectPartData(swPartData* PartData, const QString& ProjectDirectory, bool IsPreview)
{
	const QByteArray PartName(PartData->mFileName);
	const QFileInfo Candidate(QDir(ProjectDirectory).absoluteFilePath(QString::fromLatin1(PartName.constData())));

	if (PartData->IsProject() && Candidate.isFile() &&
		QFileInfo(PartData->GetProject()->GetFileName()).canonicalFilePath() == Candidate.canonicalFilePath())
		return false;

	if (!PartData->IsProject() && !Candidate.isFile())
		return false;

	InvalidatePartDataLoad(PartData);

	if (Candidate.isFile())
	{
		const swResult<Project*> ExternalProject = LoadExternalProject(Candidate, IsPreview, true);

		if (ExternalProject)
			PartData->CreateProject(ExternalProject.value(), PartName.constData());
		else
		{
			PartData->DetachContainer();
			PartData->SetFailed(ExternalProject.error());
		}
	}
	else
	{
		PartData->DetachContainer();
		PartData->SetFailed(tr("Could not find external model file '%1'.").arg(Candidate.absoluteFilePath()));
	}

	NotifyAssetStatusChanged();

	return true;
}

swTextureReference swPartsLibrary::AcquireTexture(const char* TextureName, Project* CurrentProject, bool SearchProjectFolder)
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

QString swPartsLibrary::FindProjectTextureFile(const QString& ProjectPath, const QString& TextureName)
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

swFileSource swPartsLibrary::ResolveTextureSource(const char* TextureName, const std::vector<QString>& SearchDirectories, const std::map<QString, std::shared_ptr<const swEmbeddedData>>& EmbeddedData, QString& ProjectPath)
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

	// finish prefixed paths first
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
			const bool Unofficial = Folder.startsWith(QLatin1String("unofficial/"));

			if (!ArchiveLibrary)
			{
				const QString EntryName = QDir::cleanPath(Folder + Candidate + QLatin1String(".png")).toUpper();
				const auto Entry = mTextureDiskEntries.find(EntryName);

				if (Entry != mTextureDiskEntries.end())
				{
					Source.FilePath = Entry->second;
					break;
				}
			}

			// folders also read the unofficial archive
			if (ArchiveLibrary || (Unofficial && mZipFiles[static_cast<int>(swZipFileType::Unofficial)]))
			{
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

swTextureReference swPartsLibrary::AcquireTexture(const char* TextureName, const std::vector<QString>& SearchDirectories, const std::map<QString, std::shared_ptr<const swEmbeddedData>>& EmbeddedData, swFileSource& Source)
{
	QString ProjectPath;
	Source = ResolveTextureSource(TextureName, SearchDirectories, EmbeddedData, ProjectPath);

	return swTextureReference::Adopt(mTextureManager->AcquireTexture(TextureName, Source, ProjectPath), this);
}

void swPartsLibrary::RequestTextureLoad(swTexture* Texture, swLoadPriority LoadPriority)
{
	Q_ASSERT(QThread::currentThread() == thread());

	mAssetLoader->RequestTextureLoad(Texture, Texture->mSource, LoadPriority);
}

bool swPartsLibrary::WaitForTexture(const swTextureReference& TextureRef)
{
	Q_ASSERT(QThread::currentThread() == thread());

	return mAssetLoader->WaitForTexture(TextureRef.Get(), TextureRef->mSource);
}

swTextureBuildResult swPartsLibrary::BuildTextureData(const swFileSource& Source)
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

// use to read a texture image file
QByteArray swPartsLibrary::ReadTextureFile(const swTexture* Texture)
{
	const swResult<std::unique_ptr<swFile>> File = OpenFile(Texture->mSource);

	if (!File)
		return QByteArray();

	swFile& Source = *File.value();
	QByteArray Data(static_cast<int>(Source.GetLength()), Qt::Uninitialized);
	Data.resize(static_cast<int>(Source.ReadBuffer(Data.data(), Data.size())));

	return Data;
}

swResult<std::unique_ptr<swFile>> swPartsLibrary::OpenFile(const swFileSource& Source)
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

	// archives stay until workers drain
	const std::unique_ptr<swZipFile>& Archive = mZipFiles[Source.ArchiveType];

	if (!Archive || static_cast<size_t>(Source.ArchiveIndex) >= Archive->mFiles.size())
		return swUnexpected(tr("Could not read archive entry %1.").arg(Source.ArchiveIndex));

	if (!Archive->ExtractFile(Source.ArchiveIndex, *File))
		return swUnexpected(tr("Could not read archive file '%1'.").arg(QString::fromLatin1(Archive->mFiles[Source.ArchiveIndex].file_name)));

	File->Seek(0, SEEK_SET);
	return std::unique_ptr<swFile>(std::move(File));
}

bool swPartsLibrary::Load(const QString& LibraryPath, bool ShowProgress, bool OfficialLibrary)
{
	Unload();

	mLibraryPath = LibraryPath;
	mOfficialLibrary = OfficialLibrary;

	if (OpenArchive(LibraryPath, swZipFileType::Official))
	{
		if (OfficialLibrary)
			mOfficialPartCount = static_cast<int>(mPartManager->GetLibraryPartDataCount());

		LoadColors();

		mLibraryDir = QFileInfo(LibraryPath).absoluteDir();
		QString UnofficialFileName = GetUnofficialArchiveFileName();

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

QString swPartsLibrary::GetUnofficialArchiveFileName() const
{
	return mUnofficialArchivePath.isEmpty() ? mLibraryDir.absoluteFilePath(QLatin1String("ldrawunf.zip")) : mUnofficialArchivePath;
}

// use to swap the library in place
bool swPartsLibrary::Reload(const QString& LibraryPath, bool OfficialLibrary)
{
	Q_ASSERT(QThread::currentThread() == thread());

	if (LibraryPath.isEmpty() || !mAssetLoader)
		return false;

	bool ArchiveLibrary = false;

	// keep the library on a bad path
	{
		std::unique_ptr<swDiskFile> File(new swDiskFile(LibraryPath));
		swZipFile ZipFile;

		ArchiveLibrary = File->Open(QIODevice::ReadOnly) && ZipFile.OpenRead(std::move(File));
	}

	if (!ArchiveLibrary && QDir(QDir(LibraryPath).absoluteFilePath(QLatin1String("parts/")), QLatin1String("*.dat"), QDir::NoSort, QDir::Files | QDir::Hidden | QDir::Readable).isEmpty())
		return false;

	mAssetLoader->PauseQueuedWork();
	mThumbnailManager->Clear();
	mPartManager->RemoveTemporaryPartData();

	// missing parts free their names
	for (swPartData* PartData : mPartManager->GetLibraryPartData())
	{
		if (!PartData->IsLibraryPart())
			mPartManager->RetirePartData(PartData);
		else
			PartData->SetZipFile(swZipFileType::Count, -1);
	}

	mPartManager->DetachLibraryPartData();
	mTextureManager->RetireLibraryTextures(mZipFiles[static_cast<int>(swZipFileType::Official)] ? QString() : mLibraryDir.absolutePath());

	for (std::map<QString, int>& Entries : mTextureArchiveEntries)
		Entries.clear();

	mTextureDiskEntries.clear();
	mSources.clear();
	mHasUnofficial = false;
	mOfficialPartCount = 0;

	for (std::unique_ptr<swZipFile>& ZipFile : mZipFiles)
		ZipFile.reset();

	mLibraryPath = LibraryPath;
	mOfficialLibrary = OfficialLibrary;

	if (ArchiveLibrary)
	{
		OpenArchive(LibraryPath, swZipFileType::Official);

		if (OfficialLibrary)
			mOfficialPartCount = static_cast<int>(mPartManager->GetLibraryPartDataCount());

		mLibraryDir = QFileInfo(LibraryPath).absoluteDir();
		QString UnofficialFileName = GetUnofficialArchiveFileName();

		if (!OpenArchive(UnofficialFileName, swZipFileType::Unofficial))
			UnofficialFileName.clear();

		ReadArchiveDescriptions(LibraryPath, UnofficialFileName);
	}
	else
	{
		mLibraryDir.setPath(LibraryPath);
		OpenDirectory(mLibraryDir, true);
	}

	LoadColors();
	UpdateStudStyleSource();

	// vanished parts become missing
	for (swPartData* PartData : mPartManager->RetireDetachedPartData())
	{
		InvalidatePartDataLoad(PartData);
		PartData->SetFailed(tr("Could not find part '%1'.").arg(QString::fromLatin1(PartData->mFileName)));
	}

	swSynthesisInitialize();
	swTrainTrackInformation::Initialize(this);

	// queue fresh meshes
	for (swPartData* PartData : mPartManager->GetAllPartData())
	{
		if (PartData->IsModel())
		{
			if (PartData->mState == swAssetState::Loaded && PartData->GetModel()->HasDirectGeometry())
				RebuildModelPartData(PartData);
		}
		else if (PartData->IsLibraryPart() && PartData->mState != swAssetState::Unloaded && !PartData->IsLoading())
		{
			PartData->Unload();

			if (PartData->GetRefCount() > 0)
				RequestPartDataLoad(PartData, swLoadPriority::Low);
		}
	}

	mBuffersDirty = true;

	std::vector<swPart*> SynthParts;
	SynthParts.reserve(mSynthParts.size());

	for (const auto& Entry : mSynthParts)
		SynthParts.push_back(Entry.first);

	for (swPart* Part : SynthParts)
		Part->RefreshGeneratedMesh();

	mAssetLoader->ResumeQueuedWork();
	NotifyAssetStatusChanged();

	return true;
}

std::vector<std::pair<swPartDataReference, swPartDataReference>> swPartsLibrary::RebindProjectPartData(Project* CurrentProject)
{
	Q_ASSERT(QThread::currentThread() == thread());

	std::map<swPartData*, swPartDataReference> Resolved;
	std::vector<std::pair<swPartDataReference, swPartDataReference>> Replacements;

	for (const std::unique_ptr<swModel>& Model : CurrentProject->GetModels())
	{
		std::vector<swPartData*> ModelParts;

		for (const std::unique_ptr<swPart>& Part : Model->GetParts())
			if (std::find(ModelParts.begin(), ModelParts.end(), Part->mPartData.Get()) == ModelParts.end())
				ModelParts.push_back(Part->mPartData.Get());

		for (swPartData* PartData : ModelParts)
		{
			auto ResolvedIt = Resolved.find(PartData);

			if (ResolvedIt == Resolved.end())
			{
				swPartDataReference Replacement;

				if (PartData->IsLibraryPart())
				{
					if (mPartManager->IsRetiredPartData(PartData))
						Replacement = AcquirePartData(PartData->mFileName, CurrentProject, true, true);
				}
				else if (!PartData->IsModel() && !PartData->IsProject())
				{
					swPartData* LibraryData = FindPartData(PartData->mFileName, nullptr);

					if (LibraryData && LibraryData->IsLibraryPart())
						Replacement = swPartDataReference(LibraryData);
				}

				ResolvedIt = Resolved.emplace(PartData, Replacement).first;

				if (Replacement)
					Replacements.emplace_back(swPartDataReference(PartData), Replacement);
			}

			if (ResolvedIt->second)
				Model->RebindPartData(PartData, ResolvedIt->second.Get());
		}
	}

	// promoted missing parts free their names
	for (const std::pair<swPartDataReference, swPartDataReference>& Replacement : Replacements)
		if (!Replacement.first->IsLibraryPart())
			mPartManager->RetirePartData(Replacement.first.Get());

	return Replacements;
}

std::vector<Project*> swPartsLibrary::GetExternalProjects() const
{
	std::vector<Project*> Projects;

	for (swPartData* PartData : mPartManager->GetAllPartData())
		if (PartData->IsProject())
			Projects.push_back(PartData->GetProject());

	return Projects;
}

bool swPartsLibrary::IsRetiredPartData(const swPartData* PartData) const
{
	return mPartManager->IsRetiredPartData(PartData);
}

void swPartsLibrary::LoadColors()
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

void swPartsLibrary::UpdateLoadingMeshColors()
{
	swMeshSection* Sections = mLoadingMesh->mLods[SW_MESH_LOD_HIGH].Sections;

	Sections[0].ColorIndex = gDefaultColor;
	Sections[1].ColorIndex = gEdgeColor;
}

bool swPartsLibrary::IsStudPrimitive(const char* FileName)
{
	return memcmp(FileName, "STU", 3) == 0;
}

bool swPartsLibrary::IsStudStylePrimitive(const char* FileName)
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

qint32 swPartsLibrary::MeshCacheSettingsKey(swStudStyle StudStyle, bool StudCylinderColorEnabled)
{
	return (SW_LIBRARY_MESH_CACHE_VERSION << 16) | (static_cast<qint32>(StudStyle) << 1) | static_cast<qint32>(StudCylinderColorEnabled);
}

void swPartsLibrary::UpdateStudStyleSource()
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

bool swPartsLibrary::OpenArchive(const QString& FileName, swZipFileType ZipFileType)
{
	std::unique_ptr<swDiskFile> File(new swDiskFile(FileName));

	if (!File->Open(QIODevice::ReadOnly))
		return false;

	return OpenArchive(std::move(File), ZipFileType);
}

bool swPartsLibrary::OpenArchive(std::unique_ptr<swFile> File, swZipFileType ZipFileType)
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
		const std::string NormalizedName = NormalizePartDataName(FileInfo.file_name);
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
				swPartData* PartData = FindPartData(Name, nullptr);

				// unofficial parts never replace official ones
				if (PartData && ZipFileType == swZipFileType::Unofficial && PartData->IsLibraryPart() && PartData->mZipFileType != swZipFileType::Unofficial)
					continue;

				if (!PartData)
				{
					PartData = mPartManager->CreateLibraryPartData(Name, FileInfo.file_name + (Name - NormalizedName.c_str()));
				}

				PartData->SetZipFile(ZipFileType, FileIdx);
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

bool swPartsLibrary::OpenDirectory(const QDir& LibraryDir, bool ShowProgress)
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
				const std::string Name = NormalizePartDataName(RelativeName);

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

		// index png paths once
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

	// unofficial archive fills the gaps
	if (OpenArchive(GetUnofficialArchiveFileName(), swZipFileType::Unofficial))
	{
		mHasUnofficial = true;
		ReadUnofficialArchiveDescriptions(FileLists);
	}

	return true;
}

void swPartsLibrary::ReleaseBuffers()
{
	mBuffersDirty = true;

	// library only loading needs no renderer
	if (!mVertexBuffer.IsValid() && !mIndexBuffer.IsValid())
		return;

	swContext* Context = swContext::GetGlobalOffscreenContext();

	Context->MakeCurrent();
	Context->DestroyVertexBuffer(mVertexBuffer);
	Context->DestroyIndexBuffer(mIndexBuffer);
}

void swPartsLibrary::UpdateBuffers(swContext* Context)
{
	if (!gSupportsVertexBufferObject || !mBuffersDirty)
		return;

	int VertexDataSize = 0;
	int IndexDataSize = 0;
	std::vector<swMesh*> Meshes;

	const auto AddMesh = [&VertexDataSize, &IndexDataSize, &Meshes](swPartData* PartData)
	{
		swMesh* Mesh = PartData->GetMesh();

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

	for (swPartData* PartData : mPartManager->GetAllPartData())
		AddMesh(PartData);

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

void swPartsLibrary::ScheduleBufferRepack()
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

swMesh* swPartsLibrary::GetLoadingMesh() const
{
	return mLoadingMesh.get();
}

void swPartsLibrary::AddTextureReference(swTexture* Texture)
{
	mTextureManager->AddTextureReference(Texture);
}

void swPartsLibrary::ReleaseTexture(swTexture* Texture)
{
	mTextureManager->ReleaseTexture(Texture);
}

void swPartsLibrary::UpdateStudStyleAndColors(swStudStyle StudStyle, bool StudCylinderColorEnabled)
{
	Q_ASSERT(QThread::currentThread() == thread());

	if (mStudStyle == StudStyle && mStudCylinderColorEnabled == StudCylinderColorEnabled)
		return;

	// finish workers before shared changes
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

	const auto ReloadPart = [this](swPartData* PartData)
	{
		if (PartData->mState != swAssetState::Loaded || !PartData->GetMesh() || !(PartData->GetMesh()->mFlags & swMeshFlag::HasStyleStud))
			return;

		if (PartData->IsModel())
		{
			PartData->ReleaseMesh();
			PartData->mState = swAssetState::Unloaded;

			RequestPartDataLoad(PartData, swLoadPriority::Low);
		}
		else if (!PartData->IsProject())
		{
			PartData->Unload();
			RequestPartDataLoad(PartData, swLoadPriority::Low);
		}

		mBuffersDirty = true;
	};

	for (swPartData* PartData : mPartManager->GetAllPartData())
		ReloadPart(PartData);

	std::vector<swPart*> SynthParts;
	SynthParts.reserve(mSynthParts.size());

	for (const auto& Entry : mSynthParts)
		SynthParts.push_back(Entry.first);

	for (swPart* Part : SynthParts)
		Part->RefreshGeneratedMesh();

	mAssetLoader->ResumeQueuedWork();
}

swLibraryPrimitive* swPartsLibrary::FindPrimitive(const char* Name) const
{
	for (const std::unique_ptr<swLibrarySource>& Source : mSources)
	{
		const auto PrimitiveIt = Source->Primitives.find(Name);

		if (PrimitiveIt != Source->Primitives.end())
			return PrimitiveIt->second.get();
	}

	return nullptr;
}

swResult<void> swPartsLibrary::LoadPrimitive(swLibraryPrimitive* Primitive)
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

swResult<void> swPartsLibrary::LoadPrimitiveData(swLibraryPrimitive* Primitive)
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
		// todo low lod for folder libraries
		if (strncmp(Primitive->mName, "8/", 2))
		{
			char Name[SW_PART_NAME_LENGTH];
			swstrcpy(Name, "8/");
			swstrcat(Name, Primitive->mName);
			swstrupr(Name);

			// todo low primitives with studlogo
			LowPrimitive = FindPrimitive(Name);
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

bool swPartsLibrary::PartDataInCategory(swPartData* PartData, const char* CategoryKeywords) const
{
	// sub parts and aliases are unlisted
	if (!PartData->IsLibraryPart() || PartData->IsSubPart() || PartData->IsAlias())
		return false;

	const char* PartName;
	if (PartData->m_strDescription[0] == '_')
		PartName = PartData->m_strDescription + 1;
	else
		PartName = PartData->m_strDescription;

	return swMatchCategory(PartName, CategoryKeywords);
}

void swPartsLibrary::GetCategoryEntries(int CategoryIndex, bool GroupParts, std::vector<swPartData*>& SingleParts, std::vector<swPartData*>& GroupedParts)
{
	if (CategoryIndex >= 0 && CategoryIndex < static_cast<int>(gCategories.size()))
		GetCategoryEntries(gCategories[CategoryIndex].Keywords.constData(), GroupParts, SingleParts, GroupedParts);
}

void swPartsLibrary::GetCategoryEntries(const char* CategoryKeywords, bool GroupParts, std::vector<swPartData*>& SingleParts, std::vector<swPartData*>& GroupedParts)
{
	SingleParts.clear();
	GroupedParts.clear();

	for (swPartData* PartData : mPartManager->GetLibraryPartData())
	{
		if (!PartDataInCategory(PartData, CategoryKeywords))
			continue;

		if (!GroupParts)
		{
			SingleParts.emplace_back(PartData);
			continue;
		}

		// patterned part
		if (PartData->IsDecorated())
		{
			swPartData* Parent = FindPartData(PartData->GetPatternParentId().c_str(), nullptr);

			if (Parent)
			{
				// parent added as a single part
				auto ParentIt = std::find(SingleParts.begin(), SingleParts.end(), Parent);

				if (ParentIt != SingleParts.end())
					SingleParts.erase(ParentIt);

				if (std::find(GroupedParts.begin(), GroupedParts.end(), Parent) == GroupedParts.end())
					GroupedParts.emplace_back(Parent);
			}
			else
			{
				// decorations without a parent stay separate
				SingleParts.emplace_back(PartData);
			}
		}
		else
		{
			// skip parts added by a child
			if (std::find(GroupedParts.begin(), GroupedParts.end(), PartData) == GroupedParts.end())
				SingleParts.emplace_back(PartData);
		}
	}
}

std::vector<swPartData*> swPartsLibrary::GetParts() const
{
	return mPartManager->GetLibraryPartData();
}

std::vector<swPartData*> swPartsLibrary::GetVisibleTrainTrackParts(const swTrainTrackConnectionType& ConnectionType) const
{
	std::vector<swPartData*> Parts;

	for (swPartData* PartData : mPartManager->GetLibraryPartData())
	{
		swTrainTrackInformation* TrainTrackInfo = PartData->GetTrainTrackInfo();

		if (TrainTrackInfo && TrainTrackInfo->IsVisible() && TrainTrackInfo->CanConnectTo(ConnectionType, true))
			Parts.emplace_back(PartData);
	}

	return Parts;
}

std::vector<swPartData*> swPartsLibrary::GetPartsFromSet(const std::vector<std::string>& PartIds) const
{
	std::vector<swPartData*> Parts;
	Parts.reserve(PartIds.size());

	for (const std::string& PartId : PartIds)
	{
		if (swPartData* PartData = mPartManager->FindPartData(PartId, nullptr))
			Parts.push_back(PartData);
	}

	return Parts;
}

std::string swPartsLibrary::GetPartId(const swPartData* PartData) const
{
	return mPartManager->GetLibraryPartDataName(PartData);
}

// use to set the defaults without a library
void swPartsLibrary::LoadEmptyLibrary()
{
	Unload();

	swLoadDefaultColors(mStudStyle);
	UpdateLoadingMeshColors();
	UpdateStudStyleSource();
	swLoadDefaultCategories(true);
	swSynthesisInitialize();
	swTrainTrackInformation::Initialize(this);
}
