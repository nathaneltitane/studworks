#include "sw_global.h"
#include "sw_math.h"
#include "sw_mesh.h"
#include "sw_widget_preview.h"
#include "sw_object_part.h"
#include "sw_data_part.h"
#include "sw_object_camera.h"
#include "sw_project.h"
#include "sw_instructions.h"
#include "sw_object_light.h"
#include "sw_window_main.h"
#include "sw_view.h"
#include "sw_library.h"
#include "sw_texture.h"
#include "sw_manager_thumbnail.h"
#include "sw_application.h"
#include "sw_profile.h"
#include "sw_file.h"
#include "sw_file_zip.h"
#include "sw_dialog_list_model.h"
#include "sw_bricklink.h"
#include "sw_string.h"
#include "sw_colors.h"
#include "sw_command_meta.h"

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

	// The callers have already matched filenames. Two missing part references
	// can share an identity, while failed library parts and models cannot.
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

		// Restore the current steps of the editing target and its surrounding ancestors.
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

	// Validate the final names before changing any model or index entry.
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

	// Free names being renamed or deleted before applying the new names.
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

			// A new model can reuse this swPartData. A renamed model keeps its own
			// swPartData, so references to the deleted model are rebound below.
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

	// Rename surviving models before loading any duplicates from them.
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

	// Apply original-to-final names together so a later rename cannot rewrite
	// an earlier rename's history snapshots a second time.
	for (const std::unique_ptr<swModel>& Model : mModels)
		Model->RenameParts(Renames);

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

	// Remember the observed contents so duplicate events do not prompt again.
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

void Project::LoadLDrawDocument(const QByteArray& FileData, const Project* ReferenceProject, bool ReuseExistingModels)
{
	enum class BlockType { ImplicitModel, Model, Data, Ignore };
	struct ModelBlock
	{
		QString FileName;
		qint64 Begin;
		qint64 End;
		size_t LayoutIndex;
	};

	std::vector<ModelBlock> ModelBlocks;
	BlockType Type = BlockType::ImplicitModel;
	qint64 BlockBegin = 0;
	QString ModelName;
	bool HasImplicitContent = false;
	bool HasImplicitGeometry = false;
	std::shared_ptr<swEmbeddedData> Data;

	const auto FinishBlock = [&](qint64 End, bool KeepImplicit)
	{
		DocumentBlock Block;
		Block.Raw = FileData.mid(BlockBegin, End - BlockBegin);

		if (Type == BlockType::Model)
		{
			ModelBlocks.push_back({ ModelName, BlockBegin, End, mDocumentBlocks.size() });
			Block.ExplicitModel = true;
			Block.OriginalName = ModelName;
		}
		else if (Type == BlockType::ImplicitModel && KeepImplicit && HasImplicitContent)
		{
			const QString Name = QFileInfo(mFileName).fileName();
			ModelBlocks.push_back({ Name, BlockBegin, End, mDocumentBlocks.size() });
			Block.OriginalName = Name;
		}
		else if (Type == BlockType::Data)
		{
			Data->RawBlock = Block.Raw;

			if (Data->FileName.isEmpty())
				Data->Error = tr("Embedded data has no filename.");
			else
				mEmbeddedDataIndex.emplace(NormalizeEmbeddedDataName(Data->FileName), Data);

			Block.Data = Data;
			mEmbeddedData.push_back(Data);
			Data.reset();
		}

		mDocumentBlocks.push_back(std::move(Block));
	};

	QBuffer Buffer;
	Buffer.setData(FileData);
	Buffer.open(QIODevice::ReadOnly);

	while (!Buffer.atEnd())
	{
		const qint64 Pos = Buffer.pos();
		const QByteArray OriginalLine = Buffer.readLine();
		QString Line = QString::fromUtf8(OriginalLine).trimmed();
		QTextStream LineStream(&Line, QIODevice::ReadOnly);
		QString LineType;
		QString Meta;
		LineStream >> LineType >> Meta;

		if (LineType == QLatin1String("0") && Meta == QLatin1String("FILE"))
		{
			// Preamble before an explicit first FILE is not part of that model.
			FinishBlock(Pos, false);
			Type = BlockType::Model;
			BlockBegin = Pos;
			ModelName = LineStream.readAll().trimmed();
		}
		else if (LineType == QLatin1String("0") && Meta == QLatin1String("!DATA"))
		{
			// Also accept a plain LDR model followed by embedded data.
			FinishBlock(Pos, HasImplicitGeometry);
			Type = BlockType::Data;
			BlockBegin = Pos;
			Data = std::make_shared<swEmbeddedData>();
			Data->FileName = LineStream.readAll().trimmed();
		}
		else if (LineType == QLatin1String("0") && Meta == QLatin1String("NOFILE"))
		{
			FinishBlock(Pos, true);
			Type = BlockType::Ignore;
			BlockBegin = Pos;
		}
		else if (Type == BlockType::Data)
		{
			if (LineType == QLatin1String("0") && Meta == QLatin1String("!:"))
				Data->EncodedData += LineStream.readAll().trimmed().toLatin1();
			else if (!Line.isEmpty() && Data->Error.isEmpty())
				Data->Error = tr("Unexpected statement in embedded data '%1'.").arg(Data->FileName);
		}
		else if (Type == BlockType::ImplicitModel && !Line.isEmpty())
		{
			HasImplicitContent = true;
			int GeometryType = LineType.toInt();

			if (LineType == QLatin1String("0") && Meta == QLatin1String("!:"))
			{
				QString Token;
				LineStream >> Token;
				GeometryType = Token.toInt();
			}

			HasImplicitGeometry |= GeometryType >= 1 && GeometryType <= 5;
		}
	}

	FinishBlock(Buffer.pos(), true);

	// Preserve the existing ability to open an empty ordinary LDraw document.
	if (ModelBlocks.empty() && mEmbeddedData.empty())
		ModelBlocks.push_back({ QFileInfo(mFileName).fileName(), 0, 0, 0 });

	std::vector<std::pair<const ModelBlock*, swModel*>> Models;

	for (const ModelBlock& Block : ModelBlocks)
	{
		const auto Existing = std::find_if(mModels.begin(), mModels.end(), [&Block](const std::unique_ptr<swModel>& Model)
		{
			return Model->GetFileName().compare(Block.FileName, Qt::CaseInsensitive) == 0;
		});

		if (Existing != mModels.end())
		{
			AddLoadWarning(tr("Duplicate model '%1' was ignored; model names are case-insensitive.").arg(Block.FileName));
			mDocumentBlocks[Block.LayoutIndex].Model = Existing->get();
			mDocumentBlocks[Block.LayoutIndex].DuplicateModel = true;
			continue;
		}

		if (ReuseExistingModels && ReferenceProject && !Models.empty())
		{
			const std::string Name = swPartsLibrary::NormalizePartDataName(Block.FileName.toLatin1().constData());
			swPartData* ExistingPartData = ReferenceProject->FindPartData(Name);

			if (ExistingPartData && (ExistingPartData->IsModel() || ExistingPartData->IsProject()))
			{
				RegisterPartData(Name, ExistingPartData);
				continue;
			}
		}

		std::unique_ptr<swModel> Model(new swModel(Block.FileName, this, mIsPreview));
		Models.emplace_back(&Block, Model.get());
		mModels.emplace_back(std::move(Model));
		mModels.back()->CreatePartData(this);
		mDocumentBlocks[Block.LayoutIndex].Model = mModels.back().get();
		mDocumentBlocks[Block.LayoutIndex].OriginalName = Block.FileName;
	}

	// Legacy clipboard references can use destination parts when no definition was copied.
	if (ReferenceProject)
		for (const auto& [Name, PartData] : ReferenceProject->mPartIndex)
			if (!FindPartData(Name))
				RegisterPartData(Name, PartData);

	// Every model and asset is registered before geometry resolves references.
	for (const std::pair<const ModelBlock*, swModel*>& Entry : Models)
	{
		QBuffer ModelBuffer;
		ModelBuffer.setData(FileData.mid(Entry.first->Begin, Entry.first->End - Entry.first->Begin));
		ModelBuffer.open(QIODevice::ReadOnly);
		Entry.second->LoadLDraw(ModelBuffer, this);
		Entry.second->SetSaved();
	}

	for (DocumentBlock& Block : mDocumentBlocks)
		if (Block.Model)
			Block.Baseline = SerializeModelText(Block.Model, false, 0);
}


