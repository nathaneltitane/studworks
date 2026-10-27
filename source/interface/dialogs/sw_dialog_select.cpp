#include "sw_global.h"
#include "sw_window_utilities.h"
#include "sw_dialog_select.h"
#include "ui_sw_dialog_select.h"
#include "sw_model.h"
#include "sw_object_piece.h"
#include "sw_object_camera.h"
#include "sw_object_light.h"
#include "sw_group.h"

swSelectDialog::swSelectDialog(QWidget* Parent, swModel* Model)
	: QDialog(Parent), ui(new Ui::swSelectDialog)
{
	ui->setupUi(this);
	swSetupDialogHelp(this, ui->buttonBox);

	connect(ui->selectAll, &QPushButton::clicked, this, &swSelectDialog::SelectAllClicked);
	connect(ui->selectNone, &QPushButton::clicked, this, &swSelectDialog::SelectNoneClicked);
	connect(ui->selectInvert, &QPushButton::clicked, this, &swSelectDialog::SelectInvertClicked);
	connect(ui->treeWidget, &QTreeWidget::itemChanged, this, &swSelectDialog::ItemChanged);

	AddChildren(ui->treeWidget->invisibleRootItem(), nullptr, Model);
	ui->treeWidget->expandAll();
}

swSelectDialog::~swSelectDialog()
{
	delete ui;
}

void swSelectDialog::accept()
{
	mObjects.clear();

	QList<QTreeWidgetItem*> Items;
	Items.append(ui->treeWidget->invisibleRootItem());

	while (!Items.isEmpty())
	{
		QTreeWidgetItem* Item = Items[0];
		Items.removeFirst();

		if (!Item->childCount())
		{
			if (Item->checkState(0) == Qt::Checked)
			{
				swObject* Object = (swObject*)Item->data(0, IndexRole).value<uintptr_t>();
				mObjects.emplace_back(Object);
			}
		}
		else
		{
			for (int ChildIdx = 0; ChildIdx < Item->childCount(); ChildIdx++)
				Items.append(Item->child(ChildIdx));
		}
	}

	QDialog::accept();
}

void swSelectDialog::SelectAllClicked()
{
	ui->treeWidget->blockSignals(true);

	QList<QTreeWidgetItem*> Items;
	Items.append(ui->treeWidget->invisibleRootItem());

	while (!Items.isEmpty())
	{
		QTreeWidgetItem* Item = Items[0];
		Items.removeFirst();

		if (!Item->childCount())
			Item->setCheckState(0, Qt::Checked);
		else
		{
			for (int ChildIdx = 0; ChildIdx < Item->childCount(); ChildIdx++)
				Items.append(Item->child(ChildIdx));
		}
	}

	ui->treeWidget->blockSignals(false);
}

void swSelectDialog::SelectNoneClicked()
{
	ui->treeWidget->blockSignals(true);

	QList<QTreeWidgetItem*> Items;
	Items.append(ui->treeWidget->invisibleRootItem());

	while (!Items.isEmpty())
	{
		QTreeWidgetItem* Item = Items[0];
		Items.removeFirst();

		if (!Item->childCount())
			Item->setCheckState(0, Qt::Unchecked);
		else
		{
			for (int ChildIdx = 0; ChildIdx < Item->childCount(); ChildIdx++)
				Items.append(Item->child(ChildIdx));
		}
	}

	ui->treeWidget->blockSignals(false);
}

void swSelectDialog::SelectInvertClicked()
{
	ui->treeWidget->blockSignals(true);

	QList<QTreeWidgetItem*> Items;
	Items.append(ui->treeWidget->invisibleRootItem());

	while (!Items.isEmpty())
	{
		QTreeWidgetItem* Item = Items[0];
		Items.removeFirst();

		if (!Item->childCount())
			Item->setCheckState(0, Item->checkState(0) == Qt::Checked ? Qt::Unchecked : Qt::Checked);
		else
		{
			for (int ChildIdx = 0; ChildIdx < Item->childCount(); ChildIdx++)
				Items.append(Item->child(ChildIdx));
		}
	}

	ui->treeWidget->blockSignals(false);
}

void swSelectDialog::ItemChanged(QTreeWidgetItem *item, int column)
{
	Q_UNUSED(column);

	QTreeWidgetItem* ParentItem = item->parent();

	if (!ParentItem)
		return;

	Qt::CheckState State = item->checkState(0);

	for (;;)
	{
		QTreeWidgetItem* ParentParentItem = ParentItem->parent();

		if (ParentParentItem)
			ParentItem = ParentParentItem;
		else
			break;
	}

	ui->treeWidget->blockSignals(true);

	QList<QTreeWidgetItem*> Items;
	Items.append(ParentItem);

	while (!Items.isEmpty())
	{
		QTreeWidgetItem* Item = Items[0];
		Items.removeFirst();

		if (!Item->childCount())
			Item->setCheckState(0, State);
		else
		{
			for (int ChildIdx = 0; ChildIdx < Item->childCount(); ChildIdx++)
				Items.append(Item->child(ChildIdx));
		}
	}

	ui->treeWidget->blockSignals(false);
}

void swSelectDialog::AddChildren(QTreeWidgetItem* ParentItem, swGroup* ParentGroup, swModel* Model)
{
	const std::vector<std::unique_ptr<swGroup>>& Groups = Model->GetGroups();

	for (const std::unique_ptr<swGroup>& Group : Groups)
	{
		if (Group->mGroup != ParentGroup)
			continue;

		QTreeWidgetItem* GroupItem = new QTreeWidgetItem(ParentItem, QStringList(Group->mName));

		AddChildren(GroupItem, Group.get(), Model);
	}

	const std::vector<std::unique_ptr<swPiece>>& Pieces = Model->GetPieces();
	swStep currentStep = Model->GetCurrentStep();

	for (const std::unique_ptr<swPiece>& Piece : Pieces)
	{
		if (Piece->GetGroup() != ParentGroup || !Piece->IsVisible(currentStep))
			continue;

		QTreeWidgetItem* PieceItem = new QTreeWidgetItem(ParentItem, QStringList(Piece->GetName()));
		PieceItem->setData(0, IndexRole, QVariant::fromValue<uintptr_t>((uintptr_t)Piece.get()));
		PieceItem->setCheckState(0, Piece->IsSelected() ? Qt::Checked : Qt::Unchecked);
	}

	if (!ParentGroup)
	{
		const std::vector<std::unique_ptr<swCamera>>& Cameras = Model->GetCameras();

		for (const std::unique_ptr<swCamera>& Camera : Cameras)
		{
			if (!Camera->IsVisible())
				continue;

			QTreeWidgetItem *cameraItem = new QTreeWidgetItem(ParentItem, QStringList(Camera->GetName()));
			cameraItem->setData(0, IndexRole, QVariant::fromValue<uintptr_t>((uintptr_t)Camera.get()));
			cameraItem->setCheckState(0, Camera->IsSelected() ? Qt::Checked : Qt::Unchecked);
		}

		const std::vector<std::unique_ptr<swLight>>& Lights = Model->GetLights();

		for (const std::unique_ptr<swLight>& Light : Lights)
		{
			if (!Light->IsVisible())
				continue;

			QTreeWidgetItem *lightItem = new QTreeWidgetItem(ParentItem, QStringList(Light->GetName()));
			lightItem->setData(0, IndexRole, QVariant::fromValue<uintptr_t>((uintptr_t)Light.get()));
			lightItem->setCheckState(0, Light->IsSelected() ? Qt::Checked : Qt::Unchecked);
		}
	}
}
