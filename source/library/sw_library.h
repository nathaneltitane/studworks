#pragma once

#include "sw_context.h"
#include "sw_loader_mesh.h"
#include "sw_result.h"
#include "sw_types_asset.h"

struct swTrainTrackConnectionType;
class swZipFile;
struct swLibrarySource;

enum class swStudStyle
{
	Plain,
	ThinLinesLogo,
	OutlineLogo,
	SharpTopLogo,
	RoundedTopLogo,
	FlattenedLogo,
	HighContrast,
	HighContrastLogo,
	Count
};

constexpr bool swIsHighContrast(swStudStyle StudStyle)
{
	return StudStyle == swStudStyle::HighContrast || StudStyle == swStudStyle::HighContrastLogo;
}

enum class swZipFileType
{
	Official,
	Unofficial,
	StudStyle,
	Count
};

enum class swLibraryFolderType
{
	Official,
	Unofficial,
	Count
};

enum class swPrimitiveState
{
	NotLoaded,
	Loading,
	Loaded
};

class swLibraryPrimitive
{
public:
	explicit swLibraryPrimitive(QString&& FileName, const char* Name, swZipFileType ZipFileType, quint32 ZipFileIndex, bool Stud, bool StudStyle, bool SubFile);

	void SetZipFile(swZipFileType ZipFileType, quint32 ZipFileIndex)
	{
		mZipFileType = ZipFileType;
		mZipFileIndex = ZipFileIndex;
	}

	void Unload()
	{
		mState = swPrimitiveState::NotLoaded;
		mMeshData.Clear();
		mLoadError.clear();
	}

	QString mFileName;
	char mName[SW_MAXIMUM_NAME];
	swZipFileType mZipFileType;
	quint32 mZipFileIndex;
	swPrimitiveState mState;
	bool mStud;
	bool mStudStyle;
	bool mSubFile;
	swLibraryMeshData mMeshData;
	QString mLoadError;
};

// Coordinates assets, loading and render buffers.
class swPartsLibrary : public QObject
{
	Q_OBJECT

public:
	swPartsLibrary(swStudStyle StudStyle, bool StudCylinderColorEnabled);
	~swPartsLibrary();

	swPartsLibrary(const swPartsLibrary&) = delete;
	swPartsLibrary(swPartsLibrary&&) = delete;
	swPartsLibrary& operator=(const swPartsLibrary&) = delete;
	swPartsLibrary& operator=(swPartsLibrary&&) = delete;

	// Replaces the library and reads its metadata.
	bool Load(const QString& LibraryPath, bool ShowProgress, bool OfficialLibrary);

	// Empty path loads the unofficial archive.
	void SetUnofficialArchivePath(const QString& Path)
	{
		mUnofficialArchivePath = Path;
	}

	bool HasUnofficialArchive() const
	{
		return mZipFiles[static_cast<int>(swZipFileType::Unofficial)] != nullptr;
	}

	// Opens a library in place of the current one.
	bool Reload(const QString& LibraryPath, bool OfficialLibrary);

	const QString& GetLibraryPath() const
	{
		return mLibraryPath;
	}

	bool IsOfficialLibrary() const
	{
		return mOfficialLibrary;
	}

	bool IsArchiveLibrary() const
	{
		return mZipFiles[static_cast<int>(swZipFileType::Official)] != nullptr;
	}

	// Rebinds retired and missing parts.
	std::vector<std::pair<swPartDataReference, swPartDataReference>> RebindProjectPartData(Project* Project);

	// Returns the referenced external projects.
	std::vector<Project*> GetExternalProjects() const;

	bool IsRetiredPartData(const swPartData* PartData) const;

	// Loads the defaults without parts.
	void LoadEmptyLibrary();

	// Resolves a library relative path.
	QString GetLibraryFilePath(const QString& FileName) const
	{
		return mLibraryDir.absoluteFilePath(FileName);
	}

	// Returns the official archive count.
	int GetOfficialPartCount() const
	{
		return mOfficialPartCount;
	}

	// Reloads the configured colors and updates the loading mesh.
	void LoadColors();

	swStudStyle GetStudStyle() const
	{
		return mStudStyle;
	}

