#pragma once

#include "sw_assettypes.h"

// Creates and reuses textures, tracks their references, and prepares image data.
// The asset loader handles scheduling, OpenGL uploads, and retries.
class swTextureManager
{
public:
	swTextureManager() = default;
	~swTextureManager();

	swTextureManager(const swTextureManager&) = delete;
	swTextureManager(swTextureManager&&) = delete;
	swTextureManager& operator=(const swTextureManager&) = delete;
	swTextureManager& operator=(swTextureManager&&) = delete;

	// Deletes all managed textures, regardless of reference count.
	// Stop workers and release mesh references before calling.
	void Clear();

	// Returns an existing texture for this source or creates one.
	// Call on the UI thread and release the returned reference when done.
	// Returns nullptr for an invalid name or missing source.
	swTexture* AcquireTexture(const char* Name, const swFileSource& Source, const QString& ProjectPath);

	// Keeps a texture alive for another owner.
	void AddTextureReference(swTexture* Texture);

	// Releases an owner's reference. Unloads unused textures and
	// deletes temporary textures when their last reference is released.
	void ReleaseTexture(swTexture* Texture);

	// Decodes and prepares image pixels for a later OpenGL upload.
	// Preserves the file position. Workers must use separate file instances.
	// Returns a DecodeFailed error if loading or preparation fails.
	swTextureBuildResult BuildTextureData(swFile& File);

private:
	// Reads an image from the current file position without advancing it.
	// Supports memory and disk files; returns DecodeFailed on failure.
	swTextureBuildResult DecodeTextureData(swFile& File);

	QMutex mMutex;
	std::vector<swTexture*> mTextures;
};
