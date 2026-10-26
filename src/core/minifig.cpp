#include "sw_global.h"
#include "minifig.h"
#include "sw_colors.h"
#include "sw_piecedata.h"
#include "sw_model.h"
#include "sw_library.h"
#include "sw_application.h"
#include "sw_file.h"
#include "sw_profile.h"
#include "sw_string.h"

const char* MinifigWizard::mSectionNames[SW_MFW_NUMITEMS] =
{
	"HATS",   // SW_MFW_HATS
	"HATS2",  // SW_MFW_HATS2
	"HEAD",   // SW_MFW_HEAD
	"NECK",   // SW_MFW_NECK
	"BODY",   // SW_MFW_BODY
	"BODY2",  // SW_MFW_BODY2
	"BODY3",  // SW_MFW_BODY3
	"RARM",   // SW_MFW_RARM
	"LARM",   // SW_MFW_LARM
	"RHAND",  // SW_MFW_RHAND
	"LHAND",  // SW_MFW_LHAND
	"RHANDA", // SW_MFW_RHANDA
	"LHANDA", // SW_MFW_LHANDA
	"RLEG",   // SW_MFW_RLEG
	"LLEG",   // SW_MFW_LLEG
	"RLEGA",  // SW_MFW_RLEGA
	"LLEGA",  // SW_MFW_LLEGA
};

MinifigWizard::MinifigWizard()
	: mModel(new swModel(QString(), nullptr, false))
{
	LoadSettings();
	LoadTemplates();

	swMinifigTemplate& Template = mTemplates[GetDefaultTemplateName()];
	const int ColorCodes[SW_MFW_NUMITEMS] = { 4, 7, 14, 7, 1, 0, 7, 4, 4, 14, 14, 7, 7, 0, 0, 7, 7 };
	const char* const Pieces[SW_MFW_NUMITEMS] = { "3624.dat", "", "3626bp01.dat", "", "973.dat", "3815.dat", "", "3819.dat", "3818.dat", "3820.dat", "3820.dat", "", "", "3817.dat", "3816.dat", "", "" };

	for (int PartIndex = 0; PartIndex < SW_MFW_NUMITEMS; PartIndex++)
	{
		Template.Parts[PartIndex] = Pieces[PartIndex];
		Template.ColorCodes[PartIndex] = ColorCodes[PartIndex];
		Template.Angles[PartIndex] = 0.0f;
	}

	for (int PartIndex = 0; PartIndex < SW_MFW_NUMITEMS; PartIndex++)
	{
		mMinifig.ColorIndices[PartIndex] = gDefaultColor;
		mMinifig.Angles[PartIndex] = 0.0f;
		mMinifig.Matrices[PartIndex] = swMatrix44Identity();
	}
}

MinifigWizard::~MinifigWizard()
{
	SaveTemplates();
}

QString MinifigWizard::GetDefaultTemplateName()
{
	return QCoreApplication::translate("MinifigWizard", "Default");
}

void MinifigWizard::LoadSettings()
{
	QString CustomSettingsPath = swGetProfileString(SW_PROFILE_MINIFIG_SETTINGS);

	if (!CustomSettingsPath.isEmpty())
	{
		swDiskFile DiskSettings(CustomSettingsPath);

		if (DiskSettings.Open(QIODevice::ReadOnly))
		{
			ParseSettings(DiskSettings);
			return;
		}
	}

	swDiskFile MinifigFile(":/resources/minifig.ini");

	if (MinifigFile.Open(QIODevice::ReadOnly))
		ParseSettings(MinifigFile);
}

void MinifigWizard::ParseSettings(swFile& Settings)
{
	for (int SectionIndex = 0; SectionIndex < SW_MFW_NUMITEMS; SectionIndex++)
	{
		std::vector<swMinifigPieceInfo>& InfoArray = mSettings[SectionIndex];

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
			swMinifigPieceInfo MinifigInfo;
			swstrcpy(MinifigInfo.Description, "None");
			MinifigInfo.Offset = swMatrix44Identity();
			MinifigInfo.PieceData = nullptr;

			InfoArray.emplace_back(std::move(MinifigInfo));
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

			swPieceData* PieceData = swGetPiecesLibrary()->FindPieceData(NameStart, nullptr);
			if (!PieceData && *NameStart)
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

			swMinifigPieceInfo MinifigInfo;
			swstrcpy(MinifigInfo.Description, DescriptionStart);
			MinifigInfo.Offset = Offset;
			MinifigInfo.PieceData = PieceData;

			InfoArray.emplace_back(std::move(MinifigInfo));
		}
	}
}

