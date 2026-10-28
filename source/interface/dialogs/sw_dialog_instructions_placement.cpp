#include "sw_global.h"
#include "sw_dialog_instructions_placement.h"
#include "sw_dialog_instructions_page.h"
#include "sw_dialog_instructions_properties.h"
#include "sw_widget_collapsible.h"
#include "sw_window_utilities.h"

// the names follow the placement grid row by row, the inner cells lie inside the anchor
static const char* const swPlacementNames[SW_PLACEMENT_COUNT] =
{
	QT_TRANSLATE_NOOP("swInstructionsPlacementWidget", "Above, Outside Left"),
	QT_TRANSLATE_NOOP("swInstructionsPlacementWidget", "Above, Left Aligned"),
	QT_TRANSLATE_NOOP("swInstructionsPlacementWidget", "Above, Centered"),
	QT_TRANSLATE_NOOP("swInstructionsPlacementWidget", "Above, Right Aligned"),
	QT_TRANSLATE_NOOP("swInstructionsPlacementWidget", "Above, Outside Right"),
	QT_TRANSLATE_NOOP("swInstructionsPlacementWidget", "Left, Top Aligned"),
	QT_TRANSLATE_NOOP("swInstructionsPlacementWidget", "Top Left"),
	QT_TRANSLATE_NOOP("swInstructionsPlacementWidget", "Top"),
	QT_TRANSLATE_NOOP("swInstructionsPlacementWidget", "Top Right"),
	QT_TRANSLATE_NOOP("swInstructionsPlacementWidget", "Right, Top Aligned"),
	QT_TRANSLATE_NOOP("swInstructionsPlacementWidget", "Left, Centered"),
	QT_TRANSLATE_NOOP("swInstructionsPlacementWidget", "Left"),
	QT_TRANSLATE_NOOP("swInstructionsPlacementWidget", "Center"),
	QT_TRANSLATE_NOOP("swInstructionsPlacementWidget", "Right"),
	QT_TRANSLATE_NOOP("swInstructionsPlacementWidget", "Right, Centered"),
	QT_TRANSLATE_NOOP("swInstructionsPlacementWidget", "Left, Bottom Aligned"),
	QT_TRANSLATE_NOOP("swInstructionsPlacementWidget", "Bottom Left"),
	QT_TRANSLATE_NOOP("swInstructionsPlacementWidget", "Bottom"),
	QT_TRANSLATE_NOOP("swInstructionsPlacementWidget", "Bottom Right"),
	QT_TRANSLATE_NOOP("swInstructionsPlacementWidget", "Right, Bottom Aligned"),
	QT_TRANSLATE_NOOP("swInstructionsPlacementWidget", "Below, Outside Left"),
	QT_TRANSLATE_NOOP("swInstructionsPlacementWidget", "Below, Left Aligned"),
	QT_TRANSLATE_NOOP("swInstructionsPlacementWidget", "Below, Centered"),
	QT_TRANSLATE_NOOP("swInstructionsPlacementWidget", "Below, Right Aligned"),
	QT_TRANSLATE_NOOP("swInstructionsPlacementWidget", "Below, Outside Right")
};

