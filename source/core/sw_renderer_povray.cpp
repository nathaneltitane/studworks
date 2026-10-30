#include "sw_global.h"
#include "sw_renderer_povray.h"
#include "sw_dialog_render.h"
#include "sw_file_zip.h"
#include "sw_file.h"
#include "sw_http.h"
#include "sw_profile.h"

#define SW_POVRAY_DOWNLOAD_URL "https://www.povray.org/download/"
#define SW_LGEO_DOWNLOAD_URL "https://github.com/jncraton/lgeo/archive/HEAD.zip"
#define SW_LGEO_PROJECT_URL "https://github.com/jncraton/lgeo"

// use to run a program for a render
static QString swRunPOVRay(const QString& Path, const QStringList& Arguments, int& ExitCode)
{
	QProcess Process;
	Process.setEnvironment(swGetPOVRayEnvironment());
	Process.setProcessChannelMode(QProcess::MergedChannels);
	Process.start(Path, Arguments);

	if (!Process.waitForStarted(5000) || !Process.waitForFinished(15000))
	{
		Process.kill();
		Process.waitForFinished(1000);
		ExitCode = -1;

		return QString();
	}

	ExitCode = Process.exitStatus() == QProcess::NormalExit ? Process.exitCode() : -1;

	return QString::fromLocal8Bit(Process.readAll());
}

// use to read a program version
swPOVRayProgram swGetPOVRayProgram(const QString& Path)
{
	static std::map<QString, swPOVRayProgram> Programs;

	swPOVRayProgram Program;
	Program.Path = Path;

	if (Path.isEmpty() || !QFileInfo(Path).isExecutable())
		return Program;

	const std::map<QString, swPOVRayProgram>::const_iterator Cached = Programs.find(Path);

	if (Cached != Programs.end())
		return Cached->second;

#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)

	// the bundled console is supported
	Program.Version = QLatin1String("3.7");
	Program.VersionMajor = 3;
	Program.VersionMinor = 7;
	Program.Supported = true;
#else
	int ExitCode = 0;
	const QString Output = swRunPOVRay(Path, { QLatin1String("--version") }, ExitCode);
	const QRegularExpressionMatch Match = QRegularExpression(QLatin1String("POV-Ray\\s+((\\d+)\\.(\\d+)[\\w.+-]*)")).match(Output);

	if (Match.hasMatch())
	{
		Program.Version = Match.captured(1);
		Program.VersionMajor = Match.captured(2).toInt();
		Program.VersionMinor = Match.captured(3).toInt();
		Program.Supported = Program.VersionMajor > 3 || (Program.VersionMajor == 3 && Program.VersionMinor >= 7);
	}
#endif

	Programs[Path] = Program;

	return Program;
}

// use to list installed povray programs
static QStringList swGetPOVRayCandidates()
{
	QStringList Candidates;

#if defined(Q_OS_WIN)
	Candidates.append(QDir::cleanPath(QCoreApplication::applicationDirPath() + QLatin1String("/povconsole32-sse2.exe")));
#elif defined(Q_OS_MACOS)
	Candidates.append(QDir::cleanPath(QCoreApplication::applicationDirPath() + QLatin1String("/povray")));
#else
	Candidates.append(swGetProfileString(SW_PROFILE_POVRAY_PATH));

	for (const QString& Name : { QLatin1String("povray"), QLatin1String("povray3.7"), QLatin1String("povray37") })
		Candidates.append(QStandardPaths::findExecutable(Name));

	Candidates.append(QLatin1String("/usr/local/bin/povray"));
	Candidates.append(QLatin1String("/snap/bin/povray"));

	// one flatpak launcher per application
	const QStringList FlatpakFolders = { QDir::homePath() + QLatin1String("/.local/share/flatpak/exports/bin"), QLatin1String("/var/lib/flatpak/exports/bin") };

	for (const QString& Folder : FlatpakFolders)
		for (const QFileInfo& Launcher : QDir(Folder).entryInfoList(QDir::Files | QDir::Executable))
			if (Launcher.fileName().contains(QLatin1String("povray"), Qt::CaseInsensitive))
				Candidates.append(Launcher.absoluteFilePath());
#endif

	Candidates.removeAll(QString());
	Candidates.removeDuplicates();

	return Candidates;
}

