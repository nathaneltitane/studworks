#include "sw_global.h"
#include "sw_texture_manager.h"
#include "sw_file.h"
#include "sw_string.h"
#include "sw_texture.h"

swTextureManager::~swTextureManager()
{
	Clear();
}

void swTextureManager::Clear()
{
	QMutexLocker Lock(&mMutex);

	for (swTexture* Texture : mTextures)
		delete Texture;

	mTextures.clear();
}

void swTextureManager::AddTextureReference(swTexture* Texture)
{
	QMutexLocker Lock(&mMutex);

	Texture->AddRef();
}

swTexture* swTextureManager::AcquireTexture(const char* TextureName, const swFileSource& Source, const QString& ProjectPath)
{
	if (!TextureName[0] || strlen(TextureName) >= SW_TEXTURE_NAME_LENGTH)
		return nullptr;

	if (!Source.EmbeddedData && Source.FilePath.isEmpty() && Source.ArchiveIndex == -1)
		return nullptr;

	QMutexLocker Lock(&mMutex);

	for (swTexture* Texture : mTextures)
	{
		if (Texture->mSource != Source)
			continue;

		Texture->AddRef();

		return Texture;
	}

	swTexture* Texture = new swTexture(SW_TEXTURE_MIPMAPS);

	swstrcpy(Texture->mName, TextureName);
	Texture->mSource = Source;
	Texture->SetTemporary(bool(Source.EmbeddedData) || !ProjectPath.isEmpty());
	Texture->AddRef();

	mTextures.push_back(Texture);

	return Texture;
}

swTextureBuildResult swTextureManager::BuildTextureData(swFile& File)
{
	swTextureBuildResult Result = DecodeTextureData(File);

	if (!Result.DecodedImage.isNull())
	{
		Result.DecodedImage = swTexture::PrepareImage(std::move(Result.DecodedImage));

		if (Result.DecodedImage.isNull())
			Result.Error = swTextureLoadError::DecodeFailed;
	}

	return Result;
}

swTextureBuildResult swTextureManager::DecodeTextureData(swFile& File)
{
	swTextureBuildResult Result;

	bool Loaded = false;

	if (swMemoryFile* MemoryFile = dynamic_cast<swMemoryFile*>(&File))
	{
		if (MemoryFile->mBuffer && MemoryFile->mPosition < MemoryFile->mFileSize)
		{
			const size_t Length = MemoryFile->mFileSize - MemoryFile->mPosition;

			if (Length <= static_cast<size_t>(std::numeric_limits<int>::max()))
				Loaded = Result.DecodedImage.loadFromData(MemoryFile->mBuffer + MemoryFile->mPosition, static_cast<int>(Length));
		}
	}
	else if (swDiskFile* DiskFile = dynamic_cast<swDiskFile*>(&File))
	{
		QFile& Device = DiskFile->GetFile();
		const qint64 Position = Device.pos();

		if (Position >= 0)
		{
			Loaded = Result.DecodedImage.load(&Device, nullptr);
			const bool Restored = Device.seek(Position);
			Loaded = Loaded && Restored;
		}
	}

	if (!Loaded || Result.DecodedImage.isNull())
	{
		Result.DecodedImage = QImage();
		Result.Error = swTextureLoadError::DecodeFailed;
	}

	return Result;
}

void swTextureManager::ReleaseTexture(swTexture* Texture)
{
	QMutexLocker Lock(&mMutex);

	if (Texture->Release() == 0 && Texture->IsTemporary())
	{
		std::vector<swTexture*>::iterator TextureIt = std::find(mTextures.begin(), mTextures.end(), Texture);
		if (TextureIt != mTextures.end())
			mTextures.erase(TextureIt);
		delete Texture;
	}
}
