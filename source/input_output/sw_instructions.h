#pragma once

#include "sw_math.h"

enum class swInstructionsPageUnits
{
	Pixels,
	Millimeters,
	Centimeters,
	Inches
};

// use to get the points in one page unit and the unit suffix shown next to the values
float swGetPageUnitScale(swInstructionsPageUnits Units);
QString swGetPageUnitSuffix(swInstructionsPageUnits Units);

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
	CoverTitleColor,
	ShowCoverAuthor,
	PageBackgroundColor,
	StepNumberPlacement,
	StepNumberAnchor,
	PLIPlacement,
	PLIAnchor,
	CoverTitlePlacement,
	BillOfMaterialsPlacement,
	PLIThumbnailSize,
	PLIThumbnailBackgroundColor,
	CoverAuthorPlacement,
	CoverImagePlacement,
	StepImagePlacement,
	CoverBackgroundColor,
	BillOfMaterialsBackgroundColor,
	ShowCoverBlankPage,
	ShowBillOfMaterialsBlankPage,
	UseDocumentUnits,
	DocumentUnits,
	ShowPageNumber,
	PageNumberFormat,
	PageNumberFont,
	PageNumberColor,
	PageNumberPlacement,
	ShowPageNumberAllPages,
	Count
};

// the page number alone or with the page count
enum class swInstructionsPageNumberFormat
{
	Number,
	NumberOfPages
};

// what a step element is placed against, the area is the page or its part given to the step by the split
enum class swInstructionsPlacementAnchor
{
	Area,
	StepNumber,
	PartsList
};

// placements are cells of a grid around the anchor, numbered row by row, its inner cells lie inside the anchor
constexpr int SW_PLACEMENT_GRID_SIZE = 5;
constexpr int SW_PLACEMENT_COUNT = SW_PLACEMENT_GRID_SIZE * SW_PLACEMENT_GRID_SIZE;

inline bool swIsInsidePlacement(int Placement)
{
	const int Row = Placement / SW_PLACEMENT_GRID_SIZE;
	const int Column = Placement % SW_PLACEMENT_GRID_SIZE;

	return Row > 0 && Row < SW_PLACEMENT_GRID_SIZE - 1 && Column > 0 && Column < SW_PLACEMENT_GRID_SIZE - 1;
}

// use to move a placement inside an area or outside an element, the nearest cell keeps its side
inline int swGetValidPlacement(int Placement, bool Inside)
{
	const int Last = SW_PLACEMENT_GRID_SIZE - 1;
	int Row = qBound(0, Placement / SW_PLACEMENT_GRID_SIZE, Last);
	int Column = qBound(0, Placement % SW_PLACEMENT_GRID_SIZE, Last);

	if (Inside)
	{
		Row = qBound(1, Row, Last - 1);
		Column = qBound(1, Column, Last - 1);
	}
	else if (swIsInsidePlacement(Row * SW_PLACEMENT_GRID_SIZE + Column))
	{
		if (Column == 1)
			Column = 0;
		else if (Column == Last - 1)
			Column = Last;
		else if (Row == 1)
			Row = 0;
		else if (Row == Last - 1)
			Row = Last;
		else
		{
			// the center goes to the right of the element, aligned to its top
			Row = 1;
			Column = Last;
		}
	}

	return Row * SW_PLACEMENT_GRID_SIZE + Column;
}

// the page elements that can be selected and moved, the placement panel places some of them
enum class swInstructionsPlacementElement
{
	None,
	StepNumber,
	PartsList,
	CoverTitle,
	CoverAuthor,
	BillOfMaterials,
	StepImage,
	CoverImage,
	PageNumber
};

// a moved element keeps its offset from its placement through page rebuilds
using swInstructionsElementKey = std::tuple<swInstructionsPlacementElement, swModel*, swStep>;

