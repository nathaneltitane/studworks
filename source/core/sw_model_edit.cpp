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

swVector3 swModel::SnapPosition(const swVector3& Distance) const
{
	swVector3 NewDistance(Distance);

	float SnapXY = gMainWindow->GetMoveXYSnap();
	if (SnapXY != 0.0f)
	{
		int i = (int)(NewDistance[0] / SnapXY);
		float Leftover = NewDistance[0] - (SnapXY * i);

		if (Leftover > SnapXY / 2)
			i++;
		else if (Leftover < -SnapXY / 2)
			i--;

		NewDistance[0] = SnapXY * i;

		i = (int)(NewDistance[1] / SnapXY);
		Leftover = NewDistance[1] - (SnapXY * i);

		if (Leftover > SnapXY / 2)
			i++;
		else if (Leftover < -SnapXY / 2)
			i--;

		NewDistance[1] = SnapXY * i;
	}

	float SnapZ = gMainWindow->GetMoveZSnap();
	if (SnapZ != 0.0f)
	{
		int i = (int)(NewDistance[2] / SnapZ);
		const float Leftover = NewDistance[2] - (SnapZ * i);

		if (Leftover > SnapZ / 2)
			i++;
		else if (Leftover < -SnapZ / 2)
			i--;

		NewDistance[2] = SnapZ * i;
	}

	return NewDistance;
}

swVector3 swModel::SnapRotation(const swVector3& Angles) const
{
	const float AngleSnap = gMainWindow->GetAngleSnap();
	swVector3 NewAngles(Angles);

	if (AngleSnap != 0.0f)
	{
		int Snap[3];

		for (int i = 0; i < 3; i++)
			Snap[i] = (int)(Angles[i] / AngleSnap);

		NewAngles = swVector3((float)(AngleSnap * Snap[0]), (float)(AngleSnap * Snap[1]), (float)(AngleSnap * Snap[2]));
	}

	return NewAngles;
}

swMatrix33 swModel::GetRelativeRotation() const
{
	if (gMainWindow->GetRelativeTransform())
	{
		const swObject* Focus = GetFocusObject();

		if (Focus)
		{
			if (Focus->IsPart())
				return ((swPart*)Focus)->GetRelativeRotation();

			if (Focus->IsLight())
				return ((swLight*)Focus)->GetRelativeRotation();
		}
	}

	return swMatrix33Identity();
}

swPart* swModel::AddPart(swPartData* PartData, quint32 Section)
{
	if (!PartData)
		PartData = gMainWindow->GetInsertPartData().Get();

	if (!PartData)
		return nullptr;

	const swPartDataReference PartDataRef(PartData);

	swPart* Last = mParts.empty() ? nullptr : mParts.back().get();

	for (const std::unique_ptr<swPart>& Part : mParts)
	{
		if (Part->IsFocused())
		{
			Last = Part.get();
			break;
		}
	}

	swGetPartsLibrary()->EnsurePartDataAssetsReady(PartDataRef);

	if (Last)
	{
		swGetPartsLibrary()->EnsurePartDataAssetsReady(Last->mPartData);

		if (Last->mPartData->GetSynthInfo())
			swGetPartsLibrary()->WaitForSynthMeshes({ Last });
	}

	const swBoundingBox& PartDataBoundingBox = PartData->GetBoundingBox();
	swPart* Part = nullptr;

	auto CreatePart=[this, &Part](swPartData* PartData, const swMatrix44& WorldMatrix, int ColorIndex)
	{
		Part = new swPart(PartData);
		Part->Initialize(WorldMatrix, mCurrentStep);
		Part->SetColorIndex(ColorIndex);
		Part->AddInsertedControlPoints();
		AddPart(Part);
	};

	BeginHistorySequence();
	BeginEditHistory();

	if (Last)
	{
		std::vector<swInsertPartInformation> TrainTracks;

		if (PartData->GetTrainTrackInfo() && Last->mPartData->GetTrainTrackInfo())
		{
			quint32 FocusSection = Last->GetFocusSection();

			if (FocusSection != SW_PART_SECTION_INVALID && FocusSection >= SW_PART_SECTION_TRAIN_TRACK_CONNECTION_FIRST)
				Section = FocusSection;

			if (!Last->IsFocused())
				UpdateTrainTrackConnections(Last, false);

			TrainTracks = swTrainTrackInformation::GetInsertPartInfo(Last, PartData, nullptr, gMainWindow->GetInsertColorIndex(), Section, true, std::nullopt);

			for (const swInsertPartInformation& TrainTrack : TrainTracks)
				CreatePart(TrainTrack.PartData, TrainTrack.Transform, TrainTrack.ColorIndex);
		}

		if (TrainTracks.empty())
		{
			const swBoundingBox& LastBoundingBox = Last->GetBoundingBox();
			swVector3 Dist(0, 0, LastBoundingBox.Max.z - PartDataBoundingBox.Min.z);
			Dist = SnapPosition(Dist);

			swMatrix44 WorldMatrix = Last->mModelWorld;
			WorldMatrix.SetTranslation(swMultiply31(Dist, Last->mModelWorld));

			CreatePart(PartData, WorldMatrix, gMainWindow->GetInsertColorIndex());
		}
	}
	else
	{
		swMatrix44 WorldMatrix = swGetPreferences().mFirstPartAtOrigin ? swMatrix44Identity() : swMatrix44Translation(swVector3(0.0f, 0.0f, -PartDataBoundingBox.Min.z));

		CreatePart(PartData, WorldMatrix, gMainWindow->GetInsertColorIndex());
	}

	if (!Part)
	{
		DiscardHistorySequence();

		return nullptr;
	}

	EndEditHistory();

	gMainWindow->SetLastInsertedColorIndex(Part->GetColorIndex());

	SetSelectionAndFocus(std::vector<swObject*>(), Part, SW_PART_SECTION_POSITION, swSelectionMode::Single);

	EndHistorySequence(tr("Add Part"));

	gMainWindow->UpdateTimeline(false, false);

	return Part;
}

void swModel::AddPart(std::unique_ptr<swPart> Part, size_t PartIndex)
{
	mParts.insert(mParts.begin() + PartIndex, std::move(Part));
}

