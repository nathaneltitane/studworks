#include "sw_global.h"
#include "sw_model_history.h"
#include "sw_model.h"
#include "sw_object_piece.h"
#include "sw_object_camera.h"
#include "sw_object_light.h"
#include "sw_group.h"
#include "sw_project.h"
#include "sw_window_main.h"
#include "sw_piece_data.h"
#include "sw_view.h"
#include "sw_widget_preview.h"

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

void swModel::LoadEditHistoryState(const swModelHistoryEditState& HistoryState)
{
	const std::unordered_map<uint32_t, swGroup*> GroupsById = RestoreGroups(HistoryState.Groups);
	RestorePieces(HistoryState.Pieces, GroupsById);
	RestoreCameras(HistoryState.Cameras);
	RestoreLights(HistoryState.Lights);
}

std::unordered_map<uint32_t, swGroup*> swModel::RestoreGroups(const swObjectHistory<swGroupHistoryState>& History)
{
	const std::unordered_map<uint32_t, swGroup*> GroupsById = History.RestoreObjects(mGroups);

	// The lookup uses saved IDs, so a parent can be linked before its state is restored.
	for (const swGroupHistoryState& State : History.States)
	{
		const auto Group = GroupsById.find(static_cast<uint32_t>(State.Id));
		const auto Parent = GroupsById.find(State.ParentId);

		if (Group != GroupsById.end())
			Group->second->SetHistoryState(State, Parent != GroupsById.end() ? Parent->second : nullptr);
	}

	return GroupsById;
}

void swModel::RestorePieces(const swObjectHistory<swPieceHistoryState>& History, const std::unordered_map<uint32_t, swGroup*>& GroupsById)
{
	const std::unordered_map<uint32_t, swPiece*> PiecesById = History.RestoreObjects(mPieces);

	for (const swPieceHistoryState& State : History.States)
	{
		const auto Piece = PiecesById.find(static_cast<uint32_t>(State.Id));
		const auto Group = GroupsById.find(State.GroupId);

		if (Piece != PiecesById.end())
			Piece->second->SetHistoryState(State, mProject, Group != GroupsById.end() ? Group->second : nullptr);
	}

	// Model-list changes are not undoable, but older edit states can still name
	// a submodel that was replaced. Do not restore references that now recurse.
	mPieces.erase(std::remove_if(mPieces.begin(), mPieces.end(), [this](const std::unique_ptr<swPiece>& Piece)
	{
		return Piece->mPieceData && Piece->mPieceData->IncludesModel(this);
	}), mPieces.end());
}

void swModel::RestoreCameras(const swObjectHistory<swCameraHistoryState>& History)
{
	if (History.IsEmpty())
		return;

	if (History.Order)
	{
		const std::unordered_set<uint32_t> CameraIds(History.Order->begin(), History.Order->end());

		for (const std::unique_ptr<swCamera>& Camera : mCameras)
			if (CameraIds.find(static_cast<uint32_t>(Camera->GetId())) == CameraIds.end())
				RemoveCameraFromViews(Camera.get());
	}

	const std::unordered_map<uint32_t, swCamera*> CamerasById = History.RestoreObjects(mCameras);

	for (const swCameraHistoryState& State : History.States)
	{
		const auto Camera = CamerasById.find(static_cast<uint32_t>(State.Id));

		if (Camera != CamerasById.end())
			Camera->second->SetHistoryState(State);
	}
}

void swModel::RestoreLights(const swObjectHistory<swLightHistoryState>& History)
{
	if (History.IsEmpty())
		return;

	const std::unordered_map<uint32_t, swLight*> LightsById = History.RestoreObjects(mLights);

	for (const swLightHistoryState& State : History.States)
	{
		const auto Light = LightsById.find(static_cast<uint32_t>(State.Id));

		if (Light != LightsById.end())
			Light->second->SetHistoryState(State);
	}
}

void swModel::BeginEditHistory()
{
	BeginMergeableEditHistory(swModelHistoryEditMerge::None, swObjectPropertyId::Count);
}

