#include "sw_global.h"
#include "sw_model.h"
#include "sw_model_history.h"
#include "sw_object_piece.h"
#include "sw_object_camera.h"
#include "sw_object_light.h"
#include "sw_group.h"
#include "sw_project.h"
#include "sw_window_main.h"
#include "sw_profile.h"
#include "sw_library.h"
#include "sw_scene.h"
#include "sw_track_train.h"
#include "sw_file.h"
#include "sw_data_piece.h"
#include "sw_view.h"
#include "sw_builder_minifigure.h"
#include "sw_dialog_array.h"
#include "sw_dialog_group.h"
#include "sw_dialog_groups_edit.h"
#include "sw_dialog_properties.h"
#include "sw_window_utilities.h"
#include "sw_lxf.h"
#include "sw_widget_preview.h"
#include "sw_string.h"
#include <locale.h>

swGroup* swModel::AddGroup(const QString& Prefix, swGroup* Parent)
{
	swGroup* Group = new swGroup();
	mGroups.emplace_back(Group);

	Group->mName = GetGroupName(Prefix);
	Group->mGroup = Parent;

	return Group;
}

swGroup* swModel::GetGroup(const QString& Name, bool CreateIfMissing)
{
	for (const std::unique_ptr<swGroup>& Group : mGroups)
		if (Group->mName == Name)
			return Group.get();

	if (CreateIfMissing)
	{
		swGroup* Group = new swGroup();
		Group->mName = Name;
		mGroups.emplace_back(Group);

		return Group;
	}

	return nullptr;
}

void swModel::RemoveGroup(swGroup* Group)
{
	for (std::vector<std::unique_ptr<swGroup>>::iterator GroupIt = mGroups.begin(); GroupIt != mGroups.end(); GroupIt++)
	{
		if (GroupIt->get() == Group)
		{
			mGroups.erase(GroupIt);
			break;
		}
	}
}

void swModel::GroupSelection()
{
	if (!AnyPiecesSelected())
	{
		QMessageBox::information(gMainWindow, tr("Group Selection"), tr("No pieces selected."));
		return;
	}

	swGroupDialog Dialog(gMainWindow, GetGroupName(tr("Group ")), mGroups);

	if (Dialog.exec() != QDialog::Accepted)
		return;

	BeginHistorySequence();
	BeginEditHistory();

	swGroup* NewGroup = GetGroup(Dialog.mName, true);

	for (const std::unique_ptr<swPiece>& Piece : mPieces)
	{
		if (Piece->IsSelected())
		{
			swGroup* Group = Piece->GetTopGroup();

			if (!Group)
				Piece->SetGroup(NewGroup);
			else if (Group != NewGroup)
				Group->mGroup = NewGroup;
		}
	}

	EndEditHistory();
	EndHistorySequence(tr("Group"));

	gMainWindow->UpdateSelectedObjects(true);
}

void swModel::UngroupSelection()
{
	if (!AnyPiecesSelected())
	{
		QMessageBox::information(gMainWindow, tr("Ungroup Selection"), tr("No pieces selected."));
		return;
	}

	std::set<swGroup*> SelectedGroups;

	BeginHistorySequence();
	BeginEditHistory();

	for (const std::unique_ptr<swPiece>& Piece : mPieces)
	{
		if (Piece->IsSelected())
		{
			swGroup* Group = Piece->GetTopGroup();

			if (SelectedGroups.insert(Group).second)
			{
				for (std::vector<std::unique_ptr<swGroup>>::iterator GroupIt = mGroups.begin(); GroupIt != mGroups.end(); GroupIt++)
				{
					if (GroupIt->get() == Group)
					{
						GroupIt->release();
						mGroups.erase(GroupIt);
						break;
					}
				}
			}
		}
	}

	if (SelectedGroups.empty())
	{
		DiscardHistorySequence();

		QMessageBox::information(gMainWindow, tr("Ungroup Selection"), tr("No groups selected."));

		return;
	}

	for (const std::unique_ptr<swPiece>& Piece : mPieces)
	{
		swGroup* Group = Piece->GetGroup();

		if (SelectedGroups.find(Group) != SelectedGroups.end())
			Piece->SetGroup(nullptr);
	}

	for (const std::unique_ptr<swGroup>& Group : mGroups)
		if (SelectedGroups.find(Group->mGroup) != SelectedGroups.end())
			Group->mGroup = nullptr;

	for (swGroup* Group : SelectedGroups)
		delete Group;

	RemoveEmptyGroups();

	EndEditHistory();
	EndHistorySequence(tr("Ungroup"));

	gMainWindow->UpdateSelectedObjects(true);
}

