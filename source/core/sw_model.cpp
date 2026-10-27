#include "sw_global.h"
#include "sw_model.h"
#include "sw_model_history.h"
#include "sw_piece.h"
#include "sw_camera.h"
#include "sw_light.h"
#include "sw_group.h"
#include "sw_project.h"
#include "sw_main_window.h"
#include "sw_profile.h"
#include "sw_library.h"
#include "sw_scene.h"
#include "sw_train_track.h"
#include "sw_file.h"
#include "sw_piece_data.h"
#include "sw_view.h"
#include "sw_minifigure_builder.h"
#include "sw_array_dialog.h"
#include "sw_group_dialog.h"
#include "sw_edit_groups_dialog.h"
#include "sw_properties_dialog.h"
#include "sw_utilities.h"
#include "sw_lxf.h"
#include "sw_preview_widget.h"
#include "sw_string.h"
#include <locale.h>

// Meta commands are written with the established tag so models stay compatible, both tags are read.
static bool swIsMetaCommand(const QString& Token)
{
	return Token == QLatin1String("!LEOCAD") || Token == QLatin1String("!STUDWORKS");
}

void swModelProperties::LoadDefaults()
{
	mAuthor = swGetProfileString(SW_PROFILE_DEFAULT_AUTHOR_NAME);
	mAmbientColor = swVector3FromColor(swGetProfileUnsignedInteger(SW_PROFILE_DEFAULT_AMBIENT_COLOR));
}

void swModelProperties::SaveDefaults()
{
	swSetProfileUnsignedInteger(SW_PROFILE_DEFAULT_AMBIENT_COLOR, swColorFromVector3(mAmbientColor));
}

void swModelProperties::SaveLDraw(QTextStream& Stream) const
{
	const QLatin1String LineEnding("\r\n");

	Stream << QLatin1String("0 ") << mDescription << LineEnding;
	Stream << QLatin1String("0 Name: ") << mModelName << LineEnding;
	Stream << QLatin1String("0 Author: ") << mAuthor << LineEnding;

	if (!mComments.isEmpty())
	{
		const QStringList Comments = mComments.split('\n');
		for (const QString& Comment : Comments)
			Stream << QLatin1String("0 !LEOCAD MODEL COMMENT ") << Comment << LineEnding;
	}

//	swVector3 mAmbientColor;
}

bool swModelProperties::ParseLDrawHeader(QString Line, bool FirstLine)
{
	QTextStream LineStream(&Line, QIODevice::ReadOnly);

	QString Token;
	LineStream >> Token;
	const int StartPos = LineStream.pos();
	LineStream >> Token;

	if (swIsMetaCommand(Token))
		return false;

	if (Token == QLatin1String("Name:"))
	{
		mModelName = LineStream.readLine().mid(1);
		return true;
	}

	if (Token == QLatin1String("Author:"))
	{
		mAuthor = LineStream.readLine().mid(1);
		return true;
	}

	if (FirstLine)
	{
		LineStream.seek(StartPos);
		mDescription = LineStream.readLine().mid(1);
		return true;
	}

	return false;
}

void swModelProperties::ParseLDrawLine(QTextStream& Stream)
{
	QString Token;
	Stream >> Token;

	if (Token == QLatin1String("AUTHOR"))
		mAuthor = Stream.readLine().mid(1);
	else if (Token == QLatin1String("DESCRIPTION"))
		mDescription = Stream.readLine().mid(1);
	else if (Token == QLatin1String("COMMENT"))
	{
		QString Comment = Stream.readLine().mid(1);
		if (!mComments.isEmpty())
			mComments += '\n';
		mComments += Comment;
	}
}

void swPOVRayOptions::ParseLDrawLine(QTextStream& LineStream)
{
	QString Token;
	LineStream >> Token;

	if (Token == QLatin1String("HEADER_INCLUDE_FILE"))
	{
		LineStream >> HeaderIncludeFile;
		if (!QFileInfo(HeaderIncludeFile).isReadable())
			HeaderIncludeFile.clear();
	}
	else if (Token == QLatin1String("FOOTER_INCLUDE_FILE"))
	{
		LineStream >> FooterIncludeFile;
		if (!QFileInfo(FooterIncludeFile).isReadable())
			FooterIncludeFile.clear();
	}
	else if (Token == QLatin1String("FLOOR_AXIS"))
	{
		LineStream >> FloorAxis;
		if (FloorAxis < 0 || FloorAxis > 2)
			FloorAxis = 1; // y
	}
	else if (Token == QLatin1String("FLOOR_COLOR_RGB"))
		LineStream >> FloorColor[0] >> FloorColor[1] >> FloorColor[2];
	else if (Token == QLatin1String("FLOOR_AMBIENT"))
		LineStream >> FloorAmbient;
	else if (Token == QLatin1String("FLOOR_DIFFUSE"))
		LineStream >> FloorDiffuse;
	else if (Token == QLatin1String("EXCLUDE_FLOOR"))
		ExcludeFloor = true;
	else if (Token == QLatin1String("EXCLUDE_BACKGROUND"))
		ExcludeBackground = true;
	else if (Token == QLatin1String("NO_REFLECTION"))
		NoReflection = true;
	else if (Token == QLatin1String("NO_SHADOWS"))
		NoShadow = true;
	else if (Token == QLatin1String("USE_LGEO"))
		UseLGEO = true;
}

void swPOVRayOptions::SaveLDraw(QTextStream& Stream) const
{
	const QLatin1String LineEnding("\r\n");
	if (!HeaderIncludeFile.isEmpty())
		Stream << QLatin1String("0 !LEOCAD POV_RAY HEADER_INCLUDE_FILE ") << QDir::toNativeSeparators(HeaderIncludeFile) << LineEnding;
	if (!FooterIncludeFile.isEmpty())
		Stream << QLatin1String("0 !LEOCAD POV_RAY FOOTER_INCLUDE_FILE ") << QDir::toNativeSeparators(FooterIncludeFile) << LineEnding;
	if (FloorAxis != 1)
		Stream << QLatin1String("0 !LEOCAD POV_RAY FLOOR_AXIS ") << FloorAxis << LineEnding;
	if (FloorColor != swVector3(0.8f,0.8f,0.8f))
		Stream << QLatin1String("0 !LEOCAD POV_RAY FLOOR_COLOR_RGB ") << FloorColor[0] << ' ' << FloorColor[1] << ' ' << FloorColor[2] << LineEnding;
	if (FloorAmbient != 0.4f)
		Stream << QLatin1String("0 !LEOCAD POV_RAY FLOOR_AMBIENT ") << FloorAmbient << LineEnding;
	if (FloorDiffuse != 0.4f)
		Stream << QLatin1String("0 !LEOCAD POV_RAY FLOOR_DIFFUSE ") << FloorDiffuse << LineEnding;
	if (ExcludeFloor)
		Stream << QLatin1String("0 !LEOCAD POV_RAY EXCLUDE_FLOOR") << LineEnding;
	if (ExcludeBackground)
		Stream << QLatin1String("0 !LEOCAD POV_RAY EXCLUDE_BACKGROUND") << LineEnding;
	if (NoReflection)
		Stream << QLatin1String("0 !LEOCAD POV_RAY NO_REFLECTION") << LineEnding;
	if (NoShadow)
		Stream << QLatin1String("0 !LEOCAD POV_RAY NO_SHADOWS") << LineEnding;
	if (UseLGEO)
		Stream << QLatin1String("0 !LEOCAD POV_RAY USE_LGEO") << LineEnding;
}

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

static QString swGetStepEndLine(const swModelProperties& Properties, swStep Step)
{
	const std::map<swStep, swStepView>::const_iterator StepView = Properties.mStepViews.find(Step);

	if (StepView == Properties.mStepViews.end())
		return QLatin1String("0 STEP\r\n");

	if (StepView->second.Mode == swStepViewMode::End)
		return QLatin1String("0 ROTSTEP END\r\n");

	const char* Modes[] = { "REL", "ABS", "ADD" };
	const swVector3& Angles = StepView->second.Angles;

	return QString("0 ROTSTEP %1 %2 %3 %4\r\n").arg(swFormatValue(Angles.x, 2), swFormatValue(Angles.y, 2), swFormatValue(Angles.z, 2), QLatin1String(Modes[static_cast<int>(StepView->second.Mode)]));
}

void swModel::SaveLDraw(QTextStream& Stream, bool SelectedOnly, swStep LastStep) const
{
	const QLatin1String LineEnding("\r\n");

	mProperties.SaveLDraw(Stream);

	for (const QString& Directory : mAssetSourceDirectories)
		Stream << QLatin1String("0 !LEOCAD MODEL ASSET_SOURCE ") << QString::fromLatin1(Directory.toUtf8().toBase64()) << LineEnding;

	if (!mAssetSourceDirectories.empty() && !mPreferCurrentAssetDirectory)
		Stream << QLatin1String("0 !LEOCAD MODEL ASSET_SOURCE_PRIORITY SOURCE") << LineEnding;

	std::vector<swGroup*> CurrentGroups;
	swStep Step = 1;
	int CurrentLine = 0;
	int AddedSteps = 0;
	bool SavedStep = false;

	for (const std::unique_ptr<swPiece>& Piece : mPieces)
	{
		if (SelectedOnly && !Piece->IsSelected())
			continue;

		while (Piece->GetFileLine() > CurrentLine && CurrentLine < mFileLines.size())
		{
			QString Line = mFileLines[CurrentLine];
			QTextStream LineStream(&Line, QIODevice::ReadOnly);

			QString Token;
			LineStream >> Token;
			bool Skip = false;
			bool StepLine = false;

			if (Token == QLatin1String("0"))
			{
				LineStream >> Token;

				if (Token == QLatin1String("STEP"))
				{
					if (Piece->GetStepShow() > Step)
					{
						if (LastStep && Step >= LastStep)
						{
							SavedStep = true;
							break;
						}

						Step++;
						StepLine = true;
					}
					else
						Skip = true;
				}
			}

			if (!Skip)
			{
				Stream << (StepLine ? swGetStepEndLine(mProperties, Step - 1) : mFileLines[CurrentLine]);
				if (AddedSteps > 0)
					AddedSteps--;
			}
			CurrentLine++;
		}

		if (SavedStep || (LastStep && Piece->GetStepShow() > LastStep))
			break;

		while (Piece->GetStepShow() > Step)
		{
			Stream << swGetStepEndLine(mProperties, Step);
			AddedSteps++;
			Step++;
		}

		swGroup* PieceGroup = Piece->GetGroup();

		if (PieceGroup)
		{
			if (CurrentGroups.empty() || (!CurrentGroups.empty() && PieceGroup != CurrentGroups[CurrentGroups.size() - 1]))
			{
				std::deque<swGroup*> PieceParents;

				for (swGroup* Group = PieceGroup; Group; Group = Group->mGroup)
					PieceParents.push_front(Group);

				std::deque<swGroup*>::iterator ParentsToAdd = PieceParents.begin();

				while (!CurrentGroups.empty())
				{
					swGroup* Group = CurrentGroups.back();
					const std::deque<swGroup*>::iterator ParentFound = std::find(PieceParents.begin(), PieceParents.end(), Group);

					if (ParentFound == PieceParents.end())
					{
						CurrentGroups.pop_back();
						Stream << QLatin1String("0 !LEOCAD GROUP END\r\n");
					}
					else
					{
						ParentsToAdd = ParentFound + 1;
						break;
					}
				}

				for (std::deque<swGroup*>::iterator ParentIt = ParentsToAdd; ParentIt != PieceParents.end(); ParentIt++)
				{
					swGroup* Group = *ParentIt;
					CurrentGroups.emplace_back(Group);
					Stream << QLatin1String("0 !LEOCAD GROUP BEGIN ") << Group->mName << LineEnding;
				}
			}
		}
		else
		{
			while (CurrentGroups.size())
			{
				CurrentGroups.pop_back();
				Stream << QLatin1String("0 !LEOCAD GROUP END\r\n");
			}
		}

		if (Piece->mPieceData->GetSynthInfo())
		{
			Stream << QLatin1String("0 !LEOCAD SYNTH BEGIN\r\n");

			const std::vector<swPieceControlPoint>& ControlPoints = Piece->GetControlPoints();
			for (const swPieceControlPoint& ControlPoint : ControlPoints)
			{
				Stream << QLatin1String("0 !LEOCAD SYNTH CONTROL_POINT");

				const float* FloatMatrix = ControlPoint.Transform.GetFloats();
				const float Numbers[13] = { FloatMatrix[12], -FloatMatrix[14], FloatMatrix[13], FloatMatrix[0], -FloatMatrix[8], FloatMatrix[4], -FloatMatrix[2], FloatMatrix[10], -FloatMatrix[6], FloatMatrix[1], -FloatMatrix[9], FloatMatrix[5], ControlPoint.Scale };

				for (int NumberIdx = 0; NumberIdx < 13; NumberIdx++)
					Stream << ' ' << swFormatValue(Numbers[NumberIdx], NumberIdx < 3 ? 4 : 6);

				Stream << LineEnding;
			}
		}

		Piece->SaveLDraw(Stream);

		if (Piece->mPieceData->GetSynthInfo())
			Stream << QLatin1String("0 !LEOCAD SYNTH END\r\n");
	}

	while (!SavedStep && CurrentLine < mFileLines.size())
	{
		QString Line = mFileLines[CurrentLine];
		QTextStream LineStream(&Line, QIODevice::ReadOnly);

		QString Token;
		LineStream >> Token;
		bool Skip = false;
		bool StepLine = false;

		if (Token == QLatin1String("0"))
		{
			LineStream >> Token;

			if (Token == QLatin1String("STEP"))
			{
				if (AddedSteps-- > 0)
					Skip = true;
				else
				{
					if (LastStep && Step >= LastStep)
						break;

					Step++;
					StepLine = true;
				}
			}
		}

		if (!Skip)
			Stream << (StepLine ? swGetStepEndLine(mProperties, Step - 1) : mFileLines[CurrentLine]);
		CurrentLine++;
	}

	while (CurrentGroups.size())
	{
		CurrentGroups.pop_back();
		Stream << QLatin1String("0 !LEOCAD GROUP END\r\n");
	}

	for (const std::unique_ptr<swCamera>& Camera : mCameras)
		if (!SelectedOnly || Camera->IsSelected())
			Camera->SaveLDraw(Stream);

	for (const std::unique_ptr<swLight>& Light : mLights)
		if (!SelectedOnly || Light->IsSelected())
			Light->SaveLDraw(Stream);

	Stream.flush();
}

