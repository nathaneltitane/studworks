#include "sw_global.h"
#include "sw_view_cube.h"
#include "sw_view.h"
#include "sw_object_camera.h"
#include "sw_context.h"
#include "sw_cache_string.h"
#include "sw_application.h"
#include "sw_texture.h"

swTexture* swViewCube::mTexture;
swVertexBuffer swViewCube::mVertexBuffer;
swIndexBuffer swViewCube::mIndexBuffer;
int swViewCube::mIndexCount;
swViewCube::swViewCubeFacet swViewCube::mFacets[swViewCube::mFacetCount];
const float swViewCube::mHalfSize = SW_VIEW_CUBE_HALF_SIZE;
const float swViewCube::mChamfer = SW_VIEW_CUBE_CHAMFER;

// use to get the outward direction of a facet from its view flags
static swVector3 swGetFacetNormal(const std::bitset<6>& Flags)
{
	swVector3 Normal(0.0f, 0.0f, 0.0f);

	for (int AxisIdx = 0; AxisIdx < 3; AxisIdx++)
	{
		if (Flags.test(2 * AxisIdx))
			Normal[AxisIdx] = 1.0f;
		else if (Flags.test(2 * AxisIdx + 1))
			Normal[AxisIdx] = -1.0f;
	}

	return Normal;
}

swViewCube::swViewCube(swView* View)
	: mView(View)
{
	UpdateSettings();
}

void swViewCube::UpdateSettings()
{
	const swPreferences& Preferences = swGetPreferences();

	switch (mView->GetViewType())
	{
	case swViewType::View:
		mSize = Preferences.mViewCubeSize;
		mEnabled = Preferences.mViewCubeEnabled;
		mLocation = Preferences.mViewCubeLocation;
		break;

	case swViewType::Preview:
		mSize = Preferences.mPreviewViewCubeSize;
		mEnabled = Preferences.mPreviewViewCubeEnabled;
		mLocation = Preferences.mPreviewViewCubeLocation;
		break;

	case swViewType::MinifigureBuilder:
	case swViewType::PartsList:
	case swViewType::Turntable:
	case swViewType::Count:
		break;
	}

	mEnabled = mEnabled && swContext::SupportsMaterial(swMaterialType::UnlitViewCube);
}

swMatrix44 swViewCube::GetViewMatrix() const
{
	swMatrix44 ViewMatrix = mView->GetCamera()->mWorldView;
	ViewMatrix.SetTranslation(swVector3(0, 0, 0));
	return ViewMatrix;
}

swMatrix44 swViewCube::GetProjectionMatrix() const
{
	return swMatrix44Orthographic(-SW_VIEW_CUBE_BOUNDARY, SW_VIEW_CUBE_BOUNDARY, -SW_VIEW_CUBE_BOUNDARY, SW_VIEW_CUBE_BOUNDARY, -SW_VIEW_CUBE_BOUNDARY, SW_VIEW_CUBE_BOUNDARY);
}

