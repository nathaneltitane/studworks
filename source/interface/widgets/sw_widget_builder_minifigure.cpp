#include "sw_global.h"
#include "sw_widget_builder_minifigure.h"
#include "ui_sw_widget_builder_minifigure.h"
#include "sw_view_widget.h"
#include "sw_widget_picker_color.h"
#include "sw_builder_minifigure.h"
#include "sw_data_part.h"
#include "sw_library.h"
#include "sw_view.h"
#include "sw_model.h"
#include "sw_object_camera.h"
#include "sw_widget_spin_box_double.h"
#include "sw_window_utilities.h"
#include "sw_widget_selection_part_popup.h"
#include "sw_window_main.h"

swMinifigureBuilderWidget::swMinifigureBuilderWidget(QWidget* Parent)
	: QWidget(Parent), ui(new Ui::swMinifigureBuilderWidget)
{
	// the builder adds to the current model
	ui->setupUi(this);

	QGridLayout* MinifigureBuilderLayout = ui->MinifigureBuilderLayout;

	// extra height below the panel
	MinifigureBuilderLayout->setContentsMargins(0, 0, 0, 0);
	ui->verticalLayout->addStretch(1);

	bool HasSpinBox[SW_MINIFIGURE_ITEM_COUNT] = { true, true, true, false, false, false, false, true, true, true, true, true, true, true, true, true, true };
	bool IsLeft[SW_MINIFIGURE_ITEM_COUNT] = { true, true, true, true, false, false, false, false, true, false, true, false, true, false, true, false, true };
	const int NeckIndex = 3;

	QString Labels[SW_MINIFIGURE_ITEM_COUNT] =
	{
		tr("Hat"), tr("Hat Accessory"), tr("Head"), tr("Neck"), tr("Torso"), tr("Hip"), tr("Hip Accessory"), tr("Left Arm"), tr("Right Arm"), tr("Left Hand"), tr("Right Hand"),
		tr("Left Hand Accessory"), tr("Right Hand Accessory"), tr("Left Leg"), tr("Right Leg"), tr("Left Leg Accessory"), tr("Right Leg Accessory")
	};

	std::copy(std::begin(Labels), std::end(Labels), mSectionNames.begin());

	// neck on top, sections beside the preview
	constexpr int FirstSideRow = 1;
	int LeftRow = FirstSideRow, RightRow = FirstSideRow;
	QHBoxLayout* NeckLayout = nullptr;

	for (int ItemIndex = 0; ItemIndex < SW_MINIFIGURE_ITEM_COUNT; ItemIndex++)
	{
		swElidableToolButton* PartButton = new swElidableToolButton(this);
		mPartButtons[ItemIndex] = PartButton;
		PartButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		PartButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
		PartButton->setObjectName(QString("MinifigureBuilderPart%1").arg(ItemIndex));
		PartButton->setAccessibleName(tr("%1 part").arg(Labels[ItemIndex]));
		PartButton->setWhatsThis(tr("<p><b>Choose Part</b></p><p>Choose a part for this section of the minifigure. Select None to omit it, the selector then shows the name of the section. The preview updates when you select a different part.</p>"));

		connect(PartButton, &QToolButton::clicked, this, &swMinifigureBuilderWidget::PartButtonClicked);

		swColorPicker* ColorPicker = new swColorPicker(this);
		mColorPickers[ItemIndex] = ColorPicker;
		ColorPicker->setObjectName(QString("MinifigureBuilderColor%1").arg(ItemIndex));
		ColorPicker->setAccessibleName(tr("%1 color").arg(Labels[ItemIndex]));
		ColorPicker->setToolTip(tr("Choose the color for %1.").arg(Labels[ItemIndex]));
		ColorPicker->setFixedSize(swGetPanelButtonSize());
		ColorPicker->setWhatsThis(tr("<p><b>Choose Color</b></p><p>Change the color of this minifigure part. The preview updates immediately, and saving a template includes the chosen color.</p>"));

		connect(ColorPicker, &swColorPicker::ColorChanged, this, &swMinifigureBuilderWidget::ColorChanged);

		swDoubleSpinBox* SpinBox = HasSpinBox[ItemIndex] ? new swDoubleSpinBox(this) : nullptr;
		mSpinBoxes[ItemIndex] = SpinBox;

		if (SpinBox)
		{
			SpinBox->setRange(-360.0, 360.0);
			SpinBox->SetSnap(swFloatPropertySnap::Rotation);
			SpinBox->setSingleStep(5.0);
			SpinBox->setObjectName(QString("MinifigureBuilderRotation%1").arg(ItemIndex));
			SpinBox->setAccessibleName(tr("%1 rotation").arg(Labels[ItemIndex]));
			SpinBox->setToolTip(tr("Rotate %1, in degrees (-360 to 360).").arg(Labels[ItemIndex]));
			SpinBox->setWhatsThis(tr("<p><b>Rotation</b></p><p>Adjust this part's angle to pose the minifigure. Values range from -360 to 360 degrees. The available rotation depends on the selected minifigure section.</p>"));

			connect(SpinBox, QOverload<double>::of(&swDoubleSpinBox::valueChanged), this, &swMinifigureBuilderWidget::AngleChanged);
		}

		// color, selection and angle per section
		auto AddRow = [MinifigureBuilderLayout, PartButton, ColorPicker, SpinBox](int& Row, int Column, int ColumnCount)
		{
			MinifigureBuilderLayout->addWidget(ColorPicker, Row, Column);
			MinifigureBuilderLayout->addWidget(PartButton, Row, Column + 1, 1, SpinBox ? ColumnCount - 2 : ColumnCount - 1);

			if (SpinBox)
				MinifigureBuilderLayout->addWidget(SpinBox, Row, Column + ColumnCount - 1);

			Row++;
		};

		if (ItemIndex == NeckIndex)
		{
			// neck beside the options button
			MinifigureBuilderLayout->addWidget(ColorPicker, 0, 0);
			NeckLayout = new QHBoxLayout();
			NeckLayout->addWidget(PartButton);
			MinifigureBuilderLayout->addLayout(NeckLayout, 0, 1, 1, 6);
		}
		else if (IsLeft[ItemIndex])
			AddRow(LeftRow, 0, 3);
		else
			AddRow(RightRow, 4, 3);
	}

	MinifigureBuilderLayout->setColumnStretch(1, 1);
	MinifigureBuilderLayout->setColumnStretch(3, 1);
	MinifigureBuilderLayout->setColumnStretch(5, 1);

	// preview as tall as the sections
	QFrame* PreviewFrame = new QFrame(ui->widget);
	PreviewFrame->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Ignored);
	PreviewFrame->setFrameShape(QFrame::NoFrame);
	PreviewFrame->setFrameShadow(QFrame::Plain);

	MinifigureBuilderLayout->addWidget(PreviewFrame, FirstSideRow, 3, qMax(LeftRow, RightRow) - FirstSideRow, 1);

	QGridLayout* PreviewLayout = new QGridLayout(PreviewFrame);
	PreviewLayout->setContentsMargins(0, 0, 0, 0);

	mMinifigureBuilder = new swMinifigureBuilder();
	mView = new swView(swViewType::MinifigureBuilder, mMinifigureBuilder->GetModel());

	swViewWidget* ViewWidget = new swViewWidget(nullptr, mView);
	ViewWidget->setAccessibleName(tr("Minifigure Builder Preview"));
	ViewWidget->setToolTip(tr("Preview of the chosen parts, colors, and pose."));
	ViewWidget->setWhatsThis(tr("<p><b>Minifigure Builder Preview</b></p><p>Check the assembled minifigure as you change parts, colors, or angles. Choose Add to Model to add it to the current model.</p>"));
	ViewWidget->setMinimumWidth(100);
	ViewWidget->installEventFilter(this);
	PreviewLayout->addWidget(ViewWidget);
	mViewWidget = ViewWidget;

	mView->MakeCurrent();

	mCurrentTemplateName = swMinifigureBuilder::GetDefaultTemplateName();
	mMinifigureBuilder->LoadTemplate(mCurrentTemplateName);

	UpdateControls();
	UpdateTemplateCombo();

	mMinifigureBuilder->Calculate();
	mView->GetCamera()->SetViewpoint(swViewpoint::Front);
	mView->ZoomExtents(swGeometryBoundsMode::IncludeFallback, false);

	ui->horizontalLayout_2->setContentsMargins(0, 0, 0, 0);
	ui->TemplateComboBox->setPlaceholderText(tr("Templates"));

	// compact template buttons
	ui->TemplateComboBox->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
	ui->TemplateComboBox->setMinimumContentsLength(8);

	// template buttons in pairs
	QWidget* TemplateButtons = new QWidget(this);
	QGridLayout* TemplateButtonsLayout = new QGridLayout(TemplateButtons);
	TemplateButtonsLayout->setContentsMargins(0, 0, 0, 0);
	ui->verticalLayout->insertWidget(ui->verticalLayout->indexOf(ui->TemplateGroup) + 1, TemplateButtons);

	std::vector<QToolButton*> RowButtons = { ui->TemplateSaveButton, ui->TemplateDeleteButton, ui->TemplateImportButton, ui->TemplateExportButton };

	QToolButton* RandomizeButton = new QToolButton(TemplateButtons);
	RandomizeButton->setText(tr("Randomize"));
	RandomizeButton->setObjectName("MinifigureBuilderRandomize");
	RandomizeButton->setToolTip(tr("Build a random minifigure from the library parts"));
	RandomizeButton->setWhatsThis(tr("<p><b>Randomize</b></p><p>Build a random minifigure from the parts in the current library, with random colors. Choose it again for another one, or save the result as a template.</p>"));
	connect(RandomizeButton, &QToolButton::clicked, this, &swMinifigureBuilderWidget::RandomizeButtonClicked);
	RowButtons.push_back(RandomizeButton);

	QToolButton* AddButton = new QToolButton(TemplateButtons);
	AddButton->setText(tr("Add to Model"));
	AddButton->setToolTip(tr("Add this minifigure to the current model"));
	connect(AddButton, &QToolButton::clicked, this, &swMinifigureBuilderWidget::AddToModel);
	RowButtons.push_back(AddButton);

	for (int ButtonIndex = 0; ButtonIndex < static_cast<int>(RowButtons.size()); ButtonIndex++)
	{
		QToolButton* Button = RowButtons[ButtonIndex];

		TemplateButtonsLayout->addWidget(Button, ButtonIndex / 2, ButtonIndex % 2);

		Button->setToolButtonStyle(Qt::ToolButtonTextOnly);
		swSetupPanelButton(Button);
		Button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		Button->setMinimumWidth(0);
	}

	// options hold preview settings
	QToolButton* OptionsButton = new QToolButton(this);
	OptionsButton->setIcon(swCreatePaintedIcon(swPaintedIcon::Gear));
	OptionsButton->setToolTip(tr("Configure the minifigure builder preview rendering."));
	OptionsButton->setAccessibleName(tr("Minifigure Builder Preview Options"));
	OptionsButton->setPopupMode(QToolButton::InstantPopup);
	swSetupPanelButton(OptionsButton);
	NeckLayout->addWidget(OptionsButton);

	QMenu* OptionsMenu = new QMenu(OptionsButton);
	OptionsButton->setMenu(OptionsMenu);
	connect(OptionsMenu, &QMenu::aboutToShow, this, &swMinifigureBuilderWidget::OptionsMenuAboutToShow);

	connect(ui->TemplateComboBox, QOverload<int>::of(&QComboBox::activated), this, &swMinifigureBuilderWidget::TemplateComboBoxActivated);
	connect(ui->TemplateSaveButton, &QToolButton::clicked, this, &swMinifigureBuilderWidget::TemplateSaveButtonClicked);
	connect(ui->TemplateDeleteButton, &QToolButton::clicked, this, &swMinifigureBuilderWidget::TemplateDeleteButtonClicked);
	connect(ui->TemplateImportButton, &QToolButton::clicked, this, &swMinifigureBuilderWidget::TemplateImportButtonClicked);
	connect(ui->TemplateExportButton, &QToolButton::clicked, this, &swMinifigureBuilderWidget::TemplateExportButtonClicked);
}

