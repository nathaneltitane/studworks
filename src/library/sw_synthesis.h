#pragma once

#include "sw_math.h"
#include "sw_piece.h"

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

	virtual void GetDefaultControlPoints(std::vector<swPieceControlPoint>& ControlPoints) const = 0;
	virtual void VerifyControlPoints(std::vector<swPieceControlPoint>& ControlPoints) const = 0;
	int InsertControlPoint(std::vector<swPieceControlPoint>& ControlPoints, const swVector3& Start, const swVector3& End) const;
	std::unique_ptr<swMesh> BuildMesh(const std::vector<swPieceControlPoint>& ControlPoints, const std::atomic_bool& Cancelled) const;

protected:
	using SectionCallbackFunc = std::function<void(const swVector3& CurvePoint, quint32 SegmentIndex, float t)>;
	virtual void CalculateSections(const std::vector<swPieceControlPoint>& ControlPoints, std::vector<swMatrix44>& Sections, SectionCallbackFunc SectionCallback, const std::atomic_bool* Cancelled) const = 0;
	virtual void AddParts(swMemoryFile& File, swLibraryMeshData& MeshData, const std::vector<swMatrix44>& Sections, const std::atomic_bool* Cancelled) const = 0;

	bool mCurve = false;
	bool mUnidirectional = false;
	bool mNondirectional = false;
	float mLength;
};

void swSynthesisInitialize();
