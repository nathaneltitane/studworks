#include "sw_global.h"
#include "sw_minifigure_builder_widget.h"
#include "ui_sw_minifigure_builder_widget.h"
#include "sw_view_widget.h"
#include "sw_color_picker.h"
#include "sw_minifigure_builder.h"
#include "sw_piece_data.h"
#include "sw_library.h"
#include "sw_view.h"
#include "sw_model.h"
#include "sw_camera.h"
#include "sw_double_spin_box.h"
#include "sw_utilities.h"
#include "sw_part_selection_popup.h"
#include "sw_main_window.h"

swMinifigureBuilderWidget::swMinifigureBuilderWidget(QWidget* Parent)
	: QWidget(Parent), ui(new Ui::swMinifigureBuilderWidget)
{
	// The Minifigure Builder panel stays open and adds each minifigure to the current model, F1 help comes from the panel.
	ui->setupUi(this);

	QGridLayout* MinifigureBuilderLayout = ui->MinifigureBuilderLayout;

	// Extra height goes below the panel instead of between rows.
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

	// The neck spans the top, the other sections stack on both sides of the preview.
	constexpr int FirstSideRow = 1;
	int LeftRow = FirstSideRow, RightRow = FirstSideRow;
	QHBoxLayout* NeckLayout = nullptr;

	for (int ItemIndex = 0; ItemIndex < SW_MINIFIGURE_ITEM_COUNT; ItemIndex++)
	{
		swElidableToolButton* PieceButton = new swElidableToolButton(this);
		mPieceButtons[ItemIndex] = PieceButton;
		PieceButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		PieceButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
		PieceButton->setObjectName(QString("MinifigureBuilderPiece%1").arg(ItemIndex));
		PieceButton->setAccessibleName(tr("%1 piece").arg(Labels[ItemIndex]));
		PieceButton->setWhatsThis(tr("<p><b>Choose Piece</b></p><p>Choose a piece for this section of the minifigure. Select None to omit it, the selector then shows the name of the section. The preview updates when you select a different piece.</p>"));

		connect(PieceButton, &QToolButton::clicked, this, &swMinifigureBuilderWidget::PieceButtonClicked);

		swColorPicker* ColorPicker = new swColorPicker(this);
		mColorPickers[ItemIndex] = ColorPicker;
		ColorPicker->setObjectName(QString("MinifigureBuilderColor%1").arg(ItemIndex));
		ColorPicker->setAccessibleName(tr("%1 color").arg(Labels[ItemIndex]));
		ColorPicker->setToolTip(tr("Choose the color for %1.").arg(Labels[ItemIndex]));
		ColorPicker->setWhatsThis(tr("<p><b>Choose Color</b></p><p>Change the color of this minifigure piece. The preview updates immediately, and saving a template includes the chosen color.</p>"));

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
			SpinBox->setWhatsThis(tr("<p><b>Rotation</b></p><p>Adjust this piece's angle to pose the minifigure. Values range from -360 to 360 degrees. The available rotation depends on the selected minifigure section.</p>"));

			connect(SpinBox, QOverload<double>::of(&swDoubleSpinBox::valueChanged), this, &swMinifigureBuilderWidget::AngleChanged);
		}

		// Each section is [Color] [Selection] [Angle], the selection takes the space left over and names the section when empty.
		auto AddRow = [MinifigureBuilderLayout, PieceButton, ColorPicker, SpinBox](int& Row, int Column, int ColumnCount)
		{
			MinifigureBuilderLayout->addWidget(ColorPicker, Row, Column);
			MinifigureBuilderLayout->addWidget(PieceButton, Row, Column + 1, 1, SpinBox ? ColumnCount - 2 : ColumnCount - 1);

			if (SpinBox)
				MinifigureBuilderLayout->addWidget(SpinBox, Row, Column + ColumnCount - 1);

			Row++;
		};

		if (ItemIndex == NeckIndex)
		{
			// the neck shares the top row with the options button in the top right corner
			MinifigureBuilderLayout->addWidget(ColorPicker, 0, 0);
			NeckLayout = new QHBoxLayout();
			NeckLayout->addWidget(PieceButton);
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

	// The preview is as tall as the stacked section rows beside it.
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
	ViewWidget->setToolTip(tr("Preview of the chosen pieces, colors, and pose."));
	ViewWidget->setWhatsThis(tr("<p><b>Minifigure Builder Preview</b></p><p>Check the assembled minifigure as you change pieces, colors, or angles. Choose Add to Model to add it to the current model.</p>"));
	ViewWidget->setMinimumWidth(100);
	ViewWidget->installEventFilter(this);
	PreviewLayout->addWidget(ViewWidget);
	mViewWidget = ViewWidget;

	mView->MakeCurrent();

	mCurrentTemplateName = swMinifigureBuilder::GetDefaultTemplateName();
	mMinifigureBuilder->LoadTemplate(mCurrentTemplateName);

	for (int ItemIndex = 0; ItemIndex < SW_MINIFIGURE_ITEM_COUNT; ItemIndex++)
	{
		UpdatePieceButton(ItemIndex);

		swColorPicker* ColorPicker = mColorPickers[ItemIndex];
		QSignalBlocker ColorPickerBlocker(ColorPicker);

		ColorPicker->SetCurrentColor(mMinifigureBuilder->mMinifigure.ColorIndices[ItemIndex]);
	}

	UpdateTemplateCombo();

	mMinifigureBuilder->Calculate();
	mView->GetCamera()->SetViewpoint(swViewpoint::Front);
	mView->ZoomExtents(swGeometryBoundsMode::IncludeFallback, false);

	ui->horizontalLayout_2->setContentsMargins(0, 0, 0, 0);
	ui->TemplateComboBox->setPlaceholderText(tr("Templates"));

	// The template row uses the compact panel buttons so it fits beside Add to Model.
	ui->TemplateComboBox->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
	ui->TemplateComboBox->setMinimumContentsLength(8);

	// The template buttons and Add to Model share a row below the selector and split its width.
	QWidget* TemplateButtons = new QWidget(this);
	QHBoxLayout* TemplateButtonsLayout = new QHBoxLayout(TemplateButtons);
	TemplateButtonsLayout->setContentsMargins(0, 0, 0, 0);
	ui->verticalLayout->insertWidget(ui->verticalLayout->indexOf(ui->TemplateGroup) + 1, TemplateButtons);

	std::vector<QToolButton*> RowButtons = { ui->TemplateSaveButton, ui->TemplateDeleteButton, ui->TemplateImportButton, ui->TemplateExportButton };

	QToolButton* AddButton = new QToolButton(TemplateButtons);
	AddButton->setText(tr("Add to Model"));
	AddButton->setToolTip(tr("Add this minifigure to the current model"));
	connect(AddButton, &QToolButton::clicked, this, &swMinifigureBuilderWidget::AddToModel);
	RowButtons.push_back(AddButton);

	for (QToolButton* Button : RowButtons)
	{
		TemplateButtonsLayout->addWidget(Button);
		Button->setToolButtonStyle(Qt::ToolButtonTextOnly);
		swSetupPanelButton(Button);
		Button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		Button->setMinimumWidth(0);
	}

	// The options button holds the preview rendering settings, like the other preview panels.
	QToolButton* OptionsButton = new QToolButton(this);
	OptionsButton->setIcon(QIcon(":/resources/interface/widgets/gear_in.png"));
	OptionsButton->setToolTip(tr("Configure the minifigure builder preview rendering."));
	OptionsButton->setAccessibleName(tr("Minifigure Builder Preview Options"));
	OptionsButton->setPopupMode(QToolButton::InstantPopup);
	swSetupPanelButton(OptionsButton);
	NeckLayout->addWidget(OptionsButton);

	QMenu* OptionsMenu = new QMenu(OptionsButton);
	OptionsButton->setMenu(OptionsMenu);
	connect(OptionsMenu, &QMenu::aboutToShow, this, &swMinifigureBuilderWidget::OptionsMenuAboutToShow);

	connect(ui->TemplateComboBox, &QComboBox::currentTextChanged, this, &swMinifigureBuilderWidget::TemplateComboBoxCurrentTextChanged);
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
	// Fit the figure again whenever the preview changes size so it is never cut off.
	if (Object == mViewWidget && Event->type() == QEvent::Resize)
		QTimer::singleShot(0, this, &swMinifigureBuilderWidget::FitPreview);

	return QWidget::eventFilter(Object, Event);
}

void swMinifigureBuilderWidget::FitPreview()
{
	// The whole figure stays in view, whatever pieces and angles are chosen.
	mView->ZoomExtents(swGeometryBoundsMode::IncludeFallback, false);
	mView->Redraw();
}

void swMinifigureBuilderWidget::UpdatePieceButton(int ItemIndex)
{
	QToolButton* PieceButton = mPieceButtons[ItemIndex];
	const swPieceData* PieceData = mMinifigureBuilder->mMinifigure.PieceData[ItemIndex].Get();
	QString Description;

	for (const swMinifigurePieceInformation& PieceInfo : mMinifigureBuilder->mSettings[ItemIndex])
	{
		if (PieceData && PieceInfo.PieceData == PieceData)
		{
			Description = PieceInfo.Description;
			break;
		}
	}

	// An empty section shows its name like a placeholder, a chosen piece shows its full name in the tool tip.
	QPalette Palette = PieceButton->palette();
	Palette.setColor(QPalette::ButtonText, Description.isEmpty() ? palette().color(QPalette::PlaceholderText) : palette().color(QPalette::ButtonText));
	PieceButton->setPalette(Palette);

	PieceButton->setText(Description.isEmpty() ? mSectionNames[ItemIndex] : Description);
	PieceButton->setToolTip(Description.isEmpty() ? tr("Choose the piece for %1, or None to omit it.").arg(mSectionNames[ItemIndex]) : tr("%1: %2").arg(mSectionNames[ItemIndex], Description));
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
		if (Name == swMinifigureBuilder::GetDefaultTemplateName())
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
	// The built-in template keeps its stored name and is shown as Default Template.
	ui->TemplateComboBox->addItem(tr("Default Template"));

	if (CurrentName == swMinifigureBuilder::GetDefaultTemplateName())
		CurrentName = tr("Default Template");

	// Without a current template the selector shows its name as a placeholder.
	if (mCurrentTemplateName.isEmpty())
		ui->TemplateComboBox->setCurrentIndex(-1);
	else
		ui->TemplateComboBox->setCurrentText(CurrentName);
}

void swMinifigureBuilderWidget::TemplateComboBoxCurrentTextChanged(const QString& DisplayName)
{
	const QString TemplateName = DisplayName == tr("Default Template") ? swMinifigureBuilder::GetDefaultTemplateName() : DisplayName;
	const auto& Templates = mMinifigureBuilder->GetTemplates();
	const auto& Position = Templates.find(TemplateName);

	if (Position == Templates.end())
		return;

	mCurrentTemplateName = TemplateName;
	mCurrentTemplateModified = false;

	UpdateTemplateCombo();

	const swMinifigureTemplate& Template = Position->second;

	mView->MakeCurrent();

	mMinifigureBuilder->LoadTemplate(TemplateName);

	for (int PartIdx = 0; PartIdx < SW_MINIFIGURE_ITEM_COUNT; PartIdx++)
	{
		UpdatePieceButton(PartIdx);

		QSignalBlocker ColorBlocker(mColorPickers[PartIdx]);

		mColorPickers[PartIdx]->SetCurrentColorCode(Template.ColorCodes[PartIdx]);

		swDoubleSpinBox* AngleSpinBox = mSpinBoxes[PartIdx];

		if (AngleSpinBox)
		{
			QSignalBlocker AngleBlocker(AngleSpinBox);

			AngleSpinBox->SetValue(Template.Angles[PartIdx]);
		}
	}

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

	if (TemplateName == swMinifigureBuilder::GetDefaultTemplateName())
	{
		QMessageBox::information(this, tr("Save Template"), tr("Cannot overwrite the default template."));
		return;
	}

	if (mMinifigureBuilder->GetTemplates().count(TemplateName))
	{
		QString Question = tr("Are you sure you want to overwrite the template '%1'?").arg(TemplateName);
		if (QMessageBox::question(this, tr("Overwrite Template"), Question, QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
			return;
	}

	swMinifigureTemplate Template;

	for (int ItemIndex = 0; ItemIndex < SW_MINIFIGURE_ITEM_COUNT; ItemIndex++)
	{
		Template.Parts[ItemIndex] = mMinifigureBuilder->mSettings[ItemIndex][mMinifigureBuilder->GetSelectionIndex(ItemIndex)].PieceData->mFileName;
		Template.ColorCodes[ItemIndex] = mColorPickers[ItemIndex]->GetCurrentColorCode();
		QDoubleSpinBox* AngleSpinBox = mSpinBoxes[ItemIndex];
		Template.Angles[ItemIndex] = AngleSpinBox ? AngleSpinBox->value() : 0.0f;
	}

	mMinifigureBuilder->SaveTemplate(TemplateName, Template);

	mCurrentTemplateName = TemplateName;
	mCurrentTemplateModified = false;

	UpdateTemplateCombo();
}

void swMinifigureBuilderWidget::TemplateDeleteButtonClicked()
{
	if (mCurrentTemplateName.isEmpty() || mCurrentTemplateName == swMinifigureBuilder::GetDefaultTemplateName())
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
	if (mCurrentTemplateName == swMinifigureBuilder::GetDefaultTemplateName())
		mCurrentTemplateName.clear();
	else
		mCurrentTemplateModified = true;
}

void swMinifigureBuilderWidget::PieceButtonClicked()
{
	QToolButton* PieceButton = qobject_cast<QToolButton*>(sender());

	if (!PieceButton)
		return;

	std::array<QToolButton*, SW_MINIFIGURE_ITEM_COUNT>::iterator Search = std::find(mPieceButtons.begin(), mPieceButtons.end(), PieceButton);

	if (Search == mPieceButtons.end())
		return;

	int ItemIndex = static_cast<int>(std::distance(mPieceButtons.begin(), Search));
	swPieceData* CurrentPieceData = mMinifigureBuilder->mMinifigure.PieceData[ItemIndex].Get();

	QPoint Position = PieceButton->mapToGlobal(PieceButton->rect().bottomLeft());
	std::vector<std::pair<swPieceData*, std::string>> Parts;

	Parts.reserve(mMinifigureBuilder->mSettings[ItemIndex].size());
	Parts.emplace_back(nullptr, tr("None").toStdString());

	for (const swMinifigurePieceInformation& Setting : mMinifigureBuilder->mSettings[ItemIndex])
		if (Setting.PieceData)
			Parts.emplace_back(Setting.PieceData, Setting.Description);

	int ColorIndex = mMinifigureBuilder->mMinifigure.ColorIndices[ItemIndex];

	std::optional<swPieceData*> Result = swShowPartSelectionPopup(CurrentPieceData, Parts, ColorIndex, PieceButton, Position);

	if (!Result.has_value())
		return;

	swPieceData* NewPiece = Result.value();

	SetCurrentTemplateModified();
	UpdateTemplateCombo();

	mView->MakeCurrent();
	mMinifigureBuilder->SetPieceData(ItemIndex, NewPiece);
	FitPreview();

	UpdatePieceButton(ItemIndex);
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
