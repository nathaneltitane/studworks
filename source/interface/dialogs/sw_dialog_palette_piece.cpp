#include "sw_global.h"
#include "sw_window_utilities.h"
#include "sw_dialog_palette_piece.h"
#include "ui_sw_dialog_palette_piece.h"
#include "sw_widget_selection_piece.h"
#include "sw_dialog_database_sets.h"
#include "sw_project.h"

swPiecePaletteDialog::swPiecePaletteDialog(QWidget* Parent, std::vector<swPiecePalette>& PiecePalettes)
	: QDialog(Parent), ui(new Ui::swPiecePaletteDialog), mPiecePalettes(PiecePalettes)
{
    ui->setupUi(this);
	swSetupDialogHelp(this, ui->buttonBox);

	connect(ui->NewButton, &QPushButton::clicked, this, &swPiecePaletteDialog::NewButtonClicked);
	connect(ui->DeleteButton, &QPushButton::clicked, this, &swPiecePaletteDialog::DeleteButtonClicked);
	connect(ui->RenameButton, &QPushButton::clicked, this, &swPiecePaletteDialog::RenameButtonClicked);
	connect(ui->ImportButton, &QPushButton::clicked, this, &swPiecePaletteDialog::ImportButtonClicked);
	connect(ui->MoveUpButton, &QPushButton::clicked, this, &swPiecePaletteDialog::MoveUpButtonClicked);
	connect(ui->MoveDownButton, &QPushButton::clicked, this, &swPiecePaletteDialog::MoveDownButtonClicked);
	connect(ui->PaletteList, QOverload<int>::of(&QListWidget::currentRowChanged), this, &swPiecePaletteDialog::PaletteListCurrentRowChanged);

	for (const swPiecePalette& Palette : PiecePalettes)
	{
		QListWidgetItem* Item = new QListWidgetItem(Palette.Name);
		Item->setData(Qt::UserRole, QVariant::fromValue<uintptr_t>(reinterpret_cast<uintptr_t>(&Palette)));
		ui->PaletteList->addItem(Item);
	}

	ui->PaletteList->setCurrentRow(0);
	UpdateButtons();
}

swPiecePaletteDialog::~swPiecePaletteDialog()
{
	for (swPiecePalette* Palette : mImportedPalettes)
		delete Palette;

	delete ui;
}

void swPiecePaletteDialog::UpdateButtons()
{
	int CurrentPalette = ui->PaletteList->currentRow();
	int PaletteCount = ui->PaletteList->count();

	ui->DeleteButton->setEnabled(PaletteCount > 1);
	ui->MoveUpButton->setEnabled(PaletteCount > 1 && CurrentPalette > 0);
	ui->MoveDownButton->setEnabled(PaletteCount > 1 && CurrentPalette < PaletteCount - 1);
}

void swPiecePaletteDialog::accept()
{
	std::vector<swPiecePalette> PiecePalettes;
	PiecePalettes.reserve(ui->PaletteList->count());

	for (int ItemIdx = 0; ItemIdx < ui->PaletteList->count(); ItemIdx++)
	{
		QListWidgetItem* Item = ui->PaletteList->item(ItemIdx);
		swPiecePalette* OldPalette = reinterpret_cast<swPiecePalette*>(Item->data(Qt::UserRole).value<uintptr_t>());

		swPiecePalette Palette;
		Palette.Name = Item->text();
		if (OldPalette)
			Palette.Pieces = std::move(OldPalette->Pieces);

		PiecePalettes.emplace_back(std::move(Palette));
	}

	mPiecePalettes = std::move(PiecePalettes);

	QDialog::accept();
}

void swPiecePaletteDialog::NewButtonClicked()
{
	bool Ok = false;

	QString Name = QInputDialog::getText(this, tr("New Piece Palette"), tr("Palette Name:"), QLineEdit::Normal, QString(), &Ok);

	if (!Ok || Name.isEmpty())
		return;

	QListWidgetItem* Item = new QListWidgetItem(Name);
	Item->setData(Qt::UserRole, QVariant::fromValue<uintptr_t>(0));
	ui->PaletteList->addItem(Item);
	ui->PaletteList->setCurrentRow(ui->PaletteList->count() - 1);
	UpdateButtons();
}

void swPiecePaletteDialog::DeleteButtonClicked()
{
	QList<QListWidgetItem*>	SelectedItems = ui->PaletteList->selectedItems();

	if (SelectedItems.isEmpty())
		return;

	QString Prompt = tr("Are you sure you want to delete the palette '%1'?").arg(SelectedItems[0]->text());
	if (QMessageBox::question(this, tr("Delete Piece Palette"), Prompt, QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
		return;

	QListWidgetItem* SelectedItem = SelectedItems.first();

	delete SelectedItem;
	UpdateButtons();
}

void swPiecePaletteDialog::RenameButtonClicked()
{
	QList<QListWidgetItem*>	SelectedItems = ui->PaletteList->selectedItems();

	if (SelectedItems.isEmpty())
		return;

	bool Ok = false;

	QString Name = SelectedItems[0]->text();
	Name = QInputDialog::getText(this, tr("Rename Piece Palette"), tr("Palette Name:"), QLineEdit::Normal, Name, &Ok);

	if (!Ok || Name.isEmpty())
		return;

	if (!Name.isEmpty())
		SelectedItems[0]->setText(Name);
}

void swPiecePaletteDialog::ImportButtonClicked()
{
	swSetsDatabaseDialog Dialog(this);

	if (Dialog.exec() != QDialog::Accepted)
		return;

	swPiecePalette* Palette = new swPiecePalette;
	Palette->Name = Dialog.GetSetDescription();
	mImportedPalettes.push_back(Palette);

	std::vector<swSetInventoryItem> Inventory = Dialog.GetSetInventory();

	for (const swSetInventoryItem& Item : Inventory)
	{
		Palette->Pieces.push_back(Item.PieceID.toUpper().toStdString() + ".DAT");
	}

	QListWidgetItem* Item = new QListWidgetItem(Palette->Name);
	Item->setData(Qt::UserRole, QVariant::fromValue<uintptr_t>(reinterpret_cast<uintptr_t>(Palette)));
	ui->PaletteList->addItem(Item);
	ui->PaletteList->setCurrentRow(ui->PaletteList->count() - 1);
	UpdateButtons();
}

void swPiecePaletteDialog::MoveUpButtonClicked()
{
	QList<QListWidgetItem*>	SelectedItems = ui->PaletteList->selectedItems();

	if (SelectedItems.isEmpty())
		return;

	QListWidgetItem* Item = SelectedItems[0];
	int Row = ui->PaletteList->row(Item);

	if (Row == 0)
		return;

	ui->PaletteList->takeItem(Row);
	ui->PaletteList->insertItem(Row - 1, Item);
	ui->PaletteList->setCurrentItem(Item);
	UpdateButtons();
}

void swPiecePaletteDialog::MoveDownButtonClicked()
{
	QList<QListWidgetItem*>	SelectedItems = ui->PaletteList->selectedItems();

	if (SelectedItems.isEmpty())
		return;

	QListWidgetItem* Item = SelectedItems[0];
	int Row = ui->PaletteList->row(Item);

	ui->PaletteList->takeItem(Row);
	ui->PaletteList->insertItem(Row + 1, Item);
	ui->PaletteList->setCurrentItem(Item);
	UpdateButtons();
}

void swPiecePaletteDialog::PaletteListCurrentRowChanged(int CurrentRow)
{
	Q_UNUSED(CurrentRow);

	UpdateButtons();
}
