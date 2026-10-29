#include "sw_global.h"
#include "sw_manager_thumbnail.h"
#include "sw_library.h"
#include "sw_data_part.h"
#include "sw_view.h"
#include "sw_model.h"
#include "sw_object_part.h"
#include "sw_project.h"
#include "sw_object_camera.h"
#include "sw_window_utilities.h"

swThumbnailManager::swThumbnailManager(swPartsLibrary* Library)
	: QObject(Library), mLibrary(Library)
{
	connect(mLibrary, &swPartsLibrary::PartLoaded, this, &swThumbnailManager::PartLoaded);
	connect(mLibrary, &swPartsLibrary::PartLoadFailed, this, &swThumbnailManager::PartLoadFailed);
	connect(mLibrary, &swPartsLibrary::GeneratedMeshSettled, this, &swThumbnailManager::GeneratedMeshSettled);
}

swThumbnailManager::~swThumbnailManager()
{
	Clear();
}

void swThumbnailManager::Clear()
{
	mView.reset();
	mModel.reset();
	mPendingGeneratedModels.clear();

	mThumbnails.clear();
}

void swThumbnailManager::RefreshParts(const std::vector<swPartData*>& ChangedParts)
{
	const std::unordered_set<swPartData*> Changed(ChangedParts.begin(), ChangedParts.end());
	std::vector<swPartThumbnailId> ThumbnailIds;

	for (std::map<swPartThumbnailId, swPartThumbnail>::value_type& Entry : mThumbnails)
	{
		const swPartThumbnail& Thumbnail = Entry.second;
		const bool RequiredChanged = std::any_of(Thumbnail.RequiredPartData.begin(), Thumbnail.RequiredPartData.end(), [&Changed](const swPartDataReference& RequiredData)
		{
			return Changed.find(RequiredData.Get()) != Changed.end();
		});

		if (Changed.find(Thumbnail.PartData) == Changed.end() && !RequiredChanged)
			continue;

		Entry.second.Pixmap = QPixmap();
		mPendingGeneratedModels.erase(Entry.first);
		ThumbnailIds.push_back(Entry.first);
	}

	for (swPartThumbnailId ThumbnailId : ThumbnailIds)
	{
		const std::map<swPartThumbnailId, swPartThumbnail>::iterator It = mThumbnails.find(ThumbnailId);

		if (It != mThumbnails.end())
			UpdateThumbnail(ThumbnailId, It->second);
	}
}

std::pair<swPartThumbnailId, QPixmap> swThumbnailManager::RequestThumbnail(swPartData* PartData, int ColorIndex, int Size, float DeviceScale)
{
	for (auto &[ThumbnailId, Thumbnail] : mThumbnails)
		if (Thumbnail.PartData == PartData && Thumbnail.ColorIndex == ColorIndex && Thumbnail.Size == Size && Thumbnail.DeviceScale == DeviceScale)
		{
			Thumbnail.ReferenceCount++;

			return { ThumbnailId, Thumbnail.Pixmap };
		}

	swPartThumbnailId ThumbnailId = static_cast<swPartThumbnailId>(mNextThumbnailId++);
	swPartThumbnail& Thumbnail = mThumbnails[ThumbnailId];

	Thumbnail.PartData = PartData;
	Thumbnail.ColorIndex = ColorIndex;
	Thumbnail.Size = Size;
	Thumbnail.DeviceScale = DeviceScale;
	Thumbnail.ReferenceCount = 1;

	UpdateThumbnail(ThumbnailId, Thumbnail);

	return { ThumbnailId, Thumbnail.Pixmap };
}

void swThumbnailManager::ReleaseThumbnail(swPartThumbnailId ThumbnailId)
{
	auto ThumbnailIt = mThumbnails.find(ThumbnailId);

	if (ThumbnailIt == mThumbnails.end())
		return;

	swPartThumbnail& Thumbnail = ThumbnailIt->second;

	Thumbnail.ReferenceCount--;

	if (Thumbnail.ReferenceCount == 0)
	{
		mPendingGeneratedModels.erase(ThumbnailId);

		mThumbnails.erase(ThumbnailIt);
	}
}

