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
#include "sw_view_sphere.h"
#include "sw_widget_find_replace.h"
#include "sw_library.h"
#include "sw_window_utilities.h"
#include "sw_widget_selection_part_popup.h"

swFindReplaceParameters swView::mFindReplaceParams;
QPointer<swFindReplaceWidget> swView::mFindWidget;

swView* swView::mLastFocusedView;
std::vector<swView*> swView::mViews;

swView::swView(swViewType ViewType, swModel* Model)
	: mViewType(ViewType), mScene(new swScene()), mModel(Model)
{
	mContext = new swContext();
	mViews.push_back(this);

	mViewManipulator = std::unique_ptr<swViewManipulator>(new swViewManipulator(this));
	mViewSphere = std::unique_ptr<swViewSphere>(new swViewSphere(this));
	memset(mGridSettings, 0, sizeof(mGridSettings));

	mDragState = swDragState::None;
	mTrackToolFromOverlay = false;

	swView* ActiveView = gMainWindow ? gMainWindow->GetActiveView() : nullptr;

	if (ActiveView)
		SetCamera(ActiveView->mCamera, false);
	else
	{
		mCamera = new swCamera(true);
		mCamera->SetViewpoint(swViewpoint::Home);
	}
}

swView::~swView()
{
	DestroyOutlineFramebuffers();

	mContext->DestroyVertexBuffer(mGridBuffer);

	if (gMainWindow && mViewType == swViewType::View)
		gMainWindow->RemoveView(this);

	if (mCamera && mCamera->IsSimple())
		delete mCamera;

	mViews.erase(std::find(mViews.begin(), mViews.end(), this));

	if (mLastFocusedView == this)
		mLastFocusedView = nullptr;

	delete mContext;
}

std::vector<swView*> swView::GetModelViews(const swModel* Model)
{
	std::vector<swView*> Views;

	for (swView* View : mViews)
		if (View->GetModel() == Model)
			Views.push_back(View);

	return Views;
}

void swView::UpdateProjectViews(const Project* Project)
{
	for (swView* View : mViews)
	{
		const swModel* ViewModel = View->GetActiveModel();

		if (ViewModel && ViewModel->GetProject() == Project)
			View->Redraw();
	}
}

void swView::UpdateAllViews()
{
	for (swView* View : mViews)
		View->Redraw();
}

void swView::MakeCurrent()
{
	mContext->MakeCurrent();
}

void swView::Redraw()
{
	if (mWidget)
	{
		mWidget->UpdateModelEditingPath();
		mWidget->update();
	}
}

void swView::SetOffscreenContext()
{
	mContext->SetOffscreenContext();
}

void swView::SetFocus(bool Focus)
{
	if (Focus)
	{
		if (mLastFocusedView != this)
		{
			delete mFindWidget;
			mFindWidget = nullptr;

			mLastFocusedView = this;
		}

		emit FocusReceived();
	}
}

void swView::SetMousePosition(int MouseX, int MouseY)
{
	mMouseX = MouseX;
	mMouseY = MouseY;
}

void swView::SetMouseModifiers(Qt::KeyboardModifiers MouseModifiers)
{
	mMouseModifiers = MouseModifiers;
}

swModel* swView::GetActiveModel() const
{
	return !mActiveSubmodelInstance ? mModel : mActiveSubmodelInstance->mPartData->GetModel();
}

std::vector<swModel*> swView::GetModelEditingPath() const
{
	std::vector<swModel*> Models = { mModel };

	for (const swPart* Part : mSubmodelEditingPath)
		Models.push_back(Part->mPartData->GetModel());

	return Models;
}

void swView::RestoreModelEditingContext()
{
	mModel->SetActive(true);
	mActiveSubmodelTransform = swMatrix44Identity();

	for (swPart* Part : mSubmodelEditingPath)
	{
		mActiveSubmodelTransform = swMultiply(Part->mModelWorld, mActiveSubmodelTransform);
		Part->mPartData->GetModel()->SetActive(true);
	}
}

void swView::SetSubmodelActive(size_t Depth)
{
	if (Depth > mSubmodelEditingPath.size())
		return;

	StopTracking(true);

	const std::vector<swModel*> EditingModels = GetModelEditingPath();

	for (size_t Index = EditingModels.size(); Index > Depth + 1; Index--)
		EditingModels[Index - 1]->SetActive(false);

	std::vector<swModel*> UpdatedModels;
	for (size_t Index = EditingModels.size(); Index > Depth + 1; Index--)
		EditingModels[Index - 1]->UpdatePartData(UpdatedModels);

	mSubmodelEditingPath.resize(Depth);
	mActiveSubmodelInstance = nullptr;

	if (!mSubmodelEditingPath.empty())
		mActiveSubmodelInstance = mSubmodelEditingPath.back();

	RestoreModelEditingContext();
	GetActiveModel()->UpdateInterface();
}