QByteArray Project::SerializeModelText(const swModel* Model, bool SelectedOnly, swStep LastStep)
{
	QByteArray Bytes;
	QTextStream Stream(&Bytes, QIODevice::WriteOnly);
	Model->SaveLDraw(Stream, SelectedOnly, LastStep);
	Stream.flush();

	return Bytes;
}

QByteArray Project::SerializeStoredModel(const swModel* Model, const QString& Name, const DocumentBlock* Original, bool NeedsHeader, bool SelectedOnly, swStep LastStep)
{
	const QByteArray Current = SerializeModelText(Model, SelectedOnly, LastStep);
	const bool Preserve = Original && !SelectedOnly && !LastStep && Current == Original->Baseline;
	QByteArray LineEnding("\r\n");

	if (Original && Original->Raw.contains('\n') && !Original->Raw.contains("\r\n"))
		LineEnding = "\n";

	if (Preserve && Original->ExplicitModel && Original->OriginalName == Name)
		return Original->Raw;

	QByteArray Text;
	const bool Explicit = Original && Original->ExplicitModel;
	const bool Renamed = Original && Original->OriginalName != Name;

	if (NeedsHeader || Explicit || Renamed)
	{
		const int HeaderEnd = Original ? Original->Raw.indexOf('\n') : -1;

		if (Explicit && !Renamed && HeaderEnd != -1)
			Text = Original->Raw.left(HeaderEnd + 1);
		else
			Text = "0 FILE " + Name.toUtf8() + LineEnding;
	}

	if (Preserve)
	{
		if (Explicit)
		{
			const int HeaderEnd = Original->Raw.indexOf('\n');
			Text += Original->Raw.mid(HeaderEnd == -1 ? Original->Raw.size() : HeaderEnd + 1);
		}
		else
			Text += Original->Raw;
	}
	else
	{
		QByteArray Body = Current;

		if (LineEnding == "\n")
			Body.replace("\r\n", "\n");

		if (Original && !Original->Raw.endsWith('\n') && !Original->Raw.endsWith('\r') && Body.endsWith(LineEnding))
			Body.chop(LineEnding.size());

		Text += Body;
	}

	return Text;
}

bool Project::WriteRaw(QTextStream& Stream, const QByteArray& Bytes)
{
	Stream.flush();

	if (Stream.status() != QTextStream::Ok)
		return false;

	if (QIODevice* Device = Stream.device())
		return Device->write(Bytes) == Bytes.size();

	Stream << QString::fromUtf8(Bytes);
	return Stream.status() == QTextStream::Ok;
}

bool Project::SameEmbeddedData(const swEmbeddedData& First, const swEmbeddedData& Second)
{
	if (First.Error != Second.Error)
		return false;

	if (First.EncodedData == Second.EncodedData)
		return true;

	if (!First.Error.isEmpty() || !First.HasValidEncoding() || !Second.HasValidEncoding())
		return false;

	int FirstSize = First.EncodedData.size();
	int SecondSize = Second.EncodedData.size();

	while (FirstSize && First.EncodedData[FirstSize - 1] == '=')
		FirstSize--;

	while (SecondSize && Second.EncodedData[SecondSize - 1] == '=')
		SecondSize--;

	return FirstSize == SecondSize && !memcmp(First.EncodedData.constData(), Second.EncodedData.constData(), FirstSize);
}

void Project::AddLoadWarning(const QString& Warning)
{
	AddLoadWarning(LoadWarning{ mFileName.isEmpty() ? GetTitle() : mFileName, Warning });
}

void Project::AddLoadWarning(const LoadWarning& Warning)
{
	const auto Existing = std::find_if(mLoadWarnings.begin(), mLoadWarnings.end(), [&Warning](const LoadWarning& Current)
	{
		return Current.FileName == Warning.FileName && Current.Message == Warning.Message;
	});

	if (Existing == mLoadWarnings.end())
		mLoadWarnings.push_back(Warning);
}

