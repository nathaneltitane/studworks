#include "sw_global.h"
#include "sw_view.h"
#include "sw_view_widget.h"
#include "sw_window_main.h"
#include "sw_object_light.h"
#include "sw_object_camera.h"
#include "sw_object_part.h"
#include "sw_data_part.h"
#include "sw_track_train.h"
#include "sw_view_manipulator.h"
#include "sw_view_cube.h"
#include "sw_library.h"
#include "sw_window_utilities.h"

swVector3 swView::GetMoveDirection(const swVector3& Direction) const
{
	if (swGetPreferences().mFixedAxes)
		return Direction;

	// Match the axes used by MoveSelectedObjects/RotateSelectedObjects before
	// projecting them, so relative transforms do not rotate the mapping again.
	swMatrix33 LocalToWorld = GetActiveModel()->GetRelativeRotation();

	// Separate transforms always use each part's axes. Use the focused part
	// as the reference for the shared keyboard direction.
	if (gMainWindow->GetSeparateTransform())
	{
		swObject* Focus = GetActiveModel()->GetFocusObject();

		if (Focus && Focus->IsPart())
			LocalToWorld = static_cast<swPart*>(Focus)->GetRelativeRotation();
	}

	if (mActiveSubmodelInstance)
		LocalToWorld = swMultiply(LocalToWorld, swMatrix33(mActiveSubmodelTransform));

	const swMatrix33 LocalToView = swMultiply(LocalToWorld, swMatrix33(mCamera->mWorldView));

	// Choose distinct X/Y axes for screen right and up. Page Up/Down keep
	// their Z direction regardless of the camera, including bottom views.
	float BestScore = -std::numeric_limits<float>::infinity();
	float BestAway = -std::numeric_limits<float>::infinity();
	swVector3 BestRight(1.0f, 0.0f, 0.0f);
	swVector3 BestUp(0.0f, 1.0f, 0.0f);

	for (int RightAxis = 0; RightAxis < 2; RightAxis++)
	{
		for (int UpAxis = 0; UpAxis < 2; UpAxis++)
		{
			if (RightAxis == UpAxis)
				continue;

			for (int RightSign : { 1, -1 })
			{
				for (int UpSign : { 1, -1 })
				{
					swVector3 Right(0.0f, 0.0f, 0.0f);
					swVector3 Up(0.0f, 0.0f, 0.0f);
					Right[RightAxis] = float(RightSign);
					Up[UpAxis] = float(UpSign);

					const swVector3 ViewUp = swMultiply(Up, LocalToView);
					const float Score = swMultiply(Right, LocalToView).x + ViewUp.y;
					const float Away = -ViewUp.z;

					// In edge-on views, Up moves into the scene along the remaining
					// horizontal axis. Retain the first frame on diagonal ties.
					if (Score > BestScore + SW_EPSILON || (fabsf(Score - BestScore) <= SW_EPSILON && Away > BestAway + SW_EPSILON))
					{
						BestScore = Score;
						BestAway = Away;
						BestRight = Right;
						BestUp = Up;
					}
				}
			}
		}
	}

	// Default bindings are +X = Down, +Y = Right, +Z = Page Up.
	return BestRight * Direction.y - BestUp * Direction.x + swVector3(0.0f, 0.0f, Direction.z);
}

void swView::UpdatePartPreview()
{
	const swPartDataReference PreviewPartData(gMainWindow->GetInsertPartData());
	std::vector<swInsertPartInformation> InsertPartInfo;
	bool IsConnection;

	if (PreviewPartData)
	{
		std::tie(InsertPartInfo, IsConnection) = GetMouseInsertPartInfo(false, true, PreviewPartData, nullptr);
	}

	GetActiveModel()->SetPreviewInsertPartInfo(std::move(InsertPartInfo));
}

std::pair<std::vector<swInsertPartInformation>, bool> swView::GetMouseInsertPartInfo(bool IgnoreSelected, bool AllowNewParts, const swPartDataReference& PartData, swPart* MovingPart) const
{
	swModel* ActiveModel = GetActiveModel();

	swGetPartsLibrary()->EnsurePartDataAssetsReady(PartData);

	if (MovingPart && PartData->GetSynthInfo())
		swGetPartsLibrary()->WaitForSynthMeshes({ MovingPart });

	// The first part of an empty model goes to the origin when enabled.
	if (!MovingPart && swGetPreferences().mFirstPartAtOrigin && ActiveModel->GetParts().empty())
		return { { swInsertPartInformation{ PartData.Get(), swMatrix44Identity(), gMainWindow->GetInsertColorIndex() } }, false };

	swObjectRayTest ObjectRayTest = RayTest(true, IgnoreSelected);

	// Settling a hit's geometry can reveal another pending part behind it, so pick again until the hit is stable.
	while (ObjectRayTest.ObjectSection.Object)
	{
		swPart* MousePart = reinterpret_cast<swPart*>(ObjectRayTest.ObjectSection.Object);
		swPartData* MousePartData = MousePart->mPartData.Get();
		const bool Retest = MousePartData->mState != swAssetState::Loaded || MousePartData->IsModel() || MousePartData->IsProject() || MousePart->IsGeneratedMeshPending();

		swGetPartsLibrary()->EnsurePartDataAssetsReady(MousePart->mPartData);

		if (MousePartData->GetSynthInfo())
			swGetPartsLibrary()->WaitForSynthMeshes({ MousePart });

		if (!Retest)
			break;

		swObjectRayTest ReadyRayTest = RayTest(true, IgnoreSelected);
		const bool SamePart = ReadyRayTest.ObjectSection.Object == ObjectRayTest.ObjectSection.Object;
		ObjectRayTest = ReadyRayTest;

		if (SamePart)
			break;
	}

	const swBoundingBox& BoundingBox = MovingPart ? MovingPart->GetBoundingBox() : PartData->GetBoundingBox();

	if (ObjectRayTest.ObjectSection.Object)
	{
		swPart* MousePart = reinterpret_cast<swPart*>(ObjectRayTest.ObjectSection.Object);
		swTrainTrackInformation* RayTrackInfo = MousePart->mPartData->GetTrainTrackInfo();
		swTrainTrackInformation* InfoTrackInfo = PartData->GetTrainTrackInfo();

		if (RayTrackInfo && InfoTrackInfo)
		{
			ActiveModel->UpdateTrainTrackConnections(MousePart, IgnoreSelected);

			swVector3 ClosestPoint = ObjectRayTest.Start + swNormalize(ObjectRayTest.End - ObjectRayTest.Start) * ObjectRayTest.Distance;
			quint32 FocusSection = MousePart->GetFocusSection();

			std::vector<swInsertPartInformation> TrainTracks = swTrainTrackInformation::GetInsertPartInfo(MousePart, PartData.Get(), MovingPart, gMainWindow->GetInsertColorIndex(), FocusSection, AllowNewParts, ClosestPoint);

			if (!TrainTracks.empty())
				return { TrainTracks, true };
		}

		swVector3 Position = ObjectRayTest.PartRayTest.Plane;

		if (Position.x > 0.0f)
			Position.x += fabsf(BoundingBox.Min.x);
		else if (Position.x < 0.0f)
			Position.x -= fabsf(BoundingBox.Max.x);
		else if (Position.y > 0.0f)
			Position.y += fabsf(BoundingBox.Min.y);
		else if (Position.y < 0.0f)
			Position.y -= fabsf(BoundingBox.Max.y);
		else if (Position.z > 0.0f)
			Position.z += fabsf(BoundingBox.Min.z);
		else if (Position.z < 0.0f)
			Position.z -= fabsf(BoundingBox.Max.z);

		if (gMainWindow->GetRelativeTransform())
			Position = swMultiply31(ActiveModel->SnapPosition(Position), ObjectRayTest.PartRayTest.Transform);
		else
			Position = ActiveModel->SnapPosition(swMultiply31(Position, ObjectRayTest.PartRayTest.Transform));

		swMatrix44 WorldMatrix = ObjectRayTest.PartRayTest.Transform;
		WorldMatrix.SetTranslation(Position);

		return { { swInsertPartInformation{ PartData.Get(), WorldMatrix, gMainWindow->GetInsertColorIndex() } }, false };
	}

	std::array<swVector3, 2> ClickPoints = {{ swVector3((float)mMouseX, (float)mMouseY, 0.0f), swVector3((float)mMouseX, (float)mMouseY, 1.0f) }};
	UnprojectPoints(ClickPoints.data(), 2);

	if (ActiveModel != mModel)
	{
		swMatrix44 InverseMatrix = swMatrix44AffineInverse(mActiveSubmodelTransform);

		for (swVector3& Point : ClickPoints)
			Point = swMultiply31(Point, InverseMatrix);
	}

	swVector3 Intersection;

	if (swLineSegmentPlaneIntersection(&Intersection, ClickPoints[0], ClickPoints[1], swVector4(0, 0, 1, BoundingBox.Min.z)))
	{
		Intersection = ActiveModel->SnapPosition(Intersection);
		return { { swInsertPartInformation{ PartData.Get(), swMatrix44Translation(Intersection), gMainWindow->GetInsertColorIndex() } }, false };
	}

	swVector3 Position;

	if (!ActiveModel->GetFocusPosition(Position))
		Position = ActiveModel->GetSelectionOrModelCenter();

	swVector3 FrontVector(mCamera->mTargetPosition - mCamera->mPosition);

	if (swLineSegmentPlaneIntersection(&Intersection, ClickPoints[0], ClickPoints[1], swVector4(FrontVector, -swDot(FrontVector, Position))))
	{
		Intersection = ActiveModel->SnapPosition(Intersection);
		return { { swInsertPartInformation{ PartData.Get(), swMatrix44Translation(Intersection), gMainWindow->GetInsertColorIndex() } }, false };
	}

	return { { swInsertPartInformation{ PartData.Get(), swMatrix44Translation(UnprojectPoint(swVector3((float)mMouseX, (float)mMouseY, 0.9f))), gMainWindow->GetInsertColorIndex() } }, false };
}

