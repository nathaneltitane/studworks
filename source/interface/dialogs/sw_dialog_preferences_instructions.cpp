#include "sw_global.h"
#include "sw_dialog_preferences.h"
#include "ui_sw_dialog_preferences.h"
#include "sw_window_utilities.h"
#include "sw_dialog_setup_page.h"
#include "sw_dialog_instructions_properties.h"

void swInstructionsPreferences::Load()
{
	PageSetup = { swGetProfileFloat(SW_PROFILE_INSTRUCTIONS_DEFAULT_PAGE_WIDTH), swGetProfileFloat(SW_PROFILE_INSTRUCTIONS_DEFAULT_PAGE_HEIGHT), swGetProfileFloat(SW_PROFILE_INSTRUCTIONS_DEFAULT_MARGIN_LEFT), swGetProfileFloat(SW_PROFILE_INSTRUCTIONS_DEFAULT_MARGIN_RIGHT), swGetProfileFloat(SW_PROFILE_INSTRUCTIONS_DEFAULT_MARGIN_TOP), swGetProfileFloat(SW_PROFILE_INSTRUCTIONS_DEFAULT_MARGIN_BOTTOM) };
	PageUnits = swGetProfileInteger(SW_PROFILE_INSTRUCTIONS_DEFAULT_PAGE_UNITS);
	MarginUnits = swGetProfileInteger(SW_PROFILE_INSTRUCTIONS_DEFAULT_MARGIN_UNITS);
	ShowGrid = swGetProfileInteger(SW_PROFILE_INSTRUCTIONS_DEFAULT_SHOW_GRID) != 0;
	SnapToGrid = swGetProfileInteger(SW_PROFILE_INSTRUCTIONS_DEFAULT_SNAP_TO_GRID) != 0;
	GridSize = swGetProfileFloat(SW_PROFILE_INSTRUCTIONS_DEFAULT_GRID_SIZE);
	GridDivisions = swGetProfileInteger(SW_PROFILE_INSTRUCTIONS_DEFAULT_GRID_DIVISIONS);
	GridWithinMargins = swGetProfileInteger(SW_PROFILE_INSTRUCTIONS_DEFAULT_GRID_WITHIN_MARGINS) != 0;
	NewModelDefaults = swGetProfileInteger(SW_PROFILE_INSTRUCTIONS_NEW_MODEL_DEFAULTS) != 0;
	ShowMargins = swGetProfileInteger(SW_PROFILE_INSTRUCTIONS_SHOW_MARGINS) != 0;
	GridPrimaryColor = swQColorFromRGBA(swGetProfileUnsignedInteger(SW_PROFILE_INSTRUCTIONS_GRID_PRIMARY_COLOR));
	GridSecondaryColor = swQColorFromRGBA(swGetProfileUnsignedInteger(SW_PROFILE_INSTRUCTIONS_GRID_SECONDARY_COLOR));
	MarginColor = swQColorFromRGBA(swGetProfileUnsignedInteger(SW_PROFILE_INSTRUCTIONS_MARGIN_COLOR));
	EditorHighlight = swGetProfileInteger(SW_PROFILE_INSTRUCTIONS_EDITOR_HIGHLIGHT) != 0;
	EditorNumbers = swGetProfileInteger(SW_PROFILE_INSTRUCTIONS_EDITOR_NUMBERS) != 0;
	EditorWrap = swGetProfileInteger(SW_PROFILE_INSTRUCTIONS_EDITOR_WRAP) != 0;
	EditorMarkers = swGetProfileInteger(SW_PROFILE_INSTRUCTIONS_EDITOR_MARKERS) != 0;
}

