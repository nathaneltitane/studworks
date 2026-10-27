#include "sw_global.h"
#include "sw_mesh.h"
#include "sw_colors.h"
#include "sw_texture.h"
#include "sw_file.h"
#include "sw_math.h"
#include "sw_application.h"
#include "sw_library.h"

#define SW_MESH_FILE_ID      SW_FOURCC('M', 'E', 'S', 'H')
#define SW_MESH_FILE_VERSION 0x0123

swMesh::swMesh()
{
}

swMesh::~swMesh()
{
	free(mVertexData);
	free(mIndexData);
	for (int LodIdx = 0; LodIdx < SW_MESH_LOD_COUNT; LodIdx++)
	{
		delete[] mLods[LodIdx].Sections;
	}
}

void swMesh::Create(quint16(&NumSections)[SW_MESH_LOD_COUNT], int VertexCount, int TexturedVertexCount, int ConditionalVertexCount, int IndexCount)
{
	for (int LodIdx = 0; LodIdx < SW_MESH_LOD_COUNT; LodIdx++)
	{
		if (NumSections[LodIdx])
			mLods[LodIdx].Sections = new swMeshSection[NumSections[LodIdx]];
		mLods[LodIdx].NumSections = NumSections[LodIdx];
	}

	mNumVertices = VertexCount;
	mNumTexturedVertices = TexturedVertexCount;
	mConditionalVertexCount = ConditionalVertexCount;
	mVertexDataSize = VertexCount * sizeof(swVertex) + TexturedVertexCount * sizeof(swVertexTextured) + ConditionalVertexCount * sizeof(swVertexConditional);
	mVertexData = malloc(mVertexDataSize);

	if (VertexCount < 0x10000 && TexturedVertexCount < 0x10000)
	{
		mIndexType = GL_UNSIGNED_SHORT;
		mIndexDataSize = IndexCount * sizeof(GLushort);
	}
	else
	{
		mIndexType = GL_UNSIGNED_INT;
		mIndexDataSize = IndexCount * sizeof(GLuint);
	}

	mIndexData = mIndexDataSize ? malloc(mIndexDataSize) : nullptr;
}

