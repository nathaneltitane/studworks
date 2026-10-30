#include "sw_global.h"
#include "sw_model.h"
#include "sw_model_history.h"
#include "sw_object_part.h"
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
#include "sw_data_part.h"
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
#include "sw_file_gif.h"
#include <locale.h>

void swModel::GetScene(swScene* Scene, const swCamera* ViewCamera, bool AllowHighlight, bool AllowFade) const
{
	if (mPartData)
		mPartData->AddRenderMesh(*Scene);

	swPart* FocusPart = nullptr;

	// selected parts outside the step
	const bool DrawSelected = Scene->GetDrawInterface();

	for (const std::unique_ptr<swPart>& Part : mParts)
	{
		if ((DrawSelected && Part->IsSelected()) || Part->IsVisible(mCurrentStep))
		{
			if (Part->IsFocused())
				FocusPart = Part.get();

			const swStep StepShow = Part->GetStepShow();
			Part->AddMainModelRenderMeshes(Scene, AllowHighlight && StepShow == mCurrentStep, AllowFade && StepShow < mCurrentStep);
		}
	}

	if (Scene->GetDrawInterface() && !Scene->GetActiveSubmodelInstance())
	{
		if (FocusPart)
			UpdateTrainTrackConnections(FocusPart, false);

		for (const std::unique_ptr<swCamera>& Camera : mCameras)
			if (Camera.get() != ViewCamera && Camera->IsVisible())
				Scene->AddInterfaceObject(Camera.get());

		for (const std::unique_ptr<swLight>& Light : mLights)
			if (Light->IsVisible())
				Scene->AddInterfaceObject(Light.get());
	}

	if (Scene->GetDrawInsertPreview())
	{
		for (const swPreviewInsertPartInformation& PreviewPartInfoTransform : mPreviewInsertPartInfo)
			PreviewPartInfoTransform.PartData->AddRenderMeshes(Scene, PreviewPartInfoTransform.Transform, PreviewPartInfoTransform.ColorIndex, swRenderMeshState::Preview, false);
	}
}

void swModel::AddSubModelRenderMeshes(swScene* Scene, const swMatrix44& WorldMatrix, int DefaultColorIndex, swRenderMeshState RenderMeshState, bool ParentActive) const
{
	const bool Editing = ParentActive || Scene->IsModelEditing(this);

	for (const std::unique_ptr<swPart>& Part : mParts)
	{
		const bool Visible = Editing ? ((Scene->GetDrawInterface() && Part->IsSelected()) || Part->IsVisible(mCurrentStep)) : Part->IsVisibleInSubModel();

		if (Visible)
		{
			if (Part->IsFocused())
				UpdateTrainTrackConnections(Part.get(), false);

			Part->AddSubModelRenderMeshes(Scene, WorldMatrix, DefaultColorIndex, RenderMeshState, ParentActive);
		}
	}

	if (Scene->GetDrawInsertPreview())
	{
		for (const swPreviewInsertPartInformation& PreviewPartInfoTransform : mPreviewInsertPartInfo)
			PreviewPartInfoTransform.PartData->AddRenderMeshes(Scene, swMultiply(PreviewPartInfoTransform.Transform, WorldMatrix), PreviewPartInfoTransform.ColorIndex, swRenderMeshState::Preview, false);
	}
}

