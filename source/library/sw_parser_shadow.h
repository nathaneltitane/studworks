#pragma once

#include "sw_connectivity.h"

// Connection shapes in the LDCad shadow library format, see https://www.melkert.net/LDCad/tech/meta.
// Only SNAP_CYL, SNAP_INCL and SNAP_CLEAR are read, clips, fingers and generic shapes are ignored.

enum class swShadowLineType
{
	Cylinder,
	Include,
	Clear
};

struct swShadowLine
{
	swShadowLineType Type;
	std::vector<swConnectionCylinder> Cylinders; // SNAP_CYL: one per grid position.
	std::string Reference; // SNAP_INCL: the file whose own shapes are placed at each placement.
	std::vector<swMatrix44> Placements;
	QString Id; // SNAP_CLEAR: the id of the shapes to remove, or empty to remove all.
};

class swShadowParser
{
public:
	// A supported and well formed snap line of a shadow file, or nothing.
	static std::optional<swShadowLine> ParseLine(const QString& Line);

	// The file name and transform of an LDraw type 1 line, or nothing for other lines.
	static std::optional<std::pair<std::string, swMatrix44>> ParseReference(const char* Line);
	static swMatrix44 GetLDrawMatrix(const float* Orientation, const swVector3& Position);
	// Moves the scale of a placed shape into its sections and corrects mirroring, false if the shape is dropped.
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
