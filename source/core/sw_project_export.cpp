#include "sw_global.h"
#include "sw_math.h"
#include "sw_mesh.h"
#include "sw_widget_preview.h"
#include "sw_object_part.h"
#include "sw_data_part.h"
#include "sw_object_camera.h"
#include "sw_project.h"
#include "sw_instructions.h"
#include "sw_object_light.h"
#include "sw_window_main.h"
#include "sw_view.h"
#include "sw_library.h"
#include "sw_texture.h"
#include "sw_manager_thumbnail.h"
#include "sw_application.h"
#include "sw_profile.h"
#include "sw_dialog_render.h"
#include "sw_file.h"
#include "sw_file_zip.h"
#include "sw_dialog_list_model.h"
#include "sw_command_meta.h"
#include "sw_bricklink.h"
#include "sw_string.h"
#include "sw_colors.h"


swHTMLExportOptions::swHTMLExportOptions(const Project* Project)
{
	QString FileName = Project->GetFileName();

	if (!FileName.isEmpty())
		PathName = QFileInfo(FileName).canonicalPath();

	int ImageOptions = swGetProfileInteger(SW_PROFILE_HTML_IMAGE_OPTIONS);
	int HTMLOptions = swGetProfileInteger(SW_PROFILE_HTML_OPTIONS);

	TransparentImages = (ImageOptions & SW_IMAGE_TRANSPARENT) != 0;
	SubModels = (HTMLOptions & (SW_HTML_SUBMODELS)) != 0;
	CurrentOnly = (HTMLOptions & SW_HTML_CURRENT_ONLY) != 0;
	SinglePage = (HTMLOptions & SW_HTML_SINGLE_PAGE) != 0;
	IndexPage = (HTMLOptions & SW_HTML_INDEX) != 0;
	StepImagesWidth = swGetProfileInteger(SW_PROFILE_HTML_IMAGE_WIDTH);
	StepImagesHeight = swGetProfileInteger(SW_PROFILE_HTML_IMAGE_HEIGHT);
	PartsListStep = (HTMLOptions & SW_HTML_LIST_STEP) != 0;
	PartsListEnd = (HTMLOptions & SW_HTML_LIST_END) != 0;
}

void swHTMLExportOptions::SaveDefaults()
{
	int HTMLOptions = 0;

	if (SubModels)
		HTMLOptions |= SW_HTML_SUBMODELS;
	if (CurrentOnly)
		HTMLOptions |= SW_HTML_CURRENT_ONLY;
	if (SinglePage)
		HTMLOptions |= SW_HTML_SINGLE_PAGE;
	if (IndexPage)
		HTMLOptions |= SW_HTML_INDEX;
	if (PartsListStep)
		HTMLOptions |= SW_HTML_LIST_STEP;
	if (PartsListEnd)
		HTMLOptions |= SW_HTML_LIST_END;

	swSetProfileInteger(SW_PROFILE_HTML_IMAGE_OPTIONS, TransparentImages ? SW_IMAGE_TRANSPARENT : 0);
	swSetProfileInteger(SW_PROFILE_HTML_OPTIONS, HTMLOptions);
	swSetProfileInteger(SW_PROFILE_HTML_IMAGE_WIDTH, StepImagesWidth);
	swSetProfileInteger(SW_PROFILE_HTML_IMAGE_HEIGHT, StepImagesHeight);
}

swResult<void> Project::ExportCurrentStep(const QString& FileName)
{
	const swModel* Model = swGetActiveModel();
	const swResult<QByteArray> Bytes = SerializeModel(Model, false, Model->GetCurrentStep());

	if (!Bytes)
		return swUnexpected(Bytes.error());

	// the blender add-on reads leocad metas
	const QByteArray Output = swWriteCommandMeta(Bytes.value(), swCommandMetaFormat::LeoCAD);
	QSaveFile File(FileName);

	if (!File.open(QIODevice::WriteOnly) || File.write(Output) != Output.size() || !File.commit())
		return swUnexpected(tr("Error writing to file '%1':\n%2").arg(FileName, File.errorString()));

	swSetProfileString(SW_PROFILE_PROJECTS_PATH, QFileInfo(FileName).absolutePath());
	return swResult<void>();
}

swResult<void> Project::ExportModel(const QString& FileName, swModel* Model) const
{
	const swResult<QByteArray> Bytes = SerializeModel(Model, false, 0);

	if (!Bytes)
		return swUnexpected(Bytes.error());

	QSaveFile File(FileName);

	if (!File.open(QIODevice::WriteOnly) || File.write(Bytes.value()) != Bytes.value().size() || !File.commit())
		return swUnexpected(tr("Error writing to file '%1':\n%2").arg(FileName, File.errorString()));

	return swResult<void>();
}

QString Project::MakeExportNameFragment(const QString& Name)
{
	QString Identifier;
	bool AddSeparator = false;

	for (QChar Character : Name)
	{
		const ushort Value = Character.unicode();
		const bool IsAsciiLetter = (Value >= 'A' && Value <= 'Z') || (Value >= 'a' && Value <= 'z');
		const bool IsAsciiDigit = Value >= '0' && Value <= '9';

		if (IsAsciiLetter || IsAsciiDigit)
		{
			if (AddSeparator && !Identifier.isEmpty() && !Identifier.endsWith(QLatin1Char('_')))
				Identifier.append(QLatin1Char('_'));

			Identifier.append(Character);
			AddSeparator = false;
		}
		else
			AddSeparator = true;
	}

	if (Identifier.isEmpty())
		Identifier = QLatin1String("Unnamed");

	if (Identifier.size() > 80)
	{
		const QByteArray Hash = QCryptographicHash::hash(Name.toUtf8(), QCryptographicHash::Sha256).toHex();
		Identifier = Identifier.left(67) + QLatin1Char('_') + QString::fromLatin1(Hash.left(12));
	}

	return Identifier;
}

QString Project::MakeUniqueExportIdentifier(const QString& BaseName, std::set<QString>& UsedNames, const QString& ReservedSuffix)
{
	QString Identifier = BaseName;
	int Suffix = 2;

	while (UsedNames.find(Identifier) != UsedNames.end() ||
		(!ReservedSuffix.isEmpty() && UsedNames.find(Identifier + ReservedSuffix) != UsedNames.end()))
		Identifier = BaseName + QLatin1Char('_') + QString::number(Suffix++);

	UsedNames.insert(Identifier);

	if (!ReservedSuffix.isEmpty())
		UsedNames.insert(Identifier + ReservedSuffix);

	return Identifier;
}

QString Project::GetExportFileName(const QString& FileName, const QString& DefaultExtension, const QString& DialogTitle, const QString& DialogFilter) const
{
	if (!FileName.isEmpty())
		return FileName;

	QString SaveFileName;

	if (!mFileName.isEmpty())
		SaveFileName = mFileName;
	else
		SaveFileName = GetTitle();

	QString Extension = QFileInfo(SaveFileName).suffix().toLower();

	if (Extension.isEmpty())
		SaveFileName += "." + DefaultExtension;
	else if (Extension != DefaultExtension && SaveFileName.length() > 4)
	{
		SaveFileName = SaveFileName.left(SaveFileName.length() - Extension.length() - 1);
		SaveFileName += "." + DefaultExtension;
	}

	return QFileDialog::getSaveFileName(gMainWindow, DialogTitle, SaveFileName, DialogFilter);
}

