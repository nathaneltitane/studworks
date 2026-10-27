#pragma once

#include "sw_instructions.h"

class swCollapsibleWidgetButton;
class swInstructionsPropertiesWidget;
class swInstructionsPlacementWidget;

// File printing and the instructions window print the same instruction pages.
void swSetupInstructionsPrinter(swInstructions* Instructions, QPrinter* Printer);
void swPrintInstructions(swInstructions* Instructions, QPrinter* Printer);

// the page elements keep a padding around their content so they never touch their area or the elements placed against them
constexpr qreal SW_ELEMENT_PADDING = 12.0;

template <typename T>
class swInstructionsPaddedItem : public T
{
public:
	explicit swInstructionsPaddedItem(QGraphicsItem* Parent)
		: T(Parent)
	{
	}

	QRectF boundingRect() const override
	{
		const QRectF ContentRect = T::boundingRect();

		if (ContentRect.isEmpty())
			return ContentRect;

		return ContentRect.adjusted(0.0, 0.0, 2.0 * SW_ELEMENT_PADDING, 2.0 * SW_ELEMENT_PADDING);
	}

	QPainterPath shape() const override
	{
		QPainterPath Path;
		Path.addRect(boundingRect());

		return Path;
	}

	bool contains(const QPointF& Point) const override
	{
		return boundingRect().contains(Point);
	}

	void paint(QPainter* Painter, const QStyleOptionGraphicsItem* Option, QWidget* Widget) override
	{
		// the content is drawn inside the padding, the selection frames the padded box
		QStyleOptionGraphicsItem ContentOption(*Option);
		ContentOption.state &= ~(QStyle::State_Selected | QStyle::State_HasFocus);

		Painter->save();
		Painter->translate(SW_ELEMENT_PADDING, SW_ELEMENT_PADDING);
		T::paint(Painter, &ContentOption, Widget);
		Painter->restore();

		if (!(Option->state & QStyle::State_Selected))
			return;

		Painter->save();
		// the highlight stays visible on light pages with a dark interface theme
		QPen Pen(Option->palette.highlight(), 2.0, Qt::DashLine);
		Pen.setCosmetic(true);

		Painter->setPen(Pen);
		Painter->setBrush(Qt::NoBrush);
		Painter->drawRect(boundingRect());
		Painter->restore();
	}
};

class swInstructionsStepImageItem : public swInstructionsPaddedItem<QGraphicsPixmapItem>
{
public:
	swInstructionsStepImageItem(QGraphicsItem* Parent, swInstructions* Instructions, swModel* Model, swStep Step);

	swModel* GetModel() const
	{
		return mModel;
	}

	swStep GetStep() const
	{
		return mStep;
	}

	void SetImageSize(int Width, int Height)
	{
		mWidth = Width;
		mHeight = Height;
	}

	void SetCoverImage()
	{
		mCoverImage = true;
	}

	bool IsCoverImage() const
	{
		return mCoverImage;
	}

	void Update();

protected:
	QVariant itemChange(GraphicsItemChange Change, const QVariant& Value) override;

	swInstructions* mInstructions = nullptr;
	swModel* mModel = nullptr;
	swStep mStep = 1;
	int mWidth = 1;
	int mHeight = 1;
	bool mCoverImage = false;
};

class swInstructionsStepNumberItem : public swInstructionsPaddedItem<QGraphicsSimpleTextItem>
{
public:
	swInstructionsStepNumberItem(QGraphicsItem* Parent, swInstructions* Instructions, swModel* Model, swStep Step);

	swModel* GetModel() const
	{
		return mModel;
	}

	swStep GetStep() const
	{
		return mStep;
	}

	void Update();

protected:
	QVariant itemChange(GraphicsItemChange Change, const QVariant& Value) override;

	swInstructions* mInstructions = nullptr;
	swModel* mModel = nullptr;
	swStep mStep = 1;
};

