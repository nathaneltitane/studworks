#include "sw_global.h"
#include "sw_instructions_dialog.h"
#include "sw_page_setup_dialog.h"
#include "sw_project.h"
#include "sw_model.h"
#include "sw_view.h"
#include "sw_collapsible_widget.h"
#include "sw_utilities.h"

// use to identify a page element across page rebuilds, which delete the items
static bool swGetElementKey(const QGraphicsItem* Item, swInstructionsElementKey& Key)
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
		const swInstructionsPlacementElement Element = PartsItem->IsBillOfMaterials() ? swInstructionsPlacementElement::BillOfMaterials : swInstructionsPlacementElement::PiecesList;
		Key = { Element, PartsItem->GetModel(), PartsItem->GetStep() };
	}
	else if (const swInstructionsCoverTextItem* TextItem = dynamic_cast<const swInstructionsCoverTextItem*>(Item))
	{
		const swInstructionsPlacementElement Element = TextItem->IsAuthor() ? swInstructionsPlacementElement::CoverAuthor : swInstructionsPlacementElement::CoverTitle;
		Key = { Element, TextItem->GetModel(), 0 };
	}
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

// the grid size arrows move to the next whole increment so converted sizes line up with the unit
class swInstructionsGridSizeSpinBox : public QDoubleSpinBox
{
public:
	using QDoubleSpinBox::QDoubleSpinBox;

	void stepBy(int Steps) override
	{
		const double Step = singleStep();
		const double Increments = value() / Step;
		const double Base = Steps > 0 ? std::floor(Increments + 1e-6) : std::ceil(Increments - 1e-6);

		setValue((Base + Steps) * Step);
	}
};

// the movable elements are dragged on the page and snap to its grid
constexpr QGraphicsItem::GraphicsItemFlags SW_MOVABLE_ITEM_FLAGS = QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsFocusable | QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemSendsGeometryChanges;

// the position an element gets from its placement, before it is moved or locked
constexpr int SW_PLACEMENT_POSITION_DATA = 0;

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
	// the bill of materials uses the pieces list settings but is shown whenever its page exists
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
		QMessageBox::warning(Parent, QObject::tr("Create Pieces List"), PartsImage.error());

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

