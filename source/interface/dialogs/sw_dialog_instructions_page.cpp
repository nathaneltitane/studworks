#include "sw_global.h"
#include "sw_dialog_instructions_page.h"
#include "sw_dialog_instructions_placement.h"
#include "sw_dialog_instructions_properties.h"
#include "sw_project.h"
#include "sw_model.h"
#include "sw_object_part.h"
#include "sw_object_camera.h"
#include "sw_view.h"
#include "sw_window_utilities.h"

// use to identify a page element
bool swGetElementKey(const QGraphicsItem* Item, swInstructionsElementKey& Key)
{
	if (const swInstructionsStepImageItem* ImageItem = dynamic_cast<const swInstructionsStepImageItem*>(Item))
	{
		const swInstructionsPlacementElement Element = ImageItem->IsCoverImage() ? swInstructionsPlacementElement::CoverImage : swInstructionsPlacementElement::StepImage;
		Key = { Element, ImageItem->GetModel(), ImageItem->GetStep() };
	}
	else if (const swInstructionsStepNumberItem* NumberItem = dynamic_cast<const swInstructionsStepNumberItem*>(Item))
		Key = { swInstructionsPlacementElement::StepNumber, NumberItem->GetModel(), NumberItem->GetStep() };
	else if (const swInstructionsPartsListItem* PartsItem = dynamic_cast<const swInstructionsPartsListItem*>(Item))
	{
		const swInstructionsPlacementElement Element = PartsItem->IsBillOfMaterials() ? swInstructionsPlacementElement::BillOfMaterials : swInstructionsPlacementElement::PartsList;
		Key = { Element, PartsItem->GetModel(), PartsItem->GetStep() };
	}
	else if (const swInstructionsCoverTextItem* TextItem = dynamic_cast<const swInstructionsCoverTextItem*>(Item))
	{
		const swInstructionsPlacementElement Element = TextItem->IsAuthor() ? swInstructionsPlacementElement::CoverAuthor : swInstructionsPlacementElement::CoverTitle;
		Key = { Element, TextItem->GetModel(), 0 };
	}
	else if (dynamic_cast<const swInstructionsPageNumberItem*>(Item))
		Key = { swInstructionsPlacementElement::PageNumber, nullptr, 0 };
	else if (const swInstructionsNoticeItem* NoticeItem = dynamic_cast<const swInstructionsNoticeItem*>(Item))
		Key = { swInstructionsPlacementElement::Notice, nullptr, static_cast<swStep>(NoticeItem->GetId()) };
	else if (const swInstructionsPointerItem* PointerItem = dynamic_cast<const swInstructionsPointerItem*>(Item))
		Key = { swInstructionsPlacementElement::Pointer, nullptr, static_cast<swStep>(PointerItem->GetId()) };
	else
	{
		Key = { swInstructionsPlacementElement::None, nullptr, 0 };
		return false;
	}

	return true;
}

// use to snap a dragged element
static QVariant swSnapItemPosition(const QGraphicsItem* Item, const swInstructions* Instructions, const QVariant& Value)
{
	const QGraphicsScene* Scene = Item->scene();
	const QGraphicsItem* Parent = Item->parentItem();
	const QPointF Position = Value.toPointF();
	QPointF ScenePosition = Parent ? Parent->mapToScene(Position) : Position;

	// only dragging snaps
	if (Instructions->IsSnapToGrid() && Scene && Scene->mouseGrabberItem())
	{
		const QPointF Origin = Instructions->GetGridRect().topLeft();
		const qreal GridSize = Instructions->GetGridSnapSize();

		ScenePosition = Origin + QPointF(qRound((ScenePosition.x() - Origin.x()) / GridSize) * GridSize, qRound((ScenePosition.y() - Origin.y()) / GridSize) * GridSize);
	}

	// keep large elements at the top left margin
	const swInstructionsPageSetup& PageSetup = Instructions->mPageSetup;
	const QRectF ItemRect = Item->boundingRect();
	const QPointF MarginsMin = QPointF(PageSetup.MarginLeft, PageSetup.MarginTop) - ItemRect.topLeft();
	const QPointF MarginsMax = QPointF(PageSetup.Width - PageSetup.MarginRight, PageSetup.Height - PageSetup.MarginBottom) - ItemRect.bottomRight();

	ScenePosition.setX(qMax(MarginsMin.x(), qMin(ScenePosition.x(), MarginsMax.x())));
	ScenePosition.setY(qMax(MarginsMin.y(), qMin(ScenePosition.y(), MarginsMax.y())));

	return Parent ? Parent->mapFromScene(ScenePosition) : ScenePosition;
}

// drag and snap movable elements
constexpr QGraphicsItem::GraphicsItemFlags SW_MOVABLE_ITEM_FLAGS = QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsFocusable | QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemSendsGeometryChanges;

// use to place an element
static void swPlaceElement(QGraphicsItem* Item, const swInstructions* Instructions, const QPointF& Position)
{
	swInstructionsElementKey Key;
	swGetElementKey(Item, Key);

	Item->setData(SW_PLACEMENT_POSITION_DATA, Position);

	if (Instructions->IsElementLocked(Key))
	{
		Item->setFlag(QGraphicsItem::ItemIsMovable, false);
		Item->setPos(Instructions->GetLockedPosition(Key));
	}
	else
		Item->setPos(Position + Instructions->GetElementOffset(Key));
}

// use to get pixels per page point
static qreal swGetRenderScale(const swInstructions* Instructions)
{
	const swInstructionsPageSetup& PageSetup = Instructions->mPageSetup;

	return qMax(1.0, SW_INSTRUCTIONS_RENDER_SIZE / qMax<qreal>(qMax(PageSetup.Width, PageSetup.Height), 1.0));
}

// use to crop an image to its pixels
static QImage swCropImage(const QImage& Image, QPoint& Offset)
{
	Offset = QPoint();

	const QImage Source = Image.convertToFormat(QImage::Format_ARGB32);
	int Left = Source.width(), Top = Source.height(), Right = -1, Bottom = -1;

	for (int Y = 0; Y < Source.height(); Y++)
	{
		const QRgb* Line = reinterpret_cast<const QRgb*>(Source.constScanLine(Y));

		for (int X = 0; X < Source.width(); X++)
		{
			if (!qAlpha(Line[X]))
				continue;

			Left = qMin(Left, X);
			Right = qMax(Right, X);
			Top = qMin(Top, Y);
			Bottom = qMax(Bottom, Y);
		}
	}

	if (Right < 0)
		return QImage();

	Offset = QPoint(Left, Top);

	return Source.copy(Left, Top, Right - Left + 1, Bottom - Top + 1);
}

// use to place an item on a grid cell
static QPointF swGetPlacementPosition(const QRectF& Anchor, const QSizeF& Size, int Placement)
{
	auto GetOffset = [](qreal Start, qreal Length, qreal ItemLength, int Cell)
	{
		switch (Cell)
		{
		case 0:
			return Start - ItemLength;

		case 1:
			return Start;

		case 2:
			return Start + (Length - ItemLength) / 2.0;

		case 3:
			return Start + Length - ItemLength;

		default:
			return Start + Length;
		}
	};

	const int Row = Placement / SW_PLACEMENT_GRID_SIZE;
	const int Column = Placement % SW_PLACEMENT_GRID_SIZE;

	return QPointF(GetOffset(Anchor.left(), Anchor.width(), Size.width(), Column), GetOffset(Anchor.top(), Anchor.height(), Size.height(), Row));
}

swInstructionsStepImageItem::swInstructionsStepImageItem(QGraphicsItem* Parent, swInstructions* Instructions, swModel* Model, swStep Step)
	: swInstructionsBoxItem(Parent), mInstructions(Instructions), mModel(Model), mStep(Step)
{
	setFlags(SW_MOVABLE_ITEM_FLAGS);
	SetBoxStyle(mInstructions->GetBoxStyle(swInstructionsBoxElement::StepImage, mModel, mStep));
}

void swInstructionsStepImageItem::SetCoverImage()
{
	mCoverImage = true;
	SetBoxStyle(mInstructions->GetBoxStyle(swInstructionsBoxElement::CoverImage, mModel, mStep));
}

QVariant swInstructionsStepImageItem::itemChange(GraphicsItemChange Change, const QVariant& Value)
{
	if (Change == QGraphicsItem::ItemPositionChange)
		return swSnapItemPosition(this, mInstructions, Value);

	return QGraphicsPixmapItem::itemChange(Change, Value);
}

