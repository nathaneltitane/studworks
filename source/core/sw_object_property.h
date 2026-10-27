#pragma once

enum class swObjectPropertyId
{
	PartId,
	PartColor,
	PartStepShow,
	PartStepHide,
	CameraName,
	CameraProjection,
	CameraFOV,
	CameraNear,
	CameraFar,
	CameraPositionX,
	CameraPositionY,
	CameraPositionZ,
	CameraTargetX,
	CameraTargetY,
	CameraTargetZ,
	CameraUpX,
	CameraUpY,
	CameraUpZ,
	LightName,
	LightType,
	LightColor,
	LightBlenderPower,
	LightPOVRayPower,
	LightCastShadow,
	LightPOVRayFadeDistance,
	LightPOVRayFadePower,
	LightBlenderRadius,
	LightBlenderAngle,
	LightAreaSizeX,
	LightAreaSizeY,
	LightSpotConeAngle,
	LightSpotPenumbraAngle,
	LightPOVRaySpotTightness,
	LightAreaShape,
	LightPOVRayAreaGridX,
	LightPOVRayAreaGridY,
	ObjectPositionX,
	ObjectPositionY,
	ObjectPositionZ,
	ObjectRotationX,
	ObjectRotationY,
	ObjectRotationZ,
	Count
};

enum class swFloatPropertySnap
{
	Auto,
	PartPositionXY,
	PartPositionZ,
	Position,
	Rotation
};

template<typename T>
struct swObjectPropertyKey
{
	swStep Step;
	T Value;

	bool operator==(const swObjectPropertyKey& Other) const
	{
		return Step == Other.Step && Value == Other.Value;
	}
};

template<typename T>
class swObjectProperty
{
public:
	swObjectProperty() = default;

	explicit swObjectProperty(const T& DefaultValue)
		: mValue(DefaultValue)
	{
	}

	operator const T& () const
	{
		return mValue;
	}

	bool operator==(const swObjectProperty& Other) const
	{
		return mValue == Other.mValue && mKeys == Other.mKeys;
	}

	void SetValue(const T& Value)
	{
		mValue = Value;
	}

	void RemoveAllKeys()
	{
		mKeys.clear();
	}

	swStep GetLastKeyFrame() const
	{
		return mKeys.empty() ? 0 : mKeys.back().Step;
	}

	void Update(swStep Step);
	bool ChangeKey(const T& Value, swStep Step, bool AddKey);
	void InsertTime(swStep Start, swStep Time);
	void RemoveTime(swStep Start, swStep Time);
	bool HasKeyFrame(swStep Time) const;
	bool SetKeyFrame(swStep Time, bool KeyFrame);

	void Save(QTextStream& Stream, const char* ObjectName, const char* VariableName, bool SaveEmpty) const;
	bool Load(QTextStream& Stream, const QString& Token, const char* VariableName);

protected:
	T mValue;
	std::vector<swObjectPropertyKey<T>> mKeys;
};
