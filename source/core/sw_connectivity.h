#pragma once

#include "sw_math.h"

enum class swConnectionGender
{
	Male,
	Female
};

// Section shapes of the shadow library: round, axle, square and the two radius extensions.
enum class swConnectionShape
{
	Round,
	Axle,
	Square,
	ExtensionStart,
	ExtensionEnd
};

struct swConnectionSection
{
	swConnectionShape Shape;
	float Radius;
	float Length;
};

// The scaling of a sub-file a shape survives, it is dropped from other scaled sub-files.
enum class swConnectionScale
{
	None,
	Length,
	Radius,
	LengthOrRadius
};

struct swConnectionCylinder
{
	swMatrix44 Transform; // In LDraw part coordinates, the shape runs along its -Y axis.
	swConnectionGender Gender;
	std::vector<swConnectionSection> Sections;
	QString Id;
	QString Group;
	bool Center;
	bool ClosedStart; // The end at the position, or the near end of a centered shape.
	bool ClosedEnd;
	swConnectionScale Scale;
	bool InheritMirrored; // Mirrored shapes flip a radius axis, or are dropped with mirror=none.

	float GetLength() const
	{
		float Length = 0.0f;

		for (const swConnectionSection& Section : Sections)
			Length += Section.Length;

		return Length;
	}

	float GetRadius() const
	{
		float Radius = 0.0f;

		for (const swConnectionSection& Section : Sections)
			Radius = std::max(Radius, Section.Radius);

		return Radius;
	}

	// Start and end along the shape's axis, from its position.
	std::pair<float, float> GetExtent() const
	{
		const float Length = GetLength();

		return Center ? std::make_pair(-Length / 2.0f, Length / 2.0f) : std::make_pair(0.0f, Length);
	}
};

// The shapes of each library part of a model part, at that library part's transform.
typedef std::vector<std::pair<const std::vector<swConnectionCylinder>*, swMatrix44>> swConnectionShapes;

// How connection shapes fit together, wherever the shapes come from.
class swConnectivity
{
public:
	static bool AreCompatible(const swConnectionCylinder& a, const swConnectionCylinder& b);
	// Whether any of the shapes at Transform is seated in or on any of the other shapes at OtherTransform.
	static bool AreConnected(const std::vector<swConnectionCylinder>& Cylinders, const swMatrix44& Transform, const std::vector<swConnectionCylinder>& OtherCylinders, const swMatrix44& OtherTransform);
	static bool AreConnected(const swConnectionShapes& Shapes, const swConnectionShapes& OtherShapes);
	// Grows Box to hold the shapes at Transform, connected shapes have overlapping boxes.
	static void AddToBoundingBox(const std::vector<swConnectionCylinder>& Cylinders, const swMatrix44& Transform, swBoundingBox& Box);

	// The ranges of offsets from the female's position at which a male shape running the same way fits.
	static std::vector<std::pair<float, float>> GetInsertRanges(const swConnectionCylinder& Male, const swConnectionCylinder& Female);

private:
	static bool AreSectionsCompatible(const swConnectionSection& Male, const swConnectionSection& Female);
	static bool FitsAt(const swConnectionCylinder& Male, const swConnectionCylinder& Female, float Offset);
	static bool IsSeated(const swConnectionCylinder& Cylinder, const swMatrix44& Frame, const swConnectionCylinder& Other, const swMatrix44& OtherFrame);
};