void swViewCube::CreateResources(swContext* Context)
{
	constexpr int SourceCellSize = 512, CellHeight = 256, CellBottom = (SourceCellSize - CellHeight) / 2;
	constexpr int AtlasWidth = SourceCellSize, AtlasHeight = 2048;
	mTexture = new swTexture();

	const QString ViewNames[6] =
	{
		QCoreApplication::translate("ViewName", "Left"), QCoreApplication::translate("ViewName", "Right"), QCoreApplication::translate("ViewName", "Back"),
		QCoreApplication::translate("ViewName", "Front"), QCoreApplication::translate("ViewName", "Top"), QCoreApplication::translate("ViewName", "Bottom")
	};

	QImage PainterImage(SourceCellSize, SourceCellSize, QImage::Format_ARGB32);
	QPainter Painter;
	// labels use the interface font rendered large and mipmapped so they stay sharp at any cube size
	QFont Font = QGuiApplication::font();
	Font.setPointSize(144);
	QImage AtlasImage(AtlasWidth, AtlasHeight, QImage::Format_Alpha8);
	AtlasImage.fill(0);

	for (int ViewIdx = 0; ViewIdx < 6; ViewIdx++)
	{
		QFont LabelFont = Font;
		// the shader shows the middle 80% of each cell, keep labels clear of its border
		constexpr int MaxLabelWidth = 360;
		while (LabelFont.pointSize() > 1 && QFontMetrics(LabelFont).horizontalAdvance(ViewNames[ViewIdx]) > MaxLabelWidth)
			LabelFont.setPointSize(LabelFont.pointSize() - 1);

		Painter.begin(&PainterImage);
		Painter.setRenderHint(QPainter::TextAntialiasing);
		Painter.fillRect(0, 0, PainterImage.width(), PainterImage.height(), QColor(0, 0, 0));
		Painter.setPen(QColor(255, 255, 255));
		Painter.setFont(LabelFont);
		Painter.drawText(0, 0, PainterImage.width(), PainterImage.height(), Qt::AlignCenter, ViewNames[ViewIdx]);
		Painter.end();

		// keep the flipped middle band of each label as one atlas row
		const int AtlasY = ViewIdx * CellHeight;
		for (int y = 0; y < CellHeight; y++)
		{
			const QRgb* Source = reinterpret_cast<const QRgb*>(PainterImage.constScanLine(SourceCellSize - CellBottom - y - 1));
			unsigned char* Dest = AtlasImage.scanLine(AtlasY + y);

			for (int x = 0; x < SourceCellSize; x++)
				Dest[x] = static_cast<unsigned char>(qRed(Source[x]));
		}
	}

	mTexture->SetImage(std::move(AtlasImage), SW_TEXTURE_MIPMAPS | SW_TEXTURE_TRILINEAR);
	mTexture->Upload(Context);

	swMatrix44 Transforms[6] =
	{
		swMatrix44(swVector4(0.0f,  1.0f, 0.0f, 0.0f), swVector4(0.0f,  0.0f, 1.0f, 0.0f), swVector4(1.0f, 0.0f, 0.0f, 0.0f), swVector4(1.0f,  0.0f,  0.0f, 1.0f)),
		swMatrix44(swVector4(0.0f, -1.0f, 0.0f, 0.0f), swVector4(0.0f,  0.0f, 1.0f, 0.0f), swVector4(1.0f, 0.0f, 0.0f, 0.0f), swVector4(-1.0f,  0.0f,  0.0f, 1.0f)),
		swMatrix44(swVector4(-1.0f,  0.0f, 0.0f, 0.0f), swVector4(0.0f,  0.0f, 1.0f, 0.0f), swVector4(0.0f, 1.0f, 0.0f, 0.0f), swVector4(0.0f,  1.0f,  0.0f, 1.0f)),
		swMatrix44(swVector4(1.0f,  0.0f, 0.0f, 0.0f), swVector4(0.0f,  0.0f, 1.0f, 0.0f), swVector4(0.0f, 1.0f, 0.0f, 0.0f), swVector4(0.0f, -1.0f,  0.0f, 1.0f)),
		swMatrix44(swVector4(1.0f,  0.0f, 0.0f, 0.0f), swVector4(0.0f,  1.0f, 0.0f, 0.0f), swVector4(0.0f, 0.0f, 1.0f, 0.0f), swVector4(0.0f,  0.0f,  1.0f, 1.0f)),
		swMatrix44(swVector4(1.0f,  0.0f, 0.0f, 0.0f), swVector4(0.0f, -1.0f, 0.0f, 0.0f), swVector4(0.0f, 0.0f, 1.0f, 0.0f), swVector4(0.0f,  0.0f, -1.0f, 1.0f)),
	};

	const float Outer = mHalfSize;
	const float Inner = mHalfSize - mChamfer;
	std::vector<float> Verts;
	std::vector<GLushort> Indices;
	int FacetIdx = 0;

	// use to add a flat facet as a triangle fan wound to face outward
	auto AddFacet = [&](std::vector<swVector3> Positions, std::vector<swVector3> TexCoords, const std::bitset<6>& Flags)
	{
		const swVector3 Normal = swGetFacetNormal(Flags);

		if (swDot(swCross(Positions[1] - Positions[0], Positions[2] - Positions[0]), Normal) < 0.0f)
		{
			std::reverse(Positions.begin(), Positions.end());
			std::reverse(TexCoords.begin(), TexCoords.end());
		}

		const int VertexBase = static_cast<int>(Verts.size()) / 6;

		for (size_t VertexIdx = 0; VertexIdx < Positions.size(); VertexIdx++)
		{
			Verts.insert(Verts.end(), { Positions[VertexIdx].x, Positions[VertexIdx].y, Positions[VertexIdx].z });
			Verts.insert(Verts.end(), { TexCoords[VertexIdx].x, TexCoords[VertexIdx].y, TexCoords[VertexIdx].z });
		}

		mFacets[FacetIdx].Flags = Flags;
		mFacets[FacetIdx].IndexStart = static_cast<int>(Indices.size());

		for (int VertexIdx = 1; VertexIdx + 1 < static_cast<int>(Positions.size()); VertexIdx++)
			Indices.insert(Indices.end(), { static_cast<GLushort>(VertexBase), static_cast<GLushort>(VertexBase + VertexIdx), static_cast<GLushort>(VertexBase + VertexIdx + 1) });

		mFacets[FacetIdx].IndexCount = static_cast<int>(Indices.size()) - mFacets[FacetIdx].IndexStart;
		FacetIdx++;
	};

	// faces carry their label, the face index picks its row in the atlas
	for (int FaceIdx = 0; FaceIdx < mFaceCount; FaceIdx++)
	{
		const swVector3 Normal = swMultiply31(swVector3(0.0f, 0.0f, 0.0f), Transforms[FaceIdx]);
		const float Corners[4][2] = { { -1.0f, -1.0f }, { 1.0f, -1.0f }, { 1.0f, 1.0f }, { -1.0f, 1.0f } };
		std::vector<swVector3> Positions, TexCoords;

		for (const auto& Corner : Corners)
		{
			Positions.push_back(swMultiply31(swVector3(Corner[0] * Inner, Corner[1] * Inner, 0.0f), Transforms[FaceIdx]) + Normal * (Outer - 1.0f));
			TexCoords.push_back(swVector3((Corner[0] + 1.0f) * 0.5f, (Corner[1] + 1.0f) * 0.5f, static_cast<float>(FaceIdx)));
		}

		AddFacet(Positions, TexCoords, std::bitset<6>(1u << FaceIdx));
	}

	// edges and corners have no label
	const std::vector<swVector3> Blank(4, swVector3(0.0f, 0.0f, 0.0f));

	for (int Axis1Idx = 0; Axis1Idx < 3; Axis1Idx++)
	{
		for (int Axis2Idx = Axis1Idx + 1; Axis2Idx < 3; Axis2Idx++)
		{
			const int Axis3Idx = 3 - Axis1Idx - Axis2Idx;

			for (int SignIdx = 0; SignIdx < 4; SignIdx++)
			{
				const float Sign1 = (SignIdx & 1) ? -1.0f : 1.0f;
				const float Sign2 = (SignIdx & 2) ? -1.0f : 1.0f;
				std::vector<swVector3> Positions(4, swVector3(0.0f, 0.0f, 0.0f));

				for (int VertexIdx = 0; VertexIdx < 4; VertexIdx++)
				{
					const bool Outside1 = VertexIdx < 2;
					Positions[VertexIdx][Axis1Idx] = Sign1 * (Outside1 ? Outer : Inner);
					Positions[VertexIdx][Axis2Idx] = Sign2 * (Outside1 ? Inner : Outer);
					Positions[VertexIdx][Axis3Idx] = (VertexIdx == 0 || VertexIdx == 3) ? -Inner : Inner;
				}

				std::bitset<6> Flags;
				Flags.set(2 * Axis1Idx + ((SignIdx & 1) ? 1 : 0));
				Flags.set(2 * Axis2Idx + ((SignIdx & 2) ? 1 : 0));

				AddFacet(Positions, Blank, Flags);
			}
		}
	}

	for (int SignIdx = 0; SignIdx < mCornerCount; SignIdx++)
	{
		const swVector3 Signs((SignIdx & 1) ? -1.0f : 1.0f, (SignIdx & 2) ? -1.0f : 1.0f, (SignIdx & 4) ? -1.0f : 1.0f);
		std::vector<swVector3> Positions(3);
		std::bitset<6> Flags;

		for (int AxisIdx = 0; AxisIdx < 3; AxisIdx++)
		{
			for (int VertexAxisIdx = 0; VertexAxisIdx < 3; VertexAxisIdx++)
				Positions[AxisIdx][VertexAxisIdx] = Signs[VertexAxisIdx] * (VertexAxisIdx == AxisIdx ? Outer : Inner);

			Flags.set(2 * AxisIdx + (Signs[AxisIdx] < 0.0f ? 1 : 0));
		}

		AddFacet(Positions, std::vector<swVector3>(3, swVector3(0.0f, 0.0f, 0.0f)), Flags);
	}

	mIndexCount = static_cast<int>(Indices.size());
	mVertexBuffer = Context->CreateVertexBuffer(static_cast<int>(Verts.size()) * sizeof(float), Verts.data());
	mIndexBuffer = Context->CreateIndexBuffer(static_cast<int>(Indices.size()) * sizeof(GLushort), Indices.data());
}