void swInstructionsStepImageItem::Update()
{
	swView View(swViewType::View, mModel);

	View.SetOffscreenContext();
	View.MakeCurrent();

	// step background behind the image
	const QColor BackgroundColor = mInstructions->GetColorProperty(swInstructionsPropertyType::StepBackgroundColor, mModel, mStep);
	View.SetBackgroundColorOverride(SW_RGBA(BackgroundColor.red(), BackgroundColor.green(), BackgroundColor.blue(), 0));

	// render large and draw at page size
	const qreal Scale = swGetRenderScale(mInstructions);
	const int Width = qRound(mWidth * Scale);
	const int Height = qRound(mHeight * Scale);
	View.SetSize(Width, Height);
	View.SetLineWidthScale(Scale);

	// cover shows the finished model
	const bool FadeSteps = !mCoverImage && mInstructions->GetBoolProperty(swInstructionsPropertyType::FadePreviousSteps, mModel, mStep);
	const QColor FadeColor = mInstructions->GetColorProperty(swInstructionsPropertyType::FadeColor, mModel, mStep);
	View.SetFadeStepsOverride(FadeSteps, swRGBAFromQColor(FadeColor));

	const bool HighlightNewParts = !mCoverImage && mInstructions->GetBoolProperty(swInstructionsPropertyType::HighlightNewParts, mModel, mStep);
	const QColor HighlightColor = mInstructions->GetColorProperty(swInstructionsPropertyType::HighlightColor, mModel, mStep);
	View.SetHighlightNewPartsOverride(HighlightNewParts, swRGBAFromQColor(HighlightColor));

	// work on a copy of the main camera
	View.SetCamera(View.GetCamera(), true);
	View.GetCamera()->SetProjection(static_cast<swCameraProjection>(mInstructions->GetIntProperty(swInstructionsPropertyType::ModelProjection, mModel, mStep)));

	// show the step rotation view
	View.ApplyStepView(mStep);

	// frame the complete model
	View.ZoomExtentsAtStep(mModel->GetLastStep(), swGeometryBoundsMode::IncludeFallback, false);

	std::vector<QImage> Images = View.GetStepImages(mStep, mStep);
	mPartRegions.clear();

	if (Images.empty())
	{
		setPixmap(QPixmap());
		return;
	}

	QPoint CropOffset;
	QImage Image = swCropImage(Images.front(), CropOffset);
	Image.setDevicePixelRatio(Scale);
	setPixmap(QPixmap::fromImage(Image));

	// project the step parts for pointers
	View.SetSize(Width, Height);

	const swStep CurrentStep = mModel->GetCurrentStep();
	const qreal Inset = mBoxStyle.GetInset();
	const QPointF Origin(Inset - CropOffset.x() / Scale, Inset - CropOffset.y() / Scale);
	const std::vector<std::unique_ptr<swPart>>& Parts = mModel->GetParts();

	mModel->SetTemporaryStep(mStep);

	for (size_t PartIndex = 0; PartIndex < Parts.size(); PartIndex++)
	{
		const swPart* Part = Parts[PartIndex].get();

		if (Part->IsHidden() || !Part->IsVisible(mStep))
			continue;

		swVector3 Min(FLT_MAX, FLT_MAX, FLT_MAX);
		swVector3 Max(-FLT_MAX, -FLT_MAX, -FLT_MAX);
		Part->CompareBoundingBox(Min, Max);

		swVector3 Corners[8];
		swGetBoxCorners(Min, Max, Corners);

		QRectF Rect;

		for (const swVector3& Corner : Corners)
		{
			const swVector3 Point = View.ProjectPoint(Corner);
			const QPointF ImagePoint = Origin + QPointF(Point.x, Height - Point.y) / Scale;

			Rect = Rect.isNull() ? QRectF(ImagePoint, QSizeF(0.0, 0.0)) : Rect.united(QRectF(ImagePoint, QSizeF(0.0, 0.0)));
		}

		Rect = Rect.normalized();

		const swVector3 Center = View.ProjectPoint((Min + Max) * 0.5f);
		mPartRegions.push_back({ static_cast<int>(PartIndex), Rect, Origin + QPointF(Center.x, Height - Center.y) / Scale });
	}

	mModel->SetTemporaryStep(CurrentStep);

	if (!mModel->IsActive())
		mModel->CalculateStep(SW_STEP_MAXIMUM);
}

bool swInstructionsStepImageItem::FindPart(const QPointF& Point, int& PartIndex) const
{
	// the smallest part under the point wins
	const swPartRegion* Found = nullptr;
	qreal FoundArea = 0.0;

	for (const swPartRegion& Region : mPartRegions)
	{
		if (!Region.Rect.contains(Point))
			continue;

		const qreal Area = Region.Rect.width() * Region.Rect.height();

		if (!Found || Area < FoundArea)
		{
			Found = &Region;
			FoundArea = Area;
		}
	}

	if (!Found)
	{
		qreal FoundDistance = 24.0;

		for (const swPartRegion& Region : mPartRegions)
		{
			const qreal Distance = QLineF(Point, Region.Center).length();

			if (Distance < FoundDistance)
			{
				Found = &Region;
				FoundDistance = Distance;
			}
		}
	}

	if (!Found)
		return false;

	PartIndex = Found->PartIndex;

	return true;
}

bool swInstructionsStepImageItem::GetPartPoint(int PartIndex, QPointF& Point) const
{
	for (const swPartRegion& Region : mPartRegions)
	{
		if (Region.PartIndex != PartIndex)
			continue;

		Point = Region.Center;

		return true;
	}

	return false;
}

swInstructionsStepNumberItem::swInstructionsStepNumberItem(QGraphicsItem* Parent, swInstructions* Instructions, swModel* Model, swStep Step)
	: swInstructionsBoxItem(Parent), mInstructions(Instructions), mModel(Model), mStep(Step)
{
	setFlags(SW_MOVABLE_ITEM_FLAGS);
	SetBoxStyle(mInstructions->GetBoxStyle(swInstructionsBoxElement::StepNumber, mModel, mStep));
}

QVariant swInstructionsStepNumberItem::itemChange(GraphicsItemChange Change, const QVariant& Value)
{
	if (Change == QGraphicsItem::ItemPositionChange)
		return swSnapItemPosition(this, mInstructions, Value);

	return QGraphicsSimpleTextItem::itemChange(Change, Value);
}

void swInstructionsStepNumberItem::Update()
{
	bool Visible = mInstructions->GetBoolProperty(swInstructionsPropertyType::ShowStepNumber, mModel, mStep);
	setVisible(Visible);

	QFont StepNumberFont = mInstructions->GetFontProperty(swInstructionsPropertyType::StepNumberFont, mModel, mStep);

	setFont(StepNumberFont);
	setBrush(QBrush(mInstructions->GetColorProperty(swInstructionsPropertyType::StepNumberColor, mModel, mStep)));
	setText(QString::number(mStep));
}

swInstructionsPageNumberItem::swInstructionsPageNumberItem(QGraphicsItem* Parent, swInstructions* Instructions, size_t PageIndex)
	: swInstructionsBoxItem(Parent), mInstructions(Instructions)
{
	setFlags(SW_MOVABLE_ITEM_FLAGS);
	setZValue(1.0);
	SetBoxStyle(mInstructions->GetBoxStyle(swInstructionsBoxElement::PageNumber, nullptr, 0));

	const QLocale Locale;
	const QString PageNumber = swFormatIntegerLocalized(static_cast<qlonglong>(PageIndex + 1), Locale);
	const swInstructionsPageNumberFormat Format = static_cast<swInstructionsPageNumberFormat>(mInstructions->GetIntProperty(swInstructionsPropertyType::PageNumberFormat, nullptr, 0));

	setFont(mInstructions->GetFontProperty(swInstructionsPropertyType::PageNumberFont, nullptr, 0));
	setBrush(QBrush(mInstructions->GetColorProperty(swInstructionsPropertyType::PageNumberColor, nullptr, 0)));

	if (Format == swInstructionsPageNumberFormat::NumberOfPages)
		setText(QCoreApplication::translate("swInstructionsDialog", "%1 / %2").arg(PageNumber, swFormatIntegerLocalized(static_cast<qlonglong>(mInstructions->mPages.size()), Locale)));
	else
		setText(PageNumber);
}

QVariant swInstructionsPageNumberItem::itemChange(GraphicsItemChange Change, const QVariant& Value)
{
	if (Change == QGraphicsItem::ItemPositionChange)
		return swSnapItemPosition(this, mInstructions, Value);

	return QGraphicsSimpleTextItem::itemChange(Change, Value);
}

swInstructionsPartsListItem::swInstructionsPartsListItem(QGraphicsItem* Parent, swInstructions* Instructions, swModel* Model, swStep Step)
	: swInstructionsBoxItem(Parent), mInstructions(Instructions), mModel(Model), mStep(Step)
{
	setFlags(SW_MOVABLE_ITEM_FLAGS);

	// parts image on the background color
	SetBoxStyle(mInstructions->GetBoxStyle(swInstructionsBoxElement::PartsList, mModel, mStep));
	SetContentBackground(true);
}

QVariant swInstructionsPartsListItem::itemChange(GraphicsItemChange Change, const QVariant& Value)
{
	if (Change == QGraphicsItem::ItemPositionChange)
		return swSnapItemPosition(this, mInstructions, Value);

	return QGraphicsPixmapItem::itemChange(Change, Value);
}

void swInstructionsPartsListItem::Update()
{
	// parts list settings for the materials page
	bool Visible = mBillOfMaterials || mInstructions->GetBoolProperty(swInstructionsPropertyType::ShowStepPLI, mModel, mStep);
	setVisible(Visible);

	if (!Visible)
		return;

	QColor BackgroundColor = mInstructions->GetColorProperty(swInstructionsPropertyType::PLIBackgroundColor, mModel, mStep);
	QFont Font = mInstructions->GetFontProperty(swInstructionsPropertyType::PLIFont, mModel, mStep);
	QColor TextColor = mInstructions->GetColorProperty(swInstructionsPropertyType::PLITextColor, mModel, mStep);
	const bool ShowCounts = mInstructions->GetBoolProperty(swInstructionsPropertyType::PLIShowCounts, mModel, mStep);
	const swPartsListSort Sort = static_cast<swPartsListSort>(mInstructions->GetIntProperty(swInstructionsPropertyType::PLISortOrder, mModel, mStep));
	const swPartsListLayout Layout = static_cast<swPartsListLayout>(mInstructions->GetIntProperty(swInstructionsPropertyType::PLILayout, mModel, mStep));
	const swPartsListAlignment Alignment = static_cast<swPartsListAlignment>(mInstructions->GetIntProperty(swInstructionsPropertyType::PLIAlignment, mModel, mStep));
	const int ThumbnailSize = mInstructions->GetIntProperty(swInstructionsPropertyType::PLIThumbnailSize, mModel, mStep);
	const QColor ThumbnailBackgroundColor = mInstructions->GetColorProperty(swInstructionsPropertyType::PLIThumbnailBackgroundColor, mModel, mStep);

	const swResult<QImage> PartsImage = mModel->GetPartsListImage(mMaxWidth, ThumbnailSize, mBillOfMaterials ? 0 : mStep, swRGBAFromQColor(BackgroundColor), swRGBAFromQColor(ThumbnailBackgroundColor), Font, TextColor, false, ShowCounts, Sort, Layout, Alignment, mMaxHeight, swGetRenderScale(mInstructions));

	if (!PartsImage)
	{
		setPixmap(QPixmap());

		QWidget* Parent = scene() && !scene()->views().isEmpty() ? scene()->views().front() : nullptr;
		QMessageBox::warning(Parent, QObject::tr("Create Parts List"), PartsImage.error());

		return;
	}

	setPixmap(QPixmap::fromImage(PartsImage.value()));
}