swInstructionsPlacementWidget::swInstructionsPlacementWidget(QWidget* Parent, swInstructions* Instructions)
	: QDockWidget(Parent), mInstructions(Instructions)
{
	setWindowTitle(tr("Placement"));
	setWhatsThis(tr("<p><b>Placement</b></p><p>Select an element on the page to place, move or lock it. Place the selected page element on a predefined area: the step model, step number, parts list, cover model, cover title, cover author or bill of materials. Step elements are placed in the part of the page their step gets from the split, or outside the other step element. Placements apply to all pages.</p><p>Every element can also be dragged on the page, it keeps its distance from its placement. A locked element keeps its position whatever its placement and is not dragged.</p>"));
	swSetupWidgetHelp(this, nullptr);

	QWidget* CentralWidget = new QWidget(this);
	setWidget(CentralWidget);

	QVBoxLayout* Layout = new QVBoxLayout(CentralWidget);

	mElementLabel = new QLabel(CentralWidget);
	mElementLabel->setWordWrap(true);
	Layout->addWidget(mElementLabel);

	QLabel* AnchorLabel = new QLabel(tr("Relative To"), CentralWidget);
	Layout->addWidget(AnchorLabel);

	// the anchor buttons are stacked on the left, the placement grid takes the right side level with the first button
	QHBoxLayout* PlacementLayout = new QHBoxLayout();
	Layout->addLayout(PlacementLayout);

	QVBoxLayout* ButtonsLayout = new QVBoxLayout();
	ButtonsLayout->setSpacing(CentralWidget->style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing));
	PlacementLayout->addLayout(ButtonsLayout, 1);

	// one button per anchor, the selections are anchors while several elements are selected, the others are disabled
	const std::pair<int, QString> AnchorNames[] =
	{
		{ static_cast<int>(swInstructionsPlacementAnchor::Area), tr("Page Area") },
		{ static_cast<int>(swInstructionsPlacementAnchor::StepNumber), tr("Step Number") },
		{ static_cast<int>(swInstructionsPlacementAnchor::PartsList), tr("Parts List") },
		{ SW_ANCHOR_FIRST_SELECTION, tr("First Selection") },
		{ SW_ANCHOR_LAST_SELECTION, tr("Last Selection") }
	};

	mAnchorGroup = new QButtonGroup(this);
	mAnchorGroup->setExclusive(true);

	for (const std::pair<int, QString>& AnchorName : AnchorNames)
	{
		QToolButton* Button = new QToolButton(CentralWidget);
		Button->setText(AnchorName.second);
		Button->setCheckable(true);
		Button->setToolButtonStyle(Qt::ToolButtonTextOnly);
		Button->setToolTip(tr("Place the element against the %1.").arg(AnchorName.second.toLower()));
		Button->setWhatsThis(Button->toolTip());
		Button->setAccessibleName(AnchorName.second);
		swSetupPanelButton(Button);
		Button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		ButtonsLayout->addWidget(Button);
		mAnchorGroup->addButton(Button, AnchorName.first);
	}

	AnchorLabel->setBuddy(mAnchorGroup->button(0));

	connect(mAnchorGroup, &QButtonGroup::idClicked, this, &swInstructionsPlacementWidget::AnchorClicked);

	ButtonsLayout->addStretch(1);

	QWidget* ResetRow = swCreatePanelRow(CentralWidget);

	mResetPositionButton = new QToolButton(ResetRow);
	mResetPositionButton->setText(tr("Reset Position"));
	mResetPositionButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
	mResetPositionButton->setToolTip(tr("Move the dragged element back onto its placement."));
	mResetPositionButton->setWhatsThis(mResetPositionButton->toolTip());
	mResetPositionButton->setAccessibleName(mResetPositionButton->text());
	swSetupPanelButton(mResetPositionButton);
	mResetPositionButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	static_cast<QHBoxLayout*>(ResetRow->layout())->addWidget(mResetPositionButton, 1);

	mLockButton = new QToolButton(ResetRow);
	mLockButton->setText(tr("Lock Position"));
	mLockButton->setCheckable(true);
	mLockButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
	mLockButton->setToolTip(tr("Keep the element where it is, placements and dragging no longer move it."));
	mLockButton->setWhatsThis(mLockButton->toolTip());
	mLockButton->setAccessibleName(mLockButton->text());
	swSetupPanelButton(mLockButton);
	mLockButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	static_cast<QHBoxLayout*>(ResetRow->layout())->addWidget(mLockButton, 1);

	// the grid shows the anchor in its middle, its inner buttons place inside it and its outer ring outside
	mGridWidget = new QWidget(CentralWidget);
	QGridLayout* GridLayout = new QGridLayout(mGridWidget);
	GridLayout->setContentsMargins(0, 0, 0, 0);
	PlacementLayout->addWidget(mGridWidget, 0, Qt::AlignRight | Qt::AlignTop);

	for (int Placement = 0; Placement < SW_PLACEMENT_COUNT; Placement++)
	{
		const QString Name = tr(swPlacementNames[Placement]);

		QToolButton* Button = new QToolButton(mGridWidget);
		Button->setIcon(CreatePlacementIcon(Placement, palette()));
		Button->setCheckable(true);
		Button->setToolTip(tr("Place the element: %1.").arg(Name));
		Button->setWhatsThis(Button->toolTip());
		Button->setAccessibleName(Name);
		swSetupPanelButton(Button);
		GridLayout->addWidget(Button, Placement / SW_PLACEMENT_GRID_SIZE, Placement % SW_PLACEMENT_GRID_SIZE);
		mPlacementButtons[Placement] = Button;

		connect(Button, &QToolButton::clicked, this, [this, Placement]()
		{
			PlacementClicked(Placement);
		});
	}

	// the reset button is as wide as the anchor column above it, the lock button as wide as the placement grid
	PlacementLayout->setSpacing(ResetRow->layout()->spacing());
	mLockButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	mLockButton->setFixedWidth(mGridWidget->sizeHint().width());
	static_cast<QHBoxLayout*>(ResetRow->layout())->setStretch(1, 0);

	Layout->addWidget(ResetRow);

	connect(mResetPositionButton, &QToolButton::clicked, this, &swInstructionsPlacementWidget::ResetPositionClicked);
	connect(mLockButton, &QToolButton::toggled, this, &swInstructionsPlacementWidget::LockClicked);


	// the panel is exactly as tall as its rows wherever it is docked
	swFitHeightToContents(CentralWidget);

	connect(mInstructions, &swInstructions::PageLayoutChanged, this, &swInstructionsPlacementWidget::UpdateWidget);

	UpdateWidget();
}