QImage swModel::GetStepImage(bool Zoom, int Width, int Height, swStep Step, swImageBackground Background)
{
	swScopedGLContextState GLState;

	WaitForAssets();

	const swResult<QImage> Image = RenderStepImageWithReadyAssets(Zoom, Width, Height, Step, nullptr, Background);

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

swResult<QImage> swModel::GetPartsListImage(int MaxWidth, int RequestedThumbnailSize, swStep Step, quint32 BackgroundColor, quint32 ThumbnailBackgroundColor, QFont Font, QColor TextColor, bool KeepMissingParts, bool ShowCounts, swPartsListSort Sort, swPartsListLayout Layout, swPartsListAlignment Alignment, int MaxHeight, float Scale) const
{
	// scale every pixel size
	auto Scaled = [Scale](int Size)
	{
		return qRound(Size * Scale);
	};

	MaxWidth = Scaled(MaxWidth);
	MaxHeight = MaxHeight < INT_MAX / 8 ? Scaled(MaxHeight) : INT_MAX;

	if (Font.pointSizeF() > 0.0)
		Font.setPointSizeF(Font.pointSizeF() * Scale);
	else if (Font.pixelSize() > 0)
		Font.setPixelSize(Scaled(Font.pixelSize()));

	swScopedGLContextState GLState;

	WaitForAssets();

	swPartsList PartsList;

	if (Step == 0)
		GetPartsList(gDefaultColor, true, false, PartsList);
	else
		GetPartsListForStep(Step, gDefaultColor, PartsList, false);

	if (PartsList.empty())
		return QImage();

	struct swPartsListImage
	{
		QImage Thumbnail;
		const swPartData* PartData;
		int ColorIndex;
		int Count;
		QRect Bounds;
		QPoint Position;
		QPoint TextPosition;
		int CellWidth;
	};

	std::vector<swPartsListImage> Images;

	for (const auto& PartIt : PartsList)
	{
		const swPartData* PartData = PartIt.first;

		if (!KeepMissingParts && !PartData->IsModel() && !PartData->IsProject() && (PartData->mState != swAssetState::Loaded || !PartData->GetMesh()))
			continue;

		for (const auto& ColorIt : PartIt.second)
		{
			Images.push_back(swPartsListImage());
			swPartsListImage& Image = Images.back();
			Image.PartData = PartIt.first;
			Image.ColorIndex = ColorIt.first;
			Image.Count = ColorIt.second;
		}
	}

	if (Images.empty())
	{
		QImage Image(1, 1, QImage::Format_ARGB32);

		if (Image.isNull())
			return swUnexpected(tr("Could not allocate the parts list image."));

		Image.fill(swQColorFromRGBA(BackgroundColor));
		return Image;
	}

	auto ImageCompare = [Sort](const swPartsListImage& Image1, const swPartsListImage& Image2)
	{
		switch (Sort)
		{
		case swPartsListSort::Color:
			break;

		case swPartsListSort::Name:
			if (int Compare = strcmp(Image1.PartData->m_strDescription, Image2.PartData->m_strDescription))
				return Compare < 0;
			return Image1.ColorIndex < Image2.ColorIndex;

		case swPartsListSort::PartId:
			if (int Compare = strcmp(Image1.PartData->mFileName, Image2.PartData->mFileName))
				return Compare < 0;
			return Image1.ColorIndex < Image2.ColorIndex;
		}

		if (Image1.ColorIndex != Image2.ColorIndex)
			return Image1.ColorIndex < Image2.ColorIndex;

		return strcmp(Image1.PartData->m_strDescription, Image2.PartData->m_strDescription) < 0;
	};

	std::sort(Images.begin(), Images.end(), ImageCompare);

	swView View(swViewType::PartsList, nullptr);
	View.SetOffscreenContext();
	View.MakeCurrent();
	swContext* Context = View.mContext;

	// skip thumbnails wider than the list
	const int ThumbnailSize = qBound(Scaled(16), Scaled(RequestedThumbnailSize), qMax(qMin(MaxWidth, Scaled(512)), Scaled(16)));

	View.SetSize(ThumbnailSize, ThumbnailSize);

	if (!View.BeginRenderToImage(ThumbnailSize, ThumbnailSize))
		return swUnexpected(tr("Could not create the parts list framebuffer."));

	float OrthoSize = 200.0f;

	swMatrix44 ProjectionMatrix = swMatrix44Orthographic(-OrthoSize, OrthoSize, -OrthoSize, OrthoSize, -5000.0f, 5000.0f);
	const swMatrix44 ViewMatrix = swMatrix44LookAt(swVector3(-100.0f, -100.0f, 75.0f), swVector3(0.0f, 0.0f, 0.0f), swVector3(0.0f, 0.0f, 1.0f));
	const int Viewport[4] = { 0, 0, ThumbnailSize, ThumbnailSize };

	float ExtraPixels = 0.0f;

	for (const swPartsListImage& Image : Images)
	{
		const swPartData* PartData = Image.PartData;
		const swBoundingBox& BoundingBox = PartData->GetBoundingBox();

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

	for (swPartsListImage& Image : Images)
	{
		View.BindRenderFramebuffer();
		Context->ClearColorAndDepth(swVector4(swVector3FromColor(BackgroundColor), 0.0f));

		swScene Scene;

		const swRenderSettings RenderSettings = swGetPreferences().GetRenderSettings(swRenderTarget::Parts);
		swShadingMode ShadingMode = RenderSettings.ShadingMode;
		if (ShadingMode == swShadingMode::Wireframe)
			ShadingMode = swShadingMode::Flat;

		Scene.SetShadingMode(ShadingMode);
		Scene.SetLightingStyle(swGetPreferences().mLightingStyle);
		Scene.SetLineSettings(RenderSettings.DrawEdgeLines, RenderSettings.DrawConditionalLines, RenderSettings.LineWidth * Scale);
		Scene.SetRequireCompleteAssets(true);
		Scene.SetAllowLOD(false);
		Scene.Begin(ViewMatrix);

		Image.PartData->AddRenderMeshes(&Scene, swMatrix44Identity(), Image.ColorIndex, swRenderMeshState::Default, true);

		Scene.End();

		Context->SetLineWidth(RenderSettings.LineWidth * Scale);
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
		return swUnexpected(tr("Could not read the parts list image."));

	auto CalculateImageBounds = [](swPartsListImage& Image)
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

	if (KeepMissingParts)
	{
		for (swPartsListImage& Image : Images)
			if (Image.Bounds.isEmpty())
				Image.Bounds = QRect(0, 0, qMin(Scaled(64), ThumbnailSize), qMin(Scaled(64), ThumbnailSize));
	}
	else
	{
		Images.erase(std::remove_if(Images.begin(), Images.end(), [](const swPartsListImage& Image)
		{
			return Image.Bounds.isEmpty();
		}), Images.end());
	}

	if (Images.empty())
	{
		QImage Image(1, 1, QImage::Format_ARGB32);

		if (Image.isNull())
			return swUnexpected(tr("Could not allocate the parts list image."));

		Image.fill(swQColorFromRGBA(BackgroundColor));
		return Image;
	}

	QImage DummyImage(16, 16, QImage::Format_ARGB32);

	if (DummyImage.isNull())
		return swUnexpected(tr("Could not allocate the parts list image."));

	QPainter DummyPainter(&DummyImage);

	DummyPainter.setFont(Font);
	QFontMetrics FontMetrics = DummyPainter.fontMetrics();
	const int Ascent = ShowCounts ? FontMetrics.ascent() : 0;

	const int Spacing = Scaled(20);

	// take the wider of thumbnail and count
	for (swPartsListImage& Image : Images)
		Image.CellWidth = qMax(Image.Bounds.width(), ShowCounts ? FontMetrics.horizontalAdvance(QString::number(Image.Count) + 'x') : 0);

	// use to place a part beside the edge
	auto GetAlignmentOffset = [Alignment](int Space)
	{
		switch (Alignment)
		{
		case swPartsListAlignment::Start:
			break;

		case swPartsListAlignment::Center:
			return Space / 2;

		case swPartsListAlignment::End:
			return Space;
		}

		return 0;
	};

	int ImageWidth = 0;
	int CurrentHeight = 0;

	if (Layout == swPartsListLayout::Rows)
	{
		// fill rows across the width
		size_t RowStart = 0;

		while (RowStart < Images.size())
		{
			size_t RowEnd = RowStart;
			int RowWidth = 0;
			int RowHeight = 0;

			while (RowEnd < Images.size())
			{
				const int Width = RowWidth + (RowEnd > RowStart ? Spacing : 0) + Images[RowEnd].CellWidth;

				if (RowEnd > RowStart && Width > MaxWidth)
					break;

				RowWidth = Width;
				RowHeight = qMax(RowHeight, Images[RowEnd].Bounds.height());
				RowEnd++;
			}

			int CurrentX = 0;

			for (size_t ImageIndex = RowStart; ImageIndex < RowEnd; ImageIndex++)
			{
				swPartsListImage& Image = Images[ImageIndex];

				Image.Position = QPoint(CurrentX, CurrentHeight + GetAlignmentOffset(RowHeight - Image.Bounds.height()));
				Image.TextPosition = QPoint(CurrentX, CurrentHeight + RowHeight + Ascent);
				CurrentX += Image.CellWidth + Spacing;
			}

			ImageWidth = qMax(ImageWidth, RowWidth);
			CurrentHeight += RowHeight + Ascent + Spacing;
			RowStart = RowEnd;
		}

		CurrentHeight -= Spacing;
	}
	else
	{
		// fill columns down the height
		std::vector<int> Columns(Images.size());
		std::vector<int> ColumnWidths;
		int CurrentX = 0;
		int CurrentY = 0;
		int ColumnWidth = 0;

		for (size_t ImageIndex = 0; ImageIndex < Images.size(); ImageIndex++)
		{
			swPartsListImage& Image = Images[ImageIndex];
			const int Height = Image.Bounds.height() + Ascent;

			if (CurrentY && CurrentY + Height > MaxHeight)
			{
				ColumnWidths.push_back(ColumnWidth);
				CurrentX += ColumnWidth + Spacing;
				CurrentY = 0;
				ColumnWidth = 0;
			}

			Image.Position = QPoint(CurrentX, CurrentY);
			Columns[ImageIndex] = static_cast<int>(ColumnWidths.size());
			CurrentHeight = qMax(CurrentHeight, CurrentY + Height);
			CurrentY += Height + Spacing;
			ColumnWidth = qMax(ColumnWidth, Image.CellWidth);
		}

		ColumnWidths.push_back(ColumnWidth);
		ImageWidth = CurrentX + ColumnWidth;

		for (size_t ImageIndex = 0; ImageIndex < Images.size(); ImageIndex++)
		{
			swPartsListImage& Image = Images[ImageIndex];
			const int ColumnWidth = ColumnWidths[Columns[ImageIndex]];

			Image.Position.rx() += GetAlignmentOffset(ColumnWidth - Image.CellWidth);
			Image.TextPosition = QPoint(Image.Position.x(), Image.Position.y() + Image.Bounds.height() + Ascent);
		}
	}

	QImage PainterImage(ImageWidth + Scaled(40), CurrentHeight + Scaled(40), QImage::Format_ARGB32);

	if (PainterImage.isNull())
		return swUnexpected(tr("Could not allocate the parts list image."));

	PainterImage.fill(swQColorFromRGBA(BackgroundColor));

	QPainter Painter(&PainterImage);
	Painter.setFont(Font);
	Painter.setPen(TextColor);

	const QPoint Padding(Scaled(20), Scaled(20));

	for (const swPartsListImage& Image : Images)
	{
		const QPoint Position = Image.Position + Padding;
		const QPoint TextPosition = Image.TextPosition + Padding;
		const QString Text = QString::number(Image.Count) + 'x';
		const int TextWidth = ShowCounts ? FontMetrics.horizontalAdvance(Text) : 0;

		// line up thumbnail and count
		const int ThumbnailX = Position.x() + GetAlignmentOffset(Image.CellWidth - Image.Bounds.width()) * (Layout == swPartsListLayout::Columns);
		const int TextX = Position.x() + GetAlignmentOffset(Image.CellWidth - TextWidth) * (Layout == swPartsListLayout::Columns);

		// keep the background inside the spacing
		if (qAlpha(ThumbnailBackgroundColor))
			Painter.fillRect(QRect(Position.x(), Position.y(), Image.CellWidth, TextPosition.y() - Position.y()).adjusted(-Scaled(6), -Scaled(6), Scaled(6), Scaled(6)), swQColorFromRGBA(ThumbnailBackgroundColor));

		Painter.drawImage(QPoint(ThumbnailX, Position.y()), Image.Thumbnail, Image.Bounds);

		if (ShowCounts)
			Painter.drawText(QPoint(TextX, TextPosition.y()), Text);
	}

	Painter.end();
	PainterImage.setDevicePixelRatio(Scale);

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

float swImageTurntable::GetFrameAngle(int Frame) const
{
	// each turn is one revolution or one swing
	const float Turn = static_cast<float>(Frame) / qMax(Frames, 1);
	const float Phase = Turn - floorf(Turn);

	if (FullTurn)
		return Phase * 360.0f;

	// out to the end and back
	if (Phase <= 0.5f)
		return AngleStart + (AngleEnd - AngleStart) * Phase * 2.0f;

	return AngleEnd - (AngleEnd - AngleStart) * (Phase * 2.0f - 1.0f);
}

int swImageTurntable::GetStepFrames() const
{
	if (NextStep == swImageNextStep::Seconds)
		return qMax(qRound(NextStepSeconds * FrameRate), 1);

	return qMax(NextStepTurns, 1) * qMax(Frames, 1);
}

int swImageTurntable::GetFrameCount(int StepCount) const
{
	if (Motion == swImageMotion::Steps)
		return qMax(StepCount, 1);

	if (NextStep == swImageNextStep::Off)
		return qMax(Turns, 1) * qMax(Frames, 1);

	return qMax(StepCount, 1) * GetStepFrames();
}

int swImageTurntable::GetFrameStep(int Frame) const
{
	if (Motion == swImageMotion::Steps)
		return Frame;

	if (NextStep == swImageNextStep::Off)
		return 0;

	return Frame / GetStepFrames();
}

int swImageTurntable::GetFrameDelay(int Frame) const
{
	const double Total = 100.0 / qMax(FrameRate, 0.1);

	return qRound(Total * (Frame + 1)) - qRound(Total * Frame);
}

// use to place the image camera
void swModel::SetImageCamera(swCamera* Camera, const swCamera* BaseCamera, const swImageFraming& Framing, const swImageTurntable& Turntable, int Frame, swStep Step, swStep LastStep, float Aspect) const
{
	swStep FitStep = SW_STEP_MAXIMUM;

	if (Framing.Fit == swImageFit::FitEachStep)
		FitStep = Step;
	else if (Framing.Scope == swImageFitScope::ShownSteps)
		FitStep = LastStep;

	std::vector<swVector3> Points;
	swVector3 Min(FLT_MAX, FLT_MAX, FLT_MAX);
	swVector3 Max(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	for (const std::unique_ptr<swPart>& Part : mParts)
	{
		if (!Part->IsVisible(FitStep))
			continue;

		swVector3 PartMin(FLT_MAX, FLT_MAX, FLT_MAX);
		swVector3 PartMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);

		Part->CompareBoundingBox(PartMin, PartMax);

		// corners of the part box
		for (int Corner = 0; Corner < 8; Corner++)
			Points.emplace_back(Corner & 1 ? PartMax.x : PartMin.x, Corner & 2 ? PartMax.y : PartMin.y, Corner & 4 ? PartMax.z : PartMin.z);

		Min = swMinimum(Min, PartMin);
		Max = swMaximum(Max, PartMax);
	}

	const swVector3 Position = BaseCamera->GetSectionPosition(SW_CAMERA_SECTION_POSITION);
	const swVector3 Target = BaseCamera->GetSectionPosition(SW_CAMERA_SECTION_TARGET);
	const swVector3 Up = BaseCamera->GetSectionPosition(SW_CAMERA_SECTION_UP_VECTOR) - Position;
	swVector3 Center = Target;

	if (Framing.Center == swImageCenter::WorldOrigin)
		Center = swVector3(0.0f, 0.0f, 0.0f);
	else if (!Points.empty() && Framing.Center == swImageCenter::ModelOrigin)
		Center = swVector3((Min.x + Max.x) * 0.5f, (Min.y + Max.y) * 0.5f, Min.z);
	else if (!Points.empty())
		Center = (Min + Max) * 0.5f;

	// axes in x y z order
	const swVector3 Axes[3] = { swVector3(1.0f, 0.0f, 0.0f), swVector3(0.0f, 1.0f, 0.0f), swVector3(0.0f, 0.0f, 1.0f) };
	const swVector3& Axis = Axes[static_cast<int>(Turntable.Axis)];

	// use to turn a direction for an angle
	auto GetRotation = [&Axis](float Angle)
	{
		return swMatrix33FromAxisAngle(Axis, -Angle * SW_DEGREES_TO_RADIANS);
	};

	const float FrameAngle = Turntable.IsTurning() ? Turntable.GetFrameAngle(Frame) : 0.0f;
	const swMatrix33 Rotation = GetRotation(FrameAngle);

	Camera->CopyPosition(BaseCamera);

	if (Framing.Fit == swImageFit::CurrentView || Points.empty())
	{
		Camera->SetViewpoint(Center + swMultiply(Position - Center, Rotation), Center + swMultiply(Target - Center, Rotation), swMultiply(Up, Rotation));
		return;
	}

	const swVector3 Front = swNormalize(Target - Position);
	const float Fill = 1.0f - qBound(0, Framing.Margin, 50) / 100.0f;
	const float TangentY = tanf(BaseCamera->m_fovy * 0.5f * SW_DEGREES_TO_RADIANS) * Fill;
	const float TangentX = TangentY * Aspect;
	const bool Orthographic = BaseCamera->GetProjection() == swCameraProjection::Orthographic;
	float Distance = 0.0f;
	float HalfHeight = 0.0f;

	if (!Turntable.IsTurning())
	{
		const swVector3 Side = swNormalize(swCross(Front, Up));
		const swVector3 ViewUp = swCross(Side, Front);

		// largest reach past each image edge
		float EdgeRight = -FLT_MAX;
		float EdgeLeft = -FLT_MAX;
		float EdgeTop = -FLT_MAX;
		float EdgeBottom = -FLT_MAX;
		float DepthNear = FLT_MAX;

		for (const swVector3& Point : Points)
		{
			const swVector3 Offset = Point - Center;
			const float X = swDot(Offset, Side);
			const float Y = swDot(Offset, ViewUp);
			const float Z = swDot(Offset, Front);
			const float SlopeX = Orthographic ? 0.0f : TangentX * Z;
			const float SlopeY = Orthographic ? 0.0f : TangentY * Z;

			EdgeRight = qMax(EdgeRight, X - SlopeX);
			EdgeLeft = qMax(EdgeLeft, -X - SlopeX);
			EdgeTop = qMax(EdgeTop, Y - SlopeY);
			EdgeBottom = qMax(EdgeBottom, -Y - SlopeY);
			DepthNear = qMin(DepthNear, Z);
		}

		// pan so both edges get the same space
		float ShiftX = (EdgeRight - EdgeLeft) * 0.5f;
		float ShiftY = (EdgeTop - EdgeBottom) * 0.5f;
		const float SpanX = (EdgeRight + EdgeLeft) * 0.5f;
		const float SpanY = (EdgeTop + EdgeBottom) * 0.5f;

		if (Orthographic)
		{
			HalfHeight = qMax(SpanY, SpanX / Aspect) / Fill;
			Distance = HalfHeight * 2.0f / (BaseCamera->m_fovy * SW_DEGREES_TO_RADIANS);
		}
		else
			Distance = qMax(SpanX / TangentX, SpanY / TangentY);

		// stay in front of the near plane
		Distance = qMax(Distance, BaseCamera->m_zNear - DepthNear + 1.0f);

		// use to center the projected parts along one axis
		auto Balance = [&Points, &Center, &Front, Distance](const swVector3& Direction, float Shift)
		{
			float Low = Shift - Distance;
			float High = Shift + Distance;

			for (int Pass = 0; Pass < 32; Pass++)
			{
				const float Middle = (Low + High) * 0.5f;
				float Positive = -FLT_MAX;
				float Negative = -FLT_MAX;

				for (const swVector3& Point : Points)
				{
					const swVector3 Offset = Point - Center;
					const float Depth = swDot(Offset, Front) + Distance;
					const float Along = swDot(Offset, Direction) - Middle;

					Positive = qMax(Positive, Along / Depth);
					Negative = qMax(Negative, -Along / Depth);
				}

				if (Positive > Negative)
					Low = Middle;
				else
					High = Middle;
			}

			return (Low + High) * 0.5f;
		};

		// the looser axis shifts with the distance
		if (!Orthographic)
		{
			ShiftX = Balance(Side, ShiftX);
			ShiftY = Balance(ViewUp, ShiftY);
		}

		const swVector3 Shift = Side * ShiftX + ViewUp * ShiftY;

		Camera->SetViewpoint(Center + Shift - Front * Distance, Center + Shift, Up);
		return;
	}

	// one fit for every frame keeps the zoom steady
	for (int FitFrame = 0; FitFrame < qMax(Turntable.Frames, 1); FitFrame++)
	{
		const swMatrix33 FitRotation = GetRotation(Turntable.GetFrameAngle(FitFrame));
		const swVector3 FitFront = swMultiply(Front, FitRotation);
		const swVector3 FitSide = swNormalize(swCross(FitFront, swMultiply(Up, FitRotation)));
		const swVector3 FitUp = swCross(FitSide, FitFront);

		for (const swVector3& Point : Points)
		{
			const swVector3 Offset = Point - Center;
			const float X = fabsf(swDot(Offset, FitSide));
			const float Y = fabsf(swDot(Offset, FitUp));
			const float Z = swDot(Offset, FitFront);

			if (Orthographic)
				HalfHeight = qMax(HalfHeight, qMax(Y, X / Aspect) / Fill);
			else
				Distance = qMax(Distance, qMax(X / TangentX, Y / TangentY) - Z);

			// stay in front of the near plane
			Distance = qMax(Distance, BaseCamera->m_zNear - Z + 1.0f);
		}
	}

	// orthographic size follows the distance
	if (Orthographic)
		Distance = qMax(Distance, HalfHeight * 2.0f / (BaseCamera->m_fovy * SW_DEGREES_TO_RADIANS));

	const swVector3 FrameFront = swMultiply(Front, Rotation);

	Camera->SetViewpoint(Center - FrameFront * Distance, Center, swMultiply(Up, Rotation));
}

QImage swModel::GetFramedImage(int Width, int Height, swStep Step, swStep LastStep, swImageBackground Background, const swImageFraming& Framing, const swImageTurntable& Turntable, int Frame)
{
	const swView* ActiveView = gMainWindow ? gMainWindow->GetActiveView() : nullptr;

	if (!ActiveView)
		return GetStepImage(false, Width, Height, Step, Background);

	swScopedGLContextState GLState;

	WaitForAssets();

	swCamera Camera(true);
	SetImageCamera(&Camera, ActiveView->GetCamera(), Framing, Turntable, Frame, Step, LastStep, static_cast<float>(Width) / Height);

	const swResult<QImage> Image = RenderStepImageWithReadyAssets(false, Width, Height, Step, &Camera, Background);

	if (!Image)
		return QImage();

	return Image.value();
}

swResult<void> swModel::SaveFramedImages(const QString& BaseName, bool AddStepSuffix, int Width, int Height, swStep Start, swStep End, swImageBackground Background, const swImageFraming& Framing, const swImageTurntable& Turntable)
{
	swScopedGLContextState GLState;

	WaitForAssets();

	std::unique_ptr<swView> OutputView;
	const swCamera* BaseCamera = nullptr;

	if (gMainWindow && gMainWindow->GetActiveView())
		BaseCamera = gMainWindow->GetActiveView()->GetCamera();
	else
	{
		const swStep CurrentStep = mCurrentStep;

		OutputView = std::make_unique<swView>(swViewType::View, this);
		OutputView->SetSize(Width, Height);
		SetTemporaryStep(GetLastStep());
		OutputView->ZoomExtents(swGeometryBoundsMode::AvailableOnly, true);
		SetTemporaryStep(CurrentStep);

		if (!mActive)
			CalculateStep(SW_STEP_MAXIMUM);

		BaseCamera = OutputView->GetCamera();
	}

	const bool Transparent = Background == swImageBackground::Transparent;
	const float Aspect = static_cast<float>(Width) / Height;
	swCamera Camera(true);

	// every step in a single file
	if (Turntable.IsStepping())
	{
		swGifWriter Writer(BaseName, QSize(Width, Height));

		if (!Writer.Open())
			return swUnexpected(tr("Error writing to file '%1':\n%2").arg(BaseName, Writer.GetErrorString()));

		const int FrameCount = Turntable.GetFrameCount(End - Start + 1);

		for (int Frame = 0; Frame < FrameCount; Frame++)
		{
			const swStep Step = Start + Turntable.GetFrameStep(Frame);

			SetImageCamera(&Camera, BaseCamera, Framing, Turntable, Frame, Step, End, Aspect);

			const swResult<QImage> Image = RenderStepImageWithReadyAssets(false, Width, Height, Step, &Camera, Background);

			if (!Image)
				return swUnexpected(Image.error());

			if (!Writer.AddFrame(Image.value(), Turntable.GetFrameDelay(Frame), Transparent))
				return swUnexpected(tr("Error writing to file '%1':\n%2").arg(BaseName, Writer.GetErrorString()));
		}

		if (!Writer.Close())
			return swUnexpected(tr("Error writing to file '%1':\n%2").arg(BaseName, Writer.GetErrorString()));

		return swResult<void>();
	}

	for (swStep Step = Start; Step <= End; Step++)
	{
		QString FileName;

		if (AddStepSuffix)
			FileName = BaseName.arg(Step, 2, 10, QLatin1Char('0'));
		else
			FileName = BaseName;

		if (!Turntable.Enabled)
		{
			SetImageCamera(&Camera, BaseCamera, Framing, Turntable, 0, Step, End, Aspect);

			const swResult<QImage> Image = RenderStepImageWithReadyAssets(false, Width, Height, Step, &Camera, Background);

			if (!Image)
				return swUnexpected(Image.error());

			QImageWriter Writer(FileName);

			if (Writer.format().isEmpty())
				Writer.setFormat("png");

			if (!Writer.write(Image.value()))
				return swUnexpected(tr("Error writing to file '%1':\n%2").arg(FileName, Writer.errorString()));

			continue;
		}

		swGifWriter Writer(FileName, QSize(Width, Height));

		if (!Writer.Open())
			return swUnexpected(tr("Error writing to file '%1':\n%2").arg(FileName, Writer.GetErrorString()));

		for (int Frame = 0; Frame < Turntable.GetFrameCount(1); Frame++)
		{
			SetImageCamera(&Camera, BaseCamera, Framing, Turntable, Frame, Step, End, Aspect);

			const swResult<QImage> Image = RenderStepImageWithReadyAssets(false, Width, Height, Step, &Camera, Background);

			if (!Image)
				return swUnexpected(Image.error());

			if (!Writer.AddFrame(Image.value(), Turntable.GetFrameDelay(Frame), Transparent))
				return swUnexpected(tr("Error writing to file '%1':\n%2").arg(FileName, Writer.GetErrorString()));
		}

		if (!Writer.Close())
			return swUnexpected(tr("Error writing to file '%1':\n%2").arg(FileName, Writer.GetErrorString()));
	}

	return swResult<void>();
}
