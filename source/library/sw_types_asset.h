#pragma once

class swPartData;
class swTexture;
class swPartsLibrary;
class swMesh;
struct swEmbeddedData;
enum class swStudStyle;
enum class swZipFileType;

// Scoped asset reference. Construction from a borrowed pointer and copying retain;
// moving transfers ownership. Use and destroy on the UI thread before library unload.
// Ownership operations are instantiated for part definitions and textures in the cpp.
template<typename T>
class swAssetReference
{
public:
	swAssetReference() = default;
	explicit swAssetReference(T* Asset);
	swAssetReference(const swAssetReference& OtherRef);
	swAssetReference(swAssetReference&& OtherRef) noexcept;
	~swAssetReference();

	swAssetReference& operator=(const swAssetReference& OtherRef);
	swAssetReference& operator=(swAssetReference&& OtherRef) noexcept;

	T* Get() const
	{
		return mAsset;
	}

	T* operator->() const
	{
		return mAsset;
	}

	T& operator*() const
	{
		return *mAsset;
	}

	explicit operator bool() const
	{
		return mAsset != nullptr;
	}

	friend bool operator==(const swAssetReference& LeftRef, const swAssetReference& RightRef) noexcept
	{
		return LeftRef.mAsset == RightRef.mAsset;
	}

	friend bool operator!=(const swAssetReference& LeftRef, const swAssetReference& RightRef) noexcept
	{
		return !(LeftRef == RightRef);
	}

	friend bool operator<(const swAssetReference& LeftRef, const swAssetReference& RightRef) noexcept
	{
		return std::less<T*>()(LeftRef.mAsset, RightRef.mAsset);
	}

	friend bool operator==(const swAssetReference& LeftRef, const T* Right) noexcept
	{
		return LeftRef.mAsset == Right;
	}

	friend bool operator!=(const swAssetReference& LeftRef, const T* Right) noexcept
	{
		return !(LeftRef == Right);
	}

	friend bool operator==(const T* Left, const swAssetReference& RightRef) noexcept
	{
		return Left == RightRef.mAsset;
	}

	friend bool operator!=(const T* Left, const swAssetReference& RightRef) noexcept
	{
		return !(Left == RightRef);
	}

	// True when nonempty and another reference exists. Query on the UI thread.
	bool HasOtherReferences() const;

	void Reset();
	T* Release();
	void Swap(swAssetReference& OtherRef) noexcept;

private:
	friend class swPartsLibrary;

	swAssetReference(T* Asset, swPartsLibrary* Library);
	// The library uses this only for a reference it has already acquired.
	static swAssetReference Adopt(T* Asset, swPartsLibrary* Library);
	static void Retain(T* Asset, swPartsLibrary* Library);
	static void Release(T* Asset, swPartsLibrary* Library);

	T* mAsset = nullptr;
	swPartsLibrary* mLibrary = nullptr;
};

using swPartDataReference = swAssetReference<swPartData>;
using swTextureReference = swAssetReference<swTexture>;

// Asset readiness; detailed scheduling progress belongs to loader requests.
enum class swAssetState
{
	Unloaded,
	Loading,
	Loaded,
	Failed
};

// Scheduling urgency only. Use the library's wait APIs for synchronous loading.
// Queued requests only promote and keep their priority until completion or cancellation.
enum class swLoadPriority
{
	Low,      // Background work, such as thumbnails and settings rebuilds.
	Normal,   // Visible or interactive work.
	Blocking  // Assets required by a synchronous operation.
};

// A resolved disk path, archive entry, or embedded file. Copying it does not
// snapshot file contents; workers open their own file from the copied source.
struct swFileSource
{
	bool operator==(const swFileSource& Other) const
	{
		return EmbeddedData == Other.EmbeddedData && FilePath == Other.FilePath && ArchiveType == Other.ArchiveType && ArchiveIndex == Other.ArchiveIndex;
	}

	bool operator!=(const swFileSource& Other) const
	{
		return !(*this == Other);
	}

	std::shared_ptr<const swEmbeddedData> EmbeddedData;
	QString FilePath;
	int ArchiveType = -1;
	int ArchiveIndex = -1;
};

enum class swTextureLoadError
{
	None,
	DecodeFailed,
	ContextActivationFailed,
	ContextUnavailable,
	UploadFailed
};

// Copied on the UI thread so workers do not consult live models or project lookup.
struct swPartSourceSnapshot
{
	swPartSourceSnapshot();

	QString FileName;
	QString LibraryDirectory;
	QString CachePath;
	std::vector<QString> TextureSearchDirectories;
	std::map<QString, std::shared_ptr<const swEmbeddedData>> EmbeddedData;
	QStringList InlineMeshLines;
	bool InlineModel = false;
	swZipFileType ZipFileType;
	int ZipFileIndex = -1;
	swStudStyle StudStyle;
	bool StudCylinderColorEnabled = false;
};

// Private worker output; meshes and dependencies are published only on the UI thread.
struct swPartBuildResult
{
	swPartBuildResult();
	~swPartBuildResult();

	swPartBuildResult(swPartBuildResult&&) noexcept;
	swPartBuildResult& operator=(swPartBuildResult&&) noexcept;
	swPartBuildResult(const swPartBuildResult&) = delete;
	swPartBuildResult& operator=(const swPartBuildResult&) = delete;

	std::unique_ptr<swMesh> Mesh;
	bool FromCache = false;
	bool EmptyGeometry = false;
	QString Error;
};

// Image pixels and any error produced while preparing a texture.
struct swTextureBuildResult
{
	swTextureBuildResult() = default;
	~swTextureBuildResult();

	swTextureBuildResult(swTextureBuildResult&&) = default;
	swTextureBuildResult& operator=(swTextureBuildResult&&) = default;
	swTextureBuildResult(const swTextureBuildResult&) = delete;
	swTextureBuildResult& operator=(const swTextureBuildResult&) = delete;

	QImage DecodedImage;
	swTextureLoadError Error = swTextureLoadError::None;
	QString ErrorDetails;
};
