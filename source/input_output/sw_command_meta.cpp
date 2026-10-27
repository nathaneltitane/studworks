#include "sw_global.h"
#include "sw_command_meta.h"

struct swCommandMeta
{
	QString Indent;
	QString Object;
	QString Property;
	QString Arguments;
	bool LeoCAD = false;
};

static const QLatin1String gCommandMetaMarker("!SW");
static const QLatin1String gCommandMetaMarkerLeoCAD("!LEOCAD");

static const QLatin1String gCommandMetaObjects[] =
{
	QLatin1String("PART"), QLatin1String("CAMERA"), QLatin1String("LIGHT"), QLatin1String("MODEL"),
	QLatin1String("POV_RAY"), QLatin1String("GROUP"), QLatin1String("SYNTH_PATH"), QLatin1String("CLIPBOARD")
};

// StudWorks names and their LeoCAD names.
static const QLatin1String gCommandMetaNames[][2] =
{
	{ QLatin1String("PART"), QLatin1String("PIECE") },
	{ QLatin1String("SYNTH_PATH"), QLatin1String("SYNTH") },
	{ QLatin1String("ORIENTATION"), QLatin1String("ROTATION") },
	{ QLatin1String("ORIENTATION_KEY"), QLatin1String("ROTATION_KEY") },
	{ QLatin1String("COLOR_RGB"), QLatin1String("COLOR") },
	{ QLatin1String("COLOR_RGB_KEY"), QLatin1String("COLOR_KEY") },
	{ QLatin1String("LABEL"), QLatin1String("NAME") },
	{ QLatin1String("HEADER_FILE_INCLUDE"), QLatin1String("HEADER_INCLUDE_FILE") },
	{ QLatin1String("FOOTER_FILE_INCLUDE"), QLatin1String("FOOTER_INCLUDE_FILE") }
};

static QString swRenameCommandMetaName(const QString& Name, swCommandMetaFormat Format)
{
	const int From = Format == swCommandMetaFormat::LeoCAD ? 0 : 1;

	for (const auto& Names : gCommandMetaNames)
		if (Name == Names[From])
			return Names[1 - From];

	return Name;
}

static int swSkipSpace(const QString& Line, int Position)
{
	while (Position < Line.size() && Line.at(Position).isSpace())
		Position++;

	return Position;
}

static int swSkipToken(const QString& Line, int Position)
{
	while (Position < Line.size() && !Line.at(Position).isSpace())
		Position++;

	return Position;
}

static bool swParseCommandMeta(const QString& Line, swCommandMeta& Command)
{
	int Begin[4];
	int End[4];
	int Position = 0;

	for (int TokenIndex = 0; TokenIndex < 4; TokenIndex++)
	{
		Begin[TokenIndex] = swSkipSpace(Line, Position);
		End[TokenIndex] = Position = swSkipToken(Line, Begin[TokenIndex]);
	}

	const auto Token = [&](int TokenIndex)
	{
		return Line.mid(Begin[TokenIndex], End[TokenIndex] - Begin[TokenIndex]);
	};

	const QString Marker = Token(1);
	const QString First = Token(2);
	const QString Second = Token(3);

	if (Token(0) != QLatin1String("0") || Second.isEmpty())
		return false;

	Command.Indent = Line.left(Begin[0]);
	Command.Arguments = Line.mid(End[3]);

	if (Marker == gCommandMetaMarkerLeoCAD)
	{
		Command.Object = swRenameCommandMetaName(First, swCommandMetaFormat::StudWorks);
		Command.Property = swRenameCommandMetaName(Second, swCommandMetaFormat::StudWorks);
		Command.LeoCAD = true;

		return true;
	}

	if (Marker != gCommandMetaMarker || std::find(std::begin(gCommandMetaObjects), std::end(gCommandMetaObjects), Second) == std::end(gCommandMetaObjects))
		return false;

	Command.Property = First;
	Command.Object = Second;
	Command.LeoCAD = false;

	return true;
}

static QString swFormatCommandMeta(const swCommandMeta& Command, swCommandMetaFormat Format)
{
	if (Format == swCommandMetaFormat::LeoCAD)
		return Command.Indent + QLatin1String("0 ") + gCommandMetaMarkerLeoCAD + QLatin1Char(' ') + swRenameCommandMetaName(Command.Object, Format) + QLatin1Char(' ') + swRenameCommandMetaName(Command.Property, Format) + Command.Arguments;

	return Command.Indent + QLatin1String("0 ") + gCommandMetaMarker + QLatin1Char(' ') + Command.Property + QLatin1Char(' ') + Command.Object + Command.Arguments;
}

QString swReadCommandMeta(const QString& Line)
{
	// The model reader takes the object first.
	swCommandMeta Command;

	if (!swParseCommandMeta(Line, Command))
		return Line;

	return Command.Indent + QLatin1String("0 ") + gCommandMetaMarker + QLatin1Char(' ') + Command.Object + QLatin1Char(' ') + Command.Property + Command.Arguments;
}

QByteArray swWriteCommandMeta(const QByteArray& Bytes, swCommandMetaFormat Format)
{
	QByteArray Output;
	Output.reserve(Bytes.size());
	int LineBegin = 0;

	while (LineBegin < Bytes.size())
	{
		int LineEnd = Bytes.indexOf('\n', LineBegin);
		LineEnd = LineEnd == -1 ? Bytes.size() : LineEnd + 1;
		const QByteArray Line = Bytes.mid(LineBegin, LineEnd - LineBegin);
		LineBegin = LineEnd;

		int TextEnd = Line.size();

		while (TextEnd > 0 && (Line.at(TextEnd - 1) == '\n' || Line.at(TextEnd - 1) == '\r'))
			TextEnd--;

		swCommandMeta Command;

		if (!Line.contains('!') || !swParseCommandMeta(QString::fromUtf8(Line.left(TextEnd)), Command) || Command.LeoCAD == (Format == swCommandMetaFormat::LeoCAD))
		{
			Output += Line;
			continue;
		}

		Output += swFormatCommandMeta(Command, Format).toUtf8() + Line.mid(TextEnd);
	}

	return Output;
}

bool swHasLeoCADCommandMeta(const QByteArray& Bytes)
{
	for (const QByteArray& Line : Bytes.split('\n'))
	{
		swCommandMeta Command;

		if (Line.contains(gCommandMetaMarkerLeoCAD.data()) && swParseCommandMeta(QString::fromUtf8(Line), Command) && Command.LeoCAD)
			return true;
	}

	return false;
}
