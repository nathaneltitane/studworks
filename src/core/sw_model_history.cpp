#include "sw_global.h"
#include "sw_model_history.h"
#include "sw_model.h"
#include "sw_piece.h"
#include "sw_camera.h"
#include "sw_light.h"
#include "sw_group.h"

void swModelHistorySelect::SaveStartState(const swModel* Model)
{
	SaveState(mStartState, Model);
}

void swModelHistorySelect::SaveEndState(const swModel* Model)
{
	SaveState(mEndState, Model);
}

void swModelHistorySelect::LoadStartState(swModel* Model) const
{
	LoadState(mStartState, Model);
}

void swModelHistorySelect::LoadEndState(swModel* Model) const
{
	LoadState(mEndState, Model);
}

bool swModelHistorySelect::StateChanged() const
{
	return mStartState != mEndState;
}

void swModelHistorySelect::SaveState(swModelHistorySelectState& State, const swModel* Model)
{
	State = swModelHistorySelectState();

	const std::vector<std::unique_ptr<swPiece>>& Pieces = Model->GetPieces();
	State.PieceSelection.resize(Pieces.size(), false);

	for (size_t PieceIndex = 0; PieceIndex < Pieces.size(); PieceIndex++)
	{
		swPiece* Piece = Pieces[PieceIndex].get();

		if (!Piece->IsSelected())
			continue;

		State.PieceSelection[PieceIndex] = true;

		if (Piece->IsFocused())
		{
			State.FocusIndex = PieceIndex;
			State.FocusSection = Piece->GetFocusSection();
			State.FocusObjectType = swObjectType::Piece;
		}
	}

	const std::vector<std::unique_ptr<swCamera>>& Cameras = Model->GetCameras();
	State.CameraSelection.resize(Cameras.size(), false);

	for (size_t CameraIndex = 0; CameraIndex < Cameras.size(); CameraIndex++)
	{
		swCamera* Camera = Cameras[CameraIndex].get();

		if (!Camera->IsSelected())
			continue;

		State.CameraSelection[CameraIndex] = true;

		if (Camera->IsFocused())
		{
			State.FocusIndex = CameraIndex;
			State.FocusSection = Camera->GetFocusSection();
			State.FocusObjectType = swObjectType::Camera;
		}
	}

	const std::vector<std::unique_ptr<swLight>>& Lights = Model->GetLights();
	State.LightSelection.resize(Lights.size(), false);

	for (size_t LightIndex = 0; LightIndex < Lights.size(); LightIndex++)
	{
		swLight* Light = Lights[LightIndex].get();

		if (!Light->IsSelected())
			continue;

		State.LightSelection[LightIndex] = true;

		if (Light->IsFocused())
		{
			State.FocusIndex = LightIndex;
			State.FocusSection = Light->GetFocusSection();
			State.FocusObjectType = swObjectType::Light;
		}
	}
}

void swModelHistorySelect::LoadState(const swModelHistorySelectState& State, swModel* Model)
{
	const std::vector<std::unique_ptr<swPiece>>& Pieces = Model->GetPieces();
	const std::vector<std::unique_ptr<swCamera>>& Cameras = Model->GetCameras();
	const std::vector<std::unique_ptr<swLight>>& Lights = Model->GetLights();

	if (Pieces.size() != State.PieceSelection.size() || Cameras.size() != State.CameraSelection.size() || Lights.size() != State.LightSelection.size())
		return;

	for (size_t PieceIndex = 0; PieceIndex < Pieces.size(); PieceIndex++)
	{
		swPiece* Piece = Pieces[PieceIndex].get();

		Piece->SetSelected(State.PieceSelection[PieceIndex]);

		if (State.FocusObjectType == swObjectType::Piece && State.FocusIndex == PieceIndex)
			Piece->SetFocused(State.FocusSection, true);
		else
			Piece->SetFocused(State.FocusSection, false);
	}

	for (size_t CameraIndex = 0; CameraIndex < Cameras.size(); CameraIndex++)
	{
		swCamera* Camera = Cameras[CameraIndex].get();

		Camera->SetSelected(State.CameraSelection[CameraIndex]);

		if (State.FocusObjectType == swObjectType::Camera && State.FocusIndex == CameraIndex)
			Camera->SetFocused(State.FocusSection, true);
		else
			Camera->SetFocused(State.FocusSection, false);
	}

	for (size_t LightIndex = 0; LightIndex < Lights.size(); LightIndex++)
	{
		swLight* Light = Lights[LightIndex].get();

		Light->SetSelected(State.LightSelection[LightIndex]);

		if (State.FocusObjectType == swObjectType::Light && State.FocusIndex == LightIndex)
			Light->SetFocused(State.FocusSection, true);
		else
			Light->SetFocused(State.FocusSection, false);
	}
}

swModelHistoryEdit::swModelHistoryEdit(swModelHistoryEditMergeKey MergeKey)
	: mMergeKey(MergeKey)
{
}

swModelHistoryEdit::~swModelHistoryEdit()
{
}

