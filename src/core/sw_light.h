#pragma once

#include "sw_object.h"
#include "sw_math.h"

enum swLightSection : quint32
{
	SW_LIGHT_SECTION_INVALID = SW_OBJECT_SECTION_INVALID,
	SW_LIGHT_SECTION_POSITION = 0,
	SW_LIGHT_SECTION_TARGET
};

enum class swLightType
{
	Point,
	Spot,
	Directional,
	Area,
	Count
};

enum class swLightAreaShape
{
	Rectangle,
	Square,
	Disk,
	Ellipse,
	Count
};

struct swLightHistoryState
{
	swObjectId Id;
	bool Hidden;
	QString Name;
	swLightType LightType;
	bool CastShadow;
	swObjectProperty<swVector3> Position;
	swObjectProperty<swMatrix33> Rotation;
	swObjectProperty<swVector3> Color;
	swObjectProperty<float> BlenderPower;
	swObjectProperty<float> BlenderRadius;
	swObjectProperty<float> BlenderAngle;
	swObjectProperty<float> POVRayPower;
	swObjectProperty<float> POVRayFadeDistance;
	swObjectProperty<float> POVRayFadePower;
	swObjectProperty<float> SpotConeAngle;
	swObjectProperty<float> SpotPenumbraAngle;
	swObjectProperty<float> POVRaySpotTightness;
	swLightAreaShape AreaShape;
	swObjectProperty<float> AreaSizeX;
	swObjectProperty<float> AreaSizeY;
	swObjectProperty<int> POVRayAreaGridX;
	swObjectProperty<int> POVRayAreaGridY;

	bool operator==(const swLightHistoryState& Other) const
	{
		return Id == Other.Id && Hidden == Other.Hidden && Name == Other.Name && LightType == Other.LightType &&
            CastShadow == Other.CastShadow && Position == Other.Position && Rotation == Other.Rotation && Color == Other.Color &&
            BlenderPower == Other.BlenderPower && BlenderRadius == Other.BlenderRadius && BlenderAngle == Other.BlenderAngle &&
            POVRayPower == Other.POVRayPower && POVRayFadeDistance == Other.POVRayFadeDistance && POVRayFadePower == Other.POVRayFadePower &&
            SpotConeAngle == Other.SpotConeAngle && SpotPenumbraAngle == Other.SpotPenumbraAngle &&
            POVRaySpotTightness == Other.POVRaySpotTightness && AreaShape == Other.AreaShape && AreaSizeX == Other.AreaSizeX &&
            AreaSizeY == Other.AreaSizeY && POVRayAreaGridX == Other.POVRayAreaGridX && POVRayAreaGridY == Other.POVRayAreaGridY;
	}
};

class swLight : public swObject
{
public:
	swLight();
	swLight(const swVector3& Position, swLightType LightType);
	virtual ~swLight() = default;

	swLight(const swLight&) = delete;
	swLight(swLight&&) = delete;
	swLight& operator=(const swLight&) = delete;
	swLight& operator=(swLight&&) = delete;

	static QString GetLightTypeString(swLightType LightType);
	static QStringList GetLightTypeStrings();
	static QString GetAreaShapeString(swLightAreaShape LightAreaShape);
	static QStringList GetAreaShapeStrings();

	bool IsPointLight() const
	{
		return mLightType == swLightType::Point;
	}

	bool IsSpotLight() const
	{
		return mLightType == swLightType::Spot;
	}

	bool IsDirectionalLight() const
	{
		return mLightType == swLightType::Directional;
	}

	bool IsAreaLight() const
	{
		return mLightType == swLightType::Area;
	}

	swLightType GetLightType() const
	{
		return mLightType;
	}

	bool SetLightType(swLightType LightType);

	quint32 GetAllowedTransforms() const override
	{
		if (IsPointLight())
			return SW_OBJECT_TRANSFORM_MOVE_XYZ;

		const quint32 Section = GetFocusSection();

		if (Section == SW_LIGHT_SECTION_POSITION || Section == SW_LIGHT_SECTION_INVALID)
			return SW_OBJECT_TRANSFORM_MOVE_XYZ | SW_OBJECT_TRANSFORM_ROTATE_XYZ;

		if (Section == SW_LIGHT_SECTION_TARGET)
			return SW_OBJECT_TRANSFORM_MOVE_XYZ;

		return 0;
	}

