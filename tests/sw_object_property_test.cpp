#include "sw_global.h"
#include "sw_tests.h"
#include "sw_object_property.h"

void swTests::LastKeyFrameTracksTimeEdits()
{
	swObjectProperty<float> Position(0.0f);
	QCOMPARE(Position.GetLastKeyFrame(), swStep(0));

	// Animation can extend past the step that introduces the piece.
	QVERIFY(Position.ChangeKey(0.0f, 1, true));
	QVERIFY(Position.ChangeKey(200.0f, 2, true));
	QCOMPARE(Position.GetLastKeyFrame(), swStep(2));
	Position.Update(Position.GetLastKeyFrame());
	QCOMPARE(static_cast<const float&>(Position), 200.0f);

	Position.InsertTime(2, 3);
	QCOMPARE(Position.GetLastKeyFrame(), swStep(5));
	Position.RemoveTime(2, 2);
	QCOMPARE(Position.GetLastKeyFrame(), swStep(3));

	QVERIFY(Position.SetKeyFrame(3, false));
	QCOMPARE(Position.GetLastKeyFrame(), swStep(1));
	Position.Update(Position.GetLastKeyFrame());
	QCOMPARE(static_cast<const float&>(Position), 0.0f);

	Position.RemoveAllKeys();
	QCOMPARE(Position.GetLastKeyFrame(), swStep(0));
}
