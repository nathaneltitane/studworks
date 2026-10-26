#include "sw_global.h"
#include "sw_application.h"
#include "sw_profile.h"
#include "sw_piece_data.h"
#include <QApplication>
#include <locale.h>

#ifdef Q_OS_WIN

#pragma warning(push)
#pragma warning(disable : 4091)
#include <windows.h>
#include <dbghelp.h>
#include <direct.h>
#include <shlobj.h>
#pragma warning(pop)

#ifdef UNICODE
#ifndef _UNICODE
#define _UNICODE
#endif
#endif

#include <tchar.h>

static TCHAR gMinidumpPath[_MAX_PATH];

static LONG WINAPI swSehHandler(PEXCEPTION_POINTERS ExceptionPointers)
{
	if (IsDebuggerPresent())
		return EXCEPTION_CONTINUE_SEARCH;

	HMODULE DbgHelp = LoadLibrary(TEXT("dbghelp.dll"));

	if (DbgHelp == nullptr)
		return EXCEPTION_EXECUTE_HANDLER;

	HANDLE File = CreateFile(gMinidumpPath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

	if (File == INVALID_HANDLE_VALUE)
		return EXCEPTION_EXECUTE_HANDLER;

	typedef BOOL (WINAPI *LPMINIDUMPWRITEDUMP)(HANDLE hProcess, DWORD ProcessId, HANDLE hFile, MINIDUMP_TYPE DumpType, CONST PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam, CONST PMINIDUMP_USER_STREAM_INFORMATION UserEncoderParam, CONST PMINIDUMP_CALLBACK_INFORMATION CallbackParam);
	LPMINIDUMPWRITEDUMP MiniDumpWriteDump = (LPMINIDUMPWRITEDUMP)GetProcAddress(DbgHelp, "MiniDumpWriteDump");
	if (!MiniDumpWriteDump)
		return EXCEPTION_EXECUTE_HANDLER;

	MINIDUMP_EXCEPTION_INFORMATION Mei;

	Mei.ThreadId = GetCurrentThreadId();
	Mei.ExceptionPointers = ExceptionPointers;
	Mei.ClientPointers = TRUE;

	BOOL WriteDump = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), File, MiniDumpNormal, ExceptionPointers ? &Mei : nullptr, nullptr, nullptr);

	CloseHandle(File);
	FreeLibrary(DbgHelp);

	if (WriteDump)
	{
		TCHAR Message[_MAX_PATH + 256];
		lstrcpy(Message, TEXT("StudWorks just crashed. Crash information was saved to the file '"));
		lstrcat(Message, gMinidumpPath);
		lstrcat(Message, TEXT("', please send it to the developers for debugging."));

		MessageBox(nullptr, Message, TEXT("StudWorks"), MB_OK);
	}

	return EXCEPTION_EXECUTE_HANDLER;
}

static void swSehInitialize()
{
	if (GetTempPath(SW_ARRAY_COUNT(gMinidumpPath), gMinidumpPath))
		lstrcat(gMinidumpPath, TEXT("studworks.dmp"));

	SetUnhandledExceptionFilter(swSehHandler);
}

static void swRegisterShellFileTypes()
{
	TCHAR ModulePath[_MAX_PATH], LongModulePath[_MAX_PATH];
	TCHAR Temp[2*_MAX_PATH];

	GetModuleFileName(nullptr, LongModulePath, _MAX_PATH);
	if (GetShortPathName(LongModulePath, ModulePath, _MAX_PATH) == 0)
		lstrcpy(ModulePath, LongModulePath);

	if (RegSetValue(HKEY_CLASSES_ROOT, TEXT("StudWorks.Project"), REG_SZ, TEXT("StudWorks Model"), lstrlen(TEXT("StudWorks Model")) * sizeof(TCHAR)) != ERROR_SUCCESS)
		return;

	lstrcpy(Temp, ModulePath);
	lstrcat(Temp, TEXT(",0"));
	if (RegSetValue(HKEY_CLASSES_ROOT, TEXT("StudWorks.Project\\DefaultIcon"), REG_SZ, Temp, lstrlen(Temp) * sizeof(TCHAR)) != ERROR_SUCCESS)
		return;

	lstrcpy(Temp, ModulePath);
	lstrcat(Temp, TEXT(" \"%1\""));
	if (RegSetValue(HKEY_CLASSES_ROOT, TEXT("StudWorks.Project\\shell\\open\\command"), REG_SZ, Temp, lstrlen(Temp) * sizeof(TCHAR)) != ERROR_SUCCESS)
		return;

	LONG Size = 2 * _MAX_PATH;
	LONG Result = RegQueryValue(HKEY_CLASSES_ROOT, TEXT(".lcd"), Temp, &Size);

	if (Result != ERROR_SUCCESS || !lstrlen(Temp) || lstrcmp(Temp, TEXT("StudWorks.Project")))
	{
		if (RegSetValue(HKEY_CLASSES_ROOT, TEXT(".lcd"), REG_SZ, TEXT("StudWorks.Project"), lstrlen(TEXT("StudWorks.Project")) * sizeof(TCHAR)) != ERROR_SUCCESS)
			return;

		HKEY Key;
		DWORD Disposition = 0;

		if (RegCreateKeyEx(HKEY_CLASSES_ROOT, TEXT(".lcd\\ShellNew"), 0, REG_NONE, REG_OPTION_NON_VOLATILE, KEY_WRITE | KEY_READ, nullptr, &Key, &Disposition) != ERROR_SUCCESS)
			return;

		Result = RegSetValueEx(Key, TEXT("NullFile"), 0, REG_SZ, (CONST BYTE*)TEXT(""), (lstrlen(TEXT("")) + 1) * sizeof(TCHAR));

		if (RegCloseKey(Key) != ERROR_SUCCESS || Result != ERROR_SUCCESS)
			return;
	}
}

