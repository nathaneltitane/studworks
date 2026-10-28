#include "sw_global.h"
#include "sw_view.h"
#include "sw_view_widget.h"
#include <stdlib.h>
#include "sw_window_main.h"
#include "sw_object_light.h"
#include "sw_object_camera.h"
#include "sw_texture_font.h"
#include "sw_texture.h"
#include "sw_object_part.h"
#include "sw_data_part.h"
#include "sw_track_train.h"
#include "sw_scene.h"
#include "sw_context.h"
#include "sw_view_manipulator.h"
#include "sw_view_cube.h"
#include "sw_widget_find_replace.h"
#include "sw_library.h"
#include "sw_window_utilities.h"

void swView::OnDraw()
{
	if (!mModel)
		return;

//#define SW_PROFILE_DRAW
#ifdef SW_PROFILE_DRAW
	static std::array<GLuint64, 30> QueryResults;
	static std::array<qint64, 30> TimerResults;
	static int FrameNumber;

	QElapsedTimer Timer;
	QOpenGLTimerQuery Query;

	if (!Query.isCreated())
		Query.create();

	Query.begin();
	Timer.start();
#endif

	const swPreferences& Preferences = swGetPreferences();
	// the turntable shows the bare model
	const bool DrawOverlays = mWidget != nullptr && mViewType != swViewType::Turntable;
	const bool DrawInterface = mWidget != nullptr && mViewType == swViewType::View;

	swRenderTarget RenderTarget = swRenderTarget::ModelView;

	if (mViewType == swViewType::PartsList)
		RenderTarget = swRenderTarget::Parts;
	else if (mViewType == swViewType::Preview)
		RenderTarget = swRenderTarget::Preview;
	else if (mViewType == swViewType::MinifigureBuilder)
		RenderTarget = swRenderTarget::MinifigureBuilder;

	const swRenderSettings RenderSettings = Preferences.GetRenderSettings(RenderTarget);
	swShadingMode ShadingMode = RenderSettings.ShadingMode;
	if (ShadingMode == swShadingMode::Wireframe && !mWidget)
		ShadingMode = swShadingMode::Flat;

	mScene->SetShadingMode(ShadingMode);
	mScene->SetLightingStyle(Preferences.mLightingStyle);
	mScene->SetLineSettings(RenderSettings.DrawEdgeLines, RenderSettings.DrawConditionalLines, RenderSettings.LineWidth * mLineWidthScale);
	mScene->SetAllowLOD(Preferences.mAllowLOD && mWidget != nullptr);
	mScene->SetRequireCompleteAssets(static_cast<bool>(mRenderFramebuffer));
	mScene->SetRequireGeneratedMeshes(mRenderFramebuffer && mViewType != swViewType::PartsList);
	mScene->SetLODDistance(Preferences.mMeshLODDistance);
	mScene->SetCutaway(mViewType == swViewType::View ? mCutawayShape : swCutawayShape::None, GetCutawayMatrix());

	mScene->Begin(mCamera->mWorldView);

	mScene->SetActiveSubmodelInstance(mActiveSubmodelInstance, mActiveSubmodelTransform, GetModelEditingPath());
	mScene->SetDrawInterface(DrawInterface);

	bool DrawInsertPreview = DrawInterface && std::any_of(mViews.begin(), mViews.end(), [](swView* View){ return View->mTrackTool == swTrackTool::Insert; });
	mScene->SetDrawInsertPreview(DrawInsertPreview);
	
	mModel->GetScene(mScene.get(), mCamera, Preferences.mHighlightNewParts, Preferences.mFadeSteps && mViewType == swViewType::View);

	mScene->End();

	int TotalTileRows = 1;
	int TotalTileColumns = 1;

	if (!mRenderImage.isNull())
	{
		int ImageWidth = mRenderImage.width();
		int ImageHeight = mRenderImage.height();

		if (ImageWidth > mWidth || ImageHeight > mHeight)
		{
			TotalTileColumns = (mWidth + ImageWidth - 1) / mWidth;
			TotalTileRows = (mHeight + ImageHeight - 1) / mHeight;
		}
	}

	for (int CurrentTileRow = 0; CurrentTileRow < TotalTileRows; CurrentTileRow++)
	{
		for (int CurrentTileColumn = 0; CurrentTileColumn < TotalTileColumns; CurrentTileColumn++)
		{
			mContext->SetDefaultState();
			mContext->SetViewport(0, 0, mWidth, mHeight);

			int CurrentTileWidth, CurrentTileHeight;

			if (!mRenderImage.isNull() && (TotalTileRows > 1 || TotalTileColumns > 1))
			{
				if (CurrentTileRow < TotalTileRows - 1)
					CurrentTileHeight = mHeight;
				else
					CurrentTileHeight = mRenderImage.height() - (TotalTileRows - 1) * (mHeight);

				if (CurrentTileColumn < TotalTileColumns - 1)
					CurrentTileWidth = mWidth;
				else
					CurrentTileWidth = mRenderImage.width() - (TotalTileColumns - 1) * (mWidth);

				DrawBackground();

				mContext->SetViewport(0, 0, CurrentTileWidth, CurrentTileHeight);
				mContext->SetProjectionMatrix(GetTileProjectionMatrix(CurrentTileRow, CurrentTileColumn, CurrentTileWidth, CurrentTileHeight));
			}
			else
			{
				CurrentTileWidth = mWidth;
				CurrentTileHeight = mHeight;

				DrawBackground();

				mContext->SetProjectionMatrix(GetProjectionMatrix());
			}

			mContext->SetLineWidth(RenderSettings.LineWidth * mLineWidthScale);

			const bool OutlineEnabled = DrawInterface && !mRenderFramebuffer && mRenderImage.isNull() &&
				ShadingMode != swShadingMode::Wireframe && Preferences.mHighlightRenderMode == swHighlightRenderMode::Outline &&
				(mScene->HasSelectedMeshes() || mScene->HasHighlightedMeshes()) && PrepareOutlineFramebuffers();
			
			bool OutlineMasksReady = false;

			mScene->SetHighlightRenderMode(OutlineEnabled ? swHighlightRenderMode::Outline : swHighlightRenderMode::Tint);
			mScene->Draw(mContext, [this, OutlineEnabled, DrawInterface, &OutlineMasksReady]()
			{
				if (OutlineEnabled)
					OutlineMasksReady = GenerateOutlineMasks();

				if (DrawInterface)
					DrawGrid();
			});

			if (OutlineEnabled && OutlineMasksReady)
				mContext->DrawOutline(*mOutlineFullFramebuffer, *mOutlineVisibleFramebuffer, mWidget->GetDeviceScale());

			if (OutlineEnabled && !OutlineMasksReady)
			{
				mScene->SetHighlightRenderMode(swHighlightRenderMode::Tint);
				
				DrawBackground();
				
				mContext->SetProjectionMatrix(GetProjectionMatrix());
				
				mScene->Draw(mContext, [this]() { DrawGrid(); });
			}

			if (!mRenderImage.isNull())
			{
				UnbindRenderFramebuffer();
				QImage TileImage = GetRenderFramebufferImage();
				BindRenderFramebuffer();

				if (TileImage.isNull())
				{
					mRenderImage = QImage();
					return;
				}

				const quint8* Buffer = TileImage.constBits();
				uchar* ImageBuffer = mRenderImage.bits();

				if (!ImageBuffer)
				{
					mRenderImage = QImage();
					return;
				}

				quint32 TileY = 0, SrcY = 0;
				if (CurrentTileRow != TotalTileRows - 1)
					TileY = (TotalTileRows - CurrentTileRow - 1) * mHeight - ((mHeight - mRenderImage.height() % mHeight) % mHeight);
				else if (TotalTileRows > 1)
					SrcY = (mHeight - mRenderImage.height() % mHeight) % mHeight;

				quint32 TileStart = ((CurrentTileColumn * mWidth) + (TileY * mRenderImage.width())) * 4;

				for (int y = 0; y < CurrentTileHeight; y++)
				{
					const quint8* src = Buffer + (SrcY + y) * mWidth * 4;
					quint8* dst = ImageBuffer + TileStart + y * mRenderImage.width() * 4;

					memcpy(dst, src, CurrentTileWidth * 4);
				}
			}
		}
	}

	if (DrawInterface)
		mScene->DrawInterfaceObjects(mContext);

	if (DrawInterface)
		DrawCutaway();

	if (DrawOverlays)
		DrawAxes();

	if (DrawInterface && mTrackTool != swTrackTool::Insert)
	{
		swTool Tool = gMainWindow->GetTool();
		swModel* ActiveModel = GetActiveModel();

		if ((Tool == swTool::Select || Tool == swTool::Move) && mTrackButton == swTrackButton::None && ActiveModel->AnyObjectsSelected())
			mViewManipulator->DrawSelectMove(mTrackButton, mTrackTool, mTrackToolSection);
		else if (GetCurrentTool() == swTool::Move && mTrackButton != swTrackButton::None)
			mViewManipulator->DrawSelectMove(mTrackButton, mTrackTool, mTrackToolSection);
		else if ((Tool == swTool::Rotate || (Tool == swTool::Select && mTrackButton != swTrackButton::None && mTrackTool >= swTrackTool::RotateX && mTrackTool <= swTrackTool::RotateCamera)) && ActiveModel->CanRotateSelection())
			mViewManipulator->DrawRotate(mTrackButton, mTrackTool);
		else if ((mTrackTool == swTrackTool::Select || mTrackTool == swTrackTool::ZoomRegion) && mTrackButton != swTrackButton::None)
			DrawSelectZoomRegionOverlay();
		else if (Tool == swTool::RotateView && mTrackButton == swTrackButton::None)
			DrawRotateViewOverlay();
	}

	if (DrawOverlays)
	{
		mViewCube->Draw();

		DrawViewport();
	}

#ifdef SW_PROFILE_DRAW
	qint64 TimerElapsed = Timer.nsecsElapsed();
	Query.end();

	GLuint64 QueryElapsed = Query.waitForResult();
	QueryResults[FrameNumber % QueryResults.size()] = QueryElapsed;
	TimerResults[FrameNumber % TimerResults.size()] = TimerElapsed;
	FrameNumber++;

	GLuint64 QueryAverage = 0;
	for (GLuint64 Result : QueryResults)
		QueryAverage += Result;
	QueryAverage /= QueryResults.size();

	GLuint64 TimerAverage = 0;
	for (GLuint64 Result : TimerResults)
		TimerAverage += Result;
	TimerAverage /= TimerResults.size();

	if (gTexFont.IsLoaded())
	{
		mContext->SetWorldMatrix(swMatrix44Identity());
		mContext->SetViewMatrix(swMatrix44Translation(swVector3(0.375, 0.375, 0.0)));
		mContext->SetProjectionMatrix(swMatrix44Orthographic(0.0f, mWidth, 0.0f, mHeight, -1.0f, 1.0f));

		QString Line = QString("GPU: %1 CPU: %2").arg(QString::number(QueryAverage / 1000000.0, 'f', 2), QString::number(TimerAverage / 1000000.0, 'f', 2));

		mContext->SetMaterial(swMaterialType::TextSDF);
		mContext->SetColor(swVector4FromColor(swGetPreferences().mTextColor));
		mContext->SetTextHaloColor(swVector4FromColor(swGetPreferences().mTextHaloColor));
		mContext->BindTexture2D(gTexFont.GetTexture());

		mContext->EnableDepthTest(false);
		mContext->EnableColorBlend(true);

		gTexFont.PrintText(mContext, 3.0f, (float)mHeight - 1.0f - 6.0f, 0.0f, Line.toLatin1().constData());

		mContext->EnableColorBlend(false);
		mContext->EnableDepthTest(true);
	}

	Redraw();
#endif

	mContext->ClearResources();
}

