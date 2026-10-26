#include "sw_global.h"
#include "sw_viewsphere.h"
#include "sw_view.h"
#include "camera.h"
#include "sw_context.h"
#include "sw_stringcache.h"
#include "sw_application.h"
#include "sw_texture.h"

swTexture* swViewSphere::mTexture;
swVertexBuffer swViewSphere::mVertexBuffer;
swIndexBuffer swViewSphere::mIndexBuffer;
int swViewSphere::mSphereIndexCount;
const float swViewSphere::mRadius = 1.0f;
const float swViewSphere::mHighlightRadius = 0.35f;
const int swViewSphere::mSubdivisions = 24;

swViewSphere::swViewSphere(swView* View)
	: mView(View)
{
	UpdateSettings();
}

void swViewSphere::UpdateSettings()
{
	const swPreferences& Preferences = swGetPreferences();

	switch (mView->GetViewType())
	{
	case swViewType::View:
		mSize = Preferences.mViewSphereSize;
		mEnabled = Preferences.mViewSphereEnabled;
		mLocation = Preferences.mViewSphereLocation;
		break;

	case swViewType::Preview:
		mSize = Preferences.mPreviewViewSphereSize;
		mEnabled = Preferences.mPreviewViewSphereEnabled;
		mLocation = Preferences.mPreviewViewSphereLocation;
		break;

	case swViewType::Minifig:
	case swViewType::PartsList:
	case swViewType::Count:
		break;
	}

	mEnabled = mEnabled && swContext::SupportsMaterial(swMaterialType::UnlitViewSphere);
}

swMatrix44 swViewSphere::GetViewMatrix() const
{
	swMatrix44 ViewMatrix = mView->GetCamera()->mWorldView;
	ViewMatrix.SetTranslation(swVector3(0, 0, 0));
	return ViewMatrix;
}

swMatrix44 swViewSphere::GetProjectionMatrix() const
{
	return swMatrix44Ortho(-mRadius * 1.25f, mRadius * 1.25f, -mRadius * 1.25f, mRadius * 1.25f, -mRadius * 1.25f, mRadius * 1.25f);
}

