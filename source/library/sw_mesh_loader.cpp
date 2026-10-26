#include "sw_global.h"
#include "sw_mesh_loader.h"
#include "sw_file.h"
#include "sw_colors.h"
#include "sw_library.h"
#include "sw_application.h"
#include "sw_texture.h"
#include "sw_string.h"

static void swCheckTextureCoordinatesWrap(const swVector4& Plane2, const swVector3 (&Positions)[3], swVector2 (&TexCoords)[3])
{
	swVector2& TexCoords1 = TexCoords[0];
	swVector2& TexCoords2 = TexCoords[1];
	swVector2& TexCoords3 = TexCoords[2];

	const float u12 = fabsf(TexCoords1.x - TexCoords2.x);
	const float u13 = fabsf(TexCoords1.x - TexCoords3.x);
	const float u23 = fabsf(TexCoords2.x - TexCoords3.x);

	if (u12 < 0.5f && u13 < 0.5f && u23 < 0.5f)
		return;

	const float Dot1 = fabsf(swDot(Plane2, swVector4(Positions[0], 1.0f)));
	const float Dot2 = fabsf(swDot(Plane2, swVector4(Positions[1], 1.0f)));
	const float Dot3 = fabsf(swDot(Plane2, swVector4(Positions[2], 1.0f)));

	if (Dot1 > Dot2)
	{
		if (Dot1 > Dot3)
		{
			if (u12 > 0.5f)
				TexCoords2.x += TexCoords2.x < 0.5f ? 1.0f : -1.0f;

			if (u13 > 0.5f)
				TexCoords3.x += TexCoords3.x < 0.5f ? 1.0f : -1.0f;
		}
		else
		{
			if (u13 > 0.5f)
				TexCoords1.x += TexCoords1.x < 0.5f ? 1.0f : -1.0f;

			if (u23 > 0.5f)
				TexCoords2.x += TexCoords2.x < 0.5f ? 1.0f : -1.0f;
		}
	}
	else
	{
		if (Dot2 > Dot3)
		{
			if (u12 > 0.5f)
				TexCoords1.x += TexCoords1.x < 0.5f ? 1.0f : -1.0f;

			if (u23 > 0.5f)
				TexCoords3.x += TexCoords3.x < 0.5f ? 1.0f : -1.0f;
		}
		else
		{
			if (u13 > 0.5f)
				TexCoords1.x += TexCoords1.x < 0.5f ? 1.0f : -1.0f;

			if (u23 > 0.5f)
				TexCoords2.x += TexCoords2.x < 0.5f ? 1.0f : -1.0f;
		}
	}
}

static void swCheckTextureCoordinatesPole(const swVector4& FrontPlane, const swVector4& Plane2, const swVector3(&Positions)[3], swVector2(&TexCoords)[3])
{
	int PoleIndex;
	int EdgeIndex1, EdgeIndex2;

	if (fabsf(swDot(swVector4(Positions[0], 1.0f), FrontPlane)) < 0.01f && fabsf(swDot(swVector4(Positions[0], 1.0f), Plane2)) < 0.01f)
	{
		PoleIndex = 0;
		EdgeIndex1 = 1;
		EdgeIndex2 = 2;
	}
	else if (fabsf(swDot(swVector4(Positions[1], 1.0f), FrontPlane)) < 0.01f && fabsf(swDot(swVector4(Positions[1], 1.0f), Plane2)) < 0.01f)
	{
		PoleIndex = 1;
		EdgeIndex1 = 0;
		EdgeIndex2 = 2;
	}
	else if (fabsf(swDot(swVector4(Positions[2], 1.0f), FrontPlane)) < 0.01f && fabsf(swDot(swVector4(Positions[2], 1.0f), Plane2)) < 0.01f)
	{
		PoleIndex = 2;
		EdgeIndex1 = 0;
		EdgeIndex2 = 1;
	}
	else
		return;

	const swVector3 OppositeEdge = Positions[EdgeIndex2] - Positions[EdgeIndex1];
	const swVector3 SideEdge = Positions[PoleIndex] - Positions[EdgeIndex1];

	const float OppositeLength = swLength(OppositeEdge);
	const float Projection = swDot(OppositeEdge, SideEdge) / (OppositeLength * OppositeLength);

	TexCoords[PoleIndex].x = TexCoords[EdgeIndex1].x + (TexCoords[EdgeIndex2].x - TexCoords[EdgeIndex1].x) * Projection;
}

static void swResequenceQuad(int* Indices, int a, int b, int c, int d)
{
	Indices[0] = a;
	Indices[1] = b;
	Indices[2] = c;
	Indices[3] = d;
}

static void swTestQuad(int (&TriangleIndices)[2][3], const swVector3 (&Vertices)[4])
{
	const swVector3 v01 = Vertices[1] - Vertices[0];
	const swVector3 v02 = Vertices[2] - Vertices[0];
	const swVector3 v03 = Vertices[3] - Vertices[0];
	const swVector3 cp1 = swCross(v01, v02);
	const swVector3 cp2 = swCross(v02, v03);

	if (swDot(cp1, cp2) > 0.0f)
		return;

	const swVector3 v12 = Vertices[2] - Vertices[1];
	const swVector3 v13 = Vertices[3] - Vertices[1];
	const swVector3 v23 = Vertices[3] - Vertices[2];
	int QuadIndices[4] = { 0, 1, 2, 3 };

	if (swDot(swCross(v12, v01), swCross(v01, v13)) > 0.0f)
	{
		if (-swDot(swCross(v02, v12), swCross(v12, v23)) > 0.0f)
			swResequenceQuad(QuadIndices, 1, 2, 3, 0);
		else
			swResequenceQuad(QuadIndices, 0, 3, 1, 2);
	}
	else
	{
		if (-swDot(swCross(v02, v12), swCross(v12, v23)) > 0.0f)
			swResequenceQuad(QuadIndices, 0, 1, 3, 2);
		else
			swResequenceQuad(QuadIndices, 1, 2, 3, 0);
	}

	TriangleIndices[0][0] = QuadIndices[0];
	TriangleIndices[0][1] = QuadIndices[1];
	TriangleIndices[0][2] = QuadIndices[2];

	TriangleIndices[1][0] = QuadIndices[2];
	TriangleIndices[1][1] = QuadIndices[3];
	TriangleIndices[1][2] = QuadIndices[0];
}

constexpr float swDistanceEpsilon = 0.01f; // Maximum value for 50591.dat

static bool swCompareVertices(const swVector3& Position1, const swVector3& Position2)
{
	return fabsf(Position1.x - Position2.x) < swDistanceEpsilon && fabsf(Position1.y - Position2.y) < swDistanceEpsilon && fabsf(Position1.z - Position2.z) < swDistanceEpsilon;
}

swMeshLoaderSection* swMeshLoaderTypeData::AddSection(swMeshPrimitiveType PrimitiveType, swMeshLoaderMaterial* Material)
{
	for (const std::unique_ptr<swMeshLoaderSection>& Section : mSections)
		if (Section->mMaterial == Material && Section->mPrimitiveType == PrimitiveType)
			return Section.get();

	mSections.emplace_back(new swMeshLoaderSection(PrimitiveType, Material));

	return mSections.back().get();
}

quint32 swMeshLoaderTypeData::AddVertex(const swVector3& Position, bool Optimize)
{
	if (Optimize)
	{
		for (int VertexIdx = static_cast<int>(mVertices.size()) - 1; VertexIdx >= 0; VertexIdx--)
		{
			const swMeshLoaderVertex& Vertex = mVertices[VertexIdx];

			if (swCompareVertices(Position, Vertex.Position))
				return VertexIdx;
		}
	}

	swMeshLoaderVertex& Vertex = mVertices.emplace_back();

	Vertex.Position = Position;
	Vertex.Normal = swVector3(0.0f, 0.0f, 0.0f);
	Vertex.NormalWeight = 0.0f;

	return static_cast<quint32>(mVertices.size()) - 1;
}

quint32 swMeshLoaderTypeData::AddVertex(const swVector3& Position, const swVector3& Normal, float NormalWeight, bool Optimize)
{
	if (Optimize)
	{
		for (int VertexIdx = static_cast<int>(mVertices.size()) - 1; VertexIdx >= 0; VertexIdx--)
		{
			swMeshLoaderVertex& Vertex = mVertices[VertexIdx];

			if (swCompareVertices(Position, Vertex.Position))
			{
				if (Vertex.NormalWeight == 0.0f)
				{
					Vertex.Normal = Normal;
					Vertex.NormalWeight = NormalWeight;
					return VertexIdx;
				}
				else if (swDot(Normal, Vertex.Normal) > 0.71f)
				{
					Vertex.Normal = swNormalize(Vertex.Normal * Vertex.NormalWeight + Normal * NormalWeight);
					Vertex.NormalWeight += NormalWeight;
					return VertexIdx;
				}
			}
		}
	}

	swMeshLoaderVertex& Vertex = mVertices.emplace_back();

	Vertex.Position = Position;
	Vertex.Normal = Normal;
	Vertex.NormalWeight = 1.0f;

	return static_cast<quint32>(mVertices.size()) - 1;
}

