#pragma once

#include "sw_math.h"
#include "sw_mesh.h"

class swLibraryMeshData;
class swMeshLoader;

enum swMeshDataType
{
	SW_MESH_DATA_HIGH,
	SW_MESH_DATA_LOW,
	SW_MESH_DATA_SHARED,
	SW_MESH_DATA_TYPE_COUNT
};

struct swMeshLoaderVertex
{
	swVector3 Position;
	swVector3 Normal;
	float NormalWeight;
};

struct swMeshLoaderTexturedVertex
{
	swVector3 Position;
	swVector3 Normal;
	swVector2 TexCoords;
};

struct swMeshLoaderConditionalVertex
{
	swVector3 Position[4];
};

enum class swMeshLoaderMaterialType
{
	Solid,
	Planar,
	Cylindrical,
	Spherical
};

struct swMeshLoaderTextureParameters
{
	swMeshLoaderMaterialType Type = swMeshLoaderMaterialType::Solid;
	swVector3 Points[3] = {};
	float Angles[2] = {};
	char Name[SW_MAXIMUM_PATH] = {};
};

struct swMeshLoaderMaterial : public swMeshLoaderTextureParameters
{
	quint32 Color = 16;
};

class swMeshLoaderSection
{
public:
	swMeshLoaderSection(swMeshPrimitiveType PrimitiveType, swMeshLoaderMaterial* Material)
		: mMaterial(Material), mPrimitiveType(PrimitiveType)
	{
		mIndices.reserve(1024);
	}

	swMeshLoaderMaterial* mMaterial;
	swMeshPrimitiveType mPrimitiveType;
	std::vector<quint32> mIndices;
};

struct swMeshLoaderFinalSection
{
	quint32 Color;
	swMeshPrimitiveType PrimitiveType;
	char Name[256];
};

struct swMeshLoaderTextureMap : public swMeshLoaderTextureParameters
{
	bool Fallback = false;
	bool Next = false;
};

class swMeshLoaderTypeData
{
public:
	swMeshLoaderTypeData() = default;

	swMeshLoaderTypeData(const swMeshLoaderTypeData&) = delete;
	swMeshLoaderTypeData& operator=(const swMeshLoaderTypeData&) = delete;

	bool IsEmpty() const
	{
		return mSections.empty();
	}

	void Clear()
	{
		mSections.clear();
		mVertices.clear();
		mConditionalVertices.clear();
	}

	void SetMeshData(swLibraryMeshData* MeshData)
	{
		mMeshData = MeshData;
	}

	swMeshLoaderSection* AddSection(swMeshPrimitiveType PrimitiveType, swMeshLoaderMaterial* Material);

	quint32 AddVertex(const swVector3& Position, bool Optimize);
	quint32 AddVertex(const swVector3& Position, const swVector3& Normal, float NormalWeight, bool Optimize);
	quint32 AddConditionalVertex(const swVector3 (&Position)[4]);

	void ProcessLine(int LineType, swMeshLoaderMaterial* Material, bool WindingCCW, swVector3 (&Vertices)[4], bool Optimize);

	void AddMeshData(const swMeshLoaderTypeData& Data, const swMatrix44& Transform, quint32 CurrentColorCode, bool InvertWinding, bool InvertNormals, swMeshLoaderTextureMap* TextureMap);
	void AddMeshDataNoDuplicateCheck(const swMeshLoaderTypeData& Data, const swMatrix44& Transform, quint32 CurrentColorCode, bool InvertWinding, bool InvertNormals, swMeshLoaderTextureMap* TextureMap);

	std::vector<std::unique_ptr<swMeshLoaderSection>> mSections;
	std::vector<swMeshLoaderVertex> mVertices;
	std::vector<swMeshLoaderConditionalVertex> mConditionalVertices;

protected:
	swLibraryMeshData* mMeshData = nullptr;
};

class swLibraryMeshData
{
public:
	swLibraryMeshData()
	{
		mHasTextures = false;
		mHasStyleStud = false;

		for (swMeshLoaderTypeData& Data : mData)
			Data.SetMeshData(this);
	}

	swLibraryMeshData(const swLibraryMeshData&) = delete;
	swLibraryMeshData& operator=(const swLibraryMeshData&) = delete;

	bool IsEmpty() const
	{
		for (const swMeshLoaderTypeData& Data : mData)
			if (!Data.IsEmpty())
				return false;

		return true;
	}

	void Clear()
	{
		for (swMeshLoaderTypeData& Data : mData)
			Data.Clear();

		mMaterials.clear();
		mTexturedVertices.clear();
		mHasTextures = false;
		mHasStyleStud = false;
	}

