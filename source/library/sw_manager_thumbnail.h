#pragma once

#include "sw_types_asset.h"

struct swPartThumbnail
{
	QPixmap Pixmap;
	swPartData* PartData;
	std::vector<swPartDataReference> RequiredPartData;
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
	swThumbnailManager(swPartsLibrary* Library);
	~swThumbnailManager();

	std::pair<swPartThumbnailId, QPixmap> RequestThumbnail(swPartData* PartData, int ColorIndex, int Size, float DeviceScale);
	void ReleaseThumbnail(swPartThumbnailId ThumbnailId);
	void Clear();
	void RefreshParts(const std::vector<swPartData*>& ChangedParts);

signals:
	void ThumbnailReady(swPartThumbnailId ThumbnailId, QPixmap Pixmap);

protected slots:
	void PartLoaded(swPartData* PartData);
	void PartLoadFailed(swPartData* PartData);
	void GeneratedMeshSettled(swPartData* PartData);

protected:
	void RefreshRequiredParts(swPartThumbnail& Thumbnail);
	void UpdateThumbnail(swPartThumbnailId ThumbnailId, swPartThumbnail& Thumbnail);
	void DrawThumbnail(swPartThumbnailId ThumbnailId, swPartThumbnail& Thumbnail);
	void DrawFailedThumbnail(swPartThumbnailId ThumbnailId, swPartThumbnail& Thumbnail);
	void SchedulePendingThumbnails();

	swPartsLibrary* mLibrary = nullptr;
	std::map<swPartThumbnailId, swPartThumbnail> mThumbnails;
	int mNextThumbnailId = 1;

	std::unique_ptr<swView> mView;
	std::unique_ptr<swModel> mModel;
	std::map<swPartThumbnailId, std::unique_ptr<swModel>> mPendingGeneratedModels;
	bool mRetryScheduled = false;
};