void swModel::LoadLDraw(QIODevice& Device, Project* Project)
{
	swPiece* Piece = nullptr;
	swCamera* Camera = nullptr;
	swLight* Light = nullptr;
	std::vector<swGroup*> CurrentGroups;
	std::vector<swPieceControlPoint> ControlPoints;
	int CurrentStep = 1;
	swPiecesLibrary* Library = swGetPiecesLibrary();
	mHasDirectGeometry = false;

	mProperties.mAuthor.clear();
	mProperties.mDescription.clear();
	mProperties.mComments.clear();
	mProperties.mStepViews.clear();
	mAssetSourceDirectories.clear();
	mPreferCurrentAssetDirectory = true;
	bool ReadingHeader = true;
	bool FirstLine = true;

	while (!Device.atEnd())
	{
		const qint64 Pos = Device.pos();
		QString OriginalLine = Device.readLine();
		QString Line = OriginalLine.trimmed();
		QTextStream LineStream(&Line, QIODevice::ReadOnly);

		QString Token;
		LineStream >> Token;

		if (Token == QLatin1String("0"))
		{
			LineStream >> Token;

			if (Token == QLatin1String("FILE"))
			{
				QString Name = LineStream.readAll().trimmed();

				if (mProperties.mFileName != Name)
				{
					Device.seek(Pos);
					break;
				}

				continue;
			}
			else if (Token == QLatin1String("NOFILE") || Token == QLatin1String("!DATA"))
			{
				break;
			}

			if (ReadingHeader)
			{
				ReadingHeader = mProperties.ParseLDrawHeader(Line, FirstLine);
				FirstLine = false;

				if (ReadingHeader)
					continue;
			}

			if (Token == QLatin1String("STEP") || Token == QLatin1String("ROTSTEP"))
			{
				// ROTSTEP ends the step like STEP and sets its view rotation.
				if (Token == QLatin1String("ROTSTEP"))
				{
					swStepView StepView;
					QString Value;
					LineStream >> Value;

					if (Value.compare(QLatin1String("END"), Qt::CaseInsensitive) == 0)
						StepView.Mode = swStepViewMode::End;
					else
					{
						QString Mode;
						StepView.Angles.x = Value.toFloat();
						LineStream >> StepView.Angles.y >> StepView.Angles.z >> Mode;

						if (Mode.compare(QLatin1String("ABS"), Qt::CaseInsensitive) == 0)
							StepView.Mode = swStepViewMode::Absolute;
						else if (Mode.compare(QLatin1String("ADD"), Qt::CaseInsensitive) == 0)
							StepView.Mode = swStepViewMode::Add;
					}

					mProperties.mStepViews[CurrentStep] = StepView;
					OriginalLine = QLatin1String(OriginalLine.endsWith(QLatin1String("\r\n")) ? "0 STEP\r\n" : "0 STEP\n");
				}

				delete Piece;
				Piece = nullptr;
				CurrentStep++;
				mFileLines.append(OriginalLine);
				continue;
			}

			if (!swIsMetaCommand(Token))
			{
				mFileLines.append(OriginalLine);

				if (Token == QLatin1String("!:"))
				{
					LineStream >> Token;

					if (Token == QLatin1String("1") || Token == QLatin1String("2") || Token == QLatin1String("3") || Token == QLatin1String("4") || Token == QLatin1String("5"))
						mHasDirectGeometry = true;
				}

				continue;
			}

			LineStream >> Token;

			if (Token == QLatin1String("MODEL"))
			{
				LineStream >> Token;

				if (Token == QLatin1String("ASSET_SOURCE"))
				{
					const QString Directory = QString::fromUtf8(QByteArray::fromBase64(LineStream.readAll().trimmed().toLatin1()));

					if (QDir::isAbsolutePath(Directory))
					{
						const QString CleanDirectory = QDir::cleanPath(Directory);

						if (std::find(mAssetSourceDirectories.begin(), mAssetSourceDirectories.end(), CleanDirectory) == mAssetSourceDirectories.end())
							mAssetSourceDirectories.push_back(CleanDirectory);
					}
				}
				else if (Token == QLatin1String("ASSET_SOURCE_PRIORITY"))
				{
					LineStream >> Token;
					mPreferCurrentAssetDirectory = Token != QLatin1String("SOURCE");
				}
				else
				{
					QString PropertiesLine = Token + LineStream.readAll();
					QTextStream PropertiesStream(&PropertiesLine, QIODevice::ReadOnly);
					mProperties.ParseLDrawLine(PropertiesStream);
				}
			}
			else if (Token == QLatin1String("PIECE"))
			{
				if (!Piece)
					Piece = new swPiece(nullptr);

				Piece->ParseLDrawLine(LineStream);
			}
			else if (Token == QLatin1String("CAMERA"))
			{
				if (!Camera)
					Camera = new swCamera(false);

				if (Camera->ParseLDrawLine(LineStream))
				{
					Camera->CreateName(mCameras);
					mCameras.emplace_back(Camera);
					Camera = nullptr;
				}
			}
			else if (Token == QLatin1String("LIGHT"))
			{
				if (!Light)
					Light = new swLight(swVector3(0.0f, 0.0f, 0.0f), swLightType::Point);

				if (Light->ParseLDrawLine(LineStream))
				{
					Light->CreateName(mLights);
					mLights.emplace_back(Light);
					Light = nullptr;
				}
			}
			else if (Token == QLatin1String("POV_RAY"))
			{
				mPOVRayOptions.ParseLDrawLine(LineStream);
			}
			else if (Token == QLatin1String("GROUP"))
			{
				LineStream >> Token;

				if (Token == QLatin1String("BEGIN"))
				{
					QString Name = LineStream.readAll().trimmed();
					swGroup* Group = GetGroup(Name, true);
					if (!CurrentGroups.empty())
						Group->mGroup = CurrentGroups[CurrentGroups.size() - 1];
					else
						Group->mGroup = nullptr;
					CurrentGroups.emplace_back(Group);
				}
				else if (Token == QLatin1String("END"))
				{
					if (!CurrentGroups.empty())
						CurrentGroups.pop_back();
				}
			}
			else if (Token == QLatin1String("SYNTH"))
			{
				LineStream >> Token;

				if (Token == QLatin1String("BEGIN"))
				{
					ControlPoints.clear();
				}
				else if (Token == QLatin1String("END"))
				{
					ControlPoints.clear();
				}
				else if (Token == QLatin1String("CONTROL_POINT"))
				{
					float Numbers[13];
					for (int TokenIdx = 0; TokenIdx < 13; TokenIdx++)
						LineStream >> Numbers[TokenIdx];

					swPieceControlPoint& PieceControlPoint = ControlPoints.emplace_back();
					PieceControlPoint.Transform = swMatrix44(swVector4(Numbers[3], Numbers[9], -Numbers[6], 0.0f), swVector4(Numbers[5], Numbers[11], -Numbers[8], 0.0f),
					                                         swVector4(-Numbers[4], -Numbers[10], Numbers[7], 0.0f), swVector4(Numbers[0], Numbers[2], -Numbers[1], 1.0f));
					PieceControlPoint.Scale = Numbers[12];
				}
			}

			continue;
		}
		else if (Token == QLatin1String("1"))
		{
			ReadingHeader = false;
			int ColorCode;
			LineStream >> ColorCode;

			float IncludeMatrix[12];
			for (int TokenIdx = 0; TokenIdx < 12; TokenIdx++)
				LineStream >> IncludeMatrix[TokenIdx];

			swMatrix44 IncludeTransform(swVector4(IncludeMatrix[3], IncludeMatrix[6], IncludeMatrix[9], 0.0f), swVector4(IncludeMatrix[4], IncludeMatrix[7], IncludeMatrix[10], 0.0f),
										swVector4(IncludeMatrix[5], IncludeMatrix[8], IncludeMatrix[11], 0.0f), swVector4(IncludeMatrix[0], IncludeMatrix[1], IncludeMatrix[2], 1.0f));

			QString PartId = LineStream.readAll().trimmed();

			if (PartId.isEmpty())
				continue;

			QByteArray CleanId = PartId.toLatin1().toUpper().replace('\\', '/');

			if (Library->FindPrimitive(CleanId.constData()))
			{
				mFileLines.append(OriginalLine);
				mHasDirectGeometry = true;
			}
			else
			{
				if (!Piece)
					Piece = new swPiece(nullptr);

				if (!CurrentGroups.empty())
					Piece->SetGroup(CurrentGroups[CurrentGroups.size() - 1]);

				swPieceDataReference PieceData = Library->AcquirePieceData(PartId.toLatin1().constData(), Project, true, true);

				const float* Matrix = IncludeTransform.GetFloats();
				const swMatrix44 Transform(swVector4(Matrix[0], Matrix[2], -Matrix[1], 0.0f), swVector4(Matrix[8], Matrix[10], -Matrix[9], 0.0f),
									       swVector4(-Matrix[4], -Matrix[6], Matrix[5], 0.0f), swVector4(Matrix[12], Matrix[14], -Matrix[13], 1.0f));

				Piece->SetFileLine(mFileLines.size());
				Piece->SetPieceData(std::move(PieceData), PartId, false, false);
				Piece->Initialize(Transform, CurrentStep);
				Piece->SetColorCode(ColorCode);
				Piece->VerifyControlPoints(ControlPoints);
				Piece->SetControlPoints(ControlPoints);
				ControlPoints.clear();

				if (Piece->mPieceData->IsModel() && Piece->mPieceData->GetModel()->IncludesModel(this))
				{
					Project->AddLoadWarning(tr("Model '%1': reference to '%2' was ignored because it would create a circular reference.").arg(GetFileName(), PartId));
					delete Piece;
					Piece = nullptr;
					continue;
				}

				AddPiece(Piece);
				Piece = nullptr;
			}
		}
		else
		{
			ReadingHeader = false;
			mFileLines.append(OriginalLine);

			if (Token == QLatin1String("2") || Token == QLatin1String("3") || Token == QLatin1String("4") || Token == QLatin1String("5"))
				mHasDirectGeometry = true;
		}

		FirstLine = false;
	}

	mCurrentStep = CurrentStep;
	CalculateStep(mCurrentStep);

	delete Piece;
	delete Camera;
	delete Light;
}

