#pragma once

#include "sw_assettypes.h"

struct swPartThumbnail
{
	QPixmap Pixmap;
	swPieceData* PieceData;
	std::vector<swPieceDataRef> RequiredPieceData;
	int ColorIndex;
	int Size;
	float DeviceScale;
	int ReferenceCount;
};

enum class swPartThumbnailId : uint32_t
{
	Invalid = 0
};

class swThumbnailManager : public QObject
{
	Q_OBJECT

public:
	swThumbnailManager(swPiecesLibrary* Library);
	~swThumbnailManager();

	std::pair<swPartThumbnailId, QPixmap> RequestThumbnail(swPieceData* PieceData, int ColorIndex, int Size, float DeviceScale);
	void ReleaseThumbnail(swPartThumbnailId ThumbnailId);
	void Clear();
	void RefreshPieces(const std::vector<swPieceData*>& ChangedPieces);

signals:
	void ThumbnailReady(swPartThumbnailId ThumbnailId, QPixmap Pixmap);

protected slots:
	void PartLoaded(swPieceData* PieceData);
	void PartLoadFailed(swPieceData* PieceData);
	void GeneratedMeshSettled(swPieceData* PieceData);

protected:
	void RefreshRequiredPieces(swPartThumbnail& Thumbnail);
	void UpdateThumbnail(swPartThumbnailId ThumbnailId, swPartThumbnail& Thumbnail);
	void DrawThumbnail(swPartThumbnailId ThumbnailId, swPartThumbnail& Thumbnail);
	void DrawFailedThumbnail(swPartThumbnailId ThumbnailId, swPartThumbnail& Thumbnail);
	void SchedulePendingThumbnails();

	swPiecesLibrary* mLibrary = nullptr;
	std::map<swPartThumbnailId, swPartThumbnail> mThumbnails;
	int mNextThumbnailId = 1;

	std::unique_ptr<swView> mView;
	std::unique_ptr<swModel> mModel;
	std::map<swPartThumbnailId, std::unique_ptr<swModel>> mPendingGeneratedModels;
	bool mRetryScheduled = false;
};
