#pragma once

#include "sw_instructions.h"

class swInstructionsPropertiesWidget;
class swInstructionsPlacementWidget;

// Snaps a border to device pixels.
inline void swSnapBorderRect(const QTransform& Transform, QRectF& Rect, qreal& BorderWidth)
{
	const qreal Scale = qAbs(Transform.m11());

	if (Transform.isRotating() || !Transform.isInvertible() || Scale <= 0.0)
		return;

	const QRectF DeviceRect = Transform.mapRect(Rect);
	const QRectF SnappedRect(QPointF(qRound(DeviceRect.left()), qRound(DeviceRect.top())), QPointF(qRound(DeviceRect.right()), qRound(DeviceRect.bottom())));

	Rect = Transform.inverted().mapRect(SnappedRect);

	if (BorderWidth > 0.0)
		BorderWidth = qMax<qreal>(qRound(BorderWidth * Scale), 1.0) / Scale;
}

// Pointers leave any boxed element.
class swInstructionsBoxBase
{
public:
	virtual ~swInstructionsBoxBase() = default;

	// The outer border edge.
	virtual QRectF GetBorderRect() const = 0;

	// The border width.
	virtual qreal GetBorderWidth() const = 0;
};

// The box around page elements.
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

	// The content fills its own background.
	void SetContentBackground(bool ContentBackground)
	{
		mContentBackground = ContentBackground;
	}

	QRectF GetBorderRect() const override
	{
		const QRectF BoxRect = boundingRect();

		return BoxRect.adjusted(mBoxStyle.Margin, mBoxStyle.Margin, -mBoxStyle.Margin, -mBoxStyle.Margin);
	}

	qreal GetBorderWidth() const override
	{
		return mBoxStyle.BorderColor.alpha() ? mBoxStyle.BorderWidth : 0.0;
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

		// Content inside the padding.
		QStyleOptionGraphicsItem ContentOption(*Option);
		ContentOption.state &= ~(QStyle::State_Selected | QStyle::State_HasFocus);

		Painter->save();
		Painter->translate(Inset, Inset);
		T::paint(Painter, &ContentOption, Widget);
		Painter->restore();

		if (mBoxStyle.BorderWidth > 0.0 && mBoxStyle.BorderColor.alpha())
		{
			// Equal border sides at any zoom.
			QRectF OuterRect = GetBorderRect();
			qreal BorderWidth = mBoxStyle.BorderWidth;
			swSnapBorderRect(Painter->worldTransform(), OuterRect, BorderWidth);

			const QRectF InnerRect = OuterRect.adjusted(BorderWidth, BorderWidth, -BorderWidth, -BorderWidth);
			QPainterPath BorderPath;
			BorderPath.setFillRule(Qt::OddEvenFill);
			BorderPath.addRect(OuterRect);

			if (InnerRect.isValid())
				BorderPath.addRect(InnerRect);

			Painter->save();
			Painter->setRenderHint(QPainter::Antialiasing, true);
			Painter->fillPath(BorderPath, mBoxStyle.BorderColor);
			Painter->restore();
		}

		if (!(Option->state & QStyle::State_Selected))
			return;

		Painter->save();

		// A highlight visible on light pages.
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

	// Finds the part drawn at a point.
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

// One page number item per page.
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

	void SetBillOfMaterials(int MaxWidth, int MaxHeight)
	{
		mBillOfMaterials = true;
		SetMaxSize(MaxWidth, MaxHeight);
	}

	void SetMaxSize(int MaxWidth, int MaxHeight)
	{
		mMaxWidth = MaxWidth;
		mMaxHeight = MaxHeight;
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
	int mMaxHeight = 300;
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

// A free text notice on a page.
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

// A pointer from an element anchor.
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
	QPainterPath GetPath(const QRectF& SourceRect, qreal BorderWidth) const;
	QPainterPath GetWedgePath(const QRectF& SourceRect, qreal BorderWidth) const;
	QPainterPath GetClipPath(const QRectF& SourceRect, qreal BorderWidth) const;

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
	QRectF mSourceRect;
	int mAnchor = 0;
	qreal mSourceBorderWidth = 0.0;
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
	void AlignSelection(int Placement, bool LastSelection);
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
	std::vector<swInstructionsElementKey> mSelectionOrder;
	bool mRebuildingPage = false;
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

// The position from the placement.
constexpr int SW_PLACEMENT_POSITION_DATA = 0;

// Identifies a page element.
bool swGetElementKey(const QGraphicsItem* Item, swInstructionsElementKey& Key);

// Gets the border anchors of an element.
QPointF swGetPointerAnchor(const QRectF& BorderRect, int Anchor);

// Edits the text of a notice.
bool swEditNoticeText(QWidget* Parent, QString& Text, bool NewNotice);

// Fills a scene with page elements.
void swUpdateInstructionsPageScene(swInstructions* Instructions, const swInstructionsPage* Page, QGraphicsScene* Scene);