swInstructionsCoverTextItem::swInstructionsCoverTextItem(QGraphicsItem* Parent, swInstructions* Instructions, swModel* Model, bool Author)
	: swInstructionsBoxItem(Parent), mInstructions(Instructions), mModel(Model), mAuthor(Author)
{
	setFlags(SW_MOVABLE_ITEM_FLAGS);
	setZValue(1.0);
	SetBoxStyle(mInstructions->GetBoxStyle(Author ? swInstructionsBoxElement::CoverAuthor : swInstructionsBoxElement::CoverTitle, mModel, 0));
}

QVariant swInstructionsCoverTextItem::itemChange(GraphicsItemChange Change, const QVariant& Value)
{
	if (Change == QGraphicsItem::ItemPositionChange)
		return swSnapItemPosition(this, mInstructions, Value);

	return QGraphicsTextItem::itemChange(Change, Value);
}

bool swEditNoticeText(QWidget* Parent, QString& Text, bool NewNotice)
{
	bool Accepted = false;
	const QString Title = NewNotice ? QCoreApplication::translate("swInstructionsDialog", "Add Notice") : QCoreApplication::translate("swInstructionsDialog", "Edit Notice");
	const QString NewText = QInputDialog::getMultiLineText(Parent, Title, QCoreApplication::translate("swInstructionsDialog", "Notice text:"), Text, &Accepted);

	if (!Accepted || NewText.trimmed().isEmpty())
		return false;

	Text = NewText;

	return true;
}

swInstructionsNoticeItem::swInstructionsNoticeItem(QGraphicsItem* Parent, swInstructions* Instructions, const swInstructionsNotice& Notice)
	: swInstructionsBoxItem(Parent), mInstructions(Instructions), mId(Notice.Id)
{
	setFlags(SW_MOVABLE_ITEM_FLAGS);
	setZValue(1.0);
	SetBoxStyle(mInstructions->GetBoxStyle(swInstructionsBoxElement::Notice, nullptr, 0));

	setFont(mInstructions->GetFontProperty(swInstructionsPropertyType::NoticeFont, nullptr, 0));
	setDefaultTextColor(mInstructions->GetColorProperty(swInstructionsPropertyType::NoticeTextColor, nullptr, 0));
	setPlainText(Notice.Text);
}

QVariant swInstructionsNoticeItem::itemChange(GraphicsItemChange Change, const QVariant& Value)
{
	if (Change == QGraphicsItem::ItemPositionChange)
		return swSnapItemPosition(this, mInstructions, Value);

	return QGraphicsTextItem::itemChange(Change, Value);
}

void swInstructionsNoticeItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* Event)
{
	Event->accept();

	const swInstructionsNotice* Notice = mInstructions->FindNotice(mId);
	QWidget* Parent = scene() && !scene()->views().isEmpty() ? scene()->views().front() : nullptr;

	if (!Notice)
		return;

	// rebuild after the click
	const int Id = mId;
	swInstructions* Instructions = mInstructions;
	QString Text = Notice->Text;

	QTimer::singleShot(0, Instructions, [Instructions, Parent, Id, Text]() mutable
	{
		if (swEditNoticeText(Parent, Text, false))
			Instructions->SetNoticeText(Id, Text);
	});
}

QPointF swGetPointerAnchor(const QRectF& BorderRect, int Anchor)
{
	switch (Anchor)
	{
	case 0:
		return BorderRect.topLeft();

	case 1:
		return QPointF(BorderRect.center().x(), BorderRect.top());

	case 2:
		return BorderRect.topRight();

	case 3:
		return QPointF(BorderRect.right(), BorderRect.center().y());

	case 4:
		return BorderRect.bottomRight();

	case 5:
		return QPointF(BorderRect.center().x(), BorderRect.bottom());

	case 6:
		return BorderRect.bottomLeft();

	default:
		return QPointF(BorderRect.left(), BorderRect.center().y());
	}
}

// use to get an element border
static bool swGetBorderRect(const QGraphicsItem* Item, const QGraphicsItem* Parent, QRectF& BorderRect)
{
	const swInstructionsBoxBase* Box = dynamic_cast<const swInstructionsBoxBase*>(Item);

	if (!Box || !Item->isVisible())
		return false;

	BorderRect = Item->mapRectToItem(Parent, Box->GetBorderRect());

	return true;
}

swInstructionsPointerItem::swInstructionsPointerItem(QGraphicsItem* Parent, swInstructions* Instructions, const swInstructionsPointer& Pointer, QGraphicsItem* Source, QGraphicsItem* Target)
	: QGraphicsItem(Parent), mInstructions(Instructions), mPointer(Pointer), mSource(Source), mTarget(Target)
{
	setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsFocusable);
	setZValue(2.0);

	// pointers default to the pointer properties
	const int Style = mPointer.Style >= 0 ? mPointer.Style : mInstructions->GetIntProperty(swInstructionsPropertyType::PointerStyle, nullptr, 0);
	const int Tip = mPointer.Tip >= 0 ? mPointer.Tip : mInstructions->GetIntProperty(swInstructionsPropertyType::PointerTip, nullptr, 0);

	mStyle = static_cast<swInstructionsPointerStyle>(Style);
	mTip = static_cast<swInstructionsPointerTip>(Tip);
	mColor = mInstructions->GetColorProperty(swInstructionsPropertyType::PointerColor, nullptr, 0);

	// body follows the element border
	const swInstructionsBoxBase* Box = dynamic_cast<const swInstructionsBoxBase*>(mSource);
	const int Width = mInstructions->GetIntProperty(swInstructionsPropertyType::PointerWidth, nullptr, 0);

	mSourceBorderWidth = Box ? Box->GetBorderWidth() : 0.0;
	mWidth = Width > 0 ? Width : qMax(mSourceBorderWidth, 2.0);

	// the tip is at least three times the body
	mTipSize = qMax<qreal>(mInstructions->GetIntProperty(swInstructionsPropertyType::PointerTipSize, nullptr, 0), 3.0 * mWidth);

	UpdateGeometry();
}

void swInstructionsPointerItem::UpdateGeometry()
{
	QRectF SourceRect;

	if (!swGetBorderRect(mSource, parentItem(), SourceRect))
		return;

	QPointF TipPosition = mPointer.TipPosition;
	QRectF TargetRect;

	if (mDraggingTip)
		TipPosition = mTipPosition;
	else if (mPointer.TargetType == swInstructionsPointerTarget::Part && mTarget)
	{
		const swInstructionsStepImageItem* ImageItem = dynamic_cast<const swInstructionsStepImageItem*>(mTarget);
		QPointF PartPoint;

		if (ImageItem && ImageItem->GetPartPoint(mPointer.TargetPart, PartPoint))
			TipPosition = mTarget->mapToItem(parentItem(), PartPoint);
	}
	else if (mPointer.TargetType == swInstructionsPointerTarget::Element && swGetBorderRect(mTarget, parentItem(), TargetRect))
	{
		// tip touches the target border
		const QPointF Center = SourceRect.center();

		if (TargetRect.contains(Center))
			TipPosition = TargetRect.center();
		else
			TipPosition = QPointF(qBound(TargetRect.left(), Center.x(), TargetRect.right()), qBound(TargetRect.top(), Center.y(), TargetRect.bottom()));
	}

	// leave the middle of the border
	const qreal BorderHalf = mSourceBorderWidth / 2.0;
	const QRectF AnchorRect = SourceRect.adjusted(BorderHalf, BorderHalf, -BorderHalf, -BorderHalf);

	// automatic anchor nearest the tip
	int Anchor = mPointer.Anchor;

	if (Anchor < 0 || Anchor >= SW_POINTER_ANCHOR_COUNT)
	{
		qreal AnchorDistance = 0.0;

		for (int Index = 0; Index < SW_POINTER_ANCHOR_COUNT; Index++)
		{
			const qreal Distance = QLineF(swGetPointerAnchor(AnchorRect, Index), TipPosition).length();

			if (Index == 0 || Distance < AnchorDistance)
			{
				Anchor = Index;
				AnchorDistance = Distance;
			}
		}
	}

	prepareGeometryChange();
	mSourceRect = SourceRect;
	mAnchor = Anchor;
	mBasePosition = swGetPointerAnchor(AnchorRect, Anchor);
	mTipPosition = TipPosition;
	update();
}

