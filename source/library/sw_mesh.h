#pragma once

#include "sw_math.h"
#include "sw_types_asset.h"

#define SW_MESH_POVRAY_PLAIN 0x01
#define SW_MESH_POVRAY_DECALS 0x02

enum swMeshPrimitiveType
{
	SW_MESH_LINES = 0x01,
	SW_MESH_TRIANGLES = 0x02,
	SW_MESH_TEXTURED_TRIANGLES = 0x08,
	SW_MESH_CONDITIONAL_LINES = 0x10,
	SW_MESH_PRIMITIVE_TYPE_COUNT
};

struct swVertex
{
	swVector3 Position;
	quint32 Normal;
};

struct swVertexTextured
{
	swVector3 Position;
	quint32 Normal;
	swVector2 TexCoord;
};

struct swVertexConditional
{
	swVector3 Position1;
	swVector3 Position2;
	swVector3 Position3;
	swVector3 Position4;
};

struct swMeshSection
{
	// Color code until finalized.
	int ColorIndex;

	// Byte offset or first conditional vertex.
	int DrawOffset;

	// Index or conditional vertex count.
	int DrawCount;
	swMeshPrimitiveType PrimitiveType;
	swTextureReference Texture;
	QString TextureName;
	swBoundingBox BoundingBox;
	float Radius;
};

struct swMeshLod
{
	swMeshSection* Sections = nullptr;
	int NumSections = 0;
};

enum
{
	SW_MESH_LOD_HIGH,
	SW_MESH_LOD_LOW,
	SW_MESH_LOD_COUNT
};

enum class swMeshFlag
{
	// Triangles in the default color.
	HasDefault     = 0x01,

	// Triangles in a solid color.
	HasSolid       = 0x02,

	// Triangles in a translucent color.
	HasTranslucent = 0x04,

	// Has lines.
	HasLines       = 0x08,

	// Has textured sections.
	HasTexture     = 0x10,

	// Has a logo stud.
	HasStyleStud   = 0x20
};

Q_DECLARE_FLAGS(swMeshFlags, swMeshFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(swMeshFlags)

class swMesh
{
public:
	swMesh();
	~swMesh();

	swMesh(const swMesh&) = delete;
	swMesh(swMesh&&) = delete;
	swMesh& operator=(const swMesh&) = delete;
	swMesh& operator=(swMesh&&) = delete;

	void Create(quint16 (&NumSections)[SW_MESH_LOD_COUNT], int VertexCount, int TexturedVertexCount, int ConditionalVertexCount, int IndexCount);
	void CreateBox();

	bool FileLoad(swMemoryFile& File);
	bool FileSave(swMemoryFile& File);

	// Sets colors and sorts sections.
	void FinalizeSections();

	template<typename IndexType>
	void ExportPOVRayTriangles(swFile& File, const swMeshSection* Section);
	template<typename IndexType>
	int ExportPOVRay(swFile& File, const char* MeshName, const char** ColorTable, const std::map<const swTexture*, QString>& TextureFiles);

	// Writes plain and textured sections.
	int ExportPOVRay(swFile& File, const char* MeshName, const char** ColorTable, const std::map<const swTexture*, QString>& TextureFiles);

	template<typename IndexType>
	void ExportWavefrontIndices(swFile& File, int DefaultColorIndex, int VertexOffset);
	void ExportWavefrontIndices(swFile& File, int DefaultColorIndex, int VertexOffset);

	template<typename IndexType>
	bool MinIntersectDist(const swVector3& Start, const swVector3& End, float& MinDist, swVector3& HitPlane, swVector3& HitNormal);
	bool MinIntersectDist(const swVector3& Start, const swVector3& End, float& MinDist, swVector3& HitPlane, swVector3& HitNormal);

	template<typename IndexType>
	bool IntersectsPlanes(const swVector4 (&Planes)[6]);
	bool IntersectsPlanes(const swVector4 (&Planes)[6]);

	int GetLodIndex(float Distance) const;

	const swVertex* GetVertexData() const
	{
		return static_cast<swVertex*>(mVertexData);
	}

	const swVertexTextured* GetTexturedVertexData() const
	{
		return reinterpret_cast<swVertexTextured*>(static_cast<char*>(mVertexData) + mNumVertices * sizeof(swVertex));
	}

	const swVertexConditional* GetConditionalVertexData() const
	{
		return reinterpret_cast<swVertexConditional*>(static_cast<char*>(mVertexData) + mNumVertices * sizeof(swVertex) + mNumTexturedVertices * sizeof(swVertexTextured));
	}

	swMeshLod mLods[SW_MESH_LOD_COUNT];
	swBoundingBox mBoundingBox;
	float mRadius = 0.0f;
	swMeshFlags mFlags;

	void* mVertexData = nullptr;
	int mVertexDataSize = 0;
	void* mIndexData = nullptr;
	int mIndexDataSize = 0;
	int mVertexCacheOffset = -1;
	int mIndexCacheOffset = -1;

	int mNumVertices = 0;
	int mNumTexturedVertices = 0;
	int mConditionalVertexCount = 0;
	int mIndexType = 0;
};
