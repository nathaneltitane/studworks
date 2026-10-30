#include "sw_global.h"
#include "sw_view.h"
#include "sw_window_main.h"
#include "sw_object_camera.h"

swVector3 swView::ProjectPoint(const swVector3& Point) const
{
	int Viewport[4] = { 0, 0, mWidth, mHeight };
	return swProjectPoint(Point, mCamera->mWorldView, GetProjectionMatrix(), Viewport);
}

swVector3 swView::UnprojectPoint(const swVector3& Point) const
{
	int Viewport[4] = { 0, 0, mWidth, mHeight };
	return swUnprojectPoint(Point, mCamera->mWorldView, GetProjectionMatrix(), Viewport);
}

void swView::UnprojectPoints(swVector3* Points, int NumPoints) const
{
	int Viewport[4] = { 0, 0, mWidth, mHeight };
	swUnprojectPoints(Points, NumPoints, mCamera->mWorldView, GetProjectionMatrix(), Viewport);
}

swMatrix44 swView::GetProjectionMatrix() const
{
	float AspectRatio = (float)mWidth / (float)mHeight;

	if (mCamera->GetProjection() == swCameraProjection::Orthographic)
	{
		float OrthoHeight = mCamera->GetOrthoHeight() / 2.0f;
		float OrthoWidth = OrthoHeight * AspectRatio;

		return swMatrix44Orthographic(-OrthoWidth, OrthoWidth, -OrthoHeight, OrthoHeight, mCamera->m_zNear, mCamera->m_zFar * 4);
	}
	else
		return swMatrix44Perspective(mCamera->m_fovy, AspectRatio, mCamera->m_zNear, mCamera->m_zFar);
}

swMatrix44 swView::GetTileProjectionMatrix(int CurrentRow, int CurrentColumn, int CurrentTileWidth, int CurrentTileHeight) const
{
	int ImageWidth = mRenderImage.width();
	int ImageHeight = mRenderImage.height();

	double ImageLeft, ImageRight, ImageBottom, ImageTop, Near, Far;
	double AspectRatio = (double)ImageWidth / (double)ImageHeight;

	if (mCamera->GetProjection() == swCameraProjection::Orthographic)
	{
		float OrthoHeight = mCamera->GetOrthoHeight() / 2.0f;
		float OrthoWidth = OrthoHeight * AspectRatio;

		ImageLeft = -OrthoWidth;
		ImageRight = OrthoWidth;
		ImageBottom = -OrthoHeight;
		ImageTop = OrthoHeight;
		Near = mCamera->m_zNear;
		Far = mCamera->m_zFar * 4;
	}
	else
	{
		double xmin, xmax, ymin, ymax;
		ymax = mCamera->m_zNear * tan(mCamera->m_fovy * 3.14159265 / 360.0);
		ymin = -ymax;
		xmin = ymin * AspectRatio;
		xmax = ymax * AspectRatio;

		ImageLeft = xmin;
		ImageRight = xmax;
		ImageBottom = ymin;
		ImageTop = ymax;
		Near = mCamera->m_zNear;
		Far = mCamera->m_zFar;
	}

	double Left = ImageLeft + (ImageRight - ImageLeft) * (CurrentColumn * mWidth) / ImageWidth;
	double Right = Left + (ImageRight - ImageLeft) * CurrentTileWidth / ImageWidth;
	double Bottom = ImageBottom + (ImageTop - ImageBottom) * (CurrentRow * mHeight) / ImageHeight;
	double Top = Bottom + (ImageTop - ImageBottom) * CurrentTileHeight / ImageHeight;

	if (mCamera->GetProjection() == swCameraProjection::Orthographic)
		return swMatrix44Orthographic(Left, Right, Bottom, Top, Near, Far);
	else
		return swMatrix44Frustum(Left, Right, Bottom, Top, Near, Far);
}

void swView::SetViewpoint(swViewpoint Viewpoint)
{
	if (!mCamera || !mCamera->IsSimple())
	{
		swCamera* OldCamera = mCamera;

		mCamera = new swCamera(true);

		if (OldCamera)
			mCamera->CopySettings(OldCamera);
	}

	mCamera->SetViewpoint(Viewpoint);
	ZoomExtents(swGeometryBoundsMode::IncludeFallback, false);
	Redraw();
}

void swView::SetViewpoint(const swVector3& Position)
{
	if (!mCamera || !mCamera->IsSimple())
	{
		swCamera* OldCamera = mCamera;

		mCamera = new swCamera(true);

		if (OldCamera)
			mCamera->CopySettings(OldCamera);
	}

	mCamera->SetViewpoint(Position);
	ZoomExtents(swGeometryBoundsMode::IncludeFallback, false);
	Redraw();
}