void swThumbnailManager::RefreshRequiredParts(swPartThumbnail& Thumbnail)
{
	std::vector<swPartData*> Required{ Thumbnail.PartData };

	if (Thumbnail.PartData->IsModel())
	{
		std::vector<swPartDataReference> ModelPartData = Thumbnail.PartData->GetModel()->GetRequiredPartData();
		for (const swPartDataReference& ModelData : ModelPartData)
			Required.push_back(ModelData.Get());
	}
	else if (Thumbnail.PartData->IsProject())
	{
		const swModel* Model = Thumbnail.PartData->GetProject()->GetMainModel();

		if (Model)
		{
			std::vector<swPartDataReference> ModelPartData = Model->GetRequiredPartData();
			for (const swPartDataReference& ModelData : ModelPartData)
				Required.push_back(ModelData.Get());
		}
	}

	std::sort(Required.begin(), Required.end());
	Required.erase(std::unique(Required.begin(), Required.end()), Required.end());

	std::unordered_map<swPartData*, size_t> Previous;

	for (size_t Index = 0; Index < Thumbnail.RequiredPartData.size(); Index++)
		Previous.emplace(Thumbnail.RequiredPartData[Index].Get(), Index);

	std::vector<swPartDataReference> RequiredPartData;
	RequiredPartData.reserve(Required.size());

	for (swPartData* PartData : Required)
	{
		const auto It = Previous.find(PartData);

		if (It != Previous.end())
			RequiredPartData.push_back(std::move(Thumbnail.RequiredPartData[It->second]));
		else
		{
			RequiredPartData.emplace_back(PartData);
			mLibrary->RequestPartDataLoad(PartData, swLoadPriority::Low);
		}
	}

	Thumbnail.RequiredPartData = std::move(RequiredPartData);
}

void swThumbnailManager::UpdateThumbnail(swPartThumbnailId ThumbnailId, swPartThumbnail& Thumbnail)
{
	RefreshRequiredParts(Thumbnail);

	bool Pending = false;

	for (const swPartDataReference& RequiredData : Thumbnail.RequiredPartData)
	{
		if (RequiredData->mState == swAssetState::Failed || RequiredData->mState == swAssetState::Unloaded)
		{
			DrawFailedThumbnail(ThumbnailId, Thumbnail);
			return;
		}

		if (RequiredData->mState != swAssetState::Loaded)
			Pending = true;
	}

	if (Pending)
		return;

	if (Thumbnail.PartData->IsModel())
	{
		std::vector<swModel*> UpdatedModels;
		Thumbnail.PartData->GetModel()->UpdatePartData(UpdatedModels);
	}
	else if (Thumbnail.PartData->IsProject())
		Thumbnail.PartData->GetProject()->UpdatePartData(Thumbnail.PartData);

	DrawThumbnail(ThumbnailId, Thumbnail);
}

void swThumbnailManager::PartLoaded(swPartData* PartData)
{
	std::vector<swPartThumbnailId> Ready;

	for (const auto& [ThumbnailId, Thumbnail] : mThumbnails)
		if (Thumbnail.Pixmap.isNull() && std::any_of(Thumbnail.RequiredPartData.begin(), Thumbnail.RequiredPartData.end(), [PartData](const swPartDataReference& RequiredData)
		{
			return RequiredData == PartData;
		}))
			Ready.push_back(ThumbnailId);

	for (swPartThumbnailId ThumbnailId : Ready)
	{
		const auto It = mThumbnails.find(ThumbnailId);

		if (It != mThumbnails.end() && It->second.Pixmap.isNull())
			UpdateThumbnail(ThumbnailId, It->second);
	}
}

