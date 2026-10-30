#include "sw_global.h"
#include "sw_math.h"
#include "sw_mesh.h"
#include "sw_colors.h"
#include "sw_data_part.h"
#include "sw_string.h"
#include "sw_library.h"
#include "sw_application.h"
#include "sw_model.h"
#include "sw_project.h"
#include "sw_scene.h"
#include "sw_synthesis.h"
#include "sw_track_train.h"

swPartData::swPartData(bool IsLibraryPart)
	: mIsLibraryPart(IsLibraryPart)
{
	mZipFileType = swZipFileType::Count;
	SetLoadingBoundingBox();
}

bool swPartData::HasDecorationDescription(const char* Description)
{
	// assemblies may lack a pattern suffix
	std::string PreviousWord;
	bool With = false;

	for (const char* Position = Description; *Position;)
	{
		std::string Word;

		while ((*Position >= 'A' && *Position <= 'Z') || (*Position >= 'a' && *Position <= 'z'))
		{
			char Character = *Position++;

			if (Character >= 'a' && Character <= 'z')
				Character += 'A' - 'a';

			Word.push_back(Character);
		}

		if (Word.empty())
		{
			Position++;
			continue;
		}

		if (PreviousWord != "WITHOUT" && (Word == "PATTERN" || Word == "PATTERNS" || Word == "PATTERNED" || (With && (Word == "STICKER" || Word == "STICKERS"))))
			return true;

		if (Word == "WITH")
			With = true;
		else if (Word == "WITHOUT")
			With = false;

		PreviousWord = std::move(Word);
	}

	return false;
}

