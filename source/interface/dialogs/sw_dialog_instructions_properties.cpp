#include "sw_global.h"
#include "sw_dialog_instructions_properties.h"
#include "sw_dialog_instructions_page.h"
#include "sw_dialog_setup_page.h"
#include "sw_project.h"
#include "sw_model.h"
#include "sw_widget_collapsible.h"
#include "sw_window_utilities.h"

// use to show a length stored in points in the page units, each step is a usual increment of the unit, the unit is shown beside the box
void swSetupLengthSpinBox(QDoubleSpinBox* SpinBox, swInstructionsPageUnits Units, double Minimum, double Maximum)
{
	const double Scale = swGetPageUnitScale(Units);
	int Decimals = 0;
	double Step = 1.0;

	switch (Units)
	{
	case swInstructionsPageUnits::Pixels:
		break;

	case swInstructionsPageUnits::Millimeters:
		Decimals = 1;
		break;

	case swInstructionsPageUnits::Centimeters:
		Decimals = 2;
		Step = 0.1;
		break;

	case swInstructionsPageUnits::Inches:
		Decimals = 3;
		Step = 0.125;
		break;
	}

	// short ranges like line widths step by one point
	if (Step * 10.0 > (Maximum - Minimum) / Scale)
		Step = 1.0 / Scale;

	// unchanged settings are left alone, setting them rewrites the text being typed
	if (SpinBox->decimals() != Decimals)
		SpinBox->setDecimals(Decimals);

	if (!qFuzzyCompare(SpinBox->singleStep(), Step))
		SpinBox->setSingleStep(Step);

	if (!qFuzzyCompare(SpinBox->minimum() + 1.0, Minimum / Scale + 1.0) || !qFuzzyCompare(SpinBox->maximum() + 1.0, Maximum / Scale + 1.0))
		SpinBox->setRange(Minimum / Scale, Maximum / Scale);

	// the maximum follows the value so the limit is known before typing
	const QString Suffix = QString(" / %1").arg(SpinBox->textFromValue(SpinBox->maximum()));

	if (SpinBox->suffix() != Suffix)
	{
		SpinBox->setSuffix(Suffix);
		swUpdatePanelSpinBoxWidth(SpinBox);
	}
}

swInstructionsUnitButton::swInstructionsUnitButton(QWidget* Parent, const QString& Name)
	: QToolButton(Parent)
{
	setToolTip(tr("Units of the value, press to show it in other units than the page units."));
	setWhatsThis(toolTip());
	setAccessibleName(tr("%1 Units").arg(Name));
	setToolButtonStyle(Qt::ToolButtonTextOnly);
	swSetupPanelButton(this);

	// the button is square like the icon buttons of the panels
	setFixedWidth(height());

	connect(this, &QToolButton::clicked, this, &swInstructionsUnitButton::ShowUnitsMenu);
}

swInstructionsPageUnits swInstructionsUnitButton::GetUnits(const swInstructions* Instructions) const
{
	// the document units win over the units of this value, which win over the page units
	const std::optional<swInstructionsPageUnits> DocumentUnits = Instructions->GetDocumentUnits();

	if (DocumentUnits)
		return *DocumentUnits;

	return mUnits ? *mUnits : Instructions->GetPageUnits();
}

void swInstructionsUnitButton::UpdateUnits(const swInstructions* Instructions)
{
	setText(swGetPageUnitSuffix(GetUnits(Instructions)));
	setEnabled(!Instructions->GetDocumentUnits());
}

void swInstructionsUnitButton::ShowUnitsMenu()
{
	QMenu Menu(this);
	QActionGroup* Group = new QActionGroup(&Menu);

	QAction* PageAction = Menu.addAction(tr("Page Units"));
	PageAction->setCheckable(true);
	PageAction->setChecked(!mUnits);
	Group->addAction(PageAction);

	Menu.addSeparator();

	const std::pair<swInstructionsPageUnits, QString> UnitNames[] =
	{
		{ swInstructionsPageUnits::Pixels, tr("Pixels") },
		{ swInstructionsPageUnits::Millimeters, tr("Millimeters") },
		{ swInstructionsPageUnits::Centimeters, tr("Centimeters") },
		{ swInstructionsPageUnits::Inches, tr("Inches") }
	};

	for (const std::pair<swInstructionsPageUnits, QString>& UnitName : UnitNames)
	{
		QAction* Action = Menu.addAction(UnitName.second);
		Action->setCheckable(true);
		Action->setChecked(mUnits && *mUnits == UnitName.first);
		Action->setData(static_cast<int>(UnitName.first));
		Group->addAction(Action);
	}

	QAction* Chosen = Menu.exec(mapToGlobal(rect().bottomLeft()));

	if (!Chosen)
		return;

	if (Chosen == PageAction)
		mUnits.reset();
	else
		mUnits = static_cast<swInstructionsPageUnits>(Chosen->data().toInt());

	emit UnitsChanged();
}