swVector3 swView::GetCameraLightInsertPosition() const
{
	swModel* ActiveModel = GetActiveModel();

	std::array<swVector3, 2> ClickPoints = { { swVector3((float)mMouseX, (float)mMouseY, 0.0f), swVector3((float)mMouseX, (float)mMouseY, 1.0f) } };
	UnprojectPoints(ClickPoints.data(), 2);

	if (ActiveModel != mModel)
	{
		swMatrix44 InverseMatrix = swMatrix44AffineInverse(mActiveSubmodelTransform);

		for (swVector3& Point : ClickPoints)
			Point = swMultiply31(Point, InverseMatrix);
	}

	swVector3 Min, Max;
	swVector3 Center;

	if (ActiveModel->GetVisiblePartsBoundingBox(Min, Max))
		Center = (Min + Max) / 2.0f;
	else
		Center = swVector3(0.0f, 0.0f, 0.0f);

	return swRayPointClosestPoint(Center, ClickPoints[0], ClickPoints[1]);
}

void swView::GetRayUnderPointer(swVector3& Start, swVector3& End) const
{
	swVector3 StartEnd[2] =
	{
		swVector3((float)mMouseX, (float)mMouseY, 0.0f),
		swVector3((float)mMouseX, (float)mMouseY, 1.0f)
	};

	UnprojectPoints(StartEnd, 2);

	Start = StartEnd[0];
	End = StartEnd[1];
}

swObjectRayTest swView::RayTest(bool PartsOnly, bool IgnoreSelected) const
{
	swVector3 StartEnd[2] =
	{
		swVector3((float)mMouseX, (float)mMouseY, 0.0f),
		swVector3((float)mMouseX, (float)mMouseY, 1.0f)
	};

	UnprojectPoints(StartEnd, 2);

	swObjectRayTest ObjectRayTest;

	ObjectRayTest.PartsOnly = PartsOnly;
	ObjectRayTest.IgnoreSelected = IgnoreSelected;
	ObjectRayTest.ViewCamera = mCamera;
	ObjectRayTest.Start = StartEnd[0];
	ObjectRayTest.End = StartEnd[1];

	swModel* ActiveModel = GetActiveModel();

	if (ActiveModel != mModel)
	{
		swMatrix44 InverseMatrix = swMatrix44AffineInverse(mActiveSubmodelTransform);

		ObjectRayTest.Start = swMultiply31(ObjectRayTest.Start, InverseMatrix);
		ObjectRayTest.End = swMultiply31(ObjectRayTest.End, InverseMatrix);
	}

	ActiveModel->RayTest(ObjectRayTest);

	// a hit inside the cutaway is hidden, so test again from where the ray leaves it
	float Entry, Exit;

	if (ObjectRayTest.ObjectSection.Object && mCutawayShape != swCutawayShape::None && GetCutawayRange(StartEnd[0], StartEnd[1], Entry, Exit))
	{
		const float Hit = ObjectRayTest.Distance / swLength(ObjectRayTest.End - ObjectRayTest.Start);

		if (Hit > Entry && Hit < Exit && Exit < 1.0f)
		{
			swObjectRayTest ExitRayTest;

			ExitRayTest.PartsOnly = PartsOnly;
			ExitRayTest.IgnoreSelected = IgnoreSelected;
			ExitRayTest.ViewCamera = mCamera;
			ExitRayTest.Start = ObjectRayTest.Start + (ObjectRayTest.End - ObjectRayTest.Start) * Exit;
			ExitRayTest.End = ObjectRayTest.End;

			ActiveModel->RayTest(ExitRayTest);

			return ExitRayTest;
		}
	}

	return ObjectRayTest;
}

swObjectSection swView::FindObjectUnderPointer(bool PartsOnly, bool IgnoreSelected) const
{
	return RayTest(PartsOnly, IgnoreSelected).ObjectSection;
}

std::vector<swObject*> swView::FindObjectsInBox(float x1, float y1, float x2, float y2) const
{
	float Left, Top, Bottom, Right;

	if (x1 < x2)
	{
		Left = x1;
		Right = x2;
	}
	else
	{
		Left = x2;
		Right = x1;
	}

	if (y1 > y2)
	{
		Top = y1;
		Bottom = y2;
	}
	else
	{
		Top = y2;
		Bottom = y1;
	}

	std::array<swVector3, 6> Corners =
	{{
		swVector3(Left, Top, 0),
		swVector3(Left, Bottom, 0),
		swVector3(Right, Bottom, 0),
		swVector3(Right, Top, 0),
		swVector3(Left, Top, 1),
		swVector3(Right, Bottom, 1)
	}};

	UnprojectPoints(Corners.data(), (int)Corners.size());

	swModel* ActiveModel = GetActiveModel();

	if (ActiveModel != mModel)
	{
		swMatrix44 InverseMatrix = swMatrix44AffineInverse(mActiveSubmodelTransform);

		for (swVector3& Point : Corners)
			Point = swMultiply31(Point, InverseMatrix);
	}

	swVector3 PlaneNormals[6];
	PlaneNormals[0] = swNormalize(swCross(Corners[4] - Corners[0], Corners[1] - Corners[0])); // Left
	PlaneNormals[1] = swNormalize(swCross(Corners[5] - Corners[2], Corners[3] - Corners[2])); // Right
	PlaneNormals[2] = swNormalize(swCross(Corners[3] - Corners[0], Corners[4] - Corners[0])); // Top
	PlaneNormals[3] = swNormalize(swCross(Corners[1] - Corners[2], Corners[5] - Corners[2])); // Bottom
	PlaneNormals[4] = swNormalize(swCross(Corners[1] - Corners[0], Corners[3] - Corners[0])); // Front
	PlaneNormals[5] = swNormalize(swCross(Corners[1] - Corners[2], Corners[3] - Corners[2])); // Back

	swObjectBoxTest ObjectBoxTest;
	ObjectBoxTest.ViewCamera = mCamera;
	ObjectBoxTest.Planes[0] = swVector4(PlaneNormals[0], -swDot(PlaneNormals[0], Corners[0]));
	ObjectBoxTest.Planes[1] = swVector4(PlaneNormals[1], -swDot(PlaneNormals[1], Corners[5]));
	ObjectBoxTest.Planes[2] = swVector4(PlaneNormals[2], -swDot(PlaneNormals[2], Corners[0]));
	ObjectBoxTest.Planes[3] = swVector4(PlaneNormals[3], -swDot(PlaneNormals[3], Corners[5]));
	ObjectBoxTest.Planes[4] = swVector4(PlaneNormals[4], -swDot(PlaneNormals[4], Corners[0]));
	ObjectBoxTest.Planes[5] = swVector4(PlaneNormals[5], -swDot(PlaneNormals[5], Corners[5]));

	ActiveModel->BoxTest(ObjectBoxTest);

	return ObjectBoxTest.Objects;
}

swTrackTool swView::GetOverrideTrackTool(Qt::MouseButton Button) const
{
	if (mTrackToolFromOverlay)
		return swTrackTool::None;

	swTool OverrideTool = gMouseShortcuts.GetTool(Button, mMouseModifiers);

	if (OverrideTool == swTool::Count)
		return swTrackTool::None;

	constexpr swTrackTool TrackToolFromTool[] =
	{
	    swTrackTool::Insert,           // swTool::Insert
	    swTrackTool::PointLight,       // swTool::PointLight
		swTrackTool::SpotLight,        // swTool::SpotLight
		swTrackTool::DirectionalLight, // swTool::DirectionalLight
		swTrackTool::AreaLight,        // swTool::AreaLight
		swTrackTool::Camera,           // swTool::Camera
	    swTrackTool::Select,           // swTool::Select
	    swTrackTool::MoveXYZ,          // swTool::Move
	    swTrackTool::RotateTrackBall,  // swTool::Rotate
	    swTrackTool::Eraser,           // swTool::Eraser
	    swTrackTool::Paint,            // swTool::Paint
	    swTrackTool::ColorPicker,      // swTool::ColorPicker
	    swTrackTool::Zoom,             // swTool::Zoom
	    swTrackTool::Pan,              // swTool::Pan
	    swTrackTool::OrbitXY,          // swTool::RotateView
	    swTrackTool::Roll,             // swTool::Roll
	    swTrackTool::ZoomRegion        // swTool::ZoomRegion
	};

	SW_ARRAY_SIZE_CHECK(TrackToolFromTool, swTool::Count);

	return TrackToolFromTool[static_cast<int>(OverrideTool)];
}

float swView::GetUIScale() const
{
	return mWidget ? mWidget->GetDeviceScale() : 1.0f;
}

float swView::GetOverlayScale() const
{
	swVector3 OverlayCenter;
	swMatrix33 RelativeRotation;
	swModel* ActiveModel = GetActiveModel();
	ActiveModel->GetMoveRotateTransform(OverlayCenter, RelativeRotation);

	swMatrix44 WorldMatrix = swMatrix44(RelativeRotation, OverlayCenter);

	if (ActiveModel != mModel)
		WorldMatrix = swMultiply(WorldMatrix, mActiveSubmodelTransform);

	swVector3 ScreenPos = ProjectPoint(WorldMatrix.GetTranslation());
	ScreenPos[0] += 10.0f;
	swVector3 Point = UnprojectPoint(ScreenPos);

	swVector3 Dist(Point - WorldMatrix.GetTranslation());

	return Dist.Length() * 5.0f * GetUIScale();
}

void swView::BeginDrag(swDragState DragState)
{
	mDragState = DragState;
	UpdateTrackTool();
}