static void swUpdateInstructionsPageScene(swInstructions* Instructions, const swInstructionsPage* Page, QGraphicsScene* Scene)
{
	Scene->clear();

	if (!Page)
		return;

	const swInstructionsPageSetup& PageSetup = Instructions->mPageSetup;
	Scene->setSceneRect(0, 0, PageSetup.Width, PageSetup.Height);

	// the background fills the whole page, margins included, so prints have no white border
	const QColor BackgroundColor = Instructions->GetColorProperty(swInstructionsPropertyType::PageBackgroundColor, nullptr, 0);
	QGraphicsRectItem* PageItem = Scene->addRect(QRectF(0.0f, 0.0f, PageSetup.Width, PageSetup.Height), QPen(Qt::NoPen), QBrush(BackgroundColor));
	PageItem->setFlag(QGraphicsItem::ItemClipsChildrenToShape, true);

	QRectF MarginsRect(PageSetup.MarginLeft, PageSetup.MarginTop, PageSetup.Width - PageSetup.MarginLeft - PageSetup.MarginRight, PageSetup.Height - PageSetup.MarginTop - PageSetup.MarginBottom);

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

		// the pieces list wraps at the width of its area
		swInstructionsPartsListItem* PartsImageItem = new swInstructionsPartsListItem(StepAreaItem, Instructions, Step.Model, Step.Step);
		PartsImageItem->SetMaxWidth(qMax(static_cast<int>(AreaRect.width() - 2.0 * SW_ELEMENT_PADDING), 16));
		PartsImageItem->Update();

		swInstructionsPlacementAnchor NumberAnchor = static_cast<swInstructionsPlacementAnchor>(Instructions->GetIntProperty(swInstructionsPropertyType::StepNumberAnchor, Step.Model, Step.Step));
		swInstructionsPlacementAnchor PartsAnchor = static_cast<swInstructionsPlacementAnchor>(Instructions->GetIntProperty(swInstructionsPropertyType::PLIAnchor, Step.Model, Step.Step));

		// an element placed against a hidden element or against the element placed against it falls back to the area
		if (NumberAnchor != swInstructionsPlacementAnchor::PiecesList || !PartsImageItem->isVisible() || PartsAnchor == swInstructionsPlacementAnchor::StepNumber)
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

		if (NumberAnchor == swInstructionsPlacementAnchor::PiecesList)
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

swInstructionsPropertiesWidget::swInstructionsPropertiesWidget(QWidget* Parent, swInstructions* Instructions)
	: QDockWidget(Parent), mInstructions(Instructions)
{
	setWindowTitle(tr("Properties"));
	setWhatsThis(tr("<p><b>Properties</b></p><p>Edit how the instruction pages look. With nothing selected on the page every section is shown, select a step, its number or its pieces list to show only their settings. Changes apply to all pages.</p>"));
	swSetupWidgetHelp(this, nullptr);

	QWidget* CentralWidget = new QWidget(this);
	setWidget(CentralWidget);

	QVBoxLayout* Layout = new QVBoxLayout(CentralWidget);

	QWidget* OptionsRow = swCreatePanelRow(CentralWidget);
	swAddPanelRowStretch(OptionsRow);
	Layout->addWidget(OptionsRow);

	QMenu* OptionsMenu = new QMenu(this);
	swAddPanelOptionsButton(OptionsRow, OptionsMenu, tr("Properties options"), tr("Properties Options"));

	QAction* PageSetupAction = OptionsMenu->addAction(tr("Page Setup..."));
	PageSetupAction->setToolTip(tr("Choose the page size, orientation and margins"));
	connect(PageSetupAction, &QAction::triggered, this, &swInstructionsPropertiesWidget::ShowPageSetupDialog);

	OptionsMenu->addSeparator();

	QAction* ExpandAction = OptionsMenu->addAction(tr("Expand All Sections"));
	ExpandAction->setToolTip(tr("Show the settings of every section"));
	connect(ExpandAction, &QAction::triggered, this, [this]()
	{
		SetCategoriesExpanded(true);
	});

	QAction* CollapseAction = OptionsMenu->addAction(tr("Collapse All Sections"));
	CollapseAction->setToolTip(tr("Hide the settings of every section"));
	connect(CollapseAction, &QAction::triggered, this, [this]()
	{
		SetCategoriesExpanded(false);
	});

	OptionsMenu->addSeparator();

	QAction* ResetAction = OptionsMenu->addAction(tr("Reset Properties"));
	ResetAction->setToolTip(tr("Restore the default look of the instructions"));
	connect(ResetAction, &QAction::triggered, mInstructions, &swInstructions::ResetProperties);

	// the sections scroll when the panel is shorter than them
	QScrollArea* ScrollArea = new QScrollArea(CentralWidget);
	ScrollArea->setWidgetResizable(true);
	ScrollArea->setFrameShape(QFrame::NoFrame);
	Layout->addWidget(ScrollArea);

	QWidget* CategoriesWidget = new QWidget(ScrollArea);
	mCategoriesLayout = new QVBoxLayout(CategoriesWidget);
	mCategoriesLayout->setContentsMargins(0, 0, 0, 0);
	ScrollArea->setWidget(CategoriesWidget);

	CreateWidgets();
	mCategoriesLayout->addStretch(1);

	// the panel is as wide as its widest row so the sections never scroll sideways
	ScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	ScrollArea->setMinimumWidth(CategoriesWidget->minimumSizeHint().width() + ScrollArea->verticalScrollBar()->sizeHint().width());

	connect(mInstructions, &swInstructions::StepSettingsChanged, this, &swInstructionsPropertiesWidget::UpdateEditors);
	connect(mInstructions, &swInstructions::PageLayoutChanged, this, &swInstructionsPropertiesWidget::UpdateWidget);
	connect(mInstructions, &swInstructions::PageSetupChanged, this, &swInstructionsPropertiesWidget::UpdateWidget);

	UpdateWidget();
}

void swInstructionsPropertiesWidget::ShowPageSetupDialog()
{
	swInstructionsPageSetup PageSetup = mInstructions->mPageSetup;
	swPageSetupDialog Dialog(this, &PageSetup);

	if (Dialog.exec() != QDialog::Accepted)
		return;

	mInstructions->SetPageSetup(PageSetup);
}

// the presets split the page in halves, thirds or quarters, horizontal dividers stack rows and vertical dividers line up columns
struct swDividerPreset
{
	int Rows;
	int Columns;
	const char* Name;
	const char* ToolTip;
};

static const swDividerPreset swDividerPresets[] =
{
	{ 1, 1, QT_TRANSLATE_NOOP("swInstructionsPropertiesWidget", "Full Page"), QT_TRANSLATE_NOOP("swInstructionsPropertiesWidget", "Give the page a single step, without dividers.") },
	{ 2, 1, QT_TRANSLATE_NOOP("swInstructionsPropertiesWidget", "Horizontal Halves"), QT_TRANSLATE_NOOP("swInstructionsPropertiesWidget", "Split the page in two rows with a horizontal divider.") },
	{ 3, 1, QT_TRANSLATE_NOOP("swInstructionsPropertiesWidget", "Horizontal Thirds"), QT_TRANSLATE_NOOP("swInstructionsPropertiesWidget", "Split the page in three rows with horizontal dividers.") },
	{ 4, 1, QT_TRANSLATE_NOOP("swInstructionsPropertiesWidget", "Horizontal Quarters"), QT_TRANSLATE_NOOP("swInstructionsPropertiesWidget", "Split the page in four rows with horizontal dividers.") },
	{ 1, 2, QT_TRANSLATE_NOOP("swInstructionsPropertiesWidget", "Vertical Halves"), QT_TRANSLATE_NOOP("swInstructionsPropertiesWidget", "Split the page in two columns with a vertical divider.") },
	{ 1, 3, QT_TRANSLATE_NOOP("swInstructionsPropertiesWidget", "Vertical Thirds"), QT_TRANSLATE_NOOP("swInstructionsPropertiesWidget", "Split the page in three columns with vertical dividers.") },
	{ 1, 4, QT_TRANSLATE_NOOP("swInstructionsPropertiesWidget", "Vertical Quarters"), QT_TRANSLATE_NOOP("swInstructionsPropertiesWidget", "Split the page in four columns with vertical dividers.") },
	{ 2, 2, QT_TRANSLATE_NOOP("swInstructionsPropertiesWidget", "Quarters"), QT_TRANSLATE_NOOP("swInstructionsPropertiesWidget", "Split the page in two rows and two columns with dividers both ways.") }
};

void swInstructionsPropertiesWidget::CreateWidgets()
{
	AddCategory(CategoryIndex::Document, tr("Document"), tr("<p><b>Document</b></p><p>One font for all text of the instructions.</p>"));

	AddRow(CategoryIndex::Document, {
		CreateToggleEditor(swInstructionsPropertyType::UseDocumentFont, tr("Use Document Font"), tr("Use one font for all text in the instructions.")),
		CreateFontEditor(swInstructionsPropertyType::DocumentFont, tr("Document Font"), tr("Font of all text while the document font is used."))
	});

	AddCategory(CategoryIndex::Cover, tr("Cover Page"), tr("<p><b>Cover Page</b></p><p>The first page, showing the model title, its author and the finished model. The title comes from the model description, or its name when there is none, the author from the model properties.</p>"));

	AddRow(CategoryIndex::Cover, {
		CreateToggleEditor(swInstructionsPropertyType::ShowCoverPage, tr("Show Cover Page"), tr("Start the instructions with a page showing the model title and the finished model.")),
		CreateToggleEditor(swInstructionsPropertyType::ShowCoverAuthor, tr("Show Author"), tr("Show the model author on the cover."))
	});

	AddRow(CategoryIndex::Cover, {
		CreateFontEditor(swInstructionsPropertyType::CoverTitleFont, tr("Cover Title Font"), tr("Font of the model title, the author uses half its size.")),
		CreateColorEditor(swInstructionsPropertyType::CoverTitleColor, tr("Cover Title Color"), tr("Color of the model title and author."))
	});

	AddCategory(CategoryIndex::BillOfMaterials, tr("Bill of Materials"), tr("<p><b>Bill of Materials</b></p><p>The last page, listing every piece of the model. It uses the Pieces List settings.</p>"));

	AddRow(CategoryIndex::BillOfMaterials, {
		CreateToggleEditor(swInstructionsPropertyType::ShowBillOfMaterials, tr("Show Bill of Materials"), tr("End the instructions with a page listing every piece of the model."))
	});

	AddCategory(CategoryIndex::PageLayout, tr("Steps per Page"), tr("<p><b>Steps per Page</b></p><p>The rows and columns of steps on the current page, the following pages flow on from it. The presets split the page in halves, thirds or quarters and show the step dividers. Steps fill landscape pages across each row and portrait pages down each column, following the orientation chosen in Page Setup.</p>"));

	std::vector<QWidget*> PresetButtons;

	for (int Preset = 0; Preset < static_cast<int>(std::size(swDividerPresets)); Preset++)
	{
		const swDividerPreset& DividerPreset = swDividerPresets[Preset];
		const QString Name = tr(DividerPreset.Name);

		QToolButton* Button = new QToolButton(this);
		Button->setIcon(CreateDividerPresetIcon(DividerPreset.Rows, DividerPreset.Columns, palette()));
		Button->setCheckable(true);
		Button->setToolTip(tr(DividerPreset.ToolTip));
		Button->setWhatsThis(Button->toolTip());
		Button->setAccessibleName(Name);
		swSetupPanelButton(Button);

		connect(Button, &QToolButton::clicked, this, [this, Preset]()
		{
			DividerPresetClicked(Preset);
		});

		mDividerPresetButtons.push_back(Button);
		PresetButtons.push_back(Button);
	}

	AddRow(CategoryIndex::PageLayout, PresetButtons);

	auto CreatePageLayoutSpinBox = [this](const QString& Name, const QString& ToolTip)
	{
		QSpinBox* SpinBox = new QSpinBox(this);
		SpinBox->setRange(1, 10);
		SpinBox->setPrefix(Name + QLatin1Char(' '));
		SpinBox->setToolTip(ToolTip);
		SpinBox->setWhatsThis(ToolTip);
		SpinBox->setAccessibleName(Name);
		swSetupPanelSpinBox(SpinBox);

		connect(SpinBox, qOverload<int>(&QSpinBox::valueChanged), this, [this]()
		{
			mInstructions->SetPageSettings(mPageIndex, mRowsSpinBox->value(), mColumnsSpinBox->value());
		});

		return SpinBox;
	};

	mRowsSpinBox = CreatePageLayoutSpinBox(tr("Rows"), tr("Number of step rows on this page."));
	mColumnsSpinBox = CreatePageLayoutSpinBox(tr("Columns"), tr("Number of step columns on this page."));

	QToolButton* ApplyAllButton = new QToolButton(this);
	ApplyAllButton->setText(tr("Apply to All Pages"));
	ApplyAllButton->setToolTip(tr("Use this number of rows and columns on every page."));
	ApplyAllButton->setWhatsThis(ApplyAllButton->toolTip());
	ApplyAllButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
	swSetupPanelButton(ApplyAllButton);

	connect(ApplyAllButton, &QToolButton::clicked, this, [this]()
	{
		swInstructionsPageSettings PageSettings;

		PageSettings.Rows = mRowsSpinBox->value();
		PageSettings.Columns = mColumnsSpinBox->value();

		mInstructions->SetDefaultPageSettings(PageSettings);
	});

	AddRow(CategoryIndex::PageLayout, { mRowsSpinBox, mColumnsSpinBox, ApplyAllButton });

	AddCategory(CategoryIndex::Page, tr("Page"), tr("<p><b>Page</b></p><p>The page background and the lines dividing neighboring steps.</p>"));

	AddRow(CategoryIndex::Page, {
		CreateColorEditor(swInstructionsPropertyType::PageBackgroundColor, tr("Page Background"), tr("Color filling the whole page, margins included."))
	});

	AddRow(CategoryIndex::Page, {
		CreateToggleEditor(swInstructionsPropertyType::ShowStepDividers, tr("Show Step Dividers"), tr("Draw lines between neighboring steps.")),
		CreateColorEditor(swInstructionsPropertyType::StepDividerColor, tr("Divider Color"), tr("Color of the lines between steps.")),
		CreateIntegerEditor(swInstructionsPropertyType::StepDividerWidth, tr("Divider Width"), tr("Width of the lines between steps, in points."), 1, 20)
	});

	AddCategory(CategoryIndex::Step, tr("Step"), tr("<p><b>Step</b></p><p>The step number and the area behind each step.</p>"));

	AddRow(CategoryIndex::Step, {
		CreateToggleEditor(swInstructionsPropertyType::ShowStepNumber, tr("Show Step Number"), tr("Show the number of each step in its top left corner.")),
		CreateFontEditor(swInstructionsPropertyType::StepNumberFont, tr("Step Number Font"), tr("Font of the step numbers.")),
		CreateColorEditor(swInstructionsPropertyType::StepNumberColor, tr("Step Number Color"), tr("Color of the step numbers."))
	});

	AddRow(CategoryIndex::Step, {
		CreateColorEditor(swInstructionsPropertyType::StepBackgroundColor, tr("Step Background"), tr("Color filling the area of each step, transparent shows the page background."))
	});

	AddCategory(CategoryIndex::PiecesList, tr("Pieces List"), tr("<p><b>Pieces List</b></p><p>The list of pieces added in each step. The bill of materials uses the same settings.</p>"));

	AddRow(CategoryIndex::PiecesList, {
		CreateToggleEditor(swInstructionsPropertyType::ShowStepPLI, tr("Show Pieces List"), tr("Show the pieces added in each step next to its number.")),
		CreateToggleEditor(swInstructionsPropertyType::PLIShowCounts, tr("Show Piece Counts"), tr("Show how many of each piece are needed."))
	});

	AddRow(CategoryIndex::PiecesList, {
		CreateSortEditor(swInstructionsPropertyType::PLISortOrder, tr("Pieces List Order"), tr("Order of the pieces in the list."))
	});

	AddRow(CategoryIndex::PiecesList, {
		CreateFontEditor(swInstructionsPropertyType::PLIFont, tr("Pieces List Font"), tr("Font of the piece counts.")),
		CreateColorEditor(swInstructionsPropertyType::PLITextColor, tr("Pieces List Text Color"), tr("Color of the piece counts."))
	});

	AddRow(CategoryIndex::PiecesList, {
		CreateColorEditor(swInstructionsPropertyType::PLIBackgroundColor, tr("Pieces List Background"), tr("Color filling the pieces list.")),
		CreateColorEditor(swInstructionsPropertyType::PLIBorderColor, tr("Pieces List Border Color"), tr("Color of the line around the pieces list.")),
		CreateIntegerEditor(swInstructionsPropertyType::PLIBorderWidth, tr("Border Width"), tr("Width of the line around the pieces list in points, 0 draws no line."), 0, 20)
	});

	AddRow(CategoryIndex::PiecesList, {
		CreateIntegerEditor(swInstructionsPropertyType::PLIThumbnailSize, tr("Thumbnail Size"), tr("Size each piece is drawn at in points, larger thumbnails show larger pieces."), 32, 512),
		CreateColorEditor(swInstructionsPropertyType::PLIThumbnailBackgroundColor, tr("Thumbnail Background"), tr("Color behind each piece and its count, transparent draws none."))
	});

	// the thumbnail settings belong to all pages or only to the current one
	mThumbnailPageButton = new QToolButton(this);
	mThumbnailPageButton->setText(tr("Thumbnails on This Page Only"));
	mThumbnailPageButton->setCheckable(true);
	mThumbnailPageButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
	mThumbnailPageButton->setToolTip(tr("Give this page its own thumbnail size and background, unchecked it follows all pages."));
	mThumbnailPageButton->setWhatsThis(mThumbnailPageButton->toolTip());
	mThumbnailPageButton->setAccessibleName(mThumbnailPageButton->text());
	swSetupPanelButton(mThumbnailPageButton);

	connect(mThumbnailPageButton, &QToolButton::toggled, this, &swInstructionsPropertiesWidget::SetThumbnailPageScope);

	AddRow(CategoryIndex::PiecesList, { mThumbnailPageButton });
}

void swInstructionsPropertiesWidget::AddCategory(CategoryIndex Index, const QString& Title, const QString& WhatsThis)
{
	CategoryWidgets& Category = mCategoryWidgets[static_cast<int>(Index)];

	Category.Button = new swCollapsibleWidgetButton(Title, this);
	Category.Button->setToolTip(tr("Expand or collapse the %1 properties.").arg(Title));
	Category.Button->setWhatsThis(WhatsThis);
	mCategoriesLayout->addWidget(Category.Button);

	Category.Rows = new QWidget(this);
	Category.Rows->setWhatsThis(WhatsThis);
	QVBoxLayout* RowsLayout = new QVBoxLayout(Category.Rows);
	RowsLayout->setContentsMargins(0, 0, 0, 0);
	mCategoriesLayout->addWidget(Category.Rows);

	connect(Category.Button, &swCollapsibleWidgetButton::StateChanged, this, &swInstructionsPropertiesWidget::UpdateCategories);
}

QWidget* swInstructionsPropertiesWidget::AddRow(CategoryIndex Index, const std::vector<QWidget*>& Editors)
{
	QWidget* Rows = mCategoryWidgets[static_cast<int>(Index)].Rows;
	QWidget* Row = swCreatePanelRow(Rows);
	Rows->layout()->addWidget(Row);

	// the editors of a row share its width evenly
	for (QWidget* Editor : Editors)
	{
		Editor->setParent(Row);
		Editor->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		Editor->setMinimumWidth(0);
		static_cast<QHBoxLayout*>(Row->layout())->addWidget(Editor, 1);
	}

	return Row;
}

void swInstructionsPropertiesWidget::RegisterEditor(swInstructionsPropertyType Type, EditorType Editor, QWidget* Widget, const QString& Name, const QString& ToolTip)
{
	Widget->setToolTip(ToolTip);
	Widget->setWhatsThis(ToolTip);
	Widget->setAccessibleName(Name);

	EditorWidgets& Editors = mEditors[static_cast<int>(Type)];
	Editors.Widget = Widget;
	Editors.Type = Editor;
	Editors.Name = Name;
}

QToolButton* swInstructionsPropertiesWidget::CreateToggleEditor(swInstructionsPropertyType Type, const QString& Name, const QString& ToolTip)
{
	QToolButton* Button = new QToolButton(this);
	RegisterEditor(Type, EditorType::Bool, Button, Name, ToolTip);
	Button->setText(Name);
	Button->setCheckable(true);
	Button->setToolButtonStyle(Qt::ToolButtonTextOnly);
	swSetupPanelButton(Button);

	connect(Button, &QToolButton::toggled, this, [this, Type](bool Checked)
	{
		mInstructions->SetDefaultBool(Type, Checked);
	});

	return Button;
}

QSpinBox* swInstructionsPropertiesWidget::CreateIntegerEditor(swInstructionsPropertyType Type, const QString& Name, const QString& ToolTip, int Minimum, int Maximum)
{
	QSpinBox* SpinBox = new QSpinBox(this);
	RegisterEditor(Type, EditorType::Integer, SpinBox, Name, ToolTip);
	SpinBox->setRange(Minimum, Maximum);
	SpinBox->setPrefix(Name + QLatin1Char(' '));
	swSetupPanelSpinBox(SpinBox);

	connect(SpinBox, qOverload<int>(&QSpinBox::valueChanged), this, [this, Type](int Value)
	{
		SetIntValue(Type, Value);
	});

	return SpinBox;
}

QComboBox* swInstructionsPropertiesWidget::CreateSortEditor(swInstructionsPropertyType Type, const QString& Name, const QString& ToolTip)
{
	QComboBox* ComboBox = new QComboBox(this);
	RegisterEditor(Type, EditorType::Sort, ComboBox, Name, ToolTip);

	// the items follow the order of swPartsListSort
	ComboBox->addItems({ tr("Sort by Color"), tr("Sort by Name"), tr("Sort by Part ID") });
	swSetupPanelEdit(ComboBox);

	connect(ComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, Type](int Index)
	{
		mInstructions->SetDefaultInt(Type, Index);
	});

	return ComboBox;
}

