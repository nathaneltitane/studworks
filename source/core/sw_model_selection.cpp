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

void swModel::RayTest(swObjectRayTest& ObjectRayTest) const
{
	for (const std::unique_ptr<swPiece>& Piece : mPieces)
		if ((Piece->IsSelected() || Piece->IsVisible(mCurrentStep)) && (!ObjectRayTest.IgnoreSelected || !Piece->IsSelected()))
			Piece->RayTest(ObjectRayTest);

	if (ObjectRayTest.PiecesOnly)
		return;

	for (const std::unique_ptr<swCamera>& Camera : mCameras)
		if (Camera.get() != ObjectRayTest.ViewCamera && Camera->IsVisible() && (!ObjectRayTest.IgnoreSelected || !Camera->IsSelected()))
			Camera->RayTest(ObjectRayTest);

	for (const std::unique_ptr<swLight>& Light : mLights)
		if (Light->IsVisible() && (!ObjectRayTest.IgnoreSelected || !Light->IsSelected()))
			Light->RayTest(ObjectRayTest);
}

void swModel::BoxTest(swObjectBoxTest& ObjectBoxTest) const
{
	for (const std::unique_ptr<swPiece>& Piece : mPieces)
		if (Piece->IsSelected() || Piece->IsVisible(mCurrentStep))
			Piece->BoxTest(ObjectBoxTest);

	for (const std::unique_ptr<swCamera>& Camera : mCameras)
		if (Camera.get() != ObjectBoxTest.ViewCamera && Camera->IsVisible())
			Camera->BoxTest(ObjectBoxTest);

	for (const std::unique_ptr<swLight>& Light : mLights)
		if (Light->IsVisible())
			Light->BoxTest(ObjectBoxTest);
}

bool swModel::SubModelMinIntersectDist(const swVector3& WorldStart, const swVector3& WorldEnd, float& MinDistance, swPieceRayTest& PieceRayTest) const
{
	bool MinIntersect = false;

	for (const std::unique_ptr<swPiece>& Piece : mPieces)
	{
		const swMatrix44 InverseWorldMatrix = swMatrix44AffineInverse(Piece->mModelWorld);
		const swVector3 Start = swMultiply31(WorldStart, InverseWorldMatrix);
		const swVector3 End = swMultiply31(WorldEnd, InverseWorldMatrix);

		if (Piece->IsVisibleInSubModel())
		{
			if (Piece->MinIntersectDist(Start, End, MinDistance, PieceRayTest))
			{
				MinIntersect = true;
				PieceRayTest.Transform = swMultiply(PieceRayTest.Transform, Piece->mModelWorld);
			}
		}
	}

	return MinIntersect;
}

bool swModel::SubModelBoxTest(const swVector4 Planes[6]) const
{
	for (const std::unique_ptr<swPiece>& Piece : mPieces)
		if (Piece->IsVisibleInSubModel() && Piece->IntersectsPlanes(Piece->mModelWorld, Planes))
			return true;

	return false;
}

void swModel::SubModelCompareBoundingBox(const swMatrix44& WorldMatrix, swVector3& Min, swVector3& Max) const
{
	const swMesh* Mesh = mPieceData ? mPieceData->GetDisplayMesh() : nullptr;

	if (Mesh)
	{
		swVector3 Box[8];
		swGetBoxCorners(Mesh->mBoundingBox, Box);

		for (const swVector3& Corner : Box)
		{
			const swVector3 Point = swMultiply31(Corner, WorldMatrix);
			Min = swMinimum(Min, Point);
			Max = swMaximum(Max, Point);
		}
	}

	for (const std::unique_ptr<swPiece>& Piece : mPieces)
		if (Piece->IsVisibleInSubModel())
			Piece->SubModelCompareBoundingBox(WorldMatrix, Min, Max);
}

void swModel::SubModelAddBoundingBoxPoints(const swMatrix44& WorldMatrix, std::vector<swVector3>& Points, swGeometryBoundsMode Mode) const
{
	const swMesh* Mesh = mPieceData ? mPieceData->GetDisplayMesh() : nullptr;

	if (Mode == swGeometryBoundsMode::AvailableOnly && mPieceData && mPieceData->mState != swAssetState::Loaded)
		Mesh = nullptr;

	if (Mesh)
	{
		swVector3 Box[8];
		swGetBoxCorners(Mesh->mBoundingBox, Box);

		for (const swVector3& Corner : Box)
			Points.emplace_back(swMultiply31(Corner, WorldMatrix));
	}

	for (const std::unique_ptr<swPiece>& Piece : mPieces)
		if (Piece->IsVisibleInSubModel())
			Piece->SubModelAddBoundingBoxPoints(WorldMatrix, Points, Mode);
}

