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
#include "sw_train_track.h"
#include "sw_file.h"
#include "sw_piece_data.h"
#include "sw_view.h"
#include "sw_minifigure_builder.h"
#include "sw_dialog_array.h"
#include "sw_dialog_group.h"
#include "sw_dialog_edit_groups.h"
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
			if (Focus->IsPiece())
				return ((swPiece*)Focus)->GetRelativeRotation();

			if (Focus->IsLight())
				return ((swLight*)Focus)->GetRelativeRotation();
		}
	}

	return swMatrix33Identity();
}

swPiece* swModel::AddPiece(swPieceData* PieceData, quint32 Section)
{
	if (!PieceData)
		PieceData = gMainWindow->GetInsertPieceData().Get();

	if (!PieceData)
		return nullptr;

	const swPieceDataReference PieceDataRef(PieceData);

	swPiece* Last = mPieces.empty() ? nullptr : mPieces.back().get();

	for (const std::unique_ptr<swPiece>& Piece : mPieces)
	{
		if (Piece->IsFocused())
		{
			Last = Piece.get();
			break;
		}
	}

	swGetPiecesLibrary()->EnsurePieceDataAssetsReady(PieceDataRef);

	if (Last)
	{
		swGetPiecesLibrary()->EnsurePieceDataAssetsReady(Last->mPieceData);

		if (Last->mPieceData->GetSynthInfo())
			swGetPiecesLibrary()->WaitForSynthMeshes({ Last });
	}

	const swBoundingBox& PieceDataBoundingBox = PieceData->GetBoundingBox();
	swPiece* Piece = nullptr;

	auto CreatePiece=[this, &Piece](swPieceData* PieceData, const swMatrix44& WorldMatrix, int ColorIndex)
	{
		Piece = new swPiece(PieceData);
		Piece->Initialize(WorldMatrix, mCurrentStep);
		Piece->SetColorIndex(ColorIndex);
		AddPiece(Piece);
	};

	BeginHistorySequence();
	BeginEditHistory();

	if (Last)
	{
		std::vector<swInsertPieceInformation> TrainTracks;

		if (PieceData->GetTrainTrackInfo() && Last->mPieceData->GetTrainTrackInfo())
		{
			quint32 FocusSection = Last->GetFocusSection();

			if (FocusSection != SW_PIECE_SECTION_INVALID && FocusSection >= SW_PIECE_SECTION_TRAIN_TRACK_CONNECTION_FIRST)
				Section = FocusSection;

			if (!Last->IsFocused())
				UpdateTrainTrackConnections(Last, false);

			TrainTracks = swTrainTrackInformation::GetInsertPieceInfo(Last, PieceData, nullptr, gMainWindow->GetInsertColorIndex(), Section, true, std::nullopt);

			for (const swInsertPieceInformation& TrainTrack : TrainTracks)
				CreatePiece(TrainTrack.PieceData, TrainTrack.Transform, TrainTrack.ColorIndex);
		}

		if (TrainTracks.empty())
		{
			const swBoundingBox& LastBoundingBox = Last->GetBoundingBox();
			swVector3 Dist(0, 0, LastBoundingBox.Max.z - PieceDataBoundingBox.Min.z);
			Dist = SnapPosition(Dist);

			swMatrix44 WorldMatrix = Last->mModelWorld;
			WorldMatrix.SetTranslation(swMultiply31(Dist, Last->mModelWorld));

			CreatePiece(PieceData, WorldMatrix, gMainWindow->GetInsertColorIndex());
		}
	}
	else
	{
		swMatrix44 WorldMatrix = swGetPreferences().mFirstPieceAtOrigin ? swMatrix44Identity() : swMatrix44Translation(swVector3(0.0f, 0.0f, -PieceDataBoundingBox.Min.z));

		CreatePiece(PieceData, WorldMatrix, gMainWindow->GetInsertColorIndex());
	}

	if (!Piece)
	{
		DiscardHistorySequence();

		return nullptr;
	}

	EndEditHistory();

	gMainWindow->SetLastInsertedColorIndex(Piece->GetColorIndex());

	SetSelectionAndFocus(std::vector<swObject*>(), Piece, SW_PIECE_SECTION_POSITION, swSelectionMode::Single);

	EndHistorySequence(tr("Add Piece"));

	gMainWindow->UpdateTimeline(false, false);

	return Piece;
}

