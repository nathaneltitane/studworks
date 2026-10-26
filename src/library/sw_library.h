#pragma once

#include "sw_context.h"
#include "sw_mesh_loader.h"
#include "sw_result.h"
#include "sw_asset_types.h"

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

// Coordinates asset sources, loading, settings and shared render buffers.
// Piece/texture managers own identities and references; the loader owns pending work.
// Acquisition, scheduling and publication run on the UI thread; workers use source snapshots.
class swPiecesLibrary : public QObject
{
	Q_OBJECT

public:
	swPiecesLibrary(swStudStyle StudStyle, bool StudCylinderColorEnabled);
	~swPiecesLibrary();

	swPiecesLibrary(const swPiecesLibrary&) = delete;
	swPiecesLibrary(swPiecesLibrary&&) = delete;
	swPiecesLibrary& operator=(const swPiecesLibrary&) = delete;
	swPiecesLibrary& operator=(swPiecesLibrary&&) = delete;

	// Replaces the current library with an archive or directory and initializes its metadata.
	// OfficialLibrary enables archive update checks; directories remain excluded.
	bool Load(const QString& LibraryPath, bool ShowProgress, bool OfficialLibrary);

	// Loads the bundled fallback library and its default settings.
	bool LoadBuiltinPieceLibrary();

	// Resolves a file path relative to the library directory without opening it.
	QString GetLibraryFilePath(const QString& FileName) const
	{
		return mLibraryDir.absoluteFilePath(FileName);
	}

	// Returns the official archive count for update checks, or zero for custom/directory libraries.
	int GetOfficialPieceCount() const
	{
		return mOfficialPieceCount;
	}

	// Reloads the configured colors and updates the loading mesh.
	void LoadColors();

	swStudStyle GetStudStyle() const
	{
		return mStudStyle;
	}

	// Drains running workers before changing settings; queues replacement meshes asynchronously.
	void UpdateStudStyleAndColors(swStudStyle StudStyle, bool StudCylinderColorEnabled);

	// Normalizes case and slashes, preserving the piece name length limit.
	static std::string NormalizePieceDataName(const char* PieceName);

	// Returns a borrowed project identity, falling back to the library. No I/O or creation.
	swPieceData* FindPieceData(const char* PieceName, Project* Project);

	// Resolves or creates an identity and returns one scoped reference; does not ensure readiness.
	swPieceDataReference AcquirePieceData(const char* PieceName, Project* Project, bool CreateMissing, bool SearchProjectFolder);

	// Creates or reuses a local identity and returns one scoped reference for the model.
	// Reused data is invalidated before returning.
	swPieceDataReference AcquireModelPieceData(const char* PieceName, Project* Project, bool& Reused);

	// Renames the model identity and its index key, rejecting collisions in the same scope.
	bool RenameModelPieceData(swPieceData* PieceData, const char* NewName);

	// Rechecks an external/missing identity against the project directory; returns whether it changed.
	bool RemapProjectPieceData(swPieceData* PieceData, const QString& ProjectDirectory, bool IsPreview);

	// Removes unreferenced non-library identities.
	void RemoveTemporaryPieceData();

	// Detaches the project's borrowed index and deletes its unreferenced local identities.
	void ReleaseProjectPieceData(Project* OwnerProject);

	// Transfers registrations and borrowed indexes, preserving destination name collisions.
	void TransferProjectPieceData(Project* Source, Project* Destination);

	// Starts or promotes asynchronous loading. Adds no consumer references and never waits.
	// Priority controls scheduling only; the caller retains the identity while it needs the result.
	void RequestPieceDataLoad(swPieceData* PieceData, swLoadPriority LoadPriority);

	// Starts or promotes loading, then waits. Adds no consumer references; false means failure/cancellation.
	// Does not traverse container children or wait for per-instance synth geometry.
	bool WaitForPieceData(const swPieceDataReference& PieceDataRef);
	bool WaitForPieceData(const std::vector<swPieceDataReference>& PieceDataRefs, const std::function<void(int, int)>& Progress = nullptr);

