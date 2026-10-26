#include "sw_global.h"
#include "sw_assettypes.h"
#include "sw_library.h"
#include "sw_application.h"
#include "sw_mesh.h"
#include "sw_piecedata.h"
#include "sw_texture.h"

template<>
void swAssetRef<swPieceData>::Retain(swPieceData* Asset, swPiecesLibrary* Library)
{
	Library->AddPieceDataReference(Asset);
}

template<>
void swAssetRef<swPieceData>::Release(swPieceData* Asset, swPiecesLibrary* Library)
{
	Library->ReleasePieceData(Asset);
}

template<>
void swAssetRef<swTexture>::Retain(swTexture* Asset, swPiecesLibrary* Library)
{
	Library->AddTextureReference(Asset);
}

template<>
void swAssetRef<swTexture>::Release(swTexture* Asset, swPiecesLibrary* Library)
{
	Library->ReleaseTexture(Asset);
}

template<typename T>
swAssetRef<T>::swAssetRef(T* Asset)
	: swAssetRef(Asset, Asset ? swGetPiecesLibrary() : nullptr)
{
}

template<typename T>
swAssetRef<T>::swAssetRef(T* Asset, swPiecesLibrary* Library)
	: mAsset(Asset), mLibrary(Library)
{
	if (mAsset)
	{
		Q_ASSERT(mLibrary && QThread::currentThread() == mLibrary->thread());
		Retain(mAsset, mLibrary);
	}
}

template<typename T>
swAssetRef<T>::swAssetRef(const swAssetRef& OtherRef)
	: swAssetRef(OtherRef.mAsset, OtherRef.mLibrary)
{
}

template<typename T>
swAssetRef<T>::swAssetRef(swAssetRef&& OtherRef) noexcept
	: mAsset(std::exchange(OtherRef.mAsset, nullptr)), mLibrary(std::exchange(OtherRef.mLibrary, nullptr))
{
}

template<typename T>
swAssetRef<T>::~swAssetRef()
{
	if (mAsset)
	{
		Q_ASSERT(mLibrary && QThread::currentThread() == mLibrary->thread());
		Release(mAsset, mLibrary);
	}
}

template<typename T>
swAssetRef<T>& swAssetRef<T>::operator=(const swAssetRef& OtherRef)
{
	if (this != &OtherRef)
	{
		swAssetRef CopyRef(OtherRef);
		Swap(CopyRef);
	}

	return *this;
}

template<typename T>
swAssetRef<T>& swAssetRef<T>::operator=(swAssetRef&& OtherRef) noexcept
{
	if (this != &OtherRef)
	{
		swAssetRef MovedRef(std::move(OtherRef));
		Swap(MovedRef);
	}

	return *this;
}

template<typename T>
bool swAssetRef<T>::HasOtherReferences() const
{
	Q_ASSERT(!mAsset || (mLibrary && QThread::currentThread() == mLibrary->thread()));

	return mAsset && mAsset->GetRefCount() > 1;
}

template<typename T>
void swAssetRef<T>::Reset()
{
	swAssetRef EmptyRef;
	Swap(EmptyRef);
}

template<typename T>
T* swAssetRef<T>::Release()
{
	mLibrary = nullptr;
	return std::exchange(mAsset, nullptr);
}

template<typename T>
void swAssetRef<T>::Swap(swAssetRef& OtherRef) noexcept
{
	std::swap(mAsset, OtherRef.mAsset);
	std::swap(mLibrary, OtherRef.mLibrary);
}

template<typename T>
swAssetRef<T> swAssetRef<T>::Adopt(T* Asset, swPiecesLibrary* Library)
{
	Q_ASSERT(!Asset || (Library && QThread::currentThread() == Library->thread()));

	swAssetRef AssetRef;
	AssetRef.mAsset = Asset;
	AssetRef.mLibrary = Library;
	return AssetRef;
}

template class swAssetRef<swPieceData>;
template class swAssetRef<swTexture>;

swPieceSourceSnapshot::swPieceSourceSnapshot()
	: ZipFileType(swZipFileType::Count), StudStyle(swStudStyle::Plain)
{
}

swPieceBuildResult::swPieceBuildResult() = default;
swPieceBuildResult::~swPieceBuildResult() = default;
swPieceBuildResult::swPieceBuildResult(swPieceBuildResult&&) noexcept = default;
swPieceBuildResult& swPieceBuildResult::operator=(swPieceBuildResult&&) noexcept = default;

swTextureBuildResult::~swTextureBuildResult() = default;