quint32 swMeshLoaderTypeData::AddConditionalVertex(const swVector3(&Position)[4])
{
	swMeshLoaderConditionalVertex& Vertex = mConditionalVertices.emplace_back();

	Vertex.Position[0] = Position[0];
	Vertex.Position[1] = Position[1];
	Vertex.Position[2] = Position[2];
	Vertex.Position[3] = Position[3];

	return static_cast<quint32>(mConditionalVertices.size()) - 1;
}

void swMeshLoaderTypeData::ProcessLine(int LineType, swMeshLoaderMaterial* Material, bool WindingCCW, swVector3 (&Vertices)[4], bool Optimize)
{
	constexpr swMeshPrimitiveType PrimitiveTypes[4] = { SW_MESH_LINES, SW_MESH_TRIANGLES, SW_MESH_TRIANGLES, SW_MESH_CONDITIONAL_LINES };
	swMeshPrimitiveType PrimitiveType = PrimitiveTypes[LineType - 2];

	if (Material->Type != swMeshLoaderMaterialType::Solid && PrimitiveType == SW_MESH_TRIANGLES)
		PrimitiveType = SW_MESH_TEXTURED_TRIANGLES;

	swMeshLoaderSection* Section = AddSection(PrimitiveType, Material);

	if (LineType == 3 || LineType == 4)
	{
		int TriangleIndices[2][3] = { { 0, 1, 2 }, { 2, 3, 0 } };

		if (LineType == 4)
			swTestQuad(TriangleIndices, Vertices);

		swVector3 Normal = swNormalize(swCross(Vertices[1] - Vertices[0], Vertices[2] - Vertices[0]));

		if (!WindingCCW)
			Normal = -Normal;

		for (int TriangleIndex = 0; TriangleIndex < LineType - 2; TriangleIndex++)
		{
			const swVector3& Vertex1 = Vertices[TriangleIndices[TriangleIndex][0]];
			const swVector3& Vertex2 = Vertices[TriangleIndices[TriangleIndex][1]];
			const swVector3& Vertex3 = Vertices[TriangleIndices[TriangleIndex][2]];
			const swVector3 Edge1 = swNormalize(Vertex2 - Vertex1);
			const swVector3 Edge2 = swNormalize(Vertex3 - Vertex1);
			const swVector3 Edge3 = swNormalize(Vertex3 - Vertex2);
			const float Angle1 = acosf(swDot(Edge1, Edge2));
			const float Angle2 = acosf(swDot(-Edge1, Edge3));
			const float Angle3 = SW_PI - Angle1 - Angle2;

			int Indices[3];

			Indices[0] = AddVertex(Vertex1, Normal, Angle1, Optimize);
			Indices[1] = AddVertex(Vertex2, Normal, Angle2, Optimize);
			Indices[2] = AddVertex(Vertex3, Normal, Angle3, Optimize);

			if (Indices[0] != Indices[1] && Indices[0] != Indices[2] && Indices[1] != Indices[2])
			{
				if (WindingCCW)
				{
					Section->mIndices.emplace_back(Indices[0]);
					Section->mIndices.emplace_back(Indices[1]);
					Section->mIndices.emplace_back(Indices[2]);
				}
				else
				{
					Section->mIndices.emplace_back(Indices[2]);
					Section->mIndices.emplace_back(Indices[1]);
					Section->mIndices.emplace_back(Indices[0]);
				}
			}
		}
	}
	else if (LineType == 2)
	{
		int Indices[2];

		Indices[0] = AddVertex(Vertices[0], Optimize);
		Indices[1] = AddVertex(Vertices[1], Optimize);

		if (Indices[0] != Indices[1])
		{
			Section->mIndices.emplace_back(Indices[0]);
			Section->mIndices.emplace_back(Indices[1]);
		}
	}
	else if (LineType == 5)
	{
		int Indices[2];

		Indices[0] = AddConditionalVertex(Vertices);
		Section->mIndices.emplace_back(Indices[0]);

		std::swap(Vertices[0], Vertices[1]);

		Indices[1] = AddConditionalVertex(Vertices);
		Section->mIndices.emplace_back(Indices[1]);
	}
}

void swMeshLoaderTypeData::AddMeshData(const swMeshLoaderTypeData& Data, const swMatrix44& Transform, quint32 CurrentColorCode, bool InvertWinding, bool InvertNormals, swMeshLoaderTextureMap* TextureMap)
{
	const std::vector<swMeshLoaderVertex>& DataVertices = Data.mVertices;
	std::vector<quint32> IndexRemap;
	const swMatrix33 NormalTransform = swMatrix33Transpose(swMatrix33(swMatrix44Inverse(Transform)));

	IndexRemap.reserve(DataVertices.size());
	mVertices.reserve(mVertices.size() + DataVertices.size());

	for (const swMeshLoaderVertex& DataVertex : DataVertices)
	{
		const swVector3 Position = swMultiply31(DataVertex.Position, Transform);
		int Index;

		if (DataVertex.NormalWeight == 0.0f)
			Index = AddVertex(Position, true);
		else
		{
			swVector3 Normal = swNormalize(swMultiply(DataVertex.Normal, NormalTransform));
			if (InvertNormals)
				Normal = -Normal;
			Index = AddVertex(Position, Normal, DataVertex.NormalWeight, true);
		}

		IndexRemap.emplace_back(Index);
	}

	mConditionalVertices.reserve(mConditionalVertices.size() + Data.mConditionalVertices.size());
	std::vector<quint32> ConditionalRemap;
	ConditionalRemap.reserve(Data.mConditionalVertices.size());

	for (const swMeshLoaderConditionalVertex& DataVertex : Data.mConditionalVertices)
	{
		swVector3 Position[4];

		Position[0] = swMultiply31(DataVertex.Position[0], Transform);
		Position[1] = swMultiply31(DataVertex.Position[1], Transform);
		Position[2] = swMultiply31(DataVertex.Position[2], Transform);
		Position[3] = swMultiply31(DataVertex.Position[3], Transform);

		const int Index = AddConditionalVertex(Position);
		ConditionalRemap.emplace_back(Index);
	}

	for (const std::unique_ptr<swMeshLoaderSection>& SrcSection : Data.mSections)
	{
		const quint32 ColorCode = SrcSection->mMaterial->Color == 16 ? CurrentColorCode : SrcSection->mMaterial->Color;
		swMeshPrimitiveType PrimitiveType = SrcSection->mPrimitiveType;
		swMeshLoaderSection* DstSection;

		if (SrcSection->mMaterial->Type != swMeshLoaderMaterialType::Solid)
		{
			swMeshLoaderTextureParameters DstTextureMap;

			if (TextureMap)
				DstTextureMap = *TextureMap;
			else
				DstTextureMap = *SrcSection->mMaterial;

			for (swVector3& Point : DstTextureMap.Points)
				Point = swMultiply31(Point, Transform);

			DstSection = AddSection(PrimitiveType, mMeshData->GetTexturedMaterial(ColorCode, DstTextureMap));
		}
		else if (TextureMap && SrcSection->mPrimitiveType == SW_MESH_TRIANGLES)
		{
			PrimitiveType = SW_MESH_TEXTURED_TRIANGLES;

			DstSection = AddSection(PrimitiveType, mMeshData->GetTexturedMaterial(ColorCode, *TextureMap));
		}
		else
		{
			DstSection = AddSection(PrimitiveType, mMeshData->GetMaterial(ColorCode));
		}

		DstSection->mIndices.reserve(DstSection->mIndices.size() + SrcSection->mIndices.size());

		if (PrimitiveType == SW_MESH_CONDITIONAL_LINES)
		{
			for (const quint32 Index : SrcSection->mIndices)
				DstSection->mIndices.emplace_back(ConditionalRemap[Index]);
		}
		else if (!InvertWinding || (PrimitiveType == SW_MESH_LINES))
		{
			for (const quint32 Index : SrcSection->mIndices)
				DstSection->mIndices.emplace_back(IndexRemap[Index]);
		}
		else
		{
			for (size_t IndexIdx = 0; IndexIdx < SrcSection->mIndices.size(); IndexIdx += 3)
			{
				DstSection->mIndices.emplace_back(IndexRemap[SrcSection->mIndices[IndexIdx + 2]]);
				DstSection->mIndices.emplace_back(IndexRemap[SrcSection->mIndices[IndexIdx + 1]]);
				DstSection->mIndices.emplace_back(IndexRemap[SrcSection->mIndices[IndexIdx + 0]]);
			}
		}
	}
}