void swModel::BeginMergeableEditHistory(swModelHistoryEditMerge ModelHistoryEditMerge, swObjectPropertyId PropertyId)
{
	const swModelHistoryEditMergeKey MergeKey = { ModelHistoryEditMerge, PropertyId, mCurrentStep, gMainWindow ? gMainWindow->GetAddKeys() : false };
	std::unique_ptr<swModelHistoryEdit> ModelHistoryEdit = std::make_unique<swModelHistoryEdit>(MergeKey);

	ModelHistoryEdit->SaveStartState(this);

	mHistorySequence.emplace_back(std::move(ModelHistoryEdit));
}

void swModel::EndEditHistory()
{
	if (mHistorySequence.empty())
		return;

	swModelHistoryEdit* ModelHistoryEdit = dynamic_cast<swModelHistoryEdit*>(mHistorySequence.back().get());

	if (!ModelHistoryEdit)
		return;

	ModelHistoryEdit->SaveEndState(this);

	if (!ModelHistoryEdit->StateChanged())
		mHistorySequence.pop_back();
}

void swModel::LoadModelPropertiesState(const swModelProperties& ModelProperties)
{
	const bool StepViewsChanged = mProperties.mStepViews != ModelProperties.mStepViews;
	mProperties = ModelProperties;

	// Undo and redo of step views update the Timeline and turn the view again.
	if (StepViewsChanged && gMainWindow && !mIsPreview)
	{
		gMainWindow->UpdateTimeline(false, false);

		if (swGetPreferences().mFollowStepViews)
			for (swView* View : swView::GetModelViews(this))
				View->ApplyStepView();
	}

	if (gMainWindow)
		gMainWindow->GetPreviewWidget()->UpdatePreview();
}

void swModel::SetModelProperties(const swModelProperties& ModelProperties)
{
	std::unique_ptr<swModelHistoryProperties> ModelHistoryProperties = std::make_unique<swModelHistoryProperties>();
	ModelHistoryProperties->SaveStartState(this);

	LoadModelPropertiesState(ModelProperties);

	ModelHistoryProperties->SaveEndState(this);

	if (ModelHistoryProperties->StateChanged())
		mHistorySequence.emplace_back(std::move(ModelHistoryProperties));
}

void swModel::RunHistorySequence(const std::vector<std::unique_ptr<swModelHistory>>& HistorySequence, bool Apply)
{
	bool SelectionChanged = false;
	bool EditChanged = false;

	auto RunAction=[this, &SelectionChanged, &EditChanged](const swModelHistory* ModelHistory, bool Apply)
	{
		if (!ModelHistory)
			return;

		if (Apply)
			ModelHistory->LoadEndState(this);
		else
			ModelHistory->LoadStartState(this);

		if (dynamic_cast<const swModelHistorySelect*>(ModelHistory))
		{
			SelectionChanged = true;
		}
		else if (dynamic_cast<const swModelHistoryEdit*>(ModelHistory))
		{
			EditChanged = true;
			SetCurrentStep(mCurrentStep);
		}
		else if (dynamic_cast<const swModelHistoryProperties*>(ModelHistory))
		{
		}
	};

	if (Apply)
	{
		for (auto ModelHistory = HistorySequence.begin(); ModelHistory != HistorySequence.end(); ++ModelHistory)
			RunAction(ModelHistory->get(), true);
	}
	else
	{
		for (auto ModelHistory = HistorySequence.rbegin(); ModelHistory != HistorySequence.rend(); ++ModelHistory)
			RunAction(ModelHistory->get(), false);
	}

	if (EditChanged)
		gMainWindow->UpdateInUseCategory();

	if (SelectionChanged)
		gMainWindow->UpdateSelectedObjects(true);

	gMainWindow->UpdateCurrentStep();
	gMainWindow->UpdateTimeline(true, false);

	UpdateAllViews();
	if (EditChanged)
		RefreshPreview();
}

void swModel::BeginHistorySequence()
{
	mHistorySequence.clear();
}