void swModel::AddPiece(std::unique_ptr<swPiece> Piece, size_t PieceIndex)
{
	mPieces.insert(mPieces.begin() + PieceIndex, std::move(Piece));
}

size_t swModel::AddPiece(swPiece* Piece)
{
	size_t PieceIndex;

	for (PieceIndex = 0; PieceIndex < mPieces.size(); PieceIndex++)
	{
		if (mPieces[PieceIndex]->GetStepShow() > Piece->GetStepShow())
		{
			AddPiece(std::unique_ptr<swPiece>(Piece), PieceIndex);

			return PieceIndex;
		}
	}

	AddPiece(std::unique_ptr<swPiece>(Piece), PieceIndex);

	return PieceIndex;
}

void swModel::FocusNextTrainTrack()
{
	const swObject* Focus = GetFocusObject();

	if (!Focus || !Focus->IsPiece())
		return;

	swPiece* FocusPiece = (swPiece*)Focus;
	const swTrainTrackInformation* TrainTrackInfo = FocusPiece->mPieceData->GetTrainTrackInfo();

	if (!TrainTrackInfo)
		return;

	quint32 FocusSection = FocusPiece->GetFocusSection();
	int ConnectionIndex = 0;

	if (FocusSection != SW_PIECE_SECTION_INVALID && FocusSection != SW_PIECE_SECTION_POSITION)
	{
		ConnectionIndex = FocusSection - SW_PIECE_SECTION_TRAIN_TRACK_CONNECTION_FIRST;
		ConnectionIndex = (ConnectionIndex + 1) % TrainTrackInfo->GetConnections().size();
	}

	BeginHistorySequence();
	SetFocus(FocusPiece, SW_PIECE_SECTION_TRAIN_TRACK_CONNECTION_FIRST + ConnectionIndex, swSelectionMode::Single);
	EndHistorySequence(tr("Selection"));
}

void swModel::FocusPreviousTrainTrack()
{
	const swObject* Focus = GetFocusObject();

	if (!Focus || !Focus->IsPiece())
		return;

	swPiece* FocusPiece = (swPiece*)Focus;
	const swTrainTrackInformation* TrainTrackInfo = FocusPiece->mPieceData->GetTrainTrackInfo();

	if (!TrainTrackInfo)
		return;

	quint32 FocusSection = FocusPiece->GetFocusSection();
	int ConnectionIndex = 0;

	if (FocusSection != SW_PIECE_SECTION_INVALID && FocusSection != SW_PIECE_SECTION_POSITION)
	{
		ConnectionIndex = FocusSection - SW_PIECE_SECTION_TRAIN_TRACK_CONNECTION_FIRST;
		ConnectionIndex = (ConnectionIndex + static_cast<int>(TrainTrackInfo->GetConnections().size()) - 1) % TrainTrackInfo->GetConnections().size();
	}

	BeginHistorySequence();
	SetFocus(FocusPiece, SW_PIECE_SECTION_TRAIN_TRACK_CONNECTION_FIRST + ConnectionIndex, swSelectionMode::Single);
	EndHistorySequence(tr("Selection"));
}