swInstructionsPropertiesWidget::swInstructionsPropertiesWidget(QWidget* Parent, swInstructions* Instructions)
	: QDockWidget(Parent), mInstructions(Instructions)
{
	setWindowTitle(tr("Properties"));
	setWhatsThis(tr("<p><b>Properties</b></p><p>Edit how the instruction pages look. With nothing selected on the page every section is shown, select an element to show only its settings and its box. Right-click the page to add notices and pointers. Changes apply to all pages.</p>"));
	swSetupWidgetHelp(this, nullptr);

	QWidget* CentralWidget = new QWidget(this);
	setWidget(CentralWidget);

	QVBoxLayout* Layout = new QVBoxLayout(CentralWidget);

	mOptionsRow = swCreatePanelRow(CentralWidget);
	swAddPanelRowStretch(mOptionsRow);
	Layout->addWidget(mOptionsRow);

	QMenu* OptionsMenu = new QMenu(this);
	swAddPanelOptionsButton(mOptionsRow, OptionsMenu, tr("Properties options"), tr("Properties Options"));

	QAction* PageSetupAction = OptionsMenu->addAction(tr("Page Setup..."));
	PageSetupAction->setToolTip(tr("Choose the page size, orientation and margins"));
	connect(PageSetupAction, &QAction::triggered, this, &swInstructionsPropertiesWidget::ShowPageSetupDialog);

	QAction* DefaultPageSetupAction = OptionsMenu->addAction(tr("Default Page Setup"));
	DefaultPageSetupAction->setToolTip(tr("Load the default page size, orientation, margins, grid and units of the preferences"));
	connect(DefaultPageSetupAction, &QAction::triggered, mInstructions, &swInstructions::ResetPageSetup);

	QAction* ShowMarginsAction = OptionsMenu->addAction(tr("Show Margins"));
	ShowMarginsAction->setToolTip(tr("Outline the page margins on screen, they are never printed"));
	ShowMarginsAction->setCheckable(true);
	ShowMarginsAction->setChecked(mInstructions->IsMarginsShown());
	connect(ShowMarginsAction, &QAction::toggled, mInstructions, &swInstructions::SetShowMargins);

	OptionsMenu->addSeparator();

	// the document units replace the units of every value while they are used
	QAction* DocumentUnitsAction = OptionsMenu->addAction(tr("Use Document Units"));
	DocumentUnitsAction->setToolTip(tr("Show every value in the document units, the unit buttons of the values are disabled"));
	DocumentUnitsAction->setCheckable(true);
	connect(DocumentUnitsAction, &QAction::toggled, this, [this](bool Checked)
	{
		mInstructions->SetDefaultBool(swInstructionsPropertyType::UseDocumentUnits, Checked);
	});

	QMenu* DocumentUnitsMenu = OptionsMenu->addMenu(tr("Document Units"));
	QActionGroup* DocumentUnitsGroup = new QActionGroup(DocumentUnitsMenu);

	const std::pair<swInstructionsPageUnits, QString> UnitNames[] =
	{
		{ swInstructionsPageUnits::Pixels, tr("Pixels") },
		{ swInstructionsPageUnits::Millimeters, tr("Millimeters") },
		{ swInstructionsPageUnits::Centimeters, tr("Centimeters") },
		{ swInstructionsPageUnits::Inches, tr("Inches") }
	};

	for (const std::pair<swInstructionsPageUnits, QString>& UnitName : UnitNames)
	{
		QAction* Action = DocumentUnitsMenu->addAction(UnitName.second);
		Action->setCheckable(true);
		Action->setData(static_cast<int>(UnitName.first));
		DocumentUnitsGroup->addAction(Action);
	}

	connect(DocumentUnitsGroup, &QActionGroup::triggered, this, [this](QAction* Action)
	{
		mInstructions->SetDefaultInt(swInstructionsPropertyType::DocumentUnits, Action->data().toInt());
	});

	connect(OptionsMenu, &QMenu::aboutToShow, this, [this, ShowMarginsAction, DocumentUnitsAction, DocumentUnitsMenu, DocumentUnitsGroup]()
	{
		const bool UseDocumentUnits = mInstructions->GetBoolProperty(swInstructionsPropertyType::UseDocumentUnits, nullptr, 0);
		const int Units = mInstructions->GetIntProperty(swInstructionsPropertyType::DocumentUnits, nullptr, 0);
		QSignalBlocker Blocker(DocumentUnitsAction);
		QSignalBlocker MarginsBlocker(ShowMarginsAction);

		ShowMarginsAction->setChecked(mInstructions->IsMarginsShown());
		DocumentUnitsAction->setChecked(UseDocumentUnits);
		DocumentUnitsMenu->setEnabled(UseDocumentUnits);

		for (QAction* Action : DocumentUnitsGroup->actions())
			Action->setChecked(Action->data().toInt() == Units);
	});

	connect(DocumentUnitsAction, &QAction::toggled, DocumentUnitsMenu, &QMenu::setEnabled);

	OptionsMenu->addSeparator();

	QAction* ExpandAction = OptionsMenu->addAction(tr("Expand All Sections"));
	ExpandAction->setToolTip(tr("Show the settings of every section"));
	connect(ExpandAction, &QAction::triggered, this, [this]()
	{
		SetCategoriesExpanded(true);
	});

	QAction* CollapseAction = OptionsMenu->addAction(tr("Collapse All Sections"));
	CollapseAction->setToolTip(tr("Hide the settings of every section"));
	connect(CollapseAction, &QAction::triggered, this, [this]()
	{
		SetCategoriesExpanded(false);
	});

	OptionsMenu->addSeparator();

	QAction* ResetAction = OptionsMenu->addAction(tr("Reset Properties"));
	ResetAction->setToolTip(tr("Restore the default look of the instructions"));
	connect(ResetAction, &QAction::triggered, mInstructions, &swInstructions::ResetProperties);

	// the sections scroll when the panel is shorter than them
	QScrollArea* ScrollArea = new QScrollArea(CentralWidget);
	ScrollArea->setWidgetResizable(true);
	ScrollArea->setFrameShape(QFrame::NoFrame);
	Layout->addWidget(ScrollArea);

	QWidget* CategoriesWidget = new QWidget(ScrollArea);
	mCategoriesLayout = new QVBoxLayout(CategoriesWidget);
	mCategoriesLayout->setContentsMargins(0, 0, 0, 0);
	ScrollArea->setWidget(CategoriesWidget);

	CreateWidgets();
	SetBoxElement(mBoxElement);
	mCategoriesLayout->addStretch(1);

	// the panel is as wide as its widest row so the sections never scroll sideways
	ScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	ScrollArea->setMinimumWidth(CategoriesWidget->minimumSizeHint().width() + ScrollArea->verticalScrollBar()->sizeHint().width());

	connect(mInstructions, &swInstructions::StepSettingsChanged, this, &swInstructionsPropertiesWidget::UpdateEditors);
	connect(mInstructions, &swInstructions::UnitsChanged, this, &swInstructionsPropertiesWidget::UpdateEditors);
	connect(mInstructions, &swInstructions::PageLayoutChanged, this, &swInstructionsPropertiesWidget::UpdateWidget);
	connect(mInstructions, &swInstructions::PageSetupChanged, this, &swInstructionsPropertiesWidget::UpdateWidget);

	UpdateWidget();
}

void swInstructionsPropertiesWidget::ShowPageSetupDialog()
{
	swInstructionsPageSetup PageSetup = mInstructions->mPageSetup;
	swPageSetupDialog Dialog(this, &PageSetup);

	if (Dialog.exec() != QDialog::Accepted)
		return;

	mInstructions->SetPageSetup(PageSetup);
}

// the presets split the page in halves, thirds or quarters, horizontal dividers stack rows and vertical dividers line up columns
struct swDividerPreset
{
	int Rows;
	int Columns;
	const char* Name;
	const char* ToolTip;
};

static const swDividerPreset swDividerPresets[] =
{
	{ 1, 1, QT_TRANSLATE_NOOP("swInstructionsPropertiesWidget", "Full Page"), QT_TRANSLATE_NOOP("swInstructionsPropertiesWidget", "Give the page a single step, without dividers.") },
	{ 2, 1, QT_TRANSLATE_NOOP("swInstructionsPropertiesWidget", "Horizontal Halves"), QT_TRANSLATE_NOOP("swInstructionsPropertiesWidget", "Split the page in two rows with a horizontal divider.") },
	{ 3, 1, QT_TRANSLATE_NOOP("swInstructionsPropertiesWidget", "Horizontal Thirds"), QT_TRANSLATE_NOOP("swInstructionsPropertiesWidget", "Split the page in three rows with horizontal dividers.") },
	{ 4, 1, QT_TRANSLATE_NOOP("swInstructionsPropertiesWidget", "Horizontal Quarters"), QT_TRANSLATE_NOOP("swInstructionsPropertiesWidget", "Split the page in four rows with horizontal dividers.") },
	{ 1, 2, QT_TRANSLATE_NOOP("swInstructionsPropertiesWidget", "Vertical Halves"), QT_TRANSLATE_NOOP("swInstructionsPropertiesWidget", "Split the page in two columns with a vertical divider.") },
	{ 1, 3, QT_TRANSLATE_NOOP("swInstructionsPropertiesWidget", "Vertical Thirds"), QT_TRANSLATE_NOOP("swInstructionsPropertiesWidget", "Split the page in three columns with vertical dividers.") },
	{ 1, 4, QT_TRANSLATE_NOOP("swInstructionsPropertiesWidget", "Vertical Quarters"), QT_TRANSLATE_NOOP("swInstructionsPropertiesWidget", "Split the page in four columns with vertical dividers.") },
	{ 2, 2, QT_TRANSLATE_NOOP("swInstructionsPropertiesWidget", "Quarters"), QT_TRANSLATE_NOOP("swInstructionsPropertiesWidget", "Split the page in two rows and two columns with dividers both ways.") }
};