swResult<void> Project::Export3DStudio(const QString& FileName)
{
	const std::vector<swModelPartsEntry> ModelParts = GetModelParts();

	QString SaveFileName = GetExportFileName(FileName, "3ds", tr("Export 3D Studio"), tr("3DS Files (*.3ds);;All Files (*.*)"));

	if (SaveFileName.isEmpty())
		return swUnexpected(QString());

	swDiskFile File(SaveFileName);

	if (!File.Open(QIODevice::WriteOnly))
		return swUnexpected(tr("Could not open file '%1' for writing.").arg(SaveFileName));

	long M3DStart = File.GetPosition();
	File.WriteU16(0x4D4D); // CHK_M3DMAGIC
	File.WriteU32(0);

	File.WriteU16(0x0002); // CHK_M3D_VERSION
	File.WriteU32(10);
	File.WriteU32(3);

	long MDataStart = File.GetPosition();
	File.WriteU16(0x3D3D); // CHK_MDATA
	File.WriteU32(0);

	File.WriteU16(0x3D3E); // CHK_MESH_VERSION
	File.WriteU32(10);
	File.WriteU32(3);

	const int MaterialNameLength = 11;
	char MaterialName[32];

	for (size_t ColorIdx = 0; ColorIdx < gColorList.size(); ColorIdx++)
	{
		swColor* Color = &gColorList[ColorIdx];

		snprintf(MaterialName, sizeof(MaterialName), "Material%03d", (int)ColorIdx);

		long MaterialStart = File.GetPosition();
		File.WriteU16(0xAFFF); // CHK_MAT_ENTRY
		File.WriteU32(0);

		File.WriteU16(0xA000); // CHK_MAT_NAME
		File.WriteU32(6 + MaterialNameLength + 1);
		File.WriteBuffer(MaterialName, MaterialNameLength + 1);

		File.WriteU16(0xA010); // CHK_MAT_AMBIENT
		File.WriteU32(24);

		File.WriteU16(0x0011); // CHK_COLOR_24
		File.WriteU32(9);

		File.WriteU8(floor(255.0 * Color->Value[0] + 0.5));
		File.WriteU8(floor(255.0 * Color->Value[1] + 0.5));
		File.WriteU8(floor(255.0 * Color->Value[2] + 0.5));

		File.WriteU16(0x0012); // CHK_LIN_COLOR_24
		File.WriteU32(9);

		File.WriteU8(floor(255.0 * Color->Value[0] + 0.5));
		File.WriteU8(floor(255.0 * Color->Value[1] + 0.5));
		File.WriteU8(floor(255.0 * Color->Value[2] + 0.5));

		File.WriteU16(0xA020); // CHK_MAT_AMBIENT
		File.WriteU32(24);

		File.WriteU16(0x0011); // CHK_COLOR_24
		File.WriteU32(9);

		File.WriteU8(floor(255.0 * Color->Value[0] + 0.5));
		File.WriteU8(floor(255.0 * Color->Value[1] + 0.5));
		File.WriteU8(floor(255.0 * Color->Value[2] + 0.5));

		File.WriteU16(0x0012); // CHK_LIN_COLOR_24
		File.WriteU32(9);

		File.WriteU8(floor(255.0 * Color->Value[0] + 0.5));
		File.WriteU8(floor(255.0 * Color->Value[1] + 0.5));
		File.WriteU8(floor(255.0 * Color->Value[2] + 0.5));

		File.WriteU16(0xA030); // CHK_MAT_SPECULAR
		File.WriteU32(24);

		File.WriteU16(0x0011); // CHK_COLOR_24
		File.WriteU32(9);

		File.WriteU8(floor(255.0 * 0.9f + 0.5));
		File.WriteU8(floor(255.0 * 0.9f + 0.5));
		File.WriteU8(floor(255.0 * 0.9f + 0.5));

		File.WriteU16(0x0012); // CHK_LIN_COLOR_24
		File.WriteU32(9);

		File.WriteU8(floor(255.0 * 0.9f + 0.5));
		File.WriteU8(floor(255.0 * 0.9f + 0.5));
		File.WriteU8(floor(255.0 * 0.9f + 0.5));

		File.WriteU16(0xA040); // CHK_MAT_SHININESS
		File.WriteU32(14);

		File.WriteU16(0x0030); // CHK_INT_PERCENTAGE
		File.WriteU32(8);

		File.WriteS16((quint8)floor(100.0 * 0.25 + 0.5));

		File.WriteU16(0xA041); // CHK_MAT_SHIN2PCT
		File.WriteU32(14);

		File.WriteU16(0x0030); // CHK_INT_PERCENTAGE
		File.WriteU32(8);

		File.WriteS16((quint8)floor(100.0 * 0.05 + 0.5));

		File.WriteU16(0xA050); // CHK_MAT_TRANSPARENCY
		File.WriteU32(14);

		File.WriteU16(0x0030); // CHK_INT_PERCENTAGE
		File.WriteU32(8);

		File.WriteS16((quint8)floor(100.0 * (1.0f - Color->Value[3]) + 0.5));

		File.WriteU16(0xA052); // CHK_MAT_XPFALL
		File.WriteU32(14);

		File.WriteU16(0x0030); // CHK_INT_PERCENTAGE
		File.WriteU32(8);

		File.WriteS16((quint8)floor(100.0 * 0.0 + 0.5));

		File.WriteU16(0xA053); // CHK_MAT_REFBLUR
		File.WriteU32(14);

		File.WriteU16(0x0030); // CHK_INT_PERCENTAGE
		File.WriteU32(8);

		File.WriteS16((quint8)floor(100.0 * 0.2 + 0.5));

		File.WriteU16(0xA100); // CHK_MAT_SHADING
		File.WriteU32(8);

		File.WriteS16(3);

		File.WriteU16(0xA084); // CHK_MAT_SELF_ILPCT
		File.WriteU32(14);

		File.WriteU16(0x0030); // CHK_INT_PERCENTAGE
		File.WriteU32(8);

		File.WriteS16((quint8)floor(100.0 * 0.0 + 0.5));

		File.WriteU16(0xA081); // CHK_MAT_TWO_SIDE
		File.WriteU32(6);

		File.WriteU16(0xA087); // CHK_MAT_WIRE_SIZE
		File.WriteU32(10);

		File.WriteFloat(1.0f);

		long MaterialEnd = File.GetPosition();
		File.Seek(MaterialStart + 2, SEEK_SET);
		File.WriteU32(MaterialEnd - MaterialStart);
		File.Seek(MaterialEnd, SEEK_SET);
	}

	const swModelProperties& Properties = mModels[0]->GetProperties();

	File.WriteU16(0x0100); // CHK_MASTER_SCALE
	File.WriteU32(10);

	File.WriteFloat(1.0f);

	File.WriteU16(0x1400); // CHK_LO_SHADOW_BIAS
	File.WriteU32(10);

	File.WriteFloat(1.0f);

	File.WriteU16(0x1420); // CHK_SHADOW_MAP_SIZE
	File.WriteU32(8);

	File.WriteS16(512);

	File.WriteU16(0x1450); // CHK_SHADOW_FILTER
	File.WriteU32(10);

	File.WriteFloat(3.0f);

	File.WriteU16(0x1460); // CHK_RAY_BIAS
	File.WriteU32(10);

	File.WriteFloat(1.0f);

	File.WriteU16(0x1500); // CHK_O_CONSTS
	File.WriteU32(18);

	File.WriteFloat(0.0f);
	File.WriteFloat(0.0f);
	File.WriteFloat(0.0f);

	File.WriteU16(0x2100); // CHK_AMBIENT_LIGHT
	File.WriteU32(42);

	File.WriteU16(0x0010); // CHK_COLOR_F
	File.WriteU32(18);

	File.WriteFloats(Properties.mAmbientColor.GetFloats(), 3);

	File.WriteU16(0x0013); // CHK_LIN_COLOR_F
	File.WriteU32(18);

	File.WriteFloats(Properties.mAmbientColor.GetFloats(), 3);

	File.WriteU16(0x1200); // CHK_SOLID_BGND
	File.WriteU32(42);

	File.WriteU16(0x0010); // CHK_COLOR_F
	File.WriteU32(18);

	const swPreferences& Preferences = swGetPreferences();
	swVector3 BackgroundSolidColor = swVector3FromColor(Preferences.mBackgroundSolidColor);

	File.WriteFloats(BackgroundSolidColor.GetFloats(), 3);

	File.WriteU16(0x0013); // CHK_LIN_COLOR_F
	File.WriteU32(18);

	File.WriteFloats(BackgroundSolidColor.GetFloats(), 3);

	File.WriteU16(0x1201); // LIB3DS_USE_SOLID_BGND
	File.WriteU32(6);

	int NumParts = 0;
	for (const swModelPartsEntry& ModelPart : ModelParts)
	{
		swMesh* Mesh = !ModelPart.Mesh ? ModelPart.PartData->GetMesh() : ModelPart.Mesh;

		if (!Mesh || Mesh->mIndexType == GL_UNSIGNED_INT)
			continue;

		long NamedObjectStart = File.GetPosition();
		File.WriteU16(0x4000); // CHK_NAMED_OBJECT
		File.WriteU32(0);

		char Name[32];
		snprintf(Name, sizeof(Name), "Part%.3d", NumParts);
		NumParts++;
		File.WriteBuffer(Name, strlen(Name) + 1);

		long TriObjectStart = File.GetPosition();
		File.WriteU16(0x4100); // CHK_N_TRI_OBJECT
		File.WriteU32(0);

		File.WriteU16(0x4110); // CHK_POINT_ARRAY
		File.WriteU32(8 + 12 * Mesh->mNumVertices);

		File.WriteU16(Mesh->mNumVertices);

		swVertex* Verts = (swVertex*)Mesh->mVertexData;
		const swMatrix44& ModelWorld = ModelPart.WorldMatrix;

		for (int VertexIdx = 0; VertexIdx < Mesh->mNumVertices; VertexIdx++)
		{
			swVector3 Pos = swMultiply31(Verts[VertexIdx].Position, ModelWorld);
			File.WriteFloat(Pos[0]);
			File.WriteFloat(Pos[1]);
			File.WriteFloat(Pos[2]);
		}

		File.WriteU16(0x4160); // CHK_MESH_MATRIX
		File.WriteU32(54);

		swMatrix44 Matrix = swMatrix44Identity();
		File.WriteFloats(Matrix[0].GetFloats(), 3);
		File.WriteFloats(Matrix[1].GetFloats(), 3);
		File.WriteFloats(Matrix[2].GetFloats(), 3);
		File.WriteFloats(Matrix[3].GetFloats(), 3);

		File.WriteU16(0x4165); // CHK_MESH_COLOR
		File.WriteU32(7);

		File.WriteU8(0);

		long FaceArrayStart = File.GetPosition();
		File.WriteU16(0x4120); // CHK_FACE_ARRAY
		File.WriteU32(0);

		int NumTriangles = 0;

		for (int SectionIdx = 0; SectionIdx < Mesh->mLods[SW_MESH_LOD_HIGH].NumSections; SectionIdx++)
		{
			swMeshSection* Section = &Mesh->mLods[SW_MESH_LOD_HIGH].Sections[SectionIdx];

			if (Section->PrimitiveType != SW_MESH_TRIANGLES && Section->PrimitiveType != SW_MESH_TEXTURED_TRIANGLES)
				continue;

			NumTriangles += Section->DrawCount / 3;
		}

		File.WriteU16(NumTriangles);

		for (int SectionIdx = 0; SectionIdx < Mesh->mLods[SW_MESH_LOD_HIGH].NumSections; SectionIdx++)
		{
			swMeshSection* Section = &Mesh->mLods[SW_MESH_LOD_HIGH].Sections[SectionIdx];

			if (Section->PrimitiveType != SW_MESH_TRIANGLES && Section->PrimitiveType != SW_MESH_TEXTURED_TRIANGLES)
				continue;

			quint16* Indices = (quint16*)Mesh->mIndexData + Section->DrawOffset / sizeof(quint16);

			for (int IndexIdx = 0; IndexIdx < Section->DrawCount; IndexIdx += 3)
			{
				File.WriteU16(Indices[IndexIdx + 0]);
				File.WriteU16(Indices[IndexIdx + 1]);
				File.WriteU16(Indices[IndexIdx + 2]);
				File.WriteU16(7);
			}
		}

		NumTriangles = 0;

		for (int SectionIdx = 0; SectionIdx < Mesh->mLods[SW_MESH_LOD_HIGH].NumSections; SectionIdx++)
		{
			swMeshSection* Section = &Mesh->mLods[SW_MESH_LOD_HIGH].Sections[SectionIdx];

			if (Section->PrimitiveType != SW_MESH_TRIANGLES && Section->PrimitiveType != SW_MESH_TEXTURED_TRIANGLES)
				continue;

			int MaterialIndex = Section->ColorIndex == gDefaultColor ? ModelPart.ColorIndex : Section->ColorIndex;

			File.WriteU16(0x4130); // CHK_MSH_MAT_GROUP
			File.WriteU32(6 + MaterialNameLength + 1 + 2 + 2 * Section->DrawCount / 3);

			snprintf(MaterialName, sizeof(MaterialName), "Material%03d", MaterialIndex);
			File.WriteBuffer(MaterialName, MaterialNameLength + 1);

			File.WriteU16(Section->DrawCount / 3);

			for (int IndexIdx = 0; IndexIdx < Section->DrawCount; IndexIdx += 3)
				File.WriteU16(NumTriangles++);
		}

		long FaceArrayEnd = File.GetPosition();
		File.Seek(FaceArrayStart + 2, SEEK_SET);
		File.WriteU32(FaceArrayEnd - FaceArrayStart);
		File.Seek(FaceArrayEnd, SEEK_SET);

		long TriObjectEnd = File.GetPosition();
		File.Seek(TriObjectStart + 2, SEEK_SET);
		File.WriteU32(TriObjectEnd - TriObjectStart);
		File.Seek(TriObjectEnd, SEEK_SET);

		long NamedObjectEnd = File.GetPosition();
		File.Seek(NamedObjectStart + 2, SEEK_SET);
		File.WriteU32(NamedObjectEnd - NamedObjectStart);
		File.Seek(NamedObjectEnd, SEEK_SET);
	}

	long MDataEnd = File.GetPosition();
	File.Seek(MDataStart + 2, SEEK_SET);
	File.WriteU32(MDataEnd - MDataStart);
	File.Seek(MDataEnd, SEEK_SET);

	long KFDataStart = File.GetPosition();
	File.WriteU16(0xB000); // CHK_KFDATA
	File.WriteU32(0);

	File.WriteU16(0xB00A); // LIB3DS_KFHDR
	File.WriteU32(6 + 2 + 1 + 4);

	File.WriteS16(5);
	File.WriteU8(0);
	File.WriteS32(100);

	long KFDataEnd = File.GetPosition();
	File.Seek(KFDataStart + 2, SEEK_SET);
	File.WriteU32(KFDataEnd - KFDataStart);
	File.Seek(KFDataEnd, SEEK_SET);

	long M3DEnd = File.GetPosition();
	File.Seek(M3DStart + 2, SEEK_SET);
	File.WriteU32(M3DEnd - M3DStart);
	File.Seek(M3DEnd, SEEK_SET);

	return swResult<void>();
}

swResult<void> Project::ExportBrickLink(const QString& FileName)
{
	swPartsList PartsList;

	if (!mModels.empty())
		mModels[0]->GetPartsList(gDefaultColor, true, false, PartsList);

	if (PartsList.empty())
		return swUnexpected(tr("Nothing to export."));

	QString SaveFileName = GetExportFileName(FileName, "xml", tr("Export BrickLink"), tr("XML Files (*.xml);;All Files (*.*)"));

	if (SaveFileName.isEmpty())
		return swUnexpected(QString());

	return swExportBrickLink(SaveFileName, PartsList);
}

