#include "sw_global.h"
#include "sw_builder_minifigure.h"
#include "sw_colors.h"
#include "sw_data_part.h"
#include "sw_model.h"
#include "sw_library.h"
#include "sw_application.h"
#include "sw_file.h"
#include "sw_profile.h"
#include "sw_string.h"

const char* swMinifigureBuilder::mSectionNames[SW_MINIFIGURE_ITEM_COUNT] =
{
	"HATS",   // SW_MINIFIGURE_HATS
	"HATS2",  // SW_MINIFIGURE_HATS2
	"HEAD",   // SW_MINIFIGURE_HEAD
	"NECK",   // SW_MINIFIGURE_NECK
	"BODY",   // SW_MINIFIGURE_BODY
	"BODY2",  // SW_MINIFIGURE_BODY2
	"BODY3",  // SW_MINIFIGURE_BODY3
	"RARM",   // SW_MINIFIGURE_RIGHT_ARM
	"LARM",   // SW_MINIFIGURE_LEFT_ARM
	"RHAND",  // SW_MINIFIGURE_RIGHT_HAND
	"LHAND",  // SW_MINIFIGURE_LEFT_HAND
	"RHANDA", // SW_MINIFIGURE_RIGHT_HAND_ACCESSORY
	"LHANDA", // SW_MINIFIGURE_LEFT_HAND_ACCESSORY
	"RLEG",   // SW_MINIFIGURE_RIGHT_LEG
	"LLEG",   // SW_MINIFIGURE_LEFT_LEG
	"RLEGA",  // SW_MINIFIGURE_RIGHT_LEG_ACCESSORY
	"LLEGA",  // SW_MINIFIGURE_LEFT_LEG_ACCESSORY
};

// default minifigure colors and parts
static const int gMinifigureDefaultColorCodes[SW_MINIFIGURE_ITEM_COUNT] = { 71, 7, 14, 7, 71, 0, 7, 71, 71, 14, 14, 7, 72, 0, 0, 7, 7 };
static const char* const gMinifigureDefaultParts[SW_MINIFIGURE_ITEM_COUNT] = { "4485.dat", "", "3626c.dat", "", "973.dat", "3815.dat", "", "3819.dat", "3818.dat", "3820.dat", "3820.dat", "", "55299.dat", "3817.dat", "3816.dat", "", "" };

swMinifigureBuilder::swMinifigureBuilder()
	: mModel(new swModel(QString(), nullptr, false))
{
	LoadSettings();
	LoadTemplates();

	swMinifigureTemplate& Template = mTemplates[GetDefaultTemplateName()];
	swMinifigureTemplate& EmptyTemplate = mTemplates[GetEmptyTemplateName()];

	for (int PartIndex = 0; PartIndex < SW_MINIFIGURE_ITEM_COUNT; PartIndex++)
	{
		Template.Parts[PartIndex] = gMinifigureDefaultParts[PartIndex];
		Template.ColorCodes[PartIndex] = gMinifigureDefaultColorCodes[PartIndex];
		Template.Angles[PartIndex] = 0.0f;

		EmptyTemplate.Parts[PartIndex].clear();
		EmptyTemplate.ColorCodes[PartIndex] = gMinifigureDefaultColorCodes[PartIndex];
		EmptyTemplate.Angles[PartIndex] = 0.0f;
	}

	for (int PartIndex = 0; PartIndex < SW_MINIFIGURE_ITEM_COUNT; PartIndex++)
	{
		mMinifigure.ColorIndices[PartIndex] = gDefaultColor;
		mMinifigure.Angles[PartIndex] = 0.0f;
		mMinifigure.Matrices[PartIndex] = swMatrix44Identity();
	}
}

swMinifigureBuilder::~swMinifigureBuilder()
{
	SaveTemplates();
}

QString swMinifigureBuilder::GetDefaultTemplateName()
{
	return QCoreApplication::translate("swMinifigureBuilder", "Default");
}

QString swMinifigureBuilder::GetEmptyTemplateName()
{
	return QCoreApplication::translate("swMinifigureBuilder", "Empty");
}

// use to tell the templates that are not saved
bool swMinifigureBuilder::IsBuiltInTemplate(const QString& TemplateName)
{
	return TemplateName == GetDefaultTemplateName() || TemplateName == GetEmptyTemplateName();
}

void swMinifigureBuilder::LoadSettings()
{
	QString CustomSettingsPath = swGetProfileString(SW_PROFILE_MINIFIGURE_BUILDER_SETTINGS);

	if (!CustomSettingsPath.isEmpty())
	{
		swDiskFile DiskSettings(CustomSettingsPath);

		if (DiskSettings.Open(QIODevice::ReadOnly))
		{
			ParseSettings(DiskSettings);
			AddLibraryParts();
			return;
		}
	}

	swDiskFile MinifigureBuilderSettingsFile(":/resources/data/minifig.ini");

	if (MinifigureBuilderSettingsFile.Open(QIODevice::ReadOnly))
		ParseSettings(MinifigureBuilderSettingsFile);

	AddLibraryParts();
}

