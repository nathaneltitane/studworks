#include "sw_global.h"
#include "sw_scene.h"
#include "sw_context.h"
#include "sw_data_piece.h"
#include "sw_texture.h"
#include "sw_library.h"
#include "sw_application.h"
#include "sw_object.h"
#include "sw_colors.h"

swScene::swScene()
{
	mHighlightRenderMode = swHighlightRenderMode::Tint;
	mShadingMode = swShadingMode::DefaultLights;
	mLightingStyle = swLightingStyle::Standard;
	mMeshLODDistance = 250.0f;
}

void swScene::Begin(const swMatrix44& ViewMatrix)
{
	mMissingAssets = false;
	mHighlightRenderMode = swHighlightRenderMode::Tint;
	mViewMatrix = ViewMatrix;
	mActiveSubmodelInstance = nullptr;
	mModelEditingPath.clear();
	mRenderMeshes.clear();
	mOpaqueMeshes.clear();
	mTranslucentMeshes.clear();
	mInterfaceObjects.clear();

	const swPreferences& Preferences = swGetPreferences();
	mHighlightColor = swVector4FromColor(Preferences.mHighlightNewPiecesColor);
	mFadeColor = swVector4FromColor(Preferences.mFadeStepsColor);
	mHasFadedPieces = false;
	mTranslucentFade = mFadeColor.w != 1.0f;
}

void swScene::End()
{
	const auto OpaqueMeshCompare = [this](int Index1, int Index2)
	{
		const swMesh* Mesh1 = mRenderMeshes[Index1].Mesh;
		const swMesh* Mesh2 = mRenderMeshes[Index2].Mesh;

		const int Texture1 = Mesh1->mFlags & swMeshFlag::HasTexture;
		const int Texture2 = Mesh2->mFlags & swMeshFlag::HasTexture;

		if (Texture1 == Texture2)
			return Mesh1 < Mesh2;

		return Texture1 ? false : true;
	};

	std::sort(mOpaqueMeshes.begin(), mOpaqueMeshes.end(), OpaqueMeshCompare);

	auto TranslucentMeshCompare = [](const swTranslucentMeshInstance& Mesh1, const swTranslucentMeshInstance& Mesh2)
	{
		return Mesh1.Distance > Mesh2.Distance;
	};

	std::sort(mTranslucentMeshes.begin(), mTranslucentMeshes.end(), TranslucentMeshCompare);
}

void swScene::AddMesh(swMesh* Mesh, const swMatrix44& WorldMatrix, int ColorIndex, swRenderMeshState State)
{
	swRenderMesh& RenderMesh = mRenderMeshes.emplace_back();

	RenderMesh.WorldMatrix = WorldMatrix;
	RenderMesh.Mesh = Mesh;
	RenderMesh.ColorIndex = ColorIndex;
	RenderMesh.State = State;
	const float Distance = fabsf(swMultiply31(WorldMatrix[3], mViewMatrix).z) - mMeshLODDistance;
	RenderMesh.LodIndex = mAllowLOD ? RenderMesh.Mesh->GetLodIndex(Distance) : SW_MESH_LOD_HIGH;

	const bool ForceTranslucent = (mTranslucentFade && State == swRenderMeshState::Faded);
	const bool Translucent = swIsColorTranslucent(ColorIndex) || ForceTranslucent;
	const swMeshFlags Flags = Mesh->mFlags;
	mHasFadedPieces |= State == swRenderMeshState::Faded;

	if ((Flags & (swMeshFlag::HasSolid | swMeshFlag::HasLines)) || ((Flags & swMeshFlag::HasDefault) && !Translucent))
		mOpaqueMeshes.emplace_back(static_cast<int>(mRenderMeshes.size()) - 1);

	if ((Flags & swMeshFlag::HasTranslucent) || ((Flags & swMeshFlag::HasDefault) && Translucent))
	{
		const swMeshLod& Lod = Mesh->mLods[RenderMesh.LodIndex];

		for (int SectionIdx = 0; SectionIdx < Lod.NumSections; SectionIdx++)
		{
			const swMeshSection* Section = &Lod.Sections[SectionIdx];

			if ((Section->PrimitiveType & (SW_MESH_TRIANGLES | SW_MESH_TEXTURED_TRIANGLES)) == 0)
				continue;

			int SectionColorIndex = Section->ColorIndex;

			if (SectionColorIndex == gDefaultColor)
				SectionColorIndex = RenderMesh.ColorIndex;

			if (!swIsColorTranslucent(SectionColorIndex) && !ForceTranslucent)
				continue;

			const swVector3 Center = (Section->BoundingBox.Min + Section->BoundingBox.Max) / 2;
			const float InstanceDistance = fabsf(swMultiply31(swMultiply31(Center, WorldMatrix), mViewMatrix).z);

			swTranslucentMeshInstance& Instance = mTranslucentMeshes.emplace_back();
			Instance.Section = Section;
			Instance.Distance = InstanceDistance;
			Instance.RenderMeshIndex = static_cast<int>(mRenderMeshes.size()) - 1;
		}
	}
}