QIcon swInstructionsPlacementWidget::CreatePlacementIcon(int Placement, const QPalette& Palette)
{
	// the outline is the anchor, the block shows where the element goes
	const int Size = 32;
	const int Positions[SW_PLACEMENT_GRID_SIZE] = { 1, 9, 13, 17, 25 };
	const int BlockSize = 6;

	QPixmap Pixmap(Size, Size);
	Pixmap.fill(Qt::transparent);

	QPainter Painter(&Pixmap);
	QColor OutlineColor = Palette.color(QPalette::WindowText);
	OutlineColor.setAlpha(110);

	Painter.setPen(QPen(OutlineColor, 2.0));
	Painter.drawRect(QRectF(8.0, 8.0, 16.0, 16.0));
	Painter.fillRect(Positions[Placement % SW_PLACEMENT_GRID_SIZE], Positions[Placement / SW_PLACEMENT_GRID_SIZE], BlockSize, BlockSize, Palette.color(QPalette::WindowText));
	Painter.end();

	return QIcon(Pixmap);
}

void swInstructionsPlacementWidget::SelectionChanged(QGraphicsItem* FocusItem, int SelectionCount)
{
	// several elements are placed against one of them instead of their own placements
	const bool SelectionModeChanged = (SelectionCount > 1) != (mSelectionCount > 1) || (SelectionCount > 1 && SelectionCount != mSelectionCount);
	mSelectionCount = SelectionCount;

	swInstructionsElementKey Key;
	swGetElementKey(FocusItem, Key);

	// the positions of the item are kept to lock or unlock it in place, page rebuilds delete the items
	if (FocusItem)
	{
		mElementPosition = FocusItem->pos();
		mPlacementPosition = FocusItem->data(SW_PLACEMENT_POSITION_DATA).toPointF();
	}

	if (Key == mElementKey && !SelectionModeChanged)
		return;

	mElementKey = Key;
	mElement = std::get<0>(Key);
	UpdateWidget();
}

