#include "sw_global.h"
#include "sw_migration.h"

static void swCopyFolder(const QString& Source, const QString& Destination)
{
	QDir().mkpath(Destination);

	for (const QFileInfo& Entry : QDir(Source).entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden))
	{
		const QString Target = QDir(Destination).filePath(Entry.fileName());

		if (Entry.isDir())
			swCopyFolder(Entry.absoluteFilePath(), Target);
		else
			QFile::copy(Entry.absoluteFilePath(), Target);
	}
}

static void swMigrateEarlierSettings()
{
	// The first start takes over the settings and data stored under the earlier application name, later starts only use their own.
	QSettings Settings;

	if (!Settings.allKeys().isEmpty())
		return;

	const QString OrganizationName = QCoreApplication::organizationName();
	const QString ApplicationName = QCoreApplication::applicationName();
	const QString DataPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);

	QCoreApplication::setOrganizationName(QLatin1String("LeoCAD Software"));
	QCoreApplication::setApplicationName(QLatin1String("LeoCAD"));

	QSettings OldSettings;
	const QString OldDataPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);

	for (const QString& Key : OldSettings.allKeys())
		Settings.setValue(Key, OldSettings.value(Key));

	QCoreApplication::setOrganizationName(OrganizationName);
	QCoreApplication::setApplicationName(ApplicationName);

	if (QDir(OldDataPath).exists() && !QDir(DataPath).exists())
		swCopyFolder(OldDataPath, DataPath);

	Settings.sync();
}

static void swMigrateRenamedKeys()
{
	// Keys renamed from parts to pieces keep their stored value, a key already stored under its new name wins.
	constexpr const char* RenamedKeys[][2] =
	{
		{ "MainWindow/PartSelectionSplitter", "MainWindow/PieceSelectionSplitter" },
		{ "Settings/HighlightNewParts", "Settings/HighlightNewPieces" },
		{ "Settings/HighlightNewPartsColor", "Settings/HighlightNewPiecesColor" },
		{ "Settings/PartColorValueLDIndex", "Settings/PieceColorValueLDIndex" },
		{ "Settings/PartEdgeColor", "Settings/PieceEdgeColor" },
		{ "Settings/PartEdgeColorEnabled", "Settings/PieceEdgeColorEnabled" },
		{ "Settings/PartEdgeContrast", "Settings/PieceEdgeContrast" },
		{ "Settings/PartPalettes", "Settings/PiecePalettes" },
		{ "Settings/PartsDrawConditionalLines", "Settings/PiecesDrawConditionalLines" },
		{ "Settings/PartsDrawEdgeLines", "Settings/PiecesDrawEdgeLines" },
		{ "Settings/PartsLibrary", "Settings/PiecesLibrary" },
		{ "Settings/PartsLineWidth", "Settings/PiecesLineWidth" },
		{ "Settings/PartsListAliases", "Settings/PiecesListAliases" },
		{ "Settings/PartsListCaseSensitiveFilter", "Settings/PiecesListCaseSensitiveFilter" },
		{ "Settings/PartsListColorLocked", "Settings/PiecesListColorLocked" },
		{ "Settings/PartsListDecorated", "Settings/PiecesListDecorated" },
		{ "Settings/PartsListFileNameFilter", "Settings/PiecesListFileNameFilter" },
		{ "Settings/PartsListFixedColor", "Settings/PiecesListFixedColor" },
		{ "Settings/PartsListIcons", "Settings/PiecesListIcons" },
		{ "Settings/PartsListListMode", "Settings/PiecesListListMode" },
		{ "Settings/PartsListNames", "Settings/PiecesListNames" },
		{ "Settings/PartsListPartDescriptionFilter", "Settings/PiecesListPieceDescriptionFilter" },
		{ "Settings/PartsListPartFilter", "Settings/PiecesListPieceFilter" },
		{ "Settings/PartsPopupAliases", "Settings/PiecesPopupAliases" },
		{ "Settings/PartsPopupDecorated", "Settings/PiecesPopupDecorated" },
		{ "Settings/PartsPopupIcons", "Settings/PiecesPopupIcons" },
		{ "Settings/PartsPopupListMode", "Settings/PiecesPopupListMode" },
		{ "Settings/PartsPopupNames", "Settings/PiecesPopupNames" },
		{ "Settings/PartsShadingMode", "Settings/PiecesShadingMode" },
		{ "Settings/PartsSyncRendering", "Settings/PiecesSyncRendering" },
		{ "Settings/mPartColorValueLDIndex", "Settings/PieceColorValueLDIndex" }
	};

	QSettings Settings;

	for (const auto& [OldKey, NewKey] : RenamedKeys)
	{
		if (!Settings.contains(OldKey))
			continue;

		if (!Settings.contains(NewKey))
			Settings.setValue(NewKey, Settings.value(OldKey));

		Settings.remove(OldKey);
	}

	Settings.sync();
}

static QByteArray swSerializeName(const char* Name)
{
	QByteArray Data;
	QDataStream Stream(&Data, QIODevice::WriteOnly);
	Stream << QString::fromLatin1(Name);

	return Data;
}

static void swMigrateRenamedToolbars()
{
	// Toolbars are restored by name, the stored window layout takes the new names so they keep their place.
	constexpr const char* RenamedToolbars[][2] =
	{
		{ "PartsToolbar", "PiecesToolbar" }
	};

	QSettings Settings;
	const QByteArray OldState = Settings.value("MainWindow/State").toByteArray();
	QByteArray State = OldState;

	for (const auto& [OldName, NewName] : RenamedToolbars)
		State.replace(swSerializeName(OldName), swSerializeName(NewName));

	if (State == OldState)
		return;

	Settings.setValue("MainWindow/State", State);
	Settings.sync();
}

void swMigrateSettings()
{
	// Stored lists of numbers can only be read once their type is known, nothing has registered it this early.
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	qRegisterMetaTypeStreamOperators<QList<int>>("QList<int>");
#endif

	swMigrateEarlierSettings();
	swMigrateRenamedKeys();
	swMigrateRenamedToolbars();
}
