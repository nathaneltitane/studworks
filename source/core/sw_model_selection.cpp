#include "sw_global.h"
#include "sw_model.h"
#include "sw_model_history.h"
#include "sw_object_part.h"
#include "sw_object_camera.h"
#include "sw_object_light.h"
#include "sw_group.h"
#include "sw_project.h"
#include "sw_window_main.h"
#include "sw_profile.h"
#include "sw_library.h"
#include "sw_library_shadow.h"
#include "sw_synthesis.h"
#include "sw_scene.h"
#include "sw_track_train.h"
#include "sw_file.h"
#include "sw_data_part.h"
#include "sw_view.h"
#include "sw_builder_minifigure.h"
#include "sw_dialog_array.h"
#include "sw_dialog_group.h"
#include "sw_dialog_groups_edit.h"
#include "sw_dialog_properties.h"
#include "sw_window_utilities.h"
#include "sw_lxf.h"
#include "sw_widget_preview.h"
#include "sw_string.h"
#include <locale.h>

void swModel::RayTest(swObjectRayTest& ObjectRayTest) const
{
	for (const std::unique_ptr<swPart>& Part : mParts)
		if ((Part->IsSelected() || Part->IsVisible(mCurrentStep)) && (!ObjectRayTest.IgnoreSelected || !Part->IsSelected()))
			Part->RayTest(ObjectRayTest);

	if (ObjectRayTest.PartsOnly)
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
	for (const std::unique_ptr<swPart>& Part : mParts)
		if (Part->IsSelected() || Part->IsVisible(mCurrentStep))
			Part->BoxTest(ObjectBoxTest);

	for (const std::unique_ptr<swCamera>& Camera : mCameras)
		if (Camera.get() != ObjectBoxTest.ViewCamera && Camera->IsVisible())
			Camera->BoxTest(ObjectBoxTest);

	for (const std::unique_ptr<swLight>& Light : mLights)
		if (Light->IsVisible())
			Light->BoxTest(ObjectBoxTest);
}

bool swModel::SubModelMinIntersectDist(const swVector3& WorldStart, const swVector3& WorldEnd, float& MinDistance, swPartRayTest& PartRayTest) const
{
	bool MinIntersect = false;

	for (const std::unique_ptr<swPart>& Part : mParts)
	{
		const swMatrix44 InverseWorldMatrix = swMatrix44AffineInverse(Part->mModelWorld);
		const swVector3 Start = swMultiply31(WorldStart, InverseWorldMatrix);
		const swVector3 End = swMultiply31(WorldEnd, InverseWorldMatrix);

		if (Part->IsVisibleInSubModel())
		{
			if (Part->MinIntersectDist(Start, End, MinDistance, PartRayTest))
			{
				MinIntersect = true;
				PartRayTest.Transform = swMultiply(PartRayTest.Transform, Part->mModelWorld);
			}
		}
	}

	return MinIntersect;
}

bool swModel::SubModelBoxTest(const swVector4 Planes[6]) const
{
	for (const std::unique_ptr<swPart>& Part : mParts)
		if (Part->IsVisibleInSubModel() && Part->IntersectsPlanes(Part->mModelWorld, Planes))
			return true;

	return false;
}

void swModel::SubModelCompareBoundingBox(const swMatrix44& WorldMatrix, swVector3& Min, swVector3& Max) const
{
	const swMesh* Mesh = mPartData ? mPartData->GetDisplayMesh() : nullptr;

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

	for (const std::unique_ptr<swPart>& Part : mParts)
		if (Part->IsVisibleInSubModel())
			Part->SubModelCompareBoundingBox(WorldMatrix, Min, Max);
}