void swViewSphere::CreateResources(swContext* Context)
{
	constexpr int SourceCellSize = 128, CellHeight = 64, CellBottom = (SourceCellSize - CellHeight) / 2;
	constexpr int AtlasWidth = SourceCellSize, AtlasHeight = 512;
	mTexture = new swTexture();

	const QString ViewNames[6] =
	{
		QCoreApplication::translate("ViewName", "Left"), QCoreApplication::translate("ViewName", "Right"), QCoreApplication::translate("ViewName", "Back"),
		QCoreApplication::translate("ViewName", "Front"), QCoreApplication::translate("ViewName", "Top"), QCoreApplication::translate("ViewName", "Bottom")
	};

	QImage PainterImage(SourceCellSize, SourceCellSize, QImage::Format_ARGB32);
	QPainter Painter;
	// View sphere labels use the interface font, rendered large for the distance field texture.
	QFont Font = QGuiApplication::font();
	Font.setPointSize(36);
	QImage AtlasImage(AtlasWidth, AtlasHeight, QImage::Format_Alpha8);
	AtlasImage.fill(0);

	auto ConvertToSDF = [](QImage& TextureImage)
	{
		constexpr int Size = 128, Count = Size * Size;
		std::vector<int> ToInk(Count), ToOutside(Count);
		for (int i = 0; i < Count; i++) { ToInk[i] = TextureImage.constScanLine(i / Size)[i % Size] >= 128 ? 0 : 1 << 20; ToOutside[i] = TextureImage.constScanLine(i / Size)[i % Size] >= 128 ? 1 << 20 : 0; }
		auto Relax = [](std::vector<int>& Distance, bool Reverse)
		{
			for (int y = Reverse ? Size - 1 : 0; Reverse ? y >= 0 : y < Size; y += Reverse ? -1 : 1)
				for (int x = Reverse ? Size - 1 : 0; Reverse ? x >= 0 : x < Size; x += Reverse ? -1 : 1)
				{
					int& Value = Distance[y * Size + x]; const int Step = Reverse ? 1 : -1;
					if (y + Step >= 0 && y + Step < Size) Value = swMin(Value, Distance[(y + Step) * Size + x] + 3);
					if (x + Step >= 0 && x + Step < Size) Value = swMin(Value, Distance[y * Size + x + Step] + 3);
					if (y + Step >= 0 && y + Step < Size && x + Step >= 0 && x + Step < Size) Value = swMin(Value, Distance[(y + Step) * Size + x + Step] + 4);
				}
		};
		Relax(ToInk, false); Relax(ToInk, true); Relax(ToOutside, false); Relax(ToOutside, true);
		for (int i = 0; i < Count; i++) TextureImage.scanLine(i / Size)[i % Size] = static_cast<unsigned char>(swClamp(128 + (ToOutside[i] - ToInk[i]) / 3 * 16, 0, 255));
	};

	for (int ViewIdx = 0; ViewIdx < 6; ViewIdx++)
	{
		QImage TextureImage(SourceCellSize, SourceCellSize, QImage::Format_Alpha8);
		QFont LabelFont = Font;
		// The shader displays the middle 80% of each cell. Leave six pixels
		// on either side for the SDF halo, including for translated labels.
		constexpr int MaxLabelWidth = 90;
		while (LabelFont.pointSize() > 1 && QFontMetrics(LabelFont).horizontalAdvance(ViewNames[ViewIdx]) > MaxLabelWidth)
			LabelFont.setPointSize(LabelFont.pointSize() - 1);

		Painter.begin(&PainterImage);
		Painter.setRenderHint(QPainter::TextAntialiasing);
		Painter.fillRect(0, 0, PainterImage.width(), PainterImage.height(), QColor(0, 0, 0));
		Painter.setBrush(QColor(255, 255, 255));
		Painter.setPen(QColor(255, 255, 255));
		Painter.setFont(LabelFont);
		Painter.drawText(0, 0, PainterImage.width(), PainterImage.height(), Qt::AlignCenter, ViewNames[ViewIdx]);
		Painter.end();

		for (int y = 0; y < SourceCellSize; y++)
		{
			unsigned char* Dest = TextureImage.scanLine(SourceCellSize - y - 1);

			for (int x = 0; x < SourceCellSize; x++)
				*Dest++ = qRed(PainterImage.pixel(x, y));
		}
		ConvertToSDF(TextureImage);
		// Keep the middle band and its SDF margin in a vertical atlas.
		const int AtlasY = ViewIdx * CellHeight;
		for (int y = 0; y < CellHeight; y++)
			memcpy(AtlasImage.scanLine(AtlasY + y),
				TextureImage.constScanLine(CellBottom + y), SourceCellSize);
	}

	mTexture->SetImage(std::move(AtlasImage), SW_TEXTURE_LINEAR);
	mTexture->Upload(Context);

	const int SphereVertexCount = (mSubdivisions + 1) * (mSubdivisions + 1) * 6;
	mSphereIndexCount = mSubdivisions * mSubdivisions * 6 * 6;
	std::vector<float> Verts(SphereVertexCount * 6);
	std::vector<GLushort> Indices(mSphereIndexCount);

	swMatrix44 Transforms[6] =
	{
		swMatrix44(swVector4(0.0f,  1.0f, 0.0f, 0.0f), swVector4(0.0f,  0.0f, 1.0f, 0.0f), swVector4(1.0f, 0.0f, 0.0f, 0.0f), swVector4(1.0f,  0.0f,  0.0f, 1.0f)),
		swMatrix44(swVector4(0.0f, -1.0f, 0.0f, 0.0f), swVector4(0.0f,  0.0f, 1.0f, 0.0f), swVector4(1.0f, 0.0f, 0.0f, 0.0f), swVector4(-1.0f,  0.0f,  0.0f, 1.0f)),
		swMatrix44(swVector4(-1.0f,  0.0f, 0.0f, 0.0f), swVector4(0.0f,  0.0f, 1.0f, 0.0f), swVector4(0.0f, 1.0f, 0.0f, 0.0f), swVector4(0.0f,  1.0f,  0.0f, 1.0f)),
		swMatrix44(swVector4(1.0f,  0.0f, 0.0f, 0.0f), swVector4(0.0f,  0.0f, 1.0f, 0.0f), swVector4(0.0f, 1.0f, 0.0f, 0.0f), swVector4(0.0f, -1.0f,  0.0f, 1.0f)),
		swMatrix44(swVector4(1.0f,  0.0f, 0.0f, 0.0f), swVector4(0.0f,  1.0f, 0.0f, 0.0f), swVector4(0.0f, 0.0f, 1.0f, 0.0f), swVector4(0.0f,  0.0f,  1.0f, 1.0f)),
		swMatrix44(swVector4(1.0f,  0.0f, 0.0f, 0.0f), swVector4(0.0f, -1.0f, 0.0f, 0.0f), swVector4(0.0f, 0.0f, 1.0f, 0.0f), swVector4(0.0f,  0.0f, -1.0f, 1.0f)),
	};

	constexpr float Step = 2.0f / mSubdivisions;
	float* CurVert = Verts.data();
	auto ProjectToSphere = [](const swVector3& Vert)
	{
		const swVector3 Vert2 = Vert * Vert;
		return swVector3(Vert.x * sqrt(1.0 - 0.5 * (Vert2.y + Vert2.z) + Vert2.y * Vert2.z / 3.0),
			Vert.y * sqrt(1.0 - 0.5 * (Vert2.z + Vert2.x) + Vert2.z * Vert2.x / 3.0),
			Vert.z * sqrt(1.0 - 0.5 * (Vert2.x + Vert2.y) + Vert2.x * Vert2.y / 3.0));
	};

	for (int FaceIdx = 0; FaceIdx < 6; FaceIdx++)
	{
		for (int y = 0; y <= mSubdivisions; y++)
		{
			for (int x = 0; x <= mSubdivisions; x++)
			{
				const swVector3 Vert = swMul31(swVector3(Step * x - 1.0f, Step * y - 1.0f, 0.0f), Transforms[FaceIdx]);
				const swVector3 SphereVert = ProjectToSphere(Vert) * mRadius;
				*CurVert++ = SphereVert.x;
				*CurVert++ = SphereVert.y;
				*CurVert++ = SphereVert.z;
				*CurVert++ = x / static_cast<float>(mSubdivisions);
				*CurVert++ = y / static_cast<float>(mSubdivisions);
				*CurVert++ = static_cast<float>(FaceIdx);
			}
		}
	}

	GLushort* CurIndex = Indices.data();

	for (int FaceIdx = 0; FaceIdx < 6; FaceIdx++)
	{
		const int FaceBase = FaceIdx * (mSubdivisions + 1) * (mSubdivisions + 1);

		for (int y = 0; y < mSubdivisions; y++)
		{
			int RowBase = FaceBase + (mSubdivisions + 1) * y;

			for (int x = 0; x < mSubdivisions; x++)
			{
				*CurIndex++ = RowBase + x;
				*CurIndex++ = RowBase + x + 1;
				*CurIndex++ = RowBase + x + (mSubdivisions + 1);

				*CurIndex++ = RowBase + x + 1;
				*CurIndex++ = RowBase + x + 1 + (mSubdivisions + 1);
				*CurIndex++ = RowBase + x + (mSubdivisions + 1);
			}
		}
	}

	mVertexBuffer = Context->CreateVertexBuffer(static_cast<int>(Verts.size()) * sizeof(float), Verts.data());
	mIndexBuffer = Context->CreateIndexBuffer(static_cast<int>(Indices.size()) * sizeof(GLushort), Indices.data());
}

