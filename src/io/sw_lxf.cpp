#include "sw_global.h"
#include "sw_lxf.h"
#include "sw_library.h"
#include "sw_application.h"
#include "sw_main_window.h"
#include "sw_piece.h"
#include "sw_file.h"
#include <QDomDocument>

static bool swLoadLDrawXML(std::map<int, int>& MaterialTable, std::map<int, std::string>& BrickTable, std::map<std::string, std::pair<swVector3, swVector4>>& TransformTable)
{
	QFile File(swGetPiecesLibrary()->GetLibraryFilePath(QLatin1String("ldraw.xml")));
	QByteArray Data;

	if (File.open(QIODevice::ReadOnly))
		Data = File.readAll();
	else
	{
		QFile DefaultFile(":/resources/ldraw.xml");

		if (DefaultFile.open(QIODevice::ReadOnly))
			Data = DefaultFile.readAll();
	}

	if (Data.isEmpty())
		return false;

	QDomDocument Document;
	Document.setContent(QString::fromUtf8(Data));

	QDomElement Root = Document.documentElement();
	if (Root.tagName() != QLatin1String("LDrawMapping"))
		return false;

	for (QDomNode Node = Root.firstChild(); !Node.isNull(); Node = Node.nextSibling())
	{
		QDomElement Element = Node.toElement();
		QString ElementName = Element.tagName();

		if (ElementName == QLatin1String("Material"))
		{
			int LDrawColor = Element.attribute(QLatin1String("ldraw")).toInt();
			int LegoColor = Element.attribute(QLatin1String("lego")).toInt();

			MaterialTable[LegoColor] = LDrawColor;
		}
		else if (ElementName == QLatin1String("Brick"))
		{
			QString LDrawID = Element.attribute(QLatin1String("ldraw"));
			int LegoID = Element.attribute(QLatin1String("lego")).toInt();

			BrickTable.insert(std::make_pair(LegoID, LDrawID.toStdString()));
		}
		else if (ElementName == QLatin1String("Transformation"))
		{
			QString LDrawID = Element.attribute(QLatin1String("ldraw"));

			swVector3 Translation;
			swVector4 AxisAngle;

			Translation[0] = Element.attribute(QLatin1String("tx")).toFloat();
			Translation[1] = Element.attribute(QLatin1String("ty")).toFloat();
			Translation[2] = Element.attribute(QLatin1String("tz")).toFloat();
			AxisAngle[0] = Element.attribute(QLatin1String("ax")).toFloat();
			AxisAngle[1] = Element.attribute(QLatin1String("ay")).toFloat();
			AxisAngle[2] = Element.attribute(QLatin1String("az")).toFloat();
			AxisAngle[3] = Element.attribute(QLatin1String("angle")).toFloat();

			TransformTable.insert(std::make_pair(LDrawID.toStdString(), std::make_pair(Translation, AxisAngle)));
		}
	}

	return true;
}