	// Drains workers before changing settings.
	void UpdateStudStyleAndColors(swStudStyle StudStyle, bool StudCylinderColorEnabled);

	// Normalizes case and slashes.
	static std::string NormalizePartDataName(const char* PartName);

	// Returns a borrowed project identity.
	swPartData* FindPartData(const char* PartName, Project* Project);

	// Returns a scoped identity reference.
	swPartDataReference AcquirePartData(const char* PartName, Project* Project, bool CreateMissing, bool SearchProjectFolder);

	// Returns a scoped local model identity.
	swPartDataReference AcquireModelPartData(const char* PartName, Project* Project, bool& Reused);

	// Renames the model identity.
	bool RenameModelPartData(swPartData* PartData, const char* NewName);

	// Rechecks an external identity.
	bool RemapProjectPartData(swPartData* PartData, const QString& ProjectDirectory, bool IsPreview);

	// Removes unreferenced non-library identities.
	void RemoveTemporaryPartData();

	// Releases the project identities.
	void ReleaseProjectPartData(Project* OwnerProject);

	// Transfers registrations and indexes.
	void TransferProjectPartData(Project* Source, Project* Destination);

	// Starts loading without waiting.
	void RequestPartDataLoad(swPartData* PartData, swLoadPriority LoadPriority);

	// Starts loading and waits.
	bool WaitForPartData(const swPartDataReference& PartDataRef);
	bool WaitForPartData(const std::vector<swPartDataReference>& PartDataRefs, const std::function<void(int, int)>& Progress = nullptr);

	// Settles children and bounds.
	bool EnsurePartDataAssetsReady(const swPartDataReference& PartData);

	// Resets pending work and readiness.
	void InvalidatePartDataLoad(swPartData* PartData);

	// Rebuilds the model mesh.
	void RebuildModelPartData(swPartData* PartData);

	// Sparse diagnostics; an empty error does not imply readiness.
	void SetPartDataLoadError(const swPartData* PartData, QString Error);
	QString GetPartDataLoadError(const swPartData* PartData) const;
	void ClearPartDataLoadError(const swPartData* PartData);

	// Returns a scoped texture reference.
	swTextureReference AcquireTexture(const char* TextureName, Project* CurrentProject, bool SearchProjectFolder);
	swTextureReference AcquireTexture(const char* TextureName, const std::vector<QString>& SearchDirectories, const std::map<QString, std::shared_ptr<const swEmbeddedData>>& EmbeddedData, swFileSource& Source);

	// Reads a texture image file.
	QByteArray ReadTextureFile(const swTexture* Texture);

	// Starts texture loading without waiting.
	void RequestTextureLoad(swTexture* Texture, swLoadPriority LoadPriority);

	// Starts texture loading and waits.
	bool WaitForTexture(const swTextureReference& TextureRef);

	// Replaces pending generation.
	void RequestSynthMesh(swPart* Part);

	// Cancels pending generation and removes the instance's diagnostic entry.
	void CancelSynthMesh(swPart* Part);

	// Waits for queued generations.
	bool WaitForSynthMeshes(const std::vector<swPart*>& Parts);

	// Returns the last generation error.
	QString GetSynthMeshError(const swPart* Part) const;

	// Returns the parts in name order.
	std::vector<swPartData*> GetParts() const;

	// Matches library-part descriptions against category keywords.
	bool PartDataInCategory(swPartData* PartData, const char* CategoryKeywords) const;

	// Fills the category entries.
	void GetCategoryEntries(int CategoryIndex, bool GroupParts, std::vector<swPartData*>& SingleParts, std::vector<swPartData*>& GroupedParts);
	void GetCategoryEntries(const char* CategoryKeywords, bool GroupParts, std::vector<swPartData*>& SingleParts, std::vector<swPartData*>& GroupedParts);

	// Returns compatible track parts.
	std::vector<swPartData*> GetVisibleTrainTrackParts(const swTrainTrackConnectionType& ConnectionType) const;

	// Resolves library ids in order.
	std::vector<swPartData*> GetPartsFromSet(const std::vector<std::string>& PartIds) const;