void swMinifigureBuilderWidget::AddToModel()
{
	swModel* Model = gMainWindow ? gMainWindow->GetActiveModel() : nullptr;

	if (Model)
		Model->AddMinifigure(mMinifigureBuilder->mMinifigure);
}

bool swMinifigureBuilderWidget::eventFilter(QObject* Object, QEvent* Event)
{
	// refit the figure on resize
	if (Object == mViewWidget && Event->type() == QEvent::Resize)
		QTimer::singleShot(0, this, &swMinifigureBuilderWidget::FitPreview);

	return QWidget::eventFilter(Object, Event);
}

void swMinifigureBuilderWidget::FitPreview()
{
	// keep the whole figure in view
	mView->ZoomExtents(swGeometryBoundsMode::IncludeFallback, false);
	mView->Redraw();
}

void swMinifigureBuilderWidget::UpdatePartButton(int ItemIndex)
{
	QToolButton* PartButton = mPartButtons[ItemIndex];
	const swPartData* PartData = mMinifigureBuilder->mMinifigure.PartData[ItemIndex].Get();
	QString Description;

	for (const swMinifigurePartInformation& PartInfo : mMinifigureBuilder->mSettings[ItemIndex])
	{
		if (PartData && PartInfo.PartData == PartData)
		{
			Description = PartInfo.Description;
			break;
		}
	}

	// parts outside the list show their own name
	if (PartData && Description.isEmpty())
		Description = PartData->m_strDescription;

	// empty sections show their name
	QPalette Palette = PartButton->palette();
	Palette.setColor(QPalette::ButtonText, Description.isEmpty() ? palette().color(QPalette::PlaceholderText) : palette().color(QPalette::ButtonText));
	PartButton->setPalette(Palette);

	PartButton->setText(Description.isEmpty() ? mSectionNames[ItemIndex] : Description);
}

