#include "sw_global.h"
#include "sw_colors.h"
#include "sw_file.h"
#include "sw_library.h"
#include "sw_application.h"
#include "sw_string.h"
#include <float.h>

std::vector<swColor> gColorList;
swColorGroup gColorGroups[SW_COLOR_GROUP_COUNT];
int gEdgeColor;
int gDefaultColor;

static void GetToken(char*& Ptr, char* Token)
{
	while (*Ptr && *Ptr <= 32)
		Ptr++;

	while (*Ptr > 32)
		*Token++ = *Ptr++;

	*Token = 0;
}

static void swAdjustStudStyleColors(std::vector<swColor>& Colors, swStudStyle StudStyle)
{
	const swPreferences& Preferences = swGetPreferences();

	if (!Preferences.mAutomateEdgeColor && !swIsHighContrast(StudStyle))
		return;

	const swVector4 Edge = swVector4FromColor(Preferences.mPartEdgeColor);
	const swVector4 DarkEdge = swVector4FromColor(Preferences.mDarkEdgeColor);
	const swVector4 BlackEdge = swVector4FromColor(Preferences.mBlackEdgeColor);

	const float ContrastControl = Preferences.mPartEdgeContrast;
	const float LightDarkControl = Preferences.mAutomateEdgeColor ? Preferences.mPartColorValueLDIndex : SW_SRGB_TO_LINEAR(Preferences.mPartColorValueLDIndex);

	for (swColor& Color : Colors)
	{
		const swVector3 LinearColor = swSRGBToLinear(swVector3(Color.Value));
		const float ValueLuminescence = swLuminescence(LinearColor);

		if (Preferences.mAutomateEdgeColor)
		{
			if (Color.Adjusted)
				continue;

			const float EdgeLuminescence = swLuminescence(swSRGBToLinear(swVector3(Color.Edge)));

			Color.Edge = swAlgorithmicEdgeColor(LinearColor, ValueLuminescence, EdgeLuminescence, ContrastControl, LightDarkControl);
			Color.Adjusted = true;
		}
		else
		{
			if (Color.Code == SW_STUD_CYLINDER_COLOR_CODE)
				continue;
			else if (Preferences.mBlackEdgeColorEnabled && Color.Code == 0)
				Color.Edge = BlackEdge;
			else if (Preferences.mDarkEdgeColorEnabled && ValueLuminescence < LightDarkControl)
				Color.Edge = DarkEdge;
			else if (Preferences.mPartEdgeColorEnabled)
				Color.Edge = Edge;
		}
	}
}