void swViewSphere::DestroyResources(swContext* Context)
{
	delete mTexture;
	mTexture = nullptr;
	Context->DestroyVertexBuffer(mVertexBuffer);
	Context->DestroyIndexBuffer(mIndexBuffer);
}

void swViewSphere::Draw()
{
	UpdateSettings();

	if (!mSize || !mEnabled)
		return;

	swContext* Context = mView->mContext;
	const float UIScale = mView->GetUIScale();
	const int Width = mView->GetWidth();
	const int Height = mView->GetHeight();
	const int ViewportSize = mSize * UIScale;
	const int Left = (mLocation == swViewSphereLocation::BottomLeft || mLocation == swViewSphereLocation::TopLeft) ? 0 : Width - ViewportSize;
	const int Bottom = (mLocation == swViewSphereLocation::BottomLeft || mLocation == swViewSphereLocation::BottomRight) ? 0 : Height - ViewportSize;
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

	Context->DrawIndexedPrimitives(GL_TRIANGLES, mSphereIndexCount, GL_UNSIGNED_SHORT, 0);

	Context->SetMaterial(swMaterialType::UnlitViewSphere);
	Context->BindTexture2D(mTexture);

	Context->SetWorldMatrix(swMatrix44Identity());
	Context->SetViewMatrix(GetViewMatrix());
	Context->SetProjectionMatrix(GetProjectionMatrix());

	swVector4 HighlightPosition(0.0f, 0.0f, 0.0f, 0.0f);

	if (mIntersectionFlags.any())
	{
		for (int AxisIdx = 0; AxisIdx < 3; AxisIdx++)
		{
			if (mIntersectionFlags.test(2 * AxisIdx))
				HighlightPosition[AxisIdx] = 1.0f;
			else if (mIntersectionFlags.test(2 * AxisIdx + 1))
				HighlightPosition[AxisIdx] = -1.0f;
		}

		HighlightPosition = swVector4(swNormalize(swVector3(HighlightPosition)), mHighlightRadius);
	}

	const swPreferences& Preferences = swGetPreferences();
	const swVector4 TextColor = swVector4FromColor(Preferences.mViewSphereTextColor);
	const swVector4 BackgroundColor = swVector4FromColor(Preferences.mViewSphereColor);
	const swVector4 HighlightColor = swVector4FromColor(Preferences.mViewSphereHighlightColor);

	Context->SetHighlightParams(HighlightPosition, TextColor, BackgroundColor, HighlightColor);
	Context->SetTextHaloColor(swVector4FromColor(Preferences.mTextHaloColor));
	Context->DrawIndexedPrimitives(GL_TRIANGLES, mSphereIndexCount, GL_UNSIGNED_SHORT, 0);

	Context->SetDepthFunction(swDepthFunction::LessEqual);
	Context->EnableCullFace(false);

	Context->SetViewport(0, 0, mView->GetWidth(), mView->GetHeight());
}