void swMeshLoaderTypeData::AddMeshDataNoDuplicateCheck(const swMeshLoaderTypeData& Data, const swMatrix44& Transform, quint32 CurrentColorCode, bool InvertWinding, bool InvertNormals, swMeshLoaderTextureMap* TextureMap)
{
	const std::vector<swMeshLoaderVertex>& DataVertices = Data.mVertices;
	quint32 BaseIndex;
	const swMatrix33 NormalTransform = swMatrix33Transpose(swMatrix33(swMatrix44Inverse(Transform)));

	BaseIndex = static_cast<quint32>(mVertices.size());

	mVertices.reserve(mVertices.size() + DataVertices.size());

	for (const swMeshLoaderVertex& SrcVertex : DataVertices)
	{
		swMeshLoaderVertex& DstVertex = mVertices.emplace_back();
		DstVertex.Position = swMultiply31(SrcVertex.Position, Transform);
		DstVertex.Normal = swNormalize(swMultiply(SrcVertex.Normal, NormalTransform));
		if (InvertNormals)
			DstVertex.Normal = -DstVertex.Normal;
		DstVertex.NormalWeight = SrcVertex.NormalWeight;
	}

	mConditionalVertices.reserve(mConditionalVertices.size() + Data.mConditionalVertices.size());
	const quint32 BaseConditional = static_cast<quint32>(mConditionalVertices.size());

	for (const swMeshLoaderConditionalVertex& DataVertex : Data.mConditionalVertices)
	{
		swMeshLoaderConditionalVertex& Vertex = mConditionalVertices.emplace_back();

		Vertex.Position[0] = swMultiply31(DataVertex.Position[0], Transform);
		Vertex.Position[1] = swMultiply31(DataVertex.Position[1], Transform);
		Vertex.Position[2] = swMultiply31(DataVertex.Position[2], Transform);
		Vertex.Position[3] = swMultiply31(DataVertex.Position[3], Transform);
	}

	for (const std::unique_ptr<swMeshLoaderSection>& SrcSection : Data.mSections)
	{
		const quint32 ColorCode = SrcSection->mMaterial->Color == 16 ? CurrentColorCode : SrcSection->mMaterial->Color;
		swMeshPrimitiveType PrimitiveType = SrcSection->mPrimitiveType;
		swMeshLoaderSection* DstSection;

		if (SrcSection->mMaterial->Type != swMeshLoaderMaterialType::Solid)
		{
			swMeshLoaderTextureParameters DstTextureMap;

			if (TextureMap)
				DstTextureMap = *TextureMap;
			else
				DstTextureMap = *SrcSection->mMaterial;

			for (swVector3& Point : DstTextureMap.Points)
				Point = swMultiply31(Point, Transform);

			DstSection = AddSection(PrimitiveType, mMeshData->GetTexturedMaterial(ColorCode, DstTextureMap));
		}
		else if (TextureMap && SrcSection->mPrimitiveType == SW_MESH_TRIANGLES)
		{
			PrimitiveType = SW_MESH_TEXTURED_TRIANGLES;

			DstSection = AddSection(PrimitiveType, mMeshData->GetTexturedMaterial(ColorCode, *TextureMap));
		}
		else
		{
			DstSection = AddSection(PrimitiveType, mMeshData->GetMaterial(ColorCode));
		}

		DstSection->mIndices.reserve(DstSection->mIndices.size() + SrcSection->mIndices.size());

		if (PrimitiveType == SW_MESH_CONDITIONAL_LINES)
		{
			for (const quint32 Index : SrcSection->mIndices)
				DstSection->mIndices.emplace_back(BaseConditional + Index);
		}
		else if (!InvertWinding || (PrimitiveType == SW_MESH_LINES))
		{
			for (const quint32 Index : SrcSection->mIndices)
				DstSection->mIndices.emplace_back(BaseIndex + Index);
		}
		else
		{
			for (size_t IndexIdx = 0; IndexIdx < SrcSection->mIndices.size(); IndexIdx += 3)
			{
				DstSection->mIndices.emplace_back(BaseIndex + SrcSection->mIndices[IndexIdx + 2]);
				DstSection->mIndices.emplace_back(BaseIndex + SrcSection->mIndices[IndexIdx + 1]);
				DstSection->mIndices.emplace_back(BaseIndex + SrcSection->mIndices[IndexIdx + 0]);
			}
		}
	}
}

void swLibraryMeshData::AddVertices(swMeshDataType MeshDataType, size_t VertexCount, int* BaseVertex, swMeshLoaderVertex** VertexBuffer)
{
	std::vector<swMeshLoaderVertex>& Vertices = mData[MeshDataType].mVertices;
	int CurrentSize = static_cast<int>(Vertices.size());

	Vertices.resize(CurrentSize + VertexCount);

	*BaseVertex = CurrentSize;
	*VertexBuffer = &Vertices[CurrentSize];
}

void swLibraryMeshData::AddIndices(swMeshDataType MeshDataType, swMeshPrimitiveType PrimitiveType, quint32 ColorCode, size_t IndexCount, quint32** IndexBuffer)
{
	swMeshLoaderSection* Section = mData[MeshDataType].AddSection(PrimitiveType, GetMaterial(ColorCode));
	std::vector<quint32>& Indices = Section->mIndices;
	const size_t CurrentSize = Indices.size();

	Indices.resize(CurrentSize + IndexCount);

	*IndexBuffer = &Indices[CurrentSize];
}

void swLibraryMeshData::AddMeshData(const swLibraryMeshData& Data, const swMatrix44& Transform, quint32 CurrentColorCode, bool InvertWinding, bool InvertNormals, swMeshLoaderTextureMap* TextureMap, swMeshDataType OverrideDestIndex)
{
	for (int MeshDataIdx = 0; MeshDataIdx < SW_MESH_DATA_TYPE_COUNT; MeshDataIdx++)
	{
		const int DestIndex = OverrideDestIndex == SW_MESH_DATA_SHARED ? MeshDataIdx : OverrideDestIndex;
		mData[DestIndex].AddMeshData(Data.mData[MeshDataIdx], Transform, CurrentColorCode, InvertWinding, InvertNormals, TextureMap);
	}

	mHasTextures |= (Data.mHasTextures || TextureMap);
}

void swLibraryMeshData::AddMeshDataNoDuplicateCheck(const swLibraryMeshData& Data, const swMatrix44& Transform, quint32 CurrentColorCode, bool InvertWinding, bool InvertNormals, swMeshLoaderTextureMap* TextureMap, swMeshDataType OverrideDestIndex)
{
	for (int MeshDataIdx = 0; MeshDataIdx < SW_MESH_DATA_TYPE_COUNT; MeshDataIdx++)
	{
		const int DestIndex = OverrideDestIndex == SW_MESH_DATA_SHARED ? MeshDataIdx : OverrideDestIndex;
		mData[DestIndex].AddMeshDataNoDuplicateCheck(Data.mData[MeshDataIdx], Transform, CurrentColorCode, InvertWinding, InvertNormals, TextureMap);
	}

	mHasTextures |= (Data.mHasTextures || TextureMap);
}

swMeshLoaderMaterial* swLibraryMeshData::GetMaterial(quint32 ColorCode)
{
	for (const std::unique_ptr<swMeshLoaderMaterial>& Material : mMaterials)
		if (Material->Type == swMeshLoaderMaterialType::Solid && Material->Color == ColorCode)
			return Material.get();

	swMeshLoaderMaterial* Material = new swMeshLoaderMaterial();
	mMaterials.emplace_back(Material);

	Material->Type = swMeshLoaderMaterialType::Solid;
	Material->Color = ColorCode;

	return Material;
}

swMeshLoaderMaterial* swLibraryMeshData::GetTexturedMaterial(quint32 ColorCode, const swMeshLoaderTextureParameters& TextureMap)
{
	for (const std::unique_ptr<swMeshLoaderMaterial>& Material : mMaterials)
	{
		if (Material->Type != TextureMap.Type || Material->Color != ColorCode)
			continue;

		if (strcmp(Material->Name, TextureMap.Name))
			continue;

		if (Material->Points[0] != TextureMap.Points[0] || Material->Points[1] != TextureMap.Points[1] || Material->Points[2] != TextureMap.Points[2])
			continue;

		if (Material->Type == swMeshLoaderMaterialType::Cylindrical)
		{
			if (Material->Angles[0] != TextureMap.Angles[0])
				continue;
		}
		else if (Material->Type == swMeshLoaderMaterialType::Spherical)
		{
			if (Material->Angles[0] != TextureMap.Angles[0] || Material->Angles[1] != TextureMap.Angles[1])
				continue;
		}

		return Material.get();
	}

	swMeshLoaderMaterial* Material = new swMeshLoaderMaterial();
	mMaterials.emplace_back(Material);

	Material->Type = TextureMap.Type;
	Material->Color = ColorCode;
	Material->Points[0] = TextureMap.Points[0];
	Material->Points[1] = TextureMap.Points[1];
	Material->Points[2] = TextureMap.Points[2];
	Material->Angles[0] = TextureMap.Angles[0];
	Material->Angles[1] = TextureMap.Angles[1];
	swstrcpy(Material->Name, TextureMap.Name);

	return Material;
}

quint32 swLibraryMeshData::AddTexturedVertex(const swVector3& Position, const swVector3& Normal, const swVector2& TexCoords)
{
	for (int VertexIndex = static_cast<int>(mTexturedVertices.size()) - 1; VertexIndex >= 0; VertexIndex--)
	{
		const swMeshLoaderTexturedVertex& Vertex = mTexturedVertices[VertexIndex];

		if (Vertex.Position == Position && Vertex.Normal == Normal && Vertex.TexCoords == TexCoords)
			return VertexIndex;
	}

	swMeshLoaderTexturedVertex& Vertex = mTexturedVertices.emplace_back();

	Vertex.Position = Position;
	Vertex.Normal = Normal;
	Vertex.TexCoords = TexCoords;

	return static_cast<quint32>(mTexturedVertices.size()) - 1;
}