void swModelHistoryEdit::SaveState(swModelHistoryEditState& State, const swModel* Model)
{
	State.Groups.Save(Model->GetGroups());
	State.Pieces.Save(Model->GetPieces());
	State.Cameras.Save(Model->GetCameras());
	State.Lights.Save(Model->GetLights());
}

void swModelHistoryEdit::LoadState(const swModelHistoryEditState& State, swModel* Model)
{
	Model->LoadEditHistoryState(State);
}

void swModelHistoryEdit::SaveStartState(const swModel* Model)
{
	SaveState(mStartState, Model);
}

void swModelHistoryEdit::SaveEndState(const swModel* Model)
{
	SaveState(mEndState, Model);

	swObjectHistory<swGroupHistoryState>::Compact(mStartState.Groups, mEndState.Groups);
	swObjectHistory<swPieceHistoryState>::Compact(mStartState.Pieces, mEndState.Pieces);
	swObjectHistory<swCameraHistoryState>::Compact(mStartState.Cameras, mEndState.Cameras);
	swObjectHistory<swLightHistoryState>::Compact(mStartState.Lights, mEndState.Lights);
}

void swModelHistoryEdit::LoadStartState(swModel* Model) const
{
	LoadState(mStartState, Model);
}

void swModelHistoryEdit::LoadEndState(swModel* Model) const
{
	LoadState(mEndState, Model);
}

bool swModelHistoryEdit::StateChanged() const
{
	return !mStartState.IsEmpty() || !mEndState.IsEmpty();
}

void swModelHistoryEdit::RenamePieces(const std::vector<swModelPieceRename>& Renames, const swPieceData* ModelPieceData)
{
	Q_UNUSED(ModelPieceData);

	auto RenamePieceIds = [&Renames](swModelHistoryEditState& State)
	{
		for (swPieceHistoryState& Piece : State.Pieces.States)
		{
			for (const swModelPieceRename& Rename : Renames)
			{
				if (Piece.PieceId.compare(Rename.OldName, Qt::CaseInsensitive) != 0)
					continue;

				Piece.PieceId = Rename.NewName;
				break;
			}
		}
	};

	RenamePieceIds(mStartState);
	RenamePieceIds(mEndState);
}

bool swModelHistoryEdit::CanMergeWith(const swModelHistory* Other) const
{
	const swModelHistoryEdit* OtherModelHistoryEdit = dynamic_cast<const swModelHistoryEdit*>(Other);

	return OtherModelHistoryEdit && mMergeKey.Operation != swModelHistoryEditMerge::None && mMergeKey == OtherModelHistoryEdit->mMergeKey;
}

void swModelHistoryEdit::MergeWith(swModelHistory* Other)
{
	swModelHistoryEdit* OtherModelHistoryEdit = dynamic_cast<swModelHistoryEdit*>(Other);

	if (!OtherModelHistoryEdit)
		return;

	swObjectHistory<swGroupHistoryState>::Merge(mStartState.Groups, mEndState.Groups, OtherModelHistoryEdit->mStartState.Groups, OtherModelHistoryEdit->mEndState.Groups);
	swObjectHistory<swPieceHistoryState>::Merge(mStartState.Pieces, mEndState.Pieces, OtherModelHistoryEdit->mStartState.Pieces, OtherModelHistoryEdit->mEndState.Pieces);
	swObjectHistory<swCameraHistoryState>::Merge(mStartState.Cameras, mEndState.Cameras, OtherModelHistoryEdit->mStartState.Cameras, OtherModelHistoryEdit->mEndState.Cameras);
	swObjectHistory<swLightHistoryState>::Merge(mStartState.Lights, mEndState.Lights, OtherModelHistoryEdit->mStartState.Lights, OtherModelHistoryEdit->mEndState.Lights);
}

void swModelHistoryProperties::SaveStartState(const swModel* Model)
{
	SaveState(mStartState, Model);
}

void swModelHistoryProperties::SaveEndState(const swModel* Model)
{
	SaveState(mEndState, Model);
}

void swModelHistoryProperties::LoadStartState(swModel* Model) const
{
	LoadState(mStartState, Model);
}

void swModelHistoryProperties::LoadEndState(swModel* Model) const
{
	LoadState(mEndState, Model);
}

bool swModelHistoryProperties::StateChanged() const
{
	return mStartState != mEndState;
}

void swModelHistoryProperties::RenamePieces(const std::vector<swModelPieceRename>& Renames, const swPieceData* ModelPieceData)
{
	auto RenameModel = [&Renames, ModelPieceData](swModelProperties& Properties)
	{
		for (const swModelPieceRename& Rename : Renames)
		{
			if (Rename.PieceData != ModelPieceData || Properties.mFileName.compare(Rename.OldName, Qt::CaseInsensitive) != 0)
				continue;

			if (Properties.mModelName == Properties.mFileName)
				Properties.mModelName = Rename.NewName;

			Properties.mFileName = Rename.NewName;
			break;
		}
	};

	RenameModel(mStartState);
	RenameModel(mEndState);
}

void swModelHistoryProperties::SaveState(swModelProperties& State, const swModel* Model)
{
	State = Model->GetProperties();
}

void swModelHistoryProperties::LoadState(const swModelProperties& State, swModel* Model)
{
	Model->LoadModelPropertiesState(State);
}
