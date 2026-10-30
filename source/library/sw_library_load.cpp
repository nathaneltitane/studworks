#include "sw_global.h"
#include "sw_library.h"
#include "sw_loader_asset.h"
#include "sw_file_zip.h"
#include "sw_file.h"
#include "sw_data_part.h"
#include "sw_manager_part.h"
#include "sw_texture.h"
#include "sw_synthesis.h"
#include "sw_project.h"
#include "sw_model.h"

void swPartsLibrary::RequestPartDataLoad(swPartData* PartData, swLoadPriority LoadPriority)
{
	Q_ASSERT(QThread::currentThread() == thread());

	if (PartData->IsProject() || (PartData->IsModel() && !PartData->GetModel()->HasDirectGeometry()))
	{
		PartData->mState = swAssetState::Loaded;
		ClearPartDataLoadError(PartData);
		return;
	}

	if (PartData->mState == swAssetState::Unloaded)
	{
		PartData->mState = swAssetState::Loading;
		ClearPartDataLoadError(PartData);

		const swBoundingBox& Box = GetLoadingMesh()->mBoundingBox;

		if (!PartData->IsModel())
			PartData->SetBoundingBox(Box.Min, Box.Max);
		else
		{
			const swBoundingBox& ModelBox = PartData->GetBoundingBox();
			PartData->SetBoundingBox(swMinimum(ModelBox.Min, Box.Min), swMaximum(ModelBox.Max, Box.Max));
		}
	}

	if (PartData->IsLoading())
		mAssetLoader->RequestPartLoad(PartData, LoadPriority);
}

void swPartsLibrary::PartDataLoadCancelled(swPartData* PartData)
{
	Q_ASSERT(QThread::currentThread() == thread());

	PartData->mState = swAssetState::Unloaded;
	ClearPartDataLoadError(PartData);
}

void swPartsLibrary::NotifyAssetStatusChanged()
{
	emit AssetStatusChanged();
}

bool swPartsLibrary::EnsurePartDataAssetsReady(const swPartDataReference& PartData)
{
	const bool PartReady = WaitForPartData(PartData);

	if (PartData->IsModel())
		return static_cast<bool>(PartData->GetModel()->EnsureAssetsReady()) && PartReady;

	if (PartData->IsProject())
	{
		const bool ProjectReady = static_cast<bool>(PartData->GetProject()->EnsureAssetsReady());

		PartData->GetProject()->UpdatePartData(PartData.Get());
		return ProjectReady && PartReady;
	}

	return PartReady;
}

bool swPartsLibrary::WaitForPartData(const swPartDataReference& PartDataRef)
{
	Q_ASSERT(QThread::currentThread() == thread());

	if (PartDataRef->mState == swAssetState::Unloaded)
		RequestPartDataLoad(PartDataRef.Get(), swLoadPriority::Blocking);

	const bool RequestsSucceeded = mAssetLoader->WaitForParts({ PartDataRef.Get() });

	return RequestsSucceeded && PartDataRef->mState == swAssetState::Loaded;
}

bool swPartsLibrary::WaitForPartData(const std::vector<swPartDataReference>& PartDataRefs, const std::function<void(int, int)>& Progress)
{
	Q_ASSERT(QThread::currentThread() == thread());

	std::vector<swPartData*> Parts;
	Parts.reserve(PartDataRefs.size());

	for (const swPartDataReference& PartDataRef : PartDataRefs)
	{
		if (PartDataRef->mState == swAssetState::Unloaded)
			RequestPartDataLoad(PartDataRef.Get(), swLoadPriority::Blocking);

		Parts.push_back(PartDataRef.Get());
	}

	const bool RequestsSucceeded = mAssetLoader->WaitForParts(Parts, Progress);

	for (const swPartDataReference& PartDataRef : PartDataRefs)
		if (PartDataRef->mState != swAssetState::Loaded)
			return false;

	return RequestsSucceeded;
}

void swPartsLibrary::RequestSynthMesh(swPart* Part)
{
	Q_ASSERT(QThread::currentThread() == thread());

	swPartData* PartData = Part->mPartData.Get();
	std::shared_ptr<const swSynthesisInformation> SynthInfo = PartData ? PartData->GetSynthInfoShared() : nullptr;

	if (!SynthInfo)
	{
		CancelSynthMesh(Part);
		return;
	}

	mSynthParts.insert_or_assign(Part, QString());

	mAssetLoader->RequestSynthMesh(Part, PartData, std::move(SynthInfo), Part->GetControlPoints());
}

