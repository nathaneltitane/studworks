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
	StepImageMargin,
	StepImagePadding,
	StepImageBorderWidth,
	StepImageBorderColor,
	StepImageBackgroundColor,
	StepNumberMargin,
	StepNumberPadding,
	StepNumberBorderWidth,
	StepNumberBorderColor,
	StepNumberBackgroundColor,
	PLIMargin,
	PLIPadding,
	PageNumberMargin,
	PageNumberPadding,
	PageNumberBorderWidth,
	PageNumberBorderColor,
	PageNumberBackgroundColor,
	CoverTitleMargin,
	CoverTitlePadding,
	CoverTitleBorderWidth,
	CoverTitleBorderColor,
	CoverTitleBackgroundColor,
	CoverAuthorMargin,
	CoverAuthorPadding,
	CoverAuthorBorderWidth,
	CoverAuthorBorderColor,
	CoverAuthorBackgroundColor,
	CoverImageMargin,
	CoverImagePadding,
	CoverImageBorderWidth,
	CoverImageBorderColor,
	CoverImageBackgroundColor,
	NoticeFont,
	NoticeTextColor,
	NoticeMargin,
	NoticePadding,
	NoticeBorderWidth,
	NoticeBorderColor,
	NoticeBackgroundColor,
	PointerStyle,
	PointerTip,
	PointerColor,
	PointerWidth,
	PointerTipSize,
	PLILayout,
	PLIAlignment,
	ModelProjection,
	FadePreviousSteps,
	FadeColor,
	HighlightNewParts,
	HighlightColor,
	Count
};

// the page number alone or with the page count
enum class swInstructionsPageNumberFormat
{
	Number,
	NumberOfPages
};

// every page element is drawn in a box, the margin and padding surround its border and its background fills all of it
enum class swInstructionsBoxElement
{
	StepImage,
	StepNumber,
	PartsList,
	PageNumber,
	CoverTitle,
	CoverAuthor,
	CoverImage,
	Notice,
	Count
};

struct swInstructionsBoxTypes
{
	swInstructionsPropertyType Margin;
	swInstructionsPropertyType Padding;
	swInstructionsPropertyType BorderWidth;
	swInstructionsPropertyType BorderColor;
	swInstructionsPropertyType BackgroundColor;
};

// use to get the properties of the box of an element, the parts list keeps its own border and background
swInstructionsBoxTypes swGetBoxTypes(swInstructionsBoxElement Element);

struct swInstructionsBoxStyle
{
	qreal Margin = 0.0;
	qreal Padding = 0.0;
	qreal BorderWidth = 0.0;
	QColor BorderColor;
	QColor BackgroundColor;

	// the distance from the edge of the box to its content
	qreal GetInset() const
	{
		return Margin + BorderWidth + Padding;
	}
};

// a pointer is a line or a callout wedge, ending in an arrowhead or a dot
enum class swInstructionsPointerStyle
{
	Line,
	Wedge
};

enum class swInstructionsPointerTip
{
	Arrowhead,
	Dot
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
	PageNumber,
	Notice,
	Pointer
};

// a moved element keeps its offset from its placement through page rebuilds
using swInstructionsElementKey = std::tuple<swInstructionsPlacementElement, swModel*, swStep>;

// the grid size is stored in points and shown in the margin units
constexpr float SW_GRID_SIZE_MINIMUM = 2.0f;
constexpr float SW_GRID_SIZE_MAXIMUM = 144.0f;
constexpr int SW_GRID_DIVISIONS_MAXIMUM = 64;

// the longer page side renders at least this many pixels, 300 dpi on a letter page
constexpr qreal SW_INSTRUCTIONS_RENDER_SIZE = 3300.0;

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

// notices and pointers belong to one page, found by its type, its first step and its rank among pages sharing them
using swInstructionsPageKey = std::tuple<swInstructionsPageType, swModel*, swStep, int>;

// a notice is a free text box placed on a page
struct swInstructionsNotice
{
	int Id = 0;
	swInstructionsPageKey PageKey;
	QPointF Position;
	QString Text;
};

// a pointer leaves the border of an element at one of its 8 anchors and points at a part, another element or a point of the page
constexpr int SW_POINTER_ANCHOR_AUTO = -1;
constexpr int SW_POINTER_ANCHOR_COUNT = 8;

enum class swInstructionsPointerTarget
{
	Point,
	Element,
	Part
};

struct swInstructionsPointer
{
	int Id = 0;
	swInstructionsPageKey PageKey;
	swInstructionsElementKey Source;
	int Anchor = SW_POINTER_ANCHOR_AUTO;
	swInstructionsPointerTarget TargetType = swInstructionsPointerTarget::Point;
	swInstructionsElementKey TargetElement;
	int TargetPart = -1;
	QPointF TipPosition;

	// a pointer without its own style or tip follows the pointer properties
	int Style = -1;
	int Tip = -1;
};

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

// the instructions are saved as meta commands of the main model, which names the models it refers to
void swRenameInstructionsModels(QStringList& CommandLines, const std::map<QString, QString>& Names);

class swInstructions : public QObject
{
	Q_OBJECT

public:
	swInstructions(Project* Project = nullptr);

	void SetProject(Project* Project);

