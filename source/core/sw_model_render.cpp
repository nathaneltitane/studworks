#include "sw_global.h"
#include "sw_model.h"
#include "sw_model_history.h"
#include "sw_object_piece.h"
#include "sw_object_camera.h"
#include "sw_object_light.h"
#include "sw_group.h"
#include "sw_project.h"
#include "sw_window_main.h"
#include "sw_profile.h"
#include "sw_library.h"
#include "sw_scene.h"
#include "sw_track_train.h"
#include "sw_file.h"
#include "sw_data_piece.h"
#include "sw_view.h"
#include "sw_builder_minifigure.h"
#include "sw_dialog_array.h"
#include "sw_dialog_group.h"
#include "sw_dialog_groups_edit.h"
#include "sw_dialog_properties.h"
#include "sw_window_utilities.h"
#include "sw_lxf.h"
#include "sw_widget_preview.h"
#include "sw_string.h"
#include <locale.h>

void swModel::GetScene(swScene* Scene, const swCamera* ViewCamera, bool AllowHighlight, bool AllowFade) const
{
	if (mPieceData)
		mPieceData->AddRenderMesh(*Scene);

	swPiece* FocusPiece = nullptr;

	for (const std::unique_ptr<swPiece>& Piece : mPieces)
	{
		if (Piece->IsSelected() || Piece->IsVisible(mCurrentStep))
		{
			if (Piece->IsFocused())
				FocusPiece = Piece.get();

			const swStep StepShow = Piece->GetStepShow();
			Piece->AddMainModelRenderMeshes(Scene, AllowHighlight && StepShow == mCurrentStep, AllowFade && StepShow < mCurrentStep);
		}
	}

	if (Scene->GetDrawInterface() && !Scene->GetActiveSubmodelInstance())
	{
		if (FocusPiece)
			UpdateTrainTrackConnections(FocusPiece, false);

		for (const std::unique_ptr<swCamera>& Camera : mCameras)
			if (Camera.get() != ViewCamera && Camera->IsVisible())
				Scene->AddInterfaceObject(Camera.get());

		for (const std::unique_ptr<swLight>& Light : mLights)
			if (Light->IsVisible())
				Scene->AddInterfaceObject(Light.get());
	}

	if (Scene->GetDrawInsertPreview())
	{
		for (const swPreviewInsertPieceInformation& PreviewPieceInfoTransform : mPreviewInsertPieceInfo)
			PreviewPieceInfoTransform.PieceData->AddRenderMeshes(Scene, PreviewPieceInfoTransform.Transform, PreviewPieceInfoTransform.ColorIndex, swRenderMeshState::Preview, false);
	}
}

void swModel::AddSubModelRenderMeshes(swScene* Scene, const swMatrix44& WorldMatrix, int DefaultColorIndex, swRenderMeshState RenderMeshState, bool ParentActive) const
{
	const bool Editing = ParentActive || Scene->IsModelEditing(this);

	for (const std::unique_ptr<swPiece>& Piece : mPieces)
	{
		const bool Visible = Editing ? (Piece->IsSelected() || Piece->IsVisible(mCurrentStep)) : Piece->IsVisibleInSubModel();

		if (Visible)
		{
			if (Piece->IsFocused())
				UpdateTrainTrackConnections(Piece.get(), false);

			Piece->AddSubModelRenderMeshes(Scene, WorldMatrix, DefaultColorIndex, RenderMeshState, ParentActive);
		}
	}

	if (Scene->GetDrawInsertPreview())
	{
		for (const swPreviewInsertPieceInformation& PreviewPieceInfoTransform : mPreviewInsertPieceInfo)
			PreviewPieceInfoTransform.PieceData->AddRenderMeshes(Scene, swMultiply(PreviewPieceInfoTransform.Transform, WorldMatrix), PreviewPieceInfoTransform.ColorIndex, swRenderMeshState::Preview, false);
	}
}

