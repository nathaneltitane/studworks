#include "sw_global.h"
#include "sw_parser_shadow.h"

// use to build an ldraw transform
swMatrix44 swShadowParser::GetLDrawMatrix(const float* Orientation, const swVector3& Position)
{
	return swMatrix44(swVector4(Orientation[0], Orientation[3], Orientation[6], 0.0f), swVector4(Orientation[1], Orientation[4], Orientation[7], 0.0f), swVector4(Orientation[2], Orientation[5], Orientation[8], 0.0f), swVector4(Position, 1.0f));
}

std::optional<std::pair<std::string, swMatrix44>> swShadowParser::ParseReference(const char* Line)
{
	int LineType, ColorCode, NameStart = 0;
	float Numbers[12];

	if (sscanf(Line, "%d %i %f %f %f %f %f %f %f %f %f %f %f %f %n", &LineType, &ColorCode, &Numbers[0], &Numbers[1], &Numbers[2], &Numbers[3], &Numbers[4], &Numbers[5], &Numbers[6], &Numbers[7], &Numbers[8], &Numbers[9], &Numbers[10], &Numbers[11], &NameStart) != 14 || LineType != 1 || !NameStart)
		return std::nullopt;

	// file names may hold spaces
	const std::string FileName = QString::fromUtf8(Line + NameStart).trimmed().toStdString();

	if (FileName.empty())
		return std::nullopt;

	return std::make_pair(FileName, GetLDrawMatrix(Numbers + 3, swVector3(Numbers[0], Numbers[1], Numbers[2])));
}

// use to read snap line parameters
std::map<QString, QString> swShadowParser::ParseParameters(const QString& Line)
{
	std::map<QString, QString> Parameters;
	qsizetype Start = 0;

	while ((Start = Line.indexOf('[', Start)) != -1)
	{
		const qsizetype Equal = Line.indexOf('=', Start);
		const qsizetype End = Line.indexOf(']', Start);

		if (Equal == -1 || End == -1 || Equal > End)
			break;

		Parameters[Line.mid(Start + 1, Equal - Start - 1).trimmed().toLower()] = Line.mid(Equal + 1, End - Equal - 1).trimmed();
		Start = End + 1;
	}

	return Parameters;
}

QStringList swShadowParser::SplitTokens(const QString& Text)
{
	return Text.simplified().split(' ', Qt::SkipEmptyParts);
}

QString swShadowParser::GetParameter(const std::map<QString, QString>& Parameters, const char* Key)
{
	const std::map<QString, QString>::const_iterator ParameterIt = Parameters.find(QLatin1String(Key));

	return ParameterIt != Parameters.end() ? ParameterIt->second : QString();
}

bool swShadowParser::IsParameterTrue(const std::map<QString, QString>& Parameters, const char* Key)
{
	return GetParameter(Parameters, Key).compare(QLatin1String("true"), Qt::CaseInsensitive) == 0;
}

// use to read finite numbers
std::optional<std::vector<float>> swShadowParser::ParseNumbers(const QString& Text, qsizetype Count)
{
	const QStringList Tokens = SplitTokens(Text);

	if (Tokens.size() != Count)
		return std::nullopt;

	std::vector<float> Numbers;

	for (const QString& Token : Tokens)
	{
		bool Ok = false;
		const float Number = Token.toFloat(&Ok);

		if (!Ok || !std::isfinite(Number))
			return std::nullopt;

		Numbers.push_back(Number);
	}

	return Numbers;
}

std::optional<swConnectionShape> swShadowParser::ParseShape(const QString& Text)
{
	const QString Shape = Text.toUpper();

	if (Shape == QLatin1String("R"))
		return swConnectionShape::Round;
	else if (Shape == QLatin1String("A"))
		return swConnectionShape::Axle;
	else if (Shape == QLatin1String("S"))
		return swConnectionShape::Square;
	else if (Shape == QLatin1String("L_"))
		return swConnectionShape::ExtensionStart;
	else if (Shape == QLatin1String("_L"))
		return swConnectionShape::ExtensionEnd;

	return std::nullopt;
}

// use to read a snap transform
std::optional<swMatrix44> swShadowParser::ParseTransform(const std::map<QString, QString>& Parameters)
{
	float Orientation[9] = { 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f };
	swVector3 Position(0.0f, 0.0f, 0.0f);

	const QString OrientationText = GetParameter(Parameters, "ori");

	if (!OrientationText.isEmpty())
	{
		const std::optional<std::vector<float>> Numbers = ParseNumbers(OrientationText, 9);

		if (!Numbers)
			return std::nullopt;

		std::copy(Numbers->begin(), Numbers->end(), Orientation);
	}

	const QString PositionText = GetParameter(Parameters, "pos");

	if (!PositionText.isEmpty())
	{
		const std::optional<std::vector<float>> Numbers = ParseNumbers(PositionText, 3);

		if (!Numbers)
			return std::nullopt;

		Position = swVector3((*Numbers)[0], (*Numbers)[1], (*Numbers)[2]);
	}

	return GetLDrawMatrix(Orientation, Position);
}