void swInstructionsPropertiesWidget::CreateWidgets()
{
	AddCategory(CategoryIndex::Document, tr("Document"), tr("<p><b>Document</b></p><p>One font for all text of the instructions and the projection of the step and cover models. The instructions settings, notices and pointers are saved in the model file, the page setup and grid in the preferences.</p>"));

	AddRow(CategoryIndex::Document, {
		CreateToggleEditor(swInstructionsPropertyType::UseDocumentFont, tr("Use Document Font"), tr("Use one font for all text in the instructions."))
	});

	CreateFontEditor(swInstructionsPropertyType::DocumentFont, tr("Document Font"), tr("Font of all text while the document font is used."));
	AddEditorRow(CategoryIndex::Document, swInstructionsPropertyType::DocumentFont);

	CreateChoiceEditor(swInstructionsPropertyType::ModelProjection, tr("Model Projection"), tr("Show the step and cover models in perspective or orthographic, whatever the model view shows."), { tr("Perspective"), tr("Orthographic") });
	AddEditorRow(CategoryIndex::Document, swInstructionsPropertyType::ModelProjection);

	AddCategory(CategoryIndex::Cover, tr("Cover Page"), tr("<p><b>Cover Page</b></p><p>The first page, showing the model title, its author and the finished model. The title comes from the model description, or its name when there is none, the author from the model properties.</p>"));

	AddRow(CategoryIndex::Cover, {
		CreateToggleEditor(swInstructionsPropertyType::ShowCoverPage, tr("Show Cover Page"), tr("Start the instructions with a page showing the model title and the finished model.")),
		CreateToggleEditor(swInstructionsPropertyType::ShowCoverAuthor, tr("Show Author"), tr("Show the model author on the cover."))
	});

	AddRow(CategoryIndex::Cover, {
		CreateToggleEditor(swInstructionsPropertyType::ShowCoverBlankPage, tr("Blank Page After Cover"), tr("Follow the cover with a blank page, like the inside of a printed cover."))
	});

	CreateColorEditor(swInstructionsPropertyType::CoverBackgroundColor, tr("Cover Background"), tr("Color filling the whole cover page, margins included."));
	AddEditorRow(CategoryIndex::Cover, swInstructionsPropertyType::CoverBackgroundColor);

	CreateFontEditor(swInstructionsPropertyType::CoverTitleFont, tr("Cover Title Font"), tr("Font of the model title, the author uses half its size."));
	AddEditorRow(CategoryIndex::Cover, swInstructionsPropertyType::CoverTitleFont);
	CreateColorEditor(swInstructionsPropertyType::CoverTitleColor, tr("Cover Title Color"), tr("Color of the model title and author."));
	AddEditorRow(CategoryIndex::Cover, swInstructionsPropertyType::CoverTitleColor);

	AddCategory(CategoryIndex::BillOfMaterials, tr("Bill of Materials"), tr("<p><b>Bill of Materials</b></p><p>The last page, listing every part of the model. It uses the Parts List settings.</p>"));

	AddRow(CategoryIndex::BillOfMaterials, {
		CreateToggleEditor(swInstructionsPropertyType::ShowBillOfMaterials, tr("Show Bill of Materials"), tr("End the instructions with a page listing every part of the model."))
	});

	AddRow(CategoryIndex::BillOfMaterials, {
		CreateToggleEditor(swInstructionsPropertyType::ShowBillOfMaterialsBlankPage, tr("Blank Page Before Bill of Materials"), tr("Precede the bill of materials with a blank page."))
	});

	CreateColorEditor(swInstructionsPropertyType::BillOfMaterialsBackgroundColor, tr("BOM Background"), tr("Color filling the whole bill of materials page, margins included."));
	AddEditorRow(CategoryIndex::BillOfMaterials, swInstructionsPropertyType::BillOfMaterialsBackgroundColor);

	AddCategory(CategoryIndex::Page, tr("Page"), tr("<p><b>Page</b></p><p>The background of the step and blank pages. It applies to all these pages, or only to the current page when it has its own. The cover and the bill of materials have their own backgrounds.</p>"));

	CreateColorEditor(swInstructionsPropertyType::PageBackgroundColor, tr("Page Background"), tr("Color filling the whole page, margins included."));
	AddEditorRow(CategoryIndex::Page, swInstructionsPropertyType::PageBackgroundColor);

	// the background belongs to all pages or only to the current one
	mBackgroundPageButton = new QToolButton(this);
	mBackgroundPageButton->setText(tr("Background on This Page Only"));
	mBackgroundPageButton->setCheckable(true);
	mBackgroundPageButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
	mBackgroundPageButton->setToolTip(tr("Give this page its own background, unchecked it follows all pages."));
	mBackgroundPageButton->setWhatsThis(mBackgroundPageButton->toolTip());
	mBackgroundPageButton->setAccessibleName(mBackgroundPageButton->text());
	swSetupPanelButton(mBackgroundPageButton);

	connect(mBackgroundPageButton, &QToolButton::toggled, this, [this](bool PageOnly)
	{
		mInstructions->SetPageScope(mPageIndex, swInstructionsPropertyType::PageBackgroundColor, PageOnly);
	});

	AddRow(CategoryIndex::Page, { mBackgroundPageButton });

	AddCategory(CategoryIndex::PageLayout, tr("Steps per Page"), tr("<p><b>Steps per Page</b></p><p>The rows and columns of steps on the current page, the following pages flow on from it. The presets split the page in halves, thirds or quarters and show the step dividers. Steps fill landscape pages across each row and portrait pages down each column, following the orientation chosen in Page Setup. The dividers are the lines between neighboring steps.</p>"));

	std::vector<QWidget*> PresetButtons;

	for (int Preset = 0; Preset < static_cast<int>(std::size(swDividerPresets)); Preset++)
	{
		const swDividerPreset& DividerPreset = swDividerPresets[Preset];
		const QString Name = tr(DividerPreset.Name);

		QToolButton* Button = new QToolButton(this);
		Button->setIcon(CreateDividerPresetIcon(DividerPreset.Rows, DividerPreset.Columns, palette()));
		Button->setCheckable(true);
		Button->setToolTip(tr(DividerPreset.ToolTip));
		Button->setWhatsThis(Button->toolTip());
		Button->setAccessibleName(Name);
		swSetupPanelButton(Button);

		connect(Button, &QToolButton::clicked, this, [this, Preset]()
		{
			DividerPresetClicked(Preset);
		});

		mDividerPresetButtons.push_back(Button);
		PresetButtons.push_back(Button);
	}

	AddRow(CategoryIndex::PageLayout, PresetButtons);

	auto CreatePageLayoutSpinBox = [this](const QString& Name, const QString& ToolTip)
	{
		QSpinBox* SpinBox = new QSpinBox(this);
		SpinBox->setRange(1, 10);
		SpinBox->setSuffix(QString(" / %1").arg(SpinBox->maximum()));
		SpinBox->setToolTip(ToolTip);
		SpinBox->setWhatsThis(ToolTip);
		SpinBox->setAccessibleName(Name);
		swSetupPanelSpinBox(SpinBox);

		connect(SpinBox, qOverload<int>(&QSpinBox::valueChanged), this, [this]()
		{
			mInstructions->SetPageSettings(mPageIndex, mRowsSpinBox->value(), mColumnsSpinBox->value());
		});

		return SpinBox;
	};

	mRowsSpinBox = CreatePageLayoutSpinBox(tr("Rows"), tr("Number of step rows on this page."));
	mColumnsSpinBox = CreatePageLayoutSpinBox(tr("Columns"), tr("Number of step columns on this page."));

	QToolButton* ApplyAllButton = new QToolButton(this);
	ApplyAllButton->setText(tr("Apply to All Pages"));
	ApplyAllButton->setToolTip(tr("Use this number of rows and columns on every page."));
	ApplyAllButton->setWhatsThis(ApplyAllButton->toolTip());
	ApplyAllButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
	swSetupPanelButton(ApplyAllButton);

	connect(ApplyAllButton, &QToolButton::clicked, this, [this]()
	{
		swInstructionsPageSettings PageSettings;

		PageSettings.Rows = mRowsSpinBox->value();
		PageSettings.Columns = mColumnsSpinBox->value();

		mInstructions->SetDefaultPageSettings(PageSettings);
	});

	AddLabeledRow(CategoryIndex::PageLayout, tr("Rows"), mRowsSpinBox);
	AddLabeledRow(CategoryIndex::PageLayout, tr("Columns"), mColumnsSpinBox);
	AddRow(CategoryIndex::PageLayout, { ApplyAllButton });

	AddRow(CategoryIndex::PageLayout, {
		CreateToggleEditor(swInstructionsPropertyType::ShowStepDividers, tr("Show Step Dividers"), tr("Draw lines between neighboring steps."))
	});

	CreateColorEditor(swInstructionsPropertyType::StepDividerColor, tr("Divider Color"), tr("Color of the lines between steps."));
	AddEditorRow(CategoryIndex::PageLayout, swInstructionsPropertyType::StepDividerColor);
	CreateLengthEditor(swInstructionsPropertyType::StepDividerWidth, tr("Divider Width"), tr("Width of the lines between steps."), 1, 72, 0.1);
	AddEditorRow(CategoryIndex::PageLayout, swInstructionsPropertyType::StepDividerWidth);

	AddCategory(CategoryIndex::PageNumber, tr("Page Number"), tr("<p><b>Page Number</b></p><p>The number of each page, the cover and blank pages are not numbered unless all pages are. Drag it or use the Placement panel to move it on all pages.</p>"));

	AddRow(CategoryIndex::PageNumber, {
		CreateToggleEditor(swInstructionsPropertyType::ShowPageNumber, tr("Show Page Number"), tr("Show the number of each page except the cover and blank pages.")),
		CreateToggleEditor(swInstructionsPropertyType::ShowPageNumberAllPages, tr("Number All Pages"), tr("Number every page, the cover and blank pages included."))
	});

	// the choices follow the order of swInstructionsPageNumberFormat
	CreateChoiceEditor(swInstructionsPropertyType::PageNumberFormat, tr("Page Number Format"), tr("Show the page number alone or with the page count."), { tr("Page Number"), tr("Page Number / Page Count") });
	AddEditorRow(CategoryIndex::PageNumber, swInstructionsPropertyType::PageNumberFormat);
	CreateFontEditor(swInstructionsPropertyType::PageNumberFont, tr("Page Number Font"), tr("Font of the page numbers."));
	AddEditorRow(CategoryIndex::PageNumber, swInstructionsPropertyType::PageNumberFont);
	CreateColorEditor(swInstructionsPropertyType::PageNumberColor, tr("Page Number Color"), tr("Color of the page numbers."));
	AddEditorRow(CategoryIndex::PageNumber, swInstructionsPropertyType::PageNumberColor);

	AddCategory(CategoryIndex::Step, tr("Step Number"), tr("<p><b>Step Number</b></p><p>The step number and the area behind each step.</p>"));

	AddRow(CategoryIndex::Step, {
		CreateToggleEditor(swInstructionsPropertyType::ShowStepNumber, tr("Show Step Number"), tr("Show the number of each step in its top left corner."))
	});

	CreateFontEditor(swInstructionsPropertyType::StepNumberFont, tr("Step Number Font"), tr("Font of the step numbers."));
	AddEditorRow(CategoryIndex::Step, swInstructionsPropertyType::StepNumberFont);
	CreateColorEditor(swInstructionsPropertyType::StepNumberColor, tr("Step Number Color"), tr("Color of the step numbers."));
	AddEditorRow(CategoryIndex::Step, swInstructionsPropertyType::StepNumberColor);
	CreateColorEditor(swInstructionsPropertyType::StepBackgroundColor, tr("Step Background"), tr("Color filling the area of each step, transparent shows the page background."));
	AddEditorRow(CategoryIndex::Step, swInstructionsPropertyType::StepBackgroundColor);

	AddCategory(CategoryIndex::PartsList, tr("Parts List"), tr("<p><b>Parts List</b></p><p>The list of parts added in each step. The bill of materials uses the same settings.</p>"));

	AddRow(CategoryIndex::PartsList, {
		CreateToggleEditor(swInstructionsPropertyType::ShowStepPLI, tr("Show Parts List"), tr("Show the parts added in each step next to its number.")),
		CreateToggleEditor(swInstructionsPropertyType::PLIShowCounts, tr("Show Part Counts"), tr("Show how many of each part are needed."))
	});

	// the choices follow the order of swPartsListSort
	CreateChoiceEditor(swInstructionsPropertyType::PLISortOrder, tr("Parts List Order"), tr("Order of the parts in the list."), { tr("Sort by Color"), tr("Sort by Name"), tr("Sort by Part ID") });
	AddEditorRow(CategoryIndex::PartsList, swInstructionsPropertyType::PLISortOrder);
	// the choices follow the order of swPartsListLayout and swPartsListAlignment, the edges are named after the layout
	CreateChoiceEditor(swInstructionsPropertyType::PLILayout, tr("Parts List Layout"), tr("Line the parts up in rows or in columns."), { tr("Rows"), tr("Columns") });
	AddEditorRow(CategoryIndex::PartsList, swInstructionsPropertyType::PLILayout);
	CreateChoiceEditor(swInstructionsPropertyType::PLIAlignment, tr("Line Reference"), tr("Thumbnail edge the parts line up on."), { tr("Top"), tr("Middle"), tr("Bottom") });
	AddEditorRow(CategoryIndex::PartsList, swInstructionsPropertyType::PLIAlignment);
	CreateFontEditor(swInstructionsPropertyType::PLIFont, tr("Parts List Font"), tr("Font of the part counts."));
	AddEditorRow(CategoryIndex::PartsList, swInstructionsPropertyType::PLIFont);
	CreateColorEditor(swInstructionsPropertyType::PLITextColor, tr("Parts List Text Color"), tr("Color of the part counts."));
	AddEditorRow(CategoryIndex::PartsList, swInstructionsPropertyType::PLITextColor);
	CreateLengthEditor(swInstructionsPropertyType::PLIThumbnailSize, tr("Thumbnail Size"), tr("Size each part is drawn at, larger thumbnails show larger parts."), 32, 512);
	AddEditorRow(CategoryIndex::PartsList, swInstructionsPropertyType::PLIThumbnailSize);
	CreateColorEditor(swInstructionsPropertyType::PLIThumbnailBackgroundColor, tr("Thumbnail Background"), tr("Color behind each part and its count, transparent draws none."));
	AddEditorRow(CategoryIndex::PartsList, swInstructionsPropertyType::PLIThumbnailBackgroundColor);

	// the thumbnail settings belong to all pages or only to the current one
	mThumbnailPageButton = new QToolButton(this);
	mThumbnailPageButton->setText(tr("Thumbnails on This Page Only"));
	mThumbnailPageButton->setCheckable(true);
	mThumbnailPageButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
	mThumbnailPageButton->setToolTip(tr("Give this page its own thumbnail size and background, unchecked it follows all pages."));
	mThumbnailPageButton->setWhatsThis(mThumbnailPageButton->toolTip());
	mThumbnailPageButton->setAccessibleName(mThumbnailPageButton->text());
	swSetupPanelButton(mThumbnailPageButton);

	connect(mThumbnailPageButton, &QToolButton::toggled, this, &swInstructionsPropertiesWidget::SetThumbnailPageScope);

	AddRow(CategoryIndex::PartsList, { mThumbnailPageButton });

	AddCategory(CategoryIndex::Notice, tr("Notice"), tr("<p><b>Notice</b></p><p>Free text written on a page to point out something. Right-click the page and choose Add Notice to write one, double-click it to edit its text. Drag it, lock it in place or give it pointers like any other element.</p>"));

	CreateFontEditor(swInstructionsPropertyType::NoticeFont, tr("Notice Font"), tr("Font of the notices."));
	AddEditorRow(CategoryIndex::Notice, swInstructionsPropertyType::NoticeFont);
	CreateColorEditor(swInstructionsPropertyType::NoticeTextColor, tr("Notice Text Color"), tr("Color of the notice text."));
	AddEditorRow(CategoryIndex::Notice, swInstructionsPropertyType::NoticeTextColor);

	AddCategory(CategoryIndex::Pointer, tr("Pointer"), tr("<p><b>Pointer</b></p><p>A pointer leaves the border of an element to point at a part, a step or a notice. Right-click an element and choose Add Pointer, then drag its tip onto a part of a step model, another element or any point of the page, the tip follows what it points at. Right-click a pointer to choose the anchor it leaves from, or its own style and tip.</p>"));

	// the choices follow the order of swInstructionsPointerStyle and swInstructionsPointerTip
	CreateChoiceEditor(swInstructionsPropertyType::PointerStyle, tr("Pointer Style"), tr("Draw pointers as a line or as a callout wedge narrowing to the tip."), { tr("Line"), tr("Callout Wedge") });
	AddEditorRow(CategoryIndex::Pointer, swInstructionsPropertyType::PointerStyle);
	CreateChoiceEditor(swInstructionsPropertyType::PointerTip, tr("Pointer Tip"), tr("End pointers with an arrowhead or a dot."), { tr("Arrowhead"), tr("Dot") });
	AddEditorRow(CategoryIndex::Pointer, swInstructionsPropertyType::PointerTip);
	CreateColorEditor(swInstructionsPropertyType::PointerColor, tr("Pointer Color"), tr("Color of the pointers."));
	AddEditorRow(CategoryIndex::Pointer, swInstructionsPropertyType::PointerColor);
	// the lowest values follow the border of the element and three times the pointer width
	CreateLengthEditor(swInstructionsPropertyType::PointerWidth, tr("Pointer Width"), tr("Width of the pointer lines, Border follows the border of the element."), 0, 72, 0.1)->setSpecialValueText(tr("Border"));
	AddEditorRow(CategoryIndex::Pointer, swInstructionsPropertyType::PointerWidth);
	CreateLengthEditor(swInstructionsPropertyType::PointerTipSize, tr("Tip Size"), tr("Size of the arrowheads and dots, and width of the wedges at the element, never less than three times the pointer width."), 0, 288, 0.5)->setSpecialValueText(tr("Automatic"));
	AddEditorRow(CategoryIndex::Pointer, swInstructionsPropertyType::PointerTipSize);

	AddCategory(CategoryIndex::Box, tr("Box"), tr("<p><b>Box</b></p><p>The space and border around each element. The margin keeps other elements away, the padding keeps the border away from the content and the background fills all of it, margin and padding included. Pointers leave the border. Choose the element to edit, selecting an element on the page chooses it.</p>"));

	// the choices follow the order of swInstructionsBoxElement
	mBoxElementComboBox = new QComboBox(this);
	mBoxElementComboBox->addItems({ tr("Step Model"), tr("Step Number"), tr("Parts List"), tr("Page Number"), tr("Cover Title"), tr("Cover Author"), tr("Cover Model"), tr("Notice") });
	mBoxElementComboBox->setToolTip(tr("Choose the element whose box is edited, the parts list box is also the bill of materials box."));
	mBoxElementComboBox->setWhatsThis(mBoxElementComboBox->toolTip());
	mBoxElementComboBox->setAccessibleName(tr("Element"));
	swSetupPanelEdit(mBoxElementComboBox);
	AddLabeledRow(CategoryIndex::Box, tr("Element"), mBoxElementComboBox);

	connect(mBoxElementComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int Index)
	{
		if (Index >= 0)
			SetBoxElement(static_cast<swInstructionsBoxElement>(Index));
	});

	for (int ElementIndex = 0; ElementIndex < static_cast<int>(swInstructionsBoxElement::Count); ElementIndex++)
	{
		const swInstructionsBoxTypes BoxTypes = swGetBoxTypes(static_cast<swInstructionsBoxElement>(ElementIndex));
		std::vector<QWidget*>& Rows = mBoxRows[ElementIndex];

		CreateLengthEditor(BoxTypes.Margin, tr("Margin"), tr("Space outside the border, filled with the background."), 0, 288, 0.5);
		Rows.push_back(AddEditorRow(CategoryIndex::Box, BoxTypes.Margin)->parentWidget());
		CreateLengthEditor(BoxTypes.Padding, tr("Padding"), tr("Space between the border and the content, filled with the background."), 0, 288, 0.5);
		Rows.push_back(AddEditorRow(CategoryIndex::Box, BoxTypes.Padding)->parentWidget());
		CreateLengthEditor(BoxTypes.BorderWidth, tr("Border Width"), tr("Width of the line around the element, 0 draws no line."), 0, 72, 0.1);
		Rows.push_back(AddEditorRow(CategoryIndex::Box, BoxTypes.BorderWidth)->parentWidget());
		CreateColorEditor(BoxTypes.BorderColor, tr("Border Color"), tr("Color of the line around the element."));
		Rows.push_back(AddEditorRow(CategoryIndex::Box, BoxTypes.BorderColor)->parentWidget());
		CreateColorEditor(BoxTypes.BackgroundColor, tr("Background"), tr("Color filling the whole element, margin and padding included, transparent shows what lies behind."));
		Rows.push_back(AddEditorRow(CategoryIndex::Box, BoxTypes.BackgroundColor)->parentWidget());
	}

	// the labels share one width so the inputs line up across the categories
	int LabelWidth = 0;

	for (const QLabel* Label : mRowLabels)
		LabelWidth = qMax(LabelWidth, Label->sizeHint().width());

	for (QLabel* Label : mRowLabels)
		Label->setFixedWidth(LabelWidth);
}