static std::vector<swColor> swParseColorFile(swFile& File)
{
	char Line[1024], Token[1024];
	std::vector<swColor> Colors;
	swColor Color;

	while (File.ReadLine(Line, sizeof(Line)))
	{
		char* Ptr = Line;

		GetToken(Ptr, Token);
		if (strcmp(Token, "0"))
			continue;

		GetToken(Ptr, Token);
		swstrupr(Token);
		if (strcmp(Token, "!COLOUR"))
			continue;

		Color.Code = ~0U;
		Color.Translucent = false;
		Color.Chrome = false;
		Color.Rubber = false;
		Color.Finish = swColorFinish::Plain;
		Color.Luminance = 0;
		Color.MaterialFraction = 0.0f;
		Color.MaterialSize = 0.0f;
		Color.MaterialValue = swVector3(0.0f, 0.0f, 0.0f);
		Color.Group = SW_COLOR_GROUP_SOLID;
		Color.Value[0] = FLT_MAX;
		Color.Value[1] = FLT_MAX;
		Color.Value[2] = FLT_MAX;
		Color.Value[3] = 1.0f;
		Color.Edge[0] = FLT_MAX;
		Color.Edge[1] = FLT_MAX;
		Color.Edge[2] = FLT_MAX;
		Color.Edge[3] = 1.0f;

		GetToken(Ptr, Token);
		swstrcpy(Color.Name, Token);
		swstrcpy(Color.SafeName, Color.Name);

		for (char* Underscore = strchr((char*)Color.Name, '_'); Underscore; Underscore = strchr(Underscore, '_'))
			*Underscore = ' ';

		for (GetToken(Ptr, Token); Token[0]; GetToken(Ptr, Token))
		{
			swstrupr(Token);

			if (!strcmp(Token, "CODE"))
			{
				GetToken(Ptr, Token);
				Color.Code = atoi(Token);
			}
			else if (!strcmp(Token, "VALUE"))
			{
				GetToken(Ptr, Token);
				if (Token[0] == '#')
					Token[0] = ' ';

				int Value;
				if (sscanf(Token, "%x", &Value) != 1)
					Value = 0;

				Color.Value[2] = (float)(Value & 0xff) / 255.0f;
				Value >>= 8;
				Color.Value[1] = (float)(Value & 0xff) / 255.0f;
				Value >>= 8;
				Color.Value[0] = (float)(Value & 0xff) / 255.0f;
			}
			else if (!strcmp(Token, "EDGE"))
			{
				GetToken(Ptr, Token);
				if (Token[0] == '#')
					Token[0] = ' ';

				int Value;
				if (sscanf(Token, "%x", &Value) != 1)
					Value = 0;

				Color.Edge[2] = (float)(Value & 0xff) / 255.0f;
				Value >>= 8;
				Color.Edge[1] = (float)(Value & 0xff) / 255.0f;
				Value >>= 8;
				Color.Edge[0] = (float)(Value & 0xff) / 255.0f;
			}
			else if (!strcmp(Token, "ALPHA"))
			{
				GetToken(Ptr, Token);
				int Value = atoi(Token);
				Color.Value[3] = (float)(Value & 0xff) / 255.0f;
				if (Value != 255)
					Color.Translucent = true;

				if (Value == 128)
					Color.Group = SW_COLOR_GROUP_TRANSLUCENT;
				else if (Value != 0)
					Color.Group = SW_COLOR_GROUP_SPECIAL;
			}
			else if (!strcmp(Token, "CHROME"))
			{
				Color.Chrome = true;
				Color.Group = SW_COLOR_GROUP_SPECIAL;
			}
			else if (!strcmp(Token, "RUBBER"))
			{
				Color.Rubber = true;
				Color.Group = SW_COLOR_GROUP_SPECIAL;
			}
			else if (!strcmp(Token, "PEARLESCENT"))
			{
				Color.Finish = swColorFinish::Pearl;
				Color.Group = SW_COLOR_GROUP_SPECIAL;
			}
			else if (!strcmp(Token, "MATTE_METALLIC") || !strcmp(Token, "MATTE_METALIC") || !strcmp(Token, "METAL"))
			{
				Color.Finish = swColorFinish::Metal;
				Color.Group = SW_COLOR_GROUP_SPECIAL;
			}
			else if (!strcmp(Token, "LUMINANCE"))
			{
				GetToken(Ptr, Token);
				Color.Luminance = atoi(Token);
				Color.Group = SW_COLOR_GROUP_SPECIAL;
			}
			else if (!strcmp(Token, "MATERIAL"))
			{
				Color.Group = SW_COLOR_GROUP_SPECIAL;

				// material is always last
				GetToken(Ptr, Token);
				swstrupr(Token);

				if (!strcmp(Token, "GLITTER"))
					Color.Finish = swColorFinish::Glitter;
				else if (!strcmp(Token, "SPECKLE"))
					Color.Finish = swColorFinish::Speckle;
				else
					break;

				Color.MaterialValue = swVector3(Color.Value[0], Color.Value[1], Color.Value[2]);

				for (GetToken(Ptr, Token); Token[0]; GetToken(Ptr, Token))
				{
					swstrupr(Token);

					if (!strcmp(Token, "VALUE"))
					{
						GetToken(Ptr, Token);
						if (Token[0] == '#')
							Token[0] = ' ';

						int Value;
						if (sscanf(Token, "%x", &Value) != 1)
							Value = 0;

						Color.MaterialValue = swVector3((float)((Value >> 16) & 0xff) / 255.0f, (float)((Value >> 8) & 0xff) / 255.0f, (float)(Value & 0xff) / 255.0f);
					}
					else if (!strcmp(Token, "FRACTION"))
					{
						GetToken(Ptr, Token);
						Color.MaterialFraction = (float)atof(Token);
					}
					else if (!strcmp(Token, "SIZE") || !strcmp(Token, "MAXSIZE"))
					{
						GetToken(Ptr, Token);
						Color.MaterialSize = qMax(Color.MaterialSize, (float)atof(Token));
					}
				}

				break;
			}
		}

		if (Color.Code == ~0U || Color.Value[0] == FLT_MAX)
			continue;

		if (Color.Edge[0] == FLT_MAX)
		{
			Color.Edge[0] = 33.0f / 255.0f;
			Color.Edge[1] = 33.0f / 255.0f;
			Color.Edge[2] = 33.0f / 255.0f;
		}

		bool Duplicate = false;

		for (swColor& ExistingColor : Colors)
		{
			if (ExistingColor.Code == Color.Code)
			{
				ExistingColor = Color;
				Duplicate = true;
				break;
			}
		}

		if (!Duplicate)
			Colors.push_back(Color);
	}

	return Colors;
}