// use to find a part under its older or newer name
static swPartData* swFindMinifigurePart(const char* PartName)
{
	swPartsLibrary* Library = swGetPartsLibrary();
	swPartData* PartData = Library->FindPartData(PartName, nullptr);

	if (PartData)
		return PartData;

	QByteArray Name = QByteArray(PartName).trimmed().toLower();

	if (Name.endsWith(".dat"))
		Name.chop(4);

	if (Name.isEmpty())
		return nullptr;

	QList<QByteArray> Candidates;
	const int NameLength = Name.size();

	// newer name on an older library
	if (NameLength > 1 && isalpha(Name[NameLength - 1]) && isdigit(Name[NameLength - 2]))
		Candidates.append(Name.left(NameLength - 1));

	// older name on a newer library
	for (const char Suffix : { 'a', 'b', 'c', 'd' })
		Candidates.append(Name + Suffix);

	for (const QByteArray& Candidate : Candidates)
	{
		PartData = Library->FindPartData((Candidate + ".dat").constData(), nullptr);

		if (PartData && !PartData->IsSubPart())
			return PartData;
	}

	return nullptr;
}

void swMinifigureBuilder::ParseSettings(swFile& Settings)
{
	for (int SectionIndex = 0; SectionIndex < SW_MINIFIGURE_ITEM_COUNT; SectionIndex++)
	{
		std::vector<swMinifigurePartInformation>& InfoArray = mSettings[SectionIndex];

		InfoArray.clear();
		Settings.Seek(0, SEEK_SET);

		char Line[1024];
		bool FoundSection = false;
		const char* SectionName = mSectionNames[SectionIndex];
		const size_t SectionNameLength = strlen(SectionName);

		while (Settings.ReadLine(Line, sizeof(Line)))
		{
			if (Line[0] == '[' && !strncmp(Line + 1, SectionName, SectionNameLength) && Line[SectionNameLength + 1] == ']')
			{
				FoundSection = true;
				break;
			}
		}

		if (!FoundSection)
			continue;

		while (Settings.ReadLine(Line, sizeof(Line)))
		{
			if (Line[0] == '[')
				break;

			char* DescriptionStart = strchr(Line, '"');
			if (!DescriptionStart)
				continue;
			DescriptionStart++;
			char* DescriptionEnd = strchr(DescriptionStart, '"');
			if (!DescriptionEnd)
				continue;
			*DescriptionEnd = 0;
			DescriptionEnd++;

			char* NameStart = strchr(DescriptionEnd, '"');
			if (!NameStart)
				continue;
			NameStart++;
			char* NameEnd = strchr(NameStart, '"');
			if (!NameEnd)
				continue;
			*NameEnd = 0;
			NameEnd++;

			swPartData* PartData = swFindMinifigurePart(NameStart);
			if (!PartData)
				continue;

			// listed once per section
			if (std::any_of(InfoArray.begin(), InfoArray.end(), [PartData](const swMinifigurePartInformation& Info) { return Info.PartData == PartData; }))
				continue;

			float Mat[12];
			int Flags;

			if (sscanf(NameEnd, "%d %g %g %g %g %g %g %g %g %g %g %g %g",
					   &Flags, &Mat[0], &Mat[1], &Mat[2], &Mat[3], &Mat[4], &Mat[5], &Mat[6],
					   &Mat[7], &Mat[8], &Mat[9], &Mat[10], &Mat[11]) != 13)
				continue;

			swMatrix44 Offset = swMatrix44Identity();
			float* OffsetMatrix = &Offset[0][0];

			OffsetMatrix[0] =  Mat[0];
			OffsetMatrix[8] = -Mat[1];
			OffsetMatrix[4] =  Mat[2];
			OffsetMatrix[2] = -Mat[3];
			OffsetMatrix[10] = Mat[4];
			OffsetMatrix[6] = -Mat[5];
			OffsetMatrix[1] =  Mat[6];
			OffsetMatrix[9] = -Mat[7];
			OffsetMatrix[5] =  Mat[8];
			OffsetMatrix[12] =  Mat[9];
			OffsetMatrix[14] = -Mat[10];
			OffsetMatrix[13] =  Mat[11];

			swMinifigurePartInformation MinifigurePartInformation;
			swstrcpy(MinifigurePartInformation.Description, DescriptionStart);
			MinifigurePartInformation.Offset = Offset;
			MinifigurePartInformation.PartData = PartData;

			InfoArray.emplace_back(std::move(MinifigurePartInformation));
		}
	}
}

