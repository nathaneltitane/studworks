#pragma once

#include "sw_math.h"

enum class swInstructionsPageUnits
{
	Pixels,
	Millimeters,
	Centimeters,
	Inches
};

struct swInstructionsPageSetup
{
	float Width;
	float Height;
	float MarginLeft;
	float MarginRight;
	float MarginTop;
	float MarginBottom;
};

enum class swInstructionsDirection
{
	Horizontal,
	Vertical
};

struct swInstructionsPageSettings
{
	int Rows;
	int Columns;
	swInstructionsDirection Direction;
};

enum class swInstructionsPropertyMode
{
	NotSet,
	Default,
	Model,
	StepForward,
	StepOnly
};

enum class swInstructionsPropertyType
{
	ShowStepNumber,
	ShowStepPLI,
	StepNumberFont,
	StepNumberColor,
	StepBackgroundColor,
	PLIBackgroundColor,
	PLIFont,
	PLITextColor,
	PLIBorderColor,
	PLIBorderWidth,
//	PLIBorderRound,
	// pli: spacing and margins, text alignment
	PLIShowCounts,
	PLISortOrder,
	ShowStepDividers,
	StepDividerColor,
	StepDividerWidth,
	ShowBillOfMaterials,
	UseDocumentFont,
	DocumentFont,
	ShowCoverPage,
	CoverTitleFont,
	PageBackgroundColor,
	Count
};

struct swInstructionsProperty
{
	swInstructionsPropertyMode Mode = swInstructionsPropertyMode::NotSet;
	QVariant Value;
};

using swInstructionsProperties = std::array<swInstructionsProperty, static_cast<int>(swInstructionsPropertyType::Count)>;

struct swInstructionsStep
{
	QRectF Rect;
	swModel* Model;
	swStep Step;

	swInstructionsProperties Properties;
};

enum class swInstructionsPageType
{
	Steps,
	Cover,
	BillOfMaterials,
	Blank
};

using swInstructionsStepKey = std::pair<swModel*, swStep>;

struct swInstructionsPage
{
	swInstructionsPageSettings Settings;
	std::vector<swInstructionsStep> Steps;
	swInstructionsPageType Type = swInstructionsPageType::Steps;
	swModel* Model = nullptr;
	swInstructionsStepKey BlankKey = { nullptr, 0 };
};

struct swInstructionsModel
{
	std::vector<swInstructionsProperties> StepProperties;
};

class swInstructions : public QObject
{
	Q_OBJECT

public:
	swInstructions(Project* Project = nullptr);

	static QString GetPropertyLabel(swInstructionsPropertyType Type);

	void SetDefaultPageSettings(const swInstructionsPageSettings& PageSettings);
	swInstructionsPageSettings GetPageSettings(size_t PageIndex) const;
	void SetPageSettings(size_t PageIndex, int Rows, int Columns);
	bool CanInsertBlankPage(size_t PageIndex) const;
	void InsertBlankPage(size_t PageIndex);
	void RemoveBlankPage(size_t PageIndex);
	void SetPageSetup(const swInstructionsPageSetup& PageSetup);
	void Update();
	swInstructionsDirection GetPageDirection() const;

	bool GetBoolProperty(swInstructionsPropertyType Type, swModel* Model, swStep Step) const;
	int GetIntProperty(swInstructionsPropertyType Type, swModel* Model, swStep Step) const;
	QColor GetColorProperty(swInstructionsPropertyType Type, swModel* Model, swStep Step) const;
	QFont GetFontProperty(swInstructionsPropertyType Type, swModel* Model, swStep Step) const;

	void SetDefaultBool(swInstructionsPropertyType Type, bool Enabled);
	void SetDefaultInt(swInstructionsPropertyType Type, int Value);
	void SetDefaultColor(swInstructionsPropertyType Type, const QColor& Color);
	void SetDefaultFont(swInstructionsPropertyType Type, const QFont& Font);

//protected:
	std::vector<swInstructionsPage> mPages;
	swInstructionsPageSettings mPageSettings;
	std::map<swInstructionsStepKey, swInstructionsPageSettings> mPageSettingsOverrides;
	std::multiset<swInstructionsStepKey> mBlankPages;
	swInstructionsPageSetup mPageSetup;
	swInstructionsProperties mStepProperties;

	std::map<swModel*, swInstructionsModel> mModels;

signals:
	void StepSettingsChanged(swModel* Model, swStep Step);
	void PageSetupChanged();
	void PageLayoutChanged();

protected:
	void CreatePages();
	void AddDefaultPages(swModel* Model, std::vector<const swModel*>& AddedModels);
	void AddBlankPages(swInstructionsPage& Page, const swInstructionsStepKey& Key);
	swInstructionsPageSettings GetStepPageSettings(const swInstructionsStepKey& Key) const;
	bool GetPageKey(size_t PageIndex, swInstructionsStepKey& Key) const;

	QVariant GetProperty(swInstructionsPropertyType Type, swModel* Model, swStep Step) const;
	void SetDefaultProperty(swInstructionsPropertyType Type, const QVariant& Value);

	Project* mProject = nullptr;

	static const float mDisplayDPI;
};