void swPartsLibrary::CancelSynthMesh(swPart* Part)
{
	Q_ASSERT(QThread::currentThread() == thread());

	mSynthParts.erase(Part);

	if (mAssetLoader)
		mAssetLoader->CancelSynthMesh(Part);
}

bool swPartsLibrary::WaitForSynthMeshes(const std::vector<swPart*>& Parts)
{
	Q_ASSERT(QThread::currentThread() == thread());

	const bool RequestsSucceeded = mAssetLoader->WaitForSynthMeshes(Parts);

	for (const swPart* Part : Parts)
		if (Part->mPartData && Part->mPartData->GetSynthInfo() && !Part->HasGeneratedMesh())
			return false;

	return RequestsSucceeded;
}

QString swPartsLibrary::GetSynthMeshError(const swPart* Part) const
{
	Q_ASSERT(QThread::currentThread() == thread());

	const auto It = mSynthParts.find(const_cast<swPart*>(Part));

	return It != mSynthParts.end() ? It->second : QString();
}

bool swPartsLibrary::PublishPartDataMesh(swPartData* PartData, std::unique_ptr<swMesh> Mesh, QString Error)
{
	Q_ASSERT(QThread::currentThread() == thread());

	const bool Loaded = Error.isEmpty();

	if (Loaded)
	{
		if (Mesh)
			PartData->SetMesh(Mesh.release());

		PartData->mState = swAssetState::Loaded;
		ClearPartDataLoadError(PartData);
	}
	else
		PartData->SetFailed(std::move(Error));

	ScheduleBufferRepack();

	return Loaded;
}

bool swPartsLibrary::PublishSynthMesh(swPart* Part, std::unique_ptr<swMesh> Mesh, QString Error)
{
	Q_ASSERT(QThread::currentThread() == thread());

	const bool Loaded = Error.isEmpty() && Mesh;

	if (!Loaded && Error.isEmpty())
		Error = tr("Could not generate geometry for part '%1'.").arg(QString::fromLatin1(Part->mPartData->mFileName));

	Part->SetGeneratedMesh(Loaded ? Mesh.release() : nullptr);

	const auto It = mSynthParts.find(Part);

	if (It != mSynthParts.end())
		It->second = Loaded ? QString() : std::move(Error);

	return Loaded;
}

void swPartsLibrary::PublishTexture(swTexture* Texture, swTextureBuildResult Result)
{
	Q_ASSERT(QThread::currentThread() == thread());

	if (!Result.DecodedImage.isNull())
		Texture->SetImage(std::move(Result.DecodedImage), SW_TEXTURE_MIPMAPS);
	else
		Texture->SetFailed(Result.Error, std::move(Result.ErrorDetails));
}

void swPartsLibrary::RebuildModelPartData(swPartData* PartData)
{
	Q_ASSERT(QThread::currentThread() == thread());
	Q_ASSERT(PartData->IsModel());

	InvalidatePartDataLoad(PartData);
	PartData->ReleaseMesh();
	RequestPartDataLoad(PartData, swLoadPriority::Normal);
}

void swPartsLibrary::InvalidatePartDataLoad(swPartData* PartData)
{
	Q_ASSERT(QThread::currentThread() == thread());

	PartData->mState = swAssetState::Unloaded;
	ClearPartDataLoadError(PartData);

	if (mAssetLoader)
		mAssetLoader->InvalidatePartLoad(PartData);
}

void swPartsLibrary::ReleasePartData(swPartData* PartData)
{
	const int Remaining = mPartManager->ReleasePartData(PartData);

	if (Remaining == 1 && mAssetLoader)
		mAssetLoader->CancelPartLoad(PartData);
}

void swPartsLibrary::AddPartDataReference(swPartData* PartData)
{
	mPartManager->AddPartDataReference(PartData);
}

void swPartsLibrary::SetPartDataLoadError(const swPartData* PartData, QString Error)
{
	mPartManager->SetPartDataLoadError(PartData, std::move(Error));
}

QString swPartsLibrary::GetPartDataLoadError(const swPartData* PartData) const
{
	return mPartManager->GetPartDataLoadError(PartData);
}

void swPartsLibrary::ClearPartDataLoadError(const swPartData* PartData)
{
	mPartManager->ClearPartDataLoadError(PartData);
}

