#include "sw_global.h"
#include "sw_widget_preview.h"
#include "sw_object_part.h"
#include "sw_data_part.h"
#include "sw_object_camera.h"
#include "sw_project.h"
#include "sw_instructions.h"
#include "sw_window_main.h"
#include "sw_library.h"
#include "sw_profile.h"
#include "sw_dialog_list_model.h"
#include "sw_string.h"

Project::Project(bool IsPreview)
	: mIsPreview(IsPreview)
{
	mModified = false;
	mActiveModel = new swModel(tr(mIsPreview ? "Preview.ldr" : "New Model.ldr"), this, mIsPreview);
	mActiveModel->CreatePartData(this);
	mActiveModel->SetSaved();
	mModels.emplace_back(mActiveModel);

	if (!mIsPreview)
	{
		mFileCheckTimer.setSingleShot(true);
		mFileCheckTimer.setInterval(150);
		QObject::connect(&mFileWatcher, &QFileSystemWatcher::fileChanged, &mFileWatcher, [this]() { mFileCheckTimer.start(); });
		QObject::connect(&mFileWatcher, &QFileSystemWatcher::directoryChanged, &mFileWatcher, [this]() { mFileCheckTimer.start(); });
		QObject::connect(&mFileCheckTimer, &QTimer::timeout, &mFileWatcher, [this]()
		{
			const QString Path = mFileName;

			if (HasFileChanged(Path) && gMainWindow)
				gMainWindow->ProjectFileChanged(Path);
		});
	}
}

Project::~Project()
{
	mModels.clear();
	swGetPartsLibrary()->ReleaseProjectPartData(this);
}

swPartData* Project::FindPartData(const std::string& Name) const
{
	const std::map<std::string, swPartData*>::const_iterator It = mPartIndex.find(Name);

	return It == mPartIndex.end() ? nullptr : It->second;
}

void Project::RegisterPartData(const std::string& Name, swPartData* PartData)
{
	mPartIndex[Name] = PartData;
}

void Project::UnregisterPartData(swPartData* PartData)
{
	for (std::map<std::string, swPartData*>::iterator It = mPartIndex.begin(); It != mPartIndex.end();)
	{
		if (It->second == PartData)
			It = mPartIndex.erase(It);
		else
			It++;
	}
}

void Project::ClearPartDataIndex()
{
	mPartIndex.clear();
}

void Project::TransferPartDataIndexTo(Project* Destination)
{
	for (const std::map<std::string, swPartData*>::value_type& Entry : mPartIndex)
	{
		if (!Destination->FindPartData(Entry.first))
			Destination->RegisterPartData(Entry.first, Entry.second);
	}

	mPartIndex.clear();
}

bool Project::CanShareMergePartData(const swPartData* Existing, const swPartData* Incoming)
{
	if (Existing == Incoming)
		return true;

	if (Existing->IsLibraryPart() || Incoming->IsLibraryPart() || Existing->IsModel() || Incoming->IsModel())
		return false;

	// missing parts can share an identity
	if (!Existing->IsProject() && !Incoming->IsProject())
		return Existing->mState == swAssetState::Failed && Incoming->mState == swAssetState::Failed;

	if (!Existing->IsProject() || !Incoming->IsProject())
		return false;

	const QString ExistingPath = QFileInfo(Existing->GetProject()->GetFileName()).canonicalFilePath();
	const QString IncomingPath = QFileInfo(Incoming->GetProject()->GetFileName()).canonicalFilePath();

	return !ExistingPath.isEmpty() && ExistingPath == IncomingPath;
}

swModel* Project::GetModel(const QString& FileName) const
{
	for (const std::unique_ptr<swModel>& Model : mModels)
		if (Model->GetProperties().mFileName == FileName)
			return Model.get();

	return nullptr;
}

bool Project::IsModified() const
{
	if (mModified)
		return true;

	for (const std::unique_ptr<swModel>& Model : mModels)
		if (Model->IsModified())
			return true;

	return false;
}

QString Project::GetTitle() const
{
	if (!mFileName.isEmpty())
		return QFileInfo(mFileName).fileName();

	return mModels.size() == 1 ? tr("New Model.ldr") : tr("New Model.mpd");
}