void swModel::SubModelAddBoundingBoxPoints(const swMatrix44& WorldMatrix, std::vector<swVector3>& Points, swGeometryBoundsMode Mode) const
{
	const swMesh* Mesh = mPartData ? mPartData->GetDisplayMesh() : nullptr;

	if (Mode == swGeometryBoundsMode::AvailableOnly && mPartData && mPartData->mState != swAssetState::Loaded)
		Mesh = nullptr;

	if (Mesh)
	{
		swVector3 Box[8];
		swGetBoxCorners(Mesh->mBoundingBox, Box);

		for (const swVector3& Corner : Box)
			Points.emplace_back(swMultiply31(Corner, WorldMatrix));
	}

	for (const std::unique_ptr<swPart>& Part : mParts)
		if (Part->IsVisibleInSubModel())
			Part->SubModelAddBoundingBoxPoints(WorldMatrix, Points, Mode);
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

void swModel::SelectAllParts()
{
	AddSelectionHistory([this]()
	{
		for (const std::unique_ptr<swPart>& Part : mParts)
			if (Part->IsVisible(mCurrentStep))
				Part->SetSelected(true);
	});
}

void swModel::InvertPartSelection()
{
	AddSelectionHistory([this]()
	{
		for (const std::unique_ptr<swPart>& Part : mParts)
			if (Part->IsSelected() || Part->IsVisible(mCurrentStep))
				Part->SetSelected(!Part->IsSelected());
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

bool swModel::AnyPartsSelected() const
{
	for (const std::unique_ptr<swPart>& Part : mParts)
		if (Part->IsSelected())
			return true;

	return false;
}

bool swModel::AnyObjectsSelected() const
{
	for (const std::unique_ptr<swPart>& Part : mParts)
		if (Part->IsSelected())
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
	for (const std::unique_ptr<swPart>& Part : mParts)
		if (Part->IsFocused())
			return Part.get();

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
	for (const std::unique_ptr<swPart>& Part : mParts)
		if (Part->IsSelected() && Part->mPartData->IsModel())
			return Part->mPartData->GetModel();

	return nullptr;
}

void swModel::GetSubModels(std::set<swModel*>& SubModels) const
{
	for (const std::unique_ptr<swPart>& Part : mParts)
	{
		if (Part->mPartData->IsModel())
		{
			swModel* SubModel = Part->mPartData->GetModel();

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

	for (const std::unique_ptr<swPart>& Part : mParts)
	{
		if (!Part->IsSelected())
			continue;

		if (Part->IsFocused() && Relative)
		{
			Center = Part->GetRotationCenter();
			RelativeRotation = Part->GetRelativeRotation();
			return true;
		}

		Center += Part->mModelWorld.GetTranslation();
		NumSelected++;
	}

	for (const std::unique_ptr<swCamera>& Camera : mCameras)
	{
		if (!Camera->IsSelected())
			continue;

		if (Camera->IsFocused() && Relative)
		{
			Center = Camera->GetSectionPosition(Camera->GetFocusSection());
//			RelativeRotation = Part->GetRelativeRotation();
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

	if (Flags & SW_SELECTION_PART)
	{
		if ((Flags & (SW_SELECTION_CAMERA | SW_SELECTION_LIGHT)) == 0)
			return true;
	}

	if ((Flags & (SW_SELECTION_PART | SW_SELECTION_CAMERA)) == 0)
	{
		if (Focus && Focus->IsLight())
		{
			swLight* Light = (swLight*)Focus;

			return (Light->GetAllowedTransforms() & SW_OBJECT_TRANSFORM_ROTATE_XYZ) != 0;
		}
	}

	return false;
}

bool swModel::GetPartFocusOrSelectionCenter(swVector3& Center) const
{
	swVector3 Min(FLT_MAX, FLT_MAX, FLT_MAX), Max(-FLT_MAX, -FLT_MAX, -FLT_MAX);
	swPart* Selected = nullptr;
	int NumSelected = 0;

	for (const std::unique_ptr<swPart>& Part : mParts)
	{
		if (Part->IsFocused())
		{
			Center = Part->mModelWorld.GetTranslation();
			return true;
		}

		if (Part->IsSelected())
		{
			Part->CompareBoundingBox(Min, Max);
			Selected = Part.get();
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

		if (GetVisiblePartsBoundingBox(Min, Max))
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
	swPart* SelectedPart = nullptr;
	bool SinglePartSelected = true;
	bool Selected = false;

	for (const std::unique_ptr<swPart>& Part : mParts)
	{
		if (Part->IsSelected())
		{
			Part->CompareBoundingBox(Min, Max);
			Selected = true;

			if (!SelectedPart)
				SelectedPart = Part.get();
			else
				SinglePartSelected = false;
		}
	}

	for (const std::unique_ptr<swCamera>& Camera : mCameras)
	{
		if (Camera->IsSelected())
		{
			Camera->CompareBoundingBox(Min, Max);
			Selected = true;
			SinglePartSelected = false;
		}
	}

	for (const std::unique_ptr<swLight>& Light : mLights)
	{
		if (Light->IsSelected())
		{
			Light->CompareBoundingBox(Min, Max);
			Selected = true;
			SinglePartSelected = false;
		}
	}

	if (SelectedPart && SinglePartSelected)
		Center = SelectedPart->GetSectionPosition(SW_PART_SECTION_POSITION);
	else if (Selected)
		Center = (Min + Max) / 2.0f;
	else
		Center = swVector3(0.0f, 0.0f, 0.0f);

	return Selected;
}

swBoundingBox swModel::GetAllPartsBoundingBox() const
{
	swBoundingBox Box;

	if (!mParts.empty())
	{
		Box.Min = swVector3(FLT_MAX, FLT_MAX, FLT_MAX);
		Box.Max = swVector3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

		for (const std::unique_ptr<swPart>& Part : mParts)
			Part->CompareBoundingBox(Box.Min, Box.Max);
	}
	else
		Box.Min = Box.Max = swVector3(0.0f, 0.0f, 0.0f);

	return Box;
}

bool swModel::GetVisiblePartsBoundingBox(swVector3& Min, swVector3& Max) const
{
	bool Valid = false;
	Min = swVector3(FLT_MAX, FLT_MAX, FLT_MAX);
	Max = swVector3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	for (const std::unique_ptr<swPart>& Part : mParts)
	{
		if (Part->IsSelected() || Part->IsVisible(mCurrentStep))
		{
			Part->CompareBoundingBox(Min, Max);
			Valid = true;
		}
	}

	return Valid;
}

std::vector<swVector3> swModel::GetPartsBoundingBoxPoints(swGeometryBoundsMode Mode) const
{
	std::vector<swVector3> Points;
	const swMesh* Mesh = mPartData ? mPartData->GetDisplayMesh() : nullptr;

	if (Mode == swGeometryBoundsMode::AvailableOnly && mPartData && mPartData->mState != swAssetState::Loaded)
		Mesh = nullptr;

	if (Mesh)
	{
		swVector3 Box[8];
		swGetBoxCorners(Mesh->mBoundingBox, Box);

		for (const swVector3& Point : Box)
			Points.emplace_back(Point);
	}

	for (const std::unique_ptr<swPart>& Part : mParts)
		if (Part->IsSelected() || Part->IsVisible(mCurrentStep))
			Part->SubModelAddBoundingBoxPoints(swMatrix44Identity(), Points, Mode);

	return Points;
}

void swModel::GetSelectionInformation(int* Flags, std::vector<swObject*>& Selection, swObject** Focus) const
{
	*Flags = 0;
	*Focus = nullptr;

	if (mParts.empty())
		*Flags |= SW_SELECTION_NO_PARTS;
	else
	{
		swGroup* Group = nullptr;
		bool First = true;

		for (const std::unique_ptr<swPart>& Part : mParts)
		{
			if (Part->IsSelected())
			{
				Selection.emplace_back(Part.get());

				if (Part->IsFocused())
					*Focus = Part.get();

				if (Part->mPartData->IsModel())
					*Flags |= SW_SELECTION_MODEL_SELECTED;

				if (Part->IsHidden())
					*Flags |= SW_SELECTION_HIDDEN | SW_SELECTION_HIDDEN_SELECTED;
				else
					*Flags |= SW_SELECTION_VISIBLE_SELECTED;

				*Flags |= SW_SELECTION_PART | SW_SELECTION_SELECTED;

				if (Part->AreTrainTrackConnectionsVisible())
					*Flags |= SW_SELECTION_TRAIN_TRACK_VISIBLE;

				if (Part->CanAddControlPoint())
					*Flags |= SW_SELECTION_CAN_ADD_CONTROL_POINT;

				if (Part->CanRemoveControlPoint())
					*Flags |= SW_SELECTION_CAN_REMOVE_CONTROL_POINT;

				if (Part->CanClampControlPointTangent())
					*Flags |= SW_SELECTION_CAN_CLAMP_TANGENT;

				if (Part->IsControlPointTangentClamped())
					*Flags |= SW_SELECTION_TANGENT_CLAMPED;

				if (Part->CanAlignControlPoint())
					*Flags |= SW_SELECTION_CONTROL_POINT_FOCUSED;

				if (Part->GetGroup() != nullptr)
				{
					*Flags |= SW_SELECTION_GROUPED;
					if (Part->IsFocused())
						*Flags |= SW_SELECTION_FOCUS_GROUPED;
				}

				if (First)
				{
					Group = Part->GetGroup();
					First = false;
				}
				else
				{
					if (Group != Part->GetGroup())
						*Flags |= SW_SELECTION_CAN_GROUP;
					else
						if (Group == nullptr)
							*Flags |= SW_SELECTION_CAN_GROUP;
				}
			}
			else
			{
				*Flags |= SW_SELECTION_UNSELECTED;

				if (Part->IsHidden())
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
		if (Object->IsFocused(Section) || (Object->IsPart() && Object->IsSelected()))
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

void swModel::SelectAllPartsAction()
{
	BeginHistorySequence();
	SelectAllParts();
	EndHistorySequence(tr("Selection"));
}

void swModel::InvertPartSelectionAction()
{
	BeginHistorySequence();
	InvertPartSelection();
	EndHistorySequence(tr("Selection"));
}

void swModel::SelectConnectedPartsAction(bool AllConnected)
{
	const std::vector<swObject*> Connected = FindConnectedParts(AllConnected);

	if (!Connected.empty())
		AddToSelectionAction(Connected);
}

// use to find parts connected to the selection
std::vector<swObject*> swModel::FindConnectedParts(bool AllConnected) const
{
	struct swConnectedPart
	{
		swPart* Part;
		swBoundingBox Box;
		swConnectionShapes Shapes;
		bool Reached;
	};

	std::vector<swConnectedPart> ConnectedParts;
	std::vector<size_t> Queue;

	for (const std::unique_ptr<swPart>& Part : mParts)
	{
		if (!Part->IsSelected() && !Part->IsVisible(mCurrentStep))
			continue;

		swConnectionShapes Shapes;

		AddPartShapes(Part.get(), swMatrix44Identity(), Shapes);

		if (Shapes.empty())
			continue;

		swBoundingBox Box = { swVector3(FLT_MAX, FLT_MAX, FLT_MAX), swVector3(-FLT_MAX, -FLT_MAX, -FLT_MAX) };

		for (const auto& [Cylinders, Transform] : Shapes)
			swConnectivity::AddToBoundingBox(*Cylinders, Transform, Box);

		ConnectedParts.push_back({ Part.get(), Box, std::move(Shapes), Part->IsSelected() });

		if (ConnectedParts.back().Reached)
			Queue.push_back(ConnectedParts.size() - 1);
	}

	// only compare shapes of nearby parts
	const auto BoxesMeet = [](const swBoundingBox& a, const swBoundingBox& b)
	{
		const float Margin = 1.0f;

		for (int Axis = 0; Axis < 3; Axis++)
			if (a.Min[Axis] > b.Max[Axis] + Margin || b.Min[Axis] > a.Max[Axis] + Margin)
				return false;

		return true;
	};

	std::vector<swObject*> Found;

	// breadth first from the selection
	for (size_t QueueIndex = 0; QueueIndex < Queue.size(); QueueIndex++)
	{
		const swConnectedPart& From = ConnectedParts[Queue[QueueIndex]];

		for (size_t PartIndex = 0; PartIndex < ConnectedParts.size(); PartIndex++)
		{
			swConnectedPart& To = ConnectedParts[PartIndex];

			if (To.Reached || !BoxesMeet(From.Box, To.Box) || !swConnectivity::AreConnected(From.Shapes, To.Shapes))
				continue;

			To.Reached = true;
			Found.push_back(To.Part);

			if (AllConnected)
				Queue.push_back(PartIndex);
		}
	}

	return Found;
}

// use to collect placed part shapes
void swModel::AddPartShapes(const swPart* Part, const swMatrix44& Transform, swConnectionShapes& Shapes)
{
	const swMatrix44 World = swMultiply(Part->mModelWorld, Transform);
	const swPartData* PartData = Part->mPartData.Get();

	if (!PartData->IsModel())
	{
		const std::vector<swConnectionCylinder>& Cylinders = swShadowLibrary::GetCylinders(PartData->mFileName);

		if (!Cylinders.empty())
			Shapes.emplace_back(&Cylinders, World);

		return;
	}

	for (const std::unique_ptr<swPart>& SubModelPart : PartData->GetModel()->GetParts())
		if (SubModelPart->IsVisibleInSubModel())
			AddPartShapes(SubModelPart.get(), World, Shapes);
}

// use to seat the end fitting of a flexible part on the nearest connection that takes it
bool swModel::SnapControlPointToConnection(swPart* Part, quint32 ControlPointIndex) const
{
	swMatrix44 SeatTransform;
	swVector3 SurfacePosition, SurfaceNormal;

	if (!FindControlPointSeat(Part, ControlPointIndex, SeatTransform, &SurfacePosition, &SurfaceNormal))
		return false;

	std::vector<swPartControlPoint> ControlPoints = Part->GetControlPoints();
	ControlPoints[ControlPointIndex].Transform = SeatTransform;
	Part->SetControlPoints(ControlPoints);
	Part->SetControlPointSeated(ControlPointIndex, SurfacePosition, SurfaceNormal);

	return true;
}

// use to find where the end fitting of a flexible part would seat, in part space
bool swModel::FindControlPointSeat(const swPart* Part, quint32 ControlPointIndex, swMatrix44& SeatTransform, swVector3* SurfacePosition, swVector3* SurfaceNormal) const
{
	const swSynthesisInformation* SynthInfo = Part->mPartData->GetSynthInfo();
	std::vector<swPartControlPoint> ControlPoints = Part->GetControlPoints();

	if (!SynthInfo || !SynthInfo->IsCurve() || !swShadowLibrary::IsLoaded() || ControlPoints.size() < 2)
		return false;

	if (ControlPointIndex != 0 && ControlPointIndex != ControlPoints.size() - 1)
		return false;

	const float SnapDistance = 20.0f;
	const float FittingDistance = 8.0f;
	const swMatrix44 ControlPointWorld = swMultiply(ControlPoints[ControlPointIndex].Transform, Part->mModelWorld);

	// the connection shapes of the fitting parts at this end
	std::vector<std::pair<const swConnectionCylinder*, swMatrix44>> Fittings;

	for (const auto& [PartName, Transform] : SynthInfo->GetParts(ControlPoints))
	{
		const swMatrix44 PartWorld = swMultiply(swMatrix44LDrawToStudWorks(Transform), Part->mModelWorld);

		if (swLength(PartWorld.GetTranslation() - ControlPointWorld.GetTranslation()) > FittingDistance)
			continue;

		for (const swConnectionCylinder& Cylinder : swShadowLibrary::GetCylinders(PartName.c_str()))
			Fittings.emplace_back(&Cylinder, swMultiply(swMatrix44LDrawToStudWorks(Cylinder.Transform), PartWorld));
	}

	if (Fittings.empty())
		return false;

	// turn the fitting the shortest way onto the connection axis and move it there
	auto GetSeatedControlPoint = [&ControlPointWorld](const swMatrix44& Fitting, const swMatrix44& Target)
	{
		const swVector3 FittingAxis = swNormalize(swMultiply30(swVector3(0.0f, 0.0f, 1.0f), Fitting));
		const swVector3 TargetAxis = swNormalize(swMultiply30(swVector3(0.0f, 0.0f, 1.0f), Target));
		swMatrix33 Rotation = swMatrix33Identity();
		swVector3 RotationAxis = swCross(FittingAxis, TargetAxis);
		const float Angle = acosf(swClamp(swDot(FittingAxis, TargetAxis), -1.0f, 1.0f));

		if (swLength(RotationAxis) < 1e-4f)
			RotationAxis = swCross(FittingAxis, fabsf(FittingAxis[0]) < 0.9f ? swVector3(1.0f, 0.0f, 0.0f) : swVector3(0.0f, 1.0f, 0.0f));

		if (Angle > 1e-4f)
		{
			RotationAxis = swNormalize(RotationAxis);
			Rotation = swMatrix33FromAxisAngle(RotationAxis, Angle);

			if (swDot(swMultiply(FittingAxis, Rotation), TargetAxis) < 0.99f)
				Rotation = swMatrix33FromAxisAngle(RotationAxis, -Angle);
		}

		const swMatrix33 ControlPointRotation = swMultiply(swMatrix33(ControlPointWorld), Rotation);
		const swVector3 ControlPointPosition = swMultiply(ControlPointWorld.GetTranslation() - Fitting.GetTranslation(), Rotation) + Target.GetTranslation();

		return swMatrix44(ControlPointRotation, ControlPointPosition);
	};

	// every seat in reach by how far the control point moves to it, whatever way the connection faces
	struct swConnectionSeat
	{
		float Distance;
		swMatrix44 Transform;
		swVector3 SurfacePosition;
		swVector3 SurfaceNormal;
	};

	std::vector<swConnectionSeat> Seats;

	for (const std::unique_ptr<swPart>& OtherPart : mParts)
	{
		if (OtherPart.get() == Part || !OtherPart->IsVisible(mCurrentStep))
			continue;

		// parts too far for any of their connections to reach are skipped
		swVector3 Min(FLT_MAX, FLT_MAX, FLT_MAX), Max(-FLT_MAX, -FLT_MAX, -FLT_MAX);
		OtherPart->CompareBoundingBox(Min, Max);

		const swVector3 Position = ControlPointWorld.GetTranslation();
		const swVector3 Nearest(swClamp(Position.x, Min.x, Max.x), swClamp(Position.y, Min.y, Max.y), swClamp(Position.z, Min.z, Max.z));

		if (Min.x <= Max.x && swLength(Position - Nearest) > SnapDistance + 2.0f * FittingDistance)
			continue;

		const swVector3 PartCenter = (Min + Max) * 0.5f;

		swConnectionShapes Shapes;
		AddPartShapes(OtherPart.get(), swMatrix44Identity(), Shapes);

		for (const auto& [Cylinders, Transform] : Shapes)
		{
			for (const swConnectionCylinder& Cylinder : *Cylinders)
			{
				const swMatrix44 Frame = swMultiply(swMatrix44LDrawToStudWorks(Cylinder.Transform), Transform);
				const swVector3 Axis = swNormalize(swMultiply30(swVector3(0.0f, 0.0f, 1.0f), Frame));

				for (const auto& [Fitting, FittingFrame] : Fittings)
				{
					if (!swConnectivity::AreCompatible(*Fitting, Cylinder))
						continue;

					// seat the male as deep as the female takes it, closest to their positions meeting
					const bool IsMale = Fitting->Gender == swConnectionGender::Male;
					const std::vector<std::pair<float, float>> InsertRanges = IsMale ? swConnectivity::GetInsertRanges(*Fitting, Cylinder) : swConnectivity::GetInsertRanges(Cylinder, *Fitting);

					for (const std::pair<float, float>& Range : InsertRanges)
					{
						const float MaleOffset = swClamp(0.0f, Range.first, Range.second);
						const swVector3 Position = Frame.GetTranslation() + Axis * (IsMale ? MaleOffset : -MaleOffset);
						const swMatrix44 Seat = GetSeatedControlPoint(FittingFrame, swMatrix44(swMatrix33(Frame), Position));
						const float Distance = swLength(Seat.GetTranslation() - ControlPointWorld.GetTranslation());

						// the connection surface faces away from the part it belongs to
						const swVector3 Surface = Frame.GetTranslation();
						const swVector3 Normal = swDot(Axis, Surface - PartCenter) < 0.0f ? -Axis : Axis;

						if (Distance < SnapDistance)
							Seats.push_back({ Distance, Seat, Surface, Normal });
					}
				}
			}
		}
	}

	std::sort(Seats.begin(), Seats.end(), [](const swConnectionSeat& First, const swConnectionSeat& Second)
	{
		return First.Distance < Second.Distance;
	});

	// a connection the part cannot reach without stretching stays free
	bool Seated = false;

	const swMatrix44 InverseWorld = swMatrix44AffineInverse(Part->mModelWorld);

	for (const swConnectionSeat& Seat : Seats)
	{
		ControlPoints[ControlPointIndex].Transform = swMultiply(Seat.Transform, InverseWorld);

		if (SynthInfo->IsReachable(ControlPoints))
		{
			if (SurfacePosition)
				*SurfacePosition = swMultiply31(Seat.SurfacePosition, InverseWorld);

			if (SurfaceNormal)
				*SurfaceNormal = swNormalize(swMultiply30(Seat.SurfaceNormal, InverseWorld));

			Seated = true;
			break;
		}
	}

	if (!Seated)
		return false;

	SeatTransform = ControlPoints[ControlPointIndex].Transform;

	return true;
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

std::vector<swObject*> swModel::GetSelectionModeParts(swSelectionMode SelectionMode, const swPart* SelectedPart) const
{
	const swPartData* PartData = SelectedPart->mPartData.Get();
	const int ColorIndex = SelectedPart->GetColorIndex();
	std::vector<swObject*> Parts;

	switch (SelectionMode)
	{
	case swSelectionMode::Single:
		break;

	case swSelectionMode::Part:
		for (const std::unique_ptr<swPart>& Part : mParts)
			if (Part->IsVisible(mCurrentStep) && Part->mPartData == PartData && Part.get() != SelectedPart)
				Parts.emplace_back(Part.get());
		break;

	case swSelectionMode::Color:
		for (const std::unique_ptr<swPart>& Part : mParts)
			if (Part->IsVisible(mCurrentStep) && Part->GetColorIndex() == ColorIndex && Part.get() != SelectedPart)
				Parts.emplace_back(Part.get());
		break;

	case swSelectionMode::PartColor:
		for (const std::unique_ptr<swPart>& Part : mParts)
			if (Part->IsVisible(mCurrentStep) && Part->mPartData == PartData && Part->GetColorIndex() == ColorIndex && Part.get() != SelectedPart)
				Parts.emplace_back(Part.get());
		break;
	}

	return Parts;
}

void swModel::DeselectAllObjects()
{
	for (const std::unique_ptr<swPart>& Part : mParts)
		Part->SetSelected(false);

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
		swPart* Part = dynamic_cast<swPart*>(FocusObject);

		if (Part)
		{
			if (!Part->IsSelected())
				SetObjectsSelected({ Part }, true);

			std::vector<swObject*> Parts = GetSelectionModeParts(SelectionMode, Part);

			SetObjectsSelected(Parts, true);
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

		if (Object->IsPart())
		{
			swPart* Part = dynamic_cast<swPart*>(Object);

			SelectGroup(Part->GetTopGroup(), Selected);
		}
	}
}

void swModel::SelectGroup(swGroup* TopGroup, bool Select)
{
	if (!TopGroup)
		return;

	for (const std::unique_ptr<swPart>& Part : mParts)
		if (!Part->IsSelected() && (Part->GetTopGroup() == TopGroup))
			Part->SetSelected(Select);
}

void swModel::HideSelectedParts()
{
	BeginHistorySequence();
	BeginEditHistory();

	bool Modified = false;

	for (const std::unique_ptr<swPart>& Part : mParts)
	{
		if (Part->IsSelected() && !Part->IsHidden())
		{
			Part->SetHidden(true);

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

	EndHistorySequence(tr("Hide Parts"));

	gMainWindow->UpdateTimeline(false, true);
	gMainWindow->UpdateSelectedObjects(true);
}

void swModel::HideUnselectedParts()
{
	BeginHistorySequence();
	BeginEditHistory();

	bool Modified = false;

	for (const std::unique_ptr<swPart>& Part : mParts)
	{
		if (!Part->IsSelected() && !Part->IsHidden())
		{
			Part->SetHidden(true);

			Modified = true;
		}
	}

	if (!Modified)
	{
		DiscardHistorySequence();

		return;
	}

	EndEditHistory();
	EndHistorySequence(tr("Hide Parts"));

	gMainWindow->UpdateTimeline(false, true);
	gMainWindow->UpdateSelectedObjects(true);
}

void swModel::UnhideSelectedParts()
{
	BeginHistorySequence();
	BeginEditHistory();

	bool Modified = false;

	for (const std::unique_ptr<swPart>& Part : mParts)
	{
		if (Part->IsSelected() && Part->IsHidden())
		{
			Part->SetHidden(false);

			Modified = true;
		}
	}

	if (!Modified)
	{
		DiscardHistorySequence();

		return;
	}

	EndEditHistory();
	EndHistorySequence(tr("Unhide Parts"));

	gMainWindow->UpdateTimeline(false, true);
	gMainWindow->UpdateSelectedObjects(true);
}

void swModel::UnhideAllParts()
{
	BeginHistorySequence();
	BeginEditHistory();

	bool Modified = false;

	for (const std::unique_ptr<swPart>& Part : mParts)
	{
		if (Part->IsHidden())
		{
			Part->SetHidden(false);

			Modified = true;
		}
	}

	if (!Modified)
	{
		DiscardHistorySequence();

		return;
	}

	EndEditHistory();
	EndHistorySequence(tr("Unhide Parts"));

	gMainWindow->UpdateTimeline(false, true);
	gMainWindow->UpdateSelectedObjects(true);
}

void swModel::FindReplacePart(bool SearchForward, bool FindAll, bool Replace)
{
	if (mParts.empty())
		return;

	const swFindReplaceParameters& Params = swView::GetFindReplaceParams();

	const bool ReplacePartData = Replace && Params.ReplacePartData;
	const bool ReplaceColor = Replace && swGetColorCode(Params.ReplaceColorIndex) != SW_COLOR_NO_COLOR;

	// Check if we are supposed to actually replace something
	const bool Replacing = (ReplaceColor || ReplacePartData);

	auto PartMatches = [&Params](const swPart* Part)
	{
		if (Params.FindPartData && Params.FindPartData != Part->mPartData)
			return false;

		if (!Params.FindString.isEmpty() && !swstrcasestr(Part->mPartData->m_strDescription, Params.FindString.toLatin1()))
			return false;

		return (swGetColorCode(Params.FindColorIndex) == SW_COLOR_NO_COLOR) || (Part->GetColorIndex() == Params.FindColorIndex);
	};

	auto ReplacePart = [&Params, ReplacePartData, ReplaceColor](swPart* Part)
	{
		if (ReplaceColor)
			Part->SetColorIndex(Params.ReplaceColorIndex);

		if (ReplacePartData)
			Part->SetPartData(swPartDataReference(Params.ReplacePartData), QString(), true, true);
	};

	size_t StartIndex = mParts.size() - 1;
	int ReplacedCount = 0;

	BeginHistorySequence();
	BeginEditHistory();

	if (!FindAll)
	{
		// We have to find the currently focused part, in order to find next/prev match and (optionally) to replace it
		swPart* const FocusedPart = dynamic_cast<swPart*>(GetFocusObject());

		if (FocusedPart)
		{
			for (size_t PartIndex = 0; PartIndex < mParts.size(); PartIndex++)
			{
				if (FocusedPart == mParts[PartIndex].get())
				{
					StartIndex = PartIndex;
					break;
				}
			}

			if (Replacing && PartMatches(FocusedPart))
			{
				ReplacePart(FocusedPart);
				ReplacedCount++;
			}
		}
	}

	size_t CurrentIndex = StartIndex;
	int SubModelReplacedCount = 0;
	swPart* Focus = nullptr;
	std::vector<swObject*> Selection;

	for (;;)
	{
		if (SearchForward)
		{
			CurrentIndex++;

			if (CurrentIndex >= mParts.size())
				CurrentIndex = 0;
		}
		else
		{
			if (CurrentIndex == 0)
				CurrentIndex = mParts.size();

			CurrentIndex--;
		}

		swPart* Current = mParts[CurrentIndex].get();

		if (Current->IsVisible(mCurrentStep) && PartMatches(Current))
		{
			if (FindAll)
			{
				Selection.emplace_back(Current);
				if (Replacing)
				{
					ReplacePart(Current);
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

	// replace all also reaches every submodel used below this one and keeps them all in this undo entry
	if (FindAll && Replacing && Params.ReplaceSubModels)
	{
		std::vector<swModel*> SubModels;
		std::set<swModel*> VisitedModels = { this };

		for (size_t ModelIndex = 0; ModelIndex <= SubModels.size(); ModelIndex++)
		{
			std::set<swModel*> ChildModels;
			(ModelIndex ? SubModels[ModelIndex - 1] : this)->GetSubModels(ChildModels);

			for (swModel* ChildModel : ChildModels)
				if (VisitedModels.insert(ChildModel).second)
					SubModels.push_back(ChildModel);
		}

		for (swModel* SubModel : SubModels)
		{
			std::unique_ptr<swModelHistoryEdit> SubModelEdit = std::make_unique<swModelHistoryEdit>(swModelHistoryEditMergeKey{ swModelHistoryEditMerge::None, swObjectPropertyId::Count, SubModel->mCurrentStep, false });

			SubModelEdit->SaveStartState(SubModel);

			for (const std::unique_ptr<swPart>& Part : SubModel->mParts)
			{
				if (PartMatches(Part.get()))
				{
					ReplacePart(Part.get());
					SubModelReplacedCount++;
				}
			}

			SubModelEdit->SaveEndState(SubModel);

			if (SubModelEdit->StateChanged())
			{
				SubModel->CalculateStep(SubModel->mCurrentStep);
				mHistorySequence.emplace_back(std::make_unique<swModelHistorySubModel>(SubModel->GetFileName(), std::move(SubModelEdit)));
			}
		}

		// the models above the replaced submodels take their new bounds
		if (SubModelReplacedCount && mProject)
		{
			std::vector<swModel*> UpdatedModels;

			for (const std::unique_ptr<swModel>& Model : mProject->GetModels())
				Model->UpdatePartData(UpdatedModels);

			gMainWindow->UpdateInUseCategory();
		}
	}

	if (FindAll)
		SetSelectionAndFocus(Selection, nullptr, 0, swSelectionMode::Single);
	else
		SetSelectionAndFocus(std::vector<swObject*>(), Focus, SW_PART_SECTION_POSITION, swSelectionMode::Single);

	ReplacedCount += SubModelReplacedCount;

	if (ReplacedCount)
	{
		EndHistorySequence(tr("Replace Parts", nullptr, ReplacedCount));

		gMainWindow->UpdateSelectedObjects(false);
		gMainWindow->UpdateTimeline(false, true);
	}
	else
		EndHistorySequence(tr("Selection"));
}