	// Also settles container children/synths and updates bounds; the caller retains the identity.
	bool EnsurePieceDataAssetsReady(const swPieceDataReference& PieceData);

	// Obsoletes pending work and resets readiness without acquiring a reference.
	void InvalidatePieceDataLoad(swPieceData* PieceData);

	// Invalidates the model mesh, releases old geometry and requests an asynchronous rebuild.
	void RebuildModelPieceData(swPieceData* PieceData);

	// Sparse diagnostics; an empty error does not imply readiness.
	void SetPieceDataLoadError(const swPieceData* PieceData, QString Error);
	QString GetPieceDataLoadError(const swPieceData* PieceData) const;
	void ClearPieceDataLoadError(const swPieceData* PieceData);

	// Resolves or creates a texture and returns one scoped reference without decoding or uploading.
	swTextureReference AcquireTexture(const char* TextureName, Project* CurrentProject, bool SearchProjectFolder);
	swTextureReference AcquireTexture(const char* TextureName, const std::vector<QString>& SearchDirectories, const std::map<QString, std::shared_ptr<const swEmbeddedData>>& EmbeddedData, swFileSource& Source);

	// Starts or promotes asynchronous loading; the caller retains the texture. Never waits.
	void RequestTextureLoad(swTexture* Texture, swLoadPriority LoadPriority);

	// Starts or promotes loading, then waits for settlement; the caller retains the texture.
	bool WaitForTexture(const swTextureReference& TextureRef);

	// Replaces pending generation for this instance; snapshots control points for workers.
	void RequestSynthMesh(swPiece* Piece);

	// Cancels pending generation and removes the instance's diagnostic entry.
	void CancelSynthMesh(swPiece* Piece);

	// Promotes and waits for queued generations; false means missing or outdated generated geometry.
	// Call RequestSynthMesh() to start a generation; keep the instances alive through the wait.
	bool WaitForSynthMeshes(const std::vector<swPiece*>& Pieces);

	// Returns the last generation error; an empty error does not imply readiness.
	QString GetSynthMeshError(const swPiece* Piece) const;

	// Returns a borrowed library snapshot in name order. Caller prevents identity teardown.
	std::vector<swPieceData*> GetParts() const;

	// Matches library-part descriptions against category keywords.
	bool PieceDataInCategory(swPieceData* PieceData, const char* CategoryKeywords) const;

	// Fills borrowed category entries, optionally grouping patterned pieces under their parent.
	void GetCategoryEntries(int CategoryIndex, bool GroupPieces, std::vector<swPieceData*>& SinglePieces, std::vector<swPieceData*>& GroupedPieces);
	void GetCategoryEntries(const char* CategoryKeywords, bool GroupPieces, std::vector<swPieceData*>& SinglePieces, std::vector<swPieceData*>& GroupedPieces);

	// Returns borrowed visible track parts compatible with the connection type.
	std::vector<swPieceData*> GetVisibleTrainTrackParts(const swTrainTrackConnectionType& ConnectionType) const;

	// Resolves normalized library IDs, preserving input order and duplicates; missing IDs are skipped.
	std::vector<swPieceData*> GetPartsFromSet(const std::vector<std::string>& PartIds) const;

	// Returns the library index key, or an empty string for an unregistered/local identity.
	std::string GetPartId(const swPieceData* PieceData) const;

	// Queries primitive sources; FindPrimitive() returns a borrowed entry.
	swLibraryPrimitive* FindPrimitive(const char* Name) const;

	// Loads shared primitive data, waiting for another worker when necessary.
	swResult<void> LoadPrimitive(swLibraryPrimitive* Primitive);

	// Opens an owned include on the reading thread, positioned at zero, or returns an error.
	// Piece lookup preserves official-first search order; callers decide whether missing includes are fatal.
	swResult<std::unique_ptr<swFile>> OpenPrimitiveFile(const swLibraryPrimitive* Primitive);
	swResult<std::unique_ptr<swFile>> OpenPieceDataFile(const char* PieceName);

	// Worker builds poll this shutdown flag without consulting live UI state.
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

	// Rebuilds shared mesh buffers when dirty, using the supplied render context.
	void UpdateBuffers(swContext* Context);

