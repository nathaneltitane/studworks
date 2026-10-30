#pragma once

#include "sw_math.h"

enum class swConnectionGender
{
	Male,
	Female
};

// Shadow library section shapes.
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

// The sub-file scaling a shape survives.
enum class swConnectionScale
{
	None,
	Length,
	Radius,
	LengthOrRadius
};

struct swConnectionCylinder
{
	// The shape runs along -Y in part coordinates.
	swMatrix44 Transform;
	swConnectionGender Gender;
	std::vector<swConnectionSection> Sections;
	QString Id;
	QString Group;
	bool Center;

	// The end at the position.
	bool ClosedStart;
	bool ClosedEnd;
	swConnectionScale Scale;

	// Mirroring flips a radius axis.
	bool InheritMirrored;

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

	// Start and end along the shape axis.
	std::pair<float, float> GetExtent() const
	{
		const float Length = GetLength();

		return Center ? std::make_pair(-Length / 2.0f, Length / 2.0f) : std::make_pair(0.0f, Length);
	}
};

// The shapes of each library part.
typedef std::vector<std::pair<const std::vector<swConnectionCylinder>*, swMatrix44>> swConnectionShapes;

// How connection shapes fit together.
class swConnectivity
{
public:
	static bool AreCompatible(const swConnectionCylinder& a, const swConnectionCylinder& b);

	// Tells if any shapes are seated together.
	static bool AreConnected(const std::vector<swConnectionCylinder>& Cylinders, const swMatrix44& Transform, const std::vector<swConnectionCylinder>& OtherCylinders, const swMatrix44& OtherTransform);
	static bool AreConnected(const swConnectionShapes& Shapes, const swConnectionShapes& OtherShapes);

	// Grows Box to hold the shapes.
	static void AddToBoundingBox(const std::vector<swConnectionCylinder>& Cylinders, const swMatrix44& Transform, swBoundingBox& Box);

	// The offsets where a male shape fits.
	static std::vector<std::pair<float, float>> GetInsertRanges(const swConnectionCylinder& Male, const swConnectionCylinder& Female);

private:
	static bool AreSectionsCompatible(const swConnectionSection& Male, const swConnectionSection& Female);
	static bool FitsAt(const swConnectionCylinder& Male, const swConnectionCylinder& Female, float Offset);
	static bool IsSeated(const swConnectionCylinder& Cylinder, const swMatrix44& Frame, const swConnectionCylinder& Other, const swMatrix44& OtherFrame);
};