void swView::SetTopSubmodelActive()
{
	SetSubmodelActive(0);
}

void swView::SetSelectedSubmodelActive()
{
	swModel* ActiveModel = GetActiveModel();
	swObject* Object = ActiveModel->GetFocusObject();

	if (!Object || !Object->IsPart())
		return;

	swPart* Part = (swPart*)Object;

	if (!Part->mPartData->IsModel())
		return;

	// Keep ancestor models at their chosen steps so the selected instance stays in place.
	mSubmodelEditingPath.push_back(Part);
	mActiveSubmodelInstance = Part;

	ActiveModel = mActiveSubmodelInstance->mPartData->GetModel();
	ActiveModel->SetTemporaryStep(ActiveModel->GetLastStep());
	RestoreModelEditingContext();

	RemoveCamera();

	GetActiveModel()->UpdateInterface();
}

void swView::CreateResources(swContext* Context)
{
	Q_UNUSED(Context);

	gGridTexture = new swTexture;
	gGridTexture->CreateGridTexture();
}

void swView::DestroyResources(swContext* Context)
{
	Q_UNUSED(Context);

	delete gGridTexture;
	gGridTexture = nullptr;
}

void swView::RemoveCamera()
{
	if (mCamera && mCamera->IsSimple())
		return;

	swCamera* Camera = mCamera;
	mCamera = new swCamera(true);

	if (Camera)
		mCamera->CopyPosition(Camera);
	else
		mCamera->SetViewpoint(swViewpoint::Home);

	Redraw();
}

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

void swView::ShowTrainTrackPopup()
{
	if (mViewType != swViewType::View)
		return;

	swModel* ActiveModel = GetActiveModel();
	swObject* Focus = ActiveModel->GetFocusObject();

	if (!Focus || !Focus->IsPart())
		return;

	swPart* Part = (swPart*)Focus;
	const swTrainTrackInformation* TrainTrackInfo = Part->mPartData->GetTrainTrackInfo();

	if (!TrainTrackInfo)
		return;

	if (mTrackToolSection < SW_PART_SECTION_TRAIN_TRACK_CONNECTION_FIRST)
		return;

	int ConnectionIndex = mTrackToolSection - SW_PART_SECTION_TRAIN_TRACK_CONNECTION_FIRST;
	const swTrainTrackConnectionType& ConnectionType = TrainTrackInfo->GetConnections()[ConnectionIndex].Type;
	std::vector<swPartData*> TrainTrackParts = swGetPartsLibrary()->GetVisibleTrainTrackParts(ConnectionType);

	auto PartSortFunc = [](const swPartData* a, const swPartData* b)
	{
		if (!a)
			return true;

		if (!b)
			return false;

		return strcmp(a->m_strDescription, b->m_strDescription) < 0;
	};

	std::sort(TrainTrackParts.begin(), TrainTrackParts.end(), PartSortFunc);

	std::vector<std::pair<swPartData*, std::string>> Parts;

	Parts.reserve(TrainTrackParts.size());

	for (swPartData* PartData : TrainTrackParts)
		Parts.emplace_back(PartData, std::string());

	std::optional<swPartData*> Result = swShowPartSelectionPopup(nullptr, Parts, gMainWindow->GetInsertColorIndex(), mWidget, QCursor::pos());
	swPartData* PartData = Result.has_value() ? Result.value() : nullptr;

	if (PartData)
	{
		Part = ActiveModel->AddPart(PartData, mTrackToolSection);

		if (Part)
			ActiveModel->UpdateTrainTrackConnections(Part, false);
	}

	mWidget->UpdateMousePosition();
}