void swMesh::CreateBox()
{
	quint16 NumSections[SW_MESH_LOD_COUNT];
	memset(NumSections, 0, sizeof(NumSections));
	NumSections[SW_MESH_LOD_HIGH] = 2;

	Create(NumSections, 24, 0, 0, 36 + 24);

	swVector3 Min(-10.0f, -10.0f, -24.0f);
	swVector3 Max(10.0f, 10.0f, 4.0f);
	mRadius = swLength(Max - Min) / 2.0f;
	mBoundingBox.Min = Min;
	mBoundingBox.Max = Max;
	mFlags |= swMeshFlag::HasDefault | swMeshFlag::HasLines;

	swVertex* Verts = (swVertex*)mVertexData;
	quint16* Indices = (quint16*)mIndexData;

	Verts[0].Position = swVector3(Min[0], Min[1], Min[2]);
	Verts[0].Normal = swPackNormal(swVector3(0.0f, 0.0f, -1.0f));
	Verts[1].Position = swVector3(Min[0], Max[1], Min[2]);
	Verts[1].Normal = swPackNormal(swVector3(0.0f, 0.0f, -1.0f));
	Verts[2].Position = swVector3(Max[0], Max[1], Min[2]);
	Verts[2].Normal = swPackNormal(swVector3(0.0f, 0.0f, -1.0f));
	Verts[3].Position = swVector3(Max[0], Min[1], Min[2]);
	Verts[3].Normal = swPackNormal(swVector3(0.0f, 0.0f, -1.0f));
	Verts[4].Position = swVector3(Min[0], Min[1], Max[2]);
	Verts[4].Normal = swPackNormal(swVector3(0.0f, 0.0f, 1.0f));
	Verts[5].Position = swVector3(Min[0], Max[1], Max[2]);
	Verts[5].Normal = swPackNormal(swVector3(0.0f, 0.0f, 1.0f));
	Verts[6].Position = swVector3(Max[0], Max[1], Max[2]);
	Verts[6].Normal = swPackNormal(swVector3(0.0f, 0.0f, 1.0f));
	Verts[7].Position = swVector3(Max[0], Min[1], Max[2]);
	Verts[7].Normal = swPackNormal(swVector3(0.0f, 0.0f, 1.0f));

	Verts[8].Position = swVector3(Min[0], Min[1], Min[2]);
	Verts[8].Normal = swPackNormal(swVector3(-1.0f, 0.0f, 0.0f));
	Verts[9].Position = swVector3(Min[0], Min[1], Max[2]);
	Verts[9].Normal = swPackNormal(swVector3(-1.0f, 0.0f, 0.0f));
	Verts[10].Position = swVector3(Min[0], Max[1], Max[2]);
	Verts[10].Normal = swPackNormal(swVector3(-1.0f, 0.0f, 0.0f));
	Verts[11].Position = swVector3(Min[0], Max[1], Min[2]);
	Verts[11].Normal = swPackNormal(swVector3(-1.0f, 0.0f, 0.0f));
	Verts[12].Position = swVector3(Max[0], Min[1], Min[2]);
	Verts[12].Normal = swPackNormal(swVector3(1.0f, 0.0f, 0.0f));
	Verts[13].Position = swVector3(Max[0], Min[1], Max[2]);
	Verts[13].Normal = swPackNormal(swVector3(1.0f, 0.0f, 0.0f));
	Verts[14].Position = swVector3(Max[0], Max[1], Max[2]);
	Verts[14].Normal = swPackNormal(swVector3(1.0f, 0.0f, 0.0f));
	Verts[15].Position = swVector3(Max[0], Max[1], Min[2]);
	Verts[15].Normal = swPackNormal(swVector3(1.0f, 0.0f, 0.0f));

	Verts[16].Position = swVector3(Min[0], Min[1], Min[2]);
	Verts[16].Normal = swPackNormal(swVector3(0.0f, -1.0f, 0.0f));
	Verts[17].Position = swVector3(Min[0], Min[1], Max[2]);
	Verts[17].Normal = swPackNormal(swVector3(0.0f, -1.0f, 0.0f));
	Verts[18].Position = swVector3(Max[0], Min[1], Max[2]);
	Verts[18].Normal = swPackNormal(swVector3(0.0f, -1.0f, 0.0f));
	Verts[19].Position = swVector3(Max[0], Min[1], Min[2]);
	Verts[19].Normal = swPackNormal(swVector3(0.0f, -1.0f, 0.0f));
	Verts[20].Position = swVector3(Min[0], Max[1], Min[2]);
	Verts[20].Normal = swPackNormal(swVector3(0.0f, 1.0f, 0.0f));
	Verts[21].Position = swVector3(Min[0], Max[1], Max[2]);
	Verts[21].Normal = swPackNormal(swVector3(0.0f, 1.0f, 0.0f));
	Verts[22].Position = swVector3(Max[0], Max[1], Max[2]);
	Verts[22].Normal = swPackNormal(swVector3(0.0f, 1.0f, 0.0f));
	Verts[23].Position = swVector3(Max[0], Max[1], Min[2]);
	Verts[23].Normal = swPackNormal(swVector3(0.0f, 1.0f, 0.0f));

	swMeshSection* Section = &mLods[SW_MESH_LOD_HIGH].Sections[0];
	Section->ColorIndex = gDefaultColor;
	Section->DrawOffset = 0;
	Section->DrawCount = 36;
	Section->PrimitiveType = SW_MESH_TRIANGLES;
	Section->BoundingBox = mBoundingBox;
	Section->Radius = mRadius;

	*Indices++ = 0; *Indices++ = 1; *Indices++ = 2;
	*Indices++ = 0; *Indices++ = 2; *Indices++ = 3;

	*Indices++ = 7; *Indices++ = 6; *Indices++ = 5;
	*Indices++ = 7; *Indices++ = 5; *Indices++ = 4;

	*Indices++ = 8; *Indices++ = 9; *Indices++ = 10;
	*Indices++ = 8; *Indices++ = 10; *Indices++ = 11;

	*Indices++ = 15; *Indices++ = 14; *Indices++ = 13;
	*Indices++ = 15; *Indices++ = 13; *Indices++ = 12;

	*Indices++ = 16; *Indices++ = 17; *Indices++ = 18;
	*Indices++ = 16; *Indices++ = 18; *Indices++ = 19;

	*Indices++ = 23; *Indices++ = 22; *Indices++ = 21;
	*Indices++ = 23; *Indices++ = 21; *Indices++ = 20;

	Section = &mLods[SW_MESH_LOD_HIGH].Sections[1];
	Section->ColorIndex = gEdgeColor;
	Section->DrawOffset = 36 * 2;
	Section->DrawCount = 24;
	Section->PrimitiveType = SW_MESH_LINES;
	Section->BoundingBox = mBoundingBox;
	Section->Radius = mRadius;

	*Indices++ = 0; *Indices++ = 1; *Indices++ = 1; *Indices++ = 2;
	*Indices++ = 2; *Indices++ = 3; *Indices++ = 3; *Indices++ = 0;

	*Indices++ = 4; *Indices++ = 5; *Indices++ = 5; *Indices++ = 6;
	*Indices++ = 6; *Indices++ = 7; *Indices++ = 7; *Indices++ = 4;

	*Indices++ = 0; *Indices++ = 4; *Indices++ = 1; *Indices++ = 5;
	*Indices++ = 2; *Indices++ = 6; *Indices++ = 3; *Indices++ = 7;
}

