#include "sw_global.h"
#include "sw_dialog_instructions_page.h"
#include "sw_dialog_instructions_placement.h"
#include "sw_dialog_instructions_properties.h"
#include "sw_project.h"
#include "sw_model.h"
#include "sw_view.h"
#include "sw_window_utilities.h"

// use to identify a page element across page rebuilds, which delete the items
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
	else
	{
		Key = { swInstructionsPlacementElement::None, nullptr, 0 };
		return false;
	}

	return true;
}

// use to snap a dragged element to the page grid, the grid starts at the corner of its area
static QVariant swSnapItemPosition(const QGraphicsItem* Item, const swInstructions* Instructions, const QVariant& Value)
{
	const QGraphicsScene* Scene = Item->scene();

	// only dragging snaps, building the page keeps the exact placements
	if (!Instructions->IsSnapToGrid() || !Scene || !Scene->mouseGrabberItem())
		return Value;

	const QGraphicsItem* Parent = Item->parentItem();
	const QPointF Position = Value.toPointF();
	const QPointF ScenePosition = Parent ? Parent->mapToScene(Position) : Position;
	const QPointF Origin = Instructions->GetGridRect().topLeft();
	const qreal GridSize = Instructions->GetGridSize();

	const QPointF Snapped = Origin + QPointF(qRound((ScenePosition.x() - Origin.x()) / GridSize) * GridSize, qRound((ScenePosition.y() - Origin.y()) / GridSize) * GridSize);

	return Parent ? Parent->mapFromScene(Snapped) : Snapped;
}


// the movable elements are dragged on the page and snap to its grid
constexpr QGraphicsItem::GraphicsItemFlags SW_MOVABLE_ITEM_FLAGS = QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsFocusable | QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemSendsGeometryChanges;

// use to put an element on its placement, a moved element keeps its offset and a locked one its position
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

// use to crop a rendered image to its visible pixels so it can be placed like any other element
static QImage swCropImage(const QImage& Image)
{
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

	return Source.copy(Left, Top, Right - Left + 1, Bottom - Top + 1);
}

// use to position an item on a placement grid cell around the anchor
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
	: swInstructionsPaddedItem(Parent), mInstructions(Instructions), mModel(Model), mStep(Step)
{
	setFlags(SW_MOVABLE_ITEM_FLAGS);
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
	// the step background lies behind the image, the model is rendered alone and cropped to be placed
	const QColor BackgroundColor = mInstructions->GetColorProperty(swInstructionsPropertyType::StepBackgroundColor, mModel, mStep);
	View.SetBackgroundColorOverride(SW_RGBA(BackgroundColor.red(), BackgroundColor.green(), BackgroundColor.blue(), 0));
	View.SetSize(mWidth, mHeight);

	// Work on a copy of the main view camera, fitting the image must not move a model camera.
	View.SetCamera(View.GetCamera(), true);

	// show each step from its rotation step view, like the model view does while stepping
	View.ApplyStepView(mStep);

	// Frame the complete model whatever the main view zoom, so no step or cover image is clipped.
	View.ZoomExtentsAtStep(mModel->GetLastStep(), swGeometryBoundsMode::IncludeFallback, false);

	std::vector<QImage> Images = View.GetStepImages(mStep, mStep);

	if (!Images.empty())
		setPixmap(QPixmap::fromImage(swCropImage(Images.front())));
	else
		setPixmap(QPixmap());
}