	swMatrix33 GetRelativeRotation() const
	{
		const quint32 Section = GetFocusSection();

		if (Section == SW_LIGHT_SECTION_POSITION)
			return swMatrix33(mWorldMatrix);
		else
			return swMatrix33Identity();
	}

	swVector3 GetSectionPosition(quint32 Section) const override
	{
		if (Section == SW_LIGHT_SECTION_POSITION)
			return mWorldMatrix.GetTranslation();

		if (Section == SW_LIGHT_SECTION_TARGET)
			return swMultiply31(swVector3(0.0f, 0.0f, -mTargetDistance), mWorldMatrix);

		return swVector3(0.0f, 0.0f, 0.0f);
	}

	void SetPosition(const swVector3& Position, swStep Step, bool AddKey)
	{
		mPosition.ChangeKey(Position, Step, AddKey);
	}

	void SetRotation(const swMatrix33& Rotation, swStep Step, bool AddKey)
	{
		mRotation.ChangeKey(Rotation, Step, AddKey);
	}

	swVector3 GetRotationCenter() const
	{
		const quint32 Section = GetFocusSection();

		if (Section == SW_LIGHT_SECTION_POSITION || Section == SW_LIGHT_SECTION_INVALID)
		{
			return mWorldMatrix.GetTranslation();
		}
		else
		{
			return swMultiply31(swVector3(0.0f, 0.0f, -mTargetDistance), mWorldMatrix);
		}
	}

	swVector3 GetPosition() const
	{
		return mWorldMatrix.GetTranslation();
	}

	swVector3 GetDirection() const
	{
		return -swVector3(mWorldMatrix[2]);
	}

	const swMatrix44& GetWorldMatrix() const
	{
		return mWorldMatrix;
	}

	void SaveLDraw(QTextStream& Stream) const;
	bool ParseLDrawLine(QTextStream& Stream);

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
	swLightHistoryState GetHistoryState() const;
	void SetHistoryState(const swLightHistoryState& State);

	void InsertTime(swStep Start, swStep Time);
	void RemoveTime(swStep Start, swStep Time);

	bool IsVisible() const
	{
		return !mHidden;
	}

	bool SetColor(const swVector3& Color, swStep Step, bool AddKey);

	swVector3 GetColor() const
	{
		return mColor;
	}

	bool SetPOVRayFadeDistance(float Distance, swStep Step, bool AddKey);

	float GetPOVRayFadeDistance() const
	{
		return mPOVRayFadeDistance;
	}

	bool SetPOVRayFadePower(float Power, swStep Step, bool AddKey);

	float GetPOVRayFadePower() const
	{
		return mPOVRayFadePower;
	}

	bool SetSpotConeAngle(float Angle, swStep Step, bool AddKey);

	float GetSpotConeAngle() const
	{
		return mSpotConeAngle;
	}

	bool SetSpotPenumbraAngle(float Angle, swStep Step, bool AddKey);

	float GetSpotPenumbraAngle() const
	{
		return mSpotPenumbraAngle;
	}

	bool SetSpotPOVRayTightness(float Angle, swStep Step, bool AddKey);

	float GetSpotPOVRayTightness() const
	{
		return mPOVRaySpotTightness;
	}

	bool SetAreaShape(swLightAreaShape LightAreaShape);

	swLightAreaShape GetAreaShape() const
	{
		return mAreaShape;
	}

	bool SetAreaPOVRayGridX(int AreaGrid, swStep Step, bool AddKey);

	int GetAreaPOVRayGridX() const
	{
		return mPOVRayAreaGridX;
	}

	bool SetAreaPOVRayGridY(int AreaGrid, swStep Step, bool AddKey);

	int GetAreaPOVRayGridY() const
	{
		return mPOVRayAreaGridY;
	}

	bool SetBlenderRadius(float Radius, swStep Step, bool AddKey);

	float GetBlenderRadius() const
	{
		return mBlenderRadius;
	}