swPartSourceSnapshot swPartsLibrary::SnapshotPartDataSource(const swPartData* PartData) const
{
	swPartSourceSnapshot Source;

	Source.FileName = QString::fromLatin1(PartData->mFileName);
	Source.LibraryDirectory = mLibraryDir.absolutePath();
	Source.CachePath = QFileInfo(QDir(mCachePath), Source.FileName).absoluteFilePath();
	Source.ZipFileType = PartData->mZipFileType;
	Source.ZipFileIndex = PartData->mZipFileIndex;
	Source.StudStyle = mStudStyle;
	Source.StudCylinderColorEnabled = mStudCylinderColorEnabled;

	if (PartData->IsModel())
	{
		Source.InlineModel = true;
		Source.InlineMeshLines = PartData->GetModel()->GetFileLines();

		const swModel* Model = PartData->GetModel();
		Source.TextureSearchDirectories = Model->GetAssetSearchDirectories();

		if (Model->GetProject())
			Source.EmbeddedData = Model->GetProject()->GetEmbeddedDataLookup();
	}

	return Source;
}

swPartBuildResult swPartsLibrary::BuildPartData(const swPartSourceSnapshot& Source)
{
	swPartBuildResult Result;

	if (mCancelLoading.load())
	{
		Result.Error = tr("Library loading was canceled.");
		return Result;
	}

	if (!Source.InlineModel && Source.ZipFileType != swZipFileType::Count && mZipFiles[static_cast<int>(Source.ZipFileType)])
	{
		swMemoryFile CacheData;

		if (ReadCacheFile(Source.CachePath, CacheData, CacheType::Archive))
		{
			qint32 Flags;

			if (CacheData.ReadBuffer(&Flags, sizeof(Flags)) == sizeof(Flags) && Flags == MeshCacheSettingsKey(Source.StudStyle, Source.StudCylinderColorEnabled))
			{
				Result.Mesh.reset(new swMesh);

				if (Result.Mesh->FileLoad(CacheData))
				{
					Result.FromCache = true;
					return Result;
				}

				Result.Mesh.reset();
			}
		}
	}

	swLibraryMeshData MeshData;
	swMeshLoader MeshLoader(MeshData, nullptr, swMeshLoaderFlag::Optimize | swMeshLoaderFlag::RequireAllIncludes);

	if (Source.InlineModel)
	{
		swMemoryFile PartFile;

		for (const QString& Line : Source.InlineMeshLines)
		{
			const QByteArray Buffer = Line.toLatin1();

			PartFile.WriteBuffer(Buffer.constData(), Buffer.size());
			PartFile.WriteBuffer("\r\n", 2);
		}

		PartFile.Seek(0, SEEK_SET);

		if (!MeshLoader.LoadMesh(PartFile, SW_MESH_DATA_SHARED))
		{
			Result.Error = tr("Could not load model geometry '%1'.").arg(Source.FileName);

			if (!MeshLoader.GetError().isEmpty())
				Result.Error += QLatin1Char('\n') + MeshLoader.GetError();

			return Result;
		}

		if (MeshData.IsEmpty())
		{
			Result.EmptyGeometry = true;
			return Result;
		}
	}
	else
	{
		bool Loaded = false;

		if (Source.ZipFileType != swZipFileType::Count && mZipFiles[static_cast<int>(Source.ZipFileType)])
		{
			swFileSource FileSource;
			FileSource.ArchiveType = static_cast<int>(Source.ZipFileType);
			FileSource.ArchiveIndex = Source.ZipFileIndex;
			const swResult<std::unique_ptr<swFile>> File = OpenFile(FileSource);

			if (File)
				Loaded = MeshLoader.LoadMesh(*File.value(), SW_MESH_DATA_SHARED);
		}
		else
		{
			swFileSource FileSource;
			FileSource.FilePath = QDir(Source.LibraryDirectory).absoluteFilePath(QStringLiteral("parts/") + Source.FileName);
			const swResult<std::unique_ptr<swFile>> File = OpenFile(FileSource);

			if (File)
				Loaded = MeshLoader.LoadMesh(*File.value(), SW_MESH_DATA_SHARED);

			// retry unofficial geometry on failure
			if (mHasUnofficial && !Loaded)
			{
				MeshData.Clear();

				FileSource.FilePath = QDir(Source.LibraryDirectory).absoluteFilePath(QStringLiteral("unofficial/parts/") + Source.FileName);
				const swResult<std::unique_ptr<swFile>> UnofficialFile = OpenFile(FileSource);

				if (UnofficialFile)
					Loaded = MeshLoader.LoadMesh(*UnofficialFile.value(), SW_MESH_DATA_SHARED);
			}
		}

		if (mCancelLoading.load())
			Result.Error = tr("Library loading was canceled.");
		else if (!Loaded)
		{
			Result.Error = tr("Could not load part '%1'.").arg(Source.FileName);

			if (!MeshLoader.GetError().isEmpty())
				Result.Error += QLatin1Char('\n') + MeshLoader.GetError();
		}

		if (!Result.Error.isEmpty())
			return Result;
	}

	Result.Mesh.reset(MeshData.CreateMesh(nullptr));

	if (!Result.Mesh)
		Result.Error = tr("Could not prepare part '%1' for rendering.").arg(Source.FileName);

	return Result;
}