// use to expand grid offsets
std::optional<std::vector<swVector3>> swShadowParser::ParseGridOffsets(const std::map<QString, QString>& Parameters)
{
	const QStringList Tokens = SplitTokens(GetParameter(Parameters, "grid"));

	if (Tokens.empty())
		return std::vector<swVector3>{ swVector3(0.0f, 0.0f, 0.0f) };

	qsizetype Index = 0;
	int Counts[2] = { 1, 1 };
	bool Centered[2] = { false, false };

	for (int Axis = 0; Axis < 2; Axis++)
	{
		if (Index < Tokens.size() && Tokens[Index].compare(QLatin1String("C"), Qt::CaseInsensitive) == 0)
		{
			Centered[Axis] = true;
			Index++;
		}

		bool Ok = false;
		Counts[Axis] = Index < Tokens.size() ? Tokens[Index++].toInt(&Ok) : 0;

		// larger grids are typos
		if (!Ok || Counts[Axis] < 1 || Counts[Axis] > 1000)
			return std::nullopt;
	}

	const std::optional<std::vector<float>> Steps = ParseNumbers(Tokens.mid(Index).join(' '), 2);

	if (!Steps)
		return std::nullopt;

	std::vector<swVector3> Offsets;

	for (int x = 0; x < Counts[0]; x++)
	{
		const float OffsetX = (Centered[0] ? x - (Counts[0] - 1) / 2.0f : x) * (*Steps)[0];

		for (int z = 0; z < Counts[1]; z++)
		{
			const float OffsetZ = (Centered[1] ? z - (Counts[1] - 1) / 2.0f : z) * (*Steps)[1];
			Offsets.emplace_back(OffsetX, 0.0f, OffsetZ);
		}
	}

	return Offsets;
}

bool swShadowParser::NormalizeFrame(swConnectionCylinder& Cylinder)
{
	float Scales[3];

	for (int Row = 0; Row < 3; Row++)
	{
		Scales[Row] = swLength(swVector3(Cylinder.Transform.r[Row]));

		// flattened shapes have no axis left
		if (Scales[Row] < 0.001f)
			return false;

		Cylinder.Transform.r[Row] = swVector4(swVector3(Cylinder.Transform.r[Row]) / Scales[Row], 0.0f);
	}

	const auto IsOne = [](float Scale) { return fabsf(Scale - 1.0f) < 0.001f; };
	const bool ScalesLength = !IsOne(Scales[1]);
	const bool ScalesRadius = !IsOne(Scales[0]) || !IsOne(Scales[2]);

	if (ScalesLength || ScalesRadius)
	{
		const bool LengthOnly = ScalesLength && !ScalesRadius;
		const bool RadiusOnly = ScalesRadius && !ScalesLength && fabsf(Scales[0] - Scales[2]) < 0.001f;
		const bool LengthAllowed = Cylinder.Scale == swConnectionScale::Length || Cylinder.Scale == swConnectionScale::LengthOrRadius;
		const bool RadiusAllowed = Cylinder.Scale == swConnectionScale::Radius || Cylinder.Scale == swConnectionScale::LengthOrRadius;

		if (!(LengthOnly && LengthAllowed) && !(RadiusOnly && RadiusAllowed))
			return false;

		for (swConnectionSection& Section : Cylinder.Sections)
		{
			if (LengthOnly)
				Section.Length *= Scales[1];
			else
				Section.Radius *= Scales[0];
		}
	}

	if (Cylinder.Transform.Determinant() > 0.0f)
		return true;

	if (!Cylinder.InheritMirrored)
		return false;

	// flip a radius axis
	const swVector4& RadiusAxis = Cylinder.Transform.r[0];
	Cylinder.Transform.r[0] = swVector4(-RadiusAxis[0], -RadiusAxis[1], -RadiusAxis[2], RadiusAxis[3]);

	return true;
}

swMatrix44 swShadowParser::GetOffsetTransform(const swMatrix44& Transform, const swVector3& Offset)
{
	swMatrix44 Result = Transform;
	Result.SetTranslation(Transform.GetTranslation() + swMultiply30(Offset, Transform));

	return Result;
}