void swScene::DrawDebugNormals(swContext* Context, const swMesh* Mesh) const
{
	const swVertex* const VertexBuffer = Mesh->GetVertexData();
	const swVertexTextured* const TexturedVertexBuffer = Mesh->GetTexturedVertexData();
	swVector3* const Vertices = (swVector3*)malloc((Mesh->mNumVertices + Mesh->mNumTexturedVertices) * 2 * sizeof(swVector3));

	for (int VertexIdx = 0; VertexIdx < Mesh->mNumVertices; VertexIdx++)
	{
		Vertices[VertexIdx * 2] = VertexBuffer[VertexIdx].Position;
		Vertices[VertexIdx * 2 + 1] = VertexBuffer[VertexIdx].Position + swUnpackNormal(VertexBuffer[VertexIdx].Normal);
	}

	for (int VertexIdx = 0; VertexIdx < Mesh->mNumTexturedVertices; VertexIdx++)
	{
		Vertices[(Mesh->mNumVertices + VertexIdx) * 2] = TexturedVertexBuffer[VertexIdx].Position;
		Vertices[(Mesh->mNumVertices + VertexIdx) * 2 + 1] = TexturedVertexBuffer[VertexIdx].Position + swUnpackNormal(TexturedVertexBuffer[VertexIdx].Normal);
	}

	Context->SetVertexBufferPointer(Vertices);
	Context->SetVertexFormatPosition(3);
	Context->DrawPrimitives(GL_LINES, 0, (Mesh->mNumVertices + Mesh->mNumTexturedVertices) * 2);
	free(Vertices);
}