void swView::ShowContextMenu() const
{
	if (mViewType != swViewType::View)
		return;

	QAction** Actions = gMainWindow->mActions;

	QMenu* Popup = new QMenu();

	Popup->addAction(Actions[SW_EDIT_CUT]);
	Popup->addAction(Actions[SW_EDIT_COPY]);
	Popup->addAction(Actions[SW_EDIT_PASTE]);
	Popup->addAction(Actions[SW_EDIT_PASTE_STEPS]);
	Popup->addAction(Actions[SW_PART_DUPLICATE]);

	Popup->addSeparator();

	Popup->addAction(Actions[SW_PART_CONTROL_POINT_INSERT]);
	Popup->addAction(Actions[SW_PART_CONTROL_POINT_REMOVE]);

	Popup->addSeparator();

	Popup->addAction(Actions[SW_PART_EDIT_SELECTED_SUBMODEL]);
	Popup->addAction(Actions[SW_PART_EDIT_END_SUBMODEL]);
	Popup->addAction(Actions[SW_PART_VIEW_SELECTED_MODEL]);
	Popup->addAction(Actions[SW_PART_INLINE_SELECTED_MODELS]);
	Popup->addAction(Actions[SW_PART_MOVE_SELECTION_TO_MODEL]);

	Popup->addSeparator();

	Popup->addMenu(gMainWindow->GetToolsMenu());
	Popup->addAction(Actions[SW_VIEW_NORMAL_TO_FACE]);
	Popup->addMenu(gMainWindow->GetCutawayMenu());
	Popup->addMenu(gMainWindow->GetViewpointMenu());
	Popup->addMenu(gMainWindow->GetCameraMenu());
	Popup->addMenu(gMainWindow->GetProjectionMenu());
	Popup->addMenu(gMainWindow->GetShadingMenu());

	Popup->addSeparator();

	Popup->addAction(Actions[SW_VIEW_SPLIT_HORIZONTAL]);
	Popup->addAction(Actions[SW_VIEW_SPLIT_VERTICAL]);
	Popup->addAction(Actions[SW_VIEW_REMOVE_VIEW]);
	Popup->addAction(Actions[SW_VIEW_RESET_VIEWS]);

	Popup->exec(QCursor::pos());
	delete Popup;
}

bool swView::CloseFindReplaceDialog()
{
	if (mFindWidget && (mWidget->hasFocus() || mFindWidget->focusWidget()))
	{
		delete mFindWidget;
		mFindWidget = nullptr;

		return true;
	}

	return false;
}

void swView::ShowFindReplaceWidget(bool Replace)
{
	delete mFindWidget;
	mFindReplaceParams = swFindReplaceParameters();
	mFindWidget = new swFindReplaceWidget(mWidget, GetActiveModel(), Replace);
	mWidget->SetFindReplaceWidget(mFindWidget);
}

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

// Step view angles rotate the model about the LDraw X, Y and Z axes, in that order.
static swMatrix44 swGetStepViewRotation(const swVector3& Angles)
{
	const swMatrix44 Rotation = swMultiply(swMultiply(swMatrix44RotationX(Angles.x * SW_DEGREES_TO_RADIANS), swMatrix44RotationY(Angles.y * SW_DEGREES_TO_RADIANS)), swMatrix44RotationZ(Angles.z * SW_DEGREES_TO_RADIANS));

	return swMatrix44LDrawToStudWorks(Rotation);
}