QToolButton* swInstructionsPropertiesWidget::CreateColorEditor(swInstructionsPropertyType Type, const QString& Name, const QString& ToolTip)
{
	QToolButton* Button = new QToolButton(this);
	RegisterEditor(Type, EditorType::Color, Button, Name, ToolTip);
	Button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	swSetupPanelButton(Button);

	connect(Button, &QToolButton::clicked, this, [this, Type]()
	{
		ColorButtonClicked(Type);
	});

	return Button;
}

QToolButton* swInstructionsPropertiesWidget::CreateFontEditor(swInstructionsPropertyType Type, const QString& Name, const QString& ToolTip)
{
	QToolButton* Button = new QToolButton(this);
	RegisterEditor(Type, EditorType::Font, Button, Name, ToolTip);
	Button->setToolButtonStyle(Qt::ToolButtonTextOnly);
	swSetupPanelButton(Button);

	connect(Button, &QToolButton::clicked, this, [this, Type]()
	{
		FontButtonClicked(Type);
	});

	return Button;
}

void swInstructionsPropertiesWidget::ColorButtonClicked(swInstructionsPropertyType Type)
{
	QToolButton* Button = qobject_cast<QToolButton*>(mEditors[static_cast<int>(Type)].Widget);
	const QColor Color = GetColorValue(Type);

	// backgrounds can be transparent so the picker edits the alpha channel too
	QMenu* Menu = new QMenu(Button);
	QWidgetAction* Action = new QWidgetAction(Menu);
	swColorDialogPopup* Popup = new swColorDialogPopup(Color, true, Menu);
	Action->setDefaultWidget(Popup);
	Menu->addAction(Action);

	connect(Popup, &swColorDialogPopup::ColorSelected, this, [this, Type](QColor SelectedColor)
	{
		if (SelectedColor.isValid())
			SetColorValue(Type, SelectedColor);
	});

	Menu->exec(Button->mapToGlobal(Button->rect().bottomLeft()));

	delete Menu;
}

