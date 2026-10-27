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

swModel::swModel(const QString& FileName, Project* Project, bool Preview)
	: mProject(Project), mIsPreview(Preview)
{
	mProperties.mModelName = FileName;
	mProperties.mFileName = FileName;
	mProperties.LoadDefaults();

	mActive = false;
	mCurrentStep = 1;
}

void swModel::SetProject(Project* Project)
{
	if (mProject && mProject != Project)
	{
		const QString SourceFileName = mProject->GetFileName();
		const QString SourceDirectory = SourceFileName.isEmpty() ? QString() : QFileInfo(SourceFileName).absolutePath();

		PreserveAssetSourceDirectory(SourceDirectory, false);
	}

	mProject = Project;
}

void swModel::PreserveAssetSourceDirectory(const QString& Directory, bool PreferCurrentDirectory)
{
	if (!Directory.isEmpty())
	{
		const QString CleanDirectory = QDir::cleanPath(Directory);
		const std::vector<QString>::iterator NewEnd = std::remove(mAssetSourceDirectories.begin(), mAssetSourceDirectories.end(), CleanDirectory);

		mAssetSourceDirectories.erase(NewEnd, mAssetSourceDirectories.end());

		if (mPreferCurrentAssetDirectory)
			mAssetSourceDirectories.insert(mAssetSourceDirectories.begin(), CleanDirectory);
		else
			mAssetSourceDirectories.push_back(CleanDirectory);
	}

	mPreferCurrentAssetDirectory = PreferCurrentDirectory;
}

swModelAssetSourceState swModel::GetAssetSourceState() const
{
	return { mAssetSourceDirectories, mPreferCurrentAssetDirectory };
}

void swModel::RestoreAssetSourceState(const swModelAssetSourceState& State)
{
	mAssetSourceDirectories = State.Directories;
	mPreferCurrentAssetDirectory = State.PreferCurrentDirectory;
}

std::vector<QString> swModel::GetAssetSearchDirectories() const
{
	std::vector<QString> Directories;
	QString CurrentDirectory;

	if (mProject && !mProject->GetFileName().isEmpty())
		CurrentDirectory = QFileInfo(mProject->GetFileName()).absolutePath();

	if (mPreferCurrentAssetDirectory && !CurrentDirectory.isEmpty())
		Directories.push_back(CurrentDirectory);

	for (const QString& Directory : mAssetSourceDirectories)
	{
		if (mPreferCurrentAssetDirectory && Directory == CurrentDirectory)
			continue;

		Directories.push_back(Directory);
	}

	if (!mPreferCurrentAssetDirectory && !CurrentDirectory.isEmpty() &&
		std::find(mAssetSourceDirectories.begin(), mAssetSourceDirectories.end(), CurrentDirectory) == mAssetSourceDirectories.end())
		Directories.push_back(CurrentDirectory);

	return Directories;
}

swModel::~swModel()
{
	mPreviewInsertPartInfo.clear();

	if (mPartData)
	{
		if (mPartData->GetModel() == this)
		{
			if (!mIsPreview && gMainWindow && gMainWindow->GetInsertPartData() == mPartData)
				gMainWindow->SetInsertPartData(nullptr);

			// Keep the name indexed while placements still reference the deleted model.
			swGetPartsLibrary()->InvalidatePartDataLoad(mPartData.Get());
			mPartData->DetachContainer();
			mPartData->SetFailed(tr("Model '%1' was deleted.").arg(QString::fromLatin1(mPartData->mFileName)));
			swGetPartsLibrary()->NotifyAssetStatusChanged();
		}

		mPartData.Reset();
	}

	DeleteModel();
}