// one page number item per page, they share their placement and offset so all pages number alike
class swInstructionsPageNumberItem : public swInstructionsPaddedItem<QGraphicsSimpleTextItem>
{
public:
	swInstructionsPageNumberItem(QGraphicsItem* Parent, swInstructions* Instructions, size_t PageIndex);

protected:
	QVariant itemChange(GraphicsItemChange Change, const QVariant& Value) override;

	swInstructions* mInstructions = nullptr;
};

class swInstructionsPartsListItem : public swInstructionsPaddedItem<QGraphicsPixmapItem>
{
public:
	swInstructionsPartsListItem(QGraphicsItem* Parent, swInstructions* Instructions, swModel* Model, swStep Step);

	swModel* GetModel() const
	{
		return mModel;
	}

	swStep GetStep() const
	{
		return mStep;
	}

	void SetBillOfMaterials(int MaxWidth)
	{
		mBillOfMaterials = true;
		mMaxWidth = MaxWidth;
	}

	void SetMaxWidth(int MaxWidth)
	{
		mMaxWidth = MaxWidth;
	}

	bool IsBillOfMaterials() const
	{
		return mBillOfMaterials;
	}

	void Update();

protected:
	QVariant itemChange(GraphicsItemChange Change, const QVariant& Value) override;

	swInstructions* mInstructions = nullptr;
	swModel* mModel = nullptr;
	swStep mStep = 1;
	bool mBillOfMaterials = false;
	int mMaxWidth = 300;
};

class swInstructionsCoverTextItem : public swInstructionsPaddedItem<QGraphicsTextItem>
{
public:
	swInstructionsCoverTextItem(QGraphicsItem* Parent, swInstructions* Instructions, swModel* Model, bool Author);

	swModel* GetModel() const
	{
		return mModel;
	}

	bool IsAuthor() const
	{
		return mAuthor;
	}

protected:
	QVariant itemChange(GraphicsItemChange Change, const QVariant& Value) override;

	swInstructions* mInstructions = nullptr;
	swModel* mModel = nullptr;
	bool mAuthor = false;
};

class swInstructionsPageWidget : public QGraphicsView
{
	Q_OBJECT

public:
	swInstructionsPageWidget(QWidget* Parent, swInstructions* Instructions, swInstructionsPropertiesWidget* PropertiesWidget, swInstructionsPlacementWidget* PlacementWidget);

	void SetCurrentPage(const swInstructionsPage* Page);

protected:
	void resizeEvent(QResizeEvent* Event) override;
	void mousePressEvent(QMouseEvent* Event) override;
	void mouseReleaseEvent(QMouseEvent* Event) override;
	void drawForeground(QPainter* Painter, const QRectF& Rect) override;
	void FitPage();

protected slots:
	void StepSettingsChanged(swModel* Model, swStep Step);
	void SelectionChanged();

protected:
	swInstructions* mInstructions;
	swInstructionsPropertiesWidget* mPropertiesWidget;
	swInstructionsPlacementWidget* mPlacementWidget;
	const swInstructionsPage* mPage = nullptr;
	std::vector<std::pair<QGraphicsItem*, QPointF>> mDragItems;
};

class swInstructionsPageListWidget : public QDockWidget
{
	Q_OBJECT

public:
	swInstructionsPageListWidget(QWidget* Parent, swInstructions* Instructions);

	QListWidget* mThumbnailsWidget = nullptr;

protected:
	void CustomMenuRequested(const QPoint& Position);

	swInstructions* mInstructions;
};

// use to show the units of a length beside its input, pressing it overrides the page units for that input
class swInstructionsUnitButton : public QToolButton
{
	Q_OBJECT

public:
	swInstructionsUnitButton(QWidget* Parent, const QString& Name);

	swInstructionsPageUnits GetUnits(const swInstructions* Instructions) const;
	void UpdateUnits(const swInstructions* Instructions);

signals:
	void UnitsChanged();

protected:
	void ShowUnitsMenu();

	std::optional<swInstructionsPageUnits> mUnits;
};

class swInstructionsPropertiesWidget : public QDockWidget
{
	Q_OBJECT

public:
	swInstructionsPropertiesWidget(QWidget* Parent, swInstructions* Instructions);

