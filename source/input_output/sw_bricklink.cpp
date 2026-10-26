#include "sw_global.h"
#include "sw_bricklink.h"
#include "sw_file.h"
#include "sw_library.h"
#include "sw_application.h"
#include "sw_string.h"
#include "sw_piece_data.h"
#include "sw_colors.h"

static QJsonDocument swLoadBrickLinkMapping()
{
	std::map<std::string, std::string> Remapping;

	QFile File(swGetPiecesLibrary()->GetLibraryFilePath(QLatin1String("ldraw2bl.txt")));
	QByteArray Data;

	if (File.open(QIODevice::ReadOnly))
		Data = File.readAll();
	else
	{
		QFile DefaultFile(":/resources/data/ldraw2bl.txt");

		if (DefaultFile.open(QIODevice::ReadOnly))
			Data = DefaultFile.readAll();
	}

	return QJsonDocument::fromJson(Data);
}

static std::string swGetBrickLinkPart(const QJsonObject& PartMapping, const QString& LDrawPart)
{
	QJsonObject::const_iterator PartIt = PartMapping.find(LDrawPart);

	return (PartIt != PartMapping.end()) ? PartIt.value().toString().toStdString() : LDrawPart.toStdString();
}

static int swGetBrickLinkColor(const QJsonObject& ColorMapping, int ColorIndex)
{
	int ColorCode = gColorList[ColorIndex].Code;
	QJsonObject::const_iterator ColorIt = ColorMapping.find(QString::number(ColorCode));

	return (ColorIt != ColorMapping.end()) ? ColorIt.value().toString().toInt() : 0;
}

class swBrickLinkItem
{
public:
	swBrickLinkItem(const std::string& id, int color, int count)
		: mId(id), mColor(color), mCount(count)
	{
	}

	void AddToCount(int count)
	{
		mCount += count;
	}

	std::string mId;
	int mColor;
	int mCount;
};

swResult<void> swExportBrickLink(const QString& SaveFileName, const swPartsList& PartsList)
{
	QJsonDocument Document = swLoadBrickLinkMapping();
	QJsonObject Root = Document.object();
	QJsonObject PartMapping = Root["Parts"].toObject();
	QJsonObject ColorMapping = Root["Colors"].toObject();

	swDiskFile BrickLinkFile(SaveFileName);
	char Line[1024];

	if (!BrickLinkFile.Open(QIODevice::WriteOnly))
		return swUnexpected(QObject::tr("Could not open file '%1' for writing.").arg(SaveFileName));

	std::map<std::string, swBrickLinkItem> Inventory;

	BrickLinkFile.WriteLine("<INVENTORY>\n");

	for (const auto& PartIt : PartsList)
	{
		const swPieceData* PieceData = PartIt.first;

		for (const auto& ColorIt : PartIt.second)
		{
			char FileName[SW_PIECE_NAME_LENGTH];
			swstrcpy(FileName, PieceData->mFileName);
			char* Ext = strchr(FileName, '.');
			if (Ext)
				*Ext = 0;

			std::string Remapped = swGetBrickLinkPart(PartMapping, FileName);
			int Color = swGetBrickLinkColor(ColorMapping, ColorIt.first);
			std::string key(Remapped);
			key += "-" + std::to_string(Color);
			auto iter = Inventory.find(key);
			if (iter == Inventory.end())
			{
				Inventory.emplace(std::make_pair(key, swBrickLinkItem(Remapped, Color, ColorIt.second)));
			}
			else
			{
				iter->second.AddToCount(ColorIt.second);
			}
		}
	}

	for (const auto& Item : Inventory)
	{
		BrickLinkFile.WriteLine("  <ITEM>\n");
		BrickLinkFile.WriteLine("    <ITEMTYPE>P</ITEMTYPE>\n");
		snprintf(Line, sizeof(Line), "    <ITEMID>%s</ITEMID>\n", Item.second.mId.c_str());
		BrickLinkFile.WriteLine(Line);

		snprintf(Line, sizeof(Line), "    <MINQTY>%d</MINQTY>\n", Item.second.mCount);
		BrickLinkFile.WriteLine(Line);

		if (Item.second.mColor)
		{
			snprintf(Line, sizeof(Line), "    <COLOR>%d</COLOR>\n", Item.second.mColor);
			BrickLinkFile.WriteLine(Line);
		}

		BrickLinkFile.WriteLine("  </ITEM>\n");
	}

	BrickLinkFile.WriteLine("</INVENTORY>\n");

	return swResult<void>();
}