void swPartsLibrary::SaveBuiltPartDataCache(const swPartSourceSnapshot& Source, swMesh& Mesh)
{
	if (Source.ZipFileType == swZipFileType::Count)
		return;

	swMemoryFile MeshData;

	const qint32 Flags = MeshCacheSettingsKey(Source.StudStyle, Source.StudCylinderColorEnabled);

	if (MeshData.WriteBuffer((char*)&Flags, sizeof(Flags)) != sizeof(Flags) || !Mesh.FileSave(MeshData))
		return;

	WriteCacheFile(Source.CachePath, MeshData, CacheType::Archive);
}

swResult<std::unique_ptr<swFile>> swPartsLibrary::OpenPrimitiveFile(const swLibraryPrimitive* Primitive)
{
	swFileSource Source;

	if (Primitive->mZipFileType != swZipFileType::Count)
	{
		Source.ArchiveType = static_cast<int>(Primitive->mZipFileType);
		Source.ArchiveIndex = Primitive->mZipFileIndex;
	}
	else
		Source.FilePath = Primitive->mFileName;

	return OpenFile(Source);
}

swResult<std::unique_ptr<swFile>> swPartsLibrary::OpenPartDataFile(const char* PartName)
{
	const std::optional<swPartManager::FileInfo> FileInfo = mPartManager->GetPartDataFileInfo(PartName);

	if (FileInfo && FileInfo->ZipFileType != swZipFileType::Count)
	{
		swFileSource Source;
		Source.ArchiveType = static_cast<int>(FileInfo->ZipFileType);
		Source.ArchiveIndex = FileInfo->ZipFileIndex;
		return OpenFile(Source);
	}

	// use to find an include in the archives
	const auto OpenArchiveFile = [this, PartName]() -> swResult<std::unique_ptr<swFile>>
	{
		for (const swZipFileType Type : { swZipFileType::Official, swZipFileType::Unofficial })
		{
			const std::unique_ptr<swZipFile>& Archive = mZipFiles[static_cast<int>(Type)];

			if (!Archive)
				continue;

			const QString Prefix = Type == swZipFileType::Official ? QStringLiteral("ldraw/") : QString();

			for (const QString& Folder : { QStringLiteral("parts/"), QStringLiteral("p/") })
			{
				const QByteArray FileName = (Prefix + Folder + QString::fromLatin1(PartName)).toLatin1();
				const std::optional<quint32> FileIndex = Archive->FindFileIndex(FileName.constData());

				if (!FileIndex)
					continue;

				swFileSource Source;
				Source.ArchiveType = static_cast<int>(Type);
				Source.ArchiveIndex = *FileIndex;

				if (swResult<std::unique_ptr<swFile>> File = OpenFile(Source))
					return File;
			}
		}

		return swUnexpected(tr("Could not open included file '%1'.").arg(QString::fromLatin1(PartName)));
	};

	if (mZipFiles[static_cast<int>(swZipFileType::Official)] && !FileInfo)
		return OpenArchiveFile();

	const QString Name = QString::fromLatin1(FileInfo ? FileInfo->FileName.c_str() : PartName);
	std::vector<QString> Folders(1, QStringLiteral("parts/"));

	if (!FileInfo)
		Folders.emplace_back(QStringLiteral("p/"));

	if (mHasUnofficial)
	{
		Folders.emplace_back(QStringLiteral("unofficial/parts/"));

		if (!FileInfo)
			Folders.emplace_back(QStringLiteral("unofficial/p/"));
	}

	for (const QString& Folder : Folders)
	{
		const QString FileName = Folder + Name;
		swFileSource Source;
		Source.FilePath = mLibraryDir.absoluteFilePath(FileName);

		if (swResult<std::unique_ptr<swFile>> File = OpenFile(Source))
			return File;

#if defined(Q_OS_MACOS) || defined(Q_OS_LINUX)
		if (!FileInfo)
		{
			Source.FilePath = mLibraryDir.absoluteFilePath(FileName.toLower());

			if (swResult<std::unique_ptr<swFile>> File = OpenFile(Source))
				return File;
		}
#endif
	}

	// folders fall back to the unofficial archive
	if (!FileInfo)
		return OpenArchiveFile();

	return swUnexpected(tr("Could not open included file '%1'.").arg(QString::fromLatin1(PartName)));
}