// the grid size is stored in points and shown in the margin units
constexpr float SW_GRID_SIZE_MINIMUM = 2.0f;
constexpr float SW_GRID_SIZE_MAXIMUM = 144.0f;

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

	void SetDefaultPageSettings(const swInstructionsPageSettings& PageSettings);
	swInstructionsPageSettings GetPageSettings(size_t PageIndex) const;
	void SetPageSettings(size_t PageIndex, int Rows, int Columns);
	void SetPageDividerPreset(size_t PageIndex, int Rows, int Columns);
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
	void ResetProperties();

	// some properties can differ on a single page, the page keeps them through page layout changes
	static bool IsPageProperty(swInstructionsPropertyType Type);
	bool HasPageProperty(size_t PageIndex, swInstructionsPropertyType Type) const;
	void SetPageScope(size_t PageIndex, swInstructionsPropertyType Type, bool PageOnly);
	int GetPageIntProperty(size_t PageIndex, swInstructionsPropertyType Type) const;
	QColor GetPageColorProperty(size_t PageIndex, swInstructionsPropertyType Type) const;
	void SetPageInt(size_t PageIndex, swInstructionsPropertyType Type, int Value);
	void SetPageColor(size_t PageIndex, swInstructionsPropertyType Type, const QColor& Color);

	// the cover and the bill of materials have their own background, the other pages share one unless a page has its own
	QColor GetPageBackgroundColor(size_t PageIndex) const;

	QPointF GetElementOffset(const swInstructionsElementKey& Key) const;
	bool HasElementOffset(const swInstructionsElementKey& Key) const;
	void SetElementOffsets(const std::map<swInstructionsElementKey, QPointF>& Offsets);
	void ResetElementOffset(const swInstructionsElementKey& Key);
	void SetElementPlacement(swInstructionsPropertyType Type, int Placement, const swInstructionsElementKey& Key);

	// a locked element keeps its position on the page whatever its placement and is not dragged
	bool IsElementLocked(const swInstructionsElementKey& Key) const;
	QPointF GetLockedPosition(const swInstructionsElementKey& Key) const;
	void LockElement(const swInstructionsElementKey& Key, const QPointF& Position);
	void UnlockElement(const swInstructionsElementKey& Key, const QPointF& Offset);

	// the page grid helps aligning elements on screen, it is never printed
	bool IsGridShown() const
	{
		return mShowGrid;
	}

	bool IsSnapToGrid() const
	{
		return mSnapToGrid;
	}

	float GetGridSize() const
	{
		return mGridSize;
	}

	// the grid covers the area inside the margins or the full page, it starts at the margins or the page corner
	bool IsGridWithinMargins() const
	{
		return mGridWithinMargins;
	}

	QRectF GetGridRect() const;

	// every length shown in the instructions uses the margin units of the page setup
	swInstructionsPageUnits GetPageUnits() const;

	// the document units replace the page units and every unit chosen for a single value
	std::optional<swInstructionsPageUnits> GetDocumentUnits() const;

	void SetShowGrid(bool ShowGrid);
	void SetSnapToGrid(bool SnapToGrid);
	void SetGridSize(float GridSize);
	void SetGridWithinMargins(bool WithinMargins);

//protected:
	std::vector<swInstructionsPage> mPages;
	swInstructionsPageSettings mPageSettings;
	std::map<swInstructionsStepKey, swInstructionsPageSettings> mPageSettingsOverrides;
	std::map<swInstructionsStepKey, swInstructionsProperties> mPagePropertyOverrides;
	std::map<swInstructionsElementKey, QPointF> mElementOffsets;
	std::map<swInstructionsElementKey, QPointF> mLockedPositions;
	std::multiset<swInstructionsStepKey> mBlankPages;
	swInstructionsPageSetup mPageSetup;
	swInstructionsProperties mStepProperties;

	std::map<swModel*, swInstructionsModel> mModels;

signals:
	void StepSettingsChanged(swModel* Model, swStep Step);
	void PageSetupChanged();
	void PageLayoutChanged();
	void GridChanged();
	void UnitsChanged();

protected:
	void SetDefaultProperties();
	void CreatePages();
	void AddDefaultPages(swModel* Model, std::vector<const swModel*>& AddedModels);
	void AddBlankPages(swInstructionsPage& Page, const swInstructionsStepKey& Key);
	void AddDefaultBlankPage(const swInstructionsStepKey& Key);
	swInstructionsPageSettings GetStepPageSettings(const swInstructionsStepKey& Key) const;
	bool GetPageKey(size_t PageIndex, swInstructionsStepKey& Key) const;

	QVariant GetProperty(swInstructionsPropertyType Type, swModel* Model, swStep Step) const;
	QVariant GetPageProperty(size_t PageIndex, swInstructionsPropertyType Type) const;
	void SetPageProperty(size_t PageIndex, swInstructionsPropertyType Type, const QVariant& Value);
	bool GetStepPageIndex(swModel* Model, swStep Step, size_t& PageIndex) const;
	void EmitPagePropertyChanged(swInstructionsPropertyType Type);
	void SetDefaultProperty(swInstructionsPropertyType Type, const QVariant& Value);

	Project* mProject = nullptr;
	bool mShowGrid = false;
	bool mSnapToGrid = false;
	bool mGridWithinMargins = true;
	float mGridSize = 18.0f;

	static const float mDisplayDPI;

	// the default blank pages after the cover and before the bill of materials are keyed apart from the steps
	static const swInstructionsStepKey mCoverBlankKey;
	static const swInstructionsStepKey mBillOfMaterialsBlankKey;
};
