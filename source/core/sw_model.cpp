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
	mPreviewInsertPieceInfo.clear();

	if (mPieceData)
	{
		if (mPieceData->GetModel() == this)
		{
			if (!mIsPreview && gMainWindow && gMainWindow->GetInsertPieceData() == mPieceData)
				gMainWindow->SetInsertPieceData(nullptr);

			// Keep the name indexed while placements still reference the deleted model.
			swGetPiecesLibrary()->InvalidatePieceDataLoad(mPieceData.Get());
			mPieceData->DetachContainer();
			mPieceData->SetFailed(tr("Model '%1' was deleted.").arg(QString::fromLatin1(mPieceData->mFileName)));
			swGetPiecesLibrary()->NotifyAssetStatusChanged();
		}

		mPieceData.Reset();
	}

	DeleteModel();
}

bool swModel::GetPieceWorldMatrix(swPiece* Piece, swMatrix44& ParentWorldMatrix) const
{
	for (const std::unique_ptr<swPiece>& ModelPiece : mPieces)
	{
		if (ModelPiece.get() == Piece)
		{
			ParentWorldMatrix = swMultiply(ModelPiece->mModelWorld, ParentWorldMatrix);
			return true;
		}

		const swPieceData* PieceData = ModelPiece->mPieceData.Get();

		if (PieceData->IsModel())
		{
			swMatrix44 WorldMatrix = swMultiply(ModelPiece->mModelWorld, ParentWorldMatrix);

			if (PieceData->GetPieceWorldMatrix(Piece, WorldMatrix))
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

		for (const std::unique_ptr<swPiece>& Piece : Current->mPieces)
		{
			const swPieceData* PieceData = Piece->mPieceData.Get();

			if (PieceData && PieceData->IsModel() && Visit(PieceData->GetModel()))
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

	mPieces.clear();
	mCameras.clear();
	mLights.clear();
	mGroups.clear();
	mFileLines.clear();
	mHasDirectGeometry = false;
}

void swModel::CreatePieceData(Project* Project)
{
	swPiecesLibrary* Library = swGetPiecesLibrary();
	bool Reused;
	mPieceData = Library->AcquireModelPieceData(mProperties.mFileName.toLatin1().constData(), Project, Reused);
	mPieceData->SetModel(this);

	if (Reused)
	{
		// Reusing a referenced name can turn an existing placement into a cycle.
		mPieces.erase(std::remove_if(mPieces.begin(), mPieces.end(), [this](const std::unique_ptr<swPiece>& Piece)
		{
			return Piece->mPieceData && Piece->mPieceData->IncludesModel(this);
		}), mPieces.end());
	}

	// The model owns the reference acquired by AcquireModelPieceData(), including while
	// mesh requests are deferred until its source lines have been parsed.
	if (!Project->DefersModelMeshRequests())
	{
		if (mIsPreview)
			Library->RequestPieceDataLoad(mPieceData.Get(), swLoadPriority::Normal);
		else
			Library->WaitForPieceData(mPieceData);
	}

	if (Reused)
	{
		UpdateAllViews();

		if (!Project->DefersModelMeshRequests())
			RefreshPreview();
	}
}

bool swModel::RebindPieceData(swPieceData* Previous, swPieceData* Replacement)
{
	bool Changed = false;

	mPieces.erase(std::remove_if(mPieces.begin(), mPieces.end(), [this, Previous, Replacement, &Changed](const std::unique_ptr<swPiece>& Piece)
	{
		if (Piece->mPieceData != Previous)
			return false;

		Changed = true;

		if (Replacement->IncludesModel(this))
			return true;

		const QString PieceId = Piece->GetID();
		Piece->SetPieceData(swPieceDataReference(Replacement), PieceId, false, false);

		return false;
	}), mPieces.end());

	return Changed;
}

void swModel::QueueMeshBuild()
{
	mPieceData->SetModel(this);
	swGetPiecesLibrary()->RebuildModelPieceData(mPieceData.Get());
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

std::vector<swPieceDataReference> swModel::GetRequiredPieceData() const
{
	std::vector<swPieceDataReference> RequiredPieceData;
	std::unordered_set<const swModel*> Visited;

	const std::function<void(const swModel*)> Collect = [&RequiredPieceData, &Visited, &Collect](const swModel* Model)
	{
		if (!Model || !Visited.insert(Model).second)
			return;

		if (Model->mPieceData)
			RequiredPieceData.push_back(Model->mPieceData);

		for (const std::unique_ptr<swPiece>& Piece : Model->mPieces)
		{
			const swPieceDataReference& PieceData = Piece->mPieceData;

			if (PieceData->IsModel())
				Collect(PieceData->GetModel());
			else if (PieceData->IsProject())
				Collect(PieceData->GetProject()->GetMainModel());
			else
				RequiredPieceData.push_back(PieceData);
		}
	};

	Collect(this);

	std::sort(RequiredPieceData.begin(), RequiredPieceData.end());
	RequiredPieceData.erase(std::unique(RequiredPieceData.begin(), RequiredPieceData.end()), RequiredPieceData.end());

	return RequiredPieceData;
}

std::vector<swPiece*> swModel::GetRequiredSynthPieces() const
{
	std::vector<swPiece*> Required;
	std::unordered_set<const swModel*> Visited;

	const std::function<void(const swModel*)> Collect = [&Required, &Visited, &Collect](const swModel* Model)
	{
		if (!Model || !Visited.insert(Model).second)
			return;

		for (const std::unique_ptr<swPiece>& Piece : Model->mPieces)
		{
			swPieceData* PieceData = Piece->mPieceData.Get();

			if (PieceData->IsModel())
				Collect(PieceData->GetModel());
			else if (PieceData->IsProject())
				Collect(PieceData->GetProject()->GetMainModel());
			else if (PieceData->GetSynthInfo())
				Required.push_back(Piece.get());
		}
	};

	Collect(this);

	return Required;
}

void swModel::WaitForAssets() const
{
	const std::vector<swPieceDataReference> RequiredPieceData = GetRequiredPieceData();
	swPiecesLibrary* Library = swGetPiecesLibrary();

	swBlockingProgress Progress(tr("Loading"), tr("Loading pieces..."), false);

	Library->WaitForPieceData(RequiredPieceData, [&Progress](int Finished, int Total)
	{
		Progress.SetProgress(Finished, Total);
	});

	Library->WaitForSynthMeshes(GetRequiredSynthPieces());

	// Temporary minifigure and thumbnail models have no container piece to update.
	if (mPieceData)
	{
		std::vector<swModel*> UpdatedModels;
		const_cast<swModel*>(this)->UpdatePieceData(UpdatedModels);
	}
}

swResult<void> swModel::EnsureAssetsReady() const
{
	WaitForAssets();

	const std::vector<swPieceDataReference> RequiredPieceData = GetRequiredPieceData();
	swPiecesLibrary* Library = swGetPiecesLibrary();

	for (const swPieceDataReference& RequiredData : RequiredPieceData)
	{
		if (RequiredData->mState == swAssetState::Loaded)
			continue;

		const QString Error = Library->GetPieceDataLoadError(RequiredData.Get());

		if (!Error.isEmpty())
			return swUnexpected(Error);

		return swUnexpected(tr("Could not load all required pieces."));
	}

	const std::vector<swPiece*> GeneratedPieces = GetRequiredSynthPieces();

	for (const swPiece* Piece : GeneratedPieces)
		if (!Piece->HasGeneratedMesh())
		{
			QString Error = Library->GetSynthMeshError(Piece);

			if (Error.isEmpty())
				Error = tr("Could not generate geometry for piece '%1'.").arg(QString::fromLatin1(Piece->mPieceData->mFileName));

			return swUnexpected(Error);
		}

	return swResult<void>();
}

void swModel::UpdatePieceData(std::vector<swModel*>& UpdatedModels)
{
	if (std::find(UpdatedModels.begin(), UpdatedModels.end(), this) != UpdatedModels.end())
		return;

	mPieceData->SetModel(this);
	UpdatedModels.push_back(this);

	const swMesh* Mesh = mPieceData->GetDisplayMesh();

	if (mPieces.empty() && !Mesh)
	{
		mPieceData->SetBoundingBox(swVector3(0.0f, 0.0f, 0.0f), swVector3(0.0f, 0.0f, 0.0f));
		return;
	}

	swVector3 Min(FLT_MAX, FLT_MAX, FLT_MAX), Max(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	for (const std::unique_ptr<swPiece>& Piece : mPieces)
	{
		if (Piece->IsVisibleInSubModel())
		{
			Piece->mPieceData->UpdateBoundingBox(UpdatedModels);
			Piece->CompareBoundingBox(Min, Max);
		}
	}

	if (Mesh)
	{
		Min = swMinimum(Min, Mesh->mBoundingBox.Min);
		Max = swMaximum(Max, Mesh->mBoundingBox.Max);
	}

	mPieceData->SetBoundingBox(Min, Max);
}

void swModel::SetActive(bool Active)
{
	CalculateStep(Active ? mCurrentStep : SW_STEP_MAXIMUM);
	mActive = Active;
}

void swModel::CalculateStep(swStep Step)
{
	for (const std::unique_ptr<swPiece>& Piece : mPieces)
		Piece->UpdatePosition(Step);

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

	for (const std::unique_ptr<swPiece>& Piece : mPieces)
		Step = swMaximum(Step, Piece->GetLastStep());

	for (const std::unique_ptr<swCamera>& Camera : mCameras)
		Step = swMaximum(Step, Camera->GetLastStep());

	for (const std::unique_ptr<swLight>& Light : mLights)
		Step = swMaximum(Step, Light->GetLastStep());

	return Step;
}

void swModel::InsertStep(swStep Step)
{
	for (const std::unique_ptr<swPiece>& Piece : mPieces)
		Piece->InsertTime(Step, 1);

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

	for (const std::unique_ptr<swPiece>& Piece : mPieces)
		Piece->RemoveTime(Step, 1);

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
	for (const std::unique_ptr<swPiece>& Piece : mPieces)
	{
		if (!Piece->IsVisibleInSubModel())
			continue;

		int ColorIndex = Piece->GetColorIndex();

		if (ColorIndex == gDefaultColor)
			ColorIndex = DefaultColorIndex;

		Piece->mPieceData->GetPartsList(ColorIndex, ScanSubModels, AddSubModels, PartsList);
	}
}

void swModel::GetPartsListForStep(swStep Step, int DefaultColorIndex, swPartsList& PartsList, bool Cumulative) const
{
	for (const std::unique_ptr<swPiece>& Piece : mPieces)
	{
		if (Cumulative ? Piece->GetStepShow() > Step : Piece->GetStepShow() != Step || Piece->IsHidden())
			continue;

		int ColorIndex = Piece->GetColorIndex();

		if (ColorIndex == gDefaultColor)
			ColorIndex = DefaultColorIndex;

		Piece->mPieceData->GetPartsList(ColorIndex, false, true, PartsList);
	}
}

void swModel::GetModelParts(const swMatrix44& WorldMatrix, int DefaultColorIndex, std::vector<swModelPartsEntry>& ModelParts) const
{
	if (mHasDirectGeometry && mPieceData->GetMesh())
		ModelParts.emplace_back(swModelPartsEntry{ WorldMatrix, mPieceData.Get(), mPieceData->GetMesh(), DefaultColorIndex });

	for (const std::unique_ptr<swPiece>& Piece : mPieces)
		Piece->GetModelParts(WorldMatrix, DefaultColorIndex, ModelParts);
}

void swModel::ShowPropertiesDialog()
{
	swPropertiesDialogOptions Options;

	Options.Properties = mProperties;
	Options.BoundingBox = GetAllPiecesBoundingBox();

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

	if (!GetPieceFocusOrSelectionCenter(Center))
	{
		QMessageBox::information(gMainWindow, tr("Piece Array"), tr("No pieces selected."));
		return;
	}

	swArrayDialog Dialog(gMainWindow);

	if (Dialog.exec() != QDialog::Accepted)
		return;

	if (Dialog.mCounts[0] * Dialog.mCounts[1] * Dialog.mCounts[2] < 2)
	{
		QMessageBox::information(gMainWindow, tr("Piece Array"), tr("Array only has 1 element or less. No pieces added."));
		return;
	}

	BeginHistorySequence();
	BeginEditHistory();

	std::vector<swObject*> NewPieces;

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

				for (const std::unique_ptr<swPiece>& Piece : mPieces)
				{
					if (!Piece->IsSelected())
						continue;

					ModelWorld = Piece->mModelWorld;

					ModelWorld.r[3] -= swVector4(Center, 0.0f);
					ModelWorld = swMultiply(ModelWorld, swMatrix44RotationX(RotationAngles[0] * SW_DEGREES_TO_RADIANS));
					ModelWorld = swMultiply(ModelWorld, swMatrix44RotationY(RotationAngles[1] * SW_DEGREES_TO_RADIANS));
					ModelWorld = swMultiply(ModelWorld, swMatrix44RotationZ(RotationAngles[2] * SW_DEGREES_TO_RADIANS));
					ModelWorld.r[3] += swVector4(Center, 0.0f);

					Position = swVector3(ModelWorld.r[3].x, ModelWorld.r[3].y, ModelWorld.r[3].z);
					ModelWorld.SetTranslation(Position + Offset);

					swPiece* NewPiece = new swPiece(nullptr);
					NewPiece->SetPieceData(Piece->mPieceData, Piece->GetID(), true, true);
					NewPiece->Initialize(ModelWorld, mCurrentStep);
					NewPiece->SetColorIndex(Piece->GetColorIndex());

					NewPieces.emplace_back(NewPiece);
				}
			}
		}
	}

	for (size_t PieceIdx = 0; PieceIdx < NewPieces.size(); PieceIdx++)
	{
		swPiece* Piece = (swPiece*)NewPieces[PieceIdx];
		Piece->UpdatePosition(mCurrentStep);
		AddPiece(Piece);
	}

	EndEditHistory();

	AddToSelection(NewPieces);

	EndHistorySequence(tr("Piece Array"));

	gMainWindow->UpdateTimeline(false, false);
}

void swModel::AddMinifigure(const swMinifigure& Minifigure)
{
	gMainWindow->GetActiveView()->MakeCurrent();

	BeginHistorySequence();
	BeginEditHistory();

	swGroup* Group = AddGroup(tr("Minifigure "), nullptr);
	std::vector<swObject*> Pieces;

	Pieces.reserve(SW_MINIFIGURE_ITEM_COUNT);

	for (int PartIndex = 0; PartIndex < SW_MINIFIGURE_ITEM_COUNT; PartIndex++)
	{
		if (!Minifigure.PieceData[PartIndex])
			continue;

		swPiece* Piece = new swPiece(Minifigure.PieceData[PartIndex]);

		Piece->Initialize(Minifigure.Matrices[PartIndex], mCurrentStep);
		Piece->SetColorIndex(Minifigure.ColorIndices[PartIndex]);
		Piece->SetGroup(Group);
		AddPiece(Piece);
		Piece->UpdatePosition(mCurrentStep);

		Pieces.emplace_back(Piece);
	}

	EndEditHistory();

	SetSelectionAndFocus(Pieces, nullptr, 0, swSelectionMode::Single);

	EndHistorySequence(tr("Add Minifigure"));

	gMainWindow->UpdateTimeline(false, false);
}

void swModel::SetMinifigure(const swMinifigure& Minifigure)
{
	DeleteModel();

	std::vector<swObject*> Pieces;
	Pieces.reserve(SW_MINIFIGURE_ITEM_COUNT);

	for (int PartIdx = 0; PartIdx < SW_MINIFIGURE_ITEM_COUNT; PartIdx++)
	{
		if (!Minifigure.PieceData[PartIdx])
			continue;

		swPiece* Piece = new swPiece(Minifigure.PieceData[PartIdx]);

		Piece->Initialize(Minifigure.Matrices[PartIdx], 1);
		Piece->SetColorIndex(Minifigure.ColorIndices[PartIdx]);
		AddPiece(Piece);
		Piece->UpdatePosition(1);

		Pieces.emplace_back(Piece);
	}
}

void swModel::SetPreviewPieceData(swPieceData* PieceData, int ColorIndex)
{
	DeleteModel();

	swPiece* Piece = new swPiece(PieceData);

	Piece->Initialize(swMatrix44Identity(), 1);
	Piece->SetColorIndex(ColorIndex);
	AddPiece(Piece);
	Piece->UpdatePosition(1);

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