std::vector<QString> Project::GetLoadWarningDetails() const
{
	std::vector<QString> Details;
	std::set<QString> SeenWarnings;
	std::vector<const Project*> Projects{ this };
	std::unordered_set<const Project*> SeenProjects{ this };

	for (size_t ProjectIndex = 0; ProjectIndex < Projects.size(); ProjectIndex++)
	{
		const Project* Current = Projects[ProjectIndex];

		for (const LoadWarning& Warning : Current->mLoadWarnings)
		{
			const QString Detail = QStringLiteral("'%1': %2").arg(QDir::toNativeSeparators(Warning.FileName), Warning.Message);

			if (SeenWarnings.insert(Detail).second)
				Details.push_back(Detail);
		}

		for (const std::unique_ptr<swModel>& Model : Current->mModels)
			for (const std::unique_ptr<swPart>& Part : Model->GetParts())
			{
				const swPartData* PartData = Part->mPartData.Get();

				if (!PartData || !PartData->IsProject())
					continue;

				const Project* Referenced = PartData->GetProject();

				if (Referenced && SeenProjects.insert(Referenced).second)
					Projects.push_back(Referenced);
			}
	}

	return Details;
}

bool Project::Load(const QString& FileName, bool ShowErrors)
{
	QWidget *parent = nullptr;
	if (!mIsPreview)
		parent = gMainWindow;

	QFile File(FileName);

	if (!File.open(QIODevice::ReadOnly))
	{
		if (ShowErrors)
			QMessageBox::warning(parent, tr("Load Model"), tr("Error reading file '%1':\n%2").arg(FileName, File.errorString()));
		return false;
	}

	QByteArray FileData = File.readAll();

	if (File.error() != QFileDevice::NoError)
	{
		if (ShowErrors)
			QMessageBox::warning(parent, tr("Load Model"), tr("Error reading file '%1':\n%2").arg(FileName, File.errorString()));
		return false;
	}

	mModels.clear();
	mDocumentBlocks.clear();
	mLoadWarnings.clear();
	mLeoCADMetaCommands = false;
	mEmbeddedData.clear();
	mEmbeddedDataIndex.clear();
	swGetPartsLibrary()->ReleaseProjectPartData(this);
	SetFileName(FileName);
	QFileInfo FileInfo(FileName);
	QString Extension = FileInfo.suffix().toLower();

	if (!mIsPreview)
	{
		mFileHash = QCryptographicHash::hash(FileData, QCryptographicHash::Sha256);
		mFileExists = true;
		mFileReadable = true;
		mFileReadAttempts = 0;
	}

	bool LoadDAT;

	if (Extension == QLatin1String("dat") || Extension == QLatin1String("ldr") || Extension == QLatin1String("mpd"))
		LoadDAT = true;
	else if (Extension == QLatin1String("lcd") || Extension == QLatin1String("leocad"))
		LoadDAT = false;
	else
		LoadDAT = FileData.size() > 7 && memcmp(FileData, "LeoCAD ", 7);

	if (LoadDAT)
	{
		LoadLDrawDocument(FileData, nullptr, false);
		mLeoCADMetaCommands = !mIsPreview && swHasLeoCADCommandMeta(FileData);
	}
	else
	{
		swMemoryFile MemFile;
		MemFile.WriteBuffer(FileData.constData(), FileData.size());
		MemFile.Seek(0, SEEK_SET);

		swModel* Model = new swModel(QString(), this, mIsPreview);

		if (Model->LoadBinary(&MemFile))
		{
			mModels.emplace_back(Model);
			Model->CreatePartData(this);
			Model->SetSaved();
		}
		else
			delete Model;
	}

	if (mModels.empty())
	{
		if (ShowErrors)
			QMessageBox::warning(parent, tr("Load Model"), tr("Error loading file '%1':\nFile format is not recognized.").arg(FileName));
		return false;
	}

	if (mModels.size() == 1)
	{
		swModel* Model = mModels.front().get();

		if (Model->GetProperties().mFileName.isEmpty())
		{
			QString ModelName = FileInfo.fileName();
			int Suffix = 2;

			while (!swGetPartsLibrary()->RenameModelPartData(Model->GetPartData(), ModelName.toLatin1().constData()))
			{
				const QString FileExtension = FileInfo.suffix().isEmpty() ? QString() : QLatin1Char('.') + FileInfo.suffix();
				const QString NameSuffix = QLatin1Char(' ') + QString::number(Suffix++) + FileExtension;
				const int BaseLength = SW_PART_NAME_LENGTH - 1 - NameSuffix.length();
				ModelName = FileInfo.completeBaseName().left(BaseLength) + NameSuffix;
			}

			Model->SetFileName(ModelName);
		}
	}

	std::vector<swModel*> UpdatedModels;
	UpdatedModels.reserve(mModels.size());

	for (const std::unique_ptr<swModel>& Model : mModels)
	{
		if (mIsPreview)
		{
			swPartData* PartData = Model->GetPartData();

			PartData->ReleaseMesh();
			swGetPartsLibrary()->InvalidatePartDataLoad(PartData);
		}
		else
		{
			if (mDeferModelMeshRequests)
				swGetPartsLibrary()->InvalidatePartDataLoad(Model->GetPartData());

			Model->QueueMeshBuild();
		}
	}

	if (!mIsPreview)
	{
		for (const std::unique_ptr<swModel>& Model : mModels)
			Model->UpdatePartData(UpdatedModels);
	}

	mModified = false;

	return true;
}

