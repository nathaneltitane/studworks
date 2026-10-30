#include "sw_global.h"
#include "sw_window_utilities.h"
#include "sw_dialog_group.h"
#include "ui_sw_dialog_group.h"
#include "sw_group.h"

swGroupDialog::swGroupDialog(QWidget* Parent, const QString& Name, const std::vector<std::unique_ptr<swGroup>>& Groups)
    : QDialog(Parent), mGroups(Groups), ui(new Ui::swGroupDialog)
{
	ui->setupUi(this);
	swSetupDialogHelp(this, ui->buttonBox);

	swSetInputPrompt(this);

	ui->name->setText(Name);
}

swGroupDialog::~swGroupDialog()
{
	delete ui;
}

void swGroupDialog::accept()
{
	QString Name = ui->name->text();

	if (Name.isEmpty())
	{
		QMessageBox::information(this, windowTitle(), tr("Name cannot be empty."));
		return;
	}

	for (const std::unique_ptr<swGroup>& Group : mGroups)
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