size_t swModel::AddPart(swPart* Part)
{
	size_t PartIndex;

	for (PartIndex = 0; PartIndex < mParts.size(); PartIndex++)
	{
		if (mParts[PartIndex]->GetStepShow() > Part->GetStepShow())
		{
			AddPart(std::unique_ptr<swPart>(Part), PartIndex);

			return PartIndex;
		}
	}

	AddPart(std::unique_ptr<swPart>(Part), PartIndex);

	return PartIndex;
}

void swModel::FocusNextTrainTrack()
{
	const swObject* Focus = GetFocusObject();

	if (!Focus || !Focus->IsPart())
		return;

	swPart* FocusPart = (swPart*)Focus;
	const swTrainTrackInformation* TrainTrackInfo = FocusPart->mPartData->GetTrainTrackInfo();

	if (!TrainTrackInfo)
		return;

	quint32 FocusSection = FocusPart->GetFocusSection();
	int ConnectionIndex = 0;

	if (FocusSection != SW_PART_SECTION_INVALID && FocusSection != SW_PART_SECTION_POSITION)
	{
		ConnectionIndex = FocusSection - SW_PART_SECTION_TRAIN_TRACK_CONNECTION_FIRST;
		ConnectionIndex = (ConnectionIndex + 1) % TrainTrackInfo->GetConnections().size();
	}

	BeginHistorySequence();
	SetFocus(FocusPart, SW_PART_SECTION_TRAIN_TRACK_CONNECTION_FIRST + ConnectionIndex, swSelectionMode::Single);
	EndHistorySequence(tr("Selection"));
}

void swModel::FocusPreviousTrainTrack()
{
	const swObject* Focus = GetFocusObject();

	if (!Focus || !Focus->IsPart())
		return;

	swPart* FocusPart = (swPart*)Focus;
	const swTrainTrackInformation* TrainTrackInfo = FocusPart->mPartData->GetTrainTrackInfo();

	if (!TrainTrackInfo)
		return;

	quint32 FocusSection = FocusPart->GetFocusSection();
	int ConnectionIndex = 0;

	if (FocusSection != SW_PART_SECTION_INVALID && FocusSection != SW_PART_SECTION_POSITION)
	{
		ConnectionIndex = FocusSection - SW_PART_SECTION_TRAIN_TRACK_CONNECTION_FIRST;
		ConnectionIndex = (ConnectionIndex + static_cast<int>(TrainTrackInfo->GetConnections().size()) - 1) % TrainTrackInfo->GetConnections().size();
	}

	BeginHistorySequence();
	SetFocus(FocusPart, SW_PART_SECTION_TRAIN_TRACK_CONNECTION_FIRST + ConnectionIndex, swSelectionMode::Single);
	EndHistorySequence(tr("Selection"));
}

void swModel::RotateFocusedTrainTrack(int Direction)
{
	const swObject* Focus = GetFocusObject();

	if (!Focus || !Focus->IsPart())
		return;

	swPart* FocusPart = (swPart*)Focus;
	const swTrainTrackInformation* TrainTrackInfo = FocusPart->mPartData->GetTrainTrackInfo();

	if (!TrainTrackInfo)
		return;

	quint32 FocusSection = FocusPart->GetFocusSection();
	std::optional<swMatrix44> Transform;
	int FirstConnectionIndex = 0, RotateConnectionIndex = 0, TracksConnected = 0;

	for (int ConnectionIndex = 0; ConnectionIndex < static_cast<int>(TrainTrackInfo->GetConnections().size()); ConnectionIndex++)
	{
		if (FocusPart->IsTrainTrackConnected(ConnectionIndex))
		{
			if (!TracksConnected)
				FirstConnectionIndex = ConnectionIndex;

			TracksConnected++;
		}
	}

	if (FocusSection != SW_PART_SECTION_INVALID && FocusSection != SW_PART_SECTION_POSITION)
	{
		RotateConnectionIndex = FocusSection - SW_PART_SECTION_TRAIN_TRACK_CONNECTION_FIRST;
	}
	else
	{
		RotateConnectionIndex = FirstConnectionIndex;

		if (TracksConnected > 1)
		{
			// todo: find most recent connection
		}
	}

	swMatrix44 ConnectionTransform = swMultiply(TrainTrackInfo->GetConnections()[RotateConnectionIndex].Transform, FocusPart->mModelWorld);
	int NewConnectionIndex = (RotateConnectionIndex + Direction + static_cast<int>(TrainTrackInfo->GetConnections().size())) % TrainTrackInfo->GetConnections().size();

	Transform = swTrainTrackInformation::CalculateTransformToConnection(ConnectionTransform, FocusPart->mPartData.Get(), NewConnectionIndex);

	if (!Transform)
		return;

	BeginHistorySequence();

	BeginEditHistory();

	FocusPart->SetPosition(Transform.value().GetTranslation(), mCurrentStep, gMainWindow->GetAddKeys());
	FocusPart->SetRotation(swMatrix33(Transform.value()), mCurrentStep, gMainWindow->GetAddKeys());
	FocusPart->UpdatePosition(mCurrentStep);

	EndEditHistory();

	if ((FocusSection != SW_PART_SECTION_INVALID && FocusSection != SW_PART_SECTION_POSITION) || !TracksConnected)
		SetFocus(FocusPart, SW_PART_SECTION_TRAIN_TRACK_CONNECTION_FIRST + NewConnectionIndex, swSelectionMode::Single);

	EndHistorySequence(tr("Rotate"));

	gMainWindow->UpdateSelectedObjects(true);
}

void swModel::UpdateTrainTrackConnections(swPart* TrackPart, bool IgnoreSelected) const
{
	if (!TrackPart)
		return;

	const swTrainTrackInformation* TrainTrackInfo = TrackPart->mPartData->GetTrainTrackInfo();

	if (!TrainTrackInfo)
		return;

	const int ConnectionCount = static_cast<int>(TrainTrackInfo->GetConnections().size());
	std::vector<bool> Connections(ConnectionCount, false);

	for (const std::unique_ptr<swPart>& Part : mParts)
	{
		if (Part.get() == TrackPart || !Part->mPartData->GetTrainTrackInfo() || (IgnoreSelected && Part->IsSelected()))
			continue;

		for (int ConnectionIndex = 0; ConnectionIndex < ConnectionCount; ConnectionIndex++)
			if (!Connections[ConnectionIndex] && swTrainTrackInformation::GetPartConnectionIndex(TrackPart, ConnectionIndex, Part.get()) != -1)
				Connections[ConnectionIndex] = true;
	}

	TrackPart->SetTrainTrackConnections(std::move(Connections));
}