void swThumbnailManager::PartLoadFailed(swPartData* PartData)
{
	std::vector<swPartThumbnailId> Failed;

	for (const auto& [ThumbnailId, Thumbnail] : mThumbnails)
		if (Thumbnail.Pixmap.isNull() && std::any_of(Thumbnail.RequiredPartData.begin(), Thumbnail.RequiredPartData.end(), [PartData](const swPartDataReference& RequiredData)
		{
			return RequiredData == PartData;
		}))
			Failed.push_back(ThumbnailId);

	for (swPartThumbnailId ThumbnailId : Failed)
	{
		const auto It = mThumbnails.find(ThumbnailId);

		if (It != mThumbnails.end() && It->second.Pixmap.isNull())
			UpdateThumbnail(ThumbnailId, It->second);
	}
}

void swThumbnailManager::GeneratedMeshSettled(swPartData*)
{
	std::vector<swPartThumbnailId> Ready;

	for (const std::map<swPartThumbnailId, std::unique_ptr<swModel>>::value_type& Entry : mPendingGeneratedModels)
	{
		bool Pending = false;

		for (const swPart* Part : Entry.second->GetRequiredSynthParts())
			if (Part->IsGeneratedMeshPending())
			{
				Pending = true;
				break;
			}

		if (!Pending)
			Ready.push_back(Entry.first);
	}

	for (swPartThumbnailId ThumbnailId : Ready)
	{
		const std::map<swPartThumbnailId, swPartThumbnail>::iterator It = mThumbnails.find(ThumbnailId);

		if (It != mThumbnails.end())
			UpdateThumbnail(It->first, It->second);
	}
}

void swThumbnailManager::SchedulePendingThumbnails()
{
	if (mRetryScheduled)
		return;

	mRetryScheduled = true;

	QTimer::singleShot(0, this, [this]()
	{
		mRetryScheduled = false;
		std::vector<swPartThumbnailId> Pending;

		for (const std::map<swPartThumbnailId, swPartThumbnail>::value_type& Entry : mThumbnails)
			if (Entry.second.Pixmap.isNull())
				Pending.push_back(Entry.first);

		for (swPartThumbnailId ThumbnailId : Pending)
		{
			const std::map<swPartThumbnailId, swPartThumbnail>::iterator It = mThumbnails.find(ThumbnailId);

			if (It != mThumbnails.end() && It->second.Pixmap.isNull())
				UpdateThumbnail(It->first, It->second);
		}
	});
}

void swThumbnailManager::DrawFailedThumbnail(swPartThumbnailId ThumbnailId, swPartThumbnail& Thumbnail)
{
	mPendingGeneratedModels.erase(ThumbnailId);

	const int Size = qMax(1, static_cast<int>(Thumbnail.Size * Thumbnail.DeviceScale));

	Thumbnail.Pixmap = QPixmap(Size, Size);
	Thumbnail.Pixmap.fill(Qt::transparent);

	QPainter Painter(&Thumbnail.Pixmap);
	QPen Pen(QColor(180, 40, 40));

	Pen.setWidth(qMax(2, Size / 12));
	Painter.setPen(Pen);

	const int Margin = qMax(3, Size / 5);

	Painter.drawLine(Margin, Margin, Size - Margin, Size - Margin);
	Painter.drawLine(Size - Margin, Margin, Margin, Size - Margin);
	Painter.end();

	Thumbnail.Pixmap.setDevicePixelRatio(Thumbnail.DeviceScale);

	Thumbnail.RequiredPartData.clear();

	emit ThumbnailReady(ThumbnailId, Thumbnail.Pixmap);
	SchedulePendingThumbnails();
}

