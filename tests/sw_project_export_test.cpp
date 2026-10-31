#include "sw_tests.h"
#include <zlib.h>

void swTests::KMZKeepsVisibleSurfaces_data()
{
	QTest::addColumn<bool>("AllParts");
	QTest::addColumn<QString>("ArchiveName");
	QTest::addColumn<QString>("ModelName");
	QTest::newRow("visible-surfaces") << false << "model.kmz" << "model.dae";
	QTest::newRow("all-parts") << true << "spaced model.kmz" << "spaced_model.dae";
}

// Exports a red box around a hidden blue box to KMZ with the StudWorx binary. The archive must hold
// the kml index and the COLLADA model named after the archive with spaces as underscores, both with
// fixed dates, visible surfaces only must drop the blue box
// and merge the red faces into one mesh, and --kmz-all-parts must keep both. SW_TEST_BINARY is the
// studworx program, and SW_TEST_NATIVE_DISPLAY=1 renders on the running display instead of offscreen.
void swTests::KMZKeepsVisibleSurfaces()
{
	const QProcessEnvironment Environment = QProcessEnvironment::systemEnvironment();
	const QString Binary = Environment.value("SW_TEST_BINARY");

	if (Binary.isEmpty())
		QSKIP("SW_TEST_BINARY is not set");

	QFETCH(bool, AllParts);
	QFETCH(QString, ArchiveName);
	QFETCH(QString, ModelName);

	QTemporaryDir Folder;
	QVERIFY(Folder.isValid());

	const auto WriteBox = [](QFile& File, int ColorIndex, int HalfSize)
	{
		const int Faces[6][4] = { { 0, 1, 3, 2 }, { 4, 6, 7, 5 }, { 0, 4, 5, 1 }, { 2, 3, 7, 6 }, { 0, 2, 6, 4 }, { 1, 5, 7, 3 } };

		for (const auto& Face : Faces)
		{
			QString Line = QString("4 %1").arg(ColorIndex);

			for (const int Corner : Face)
				Line += QString(" %1 %2 %3").arg(Corner & 4 ? HalfSize : -HalfSize).arg(Corner & 2 ? HalfSize : -HalfSize).arg(Corner & 1 ? HalfSize : -HalfSize);

			File.write(Line.toLatin1() + "\r\n");
		}
	};

	QFile Model(Folder.filePath("model.ldr"));
	QVERIFY(Model.open(QIODevice::WriteOnly));
	Model.write("0 Test\r\n");
	WriteBox(Model, 4, 20);
	WriteBox(Model, 1, 5);
	Model.close();

	QProcessEnvironment ExportEnvironment = Environment;
	ExportEnvironment.insert("XDG_CONFIG_HOME", Folder.filePath("config"));
	ExportEnvironment.insert("XDG_DATA_HOME", Folder.filePath("data"));
	ExportEnvironment.insert("XDG_CACHE_HOME", Folder.filePath("cache"));

	if (Environment.value("SW_TEST_NATIVE_DISPLAY") != QLatin1String("1"))
		ExportEnvironment.insert("QT_QPA_PLATFORM", "offscreen");

	const QString FileName = Folder.filePath(ArchiveName);
	QStringList Arguments = { Model.fileName(), "-kmz", FileName };

	if (AllParts)
		Arguments << "--kmz-all-parts";

	QProcess Export;
	Export.setProcessEnvironment(ExportEnvironment);
	Export.setProcessChannelMode(QProcess::MergedChannels);
	Export.start(Binary, Arguments);
	QVERIFY2(Export.waitForFinished(120000), "the kmz was not saved");
	QVERIFY2(Export.exitCode() == 0, Export.readAll().constData());

	QFile Archive(FileName);
	QVERIFY(Archive.open(QIODevice::ReadOnly));
	const QByteArray Data = Archive.readAll();

	// local file headers in archive order
	QStringList Names;
	QMap<QString, QByteArray> Files;
	int Offset = 0;

	const auto ReadU16 = [&Data](int Position) { return qFromLittleEndian<quint16>(Data.constData() + Position); };
	const auto ReadU32 = [&Data](int Position) { return qFromLittleEndian<quint32>(Data.constData() + Position); };

	while (Offset + 30 <= Data.size() && ReadU32(Offset) == 0x04034b50)
	{
		QCOMPARE(ReadU16(Offset + 8), quint16(Z_DEFLATED));
		QCOMPARE(ReadU32(Offset + 10), quint32(((2000 - 1980) << 25) | (1 << 21) | (1 << 16)));

		const quint32 CompressedSize = ReadU32(Offset + 18);
		const quint32 Size = ReadU32(Offset + 22);
		const int NameLength = ReadU16(Offset + 26);
		const int DataOffset = Offset + 30 + NameLength + ReadU16(Offset + 28);
		const QString Name = QString::fromLatin1(Data.mid(Offset + 30, NameLength));

		QByteArray Contents(static_cast<int>(Size), Qt::Uninitialized);
		z_stream Stream = {};
		QVERIFY(inflateInit2(&Stream, -MAX_WBITS) == Z_OK);
		Stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(Data.constData() + DataOffset));
		Stream.avail_in = CompressedSize;
		Stream.next_out = reinterpret_cast<Bytef*>(Contents.data());
		Stream.avail_out = Size;
		const int Result = inflate(&Stream, Z_FINISH);
		inflateEnd(&Stream);
		QCOMPARE(Result, Z_STREAM_END);
		QCOMPARE(static_cast<quint32>(crc32(0, reinterpret_cast<const Bytef*>(Contents.constData()), Size)), ReadU32(Offset + 14));

		Names << Name;
		Files[Name] = Contents;
		Offset = DataOffset + static_cast<int>(CompressedSize);
	}

	QCOMPARE(Names, QStringList({ "doc.kml", ModelName }));
	QCOMPARE(Files["doc.kml"], QByteArray("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<kml><Placemark><Model><Link><href>") + ModelName.toUtf8() + QByteArray("</href></Link></Model></Placemark></kml>\n"));

	const QString COLLADA = QString::fromUtf8(Files[ModelName]);
	QVERIFY(COLLADA.contains("<created>2000-01-01T00:00:00</created>"));
	QVERIFY(COLLADA.contains("<modified>2000-01-01T00:00:00</modified>"));
	QVERIFY(COLLADA.contains("target=\"#Color_Red-material\""));

	if (AllParts)
	{
		QCOMPARE(COLLADA.count("<triangles count=\"12\""), 2);
		QVERIFY(COLLADA.contains("target=\"#Color_Blue-material\""));
	}
	else
	{
		QCOMPARE(COLLADA.count("<geometry "), 1);
		QVERIFY(COLLADA.contains("<triangles count=\"12\" material=\"surface\">"));
		QVERIFY(!COLLADA.contains("target=\"#Color_Blue-material\""));
	}
}