#endif

static void swInitializeSurfaceFormat(int argc, char* argv[])
{
	QCoreApplication Application(argc, argv);
	const swCommandLineOptions Options = swApplication::ParseCommandLineOptions();

	if (Options.ParseOK && Options.AASamples > 1)
	{
		QSurfaceFormat Format = QSurfaceFormat::defaultFormat();
		Format.setSamples(Options.AASamples);
		QSurfaceFormat::setDefaultFormat(Format);
	}
}

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

static void swMigrateLeoCADData()
{
	// The first start takes over the LeoCAD settings and data, later starts only use their own.
	QSettings Settings;

	if (!Settings.allKeys().isEmpty())
		return;

	const QString OrganizationName = QCoreApplication::organizationName();
	const QString ApplicationName = QCoreApplication::applicationName();
	const QString DataPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);

	QCoreApplication::setOrganizationName(QLatin1String("LeoCAD Software"));
	QCoreApplication::setApplicationName(QLatin1String("LeoCAD"));

	// Stored lists of numbers can only be read once their type is known, nothing has registered it this early.
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	qRegisterMetaTypeStreamOperators<QList<int>>("QList<int>");
#endif

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

int main(int argc, char *argv[])
{
	QCoreApplication::setOrganizationDomain(QLatin1String("nathaneltitane.github.io"));
	QCoreApplication::setOrganizationName(QLatin1String("StudWorks"));
	QCoreApplication::setApplicationName(QLatin1String("StudWorks"));
	QCoreApplication::setApplicationVersion(QLatin1String(SW_VERSION_TEXT));
	QGuiApplication::setDesktopFileName(QLatin1String("studworks"));

	swMigrateLeoCADData();
	swInitializeSurfaceFormat(argc, argv);

	QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

	swApplication Application(argc, argv);

	QString Language = swGetProfileString(SW_PROFILE_LANGUAGE);
	QLocale Locale;

	if (!Language.isEmpty())
		Locale = QLocale(Language);

	QTranslator QtTranslator;
	if (QtTranslator.load(Locale, "qt", "_", QLibraryInfo::location(QLibraryInfo::TranslationsPath)))
		Application.installTranslator(&QtTranslator);
#ifdef Q_OS_WIN
	else if (QtTranslator.load(Locale, "qt", "_", qApp->applicationDirPath() + "/translations"))
		Application.installTranslator(&QtTranslator);
#endif

	QTranslator QtBaseTranslator;
	if (QtBaseTranslator.load("qtbase_" + Locale.name(), QLibraryInfo::location(QLibraryInfo::TranslationsPath)))
		Application.installTranslator(&QtBaseTranslator);
#ifdef Q_OS_WIN
	else if (QtBaseTranslator.load("qtbase_" + Locale.name(), qApp->applicationDirPath() + "/translations"))
		Application.installTranslator(&QtBaseTranslator);
#endif

	// Supply English plural forms when a locale has no translation for a numerus message.
	QTranslator EnglishTranslator;
	if (EnglishTranslator.load("studworks_en", ":/resources"))
		Application.installTranslator(&EnglishTranslator);

	QTranslator Translator;
	if (Translator.load("studworks_" + Locale.name(), ":/resources"))
		Application.installTranslator(&Translator);

	qRegisterMetaType<swPieceData*>("swPieceData*");
	qRegisterMetaType<QList<int> >("QList<int>");
	qRegisterMetaType<swVector3>("swVector3");
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
	qRegisterMetaTypeStreamOperators<QList<int> >("QList<int>");
	QMetaType::registerComparators<swVector3>();
#endif

	QList<QPair<QString, bool>> LibraryPaths;

#ifdef Q_OS_WIN
	swRegisterShellFileTypes();
	swSehInitialize();

	LibraryPaths += qMakePair(QDir::cleanPath(QCoreApplication::applicationDirPath() + "/library.bin"), true);
#endif

#ifdef Q_OS_LINUX
	LibraryPaths += qMakePair(QDir::cleanPath(QCoreApplication::applicationDirPath() + "/../share/studworks/library.bin"), true);
#endif

#ifdef Q_OS_MAC
	LibraryPaths += qMakePair(QDir::cleanPath(QCoreApplication::applicationDirPath() + "/../../Contents/Resources/library.bin"), true);
#endif

#ifdef SW_LDRAW_LIBRARY_PATH
	LibraryPaths += qMakePair(QString::fromLatin1(SW_LDRAW_LIBRARY_PATH), false);
#endif

	setlocale(LC_NUMERIC, "C");

	swStartupMode StartupMode = Application.Initialize(LibraryPaths);

	if (StartupMode == swStartupMode::Error)
	{
		Application.Shutdown();
		return 1;
	}

	int ExecReturn = 0;

	if (StartupMode == swStartupMode::ShowWindow)
	{
		ExecReturn = Application.exec();
	}

	Application.Shutdown();

	return ExecReturn;
}
