#include "sw_global.h"
#include "sw_library_shadow.h"
#include "sw_library.h"
#include "sw_application.h"
#include "sw_file.h"

std::map<std::string, QString> swShadowLibrary::mShadowFiles;
std::map<std::string, std::vector<swShadowLine>> swShadowLibrary::mShadowLines;
std::map<std::string, std::vector<swConnectionCylinder>> swShadowLibrary::mCylinders;

static const std::vector<swConnectionCylinder> gNoCylinders;

bool swShadowLibrary::SetPath(const QString& Path)
{
	mShadowFiles.clear();
	mShadowLines.clear();
	mCylinders.clear();

	if (Path.isEmpty())
		return false;

	const QDir RootDirectory(Path);

	// parts win over primitives
	for (const char* Folder : { "parts", "p" })
	{
		const QDir Directory(RootDirectory.filePath(QLatin1String(Folder)));
		QDirIterator FileIt(Directory.absolutePath(), QStringList() << QLatin1String("*.dat"), QDir::Files, QDirIterator::Subdirectories);

		while (FileIt.hasNext())
		{
			const QString FilePath = FileIt.next();
			mShadowFiles.try_emplace(NormalizeName(Directory.relativeFilePath(FilePath).toStdString()), FilePath);
		}
	}

	return !mShadowFiles.empty();
}

bool swShadowLibrary::IsLoaded()
{
	return !mShadowFiles.empty();
}

std::string swShadowLibrary::NormalizeName(const std::string& Name)
{
	std::string Result = swPartsLibrary::NormalizePartDataName(Name.c_str());

	if (Result.size() < 4 || Result.compare(Result.size() - 4, 4, ".DAT") != 0)
		Result += ".DAT";

	return Result;
}

// use to cache snap lines
const std::vector<swShadowLine>& swShadowLibrary::GetShadowLines(const std::string& Name)
{
	const auto [LinesIt, Inserted] = mShadowLines.try_emplace(Name);
	std::vector<swShadowLine>& Lines = LinesIt->second;

	if (!Inserted)
		return Lines;

	const std::map<std::string, QString>::const_iterator FileIt = mShadowFiles.find(Name);

	if (FileIt == mShadowFiles.end())
		return Lines;

	QFile File(FileIt->second);

	if (!File.open(QIODevice::ReadOnly | QIODevice::Text))
		return Lines;

	while (!File.atEnd())
	{
		std::optional<swShadowLine> Line = swShadowParser::ParseLine(QString::fromUtf8(File.readLine()));

		if (Line)
			Lines.push_back(std::move(*Line));
	}

	return Lines;
}

std::vector<swConnectionCylinder> swShadowLibrary::GetOwnCylinders(const std::string& Name)
{
	std::vector<swConnectionCylinder> Cylinders;

	for (const swShadowLine& Line : GetShadowLines(Name))
		if (Line.Type == swShadowLineType::Cylinder)
			Cylinders.insert(Cylinders.end(), Line.Cylinders.begin(), Line.Cylinders.end());

	return Cylinders;
}

// use to list placed sub-files
std::vector<std::pair<std::string, swMatrix44>> swShadowLibrary::ReadReferences(const std::string& Name)
{
	std::vector<std::pair<std::string, swMatrix44>> References;
	swPartsLibrary* Library = swGetPartsLibrary();
	swLibraryPrimitive* Primitive = Library->FindPrimitive(Name.c_str());
	const swResult<std::unique_ptr<swFile>> File = Primitive ? Library->OpenPrimitiveFile(Primitive) : Library->OpenPartDataFile(Name.c_str());

	if (!File)
		return References;

	char Line[1024];

	while (File.value()->ReadLine(Line, sizeof(Line)))
	{
		std::optional<std::pair<std::string, swMatrix44>> Reference = swShadowParser::ParseReference(Line);

		if (Reference)
			References.push_back(std::move(*Reference));
	}

	return References;
}

void swShadowLibrary::AddPlacedCylinders(const std::vector<swConnectionCylinder>& Cylinders, const swMatrix44& Transform, std::vector<swConnectionCylinder>& Result)
{
	for (swConnectionCylinder Cylinder : Cylinders)
	{
		Cylinder.Transform = swMultiply(Cylinder.Transform, Transform);

		if (swShadowParser::NormalizeFrame(Cylinder))
			Result.push_back(std::move(Cylinder));
	}
}

void swShadowLibrary::ApplyShadowLine(const swShadowLine& Line, std::vector<swConnectionCylinder>& Cylinders)
{
	switch (Line.Type)
	{
	case swShadowLineType::Cylinder:
		Cylinders.insert(Cylinders.end(), Line.Cylinders.begin(), Line.Cylinders.end());
		break;

	case swShadowLineType::Include:
		{
			const std::vector<swConnectionCylinder> Included = GetOwnCylinders(NormalizeName(Line.Reference));

			for (const swMatrix44& Placement : Line.Placements)
				AddPlacedCylinders(Included, Placement, Cylinders);
		}
		break;

	case swShadowLineType::Clear:
		if (Line.Id.isEmpty())
			Cylinders.clear();
		else
			Cylinders.erase(std::remove_if(Cylinders.begin(), Cylinders.end(), [&Line](const swConnectionCylinder& Cylinder) { return Line.Id.compare(Cylinder.Id, Qt::CaseInsensitive) == 0; }), Cylinders.end());
		break;
	}
}

const std::vector<swConnectionCylinder>& swShadowLibrary::GetCylinders(const char* PartName)
{
	if (mShadowFiles.empty())
		return gNoCylinders;

	const std::string Name = NormalizeName(PartName);

	// insert first to stop recursion
	const auto [CylindersIt, Inserted] = mCylinders.try_emplace(Name);
	std::vector<swConnectionCylinder>& Collected = CylindersIt->second;

	if (!Inserted)
		return Collected;

	std::vector<swConnectionCylinder> Result;

	// inherit sub-file shapes
	for (const auto& [ReferenceName, ReferenceTransform] : ReadReferences(Name))
		AddPlacedCylinders(GetCylinders(ReferenceName.c_str()), ReferenceTransform, Result);

	// own lines in file order
	for (const swShadowLine& Line : GetShadowLines(Name))
		ApplyShadowLine(Line, Result);

	Collected = std::move(Result);

	return Collected;
}