void swInstructionsPreferences::Save() const
{
	swSetProfileFloat(SW_PROFILE_INSTRUCTIONS_DEFAULT_PAGE_WIDTH, PageSetup.Width);
	swSetProfileFloat(SW_PROFILE_INSTRUCTIONS_DEFAULT_PAGE_HEIGHT, PageSetup.Height);
	swSetProfileFloat(SW_PROFILE_INSTRUCTIONS_DEFAULT_MARGIN_LEFT, PageSetup.MarginLeft);
	swSetProfileFloat(SW_PROFILE_INSTRUCTIONS_DEFAULT_MARGIN_RIGHT, PageSetup.MarginRight);
	swSetProfileFloat(SW_PROFILE_INSTRUCTIONS_DEFAULT_MARGIN_TOP, PageSetup.MarginTop);
	swSetProfileFloat(SW_PROFILE_INSTRUCTIONS_DEFAULT_MARGIN_BOTTOM, PageSetup.MarginBottom);
	swSetProfileInteger(SW_PROFILE_INSTRUCTIONS_DEFAULT_PAGE_UNITS, PageUnits);
	swSetProfileInteger(SW_PROFILE_INSTRUCTIONS_DEFAULT_MARGIN_UNITS, MarginUnits);
	swSetProfileInteger(SW_PROFILE_INSTRUCTIONS_DEFAULT_SHOW_GRID, ShowGrid);
	swSetProfileInteger(SW_PROFILE_INSTRUCTIONS_DEFAULT_SNAP_TO_GRID, SnapToGrid);
	swSetProfileFloat(SW_PROFILE_INSTRUCTIONS_DEFAULT_GRID_SIZE, GridSize);
	swSetProfileInteger(SW_PROFILE_INSTRUCTIONS_DEFAULT_GRID_DIVISIONS, GridDivisions);
	swSetProfileInteger(SW_PROFILE_INSTRUCTIONS_DEFAULT_GRID_WITHIN_MARGINS, GridWithinMargins);
	swSetProfileInteger(SW_PROFILE_INSTRUCTIONS_NEW_MODEL_DEFAULTS, NewModelDefaults);
	swSetProfileInteger(SW_PROFILE_INSTRUCTIONS_SHOW_MARGINS, ShowMargins);
	swSetProfileUnsignedInteger(SW_PROFILE_INSTRUCTIONS_GRID_PRIMARY_COLOR, swRGBAFromQColor(GridPrimaryColor));
	swSetProfileUnsignedInteger(SW_PROFILE_INSTRUCTIONS_GRID_SECONDARY_COLOR, swRGBAFromQColor(GridSecondaryColor));
	swSetProfileUnsignedInteger(SW_PROFILE_INSTRUCTIONS_MARGIN_COLOR, swRGBAFromQColor(MarginColor));
	swSetProfileInteger(SW_PROFILE_INSTRUCTIONS_EDITOR_HIGHLIGHT, EditorHighlight);
	swSetProfileInteger(SW_PROFILE_INSTRUCTIONS_EDITOR_NUMBERS, EditorNumbers);
	swSetProfileInteger(SW_PROFILE_INSTRUCTIONS_EDITOR_WRAP, EditorWrap);
	swSetProfileInteger(SW_PROFILE_INSTRUCTIONS_EDITOR_MARKERS, EditorMarkers);
}

