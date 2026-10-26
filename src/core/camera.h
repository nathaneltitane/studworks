#pragma once

#include "object.h"
#include "sw_math.h"

enum class swViewpoint
{
	Front,
	Back,
	Top,
	Bottom,
	Left,
	Right,
	Home,
	Count
};

enum class swCameraProjection
{
	Perspective,
	Orthographic,
	Count
};

enum swCameraSection : quint32
{
	SW_CAMERA_SECTION_INVALID = SW_OBJECT_SECTION_INVALID,
	SW_CAMERA_SECTION_POSITION = 0,
	SW_CAMERA_SECTION_TARGET,
	SW_CAMERA_SECTION_UPVECTOR
};

struct swCameraHistoryState
{
	swObjectId Id;
	bool Hidden;
	bool Simple;
	float Fovy;
	float NearPlane;
	float FarPlane;
	swCameraProjection Projection;
	swObjectProperty<swVector3> Position;
	swObjectProperty<swVector3> TargetPosition;
	swObjectProperty<swVector3> UpVector;
	QString Name;

	bool operator==(const swCameraHistoryState& Other) const
	{
		return Id == Other.Id && Hidden == Other.Hidden && Simple == Other.Simple && Fovy == Other.Fovy &&
            NearPlane == Other.NearPlane && FarPlane == Other.FarPlane && Projection == Other.Projection &&
            Position == Other.Position && TargetPosition == Other.TargetPosition && UpVector == Other.UpVector && Name == Other.Name;
	}
};

class swCamera : public swObject
{
public:
	swCamera();
	swCamera(bool Simple);
	swCamera(bool Simple, const swVector3& Position, const swVector3& TargetPosition);
	virtual ~swCamera();

	swCamera(const swCamera&) = delete;
	swCamera(swCamera&&) = delete;
	swCamera& operator=(const swCamera&) = delete;
	swCamera& operator=(swCamera&&) = delete;

	static QString GetCameraProjectionString(swCameraProjection CameraProjection);
	static QStringList GetCameraProjectionStrings();
	static swViewpoint GetViewpoint(const QString& ViewpointName);

	QString GetName() const override
	{
		return mName;
	}

	bool SetName(const QString& Name);
	void CreateName(const std::vector<std::unique_ptr<swCamera>>& Cameras);

	bool IsSimple() const
	{
		return mSimple;
	}

	swCameraProjection GetProjection() const
	{
		return mProjection;
	}

	bool SetProjection(swCameraProjection CameraProjection);
	bool SetFOV(float Fovy);
	bool SetNearPlane(float NearPlane);
	bool SetFarPlane(float FarPlane);

	quint32 GetAllowedTransforms() const override
	{
		return SW_OBJECT_TRANSFORM_MOVE_XYZ;
	}

	swVector3 GetSectionPosition(quint32 Section) const override
	{
		switch (Section)
		{
		case SW_CAMERA_SECTION_POSITION:
			return mPosition;

		case SW_CAMERA_SECTION_TARGET:
			return mTargetPosition;

		case SW_CAMERA_SECTION_UPVECTOR:
			return swMul31(swVector3(0, 25, 0), swMatrix44AffineInverse(mWorldView));
		}

		return swVector3(0.0f, 0.0f, 0.0f);
	}

	swMatrix33 GetRelativeRotation() const
	{
		const quint32 Section = GetFocusSection();

		if (Section == SW_CAMERA_SECTION_POSITION)
			return swMatrix33(mWorldView);
		else
			return swMatrix33Identity();
	}

	swVector3 GetRotationCenter() const
	{
		const quint32 Section = GetFocusSection();

		if (Section != SW_CAMERA_SECTION_TARGET)
		{
			return mPosition;
		}
		else
		{
			return mTargetPosition;
		}
	}

	void SaveLDraw(QTextStream& Stream) const;
	bool ParseLDrawLine(QTextStream& Stream);

public:
	bool IsVisible() const
	{
		return !mHidden;
	}

	bool IsHidden() const
	{
		return mHidden;
	}

	void SetHidden(bool Hidden)
	{
		mHidden = Hidden;
	}

	void SetPosition(const swVector3& Position, swStep Step, bool AddKey)
	{
		mPosition.ChangeKey(Position, Step, AddKey);
	}