bool swViewSphere::OnLeftButtonDown()
{
	if (!mSize || !mEnabled)
		return false;

	mIntersectionFlags = GetIntersectionFlags(mIntersection);

	if (!mIntersectionFlags.any())
		return false;

	mMouseDownX = mView->GetMouseX();
	mMouseDownY = mView->GetMouseY();
	mMouseDown = true;

	return true;
}

bool swViewSphere::OnLeftButtonUp()
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

bool swViewSphere::OnMouseMove()
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

	std::bitset<6> IntersectionFlags = GetIntersectionFlags(mIntersection);

	if (IntersectionFlags != mIntersectionFlags)
	{
		mIntersectionFlags = IntersectionFlags;
		mView->Redraw();
	}

	return mIntersectionFlags.any();
}

bool swViewSphere::IsDragging() const
{
	int InputStateX = mView->GetMouseX();
	int InputStateY = mView->GetMouseY();
	return mMouseDown && (qAbs(mMouseDownX - InputStateX) > 3 || qAbs(mMouseDownY - InputStateY) > 3);
}

std::bitset<6> swViewSphere::GetIntersectionFlags(swVector3& Intersection) const
{
	const float UIScale = mView->GetUIScale();
	const int Width = mView->GetWidth();
	const int Height = mView->GetHeight();
	const int ViewportSize = mSize * UIScale;
	const int Left = (mLocation == swViewSphereLocation::BottomLeft || mLocation == swViewSphereLocation::TopLeft) ? 0 : Width - ViewportSize;
	const int Bottom = (mLocation == swViewSphereLocation::BottomLeft || mLocation == swViewSphereLocation::BottomRight) ? 0 : Height - ViewportSize;
	const int x = mView->GetMouseX() - Left;
	const int y = mView->GetMouseY() - Bottom;
	std::bitset<6> IntersectionFlags;

	if (x < 0 || x > Width || y < 0 || y > Height)
		return IntersectionFlags;

	swVector3 StartEnd[2] = { swVector3(x, y, 0), swVector3(x, y, 1) };
	const int Viewport[4] = { 0, 0, ViewportSize, ViewportSize };

	swUnprojectPoints(StartEnd, 2, GetViewMatrix(), GetProjectionMatrix(), Viewport);

	float Distance;
	if (swSphereRayMinIntersectDistance(swVector3(0.0f, 0.0f, 0.0f), mRadius, StartEnd[0], StartEnd[1], &Distance))
	{
		Intersection = (StartEnd[0] + (StartEnd[1] - StartEnd[0]) * Distance) / mRadius;

		auto CheckIntersection = [&]()
		{
			for (int Axis1Idx = 0; Axis1Idx < 6; Axis1Idx++)
			{
				swVector3 Point1(0.0f, 0.0f, 0.0f);

				Point1[Axis1Idx / 2] = Axis1Idx % 2 ? -1.0f : 1.0f;

				if (swLengthSquared(Point1 - Intersection) < mHighlightRadius * mHighlightRadius)
				{
					IntersectionFlags.set(Axis1Idx);
					return;
				}

				for (int Axis2Idx = 0; Axis2Idx < 6; Axis2Idx++)
				{
					if (Axis1Idx / 2 == Axis2Idx / 2)
						continue;

					swVector3 Point2(0.0f, 0.0f, 0.0f);
					Point2[Axis1Idx / 2] = Axis1Idx % 2 ? -0.70710678118f : 0.70710678118f;
					Point2[Axis2Idx / 2] = Axis2Idx % 2 ? -0.70710678118f : 0.70710678118f;

					if (swLengthSquared(Point2 - Intersection) < mHighlightRadius * mHighlightRadius)
					{
						IntersectionFlags.set(Axis1Idx);
						IntersectionFlags.set(Axis2Idx);
						return;
					}

					for (int Axis3Idx = 0; Axis3Idx < 6; Axis3Idx++)
					{
						if (Axis1Idx / 2 == Axis3Idx / 2 || Axis2Idx / 2 == Axis3Idx / 2)
							continue;

						swVector3 Point3(0.0f, 0.0f, 0.0f);
						Point3[Axis1Idx / 2] = Axis1Idx % 2 ? -0.57735026919f : 0.57735026919f;
						Point3[Axis2Idx / 2] = Axis2Idx % 2 ? -0.57735026919f : 0.57735026919f;
						Point3[Axis3Idx / 2] = Axis3Idx % 2 ? -0.57735026919f : 0.57735026919f;

						if (swLengthSquared(Point3 - Intersection) < mHighlightRadius * mHighlightRadius)
						{
							IntersectionFlags.set(Axis1Idx);
							IntersectionFlags.set(Axis2Idx);
							IntersectionFlags.set(Axis3Idx);
							return;
						}
					}
				}
			}
		};

		CheckIntersection();
	}

	return IntersectionFlags;
}