swInstructionsPropertyType swInstructionsPlacementWidget::GetPlacementType() const
{
	switch (mElement)
	{
	case swInstructionsPlacementElement::StepNumber:
		return swInstructionsPropertyType::StepNumberPlacement;

	case swInstructionsPlacementElement::PartsList:
		return swInstructionsPropertyType::PLIPlacement;

	case swInstructionsPlacementElement::CoverTitle:
		return swInstructionsPropertyType::CoverTitlePlacement;

	case swInstructionsPlacementElement::CoverAuthor:
		return swInstructionsPropertyType::CoverAuthorPlacement;

	case swInstructionsPlacementElement::BillOfMaterials:
		return swInstructionsPropertyType::BillOfMaterialsPlacement;

	case swInstructionsPlacementElement::StepImage:
		return swInstructionsPropertyType::StepImagePlacement;

	case swInstructionsPlacementElement::CoverImage:
		return swInstructionsPropertyType::CoverImagePlacement;

	case swInstructionsPlacementElement::PageNumber:
		return swInstructionsPropertyType::PageNumberPlacement;

	case swInstructionsPlacementElement::Notice:
	case swInstructionsPlacementElement::Pointer:
	case swInstructionsPlacementElement::None:
		break;
	}

	return swInstructionsPropertyType::Count;
}

swInstructionsPropertyType swInstructionsPlacementWidget::GetAnchorType() const
{
	switch (mElement)
	{
	case swInstructionsPlacementElement::StepNumber:
		return swInstructionsPropertyType::StepNumberAnchor;

	case swInstructionsPlacementElement::PartsList:
		return swInstructionsPropertyType::PLIAnchor;

	case swInstructionsPlacementElement::CoverTitle:
	case swInstructionsPlacementElement::CoverAuthor:
	case swInstructionsPlacementElement::BillOfMaterials:
	case swInstructionsPlacementElement::StepImage:
	case swInstructionsPlacementElement::CoverImage:
	case swInstructionsPlacementElement::PageNumber:
	case swInstructionsPlacementElement::Notice:
	case swInstructionsPlacementElement::Pointer:
	case swInstructionsPlacementElement::None:
		break;
	}

	return swInstructionsPropertyType::Count;
}

bool swInstructionsPlacementWidget::IsInsideAnchor() const
{
	const swInstructionsPropertyType AnchorType = GetAnchorType();

	if (AnchorType == swInstructionsPropertyType::Count)
		return true;

	return static_cast<swInstructionsPlacementAnchor>(mInstructions->GetIntProperty(AnchorType, nullptr, 0)) == swInstructionsPlacementAnchor::Area;
}

void swInstructionsPlacementWidget::AnchorClicked(int Anchor)
{
	if (mSelectionCount > 1)
	{
		mLastSelection = Anchor == SW_ANCHOR_LAST_SELECTION;
		return;
	}

	const swInstructionsPropertyType AnchorType = GetAnchorType();

	if (AnchorType == swInstructionsPropertyType::Count)
		return;

	// the placement moves inside the area or outside the element so it stays valid for the new anchor
	const swInstructionsPropertyType PlacementType = GetPlacementType();
	const int Placement = swGetValidPlacement(mInstructions->GetIntProperty(PlacementType, nullptr, 0), static_cast<swInstructionsPlacementAnchor>(Anchor) == swInstructionsPlacementAnchor::Area);

	mInstructions->SetDefaultInt(PlacementType, Placement);
	mInstructions->SetDefaultInt(AnchorType, Anchor);
}

void swInstructionsPlacementWidget::PlacementClicked(int Placement)
{
	if (mSelectionCount > 1)
	{
		emit AlignSelection(Placement, mLastSelection);
		UpdateWidget();
		return;
	}

	const swInstructionsPropertyType PlacementType = GetPlacementType();

	// the element leaves the position it was dragged to
	if (PlacementType != swInstructionsPropertyType::Count)
		mInstructions->SetElementPlacement(PlacementType, Placement, mElementKey);

	// clicking the current placement changes nothing but unchecks its button
	UpdateWidget();
}

void swInstructionsPlacementWidget::ResetPositionClicked()
{
	mInstructions->ResetElementOffset(mElementKey);
}

void swInstructionsPlacementWidget::LockClicked(bool Locked)
{
	if (Locked)
		mInstructions->LockElement(mElementKey, mElementPosition);
	else
		mInstructions->UnlockElement(mElementKey, mElementPosition - mPlacementPosition);
}

