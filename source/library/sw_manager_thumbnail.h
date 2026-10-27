#pragma once

#include "sw_types_asset.h"

struct swPieceThumbnail
{
	QPixmap Pixmap;
	swPieceData* PieceData;
	std::vector<swPieceDataReference> RequiredPieceData;
	int ColorIndex;
	int Size;
	float DeviceScale;
	int ReferenceCount;
};

enum class swPieceThumbnailId : uint32_t
{
	Invalid = 0
};

class swThumbnailManager : public QObject
{
	Q_OBJECT

public:
	swThumbnailManager(swPiecesLibrary* Library);
	~swThumbnailManager();

	std::pair<swPieceThumbnailId, QPixmap> RequestThumbnail(swPieceData* PieceData, int ColorIndex, int Size, float DeviceScale);
	void ReleaseThumbnail(swPieceThumbnailId ThumbnailId);
	void Clear();
	void RefreshPieces(const std::vector<swPieceData*>& ChangedPieces);

signals:
	void ThumbnailReady(swPieceThumbnailId ThumbnailId, QPixmap Pixmap);

protected slots:
	void PartLoaded(swPieceData* PieceData);
	void PartLoadFailed(swPieceData* PieceData);
	void GeneratedMeshSettled(swPieceData* PieceData);

protected:
	void RefreshRequiredPieces(swPieceThumbnail& Thumbnail);
	void UpdateThumbnail(swPieceThumbnailId ThumbnailId, swPieceThumbnail& Thumbnail);
	void DrawThumbnail(swPieceThumbnailId ThumbnailId, swPieceThumbnail& Thumbnail);
	void DrawFailedThumbnail(swPieceThumbnailId ThumbnailId, swPieceThumbnail& Thumbnail);
	void SchedulePendingThumbnails();

	swPiecesLibrary* mLibrary = nullptr;
	std::map<swPieceThumbnailId, swPieceThumbnail> mThumbnails;
	int mNextThumbnailId = 1;

	std::unique_ptr<swView> mView;
	std::unique_ptr<swModel> mModel;
	std::map<swPieceThumbnailId, std::unique_ptr<swModel>> mPendingGeneratedModels;
	bool mRetryScheduled = false;
};