QString Project::GetImageFileName(bool AllowCurrentFolder) const
{
	QString FileName = GetFileName();

	if (!FileName.isEmpty())
	{
		QString Extension = QFileInfo(FileName).suffix();
		if (!Extension.isEmpty())
			FileName = FileName.left(FileName.length() - Extension.length() - 1);
	}
	else
	{
		if (AllowCurrentFolder)
			FileName = QLatin1String("image");
		else
		{
			QStringList CachePathList = QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation);
			FileName = CachePathList.first();
			FileName = QDir(FileName).absoluteFilePath(QLatin1String("image"));
		}
	}

	return QDir::toNativeSeparators(FileName) + swGetProfileString(SW_PROFILE_IMAGE_EXTENSION);
}

void Project::SetActiveModel(swModel* ActiveModel, bool UpdateInterface)
{
	if (!ActiveModel)
		return;

	for (const std::unique_ptr<swModel> &Model : mModels)
		Model->SetActive(Model.get() == ActiveModel);

	if (!mIsPreview)
	{
		std::vector<swModel*> UpdatedModels;
		UpdatedModels.reserve(mModels.size());
		for (const std::unique_ptr<swModel> &Model : mModels)
			Model->UpdatePartData(UpdatedModels);
	}

	mActiveModel = ActiveModel;

	if (!mIsPreview && gMainWindow && UpdateInterface)
	{
		gMainWindow->SetCurrentModelTab(mActiveModel, true);

		// restore the current steps of the target
		swView* EditingView = gMainWindow->GetActiveView();

		if (EditingView)
		{
			EditingView->RestoreModelEditingContext();
			EditingView->GetActiveModel()->UpdateInterface();
		}
	}
}

void Project::SetActiveModel(int ModelIndex, bool UpdateInterface)
{
	if (ModelIndex < 0 || ModelIndex >= static_cast<int>(mModels.size()))
		return;

	SetActiveModel(mModels[ModelIndex].get(), UpdateInterface);
}

void Project::SetActiveModel(const QString& FileName, bool UpdateInterface)
{
	for (const std::unique_ptr<swModel>& Model : mModels)
	{
		if (FileName.compare(Model->GetFileName(), Qt::CaseInsensitive) == 0)
		{
			SetActiveModel(Model.get(), UpdateInterface);
			return;
		}
	}
}

QString Project::GetNewModelName(QWidget* ParentWidget, const QString& DialogTitle, const QString& CurrentName, const QStringList& ExistingModels) const
{
	QString Name = CurrentName;

	if (Name.isEmpty())
	{
		const QString Prefix = tr("Submodel ");
		int Max = 0;

		for (const QString& ExistingName : ExistingModels)
			Max = qMax(Max, swGetDefaultNameNumber(ExistingName, Prefix));

		Name = swFormatDefaultName(Prefix, Max + 1) + QLatin1String(".ldr");
	}

	for (;;)
	{
		bool Ok = false;

		Name = QInputDialog::getText(ParentWidget, DialogTitle, tr("Submodel Name:"), QLineEdit::Normal, Name, &Ok);

		if (!Ok)
			return QString();

		if (Name.isEmpty())
		{
			QMessageBox::information(ParentWidget, DialogTitle, tr("The submodel name cannot be empty."));
			continue;
		}

		bool ExtensionValid = false;

		if (Name.length() < 5)
			ExtensionValid = false;
		else
		{
			QString Extension = Name.right(4);
			if (Extension.compare(".dat", Qt::CaseInsensitive) == 0 || Extension.compare(".ldr", Qt::CaseInsensitive) == 0 || Extension.compare(".mpd", Qt::CaseInsensitive) == 0)
				ExtensionValid = true;
		}

		if (!ExtensionValid)
			Name += ".ldr";

		if (ExistingModels.contains(Name, Qt::CaseInsensitive) && Name != CurrentName)
		{
			QMessageBox::information(ParentWidget, DialogTitle, tr("A submodel named '%1' already exists. Please enter a unique name.").arg(Name));
			continue;
		}

		if (!CurrentName.isEmpty())
		{
			const std::string PartName = swPartsLibrary::NormalizePartDataName(Name.toLatin1().constData());
			swPartData* ExistingPart = FindPartData(PartName);
			swModel* CurrentModel = GetModel(CurrentName);

			if (ExistingPart && (!CurrentModel || ExistingPart != CurrentModel->GetPartData()) &&
				(!ExistingPart->IsModel() || ExistingModels.contains(Name, Qt::CaseInsensitive)))
			{
				QMessageBox::information(ParentWidget, DialogTitle, tr("A part named '%1' already exists in this model. Please enter a unique name.").arg(Name));
				continue;
			}
		}

		break;
	}

	return Name;
}