swInstructionsStepNumberItem::swInstructionsStepNumberItem(QGraphicsItem* Parent, swInstructions* Instructions, swModel* Model, swStep Step)
	: swInstructionsPaddedItem(Parent), mInstructions(Instructions), mModel(Model), mStep(Step)
{
	setFlags(SW_MOVABLE_ITEM_FLAGS);
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
	: swInstructionsPaddedItem(Parent), mInstructions(Instructions)
{
	setFlags(SW_MOVABLE_ITEM_FLAGS);
	setZValue(1.0);

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
	: swInstructionsPaddedItem(Parent), mInstructions(Instructions), mModel(Model), mStep(Step)
{
	setFlags(SW_MOVABLE_ITEM_FLAGS);
}

QVariant swInstructionsPartsListItem::itemChange(GraphicsItemChange Change, const QVariant& Value)
{
	if (Change == QGraphicsItem::ItemPositionChange)
		return swSnapItemPosition(this, mInstructions, Value);

	return QGraphicsPixmapItem::itemChange(Change, Value);
}

void swInstructionsPartsListItem::Update()
{
	// the bill of materials uses the parts list settings but is shown whenever its page exists
	bool Visible = mBillOfMaterials || mInstructions->GetBoolProperty(swInstructionsPropertyType::ShowStepPLI, mModel, mStep);
	setVisible(Visible);

	if (!Visible)
		return;

	QColor BackgroundColor = mInstructions->GetColorProperty(swInstructionsPropertyType::PLIBackgroundColor, mModel, mStep);
	QFont Font = mInstructions->GetFontProperty(swInstructionsPropertyType::PLIFont, mModel, mStep);
	QColor TextColor = mInstructions->GetColorProperty(swInstructionsPropertyType::PLITextColor, mModel, mStep);
	const bool ShowCounts = mInstructions->GetBoolProperty(swInstructionsPropertyType::PLIShowCounts, mModel, mStep);
	const swPartsListSort Sort = static_cast<swPartsListSort>(mInstructions->GetIntProperty(swInstructionsPropertyType::PLISortOrder, mModel, mStep));
	const int ThumbnailSize = mInstructions->GetIntProperty(swInstructionsPropertyType::PLIThumbnailSize, mModel, mStep);
	const QColor ThumbnailBackgroundColor = mInstructions->GetColorProperty(swInstructionsPropertyType::PLIThumbnailBackgroundColor, mModel, mStep);

	const swResult<QImage> PartsImage = mModel->GetPartsListImage(mMaxWidth, ThumbnailSize, mBillOfMaterials ? 0 : mStep, swRGBAFromQColor(BackgroundColor), swRGBAFromQColor(ThumbnailBackgroundColor), Font, TextColor, false, ShowCounts, Sort);

	if (!PartsImage)
	{
		setPixmap(QPixmap());

		QWidget* Parent = scene() && !scene()->views().isEmpty() ? scene()->views().front() : nullptr;
		QMessageBox::warning(Parent, QObject::tr("Create Parts List"), PartsImage.error());

		return;
	}

	QImage Image = PartsImage.value();
	const int BorderWidth = mInstructions->GetIntProperty(swInstructionsPropertyType::PLIBorderWidth, mModel, mStep);

	if (BorderWidth > 0 && !Image.isNull())
	{
		QPainter Painter(&Image);
		QPen Pen(mInstructions->GetColorProperty(swInstructionsPropertyType::PLIBorderColor, mModel, mStep), BorderWidth);
		Pen.setJoinStyle(Qt::MiterJoin);
		Painter.setPen(Pen);
		Painter.drawRect(QRectF(BorderWidth / 2.0, BorderWidth / 2.0, Image.width() - BorderWidth, Image.height() - BorderWidth));
	}

	setPixmap(QPixmap::fromImage(Image));
}

swInstructionsCoverTextItem::swInstructionsCoverTextItem(QGraphicsItem* Parent, swInstructions* Instructions, swModel* Model, bool Author)
	: swInstructionsPaddedItem(Parent), mInstructions(Instructions), mModel(Model), mAuthor(Author)
{
	setFlags(SW_MOVABLE_ITEM_FLAGS);
	setZValue(1.0);
}

QVariant swInstructionsCoverTextItem::itemChange(GraphicsItemChange Change, const QVariant& Value)
{
	if (Change == QGraphicsItem::ItemPositionChange)
		return swSnapItemPosition(this, mInstructions, Value);

	return QGraphicsTextItem::itemChange(Change, Value);
}

swInstructionsPageWidget::swInstructionsPageWidget(QWidget* Parent, swInstructions* Instructions, swInstructionsPropertiesWidget* PropertiesWidget, swInstructionsPlacementWidget* PlacementWidget)
	: QGraphicsView(Parent), mInstructions(Instructions), mPropertiesWidget(PropertiesWidget), mPlacementWidget(PlacementWidget)
{
	QGraphicsScene* Scene = new QGraphicsScene();
	setScene(Scene);
	setBackgroundBrush(palette().color(QPalette::Mid));

	connect(mInstructions, &swInstructions::StepSettingsChanged, this, &swInstructionsPageWidget::StepSettingsChanged);
	connect(Scene, &QGraphicsScene::selectionChanged, this, &swInstructionsPageWidget::SelectionChanged);

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
	QGraphicsScene* Scene = qobject_cast<QGraphicsScene*>(sender());
	QGraphicsItem* Focus = nullptr;

	if (Scene)
	{
		QList<QGraphicsItem*> SelectedItems = Scene->selectedItems();

		if (!SelectedItems.isEmpty())
			Focus = SelectedItems.first();
	}

	mPropertiesWidget->SelectionChanged(Focus);
	mPlacementWidget->SelectionChanged(Focus);
}

void swUpdateInstructionsPageScene(swInstructions* Instructions, const swInstructionsPage* Page, QGraphicsScene* Scene)
{
	Scene->clear();

	if (!Page)
		return;

	const swInstructionsPageSetup& PageSetup = Instructions->mPageSetup;
	Scene->setSceneRect(0, 0, PageSetup.Width, PageSetup.Height);

	// the background fills the whole page, margins included, so prints have no white border
	const size_t PageIndex = static_cast<size_t>(Page - Instructions->mPages.data());
	const QColor BackgroundColor = Instructions->GetPageBackgroundColor(PageIndex);
	QGraphicsRectItem* PageItem = Scene->addRect(QRectF(0.0f, 0.0f, PageSetup.Width, PageSetup.Height), QPen(Qt::NoPen), QBrush(BackgroundColor));
	PageItem->setFlag(QGraphicsItem::ItemClipsChildrenToShape, true);

	QRectF MarginsRect(PageSetup.MarginLeft, PageSetup.MarginTop, PageSetup.Width - PageSetup.MarginLeft - PageSetup.MarginRight, PageSetup.Height - PageSetup.MarginTop - PageSetup.MarginBottom);

	// pages with steps and the bill of materials are numbered unless all pages are, placed halfway into the margins so it stays clear of the steps
	const bool PageHasContent = (Page->Type == swInstructionsPageType::Steps && !Page->Steps.empty()) || Page->Type == swInstructionsPageType::BillOfMaterials || Instructions->GetBoolProperty(swInstructionsPropertyType::ShowPageNumberAllPages, nullptr, 0);

	if (PageHasContent && Instructions->GetBoolProperty(swInstructionsPropertyType::ShowPageNumber, nullptr, 0))
	{
		const QRectF NumberRect(PageSetup.MarginLeft / 2.0, PageSetup.MarginTop / 2.0, PageSetup.Width - (PageSetup.MarginLeft + PageSetup.MarginRight) / 2.0, PageSetup.Height - (PageSetup.MarginTop + PageSetup.MarginBottom) / 2.0);
		swInstructionsPageNumberItem* PageNumberItem = new swInstructionsPageNumberItem(PageItem, Instructions, PageIndex);

		const int Placement = swGetValidPlacement(Instructions->GetIntProperty(swInstructionsPropertyType::PageNumberPlacement, nullptr, 0), true);
		swPlaceElement(PageNumberItem, Instructions, swGetPlacementPosition(NumberRect, PageNumberItem->boundingRect().size(), Placement));
	}

	if (Page->Type == swInstructionsPageType::Blank)
		return;

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

			// the placement column aligns the lines of a text wrapped at the page width
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

			const qreal TextWidth = MarginsRect.width() - 2.0 * SW_ELEMENT_PADDING;

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

		// texts sharing a placement row are stacked in their order, the title above the author
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

		// the finished model is placed in the rest of the cover, centered texts lie over it
		const QRectF ImageRect = MarginsRect.adjusted(0.0, TopHeight, 0.0, -BottomHeight);
		const swStep LastStep = Model->GetLastStep();

		if (ImageRect.height() > 2.0 * SW_ELEMENT_PADDING + 1.0)
		{
			Scene->addRect(ImageRect, QPen(Qt::NoPen), QBrush(Instructions->GetColorProperty(swInstructionsPropertyType::StepBackgroundColor, Model, LastStep)))->setParentItem(PageItem);

			swInstructionsStepImageItem* ModelImageItem = new swInstructionsStepImageItem(PageItem, Instructions, Model, LastStep);
			ModelImageItem->SetCoverImage();
			ModelImageItem->SetImageSize(qMax(static_cast<int>(ImageRect.width() - 2.0 * SW_ELEMENT_PADDING), 1), static_cast<int>(ImageRect.height() - 2.0 * SW_ELEMENT_PADDING));
			ModelImageItem->Update();

			const int Placement = swGetValidPlacement(Instructions->GetIntProperty(swInstructionsPropertyType::CoverImagePlacement, Model, LastStep), true);
			swPlaceElement(ModelImageItem, Instructions, swGetPlacementPosition(ImageRect, ModelImageItem->boundingRect().size(), Placement));
		}

		return;
	}

	if (Page->Type == swInstructionsPageType::BillOfMaterials)
	{
		swInstructionsPartsListItem* PartsImageItem = new swInstructionsPartsListItem(PageItem, Instructions, Page->Model, 0);
		PartsImageItem->SetBillOfMaterials(qMax(static_cast<int>(MarginsRect.width() - 2.0 * SW_ELEMENT_PADDING), 16));
		PartsImageItem->Update();

		const int Placement = swGetValidPlacement(Instructions->GetIntProperty(swInstructionsPropertyType::BillOfMaterialsPlacement, Page->Model, 0), true);
		swPlaceElement(PartsImageItem, Instructions, swGetPlacementPosition(MarginsRect, PartsImageItem->boundingRect().size(), Placement));

		return;
	}

	// draw a divider between neighboring steps, across rows and down columns
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

		// the step area holds its background and elements, they are placed and moved inside it
		QGraphicsRectItem* StepAreaItem = new QGraphicsRectItem(QRectF(QPointF(0.0, 0.0), CellRect.size()), PageItem);
		StepAreaItem->setPen(QPen(Qt::NoPen));
		StepAreaItem->setBrush(QBrush(Instructions->GetColorProperty(swInstructionsPropertyType::StepBackgroundColor, Step.Model, Step.Step)));
		StepAreaItem->setPos(CellRect.topLeft());

		swInstructionsStepImageItem* StepImageItem = new swInstructionsStepImageItem(StepAreaItem, Instructions, Step.Model, Step.Step);
		StepImageItem->SetImageSize(qMax(static_cast<int>(CellRect.width() - 2.0 * SW_ELEMENT_PADDING), 1), qMax(static_cast<int>(CellRect.height() - 2.0 * SW_ELEMENT_PADDING), 1));
		StepImageItem->Update();

		swInstructionsStepNumberItem* StepNumberItem = new swInstructionsStepNumberItem(StepAreaItem, Instructions, Step.Model, Step.Step);
		StepNumberItem->Update();

		// step elements are placed in the part of the page the split gives the step, inside its dividers
		QRectF AreaRect(QPointF(0.0, 0.0), CellRect.size());
		const qreal InsetLeft = Step.Rect.left() > 0.0001 ? DividerInset : 0.0;
		const qreal InsetTop = Step.Rect.top() > 0.0001 ? DividerInset : 0.0;
		const qreal InsetRight = Step.Rect.right() < 0.9999 ? DividerInset : 0.0;
		const qreal InsetBottom = Step.Rect.bottom() < 0.9999 ? DividerInset : 0.0;
		AreaRect.adjust(InsetLeft, InsetTop, -InsetRight, -InsetBottom);

		// the parts list wraps at the width of its area
		swInstructionsPartsListItem* PartsImageItem = new swInstructionsPartsListItem(StepAreaItem, Instructions, Step.Model, Step.Step);
		PartsImageItem->SetMaxWidth(qMax(static_cast<int>(AreaRect.width() - 2.0 * SW_ELEMENT_PADDING), 16));
		PartsImageItem->Update();

		swInstructionsPlacementAnchor NumberAnchor = static_cast<swInstructionsPlacementAnchor>(Instructions->GetIntProperty(swInstructionsPropertyType::StepNumberAnchor, Step.Model, Step.Step));
		swInstructionsPlacementAnchor PartsAnchor = static_cast<swInstructionsPlacementAnchor>(Instructions->GetIntProperty(swInstructionsPropertyType::PLIAnchor, Step.Model, Step.Step));

		// an element placed against a hidden element or against the element placed against it falls back to the area
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

		// a moved or locked element is placed first, an element placed against it follows it
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

void swInstructionsPageWidget::SetCurrentPage(const swInstructionsPage* Page)
{
	QGraphicsScene* Scene = scene();

	// a rebuilt page keeps its selected elements selected so a new placement stays editable
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
	swUpdateInstructionsPageScene(mInstructions, Page, Scene);
	FitPage();

	if (SelectedKeys.empty())
		return;

	const QList<QGraphicsItem*> Items = Scene->items();

	for (QGraphicsItem* Item : Items)
	{
		swInstructionsElementKey Key;

		if (swGetElementKey(Item, Key) && SelectedKeys.find(Key) != SelectedKeys.end())
			Item->setSelected(true);
	}
}

void swInstructionsPageWidget::mousePressEvent(QMouseEvent* Event)
{
	QGraphicsView::mousePressEvent(Event);

	// the selected elements are the ones a drag moves, their start keeps how far they move
	mDragItems.clear();

	if (Event->button() != Qt::LeftButton)
		return;

	const QList<QGraphicsItem*> SelectedItems = scene()->selectedItems();

	for (QGraphicsItem* Item : SelectedItems)
		if (Item->flags() & QGraphicsItem::ItemIsMovable)
			mDragItems.emplace_back(Item, Item->pos());
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

	// the page is rebuilt once the scene has finished with the release, the rebuild deletes the items
	QTimer::singleShot(0, this, [this, Offsets]()
	{
		mInstructions->SetElementOffsets(Offsets);
	});
}

void swInstructionsPageWidget::drawForeground(QPainter* Painter, const QRectF& Rect)
{
	QGraphicsView::drawForeground(Painter, Rect);

	if (!mPage || !mInstructions->IsGridShown())
		return;

	// the grid starts at the corner of its area and is only drawn on screen
	const QRectF GridRect = mInstructions->GetGridRect();
	const QRectF PageRect = GridRect.intersected(Rect);
	const qreal GridSize = mInstructions->GetGridSize();

	if (PageRect.isEmpty())
		return;

	QColor LineColor = palette().color(QPalette::Highlight);
	LineColor.setAlpha(70);

	QPen Pen(LineColor, 0.0);
	Pen.setCosmetic(true);

	Painter->save();
	Painter->setPen(Pen);
	Painter->setClipRect(PageRect);

	const qreal FirstX = GridRect.left() + std::ceil((PageRect.left() - GridRect.left()) / GridSize) * GridSize;
	const qreal FirstY = GridRect.top() + std::ceil((PageRect.top() - GridRect.top()) / GridSize) * GridSize;
	QVector<QLineF> Lines;

	for (qreal X = FirstX; X <= PageRect.right(); X += GridSize)
		Lines.append(QLineF(X, PageRect.top(), X, PageRect.bottom()));

	for (qreal Y = FirstY; Y <= PageRect.bottom(); Y += GridSize)
		Lines.append(QLineF(PageRect.left(), Y, PageRect.right(), Y));

	Painter->drawLines(Lines);
	Painter->restore();
}

void swInstructionsPageWidget::resizeEvent(QResizeEvent* Event)
{
	QGraphicsView::resizeEvent(Event);
	FitPage();
}

void swInstructionsPageWidget::FitPage()
{
	// show the whole page whatever its size, from small paper to large screen formats
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
	// right-clicking a page makes it current so the entries act on it
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
