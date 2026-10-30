#include "sw_global.h"
#include "sw_model_history.h"
#include "sw_model.h"
#include "sw_object_part.h"
#include "sw_object_camera.h"
#include "sw_object_light.h"
#include "sw_group.h"
#include "sw_project.h"
#include "sw_window_main.h"
#include "sw_data_part.h"
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

	const std::vector<std::unique_ptr<swPart>>& Parts = Model->GetParts();
	State.PartSelection.resize(Parts.size(), false);
	State.PartSelectedSection.resize(Parts.size(), SW_OBJECT_SECTION_INVALID);

	for (size_t PartIndex = 0; PartIndex < Parts.size(); PartIndex++)
	{
		swPart* Part = Parts[PartIndex].get();

		if (!Part->IsSelected())
			continue;

		State.PartSelection[PartIndex] = true;
		State.PartSelectedSection[PartIndex] = Part->GetSelectedSection();

		if (Part->IsFocused())
		{
			State.FocusIndex = PartIndex;
			State.FocusSection = Part->GetFocusSection();
			State.FocusObjectType = swObjectType::Part;
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
	const std::vector<std::unique_ptr<swPart>>& Parts = Model->GetParts();
	const std::vector<std::unique_ptr<swCamera>>& Cameras = Model->GetCameras();
	const std::vector<std::unique_ptr<swLight>>& Lights = Model->GetLights();

	if (Parts.size() != State.PartSelection.size() || Parts.size() != State.PartSelectedSection.size() || Cameras.size() != State.CameraSelection.size() || Lights.size() != State.LightSelection.size())
		return;

	for (size_t PartIndex = 0; PartIndex < Parts.size(); PartIndex++)
	{
		swPart* Part = Parts[PartIndex].get();

		Part->SetSelected(State.PartSelection[PartIndex]);

		if (State.FocusObjectType == swObjectType::Part && State.FocusIndex == PartIndex)
			Part->SetFocused(State.FocusSection, true);
		else
			Part->SetFocused(State.FocusSection, false);

		Part->SetSelectedSection(State.PartSelectedSection[PartIndex]);
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
	State.Parts.Save(Model->GetParts());
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
	swObjectHistory<swPartHistoryState>::Compact(mStartState.Parts, mEndState.Parts);
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

void swModelHistoryEdit::RenameParts(const std::vector<swModelPartRename>& Renames, const swPartData* ModelPartData)
{
	Q_UNUSED(ModelPartData);

	auto RenamePartIds = [&Renames](swModelHistoryEditState& State)
	{
		for (swPartHistoryState& Part : State.Parts.States)
		{
			for (const swModelPartRename& Rename : Renames)
			{
				if (Part.PartId.compare(Rename.OldName, Qt::CaseInsensitive) != 0)
					continue;

				Part.PartId = Rename.NewName;
				break;
			}
		}
	};

	RenamePartIds(mStartState);
	RenamePartIds(mEndState);
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
	swObjectHistory<swPartHistoryState>::Merge(mStartState.Parts, mEndState.Parts, OtherModelHistoryEdit->mStartState.Parts, OtherModelHistoryEdit->mEndState.Parts);
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

void swModelHistoryProperties::RenameParts(const std::vector<swModelPartRename>& Renames, const swPartData* ModelPartData)
{
	auto RenameModel = [&Renames, ModelPartData](swModelProperties& Properties)
	{
		for (const swModelPartRename& Rename : Renames)
		{
			if (Rename.PartData != ModelPartData || Properties.mFileName.compare(Rename.OldName, Qt::CaseInsensitive) != 0)
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

swModelHistorySubModel::swModelHistorySubModel(const QString& SubModelName, std::unique_ptr<swModelHistoryEdit> SubModelEdit)
	: mSubModelName(SubModelName), mSubModelEdit(std::move(SubModelEdit))
{
}

void swModelHistorySubModel::SaveStartState(const swModel* Model)
{
	Q_UNUSED(Model);
}

void swModelHistorySubModel::SaveEndState(const swModel* Model)
{
	Q_UNUSED(Model);
}

swModel* swModelHistorySubModel::FindSubModel(const swModel* Model) const
{
	// skip deleted submodels
	Project* const ModelProject = Model->GetProject();

	return ModelProject ? ModelProject->GetModel(mSubModelName) : nullptr;
}

void swModelHistorySubModel::UpdateProjectModels(swModel* SubModel)
{
	SubModel->CalculateStep(SubModel->GetCurrentStep());

	std::vector<swModel*> UpdatedModels;

	for (const std::unique_ptr<swModel>& ProjectModel : SubModel->GetProject()->GetModels())
		ProjectModel->UpdatePartData(UpdatedModels);
}

void swModelHistorySubModel::LoadStartState(swModel* Model) const
{
	swModel* const SubModel = FindSubModel(Model);

	if (!SubModel)
		return;

	mSubModelEdit->LoadStartState(SubModel);
	UpdateProjectModels(SubModel);
}

void swModelHistorySubModel::LoadEndState(swModel* Model) const
{
	swModel* const SubModel = FindSubModel(Model);

	if (!SubModel)
		return;

	mSubModelEdit->LoadEndState(SubModel);
	UpdateProjectModels(SubModel);
}

bool swModelHistorySubModel::StateChanged() const
{
	return mSubModelEdit->StateChanged();
}

void swModelHistorySubModel::RenameParts(const std::vector<swModelPartRename>& Renames, const swPartData* ModelPartData)
{
	mSubModelEdit->RenameParts(Renames, ModelPartData);

	for (const swModelPartRename& Rename : Renames)
	{
		if (mSubModelName.compare(Rename.OldName, Qt::CaseInsensitive) != 0)
			continue;

		mSubModelName = Rename.NewName;
		break;
	}
}

void swModel::LoadEditHistoryState(const swModelHistoryEditState& HistoryState)
{
	const std::unordered_map<uint32_t, swGroup*> GroupsById = RestoreGroups(HistoryState.Groups);
	RestoreParts(HistoryState.Parts, GroupsById);
	RestoreCameras(HistoryState.Cameras);
	RestoreLights(HistoryState.Lights);
	UpdateControlPointSeats();
}

std::unordered_map<uint32_t, swGroup*> swModel::RestoreGroups(const swObjectHistory<swGroupHistoryState>& History)
{
	const std::unordered_map<uint32_t, swGroup*> GroupsById = History.RestoreObjects(mGroups);

	// link parents by saved ids
	for (const swGroupHistoryState& State : History.States)
	{
		const auto Group = GroupsById.find(static_cast<uint32_t>(State.Id));
		const auto Parent = GroupsById.find(State.ParentId);

		if (Group != GroupsById.end())
			Group->second->SetHistoryState(State, Parent != GroupsById.end() ? Parent->second : nullptr);
	}

	return GroupsById;
}

void swModel::RestoreParts(const swObjectHistory<swPartHistoryState>& History, const std::unordered_map<uint32_t, swGroup*>& GroupsById)
{
	const std::unordered_map<uint32_t, swPart*> PartsById = History.RestoreObjects(mParts);

	for (const swPartHistoryState& State : History.States)
	{
		const auto Part = PartsById.find(static_cast<uint32_t>(State.Id));
		const auto Group = GroupsById.find(State.GroupId);

		if (Part != PartsById.end())
			Part->second->SetHistoryState(State, mProject, Group != GroupsById.end() ? Group->second : nullptr);
	}

	// never restore recursive references
	mParts.erase(std::remove_if(mParts.begin(), mParts.end(), [this](const std::unique_ptr<swPart>& Part)
	{
		return Part->mPartData && Part->mPartData->IncludesModel(this);
	}), mParts.end());
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
	UpdateControlPointSeats();

	if (!ModelHistoryEdit->StateChanged())
		mHistorySequence.pop_back();
}

void swModel::LoadModelPropertiesState(const swModelProperties& ModelProperties)
{
	const bool StepViewsChanged = mProperties.mStepViews != ModelProperties.mStepViews;
	mProperties = ModelProperties;

	// update the timeline and view
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
		else if (dynamic_cast<const swModelHistoryEdit*>(ModelHistory) || dynamic_cast<const swModelHistorySubModel*>(ModelHistory))
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
		if (dynamic_cast<const swModelHistoryEdit*>(ModelHistory.get()) || dynamic_cast<const swModelHistorySubModel*>(ModelHistory.get()))
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