void swLibraryMeshData::GeneratePlanarTexcoords(swMeshLoaderSection* Section, const swMeshLoaderTypeData& Data)
{
	const swMeshLoaderMaterial* Material = Section->mMaterial;
	swVector4 Planes[2];

	for (int EdgeIdx = 0; EdgeIdx < 2; EdgeIdx++)
	{
		swVector3 Normal = Material->Points[EdgeIdx + 1] - Material->Points[0];
		const float Length = swLength(Normal);
		Normal /= Length;

		Planes[EdgeIdx].x = Normal.x / Length;
		Planes[EdgeIdx].y = Normal.y / Length;
		Planes[EdgeIdx].z = Normal.z / Length;
		Planes[EdgeIdx].w = -swDot(Normal, Material->Points[0]) / Length;
	}

	for (quint32& Index : Section->mIndices)
	{
		const swMeshLoaderVertex& SrcVertex = Data.mVertices[Index];

		const swVector2 TexCoords(swDot3(SrcVertex.Position, Planes[0]) + Planes[0].w, swDot3(SrcVertex.Position, Planes[1]) + Planes[1].w);

		Index = AddTexturedVertex(SrcVertex.Position, SrcVertex.Normal, TexCoords);
	}
}

void swLibraryMeshData::GenerateCylindricalTexcoords(swMeshLoaderSection* Section, const swMeshLoaderTypeData& Data)
{
	const swMeshLoaderMaterial* Material = Section->mMaterial;
	const swVector3 Up = Material->Points[0] - Material->Points[1];
	const float UpLength = swLength(Up);
	const swVector3 Front = swNormalize(Material->Points[2] - Material->Points[1]);
	const swVector3 Plane1Normal = Up / UpLength;
	const swVector3 Plane2Normal = swNormalize(swCross(Front, Up));
	const swVector4 FrontPlane = swVector4(Front, -swDot(Front, Material->Points[1]));
	const swVector4 Plane1 = swVector4(Plane1Normal, -swDot(Plane1Normal, Material->Points[1]));
	const swVector4 Plane2 = swVector4(Plane2Normal, -swDot(Plane2Normal, Material->Points[1]));
	const float Angle = 360.0f / Material->Angles[0];

	for (size_t TriangleIndex = 0; TriangleIndex < Section->mIndices.size(); TriangleIndex += 3)
	{
		const swVector3 Positions[3] =
		{
			Data.mVertices[Section->mIndices[TriangleIndex + 0]].Position,
			Data.mVertices[Section->mIndices[TriangleIndex + 1]].Position,
			Data.mVertices[Section->mIndices[TriangleIndex + 2]].Position
		};

		swVector2 TexCoords[3];

		for (int CornerIndex = 0; CornerIndex < 3; CornerIndex++)
		{
			const float DotPlane1 = swDot(swVector4(Positions[CornerIndex], 1.0f), Plane1);
			const swVector3 PointInPlane1 = Positions[CornerIndex] - swVector3(Plane1) * DotPlane1;
			const float DotFrontPlane = swDot(swVector4(PointInPlane1, 1.0f), FrontPlane);
			const float DotPlane2 = swDot(swVector4(PointInPlane1, 1.0f), Plane2);
			const float Angle1 = atan2f(DotPlane2, DotFrontPlane) / SW_PI * Angle;

			TexCoords[CornerIndex].x = swClamp(0.5f + 0.5f * Angle1, 0.0f, 1.0f);
			TexCoords[CornerIndex].y = DotPlane1 / UpLength;
		}

		swCheckTextureCoordinatesWrap(Plane2, Positions, TexCoords);

		for (int CornerIndex = 0; CornerIndex < 3; CornerIndex++)
			Section->mIndices[TriangleIndex + CornerIndex] = AddTexturedVertex(Positions[CornerIndex], Data.mVertices[Section->mIndices[TriangleIndex + CornerIndex]].Normal, TexCoords[CornerIndex]);
	}
}

void swLibraryMeshData::GenerateSphericalTexcoords(swMeshLoaderSection* Section, const swMeshLoaderTypeData& Data)
{
	const swMeshLoaderMaterial* Material = Section->mMaterial;
	const swVector3 Front = swNormalize(Material->Points[1] - Material->Points[0]);
	const swVector3 Plane1Normal = swNormalize(swCross(Front, Material->Points[2] - Material->Points[0]));
	const swVector3 Plane2Normal = swNormalize(swCross(Plane1Normal, Front));
	const swVector4 FrontPlane = swVector4(Front, -swDot(Front, Material->Points[0]));
	const swVector3 Center = Material->Points[0];
	const swVector4 Plane1 = swVector4(Plane1Normal, -swDot(Plane1Normal, Material->Points[0]));
	const swVector4 Plane2 = swVector4(Plane2Normal, -swDot(Plane2Normal, Material->Points[0]));
	const float Angle1 = 360.0f / Material->Angles[0];
	const float Angle2 = 180.0f / Material->Angles[1];

	for (size_t TriangleIndex = 0; TriangleIndex < Section->mIndices.size(); TriangleIndex += 3)
	{
		const swVector3 Positions[3] =
		{
			Data.mVertices[Section->mIndices[TriangleIndex + 0]].Position,
			Data.mVertices[Section->mIndices[TriangleIndex + 1]].Position,
			Data.mVertices[Section->mIndices[TriangleIndex + 2]].Position
		};

		swVector2 TexCoords[3];

		for (int CornerIndex = 0; CornerIndex < 3; CornerIndex++)
		{
			const swVector3 VertexDir = Positions[CornerIndex] - Center;

			const float DotPlane1 = swDot(swVector4(Positions[CornerIndex], 1.0f), Plane1);
			const swVector3 PointInPlane1 = Positions[CornerIndex] - swVector3(Plane1) * DotPlane1;
			const float DotFrontPlane = swDot(swVector4(PointInPlane1, 1.0f), FrontPlane);
			const float DotPlane2 = swDot(swVector4(PointInPlane1, 1.0f), Plane2);

			const float AngleX = atan2f(DotPlane2, DotFrontPlane) / SW_PI * Angle1;
			TexCoords[CornerIndex].x = 0.5f + 0.5f * AngleX;

			const float AngleY = asinf(DotPlane1 / swLength(VertexDir)) / SW_PI * Angle2;
			TexCoords[CornerIndex].y = 0.5f - AngleY;
		}

		swCheckTextureCoordinatesWrap(Plane2, Positions, TexCoords);
		swCheckTextureCoordinatesPole(FrontPlane, Plane2, Positions, TexCoords);

		for (int CornerIndex = 0; CornerIndex < 3; CornerIndex++)
			Section->mIndices[TriangleIndex + CornerIndex] = AddTexturedVertex(Positions[CornerIndex], Data.mVertices[Section->mIndices[TriangleIndex + CornerIndex]].Normal, TexCoords[CornerIndex]);
	}
}

void swLibraryMeshData::GenerateTexturedVertices()
{
	for (swMeshLoaderTypeData& Data : mData)
	{
		for (const std::unique_ptr<swMeshLoaderSection>& Section : Data.mSections)
		{
			switch (Section->mMaterial->Type)
			{
				case swMeshLoaderMaterialType::Solid:
					break;

				case swMeshLoaderMaterialType::Planar:
					GeneratePlanarTexcoords(Section.get(), Data);
					break;

				case swMeshLoaderMaterialType::Cylindrical:
					GenerateCylindricalTexcoords(Section.get(), Data);
					break;

				case swMeshLoaderMaterialType::Spherical:
					GenerateSphericalTexcoords(Section.get(), Data);
					break;
			}
		}
	}
}

