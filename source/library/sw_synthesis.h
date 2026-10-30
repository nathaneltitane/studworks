#pragma once

#include "sw_math.h"
#include "sw_object_part.h"

class swLibraryMeshData;
class swMesh;

class swSynthesisInformation
{
public:
	explicit swSynthesisInformation(float Length);
	virtual ~swSynthesisInformation() = default;

	swSynthesisInformation(const swSynthesisInformation&) = delete;
	swSynthesisInformation(swSynthesisInformation&&) = delete;
	swSynthesisInformation& operator=(const swSynthesisInformation&) = delete;
	swSynthesisInformation& operator=(swSynthesisInformation&&) = delete;

	bool CanAddControlPoints() const
	{
		return mCurve;
	}

	bool IsCurve() const
	{
		return mCurve;
	}

	bool IsUnidirectional() const
	{
		return mUnidirectional;
	}

	bool IsNondirectional() const
	{
		return mNondirectional;
	}

	virtual void GetDefaultControlPoints(std::vector<swPartControlPoint>& ControlPoints) const = 0;
	virtual void VerifyControlPoints(std::vector<swPartControlPoint>& ControlPoints) const = 0;
	int InsertControlPoint(std::vector<swPartControlPoint>& ControlPoints, const swVector3& Start, const swVector3& End) const;
	// The control point rotation that runs the path along a section, curves only.
	virtual swMatrix33 GetSectionControlPointRotation(const swMatrix44& Section, const swMatrix33& Rotation) const
	{
		Q_UNUSED(Section);
		return Rotation;
	}
	// Turns the clamped middle control points along the path between their neighbors, curves only.
	virtual void ClampControlPointTangents(std::vector<swPartControlPoint>& ControlPoints) const
	{
		Q_UNUSED(ControlPoints);
	}
	// Turns one middle control point along the path between its neighbors once, curves only.
	virtual void AlignControlPointTangent(std::vector<swPartControlPoint>& ControlPoints, quint32 ControlPointIndex) const
	{
		Q_UNUSED(ControlPoints);
		Q_UNUSED(ControlPointIndex);
	}
	// Turns a control point so the path leaves it along a part space direction, curves only.
	virtual void AlignControlPointDirection(std::vector<swPartControlPoint>& ControlPoints, quint32 ControlPointIndex, const swVector3& Direction) const
	{
		Q_UNUSED(ControlPoints);
		Q_UNUSED(ControlPointIndex);
		Q_UNUSED(Direction);
	}
	// False when the control points are further apart than the part can reach without stretching.
	virtual bool IsReachable(const std::vector<swPartControlPoint>& ControlPoints) const
	{
		Q_UNUSED(ControlPoints);
		return true;
	}
	// The path the part follows in part space, the points before ReachedPointCount are covered by the part and the rest is too far for its length.
	virtual void GetPath(const std::vector<swPartControlPoint>& ControlPoints, std::vector<swVector3>& Points, size_t& ReachedPointCount) const
	{
		Q_UNUSED(ControlPoints);
		Points.clear();
		ReachedPointCount = 0;
	}
	// The library parts the path is built from, by file name and LDraw transform.
	std::vector<std::pair<std::string, swMatrix44>> GetParts(const std::vector<swPartControlPoint>& ControlPoints) const;
	std::unique_ptr<swMesh> BuildMesh(const std::vector<swPartControlPoint>& ControlPoints, const std::atomic_bool& Cancelled) const;

protected:
	using SectionCallbackFunc = std::function<void(const swVector3& CurvePoint, quint32 SegmentIndex, float t)>;
	virtual void CalculateSections(const std::vector<swPartControlPoint>& ControlPoints, std::vector<swMatrix44>& Sections, SectionCallbackFunc SectionCallback, const std::atomic_bool* Cancelled) const = 0;
	virtual void AddParts(swMemoryFile& File, swLibraryMeshData& MeshData, const std::vector<swMatrix44>& Sections, const std::atomic_bool* Cancelled) const = 0;

	bool mCurve = false;
	bool mUnidirectional = false;
	bool mNondirectional = false;
	float mLength;
};

void swSynthesisInitialize();