// use to get the matching section on the other side
static int swGetMinifigureMirrorSection(int SectionIndex)
{
	switch (SectionIndex)
	{
	case SW_MINIFIGURE_RIGHT_ARM:
		return SW_MINIFIGURE_LEFT_ARM;
	case SW_MINIFIGURE_LEFT_ARM:
		return SW_MINIFIGURE_RIGHT_ARM;
	case SW_MINIFIGURE_RIGHT_HAND:
		return SW_MINIFIGURE_LEFT_HAND;
	case SW_MINIFIGURE_LEFT_HAND:
		return SW_MINIFIGURE_RIGHT_HAND;
	case SW_MINIFIGURE_RIGHT_HAND_ACCESSORY:
		return SW_MINIFIGURE_LEFT_HAND_ACCESSORY;
	case SW_MINIFIGURE_LEFT_HAND_ACCESSORY:
		return SW_MINIFIGURE_RIGHT_HAND_ACCESSORY;
	case SW_MINIFIGURE_RIGHT_LEG:
		return SW_MINIFIGURE_LEFT_LEG;
	case SW_MINIFIGURE_LEFT_LEG:
		return SW_MINIFIGURE_RIGHT_LEG;
	case SW_MINIFIGURE_RIGHT_LEG_ACCESSORY:
		return SW_MINIFIGURE_LEFT_LEG_ACCESSORY;
	case SW_MINIFIGURE_LEFT_LEG_ACCESSORY:
		return SW_MINIFIGURE_RIGHT_LEG_ACCESSORY;
	default:
		return -1;
	}
}

// use to get the description words of a part
static QStringList swGetMinifigureDescriptionWords(const swPartData* PartData)
{
	QString Description = QString::fromLatin1(PartData->m_strDescription);

	if (Description.startsWith('_'))
		Description.remove(0, 1);

	return Description.toLower().replace(',', ' ').split(' ', Qt::SkipEmptyParts);
}

// use to add the library minifigure parts missing from the lists
void swMinifigureBuilder::AddLibraryParts()
{
	constexpr int MinimumPrefixWords = 2;
	constexpr int MaximumPrefixWords = 4;
	using swSectionCounts = std::array<int, SW_MINIFIGURE_ITEM_COUNT>;

	std::map<QString, swSectionCounts> PrefixSections;
	std::map<const swPartData*, std::vector<std::pair<int, swMatrix44>>> ListedParts;

	// learn the sections from the listed parts
	for (int SectionIndex = 0; SectionIndex < SW_MINIFIGURE_ITEM_COUNT; SectionIndex++)
	{
		for (const swMinifigurePartInformation& Info : mSettings[SectionIndex])
		{
			ListedParts[Info.PartData].emplace_back(SectionIndex, Info.Offset);

			const QStringList Words = swGetMinifigureDescriptionWords(Info.PartData);

			for (int WordCount = MinimumPrefixWords; WordCount <= MaximumPrefixWords && WordCount <= Words.size(); WordCount++)
			{
				swSectionCounts& Counts = PrefixSections.try_emplace(Words.mid(0, WordCount).join(' '), swSectionCounts{}).first->second;
				Counts[SectionIndex]++;
			}
		}
	}

	swPartsLibrary* Library = swGetPartsLibrary();

	auto AddPart = [this](int SectionIndex, swPartData* PartData, const swMatrix44& Offset)
	{
		swMinifigurePartInformation Info;
		const char* Description = PartData->m_strDescription;

		if (*Description == '_')
			Description++;

		if (!strncmp(Description, "Minifig ", 8))
			Description += 8;

		swstrcpy(Info.Description, Description);
		Info.PartData = PartData;
		Info.Offset = Offset;

		mSettings[SectionIndex].emplace_back(std::move(Info));
	};

	for (swPartData* PartData : Library->GetParts())
	{
		if (!PartData->IsLibraryPart() || PartData->IsSubPart() || PartData->IsAlias() || ListedParts.count(PartData))
			continue;

		const char* Description = PartData->m_strDescription;

		if (*Description == '_')
			Description++;

		// assemblies and retired parts stay out
		if (strncmp(Description, "Minifig ", 8) || strstr(Description, "Shortcut") || strstr(Description, "(Obsolete)"))
			continue;

		// patterns follow their parent part
		if (PartData->IsDecorated())
		{
			const auto ParentIt = ListedParts.find(Library->FindPartData(PartData->GetPatternParentId().c_str(), nullptr));

			if (ParentIt != ListedParts.end())
			{
				for (const std::pair<int, swMatrix44>& Section : ParentIt->second)
					AddPart(Section.first, PartData, Section.second);

				continue;
			}
		}

		// longest known description start
		const QStringList Words = swGetMinifigureDescriptionWords(PartData);
		const swSectionCounts* Counts = nullptr;

		for (int WordCount = qMin(MaximumPrefixWords, static_cast<int>(Words.size())); WordCount >= MinimumPrefixWords && !Counts; WordCount--)
		{
			const auto PrefixIt = PrefixSections.find(Words.mid(0, WordCount).join(' '));

			if (PrefixIt != PrefixSections.end())
				Counts = &PrefixIt->second;
		}

		// unknown items are held
		if (!Counts)
		{
			AddPart(SW_MINIFIGURE_RIGHT_HAND_ACCESSORY, PartData, swMatrix44Identity());
			AddPart(SW_MINIFIGURE_LEFT_HAND_ACCESSORY, PartData, swMatrix44Identity());
			continue;
		}

		const int SectionIndex = static_cast<int>(std::max_element(Counts->begin(), Counts->end()) - Counts->begin());
		const int MirrorIndex = swGetMinifigureMirrorSection(SectionIndex);

		AddPart(SectionIndex, PartData, swMatrix44Identity());

		if (MirrorIndex != -1 && (*Counts)[MirrorIndex] * 4 >= (*Counts)[SectionIndex])
			AddPart(MirrorIndex, PartData, swMatrix44Identity());
	}

	for (std::vector<swMinifigurePartInformation>& InfoArray : mSettings)
	{
		std::stable_sort(InfoArray.begin(), InfoArray.end(), [](const swMinifigurePartInformation& First, const swMinifigurePartInformation& Second)
		{
			return qstricmp(First.Description, Second.Description) < 0;
		});
	}
}