void swInstructionsPropertiesWidget::FontButtonClicked(swInstructionsPropertyType Type)
{
	bool Ok = false;
	QFont Font = mInstructions->GetFontProperty(Type, mModel, mStep);

	Font = QFontDialog::getFont(&Ok, Font, this, mEditors[static_cast<int>(Type)].Name);

	if (Ok)
		mInstructions->SetDefaultFont(Type, Font);
}

void swInstructionsPropertiesWidget::SetCategoriesExpanded(bool Expanded)
{
	for (CategoryWidgets& Category : mCategoryWidgets)
		Category.Button->SetExpanded(Expanded);

	UpdateCategories();
}

QIcon swInstructionsPropertiesWidget::CreateColorIcon(const QColor& Color, int Size)
{
	QPixmap Pixmap(Size, Size);
	QPainter Painter(&Pixmap);

	// a checkerboard behind the color shows how transparent it is
	const int CellSize = qMax(Size / 4, 2);

	for (int Y = 0; Y < Size; Y += CellSize)
		for (int X = 0; X < Size; X += CellSize)
			Painter.fillRect(X, Y, CellSize, CellSize, ((X + Y) / CellSize) % 2 ? QColor(204, 204, 204) : QColor(255, 255, 255));

	Painter.fillRect(Pixmap.rect(), Color);
	Painter.setPen(QColor(128, 128, 128));
	Painter.drawRect(Pixmap.rect().adjusted(0, 0, -1, -1));
	Painter.end();

	return QIcon(Pixmap);
}

void swInstructionsPropertiesWidget::SelectionChanged(QGraphicsItem* FocusItem)
{
	// only the kind and step of the item are kept, page rebuilds delete the items
	FocusType Focus = FocusType::None;
	swModel* Model = nullptr;
	swStep Step = 1;

	if (swInstructionsStepImageItem* ImageItem = dynamic_cast<swInstructionsStepImageItem*>(FocusItem))
	{
		Focus = FocusType::StepImage;
		Model = ImageItem->GetModel();
		Step = ImageItem->GetStep();
	}
	else if (swInstructionsStepNumberItem* NumberItem = dynamic_cast<swInstructionsStepNumberItem*>(FocusItem))
	{
		Focus = FocusType::StepNumber;
		Model = NumberItem->GetModel();
		Step = NumberItem->GetStep();
	}
	else if (swInstructionsPartsListItem* PartsItem = dynamic_cast<swInstructionsPartsListItem*>(FocusItem))
	{
		Focus = FocusType::PiecesList;
		Model = PartsItem->GetModel();
		Step = PartsItem->GetStep();
	}

	// the model image of the cover and the list of the bill of materials show their page settings
	if (GetPageType() != swInstructionsPageType::Steps)
		Focus = FocusType::None;

	if (Focus == mFocus && Model == mModel && Step == mStep)
		return;

	mFocus = Focus;
	mModel = Model;
	mStep = Step;

	UpdateWidget();
}

void swInstructionsPropertiesWidget::SetCurrentPage(int PageIndex)
{
	mPageIndex = PageIndex;

	// the page layout shows the rows and columns of the current page
	UpdatePageLayout();
	UpdateCategories();
}

void swInstructionsPropertiesWidget::UpdateWidget()
{
	UpdateEditors();
	UpdatePageLayout();
	UpdateCategories();
}

void swInstructionsPropertiesWidget::UpdateEditors()
{
	for (int TypeIndex = 0; TypeIndex < static_cast<int>(swInstructionsPropertyType::Count); TypeIndex++)
	{
		const swInstructionsPropertyType Type = static_cast<swInstructionsPropertyType>(TypeIndex);
		const EditorWidgets& Editor = mEditors[TypeIndex];

		if (!Editor.Widget)
			continue;

		QSignalBlocker Blocker(Editor.Widget);

		switch (Editor.Type)
		{
		case EditorType::Bool:
			qobject_cast<QToolButton*>(Editor.Widget)->setChecked(mInstructions->GetBoolProperty(Type, mModel, mStep));
			break;

		case EditorType::Integer:
			qobject_cast<QSpinBox*>(Editor.Widget)->setValue(GetIntValue(Type));
			break;

		case EditorType::Sort:
			qobject_cast<QComboBox*>(Editor.Widget)->setCurrentIndex(mInstructions->GetIntProperty(Type, mModel, mStep));
			break;

		case EditorType::Color:
			{
				QToolButton* Button = qobject_cast<QToolButton*>(Editor.Widget);
				const QColor Color = GetColorValue(Type);

				Button->setIcon(CreateColorIcon(Color, Button->iconSize().height()));
				Button->setText(Color.alpha() ? Color.name().toUpper() : tr("Transparent"));
			}
			break;

		case EditorType::Font:
			{
				const QFont Font = mInstructions->GetFontProperty(Type, mModel, mStep);
				qobject_cast<QToolButton*>(Editor.Widget)->setText(QString("%1 %2").arg(Font.family(), swFormatIntegerLocalized(Font.pointSize(), locale())));
			}
			break;
		}
	}

	// settings without effect are disabled, the document font replaces every element font
	auto GetBool = [this](swInstructionsPropertyType Type)
	{
		return mInstructions->GetBoolProperty(Type, mModel, mStep);
	};

	auto SetEnabled = [this](swInstructionsPropertyType Type, bool Enabled)
	{
		mEditors[static_cast<int>(Type)].Widget->setEnabled(Enabled);
	};

	const bool UseDocumentFont = GetBool(swInstructionsPropertyType::UseDocumentFont);
	const bool ShowStepNumber = GetBool(swInstructionsPropertyType::ShowStepNumber);
	const bool ShowStepDividers = GetBool(swInstructionsPropertyType::ShowStepDividers);

	SetEnabled(swInstructionsPropertyType::DocumentFont, UseDocumentFont);
	const bool ShowCoverPage = GetBool(swInstructionsPropertyType::ShowCoverPage);

	SetEnabled(swInstructionsPropertyType::CoverTitleFont, !UseDocumentFont && ShowCoverPage);
	SetEnabled(swInstructionsPropertyType::CoverTitleColor, ShowCoverPage);
	SetEnabled(swInstructionsPropertyType::ShowCoverAuthor, ShowCoverPage);
	SetEnabled(swInstructionsPropertyType::StepNumberFont, !UseDocumentFont && ShowStepNumber);
	SetEnabled(swInstructionsPropertyType::StepNumberColor, ShowStepNumber);
	SetEnabled(swInstructionsPropertyType::PLIFont, !UseDocumentFont);
	SetEnabled(swInstructionsPropertyType::StepDividerColor, ShowStepDividers);
	SetEnabled(swInstructionsPropertyType::StepDividerWidth, ShowStepDividers);

	const swInstructionsPageType PageType = GetPageType();
	QSignalBlocker Blocker(mThumbnailPageButton);

	mThumbnailPageButton->setEnabled(PageType == swInstructionsPageType::Steps || PageType == swInstructionsPageType::BillOfMaterials);
	mThumbnailPageButton->setChecked(mInstructions->HasPageProperty(mPageIndex, swInstructionsPropertyType::PLIThumbnailSize));
}

int swInstructionsPropertiesWidget::GetIntValue(swInstructionsPropertyType Type) const
{
	// page properties always show the current page, which may follow all pages
	if (swInstructions::IsPageProperty(Type))
		return mInstructions->GetPageIntProperty(mPageIndex, Type);

	return mInstructions->GetIntProperty(Type, mModel, mStep);
}

QColor swInstructionsPropertiesWidget::GetColorValue(swInstructionsPropertyType Type) const
{
	if (swInstructions::IsPageProperty(Type))
		return mInstructions->GetPageColorProperty(mPageIndex, Type);

	return mInstructions->GetColorProperty(Type, mModel, mStep);
}

void swInstructionsPropertiesWidget::SetIntValue(swInstructionsPropertyType Type, int Value)
{
	if (mInstructions->HasPageProperty(mPageIndex, Type))
		mInstructions->SetPageInt(mPageIndex, Type, Value);
	else
		mInstructions->SetDefaultInt(Type, Value);
}

void swInstructionsPropertiesWidget::SetColorValue(swInstructionsPropertyType Type, const QColor& Color)
{
	if (mInstructions->HasPageProperty(mPageIndex, Type))
		mInstructions->SetPageColor(mPageIndex, Type, Color);
	else
		mInstructions->SetDefaultColor(Type, Color);
}

void swInstructionsPropertiesWidget::SetThumbnailPageScope(bool PageOnly)
{
	mInstructions->SetPageScope(mPageIndex, swInstructionsPropertyType::PLIThumbnailSize, PageOnly);
	mInstructions->SetPageScope(mPageIndex, swInstructionsPropertyType::PLIThumbnailBackgroundColor, PageOnly);
}

