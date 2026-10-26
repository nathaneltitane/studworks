#pragma once

namespace Ui {
class swSelectDialog;
}

class swSelectDialog : public QDialog
{
	Q_OBJECT

public:
	swSelectDialog(QWidget* Parent, swModel* Model);
	~swSelectDialog();

	std::vector<swObject*> mObjects;

	enum
	{
		IndexRole = Qt::UserRole
	};

public slots:
	void accept() override;

private slots:
	void SelectAllClicked();
	void SelectNoneClicked();
	void SelectInvertClicked();
	void ItemChanged(QTreeWidgetItem *item, int column);

private:
	Ui::swSelectDialog* ui;

	void AddChildren(QTreeWidgetItem* ParentItem, swGroup* ParentGroup, swModel* Model);
};

