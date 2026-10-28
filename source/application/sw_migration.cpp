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

QString swRenameStoredName(QString Name)
{
	return Name.replace(QLatin1String("Pieces"), QLatin1String("Parts")).replace(QLatin1String("Piece"), QLatin1String("Part")).replace(QLatin1String("ViewSphere"), QLatin1String("ViewCube"));
}

static void swMigrateRenamedKeys()
{
	// Keys named after pieces or the view sphere take their part and view cube names, their values are newer than any left under the new name.
	QSettings Settings;

	for (const QString& Key : Settings.allKeys())
	{
		if (!Key.contains(QLatin1String("Piece")) && !Key.contains(QLatin1String("ViewSphere")))
			continue;

		Settings.setValue(swRenameStoredName(Key), Settings.value(Key));
		Settings.remove(Key);
	}

	Settings.remove(QLatin1String("Settings/mPartColorValueLDIndex"));
	Settings.sync();
}

static void swMigrateRenamedShortcuts()
{
	// Keyboard shortcuts are stored by command name, renamed commands keep their keys.
	QSettings Settings;
	const QString Key = QLatin1String("Settings/Shortcuts");
	const QString OldShortcuts = QString::fromUtf8(Settings.value(Key).toByteArray());
	QStringList Lines = OldShortcuts.split(QLatin1Char('\n'));

	for (QString& Line : Lines)
	{
		const qsizetype Equals = Line.indexOf(QLatin1Char('='));

		if (Equals != -1)
			Line = swRenameStoredName(Line.left(Equals)) + Line.mid(Equals);
	}

	const QString Shortcuts = Lines.join(QLatin1Char('\n'));

	if (Shortcuts == OldShortcuts)
		return;

	Settings.setValue(Key, Shortcuts.toUtf8());
	Settings.sync();
}

static QByteArray swSerializeName(const QString& Name)
{
	QByteArray Data;
	QDataStream Stream(&Data, QIODevice::WriteOnly);
	Stream << Name;

	return Data;
}

static void swMigrateRenamedToolbars()
{
	// Toolbars are restored by name, the stored window layout takes the new names so they keep their place.
	const QString RenamedToolbars[][2] =
	{
		{ QLatin1String("PiecesToolbar"), QLatin1String("PartsToolbar") }
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
	swMigrateRenamedShortcuts();
	swMigrateRenamedToolbars();
}