void swViewCube::DestroyResources(swContext* Context)
{
	delete mTexture;
	mTexture = nullptr;
	Context->DestroyVertexBuffer(mVertexBuffer);
	Context->DestroyIndexBuffer(mIndexBuffer);
}

void swViewCube::Draw()
{
	UpdateSettings();

	if (!mSize || !mEnabled)
		return;

	swContext* Context = mView->mContext;
	const float UIScale = mView->GetUIScale();
	const int Width = mView->GetWidth();
	const int Height = mView->GetHeight();
	const int ViewportSize = mSize * UIScale;
	const int Left = (mLocation == swViewCubeLocation::BottomLeft || mLocation == swViewCubeLocation::TopLeft) ? 0 : Width - ViewportSize;
	const int Bottom = (mLocation == swViewCubeLocation::BottomLeft || mLocation == swViewCubeLocation::BottomRight) ? 0 : Height - ViewportSize;
	Context->SetViewport(Left, Bottom, ViewportSize, ViewportSize);

	Context->SetDepthFunction(swDepthFunction::Always);
	Context->EnableCullFace(true);

	Context->SetVertexBuffer(mVertexBuffer);
	Context->SetVertexFormat(0, 3, 0, 3, 0, false);
	Context->SetIndexBuffer(mIndexBuffer);

	Context->SetMaterial(swMaterialType::UnlitColor);
	Context->SetColor(swVector4(0.0f, 0.0f, 0.0f, 1.0f));

	float Scale = 1.005f + 2.0f / (float)ViewportSize;
	Context->SetWorldMatrix(swMatrix44Scale(swVector3(Scale, Scale, Scale)));
	Context->SetViewMatrix(GetViewMatrix());
	Context->SetProjectionMatrix(GetProjectionMatrix());

	Context->DrawIndexedPrimitives(GL_TRIANGLES, mIndexCount, GL_UNSIGNED_SHORT, 0);

	Context->SetMaterial(swMaterialType::UnlitViewCube);
	Context->BindTexture2D(mTexture);

	Context->SetWorldMatrix(swMatrix44Identity());
	Context->SetViewMatrix(GetViewMatrix());
	Context->SetProjectionMatrix(GetProjectionMatrix());

	const swPreferences& Preferences = swGetPreferences();
	const swVector4 TextColor = swVector4FromColor(Preferences.mViewCubeTextColor);
	const swVector4 BackgroundColor = swVector4FromColor(Preferences.mViewCubeColor);
	const swVector4 HighlightColor = swVector4FromColor(Preferences.mViewCubeHighlightColor);
	// a negative radius turns the position based highlight of the shader off
	const swVector4 NoHighlight(0.0f, 0.0f, 0.0f, -1.0f);

	// edges and corners are shaded toward the text color so the chamfers stand out
	const swVector4 EdgeColor = BackgroundColor + (TextColor - BackgroundColor) * 0.14f;
	const swVector4 CornerColor = BackgroundColor + (TextColor - BackgroundColor) * 0.28f;

	Context->SetHighlightParams(NoHighlight, TextColor, swVector4(swVector3(BackgroundColor), 1.0f), HighlightColor);
	DrawFacets(0, mFaceCount);

	Context->SetHighlightParams(NoHighlight, TextColor, swVector4(swVector3(EdgeColor), 1.0f), HighlightColor);
	DrawFacets(mFaceCount, mEdgeCount);

	Context->SetHighlightParams(NoHighlight, TextColor, swVector4(swVector3(CornerColor), 1.0f), HighlightColor);
	DrawFacets(mFaceCount + mEdgeCount, mCornerCount);

	if (mIntersectionFlags.any())
	{
		for (int FacetIdx = 0; FacetIdx < mFacetCount; FacetIdx++)
		{
			if (mFacets[FacetIdx].Flags != mIntersectionFlags)
				continue;

			Context->SetHighlightParams(NoHighlight, TextColor, HighlightColor, HighlightColor);
			DrawFacets(FacetIdx, 1);
			break;
		}
	}

	Context->SetDepthFunction(swDepthFunction::LessEqual);
	Context->EnableCullFace(false);

	Context->SetViewport(0, 0, mView->GetWidth(), mView->GetHeight());
}