void swModel::AddSelectedPiecesToGroup()
{
	swGroup* Group = nullptr;

	for (const std::unique_ptr<swPiece>& Piece : mPieces)
	{
		if (Piece->IsSelected())
		{
			Group = Piece->GetTopGroup();
			if (Group)
				break;
		}
	}

	if (!Group)
		return;

	BeginHistorySequence();
	BeginEditHistory();

	for (const std::unique_ptr<swPiece>& Piece : mPieces)
	{
		if (Piece->IsFocused())
		{
			Piece->SetGroup(Group);
			break;
		}
	}

	RemoveEmptyGroups();

	EndEditHistory();
	EndHistorySequence(tr("Group"));

	gMainWindow->UpdateSelectedObjects(false);
}

void swModel::RemoveFocusPieceFromGroup()
{
	bool Modified = false;

	BeginHistorySequence();
	BeginEditHistory();

	for (const std::unique_ptr<swPiece>& Piece : mPieces)
	{
		if (Piece->IsFocused())
		{
			Piece->SetGroup(nullptr);

			Modified = true;

			break;
		}
	}

	if (!Modified)
	{
		DiscardHistorySequence();

		return;
	}

	RemoveEmptyGroups();

	EndEditHistory();
	EndHistorySequence(tr("Ungroup"));
}

void swModel::ShowEditGroupsDialog()
{
	swEditGroupsDialog Dialog(gMainWindow, this);

	if (Dialog.exec() != QDialog::Accepted)
		return;

	BeginHistorySequence();
	BeginEditHistory();

	std::function<void(const swEditGroupsDialog::GroupInfo&, swGroup*)> UpdateGroups=[this, &UpdateGroups](const swEditGroupsDialog::GroupInfo& GroupInfo, swGroup* ParentGroup)
	{
		swGroup* Group = GroupInfo.Group;

		if (!Group)
		{
			Group = new swGroup();
			mGroups.emplace_back(Group);
		}

		Group->mName = GroupInfo.Name;
		Group->mGroup = ParentGroup;

		for (const swEditGroupsDialog::GroupInfo& ChildGroupInfo : GroupInfo.ChildGroups)
			UpdateGroups(ChildGroupInfo, Group);

		for (swPiece* Piece : GroupInfo.ChildPieces)
			Piece->SetGroup(Group);
	};

	swEditGroupsDialog::GroupInfo GroupInfo = Dialog.GetGroups();

	for (const swEditGroupsDialog::GroupInfo& ChildGroupInfo : GroupInfo.ChildGroups)
		UpdateGroups(ChildGroupInfo, nullptr);

	for (swPiece* Piece : GroupInfo.ChildPieces)
		Piece->SetGroup(nullptr);

	RemoveEmptyGroups();

	EndEditHistory();

	if (mHistorySequence.empty())
	{
		DiscardHistorySequence();
		return;
	}

	ClearSelection();

	EndHistorySequence(tr("Edit Groups"));

	gMainWindow->UpdateSelectedObjects(true);
}

QString swModel::GetGroupName(const QString& Prefix)
{
	int Max = 0;

	for (const std::unique_ptr<swGroup>& Group : mGroups)
		Max = qMax(Max, swGetDefaultNameNumber(Group->mName, Prefix));

	return swFormatDefaultName(Prefix, Max + 1);
}

void swModel::RemoveEmptyGroups()
{
	bool Removed;

	do
	{
		Removed = false;

		for (std::vector<std::unique_ptr<swGroup>>::iterator GroupIt = mGroups.begin(); GroupIt != mGroups.end();)
		{
			swGroup* Group = GroupIt->get();
			int Ref = 0;

			for (const std::unique_ptr<swPiece>& Piece : mPieces)
				if (Piece->GetGroup() == Group)
					Ref++;

			for (size_t ParentIdx = 0; ParentIdx < mGroups.size(); ParentIdx++)
				if (mGroups[ParentIdx]->mGroup == Group)
					Ref++;

			if (Ref > 1)
			{
				GroupIt++;
				continue;
			}

			if (Ref != 0)
			{
				for (const std::unique_ptr<swPiece>& Piece : mPieces)
				{
					if (Piece->GetGroup() == Group)
					{
						Piece->SetGroup(Group->mGroup);
						break;
					}
				}

				for (size_t ParentIdx = 0; ParentIdx < mGroups.size(); ParentIdx++)
				{
					if (mGroups[ParentIdx]->mGroup == Group)
					{
						mGroups[ParentIdx]->mGroup = Group->mGroup;
						break;
					}
				}
			}

			GroupIt = mGroups.erase(GroupIt);
			Removed = true;
		}
	}
	while (Removed);
}