// step view angles about the ldraw axes
static swMatrix44 swGetStepViewRotation(const swVector3& Angles)
{
	const swMatrix44 Rotation = swMultiply(swMultiply(swMatrix44RotationX(Angles.x * SW_DEGREES_TO_RADIANS), swMatrix44RotationY(Angles.y * SW_DEGREES_TO_RADIANS)), swMatrix44RotationZ(Angles.z * SW_DEGREES_TO_RADIANS));

	return swMatrix44LDrawToStudWorks(Rotation);
}

static void swGetStepViewBase(swStepViewMode Mode, swVector3& Direction, swVector3& Up)
{
	// relative from home, absolute from front
	if (Mode == swStepViewMode::Absolute)
	{
		Direction = swVector3(0.0f, -1.0f, 0.0f);
		Up = swVector3(0.0f, 0.0f, 1.0f);
	}
	else
	{
		Direction = swNormalize(swVector3(375.0f, -375.0f, 187.5f));
		Up = swNormalize(swVector3(0.2357f, -0.2357f, 0.94281f));
	}
}

void swView::ApplyStepView()
{
	ApplyStepView(mModel->GetCurrentStep());
}

void swView::ApplyStepView(swStep Step)
{
	if (mViewType != swViewType::View)
		return;

	const std::optional<swStepView> StepView = mModel->GetEffectiveStepView(Step);

	// steps without a view use the default
	if (!StepView)
	{
		if (!mModel->GetProperties().mStepViews.empty())
			SetViewpoint(swViewpoint::Home);

		return;
	}

	swVector3 Direction, Up;
	swGetStepViewBase(StepView->Mode, Direction, Up);

	// model turn equals camera turn
	const swMatrix44 Inverse = swMatrix44Transpose(swGetStepViewRotation(StepView->Angles));

	Direction = swNormalize(swMultiply30(Direction, Inverse));
	Up = swNormalize(swMultiply30(Up, Inverse));

	SetViewpoint(Direction * 1250.0f, swVector3(0.0f, 0.0f, 0.0f), Up);
	ZoomExtents(swGeometryBoundsMode::IncludeFallback, false);
}

swVector3 swView::GetStepViewAngles() const
{
	swVector3 BaseDirection, BaseUp;
	swGetStepViewBase(swStepViewMode::Absolute, BaseDirection, BaseUp);

	const swVector3 Direction = swNormalize(mCamera->mPosition - mCamera->mTargetPosition);
	const swVector3 Right = swNormalize(swCross(mCamera->mUpVector, Direction));
	const swVector3 Up = swCross(Direction, Right);
	const swVector3 BaseRight = swNormalize(swCross(BaseUp, BaseDirection));

	// frame against the front view
	const swMatrix44 Base(swVector4(BaseRight, 0.0f), swVector4(BaseUp, 0.0f), swVector4(BaseDirection, 0.0f), swVector4(0.0f, 0.0f, 0.0f, 1.0f));
	const swMatrix44 Current(swVector4(Right, 0.0f), swVector4(Up, 0.0f), swVector4(Direction, 0.0f), swVector4(0.0f, 0.0f, 0.0f, 1.0f));
	const swMatrix44 Rotation = swMatrix44StudWorksToLDraw(swMultiply(swMatrix44Transpose(Current), Base));

	float X, Y, Z;
	const float SinY = swClamp(-Rotation[0][2], -1.0f, 1.0f);
	Y = asinf(SinY);

	if (fabsf(SinY) < 0.9999f)
	{
		X = atan2f(Rotation[1][2], Rotation[2][2]);
		Z = atan2f(Rotation[0][1], Rotation[0][0]);
	}
	else
	{
		X = atan2f(-Rotation[2][1], Rotation[1][1]);
		Z = 0.0f;
	}

	auto Round = [](float Radians)
	{
		return qRound(Radians * SW_RADIANS_TO_DEGREES * 100.0f) / 100.0f;
	};

	return swVector3(Round(X), Round(Y), Round(Z));
}

void swView::SetViewpoint(const swVector3& Position, const swVector3& Target, const swVector3& Up)
{
	if (!mCamera || !mCamera->IsSimple())
	{
		swCamera* OldCamera = mCamera;

		mCamera = new swCamera(true);

		if (OldCamera)
			mCamera->CopySettings(OldCamera);
	}

	mCamera->SetViewpoint(Position, Target, Up);
	Redraw();
}

void swView::SetCameraAngles(float Latitude, float Longitude)
{
	if (!mCamera || !mCamera->IsSimple())
	{
		swCamera* OldCamera = mCamera;

		mCamera = new swCamera(true);

		if (OldCamera)
			mCamera->CopySettings(OldCamera);
	}

	mCamera->SetAngles(Latitude, Longitude, 1.0f);
	ZoomExtents(swGeometryBoundsMode::IncludeFallback, false);
	Redraw();
}