void swModel::EndHistorySequence(const QString& Description)
{
	if (mHistorySequence.empty())
		return;

	if (mIsPreview)
	{
		mHistorySequence.clear();

		return;
	}

	bool EditChanged = false;

	for (const std::unique_ptr<swModelHistory>& ModelHistory : mHistorySequence)
	{
		if (dynamic_cast<const swModelHistoryEdit*>(ModelHistory.get()))
		{
			EditChanged = true;
			break;
		}
	}

	bool CanMerge = false;

	if (mHistorySequence.size() == 1 && !mUndoHistory.empty() && mUndoHistory.front()->Revision != mSavedHistoryRevision && mUndoHistory.front()->HistorySequence.size() == 1)
		CanMerge = mHistorySequence.front()->CanMergeWith(mUndoHistory.front()->HistorySequence.front().get());

	if (!CanMerge)
	{
		std::unique_ptr<swModelHistoryEntry> ModelHistoryEntry = std::make_unique<swModelHistoryEntry>(swModelHistoryEntry());

		ModelHistoryEntry->Description = Description;
		ModelHistoryEntry->Revision = mNextHistoryRevision++;
		ModelHistoryEntry->HistorySequence = std::move(mHistorySequence);

		mUndoHistory.insert(mUndoHistory.begin(), std::move(ModelHistoryEntry));
	}
	else
	{
		swModelHistoryEdit* ModelHistoryEdit = dynamic_cast<swModelHistoryEdit*>(mHistorySequence.front().get());
		swModelHistoryEntry* LastHistoryEntry = mUndoHistory.front().get();
		swModelHistoryEdit* LastModelHistoryEdit = dynamic_cast<swModelHistoryEdit*>(LastHistoryEntry->HistorySequence.front().get());

		LastModelHistoryEdit->MergeWith(ModelHistoryEdit);

		mHistorySequence.clear();
	}

	mRedoHistory.clear();

	gMainWindow->UpdateModified(mProject ? mProject->IsModified() : IsModified());
	gMainWindow->UpdateUndoRedo(!mUndoHistory.empty() ? mUndoHistory.front()->Description : nullptr, !mRedoHistory.empty() ? mRedoHistory.front()->Description : nullptr);
	gMainWindow->UpdateTimeline(true, false);

	for (const std::unique_ptr<swModelHistory>& ModelHistory : mUndoHistory.front()->HistorySequence)
	{
		if (dynamic_cast<const swModelHistorySelect*>(ModelHistory.get()))
		{
			gMainWindow->UpdateSelectedObjects(true);
			break;
		}
	}

	UpdateAllViews();
	if (EditChanged)
		RefreshPreview();
}

void swModel::DiscardHistorySequence()
{
	mHistorySequence.clear();
}

void swModel::RevertHistorySequence()
{
	RunHistorySequence(mHistorySequence, false);

	mHistorySequence.clear();
}

bool swModel::IsModified() const
{
	return GetCurrentHistoryRevision() != mSavedHistoryRevision;
}

void swModel::SetSaved()
{
	mSavedHistoryRevision = GetCurrentHistoryRevision();
}

void swModel::RemoveFirstUndoIfUnchanged()
{
	if (mUndoHistory.empty())
		return;

	for (const std::unique_ptr<swModelHistory>& ModelHistory : mUndoHistory.front()->HistorySequence)
		if (ModelHistory->StateChanged())
			return;

	mUndoHistory.erase(mUndoHistory.begin());

	gMainWindow->UpdateModified(mProject ? mProject->IsModified() : IsModified());
	gMainWindow->UpdateUndoRedo(!mUndoHistory.empty() ? mUndoHistory.front()->Description : nullptr, !mRedoHistory.empty() ? mRedoHistory.front()->Description : nullptr);
}

quint64 swModel::GetCurrentHistoryRevision() const
{
	for (const std::unique_ptr<swModelHistoryEntry>& UndoEntry : mUndoHistory)
		if (UndoEntry->HistorySequence.size() != 1 || !dynamic_cast<swModelHistorySelect*>(UndoEntry->HistorySequence.front().get()))
			return UndoEntry->Revision;

	return 0;
}
