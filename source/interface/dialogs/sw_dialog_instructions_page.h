#pragma once

#include "sw_instructions.h"

class swInstructionsPropertiesWidget;
class swInstructionsPlacementWidget;

// pointers leave the border of any boxed element, whatever its content
class swInstructionsBoxBase
{
public:
	virtual ~swInstructionsBoxBase() = default;

	// the outer edge of the border, inside the margin
	virtual QRectF GetBorderRect() const = 0;
};

// the page elements are drawn in a box, the background fills the margin, border and padding around the content
template <typename T>
class swInstructionsBoxItem : public T, public swInstructionsBoxBase
{
public:
	explicit swInstructionsBoxItem(QGraphicsItem* Parent)
		: T(Parent)
	{
	}

	void SetBoxStyle(const swInstructionsBoxStyle& BoxStyle)
	{
		T::prepareGeometryChange();
		mBoxStyle = BoxStyle;
	}

	const swInstructionsBoxStyle& GetBoxStyle() const
	{
		return mBoxStyle;
	}

	// use when the content fills its own rect with the background, the box then fills only around it
	void SetContentBackground(bool ContentBackground)
	{
		mContentBackground = ContentBackground;
	}

	QRectF GetBorderRect() const override
	{
		const QRectF BoxRect = boundingRect();

		return BoxRect.adjusted(mBoxStyle.Margin, mBoxStyle.Margin, -mBoxStyle.Margin, -mBoxStyle.Margin);
	}

	QRectF boundingRect() const override
	{
		const QRectF ContentRect = T::boundingRect();

		if (ContentRect.isEmpty())
			return ContentRect;

		const qreal Inset = mBoxStyle.GetInset();

		return ContentRect.adjusted(0.0, 0.0, 2.0 * Inset, 2.0 * Inset);
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
		const QRectF BoxRect = boundingRect();

		if (BoxRect.isEmpty())
			return;

		const qreal Inset = mBoxStyle.GetInset();

		if (mBoxStyle.BackgroundColor.alpha())
		{
			QPainterPath BackgroundPath;
			BackgroundPath.addRect(BoxRect);

			if (mContentBackground)
			{
				BackgroundPath.setFillRule(Qt::OddEvenFill);
				BackgroundPath.addRect(T::boundingRect().translated(Inset, Inset));
			}

			Painter->fillPath(BackgroundPath, mBoxStyle.BackgroundColor);
		}

		// the content is drawn inside the padding, the selection frames the whole box
		QStyleOptionGraphicsItem ContentOption(*Option);
		ContentOption.state &= ~(QStyle::State_Selected | QStyle::State_HasFocus);

		Painter->save();
		Painter->translate(Inset, Inset);
		T::paint(Painter, &ContentOption, Widget);
		Painter->restore();

		if (mBoxStyle.BorderWidth > 0.0 && mBoxStyle.BorderColor.alpha())
		{
			const qreal BorderInset = mBoxStyle.Margin + mBoxStyle.BorderWidth / 2.0;
			QPen Pen(mBoxStyle.BorderColor, mBoxStyle.BorderWidth);
			Pen.setJoinStyle(Qt::MiterJoin);

			Painter->save();
			Painter->setPen(Pen);
			Painter->setBrush(Qt::NoBrush);
			Painter->drawRect(BoxRect.adjusted(BorderInset, BorderInset, -BorderInset, -BorderInset));
			Painter->restore();
		}

		if (!(Option->state & QStyle::State_Selected))
			return;

		Painter->save();
		// the highlight stays visible on light pages with a dark interface theme
		QPen Pen(Option->palette.highlight(), 2.0, Qt::DashLine);
		Pen.setCosmetic(true);

		Painter->setPen(Pen);
		Painter->setBrush(Qt::NoBrush);
		Painter->drawRect(BoxRect);
		Painter->restore();
	}

protected:
	swInstructionsBoxStyle mBoxStyle;
	bool mContentBackground = false;
};

class swInstructionsStepImageItem : public swInstructionsBoxItem<QGraphicsPixmapItem>
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

	void SetCoverImage();

	bool IsCoverImage() const
	{
		return mCoverImage;
	}

	void Update();

	// use to find the part drawn at a point of the item and where a part is drawn, the parts are known by their index in the model
	bool FindPart(const QPointF& Point, int& PartIndex) const;
	bool GetPartPoint(int PartIndex, QPointF& Point) const;

