#include "sw_global.h"
#include "sw_dialog_preferences.h"
#include "ui_sw_dialog_preferences.h"
#include "sw_dialog_category.h"
#include "sw_library.h"
#include "sw_data_part.h"

void swPreferencesDialog::UpdateCategories()
{
	QTreeWidgetItem* CategoryItem;
	QTreeWidget* CategoriesTree = ui->categoriesTree;

	disconnect(ui->categoriesTree->model(), &QAbstractItemModel::rowsInserted, this, &swPreferencesDialog::CategoriesDropped);

	CategoriesTree->clear();

	for (int CategoryIndex = 0; CategoryIndex < static_cast<int>(mOptions->Categories.size()); CategoryIndex++)
	{
		CategoryItem = new QTreeWidgetItem(CategoriesTree, QStringList(mOptions->Categories[CategoryIndex].Name));
		CategoryItem->setData(0, CategoryRole, QVariant(CategoryIndex));
		CategoryItem->setFlags((CategoryItem->flags() | Qt::ItemFlag::ItemIsUserCheckable) & ~Qt::ItemFlag::ItemIsDropEnabled);
		CategoryItem->setCheckState(0, mOptions->Categories[CategoryIndex].Enabled ? Qt::Checked : Qt::Unchecked);
	}

	CategoryItem = new QTreeWidgetItem(CategoriesTree, QStringList(tr("Unassigned")));
	CategoryItem->setData(0, CategoryRole, QVariant(-1));
	CategoryItem->setFlags(CategoryItem->flags() & ~Qt::ItemFlag::ItemIsDropEnabled);

	connect(ui->categoriesTree->model(), &QAbstractItemModel::rowsInserted, this, &swPreferencesDialog::CategoriesDropped);

	QTimer::singleShot(0, this, &swPreferencesDialog::SnapCategoriesTree);
}

// use to fit the categories list to its longest name beside the scroll bar
void swPreferencesDialog::SnapCategoriesTree()
{
	QTreeWidget* CategoriesTree = ui->categoriesTree;

	if (!CategoriesTree->isVisible())
		return;

	int TextWidth = CategoriesTree->header()->sectionSizeHint(0);

	for (int Row = 0; Row < CategoriesTree->topLevelItemCount(); Row++)
		TextWidth = qMax(TextWidth, CategoriesTree->sizeHintForIndex(CategoriesTree->model()->index(Row, 0)).width());

	const int ScrollBarWidth = CategoriesTree->verticalScrollBar()->isVisible() ? CategoriesTree->verticalScrollBar()->width() : 0;
	const int Width = TextWidth + ScrollBarWidth + CategoriesTree->frameWidth() * 2;
	const QList<int> Sizes = ui->splitter->sizes();

	if (Sizes.size() != 2 || Sizes[0] == Width)
		return;

	ui->splitter->setSizes({ Width, Sizes[0] + Sizes[1] - Width });
}

void swPreferencesDialog::UpdateParts()
{
	swPartsLibrary* Library = swGetPartsLibrary();
	QTreeWidget* Tree = ui->partsTree;

	Tree->clear();

	QList<QTreeWidgetItem*> SelectedItems = ui->categoriesTree->selectedItems();

	if (SelectedItems.empty())
		return;

	QTreeWidgetItem* CategoryItem = SelectedItems.first();
	int CategoryIndex = CategoryItem->data(0, CategoryRole).toInt();

	if (CategoryIndex != -1)
	{
		std::vector<swPartData*> SingleParts, GroupedParts;

		Library->GetCategoryEntries(mOptions->Categories[CategoryIndex].Keywords.constData(), false, SingleParts, GroupedParts);

		for (swPartData* PartData : SingleParts)
		{
			QStringList RowList(PartData->m_strDescription);
			RowList.append(PartData->mFileName);

			new QTreeWidgetItem(Tree, RowList);
		}
	}
	else
	{
		for (swPartData* PartData : Library->GetParts())
		{
			// sub parts and aliases belong to no category
			if (PartData->IsSubPart() || PartData->IsAlias())
				continue;

			for (CategoryIndex = 0; CategoryIndex < static_cast<int>(mOptions->Categories.size()); CategoryIndex++)
			{
				if (Library->PartDataInCategory(PartData, mOptions->Categories[CategoryIndex].Keywords.constData()))
					break;
			}

			if (CategoryIndex == static_cast<int>(mOptions->Categories.size()))
			{
				QStringList RowList(PartData->m_strDescription);
				RowList.append(PartData->mFileName);

				new QTreeWidgetItem(Tree, RowList);
			}
		}
	}

	Tree->resizeColumnToContents(0);
	Tree->resizeColumnToContents(1);
}

