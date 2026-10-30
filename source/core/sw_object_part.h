#pragma once

class swPartData;

#include "sw_object.h"
#include "sw_colors.h"
#include "sw_math.h"
#include "sw_types_asset.h"

#define SW_MAXIMUM_CONTROL_POINTS 1000

enum swPartSection : quint32
{
	SW_PART_SECTION_INVALID = SW_OBJECT_SECTION_INVALID,
	SW_PART_SECTION_POSITION = 0,
	SW_PART_SECTION_CONTROL_POINT_FIRST,
	SW_PART_SECTION_CONTROL_POINT_LAST = SW_PART_SECTION_CONTROL_POINT_FIRST + SW_MAXIMUM_CONTROL_POINTS - 1,
	SW_PART_SECTION_TRAIN_TRACK_CONNECTION_FIRST = 1
};

struct swPartControlPoint
{
	swMatrix44 Transform;
	float Scale;
	// A clamped middle control point turns along the path between its neighbors, a released one keeps its own rotation.
	bool TangentClamped = false;

	bool operator==(const swPartControlPoint& Other) const
	{
		return Transform == Other.Transform && Scale == Other.Scale && TangentClamped == Other.TangentClamped;
	}
};

struct swPartHistoryState
{
	swObjectId Id;
	bool Hidden;
	int FileLine;
	QString PartId;
	uint32_t GroupId;
	int ColorIndex;
	quint32 ColorCode;
	swStep StepShow;
	swStep StepHide;
	swMatrix44 PivotMatrix;
	bool PivotPointValid;
	std::vector<swPartControlPoint> ControlPoints;
	swObjectProperty<swVector3> Position;
	swObjectProperty<swMatrix33> Rotation;

	bool operator==(const swPartHistoryState& Other) const
	{
		return Id == Other.Id && Hidden == Other.Hidden && FileLine == Other.FileLine && PartId == Other.PartId &&
            GroupId == Other.GroupId && ColorIndex == Other.ColorIndex && ColorCode == Other.ColorCode &&
            StepShow == Other.StepShow && StepHide == Other.StepHide && PivotMatrix == Other.PivotMatrix &&
            PivotPointValid == Other.PivotPointValid && ControlPoints == Other.ControlPoints &&
            Position == Other.Position && Rotation == Other.Rotation;
	}
};

class swPart : public swObject
{
public:
	swPart();
	swPart(swPartData* PartData);
	swPart(swPartDataReference PartDataRef);
	swPart(const swPart& Other);
	virtual ~swPart();

	swPart(swPart&&) = delete;
	swPart& operator=(const swPart&) = delete;
	swPart& operator=(swPart&&) = delete;

	quint32 GetAllowedTransforms() const override;
	swVector3 GetSectionPosition(quint32 Section) const override;

	void SaveLDraw(QTextStream& Stream) const;
	bool ParseLDrawLine(QTextStream& Stream);

	void SetFileLine(int Line)
	{
		mFileLine = Line;
	}

	int GetFileLine() const
	{
		return mFileLine;
	}

	void RayTest(swObjectRayTest& ObjectRayTest) const override;
	void BoxTest(swObjectBoxTest& ObjectBoxTest) const override;
	bool MinIntersectDist(const swVector3& Start, const swVector3& End, float& MinDistance, swPartRayTest& PartRayTest) const;
	bool IntersectsPlanes(const swMatrix44& WorldMatrix, const swVector4 Planes[6]) const;
	void DrawInterface(swContext* Context, const swScene& Scene) const override;
	QVariant GetPropertyValue(swObjectPropertyId PropertyId) const override;
	bool SetPropertyValue(swObjectPropertyId PropertyId, swStep Step, bool AddKey, QVariant Value) override;
	bool HasKeyFrame(swObjectPropertyId PropertyId, swStep Time) const override;
	swStep GetLastStep() const override;
	bool SetKeyFrame(swObjectPropertyId PropertyId, swStep Time, bool KeyFrame) override;
	void RemoveKeyFrames() override;
	swPartHistoryState GetHistoryState() const;
	void SetHistoryState(const swPartHistoryState& State, Project* Project, swGroup* Group);

	void AddMainModelRenderMeshes(swScene* Scene, bool Highlight, bool Fade) const;
	void AddSubModelRenderMeshes(swScene* Scene, const swMatrix44& WorldMatrix, int DefaultColorIndex, swRenderMeshState RenderMeshState, bool ParentActive) const;
	void SubModelCompareBoundingBox(const swMatrix44& WorldMatrix, swVector3& Min, swVector3& Max) const;
	void SubModelAddBoundingBoxPoints(const swMatrix44& WorldMatrix, std::vector<swVector3>& Points, swGeometryBoundsMode Mode) const;

	void InsertTime(swStep Start, swStep Time);
	void RemoveTime(swStep Start, swStep Time);

	bool IsHidden() const
	{
		return mHidden;
	}

	void SetHidden(bool Hidden)
	{
		mHidden = Hidden;
	}

	const std::vector<swPartControlPoint>& GetControlPoints() const
	{
		return mControlPoints;
	}

	void SetControlPoints(const std::vector<swPartControlPoint>& ControlPoints)
	{
		mControlPoints = ControlPoints;
		UpdateMesh();
	}

	void SetControlPointScale(int ControlPointIndex, float Scale)
	{
		mControlPoints[ControlPointIndex].Scale = Scale;
		UpdateMesh();
	}

	bool IsTrainTrackConnected(int ConnectionIndex) const
	{
		return mTrainTrackConnections[ConnectionIndex];
	}