swModel* Project::CreateNewModel(bool ShowModel)
{
	QStringList ModelNames;

	for (const std::unique_ptr<swModel> &Model : mModels)
		ModelNames.append(Model->GetProperties().mFileName);

	QString Name = GetNewModelName(gMainWindow, tr("New Submodel"), QString(), ModelNames);

	if (Name.isEmpty())
		return nullptr;

	mModified = true;
	swModel* Model = new swModel(Name, this, false);
	Model->CreatePartData(this);
	Model->SetSaved();
	mModels.emplace_back(Model);

	if (ShowModel)
	{
		SetActiveModel(mModels.back().get(), true);

		swView* ActiveView = gMainWindow ? gMainWindow->GetActiveView() : nullptr;

		if (ActiveView)
			ActiveView->GetCamera()->SetViewpoint(swViewpoint::Home);

		if (gMainWindow)
			gMainWindow->UpdateTitle();
	}
	else
		SetActiveModel(mActiveModel, true);

	return Model;
}

void Project::ShowModelListDialog()
{
	swModelListDialog Dialog(gMainWindow, mModels);

	if (Dialog.exec() != QDialog::Accepted)
		return;

	std::vector<std::unique_ptr<swModel>> NewModels;
	std::vector<swModelListDialogEntry> Results = Dialog.GetResults();
	std::vector<std::pair<swPartData*, swPartData*>> Replacements;
	std::vector<swModel*> ChangedModels;
	std::vector<swModelPartRename> Renames;
	std::set<std::string> FinalNames;
	int ModelIndex = Dialog.GetActiveModelIndex();

	if (ModelIndex == -1)
	{
		for (size_t Index = 0; Index < Results.size(); Index++)
		{
			if (Results[Index].ExistingModel != mActiveModel)
				continue;

			ModelIndex = static_cast<int>(Index);
			break;
		}

		if (ModelIndex == -1)
			ModelIndex = 0;
	}

	// validate names before changes
	for (const swModelListDialogEntry& Entry : Results)
	{
		const std::string Name = swPartsLibrary::NormalizePartDataName(Entry.Name.toLatin1().constData());

		if (!FinalNames.insert(Name).second)
		{
			QMessageBox::information(gMainWindow, tr("Submodels"), tr("A part named '%1' already exists in this model.").arg(Entry.Name));
			return;
		}

		if (!Entry.ExistingModel || Entry.ExistingModel->GetFileName() == Entry.Name)
			continue;

		swPartData* Occupant = FindPartData(Name);

		if (!Occupant || Occupant == Entry.ExistingModel->GetPartData())
			continue;

		const bool Vacated = std::any_of(mModels.begin(), mModels.end(), [&Results, Occupant, &Name](const std::unique_ptr<swModel>& OldModel)
		{
			if (OldModel->GetPartData() != Occupant)
				return false;

			const std::vector<swModelListDialogEntry>::const_iterator Result = std::find_if(Results.begin(), Results.end(), [OldModel = OldModel.get()](const swModelListDialogEntry& CheckEntry)
			{
				return CheckEntry.ExistingModel == OldModel;
			});

			return Result == Results.end() || swPartsLibrary::NormalizePartDataName(Result->Name.toLatin1().constData()) != Name;
		});

		if (!Vacated)
		{
			QMessageBox::information(gMainWindow, tr("Submodels"), tr("A part named '%1' already exists in this model.").arg(Entry.Name));
			return;
		}
	}

	// free renamed or deleted names
	for (const std::unique_ptr<swModel>& OldModel : mModels)
	{
		const swModel* Model = OldModel.get();
		const std::vector<swModelListDialogEntry>::const_iterator Kept = std::find_if(Results.begin(), Results.end(), [Model](const swModelListDialogEntry& Entry)
		{
			return Entry.ExistingModel == Model;
		});

		if (Kept != Results.end())
		{
			if (Kept->Name != Model->GetFileName())
				UnregisterPartData(Model->GetPartData());

			continue;
		}

		const std::string OldName = swPartsLibrary::NormalizePartDataName(Model->GetFileName().toLatin1().constData());

		for (const swModelListDialogEntry& Entry : Results)
		{
			if (swPartsLibrary::NormalizePartDataName(Entry.Name.toLatin1().constData()) != OldName)
				continue;

			// rebind references to deleted models
			swGetPartsLibrary()->InvalidatePartDataLoad(OldModel->GetPartData());
			OldModel->GetPartData()->DetachContainer();
			OldModel->GetPartData()->SetFailed(tr("Model '%1' was deleted.").arg(Model->GetFileName()));

			if (Entry.ExistingModel)
			{
				UnregisterPartData(OldModel->GetPartData());
				Replacements.emplace_back(OldModel->GetPartData(), Entry.ExistingModel->GetPartData());
			}

			break;
		}
	}

	// rename models before loading duplicates
	for (const swModelListDialogEntry& Entry : Results)
	{
		swModel* Model = Entry.ExistingModel;

		if (!Model || Model->GetFileName() == Entry.Name)
			continue;

		const QString OldName = Model->GetFileName();

		if (!swGetPartsLibrary()->RenameModelPartData(Model->GetPartData(), Entry.Name.toLatin1().constData()))
		{
			QMessageBox::information(gMainWindow, tr("Submodels"), tr("A part named '%1' already exists in this model.").arg(Entry.Name));
			return;
		}

		Model->SetFileName(Entry.Name);
		Renames.push_back({ Model->GetPartData(), OldName, Entry.Name });

		mModified = true;
	}

	// apply all renames together
	for (const std::unique_ptr<swModel>& Model : mModels)
		Model->RenameParts(Renames);

	// instructions stay with the main model
	std::map<QString, QString> InstructionNames;

	for (const swModelPartRename& Rename : Renames)
		InstructionNames[Rename.OldName] = Rename.NewName;

	QStringList InstructionLines = GetMainModel() ? GetMainModel()->GetInstructionLines() : QStringList();
	swRenameInstructionsModels(InstructionLines, InstructionNames);

	for (const swModelListDialogEntry& Entry : Results)
	{
		swModel* Model = Entry.ExistingModel;

		if (!Model)
		{
			const swModel* Source = Entry.DuplicateSource;

			if (!Source)
			{
				Model = new swModel(Entry.Name, this, false);
			}
			else
			{
				Model = new swModel(Source->GetProperties().mFileName, this, false);

				QByteArray File;

				QTextStream SaveStream(&File);
				Source->SaveLDraw(SaveStream, false, 0);
				SaveStream.flush();

				QBuffer Buffer(&File);
				Buffer.open(QIODevice::ReadOnly);

				Model->LoadLDraw(Buffer, this);
				Model->SetFileName(Entry.Name);
			}

			Model->CreatePartData(this);
			Model->SetSaved();

			mModified = true;
		}
		NewModels.emplace_back(Model);
	}

	for (const std::pair<swPartData*, swPartData*>& Replacement : Replacements)
	{
		for (const std::unique_ptr<swModel>& Model : NewModels)
		{
			if (Model->RebindPartData(Replacement.first, Replacement.second) && std::find(ChangedModels.begin(), ChangedModels.end(), Model.get()) == ChangedModels.end())
				ChangedModels.push_back(Model.get());
		}

		if (gMainWindow->GetInsertPartData() == Replacement.first)
			gMainWindow->SetInsertPartData(Replacement.second);

		gMainWindow->GetPreviewWidget()->RebindPartData(Replacement.first, Replacement.second);
	}

	for (std::unique_ptr<swModel>& Model : mModels)
	{
		if (std::find(NewModels.begin(), NewModels.end(), Model) != NewModels.end())
			Model.release();
		else
			mModified = true;
	}

	mModels = std::move(NewModels);

	for (const std::unique_ptr<swModel>& Model : mModels)
		Model->SetInstructionLines(Model == mModels.front() ? InstructionLines : QStringList());

	gMainWindow->UpdateTitle();

	SetActiveModel(ModelIndex, true);
	gMainWindow->UpdateModels();

	if (!Renames.empty())
		gMainWindow->GetPreviewWidget()->RefreshDescription();

	for (swModel* Model : ChangedModels)
		Model->RefreshPreview();
}