void swInstructionsPlacementWidget::UpdateWidget()
{
	// the other selected elements are placed against the first or the last one selected
	if (mSelectionCount > 1)
	{
		mElementLabel->setText(tr("%1 Elements Selected").arg(mSelectionCount));
		mResetPositionButton->setEnabled(false);

		{
			QSignalBlocker Blocker(mLockButton);
			mLockButton->setEnabled(false);
			mLockButton->setChecked(false);
		}

		for (QAbstractButton* Button : mAnchorGroup->buttons())
			Button->setEnabled(mAnchorGroup->id(Button) >= SW_ANCHOR_FIRST_SELECTION);

		mAnchorGroup->button(mLastSelection ? SW_ANCHOR_LAST_SELECTION : SW_ANCHOR_FIRST_SELECTION)->setChecked(true);

		for (QToolButton* Button : mPlacementButtons)
		{
			Button->setEnabled(true);
			Button->setChecked(false);
		}

		return;
	}

	// a locked element keeps its position, it is neither placed nor dragged
	const bool Locked = mInstructions->IsElementLocked(mElementKey);
	const bool Active = GetPlacementType() != swInstructionsPropertyType::Count && !Locked;

	// a notice has no placement but returns to where it was added, a pointer follows its elements
	const bool Movable = Active || (mElement == swInstructionsPlacementElement::Notice && !Locked);

	mResetPositionButton->setEnabled(Movable && mInstructions->HasElementOffset(mElementKey));

	{
		QSignalBlocker Blocker(mLockButton);
		mLockButton->setEnabled(mElement != swInstructionsPlacementElement::None && mElement != swInstructionsPlacementElement::Pointer);
		mLockButton->setChecked(Locked);
	}

	switch (mElement)
	{
	case swInstructionsPlacementElement::StepNumber:
		mElementLabel->setText(tr("Step Number"));
		break;

	case swInstructionsPlacementElement::PartsList:
		mElementLabel->setText(tr("Parts List"));
		break;

	case swInstructionsPlacementElement::CoverTitle:
		mElementLabel->setText(tr("Cover Title"));
		break;

	case swInstructionsPlacementElement::CoverAuthor:
		mElementLabel->setText(tr("Cover Author"));
		break;

	case swInstructionsPlacementElement::BillOfMaterials:
		mElementLabel->setText(tr("Bill of Materials"));
		break;

	case swInstructionsPlacementElement::StepImage:
		mElementLabel->setText(tr("Step Model"));
		break;

	case swInstructionsPlacementElement::CoverImage:
		mElementLabel->setText(tr("Cover Model"));
		break;

	case swInstructionsPlacementElement::PageNumber:
		mElementLabel->setText(tr("Page Number"));
		break;

	case swInstructionsPlacementElement::Notice:
		mElementLabel->setText(tr("Notice"));
		break;

	case swInstructionsPlacementElement::Pointer:
		mElementLabel->setText(tr("Pointer"));
		break;

	case swInstructionsPlacementElement::None:
		mElementLabel->setText(tr("No Element Selected"));
		break;
	}

	{
		// the page area and the other step element are the anchors of a single element
		const swInstructionsPropertyType AnchorType = GetAnchorType();
		int Anchor = static_cast<int>(swInstructionsPlacementAnchor::Area);

		if (AnchorType != swInstructionsPropertyType::Count)
			Anchor = mInstructions->GetIntProperty(AnchorType, nullptr, 0);

		const int OtherAnchor = static_cast<int>(mElement == swInstructionsPlacementElement::StepNumber ? swInstructionsPlacementAnchor::PartsList : swInstructionsPlacementAnchor::StepNumber);
		const bool StepElement = mElement == swInstructionsPlacementElement::StepNumber || mElement == swInstructionsPlacementElement::PartsList;

		for (QAbstractButton* Button : mAnchorGroup->buttons())
		{
			const int ButtonAnchor = mAnchorGroup->id(Button);
			Button->setEnabled(Active && StepElement && (ButtonAnchor == static_cast<int>(swInstructionsPlacementAnchor::Area) || ButtonAnchor == OtherAnchor));
		}

		if (QAbstractButton* Button = mAnchorGroup->button(Anchor))
			Button->setChecked(true);
	}

	const bool Inside = IsInsideAnchor();
	const swInstructionsPropertyType PlacementType = GetPlacementType();
	int CurrentPlacement = -1;

	if (Active)
		CurrentPlacement = swGetValidPlacement(mInstructions->GetIntProperty(PlacementType, nullptr, 0), Inside);

	// an area takes the inner placements, an element only the outer ring
	for (int Placement = 0; Placement < SW_PLACEMENT_COUNT; Placement++)
	{
		QToolButton* Button = mPlacementButtons[Placement];

		Button->setEnabled(Active && swIsInsidePlacement(Placement) == Inside);
		Button->setChecked(Active && Placement == CurrentPlacement);
	}
}