swResult<void> Project::Save(const QString& FileName)
{
	std::vector<std::pair<swModel*, swModelAssetSourceState>> PreviousAssetSources;
	const QString SourceDirectory = mFileName.isEmpty() ? QString() : QFileInfo(mFileName).absolutePath();
	const QString DestinationDirectory = QFileInfo(FileName).absolutePath();
	const bool DirectoryChanged = SourceDirectory.isEmpty() ||
		(SourceDirectory != DestinationDirectory && QDir(SourceDirectory).canonicalPath() != QDir(DestinationDirectory).canonicalPath());

	if (DirectoryChanged && !SourceDirectory.isEmpty())
	{
		for (const std::unique_ptr<swModel>& Model : mModels)
		{
			PreviousAssetSources.emplace_back(Model.get(), Model->GetAssetSourceState());
			Model->PreserveAssetSourceDirectory(SourceDirectory, true);
		}
	}

	const auto RestoreAssetSources = [&PreviousAssetSources]()
	{
		for (const std::pair<swModel*, swModelAssetSourceState>& Entry : PreviousAssetSources)
			Entry.first->RestoreAssetSourceState(Entry.second);
	};

	QSaveFile File(FileName);

	if (!File.open(QIODevice::WriteOnly))
	{
		RestoreAssetSources();
		return swUnexpected(tr("Error writing to file '%1':\n%2").arg(FileName, File.errorString()));
	}

	{
		QByteArray Bytes;
		QTextStream Stream(&Bytes, QIODevice::WriteOnly);
		const bool Serialized = Save(Stream);
		Stream.flush();

		if (Serialized && Stream.status() == QTextStream::Ok)
			Bytes = swWriteCommandMeta(Bytes, mLeoCADMetaCommands ? swCommandMetaFormat::LeoCAD : swCommandMetaFormat::StudWorks);

		if (!Serialized || Stream.status() != QTextStream::Ok || File.write(Bytes) != Bytes.size())
		{
			File.cancelWriting();
			RestoreAssetSources();
			return swUnexpected(tr("Error writing to file '%1':\n%2").arg(FileName, tr("Could not write all model data.")));
		}
	}

	const QString PreviousFileName = mFileName;
	SetFileName(QString());

	if (!File.commit())
	{
		SetFileName(PreviousFileName);
		RestoreAssetSources();
		return swUnexpected(tr("Error writing to file '%1':\n%2").arg(FileName, File.errorString()));
	}

	if (!mIsPreview)
	{
		const FileState State = ReadFileState(FileName);
		mFileHash = State.Hash;
		mFileExists = State.Exists;
		mFileReadable = State.Readable;
		mFileReadAttempts = 0;
	}

	SetFileName(FileName);

	if (DirectoryChanged)
	{
		swPartsLibrary* Library = swGetPartsLibrary();
		std::vector<swPartDataReference> LocalPartData;
		std::vector<swPartData*> ChangedParts;

		for (const std::map<std::string, swPartData*>::value_type& Entry : mPartIndex)
		{
			swPartData* PartData = Entry.second;

			if (PartData->IsModel())
				continue;

			LocalPartData.push_back(swPartDataReference(PartData));
		}

		for (const swPartDataReference& LocalData : LocalPartData)
			if (Library->RemapProjectPartData(LocalData.Get(), DestinationDirectory, mIsPreview))
				ChangedParts.push_back(LocalData.Get());

		for (const std::unique_ptr<swModel>& Model : mModels)
		{
			if (!Model->HasDirectGeometry())
				continue;

			Library->InvalidatePartDataLoad(Model->GetPartData());
			Model->QueueMeshBuild();
			ChangedParts.push_back(Model->GetPartData());
		}

		if (!ChangedParts.empty())
		{
			std::vector<swModel*> UpdatedModels;

			for (const std::unique_ptr<swModel>& Model : mModels)
			{
				Model->UpdatePartData(UpdatedModels);
				ChangedParts.push_back(Model->GetPartData());
			}

			Library->GetThumbnailManager()->RefreshParts(ChangedParts);
			swView::UpdateProjectViews(this);

			if (!mIsPreview && gMainWindow && gMainWindow->GetPreviewWidget())
				gMainWindow->GetPreviewWidget()->RefreshAfterAssetChange(ChangedParts);
		}

	}

	for (const std::unique_ptr<swModel>& Model : mModels)
		Model->SetSaved();

	mModified = false;

	return swResult<void>();
}

bool Project::Save(QTextStream& Stream) const
{
	const bool MPD = mModels.size() > 1 || !mEmbeddedData.empty();
	std::set<const swModel*> WrittenModels;
	std::set<const swEmbeddedData*> WrittenData;
	std::vector<const swModel*> OriginalOrder;

	const auto IsPresent = [this](const swModel* Model)
	{
		return std::any_of(mModels.begin(), mModels.end(), [Model](const std::unique_ptr<swModel>& Entry) { return Entry.get() == Model; });
	};

	for (const DocumentBlock& Block : mDocumentBlocks)
		if (Block.Model && !Block.DuplicateModel && IsPresent(Block.Model))
			OriginalOrder.push_back(Block.Model);

	bool Reordered = false;

	for (size_t Index = 0; Index < OriginalOrder.size(); Index++)
		Reordered |= OriginalOrder[Index] != mModels[Index].get();

	const auto OriginalFor = [this](const swModel* Model) -> const DocumentBlock*
	{
		for (const DocumentBlock& Block : mDocumentBlocks)
			if (Block.Model == Model && !Block.DuplicateModel)
				return &Block;

		return nullptr;
	};

	QByteArray Output;
	const auto Append = [&Output](const QByteArray& Bytes, bool NewBlock)
	{
		if (NewBlock && !Output.isEmpty() && !Output.endsWith('\n') && !Output.endsWith('\r'))
			Output += "\r\n";

		Output += Bytes;
	};

	size_t NextModel = 0;

	for (const DocumentBlock& Block : mDocumentBlocks)
	{
		if (Block.Model)
		{
			if (!IsPresent(Block.Model))
				continue;

			if (Block.DuplicateModel)
			{
				if (WrittenModels.find(Block.Model) != WrittenModels.end() && Block.Model->GetFileName().compare(Block.OriginalName, Qt::CaseInsensitive) == 0)
					Append(Block.Raw, true);

				continue;
			}

			const swModel* Model = Reordered ? mModels[NextModel++].get() : Block.Model;
			Append(SerializeStoredModel(Model, Model->GetFileName(), OriginalFor(Model), mModels.size() > 1, false, 0), true);
			WrittenModels.insert(Model);
		}
		else if (Block.Data)
		{
			Append(Block.Raw, true);
			WrittenData.insert(Block.Data.get());
		}
		else
			Append(Block.Raw, false);
	}

	for (const std::unique_ptr<swModel>& Model : mModels)
	{
		if (WrittenModels.find(Model.get()) != WrittenModels.end())
			continue;

		Append(SerializeStoredModel(Model.get(), Model->GetFileName(), OriginalFor(Model.get()), MPD || !mDocumentBlocks.empty(), false, 0), true);
	}

	for (const std::shared_ptr<const swEmbeddedData>& Data : mEmbeddedData)
		if (WrittenData.find(Data.get()) == WrittenData.end())
			Append(Data->RawBlock, true);

	return WriteRaw(Stream, Output);
}