void swView::EndDrag(bool Accept)
{
	swModel* ActiveModel = GetActiveModel();

	if (Accept)
	{
		switch (mDragState)
		{
		case swDragState::None:
			break;

		case swDragState::Part:
			ActiveModel->InsertPartToolClicked();
			break;

		case swDragState::Color:
			ActiveModel->PaintToolClicked(FindObjectUnderPointer(true, false).Object);
			break;
		}
	}

	mDragState = swDragState::None;
	UpdateTrackTool();

	ActiveModel->SetPreviewInsertPartInfo(std::vector<swInsertPartInformation>());
	ActiveModel->UpdateAllViews();
}

swCursor swView::GetCursor() const
{
	// the cursor of the tool stays visible while a mouse button drags it
	if (mTrackTool == swTrackTool::Select)
	{
		if (mMouseModifiers & Qt::ControlModifier)
			return swCursor::SelectAdd;

		if (mMouseModifiers & Qt::ShiftModifier)
			return swCursor::SelectRemove;
	}

	constexpr swCursor CursorFromTrackTool[] =
	{
		swCursor::Select,           // swTrackTool::None
		swCursor::Brick,            // swTrackTool::Insert
		swCursor::PointLight,       // swTrackTool::PointLight
		swCursor::SpotLight,        // swTrackTool::SpotLight
		swCursor::DirectionalLight, // swTrackTool::DirectionalLight
		swCursor::AreaLight,        // swTrackTool::AreaLight
		swCursor::Camera,           // swTrackTool::Camera
		swCursor::Select,           // swTrackTool::Select
		swCursor::MoveAxisX,        // swTrackTool::MoveX
		swCursor::MoveAxisY,        // swTrackTool::MoveY
		swCursor::MoveAxisZ,        // swTrackTool::MoveZ
		swCursor::Move,             // swTrackTool::MoveXY
		swCursor::Move,             // swTrackTool::MoveXZ
		swCursor::Move,             // swTrackTool::MoveYZ
		swCursor::Move,             // swTrackTool::MoveXYZ
		swCursor::RotateAxisX,      // swTrackTool::RotateX
		swCursor::RotateAxisY,      // swTrackTool::RotateY
		swCursor::RotateAxisZ,      // swTrackTool::RotateZ
		swCursor::Rotate,           // swTrackTool::RotateTrackBall
		swCursor::Rotate,           // swTrackTool::RotateCamera
		swCursor::Select,           // swTrackTool::RotateTrainTrackRight
		swCursor::Select,           // swTrackTool::RotateTrainTrackLeft
		swCursor::Select,           // swTrackTool::InsertTrainTrack
		swCursor::Select,           // swTrackTool::SelectTrainTrack
		swCursor::Move,             // swTrackTool::ScalePlus
		swCursor::Move,             // swTrackTool::ScaleMinus
		swCursor::Delete,           // swTrackTool::Eraser
		swCursor::Paint,            // swTrackTool::Paint
		swCursor::ColorPicker,      // swTrackTool::ColorPicker
		swCursor::Zoom,             // swTrackTool::Zoom
		swCursor::Pan,              // swTrackTool::Pan
		swCursor::RotateX,          // swTrackTool::OrbitX
		swCursor::RotateY,          // swTrackTool::OrbitY
		swCursor::RotateView,       // swTrackTool::OrbitXY
		swCursor::Roll,             // swTrackTool::Roll
		swCursor::ZoomRegion        // swTrackTool::ZoomRegion
	};

	SW_ARRAY_SIZE_CHECK(CursorFromTrackTool, swTrackTool::Count);

	if (mTrackTool >= swTrackTool::None && mTrackTool < swTrackTool::Count)
		return CursorFromTrackTool[static_cast<int>(mTrackTool)];

	return swCursor::Select;
}

// Cursors draw the painted icons in white with a black outline so they stay visible on any background.
static QPixmap swCreateCursorPixmap(swPaintedIcon Icon, swPaintedIcon Badge)
{
	QImage Glyph(32, 32, QImage::Format_ARGB32_Premultiplied);
	Glyph.fill(Qt::transparent);

	QPainter Painter(&Glyph);

	// the arrow tip sits in the top left corner and leaves the bottom right for the badge
	if (Icon == swPaintedIcon::ActionSelect)
		Painter.drawImage(-2, -1, swCreatePaintedIconImage(Icon, 24, Qt::white));
	else
		Painter.drawImage(4, 4, swCreatePaintedIconImage(Icon, 24, Qt::white));

	if (Badge != swPaintedIcon::Count)
		Painter.drawImage(15, 15, swCreatePaintedIconImage(Badge, 16, Qt::white));

	Painter.end();

	QImage Image(32, 32, QImage::Format_ARGB32_Premultiplied);
	Image.fill(Qt::transparent);

	for (int Y = 0; Y < 32; Y++)
	{
		QRgb* Line = reinterpret_cast<QRgb*>(Image.scanLine(Y));

		for (int X = 0; X < 32; X++)
		{
			int Alpha = 0;

			for (int OffsetY = qMax(Y - 1, 0); OffsetY <= qMin(Y + 1, 31); OffsetY++)
				for (int OffsetX = qMax(X - 1, 0); OffsetX <= qMin(X + 1, 31); OffsetX++)
					Alpha = qMax(Alpha, qAlpha(Glyph.pixel(OffsetX, OffsetY)));

			Line[X] = qRgba(0, 0, 0, Alpha);
		}
	}

	Painter.begin(&Image);
	Painter.drawImage(0, 0, Glyph);
	Painter.end();

	return QPixmap::fromImage(Image);
}

void swView::SetCursor(swCursor CursorType)
{
	if (mCursor == CursorType)
		return;

	struct swCursorInformation
	{
		int x, y;
		swPaintedIcon Icon;
		swPaintedIcon Badge;
	};

	// count stands for no icon or no badge
	constexpr swCursorInformation Cursors[] =
	{
		{  0,  0, swPaintedIcon::Count, swPaintedIcon::Count },              // swCursor::Hidden
		{  0,  0, swPaintedIcon::Count, swPaintedIcon::Count },              // swCursor::Default
		{ 16, 16, swPaintedIcon::CursorBrick, swPaintedIcon::Count },        // swCursor::Brick
		{ 16, 16, swPaintedIcon::LightPoint, swPaintedIcon::Count },         // swCursor::PointLight
		{ 16, 16, swPaintedIcon::LightSpot, swPaintedIcon::Count },          // swCursor::SpotLight
		{ 16, 16, swPaintedIcon::LightDirectional, swPaintedIcon::Count },   // swCursor::DirectionalLight
		{ 16, 16, swPaintedIcon::LightArea, swPaintedIcon::Count },          // swCursor::AreaLight
		{ 16, 16, swPaintedIcon::ActionCamera, swPaintedIcon::Count },       // swCursor::Camera
		{  1,  1, swPaintedIcon::ActionSelect, swPaintedIcon::Count },       // swCursor::Select
		{  1,  1, swPaintedIcon::ActionSelect, swPaintedIcon::BadgeAdd },    // swCursor::SelectAdd
		{  1,  1, swPaintedIcon::ActionSelect, swPaintedIcon::BadgeRemove }, // swCursor::SelectRemove
		{ 16, 16, swPaintedIcon::CursorMove, swPaintedIcon::Count },         // swCursor::Move
		{ 16, 16, swPaintedIcon::CursorMoveAxisX, swPaintedIcon::Count },    // swCursor::MoveAxisX
		{ 16, 16, swPaintedIcon::CursorMoveAxisY, swPaintedIcon::Count },    // swCursor::MoveAxisY
		{ 16, 16, swPaintedIcon::CursorMoveAxisZ, swPaintedIcon::Count },    // swCursor::MoveAxisZ
		{ 16, 16, swPaintedIcon::ActionRotate, swPaintedIcon::Count },       // swCursor::Rotate
		{ 16, 16, swPaintedIcon::CursorRotateAxisX, swPaintedIcon::Count },  // swCursor::RotateAxisX
		{ 16, 16, swPaintedIcon::CursorRotateAxisY, swPaintedIcon::Count },  // swCursor::RotateAxisY
		{ 16, 16, swPaintedIcon::CursorRotateAxisZ, swPaintedIcon::Count },  // swCursor::RotateAxisZ
		{ 16, 16, swPaintedIcon::CursorRotateX, swPaintedIcon::Count },      // swCursor::RotateX
		{ 16, 16, swPaintedIcon::CursorRotateY, swPaintedIcon::Count },      // swCursor::RotateY
		{ 16, 16, swPaintedIcon::ActionDelete, swPaintedIcon::Count },       // swCursor::Delete
		{ 16, 25, swPaintedIcon::ActionPaint, swPaintedIcon::Count },        // swCursor::Paint
		{  8, 24, swPaintedIcon::ActionPickerColor, swPaintedIcon::Count },  // swCursor::ColorPicker
		{ 14, 14, swPaintedIcon::ActionZoom, swPaintedIcon::Count },         // swCursor::Zoom
		{ 16, 16, swPaintedIcon::ActionZoomRegion, swPaintedIcon::Count },   // swCursor::ZoomRegion
		{ 16, 16, swPaintedIcon::ActionPan, swPaintedIcon::Count },          // swCursor::Pan
		{ 16, 16, swPaintedIcon::ActionRoll, swPaintedIcon::Count },         // swCursor::Roll
		{ 16, 16, swPaintedIcon::ActionViewRotate, swPaintedIcon::Count },   // swCursor::RotateView
	};

	SW_ARRAY_SIZE_CHECK(Cursors, swCursor::Count);

	if (CursorType == swCursor::Hidden)
	{
		mWidget->setCursor(Qt::BlankCursor);
		mCursor = CursorType;
	}
	else if (CursorType == swCursor::Default)
	{
		mWidget->unsetCursor();
		mCursor = CursorType;
	}
	else if (CursorType >= swCursor::First && CursorType < swCursor::Count)
	{
		const swCursorInformation& Cursor = Cursors[static_cast<int>(CursorType)];
		mWidget->setCursor(QCursor(swCreateCursorPixmap(Cursor.Icon, Cursor.Badge), Cursor.x, Cursor.y));
		mCursor = CursorType;
	}
	else
	{
		mWidget->unsetCursor();
		mCursor = swCursor::Default;
	}
}