void swView::SetDefaultCamera()
{
	if (!mCamera || !mCamera->IsSimple())
		mCamera = new swCamera(true);

	mCamera->SetViewpoint(swViewpoint::Home);
	ZoomExtents(swGeometryBoundsMode::IncludeFallback, false);
	Redraw();
}

void swView::SetCamera(swCamera* Camera, bool ForceCopy)
{
	if (Camera->IsSimple() || ForceCopy)
	{
		if (!mCamera || !mCamera->IsSimple())
			mCamera = new swCamera(true);

		mCamera->CopyPosition(Camera);
	}
	else
	{
		if (mCamera && mCamera->IsSimple())
			delete mCamera;

		mCamera = Camera;
	}
}

void swView::SetCamera(const QString& CameraName)
{
	const std::vector<std::unique_ptr<swCamera>>& Cameras = mModel->GetCameras();

	for (size_t CameraIndex = 0; CameraIndex < Cameras.size(); CameraIndex++)
	{
		if (CameraName.compare(Cameras[CameraIndex]->GetName(), Qt::CaseInsensitive) == 0)
		{
			SetCameraIndex(CameraIndex);
			return;
		}
	}
}

void swView::SetCameraIndex(size_t CameraIndex)
{
	const std::vector<std::unique_ptr<swCamera>>& Cameras = mModel->GetCameras();

	if (CameraIndex >= Cameras.size())
		return;

	swCamera* Camera = Cameras[CameraIndex].get();
	SetCamera(Camera, false);

	Redraw();
}

void swView::SetCameraProjection(swCameraProjection CameraProjection)
{
	if (mViewType == swViewType::View)
	{
		mLastProjection = CameraProjection;

		// share the projection with the other views
		if (swGetPreferences().mProjectionScope == swProjectionScope::AllViews)
		{
			for (swView* View : mViews)
			{
				if (View != this && View->mViewType == swViewType::View && View->mCamera->IsSimple())
				{
					View->mCamera->SetProjection(CameraProjection);
					View->Redraw();
				}
			}
		}
	}

	if (mCamera->IsSimple())
	{
		mCamera->SetProjection(CameraProjection);
		Redraw();
	}
	else
	{
		swModel* ActiveModel = GetActiveModel();

		if (ActiveModel)
			ActiveModel->SetCameraProjection(mCamera, CameraProjection);
	}
}

void swView::LookAt()
{
	swModel* ActiveModel = GetActiveModel();
	if (ActiveModel)
		ActiveModel->LookAt(mCamera);
}

void swView::MoveCamera(const swVector3& Direction)
{
	swModel* ActiveModel = GetActiveModel();
	if (ActiveModel)
		ActiveModel->MoveCamera(mCamera, Direction);
}

void swView::Zoom(float Amount)
{
	swModel* ActiveModel = GetActiveModel();
	if (ActiveModel)
		ActiveModel->Zoom(mCamera, Amount);
}

// use to resize the view
void swView::SetSize(int Width, int Height)
{
	const bool Resized = Width != mWidth || Height != mHeight;

	mWidth = Width;
	mHeight = Height;

	if (Resized && mWidth > 1 && mHeight > 1 && IsFitted())
		ZoomExtents(mZoomExtentsMode, mZoomExtentsFarPlane);
}

// use to check the view is still fitted
bool swView::IsFitted() const
{
	return mCamera && mCamera->IsSimple() && mCamera == mZoomExtentsCamera && mCamera->mWorldView == mZoomExtentsWorldView;
}

void swView::ZoomExtents(swGeometryBoundsMode Mode, bool AdjustFarPlane)
{
	swModel* ActiveModel = GetActiveModel();

	mZoomExtentsMode = Mode;
	mZoomExtentsFarPlane = AdjustFarPlane;

	if (ActiveModel)
	{
		const swMatrix44 WorldMatrix = mActiveSubmodelInstance ? mActiveSubmodelTransform : swMatrix44Identity();

		ActiveModel->ZoomExtents(mCamera, (float)mWidth / (float)mHeight, WorldMatrix, Mode, AdjustFarPlane);
	}

	// remember the fitted camera
	mZoomExtentsCamera = mCamera;

	if (mCamera)
		mZoomExtentsWorldView = mCamera->mWorldView;
}

void swView::ZoomExtentsAtStep(swStep Step, swGeometryBoundsMode Mode, bool AdjustFarPlane)
{
	swModel* ActiveModel = GetActiveModel();

	if (!ActiveModel)
		return;

	const swStep CurrentStep = ActiveModel->GetCurrentStep();

	ActiveModel->SetTemporaryStep(Step);
	ZoomExtents(Mode, AdjustFarPlane);
	ActiveModel->SetTemporaryStep(CurrentStep);

	if (!ActiveModel->IsActive())
		ActiveModel->CalculateStep(SW_STEP_MAXIMUM);
}