void swModel::UpdateSelectedPartsTrainTrackConnections()
{
	std::vector<swPart *> TrackParts;

	for (const std::unique_ptr<swPart>& Part : mParts)
		if (Part->mPartData->GetTrainTrackInfo() && Part->IsSelected())
			TrackParts.push_back(Part.get());

	for (swPart* TrackPart : TrackParts)
	{
		const swTrainTrackInformation* TrainTrackInfo = TrackPart->mPartData->GetTrainTrackInfo();
		const int ConnectionCount = static_cast<int>(TrainTrackInfo->GetConnections().size());
		std::vector<bool> Connections(ConnectionCount, false);

		for (const swPart* CheckPart : TrackParts)
		{
			if (CheckPart == TrackPart)
				continue;

			for (int ConnectionIndex = 0; ConnectionIndex < ConnectionCount; ConnectionIndex++)
				if (!Connections[ConnectionIndex] && swTrainTrackInformation::GetPartConnectionIndex(TrackPart, ConnectionIndex, CheckPart) != -1)
					Connections[ConnectionIndex] = true;
		}

		TrackPart->SetTrainTrackConnections(std::move(Connections));
	}
}

void swModel::SetPreviewInsertPartInfo(std::vector<swInsertPartInformation>&& PreviewInsertPartInfo)
{
	swPartsLibrary* Library = swGetPartsLibrary();
	std::vector<swPartDataReference> RequiredPartData;
	std::vector<swPreviewInsertPartInformation> PreviewParts;

	RequiredPartData.reserve(PreviewInsertPartInfo.size());
	PreviewParts.reserve(PreviewInsertPartInfo.size());

	for (const swInsertPartInformation& InfoTransform : PreviewInsertPartInfo)
	{
		PreviewParts.emplace_back(InfoTransform);
		Library->RequestPartDataLoad(InfoTransform.PartData, swLoadPriority::Normal);
		RequiredPartData.push_back(PreviewParts.back().PartData);
	}

	Library->WaitForPartData(RequiredPartData);
	mPreviewInsertPartInfo = std::move(PreviewParts);

	UpdateAllViews();
}

void swModel::DeleteSelectedObjects()
{
	if (mIsPreview)
	{
		RemoveSelectedObjects();

		return;
	}

	// remove a focused control point alone
	const swPart* FocusPart = dynamic_cast<swPart*>(GetFocusObject());
	const quint32 FocusSection = FocusPart ? FocusPart->GetFocusSection() : SW_OBJECT_SECTION_INVALID;

	if (FocusSection >= SW_PART_SECTION_CONTROL_POINT_FIRST && FocusSection <= SW_PART_SECTION_CONTROL_POINT_LAST)
	{
		RemoveFocusedControlPoint();

		return;
	}

	BeginHistorySequence();
	BeginEditHistory();

	bool Modified = RemoveSelectedObjects();

	if (!Modified)
	{
		DiscardHistorySequence();

		return;
	}

	EndEditHistory();
	EndHistorySequence(tr("Delete"));

	gMainWindow->UpdateTimeline(false, false);
	gMainWindow->UpdateSelectedObjects(true);
	gMainWindow->UpdateInUseCategory();
}

void swModel::ResetSelectedPartsPivotPoint()
{
	BeginHistorySequence();
	BeginEditHistory();

	for (const std::unique_ptr<swPart>& Part : mParts)
		if (Part->IsSelected())
			Part->ResetPivotPoint();

	EndEditHistory();
	EndHistorySequence(tr("Reset Pivot Point"));
}

void swModel::RemoveSelectedObjectsKeyFrames()
{
	BeginHistorySequence();
	BeginEditHistory();

	for (const std::unique_ptr<swPart>& Part : mParts)
		if (Part->IsSelected())
			Part->RemoveKeyFrames();

	for (const std::unique_ptr<swCamera>& Camera : mCameras)
		if (Camera->IsSelected())
			Camera->RemoveKeyFrames();

	for (const std::unique_ptr<swLight>& Light : mLights)
		if (Light->IsSelected())
			Light->RemoveKeyFrames();

	EndEditHistory();
	EndHistorySequence(tr("Remove Keyframes"));
}

void swModel::InsertControlPoint()
{
	swPart* Part = dynamic_cast<swPart*>(GetFocusObject());

	if (!Part)
		return;

	swVector3 Start, End;

	gMainWindow->GetActiveView()->GetRayUnderPointer(Start, End);

	BeginHistorySequence();
	BeginEditHistory();

	bool Modified = Part->InsertControlPoint(Start, End);

	if (!Modified)
	{
		DiscardHistorySequence();

		return;
	}

	EndEditHistory();
	EndHistorySequence(tr("Add Control Point"));

	gMainWindow->UpdateSelectedObjects(true);
}

void swModel::RemoveFocusedControlPoint()
{
	swPart* Part = dynamic_cast<swPart*>(GetFocusObject());

	if (!Part)
		return;

	BeginHistorySequence();
	BeginEditHistory();

	bool Modified = Part->RemoveFocusedControlPoint();

	if (!Modified)
	{
		DiscardHistorySequence();

		return;
	}

	EndEditHistory();
	EndHistorySequence(tr("Remove Control Point"));

	gMainWindow->UpdateSelectedObjects(true);
}

void swModel::ToggleFocusedControlPointTangent()
{
	swPart* Part = dynamic_cast<swPart*>(GetFocusObject());

	if (!Part)
		return;

	BeginHistorySequence();
	BeginEditHistory();

	const bool Modified = Part->ToggleFocusedControlPointTangent();

	if (!Modified)
	{
		DiscardHistorySequence();

		return;
	}

	EndEditHistory();
	EndHistorySequence(Part->IsControlPointTangentClamped() ? tr("Clamp Tangent") : tr("Release Tangent"));

	gMainWindow->UpdateSelectedObjects(true);
}