	// Returns the library index key.
	std::string GetPartId(const swPartData* PartData) const;

	// Queries primitive sources; FindPrimitive() returns a borrowed entry.
	swLibraryPrimitive* FindPrimitive(const char* Name) const;

	// Loads shared primitive data.
	swResult<void> LoadPrimitive(swLibraryPrimitive* Primitive);

	// Opens an include on the reading thread.
	swResult<std::unique_ptr<swFile>> OpenPrimitiveFile(const swLibraryPrimitive* Primitive);
	swResult<std::unique_ptr<swFile>> OpenPartDataFile(const char* PartName);

	// Shutdown flag for worker builds.
	bool ShouldCancelLoading() const
	{
		return mCancelLoading.load();
	}

	// Notifies consumers to refresh pending/failed asset status.
	void NotifyAssetStatusChanged();

	// Returns the library-owned asynchronous thumbnail service.
	swThumbnailManager* GetThumbnailManager() const
	{
		return mThumbnailManager.get();
	}

	// Rebuilds dirty mesh buffers.
	void UpdateBuffers(swContext* Context);

	// Returns the shared vertex and index buffers.
	std::pair<const swVertexBuffer&, const swIndexBuffer&> GetBuffers() const
	{
		return { mVertexBuffer, mIndexBuffer };
	}

	// Returns the placeholder mesh.
	swMesh* GetLoadingMesh() const;

signals:
	void AssetStatusChanged();
	void PartLoaded(swPartData* PartData);
	void PartLoadFailed(swPartData* PartData, const QString& Error);
	void GeneratedMeshSettled(swPartData* PartData);
	void ColorsLoaded();

protected:
	friend class swAssetLoader;
	template<typename T> friend class swAssetReference;

	// Drains the loader and clears everything.
	void Unload();

	// Retain and release for scoped references.
	void AddPartDataReference(swPartData* PartData);
	void ReleasePartData(swPartData* PartData);
	void AddTextureReference(swTexture* Texture);
	void ReleaseTexture(swTexture* Texture);

	// Snapshots sources for a worker build.
	swPartSourceSnapshot SnapshotPartDataSource(const swPartData* PartData) const;

	// Returns a converted private mesh.
	swPartBuildResult BuildPartData(const swPartSourceSnapshot& Source);

	// Saves geometry with the build settings.
	void SaveBuiltPartDataCache(const swPartSourceSnapshot& Source, swMesh& Mesh);

	// Returns private decoded pixels/errors without GL upload or publication.
	swTextureBuildResult BuildTextureData(const swFileSource& Source);

	// Called when cancelling the current request.
	void PartDataLoadCancelled(swPartData* PartData);

	// Publishes current results.
	bool PublishPartDataMesh(swPartData* PartData, std::unique_ptr<swMesh> Mesh, QString Error);
	bool PublishSynthMesh(swPart* Part, std::unique_ptr<swMesh> Mesh, QString Error);

	// Applies decoded pixels or a failure.
	void PublishTexture(swTexture* Texture, swTextureBuildResult Result);

	// Opens a resolved source on the reading thread.
	swResult<std::unique_ptr<swFile>> OpenFile(const swFileSource& Source);

	// Loads an external project without cycles.
	swResult<Project*> LoadExternalProject(const QFileInfo& FileInfo, bool Preview, bool DeferModelMeshRequests);

	// Discovers source entries before workers consume their metadata.
	bool OpenArchive(const QString& FileName, swZipFileType ZipFileType);
	bool OpenArchive(std::unique_ptr<swFile> File, swZipFileType ZipFileType);
	bool OpenDirectory(const QDir& LibraryDir, bool ShowProgress);

	// Initializes descriptions from source files and compatible description caches.
	void ReadArchiveDescriptions(const QString& OfficialFileName, const QString& UnofficialFileName);
	void ReadDirectoryDescriptions(const QFileInfoList (&FileLists)[static_cast<int>(swLibraryFolderType::Count)], bool ShowProgress);
	void ReadUnofficialArchiveDescriptions(const QFileInfoList (&FileLists)[static_cast<int>(swLibraryFolderType::Count)]);
	void ReadArchivePartDescription(swPartData* PartData, swMemoryFile& PartFile);