// use to show the builder minifigure in the controls
void swMinifigureBuilderWidget::UpdateControls()
{
	const swMinifigure& Minifigure = mMinifigureBuilder->mMinifigure;

	for (int ItemIndex = 0; ItemIndex < SW_MINIFIGURE_ITEM_COUNT; ItemIndex++)
	{
		UpdatePartButton(ItemIndex);

		swColorPicker* ColorPicker = mColorPickers[ItemIndex];
		QSignalBlocker ColorPickerBlocker(ColorPicker);

		ColorPicker->SetCurrentColor(Minifigure.ColorIndices[ItemIndex]);

		swDoubleSpinBox* AngleSpinBox = mSpinBoxes[ItemIndex];

		if (AngleSpinBox)
		{
			QSignalBlocker AngleBlocker(AngleSpinBox);

			AngleSpinBox->SetValue(Minifigure.Angles[ItemIndex]);
		}
	}
}

// use to rebuild the part lists after a library change
void swMinifigureBuilderWidget::ReloadParts()
{
	mView->MakeCurrent();
	mMinifigureBuilder->ReloadSettings();

	UpdateControls();
	FitPreview();
}

void swMinifigureBuilderWidget::OptionsMenuAboutToShow()
{
	QMenu* Menu = qobject_cast<QMenu*>(sender());
	Menu->clear();

	gMainWindow->AddRenderTargetActions(Menu, swRenderTarget::MinifigureBuilder);
}

