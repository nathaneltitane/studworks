#pragma once

namespace Ui {
class swImageDialog;
}

struct swImageDialogOptions;

class swImageDialog : public QDialog
{
	Q_OBJECT

public:
	swImageDialog(QWidget* Parent, swImageDialogOptions* Options);
	virtual ~swImageDialog();

public slots:
	void accept() override;

private slots:
	void FileNameBrowseClicked();

private:
	swImageDialogOptions* mOptions;
	Ui::swImageDialog* ui;
};

