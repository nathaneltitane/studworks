#pragma once

class swTextureFont
{
public:
	swTextureFont();

	bool IsLoaded() const
	{
		return mTexture != nullptr;
	}

	swTexture* GetTexture() const
	{
		return mTexture;
	}

	bool Initialize(swContext* Context);
	void Reset();

	void PrintText(swContext* Context, float Left, float Top, float Z, const char* Text) const;
	void GetTriangles(const swMatrix44& Transform, const char* Text, float* Buffer) const;
	void GetGlyphTriangles(float Left, float Top, float Z, int Glyph, float* Buffer) const;
	void GetStringDimensions(int* cx, int* cy, const char* Text) const;

protected:
	struct
	{
		float Width, Offset, RenderWidth, RenderHeight;
		float Left, Right, Top, Bottom;
	} mGlyphs[256];

	swTexture* mTexture = nullptr;
	int mTextureWidth;
	int mTextureHeight;
	int mFontHeight;
};

extern swTextureFont gTexFont;