QImage swModel::GetStepImage(bool Zoom, int Width, int Height, swStep Step)
{
	swScopedGLContextState GLState;

	WaitForAssets();

	const swResult<QImage> Image = RenderStepImageWithReadyAssets(Zoom, Width, Height, Step, nullptr, swImageBackground::Default);

	if (!Image)
	{
		if (gMainWindow)
			QMessageBox::warning(gMainWindow, tr("Create Step Image"), Image.error());

		return QImage();
	}

	return Image.value();
}

swResult<QImage> swModel::RenderStepImageWithReadyAssets(bool Zoom, int Width, int Height, swStep Step, swCamera* OutputCamera, swImageBackground Background)
{
	swScopedGLContextState GLState;

	const swView* ActiveView = gMainWindow ? gMainWindow->GetActiveView() : nullptr;
	const swStep CurrentStep = mCurrentStep;

	swView View(swViewType::View, this);

	if (Background == swImageBackground::Transparent)
		View.SetBackgroundColorOverride(SW_RGBA(0, 0, 0, 0));

	if (OutputCamera)
		View.SetCamera(OutputCamera, true);
	else if (ActiveView)
		View.SetCamera(ActiveView->GetCamera(), true);

	View.SetOffscreenContext();
	View.MakeCurrent();
	View.SetSize(Width, Height);

	if (!OutputCamera && !ActiveView)
	{
		SetTemporaryStep(GetLastStep());
		View.ZoomExtents(swGeometryBoundsMode::AvailableOnly, true);
	}

	SetTemporaryStep(Step);

	if (Zoom && ActiveView && !OutputCamera)
	{
		View.ZoomExtents(swGeometryBoundsMode::AvailableOnly, true);
	}

	if (!View.BeginRenderToImage(Width, Height))
	{
		SetTemporaryStep(CurrentStep);

		if (!mActive)
			CalculateStep(SW_STEP_MAXIMUM);

		return swUnexpected(tr("Error creating images."));
	}

	View.OnDraw();

	QImage Image = View.GetRenderImage();

	View.EndRenderToImage();

	SetTemporaryStep(CurrentStep);

	if (!mActive)
		CalculateStep(SW_STEP_MAXIMUM);

	if (Background == swImageBackground::Opaque)
		Image = Image.convertToFormat(QImage::Format_RGB32);

	if (Image.isNull())
		return swUnexpected(tr("Error creating images."));

	return Image;
}