swMesh* swLibraryMeshData::CreateMesh(const std::atomic_bool* Cancelled)
{
	if (Cancelled && Cancelled->load(std::memory_order_relaxed))
		return nullptr;

	swMesh* Mesh = new swMesh();

	int BaseVertices[SW_MESH_DATA_TYPE_COUNT];
	int NumVertices = 0;
	int ConditionalVertexCount = 0;

	for (int MeshDataIdx = 0; MeshDataIdx < SW_MESH_DATA_TYPE_COUNT; MeshDataIdx++)
	{
		BaseVertices[MeshDataIdx] = NumVertices;
		NumVertices += static_cast<int>(mData[MeshDataIdx].mVertices.size());

		for (const std::unique_ptr<swMeshLoaderSection>& Section : mData[MeshDataIdx].mSections)
			if (Section->mPrimitiveType == SW_MESH_CONDITIONAL_LINES)
				ConditionalVertexCount += static_cast<int>(Section->mIndices.size());
	}

	if (mHasTextures)
		GenerateTexturedVertices();

	quint16 NumSections[SW_MESH_LOD_COUNT];
	int NumIndices = 0;

	std::vector<swMeshLoaderFinalSection> FinalSections[SW_MESH_LOD_COUNT];

	for (int LodIdx = 0; LodIdx < SW_MESH_LOD_COUNT; LodIdx++)
	{
		auto AddFinalSection = [](swMeshLoaderSection* Section, std::vector<swMeshLoaderFinalSection>& FinalSections)
		{
			for (const swMeshLoaderFinalSection& FinalSection : FinalSections)
				if (FinalSection.PrimitiveType == Section->mPrimitiveType && FinalSection.Color == Section->mMaterial->Color && !strcmp(FinalSection.Name, Section->mMaterial->Name))
					return;

			swMeshLoaderFinalSection& FinalSection = FinalSections.emplace_back();

			FinalSection.PrimitiveType = Section->mPrimitiveType;
			FinalSection.Color = Section->mMaterial->Color;
			swstrcpy(FinalSection.Name, Section->mMaterial->Name);
		};

		for (const std::unique_ptr<swMeshLoaderSection>& Section : mData[SW_MESH_DATA_SHARED].mSections)
		{
			if (Section->mPrimitiveType != SW_MESH_CONDITIONAL_LINES)
				NumIndices += static_cast<int>(Section->mIndices.size());

			AddFinalSection(Section.get(), FinalSections[LodIdx]);
		}

		for (const std::unique_ptr<swMeshLoaderSection>& Section : mData[LodIdx].mSections)
		{
			if (Section->mPrimitiveType != SW_MESH_CONDITIONAL_LINES)
				NumIndices += static_cast<int>(Section->mIndices.size());

			AddFinalSection(Section.get(), FinalSections[LodIdx]);
		}

		NumSections[LodIdx] = static_cast<quint16>(FinalSections[LodIdx].size());
	}

	if (Cancelled && Cancelled->load(std::memory_order_relaxed))
	{
		delete Mesh;
		return nullptr;
	}

	Mesh->Create(NumSections, NumVertices, static_cast<int>(mTexturedVertices.size()), ConditionalVertexCount, NumIndices);

	if ((Mesh->mVertexDataSize && !Mesh->mVertexData) || (Mesh->mIndexDataSize && !Mesh->mIndexData))
	{
		delete Mesh;
		return nullptr;
	}

	swVertex* DstVerts = (swVertex*)Mesh->mVertexData;

	for (const swMeshLoaderTypeData& Data : mData)
	{
		if (Cancelled && Cancelled->load(std::memory_order_relaxed))
		{
			delete Mesh;
			return nullptr;
		}

		size_t VertexIndex = 0;

		for (const swMeshLoaderVertex& SrcVertex : Data.mVertices)
		{
			if ((VertexIndex++ & 1023) == 0 && Cancelled && Cancelled->load(std::memory_order_relaxed))
			{
				delete Mesh;
				return nullptr;
			}

			swVertex& DstVertex = *DstVerts++;

			DstVertex.Position = swVector3LDrawToStudWorks(SrcVertex.Position);
			DstVertex.Normal = swPackNormal(swVector3LDrawToStudWorks(SrcVertex.Normal));
		}
	}

	swVertexTextured* DstTexturedVerts = (swVertexTextured*)DstVerts;

	if (mHasTextures)
	{
		size_t VertexIndex = 0;

		for (const swMeshLoaderTexturedVertex& SrcVertex : mTexturedVertices)
		{
			if ((VertexIndex++ & 1023) == 0 && Cancelled && Cancelled->load(std::memory_order_relaxed))
			{
				delete Mesh;
				return nullptr;
			}

			swVertexTextured& DstVertex = *DstTexturedVerts++;

			DstVertex.Position = swVector3LDrawToStudWorks(SrcVertex.Position);
			DstVertex.Normal = swPackNormal(swVector3LDrawToStudWorks(SrcVertex.Normal));
			DstVertex.TexCoord = SrcVertex.TexCoords;
		}
	}

	bool SectionsWritten;

	if (Mesh->mIndexType == GL_UNSIGNED_SHORT)
		SectionsWritten = WriteSections<quint16>(Mesh, FinalSections, BaseVertices, Cancelled);
	else
		SectionsWritten = WriteSections<quint32>(Mesh, FinalSections, BaseVertices, Cancelled);

	if (!SectionsWritten)
	{
		delete Mesh;
		return nullptr;
	}

	if (Cancelled && Cancelled->load(std::memory_order_relaxed))
	{
		delete Mesh;
		return nullptr;
	}

	// Pack each color/material group as high, shared, low. Either LOD then
	// occupies one contiguous range without duplicating the shared vertices.
	std::vector<swMeshLoaderFinalSection> ConditionalSections;
	for (int LodIdx = 0; LodIdx < SW_MESH_LOD_COUNT; LodIdx++)
		for (const swMeshLoaderFinalSection& Section : FinalSections[LodIdx])
		{
			if (Section.PrimitiveType != SW_MESH_CONDITIONAL_LINES)
				continue;

			const auto SameSection = [&Section](const swMeshLoaderFinalSection& Other)
			{
				return Section.Color == Other.Color && !strcmp(Section.Name, Other.Name);
			};
			if (std::find_if(ConditionalSections.begin(), ConditionalSections.end(), SameSection) == ConditionalSections.end())
				ConditionalSections.push_back(Section);
		}

	swVertexConditional* const ConditionalVertices = (swVertexConditional*)DstTexturedVerts;
	int ConditionalOffset = 0;
	for (const swMeshLoaderFinalSection& FinalSection : ConditionalSections)
	{
		if (Cancelled && Cancelled->load(std::memory_order_relaxed))
		{
			delete Mesh;
			return nullptr;
		}
		const auto AppendVertices = [this, &FinalSection, &ConditionalOffset, ConditionalVertices](swMeshDataType DataType)
		{
			const swMeshLoaderTypeData& Data = mData[DataType];
			for (const std::unique_ptr<swMeshLoaderSection>& Section : Data.mSections)
			{
				if (Section->mPrimitiveType != SW_MESH_CONDITIONAL_LINES || Section->mMaterial->Color != FinalSection.Color || strcmp(Section->mMaterial->Name, FinalSection.Name))
					continue;

				for (const quint32 Index : Section->mIndices)
				{
					const swMeshLoaderConditionalVertex& SrcVertex = Data.mConditionalVertices[Index];
					swVertexConditional& DstVertex = ConditionalVertices[ConditionalOffset++];
					DstVertex.Position1 = swVector3LDrawToStudWorks(SrcVertex.Position[0]);
					DstVertex.Position2 = swVector3LDrawToStudWorks(SrcVertex.Position[1]);
					DstVertex.Position3 = swVector3LDrawToStudWorks(SrcVertex.Position[2]);
					DstVertex.Position4 = swVector3LDrawToStudWorks(SrcVertex.Position[3]);
				}
			}
		};

		const int HighStart = ConditionalOffset;
		AppendVertices(SW_MESH_DATA_HIGH);
		const int SharedStart = ConditionalOffset;
		AppendVertices(SW_MESH_DATA_SHARED);
		const int LowStart = ConditionalOffset;
		AppendVertices(SW_MESH_DATA_LOW);

		for (int LodIdx = 0; LodIdx < SW_MESH_LOD_COUNT; LodIdx++)
			for (size_t SectionIdx = 0; SectionIdx < FinalSections[LodIdx].size(); SectionIdx++)
			{
				const swMeshLoaderFinalSection& Section = FinalSections[LodIdx][SectionIdx];
				if (Section.PrimitiveType != SW_MESH_CONDITIONAL_LINES || Section.Color != FinalSection.Color || strcmp(Section.Name, FinalSection.Name))
					continue;

				swMeshSection& DstSection = Mesh->mLods[LodIdx].Sections[SectionIdx];
				DstSection.DrawOffset = LodIdx == SW_MESH_LOD_HIGH ? HighStart : SharedStart;
				DstSection.DrawCount = LodIdx == SW_MESH_LOD_HIGH ? LowStart - HighStart : ConditionalOffset - SharedStart;
			}
	}

	if (mHasStyleStud)
		Mesh->mFlags |= swMeshFlag::HasStyleStud;

	if (Cancelled && Cancelled->load(std::memory_order_relaxed))
	{
		delete Mesh;
		return nullptr;
	}

	if (!UpdateMeshBoundingBox(Mesh, Cancelled))
	{
		delete Mesh;
		return nullptr;
	}

	return Mesh;
}