void swView::UpdateCursor()
{
	// Face picking keeps a crosshair until a face is clicked or picking is canceled.
	if (mPickingFaceNormal || mCutawayPickShape != swCutawayShape::None)
	{
		mWidget->setCursor(Qt::CrossCursor);
		return;
	}

	if (mCutawayHoverHandle != -1 || mCutawayDragHandle != -1)
	{
		mWidget->setCursor(Qt::SizeAllCursor);
		mCursor = swCursor::Count;
		return;
	}

	SetCursor(GetCursor());
}

void swView::StartFaceNormalPick()
{
	if (mViewType != swViewType::View || !mWidget)
		return;

	if (mTrackButton != swTrackButton::None)
		StopTracking(false);

	mCutawayPickShape = swCutawayShape::None;
	mPickingFaceNormal = true;
	UpdateCursor();

	gMainWindow->statusBar()->showMessage(tr("Click a face to view it straight on, or press Esc to cancel."));
}

void swView::StopFaceNormalPick()
{
	mPickingFaceNormal = false;
	mCursor = swCursor::Count;
	UpdateCursor();

	gMainWindow->statusBar()->clearMessage();
}

void swView::PickFaceNormal()
{
	const swObjectRayTest ObjectRayTest = RayTest(true, false);

	if (ObjectRayTest.ObjectSection.Object)
	{
		// The hit triangle normal is in the space of the innermost part, so bring it to the view.
		swVector3 Point = ObjectRayTest.Start + swNormalize(ObjectRayTest.End - ObjectRayTest.Start) * ObjectRayTest.Distance;
		swVector3 Normal = swNormalize(swMultiply30(ObjectRayTest.PartRayTest.Normal, ObjectRayTest.PartRayTest.Transform));

		if (swDot(Normal, ObjectRayTest.Start - Point) < 0.0f)
			Normal = -Normal;

		if (GetActiveModel() != mModel)
		{
			Point = swMultiply31(Point, mActiveSubmodelTransform);
			Normal = swNormalize(swMultiply30(Normal, mActiveSubmodelTransform));
		}

		const float Distance = swLength(mCamera->mPosition - mCamera->mTargetPosition);
		const swVector3 Up = fabsf(Normal.z) > 0.99f ? swVector3(0.0f, 1.0f, 0.0f) : swVector3(0.0f, 0.0f, 1.0f);

		SetViewpoint(Point + Normal * Distance, Point, Up);
	}

	StopFaceNormalPick();
}

swTool swView::GetCurrentTool() const
{
	constexpr swTool ToolFromTrackTool[] =
	{
		swTool::Select,           // swTrackTool::None
		swTool::Insert,           // swTrackTool::Insert
		swTool::PointLight,       // swTrackTool::PointLight
		swTool::SpotLight,        // swTrackTool::SpotLight
		swTool::DirectionalLight, // swTrackTool::DirectionalLight
		swTool::AreaLight,        // swTrackTool::AreaLight
		swTool::Camera,           // swTrackTool::Camera
		swTool::Select,           // swTrackTool::Select
		swTool::Move,             // swTrackTool::MoveX
		swTool::Move,             // swTrackTool::MoveY
		swTool::Move,             // swTrackTool::MoveZ
		swTool::Move,             // swTrackTool::MoveXY
		swTool::Move,             // swTrackTool::MoveXZ
		swTool::Move,             // swTrackTool::MoveYZ
		swTool::Move,             // swTrackTool::MoveXYZ
		swTool::Rotate,           // swTrackTool::RotateX
		swTool::Rotate,           // swTrackTool::RotateY
		swTool::Rotate,           // swTrackTool::RotateZ
		swTool::Rotate,           // swTrackTool::RotateTrackBall
		swTool::Rotate,           // swTrackTool::RotateCamera
		swTool::Rotate,           // swTrackTool::RotateTrainTrackRight
		swTool::Rotate,           // swTrackTool::RotateTrainTrackLeft
		swTool::Insert,           // swTrackTool::InsertTrainTrack
		swTool::Select,           // swTrackTool::SelectTrainTrack
		swTool::Move,             // swTrackTool::ScalePlus
		swTool::Move,             // swTrackTool::ScaleMinus
		swTool::Eraser,           // swTrackTool::Eraser
		swTool::Paint,            // swTrackTool::Paint
		swTool::ColorPicker,      // swTrackTool::ColorPicker
		swTool::Zoom,             // swTrackTool::Zoom
		swTool::Pan,              // swTrackTool::Pan
		swTool::RotateView,       // swTrackTool::OrbitX
		swTool::RotateView,       // swTrackTool::OrbitY
		swTool::RotateView,       // swTrackTool::OrbitXY
		swTool::Roll,             // swTrackTool::Roll
		swTool::ZoomRegion        // swTrackTool::ZoomRegion
	};

	SW_ARRAY_SIZE_CHECK(ToolFromTrackTool, swTrackTool::Count);

	if (mTrackTool >= swTrackTool::None && mTrackTool < swTrackTool::Count)
		return ToolFromTrackTool[static_cast<int>(mTrackTool)];

	return swTool::Select;
}

void swView::UpdateTrackTool()
{
	if (mViewType != swViewType::View)
	{
		mTrackTool = swTrackTool::None;
		UpdateCursor();
		return;
	}

	swTool CurrentTool = gMainWindow->GetTool();
	swTrackTool NewTrackTool = mTrackTool;
	quint32 NewTrackSection = ~0U;
	int x = mMouseX;
	int y = mMouseY;
	bool Redraw = false;
	mMouseDownPart = nullptr;
	mTrackToolFromOverlay = false;
	swModel* ActiveModel = GetActiveModel();

	switch (CurrentTool)
	{
	case swTool::Insert:
		NewTrackTool = swTrackTool::Insert;
		break;

	case swTool::PointLight:
		NewTrackTool = swTrackTool::PointLight;
		break;

	case swTool::SpotLight:
		NewTrackTool = swTrackTool::SpotLight;
		break;

	case swTool::DirectionalLight:
		NewTrackTool = swTrackTool::DirectionalLight;
		break;

	case swTool::AreaLight:
		NewTrackTool = swTrackTool::AreaLight;
		break;

	case swTool::Camera:
		NewTrackTool = swTrackTool::Camera;
		break;

	case swTool::Select:
	case swTool::Move:
		{
			std::tie(NewTrackTool, NewTrackSection) = mViewManipulator->UpdateSelectMove(mTrackButton);
			mTrackToolFromOverlay = NewTrackTool != swTrackTool::MoveXYZ && NewTrackTool != swTrackTool::Select;
			Redraw = NewTrackTool != mTrackTool || NewTrackSection != mTrackToolSection;

			if (CurrentTool == swTool::Select && NewTrackTool == swTrackTool::Select && mMouseModifiers == Qt::NoModifier)
			{
				swObjectSection ObjectSection = FindObjectUnderPointer(false, false);
				swObject* Object = ObjectSection.Object;

				if (Object && Object->IsPart() && ObjectSection.Section == SW_PART_SECTION_POSITION && Object->IsSelected())
				{
					swPart* Part = (swPart*)Object;
					mMouseDownTransform = Part->mModelWorld;
					mMouseDownPart = Part;
					NewTrackTool = swTrackTool::MoveXYZ;
				}
			}
		}
		break;

	case swTool::Rotate:
		{
			NewTrackTool = mViewManipulator->UpdateRotate();
			mTrackToolFromOverlay = NewTrackTool != swTrackTool::RotateTrackBall;
			Redraw = NewTrackTool != mTrackTool;
		}
		break;

	case swTool::Eraser:
		NewTrackTool = swTrackTool::Eraser;
		break;

	case swTool::Paint:
		NewTrackTool = swTrackTool::Paint;
		break;

	case swTool::ColorPicker:
		NewTrackTool = swTrackTool::ColorPicker;
		break;

	case swTool::Zoom:
		NewTrackTool = swTrackTool::Zoom;
		break;

	case swTool::Pan:
		NewTrackTool = swTrackTool::Pan;
		break;

	case swTool::RotateView:
		{
			int vx, vy, vw, vh;

			vx = 0;
			vy = 0;
			vw = mWidth;
			vh = mHeight;

			int cx = vx + vw / 2;
			int cy = vy + vh / 2;

			float d = sqrtf((float)((cx - x) * (cx - x) + (cy - y) * (cy - y)));
			float r = swMinimum(vw, vh) * 0.35f;

			const float SquareSize = swMaximum(8.0f, (vw + vh) / 200.0f);

			if ((d < r + SquareSize) && (d > r - SquareSize))
			{
				if ((cx - x < SquareSize) && (cx - x > -SquareSize))
				{
					NewTrackTool = swTrackTool::OrbitY;
					mTrackToolFromOverlay = true;
				}

				if ((cy - y < SquareSize) && (cy - y > -SquareSize))
				{
					NewTrackTool = swTrackTool::OrbitX;
					mTrackToolFromOverlay = true;
				}
			}
			else
			{
				if (d < r)
					NewTrackTool = swTrackTool::OrbitXY;
				else
					NewTrackTool = swTrackTool::Roll;
			}
		}
		break;

	case swTool::Roll:
		NewTrackTool = swTrackTool::Roll;
		break;

	case swTool::ZoomRegion:
		NewTrackTool = swTrackTool::ZoomRegion;
		break;

	case swTool::Count:
		break;
	}

	switch (mDragState)
	{
	case swDragState::None:
		break;

	case swDragState::Part:
		NewTrackTool = swTrackTool::Insert;
		Redraw = true;
		break;

	case swDragState::Color:
		NewTrackTool = swTrackTool::Paint;
		break;
	}

	mTrackTool = NewTrackTool;
	mTrackToolSection = NewTrackSection;

	UpdateCursor();

	if (Redraw)
		ActiveModel->UpdateAllViews();
}