QPainterPath swInstructionsPointerItem::GetPath(const QRectF& SourceRect, qreal BorderWidth) const
{
	QPainterPath Path;
	const QLineF Line(mBasePosition, mTipPosition);

	if (Line.length() < 0.5)
		return Path;

	const QPointF Direction = (mTipPosition - mBasePosition) / Line.length();
	const QPointF Normal(-Direction.y(), Direction.x());

	// wedge narrows to the tip
	if (mStyle == swInstructionsPointerStyle::Wedge)
		Path.addPath(GetWedgePath(SourceRect, BorderWidth));

	if (mTip == swInstructionsPointerTip::Dot)
		Path.addEllipse(mTipPosition, mTipSize / 2.0, mTipSize / 2.0);
	else if (mStyle == swInstructionsPointerStyle::Line)
	{
		const QPointF Back = mTipPosition - Direction * mTipSize;
		const qreal HalfWidth = mTipSize * 0.4;

		Path.moveTo(mTipPosition);
		Path.lineTo(Back + Normal * HalfWidth);
		Path.lineTo(Back - Normal * HalfWidth);
		Path.closeSubpath();
	}

	return Path;
}

// use to sit the wedge on the box edge
QPainterPath swInstructionsPointerItem::GetWedgePath(const QRectF& SourceRect, qreal BorderWidth) const
{
	const QRectF& Rect = SourceRect;
	const QPointF Corners[4] = { Rect.topLeft(), Rect.topRight(), Rect.bottomRight(), Rect.bottomLeft() };
	QPointF Start, End, Normal;

	if (mAnchor % 2 == 0)
	{
		// corner wedges run along the far side
		const QPointF Corner = Corners[mAnchor / 2];
		const qreal SignX = Corner.x() > Rect.center().x() ? 1.0 : -1.0;
		const qreal SignY = Corner.y() > Rect.center().y() ? 1.0 : -1.0;
		const bool Vertical = (mTipPosition.x() - Corner.x()) * SignX >= (mTipPosition.y() - Corner.y()) * SignY;
		const qreal Length = qMin(mTipSize, Vertical ? Rect.height() : Rect.width());

		Normal = Vertical ? QPointF(SignX, 0.0) : QPointF(0.0, SignY);
		Start = Corner;
		End = Corner - (Vertical ? QPointF(0.0, SignY) : QPointF(SignX, 0.0)) * Length;
	}
	else
	{
		// center side wedges
		const QPointF Middle = swGetPointerAnchor(Rect, mAnchor);
		const bool Vertical = mAnchor == 3 || mAnchor == 7;
		const qreal HalfLength = qMin(mTipSize, Vertical ? Rect.height() : Rect.width()) / 2.0;
		const QPointF Along = Vertical ? QPointF(0.0, HalfLength) : QPointF(HalfLength, 0.0);

		Normal = Vertical ? QPointF(mAnchor == 3 ? 1.0 : -1.0, 0.0) : QPointF(0.0, mAnchor == 1 ? -1.0 : 1.0);
		Start = Middle - Along;
		End = Middle + Along;
	}

	// base reaches through the border
	const QPointF Inset = Normal * BorderWidth;
	QPainterPath Path;

	Path.moveTo(Start);
	Path.lineTo(mTipPosition);
	Path.lineTo(End);
	Path.lineTo(End - Inset);
	Path.lineTo(Start - Inset);
	Path.closeSubpath();

	return Path;
}

// use to keep the pointer outside its box
QPainterPath swInstructionsPointerItem::GetClipPath(const QRectF& SourceRect, qreal BorderWidth) const
{
	const QRectF InnerRect = SourceRect.adjusted(BorderWidth, BorderWidth, -BorderWidth, -BorderWidth);
	QPainterPath Path;

	Path.addRect(boundingRect());

	if (InnerRect.isValid())
	{
		QPainterPath InnerPath;
		InnerPath.addRect(InnerRect);
		Path = Path.subtracted(InnerPath);
	}

	return Path;
}

QRectF swInstructionsPointerItem::boundingRect() const
{
	const qreal Extent = qMax(mTipSize, mWidth) + mSourceBorderWidth + mWidth + 8.0;

	return QRectF(mBasePosition, mTipPosition).normalized().adjusted(-Extent, -Extent, Extent, Extent);
}

QPainterPath swInstructionsPointerItem::shape() const
{
	QPainterPath Path;
	Path.moveTo(mBasePosition);
	Path.lineTo(mTipPosition);

	// easy to pick at any width
	QPainterPathStroker Stroker;
	Stroker.setWidth(qMax(mWidth, 8.0));

	QPainterPath Shape = Stroker.createStroke(Path);
	Shape.addPath(GetPath(mSourceRect, mSourceBorderWidth));
	Shape.addEllipse(mTipPosition, qMax(mTipSize, 10.0), qMax(mTipSize, 10.0));

	return Shape;
}

void swInstructionsPointerItem::paint(QPainter* Painter, const QStyleOptionGraphicsItem* Option, QWidget* Widget)
{
	Q_UNUSED(Widget);

	// meet the border on its pixels
	QRectF SourceRect = mSourceRect;
	qreal BorderWidth = mSourceBorderWidth;
	swSnapBorderRect(Painter->worldTransform(), SourceRect, BorderWidth);

	Painter->save();
	Painter->setRenderHint(QPainter::Antialiasing, true);
	Painter->setClipPath(GetClipPath(SourceRect, BorderWidth));

	const QLineF Line(mBasePosition, mTipPosition);

	if (mStyle == swInstructionsPointerStyle::Line && Line.length() >= 0.5)
	{
		// start the line inside the box
		const qreal BorderHalf = BorderWidth / 2.0;
		const QPointF Base = swGetPointerAnchor(SourceRect.adjusted(BorderHalf, BorderHalf, -BorderHalf, -BorderHalf), mAnchor);
		const QPointF Direction = (mTipPosition - Base) / QLineF(Base, mTipPosition).length();
		const qreal Width = mWidth == mSourceBorderWidth ? BorderWidth : mWidth;

		// stop the line at the arrowhead
		QPointF LineEnd = mTipPosition;

		if (mTip == swInstructionsPointerTip::Arrowhead && Line.length() > mTipSize)
			LineEnd = mTipPosition - Direction * (mTipSize * 0.5);

		QPen Pen(mColor, Width);
		Pen.setCapStyle(Qt::FlatCap);
		Painter->setPen(Pen);
		Painter->drawLine(Base - Direction * (BorderWidth + Width), LineEnd);
	}

	Painter->setPen(Qt::NoPen);
	Painter->setBrush(mColor);
	Painter->drawPath(GetPath(SourceRect, BorderWidth));
	Painter->setClipping(false);

	if (Option->state & QStyle::State_Selected)
	{
		// show anchor and tip handles
		QPen Pen(Option->palette.highlight(), 2.0);
		Pen.setCosmetic(true);

		Painter->setPen(Pen);
		Painter->setBrush(Option->palette.base());
		Painter->drawEllipse(mBasePosition, 4.0, 4.0);
		Painter->drawEllipse(mTipPosition, 5.0, 5.0);
	}

	Painter->restore();
}

void swInstructionsPointerItem::mousePressEvent(QGraphicsSceneMouseEvent* Event)
{
	QGraphicsItem::mousePressEvent(Event);

	// drag the tip to a new target
	if (Event->button() == Qt::LeftButton && QLineF(Event->pos(), mTipPosition).length() <= qMax(mTipSize, 10.0))
	{
		mDraggingTip = true;
		Event->accept();
	}
}

void swInstructionsPointerItem::mouseMoveEvent(QGraphicsSceneMouseEvent* Event)
{
	if (!mDraggingTip)
	{
		QGraphicsItem::mouseMoveEvent(Event);
		return;
	}

	mTipPosition = Event->pos();
	UpdateGeometry();
}

void swInstructionsPointerItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* Event)
{
	QGraphicsItem::mouseReleaseEvent(Event);

	if (!mDraggingTip)
		return;

	mDraggingTip = false;

	// point at the topmost target
	swInstructionsPointerTarget TargetType = swInstructionsPointerTarget::Point;
	swInstructionsElementKey TargetElement = { swInstructionsPlacementElement::None, nullptr, 0 };
	int TargetPart = -1;
	const QPointF TipPosition = mapToItem(parentItem(), Event->pos());
	const QList<QGraphicsItem*> Items = scene()->items(Event->scenePos());

	for (const QGraphicsItem* Item : Items)
	{
		swInstructionsElementKey Key;

		if (Item == mSource || dynamic_cast<const swInstructionsPointerItem*>(Item) || !swGetElementKey(Item, Key))
			continue;

		if (const swInstructionsStepImageItem* ImageItem = dynamic_cast<const swInstructionsStepImageItem*>(Item))
		{
			if (ImageItem->FindPart(ImageItem->mapFromScene(Event->scenePos()), TargetPart))
			{
				TargetType = swInstructionsPointerTarget::Part;
				TargetElement = Key;
				break;
			}
		}

		TargetType = swInstructionsPointerTarget::Element;
		TargetElement = Key;
		break;
	}

	// rebuild after the release
	swInstructions* Instructions = mInstructions;
	const int Id = mPointer.Id;

	QTimer::singleShot(0, Instructions, [Instructions, Id, TargetType, TargetElement, TargetPart, TipPosition]()
	{
		Instructions->SetPointerTarget(Id, TargetType, TargetElement, TargetPart, TipPosition);
	});
}

