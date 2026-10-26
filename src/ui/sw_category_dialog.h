#pragma once

struct swLibraryCategory;

namespace Ui {
class swCategoryDialog;
}

class swCategoryDialog : public QDialog
{
	Q_OBJECT

public:
	explicit swCategoryDialog(QWidget* Parent, swLibraryCategory* Category);
	virtual ~swCategoryDialog();

	swLibraryCategory* mOptions;

public slots:
	void accept() override;

private:
	Ui::swCategoryDialog* ui;
};

