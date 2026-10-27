#pragma once

#include "sw_math.h"
#include "sw_types_asset.h"

enum SW_MINIFIGURE_TYPES
{
	SW_MINIFIGURE_HATS,
	SW_MINIFIGURE_HATS2,
	SW_MINIFIGURE_HEAD,
	SW_MINIFIGURE_NECK,
	SW_MINIFIGURE_BODY,
	SW_MINIFIGURE_BODY2,
	SW_MINIFIGURE_BODY3,
	SW_MINIFIGURE_RIGHT_ARM,
	SW_MINIFIGURE_LEFT_ARM,
	SW_MINIFIGURE_RIGHT_HAND,
	SW_MINIFIGURE_LEFT_HAND,
	SW_MINIFIGURE_RIGHT_HAND_ACCESSORY,
	SW_MINIFIGURE_LEFT_HAND_ACCESSORY,
	SW_MINIFIGURE_RIGHT_LEG,
	SW_MINIFIGURE_LEFT_LEG,
	SW_MINIFIGURE_RIGHT_LEG_ACCESSORY,
	SW_MINIFIGURE_LEFT_LEG_ACCESSORY,
	SW_MINIFIGURE_ITEM_COUNT
};

struct swMinifigurePartInformation
{
	char Description[128];
	swPartData* PartData;
	swMatrix44 Offset;
};

struct swMinifigure
{
	swPartDataReference PartData[SW_MINIFIGURE_ITEM_COUNT];
	int ColorIndices[SW_MINIFIGURE_ITEM_COUNT];
	float Angles[SW_MINIFIGURE_ITEM_COUNT];
	swMatrix44 Matrices[SW_MINIFIGURE_ITEM_COUNT];
};

struct swMinifigureTemplate
{
	QString Parts[SW_MINIFIGURE_ITEM_COUNT];
	int ColorCodes[SW_MINIFIGURE_ITEM_COUNT];
	float Angles[SW_MINIFIGURE_ITEM_COUNT];
};

class swMinifigureBuilder
{
public:
	swMinifigureBuilder();
	~swMinifigureBuilder();

	swMinifigureBuilder(const swMinifigureBuilder&) = delete;
	swMinifigureBuilder(swMinifigureBuilder&&) = delete;
	swMinifigureBuilder& operator=(const swMinifigureBuilder&) = delete;
	swMinifigureBuilder& operator=(swMinifigureBuilder&&) = delete;

	swModel* GetModel() const
	{
		return mModel.get();
	}

	const std::map<QString, swMinifigureTemplate>& GetTemplates() const
	{
		return mTemplates;
	}

	static QString GetDefaultTemplateName();

	void LoadTemplate(const QString& TemplateName);
	void SaveTemplate(const QString& TemplateName, const swMinifigureTemplate& Template);
	void DeleteTemplate(const QString& TemplateName);
	void AddTemplatesJson(const QByteArray& TemplateData);
	QByteArray GetTemplatesJson() const;

	void Calculate();
	int GetSelectionIndex(int Type) const;
	void SetPartData(int Type, swPartData* PartData);
	void SetColorIndex(int Type, int ColorIndex);
	void SetAngle(int Type, float Angle);

	std::vector<swMinifigurePartInformation> mSettings[SW_MINIFIGURE_ITEM_COUNT];

	swMinifigure mMinifigure;

protected:
	void LoadSettings();
	void ParseSettings(swFile& Settings);

	void LoadTemplates();
	void SaveTemplates();

	std::unique_ptr<swModel> mModel;
	std::map<QString, swMinifigureTemplate> mTemplates;
	static const char* mSectionNames[SW_MINIFIGURE_ITEM_COUNT];
};