bool swView::HasMissingAssets() const
{
	return mScene->HasMissingAssets();
}

void swView::DrawBackground() const
{
	if (mOverrideBackgroundColor)
	{
		swVector4 BackgroundColor(swVector4FromColor(mBackgroundColor));
		mContext->ClearColorAndDepth(BackgroundColor);
		return;
	}

	const swPreferences& Preferences = swGetPreferences();
	float Alpha = mContext->IsOffscreen() ? 0.0f : 1.0f;

	swVector4 BackgroundColor(swVector3FromColor(Preferences.mBackgroundSolidColor), Alpha);
	mContext->ClearColorAndDepth(BackgroundColor);
}

void swView::DrawViewport() const
{
	float Scale = GetUIScale();
	float Width = mWidth / Scale;
	float Height = mHeight / Scale;

	mContext->SetWorldMatrix(swMatrix44Identity());
	mContext->SetViewMatrix(swMatrix44Translation(swVector3(0.375, 0.375, 0.0)));
	mContext->SetProjectionMatrix(swMatrix44Orthographic(0.0f, Width, 0.0f, Height, -1.0f, 1.0f));
	mContext->SetLineWidth(1.0f);

	mContext->SetDepthWrite(false);
	mContext->EnableDepthTest(false);

	mContext->SetMaterial(swMaterialType::UnlitColor);

	if (mLastFocusedView == this)
		mContext->SetColor(swVector4FromColor(swGetPreferences().mActiveViewColor));
	else
		mContext->SetColor(swVector4FromColor(swGetPreferences().mInactiveViewColor));

	float Verts[8] = { 0.0f, 0.0f, Width - 1.0f, 0.0f, Width - 1.0f, Height - 1.0f, 0.0f, Height - 1.0f };

	if (mDrawViewportBorder)
	{
		mContext->SetVertexBufferPointer(Verts);
		mContext->SetVertexFormatPosition(2);
		mContext->DrawPrimitives(GL_LINE_LOOP, 0, 4);
	}

	QString CameraName = mCamera->GetName();

	if (!CameraName.isEmpty() && gTexFont.IsLoaded())
	{
		mContext->SetMaterial(swMaterialType::TextSDF);
		mContext->SetColor(swVector4FromColor(swGetPreferences().mTextColor));
		mContext->SetTextHaloColor(swVector4FromColor(swGetPreferences().mTextHaloColor));
		mContext->BindTexture2D(gTexFont.GetTexture());

		mContext->EnableColorBlend(true);

		// TODO: Make swTextureFont support Unicode camera names instead of converting them to Latin-1.
		gTexFont.PrintText(mContext, 3.0f, (float)Height - 1.0f - 6.0f, 0.0f, CameraName.toLatin1().constData());

		mContext->EnableColorBlend(false);
	}

	mContext->SetDepthWrite(true);
	mContext->EnableDepthTest(true);
}

