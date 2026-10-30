#include "sw_global.h"
#include "sw_tests.h"
#include "sw_command_meta.h"

void swTests::CommandMetaReadsBothFormats()
{
	QCOMPARE(swReadCommandMeta("0 !SW PIVOT PART 1 0 0"), QString("0 !SW PART PIVOT 1 0 0"));
	QCOMPARE(swReadCommandMeta("0 !LEOCAD PIECE PIVOT 1 0 0"), QString("0 !SW PART PIVOT 1 0 0"));
	QCOMPARE(swReadCommandMeta("0 !LEOCAD CAMERA NAME Front"), QString("0 !SW CAMERA LABEL Front"));
	QCOMPARE(swReadCommandMeta("0 !LEOCAD LIGHT COLOR_KEY 2 1 1 1"), QString("0 !SW LIGHT COLOR_RGB_KEY 2 1 1 1"));
	QCOMPARE(swReadCommandMeta("0 !LEOCAD SYNTH CONTROL_POINT 1 0 0"), QString("0 !SW SYNTH_PATH CONTROL_POINT 1 0 0"));
	QCOMPARE(swReadCommandMeta("0 !LEOCAD POV_RAY HEADER_INCLUDE_FILE a.inc"), QString("0 !SW POV_RAY HEADER_FILE_INCLUDE a.inc"));

	// Other lines pass unchanged.
	QCOMPARE(swReadCommandMeta("0 !COLOUR Black CODE 0"), QString("0 !COLOUR Black CODE 0"));
	QCOMPARE(swReadCommandMeta("0 !SW PIVOT UNKNOWN 1"), QString("0 !SW PIVOT UNKNOWN 1"));
	QCOMPARE(swReadCommandMeta("1 16 0 0 0 1 0 0 0 1 0 0 0 1 3001.dat"), QString("1 16 0 0 0 1 0 0 0 1 0 0 0 1 3001.dat"));
}

void swTests::CommandMetaWritesBothFormats()
{
	const QByteArray LeoCAD =
		"0 FILE main.ldr\r\n"
		"0 !LEOCAD GROUP BEGIN Wall\r\n"
		"  0 !LEOCAD PIECE PIVOT 1 0 0 0 1 0 0 0 1 0 0 0\r\n"
		"1 16 0 0 0 1 0 0 0 1 0 0 0 1 3001.dat\r\n"
		"0 !LEOCAD PIECE HIDDEN\n"
		"0 !LEOCAD CAMERA ROTATION_KEY 1 0 0 0\r\n"
		"0 !LEOCAD LIGHT COLOR 1 1 1\r\n"
		"0 !LEOCAD GROUP END\r\n"
		"0 // comment\r\n"
		"0 NOFILE";
	const QByteArray StudWorx =
		"0 FILE main.ldr\r\n"
		"0 !SW BEGIN GROUP Wall\r\n"
		"  0 !SW PIVOT PART 1 0 0 0 1 0 0 0 1 0 0 0\r\n"
		"1 16 0 0 0 1 0 0 0 1 0 0 0 1 3001.dat\r\n"
		"0 !SW HIDDEN PART\n"
		"0 !SW ORIENTATION_KEY CAMERA 1 0 0 0\r\n"
		"0 !SW COLOR_RGB LIGHT 1 1 1\r\n"
		"0 !SW END GROUP\r\n"
		"0 // comment\r\n"
		"0 NOFILE";

	QVERIFY(swHasLeoCADCommandMeta(LeoCAD));
	QVERIFY(!swHasLeoCADCommandMeta(StudWorx));
	QCOMPARE(swWriteCommandMeta(LeoCAD, swCommandMetaFormat::StudWorx), StudWorx);
	QCOMPARE(swWriteCommandMeta(StudWorx, swCommandMetaFormat::LeoCAD), LeoCAD);
	QCOMPARE(swWriteCommandMeta(StudWorx, swCommandMetaFormat::StudWorx), StudWorx);
}

void swTests::CommandMetaRoundTripsModel()
{
	const QString FileName = QProcessEnvironment::systemEnvironment().value("SW_TEST_MODEL");

	if (FileName.isEmpty())
		QSKIP("SW_TEST_MODEL is not set");

	QFile File(FileName);
	QVERIFY(File.open(QIODevice::ReadOnly));
	const QByteArray LeoCAD = File.readAll();
	const QByteArray StudWorx = swWriteCommandMeta(LeoCAD, swCommandMetaFormat::StudWorx);

	QVERIFY(!swHasLeoCADCommandMeta(StudWorx));
	QCOMPARE(StudWorx.count("0 !SW "), LeoCAD.count("0 !LEOCAD "));
	QCOMPARE(swWriteCommandMeta(StudWorx, swCommandMetaFormat::LeoCAD), LeoCAD);
}