std::vector<QString> Project::GetMergedAssetDirectories(const swModel* Model) const
{
	if (Model->GetProject() == this)
		return Model->GetAssetSearchDirectories();

	swModelAssetSourceState State = Model->GetAssetSourceState();
	const QString SourceFile = Model->GetProject()->GetFileName();
	const QString SourceDirectory = SourceFile.isEmpty() ? QString() : QFileInfo(SourceFile).absolutePath();

	if (!SourceDirectory.isEmpty())
	{
		State.Directories.erase(std::remove(State.Directories.begin(), State.Directories.end(), SourceDirectory), State.Directories.end());

		if (State.PreferCurrentDirectory)
			State.Directories.insert(State.Directories.begin(), SourceDirectory);
		else
			State.Directories.push_back(SourceDirectory);
	}

	const QString DestinationDirectory = mFileName.isEmpty() ? QString() : QFileInfo(mFileName).absolutePath();

	if (!DestinationDirectory.isEmpty() && std::find(State.Directories.begin(), State.Directories.end(), DestinationDirectory) == State.Directories.end())
		State.Directories.push_back(DestinationDirectory);

	return State.Directories;
}

swResult<void> Project::ValidateMergedTextureBindings(const Project& Source, const std::map<QString, std::shared_ptr<const swEmbeddedData>>& MergedData, const swModel* ClipboardRoot) const
{
	swPartsLibrary* Library = swGetPartsLibrary();

	for (const std::unique_ptr<swModel>& Model : Source.mModels)
	{
		if (Model.get() == ClipboardRoot)
			continue;

		std::vector<std::string> References = swMeshLoader::GetTextureReferences(SerializeModelText(Model.get(), false, 0));

		if (const swMesh* Mesh = Model->GetPartData()->GetMesh())
			for (const swMeshLod& Lod : Mesh->mLods)
				for (int SectionIndex = 0; SectionIndex < Lod.NumSections; SectionIndex++)
					if (!Lod.Sections[SectionIndex].TextureName.isEmpty())
						References.push_back(Lod.Sections[SectionIndex].TextureName.toLatin1().constData());

		const std::vector<QString> BeforeDirectories = Model->GetAssetSearchDirectories();
		const std::vector<QString> AfterDirectories = GetMergedAssetDirectories(Model.get());

		for (const std::string& Name : References)
		{
			swFileSource BeforeSource;
			swTextureReference Before = Library->AcquireTexture(Name.c_str(), BeforeDirectories, Source.mEmbeddedDataIndex, BeforeSource);

			if (!Before)
				continue;

			swFileSource AfterSource;
			swTextureReference After = Library->AcquireTexture(Name.c_str(), AfterDirectories, MergedData, AfterSource);
			bool Same = bool(After);

			if (Same && BeforeSource.EmbeddedData && AfterSource.EmbeddedData)
				Same = SameEmbeddedData(*BeforeSource.EmbeddedData, *AfterSource.EmbeddedData);
			else if (Same)
				Same = BeforeSource == AfterSource;

			if (!Same)
				return swUnexpected(tr("Merging would change texture '%1' used by model '%2'. Rename the conflicting texture before merging.").arg(QString::fromLatin1(Name.c_str()), Model->GetFileName()));
		}
	}

	return swResult<void>();
}

swResult<void> Project::Merge(const std::vector<Project*>& Sources)
{
	return MergeInternal(Sources, nullptr);
}