void swModel::AddSelectionHistory(std::function<void()> Callback)
{
	std::unique_ptr<swModelHistorySelect> ModelHistorySelect = std::make_unique<swModelHistorySelect>();

	ModelHistorySelect->SaveStartState(this);

	Callback();

	ModelHistorySelect->SaveEndState(this);

	if (ModelHistorySelect->StateChanged())
		mHistorySequence.emplace_back(std::move(ModelHistorySelect));
}

void swModel::ClearSelection()
{
	AddSelectionHistory([this](){ DeselectAllObjects(); });
}

void swModel::SetFocus(swObject* FocusObject, uint32_t FocusSection, swSelectionMode SelectionMode)
{
	AddSelectionHistory([this, FocusObject, FocusSection, SelectionMode](){ SetFocusedObject(FocusObject, FocusSection, SelectionMode); });
}

void swModel::SetSelectionAndFocus(const std::vector<swObject*>& Objects, swObject* FocusObject, uint32_t FocusSection, swSelectionMode SelectionMode)
{
	AddSelectionHistory([this, &Objects, FocusObject, FocusSection, SelectionMode]()
	{
		DeselectAllObjects();
		SetObjectsSelected(Objects, true);
		SetFocusedObject(FocusObject, FocusSection, SelectionMode);
	});
}

void swModel::SelectAllPieces()
{
	AddSelectionHistory([this]()
	{
		for (const std::unique_ptr<swPiece>& Piece : mPieces)
			if (Piece->IsVisible(mCurrentStep))
				Piece->SetSelected(true);
	});
}

void swModel::InvertPieceSelection()
{
	AddSelectionHistory([this]()
	{
		for (const std::unique_ptr<swPiece>& Piece : mPieces)
			if (Piece->IsSelected() || Piece->IsVisible(mCurrentStep))
				Piece->SetSelected(!Piece->IsSelected());
	});
}

void swModel::AddToSelection(const std::vector<swObject*>& Objects)
{
	AddSelectionHistory([this, &Objects](){ SetObjectsSelected(Objects, true); });
}

void swModel::RemoveFromSelection(const std::vector<swObject*>& Objects)
{
	AddSelectionHistory([this, &Objects](){ SetObjectsSelected(Objects, false); });
}

bool swModel::AnyPiecesSelected() const
{
	for (const std::unique_ptr<swPiece>& Piece : mPieces)
		if (Piece->IsSelected())
			return true;

	return false;
}

bool swModel::AnyObjectsSelected() const
{
	for (const std::unique_ptr<swPiece>& Piece : mPieces)
		if (Piece->IsSelected())
			return true;

	for (const std::unique_ptr<swCamera>& Camera : mCameras)
		if (Camera->IsSelected())
			return true;

	for (const std::unique_ptr<swLight>& Light : mLights)
		if (Light->IsSelected())
			return true;

	return false;
}

swObject* swModel::GetFocusObject() const
{
	for (const std::unique_ptr<swPiece>& Piece : mPieces)
		if (Piece->IsFocused())
			return Piece.get();

	for (const std::unique_ptr<swCamera>& Camera : mCameras)
		if (Camera->IsFocused())
			return Camera.get();

	for (const std::unique_ptr<swLight>& Light : mLights)
		if (Light->IsFocused())
			return Light.get();

	return nullptr;
}

swModel* swModel::GetFirstSelectedSubmodel() const
{
	for (const std::unique_ptr<swPiece>& Piece : mPieces)
		if (Piece->IsSelected() && Piece->mPieceData->IsModel())
			return Piece->mPieceData->GetModel();

	return nullptr;
}

void swModel::GetSubModels(std::set<swModel*>& SubModels) const
{
	for (const std::unique_ptr<swPiece>& Piece : mPieces)
	{
		if (Piece->mPieceData->IsModel())
		{
			swModel* SubModel = Piece->mPieceData->GetModel();

			SubModels.insert(SubModel);
		}
	}
}

