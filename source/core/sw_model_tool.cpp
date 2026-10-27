#include "sw_global.h"
#include "sw_model.h"
#include "sw_model_history.h"
#include "sw_object_piece.h"
#include "sw_object_camera.h"
#include "sw_object_light.h"
#include "sw_group.h"
#include "sw_project.h"
#include "sw_window_main.h"
#include "sw_profile.h"
#include "sw_library.h"
#include "sw_scene.h"
#include "sw_train_track.h"
#include "sw_file.h"
#include "sw_piece_data.h"
#include "sw_view.h"
#include "sw_minifigure_builder.h"
#include "sw_dialog_array.h"
#include "sw_dialog_group.h"
#include "sw_dialog_edit_groups.h"
#include "sw_dialog_properties.h"
#include "sw_window_utilities.h"
#include "sw_lxf.h"
#include "sw_widget_preview.h"
#include "sw_string.h"
#include <locale.h>

void swModel::UndoAction()
{
	if (mUndoHistory.empty())
		return;

	std::unique_ptr<swModelHistoryEntry> Undo = std::move(mUndoHistory.front());

	RunHistorySequence(Undo->HistorySequence, false);

	mUndoHistory.erase(mUndoHistory.begin());
	mRedoHistory.insert(mRedoHistory.begin(), std::move(Undo));

	gMainWindow->UpdateModified(mProject ? mProject->IsModified() : IsModified());
	gMainWindow->UpdateUndoRedo(!mUndoHistory.empty() ? mUndoHistory.front()->Description : nullptr, !mRedoHistory.empty() ? mRedoHistory.front()->Description : nullptr);
}

void swModel::RedoAction()
{
	if (mRedoHistory.empty())
		return;

	std::unique_ptr<swModelHistoryEntry> Redo = std::move(mRedoHistory.front());

	RunHistorySequence(Redo->HistorySequence, true);

	mRedoHistory.erase(mRedoHistory.begin());
	mUndoHistory.insert(mUndoHistory.begin(), std::move(Redo));

	gMainWindow->UpdateModified(mProject ? mProject->IsModified() : IsModified());
	gMainWindow->UpdateUndoRedo(!mUndoHistory.empty() ? mUndoHistory.front()->Description : nullptr, !mRedoHistory.empty() ? mRedoHistory.front()->Description : nullptr);
}

void swModel::BeginMouseTool(swTool Tool, swView* View)
{
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
			BeginHistorySequence();
			BeginEditHistory();
			break;

		case swTool::Eraser:
		case swTool::Paint:
		case swTool::ColorPicker:
			break;

		case swTool::Pan:
		case swTool::Zoom:
		case swTool::RotateView:
		case swTool::Roll:
			if (!View->GetCamera()->IsSimple())
			{
				BeginHistorySequence();
				BeginEditHistory();
			}
			break;

		case swTool::ZoomRegion:
			break;

		case swTool::Count:
			break;
	}

	mMouseToolDistance = swVector3(0.0f, 0.0f, 0.0f);
	mMouseToolMoveStartPosition = swVector3(0.0f, 0.0f, 0.0f);
	mMouseToolMoveEndPosition = swVector3(0.0f, 0.0f, 0.0f);
	mMouseToolMovePositionValid = false;
	mMouseToolMoveUsesFocus = false;

	if (Tool == swTool::Move)
	{
		swObject* Focus = GetFocusObject();

		if (Focus && Focus->GetFocusSection() != SW_OBJECT_SECTION_INVALID)
		{
			mMouseToolMoveStartPosition = Focus->GetSectionPosition(Focus->GetFocusSection());
			mMouseToolMovePositionValid = true;
			mMouseToolMoveUsesFocus = true;
		}
		else
		{
			swMatrix33 RelativeRotation;
			mMouseToolMovePositionValid = GetMoveRotateTransform(mMouseToolMoveStartPosition, RelativeRotation);
		}

		mMouseToolMoveEndPosition = mMouseToolMoveStartPosition;
	}

	mMouseToolFirstMove = true;
}