void swModel::RotateFocusedTrainTrack(int Direction)
{
	const swObject* Focus = GetFocusObject();

	if (!Focus || !Focus->IsPiece())
		return;

	swPiece* FocusPiece = (swPiece*)Focus;
	const swTrainTrackInformation* TrainTrackInfo = FocusPiece->mPieceData->GetTrainTrackInfo();

	if (!TrainTrackInfo)
		return;

	quint32 FocusSection = FocusPiece->GetFocusSection();
	std::optional<swMatrix44> Transform;
	int FirstConnectionIndex = 0, RotateConnectionIndex = 0, TracksConnected = 0;

	for (int ConnectionIndex = 0; ConnectionIndex < static_cast<int>(TrainTrackInfo->GetConnections().size()); ConnectionIndex++)
	{
		if (FocusPiece->IsTrainTrackConnected(ConnectionIndex))
		{
			if (!TracksConnected)
				FirstConnectionIndex = ConnectionIndex;

			TracksConnected++;
		}
	}

	if (FocusSection != SW_PIECE_SECTION_INVALID && FocusSection != SW_PIECE_SECTION_POSITION)
	{
		RotateConnectionIndex = FocusSection - SW_PIECE_SECTION_TRAIN_TRACK_CONNECTION_FIRST;
	}
	else
	{
		RotateConnectionIndex = FirstConnectionIndex;

		if (TracksConnected > 1)
		{
			// todo: find most recent connection
		}
	}

	swMatrix44 ConnectionTransform = swMultiply(TrainTrackInfo->GetConnections()[RotateConnectionIndex].Transform, FocusPiece->mModelWorld);
	int NewConnectionIndex = (RotateConnectionIndex + Direction + static_cast<int>(TrainTrackInfo->GetConnections().size())) % TrainTrackInfo->GetConnections().size();

	Transform = swTrainTrackInformation::CalculateTransformToConnection(ConnectionTransform, FocusPiece->mPieceData.Get(), NewConnectionIndex);

	if (!Transform)
		return;

	BeginHistorySequence();

	BeginEditHistory();

	FocusPiece->SetPosition(Transform.value().GetTranslation(), mCurrentStep, gMainWindow->GetAddKeys());
	FocusPiece->SetRotation(swMatrix33(Transform.value()), mCurrentStep, gMainWindow->GetAddKeys());
	FocusPiece->UpdatePosition(mCurrentStep);

	EndEditHistory();

	if ((FocusSection != SW_PIECE_SECTION_INVALID && FocusSection != SW_PIECE_SECTION_POSITION) || !TracksConnected)
		SetFocus(FocusPiece, SW_PIECE_SECTION_TRAIN_TRACK_CONNECTION_FIRST + NewConnectionIndex, swSelectionMode::Single);

	EndHistorySequence(tr("Rotate"));

	gMainWindow->UpdateSelectedObjects(true);
}

void swModel::UpdateTrainTrackConnections(swPiece* TrackPiece, bool IgnoreSelected) const
{
	if (!TrackPiece)
		return;

	const swTrainTrackInformation* TrainTrackInfo = TrackPiece->mPieceData->GetTrainTrackInfo();

	if (!TrainTrackInfo)
		return;

	const int ConnectionCount = static_cast<int>(TrainTrackInfo->GetConnections().size());
	std::vector<bool> Connections(ConnectionCount, false);

	for (const std::unique_ptr<swPiece>& Piece : mPieces)
	{
		if (Piece.get() == TrackPiece || !Piece->mPieceData->GetTrainTrackInfo() || (IgnoreSelected && Piece->IsSelected()))
			continue;

		for (int ConnectionIndex = 0; ConnectionIndex < ConnectionCount; ConnectionIndex++)
			if (!Connections[ConnectionIndex] && swTrainTrackInformation::GetPieceConnectionIndex(TrackPiece, ConnectionIndex, Piece.get()) != -1)
				Connections[ConnectionIndex] = true;
	}

	TrackPiece->SetTrainTrackConnections(std::move(Connections));
}

void swModel::UpdateSelectedPiecesTrainTrackConnections()
{
	std::vector<swPiece *> TrackPieces;

	for (const std::unique_ptr<swPiece>& Piece : mPieces)
		if (Piece->mPieceData->GetTrainTrackInfo() && Piece->IsSelected())
			TrackPieces.push_back(Piece.get());

	for (swPiece* TrackPiece : TrackPieces)
	{
		const swTrainTrackInformation* TrainTrackInfo = TrackPiece->mPieceData->GetTrainTrackInfo();
		const int ConnectionCount = static_cast<int>(TrainTrackInfo->GetConnections().size());
		std::vector<bool> Connections(ConnectionCount, false);

		for (const swPiece* CheckPiece : TrackPieces)
		{
			if (CheckPiece == TrackPiece)
				continue;

			for (int ConnectionIndex = 0; ConnectionIndex < ConnectionCount; ConnectionIndex++)
				if (!Connections[ConnectionIndex] && swTrainTrackInformation::GetPieceConnectionIndex(TrackPiece, ConnectionIndex, CheckPiece) != -1)
					Connections[ConnectionIndex] = true;
		}

		TrackPiece->SetTrainTrackConnections(std::move(Connections));
	}
}