std::vector<std::string> swPartData::Classify(std::string_view Name)
{
	mPatternParentId.clear();
	mFlags = Flags();

	if (!IsLibraryPart())
		return std::vector<std::string>();

	if (m_strDescription[0] == '=')
		mFlags |= Flag::Alias;
	else if (m_strDescription[0] == '~')
		mFlags |= Flag::SubPart;

	if (HasDecorationDescription(m_strDescription))
		mFlags |= Flag::Decorated;

	if (IsAlias())
		return std::vector<std::string>();

	if (Name.size() <= 4 || Name.compare(Name.size() - 4, 4, ".DAT") != 0)
		return std::vector<std::string>();

	const std::string_view Stem = Name.substr(0, Name.size() - 4);

	const size_t NumberStart = Stem[0] == 'U' || Stem[0] == 'T' || Stem[0] == 'S' ? 1 : 0;
	const size_t NumberEnd = Stem.find_first_not_of("0123456789", NumberStart);

	if (NumberStart == NumberEnd)
		return std::vector<std::string>();

	std::vector<std::string> ParentIds;

	// p prints, d stickers
	for (size_t Position = Stem.find_last_of("PD"); Position != std::string_view::npos; Position = Position ? Stem.find_last_of("PD", Position - 1) : std::string_view::npos)
	{
		if (Position == 0 || Position + 1 == Stem.size())
			continue;

		if (Stem.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", Position + 1) != std::string_view::npos)
			continue;

		mFlags |= Flag::Decorated;

		ParentIds.emplace_back(Stem.substr(0, Position));
		ParentIds.back() += ".DAT";
	}

	return ParentIds;
}

bool swPartData::InitializeFromCache(const CacheData& CacheData)
{
	const int KnownFlags = static_cast<int>(Flag::Decorated) | static_cast<int>(Flag::Alias) | static_cast<int>(Flag::SubPart);

	if (CacheData.Description.size() >= sizeof(m_strDescription) || CacheData.PatternParentId.size() >= SW_PART_NAME_LENGTH || (CacheData.Flags & ~KnownFlags) != 0)
		return false;

	swstrcpy(m_strDescription, CacheData.Description.c_str());
	mPatternParentId = CacheData.PatternParentId;
	mFlags = Flags(static_cast<Flag>(CacheData.Flags));

	return true;
}

swPartData::CacheData swPartData::GetCacheData() const
{
	return { m_strDescription, mPatternParentId, static_cast<quint8>(static_cast<int>(mFlags)) };
}

void swPartData::SetAliasTarget(const swPartData* Target)
{
	if (!IsAlias())
		return;

	mPatternParentId = Target ? Target->GetPatternParentId() : std::string();
	mFlags.setFlag(Flag::Decorated, HasDecorationDescription(m_strDescription) || (Target && Target->IsDecorated()));
}

void swPartData::SetLoadingBoundingBox()
{
	// keep in sync with swMesh::CreateBox
	mBoundingBox.Min = swVector3(-10.0f, -10.0f, -24.0f);
	mBoundingBox.Max = swVector3(10.0f, 10.0f, 4.0f);
}

swPartData::~swPartData()
{
	delete mTrainTrackInfo;

	Unload();
}

void swPartData::SetTrainTrackInfo(swTrainTrackInformation* TrainTrackInfo)
{
	delete mTrainTrackInfo;
	mTrainTrackInfo = TrainTrackInfo;
}

void swPartData::SetSynthInfo(swSynthesisInformation* SynthInfo)
{
	mSynthInfo.reset(SynthInfo);
}

void swPartData::SetMesh(swMesh* Mesh)
{
	mBoundingBox = Mesh->mBoundingBox;
	ReleaseMesh();
	mMesh = Mesh;
}

swMesh* swPartData::GetDisplayMesh() const
{
	if (mState == swAssetState::Loaded)
		return mMesh;

	if ((IsLoading() || mState == swAssetState::Failed) &&
		((!IsModel() && !IsProject()) || (IsModel() && mModel->HasDirectGeometry())))
		return swGetPartsLibrary()->GetLoadingMesh();

	return nullptr;
}

void swPartData::SetFailed(QString Error)
{
	ReleaseMesh();
	if (!IsModel() && !IsProject())
		SetLoadingBoundingBox();

	mState = swAssetState::Failed;

	if (IsModel())
	{
		if (const swMesh* Mesh = GetDisplayMesh())
		{
			mBoundingBox.Min = swMinimum(mBoundingBox.Min, Mesh->mBoundingBox.Min);
			mBoundingBox.Max = swMaximum(mBoundingBox.Max, Mesh->mBoundingBox.Max);
		}
	}

	swGetPartsLibrary()->SetPartDataLoadError(this, std::move(Error));
}

void swPartData::DetachContainer()
{
	Project* PreviousProject = IsProject() ? mProject : nullptr;
	mType = Type::Part;
	mModel = nullptr;
	mProject = nullptr;
	swstrcpy(m_strDescription, mFileName);
	delete PreviousProject;

	ReleaseMesh();
	SetLoadingBoundingBox();
}

void swPartData::SetModel(swModel* Model)
{
	if (mModel != Model)
	{
		// submodels can take local project names
		Project* PreviousProject = IsProject() ? mProject : nullptr;
		mType = Type::Model;
		mModel = Model;
		mProject = nullptr;
		delete PreviousProject;
		delete mMesh;
		mMesh = nullptr;
		mState = swAssetState::Unloaded;
		swGetPartsLibrary()->ClearPartDataLoadError(this);
	}

	const QByteArray Name = Model->GetProperties().mFileName.toLatin1();
	swstrcpy(mFileName, Name.constData());
	swstrcpy(m_strDescription, Name.constData());
}

void swPartData::CreateProject(Project* NewProject, const char* PartName)
{
	if (mProject != NewProject)
	{
		swGetPartsLibrary()->ClearPartDataLoadError(this);
		Project* PreviousProject = IsProject() ? mProject : nullptr;
		mType = Type::Project;
		mModel = nullptr;
		mProject = NewProject;
		mState = swAssetState::Loaded;
		ReleaseMesh();
		delete PreviousProject;
	}

	swstrcpy(mFileName, PartName);
	swstrcpy(m_strDescription, NewProject->GetFileName().toLatin1().data());
}

bool swPartData::IsProjectPart() const
{
	if (mProject)
		return !strcmp(m_strDescription, mProject->GetFileName().toLatin1().data());
	return false;
}

bool swPartData::GetPartWorldMatrix(swPart* Part, swMatrix44& WorldMatrix) const
{
	if (IsModel())
		return mModel->GetPartWorldMatrix(Part, WorldMatrix);

	return false;
}

bool swPartData::IncludesModel(const swModel* Model) const
{
	if (IsModel())
	{
		if (mModel == Model)
			return true;

		return mModel->IncludesModel(Model);
	}

	return false;
}

void swPartData::CreatePart(const char* Name)
{
	swstrcpy(mFileName, Name);
	swstrcpy(m_strDescription, Name);
}

void swPartData::ReleaseMesh()
{
	if (mMesh)
	{
		delete mMesh;
		mMesh = nullptr;
	}
}

void swPartData::Unload()
{
	swGetPartsLibrary()->ClearPartDataLoadError(this);

	ReleaseMesh();
	mState = swAssetState::Unloaded;
	SetLoadingBoundingBox();
	mModel = nullptr;

	delete mProject;
	mProject = nullptr;
}

bool swPartData::MinIntersectDist(const swVector3& Start, const swVector3& End, float& MinDistance, swPartRayTest& PartRayTest) const
{
	bool Intersect = false;
	swMesh* Mesh = GetDisplayMesh();

	if (Mesh == swGetPartsLibrary()->GetLoadingMesh() || IsModel() || IsProject())
	{
		float Distance;
		swVector3 Plane;

		if (!swBoundingBoxRayIntersectDistance(mBoundingBox.Min, mBoundingBox.Max, Start, End, &Distance, nullptr, &Plane) || (Distance >= MinDistance))
			return false;

		if (Mesh == swGetPartsLibrary()->GetLoadingMesh() && !IsModel() && !IsProject())
		{
			PartRayTest.PartData = this;
			PartRayTest.Transform = swMatrix44Identity();
			MinDistance = Distance;
			PartRayTest.Plane = Plane;
			PartRayTest.Normal = Plane;
			return true;
		}

		if (IsModel())
			Intersect |= mModel->SubModelMinIntersectDist(Start, End, MinDistance, PartRayTest);
		else if (IsProject())
		{
			const swModel* const Model = mProject->GetMainModel();
			if (Model && Model->GetPartData())
				Intersect |= Model->GetPartData()->MinIntersectDist(Start, End, MinDistance, PartRayTest);
		}
	}

	if (Mesh)
	{
		if (Mesh->MinIntersectDist(Start, End, MinDistance, PartRayTest.Plane, PartRayTest.Normal))
		{
			PartRayTest.PartData = this;
			PartRayTest.Transform = swMatrix44Identity();
			Intersect = true;
		}
	}

	return Intersect;
}

bool swPartData::BoxTest(const swMatrix44& WorldMatrix, const swVector4 WorldPlanes[6]) const
{
	swMatrix44 InverseWorldMatrix = swMatrix44AffineInverse(WorldMatrix);

	constexpr int NumCorners = 8;
	constexpr int NumPlanes = 6;
	swVector4 LocalPlanes[NumPlanes];

	for (int PlaneIdx = 0; PlaneIdx < NumPlanes; PlaneIdx++)
	{
		const swVector3 PlaneNormal = swMultiply30(WorldPlanes[PlaneIdx], InverseWorldMatrix);
		LocalPlanes[PlaneIdx] = swVector4(PlaneNormal, WorldPlanes[PlaneIdx][3] - swDot3(InverseWorldMatrix[3], PlaneNormal));
	}

	swVector3 Box[NumCorners];
	swGetBoxCorners(mBoundingBox, Box);

	int Outcodes[NumCorners];

	for (int CornerIdx = 0; CornerIdx < NumCorners; CornerIdx++)
	{
		Outcodes[CornerIdx] = 0;

		for (int PlaneIdx = 0; PlaneIdx < NumPlanes; PlaneIdx++)
		{
			if (swDot3(Box[CornerIdx], LocalPlanes[PlaneIdx]) + LocalPlanes[PlaneIdx][3] > 0)
				Outcodes[CornerIdx] |= 1 << PlaneIdx;
		}
	}

	int OutcodesOR = 0, OutcodesAND = 0x3f;

	for (int CornerIdx = 0; CornerIdx < NumCorners; CornerIdx++)
	{
		OutcodesAND &= Outcodes[CornerIdx];
		OutcodesOR |= Outcodes[CornerIdx];
	}

	if (OutcodesAND != 0)
		return false;

	if (OutcodesOR == 0)
		return true;

	swMesh* Mesh = GetDisplayMesh();

	if (Mesh && Mesh->IntersectsPlanes(LocalPlanes))
		return true;

	if (IsModel())
		return mModel->SubModelBoxTest(LocalPlanes);
	else if (IsProject())
	{
		const swModel* const Model = mProject->GetMainModel();
		return Model && Model->GetPartData() ? Model->GetPartData()->BoxTest(swMatrix44Identity(), LocalPlanes) : false;
	}

	return false;
}

void swPartData::ZoomExtents(float FoV, float AspectRatio, swMatrix44& ProjectionMatrix, swMatrix44& ViewMatrix) const
{
	swVector3 Points[8];
	swGetBoxCorners(mBoundingBox, Points);

	const swVector3 Center = (mBoundingBox.Min + mBoundingBox.Max) / 2.0f;
	swVector3 Position = Center + swVector3(100.0f, -100.0f, 75.0f);

	ProjectionMatrix = swMatrix44Perspective(FoV, AspectRatio, 1.0f, 12500.0f);
	const swMatrix44 ModelView = swMatrix44LookAt(Position, Center, swVector3(0, 0, 1));
	float FarDistance;
	std::tie(Position, FarDistance) = swZoomExtents(Position, ModelView, ProjectionMatrix, Points, 8);
	ViewMatrix = swMatrix44LookAt(Position, Center, swVector3(0, 0, 1));
	ProjectionMatrix = swMatrix44Perspective(FoV, AspectRatio, 1.0f, FarDistance);
}

void swPartData::AddRenderMesh(swScene& Scene)
{
	if (mState != swAssetState::Loaded)
	{
		if (Scene.GetRequireCompleteAssets())
			Scene.MarkMissingAssets();
		else if (swMesh* Mesh = GetDisplayMesh())
			Scene.AddMesh(Mesh, swMatrix44Identity(), gDefaultColor, swRenderMeshState::Default);
	}
	else if (mMesh)
		Scene.AddMesh(mMesh, swMatrix44Identity(), gDefaultColor, swRenderMeshState::Default);
}

void swPartData::AddRenderMeshes(swScene* Scene, const swMatrix44& WorldMatrix, int ColorIndex, swRenderMeshState RenderMeshState, bool ParentActive) const
{
	if (mState != swAssetState::Loaded)
	{
		if (Scene->GetRequireCompleteAssets())
			Scene->MarkMissingAssets();
		else if (swMesh* Mesh = GetDisplayMesh())
			Scene->AddMesh(Mesh, WorldMatrix, ColorIndex, RenderMeshState);
	}
	else if (mMesh)
		Scene->AddMesh(mMesh, WorldMatrix, ColorIndex, RenderMeshState);

	if (IsModel())
		mModel->AddSubModelRenderMeshes(Scene, WorldMatrix, ColorIndex, RenderMeshState, ParentActive);
	else if (IsProject())
	{
		const swModel* const Model = mProject->GetMainModel();
		if (Model && Model->GetPartData())
			Model->GetPartData()->AddRenderMeshes(Scene, WorldMatrix, ColorIndex, RenderMeshState, ParentActive);
	}
}

void swPartData::GetPartsList(int DefaultColorIndex, bool ScanSubModels, bool AddSubModels, swPartsList& PartsList) const
{
	if (IsModel())
	{
		if (ScanSubModels)
			mModel->GetPartsList(DefaultColorIndex, ScanSubModels, AddSubModels, PartsList);

		if (AddSubModels)
			PartsList[this][DefaultColorIndex]++;
	}
	else if (IsProject() && !IsProjectPart())
	{
		const swModel* const Model = mProject->GetMainModel();
		if (Model)
			Model->GetPartsList(DefaultColorIndex, ScanSubModels, AddSubModels, PartsList);
	}
	else
		PartsList[this][DefaultColorIndex]++;
}

void swPartData::GetModelParts(const swMatrix44& WorldMatrix, int DefaultColorIndex, std::vector<swModelPartsEntry>& ModelParts) const
{
	if (IsModel())
	{
		mModel->GetModelParts(WorldMatrix, DefaultColorIndex, ModelParts);
		return;
	}
	else if (IsProject())
	{
		const swModel* const Model = mProject->GetMainModel();
		if (Model)
			Model->GetModelParts(WorldMatrix, DefaultColorIndex, ModelParts);
		return;
	}

	ModelParts.emplace_back(swModelPartsEntry{ WorldMatrix, this, nullptr, DefaultColorIndex });
}

void swPartData::CompareBoundingBox(const swMatrix44& WorldMatrix, swVector3& Min, swVector3& Max) const
{
	if (!IsModel())
	{
		swVector3 Points[8];

		if (!mMesh || IsLoading())
			swGetBoxCorners(GetBoundingBox(), Points);
		else
			swGetBoxCorners(mMesh->mBoundingBox, Points);

		for (int i = 0; i < 8; i++)
		{
			const swVector3 Point = swMultiply31(Points[i], WorldMatrix);

			Min = swMinimum(Point, Min);
			Max = swMaximum(Point, Max);
		}
	}
	else
	{
		mModel->SubModelCompareBoundingBox(WorldMatrix, Min, Max);
	}
}

void swPartData::AddSubModelBoundingBoxPoints(const swMatrix44& WorldMatrix, std::vector<swVector3>& Points, swGeometryBoundsMode Mode) const
{
	if (Mode == swGeometryBoundsMode::AvailableOnly)
	{
		if (IsModel())
		{
			mModel->SubModelAddBoundingBoxPoints(WorldMatrix, Points, Mode);
			return;
		}

		if (IsProject())
		{
			if (const swModel* Model = mProject->GetMainModel())
				Model->SubModelAddBoundingBoxPoints(WorldMatrix, Points, Mode);

			return;
		}

		if (mState != swAssetState::Loaded || !mMesh)
			return;
	}

	if (!IsModel())
	{
		swVector3 BoxPoints[8];

		if (!mMesh || IsLoading())
			swGetBoxCorners(GetBoundingBox(), BoxPoints);
		else
			swGetBoxCorners(mMesh->mBoundingBox, BoxPoints);

		for (int i = 0; i < 8; i++)
			Points.emplace_back(swMultiply31(BoxPoints[i], WorldMatrix));
	}
	else
	{
		mModel->SubModelAddBoundingBoxPoints(WorldMatrix, Points, Mode);
	}
}

void swPartData::UpdateBoundingBox(std::vector<swModel*>& UpdatedModels)
{
	if (IsModel())
		mModel->UpdatePartData(UpdatedModels);
	else if (IsProject())
		mProject->UpdatePartData(this);
}
