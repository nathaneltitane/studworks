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

	// Adds a halfway control point.
	void AddMiddleControlPoint(std::vector<swPartControlPoint>& ControlPoints) const;

	// Control point rotation along a section.
	virtual swMatrix33 GetSectionControlPointRotation(const swMatrix44& Section, const swMatrix33& Rotation) const
	{
		Q_UNUSED(Section);
		return Rotation;
	}

	// Turns the clamped control points.
	virtual void ClampControlPointTangents(std::vector<swPartControlPoint>& ControlPoints) const
	{
		Q_UNUSED(ControlPoints);
	}

	// Turns one middle control point.
	virtual void AlignControlPointTangent(std::vector<swPartControlPoint>& ControlPoints, quint32 ControlPointIndex) const
	{
		Q_UNUSED(ControlPoints);
		Q_UNUSED(ControlPointIndex);
	}

	// The path direction at a control point.
	virtual swVector3 GetControlPointDirection(const swMatrix44& Transform) const
	{
		return swVector3(Transform[0][0], Transform[0][1], Transform[0][2]);
	}

	// Turns a control point to a direction.
	virtual void AlignControlPointDirection(std::vector<swPartControlPoint>& ControlPoints, quint32 ControlPointIndex, const swVector3& Direction) const
	{
		Q_UNUSED(ControlPoints);
		Q_UNUSED(ControlPointIndex);
		Q_UNUSED(Direction);
	}

	// False when the part cannot reach.
	virtual bool IsReachable(const std::vector<swPartControlPoint>& ControlPoints) const
	{
		Q_UNUSED(ControlPoints);
		return true;
	}

	// The part space path.
	virtual void GetPath(const std::vector<swPartControlPoint>& ControlPoints, std::vector<swVector3>& Points, size_t& ReachedPointCount, std::vector<float>& HandleLengths) const
	{
		Points.clear();
		ReachedPointCount = 0;
		HandleLengths.clear();

		for (const swPartControlPoint& ControlPoint : ControlPoints)
			HandleLengths.emplace_back(ControlPoint.Scale);
	}

	// The spread for a handle length.
	virtual float GetSpreadForHandleLength(const std::vector<swPartControlPoint>& ControlPoints, quint32 ControlPointIndex, float HandleLength, float CurrentHandleLength) const
	{
		Q_UNUSED(ControlPoints);
		Q_UNUSED(ControlPointIndex);
		Q_UNUSED(CurrentHandleLength);
		return HandleLength;
	}

	// The parts the path is built from.
	std::vector<std::pair<std::string, swMatrix44>> GetParts(const std::vector<swPartControlPoint>& ControlPoints) const;
	std::unique_ptr<swMesh> BuildMesh(const std::vector<swPartControlPoint>& ControlPoints, const std::atomic_bool& Cancelled) const;

protected:
	using SectionCallbackFunc = std::function<void(const swVector3& CurvePoint, quint32 SegmentIndex, float t)>;
	virtual void CalculateSections(const std::vector<swPartControlPoint>& ControlPoints, std::vector<swMatrix44>& Sections, SectionCallbackFunc SectionCallback, const std::atomic_bool* Cancelled) const = 0;
	virtual void AddParts(swMemoryFile& File, swLibraryMeshData& MeshData, const std::vector<swMatrix44>& Sections, const std::atomic_bool* Cancelled) const = 0;
	int InsertControlPointAt(std::vector<swPartControlPoint>& ControlPoints, quint32 SegmentIndex, float Time, const swVector3& Position, const swMatrix44& Section) const;

	bool mCurve = false;
	bool mUnidirectional = false;
	bool mNondirectional = false;
	float mLength;
};

void swSynthesisInitialize();