template<typename IndexType>
bool swLibraryMeshData::WriteSections(swMesh* Mesh, const std::vector<swMeshLoaderFinalSection> (&FinalSections)[SW_MESH_LOD_COUNT], int(&BaseVertices)[SW_MESH_DATA_TYPE_COUNT], const std::atomic_bool* Cancelled)
{
	int NumIndices = 0;

	for (int LodIdx = 0; LodIdx < SW_MESH_LOD_COUNT; LodIdx++)
	{
		for (size_t SectionIndex = 0; SectionIndex < FinalSections[LodIdx].size(); SectionIndex++)
		{
			if (Cancelled && Cancelled->load(std::memory_order_relaxed))
				return false;

			const swMeshLoaderFinalSection& FinalSection = FinalSections[LodIdx][SectionIndex];
			swMeshSection& DstSection = Mesh->mLods[LodIdx].Sections[SectionIndex];

			DstSection.ColorIndex = static_cast<int>(FinalSection.Color);
			DstSection.PrimitiveType = FinalSection.PrimitiveType;
			DstSection.DrawCount = 0;
			DstSection.TextureName = QString::fromLatin1(FinalSection.Name);

			DstSection.DrawOffset = NumIndices * sizeof(IndexType);

			IndexType* Index = Mesh->mIndexDataSize ? (IndexType*)Mesh->mIndexData + NumIndices : nullptr;

			const auto AddSection = [&DstSection, &Index, &BaseVertices, Cancelled](swMeshLoaderSection* SrcSection, swMeshDataType SrcDataType)
			{
				if (!Index && DstSection.PrimitiveType != SW_MESH_CONDITIONAL_LINES)
					return SrcSection->mIndices.empty();

				switch (DstSection.PrimitiveType)
				{
					case SW_MESH_LINES:
					case SW_MESH_TRIANGLES:
					{
						const IndexType BaseVertex = BaseVertices[SrcDataType];

						for (size_t IndexIdx = 0; IndexIdx < SrcSection->mIndices.size(); IndexIdx++)
						{
							if ((IndexIdx & 1023) == 0 && Cancelled && Cancelled->load(std::memory_order_relaxed))
								return false;

							*Index++ = BaseVertex + SrcSection->mIndices[IndexIdx];
						}
					}
					break;

					case SW_MESH_CONDITIONAL_LINES:
						return true;

					case SW_MESH_TEXTURED_TRIANGLES:
					{
						for (size_t IndexIdx = 0; IndexIdx < SrcSection->mIndices.size(); IndexIdx++)
						{
							if ((IndexIdx & 1023) == 0 && Cancelled && Cancelled->load(std::memory_order_relaxed))
								return false;

							*Index++ = SrcSection->mIndices[IndexIdx];
						}
					}
					break;

					case SW_MESH_PRIMITIVE_TYPE_COUNT:
						break;
				}

				DstSection.DrawCount += static_cast<int>(SrcSection->mIndices.size());

				return true;
			};

			for (const std::unique_ptr<swMeshLoaderSection>& Section : mData[SW_MESH_DATA_SHARED].mSections)
				if (FinalSection.PrimitiveType == Section->mPrimitiveType && FinalSection.Color == Section->mMaterial->Color && !strcmp(FinalSection.Name, Section->mMaterial->Name))
					if (!AddSection(Section.get(), SW_MESH_DATA_SHARED))
						return false;

			const swMeshDataType MeshDataType = (LodIdx == SW_MESH_LOD_LOW) ? SW_MESH_DATA_LOW : SW_MESH_DATA_HIGH;

			for (const std::unique_ptr<swMeshLoaderSection>& Section : mData[MeshDataType].mSections)
				if (FinalSection.PrimitiveType == Section->mPrimitiveType && FinalSection.Color == Section->mMaterial->Color && !strcmp(FinalSection.Name, Section->mMaterial->Name))
					if (!AddSection(Section.get(), MeshDataType))
						return false;

			if (DstSection.PrimitiveType != SW_MESH_TRIANGLES && DstSection.PrimitiveType != SW_MESH_TEXTURED_TRIANGLES)
				Mesh->mFlags |= swMeshFlag::HasLines;

			if (DstSection.PrimitiveType == SW_MESH_TEXTURED_TRIANGLES)
				Mesh->mFlags |= swMeshFlag::HasTexture;

			NumIndices += DstSection.DrawCount;
		}
	}

	return true;
}

bool swLibraryMeshData::UpdateMeshBoundingBox(swMesh* Mesh, const std::atomic_bool* Cancelled)
{
	swVector3 MeshMin(FLT_MAX, FLT_MAX, FLT_MAX), MeshMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
	bool UpdatedBoundingBox = false;

	for (int LodIdx = 0; LodIdx < SW_MESH_LOD_COUNT; LodIdx++)
	{
		swMeshLod& Lod = Mesh->mLods[LodIdx];

		for (int SectionIdx = 0; SectionIdx < Lod.NumSections; SectionIdx++)
		{
			if (Cancelled && Cancelled->load(std::memory_order_relaxed))
				return false;

			swMeshSection& Section = Lod.Sections[SectionIdx];
			swVector3 SectionMin(FLT_MAX, FLT_MAX, FLT_MAX), SectionMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);

			if (Mesh->mIndexType == GL_UNSIGNED_SHORT)
			{
				if (!UpdateMeshSectionBoundingBox<quint16>(Mesh, Section, SectionMin, SectionMax, Cancelled))
					return false;
			}
			else
			{
				if (!UpdateMeshSectionBoundingBox<quint32>(Mesh, Section, SectionMin, SectionMax, Cancelled))
					return false;
			}

			Section.BoundingBox.Max = SectionMax;
			Section.BoundingBox.Min = SectionMin;
			Section.Radius = swLength((SectionMax - SectionMin) / 2.0f);

			if (Section.PrimitiveType == SW_MESH_TRIANGLES || Section.PrimitiveType == SW_MESH_TEXTURED_TRIANGLES)
			{
				UpdatedBoundingBox = true;
				MeshMin = swMinimum(SectionMin, MeshMin);
				MeshMax = swMaximum(SectionMax, MeshMax);
			}
		}
	}

	if (!UpdatedBoundingBox)
		MeshMin = MeshMax = swVector3(0.0f, 0.0f, 0.0f);

	Mesh->mBoundingBox.Max = MeshMax;
	Mesh->mBoundingBox.Min = MeshMin;
	Mesh->mRadius = swLength((MeshMax - MeshMin) / 2.0f);

	return true;
}

template<typename IndexType>
bool swLibraryMeshData::UpdateMeshSectionBoundingBox(const swMesh* Mesh, const swMeshSection& Section, swVector3& SectionMin, swVector3& SectionMax, const std::atomic_bool* Cancelled)
{
	switch (Section.PrimitiveType)
	{
		case SW_MESH_LINES:
		case SW_MESH_TRIANGLES:
		{
			const IndexType* IndexBuffer = reinterpret_cast<IndexType*>(static_cast<char*>(Mesh->mIndexData) + Section.DrawOffset);
			const swVertex* VertexBuffer = Mesh->GetVertexData();

			for (int Index = 0; Index < Section.DrawCount; Index++)
			{
				if ((Index & 1023) == 0 && Cancelled && Cancelled->load(std::memory_order_relaxed))
					return false;

				const swVector3& Position = VertexBuffer[IndexBuffer[Index]].Position;
				SectionMin = swMinimum(SectionMin, Position);
				SectionMax = swMaximum(SectionMax, Position);
			}
		}
		break;

		case SW_MESH_CONDITIONAL_LINES:
		{
			const swVertexConditional* VertexBuffer = Mesh->GetConditionalVertexData();

			for (int Index = 0; Index < Section.DrawCount; Index++)
			{
				if ((Index & 1023) == 0 && Cancelled && Cancelled->load(std::memory_order_relaxed))
					return false;

				const swVector3& Position = VertexBuffer[Section.DrawOffset + Index].Position1;
				SectionMin = swMinimum(SectionMin, Position);
				SectionMax = swMaximum(SectionMax, Position);
			}
		}
		break;

		case SW_MESH_TEXTURED_TRIANGLES:
		{
			const IndexType* IndexBuffer = reinterpret_cast<IndexType*>(static_cast<char*>(Mesh->mIndexData) + Section.DrawOffset);
			const swVertexTextured* VertexBuffer = Mesh->GetTexturedVertexData();

			for (int Index = 0; Index < Section.DrawCount; Index++)
			{
				if ((Index & 1023) == 0 && Cancelled && Cancelled->load(std::memory_order_relaxed))
					return false;

				const swVector3& Position = VertexBuffer[IndexBuffer[Index]].Position;
				SectionMin = swMinimum(SectionMin, Position);
				SectionMax = swMaximum(SectionMax, Position);
			}
		}
		break;

		case SW_MESH_PRIMITIVE_TYPE_COUNT:
			break;
	}

	return true;
}

swMeshLoader::swMeshLoader(swLibraryMeshData& MeshData, Project* CurrentProject, swMeshLoaderFlags Flags)
	: mCurrentProject(CurrentProject), mMeshData(MeshData), mFlags(Flags)
{
	MeshData.SetMeshLoader(this);
}

bool swMeshLoader::LoadMesh(swFile& File, swMeshDataType MeshDataType)
{
	mTextureStack.clear();
	mError.clear();

	return ReadMeshData(File, swMatrix44Identity(), 16, false, MeshDataType);
}