void swInstructionsPropertiesWidget::AddCategory(CategoryIndex Index, const QString& Title, const QString& WhatsThis)
{
	CategoryWidgets& Category = mCategoryWidgets[static_cast<int>(Index)];

	Category.Button = new swCollapsibleWidgetButton(Title, this);
	Category.Button->setToolTip(tr("Expand or collapse the %1 properties.").arg(Title));
	Category.Button->setWhatsThis(WhatsThis);
	mCategoriesLayout->addWidget(Category.Button);

	Category.Rows = new QWidget(this);
	Category.Rows->setWhatsThis(WhatsThis);
	QVBoxLayout* RowsLayout = new QVBoxLayout(Category.Rows);
	RowsLayout->setContentsMargins(0, 0, 0, 0);
	mCategoriesLayout->addWidget(Category.Rows);

	connect(Category.Button, &swCollapsibleWidgetButton::StateChanged, this, &swInstructionsPropertiesWidget::UpdateCategories);
}

QWidget* swInstructionsPropertiesWidget::AddRow(CategoryIndex Index, const std::vector<QWidget*>& Editors)
{
	QWidget* Rows = mCategoryWidgets[static_cast<int>(Index)].Rows;
	QWidget* Row = swCreatePanelRow(Rows);
	Rows->layout()->addWidget(Row);

	// the editors of a row share its width evenly
	for (QWidget* Editor : Editors)
	{
		Editor->setParent(Row);
		Editor->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

		// spin boxes keep the width of their largest value so the limit is never cut off
		if (!qobject_cast<QAbstractSpinBox*>(Editor))
			Editor->setMinimumWidth(0);
		static_cast<QHBoxLayout*>(Row->layout())->addWidget(Editor, 1);
	}

	return Row;
}