swResult<void> Project::MergeInternal(const std::vector<Project*>& Sources, const swModel* ClipboardRoot)
{
	swPartsLibrary* Library = swGetPartsLibrary();
	std::map<std::string, swPartData*> NonModelParts;
	std::set<std::string> DestinationModelNames;
	std::set<std::string> OriginalModelNames;
	std::set<std::string> PlannedModelNames;
	QString ConflictName;

	struct ModelPlan
	{
		swModel* Model;
		QString FinalName;
	};

	std::vector<ModelPlan> PlannedModels;
	std::map<QString, std::shared_ptr<const swEmbeddedData>> MergedData = mEmbeddedDataIndex;

	// Validate every incoming asset before reserving names or moving models.
	for (const Project* Source : Sources)
	{
		for (const auto& [Name, Data] : Source->mEmbeddedDataIndex)
		{
			const auto Existing = MergedData.find(Name);

			if (Existing != MergedData.end() && !SameEmbeddedData(*Existing->second, *Data))
				return swUnexpected(tr("Both models contain different embedded data named '%1'. Rename one before merging.").arg(Data->FileName));

			MergedData.emplace(Name, Data);
		}
	}

	const swResult<void> DestinationBindings = ValidateMergedTextureBindings(*this, MergedData, ClipboardRoot);

	if (!DestinationBindings)
		return DestinationBindings;

	for (const Project* Source : Sources)
	{
		const swResult<void> SourceBindings = ValidateMergedTextureBindings(*Source, MergedData, ClipboardRoot);

		if (!SourceBindings)
			return SourceBindings;
	}


	for (const std::map<std::string, swPartData*>::value_type& Entry : mPartIndex)
	{
		if (Entry.second->IsModel())
			DestinationModelNames.insert(Entry.first);
		else
			NonModelParts.emplace(Entry.first, Entry.second);
	}

	for (const std::unique_ptr<swModel>& Model : mModels)
	{
		DestinationModelNames.insert(swPartsLibrary::NormalizePartDataName(Model->GetFileName().toLatin1().constData()));

		for (const std::unique_ptr<swPart>& Part : Model->GetParts())
		{
			if (Part->mPartData && !Part->mPartData->IsModel())
				NonModelParts.emplace(swPartsLibrary::NormalizePartDataName(Part->GetID().toLatin1().constData()), Part->mPartData.Get());
		}
	}

	const auto RegisterSourcePart = [&NonModelParts, &DestinationModelNames, &ConflictName](const std::string& Name, swPartData* PartData)
	{
		if (!PartData || PartData->IsModel())
			return true;

		if (DestinationModelNames.find(Name) != DestinationModelNames.end())
		{
			ConflictName = QString::fromStdString(Name);
			return false;
		}

		const std::map<std::string, swPartData*>::const_iterator Existing = NonModelParts.find(Name);

		if (Existing != NonModelParts.end() && !CanShareMergePartData(Existing->second, PartData))
		{
			ConflictName = QString::fromStdString(Name);
			return false;
		}

		NonModelParts.emplace(Name, PartData);
		return true;
	};

	// Validate the identities behind every part name before changing any project.
	for (Project* Source : Sources)
	{
		for (const std::map<std::string, swPartData*>::value_type& Entry : Source->mPartIndex)
		{
			if (!RegisterSourcePart(Entry.first, Entry.second))
				return swUnexpected(tr("Both models contain different parts named '%1'. Rename one before merging.").arg(ConflictName));
		}

		for (const std::unique_ptr<swModel>& Model : Source->mModels)
		{
			OriginalModelNames.insert(swPartsLibrary::NormalizePartDataName(Model->GetFileName().toLatin1().constData()));

			for (const std::unique_ptr<swPart>& Part : Model->GetParts())
			{
				const std::string Name = swPartsLibrary::NormalizePartDataName(Part->GetID().toLatin1().constData());

				if (!RegisterSourcePart(Name, Part->mPartData.Get()))
					return swUnexpected(tr("Both models contain different parts named '%1'. Rename one before merging.").arg(ConflictName));
			}
		}
	}

	// Pick final model names against the complete set of incoming part names.
	for (Project* Source : Sources)
	{
		for (const std::unique_ptr<swModel>& Model : Source->mModels)
		{
			const QString OriginalName = Model->GetFileName();
			QString FinalName = OriginalName;
			int RenameIndex = 0;

			for (;;)
			{
				const std::string Name = swPartsLibrary::NormalizePartDataName(FinalName.toLatin1().constData());
				const bool ReservedOriginal = FinalName != OriginalName && OriginalModelNames.find(Name) != OriginalModelNames.end();
				const bool Duplicate = DestinationModelNames.find(Name) != DestinationModelNames.end() || NonModelParts.find(Name) != NonModelParts.end() ||
					PlannedModelNames.find(Name) != PlannedModelNames.end() || ReservedOriginal ||
					Library->FindPartData(FinalName.toLatin1().constData(), nullptr) != nullptr;

				if (!Duplicate)
				{
					PlannedModelNames.insert(Name);
					PlannedModels.push_back({ Model.get(), FinalName });
					break;
				}

				RenameIndex++;
				const QString Prefix = RenameIndex == 1 ? tr("Merged ") : tr("Merged %1 ").arg(RenameIndex);
				FinalName = QString::fromLatin1((Prefix + OriginalName).toLatin1().left(SW_PART_NAME_LENGTH - 1));
			}
		}
	}

	// Reserve all planned names before moving models, so a failed reservation
	// can be rolled back without leaving either project partly merged.
	std::vector<const ModelPlan*> AppliedRenames;

	for (const ModelPlan& Planned : PlannedModels)
	{
		if (Planned.FinalName == Planned.Model->GetFileName())
			continue;

		if (!Library->RenameModelPartData(Planned.Model->GetPartData(), Planned.FinalName.toLatin1().constData()))
		{
			for (std::vector<const ModelPlan*>::reverse_iterator It = AppliedRenames.rbegin(); It != AppliedRenames.rend(); It++)
				Library->RenameModelPartData((*It)->Model->GetPartData(), (*It)->Model->GetFileName().toLatin1().constData());

			return swUnexpected(tr("Could not reserve submodel name '%1'.").arg(Planned.FinalName));
		}

		AppliedRenames.push_back(&Planned);
	}

	mEmbeddedDataIndex = std::move(MergedData);

	size_t PlanIndex = 0;

	for (Project* Source : Sources)
	{
		std::vector<swModelPartRename> Renames;
		std::vector<swModel*> ImportedModels;

		for (const std::unique_ptr<swModel>& Model : Source->mModels)
		{
			for (const std::unique_ptr<swPart>& Part : Model->GetParts())
			{
				swPartData* Incoming = Part->mPartData.Get();

				if (!Incoming || Incoming->IsModel())
					continue;

				const std::string Name = swPartsLibrary::NormalizePartDataName(Part->GetID().toLatin1().constData());
				swPartData* Existing = FindPartData(Name);

				if (!Existing || Existing == Incoming || !CanShareMergePartData(Existing, Incoming))
					continue;

				const QString PartId = Part->GetID();
				Part->SetPartData(swPartDataReference(Existing), PartId, false, false);
			}
		}

		for (std::unique_ptr<swModel>& Model : Source->mModels)
		{
			const ModelPlan& Planned = PlannedModels[PlanIndex++];

			if (Planned.FinalName != Model->GetFileName())
			{
				Renames.push_back({ Model->GetPartData(), Model->GetFileName(), Planned.FinalName });
				Model->SetFileName(Planned.FinalName);
			}

			Model->SetProject(this);
			ImportedModels.push_back(Model.get());
			mModels.emplace_back(std::move(Model));
		}

		// Only incoming histories used these original names for the incoming models.
		for (swModel* Model : ImportedModels)
			Model->RenameParts(Renames);

		Library->TransferProjectPartData(Source, this);
		Source->mModels.clear();
		mEmbeddedData.insert(mEmbeddedData.end(), Source->mEmbeddedData.begin(), Source->mEmbeddedData.end());
		mDocumentBlocks.insert(mDocumentBlocks.end(), Source->mDocumentBlocks.begin(), Source->mDocumentBlocks.end());
		for (const LoadWarning& Warning : Source->mLoadWarnings)
			AddLoadWarning(Warning);

		Source->mEmbeddedData.clear();
		Source->mEmbeddedDataIndex.clear();
		Source->mDocumentBlocks.clear();

	}

	if (!Sources.empty())
		mModified = true;

	return swResult<void>();
}

