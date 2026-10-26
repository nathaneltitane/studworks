#include "sw_global.h"
#include "sw_utils.h"
#include "sw_string.h"
#include "sw_editgroupsdialog.h"
#include "ui_sw_editgroupsdialog.h"
#include "sw_model.h"
#include "piece.h"
#include "group.h"

constexpr uintptr_t SW_GROUPDIALOG_NEW_GROUP = ~0U;

class swEditGroupsDialogDelegate : public QItemDelegate
{
public:
	swEditGroupsDialogDelegate(QObject* Parent)
	    : QItemDelegate(Parent)
	{
	}

	void setModelData(QWidget* Editor, QAbstractItemModel* Model, const QModelIndex& Index) const
	{
		QLineEdit* LineEdit = qobject_cast<QLineEdit *>(Editor);

		if (!LineEdit->isModified())
			return;

		QString Text = LineEdit->text().trimmed();

		swEditGroupsDialog* Dialog = qobject_cast<swEditGroupsDialog*>(parent());

		if (Dialog && !Dialog->CanRenameGroup(Text))
			return;

		QItemDelegate::setModelData(Editor, Model, Index);
	}
};

swEditGroupsDialog::swEditGroupsDialog(QWidget* Parent, const swModel* Model)
    : QDialog(Parent), mModel(Model)
{
	ui = new Ui::swEditGroupsDialog;

	ui->setupUi(this);
	swSetupDialogHelp(this, ui->buttonBox);

	QPushButton* NewGroup = ui->buttonBox->addButton(tr("&New Group"), QDialogButtonBox::ActionRole);
	NewGroup->setToolTip(tr("Create a group under the selected group or at the top level."));
	NewGroup->setWhatsThis(tr("<p><b>New Group</b></p><p>Select a group to create a subgroup inside it. If a piece is selected, the new group is created beside that piece. With no selection, the new group is created at the top level. Rename the new group by editing its name, then drag pieces into it.</p>"));

	connect(NewGroup, &QPushButton::clicked, this, &swEditGroupsDialog::NewGroupClicked);

	PopulateTree();

	ui->treeWidget->setItemDelegate(new swEditGroupsDialogDelegate(this));
	ui->treeWidget->expandAll();
}

swEditGroupsDialog::~swEditGroupsDialog()
{
	delete ui;
}

bool swEditGroupsDialog::CanRenameGroup(const QString& Text) const
{
	if (Text.isEmpty())
		return false;

	std::function<bool(QTreeWidgetItem*)> ScanGroups = [&ScanGroups, &Text](QTreeWidgetItem* ParentItem)
	{
		for (int ChildIndex = 0; ChildIndex < ParentItem->childCount(); ChildIndex++)
		{
			QTreeWidgetItem* ChildItem = ParentItem->child(ChildIndex);

			if (ChildItem->data(0, GroupRole).value<uintptr_t>())
			{
				if (ChildItem->text(0) == Text || !ScanGroups(ChildItem))
					return false;
			}
		}

		return true;
	};

	return ScanGroups(ui->treeWidget->invisibleRootItem());
}

void swEditGroupsDialog::NewGroupClicked()
{
	QTreeWidgetItem* CurrentItem = ui->treeWidget->currentItem();

	if (CurrentItem && CurrentItem->data(0, PieceRole).value<uintptr_t>())
		CurrentItem = CurrentItem->parent();

	if (!CurrentItem)
		CurrentItem = ui->treeWidget->invisibleRootItem();

	const QString Prefix = tr("Group ");
	int MaxIndex = 0;

	std::function<void(QTreeWidgetItem*)> ScanGroups = [&ScanGroups, &Prefix, &MaxIndex](QTreeWidgetItem* ParentItem)
	{
		for (int ChildIndex = 0; ChildIndex < ParentItem->childCount(); ChildIndex++)
		{
			QTreeWidgetItem* ChildItem = ParentItem->child(ChildIndex);

			if (ChildItem->data(0, GroupRole).value<uintptr_t>())
			{
				QString Name = ChildItem->text(0);

				MaxIndex = qMax(MaxIndex, swGetDefaultNameNumber(Name, Prefix));

				ScanGroups(ChildItem);
			}
		}
	};

	ScanGroups(ui->treeWidget->invisibleRootItem());

	QString Name = swFormatDefaultName(Prefix, MaxIndex + 1);

	QTreeWidgetItem* GroupItem = new QTreeWidgetItem(CurrentItem, QStringList(Name));
	GroupItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled | Qt::ItemIsEditable);
	GroupItem->setData(0, GroupRole, QVariant::fromValue<uintptr_t>(SW_GROUPDIALOG_NEW_GROUP));
	GroupItem->setExpanded(true);
}