std::vector<std::string> swMeshLoader::GetTextureReferences(const QByteArray& FileData)
{
	std::vector<std::string> Names;
	const QList<QByteArray> Lines = FileData.split('\n');

	for (int Index = 0; Index < Lines.size(); Index++)
	{
		QString Line = QString::fromUtf8(Lines[Index]).trimmed();
		QTextStream Stream(&Line, QIODevice::ReadOnly);
		QString LineType, Meta, Action, Projection;
		Stream >> LineType >> Meta >> Action >> Projection;

		if (LineType != QLatin1String("0") || Meta != QLatin1String("!TEXMAP") ||
			(Action != QLatin1String("START") && Action != QLatin1String("NEXT")))
			continue;

		if (Action == QLatin1String("NEXT"))
		{
			int Next = Index + 1;

			while (Next < Lines.size() && Lines[Next].trimmed().isEmpty())
				Next++;

			if (Next == Lines.size())
				continue;

			QString NextLine = QString::fromUtf8(Lines[Next]);
			QTextStream NextStream(&NextLine, QIODevice::ReadOnly);
			QString NextType;
			NextStream >> NextType;

			if (NextType == QLatin1String("0"))
				continue;
		}

		int Parameters = 0;

		if (Projection == QLatin1String("PLANAR"))
			Parameters = 9;
		else if (Projection == QLatin1String("CYLINDRICAL"))
			Parameters = 10;
		else if (Projection == QLatin1String("SPHERICAL"))
			Parameters = 11;
		else
			continue;

		bool Valid = true;

		for (int Parameter = 0; Parameter < Parameters; Parameter++)
		{
			QString Token;
			Stream >> Token;
			bool Parsed;
			Token.toFloat(&Parsed);
			Valid &= Parsed;
		}

		if (!Valid)
			continue;

		const QByteArray Text = Stream.readAll().toLatin1();
		char Name[SW_TEXTURE_NAME_LENGTH];

		const char* End = nullptr;

		if (!ParseTextureName(Text.constData(), Name, sizeof(Name), &End))
			continue;

		if (std::find(Names.begin(), Names.end(), Name) == Names.end())
			Names.emplace_back(Name);

		while (*End && *End <= 32)
			End++;

		if (!strncmp(End, "GLOSSMAP", 8) && (!End[8] || End[8] <= 32) &&
			ParseTextureName(End + 8, Name, sizeof(Name), nullptr) && std::find(Names.begin(), Names.end(), Name) == Names.end())
			Names.emplace_back(Name);
	}

	return Names;
}

bool swMeshLoader::ParseTextureName(const char* Text, char* Name, size_t NameSize, const char** EndOfName)
{
	while (*Text && *Text <= 32)
		Text++;

	const bool Quoted = *Text == '"';

	if (Quoted)
		Text++;

	size_t Length = 0;

	while (*Text && (Quoted ? *Text != '"' : *Text > 32))
	{
		char Character = *Text++;

		if (Character == '\\' && (*Text == '"' || *Text == '\\'))
			Character = *Text++;

		if (Length + 1 >= NameSize || Length + 1 >= SW_TEXTURE_NAME_LENGTH)
			return false;

		if (Character >= 'a' && Character <= 'z')
			Character += 'A' - 'a';
		else if (Character == '\\')
			Character = '/';

		Name[Length++] = Character;
	}

	if (!Length || (Quoted && *Text != '"'))
		return false;

	Name[Length] = 0;

	if (Length > 4 && !memcmp(Name + Length - 4, ".PNG", 4))
		Name[Length - 4] = 0;

	if (EndOfName)
		*EndOfName = Text + (Quoted ? 1 : 0);

	return true;
}

