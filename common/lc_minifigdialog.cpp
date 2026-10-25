#include "lc_global.h"
#include "lc_minifigdialog.h"
#include "ui_lc_minifigdialog.h"
#include "lc_viewwidget.h"
#include "lc_colorpicker.h"
#include "minifig.h"
#include "lc_piecedata.h"
#include "lc_library.h"
#include "lc_view.h"
#include "lc_model.h"
#include "camera.h"
#include "lc_doublespinbox.h"
#include "lc_qutils.h"
#include "lc_partselectionpopup.h"
#include "lc_mainwindow.h"

lcMinifigDialog::lcMinifigDialog(QWidget* Parent, bool Docked)
	: QDialog(Parent), ui(new Ui::lcMinifigDialog), mDocked(Docked)
{
	ui->setupUi(this);

	// docked in a panel the dialog stays open and adds each minifigure to the current model
	if (mDocked)
	{
		setWindowFlags(Qt::Widget);
		ui->buttonBox->setStandardButtons(QDialogButtonBox::NoButton);

		ui->buttonBox->hide();
	}

	// Help comes from What's This and F1 instead of a help button.
	lcSetupDialogHelp(this, nullptr);

	if (!mDocked)
		setWindowFlag(Qt::WindowContextHelpButtonHint, true);

	QGridLayout* MinifigLayout = ui->MinifigLayout;

	// Rows sit close together and extra height goes below the panel instead of between rows.
	MinifigLayout->setContentsMargins(0, 0, 0, 0);
	MinifigLayout->setVerticalSpacing(2);
	ui->verticalLayout->addStretch(1);

	bool HasSpinBox[LC_MFW_NUMITEMS] = { true, true, true, false, false, false, false, true, true, true, true, true, true, true, true, true, true };
	bool IsLeft[LC_MFW_NUMITEMS] = { true, true, true, true, false, false, false, false, true, false, true, false, true, false, true, false, true };
	const int NeckIndex = 3;

	QString Labels[LC_MFW_NUMITEMS] =
	{
		tr("Hat"), tr("Hat Accessory"), tr("Head"), tr("Neck"), tr("Torso"), tr("Hip"), tr("Hip Accessory"), tr("Left Arm"), tr("Right Arm"), tr("Left Hand"), tr("Right Hand"),
		tr("Left Hand Accessory"), tr("Right Hand Accessory"), tr("Left Leg"), tr("Right Leg"), tr("Left Leg Accessory"), tr("Right Leg Accessory")
	};

	std::copy(std::begin(Labels), std::end(Labels), mSectionNames.begin());

	// The neck spans the top, the other sections stack on both sides of the preview.
	constexpr int FirstSideRow = 1;
	int LeftRow = FirstSideRow, RightRow = FirstSideRow;

	for (int ItemIndex = 0; ItemIndex < LC_MFW_NUMITEMS; ItemIndex++)
	{
		lcElidableToolButton* PieceButton = new lcElidableToolButton(this);
		mPieceButtons[ItemIndex] = PieceButton;
		PieceButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		PieceButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
		PieceButton->setObjectName(QString("MinifigPiece%1").arg(ItemIndex));
		PieceButton->setAccessibleName(tr("%1 piece").arg(Labels[ItemIndex]));
		PieceButton->setWhatsThis(tr("<p><b>Choose Piece</b></p><p>Choose a piece for this section of the minifigure. Select None to omit it, the selector then shows the name of the section. The preview updates when you select a different piece.</p>"));

		connect(PieceButton, &QToolButton::clicked, this, &lcMinifigDialog::PieceButtonClicked);

		lcColorPicker* ColorPicker = new lcColorPicker(this);
		mColorPickers[ItemIndex] = ColorPicker;
		ColorPicker->setObjectName(QString("MinifigColor%1").arg(ItemIndex));
		ColorPicker->setAccessibleName(tr("%1 color").arg(Labels[ItemIndex]));
		ColorPicker->setToolTip(tr("Choose the color for %1.").arg(Labels[ItemIndex]));
		ColorPicker->setWhatsThis(tr("<p><b>Choose Color</b></p><p>Change the color of this minifigure piece. The preview updates immediately, and saving a template includes the chosen color.</p>"));

		connect(ColorPicker, &lcColorPicker::ColorChanged, this, &lcMinifigDialog::ColorChanged);

		lcDoubleSpinBox* SpinBox = HasSpinBox[ItemIndex] ? new lcDoubleSpinBox(this) : nullptr;
		mSpinBoxes[ItemIndex] = SpinBox;

		if (SpinBox)
		{
			SpinBox->setRange(-360.0, 360.0);
			SpinBox->SetSnap(lcFloatPropertySnap::Rotation);
			SpinBox->setSingleStep(5.0);
			SpinBox->setObjectName(QString("MinifigRotation%1").arg(ItemIndex));
			SpinBox->setAccessibleName(tr("%1 rotation").arg(Labels[ItemIndex]));
			SpinBox->setToolTip(tr("Rotate %1, in degrees (-360 to 360).").arg(Labels[ItemIndex]));
			SpinBox->setWhatsThis(tr("<p><b>Rotation</b></p><p>Adjust this piece's angle to pose the minifigure. Values range from -360 to 360 degrees. The available rotation depends on the selected minifigure section.</p>"));

			connect(SpinBox, QOverload<double>::of(&lcDoubleSpinBox::valueChanged), this, &lcMinifigDialog::AngleChanged);
		}

		// Each section is [Color] [Selection] [Angle], the selection takes the space left over and names the section when empty.
		auto AddRow = [MinifigLayout, PieceButton, ColorPicker, SpinBox](int& Row, int Column, int ColumnCount)
		{
			MinifigLayout->addWidget(ColorPicker, Row, Column);
			MinifigLayout->addWidget(PieceButton, Row, Column + 1, 1, SpinBox ? ColumnCount - 2 : ColumnCount - 1);

			if (SpinBox)
				MinifigLayout->addWidget(SpinBox, Row, Column + ColumnCount - 1);

			Row++;
		};

		if (ItemIndex == NeckIndex)
		{
			int NeckRow = 0;
			AddRow(NeckRow, 0, 7);
		}
		else if (IsLeft[ItemIndex])
			AddRow(LeftRow, 0, 3);
		else
			AddRow(RightRow, 4, 3);
	}

	MinifigLayout->setColumnStretch(1, 1);
	MinifigLayout->setColumnStretch(3, 1);
	MinifigLayout->setColumnStretch(5, 1);

	// The preview is as tall as the stacked section rows beside it.
	QFrame* PreviewFrame = new QFrame(ui->widget);
	PreviewFrame->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Ignored);
	PreviewFrame->setFrameShape(QFrame::NoFrame);
	PreviewFrame->setFrameShadow(QFrame::Plain);

	MinifigLayout->addWidget(PreviewFrame, FirstSideRow, 3, qMax(LeftRow, RightRow) - FirstSideRow, 1);

	QGridLayout* PreviewLayout = new QGridLayout(PreviewFrame);
	PreviewLayout->setContentsMargins(0, 0, 0, 0);

	mMinifigWizard = new MinifigWizard();
	mView = new lcView(lcViewType::Minifig, mMinifigWizard->GetModel());

	lcViewWidget* ViewWidget = new lcViewWidget(nullptr, mView);
	ViewWidget->setAccessibleName(tr("Minifigure Preview"));
	ViewWidget->setToolTip(tr("Preview of the chosen pieces, colors, and pose."));
	ViewWidget->setWhatsThis(tr("<p><b>Minifigure Preview</b></p><p>Check the assembled minifigure as you change pieces, colors, or angles. Choose OK to add this configuration to the current model.</p>"));
	ViewWidget->setMinimumWidth(100);
	ViewWidget->installEventFilter(this);
	PreviewLayout->addWidget(ViewWidget);
	mViewWidget = ViewWidget;

	mView->MakeCurrent();

	mCurrentTemplateName = MinifigWizard::GetDefaultTemplateName();
	mMinifigWizard->LoadTemplate(mCurrentTemplateName);

	for (int ItemIndex = 0; ItemIndex < LC_MFW_NUMITEMS; ItemIndex++)
	{
		UpdatePieceButton(ItemIndex);

		lcColorPicker* ColorPicker = mColorPickers[ItemIndex];
		QSignalBlocker ColorPickerBlocker(ColorPicker);

		ColorPicker->SetCurrentColor(mMinifigWizard->mMinifig.ColorIndices[ItemIndex]);
	}

	UpdateTemplateCombo();

	mMinifigWizard->Calculate();
	mView->GetCamera()->SetViewpoint(lcViewpoint::Front);
	mView->ZoomExtents(lcGeometryBoundsMode::IncludeFallback, false);

	if (QPushButton* OkButton = ui->buttonBox->button(QDialogButtonBox::StandardButton::Ok))
		OkButton->setFocus();

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

	if (mDocked)
	{
		QToolButton* AddButton = new QToolButton(TemplateButtons);
		AddButton->setText(tr("Add to Model"));
		AddButton->setToolTip(tr("Add this minifigure to the current model"));
		connect(AddButton, &QToolButton::clicked, this, &lcMinifigDialog::accept);
		RowButtons.push_back(AddButton);
	}

	for (QToolButton* Button : RowButtons)
	{
		TemplateButtonsLayout->addWidget(Button);
		Button->setToolButtonStyle(Qt::ToolButtonTextOnly);
		lcSetupPanelButton(Button);
		Button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		Button->setMinimumWidth(0);
	}

	// The options button holds the preview rendering settings, like the other preview panels.
	QToolButton* OptionsButton = new QToolButton(ui->TemplateGroup);
	OptionsButton->setIcon(QIcon(":/resources/gear_in.png"));
	OptionsButton->setToolTip(tr("Configure the minifigure preview rendering."));
	OptionsButton->setAccessibleName(tr("Minifigure Preview Options"));
	OptionsButton->setPopupMode(QToolButton::InstantPopup);
	lcSetupPanelButton(OptionsButton);
	ui->horizontalLayout_2->addWidget(OptionsButton);

	QMenu* OptionsMenu = new QMenu(OptionsButton);
	OptionsButton->setMenu(OptionsMenu);
	connect(OptionsMenu, &QMenu::aboutToShow, this, &lcMinifigDialog::OptionsMenuAboutToShow);

	connect(ui->TemplateComboBox, &QComboBox::currentTextChanged, this, &lcMinifigDialog::TemplateComboBoxCurrentTextChanged);
	connect(ui->TemplateSaveButton, &QToolButton::clicked, this, &lcMinifigDialog::TemplateSaveButtonClicked);
	connect(ui->TemplateDeleteButton, &QToolButton::clicked, this, &lcMinifigDialog::TemplateDeleteButtonClicked);
	connect(ui->TemplateImportButton, &QToolButton::clicked, this, &lcMinifigDialog::TemplateImportButtonClicked);
	connect(ui->TemplateExportButton, &QToolButton::clicked, this, &lcMinifigDialog::TemplateExportButtonClicked);
}