bool swModel::LoadBinary(swFile* file)
{
	qint32 i, count;
	char id[32];
	quint32 rgb;
	float fv = 0.4f;
	quint8 ch;
	quint16 sh;

	file->Seek(0, SEEK_SET);
	file->ReadBuffer(id, 32);
	sscanf(&id[7], "%f", &fv);

	if (memcmp(id, "LeoCAD ", 7))
		return false;

	if (fv == 0.0f)
	{
		const lconv *loc = localeconv();
		id[8] = loc->decimal_point[0];
		sscanf(&id[7], "%f", &fv);

		if (fv == 0.0f)
			return false;
	}

	if (fv > 0.4f)
		file->ReadFloats(&fv, 1);

	file->ReadU32(&rgb, 1);

	if (fv < 0.6f) // old view
	{
		double eye[3], target[3];
		file->ReadDoubles(eye, 3);
		file->ReadDoubles(target, 3);
	}

	file->Seek(28, SEEK_CUR);
	file->ReadS32(&i, 1);
	mCurrentStep = i;

	if (fv > 0.8f)
		file->ReadU32();//m_nScene

	file->ReadS32(&count, 1);
	swPiecesLibrary* Library = swGetPiecesLibrary();

	const size_t FirstNewPiece = mPieces.size();

	while (count--)
	{
		if (fv > 0.4f)
		{
			swPiece* pPiece = new swPiece(nullptr);
			pPiece->FileLoad(*file, mProject);
			AddPiece(pPiece);
		}
		else
		{
			char name[SW_PIECE_NAME_LENGTH];
			swVector3 pos, rot;
			quint8 color, step, group;

			file->ReadFloats(pos.GetFloats(), 3);
			file->ReadFloats(rot.GetFloats(), 3);
			file->ReadU8(&color, 1);
			file->ReadBuffer(name, 9);
			swstrcat(name, ".dat");
			file->ReadU8(&step, 1);
			file->ReadU8(&group, 1);

			pos *= 25.0f;
			swMatrix44 WorldMatrix = swMultiply(swMatrix44RotationZ(rot[2] * SW_DEGREES_TO_RADIANS), swMultiply(swMatrix44RotationY(rot[1] * SW_DEGREES_TO_RADIANS), swMatrix44RotationX(rot[0] * SW_DEGREES_TO_RADIANS)));
			WorldMatrix.SetTranslation(pos);

			swPiece* pPiece = new swPiece(Library->AcquirePieceData(name, mProject, true, false));

			pPiece->Initialize(WorldMatrix, step);
			pPiece->SetColorCode(swGetColorCodeFromOriginalColor(color));
			AddPiece(pPiece);

//			pPiece->SetGroup((swGroup*)group);
		}
	}

	if (fv >= 0.4f)
	{
		file->ReadBuffer(&ch, 1);
		if (ch == 0xFF) file->ReadU16(&sh, 1); else sh = ch;
		if (sh > 100)
			file->Seek(sh, SEEK_CUR);
		else
		{
			QByteArray Author;
			Author.resize(sh + 1);
			file->ReadBuffer(Author.data(), sh);
			Author[sh] = 0;
			mProperties.mAuthor = QString::fromUtf8(Author);
		}

		file->ReadBuffer(&ch, 1);
		if (ch == 0xFF) file->ReadU16(&sh, 1); else sh = ch;
		if (sh > 100)
			file->Seek(sh, SEEK_CUR);
		else
		{
			QByteArray Description;
			Description.resize(sh + 1);
			file->ReadBuffer(Description.data(), sh);
			Description[sh] = 0;
			mProperties.mDescription = QString::fromUtf8(Description);
		}

		file->ReadBuffer(&ch, 1);
		if (ch == 0xFF && fv < 1.3f) file->ReadU16(&sh, 1); else sh = ch;
		if (sh > 255)
			file->Seek(sh, SEEK_CUR);
		else
		{
			QByteArray Comments;
			Comments.resize(sh + 1);
			file->ReadBuffer(Comments.data(), sh);
			Comments[sh] = 0;
			mProperties.mComments = QString::fromUtf8(Comments);
			mProperties.mComments.replace(QLatin1String("\r\n"), QLatin1String("\n"));
		}
	}

	if (fv >= 0.5f)
	{
		const size_t NumGroups = mGroups.size();

		file->ReadS32(&count, 1);
		for (i = 0; i < count; i++)
			mGroups.emplace_back(new swGroup());

		for (size_t GroupIdx = NumGroups; GroupIdx < mGroups.size(); GroupIdx++)
		{
			swGroup* Group = mGroups[GroupIdx].get();

			if (fv < 1.0f)
			{
				char Name[SW_MAXIMUM_GROUP_NAME + 1];
				file->ReadBuffer(Name, sizeof(Name));
				Group->mName = QString::fromUtf8(Name);
				file->ReadBuffer(&ch, 1);
				Group->mGroup = (swGroup*)-1;
			}
			else
				Group->FileLoad(file);
		}

		for (size_t GroupIdx = NumGroups; GroupIdx < mGroups.size(); GroupIdx++)
		{
			swGroup* Group = mGroups[GroupIdx].get();

			i = (qint32)(quintptr)(Group->mGroup);
			Group->mGroup = nullptr;

			if (i > 0xFFFF || i == -1)
				continue;

			Group->mGroup = mGroups[NumGroups + i].get();
		}

		for (size_t PieceIndex = FirstNewPiece; PieceIndex < mPieces.size(); PieceIndex++)
		{
			swPiece* Piece = mPieces[PieceIndex].get();

			i = (qint32)(quintptr)(Piece->GetGroup());
			Piece->SetGroup(nullptr);

			if (i > 0xFFFF || i == -1)
				continue;

			Piece->SetGroup(mGroups[NumGroups + i].get());
		}

		RemoveEmptyGroups();
	}

	if (fv >= 0.6f)
	{
		if (fv < 1.0f)
			file->Seek(4, SEEK_CUR);
		else
			file->Seek(2, SEEK_CUR);

		file->ReadS32(&count, 1);

		for (i = 0; i < count; i++)
			swCamera::FileLoad(*file);
	}

	if (fv >= 0.7f)
	{
		file->Seek(24, SEEK_CUR);

		if (fv < 1.3f)
		{
			file->ReadU8(&ch, 1);
			if (ch == 0xFF)
				file->ReadU16(&sh, 1);
			sh = ch;
		}
		else
			file->ReadU16(&sh, 1);

		file->Seek(sh, SEEK_CUR); // Background
	}

	if (fv >= 0.8f)
	{
		file->ReadBuffer(&ch, 1);
		file->Seek(ch, SEEK_CUR);
		file->ReadBuffer(&ch, 1);
		file->Seek(ch, SEEK_CUR);
	}

	if (fv > 0.9f)
	{
		file->ReadU32(&rgb, 1);
		mProperties.mAmbientColor[0] = (float)((unsigned char) (rgb))/255;
		mProperties.mAmbientColor[1] = (float)((unsigned char) (((unsigned short) (rgb)) >> 8))/255;
		mProperties.mAmbientColor[2] = (float)((unsigned char) ((rgb) >> 16))/255;

		if (fv < 1.3f)
			file->Seek(23, SEEK_CUR);
		else
			file->Seek(11, SEEK_CUR);
	}

	if (fv > 1.0f)
	{
		file->ReadU32(&rgb, 1);
		file->ReadU32(&rgb, 1);
	}

	CalculateStep(mCurrentStep);

	return true;
}

bool swModel::LoadLDD(const QString& FileData)
{
	std::vector<swPiece*> Pieces;
	std::vector<std::vector<swPiece*>> Groups;

	if (!swImportLXFMLFile(FileData, mProject, Pieces, Groups))
		return false;

	for (swPiece* Piece : Pieces)
		AddPiece(Piece);

	for (const std::vector<swPiece*>& Group : Groups)
	{
		swGroup* NewGroup = AddGroup(tr("Group "), nullptr);
		for (swPiece* Piece : Group)
			Piece->SetGroup(NewGroup);
	}

	CalculateStep(mCurrentStep);

	return true;
}

bool swModel::LoadInventory(const std::vector<swSetInventoryItem>& SetInventory)
{
	swPiecesLibrary* Library = swGetPiecesLibrary();

	for (const swSetInventoryItem& SetInventoryItem : SetInventory)
	{
		const swPieceDataReference PieceData = Library->AcquirePieceData(SetInventoryItem.PartID + ".dat", mProject, true, false);
		int Quantity = SetInventoryItem.Quantity;

		while (Quantity--)
		{
			swPiece* Piece = new swPiece(nullptr);
			Piece->SetPieceData(PieceData, QString(), false, true);
			Piece->Initialize(swMatrix44Identity(), 1);
			Piece->SetColorCode(SetInventoryItem.ColorCode);
			AddPiece(Piece);
		}
	}

	if (mPieces.empty())
		return false;

	std::vector<swPieceDataReference> RequiredPieceData;

	RequiredPieceData.reserve(mPieces.size());

	for (const std::unique_ptr<swPiece>& Piece : mPieces)
		RequiredPieceData.push_back(Piece->mPieceData);

	if (!Library->WaitForPieceData(RequiredPieceData))
		return false;

	auto RoundBounds = [](float& Value)
	{
		Value = ((Value < 0.0f) ? floor((Value - 5.0f) / 10.0f) : ceil((Value + 5.0f) / 10.0f)) * 10.0f;
	};

	constexpr float TargetHeight = 800.0f;
	float CurrentX = 0.0f;
	float CurrentY = 0.0f;
	float ColumnWidth = 0.0f;

	for (const std::unique_ptr<swPiece>& Piece : mPieces)
	{
		swBoundingBox BoundingBox = Piece->mPieceData->GetBoundingBox();
		RoundBounds(BoundingBox.Min.x);
		RoundBounds(BoundingBox.Min.y);
		RoundBounds(BoundingBox.Max.x);
		RoundBounds(BoundingBox.Max.y);

		const float PieceWidth = BoundingBox.Max.x - BoundingBox.Min.x;
		const float PieceHeight = BoundingBox.Max.y - BoundingBox.Min.y;

		if (CurrentY + PieceHeight > TargetHeight)
		{
			CurrentY = 0.0f;
			CurrentX += ColumnWidth;
			ColumnWidth = 0.0f;
		}

		Piece->SetPosition(swVector3(CurrentX + PieceWidth / 2.0f, CurrentY + PieceHeight / 2.0f, 0.0f), 1, false);
		CurrentY += PieceHeight;
		ColumnWidth = qMax(ColumnWidth, PieceWidth);
	}

	CalculateStep(mCurrentStep);

	return true;
}

void swModel::Merge(std::unique_ptr<swModel> Other)
{
	for (std::unique_ptr<swPiece>& Piece : Other->mPieces)
	{
		Piece->SetFileLine(-1);
		AddPiece(Piece.release());
	}

	Other->mPieces.clear();

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

	const std::vector<std::unique_ptr<swPiece>>& PastedPieces = Model->mPieces;
	std::vector<swObject*> SelectedObjects;
	SelectedObjects.reserve(PastedPieces.size());

	for (const std::unique_ptr<swPiece>& Piece : PastedPieces)
	{
		Piece->SetFileLine(-1);

		if (PasteToCurrentStep)
		{
			Piece->SetStepShow(mCurrentStep);
			SelectedObjects.emplace_back(Piece.get());
		}
		else
		{
			if (Piece->GetStepShow() <= mCurrentStep)
				SelectedObjects.emplace_back(Piece.get());
		}
	}

	if (PastedPieces.empty())
		return swResult<void>();

	BeginHistorySequence();
	BeginEditHistory();

	Merge(std::move(Model));

	CalculateStep(mCurrentStep);

	EndEditHistory();

	if (SelectedObjects.size() == 1)
		SetSelectionAndFocus(std::vector<swObject*>(), SelectedObjects.front(), SW_PIECE_SECTION_POSITION, swSelectionMode::Single);
	else
		SetSelectionAndFocus(SelectedObjects, nullptr, 0, swSelectionMode::Single);

	EndHistorySequence(tr("Paste"));

	gMainWindow->UpdateTimeline(false, false);

	return swResult<void>();
}

void swModel::DuplicateSelectedPieces()
{
	if (!AnyPiecesSelected())
	{
		QMessageBox::information(gMainWindow, tr("Duplicate Pieces"), tr("No pieces selected."));
		return;
	}

	BeginHistorySequence();
	BeginEditHistory();

	std::vector<swObject*> NewPieces;
	swPiece* Focus = nullptr;
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

	for (size_t PieceIndex = 0; PieceIndex < mPieces.size(); PieceIndex++)
	{
		const swPiece* Piece = mPieces[PieceIndex].get();

		if (!Piece->IsSelected())
			continue;

		swPiece* NewPiece = new swPiece(*Piece);
		NewPiece->UpdatePosition(mCurrentStep);
		NewPieces.emplace_back(NewPiece);

		if (Piece->IsFocused())
			Focus = NewPiece;

		PieceIndex++;
		AddPiece(std::unique_ptr<swPiece>(NewPiece), PieceIndex);

		swGroup* Group = Piece->GetGroup();
		if (Group)
			NewPiece->SetGroup(GetNewGroup(Group));
	}

	EndEditHistory();

	SetSelectionAndFocus(NewPieces, Focus, SW_PIECE_SECTION_POSITION, swSelectionMode::Single);

	EndHistorySequence(tr("Duplicate"));

	gMainWindow->UpdateTimeline(false, false);
}

void swModel::PaintSelectedPieces()
{
	SetSelectedPiecesColorIndex(gMainWindow->mColorIndex);
}

void swModel::GetScene(swScene* Scene, const swCamera* ViewCamera, bool AllowHighlight, bool AllowFade) const
{
	if (mPieceData)
		mPieceData->AddRenderMesh(*Scene);

	swPiece* FocusPiece = nullptr;

	for (const std::unique_ptr<swPiece>& Piece : mPieces)
	{
		if (Piece->IsSelected() || Piece->IsVisible(mCurrentStep))
		{
			if (Piece->IsFocused())
				FocusPiece = Piece.get();

			const swStep StepShow = Piece->GetStepShow();
			Piece->AddMainModelRenderMeshes(Scene, AllowHighlight && StepShow == mCurrentStep, AllowFade && StepShow < mCurrentStep);
		}
	}

	if (Scene->GetDrawInterface() && !Scene->GetActiveSubmodelInstance())
	{
		if (FocusPiece)
			UpdateTrainTrackConnections(FocusPiece, false);

		for (const std::unique_ptr<swCamera>& Camera : mCameras)
			if (Camera.get() != ViewCamera && Camera->IsVisible())
				Scene->AddInterfaceObject(Camera.get());

		for (const std::unique_ptr<swLight>& Light : mLights)
			if (Light->IsVisible())
				Scene->AddInterfaceObject(Light.get());
	}

	if (Scene->GetDrawInsertPreview())
	{
		for (const swPreviewInsertPieceInformation& PreviewPieceInfoTransform : mPreviewInsertPieceInfo)
			PreviewPieceInfoTransform.PieceData->AddRenderMeshes(Scene, PreviewPieceInfoTransform.Transform, PreviewPieceInfoTransform.ColorIndex, swRenderMeshState::Preview, false);
	}
}