Project::FileState Project::ReadFileState(const QString& FileName)
{
	FileState State;
	State.Exists = QFileInfo(FileName).isFile();

	if (!State.Exists)
		return State;

	QFile File(FileName);
	QCryptographicHash Hash(QCryptographicHash::Sha256);
	State.Readable = File.open(QIODevice::ReadOnly) && Hash.addData(&File);

	if (State.Readable)
		State.Hash = Hash.result();

	return State;
}

bool Project::HasFileChanged(const QString& Path)
{
	if (mIsPreview || this != swGetActiveProject() || Path.isEmpty() || Path != mFileName)
		return false;

	UpdateFileWatcher();
	const FileState State = ReadFileState(Path);

	if (State.Exists && !State.Readable && ++mFileReadAttempts < 3)
	{
		mFileCheckTimer.start();
		return false;
	}

	mFileReadAttempts = 0;

	if (State.Hash == mFileHash && State.Exists == mFileExists && State.Readable == mFileReadable)
		return false;

	// remember contents against duplicate events
	mFileHash = State.Hash;
	mFileExists = State.Exists;
	mFileReadable = State.Readable;
	return true;
}

void Project::UpdateFileWatcher()
{
	QStringList Files;
	QStringList Directories;
	const bool Active = !mIsPreview && this == swGetActiveProject() && !mFileName.isEmpty() && swGetPreferences().mWatchFileChanges;
	const bool WasWatching = !mFileWatcher.files().isEmpty() || !mFileWatcher.directories().isEmpty();

	if (Active)
	{
		if (QFileInfo(mFileName).isFile())
			Files.push_back(mFileName);

		QDir Directory(QFileInfo(mFileName).absolutePath());

		while (!Directory.exists())
		{
			const QString Parent = QFileInfo(Directory.absolutePath()).absolutePath();

			if (Parent == Directory.absolutePath())
				break;

			Directory.setPath(Parent);
		}

		if (Directory.exists())
			Directories.push_back(Directory.absolutePath());
	}

	for (const QString& Path : mFileWatcher.files())
		if (!Files.contains(Path))
			mFileWatcher.removePath(Path);

	for (const QString& Path : mFileWatcher.directories())
		if (!Directories.contains(Path))
			mFileWatcher.removePath(Path);

	for (const QString& Path : Files)
		if (!mFileWatcher.files().contains(Path))
			mFileWatcher.addPath(Path);

	for (const QString& Path : Directories)
		if (!mFileWatcher.directories().contains(Path))
			mFileWatcher.addPath(Path);

	if (!Active)
		mFileCheckTimer.stop();
	else if (!WasWatching)
		mFileCheckTimer.start();
}