protected:
	QVariant itemChange(GraphicsItemChange Change, const QVariant& Value) override;

	struct swPartRegion
	{
		int PartIndex;
		QRectF Rect;
		QPointF Center;
	};

	swInstructions* mInstructions = nullptr;
	swModel* mModel = nullptr;
	swStep mStep = 1;
	int mWidth = 1;
	int mHeight = 1;
	bool mCoverImage = false;
	std::vector<swPartRegion> mPartRegions;
};

class swInstructionsStepNumberItem : public swInstructionsBoxItem<QGraphicsSimpleTextItem>
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
class swInstructionsPageNumberItem : public swInstructionsBoxItem<QGraphicsSimpleTextItem>
{
public:
	swInstructionsPageNumberItem(QGraphicsItem* Parent, swInstructions* Instructions, size_t PageIndex);

protected:
	QVariant itemChange(GraphicsItemChange Change, const QVariant& Value) override;

	swInstructions* mInstructions = nullptr;
};

class swInstructionsPartsListItem : public swInstructionsBoxItem<QGraphicsPixmapItem>
{
public:
	swInstructionsPartsListItem(QGraphicsItem* Parent, swInstructions* Instructions, swModel* Model, swStep Step);

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

class swInstructionsCoverTextItem : public swInstructionsBoxItem<QGraphicsTextItem>
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

// a notice is free text written on a page, it is moved and locked like the other elements
class swInstructionsNoticeItem : public swInstructionsBoxItem<QGraphicsTextItem>
{
public:
	swInstructionsNoticeItem(QGraphicsItem* Parent, swInstructions* Instructions, const swInstructionsNotice& Notice);

	int GetId() const
	{
		return mId;
	}

protected:
	QVariant itemChange(GraphicsItemChange Change, const QVariant& Value) override;
	void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* Event) override;

	swInstructions* mInstructions = nullptr;
	int mId = 0;
};

// a pointer leaves an anchor of the border of its element, its tip follows the part or element it points at
class swInstructionsPointerItem : public QGraphicsItem
{
public:
	swInstructionsPointerItem(QGraphicsItem* Parent, swInstructions* Instructions, const swInstructionsPointer& Pointer, QGraphicsItem* Source, QGraphicsItem* Target);

	int GetId() const
	{
		return mPointer.Id;
	}

	void UpdateGeometry();

	QRectF boundingRect() const override;
	QPainterPath shape() const override;
	void paint(QPainter* Painter, const QStyleOptionGraphicsItem* Option, QWidget* Widget) override;

protected:
	void mousePressEvent(QGraphicsSceneMouseEvent* Event) override;
	void mouseMoveEvent(QGraphicsSceneMouseEvent* Event) override;
	void mouseReleaseEvent(QGraphicsSceneMouseEvent* Event) override;
	QPainterPath GetPath() const;

	swInstructions* mInstructions = nullptr;
	swInstructionsPointer mPointer;
	QGraphicsItem* mSource = nullptr;
	QGraphicsItem* mTarget = nullptr;
	swInstructionsPointerStyle mStyle = swInstructionsPointerStyle::Line;
	swInstructionsPointerTip mTip = swInstructionsPointerTip::Arrowhead;
	QColor mColor;
	qreal mWidth = 2.0;
	qreal mTipSize = 12.0;
	QPointF mBasePosition;
	QPointF mTipPosition;
	bool mDraggingTip = false;
};

class swInstructionsPageWidget : public QGraphicsView
{
	Q_OBJECT

public:
	swInstructionsPageWidget(QWidget* Parent, swInstructions* Instructions, swInstructionsPropertiesWidget* PropertiesWidget, swInstructionsPlacementWidget* PlacementWidget);

	void SetCurrentPage(const swInstructionsPage* Page);

protected:
	void SelectElement(const swInstructionsElementKey& Key);
	void resizeEvent(QResizeEvent* Event) override;
	void mousePressEvent(QMouseEvent* Event) override;
	void mouseReleaseEvent(QMouseEvent* Event) override;
	void mouseMoveEvent(QMouseEvent* Event) override;
	void contextMenuEvent(QContextMenuEvent* Event) override;
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
	std::optional<swInstructionsElementKey> mPendingSelection;
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

// use to get the anchors on the border of an element, clockwise from its top left corner
QPointF swGetPointerAnchor(const QRectF& BorderRect, int Anchor);

// use to edit the text of a notice, an empty text keeps the notice
bool swEditNoticeText(QWidget* Parent, QString& Text, bool NewNotice);

// use to fill a scene with the elements of a page
void swUpdateInstructionsPageScene(swInstructions* Instructions, const swInstructionsPage* Page, QGraphicsScene* Scene);
