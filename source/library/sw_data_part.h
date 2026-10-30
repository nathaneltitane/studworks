#pragma once

#include "sw_types_asset.h"
#include "sw_math.h"

#define SW_PART_NAME_LENGTH 256

struct swModelPartsEntry
{
	swMatrix44 WorldMatrix;
	const swPartData* PartData;

	// Borrowed explicit mesh for generated/direct geometry; null uses PartData->GetMesh().
	swMesh* Mesh;
	int ColorIndex;
};

class swPartData
{
public:
	explicit swPartData(bool IsLibraryPart);
	~swPartData();

	swPartData(const swPartData&) = delete;
	swPartData(swPartData&&) = delete;
	swPartData& operator=(const swPartData&) = delete;
	swPartData& operator=(swPartData&&) = delete;

	const swBoundingBox& GetBoundingBox() const
	{
		return mBoundingBox;
	}

	void SetBoundingBox(const swVector3& Min, const swVector3& Max)
	{
		mBoundingBox.Min = Min;
		mBoundingBox.Max = Max;
	}

	swSynthesisInformation* GetSynthInfo() const
	{
		return mSynthInfo.get();
	}

	std::shared_ptr<const swSynthesisInformation> GetSynthInfoShared() const
	{
		return mSynthInfo;
	}

	void SetSynthInfo(swSynthesisInformation* SynthInfo);

	swTrainTrackInformation* GetTrainTrackInfo() const
	{
		return mTrainTrackInfo;
	}

	// Takes ownership and frees the previous information.
	void SetTrainTrackInfo(swTrainTrackInformation* TrainTrackInfo);

	swMesh* GetMesh() const
	{
		return mMesh;
	}

	swModel* GetModel() const
	{
		return mModel;
	}

	Project* GetProject() const
	{
		return mProject;
	}

	void SetMesh(swMesh* Mesh);

	// Fallback geometry is never owned here.
	swMesh* GetDisplayMesh() const;
	void SetFailed(QString Error);
	void ReleaseMesh();

	int AddRef()
	{
		mRefCount++;
		return mRefCount;
	}

	int Release()
	{
		mRefCount--;
		return mRefCount;
	}

	int GetRefCount() const
	{
		return mRefCount;
	}

	bool IsLibraryPart() const
	{
		return mIsLibraryPart;
	}

	bool IsLoading() const
	{
		return mState == swAssetState::Loading;
	}

	bool IsModel() const
	{
		return mType == Type::Model;
	}

	bool IsProject() const
	{
		return mType == Type::Project;
	}

	void SetZipFile(swZipFileType ZipFileType, int ZipFileIndex)
	{
		mZipFileType = ZipFileType;
		mZipFileIndex = ZipFileIndex;
	}

	struct CacheData
	{
		std::string Description;
		std::string PatternParentId;
		quint8 Flags;
	};

	// Caches the description and classification.
	bool InitializeFromCache(const CacheData& CacheData);
	CacheData GetCacheData() const;

	// Inherits the target decoration and parent.
	void SetAliasTarget(const swPartData* Target);

	// Updates the flags from the name and description.
	std::vector<std::string> Classify(std::string_view Name);

	bool IsDecorated() const
	{
		return IsLibraryPart() && mFlags.testFlag(Flag::Decorated);
	}

	bool IsAlias() const
	{
		return mFlags.testFlag(Flag::Alias);
	}

	// Undecorated parent part id.
	const std::string& GetPatternParentId() const
	{
		return mPatternParentId;
	}

	void SetPatternParentId(std::string ParentId)
	{
		mPatternParentId = std::move(ParentId);
	}

	bool IsSubPart() const
	{
		return mFlags.testFlag(Flag::SubPart);
	}

	bool IsProjectPart() const;
	void ZoomExtents(float FoV, float AspectRatio, swMatrix44& ProjectionMatrix, swMatrix44& ViewMatrix) const;
	void AddRenderMesh(swScene& Scene);
	void AddRenderMeshes(swScene* Scene, const swMatrix44& WorldMatrix, int ColorIndex, swRenderMeshState RenderMeshState, bool ParentActive) const;

	void CreatePart(const char* Name);

	void DetachContainer();
	void SetModel(swModel* Model);
	void CreateProject(Project* Project, const char* PartName);
	bool GetPartWorldMatrix(swPart* Part, swMatrix44& WorldMatrix) const;
	bool IncludesModel(const swModel* Model) const;
	bool MinIntersectDist(const swVector3& Start, const swVector3& End, float& MinDistance, swPartRayTest& PartRayTest) const;
	bool BoxTest(const swMatrix44& WorldMatrix, const swVector4 Planes[6]) const;
	void GetPartsList(int DefaultColorIndex, bool ScanSubModels, bool AddSubModels, swPartsList& PartsList) const;
	void GetModelParts(const swMatrix44& WorldMatrix, int DefaultColorIndex, std::vector<swModelPartsEntry>& ModelParts) const;
	void CompareBoundingBox(const swMatrix44& WorldMatrix, swVector3& Min, swVector3& Max) const;
	void AddSubModelBoundingBoxPoints(const swMatrix44& WorldMatrix, std::vector<swVector3>& Points, swGeometryBoundsMode Mode) const;
	void UpdateBoundingBox(std::vector<swModel*>& UpdatedModels);

	void Unload();

public:
	char mFileName[SW_PART_NAME_LENGTH] = {};
	char m_strDescription[256] = {};
	swZipFileType mZipFileType;
	int mZipFileIndex = -1;
	swAssetState mState = swAssetState::Unloaded;

protected:
	enum class Flag
	{
		Decorated = 0x01,
		Alias = 0x02,
		SubPart = 0x04
	};
	Q_DECLARE_FLAGS(Flags, Flag)

	enum class Type
	{
		Part,
		Model,
		Project
	};

	void SetLoadingBoundingBox();

	// Fixed at creation.
	const bool mIsLibraryPart;
	int mRefCount = 0;
	Type mType = Type::Part;
	swModel* mModel = nullptr;
	Project* mProject = nullptr;
	swMesh* mMesh = nullptr;
	swBoundingBox mBoundingBox;
	Flags mFlags;
	std::string mPatternParentId;
	std::shared_ptr<swSynthesisInformation> mSynthInfo;
	swTrainTrackInformation* mTrainTrackInfo = nullptr;

private:
	static bool HasDecorationDescription(const char* Description);

};
