#include "sw_global.h"
#include "sw_window_utilities.h"
#include "sw_dialog_html.h"
#include "ui_sw_dialog_html.h"
#include "sw_project.h"

swHTMLDialog::swHTMLDialog(QWidget* Parent, swHTMLExportOptions* Options)
	: QDialog(Parent), ui(new Ui::swHTMLDialog)
{
    ui->setupUi(this);
	swSetupDialogHelp(this, ui->buttonBox);

	connect(ui->outputFolderBrowse, &QPushButton::clicked, this, &swHTMLDialog::OutputFolderBrowseClicked);

	ui->stepWidth->setValidator(new QIntValidator(1, SW_MAXIMUM_IMAGE_DIMENSION, ui->stepWidth));
	ui->stepHeight->setValidator(new QIntValidator(1, SW_MAXIMUM_IMAGE_DIMENSION, ui->stepHeight));

	mOptions = Options;

	ui->outputFolder->setText(QDir::toNativeSeparators(mOptions->PathName));

	if (mOptions->CurrentOnly)
		ui->currentModelOnly->setChecked(true);
	else if (mOptions->SubModels)
		ui->currentModelSubmodels->setChecked(true);
	else
		ui->allModels->setChecked(true);

	ui->transparentImages->setChecked(mOptions->TransparentImages);
	ui->singlePage->setChecked(mOptions->SinglePage);
	ui->oneStepPerPage->setChecked(!mOptions->SinglePage);
	ui->indexPage->setChecked(mOptions->SinglePage);
	ui->stepWidth->setText(swFormatIntegerLocalized(mOptions->StepImagesWidth, ui->stepWidth->validator()->locale()));
	ui->stepHeight->setText(swFormatIntegerLocalized(mOptions->StepImagesHeight, ui->stepHeight->validator()->locale()));
	ui->partsAfterEachStep->setChecked(mOptions->PiecesListStep);
	ui->partsAtTheEnd->setChecked(mOptions->PiecesListEnd);
}

swHTMLDialog::~swHTMLDialog()
{
    delete ui;
}

void swHTMLDialog::accept()
{
	QString pathName = ui->outputFolder->text();

	if (pathName.isEmpty())
	{
		QMessageBox::information(this, tr("Export HTML"), tr("Output folder cannot be empty."));
		return;
	}

	bool WidthOk, HeightOk;
	const int Width = ui->stepWidth->validator()->locale().toInt(ui->stepWidth->text(), &WidthOk);
	const int Height = ui->stepHeight->validator()->locale().toInt(ui->stepHeight->text(), &HeightOk);

	if (!WidthOk || !ui->stepWidth->hasAcceptableInput())
	{
		QMessageBox::information(this, windowTitle(), tr("Please enter a width between 1 and 32768 pixels."));
		ui->stepWidth->setFocus();
		return;
	}

	if (!HeightOk || !ui->stepHeight->hasAcceptableInput())
	{
		QMessageBox::information(this, windowTitle(), tr("Please enter a height between 1 and 32768 pixels."));
		ui->stepHeight->setFocus();
		return;
	}

	mOptions->PathName = pathName;
	mOptions->SubModels = ui->currentModelSubmodels->isChecked();
	mOptions->CurrentOnly = ui->currentModelOnly->isChecked();
	mOptions->TransparentImages = ui->transparentImages->isChecked();
	mOptions->SinglePage = ui->singlePage->isChecked();
	mOptions->IndexPage = ui->indexPage->isChecked();
	mOptions->StepImagesWidth = Width;
	mOptions->StepImagesHeight = Height;
	mOptions->PiecesListStep = ui->partsAfterEachStep->isChecked();
	mOptions->PiecesListEnd = ui->partsAtTheEnd->isChecked();

	QDialog::accept();
}

void swHTMLDialog::OutputFolderBrowseClicked()
{
	QString result = QFileDialog::getExistingDirectory(this, tr("Select Output Folder"), ui->outputFolder->text());

	if (!result.isEmpty())
		ui->outputFolder->setText(QDir::toNativeSeparators(result));
}
