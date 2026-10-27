#pragma once

namespace Ui {
class swPiecePaletteDialog;
}

struct swPiecePalette;

class swPiecePaletteDialog : public QDialog
{
    Q_OBJECT

public:
    swPiecePaletteDialog(QWidget* Parent, std::vector<swPiecePalette>& PartPalettes);
    ~swPiecePaletteDialog();

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

	Ui::swPiecePaletteDialog* ui;
	std::vector<swPiecePalette>& mPartPalettes;
	std::vector<swPiecePalette*> mImportedPalettes;
};
