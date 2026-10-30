#include "sw_global.h"
#include "sw_window_utilities.h"
#include "sw_dialog_category.h"
#include "ui_sw_dialog_category.h"
#include "sw_category.h"

swCategoryDialog::swCategoryDialog(QWidget* Parent, swLibraryCategory* Options)
	: QDialog(Parent), ui(new Ui::swCategoryDialog)
{
	ui->setupUi(this);
	swSetupDialogHelp(this, ui->buttonBox);
	swSetInputPrompt(this);

	mOptions = Options;

	if (!mOptions->Name.isEmpty())
		setWindowTitle(tr("Edit Category"));
	else
		setWindowTitle(tr("New Category"));

	ui->name->setText(mOptions->Name);
	ui->keywords->setText(mOptions->Keywords);
}

swCategoryDialog::~swCategoryDialog()
{
	delete ui;
}

void swCategoryDialog::accept()
{
	QString Name = ui->name->text();

	if (Name.isEmpty())
	{
		QMessageBox::information(this, windowTitle(), tr("Name cannot be empty."));
		return;
	}

	QString Keywords = ui->keywords->text();

	if (Keywords.isEmpty())
	{
		QMessageBox::information(this, windowTitle(), tr("Keywords cannot be empty."));
		return;
	}

	mOptions->Name = Name;
	mOptions->Keywords = Keywords.toLatin1();

	QDialog::accept();
}
