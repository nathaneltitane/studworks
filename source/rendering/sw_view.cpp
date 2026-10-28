#include "sw_global.h"
#include "sw_view.h"
#include "sw_view_widget.h"
#include "sw_window_main.h"
#include "sw_object_camera.h"
#include "sw_texture.h"
#include "sw_object_part.h"
#include "sw_data_part.h"
#include "sw_track_train.h"
#include "sw_scene.h"
#include "sw_view_manipulator.h"
#include "sw_view_sphere.h"
#include "sw_widget_find_replace.h"
#include "sw_library.h"
#include "sw_widget_selection_part_popup.h"

swFindReplaceParameters swView::mFindReplaceParams;
QPointer<swFindReplaceWidget> swView::mFindWidget;

swView* swView::mLastFocusedView;
std::vector<swView*> swView::mViews;
swCameraProjection swView::mLastProjection = swCameraProjection::Perspective;

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

	// new model views and tabs open with the last chosen projection
	if (mViewType == swViewType::View && mCamera->IsSimple())
		mCamera->SetProjection(mLastProjection);
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

// use to fit views again once the parts they show have loaded
void swView::UpdateFittedViews()
{
	for (swView* View : mViews)
	{
		if (!View->IsFitted())
			continue;

		View->ZoomExtents(View->mZoomExtentsMode, View->mZoomExtentsFarPlane);
		View->Redraw();
	}
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