bool swMeshLoader::ReadMeshData(swFile& File, const swMatrix44& CurrentTransform, quint32 CurrentColorCode, bool InvertWinding, swMeshDataType MeshDataType)
{
	char Buffer[1024];
	char* Line;
	const size_t TextureStackStart = mTextureStack.size();
	bool InvertNext = false;
	bool WindingCCW = !InvertWinding;
	swPiecesLibrary* Library = swGetPiecesLibrary();
	const bool Optimize = mFlags.testFlag(swMeshLoaderFlag::Optimize);

	while (File.ReadLine(Buffer, sizeof(Buffer)))
	{
		if (Library->ShouldCancelLoading() || (mCancelled && mCancelled->load(std::memory_order_relaxed)))
			return false;

		bool LastToken = false;
		int LineType;

		Line = Buffer;

		if (sscanf(Line, "%d", &LineType) != 1)
			continue;

		// TEXMAP NEXT is ignored if the next non-blank line is type 0.
		if (LineType == 0 && mTextureStack.size() > TextureStackStart && mTextureStack.back().Next)
			mTextureStack.pop_back();

		if (LineType == 0)
		{
			char* Token = Line;

			while (*Token && *Token <= 32)
				Token++;

			Token++;

			while (*Token && *Token <= 32)
				Token++;

			char* End = Token;
			while (*End && *End > 32)
				End++;

			LastToken = (*End == 0);
			*End = 0;

			if (!strcmp(Token, "!TEXMAP"))
			{
				Token += 8;

				while (*Token && *Token <= 32)
					Token++;

				End = Token;
				while (*End && *End > 32)
					End++;
				*End = 0;

				bool Start = false;
				bool Next = false;

				if (!strcmp(Token, "START"))
				{
					Token += 6;
					Start = true;
				}
				else if (!strcmp(Token, "NEXT"))
				{
					Token += 5;
					Next = true;
				}

				if (Start || Next)
				{
					while (*Token && *Token <= 32)
						Token++;

					End = Token;
					while (*End && *End > 32)
						End++;
					*End = 0;


					if (!strcmp(Token, "PLANAR"))
					{
						Token += 7;

						mTextureStack.emplace_back();
						swMeshLoaderTextureMap& Map = mTextureStack.back();
						Map.Type = swMeshLoaderMaterialType::Planar;
						Map.Next = Next;

						swVector3 (&Points)[3] = Map.Points;

						int NameOffset = 0;
						const int Parsed = sscanf(Token, "%f %f %f %f %f %f %f %f %f %n", &Points[0].x, &Points[0].y, &Points[0].z, &Points[1].x, &Points[1].y, &Points[1].z, &Points[2].x, &Points[2].y, &Points[2].z, &NameOffset);

						if (Parsed != 9 || !ParseTextureName(Token + NameOffset, Map.Name, sizeof(Map.Name), nullptr))
						{
							mTextureStack.pop_back();
							continue;
						}

						Points[0] = swMultiply31(Points[0], CurrentTransform);
						Points[1] = swMultiply31(Points[1], CurrentTransform);
						Points[2] = swMultiply31(Points[2], CurrentTransform);
					}
					else if (!strcmp(Token, "CYLINDRICAL"))
					{
						Token += 12;

						mTextureStack.emplace_back();
						swMeshLoaderTextureMap& Map = mTextureStack.back();
						Map.Type = swMeshLoaderMaterialType::Cylindrical;
						Map.Next = Next;

						swVector3 (&Points)[3] = Map.Points;
						float& Angle = Map.Angles[0];

						int NameOffset = 0;
						const int Parsed = sscanf(Token, "%f %f %f %f %f %f %f %f %f %f %n", &Points[0].x, &Points[0].y, &Points[0].z, &Points[1].x, &Points[1].y, &Points[1].z, &Points[2].x, &Points[2].y, &Points[2].z, &Angle, &NameOffset);

						if (Parsed != 10 || !ParseTextureName(Token + NameOffset, Map.Name, sizeof(Map.Name), nullptr))
						{
							mTextureStack.pop_back();
							continue;
						}

						Points[0] = swMultiply31(Points[0], CurrentTransform);
						Points[1] = swMultiply31(Points[1], CurrentTransform);
						Points[2] = swMultiply31(Points[2], CurrentTransform);
					}
					else if (!strcmp(Token, "SPHERICAL"))
					{
						Token += 10;

						mTextureStack.emplace_back();
						swMeshLoaderTextureMap& Map = mTextureStack.back();
						Map.Type = swMeshLoaderMaterialType::Spherical;
						Map.Next = Next;

						swVector3(&Points)[3] = Map.Points;
						float& Angle1 = Map.Angles[0];
						float& Angle2 = Map.Angles[1];

						int NameOffset = 0;
						const int Parsed = sscanf(Token, "%f %f %f %f %f %f %f %f %f %f %f %n", &Points[0].x, &Points[0].y, &Points[0].z, &Points[1].x, &Points[1].y, &Points[1].z, &Points[2].x, &Points[2].y, &Points[2].z, &Angle1, &Angle2, &NameOffset);

						if (Parsed != 11 || !ParseTextureName(Token + NameOffset, Map.Name, sizeof(Map.Name), nullptr))
						{
							mTextureStack.pop_back();
							continue;
						}

						Points[0] = swMultiply31(Points[0], CurrentTransform);
						Points[1] = swMultiply31(Points[1], CurrentTransform);
						Points[2] = swMultiply31(Points[2], CurrentTransform);
					}
				}
				else if (!strcmp(Token, "FALLBACK"))
				{
					if (mTextureStack.size() > TextureStackStart)
						mTextureStack.back().Fallback = true;
				}
				else if (!strcmp(Token, "END"))
				{
					if (mTextureStack.size() > TextureStackStart)
						mTextureStack.pop_back();
				}

				continue;
			}
			else if (!strcmp(Token, "BFC"))
			{
				while (!LastToken)
				{
					Token = End + 1;

					while (*Token && *Token <= 32)
						Token++;

					End = Token;
					while (*End && *End > 32)
						End++;

					LastToken = (*End == 0);
					*End = 0;

					if (!strcmp(Token, "INVERTNEXT"))
						InvertNext = true;
					else if (!strcmp(Token, "CCW"))
						WindingCCW = !InvertWinding;
					else if (!strcmp(Token, "CW"))
						WindingCCW = InvertWinding;
				}
			}
			else if (!strcmp(Token, "!:"))
			{
				Token += 3;

				Line = Token;

				if (mTextureStack.empty())
					continue;
			}
			else
				continue;
		}

		int ParsedColorCode;
		if (sscanf(Line, "%d %d", &LineType, &ParsedColorCode) != 2)
			continue;

		if (LineType < 1 || LineType > 5)
			continue;

		quint32 ColorCode = static_cast<quint32>(ParsedColorCode);
		if (ColorCode == 0)
		{
			int ParsedHexColorCode;
			if (sscanf(Line, "%d %i", &LineType, &ParsedHexColorCode) != 2)
				continue;

			const quint32 ColorCodeHex = static_cast<quint32>(ParsedHexColorCode);
			if (ColorCode != ColorCodeHex)
				ColorCode = ColorCodeHex | SW_COLOR_DIRECT;
		}

		if (ColorCode == 16)
			ColorCode = CurrentColorCode;

		swMeshLoaderTextureMap* TextureMap = nullptr;

		if (!mTextureStack.empty())
		{
			TextureMap = &mTextureStack.back();

			// TODO: think about a way to handle the texture fallback
//			if (TextureMap->Texture)
			{
				if (TextureMap->Fallback)
					continue;
			}
//			else
//			{
//				if (!TextureMap->Fallback)
//					continue;
//
//				TextureMap = nullptr;
//			}
		}

		int Dummy;
		swVector3 Points[4];
		const bool ConsumeNext = mTextureStack.size() > TextureStackStart && mTextureStack.back().Next;

		switch (LineType)
		{
		case 1:
		{
			char OriginalFileName[SW_MAXIMUM_PATH];
			float fm[12];

			if (sscanf(Line, "%d %i %f %f %f %f %f %f %f %f %f %f %f %f %1023s", &LineType, &Dummy, &fm[0], &fm[1], &fm[2], &fm[3], &fm[4], &fm[5], &fm[6], &fm[7], &fm[8], &fm[9], &fm[10], &fm[11], OriginalFileName) != 15)
				break;

			char FileName[SW_MAXIMUM_PATH];
			swstrcpy(FileName, OriginalFileName);

			char* Ch;
			for (Ch = FileName; *Ch; Ch++)
			{
				if (*Ch >= 'a' && *Ch <= 'z')
					*Ch = *Ch + 'A' - 'a';
				else if (*Ch == '\\')
					*Ch = '/';
			}

			swLibraryPrimitive* Primitive = !TextureMap ? Library->FindPrimitive(FileName) : nullptr;
			swMatrix44 IncludeTransform(swVector4(fm[3], fm[6], fm[9], 0.0f), swVector4(fm[4], fm[7], fm[10], 0.0f), swVector4(fm[5], fm[8], fm[11], 0.0f), swVector4(fm[0], fm[1], fm[2], 1.0f));
			IncludeTransform = swMultiply(IncludeTransform, CurrentTransform);
			bool Mirror = IncludeTransform.Determinant() < 0.0f;
			bool IncludeLoaded = true;

			if (Primitive)
			{
				// Read shared loading state under the library's primitive lock.
				const swResult<void> Result = Library->LoadPrimitive(Primitive);

				if (!Result)
				{
					mError = Result.error();
					return false;
				}

				if (Primitive->mStud)
					mMeshData.AddMeshDataNoDuplicateCheck(Primitive->mMeshData, IncludeTransform, ColorCode, Mirror ^ InvertNext, InvertNext, TextureMap, MeshDataType);
				else if (!Primitive->mSubFile)
				{
					if (Optimize)
						mMeshData.AddMeshData(Primitive->mMeshData, IncludeTransform, ColorCode, Mirror ^ InvertNext, InvertNext, TextureMap, MeshDataType);
					else
						mMeshData.AddMeshDataNoDuplicateCheck(Primitive->mMeshData, IncludeTransform, ColorCode, Mirror ^ InvertNext, InvertNext, TextureMap, MeshDataType);
				}
				else
				{
					const swResult<std::unique_ptr<swFile>> IncludeFile = Library->OpenPrimitiveFile(Primitive);

					if (IncludeFile)
						IncludeLoaded = ReadMeshData(*IncludeFile.value(), IncludeTransform, ColorCode, Mirror ^ InvertNext, MeshDataType);
					else
						IncludeLoaded = !mFlags.testFlag(swMeshLoaderFlag::RequireAllIncludes);
				}

				mMeshData.mHasStyleStud |= Primitive->mStudStyle | Primitive->mMeshData.mHasStyleStud;
			}
			else
			{
				const swResult<std::unique_ptr<swFile>> IncludeFile = Library->OpenPieceDataFile(FileName);

				if (IncludeFile)
					IncludeLoaded = ReadMeshData(*IncludeFile.value(), IncludeTransform, ColorCode, Mirror ^ InvertNext, MeshDataType);
				else
					IncludeLoaded = !mFlags.testFlag(swMeshLoaderFlag::RequireAllIncludes);
			}

			if (!IncludeLoaded)
			{
				if (mError.isEmpty())
					mError = tr("Could not load included file '%1'.").arg(QString::fromLatin1(OriginalFileName));

				return false;
			}
		} break;

		case 2:
			if (sscanf(Line, "%d %i %f %f %f %f %f %f", &LineType, &Dummy, &Points[0].x, &Points[0].y, &Points[0].z, &Points[1].x, &Points[1].y, &Points[1].z) != 8)
				break;

			Points[0] = swMultiply31(Points[0], CurrentTransform);
			Points[1] = swMultiply31(Points[1], CurrentTransform);

			mMeshData.mData[MeshDataType].ProcessLine(LineType, mMeshData.GetMaterial(ColorCode), WindingCCW, Points, Optimize);
			break;

		case 3:
			if (sscanf(Line, "%d %i %f %f %f %f %f %f %f %f %f", &LineType, &Dummy, &Points[0].x, &Points[0].y, &Points[0].z,
				   &Points[1].x, &Points[1].y, &Points[1].z, &Points[2].x, &Points[2].y, &Points[2].z) != 11)
				break;

			Points[0] = swMultiply31(Points[0], CurrentTransform);
			Points[1] = swMultiply31(Points[1], CurrentTransform);
			Points[2] = swMultiply31(Points[2], CurrentTransform);

			if (!TextureMap)
				mMeshData.mData[MeshDataType].ProcessLine(LineType, mMeshData.GetMaterial(ColorCode), WindingCCW, Points, Optimize);
			else
			{
				mMeshData.mHasTextures = true;
				mMeshData.mData[MeshDataType].ProcessLine(LineType, mMeshData.GetTexturedMaterial(ColorCode, *TextureMap), WindingCCW, Points, Optimize);

			}
			break;

		case 4:
			if (sscanf(Line, "%d %i %f %f %f %f %f %f %f %f %f %f %f %f", &LineType, &Dummy, &Points[0].x, &Points[0].y, &Points[0].z,
				   &Points[1].x, &Points[1].y, &Points[1].z, &Points[2].x, &Points[2].y, &Points[2].z, &Points[3].x, &Points[3].y, &Points[3].z) != 14)
				break;

			Points[0] = swMultiply31(Points[0], CurrentTransform);
			Points[1] = swMultiply31(Points[1], CurrentTransform);
			Points[2] = swMultiply31(Points[2], CurrentTransform);
			Points[3] = swMultiply31(Points[3], CurrentTransform);

			if (!TextureMap)
				mMeshData.mData[MeshDataType].ProcessLine(LineType, mMeshData.GetMaterial(ColorCode), WindingCCW, Points, Optimize);
			else
			{
				mMeshData.mHasTextures = true;
				mMeshData.mData[MeshDataType].ProcessLine(LineType, mMeshData.GetTexturedMaterial(ColorCode, *TextureMap), WindingCCW, Points, Optimize);

			}
			break;

		case 5:
			if (sscanf(Line, "%d %i %f %f %f %f %f %f %f %f %f %f %f %f", &LineType, &Dummy, &Points[0].x, &Points[0].y, &Points[0].z,
				   &Points[1].x, &Points[1].y, &Points[1].z, &Points[2].x, &Points[2].y, &Points[2].z, &Points[3].x, &Points[3].y, &Points[3].z) != 14)
				break;

			Points[0] = swMultiply31(Points[0], CurrentTransform);
			Points[1] = swMultiply31(Points[1], CurrentTransform);
			Points[2] = swMultiply31(Points[2], CurrentTransform);
			Points[3] = swMultiply31(Points[3], CurrentTransform);

			mMeshData.mData[MeshDataType].ProcessLine(LineType, mMeshData.GetMaterial(ColorCode), WindingCCW, Points, Optimize);
			break;
		}

		if (ConsumeNext)
			mTextureStack.pop_back();

		InvertNext = false;
	}

	mTextureStack.resize(TextureStackStart);

	return true;
}