void swMinifigureBuilder::LoadTemplate(const QString& TemplateName)
{
	const std::map<QString, swMinifigureTemplate>::iterator& TemplateIt = mTemplates.find(TemplateName);

	if (TemplateIt != mTemplates.end())
		ApplyTemplate(TemplateIt->second);
}

void swMinifigureBuilder::ApplyTemplate(const swMinifigureTemplate& Template)
{
	swPartsLibrary* Library = swGetPartsLibrary();

	for (int PartIndex = 0; PartIndex < SW_MINIFIGURE_ITEM_COUNT; PartIndex++)
	{
		mMinifigure.ColorIndices[PartIndex] = swGetColorIndex(Template.ColorCodes[PartIndex]);
		mMinifigure.Angles[PartIndex] = Template.Angles[PartIndex];
		mMinifigure.Matrices[PartIndex] = swMatrix44Identity();
		mMinifigure.PartData[PartIndex] = swPartDataReference();

		if (Template.Parts[PartIndex].isEmpty())
			continue;

		swPartData* PartData = swFindMinifigurePart(Template.Parts[PartIndex].toLatin1().constData());

		if (PartData)
			mMinifigure.PartData[PartIndex] = swPartDataReference(PartData);
		else
			mMinifigure.PartData[PartIndex] = Library->AcquirePartData(Template.Parts[PartIndex].toLatin1(), nullptr, false, false);

		if (mMinifigure.PartData[PartIndex])
			Library->RequestPartDataLoad(mMinifigure.PartData[PartIndex].Get(), swLoadPriority::Normal);
	}

	std::vector<swPartDataReference> RequiredPartData;

	for (const swPartDataReference& PartData : mMinifigure.PartData)
		if (PartData)
			RequiredPartData.push_back(PartData);

	Library->WaitForPartData(RequiredPartData);

	Calculate();
}

swMinifigureTemplate swMinifigureBuilder::GetCurrentTemplate() const
{
	swMinifigureTemplate Template;

	for (int PartIndex = 0; PartIndex < SW_MINIFIGURE_ITEM_COUNT; PartIndex++)
	{
		const swPartData* PartData = mMinifigure.PartData[PartIndex].Get();

		Template.Parts[PartIndex] = PartData ? QString::fromLatin1(PartData->mFileName) : QString();
		Template.ColorCodes[PartIndex] = swGetColorCode(mMinifigure.ColorIndices[PartIndex]);
		Template.Angles[PartIndex] = mMinifigure.Angles[PartIndex];
	}

	return Template;
}

