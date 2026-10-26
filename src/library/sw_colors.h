#pragma once

#include "sw_math.h"

#define SW_MAXIMUM_COLOR_NAME 64
#define SW_COLOR_DIRECT 0x80000000
#define SW_COLOR_NO_COLOR 0xffffffff
#define SW_STUD_CYLINDER_COLOR_CODE 4242

struct swColor
{
	quint32 Code;
	int Group;
	bool Translucent = false;
	bool Chrome = false;
	bool Rubber = false;
	bool Adjusted = false;
	swVector4 Value;
	swVector4 Edge;
	char Name[SW_MAXIMUM_COLOR_NAME];
	char SafeName[SW_MAXIMUM_COLOR_NAME];
};

enum
{
	SW_COLOR_GROUP_SOLID,
	SW_COLOR_GROUP_TRANSLUCENT,
	SW_COLOR_GROUP_SPECIAL,
	SW_COLOR_GROUP_COUNT
};

struct swColorGroup
{
	std::vector<int> Colors;
	QString Name;
};

extern std::vector<swColor> gColorList;
extern swColorGroup gColorGroups[SW_COLOR_GROUP_COUNT];
extern int gEdgeColor;
extern int gDefaultColor;

void swLoadDefaultColors(swStudStyle StudStyle);
bool swLoadColorFile(swFile& File, swStudStyle StudStyle);
int swGetColorIndex(quint32 ColorCode);

inline quint32 swGetColorCodeFromExtendedColor(int Color)
{
	const quint32 ConversionTable[] = { 4, 12, 2, 10, 1, 9, 14, 15, 8, 0, 6, 13, 13, 334, 36, 44, 34, 42, 33, 41, 46, 47, 7, 382, 6, 13, 11, 383 };
	return ConversionTable[Color];
}

inline quint32 swGetColorCodeFromOriginalColor(int Color)
{
	const quint32 ConversionTable[] = { 0, 2, 4, 9, 7, 6, 22, 8, 10, 11, 14, 16, 18, 9, 21, 20, 22, 8, 10, 11 };
	return swGetColorCodeFromExtendedColor(ConversionTable[Color]);
}

inline quint32 swGetColorCode(int ColorIndex)
{
	return gColorList[ColorIndex].Code;
}

inline bool swIsColorTranslucent(size_t ColorIndex)
{
	return gColorList[ColorIndex].Translucent;
}

inline bool swIsColorChrome(size_t ColorIndex)
{
	return gColorList[ColorIndex].Chrome;
}

inline bool swIsColorRubber(size_t ColorIndex)
{
	return gColorList[ColorIndex].Rubber;
}

QString swGetColorToolTip(int ColorIndex);
void swDrawNoColorRectangle(QPainter& Painter, const QRect& Rect);