void Project::SetFileName(const QString& FileName)
{
	if (mFileName == FileName)
		return;

	mFileName = FileName;
	UpdateFileWatcher();
}

bool swEmbeddedData::HasValidEncoding() const
{
	const QByteArray& Encoded = EncodedData;
	int Padding = 0;
	bool Valid = !Encoded.isEmpty();

	for (const unsigned char Character : Encoded)
	{
		if (Character == '=')
			Padding++;
		else if (Padding || !((Character >= 'A' && Character <= 'Z') || (Character >= 'a' && Character <= 'z') ||
			(Character >= '0' && Character <= '9') || Character == '+' || Character == '/'))
		{
			Valid = false;
			break;
		}
	}

	return Valid && Padding <= 2 && Encoded.size() % 4 != 1 && (!Padding || Encoded.size() % 4 == 0);
}

QString Project::NormalizeEmbeddedDataName(const QString& FileName)
{
	QString Name = FileName;
	Name.replace(QLatin1Char('\\'), QLatin1Char('/'));

	return QDir::cleanPath(Name).toUpper();
}

std::shared_ptr<const swEmbeddedData> Project::FindEmbeddedData(const QString& FileName) const
{
	if (FileName.isEmpty())
		return std::shared_ptr<const swEmbeddedData>();

	const auto Entry = mEmbeddedDataIndex.find(NormalizeEmbeddedDataName(FileName));

	if (Entry == mEmbeddedDataIndex.end())
		return std::shared_ptr<const swEmbeddedData>();

	return Entry->second;
}