swEditGroupsDialog::GroupInfo swEditGroupsDialog::GetGroupInfo(QTreeWidgetItem* ParentItem) const
{
	swEditGroupsDialog::GroupInfo GroupInfo;

	GroupInfo.Name = ParentItem->text(0);

	uintptr_t GroupPointer = ParentItem->data(0, GroupRole).value<uintptr_t>();
	GroupInfo.Group = (GroupPointer == SW_GROUPDIALOG_NEW_GROUP) ? nullptr : reinterpret_cast<swGroup*>(GroupPointer);

	for (int ChildIndex = 0; ChildIndex < ParentItem->childCount(); ChildIndex++)
	{
		QTreeWidgetItem* ChildItem = ParentItem->child(ChildIndex);

		if (ChildItem->data(0, GroupRole).value<uintptr_t>())
			GroupInfo.ChildGroups.emplace_back(GetGroupInfo(ChildItem));
		else
		{
			swPiece* Piece = reinterpret_cast<swPiece*>(ChildItem->data(0, PieceRole).value<uintptr_t>());

			if (Piece)
				GroupInfo.ChildPieces.push_back(Piece);
		}
	}

	return GroupInfo;
}

swEditGroupsDialog::GroupInfo swEditGroupsDialog::GetGroups() const
{
	return GetGroupInfo(ui->treeWidget->invisibleRootItem());
}

void swEditGroupsDialog::PopulateTree()
{
	const std::vector<std::unique_ptr<swGroup>>& Groups = mModel->GetGroups();
	std::unordered_map<swGroup*, QTreeWidgetItem*> AddedGroups;
	std::vector<swGroup*> GroupsToAdd;

	AddedGroups[nullptr] = ui->treeWidget->invisibleRootItem();

	auto CreateGroupItem = [&AddedGroups](swGroup* Group)
	{
		QTreeWidgetItem* ParentItem = AddedGroups.find(Group->mGroup)->second;

		if (!ParentItem)
			return false;

		QTreeWidgetItem* GroupItem = new QTreeWidgetItem(ParentItem, QStringList(Group->mName));
		GroupItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled | Qt::ItemIsEditable);
		GroupItem->setData(0, GroupRole, QVariant::fromValue<uintptr_t>((uintptr_t)Group));

		AddedGroups[Group] = GroupItem;

		return true;
	};

	for (const std::unique_ptr<swGroup>& Group : Groups)
		if (!CreateGroupItem(Group.get()))
			GroupsToAdd.push_back(Group.get());

	while (!GroupsToAdd.empty())
	{
		size_t GroupCount = GroupsToAdd.size();

		for (auto GroupIt = GroupsToAdd.begin(); GroupIt != GroupsToAdd.end();)
		{
			if (CreateGroupItem(*GroupIt))
				GroupIt = GroupsToAdd.erase(GroupIt);
			else
				++GroupIt;
		}

		if (GroupCount == GroupsToAdd.size())
			break;
	}

	const std::vector<std::unique_ptr<swPiece>>& Pieces = mModel->GetPieces();

	for (const std::unique_ptr<swPiece>& Piece : Pieces)
	{
		QTreeWidgetItem* ParentItem = AddedGroups.find(Piece->GetGroup())->second;

		if (ParentItem)
		{
			QTreeWidgetItem* PieceItem = new QTreeWidgetItem(ParentItem, QStringList(Piece->GetName()));
			PieceItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled);
			PieceItem->setData(0, PieceRole, QVariant::fromValue<uintptr_t>((uintptr_t)Piece.get()));
		}
	}
}
