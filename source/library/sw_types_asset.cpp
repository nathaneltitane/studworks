#include "sw_global.h"
#include "sw_types_asset.h"
#include "sw_library.h"
#include "sw_application.h"
#include "sw_mesh.h"
#include "sw_data_piece.h"
#include "sw_texture.h"

template<>
void swAssetReference<swPieceData>::Retain(swPieceData* Asset, swPiecesLibrary* Library)
{
	Library->AddPieceDataReference(Asset);
}

template<>
void swAssetReference<swPieceData>::Release(swPieceData* Asset, swPiecesLibrary* Library)
{
	Library->ReleasePieceData(Asset);
}

template<>
void swAssetReference<swTexture>::Retain(swTexture* Asset, swPiecesLibrary* Library)
{
	Library->AddTextureReference(Asset);
}

template<>
void swAssetReference<swTexture>::Release(swTexture* Asset, swPiecesLibrary* Library)
{
	Library->ReleaseTexture(Asset);
}

template<typename T>
swAssetReference<T>::swAssetReference(T* Asset)
	: swAssetReference(Asset, Asset ? swGetPiecesLibrary() : nullptr)
{
}

template<typename T>
swAssetReference<T>::swAssetReference(T* Asset, swPiecesLibrary* Library)
	: mAsset(Asset), mLibrary(Library)
{
	if (mAsset)
	{
		Q_ASSERT(mLibrary && QThread::currentThread() == mLibrary->thread());
		Retain(mAsset, mLibrary);
	}
}

template<typename T>
swAssetReference<T>::swAssetReference(const swAssetReference& OtherRef)
	: swAssetReference(OtherRef.mAsset, OtherRef.mLibrary)
{
}

template<typename T>
swAssetReference<T>::swAssetReference(swAssetReference&& OtherRef) noexcept
	: mAsset(std::exchange(OtherRef.mAsset, nullptr)), mLibrary(std::exchange(OtherRef.mLibrary, nullptr))
{
}

template<typename T>
swAssetReference<T>::~swAssetReference()
{
	if (mAsset)
	{
		Q_ASSERT(mLibrary && QThread::currentThread() == mLibrary->thread());
		Release(mAsset, mLibrary);
	}
}

template<typename T>
swAssetReference<T>& swAssetReference<T>::operator=(const swAssetReference& OtherRef)
{
	if (this != &OtherRef)
	{
		swAssetReference CopyRef(OtherRef);
		Swap(CopyRef);
	}

	return *this;
}

template<typename T>
swAssetReference<T>& swAssetReference<T>::operator=(swAssetReference&& OtherRef) noexcept
{
	if (this != &OtherRef)
	{
		swAssetReference MovedRef(std::move(OtherRef));
		Swap(MovedRef);
	}

	return *this;
}

template<typename T>
bool swAssetReference<T>::HasOtherReferences() const
{
	Q_ASSERT(!mAsset || (mLibrary && QThread::currentThread() == mLibrary->thread()));

	return mAsset && mAsset->GetRefCount() > 1;
}

template<typename T>
void swAssetReference<T>::Reset()
{
	swAssetReference EmptyRef;
	Swap(EmptyRef);
}

template<typename T>
T* swAssetReference<T>::Release()
{
	mLibrary = nullptr;
	return std::exchange(mAsset, nullptr);
}

template<typename T>
void swAssetReference<T>::Swap(swAssetReference& OtherRef) noexcept
{
	std::swap(mAsset, OtherRef.mAsset);
	std::swap(mLibrary, OtherRef.mLibrary);
}

template<typename T>
swAssetReference<T> swAssetReference<T>::Adopt(T* Asset, swPiecesLibrary* Library)
{
	Q_ASSERT(!Asset || (Library && QThread::currentThread() == Library->thread()));

	swAssetReference AssetRef;
	AssetRef.mAsset = Asset;
	AssetRef.mLibrary = Library;
	return AssetRef;
}

template class swAssetReference<swPieceData>;
template class swAssetReference<swTexture>;

swPieceSourceSnapshot::swPieceSourceSnapshot()
	: ZipFileType(swZipFileType::Count), StudStyle(swStudStyle::Plain)
{
}

swPieceBuildResult::swPieceBuildResult() = default;
swPieceBuildResult::~swPieceBuildResult() = default;
swPieceBuildResult::swPieceBuildResult(swPieceBuildResult&&) noexcept = default;
swPieceBuildResult& swPieceBuildResult::operator=(swPieceBuildResult&&) noexcept = default;

swTextureBuildResult::~swTextureBuildResult() = default;