bool swImportLXFMLFile(const QString& FileData, Project* Project, std::vector<swPiece*>& Pieces, std::vector<std::vector<swPiece*>>& Groups)
{
	std::map<int, int> MaterialTable;
	std::map<int, std::string> BrickTable;
	std::map<std::string, std::pair<swVector3, swVector4>> TransformTable;

	if (!swLoadLDrawXML(MaterialTable, BrickTable, TransformTable))
		return false;

	QDomDocument Document;
	Document.setContent(FileData);

	QDomElement Root = Document.documentElement();
	if (Root.tagName() != QLatin1String("LXFML"))
		return false;

	for (QDomNode SectionNode = Root.firstChild(); !SectionNode.isNull(); SectionNode = SectionNode.nextSibling())
	{
		QDomElement SectionElement = SectionNode.toElement();
		if (SectionElement.tagName() != QLatin1String("Bricks"))
			continue;

		for (QDomNode BrickNode = SectionElement.firstChild(); !BrickNode.isNull(); BrickNode = BrickNode.nextSibling())
		{
			QDomElement BrickElement = BrickNode.toElement();
			if (BrickElement.tagName() != QLatin1String("Brick"))
				continue;

			int NumBrickPieces = 0;

			for (QDomNode PartNode = BrickElement.firstChild(); !PartNode.isNull(); PartNode = PartNode.nextSibling())
			{
				QDomElement PartElement = PartNode.toElement();
				if (PartElement.tagName() != QLatin1String("Part"))
					continue;

				bool BoneFound = false;
				swMatrix44 WorldMatrix;

				for (QDomNode BoneNode = PartElement.firstChild(); !BoneNode.isNull(); BoneNode = BoneNode.nextSibling())
				{
					QDomElement BoneElement = BoneNode.toElement();
					if (BoneElement.tagName() != QLatin1String("Bone"))
						continue;

					QString BoneTransform = BoneElement.attribute(QLatin1String("transformation"));
					QStringList BoneElements = BoneTransform.split(',');

					if (BoneElements.size() != 12)
						continue;

					WorldMatrix[0][0] = BoneElements[0].toFloat();
					WorldMatrix[0][1] = BoneElements[1].toFloat();
					WorldMatrix[0][2] = BoneElements[2].toFloat();
					WorldMatrix[0][3] = 0.0f;
					WorldMatrix[1][0] = BoneElements[3].toFloat();
					WorldMatrix[1][1] = BoneElements[4].toFloat();
					WorldMatrix[1][2] = BoneElements[5].toFloat();
					WorldMatrix[1][3] = 0.0f;
					WorldMatrix[2][0] = BoneElements[6].toFloat();
					WorldMatrix[2][1] = BoneElements[7].toFloat();
					WorldMatrix[2][2] = BoneElements[8].toFloat();
					WorldMatrix[2][3] = 0.0f;
					WorldMatrix[3][0] = BoneElements[9].toFloat();
					WorldMatrix[3][1] = BoneElements[10].toFloat();
					WorldMatrix[3][2] = BoneElements[11].toFloat();
					WorldMatrix[3][3] = 1.0f;

					BoneFound = true;
					break;
				}

				if (!BoneFound)
					continue;

				QString LegoID = PartElement.attribute(QLatin1String("designID"));
				int Material = PartElement.attribute(QLatin1String("materials")).split(',').first().toInt();

				swPieceDataReference PieceData;
				const auto BrickIt = BrickTable.find(LegoID.toInt());

				if (BrickIt != BrickTable.end())
					PieceData = swGetPiecesLibrary()->AcquirePieceData(BrickIt->second.c_str(), Project, true, false);
				else
					PieceData = swGetPiecesLibrary()->AcquirePieceData(LegoID.toLatin1() + ".dat", Project, true, false);

				const auto ColorIt = MaterialTable.find(Material);
				int ColorCode = 16;

				if (ColorIt != MaterialTable.end())
					ColorCode = ColorIt->second;

				const auto TransformIt = TransformTable.find(BrickIt != BrickTable.end() ? BrickIt->second : (LegoID.toStdString() + ".dat"));
				if (TransformIt != TransformTable.end())
				{
					const swVector4& AxisAngle = TransformIt->second.second;
					const swVector3& TransformTranslation = TransformIt->second.first;

					swMatrix44 Transform = swMatrix44FromAxisAngle(swVector3(AxisAngle[0], AxisAngle[1], AxisAngle[2]), -AxisAngle[3]);
					Transform.SetTranslation(swMultiply30(-TransformTranslation, Transform));

					WorldMatrix = swMultiply(Transform, WorldMatrix);
				}

				WorldMatrix = swMatrix44(swVector4(WorldMatrix[0][0], -WorldMatrix[0][1], -WorldMatrix[0][2], 0.0f), swVector4(-WorldMatrix[1][0], WorldMatrix[1][1], WorldMatrix[1][2], 0.0f),
										 swVector4(-WorldMatrix[2][0], WorldMatrix[2][1], WorldMatrix[2][2], 0.0f), swVector4(WorldMatrix[3][0] * 25.0f, -WorldMatrix[3][1] * 25.0f, -WorldMatrix[3][2] * 25.0f, 1.0f));

				swPiece* Piece = new swPiece(nullptr);
				Piece->SetPieceData(std::move(PieceData), QString(), false, true);
				Piece->Initialize(swMatrix44LDrawToStudWorks(WorldMatrix), 1);
				Piece->SetColorCode(ColorCode);
				Pieces.push_back(Piece);
				NumBrickPieces++;
			}

			if (NumBrickPieces > 1)
			{
				std::vector<swPiece*> Group;
				for (size_t PieceIdx = Pieces.size() - NumBrickPieces; PieceIdx < Pieces.size(); PieceIdx++)
					Group.push_back(Pieces[PieceIdx]);
				Groups.push_back(Group);
			}
		}
	}

	return true;
}