swInstructionsPageWidget::swInstructionsPageWidget(QWidget* Parent, swInstructions* Instructions, swInstructionsPropertiesWidget* PropertiesWidget, swInstructionsPlacementWidget* PlacementWidget)
	: QGraphicsView(Parent), mInstructions(Instructions), mPropertiesWidget(PropertiesWidget), mPlacementWidget(PlacementWidget)
{
	QGraphicsScene* Scene = new QGraphicsScene();
	setScene(Scene);
	setBackgroundBrush(palette().color(QPalette::Mid));

	connect(mInstructions, &swInstructions::StepSettingsChanged, this, &swInstructionsPageWidget::StepSettingsChanged);
	connect(Scene, &QGraphicsScene::selectionChanged, this, &swInstructionsPageWidget::SelectionChanged);
	connect(mPlacementWidget, &swInstructionsPlacementWidget::AlignSelection, this, &swInstructionsPageWidget::AlignSelection);

	connect(mInstructions, &swInstructions::GridChanged, this, [this]()
	{
		viewport()->update();
	});
}

void swInstructionsPageWidget::StepSettingsChanged(swModel* Model, swStep Step)
{
	QGraphicsScene* Scene = scene();

	const QList<QGraphicsItem*> Items = Scene->items();

	for (QGraphicsItem* Item : Items)
	{
		swInstructionsStepImageItem* ImageItem = dynamic_cast<swInstructionsStepImageItem*>(Item);

		if (ImageItem)
		{
			if (!Model || (ImageItem->GetModel() == Model && ImageItem->GetStep() == Step))
				ImageItem->Update();

			continue;
		}

		swInstructionsStepNumberItem* NumberItem = dynamic_cast<swInstructionsStepNumberItem*>(Item);

		if (NumberItem)
		{
			if (!Model || (NumberItem->GetModel() == Model && NumberItem->GetStep() == Step))
				NumberItem->Update();

			continue;
		}

		swInstructionsPartsListItem* PartsItem = dynamic_cast<swInstructionsPartsListItem*>(Item);

		if (PartsItem)
		{
			if (!Model || (PartsItem->GetModel() == Model && PartsItem->GetStep() == Step))
				PartsItem->Update();

			continue;
		}
	}
}

void swInstructionsPageWidget::SelectionChanged()
{
	// reselect elements after a rebuild
	if (mRebuildingPage)
		return;

	QGraphicsItem* Focus = nullptr;
	const QList<QGraphicsItem*> SelectedItems = scene()->selectedItems();
	std::vector<swInstructionsElementKey> SelectedKeys;

	if (!SelectedItems.isEmpty())
		Focus = SelectedItems.first();

	for (const QGraphicsItem* Item : SelectedItems)
	{
		swInstructionsElementKey Key;

		if (swGetElementKey(Item, Key) && std::get<0>(Key) != swInstructionsPlacementElement::Pointer)
			SelectedKeys.push_back(Key);
	}

	mSelectionOrder.erase(std::remove_if(mSelectionOrder.begin(), mSelectionOrder.end(), [&SelectedKeys](const swInstructionsElementKey& Key)
	{
		return std::find(SelectedKeys.begin(), SelectedKeys.end(), Key) == SelectedKeys.end();
	}), mSelectionOrder.end());

	for (const swInstructionsElementKey& Key : SelectedKeys)
		if (std::find(mSelectionOrder.begin(), mSelectionOrder.end(), Key) == mSelectionOrder.end())
			mSelectionOrder.push_back(Key);

	mPropertiesWidget->SelectionChanged(Focus);
	mPlacementWidget->SelectionChanged(Focus, static_cast<int>(mSelectionOrder.size()));
}

// use to align the selected elements
void swInstructionsPageWidget::AlignSelection(int Placement, bool LastSelection)
{
	if (mSelectionOrder.size() < 2)
		return;

	std::map<swInstructionsElementKey, QGraphicsItem*> ElementItems;
	const QList<QGraphicsItem*> SelectedItems = scene()->selectedItems();

	for (QGraphicsItem* Item : SelectedItems)
	{
		swInstructionsElementKey Key;

		if (swGetElementKey(Item, Key))
			ElementItems[Key] = Item;
	}

	const swInstructionsElementKey& ReferenceKey = LastSelection ? mSelectionOrder.back() : mSelectionOrder.front();
	const auto ReferenceIt = ElementItems.find(ReferenceKey);

	if (ReferenceIt == ElementItems.end())
		return;

	const QRectF ReferenceRect = ReferenceIt->second->sceneBoundingRect();
	std::map<swInstructionsElementKey, QPointF> Offsets;

	// locked elements keep their position
	for (const swInstructionsElementKey& Key : mSelectionOrder)
	{
		const auto ItemIt = ElementItems.find(Key);

		if (Key == ReferenceKey || ItemIt == ElementItems.end() || mInstructions->IsElementLocked(Key))
			continue;

		const QRectF ItemRect = ItemIt->second->sceneBoundingRect();
		const QPointF Distance = swGetPlacementPosition(ReferenceRect, ItemRect.size(), Placement) - ItemRect.topLeft();

		if (!Distance.isNull())
			Offsets[Key] = mInstructions->GetElementOffset(Key) + Distance;
	}

	mInstructions->SetElementOffsets(Offsets);
}