void swView::DrawAxes() const
{
	const swPreferences& Preferences = swGetPreferences();

	switch (mViewType)
	{
		case swViewType::View:
			if (!Preferences.mDrawAxes)
				return;
			break;

		case swViewType::Preview:
			if (!Preferences.mDrawPreviewAxis)
				return;
			break;

		case swViewType::MinifigureBuilder:
		case swViewType::PartsList:
		case swViewType::Turntable:
		case swViewType::Count:
			return;
	}

//	mContext->ClearDepth();

	struct swAxisVertex
	{
		float x, y, z;
		quint32 Color;
	};

	const quint32 Red = Preferences.mAxisXColor;
	const quint32 Green = Preferences.mAxisYColor;
	const quint32 Blue = Preferences.mAxisZColor;

	const swAxisVertex Verts[30] =
	{
		{  0.00f,  0.00f,  0.00f, Red }, { 20.00f,  0.00f,  0.00f, Red }, { 12.00f,  3.00f,  0.00f, Red }, { 12.00f,  2.12f,  2.12f, Red }, { 12.00f,  0.00f,  3.00f, Red },
		{ 12.00f, -2.12f,  2.12f, Red }, { 12.00f, -3.00f,  0.00f, Red }, { 12.00f, -2.12f, -2.12f, Red }, { 12.00f,  0.00f, -3.00f, Red }, { 12.00f,  2.12f, -2.12f, Red },
		{  0.00f,  0.00f,  0.00f, Green }, {  0.00f, 20.00f,  0.00f, Green }, {  3.00f, 12.00f,  0.00f, Green }, {  2.12f, 12.00f,  2.12f, Green }, {  0.00f, 12.00f,  3.00f, Green },
		{ -2.12f, 12.00f,  2.12f, Green }, { -3.00f, 12.00f,  0.00f, Green }, { -2.12f, 12.00f, -2.12f, Green }, {  0.00f, 12.00f, -3.00f, Green }, {  2.12f, 12.00f, -2.12f, Green },
		{  0.00f,  0.00f,  0.00f, Blue }, {  0.00f,  0.00f, 20.00f, Blue }, {  0.00f,  3.00f, 12.00f, Blue }, {  2.12f,  2.12f, 12.00f, Blue }, {  3.00f,  0.00f, 12.00f, Blue },
		{  2.12f, -2.12f, 12.00f, Blue }, {  0.00f, -3.00f, 12.00f, Blue }, { -2.12f, -2.12f, 12.00f, Blue }, { -3.00f,  0.00f, 12.00f, Blue }, { -2.12f,  2.12f, 12.00f, Blue }
	};

	const GLushort Indices[78] =
	{
		 0,  1, 10, 11, 20, 21,
		 1,  2,  3,  1,  3,  4,  1,  4,  5,  1,  5,  6,  1,  6,  7,  1,  7,  8,  1,  8,  9,  1,  9,  2,
		11, 12, 13, 11, 13, 14, 11, 14, 15, 11, 15, 16, 11, 16, 17, 11, 17, 18, 11, 18, 19, 11, 19, 12,
		21, 22, 23, 21, 23, 24, 21, 24, 25, 21, 25, 26, 21, 26, 27, 21, 27, 28, 21, 28, 29, 21, 29, 22
	};

	// the arrows span half the icon size from the origin
	const float IconSize = static_cast<float>(mViewType == swViewType::Preview ? Preferences.mPreviewAxisIconSize : Preferences.mAxisIconSize);
	const float ArrowLength = IconSize / 2.0f;
	const float NearOffset = ArrowLength + 12.0f;
	const float FarOffset = ArrowLength + 16.0f;

	swMatrix44 TranslationMatrix;
	float Scale = GetUIScale();

	switch (Preferences.mAxisIconLocation)
	{
	default:
	case swAxisIconLocation::BottomLeft:
		TranslationMatrix = swMatrix44Translation(swVector3(NearOffset, NearOffset, 0.0f) / Scale);
		break;

	case swAxisIconLocation::BottomRight:
		TranslationMatrix = swMatrix44Translation(swVector3(mWidth - FarOffset, NearOffset, 0.0f) / Scale);
		break;

	case swAxisIconLocation::TopLeft:
		TranslationMatrix = swMatrix44Translation(swVector3(NearOffset, mHeight - FarOffset, 0.0f) / Scale);
		break;

	case swAxisIconLocation::TopRight:
		TranslationMatrix = swMatrix44Translation(swVector3(mWidth - FarOffset, mHeight - FarOffset, 0.0f) / Scale);
		break;
	}

	swMatrix44 WorldViewMatrix = mCamera->mWorldView;
	WorldViewMatrix.SetTranslation(swVector3(0, 0, 0));

	mContext->SetLineWidth(qMax(1.0f, IconSize / 48.0f));
	mContext->SetMaterial(swMaterialType::UnlitVertexColor);
	mContext->SetWorldMatrix(swMatrix44Scale(swVector3(ArrowLength / 20.0f, ArrowLength / 20.0f, ArrowLength / 20.0f)));
	mContext->SetViewMatrix(swMultiply(WorldViewMatrix, TranslationMatrix));
	mContext->SetProjectionMatrix(swMatrix44Orthographic(0, mWidth / Scale, 0, mHeight / Scale, -IconSize, IconSize));

	mContext->SetVertexBufferPointer(Verts);
	mContext->SetVertexFormat(0, 3, 0, 0, 4, false);
	mContext->SetIndexBufferPointer(Indices);

	mContext->DrawIndexedPrimitives(GL_LINES, 6, GL_UNSIGNED_SHORT, 0);
	mContext->SetLineWidth(1.0f);
	mContext->DrawIndexedPrimitives(GL_TRIANGLES, 72, GL_UNSIGNED_SHORT, 6 * 2);

	if (!gTexFont.IsLoaded())
		return;

	mContext->SetMaterial(swMaterialType::TextSDF);
	mContext->SetWorldMatrix(swMatrix44Identity());
	mContext->SetViewMatrix(TranslationMatrix);
	mContext->BindTexture2D(gTexFont.GetTexture());
	mContext->EnableColorBlend(true);

	float TextBuffer[6 * 5 * 3];
	swVector3 PosX = swMultiply30(swVector3(ArrowLength + 5.0f, 0.0f, 0.0f), WorldViewMatrix);
	gTexFont.GetGlyphTriangles(PosX.x, PosX.y, PosX.z, 'X', TextBuffer);
	swVector3 PosY = swMultiply30(swVector3(0.0f, ArrowLength + 5.0f, 0.0f), WorldViewMatrix);
	gTexFont.GetGlyphTriangles(PosY.x, PosY.y, PosY.z, 'Y', TextBuffer + 5 * 6);
	swVector3 PosZ = swMultiply30(swVector3(0.0f, 0.0f, ArrowLength + 5.0f), WorldViewMatrix);
	gTexFont.GetGlyphTriangles(PosZ.x, PosZ.y, PosZ.z, 'Z', TextBuffer + 5 * 6 * 2);

	mContext->SetVertexBufferPointer(TextBuffer);
	mContext->SetVertexFormat(0, 3, 0, 2, 0, false);

	mContext->SetColor(swVector4FromColor(swGetPreferences().mTextColor));
	mContext->SetTextHaloColor(swVector4FromColor(swGetPreferences().mTextHaloColor));
	mContext->DrawPrimitives(GL_TRIANGLES, 0, 6 * 3);

	mContext->EnableColorBlend(false);
}