	void SetTrainTrackConnections(std::vector<bool>&& Connections)
	{
		mTrainTrackConnections = std::move(Connections);
	}

	const QString& GetID() const
	{
		return mID;
	}

	void UpdateID();

	QString GetName() const override;
	bool IsVisible(swStep Step) const;
	bool IsVisibleInSubModel() const;

	bool AreTrainTrackConnectionsVisible() const
	{
		return IsFocused();
	}

	void GetModelParts(const swMatrix44& WorldMatrix, int DefaultColorIndex, std::vector<swModelPartsEntry>& ModelParts) const;
	void Initialize(const swMatrix44& WorldMatrix, swStep Step);
	const swBoundingBox& GetBoundingBox() const;
	void CompareBoundingBox(swVector3& Min, swVector3& Max) const;
	void SetPartData(swPartDataReference PartDataRef, const QString& ID, bool Wait, bool UpdateSynthInfo);
	bool SetPartId(swPartData* PartData);
	bool FileLoad(swFile& file, Project* Project);

	void UpdatePosition(swStep Step) override;
	void MoveSelected(swStep Step, bool AddKey, const swVector3& Distance);
	void Rotate(swStep Step, bool AddKey, const swMatrix33& RotationMatrix, const swVector3& Center, const swMatrix33& RotationFrame);
	void MovePivotPoint(const swVector3& Distance);
	void RotatePivotPoint(const swMatrix33& RotationMatrix);

	bool CanAddControlPoint() const;
	bool CanRemoveControlPoint() const;
	bool CanClampControlPointTangent() const;
	bool IsControlPointTangentClamped() const;

	bool InsertControlPoint(const swVector3& WorldStart, const swVector3& WorldEnd);
	bool RemoveFocusedControlPoint();
	bool ToggleFocusedControlPointTangent();
	void VerifyControlPoints(std::vector<swPartControlPoint>& ControlPoints) const;
	bool IsGeneratedMeshPending() const { return mSynthPending; }
	// An older mesh may remain visible while its replacement is pending.
	bool HasGeneratedMesh() const { return mMesh != nullptr && !mSynthPending; }
	void SetGeneratedMesh(swMesh* Mesh);
	void RefreshGeneratedMesh();

	swGroup* GetTopGroup();

	void SetGroup(swGroup* Group)
	{
		mGroup = Group;
	}

	swGroup* GetGroup() const
	{
		return mGroup;
	}

	swStep GetStepShow() const
	{
		return mStepShow;
	}

	swStep GetStepHide() const
	{
		return mStepHide;
	}

	void SetStepHide(swStep Step)
	{
		if (Step < 2)
			Step = 2;

		mStepHide = Step;

		if (mStepHide <= mStepShow)
			SetStepShow(mStepHide - 1);
	}

	void SetStepShow(swStep Step)
	{
		if (Step == SW_STEP_MAXIMUM)
			Step = SW_STEP_MAXIMUM - 1;

		if (mStepShow != Step)
		{
			mFileLine = -1;
			mStepShow = Step;
		}

		if (mStepHide <= mStepShow)
			mStepHide = mStepShow + 1;
	}

	quint32 GetColorCode() const
	{
		return mColorCode;
	}

	void SetColorCode(quint32 ColorCode)
	{
		mColorCode = ColorCode;
		mColorIndex = swGetColorIndex(ColorCode);
	}

	int GetColorIndex() const
	{
		return mColorIndex;
	}

	bool SetColorIndex(int ColorIndex)
	{
		if (mColorIndex == ColorIndex)
			return false;

		mColorIndex = ColorIndex;
		mColorCode = swGetColorCode(ColorIndex);

		return true;
	}

	void SetPosition(const swVector3& Position, swStep Step, bool AddKey)
	{
		mPosition.ChangeKey(Position, Step, AddKey);
	}

	void SetRotation(const swMatrix33& Rotation, swStep Step, bool AddKey)
	{
		mRotation.ChangeKey(Rotation, Step, AddKey);
	}

	swVector3 GetRotationCenter() const;
	swMatrix33 GetRelativeRotation() const;

	void ResetPivotPoint()
	{
		mPivotPointValid = false;
		mPivotMatrix = swMatrix44Identity();
	}

public:
	swPartDataReference mPartData;

	swMatrix44 mModelWorld;
	swMatrix44 mPivotMatrix = swMatrix44Identity();

protected:
	void UpdateMesh();
	swMesh* GetDisplayMesh() const;

	bool IsPivotPointVisible() const
	{
		return mPivotPointValid && IsFocused();
	}

	bool AreControlPointsVisible() const
	{
		return IsSelected();
	}

	void DrawSynthInterface(swContext* Context, const swMatrix44& WorldMatrix) const;

	swObjectProperty<swVector3> mPosition = swObjectProperty<swVector3>(swVector3(0.0f, 0.0f, 0.0f));
	swObjectProperty<swMatrix33> mRotation = swObjectProperty<swMatrix33>(swMatrix33Identity());

	int mFileLine = -1;
	QString mID;

	swGroup* mGroup = nullptr;

	int mColorIndex;
	quint32 mColorCode;

	swStep mStepShow = 1;
	swStep mStepHide = SW_STEP_MAXIMUM;

	bool mPivotPointValid = false;
	std::vector<swPartControlPoint> mControlPoints;
	std::vector<bool> mTrainTrackConnections;
	swMesh* mMesh = nullptr;
	bool mSynthPending = false;

	// Path line drawn with the control points, kept until the control points change.
	mutable std::vector<swPartControlPoint> mPathControlPoints;
	mutable std::vector<swVector3> mPathPoints;
	mutable size_t mPathReachedPointCount = 0;
};