QLabel* swInstructionsPropertiesWidget::AddLabeledRow(CategoryIndex Index, const QString& Name, QWidget* Editor)
{
	// the name sits beside the input so the input keeps its width for the value
	QWidget* Row = AddRow(Index, { Editor });
	QLabel* Label = new QLabel(Name, Row);
	Label->setBuddy(Editor);
	static_cast<QHBoxLayout*>(Row->layout())->insertWidget(0, Label);
	mRowLabels.push_back(Label);

	return Label;
}

QLabel* swInstructionsPropertiesWidget::AddEditorRow(CategoryIndex Index, swInstructionsPropertyType Type)
{
	EditorWidgets& Editors = mEditors[static_cast<int>(Type)];

	// the unit and the font size sit between the name and the input
	std::vector<QWidget*> RowEditors;

	if (Editors.SizeSpinBox)
		RowEditors.push_back(Editors.SizeSpinBox);

	RowEditors.push_back(Editors.Widget);

	QWidget* Row = AddRow(Index, RowEditors);
	QHBoxLayout* RowLayout = static_cast<QHBoxLayout*>(Row->layout());

	Editors.Label = new QLabel(Editors.Name, Row);
	Editors.Label->setBuddy(Editors.SizeSpinBox ? Editors.SizeSpinBox : Editors.Widget);
	RowLayout->insertWidget(0, Editors.Label);
	mRowLabels.push_back(Editors.Label);

	if (Editors.UnitButton)
	{
		Editors.UnitButton->setParent(Row);
		RowLayout->insertWidget(1, Editors.UnitButton);
	}

	return Editors.Label;
}

void swInstructionsPropertiesWidget::RegisterEditor(swInstructionsPropertyType Type, EditorType Editor, QWidget* Widget, const QString& Name, const QString& ToolTip)
{
	Widget->setToolTip(ToolTip);
	Widget->setWhatsThis(ToolTip);
	Widget->setAccessibleName(Name);

	EditorWidgets& Editors = mEditors[static_cast<int>(Type)];
	Editors.Widget = Widget;
	Editors.Type = Editor;
	Editors.Name = Name;
}

QToolButton* swInstructionsPropertiesWidget::CreateToggleEditor(swInstructionsPropertyType Type, const QString& Name, const QString& ToolTip)
{
	QToolButton* Button = new QToolButton(this);
	RegisterEditor(Type, EditorType::Bool, Button, Name, ToolTip);
	Button->setText(Name);
	Button->setCheckable(true);
	Button->setToolButtonStyle(Qt::ToolButtonTextOnly);
	swSetupPanelButton(Button);

	connect(Button, &QToolButton::toggled, this, [this, Type](bool Checked)
	{
		mInstructions->SetDefaultBool(Type, Checked);
	});

	return Button;
}

// use to get the largest value of an editor, a share of the shorter page side when set
double swInstructionsPropertiesWidget::GetEditorMaximum(const EditorWidgets& Editor) const
{
	if (Editor.PageFraction <= 0.0)
		return Editor.Maximum;

	const swInstructionsPageSetup& PageSetup = mInstructions->mPageSetup;

	return qMax<double>(Editor.Minimum + 1, qFloor(qMin(PageSetup.Width, PageSetup.Height) * Editor.PageFraction));
}

QDoubleSpinBox* swInstructionsPropertiesWidget::CreateLengthEditor(swInstructionsPropertyType Type, const QString& Name, const QString& ToolTip, int Minimum, int Maximum, double PageFraction)
{
	// lengths are stored in points and shown in the page units
	QDoubleSpinBox* SpinBox = new swInstructionsLengthSpinBox(this);
	RegisterEditor(Type, EditorType::Length, SpinBox, Name, ToolTip);

	EditorWidgets& Editors = mEditors[static_cast<int>(Type)];
	Editors.Minimum = Minimum;
	Editors.Maximum = Maximum;
	Editors.PageFraction = PageFraction;

	Editors.UnitButton = new swInstructionsUnitButton(this, Name);
	swSetupLengthSpinBox(SpinBox, mInstructions->GetPageUnits(), Minimum, GetEditorMaximum(Editors));

	// the value applies once typed, each change rebuilds the pages
	SpinBox->setKeyboardTracking(false);
	swSetupPanelSpinBox(SpinBox);

	connect(SpinBox, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this, Type](double Value)
	{
		SetIntValue(Type, qRound(Value * swGetPageUnitScale(GetEditorUnits(Type))));
	});

	connect(Editors.UnitButton, &swInstructionsUnitButton::UnitsChanged, this, &swInstructionsPropertiesWidget::UpdateEditors);

	return SpinBox;
}