// use to outline the cutaway so it can be placed against the model
// use to fill a buffer with the triangles of the unit cube, sphere or cylinder
static void swAddCutawayShapeTriangles(swCutawayShape CutawayShape, std::vector<float>& Verts)
{
	const int Segments = 32;
	const int Rings = 16;

	const auto AddVertex = [&Verts](const swVector3& Point)
	{
		Verts.push_back(Point.x);
		Verts.push_back(Point.y);
		Verts.push_back(Point.z);
	};

	const auto AddQuad = [&AddVertex](const swVector3& Point1, const swVector3& Point2, const swVector3& Point3, const swVector3& Point4)
	{
		AddVertex(Point1);
		AddVertex(Point2);
		AddVertex(Point3);
		AddVertex(Point1);
		AddVertex(Point3);
		AddVertex(Point4);
	};

	if (CutawayShape == swCutawayShape::Cube)
	{
		const swVector3 Axes[3] = { swVector3(1.0f, 0.0f, 0.0f), swVector3(0.0f, 1.0f, 0.0f), swVector3(0.0f, 0.0f, 1.0f) };

		for (int AxisIndex = 0; AxisIndex < 3; AxisIndex++)
		{
			const swVector3& Side = Axes[(AxisIndex + 1) % 3];
			const swVector3& Up = Axes[(AxisIndex + 2) % 3];

			for (float Face = -1.0f; Face <= 1.0f; Face += 2.0f)
			{
				const swVector3 Normal = Axes[AxisIndex] * Face;

				AddQuad(Normal - Side - Up, Normal + Side - Up, Normal + Side + Up, Normal - Side + Up);
			}
		}
	}
	else if (CutawayShape == swCutawayShape::Sphere)
	{
		const auto SpherePoint = [](int Ring, int Segment)
		{
			const float Latitude = Ring * SW_PI / Rings - SW_PI / 2.0f;
			const float Longitude = Segment * 2.0f * SW_PI / Segments;

			return swVector3(cosf(Latitude) * cosf(Longitude), cosf(Latitude) * sinf(Longitude), sinf(Latitude));
		};

		for (int Ring = 0; Ring < Rings; Ring++)
			for (int Segment = 0; Segment < Segments; Segment++)
				AddQuad(SpherePoint(Ring, Segment), SpherePoint(Ring, Segment + 1), SpherePoint(Ring + 1, Segment + 1), SpherePoint(Ring + 1, Segment));
	}
	else if (CutawayShape == swCutawayShape::Cylinder)
	{
		const swVector3 Height(0.0f, 0.0f, 1.0f);

		for (int Segment = 0; Segment < Segments; Segment++)
		{
			const float Angle1 = Segment * 2.0f * SW_PI / Segments;
			const float Angle2 = (Segment + 1) * 2.0f * SW_PI / Segments;
			const swVector3 Side1(cosf(Angle1), sinf(Angle1), 0.0f);
			const swVector3 Side2(cosf(Angle2), sinf(Angle2), 0.0f);

			AddQuad(Side1 - Height, Side2 - Height, Side2 + Height, Side1 + Height);
			AddVertex(Height);
			AddVertex(Side1 + Height);
			AddVertex(Side2 + Height);
			AddVertex(-Height);
			AddVertex(Side2 - Height);
			AddVertex(Side1 - Height);
		}
	}
}