void swModel::EndMouseTool(swTool Tool, swView* View, bool Accept)
{
	if (!Accept)
	{
		RevertHistorySequence();
		return;
	}

	const swCamera* Camera = View->GetCamera();

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
		EndEditHistory();
		EndHistorySequence(tr("Move"));
		break;

	case swTool::Rotate:
		EndEditHistory();
		EndHistorySequence(tr("Rotate"));
		break;

	case swTool::Eraser:
	case swTool::Paint:
	case swTool::ColorPicker:
		break;

	case swTool::Zoom:
		if (!Camera->IsSimple())
		{
			EndEditHistory();
			EndHistorySequence(tr("Zoom"));
		}
		break;

	case swTool::Pan:
		if (!Camera->IsSimple())
		{
			EndEditHistory();
			EndHistorySequence(tr("Pan"));
		}
		break;

	case swTool::RotateView:
		if (!Camera->IsSimple())
		{
			EndEditHistory();
			EndHistorySequence(tr("Orbit"));
		}
		break;

	case swTool::Roll:
		if (!Camera->IsSimple())
		{
			EndEditHistory();
			EndHistorySequence(tr("Roll"));
		}
		break;

	case swTool::ZoomRegion:
		break;

	case swTool::Count:
		break;
	}
}

bool swModel::InsertPieceToolClicked()
{
	if (mPreviewInsertPieceInfo.empty())
		return false;

	BeginHistorySequence();
	BeginEditHistory();

	swPiece* Piece = nullptr;

	for (const swPreviewInsertPieceInformation& InsertPieceInfo : mPreviewInsertPieceInfo)
	{
		Piece = new swPiece(InsertPieceInfo.PieceData);

		Piece->Initialize(InsertPieceInfo.Transform, mCurrentStep);
		Piece->SetColorIndex(InsertPieceInfo.ColorIndex);
		Piece->UpdatePosition(mCurrentStep);

		AddPiece(Piece);
	}

	SetPreviewInsertPieceInfo(std::vector<swInsertPieceInformation>());

	EndEditHistory();

	gMainWindow->SetLastInsertedColorIndex(Piece->GetColorIndex());

	SetSelectionAndFocus(std::vector<swObject*>(), Piece, SW_PIECE_SECTION_POSITION, swSelectionMode::Single);

	EndHistorySequence(tr("Add Piece"));

	gMainWindow->UpdateTimeline(false, false);
	gMainWindow->UpdateInUseCategory();

	UpdateTrainTrackConnections(Piece, false);

	return true;
}

void swModel::InsertCameraToolClicked(const swVector3& Position)
{
	BeginHistorySequence();
	BeginEditHistory();

	swCamera* Camera = new swCamera(false, Position, GetSelectionOrModelCenter());

	Camera->CreateName(mCameras);
	mCameras.emplace_back(Camera);

	EndEditHistory();

	SetSelectionAndFocus(std::vector<swObject*>(), Camera, SW_CAMERA_SECTION_POSITION, swSelectionMode::Single);

	EndHistorySequence(tr("Add Camera"));
}

void swModel::InsertLightToolClicked(const swVector3& Position, swLightType LightType)
{
	QString ActionName;

	switch (LightType)
	{
	case swLightType::Point:
		ActionName = tr("Add Point Light");
		break;

	case swLightType::Spot:
		ActionName = tr("Add Spotlight");
		break;

	case swLightType::Directional:
		ActionName = tr("Add Directional Light");
		break;

	case swLightType::Area:
		ActionName = tr("Add Area Light");
		break;

	case swLightType::Count:
		return;
	}

	BeginHistorySequence();
	BeginEditHistory();

	swLight* Light = new swLight(Position, LightType);

	Light->CreateName(mLights);
	mLights.emplace_back(Light);

	EndEditHistory();

	SetSelectionAndFocus(std::vector<swObject*>(), Light, SW_LIGHT_SECTION_POSITION, swSelectionMode::Single);

	EndHistorySequence(ActionName);
}

void swModel::UpdateMoveTool(const swVector3& Distance, bool AllowRelative, bool AlternateButtonDrag)
{
	const swVector3 PieceDistance = SnapPosition(Distance) - SnapPosition(mMouseToolDistance);
	const swVector3 ObjectDistance = Distance - mMouseToolDistance;

	swModelTransformFlags Flags = (AllowRelative ? swModelTransformFlag::Relative : swModelTransformFlag::None) | (AlternateButtonDrag ? swModelTransformFlag::RotatePivotPoint : swModelTransformFlag::None) | (mMouseToolFirstMove ? swModelTransformFlag::FirstMove : swModelTransformFlag::None);
	MoveSelectedObjects(PieceDistance, ObjectDistance, Flags, swModelHistoryEditMerge::None);

	if (mMouseToolMovePositionValid)
	{
		if (mMouseToolMoveUsesFocus)
		{
			swObject* Focus = GetFocusObject();

			if (Focus && Focus->GetFocusSection() != SW_OBJECT_SECTION_INVALID)
				mMouseToolMoveEndPosition = Focus->GetSectionPosition(Focus->GetFocusSection());
			else
				mMouseToolMovePositionValid = false;
		}
		else
		{
			swMatrix33 RelativeRotation;
			mMouseToolMovePositionValid = GetMoveRotateTransform(mMouseToolMoveEndPosition, RelativeRotation);
		}
	}

	mMouseToolDistance = Distance;
	mMouseToolFirstMove = false;

	gMainWindow->UpdateSelectedObjects(false);
	UpdateAllViews();
}