void swInstructionsPropertiesWidget::DividerPresetClicked(int Preset)
{
	const swDividerPreset& DividerPreset = swDividerPresets[Preset];

	mInstructions->SetPageDividerPreset(mPageIndex, DividerPreset.Rows, DividerPreset.Columns);

	// clicking the current preset changes nothing but unchecks its button
	UpdatePageLayout();
}

QIcon swInstructionsPropertiesWidget::CreateDividerPresetIcon(int Rows, int Columns, const QPalette& Palette)
{
	// the outline is the page, the lines are its dividers
	const int Size = 32;
	const QRectF PageRect(6.0, 4.0, 20.0, 24.0);

	QPixmap Pixmap(Size, Size);
	Pixmap.fill(Qt::transparent);

	QPainter Painter(&Pixmap);
	QColor OutlineColor = Palette.color(QPalette::WindowText);
	OutlineColor.setAlpha(110);

	Painter.setPen(QPen(OutlineColor, 2.0));
	Painter.drawRect(PageRect);
	Painter.setPen(QPen(Palette.color(QPalette::WindowText), 2.0));

	for (int Row = 1; Row < Rows; Row++)
	{
		const qreal Y = PageRect.top() + PageRect.height() * Row / Rows;
		Painter.drawLine(QPointF(PageRect.left() + 3.0, Y), QPointF(PageRect.right() - 3.0, Y));
	}

	for (int Column = 1; Column < Columns; Column++)
	{
		const qreal X = PageRect.left() + PageRect.width() * Column / Columns;
		Painter.drawLine(QPointF(X, PageRect.top() + 3.0), QPointF(X, PageRect.bottom() - 3.0));
	}

	Painter.end();

	return QIcon(Pixmap);
}

void swInstructionsPropertiesWidget::UpdatePageLayout()
{
	const swInstructionsPageSettings PageSettings = mInstructions->GetPageSettings(mPageIndex);

	QSignalBlocker RowsBlocker(mRowsSpinBox);
	QSignalBlocker ColumnsBlocker(mColumnsSpinBox);

	mRowsSpinBox->setValue(PageSettings.Rows);
	mColumnsSpinBox->setValue(PageSettings.Columns);

	// the preset matching the rows and columns of the page is checked
	for (int Preset = 0; Preset < static_cast<int>(mDividerPresetButtons.size()); Preset++)
		mDividerPresetButtons[Preset]->setChecked(swDividerPresets[Preset].Rows == PageSettings.Rows && swDividerPresets[Preset].Columns == PageSettings.Columns);
}

swInstructionsPageType swInstructionsPropertiesWidget::GetPageType() const
{
	if (mPageIndex < 0 || mPageIndex >= static_cast<int>(mInstructions->mPages.size()))
		return swInstructionsPageType::Steps;

	return mInstructions->mPages[mPageIndex].Type;
}

void swInstructionsPropertiesWidget::UpdateCategories()
{
	std::array<bool, static_cast<int>(CategoryIndex::Count)> Visible = {};

	// a selected item shows only its own settings, otherwise everything that applies to the page
	switch (mFocus)
	{
	case FocusType::None:
		{
			// the page type decides which page and step settings apply, the cover and bill of materials can always be switched on
			const swInstructionsPageType PageType = GetPageType();

			Visible[static_cast<int>(CategoryIndex::Document)] = true;
			Visible[static_cast<int>(CategoryIndex::Cover)] = true;
			Visible[static_cast<int>(CategoryIndex::BillOfMaterials)] = true;
			Visible[static_cast<int>(CategoryIndex::PageLayout)] = PageType == swInstructionsPageType::Steps;
			Visible[static_cast<int>(CategoryIndex::Page)] = true;
			Visible[static_cast<int>(CategoryIndex::Step)] = PageType == swInstructionsPageType::Steps;
			Visible[static_cast<int>(CategoryIndex::PiecesList)] = PageType == swInstructionsPageType::Steps || PageType == swInstructionsPageType::BillOfMaterials;
		}
		break;

	case FocusType::StepImage:
		Visible[static_cast<int>(CategoryIndex::Step)] = true;
		Visible[static_cast<int>(CategoryIndex::PiecesList)] = true;
		break;

	case FocusType::StepNumber:
		Visible[static_cast<int>(CategoryIndex::Step)] = true;
		break;

	case FocusType::PiecesList:
		Visible[static_cast<int>(CategoryIndex::PiecesList)] = true;
		break;
	}

	for (int Index = 0; Index < static_cast<int>(Visible.size()); Index++)
	{
		CategoryWidgets& Category = mCategoryWidgets[Index];

		Category.Button->setVisible(Visible[Index]);
		Category.Rows->setVisible(Visible[Index] && Category.Button->IsExpanded());
	}
}

// the names follow the placement grid row by row, the inner cells lie inside the anchor
static const char* const swPlacementNames[SW_PLACEMENT_COUNT] =
{
	QT_TRANSLATE_NOOP("swInstructionsPlacementWidget", "Above, Outside Left"),
	QT_TRANSLATE_NOOP("swInstructionsPlacementWidget", "Above, Left Aligned"),
	QT_TRANSLATE_NOOP("swInstructionsPlacementWidget", "Above, Centered"),
	QT_TRANSLATE_NOOP("swInstructionsPlacementWidget", "Above, Right Aligned"),
	QT_TRANSLATE_NOOP("swInstructionsPlacementWidget", "Above, Outside Right"),
	QT_TRANSLATE_NOOP("swInstructionsPlacementWidget", "Left, Top Aligned"),
	QT_TRANSLATE_NOOP("swInstructionsPlacementWidget", "Top Left"),
	QT_TRANSLATE_NOOP("swInstructionsPlacementWidget", "Top"),
	QT_TRANSLATE_NOOP("swInstructionsPlacementWidget", "Top Right"),
	QT_TRANSLATE_NOOP("swInstructionsPlacementWidget", "Right, Top Aligned"),
	QT_TRANSLATE_NOOP("swInstructionsPlacementWidget", "Left, Centered"),
	QT_TRANSLATE_NOOP("swInstructionsPlacementWidget", "Left"),
	QT_TRANSLATE_NOOP("swInstructionsPlacementWidget", "Center"),
	QT_TRANSLATE_NOOP("swInstructionsPlacementWidget", "Right"),
	QT_TRANSLATE_NOOP("swInstructionsPlacementWidget", "Right, Centered"),
	QT_TRANSLATE_NOOP("swInstructionsPlacementWidget", "Left, Bottom Aligned"),
	QT_TRANSLATE_NOOP("swInstructionsPlacementWidget", "Bottom Left"),
	QT_TRANSLATE_NOOP("swInstructionsPlacementWidget", "Bottom"),
	QT_TRANSLATE_NOOP("swInstructionsPlacementWidget", "Bottom Right"),
	QT_TRANSLATE_NOOP("swInstructionsPlacementWidget", "Right, Bottom Aligned"),
	QT_TRANSLATE_NOOP("swInstructionsPlacementWidget", "Below, Outside Left"),
	QT_TRANSLATE_NOOP("swInstructionsPlacementWidget", "Below, Left Aligned"),
	QT_TRANSLATE_NOOP("swInstructionsPlacementWidget", "Below, Centered"),
	QT_TRANSLATE_NOOP("swInstructionsPlacementWidget", "Below, Right Aligned"),
	QT_TRANSLATE_NOOP("swInstructionsPlacementWidget", "Below, Outside Right")
};