bool swLoadColorFile(swFile& File, swStudStyle StudStyle)
{
	std::vector<swColor> Colors = swParseColorFile(File);
	const bool Valid = !Colors.empty();

	if (Valid)
		swAdjustStudStyleColors(Colors, StudStyle);

	bool FoundMain = false, FoundEdge = false, FoundStudCylinder = false, FoundNoColor = false;

	for (const swColor& Color : Colors)
	{
		switch (Color.Code)
		{
			case 16:
				FoundMain = true;
				break;

			case 24:
				FoundEdge = true;
				break;

			case SW_STUD_CYLINDER_COLOR_CODE:
				FoundStudCylinder = true;
				break;

			case SW_COLOR_NO_COLOR:
				FoundNoColor = true;
				break;
		}
	}

	if (!FoundMain)
	{
		swColor MainColor;

		MainColor.Code = 16;
		MainColor.Translucent = false;
		MainColor.Chrome = false;
		MainColor.Rubber = false;
		MainColor.Group = SW_COLOR_GROUP_SOLID;
		MainColor.Value[0] = 1.0f;
		MainColor.Value[1] = 1.0f;
		MainColor.Value[2] = 0.5f;
		MainColor.Value[3] = 1.0f;
		MainColor.Edge[0] = 0.2f;
		MainColor.Edge[1] = 0.2f;
		MainColor.Edge[2] = 0.2f;
		MainColor.Edge[3] = 1.0f;
		swstrcpy(MainColor.Name, "Main Color");
		swstrcpy(MainColor.SafeName, "Main_Color");

		Colors.push_back(MainColor);
	}

	if (!FoundEdge)
	{
		swColor EdgeColor;

		EdgeColor.Code = 24;
		EdgeColor.Translucent = false;
		EdgeColor.Chrome = false;
		EdgeColor.Rubber = false;
		EdgeColor.Group = SW_COLOR_GROUP_COUNT;
		EdgeColor.Value[0] = 0.5f;
		EdgeColor.Value[1] = 0.5f;
		EdgeColor.Value[2] = 0.5f;
		EdgeColor.Value[3] = 1.0f;
		EdgeColor.Edge[0] = 0.2f;
		EdgeColor.Edge[1] = 0.2f;
		EdgeColor.Edge[2] = 0.2f;
		EdgeColor.Edge[3] = 1.0f;
		swstrcpy(EdgeColor.Name, "Edge Color");
		swstrcpy(EdgeColor.SafeName, "Edge_Color");

		Colors.push_back(EdgeColor);
	}

	if (!FoundStudCylinder)
	{
		const swPreferences& Preferences = swGetPreferences();
		swColor StudCylinderColor;

		StudCylinderColor.Code = SW_STUD_CYLINDER_COLOR_CODE;
		StudCylinderColor.Translucent = false;
		StudCylinderColor.Chrome = false;
		StudCylinderColor.Rubber = false;
		StudCylinderColor.Group = SW_COLOR_GROUP_COUNT;
		StudCylinderColor.Value = swVector4FromColor(Preferences.mStudCylinderColor);
		StudCylinderColor.Edge = swVector4FromColor(Preferences.mPartEdgeColor);
		swstrcpy(StudCylinderColor.Name, "Stud Cylinder Color");
		swstrcpy(StudCylinderColor.SafeName, "Stud_Cylinder_Color");

		Colors.push_back(StudCylinderColor);
	}

	if (!FoundNoColor)
	{
		swColor NoColor;

		NoColor.Code = SW_COLOR_NO_COLOR;
		NoColor.Translucent = false;
		NoColor.Chrome = false;
		NoColor.Rubber = false;
		NoColor.Group = SW_COLOR_GROUP_COUNT;
		NoColor.Value[0] = 0.5f;
		NoColor.Value[1] = 0.5f;
		NoColor.Value[2] = 0.5f;
		NoColor.Value[3] = 1.0f;
		NoColor.Edge[0] = 0.2f;
		NoColor.Edge[1] = 0.2f;
		NoColor.Edge[2] = 0.2f;
		NoColor.Edge[3] = 1.0f;
		swstrcpy(NoColor.Name, "No Color");
		swstrcpy(NoColor.SafeName, "No_Color");

		Colors.push_back(NoColor);
	}

	for (swColor& Color : gColorList)
		Color.Group = SW_COLOR_GROUP_COUNT;

	for (int GroupIdx = 0; GroupIdx < SW_COLOR_GROUP_COUNT; GroupIdx++)
		gColorGroups[GroupIdx].Colors.clear();

	gColorGroups[0].Name = QApplication::tr("Solid", "Colors");
	gColorGroups[1].Name = QApplication::tr("Translucent", "Colors");
	gColorGroups[2].Name = QApplication::tr("Special", "Colors");

	for (swColor& Color : Colors)
	{
		int ColorIndex;

		for (ColorIndex = 0; ColorIndex < static_cast<int>(gColorList.size()); ColorIndex++)
			if (gColorList[ColorIndex].Code == Color.Code)
				break;

		if (ColorIndex == static_cast<int>(gColorList.size()))
			gColorList.push_back(Color);
		else
			gColorList[ColorIndex] = Color;

		if (Color.Group != SW_COLOR_GROUP_COUNT)
			gColorGroups[Color.Group].Colors.push_back(ColorIndex);

		if (Color.Code == 16)
			gDefaultColor = ColorIndex;
		else if (Color.Code == 24)
			gEdgeColor = ColorIndex;
	}

	return Valid;
}