void swThumbnailManager::DrawThumbnail(swPartThumbnailId ThumbnailId, swPartThumbnail& Thumbnail)
{
	const std::map<swPartThumbnailId, std::unique_ptr<swModel>>::iterator PendingIt = mPendingGeneratedModels.find(ThumbnailId);
	const bool ReuseGeneratedPart = PendingIt != mPendingGeneratedModels.end();

	if (ReuseGeneratedPart)
	{
		for (const swPart* Part : PendingIt->second->GetRequiredSynthParts())
			if (Part->IsGeneratedMeshPending())
				return;

		mView.reset();
		mModel = std::move(PendingIt->second);
		mPendingGeneratedModels.erase(PendingIt);
	}

	const int Width = Thumbnail.Size * 2 * Thumbnail.DeviceScale;
	const int Height = Thumbnail.Size * 2 * Thumbnail.DeviceScale;

	if (mView && mView->GetRenderImage().size() != QSize(Width, Height))
		mView.reset();

	if (!mView)
	{
		if (!mModel)
			mModel = std::unique_ptr<swModel>(new swModel(QString(), nullptr, true));
		mView = std::unique_ptr<swView>(new swView(swViewType::PartsList, mModel.get()));

		mView->SetOffscreenContext();
		mView->MakeCurrent();
		mView->SetSize(Width, Height);

		if (!mView->BeginRenderToImage(Width, Height))
		{
			mView.reset();
			DrawFailedThumbnail(ThumbnailId, Thumbnail);
			return;
		}
	}

	mView->MakeCurrent();
	mView->BindRenderFramebuffer();

	const uint BackgroundColor = QApplication::palette().color(QPalette::Base).rgba();
	mView->SetBackgroundColorOverride(SW_RGBA(qRed(BackgroundColor), qGreen(BackgroundColor), qBlue(BackgroundColor), 0));

	swPartData* PartData = Thumbnail.PartData;

	if (!ReuseGeneratedPart)
	{
		mModel->SetPreviewPartData(PartData, Thumbnail.ColorIndex);

		bool GeneratedPartPending = false;

		for (const swPart* Part : mModel->GetRequiredSynthParts())
			if (Part->IsGeneratedMeshPending())
			{
				GeneratedPartPending = true;
				break;
			}

		if (GeneratedPartPending)
		{
			mView->UnbindRenderFramebuffer();
			mView.reset();
			mPendingGeneratedModels.emplace(ThumbnailId, std::move(mModel));
			return;
		}
	}

	const swVector3 Center = (PartData->GetBoundingBox().Min + PartData->GetBoundingBox().Max) / 2.0f;
	const swVector3 Position = Center + swVector3(100.0f, -100.0f, 75.0f);

	mView->GetCamera()->SetViewpoint(Position, Center, swVector3(0, 0, 1));
	mView->GetCamera()->m_fovy = 20.0f;
	mView->ZoomExtents(swGeometryBoundsMode::IncludeFallback, false);

	mView->OnDraw();

	mView->UnbindRenderFramebuffer();

	if (mView->HasMissingAssets())
	{
		RefreshRequiredParts(Thumbnail);

		for (const swPartDataReference& RequiredData : Thumbnail.RequiredPartData)
			if (RequiredData->IsLoading())
				return;

		DrawFailedThumbnail(ThumbnailId, Thumbnail);
		return;
	}

	QImage Image = mView->GetRenderImage().convertToFormat(QImage::Format_ARGB32);

	if (Image.isNull())
	{
		DrawFailedThumbnail(ThumbnailId, Thumbnail);
		return;
	}

	// flexible parts and train tracks are marked in the top left corner in the text color
	std::optional<swPaintedIcon> Icon;

	if (PartData->GetSynthInfo())
		Icon = swPaintedIcon::ThumbnailFlexible;
	else if (PartData->GetTrainTrackInfo())
		Icon = swPaintedIcon::ThumbnailTrackTrain;

	if (Icon.has_value())
	{
		QPainter Painter(&Image);
		Painter.drawImage(QPoint(0, 0), swCreatePaintedIconImage(Icon.value(), qRound(32 * Thumbnail.DeviceScale), QApplication::palette().color(QPalette::WindowText)));
		Painter.end();
	}

	Image.setDevicePixelRatio(Thumbnail.DeviceScale);

	float ScaledSize = Thumbnail.Size * Thumbnail.DeviceScale;

	Thumbnail.Pixmap = QPixmap::fromImage(Image).scaled(ScaledSize, ScaledSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

	Thumbnail.RequiredPartData.clear();

	emit ThumbnailReady(ThumbnailId, Thumbnail.Pixmap);
	SchedulePendingThumbnails();
}