void MinifigWizard::LoadTemplate(const QString& TemplateName)
{
	const std::map<QString, swMinifigTemplate>::iterator& TemplateIt = mTemplates.find(TemplateName);

	if (TemplateIt == mTemplates.end())
		return;

	const swMinifigTemplate& Template = TemplateIt->second;
	swPiecesLibrary* Library = swGetPiecesLibrary();

	for (int PartIndex = 0; PartIndex < SW_MFW_NUMITEMS; PartIndex++)
	{
		mMinifig.ColorIndices[PartIndex] = swGetColorIndex(Template.ColorCodes[PartIndex]);
		mMinifig.Angles[PartIndex] = Template.Angles[PartIndex];
		mMinifig.Matrices[PartIndex] = swMatrix44Identity();

		mMinifig.PieceData[PartIndex] = Library->AcquirePieceData(Template.Parts[PartIndex].toLatin1(), nullptr, false, false);

		if (mMinifig.PieceData[PartIndex])
			Library->RequestPieceDataLoad(mMinifig.PieceData[PartIndex].Get(), swLoadPriority::Normal);
	}

	std::vector<swPieceDataRef> RequiredPieceData;

	for (const swPieceDataRef& PieceData : mMinifig.PieceData)
		if (PieceData)
			RequiredPieceData.push_back(PieceData);

	Library->WaitForPieceData(RequiredPieceData);

	Calculate();
}

void MinifigWizard::SaveTemplate(const QString& TemplateName, const swMinifigTemplate& Template)
{
	mTemplates[TemplateName] = Template;
}

void MinifigWizard::DeleteTemplate(const QString& TemplateName)
{
	mTemplates.erase(TemplateName);
}

void MinifigWizard::AddTemplatesJson(const QByteArray& TemplateData)
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
		swMinifigTemplate Template;

		for (int PartIdx = 0; PartIdx < SW_MFW_NUMITEMS; PartIdx++)
		{
			QJsonObject PartObject = TemplateObject.value(QLatin1String(mSectionNames[PartIdx])).toObject();

			Template.Parts[PartIdx] = PartObject["Id"].toString();
			Template.ColorCodes[PartIdx] = PartObject["Color"].toInt();
			Template.Angles[PartIdx] = PartObject["Angle"].toDouble();
		}

		mTemplates.emplace(ElementIt.key(), std::move(Template));
	}
}