bool Project::ImportLDD(const QString& FileName)
{
	swZipFile ZipFile;

	if (!ZipFile.OpenRead(FileName))
		return false;

	swMemoryFile XMLFile;
	if (!ZipFile.ExtractFile("IMAGE100.LXFML", XMLFile))
		return false;

	QString ModelName = QFileInfo(FileName).completeBaseName();
	std::unique_ptr<swModel> NewModel(new swModel(ModelName, this, false));

	if (!NewModel->LoadLDD(QString::fromUtf8((const char*)XMLFile.mBuffer)))
		return false;

	NewModel->SetSaved();

	mModels.clear();
	mDocumentBlocks.clear();
	mEmbeddedData.clear();
	mEmbeddedDataIndex.clear();
	mModels.emplace_back(std::move(NewModel));

	for (const std::unique_ptr<swModel>& Model : mModels)
		Model->CreatePartData(this);

	std::vector<swModel*> UpdatedModels;
	UpdatedModels.reserve(mModels.size());

	for (const std::unique_ptr<swModel>& Model : mModels)
		Model->UpdatePartData(UpdatedModels);

	mModified = false;

	return true;
}

bool Project::ImportInventory(const std::vector<swSetInventoryItem>& SetInventory, const QString& Name, const QString& Description)
{
	if (SetInventory.empty())
		return false;

	std::unique_ptr<swModel> NewModel(new swModel(Name, this, false));

	if (!NewModel->LoadInventory(SetInventory))
		return false;

	NewModel->SetSaved();
	NewModel->SetDescription(Description);

	mModels.clear();
	mDocumentBlocks.clear();
	mEmbeddedData.clear();
	mEmbeddedDataIndex.clear();
	mModels.emplace_back(std::move(NewModel));

	for (const std::unique_ptr<swModel>& Model : mModels)
		Model->CreatePartData(this);

	std::vector<swModel*> UpdatedModels;
	UpdatedModels.reserve(mModels.size());

	for (const std::unique_ptr<swModel>& Model : mModels)
		Model->UpdatePartData(UpdatedModels);

	mModified = false;

	return true;
}

std::vector<swModelPartsEntry> Project::GetModelParts()
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

	// Export completed geometry without changing which models are being edited.
	swScopedModelSteps ModelSteps;
	ModelSteps.States.reserve(mModels.size());

	for (const std::unique_ptr<swModel>& Model : mModels)
	{
		ModelSteps.States.emplace_back(Model.get(), Model->IsActive());
		Model->CalculateStep(SW_STEP_MAXIMUM);
	}

	mModels[0]->GetModelParts(swMatrix44Identity(), gDefaultColor, ModelParts);

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

swResult<QByteArray> Project::SerializeModel(const swModel* Root, bool SelectedOnly, swStep LastStep) const
{
	struct ExportEntry
	{
		const swModel* Model;
		QString Name;
		bool SelectedOnly;
	};

	std::vector<ExportEntry> Models;
	std::map<QString, const swModel*> Names;
	std::vector<std::shared_ptr<const swEmbeddedData>> Assets;
	std::map<QString, std::shared_ptr<const swEmbeddedData>> AssetLookup;

	std::function<swResult<void>(const swModel*, const QString&, bool)> Collect = [&](const swModel* Model, const QString& Name, bool FilterSelection) -> swResult<void>
	{
		const QString Key = NormalizeEmbeddedDataName(Name);
		const auto Existing = Names.find(Key);

		if (Existing != Names.end())
		{
			if (Existing->second != Model)
				return swUnexpected(tr("Cannot export different submodels named '%1'.").arg(Name));

			return swResult<void>();
		}

		Names.emplace(Key, Model);
		Models.push_back({ Model, Name, FilterSelection });

		for (const std::unique_ptr<swPart>& Part : Model->GetParts())
		{
			if ((FilterSelection && !Part->IsSelected()) || (LastStep && Part->GetStepShow() > LastStep))
				continue;

			const swPartData* PartData = Part->mPartData.Get();
			const swModel* Child = nullptr;

			if (PartData && PartData->IsModel())
				Child = PartData->GetModel();
			else if (PartData && PartData->IsProject())
				Child = PartData->GetProject()->GetMainModel();

			if (Child)
			{
				const swResult<void> Result = Collect(Child, Part->GetID(), false);

				if (!Result)
					return Result;
			}
		}

		return swResult<void>();
	};

	const swResult<void> Collected = Collect(Root, Root->GetFileName(), SelectedOnly);

	if (!Collected)
		return swUnexpected(Collected.error());

	// A complete model export can retain the original document layout too.
	bool WholeProject = !SelectedOnly && !LastStep && Root == GetMainModel() && Models.size() == mModels.size();

	for (const ExportEntry& Entry : Models)
		WholeProject &= Entry.Model->GetProject() == this && NormalizeEmbeddedDataName(Entry.Name) == NormalizeEmbeddedDataName(Entry.Model->GetFileName());

	if (WholeProject)
	{
		QByteArray Bytes;
		QTextStream Stream(&Bytes, QIODevice::WriteOnly);

		if (!Save(Stream))
			return swUnexpected(tr("Could not serialize all model data."));

		Stream.flush();
		return Bytes;
	}

	std::vector<const Project*> Owners;
	std::map<const Project*, std::set<QString>> RequiredNames;

	for (const ExportEntry& Entry : Models)
	{
		const Project* Owner = Entry.Model->GetProject();

		if (!Owner)
			continue;

		if (RequiredNames.find(Owner) == RequiredNames.end())
			Owners.push_back(Owner);

		std::set<QString>& Required = RequiredNames[Owner];
		const std::vector<std::string> References = swMeshLoader::GetTextureReferences(SerializeModelText(Entry.Model, Entry.SelectedOnly, LastStep));

		for (const std::string& Name : References)
		{
			const QString Bare = QString::fromLatin1(Name.c_str()) + QLatin1String(".png");
			const QString Candidates[] = { QLatin1String("textures/") + Bare, Bare };

			for (const QString& Candidate : Candidates)
			{
				const std::shared_ptr<const swEmbeddedData> Data = Owner->FindEmbeddedData(Candidate);

				if (!Data)
					continue;

				const QString Key = NormalizeEmbeddedDataName(Data->FileName);
				const auto Existing = AssetLookup.find(Key);

				if (Existing != AssetLookup.end() && !SameEmbeddedData(*Existing->second, *Data))
					return swUnexpected(tr("Cannot export different embedded data named '%1'.").arg(Data->FileName));

				AssetLookup.emplace(Key, Data);
				Required.insert(Key);
			}
		}
	}

	for (const Project* Owner : Owners)
		for (const std::shared_ptr<const swEmbeddedData>& Data : Owner->mEmbeddedData)
			if (RequiredNames[Owner].find(NormalizeEmbeddedDataName(Data->FileName)) != RequiredNames[Owner].end())
				Assets.push_back(Data);

	const bool MPD = Models.size() > 1 || !Assets.empty();
	QByteArray Output;
	const auto Append = [&Output](const QByteArray& Bytes)
	{
		if (!Output.isEmpty() && !Output.endsWith('\n') && !Output.endsWith('\r'))
			Output += "\r\n";

		Output += Bytes;
	};

	for (const ExportEntry& Entry : Models)
	{
		const DocumentBlock* Original = nullptr;
		const Project* Owner = Entry.Model->GetProject();

		if (Owner)
			for (const DocumentBlock& Block : Owner->mDocumentBlocks)
				if (Block.Model == Entry.Model && !Block.DuplicateModel)
				{
					Original = &Block;
					break;
				}

		Append(SerializeStoredModel(Entry.Model, Entry.Name, Original, MPD, Entry.SelectedOnly, LastStep));
	}

	for (const std::shared_ptr<const swEmbeddedData>& Data : Assets)
		Append(Data->RawBlock);

	if (SelectedOnly)
		Output.prepend("0 !SW PROJECT CLIPBOARD " + mClipboardIdentity.toString().toUtf8() + "\r\n");

	return Output;
}