	void SetDefaultPageSettings(const swInstructionsPageSettings& PageSettings);
	swInstructionsPageSettings GetPageSettings(size_t PageIndex) const;
	void SetPageSettings(size_t PageIndex, int Rows, int Columns);
	void SetPageDividerPreset(size_t PageIndex, int Rows, int Columns);
	bool CanInsertBlankPage(size_t PageIndex) const;
	void InsertBlankPage(size_t PageIndex);
	void RemoveBlankPage(size_t PageIndex);
	void SetPageSetup(const swInstructionsPageSetup& PageSetup);
	void ResetPageSetup();
	static void LoadDefaultPageSetup();
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

	// the box of an element, the bill of materials uses the box of the parts list
	swInstructionsBoxStyle GetBoxStyle(swInstructionsBoxElement Element, swModel* Model, swStep Step) const;

	// notices and pointers stay on their page through page layout changes
	swInstructionsPageKey GetPageElementsKey(size_t PageIndex) const;

	const std::vector<swInstructionsNotice>& GetNotices() const
	{
		return mNotices;
	}

	const swInstructionsNotice* FindNotice(int Id) const;
	int AddNotice(size_t PageIndex, const QPointF& Position, const QString& Text);
	void SetNoticeText(int Id, const QString& Text);
	void RemoveNotice(int Id);

	const std::vector<swInstructionsPointer>& GetPointers() const
	{
		return mPointers;
	}

	const swInstructionsPointer* FindPointer(int Id) const;
	int AddPointer(size_t PageIndex, const swInstructionsElementKey& Source, const QPointF& TipPosition);
	void SetPointerTarget(int Id, swInstructionsPointerTarget TargetType, const swInstructionsElementKey& TargetElement, int TargetPart, const QPointF& TipPosition);
	void SetPointerAnchor(int Id, int Anchor);
	void SetPointerStyle(int Id, int Style, int Tip);
	void RemovePointer(int Id);

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

	// the grid size is the main unit, its divisions split it into the fractions dragged elements snap to
	int GetGridDivisions() const
	{
		return mGridDivisions;
	}

	float GetGridSnapSize() const
	{
		return mGridSize / mGridDivisions;
	}

	QColor GetGridPrimaryColor() const
	{
		return mGridPrimaryColor;
	}

	QColor GetGridSecondaryColor() const
	{
		return mGridSecondaryColor;
	}

	// the grid covers the area inside the margins or the full page, it starts at the margins or the page corner
	bool IsGridWithinMargins() const
	{
		return mGridWithinMargins;
	}

	QRectF GetGridRect() const;

	// the margins are outlined on screen like the grid, they are never printed
	bool IsMarginsShown() const
	{
		return mShowMargins;
	}

	QColor GetMarginColor() const
	{
		return mMarginColor;
	}


	// every length shown in the instructions uses the margin units of the page setup
	swInstructionsPageUnits GetPageUnits() const;

	// the document units replace the page units and every unit chosen for a single value
	std::optional<swInstructionsPageUnits> GetDocumentUnits() const;

	void SetShowGrid(bool ShowGrid);
	void SetSnapToGrid(bool SnapToGrid);
	void SetGridSize(float GridSize);
	void SetGridDivisions(int Divisions);
	void SetGridPrimaryColor(const QColor& Color);
	void SetGridSecondaryColor(const QColor& Color);
	void SetMarginColor(const QColor& Color);
	void UpdateColors();
	void SetGridWithinMargins(bool WithinMargins);
	void SetShowMargins(bool ShowMargins);

//protected:
	std::vector<swInstructionsPage> mPages;
	swInstructionsPageSettings mPageSettings;
	std::map<swInstructionsStepKey, swInstructionsPageSettings> mPageSettingsOverrides;
	std::map<swInstructionsStepKey, swInstructionsProperties> mPagePropertyOverrides;
	std::map<swInstructionsElementKey, QPointF> mElementOffsets;
	std::map<swInstructionsElementKey, QPointF> mLockedPositions;
	std::multiset<swInstructionsStepKey> mBlankPages;
	std::vector<swInstructionsNotice> mNotices;
	std::vector<swInstructionsPointer> mPointers;
	swInstructionsPageSetup mPageSetup;
	swInstructionsProperties mStepProperties;
	swInstructionsProperties mDefaultProperties;

	std::map<swModel*, swInstructionsModel> mModels;

signals:
	void StepSettingsChanged(swModel* Model, swStep Step);
	void PageSetupChanged();
	void PageLayoutChanged();
	void GridChanged();
	void UnitsChanged();

protected:
	static swInstructionsProperties CreateDefaultProperties();
	void SetDefaultProperties();
	void LoadCommandLines();
	QStringList GetCommandLines() const;
	void StoreCommandLines();
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
	bool mShowGrid = true;
	bool mSnapToGrid = true;
	bool mGridWithinMargins = true;
	bool mShowMargins = true;
	float mGridSize = 72.0f;
	int mGridDivisions = 8;
	QColor mGridPrimaryColor;
	QColor mGridSecondaryColor;
	QColor mMarginColor;
	int mNextNoticeId = 1;
	int mNextPointerId = 1;

	static const float mDisplayDPI;

	// the default blank pages after the cover and before the bill of materials are keyed apart from the steps
	static const swInstructionsStepKey mCoverBlankKey;
	static const swInstructionsStepKey mBillOfMaterialsBlankKey;
};