// use to build a random minifigure
void swMinifigureBuilder::Randomize()
{
	QRandomGenerator* Random = QRandomGenerator::global();

	const int ColorCodes[] = { 0, 1, 2, 4, 14, 15, 19, 25, 26, 28, 70, 71, 72, 73, 85, 272, 288, 308, 320, 326 };
	const int SkinColorCodes[] = { 14, 14, 14, 78, 84, 92 };

	auto RandomChance = [Random](int Percent)
	{
		return Random->bounded(100) < Percent;
	};

	auto RandomColor = [Random, &ColorCodes]()
	{
		return ColorCodes[Random->bounded(static_cast<int>(std::size(ColorCodes)))];
	};

	auto RandomPart = [this, Random](int SectionIndex)
	{
		const std::vector<swMinifigurePartInformation>& InfoArray = mSettings[SectionIndex];
		return InfoArray.empty() ? QString() : QString::fromLatin1(InfoArray[Random->bounded(static_cast<int>(InfoArray.size()))].PartData->mFileName);
	};

	swMinifigureTemplate Template;

	// plain parts unless picked below
	for (int PartIndex = 0; PartIndex < SW_MINIFIGURE_ITEM_COUNT; PartIndex++)
	{
		Template.Parts[PartIndex] = gMinifigureDefaultParts[PartIndex];
		Template.ColorCodes[PartIndex] = RandomColor();
		Template.Angles[PartIndex] = 0.0f;
	}

	const int SkinColorCode = SkinColorCodes[Random->bounded(static_cast<int>(std::size(SkinColorCodes)))];
	const int TorsoColorCode = Template.ColorCodes[SW_MINIFIGURE_BODY];
	const int HipColorCode = Template.ColorCodes[SW_MINIFIGURE_BODY2];

	Template.Parts[SW_MINIFIGURE_HATS] = RandomChance(85) ? RandomPart(SW_MINIFIGURE_HATS) : QString();
	Template.Parts[SW_MINIFIGURE_HEAD] = RandomPart(SW_MINIFIGURE_HEAD);
	Template.ColorCodes[SW_MINIFIGURE_HEAD] = SkinColorCode;
	Template.Parts[SW_MINIFIGURE_NECK] = RandomChance(15) ? RandomPart(SW_MINIFIGURE_NECK) : QString();
	Template.Parts[SW_MINIFIGURE_BODY] = RandomPart(SW_MINIFIGURE_BODY);

	for (int ArmIndex : { SW_MINIFIGURE_RIGHT_ARM, SW_MINIFIGURE_LEFT_ARM })
		Template.ColorCodes[ArmIndex] = RandomChance(70) ? TorsoColorCode : Template.ColorCodes[ArmIndex];

	const int HandColorCode = RandomChance(70) ? SkinColorCode : RandomColor();

	for (int HandIndex : { SW_MINIFIGURE_RIGHT_HAND, SW_MINIFIGURE_LEFT_HAND })
		Template.ColorCodes[HandIndex] = HandColorCode;

	const int LegColorCode = RandomChance(70) ? HipColorCode : RandomColor();

	for (int LegIndex : { SW_MINIFIGURE_RIGHT_LEG, SW_MINIFIGURE_LEFT_LEG })
		Template.ColorCodes[LegIndex] = LegColorCode;

	Template.Parts[SW_MINIFIGURE_RIGHT_HAND_ACCESSORY] = RandomChance(40) ? RandomPart(SW_MINIFIGURE_RIGHT_HAND_ACCESSORY) : QString();
	Template.Parts[SW_MINIFIGURE_LEFT_HAND_ACCESSORY] = RandomChance(20) ? RandomPart(SW_MINIFIGURE_LEFT_HAND_ACCESSORY) : QString();

	ApplyTemplate(Template);
}

// use to rebuild the lists from the current library
void swMinifigureBuilder::ReloadSettings()
{
	const swMinifigureTemplate Template = GetCurrentTemplate();

	LoadSettings();
	ApplyTemplate(Template);
}

void swMinifigureBuilder::SaveTemplate(const QString& TemplateName, const swMinifigureTemplate& Template)
{
	mTemplates[TemplateName] = Template;
}

void swMinifigureBuilder::DeleteTemplate(const QString& TemplateName)
{
	mTemplates.erase(TemplateName);
}

void swMinifigureBuilder::AddTemplatesJson(const QByteArray& TemplateData)
{
	QJsonDocument Document = QJsonDocument::fromJson(TemplateData);
	QJsonObject RootObject = Document.object();

	int Version = RootObject["Version"].toInt(0);
	QJsonObject TemplatesObject;

	if (Version > 0)
		TemplatesObject = RootObject["Templates"].toObject();
	else
		TemplatesObject = RootObject;

	for (QJsonObject::const_iterator ElementIt = TemplatesObject.constBegin(); ElementIt != TemplatesObject.constEnd(); ElementIt++)
	{
		if (!ElementIt.value().isObject())
			continue;

		if (IsBuiltInTemplate(ElementIt.key()))
			continue;

		QJsonObject TemplateObject = ElementIt.value().toObject();
		swMinifigureTemplate Template;

		for (int PartIdx = 0; PartIdx < SW_MINIFIGURE_ITEM_COUNT; PartIdx++)
		{
			QJsonObject PartObject = TemplateObject.value(QLatin1String(mSectionNames[PartIdx])).toObject();

			Template.Parts[PartIdx] = PartObject["Id"].toString();
			Template.ColorCodes[PartIdx] = PartObject["Color"].toInt();
			Template.Angles[PartIdx] = PartObject["Angle"].toDouble();
		}

		mTemplates.emplace(ElementIt.key(), std::move(Template));
	}
}