template<typename IndexType>
bool swMesh::MinIntersectDist(const swVector3& Start, const swVector3& End, float& MinDistance, swVector3& HitPlane, swVector3& HitNormal)
{
	float Distance;
	swVector3 IntersectionPlane;
	swVector3 TriangleNormal;

	if (!swBoundingBoxRayIntersectDistance(mBoundingBox.Min, mBoundingBox.Max, Start, End, &Distance, nullptr, &IntersectionPlane) || (Distance >= MinDistance))
		return false;

	bool Hit = false;
	swVector3 Intersection;

	for (int SectionIdx = 0; SectionIdx < mLods[SW_MESH_LOD_HIGH].NumSections; SectionIdx++)
	{
		const swMeshSection* Section = &mLods[SW_MESH_LOD_HIGH].Sections[SectionIdx];

		if (Section->PrimitiveType == SW_MESH_TRIANGLES)
		{
			const swVertex* Verts = GetVertexData();
			const IndexType* Indices = (IndexType*)mIndexData + Section->DrawOffset / sizeof(IndexType);

			for (int Idx = 0; Idx < Section->DrawCount; Idx += 3)
			{
				const swVector3& v1 = Verts[Indices[Idx]].Position;
				const swVector3& v2 = Verts[Indices[Idx + 1]].Position;
				const swVector3& v3 = Verts[Indices[Idx + 2]].Position;

				if (swLineTriangleMinimumIntersection(v1, v2, v3, Start, End, &MinDistance, &Intersection))
				{
					Hit = true;
					TriangleNormal = swCross(v2 - v1, v3 - v1);
				}
			}
		}
		else if (Section->PrimitiveType == SW_MESH_TEXTURED_TRIANGLES)
		{
			const swVertexTextured* Verts = GetTexturedVertexData();
			const IndexType* Indices = (IndexType*)mIndexData + Section->DrawOffset / sizeof(IndexType);

			for (int Idx = 0; Idx < Section->DrawCount; Idx += 3)
			{
				const swVector3& v1 = Verts[Indices[Idx]].Position;
				const swVector3& v2 = Verts[Indices[Idx + 1]].Position;
				const swVector3& v3 = Verts[Indices[Idx + 2]].Position;

				if (swLineTriangleMinimumIntersection(v1, v2, v3, Start, End, &MinDistance, &Intersection))
				{
					Hit = true;
					TriangleNormal = swCross(v2 - v1, v3 - v1);
				}
			}
		}
	}

	if (Hit)
	{
		HitPlane = IntersectionPlane;
		HitNormal = swNormalize(TriangleNormal);
	}

	return Hit;
}