void swView::DrawCutaway() const
{
	if (mViewType != swViewType::View || mCutawayShape == swCutawayShape::None)
		return;

	const int CircleSegments = 48;
	float Verts[CircleSegments * 3 * 2 * 3];
	int VertexCount = 0;

	const auto AddVertex = [&Verts, &VertexCount](float x, float y, float z)
	{
		Verts[VertexCount * 3] = x;
		Verts[VertexCount * 3 + 1] = y;
		Verts[VertexCount * 3 + 2] = z;
		VertexCount++;
	};

	if (mCutawayShape == swCutawayShape::Cube)
	{
		for (int AxisIndex = 0; AxisIndex < 3; AxisIndex++)
		{
			for (int Corner = 0; Corner < 4; Corner++)
			{
				float Start[3], End[3];

				Start[AxisIndex] = -1.0f;
				End[AxisIndex] = 1.0f;
				Start[(AxisIndex + 1) % 3] = End[(AxisIndex + 1) % 3] = Corner & 1 ? 1.0f : -1.0f;
				Start[(AxisIndex + 2) % 3] = End[(AxisIndex + 2) % 3] = Corner & 2 ? 1.0f : -1.0f;

				AddVertex(Start[0], Start[1], Start[2]);
				AddVertex(End[0], End[1], End[2]);
			}
		}
	}
	else if (mCutawayShape == swCutawayShape::Cylinder)
	{
		for (int Segment = 0; Segment < CircleSegments; Segment++)
		{
			const float Angle1 = Segment * 2.0f * SW_PI / CircleSegments;
			const float Angle2 = (Segment + 1) * 2.0f * SW_PI / CircleSegments;

			for (float Height = -1.0f; Height <= 1.0f; Height += 2.0f)
			{
				AddVertex(cosf(Angle1), sinf(Angle1), Height);
				AddVertex(cosf(Angle2), sinf(Angle2), Height);
			}

			// four side lines join the end circles
			if (Segment % (CircleSegments / 4) == 0)
			{
				AddVertex(cosf(Angle1), sinf(Angle1), -1.0f);
				AddVertex(cosf(Angle1), sinf(Angle1), 1.0f);
			}
		}
	}
	else
	{
		for (int AxisIndex = 0; AxisIndex < 3; AxisIndex++)
		{
			for (int Segment = 0; Segment < CircleSegments; Segment++)
			{
				for (int Step = 0; Step < 2; Step++)
				{
					const float Angle = (Segment + Step) * 2.0f * SW_PI / CircleSegments;
					float Point[3];

					Point[AxisIndex] = 0.0f;
					Point[(AxisIndex + 1) % 3] = cosf(Angle);
					Point[(AxisIndex + 2) % 3] = sinf(Angle);

					AddVertex(Point[0], Point[1], Point[2]);
				}
			}
		}
	}

	const swMatrix44 WorldMatrix(swVector4(mCutawayShapeAxes[0] * mCutawaySize, 0.0f), swVector4(mCutawayShapeAxes[1] * mCutawaySize, 0.0f), swVector4(mCutawayShapeAxes[2] * mCutawaySize, 0.0f), swVector4(mCutawayCenter, 1.0f));

	mContext->SetLineWidth(1.0f);
	mContext->SetMaterial(swMaterialType::UnlitColor);
	mContext->SetWorldMatrix(WorldMatrix);
	mContext->SetViewMatrix(mCamera->mWorldView);
	mContext->SetProjectionMatrix(GetProjectionMatrix());

	// a very light shade on the faces shows where the cut runs through the model
	std::vector<float> ShadeVerts;
	swAddCutawayShapeTriangles(mCutawayShape, ShadeVerts);

	mContext->EnableColorBlend(true);
	mContext->SetDepthWrite(false);
	// the shade stays between 90% and 99% transparent
	swVector4 ShadeColor = swVector4FromColor(swGetPreferences().mCutawayShadeColor);
	ShadeColor.w = (100 - qBound(90, swGetPreferences().mCutawayShadeTransparency, 99)) / 100.0f;
	mContext->SetColor(ShadeColor);
	mContext->SetVertexBufferPointer(ShadeVerts.data());
	mContext->SetVertexFormatPosition(3);
	mContext->DrawPrimitives(GL_TRIANGLES, 0, static_cast<int>(ShadeVerts.size() / 3));
	mContext->SetDepthWrite(true);
	mContext->EnableColorBlend(false);

	mContext->EnableDepthTest(false);
	mContext->SetColor(swVector4FromColor(swGetPreferences().mOverlayColor));

	mContext->SetVertexBufferPointer(Verts);
	mContext->SetVertexFormatPosition(3);
	mContext->DrawPrimitives(GL_LINES, 0, VertexCount);

	if (mWidget && mCutawayPickShape == swCutawayShape::None)
	{
		// the center handle is a small solid copy of the cutaway shape
		const swPreferences& Preferences = swGetPreferences();
		const float CenterSize = GetCutawayHandleLength() * 0.1f;
		const int ActiveHandle = mCutawayDragHandle != -1 ? mCutawayDragHandle : mCutawayHoverHandle;

		mContext->SetWorldMatrix(swMatrix44(swVector4(mCutawayShapeAxes[0] * CenterSize, 0.0f), swVector4(mCutawayShapeAxes[1] * CenterSize, 0.0f), swVector4(mCutawayShapeAxes[2] * CenterSize, 0.0f), swVector4(mCutawayCenter, 1.0f)));
		mContext->SetColor(swVector4FromColor(ActiveHandle == 3 ? Preferences.mObjectFocusedColor : Preferences.mOverlayColor));
		mContext->SetVertexBufferPointer(ShadeVerts.data());
		mContext->SetVertexFormatPosition(3);
		mContext->DrawPrimitives(GL_TRIANGLES, 0, static_cast<int>(ShadeVerts.size() / 3));

		DrawCutawayGizmo();
	}

	mContext->EnableDepthTest(true);
}

// use to draw the arrows that move the cutaway and the knobs that resize it
void swView::DrawCutawayGizmo() const
{
	const int ConeSegments = 12;
	const float ConeStart = 0.75f;
	const float ConeRadius = 0.08f;
	const float BoxSize = 0.07f;
	float Verts[(3 * 2 + 3 * ConeSegments * 3 + 36) * 3];
	int VertexCount = 0;

	const auto AddVertex = [&Verts, &VertexCount](const swVector3& Point)
	{
		Verts[VertexCount * 3] = Point.x;
		Verts[VertexCount * 3 + 1] = Point.y;
		Verts[VertexCount * 3 + 2] = Point.z;
		VertexCount++;
	};

	const swVector3 Axes[3] = { swVector3(1.0f, 0.0f, 0.0f), swVector3(0.0f, 1.0f, 0.0f), swVector3(0.0f, 0.0f, 1.0f) };

	for (int AxisIndex = 0; AxisIndex < 3; AxisIndex++)
	{
		AddVertex(swVector3(0.0f, 0.0f, 0.0f));
		AddVertex(Axes[AxisIndex] * ConeStart);
	}

	for (int AxisIndex = 0; AxisIndex < 3; AxisIndex++)
	{
		const swVector3& Side = Axes[(AxisIndex + 1) % 3];
		const swVector3& Up = Axes[(AxisIndex + 2) % 3];

		for (int Segment = 0; Segment < ConeSegments; Segment++)
		{
			const float Angle1 = Segment * 2.0f * SW_PI / ConeSegments;
			const float Angle2 = (Segment + 1) * 2.0f * SW_PI / ConeSegments;

			AddVertex(Axes[AxisIndex]);
			AddVertex(Axes[AxisIndex] * ConeStart + (Side * cosf(Angle1) + Up * sinf(Angle1)) * ConeRadius);
			AddVertex(Axes[AxisIndex] * ConeStart + (Side * cosf(Angle2) + Up * sinf(Angle2)) * ConeRadius);
		}
	}

	for (int AxisIndex = 0; AxisIndex < 3; AxisIndex++)
	{
		const swVector3& Side = Axes[(AxisIndex + 1) % 3];
		const swVector3& Up = Axes[(AxisIndex + 2) % 3];

		for (float Face = -1.0f; Face <= 1.0f; Face += 2.0f)
		{
			const swVector3 Normal = Axes[AxisIndex] * Face * BoxSize;
			const swVector3 Corners[4] = { Normal + (-Side - Up) * BoxSize, Normal + (Side - Up) * BoxSize, Normal + (Side + Up) * BoxSize, Normal + (-Side + Up) * BoxSize };

			AddVertex(Corners[0]);
			AddVertex(Corners[1]);
			AddVertex(Corners[2]);
			AddVertex(Corners[0]);
			AddVertex(Corners[2]);
			AddVertex(Corners[3]);
		}
	}

	const int BoxStart = 6 + 3 * ConeSegments * 3;

	const swPreferences& Preferences = swGetPreferences();
	const float HandleLength = GetCutawayHandleLength();
	const swMatrix44 WorldMatrix(swVector4(mCutawayAxes[0] * HandleLength, 0.0f), swVector4(mCutawayAxes[1] * HandleLength, 0.0f), swVector4(mCutawayAxes[2] * HandleLength, 0.0f), swVector4(mCutawayCenter, 1.0f));
	const int ActiveHandle = mCutawayDragHandle != -1 ? mCutawayDragHandle : mCutawayHoverHandle;
	const quint32 AxisColors[3] = { Preferences.mAxisXColor, Preferences.mAxisYColor, Preferences.mAxisZColor };

	mContext->SetLineWidth(2.0f * GetUIScale());
	mContext->SetWorldMatrix(WorldMatrix);
	mContext->SetVertexBufferPointer(Verts);
	mContext->SetVertexFormatPosition(3);

	for (int AxisIndex = 0; AxisIndex < 3; AxisIndex++)
	{
		mContext->SetColor(swVector4FromColor(ActiveHandle == AxisIndex ? Preferences.mObjectFocusedColor : AxisColors[AxisIndex]));
		mContext->DrawPrimitives(GL_LINES, AxisIndex * 2, 2);
		mContext->DrawPrimitives(GL_TRIANGLES, 6 + AxisIndex * ConeSegments * 3, ConeSegments * 3);
	}

	// the size knobs sit on the cutaway surface at the end of each axis
	for (int AxisIndex = 0; AxisIndex < 3; AxisIndex++)
	{
		const swVector3 Knob = mCutawayCenter + mCutawayAxes[AxisIndex] * mCutawaySize;

		mContext->SetWorldMatrix(swMatrix44(WorldMatrix[0], WorldMatrix[1], WorldMatrix[2], swVector4(Knob, 1.0f)));
		mContext->SetColor(swVector4FromColor(ActiveHandle == 4 + AxisIndex ? Preferences.mObjectFocusedColor : AxisColors[AxisIndex]));
		mContext->DrawPrimitives(GL_TRIANGLES, BoxStart, 36);
	}
	mContext->SetLineWidth(1.0f);
}