swResult<void> Project::ExportCOLLADA(const QString& FileName)
{
	const std::vector<swModelPartsEntry> ModelParts = GetModelParts();

	QString SaveFileName = GetExportFileName(FileName, "dae", tr("Export COLLADA"), tr("COLLADA Files (*.dae);;All Files (*.*)"));

	if (SaveFileName.isEmpty())
		return swUnexpected(QString());

	QFile File(SaveFileName);

	if (!File.open(QIODevice::WriteOnly))
		return swUnexpected(tr("Could not open file '%1' for writing.").arg(SaveFileName));

	QTextStream Stream(&File);

	Stream << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n";
	Stream << "<COLLADA xmlns=\"http://www.collada.org/2005/11/COLLADASchema\" version=\"1.4.1\">\r\n";

	Stream << "<asset>\r\n";
	Stream << "\t<created>" << QDateTime::currentDateTime().toString(Qt::ISODate) << "</created>\r\n";
	Stream << "\t<modified>" << QDateTime::currentDateTime().toString(Qt::ISODate) << "</modified>\r\n";
	Stream << "<unit name=\"StudWorx\" meter=\"0.0004\" />\r\n";
	Stream << "\t<up_axis>Z_UP</up_axis>\r\n";
	Stream << "</asset>\r\n";

	std::vector<QString> ColorIDs(gColorList.size());
	std::set<QString> UsedColorIDs;

	for (size_t ColorIndex = 0; ColorIndex < gColorList.size(); ColorIndex++)
	{
		const QString BaseName = QLatin1String("Color_") + MakeExportNameFragment(QString::fromLatin1(gColorList[ColorIndex].SafeName));
		QString Identifier = BaseName;

		if (UsedColorIDs.find(Identifier) != UsedColorIDs.end())
			Identifier += QLatin1Char('_') + QString::number(ColorIndex);

		ColorIDs[ColorIndex] = MakeUniqueExportIdentifier(Identifier, UsedColorIDs, QString());
	}

	Stream << "<library_effects>\r\n";

	for (size_t ColorIndex = 0; ColorIndex < gColorList.size(); ColorIndex++)
	{
		const swColor& Color = gColorList[ColorIndex];
		const QString& ColorName = ColorIDs[ColorIndex];

		Stream << QString("\t<effect id=\"%1-phong\">\r\n").arg(ColorName);
		Stream << "\t\t<profile_COMMON>\r\n";
		Stream << "\t\t\t<technique sid=\"phong1\">\r\n";
		Stream << "\t\t\t\t<phong>\r\n";
		Stream << "\t\t\t\t\t<emission>\r\n";
		Stream << "\t\t\t\t\t\t<color>0.0 0.0 0.0 0.0</color>\r\n";
		Stream << "\t\t\t\t\t</emission>\r\n";
		Stream << "\t\t\t\t\t<ambient>\r\n";
		Stream << QString("\t\t\t\t\t\t<color>%1 %2 %3 1.0</color>\r\n").arg(QString::number(Color.Value[0]), QString::number(Color.Value[1]), QString::number(Color.Value[2]));
		Stream << "\t\t\t\t\t</ambient>\r\n";
		Stream << "\t\t\t\t\t<diffuse>\r\n";
		Stream << QString("\t\t\t\t\t\t<color>%1 %2 %3 1.0</color>\r\n").arg(QString::number(Color.Value[0]), QString::number(Color.Value[1]), QString::number(Color.Value[2]));
		Stream << "\t\t\t\t\t</diffuse>\r\n";
		Stream << "\t\t\t\t\t<specular>\r\n";
		Stream << "\t\t\t\t\t\t<color>0.9 0.9 0.9 1.0</color>\r\n";
		Stream << "\t\t\t\t\t</specular>\r\n";
		Stream << "\t\t\t\t\t<shininess>\r\n";
		Stream << "\t\t\t\t\t\t<float>20.0</float>\r\n";
		Stream << "\t\t\t\t\t</shininess>\r\n";
		Stream << "\t\t\t\t\t<transparent>\r\n";
		Stream << QString("\t\t\t\t\t\t<color>%1 %2 %3 %4</color>\r\n").arg(QString::number(Color.Value[0]), QString::number(Color.Value[1]), QString::number(Color.Value[2]), QString::number(Color.Value[3]));
		Stream << "\t\t\t\t\t</transparent>\r\n";
		Stream << "\t\t\t\t\t<transparency>\r\n";
		Stream << "\t\t\t\t\t\t<float>1.0</float>\r\n";
		Stream << "\t\t\t\t\t</transparency>\r\n";
		Stream << "\t\t\t\t</phong>\r\n";
		Stream << "\t\t\t</technique>\r\n";
		Stream << "\t\t</profile_COMMON>\r\n";
		Stream << "\t</effect>\r\n";
	}

	Stream << "</library_effects>\r\n";
	Stream << "<library_materials>\r\n";

	for (size_t ColorIndex = 0; ColorIndex < gColorList.size(); ColorIndex++)
	{
		const QString& ColorName = ColorIDs[ColorIndex];
		Stream << QString("\t<material id=\"%1-material\">\r\n").arg(ColorName);
		Stream << QString("\t\t<instance_effect url=\"#%1-phong\" />\r\n").arg(ColorName);
		Stream << "\t</material>\r\n";
	}

	Stream << "</library_materials>\r\n";
	Stream << "<library_geometries>\r\n";
	std::map<const swMesh*, QString> MeshIDs;
	std::set<QString> UsedMeshIDs;

	for (const swModelPartsEntry& ModelPart : ModelParts)
	{
		swMesh* Mesh = !ModelPart.Mesh ? ModelPart.PartData->GetMesh() : ModelPart.Mesh;

		if (!Mesh)
			continue;

		if (MeshIDs.find(Mesh) != MeshIDs.end())
			continue;

		const QString BaseName = QLatin1String("Part_") + MakeExportNameFragment(QString::fromUtf8(ModelPart.PartData->mFileName));
		const QString ID = MakeUniqueExportIdentifier(BaseName, UsedMeshIDs, QString());
		MeshIDs.emplace(Mesh, ID);
		const int NumVertices = Mesh->mNumVertices + Mesh->mNumTexturedVertices;

		Stream << QString("\t<geometry id=\"%1\">\r\n").arg(ID);
		Stream << "\t\t<mesh>\r\n";

		Stream << QString("\t\t\t<source id=\"%1-pos\">\r\n").arg(ID);
		Stream << QString("\t\t\t\t<float_array id=\"%1-pos-array\" count=\"%2\">\r\n").arg(ID, QString::number(NumVertices * 3));

		const swVertex* Verts = static_cast<const swVertex*>(Mesh->mVertexData);

		for (int VertexIdx = 0; VertexIdx < Mesh->mNumVertices; VertexIdx++)
		{
			const swVector3& Position = Verts[VertexIdx].Position;
			Stream << QString("\t\t\t\t\t%1 %2 %3\r\n").arg(QString::number(Position.x), QString::number(Position.y), QString::number(Position.z));
		}

		const swVertexTextured* TexturedVerts = reinterpret_cast<const swVertexTextured*>(Verts + Mesh->mNumVertices);

		for (int VertexIdx = 0; VertexIdx < Mesh->mNumTexturedVertices; VertexIdx++)
		{
			const swVector3& Position = TexturedVerts[VertexIdx].Position;
			Stream << QString("\t\t\t\t\t%1 %2 %3\r\n").arg(QString::number(Position.x), QString::number(Position.y), QString::number(Position.z));
		}

		Stream << "\t\t\t\t</float_array>\r\n";
		Stream << "\t\t\t\t<technique_common>\r\n";
		Stream << QString("\t\t\t\t\t<accessor source=\"#%1-pos-array\" count=\"%2\" stride=\"3\">\r\n").arg(ID, QString::number(NumVertices));
		Stream << "\t\t\t\t\t\t<param name=\"X\" type=\"float\" />\r\n";
		Stream << "\t\t\t\t\t\t<param name=\"Y\" type=\"float\" />\r\n";
		Stream << "\t\t\t\t\t\t<param name=\"Z\" type=\"float\" />\r\n";
		Stream << "\t\t\t\t\t</accessor>\r\n";
		Stream << "\t\t\t\t</technique_common>\r\n";
		Stream << "\t\t\t</source>\r\n";

		Stream << QString("\t\t\t<source id=\"%1-normal\">\r\n").arg(ID);
		Stream << QString("\t\t\t\t<float_array id=\"%1-normal-array\" count=\"%2\">\r\n").arg(ID, QString::number(NumVertices * 3));

		for (int VertexIdx = 0; VertexIdx < Mesh->mNumVertices; VertexIdx++)
		{
			swVector3 Normal = swUnpackNormal(Verts[VertexIdx].Normal);
			Stream << QString("\t\t\t\t\t%1 %2 %3\r\n").arg(QString::number(Normal.x), QString::number(Normal.y), QString::number(Normal.z));
		}

		for (int VertexIdx = 0; VertexIdx < Mesh->mNumTexturedVertices; VertexIdx++)
		{
			swVector3 Normal = swUnpackNormal(TexturedVerts[VertexIdx].Normal);
			Stream << QString("\t\t\t\t\t%1 %2 %3\r\n").arg(QString::number(Normal.x), QString::number(Normal.y), QString::number(Normal.z));
		}

		Stream << "\t\t\t\t</float_array>\r\n";
		Stream << "\t\t\t\t<technique_common>\r\n";
		Stream << QString("\t\t\t\t\t<accessor source=\"#%1-normal-array\" count=\"%2\" stride=\"3\">\r\n").arg(ID, QString::number(NumVertices));
		Stream << "\t\t\t\t\t\t<param name=\"X\" type=\"float\" />\r\n";
		Stream << "\t\t\t\t\t\t<param name=\"Y\" type=\"float\" />\r\n";
		Stream << "\t\t\t\t\t\t<param name=\"Z\" type=\"float\" />\r\n";
		Stream << "\t\t\t\t\t</accessor>\r\n";
		Stream << "\t\t\t\t</technique_common>\r\n";
		Stream << "\t\t\t</source>\r\n";

		Stream << QString("\t\t\t<vertices id=\"%1-vertices\">\r\n").arg(ID);
		Stream << QString("\t\t\t\t<input semantic=\"POSITION\" source=\"#%1-pos\"/>\r\n").arg(ID);
		Stream << "\t\t\t</vertices>\r\n";

		for (int SectionIdx = 0; SectionIdx < Mesh->mLods[SW_MESH_LOD_HIGH].NumSections; SectionIdx++)
		{
			swMeshSection* Section = &Mesh->mLods[SW_MESH_LOD_HIGH].Sections[SectionIdx];
			int Offset = 0;

			if (Section->PrimitiveType == SW_MESH_TRIANGLES)
				Offset = 0;
			else if (Section->PrimitiveType == SW_MESH_TEXTURED_TRIANGLES)
				Offset = Mesh->mNumVertices;
			else
				continue;

			const QString& ColorName = ColorIDs[Section->ColorIndex];

			if (Mesh->mIndexType == GL_UNSIGNED_SHORT)
			{
				quint16* Indices = (quint16*)Mesh->mIndexData + Section->DrawOffset / sizeof(quint16);

				Stream << QString("\t\t\t<triangles count=\"%1\" material=\"%2\">\r\n").arg(QString::number(Section->DrawCount / 3), ColorName);
				Stream << QString("\t\t\t<input semantic=\"VERTEX\" source=\"#%1-vertices\" offset=\"0\" />\r\n").arg(ID);
				Stream << QString("\t\t\t<input semantic=\"NORMAL\" source=\"#%1-normal\" offset=\"0\" />\r\n").arg(ID);
				Stream << "\t\t\t<p>\r\n";

				for (int Idx = 0; Idx < Section->DrawCount; Idx += 3)
				{
					QString idx1 = QString::number(Indices[Idx + 0] + Offset);
					QString idx2 = QString::number(Indices[Idx + 1] + Offset);
					QString idx3 = QString::number(Indices[Idx + 2] + Offset);

					Stream << QString("\t\t\t\t %1 %2 %3\r\n").arg(idx1, idx2, idx3);
				}
			}
			else
			{
				quint32* Indices = (quint32*)Mesh->mIndexData + Section->DrawOffset / sizeof(quint32);

				Stream << QString("\t\t\t<triangles count=\"%1\" material=\"%2\">\r\n").arg(QString::number(Section->DrawCount / 3), ColorName);
				Stream << QString("\t\t\t<input semantic=\"VERTEX\" source=\"#%1-vertices\" offset=\"0\" />\r\n").arg(ID);
				Stream << QString("\t\t\t<input semantic=\"NORMAL\" source=\"#%1-normal\" offset=\"0\" />\r\n").arg(ID);
				Stream << "\t\t\t<p>\r\n";

				for (int Idx = 0; Idx < Section->DrawCount; Idx += 3)
				{
					QString idx1 = QString::number(Indices[Idx + 0] + Offset);
					QString idx2 = QString::number(Indices[Idx + 1] + Offset);
					QString idx3 = QString::number(Indices[Idx + 2] + Offset);

					Stream << QString("\t\t\t\t %1 %2 %3\r\n").arg(idx1, idx2, idx3);
				}
			}

			Stream << "\t\t\t\t</p>\r\n";
			Stream << "\t\t\t</triangles>\r\n";
		}

		Stream << "\t\t</mesh>\r\n";
		Stream << "\t</geometry>\r\n";
	}

	Stream << "</library_geometries>\r\n";
	Stream << "<library_visual_scenes>\r\n";
	Stream << "\t<visual_scene id=\"DefaultScene\">\r\n";

	for (const swModelPartsEntry& ModelPart : ModelParts)
	{
		swMesh* Mesh = !ModelPart.Mesh ? ModelPart.PartData->GetMesh() : ModelPart.Mesh;

		if (!Mesh)
			continue;

		const QString& ID = MeshIDs.at(Mesh);

		Stream << "\t\t<node>\r\n";
		Stream << "\t\t\t<matrix>\r\n";

		const swMatrix44& Matrix = ModelPart.WorldMatrix;
		Stream << QString("\t\t\t\t%1 %2 %3 %4\r\n").arg(QString::number(Matrix[0][0]), QString::number(Matrix[1][0]), QString::number(Matrix[2][0]), QString::number(Matrix[3][0]));
		Stream << QString("\t\t\t\t%1 %2 %3 %4\r\n").arg(QString::number(Matrix[0][1]), QString::number(Matrix[1][1]), QString::number(Matrix[2][1]), QString::number(Matrix[3][1]));
		Stream << QString("\t\t\t\t%1 %2 %3 %4\r\n").arg(QString::number(Matrix[0][2]), QString::number(Matrix[1][2]), QString::number(Matrix[2][2]), QString::number(Matrix[3][2]));
		Stream << QString("\t\t\t\t%1 %2 %3 %4\r\n").arg(QString::number(Matrix[0][3]), QString::number(Matrix[1][3]), QString::number(Matrix[2][3]), QString::number(Matrix[3][3]));

		Stream << "\t\t\t</matrix>\r\n";
		Stream << QString("\t\t\t<instance_geometry url=\"#%1\">\r\n").arg(ID);
		Stream << "\t\t\t\t<bind_material>\r\n";
		Stream << "\t\t\t\t\t<technique_common>\r\n";

		for (int SectionIdx = 0; SectionIdx < Mesh->mLods[SW_MESH_LOD_HIGH].NumSections; SectionIdx++)
		{
			swMeshSection* Section = &Mesh->mLods[SW_MESH_LOD_HIGH].Sections[SectionIdx];

			if (Section->PrimitiveType != SW_MESH_TRIANGLES && Section->PrimitiveType != SW_MESH_TEXTURED_TRIANGLES)
				continue;

			const QString& SourceColorName = ColorIDs[Section->ColorIndex];
			const QString* TargetColorName;
			if (Section->ColorIndex == gDefaultColor)
				TargetColorName = &ColorIDs[ModelPart.ColorIndex];
			else
				TargetColorName = &SourceColorName;

			Stream << QString("\t\t\t\t\t\t<instance_material symbol=\"%1\" target=\"#%2-material\"/>\r\n").arg(SourceColorName, *TargetColorName);
		}

		Stream << "\t\t\t\t\t</technique_common>\r\n";
		Stream << "\t\t\t\t</bind_material>\r\n";
		Stream << "\t\t\t</instance_geometry>\r\n";
		Stream << "\t\t</node>\r\n";
	}

	Stream << "\t</visual_scene>\r\n";
	Stream << "</library_visual_scenes>\r\n";
	Stream << "<scene>\r\n";
	Stream << "\t<instance_visual_scene url=\"#DefaultScene\"/>\r\n";
	Stream << "</scene>\r\n";

	Stream << "</COLLADA>\r\n";

	return swResult<void>();
}

swResult<void> Project::ExportCSV(const QString& FileName)
{
	swPartsList PartsList;

	if (!mModels.empty())
		mModels[0]->GetPartsList(gDefaultColor, true, false, PartsList);

	if (PartsList.empty())
		return swUnexpected(tr("Nothing to export."));

	QString SaveFileName = GetExportFileName(FileName, "csv", tr("Export CSV"), tr("CSV Files (*.csv);;All Files (*.*)"));

	if (SaveFileName.isEmpty())
		return swUnexpected(QString());

	return swExportCSV(SaveFileName, PartsList);
}