void swModel::AlignFocusedControlPointTangent()
{
	swPart* Part = dynamic_cast<swPart*>(GetFocusObject());

	if (!Part)
		return;

	BeginHistorySequence();
	BeginEditHistory();

	const bool Modified = Part->AlignFocusedControlPointTangent();

	if (!Modified)
	{
		DiscardHistorySequence();

		return;
	}

	EndEditHistory();
	EndHistorySequence(tr("Align to Tangent"));

	gMainWindow->UpdateSelectedObjects(true);
}

void swModel::AlignFocusedControlPointAxis(const swVector3& Axis)
{
	swPart* Part = dynamic_cast<swPart*>(GetFocusObject());

	if (!Part)
		return;

	BeginHistorySequence();
	BeginEditHistory();

	const bool Modified = Part->AlignFocusedControlPointAxis(Axis);

	if (!Modified)
	{
		DiscardHistorySequence();

		return;
	}

	EndEditHistory();
	EndHistorySequence(tr("Align to Axis"));

	gMainWindow->UpdateSelectedObjects(true);
}

void swModel::ShowSelectedPartsEarlier()
{
	BeginHistorySequence();
	BeginEditHistory();

	std::vector<swPart*> MovedParts;

	for (auto PartIt = mParts.begin(); PartIt != mParts.end(); )
	{
		swPart* Part = PartIt->get();

		if (Part->IsSelected())
		{
			swStep Step = Part->GetStepShow();

			if (Step > 1)
			{
				Step--;
				Part->SetStepShow(Step);

				MovedParts.emplace_back(PartIt->release());
				PartIt = mParts.erase(PartIt);
				continue;
			}
		}

		PartIt++;
	}

	if (MovedParts.empty())
	{
		DiscardHistorySequence();

		return;
	}

	for (swPart* Part : MovedParts)
	{
		Part->SetFileLine(-1);
		AddPart(Part);
	}

	EndEditHistory();
	EndHistorySequence(tr("Show Earlier"));

	gMainWindow->UpdateTimeline(false, false);
	gMainWindow->UpdateSelectedObjects(true);
}

void swModel::ShowSelectedPartsLater()
{
	BeginHistorySequence();
	BeginEditHistory();

	std::vector<swPart*> MovedParts;

	for (auto PartIt = mParts.begin(); PartIt != mParts.end(); )
	{
		swPart* Part = PartIt->get();

		if (Part->IsSelected())
		{
			swStep Step = Part->GetStepShow();

			if (Step < SW_STEP_MAXIMUM)
			{
				Step++;
				Part->SetStepShow(Step);

				MovedParts.emplace_back(PartIt->release());
				PartIt = mParts.erase(PartIt);

				continue;
			}
		}

		PartIt++;
	}

	if (MovedParts.empty())
	{
		DiscardHistorySequence();

		return;
	}

	for (swPart* Part : MovedParts)
	{
		Part->SetFileLine(-1);
		AddPart(Part);
	}

	EndEditHistory();
	EndHistorySequence(tr("Show Later"));

	gMainWindow->UpdateTimeline(false, false);
	gMainWindow->UpdateSelectedObjects(false);
}

void swModel::InsertStepAndMoveSelectedParts(swStep Step)
{
	BeginHistorySequence();
	BeginEditHistory();

	InsertStep(Step);

	std::vector<swPart*> MovedParts;

	for (auto PartIt = mParts.begin(); PartIt != mParts.end(); )
	{
		swPart* Part = PartIt->get();

		if (Part->IsSelected())
		{
			Part->SetStepShow(Step);

			MovedParts.emplace_back(PartIt->release());
			PartIt = mParts.erase(PartIt);

			continue;
		}

		PartIt++;
	}

	for (swPart* Part : MovedParts)
	{
		Part->SetFileLine(-1);
		AddPart(Part);
	}

	EndEditHistory();
	EndHistorySequence(tr("Show Parts"));

	gMainWindow->UpdateTimeline(false, false);
	gMainWindow->UpdateSelectedObjects(false);
}

void swModel::SetPartSteps(const std::vector<std::pair<swPart*, swStep>>& PartSteps)
{
	if (PartSteps.size() != mParts.size())
		return;

	BeginHistorySequence();
	BeginEditHistory();

	bool Modified = false;

	for (size_t PartIdx = 0; PartIdx < PartSteps.size(); PartIdx++)
	{
		const std::pair<swPart*, swStep>& PartStep = PartSteps[PartIdx];
		swPart* Part = mParts[PartIdx].get();

		if (Part != PartStep.first || Part->GetStepShow() != PartStep.second)
		{
			Part = PartStep.first;
			const swStep Step = PartStep.second;

			mParts[PartIdx].release();
			mParts[PartIdx] = std::unique_ptr<swPart>(Part);
			Part->SetStepShow(Step);

			Modified = true;
		}
	}

	if (Modified)
	{
		EndEditHistory();
		EndHistorySequence(tr("Change Step"));

		gMainWindow->UpdateTimeline(false, false);
		gMainWindow->UpdateSelectedObjects(false);
	}
	else
	{
		DiscardHistorySequence();
	}
}

void swModel::RenameParts(const std::vector<swModelPartRename>& Renames)
{
	if (Renames.empty())
		return;

	for (const std::unique_ptr<swPart>& Part : mParts)
	{
		for (const swModelPartRename& Rename : Renames)
		{
			if (Part->mPartData != Rename.PartData)
				continue;

			Part->UpdateID();
			break;
		}
	}

	auto RenameHistorySequence = [&Renames, ModelPartData = mPartData.Get()](std::vector<std::unique_ptr<swModelHistory>>& HistorySequence)
	{
		for (const std::unique_ptr<swModelHistory>& ModelHistory : HistorySequence)
			ModelHistory->RenameParts(Renames, ModelPartData);
	};

	RenameHistorySequence(mHistorySequence);

	for (const std::unique_ptr<swModelHistoryEntry>& HistoryEntry : mUndoHistory)
		RenameHistorySequence(HistoryEntry->HistorySequence);

	for (const std::unique_ptr<swModelHistoryEntry>& HistoryEntry : mRedoHistory)
		RenameHistorySequence(HistoryEntry->HistorySequence);
}

