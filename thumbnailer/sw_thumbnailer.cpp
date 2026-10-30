#include "sw_thumbnailer.h"
#include <KPluginFactory>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>

// large models load and render well within this
static const int gThumbnailTimeout = 30000;

class swThumbnailFactory : public KPluginFactory
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "org.kde.KPluginFactory" FILE "sw_thumbnailer.json")
	Q_INTERFACES(KPluginFactory)

public:
	swThumbnailFactory()
	{
		registerPlugin<swThumbnailCreator>();
	}
};

swThumbnailCreator::swThumbnailCreator(QObject* Parent, const QVariantList& Arguments)
	: KIO::ThumbnailCreator(Parent, Arguments)
{
}

KIO::ThumbnailResult swThumbnailCreator::create(const KIO::ThumbnailRequest& Request)
{
	const QString FileName = Request.url().toLocalFile();
	const QString Program = GetProgramPath();

	if (FileName.isEmpty() || Program.isEmpty() || !IsLDrawFile(FileName))
		return KIO::ThumbnailResult::fail();

	QTemporaryDir Directory;

	if (!Directory.isValid())
		return KIO::ThumbnailResult::fail();

	const QString ImageName = Directory.filePath(QStringLiteral("thumbnail.png"));
	const QSize Size = Request.targetSize().expandedTo(QSize(1, 1));

	// render without a window, the offscreen platform still needs the display for opengl
	QProcessEnvironment Environment = QProcessEnvironment::systemEnvironment();

	if (!qEnvironmentVariableIsEmpty("DISPLAY"))
		Environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));

	QProcess Process;
	Process.setProcessEnvironment(Environment);
	Process.setProcessChannelMode(QProcess::ForwardedErrorChannel);
	Process.setStandardOutputFile(QProcess::nullDevice());
	Process.start(Program, { QStringLiteral("--transparent"), QStringLiteral("-w"), QString::number(Size.width()), QStringLiteral("-h"), QString::number(Size.height()), QStringLiteral("-i"), ImageName, FileName });

	if (!Process.waitForFinished(gThumbnailTimeout))
	{
		Process.kill();
		Process.waitForFinished();

		return KIO::ThumbnailResult::fail();
	}

	if (Process.exitStatus() != QProcess::NormalExit || Process.exitCode() != 0)
		return KIO::ThumbnailResult::fail();

	const QImage Image(ImageName);

	if (Image.isNull())
		return KIO::ThumbnailResult::fail();

	return KIO::ThumbnailResult::pass(Image);
}

// use to skip data files sharing the dat extension, ldraw lines start with a line type from 0 to 5
bool swThumbnailCreator::IsLDrawFile(const QString& FileName)
{
	QFile File(FileName);

	if (!File.open(QIODevice::ReadOnly))
		return false;

	int LineCount = 0;

	while (!File.atEnd() && LineCount < 20)
	{
		QByteArray Line = File.readLine(4096).trimmed();

		if (LineCount == 0 && Line.startsWith("\xEF\xBB\xBF"))
			Line.remove(0, 3);

		if (Line.isEmpty())
			continue;

		const char LineType = Line.at(0);

		if (LineType < '0' || LineType > '5' || (Line.size() > 1 && Line.at(1) != ' ' && Line.at(1) != '\t'))
			return false;

		LineCount++;
	}

	return LineCount > 0;
}

// use to find the studworx installed beside this plugin, then any on the path
QString swThumbnailCreator::GetProgramPath()
{
	const QFileInfo Program(QStringLiteral(SW_THUMBNAILER_PROGRAM));

	if (Program.isExecutable())
		return Program.filePath();

	return QStandardPaths::findExecutable(QStringLiteral("studworx"));
}

#include "sw_thumbnailer.moc"