void swModel::SetPreviewInsertPieceInfo(std::vector<swInsertPieceInformation>&& PreviewInsertPieceInfo)
{
	swPiecesLibrary* Library = swGetPiecesLibrary();
	std::vector<swPieceDataReference> RequiredPieceData;
	std::vector<swPreviewInsertPieceInformation> PreviewPieces;

	RequiredPieceData.reserve(PreviewInsertPieceInfo.size());
	PreviewPieces.reserve(PreviewInsertPieceInfo.size());

	for (const swInsertPieceInformation& InfoTransform : PreviewInsertPieceInfo)
	{
		PreviewPieces.emplace_back(InfoTransform);
		Library->RequestPieceDataLoad(InfoTransform.PieceData, swLoadPriority::Normal);
		RequiredPieceData.push_back(PreviewPieces.back().PieceData);
	}

	Library->WaitForPieceData(RequiredPieceData);
	mPreviewInsertPieceInfo = std::move(PreviewPieces);

	UpdateAllViews();
}

void swModel::DeleteSelectedObjects()
{
	if (mIsPreview)
	{
		RemoveSelectedObjects();

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

void swModel::ResetSelectedPiecesPivotPoint()
{
	BeginHistorySequence();
	BeginEditHistory();

	for (const std::unique_ptr<swPiece>& Piece : mPieces)
		if (Piece->IsSelected())
			Piece->ResetPivotPoint();

	EndEditHistory();
	EndHistorySequence(tr("Reset Pivot Point"));
}

void swModel::RemoveSelectedObjectsKeyFrames()
{
	BeginHistorySequence();
	BeginEditHistory();

	for (const std::unique_ptr<swPiece>& Piece : mPieces)
		if (Piece->IsSelected())
			Piece->RemoveKeyFrames();

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
	swPiece* Piece = dynamic_cast<swPiece*>(GetFocusObject());

	if (!Piece)
		return;

	swVector3 Start, End;

	gMainWindow->GetActiveView()->GetRayUnderPointer(Start, End);

	BeginHistorySequence();
	BeginEditHistory();

	bool Modified = Piece->InsertControlPoint(Start, End);

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
	swPiece* Piece = dynamic_cast<swPiece*>(GetFocusObject());

	if (!Piece)
		return;

	BeginHistorySequence();
	BeginEditHistory();

	bool Modified = Piece->RemoveFocusedControlPoint();

	if (!Modified)
	{
		DiscardHistorySequence();

		return;
	}

	EndEditHistory();
	EndHistorySequence(tr("Remove Control Point"));

	gMainWindow->UpdateSelectedObjects(true);
}

void swModel::ShowSelectedPiecesEarlier()
{
	BeginHistorySequence();
	BeginEditHistory();

	std::vector<swPiece*> MovedPieces;

	for (auto PieceIt = mPieces.begin(); PieceIt != mPieces.end(); )
	{
		swPiece* Piece = PieceIt->get();

		if (Piece->IsSelected())
		{
			swStep Step = Piece->GetStepShow();

			if (Step > 1)
			{
				Step--;
				Piece->SetStepShow(Step);

				MovedPieces.emplace_back(PieceIt->release());
				PieceIt = mPieces.erase(PieceIt);
				continue;
			}
		}

		PieceIt++;
	}

	if (MovedPieces.empty())
	{
		DiscardHistorySequence();

		return;
	}

	for (swPiece* Piece : MovedPieces)
	{
		Piece->SetFileLine(-1);
		AddPiece(Piece);
	}

	EndEditHistory();
	EndHistorySequence(tr("Show Earlier"));

	gMainWindow->UpdateTimeline(false, false);
	gMainWindow->UpdateSelectedObjects(true);
}

void swModel::ShowSelectedPiecesLater()
{
	BeginHistorySequence();
	BeginEditHistory();

	std::vector<swPiece*> MovedPieces;

	for (auto PieceIt = mPieces.begin(); PieceIt != mPieces.end(); )
	{
		swPiece* Piece = PieceIt->get();

		if (Piece->IsSelected())
		{
			swStep Step = Piece->GetStepShow();

			if (Step < SW_STEP_MAXIMUM)
			{
				Step++;
				Piece->SetStepShow(Step);

				MovedPieces.emplace_back(PieceIt->release());
				PieceIt = mPieces.erase(PieceIt);

				continue;
			}
		}

		PieceIt++;
	}

	if (MovedPieces.empty())
	{
		DiscardHistorySequence();

		return;
	}

	for (swPiece* Piece : MovedPieces)
	{
		Piece->SetFileLine(-1);
		AddPiece(Piece);
	}

	EndEditHistory();
	EndHistorySequence(tr("Show Later"));

	gMainWindow->UpdateTimeline(false, false);
	gMainWindow->UpdateSelectedObjects(false);
}

void swModel::InsertStepAndMoveSelectedPieces(swStep Step)
{
	BeginHistorySequence();
	BeginEditHistory();

	InsertStep(Step);

	std::vector<swPiece*> MovedPieces;

	for (auto PieceIt = mPieces.begin(); PieceIt != mPieces.end(); )
	{
		swPiece* Piece = PieceIt->get();

		if (Piece->IsSelected())
		{
			Piece->SetStepShow(Step);

			MovedPieces.emplace_back(PieceIt->release());
			PieceIt = mPieces.erase(PieceIt);

			continue;
		}

		PieceIt++;
	}

	for (swPiece* Piece : MovedPieces)
	{
		Piece->SetFileLine(-1);
		AddPiece(Piece);
	}

	EndEditHistory();
	EndHistorySequence(tr("Show Pieces"));

	gMainWindow->UpdateTimeline(false, false);
	gMainWindow->UpdateSelectedObjects(false);
}

void swModel::SetPieceSteps(const std::vector<std::pair<swPiece*, swStep>>& PieceSteps)
{
	if (PieceSteps.size() != mPieces.size())
		return;

	BeginHistorySequence();
	BeginEditHistory();

	bool Modified = false;

	for (size_t PieceIdx = 0; PieceIdx < PieceSteps.size(); PieceIdx++)
	{
		const std::pair<swPiece*, swStep>& PieceStep = PieceSteps[PieceIdx];
		swPiece* Piece = mPieces[PieceIdx].get();

		if (Piece != PieceStep.first || Piece->GetStepShow() != PieceStep.second)
		{
			Piece = PieceStep.first;
			const swStep Step = PieceStep.second;

			mPieces[PieceIdx].release();
			mPieces[PieceIdx] = std::unique_ptr<swPiece>(Piece);
			Piece->SetStepShow(Step);

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

void swModel::RenamePieces(const std::vector<swModelPieceRename>& Renames)
{
	if (Renames.empty())
		return;

	for (const std::unique_ptr<swPiece>& Piece : mPieces)
	{
		for (const swModelPieceRename& Rename : Renames)
		{
			if (Piece->mPieceData != Rename.PieceData)
				continue;

			Piece->UpdateID();
			break;
		}
	}

	auto RenameHistorySequence = [&Renames, ModelPieceData = mPieceData.Get()](std::vector<std::unique_ptr<swModelHistory>>& HistorySequence)
	{
		for (const std::unique_ptr<swModelHistory>& ModelHistory : HistorySequence)
			ModelHistory->RenamePieces(Renames, ModelPieceData);
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

	std::vector<swPiece*> Pieces;
	swPiece* ModelPiece = nullptr;
	swStep FirstStep = SW_STEP_MAXIMUM;
	swVector3 Min(FLT_MAX, FLT_MAX, FLT_MAX), Max(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	for (size_t PieceIndex = 0; PieceIndex < mPieces.size(); )
	{
		swPiece* Piece = mPieces[PieceIndex].get();

		if (Piece->IsSelected())
		{
			Piece->CompareBoundingBox(Min, Max);
			mPieces[PieceIndex].release();
			mPieces.erase(mPieces.begin() + PieceIndex);
			Piece->SetGroup(nullptr); // todo: copy groups
			Pieces.emplace_back(Piece);
			FirstStep = qMin(FirstStep, Piece->GetStepShow());

			if (!ModelPiece)
			{
				ModelPiece = new swPiece(Model->mPieceData);
				ModelPiece->SetColorIndex(gDefaultColor);
				AddPiece(std::unique_ptr<swPiece>(ModelPiece), PieceIndex);
				PieceIndex++;
			}
		}
		else
			PieceIndex++;
	}

	if (Pieces.empty())
	{
		DiscardHistorySequence();

		return;
	}

	swVector3 ModelCenter = (Min + Max) / 2.0f;
	ModelCenter.z += (Min.z - Max.z) / 2.0f;

	for (swPiece* Piece : Pieces)
	{
		Piece->SetFileLine(-1);
		Piece->SetStepShow(Piece->GetStepShow() - FirstStep + 1);
		Piece->MoveSelected(Piece->GetStepShow(), false, -ModelCenter);
		Model->AddPiece(Piece);
	}

	std::vector<swModel*> UpdatedModels;
	Model->UpdatePieceData(UpdatedModels);
	if (ModelPiece)
	{
		ModelPiece->Initialize(swMatrix44Translation(ModelCenter), FirstStep);
		ModelPiece->UpdatePosition(mCurrentStep);
	}

	EndEditHistory();

	gMainWindow->UpdateTimeline(false, false);

	SetSelectionAndFocus(std::vector<swObject*>(), ModelPiece, SW_PIECE_SECTION_POSITION, swSelectionMode::Single);

	EndHistorySequence(tr("Move to Model"));
}

void swModel::InlineSelectedModels()
{
	BeginHistorySequence();
	BeginEditHistory();

	std::vector<swObject*> NewPieces;
	bool Modified = false;

	for (size_t PieceIndex = 0; PieceIndex < mPieces.size(); )
	{
		swPiece* Piece = mPieces[PieceIndex].get();

		if (!Piece->IsSelected() || !Piece->mPieceData->IsModel())
		{
			PieceIndex++;
			continue;
		}

		mPieces[PieceIndex].release();
		mPieces.erase(mPieces.begin() + PieceIndex);

		swModel* Model = Piece->mPieceData->GetModel();

		Modified = true;

		for (const std::unique_ptr<swPiece>& ModelPiece : Model->mPieces)
		{
			swPiece* NewPiece = new swPiece(nullptr);

			// todo: recreate in groups in the current model

			int ColorIndex = ModelPiece->GetColorIndex();

			if (ColorIndex == gDefaultColor)
				ColorIndex = Piece->GetColorIndex();

			NewPiece->SetPieceData(ModelPiece->mPieceData, ModelPiece->GetID(), true, true);
			NewPiece->Initialize(swMultiply(ModelPiece->mModelWorld, Piece->mModelWorld), Piece->GetStepShow());
			NewPiece->SetColorIndex(ColorIndex);
			NewPiece->UpdatePosition(mCurrentStep);

			NewPieces.emplace_back(NewPiece);
			AddPiece(std::unique_ptr<swPiece>(NewPiece), PieceIndex);
			PieceIndex++;
		}

		delete Piece;
	}

	if (!Modified)
	{
		QMessageBox::information(gMainWindow, tr("Inline Selected Models"), tr("No models selected."));

		DiscardHistorySequence();

		return;
	}

	EndEditHistory();

	SetSelectionAndFocus(NewPieces, nullptr, 0, swSelectionMode::Single);

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
	bool RemovedPiece = false;
	bool RemovedCamera = false;
	bool RemovedLight = false;

	for (std::vector<std::unique_ptr<swPiece>>::iterator PieceIt = mPieces.begin(); PieceIt != mPieces.end(); )
	{
		swPiece* Piece = PieceIt->get();

		if (Piece->IsSelected())
		{
			RemovedPiece = true;
			PieceIt = mPieces.erase(PieceIt);
		}
		else
			PieceIt++;
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

	return RemovedPiece || RemovedCamera || RemovedLight;
}

void swModel::MoveSelectedObjects(const swVector3& PieceDistance, const swVector3& ObjectDistance, swModelTransformFlags Flags, swModelHistoryEditMerge ModelHistoryEditMerge)
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

	if (PieceDistance.LengthSquared() >= 0.001f)
	{
		swVector3 TransformedPieceDistance = swMultiply(PieceDistance, RelativeRotation);

		if (Flags.testFlag(swModelTransformFlag::RotatePivotPoint))
		{
			for (const std::unique_ptr<swPiece>& Piece : mPieces)
			{
				if (Piece->IsFocused())
				{
					Piece->MovePivotPoint(TransformedPieceDistance);
					Moved = true;
					break;
				}
			}
		}
		else
		{
			for (const std::unique_ptr<swPiece>& Piece : mPieces)
			{
				if (Piece->IsSelected())
				{
					if (gMainWindow->GetSeparateTransform())
						TransformedPieceDistance = swMultiply(PieceDistance, Piece->GetRelativeRotation());

					Piece->MoveSelected(mCurrentStep, gMainWindow->GetAddKeys(), TransformedPieceDistance);
					Piece->UpdatePosition(mCurrentStep);
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

		if (Focus && Focus->IsPiece())
		{
			((swPiece*)Focus)->RotatePivotPoint(RotationMatrix);
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
				if (Object->IsPiece())
				{
					swPiece* Piece = (swPiece*)Object;

					Piece->Rotate(mCurrentStep, gMainWindow->GetAddKeys(), RotationMatrix, Center, WorldToFocusMatrix);
					Piece->UpdatePosition(mCurrentStep);
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
				if (Object->IsPiece())
				{
					swPiece* Piece = (swPiece*)Object;

					const swVector3 Center = Piece->GetRotationCenter();
					swMatrix33 WorldToFocusMatrix;
					swMatrix33 RelativeRotationMatrix;

					if (Flags.testFlag(swModelTransformFlag::Relative))
					{
						const swMatrix33 RelativeRotation = Piece->GetRelativeRotation();
						WorldToFocusMatrix = swMatrix33AffineInverse(RelativeRotation);
						RelativeRotationMatrix = swMultiply(RotationMatrix, RelativeRotation);
					}
					else
					{
						WorldToFocusMatrix = swMatrix33Identity();
						RelativeRotationMatrix = RotationMatrix;
					}

					Piece->Rotate(mCurrentStep, gMainWindow->GetAddKeys(), RelativeRotationMatrix, Center, WorldToFocusMatrix);
					Piece->UpdatePosition(mCurrentStep);
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

void swModel::ScaleSelectedPieces(const float Scale)
{
	if (Scale < 0.001f)
		return;

	swObject* Focus = GetFocusObject();
	if (!Focus || !Focus->IsPiece())
		return;

	swPiece* Piece = (swPiece*)Focus;
	const quint32 Section = Piece->GetFocusSection();

	if (Section >= SW_PIECE_SECTION_CONTROL_POINT_FIRST && Section <= SW_PIECE_SECTION_CONTROL_POINT_LAST)
	{
		const int ControlPointIndex = Section - SW_PIECE_SECTION_CONTROL_POINT_FIRST;
		Piece->SetControlPointScale(ControlPointIndex, Scale);
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

void swModel::SetSelectedPiecesColorIndex(int ColorIndex)
{
	BeginHistorySequence();
	BeginEditHistory();

	bool Modified = false;

	for (const std::unique_ptr<swPiece>& Piece : mPieces)
	{
		if (Piece->IsSelected() && Piece->GetColorIndex() != ColorIndex)
		{
			Piece->SetColorIndex(ColorIndex);
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

void swModel::SetSelectedPiecesStepShow(swStep Step)
{
	std::vector<swPiece*> MovedPieces;

	BeginHistorySequence();
	BeginEditHistory();

	for (auto PieceIt = mPieces.begin(); PieceIt != mPieces.end(); )
	{
		swPiece* Piece = PieceIt->get();

		if (Piece->IsSelected() && Piece->GetStepShow() != Step)
		{
			Piece->SetStepShow(Step);

			MovedPieces.emplace_back(PieceIt->release());
			PieceIt = mPieces.erase(PieceIt);
			continue;
		}

		PieceIt++;
	}

	if (MovedPieces.empty())
	{
		DiscardHistorySequence();

		return;
	}

	for (swPiece* Piece : MovedPieces)
	{
		Piece->SetFileLine(-1);
		AddPiece(Piece);
	}

	EndEditHistory();
	EndHistorySequence(tr("Set Show Step"));

	gMainWindow->UpdateTimeline(false, false);
	gMainWindow->UpdateSelectedObjects(false);
}

void swModel::SetSelectedPiecesStepHide(swStep Step)
{
	bool Modified = false;

	BeginHistorySequence();
	BeginEditHistory();

	for (const std::unique_ptr<swPiece>& Piece : mPieces)
	{
		if (Piece->IsSelected() && Piece->GetStepHide() != Step)
		{
			Piece->SetStepHide(Step);

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
	if (PropertyId == swObjectPropertyId::PieceId || PropertyId == swObjectPropertyId::PieceColor)
	{
		gMainWindow->UpdateTimeline(false, true);
	}

	// todo: fix hacky category update
	if (PropertyId == swObjectPropertyId::PieceId)
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
