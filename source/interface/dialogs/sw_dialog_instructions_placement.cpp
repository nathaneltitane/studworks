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
	setWhatsThis(tr("<p><b>Placement</b></p><p>Select an element on the page to place, move or lock it. Place the selected page element on a predefined area: the step model, step number, pieces list, cover model, cover title, cover author or bill of materials. Step elements are placed in the part of the page their step gets from the split, or outside the other step element. Placements apply to all pages.</p><p>Every element can also be dragged on the page, it keeps its distance from its placement. A locked element keeps its position whatever its placement and is not dragged.</p>"));
	swSetupWidgetHelp(this, nullptr);

	QWidget* CentralWidget = new QWidget(this);
	setWidget(CentralWidget);

	QVBoxLayout* Layout = new QVBoxLayout(CentralWidget);

	mElementLabel = new QLabel(CentralWidget);
	mElementLabel->setWordWrap(true);
	Layout->addWidget(mElementLabel);

	QWidget* AnchorRow = swCreatePanelRow(CentralWidget);
	Layout->addWidget(AnchorRow);

	QLabel* AnchorLabel = new QLabel(tr("Relative To"), AnchorRow);
	AnchorRow->layout()->addWidget(AnchorLabel);

	mAnchorComboBox = new QComboBox(AnchorRow);
	mAnchorComboBox->setToolTip(tr("Choose what the element is placed against."));
	mAnchorComboBox->setWhatsThis(mAnchorComboBox->toolTip());
	mAnchorComboBox->setAccessibleName(tr("Relative To"));
	swSetupPanelEdit(mAnchorComboBox);
	mAnchorComboBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	static_cast<QHBoxLayout*>(AnchorRow->layout())->addWidget(mAnchorComboBox, 1);

	AnchorLabel->setBuddy(mAnchorComboBox);

	connect(mAnchorComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, &swInstructionsPlacementWidget::AnchorChanged);

	// the grid shows the anchor in its middle, its inner buttons place inside it and its outer ring outside
	mGridWidget = new QWidget(CentralWidget);
	QGridLayout* GridLayout = new QGridLayout(mGridWidget);
	GridLayout->setContentsMargins(0, 0, 0, 0);
	Layout->addWidget(mGridWidget, 0, Qt::AlignHCenter);

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

	QWidget* ResetRow = swCreatePanelRow(CentralWidget);
	Layout->addWidget(ResetRow);

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