// use to build the instructions tab after the colors, its values are kept in the dialog options until they are accepted
void swPreferencesDialog::CreateInstructionsTab()
{
	swInstructionsPreferences& Instructions = mOptions->Instructions;

	QWidget* Tab = new QWidget(ui->tabWidget);
	Tab->setWhatsThis(tr("<p><b>Instructions Preferences</b></p><p>Set the default page setup and grid that new models and the Default Page Setup option of the Properties panel load, and the options of the Instructions window.</p>"));
	QFormLayout* TabLayout = new QFormLayout(Tab);
	TabLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

	auto CreateCheckBox = [](QWidget* Parent, const QString& Text, const QString& ToolTip, bool Checked)
	{
		QCheckBox* CheckBox = new QCheckBox(Text, Parent);
		CheckBox->setToolTip(ToolTip);
		CheckBox->setChecked(Checked);
		return CheckBox;
	};

	QGroupBox* PageGroup = new QGroupBox(tr("Default Page Setup"), Tab);
	QFormLayout* PageLayout = new QFormLayout(PageGroup);
	TabLayout->addRow(PageGroup);

	QHBoxLayout* PageRowLayout = new QHBoxLayout();
	// the button shows the default page setup and opens it
	mInstructionsPageButton = new QPushButton(PageGroup);
	mInstructionsPageButton->setToolTip(tr("Choose the default page size, orientation, margins and their units."));
	mInstructionsPageButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	PageRowLayout->addWidget(mInstructionsPageButton, 1);

	QPushButton* PageResetButton = new QPushButton(tr("Reset"), PageGroup);
	PageResetButton->setToolTip(tr("Bring back the built-in default page setup, grid and line colors."));
	PageRowLayout->addWidget(PageResetButton);

	QLabel* PageLabel = new QLabel(tr("Page"), PageGroup);
	PageLabel->setToolTip(tr("Page size, orientation and margins of new instructions."));
	PageLayout->addRow(PageLabel, PageRowLayout);

	mInstructionsNewModelDefaults = CreateCheckBox(PageGroup, tr("Load on New Models"), tr("Start every new model with the default page setup and grid."), Instructions.NewModelDefaults);
	PageLayout->addRow(mInstructionsNewModelDefaults);

	QGroupBox* GridGroup = new QGroupBox(tr("Default Grid"), Tab);
	QFormLayout* GridLayout = new QFormLayout(GridGroup);
	TabLayout->addRow(GridGroup);

	mInstructionsShowGrid = CreateCheckBox(GridGroup, tr("Show Grid"), tr("Show the grid on the pages of new instructions, it is never printed."), Instructions.ShowGrid);
	GridLayout->addRow(mInstructionsShowGrid);
	mInstructionsSnapToGrid = CreateCheckBox(GridGroup, tr("Snap to Grid"), tr("Snap dragged elements to the grid of new instructions."), Instructions.SnapToGrid);
	GridLayout->addRow(mInstructionsSnapToGrid);

	mInstructionsGridSize = new QDoubleSpinBox(GridGroup);
	mInstructionsGridSize->setToolTip(tr("Main unit of the grid drawn with the primary color, in the margin units of the default page setup."));
	mInstructionsGridSize->setAccessibleName(tr("Grid Size"));
	QLabel* GridSizeLabel = new QLabel(tr("Grid Size"), GridGroup);
	GridSizeLabel->setBuddy(mInstructionsGridSize);
	GridSizeLabel->setToolTip(mInstructionsGridSize->toolTip());
	GridLayout->addRow(GridSizeLabel, mInstructionsGridSize);

	mInstructionsGridDivisions = new QSpinBox(GridGroup);
	mInstructionsGridDivisions->setRange(1, SW_GRID_DIVISIONS_MAXIMUM);
	mInstructionsGridDivisions->setPrefix(QLatin1String("1/"));
	mInstructionsGridDivisions->setValue(Instructions.GridDivisions);
	mInstructionsGridDivisions->setToolTip(tr("Split each grid unit into fractions drawn with the secondary color, dragged elements snap to the fractions."));
	mInstructionsGridDivisions->setAccessibleName(tr("Divisions"));
	QLabel* GridDivisionsLabel = new QLabel(tr("Divisions"), GridGroup);
	GridDivisionsLabel->setBuddy(mInstructionsGridDivisions);
	GridDivisionsLabel->setToolTip(mInstructionsGridDivisions->toolTip());
	GridLayout->addRow(GridDivisionsLabel, mInstructionsGridDivisions);

	mInstructionsGridScope = new QComboBox(GridGroup);
	mInstructionsGridScope->addItems({ tr("Margins"), tr("Page") });
	mInstructionsGridScope->setToolTip(tr("Lay the grid out within the margins or across the whole page."));
	mInstructionsGridScope->setAccessibleName(tr("Grid Area"));
	mInstructionsGridScope->setCurrentIndex(Instructions.GridWithinMargins ? 0 : 1);
	QLabel* GridScopeLabel = new QLabel(tr("Grid Area"), GridGroup);
	GridScopeLabel->setBuddy(mInstructionsGridScope);
	GridScopeLabel->setToolTip(mInstructionsGridScope->toolTip());
	GridLayout->addRow(GridScopeLabel, mInstructionsGridScope);

	QGroupBox* WindowGroup = new QGroupBox(tr("Instructions Page"), Tab);
	QFormLayout* WindowLayout = new QFormLayout(WindowGroup);
	TabLayout->addRow(WindowGroup);

	mInstructionsShowMargins = CreateCheckBox(WindowGroup, tr("Show Margins"), tr("Outline the page margins on screen, they are never printed."), Instructions.ShowMargins);
	WindowLayout->addRow(mInstructionsShowMargins);

	// the line colors keep their transparency, they are picked below their button
	auto CreateColorButton = [this, WindowLayout, WindowGroup](const QString& Name, const QString& ToolTip, QColor* Color)
	{
		QToolButton* Button = new QToolButton(WindowGroup);
		Button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
		Button->setToolTip(ToolTip);
		Button->setAccessibleName(Name);
		swUpdateColorButton(Button, *Color);

		QLabel* Label = new QLabel(Name, WindowGroup);
		Label->setBuddy(Button);
		Label->setToolTip(ToolTip);
		WindowLayout->addRow(Label, Button);

		connect(Button, &QToolButton::clicked, this, [Button, Color]()
		{
			const QColor SelectedColor = swPickColor(Button, *Color);

			if (!SelectedColor.isValid())
				return;

			*Color = SelectedColor;
			swUpdateColorButton(Button, SelectedColor);
		});

		return Button;
	};

	mInstructionsMarginColor = CreateColorButton(tr("Margin Color"), tr("Color of the dashed outline of the page margins."), &Instructions.MarginColor);
	mInstructionsGridPrimaryColor = CreateColorButton(tr("Grid Primary Color"), tr("Color of the grid lines at every grid unit."), &Instructions.GridPrimaryColor);
	mInstructionsGridSecondaryColor = CreateColorButton(tr("Grid Secondary Color"), tr("Color of the grid lines at the fractions of a grid unit."), &Instructions.GridSecondaryColor);

	QGroupBox* EditorGroup = new QGroupBox(tr("Editor"), Tab);
	QFormLayout* EditorLayout = new QFormLayout(EditorGroup);
	TabLayout->addRow(EditorGroup);

	mInstructionsEditorHighlight = CreateCheckBox(EditorGroup, tr("Syntax Highlighting"), tr("Color the commands of the model file in the Editor panel."), Instructions.EditorHighlight);
	EditorLayout->addRow(mInstructionsEditorHighlight);
	mInstructionsEditorNumbers = CreateCheckBox(EditorGroup, tr("Line Numbers"), tr("Number the lines of the model file in the Editor panel."), Instructions.EditorNumbers);
	EditorLayout->addRow(mInstructionsEditorNumbers);
	mInstructionsEditorWrap = CreateCheckBox(EditorGroup, tr("Wrap Lines"), tr("Wrap long lines of the model file in the Editor panel."), Instructions.EditorWrap);
	EditorLayout->addRow(mInstructionsEditorWrap);
	mInstructionsEditorMarkers = CreateCheckBox(EditorGroup, tr("Whitespace Markers"), tr("Mark tabs, spaces and line ends in the Editor panel."), Instructions.EditorMarkers);
	EditorLayout->addRow(mInstructionsEditorMarkers);

	ui->tabWidget->insertTab(ui->tabWidget->indexOf(ui->tabCategories), Tab, tr("Instructions"));

	connect(mInstructionsPageButton, &QPushButton::clicked, this, [this]()
	{
		swInstructionsPreferences& Instructions = mOptions->Instructions;

		// the dialog stores its units right away, they are kept with the other values until the preferences are accepted
		const int PageUnits = swGetProfileInteger(SW_PROFILE_INSTRUCTIONS_DEFAULT_PAGE_UNITS);
		const int MarginUnits = swGetProfileInteger(SW_PROFILE_INSTRUCTIONS_DEFAULT_MARGIN_UNITS);
		swSetProfileInteger(SW_PROFILE_INSTRUCTIONS_DEFAULT_PAGE_UNITS, Instructions.PageUnits);
		swSetProfileInteger(SW_PROFILE_INSTRUCTIONS_DEFAULT_MARGIN_UNITS, Instructions.MarginUnits);

		Instructions.GridSize = static_cast<float>(mInstructionsGridSize->value()) * swGetPageUnitScale(static_cast<swInstructionsPageUnits>(Instructions.MarginUnits));

		swPageSetupDialog Dialog(this, &Instructions.PageSetup, true);

		if (Dialog.exec() == QDialog::Accepted)
		{
			Instructions.PageUnits = swGetProfileInteger(SW_PROFILE_INSTRUCTIONS_DEFAULT_PAGE_UNITS);
			Instructions.MarginUnits = swGetProfileInteger(SW_PROFILE_INSTRUCTIONS_DEFAULT_MARGIN_UNITS);
		}

		swSetProfileInteger(SW_PROFILE_INSTRUCTIONS_DEFAULT_PAGE_UNITS, PageUnits);
		swSetProfileInteger(SW_PROFILE_INSTRUCTIONS_DEFAULT_MARGIN_UNITS, MarginUnits);

		UpdateInstructionsPage();
	});

	connect(PageResetButton, &QPushButton::clicked, this, [this]()
	{
		swInstructionsPreferences& Instructions = mOptions->Instructions;

		Instructions.PageSetup = { swGetProfileDefaultFloat(SW_PROFILE_INSTRUCTIONS_DEFAULT_PAGE_WIDTH), swGetProfileDefaultFloat(SW_PROFILE_INSTRUCTIONS_DEFAULT_PAGE_HEIGHT), swGetProfileDefaultFloat(SW_PROFILE_INSTRUCTIONS_DEFAULT_MARGIN_LEFT), swGetProfileDefaultFloat(SW_PROFILE_INSTRUCTIONS_DEFAULT_MARGIN_RIGHT), swGetProfileDefaultFloat(SW_PROFILE_INSTRUCTIONS_DEFAULT_MARGIN_TOP), swGetProfileDefaultFloat(SW_PROFILE_INSTRUCTIONS_DEFAULT_MARGIN_BOTTOM) };
		Instructions.PageUnits = swGetProfileDefaultInteger(SW_PROFILE_INSTRUCTIONS_DEFAULT_PAGE_UNITS);
		Instructions.MarginUnits = swGetProfileDefaultInteger(SW_PROFILE_INSTRUCTIONS_DEFAULT_MARGIN_UNITS);
		Instructions.GridSize = swGetProfileDefaultFloat(SW_PROFILE_INSTRUCTIONS_DEFAULT_GRID_SIZE);

		mInstructionsShowGrid->setChecked(swGetProfileDefaultInteger(SW_PROFILE_INSTRUCTIONS_DEFAULT_SHOW_GRID) != 0);
		mInstructionsSnapToGrid->setChecked(swGetProfileDefaultInteger(SW_PROFILE_INSTRUCTIONS_DEFAULT_SNAP_TO_GRID) != 0);
		mInstructionsGridScope->setCurrentIndex(swGetProfileDefaultInteger(SW_PROFILE_INSTRUCTIONS_DEFAULT_GRID_WITHIN_MARGINS) ? 0 : 1);
		mInstructionsGridDivisions->setValue(swGetProfileDefaultInteger(SW_PROFILE_INSTRUCTIONS_DEFAULT_GRID_DIVISIONS));

		Instructions.GridPrimaryColor = swQColorFromRGBA(swGetProfileDefaultUnsignedInteger(SW_PROFILE_INSTRUCTIONS_GRID_PRIMARY_COLOR));
		Instructions.GridSecondaryColor = swQColorFromRGBA(swGetProfileDefaultUnsignedInteger(SW_PROFILE_INSTRUCTIONS_GRID_SECONDARY_COLOR));
		Instructions.MarginColor = swQColorFromRGBA(swGetProfileDefaultUnsignedInteger(SW_PROFILE_INSTRUCTIONS_MARGIN_COLOR));
		swUpdateColorButton(mInstructionsGridPrimaryColor, Instructions.GridPrimaryColor);
		swUpdateColorButton(mInstructionsGridSecondaryColor, Instructions.GridSecondaryColor);
		swUpdateColorButton(mInstructionsMarginColor, Instructions.MarginColor);

		UpdateInstructionsPage();
	});

	UpdateInstructionsPage();
}