	void AddOptionsAction(QAction* Action);
	void SelectionChanged(QGraphicsItem* FocusItem);
	void SetCurrentPage(int PageIndex);

protected:
	enum class CategoryIndex
	{
		Document,
		Cover,
		BillOfMaterials,
		PageLayout,
		Page,
		Step,
		PiecesList,
		PageNumber,
		Count
	};

	enum class EditorType
	{
		Bool,
		Length,
		Choice,
		Color,
		Font
	};

	enum class FocusType
	{
		None,
		StepImage,
		StepNumber,
		PiecesList,
		PageNumber
	};

	struct CategoryWidgets
	{
		swCollapsibleWidgetButton* Button = nullptr;
		QWidget* Rows = nullptr;
	};

	struct EditorWidgets
	{
		QWidget* Widget = nullptr;
		QLabel* Label = nullptr;
		swInstructionsUnitButton* UnitButton = nullptr;
		QDoubleSpinBox* SizeSpinBox = nullptr;
		EditorType Type = EditorType::Bool;
		QString Name;
		int Minimum = 0;
		int Maximum = 0;
	};

	void ShowPageSetupDialog();
	void CreateWidgets();
	void AddCategory(CategoryIndex Index, const QString& Title, const QString& WhatsThis);
	QWidget* AddRow(CategoryIndex Index, const std::vector<QWidget*>& Editors);
	QLabel* AddLabeledRow(CategoryIndex Index, const QString& Name, QWidget* Editor);
	QLabel* AddEditorRow(CategoryIndex Index, swInstructionsPropertyType Type);
	void RegisterEditor(swInstructionsPropertyType Type, EditorType Editor, QWidget* Widget, const QString& Name, const QString& ToolTip);
	QToolButton* CreateToggleEditor(swInstructionsPropertyType Type, const QString& Name, const QString& ToolTip);
	QDoubleSpinBox* CreateLengthEditor(swInstructionsPropertyType Type, const QString& Name, const QString& ToolTip, int Minimum, int Maximum);
	QComboBox* CreateChoiceEditor(swInstructionsPropertyType Type, const QString& Name, const QString& ToolTip, const QStringList& Choices);
	QToolButton* CreateColorEditor(swInstructionsPropertyType Type, const QString& Name, const QString& ToolTip);
	QToolButton* CreateFontEditor(swInstructionsPropertyType Type, const QString& Name, const QString& ToolTip);
	void ColorButtonClicked(swInstructionsPropertyType Type);
	void FontButtonClicked(swInstructionsPropertyType Type);
	void SetCategoriesExpanded(bool Expanded);
	int GetIntValue(swInstructionsPropertyType Type) const;
	swInstructionsPageUnits GetEditorUnits(swInstructionsPropertyType Type) const;
	QColor GetColorValue(swInstructionsPropertyType Type) const;
	void SetIntValue(swInstructionsPropertyType Type, int Value);
	void SetColorValue(swInstructionsPropertyType Type, const QColor& Color);
	void SetThumbnailPageScope(bool PageOnly);
	void DividerPresetClicked(int Preset);

	void UpdateWidget();
	void UpdateEditors();
	void UpdatePageLayout();
	void UpdateCategories();
	swInstructionsPageType GetPageType() const;

	static QIcon CreateColorIcon(const QColor& Color, int Size);
	static QIcon CreateDividerPresetIcon(int Rows, int Columns, const QPalette& Palette);

	swInstructions* mInstructions = nullptr;
	QWidget* mOptionsRow = nullptr;
	FocusType mFocus = FocusType::None;
	int mPageIndex = 0;
	swModel* mModel = nullptr;
	swStep mStep = 1;

	std::array<CategoryWidgets, static_cast<int>(CategoryIndex::Count)> mCategoryWidgets = {};
	std::array<EditorWidgets, static_cast<int>(swInstructionsPropertyType::Count)> mEditors;
	QSpinBox* mRowsSpinBox = nullptr;
	QSpinBox* mColumnsSpinBox = nullptr;
	QToolButton* mThumbnailPageButton = nullptr;
	QToolButton* mBackgroundPageButton = nullptr;
	std::vector<QToolButton*> mDividerPresetButtons;
	std::vector<QLabel*> mRowLabels;
	QVBoxLayout* mCategoriesLayout = nullptr;
};