bool swMesh::MinIntersectDist(const swVector3& Start, const swVector3& End, float& MinDist, swVector3& HitPlane, swVector3& HitNormal)
{
	if (mIndexType == GL_UNSIGNED_SHORT)
		return MinIntersectDist<GLushort>(Start, End, MinDist, HitPlane, HitNormal);
	else
		return MinIntersectDist<GLuint>(Start, End, MinDist, HitPlane, HitNormal);
}

template<typename IndexType>
bool swMesh::IntersectsPlanes(const swVector4 (&Planes)[6])
{
	for (int SectionIdx = 0; SectionIdx < mLods[SW_MESH_LOD_HIGH].NumSections; SectionIdx++)
	{
		const swMeshSection* Section = &mLods[SW_MESH_LOD_HIGH].Sections[SectionIdx];

		if (Section->PrimitiveType == SW_MESH_TRIANGLES)
		{
			const swVertex* Verts = GetVertexData();
			const IndexType* Indices = (IndexType*)mIndexData + Section->DrawOffset / sizeof(IndexType);

			for (int Idx = 0; Idx < Section->DrawCount; Idx += 3)
				if (swTriangleIntersectsPlanes(Verts[Indices[Idx]].Position, Verts[Indices[Idx+1]].Position, Verts[Indices[Idx+2]].Position, Planes))
					return true;
		}
		else if (Section->PrimitiveType == SW_MESH_TEXTURED_TRIANGLES)
		{
			const swVertexTextured* Verts = GetTexturedVertexData();
			const IndexType* Indices = (IndexType*)mIndexData + Section->DrawOffset / sizeof(IndexType);

			for (int Idx = 0; Idx < Section->DrawCount; Idx += 3)
				if (swTriangleIntersectsPlanes(Verts[Indices[Idx]].Position, Verts[Indices[Idx+1]].Position, Verts[Indices[Idx+2]].Position, Planes))
					return true;
		}
	}

	return false;
}

bool swMesh::IntersectsPlanes(const swVector4 (&Planes)[6])
{
	if (mIndexType == GL_UNSIGNED_SHORT)
		return IntersectsPlanes<GLushort>(Planes);
	else
		return IntersectsPlanes<GLuint>(Planes);
}