// use to find a supported povray
swPOVRayProgram swFindPOVRay()
{
	swPOVRayProgram Unsupported;

	for (const QString& Candidate : swGetPOVRayCandidates())
	{
		const swPOVRayProgram Program = swGetPOVRayProgram(Candidate);

		if (Program.Supported)
		{
#if !defined(Q_OS_WIN) && !defined(Q_OS_MACOS)
			if (Program.Path != swGetProfileString(SW_PROFILE_POVRAY_PATH))
				swSetProfileString(SW_PROFILE_POVRAY_PATH, Program.Path);
#endif

			return Program;
		}

		if (Unsupported.Version.isEmpty() && !Program.Version.isEmpty())
			Unsupported = Program;
	}

	return Unsupported;
}

// use to find the include folder
static QString swGetPOVRayIncludePath(const swPOVRayProgram& Program)
{
	const QString ProgramFolder = QFileInfo(Program.Path).absolutePath();
	const QString VersionFolder = QString::fromLatin1("povray-%1.%2").arg(Program.VersionMajor).arg(Program.VersionMinor);

	const QStringList Candidates =
	{
		ProgramFolder + QLatin1String("/include"),
		ProgramFolder + QLatin1String("/../share/") + VersionFolder + QLatin1String("/include"),
		QLatin1String("/usr/share/") + VersionFolder + QLatin1String("/include"),
		QLatin1String("/usr/local/share/") + VersionFolder + QLatin1String("/include")
	};

	for (const QString& Candidate : Candidates)
		if (QFileInfo::exists(Candidate + QLatin1String("/colors.inc")))
			return QDir::cleanPath(Candidate);

	return QString();
}

// use to test the include files
bool swPOVRayHasIncludeFiles(const swPOVRayProgram& Program)
{
	static std::map<QString, bool> Results;

	const std::map<QString, bool>::const_iterator Cached = Results.find(Program.Path);

	if (Cached != Results.end())
		return Cached->second;

	QTemporaryDir Folder;

	if (!Folder.isValid())
		return true;

	const QString FileName = Folder.filePath(QLatin1String("includes.pov"));
	QFile File(FileName);

	if (!File.open(QIODevice::WriteOnly))
		return true;

	File.write("#include \"colors.inc\"\ncamera { location <0, 0, -1> look_at <0, 0, 0> }\nbackground { color White }\n");
	File.close();

	QStringList Arguments = { QLatin1String("+I") + FileName, QLatin1String("+W1"), QLatin1String("+H1"), QLatin1String("-D"), QLatin1String("-F") };
	const QString IncludePath = swGetPOVRayIncludePath(Program);

	if (!IncludePath.isEmpty())
		Arguments.append(QLatin1String("+L") + IncludePath);

	// only a missing colors.inc counts
	int ExitCode = 0;
	const QString Output = swRunPOVRay(Program.Path, Arguments, ExitCode);
	const bool HasIncludeFiles = ExitCode == 0 || !Output.contains(QLatin1String("colors.inc"));

	Results[Program.Path] = HasIncludeFiles;

	return HasIncludeFiles;
}

// use to tell an lgeo folder
static bool swIsLGEOPath(const QString& Path)
{
	return !Path.isEmpty() && QFileInfo::exists(QDir(Path).filePath(QLatin1String("lg_elements.lst"))) && QFileInfo::exists(QDir(Path).filePath(QLatin1String("lg/lg_defs.inc")));
}

// use to name the lgeo folder
static QString swGetDownloadLGEOPath()
{
	return QDir::cleanPath(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QLatin1String("/lgeo"));
}

