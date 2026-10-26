#pragma once

namespace Ui {
class swPartPaletteDialog;
}

struct swPartPalette;

class swPartPaletteDialog : public QDialog
{
    Q_OBJECT

public:
    swPartPaletteDialog(QWidget* Parent, std::vector<swPartPalette>& PartPalettes);
    ~swPartPaletteDialog();

protected slots:
	void accept() override;

private slots:
	void NewButtonClicked();
	void DeleteButtonClicked();
	void RenameButtonClicked();
	void ImportButtonClicked();
	void MoveUpButtonClicked();
	void MoveDownButtonClicked();
	void PaletteListCurrentRowChanged(int CurrentRow);

private:
	void UpdateButtons();

	Ui::swPartPaletteDialog* ui;
	std::vector<swPartPalette>& mPartPalettes;
	std::vector<swPartPalette*> mImportedPalettes;
};
