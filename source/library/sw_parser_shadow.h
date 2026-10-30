#pragma once

#include "sw_connectivity.h"

// Ldcad shadow library connection shapes.

enum class swShadowLineType
{
	Cylinder,
	Include,
	Clear
};

struct swShadowLine
{
	swShadowLineType Type;

	// One per grid position.
	std::vector<swConnectionCylinder> Cylinders;

	// File placed at each placement.
	std::string Reference;
	std::vector<swMatrix44> Placements;

	// Shapes to remove, empty for all.
	QString Id;
};

class swShadowParser
{
public:
	// A well formed snap line.
	static std::optional<swShadowLine> ParseLine(const QString& Line);

	// A type 1 line name and transform.
	static std::optional<std::pair<std::string, swMatrix44>> ParseReference(const char* Line);
	static swMatrix44 GetLDrawMatrix(const float* Orientation, const swVector3& Position);

	// Moves the scale into the sections.
	static bool NormalizeFrame(swConnectionCylinder& Cylinder);

private:
	static std::vector<swConnectionCylinder> ParseCylinder(const std::map<QString, QString>& Parameters);
	static std::map<QString, QString> ParseParameters(const QString& Line);
	static QString GetParameter(const std::map<QString, QString>& Parameters, const char* Key);
	static bool IsParameterTrue(const std::map<QString, QString>& Parameters, const char* Key);
	static std::optional<swMatrix44> ParseTransform(const std::map<QString, QString>& Parameters);
	static std::optional<std::vector<swVector3>> ParseGridOffsets(const std::map<QString, QString>& Parameters);
	static std::optional<swConnectionShape> ParseShape(const QString& Text);
	static std::optional<std::vector<float>> ParseNumbers(const QString& Text, qsizetype Count);
	static swMatrix44 GetOffsetTransform(const swMatrix44& Transform, const swVector3& Offset);
	static QStringList SplitTokens(const QString& Text);
};
