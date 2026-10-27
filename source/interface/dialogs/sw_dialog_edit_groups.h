#pragma once

namespace Ui {
class swEditGroupsDialog;
}

class swEditGroupsDialog : public QDialog
{
	Q_OBJECT

public:
	swEditGroupsDialog(QWidget* Parent, const swModel* Model);
	virtual ~swEditGroupsDialog();

	struct GroupInfo
	{
		QString Name;
		swGroup* Group;
		std::vector<GroupInfo> ChildGroups;
		std::vector<swPiece*> ChildPieces;
	};

	GroupInfo GetGroups() const;
	bool CanRenameGroup(const QString& Text) const;

protected slots:
	void NewGroupClicked();

protected:
	enum
	{
		PieceRole = Qt::UserRole,
		GroupRole
	};

	GroupInfo GetGroupInfo(QTreeWidgetItem* ParentItem) const;
	void PopulateTree();

	Ui::swEditGroupsDialog* ui = nullptr;
	const swModel* mModel = nullptr;
};
