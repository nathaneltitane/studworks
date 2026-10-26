#pragma once

namespace Ui
{
	class swModelListDialog;
}

struct swModelListDialogEntry
{
	QString Name;
	swModel* ExistingModel;
	swModel* DuplicateSource;
};

class swModelListDialog : public QDialog
{
	Q_OBJECT

public:
	swModelListDialog(QWidget* Parent, const std::vector<std::unique_ptr<swModel>>& Models);
	~swModelListDialog();

	int GetActiveModelIndex() const;
	std::vector<swModelListDialogEntry> GetResults() const;

public slots:
	void accept() override;

private slots:
	void NewModelClicked();
	void DeleteModelClicked();
	void RenameModelClicked();
	void ExportModelClicked();
	void DuplicateModelClicked();
	void ModelListItemDoubleClicked(QListWidgetItem* Item);
	void ModelListItemSelectionChanged();

private:
	QListWidgetItem* mActiveModelItem;
	void UpdateButtons();
	Ui::swModelListDialog* ui;
};

