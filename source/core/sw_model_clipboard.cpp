#include "sw_global.h"
#include "sw_model.h"
#include "sw_model_history.h"
#include "sw_object_part.h"
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
#include "sw_data_part.h"
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

void swModel::Merge(std::unique_ptr<swModel> Other)
{
	for (std::unique_ptr<swPart>& Part : Other->mParts)
	{
		Part->SetFileLine(-1);
		AddPart(Part.release());
	}

	Other->mParts.clear();

	for (std::unique_ptr<swCamera>& Camera : Other->mCameras)
	{
		Camera->CreateName(mCameras);
		mCameras.emplace_back(std::move(Camera));
	}

	Other->mCameras.clear();

	for (std::unique_ptr<swLight>& Light : Other->mLights)
	{
		Light->CreateName(mLights);
		mLights.emplace_back(std::move(Light));
	}

	Other->mLights.clear();

	for (std::vector<std::unique_ptr<swGroup>>::iterator GroupIt = Other->mGroups.begin(); GroupIt != Other->mGroups.end(); GroupIt++)
	{
		std::unique_ptr<swGroup>& Group = *GroupIt;
		Group->CreateName(mGroups);
		mGroups.emplace_back(std::move(Group));
	}

	Other->mGroups.clear();

	gMainWindow->UpdateTimeline(false, false);
}

swResult<void> swModel::Cut()
{
	if (!AnyObjectsSelected())
		return swResult<void>();

	const swResult<void> Result = Copy();

	if (!Result)
		return Result;

	BeginHistorySequence();
	BeginEditHistory();

	RemoveSelectedObjects();

	EndEditHistory();
	EndHistorySequence(tr("Cut"));

	gMainWindow->UpdateTimeline(false, false);
	gMainWindow->UpdateSelectedObjects(true);

	return swResult<void>();
}

swResult<void> swModel::Copy()
{
	if (mProject)
	{
		const swResult<QByteArray> File = mProject->SerializeModel(this, true, 0);

		if (!File)
			return swUnexpected(File.error());

		gApplication->ExportClipboard(File.value());
	}
	else
	{
		QByteArray File;
		QTextStream Stream(&File, QIODevice::WriteOnly);
		SaveLDraw(Stream, true, 0);
		gApplication->ExportClipboard(File);
	}

	return swResult<void>();
}

swResult<void> swModel::Paste(bool PasteToCurrentStep)
{
	if (gApplication->mClipboard.isEmpty())
		return swResult<void>();

	const swResult<swModel*> Imported = swGetActiveProject()->ImportClipboard(gApplication->mClipboard);

	if (!Imported)
		return swUnexpected(Imported.error());

	std::unique_ptr<swModel> Model(Imported.value());

	if (!Model)
		return swResult<void>();

	const std::vector<std::unique_ptr<swPart>>& PastedParts = Model->mParts;
	std::vector<swObject*> SelectedObjects;
	SelectedObjects.reserve(PastedParts.size());

	for (const std::unique_ptr<swPart>& Part : PastedParts)
	{
		Part->SetFileLine(-1);

		if (PasteToCurrentStep)
		{
			Part->SetStepShow(mCurrentStep);
			SelectedObjects.emplace_back(Part.get());
		}
		else
		{
			if (Part->GetStepShow() <= mCurrentStep)
				SelectedObjects.emplace_back(Part.get());
		}
	}

	if (PastedParts.empty())
		return swResult<void>();

	BeginHistorySequence();
	BeginEditHistory();

	Merge(std::move(Model));

	CalculateStep(mCurrentStep);

	EndEditHistory();

	if (SelectedObjects.size() == 1)
		SetSelectionAndFocus(std::vector<swObject*>(), SelectedObjects.front(), SW_PART_SECTION_POSITION, swSelectionMode::Single);
	else
		SetSelectionAndFocus(SelectedObjects, nullptr, 0, swSelectionMode::Single);

	EndHistorySequence(tr("Paste"));

	gMainWindow->UpdateTimeline(false, false);

	return swResult<void>();
}

void swModel::DuplicateSelectedParts()
{
	if (!AnyPartsSelected())
	{
		QMessageBox::information(gMainWindow, tr("Duplicate Parts"), tr("No parts selected."));
		return;
	}

	BeginHistorySequence();
	BeginEditHistory();

	std::vector<swObject*> NewParts;
	swPart* Focus = nullptr;
	std::map<swGroup*, swGroup*> GroupMap;

	std::function<swGroup*(swGroup*)> GetNewGroup = [this, &GroupMap, &GetNewGroup](swGroup* Group)
	{
		const auto GroupIt = GroupMap.find(Group);

		if (GroupIt != GroupMap.end())
			return GroupIt->second;
		else
		{
			swGroup* Parent = Group->mGroup ? GetNewGroup(Group->mGroup) : nullptr;
			QString GroupName = Group->mName;

			while (!GroupName.isEmpty())
			{
				const QChar Last = GroupName[GroupName.size() - 1];
				if (Last.isDigit())
					GroupName.chop(1);
				else
					break;
			}

			if (GroupName.isEmpty())
				GroupName = Group->mName;

			swGroup* NewGroup = AddGroup(GroupName, Parent);
			GroupMap[Group] = NewGroup;
			return NewGroup;
		}
	};

	for (size_t PartIndex = 0; PartIndex < mParts.size(); PartIndex++)
	{
		const swPart* Part = mParts[PartIndex].get();

		if (!Part->IsSelected())
			continue;

		swPart* NewPart = new swPart(*Part);
		NewPart->UpdatePosition(mCurrentStep);
		NewParts.emplace_back(NewPart);

		if (Part->IsFocused())
			Focus = NewPart;

		PartIndex++;
		AddPart(std::unique_ptr<swPart>(NewPart), PartIndex);

		swGroup* Group = Part->GetGroup();
		if (Group)
			NewPart->SetGroup(GetNewGroup(Group));
	}

	EndEditHistory();

	SetSelectionAndFocus(NewParts, Focus, SW_PART_SECTION_POSITION, swSelectionMode::Single);

	EndHistorySequence(tr("Duplicate"));

	gMainWindow->UpdateTimeline(false, false);
}

void swModel::PaintSelectedParts()
{
	SetSelectedPartsColorIndex(gMainWindow->mColorIndex);
}