bool swModel::GetMoveRotateTransform(swVector3& Center, swMatrix33& RelativeRotation) const
{
	const bool Relative = gMainWindow->GetRelativeTransform();
	int NumSelected = 0;

	Center = swVector3(0.0f, 0.0f, 0.0f);
	RelativeRotation = swMatrix33Identity();

	for (const std::unique_ptr<swPiece>& Piece : mPieces)
	{
		if (!Piece->IsSelected())
			continue;

		if (Piece->IsFocused() && Relative)
		{
			Center = Piece->GetRotationCenter();
			RelativeRotation = Piece->GetRelativeRotation();
			return true;
		}

		Center += Piece->mModelWorld.GetTranslation();
		NumSelected++;
	}

	for (const std::unique_ptr<swCamera>& Camera : mCameras)
	{
		if (!Camera->IsSelected())
			continue;

		if (Camera->IsFocused() && Relative)
		{
			Center = Camera->GetSectionPosition(Camera->GetFocusSection());
//			RelativeRotation = Piece->GetRelativeRotation();
			return true;
		}

		Center += Camera->GetSectionPosition(SW_CAMERA_SECTION_POSITION);
		Center += Camera->GetSectionPosition(SW_CAMERA_SECTION_TARGET);
		Center += Camera->GetSectionPosition(SW_CAMERA_SECTION_UP_VECTOR);
		NumSelected += 3;
	}

	for (const std::unique_ptr<swLight>& Light : mLights)
	{
		if (!Light->IsSelected())
			continue;

		if (Light->IsFocused())
		{
			Center = Light->GetRotationCenter();

			if (Relative)
				RelativeRotation = Light->GetRelativeRotation();

			return true;
		}

		Center += Light->GetSectionPosition(SW_LIGHT_SECTION_POSITION);
		NumSelected++;
	}

	if (NumSelected)
	{
		Center /= NumSelected;
		return true;
	}

	return false;
}

bool swModel::CanRotateSelection() const
{
	int Flags;
	std::vector<swObject*> Selection;
	swObject* Focus;

	GetSelectionInformation(&Flags, Selection, &Focus);

	if (Flags & SW_SELECTION_PIECE)
	{
		if ((Flags & (SW_SELECTION_CAMERA | SW_SELECTION_LIGHT)) == 0)
			return true;
	}

	if ((Flags & (SW_SELECTION_PIECE | SW_SELECTION_CAMERA)) == 0)
	{
		if (Focus && Focus->IsLight())
		{
			swLight* Light = (swLight*)Focus;

			return (Light->GetAllowedTransforms() & SW_OBJECT_TRANSFORM_ROTATE_XYZ) != 0;
		}
	}

	return false;
}

bool swModel::GetPieceFocusOrSelectionCenter(swVector3& Center) const
{
	swVector3 Min(FLT_MAX, FLT_MAX, FLT_MAX), Max(-FLT_MAX, -FLT_MAX, -FLT_MAX);
	swPiece* Selected = nullptr;
	int NumSelected = 0;

	for (const std::unique_ptr<swPiece>& Piece : mPieces)
	{
		if (Piece->IsFocused())
		{
			Center = Piece->mModelWorld.GetTranslation();
			return true;
		}

		if (Piece->IsSelected())
		{
			Piece->CompareBoundingBox(Min, Max);
			Selected = Piece.get();
			NumSelected++;
		}
	}

	if (NumSelected == 1)
		Center = Selected->mModelWorld.GetTranslation();
	else if (NumSelected)
		Center = (Min + Max) / 2.0f;
	else
		Center = swVector3(0.0f, 0.0f, 0.0f);

	return NumSelected != 0;
}

swVector3 swModel::GetSelectionOrModelCenter() const
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

	return Center;
}

bool swModel::GetFocusPosition(swVector3& Position) const
{
	const swObject* FocusObject = GetFocusObject();

	if (FocusObject)
	{
		Position = FocusObject->GetSectionPosition(FocusObject->GetFocusSection());
		return true;
	}
	else
	{
		Position = swVector3(0.0f, 0.0f, 0.0f);
		return false;
	}
}

