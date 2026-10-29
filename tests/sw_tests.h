#pragma once

#include <QtTest/QtTest>

class swTests : public QObject
{
	Q_OBJECT

private slots:
	void PropertyKeysAffectEquality();
	void LastKeyFrameTracksTimeEdits();
	void HistoryStoresOnlyChangedObjects();
	void HistoryCancelsPendingEdit();
	void HistoryReplaysAndMergesEdits();
	void HistoryDropsRevertedMerge();
	void CommandMetaReadsBothFormats();
	void CommandMetaWritesBothFormats();
	void CommandMetaRoundTripsModel();
	void ShadowParsesLines();
	void ShadowParsesIncludesAndClears();
	void ShadowReadsSubFileReferences();
	void ShadowScalesAndMirrorsSubFileShapes();
	void ConnectivityFitsShapesAlongTheirAxis();
	void ConnectivityConnectsSeatedShapes();
};