// use to add the page elements
static void swAddPageContent(swInstructions* Instructions, const swInstructionsPage* Page, QGraphicsScene* Scene, QGraphicsRectItem* PageItem, const QRectF& MarginsRect)
{
	if (Page->Type == swInstructionsPageType::Cover)
	{
		swModel* Model = Page->Model;
		const swModelProperties& Properties = Model->GetProperties();
		QString Title = Properties.mDescription.trimmed();

		if (Title.isEmpty())
			Title = Properties.mModelName.trimmed();

		if (Title.isEmpty())
			Title = QFileInfo(Properties.mFileName).completeBaseName();

		const QFont TitleFont = Instructions->GetFontProperty(swInstructionsPropertyType::CoverTitleFont, Model, 0);
		const QColor TextColor = Instructions->GetColorProperty(swInstructionsPropertyType::CoverTitleColor, Model, 0);

		struct swCoverText
		{
			swInstructionsCoverTextItem* Item;
			int Placement;
			QSizeF Size;
		};

		std::vector<swCoverText> CoverTexts;

		auto AddCoverText = [&](const QString& Text, const QFont& Font, bool Author, swInstructionsPropertyType PlacementType)
		{
			const int Placement = swGetValidPlacement(Instructions->GetIntProperty(PlacementType, Model, 0), true);
			const int Column = Placement % SW_PLACEMENT_GRID_SIZE;

			// align wrapped text lines
			QTextOption TextOption;

			if (Column == 1)
				TextOption.setAlignment(Qt::AlignLeft);
			else if (Column == 3)
				TextOption.setAlignment(Qt::AlignRight);
			else
				TextOption.setAlignment(Qt::AlignHCenter);

			swInstructionsCoverTextItem* Item = new swInstructionsCoverTextItem(PageItem, Instructions, Model, Author);
			Item->setFont(Font);
			Item->setDefaultTextColor(TextColor);
			Item->setPlainText(Text);
			Item->document()->setDefaultTextOption(TextOption);

			const qreal TextWidth = MarginsRect.width() - 2.0 * Item->GetBoxStyle().GetInset();

			if (Item->document()->idealWidth() > TextWidth)
				Item->setTextWidth(TextWidth);

			CoverTexts.push_back({ Item, Placement, Item->boundingRect().size() });
		};

		AddCoverText(Title, TitleFont, false, swInstructionsPropertyType::CoverTitlePlacement);

		const QString Author = Properties.mAuthor.trimmed();

		if (!Author.isEmpty() && Instructions->GetBoolProperty(swInstructionsPropertyType::ShowCoverAuthor, Model, 0))
		{
			QFont AuthorFont = TitleFont;
			AuthorFont.setPointSizeF(TitleFont.pointSizeF() / 2.0);
			AuthorFont.setBold(false);

			AddCoverText(Author, AuthorFont, true, swInstructionsPropertyType::CoverAuthorPlacement);
		}

		// stack texts sharing a row
		qreal TopHeight = 0.0;
		qreal BottomHeight = 0.0;

		for (int Row = 1; Row < SW_PLACEMENT_GRID_SIZE - 1; Row++)
		{
			qreal RowHeight = 0.0;

			for (const swCoverText& CoverText : CoverTexts)
				if (CoverText.Placement / SW_PLACEMENT_GRID_SIZE == Row)
					RowHeight += CoverText.Size.height();

			const qreal RowTop = swGetPlacementPosition(MarginsRect, QSizeF(MarginsRect.width(), RowHeight), Row * SW_PLACEMENT_GRID_SIZE + 2).y();
			qreal TextTop = RowTop;

			for (const swCoverText& CoverText : CoverTexts)
			{
				if (CoverText.Placement / SW_PLACEMENT_GRID_SIZE != Row)
					continue;

				const QPointF Position = swGetPlacementPosition(MarginsRect, CoverText.Size, CoverText.Placement);
				swPlaceElement(CoverText.Item, Instructions, QPointF(Position.x(), TextTop));
				TextTop += CoverText.Size.height();
			}

			if (Row == 1)
				TopHeight = RowHeight;
			else if (Row == SW_PLACEMENT_GRID_SIZE - 2)
				BottomHeight = RowHeight;
		}

		// place the finished model on the cover
		const QRectF ImageRect = MarginsRect.adjusted(0.0, TopHeight, 0.0, -BottomHeight);
		const swStep LastStep = Model->GetLastStep();

		const qreal ImageInset = Instructions->GetBoxStyle(swInstructionsBoxElement::CoverImage, Model, LastStep).GetInset();

		if (ImageRect.height() > 2.0 * ImageInset + 1.0)
		{
			Scene->addRect(ImageRect, QPen(Qt::NoPen), QBrush(Instructions->GetColorProperty(swInstructionsPropertyType::StepBackgroundColor, Model, LastStep)))->setParentItem(PageItem);

			swInstructionsStepImageItem* ModelImageItem = new swInstructionsStepImageItem(PageItem, Instructions, Model, LastStep);
			ModelImageItem->SetCoverImage();
			ModelImageItem->SetImageSize(qMax(static_cast<int>(ImageRect.width() - 2.0 * ImageInset), 1), static_cast<int>(ImageRect.height() - 2.0 * ImageInset));
			ModelImageItem->Update();

			const int Placement = swGetValidPlacement(Instructions->GetIntProperty(swInstructionsPropertyType::CoverImagePlacement, Model, LastStep), true);
			swPlaceElement(ModelImageItem, Instructions, swGetPlacementPosition(ImageRect, ModelImageItem->boundingRect().size(), Placement));
		}

		return;
	}

	if (Page->Type == swInstructionsPageType::BillOfMaterials)
	{
		swInstructionsPartsListItem* PartsImageItem = new swInstructionsPartsListItem(PageItem, Instructions, Page->Model, 0);
		const qreal Inset = 2.0 * PartsImageItem->GetBoxStyle().GetInset();
		PartsImageItem->SetBillOfMaterials(qMax(static_cast<int>(MarginsRect.width() - Inset), 16), qMax(static_cast<int>(MarginsRect.height() - Inset), 16));
		PartsImageItem->Update();

		const int Placement = swGetValidPlacement(Instructions->GetIntProperty(swInstructionsPropertyType::BillOfMaterialsPlacement, Page->Model, 0), true);
		swPlaceElement(PartsImageItem, Instructions, swGetPlacementPosition(MarginsRect, PartsImageItem->boundingRect().size(), Placement));

		return;
	}

	// draw dividers between steps
	qreal DividerInset = 0.0;

	if (!Page->Steps.empty())
	{
		const swInstructionsStep& FirstStep = Page->Steps.front();

		if (Instructions->GetBoolProperty(swInstructionsPropertyType::ShowStepDividers, FirstStep.Model, FirstStep.Step))
		{
			const int DividerWidth = Instructions->GetIntProperty(swInstructionsPropertyType::StepDividerWidth, FirstStep.Model, FirstStep.Step);
			const QPen DividerPen(Instructions->GetColorProperty(swInstructionsPropertyType::StepDividerColor, FirstStep.Model, FirstStep.Step), DividerWidth);
			const qreal Inset = 10.0;

			DividerInset = DividerWidth / 2.0;

			auto IsNear = [](qreal Value1, qreal Value2)
			{
				return qAbs(Value1 - Value2) < 0.0001;
			};

			for (const swInstructionsStep& Step : Page->Steps)
			{
				const QRectF& Rect = Step.Rect;

				for (const swInstructionsStep& Other : Page->Steps)
				{
					const QRectF& OtherRect = Other.Rect;

					if (IsNear(OtherRect.left(), Rect.right()) && IsNear(OtherRect.top(), Rect.top()))
					{
						const qreal X = MarginsRect.left() + MarginsRect.width() * Rect.right();
						Scene->addLine(X, MarginsRect.top() + MarginsRect.height() * Rect.top() + Inset, X, MarginsRect.top() + MarginsRect.height() * Rect.bottom() - Inset, DividerPen)->setParentItem(PageItem);
					}

					if (IsNear(OtherRect.top(), Rect.bottom()) && IsNear(OtherRect.left(), Rect.left()))
					{
						const qreal Y = MarginsRect.top() + MarginsRect.height() * Rect.bottom();
						Scene->addLine(MarginsRect.left() + MarginsRect.width() * Rect.left() + Inset, Y, MarginsRect.left() + MarginsRect.width() * Rect.right() - Inset, Y, DividerPen)->setParentItem(PageItem);
					}
				}
			}
		}
	}

	for (const swInstructionsStep& Step : Page->Steps)
	{
		const QRectF CellRect(MarginsRect.left() + MarginsRect.width() * Step.Rect.x(), MarginsRect.top() + MarginsRect.height() * Step.Rect.y(), MarginsRect.width() * Step.Rect.width(), MarginsRect.height() * Step.Rect.height());

		// the step area holds its elements
		QGraphicsRectItem* StepAreaItem = new QGraphicsRectItem(QRectF(QPointF(0.0, 0.0), CellRect.size()), PageItem);
		StepAreaItem->setPen(QPen(Qt::NoPen));
		StepAreaItem->setBrush(QBrush(Instructions->GetColorProperty(swInstructionsPropertyType::StepBackgroundColor, Step.Model, Step.Step)));
		StepAreaItem->setPos(CellRect.topLeft());

		swInstructionsStepImageItem* StepImageItem = new swInstructionsStepImageItem(StepAreaItem, Instructions, Step.Model, Step.Step);
		const qreal ImageInset = StepImageItem->GetBoxStyle().GetInset();
		StepImageItem->SetImageSize(qMax(static_cast<int>(CellRect.width() - 2.0 * ImageInset), 1), qMax(static_cast<int>(CellRect.height() - 2.0 * ImageInset), 1));
		StepImageItem->Update();

		swInstructionsStepNumberItem* StepNumberItem = new swInstructionsStepNumberItem(StepAreaItem, Instructions, Step.Model, Step.Step);
		StepNumberItem->Update();

		// place step elements in the split area
		QRectF AreaRect(QPointF(0.0, 0.0), CellRect.size());
		const qreal InsetLeft = Step.Rect.left() > 0.0001 ? DividerInset : 0.0;
		const qreal InsetTop = Step.Rect.top() > 0.0001 ? DividerInset : 0.0;
		const qreal InsetRight = Step.Rect.right() < 0.9999 ? DividerInset : 0.0;
		const qreal InsetBottom = Step.Rect.bottom() < 0.9999 ? DividerInset : 0.0;
		AreaRect.adjust(InsetLeft, InsetTop, -InsetRight, -InsetBottom);

		// wrap the parts list at its area
		swInstructionsPartsListItem* PartsImageItem = new swInstructionsPartsListItem(StepAreaItem, Instructions, Step.Model, Step.Step);
		const qreal Inset = 2.0 * PartsImageItem->GetBoxStyle().GetInset();
		PartsImageItem->SetMaxSize(qMax(static_cast<int>(AreaRect.width() - Inset), 16), qMax(static_cast<int>(AreaRect.height() - Inset), 16));
		PartsImageItem->Update();

		swInstructionsPlacementAnchor NumberAnchor = static_cast<swInstructionsPlacementAnchor>(Instructions->GetIntProperty(swInstructionsPropertyType::StepNumberAnchor, Step.Model, Step.Step));
		swInstructionsPlacementAnchor PartsAnchor = static_cast<swInstructionsPlacementAnchor>(Instructions->GetIntProperty(swInstructionsPropertyType::PLIAnchor, Step.Model, Step.Step));

		// fall back from hidden anchors
		if (NumberAnchor != swInstructionsPlacementAnchor::PartsList || !PartsImageItem->isVisible() || PartsAnchor == swInstructionsPlacementAnchor::StepNumber)
			NumberAnchor = swInstructionsPlacementAnchor::Area;

		if (PartsAnchor != swInstructionsPlacementAnchor::StepNumber || !StepNumberItem->isVisible())
			PartsAnchor = swInstructionsPlacementAnchor::Area;

		const int NumberPlacement = swGetValidPlacement(Instructions->GetIntProperty(swInstructionsPropertyType::StepNumberPlacement, Step.Model, Step.Step), NumberAnchor == swInstructionsPlacementAnchor::Area);
		const int PartsPlacement = swGetValidPlacement(Instructions->GetIntProperty(swInstructionsPropertyType::PLIPlacement, Step.Model, Step.Step), PartsAnchor == swInstructionsPlacementAnchor::Area);
		const QSizeF NumberSize = StepNumberItem->boundingRect().size();
		const QSizeF PartsSize = PartsImageItem->boundingRect().size();

		const int ImagePlacement = swGetValidPlacement(Instructions->GetIntProperty(swInstructionsPropertyType::StepImagePlacement, Step.Model, Step.Step), true);
		swPlaceElement(StepImageItem, Instructions, swGetPlacementPosition(AreaRect, StepImageItem->boundingRect().size(), ImagePlacement));

		// place moved and locked elements first
		if (NumberAnchor == swInstructionsPlacementAnchor::Area)
			swPlaceElement(StepNumberItem, Instructions, swGetPlacementPosition(AreaRect, NumberSize, NumberPlacement));

		if (PartsAnchor == swInstructionsPlacementAnchor::Area)
			swPlaceElement(PartsImageItem, Instructions, swGetPlacementPosition(AreaRect, PartsSize, PartsPlacement));

		if (NumberAnchor == swInstructionsPlacementAnchor::PartsList)
			swPlaceElement(StepNumberItem, Instructions, swGetPlacementPosition(QRectF(PartsImageItem->pos(), PartsSize), NumberSize, NumberPlacement));

		if (PartsAnchor == swInstructionsPlacementAnchor::StepNumber)
			swPlaceElement(PartsImageItem, Instructions, swGetPlacementPosition(QRectF(StepNumberItem->pos(), NumberSize), PartsSize, PartsPlacement));
	}
}