template<typename IndexType>
void swMesh::ExportPOVRay(swFile& File, const char* MeshName, const char** ColorTable)
{
	char Line[1024];

	int NumSections = 0;

	for (int SectionIdx = 0; SectionIdx < mLods[SW_MESH_LOD_HIGH].NumSections; SectionIdx++)
	{
		const swMeshSection* const Section = &mLods[SW_MESH_LOD_HIGH].Sections[SectionIdx];

		if (Section->PrimitiveType == SW_MESH_TRIANGLES || Section->PrimitiveType == SW_MESH_TEXTURED_TRIANGLES)
			NumSections++;
	}

	if (NumSections > 1)
		snprintf(Line, sizeof(Line), "#declare sw_%s = union {\n", MeshName);
	else
		snprintf(Line, sizeof(Line), "#declare sw_%s = mesh {\n", MeshName);
	File.WriteLine(Line);

	for (int SectionIdx = 0; SectionIdx < mLods[SW_MESH_LOD_HIGH].NumSections; SectionIdx++)
	{
		swMeshSection* Section = &mLods[SW_MESH_LOD_HIGH].Sections[SectionIdx];

		if (Section->PrimitiveType == SW_MESH_TRIANGLES)
		{
			const swVertex* Verts = GetVertexData();
			const IndexType* Indices = (IndexType*)mIndexData + Section->DrawOffset / sizeof(IndexType);

			if (NumSections > 1)
				File.WriteLine(" mesh {\n");

			for (int Idx = 0; Idx < Section->DrawCount; Idx += 3)
			{
				const swVector3 v1 = Verts[Indices[Idx]].Position / 25.0f;
				const swVector3 v2 = Verts[Indices[Idx + 1]].Position / 25.0f;
				const swVector3 v3 = Verts[Indices[Idx + 2]].Position / 25.0f;
				const swVector3 n1 = swUnpackNormal(Verts[Indices[Idx]].Normal);
				const swVector3 n2 = swUnpackNormal(Verts[Indices[Idx + 1]].Normal);
				const swVector3 n3 = swUnpackNormal(Verts[Indices[Idx + 2]].Normal);

				snprintf(Line, sizeof(Line), "  smooth_triangle { <%g, %g, %g>, <%g, %g, %g>, <%g, %g, %g>, <%g, %g, %g>, <%g, %g, %g>, <%g, %g, %g> }\n",
				         -v1.y, -v1.x, v1.z, -n1.y, -n1.x, n1.z, -v2.y, -v2.x, v2.z, -n2.y, -n2.x, n2.z, -v3.y, -v3.x, v3.z, -n3.y, -n3.x, n3.z);
				File.WriteLine(Line);
			}
		}
		else if (Section->PrimitiveType == SW_MESH_TEXTURED_TRIANGLES)
		{
			const swVertexTextured* Verts = GetTexturedVertexData();
			const IndexType* Indices = (IndexType*)mIndexData + Section->DrawOffset / sizeof(IndexType);

			if (NumSections > 1)
				File.WriteLine(" mesh {\n");

			for (int Idx = 0; Idx < Section->DrawCount; Idx += 3)
			{
				const swVector3 v1 = Verts[Indices[Idx]].Position / 25.0f;
				const swVector3 v2 = Verts[Indices[Idx + 1]].Position / 25.0f;
				const swVector3 v3 = Verts[Indices[Idx + 2]].Position / 25.0f;
				const swVector3 n1 = swUnpackNormal(Verts[Indices[Idx]].Normal);
				const swVector3 n2 = swUnpackNormal(Verts[Indices[Idx + 1]].Normal);
				const swVector3 n3 = swUnpackNormal(Verts[Indices[Idx + 2]].Normal);

				snprintf(Line, sizeof(Line), "  smooth_triangle { <%g, %g, %g>, <%g, %g, %g>, <%g, %g, %g>, <%g, %g, %g>, <%g, %g, %g>, <%g, %g, %g> }\n",
				         -v1.y, -v1.x, v1.z, -n1.y, -n1.x, n1.z, -v2.y, -v2.x, v2.z, -n2.y, -n2.x, n2.z, -v3.y, -v3.x, v3.z, -n3.y, -n3.x, n3.z);
				File.WriteLine(Line);
			}
		}
		else
			continue;

		if (Section->ColorIndex != gDefaultColor)
		{
			snprintf(Line, sizeof(Line), "  material { texture { %s normal { bumps 0.1 scale 2 } } }", ColorTable[Section->ColorIndex]);
			File.WriteLine(Line);
		}

		if (NumSections > 1)
			File.WriteLine(" }\n");
	}

	File.WriteLine("}\n\n");
}

void swMesh::ExportPOVRay(swFile& File, const char* MeshName, const char** ColorTable)
{
	if (mIndexType == GL_UNSIGNED_SHORT)
		ExportPOVRay<GLushort>(File, MeshName, ColorTable);
	else
		ExportPOVRay<GLuint>(File, MeshName, ColorTable);
}