QByteArray swMinifigureBuilder::GetTemplatesJson() const
{
	QJsonObject TemplatesObject;

	for (const auto& TemplateEntry : mTemplates)
	{
		if (IsBuiltInTemplate(TemplateEntry.first))
			continue;

		const swMinifigureTemplate& Template = TemplateEntry.second;
		QJsonObject TemplateObject;

		for (int PartIdx = 0; PartIdx < SW_MINIFIGURE_ITEM_COUNT; PartIdx++)
		{
			QJsonObject PartObject;

			PartObject["Id"] = Template.Parts[PartIdx];
			PartObject["Color"] = Template.ColorCodes[PartIdx];
			PartObject["Angle"] = Template.Angles[PartIdx];

			TemplateObject[QLatin1String(mSectionNames[PartIdx])] = PartObject;
		}

		TemplatesObject[TemplateEntry.first] = TemplateObject;
	}

	QJsonObject RootObject;
	RootObject["Templates"] = TemplatesObject;
	RootObject["Version"] = 1;

	return QJsonDocument(RootObject).toJson();
}

void swMinifigureBuilder::LoadTemplates()
{
	mTemplates.clear();

	QSettings Settings;
	Settings.beginGroup("Minifig");
	QByteArray TemplateData = Settings.value("Templates").toByteArray();

	AddTemplatesJson(TemplateData);
}

void swMinifigureBuilder::SaveTemplates()
{
	QSettings Settings;
	Settings.beginGroup("Minifig");
	Settings.setValue("Templates", GetTemplatesJson());
}