std::vector<swModelPartsEntry> Project::GetModelParts(swStep Step)
{
	std::vector<swModelPartsEntry> ModelParts;

	if (mModels.empty())
		return ModelParts;

	mModels[0]->WaitForAssets();

	struct swScopedModelSteps
	{
		std::vector<std::pair<swModel*, bool>> States;

		~swScopedModelSteps()
		{
			for (const std::pair<swModel*, bool>& State : States)
				State.first->SetActive(State.second);
		}
	};

	// export geometry without changing edits
	swScopedModelSteps ModelSteps;
	ModelSteps.States.reserve(mModels.size());

	for (const std::unique_ptr<swModel>& Model : mModels)
	{
		ModelSteps.States.emplace_back(Model.get(), Model->IsActive());
		Model->CalculateStep(SW_STEP_MAXIMUM);
	}

	if (Step == SW_STEP_MAXIMUM)
		mModels[0]->GetModelParts(swMatrix44Identity(), gDefaultColor, ModelParts);
	else
	{
		// keep parts in place at their step
		mModels[0]->CalculateStep(Step);

		for (const std::unique_ptr<swPart>& Part : mModels[0]->GetParts())
			if (Part->IsVisible(Step))
				Part->GetModelParts(swMatrix44Identity(), gDefaultColor, ModelParts);
	}

	ModelParts.erase(std::remove_if(ModelParts.begin(), ModelParts.end(), [](const swModelPartsEntry& ModelPart)
	{
		return ModelPart.PartData->mState != swAssetState::Loaded ||
			(ModelPart.PartData->GetSynthInfo() && !ModelPart.Mesh) ||
			(!ModelPart.Mesh && !ModelPart.PartData->GetMesh());
	}), ModelParts.end());

	return ModelParts;
}

std::vector<swPartDataReference> Project::GetRequiredPartData() const
{
	std::vector<swPartDataReference> RequiredPartData;

	for (const std::unique_ptr<swModel>& Model : mModels)
	{
		std::vector<swPartDataReference> ModelPartData = Model->GetRequiredPartData();
		RequiredPartData.insert(RequiredPartData.end(), ModelPartData.begin(), ModelPartData.end());
	}

	std::sort(RequiredPartData.begin(), RequiredPartData.end());
	RequiredPartData.erase(std::unique(RequiredPartData.begin(), RequiredPartData.end()), RequiredPartData.end());

	return RequiredPartData;
}

std::vector<swPart*> Project::GetRequiredSynthParts() const
{
	std::vector<swPart*> Required;
	std::unordered_set<swPart*> Seen;

	for (const std::unique_ptr<swModel>& Model : mModels)
	{
		const std::vector<swPart*> ModelRequired = Model->GetRequiredSynthParts();

		for (swPart* Part : ModelRequired)
			if (Seen.insert(Part).second)
				Required.push_back(Part);
	}

	return Required;
}

swResult<void> Project::EnsureAssetsReady() const
{
	const swModel* Model = GetMainModel();

	return Model ? Model->EnsureAssetsReady() : swResult<void>();
}

swInstructions* Project::GetInstructions()
{
	mInstructions.reset();
	mInstructions = std::unique_ptr<swInstructions>(new swInstructions(this));

	return mInstructions.get();
}

std::unique_ptr<swInstructions> Project::TakeInstructions()
{
	return std::move(mInstructions);
}

void Project::SetInstructions(std::unique_ptr<swInstructions> Instructions)
{
	mInstructions = std::move(Instructions);

	if (mInstructions)
		mInstructions->SetProject(this);
}

void Project::UpdatePartData(swPartData* PartData) const
{
	if (!mModels.empty())
	{
		std::vector<swModel*> UpdatedModels;
		mModels[0]->UpdatePartData(UpdatedModels);

		swBoundingBox BoundingBox = mModels[0]->GetPartData()->GetBoundingBox();
		PartData->SetBoundingBox(BoundingBox.Min, BoundingBox.Max);
	}
}

void Project::MarkAsModified()
{
	mModified = true;
}

void Project::MigrateMetaCommands()
{
	mLeoCADMetaCommands = false;
	mModified = true;

	// reserialize models
	for (DocumentBlock& Block : mDocumentBlocks)
		Block.Baseline.clear();
}
