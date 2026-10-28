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

swMinifigureBuilder::swMinifigureBuilder()
	: mModel(new swModel(QString(), nullptr, false))
{
	LoadSettings();
	LoadTemplates();

	swMinifigureTemplate& Template = mTemplates[GetDefaultTemplateName()];
	const int ColorCodes[SW_MINIFIGURE_ITEM_COUNT] = { 4, 7, 14, 7, 1, 0, 7, 4, 4, 14, 14, 7, 7, 0, 0, 7, 7 };
	const char* const Parts[SW_MINIFIGURE_ITEM_COUNT] = { "3624.dat", "", "3626bp01.dat", "", "973.dat", "3815.dat", "", "3819.dat", "3818.dat", "3820.dat", "3820.dat", "", "", "3817.dat", "3816.dat", "", "" };

	for (int PartIndex = 0; PartIndex < SW_MINIFIGURE_ITEM_COUNT; PartIndex++)
	{
		Template.Parts[PartIndex] = Parts[PartIndex];
		Template.ColorCodes[PartIndex] = ColorCodes[PartIndex];
		Template.Angles[PartIndex] = 0.0f;
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

void swMinifigureBuilder::LoadSettings()
{
	QString CustomSettingsPath = swGetProfileString(SW_PROFILE_MINIFIGURE_BUILDER_SETTINGS);

	if (!CustomSettingsPath.isEmpty())
	{
		swDiskFile DiskSettings(CustomSettingsPath);

		if (DiskSettings.Open(QIODevice::ReadOnly))
		{
			ParseSettings(DiskSettings);
			return;
		}
	}

	swDiskFile MinifigureBuilderSettingsFile(":/resources/data/minifig.ini");

	if (MinifigureBuilderSettingsFile.Open(QIODevice::ReadOnly))
		ParseSettings(MinifigureBuilderSettingsFile);
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
		{
			swMinifigurePartInformation MinifigurePartInformation;
			swstrcpy(MinifigurePartInformation.Description, "None");
			MinifigurePartInformation.Offset = swMatrix44Identity();
			MinifigurePartInformation.PartData = nullptr;

			InfoArray.emplace_back(std::move(MinifigurePartInformation));
			continue;
		}

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

			swPartData* PartData = swGetPartsLibrary()->FindPartData(NameStart, nullptr);
			if (!PartData && *NameStart)
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

void swMinifigureBuilder::LoadTemplate(const QString& TemplateName)
{
	const std::map<QString, swMinifigureTemplate>::iterator& TemplateIt = mTemplates.find(TemplateName);

	if (TemplateIt == mTemplates.end())
		return;

	const swMinifigureTemplate& Template = TemplateIt->second;
	swPartsLibrary* Library = swGetPartsLibrary();

	for (int PartIndex = 0; PartIndex < SW_MINIFIGURE_ITEM_COUNT; PartIndex++)
	{
		mMinifigure.ColorIndices[PartIndex] = swGetColorIndex(Template.ColorCodes[PartIndex]);
		mMinifigure.Angles[PartIndex] = Template.Angles[PartIndex];
		mMinifigure.Matrices[PartIndex] = swMatrix44Identity();

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

	if (PartData[SW_MINIFIGURE_BODY3])
		Root = swMatrix44Translation(swVector3(0, 0, 74.0f));
	else
		Root = swMatrix44Translation(swVector3(0, 0, 72.0f));
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
		Mat.SetTranslation(swVector3(0, 0, -44.0f));
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
		Mat.SetTranslation(swVector3(0, 0, -44.0f));
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
		if (InfoArray[Index].PartData == mMinifigure.PartData[Type])
			return (int)Index;

	return 0;
}

// use to get the offset of the selected part, none without a parts library
swMatrix44 swMinifigureBuilder::GetSelectionOffset(int Type) const
{
	if (mSettings[Type].empty())
		return swMatrix44Identity();

	return mSettings[Type][GetSelectionIndex(Type)].Offset;
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