void swMinifigureBuilder::Calculate()
{
	float HeadOffset = 0.0f;
	swMatrix44 Root, Mat, Mat2;

	const swPartDataReference* PartData = mMinifigure.PartData;
	const float* Angles = mMinifigure.Angles;
	swMatrix44* Matrices = mMinifigure.Matrices;

	const bool DroidTorso = PartData[SW_MINIFIGURE_BODY] && !qstricmp(PartData[SW_MINIFIGURE_BODY]->mFileName, "30375.dat");
	const bool SkeletonTorso = PartData[SW_MINIFIGURE_BODY] && !qstricmp(PartData[SW_MINIFIGURE_BODY]->mFileName, "6260.dat");

	// skirts raise the hips while the legs stay on the ground
	const float HipHeight = PartData[SW_MINIFIGURE_BODY3] ? 74.0f : 72.0f;

	// land the feet on stud centres
	Root = swMatrix44Translation(swVector3(0, 11.0f, HipHeight));
	Matrices[SW_MINIFIGURE_BODY] = swMultiply(GetSelectionOffset(SW_MINIFIGURE_BODY), Root);

	if (PartData[SW_MINIFIGURE_NECK])
	{
		Matrices[SW_MINIFIGURE_NECK] = swMultiply(GetSelectionOffset(SW_MINIFIGURE_NECK), Root);
		HeadOffset = 0.08f;
	}

	if (PartData[SW_MINIFIGURE_HEAD])
	{
		Mat = swMatrix44RotationZ(-SW_DEGREES_TO_RADIANS * Angles[SW_MINIFIGURE_HEAD]);
		Mat.SetTranslation(swVector3(0.0f, 0.0f, 24.0f + HeadOffset));
		Mat = swMultiply(GetSelectionOffset(SW_MINIFIGURE_HEAD), Mat);
		Matrices[SW_MINIFIGURE_HEAD] = swMultiply(Mat, Root);
	}

	if (PartData[SW_MINIFIGURE_HATS])
	{
		Mat = swMatrix44RotationZ(-SW_DEGREES_TO_RADIANS * Angles[SW_MINIFIGURE_HATS]);
		Mat = swMultiply(GetSelectionOffset(SW_MINIFIGURE_HATS), Mat);
		Matrices[SW_MINIFIGURE_HATS] = swMultiply(Mat, Matrices[SW_MINIFIGURE_HEAD]);
	}

	if (PartData[SW_MINIFIGURE_HATS2])
	{
		Mat = swMatrix44RotationX(-SW_DEGREES_TO_RADIANS * Angles[SW_MINIFIGURE_HATS2]);
		Mat = swMultiply(GetSelectionOffset(SW_MINIFIGURE_HATS2), Mat);
		Matrices[SW_MINIFIGURE_HATS2] = swMultiply(Mat, Matrices[SW_MINIFIGURE_HATS]);
	}

	if (PartData[SW_MINIFIGURE_RIGHT_ARM])
	{
		Mat = swMatrix44RotationX(-SW_DEGREES_TO_RADIANS * Angles[SW_MINIFIGURE_RIGHT_ARM]);

		if (DroidTorso || SkeletonTorso)
			Mat2 = swMatrix44Identity();
		else
			Mat2 = swMatrix44RotationY(-SW_DEGREES_TO_RADIANS * 9.791f);
		Mat2.SetTranslation(swVector3(15.552f, 0, -8.88f));

		Mat = swMultiply(GetSelectionOffset(SW_MINIFIGURE_RIGHT_ARM), Mat);
		Mat = swMultiply(Mat, Mat2);
		Matrices[SW_MINIFIGURE_RIGHT_ARM] = swMultiply(Mat, Root);
	}

	if (PartData[SW_MINIFIGURE_RIGHT_HAND])
	{
		Mat = swMatrix44RotationY(-SW_DEGREES_TO_RADIANS * Angles[SW_MINIFIGURE_RIGHT_HAND]);
		Mat2 = swMatrix44RotationX(SW_DEGREES_TO_RADIANS * 45);
		Mat = swMultiply(GetSelectionOffset(SW_MINIFIGURE_RIGHT_HAND), Mat);
		Mat = swMultiply(Mat, Mat2);
		Mat.SetTranslation(swVector3(5.0f, -10.0f, -19.0f));
		Matrices[SW_MINIFIGURE_RIGHT_HAND] = swMultiply(Mat, Matrices[SW_MINIFIGURE_RIGHT_ARM]);
	}

	if (PartData[SW_MINIFIGURE_RIGHT_HAND_ACCESSORY])
	{
		Mat = swMatrix44RotationZ(SW_DEGREES_TO_RADIANS * Angles[SW_MINIFIGURE_RIGHT_HAND_ACCESSORY]);
		Mat.SetTranslation(swVector3(0, -9.25f, 0));
		Mat = swMultiply(GetSelectionOffset(SW_MINIFIGURE_RIGHT_HAND_ACCESSORY), Mat);
		Mat = swMultiply(Mat, swMatrix44RotationX(SW_DEGREES_TO_RADIANS * 15.0f));
		Matrices[SW_MINIFIGURE_RIGHT_HAND_ACCESSORY] = swMultiply(Mat, Matrices[SW_MINIFIGURE_RIGHT_HAND]);
	}

	if (PartData[SW_MINIFIGURE_LEFT_ARM])
	{
		Mat = swMatrix44RotationX(-SW_DEGREES_TO_RADIANS * Angles[SW_MINIFIGURE_LEFT_ARM]);

		if (DroidTorso || SkeletonTorso)
			Mat2 = swMatrix44Identity();
		else
			Mat2 = swMatrix44RotationY(SW_DEGREES_TO_RADIANS * 9.791f);
		Mat2.SetTranslation(swVector3(-15.552f, 0.0f, -8.88f));

		Mat = swMultiply(GetSelectionOffset(SW_MINIFIGURE_LEFT_ARM), Mat);
		Mat = swMultiply(Mat, Mat2);
		Matrices[SW_MINIFIGURE_LEFT_ARM] = swMultiply(Mat, Root);
	}

	if (PartData[SW_MINIFIGURE_LEFT_HAND])
	{
		Mat = swMatrix44RotationY(-SW_DEGREES_TO_RADIANS * Angles[SW_MINIFIGURE_LEFT_HAND]);
		Mat2 = swMatrix44RotationX(SW_DEGREES_TO_RADIANS * 45);
		Mat = swMultiply(GetSelectionOffset(SW_MINIFIGURE_LEFT_HAND), Mat);
		Mat = swMultiply(Mat, Mat2);
		Mat.SetTranslation(swVector3(-5.0f, -10.0f, -19.0f));
		Matrices[SW_MINIFIGURE_LEFT_HAND] = swMultiply(Mat, Matrices[SW_MINIFIGURE_LEFT_ARM]);
	}

	if (PartData[SW_MINIFIGURE_LEFT_HAND_ACCESSORY])
	{
		Mat = swMatrix44RotationZ(SW_DEGREES_TO_RADIANS * Angles[SW_MINIFIGURE_LEFT_HAND_ACCESSORY]);
		Mat.SetTranslation(swVector3(0, -9.25f, 0));
		Mat = swMultiply(GetSelectionOffset(SW_MINIFIGURE_LEFT_HAND_ACCESSORY), Mat);
		Mat = swMultiply(Mat, swMatrix44RotationX(SW_DEGREES_TO_RADIANS * 15.0f));
		Matrices[SW_MINIFIGURE_LEFT_HAND_ACCESSORY] = swMultiply(Mat, Matrices[SW_MINIFIGURE_LEFT_HAND]);
	}

	if (PartData[SW_MINIFIGURE_BODY2])
	{
		Mat = swMatrix44Identity();
		Mat.SetTranslation(swVector3(0, 0, -32.0f));
		Mat = swMultiply(GetSelectionOffset(SW_MINIFIGURE_BODY2), Mat);
		Matrices[SW_MINIFIGURE_BODY2] = swMultiply(Mat, Root);
	}

	if (PartData[SW_MINIFIGURE_BODY3])
	{
		Mat = swMatrix44Identity();
		Mat.SetTranslation(swVector3(0, 0, -32.0f));
		Mat = swMultiply(GetSelectionOffset(SW_MINIFIGURE_BODY3), Mat);
		Matrices[SW_MINIFIGURE_BODY3] = swMultiply(Mat, Root);
	}

	if (PartData[SW_MINIFIGURE_RIGHT_LEG])
	{
		Mat = swMatrix44RotationX(-SW_DEGREES_TO_RADIANS * Angles[SW_MINIFIGURE_RIGHT_LEG]);
		Mat.SetTranslation(swVector3(0, 0, 28.0f - HipHeight));
		Mat = swMultiply(GetSelectionOffset(SW_MINIFIGURE_RIGHT_LEG), Mat);
		Matrices[SW_MINIFIGURE_RIGHT_LEG] = swMultiply(Mat, Root);
	}

	if (PartData[SW_MINIFIGURE_RIGHT_LEG_ACCESSORY])
	{
		const swVector3 Center(-10.0f, -1.0f, -28.0f);
		Mat = swMatrix44RotationZ(SW_DEGREES_TO_RADIANS * Angles[SW_MINIFIGURE_RIGHT_LEG_ACCESSORY]);
		Mat2 = GetSelectionOffset(SW_MINIFIGURE_RIGHT_LEG_ACCESSORY);
		Mat2.SetTranslation(swMultiply31(-Center, Mat2));
		Mat = swMultiply(Mat2, Mat);
		Mat.SetTranslation(swMultiply31(Center, Mat2));
		Matrices[SW_MINIFIGURE_RIGHT_LEG_ACCESSORY] = swMultiply(Mat, Matrices[SW_MINIFIGURE_RIGHT_LEG]);
	}

	if (PartData[SW_MINIFIGURE_LEFT_LEG])
	{
		Mat = swMatrix44RotationX(-SW_DEGREES_TO_RADIANS * Angles[SW_MINIFIGURE_LEFT_LEG]);
		Mat.SetTranslation(swVector3(0, 0, 28.0f - HipHeight));
		Mat = swMultiply(GetSelectionOffset(SW_MINIFIGURE_LEFT_LEG), Mat);
		Matrices[SW_MINIFIGURE_LEFT_LEG] = swMultiply(Mat, Root);
	}

	if (PartData[SW_MINIFIGURE_LEFT_LEG_ACCESSORY])
	{
		const swVector3 Center(10.0f, -1.0f, -28.0f);
		Mat = swMatrix44RotationZ(SW_DEGREES_TO_RADIANS * Angles[SW_MINIFIGURE_LEFT_LEG_ACCESSORY]);
		Mat2 = GetSelectionOffset(SW_MINIFIGURE_LEFT_LEG_ACCESSORY);
		Mat2.SetTranslation(swMultiply31(-Center, Mat2));
		Mat = swMultiply(Mat2, Mat);
		Mat.SetTranslation(swMultiply31(Center, Mat2));
		Matrices[SW_MINIFIGURE_LEFT_LEG_ACCESSORY] = swMultiply(Mat, Matrices[SW_MINIFIGURE_LEFT_LEG]);
	}

	mModel->SetMinifigure(mMinifigure);
}