void swLoadDefaultColors(swStudStyle StudStyle)
{
	swDiskFile ConfigFile(":/resources/data/ldconfig.ldr");

	if (ConfigFile.Open(QIODevice::ReadOnly))
		swLoadColorFile(ConfigFile, StudStyle);
}

int swGetColorIndex(quint32 ColorCode)
{
	for (size_t ColorIdx = 0; ColorIdx < gColorList.size(); ColorIdx++)
		if (gColorList[ColorIdx].Code == ColorCode)
			return (int)ColorIdx;

	swColor Color;

	Color.Code = ColorCode;
	Color.Translucent = false;
	Color.Chrome = false;
	Color.Rubber = false;
	Color.Edge[0] = 0.2f;
	Color.Edge[1] = 0.2f;
	Color.Edge[2] = 0.2f;
	Color.Edge[3] = 1.0f;

	if (ColorCode & SW_COLOR_DIRECT)
	{
		Color.Value[0] = (float)((ColorCode & 0xff0000) >> 16) / 255.0f;
		Color.Value[1] = (float)((ColorCode & 0x00ff00) >> 8) / 255.0f;
		Color.Value[2] = (float)((ColorCode & 0x0000ff) >> 0) / 255.0f;
		Color.Value[3] = 1.0f;
		snprintf(Color.Name, sizeof(Color.Name), "Color %06X", ColorCode & 0xffffff);
		snprintf(Color.SafeName, sizeof(Color.SafeName), "Color_%06X", ColorCode & 0xffffff);
	}
	else
	{
		Color.Value[0] = 0.5f;
		Color.Value[1] = 0.5f;
		Color.Value[2] = 0.5f;
		Color.Value[3] = 1.0f;
		snprintf(Color.Name, sizeof(Color.Name), "Color %03d", ColorCode);
		snprintf(Color.SafeName, sizeof(Color.SafeName), "Color_%03d", ColorCode);
	}

	gColorList.push_back(Color);
	return (int)gColorList.size() - 1;
}

