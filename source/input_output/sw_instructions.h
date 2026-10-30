#pragma once

#include "sw_math.h"

enum class swInstructionsPageUnits
{
	Pixels,
	Millimeters,
	Centimeters,
	Inches
};

// Gets the points per page unit.
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

	// Parts list spacing and alignment.
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

// The page number with or without count.
enum class swInstructionsPageNumberFormat
{
	Number,
	NumberOfPages
};

// The box around every page element.
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

// Gets the box of an element.
swInstructionsBoxTypes swGetBoxTypes(swInstructionsBoxElement Element);

struct swInstructionsBoxStyle
{
	qreal Margin = 0.0;
	qreal Padding = 0.0;
	qreal BorderWidth = 0.0;
	QColor BorderColor;
	QColor BackgroundColor;

	// The box padding.
	qreal GetInset() const
	{
		return Margin + BorderWidth + Padding;
	}
};

// A pointer line or callout.
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

// What a step element is placed against.
enum class swInstructionsPlacementAnchor
{
	Area,
	StepNumber,
	PartsList
};

// Placement cells around the anchor.
constexpr int SW_PLACEMENT_GRID_SIZE = 5;
constexpr int SW_PLACEMENT_COUNT = SW_PLACEMENT_GRID_SIZE * SW_PLACEMENT_GRID_SIZE;

inline bool swIsInsidePlacement(int Placement)
{
	const int Row = Placement / SW_PLACEMENT_GRID_SIZE;
	const int Column = Placement % SW_PLACEMENT_GRID_SIZE;

	return Row > 0 && Row < SW_PLACEMENT_GRID_SIZE - 1 && Column > 0 && Column < SW_PLACEMENT_GRID_SIZE - 1;
}

// Moves a placement into an area.
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
			// Center goes right of the element.
			Row = 1;
			Column = Last;
		}
	}

	return Row * SW_PLACEMENT_GRID_SIZE + Column;
}

// The selectable page elements.
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

// Moved elements keep their offset.
using swInstructionsElementKey = std::tuple<swInstructionsPlacementElement, swModel*, swStep>;

// The grid size in points.
constexpr float SW_GRID_SIZE_MINIMUM = 2.0f;
constexpr float SW_GRID_SIZE_MAXIMUM = 144.0f;
constexpr int SW_GRID_DIVISIONS_MAXIMUM = 64;

// The minimum render size of a page.
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

// The page notices and pointers belong to.
using swInstructionsPageKey = std::tuple<swInstructionsPageType, swModel*, swStep, int>;

// A free text box on a page.
struct swInstructionsNotice
{
	int Id = 0;
	swInstructionsPageKey PageKey;
	QPointF Position;
	QString Text;
};

// A pointer from an element anchor.
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

	// Pointers default to the pointer properties.
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

// The instructions are main model metas.
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

	// Properties set on a single page.
	static bool IsPageProperty(swInstructionsPropertyType Type);
	bool HasPageProperty(size_t PageIndex, swInstructionsPropertyType Type) const;
	void SetPageScope(size_t PageIndex, swInstructionsPropertyType Type, bool PageOnly);
	int GetPageIntProperty(size_t PageIndex, swInstructionsPropertyType Type) const;
	QColor GetPageColorProperty(size_t PageIndex, swInstructionsPropertyType Type) const;
	void SetPageInt(size_t PageIndex, swInstructionsPropertyType Type, int Value);
	void SetPageColor(size_t PageIndex, swInstructionsPropertyType Type, const QColor& Color);

	// Cover and parts list backgrounds.
	QColor GetPageBackgroundColor(size_t PageIndex) const;

	QPointF GetElementOffset(const swInstructionsElementKey& Key) const;
	bool HasElementOffset(const swInstructionsElementKey& Key) const;
	void SetElementOffsets(const std::map<swInstructionsElementKey, QPointF>& Offsets);
	void ResetElementOffset(const swInstructionsElementKey& Key);
	void SetElementPlacement(swInstructionsPropertyType Type, int Placement, const swInstructionsElementKey& Key);

	// The box of an element.
	swInstructionsBoxStyle GetBoxStyle(swInstructionsBoxElement Element, swModel* Model, swStep Step) const;

	// Notices and pointers stay on their page.
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

	// A locked element keeps its position.
	bool IsElementLocked(const swInstructionsElementKey& Key) const;
	QPointF GetLockedPosition(const swInstructionsElementKey& Key) const;
	void LockElement(const swInstructionsElementKey& Key, const QPointF& Position);
	void UnlockElement(const swInstructionsElementKey& Key, const QPointF& Offset);

	// The page grid shows on screen only.
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

	// The grid size and its divisions.
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

	// The area the grid covers.
	bool IsGridWithinMargins() const
	{
		return mGridWithinMargins;
	}

	QRectF GetGridRect() const;

	// The margins show on screen only.
	bool IsMarginsShown() const
	{
		return mShowMargins;
	}

	QColor GetMarginColor() const
	{
		return mMarginColor;
	}


	// Lengths use the margin units.
	swInstructionsPageUnits GetPageUnits() const;

	// The document units replace page units.
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

	// Keys of the default blank pages.
	static const swInstructionsStepKey mCoverBlankKey;
	static const swInstructionsStepKey mBillOfMaterialsBlankKey;
};