void swView::DrawSelectZoomRegionOverlay()
{
	mContext->SetMaterial(swMaterialType::UnlitColor);
	mContext->SetWorldMatrix(swMatrix44Identity());
	mContext->SetViewMatrix(swMatrix44Translation(swVector3(0.375, 0.375, 0.0)));
	mContext->SetProjectionMatrix(swMatrix44Orthographic(0.0f, mWidth, 0.0f, mHeight, -1.0f, 1.0f));
	mContext->SetLineWidth(1.0f);

	mContext->EnableDepthTest(false);

	float pt1x = (float)mMouseDownX;
	float pt1y = (float)mMouseDownY;
	float pt2x = (float)mMouseX;
	float pt2y = (float)mMouseY;

	float Left, Right, Bottom, Top;

	if (pt1x < pt2x)
	{
		Left = pt1x;
		Right = pt2x;
	}
	else
	{
		Left = pt2x;
		Right = pt1x;
	}

	if (pt1y < pt2y)
	{
		Bottom = pt1y;
		Top = pt2y;
	}
	else
	{
		Bottom = pt2y;
		Top = pt1y;
	}

	Left = swMaximum(Left, 0.0f);
	Right = swMinimum(Right, mWidth - 1.0f);
	Bottom = swMaximum(Bottom, 0.0f);
	Top = swMinimum(Top, mHeight - 1.0f);

	float BorderX = swMinimum(2.0f, Right - Left);
	float BorderY = swMinimum(2.0f, Top - Bottom);

	float Verts[14][2] =
	{
		{ Left, Bottom },
		{ Left + BorderX, Bottom + BorderY },
		{ Right, Bottom },
		{ Right - BorderX, Bottom + BorderY },
		{ Right, Top },
		{ Right - BorderX, Top - BorderY },
		{ Left, Top },
		{ Left + BorderX, Top - BorderY },
		{ Left, Bottom },
		{ Left + BorderX, Bottom + BorderY },
		{ Left + BorderX, Bottom + BorderY },
		{ Right - BorderX, Bottom + BorderY },
		{ Left + BorderX, Top - BorderY },
		{ Right - BorderX, Top - BorderY },
	};

	mContext->SetVertexBufferPointer(Verts);
	mContext->SetVertexFormatPosition(2);

	const swPreferences& Preferences = swGetPreferences();

	mContext->SetColor(swVector4FromColor(Preferences.mMarqueeBorderColor));
	mContext->DrawPrimitives(GL_TRIANGLE_STRIP, 0, 10);

	if (SW_RGBA_ALPHA(Preferences.mMarqueeFillColor))
	{
		mContext->EnableColorBlend(true);
		mContext->SetColor(swVector4FromColor(Preferences.mMarqueeFillColor));
		mContext->DrawPrimitives(GL_TRIANGLE_STRIP, 10, 4);
		mContext->EnableColorBlend(false);
	}

	mContext->EnableDepthTest(true);
}

void swView::DrawRotateViewOverlay()
{
	int x, y, w, h;

	x = 0;
	y = 0;
	w = mWidth;
	h = mHeight;

	mContext->SetMaterial(swMaterialType::UnlitColor);
	mContext->SetWorldMatrix(swMatrix44Identity());
	mContext->SetViewMatrix(swMatrix44Translation(swVector3(0.375, 0.375, 0.0)));
	mContext->SetProjectionMatrix(swMatrix44Orthographic(0, w, 0, h, -1, 1));
	mContext->SetLineWidth(1.0f);

	mContext->EnableDepthTest(false);
	mContext->SetColor(swVector4FromColor(swGetPreferences().mOverlayColor));

	float Verts[32 * 16 * 2];
	float* CurVert = Verts;

	float r = swMinimum(w, h) * 0.35f;
	float cx = x + w / 2.0f;
	float cy = y + h / 2.0f;

	for (int i = 0; i < 32; i++)
	{
		*CurVert++ = cosf((float)i / 32.0f * (2.0f * SW_PI)) * r + cx;
		*CurVert++ = sinf((float)i / 32.0f * (2.0f * SW_PI)) * r + cy;
	}

	const float OverlayCameraSquareSize = swMaximum(8.0f, (w + h) / 200.0f);

	*CurVert++ = cx + OverlayCameraSquareSize; *CurVert++ = cy + r + OverlayCameraSquareSize;
	*CurVert++ = cx - OverlayCameraSquareSize; *CurVert++ = cy + r + OverlayCameraSquareSize;
	*CurVert++ = cx - OverlayCameraSquareSize; *CurVert++ = cy + r - OverlayCameraSquareSize;
	*CurVert++ = cx + OverlayCameraSquareSize; *CurVert++ = cy + r - OverlayCameraSquareSize;
	*CurVert++ = cx + OverlayCameraSquareSize; *CurVert++ = cy - r + OverlayCameraSquareSize;
	*CurVert++ = cx - OverlayCameraSquareSize; *CurVert++ = cy - r + OverlayCameraSquareSize;
	*CurVert++ = cx - OverlayCameraSquareSize; *CurVert++ = cy - r - OverlayCameraSquareSize;
	*CurVert++ = cx + OverlayCameraSquareSize; *CurVert++ = cy - r - OverlayCameraSquareSize;
	*CurVert++ = cx + r + OverlayCameraSquareSize; *CurVert++ = cy + OverlayCameraSquareSize;
	*CurVert++ = cx + r - OverlayCameraSquareSize; *CurVert++ = cy + OverlayCameraSquareSize;
	*CurVert++ = cx + r - OverlayCameraSquareSize; *CurVert++ = cy - OverlayCameraSquareSize;
	*CurVert++ = cx + r + OverlayCameraSquareSize; *CurVert++ = cy - OverlayCameraSquareSize;
	*CurVert++ = cx - r + OverlayCameraSquareSize; *CurVert++ = cy + OverlayCameraSquareSize;
	*CurVert++ = cx - r - OverlayCameraSquareSize; *CurVert++ = cy + OverlayCameraSquareSize;
	*CurVert++ = cx - r - OverlayCameraSquareSize; *CurVert++ = cy - OverlayCameraSquareSize;
	*CurVert++ = cx - r + OverlayCameraSquareSize; *CurVert++ = cy - OverlayCameraSquareSize;

	mContext->SetVertexBufferPointer(Verts);
	mContext->SetVertexFormatPosition(2);

	GLushort Indices[64 + 32] =
	{
		0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15, 16, 16,
		17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 23, 24, 24, 25, 25, 26, 26, 27, 27, 28, 28, 29, 29, 30, 30, 31, 31, 0,
		32, 33, 33, 34, 34, 35, 35, 32, 36, 37, 37, 38, 38, 39, 39, 36,
		40, 41, 41, 42, 42, 43, 43, 40, 44, 45, 45, 46, 46, 47, 47, 44
	};

	mContext->SetIndexBufferPointer(Indices);
	mContext->DrawIndexedPrimitives(GL_LINES, 96, GL_UNSIGNED_SHORT, 0);

	mContext->EnableDepthTest(true);
}