static void swGetStepViewBase(swStepViewMode Mode, swVector3& Direction, swVector3& Up)
{
	// Relative views start from the home view, absolute views from the front view.
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

	// Steps without a view use the default view, but only in models that use step views.
	if (!StepView)
	{
		if (!mModel->GetProperties().mStepViews.empty())
			SetViewpoint(swViewpoint::Home);

		return;
	}

	swVector3 Direction, Up;
	swGetStepViewBase(StepView->Mode, Direction, Up);

	// Turning the model by the step rotation is the same as turning the camera the opposite way.
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

	// The camera frame expressed against the front view frame gives the model rotation.
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

// use to resize the view, a fit asked for before it had a size is done now
void swView::SetSize(int Width, int Height)
{
	mWidth = Width;
	mHeight = Height;

	if (mZoomExtentsPending && mWidth > 1 && mHeight > 1)
	{
		mZoomExtentsPending = false;
		ZoomExtents(mZoomExtentsMode, mZoomExtentsFarPlane);
	}
}

void swView::ZoomExtents(swGeometryBoundsMode Mode, bool AdjustFarPlane)
{
	swModel* ActiveModel = GetActiveModel();

	// a view without its size yet fits again once it has one
	mZoomExtentsPending = mWidth <= 1 || mHeight <= 1;
	mZoomExtentsMode = Mode;
	mZoomExtentsFarPlane = AdjustFarPlane;

	if (ActiveModel)
	{
		const swMatrix44 WorldMatrix = mActiveSubmodelInstance ? mActiveSubmodelTransform : swMatrix44Identity();

		ActiveModel->ZoomExtents(mCamera, (float)mWidth / (float)mHeight, WorldMatrix, Mode, AdjustFarPlane);
	}
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

swCursor swView::GetCursor() const
{
	const swTool CurrentTool = GetCurrentTool();
	const bool IsMoveTool = mTrackTool >= swTrackTool::MoveX && mTrackTool <= swTrackTool::MoveXYZ;

	if (mTrackButton != swTrackButton::None && mTrackTool != swTrackTool::Paint && !IsMoveTool && CurrentTool != swTool::Rotate)
		return swCursor::Hidden;

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

// Cursors draw the white Blender icons with a black outline so they stay visible on any background.
static QPixmap swCreateCursorPixmap(const QString& Name, const QString& Badge)
{
	QImage Glyph(32, 32, QImage::Format_ARGB32_Premultiplied);
	Glyph.fill(Qt::transparent);

	QImage Icon(QString::fromLatin1(":/resources/interface/cursors/png/24/%1_24.png").arg(Name));

	QPainter Painter(&Glyph);

	// the arrow tip sits in the top left corner and leaves the bottom right for the badge
	if (Name == QLatin1String("cursor_select"))
		Painter.drawImage(-1, -3, Icon);
	else
		Painter.drawImage(4, 4, Icon);

	if (!Badge.isEmpty())
		Painter.drawImage(15, 15, QImage(QString::fromLatin1(":/resources/interface/cursors/png/16/%1_16.png").arg(Badge)));

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
		const char* Name;
		const char* Badge;
	};

	constexpr swCursorInformation Cursors[] =
	{
		{  0,  0, "", "" },                                  // swCursor::Hidden
		{  0,  0, "", "" },                                  // swCursor::Default
		{ 16, 16, "cursor_brick", "" },                      // swCursor::Brick
		{ 16, 16, "cursor_light_point", "" },                // swCursor::PointLight
		{ 16, 16, "cursor_light_spot", "" },                 // swCursor::SpotLight
		{ 16, 16, "cursor_light_directional", "" },          // swCursor::DirectionalLight
		{ 16, 16, "cursor_light_area", "" },                 // swCursor::AreaLight
		{ 16, 16, "cursor_camera", "" },                     // swCursor::Camera
		{  1,  1, "cursor_select", "" },                     // swCursor::Select
		{  1,  1, "cursor_select", "cursor_select_add" },    // swCursor::SelectAdd
		{  1,  1, "cursor_select", "cursor_select_remove" }, // swCursor::SelectRemove
		{ 16, 16, "cursor_move", "" },                       // swCursor::Move
		{ 16, 16, "cursor_move_axis_x", "" },                // swCursor::MoveAxisX
		{ 16, 16, "cursor_move_axis_y", "" },                // swCursor::MoveAxisY
		{ 16, 16, "cursor_move_axis_z", "" },                // swCursor::MoveAxisZ
		{ 16, 16, "cursor_rotate", "" },                     // swCursor::Rotate
		{ 16, 16, "cursor_rotate_axis_x", "" },              // swCursor::RotateAxisX
		{ 16, 16, "cursor_rotate_axis_y", "" },              // swCursor::RotateAxisY
		{ 16, 16, "cursor_rotate_axis_z", "" },              // swCursor::RotateAxisZ
		{ 16, 16, "cursor_rotate_x", "" },                   // swCursor::RotateX
		{ 16, 16, "cursor_rotate_y", "" },                   // swCursor::RotateY
		{ 16, 16, "cursor_delete", "" },                     // swCursor::Delete
		{ 14, 25, "cursor_paint", "" },                      // swCursor::Paint
		{  6, 24, "cursor_picker_color", "" },               // swCursor::ColorPicker
		{ 13, 14, "cursor_zoom", "" },                       // swCursor::Zoom
		{ 12, 12, "cursor_zoom_region", "" },                // swCursor::ZoomRegion
		{ 16, 16, "cursor_pan", "" },                        // swCursor::Pan
		{ 16, 16, "cursor_roll", "" },                       // swCursor::Roll
		{ 16, 16, "cursor_view_rotate", "" },                // swCursor::RotateView
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
		mWidget->setCursor(QCursor(swCreateCursorPixmap(Cursor.Name, Cursor.Badge), Cursor.x, Cursor.y));
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

	if (mViewSphere->OnLeftButtonDown())
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

	if (mViewSphere->OnLeftButtonUp())
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

		if (mViewSphere->OnMouseMove())
		{
			swTrackTool NewTrackTool = mViewSphere->IsDragging() ? swTrackTool::OrbitXY : swTrackTool::None;

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