swInstructionsPlacementWidget::swInstructionsPlacementWidget(QWidget* Parent, swInstructions* Instructions)
	: QDockWidget(Parent), mInstructions(Instructions)
{
	setWindowTitle(tr("Placement"));
	setWhatsThis(tr("<p><b>Placement</b></p><p>Place the selected page element on a predefined area: the step model, step number, pieces list, cover model, cover title, cover author or bill of materials. Step elements are placed in the part of the page their step gets from the split, or outside the other step element. Placements apply to all pages.</p><p>Every element can also be dragged on the page, it keeps its distance from its placement. A locked element keeps its position whatever its placement and is not dragged. The page grid helps aligning dragged elements, it is never printed.</p>"));
	swSetupWidgetHelp(this, nullptr);

	QWidget* CentralWidget = new QWidget(this);
	setWidget(CentralWidget);

	QVBoxLayout* Layout = new QVBoxLayout(CentralWidget);

	mElementLabel = new QLabel(CentralWidget);
	mElementLabel->setWordWrap(true);
	Layout->addWidget(mElementLabel);

	QWidget* AnchorRow = swCreatePanelRow(CentralWidget);
	Layout->addWidget(AnchorRow);

	mAnchorComboBox = new QComboBox(AnchorRow);
	mAnchorComboBox->setToolTip(tr("Choose what the element is placed against."));
	mAnchorComboBox->setWhatsThis(mAnchorComboBox->toolTip());
	mAnchorComboBox->setAccessibleName(tr("Relative To"));
	swSetupPanelEdit(mAnchorComboBox);
	mAnchorComboBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	static_cast<QHBoxLayout*>(AnchorRow->layout())->addWidget(mAnchorComboBox, 1);

	connect(mAnchorComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, &swInstructionsPlacementWidget::AnchorChanged);

	// the grid shows the anchor in its middle, its inner buttons place inside it and its outer ring outside
	mGridWidget = new QWidget(CentralWidget);
	QGridLayout* GridLayout = new QGridLayout(mGridWidget);
	GridLayout->setContentsMargins(0, 0, 0, 0);
	Layout->addWidget(mGridWidget, 0, Qt::AlignHCenter);

	for (int Placement = 0; Placement < SW_PLACEMENT_COUNT; Placement++)
	{
		const QString Name = tr(swPlacementNames[Placement]);

		QToolButton* Button = new QToolButton(mGridWidget);
		Button->setIcon(CreatePlacementIcon(Placement, palette()));
		Button->setCheckable(true);
		Button->setToolTip(tr("Place the element: %1.").arg(Name));
		Button->setWhatsThis(Button->toolTip());
		Button->setAccessibleName(Name);
		swSetupPanelButton(Button);
		GridLayout->addWidget(Button, Placement / SW_PLACEMENT_GRID_SIZE, Placement % SW_PLACEMENT_GRID_SIZE);
		mPlacementButtons[Placement] = Button;

		connect(Button, &QToolButton::clicked, this, [this, Placement]()
		{
			PlacementClicked(Placement);
		});
	}

	QWidget* ResetRow = swCreatePanelRow(CentralWidget);
	Layout->addWidget(ResetRow);

	mResetPositionButton = new QToolButton(ResetRow);
	mResetPositionButton->setText(tr("Reset Position"));
	mResetPositionButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
	mResetPositionButton->setToolTip(tr("Move the dragged element back onto its placement."));
	mResetPositionButton->setWhatsThis(mResetPositionButton->toolTip());
	mResetPositionButton->setAccessibleName(mResetPositionButton->text());
	swSetupPanelButton(mResetPositionButton);
	mResetPositionButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	static_cast<QHBoxLayout*>(ResetRow->layout())->addWidget(mResetPositionButton, 1);

	mLockButton = new QToolButton(ResetRow);
	mLockButton->setText(tr("Lock Position"));
	mLockButton->setCheckable(true);
	mLockButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
	mLockButton->setToolTip(tr("Keep the element where it is, placements and dragging no longer move it."));
	mLockButton->setWhatsThis(mLockButton->toolTip());
	mLockButton->setAccessibleName(mLockButton->text());
	swSetupPanelButton(mLockButton);
	mLockButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	static_cast<QHBoxLayout*>(ResetRow->layout())->addWidget(mLockButton, 1);

	connect(mResetPositionButton, &QToolButton::clicked, this, &swInstructionsPlacementWidget::ResetPositionClicked);
	connect(mLockButton, &QToolButton::toggled, this, &swInstructionsPlacementWidget::LockClicked);

	// the grid settings apply to every element and stay available without a selection
	QWidget* GridRow = swCreatePanelRow(CentralWidget);
	QHBoxLayout* GridRowLayout = static_cast<QHBoxLayout*>(GridRow->layout());
	Layout->addWidget(GridRow);

	auto CreateGridButton = [GridRow, GridRowLayout](const QString& Name, const QString& ToolTip)
	{
		QToolButton* Button = new QToolButton(GridRow);
		Button->setText(Name);
		Button->setCheckable(true);
		Button->setToolButtonStyle(Qt::ToolButtonTextOnly);
		Button->setToolTip(ToolTip);
		Button->setWhatsThis(ToolTip);
		Button->setAccessibleName(Name);
		swSetupPanelButton(Button);
		Button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		GridRowLayout->addWidget(Button, 1);

		return Button;
	};

	mShowGridButton = CreateGridButton(tr("Show Grid"), tr("Draw the page grid on screen, it is never printed."));
	mSnapToGridButton = CreateGridButton(tr("Snap to Grid"), tr("Snap dragged elements to the page grid."));

	// the grid area and size share a row under the grid toggles
	QWidget* GridSizeRow = swCreatePanelRow(CentralWidget);
	QHBoxLayout* GridSizeRowLayout = static_cast<QHBoxLayout*>(GridSizeRow->layout());
	Layout->addWidget(GridSizeRow);

	mGridScopeComboBox = new QComboBox(GridSizeRow);
	mGridScopeComboBox->addItem(tr("Within Margins"));
	mGridScopeComboBox->addItem(tr("Full Page"));
	mGridScopeComboBox->setToolTip(tr("Choose whether the grid covers the area inside the margins or the full page."));
	mGridScopeComboBox->setWhatsThis(mGridScopeComboBox->toolTip());
	mGridScopeComboBox->setAccessibleName(tr("Grid Area"));
	swSetupPanelEdit(mGridScopeComboBox);
	mGridScopeComboBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	GridSizeRowLayout->addWidget(mGridScopeComboBox, 1);

	mGridSizeSpinBox = new swInstructionsGridSizeSpinBox(GridSizeRow);
	mGridSizeSpinBox->setPrefix(tr("Grid Size") + QLatin1Char(' '));
	mGridSizeSpinBox->setToolTip(tr("Distance between the grid lines in the margin units of the page setup."));
	mGridSizeSpinBox->setWhatsThis(mGridSizeSpinBox->toolTip());
	mGridSizeSpinBox->setAccessibleName(tr("Grid Size"));
	swSetupPanelSpinBox(mGridSizeSpinBox);
	mGridSizeSpinBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	GridSizeRowLayout->addWidget(mGridSizeSpinBox, 1);

	connect(mShowGridButton, &QToolButton::toggled, mInstructions, &swInstructions::SetShowGrid);
	connect(mSnapToGridButton, &QToolButton::toggled, mInstructions, &swInstructions::SetSnapToGrid);
	connect(mGridScopeComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int Index)
	{
		mInstructions->SetGridWithinMargins(Index == 0);
	});
	connect(mGridSizeSpinBox, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &swInstructionsPlacementWidget::GridSizeChanged);

	Layout->addStretch(1);

	connect(mInstructions, &swInstructions::PageLayoutChanged, this, &swInstructionsPlacementWidget::UpdateWidget);
	connect(mInstructions, &swInstructions::GridChanged, this, &swInstructionsPlacementWidget::UpdateGrid);
	connect(mInstructions, &swInstructions::PageSetupChanged, this, &swInstructionsPlacementWidget::UpdateGrid);

	UpdateWidget();
	UpdateGrid();
}

QIcon swInstructionsPlacementWidget::CreatePlacementIcon(int Placement, const QPalette& Palette)
{
	// the outline is the anchor, the block shows where the element goes
	const int Size = 32;
	const int Positions[SW_PLACEMENT_GRID_SIZE] = { 1, 9, 13, 17, 25 };
	const int BlockSize = 6;

	QPixmap Pixmap(Size, Size);
	Pixmap.fill(Qt::transparent);

	QPainter Painter(&Pixmap);
	QColor OutlineColor = Palette.color(QPalette::WindowText);
	OutlineColor.setAlpha(110);

	Painter.setPen(QPen(OutlineColor, 2.0));
	Painter.drawRect(QRectF(8.0, 8.0, 16.0, 16.0));
	Painter.fillRect(Positions[Placement % SW_PLACEMENT_GRID_SIZE], Positions[Placement / SW_PLACEMENT_GRID_SIZE], BlockSize, BlockSize, Palette.color(QPalette::WindowText));
	Painter.end();

	return QIcon(Pixmap);
}

void swInstructionsPlacementWidget::SelectionChanged(QGraphicsItem* FocusItem)
{
	swInstructionsElementKey Key;
	swGetElementKey(FocusItem, Key);

	// the positions of the item are kept to lock or unlock it in place, page rebuilds delete the items
	if (FocusItem)
	{
		mElementPosition = FocusItem->pos();
		mPlacementPosition = FocusItem->data(SW_PLACEMENT_POSITION_DATA).toPointF();
	}

	if (Key == mElementKey)
		return;

	mElementKey = Key;
	mElement = std::get<0>(Key);
	UpdateWidget();
}

swInstructionsPropertyType swInstructionsPlacementWidget::GetPlacementType() const
{
	switch (mElement)
	{
	case swInstructionsPlacementElement::StepNumber:
		return swInstructionsPropertyType::StepNumberPlacement;

	case swInstructionsPlacementElement::PiecesList:
		return swInstructionsPropertyType::PLIPlacement;

	case swInstructionsPlacementElement::CoverTitle:
		return swInstructionsPropertyType::CoverTitlePlacement;

	case swInstructionsPlacementElement::CoverAuthor:
		return swInstructionsPropertyType::CoverAuthorPlacement;

	case swInstructionsPlacementElement::BillOfMaterials:
		return swInstructionsPropertyType::BillOfMaterialsPlacement;

	case swInstructionsPlacementElement::StepImage:
		return swInstructionsPropertyType::StepImagePlacement;

	case swInstructionsPlacementElement::CoverImage:
		return swInstructionsPropertyType::CoverImagePlacement;

	case swInstructionsPlacementElement::None:
		break;
	}

	return swInstructionsPropertyType::Count;
}

