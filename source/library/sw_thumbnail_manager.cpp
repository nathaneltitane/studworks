#include "sw_global.h"
#include "sw_thumbnail_manager.h"
#include "sw_library.h"
#include "sw_piece_data.h"
#include "sw_view.h"
#include "sw_model.h"
#include "sw_piece.h"
#include "sw_project.h"
#include "sw_camera.h"

swThumbnailManager::swThumbnailManager(swPiecesLibrary* Library)
	: QObject(Library), mLibrary(Library)
{
	connect(mLibrary, &swPiecesLibrary::PartLoaded, this, &swThumbnailManager::PartLoaded);
	connect(mLibrary, &swPiecesLibrary::PartLoadFailed, this, &swThumbnailManager::PartLoadFailed);
	connect(mLibrary, &swPiecesLibrary::GeneratedMeshSettled, this, &swThumbnailManager::GeneratedMeshSettled);
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

void swThumbnailManager::RefreshPieces(const std::vector<swPieceData*>& ChangedPieces)
{
	const std::unordered_set<swPieceData*> Changed(ChangedPieces.begin(), ChangedPieces.end());
	std::vector<swPartThumbnailId> ThumbnailIds;

	for (std::map<swPartThumbnailId, swPartThumbnail>::value_type& Entry : mThumbnails)
	{
		const swPartThumbnail& Thumbnail = Entry.second;
		const bool RequiredChanged = std::any_of(Thumbnail.RequiredPieceData.begin(), Thumbnail.RequiredPieceData.end(), [&Changed](const swPieceDataReference& RequiredData)
		{
			return Changed.find(RequiredData.Get()) != Changed.end();
		});

		if (Changed.find(Thumbnail.PieceData) == Changed.end() && !RequiredChanged)
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

std::pair<swPartThumbnailId, QPixmap> swThumbnailManager::RequestThumbnail(swPieceData* PieceData, int ColorIndex, int Size, float DeviceScale)
{
	for (auto &[ThumbnailId, Thumbnail] : mThumbnails)
		if (Thumbnail.PieceData == PieceData && Thumbnail.ColorIndex == ColorIndex && Thumbnail.Size == Size && Thumbnail.DeviceScale == DeviceScale)
		{
			Thumbnail.ReferenceCount++;

			return { ThumbnailId, Thumbnail.Pixmap };
		}

	swPartThumbnailId ThumbnailId = static_cast<swPartThumbnailId>(mNextThumbnailId++);
	swPartThumbnail& Thumbnail = mThumbnails[ThumbnailId];

	Thumbnail.PieceData = PieceData;
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

void swThumbnailManager::RefreshRequiredPieces(swPartThumbnail& Thumbnail)
{
	std::vector<swPieceData*> Required{ Thumbnail.PieceData };

	if (Thumbnail.PieceData->IsModel())
	{
		std::vector<swPieceDataReference> ModelPieceData = Thumbnail.PieceData->GetModel()->GetRequiredPieceData();
		for (const swPieceDataReference& ModelData : ModelPieceData)
			Required.push_back(ModelData.Get());
	}
	else if (Thumbnail.PieceData->IsProject())
	{
		const swModel* Model = Thumbnail.PieceData->GetProject()->GetMainModel();

		if (Model)
		{
			std::vector<swPieceDataReference> ModelPieceData = Model->GetRequiredPieceData();
			for (const swPieceDataReference& ModelData : ModelPieceData)
				Required.push_back(ModelData.Get());
		}
	}

	std::sort(Required.begin(), Required.end());
	Required.erase(std::unique(Required.begin(), Required.end()), Required.end());

	std::unordered_map<swPieceData*, size_t> Previous;

	for (size_t Index = 0; Index < Thumbnail.RequiredPieceData.size(); Index++)
		Previous.emplace(Thumbnail.RequiredPieceData[Index].Get(), Index);

	std::vector<swPieceDataReference> RequiredPieceData;
	RequiredPieceData.reserve(Required.size());

	for (swPieceData* PieceData : Required)
	{
		const auto It = Previous.find(PieceData);

		if (It != Previous.end())
			RequiredPieceData.push_back(std::move(Thumbnail.RequiredPieceData[It->second]));
		else
		{
			RequiredPieceData.emplace_back(PieceData);
			mLibrary->RequestPieceDataLoad(PieceData, swLoadPriority::Low);
		}
	}

	Thumbnail.RequiredPieceData = std::move(RequiredPieceData);
}

void swThumbnailManager::UpdateThumbnail(swPartThumbnailId ThumbnailId, swPartThumbnail& Thumbnail)
{
	RefreshRequiredPieces(Thumbnail);

	bool Pending = false;

	for (const swPieceDataReference& RequiredData : Thumbnail.RequiredPieceData)
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

	if (Thumbnail.PieceData->IsModel())
	{
		std::vector<swModel*> UpdatedModels;
		Thumbnail.PieceData->GetModel()->UpdatePieceData(UpdatedModels);
	}
	else if (Thumbnail.PieceData->IsProject())
		Thumbnail.PieceData->GetProject()->UpdatePieceData(Thumbnail.PieceData);

	DrawThumbnail(ThumbnailId, Thumbnail);
}

void swThumbnailManager::PartLoaded(swPieceData* PieceData)
{
	std::vector<swPartThumbnailId> Ready;

	for (const auto& [ThumbnailId, Thumbnail] : mThumbnails)
		if (Thumbnail.Pixmap.isNull() && std::any_of(Thumbnail.RequiredPieceData.begin(), Thumbnail.RequiredPieceData.end(), [PieceData](const swPieceDataReference& RequiredData)
		{
			return RequiredData == PieceData;
		}))
			Ready.push_back(ThumbnailId);

	for (swPartThumbnailId ThumbnailId : Ready)
	{
		const auto It = mThumbnails.find(ThumbnailId);

		if (It != mThumbnails.end() && It->second.Pixmap.isNull())
			UpdateThumbnail(ThumbnailId, It->second);
	}
}

void swThumbnailManager::PartLoadFailed(swPieceData* PieceData)
{
	std::vector<swPartThumbnailId> Failed;

	for (const auto& [ThumbnailId, Thumbnail] : mThumbnails)
		if (Thumbnail.Pixmap.isNull() && std::any_of(Thumbnail.RequiredPieceData.begin(), Thumbnail.RequiredPieceData.end(), [PieceData](const swPieceDataReference& RequiredData)
		{
			return RequiredData == PieceData;
		}))
			Failed.push_back(ThumbnailId);

	for (swPartThumbnailId ThumbnailId : Failed)
	{
		const auto It = mThumbnails.find(ThumbnailId);

		if (It != mThumbnails.end() && It->second.Pixmap.isNull())
			UpdateThumbnail(ThumbnailId, It->second);
	}
}

void swThumbnailManager::GeneratedMeshSettled(swPieceData*)
{
	std::vector<swPartThumbnailId> Ready;

	for (const std::map<swPartThumbnailId, std::unique_ptr<swModel>>::value_type& Entry : mPendingGeneratedModels)
	{
		bool Pending = false;

		for (const swPiece* Piece : Entry.second->GetRequiredSynthPieces())
			if (Piece->IsGeneratedMeshPending())
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

	Thumbnail.RequiredPieceData.clear();

	emit ThumbnailReady(ThumbnailId, Thumbnail.Pixmap);
	SchedulePendingThumbnails();
}

void swThumbnailManager::DrawThumbnail(swPartThumbnailId ThumbnailId, swPartThumbnail& Thumbnail)
{
	const std::map<swPartThumbnailId, std::unique_ptr<swModel>>::iterator PendingIt = mPendingGeneratedModels.find(ThumbnailId);
	const bool ReuseGeneratedPiece = PendingIt != mPendingGeneratedModels.end();

	if (ReuseGeneratedPiece)
	{
		for (const swPiece* Piece : PendingIt->second->GetRequiredSynthPieces())
			if (Piece->IsGeneratedMeshPending())
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

	swPieceData* PieceData = Thumbnail.PieceData;

	if (!ReuseGeneratedPiece)
	{
		mModel->SetPreviewPieceData(PieceData, Thumbnail.ColorIndex);

		bool GeneratedPiecePending = false;

		for (const swPiece* Piece : mModel->GetRequiredSynthPieces())
			if (Piece->IsGeneratedMeshPending())
			{
				GeneratedPiecePending = true;
				break;
			}

		if (GeneratedPiecePending)
		{
			mView->UnbindRenderFramebuffer();
			mView.reset();
			mPendingGeneratedModels.emplace(ThumbnailId, std::move(mModel));
			return;
		}
	}

	const swVector3 Center = (PieceData->GetBoundingBox().Min + PieceData->GetBoundingBox().Max) / 2.0f;
	const swVector3 Position = Center + swVector3(100.0f, -100.0f, 75.0f);

	mView->GetCamera()->SetViewpoint(Position, Center, swVector3(0, 0, 1));
	mView->GetCamera()->m_fovy = 20.0f;
	mView->ZoomExtents(swGeometryBoundsMode::IncludeFallback, false);

	mView->OnDraw();

	mView->UnbindRenderFramebuffer();

	if (mView->HasMissingAssets())
	{
		RefreshRequiredPieces(Thumbnail);

		for (const swPieceDataReference& RequiredData : Thumbnail.RequiredPieceData)
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

	const char* IconName = nullptr;

	if (PieceData->GetSynthInfo())
		IconName = ":/resources/part_flexible.png";
	else if (PieceData->GetTrainTrackInfo())
		IconName = ":/resources/part_traintrack.png";

	if (IconName)
	{
		QPainter Painter(&Image);
		QImage Icon = QImage(IconName);
		uchar* ImageBits = Icon.bits();
		QRgb TextColor = QApplication::palette().color(QPalette::WindowText).rgba();
		int Red = qRed(TextColor);
		int Green = qGreen(TextColor);
		int Blue = qBlue(TextColor);

		for (int y = 0; y < Icon.height(); y++)
		{
			for (int x = 0; x < Icon.width(); x++)
			{
				QRgb& Pixel = ((QRgb*)ImageBits)[x];
				Pixel = qRgba(Red, Green, Blue, qAlpha(Pixel));
			}

			ImageBits += Icon.bytesPerLine();
		}

		Painter.setRenderHint(QPainter::SmoothPixmapTransform);
		Painter.scale(Thumbnail.DeviceScale, Thumbnail.DeviceScale);
		Painter.drawImage(QPoint(0, 0), Icon);
		Painter.end();
	}

	Image.setDevicePixelRatio(Thumbnail.DeviceScale);

	float ScaledSize = Thumbnail.Size * Thumbnail.DeviceScale;

	Thumbnail.Pixmap = QPixmap::fromImage(Image).scaled(ScaledSize, ScaledSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

	Thumbnail.RequiredPieceData.clear();

	emit ThumbnailReady(ThumbnailId, Thumbnail.Pixmap);
	SchedulePendingThumbnails();
}