void swScene::DrawOpaqueMeshes(swContext* Context, bool DrawLit, int PrimitiveTypes, bool DrawFaded, bool DrawNonFaded) const
{
	if (mOpaqueMeshes.empty())
		return;

	swMaterialType FlatMaterial, TexturedMaterial;

	if (DrawLit)
	{
		FlatMaterial = swMaterialType::FakeLitColor;
		TexturedMaterial = swMaterialType::FakeLitTextureDecal;
	}
	else
	{
		FlatMaterial = swMaterialType::UnlitColor;
		TexturedMaterial = swMaterialType::UnlitTextureDecal;
	}

	Context->SetPolygonOffset(swPolygonOffset::Opaque);

	const swPreferences& Preferences = swGetPreferences();
	const swVector4 FocusedColor = swVector4FromColor(Preferences.mObjectFocusedColor);
	const swVector4 SelectedColor = swVector4FromColor(Preferences.mObjectSelectedColor);

	for (const int MeshIndex : mOpaqueMeshes)
	{
		const swRenderMesh& RenderMesh = mRenderMeshes[MeshIndex];
		const swMesh* Mesh = RenderMesh.Mesh;
		const int LodIndex = RenderMesh.LodIndex;
		swRenderMeshState State = RenderMesh.State;
		const bool HasTriangles = (Mesh->mFlags & (swMeshFlag::HasDefault | swMeshFlag::HasSolid | swMeshFlag::HasTranslucent)) != 0;

		if (mHighlightRenderMode == swHighlightRenderMode::Outline && HasTriangles &&
			(State == swRenderMeshState::Selected || State == swRenderMeshState::Focused || State == swRenderMeshState::Highlighted))
			State = swRenderMeshState::Default;

		if (!DrawFaded && RenderMesh.State == swRenderMeshState::Faded)
			continue;

		if (!DrawNonFaded && RenderMesh.State != swRenderMeshState::Faded)
			continue;

		Context->BindMesh(Mesh);
		Context->SetWorldMatrix(RenderMesh.WorldMatrix);

		for (int SectionIdx = 0; SectionIdx < Mesh->mLods[LodIndex].NumSections; SectionIdx++)
		{
			const swMeshSection* const Section = &Mesh->mLods[LodIndex].Sections[SectionIdx];

			if ((Section->PrimitiveType & PrimitiveTypes) == 0)
				continue;

			int ColorIndex = Section->ColorIndex;

			if (Section->PrimitiveType & (SW_MESH_TRIANGLES | SW_MESH_TEXTURED_TRIANGLES))
			{
				if (ColorIndex == gDefaultColor)
					ColorIndex = RenderMesh.ColorIndex;

				if (swIsColorTranslucent(ColorIndex))
					continue;

				switch (State)
				{
				case swRenderMeshState::Default:
				case swRenderMeshState::Highlighted:
					Context->SetColorIndex(ColorIndex);
					break;

				case swRenderMeshState::Selected:
					Context->SetColorIndexTinted(ColorIndex, SelectedColor, 0.5f);
					break;

				case swRenderMeshState::Focused:
				case swRenderMeshState::Preview:
					Context->SetColorIndexTinted(ColorIndex, FocusedColor, 0.5f);
					break;

				case swRenderMeshState::Faded:
					if (mTranslucentFade)
						continue;
					Context->SetColorIndexTinted(ColorIndex, mFadeColor);
					break;
				}
			}
			else if (Section->PrimitiveType & (SW_MESH_LINES | SW_MESH_CONDITIONAL_LINES))
			{
				switch (State)
				{
				case swRenderMeshState::Default:
					if (mShadingMode != swShadingMode::Wireframe)
					{
						if (ColorIndex != gEdgeColor)
							Context->SetColorIndex(ColorIndex);
						else
							Context->SetEdgeColorIndex(RenderMesh.ColorIndex);
					}
					else
					{
						if (ColorIndex == gEdgeColor)
							ColorIndex = RenderMesh.ColorIndex;

						Context->SetColorIndex(ColorIndex);
					}
					break;

				case swRenderMeshState::Selected:
					Context->SetColor(SelectedColor);
					break;

				case swRenderMeshState::Focused:
				case swRenderMeshState::Preview:
					Context->SetColor(FocusedColor);
					break;

				case swRenderMeshState::Highlighted:
					Context->SetColor(mHighlightColor);
					break;

				case swRenderMeshState::Faded:
					Context->SetEdgeColorIndexTinted(ColorIndex, mFadeColor);
					break;
				}

				if (Section->PrimitiveType == SW_MESH_CONDITIONAL_LINES)
				{
					int VertexBufferOffset = Mesh->mVertexCacheOffset != -1 ? Mesh->mVertexCacheOffset : 0;
					VertexBufferOffset += Mesh->mNumVertices * sizeof(swVertex) + Mesh->mNumTexturedVertices * sizeof(swVertexTextured);
					Context->SetMaterial(swMaterialType::UnlitColorConditional);
					Context->SetVertexFormatConditional(VertexBufferOffset);

					Context->DrawPrimitives(GL_LINES, Section->DrawOffset, Section->DrawCount);

					continue;
				}
			}

			int VertexBufferOffset = Mesh->mVertexCacheOffset != -1 ? Mesh->mVertexCacheOffset : 0;
			const int IndexBufferOffset = Mesh->mIndexCacheOffset != -1 ? Mesh->mIndexCacheOffset : 0;

			if (Section->PrimitiveType != SW_MESH_TEXTURED_TRIANGLES)
			{
				Context->SetMaterial(FlatMaterial);
				Context->SetVertexFormat(VertexBufferOffset, 3, 1, 0, 0, DrawLit);
			}
			else
			{
				swTexture* const Texture = Section->Texture.Get();

				if (Texture)
				{
					if (Texture->NeedsUpload())
						Texture->Upload(Context);

					Context->SetMaterial(TexturedMaterial);
					Context->BindTexture2D(Texture);
				}
				else
				{
					Context->SetMaterial(FlatMaterial);
				}

				VertexBufferOffset += Mesh->mNumVertices * sizeof(swVertex);
				Context->SetVertexFormat(VertexBufferOffset, 3, 1, 2, 0, DrawLit);
			}

			const GLenum DrawPrimitiveType = Section->PrimitiveType & (SW_MESH_TRIANGLES | SW_MESH_TEXTURED_TRIANGLES) ? GL_TRIANGLES : GL_LINES;
			Context->DrawIndexedPrimitives(DrawPrimitiveType, Section->DrawCount, Mesh->mIndexType, IndexBufferOffset + Section->DrawOffset);
		}

#ifdef SW_DEBUG_NORMALS
		DrawDebugNormals(Context, Mesh);
#endif
	}

	Context->ClearTexture2D();
	Context->SetPolygonOffset(swPolygonOffset::None);
}