QString swGetColorToolTip(int ColorIndex)
{
	swColor* Color = &gColorList[ColorIndex];
	QColor Rgb(Color->Value[0] * 255, Color->Value[1] * 255, Color->Value[2] * 255);

	QImage Image(16, 16, QImage::Format_RGB888);
	Image.fill(Rgb);

	QPainter Painter(&Image);
	Painter.setPen(Qt::darkGray);

	if (Color->Code != SW_COLOR_NO_COLOR)
		Painter.drawRect(0, 0, Image.width() - 1, Image.height() - 1);
	else
		swDrawNoColorRectangle(Painter, QRect(0, 0, Image.width() - 1, Image.height() - 1));

	Painter.end();

	QByteArray ByteArray;
	QBuffer Buffer(&ByteArray);
	Buffer.open(QIODevice::WriteOnly);
	Image.save(&Buffer, "PNG");
	Buffer.close();

	QString Text;

	if (Color->Code != SW_COLOR_NO_COLOR)
	{
		const char* format = "<table><tr><td style=\"vertical-align:middle\"><img src=\"data:image/png;base64,%1\"/></td><td>%2 (%3)</td></tr></table>";
		Text = QString(format).arg(QString(Buffer.data().toBase64()), gColorList[ColorIndex].Name, QString::number(gColorList[ColorIndex].Code));
	}
	else
	{
		const char* format = "<table><tr><td style=\"vertical-align:middle\"><img src=\"data:image/png;base64,%1\"/></td><td>%2</td></tr></table>";
		Text = QString(format).arg(QString(Buffer.data().toBase64()), gColorList[ColorIndex].Name);
	}

	return Text;
}


void swDrawNoColorRectangle(QPainter& Painter, const QRect& Rect)
{
	Painter.setBrush(Qt::black);
	Painter.drawRect(Rect);

	const int SquareSize = 3;
	int Column = 0;

	for (;;)
	{
		int x = Rect.left() + 1 + Column * SquareSize;

		if (x >= Rect.right())
			break;

		int Row = Column & 1;

		for (;;)
		{
			int y = Rect.top() + 1 + Row * SquareSize;

			if (y >= Rect.bottom())
				break;

			QRect GridRect(x, y, SquareSize, SquareSize);

			if (GridRect.right() > Rect.right())
				GridRect.setRight(Rect.right());

			if (GridRect.bottom() > Rect.bottom())
				GridRect.setBottom(Rect.bottom());

			Painter.fillRect(GridRect, Qt::white);

			Row += 2;
		}

		Column++;
	}
}