void swModel::MoveSelectionToModel(swModel* Model)
{
	if (!Model)
		return;

	BeginHistorySequence();
	BeginEditHistory();

	std::vector<swPart*> Parts;
	swPart* ModelPart = nullptr;
	swStep FirstStep = SW_STEP_MAXIMUM;
	swVector3 Min(FLT_MAX, FLT_MAX, FLT_MAX), Max(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	for (size_t PartIndex = 0; PartIndex < mParts.size(); )
	{
		swPart* Part = mParts[PartIndex].get();

		if (Part->IsSelected())
		{
			Part->CompareBoundingBox(Min, Max);
			mParts[PartIndex].release();
			mParts.erase(mParts.begin() + PartIndex);
			Part->SetGroup(nullptr); // todo: copy groups
			Parts.emplace_back(Part);
			FirstStep = qMin(FirstStep, Part->GetStepShow());

			if (!ModelPart)
			{
				ModelPart = new swPart(Model->mPartData);
				ModelPart->SetColorIndex(gDefaultColor);
				AddPart(std::unique_ptr<swPart>(ModelPart), PartIndex);
				PartIndex++;
			}
		}
		else
			PartIndex++;
	}

	if (Parts.empty())
	{
		DiscardHistorySequence();

		return;
	}

	swVector3 ModelCenter = (Min + Max) / 2.0f;
	ModelCenter.z += (Min.z - Max.z) / 2.0f;

	for (swPart* Part : Parts)
	{
		Part->SetFileLine(-1);
		Part->SetStepShow(Part->GetStepShow() - FirstStep + 1);
		Part->MoveSelected(Part->GetStepShow(), false, -ModelCenter);
		Model->AddPart(Part);
	}

	std::vector<swModel*> UpdatedModels;
	Model->UpdatePartData(UpdatedModels);
	if (ModelPart)
	{
		ModelPart->Initialize(swMatrix44Translation(ModelCenter), FirstStep);
		ModelPart->UpdatePosition(mCurrentStep);
	}

	EndEditHistory();

	gMainWindow->UpdateTimeline(false, false);

	SetSelectionAndFocus(std::vector<swObject*>(), ModelPart, SW_PART_SECTION_POSITION, swSelectionMode::Single);

	EndHistorySequence(tr("Move to Model"));
}

void swModel::InlineSelectedModels()
{
	BeginHistorySequence();
	BeginEditHistory();

	std::vector<swObject*> NewParts;
	bool Modified = false;

	for (size_t PartIndex = 0; PartIndex < mParts.size(); )
	{
		swPart* Part = mParts[PartIndex].get();

		if (!Part->IsSelected() || !Part->mPartData->IsModel())
		{
			PartIndex++;
			continue;
		}

		mParts[PartIndex].release();
		mParts.erase(mParts.begin() + PartIndex);

		swModel* Model = Part->mPartData->GetModel();

		Modified = true;

		for (const std::unique_ptr<swPart>& ModelPart : Model->mParts)
		{
			swPart* NewPart = new swPart(nullptr);

			// todo: recreate in groups in the current model

			int ColorIndex = ModelPart->GetColorIndex();

			if (ColorIndex == gDefaultColor)
				ColorIndex = Part->GetColorIndex();

			NewPart->SetPartData(ModelPart->mPartData, ModelPart->GetID(), true, true);
			NewPart->Initialize(swMultiply(ModelPart->mModelWorld, Part->mModelWorld), Part->GetStepShow());
			NewPart->SetColorIndex(ColorIndex);
			NewPart->UpdatePosition(mCurrentStep);

			NewParts.emplace_back(NewPart);
			AddPart(std::unique_ptr<swPart>(NewPart), PartIndex);
			PartIndex++;
		}

		delete Part;
	}

	if (!Modified)
	{
		QMessageBox::information(gMainWindow, tr("Inline Selected Models"), tr("No models selected."));

		DiscardHistorySequence();

		return;
	}

	EndEditHistory();

	SetSelectionAndFocus(NewParts, nullptr, 0, swSelectionMode::Single);

	EndHistorySequence(tr("Inline Model"));

	gMainWindow->UpdateTimeline(false, false);
	gMainWindow->UpdateInUseCategory();
}

void swModel::RemoveCameraFromViews(swCamera* Camera)
{
	std::vector<swView*> Views = swView::GetModelViews(this);

	for (swView* View : Views)
		if (Camera == View->GetCamera())
			View->SetCamera(Camera, true);
}

bool swModel::RemoveSelectedObjects()
{
	bool RemovedPart = false;
	bool RemovedCamera = false;
	bool RemovedLight = false;

	for (std::vector<std::unique_ptr<swPart>>::iterator PartIt = mParts.begin(); PartIt != mParts.end(); )
	{
		swPart* Part = PartIt->get();

		if (Part->IsSelected())
		{
			RemovedPart = true;
			PartIt = mParts.erase(PartIt);
		}
		else
			PartIt++;
	}

	for (std::vector<std::unique_ptr<swCamera>>::iterator CameraIt = mCameras.begin(); CameraIt != mCameras.end(); )
	{
		swCamera* Camera = CameraIt->get();

		if (Camera->IsSelected())
		{
			RemoveCameraFromViews(Camera);

			RemovedCamera = true;
			CameraIt = mCameras.erase(CameraIt);
		}
		else
			CameraIt++;
	}

	for (std::vector<std::unique_ptr<swLight>>::iterator LightIt = mLights.begin(); LightIt != mLights.end(); )
	{
		swLight* Light = LightIt->get();

		if (Light->IsSelected())
		{
			RemovedLight = true;
			LightIt = mLights.erase(LightIt);
		}
		else
			LightIt++;
	}

	RemoveEmptyGroups();

	return RemovedPart || RemovedCamera || RemovedLight;
}

void swModel::MoveSelectedObjects(const swVector3& PartDistance, const swVector3& ObjectDistance, swModelTransformFlags Flags, swModelHistoryEditMerge ModelHistoryEditMerge)
{
	if (Flags.testFlag(swModelTransformFlag::Checkpoint))
	{
		BeginHistorySequence();
		BeginMergeableEditHistory(ModelHistoryEditMerge, swObjectPropertyId::Count);
	}

	bool Moved = false;
	swMatrix33 RelativeRotation;

	if (Flags.testFlag(swModelTransformFlag::Relative))
		RelativeRotation = GetRelativeRotation();
	else
		RelativeRotation = swMatrix33Identity();

	if (PartDistance.LengthSquared() >= 0.001f)
	{
		swVector3 TransformedPartDistance = swMultiply(PartDistance, RelativeRotation);

		if (Flags.testFlag(swModelTransformFlag::RotatePivotPoint))
		{
			for (const std::unique_ptr<swPart>& Part : mParts)
			{
				if (Part->IsFocused())
				{
					Part->MovePivotPoint(TransformedPartDistance);
					Moved = true;
					break;
				}
			}
		}
		else
		{
			for (const std::unique_ptr<swPart>& Part : mParts)
			{
				if (Part->IsSelected())
				{
					if (gMainWindow->GetSeparateTransform())
						TransformedPartDistance = swMultiply(PartDistance, Part->GetRelativeRotation());

					Part->MoveSelected(mCurrentStep, gMainWindow->GetAddKeys(), TransformedPartDistance);
					Part->UpdatePosition(mCurrentStep);
					Moved = true;
				}
			}
		}
	}

	if (ObjectDistance.LengthSquared() >= 0.001f && !Flags.testFlag(swModelTransformFlag::RotatePivotPoint))
	{
		const swVector3 TransformedObjectDistance = swMultiply(ObjectDistance, RelativeRotation);

		for (const std::unique_ptr<swCamera>& Camera : mCameras)
		{
			if (Camera->IsSelected())
			{
				Camera->MoveSelected(mCurrentStep, gMainWindow->GetAddKeys(), TransformedObjectDistance);
				Camera->UpdatePosition(mCurrentStep);
				Moved = true;
			}
		}

		for (const std::unique_ptr<swLight>& Light : mLights)
		{
			if (Light->IsSelected())
			{
				Light->MoveSelected(mCurrentStep, gMainWindow->GetAddKeys(), TransformedObjectDistance, Flags.testFlag(swModelTransformFlag::FirstMove));
				Light->UpdatePosition(mCurrentStep);
				Moved = true;
			}
		}
	}

	if (!Moved)
	{
		if (Flags.testFlag(swModelTransformFlag::Checkpoint))
			DiscardHistorySequence();

		return;
	}

	if (Flags.testFlag(swModelTransformFlag::Checkpoint))
	{
		EndEditHistory();
		EndHistorySequence(tr("Move"));

		RemoveFirstUndoIfUnchanged();
	}

	UpdateAllViews();
	gMainWindow->UpdateSelectedObjects(false);
}

void swModel::RotateSelectedObjects(const swVector3& Angles, swModelTransformFlags Flags, swModelHistoryEditMerge ModelHistoryEditMerge)
{
	if (Angles.LengthSquared() < 0.001f)
		return;

	swMatrix33 RotationMatrix = swMatrix33Identity();

	if (Angles[0] != 0.0f)
		RotationMatrix = swMultiply(swMatrix33RotationX(Angles[0] * SW_DEGREES_TO_RADIANS), RotationMatrix);

	if (Angles[1] != 0.0f)
		RotationMatrix = swMultiply(swMatrix33RotationY(Angles[1] * SW_DEGREES_TO_RADIANS), RotationMatrix);

	if (Angles[2] != 0.0f)
		RotationMatrix = swMultiply(swMatrix33RotationZ(Angles[2] * SW_DEGREES_TO_RADIANS), RotationMatrix);

	RotateSelectedObjects(RotationMatrix, Flags, ModelHistoryEditMerge);
}

void swModel::RotateSelectedObjects(const swMatrix33& InputRotationMatrix, swModelTransformFlags Flags, swModelHistoryEditMerge ModelHistoryEditMerge)
{
	if (Flags.testFlag(swModelTransformFlag::Checkpoint))
	{
		BeginHistorySequence();
		BeginMergeableEditHistory(ModelHistoryEditMerge, swObjectPropertyId::Count);
	}

	swMatrix33 RotationMatrix = InputRotationMatrix;
	bool Rotated = false;

	if (Flags.testFlag(swModelTransformFlag::RotatePivotPoint))
	{
		swObject* Focus = GetFocusObject();

		if (Focus && Focus->IsPart())
		{
			((swPart*)Focus)->RotatePivotPoint(RotationMatrix);
			Rotated = true;
		}
	}
	else
	{
		int SelectionFlags;
		std::vector<swObject*> Selection;
		swObject* Focus;

		GetSelectionInformation(&SelectionFlags, Selection, &Focus);
		Q_UNUSED(SelectionFlags);
		Q_UNUSED(Focus);

		if (!gMainWindow->GetSeparateTransform())
		{
			swVector3 Center;
			swMatrix33 RelativeRotation;

			GetMoveRotateTransform(Center, RelativeRotation);

			swMatrix33 WorldToFocusMatrix;

			if (Flags.testFlag(swModelTransformFlag::Relative))
			{
				WorldToFocusMatrix = swMatrix33AffineInverse(RelativeRotation);
				RotationMatrix = swMultiply(RotationMatrix, RelativeRotation);
			}
			else
				WorldToFocusMatrix = swMatrix33Identity();

			for (swObject* Object : Selection)
			{
				if (Object->IsPart())
				{
					swPart* Part = (swPart*)Object;

					Part->Rotate(mCurrentStep, gMainWindow->GetAddKeys(), RotationMatrix, Center, WorldToFocusMatrix);
					Part->UpdatePosition(mCurrentStep);
					Rotated = true;
				}
				else if (Object->IsCamera())
				{
					swCamera* Camera = (swCamera*)Object;

					Camera->Rotate(mCurrentStep, gMainWindow->GetAddKeys(), RotationMatrix, Center, WorldToFocusMatrix);
					Camera->UpdatePosition(mCurrentStep);
					Rotated = true;
				}
				else if (Object->IsLight())
				{
					swLight* Light = (swLight*)Object;

					Light->Rotate(mCurrentStep, gMainWindow->GetAddKeys(), RotationMatrix, Center, WorldToFocusMatrix);
					Light->UpdatePosition(mCurrentStep);
					Rotated = true;
				}
			}
		}
		else
		{
			for (swObject* Object : Selection)
			{
				if (Object->IsPart())
				{
					swPart* Part = (swPart*)Object;

					const swVector3 Center = Part->GetRotationCenter();
					swMatrix33 WorldToFocusMatrix;
					swMatrix33 RelativeRotationMatrix;

					if (Flags.testFlag(swModelTransformFlag::Relative))
					{
						const swMatrix33 RelativeRotation = Part->GetRelativeRotation();
						WorldToFocusMatrix = swMatrix33AffineInverse(RelativeRotation);
						RelativeRotationMatrix = swMultiply(RotationMatrix, RelativeRotation);
					}
					else
					{
						WorldToFocusMatrix = swMatrix33Identity();
						RelativeRotationMatrix = RotationMatrix;
					}

					Part->Rotate(mCurrentStep, gMainWindow->GetAddKeys(), RelativeRotationMatrix, Center, WorldToFocusMatrix);
					Part->UpdatePosition(mCurrentStep);
					Rotated = true;
				}
				else if (Object->IsCamera())
				{
					swCamera* Camera = (swCamera*)Object;

					const swVector3 Center = Camera->GetRotationCenter();
					swMatrix33 WorldToFocusMatrix;
					swMatrix33 RelativeRotationMatrix;

					if (Flags.testFlag(swModelTransformFlag::Relative))
					{
						const swMatrix33 RelativeRotation = Camera->GetRelativeRotation();
						WorldToFocusMatrix = swMatrix33AffineInverse(RelativeRotation);
						RelativeRotationMatrix = swMultiply(RotationMatrix, RelativeRotation);
					}
					else
					{
						WorldToFocusMatrix = swMatrix33Identity();
						RelativeRotationMatrix = RotationMatrix;
					}

					Camera->Rotate(mCurrentStep, gMainWindow->GetAddKeys(), RotationMatrix, Center, WorldToFocusMatrix);
					Camera->UpdatePosition(mCurrentStep);
					Rotated = true;
				}
				else if (Object->IsLight())
				{
					swLight* Light = (swLight*)Object;

					const swVector3 Center = Light->GetRotationCenter();
					swMatrix33 WorldToFocusMatrix;
					swMatrix33 RelativeRotationMatrix;

					if (Flags.testFlag(swModelTransformFlag::Relative))
					{
						const swMatrix33 RelativeRotation = Light->GetRelativeRotation();
						WorldToFocusMatrix = swMatrix33AffineInverse(RelativeRotation);
						RelativeRotationMatrix = swMultiply(RotationMatrix, RelativeRotation);
					}
					else
					{
						WorldToFocusMatrix = swMatrix33Identity();
						RelativeRotationMatrix = RotationMatrix;
					}

					Light->Rotate(mCurrentStep, gMainWindow->GetAddKeys(), RotationMatrix, Center, WorldToFocusMatrix);
					Light->UpdatePosition(mCurrentStep);
					Rotated = true;
				}
			}
		}
	}

	if (!Rotated)
	{
		if (Flags.testFlag(swModelTransformFlag::Checkpoint))
			DiscardHistorySequence();

		return;
	}

	if (Flags.testFlag(swModelTransformFlag::Checkpoint))
	{
		EndEditHistory();
		EndHistorySequence(tr("Rotate"));

		RemoveFirstUndoIfUnchanged();
	}

	UpdateAllViews();
	gMainWindow->UpdateSelectedObjects(false);
}

void swModel::ScaleSelectedParts(const float Scale)
{
	if (Scale < 0.001f)
		return;

	swObject* Focus = GetFocusObject();
	if (!Focus || !Focus->IsPart())
		return;

	swPart* Part = (swPart*)Focus;
	const quint32 Section = Part->GetFocusSection();

	if (Section >= SW_PART_SECTION_CONTROL_POINT_FIRST && Section <= SW_PART_SECTION_CONTROL_POINT_LAST)
	{
		const int ControlPointIndex = Section - SW_PART_SECTION_CONTROL_POINT_FIRST;
		Part->SetControlPointHandleLength(ControlPointIndex, Scale);
	}
}

void swModel::TransformSelectedObjects(swTransformType TransformType, const swVector3& Transform)
{
	switch (TransformType)
	{
	case swTransformType::AbsoluteTranslation:
		MoveSelectedObjects(Transform, swModelTransformFlag::Checkpoint | swModelTransformFlag::FirstMove, swModelHistoryEditMerge::None);
		break;

	case swTransformType::RelativeTranslation:
		MoveSelectedObjects(Transform, swModelTransformFlag::Relative | swModelTransformFlag::Checkpoint | swModelTransformFlag::FirstMove, swModelHistoryEditMerge::None);
		break;

	case swTransformType::AbsoluteRotation:
		RotateSelectedObjects(Transform, swModelTransformFlag::Checkpoint, swModelHistoryEditMerge::None);
		break;

	case swTransformType::RelativeRotation:
		RotateSelectedObjects(Transform, swModelTransformFlag::Relative | swModelTransformFlag::Checkpoint, swModelHistoryEditMerge::None);
		break;

	case swTransformType::Count:
		break;
	}
}

void swModel::SetObjectsKeyFrame(const std::vector<swObject*>& Objects, swObjectPropertyId PropertyId, bool KeyFrame)
{
	BeginHistorySequence();
	BeginEditHistory();

	bool Modified = false;

	for (swObject* Object : Objects)
	{
		Modified |= Object->SetKeyFrame(PropertyId, mCurrentStep, KeyFrame);
		Object->UpdatePosition(mCurrentStep);
	}

	if (Modified)
	{
		EndEditHistory();
		EndHistorySequence(KeyFrame ? tr("Add Keyframe") : tr("Remove Keyframe"));

		gMainWindow->UpdateSelectedObjects(false);
	}
	else
	{
		DiscardHistorySequence();
	}
}

void swModel::SetSelectedPartsColorIndex(int ColorIndex)
{
	BeginHistorySequence();
	BeginEditHistory();

	bool Modified = false;

	for (const std::unique_ptr<swPart>& Part : mParts)
	{
		if (Part->IsSelected() && Part->GetColorIndex() != ColorIndex)
		{
			Part->SetColorIndex(ColorIndex);
			Modified = true;
		}
	}

	if (Modified)
	{
		EndEditHistory();
		EndHistorySequence(tr("Paint"));

		gMainWindow->UpdateSelectedObjects(false);
		gMainWindow->UpdateTimeline(false, true);
	}
	else
	{
		DiscardHistorySequence();
	}
}

void swModel::SetSelectedPartsStepShow(swStep Step)
{
	std::vector<swPart*> MovedParts;

	BeginHistorySequence();
	BeginEditHistory();

	for (auto PartIt = mParts.begin(); PartIt != mParts.end(); )
	{
		swPart* Part = PartIt->get();

		if (Part->IsSelected() && Part->GetStepShow() != Step)
		{
			Part->SetStepShow(Step);

			MovedParts.emplace_back(PartIt->release());
			PartIt = mParts.erase(PartIt);
			continue;
		}

		PartIt++;
	}

	if (MovedParts.empty())
	{
		DiscardHistorySequence();

		return;
	}

	for (swPart* Part : MovedParts)
	{
		Part->SetFileLine(-1);
		AddPart(Part);
	}

	EndEditHistory();
	EndHistorySequence(tr("Set Show Step"));

	gMainWindow->UpdateTimeline(false, false);
	gMainWindow->UpdateSelectedObjects(false);
}

void swModel::SetSelectedPartsStepHide(swStep Step)
{
	bool Modified = false;

	BeginHistorySequence();
	BeginEditHistory();

	for (const std::unique_ptr<swPart>& Part : mParts)
	{
		if (Part->IsSelected() && Part->GetStepHide() != Step)
		{
			Part->SetStepHide(Step);

			Modified = true;
		}
	}

	if (!Modified)
	{
		DiscardHistorySequence();

		return;
	}

	EndEditHistory();
	EndHistorySequence(tr("Set Hide Step"));

	gMainWindow->UpdateTimeline(false, false);
	gMainWindow->UpdateSelectedObjects(false);
}

void swModel::SetCameraProjection(swCamera* Camera, swCameraProjection CameraProjection)
{
	if (Camera->GetProjection() == CameraProjection)
		return;

	if (!Camera->IsSimple())
	{
		BeginHistorySequence();
		BeginEditHistory();
	}

	Camera->SetProjection(CameraProjection);
	Camera->UpdatePosition(mCurrentStep);

	if (!Camera->IsSimple())
	{
		EndEditHistory();
		EndHistorySequence(tr("Change Projection"));
	}

	UpdateAllViews();
	gMainWindow->UpdateSelectedObjects(false);
}

void swModel::SetObjectsProperty(const std::vector<swObject*>& Objects, swObjectPropertyId PropertyId, QVariant Value)
{
	const bool PropertyEditActive = mActivePropertyEdit == PropertyId;

	if (!PropertyEditActive)
	{
		BeginHistorySequence();
		BeginMergeableEditHistory(swModelHistoryEditMerge::PropertiesEdit, PropertyId);
	}

	bool Modified = false;

	for (swObject* Object : Objects)
	{
		bool ObjectModified = Object->SetPropertyValue(PropertyId, mCurrentStep, gMainWindow->GetAddKeys(), Value);

		if (ObjectModified)
		{
			Object->UpdatePosition(mCurrentStep);
			Modified = true;
		}
	}

	if (!Modified)
	{
		if (!PropertyEditActive)
			DiscardHistorySequence();

		return;
	}

	if (!PropertyEditActive)
	{
		EndEditHistory();
		EndHistorySequence(swObject::GetCheckpointString(PropertyId));

		RemoveFirstUndoIfUnchanged();
	}

	gMainWindow->UpdateSelectedObjects(false);

	// todo: fix hacky timeline update
	if (PropertyId == swObjectPropertyId::PartId || PropertyId == swObjectPropertyId::PartColor)
	{
		gMainWindow->UpdateTimeline(false, true);
	}

	// todo: fix hacky category update
	if (PropertyId == swObjectPropertyId::PartId)
	{
		gMainWindow->UpdateInUseCategory();
	}
}

void swModel::BeginPropertyEdit(swObjectPropertyId PropertyId)
{
	if (mActivePropertyEdit != swObjectPropertyId::Count)
		EndPropertyEdit(mActivePropertyEdit, true);

	mActivePropertyEdit = PropertyId;
	BeginHistorySequence();
	BeginMergeableEditHistory(swModelHistoryEditMerge::PropertiesEdit, PropertyId);
}

void swModel::EndPropertyEdit(swObjectPropertyId PropertyId, bool Accept)
{
	if (mActivePropertyEdit != PropertyId)
		return;

	mActivePropertyEdit = swObjectPropertyId::Count;

	if (!Accept)
	{
		RevertHistorySequence();
		return;
	}

	EndEditHistory();

	switch (PropertyId)
	{
	case swObjectPropertyId::ObjectPositionX:
	case swObjectPropertyId::ObjectPositionY:
	case swObjectPropertyId::ObjectPositionZ:
	case swObjectPropertyId::CameraPositionX:
	case swObjectPropertyId::CameraPositionY:
	case swObjectPropertyId::CameraPositionZ:
	case swObjectPropertyId::CameraTargetX:
	case swObjectPropertyId::CameraTargetY:
	case swObjectPropertyId::CameraTargetZ:
	case swObjectPropertyId::CameraUpX:
	case swObjectPropertyId::CameraUpY:
	case swObjectPropertyId::CameraUpZ:
		EndHistorySequence(tr("Move"));
		break;

	case swObjectPropertyId::ObjectRotationX:
	case swObjectPropertyId::ObjectRotationY:
	case swObjectPropertyId::ObjectRotationZ:
		EndHistorySequence(tr("Rotate"));
		break;

	default:
		EndHistorySequence(swObject::GetCheckpointString(PropertyId));
		break;
	}

	RemoveFirstUndoIfUnchanged();
}
