#pragma once

struct swStringCacheEntry
{
	int Left, Right, Top, Bottom;
};

class swStringCache
{
public:
    swStringCache();
	~swStringCache();

	void Initialize(swContext* Context);
	void Reset();

	void CacheStrings(swContext* Context, const QStringList& Strings);
	void GetStringDimensions(int* cx, int* cy, const QString& String) const;
	void DrawStrings(swContext* Context, const swMatrix44* Transforms, const QStringList& Strings) const;

protected:
	swTexture* mTexture;

	std::map<QString, swStringCacheEntry> mStrings;
};

extern swStringCache gStringCache;
