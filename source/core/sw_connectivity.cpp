#include "sw_global.h"
#include "sw_connectivity.h"

// equal length tolerance
static const float gConnectionTolerance = 0.01f;

// seated shape tolerance
static const float gSeatTolerance = 0.5f;

// use to detect radius extensions
static bool swIsExtension(const swConnectionSection& Section)
{
	return Section.Shape == swConnectionShape::ExtensionStart || Section.Shape == swConnectionShape::ExtensionEnd;
}

// use to test a fit at an offset
bool swConnectivity::FitsAt(const swConnectionCylinder& Male, const swConnectionCylinder& Female, float Offset)
{
	bool Grips = false;
	float MaleStart = Male.GetExtent().first + Offset;

	for (const swConnectionSection& MaleSection : Male.Sections)
	{
		const float MaleEnd = MaleStart + MaleSection.Length;
		float FemaleStart = Female.GetExtent().first;

		for (const swConnectionSection& FemaleSection : Female.Sections)
		{
			const float FemaleEnd = FemaleStart + FemaleSection.Length;
			const float Overlap = std::min(MaleEnd, FemaleEnd) - std::max(MaleStart, FemaleStart);

			if (Overlap > gConnectionTolerance)
			{
				// male never wider than female
				if (MaleSection.Radius > FemaleSection.Radius + gConnectionTolerance && !swIsExtension(MaleSection) && !swIsExtension(FemaleSection))
					return false;

				// shorter section inside the longer grips
				const bool MaleInside = MaleStart > FemaleStart - gConnectionTolerance && MaleEnd < FemaleEnd + gConnectionTolerance;
				const bool FemaleInside = FemaleStart > MaleStart - gConnectionTolerance && FemaleEnd < MaleEnd + gConnectionTolerance;

				if (AreSectionsCompatible(MaleSection, FemaleSection) && (MaleInside || FemaleInside))
					Grips = true;
			}

			FemaleStart = FemaleEnd;
		}

		MaleStart = MaleEnd;
	}

	return Grips;
}

// use to find valid insert offsets
std::vector<std::pair<float, float>> swConnectivity::GetInsertRanges(const swConnectionCylinder& Male, const swConnectionCylinder& Female)
{
	const auto [MaleStart, MaleEnd] = Male.GetExtent();
	const auto [FemaleStart, FemaleEnd] = Female.GetExtent();
	float Low = FemaleStart - MaleEnd;
	float High = FemaleEnd - MaleStart;

	if (Female.ClosedStart)
		Low = std::max(Low, FemaleStart - MaleStart);

	if (Female.ClosedEnd)
		High = std::min(High, FemaleEnd - MaleEnd);

	if (Male.ClosedStart)
		High = std::min(High, FemaleStart - MaleStart);

	if (Male.ClosedEnd)
		Low = std::max(Low, FemaleEnd - MaleEnd);

	if (Low > High + gConnectionTolerance)
		return {};

	// fit changes at section boundaries
	std::vector<float> MaleBoundaries = { MaleStart };
	std::vector<float> FemaleBoundaries = { FemaleStart };

	for (const swConnectionSection& Section : Male.Sections)
		MaleBoundaries.push_back(MaleBoundaries.back() + Section.Length);

	for (const swConnectionSection& Section : Female.Sections)
		FemaleBoundaries.push_back(FemaleBoundaries.back() + Section.Length);

	std::vector<float> Offsets = { Low, High };

	for (float FemaleBoundary : FemaleBoundaries)
		for (float MaleBoundary : MaleBoundaries)
			Offsets.push_back(FemaleBoundary - MaleBoundary);

	Offsets.erase(std::remove_if(Offsets.begin(), Offsets.end(), [Low, High](float Offset) { return Offset < Low - gConnectionTolerance || Offset > High + gConnectionTolerance; }), Offsets.end());
	std::sort(Offsets.begin(), Offsets.end());
	Offsets.erase(std::unique(Offsets.begin(), Offsets.end(), [](float a, float b) { return b - a < gConnectionTolerance; }), Offsets.end());

	// test boundaries and midpoints
	for (size_t Index = Offsets.size() - 1; Index > 0; Index--)
		Offsets.insert(Offsets.begin() + Index, (Offsets[Index - 1] + Offsets[Index]) / 2.0f);

	std::vector<std::pair<float, float>> Ranges;
	bool Fitted = false;

	for (float Offset : Offsets)
	{
		const bool Fits = FitsAt(Male, Female, Offset);

		if (Fits && Fitted)
			Ranges.back().second = Offset;
		else if (Fits)
			Ranges.emplace_back(Offset, Offset);

		Fitted = Fits;
	}

	return Ranges;
}