bool swModel::GetSelectionCenter(swVector3& Center) const
{
	swVector3 Min(FLT_MAX, FLT_MAX, FLT_MAX), Max(-FLT_MAX, -FLT_MAX, -FLT_MAX);
	swPiece* SelectedPiece = nullptr;
	bool SinglePieceSelected = true;
	bool Selected = false;

	for (const std::unique_ptr<swPiece>& Piece : mPieces)
	{
		if (Piece->IsSelected())
		{
			Piece->CompareBoundingBox(Min, Max);
			Selected = true;

			if (!SelectedPiece)
				SelectedPiece = Piece.get();
			else
				SinglePieceSelected = false;
		}
	}

	for (const std::unique_ptr<swCamera>& Camera : mCameras)
	{
		if (Camera->IsSelected())
		{
			Camera->CompareBoundingBox(Min, Max);
			Selected = true;
			SinglePieceSelected = false;
		}
	}

	for (const std::unique_ptr<swLight>& Light : mLights)
	{
		if (Light->IsSelected())
		{
			Light->CompareBoundingBox(Min, Max);
			Selected = true;
			SinglePieceSelected = false;
		}
	}

	if (SelectedPiece && SinglePieceSelected)
		Center = SelectedPiece->GetSectionPosition(SW_PIECE_SECTION_POSITION);
	else if (Selected)
		Center = (Min + Max) / 2.0f;
	else
		Center = swVector3(0.0f, 0.0f, 0.0f);

	return Selected;
}

swBoundingBox swModel::GetAllPiecesBoundingBox() const
{
	swBoundingBox Box;

	if (!mPieces.empty())
	{
		Box.Min = swVector3(FLT_MAX, FLT_MAX, FLT_MAX);
		Box.Max = swVector3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

		for (const std::unique_ptr<swPiece>& Piece : mPieces)
			Piece->CompareBoundingBox(Box.Min, Box.Max);
	}
	else
		Box.Min = Box.Max = swVector3(0.0f, 0.0f, 0.0f);

	return Box;
}

bool swModel::GetVisiblePiecesBoundingBox(swVector3& Min, swVector3& Max) const
{
	bool Valid = false;
	Min = swVector3(FLT_MAX, FLT_MAX, FLT_MAX);
	Max = swVector3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	for (const std::unique_ptr<swPiece>& Piece : mPieces)
	{
		if (Piece->IsSelected() || Piece->IsVisible(mCurrentStep))
		{
			Piece->CompareBoundingBox(Min, Max);
			Valid = true;
		}
	}

	return Valid;
}

std::vector<swVector3> swModel::GetPiecesBoundingBoxPoints(swGeometryBoundsMode Mode) const
{
	std::vector<swVector3> Points;
	const swMesh* Mesh = mPieceData ? mPieceData->GetDisplayMesh() : nullptr;

	if (Mode == swGeometryBoundsMode::AvailableOnly && mPieceData && mPieceData->mState != swAssetState::Loaded)
		Mesh = nullptr;

	if (Mesh)
	{
		swVector3 Box[8];
		swGetBoxCorners(Mesh->mBoundingBox, Box);

		for (const swVector3& Point : Box)
			Points.emplace_back(Point);
	}

	for (const std::unique_ptr<swPiece>& Piece : mPieces)
		if (Piece->IsSelected() || Piece->IsVisible(mCurrentStep))
			Piece->SubModelAddBoundingBoxPoints(swMatrix44Identity(), Points, Mode);

	return Points;
}