swResult<QImage> swModel::GetPiecesListImage(int MaxWidth, int RequestedThumbnailSize, swStep Step, quint32 BackgroundColor, quint32 ThumbnailBackgroundColor, QFont Font, QColor TextColor, bool KeepMissingPieces, bool ShowCounts, swPiecesListSort Sort) const
{
	swScopedGLContextState GLState;

	WaitForAssets();

	swPiecesList PiecesList;

	if (Step == 0)
		GetPiecesList(gDefaultColor, true, false, PiecesList);
	else
		GetPiecesListForStep(Step, gDefaultColor, PiecesList, false);

	if (PiecesList.empty())
		return QImage();

	struct swPiecesListImage
	{
		QImage Thumbnail;
		const swPieceData* PieceData;
		int ColorIndex;
		int Count;
		QRect Bounds;
		QPoint Position;
	};

	std::vector<swPiecesListImage> Images;

	for (const auto& PieceIt : PiecesList)
	{
		const swPieceData* PieceData = PieceIt.first;

		if (!KeepMissingPieces && !PieceData->IsModel() && !PieceData->IsProject() && (PieceData->mState != swAssetState::Loaded || !PieceData->GetMesh()))
			continue;

		for (const auto& ColorIt : PieceIt.second)
		{
			Images.push_back(swPiecesListImage());
			swPiecesListImage& Image = Images.back();
			Image.PieceData = PieceIt.first;
			Image.ColorIndex = ColorIt.first;
			Image.Count = ColorIt.second;
		}
	}

	if (Images.empty())
	{
		QImage Image(1, 1, QImage::Format_ARGB32);

		if (Image.isNull())
			return swUnexpected(tr("Could not allocate the pieces list image."));

		Image.fill(swQColorFromRGBA(BackgroundColor));
		return Image;
	}

	auto ImageCompare = [Sort](const swPiecesListImage& Image1, const swPiecesListImage& Image2)
	{
		switch (Sort)
		{
		case swPiecesListSort::Color:
			break;

		case swPiecesListSort::Name:
			if (int Compare = strcmp(Image1.PieceData->m_strDescription, Image2.PieceData->m_strDescription))
				return Compare < 0;
			return Image1.ColorIndex < Image2.ColorIndex;

		case swPiecesListSort::PieceId:
			if (int Compare = strcmp(Image1.PieceData->mFileName, Image2.PieceData->mFileName))
				return Compare < 0;
			return Image1.ColorIndex < Image2.ColorIndex;
		}

		if (Image1.ColorIndex != Image2.ColorIndex)
			return Image1.ColorIndex < Image2.ColorIndex;

		return strcmp(Image1.PieceData->m_strDescription, Image2.PieceData->m_strDescription) < 0;
	};

	std::sort(Images.begin(), Images.end(), ImageCompare);

	swView View(swViewType::PiecesList, nullptr);
	View.SetOffscreenContext();
	View.MakeCurrent();
	swContext* Context = View.mContext;
	// a thumbnail wider than the list could never be laid out
	const int ThumbnailSize = qBound(16, RequestedThumbnailSize, qMax(qMin(MaxWidth, 512), 16));

	View.SetSize(ThumbnailSize, ThumbnailSize);

	if (!View.BeginRenderToImage(ThumbnailSize, ThumbnailSize))
		return swUnexpected(tr("Could not create the pieces list framebuffer."));

	float OrthoSize = 200.0f;

	swMatrix44 ProjectionMatrix = swMatrix44Orthographic(-OrthoSize, OrthoSize, -OrthoSize, OrthoSize, -5000.0f, 5000.0f);
	const swMatrix44 ViewMatrix = swMatrix44LookAt(swVector3(-100.0f, -100.0f, 75.0f), swVector3(0.0f, 0.0f, 0.0f), swVector3(0.0f, 0.0f, 1.0f));
	const int Viewport[4] = { 0, 0, ThumbnailSize, ThumbnailSize };

	float ExtraPixels = 0.0f;

	for (const swPiecesListImage& Image : Images)
	{
		const swPieceData* PieceData = Image.PieceData;
		const swBoundingBox& BoundingBox = PieceData->GetBoundingBox();

		swVector3 Points[8];
		swGetBoxCorners(BoundingBox.Min, BoundingBox.Max, Points);

		for (swVector3& Point : Points)
		{
			Point = swProjectPoint(Point, ViewMatrix, ProjectionMatrix, Viewport);

			ExtraPixels = qMax(ExtraPixels, -Point.x);
			ExtraPixels = qMax(ExtraPixels, Point.x - ThumbnailSize);
			ExtraPixels = qMax(ExtraPixels, -Point.y);
			ExtraPixels = qMax(ExtraPixels, Point.y - ThumbnailSize);
		}
	}

	if (ExtraPixels)
	{
		OrthoSize += ExtraPixels * (2.0f * OrthoSize / ThumbnailSize);
		ProjectionMatrix = swMatrix44Orthographic(-OrthoSize, OrthoSize, -OrthoSize, OrthoSize, -5000.0f, 5000.0f);
	}

	Context->SetViewport(0, 0, ThumbnailSize, ThumbnailSize);
	Context->SetDefaultState();
	Context->SetProjectionMatrix(ProjectionMatrix);

	bool RenderFailed = false;

	for (swPiecesListImage& Image : Images)
	{
		View.BindRenderFramebuffer();
		Context->ClearColorAndDepth(swVector4(swVector3FromColor(BackgroundColor), 0.0f));

		swScene Scene;

		const swRenderSettings RenderSettings = swGetPreferences().GetRenderSettings(swRenderTarget::Pieces);
		swShadingMode ShadingMode = RenderSettings.ShadingMode;
		if (ShadingMode == swShadingMode::Wireframe)
			ShadingMode = swShadingMode::Flat;

		Scene.SetShadingMode(ShadingMode);
		Scene.SetLightingStyle(swGetPreferences().mLightingStyle);
		Scene.SetLineSettings(RenderSettings.DrawEdgeLines, RenderSettings.DrawConditionalLines, RenderSettings.LineWidth);
		Scene.SetRequireCompleteAssets(true);
		Scene.SetAllowLOD(false);
		Scene.Begin(ViewMatrix);

		Image.PieceData->AddRenderMeshes(&Scene, swMatrix44Identity(), Image.ColorIndex, swRenderMeshState::Default, true);

		Scene.End();

		Context->SetLineWidth(RenderSettings.LineWidth);
		Scene.Draw(Context, {});

		View.UnbindRenderFramebuffer();
		Image.Thumbnail = View.GetRenderFramebufferImage().convertToFormat(QImage::Format_ARGB32);

		if (Image.Thumbnail.isNull())
		{
			RenderFailed = true;
			break;
		}
	}

	View.EndRenderToImage();
	Context->ClearResources();

	if (RenderFailed)
		return swUnexpected(tr("Could not read the pieces list image."));

	auto CalculateImageBounds = [](swPiecesListImage& Image)
	{
		const QImage& Thumbnail = Image.Thumbnail;
		const int Width = Thumbnail.width();
		const int Height = Thumbnail.height();

		int MinX = Width;
		int MinY = Height;
		int MaxX = 0;
		int MaxY = 0;

		for (int x = 0; x < Width; x++)
		{
			for (int y = 0; y < Height; y++)
			{
				if (qAlpha(Thumbnail.pixel(x, y)))
				{
					MinX = qMin(x, MinX);
					MinY = qMin(y, MinY);
					MaxX = qMax(x, MaxX);
					MaxY = qMax(y, MaxY);
				}
			}
		}

		Image.Bounds = QRect(QPoint(MinX, MinY), QPoint(MaxX, MaxY));
	};

	QtConcurrent::blockingMap(Images, CalculateImageBounds);

	if (KeepMissingPieces)
	{
		for (swPiecesListImage& Image : Images)
			if (Image.Bounds.isEmpty())
				Image.Bounds = QRect(0, 0, qMin(64, ThumbnailSize), qMin(64, ThumbnailSize));
	}
	else
	{
		Images.erase(std::remove_if(Images.begin(), Images.end(), [](const swPiecesListImage& Image)
		{
			return Image.Bounds.isEmpty();
		}), Images.end());
	}

	if (Images.empty())
	{
		QImage Image(1, 1, QImage::Format_ARGB32);

		if (Image.isNull())
			return swUnexpected(tr("Could not allocate the pieces list image."));

		Image.fill(swQColorFromRGBA(BackgroundColor));
		return Image;
	}

	QImage DummyImage(16, 16, QImage::Format_ARGB32);

	if (DummyImage.isNull())
		return swUnexpected(tr("Could not allocate the pieces list image."));

	QPainter DummyPainter(&DummyImage);

	DummyPainter.setFont(Font);
	QFontMetrics FontMetrics = DummyPainter.fontMetrics();
	const int Ascent = ShowCounts ? FontMetrics.ascent() : 0;

	int CurrentHeight = 0;
	int ImageWidth = MaxWidth;

	for (const swPiecesListImage& Image : Images)
		CurrentHeight = qMax(Image.Bounds.height() + Ascent, CurrentHeight);

	for (;;)
	{
		int CurrentWidth = 0;
		int CurrentX = 0;
		int CurrentY = 0;
		int ColumnWidth = 0;
		constexpr int Spacing = 20;
		int NextHeightIncrease = INT_MAX;

		for (swPiecesListImage& Image : Images)
		{
			if (CurrentY + Image.Bounds.height() + Ascent > CurrentHeight)
			{
				const int NeededSpace = Image.Bounds.height() + Ascent - (CurrentHeight - CurrentY);
				NextHeightIncrease = qMin(NeededSpace, NextHeightIncrease);

				CurrentY = 0;
				CurrentX += ColumnWidth + Spacing;
				ColumnWidth = 0;
			}

			Image.Position = QPoint(CurrentX, CurrentY);
			CurrentY += Image.Bounds.height() + Ascent + Spacing;
			CurrentWidth = qMax(CurrentWidth, CurrentX + Image.Bounds.width());
			ColumnWidth = qMax(ColumnWidth, Image.Bounds.width());
		}

		if (CurrentWidth <= MaxWidth)
		{
			ImageWidth = CurrentWidth;
			break;
		}

		CurrentHeight += NextHeightIncrease;
	}

	QImage PainterImage(ImageWidth + 40, CurrentHeight + 40, QImage::Format_ARGB32);

	if (PainterImage.isNull())
		return swUnexpected(tr("Could not allocate the pieces list image."));

	PainterImage.fill(swQColorFromRGBA(BackgroundColor));

	QPainter Painter(&PainterImage);
	Painter.setFont(Font);
	Painter.setPen(TextColor);

	for (const swPiecesListImage& Image : Images)
	{
		const QPoint Position = Image.Position + QPoint(20, 20);

		// the thumbnail background holds the piece and its count, it stays inside the spacing between pieces
		if (qAlpha(ThumbnailBackgroundColor))
			Painter.fillRect(QRect(Position, Image.Bounds.size() + QSize(0, Ascent)).adjusted(-6, -6, 6, 6), swQColorFromRGBA(ThumbnailBackgroundColor));

		Painter.drawImage(Position, Image.Thumbnail, Image.Bounds);

		if (ShowCounts)
			Painter.drawText(QPoint(Position.x(), Position.y() + Image.Bounds.height() + Ascent), QString::number(Image.Count) + 'x');
	}

	Painter.end();

	return PainterImage;
}

