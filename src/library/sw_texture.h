#pragma once

#define SW_TEXTURE_WRAP_U         0x01
#define SW_TEXTURE_WRAP_V         0x02
#define SW_TEXTURE_MIPMAPS       0x04
#define SW_TEXTURE_CUBE_MAP       0x08

#define SW_TEXTURE_POINT         0x00
#define SW_TEXTURE_LINEAR        0x10
#define SW_TEXTURE_BILINEAR      0x20
#define SW_TEXTURE_TRILINEAR     0x30
#define SW_TEXTURE_ANISOTROPIC   0x40
#define SW_TEXTURE_FILTER_MASK   0xf0
#define SW_TEXTURE_FILTER_SHIFT  4

#define SW_TEXTURE_NAME_LENGTH 256

#include "sw_asset_types.h"

class swTexture
{
public:
	swTexture(int Flags = 0);
	~swTexture();

	swTexture(const swTexture&) = delete;
	swTexture(swTexture&&) = delete;
	swTexture& operator=(const swTexture&) = delete;
	swTexture& operator=(swTexture&&) = delete;

	void CreateGridTexture();
	static QImage PrepareImage(QImage Image);

	void SetImage(QImage Image, int Flags);
	void SetImages(std::vector<QImage>&& Images, int Flags);
	void DiscardDecodedImage();
	// Records a failure without discarding pixels retained for upload retries.
	void SetFailed(swTextureLoadError Error, QString Details);
	// Failure details or a fallback message; empty unless loading has failed.
	QString GetLoadError() const;
	void Upload(swContext* Context);
	void Unload();

	bool HasImageData() const
	{
		return mTexture != 0 || !mImages.empty();
	}

	// Reference bookkeeping belongs to swTextureManager.
	void AddRef()
	{
		mRefCount.ref();
	}

	// Reference bookkeeping belongs to swTextureManager.
	bool Release()
	{
		const bool InUse = mRefCount.deref();

		if (!InUse)
			Unload();

		return InUse;
	}

	int GetRefCount() const
	{
		return mRefCount.loadRelaxed();
	}

	void SetTemporary(bool Temporary)
	{
		mTemporary = Temporary;
	}

	bool IsTemporary() const
	{
		return mTemporary;
	}

	bool NeedsUpload() const
	{
		return mTexture == 0 && !mImages.empty() && mState != swAssetState::Failed;
	}

	bool IsReady() const
	{
		return mState == swAssetState::Loaded && mTexture != 0;
	}

	int GetFlags() const
	{
		return mFlags;
	}

	const QImage& GetImage(int Index) const
	{
		return mImages[Index];
	}

	size_t GetImageCount() const
	{
		return mImages.size();
	}

	int mWidth;
	int mHeight;
	char mName[SW_TEXTURE_NAME_LENGTH] = {};
	swFileSource mSource;
	GLuint mTexture = 0;
	swAssetState mState = swAssetState::Unloaded;
	swTextureLoadError mLoadFailure = swTextureLoadError::None;
	QString mLoadFailureDetails;

protected:
	bool mTemporary = false;
	QAtomicInt mRefCount = 0;
	std::vector<QImage> mImages;
	int mFlags = 0;

	Q_DECLARE_TR_FUNCTIONS(swTexture)
};

extern swTexture* gGridTexture;