void swInstructionsPlacementWidget::SelectionChanged(QGraphicsItem* FocusItem)
{
	swInstructionsElementKey Key;
	swGetElementKey(FocusItem, Key);

	// the positions of the item are kept to lock or unlock it in place, page rebuilds delete the items
	if (FocusItem)
	{
		mElementPosition = FocusItem->pos();
		mPlacementPosition = FocusItem->data(SW_PLACEMENT_POSITION_DATA).toPointF();
	}

	if (Key == mElementKey)
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

	case swInstructionsPlacementElement::PiecesList:
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

	case swInstructionsPlacementElement::PiecesList:
		return swInstructionsPropertyType::PLIAnchor;

	case swInstructionsPlacementElement::CoverTitle:
	case swInstructionsPlacementElement::CoverAuthor:
	case swInstructionsPlacementElement::BillOfMaterials:
	case swInstructionsPlacementElement::StepImage:
	case swInstructionsPlacementElement::CoverImage:
	case swInstructionsPlacementElement::PageNumber:
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

void swInstructionsPlacementWidget::AnchorChanged(int Index)
{
	const swInstructionsPropertyType AnchorType = GetAnchorType();

	if (AnchorType == swInstructionsPropertyType::Count || Index < 0)
		return;

	// the placement moves inside the area or outside the element so it stays valid for the new anchor
	const int Anchor = mAnchorComboBox->itemData(Index).toInt();
	const swInstructionsPropertyType PlacementType = GetPlacementType();
	const int Placement = swGetValidPlacement(mInstructions->GetIntProperty(PlacementType, nullptr, 0), static_cast<swInstructionsPlacementAnchor>(Anchor) == swInstructionsPlacementAnchor::Area);

	mInstructions->SetDefaultInt(PlacementType, Placement);
	mInstructions->SetDefaultInt(AnchorType, Anchor);
}

void swInstructionsPlacementWidget::PlacementClicked(int Placement)
{
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
	// a locked element keeps its position, it is neither placed nor dragged
	const bool Locked = mInstructions->IsElementLocked(mElementKey);
	const bool Active = GetPlacementType() != swInstructionsPropertyType::Count && !Locked;

	mResetPositionButton->setEnabled(Active && mInstructions->HasElementOffset(mElementKey));

	{
		QSignalBlocker Blocker(mLockButton);
		mLockButton->setEnabled(mElement != swInstructionsPlacementElement::None);
		mLockButton->setChecked(Locked);
	}

	switch (mElement)
	{
	case swInstructionsPlacementElement::StepNumber:
		mElementLabel->setText(tr("Step Number"));
		break;

	case swInstructionsPlacementElement::PiecesList:
		mElementLabel->setText(tr("Pieces List"));
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

	case swInstructionsPlacementElement::None:
		mElementLabel->setText(tr("No Element Selected"));
		break;
	}

	{
		QSignalBlocker Blocker(mAnchorComboBox);
		mAnchorComboBox->clear();
		mAnchorComboBox->addItem(tr("Page Area"), static_cast<int>(swInstructionsPlacementAnchor::Area));

		if (mElement == swInstructionsPlacementElement::StepNumber)
			mAnchorComboBox->addItem(tr("Pieces List"), static_cast<int>(swInstructionsPlacementAnchor::PiecesList));
		else if (mElement == swInstructionsPlacementElement::PiecesList)
			mAnchorComboBox->addItem(tr("Step Number"), static_cast<int>(swInstructionsPlacementAnchor::StepNumber));

		const swInstructionsPropertyType AnchorType = GetAnchorType();

		if (AnchorType != swInstructionsPropertyType::Count)
			mAnchorComboBox->setCurrentIndex(qMax(mAnchorComboBox->findData(mInstructions->GetIntProperty(AnchorType, nullptr, 0)), 0));

		mAnchorComboBox->setEnabled(Active && mAnchorComboBox->count() > 1);
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
	setWhatsThis(tr("<p><b>Grid</b></p><p>The page grid helps aligning dragged elements, it is never printed. Show it on screen, snap dragged elements to it, and choose its area and the distance between its lines.</p>"));
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
	mGridSizeSpinBox->setToolTip(tr("Distance between the grid lines in the margin units of the page setup."));
	mGridSizeSpinBox->setWhatsThis(mGridSizeSpinBox->toolTip());
	mGridSizeSpinBox->setAccessibleName(tr("Grid Size"));
	swSetupPanelSpinBox(mGridSizeSpinBox);
	mGridSizeSpinBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	GridSizeRowLayout->addWidget(mGridSizeSpinBox, 1);
	GridSizeLabel->setBuddy(mGridSizeSpinBox);

	// the labels share one width so the inputs line up
	const int LabelWidth = qMax(GridScopeLabel->sizeHint().width(), GridSizeLabel->sizeHint().width());
	GridScopeLabel->setFixedWidth(LabelWidth);
	GridSizeLabel->setFixedWidth(LabelWidth);

	connect(mShowGridButton, &QToolButton::toggled, mInstructions, &swInstructions::SetShowGrid);
	connect(mSnapToGridButton, &QToolButton::toggled, mInstructions, &swInstructions::SetSnapToGrid);
	connect(mGridScopeComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int Index)
	{
		mInstructions->SetGridWithinMargins(Index == 0);
	});
	connect(mGridSizeSpinBox, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &swInstructionsGridWidget::GridSizeChanged);
	connect(mGridSizeUnitButton, &swInstructionsUnitButton::UnitsChanged, this, &swInstructionsGridWidget::UpdateGrid);

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

	mShowGridButton->setChecked(mInstructions->IsGridShown());
	mSnapToGridButton->setChecked(mInstructions->IsSnapToGrid());
	mGridScopeComboBox->setCurrentIndex(mInstructions->IsGridWithinMargins() ? 0 : 1);

	const swInstructionsPageUnits Units = mGridSizeUnitButton->GetUnits(mInstructions);

	swSetupLengthSpinBox(mGridSizeSpinBox, Units, SW_GRID_SIZE_MINIMUM, SW_GRID_SIZE_MAXIMUM);
	mGridSizeUnitButton->UpdateUnits(mInstructions);
	mGridSizeSpinBox->setValue(mInstructions->GetGridSize() / swGetPageUnitScale(Units));
}

void swInstructionsGridWidget::GridSizeChanged(double Size)
{
	mInstructions->SetGridSize(Size * swGetPageUnitScale(mGridSizeUnitButton->GetUnits(mInstructions)));
}