void swViewCube::DrawFacets(int FirstFacet, int FacetCount) const
{
	const swViewCubeFacet& First = mFacets[FirstFacet];
	const swViewCubeFacet& Last = mFacets[FirstFacet + FacetCount - 1];

	mView->mContext->DrawIndexedPrimitives(GL_TRIANGLES, Last.IndexStart + Last.IndexCount - First.IndexStart, GL_UNSIGNED_SHORT, First.IndexStart * sizeof(GLushort));
}

bool swViewCube::OnLeftButtonDown()
{
	if (!mSize || !mEnabled)
		return false;

	mIntersectionFlags = GetIntersectionFlags();

	if (!mIntersectionFlags.any())
		return false;

	mMouseDownX = mView->GetMouseX();
	mMouseDownY = mView->GetMouseY();
	mMouseDown = true;

	return true;
}

bool swViewCube::OnLeftButtonUp()
{
	if (!mSize || !mEnabled)
		return false;

	if (!mMouseDown)
		return false;

	mMouseDown = false;

	if (!mIntersectionFlags.any())
		return false;

	swVector3 Position(0.0f, 0.0f, 0.0f);

	for (int AxisIdx = 0; AxisIdx < 3; AxisIdx++)
	{
		if (mIntersectionFlags.test(AxisIdx * 2))
			Position[AxisIdx] = 1250.0f;
		else if (mIntersectionFlags.test(AxisIdx * 2 + 1))
			Position[AxisIdx] = -1250.0f;
	}

	mView->SetViewpoint(Position);

	return true;
}

