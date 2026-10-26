#pragma once

namespace Ui {
class swAboutDialog;
}

class swAboutDialog : public QDialog
{
	Q_OBJECT

public:
	explicit swAboutDialog(QWidget* Parent);
	virtual ~swAboutDialog();

private:
	Ui::swAboutDialog* ui;
};