QByteArray MinifigWizard::GetTemplatesJson() const
{
	QJsonObject TemplatesObject;

	for (const auto& TemplateEntry : mTemplates)
	{
		const swMinifigTemplate& Template = TemplateEntry.second;
		QJsonObject TemplateObject;

		for (int PartIdx = 0; PartIdx < SW_MFW_NUMITEMS; PartIdx++)
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

void MinifigWizard::LoadTemplates()
{
	mTemplates.clear();

	QSettings Settings;
	Settings.beginGroup("Minifig");
	QByteArray TemplateData = Settings.value("Templates").toByteArray();

	AddTemplatesJson(TemplateData);
}

void MinifigWizard::SaveTemplates()
{
	QSettings Settings;
	Settings.beginGroup("Minifig");
	Settings.setValue("Templates", GetTemplatesJson());
}

void MinifigWizard::Calculate()
{
	float HeadOffset = 0.0f;
	swMatrix44 Root, Mat, Mat2;

	const swPieceDataRef* PieceData = mMinifig.PieceData;
	const float* Angles = mMinifig.Angles;
	swMatrix44* Matrices = mMinifig.Matrices;

	const bool DroidTorso = PieceData[SW_MFW_BODY] && !qstricmp(PieceData[SW_MFW_BODY]->mFileName, "30375.dat");
	const bool SkeletonTorso = PieceData[SW_MFW_BODY] && !qstricmp(PieceData[SW_MFW_BODY]->mFileName, "6260.dat");

	if (PieceData[SW_MFW_BODY3])
		Root = swMatrix44Translation(swVector3(0, 0, 74.0f));
	else
		Root = swMatrix44Translation(swVector3(0, 0, 72.0f));
	Matrices[SW_MFW_BODY] = swMul(mSettings[SW_MFW_BODY][GetSelectionIndex(SW_MFW_BODY)].Offset, Root);

	if (PieceData[SW_MFW_NECK])
	{
		Matrices[SW_MFW_NECK] = swMul(mSettings[SW_MFW_NECK][GetSelectionIndex(SW_MFW_NECK)].Offset, Root);
		HeadOffset = 0.08f;
	}

	if (PieceData[SW_MFW_HEAD])
	{
		Mat = swMatrix44RotationZ(-SW_DTOR * Angles[SW_MFW_HEAD]);
		Mat.SetTranslation(swVector3(0.0f, 0.0f, 24.0f + HeadOffset));
		Mat = swMul(mSettings[SW_MFW_HEAD][GetSelectionIndex(SW_MFW_HEAD)].Offset, Mat);
		Matrices[SW_MFW_HEAD] = swMul(Mat, Root);
	}

	if (PieceData[SW_MFW_HATS])
	{
		Mat = swMatrix44RotationZ(-SW_DTOR * Angles[SW_MFW_HATS]);
		Mat = swMul(mSettings[SW_MFW_HATS][GetSelectionIndex(SW_MFW_HATS)].Offset, Mat);
		Matrices[SW_MFW_HATS] = swMul(Mat, Matrices[SW_MFW_HEAD]);
	}

	if (PieceData[SW_MFW_HATS2])
	{
		Mat = swMatrix44RotationX(-SW_DTOR * Angles[SW_MFW_HATS2]);
		Mat = swMul(mSettings[SW_MFW_HATS2][GetSelectionIndex(SW_MFW_HATS2)].Offset, Mat);
		Matrices[SW_MFW_HATS2] = swMul(Mat, Matrices[SW_MFW_HATS]);
	}

	if (PieceData[SW_MFW_RARM])
	{
		Mat = swMatrix44RotationX(-SW_DTOR * Angles[SW_MFW_RARM]);

		if (DroidTorso || SkeletonTorso)
			Mat2 = swMatrix44Identity();
		else
			Mat2 = swMatrix44RotationY(-SW_DTOR * 9.791f);
		Mat2.SetTranslation(swVector3(15.552f, 0, -8.88f));

		Mat = swMul(mSettings[SW_MFW_RARM][GetSelectionIndex(SW_MFW_RARM)].Offset, Mat);
		Mat = swMul(Mat, Mat2);
		Matrices[SW_MFW_RARM] = swMul(Mat, Root);
	}

	if (PieceData[SW_MFW_RHAND])
	{
		Mat = swMatrix44RotationY(-SW_DTOR * Angles[SW_MFW_RHAND]);
		Mat2 = swMatrix44RotationX(SW_DTOR * 45);
		Mat = swMul(mSettings[SW_MFW_RHAND][GetSelectionIndex(SW_MFW_RHAND)].Offset, Mat);
		Mat = swMul(Mat, Mat2);
		Mat.SetTranslation(swVector3(5.0f, -10.0f, -19.0f));
		Matrices[SW_MFW_RHAND] = swMul(Mat, Matrices[SW_MFW_RARM]);
	}

	if (PieceData[SW_MFW_RHANDA])
	{
		Mat = swMatrix44RotationZ(SW_DTOR * Angles[SW_MFW_RHANDA]);
		Mat.SetTranslation(swVector3(0, -9.25f, 0));
		Mat = swMul(mSettings[SW_MFW_RHANDA][GetSelectionIndex(SW_MFW_RHANDA)].Offset, Mat);
		Mat = swMul(Mat, swMatrix44RotationX(SW_DTOR * 15.0f));
		Matrices[SW_MFW_RHANDA] = swMul(Mat, Matrices[SW_MFW_RHAND]);
	}

	if (PieceData[SW_MFW_LARM])
	{
		Mat = swMatrix44RotationX(-SW_DTOR * Angles[SW_MFW_LARM]);

		if (DroidTorso || SkeletonTorso)
			Mat2 = swMatrix44Identity();
		else
			Mat2 = swMatrix44RotationY(SW_DTOR * 9.791f);
		Mat2.SetTranslation(swVector3(-15.552f, 0.0f, -8.88f));

		Mat = swMul(mSettings[SW_MFW_LARM][GetSelectionIndex(SW_MFW_LARM)].Offset, Mat);
		Mat = swMul(Mat, Mat2);
		Matrices[SW_MFW_LARM] = swMul(Mat, Root);
	}

	if (PieceData[SW_MFW_LHAND])
	{
		Mat = swMatrix44RotationY(-SW_DTOR * Angles[SW_MFW_LHAND]);
		Mat2 = swMatrix44RotationX(SW_DTOR * 45);
		Mat = swMul(mSettings[SW_MFW_LHAND][GetSelectionIndex(SW_MFW_LHAND)].Offset, Mat);
		Mat = swMul(Mat, Mat2);
		Mat.SetTranslation(swVector3(-5.0f, -10.0f, -19.0f));
		Matrices[SW_MFW_LHAND] = swMul(Mat, Matrices[SW_MFW_LARM]);
	}

	if (PieceData[SW_MFW_LHANDA])
	{
		Mat = swMatrix44RotationZ(SW_DTOR * Angles[SW_MFW_LHANDA]);
		Mat.SetTranslation(swVector3(0, -9.25f, 0));
		Mat = swMul(mSettings[SW_MFW_LHANDA][GetSelectionIndex(SW_MFW_LHANDA)].Offset, Mat);
		Mat = swMul(Mat, swMatrix44RotationX(SW_DTOR * 15.0f));
		Matrices[SW_MFW_LHANDA] = swMul(Mat, Matrices[SW_MFW_LHAND]);
	}

	if (PieceData[SW_MFW_BODY2])
	{
		Mat = swMatrix44Identity();
		Mat.SetTranslation(swVector3(0, 0, -32.0f));
		Mat = swMul(mSettings[SW_MFW_BODY2][GetSelectionIndex(SW_MFW_BODY2)].Offset, Mat);
		Matrices[SW_MFW_BODY2] = swMul(Mat, Root);
	}

	if (PieceData[SW_MFW_BODY3])
	{
		Mat = swMatrix44Identity();
		Mat.SetTranslation(swVector3(0, 0, -32.0f));
		Mat = swMul(mSettings[SW_MFW_BODY3][GetSelectionIndex(SW_MFW_BODY3)].Offset, Mat);
		Matrices[SW_MFW_BODY3] = swMul(Mat, Root);
	}

	if (PieceData[SW_MFW_RLEG])
	{
		Mat = swMatrix44RotationX(-SW_DTOR * Angles[SW_MFW_RLEG]);
		Mat.SetTranslation(swVector3(0, 0, -44.0f));
		Mat = swMul(mSettings[SW_MFW_RLEG][GetSelectionIndex(SW_MFW_RLEG)].Offset, Mat);
		Matrices[SW_MFW_RLEG] = swMul(Mat, Root);
	}

	if (PieceData[SW_MFW_RLEGA])
	{
		const swVector3 Center(-10.0f, -1.0f, -28.0f);
		Mat = swMatrix44RotationZ(SW_DTOR * Angles[SW_MFW_RLEGA]);
		Mat2 = mSettings[SW_MFW_RLEGA][GetSelectionIndex(SW_MFW_RLEGA)].Offset;
		Mat2.SetTranslation(swMul31(-Center, Mat2));
		Mat = swMul(Mat2, Mat);
		Mat.SetTranslation(swMul31(Center, Mat2));
		Matrices[SW_MFW_RLEGA] = swMul(Mat, Matrices[SW_MFW_RLEG]);
	}

	if (PieceData[SW_MFW_LLEG])
	{
		Mat = swMatrix44RotationX(-SW_DTOR * Angles[SW_MFW_LLEG]);
		Mat.SetTranslation(swVector3(0, 0, -44.0f));
		Mat = swMul(mSettings[SW_MFW_LLEG][GetSelectionIndex(SW_MFW_LLEG)].Offset, Mat);
		Matrices[SW_MFW_LLEG] = swMul(Mat, Root);
	}

	if (PieceData[SW_MFW_LLEGA])
	{
		const swVector3 Center(10.0f, -1.0f, -28.0f);
		Mat = swMatrix44RotationZ(SW_DTOR * Angles[SW_MFW_LLEGA]);
		Mat2 = mSettings[SW_MFW_LLEGA][GetSelectionIndex(SW_MFW_LLEGA)].Offset;
		Mat2.SetTranslation(swMul31(-Center, Mat2));
		Mat = swMul(Mat2, Mat);
		Mat.SetTranslation(swMul31(Center, Mat2));
		Matrices[SW_MFW_LLEGA] = swMul(Mat, Matrices[SW_MFW_LLEG]);
	}

	mModel->SetMinifig(mMinifig);
}

int MinifigWizard::GetSelectionIndex(int Type) const
{
	const std::vector<swMinifigPieceInfo>& InfoArray = mSettings[Type];

	for (size_t Index = 0; Index < InfoArray.size(); Index++)
		if (InfoArray[Index].PieceData == mMinifig.PieceData[Type])
			return (int)Index;

	return 0;
}

void MinifigWizard::SetPieceData(int Type, swPieceData* PieceData)
{
	mMinifig.PieceData[Type] = swPieceDataRef(PieceData);

	if (mMinifig.PieceData[Type])
		swGetPiecesLibrary()->WaitForPieceData(mMinifig.PieceData[Type]);

	Calculate();
}

void MinifigWizard::SetColorIndex(int Type, int ColorIndex)
{
	mMinifig.ColorIndices[Type] = ColorIndex;

	Calculate();
}

void MinifigWizard::SetAngle(int Type, float Angle)
{
	mMinifig.Angles[Type] = Angle;

	Calculate();
}