void swModel::AddSubModelRenderMeshes(swScene* Scene, const swMatrix44& WorldMatrix, int DefaultColorIndex, swRenderMeshState RenderMeshState, bool ParentActive) const
{
	const bool Editing = ParentActive || Scene->IsModelEditing(this);

	for (const std::unique_ptr<swPiece>& Piece : mPieces)
	{
		const bool Visible = Editing ? (Piece->IsSelected() || Piece->IsVisible(mCurrentStep)) : Piece->IsVisibleInSubModel();

		if (Visible)
		{
			if (Piece->IsFocused())
				UpdateTrainTrackConnections(Piece.get(), false);

			Piece->AddSubModelRenderMeshes(Scene, WorldMatrix, DefaultColorIndex, RenderMeshState, ParentActive);
		}
	}

	if (Scene->GetDrawInsertPreview())
	{
		for (const swPreviewInsertPieceInformation& PreviewPieceInfoTransform : mPreviewInsertPieceInfo)
			PreviewPieceInfoTransform.PieceData->AddRenderMeshes(Scene, swMultiply(PreviewPieceInfoTransform.Transform, WorldMatrix), PreviewPieceInfoTransform.ColorIndex, swRenderMeshState::Preview, false);
	}
}

QImage swModel::GetStepImage(bool Zoom, int Width, int Height, swStep Step)
{
	swScopedGLContextState GLState;

	WaitForAssets();

	const swResult<QImage> Image = RenderStepImageWithReadyAssets(Zoom, Width, Height, Step, nullptr, swImageBackground::Default);

	if (!Image)
	{
		if (gMainWindow)
			QMessageBox::warning(gMainWindow, tr("Create Step Image"), Image.error());

		return QImage();
	}

	return Image.value();
}

swResult<QImage> swModel::RenderStepImageWithReadyAssets(bool Zoom, int Width, int Height, swStep Step, swCamera* OutputCamera, swImageBackground Background)
{
	swScopedGLContextState GLState;

	const swView* ActiveView = gMainWindow ? gMainWindow->GetActiveView() : nullptr;
	const swStep CurrentStep = mCurrentStep;

	swView View(swViewType::View, this);

	if (Background == swImageBackground::Transparent)
		View.SetBackgroundColorOverride(SW_RGBA(0, 0, 0, 0));

	if (OutputCamera)
		View.SetCamera(OutputCamera, true);
	else if (ActiveView)
		View.SetCamera(ActiveView->GetCamera(), true);

	View.SetOffscreenContext();
	View.MakeCurrent();
	View.SetSize(Width, Height);

	if (!OutputCamera && !ActiveView)
	{
		SetTemporaryStep(GetLastStep());
		View.ZoomExtents(swGeometryBoundsMode::AvailableOnly, true);
	}

	SetTemporaryStep(Step);

	if (Zoom && ActiveView && !OutputCamera)
	{
		View.ZoomExtents(swGeometryBoundsMode::AvailableOnly, true);
	}

	if (!View.BeginRenderToImage(Width, Height))
	{
		SetTemporaryStep(CurrentStep);

		if (!mActive)
			CalculateStep(SW_STEP_MAXIMUM);

		return swUnexpected(tr("Error creating images."));
	}

	View.OnDraw();

	QImage Image = View.GetRenderImage();

	View.EndRenderToImage();

	SetTemporaryStep(CurrentStep);

	if (!mActive)
		CalculateStep(SW_STEP_MAXIMUM);

	if (Background == swImageBackground::Opaque)
		Image = Image.convertToFormat(QImage::Format_RGB32);

	if (Image.isNull())
		return swUnexpected(tr("Error creating images."));

	return Image;
}

swResult<QImage> swModel::GetPartsListImage(int MaxWidth, int RequestedThumbnailSize, swStep Step, quint32 BackgroundColor, quint32 ThumbnailBackgroundColor, QFont Font, QColor TextColor, bool KeepMissingParts, bool ShowCounts, swPartsListSort Sort) const
{
	swScopedGLContextState GLState;

	WaitForAssets();

	swPartsList PartsList;

	if (Step == 0)
		GetPartsList(gDefaultColor, true, false, PartsList);
	else
		GetPartsListForStep(Step, gDefaultColor, PartsList, false);

	if (PartsList.empty())
		return QImage();

	struct swPartsListImage
	{
		QImage Thumbnail;
		const swPieceData* PieceData;
		int ColorIndex;
		int Count;
		QRect Bounds;
		QPoint Position;
	};

	std::vector<swPartsListImage> Images;

	for (const auto& PartIt : PartsList)
	{
		const swPieceData* PieceData = PartIt.first;

		if (!KeepMissingParts && !PieceData->IsModel() && !PieceData->IsProject() && (PieceData->mState != swAssetState::Loaded || !PieceData->GetMesh()))
			continue;

		for (const auto& ColorIt : PartIt.second)
		{
			Images.push_back(swPartsListImage());
			swPartsListImage& Image = Images.back();
			Image.PieceData = PartIt.first;
			Image.ColorIndex = ColorIt.first;
			Image.Count = ColorIt.second;
		}
	}

	if (Images.empty())
	{
		QImage Image(1, 1, QImage::Format_ARGB32);

		if (Image.isNull())
			return swUnexpected(tr("Could not allocate the pieces list image."));

		Image.fill(swQColorFromRGBA(BackgroundColor));
		return Image;
	}

	auto ImageCompare = [Sort](const swPartsListImage& Image1, const swPartsListImage& Image2)
	{
		switch (Sort)
		{
		case swPartsListSort::Color:
			break;

		case swPartsListSort::Name:
			if (int Compare = strcmp(Image1.PieceData->m_strDescription, Image2.PieceData->m_strDescription))
				return Compare < 0;
			return Image1.ColorIndex < Image2.ColorIndex;

		case swPartsListSort::PartId:
			if (int Compare = strcmp(Image1.PieceData->mFileName, Image2.PieceData->mFileName))
				return Compare < 0;
			return Image1.ColorIndex < Image2.ColorIndex;
		}

		if (Image1.ColorIndex != Image2.ColorIndex)
			return Image1.ColorIndex < Image2.ColorIndex;

		return strcmp(Image1.PieceData->m_strDescription, Image2.PieceData->m_strDescription) < 0;
	};

	std::sort(Images.begin(), Images.end(), ImageCompare);

	swView View(swViewType::PartsList, nullptr);
	View.SetOffscreenContext();
	View.MakeCurrent();
	swContext* Context = View.mContext;
	// a thumbnail wider than the list could never be laid out
	const int ThumbnailSize = qBound(16, RequestedThumbnailSize, qMax(qMin(MaxWidth, 512), 16));

	View.SetSize(ThumbnailSize, ThumbnailSize);

	if (!View.BeginRenderToImage(ThumbnailSize, ThumbnailSize))
		return swUnexpected(tr("Could not create the pieces list framebuffer."));

	float OrthoSize = 200.0f;

	swMatrix44 ProjectionMatrix = swMatrix44Orthographic(-OrthoSize, OrthoSize, -OrthoSize, OrthoSize, -5000.0f, 5000.0f);
	const swMatrix44 ViewMatrix = swMatrix44LookAt(swVector3(-100.0f, -100.0f, 75.0f), swVector3(0.0f, 0.0f, 0.0f), swVector3(0.0f, 0.0f, 1.0f));
	const int Viewport[4] = { 0, 0, ThumbnailSize, ThumbnailSize };

	float ExtraPixels = 0.0f;

	for (const swPartsListImage& Image : Images)
	{
		const swPieceData* PieceData = Image.PieceData;
		const swBoundingBox& BoundingBox = PieceData->GetBoundingBox();

		swVector3 Points[8];
		swGetBoxCorners(BoundingBox.Min, BoundingBox.Max, Points);

		for (swVector3& Point : Points)
		{
			Point = swProjectPoint(Point, ViewMatrix, ProjectionMatrix, Viewport);

			ExtraPixels = qMax(ExtraPixels, -Point.x);
			ExtraPixels = qMax(ExtraPixels, Point.x - ThumbnailSize);
			ExtraPixels = qMax(ExtraPixels, -Point.y);
			ExtraPixels = qMax(ExtraPixels, Point.y - ThumbnailSize);
		}
	}

	if (ExtraPixels)
	{
		OrthoSize += ExtraPixels * (2.0f * OrthoSize / ThumbnailSize);
		ProjectionMatrix = swMatrix44Orthographic(-OrthoSize, OrthoSize, -OrthoSize, OrthoSize, -5000.0f, 5000.0f);
	}

	Context->SetViewport(0, 0, ThumbnailSize, ThumbnailSize);
	Context->SetDefaultState();
	Context->SetProjectionMatrix(ProjectionMatrix);

	bool RenderFailed = false;

	for (swPartsListImage& Image : Images)
	{
		View.BindRenderFramebuffer();
		Context->ClearColorAndDepth(swVector4(swVector3FromColor(BackgroundColor), 0.0f));

		swScene Scene;

		const swRenderSettings RenderSettings = swGetPreferences().GetRenderSettings(swRenderTarget::Pieces);
		swShadingMode ShadingMode = RenderSettings.ShadingMode;
		if (ShadingMode == swShadingMode::Wireframe)
			ShadingMode = swShadingMode::Flat;

		Scene.SetShadingMode(ShadingMode);
		Scene.SetLightingStyle(swGetPreferences().mLightingStyle);
		Scene.SetLineSettings(RenderSettings.DrawEdgeLines, RenderSettings.DrawConditionalLines, RenderSettings.LineWidth);
		Scene.SetRequireCompleteAssets(true);
		Scene.SetAllowLOD(false);
		Scene.Begin(ViewMatrix);

		Image.PieceData->AddRenderMeshes(&Scene, swMatrix44Identity(), Image.ColorIndex, swRenderMeshState::Default, true);

		Scene.End();

		Context->SetLineWidth(RenderSettings.LineWidth);
		Scene.Draw(Context, {});

		View.UnbindRenderFramebuffer();
		Image.Thumbnail = View.GetRenderFramebufferImage().convertToFormat(QImage::Format_ARGB32);

		if (Image.Thumbnail.isNull())
		{
			RenderFailed = true;
			break;
		}
	}

	View.EndRenderToImage();
	Context->ClearResources();

	if (RenderFailed)
		return swUnexpected(tr("Could not read the pieces list image."));

	auto CalculateImageBounds = [](swPartsListImage& Image)
	{
		const QImage& Thumbnail = Image.Thumbnail;
		const int Width = Thumbnail.width();
		const int Height = Thumbnail.height();

		int MinX = Width;
		int MinY = Height;
		int MaxX = 0;
		int MaxY = 0;

		for (int x = 0; x < Width; x++)
		{
			for (int y = 0; y < Height; y++)
			{
				if (qAlpha(Thumbnail.pixel(x, y)))
				{
					MinX = qMin(x, MinX);
					MinY = qMin(y, MinY);
					MaxX = qMax(x, MaxX);
					MaxY = qMax(y, MaxY);
				}
			}
		}

		Image.Bounds = QRect(QPoint(MinX, MinY), QPoint(MaxX, MaxY));
	};

	QtConcurrent::blockingMap(Images, CalculateImageBounds);

	if (KeepMissingParts)
	{
		for (swPartsListImage& Image : Images)
			if (Image.Bounds.isEmpty())
				Image.Bounds = QRect(0, 0, qMin(64, ThumbnailSize), qMin(64, ThumbnailSize));
	}
	else
	{
		Images.erase(std::remove_if(Images.begin(), Images.end(), [](const swPartsListImage& Image)
		{
			return Image.Bounds.isEmpty();
		}), Images.end());
	}

	if (Images.empty())
	{
		QImage Image(1, 1, QImage::Format_ARGB32);

		if (Image.isNull())
			return swUnexpected(tr("Could not allocate the pieces list image."));

		Image.fill(swQColorFromRGBA(BackgroundColor));
		return Image;
	}

	QImage DummyImage(16, 16, QImage::Format_ARGB32);

	if (DummyImage.isNull())
		return swUnexpected(tr("Could not allocate the pieces list image."));

	QPainter DummyPainter(&DummyImage);

	DummyPainter.setFont(Font);
	QFontMetrics FontMetrics = DummyPainter.fontMetrics();
	const int Ascent = ShowCounts ? FontMetrics.ascent() : 0;

	int CurrentHeight = 0;
	int ImageWidth = MaxWidth;

	for (const swPartsListImage& Image : Images)
		CurrentHeight = qMax(Image.Bounds.height() + Ascent, CurrentHeight);

	for (;;)
	{
		int CurrentWidth = 0;
		int CurrentX = 0;
		int CurrentY = 0;
		int ColumnWidth = 0;
		constexpr int Spacing = 20;
		int NextHeightIncrease = INT_MAX;

		for (swPartsListImage& Image : Images)
		{
			if (CurrentY + Image.Bounds.height() + Ascent > CurrentHeight)
			{
				const int NeededSpace = Image.Bounds.height() + Ascent - (CurrentHeight - CurrentY);
				NextHeightIncrease = qMin(NeededSpace, NextHeightIncrease);

				CurrentY = 0;
				CurrentX += ColumnWidth + Spacing;
				ColumnWidth = 0;
			}

			Image.Position = QPoint(CurrentX, CurrentY);
			CurrentY += Image.Bounds.height() + Ascent + Spacing;
			CurrentWidth = qMax(CurrentWidth, CurrentX + Image.Bounds.width());
			ColumnWidth = qMax(ColumnWidth, Image.Bounds.width());
		}

		if (CurrentWidth <= MaxWidth)
		{
			ImageWidth = CurrentWidth;
			break;
		}

		CurrentHeight += NextHeightIncrease;
	}

	QImage PainterImage(ImageWidth + 40, CurrentHeight + 40, QImage::Format_ARGB32);

	if (PainterImage.isNull())
		return swUnexpected(tr("Could not allocate the pieces list image."));

	PainterImage.fill(swQColorFromRGBA(BackgroundColor));

	QPainter Painter(&PainterImage);
	Painter.setFont(Font);
	Painter.setPen(TextColor);

	for (const swPartsListImage& Image : Images)
	{
		const QPoint Position = Image.Position + QPoint(20, 20);

		// the thumbnail background holds the piece and its count, it stays inside the spacing between pieces
		if (qAlpha(ThumbnailBackgroundColor))
			Painter.fillRect(QRect(Position, Image.Bounds.size() + QSize(0, Ascent)).adjusted(-6, -6, 6, 6), swQColorFromRGBA(ThumbnailBackgroundColor));

		Painter.drawImage(Position, Image.Thumbnail, Image.Bounds);

		if (ShowCounts)
			Painter.drawText(QPoint(Position.x(), Position.y() + Image.Bounds.height() + Ascent), QString::number(Image.Count) + 'x');
	}

	Painter.end();

	return PainterImage;
}