// use to show the default page and the grid size in the units of the default page setup
void swPreferencesDialog::UpdateInstructionsPage()
{
	const swInstructionsPreferences& Instructions = mOptions->Instructions;
	const swInstructionsPageSetup& PageSetup = Instructions.PageSetup;
	const swInstructionsPageUnits PageUnits = static_cast<swInstructionsPageUnits>(qBound(0, Instructions.PageUnits, static_cast<int>(swInstructionsPageUnits::Inches)));
	const swInstructionsPageUnits MarginUnits = static_cast<swInstructionsPageUnits>(qBound(0, Instructions.MarginUnits, static_cast<int>(swInstructionsPageUnits::Inches)));

	auto FormatLength = [this](float Points, swInstructionsPageUnits Units)
	{
		return locale().toString(Points / swGetPageUnitScale(Units), 'g', 6);
	};

	const QString Orientation = PageSetup.Width > PageSetup.Height ? tr("Landscape") : tr("Portrait");
	QString Margins = FormatLength(PageSetup.MarginLeft, MarginUnits);

	if (PageSetup.MarginRight != PageSetup.MarginLeft || PageSetup.MarginTop != PageSetup.MarginLeft || PageSetup.MarginBottom != PageSetup.MarginLeft)
		Margins = tr("%1, %2, %3, %4").arg(Margins, FormatLength(PageSetup.MarginRight, MarginUnits), FormatLength(PageSetup.MarginTop, MarginUnits), FormatLength(PageSetup.MarginBottom, MarginUnits));

	mInstructionsPageButton->setText(tr("%1 x %2 %3 %4, margins %5 %6...").arg(FormatLength(PageSetup.Width, PageUnits), FormatLength(PageSetup.Height, PageUnits), swGetPageUnitSuffix(PageUnits), Orientation, Margins, swGetPageUnitSuffix(MarginUnits)));

	QSignalBlocker Blocker(mInstructionsGridSize);
	swSetupLengthSpinBox(mInstructionsGridSize, MarginUnits, SW_GRID_SIZE_MINIMUM, SW_GRID_SIZE_MAXIMUM);
	mInstructionsGridSize->setValue(Instructions.GridSize / swGetPageUnitScale(MarginUnits));
}
