#include "sw_global.h"
#include "sw_view.h"
#include "sw_window_main.h"
#include "sw_object_camera.h"

void swView::StartCutawayPick(swCutawayShape CutawayShape)
{
	if (mViewType != swViewType::View || !mWidget)
		return;

	if (mTrackButton != swTrackButton::None)
		StopTracking(false);

	mPickingFaceNormal = false;
	mCutawayPickShape = CutawayShape;
	UpdateCursor();

	gMainWindow->statusBar()->showMessage(tr("Click a face to cut away the model around it, or press Esc to cancel."));
}

void swView::StopCutawayPick()
{
	mCutawayPickShape = swCutawayShape::None;
	mCursor = swCursor::Count;
	UpdateCursor();

	gMainWindow->statusBar()->clearMessage();
}

// use to center the cutaway on the clicked face with its depth along the face normal
void swView::PickCutaway()
{
	const swObjectRayTest ObjectRayTest = RayTest(true, false);
	swVector3 Point = mCamera->mTargetPosition;
	swVector3 Normal = swNormalize(mCamera->mPosition - mCamera->mTargetPosition);

	if (ObjectRayTest.ObjectSection.Object)
	{
		Point = ObjectRayTest.Start + swNormalize(ObjectRayTest.End - ObjectRayTest.Start) * ObjectRayTest.Distance;
		Normal = swNormalize(swMultiply30(ObjectRayTest.PartRayTest.Normal, ObjectRayTest.PartRayTest.Transform));

		if (swDot(Normal, ObjectRayTest.Start - Point) < 0.0f)
			Normal = -Normal;

		if (GetActiveModel() != mModel)
		{
			Point = swMultiply31(Point, mActiveSubmodelTransform);
			Normal = swNormalize(swMultiply30(Normal, mActiveSubmodelTransform));
		}
	}

	// a click places the default size in studs across, one stud is 20 ldu
	mCutawaySize = swGetPreferences().mCutawaySize * 10.0f;

	// the cutaway stays aligned to the world axes, the cylinder runs along the one closest to the face normal
	int NormalAxis = 0;

	for (int AxisIndex = 1; AxisIndex < 3; AxisIndex++)
		if (fabsf(Normal[AxisIndex]) > fabsf(Normal[NormalAxis]))
			NormalAxis = AxisIndex;

	mCutawayNormal = mCutawayAxes[NormalAxis] * (Normal[NormalAxis] < 0.0f ? -1.0f : 1.0f);
	mCutawayShapeAxes[0] = mCutawayAxes[(NormalAxis + 1) % 3];
	mCutawayShapeAxes[1] = mCutawayAxes[(NormalAxis + 2) % 3];
	mCutawayShapeAxes[2] = mCutawayAxes[NormalAxis];
	mCutawayCenter = Point;
	mCutawayShape = mCutawayPickShape;

	StopCutawayPick();

	// dragging on from the click draws the size
	mCutawayDragHandle = 7;
	mCutawayDragCenter = mCutawayCenter;
	mCutawayDragSize = mCutawaySize;
	mCutawayDragMouseX = mMouseX;
	mCutawayDragMouseY = mMouseY;
	UpdateCursor();
	Redraw();
}

void swView::ResizeCutaway(float Scale)
{
	if (mCutawayShape == swCutawayShape::None)
	{
		gMainWindow->statusBar()->showMessage(tr("Place a cube, sphere or cylinder cutaway first."), 3000);
		return;
	}

	mCutawaySize = qMax(mCutawaySize * Scale, 2.0f);
	Redraw();
}

// use to move the cutaway into the model by a positive distance or out of it by a negative one
void swView::MoveCutaway(float Distance)
{
	if (mCutawayShape == swCutawayShape::None)
	{
		gMainWindow->statusBar()->showMessage(tr("Place a cube, sphere or cylinder cutaway first."), 3000);
		return;
	}

	mCutawayCenter -= mCutawayNormal * Distance;
	Redraw();
}

void swView::RemoveCutaway()
{
	if (mCutawayPickShape != swCutawayShape::None)
		StopCutawayPick();

	mCutawayShape = swCutawayShape::None;
	mCutawayHoverHandle = -1;
	mCutawayDragHandle = -1;
	UpdateCursor();
	Redraw();
}