// use to find lgeo
QString swFindLGEOPath()
{
	const QString Path = swGetProfileString(SW_PROFILE_POVRAY_LGEO_PATH);

	if (swIsLGEOPath(Path))
		return Path;

	QStringList Folders;
	const QString LibraryPath = swGetProfileString(SW_PROFILE_PARTS_LIBRARY);

	if (!LibraryPath.isEmpty())
		Folders.append(QFileInfo(LibraryPath).absolutePath());

	const QString LDrawPath = qEnvironmentVariable("LDRAWDIR");

	if (!LDrawPath.isEmpty())
		Folders.append(QFileInfo(LDrawPath).absolutePath());

	Folders.append(QDir::homePath());
	Folders.append(QDir::homePath() + QLatin1String("/.local/share"));
	Folders.append(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
	Folders.append(QLatin1String("/usr/local/share"));
	Folders.append(QLatin1String("/usr/share"));
	Folders.append(QLatin1String("/opt"));

	for (const QString& Folder : Folders)
	{
		for (const QString& Name : { QLatin1String("lgeo"), QLatin1String("LGEO") })
		{
			const QString Candidate = QDir::cleanPath(Folder + QLatin1Char('/') + Name);

			if (swIsLGEOPath(Candidate))
			{
				swSetProfileString(SW_PROFILE_POVRAY_LGEO_PATH, Candidate);
				return Candidate;
			}
		}
	}

	return QString();
}

// use to unpack the lgeo archive
static bool swExtractLGEO(const QByteArray& Data, const QString& Path)
{
	std::unique_ptr<swMemoryFile> ArchiveFile = std::make_unique<swMemoryFile>();
	ArchiveFile->WriteBuffer(Data.constData(), Data.size());
	ArchiveFile->Seek(0, SEEK_SET);

	swZipFile Archive;

	if (!Archive.OpenRead(std::move(ArchiveFile)))
		return false;

	const QString TemporaryPath = Path + QLatin1String(".part");
	QDir(TemporaryPath).removeRecursively();

	for (quint32 FileIndex = 0; FileIndex < Archive.mFiles.size(); FileIndex++)
	{
		const QString Name = QString::fromUtf8(Archive.mFiles[FileIndex].file_name);
		const int Separator = Name.indexOf(QLatin1Char('/'));
		const QString RelativeName = Separator >= 0 ? Name.mid(Separator + 1) : Name;

		if (RelativeName.isEmpty() || RelativeName.endsWith(QLatin1Char('/')) || RelativeName.contains(QLatin1String("..")))
			continue;

		swMemoryFile EntryFile;

		if (!Archive.ExtractFile(FileIndex, EntryFile))
			return false;

		const QString FileName = TemporaryPath + QLatin1Char('/') + RelativeName;
		QDir().mkpath(QFileInfo(FileName).absolutePath());

		QFile File(FileName);

		if (!File.open(QIODevice::WriteOnly) || File.write(reinterpret_cast<const char*>(EntryFile.mBuffer), EntryFile.GetLength()) != static_cast<qint64>(EntryFile.GetLength()))
			return false;
	}

	if (!swIsLGEOPath(TemporaryPath))
		return false;

	QDir(Path).removeRecursively();

	return QDir().rename(TemporaryPath, Path);
}

// use to get a missing lgeo
QString swSetupLGEO(QWidget* Parent)
{
	const QString Title = QCoreApplication::translate("swRenderDialog", "LGEO Library");
	const QString DownloadPath = swGetDownloadLGEOPath();
	const QString Question = QCoreApplication::translate("swRenderDialog", "StudWorks could not find the LGEO part library.\n\nDownload it (about 7 MB) from %1 into '%2'?").arg(QLatin1String(SW_LGEO_PROJECT_URL), QDir::toNativeSeparators(DownloadPath));

	if (QMessageBox::question(Parent, Title, Question, QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
	{
		const std::optional<QByteArray> Data = swDownloadWithProgress(Parent, QLatin1String(SW_LGEO_DOWNLOAD_URL), Title, QCoreApplication::translate("swRenderDialog", "Downloading the LGEO part library..."));

		if (Data && !Data->isEmpty() && swExtractLGEO(*Data, DownloadPath))
		{
			swSetProfileString(SW_PROFILE_POVRAY_LGEO_PATH, DownloadPath);
			return DownloadPath;
		}

		if (Data)
			QMessageBox::warning(Parent, Title, QCoreApplication::translate("swRenderDialog", "Could not download the LGEO part library from %1.").arg(QLatin1String(SW_LGEO_DOWNLOAD_URL)));
	}

	for (;;)
	{
		const QString Path = QFileDialog::getExistingDirectory(Parent, QCoreApplication::translate("swRenderDialog", "Open LGEO Folder"), QDir::homePath());

		if (Path.isEmpty())
			break;

		if (swIsLGEOPath(Path))
		{
			swSetProfileString(SW_PROFILE_POVRAY_LGEO_PATH, QDir::cleanPath(Path));
			return QDir::cleanPath(Path);
		}

		QMessageBox::warning(Parent, Title, QCoreApplication::translate("swRenderDialog", "'%1' is not an LGEO part library, it has no lg_elements.lst and lg folder.").arg(QDir::toNativeSeparators(Path)));
	}

	QMessageBox::warning(Parent, Title, QCoreApplication::translate("swRenderDialog", "No LGEO part library was downloaded or set, the render uses LDraw parts.\n\nDownload it from %1 and set it under Preferences › General › LGEO Library.").arg(QLatin1String(SW_LGEO_PROJECT_URL)));

	return QString();
}

// use to explain a missing povray
QString swGetPOVRayMissingMessage(const swPOVRayProgram& Program)
{
	if (!Program.Version.isEmpty())
		return QCoreApplication::translate("swRenderDialog", "POV-Ray %1 at '%2' is too old, StudWorks needs POV-Ray 3.7 or newer.").arg(Program.Version, QDir::toNativeSeparators(Program.Path));

	return QCoreApplication::translate("swRenderDialog", "StudWorks could not find POV-Ray 3.7 or newer.");
}

// use to choose a povray program
swPOVRayProgram swSetupPOVRay(QWidget* Parent, const swPOVRayProgram& Found)
{
	const QString Title = QCoreApplication::translate("swRenderDialog", "POV-Ray");
	const QString Message = swGetPOVRayMissingMessage(Found) + QLatin1String("\n\n") + QCoreApplication::translate("swRenderDialog", "Install the povray package of your distribution, or POV-Ray from Flathub or the Snap Store, then choose its program.");

	QMessageBox MessageBox(QMessageBox::Question, Title, Message, QMessageBox::NoButton, Parent);
	QPushButton* ChooseButton = MessageBox.addButton(QCoreApplication::translate("swRenderDialog", "Choose..."), QMessageBox::AcceptRole);
	QPushButton* DownloadButton = MessageBox.addButton(QCoreApplication::translate("swRenderDialog", "Download Page"), QMessageBox::HelpRole);
	MessageBox.addButton(QMessageBox::Cancel);
	MessageBox.setDefaultButton(ChooseButton);
	MessageBox.exec();

	if (MessageBox.clickedButton() == DownloadButton)
		QDesktopServices::openUrl(QUrl(QLatin1String(SW_POVRAY_DOWNLOAD_URL)));
	else if (MessageBox.clickedButton() == ChooseButton)
	{
		for (;;)
		{
			const QString Path = QFileDialog::getOpenFileName(Parent, QCoreApplication::translate("swRenderDialog", "Open POV-Ray Program"), QLatin1String("/usr/bin"));

			if (Path.isEmpty())
				break;

			const swPOVRayProgram Program = swGetPOVRayProgram(Path);

			if (Program.Supported)
			{
				swSetProfileString(SW_PROFILE_POVRAY_PATH, Program.Path);
				return Program;
			}

			QMessageBox::warning(Parent, Title, Program.Version.isEmpty() ? QCoreApplication::translate("swRenderDialog", "'%1' is not a POV-Ray program.").arg(QDir::toNativeSeparators(Path)) : swGetPOVRayMissingMessage(Program));
		}
	}

	QMessageBox::warning(Parent, Title, QCoreApplication::translate("swRenderDialog", "No POV-Ray program was set.\n\nInstall POV-Ray 3.7 or newer from %1 and set it under Preferences › General › POV-Ray Executable.").arg(QLatin1String(SW_POVRAY_DOWNLOAD_URL)));

	return swPOVRayProgram();
}

// use to build shared render arguments
QStringList swGetPOVRayArguments(const swPOVRayProgram& Program, swPOVRayRenderQuality Quality, bool UseLGEO)
{
	QStringList Arguments;

	switch (Quality)
	{
	case swPOVRayRenderQuality::High:
		Arguments.append("+Q11");
		Arguments.append("+R3");
		Arguments.append("+A0.1");
		Arguments.append("+J0.5");
		break;

	case swPOVRayRenderQuality::Medium:
		Arguments.append("+Q5");
		Arguments.append("+A0.1");
		break;

	case swPOVRayRenderQuality::Low:
		break;
	}

#ifdef Q_OS_LINUX
	Arguments.append("+FN");
	Arguments.append("-D");
#endif

	const QString IncludePath = swGetPOVRayIncludePath(Program);

	if (!IncludePath.isEmpty())
		Arguments.append(QString("+L\"%1\"").arg(IncludePath));

	const QString IniPath = QDir::cleanPath(QFileInfo(Program.Path).absolutePath() + "/ini");

	if (QFileInfo::exists(IniPath))
		Arguments.append(QString("+L\"%1\"").arg(IniPath));

	if (UseLGEO)
	{
		const QString LGEOPath = swFindLGEOPath();

		for (const QString& Folder : { QLatin1String("lg"), QLatin1String("ar"), QLatin1String("stl") })
		{
			const QString FolderPath = QDir::cleanPath(LGEOPath + QLatin1Char('/') + Folder);

			if (!LGEOPath.isEmpty() && QFileInfo::exists(FolderPath))
				Arguments.append(QString("+L\"%1\"").arg(FolderPath));
		}
	}

	return Arguments;
}

// use to keep the config notice out
QStringList swGetPOVRayEnvironment()
{
	QStringList Environment = QProcess::systemEnvironment();
	Environment.prepend("POV_IGNORE_SYSCONF_MSG=1");

	return Environment;
}
