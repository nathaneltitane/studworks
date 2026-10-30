#include "sw_global.h"
#include "sw_object_part.h"
#include "sw_data_part.h"
#include "sw_project.h"
#include "sw_window_main.h"
#include "sw_library.h"
#include "sw_file.h"
#include "sw_file_zip.h"

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

	// validate incoming assets first
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

	// validate part name identities first
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

	// pick final model names
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

	// reserve names before moving models
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

		// original names of incoming histories
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

		// keep snapshots of missing definitions
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

	// keep only assets and submodels
	Source.mDocumentBlocks.clear();
	const swResult<void> Result = MergeInternal({ &Source }, Root);

	if (!Result)
		return swUnexpected(Result.error());

	const auto Entry = std::find_if(mModels.begin(), mModels.end(), [Root](const std::unique_ptr<swModel>& Model) { return Model.get() == Root; });
	std::unique_ptr<swModel> Wrapper = std::move(*Entry);
	mModels.erase(Entry);

	return Wrapper.release();
}