QComboBox* swInstructionsPropertiesWidget::CreateChoiceEditor(swInstructionsPropertyType Type, const QString& Name, const QString& ToolTip, const QStringList& Choices)
{
	QComboBox* ComboBox = new QComboBox(this);
	RegisterEditor(Type, EditorType::Choice, ComboBox, Name, ToolTip);

	// the choices are in the order of the property values
	ComboBox->addItems(Choices);
	swSetupPanelEdit(ComboBox);

	connect(ComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, Type](int Index)
	{
		mInstructions->SetDefaultInt(Type, Index);
	});

	return ComboBox;
}

QToolButton* swInstructionsPropertiesWidget::CreateColorEditor(swInstructionsPropertyType Type, const QString& Name, const QString& ToolTip)
{
	QToolButton* Button = new QToolButton(this);
	RegisterEditor(Type, EditorType::Color, Button, Name, ToolTip);
	Button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	swSetupPanelButton(Button);

	connect(Button, &QToolButton::clicked, this, [this, Type]()
	{
		ColorButtonClicked(Type);
	});

	return Button;
}

QToolButton* swInstructionsPropertiesWidget::CreateFontEditor(swInstructionsPropertyType Type, const QString& Name, const QString& ToolTip)
{
	// the size is typed in the page units like every length beside the button choosing the font
	QToolButton* Button = new QToolButton(this);
	RegisterEditor(Type, EditorType::Font, Button, Name, ToolTip);
	Button->setToolButtonStyle(Qt::ToolButtonTextOnly);
	swSetupPanelButton(Button);

	EditorWidgets& Editors = mEditors[static_cast<int>(Type)];
	Editors.Minimum = 4;
	Editors.Maximum = 288;
	Editors.UnitButton = new swInstructionsUnitButton(this, Name);
	Editors.SizeSpinBox = new swInstructionsLengthSpinBox(this);
	swSetupLengthSpinBox(Editors.SizeSpinBox, mInstructions->GetPageUnits(), Editors.Minimum, Editors.Maximum);
	Editors.SizeSpinBox->setKeyboardTracking(false);
	Editors.SizeSpinBox->setToolTip(tr("Size of the font in the page units."));
	Editors.SizeSpinBox->setWhatsThis(Editors.SizeSpinBox->toolTip());
	Editors.SizeSpinBox->setAccessibleName(tr("%1 Size").arg(Name));
	swSetupPanelSpinBox(Editors.SizeSpinBox);

	connect(Button, &QToolButton::clicked, this, [this, Type]()
	{
		FontButtonClicked(Type);
	});

	connect(Editors.SizeSpinBox, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this, Type](double Size)
	{
		QFont Font = mInstructions->GetFontProperty(Type, mModel, mStep);
		Font.setPointSizeF(Size * swGetPageUnitScale(GetEditorUnits(Type)));
		mInstructions->SetDefaultFont(Type, Font);
	});

	connect(Editors.UnitButton, &swInstructionsUnitButton::UnitsChanged, this, &swInstructionsPropertiesWidget::UpdateEditors);

	return Button;
}

void swInstructionsPropertiesWidget::ColorButtonClicked(swInstructionsPropertyType Type)
{
	QToolButton* Button = qobject_cast<QToolButton*>(mEditors[static_cast<int>(Type)].Widget);

	// backgrounds can be transparent so the picker edits the alpha channel too
	const QColor SelectedColor = swPickColor(Button, GetColorValue(Type));

	if (SelectedColor.isValid())
		SetColorValue(Type, SelectedColor);
}

void swInstructionsPropertiesWidget::FontButtonClicked(swInstructionsPropertyType Type)
{
	bool Ok = false;
	QFont Font = mInstructions->GetFontProperty(Type, mModel, mStep);

	Font = QFontDialog::getFont(&Ok, Font, this, mEditors[static_cast<int>(Type)].Name);

	if (Ok)
		mInstructions->SetDefaultFont(Type, Font);
}

void swInstructionsPropertiesWidget::SetCategoriesExpanded(bool Expanded)
{
	for (CategoryWidgets& Category : mCategoryWidgets)
		Category.Button->SetExpanded(Expanded);

	UpdateCategories();
}

void swInstructionsPropertiesWidget::AddOptionsAction(QAction* Action)
{
	// actions fill the row beside the options button in place of the stretch
	QHBoxLayout* RowLayout = static_cast<QHBoxLayout*>(mOptionsRow->layout());
	QToolButton* Button = new QToolButton(mOptionsRow);

	Button->setDefaultAction(Action);
	Button->setToolButtonStyle(Qt::ToolButtonTextOnly);
	Button->setWhatsThis(Action->toolTip());
	swSetupPanelButton(Button);
	Button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

	if (RowLayout->itemAt(0)->spacerItem())
		delete RowLayout->takeAt(0);

	RowLayout->insertWidget(RowLayout->count() - 1, Button, 1);
}

void swInstructionsPropertiesWidget::SelectionChanged(QGraphicsItem* FocusItem)
{
	// only the kind and step of the item are kept, page rebuilds delete the items
	FocusType Focus = FocusType::None;
	swModel* Model = nullptr;
	swStep Step = 1;

	if (swInstructionsStepImageItem* ImageItem = dynamic_cast<swInstructionsStepImageItem*>(FocusItem))
	{
		Focus = FocusType::StepImage;
		Model = ImageItem->GetModel();
		Step = ImageItem->GetStep();
	}
	else if (swInstructionsStepNumberItem* NumberItem = dynamic_cast<swInstructionsStepNumberItem*>(FocusItem))
	{
		Focus = FocusType::StepNumber;
		Model = NumberItem->GetModel();
		Step = NumberItem->GetStep();
	}
	else if (swInstructionsPartsListItem* PartsItem = dynamic_cast<swInstructionsPartsListItem*>(FocusItem))
	{
		Focus = FocusType::PartsList;
		Model = PartsItem->GetModel();
		Step = PartsItem->GetStep();
	}
	else if (dynamic_cast<swInstructionsPageNumberItem*>(FocusItem))
		Focus = FocusType::PageNumber;
	else if (swInstructionsCoverTextItem* TextItem = dynamic_cast<swInstructionsCoverTextItem*>(FocusItem))
	{
		Focus = TextItem->IsAuthor() ? FocusType::CoverAuthor : FocusType::CoverTitle;
		Model = TextItem->GetModel();
		Step = 0;
	}
	else if (dynamic_cast<swInstructionsNoticeItem*>(FocusItem))
		Focus = FocusType::Notice;
	else if (dynamic_cast<swInstructionsPointerItem*>(FocusItem))
		Focus = FocusType::Pointer;

	// the model image of the cover and the list of the bill of materials have their own sections
	if (Focus == FocusType::StepImage && dynamic_cast<swInstructionsStepImageItem*>(FocusItem)->IsCoverImage())
		Focus = FocusType::CoverImage;
	else if (Focus == FocusType::PartsList && dynamic_cast<swInstructionsPartsListItem*>(FocusItem)->IsBillOfMaterials())
		Focus = FocusType::BillOfMaterials;

	// the box section edits the selected element
	switch (Focus)
	{
	case FocusType::StepImage:
		SetBoxElement(swInstructionsBoxElement::StepImage);
		break;

	case FocusType::StepNumber:
		SetBoxElement(swInstructionsBoxElement::StepNumber);
		break;

	case FocusType::PartsList:
	case FocusType::BillOfMaterials:
		SetBoxElement(swInstructionsBoxElement::PartsList);
		break;

	case FocusType::PageNumber:
		SetBoxElement(swInstructionsBoxElement::PageNumber);
		break;

	case FocusType::CoverTitle:
		SetBoxElement(swInstructionsBoxElement::CoverTitle);
		break;

	case FocusType::CoverAuthor:
		SetBoxElement(swInstructionsBoxElement::CoverAuthor);
		break;

	case FocusType::CoverImage:
		SetBoxElement(swInstructionsBoxElement::CoverImage);
		break;

	case FocusType::Notice:
		SetBoxElement(swInstructionsBoxElement::Notice);
		break;

	case FocusType::Pointer:
	case FocusType::None:
		break;
	}

	if (Focus == mFocus && Model == mModel && Step == mStep)
		return;

	mFocus = Focus;
	mModel = Model;
	mStep = Step;

	UpdateWidget();
}