int swMinifigureBuilder::GetSelectionIndex(int Type) const
{
	const std::vector<swMinifigurePartInformation>& InfoArray = mSettings[Type];

	for (size_t Index = 0; Index < InfoArray.size(); Index++)
		if (mMinifigure.PartData[Type] && InfoArray[Index].PartData == mMinifigure.PartData[Type])
			return (int)Index;

	return -1;
}

// use to get the selected part offset
swMatrix44 swMinifigureBuilder::GetSelectionOffset(int Type) const
{
	const int SelectionIndex = GetSelectionIndex(Type);

	if (SelectionIndex == -1)
		return swMatrix44Identity();

	return mSettings[Type][SelectionIndex].Offset;
}

void swMinifigureBuilder::SetPartData(int Type, swPartData* PartData)
{
	mMinifigure.PartData[Type] = swPartDataReference(PartData);

	if (mMinifigure.PartData[Type])
		swGetPartsLibrary()->WaitForPartData(mMinifigure.PartData[Type]);

	Calculate();
}

void swMinifigureBuilder::SetColorIndex(int Type, int ColorIndex)
{
	mMinifigure.ColorIndices[Type] = ColorIndex;

	Calculate();
}

void swMinifigureBuilder::SetAngle(int Type, float Angle)
{
	mMinifigure.Angles[Type] = Angle;

	Calculate();
}