// use to add notices and pointers
static void swAddPageAnnotations(swInstructions* Instructions, size_t PageIndex, QGraphicsScene* Scene, QGraphicsRectItem* PageItem, const QRectF& MarginsRect)
{
	const swInstructionsPageKey PageKey = Instructions->GetPageElementsKey(PageIndex);

	for (const swInstructionsNotice& Notice : Instructions->GetNotices())
	{
		if (Notice.PageKey != PageKey)
			continue;

		swInstructionsNoticeItem* NoticeItem = new swInstructionsNoticeItem(PageItem, Instructions, Notice);
		const qreal TextWidth = MarginsRect.width() / 2.0;

		// wrap long notices at half width
		if (NoticeItem->document()->idealWidth() > TextWidth)
			NoticeItem->setTextWidth(TextWidth);

		swPlaceElement(NoticeItem, Instructions, Notice.Position);
	}

	std::map<swInstructionsElementKey, QGraphicsItem*> ElementItems;
	const QList<QGraphicsItem*> Items = Scene->items();

	for (QGraphicsItem* Item : Items)
	{
		swInstructionsElementKey Key;

		if (swGetElementKey(Item, Key))
			ElementItems[Key] = Item;
	}

	auto FindItem = [&ElementItems](const swInstructionsElementKey& Key) -> QGraphicsItem*
	{
		const auto ItemIt = ElementItems.find(Key);

		return ItemIt != ElementItems.end() && ItemIt->second->isVisible() ? ItemIt->second : nullptr;
	};

	// skip pointers of hidden elements
	for (const swInstructionsPointer& Pointer : Instructions->GetPointers())
	{
		if (Pointer.PageKey != PageKey)
			continue;

		QGraphicsItem* SourceItem = FindItem(Pointer.Source);

		if (SourceItem)
			new swInstructionsPointerItem(PageItem, Instructions, Pointer, SourceItem, FindItem(Pointer.TargetElement));
	}
}

void swUpdateInstructionsPageScene(swInstructions* Instructions, const swInstructionsPage* Page, QGraphicsScene* Scene)
{
	Scene->clear();

	if (!Page)
		return;

	const swInstructionsPageSetup& PageSetup = Instructions->mPageSetup;
	Scene->setSceneRect(0, 0, PageSetup.Width, PageSetup.Height);

	// fill the whole page background
	const size_t PageIndex = static_cast<size_t>(Page - Instructions->mPages.data());
	const QColor BackgroundColor = Instructions->GetPageBackgroundColor(PageIndex);
	QGraphicsRectItem* PageItem = Scene->addRect(QRectF(0.0f, 0.0f, PageSetup.Width, PageSetup.Height), QPen(Qt::NoPen), QBrush(BackgroundColor));
	PageItem->setFlag(QGraphicsItem::ItemClipsChildrenToShape, true);

	QRectF MarginsRect(PageSetup.MarginLeft, PageSetup.MarginTop, PageSetup.Width - PageSetup.MarginLeft - PageSetup.MarginRight, PageSetup.Height - PageSetup.MarginTop - PageSetup.MarginBottom);

	// number step and materials pages
	const bool PageHasContent = (Page->Type == swInstructionsPageType::Steps && !Page->Steps.empty()) || Page->Type == swInstructionsPageType::BillOfMaterials || Instructions->GetBoolProperty(swInstructionsPropertyType::ShowPageNumberAllPages, nullptr, 0);

	if (PageHasContent && Instructions->GetBoolProperty(swInstructionsPropertyType::ShowPageNumber, nullptr, 0))
	{
		swInstructionsPageNumberItem* PageNumberItem = new swInstructionsPageNumberItem(PageItem, Instructions, PageIndex);

		const int Placement = swGetValidPlacement(Instructions->GetIntProperty(swInstructionsPropertyType::PageNumberPlacement, nullptr, 0), true);
		swPlaceElement(PageNumberItem, Instructions, swGetPlacementPosition(MarginsRect, PageNumberItem->boundingRect().size(), Placement));
	}

	if (Page->Type != swInstructionsPageType::Blank)
		swAddPageContent(Instructions, Page, Scene, PageItem, MarginsRect);

	swAddPageAnnotations(Instructions, PageIndex, Scene, PageItem, MarginsRect);
}

void swInstructionsPageWidget::SetCurrentPage(const swInstructionsPage* Page)
{
	QGraphicsScene* Scene = scene();

	// keep the selection after a rebuild
	std::set<swInstructionsElementKey> SelectedKeys;
	const QList<QGraphicsItem*> SelectedItems = Scene->selectedItems();

	for (const QGraphicsItem* Item : SelectedItems)
	{
		swInstructionsElementKey Key;

		if (swGetElementKey(Item, Key))
			SelectedKeys.insert(Key);
	}

	mDragItems.clear();
	mPage = Page;
	mRebuildingPage = true;
	swUpdateInstructionsPageScene(mInstructions, Page, Scene);
	FitPage();

	// select a new pointer alone
	if (mPendingSelection)
	{
		SelectedKeys = { *mPendingSelection };
		mPendingSelection.reset();
	}

	const QList<QGraphicsItem*> Items = Scene->items();

	for (QGraphicsItem* Item : Items)
	{
		swInstructionsElementKey Key;

		if (swGetElementKey(Item, Key) && SelectedKeys.find(Key) != SelectedKeys.end())
			Item->setSelected(true);
	}

	mRebuildingPage = false;
	SelectionChanged();
}

void swInstructionsPageWidget::mousePressEvent(QMouseEvent* Event)
{
	QGraphicsView::mousePressEvent(Event);

	// the selection moves with a drag
	mDragItems.clear();

	if (Event->button() != Qt::LeftButton)
		return;

	const QList<QGraphicsItem*> SelectedItems = scene()->selectedItems();

	for (QGraphicsItem* Item : SelectedItems)
		if (Item->flags() & QGraphicsItem::ItemIsMovable)
			mDragItems.emplace_back(Item, Item->pos());
}

void swInstructionsPageWidget::mouseMoveEvent(QMouseEvent* Event)
{
	QGraphicsView::mouseMoveEvent(Event);

	if (mDragItems.empty())
		return;

	// pointers follow dragged elements
	const QList<QGraphicsItem*> Items = scene()->items();

	for (QGraphicsItem* Item : Items)
		if (swInstructionsPointerItem* PointerItem = dynamic_cast<swInstructionsPointerItem*>(Item))
			PointerItem->UpdateGeometry();
}

void swInstructionsPageWidget::contextMenuEvent(QContextMenuEvent* Event)
{
	if (!mPage)
		return;

	const size_t PageIndex = static_cast<size_t>(mPage - mInstructions->mPages.data());
	const QPointF ScenePosition = mapToScene(Event->pos());
	QGraphicsItem* ElementItem = nullptr;
	swInstructionsElementKey Key = { swInstructionsPlacementElement::None, nullptr, 0 };

	// act on the topmost element
	const QList<QGraphicsItem*> Items = scene()->items(ScenePosition);

	for (QGraphicsItem* Item : Items)
	{
		if (swGetElementKey(Item, Key))
		{
			ElementItem = Item;
			break;
		}
	}

	if (ElementItem && !ElementItem->isSelected())
	{
		scene()->clearSelection();
		ElementItem->setSelected(true);
	}

	QMenu Menu(this);
	const swInstructionsPlacementElement Element = std::get<0>(Key);
	const int Id = static_cast<int>(std::get<2>(Key));

	if (Element == swInstructionsPlacementElement::Pointer)
	{
		const swInstructionsPointer* Pointer = mInstructions->FindPointer(Id);

		if (!Pointer)
			return;

		QMenu* AnchorMenu = Menu.addMenu(tr("Anchor"));
		const QStringList AnchorNames = { tr("Top Left"), tr("Top"), tr("Top Right"), tr("Right"), tr("Bottom Right"), tr("Bottom"), tr("Bottom Left"), tr("Left") };
		QActionGroup* AnchorGroup = new QActionGroup(AnchorMenu);

		for (int Anchor = SW_POINTER_ANCHOR_AUTO; Anchor < SW_POINTER_ANCHOR_COUNT; Anchor++)
		{
			QAction* Action = AnchorMenu->addAction(Anchor < 0 ? tr("Automatic") : AnchorNames[Anchor]);
			Action->setCheckable(true);
			Action->setChecked(Pointer->Anchor == Anchor);
			AnchorGroup->addAction(Action);

			if (Anchor < 0)
				AnchorMenu->addSeparator();

			connect(Action, &QAction::triggered, this, [this, Id, Anchor]()
			{
				mInstructions->SetPointerAnchor(Id, Anchor);
			});
		}

		const int PointerStyle = Pointer->Style;
		const int PointerTip = Pointer->Tip;

		auto AddChoiceMenu = [this, &Menu](const QString& Title, const QStringList& Names, int Current, std::function<void(int)> SetChoice)
		{
			QMenu* ChoiceMenu = Menu.addMenu(Title);
			QActionGroup* ChoiceGroup = new QActionGroup(ChoiceMenu);

			for (int Choice = -1; Choice < Names.size(); Choice++)
			{
				QAction* Action = ChoiceMenu->addAction(Choice < 0 ? tr("Default") : Names[Choice]);
				Action->setCheckable(true);
				Action->setChecked(Current == Choice);
				ChoiceGroup->addAction(Action);

				if (Choice < 0)
					ChoiceMenu->addSeparator();

				connect(Action, &QAction::triggered, this, [SetChoice, Choice]()
				{
					SetChoice(Choice);
				});
			}
		};

		AddChoiceMenu(tr("Style"), { tr("Line"), tr("Callout Wedge") }, PointerStyle, [this, Id, PointerTip](int Style)
		{
			mInstructions->SetPointerStyle(Id, Style, PointerTip);
		});

		AddChoiceMenu(tr("Tip"), { tr("Arrowhead"), tr("Dot") }, PointerTip, [this, Id, PointerStyle](int Tip)
		{
			mInstructions->SetPointerStyle(Id, PointerStyle, Tip);
		});

		Menu.addSeparator();

		connect(Menu.addAction(tr("Remove Pointer")), &QAction::triggered, this, [this, Id]()
		{
			mInstructions->RemovePointer(Id);
		});
	}
	else
	{
		connect(Menu.addAction(tr("Add Notice...")), &QAction::triggered, this, [this, PageIndex, ScenePosition]()
		{
			QString Text;

			if (swEditNoticeText(this, Text, true))
				mInstructions->AddNotice(PageIndex, ScenePosition, Text);
		});

		if (ElementItem && dynamic_cast<swInstructionsBoxBase*>(ElementItem))
		{
			// new pointers leave sideways
			const QRectF BorderRect = ElementItem->mapRectToScene(dynamic_cast<swInstructionsBoxBase*>(ElementItem)->GetBorderRect());
			QPointF TipPosition(BorderRect.right() + 72.0, BorderRect.center().y());

			if (!sceneRect().contains(TipPosition))
				TipPosition.setX(BorderRect.left() - 72.0);

			connect(Menu.addAction(tr("Add Pointer")), &QAction::triggered, this, [this, PageIndex, Key, TipPosition]()
			{
				const int PointerId = mInstructions->AddPointer(PageIndex, Key, TipPosition);

				SelectElement({ swInstructionsPlacementElement::Pointer, nullptr, static_cast<swStep>(PointerId) });
			});
		}

		if (Element == swInstructionsPlacementElement::Notice)
		{
			Menu.addSeparator();

			connect(Menu.addAction(tr("Edit Notice...")), &QAction::triggered, this, [this, Id]()
			{
				const swInstructionsNotice* Notice = mInstructions->FindNotice(Id);
				QString Text = Notice ? Notice->Text : QString();

				if (Notice && swEditNoticeText(this, Text, false))
					mInstructions->SetNoticeText(Id, Text);
			});

			connect(Menu.addAction(tr("Remove Notice")), &QAction::triggered, this, [this, Id]()
			{
				mInstructions->RemoveNotice(Id);
			});
		}
	}

	Menu.exec(Event->globalPos());
}