void swModel::UpdateFreeMoveTool(swPiece* MousePiece, const swMatrix44& StartTransform, const swMatrix44& NewTransform, bool IsConnection, bool AlternateButtonDrag)
{
	swMatrix33 NewRotation = IsConnection ? swMatrix33(NewTransform) : swMatrix33(StartTransform);
	swMatrix33 CurrentRotation = swMatrix33(MousePiece->mModelWorld);

	if (!swMatrix33Similar(CurrentRotation, NewRotation))
	{
		const swVector3 Distance = NewTransform.GetTranslation() - MousePiece->mModelWorld.GetTranslation();

		swMatrix33 RotationMatrix = swMultiply(swMatrix33AffineInverse(CurrentRotation), NewRotation);

		const swVector3& Center = NewTransform.GetTranslation();
		swMatrix33 RelativeRotation = MousePiece->GetRelativeRotation();

		swMatrix33 WorldToFocusMatrix = swMatrix33AffineInverse(RelativeRotation);
		// Rotate() expects the rotation in the focus frame.
		RotationMatrix = swMultiply(RelativeRotation, RotationMatrix);

		int Flags;
		std::vector<swObject*> Selection;
		swObject* Focus;

		GetSelectionInformation(&Flags, Selection, &Focus);

		for (swObject* Object : Selection)
		{
			if (Object->IsPiece())
			{
				swPiece* Piece = (swPiece*)Object;

				Piece->MoveSelected(mCurrentStep, gMainWindow->GetAddKeys(), Distance);
				Piece->UpdatePosition(mCurrentStep);

				Piece->Rotate(mCurrentStep, gMainWindow->GetAddKeys(), RotationMatrix, Center, WorldToFocusMatrix);
				Piece->UpdatePosition(mCurrentStep);
			}
			else if (Object->IsCamera())
			{
				swCamera* Camera = (swCamera*)Object;

				Camera->MoveSelected(mCurrentStep, gMainWindow->GetAddKeys(), Distance);
				Camera->UpdatePosition(mCurrentStep);

				Camera->Rotate(mCurrentStep, gMainWindow->GetAddKeys(), RotationMatrix, Center, WorldToFocusMatrix);
				Camera->UpdatePosition(mCurrentStep);
			}
			else if (Object->IsLight())
			{
				swLight* Light = (swLight*)Object;

				Light->MoveSelected(mCurrentStep, gMainWindow->GetAddKeys(), Distance, mMouseToolFirstMove);
				Light->UpdatePosition(mCurrentStep);

				Light->Rotate(mCurrentStep, gMainWindow->GetAddKeys(), RotationMatrix, Center, WorldToFocusMatrix);
				Light->UpdatePosition(mCurrentStep);
			}
		}

		mMouseToolDistance = NewTransform.GetTranslation() - StartTransform.GetTranslation();
		mMouseToolFirstMove = false;

		gMainWindow->UpdateSelectedObjects(false);
		UpdateAllViews();
	}
	else
	{
		swVector3 Distance = NewTransform.GetTranslation() - StartTransform.GetTranslation();

		UpdateMoveTool(Distance, false, AlternateButtonDrag);
	}
}

void swModel::UpdateRotateTool(const swVector3& Axis, float Angle, bool AlternateButtonDrag, bool Relative)
{
	const swVector3 Angles(Angle, Angle, Angle);
	const float Delta = SnapRotation(Angles)[0] - SnapRotation(mMouseToolDistance)[0];

	if (Delta != 0.0f)
		RotateSelectedObjects(swMatrix33FromAxisAngle(Axis, Delta * SW_DEGREES_TO_RADIANS), (Relative ? swModelTransformFlag::Relative : swModelTransformFlag::None) | (AlternateButtonDrag ? swModelTransformFlag::RotatePivotPoint : swModelTransformFlag::None), swModelHistoryEditMerge::None);

	mMouseToolDistance = Angles;
	mMouseToolFirstMove = false;
}