swInstructionsGridWidget::swInstructionsGridWidget(QWidget* Parent, swInstructions* Instructions)
	: QDockWidget(Parent), mInstructions(Instructions)
{
	setWindowTitle(tr("Grid"));
	setWhatsThis(tr("<p><b>Grid</b></p><p>The page grid helps aligning dragged elements, it is never printed. Show it on screen, snap dragged elements to it, choose its area, the size of its main unit and the fractions it is split into, and the colors of its lines and of the margins.</p>"));
	swSetupWidgetHelp(this, nullptr);

	QWidget* CentralWidget = new QWidget(this);
	setWidget(CentralWidget);

	QVBoxLayout* Layout = new QVBoxLayout(CentralWidget);

	// the grid settings apply to every element and need no selection
	QWidget* GridRow = swCreatePanelRow(CentralWidget);
	QHBoxLayout* GridRowLayout = static_cast<QHBoxLayout*>(GridRow->layout());
	Layout->addWidget(GridRow);

	auto CreateGridButton = [GridRow, GridRowLayout](const QString& Name, const QString& ToolTip)
	{
		QToolButton* Button = new QToolButton(GridRow);
		Button->setText(Name);
		Button->setCheckable(true);
		Button->setToolButtonStyle(Qt::ToolButtonTextOnly);
		Button->setToolTip(ToolTip);
		Button->setWhatsThis(ToolTip);
		Button->setAccessibleName(Name);
		swSetupPanelButton(Button);
		Button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		GridRowLayout->addWidget(Button, 1);

		return Button;
	};

	mShowGridButton = CreateGridButton(tr("Show Grid"), tr("Draw the page grid on screen, it is never printed."));
	mSnapToGridButton = CreateGridButton(tr("Snap to Grid"), tr("Snap dragged elements to the page grid."));

	// the grid area and size rows follow the toggles, each name beside its input
	QWidget* GridScopeRow = swCreatePanelRow(CentralWidget);
	QHBoxLayout* GridScopeRowLayout = static_cast<QHBoxLayout*>(GridScopeRow->layout());
	Layout->addWidget(GridScopeRow);

	QLabel* GridScopeLabel = new QLabel(tr("Grid Area"), GridScopeRow);
	GridScopeRowLayout->addWidget(GridScopeLabel);

	mGridScopeComboBox = new QComboBox(GridScopeRow);
	mGridScopeComboBox->addItem(tr("Within Margins"));
	mGridScopeComboBox->addItem(tr("Full Page"));
	mGridScopeComboBox->setToolTip(tr("Choose whether the grid covers the area inside the margins or the full page."));
	mGridScopeComboBox->setWhatsThis(mGridScopeComboBox->toolTip());
	mGridScopeComboBox->setAccessibleName(tr("Grid Area"));
	swSetupPanelEdit(mGridScopeComboBox);
	mGridScopeComboBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	GridScopeRowLayout->addWidget(mGridScopeComboBox, 1);
	GridScopeLabel->setBuddy(mGridScopeComboBox);

	QWidget* GridSizeRow = swCreatePanelRow(CentralWidget);
	QHBoxLayout* GridSizeRowLayout = static_cast<QHBoxLayout*>(GridSizeRow->layout());
	Layout->addWidget(GridSizeRow);

	QLabel* GridSizeLabel = new QLabel(tr("Grid Size"), GridSizeRow);
	GridSizeRowLayout->addWidget(GridSizeLabel);

	mGridSizeUnitButton = new swInstructionsUnitButton(GridSizeRow, tr("Grid Size"));
	GridSizeRowLayout->addWidget(mGridSizeUnitButton);

	mGridSizeSpinBox = new swInstructionsLengthSpinBox(GridSizeRow);
	mGridSizeSpinBox->setToolTip(tr("Main unit of the grid drawn with the primary color, in the margin units of the page setup."));
	mGridSizeSpinBox->setWhatsThis(mGridSizeSpinBox->toolTip());
	mGridSizeSpinBox->setAccessibleName(tr("Grid Size"));
	swSetupPanelSpinBox(mGridSizeSpinBox);
	mGridSizeSpinBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	GridSizeRowLayout->addWidget(mGridSizeSpinBox, 1);
	GridSizeLabel->setBuddy(mGridSizeSpinBox);

	QWidget* GridDivisionsRow = swCreatePanelRow(CentralWidget);
	QHBoxLayout* GridDivisionsRowLayout = static_cast<QHBoxLayout*>(GridDivisionsRow->layout());
	Layout->addWidget(GridDivisionsRow);

	QLabel* GridDivisionsLabel = new QLabel(tr("Divisions"), GridDivisionsRow);
	GridDivisionsRowLayout->addWidget(GridDivisionsLabel);

	mGridDivisionsSpinBox = new swRangeSpinBox(GridDivisionsRow);
	mGridDivisionsSpinBox->setRange(1, SW_GRID_DIVISIONS_MAXIMUM);
	mGridDivisionsSpinBox->setPrefix(QLatin1String("1/"));
	mGridDivisionsSpinBox->setToolTip(tr("Split each grid unit into fractions drawn with the secondary color, dragged elements snap to the fractions."));
	mGridDivisionsSpinBox->setWhatsThis(mGridDivisionsSpinBox->toolTip());
	mGridDivisionsSpinBox->setAccessibleName(tr("Divisions"));
	swSetupPanelSpinBox(mGridDivisionsSpinBox);
	mGridDivisionsSpinBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	GridDivisionsRowLayout->addWidget(mGridDivisionsSpinBox, 1);
	GridDivisionsLabel->setBuddy(mGridDivisionsSpinBox);

	// the grid and margin lines each take a color with its transparency
	auto CreateColorRow = [CentralWidget, Layout](const QString& Name, const QString& ToolTip, QLabel*& Label)
	{
		QWidget* ColorRow = swCreatePanelRow(CentralWidget);
		QHBoxLayout* ColorRowLayout = static_cast<QHBoxLayout*>(ColorRow->layout());
		Layout->addWidget(ColorRow);

		Label = new QLabel(Name, ColorRow);
		ColorRowLayout->addWidget(Label);

		QToolButton* Button = new QToolButton(ColorRow);
		Button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
		Button->setToolTip(ToolTip);
		Button->setWhatsThis(ToolTip);
		Button->setAccessibleName(Name);
		swSetupPanelButton(Button);
		Button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		ColorRowLayout->addWidget(Button, 1);
		Label->setBuddy(Button);

		return Button;
	};

	QLabel* PrimaryColorLabel = nullptr;
	QLabel* SecondaryColorLabel = nullptr;
	QLabel* MarginColorLabel = nullptr;
	mPrimaryColorButton = CreateColorRow(tr("Primary Color"), tr("Color of the grid lines at every grid unit."), PrimaryColorLabel);
	mSecondaryColorButton = CreateColorRow(tr("Secondary Color"), tr("Color of the grid lines at the fractions of a grid unit."), SecondaryColorLabel);
	mMarginColorButton = CreateColorRow(tr("Margin Color"), tr("Color of the dashed outline of the page margins."), MarginColorLabel);

	// the labels share one width so the inputs line up
	const QList<QLabel*> Labels = { GridScopeLabel, GridSizeLabel, GridDivisionsLabel, PrimaryColorLabel, SecondaryColorLabel, MarginColorLabel };
	int LabelWidth = 0;

	for (QLabel* Label : Labels)
		LabelWidth = qMax(LabelWidth, Label->sizeHint().width());

	for (QLabel* Label : Labels)
		Label->setFixedWidth(LabelWidth);

	connect(mShowGridButton, &QToolButton::toggled, mInstructions, &swInstructions::SetShowGrid);
	connect(mSnapToGridButton, &QToolButton::toggled, mInstructions, &swInstructions::SetSnapToGrid);
	connect(mGridScopeComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int Index)
	{
		mInstructions->SetGridWithinMargins(Index == 0);
	});
	connect(mGridSizeSpinBox, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &swInstructionsGridWidget::GridSizeChanged);
	connect(mGridSizeUnitButton, &swInstructionsUnitButton::UnitsChanged, this, &swInstructionsGridWidget::UpdateGrid);
	connect(mGridDivisionsSpinBox, qOverload<int>(&QSpinBox::valueChanged), mInstructions, &swInstructions::SetGridDivisions);
	connect(mPrimaryColorButton, &QToolButton::clicked, this, [this]()
	{
		const QColor Color = swPickColor(mPrimaryColorButton, mInstructions->GetGridPrimaryColor());

		if (Color.isValid())
			mInstructions->SetGridPrimaryColor(Color);
	});
	connect(mSecondaryColorButton, &QToolButton::clicked, this, [this]()
	{
		const QColor Color = swPickColor(mSecondaryColorButton, mInstructions->GetGridSecondaryColor());

		if (Color.isValid())
			mInstructions->SetGridSecondaryColor(Color);
	});
	connect(mMarginColorButton, &QToolButton::clicked, this, [this]()
	{
		const QColor Color = swPickColor(mMarginColorButton, mInstructions->GetMarginColor());

		if (Color.isValid())
			mInstructions->SetMarginColor(Color);
	});

	// the panel is exactly as tall as its rows wherever it is docked
	swFitHeightToContents(CentralWidget);

	connect(mInstructions, &swInstructions::GridChanged, this, &swInstructionsGridWidget::UpdateGrid);
	connect(mInstructions, &swInstructions::PageSetupChanged, this, &swInstructionsGridWidget::UpdateGrid);
	connect(mInstructions, &swInstructions::PageLayoutChanged, this, &swInstructionsGridWidget::UpdateGrid);
	connect(mInstructions, &swInstructions::UnitsChanged, this, &swInstructionsGridWidget::UpdateGrid);

	UpdateGrid();
}