	// Returns borrowed read-only references to the shared vertex and index buffers.
	std::pair<const swVertexBuffer&, const swIndexBuffer&> GetBuffers() const
	{
		return { mVertexBuffer, mIndexBuffer };
	}

	// Returns the borrowed placeholder mesh for pending or failed piece geometry.
	swMesh* GetLoadingMesh() const;

signals:
	void AssetStatusChanged();
	void PartLoaded(swPieceData* PieceData);
	void PartLoadFailed(swPieceData* PieceData, const QString& Error);
	void GeneratedMeshSettled(swPieceData* PieceData);
	void ColorsLoaded();

protected:
	friend class swAssetLoader;
	template<typename T> friend class swAssetReference;

	// Drains the loader and clears pieces before sources, textures and archives.
	// Consumers must release references before teardown; the library remains alive during callbacks.
	void Unload();

	// Internal retain/release primitives for scoped references. Retaining never starts work.
	// Piece release also lets the loader cancel unused queued work.
	void AddPieceDataReference(swPieceData* PieceData);
	void ReleasePieceData(swPieceData* PieceData);
	void AddTextureReference(swTexture* Texture);
	void ReleaseTexture(swTexture* Texture);

	// Snapshots live piece/model sources on the UI thread for a private worker build.
	swPieceSourceSnapshot SnapshotPieceDataSource(const swPieceData* PieceData) const;

	// Returns a converted private mesh or an error without publishing asset state.
	swPieceBuildResult BuildPieceData(const swPieceSourceSnapshot& Source);

	// Saves geometry using the settings and cache location captured for the build.
	void SaveBuiltPieceDataCache(const swPieceSourceSnapshot& Source, swMesh& Mesh);

	// Returns private decoded pixels/errors without GL upload or publication.
	swTextureBuildResult BuildTextureData(const swFileSource& Source);

	// The loader calls this only when cancelling the current request, before releasing its hold.
	void PieceDataLoadCancelled(swPieceData* PieceData);

	// Publishes current results on the UI thread; the loader rejects stale requests first.
	// A successful piece result may have no mesh when its geometry is empty.
	bool PublishPieceDataMesh(swPieceData* PieceData, std::unique_ptr<swMesh> Mesh, QString Error);
	bool PublishSynthMesh(swPiece* Piece, std::unique_ptr<swMesh> Mesh, QString Error);

	// Applies decoded pixels or a decode failure; upload scheduling and retries stay in the loader.
	void PublishTexture(swTexture* Texture, swTextureBuildResult Result);

	// Opens a resolved source on the reading thread, positioned at zero.
	// Caller owns the file; archive replacement requires draining workers first.
	swResult<std::unique_ptr<swFile>> OpenFile(const swFileSource& Source);

	// Loads an external project, rejecting recursive include cycles.
	swResult<Project*> LoadExternalProject(const QFileInfo& FileInfo, bool Preview, bool DeferModelMeshRequests);

	// Discovers source entries before workers consume their metadata.
	bool OpenArchive(const QString& FileName, swZipFileType ZipFileType);
	bool OpenArchive(std::unique_ptr<swFile> File, swZipFileType ZipFileType);
	bool OpenDirectory(const QDir& LibraryDir, bool ShowProgress);

	// Initializes descriptions from source files and compatible description caches.
	void ReadArchiveDescriptions(const QString& OfficialFileName, const QString& UnofficialFileName);
	void ReadDirectoryDescriptions(const QFileInfoList (&FileLists)[static_cast<int>(swLibraryFolderType::Count)], bool ShowProgress);

	enum class CacheType : quint32
	{
		Archive = 0x0001,
		Directory = 0x0002
	};

	// Reads/writes Qt-compressed caches with version, size and payload checksum validation.
	// Archive caches also validate source archive sizes and modification times.
	bool ReadCacheFile(const QString& FileName, swMemoryFile& CacheFile, CacheType Type);
	bool WriteCacheFile(const QString& FileName, const swMemoryFile& CacheFile, CacheType Type);

