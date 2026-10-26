#include "sw_global.h"
#include "sw_utils.h"
#include "sw_groupdialog.h"
#include "ui_sw_groupdialog.h"
#include "group.h"

lcGroupDialog::lcGroupDialog(QWidget* Parent, const QString& Name, const std::vector<std::unique_ptr<lcGroup>>& Groups)
    : QDialog(Parent), mGroups(Groups), ui(new Ui::lcGroupDialog)
{
	ui->setupUi(this);
	lcSetupDialogHelp(this, ui->buttonBox);

	ui->name->setText(Name);
}

lcGroupDialog::~lcGroupDialog()
{
	delete ui;
}

void lcGroupDialog::accept()
{
	QString Name = ui->name->text();

	if (Name.isEmpty())
	{
		QMessageBox::information(this, windowTitle(), tr("Name cannot be empty."));
		return;
	}

	for (const std::unique_ptr<lcGroup>& Group : mGroups)
	{
		if (Group->mName == Name)
		{
			QMessageBox::information(this, windowTitle(), tr("A group with this name already exists."));
			return;
		}
	}

	mName = Name;

	QDialog::accept();
}