template<typename IndexType>
void swMesh::ExportWavefrontIndices(swFile& File, int DefaultColorIndex, int VertexOffset)
{
	char Line[1024];

	for (int SectionIdx = 0; SectionIdx < mLods[SW_MESH_LOD_HIGH].NumSections; SectionIdx++)
	{
		swMeshSection* Section = &mLods[SW_MESH_LOD_HIGH].Sections[SectionIdx];

		if (Section->PrimitiveType != SW_MESH_TRIANGLES && Section->PrimitiveType != SW_MESH_TEXTURED_TRIANGLES)
			continue;

		IndexType* Indices = (IndexType*)mIndexData + Section->DrawOffset / sizeof(IndexType);

		if (Section->ColorIndex == gDefaultColor)
			snprintf(Line, sizeof(Line), "usemtl %s\n", gColorList[DefaultColorIndex].SafeName);
		else
			snprintf(Line, sizeof(Line), "usemtl %s\n", gColorList[Section->ColorIndex].SafeName);

		File.WriteLine(Line);

		if (Section->PrimitiveType == SW_MESH_TRIANGLES)
		{
			for (int Idx = 0; Idx < Section->DrawCount; Idx += 3)
			{
				const long int idx1 = Indices[Idx + 0] + VertexOffset;
				const long int idx2 = Indices[Idx + 1] + VertexOffset;
				const long int idx3 = Indices[Idx + 2] + VertexOffset;

				if (idx1 != idx2 && idx1 != idx3 && idx2 != idx3)
				{
					snprintf(Line, sizeof(Line), "f %ld//%ld %ld//%ld %ld//%ld\n", idx1, idx1, idx2, idx2, idx3, idx3);
					File.WriteLine(Line);
				}
			}
		}
		else
		{
			for (int Idx = 0; Idx < Section->DrawCount; Idx += 3)
			{
				const long int idx1 = Indices[Idx + 0] + VertexOffset;
				const long int idx2 = Indices[Idx + 1] + VertexOffset;
				const long int idx3 = Indices[Idx + 2] + VertexOffset;

				if (idx1 != idx2 && idx1 != idx3 && idx2 != idx3)
				{
					snprintf(Line, sizeof(Line), "f %ld//%ld %ld//%ld %ld//%ld\n", idx1 + mNumVertices, idx1 + mNumVertices, idx2 + mNumVertices, idx2 + mNumVertices, idx3 + mNumVertices, idx3 + mNumVertices);
					File.WriteLine(Line);
				}
			}
		}
	}

	File.WriteLine("\n");
}

void swMesh::ExportWavefrontIndices(swFile& File, int DefaultColorIndex, int VertexOffset)
{
	if (mIndexType == GL_UNSIGNED_SHORT)
		ExportWavefrontIndices<GLushort>(File, DefaultColorIndex, VertexOffset);
	else
		ExportWavefrontIndices<GLuint>(File, DefaultColorIndex, VertexOffset);
}

