#include "sw_tests.h"

void swTests::TranslucentImagesKeepColor_data()
{
	QTest::addColumn<int>("Samples");
	QTest::newRow("single-sample") << 1;
	QTest::newRow("multisample") << 4;
}

// Saves a Trans-Clear model with the StudWorx binary, once on a transparent background and once on
// an opaque one. Translucent pixels must stay white rather than darkened by the cleared background,
// and opaque images must keep the background color. SW_TEST_BINARY is the studworx program, and
// SW_TEST_NATIVE_DISPLAY=1 renders on the running display instead of offscreen.
void swTests::TranslucentImagesKeepColor()
{
	const QProcessEnvironment Environment = QProcessEnvironment::systemEnvironment();
	const QString Binary = Environment.value("SW_TEST_BINARY");

	if (Binary.isEmpty())
		QSKIP("SW_TEST_BINARY is not set");

	QFETCH(int, Samples);

	QTemporaryDir Folder;
	QVERIFY(Folder.isValid());

	QFile Model(Folder.filePath("model.ldr"));
	QVERIFY(Model.open(QIODevice::WriteOnly));
	Model.write("0 Test\r\n"
		"4 47 -20 0 -20 20 0 -20 20 0 20 -20 0 20\r\n"
		"4 47 -20 4 -20 20 4 -20 20 4 20 -20 4 20\r\n");
	Model.close();

	// A throwaway config sets the samples and keeps the user's settings out of the images.
	QVERIFY(QDir().mkpath(Folder.filePath("config/StudWorx")));
	QFile Config(Folder.filePath("config/StudWorx/StudWorx.conf"));
	QVERIFY(Config.open(QIODevice::WriteOnly));
	Config.write(QString("[Settings]\nAASamples=%1\n").arg(Samples).toLatin1());
	Config.close();

	QProcessEnvironment ImageEnvironment = Environment;
	ImageEnvironment.insert("XDG_CONFIG_HOME", Folder.filePath("config"));
	ImageEnvironment.insert("XDG_DATA_HOME", Folder.filePath("data"));
	ImageEnvironment.insert("XDG_CACHE_HOME", Folder.filePath("cache"));

	if (Environment.value("SW_TEST_NATIVE_DISPLAY") != QLatin1String("1"))
		ImageEnvironment.insert("QT_QPA_PLATFORM", "offscreen");

	for (const bool Transparent : { true, false })
	{
		const QString FileName = Folder.filePath(Transparent ? "transparent.png" : "opaque.png");
		QStringList Arguments = { Model.fileName(), "--shading", "flat", "--line-width", "0", "--camera-angles", "30", "45", "-w", "64", "-h", "64", "-i", FileName };

		if (Transparent)
			Arguments << "--transparent";

		QProcess Render;
		Render.setProcessEnvironment(ImageEnvironment);
		Render.setProcessChannelMode(QProcess::MergedChannels);
		Render.start(Binary, Arguments);
		QVERIFY2(Render.waitForFinished(120000), "the image was not saved");
		QVERIFY2(Render.exitCode() == 0, Render.readAll().constData());

		const QImage Image = QImage(FileName).convertToFormat(QImage::Format_ARGB32);
		QCOMPARE(Image.size(), QSize(64, 64));

		if (Transparent)
		{
			QCOMPARE(Image.pixel(0, 0), QRgb(0));
			int TranslucentPixels = 0;

			for (int Y = 0; Y < Image.height(); Y++)
				for (int X = 0; X < Image.width(); X++)
				{
					const QRgb Pixel = Image.pixel(X, Y);

					if (qAlpha(Pixel) <= 100 || qAlpha(Pixel) >= 240)
						continue;

					QVERIFY2(qRed(Pixel) > 245 && qGreen(Pixel) > 245 && qBlue(Pixel) > 245, qPrintable(QString("pixel %1,%2 is %3").arg(X).arg(Y).arg(Pixel, 8, 16, QLatin1Char('0'))));
					TranslucentPixels++;
				}

			QVERIFY(TranslucentPixels > 100);
		}
		else
		{
			// the default background is dark gray, never black
			const QRgb Corner = Image.pixel(0, 0);
			QCOMPARE(qAlpha(Corner), 255);
			QVERIFY(qRed(Corner) > 20 && qGreen(Corner) > 20 && qBlue(Corner) > 20);
		}
	}
}