swMinifigureBuilderWidget::~swMinifigureBuilderWidget()
{
	delete mMinifigureBuilder;
	delete ui;
}

void swMinifigureBuilderWidget::UpdateTemplateCombo()
{
	QSignalBlocker Blocker(ui->TemplateComboBox);

	ui->TemplateComboBox->clear();

	QString CurrentName = mCurrentTemplateName;

	for (const auto& [Name, Template] : mMinifigureBuilder->GetTemplates())
	{
		if (swMinifigureBuilder::IsBuiltInTemplate(Name))
			continue;

		if (mCurrentTemplateModified && Name == mCurrentTemplateName)
		{
			CurrentName += " *";
			ui->TemplateComboBox->addItem(CurrentName);
		}
		else
			ui->TemplateComboBox->addItem(Name);
	}

	if (ui->TemplateComboBox->count())
		ui->TemplateComboBox->insertSeparator(ui->TemplateComboBox->count());

	// built in templates last
	ui->TemplateComboBox->addItem(tr("Default Template"), swMinifigureBuilder::GetDefaultTemplateName());
	ui->TemplateComboBox->addItem(tr("Empty Minifigure"), swMinifigureBuilder::GetEmptyTemplateName());

	// show the selector placeholder
	if (mCurrentTemplateName.isEmpty())
		ui->TemplateComboBox->setCurrentIndex(-1);
	else if (swMinifigureBuilder::IsBuiltInTemplate(mCurrentTemplateName))
		ui->TemplateComboBox->setCurrentIndex(ui->TemplateComboBox->findData(mCurrentTemplateName));
	else
		ui->TemplateComboBox->setCurrentText(CurrentName);
}