void swView::StartOrbitTracking()
{
	mTrackTool = swTrackTool::OrbitXY;
	UpdateCursor();
	OnButtonDown(swTrackButton::Left);
}

void swView::StartPanGesture()
{
	swModel* ActiveModel = GetActiveModel();

	StartPan(mWidth / 2, mHeight / 2);
	ActiveModel->BeginMouseTool(swTool::Pan, this);
}

void swView::UpdatePanGesture(int dx, int dy)
{
	UpdatePan(mPanX + dx, mPanY + dy);
}

void swView::StartPan(int x, int y)
{
	mPanX = x;
	mPanY = y;
}

void swView::UpdatePan(int x, int y)
{
	if (x == mPanX && y == mPanY)
		return;

	swModel* ActiveModel = GetActiveModel();

	swVector3 Points[4] =
	{
	    swVector3((float)x, (float)y, 0.0f),
	    swVector3((float)x, (float)y, 1.0f),
	    swVector3(mPanX, mPanY, 0.0f),
	    swVector3(mPanX, mPanY, 1.0f)
	};

	UnprojectPoints(Points, 4);

	const swVector3& CurrentStart = Points[0];
	const swVector3& CurrentEnd = Points[1];
	const swVector3& MouseDownStart = Points[2];
	const swVector3& MouseDownEnd = Points[3];
	swVector3 Center = ActiveModel->GetSelectionOrModelCenter();

	swVector3 PlaneNormal(mCamera->mPosition - mCamera->mTargetPosition);
	swVector4 Plane(PlaneNormal, -swDot(PlaneNormal, Center));
	swVector3 Intersection, MoveStart;

	if (!swLineSegmentPlaneIntersection(&Intersection, CurrentStart, CurrentEnd, Plane) || !swLineSegmentPlaneIntersection(&MoveStart, MouseDownStart, MouseDownEnd, Plane))
	{
		Center = MouseDownStart + swNormalize(MouseDownEnd - MouseDownStart) * 10.0f;
		Plane = swVector4(PlaneNormal, -swDot(PlaneNormal, Center));

		if (!swLineSegmentPlaneIntersection(&Intersection, CurrentStart, CurrentEnd, Plane) || !swLineSegmentPlaneIntersection(&MoveStart, MouseDownStart, MouseDownEnd, Plane))
			return;
	}

	mPanX = x;
	mPanY = y;

	ActiveModel->UpdatePanTool(mCamera, MoveStart - Intersection);
}

void swView::EndPanGesture(bool Accept)
{
	swModel* ActiveModel = GetActiveModel();

	ActiveModel->EndMouseTool(swTool::Pan, this, Accept);
}

swVector3 swView::GetTrackballPoint(int x, int y) const
{
	const float PointX = ((float)x - mTrackballCenter[0]) / mTrackballRadius;
	const float PointY = ((float)y - mTrackballCenter[1]) / mTrackballRadius;
	const float LengthSquared = PointX * PointX + PointY * PointY;
	const float PointZ = LengthSquared <= 0.5f ? sqrtf(1.0f - LengthSquared) : 0.5f / sqrtf(LengthSquared);

	return swNormalize(swVector3(PointX, PointY, PointZ));
}

void swView::StartTracking(swTrackButton TrackButton)
{
	mTrackButton = TrackButton;
	mTrackUpdated = false;
	mMouseDownX = mMouseX;
	mMouseDownY = mMouseY;
	mCameraRotationAngle = 0.0f;
	swTool Tool = GetCurrentTool();
	swModel* ActiveModel = GetActiveModel();

	switch (Tool)
	{
		case swTool::Insert:
		case swTool::PointLight:
		case swTool::SpotLight:
		case swTool::DirectionalLight:
		case swTool::AreaLight:
		case swTool::Camera:
		case swTool::Select:
			break;

		case swTool::Move:
		case swTool::Rotate:
			ActiveModel->BeginMouseTool(Tool, this);

			if (mTrackTool == swTrackTool::RotateX || mTrackTool == swTrackTool::RotateY || mTrackTool == swTrackTool::RotateZ || mTrackTool == swTrackTool::RotateCamera)
			{
				swVector3 OverlayCenter;
				swMatrix33 RelativeRotation;

			    ActiveModel->GetMoveRotateTransform(OverlayCenter, RelativeRotation);

				swMatrix44 WorldMatrix = swMatrix44(RelativeRotation, OverlayCenter);

			    if (ActiveModel != mModel)
					WorldMatrix = swMultiply(WorldMatrix, mActiveSubmodelTransform);

				const swVector3 ScreenCenter = ProjectPoint(WorldMatrix.GetTranslation());

			    mCameraRotationMouseAngle = atan2f((float)mMouseY - ScreenCenter[1], (float)mMouseX - ScreenCenter[0]);
				mCameraRotationLastMouseAngle = mCameraRotationMouseAngle;
			}

			if (mTrackTool == swTrackTool::RotateTrackBall)
			{
				swVector3 OverlayCenter;
				swMatrix33 RelativeRotation;
				ActiveModel->GetMoveRotateTransform(OverlayCenter, RelativeRotation);

				swMatrix44 WorldMatrix = swMatrix44(RelativeRotation, OverlayCenter);
				if (ActiveModel != mModel)
					WorldMatrix = swMultiply(WorldMatrix, mActiveSubmodelTransform);

				mTrackballCenter = ProjectPoint(WorldMatrix.GetTranslation());
				const swMatrix44 CameraWorldMatrix = swMatrix44AffineInverse(mCamera->mWorldView);
				const swVector3 CameraRight = swNormalize(swMultiply30(swVector3(1.0f, 0.0f, 0.0f), CameraWorldMatrix));
				const swVector3 RadiusPoint = ProjectPoint(WorldMatrix.GetTranslation() + CameraRight * (2.0f * GetOverlayScale()));
				mTrackballRadius = swMaximum(1.0f, swLength(RadiusPoint - mTrackballCenter));
				mTrackballVector = GetTrackballPoint(mMouseX, mMouseY);
				mTrackballPendingRotation = swMatrix33Identity();
				mTrackballSnapping = gMainWindow->GetAngleSnap() != 0.0f;
				mTrackballHasPendingRotation = false;
			}
			break;

		case swTool::Eraser:
		case swTool::Paint:
		case swTool::ColorPicker:
			break;

	    case swTool::Pan:
		    StartPan(mMouseX, mMouseY);
			ActiveModel->BeginMouseTool(Tool, this);
		    break;

	    case swTool::Zoom:
		case swTool::RotateView:
		case swTool::Roll:
			ActiveModel->BeginMouseTool(Tool, this);
			break;

		case swTool::ZoomRegion:
			break;

		case swTool::Count:
			break;
	}

	UpdateCursor();
}

void swView::StopTracking(bool Accept)
{
	if (mTrackButton == swTrackButton::None)
		return;

	swTool Tool = GetCurrentTool();
	swModel* ActiveModel = GetActiveModel();

	switch (Tool)
	{
	case swTool::Insert:
	case swTool::PointLight:
	case swTool::SpotLight:
	case swTool::DirectionalLight:
	case swTool::AreaLight:
	case swTool::Camera:
		break;

	case swTool::Select:
		if (Accept && mMouseDownX != mMouseX && mMouseDownY != mMouseY)
		{
			std::vector<swObject*> Objects = FindObjectsInBox(mMouseDownX, mMouseDownY, mMouseX, mMouseY);

			if (mMouseModifiers & Qt::ControlModifier)
				ActiveModel->AddToSelectionAction(Objects);
			else if (mMouseModifiers & Qt::ShiftModifier)
				ActiveModel->RemoveFromSelectionAction(Objects);
			else
				ActiveModel->SetSelectionAndFocusAction(Objects, nullptr, 0, gMainWindow->GetSelectionMode());
		}
		break;

	case swTool::Move:
	case swTool::Rotate:
		ActiveModel->EndMouseTool(Tool, this, Accept);
		break;

	case swTool::Eraser:
	case swTool::Paint:
	case swTool::ColorPicker:
		break;

	case swTool::Zoom:
	case swTool::Pan:
	case swTool::RotateView:
	case swTool::Roll:
		ActiveModel->EndMouseTool(Tool, this, Accept);
		break;

	case swTool::ZoomRegion:
		{
			if (mMouseX == mMouseDownX || mMouseY == mMouseDownY)
				break;

			swVector3 Points[6] =
			{
				swVector3((mMouseDownX + swMinimum(mMouseX, mWidth - 1)) / 2, (mMouseDownY + swMinimum(mMouseY, mHeight - 1)) / 2, 0.0f),
				swVector3((mMouseDownX + swMinimum(mMouseX, mWidth - 1)) / 2, (mMouseDownY + swMinimum(mMouseY, mHeight - 1)) / 2, 1.0f),
				swVector3((float)mMouseX, (float)mMouseY, 0.0f),
				swVector3((float)mMouseX, (float)mMouseY, 1.0f),
				swVector3(mMouseDownX, mMouseDownY, 0.0f),
				swVector3(mMouseDownX, mMouseDownY, 1.0f)
			};

			UnprojectPoints(Points, 5);

			swVector3 Center = ActiveModel->GetSelectionOrModelCenter();

			swVector3 PlaneNormal(mCamera->mPosition - mCamera->mTargetPosition);
			swVector4 Plane(PlaneNormal, -swDot(PlaneNormal, Center));
			swVector3 Target, Corners[2];

			if (swLineSegmentPlaneIntersection(&Target, Points[0], Points[1], Plane) && swLineSegmentPlaneIntersection(&Corners[0], Points[2], Points[3], Plane) && swLineSegmentPlaneIntersection(&Corners[1], Points[3], Points[4], Plane))
			{
				float AspectRatio = (float)mWidth / (float)mHeight;
				ActiveModel->ZoomRegionToolClicked(this, AspectRatio, Points[0], Target, Corners);
			}
		}
		break;

	case swTool::Count:
		break;
	}

	mTrackButton = swTrackButton::None;
	UpdateTrackTool();
	ActiveModel->UpdateAllViews();
}