void swModel::UpdateRotateTool(const swMatrix33& RotationMatrix, bool AlternateButtonDrag, bool Relative)
{
	RotateSelectedObjects(RotationMatrix, (Relative ? swModelTransformFlag::Relative : swModelTransformFlag::None) | (AlternateButtonDrag ? swModelTransformFlag::RotatePivotPoint : swModelTransformFlag::None), swModelHistoryEditMerge::None);
	mMouseToolFirstMove = false;
}

void swModel::UpdateScaleTool(const float Scale)
{
	ScaleSelectedPieces(Scale);

	gMainWindow->UpdateSelectedObjects(false);
	UpdateAllViews();
}

void swModel::EraserToolClicked(swObject* Object)
{
	if (!Object)
		return;

	BeginHistorySequence();
	BeginEditHistory();

	switch (Object->GetType())
	{
	case swObjectType::Piece:
		for (auto PieceIt = mPieces.begin(); PieceIt != mPieces.end(); ++PieceIt)
			if (PieceIt->get() == Object)
			{
				mPieces.erase(PieceIt);
				RemoveEmptyGroups();
				break;
			}
		break;

	case swObjectType::Camera:
		{
			std::vector<swView*> Views = swView::GetModelViews(this);

			for (swView* View : Views)
			{
				swCamera* Camera = View->GetCamera();

				if (Camera == Object)
					View->SetCamera(Camera, true);
			}

			for (std::vector<std::unique_ptr<swCamera>>::iterator CameraIt = mCameras.begin(); CameraIt != mCameras.end(); CameraIt++)
			{
				if (CameraIt->get() == Object)
				{
					mCameras.erase(CameraIt);
					break;
				}
			}
		}
		break;

	case swObjectType::Light:
		for (std::vector<std::unique_ptr<swLight>>::iterator LightIt = mLights.begin(); LightIt != mLights.end(); LightIt++)
		{
			if (LightIt->get() == Object)
			{
				mLights.erase(LightIt);
				break;
			}
		}
		break;
	}

	EndEditHistory();
	EndHistorySequence(tr("Delete"));

	gMainWindow->UpdateTimeline(false, false);
	gMainWindow->UpdateSelectedObjects(true);
}

void swModel::PaintToolClicked(swObject* Object)
{
	if (!Object || !Object->IsPiece())
		return;

	swPiece* Piece = (swPiece*)Object;

	if (Piece->GetColorIndex() != gMainWindow->mColorIndex)
	{
		BeginHistorySequence();
		BeginEditHistory();

		Piece->SetColorIndex(gMainWindow->mColorIndex);

		EndEditHistory();
		EndHistorySequence(tr("Paint"));

		gMainWindow->UpdateSelectedObjects(false);
		gMainWindow->UpdateTimeline(false, true);
	}
}

void swModel::ColorPickerToolClicked(const swObject* Object)
{
	if (!Object || !Object->IsPiece())
		return;

	const swPiece* Piece = (swPiece*)Object;

	gMainWindow->SetColorIndex(Piece->GetColorIndex());
}

void swModel::UpdateZoomTool(swCamera* Camera, float Mouse)
{
	Camera->Zoom(Mouse - mMouseToolDistance.x, mCurrentStep, gMainWindow->GetAddKeys());
	mMouseToolDistance.x = Mouse;

	UpdateAllViews();
}

void swModel::UpdatePanTool(swCamera* Camera, const swVector3& Distance)
{
	Camera->Pan(Distance, mCurrentStep, gMainWindow->GetAddKeys());

	UpdateAllViews();
}

void swModel::UpdateOrbitTool(swCamera* Camera, float MouseX, float MouseY)
{
	swVector3 Center;
	GetSelectionCenter(Center);

	// With orbit snapping the camera turns in whole steps of the rotation snap angle.
	const float OrbitSnap = gMainWindow->GetOrbitSnap() * SW_DEGREES_TO_RADIANS;

	if (OrbitSnap > 0.0f)
	{
		MouseX = roundf(MouseX / OrbitSnap) * OrbitSnap;
		MouseY = roundf(MouseY / OrbitSnap) * OrbitSnap;
	}

	Camera->Orbit(MouseX - mMouseToolDistance.x, MouseY - mMouseToolDistance.y, Center, mCurrentStep, gMainWindow->GetAddKeys());
	mMouseToolDistance.x = MouseX;
	mMouseToolDistance.y = MouseY;

	UpdateAllViews();
}