bool swConnectivity::AreCompatible(const swConnectionCylinder& a, const swConnectionCylinder& b)
{
	if (a.Gender == b.Gender || a.Group.compare(b.Group, Qt::CaseInsensitive) != 0)
		return false;

	const swConnectionCylinder& Male = a.Gender == swConnectionGender::Male ? a : b;
	const swConnectionCylinder& Female = a.Gender == swConnectionGender::Male ? b : a;

	for (const swConnectionSection& MaleSection : Male.Sections)
		for (const swConnectionSection& FemaleSection : Female.Sections)
			if (AreSectionsCompatible(MaleSection, FemaleSection))
				return true;

	return false;
}

// use to match section shapes
bool swConnectivity::AreSectionsCompatible(const swConnectionSection& Male, const swConnectionSection& Female)
{
	if (fabsf(Male.Radius - Female.Radius) >= gConnectionTolerance)
		return false;

	return Male.Shape == Female.Shape || (Male.Shape == swConnectionShape::Round && Female.Shape == swConnectionShape::Square);
}

// use to test axis and depth
bool swConnectivity::IsSeated(const swConnectionCylinder& Cylinder, const swMatrix44& Frame, const swConnectionCylinder& Other, const swMatrix44& OtherFrame)
{
	const swVector3 ShapeAxis(0.0f, 0.0f, 1.0f);
	const swVector3 Axis = swNormalize(swMultiply30(ShapeAxis, Frame));

	if (swDot(swNormalize(swMultiply30(ShapeAxis, OtherFrame)), Axis) < 0.99f)
		return false;

	const swVector3 Offset = Frame.GetTranslation() - OtherFrame.GetTranslation();
	const float Along = swDot(Offset, Axis);

	if (swLength(Offset - Axis * Along) > gSeatTolerance)
		return false;

	const bool IsMale = Cylinder.Gender == swConnectionGender::Male;
	const std::vector<std::pair<float, float>> InsertRanges = IsMale ? GetInsertRanges(Cylinder, Other) : GetInsertRanges(Other, Cylinder);
	const float MaleOffset = IsMale ? Along : -Along;

	return std::any_of(InsertRanges.begin(), InsertRanges.end(), [MaleOffset](const std::pair<float, float>& Range)
	{
		return MaleOffset > Range.first - gSeatTolerance && MaleOffset < Range.second + gSeatTolerance;
	});
}

void swConnectivity::AddToBoundingBox(const std::vector<swConnectionCylinder>& Cylinders, const swMatrix44& Transform, swBoundingBox& Box)
{
	for (const swConnectionCylinder& Cylinder : Cylinders)
	{
		const swMatrix44 Frame = swMultiply(swMatrix44LDrawToStudWorks(Cylinder.Transform), Transform);
		const swVector3 Axis = swNormalize(swMultiply30(swVector3(0.0f, 0.0f, 1.0f), Frame));
		const swVector3 Radius(Cylinder.GetRadius(), Cylinder.GetRadius(), Cylinder.GetRadius());
		const auto [Start, End] = Cylinder.GetExtent();

		for (float Along : { Start, End })
		{
			const swVector3 Point = Frame.GetTranslation() + Axis * Along;

			Box.Min = swMinimum(Box.Min, Point - Radius);
			Box.Max = swMaximum(Box.Max, Point + Radius);
		}
	}
}

bool swConnectivity::AreConnected(const std::vector<swConnectionCylinder>& Cylinders, const swMatrix44& Transform, const std::vector<swConnectionCylinder>& OtherCylinders, const swMatrix44& OtherTransform)
{
	for (const swConnectionCylinder& Cylinder : Cylinders)
	{
		const swMatrix44 Frame = swMultiply(swMatrix44LDrawToStudWorks(Cylinder.Transform), Transform);

		for (const swConnectionCylinder& OtherCylinder : OtherCylinders)
			if (AreCompatible(Cylinder, OtherCylinder) && IsSeated(Cylinder, Frame, OtherCylinder, swMultiply(swMatrix44LDrawToStudWorks(OtherCylinder.Transform), OtherTransform)))
				return true;
	}

	return false;
}

bool swConnectivity::AreConnected(const swConnectionShapes& Shapes, const swConnectionShapes& OtherShapes)
{
	for (const auto& [Cylinders, Transform] : Shapes)
		for (const auto& [OtherCylinders, OtherTransform] : OtherShapes)
			if (AreConnected(*Cylinders, Transform, *OtherCylinders, OtherTransform))
				return true;

	return false;
}