swResult<void> swExportCSV(const QString& FileName, const swPartsList& PartsList)
{
	swDiskFile CSVFile(FileName);
	char Line[1024];

	if (!CSVFile.Open(QIODevice::WriteOnly))
		return swUnexpected(QCoreApplication::translate("Project", "Could not open file '%1' for writing.").arg(FileName));

	CSVFile.WriteLine("Part Name,Color,Quantity,Part ID,Color Code\n");

	for (const auto& PartIt : PartsList)
	{
		const swPartData* PartData = PartIt.first;

		for (const auto& ColorIt : PartIt.second)
		{
			std::string Description = PartData->m_strDescription;
			Description.erase(std::remove(Description.begin(), Description.end(), ','), Description.end());

			snprintf(Line, sizeof(Line), "\"%s\",\"%s\",%d,%s,%d\n", Description.c_str(), gColorList[ColorIt.first].Name, ColorIt.second, PartData->mFileName, gColorList[ColorIt.first].Code);
			CSVFile.WriteLine(Line);
		}
	}

	return swResult<void>();
}

swResult<void> Project::ExportHTML(const swHTMLExportOptions& Options)
{
	std::set<swModel*> Models;

	if (Options.CurrentOnly)
		Models.insert(mActiveModel);
	else if (Options.SubModels)
	{
		Models.insert(mActiveModel);
		mActiveModel->GetSubModels(Models);
	}
	else
	{
		for (const std::unique_ptr<swModel>& Model : mModels)
			Models.insert(Model.get());
	}

	for (const swModel* Model : Models)
		Model->WaitForAssets();

	QDir Dir(Options.PathName);
	Dir.mkpath(QLatin1String("."));

	const QString ProjectTitle = GetTitle();
	auto MakeSafeFileName = [](const QString& Name)
	{
		const QByteArray Utf8Name = Name.toUtf8();
		QString SafeName;
		SafeName.reserve(Utf8Name.size() * 3);

		for (char Byte : Utf8Name)
		{
			const unsigned char Value = static_cast<unsigned char>(Byte);

			if ((Value >= 'A' && Value <= 'Z') || (Value >= 'a' && Value <= 'z') || (Value >= '0' && Value <= '9') || Value == '-')
				SafeName.append(QLatin1Char(Value));
			else
				SafeName.append(QString("_%1").arg(Value, 2, 16, QLatin1Char('0')));
		}

		if (SafeName.isEmpty())
			return QString("Model");

		if (SafeName.size() > 80)
		{
			const QByteArray Digest = QCryptographicHash::hash(Utf8Name, QCryptographicHash::Sha256).toHex();
			SafeName = SafeName.left(60) + '-' + QString::fromLatin1(Digest.left(16));
		}

		// windows reserves device names
		const QString UpperName = SafeName.toUpper();
		const bool NumberedDevice = UpperName.size() == 4 && (UpperName.startsWith(QLatin1String("COM")) || UpperName.startsWith(QLatin1String("LPT"))) &&
			UpperName[3] >= QLatin1Char('1') && UpperName[3] <= QLatin1Char('9');

		if (UpperName == QLatin1String("CON") || UpperName == QLatin1String("PRN") || UpperName == QLatin1String("AUX") || UpperName == QLatin1String("NUL") || NumberedDevice)
			SafeName.prepend('_');

		return SafeName;
	};

	const QString ProjectBaseName = MakeSafeFileName(QFileInfo(ProjectTitle).completeBaseName());
	std::map<const swModel*, QString> ModelBaseNames;
	int ModelOrdinal = 0;

	for (const std::unique_ptr<swModel>& Model : mModels)
	{
		if (Models.find(Model.get()) == Models.end())
			continue;

		QString BaseName = ProjectBaseName;

		if (Models.size() > 1)
			BaseName += QString("-%1-%2").arg(++ModelOrdinal).arg(MakeSafeFileName(Model->GetProperties().mFileName));

		ModelBaseNames.emplace(Model.get(), std::move(BaseName));
	}

	auto AddPartsListImage = [&Dir](QTextStream& Stream, swModel* Model, swStep Step, const QString& BaseName) -> swResult<void>
	{
		swPartsList PartsList;

		if (Step == 0)
			Model->GetPartsList(gDefaultColor, true, false, PartsList);
		else
			Model->GetPartsListForStep(Step, gDefaultColor, PartsList, false);

		if (PartsList.empty())
			return swResult<void>();

		const swResult<QImage> Image = Model->GetPartsListImage(1024, 512, Step, SW_RGBA(255, 255, 255, 0), SW_RGBA(255, 255, 255, 0), QFont("Arial", 16), Qt::black, true);

		if (!Image)
			return swUnexpected(Image.error());

		QString ImageName = BaseName + QLatin1String("-parts.png");
		QString FileName = QFileInfo(Dir, ImageName).absoluteFilePath();
		QImageWriter Writer(FileName);

		if (!Writer.write(Image.value()))
			return swUnexpected(tr("Error writing to file '%1':\n%2").arg(FileName, Writer.errorString()));

		Stream << QString::fromLatin1("<p><IMG SRC=\"%1\" /></p><br><br>\r\n").arg(ImageName.toHtmlEscaped());

		return swResult<void>();
	};

	auto WriteHeader = [](QTextStream& Stream, const QString& Title)
	{
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
		Stream.setCodec("UTF-8");
#endif
		Stream << QString::fromLatin1("<HTML>\r\n<HEAD>\r\n<META CHARSET=\"UTF-8\">\r\n<TITLE>%1</TITLE>\r\n</HEAD>\r\n<BR>\r\n<CENTER>\r\n").arg(Title.toHtmlEscaped());
	};

	const QString CreatedBy = tr("Created by %1").toHtmlEscaped().arg(QLatin1String("<A HREF=\"https://github.com/nathaneltitane/studworx\">StudWorx</A>"));

	for (swModel* Model : Models)
	{
		const QString& BaseName = ModelBaseNames.at(Model);
		swStep LastStep = Model->GetLastStep();
		QString PageTitle;

		if (Models.size() > 1)
			PageTitle = Model->GetProperties().mFileName;
		else
			PageTitle = ProjectTitle;

		if (Options.SinglePage)
		{
			QString FileName = QFileInfo(Dir, BaseName + QLatin1String(".html")).absoluteFilePath();
			QFile File(FileName);

			if (!File.open(QIODevice::WriteOnly))
				return swUnexpected(tr("Error writing to file '%1':\n%2").arg(FileName, File.errorString()));

			QTextStream Stream(&File);

			WriteHeader(Stream, tr("Instructions for %1").arg(PageTitle));

			for (swStep Step = 1; Step <= LastStep; Step++)
			{
				QString StepString = QString::fromLatin1("%1").arg(Step, 2, 10, QLatin1Char('0'));
				Stream << QString::fromLatin1("<p><IMG SRC=\"%1-%2.png\" ALT=\"%3\" WIDTH=%4 HEIGHT=%5></p><BR><BR>\r\n").arg(BaseName.toHtmlEscaped(), StepString, tr("Step %1").arg(StepString).toHtmlEscaped(), QString::number(Options.StepImagesWidth), QString::number(Options.StepImagesHeight));

				if (Options.PartsListStep)
				{
					const swResult<void> PartsImageSaved = AddPartsListImage(Stream, Model, Step, QString("%1-%2").arg(BaseName, StepString));

					if (!PartsImageSaved)
						return PartsImageSaved;
				}
			}

			if (Options.PartsListEnd)
			{
				const swResult<void> PartsImageSaved = AddPartsListImage(Stream, Model, 0, BaseName);

				if (!PartsImageSaved)
					return PartsImageSaved;
			}

			Stream << QString::fromLatin1("</CENTER>\r\n<BR><HR><BR><B><I>%1</I></B><BR></HTML>\r\n").arg(CreatedBy);
		}
		else
		{
			if (Options.IndexPage)
			{
				QString FileName = QFileInfo(Dir, BaseName + QLatin1String("-index.html")).absoluteFilePath();
				QFile File(FileName);

				if (!File.open(QIODevice::WriteOnly))
					return swUnexpected(tr("Error writing to file '%1':\n%2").arg(FileName, File.errorString()));

				QTextStream Stream(&File);

				WriteHeader(Stream, tr("Instructions for %1").arg(PageTitle));

				for (swStep Step = 1; Step <= LastStep; Step++)
					Stream << QString::fromLatin1("<A HREF=\"%1-%2.html\">%3<BR>\r\n</A>").arg(BaseName, QString("%1").arg(Step, 2, 10, QLatin1Char('0')), tr("Step %1").arg(QString::number(Step)).toHtmlEscaped());

				if (Options.PartsListEnd)
					Stream << QString::fromLatin1("<A HREF=\"%1-parts.html\">%2</A><BR>\r\n").arg(BaseName, tr("Parts Used").toHtmlEscaped());

				Stream << QString::fromLatin1("</CENTER>\r\n<BR><HR><BR><B><I>%1</I></B><BR></HTML>\r\n").arg(CreatedBy);
			}

			for (swStep Step = 1; Step <= LastStep; Step++)
			{
				QString StepString = QString::fromLatin1("%1").arg(Step, 2, 10, QLatin1Char('0'));
				QString FileName = QFileInfo(Dir, QString("%1-%2.html").arg(BaseName, StepString)).absoluteFilePath();
				QFile File(FileName);

				if (!File.open(QIODevice::WriteOnly))
					return swUnexpected(tr("Error writing to file '%1':\n%2").arg(FileName, File.errorString()));

				QTextStream Stream(&File);

				WriteHeader(Stream, tr("%1 - Step %2").arg(PageTitle, QString::number(Step)));
				Stream << QString::fromLatin1("<IMG SRC=\"%1-%2.png\" ALT=\"%3\" WIDTH=%4 HEIGHT=%5><BR><BR>\r\n").arg(BaseName.toHtmlEscaped(), StepString, tr("Step %1").arg(StepString).toHtmlEscaped(), QString::number(Options.StepImagesWidth), QString::number(Options.StepImagesHeight));

				if (Options.PartsListStep)
				{
					const swResult<void> PartsImageSaved = AddPartsListImage(Stream, Model, Step, QString("%1-%2").arg(BaseName, StepString));

					if (!PartsImageSaved)
						return PartsImageSaved;
				}

				Stream << QLatin1String("</CENTER>\r\n<BR><HR><BR>");
				if (Step != 1)
					Stream << QString::fromLatin1("<A HREF=\"%1-%2.html\">%3</A> ").arg(BaseName, QString("%1").arg(Step - 1, 2, 10, QLatin1Char('0')), tr("Previous").toHtmlEscaped());

				if (Options.IndexPage)
					Stream << QString::fromLatin1("<A HREF=\"%1-index.html\">%2</A> ").arg(BaseName, tr("Index").toHtmlEscaped());

				if (Step != LastStep)
					Stream << QString::fromLatin1("<A HREF=\"%1-%2.html\">%3</A>").arg(BaseName, QString("%1").arg(Step + 1, 2, 10, QLatin1Char('0')), tr("Next").toHtmlEscaped());
				else if (Options.PartsListEnd)
					Stream << QString::fromLatin1("<A HREF=\"%1-parts.html\">%2</A>").arg(BaseName, tr("Parts Used").toHtmlEscaped());

				Stream << QLatin1String("<BR></HTML>\r\n");
			}

			if (Options.PartsListEnd)
			{
				QString FileName = QFileInfo(Dir, BaseName + QLatin1String("-parts.html")).absoluteFilePath();
				QFile File(FileName);

				if (!File.open(QIODevice::WriteOnly))
					return swUnexpected(tr("Error writing to file '%1':\n%2").arg(FileName, File.errorString()));

				QTextStream Stream(&File);

				WriteHeader(Stream, tr("Parts used by %1").arg(PageTitle));

				const swResult<void> PartsImageSaved = AddPartsListImage(Stream, Model, 0, BaseName);

				if (!PartsImageSaved)
					return PartsImageSaved;

				Stream << QLatin1String("</CENTER>\r\n<BR><HR><BR>");
				Stream << QString::fromLatin1("<A HREF=\"%1-%2.html\">%3</A> ").arg(BaseName, QString("%1").arg(LastStep, 2, 10, QLatin1Char('0')), tr("Previous").toHtmlEscaped());

				if (Options.IndexPage)
					Stream << QString::fromLatin1("<A HREF=\"%1-index.html\">%2</A> ").arg(BaseName, tr("Index").toHtmlEscaped());

				Stream << QLatin1String("<BR></HTML>\r\n");
			}
		}

		QString StepImageBaseName = QFileInfo(Dir, BaseName + QLatin1String("-%1.png")).absoluteFilePath();
		const swResult<void> ImagesSaved = Model->SaveStepImages(StepImageBaseName, true, false, Options.StepImagesWidth, Options.StepImagesHeight, 1, LastStep, Options.TransparentImages ? swImageBackground::Transparent : swImageBackground::Opaque);

		if (!ImagesSaved)
			return ImagesSaved;
	}

	if (Models.size() > 1)
	{
		QString FileName = QFileInfo(Dir, ProjectBaseName + QLatin1String("-index.html")).absoluteFilePath();
		QFile File(FileName);

		if (!File.open(QIODevice::WriteOnly))
			return swUnexpected(tr("Error writing to file '%1':\n%2").arg(FileName, File.errorString()));

		QTextStream Stream(&File);

		WriteHeader(Stream, tr("Instructions for %1").arg(ProjectTitle));

		for (const swModel* Model : Models)
		{
			const QString& BaseName = ModelBaseNames.at(Model);

			if (Options.SinglePage)
			{
				FileName = BaseName + QLatin1String(".html");
				Stream << QString::fromLatin1("<p><a href=\"%1\">%2</a>").arg(FileName.toHtmlEscaped(), Model->GetProperties().mFileName.toHtmlEscaped());
			}
			else if (Options.IndexPage)
			{
				FileName = BaseName + QLatin1String("-index.html");
				Stream << QString::fromLatin1("<p><a href=\"%1\">%2</a>").arg(FileName.toHtmlEscaped(), Model->GetProperties().mFileName.toHtmlEscaped());
			}
			else
			{
				swStep LastStep = Model->GetLastStep();

				Stream << QString::fromLatin1("<p>%1</p>").arg(Model->GetProperties().mFileName.toHtmlEscaped());

				for (swStep Step = 1; Step <= LastStep; Step++)
					Stream << QString::fromLatin1("<A HREF=\"%1-%2.html\">%3<BR>\r\n</A>").arg(BaseName, QString("%1").arg(Step, 2, 10, QLatin1Char('0')), tr("Step %1").arg(QString::number(Step)).toHtmlEscaped());

				if (Options.PartsListEnd)
					Stream << QString::fromLatin1("<A HREF=\"%1-parts.html\">%2</A><BR>\r\n").arg(BaseName, tr("Parts Used").toHtmlEscaped());
			}
		}

		Stream << QString::fromLatin1("</CENTER>\r\n<BR><HR><BR><B><I>%1</I></B><BR></HTML>\r\n").arg(CreatedBy);
	}

	return swResult<void>();
}