class swInstructionsPlacementWidget : public QDockWidget
{
	Q_OBJECT

public:
	swInstructionsPlacementWidget(QWidget* Parent, swInstructions* Instructions);

	void SelectionChanged(QGraphicsItem* FocusItem);

protected:
	void AnchorChanged(int Index);
	void PlacementClicked(int Placement);
	void ResetPositionClicked();
	void LockClicked(bool Locked);
	void UpdateWidget();
	swInstructionsPropertyType GetPlacementType() const;
	swInstructionsPropertyType GetAnchorType() const;
	bool IsInsideAnchor() const;

	static QIcon CreatePlacementIcon(int Placement, const QPalette& Palette);

	swInstructions* mInstructions = nullptr;
	swInstructionsPlacementElement mElement = swInstructionsPlacementElement::None;
	swInstructionsElementKey mElementKey = { swInstructionsPlacementElement::None, nullptr, 0 };
	QPointF mElementPosition;
	QPointF mPlacementPosition;

	QLabel* mElementLabel = nullptr;
	QToolButton* mResetPositionButton = nullptr;
	QToolButton* mLockButton = nullptr;
	QComboBox* mAnchorComboBox = nullptr;
	QWidget* mGridWidget = nullptr;
	std::array<QToolButton*, SW_PLACEMENT_COUNT> mPlacementButtons = {};
};

class swInstructionsGridWidget : public QDockWidget
{
	Q_OBJECT

public:
	swInstructionsGridWidget(QWidget* Parent, swInstructions* Instructions);

protected:
	void UpdateGrid();
	void GridSizeChanged(double Size);

	swInstructions* mInstructions = nullptr;

	QToolButton* mShowGridButton = nullptr;
	QToolButton* mSnapToGridButton = nullptr;
	QDoubleSpinBox* mGridSizeSpinBox = nullptr;
	swInstructionsUnitButton* mGridSizeUnitButton = nullptr;
	QComboBox* mGridScopeComboBox = nullptr;
};

// the print panel is the only print dialog of the application, the pages beside it are its preview
class swInstructionsPrintWidget : public QDockWidget
{
	Q_OBJECT

public:
	swInstructionsPrintWidget(QWidget* Parent, swInstructions* Instructions);

protected:
	void UpdatePrinters();
	void UpdatePages();
	void UpdateFileRow();
	bool BrowseFile();
	void Print();

	swInstructions* mInstructions = nullptr;

	QComboBox* mPrinterComboBox = nullptr;
	QWidget* mFileRow = nullptr;
	QLineEdit* mFileEdit = nullptr;
	QSpinBox* mFromSpinBox = nullptr;
	QSpinBox* mToSpinBox = nullptr;
	QSpinBox* mCopiesSpinBox = nullptr;
	QToolButton* mCollateButton = nullptr;
};

class swInstructionsDialog : public QMainWindow
{
	Q_OBJECT

public:
	swInstructionsDialog(QWidget* Parent, Project* Project);

public slots:
	void ModelChanged();
	void ShowPrintWidget();

protected slots:
	void PageSetupChanged();
	void CurrentThumbnailChanged(int Index);

protected:
	bool eventFilter(QObject* Object, QEvent* Event) override;
	void UpdatePageList();
	void SnapPageListDock();

	Project* mProject = nullptr;
	bool mUpdatePending = false;
	bool mPageListSnapPending = false;

	int mCurrentPageNumber;
	swInstructions* mInstructions;

	swInstructionsPageWidget* mPageWidget = nullptr;
	swInstructionsPageListWidget* mPageListWidget = nullptr;
	swInstructionsPropertiesWidget* mPropertiesWidget = nullptr;
	swInstructionsPlacementWidget* mPlacementWidget = nullptr;
	swInstructionsGridWidget* mGridWidget = nullptr;
	swInstructionsPrintWidget* mPrintWidget = nullptr;
};