void swMinifigureBuilderWidget::TemplateComboBoxActivated(int Index)
{
	// picking the same entry again reloads it
	const QVariant BuiltInName = ui->TemplateComboBox->itemData(Index);
	QString TemplateName = BuiltInName.isValid() ? BuiltInName.toString() : ui->TemplateComboBox->itemText(Index);

	if (mCurrentTemplateModified && TemplateName == mCurrentTemplateName + " *")
		TemplateName = mCurrentTemplateName;

	if (!mMinifigureBuilder->GetTemplates().count(TemplateName))
		return;

	mCurrentTemplateName = TemplateName;
	mCurrentTemplateModified = false;

	UpdateTemplateCombo();

	mView->MakeCurrent();
	mMinifigureBuilder->LoadTemplate(TemplateName);

	UpdateControls();
	FitPreview();
}

void swMinifigureBuilderWidget::RandomizeButtonClicked()
{
	mCurrentTemplateName.clear();
	mCurrentTemplateModified = false;

	UpdateTemplateCombo();

	mView->MakeCurrent();
	mMinifigureBuilder->Randomize();

	UpdateControls();
	FitPreview();
}

void swMinifigureBuilderWidget::TemplateSaveButtonClicked()
{
	QString CurrentName = mCurrentTemplateName;
	bool Ok;
	QString TemplateName = QInputDialog::getText(this, tr("Save Template"), tr("Template Name:"), QLineEdit::Normal, CurrentName, &Ok);

	if (!Ok)
		return;

	if (TemplateName.isEmpty())
	{
		QMessageBox::information(this, tr("Save Template"), tr("Template name cannot be empty."));
		return;
	}

	if (swMinifigureBuilder::IsBuiltInTemplate(TemplateName))
	{
		QMessageBox::information(this, tr("Save Template"), tr("Cannot overwrite a built in template."));
		return;
	}

	if (mMinifigureBuilder->GetTemplates().count(TemplateName))
	{
		QString Question = tr("Are you sure you want to overwrite the template '%1'?").arg(TemplateName);
		if (QMessageBox::question(this, tr("Overwrite Template"), Question, QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
			return;
	}

	mMinifigureBuilder->SaveTemplate(TemplateName, mMinifigureBuilder->GetCurrentTemplate());

	mCurrentTemplateName = TemplateName;
	mCurrentTemplateModified = false;

	UpdateTemplateCombo();
}

void swMinifigureBuilderWidget::TemplateDeleteButtonClicked()
{
	if (mCurrentTemplateName.isEmpty() || swMinifigureBuilder::IsBuiltInTemplate(mCurrentTemplateName))
		return;

	QString Template = mCurrentTemplateName;
	QString Question = tr("Are you sure you want to delete the template '%1'?").arg(Template);

	if (QMessageBox::question(this, tr("Delete Template"), Question, QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
		return;

	mMinifigureBuilder->DeleteTemplate(Template);

	mCurrentTemplateName.clear();
	mCurrentTemplateModified = false;

	UpdateTemplateCombo();
}

void swMinifigureBuilderWidget::TemplateImportButtonClicked()
{
	QString FileName = QFileDialog::getOpenFileName(this, tr("Import Templates"), "", tr("Minifigure Template Files (*.minifig);;All Files (*.*)"));

	if (FileName.isEmpty())
		return;

	QFile File(FileName);

	if (!File.open(QIODevice::ReadOnly))
	{
		QMessageBox::warning(this, tr("Import Templates"), tr("Error reading file '%1':\n%2").arg(FileName, File.errorString()));
		return;
	}

	QByteArray FileData = File.readAll();
	mMinifigureBuilder->AddTemplatesJson(FileData);

	UpdateTemplateCombo();
}

void swMinifigureBuilderWidget::TemplateExportButtonClicked()
{
	QString FileName = QFileDialog::getSaveFileName(this, tr("Export Templates"), "", tr("Minifigure Template Files (*.minifig);;All Files (*.*)"));

	if (FileName.isEmpty())
		return;

	QFile File(FileName);

	if (!File.open(QIODevice::WriteOnly))
	{
		QMessageBox::warning(this, tr("Export Templates"), tr("Error writing to file '%1':\n%2").arg(FileName, File.errorString()));
		return;
	}

	QByteArray Templates = mMinifigureBuilder->GetTemplatesJson();
	File.write(Templates);
}

void swMinifigureBuilderWidget::SetCurrentTemplateModified()
{
	if (swMinifigureBuilder::IsBuiltInTemplate(mCurrentTemplateName))
		mCurrentTemplateName.clear();
	else
		mCurrentTemplateModified = true;
}

void swMinifigureBuilderWidget::PartButtonClicked()
{
	QToolButton* PartButton = qobject_cast<QToolButton*>(sender());

	if (!PartButton)
		return;

	std::array<QToolButton*, SW_MINIFIGURE_ITEM_COUNT>::iterator Search = std::find(mPartButtons.begin(), mPartButtons.end(), PartButton);

	if (Search == mPartButtons.end())
		return;

	int ItemIndex = static_cast<int>(std::distance(mPartButtons.begin(), Search));
	swPartData* CurrentPartData = mMinifigureBuilder->mMinifigure.PartData[ItemIndex].Get();

	QPoint Position = PartButton->mapToGlobal(PartButton->rect().bottomLeft());
	std::vector<std::pair<swPartData*, std::string>> Parts;

	Parts.reserve(mMinifigureBuilder->mSettings[ItemIndex].size());
	Parts.emplace_back(nullptr, tr("None").toStdString());

	for (const swMinifigurePartInformation& Setting : mMinifigureBuilder->mSettings[ItemIndex])
		if (Setting.PartData)
			Parts.emplace_back(Setting.PartData, Setting.Description);

	int ColorIndex = mMinifigureBuilder->mMinifigure.ColorIndices[ItemIndex];

	std::optional<swPartData*> Result = swShowPartSelectionPopup(CurrentPartData, Parts, ColorIndex, PartButton, Position);

	if (!Result.has_value())
		return;

	swPartData* NewPart = Result.value();

	SetCurrentTemplateModified();
	UpdateTemplateCombo();

	mView->MakeCurrent();
	mMinifigureBuilder->SetPartData(ItemIndex, NewPart);
	FitPreview();

	UpdatePartButton(ItemIndex);
}

void swMinifigureBuilderWidget::ColorChanged(int Index)
{
	std::array<swColorPicker*, SW_MINIFIGURE_ITEM_COUNT>::iterator Search = std::find(mColorPickers.begin(), mColorPickers.end(), sender());

	if (Search == mColorPickers.end())
		return;

	SetCurrentTemplateModified();
	UpdateTemplateCombo();

	int ItemIndex = static_cast<int>(std::distance(mColorPickers.begin(), Search));

	mMinifigureBuilder->SetColorIndex(ItemIndex, Index);
	mView->Redraw();
}

void swMinifigureBuilderWidget::AngleChanged(double Value)
{
	std::array<swDoubleSpinBox*, SW_MINIFIGURE_ITEM_COUNT>::iterator Search = std::find(mSpinBoxes.begin(), mSpinBoxes.end(), sender());

	if (Search == mSpinBoxes.end())
		return;

	SetCurrentTemplateModified();
	UpdateTemplateCombo();

	int ItemIndex = static_cast<int>(std::distance(mSpinBoxes.begin(), Search));

	mMinifigureBuilder->SetAngle(ItemIndex, Value);
	FitPreview();
}