void swView::CancelTrackingOrClearSelectionAction()
{
	if (mPickingFaceNormal)
		StopFaceNormalPick();
	else if (mCutawayPickShape != swCutawayShape::None)
		StopCutawayPick();
	else if (mCutawayDragHandle != -1)
	{
		mCutawayCenter = mCutawayDragCenter;
		mCutawaySize = mCutawayDragSize;
		mCutawayDragHandle = -1;
		UpdateCursor();
		Redraw();
	}
	else if (mTrackButton != swTrackButton::None)
		StopTracking(false);
	else
	{
		swModel* ActiveModel = GetActiveModel();

		// with nothing selected esc leaves the cutaway
		if (HasCutaway() && (!ActiveModel || !ActiveModel->AnyObjectsSelected()))
			RemoveCutaway();
		else if (ActiveModel)
			ActiveModel->ClearSelectionAction();
	}
}

void swView::OnButtonDown(swTrackButton TrackButton)
{
	swModel* ActiveModel = GetActiveModel();
	mToolClicked = false;

	auto AddLight = [this, ActiveModel](swLightType LightType)
	{
		ActiveModel->InsertLightToolClicked(GetCameraLightInsertPosition(), LightType);

		if ((mMouseModifiers & Qt::ControlModifier) == 0)
			gMainWindow->SetTool(swTool::Select);

		mToolClicked = true;
		UpdateTrackTool();
	};

	switch (mTrackTool)
	{
	case swTrackTool::None:
		break;

	case swTrackTool::Insert:
		{
			UpdatePartPreview();

			if (!ActiveModel->InsertPartToolClicked())
				break;

			if ((mMouseModifiers & Qt::ControlModifier) == 0)
				gMainWindow->SetTool(swTool::Select);

			mToolClicked = true;
			UpdateTrackTool();
		}
		break;

	case swTrackTool::PointLight:
		AddLight(swLightType::Point);
		break;

	case swTrackTool::SpotLight:
		AddLight(swLightType::Spot);
		break;

	case swTrackTool::DirectionalLight:
		AddLight(swLightType::Directional);
		break;

	case swTrackTool::AreaLight:
		AddLight(swLightType::Area);
		break;

	case swTrackTool::Camera:
        {
            ActiveModel->InsertCameraToolClicked(GetCameraLightInsertPosition());

            if ((mMouseModifiers & Qt::ControlModifier) == 0)
                gMainWindow->SetTool(swTool::Select);

            mToolClicked = true;
            UpdateTrackTool();
        }
		break;

	case swTrackTool::Select:
		{
			swObjectSection ObjectSection = FindObjectUnderPointer(false, false);

			if (mMouseModifiers & Qt::ControlModifier)
				ActiveModel->FocusOrDeselectObjectAction(ObjectSection.Object, ObjectSection.Section, gMainWindow->GetSelectionMode());
			else if (mMouseModifiers & Qt::ShiftModifier)
		    {
			    if (ObjectSection.Object)
				    ActiveModel->RemoveFromSelectionAction({ ObjectSection.Object });
		    }
			else
				ActiveModel->SetSelectionAndFocusAction(std::vector<swObject*>(), ObjectSection.Object, ObjectSection.Section, gMainWindow->GetSelectionMode());

			StartTracking(TrackButton);
		}
		break;

	case swTrackTool::MoveX:
	case swTrackTool::MoveY:
	case swTrackTool::MoveZ:
	case swTrackTool::MoveXY:
	case swTrackTool::MoveXZ:
	case swTrackTool::MoveYZ:
	case swTrackTool::MoveXYZ:
		if (ActiveModel->AnyObjectsSelected())
			StartTracking(TrackButton);
		break;

	case swTrackTool::RotateX:
	case swTrackTool::RotateY:
	case swTrackTool::RotateZ:
	case swTrackTool::RotateTrackBall:
	case swTrackTool::RotateCamera:
		if (ActiveModel->CanRotateSelection())
			StartTracking(TrackButton);
		break;

	case swTrackTool::RotateTrainTrackRight:
	case swTrackTool::RotateTrainTrackLeft:
		{
			ActiveModel->RotateFocusedTrainTrack(mTrackTool == swTrackTool::RotateTrainTrackRight ? 1 : -1);

			mToolClicked = true;
			UpdateTrackTool();
		}
		break;

	case swTrackTool::InsertTrainTrack:
		{
			ShowTrainTrackPopup();

			mToolClicked = true;
			UpdateTrackTool();
		}
		break;

	case swTrackTool::SelectTrainTrack:
		{
			swObject* Focus = ActiveModel->GetFocusObject();

			if (!Focus || !Focus->IsPart())
				break;

			swPart* Part = (swPart*)Focus;
			const swTrainTrackInformation* TrainTrackInfo = Part->mPartData->GetTrainTrackInfo();

			if (!TrainTrackInfo)
				break;

			if (mTrackToolSection < SW_PART_SECTION_TRAIN_TRACK_CONNECTION_FIRST)
				break;

			swObjectSection ObjectSection;

			ObjectSection.Object = Focus;
			ObjectSection.Section = mTrackToolSection;

			ActiveModel->SetSelectionAndFocusAction(std::vector<swObject*>(), ObjectSection.Object, ObjectSection.Section, gMainWindow->GetSelectionMode());
		}
		break;

	case swTrackTool::ScalePlus:
	case swTrackTool::ScaleMinus:
		if (ActiveModel->AnyPartsSelected())
			StartTracking(TrackButton);
		break;

	case swTrackTool::Eraser:
		ActiveModel->EraserToolClicked(FindObjectUnderPointer(false, false).Object);
		mToolClicked = true;
		break;

	case swTrackTool::Paint:
		ActiveModel->PaintToolClicked(FindObjectUnderPointer(true, false).Object);
		StartTracking(TrackButton);
		mToolClicked = true;
		break;

	case swTrackTool::ColorPicker:
		ActiveModel->ColorPickerToolClicked(FindObjectUnderPointer(true, false).Object);
		mToolClicked = true;
		break;

	case swTrackTool::Zoom:
	case swTrackTool::Pan:
	case swTrackTool::OrbitX:
	case swTrackTool::OrbitY:
	case swTrackTool::OrbitXY:
	case swTrackTool::Roll:
	case swTrackTool::ZoomRegion:
		StartTracking(TrackButton);
		break;

	case swTrackTool::Count:
		break;
	}
}

void swView::OnLeftButtonDown()
{
	if (mPickingFaceNormal)
	{
		PickFaceNormal();
		return;
	}

	if (mCutawayPickShape != swCutawayShape::None)
	{
		PickCutaway();
		return;
	}

	if (mTrackButton != swTrackButton::None)
	{
		StopTracking(false);
		return;
	}

	if (mViewCube->OnLeftButtonDown())
		return;

	mCutawayDragHandle = FindCutawayHandle();

	if (mCutawayDragHandle != -1)
	{
		mCutawayDragCenter = mCutawayCenter;
		mCutawayDragSize = mCutawaySize;
		mCutawayDragMouseX = mMouseX;
		mCutawayDragMouseY = mMouseY;
		UpdateCursor();
		return;
	}

	// pick the tool with the modifiers held at the press
	UpdateTrackTool();

	swTrackTool OverrideTool = GetOverrideTrackTool(Qt::LeftButton);

	if (OverrideTool != swTrackTool::None)
	{
		mTrackTool = OverrideTool;
		UpdateCursor();
	}

	OnButtonDown(swTrackButton::Left);
}

void swView::OnLeftButtonUp()
{
	if (mCutawayDragHandle != -1)
	{
		mCutawayDragHandle = -1;
		UpdateCursor();
		return;
	}

	StopTracking(mTrackButton == swTrackButton::Left);

	if (mViewCube->OnLeftButtonUp())
		return;
}

void swView::OnLeftButtonDoubleClick()
{
	if (mViewType != swViewType::View)
	{
		ZoomExtents(swGeometryBoundsMode::IncludeFallback, false);
		return;
	}

	swModel* ActiveModel = GetActiveModel();

	if (mTrackTool == swTrackTool::RotateTrainTrackRight || mTrackTool == swTrackTool::RotateTrainTrackLeft)
	{
		ActiveModel->RotateFocusedTrainTrack(mTrackTool == swTrackTool::RotateTrainTrackRight ? 1 : -1);

		mToolClicked = true;
		UpdateTrackTool();

		return;
	}
}

void swView::OnMiddleButtonDown()
{
	if (mTrackButton != swTrackButton::None)
	{
		StopTracking(false);
		return;
	}

	swTrackTool OverrideTool = GetOverrideTrackTool(Qt::MiddleButton);

	if (OverrideTool != swTrackTool::None)
	{
		mTrackTool = OverrideTool;
		UpdateCursor();
	}

	OnButtonDown(swTrackButton::Middle);
}

void swView::OnMiddleButtonUp()
{
	StopTracking(mTrackButton == swTrackButton::Middle);
}

