#pragma once

#include "sw_types_asset.h"

// Creates and reuses textures.
class swTextureManager
{
public:
	swTextureManager() = default;
	~swTextureManager();

	swTextureManager(const swTextureManager&) = delete;
	swTextureManager(swTextureManager&&) = delete;
	swTextureManager& operator=(const swTextureManager&) = delete;
	swTextureManager& operator=(swTextureManager&&) = delete;

	// Deletes all textures.
	void Clear();

	// Returns or creates a texture.
	swTexture* AcquireTexture(const char* Name, const swFileSource& Source, const QString& ProjectPath);

	// Detaches library textures.
	void RetireLibraryTextures(const QString& LibraryDirectory);

	// Keeps a texture alive for another owner.
	void AddTextureReference(swTexture* Texture);

	// Releases an owner reference.
	void ReleaseTexture(swTexture* Texture);

	// Decodes image pixels for upload.
	swTextureBuildResult BuildTextureData(swFile& File);

private:
	// Reads an image without advancing.
	swTextureBuildResult DecodeTextureData(swFile& File);

	QMutex mMutex;
	std::vector<swTexture*> mTextures;
};