swResult<void> Project::ExportPOVRay(const QString& FileName, const swCamera* ExportCamera, swStep Step, bool Transparent)
{
	const std::vector<swModelPartsEntry> ModelParts = GetModelParts(Step);

	QString SaveFileName = GetExportFileName(FileName, QLatin1String("pov"), tr("Export POV-Ray"), tr("POV-Ray Files (*.pov);;All Files (*.*)"));

	if (SaveFileName.isEmpty())
		return swUnexpected(QString());

	swDiskFile POVFile(SaveFileName);

	if (!POVFile.Open(QIODevice::WriteOnly))
		return swUnexpected(tr("Could not open file '%1' for writing.").arg(SaveFileName));

	enum
	{
		LGEO_PART_LGEO  = 0x01,
		LGEO_PART_AR    = 0x02,
		LGEO_PART_SLOPE = 0x04
	};

	enum
	{
		LGEO_COLOR_SOLID       = 0x01,
		LGEO_COLOR_TRANSPARENT = 0x02,
		LGEO_COLOR_CHROME      = 0x04,
		LGEO_COLOR_PEARL       = 0x08,
		LGEO_COLOR_METALLIC    = 0x10,
		LGEO_COLOR_RUBBER      = 0x20,
		LGEO_COLOR_GLITTER     = 0x40
	};

	QString Line;

	Line = QString::asprintf("// Generated By: StudWorx %s\n// LDraw File: %s\n// Date: %s\n\n",
			SW_VERSION_TEXT,
			mModels[0]->GetProperties().mFileName.toUtf8().constData(),
			QDateTime::currentDateTime().toString(Qt::ISODate).toLatin1().constData());
	POVFile.WriteLine(Line.toUtf8().constData());

	POVFile.WriteLine("#version 3.7;\n\n");

	POVFile.WriteLine("global_settings { assumed_gamma 1.0 }\n\n");

	swPartsLibrary* Library = swGetPartsLibrary();
	std::map<const swPartData*, std::pair<char[SW_PART_NAME_LENGTH + 1], int>> PartTable;
	size_t NumColors = gColorList.size();
	std::vector<std::array<char, SW_MAXIMUM_COLOR_NAME + 3>> ColorTable(NumColors);

	const std::vector<std::unique_ptr<swLight>>& Lights = mActiveModel->GetLights();
	const swCamera* Camera = ExportCamera ? ExportCamera : gMainWindow->GetActiveView()->GetCamera();
	auto MakeIdentifier = [](const QString& Name, const QString& Fallback)
	{
		QString Identifier;

		for (QChar Character : Name)
		{
			const ushort Value = Character.unicode();

			if ((Value >= 'A' && Value <= 'Z') || (Value >= 'a' && Value <= 'z') || (Value >= '0' && Value <= '9') || Value == '_')
				Identifier.append(Character);
			else
				Identifier.append('_');
		}

		return Identifier.isEmpty() ? Fallback : Identifier.left(48);
	};

	const QString CameraName = MakeIdentifier(QString(Camera->GetName()), QLatin1String("Camera"));
	const swVector3& Position = Camera->mPosition;
	const swVector3& Target = Camera->mTargetPosition;
	const swVector3& Up = Camera->mUpVector;
	const swVector3 BackgroundColor = swVector3FromColor(swGetPreferences().mBackgroundSolidColor);
	const swPOVRayOptions& POVRayOptions = mModels[0]->GetPOVRayOptions();
	const QString TopModelName = QLatin1String("SW_") + MakeExportNameFragment(QString(mModels[0]->GetFileName()));
	const bool WantsLGEO = POVRayOptions.UseLGEO || swGetProfileInteger(SW_PROFILE_RENDER_PARTS) == 1;
	const QString LGEOPath = WantsLGEO ? swFindLGEOPath() : QString();
	const bool UseLGEO = WantsLGEO && !LGEOPath.isEmpty();
	const int TopModelColorCode = 7;

	swVector3 Min(FLT_MAX, FLT_MAX, FLT_MAX);
	swVector3 Max(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	if (ModelParts.empty())
	{
		Min = swVector3(-10.0f, -10.0f, -10.0f);
		Max = swVector3(10.0f, 10.0f, 10.0f);
	}

	for (const swModelPartsEntry& ModelPart : ModelParts)
	{
		swVector3 Points[8];

		const swMesh* Mesh = ModelPart.Mesh ? ModelPart.Mesh : ModelPart.PartData->GetMesh();
		swGetBoxCorners(Mesh->mBoundingBox, Points);

		for (int PointIdx = 0; PointIdx < 8; PointIdx++)
		{
			swVector3 Point = swMultiply31(Points[PointIdx], ModelPart.WorldMatrix);

			Min = swMinimum(Point, Min);
			Max = swMaximum(Point, Max);
		}
	}

	swVector3 Center = (Min + Max) / 2.0f;
	float Radius = (Max - Center).Length() / 25.0f;
	Center = swVector3(Center[1], Center[0], Center[2]) / 25.0f;

	const bool AreaLights = std::any_of(Lights.begin(), Lights.end(), [](const std::unique_ptr<swLight>& Light) { return Light->GetLightType() == swLightType::Area; });

	// area lit scenes get a white floor
	const swVector3 FloorColor = POVRayOptions.FloorColor.value_or(AreaLights ? swVector3(1.0f, 1.0f, 1.0f) : swVector3(0.8f, 0.8f, 0.8f));
	const float FloorAmbient = POVRayOptions.FloorAmbient.value_or(AreaLights ? 0.0f : 0.4f);
	const float FloorDiffuse = POVRayOptions.FloorDiffuse.value_or(AreaLights ? 0.9f : 0.4f);

	// ldraw xyz to pov-ray yzx
	const char* FloorAxis = POVRayOptions.FloorAxis == 0 ? "y" : POVRayOptions.FloorAxis == 2 ? "x" : "z";
	const char* FloorLocation = POVRayOptions.FloorAxis == 0 ? "MinY" : POVRayOptions.FloorAxis == 2 ? "MinX" : "MinZ";

	const int RenderQuality = static_cast<int>(swGetProfileRenderQuality());
	const int LightRig = std::clamp(swGetProfileInteger(SW_PROFILE_RENDER_LIGHT_RIG), 0, static_cast<int>(swPOVRayLightRig::Count) - 1);
	const int FloorStyle = std::clamp(swGetProfileInteger(SW_PROFILE_RENDER_FLOOR_STYLE), 0, static_cast<int>(swPOVRayFloorStyle::Count) - 1);
	const QString EnvironmentImage = swGetProfileString(SW_PROFILE_RENDER_ENVIRONMENT_IMAGE);
	Transparent = Transparent || swGetProfileInteger(SW_PROFILE_RENDER_TRANSPARENT) != 0;

	if (!POVRayOptions.HeaderIncludeFile.isEmpty())
	{
		Line = QString::asprintf("#include \"%s\"\n\n", POVRayOptions.HeaderIncludeFile.toUtf8().constData());
		POVFile.WriteLine(Line.toUtf8().constData());
	}

	Line = QString::asprintf(
			"#ifndef (MinX) #declare MinX = %g; #end\n"
			"#ifndef (MinY) #declare MinY = %g; #end\n"
			"#ifndef (MinZ) #declare MinZ = %g; #end\n"
			"#ifndef (MaxX) #declare MaxX = %g; #end\n"
			"#ifndef (MaxY) #declare MaxY = %g; #end\n"
			"#ifndef (MaxZ) #declare MaxZ = %g; #end\n"
			"#ifndef (CenterX) #declare CenterX = %g; #end\n"
			"#ifndef (CenterY) #declare CenterY = %g; #end\n"
			"#ifndef (CenterZ) #declare CenterZ = %g; #end\n"
			"#ifndef (Center) #declare Center = <CenterX,CenterY,CenterZ>; #end\n"
			"#ifndef (Radius) #declare Radius = %g; #end\n",
			Min[1] / 25.0f, Min[0] / 25.0f, Min[2] / 25.0f, Max[1] / 25.0f, Max[0] / 25.0f, Max[2] / 25.0f, Center[0], Center[1], Center[2], Radius);
	POVFile.WriteLine(Line.toUtf8().constData());
	Line = QString::asprintf(
			"#ifndef (CameraSky) #declare CameraSky = <%g,%g,%g>; #end\n"
			"#ifndef (CameraLocation) #declare CameraLocation = <%g, %g, %g>; #end\n"
			"#ifndef (CameraTarget) #declare CameraTarget = <%g, %g, %g>; #end\n"
			"#ifndef (CameraAngle) #declare CameraAngle = %g; #end\n"
			"#ifndef (CameraOrthographic) #declare CameraOrthographic = %s; #end\n"
			"#ifndef (CameraHeight) #declare CameraHeight = %g; #end\n",
			Up[1], Up[0], Up[2], Position[1] / 25.0f, Position[0] / 25.0f, Position[2] / 25.0f, Target[1] / 25.0f, Target[0] / 25.0f, Target[2] / 25.0f, Camera->m_fovy,
			Camera->GetProjection() == swCameraProjection::Orthographic ? "true" : "false", Camera->GetOrthoHeight() / 25.0f);
	POVFile.WriteLine(Line.toUtf8().constData());
	Line = QString::asprintf(
			"#ifndef (BackgroundColor) #declare BackgroundColor = <%1g, %1g, %1g>; #end\n"
			"#ifndef (Background) #declare Background = %s; #end\n",
			BackgroundColor[0], BackgroundColor[1], BackgroundColor[2], (POVRayOptions.ExcludeBackground || Transparent ? "false" : "true"));
	POVFile.WriteLine(Line.toUtf8().constData());
	Line = QString::asprintf(
			"#ifndef (FloorAxis) #declare FloorAxis = %s; #end\n"
			"#ifndef (FloorLocation) #declare FloorLocation = %s; #end\n"
			"#ifndef (FloorColor) #declare FloorColor = <%1g, %1g, %1g>; #end\n"
			"#ifndef (FloorAmbient) #declare FloorAmbient = %1g; #end\n"
			"#ifndef (FloorDiffuse) #declare FloorDiffuse = %1g; #end\n"
			"#ifndef (Floor) #declare Floor = %s; #end\n"
			"#ifndef (FloorStyle) #declare FloorStyle = %i; #end\n",
			FloorAxis, FloorLocation, FloorColor[0], FloorColor[1], FloorColor[2], FloorAmbient, FloorDiffuse, (POVRayOptions.ExcludeFloor || Transparent ? "false" : "true"), FloorStyle);
	POVFile.WriteLine(Line.toUtf8().constData());

	// quality sets radiosity and light sampling
	POVFile.WriteLine(QString::fromLatin1(
		"#ifndef (RenderQuality) #declare RenderQuality = %1; #end\n"
		"#ifndef (Radiosity) #declare Radiosity = (RenderQuality > 0); #end\n"
		"#ifndef (LightRig) #declare LightRig = %2; #end\n"
		"#ifndef (AreaSamples) #declare AreaSamples = select(RenderQuality - 1, 2, 3, 5); #end\n"
		"#ifndef (FocalBlur) #declare FocalBlur = (RenderQuality > 1 & !CameraOrthographic); #end\n"
		"#ifndef (FocalAperture) #declare FocalAperture = vlength(CameraLocation - CameraTarget) * 0.01; #end\n\n"
		"#if (Radiosity)\n"
		"  global_settings {\n"
		"    radiosity {\n"
		"      #if (RenderQuality > 1)\n"
		"        pretrace_start 0.08 pretrace_end 0.004 count 400 nearest_count 8 error_bound 0.5 recursion_limit 2 low_error_factor 0.5 gray_threshold 0 brightness 1\n"
		"      #else\n"
		"        pretrace_start 0.08 pretrace_end 0.02 count 60 nearest_count 5 error_bound 1.5 recursion_limit 1 low_error_factor 0.5 gray_threshold 0 brightness 1\n"
		"      #end\n"
		"    }\n"
		"  }\n"
		"#end\n\n").arg(RenderQuality).arg(LightRig).toLatin1().constData());
	Line = QString::asprintf(
			"#ifndef (Ambient) #declare Ambient = 0.4; #end\n"
			"#ifndef (Diffuse) #declare Diffuse = 0.4; #end\n"
			"#ifndef (Reflection) #declare Reflection = 0.08; #end\n"
			"#ifndef (Phong) #declare Phong = 0.5; #end\n"
			"#ifndef (PhongSize) #declare PhongSize = 40; #end\n"
			"#ifndef (TransReflection) #declare TransReflection = 0.2; #end\n"
			"#ifndef (TransFilter) #declare TransFilter = 0.85; #end\n"
			"#ifndef (ReflectionMax) #declare ReflectionMax = 0.5; #end\n"
			"#ifndef (OpaqueIoR) #declare OpaqueIoR = 1.5; #end\n"
			"#ifndef (TransIoR) #declare TransIoR = 1.5; #end\n"
			"#ifndef (TransFade) #declare TransFade = 3; #end\n"
			"#ifndef (PearlIridescence) #declare PearlIridescence = 0.12; #end\n"
			"#ifndef (MetalReflection) #declare MetalReflection = 0.3; #end\n"
			"#ifndef (FlakeReflection) #declare FlakeReflection = 0.6; #end\n"
			"#ifndef (Glow) #declare Glow = 1; #end\n"
			"#ifndef (RubberReflection) #declare RubberReflection = 0; #end\n"
			"#ifndef (RubberPhong) #declare RubberPhong = 0.1; #end\n"
			"#ifndef (RubberPhongS) #declare RubberPhongS = 10; #end\n"
			"#ifndef (ChromeReflection) #declare ChromeReflection = 0.85; #end\n"
			"#ifndef (ChromeBrilliance) #declare ChromeBrilliance = 5; #end\n"
			"#ifndef (ChromeSpecular) #declare ChromeSpecular = 0.8; #end\n"
			"#ifndef (ChromeRough) #declare ChromeRough = 0.01; #end\n"
			"#ifndef (OpaqueNormal) #declare OpaqueNormal = normal { bumps 0.001 scale 0.5 }; #end\n"
			"#ifndef (TransNormal) #declare TransNormal = normal { bumps 0.001 scale 0.5 }; #end\n");
	POVFile.WriteLine(Line.toUtf8().constData());
	Line = QString::asprintf(
			"#ifndef (Quality) #declare Quality = 3; #end\n"
			"#ifndef (Studs) #declare Studs = 1; #end\n"
			"#ifndef (LgeoLibrary) #declare LgeoLibrary = %s; #end\n"
			"#ifndef (ModelReflection) #declare ModelReflection = %i; #end\n"
			"#ifndef (ModelShadow) #declare ModelShadow = %i; #end\n\n",
			(UseLGEO ? "true" : "false"), (POVRayOptions.NoReflection ? 0 : 1), (POVRayOptions.NoShadow ? 0 : 1));
	POVFile.WriteLine(Line.toUtf8().constData());

	Line = QString::asprintf(
			"#ifndef (SkipWriteLightMacro)\n"
			"#macro WriteLight(Type, Shadowless, Location, Target, Color, Power, FadeDistance, FadePower, SpotRadius, SpotFalloff, SpotTightness, AreaCircle, AreaWidth, AreaHeight, AreaRows, AreaColumns)\n"
			"  #local PointLight = %i;\n"
			"  #local Spotlight = %i;\n"
			"  #local DirectionalLight = %i;\n"
			"  #local AreaLight = %i;\n"
			"  light_source {\n"
			"    Location\n"
			"    color rgb Color*Power\n"
			"    #if (Shadowless > 0)\n"
			"      shadowless\n"
			"    #end\n"
			"    #if (FadeDistance > 0)\n"
			"    fade_distance FadeDistance\n"
			"    #end\n"
			"    #if (FadePower > 0)\n"
			"    fade_power FadePower\n"
			"    #end\n"
			"    #if (Type = Spotlight)\n"
			"      spotlight\n"
			"      radius SpotRadius\n"
			"      falloff SpotFalloff\n"
			"      tightness SpotTightness\n"
			"      point_at Target\n"
			"    #elseif (Type = DirectionalLight)\n"
			"      parallel\n"
			"      point_at Target\n"
			"    #elseif (Type = AreaLight)\n"
			"      area_light AreaWidth, AreaHeight, AreaRows, AreaColumns\n"
			"      jitter\n"
			"      #if (AreaCircle > 0 & AreaRows > 1 & AreaColumns > 1)\n"
			"        circular \n"
			"        #if (vlength(AreaWidth) = vlength(AreaHeight) & AreaRows = AreaColumns)\n"
			"          orient\n"
			"        #end\n"
			"      #end\n"
			"    #end\n"
			"  }\n"
			"#end\n"
			"#end\n\n",
			static_cast<int>(swLightType::Point), static_cast<int>(swLightType::Spot), static_cast<int>(swLightType::Directional), static_cast<int>(swLightType::Area));
	POVFile.WriteLine(Line.toUtf8().constData());

	// wrap color finishes in materials
	POVFile.WriteLine(
		"#ifndef (SkipColorMacros)\n"
		"#macro OpaqueTexture(r, g, b, E)\n"
		"  texture {\n"
		"    pigment { srgb <r,g,b> }\n"
		"    finish { emission E ambient Ambient diffuse Diffuse phong Phong phong_size PhongSize reflection { Reflection / 2, ReflectionMax fresnel on } conserve_energy }\n"
		"    normal { OpaqueNormal }\n"
		"  }\n"
		"#end\n\n"
		"#macro TranslucentTexture(r, g, b, f, E)\n"
		"  texture {\n"
		"    pigment { srgbf <r,g,b,f> }\n"
		"    finish { emission E ambient Ambient diffuse Diffuse phong Phong phong_size PhongSize reflection { TransReflection / 4, 1 fresnel on } conserve_energy }\n"
		"    normal { TransNormal }\n"
		"  }\n"
		"#end\n\n"
		"#macro ChromeTexture(r, g, b, E)\n"
		"  texture {\n"
		"    pigment { srgb <r,g,b> }\n"
		"    finish { emission E ambient Ambient diffuse Diffuse / 4 brilliance ChromeBrilliance metallic specular ChromeSpecular roughness ChromeRough reflection { ChromeReflection metallic } }\n"
		"  }\n"
		"#end\n\n"
		"#macro PearlTexture(r, g, b, E)\n"
		"  texture {\n"
		"    pigment { srgb <r,g,b> }\n"
		"    finish { emission E ambient Ambient diffuse Diffuse brilliance 2 phong Phong phong_size PhongSize / 2 reflection { Reflection / 2, ReflectionMax fresnel on } irid { PearlIridescence thickness 0.4 turbulence 0.5 } conserve_energy }\n"
		"  }\n"
		"#end\n\n"
		"#macro MetalTexture(r, g, b, E)\n"
		"  texture {\n"
		"    pigment { srgb <r,g,b> }\n"
		"    finish { emission E ambient Ambient diffuse Diffuse * 0.7 brilliance 3 metallic specular 0.6 roughness 0.03 reflection { MetalReflection metallic } }\n"
		"    normal { granite 0.04 scale 0.02 }\n"
		"  }\n"
		"#end\n\n"
		"#macro RubberTexture(r, g, b, E)\n"
		"  texture {\n"
		"    pigment { srgb <r,g,b> }\n"
		"    finish { emission E ambient Ambient diffuse Diffuse phong RubberPhong phong_size RubberPhongS reflection RubberReflection }\n"
		"  }\n"
		"#end\n\n"
		"#macro FlakeTexture(r, g, b, f, Fraction, Size, fr, fg, fb, E)\n"
		"  #if (f > 0)\n"
		"    #local Base = TranslucentTexture(r, g, b, f, E)\n"
		"  #else\n"
		"    #local Base = OpaqueTexture(r, g, b, E)\n"
		"  #end\n"
		"  #local Flake = texture {\n"
		"    pigment { srgb <fr,fg,fb> }\n"
		"    finish { emission E ambient Ambient diffuse Diffuse brilliance 4 metallic specular 0.9 roughness 0.005 reflection { FlakeReflection metallic } }\n"
		"  }\n"
		"  #local Edge = 1 - Fraction;\n"
		"  texture {\n"
		"    cells\n"
		"    texture_map { [0 Base] [Edge Base] [Edge Flake] [1 Flake] }\n"
		"    scale Size / 25\n"
		"  }\n"
		"#end\n\n"
		"#macro OpaqueMaterial(T)\n"
		"  material { texture { T } interior { ior OpaqueIoR } }\n"
		"#end\n\n"
		"#macro TranslucentMaterial(T, r, g, b)\n"
		"  material { texture { T } interior { ior TransIoR fade_distance TransFade fade_power 1001 fade_color srgb <r,g,b> } }\n"
		"#end\n"
		"#end\n\n");

	// an environment image lights the scene
	if (!EnvironmentImage.isEmpty())
	{
		const QString Suffix = QFileInfo(EnvironmentImage).suffix().toLower();
		const QLatin1String ImageType = Suffix == QLatin1String("hdr") ? QLatin1String("hdr") : Suffix == QLatin1String("exr") ? QLatin1String("exr") : Suffix == QLatin1String("png") ? QLatin1String("png") : Suffix == QLatin1String("tif") || Suffix == QLatin1String("tiff") ? QLatin1String("tiff") : QLatin1String("jpeg");

		POVFile.WriteLine(QString::fromLatin1(
			"#ifndef (EnvironmentImage) #declare EnvironmentImage = \"%1\"; #end\n"
			"#ifndef (EnvironmentRotation) #declare EnvironmentRotation = 0; #end\n"
			"#ifndef (EnvironmentPower) #declare EnvironmentPower = 1; #end\n\n"
			"#if (Background | Radiosity)\n"
			"  sky_sphere {\n"
			"    pigment { image_map { %2 EnvironmentImage map_type 1 interpolate 2 } rotate x * 90 rotate z * EnvironmentRotation }\n"
			"    emission EnvironmentPower\n"
			"  }\n"
			"#end\n\n").arg(QDir::fromNativeSeparators(EnvironmentImage), ImageType).toUtf8().constData());
	}
	else
		POVFile.WriteLine("#if (Background)\n  background {\n    color rgb BackgroundColor\n  }\n#end\n\n");

	// match the view height or angle
	POVFile.WriteLine(QString(
		"#ifndef (Skip%1)\n"
		"  camera {\n"
		"    #if (CameraOrthographic)\n"
		"      orthographic\n"
		"      right x * CameraHeight * image_width / image_height\n"
		"      up y * CameraHeight\n"
		"    #else\n"
		"      perspective\n"
		"      right x * image_width / image_height\n"
		"    #end\n"
		"    sky CameraSky\n"
		"    location CameraLocation\n"
		"    look_at CameraTarget\n"
		"    #if (!CameraOrthographic)\n"
		"      angle 2 * degrees(atan(tan(radians(CameraAngle) / 2) * image_width / image_height))\n"
		"    #end\n"
		"    #if (FocalBlur)\n"
		"      aperture FocalAperture\n"
		"      focal_point CameraTarget\n"
		"      blur_samples 8, 64\n"
		"      confidence 0.95\n"
		"      variance 1 / 256\n"
		"    #end\n"
		"  }\n"
		"#end\n\n").arg(CameraName.isEmpty() ? QLatin1String("Camera") : CameraName).toLatin1().constData());

	swVector3 AreaX(1.0f, 0.0f, 0.0f), AreaY(0.0f, 1.0f, 0.0f);
	swVector2i AreaGrid(1, 1);
	int AreaCircle = 0, Shadowless = 0;
	swLightType LightType = swLightType::Area;
	float Power = 0.0f, FadeDistance = 0.0f, FadePower = 0.0f, SpotRadius = 0.0f, SpotFalloff = 0.0f, SpotTightness = 0.0f;

	// place a light rig for unlit models
	if (Lights.empty())
	{
		POVFile.WriteLine(
			"#ifndef (SkipLightRig)\n"
			"#local RigFront = <CameraLocation.x - CenterX, CameraLocation.y - CenterY, 0>;\n"
			"#if (vlength(RigFront) < 0.001) #local RigFront = <0, -1, 0>; #end\n"
			"#local RigFront = vnormalize(RigFront);\n"
			"#local RigSide = vnormalize(vcross(z, RigFront));\n"
			"#local RigSize = max(Radius, 0.5);\n\n"
			"#macro RigAreaLight(Location, Power, Size, Shadowless)\n"
			"  #local LightFront = vnormalize(Center - Location);\n"
			"  #local LightSide = vnormalize(vcross(z, LightFront));\n"
			"  #local LightUp = vcross(LightFront, LightSide);\n"
			"  WriteLight(3, Shadowless, Location, Center, <1, 1, 1>, Power, 0, 0, 0, 0, 0, 1, LightSide * Size, LightUp * Size, AreaSamples, AreaSamples)\n"
			"#end\n\n"
			"#if (LightRig = 1)\n"
			"  RigAreaLight(Center + (RigFront * 2 + RigSide * 1.6 + z * 2.2) * RigSize, 1.2, RigSize * 1.5, 0)\n"
			"  RigAreaLight(Center + (RigFront * 2 - RigSide * 2.2 + z * 1) * RigSize, 0.5, RigSize * 2.5, 1)\n"
			"  RigAreaLight(Center + (-RigFront * 2.5 + RigSide * 0.5 + z * 2.5) * RigSize, 0.8, RigSize, 0)\n"
			"#elseif (LightRig = 2)\n"
			"  RigAreaLight(Center + (RigFront * 0.5 + z * 4) * RigSize, 1.1, RigSize * 4, 0)\n"
			"  WriteLight(0, 1, CameraLocation, Center, <1, 1, 1>, 0.6, 0, 0, 0, 0, 0, 0, x, y, 1, 1)\n"
			"#else\n"
			"  WriteLight(0, 0, Center + (RigFront * 1.5 + RigSide * 1.2 + z * 2) * RigSize, Center, <1, 1, 1>, 1, 0, 0, 0, 0, 0, 0, x, y, 1, 1)\n"
			"  WriteLight(0, 0, Center + (RigFront * 1.5 - RigSide * 1.5 + z * 1) * RigSize, Center, <1, 1, 1>, 0.65, 0, 0, 0, 0, 0, 0, x, y, 1, 1)\n"
			"  WriteLight(0, 0, Center + (-RigFront * 2 + RigSide * 0.5 + z * 2) * RigSize, Center, <1, 1, 1>, 0.6, 0, 0, 0, 0, 0, 0, x, y, 1, 1)\n"
			"  WriteLight(0, 1, Center + z * 3 * RigSize, Center, <1, 1, 1>, 0.35, 0, 0, 0, 0, 0, 0, x, y, 1, 1)\n"
			"#end\n"
			"#end\n\n");
	}
	else
	{
		std::set<QString> UsedLightNames;

		for (const std::unique_ptr<swLight>& Light : Lights)
		{
			const swVector3 LightPosition = Light->GetPosition();
			const swVector3 LightTarget = LightPosition + Light->GetDirection();
			const swVector3 LightColor = Light->GetColor();
			const QString BaseLightName = MakeIdentifier(QString(Light->GetName()), QLatin1String("Light"));
			QString LightName = BaseLightName;
			int LightSuffix = 1;

			while (!UsedLightNames.insert(LightName).second)
				LightName = BaseLightName + '_' + QString::number(LightSuffix++);

			LightType = Light->GetLightType();
			Shadowless = Light->GetCastShadow() ? 0 : 1;
			Power = Light->GetPOVRayPower();
			FadeDistance = Light->GetPOVRayFadeDistance();
			FadePower = Light->GetPOVRayFadePower();
			SpotFalloff = Light->GetSpotConeAngle() / 2.0f;
			SpotRadius = SpotFalloff - Light->GetSpotPenumbraAngle();
			SpotTightness = Light->GetSpotPOVRayTightness();
			AreaCircle = (Light->GetAreaShape() == swLightAreaShape::Disk || Light->GetAreaShape() == swLightAreaShape::Ellipse) ? 1 : 0;
			AreaX = swVector3(Light->GetWorldMatrix()[0]) * Light->GetAreaSizeX();
			AreaY = swVector3(Light->GetWorldMatrix()[1]) * Light->GetAreaSizeY();
			AreaGrid = swVector2i(Light->GetAreaPOVRayGridX(), Light->GetAreaPOVRayGridY());

			Line = QString::asprintf(
					"#ifndef (Skip%s)\n  WriteLight(%i, %i, <%g, %g, %g>, <%g, %g, %g>, <%g, %g, %g>, %g, %g, %g, %g, %g, %g, %i, <%g, %g, %g>, <%g, %g, %g>, %i, %i)\n#end\n\n",
					LightName.toUtf8().constData(),
					static_cast<int>(LightType),
					Shadowless,
					LightPosition[1], LightPosition[0], LightPosition[2],
					LightTarget[1], LightTarget[0], LightTarget[2],
					LightColor[0], LightColor[1], LightColor[2],
					Power,
					FadeDistance, FadePower,
					SpotRadius, SpotFalloff, SpotTightness,
					AreaCircle, AreaX[0], AreaX[1], AreaX[2], AreaY[0], AreaY[1], AreaY[2], AreaGrid.x, AreaGrid.y);
			POVFile.WriteLine(Line.toUtf8().constData());
		}
	}

	POVFile.WriteLine("#ifndef (lg_quality) #declare lg_quality = Quality; #end\n\n");

	if (UseLGEO)
	{
		POVFile.WriteLine("#ifndef (lg_studs) #declare lg_studs = Studs; #end\n\n");

		POVFile.WriteLine("#if (lg_quality = 3) #declare lg_quality = 4; #end\n\n");

		POVFile.WriteLine("#include \"lg_defs.inc\"\n\n#include \"lg_color.inc\"\n\n");

		swDiskFile TableFile(QFileInfo(QDir(LGEOPath), QLatin1String("lg_elements.lst")).absoluteFilePath());

		if (!TableFile.Open(QIODevice::ReadOnly))
			return swUnexpected(tr("Could not find LGEO files in folder '%1'.").arg(LGEOPath));

		char TableLine[1024];

		while (TableFile.ReadLine(TableLine, sizeof(TableLine)))
		{

			char Src[129], Dst[129], Flags[11];

			if (*TableLine == ';')
				continue;

			if (sscanf(TableLine, "%128s%128s%10s", Src, Dst, Flags) != 3)
				continue;

			swstrcat(Src, ".dat");

			swPartData* PartData = Library->FindPartData(Src, nullptr);
			if (!PartData)
				continue;

			bool LgeoPartFound = false;

			if ((LgeoPartFound = strchr(Flags, 'L')))
			{
				std::pair<char[SW_PART_NAME_LENGTH + 1], int>& Entry = PartTable[PartData];
				Entry.second |= LGEO_PART_LGEO;
				snprintf(Entry.first, sizeof(Entry.first), "lg_%s", Dst);
			}

			if (strchr(Flags, 'A') && !LgeoPartFound)
			{
				std::pair<char[SW_PART_NAME_LENGTH + 1], int>& Entry = PartTable[PartData];
				Entry.second |= LGEO_PART_AR;
				snprintf(Entry.first, sizeof(Entry.first), "ar_%s", Dst);
			}

			if (strchr(Flags, 'S'))
			{
				auto Entry = PartTable.find(PartData);

				if (Entry != PartTable.end() && Entry->second.first[0])
				{
					// declare the base object and slope
					Entry->second.second |= LGEO_PART_SLOPE;
				}
			}
		}
	}

	// lgeo parts bring geometry only
	auto AddColorDefinition=[&ColorTable, &POVFile](int ColorIndex)
	{
		if (ColorTable[ColorIndex][0])
			return;

		const swColor* Color = &gColorList[ColorIndex];
		QString Definition;

		snprintf(ColorTable[ColorIndex].data(), ColorTable[ColorIndex].size(), "sw_%s", Color->SafeName);

		const QString Name = QLatin1String(ColorTable[ColorIndex].data());
		const QString Rgb = QString::fromLatin1("%1, %2, %3").arg(Color->Value[0]).arg(Color->Value[1]).arg(Color->Value[2]);
		const float Filter = Color->Translucent ? 0.85f : 0.0f;

		// nearly opaque colors pass little light
		const QString FilterValue = Color->Value[3] < 0.9f ? QString::fromLatin1("TransFilter") : QString::number(1.0f - Color->Value[3]);
		const float Emission = qMin(Color->Luminance / 100.0f, 1.0f);
		const QString Glow = QString::fromLatin1("Glow * %1").arg(Emission);
		QString Texture, Surface;

		if (Color->Finish == swColorFinish::Glitter || Color->Finish == swColorFinish::Speckle)
		{
			const QString Flake = QString::fromLatin1("%1, %2, %3").arg(Color->MaterialValue[0]).arg(Color->MaterialValue[1]).arg(Color->MaterialValue[2]);
			const float Size = Color->MaterialSize > 0.0f ? Color->MaterialSize : 1.0f;
			const QString FilterName = Color->Translucent ? FilterValue : QLatin1String("0");
			Surface = QString::fromLatin1("FlakeTexture(%1, %2, %3, %4, %5, %6)").arg(Rgb, FilterName).arg(Color->MaterialFraction).arg(Size).arg(Flake, Glow);

			// keep flakes in the material
			if (Color->Translucent)
				Texture = QString::fromLatin1("TranslucentTexture(%1, %2, %3)").arg(Rgb, FilterValue, Glow);
			else
				Texture = QString::fromLatin1("OpaqueTexture(%1, %2)").arg(Rgb, Glow);
		}
		else if (Color->Translucent)
			Texture = QString::fromLatin1("TranslucentTexture(%1, %2, %3)").arg(Rgb, FilterValue, Glow);
		else if (Color->Chrome)
			Texture = QString::fromLatin1("ChromeTexture(%1, %2)").arg(Rgb, Glow);
		else if (Color->Rubber)
			Texture = QString::fromLatin1("RubberTexture(%1, %2)").arg(Rgb, Glow);
		else if (Color->Finish == swColorFinish::Pearl)
			Texture = QString::fromLatin1("PearlTexture(%1, %2)").arg(Rgb, Glow);
		else if (Color->Finish == swColorFinish::Metal)
			Texture = QString::fromLatin1("MetalTexture(%1, %2)").arg(Rgb, Glow);
		else
			Texture = QString::fromLatin1("OpaqueTexture(%1, %2)").arg(Rgb, Glow);

		if (Surface.isEmpty())
			Surface = Name + QLatin1String("_texture");

		const QString Material = Filter > 0.0f ? QString::fromLatin1("TranslucentMaterial(%1, %2)").arg(Surface, Rgb) : QString::fromLatin1("OpaqueMaterial(%1)").arg(Surface);

		Definition = QString::fromLatin1("#ifndef (%1_texture)\n#declare %1_texture = %2\n#end\n#ifndef (%1)\n#declare %1 = %3\n#end\n\n").arg(Name, Texture, Material);

		POVFile.WriteLine(Definition.toLatin1().constData());
	};

	for (const swModelPartsEntry& ModelPart : ModelParts)
	{
		AddColorDefinition(ModelPart.ColorIndex);

		// declare colors fixed in the mesh
		const swMesh* Mesh = ModelPart.Mesh ? ModelPart.Mesh : ModelPart.PartData->GetMesh();

		if (Mesh)
			for (int SectionIdx = 0; SectionIdx < Mesh->mLods[SW_MESH_LOD_HIGH].NumSections; SectionIdx++)
				if (Mesh->mLods[SW_MESH_LOD_HIGH].Sections[SectionIdx].ColorIndex != gDefaultColor)
					AddColorDefinition(Mesh->mLods[SW_MESH_LOD_HIGH].Sections[SectionIdx].ColorIndex);
	}

	AddColorDefinition(swGetColorIndex(TopModelColorCode));

	std::set<std::string> IncludedLgeoNames;

	if (UseLGEO)
	{
		for (const swModelPartsEntry& ModelPart : ModelParts)
		{
			if (ModelPart.Mesh)
				continue;

			auto Search = PartTable.find(ModelPart.PartData);
			if (Search == PartTable.end())
				continue;

			swMesh* Mesh = ModelPart.PartData->GetMesh();

			if (!Mesh)
				continue;

			const std::pair<char[SW_PART_NAME_LENGTH + 1], int>& Entry = Search->second;
			if (Entry.first[0] && IncludedLgeoNames.insert(Entry.first).second)
			{
				Line = QString::asprintf("#include \"%s.inc\" // %s\n", Entry.first, ModelPart.PartData->m_strDescription);
				POVFile.WriteLine(Line.toUtf8().constData());
			}
		}

		POVFile.WriteLine("\n");
	}

	std::vector<const char*> ColorTablePointer;
	ColorTablePointer.resize(NumColors);
	for (size_t ColorIdx = 0; ColorIdx < NumColors; ColorIdx++)
		ColorTablePointer[ColorIdx] = ColorTable[ColorIdx].data();
	std::map<const swMesh*, std::string> MeshNames;
	std::map<const swMesh*, int> MeshFlags;
	std::map<const swTexture*, QString> TextureFiles;
	std::set<QString> UsedTextureFiles;
	std::set<QString> UsedPovNames;
	const QFileInfo SaveFileInfo(SaveFileName);
	const QDir TextureDir(SaveFileInfo.absolutePath() + QLatin1Char('/') + SaveFileInfo.completeBaseName() + QLatin1String("_textures"));

	// copy printed part images
	for (const swModelPartsEntry& ModelPart : ModelParts)
	{
		const swMesh* Mesh = ModelPart.Mesh ? ModelPart.Mesh : ModelPart.PartData->GetMesh();

		if (!Mesh)
			continue;

		for (int SectionIdx = 0; SectionIdx < Mesh->mLods[SW_MESH_LOD_HIGH].NumSections; SectionIdx++)
		{
			const swMeshSection& Section = Mesh->mLods[SW_MESH_LOD_HIGH].Sections[SectionIdx];
			const swTexture* Texture = Section.Texture.Get();

			if (Section.PrimitiveType != SW_MESH_TEXTURED_TRIANGLES || !Texture || TextureFiles.count(Texture))
				continue;

			const QImage Image = QImage::fromData(Library->ReadTextureFile(Texture));

			if (Image.isNull())
				continue;

			QString BaseName = QFileInfo(QString::fromLatin1(Texture->mName)).completeBaseName();
			QString ImageName = BaseName + QLatin1String(".png");

			for (int Suffix = 1; UsedTextureFiles.count(ImageName.toLower()); Suffix++)
				ImageName = BaseName + QLatin1Char('_') + QString::number(Suffix) + QLatin1String(".png");

			if (!TextureDir.mkpath(QLatin1String(".")) || !Image.save(TextureDir.filePath(ImageName), "PNG"))
				continue;

			UsedTextureFiles.insert(ImageName.toLower());
			TextureFiles[Texture] = TextureDir.filePath(ImageName);
		}
	}

	UsedPovNames.insert(TopModelName);

	for (const swColor& Color : gColorList)
	{
		UsedPovNames.insert(QLatin1String("sw_") + QString::fromLatin1(Color.SafeName));
		UsedPovNames.insert(QLatin1String("sw_") + QString::fromLatin1(Color.SafeName) + QLatin1String("_texture"));
	}

	for (const swModelPartsEntry& ModelPart : ModelParts)
	{
		swMesh* Mesh = !ModelPart.Mesh ? ModelPart.PartData->GetMesh() : ModelPart.Mesh;

		if (!Mesh)
			continue;

		if (!ModelPart.Mesh)
		{
			const std::map<const swPartData*, std::pair<char[SW_PART_NAME_LENGTH + 1], int>>::const_iterator ExistingPart = PartTable.find(ModelPart.PartData);

			if (ExistingPart != PartTable.end() && ExistingPart->second.first[0] &&
				(ExistingPart->second.second & (LGEO_PART_LGEO | LGEO_PART_AR)))
				continue;
		}

		std::map<const swMesh*, std::string>::iterator MeshNameIt = MeshNames.find(Mesh);
		const bool Added = MeshNameIt == MeshNames.end();

		if (Added)
		{
			const QString BaseName = QLatin1String("sw_") + MakeExportNameFragment(QString::fromUtf8(ModelPart.PartData->mFileName));
			const QString Identifier = MakeUniqueExportIdentifier(BaseName, UsedPovNames, QLatin1String("_clear"));
			MeshNameIt = MeshNames.emplace(Mesh, Identifier.mid(3).toStdString()).first;
		}

		const std::string& MeshName = MeshNameIt->second;

		if (!ModelPart.Mesh)
		{
			std::pair<char[SW_PART_NAME_LENGTH + 1], int>& Entry = PartTable[ModelPart.PartData];
			snprintf(Entry.first, sizeof(Entry.first), "sw_%s", MeshName.c_str());
		}

		if (!Added)
			continue;

		const int Flags = Mesh->ExportPOVRay(POVFile, MeshName.c_str(), &ColorTablePointer[0], TextureFiles);
		MeshFlags[Mesh] = Flags;

		if (Flags & SW_MESH_POVRAY_PLAIN)
		{
			Line = QString::asprintf("#declare sw_%s_clear = sw_%s\n\n", MeshName.c_str(), MeshName.c_str());
			POVFile.WriteLine(Line.toUtf8().constData());
		}
	}

	if (!ModelParts.empty())
	{
		Line = QString::asprintf("#declare %s = union {\n", TopModelName.toUtf8().constData());
		POVFile.WriteLine(Line.toUtf8().constData());
	}

	// join plain faces with the decal macro
	auto InstanceObject = [&MeshFlags, &ColorTable](const char* Name, const char* Suffix, const swMesh* Mesh, int ColorIdx)
	{
		const std::map<const swMesh*, int>::const_iterator Flags = MeshFlags.find(Mesh);
		const QByteArray Plain = QByteArray(Name) + Suffix;

		if (Flags == MeshFlags.end() || !(Flags->second & SW_MESH_POVRAY_DECALS))
			return Plain;

		const QByteArray Decals = QByteArray(Name) + "_decals(" + QByteArray(ColorTable[ColorIdx].data()) + "_texture)";

		if (!(Flags->second & SW_MESH_POVRAY_PLAIN))
			return QByteArray("union { ") + Decals + " }";

		return QByteArray("union { object { ") + Plain + " } " + Decals + " }";
	};

	for (const swModelPartsEntry& ModelPart : ModelParts)
	{
		int ColorIdx = ModelPart.ColorIndex;
		const char* Suffix = swIsColorTranslucent(ColorIdx) ? "_clear" : "";
		const float* f = ModelPart.WorldMatrix.GetFloats();
		const char* Modifier = "material";
		if (!ModelPart.Mesh)
		{
			std::pair<char[SW_PART_NAME_LENGTH + 1], int>& Entry = PartTable[ModelPart.PartData];

			if (!Entry.first[0] && !(Entry.second & LGEO_PART_SLOPE))
			{
				const std::map<const swMesh*, std::string>::const_iterator MeshName = MeshNames.find(ModelPart.PartData->GetMesh());

				if (MeshName != MeshNames.end())
					snprintf(Entry.first, sizeof(Entry.first), "sw_%s", MeshName->second.c_str());
			}

			if (Entry.second & LGEO_PART_SLOPE)
			{
				Line = QString::asprintf(
						"  merge {\n    object {\n      %s%s\n      %s { %s }\n    }\n"
						"    object {\n      %s_slope\n      %s { %s }\n      normal { bumps 0.3 scale 0.02 }\n    }\n"
						"    matrix <%g, %g, %g, %g, %g, %g, %g, %g, %g, %g, %g, %g>\n  }\n",
						Entry.first, Suffix, Modifier, ColorTable[ColorIdx].data(), Entry.first, Modifier, ColorTable[ColorIdx].data(),
						-f[5], -f[4], -f[6], -f[1], -f[0], -f[2], f[9], f[8], f[10], f[13] / 25.0f, f[12] / 25.0f, f[14] / 25.0f);
			}
			else
			{
				if (!ModelPart.PartData || !ModelPart.PartData->GetMesh())
					continue;

				Line = QString::asprintf("  object {\n    %s\n    %s { %s }\n    matrix <%g, %g, %g, %g, %g, %g, %g, %g, %g, %g, %g, %g>\n  }\n",
						InstanceObject(Entry.first, Suffix, ModelPart.PartData->GetMesh(), ColorIdx).constData(), Modifier, ColorTable[ColorIdx].data(), -f[5], -f[4], -f[6], -f[1], -f[0], -f[2], f[9], f[8], f[10], f[13] / 25.0f, f[12] / 25.0f, f[14] / 25.0f);

			}
		}
		else
		{
			const std::string& Name = MeshNames.at(ModelPart.Mesh);

			Line = QString::asprintf("  object {\n    %s\n    %s { %s }\n    matrix <%g, %g, %g, %g, %g, %g, %g, %g, %g, %g, %g, %g>\n  }\n",
					InstanceObject(("sw_" + Name).c_str(), Suffix, ModelPart.Mesh, ColorIdx).constData(), Modifier, ColorTable[ColorIdx].data(), -f[5], -f[4], -f[6], -f[1], -f[0], -f[2], f[9], f[8], f[10], f[13] / 25.0f, f[12] / 25.0f, f[14] / 25.0f);
		}

		POVFile.WriteLine(Line.toUtf8().constData());
	}

	if (!ModelParts.empty())
	{
		Line = QString::asprintf("\n  #if (ModelReflection = 0)\n    no_reflection\n  #end\n  #if (ModelShadow = 0)\n    no_shadow\n  #end\n}\n\n");
		POVFile.WriteLine(Line.toUtf8().constData());

		Line = QString::asprintf("object {\n  %s\n  material { %s }\n}\n\n",
				TopModelName.toUtf8().constData(), ColorTable[swGetColorIndex(TopModelColorCode)].data());
		POVFile.WriteLine(Line.toUtf8().constData());
	}

	// shadow catcher and cyclorama floors
	POVFile.WriteLine(
		"#if (Floor)\n"
		"  #local FloorTexture = texture { pigment { srgb FloorColor } finish { emission 0 ambient FloorAmbient diffuse FloorDiffuse } }\n"
		"  #local HorizonTexture = texture { pigment { rgb BackgroundColor } finish { emission 1 ambient 0 diffuse 0 } }\n"
		"  object {\n"
		"    plane { FloorAxis, FloorLocation hollow }\n"
		"    #if (FloorStyle = 1)\n"
		"      texture { pigment { rgb BackgroundColor } finish { emission 0.55 ambient 0 diffuse 0.5 } }\n"
		"      no_reflection\n"
		"    #elseif (FloorStyle = 2)\n"
		"      texture { pigment { srgb FloorColor } finish { emission 0 ambient FloorAmbient diffuse FloorDiffuse specular 0.4 roughness 0.01 reflection { 0.05, 0.6 fresnel on } } }\n"
		"      interior { ior 1.5 }\n"
		"    #elseif (FloorStyle = 3)\n"
		"      texture { spherical texture_map { [0 HorizonTexture] [0.6 FloorTexture] [1 FloorTexture] } scale Radius * 6 translate Center }\n"
		"    #else\n"
		"      texture { FloorTexture }\n"
		"    #end\n"
		"  }\n"
		"#end\n");

	if (!POVRayOptions.FooterIncludeFile.isEmpty())
	{
		Line = QString::asprintf("\n#include \"%s\"\n", POVRayOptions.FooterIncludeFile.toUtf8().constData());
		POVFile.WriteLine(Line.toUtf8().constData());
	}

	return swResult<void>();
}

swResult<void> Project::ExportWavefront(const QString& FileName)
{
	const std::vector<swModelPartsEntry> ModelParts = GetModelParts();

	QString SaveFileName = GetExportFileName(FileName, QLatin1String("obj"), tr("Export Wavefront"), tr("Wavefront Files (*.obj);;All Files (*.*)"));

	if (SaveFileName.isEmpty())
		return swUnexpected(QString());

	swDiskFile OBJFile(SaveFileName);
	char Line[1024];

	if (!OBJFile.Open(QIODevice::WriteOnly))
		return swUnexpected(tr("Could not open file '%1' for writing.").arg(SaveFileName));

	OBJFile.WriteLine("# Model exported from StudWorx\n");

	QFileInfo SaveInfo(SaveFileName);
	QString MaterialFileName = QDir(SaveInfo.absolutePath()).absoluteFilePath(SaveInfo.completeBaseName() + QLatin1String(".mtl"));

	snprintf(Line, sizeof(Line), "#\n\nmtllib %s\n\n", QFileInfo(MaterialFileName).fileName().toLatin1().constData());
	OBJFile.WriteLine(Line);

	swDiskFile MaterialFile(MaterialFileName);

	if (!MaterialFile.Open(QIODevice::WriteOnly))
		return swUnexpected(tr("Could not open file '%1' for writing.").arg(MaterialFileName));

	MaterialFile.WriteLine("# Colors used by StudWorx\n\n");

	for (const swColor& Color : gColorList)
	{
		if (Color.Translucent)
			snprintf(Line, sizeof(Line), "newmtl %s\nKd %.2f %.2f %.2f\nD %.2f\n\n", Color.SafeName, Color.Value[0], Color.Value[1], Color.Value[2], Color.Value[3]);
		else
			snprintf(Line, sizeof(Line), "newmtl %s\nKd %.2f %.2f %.2f\n\n", Color.SafeName, Color.Value[0], Color.Value[1], Color.Value[2]);

		MaterialFile.WriteLine(Line);
	}

	auto WritePositions=[&Line, &OBJFile](const auto* Verts, int VertexCount, const swMatrix44& ModelWorld)
	{
		for (int VertexIndex = 0; VertexIndex < VertexCount; VertexIndex++)
		{
			swVector3 Position = swMultiply31(Verts[VertexIndex].Position, ModelWorld);
			snprintf(Line, sizeof(Line), "v %.2f %.2f %.2f\n", Position[0], Position[1], Position[2]);
			OBJFile.WriteLine(Line);
		}
	};

	auto WriteNormals=[&Line, &OBJFile](const auto* Verts, int VertexCount, const swMatrix44& ModelWorld)
	{
		for (int VertexIndex = 0; VertexIndex < VertexCount; VertexIndex++)
		{
			swVector3 Normal = swMultiply30(swUnpackNormal(Verts[VertexIndex].Normal), ModelWorld);
			snprintf(Line, sizeof(Line), "vn %.2f %.2f %.2f\n", Normal[0], Normal[1], Normal[2]);
			OBJFile.WriteLine(Line);
		}
	};

	for (const swModelPartsEntry& ModelPart : ModelParts)
	{
		swMesh* Mesh = !ModelPart.Mesh ? ModelPart.PartData->GetMesh() : ModelPart.Mesh;

		if (!Mesh)
			continue;

		const swVertex* Verts = static_cast<const swVertex*>(Mesh->mVertexData);

		WritePositions(Verts, Mesh->mNumVertices, ModelPart.WorldMatrix);

		const swVertexTextured* VertsTextured = reinterpret_cast<const swVertexTextured*>(Verts + Mesh->mNumVertices);

		WritePositions(VertsTextured, Mesh->mNumTexturedVertices, ModelPart.WorldMatrix);

		OBJFile.WriteLine("#\n\n");
	}

	for (const swModelPartsEntry& ModelPart : ModelParts)
	{
		swMesh* Mesh = !ModelPart.Mesh ? ModelPart.PartData->GetMesh() : ModelPart.Mesh;

		if (!Mesh)
			continue;

		const swVertex* Verts = static_cast<const swVertex*>(Mesh->mVertexData);

		WriteNormals(Verts, Mesh->mNumVertices, ModelPart.WorldMatrix);

		const swVertexTextured* VertsTextured = reinterpret_cast<const swVertexTextured*>(Verts + Mesh->mNumVertices);

		WriteNormals(VertsTextured, Mesh->mNumTexturedVertices, ModelPart.WorldMatrix);

		OBJFile.WriteLine("#\n\n");
	}

	int PartCount = 0;
	quint32 BaseVertex = 1;

	for (const swModelPartsEntry& ModelPart : ModelParts)
	{
		swMesh* Mesh = !ModelPart.Mesh ? ModelPart.PartData->GetMesh() : ModelPart.Mesh;

		if (!Mesh)
			continue;

		snprintf(Line, sizeof(Line), "g Part%.3d\n", PartCount++);
		OBJFile.WriteLine(Line);

		Mesh->ExportWavefrontIndices(OBJFile, ModelPart.ColorIndex, BaseVertex);
		BaseVertex += Mesh->mNumVertices + Mesh->mNumTexturedVertices;
	}

	return swResult<void>();
}

swResult<void> Project::SaveImage(const swImageDialogOptions& Options)
{
	QString FilePath = Options.FilePath;
	QString Extension = QFileInfo(FilePath).suffix();

	if (!Extension.isEmpty())
		swSetProfileString(SW_PROFILE_IMAGE_EXTENSION, FilePath.right(Extension.length() + 1));

	// step animations hold every step in one file
	const bool StepFiles = Options.Start != Options.End && !Options.Turntable.IsStepping();

	if (StepFiles)
		FilePath = FilePath.insert(FilePath.length() - Extension.length() - 1, QLatin1String("%1"));

	return mActiveModel->SaveFramedImages(FilePath, StepFiles, Options.Width, Options.Height, Options.Start, Options.End, Options.Background, Options.Framing, Options.Turntable);
}