bool swModel::GetPartWorldMatrix(swPart* Part, swMatrix44& ParentWorldMatrix) const
{
	for (const std::unique_ptr<swPart>& ModelPart : mParts)
	{
		if (ModelPart.get() == Part)
		{
			ParentWorldMatrix = swMultiply(ModelPart->mModelWorld, ParentWorldMatrix);
			return true;
		}

		const swPartData* PartData = ModelPart->mPartData.Get();

		if (PartData->IsModel())
		{
			swMatrix44 WorldMatrix = swMultiply(ModelPart->mModelWorld, ParentWorldMatrix);

			if (PartData->GetPartWorldMatrix(Part, WorldMatrix))
			{
				ParentWorldMatrix = WorldMatrix;
				return true;
			}
		}
	}

	return false;
}

bool swModel::IncludesModel(const swModel* Model) const
{
	std::unordered_set<const swModel*> Visited;
	const std::function<bool(const swModel*)> Visit = [Model, &Visited, &Visit](const swModel* Current)
	{
		if (Current == Model)
			return true;

		if (!Current || !Visited.insert(Current).second)
			return false;

		for (const std::unique_ptr<swPart>& Part : Current->mParts)
		{
			const swPartData* PartData = Part->mPartData.Get();

			if (PartData && PartData->IsModel() && Visit(PartData->GetModel()))
				return true;
		}

		return false;
	};

	return Visit(this);
}

void swModel::DeleteModel()
{
	if (gMainWindow)
	{
		std::vector<swView*> Views = swView::GetModelViews(this);

		// TODO: this is only needed to avoid a dangling pointer during undo/redo if a camera is set to a view but we should find a better solution instead
		for (swView* View : Views)
		{
			swCamera* ViewCamera = View->GetCamera();

			if (!ViewCamera->IsSimple())
			{
				for (const std::unique_ptr<swCamera>& Camera : mCameras)
				{
					if (Camera.get() == ViewCamera)
					{
						View->SetCamera(ViewCamera, true);
						break;
					}
				}
			}
		}
	}

	mParts.clear();
	mCameras.clear();
	mLights.clear();
	mGroups.clear();
	mFileLines.clear();
	mHasDirectGeometry = false;
}

void swModel::CreatePartData(Project* Project)
{
	swPartsLibrary* Library = swGetPartsLibrary();
	bool Reused;
	mPartData = Library->AcquireModelPartData(mProperties.mFileName.toLatin1().constData(), Project, Reused);
	mPartData->SetModel(this);

	if (Reused)
	{
		// Reusing a referenced name can turn an existing placement into a cycle.
		mParts.erase(std::remove_if(mParts.begin(), mParts.end(), [this](const std::unique_ptr<swPart>& Part)
		{
			return Part->mPartData && Part->mPartData->IncludesModel(this);
		}), mParts.end());
	}

	// The model owns the reference acquired by AcquireModelPartData(), including while
	// mesh requests are deferred until its source lines have been parsed.
	if (!Project->DefersModelMeshRequests())
	{
		if (mIsPreview)
			Library->RequestPartDataLoad(mPartData.Get(), swLoadPriority::Normal);
		else
			Library->WaitForPartData(mPartData);
	}

	if (Reused)
	{
		UpdateAllViews();

		if (!Project->DefersModelMeshRequests())
			RefreshPreview();
	}
}

bool swModel::RebindPartData(swPartData* Previous, swPartData* Replacement)
{
	bool Changed = false;

	mParts.erase(std::remove_if(mParts.begin(), mParts.end(), [this, Previous, Replacement, &Changed](const std::unique_ptr<swPart>& Part)
	{
		if (Part->mPartData != Previous)
			return false;

		Changed = true;

		if (Replacement->IncludesModel(this))
			return true;

		const QString PartId = Part->GetID();
		Part->SetPartData(swPartDataReference(Replacement), PartId, false, false);

		return false;
	}), mParts.end());

	return Changed;
}

void swModel::QueueMeshBuild()
{
	mPartData->SetModel(this);
	swGetPartsLibrary()->RebuildModelPartData(mPartData.Get());
}

void swModel::UpdateAllViews() const
{
	swView::UpdateProjectViews(mProject);
}