	enum class CacheType : quint32
	{
		Archive = 0x0001,
		Directory = 0x0002
	};

	// Reads and writes checked compressed caches.
	bool ReadCacheFile(const QString& FileName, swMemoryFile& CacheFile, CacheType Type);
	bool WriteCacheFile(const QString& FileName, const swMemoryFile& CacheFile, CacheType Type);

	// Restores and saves the descriptions.
	bool LoadCacheIndex(const QString& FileName);
	bool SaveArchiveCacheIndex(const QString& FileName);

	// Builds primitive data on its worker.
	swResult<void> LoadPrimitiveData(swLibraryPrimitive* Primitive);

	static bool IsStudPrimitive(const char* FileName);
	static bool IsStudStylePrimitive(const char* FileName);
	static qint32 MeshCacheSettingsKey(swStudStyle StudStyle, bool StudCylinderColorEnabled);

	// Updates colors after settings change.
	void UpdateLoadingMeshColors();
	void UpdateStudStyleSource();

	// Resolves texture sources on the ui thread.
	static QString FindProjectTextureFile(const QString& ProjectPath, const QString& TextureName);
	swFileSource ResolveTextureSource(const char* TextureName, const std::vector<QString>& SearchDirectories, const std::map<QString, std::shared_ptr<const swEmbeddedData>>& EmbeddedData, QString& ProjectPath);

	// Defers repacking until work settles.
	void ScheduleBufferRepack();

	void ReleaseBuffers();

	using DirectoryFilePaths = std::unordered_map<const swPartData*, QString>;

	// Classifies parts after reading.
	void ClassifyParts(const DirectoryFilePaths& FilePaths);
	static std::string ReadAliasTarget(swFile& File);

	// Returns the unofficial archive path.
	QString GetUnofficialArchiveFileName() const;

	// Bounded description cache strings.
	static bool ReadIndexString(swMemoryFile& File, std::string& String);
	static bool WriteIndexString(swMemoryFile& File, const std::string& String);

	// Library sources, drained before changes.
	QDir mLibraryDir;
	std::unique_ptr<swZipFile> mZipFiles[static_cast<int>(swZipFileType::Count)];
	std::vector<std::unique_ptr<swLibrarySource>> mSources;
	bool mHasUnofficial;
	QString mLibraryPath;
	bool mOfficialLibrary = false;
	QString mUnofficialArchivePath;

	// Captured before unofficial additions/overrides; custom libraries skip update checks.
	int mOfficialPartCount = 0;

	// Settings captured for worker builds.
	swStudStyle mStudStyle;
	bool mStudCylinderColorEnabled;

	// Identity managers and services.
	std::unique_ptr<swPartManager> mPartManager;
	std::unique_ptr<swTextureManager> mTextureManager;
	std::unique_ptr<swAssetLoader> mAssetLoader;
	std::unique_ptr<swThumbnailManager> mThumbnailManager;

	// Live synth instances and their errors.
	std::unordered_map<swPart*, QString> mSynthParts;

	// Active dependency edges.
	QMutex mPrimitiveMutex;
	QWaitCondition mPrimitiveLoaded;
	std::unordered_map<QThread*, std::vector<swLibraryPrimitive*>> mPrimitiveLoadStacks;
	std::unordered_map<swLibraryPrimitive*, swLibraryPrimitive*> mPrimitiveDependencies;

	// Texture names to their sources.
	std::map<QString, QString> mTextureDiskEntries;
	std::map<QString, int> mTextureArchiveEntries[static_cast<int>(swZipFileType::Count)];

	// Cache location and official/unofficial archive size/timestamp validation data.
	QString mCachePath;
	qint64 mArchiveCheckSum[4];

	// Shared gpu buffers and repacking state.
	swVertexBuffer mVertexBuffer;
	swIndexBuffer mIndexBuffer;
	bool mBuffersDirty;
	std::unique_ptr<swMesh> mLoadingMesh;
	bool mStreamingBuffersPending = false;
	bool mBufferRepackScheduled = false;

	// Shutdown flag polled by workers.
	std::atomic_bool mCancelLoading = false;
};
