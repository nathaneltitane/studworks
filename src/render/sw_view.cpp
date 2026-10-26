#include "sw_global.h"
#include "sw_view.h"
#include "sw_view_widget.h"
#include <stdlib.h>
#include "sw_main_window.h"
#include "sw_light.h"
#include "sw_camera.h"
#include "sw_texture_font.h"
#include "sw_texture.h"
#include "sw_piece.h"
#include "sw_piece_data.h"
#include "sw_train_track.h"
#include "sw_scene.h"
#include "sw_context.h"
#include "sw_view_manipulator.h"
#include "sw_view_sphere.h"
#include "sw_find_replace_widget.h"
#include "sw_library.h"
#include "sw_utilities.h"
#include "sw_part_selection_popup.h"

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
	return !mActiveSubmodelInstance ? mModel : mActiveSubmodelInstance->mPieceData->GetModel();
}

std::vector<swModel*> swView::GetModelEditingPath() const
{
	std::vector<swModel*> Models = { mModel };

	for (const swPiece* Piece : mSubmodelEditingPath)
		Models.push_back(Piece->mPieceData->GetModel());

	return Models;
}

void swView::RestoreModelEditingContext()
{
	mModel->SetActive(true);
	mActiveSubmodelTransform = swMatrix44Identity();

	for (swPiece* Piece : mSubmodelEditingPath)
	{
		mActiveSubmodelTransform = swMultiply(Piece->mModelWorld, mActiveSubmodelTransform);
		Piece->mPieceData->GetModel()->SetActive(true);
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
		EditingModels[Index - 1]->UpdatePieceData(UpdatedModels);

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

	if (!Object || !Object->IsPiece())
		return;

	swPiece* Piece = (swPiece*)Object;

	if (!Piece->mPieceData->IsModel())
		return;

	// Keep ancestor models at their chosen steps so the selected instance stays in place.
	mSubmodelEditingPath.push_back(Piece);
	mActiveSubmodelInstance = Piece;

	ActiveModel = mActiveSubmodelInstance->mPieceData->GetModel();
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

	if (!Focus || !Focus->IsPiece())
		return;

	swPiece* Piece = (swPiece*)Focus;
	const swTrainTrackInformation* TrainTrackInfo = Piece->mPieceData->GetTrainTrackInfo();

	if (!TrainTrackInfo)
		return;

	if (mTrackToolSection < SW_PIECE_SECTION_TRAIN_TRACK_CONNECTION_FIRST)
		return;

	int ConnectionIndex = mTrackToolSection - SW_PIECE_SECTION_TRAIN_TRACK_CONNECTION_FIRST;
	const swTrainTrackConnectionType& ConnectionType = TrainTrackInfo->GetConnections()[ConnectionIndex].Type;
	std::vector<swPieceData*> TrainTrackParts = swGetPiecesLibrary()->GetVisibleTrainTrackParts(ConnectionType);

	auto PartSortFunc = [](const swPieceData* a, const swPieceData* b)
	{
		if (!a)
			return true;

		if (!b)
			return false;

		return strcmp(a->m_strDescription, b->m_strDescription) < 0;
	};

	std::sort(TrainTrackParts.begin(), TrainTrackParts.end(), PartSortFunc);

	std::vector<std::pair<swPieceData*, std::string>> Parts;

	Parts.reserve(TrainTrackParts.size());

	for (swPieceData* PieceData : TrainTrackParts)
		Parts.emplace_back(PieceData, std::string());

	std::optional<swPieceData*> Result = swShowPartSelectionPopup(nullptr, Parts, gMainWindow->GetInsertColorIndex(), mWidget, QCursor::pos());
	swPieceData* PieceData = Result.has_value() ? Result.value() : nullptr;

	if (PieceData)
	{
		Piece = ActiveModel->AddPiece(PieceData, mTrackToolSection);

		if (Piece)
			ActiveModel->UpdateTrainTrackConnections(Piece, false);
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
	Popup->addAction(Actions[SW_PIECE_DUPLICATE]);

	Popup->addSeparator();

	Popup->addAction(Actions[SW_PIECE_CONTROL_POINT_INSERT]);
	Popup->addAction(Actions[SW_PIECE_CONTROL_POINT_REMOVE]);

	Popup->addSeparator();

	Popup->addAction(Actions[SW_PIECE_EDIT_SELECTED_SUBMODEL]);
	Popup->addAction(Actions[SW_PIECE_EDIT_END_SUBMODEL]);
	Popup->addAction(Actions[SW_PIECE_VIEW_SELECTED_MODEL]);
	Popup->addAction(Actions[SW_PIECE_INLINE_SELECTED_MODELS]);
	Popup->addAction(Actions[SW_PIECE_MOVE_SELECTION_TO_MODEL]);

	Popup->addSeparator();

	Popup->addMenu(gMainWindow->GetToolsMenu());
	Popup->addAction(Actions[SW_VIEW_NORMAL_TO_FACE]);
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

	// Separate transforms always use each piece's axes. Use the focused piece
	// as the reference for the shared keyboard direction.
	if (gMainWindow->GetSeparateTransform())
	{
		swObject* Focus = GetActiveModel()->GetFocusObject();

		if (Focus && Focus->IsPiece())
			LocalToWorld = static_cast<swPiece*>(Focus)->GetRelativeRotation();
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

void swView::UpdatePiecePreview()
{
	const swPieceDataReference PreviewPieceData(gMainWindow->GetInsertPieceData());
	std::vector<swInsertPieceInformation> InsertPieceInfo;
	bool IsConnection;

	if (PreviewPieceData)
	{
		std::tie(InsertPieceInfo, IsConnection) = GetMouseInsertPieceInfo(false, true, PreviewPieceData, nullptr);
	}

	GetActiveModel()->SetPreviewInsertPieceInfo(std::move(InsertPieceInfo));
}

std::pair<std::vector<swInsertPieceInformation>, bool> swView::GetMouseInsertPieceInfo(bool IgnoreSelected, bool AllowNewPieces, const swPieceDataReference& PieceData, swPiece* MovingPiece) const
{
	swModel* ActiveModel = GetActiveModel();

	swGetPiecesLibrary()->EnsurePieceDataAssetsReady(PieceData);

	if (MovingPiece && PieceData->GetSynthInfo())
		swGetPiecesLibrary()->WaitForSynthMeshes({ MovingPiece });

	// The first piece of an empty model goes to the origin when enabled.
	if (!MovingPiece && swGetPreferences().mFirstPieceAtOrigin && ActiveModel->GetPieces().empty())
		return { { swInsertPieceInformation{ PieceData.Get(), swMatrix44Identity(), gMainWindow->GetInsertColorIndex() } }, false };

	swObjectRayTest ObjectRayTest = RayTest(true, IgnoreSelected);

	// Settling a hit's geometry can reveal another pending piece behind it, so pick again until the hit is stable.
	while (ObjectRayTest.ObjectSection.Object)
	{
		swPiece* MousePiece = reinterpret_cast<swPiece*>(ObjectRayTest.ObjectSection.Object);
		swPieceData* MousePieceData = MousePiece->mPieceData.Get();
		const bool Retest = MousePieceData->mState != swAssetState::Loaded || MousePieceData->IsModel() || MousePieceData->IsProject() || MousePiece->IsGeneratedMeshPending();

		swGetPiecesLibrary()->EnsurePieceDataAssetsReady(MousePiece->mPieceData);

		if (MousePieceData->GetSynthInfo())
			swGetPiecesLibrary()->WaitForSynthMeshes({ MousePiece });

		if (!Retest)
			break;

		swObjectRayTest ReadyRayTest = RayTest(true, IgnoreSelected);
		const bool SamePiece = ReadyRayTest.ObjectSection.Object == ObjectRayTest.ObjectSection.Object;
		ObjectRayTest = ReadyRayTest;

		if (SamePiece)
			break;
	}

	const swBoundingBox& BoundingBox = MovingPiece ? MovingPiece->GetBoundingBox() : PieceData->GetBoundingBox();

	if (ObjectRayTest.ObjectSection.Object)
	{
		swPiece* MousePiece = reinterpret_cast<swPiece*>(ObjectRayTest.ObjectSection.Object);
		swTrainTrackInformation* RayTrackInfo = MousePiece->mPieceData->GetTrainTrackInfo();
		swTrainTrackInformation* InfoTrackInfo = PieceData->GetTrainTrackInfo();

		if (RayTrackInfo && InfoTrackInfo)
		{
			ActiveModel->UpdateTrainTrackConnections(MousePiece, IgnoreSelected);

			swVector3 ClosestPoint = ObjectRayTest.Start + swNormalize(ObjectRayTest.End - ObjectRayTest.Start) * ObjectRayTest.Distance;
			quint32 FocusSection = MousePiece->GetFocusSection();

			std::vector<swInsertPieceInformation> TrainTracks = swTrainTrackInformation::GetInsertPieceInfo(MousePiece, PieceData.Get(), MovingPiece, gMainWindow->GetInsertColorIndex(), FocusSection, AllowNewPieces, ClosestPoint);

			if (!TrainTracks.empty())
				return { TrainTracks, true };
		}

		swVector3 Position = ObjectRayTest.PieceRayTest.Plane;

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
			Position = swMultiply31(ActiveModel->SnapPosition(Position), ObjectRayTest.PieceRayTest.Transform);
		else
			Position = ActiveModel->SnapPosition(swMultiply31(Position, ObjectRayTest.PieceRayTest.Transform));

		swMatrix44 WorldMatrix = ObjectRayTest.PieceRayTest.Transform;
		WorldMatrix.SetTranslation(Position);

		return { { swInsertPieceInformation{ PieceData.Get(), WorldMatrix, gMainWindow->GetInsertColorIndex() } }, false };
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
		return { { swInsertPieceInformation{ PieceData.Get(), swMatrix44Translation(Intersection), gMainWindow->GetInsertColorIndex() } }, false };
	}

	swVector3 Position;

	if (!ActiveModel->GetFocusPosition(Position))
		Position = ActiveModel->GetSelectionOrModelCenter();

	swVector3 FrontVector(mCamera->mTargetPosition - mCamera->mPosition);

	if (swLineSegmentPlaneIntersection(&Intersection, ClickPoints[0], ClickPoints[1], swVector4(FrontVector, -swDot(FrontVector, Position))))
	{
		Intersection = ActiveModel->SnapPosition(Intersection);
		return { { swInsertPieceInformation{ PieceData.Get(), swMatrix44Translation(Intersection), gMainWindow->GetInsertColorIndex() } }, false };
	}

	return { { swInsertPieceInformation{ PieceData.Get(), swMatrix44Translation(UnprojectPoint(swVector3((float)mMouseX, (float)mMouseY, 0.9f))), gMainWindow->GetInsertColorIndex() } }, false };
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

	if (ActiveModel->GetVisiblePiecesBoundingBox(Min, Max))
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

swObjectRayTest swView::RayTest(bool PiecesOnly, bool IgnoreSelected) const
{
	swVector3 StartEnd[2] =
	{
		swVector3((float)mMouseX, (float)mMouseY, 0.0f),
		swVector3((float)mMouseX, (float)mMouseY, 1.0f)
	};

	UnprojectPoints(StartEnd, 2);

	swObjectRayTest ObjectRayTest;

	ObjectRayTest.PiecesOnly = PiecesOnly;
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

	return ObjectRayTest;
}

swObjectSection swView::FindObjectUnderPointer(bool PiecesOnly, bool IgnoreSelected) const
{
	return RayTest(PiecesOnly, IgnoreSelected).ObjectSection;
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

std::vector<QImage> swView::GetStepImages(swStep Start, swStep End)
{
	std::vector<QImage> Images;
	mModel->WaitForAssets();

	if (!BeginRenderToImage(mWidth, mHeight))
	{
		if (gMainWindow)
			QMessageBox::warning(gMainWindow, tr("Create Step Images"), tr("Error creating images."));

		return Images;
	}

	const swStep CurrentStep = mModel->GetCurrentStep();
	bool RenderFailed = false;

	for (swStep Step = Start; Step <= End; Step++)
	{
		mModel->SetTemporaryStep(Step);

		OnDraw();

		QImage Image = GetRenderImage();

		if (Image.isNull())
		{
			RenderFailed = true;
			Images.clear();
			break;
		}

		Images.emplace_back(std::move(Image));
	}

	EndRenderToImage();

	mModel->SetTemporaryStep(CurrentStep);

	if (!mModel->IsActive())
		mModel->CalculateStep(SW_STEP_MAXIMUM);

	if (RenderFailed && gMainWindow)
		QMessageBox::warning(gMainWindow, tr("Create Step Images"), tr("Error creating images."));

	return Images;
}

swResult<void> swView::SaveStepImages(const QString& BaseName, bool AddStepSuffix, swStep Start, swStep End, std::function<void(const QString&)> ProgressCallback)
{
	std::vector<QImage> Images = GetStepImages(Start, End);

	if (Images.size() != static_cast<size_t>(End - Start + 1))
		return swUnexpected(tr("Error creating images."));

	for (swStep Step = Start; Step <= End; Step++)
	{
		QString FileName;

		if (AddStepSuffix)
			FileName = BaseName.arg(Step, 2, 10, QLatin1Char('0'));
		else
			FileName = BaseName;

		QImageWriter Writer(FileName);

		if (Writer.format().isEmpty())
			Writer.setFormat("png");

		if (!Writer.write(Images[Step - Start]))
			return swUnexpected(tr("Error writing to file '%1':\n%2").arg(FileName, Writer.errorString()));

		if (ProgressCallback)
			ProgressCallback(FileName);
	}

	return swResult<void>();
}

bool swView::BeginRenderToImage(int Width, int Height)
{
	swContext* Context = swContext::GetGlobalOffscreenContext();
	int MaxTexture = qMin(Context->GetMaxTextureSize(), 2048);

	const int Samples = QSurfaceFormat::defaultFormat().samples();
	if (Samples > 1)
		MaxTexture /= Samples;

	int TileWidth = qMin(Width, MaxTexture);
	int TileHeight = qMin(Height, MaxTexture);

	mWidth = TileWidth;
	mHeight = TileHeight;
	mRenderImage = QImage(Width, Height, QImage::Format_ARGB32);

	QOpenGLFramebufferObjectFormat Format;
	Format.setAttachment(QOpenGLFramebufferObject::Depth);

	if (QSurfaceFormat::defaultFormat().samples() > 1)
		Format.setSamples(QSurfaceFormat::defaultFormat().samples());

	mRenderFramebuffer = std::unique_ptr<QOpenGLFramebufferObject>(new QOpenGLFramebufferObject(QSize(TileWidth, TileHeight), Format));

	if (!mRenderFramebuffer->bind())
	{
		mRenderFramebuffer.reset();
		mRenderImage = QImage();
		return false;
	}

	return true;
}

void swView::EndRenderToImage()
{
	mRenderFramebuffer.reset();
}

QImage swView::GetRenderImage() const
{
	return mRenderImage;
}

void swView::DestroyOutlineFramebuffers()
{
	mOutlineFullFramebuffer.reset();
	mOutlineVisibleFramebuffer.reset();
	mOutlineAttemptSize = QSize();
}

bool swView::PrepareOutlineFramebuffers()
{
	QOpenGLContext* GLContext = mContext->GetGLContext();
	
	// Framebuffer objects are context-local under core/ARB OpenGL.
	if (mOutlineGLContext != GLContext)
	{
		DestroyOutlineFramebuffers();
		mOutlineGLContext = GLContext;
	}

	if (!GLContext || !swContext::SupportsSelectionOutline() || mWidth <= 0 || mHeight <= 0)
		return false;

	const QSize Size(mWidth, mHeight);
	
	if (mOutlineAttemptSize == Size)
		return mOutlineFullFramebuffer && mOutlineVisibleFramebuffer;

	swScopedGLContextState GLState;
	
	DestroyOutlineFramebuffers();
	
	mOutlineAttemptSize = Size;
	mOutlineFullFramebuffer = mContext->CreateFramebuffer(mWidth, mHeight);
	mOutlineVisibleFramebuffer = mContext->CreateFramebuffer(mWidth, mHeight);

	if (!mOutlineFullFramebuffer || !mOutlineVisibleFramebuffer)
	{
		mOutlineFullFramebuffer.reset();
		mOutlineVisibleFramebuffer.reset();
		
		qWarning("Selection outline framebuffer creation failed; using face tint and colored edges.");
		
		return false;
	}

	return true;
}

bool swView::GenerateOutlineMasks()
{
	if (mOutlineFullFramebuffer && mOutlineVisibleFramebuffer &&
		mContext->GenerateOutlineMasks(*mScene, *mOutlineFullFramebuffer, *mOutlineVisibleFramebuffer))
		return true;

	mOutlineFullFramebuffer.reset();
	mOutlineVisibleFramebuffer.reset();

	qWarning("Selection outline pass failed; using face tint and colored edges.");

	return false;
}

void swView::BindRenderFramebuffer()
{
	mRenderFramebuffer->bind();
}

void swView::UnbindRenderFramebuffer()
{
	mRenderFramebuffer->release();
}

QImage swView::GetRenderFramebufferImage() const
{
	return mRenderFramebuffer->toImage();
}

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
	const bool DrawOverlays = mWidget != nullptr;
	const bool DrawInterface = mWidget != nullptr && mViewType == swViewType::View;

	swRenderTarget RenderTarget = swRenderTarget::ModelView;

	if (mViewType == swViewType::PartsList)
		RenderTarget = swRenderTarget::Pieces;
	else if (mViewType == swViewType::Preview)
		RenderTarget = swRenderTarget::Preview;
	else if (mViewType == swViewType::Minifig)
		RenderTarget = swRenderTarget::Minifig;

	const swRenderSettings RenderSettings = Preferences.GetRenderSettings(RenderTarget);
	swShadingMode ShadingMode = RenderSettings.ShadingMode;
	if (ShadingMode == swShadingMode::Wireframe && !mWidget)
		ShadingMode = swShadingMode::Flat;

	mScene->SetShadingMode(ShadingMode);
	mScene->SetLineSettings(RenderSettings.DrawEdgeLines, RenderSettings.DrawConditionalLines, RenderSettings.LineWidth);
	mScene->SetAllowLOD(Preferences.mAllowLOD && mWidget != nullptr);
	mScene->SetRequireCompleteAssets(static_cast<bool>(mRenderFramebuffer));
	mScene->SetRequireGeneratedMeshes(mRenderFramebuffer && mViewType != swViewType::PartsList);
	mScene->SetLODDistance(Preferences.mMeshLODDistance);

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

				DrawBackground(CurrentTileRow, TotalTileRows, CurrentTileHeight);

				mContext->SetViewport(0, 0, CurrentTileWidth, CurrentTileHeight);
				mContext->SetProjectionMatrix(GetTileProjectionMatrix(CurrentTileRow, CurrentTileColumn, CurrentTileWidth, CurrentTileHeight));
			}
			else
			{
				CurrentTileWidth = mWidth;
				CurrentTileHeight = mHeight;

				DrawBackground(CurrentTileRow, TotalTileRows, CurrentTileHeight);

				mContext->SetProjectionMatrix(GetProjectionMatrix());
			}

			mContext->SetLineWidth(RenderSettings.LineWidth);

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
				
				DrawBackground(CurrentTileRow, TotalTileRows, CurrentTileHeight);
				
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
		mViewSphere->Draw();

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

void swView::DrawBackground(int CurrentTileRow, int TotalTileRows, int CurrentTileHeight) const
{
	if (mOverrideBackgroundColor)
	{
		swVector4 BackgroundColor(swVector4FromColor(mBackgroundColor));
		mContext->ClearColorAndDepth(BackgroundColor);
		return;
	}

	const swPreferences& Preferences = swGetPreferences();
	float Alpha = mContext->IsOffscreen() ? 0.0f : 1.0f;

	if (!Preferences.mBackgroundGradient)
	{
		swVector4 BackgroundColor(swVector3FromColor(Preferences.mBackgroundSolidColor), Alpha);
		mContext->ClearColorAndDepth(BackgroundColor);
		return;
	}

	mContext->ClearDepth();

	mContext->SetDepthWrite(false);
	mContext->EnableDepthTest(false);

	float ViewWidth = (float)mWidth;
	float ViewHeight = (float)mHeight;

	mContext->SetWorldMatrix(swMatrix44Identity());
	mContext->SetViewMatrix(swMatrix44Identity());
	mContext->SetProjectionMatrix(swMatrix44Orthographic(0.0f, ViewWidth, 0.0f, ViewHeight, -1.0f, 1.0f));

	const int TotalHeight = TotalTileRows == 1 ? mHeight : mRenderImage.height();
	const quint32 TopY = CurrentTileRow * mHeight + CurrentTileHeight;

	const double t1 = 1.0 - (double)TopY / (double)TotalHeight;
	const double t2 = 1.0 - (double)(TopY - CurrentTileHeight) / (double)TotalHeight;

	const quint32 ColorTop = Preferences.mBackgroundGradientColorTop;
	const quint32 ColorBottom = Preferences.mBackgroundGradientColorBottom;

	double TopRed = SW_RGBA_RED(ColorTop);
	double TopGreen = SW_RGBA_GREEN(ColorTop);
	double TopBlue = SW_RGBA_BLUE(ColorTop);
	double BottomRed = SW_RGBA_RED(ColorBottom);
	double BottomGreen = SW_RGBA_GREEN(ColorBottom);
	double BottomBlue = SW_RGBA_BLUE(ColorBottom);
	const double DeltaRed = BottomRed - TopRed;
	const double DeltaGreen = BottomGreen - TopGreen;
	const double DeltaBlue = BottomBlue - TopBlue;

	BottomRed = TopRed + DeltaRed * t2;
	BottomGreen = TopGreen + DeltaGreen * t2;
	BottomBlue = TopBlue + DeltaBlue * t2;

	TopRed = TopRed + DeltaRed * t1;
	TopGreen = TopGreen + DeltaGreen * t1;
	TopBlue = TopBlue + DeltaBlue * t1;

	const quint32 Color1 = SW_RGBA(TopRed, TopGreen, TopBlue, Alpha);
	const quint32 Color2 = SW_RGBA(BottomRed, BottomGreen, BottomBlue, Alpha);

	struct swBackgroundVertex
	{
		float x, y;
		quint32 Color;
	};

	const swBackgroundVertex Verts[4] =
	{
		{ ViewWidth, ViewHeight, Color1 }, { 0.0f, ViewHeight, Color1 }, { 0.0f, 0.0f, Color2 }, { ViewWidth, 0.0f, Color2 }
	};

	mContext->SetMaterial(swMaterialType::UnlitVertexColor);
	mContext->SetVertexBufferPointer(Verts);
	mContext->SetVertexFormat(0, 2, 0, 0, 4, false);

	mContext->DrawPrimitives(GL_TRIANGLE_FAN, 0, 4);

	mContext->EnableDepthTest(true);
	mContext->SetDepthWrite(true);
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

		case swViewType::Minifig:
		case swViewType::PartsList:
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

	swMatrix44 TranslationMatrix;
	float Scale = GetUIScale();

	switch (Preferences.mAxisIconLocation)
	{
	default:
	case swAxisIconLocation::BottomLeft:
		TranslationMatrix = swMatrix44Translation(swVector3(32, 32, 0.0f) / Scale);
		break;

	case swAxisIconLocation::BottomRight:
		TranslationMatrix = swMatrix44Translation(swVector3(mWidth - 36, 32, 0.0f) / Scale);
		break;

	case swAxisIconLocation::TopLeft:
		TranslationMatrix = swMatrix44Translation(swVector3(32, mHeight - 36, 0.0f) / Scale);
		break;

	case swAxisIconLocation::TopRight:
		TranslationMatrix = swMatrix44Translation(swVector3(mWidth - 36, mHeight - 36, 0.0f) / Scale);
		break;
	}

	swMatrix44 WorldViewMatrix = mCamera->mWorldView;
	WorldViewMatrix.SetTranslation(swVector3(0, 0, 0));

	mContext->SetLineWidth(1.0f);
	mContext->SetMaterial(swMaterialType::UnlitVertexColor);
	mContext->SetWorldMatrix(swMatrix44Identity());
	mContext->SetViewMatrix(swMultiply(WorldViewMatrix, TranslationMatrix));
	mContext->SetProjectionMatrix(swMatrix44Orthographic(0, mWidth / Scale, 0, mHeight / Scale, -50, 50));

	mContext->SetVertexBufferPointer(Verts);
	mContext->SetVertexFormat(0, 3, 0, 0, 4, false);
	mContext->SetIndexBufferPointer(Indices);

	mContext->DrawIndexedPrimitives(GL_LINES, 6, GL_UNSIGNED_SHORT, 0);
	mContext->DrawIndexedPrimitives(GL_TRIANGLES, 72, GL_UNSIGNED_SHORT, 6 * 2);

	if (!gTexFont.IsLoaded())
		return;

	mContext->SetMaterial(swMaterialType::TextSDF);
	mContext->SetViewMatrix(TranslationMatrix);
	mContext->BindTexture2D(gTexFont.GetTexture());
	mContext->EnableColorBlend(true);

	float TextBuffer[6 * 5 * 3];
	swVector3 PosX = swMultiply30(swVector3(25.0f, 0.0f, 0.0f), WorldViewMatrix);
	gTexFont.GetGlyphTriangles(PosX.x, PosX.y, PosX.z, 'X', TextBuffer);
	swVector3 PosY = swMultiply30(swVector3(0.0f, 25.0f, 0.0f), WorldViewMatrix);
	gTexFont.GetGlyphTriangles(PosY.x, PosY.y, PosY.z, 'Y', TextBuffer + 5 * 6);
	swVector3 PosZ = swMultiply30(swVector3(0.0f, 0.0f, 25.0f), WorldViewMatrix);
	gTexFont.GetGlyphTriangles(PosZ.x, PosZ.y, PosZ.z, 'Z', TextBuffer + 5 * 6 * 2);

	mContext->SetVertexBufferPointer(TextBuffer);
	mContext->SetVertexFormat(0, 3, 0, 2, 0, false);

	mContext->SetColor(swVector4FromColor(swGetPreferences().mTextColor));
	mContext->SetTextHaloColor(swVector4FromColor(swGetPreferences().mTextHaloColor));
	mContext->DrawPrimitives(GL_TRIANGLES, 0, 6 * 3);

	mContext->EnableColorBlend(false);
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

	bool GridSizeValid = mModel->GetVisiblePiecesBoundingBox(Min, Max);
	bool DrawInsertPreview = std::any_of(mViews.begin(), mViews.end(), [](swView* View){ return View->mTrackTool == swTrackTool::Insert; });

	if (DrawInsertPreview)
	{
		const swModel* PreviewModel = GetActiveModel();

		for (const swPreviewInsertPieceInformation& PreviewInsertPieceInfo : PreviewModel->GetPreviewInsertPieceInfo())
		{
			swVector3 Points[8];
			swGetBoxCorners(PreviewInsertPieceInfo.PieceData->GetBoundingBox(), Points);
			swMatrix44 PreviewTransform = PreviewInsertPieceInfo.Transform;

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

	if (Preferences.mDrawGridHeightXZ || Preferences.mDrawGridHeightYZ)
	{
		// Vertical grids through the origin axes, or behind the model, with a line every plate height.
		constexpr float PlateHeight = 8.0f;
		const float LineSpacing = 20.0f * Spacing;
		const float Left = MinX * LineSpacing;
		const float Right = MaxX * LineSpacing;
		const float Front = MinY * LineSpacing;
		const float Back = MaxY * LineSpacing;
		const int PlateCount = swMaximum(static_cast<int>(ceilf((GridSizeValid && std::isfinite(Max[2]) ? Max[2] : 0.0f) / PlateHeight)) + 3, 9);
		const float Top = PlateCount * PlateHeight;
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
		struct swGridVertex
		{
			float x, y;
			quint32 Color;
		};

		const quint32 Red = Preferences.mAxisXColor;
		const quint32 Green = Preferences.mAxisYColor;
		const float Scale = 20.0f * Spacing;

		const swGridVertex Verts[4] =
		{
			{ 0.0f, MinY * Scale, Green }, { 0.0f, MaxY * Scale, Green }, { MinX * Scale, 0.0f, Red }, { MaxX * Scale, 0.0f, Red }
		};

		mContext->SetMaterial(swMaterialType::UnlitVertexColor);
		mContext->SetVertexBufferPointer(Verts);
		mContext->SetVertexFormat(0, 2, 0, 0, 4, false);

		mContext->DrawPrimitives(GL_LINES, 0, 4);
	}
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

		case swDragState::Piece:
			ActiveModel->InsertPieceToolClicked();
			break;

		case swDragState::Color:
			ActiveModel->PaintToolClicked(FindObjectUnderPointer(true, false).Object);
			break;
		}
	}

	mDragState = swDragState::None;
	UpdateTrackTool();

	ActiveModel->SetPreviewInsertPieceInfo(std::vector<swInsertPieceInformation>());
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

void swView::ZoomExtents(swGeometryBoundsMode Mode, bool AdjustFarPlane)
{
	swModel* ActiveModel = GetActiveModel();

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
		swCursor::Default,          // swTrackTool::MoveX
		swCursor::Default,          // swTrackTool::MoveY
		swCursor::Default,          // swTrackTool::MoveZ
		swCursor::Default,          // swTrackTool::MoveXY
		swCursor::Default,          // swTrackTool::MoveXZ
		swCursor::Default,          // swTrackTool::MoveYZ
		swCursor::Default,          // swTrackTool::MoveXYZ
		swCursor::Default,          // swTrackTool::RotateX
		swCursor::Default,          // swTrackTool::RotateY
		swCursor::Default,          // swTrackTool::RotateZ
		swCursor::Default,          // swTrackTool::RotateTrackBall
		swCursor::Default,          // swTrackTool::RotateCamera
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

void swView::SetCursor(swCursor CursorType)
{
	if (mCursor == CursorType)
		return;

	struct swCursorInformation
	{
		int x, y;
		const char* Name;
	};

	constexpr swCursorInformation Cursors[] =
	{
		{  0,  0, "" },                                 // swCursor::Hidden
		{  0,  0, "" },                                 // swCursor::Default
		{  8,  3, ":/resources/cursor_insert" },        // swCursor::Brick
		{ 15, 15, ":/resources/cursor_light" },         // swCursor::PointLight
		{  7, 10, ":/resources/cursor_spotlight" },     // swCursor::SpotLight
		{ 15, 15, ":/resources/cursor_sunlight" },      // swCursor::DirectionalLight
		{ 15, 15, ":/resources/cursor_arealight" },     // swCursor::AreaLight
		{ 15,  9, ":/resources/cursor_camera" },        // swCursor::Camera
		{  0,  2, ":/resources/cursor_select" },        // swCursor::Select
		{  0,  2, ":/resources/cursor_select_add" },    // swCursor::SelectAdd
		{  0,  2, ":/resources/cursor_select_remove" }, // swCursor::SelectRemove
		{ 15, 15, ":/resources/cursor_move" },          // swCursor::Move
		{ 15, 15, ":/resources/cursor_rotate" },        // swCursor::Rotate
		{ 15, 15, ":/resources/cursor_rotatex" },       // swCursor::RotateX
		{ 15, 15, ":/resources/cursor_rotatey" },       // swCursor::RotateY
		{  0, 10, ":/resources/cursor_delete" },        // swCursor::Delete
		{ 14, 14, ":/resources/cursor_paint" },         // swCursor::Paint
		{  1, 13, ":/resources/cursor_color_picker" },  // swCursor::ColorPicker
		{ 15, 15, ":/resources/cursor_zoom" },          // swCursor::Zoom
		{  9,  9, ":/resources/cursor_zoom_region" },   // swCursor::ZoomRegion
		{ 15, 15, ":/resources/cursor_pan" },           // swCursor::Pan
		{ 15, 15, ":/resources/cursor_roll" },          // swCursor::Roll
		{ 15, 15, ":/resources/cursor_rotate_view" },   // swCursor::RotateView
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
		mWidget->setCursor(QCursor(QPixmap(Cursor.Name), Cursor.x, Cursor.y));
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
	if (mPickingFaceNormal)
	{
		mWidget->setCursor(Qt::CrossCursor);
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
		// The hit triangle normal is in the space of the innermost piece, so bring it to the view.
		swVector3 Point = ObjectRayTest.Start + swNormalize(ObjectRayTest.End - ObjectRayTest.Start) * ObjectRayTest.Distance;
		swVector3 Normal = swNormalize(swMultiply30(ObjectRayTest.PieceRayTest.Normal, ObjectRayTest.PieceRayTest.Transform));

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
	mMouseDownPiece = nullptr;
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

				if (Object && Object->IsPiece() && ObjectSection.Section == SW_PIECE_SECTION_POSITION && Object->IsSelected())
				{
					swPiece* Piece = (swPiece*)Object;
					mMouseDownTransform = Piece->mModelWorld;
					mMouseDownPiece = Piece;
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

	case swDragState::Piece:
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
	else if (mTrackButton != swTrackButton::None)
		StopTracking(false);
	else
	{
		swModel* ActiveModel = GetActiveModel();
		if (ActiveModel)
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
			UpdatePiecePreview();

			if (!ActiveModel->InsertPieceToolClicked())
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

			if (!Focus || !Focus->IsPiece())
				break;

			swPiece* Piece = (swPiece*)Focus;
			const swTrainTrackInformation* TrainTrackInfo = Piece->mPieceData->GetTrainTrackInfo();

			if (!TrainTrackInfo)
				break;

			if (mTrackToolSection < SW_PIECE_SECTION_TRAIN_TRACK_CONNECTION_FIRST)
				break;

			swObjectSection ObjectSection;

			ObjectSection.Object = Focus;
			ObjectSection.Section = mTrackToolSection;

			ActiveModel->SetSelectionAndFocusAction(std::vector<swObject*>(), ObjectSection.Object, ObjectSection.Section, gMainWindow->GetSelectionMode());
		}
		break;

	case swTrackTool::ScalePlus:
	case swTrackTool::ScaleMinus:
		if (ActiveModel->AnyPiecesSelected())
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

	if (mTrackButton != swTrackButton::None)
	{
		StopTracking(false);
		return;
	}

	if (mViewSphere->OnLeftButtonDown())
		return;

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

	if (mTrackButton == swTrackButton::None)
	{
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
			UpdatePiecePreview();
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
			else if (mTrackTool == swTrackTool::MoveXYZ && mMouseDownPiece)
			{
				if (mMouseDownPiece->mPieceData->GetTrainTrackInfo())
					ActiveModel->UpdateSelectedPiecesTrainTrackConnections();

				auto [PieceInfoTransforms, IsConnection] = GetMouseInsertPieceInfo(true, false, mMouseDownPiece->mPieceData, mMouseDownPiece);

				if (!PieceInfoTransforms.empty())
				{
					ActiveModel->UpdateFreeMoveTool(mMouseDownPiece, mMouseDownTransform, PieceInfoTransforms.back().Transform, IsConnection, mTrackButton != swTrackButton::Left);
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
				if (Focus && Focus->IsPiece())
				{
					swPiece* Piece = (swPiece*)Focus;
					quint32 Section = Piece->GetFocusSection();

					if (Section >= SW_PIECE_SECTION_CONTROL_POINT_FIRST && Section <= SW_PIECE_SECTION_CONTROL_POINT_LAST)
					{
						const float ScaleMax = 200.0f;
						const float OverlayScale = GetOverlayScale();
						const float ScaleStart = 2.0f * OverlayScale;

						swVector3 Position = Piece->GetSectionPosition(Section);
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
		if (mViewType != swViewType::Minifig)
			ActiveModel->UpdateOrbitTool(mCamera, 0.0f, 0.1f * MouseSensitivity * (mMouseY - mMouseDownY));
		break;

	case swTrackTool::OrbitXY:
		ActiveModel->UpdateOrbitTool(mCamera, 0.1f * MouseSensitivity * (mMouseX - mMouseDownX), mViewType == swViewType::Minifig ? 0.0f : 0.1f * MouseSensitivity * (mMouseY - mMouseDownY));

		// Keep the whole minifigure in view while it turns.
		if (mViewType == swViewType::Minifig)
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
	if (mTrackTool == swTrackTool::Insert)
	{
		mTrackTool = swTrackTool::None;

		UpdateCursor();

		swModel* ActiveModel = GetActiveModel();

		if (ActiveModel)
			ActiveModel->SetPreviewInsertPieceInfo(std::vector<swInsertPieceInformation>());
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