void swView::OnRightButtonDown()
{
	if (mTrackButton != swTrackButton::None)
	{
		StopTracking(false);
		return;
	}

	swTrackTool OverrideTool = GetOverrideTrackTool(Qt::RightButton);

	if (OverrideTool != swTrackTool::None)
	{
		mTrackTool = OverrideTool;
		UpdateCursor();
	}

	OnButtonDown(swTrackButton::Right);
}

void swView::OnRightButtonUp()
{
	bool ShowMenu = !mToolClicked && (mTrackButton == swTrackButton::None || !mTrackUpdated);

	if (mTrackButton != swTrackButton::None)
		StopTracking(mTrackButton == swTrackButton::Right);

	if (ShowMenu)
		ShowContextMenu();
}

void swView::OnBackButtonDown()
{
}

void swView::OnBackButtonUp()
{
	gMainWindow->HandleCommand(SW_VIEW_TIME_PREVIOUS);
}

void swView::OnForwardButtonDown()
{
}

void swView::OnForwardButtonUp()
{
	gMainWindow->HandleCommand(SW_VIEW_TIME_NEXT);
}

void swView::OnMouseMove()
{
	swModel* ActiveModel = GetActiveModel();

	if (!ActiveModel)
		return;

	if (mCutawayDragHandle != -1)
	{
		UpdateCutawayDrag();
		return;
	}

	if (mTrackButton == swTrackButton::None)
	{
		const int CutawayHandle = FindCutawayHandle();

		if (CutawayHandle != mCutawayHoverHandle)
		{
			mCutawayHoverHandle = CutawayHandle;
			UpdateCursor();
			Redraw();
		}

		if (CutawayHandle != -1)
			return;

		if (mViewCube->OnMouseMove())
		{
			swTrackTool NewTrackTool = mViewCube->IsDragging() ? swTrackTool::OrbitXY : swTrackTool::None;

			if (NewTrackTool != mTrackTool)
			{
				mTrackTool = NewTrackTool;
				UpdateCursor();
			}

			return;
		}

		UpdateTrackTool();

		if (mTrackTool == swTrackTool::Insert)
		{
			UpdatePartPreview();
			ActiveModel->UpdateAllViews();
		}

		return;
	}

	mTrackUpdated = true;
	const float MouseSensitivity = 0.5f / (21.0f - swGetPreferences().mMouseSensitivity);

	switch (mTrackTool)
	{
	case swTrackTool::None:
	case swTrackTool::Insert:
	case swTrackTool::PointLight:
	case swTrackTool::SpotLight:
	case swTrackTool::DirectionalLight:
	case swTrackTool::AreaLight:
	case swTrackTool::Camera:
		break;

	case swTrackTool::Select:
		Redraw();
		break;

	case swTrackTool::MoveX:
	case swTrackTool::MoveY:
	case swTrackTool::MoveZ:
	case swTrackTool::MoveXY:
	case swTrackTool::MoveXZ:
	case swTrackTool::MoveYZ:
	case swTrackTool::MoveXYZ:
	case swTrackTool::ScalePlus:
	case swTrackTool::ScaleMinus:
	{
			swVector3 Points[4] =
			{
				swVector3((float)mMouseX, (float)mMouseY, 0.0f),
				swVector3((float)mMouseX, (float)mMouseY, 1.0f),
				swVector3(mMouseDownX, mMouseDownY, 0.0f),
				swVector3(mMouseDownX, mMouseDownY, 1.0f)
			};

			UnprojectPoints(Points, 4);

			const swVector3& CurrentStart = Points[0];
			const swVector3& CurrentEnd = Points[1];
			const swVector3& MouseDownStart = Points[2];
			const swVector3& MouseDownEnd = Points[3];

			swVector3 OverlayCenter;
			swMatrix33 RelativeRotation;
			ActiveModel->GetMoveRotateTransform(OverlayCenter, RelativeRotation);

			swMatrix44 WorldMatrix = swMatrix44(RelativeRotation, OverlayCenter);

			if (ActiveModel != mModel)
				WorldMatrix = swMultiply(WorldMatrix, mActiveSubmodelTransform);

			const swVector3 Center = WorldMatrix.GetTranslation();

			if (mTrackTool == swTrackTool::MoveX || mTrackTool == swTrackTool::MoveY || mTrackTool == swTrackTool::MoveZ)
			{
				swVector3 Direction;
				if (mTrackTool == swTrackTool::MoveX)
					Direction = swVector3(1.0f, 0.0f, 0.0f);
				else if (mTrackTool == swTrackTool::MoveY)
					Direction = swVector3(0.0f, 1.0f, 0.0f);
				else
					Direction = swVector3(0.0f, 0.0f, 1.0f);

				Direction = swMultiply30(Direction, WorldMatrix);

				swVector3 Intersection;
				swClosestPointsBetweenLines(Center, Center + Direction, CurrentStart, CurrentEnd, &Intersection, nullptr);

				swVector3 MoveStart;
				swClosestPointsBetweenLines(Center, Center + Direction, MouseDownStart, MouseDownEnd, &MoveStart, nullptr);

				swVector3 Distance = Intersection - MoveStart;
				Distance = swMultiply(Distance, swMatrix33AffineInverse(swMatrix33(WorldMatrix)));
				ActiveModel->UpdateMoveTool(Distance, true, mTrackButton != swTrackButton::Left);
			}
			else if (mTrackTool == swTrackTool::MoveXY || mTrackTool == swTrackTool::MoveXZ || mTrackTool == swTrackTool::MoveYZ)
			{
				swVector3 PlaneNormal;

				if (mTrackTool == swTrackTool::MoveXY)
					PlaneNormal = swVector3(0.0f, 0.0f, 1.0f);
				else if (mTrackTool == swTrackTool::MoveXZ)
					PlaneNormal = swVector3(0.0f, 1.0f, 0.0f);
				else
					PlaneNormal = swVector3(1.0f, 0.0f, 0.0f);

				PlaneNormal = swMultiply30(PlaneNormal, WorldMatrix);
				swVector4 Plane(PlaneNormal, -swDot(PlaneNormal, Center));
				swVector3 Intersection;

				if (swLineSegmentPlaneIntersection(&Intersection, CurrentStart, CurrentEnd, Plane))
				{
					swVector3 MoveStart;

					if (swLineSegmentPlaneIntersection(&MoveStart, MouseDownStart, MouseDownEnd, Plane))
					{
						swVector3 Distance = Intersection - MoveStart;
						Distance = swMultiply(Distance, swMatrix33AffineInverse(swMatrix33(WorldMatrix)));
						ActiveModel->UpdateMoveTool(Distance, true, mTrackButton != swTrackButton::Left);
					}
				}
			}
			else if (mTrackTool == swTrackTool::MoveXYZ && mMouseDownPart)
			{
				if (mMouseDownPart->mPartData->GetTrainTrackInfo())
					ActiveModel->UpdateSelectedPartsTrainTrackConnections();

				auto [PartInfoTransforms, IsConnection] = GetMouseInsertPartInfo(true, false, mMouseDownPart->mPartData, mMouseDownPart);

				if (!PartInfoTransforms.empty())
				{
					ActiveModel->UpdateFreeMoveTool(mMouseDownPart, mMouseDownTransform, PartInfoTransforms.back().Transform, IsConnection, mTrackButton != swTrackButton::Left);
				}
			}
			else if (mTrackTool == swTrackTool::ScalePlus || mTrackTool == swTrackTool::ScaleMinus)
			{
				swVector3 Direction;
				if (mTrackTool == swTrackTool::ScalePlus)
					Direction = swVector3(1.0f, 0.0f, 0.0f);
				else
					Direction = swVector3(-1.0f, 0.0f, 0.0f);

				Direction = swMultiply30(Direction, WorldMatrix);

				swVector3 Intersection;
				swClosestPointsBetweenLines(Center, Center + Direction, CurrentStart, CurrentEnd, &Intersection, nullptr);

				swObject* Focus = ActiveModel->GetFocusObject();
				if (Focus && Focus->IsPart())
				{
					swPart* Part = (swPart*)Focus;
					quint32 Section = Part->GetFocusSection();

					if (Section >= SW_PART_SECTION_CONTROL_POINT_FIRST && Section <= SW_PART_SECTION_CONTROL_POINT_LAST)
					{
						const float ScaleMax = 200.0f;
						const float OverlayScale = GetOverlayScale();
						const float ScaleStart = 2.0f * OverlayScale;

						swVector3 Position = Part->GetSectionPosition(Section);
						swVector3 Start = Position + Direction * ScaleStart;

						float Distance = swLength(Intersection - Start);
						if (swDot(Direction, Intersection - Start) < 0.0f)
							Distance = 0.0f;

						float Scale = swClamp(Distance, 0.1f, ScaleMax);

						ActiveModel->UpdateScaleTool(Scale);
					}
				}
			}
			else
			{
				swVector3 PlaneNormal = swNormalize(mCamera->mTargetPosition - mCamera->mPosition);
				swVector4 Plane(PlaneNormal, -swDot(PlaneNormal, Center));
				swVector3 Intersection;

				if (swLineSegmentPlaneIntersection(&Intersection, CurrentStart, CurrentEnd, Plane))
				{
					swVector3 MoveStart;

					if (swLineSegmentPlaneIntersection(&MoveStart, MouseDownStart, MouseDownEnd, Plane))
					{
						swVector3 Distance = Intersection - MoveStart;
						ActiveModel->UpdateMoveTool(Distance, true, mTrackButton != swTrackButton::Left);
					}
				}
			}
		}
		break;

	case swTrackTool::RotateX:
	case swTrackTool::RotateY:
	case swTrackTool::RotateZ:
		{
			swVector3 OverlayCenter;
			swMatrix33 RelativeRotation;
			ActiveModel->GetMoveRotateTransform(OverlayCenter, RelativeRotation);

			swMatrix44 WorldMatrix = swMatrix44(RelativeRotation, OverlayCenter);
			if (ActiveModel != mModel)
				WorldMatrix = swMultiply(WorldMatrix, mActiveSubmodelTransform);

			const swVector3 ScreenCenter = ProjectPoint(WorldMatrix.GetTranslation());
			const float MouseAngle = atan2f((float)mMouseY - ScreenCenter[1], (float)mMouseX - ScreenCenter[0]);
			float Angle = mCameraRotationLastMouseAngle - MouseAngle;

			if (Angle > SW_PI)
				Angle -= SW_TWO_PI;
			else if (Angle < -SW_PI)
				Angle += SW_TWO_PI;

			mCameraRotationAngle += Angle;
			mCameraRotationLastMouseAngle = MouseAngle;

			swVector3 Axis;

			switch (mTrackTool)
			{
			case swTrackTool::RotateX:
				Axis = swVector3(1.0f, 0.0f, 0.0f);
				break;
			case swTrackTool::RotateY:
				Axis = swVector3(0.0f, 1.0f, 0.0f);
				break;
			case swTrackTool::RotateZ:
				Axis = swVector3(0.0f, 0.0f, 1.0f);
				break;
			default:
				Axis = swVector3(0.0f, 0.0f, 1.0f);
				break;
			}

			// The rings rotate with the current gizmo frame, so derive the displayed
			// axis from the current transform before determining its screen direction.
			swVector3 DisplayAxis = swNormalize(swMultiply30(Axis, WorldMatrix));
			if (WorldMatrix.Determinant() < 0.0f)
				DisplayAxis = -DisplayAxis;

			const swVector3 CameraAxis = swNormalize(mCamera->mTargetPosition - mCamera->mPosition);
			const float AxisDirection = swDot(DisplayAxis, CameraAxis) < 0.0f ? -1.0f : 1.0f;
			ActiveModel->UpdateRotateTool(Axis, AxisDirection * mCameraRotationAngle * SW_RADIANS_TO_DEGREES, mTrackButton != swTrackButton::Left, true);
		}
		break;

	case swTrackTool::RotateTrackBall:
		{
			const swVector3 CurrentVector = GetTrackballPoint(mMouseX, mMouseY);
			const swVector3 LocalAxis = swCross(mTrackballVector, CurrentVector);
			const float AxisLength = swLength(LocalAxis);

			if (AxisLength > 0.0001f)
			{
				const float Angle = atan2f(AxisLength, swClamp(swDot(mTrackballVector, CurrentVector), -1.0f, 1.0f));
				const swMatrix44 CameraWorldMatrix = swMatrix44AffineInverse(mCamera->mWorldView);
				swVector3 RotationAxis = swNormalize(swMultiply30(LocalAxis, CameraWorldMatrix));
				if (ActiveModel != mModel)
				{
					const swMatrix44 ActiveSubmodelInverse = swMatrix44AffineInverse(mActiveSubmodelTransform);
					RotationAxis = swNormalize(swMultiply30(RotationAxis, ActiveSubmodelInverse));

					if (ActiveSubmodelInverse.Determinant() < 0.0f)
						RotationAxis = -RotationAxis;
				}

				const swMatrix33 IncrementalRotation = swMatrix33FromAxisAngle(RotationAxis, Angle);

				const bool Snapping = gMainWindow->GetAngleSnap() != 0.0f;
				if (Snapping != mTrackballSnapping)
				{
					if (!Snapping && mTrackballHasPendingRotation)
						ActiveModel->UpdateRotateTool(mTrackballPendingRotation, mTrackButton != swTrackButton::Left, false);

					mTrackballPendingRotation = swMatrix33Identity();
					mTrackballSnapping = Snapping;
					mTrackballHasPendingRotation = false;
				}

				if (!Snapping)
				{
					ActiveModel->UpdateRotateTool(IncrementalRotation, mTrackButton != swTrackButton::Left, false);
				}
				else
				{
					mTrackballPendingRotation = swMultiply(mTrackballPendingRotation, IncrementalRotation);
					mTrackballHasPendingRotation = true;
					const swVector4 AxisAngle = swMatrix44ToAxisAngle(swMatrix44(mTrackballPendingRotation, swVector3(0.0f, 0.0f, 0.0f)));
					const float SnappedAngle = ActiveModel->SnapRotation(swVector3(AxisAngle[3] * SW_RADIANS_TO_DEGREES, 0.0f, 0.0f))[0];

					if (SnappedAngle != 0.0f)
					{
						const swMatrix33 SnappedRotation = swMatrix33FromAxisAngle(swVector3(AxisAngle), SnappedAngle * SW_DEGREES_TO_RADIANS);
						ActiveModel->UpdateRotateTool(SnappedRotation, mTrackButton != swTrackButton::Left, false);
						mTrackballPendingRotation = swMultiply(swMatrix33AffineInverse(SnappedRotation), mTrackballPendingRotation);
						mTrackballHasPendingRotation = swMatrix44ToAxisAngle(swMatrix44(mTrackballPendingRotation, swVector3(0.0f, 0.0f, 0.0f)))[3] > 0.000001f;
					}
				}
			}

			mTrackballVector = CurrentVector;
		}
		break;

	case swTrackTool::RotateCamera:
		{
			swVector3 OverlayCenter;
			swMatrix33 RelativeRotation;
			ActiveModel->GetMoveRotateTransform(OverlayCenter, RelativeRotation);

			swMatrix44 WorldMatrix = swMatrix44(RelativeRotation, OverlayCenter);
			if (ActiveModel != mModel)
				WorldMatrix = swMultiply(WorldMatrix, mActiveSubmodelTransform);

			const swVector3 ScreenCenter = ProjectPoint(WorldMatrix.GetTranslation());
			const float MouseAngle = atan2f((float)mMouseY - ScreenCenter[1], (float)mMouseX - ScreenCenter[0]);
			// Unwrap each clockwise-positive pointer delta so full turns do not flip
			// when the pointer crosses the opposite side of the ring.
			float Angle = mCameraRotationLastMouseAngle - MouseAngle;

			if (Angle > SW_PI)
				Angle -= SW_TWO_PI;
			else if (Angle < -SW_PI)
				Angle += SW_TWO_PI;

			mCameraRotationAngle += Angle;
			mCameraRotationLastMouseAngle = MouseAngle;

			swVector3 CameraAxis = swNormalize(mCamera->mTargetPosition - mCamera->mPosition);
			if (ActiveModel != mModel)
			{
				const swMatrix44 ActiveSubmodelInverse = swMatrix44AffineInverse(mActiveSubmodelTransform);
				CameraAxis = swNormalize(swMultiply30(CameraAxis, ActiveSubmodelInverse));

				if (ActiveSubmodelInverse.Determinant() < 0.0f)
					CameraAxis = -CameraAxis;
			}

			ActiveModel->UpdateRotateTool(CameraAxis, mCameraRotationAngle * SW_RADIANS_TO_DEGREES, mTrackButton != swTrackButton::Left, false);
		}
		break;

	case swTrackTool::RotateTrainTrackRight:
	case swTrackTool::RotateTrainTrackLeft:
	case swTrackTool::InsertTrainTrack:
	case swTrackTool::SelectTrainTrack:
	case swTrackTool::Eraser:
		break;

	case swTrackTool::Paint:
		ActiveModel->PaintToolClicked(FindObjectUnderPointer(true, false).Object);
		break;

	case swTrackTool::ColorPicker:
		break;

	case swTrackTool::Zoom:
		ActiveModel->UpdateZoomTool(mCamera, 2.0f * MouseSensitivity * (mMouseY - mMouseDownY));
		break;

	case swTrackTool::Pan:
		UpdatePan(mMouseX, mMouseY);
		break;

	case swTrackTool::OrbitX:
		ActiveModel->UpdateOrbitTool(mCamera, 0.1f * MouseSensitivity * (mMouseX - mMouseDownX), 0.0f);
		break;

	case swTrackTool::OrbitY:
		// The minifigure preview only turns around its vertical axis.
		if (mViewType != swViewType::MinifigureBuilder)
			ActiveModel->UpdateOrbitTool(mCamera, 0.0f, 0.1f * MouseSensitivity * (mMouseY - mMouseDownY));
		break;

	case swTrackTool::OrbitXY:
		ActiveModel->UpdateOrbitTool(mCamera, 0.1f * MouseSensitivity * (mMouseX - mMouseDownX), mViewType == swViewType::MinifigureBuilder ? 0.0f : 0.1f * MouseSensitivity * (mMouseY - mMouseDownY));

		// Keep the whole minifigure in view while it turns.
		if (mViewType == swViewType::MinifigureBuilder)
			ZoomExtents(swGeometryBoundsMode::IncludeFallback, false);
		break;

	case swTrackTool::Roll:
		ActiveModel->UpdateRollTool(mCamera, 2.0f * MouseSensitivity * (mMouseX - mMouseDownX) * SW_DEGREES_TO_RADIANS);
		break;

	case swTrackTool::ZoomRegion:
		Redraw();
		break;

	case swTrackTool::Count:
		break;
	}
}

void swView::OnMouseLeave()
{
	if (mCutawayHoverHandle != -1)
	{
		mCutawayHoverHandle = -1;
		UpdateCursor();
		Redraw();
	}

	if (mTrackTool == swTrackTool::Insert)
	{
		mTrackTool = swTrackTool::None;

		UpdateCursor();

		swModel* ActiveModel = GetActiveModel();

		if (ActiveModel)
			ActiveModel->SetPreviewInsertPartInfo(std::vector<swInsertPartInformation>());
	}
}

void swView::OnMouseWheel(float Direction)
{
	float Scale = 10.0f;

	if (mMouseModifiers & Qt::ControlModifier)
		Scale = 100.0f;
	else if (mMouseModifiers & Qt::ShiftModifier)
		Scale = 1.0f;

	mModel->Zoom(mCamera, static_cast<int>(Direction * Scale));
}