	bool SetBlenderAngle(float Angle, swStep Step, bool AddKey);

	float GetBlenderAngle() const
	{
		return mBlenderAngle * SW_RADIANS_TO_DEGREES;
	}

	bool SetAreaSizeX(float Size, swStep Step, bool AddKey);

	float GetAreaSizeX() const
	{
		return mAreaSizeX;
	}

	bool SetAreaSizeY(float Size, swStep Step, bool AddKey);

	float GetAreaSizeY() const
	{
		return mAreaSizeY;
	}

	bool SetBlenderPower(float Power, swStep Step, bool AddKey);

	float GetBlenderPower() const
	{
		return mBlenderPower;
	}

	bool SetPOVRayPower(float Power, swStep Step, bool AddKey);

	float GetPOVRayPower() const
	{
		return mPOVRayPower;
	}

	bool SetCastShadow(bool CastShadow);

	bool GetCastShadow() const
	{
		return mCastShadow;
	}

	bool SetName(const QString& Name);

	QString GetName() const override
	{
		return mName;
	}

	void CompareBoundingBox(swVector3& Min, swVector3& Max);
	void UpdatePosition(swStep Step) override;
	void MoveSelected(swStep Step, bool AddKey, const swVector3& Distance, bool FirstMove);
	void Rotate(swStep Step, bool AddKey, const swMatrix33& RotationMatrix, const swVector3& Center, const swMatrix33& RotationFrame);
	void CreateName(const std::vector<std::unique_ptr<swLight>>& Lights);

protected:
	void DrawPointLight(swContext* Context) const;
	void DrawSpotLight(swContext* Context) const;
	void DrawDirectionalLight(swContext* Context) const;
	void DrawAreaLight(swContext* Context) const;

	void SetupLightMatrix(swContext* Context) const;
	void DrawSphere(swContext* Context, const swVector3& Center, float Radius) const;
	void DrawCylinder(swContext* Context, float Radius, float Height) const;
	void DrawTarget(swContext* Context) const;
	void DrawCone(swContext* Context, float TargetDistance) const;

	QString mName;
	swLightType mLightType = swLightType::Point;
	bool mCastShadow = true;
	swObjectProperty<swVector3> mPosition = swObjectProperty<swVector3>(swVector3(0.0f, 0.0f, 0.0f));
	swObjectProperty<swMatrix33> mRotation = swObjectProperty<swMatrix33>(swMatrix33Identity());
	swObjectProperty<swVector3> mColor = swObjectProperty<swVector3>(swVector3(1.0f, 1.0f, 1.0f));
	swObjectProperty<float> mBlenderPower = swObjectProperty<float>(10.0f);
	swObjectProperty<float> mBlenderRadius = swObjectProperty<float>(0.0f);
	swObjectProperty<float> mBlenderAngle = swObjectProperty<float>(0.526f * SW_DEGREES_TO_RADIANS);
	swObjectProperty<float> mPOVRayPower = swObjectProperty<float>(1.0f);
	swObjectProperty<float> mPOVRayFadeDistance = swObjectProperty<float>(0.0f);
	swObjectProperty<float> mPOVRayFadePower = swObjectProperty<float>(0.0f);
	swObjectProperty<float> mSpotConeAngle = swObjectProperty<float>(80.0f);
	swObjectProperty<float> mSpotPenumbraAngle = swObjectProperty<float>(0.0f);
	swObjectProperty<float> mPOVRaySpotTightness = swObjectProperty<float>(0.0f);
	swLightAreaShape mAreaShape = swLightAreaShape::Rectangle;
	swObjectProperty<float> mAreaSizeX = swObjectProperty<float>(250.0f);
	swObjectProperty<float> mAreaSizeY = swObjectProperty<float>(250.0f);
	swObjectProperty<int> mPOVRayAreaGridX = swObjectProperty<int>(2);
	swObjectProperty<int> mPOVRayAreaGridY = swObjectProperty<int>(2);

	swVector3 mTargetMovePosition = swVector3(0.0f, 0.0f, 0.0f);
	swMatrix44 mWorldMatrix;

	static constexpr float mTargetDistance = 50.0f;
};