void swScene::DrawTranslucentMeshes(swContext* Context, bool DrawLit, bool DrawFadePrepass, bool DrawFaded, bool DrawNonFaded) const
{
	if (mTranslucentMeshes.empty())
		return;

	swMaterialType FlatMaterial, TexturedMaterial;

	if (DrawLit)
	{
		FlatMaterial = swMaterialType::FakeLitColor;
		TexturedMaterial = swMaterialType::FakeLitTextureDecal;
	}
	else
	{
		FlatMaterial = swMaterialType::UnlitColor;
		TexturedMaterial = swMaterialType::UnlitTextureDecal;
	}

	if (!DrawFadePrepass)
	{
		Context->EnableColorBlend(true);
		Context->SetDepthWrite(false);
	}
	else
		Context->EnableColorWrite(false);

	Context->SetPolygonOffset(swPolygonOffset::Translucent);

	const swPreferences& Preferences = swGetPreferences();
	const swVector4 FocusedColor = swVector4FromColor(Preferences.mObjectFocusedColor);
	const swVector4 SelectedColor = swVector4FromColor(Preferences.mObjectSelectedColor);

	for (const swTranslucentMeshInstance& MeshInstance : mTranslucentMeshes)
	{
		const swRenderMesh& RenderMesh = mRenderMeshes[MeshInstance.RenderMeshIndex];
		const swMesh* Mesh = RenderMesh.Mesh;
		swRenderMeshState State = RenderMesh.State;

		if (mHighlightRenderMode == swHighlightRenderMode::Outline && (State == swRenderMeshState::Selected || State == swRenderMeshState::Focused))
			State = swRenderMeshState::Default;

		if (!DrawFaded && RenderMesh.State == swRenderMeshState::Faded)
			continue;

		if (!DrawNonFaded && RenderMesh.State != swRenderMeshState::Faded)
			continue;

		Context->BindMesh(Mesh);
		Context->SetWorldMatrix(RenderMesh.WorldMatrix);

		const swMeshSection* Section = MeshInstance.Section;

		int ColorIndex = Section->ColorIndex;

		if (ColorIndex == gDefaultColor)
			ColorIndex = RenderMesh.ColorIndex;

		if (DrawFadePrepass && swIsColorTranslucent(ColorIndex))
			continue;

		switch (State)
		{
		case swRenderMeshState::Default:
		case swRenderMeshState::Highlighted:
			Context->SetColorIndex(ColorIndex);
			break;

		case swRenderMeshState::Selected:
			Context->SetColorIndexTinted(ColorIndex, SelectedColor, 0.5f);
			break;

		case swRenderMeshState::Focused:
		case swRenderMeshState::Preview:
			Context->SetColorIndexTinted(ColorIndex, FocusedColor, 0.5f);
			break;

		case swRenderMeshState::Faded:
			Context->SetColorIndexTinted(ColorIndex, mFadeColor);
			break;
		}

		swTexture* const Texture = Section->Texture.Get();
		int VertexBufferOffset = Mesh->mVertexCacheOffset != -1 ? Mesh->mVertexCacheOffset : 0;
		const int IndexBufferOffset = Mesh->mIndexCacheOffset != -1 ? Mesh->mIndexCacheOffset : 0;

		if (!Texture)
		{
			Context->SetMaterial(FlatMaterial);
			Context->SetVertexFormat(VertexBufferOffset, 3, 1, 0, 0, DrawLit);
		}
		else
		{
			if (Texture->NeedsUpload())
				Texture->Upload(Context);
			Context->SetMaterial(TexturedMaterial);
			VertexBufferOffset += Mesh->mNumVertices * sizeof(swVertex);
			Context->SetVertexFormat(VertexBufferOffset, 3, 1, 2, 0, DrawLit);
			Context->BindTexture2D(Texture);
		}

		const GLenum DrawPrimitiveType = Section->PrimitiveType & (SW_MESH_TRIANGLES | SW_MESH_TEXTURED_TRIANGLES) ? GL_TRIANGLES : GL_LINES;
		Context->DrawIndexedPrimitives(DrawPrimitiveType, Section->DrawCount, Mesh->mIndexType, IndexBufferOffset + Section->DrawOffset);

#ifdef SW_DEBUG_NORMALS
		DrawDebugNormals(Context, Mesh);
#endif
	}

	Context->ClearTexture2D();
	Context->SetPolygonOffset(swPolygonOffset::None);

	if (!DrawFadePrepass)
	{
		Context->SetDepthWrite(true);
		Context->EnableColorBlend(false);
	}
	else
		Context->EnableColorWrite(true);
}