// use to keep the cutaway gizmo arrows about 100 pixels long at any zoom
float swView::GetCutawayHandleLength() const
{
	swVector3 ScreenPosition = ProjectPoint(mCutawayCenter);
	ScreenPosition.x += 10.0f;

	return swLength(UnprojectPoint(ScreenPosition) - mCutawayCenter) * 10.0f * GetUIScale();
}

// use to find the gizmo handle under the mouse, 0 to 2 for the axes, 3 for the center and 4 to 6 for the size knobs, 7 draws a new cutaway
int swView::FindCutawayHandle() const
{
	if (mViewType != swViewType::View || mCutawayShape == swCutawayShape::None || mCutawayPickShape != swCutawayShape::None)
		return -1;

	const float Reach = 8.0f * GetUIScale();
	const float HandleLength = GetCutawayHandleLength();
	const swVector3 Center = ProjectPoint(mCutawayCenter);
	const swVector3 Mouse((float)mMouseX, (float)mMouseY, 0.0f);

	if (Center.z < 0.0f || Center.z > 1.0f)
		return -1;

	for (int AxisIndex = 0; AxisIndex < 3; AxisIndex++)
	{
		const swVector3 Knob = ProjectPoint(mCutawayCenter + mCutawayAxes[AxisIndex] * mCutawaySize);

		if (swLength(swVector3(Mouse.x - Knob.x, Mouse.y - Knob.y, 0.0f)) < Reach * 1.5f)
			return 4 + AxisIndex;
	}

	if (swLength(swVector3(Mouse.x - Center.x, Mouse.y - Center.y, 0.0f)) < Reach * 2.0f)
		return 3;

	int Handle = -1;
	float HandleDistance = Reach;

	for (int AxisIndex = 0; AxisIndex < 3; AxisIndex++)
	{
		const swVector3 End = ProjectPoint(mCutawayCenter + mCutawayAxes[AxisIndex] * HandleLength);
		const swVector3 Segment(End.x - Center.x, End.y - Center.y, 0.0f);
		const swVector3 Offset(Mouse.x - Center.x, Mouse.y - Center.y, 0.0f);
		const float SegmentLength = swDot(Segment, Segment);

		if (SegmentLength < 1.0f)
			continue;

		const float Along = qBound(0.0f, swDot(Offset, Segment) / SegmentLength, 1.0f);
		const float Distance = swLength(Offset - Segment * Along);

		if (Distance < HandleDistance)
		{
			Handle = AxisIndex;
			HandleDistance = Distance;
		}
	}

	return Handle;
}

// use to move the cutaway with the arrows or the center and to resize it with the knobs
void swView::UpdateCutawayDrag()
{
	const swVector3 Center = ProjectPoint(mCutawayDragCenter);

	if (mCutawayDragHandle == 7)
	{
		// a drag of a few pixels still counts as a click with the default size
		if (abs(mMouseX - mCutawayDragMouseX) + abs(mMouseY - mCutawayDragMouseY) < 4)
			mCutawaySize = mCutawayDragSize;
		else
		{
			const swVector3 Edge = UnprojectPoint(swVector3((float)mMouseX, (float)mMouseY, Center.z));
			mCutawaySize = qMax(roundf(swLength(Edge - mCutawayDragCenter) / 10.0f) * 10.0f, 10.0f);
		}
	}
	else if (mCutawayDragHandle > 3)
	{
		const swVector3& Axis = mCutawayAxes[mCutawayDragHandle - 4];
		const swVector3 Start = ProjectPoint(mCutawayDragCenter + Axis * mCutawayDragSize);
		const swVector3 End = ProjectPoint(mCutawayDragCenter + Axis * (mCutawayDragSize + 1.0f));
		const swVector3 Segment(End.x - Start.x, End.y - Start.y, 0.0f);
		const swVector3 Offset((float)(mMouseX - mCutawayDragMouseX), (float)(mMouseY - mCutawayDragMouseY), 0.0f);
		const float SegmentLength = swDot(Segment, Segment);

		if (SegmentLength < 1e-6f)
			return;

		// the size steps by half a plate so the cut lines up with the bricks
		const float Size = mCutawayDragSize + swDot(Offset, Segment) / SegmentLength;
		mCutawaySize = qMax(roundf(Size / 4.0f) * 4.0f, 4.0f);
	}
	else if (mCutawayDragHandle == 3)
	{
		const swVector3 Start = UnprojectPoint(swVector3((float)mCutawayDragMouseX, (float)mCutawayDragMouseY, Center.z));
		const swVector3 End = UnprojectPoint(swVector3((float)mMouseX, (float)mMouseY, Center.z));

		mCutawayCenter = mCutawayDragCenter + End - Start;
	}
	else
	{
		const swVector3& Axis = mCutawayAxes[mCutawayDragHandle];
		const float HandleLength = GetCutawayHandleLength();
		const swVector3 End = ProjectPoint(mCutawayDragCenter + Axis * HandleLength);
		const swVector3 Segment(End.x - Center.x, End.y - Center.y, 0.0f);
		const swVector3 Offset((float)(mMouseX - mCutawayDragMouseX), (float)(mMouseY - mCutawayDragMouseY), 0.0f);
		const float SegmentLength = swDot(Segment, Segment);

		if (SegmentLength < 1.0f)
			return;

		mCutawayCenter = mCutawayDragCenter + Axis * (swDot(Offset, Segment) / SegmentLength * HandleLength);
	}

	Redraw();
}