void swPreferencesDialog::CategoriesDropped(const QModelIndex& Parent, int First, int Last)
{
	Q_UNUSED(Parent);
	Q_UNUSED(First);
	Q_UNUSED(Last);

	std::vector<swLibraryCategory> Categories;
	// renumbering the rows must not read as checkbox changes
	QSignalBlocker Blocker(ui->categoriesTree);

	for (int Row = 0; Row < ui->categoriesTree->topLevelItemCount(); Row++)
	{
		QTreeWidgetItem* CategoryItem = ui->categoriesTree->topLevelItem(Row);
		int CategoryIndex = CategoryItem->data(0, CategoryRole).toInt();

		if (CategoryIndex >= 0 && CategoryIndex < static_cast<int>(mOptions->Categories.size()))
		{
			Categories.emplace_back(mOptions->Categories[CategoryIndex]);
			CategoryItem->setData(0, CategoryRole, Row);
		}
	}

	mOptions->CategoriesModified = true;
	mOptions->CategoriesDefault = false;
	mOptions->Categories = std::move(Categories);
}

// use to enable or disable a category from its checkbox
void swPreferencesDialog::CategoryItemChanged(QTreeWidgetItem* CategoryItem, int Column)
{
	const int CategoryIndex = CategoryItem->data(0, CategoryRole).toInt();

	// items report changes while they are built, before they have a checkbox
	if (Column != 0 || !CategoryItem->data(0, Qt::CheckStateRole).isValid() || CategoryIndex < 0 || CategoryIndex >= static_cast<int>(mOptions->Categories.size()))
		return;

	const bool Enabled = CategoryItem->checkState(0) == Qt::Checked;

	if (mOptions->Categories[CategoryIndex].Enabled == Enabled)
		return;

	mOptions->CategoriesModified = true;
	mOptions->CategoriesDefault = false;
	mOptions->Categories[CategoryIndex].Enabled = Enabled;
}

void swPreferencesDialog::NewCategoryClicked()
{
	swLibraryCategory Category;

	swCategoryDialog Dialog(this, &Category);
	if (Dialog.exec() != QDialog::Accepted)
		return;

	mOptions->CategoriesModified = true;
	mOptions->CategoriesDefault = false;
	mOptions->Categories.emplace_back(std::move(Category));

	UpdateCategories();
	ui->categoriesTree->setCurrentItem(ui->categoriesTree->topLevelItem(static_cast<int>(mOptions->Categories.size()) - 1));
}

void swPreferencesDialog::EditCategoryClicked()
{
	QList<QTreeWidgetItem*> SelectedItems = ui->categoriesTree->selectedItems();

	if (SelectedItems.empty())
		return;

	QTreeWidgetItem* CategoryItem = SelectedItems.first();
	int CategoryIndex = CategoryItem->data(0, CategoryRole).toInt();

	if (CategoryIndex == -1)
		return;

	swCategoryDialog Dialog(this, &mOptions->Categories[CategoryIndex]);

	if (Dialog.exec() != QDialog::Accepted)
		return;

	mOptions->CategoriesModified = true;
	mOptions->CategoriesDefault = false;

	UpdateCategories();
	ui->categoriesTree->setCurrentItem(ui->categoriesTree->topLevelItem(CategoryIndex));
}

void swPreferencesDialog::DeleteCategoryClicked()
{
	QList<QTreeWidgetItem*> selectedItems = ui->categoriesTree->selectedItems();

	if (selectedItems.empty())
		return;

	QTreeWidgetItem* CategoryItem = selectedItems.first();
	int CategoryIndex = CategoryItem->data(0, CategoryRole).toInt();

	if (CategoryIndex == -1)
		return;

	QString Question = tr("Are you sure you want to delete the category '%1'?").arg(mOptions->Categories[CategoryIndex].Name);
	if (QMessageBox::question(this, tr("Delete Category"), Question, QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
		return;

	mOptions->CategoriesModified = true;
	mOptions->CategoriesDefault = false;
	mOptions->Categories.erase(mOptions->Categories.begin() + CategoryIndex);

	UpdateCategories();
}

void swPreferencesDialog::ImportCategoriesClicked()
{
	QString FileName = QFileDialog::getOpenFileName(this, tr("Import Categories"), "", tr("Text Files (*.txt);;All Files (*.*)"));

	if (FileName.isEmpty())
		return;

	std::vector<swLibraryCategory> Categories;
	if (!swLoadCategories(FileName, Categories))
	{
		QMessageBox::warning(this, tr("Import Categories"), tr("Error loading categories file."));
		return;
	}

	mOptions->Categories = Categories;
	mOptions->CategoriesModified = true;
	mOptions->CategoriesDefault = false;
}

void swPreferencesDialog::ExportCategoriesClicked()
{
	QString FileName = QFileDialog::getSaveFileName(this, tr("Export Categories"), "", tr("Text Files (*.txt);;All Files (*.*)"));

	if (FileName.isEmpty())
		return;

	if (!swSaveCategories(FileName, mOptions->Categories))
	{
		QMessageBox::warning(this, tr("Export Categories"), tr("Error saving categories file."));
		return;
	}
}

void swPreferencesDialog::ResetCategoriesClicked()
{
	if (QMessageBox::question(this, tr("Reset Categories"), tr("Are you sure you want to load the default categories?"), QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
		return;

	swResetCategories(mOptions->Categories);

	mOptions->CategoriesModified = true;
	mOptions->CategoriesDefault = true;

	UpdateCategories();
}