void lcMinifigDialog::accept()
{
	if (!mDocked)
	{
		QDialog::accept();
		return;
	}

	lcModel* Model = gMainWindow ? gMainWindow->GetActiveModel() : nullptr;

	if (Model)
		Model->AddMinifig(mMinifigWizard->mMinifig);
}

bool lcMinifigDialog::eventFilter(QObject* Object, QEvent* Event)
{
	// Fit the figure again whenever the preview changes size so it is never cut off.
	if (Object == mViewWidget && Event->type() == QEvent::Resize)
		QTimer::singleShot(0, this, &lcMinifigDialog::FitPreview);

	return QDialog::eventFilter(Object, Event);
}

void lcMinifigDialog::FitPreview()
{
	// The whole figure stays in view, whatever pieces and angles are chosen.
	mView->ZoomExtents(lcGeometryBoundsMode::IncludeFallback, false);
	mView->Redraw();
}

void lcMinifigDialog::UpdatePieceButton(int ItemIndex)
{
	QToolButton* PieceButton = mPieceButtons[ItemIndex];
	const lcPieceData* PieceData = mMinifigWizard->mMinifig.PieceData[ItemIndex].Get();
	QString Description;

	for (const lcMinifigPieceInfo& PieceInfo : mMinifigWizard->mSettings[ItemIndex])
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

void lcMinifigDialog::OptionsMenuAboutToShow()
{
	QMenu* Menu = qobject_cast<QMenu*>(sender());
	Menu->clear();

	gMainWindow->AddRenderTargetActions(Menu, lcRenderTarget::Minifig);
}

void lcMinifigDialog::reject()
{
	if (!mDocked)
		QDialog::reject();
}

lcMinifigDialog::~lcMinifigDialog()
{
	delete mMinifigWizard;
	delete ui;
}

void lcMinifigDialog::UpdateTemplateCombo()
{
	QSignalBlocker Blocker(ui->TemplateComboBox);

	ui->TemplateComboBox->clear();

	QString CurrentName = mCurrentTemplateName;

	for (const auto& [Name, Template] : mMinifigWizard->GetTemplates())
	{
		if (Name == MinifigWizard::GetDefaultTemplateName())
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

	if (CurrentName == MinifigWizard::GetDefaultTemplateName())
		CurrentName = tr("Default Template");

	// Without a current template the selector shows its name as a placeholder.
	if (mCurrentTemplateName.isEmpty())
		ui->TemplateComboBox->setCurrentIndex(-1);
	else
		ui->TemplateComboBox->setCurrentText(CurrentName);
}

void lcMinifigDialog::TemplateComboBoxCurrentTextChanged(const QString& DisplayName)
{
	const QString TemplateName = DisplayName == tr("Default Template") ? MinifigWizard::GetDefaultTemplateName() : DisplayName;
	const auto& Templates = mMinifigWizard->GetTemplates();
	const auto& Position = Templates.find(TemplateName);

	if (Position == Templates.end())
		return;

	mCurrentTemplateName = TemplateName;
	mCurrentTemplateModified = false;

	UpdateTemplateCombo();

	const lcMinifigTemplate& Template = Position->second;

	mView->MakeCurrent();

	mMinifigWizard->LoadTemplate(TemplateName);

	for (int PartIdx = 0; PartIdx < LC_MFW_NUMITEMS; PartIdx++)
	{
		UpdatePieceButton(PartIdx);

		QSignalBlocker ColorBlocker(mColorPickers[PartIdx]);

		mColorPickers[PartIdx]->SetCurrentColorCode(Template.ColorCodes[PartIdx]);

		lcDoubleSpinBox* AngleSpinBox = mSpinBoxes[PartIdx];

		if (AngleSpinBox)
		{
			QSignalBlocker AngleBlocker(AngleSpinBox);

			AngleSpinBox->SetValue(Template.Angles[PartIdx]);
		}
	}

	FitPreview();
}

void lcMinifigDialog::TemplateSaveButtonClicked()
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

	if (TemplateName == MinifigWizard::GetDefaultTemplateName())
	{
		QMessageBox::information(this, tr("Save Template"), tr("Cannot overwrite the default template."));
		return;
	}

	if (mMinifigWizard->GetTemplates().count(TemplateName))
	{
		QString Question = tr("Are you sure you want to overwrite the template '%1'?").arg(TemplateName);
		if (QMessageBox::question(this, tr("Overwrite Template"), Question, QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
			return;
	}

	lcMinifigTemplate Template;

	for (int ItemIndex = 0; ItemIndex < LC_MFW_NUMITEMS; ItemIndex++)
	{
		Template.Parts[ItemIndex] = mMinifigWizard->mSettings[ItemIndex][mMinifigWizard->GetSelectionIndex(ItemIndex)].PieceData->mFileName;
		Template.ColorCodes[ItemIndex] = mColorPickers[ItemIndex]->GetCurrentColorCode();
		QDoubleSpinBox* AngleSpinBox = mSpinBoxes[ItemIndex];
		Template.Angles[ItemIndex] = AngleSpinBox ? AngleSpinBox->value() : 0.0f;
	}

	mMinifigWizard->SaveTemplate(TemplateName, Template);

	mCurrentTemplateName = TemplateName;
	mCurrentTemplateModified = false;

	UpdateTemplateCombo();
}

void lcMinifigDialog::TemplateDeleteButtonClicked()
{
	if (mCurrentTemplateName.isEmpty() || mCurrentTemplateName == MinifigWizard::GetDefaultTemplateName())
		return;

	QString Template = mCurrentTemplateName;
	QString Question = tr("Are you sure you want to delete the template '%1'?").arg(Template);

	if (QMessageBox::question(this, tr("Delete Template"), Question, QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
		return;

	mMinifigWizard->DeleteTemplate(Template);

	mCurrentTemplateName.clear();
	mCurrentTemplateModified = false;

	UpdateTemplateCombo();
}

void lcMinifigDialog::TemplateImportButtonClicked()
{
	QString FileName = QFileDialog::getOpenFileName(this, tr("Import Templates"), "", tr("Minifig Template Files (*.minifig);;All Files (*.*)"));

	if (FileName.isEmpty())
		return;

	QFile File(FileName);

	if (!File.open(QIODevice::ReadOnly))
	{
		QMessageBox::warning(this, tr("Import Templates"), tr("Error reading file '%1':\n%2").arg(FileName, File.errorString()));
		return;
	}

	QByteArray FileData = File.readAll();
	mMinifigWizard->AddTemplatesJson(FileData);

	UpdateTemplateCombo();
}

void lcMinifigDialog::TemplateExportButtonClicked()
{
	QString FileName = QFileDialog::getSaveFileName(this, tr("Export Templates"), "", tr("Minifig Template Files (*.minifig);;All Files (*.*)"));

	if (FileName.isEmpty())
		return;

	QFile File(FileName);

	if (!File.open(QIODevice::WriteOnly))
	{
		QMessageBox::warning(this, tr("Export Templates"), tr("Error writing to file '%1':\n%2").arg(FileName, File.errorString()));
		return;
	}

	QByteArray Templates = mMinifigWizard->GetTemplatesJson();
	File.write(Templates);
}

void lcMinifigDialog::SetCurrentTemplateModified()
{
	if (mCurrentTemplateName == MinifigWizard::GetDefaultTemplateName())
		mCurrentTemplateName.clear();
	else
		mCurrentTemplateModified = true;
}

void lcMinifigDialog::PieceButtonClicked()
{
	QToolButton* PieceButton = qobject_cast<QToolButton*>(sender());

	if (!PieceButton)
		return;

	std::array<QToolButton*, LC_MFW_NUMITEMS>::iterator Search = std::find(mPieceButtons.begin(), mPieceButtons.end(), PieceButton);

	if (Search == mPieceButtons.end())
		return;

	int ItemIndex = static_cast<int>(std::distance(mPieceButtons.begin(), Search));
	lcPieceData* CurrentPieceData = mMinifigWizard->mMinifig.PieceData[ItemIndex].Get();

	QPoint Position = PieceButton->mapToGlobal(PieceButton->rect().bottomLeft());
	std::vector<std::pair<lcPieceData*, std::string>> Parts;

	Parts.reserve(mMinifigWizard->mSettings[ItemIndex].size());
	Parts.emplace_back(nullptr, tr("None").toStdString());

	for (const lcMinifigPieceInfo& Setting : mMinifigWizard->mSettings[ItemIndex])
		if (Setting.PieceData)
			Parts.emplace_back(Setting.PieceData, Setting.Description);

	int ColorIndex = mMinifigWizard->mMinifig.ColorIndices[ItemIndex];

	std::optional<lcPieceData*> Result = lcShowPartSelectionPopup(CurrentPieceData, Parts, ColorIndex, PieceButton, Position);

	if (!Result.has_value())
		return;

	lcPieceData* NewPiece = Result.value();

	SetCurrentTemplateModified();
	UpdateTemplateCombo();

	mView->MakeCurrent();
	mMinifigWizard->SetPieceData(ItemIndex, NewPiece);
	FitPreview();

	UpdatePieceButton(ItemIndex);
}

void lcMinifigDialog::ColorChanged(int Index)
{
	std::array<lcColorPicker*, LC_MFW_NUMITEMS>::iterator Search = std::find(mColorPickers.begin(), mColorPickers.end(), sender());

	if (Search == mColorPickers.end())
		return;

	SetCurrentTemplateModified();
	UpdateTemplateCombo();

	int ItemIndex = static_cast<int>(std::distance(mColorPickers.begin(), Search));

	mMinifigWizard->SetColorIndex(ItemIndex, Index);
	mView->Redraw();
}

void lcMinifigDialog::AngleChanged(double Value)
{
	std::array<lcDoubleSpinBox*, LC_MFW_NUMITEMS>::iterator Search = std::find(mSpinBoxes.begin(), mSpinBoxes.end(), sender());

	if (Search == mSpinBoxes.end())
		return;

	SetCurrentTemplateModified();
	UpdateTemplateCombo();

	int ItemIndex = static_cast<int>(std::distance(mSpinBoxes.begin(), Search));

	mMinifigWizard->SetAngle(ItemIndex, Value);
	FitPreview();
}