swResult<void> swModel::SaveStepImages(const QString& BaseName, bool AddStepSuffix, bool Zoom, int Width, int Height, swStep Start, swStep End, swImageBackground Background)
{
	WaitForAssets();

	std::unique_ptr<swView> OutputView;
	swCamera* Camera = nullptr;

	if (!gMainWindow || !gMainWindow->GetActiveView())
	{
		const swStep CurrentStep = mCurrentStep;

		OutputView = std::make_unique<swView>(swViewType::View, this);
		OutputView->SetSize(Width, Height);
		SetTemporaryStep(GetLastStep());
		OutputView->ZoomExtents(swGeometryBoundsMode::AvailableOnly, true);
		SetTemporaryStep(CurrentStep);

		if (!mActive)
			CalculateStep(SW_STEP_MAXIMUM);

		Camera = OutputView->GetCamera();
	}

	for (swStep Step = Start; Step <= End; Step++)
	{
		QString FileName;

		if (AddStepSuffix)
			FileName = BaseName.arg(Step, 2, 10, QLatin1Char('0'));
		else
			FileName = BaseName;

		const swResult<QImage> Image = RenderStepImageWithReadyAssets(Zoom, Width, Height, Step, Camera, Background);

		if (!Image)
			return swUnexpected(Image.error());

		QImageWriter Writer(FileName);

		if (Writer.format().isEmpty())
			Writer.setFormat("png");

		if (!Writer.write(Image.value()))
			return swUnexpected(tr("Error writing to file '%1':\n%2").arg(FileName, Writer.errorString()));
	}

	return swResult<void>();
}