bool swMesh::FileLoad(swMemoryFile& File)
{
	quint32 FileId, FileVersion, Flags;

	if (File.ReadU32(&FileId, 1) != 1 || FileId != SW_MESH_FILE_ID || File.ReadU32(&FileVersion, 1) != 1 ||
	    FileVersion != SW_MESH_FILE_VERSION || File.ReadU32(&Flags, 1) != 1)
		return false;

	mFlags = static_cast<swMeshFlags>(Flags);

	if (File.ReadFloats(mBoundingBox.Min.GetFloats(), 3) != 3 ||
		File.ReadFloats(mBoundingBox.Max.GetFloats(), 3) != 3 || File.ReadFloats(&mRadius, 1) != 1)
		return false;

	quint32 VertexCount, TexturedVertexCount, ConditionalVertexCount, IndexCount;
	quint16 NumLods, NumSections[SW_MESH_LOD_COUNT];

	if (File.ReadU32(&VertexCount, 1) != 1 || File.ReadU32(&TexturedVertexCount, 1) != 1 ||
		File.ReadU32(&ConditionalVertexCount, 1) != 1 || File.ReadU32(&IndexCount, 1) != 1)
		return false;

	if (File.ReadU16(&NumLods, 1) != 1 || NumLods != SW_MESH_LOD_COUNT || File.ReadU16(NumSections, SW_MESH_LOD_COUNT) != SW_MESH_LOD_COUNT)
		return false;

	const quint64 VertexDataSize = static_cast<quint64>(VertexCount) * sizeof(swVertex) + static_cast<quint64>(TexturedVertexCount) * sizeof(swVertexTextured) + static_cast<quint64>(ConditionalVertexCount) * sizeof(swVertexConditional);
	const quint64 IndexElementSize = VertexCount < 0x10000 && TexturedVertexCount < 0x10000 ? sizeof(GLushort) : sizeof(GLuint);
	const quint64 IndexDataSize = static_cast<quint64>(IndexCount) * IndexElementSize;
	constexpr quint64 MinimumSectionSize = 3 * sizeof(quint32) + sizeof(quint16) + 7 * sizeof(float) + sizeof(quint16);
	const quint64 MinimumSectionsSize = static_cast<quint64>(NumSections[SW_MESH_LOD_HIGH] + NumSections[SW_MESH_LOD_LOW]) * MinimumSectionSize;
	const long Position = File.GetPosition();

	if (VertexDataSize > std::numeric_limits<uint>::max() || IndexDataSize > std::numeric_limits<uint>::max() || Position < 0 || static_cast<size_t>(Position) > File.GetLength() ||
		MinimumSectionsSize + VertexDataSize + IndexDataSize > File.GetLength() - static_cast<size_t>(Position))
		return false;

	Create(NumSections, VertexCount, TexturedVertexCount, ConditionalVertexCount, IndexCount);

	if ((mVertexDataSize && !mVertexData) || (mIndexDataSize && !mIndexData))
		return false;

	for (int LodIdx = 0; LodIdx < SW_MESH_LOD_COUNT; LodIdx++)
	{
		for (int SectionIdx = 0; SectionIdx < mLods[LodIdx].NumSections; SectionIdx++)
		{
			swMeshSection& Section = mLods[LodIdx].Sections[SectionIdx];

			quint32 ColorCode, DrawOffset, DrawCount;
			quint16 PrimtiveType, Length;

			if (File.ReadU32(&ColorCode, 1) != 1 || File.ReadU32(&DrawOffset, 1) != 1 ||
				File.ReadU32(&DrawCount, 1) != 1 || File.ReadU16(&PrimtiveType, 1) != 1)
				return false;

			Section.ColorIndex = static_cast<int>(ColorCode);
			Section.DrawOffset = DrawOffset;
			Section.DrawCount = DrawCount;
			Section.PrimitiveType = (swMeshPrimitiveType)PrimtiveType;

			if (File.ReadFloats(Section.BoundingBox.Min.GetFloats(), 3) != 3 ||
				File.ReadFloats(Section.BoundingBox.Max.GetFloats(), 3) != 3 ||
				File.ReadFloats(&Section.Radius, 1) != 1)
				return false;

			if (File.ReadU16(&Length, 1) != 1)
				return false;

			if (Length)
			{
				if (Length >= SW_TEXTURE_NAME_LENGTH)
					return false;

				char FileName[SW_TEXTURE_NAME_LENGTH];

				if (File.ReadBuffer(FileName, Length) != Length)
					return false;

				FileName[Length] = 0;

				Section.TextureName = QString::fromLatin1(FileName);
			}
		}
	}

	if (File.ReadBuffer(mVertexData, mVertexDataSize) != static_cast<size_t>(mVertexDataSize))
		return false;

	if (mIndexType == GL_UNSIGNED_SHORT)
	{
		if (File.ReadU16((quint16*)mIndexData, mIndexDataSize / 2) != static_cast<size_t>(mIndexDataSize / 2))
			return false;
	}
	else
	{
		if (File.ReadU32((quint32*)mIndexData, mIndexDataSize / 4) != static_cast<size_t>(mIndexDataSize / 4))
			return false;
	}

	return true;
}