void swModel::RefreshPreview() const
{
	if (mIsPreview || !mProject)
		return;

	if (gMainWindow && gMainWindow->GetPreviewWidget())
		gMainWindow->GetPreviewWidget()->RefreshModel(this);
}

std::vector<swPartDataReference> swModel::GetRequiredPartData() const
{
	std::vector<swPartDataReference> RequiredPartData;
	std::unordered_set<const swModel*> Visited;

	const std::function<void(const swModel*)> Collect = [&RequiredPartData, &Visited, &Collect](const swModel* Model)
	{
		if (!Model || !Visited.insert(Model).second)
			return;

		if (Model->mPartData)
			RequiredPartData.push_back(Model->mPartData);

		for (const std::unique_ptr<swPart>& Part : Model->mParts)
		{
			const swPartDataReference& PartData = Part->mPartData;

			if (PartData->IsModel())
				Collect(PartData->GetModel());
			else if (PartData->IsProject())
				Collect(PartData->GetProject()->GetMainModel());
			else
				RequiredPartData.push_back(PartData);
		}
	};

	Collect(this);

	std::sort(RequiredPartData.begin(), RequiredPartData.end());
	RequiredPartData.erase(std::unique(RequiredPartData.begin(), RequiredPartData.end()), RequiredPartData.end());

	return RequiredPartData;
}

std::vector<swPart*> swModel::GetRequiredSynthParts() const
{
	std::vector<swPart*> Required;
	std::unordered_set<const swModel*> Visited;

	const std::function<void(const swModel*)> Collect = [&Required, &Visited, &Collect](const swModel* Model)
	{
		if (!Model || !Visited.insert(Model).second)
			return;

		for (const std::unique_ptr<swPart>& Part : Model->mParts)
		{
			swPartData* PartData = Part->mPartData.Get();

			if (PartData->IsModel())
				Collect(PartData->GetModel());
			else if (PartData->IsProject())
				Collect(PartData->GetProject()->GetMainModel());
			else if (PartData->GetSynthInfo())
				Required.push_back(Part.get());
		}
	};

	Collect(this);

	return Required;
}

void swModel::WaitForAssets() const
{
	const std::vector<swPartDataReference> RequiredPartData = GetRequiredPartData();
	swPartsLibrary* Library = swGetPartsLibrary();

	swBlockingProgress Progress(tr("Loading"), tr("Loading parts..."), false);

	Library->WaitForPartData(RequiredPartData, [&Progress](int Finished, int Total)
	{
		Progress.SetProgress(Finished, Total);
	});

	Library->WaitForSynthMeshes(GetRequiredSynthParts());

	// Temporary minifigure and thumbnail models have no container part to update.
	if (mPartData)
	{
		std::vector<swModel*> UpdatedModels;
		const_cast<swModel*>(this)->UpdatePartData(UpdatedModels);
	}
}

swResult<void> swModel::EnsureAssetsReady() const
{
	WaitForAssets();

	const std::vector<swPartDataReference> RequiredPartData = GetRequiredPartData();
	swPartsLibrary* Library = swGetPartsLibrary();

	for (const swPartDataReference& RequiredData : RequiredPartData)
	{
		if (RequiredData->mState == swAssetState::Loaded)
			continue;

		const QString Error = Library->GetPartDataLoadError(RequiredData.Get());

		if (!Error.isEmpty())
			return swUnexpected(Error);

		return swUnexpected(tr("Could not load all required parts."));
	}

	const std::vector<swPart*> GeneratedParts = GetRequiredSynthParts();

	for (const swPart* Part : GeneratedParts)
		if (!Part->HasGeneratedMesh())
		{
			QString Error = Library->GetSynthMeshError(Part);

			if (Error.isEmpty())
				Error = tr("Could not generate geometry for part '%1'.").arg(QString::fromLatin1(Part->mPartData->mFileName));

			return swUnexpected(Error);
		}

	return swResult<void>();
}