void swInstructionsGridWidget::UpdateGrid()
{
	QSignalBlocker ShowBlocker(mShowGridButton);
	QSignalBlocker SnapBlocker(mSnapToGridButton);
	QSignalBlocker SizeBlocker(mGridSizeSpinBox);
	QSignalBlocker ScopeBlocker(mGridScopeComboBox);
	QSignalBlocker DivisionsBlocker(mGridDivisionsSpinBox);

	mShowGridButton->setChecked(mInstructions->IsGridShown());
	mSnapToGridButton->setChecked(mInstructions->IsSnapToGrid());
	mGridScopeComboBox->setCurrentIndex(mInstructions->IsGridWithinMargins() ? 0 : 1);

	const swInstructionsPageUnits Units = mGridSizeUnitButton->GetUnits(mInstructions);

	swSetupLengthSpinBox(mGridSizeSpinBox, Units, SW_GRID_SIZE_MINIMUM, SW_GRID_SIZE_MAXIMUM);
	mGridSizeUnitButton->UpdateUnits(mInstructions);
	mGridSizeSpinBox->setValue(mInstructions->GetGridSize() / swGetPageUnitScale(Units));
	mGridDivisionsSpinBox->setValue(mInstructions->GetGridDivisions());

	swUpdateColorButton(mPrimaryColorButton, mInstructions->GetGridPrimaryColor());
	swUpdateColorButton(mSecondaryColorButton, mInstructions->GetGridSecondaryColor());
	swUpdateColorButton(mMarginColorButton, mInstructions->GetMarginColor());
}

void swInstructionsGridWidget::GridSizeChanged(double Size)
{
	mInstructions->SetGridSize(Size * swGetPageUnitScale(mGridSizeUnitButton->GetUnits(mInstructions)));
}