void swModel::GetSelectionInformation(int* Flags, std::vector<swObject*>& Selection, swObject** Focus) const
{
	*Flags = 0;
	*Focus = nullptr;

	if (mPieces.empty())
		*Flags |= SW_SELECTION_NO_PIECES;
	else
	{
		swGroup* Group = nullptr;
		bool First = true;

		for (const std::unique_ptr<swPiece>& Piece : mPieces)
		{
			if (Piece->IsSelected())
			{
				Selection.emplace_back(Piece.get());

				if (Piece->IsFocused())
					*Focus = Piece.get();

				if (Piece->mPieceData->IsModel())
					*Flags |= SW_SELECTION_MODEL_SELECTED;

				if (Piece->IsHidden())
					*Flags |= SW_SELECTION_HIDDEN | SW_SELECTION_HIDDEN_SELECTED;
				else
					*Flags |= SW_SELECTION_VISIBLE_SELECTED;

				*Flags |= SW_SELECTION_PIECE | SW_SELECTION_SELECTED;

				if (Piece->AreTrainTrackConnectionsVisible())
					*Flags |= SW_SELECTION_TRAIN_TRACK_VISIBLE;

				if (Piece->CanAddControlPoint())
					*Flags |= SW_SELECTION_CAN_ADD_CONTROL_POINT;

				if (Piece->CanRemoveControlPoint())
					*Flags |= SW_SELECTION_CAN_REMOVE_CONTROL_POINT;

				if (Piece->GetGroup() != nullptr)
				{
					*Flags |= SW_SELECTION_GROUPED;
					if (Piece->IsFocused())
						*Flags |= SW_SELECTION_FOCUS_GROUPED;
				}

				if (First)
				{
					Group = Piece->GetGroup();
					First = false;
				}
				else
				{
					if (Group != Piece->GetGroup())
						*Flags |= SW_SELECTION_CAN_GROUP;
					else
						if (Group == nullptr)
							*Flags |= SW_SELECTION_CAN_GROUP;
				}
			}
			else
			{
				*Flags |= SW_SELECTION_UNSELECTED;

				if (Piece->IsHidden())
					*Flags |= SW_SELECTION_HIDDEN;
			}
		}
	}

	for (const std::unique_ptr<swCamera>& Camera : mCameras)
	{
		if (Camera->IsSelected())
		{
			Selection.emplace_back(Camera.get());
			*Flags |= SW_SELECTION_SELECTED | SW_SELECTION_CAMERA;

			if (Camera->IsFocused())
				*Focus = Camera.get();
		}
	}

	for (const std::unique_ptr<swLight>& Light : mLights)
	{
		if (Light->IsSelected())
		{
			Selection.emplace_back(Light.get());
			*Flags |= SW_SELECTION_SELECTED | SW_SELECTION_LIGHT;

			if (Light->IsFocused())
				*Focus = Light.get();
		}
	}
}

void swModel::ClearSelectionAction()
{
	BeginHistorySequence();
	ClearSelection();
	EndHistorySequence(tr("Selection"));
}

void swModel::SetSelectionAndFocusAction(const std::vector<swObject*>& Objects, swObject* Focus, quint32 Section, swSelectionMode SelectionMode)
{
	BeginHistorySequence();
	SetSelectionAndFocus(Objects, Focus, Section, SelectionMode);
	EndHistorySequence(tr("Selection"));
}

void swModel::FocusOrDeselectObjectAction(swObject* Object, uint32_t Section, swSelectionMode SelectionMode)
{
	BeginHistorySequence();

	if (Object)
	{
		if (Object->IsFocused(Section) || (Object->IsPiece() && Object->IsSelected()))
			RemoveFromSelection({ Object });
		else
			SetFocus(Object, Section, SelectionMode);
	}
	else
	{
		SetFocus(nullptr, 0, SelectionMode);
	}

	EndHistorySequence(tr("Selection"));
}

void swModel::SelectAllPiecesAction()
{
	BeginHistorySequence();
	SelectAllPieces();
	EndHistorySequence(tr("Selection"));
}

void swModel::InvertPieceSelectionAction()
{
	BeginHistorySequence();
	InvertPieceSelection();
	EndHistorySequence(tr("Selection"));
}

void swModel::AddToSelectionAction(const std::vector<swObject*>& Objects)
{
	BeginHistorySequence();
	AddToSelection(Objects);
	EndHistorySequence(tr("Selection"));
}

void swModel::RemoveFromSelectionAction(const std::vector<swObject*>& Objects)
{
	BeginHistorySequence();
	RemoveFromSelection(Objects);
	EndHistorySequence(tr("Selection"));
}

std::vector<swObject*> swModel::GetSelectionModePieces(swSelectionMode SelectionMode, const swPiece* SelectedPiece) const
{
	const swPieceData* PieceData = SelectedPiece->mPieceData.Get();
	const int ColorIndex = SelectedPiece->GetColorIndex();
	std::vector<swObject*> Pieces;

	switch (SelectionMode)
	{
	case swSelectionMode::Single:
		break;

	case swSelectionMode::Piece:
		for (const std::unique_ptr<swPiece>& Piece : mPieces)
			if (Piece->IsVisible(mCurrentStep) && Piece->mPieceData == PieceData && Piece.get() != SelectedPiece)
				Pieces.emplace_back(Piece.get());
		break;

	case swSelectionMode::Color:
		for (const std::unique_ptr<swPiece>& Piece : mPieces)
			if (Piece->IsVisible(mCurrentStep) && Piece->GetColorIndex() == ColorIndex && Piece.get() != SelectedPiece)
				Pieces.emplace_back(Piece.get());
		break;

	case swSelectionMode::PieceColor:
		for (const std::unique_ptr<swPiece>& Piece : mPieces)
			if (Piece->IsVisible(mCurrentStep) && Piece->mPieceData == PieceData && Piece->GetColorIndex() == ColorIndex && Piece.get() != SelectedPiece)
				Pieces.emplace_back(Piece.get());
		break;
	}

	return Pieces;
}