void swMesh::FinalizeSections()
{
	mFlags &= ~(swMeshFlag::HasDefault | swMeshFlag::HasSolid | swMeshFlag::HasTranslucent);

	for (swMeshLod& Lod : mLods)
	{
		for (int SectionIdx = 0; SectionIdx < Lod.NumSections; SectionIdx++)
		{
			swMeshSection& Section = Lod.Sections[SectionIdx];
			Section.ColorIndex = swGetColorIndex(static_cast<quint32>(Section.ColorIndex));

			if (Section.PrimitiveType != SW_MESH_TRIANGLES && Section.PrimitiveType != SW_MESH_TEXTURED_TRIANGLES)
				continue;

			if (Section.ColorIndex == gDefaultColor)
				mFlags |= swMeshFlag::HasDefault;
			else if (swIsColorTranslucent(Section.ColorIndex))
				mFlags |= swMeshFlag::HasTranslucent;
			else
				mFlags |= swMeshFlag::HasSolid;
		}

		if (Lod.NumSections > 1)
			std::sort(Lod.Sections, Lod.Sections + Lod.NumSections, [](const swMeshSection& Left, const swMeshSection& Right)
			{
				if (Left.PrimitiveType != Right.PrimitiveType)
				{
					const swMeshPrimitiveType PrimitiveOrder[] =
					{
						SW_MESH_TRIANGLES,
						SW_MESH_TEXTURED_TRIANGLES,
						SW_MESH_LINES,
						SW_MESH_CONDITIONAL_LINES
					};

					return std::find(std::begin(PrimitiveOrder), std::end(PrimitiveOrder), Left.PrimitiveType) <
						std::find(std::begin(PrimitiveOrder), std::end(PrimitiveOrder), Right.PrimitiveType);
				}

				const bool LeftTranslucent = swIsColorTranslucent(Left.ColorIndex);
				const bool RightTranslucent = swIsColorTranslucent(Right.ColorIndex);

				if (LeftTranslucent != RightTranslucent)
					return !LeftTranslucent;

				return Left.ColorIndex > Right.ColorIndex;
			});
	}
}

bool swMesh::FileSave(swMemoryFile& File)
{
	File.WriteU32(SW_MESH_FILE_ID);
	File.WriteU32(SW_MESH_FILE_VERSION);

	File.WriteU32(mFlags);
	File.WriteVector3(mBoundingBox.Min);
	File.WriteVector3(mBoundingBox.Max);
	File.WriteFloat(mRadius);

	File.WriteU32(mNumVertices);
	File.WriteU32(mNumTexturedVertices);
	File.WriteU32(mConditionalVertexCount);
	File.WriteU32(mIndexDataSize / (mIndexType == GL_UNSIGNED_SHORT ? 2 : 4));

	File.WriteU16(SW_MESH_LOD_COUNT);
	for (int LodIdx = 0; LodIdx < SW_MESH_LOD_COUNT; LodIdx++)
		File.WriteU16(mLods[LodIdx].NumSections);

	for (int LodIdx = 0; LodIdx < SW_MESH_LOD_COUNT; LodIdx++)
	{
		for (int SectionIdx = 0; SectionIdx < mLods[LodIdx].NumSections; SectionIdx++)
		{
			const swMeshSection& Section = mLods[LodIdx].Sections[SectionIdx];

			File.WriteU32(swGetColorCode(Section.ColorIndex));
			File.WriteU32(Section.DrawOffset);
			File.WriteU32(Section.DrawCount);
			File.WriteU16(Section.PrimitiveType);
			File.WriteVector3(Section.BoundingBox.Min);
			File.WriteVector3(Section.BoundingBox.Max);
			File.WriteFloat(Section.Radius);

			const QByteArray TextureName = Section.TextureName.isEmpty() && Section.Texture ? QByteArray(Section.Texture->mName) : Section.TextureName.toLatin1();

			if (!TextureName.isEmpty())
			{
				const quint16 Length = (quint16)TextureName.size();
				File.WriteU16(Length);
				File.WriteBuffer(TextureName.constData(), Length);
			}
			else
				File.WriteU16(0);
		}
	}

	File.WriteBuffer(mVertexData, mNumVertices * sizeof(swVertex) + mNumTexturedVertices * sizeof(swVertexTextured) + mConditionalVertexCount * sizeof(swVertexConditional));

	if (mIndexType == GL_UNSIGNED_SHORT)
		File.WriteU16((quint16*)mIndexData, mIndexDataSize / 2);
	else
		File.WriteU32((quint32*)mIndexData, mIndexDataSize / 4);

	return true;
}

int swMesh::GetLodIndex(float Distance) const
{
	if (swGetPartsLibrary()->GetStudStyle() != swStudStyle::Plain) // todo: support low lod studs
		return SW_MESH_LOD_HIGH;

	if (mLods[SW_MESH_LOD_LOW].NumSections && (Distance > mRadius))
		return SW_MESH_LOD_LOW;
	else
		return SW_MESH_LOD_HIGH;
}
