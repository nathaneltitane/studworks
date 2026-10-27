#pragma once

class swHTMLExportOptions;

namespace Ui {
class swHTMLDialog;
}

class swHTMLDialog : public QDialog
{
	Q_OBJECT

public:
	explicit swHTMLDialog(QWidget* Parent, swHTMLExportOptions* Options);
	virtual ~swHTMLDialog();

public slots:
	void accept() override;

private slots:
	void OutputFolderBrowseClicked();

private:
	swHTMLExportOptions* mOptions;
	Ui::swHTMLDialog* ui;
};

