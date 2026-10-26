#pragma once

#include "sw_math.h"
#include "sw_assettypes.h"

enum SW_MFW_TYPES
{
	SW_MFW_HATS,
	SW_MFW_HATS2,
	SW_MFW_HEAD,
	SW_MFW_NECK,
	SW_MFW_BODY,
	SW_MFW_BODY2,
	SW_MFW_BODY3,
	SW_MFW_RARM,
	SW_MFW_LARM,
	SW_MFW_RHAND,
	SW_MFW_LHAND,
	SW_MFW_RHANDA,
	SW_MFW_LHANDA,
	SW_MFW_RLEG,
	SW_MFW_LLEG,
	SW_MFW_RLEGA,
	SW_MFW_LLEGA,
	SW_MFW_NUMITEMS
};

struct swMinifigPieceInfo
{
	char Description[128];
	swPieceData* PieceData;
	swMatrix44 Offset;
};

struct swMinifig
{
	swPieceDataRef PieceData[SW_MFW_NUMITEMS];
	int ColorIndices[SW_MFW_NUMITEMS];
	float Angles[SW_MFW_NUMITEMS];
	swMatrix44 Matrices[SW_MFW_NUMITEMS];
};

struct swMinifigTemplate
{
	QString Parts[SW_MFW_NUMITEMS];
	int ColorCodes[SW_MFW_NUMITEMS];
	float Angles[SW_MFW_NUMITEMS];
};

class MinifigWizard
{
public:
	MinifigWizard();
	~MinifigWizard();

	MinifigWizard(const MinifigWizard&) = delete;
	MinifigWizard(MinifigWizard&&) = delete;
	MinifigWizard& operator=(const MinifigWizard&) = delete;
	MinifigWizard& operator=(MinifigWizard&&) = delete;

	swModel* GetModel() const
	{
		return mModel.get();
	}

	const std::map<QString, swMinifigTemplate>& GetTemplates() const
	{
		return mTemplates;
	}

	static QString GetDefaultTemplateName();

	void LoadTemplate(const QString& TemplateName);
	void SaveTemplate(const QString& TemplateName, const swMinifigTemplate& Template);
	void DeleteTemplate(const QString& TemplateName);
	void AddTemplatesJson(const QByteArray& TemplateData);
	QByteArray GetTemplatesJson() const;

	void Calculate();
	int GetSelectionIndex(int Type) const;
	void SetPieceData(int Type, swPieceData* PieceData);
	void SetColorIndex(int Type, int ColorIndex);
	void SetAngle(int Type, float Angle);

	std::vector<swMinifigPieceInfo> mSettings[SW_MFW_NUMITEMS];

	swMinifig mMinifig;

protected:
	void LoadSettings();
	void ParseSettings(swFile& Settings);

	void LoadTemplates();
	void SaveTemplates();

	std::unique_ptr<swModel> mModel;
	std::map<QString, swMinifigTemplate> mTemplates;
	static const char* mSectionNames[SW_MFW_NUMITEMS];
};