void swModel::UpdateRollTool(swCamera* Camera, float Mouse)
{
	Camera->Roll(Mouse - mMouseToolDistance.x, mCurrentStep, gMainWindow->GetAddKeys());
	mMouseToolDistance.x = Mouse;

	UpdateAllViews();
}

void swModel::ZoomRegionToolClicked(swView* View, float AspectRatio, const swVector3& Position, const swVector3& TargetPosition, const swVector3* Corners)
{
	swCamera* Camera = View->GetCamera();

	if (!Camera->IsSimple())
	{
		BeginHistorySequence();
		BeginEditHistory();
	}

	Camera->ZoomRegion(AspectRatio, Position, TargetPosition, Corners, mCurrentStep, gMainWindow->GetAddKeys());

	if (!Camera->IsSimple())
	{
		EndEditHistory();
		EndHistorySequence(tr("Zoom"));
	}

	gMainWindow->UpdateSelectedObjects(false);
	UpdateAllViews();
}

void swModel::LookAt(swCamera* Camera)
{
	swVector3 Center;

	if (!GetSelectionCenter(Center))
	{
		swVector3 Min(FLT_MAX, FLT_MAX, FLT_MAX), Max(-FLT_MAX, -FLT_MAX, -FLT_MAX);

		if (GetVisiblePiecesBoundingBox(Min, Max))
			Center = (Min + Max) / 2.0f;
		else
			Center = swVector3(0.0f, 0.0f, 0.0f);
	}

	if (!Camera->IsSimple())
	{
		BeginHistorySequence();
		BeginEditHistory();
	}

	Camera->Center(Center, mCurrentStep, gMainWindow->GetAddKeys());

	gMainWindow->UpdateSelectedObjects(false);
	UpdateAllViews();

	if (!Camera->IsSimple())
	{
		EndEditHistory();
		EndHistorySequence(tr("Look At"));
	}
}

void swModel::MoveCamera(swCamera* Camera, const swVector3& Direction)
{
	if (!Camera->IsSimple())
	{
		BeginHistorySequence();
		BeginMergeableEditHistory(swModelHistoryEditMerge::KeyboardMoveCamera, swObjectPropertyId::Count);
	}

	Camera->MoveRelative(Direction, mCurrentStep, gMainWindow->GetAddKeys());

	gMainWindow->UpdateSelectedObjects(false);
	UpdateAllViews();

	if (!Camera->IsSimple())
	{
		EndEditHistory();
		EndHistorySequence(tr("Move"));
	}
}

void swModel::ZoomExtents(swCamera* Camera, float Aspect, const swMatrix44& WorldMatrix, swGeometryBoundsMode Mode, bool AdjustFarPlane)
{
	std::vector<swVector3> Points = GetPiecesBoundingBoxPoints(Mode);

	if (Points.empty())
		return;

	swVector3 Min(FLT_MAX, FLT_MAX, FLT_MAX), Max(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	for (swVector3& Point : Points)
	{
		Point = swMultiply31(Point, WorldMatrix);

		Min = swMinimum(Point, Min);
		Max = swMaximum(Point, Max);
	}

	const swVector3 Center = (Min + Max) / 2.0f;

	if (!Camera->IsSimple())
	{
		BeginHistorySequence();
		BeginEditHistory();
	}

	Camera->ZoomExtents(Aspect, Center, Points, mCurrentStep, gMainWindow ? gMainWindow->GetAddKeys() : false, AdjustFarPlane);

	if (!mIsPreview && gMainWindow)
		gMainWindow->UpdateSelectedObjects(false);

	UpdateAllViews();

	if (!Camera->IsSimple())
	{
		EndEditHistory();
		EndHistorySequence(tr("Zoom Extents"));
	}
}

void swModel::Zoom(swCamera* Camera, float Amount)
{
	if (!Camera->IsSimple())
	{
		BeginHistorySequence();
		BeginMergeableEditHistory(swModelHistoryEditMerge::KeyboardZoom, swObjectPropertyId::Count);
	}

	Camera->Zoom(Amount, mCurrentStep, gMainWindow->GetAddKeys());

	if (!mIsPreview)
		gMainWindow->UpdateSelectedObjects(false);

	UpdateAllViews();

	if (!Camera->IsSimple())
	{
		EndEditHistory();
		EndHistorySequence(tr("Zoom"));
	}
}