swResult<void> swModel::SaveStepImages(const QString& BaseName, bool AddStepSuffix, bool Zoom, int Width, int Height, swStep Start, swStep End, swImageBackground Background)
{
	WaitForAssets();

	std::unique_ptr<swView> OutputView;
	swCamera* Camera = nullptr;

	if (!gMainWindow || !gMainWindow->GetActiveView())
	{
		const swStep CurrentStep = mCurrentStep;

		OutputView = std::make_unique<swView>(swViewType::View, this);
		OutputView->SetSize(Width, Height);
		SetTemporaryStep(GetLastStep());
		OutputView->ZoomExtents(swGeometryBoundsMode::AvailableOnly, true);
		SetTemporaryStep(CurrentStep);

		if (!mActive)
			CalculateStep(SW_STEP_MAXIMUM);

		Camera = OutputView->GetCamera();
	}

	for (swStep Step = Start; Step <= End; Step++)
	{
		QString FileName;

		if (AddStepSuffix)
			FileName = BaseName.arg(Step, 2, 10, QLatin1Char('0'));
		else
			FileName = BaseName;

		const swResult<QImage> Image = RenderStepImageWithReadyAssets(Zoom, Width, Height, Step, Camera, Background);

		if (!Image)
			return swUnexpected(Image.error());

		QImageWriter Writer(FileName);

		if (Writer.format().isEmpty())
			Writer.setFormat("png");

		if (!Writer.write(Image.value()))
			return swUnexpected(tr("Error writing to file '%1':\n%2").arg(FileName, Writer.errorString()));
	}

	return swResult<void>();
}

void swModel::RayTest(swObjectRayTest& ObjectRayTest) const
{
	for (const std::unique_ptr<swPiece>& Piece : mPieces)
		if ((Piece->IsSelected() || Piece->IsVisible(mCurrentStep)) && (!ObjectRayTest.IgnoreSelected || !Piece->IsSelected()))
			Piece->RayTest(ObjectRayTest);

	if (ObjectRayTest.PiecesOnly)
		return;

	for (const std::unique_ptr<swCamera>& Camera : mCameras)
		if (Camera.get() != ObjectRayTest.ViewCamera && Camera->IsVisible() && (!ObjectRayTest.IgnoreSelected || !Camera->IsSelected()))
			Camera->RayTest(ObjectRayTest);

	for (const std::unique_ptr<swLight>& Light : mLights)
		if (Light->IsVisible() && (!ObjectRayTest.IgnoreSelected || !Light->IsSelected()))
			Light->RayTest(ObjectRayTest);
}

void swModel::BoxTest(swObjectBoxTest& ObjectBoxTest) const
{
	for (const std::unique_ptr<swPiece>& Piece : mPieces)
		if (Piece->IsSelected() || Piece->IsVisible(mCurrentStep))
			Piece->BoxTest(ObjectBoxTest);

	for (const std::unique_ptr<swCamera>& Camera : mCameras)
		if (Camera.get() != ObjectBoxTest.ViewCamera && Camera->IsVisible())
			Camera->BoxTest(ObjectBoxTest);

	for (const std::unique_ptr<swLight>& Light : mLights)
		if (Light->IsVisible())
			Light->BoxTest(ObjectBoxTest);
}

bool swModel::SubModelMinIntersectDist(const swVector3& WorldStart, const swVector3& WorldEnd, float& MinDistance, swPieceRayTest& PieceRayTest) const
{
	bool MinIntersect = false;

	for (const std::unique_ptr<swPiece>& Piece : mPieces)
	{
		const swMatrix44 InverseWorldMatrix = swMatrix44AffineInverse(Piece->mModelWorld);
		const swVector3 Start = swMultiply31(WorldStart, InverseWorldMatrix);
		const swVector3 End = swMultiply31(WorldEnd, InverseWorldMatrix);

		if (Piece->IsVisibleInSubModel())
		{
			if (Piece->MinIntersectDist(Start, End, MinDistance, PieceRayTest))
			{
				MinIntersect = true;
				PieceRayTest.Transform = swMultiply(PieceRayTest.Transform, Piece->mModelWorld);
			}
		}
	}

	return MinIntersect;
}

bool swModel::SubModelBoxTest(const swVector4 Planes[6]) const
{
	for (const std::unique_ptr<swPiece>& Piece : mPieces)
		if (Piece->IsVisibleInSubModel() && Piece->IntersectsPlanes(Piece->mModelWorld, Planes))
			return true;

	return false;
}

void swModel::SubModelCompareBoundingBox(const swMatrix44& WorldMatrix, swVector3& Min, swVector3& Max) const
{
	const swMesh* Mesh = mPieceData ? mPieceData->GetDisplayMesh() : nullptr;

	if (Mesh)
	{
		swVector3 Box[8];
		swGetBoxCorners(Mesh->mBoundingBox, Box);

		for (const swVector3& Corner : Box)
		{
			const swVector3 Point = swMultiply31(Corner, WorldMatrix);
			Min = swMinimum(Min, Point);
			Max = swMaximum(Max, Point);
		}
	}

	for (const std::unique_ptr<swPiece>& Piece : mPieces)
		if (Piece->IsVisibleInSubModel())
			Piece->SubModelCompareBoundingBox(WorldMatrix, Min, Max);
}

void swModel::SubModelAddBoundingBoxPoints(const swMatrix44& WorldMatrix, std::vector<swVector3>& Points, swGeometryBoundsMode Mode) const
{
	const swMesh* Mesh = mPieceData ? mPieceData->GetDisplayMesh() : nullptr;

	if (Mode == swGeometryBoundsMode::AvailableOnly && mPieceData && mPieceData->mState != swAssetState::Loaded)
		Mesh = nullptr;

	if (Mesh)
	{
		swVector3 Box[8];
		swGetBoxCorners(Mesh->mBoundingBox, Box);

		for (const swVector3& Corner : Box)
			Points.emplace_back(swMultiply31(Corner, WorldMatrix));
	}

	for (const std::unique_ptr<swPiece>& Piece : mPieces)
		if (Piece->IsVisibleInSubModel())
			Piece->SubModelAddBoundingBoxPoints(WorldMatrix, Points, Mode);
}

void swModel::AddSelectionHistory(std::function<void()> Callback)
{
	std::unique_ptr<swModelHistorySelect> ModelHistorySelect = std::make_unique<swModelHistorySelect>();

	ModelHistorySelect->SaveStartState(this);

	Callback();

	ModelHistorySelect->SaveEndState(this);

	if (ModelHistorySelect->StateChanged())
		mHistorySequence.emplace_back(std::move(ModelHistorySelect));
}

void swModel::ClearSelection()
{
	AddSelectionHistory([this](){ DeselectAllObjects(); });
}

void swModel::SetFocus(swObject* FocusObject, uint32_t FocusSection, swSelectionMode SelectionMode)
{
	AddSelectionHistory([this, FocusObject, FocusSection, SelectionMode](){ SetFocusedObject(FocusObject, FocusSection, SelectionMode); });
}

void swModel::SetSelectionAndFocus(const std::vector<swObject*>& Objects, swObject* FocusObject, uint32_t FocusSection, swSelectionMode SelectionMode)
{
	AddSelectionHistory([this, &Objects, FocusObject, FocusSection, SelectionMode]()
	{
		DeselectAllObjects();
		SetObjectsSelected(Objects, true);
		SetFocusedObject(FocusObject, FocusSection, SelectionMode);
	});
}

void swModel::SelectAllPieces()
{
	AddSelectionHistory([this]()
	{
		for (const std::unique_ptr<swPiece>& Piece : mPieces)
			if (Piece->IsVisible(mCurrentStep))
				Piece->SetSelected(true);
	});
}

void swModel::InvertPieceSelection()
{
	AddSelectionHistory([this]()
	{
		for (const std::unique_ptr<swPiece>& Piece : mPieces)
			if (Piece->IsSelected() || Piece->IsVisible(mCurrentStep))
				Piece->SetSelected(!Piece->IsSelected());
	});
}

void swModel::AddToSelection(const std::vector<swObject*>& Objects)
{
	AddSelectionHistory([this, &Objects](){ SetObjectsSelected(Objects, true); });
}