void swScene::Draw(swContext* Context, const std::function<void()>& PreTranslucentCallback) const
{
	// TODO: find a better place for these updates
	swGetPiecesLibrary()->UpdateBuffers(Context);

	Context->SetViewMatrix(mViewMatrix);
	Context->SetLightingStyle(mLightingStyle);

	const bool DrawLines = mDrawEdgeLines && mLineWidth > 0.0f;
	const bool DrawConditional = mDrawConditionalLines && mLineWidth > 0.0f && swContext::SupportsMaterial(swMaterialType::UnlitColorConditional);

	swShadingMode ShadingMode = mShadingMode;

	if (ShadingMode != swShadingMode::Wireframe && ShadingMode != swShadingMode::Flat && !swContext::SupportsDefaultLights())
		ShadingMode = swShadingMode::Flat;

	if (ShadingMode == swShadingMode::Wireframe)
	{
		DrawOpaqueMeshes(Context, false, SW_MESH_LINES, true, true);

		if (DrawConditional)
			DrawOpaqueMeshes(Context, false, SW_MESH_CONDITIONAL_LINES, true, true);

		if (PreTranslucentCallback)
			PreTranslucentCallback();
	}
	else if (ShadingMode == swShadingMode::Flat)
	{
		const int SolidPrimitiveTypes = SW_MESH_TRIANGLES | SW_MESH_TEXTURED_TRIANGLES | (DrawLines ? SW_MESH_LINES : 0);

		if (mTranslucentFade && mHasFadedPieces)
		{
			DrawOpaqueMeshes(Context, false, SolidPrimitiveTypes, false, true);

			if (PreTranslucentCallback)
				PreTranslucentCallback();

			DrawTranslucentMeshes(Context, false, true, true, false);

			if (DrawLines)
				DrawOpaqueMeshes(Context, false, SW_MESH_LINES, true, false);

			if (DrawConditional)
				DrawOpaqueMeshes(Context, false, SW_MESH_CONDITIONAL_LINES, true, false);

			DrawTranslucentMeshes(Context, false, false, true, true);
		}
		else
		{
			DrawOpaqueMeshes(Context, false, SolidPrimitiveTypes, true, true);

			if (DrawConditional)
				DrawOpaqueMeshes(Context, false, SW_MESH_CONDITIONAL_LINES, true, true);

			if (PreTranslucentCallback)
				PreTranslucentCallback();

			DrawTranslucentMeshes(Context, false, false, true, true);
		}
	}
	else
	{
		if (mTranslucentFade && mHasFadedPieces)
		{
			DrawOpaqueMeshes(Context, true, SW_MESH_TRIANGLES | SW_MESH_TEXTURED_TRIANGLES, false, true);

			if (DrawLines)
				DrawOpaqueMeshes(Context, false, SW_MESH_LINES, false, true);

			if (DrawConditional)
				DrawOpaqueMeshes(Context, false, SW_MESH_CONDITIONAL_LINES, false, true);

			if (PreTranslucentCallback)
				PreTranslucentCallback();

			DrawTranslucentMeshes(Context, false, true, true, false);

			if (DrawLines)
				DrawOpaqueMeshes(Context, false, SW_MESH_LINES, true, false);

			if (DrawConditional)
				DrawOpaqueMeshes(Context, false, SW_MESH_CONDITIONAL_LINES, true, false);

			DrawTranslucentMeshes(Context, true, false, true, true);
		}
		else
		{
			if (DrawLines)
				DrawOpaqueMeshes(Context, false, SW_MESH_LINES, true, true);

			if (DrawConditional)
				DrawOpaqueMeshes(Context, false, SW_MESH_CONDITIONAL_LINES, true, true);

			DrawOpaqueMeshes(Context, true, SW_MESH_TRIANGLES | SW_MESH_TEXTURED_TRIANGLES, true, true);

			if (PreTranslucentCallback)
				PreTranslucentCallback();

			DrawTranslucentMeshes(Context, true, false, true, true);
		}
	}
}