void swView::DrawGrid()
{
	const swPreferences& Preferences = swGetPreferences();

	if (!Preferences.mDrawGridStuds && !Preferences.mDrawGridLines && !Preferences.mDrawGridOrigin && !Preferences.mDrawGridHeightXZ && !Preferences.mDrawGridHeightYZ)
		return;

	if (!Preferences.mGridEnabled)
		return;

	const int Spacing = swMaximum(Preferences.mGridLineSpacing, 1);
	int MinX, MaxX, MinY, MaxY;
	swVector3 Min(FLT_MAX, FLT_MAX, FLT_MAX), Max(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	bool GridSizeValid = mModel->GetVisiblePartsBoundingBox(Min, Max);
	bool DrawInsertPreview = std::any_of(mViews.begin(), mViews.end(), [](swView* View){ return View->mTrackTool == swTrackTool::Insert; });

	if (DrawInsertPreview)
	{
		const swModel* PreviewModel = GetActiveModel();

		for (const swPreviewInsertPartInformation& PreviewInsertPartInfo : PreviewModel->GetPreviewInsertPartInfo())
		{
			swVector3 Points[8];
			swGetBoxCorners(PreviewInsertPartInfo.PartData->GetBoundingBox(), Points);
			swMatrix44 PreviewTransform = PreviewInsertPartInfo.Transform;

			if (PreviewModel != mModel)
				PreviewTransform = swMultiply(PreviewTransform, mActiveSubmodelTransform);

			for (int i = 0; i < 8; i++)
			{
				swVector3 Point = swMultiply31(Points[i], PreviewTransform);

				Min = swMinimum(Point, Min);
				Max = swMaximum(Point, Max);
			}

			GridSizeValid = true;
		}
	}

	if (GridSizeValid && std::isfinite(Min[0]) && std::isfinite(Min[1]) && std::isfinite(Max[0]) && std::isfinite(Max[1]))
	{
		// Bound grid coordinates before converting to integers, keeping the vertex buffer below 1 MB.
		constexpr float MaxGridExtent = 10000.0f;
		const float GridSpacing = 20.0f * Spacing;

		MinX = static_cast<int>(floorf(swClamp(Min[0] / GridSpacing, -MaxGridExtent + 1.0f, MaxGridExtent - 1.0f))) - 1;
		MinY = static_cast<int>(floorf(swClamp(Min[1] / GridSpacing, -MaxGridExtent + 1.0f, MaxGridExtent - 1.0f))) - 1;
		MaxX = static_cast<int>(ceilf(swClamp(Max[0] / GridSpacing, -MaxGridExtent + 1.0f, MaxGridExtent - 1.0f))) + 1;
		MaxY = static_cast<int>(ceilf(swClamp(Max[1] / GridSpacing, -MaxGridExtent + 1.0f, MaxGridExtent - 1.0f))) + 1;

		MinX = swMinimum(MinX, -2);
		MinY = swMinimum(MinY, -2);
		MaxX = swMaximum(MaxX, 2);
		MaxY = swMaximum(MaxY, 2);
	}
	else
	{
		MinX = -2;
		MinY = -2;
		MaxX = 2;
		MaxY = 2;
	}

	if (!mGridBuffer.IsValid() || MinX != mGridSettings[0] || MinY != mGridSettings[1] || MaxX != mGridSettings[2] || MaxY != mGridSettings[3] ||
	    Spacing != mGridSettings[4] || (Preferences.mDrawGridStuds ? 1 : 0) != mGridSettings[5] || (Preferences.mDrawGridLines ? 1 : 0) != mGridSettings[6])
	{
		int VertexBufferSize = 0;

		if (Preferences.mDrawGridStuds)
			VertexBufferSize += 4 * 5 * sizeof(float);

		if (Preferences.mDrawGridLines)
			VertexBufferSize += 2 * (MaxX - MinX + MaxY - MinY + 2) * 3 * sizeof(float);

		float* Verts = nullptr;

		if (VertexBufferSize)
		{
			Verts = static_cast<float*>(malloc(VertexBufferSize));

			if (!Verts)
    			return;

            float* CurVert = Verts;

            if (Preferences.mDrawGridStuds)
            {
                float Left = MinX * 20.0f * Spacing;
                float Right = MaxX * 20.0f * Spacing;
                float Top = MinY * 20.0f * Spacing;
                float Bottom = MaxY * 20.0f * Spacing;
                float Z = 0;
                float U = (MaxX - MinX) * static_cast<float>(Spacing);
                float V = (MaxY - MinY) * static_cast<float>(Spacing);

                *CurVert++ = Left;
                *CurVert++ = Top;
                *CurVert++ = Z;
                *CurVert++ = 0.0f;
                *CurVert++ = V;

                *CurVert++ = Right;
                *CurVert++ = Top;
                *CurVert++ = Z;
                *CurVert++ = U;
                *CurVert++ = V;

                *CurVert++ = Left;
                *CurVert++ = Bottom;
                *CurVert++ = Z;
                *CurVert++ = 0.0f;
                *CurVert++ = 0.0f;

                *CurVert++ = Right;
                *CurVert++ = Bottom;
                *CurVert++ = Z;
                *CurVert++ = U;
                *CurVert++ = 0.0f;
            }

            if (Preferences.mDrawGridLines)
            {
                float LineSpacing = Spacing * 20.0f;

                for (int Step = MinX; Step < MaxX + 1; Step++)
                {
                    *CurVert++ = Step * LineSpacing;
                    *CurVert++ = MinY * LineSpacing;
                    *CurVert++ = 0.0f;
                    *CurVert++ = Step * LineSpacing;
                    *CurVert++ = MaxY * LineSpacing;
                    *CurVert++ = 0.0f;
                }

                for (int Step = MinY; Step < MaxY + 1; Step++)
                {
                    *CurVert++ = MinX * LineSpacing;
                    *CurVert++ = Step * LineSpacing;
                    *CurVert++ = 0.0f;
                    *CurVert++ = MaxX * LineSpacing;
                    *CurVert++ = Step * LineSpacing;
                    *CurVert++ = 0.0f;
                }
            }
		}

		mGridSettings[0] = MinX;
		mGridSettings[1] = MinY;
		mGridSettings[2] = MaxX;
		mGridSettings[3] = MaxY;
		mGridSettings[4] = Spacing;
		mGridSettings[5] = (Preferences.mDrawGridStuds ? 1 : 0);
		mGridSettings[6] = (Preferences.mDrawGridLines ? 1 : 0);

		mContext->DestroyVertexBuffer(mGridBuffer);
		mGridBuffer = mContext->CreateVertexBuffer(VertexBufferSize, Verts);
		free(Verts);
	}

	int BufferOffset = 0;
	mContext->SetVertexBuffer(mGridBuffer);
	mContext->SetWorldMatrix(swMatrix44Identity());

	if (Preferences.mDrawGridStuds)
	{
		mContext->BindTexture2D(gGridTexture);
		mContext->SetDepthWrite(false);
		mContext->EnableColorBlend(true);

		mContext->SetMaterial(swMaterialType::UnlitTextureModulate);
		mContext->SetColor(swVector4FromColor(Preferences.mGridStudColor));

		mContext->SetVertexFormat(0, 3, 0, 2, 0, false);
		mContext->DrawPrimitives(GL_TRIANGLE_STRIP, 0, 4);

		mContext->EnableColorBlend(false);
		mContext->SetDepthWrite(true);

		BufferOffset = 4 * 5 * sizeof(float);
	}

	if (Preferences.mDrawGridLines)
	{
		mContext->SetLineWidth(1.0f);
		mContext->SetMaterial(swMaterialType::UnlitColor);
		mContext->SetColor(swVector4FromColor(Preferences.mGridLineColor));

		int NumVerts = 2 * (MaxX - MinX + MaxY - MinY + 2);

		mContext->SetVertexFormat(BufferOffset, 3, 0, 0, 0, false);
		mContext->DrawPrimitives(GL_LINES, 0, NumVerts);
	}

	struct swGridVertex
	{
		float x, y, z;
		quint32 Color;
	};

	// z axis lines reach as high as the height grids
	constexpr float PlateHeight = 8.0f;
	const int PlateCount = swMaximum(static_cast<int>(ceilf((GridSizeValid && std::isfinite(Max[2]) ? Max[2] : 0.0f) / PlateHeight)) + 3, 9);
	const float Top = PlateCount * PlateHeight;
	const bool DrawGridHeight = Preferences.mDrawGridHeightXZ || Preferences.mDrawGridHeightYZ;
	std::vector<swGridVertex> AxisVerts;

	if (DrawGridHeight)
	{
		// Vertical grids through the origin axes, or behind the model, with a line every plate height.
		const float LineSpacing = 20.0f * Spacing;
		const float Left = MinX * LineSpacing;
		const float Right = MaxX * LineSpacing;
		const float Front = MinY * LineSpacing;
		const float Back = MaxY * LineSpacing;
		const swVector3 CameraPosition = mCamera->mPosition;
		const bool BehindModel = Preferences.mGridHeightBehindModel;

		std::vector<swVector3> Lines;

		if (Preferences.mDrawGridHeightXZ)
		{
			const float Y = !BehindModel ? 0.0f : CameraPosition.y > (Front + Back) * 0.5f ? Front : Back;

			for (int Step = MinX; Step <= MaxX; Step++)
			{
				Lines.emplace_back(Step * LineSpacing, Y, 0.0f);
				Lines.emplace_back(Step * LineSpacing, Y, Top);
			}

			for (int Plate = 1; Plate <= PlateCount; Plate++)
			{
				Lines.emplace_back(Left, Y, Plate * PlateHeight);
				Lines.emplace_back(Right, Y, Plate * PlateHeight);
			}

			if (Preferences.mDrawGridAxisZ)
			{
				AxisVerts.push_back({ 0.0f, Y, 0.0f, Preferences.mAxisZColor });
				AxisVerts.push_back({ 0.0f, Y, Top, Preferences.mAxisZColor });
			}
		}

		if (Preferences.mDrawGridHeightYZ)
		{
			const float X = !BehindModel ? 0.0f : CameraPosition.x > (Left + Right) * 0.5f ? Left : Right;

			for (int Step = MinY; Step <= MaxY; Step++)
			{
				Lines.emplace_back(X, Step * LineSpacing, 0.0f);
				Lines.emplace_back(X, Step * LineSpacing, Top);
			}

			for (int Plate = 1; Plate <= PlateCount; Plate++)
			{
				Lines.emplace_back(X, Front, Plate * PlateHeight);
				Lines.emplace_back(X, Back, Plate * PlateHeight);
			}

			// skip the z axis line the xz grid already drew at the origin
			if (Preferences.mDrawGridAxisZ && (BehindModel || !Preferences.mDrawGridHeightXZ))
			{
				AxisVerts.push_back({ X, 0.0f, 0.0f, Preferences.mAxisZColor });
				AxisVerts.push_back({ X, 0.0f, Top, Preferences.mAxisZColor });
			}
		}

		mContext->SetLineWidth(1.0f);
		mContext->SetMaterial(swMaterialType::UnlitColor);
		mContext->SetColor(swVector4FromColor(Preferences.mGridLineColor));
		mContext->SetVertexBufferPointer(Lines.data());
		mContext->SetVertexFormat(0, 3, 0, 0, 0, false);
		mContext->DrawPrimitives(GL_LINES, 0, static_cast<int>(Lines.size()));
	}

	if (Preferences.mDrawGridOrigin)
	{
		const float Scale = 20.0f * Spacing;

		if (Preferences.mDrawGridAxisY)
		{
			AxisVerts.push_back({ 0.0f, MinY * Scale, 0.0f, Preferences.mAxisYColor });
			AxisVerts.push_back({ 0.0f, MaxY * Scale, 0.0f, Preferences.mAxisYColor });
		}

		if (Preferences.mDrawGridAxisX)
		{
			AxisVerts.push_back({ MinX * Scale, 0.0f, 0.0f, Preferences.mAxisXColor });
			AxisVerts.push_back({ MaxX * Scale, 0.0f, 0.0f, Preferences.mAxisXColor });
		}

		// the height grids carry their own z axis lines
		if (Preferences.mDrawGridAxisZ && !DrawGridHeight)
		{
			AxisVerts.push_back({ 0.0f, 0.0f, 0.0f, Preferences.mAxisZColor });
			AxisVerts.push_back({ 0.0f, 0.0f, Top, Preferences.mAxisZColor });
		}
	}

	if (!AxisVerts.empty())
	{
		mContext->SetMaterial(swMaterialType::UnlitVertexColor);
		mContext->SetVertexBufferPointer(AxisVerts.data());
		mContext->SetVertexFormat(0, 3, 0, 0, 4, false);

		mContext->DrawPrimitives(GL_LINES, 0, static_cast<int>(AxisVerts.size()));
	}
}