// use to map world points to the unit cube or sphere of the cutaway
swMatrix44 swView::GetCutawayMatrix() const
{
	const swVector3& AxisX = mCutawayShapeAxes[0];
	const swVector3& AxisY = mCutawayShapeAxes[1];
	const swVector3& AxisZ = mCutawayShapeAxes[2];
	const float Scale = 1.0f / mCutawaySize;

	return swMatrix44(swVector4(AxisX.x, AxisY.x, AxisZ.x, 0.0f) * Scale, swVector4(AxisX.y, AxisY.y, AxisZ.y, 0.0f) * Scale, swVector4(AxisX.z, AxisY.z, AxisZ.z, 0.0f) * Scale,
		swVector4(-swDot(mCutawayCenter, AxisX) * Scale, -swDot(mCutawayCenter, AxisY) * Scale, -swDot(mCutawayCenter, AxisZ) * Scale, 1.0f));
}

// use to find the fractions of a world segment where it enters and leaves the cutaway
bool swView::GetCutawayRange(const swVector3& Start, const swVector3& End, float& Entry, float& Exit) const
{
	const swMatrix44 CutawayMatrix = GetCutawayMatrix();
	const swVector3 Origin = swMultiply31(Start, CutawayMatrix);
	const swVector3 Direction = swMultiply31(End, CutawayMatrix) - Origin;

	if (mCutawayShape == swCutawayShape::Sphere)
	{
		const float A = swDot(Direction, Direction);
		const float B = swDot(Origin, Direction);
		const float Discriminant = B * B - A * (swDot(Origin, Origin) - 1.0f);

		if (A <= 0.0f || Discriminant <= 0.0f)
			return false;

		Entry = (-B - sqrtf(Discriminant)) / A;
		Exit = (-B + sqrtf(Discriminant)) / A;
		return true;
	}

	Entry = -FLT_MAX;
	Exit = FLT_MAX;

	// the cylinder is a circle across x and y with the cube slab along z
	if (mCutawayShape == swCutawayShape::Cylinder)
	{
		const float A = Direction.x * Direction.x + Direction.y * Direction.y;
		const float B = Origin.x * Direction.x + Origin.y * Direction.y;
		const float C = Origin.x * Origin.x + Origin.y * Origin.y - 1.0f;

		if (A < 1e-12f)
		{
			if (C >= 0.0f)
				return false;
		}
		else
		{
			const float Discriminant = B * B - A * C;

			if (Discriminant <= 0.0f)
				return false;

			Entry = (-B - sqrtf(Discriminant)) / A;
			Exit = (-B + sqrtf(Discriminant)) / A;
		}
	}

	for (int AxisIndex = mCutawayShape == swCutawayShape::Cylinder ? 2 : 0; AxisIndex < 3; AxisIndex++)
	{
		if (fabsf(Direction[AxisIndex]) < 1e-6f)
		{
			if (fabsf(Origin[AxisIndex]) >= 1.0f)
				return false;

			continue;
		}

		const float Near = (-1.0f - Origin[AxisIndex]) / Direction[AxisIndex];
		const float Far = (1.0f - Origin[AxisIndex]) / Direction[AxisIndex];

		Entry = qMax(Entry, qMin(Near, Far));
		Exit = qMin(Exit, qMax(Near, Far));
	}

	return Entry < Exit;
}