	void SetTargetPosition(const swVector3& TargetPosition, swStep Step, bool AddKey)
	{
		mTargetPosition.ChangeKey(TargetPosition, Step, AddKey);
	}

	void SetUpVector(const swVector3& UpVector, swStep Step, bool AddKey)
	{
		mUpVector.ChangeKey(UpVector, Step, AddKey);
	}

	float GetOrthoHeight() const
	{
		// Compute the FOV/plane intersection radius.
		//                d               d
		//   a = 2 atan(------) => ~ a = --- => d = af
		//                2f              f
		const float f = (mPosition - mTargetPosition).Length();
		return (m_fovy * f) * (SW_PI / 180.0f);
	}

public:
	void RayTest(swObjectRayTest& ObjectRayTest) const override;
	void BoxTest(swObjectBoxTest& ObjectBoxTest) const override;
	void DrawInterface(swContext* Context, const swScene& Scene) const override;
	QVariant GetPropertyValue(swObjectPropertyId PropertyId) const override;
	bool SetPropertyValue(swObjectPropertyId PropertyId, swStep Step, bool AddKey, QVariant Value) override;
	bool HasKeyFrame(swObjectPropertyId PropertyId, swStep Time) const override;
	swStep GetLastStep() const override;
	bool SetKeyFrame(swObjectPropertyId PropertyId, swStep Time, bool KeyFrame) override;
	void RemoveKeyFrames() override;
	swCameraHistoryState GetHistoryState() const;
	void SetHistoryState(const swCameraHistoryState& State);

	void InsertTime(swStep Start, swStep Time);
	void RemoveTime(swStep Start, swStep Time);

	static bool FileLoad(swFile& file);

	void CompareBoundingBox(swVector3& Min, swVector3& Max);
	void UpdatePosition(swStep Step) override;
	void CopyPosition(const swCamera* Camera);
	void CopySettings(const swCamera* Camera);

	void ZoomExtents(float AspectRatio, const swVector3& Center, const std::vector<swVector3>& Points, swStep Step, bool AddKey, bool AdjustFarPlane);
	void ExpandFarPlane(const std::vector<swVector3>& Points);
	void ZoomRegion(float AspectRatio, const swVector3& Position, const swVector3& TargetPosition, const swVector3* Corners, swStep Step, bool AddKey);
	void Zoom(float Distance, swStep Step, bool AddKey);
	void Pan(const swVector3& Distance, swStep Step, bool AddKey);
	void Orbit(float DistanceX, float DistanceY, const swVector3& CenterPosition, swStep Step, bool AddKey);
	void Roll(float Distance, swStep Step, bool AddKey);
	void Center(const swVector3& NewCenter, swStep Step, bool AddKey);
	void MoveSelected(swStep Step, bool AddKey, const swVector3& Distance);
	void MoveRelative(const swVector3& Distance, swStep Step, bool AddKey);
	void Rotate(swStep Step, bool AddKey, const swMatrix33& RotationMatrix, const swVector3& Center, const swMatrix33& RotationFrame);
	void SetViewpoint(swViewpoint Viewpoint);
	void SetViewpoint(const swVector3& Position);
	void SetViewpoint(const swVector3& Position, const swVector3& Target, const swVector3& Up);
	void GetAngles(float& Latitude, float& Longitude, float& Distance) const;
	void SetAngles(float Latitude, float Longitude, float Distance);

	float m_fovy = 30.0f;
	float m_zNear = 25.0f;
	static constexpr float DefaultFarPlane = 50000.0f;
	float m_zFar = DefaultFarPlane;

	swMatrix44 mWorldView;
	swObjectProperty<swVector3> mPosition = swObjectProperty<swVector3>(swVector3(0.0f, 0.0f, 0.0f));
	swObjectProperty<swVector3> mTargetPosition = swObjectProperty<swVector3>(swVector3(0.0f, 0.0f, 0.0f));
	swObjectProperty<swVector3> mUpVector = swObjectProperty<swVector3>(swVector3(0.0f, 0.0f, 0.0f));

protected:
	QString mName;
	bool mSimple = true;
	swCameraProjection mProjection = swCameraProjection::Perspective;
};