swInstructionsPropertyType swInstructionsPlacementWidget::GetAnchorType() const
{
	switch (mElement)
	{
	case swInstructionsPlacementElement::StepNumber:
		return swInstructionsPropertyType::StepNumberAnchor;

	case swInstructionsPlacementElement::PiecesList:
		return swInstructionsPropertyType::PLIAnchor;

	case swInstructionsPlacementElement::CoverTitle:
	case swInstructionsPlacementElement::CoverAuthor:
	case swInstructionsPlacementElement::BillOfMaterials:
	case swInstructionsPlacementElement::StepImage:
	case swInstructionsPlacementElement::CoverImage:
	case swInstructionsPlacementElement::None:
		break;
	}

	return swInstructionsPropertyType::Count;
}

bool swInstructionsPlacementWidget::IsInsideAnchor() const
{
	const swInstructionsPropertyType AnchorType = GetAnchorType();

	if (AnchorType == swInstructionsPropertyType::Count)
		return true;

	return static_cast<swInstructionsPlacementAnchor>(mInstructions->GetIntProperty(AnchorType, nullptr, 0)) == swInstructionsPlacementAnchor::Area;
}

void swInstructionsPlacementWidget::AnchorChanged(int Index)
{
	const swInstructionsPropertyType AnchorType = GetAnchorType();

	if (AnchorType == swInstructionsPropertyType::Count || Index < 0)
		return;

	// the placement moves inside the area or outside the element so it stays valid for the new anchor
	const int Anchor = mAnchorComboBox->itemData(Index).toInt();
	const swInstructionsPropertyType PlacementType = GetPlacementType();
	const int Placement = swGetValidPlacement(mInstructions->GetIntProperty(PlacementType, nullptr, 0), static_cast<swInstructionsPlacementAnchor>(Anchor) == swInstructionsPlacementAnchor::Area);

	mInstructions->SetDefaultInt(PlacementType, Placement);
	mInstructions->SetDefaultInt(AnchorType, Anchor);
}

void swInstructionsPlacementWidget::PlacementClicked(int Placement)
{
	const swInstructionsPropertyType PlacementType = GetPlacementType();

	// the element leaves the position it was dragged to
	if (PlacementType != swInstructionsPropertyType::Count)
		mInstructions->SetElementPlacement(PlacementType, Placement, mElementKey);

	// clicking the current placement changes nothing but unchecks its button
	UpdateWidget();
}

void swInstructionsPlacementWidget::ResetPositionClicked()
{
	mInstructions->ResetElementOffset(mElementKey);
}

void swInstructionsPlacementWidget::LockClicked(bool Locked)
{
	if (Locked)
		mInstructions->LockElement(mElementKey, mElementPosition);
	else
		mInstructions->UnlockElement(mElementKey, mElementPosition - mPlacementPosition);
}

void swInstructionsPlacementWidget::UpdateGrid()
{
	QSignalBlocker ShowBlocker(mShowGridButton);
	QSignalBlocker SnapBlocker(mSnapToGridButton);
	QSignalBlocker SizeBlocker(mGridSizeSpinBox);
	QSignalBlocker ScopeBlocker(mGridScopeComboBox);

	mShowGridButton->setChecked(mInstructions->IsGridShown());
	mSnapToGridButton->setChecked(mInstructions->IsSnapToGrid());
	mGridScopeComboBox->setCurrentIndex(mInstructions->IsGridWithinMargins() ? 0 : 1);

	// the size follows the margin units, each step is a usual increment of the unit
	const swInstructionsPageUnits Units = mInstructions->GetGridUnits();
	const double Scale = swGetPageUnitScale(Units);
	int Decimals = 0;
	double Step = 1.0;

	switch (Units)
	{
	case swInstructionsPageUnits::Pixels:
		break;

	case swInstructionsPageUnits::Millimeters:
		Decimals = 1;
		break;

	case swInstructionsPageUnits::Centimeters:
		Decimals = 2;
		Step = 0.1;
		break;

	case swInstructionsPageUnits::Inches:
		Decimals = 3;
		Step = 0.125;
		break;
	}

	mGridSizeSpinBox->setDecimals(Decimals);
	mGridSizeSpinBox->setSingleStep(Step);
	mGridSizeSpinBox->setRange(SW_GRID_SIZE_MINIMUM / Scale, SW_GRID_SIZE_MAXIMUM / Scale);
	mGridSizeSpinBox->setSuffix(QLatin1Char(' ') + swGetPageUnitSuffix(Units));
	mGridSizeSpinBox->setValue(mInstructions->GetGridSize() / Scale);
}

void swInstructionsPlacementWidget::GridSizeChanged(double Size)
{
	mInstructions->SetGridSize(Size * swGetPageUnitScale(mInstructions->GetGridUnits()));
}

void swInstructionsPlacementWidget::UpdateWidget()
{
	// a locked element keeps its position, it is neither placed nor dragged
	const bool Locked = mInstructions->IsElementLocked(mElementKey);
	const bool Active = GetPlacementType() != swInstructionsPropertyType::Count && !Locked;

	mResetPositionButton->setEnabled(Active && mInstructions->HasElementOffset(mElementKey));

	{
		QSignalBlocker Blocker(mLockButton);
		mLockButton->setEnabled(mElement != swInstructionsPlacementElement::None);
		mLockButton->setChecked(Locked);
	}

	switch (mElement)
	{
	case swInstructionsPlacementElement::StepNumber:
		mElementLabel->setText(tr("Step Number"));
		break;

	case swInstructionsPlacementElement::PiecesList:
		mElementLabel->setText(tr("Pieces List"));
		break;

	case swInstructionsPlacementElement::CoverTitle:
		mElementLabel->setText(tr("Cover Title"));
		break;

	case swInstructionsPlacementElement::CoverAuthor:
		mElementLabel->setText(tr("Cover Author"));
		break;

	case swInstructionsPlacementElement::BillOfMaterials:
		mElementLabel->setText(tr("Bill of Materials"));
		break;

	case swInstructionsPlacementElement::StepImage:
		mElementLabel->setText(tr("Step Model"));
		break;

	case swInstructionsPlacementElement::CoverImage:
		mElementLabel->setText(tr("Cover Model"));
		break;

	case swInstructionsPlacementElement::None:
		mElementLabel->setText(tr("Select an element on the page to place, move or lock it."));
		break;
	}

	{
		QSignalBlocker Blocker(mAnchorComboBox);
		mAnchorComboBox->clear();
		mAnchorComboBox->addItem(tr("Page Area"), static_cast<int>(swInstructionsPlacementAnchor::Area));

		if (mElement == swInstructionsPlacementElement::StepNumber)
			mAnchorComboBox->addItem(tr("Pieces List"), static_cast<int>(swInstructionsPlacementAnchor::PiecesList));
		else if (mElement == swInstructionsPlacementElement::PiecesList)
			mAnchorComboBox->addItem(tr("Step Number"), static_cast<int>(swInstructionsPlacementAnchor::StepNumber));

		const swInstructionsPropertyType AnchorType = GetAnchorType();

		if (AnchorType != swInstructionsPropertyType::Count)
			mAnchorComboBox->setCurrentIndex(qMax(mAnchorComboBox->findData(mInstructions->GetIntProperty(AnchorType, nullptr, 0)), 0));

		mAnchorComboBox->setEnabled(Active && mAnchorComboBox->count() > 1);
	}

	const bool Inside = IsInsideAnchor();
	const swInstructionsPropertyType PlacementType = GetPlacementType();
	int CurrentPlacement = -1;

	if (Active)
		CurrentPlacement = swGetValidPlacement(mInstructions->GetIntProperty(PlacementType, nullptr, 0), Inside);

	// an area takes the inner placements, an element only the outer ring
	for (int Placement = 0; Placement < SW_PLACEMENT_COUNT; Placement++)
	{
		QToolButton* Button = mPlacementButtons[Placement];

		Button->setEnabled(Active && swIsInsidePlacement(Placement) == Inside);
		Button->setChecked(Active && Placement == CurrentPlacement);
	}
}