	// Restores/saves descriptions and patterned parent IDs in library index order.
	bool LoadCacheIndex(const QString& FileName);
	bool SaveArchiveCacheIndex(const QString& FileName);

	// Builds primitive data after dependency tracking selects this worker as its loader.
	swResult<void> LoadPrimitiveData(swLibraryPrimitive* Primitive);

	static bool IsStudPrimitive(const char* FileName);
	static bool IsStudStylePrimitive(const char* FileName);
	static qint32 MeshCacheSettingsKey(swStudStyle StudStyle, bool StudCylinderColorEnabled);

	// Updates placeholder colors and the active stud-style source after settings change.
	void UpdateLoadingMeshColors();
	void UpdateStudStyleSource();

	// Resolves embedded, project and library texture sources on the UI thread.
	static QString FindProjectTextureFile(const QString& ProjectPath, const QString& TextureName);
	swFileSource ResolveTextureSource(const char* TextureName, const std::vector<QString>& SearchDirectories, const std::map<QString, std::shared_ptr<const swEmbeddedData>>& EmbeddedData, QString& ProjectPath);

	// Defers repacking until work settles, then marks buffers dirty and refreshes views.
	void ScheduleBufferRepack();

	void ReleaseBuffers();

	using DirectoryFilePaths = std::unordered_map<const swPieceData*, QString>;

	// Classifies descriptions, decorations and alias parents after all parts are registered.
	// Directory paths are temporary discovery metadata; archive entries use their stored indices.
	void ClassifyPieces(const DirectoryFilePaths& FilePaths);
	static std::string ReadAliasTarget(swFile& File);

	// Description cache strings are bounded to the piece name/description length limit.
	static bool ReadIndexString(swMemoryFile& File, std::string& String);
	static bool WriteIndexString(swMemoryFile& File, const std::string& String);

	// Library directory, archive readers and primitive sources; workers drain before replacement or teardown.
	QDir mLibraryDir;
	std::unique_ptr<swZipFile> mZipFiles[static_cast<int>(swZipFileType::Count)];
	std::vector<std::unique_ptr<swLibrarySource>> mSources;
	bool mHasUnofficial;

	// Captured before unofficial additions/overrides; custom libraries skip update checks.
	int mOfficialPieceCount = 0;

	// Settings captured by worker source snapshots and mesh cache keys.
	swStudStyle mStudStyle;
	bool mStudCylinderColorEnabled;

	// Identity managers and asynchronous services. Unload() clears them in dependency order.
	std::unique_ptr<swPieceManager> mPieceManager;
	std::unique_ptr<swTextureManager> mTextureManager;
	std::unique_ptr<swAssetLoader> mAssetLoader;
	std::unique_ptr<swThumbnailManager> mThumbnailManager;

	// UI-thread live synth instances and their last generation errors (empty on success or retry).
	// Instances unregister before destruction; the library does not own them.
	std::unordered_map<swPiece*, QString> mSynthPieces;

	// Active dependency edges include both nested loads and waits on other workers.
	QMutex mPrimitiveMutex;
	QWaitCondition mPrimitiveLoaded;
	std::unordered_map<QThread*, std::vector<swLibraryPrimitive*>> mPrimitiveLoadStacks;
	std::unordered_map<swLibraryPrimitive*, swLibraryPrimitive*> mPrimitiveDependencies;

	// Normalized texture names mapped to disk paths or archive entry indices.
	std::map<QString, QString> mTextureDiskEntries;
	std::map<QString, int> mTextureArchiveEntries[static_cast<int>(swZipFileType::Count)];

	// Cache location and official/unofficial archive size/timestamp validation data.
	QString mCachePath;
	qint64 mArchiveCheckSum[4];

	// Shared GPU buffers, placeholder geometry and deferred repacking state.
	swVertexBuffer mVertexBuffer;
	swIndexBuffer mIndexBuffer;
	bool mBuffersDirty;
	std::unique_ptr<swMesh> mLoadingMesh;
	bool mStreamingBuffersPending = false;
	bool mBufferRepackScheduled = false;

	// Shutdown flag polled by workers while the UI thread drains outstanding builds.
	std::atomic_bool mCancelLoading = false;
};