bool swViewCube::OnMouseMove()
{
	if (!mSize || !mEnabled)
		return false;

	if (IsDragging())
	{
		mIntersectionFlags.reset();
		mView->StartOrbitTracking();
		return true;
	}

	if (mView->IsTracking())
		return false;

	std::bitset<6> IntersectionFlags = GetIntersectionFlags();

	if (IntersectionFlags != mIntersectionFlags)
	{
		mIntersectionFlags = IntersectionFlags;
		mView->Redraw();
	}

	return mIntersectionFlags.any();
}

bool swViewCube::IsDragging() const
{
	int InputStateX = mView->GetMouseX();
	int InputStateY = mView->GetMouseY();
	return mMouseDown && (qAbs(mMouseDownX - InputStateX) > 3 || qAbs(mMouseDownY - InputStateY) > 3);
}

std::bitset<6> swViewCube::GetIntersectionFlags() const
{
	const float UIScale = mView->GetUIScale();
	const int Width = mView->GetWidth();
	const int Height = mView->GetHeight();
	const int ViewportSize = mSize * UIScale;
	const int Left = (mLocation == swViewCubeLocation::BottomLeft || mLocation == swViewCubeLocation::TopLeft) ? 0 : Width - ViewportSize;
	const int Bottom = (mLocation == swViewCubeLocation::BottomLeft || mLocation == swViewCubeLocation::BottomRight) ? 0 : Height - ViewportSize;
	const int x = mView->GetMouseX() - Left;
	const int y = mView->GetMouseY() - Bottom;

	if (x < 0 || x > ViewportSize || y < 0 || y > ViewportSize)
		return std::bitset<6>();

	swVector3 StartEnd[2] = { swVector3(x, y, 0), swVector3(x, y, 1) };
	const int Viewport[4] = { 0, 0, ViewportSize, ViewportSize };

	swUnprojectPoints(StartEnd, 2, GetViewMatrix(), GetProjectionMatrix(), Viewport);

	// the cube is the inside of all facet planes, the ray enters through the plane it crosses last
	const swVector3 Direction = StartEnd[1] - StartEnd[0];
	const float Inner = mHalfSize - mChamfer;
	float EnterDistance = -FLT_MAX;
	float ExitDistance = FLT_MAX;
	int EnterFacet = -1;

	for (int FacetIdx = 0; FacetIdx < mFacetCount; FacetIdx++)
	{
		const swVector3 Normal = swGetFacetNormal(mFacets[FacetIdx].Flags);
		const float PlaneDistance = mHalfSize + (static_cast<int>(mFacets[FacetIdx].Flags.count()) - 1) * Inner;
		const float Approach = swDot(Normal, Direction);
		const float Room = PlaneDistance - swDot(Normal, StartEnd[0]);

		if (fabsf(Approach) < 1e-6f)
		{
			if (Room < 0.0f)
				return std::bitset<6>();

			continue;
		}

		const float Distance = Room / Approach;

		if (Approach < 0.0f)
		{
			if (Distance > EnterDistance)
			{
				EnterDistance = Distance;
				EnterFacet = FacetIdx;
			}
		}
		else
			ExitDistance = swMinimum(ExitDistance, Distance);
	}

	if (EnterFacet == -1 || EnterDistance > ExitDistance)
		return std::bitset<6>();

	return mFacets[EnterFacet].Flags;
}