swInstructionsDialog::swInstructionsDialog(QWidget* Parent, Project* Project)
	: QMainWindow(Parent), mProject(Project)
{
	setWindowTitle(tr("Instructions"));

	mInstructions = mProject->GetInstructions();

	mPropertiesWidget = new swInstructionsPropertiesWidget(this, mInstructions);
	mPropertiesWidget->setObjectName("InstructionsProperties");
	addDockWidget(Qt::RightDockWidgetArea, mPropertiesWidget);

	mPlacementWidget = new swInstructionsPlacementWidget(this, mInstructions);
	mPlacementWidget->setObjectName("InstructionsPlacement");
	addDockWidget(Qt::RightDockWidgetArea, mPlacementWidget);
	splitDockWidget(mPropertiesWidget, mPlacementWidget, Qt::Vertical);

	mPageWidget = new swInstructionsPageWidget(this, mInstructions, mPropertiesWidget, mPlacementWidget);
	setCentralWidget(mPageWidget);

	mPageListWidget = new swInstructionsPageListWidget(this, mInstructions);
	mPageListWidget->setObjectName("InstructionsPageList");
	addDockWidget(Qt::LeftDockWidgetArea, mPageListWidget);

	mPageSettingsToolBar = addToolBar(tr("Page Settings"));
	mPageSettingsToolBar->setObjectName("PageSettings");
	mPageSettingsToolBar->setFloatable(false);
	mPageSettingsToolBar->setMovable(false);

	mPrintAction = mPageSettingsToolBar->addAction(tr("Print..."));
	mPrintAction->setToolTip(tr("Print the instruction pages"));

	connect(mPageListWidget->mThumbnailsWidget, &QListWidget::currentRowChanged, this, &swInstructionsDialog::CurrentThumbnailChanged);
	UpdatePageList();

	connect(mPrintAction, &QAction::triggered, this, &swInstructionsDialog::ShowPrintDialog);
	connect(mInstructions, &swInstructions::PageSetupChanged, this, &swInstructionsDialog::PageSetupChanged);
	connect(mInstructions, &swInstructions::PageLayoutChanged, this, &swInstructionsDialog::PageSetupChanged);
}

#ifndef QT_NO_PRINTER
static void swSetInstructionsPageLayout(swInstructions* Instructions, QPrinter* Printer)
{
	// Print on paper matching the page setup, the pages already include their margins.
	const swInstructionsPageSetup& PageSetup = Instructions->mPageSetup;
	const QSizeF PageSize(qMin(PageSetup.Width, PageSetup.Height), qMax(PageSetup.Width, PageSetup.Height));
	const QPageLayout::Orientation Orientation = PageSetup.Width > PageSetup.Height ? QPageLayout::Landscape : QPageLayout::Portrait;

	Printer->setPageLayout(QPageLayout(QPageSize(PageSize, QPageSize::Point), Orientation, QMarginsF()));
	Printer->setFullPage(true);
}
#endif

void swSetupInstructionsPrinter(swInstructions* Instructions, QPrinter* Printer)
{
#ifndef QT_NO_PRINTER
	swSetInstructionsPageLayout(Instructions, Printer);
	Printer->setFromTo(1, static_cast<int>(Instructions->mPages.size()));
#endif
}

void swPrintInstructions(swInstructions* Instructions, QPrinter* Printer)
{
#ifndef QT_NO_PRINTER
	int DocCopies;
	int PageCopies;

	const int PageCount = static_cast<int>(Instructions->mPages.size());

	// The print dialog resets the paper when printing to a file, a file always gets the page setup paper.
	if (Printer->outputFormat() == QPrinter::PdfFormat)
		swSetInstructionsPageLayout(Instructions, Printer);

	if (Printer->collateCopies())
	{
		DocCopies = 1;
		PageCopies = Printer->supportsMultipleCopies() ? 1 : Printer->copyCount();
	}
	else
	{
		DocCopies = Printer->supportsMultipleCopies() ? 1 : Printer->copyCount();
		PageCopies = 1;
	}

	int FromPage = Printer->fromPage();
	int ToPage = Printer->toPage();
	bool Ascending = true;

	if (FromPage == 0 && ToPage == 0)
	{
		FromPage = 1;
		ToPage = PageCount;
	}

	FromPage = qMax(1, FromPage);
	ToPage = qMin(PageCount, ToPage);

	if (ToPage < FromPage)
		return;

	if (Printer->pageOrder() == QPrinter::LastPageFirst)
	{
		std::swap(FromPage, ToPage);
		Ascending = false;
	}

	// Pages keep their proportions on any paper, centered on a sheet filled with the page background.
	const swInstructionsPageSetup& PageSetup = Instructions->mPageSetup;
	const QRectF SheetRect(0.0, 0.0, Printer->width(), Printer->height());
	const QSizeF PageSize = QSizeF(PageSetup.Width, PageSetup.Height).scaled(SheetRect.size(), Qt::KeepAspectRatio);
	const QRectF PageRect(QPointF((SheetRect.width() - PageSize.width()) / 2.0, (SheetRect.height() - PageSize.height()) / 2.0), PageSize);
	const QColor BackgroundColor = Instructions->GetColorProperty(swInstructionsPropertyType::PageBackgroundColor, nullptr, 0);

	const int SheetCount = DocCopies * PageCopies * (qAbs(ToPage - FromPage) + 1);
	int SheetsPrinted = 0;
	swBlockingProgress Progress(QCoreApplication::translate("swInstructionsDialog", "Printing"), QCoreApplication::translate("swInstructionsDialog", "Printing pages..."), true);

	QGraphicsScene Scene;
	QPainter Painter(Printer);
	bool FirstPage = true;

	for (int DocCopy = 0; DocCopy < DocCopies; DocCopy++)
	{
		int Page = FromPage;

		for (;;)
		{
			for (int PageCopy = 0; PageCopy < PageCopies; PageCopy++)
			{
				if (Printer->printerState() == QPrinter::Aborted || Printer->printerState() == QPrinter::Error)
					return;

				Progress.SetProgress(SheetsPrinted++, SheetCount);

				if (Progress.IsCanceled())
				{
					Printer->abort();
					Painter.end();

					// A canceled file would otherwise be left with only the pages printed so far.
					if (Printer->outputFormat() == QPrinter::PdfFormat)
						QFile::remove(Printer->outputFileName());

					return;
				}

				if (!FirstPage)
					Printer->newPage();
				else
					FirstPage = false;

				swUpdateInstructionsPageScene(Instructions, &Instructions->mPages[Page - 1], &Scene);

				Painter.fillRect(SheetRect, BackgroundColor);
				Scene.render(&Painter, PageRect, Scene.sceneRect(), Qt::KeepAspectRatio);
			}

			if (Page == ToPage)
				break;

			if (Ascending)
				Page++;
			else
				Page--;
		}
	}
#endif
}

void swInstructionsDialog::ShowPrintDialog()
{
#ifndef QT_NO_PRINTER
	QPrinter Printer(QPrinter::HighResolution);
	swSetupInstructionsPrinter(mInstructions, &Printer);

	QPrintDialog PrintDialog(&Printer, this);

	if (PrintDialog.exec() == QDialog::Accepted)
		Print(&Printer);
#endif
}

void swInstructionsDialog::Print(QPrinter* Printer)
{
	swPrintInstructions(mInstructions, Printer);
}

void swInstructionsDialog::UpdatePageList()
{
	QListWidget* ThumbnailsWidget = mPageListWidget->mThumbnailsWidget;
	const int CurrentRow = qMax(ThumbnailsWidget->currentRow(), 0);

	// rebuild the page list and keep showing a page, the old pages no longer exist
	QSignalBlocker Blocker(ThumbnailsWidget);
	ThumbnailsWidget->clear();

	for (size_t PageNumber = 0; PageNumber < mInstructions->mPages.size(); PageNumber++)
	{
		QString Label = tr("Page %1").arg(swFormatIntegerLocalized(PageNumber + 1, locale()));

		switch (mInstructions->mPages[PageNumber].Type)
		{
		case swInstructionsPageType::Cover:
			Label = tr("%1 - Cover").arg(Label);
			break;

		case swInstructionsPageType::BillOfMaterials:
			Label = tr("%1 - Bill of Materials").arg(Label);
			break;

		case swInstructionsPageType::Blank:
			Label = tr("%1 - Blank").arg(Label);
			break;

		case swInstructionsPageType::Steps:
			break;
		}

		ThumbnailsWidget->addItem(Label);
	}

	const int Row = qMin(CurrentRow, ThumbnailsWidget->count() - 1);
	ThumbnailsWidget->setCurrentRow(Row);
	CurrentThumbnailChanged(Row);
}

void swInstructionsDialog::ModelChanged()
{
	// Several edits in a row rebuild the pages once, after the current event.
	if (mUpdatePending)
		return;

	mUpdatePending = true;

	QTimer::singleShot(0, this, [this]()
	{
		mUpdatePending = false;
		mInstructions->Update();
	});
}

void swInstructionsDialog::PageSetupChanged()
{
	UpdatePageList();
}

void swInstructionsDialog::CurrentThumbnailChanged(int Index)
{
	if (Index < 0 || Index >= static_cast<int>(mInstructions->mPages.size()))
	{
		mPageWidget->SetCurrentPage(nullptr);
		return;
	}

	const swInstructionsPage* Page = &mInstructions->mPages[Index];

	mPageWidget->SetCurrentPage(Page);
	mPropertiesWidget->SetCurrentPage(Index);
}