std::optional<swShadowLine> swShadowParser::ParseLine(const QString& Line)
{
	const QStringList Tokens = SplitTokens(Line);

	if (Tokens.size() < 3 || Tokens[0] != QLatin1String("0") || Tokens[1] != QLatin1String("!LDCAD"))
		return std::nullopt;

	const QString& Keyword = Tokens[2];
	const std::map<QString, QString> Parameters = ParseParameters(Line);
	swShadowLine Result;

	if (Keyword == QLatin1String("SNAP_CYL"))
	{
		Result.Type = swShadowLineType::Cylinder;
		Result.Cylinders = ParseCylinder(Parameters);

		if (Result.Cylinders.empty())
			return std::nullopt;
	}
	else if (Keyword == QLatin1String("SNAP_INCL"))
	{
		Result.Type = swShadowLineType::Include;
		Result.Reference = GetParameter(Parameters, "ref").toStdString();

		const std::optional<swMatrix44> Transform = ParseTransform(Parameters);
		const std::optional<std::vector<swVector3>> Offsets = ParseGridOffsets(Parameters);

		if (Result.Reference.empty() || !Transform || !Offsets)
			return std::nullopt;

		for (const swVector3& Offset : *Offsets)
			Result.Placements.push_back(GetOffsetTransform(*Transform, Offset));
	}
	else if (Keyword == QLatin1String("SNAP_CLEAR"))
	{
		Result.Type = swShadowLineType::Clear;
		Result.Id = GetParameter(Parameters, "id");
	}
	else
		return std::nullopt;

	return Result;
}

// use to read cylinder shapes
std::vector<swConnectionCylinder> swShadowParser::ParseCylinder(const std::map<QString, QString>& Parameters)
{
	swConnectionCylinder Cylinder;

	Cylinder.Gender = GetParameter(Parameters, "gender").startsWith('F', Qt::CaseInsensitive) ? swConnectionGender::Female : swConnectionGender::Male;
	Cylinder.Id = GetParameter(Parameters, "id");
	Cylinder.Group = GetParameter(Parameters, "group");
	Cylinder.Center = IsParameterTrue(Parameters, "center");

	// one cap closes the inner end
	const QString Caps = GetParameter(Parameters, "caps").toLower();
	const bool OneCap = Caps.isEmpty() || Caps == QLatin1String("one");
	Cylinder.ClosedStart = Caps == QLatin1String("two") || Caps == QLatin1String("a") || (OneCap && Cylinder.Gender == swConnectionGender::Male);
	Cylinder.ClosedEnd = Caps == QLatin1String("two") || Caps == QLatin1String("b") || (OneCap && Cylinder.Gender == swConnectionGender::Female);

	const QString Scale = GetParameter(Parameters, "scale").toLower();

	if (Scale == QLatin1String("yonly"))
		Cylinder.Scale = swConnectionScale::Length;
	else if (Scale == QLatin1String("ronly"))
		Cylinder.Scale = swConnectionScale::Radius;
	else if (Scale == QLatin1String("yandr"))
		Cylinder.Scale = swConnectionScale::LengthOrRadius;
	else
		Cylinder.Scale = swConnectionScale::None;

	Cylinder.InheritMirrored = GetParameter(Parameters, "mirror").compare(QLatin1String("none"), Qt::CaseInsensitive) != 0;

	const QStringList Sections = SplitTokens(GetParameter(Parameters, "secs"));

	if (Sections.empty() || Sections.size() % 3 != 0)
		return {};

	for (qsizetype SectionIndex = 0; SectionIndex < Sections.size(); SectionIndex += 3)
	{
		const std::optional<swConnectionShape> Shape = ParseShape(Sections[SectionIndex]);
		const std::optional<std::vector<float>> Numbers = ParseNumbers(Sections[SectionIndex + 1] + ' ' + Sections[SectionIndex + 2], 2);

		if (!Shape || !Numbers)
			return {};

		Cylinder.Sections.push_back({ *Shape, (*Numbers)[0], (*Numbers)[1] });
	}

	const std::optional<swMatrix44> Transform = ParseTransform(Parameters);
	const std::optional<std::vector<swVector3>> Offsets = ParseGridOffsets(Parameters);

	if (!Transform || !Offsets)
		return {};

	std::vector<swConnectionCylinder> Cylinders;

	// scale a copy per position
	for (const swVector3& Offset : *Offsets)
	{
		swConnectionCylinder PlacedCylinder = Cylinder;
		PlacedCylinder.Transform = GetOffsetTransform(*Transform, Offset);

		if (NormalizeFrame(PlacedCylinder))
			Cylinders.push_back(std::move(PlacedCylinder));
	}

	return Cylinders;
}