void swInstructionsPropertiesWidget::SetBoxElement(swInstructionsBoxElement Element)
{
	mBoxElement = Element;

	{
		QSignalBlocker Blocker(mBoxElementComboBox);
		mBoxElementComboBox->setCurrentIndex(static_cast<int>(Element));
	}

	// only the rows of the chosen element are shown
	for (int ElementIndex = 0; ElementIndex < static_cast<int>(mBoxRows.size()); ElementIndex++)
		for (QWidget* Row : mBoxRows[ElementIndex])
			Row->setVisible(ElementIndex == static_cast<int>(Element));
}

void swInstructionsPropertiesWidget::SetCurrentPage(int PageIndex)
{
	mPageIndex = PageIndex;

	// the page settings and the page layout show the current page
	UpdateEditors();
	UpdatePageLayout();
	UpdateCategories();
}

void swInstructionsPropertiesWidget::UpdateWidget()
{
	UpdateEditors();
	UpdatePageLayout();
	UpdateCategories();
}

void swInstructionsPropertiesWidget::UpdateEditors()
{
	for (int TypeIndex = 0; TypeIndex < static_cast<int>(swInstructionsPropertyType::Count); TypeIndex++)
	{
		const swInstructionsPropertyType Type = static_cast<swInstructionsPropertyType>(TypeIndex);
		const EditorWidgets& Editor = mEditors[TypeIndex];

		if (!Editor.Widget)
			continue;

		QSignalBlocker Blocker(Editor.Widget);

		switch (Editor.Type)
		{
		case EditorType::Bool:
			qobject_cast<QToolButton*>(Editor.Widget)->setChecked(mInstructions->GetBoolProperty(Type, mModel, mStep));
			break;

		case EditorType::Length:
			{
				// the units follow the page setup unless overridden, an unchanged value leaves the text being typed alone
				QDoubleSpinBox* SpinBox = qobject_cast<QDoubleSpinBox*>(Editor.Widget);
				const swInstructionsPageUnits Units = GetEditorUnits(Type);
				QSignalBlocker SpinBoxBlocker(SpinBox);

				swSetupLengthSpinBox(SpinBox, Units, Editor.Minimum, GetEditorMaximum(Editor));
				Editor.UnitButton->UpdateUnits(mInstructions);

				const double Value = GetIntValue(Type) / swGetPageUnitScale(Units);

				if (qRound(SpinBox->value() * swGetPageUnitScale(Units)) != GetIntValue(Type))
					SpinBox->setValue(Value);
			}
			break;

		case EditorType::Choice:
			qobject_cast<QComboBox*>(Editor.Widget)->setCurrentIndex(mInstructions->GetIntProperty(Type, mModel, mStep));
			break;

		case EditorType::Color:
			{
				QToolButton* Button = qobject_cast<QToolButton*>(Editor.Widget);
				swUpdateColorButton(Button, GetColorValue(Type));
			}
			break;

		case EditorType::Font:
			{
				const QFont Font = mInstructions->GetFontProperty(Type, mModel, mStep);
				QSignalBlocker SizeBlocker(Editor.SizeSpinBox);

				const swInstructionsPageUnits Units = GetEditorUnits(Type);

				qobject_cast<QToolButton*>(Editor.Widget)->setText(Font.family());
				swSetupLengthSpinBox(Editor.SizeSpinBox, Units, Editor.Minimum, Editor.Maximum);
				Editor.UnitButton->UpdateUnits(mInstructions);
				if (!qFuzzyCompare(Editor.SizeSpinBox->value() * swGetPageUnitScale(Units), Font.pointSizeF()))
					Editor.SizeSpinBox->setValue(Font.pointSizeF() / swGetPageUnitScale(Units));
			}
			break;
		}
	}

	// settings without effect are disabled, the document font replaces every element font
	auto GetBool = [this](swInstructionsPropertyType Type)
	{
		return mInstructions->GetBoolProperty(Type, mModel, mStep);
	};

	auto SetEnabled = [this](swInstructionsPropertyType Type, bool Enabled)
	{
		const EditorWidgets& Editors = mEditors[static_cast<int>(Type)];
		Editors.Widget->setEnabled(Enabled);

		if (Editors.Label)
			Editors.Label->setEnabled(Enabled);

		if (Editors.UnitButton)
			Editors.UnitButton->setEnabled(Enabled && !mInstructions->GetDocumentUnits());

		if (Editors.SizeSpinBox)
			Editors.SizeSpinBox->setEnabled(Enabled);
	};

	const bool UseDocumentFont = GetBool(swInstructionsPropertyType::UseDocumentFont);
	const bool ShowStepNumber = GetBool(swInstructionsPropertyType::ShowStepNumber);
	const bool ShowStepDividers = GetBool(swInstructionsPropertyType::ShowStepDividers);

	SetEnabled(swInstructionsPropertyType::DocumentFont, UseDocumentFont);
	const bool ShowCoverPage = GetBool(swInstructionsPropertyType::ShowCoverPage);

	SetEnabled(swInstructionsPropertyType::CoverTitleFont, !UseDocumentFont && ShowCoverPage);
	SetEnabled(swInstructionsPropertyType::CoverTitleColor, ShowCoverPage);
	SetEnabled(swInstructionsPropertyType::ShowCoverAuthor, ShowCoverPage);
	SetEnabled(swInstructionsPropertyType::ShowCoverBlankPage, ShowCoverPage);
	SetEnabled(swInstructionsPropertyType::CoverBackgroundColor, ShowCoverPage);

	const bool ShowBillOfMaterials = GetBool(swInstructionsPropertyType::ShowBillOfMaterials);

	SetEnabled(swInstructionsPropertyType::ShowBillOfMaterialsBlankPage, ShowBillOfMaterials);
	SetEnabled(swInstructionsPropertyType::BillOfMaterialsBackgroundColor, ShowBillOfMaterials);
	SetEnabled(swInstructionsPropertyType::StepNumberFont, !UseDocumentFont && ShowStepNumber);
	SetEnabled(swInstructionsPropertyType::StepNumberColor, ShowStepNumber);
	SetEnabled(swInstructionsPropertyType::PLIFont, !UseDocumentFont);

	// rows line up on a horizontal edge, columns on a vertical one
	QComboBox* AlignmentComboBox = qobject_cast<QComboBox*>(mEditors[static_cast<int>(swInstructionsPropertyType::PLIAlignment)].Widget);
	const bool Columns = mInstructions->GetIntProperty(swInstructionsPropertyType::PLILayout, mModel, mStep) == static_cast<int>(swPartsListLayout::Columns);
	const QStringList AlignmentNames = Columns ? QStringList{ tr("Left"), tr("Center"), tr("Right") } : QStringList{ tr("Top"), tr("Middle"), tr("Bottom") };

	for (int Index = 0; Index < AlignmentNames.size(); Index++)
		AlignmentComboBox->setItemText(Index, AlignmentNames[Index]);
	SetEnabled(swInstructionsPropertyType::NoticeFont, !UseDocumentFont);
	SetEnabled(swInstructionsPropertyType::StepDividerColor, ShowStepDividers);

	const bool ShowPageNumber = GetBool(swInstructionsPropertyType::ShowPageNumber);

	SetEnabled(swInstructionsPropertyType::ShowPageNumberAllPages, ShowPageNumber);
	SetEnabled(swInstructionsPropertyType::PageNumberFormat, ShowPageNumber);
	SetEnabled(swInstructionsPropertyType::PageNumberFont, !UseDocumentFont && ShowPageNumber);
	SetEnabled(swInstructionsPropertyType::PageNumberColor, ShowPageNumber);
	SetEnabled(swInstructionsPropertyType::StepDividerWidth, ShowStepDividers);

	const swInstructionsPageType PageType = GetPageType();
	QSignalBlocker Blocker(mThumbnailPageButton);

	mThumbnailPageButton->setEnabled(PageType == swInstructionsPageType::Steps || PageType == swInstructionsPageType::BillOfMaterials);
	mThumbnailPageButton->setChecked(mInstructions->HasPageProperty(mPageIndex, swInstructionsPropertyType::PLIThumbnailSize));

	QSignalBlocker BackgroundBlocker(mBackgroundPageButton);

	mBackgroundPageButton->setChecked(mInstructions->HasPageProperty(mPageIndex, swInstructionsPropertyType::PageBackgroundColor));
}

