#pragma once

#include "sw_instructions.h"

class swInstructionsPropertiesWidget;
class swInstructionsPlacementWidget;

// the page elements keep a padding around their content so they never touch their area or the elements placed against them
constexpr qreal SW_ELEMENT_PADDING = 12.0;

template <typename T>
class swInstructionsPaddedItem : public T
{
public:
	explicit swInstructionsPaddedItem(QGraphicsItem* Parent)
		: T(Parent)
	{
	}

	QRectF boundingRect() const override
	{
		const QRectF ContentRect = T::boundingRect();

		if (ContentRect.isEmpty())
			return ContentRect;

		return ContentRect.adjusted(0.0, 0.0, 2.0 * SW_ELEMENT_PADDING, 2.0 * SW_ELEMENT_PADDING);
	}

	QPainterPath shape() const override
	{
		QPainterPath Path;
		Path.addRect(boundingRect());

		return Path;
	}

	bool contains(const QPointF& Point) const override
	{
		return boundingRect().contains(Point);
	}

	void paint(QPainter* Painter, const QStyleOptionGraphicsItem* Option, QWidget* Widget) override
	{
		// the content is drawn inside the padding, the selection frames the padded box
		QStyleOptionGraphicsItem ContentOption(*Option);
		ContentOption.state &= ~(QStyle::State_Selected | QStyle::State_HasFocus);

		Painter->save();
		Painter->translate(SW_ELEMENT_PADDING, SW_ELEMENT_PADDING);
		T::paint(Painter, &ContentOption, Widget);
		Painter->restore();

		if (!(Option->state & QStyle::State_Selected))
			return;

		Painter->save();
		// the highlight stays visible on light pages with a dark interface theme
		QPen Pen(Option->palette.highlight(), 2.0, Qt::DashLine);
		Pen.setCosmetic(true);

		Painter->setPen(Pen);
		Painter->setBrush(Qt::NoBrush);
		Painter->drawRect(boundingRect());
		Painter->restore();
	}
};

class swInstructionsStepImageItem : public swInstructionsPaddedItem<QGraphicsPixmapItem>
{
public:
	swInstructionsStepImageItem(QGraphicsItem* Parent, swInstructions* Instructions, swModel* Model, swStep Step);

	swModel* GetModel() const
	{
		return mModel;
	}

	swStep GetStep() const
	{
		return mStep;
	}

	void SetImageSize(int Width, int Height)
	{
		mWidth = Width;
		mHeight = Height;
	}

	void SetCoverImage()
	{
		mCoverImage = true;
	}

	bool IsCoverImage() const
	{
		return mCoverImage;
	}

	void Update();

protected:
	QVariant itemChange(GraphicsItemChange Change, const QVariant& Value) override;

	swInstructions* mInstructions = nullptr;
	swModel* mModel = nullptr;
	swStep mStep = 1;
	int mWidth = 1;
	int mHeight = 1;
	bool mCoverImage = false;
};

class swInstructionsStepNumberItem : public swInstructionsPaddedItem<QGraphicsSimpleTextItem>
{
public:
	swInstructionsStepNumberItem(QGraphicsItem* Parent, swInstructions* Instructions, swModel* Model, swStep Step);

	swModel* GetModel() const
	{
		return mModel;
	}

	swStep GetStep() const
	{
		return mStep;
	}

	void Update();

protected:
	QVariant itemChange(GraphicsItemChange Change, const QVariant& Value) override;

	swInstructions* mInstructions = nullptr;
	swModel* mModel = nullptr;
	swStep mStep = 1;
};

// one page number item per page, they share their placement and offset so all pages number alike
class swInstructionsPageNumberItem : public swInstructionsPaddedItem<QGraphicsSimpleTextItem>
{
public:
	swInstructionsPageNumberItem(QGraphicsItem* Parent, swInstructions* Instructions, size_t PageIndex);

protected:
	QVariant itemChange(GraphicsItemChange Change, const QVariant& Value) override;

	swInstructions* mInstructions = nullptr;
};

class swInstructionsPiecesListItem : public swInstructionsPaddedItem<QGraphicsPixmapItem>
{
public:
	swInstructionsPiecesListItem(QGraphicsItem* Parent, swInstructions* Instructions, swModel* Model, swStep Step);

	swModel* GetModel() const
	{
		return mModel;
	}

	swStep GetStep() const
	{
		return mStep;
	}

	void SetBillOfMaterials(int MaxWidth)
	{
		mBillOfMaterials = true;
		mMaxWidth = MaxWidth;
	}

	void SetMaxWidth(int MaxWidth)
	{
		mMaxWidth = MaxWidth;
	}

	bool IsBillOfMaterials() const
	{
		return mBillOfMaterials;
	}

	void Update();

protected:
	QVariant itemChange(GraphicsItemChange Change, const QVariant& Value) override;

	swInstructions* mInstructions = nullptr;
	swModel* mModel = nullptr;
	swStep mStep = 1;
	bool mBillOfMaterials = false;
	int mMaxWidth = 300;
};

class swInstructionsCoverTextItem : public swInstructionsPaddedItem<QGraphicsTextItem>
{
public:
	swInstructionsCoverTextItem(QGraphicsItem* Parent, swInstructions* Instructions, swModel* Model, bool Author);

	swModel* GetModel() const
	{
		return mModel;
	}

	bool IsAuthor() const
	{
		return mAuthor;
	}

protected:
	QVariant itemChange(GraphicsItemChange Change, const QVariant& Value) override;

	swInstructions* mInstructions = nullptr;
	swModel* mModel = nullptr;
	bool mAuthor = false;
};

class swInstructionsPageWidget : public QGraphicsView
{
	Q_OBJECT

public:
	swInstructionsPageWidget(QWidget* Parent, swInstructions* Instructions, swInstructionsPropertiesWidget* PropertiesWidget, swInstructionsPlacementWidget* PlacementWidget);

	void SetCurrentPage(const swInstructionsPage* Page);

protected:
	void resizeEvent(QResizeEvent* Event) override;
	void mousePressEvent(QMouseEvent* Event) override;
	void mouseReleaseEvent(QMouseEvent* Event) override;
	void drawForeground(QPainter* Painter, const QRectF& Rect) override;
	void FitPage();

protected slots:
	void StepSettingsChanged(swModel* Model, swStep Step);
	void SelectionChanged();

protected:
	swInstructions* mInstructions;
	swInstructionsPropertiesWidget* mPropertiesWidget;
	swInstructionsPlacementWidget* mPlacementWidget;
	const swInstructionsPage* mPage = nullptr;
	std::vector<std::pair<QGraphicsItem*, QPointF>> mDragItems;
};

class swInstructionsPageListWidget : public QDockWidget
{
	Q_OBJECT

public:
	swInstructionsPageListWidget(QWidget* Parent, swInstructions* Instructions);

	QListWidget* mThumbnailsWidget = nullptr;

protected:
	void CustomMenuRequested(const QPoint& Position);

	swInstructions* mInstructions;
};

// the position an element gets from its placement, before it is moved or locked
constexpr int SW_PLACEMENT_POSITION_DATA = 0;

// use to identify a page element across page rebuilds, which delete the items
bool swGetElementKey(const QGraphicsItem* Item, swInstructionsElementKey& Key);

// use to fill a scene with the elements of a page
void swUpdateInstructionsPageScene(swInstructions* Instructions, const swInstructionsPage* Page, QGraphicsScene* Scene);