void swModel::UpdatePartData(std::vector<swModel*>& UpdatedModels)
{
	if (std::find(UpdatedModels.begin(), UpdatedModels.end(), this) != UpdatedModels.end())
		return;

	mPartData->SetModel(this);
	UpdatedModels.push_back(this);

	const swMesh* Mesh = mPartData->GetDisplayMesh();

	if (mParts.empty() && !Mesh)
	{
		mPartData->SetBoundingBox(swVector3(0.0f, 0.0f, 0.0f), swVector3(0.0f, 0.0f, 0.0f));
		return;
	}

	swVector3 Min(FLT_MAX, FLT_MAX, FLT_MAX), Max(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	for (const std::unique_ptr<swPart>& Part : mParts)
	{
		if (Part->IsVisibleInSubModel())
		{
			Part->mPartData->UpdateBoundingBox(UpdatedModels);
			Part->CompareBoundingBox(Min, Max);
		}
	}

	if (Mesh)
	{
		Min = swMinimum(Min, Mesh->mBoundingBox.Min);
		Max = swMaximum(Max, Mesh->mBoundingBox.Max);
	}

	mPartData->SetBoundingBox(Min, Max);
}

void swModel::SetActive(bool Active)
{
	CalculateStep(Active ? mCurrentStep : SW_STEP_MAXIMUM);
	mActive = Active;
}

void swModel::CalculateStep(swStep Step)
{
	for (const std::unique_ptr<swPart>& Part : mParts)
		Part->UpdatePosition(Step);

	for (std::unique_ptr<swCamera>& Camera : mCameras)
		Camera->UpdatePosition(Step);

	for (const std::unique_ptr<swLight>& Light : mLights)
		Light->UpdatePosition(Step);
}

void swModel::SetCurrentStep(swStep Step)
{
	const bool StepChanged = Step != mCurrentStep;
	mCurrentStep = Step;
	CalculateStep(Step);

	gMainWindow->UpdateTimeline(false, false);
	gMainWindow->UpdateSelectedObjects(true);

	// Only a step change turns the view, so edits within a step keep the camera where it is.
	if (StepChanged && swGetPreferences().mFollowStepViews && !mIsPreview)
		for (swView* View : swView::GetModelViews(this))
			View->ApplyStepView();

	UpdateAllViews();
	gMainWindow->UpdateCurrentStep();
}

void swModel::ShowFirstStep()
{
	if (mCurrentStep == 1)
		return;

	SetCurrentStep(1);
}

void swModel::ShowLastStep()
{
	const swStep LastStep = GetLastStep();

	if (mCurrentStep == LastStep)
		return;

	SetCurrentStep(LastStep);
}

void swModel::ShowPreviousStep()
{
	if (mCurrentStep == 1)
		return;

	SetCurrentStep(mCurrentStep - 1);
}

void swModel::ShowNextStep()
{
	if (mCurrentStep == SW_STEP_MAXIMUM)
		return;

	SetCurrentStep(mCurrentStep + 1);
}

swStep swModel::GetLastStep() const
{
	swStep Step = 1;

	for (const std::unique_ptr<swPart>& Part : mParts)
		Step = swMaximum(Step, Part->GetLastStep());

	for (const std::unique_ptr<swCamera>& Camera : mCameras)
		Step = swMaximum(Step, Camera->GetLastStep());

	for (const std::unique_ptr<swLight>& Light : mLights)
		Step = swMaximum(Step, Light->GetLastStep());

	return Step;
}

void swModel::InsertStep(swStep Step)
{
	for (const std::unique_ptr<swPart>& Part : mParts)
		Part->InsertTime(Step, 1);

	for (std::unique_ptr<swCamera>& Camera : mCameras)
		Camera->InsertTime(Step, 1);

	for (const std::unique_ptr<swLight>& Light : mLights)
		Light->InsertTime(Step, 1);
}

void swModel::SetStepView(swStep Step, const std::optional<swStepView>& StepView)
{
	swModelProperties Properties = mProperties;

	if (StepView)
		Properties.mStepViews[Step] = *StepView;
	else
		Properties.mStepViews.erase(Step);

	if (Properties == mProperties)
		return;

	BeginHistorySequence();
	SetModelProperties(Properties);
	EndHistorySequence(tr("Set Step View"));

	SetCurrentStep(mCurrentStep);
}

std::optional<swStepView> swModel::GetEffectiveStepView(swStep Step) const
{
	std::optional<swStepView> Effective;

	// A view set at the end of a step applies to that step and carries on until it is changed or ended.
	for (const std::pair<const swStep, swStepView>& StepView : mProperties.mStepViews)
	{
		if (StepView.first > Step)
			break;

		switch (StepView.second.Mode)
		{
		case swStepViewMode::End:
			Effective.reset();
			break;

		case swStepViewMode::Relative:
		case swStepViewMode::Absolute:
			Effective = StepView.second;
			break;

		case swStepViewMode::Add:
			if (Effective)
				Effective->Angles += StepView.second.Angles;
			else
				Effective = swStepView{ swStepViewMode::Relative, StepView.second.Angles };
			break;
		}
	}

	return Effective;
}

void swModel::ShiftStepViews(swStep Step, int Offset)
{
	swModelProperties Properties = mProperties;
	std::map<swStep, swStepView> StepViews;

	// Step views move with their steps; removing a step drops its view.
	for (const std::pair<const swStep, swStepView>& StepView : mProperties.mStepViews)
	{
		if (StepView.first < Step)
			StepViews[StepView.first] = StepView.second;
		else if (Offset > 0)
			StepViews[StepView.first + Offset] = StepView.second;
		else if (StepView.first > Step)
			StepViews[StepView.first + Offset] = StepView.second;
	}

	Properties.mStepViews = std::move(StepViews);
	SetModelProperties(Properties);
}

void swModel::InsertStepAction(swStep Step)
{
	BeginHistorySequence();
	BeginEditHistory();

	InsertStep(Step);

	EndEditHistory();
	ShiftStepViews(Step, 1);
	EndHistorySequence(tr("Insert Step"));

	SetCurrentStep(mCurrentStep);
}

void swModel::RemoveStepAction(swStep Step)
{
	BeginHistorySequence();
	BeginEditHistory();

	for (const std::unique_ptr<swPart>& Part : mParts)
		Part->RemoveTime(Step, 1);

	for (std::unique_ptr<swCamera>& Camera : mCameras)
		Camera->RemoveTime(Step, 1);

	for (const std::unique_ptr<swLight>& Light : mLights)
		Light->RemoveTime(Step, 1);

	EndEditHistory();
	ShiftStepViews(Step, -1);
	EndHistorySequence(tr("Remove Step"));

	SetCurrentStep(mCurrentStep);
}

void swModel::GetPartsList(int DefaultColorIndex, bool ScanSubModels, bool AddSubModels, swPartsList& PartsList) const
{
	for (const std::unique_ptr<swPart>& Part : mParts)
	{
		if (!Part->IsVisibleInSubModel())
			continue;

		int ColorIndex = Part->GetColorIndex();

		if (ColorIndex == gDefaultColor)
			ColorIndex = DefaultColorIndex;

		Part->mPartData->GetPartsList(ColorIndex, ScanSubModels, AddSubModels, PartsList);
	}
}

void swModel::GetPartsListForStep(swStep Step, int DefaultColorIndex, swPartsList& PartsList, bool Cumulative) const
{
	for (const std::unique_ptr<swPart>& Part : mParts)
	{
		if (Cumulative ? Part->GetStepShow() > Step : Part->GetStepShow() != Step || Part->IsHidden())
			continue;

		int ColorIndex = Part->GetColorIndex();

		if (ColorIndex == gDefaultColor)
			ColorIndex = DefaultColorIndex;

		Part->mPartData->GetPartsList(ColorIndex, false, true, PartsList);
	}
}

void swModel::GetModelParts(const swMatrix44& WorldMatrix, int DefaultColorIndex, std::vector<swModelPartsEntry>& ModelParts) const
{
	if (mHasDirectGeometry && mPartData->GetMesh())
		ModelParts.emplace_back(swModelPartsEntry{ WorldMatrix, mPartData.Get(), mPartData->GetMesh(), DefaultColorIndex });

	for (const std::unique_ptr<swPart>& Part : mParts)
		Part->GetModelParts(WorldMatrix, DefaultColorIndex, ModelParts);
}

void swModel::ShowPropertiesDialog()
{
	swPropertiesDialogOptions Options;

	Options.Properties = mProperties;
	Options.BoundingBox = GetAllPartsBoundingBox();

	GetPartsList(gDefaultColor, true, false, Options.PartsList);

	swPropertiesDialog Dialog(gMainWindow, &Options);
	if (Dialog.exec() != QDialog::Accepted)
		return;

	if (mProperties == Options.Properties)
		return;

	BeginHistorySequence();
	SetModelProperties(Options.Properties);
	EndHistorySequence(tr("Change Model Properties"));
}

void swModel::ShowArrayDialog()
{
	swVector3 Center;

	if (!GetPartFocusOrSelectionCenter(Center))
	{
		QMessageBox::information(gMainWindow, tr("Part Array"), tr("No parts selected."));
		return;
	}

	swArrayDialog Dialog(gMainWindow);

	if (Dialog.exec() != QDialog::Accepted)
		return;

	if (Dialog.mCounts[0] * Dialog.mCounts[1] * Dialog.mCounts[2] < 2)
	{
		QMessageBox::information(gMainWindow, tr("Part Array"), tr("Array only has 1 element or less. No parts added."));
		return;
	}

	BeginHistorySequence();
	BeginEditHistory();

	std::vector<swObject*> NewParts;

	for (int Step1 = 0; Step1 < Dialog.mCounts[0]; Step1++)
	{
		for (int Step2 = 0; Step2 < Dialog.mCounts[1]; Step2++)
		{
			for (int Step3 = (Step1 == 0 && Step2 == 0) ? 1 : 0; Step3 < Dialog.mCounts[2]; Step3++)
			{
				swMatrix44 ModelWorld;
				swVector3 Position;

				swVector3 RotationAngles = Dialog.mRotations[0] * Step1 + Dialog.mRotations[1] * Step2 + Dialog.mRotations[2] * Step3;
				const swVector3 Offset = Dialog.mOffsets[0] * Step1 + Dialog.mOffsets[1] * Step2 + Dialog.mOffsets[2] * Step3;

				for (const std::unique_ptr<swPart>& Part : mParts)
				{
					if (!Part->IsSelected())
						continue;

					ModelWorld = Part->mModelWorld;

					ModelWorld.r[3] -= swVector4(Center, 0.0f);
					ModelWorld = swMultiply(ModelWorld, swMatrix44RotationX(RotationAngles[0] * SW_DEGREES_TO_RADIANS));
					ModelWorld = swMultiply(ModelWorld, swMatrix44RotationY(RotationAngles[1] * SW_DEGREES_TO_RADIANS));
					ModelWorld = swMultiply(ModelWorld, swMatrix44RotationZ(RotationAngles[2] * SW_DEGREES_TO_RADIANS));
					ModelWorld.r[3] += swVector4(Center, 0.0f);

					Position = swVector3(ModelWorld.r[3].x, ModelWorld.r[3].y, ModelWorld.r[3].z);
					ModelWorld.SetTranslation(Position + Offset);

					swPart* NewPart = new swPart(nullptr);
					NewPart->SetPartData(Part->mPartData, Part->GetID(), true, true);
					NewPart->Initialize(ModelWorld, mCurrentStep);
					NewPart->SetColorIndex(Part->GetColorIndex());

					NewParts.emplace_back(NewPart);
				}
			}
		}
	}

	for (size_t PartIdx = 0; PartIdx < NewParts.size(); PartIdx++)
	{
		swPart* Part = (swPart*)NewParts[PartIdx];
		Part->UpdatePosition(mCurrentStep);
		AddPart(Part);
	}

	EndEditHistory();

	AddToSelection(NewParts);

	EndHistorySequence(tr("Part Array"));

	gMainWindow->UpdateTimeline(false, false);
}

void swModel::AddMinifigure(const swMinifigure& Minifigure)
{
	gMainWindow->GetActiveView()->MakeCurrent();

	BeginHistorySequence();
	BeginEditHistory();

	swGroup* Group = AddGroup(tr("Minifigure "), nullptr);
	std::vector<swObject*> Parts;

	Parts.reserve(SW_MINIFIGURE_ITEM_COUNT);

	for (int PartIndex = 0; PartIndex < SW_MINIFIGURE_ITEM_COUNT; PartIndex++)
	{
		if (!Minifigure.PartData[PartIndex])
			continue;

		swPart* Part = new swPart(Minifigure.PartData[PartIndex]);

		Part->Initialize(Minifigure.Matrices[PartIndex], mCurrentStep);
		Part->SetColorIndex(Minifigure.ColorIndices[PartIndex]);
		Part->SetGroup(Group);
		AddPart(Part);
		Part->UpdatePosition(mCurrentStep);

		Parts.emplace_back(Part);
	}

	EndEditHistory();

	SetSelectionAndFocus(Parts, nullptr, 0, swSelectionMode::Single);

	EndHistorySequence(tr("Add Minifigure"));

	gMainWindow->UpdateTimeline(false, false);
}

void swModel::SetMinifigure(const swMinifigure& Minifigure)
{
	DeleteModel();

	std::vector<swObject*> Parts;
	Parts.reserve(SW_MINIFIGURE_ITEM_COUNT);

	for (int PartIdx = 0; PartIdx < SW_MINIFIGURE_ITEM_COUNT; PartIdx++)
	{
		if (!Minifigure.PartData[PartIdx])
			continue;

		swPart* Part = new swPart(Minifigure.PartData[PartIdx]);

		Part->Initialize(Minifigure.Matrices[PartIdx], 1);
		Part->SetColorIndex(Minifigure.ColorIndices[PartIdx]);
		AddPart(Part);
		Part->UpdatePosition(1);

		Parts.emplace_back(Part);
	}
}

void swModel::SetPreviewPartData(swPartData* PartData, int ColorIndex)
{
	DeleteModel();

	swPart* Part = new swPart(PartData);

	Part->Initialize(swMatrix44Identity(), 1);
	Part->SetColorIndex(ColorIndex);
	AddPart(Part);
	Part->UpdatePosition(1);

	mCurrentStep = SW_STEP_MAXIMUM;
	CalculateStep(SW_STEP_MAXIMUM);
}

void swModel::UpdateInterface()
{
	if (!gMainWindow)
		return;

	gMainWindow->UpdateTimeline(true, false);
	gMainWindow->UpdateUndoRedo(!mUndoHistory.empty() ? mUndoHistory[0]->Description : nullptr, !mRedoHistory.empty() ? mRedoHistory[0]->Description : nullptr);
	gMainWindow->UpdatePaste(!gApplication->mClipboard.isEmpty());
	gMainWindow->UpdateCategories();
	gMainWindow->UpdateTitle();
	gMainWindow->SetTool(gMainWindow->GetTool());

	gMainWindow->UpdateSelectedObjects(true);
	gMainWindow->SetTransformType(gMainWindow->GetTransformType());
	gMainWindow->UpdateLockSnap();
	gMainWindow->UpdateSnap();
	gMainWindow->UpdateModels();
	gMainWindow->UpdateShadingMode();
	gMainWindow->UpdateCurrentStep();
	gMainWindow->UpdateSelectionMode();
}
