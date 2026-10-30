#pragma once

#include "sw_math.h"
#include "sw_object_property.h"

enum class swObjectType
{
	Part,
	Camera,
	Light
};

enum class swObjectId : uint32_t;

struct swObjectSection
{
	swObject* Object = nullptr;
	quint32 Section = 0;
};

struct swPartRayTest
{
	const swPartData* PartData = nullptr;
	swMatrix44 Transform;
	swVector3 Plane;
	swVector3 Normal = swVector3(0.0f, 0.0f, 1.0f);
};

struct swObjectRayTest
{
	swCamera* ViewCamera;
	bool PartsOnly;
	bool IgnoreSelected;
	swVector3 Start;
	swVector3 End;
	float Distance = FLT_MAX;
	swObjectSection ObjectSection;
	swPartRayTest PartRayTest;
};

struct swObjectBoxTest
{
	swCamera* ViewCamera;
	swVector4 Planes[6];
	std::vector<swObject*> Objects;
};

#define SW_OBJECT_TRANSFORM_MOVE_X    0x001
#define SW_OBJECT_TRANSFORM_MOVE_Y    0x002
#define SW_OBJECT_TRANSFORM_MOVE_Z    0x004
#define SW_OBJECT_TRANSFORM_MOVE_XYZ (SW_OBJECT_TRANSFORM_MOVE_X | SW_OBJECT_TRANSFORM_MOVE_Y | SW_OBJECT_TRANSFORM_MOVE_Z)
#define SW_OBJECT_TRANSFORM_ROTATE_X  0x010
#define SW_OBJECT_TRANSFORM_ROTATE_Y  0x020
#define SW_OBJECT_TRANSFORM_ROTATE_Z  0x040
#define SW_OBJECT_TRANSFORM_ROTATE_XYZ (SW_OBJECT_TRANSFORM_ROTATE_X | SW_OBJECT_TRANSFORM_ROTATE_Y | SW_OBJECT_TRANSFORM_ROTATE_Z)
#define SW_OBJECT_TRANSFORM_SCALE_X   0x100
#define SW_OBJECT_TRANSFORM_SCALE_Y   0x200
#define SW_OBJECT_TRANSFORM_SCALE_Z   0x400
#define SW_OBJECT_TRANSFORM_SCALE_XYZ (SW_OBJECT_TRANSFORM_SCALE_X | SW_OBJECT_TRANSFORM_SCALE_Y | SW_OBJECT_TRANSFORM_SCALE_Z)

#define SW_OBJECT_SECTION_INVALID 0xffffffff

class swObject
{
public:
	swObject(swObjectType ObjectType);
	virtual ~swObject();

	swObject(const swObject&) = delete;
	swObject(swObject&&) = delete;
	swObject& operator=(const swObject&) = delete;
	swObject& operator=(swObject&&) = delete;

public:
	bool IsPart() const
	{
		return mObjectType == swObjectType::Part;
	}

	bool IsCamera() const
	{
		return mObjectType == swObjectType::Camera;
	}

	bool IsLight() const
	{
		return mObjectType == swObjectType::Light;
	}

	swObjectType GetType() const
	{
		return mObjectType;
	}

	swObjectId GetId() const
	{
		return mId;
	}

	bool IsSelected() const
	{
		return mSelected;
	}

	void SetSelected(bool Selected)
	{
		if (Selected != mSelected)
			mSelectedSection = SW_OBJECT_SECTION_INVALID;

		mSelected = Selected;

		if (!Selected)
			mFocusedSection = SW_OBJECT_SECTION_INVALID;
	}

	bool IsFocused() const
	{
		return mFocusedSection != SW_OBJECT_SECTION_INVALID;
	}

	bool IsFocused(quint32 Section) const
	{
		return mFocusedSection == Section;
	}

	void SetFocused(quint32 Section, bool Focused)
	{
		if (Focused)
		{
			mFocusedSection = Section;
			mSelectedSection = Section;
			mSelected = true;
		}
		else
			mFocusedSection = SW_OBJECT_SECTION_INVALID;
	}

	quint32 GetFocusSection() const
	{
		return mFocusedSection;
	}

	// The section the object was last focused on, kept while it stays selected after the focus moves on.
	quint32 GetSelectedSection() const
	{
		return mSelectedSection;
	}

	void SetSelectedSection(quint32 Section)
	{
		mSelectedSection = Section;
	}

	// The section a move or rotate of the selection applies to.
	quint32 GetTransformSection() const
	{
		return IsFocused() ? mFocusedSection : mSelectedSection;
	}

	virtual void UpdatePosition(swStep Step) = 0;
	virtual quint32 GetAllowedTransforms() const = 0;
	virtual swVector3 GetSectionPosition(quint32 Section) const = 0;
	virtual void RayTest(swObjectRayTest& ObjectRayTest) const = 0;
	virtual void BoxTest(swObjectBoxTest& ObjectBoxTest) const = 0;
	virtual void DrawInterface(swContext* Context, const swScene& Scene) const = 0;
	virtual QVariant GetPropertyValue(swObjectPropertyId PropertyId) const = 0;
	virtual bool SetPropertyValue(swObjectPropertyId PropertyId, swStep Step, bool AddKey, QVariant Value) = 0;
	virtual bool HasKeyFrame(swObjectPropertyId PropertyId, swStep Time) const = 0;
	virtual swStep GetLastStep() const = 0;
	virtual bool SetKeyFrame(swObjectPropertyId PropertyId, swStep Time, bool KeyFrame) = 0;
	virtual void RemoveKeyFrames() = 0;
	virtual QString GetName() const = 0;
	static QString GetCheckpointString(swObjectPropertyId PropertyId);

private:
	swObjectType mObjectType;

protected:
	bool mHidden = false;
	bool mSelected = false;
	quint32 mFocusedSection = SW_OBJECT_SECTION_INVALID;
	quint32 mSelectedSection = SW_OBJECT_SECTION_INVALID;
	swObjectId mId = static_cast<swObjectId>(0);

	static swObjectId mNextId;
};
