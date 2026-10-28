#include "sw_global.h"
#include "sw_widget_preview.h"
#include "sw_object_part.h"
#include "sw_data_part.h"
#include "sw_project.h"
#include "sw_window_main.h"
#include "sw_library.h"
#include "sw_manager_thumbnail.h"
#include "sw_file.h"
#include "sw_command_meta.h"

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

	return LoadDocument(FileData, FileName, ShowErrors);
}

bool Project::LoadText(const QByteArray& FileData, const QString& FileName, const QString& ModelName)
{
	// an unsaved project names its model like the one the text replaces
	if (!LoadDocument(FileData, FileName.isEmpty() ? ModelName : FileName, false))
		return false;

	SetFileName(FileName);

	// the file on disk stays what the watcher compares with, the text is an unsaved change
	if (!mIsPreview && !FileName.isEmpty())
	{
		const FileState State = ReadFileState(FileName);

		mFileHash = State.Hash;
		mFileExists = State.Exists;
		mFileReadable = State.Readable;
	}

	mModified = true;

	return true;
}

bool Project::LoadDocument(const QByteArray& FileData, const QString& FileName, bool ShowErrors)
{
	QWidget *parent = nullptr;
	if (!mIsPreview)
		parent = gMainWindow;

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