void swInstructionsPageWidget::SelectElement(const swInstructionsElementKey& Key)
{
	const QList<QGraphicsItem*> Items = scene()->items();

	for (QGraphicsItem* Item : Items)
	{
		swInstructionsElementKey ItemKey;

		if (swGetElementKey(Item, ItemKey) && ItemKey == Key)
		{
			scene()->clearSelection();
			Item->setSelected(true);
			return;
		}
	}

	// select the element once rebuilt
	mPendingSelection = Key;
}

void swInstructionsPageWidget::mouseReleaseEvent(QMouseEvent* Event)
{
	QGraphicsView::mouseReleaseEvent(Event);

	std::map<swInstructionsElementKey, QPointF> Offsets;

	for (const std::pair<QGraphicsItem*, QPointF>& DragItem : mDragItems)
	{
		const QPointF Distance = DragItem.first->pos() - DragItem.second;
		swInstructionsElementKey Key;

		if (Distance.isNull() || !swGetElementKey(DragItem.first, Key))
			continue;

		Offsets[Key] = mInstructions->GetElementOffset(Key) + Distance;
	}

	mDragItems.clear();

	if (Offsets.empty())
		return;

	// rebuild after the release
	QTimer::singleShot(0, this, [this, Offsets]()
	{
		mInstructions->SetElementOffsets(Offsets);
	});
}

void swInstructionsPageWidget::drawForeground(QPainter* Painter, const QRectF& Rect)
{
	QGraphicsView::drawForeground(Painter, Rect);

	if (!mPage)
		return;

	// dash the margins on screen
	if (mInstructions->IsMarginsShown())
	{
		const swInstructionsPageSetup& PageSetup = mInstructions->mPageSetup;
		const QRectF MarginsRect(PageSetup.MarginLeft, PageSetup.MarginTop, PageSetup.Width - PageSetup.MarginLeft - PageSetup.MarginRight, PageSetup.Height - PageSetup.MarginTop - PageSetup.MarginBottom);

		QPen MarginPen(mInstructions->GetMarginColor(), 0.0, Qt::DashLine);
		MarginPen.setCosmetic(true);

		Painter->save();
		Painter->setPen(MarginPen);
		Painter->setBrush(Qt::NoBrush);
		Painter->drawRect(MarginsRect);
		Painter->restore();
	}

	if (!mInstructions->IsGridShown())
		return;

	// draw the grid on screen
	const QRectF GridRect = mInstructions->GetGridRect();
	const QRectF PageRect = GridRect.intersected(Rect);
	const int Divisions = mInstructions->GetGridDivisions();
	const qreal SnapSize = mInstructions->GetGridSnapSize();

	if (PageRect.isEmpty())
		return;

	// count lines from the grid corner
	QVector<QLineF> PrimaryLines;
	QVector<QLineF> SecondaryLines;

	const int FirstColumn = static_cast<int>(std::ceil((PageRect.left() - GridRect.left()) / SnapSize - 0.001));
	const int LastColumn = static_cast<int>(std::floor((PageRect.right() - GridRect.left()) / SnapSize + 0.001));
	const int FirstRow = static_cast<int>(std::ceil((PageRect.top() - GridRect.top()) / SnapSize - 0.001));
	const int LastRow = static_cast<int>(std::floor((PageRect.bottom() - GridRect.top()) / SnapSize + 0.001));

	for (int Column = FirstColumn; Column <= LastColumn; Column++)
	{
		const qreal X = GridRect.left() + Column * SnapSize;
		(Column % Divisions ? SecondaryLines : PrimaryLines).append(QLineF(X, PageRect.top(), X, PageRect.bottom()));
	}

	for (int Row = FirstRow; Row <= LastRow; Row++)
	{
		const qreal Y = GridRect.top() + Row * SnapSize;
		(Row % Divisions ? SecondaryLines : PrimaryLines).append(QLineF(PageRect.left(), Y, PageRect.right(), Y));
	}

	QPen Pen(mInstructions->GetGridSecondaryColor(), 0.0);
	Pen.setCosmetic(true);

	Painter->save();
	Painter->setClipRect(PageRect);
	Painter->setPen(Pen);
	Painter->drawLines(SecondaryLines);
	Pen.setColor(mInstructions->GetGridPrimaryColor());
	Painter->setPen(Pen);
	Painter->drawLines(PrimaryLines);
	Painter->restore();
}

void swInstructionsPageWidget::resizeEvent(QResizeEvent* Event)
{
	QGraphicsView::resizeEvent(Event);
	FitPage();
}

void swInstructionsPageWidget::FitPage()
{
	// show the whole page
	if (mPage)
		fitInView(sceneRect().adjusted(-10.0, -10.0, 10.0, 10.0), Qt::KeepAspectRatio);
}

swInstructionsPageListWidget::swInstructionsPageListWidget(QWidget* Parent, swInstructions* Instructions)
	: QDockWidget(Parent), mInstructions(Instructions)
{
	setWindowTitle(tr("Pages"));
	setWhatsThis(tr("<p><b>Pages</b></p><p>Select a page to show it. Right-click a page to insert or remove blank pages around it.</p>"));
	swSetupWidgetHelp(this, nullptr);

	QWidget* CentralWidget = new QWidget(this);
	setWidget(CentralWidget);

	QVBoxLayout* Layout = new QVBoxLayout(CentralWidget);

	mThumbnailsWidget = new QListWidget(CentralWidget);
	mThumbnailsWidget->setAccessibleName(tr("Pages"));
	mThumbnailsWidget->setContextMenuPolicy(Qt::CustomContextMenu);
	Layout->addWidget(mThumbnailsWidget);

	connect(mThumbnailsWidget, &QWidget::customContextMenuRequested, this, &swInstructionsPageListWidget::CustomMenuRequested);
}

void swInstructionsPageListWidget::CustomMenuRequested(const QPoint& Position)
{
	// right click makes the page current
	QListWidgetItem* ClickedItem = mThumbnailsWidget->itemAt(Position);

	if (ClickedItem)
		mThumbnailsWidget->setCurrentItem(ClickedItem);

	const size_t PageIndex = qMax(mThumbnailsWidget->currentRow(), 0);
	const bool IsBlank = PageIndex < mInstructions->mPages.size() && mInstructions->mPages[PageIndex].Type == swInstructionsPageType::Blank;

	QMenu Menu(this);

	QAction* InsertBeforeAction = Menu.addAction(tr("Insert Blank Page Before"));
	InsertBeforeAction->setEnabled(mInstructions->CanInsertBlankPage(PageIndex));

	connect(InsertBeforeAction, &QAction::triggered, this, [this, PageIndex]()
	{
		mInstructions->InsertBlankPage(PageIndex);
	});

	QAction* InsertAfterAction = Menu.addAction(tr("Insert Blank Page After"));
	InsertAfterAction->setEnabled(mInstructions->CanInsertBlankPage(PageIndex + 1));

	connect(InsertAfterAction, &QAction::triggered, this, [this, PageIndex]()
	{
		mInstructions->InsertBlankPage(PageIndex + 1);
	});

	QAction* RemoveAction = Menu.addAction(tr("Remove Blank Page"));
	RemoveAction->setEnabled(IsBlank);

	connect(RemoveAction, &QAction::triggered, this, [this, PageIndex]()
	{
		mInstructions->RemoveBlankPage(PageIndex);
	});

	Menu.exec(mThumbnailsWidget->viewport()->mapToGlobal(Position));
}
