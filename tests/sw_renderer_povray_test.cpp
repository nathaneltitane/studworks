#include "sw_global.h"
#include "sw_tests.h"

// Exports a one brick model with the StudWorks binary and renders it with POV-Ray, so a scene POV-Ray
// cannot parse fails here. SW_TEST_BINARY is the studworks program and SW_TEST_LIBRARY an optional
// parts library, without one the brick exports as a placeholder.
void swTests::POVRayRendersExport()
{
	const QProcessEnvironment Environment = QProcessEnvironment::systemEnvironment();
	const QString Binary = Environment.value("SW_TEST_BINARY");
	const QString Library = Environment.value("SW_TEST_LIBRARY");
	const QString POVRay = QStandardPaths::findExecutable("povray");

	if (Binary.isEmpty())
		QSKIP("SW_TEST_BINARY is not set");

	if (POVRay.isEmpty())
		QSKIP("POV-Ray is not installed");

	QTemporaryDir Folder;
	QVERIFY(Folder.isValid());

	QFile Model(Folder.filePath("model.ldr"));
	QVERIFY(Model.open(QIODevice::WriteOnly));
	Model.write("0 Test\r\n1 4 0 0 0 1 0 0 0 1 0 0 0 1 3001.dat\r\n");
	Model.close();

	// A throwaway config keeps the user's settings out of the export.
	QProcessEnvironment ExportEnvironment = Environment;
	ExportEnvironment.insert("XDG_CONFIG_HOME", Folder.filePath("config"));
	ExportEnvironment.insert("XDG_DATA_HOME", Folder.filePath("data"));
	ExportEnvironment.insert("XDG_CACHE_HOME", Folder.filePath("cache"));
	ExportEnvironment.insert("QT_QPA_PLATFORM", "offscreen");

	QStringList ExportArguments;

	if (!Library.isEmpty())
		ExportArguments << "-l" << Library;

	ExportArguments << Model.fileName() << "--export-povray" << Folder.filePath("model.pov");

	QProcess Export;
	Export.setProcessEnvironment(ExportEnvironment);
	Export.setProcessChannelMode(QProcess::MergedChannels);
	Export.start(Binary, ExportArguments);
	QVERIFY2(Export.waitForFinished(120000), "the export did not finish");
	QVERIFY2(Export.exitCode() == 0, Export.readAll().constData());
	QVERIFY(QFileInfo::exists(Folder.filePath("model.pov")));

	QProcess Render;
	Render.setWorkingDirectory(Folder.path());
	Render.setProcessChannelMode(QProcess::MergedChannels);
	Render.start(POVRay, { "-D", "+W64", "+H64", "+FN", "+Imodel.pov", "+Omodel.png" });
	QVERIFY2(Render.waitForFinished(120000), "POV-Ray did not finish");
	QVERIFY2(Render.exitCode() == 0, Render.readAll().right(600).constData());

	const QImage Image(Folder.filePath("model.png"));
	QCOMPARE(Image.size(), QSize(64, 64));
}