	void SetMeshLoader(swMeshLoader* MeshLoader)
	{
		mMeshLoader = MeshLoader;
	}

	// Build private geometry with color codes; the UI thread calls FinalizeSections().
	swMesh* CreateMesh(const std::atomic_bool* Cancelled);
	void AddVertices(swMeshDataType MeshDataType, size_t VertexCount, int* BaseVertex, swMeshLoaderVertex** VertexBuffer);
	void AddIndices(swMeshDataType MeshDataType, swMeshPrimitiveType PrimitiveType, quint32 ColorCode, size_t IndexCount, quint32** IndexBuffer);
	void AddMeshData(const swLibraryMeshData& Data, const swMatrix44& Transform, quint32 CurrentColorCode, bool InvertWinding, bool InvertNormals, swMeshLoaderTextureMap* TextureMap, swMeshDataType OverrideDestIndex);
	void AddMeshDataNoDuplicateCheck(const swLibraryMeshData& Data, const swMatrix44& Transform, quint32 CurrentColorCode, bool InvertWinding, bool InvertNormals, swMeshLoaderTextureMap* TextureMap, swMeshDataType OverrideDestIndex);

	swMeshLoaderMaterial* GetMaterial(quint32 ColorCode);
	swMeshLoaderMaterial* GetTexturedMaterial(quint32 ColorCode, const swMeshLoaderTextureParameters& TextureMap);

	std::array<swMeshLoaderTypeData, SW_MESH_DATA_TYPE_COUNT> mData;
	bool mHasTextures;
	bool mHasStyleStud;

protected:
	swMeshLoader* mMeshLoader = nullptr;
	std::vector<std::unique_ptr<swMeshLoaderMaterial>> mMaterials;
	std::vector<swMeshLoaderTexturedVertex> mTexturedVertices;

	void GenerateTexturedVertices();
	void GeneratePlanarTexcoords(swMeshLoaderSection* Section, const swMeshLoaderTypeData& Data);
	void GenerateCylindricalTexcoords(swMeshLoaderSection* Section, const swMeshLoaderTypeData& Data);
	void GenerateSphericalTexcoords(swMeshLoaderSection* Section, const swMeshLoaderTypeData& Data);
	quint32 AddTexturedVertex(const swVector3& Position, const swVector3& Normal, const swVector2& TexCoords);

	template<typename IndexType>
	bool WriteSections(swMesh* Mesh, const std::vector<swMeshLoaderFinalSection> (&FinalSections)[SW_MESH_LOD_COUNT], int (&BaseVertices)[SW_MESH_DATA_TYPE_COUNT], const std::atomic_bool* Cancelled);

	static bool UpdateMeshBoundingBox(swMesh* Mesh, const std::atomic_bool* Cancelled);
	template<typename IndexType>
	static bool UpdateMeshSectionBoundingBox(const swMesh* Mesh, const swMeshSection& Section, swVector3& SectionMin, swVector3& SectionMax, const std::atomic_bool* Cancelled);
};

enum class swMeshLoaderFlag
{
	None = 0,
	Optimize = 1 << 0,
	SearchProjectFolder = 1 << 1,
	RequireAllIncludes = 1 << 2
};

Q_DECLARE_FLAGS(swMeshLoaderFlags, swMeshLoaderFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(swMeshLoaderFlags)

class swMeshLoader
{
public:
	swMeshLoader(swLibraryMeshData& MeshData, Project* CurrentProject, swMeshLoaderFlags Flags);

	bool LoadMesh(swFile& File, swMeshDataType MeshDataType);
	static std::vector<std::string> GetTextureReferences(const QByteArray& FileData);
	const QString& GetError() const { return mError; }
	void SetCancellationFlag(const std::atomic_bool* Cancelled) { mCancelled = Cancelled; }
	bool SearchProjectFolder() const { return mFlags.testFlag(swMeshLoaderFlag::SearchProjectFolder); }

	Project* mCurrentProject;

protected:
	static bool ParseTextureName(const char* Text, char* Name, size_t NameSize, const char** EndOfName);
	bool ReadMeshData(swFile& File, const swMatrix44& CurrentTransform, quint32 CurrentColorCode, bool InvertWinding, swMeshDataType MeshDataType);

	std::vector<swMeshLoaderTextureMap> mTextureStack;

	swLibraryMeshData& mMeshData;
	swMeshLoaderFlags mFlags;
	const std::atomic_bool* mCancelled = nullptr;
	QString mError;

	Q_DECLARE_TR_FUNCTIONS(swMeshLoader)
};