swInstructionsPageUnits swInstructionsPropertiesWidget::GetEditorUnits(swInstructionsPropertyType Type) const
{
	return mEditors[static_cast<int>(Type)].UnitButton->GetUnits(mInstructions);
}

int swInstructionsPropertiesWidget::GetIntValue(swInstructionsPropertyType Type) const
{
	// page properties always show the current page, which may follow all pages
	if (swInstructions::IsPageProperty(Type))
		return mInstructions->GetPageIntProperty(mPageIndex, Type);

	return mInstructions->GetIntProperty(Type, mModel, mStep);
}

QColor swInstructionsPropertiesWidget::GetColorValue(swInstructionsPropertyType Type) const
{
	if (swInstructions::IsPageProperty(Type))
		return mInstructions->GetPageColorProperty(mPageIndex, Type);

	return mInstructions->GetColorProperty(Type, mModel, mStep);
}

void swInstructionsPropertiesWidget::SetIntValue(swInstructionsPropertyType Type, int Value)
{
	if (mInstructions->HasPageProperty(mPageIndex, Type))
		mInstructions->SetPageInt(mPageIndex, Type, Value);
	else
		mInstructions->SetDefaultInt(Type, Value);
}

void swInstructionsPropertiesWidget::SetColorValue(swInstructionsPropertyType Type, const QColor& Color)
{
	if (mInstructions->HasPageProperty(mPageIndex, Type))
		mInstructions->SetPageColor(mPageIndex, Type, Color);
	else
		mInstructions->SetDefaultColor(Type, Color);
}

void swInstructionsPropertiesWidget::SetThumbnailPageScope(bool PageOnly)
{
	mInstructions->SetPageScope(mPageIndex, swInstructionsPropertyType::PLIThumbnailSize, PageOnly);
	mInstructions->SetPageScope(mPageIndex, swInstructionsPropertyType::PLIThumbnailBackgroundColor, PageOnly);
}

void swInstructionsPropertiesWidget::DividerPresetClicked(int Preset)
{
	const swDividerPreset& DividerPreset = swDividerPresets[Preset];

	mInstructions->SetPageDividerPreset(mPageIndex, DividerPreset.Rows, DividerPreset.Columns);

	// clicking the current preset changes nothing but unchecks its button
	UpdatePageLayout();
}

QIcon swInstructionsPropertiesWidget::CreateDividerPresetIcon(int Rows, int Columns, const QPalette& Palette)
{
	// the outline is the page, the lines are its dividers
	const int Size = 32;
	const QRectF PageRect(6.0, 4.0, 20.0, 24.0);

	QPixmap Pixmap(Size, Size);
	Pixmap.fill(Qt::transparent);

	QPainter Painter(&Pixmap);
	QColor OutlineColor = Palette.color(QPalette::WindowText);
	OutlineColor.setAlpha(110);

	Painter.setPen(QPen(OutlineColor, 2.0));
	Painter.drawRect(PageRect);
	Painter.setPen(QPen(Palette.color(QPalette::WindowText), 2.0));

	for (int Row = 1; Row < Rows; Row++)
	{
		const qreal Y = PageRect.top() + PageRect.height() * Row / Rows;
		Painter.drawLine(QPointF(PageRect.left() + 3.0, Y), QPointF(PageRect.right() - 3.0, Y));
	}

	for (int Column = 1; Column < Columns; Column++)
	{
		const qreal X = PageRect.left() + PageRect.width() * Column / Columns;
		Painter.drawLine(QPointF(X, PageRect.top() + 3.0), QPointF(X, PageRect.bottom() - 3.0));
	}

	Painter.end();

	return QIcon(Pixmap);
}

void swInstructionsPropertiesWidget::UpdatePageLayout()
{
	const swInstructionsPageSettings PageSettings = mInstructions->GetPageSettings(mPageIndex);

	QSignalBlocker RowsBlocker(mRowsSpinBox);
	QSignalBlocker ColumnsBlocker(mColumnsSpinBox);

	mRowsSpinBox->setValue(PageSettings.Rows);
	mColumnsSpinBox->setValue(PageSettings.Columns);

	// the preset matching the rows and columns of the page is checked
	for (int Preset = 0; Preset < static_cast<int>(mDividerPresetButtons.size()); Preset++)
		mDividerPresetButtons[Preset]->setChecked(swDividerPresets[Preset].Rows == PageSettings.Rows && swDividerPresets[Preset].Columns == PageSettings.Columns);
}

swInstructionsPageType swInstructionsPropertiesWidget::GetPageType() const
{
	if (mPageIndex < 0 || mPageIndex >= static_cast<int>(mInstructions->mPages.size()))
		return swInstructionsPageType::Steps;

	return mInstructions->mPages[mPageIndex].Type;
}

void swInstructionsPropertiesWidget::UpdateCategories()
{
	std::array<bool, static_cast<int>(CategoryIndex::Count)> Visible = {};

	// a selected item shows only its own settings, otherwise everything that applies to the page
	switch (mFocus)
	{
	case FocusType::None:
		{
			// the page type decides which page and step settings apply, the cover and bill of materials can always be switched on
			const swInstructionsPageType PageType = GetPageType();

			Visible[static_cast<int>(CategoryIndex::Document)] = true;
			Visible[static_cast<int>(CategoryIndex::Cover)] = true;
			Visible[static_cast<int>(CategoryIndex::BillOfMaterials)] = true;
			Visible[static_cast<int>(CategoryIndex::PageLayout)] = PageType == swInstructionsPageType::Steps;
			Visible[static_cast<int>(CategoryIndex::Page)] = PageType == swInstructionsPageType::Steps || PageType == swInstructionsPageType::Blank;
			Visible[static_cast<int>(CategoryIndex::PageNumber)] = PageType != swInstructionsPageType::Cover;
			Visible[static_cast<int>(CategoryIndex::Step)] = PageType == swInstructionsPageType::Steps;
			Visible[static_cast<int>(CategoryIndex::PartsList)] = PageType == swInstructionsPageType::Steps || PageType == swInstructionsPageType::BillOfMaterials;
			Visible[static_cast<int>(CategoryIndex::Notice)] = true;
			Visible[static_cast<int>(CategoryIndex::Pointer)] = true;
			Visible[static_cast<int>(CategoryIndex::Box)] = true;
		}
		break;

	case FocusType::StepImage:
		Visible[static_cast<int>(CategoryIndex::Step)] = true;
		Visible[static_cast<int>(CategoryIndex::PartsList)] = true;
		Visible[static_cast<int>(CategoryIndex::Box)] = true;
		break;

	case FocusType::StepNumber:
		Visible[static_cast<int>(CategoryIndex::Step)] = true;
		Visible[static_cast<int>(CategoryIndex::Box)] = true;
		break;

	case FocusType::PartsList:
		Visible[static_cast<int>(CategoryIndex::PartsList)] = true;
		Visible[static_cast<int>(CategoryIndex::Box)] = true;
		break;

	case FocusType::PageNumber:
		Visible[static_cast<int>(CategoryIndex::PageNumber)] = true;
		Visible[static_cast<int>(CategoryIndex::Box)] = true;
		break;

	case FocusType::CoverTitle:
	case FocusType::CoverAuthor:
	case FocusType::CoverImage:
		Visible[static_cast<int>(CategoryIndex::Cover)] = true;
		Visible[static_cast<int>(CategoryIndex::Box)] = true;
		break;

	case FocusType::BillOfMaterials:
		Visible[static_cast<int>(CategoryIndex::BillOfMaterials)] = true;
		Visible[static_cast<int>(CategoryIndex::PartsList)] = true;
		Visible[static_cast<int>(CategoryIndex::Box)] = true;
		break;

	case FocusType::Notice:
		Visible[static_cast<int>(CategoryIndex::Notice)] = true;
		Visible[static_cast<int>(CategoryIndex::Box)] = true;
		break;

	case FocusType::Pointer:
		Visible[static_cast<int>(CategoryIndex::Pointer)] = true;
		break;
	}

	for (int Index = 0; Index < static_cast<int>(Visible.size()); Index++)
	{
		CategoryWidgets& Category = mCategoryWidgets[Index];

		Category.Button->setVisible(Visible[Index]);
		Category.Rows->setVisible(Visible[Index] && Category.Button->IsExpanded());
	}
}
