#pragma once

#include "sw_math.h"

enum class lcInstructionsPageUnits
{
	Pixels,
	Millimeters,
	Centimeters,
	Inches
};

struct lcInstructionsPageSetup
{
	float Width;
	float Height;
	float MarginLeft;
	float MarginRight;
	float MarginTop;
	float MarginBottom;
};

enum class lcInstructionsDirection
{
	Horizontal,
	Vertical
};

struct lcInstructionsPageSettings
{
	int Rows;
	int Columns;
	lcInstructionsDirection Direction;
};

enum class lcInstructionsPropertyMode
{
	NotSet,
	Default,
	Model,
	StepForward,
	StepOnly
};

enum class lcInstructionsPropertyType
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

struct lcInstructionsProperty
{
	lcInstructionsPropertyMode Mode = lcInstructionsPropertyMode::NotSet;
	QVariant Value;
};

using lcInstructionsProperties = std::array<lcInstructionsProperty, static_cast<int>(lcInstructionsPropertyType::Count)>;

struct lcInstructionsStep
{
	QRectF Rect;
	lcModel* Model;
	lcStep Step;

	lcInstructionsProperties Properties;
};

enum class lcInstructionsPageType
{
	Steps,
	Cover,
	BillOfMaterials,
	Blank
};

using lcInstructionsStepKey = std::pair<lcModel*, lcStep>;

struct lcInstructionsPage
{
	lcInstructionsPageSettings Settings;
	std::vector<lcInstructionsStep> Steps;
	lcInstructionsPageType Type = lcInstructionsPageType::Steps;
	lcModel* Model = nullptr;
	lcInstructionsStepKey BlankKey = { nullptr, 0 };
};

struct lcInstructionsModel
{
	std::vector<lcInstructionsProperties> StepProperties;
};

class lcInstructions : public QObject
{
	Q_OBJECT

public:
	lcInstructions(Project* Project = nullptr);

	static QString GetPropertyLabel(lcInstructionsPropertyType Type);

	void SetDefaultPageSettings(const lcInstructionsPageSettings& PageSettings);
	lcInstructionsPageSettings GetPageSettings(size_t PageIndex) const;
	void SetPageSettings(size_t PageIndex, int Rows, int Columns);
	bool CanInsertBlankPage(size_t PageIndex) const;
	void InsertBlankPage(size_t PageIndex);
	void RemoveBlankPage(size_t PageIndex);
	void SetPageSetup(const lcInstructionsPageSetup& PageSetup);
	void Update();
	lcInstructionsDirection GetPageDirection() const;

	bool GetBoolProperty(lcInstructionsPropertyType Type, lcModel* Model, lcStep Step) const;
	int GetIntProperty(lcInstructionsPropertyType Type, lcModel* Model, lcStep Step) const;
	QColor GetColorProperty(lcInstructionsPropertyType Type, lcModel* Model, lcStep Step) const;
	QFont GetFontProperty(lcInstructionsPropertyType Type, lcModel* Model, lcStep Step) const;

	void SetDefaultBool(lcInstructionsPropertyType Type, bool Enabled);
	void SetDefaultInt(lcInstructionsPropertyType Type, int Value);
	void SetDefaultColor(lcInstructionsPropertyType Type, const QColor& Color);
	void SetDefaultFont(lcInstructionsPropertyType Type, const QFont& Font);

//protected:
	std::vector<lcInstructionsPage> mPages;
	lcInstructionsPageSettings mPageSettings;
	std::map<lcInstructionsStepKey, lcInstructionsPageSettings> mPageSettingsOverrides;
	std::multiset<lcInstructionsStepKey> mBlankPages;
	lcInstructionsPageSetup mPageSetup;
	lcInstructionsProperties mStepProperties;

	std::map<lcModel*, lcInstructionsModel> mModels;

signals:
	void StepSettingsChanged(lcModel* Model, lcStep Step);
	void PageSetupChanged();
	void PageLayoutChanged();

protected:
	void CreatePages();
	void AddDefaultPages(lcModel* Model, std::vector<const lcModel*>& AddedModels);
	void AddBlankPages(lcInstructionsPage& Page, const lcInstructionsStepKey& Key);
	lcInstructionsPageSettings GetStepPageSettings(const lcInstructionsStepKey& Key) const;
	bool GetPageKey(size_t PageIndex, lcInstructionsStepKey& Key) const;

	QVariant GetProperty(lcInstructionsPropertyType Type, lcModel* Model, lcStep Step) const;
	void SetDefaultProperty(lcInstructionsPropertyType Type, const QVariant& Value);

	Project* mProject = nullptr;

	static const float mDisplayDPI;
};