void swModel::DeselectAllObjects()
{
	for (const std::unique_ptr<swPiece>& Piece : mPieces)
		Piece->SetSelected(false);

	for (const std::unique_ptr<swCamera>& Camera : mCameras)
		Camera->SetSelected(false);

	for (const std::unique_ptr<swLight>& Light : mLights)
		Light->SetSelected(false);
}

void swModel::SetFocusedObject(swObject* FocusObject, uint32_t FocusSection, swSelectionMode SelectionMode)
{
	swObject* PreviousFocus = GetFocusObject();

	if (PreviousFocus)
		PreviousFocus->SetFocused(PreviousFocus->GetFocusSection(), false);

	if (FocusObject)
	{
		swPiece* Piece = dynamic_cast<swPiece*>(FocusObject);

		if (Piece)
		{
			if (!Piece->IsSelected())
				SetObjectsSelected({ Piece }, true);

			std::vector<swObject*> Pieces = GetSelectionModePieces(SelectionMode, Piece);

			SetObjectsSelected(Pieces, true);
		}

		FocusObject->SetFocused(FocusSection, true);
	}
}

void swModel::SetObjectsSelected(const std::vector<swObject*>& Objects, bool Selected)
{
	for (swObject* Object : Objects)
	{
		if (Object->IsSelected() == Selected)
			continue;

		Object->SetSelected(Selected);

		if (Object->IsPiece())
		{
			swPiece* Piece = dynamic_cast<swPiece*>(Object);

			SelectGroup(Piece->GetTopGroup(), Selected);
		}
	}
}

void swModel::SelectGroup(swGroup* TopGroup, bool Select)
{
	if (!TopGroup)
		return;

	for (const std::unique_ptr<swPiece>& Piece : mPieces)
		if (!Piece->IsSelected() && (Piece->GetTopGroup() == TopGroup))
			Piece->SetSelected(Select);
}

void swModel::HideSelectedPieces()
{
	BeginHistorySequence();
	BeginEditHistory();

	bool Modified = false;

	for (const std::unique_ptr<swPiece>& Piece : mPieces)
	{
		if (Piece->IsSelected() && !Piece->IsHidden())
		{
			Piece->SetHidden(true);

			Modified = true;
		}
	}

	if (!Modified)
	{
		DiscardHistorySequence();

		return;
	}

	EndEditHistory();

	ClearSelection();

	EndHistorySequence(tr("Hide Pieces"));

	gMainWindow->UpdateTimeline(false, true);
	gMainWindow->UpdateSelectedObjects(true);
}

void swModel::HideUnselectedPieces()
{
	BeginHistorySequence();
	BeginEditHistory();

	bool Modified = false;

	for (const std::unique_ptr<swPiece>& Piece : mPieces)
	{
		if (!Piece->IsSelected() && !Piece->IsHidden())
		{
			Piece->SetHidden(true);

			Modified = true;
		}
	}

	if (!Modified)
	{
		DiscardHistorySequence();

		return;
	}

	EndEditHistory();
	EndHistorySequence(tr("Hide Pieces"));

	gMainWindow->UpdateTimeline(false, true);
	gMainWindow->UpdateSelectedObjects(true);
}

void swModel::UnhideSelectedPieces()
{
	BeginHistorySequence();
	BeginEditHistory();

	bool Modified = false;

	for (const std::unique_ptr<swPiece>& Piece : mPieces)
	{
		if (Piece->IsSelected() && Piece->IsHidden())
		{
			Piece->SetHidden(false);

			Modified = true;
		}
	}

	if (!Modified)
	{
		DiscardHistorySequence();

		return;
	}

	EndEditHistory();
	EndHistorySequence(tr("Unhide Pieces"));

	gMainWindow->UpdateTimeline(false, true);
	gMainWindow->UpdateSelectedObjects(true);
}