swResult<swModel*> Project::ImportClipboard(const QByteArray& FileData)
{
	Project Source(mIsPreview);
	Source.mModels.clear();
	swGetPartsLibrary()->ReleaseProjectPartData(&Source);
	Source.SetDeferModelMeshRequests(true);
	Source.mFileName = mFileName;
	const QByteArray Marker("0 !SW PROJECT CLIPBOARD ");
	const QByteArray FirstLine = FileData.left(FileData.indexOf('\n') == -1 ? FileData.size() : FileData.indexOf('\n')).trimmed();
	const bool SameProject = FirstLine.startsWith(Marker) && QUuid(QString::fromUtf8(FirstLine.mid(Marker.size()))) == mClipboardIdentity;
	Source.LoadLDrawDocument(FileData, this, SameProject);

	if (Source.mModels.empty())
		return swUnexpected(tr("Clipboard contains no model."));

	swModel* Root = Source.GetMainModel();

	for (const std::unique_ptr<swModel>& Model : Source.mModels)
		Model->QueueMeshBuild();

	if (Root->GetParts().empty())
		return static_cast<swModel*>(nullptr);

	if (SameProject)
	{
		std::set<const swModel*> Reachable;
		std::function<void(const swModel*)> Collect = [&](const swModel* Model)
		{
			if (!Reachable.insert(Model).second)
				return;

			for (const std::unique_ptr<swPart>& Part : Model->GetParts())
				if (Part->mPartData->IsModel() && Part->mPartData->GetModel()->GetProject() == &Source)
					Collect(Part->mPartData->GetModel());
		};
		Collect(Root);

		Source.mModels.erase(std::remove_if(Source.mModels.begin(), Source.mModels.end(), [&Reachable](const std::unique_ptr<swModel>& Model)
		{
			return Reachable.find(Model.get()) == Reachable.end();
		}), Source.mModels.end());

		// Live reused models already own their current assets. Keep snapshots only
		// for definitions that were missing and therefore still need importing.
		std::set<QString> Required;

		for (const std::unique_ptr<swModel>& Model : Source.mModels)
		{
			if (Model.get() == Root)
				continue;

			for (const std::string& Name : swMeshLoader::GetTextureReferences(SerializeModelText(Model.get(), false, 0)))
			{
				const QString Bare = QString::fromLatin1(Name.c_str()) + QLatin1String(".png");
				Required.insert(NormalizeEmbeddedDataName(Bare));
				Required.insert(NormalizeEmbeddedDataName(QLatin1String("textures/") + Bare));
			}
		}

		Source.mEmbeddedData.erase(std::remove_if(Source.mEmbeddedData.begin(), Source.mEmbeddedData.end(), [&Required](const std::shared_ptr<const swEmbeddedData>& Data)
		{
			return Required.find(NormalizeEmbeddedDataName(Data->FileName)) == Required.end();
		}), Source.mEmbeddedData.end());

		for (auto Entry = Source.mEmbeddedDataIndex.begin(); Entry != Source.mEmbeddedDataIndex.end();)
			if (Required.find(Entry->first) == Required.end())
				Entry = Source.mEmbeddedDataIndex.erase(Entry);
			else
				++Entry;
	}

	// Clipboard formatting is generated; only retain the actual assets and submodels.
	Source.mDocumentBlocks.clear();
	const swResult<void> Result = MergeInternal({ &Source }, Root);

	if (!Result)
		return swUnexpected(Result.error());

	const auto Entry = std::find_if(mModels.begin(), mModels.end(), [Root](const std::unique_ptr<swModel>& Model) { return Model.get() == Root; });
	std::unique_ptr<swModel> Wrapper = std::move(*Entry);
	mModels.erase(Entry);

	return Wrapper.release();
}

swInstructions* Project::GetInstructions()
{
	mInstructions.reset();
	mInstructions = std::unique_ptr<swInstructions>(new swInstructions(this));

	return mInstructions.get();
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

	// Reserialize models instead of keeping their text.
	for (DocumentBlock& Block : mDocumentBlocks)
		Block.Baseline.clear();
}