void swModel::RemoveFromSelection(const std::vector<swObject*>& Objects)
{
	AddSelectionHistory([this, &Objects](){ SetObjectsSelected(Objects, false); });
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

bool swModel::AnyPiecesSelected() const
{
	for (const std::unique_ptr<swPiece>& Piece : mPieces)
		if (Piece->IsSelected())
			return true;

	return false;
}

bool swModel::AnyObjectsSelected() const
{
	for (const std::unique_ptr<swPiece>& Piece : mPieces)
		if (Piece->IsSelected())
			return true;

	for (const std::unique_ptr<swCamera>& Camera : mCameras)
		if (Camera->IsSelected())
			return true;

	for (const std::unique_ptr<swLight>& Light : mLights)
		if (Light->IsSelected())
			return true;

	return false;
}

swObject* swModel::GetFocusObject() const
{
	for (const std::unique_ptr<swPiece>& Piece : mPieces)
		if (Piece->IsFocused())
			return Piece.get();

	for (const std::unique_ptr<swCamera>& Camera : mCameras)
		if (Camera->IsFocused())
			return Camera.get();

	for (const std::unique_ptr<swLight>& Light : mLights)
		if (Light->IsFocused())
			return Light.get();

	return nullptr;
}

swModel* swModel::GetFirstSelectedSubmodel() const
{
	for (const std::unique_ptr<swPiece>& Piece : mPieces)
		if (Piece->IsSelected() && Piece->mPieceData->IsModel())
			return Piece->mPieceData->GetModel();

	return nullptr;
}

void swModel::GetSubModels(std::set<swModel*>& SubModels) const
{
	for (const std::unique_ptr<swPiece>& Piece : mPieces)
	{
		if (Piece->mPieceData->IsModel())
		{
			swModel* SubModel = Piece->mPieceData->GetModel();

			SubModels.insert(SubModel);
		}
	}
}

bool swModel::GetMoveRotateTransform(swVector3& Center, swMatrix33& RelativeRotation) const
{
	const bool Relative = gMainWindow->GetRelativeTransform();
	int NumSelected = 0;

	Center = swVector3(0.0f, 0.0f, 0.0f);
	RelativeRotation = swMatrix33Identity();

	for (const std::unique_ptr<swPiece>& Piece : mPieces)
	{
		if (!Piece->IsSelected())
			continue;

		if (Piece->IsFocused() && Relative)
		{
			Center = Piece->GetRotationCenter();
			RelativeRotation = Piece->GetRelativeRotation();
			return true;
		}

		Center += Piece->mModelWorld.GetTranslation();
		NumSelected++;
	}

	for (const std::unique_ptr<swCamera>& Camera : mCameras)
	{
		if (!Camera->IsSelected())
			continue;

		if (Camera->IsFocused() && Relative)
		{
			Center = Camera->GetSectionPosition(Camera->GetFocusSection());
//			RelativeRotation = Piece->GetRelativeRotation();
			return true;
		}

		Center += Camera->GetSectionPosition(SW_CAMERA_SECTION_POSITION);
		Center += Camera->GetSectionPosition(SW_CAMERA_SECTION_TARGET);
		Center += Camera->GetSectionPosition(SW_CAMERA_SECTION_UP_VECTOR);
		NumSelected += 3;
	}

	for (const std::unique_ptr<swLight>& Light : mLights)
	{
		if (!Light->IsSelected())
			continue;

		if (Light->IsFocused())
		{
			Center = Light->GetRotationCenter();

			if (Relative)
				RelativeRotation = Light->GetRelativeRotation();

			return true;
		}

		Center += Light->GetSectionPosition(SW_LIGHT_SECTION_POSITION);
		NumSelected++;
	}

	if (NumSelected)
	{
		Center /= NumSelected;
		return true;
	}

	return false;
}

bool swModel::CanRotateSelection() const
{
	int Flags;
	std::vector<swObject*> Selection;
	swObject* Focus;

	GetSelectionInformation(&Flags, Selection, &Focus);

	if (Flags & SW_SELECTION_PIECE)
	{
		if ((Flags & (SW_SELECTION_CAMERA | SW_SELECTION_LIGHT)) == 0)
			return true;
	}

	if ((Flags & (SW_SELECTION_PIECE | SW_SELECTION_CAMERA)) == 0)
	{
		if (Focus && Focus->IsLight())
		{
			swLight* Light = (swLight*)Focus;

			return (Light->GetAllowedTransforms() & SW_OBJECT_TRANSFORM_ROTATE_XYZ) != 0;
		}
	}

	return false;
}

bool swModel::GetPieceFocusOrSelectionCenter(swVector3& Center) const
{
	swVector3 Min(FLT_MAX, FLT_MAX, FLT_MAX), Max(-FLT_MAX, -FLT_MAX, -FLT_MAX);
	swPiece* Selected = nullptr;
	int NumSelected = 0;

	for (const std::unique_ptr<swPiece>& Piece : mPieces)
	{
		if (Piece->IsFocused())
		{
			Center = Piece->mModelWorld.GetTranslation();
			return true;
		}

		if (Piece->IsSelected())
		{
			Piece->CompareBoundingBox(Min, Max);
			Selected = Piece.get();
			NumSelected++;
		}
	}

	if (NumSelected == 1)
		Center = Selected->mModelWorld.GetTranslation();
	else if (NumSelected)
		Center = (Min + Max) / 2.0f;
	else
		Center = swVector3(0.0f, 0.0f, 0.0f);

	return NumSelected != 0;
}

swVector3 swModel::GetSelectionOrModelCenter() const
{
	swVector3 Center;

	if (!GetSelectionCenter(Center))
	{
		swVector3 Min(FLT_MAX, FLT_MAX, FLT_MAX), Max(-FLT_MAX, -FLT_MAX, -FLT_MAX);

		if (GetVisiblePiecesBoundingBox(Min, Max))
			Center = (Min + Max) / 2.0f;
		else
			Center = swVector3(0.0f, 0.0f, 0.0f);
	}

	return Center;
}

bool swModel::GetFocusPosition(swVector3& Position) const
{
	const swObject* FocusObject = GetFocusObject();

	if (FocusObject)
	{
		Position = FocusObject->GetSectionPosition(FocusObject->GetFocusSection());
		return true;
	}
	else
	{
		Position = swVector3(0.0f, 0.0f, 0.0f);
		return false;
	}
}

bool swModel::GetSelectionCenter(swVector3& Center) const
{
	swVector3 Min(FLT_MAX, FLT_MAX, FLT_MAX), Max(-FLT_MAX, -FLT_MAX, -FLT_MAX);
	swPiece* SelectedPiece = nullptr;
	bool SinglePieceSelected = true;
	bool Selected = false;

	for (const std::unique_ptr<swPiece>& Piece : mPieces)
	{
		if (Piece->IsSelected())
		{
			Piece->CompareBoundingBox(Min, Max);
			Selected = true;

			if (!SelectedPiece)
				SelectedPiece = Piece.get();
			else
				SinglePieceSelected = false;
		}
	}

	for (const std::unique_ptr<swCamera>& Camera : mCameras)
	{
		if (Camera->IsSelected())
		{
			Camera->CompareBoundingBox(Min, Max);
			Selected = true;
			SinglePieceSelected = false;
		}
	}

	for (const std::unique_ptr<swLight>& Light : mLights)
	{
		if (Light->IsSelected())
		{
			Light->CompareBoundingBox(Min, Max);
			Selected = true;
			SinglePieceSelected = false;
		}
	}

	if (SelectedPiece && SinglePieceSelected)
		Center = SelectedPiece->GetSectionPosition(SW_PIECE_SECTION_POSITION);
	else if (Selected)
		Center = (Min + Max) / 2.0f;
	else
		Center = swVector3(0.0f, 0.0f, 0.0f);

	return Selected;
}

swBoundingBox swModel::GetAllPiecesBoundingBox() const
{
	swBoundingBox Box;

	if (!mPieces.empty())
	{
		Box.Min = swVector3(FLT_MAX, FLT_MAX, FLT_MAX);
		Box.Max = swVector3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

		for (const std::unique_ptr<swPiece>& Piece : mPieces)
			Piece->CompareBoundingBox(Box.Min, Box.Max);
	}
	else
		Box.Min = Box.Max = swVector3(0.0f, 0.0f, 0.0f);

	return Box;
}

bool swModel::GetVisiblePiecesBoundingBox(swVector3& Min, swVector3& Max) const
{
	bool Valid = false;
	Min = swVector3(FLT_MAX, FLT_MAX, FLT_MAX);
	Max = swVector3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	for (const std::unique_ptr<swPiece>& Piece : mPieces)
	{
		if (Piece->IsSelected() || Piece->IsVisible(mCurrentStep))
		{
			Piece->CompareBoundingBox(Min, Max);
			Valid = true;
		}
	}

	return Valid;
}

std::vector<swVector3> swModel::GetPiecesBoundingBoxPoints(swGeometryBoundsMode Mode) const
{
	std::vector<swVector3> Points;
	const swMesh* Mesh = mPieceData ? mPieceData->GetDisplayMesh() : nullptr;

	if (Mode == swGeometryBoundsMode::AvailableOnly && mPieceData && mPieceData->mState != swAssetState::Loaded)
		Mesh = nullptr;

	if (Mesh)
	{
		swVector3 Box[8];
		swGetBoxCorners(Mesh->mBoundingBox, Box);

		for (const swVector3& Point : Box)
			Points.emplace_back(Point);
	}

	for (const std::unique_ptr<swPiece>& Piece : mPieces)
		if (Piece->IsSelected() || Piece->IsVisible(mCurrentStep))
			Piece->SubModelAddBoundingBoxPoints(swMatrix44Identity(), Points, Mode);

	return Points;
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

void swModel::GetSelectionInformation(int* Flags, std::vector<swObject*>& Selection, swObject** Focus) const
{
	*Flags = 0;
	*Focus = nullptr;

	if (mPieces.empty())
		*Flags |= SW_SELECTION_NO_PIECES;
	else
	{
		swGroup* Group = nullptr;
		bool First = true;

		for (const std::unique_ptr<swPiece>& Piece : mPieces)
		{
			if (Piece->IsSelected())
			{
				Selection.emplace_back(Piece.get());

				if (Piece->IsFocused())
					*Focus = Piece.get();

				if (Piece->mPieceData->IsModel())
					*Flags |= SW_SELECTION_MODEL_SELECTED;

				if (Piece->IsHidden())
					*Flags |= SW_SELECTION_HIDDEN | SW_SELECTION_HIDDEN_SELECTED;
				else
					*Flags |= SW_SELECTION_VISIBLE_SELECTED;

				*Flags |= SW_SELECTION_PIECE | SW_SELECTION_SELECTED;

				if (Piece->AreTrainTrackConnectionsVisible())
					*Flags |= SW_SELECTION_TRAIN_TRACK_VISIBLE;

				if (Piece->CanAddControlPoint())
					*Flags |= SW_SELECTION_CAN_ADD_CONTROL_POINT;

				if (Piece->CanRemoveControlPoint())
					*Flags |= SW_SELECTION_CAN_REMOVE_CONTROL_POINT;

				if (Piece->GetGroup() != nullptr)
				{
					*Flags |= SW_SELECTION_GROUPED;
					if (Piece->IsFocused())
						*Flags |= SW_SELECTION_FOCUS_GROUPED;
				}

				if (First)
				{
					Group = Piece->GetGroup();
					First = false;
				}
				else
				{
					if (Group != Piece->GetGroup())
						*Flags |= SW_SELECTION_CAN_GROUP;
					else
						if (Group == nullptr)
							*Flags |= SW_SELECTION_CAN_GROUP;
				}
			}
			else
			{
				*Flags |= SW_SELECTION_UNSELECTED;

				if (Piece->IsHidden())
					*Flags |= SW_SELECTION_HIDDEN;
			}
		}
	}

	for (const std::unique_ptr<swCamera>& Camera : mCameras)
	{
		if (Camera->IsSelected())
		{
			Selection.emplace_back(Camera.get());
			*Flags |= SW_SELECTION_SELECTED | SW_SELECTION_CAMERA;

			if (Camera->IsFocused())
				*Focus = Camera.get();
		}
	}

	for (const std::unique_ptr<swLight>& Light : mLights)
	{
		if (Light->IsSelected())
		{
			Selection.emplace_back(Light.get());
			*Flags |= SW_SELECTION_SELECTED | SW_SELECTION_LIGHT;

			if (Light->IsFocused())
				*Focus = Light.get();
		}
	}
}

void swModel::ClearSelectionAction()
{
	BeginHistorySequence();
	ClearSelection();
	EndHistorySequence(tr("Selection"));
}

void swModel::SetSelectionAndFocusAction(const std::vector<swObject*>& Objects, swObject* Focus, quint32 Section, swSelectionMode SelectionMode)
{
	BeginHistorySequence();
	SetSelectionAndFocus(Objects, Focus, Section, SelectionMode);
	EndHistorySequence(tr("Selection"));
}

void swModel::FocusOrDeselectObjectAction(swObject* Object, uint32_t Section, swSelectionMode SelectionMode)
{
	BeginHistorySequence();

	if (Object)
	{
		if (Object->IsFocused(Section) || (Object->IsPiece() && Object->IsSelected()))
			RemoveFromSelection({ Object });
		else
			SetFocus(Object, Section, SelectionMode);
	}
	else
	{
		SetFocus(nullptr, 0, SelectionMode);
	}

	EndHistorySequence(tr("Selection"));
}

void swModel::SelectAllPiecesAction()
{
	BeginHistorySequence();
	SelectAllPieces();
	EndHistorySequence(tr("Selection"));
}

void swModel::InvertPieceSelectionAction()
{
	BeginHistorySequence();
	InvertPieceSelection();
	EndHistorySequence(tr("Selection"));
}

void swModel::AddToSelectionAction(const std::vector<swObject*>& Objects)
{
	BeginHistorySequence();
	AddToSelection(Objects);
	EndHistorySequence(tr("Selection"));
}

void swModel::RemoveFromSelectionAction(const std::vector<swObject*>& Objects)
{
	BeginHistorySequence();
	RemoveFromSelection(Objects);
	EndHistorySequence(tr("Selection"));
}

std::vector<swObject*> swModel::GetSelectionModePieces(swSelectionMode SelectionMode, const swPiece* SelectedPiece) const
{
	const swPieceData* PieceData = SelectedPiece->mPieceData.Get();
	const int ColorIndex = SelectedPiece->GetColorIndex();
	std::vector<swObject*> Pieces;

	switch (SelectionMode)
	{
	case swSelectionMode::Single:
		break;

	case swSelectionMode::Piece:
		for (const std::unique_ptr<swPiece>& Piece : mPieces)
			if (Piece->IsVisible(mCurrentStep) && Piece->mPieceData == PieceData && Piece.get() != SelectedPiece)
				Pieces.emplace_back(Piece.get());
		break;

	case swSelectionMode::Color:
		for (const std::unique_ptr<swPiece>& Piece : mPieces)
			if (Piece->IsVisible(mCurrentStep) && Piece->GetColorIndex() == ColorIndex && Piece.get() != SelectedPiece)
				Pieces.emplace_back(Piece.get());
		break;

	case swSelectionMode::PieceColor:
		for (const std::unique_ptr<swPiece>& Piece : mPieces)
			if (Piece->IsVisible(mCurrentStep) && Piece->mPieceData == PieceData && Piece->GetColorIndex() == ColorIndex && Piece.get() != SelectedPiece)
				Pieces.emplace_back(Piece.get());
		break;
	}

	return Pieces;
}

void swModel::DeselectAllObjects()
{
	for (const std::unique_ptr<swPiece>& Piece : mPieces)
		Piece->SetSelected(false);

	for (const std::unique_ptr<swCamera>& Camera : mCameras)
		Camera->SetSelected(false);

	for (const std::unique_ptr<swLight>& Light : mLights)
		Light->SetSelected(false);
}

void swModel::SetFocusedObject(swObject* FocusObject, uint32_t FocusSection, swSelectionMode SelectionMode)
{
	swObject* PreviousFocus = GetFocusObject();

	if (PreviousFocus)
		PreviousFocus->SetFocused(PreviousFocus->GetFocusSection(), false);

	if (FocusObject)
	{
		swPiece* Piece = dynamic_cast<swPiece*>(FocusObject);

		if (Piece)
		{
			if (!Piece->IsSelected())
				SetObjectsSelected({ Piece }, true);

			std::vector<swObject*> Pieces = GetSelectionModePieces(SelectionMode, Piece);

			SetObjectsSelected(Pieces, true);
		}

		FocusObject->SetFocused(FocusSection, true);
	}
}

void swModel::SetObjectsSelected(const std::vector<swObject*>& Objects, bool Selected)
{
	for (swObject* Object : Objects)
	{
		if (Object->IsSelected() == Selected)
			continue;

		Object->SetSelected(Selected);

		if (Object->IsPiece())
		{
			swPiece* Piece = dynamic_cast<swPiece*>(Object);

			SelectGroup(Piece->GetTopGroup(), Selected);
		}
	}
}

void swModel::SelectGroup(swGroup* TopGroup, bool Select)
{
	if (!TopGroup)
		return;

	for (const std::unique_ptr<swPiece>& Piece : mPieces)
		if (!Piece->IsSelected() && (Piece->GetTopGroup() == TopGroup))
			Piece->SetSelected(Select);
}

void swModel::HideSelectedPieces()
{
	BeginHistorySequence();
	BeginEditHistory();

	bool Modified = false;

	for (const std::unique_ptr<swPiece>& Piece : mPieces)
	{
		if (Piece->IsSelected() && !Piece->IsHidden())
		{
			Piece->SetHidden(true);

			Modified = true;
		}
	}

	if (!Modified)
	{
		DiscardHistorySequence();

		return;
	}

	EndEditHistory();

	ClearSelection();

	EndHistorySequence(tr("Hide Pieces"));

	gMainWindow->UpdateTimeline(false, true);
	gMainWindow->UpdateSelectedObjects(true);
}

void swModel::HideUnselectedPieces()
{
	BeginHistorySequence();
	BeginEditHistory();

	bool Modified = false;

	for (const std::unique_ptr<swPiece>& Piece : mPieces)
	{
		if (!Piece->IsSelected() && !Piece->IsHidden())
		{
			Piece->SetHidden(true);

			Modified = true;
		}
	}

	if (!Modified)
	{
		DiscardHistorySequence();

		return;
	}

	EndEditHistory();
	EndHistorySequence(tr("Hide Pieces"));

	gMainWindow->UpdateTimeline(false, true);
	gMainWindow->UpdateSelectedObjects(true);
}

void swModel::UnhideSelectedPieces()
{
	BeginHistorySequence();
	BeginEditHistory();

	bool Modified = false;

	for (const std::unique_ptr<swPiece>& Piece : mPieces)
	{
		if (Piece->IsSelected() && Piece->IsHidden())
		{
			Piece->SetHidden(false);

			Modified = true;
		}
	}

	if (!Modified)
	{
		DiscardHistorySequence();

		return;
	}

	EndEditHistory();
	EndHistorySequence(tr("Unhide Pieces"));

	gMainWindow->UpdateTimeline(false, true);
	gMainWindow->UpdateSelectedObjects(true);
}

void swModel::UnhideAllPieces()
{
	BeginHistorySequence();
	BeginEditHistory();

	bool Modified = false;

	for (const std::unique_ptr<swPiece>& Piece : mPieces)
	{
		if (Piece->IsHidden())
		{
			Piece->SetHidden(false);

			Modified = true;
		}
	}

	if (!Modified)
	{
		DiscardHistorySequence();

		return;
	}

	EndEditHistory();
	EndHistorySequence(tr("Unhide Pieces"));

	gMainWindow->UpdateTimeline(false, true);
	gMainWindow->UpdateSelectedObjects(true);
}

void swModel::FindReplacePiece(bool SearchForward, bool FindAll, bool Replace)
{
	if (mPieces.empty())
		return;

	const swFindReplaceParameters& Params = swView::GetFindReplaceParams();

	const bool ReplacePieceData = Replace && Params.ReplacePieceData;
	const bool ReplaceColor = Replace && swGetColorCode(Params.ReplaceColorIndex) != SW_COLOR_NO_COLOR;

	// Check if we are supposed to actually replace something
	const bool Replacing = (ReplaceColor || ReplacePieceData);

	auto PieceMatches = [&Params](const swPiece* Piece)
	{
		if (Params.FindPieceData && Params.FindPieceData != Piece->mPieceData)
			return false;

		if (!Params.FindString.isEmpty() && !swstrcasestr(Piece->mPieceData->m_strDescription, Params.FindString.toLatin1()))
			return false;

		return (swGetColorCode(Params.FindColorIndex) == SW_COLOR_NO_COLOR) || (Piece->GetColorIndex() == Params.FindColorIndex);
	};

	auto ReplacePiece = [&Params, ReplacePieceData, ReplaceColor](swPiece* Piece)
	{
		if (ReplaceColor)
			Piece->SetColorIndex(Params.ReplaceColorIndex);

		if (ReplacePieceData)
			Piece->SetPieceData(swPieceDataReference(Params.ReplacePieceData), QString(), true, true);
	};

	size_t StartIndex = mPieces.size() - 1;
	int ReplacedCount = 0;

	BeginHistorySequence();
	BeginEditHistory();

	if (!FindAll)
	{
		// We have to find the currently focused piece, in order to find next/prev match and (optionally) to replace it
		swPiece* const FocusedPiece = dynamic_cast<swPiece*>(GetFocusObject());

		if (FocusedPiece)
		{
			for (size_t PieceIndex = 0; PieceIndex < mPieces.size(); PieceIndex++)
			{
				if (FocusedPiece == mPieces[PieceIndex].get())
				{
					StartIndex = PieceIndex;
					break;
				}
			}

			if (Replacing && PieceMatches(FocusedPiece))
			{
				ReplacePiece(FocusedPiece);
				ReplacedCount++;
			}
		}
	}

	size_t CurrentIndex = StartIndex;
	swPiece* Focus = nullptr;
	std::vector<swObject*> Selection;

	for (;;)
	{
		if (SearchForward)
		{
			CurrentIndex++;

			if (CurrentIndex >= mPieces.size())
				CurrentIndex = 0;
		}
		else
		{
			if (CurrentIndex == 0)
				CurrentIndex = mPieces.size();

			CurrentIndex--;
		}

		swPiece* Current = mPieces[CurrentIndex].get();

		if (Current->IsVisible(mCurrentStep) && PieceMatches(Current))
		{
			if (FindAll)
			{
				Selection.emplace_back(Current);
				if (Replacing)
				{
					ReplacePiece(Current);
					ReplacedCount++;
				}
			}
			else
			{
				Focus = Current;
				break;
			}
		}

		if (CurrentIndex == StartIndex)
			break;
	}

	EndEditHistory();

	if (FindAll)
		SetSelectionAndFocus(Selection, nullptr, 0, swSelectionMode::Single);
	else
		SetSelectionAndFocus(std::vector<swObject*>(), Focus, SW_PIECE_SECTION_POSITION, swSelectionMode::Single);

	if (ReplacedCount)
	{
		EndHistorySequence(tr("Replace Pieces", nullptr, ReplacedCount));

		gMainWindow->UpdateSelectedObjects(false);
		gMainWindow->UpdateTimeline(false, true);
	}
	else
		EndHistorySequence(tr("Selection"));
}

void swModel::UndoAction()
{
	if (mUndoHistory.empty())
		return;

	std::unique_ptr<swModelHistoryEntry> Undo = std::move(mUndoHistory.front());

	RunHistorySequence(Undo->HistorySequence, false);

	mUndoHistory.erase(mUndoHistory.begin());
	mRedoHistory.insert(mRedoHistory.begin(), std::move(Undo));

	gMainWindow->UpdateModified(mProject ? mProject->IsModified() : IsModified());
	gMainWindow->UpdateUndoRedo(!mUndoHistory.empty() ? mUndoHistory.front()->Description : nullptr, !mRedoHistory.empty() ? mRedoHistory.front()->Description : nullptr);
}

void swModel::RedoAction()
{
	if (mRedoHistory.empty())
		return;

	std::unique_ptr<swModelHistoryEntry> Redo = std::move(mRedoHistory.front());

	RunHistorySequence(Redo->HistorySequence, true);

	mRedoHistory.erase(mRedoHistory.begin());
	mUndoHistory.insert(mUndoHistory.begin(), std::move(Redo));

	gMainWindow->UpdateModified(mProject ? mProject->IsModified() : IsModified());
	gMainWindow->UpdateUndoRedo(!mUndoHistory.empty() ? mUndoHistory.front()->Description : nullptr, !mRedoHistory.empty() ? mRedoHistory.front()->Description : nullptr);
}

void swModel::BeginMouseTool(swTool Tool, swView* View)
{
	switch (Tool)
	{
		case swTool::Insert:
		case swTool::PointLight:
		case swTool::SpotLight:
		case swTool::DirectionalLight:
		case swTool::AreaLight:
		case swTool::Camera:
		case swTool::Select:
			break;

		case swTool::Move:
		case swTool::Rotate:
			BeginHistorySequence();
			BeginEditHistory();
			break;

		case swTool::Eraser:
		case swTool::Paint:
		case swTool::ColorPicker:
			break;

		case swTool::Pan:
		case swTool::Zoom:
		case swTool::RotateView:
		case swTool::Roll:
			if (!View->GetCamera()->IsSimple())
			{
				BeginHistorySequence();
				BeginEditHistory();
			}
			break;

		case swTool::ZoomRegion:
			break;

		case swTool::Count:
			break;
	}

	mMouseToolDistance = swVector3(0.0f, 0.0f, 0.0f);
	mMouseToolMoveStartPosition = swVector3(0.0f, 0.0f, 0.0f);
	mMouseToolMoveEndPosition = swVector3(0.0f, 0.0f, 0.0f);
	mMouseToolMovePositionValid = false;
	mMouseToolMoveUsesFocus = false;

	if (Tool == swTool::Move)
	{
		swObject* Focus = GetFocusObject();

		if (Focus && Focus->GetFocusSection() != SW_OBJECT_SECTION_INVALID)
		{
			mMouseToolMoveStartPosition = Focus->GetSectionPosition(Focus->GetFocusSection());
			mMouseToolMovePositionValid = true;
			mMouseToolMoveUsesFocus = true;
		}
		else
		{
			swMatrix33 RelativeRotation;
			mMouseToolMovePositionValid = GetMoveRotateTransform(mMouseToolMoveStartPosition, RelativeRotation);
		}

		mMouseToolMoveEndPosition = mMouseToolMoveStartPosition;
	}

	mMouseToolFirstMove = true;
}

void swModel::EndMouseTool(swTool Tool, swView* View, bool Accept)
{
	if (!Accept)
	{
		RevertHistorySequence();
		return;
	}

	const swCamera* Camera = View->GetCamera();

	switch (Tool)
	{
	case swTool::Insert:
	case swTool::PointLight:
	case swTool::SpotLight:
	case swTool::DirectionalLight:
	case swTool::AreaLight:
	case swTool::Camera:
	case swTool::Select:
		break;

	case swTool::Move:
		EndEditHistory();
		EndHistorySequence(tr("Move"));
		break;

	case swTool::Rotate:
		EndEditHistory();
		EndHistorySequence(tr("Rotate"));
		break;

	case swTool::Eraser:
	case swTool::Paint:
	case swTool::ColorPicker:
		break;

	case swTool::Zoom:
		if (!Camera->IsSimple())
		{
			EndEditHistory();
			EndHistorySequence(tr("Zoom"));
		}
		break;

	case swTool::Pan:
		if (!Camera->IsSimple())
		{
			EndEditHistory();
			EndHistorySequence(tr("Pan"));
		}
		break;

	case swTool::RotateView:
		if (!Camera->IsSimple())
		{
			EndEditHistory();
			EndHistorySequence(tr("Orbit"));
		}
		break;

	case swTool::Roll:
		if (!Camera->IsSimple())
		{
			EndEditHistory();
			EndHistorySequence(tr("Roll"));
		}
		break;

	case swTool::ZoomRegion:
		break;

	case swTool::Count:
		break;
	}
}

bool swModel::InsertPieceToolClicked()
{
	if (mPreviewInsertPieceInfo.empty())
		return false;

	BeginHistorySequence();
	BeginEditHistory();

	swPiece* Piece = nullptr;

	for (const swPreviewInsertPieceInformation& InsertPieceInfo : mPreviewInsertPieceInfo)
	{
		Piece = new swPiece(InsertPieceInfo.PieceData);

		Piece->Initialize(InsertPieceInfo.Transform, mCurrentStep);
		Piece->SetColorIndex(InsertPieceInfo.ColorIndex);
		Piece->UpdatePosition(mCurrentStep);

		AddPiece(Piece);
	}

	SetPreviewInsertPieceInfo(std::vector<swInsertPieceInformation>());

	EndEditHistory();

	gMainWindow->SetLastInsertedColorIndex(Piece->GetColorIndex());

	SetSelectionAndFocus(std::vector<swObject*>(), Piece, SW_PIECE_SECTION_POSITION, swSelectionMode::Single);

	EndHistorySequence(tr("Add Piece"));

	gMainWindow->UpdateTimeline(false, false);
	gMainWindow->UpdateInUseCategory();

	UpdateTrainTrackConnections(Piece, false);

	return true;
}

void swModel::InsertCameraToolClicked(const swVector3& Position)
{
	BeginHistorySequence();
	BeginEditHistory();

	swCamera* Camera = new swCamera(false, Position, GetSelectionOrModelCenter());

	Camera->CreateName(mCameras);
	mCameras.emplace_back(Camera);

	EndEditHistory();

	SetSelectionAndFocus(std::vector<swObject*>(), Camera, SW_CAMERA_SECTION_POSITION, swSelectionMode::Single);

	EndHistorySequence(tr("Add Camera"));
}

void swModel::InsertLightToolClicked(const swVector3& Position, swLightType LightType)
{
	QString ActionName;

	switch (LightType)
	{
	case swLightType::Point:
		ActionName = tr("Add Point Light");
		break;

	case swLightType::Spot:
		ActionName = tr("Add Spotlight");
		break;

	case swLightType::Directional:
		ActionName = tr("Add Directional Light");
		break;

	case swLightType::Area:
		ActionName = tr("Add Area Light");
		break;

	case swLightType::Count:
		return;
	}

	BeginHistorySequence();
	BeginEditHistory();

	swLight* Light = new swLight(Position, LightType);

	Light->CreateName(mLights);
	mLights.emplace_back(Light);

	EndEditHistory();

	SetSelectionAndFocus(std::vector<swObject*>(), Light, SW_LIGHT_SECTION_POSITION, swSelectionMode::Single);

	EndHistorySequence(ActionName);
}

void swModel::UpdateMoveTool(const swVector3& Distance, bool AllowRelative, bool AlternateButtonDrag)
{
	const swVector3 PieceDistance = SnapPosition(Distance) - SnapPosition(mMouseToolDistance);
	const swVector3 ObjectDistance = Distance - mMouseToolDistance;

	swModelTransformFlags Flags = (AllowRelative ? swModelTransformFlag::Relative : swModelTransformFlag::None) | (AlternateButtonDrag ? swModelTransformFlag::RotatePivotPoint : swModelTransformFlag::None) | (mMouseToolFirstMove ? swModelTransformFlag::FirstMove : swModelTransformFlag::None);
	MoveSelectedObjects(PieceDistance, ObjectDistance, Flags, swModelHistoryEditMerge::None);

	if (mMouseToolMovePositionValid)
	{
		if (mMouseToolMoveUsesFocus)
		{
			swObject* Focus = GetFocusObject();

			if (Focus && Focus->GetFocusSection() != SW_OBJECT_SECTION_INVALID)
				mMouseToolMoveEndPosition = Focus->GetSectionPosition(Focus->GetFocusSection());
			else
				mMouseToolMovePositionValid = false;
		}
		else
		{
			swMatrix33 RelativeRotation;
			mMouseToolMovePositionValid = GetMoveRotateTransform(mMouseToolMoveEndPosition, RelativeRotation);
		}
	}

	mMouseToolDistance = Distance;
	mMouseToolFirstMove = false;

	gMainWindow->UpdateSelectedObjects(false);
	UpdateAllViews();
}

void swModel::UpdateFreeMoveTool(swPiece* MousePiece, const swMatrix44& StartTransform, const swMatrix44& NewTransform, bool IsConnection, bool AlternateButtonDrag)
{
	swMatrix33 NewRotation = IsConnection ? swMatrix33(NewTransform) : swMatrix33(StartTransform);
	swMatrix33 CurrentRotation = swMatrix33(MousePiece->mModelWorld);

	if (!swMatrix33Similar(CurrentRotation, NewRotation))
	{
		const swVector3 Distance = NewTransform.GetTranslation() - MousePiece->mModelWorld.GetTranslation();

		swMatrix33 RotationMatrix = swMultiply(swMatrix33AffineInverse(CurrentRotation), NewRotation);

		const swVector3& Center = NewTransform.GetTranslation();
		swMatrix33 RelativeRotation = MousePiece->GetRelativeRotation();

		swMatrix33 WorldToFocusMatrix = swMatrix33AffineInverse(RelativeRotation);
		// Rotate() expects the rotation in the focus frame.
		RotationMatrix = swMultiply(RelativeRotation, RotationMatrix);

		int Flags;
		std::vector<swObject*> Selection;
		swObject* Focus;

		GetSelectionInformation(&Flags, Selection, &Focus);

		for (swObject* Object : Selection)
		{
			if (Object->IsPiece())
			{
				swPiece* Piece = (swPiece*)Object;

				Piece->MoveSelected(mCurrentStep, gMainWindow->GetAddKeys(), Distance);
				Piece->UpdatePosition(mCurrentStep);

				Piece->Rotate(mCurrentStep, gMainWindow->GetAddKeys(), RotationMatrix, Center, WorldToFocusMatrix);
				Piece->UpdatePosition(mCurrentStep);
			}
			else if (Object->IsCamera())
			{
				swCamera* Camera = (swCamera*)Object;

				Camera->MoveSelected(mCurrentStep, gMainWindow->GetAddKeys(), Distance);
				Camera->UpdatePosition(mCurrentStep);

				Camera->Rotate(mCurrentStep, gMainWindow->GetAddKeys(), RotationMatrix, Center, WorldToFocusMatrix);
				Camera->UpdatePosition(mCurrentStep);
			}
			else if (Object->IsLight())
			{
				swLight* Light = (swLight*)Object;

				Light->MoveSelected(mCurrentStep, gMainWindow->GetAddKeys(), Distance, mMouseToolFirstMove);
				Light->UpdatePosition(mCurrentStep);

				Light->Rotate(mCurrentStep, gMainWindow->GetAddKeys(), RotationMatrix, Center, WorldToFocusMatrix);
				Light->UpdatePosition(mCurrentStep);
			}
		}

		mMouseToolDistance = NewTransform.GetTranslation() - StartTransform.GetTranslation();
		mMouseToolFirstMove = false;

		gMainWindow->UpdateSelectedObjects(false);
		UpdateAllViews();
	}
	else
	{
		swVector3 Distance = NewTransform.GetTranslation() - StartTransform.GetTranslation();

		UpdateMoveTool(Distance, false, AlternateButtonDrag);
	}
}

void swModel::UpdateRotateTool(const swVector3& Axis, float Angle, bool AlternateButtonDrag, bool Relative)
{
	const swVector3 Angles(Angle, Angle, Angle);
	const float Delta = SnapRotation(Angles)[0] - SnapRotation(mMouseToolDistance)[0];

	if (Delta != 0.0f)
		RotateSelectedObjects(swMatrix33FromAxisAngle(Axis, Delta * SW_DEGREES_TO_RADIANS), (Relative ? swModelTransformFlag::Relative : swModelTransformFlag::None) | (AlternateButtonDrag ? swModelTransformFlag::RotatePivotPoint : swModelTransformFlag::None), swModelHistoryEditMerge::None);

	mMouseToolDistance = Angles;
	mMouseToolFirstMove = false;
}

void swModel::UpdateRotateTool(const swMatrix33& RotationMatrix, bool AlternateButtonDrag, bool Relative)
{
	RotateSelectedObjects(RotationMatrix, (Relative ? swModelTransformFlag::Relative : swModelTransformFlag::None) | (AlternateButtonDrag ? swModelTransformFlag::RotatePivotPoint : swModelTransformFlag::None), swModelHistoryEditMerge::None);
	mMouseToolFirstMove = false;
}

void swModel::UpdateScaleTool(const float Scale)
{
	ScaleSelectedPieces(Scale);

	gMainWindow->UpdateSelectedObjects(false);
	UpdateAllViews();
}

void swModel::EraserToolClicked(swObject* Object)
{
	if (!Object)
		return;

	BeginHistorySequence();
	BeginEditHistory();

	switch (Object->GetType())
	{
	case swObjectType::Piece:
		for (auto PieceIt = mPieces.begin(); PieceIt != mPieces.end(); ++PieceIt)
			if (PieceIt->get() == Object)
			{
				mPieces.erase(PieceIt);
				RemoveEmptyGroups();
				break;
			}
		break;

	case swObjectType::Camera:
		{
			std::vector<swView*> Views = swView::GetModelViews(this);

			for (swView* View : Views)
			{
				swCamera* Camera = View->GetCamera();

				if (Camera == Object)
					View->SetCamera(Camera, true);
			}

			for (std::vector<std::unique_ptr<swCamera>>::iterator CameraIt = mCameras.begin(); CameraIt != mCameras.end(); CameraIt++)
			{
				if (CameraIt->get() == Object)
				{
					mCameras.erase(CameraIt);
					break;
				}
			}
		}
		break;

	case swObjectType::Light:
		for (std::vector<std::unique_ptr<swLight>>::iterator LightIt = mLights.begin(); LightIt != mLights.end(); LightIt++)
		{
			if (LightIt->get() == Object)
			{
				mLights.erase(LightIt);
				break;
			}
		}
		break;
	}

	EndEditHistory();
	EndHistorySequence(tr("Delete"));

	gMainWindow->UpdateTimeline(false, false);
	gMainWindow->UpdateSelectedObjects(true);
}

void swModel::PaintToolClicked(swObject* Object)
{
	if (!Object || !Object->IsPiece())
		return;

	swPiece* Piece = (swPiece*)Object;

	if (Piece->GetColorIndex() != gMainWindow->mColorIndex)
	{
		BeginHistorySequence();
		BeginEditHistory();

		Piece->SetColorIndex(gMainWindow->mColorIndex);

		EndEditHistory();
		EndHistorySequence(tr("Paint"));

		gMainWindow->UpdateSelectedObjects(false);
		gMainWindow->UpdateTimeline(false, true);
	}
}

void swModel::ColorPickerToolClicked(const swObject* Object)
{
	if (!Object || !Object->IsPiece())
		return;

	const swPiece* Piece = (swPiece*)Object;

	gMainWindow->SetColorIndex(Piece->GetColorIndex());
}

void swModel::UpdateZoomTool(swCamera* Camera, float Mouse)
{
	Camera->Zoom(Mouse - mMouseToolDistance.x, mCurrentStep, gMainWindow->GetAddKeys());
	mMouseToolDistance.x = Mouse;

	UpdateAllViews();
}

void swModel::UpdatePanTool(swCamera* Camera, const swVector3& Distance)
{
	Camera->Pan(Distance, mCurrentStep, gMainWindow->GetAddKeys());

	UpdateAllViews();
}

void swModel::UpdateOrbitTool(swCamera* Camera, float MouseX, float MouseY)
{
	swVector3 Center;
	GetSelectionCenter(Center);

	// With orbit snapping the camera turns in whole steps of the rotation snap angle.
	const float OrbitSnap = gMainWindow->GetOrbitSnap() * SW_DEGREES_TO_RADIANS;

	if (OrbitSnap > 0.0f)
	{
		MouseX = roundf(MouseX / OrbitSnap) * OrbitSnap;
		MouseY = roundf(MouseY / OrbitSnap) * OrbitSnap;
	}

	Camera->Orbit(MouseX - mMouseToolDistance.x, MouseY - mMouseToolDistance.y, Center, mCurrentStep, gMainWindow->GetAddKeys());
	mMouseToolDistance.x = MouseX;
	mMouseToolDistance.y = MouseY;

	UpdateAllViews();
}

void swModel::UpdateRollTool(swCamera* Camera, float Mouse)
{
	Camera->Roll(Mouse - mMouseToolDistance.x, mCurrentStep, gMainWindow->GetAddKeys());
	mMouseToolDistance.x = Mouse;

	UpdateAllViews();
}

void swModel::ZoomRegionToolClicked(swView* View, float AspectRatio, const swVector3& Position, const swVector3& TargetPosition, const swVector3* Corners)
{
	swCamera* Camera = View->GetCamera();

	if (!Camera->IsSimple())
	{
		BeginHistorySequence();
		BeginEditHistory();
	}

	Camera->ZoomRegion(AspectRatio, Position, TargetPosition, Corners, mCurrentStep, gMainWindow->GetAddKeys());

	if (!Camera->IsSimple())
	{
		EndEditHistory();
		EndHistorySequence(tr("Zoom"));
	}

	gMainWindow->UpdateSelectedObjects(false);
	UpdateAllViews();
}

void swModel::LookAt(swCamera* Camera)
{
	swVector3 Center;

	if (!GetSelectionCenter(Center))
	{
		swVector3 Min(FLT_MAX, FLT_MAX, FLT_MAX), Max(-FLT_MAX, -FLT_MAX, -FLT_MAX);

		if (GetVisiblePiecesBoundingBox(Min, Max))
			Center = (Min + Max) / 2.0f;
		else
			Center = swVector3(0.0f, 0.0f, 0.0f);
	}

	if (!Camera->IsSimple())
	{
		BeginHistorySequence();
		BeginEditHistory();
	}

	Camera->Center(Center, mCurrentStep, gMainWindow->GetAddKeys());

	gMainWindow->UpdateSelectedObjects(false);
	UpdateAllViews();

	if (!Camera->IsSimple())
	{
		EndEditHistory();
		EndHistorySequence(tr("Look At"));
	}
}

void swModel::MoveCamera(swCamera* Camera, const swVector3& Direction)
{
	if (!Camera->IsSimple())
	{
		BeginHistorySequence();
		BeginMergeableEditHistory(swModelHistoryEditMerge::KeyboardMoveCamera, swObjectPropertyId::Count);
	}

	Camera->MoveRelative(Direction, mCurrentStep, gMainWindow->GetAddKeys());

	gMainWindow->UpdateSelectedObjects(false);
	UpdateAllViews();

	if (!Camera->IsSimple())
	{
		EndEditHistory();
		EndHistorySequence(tr("Move"));
	}
}

void swModel::ZoomExtents(swCamera* Camera, float Aspect, const swMatrix44& WorldMatrix, swGeometryBoundsMode Mode, bool AdjustFarPlane)
{
	std::vector<swVector3> Points = GetPiecesBoundingBoxPoints(Mode);

	if (Points.empty())
		return;

	swVector3 Min(FLT_MAX, FLT_MAX, FLT_MAX), Max(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	for (swVector3& Point : Points)
	{
		Point = swMultiply31(Point, WorldMatrix);

		Min = swMinimum(Point, Min);
		Max = swMaximum(Point, Max);
	}

	const swVector3 Center = (Min + Max) / 2.0f;

	if (!Camera->IsSimple())
	{
		BeginHistorySequence();
		BeginEditHistory();
	}

	Camera->ZoomExtents(Aspect, Center, Points, mCurrentStep, gMainWindow ? gMainWindow->GetAddKeys() : false, AdjustFarPlane);

	if (!mIsPreview && gMainWindow)
		gMainWindow->UpdateSelectedObjects(false);

	UpdateAllViews();

	if (!Camera->IsSimple())
	{
		EndEditHistory();
		EndHistorySequence(tr("Zoom Extents"));
	}
}

void swModel::Zoom(swCamera* Camera, float Amount)
{
	if (!Camera->IsSimple())
	{
		BeginHistorySequence();
		BeginMergeableEditHistory(swModelHistoryEditMerge::KeyboardZoom, swObjectPropertyId::Count);
	}

	Camera->Zoom(Amount, mCurrentStep, gMainWindow->GetAddKeys());

	if (!mIsPreview)
		gMainWindow->UpdateSelectedObjects(false);

	UpdateAllViews();

	if (!Camera->IsSimple())
	{
		EndEditHistory();
		EndHistorySequence(tr("Zoom"));
	}
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