bool swScene::HasSelectedMeshes() const
{
	return std::any_of(mRenderMeshes.begin(), mRenderMeshes.end(), [](const swRenderMesh& Mesh)
	{
		return Mesh.State == swRenderMeshState::Selected || Mesh.State == swRenderMeshState::Focused;
	});
}

bool swScene::CanReuseSceneDepth() const
{
	// Faded depth prepasses and insertion previews should not obscure selection.
	if (mTranslucentFade && mHasFadedPieces)
		return false;

	return std::none_of(mRenderMeshes.begin(), mRenderMeshes.end(), [](const swRenderMesh& Mesh)
	{
		return Mesh.State == swRenderMeshState::Preview;
	});
}

bool swScene::HasHighlightedMeshes() const
{
	return std::any_of(mRenderMeshes.begin(), mRenderMeshes.end(), [](const swRenderMesh& Mesh)
	{
		return Mesh.State == swRenderMeshState::Highlighted;
	});
}

void swScene::DrawSelectionMask(swContext* Context) const
{
	DrawOutlineGeometry(Context, swOutlineGeometry::Selection);
}

void swScene::DrawHighlightMask(swContext* Context) const
{
	DrawOutlineGeometry(Context, swOutlineGeometry::Highlight);
}

void swScene::DrawOutlineOccluders(swContext* Context) const
{
	DrawOutlineGeometry(Context, swOutlineGeometry::Occluders);
}

void swScene::DrawOutlineGeometry(swContext* Context, swOutlineGeometry Geometry) const
{
	Context->SetViewMatrix(mViewMatrix);
	Context->SetMaterial(swMaterialType::UnlitColor);
	Context->ClearTexture2D();

	for (const swRenderMesh& RenderMesh : mRenderMeshes)
	{
		const bool Selected = RenderMesh.State == swRenderMeshState::Selected || RenderMesh.State == swRenderMeshState::Focused;
		const bool Highlighted = RenderMesh.State == swRenderMeshState::Highlighted;

		if (RenderMesh.State == swRenderMeshState::Preview ||
			(Geometry == swOutlineGeometry::Selection && !Selected) ||
			(Geometry == swOutlineGeometry::Highlight && !Highlighted))
			continue;

		if (Geometry == swOutlineGeometry::Occluders && mTranslucentFade && RenderMesh.State == swRenderMeshState::Faded)
			continue;

		const swMesh* Mesh = RenderMesh.Mesh;
		Context->BindMesh(Mesh);
		Context->SetWorldMatrix(RenderMesh.WorldMatrix);
		Context->SetColor(swVector4(Selected ? 1.0f : 0.0f, RenderMesh.State == swRenderMeshState::Focused ? 1.0f : 0.0f, Highlighted ? 1.0f : 0.0f, 1.0f));

		const swMeshLod& Lod = Mesh->mLods[RenderMesh.LodIndex];
		for (int SectionIndex = 0; SectionIndex < Lod.NumSections; SectionIndex++)
		{
			const swMeshSection& Section = Lod.Sections[SectionIndex];
			if (!(Section.PrimitiveType & (SW_MESH_TRIANGLES | SW_MESH_TEXTURED_TRIANGLES)))
				continue;

			const int ColorIndex = Section.ColorIndex == gDefaultColor ? RenderMesh.ColorIndex : Section.ColorIndex;
			if (Geometry == swOutlineGeometry::Occluders && !Selected && swIsColorTranslucent(ColorIndex))
				continue;

			int VertexOffset = Mesh->mVertexCacheOffset != -1 ? Mesh->mVertexCacheOffset : 0;
			if (Section.PrimitiveType == SW_MESH_TEXTURED_TRIANGLES)
			{
				VertexOffset += Mesh->mNumVertices * sizeof(swVertex);
				Context->SetVertexFormat(VertexOffset, 3, 1, 2, 0, false);
			}
			else
				Context->SetVertexFormat(VertexOffset, 3, 1, 0, 0, false);

			const int IndexOffset = Mesh->mIndexCacheOffset != -1 ? Mesh->mIndexCacheOffset : 0;
			Context->DrawIndexedPrimitives(GL_TRIANGLES, Section.DrawCount, Mesh->mIndexType, IndexOffset + Section.DrawOffset);
		}
	}
}

void swScene::DrawInterfaceObjects(swContext* Context) const
{
	for (const swObject* Object : mInterfaceObjects)
		Object->DrawInterface(Context, *this);
}