void swModel::UnhideAllPieces()
{
	BeginHistorySequence();
	BeginEditHistory();

	bool Modified = false;

	for (const std::unique_ptr<swPiece>& Piece : mPieces)
	{
		if (Piece->IsHidden())
		{
			Piece->SetHidden(false);

			Modified = true;
		}
	}

	if (!Modified)
	{
		DiscardHistorySequence();

		return;
	}

	EndEditHistory();
	EndHistorySequence(tr("Unhide Pieces"));

	gMainWindow->UpdateTimeline(false, true);
	gMainWindow->UpdateSelectedObjects(true);
}

void swModel::FindReplacePiece(bool SearchForward, bool FindAll, bool Replace)
{
	if (mPieces.empty())
		return;

	const swFindReplaceParameters& Params = swView::GetFindReplaceParams();

	const bool ReplacePieceData = Replace && Params.ReplacePieceData;
	const bool ReplaceColor = Replace && swGetColorCode(Params.ReplaceColorIndex) != SW_COLOR_NO_COLOR;

	// Check if we are supposed to actually replace something
	const bool Replacing = (ReplaceColor || ReplacePieceData);

	auto PieceMatches = [&Params](const swPiece* Piece)
	{
		if (Params.FindPieceData && Params.FindPieceData != Piece->mPieceData)
			return false;

		if (!Params.FindString.isEmpty() && !swstrcasestr(Piece->mPieceData->m_strDescription, Params.FindString.toLatin1()))
			return false;

		return (swGetColorCode(Params.FindColorIndex) == SW_COLOR_NO_COLOR) || (Piece->GetColorIndex() == Params.FindColorIndex);
	};

	auto ReplacePiece = [&Params, ReplacePieceData, ReplaceColor](swPiece* Piece)
	{
		if (ReplaceColor)
			Piece->SetColorIndex(Params.ReplaceColorIndex);

		if (ReplacePieceData)
			Piece->SetPieceData(swPieceDataReference(Params.ReplacePieceData), QString(), true, true);
	};

	size_t StartIndex = mPieces.size() - 1;
	int ReplacedCount = 0;

	BeginHistorySequence();
	BeginEditHistory();

	if (!FindAll)
	{
		// We have to find the currently focused piece, in order to find next/prev match and (optionally) to replace it
		swPiece* const FocusedPiece = dynamic_cast<swPiece*>(GetFocusObject());

		if (FocusedPiece)
		{
			for (size_t PieceIndex = 0; PieceIndex < mPieces.size(); PieceIndex++)
			{
				if (FocusedPiece == mPieces[PieceIndex].get())
				{
					StartIndex = PieceIndex;
					break;
				}
			}

			if (Replacing && PieceMatches(FocusedPiece))
			{
				ReplacePiece(FocusedPiece);
				ReplacedCount++;
			}
		}
	}

	size_t CurrentIndex = StartIndex;
	swPiece* Focus = nullptr;
	std::vector<swObject*> Selection;

	for (;;)
	{
		if (SearchForward)
		{
			CurrentIndex++;

			if (CurrentIndex >= mPieces.size())
				CurrentIndex = 0;
		}
		else
		{
			if (CurrentIndex == 0)
				CurrentIndex = mPieces.size();

			CurrentIndex--;
		}

		swPiece* Current = mPieces[CurrentIndex].get();

		if (Current->IsVisible(mCurrentStep) && PieceMatches(Current))
		{
			if (FindAll)
			{
				Selection.emplace_back(Current);
				if (Replacing)
				{
					ReplacePiece(Current);
					ReplacedCount++;
				}
			}
			else
			{
				Focus = Current;
				break;
			}
		}

		if (CurrentIndex == StartIndex)
			break;
	}

	EndEditHistory();

	if (FindAll)
		SetSelectionAndFocus(Selection, nullptr, 0, swSelectionMode::Single);
	else
		SetSelectionAndFocus(std::vector<swObject*>(), Focus, SW_PIECE_SECTION_POSITION, swSelectionMode::Single);

	if (ReplacedCount)
	{
		EndHistorySequence(tr("Replace Pieces", nullptr, ReplacedCount));

		gMainWindow->UpdateSelectedObjects(false);
		gMainWindow->UpdateTimeline(false, true);
	}
	else
		EndHistorySequence(tr("Selection"));
}
