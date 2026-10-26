#include "sw_global.h"
#include "sw_colorlist.h"
#include "sw_application.h"
#include "sw_library.h"
#include "sw_colors.h"
#include "sw_collapsiblewidget.h"
#include "sw_utils.h"

swColorList::swColorList(QWidget* Parent, bool AllowNoColor)
	: QWidget(Parent), mAllowNoColor(AllowNoColor)
{
	setFocusPolicy(Qt::StrongFocus);

	UpdateCells();

	connect(swGetPiecesLibrary(), &swPiecesLibrary::ColorsLoaded, this, &swColorList::ColorsLoaded);
}

void swColorList::UpdateCells()
{
	for (swColorListGroup& Group : mGroups)
		if (Group.Button)
			Group.Button->deleteLater();

	mCells.clear();
	mGroups.clear();

	for (int GroupIdx = 0; GroupIdx < SW_NUM_COLORGROUPS; GroupIdx++)
	{
		swColorGroup* Group = &gColorGroups[GroupIdx];
		swColorListGroup ListGroup;

		for (int ColorIndex : Group->Colors)
		{
			mCells.emplace_back(swColorListCell{ QRect(), ColorIndex });
			ListGroup.Name = Group->Name;
			ListGroup.Cells.emplace_back(mCells.size());
		}

		// Group headers use the same collapsible buttons as the properties panel.
		ListGroup.Button = new swCollapsibleWidgetButton(Group->Name, this);
		ListGroup.Button->setToolTip(tr("Expand or collapse the %1 colors.").arg(Group->Name));

		connect(ListGroup.Button, &swCollapsibleWidgetButton::StateChanged, this, [this, GroupIdx](bool Expanded)
		{
			SetGroupCollapsed(GroupIdx, !Expanded);
		});

		mGroups.emplace_back(std::move(ListGroup));
	}

	if (mAllowNoColor)
	{
		mCells.emplace_back(swColorListCell{ QRect(), swGetColorIndex(SW_COLOR_NOCOLOR) });
		mGroups[SW_COLORGROUP_SPECIAL].Cells.emplace_back(mCells.size());
	}

	// Before the first layout the width is only a placeholder, so estimate with the hinted width.
	mPreferredHeight = GetHeightForWidth(isVisible() ? width() : sizeHint().width(), &mColumns, &mRows);
}

int swColorList::GetHeightForWidth(int Width, int* Columns, int* Rows) const
{
	QFontMetrics Metrics(font());
	int TextHeight = 0;

	for (size_t GroupIdx = 0; GroupIdx < mGroups.size(); GroupIdx++)
		TextHeight += GetGroupHeaderHeight(mGroups[GroupIdx], GroupIdx == 0);

	// Tiles are always square: the width sets the tile size and the rows set the height.
	const int TargetCellSize = Metrics.height() * 3 / 2;
	const int ColumnCount = qMax(5, Width / qMax(TargetCellSize, 1));
	int RowCount = 0;

	for (const swColorListGroup& Group : mGroups)
		if (!Group.Collapsed)
			RowCount += ((int)Group.Cells.size() + ColumnCount - 1) / ColumnCount;

	if (Columns)
		*Columns = ColumnCount;

	if (Rows)
		*Rows = RowCount;

	return TextHeight + qCeil((float)Width / (float)ColumnCount * RowCount);
}

int swColorList::GetGroupHeaderHeight(const swColorListGroup& Group, bool FirstGroup) const
{
	// Headers are spaced like the rows of a standard layout, above unless first and below unless collapsed.
	const int Spacing = swGetLayoutSpacing(this, Qt::Vertical);
	const int ButtonHeight = Group.Button ? Group.Button->sizeHint().height() : QFontMetrics(font()).height();

	return (FirstGroup ? 0 : Spacing) + ButtonHeight + (Group.Collapsed ? 0 : Spacing);
}

void swColorList::UpdateRects()
{
	mPreferredHeight = GetHeightForWidth(width(), &mColumns, &mRows);

	const float CellWidth = (float)width() / (float)mColumns;
	const float CellHeight = CellWidth;

	if (height() != mPreferredHeight)
	{
		setFixedHeight(mPreferredHeight);
		emit HeightChanged();
	}

	int CurCell = 0;
	float GroupY = 0.0f;

	for (size_t GroupIdx = 0; GroupIdx < mGroups.size(); GroupIdx++)
	{
		swColorListGroup& Group = mGroups[GroupIdx];
		int CurColumn = 0;
		int NumRows = 0;

		Group.Rect = QRect(0, (int)GroupY, width(), GetGroupHeaderHeight(Group, GroupIdx == 0));
		GroupY += Group.Rect.height();

		if (Group.Button)
		{
			const int Top = Group.Rect.top() + (GroupIdx == 0 ? 0 : swGetLayoutSpacing(this, Qt::Vertical));
			Group.Button->setGeometry(QRect(0, Top, width(), Group.Button->sizeHint().height()));
			Group.Button->show();
		}

		// Collapsed groups keep their header but hide their tiles.
		if (Group.Collapsed)
		{
			for (size_t ColorIdx = 0; ColorIdx < Group.Cells.size(); ColorIdx++)
				mCells[CurCell++].Rect = QRect();

			continue;
		}

		for (size_t ColorIdx = 0; ColorIdx < Group.Cells.size(); ColorIdx++)
		{
			const int Left = CellWidth * CurColumn;
			const int Right = (CurColumn + 1 == mColumns) ? width() - 1 : (CurColumn + 1) * CellWidth;
			const int Top = GroupY + CellHeight * NumRows;
			const int Bottom = GroupY + CellHeight * (NumRows + 1);

			mCells[CurCell].Rect = QRect(Left, Top, Right - Left, Bottom - Top);

			CurColumn++;
			if (CurColumn == mColumns)
			{
				CurColumn = 0;
				NumRows++;
			}

			CurCell++;
		}

		if (CurColumn != 0)
			NumRows++;

		GroupY += NumRows * CellHeight;
	}
}

void swColorList::ToggleGroupCollapsed(int GroupIndex)
{
	if (GroupIndex < 0 || GroupIndex >= static_cast<int>(mGroups.size()))
		return;

	SetGroupCollapsed(GroupIndex, !mGroups[GroupIndex].Collapsed);
}

void swColorList::SetGroupCollapsed(int GroupIndex, bool Collapsed)
{
	swColorListGroup& Group = mGroups[GroupIndex];

	Group.Collapsed = Collapsed;

	if (Group.Button)
		Group.Button->SetExpanded(!Collapsed);

	UpdateRects();
	update();
}

void swColorList::ColorsLoaded()
{
	UpdateCells();
	UpdateRects();

	SetCurrentColor(swGetColorIndex(mColorCode));

	update();
}

QSize swColorList::sizeHint() const
{
	return QSize(200, mPreferredHeight);
}

void swColorList::SetCurrentColor(int ColorIndex)
{
	for (size_t CellIndex = 0; CellIndex < mCells.size(); CellIndex++)
	{
		if (mCells[CellIndex].ColorIndex == ColorIndex)
		{
			SelectCell(CellIndex);
			break;
		}
	}
}

bool swColorList::event(QEvent* Event)
{
	if (Event->type() == QEvent::ToolTip)
	{
		QHelpEvent* HelpEvent = static_cast<QHelpEvent*>(Event);

		for (size_t CellIndex = 0; CellIndex < mCells.size(); CellIndex++)
		{
			if (!mCells[CellIndex].Rect.contains(HelpEvent->pos()))
				continue;

			QString Text = swGetColorToolTip(mCells[CellIndex].ColorIndex);
			QToolTip::showText(HelpEvent->globalPos(), Text);

			return true;
		}

		QToolTip::hideText();
		Event->ignore();

		return true;
	}
	else if (Event->type() == QEvent::ShortcutOverride)
	{
		QKeyEvent* KeyEvent = static_cast<QKeyEvent*>(Event);

		if (KeyEvent->modifiers() == Qt::NoModifier || KeyEvent->modifiers() == Qt::KeypadModifier)
		{
			switch (KeyEvent->key())
			{
			case Qt::Key_Left:
			case Qt::Key_Right:
			case Qt::Key_Up:
			case Qt::Key_Down:
				KeyEvent->accept();
				break;

			default:
				break;
			}
		}
	}

	return QWidget::event(Event);
}

void swColorList::mousePressEvent(QMouseEvent* MouseEvent)
{
	for (size_t CellIndex = 0; CellIndex < mCells.size(); CellIndex++)
	{
		if (!mCells[CellIndex].Rect.contains(MouseEvent->pos()))
			continue;

		SelectCell(CellIndex);
		emit ColorSelected(mCells[CellIndex].ColorIndex);

		break;
	}

	mDragStartPosition = MouseEvent->pos();
}

void swColorList::mouseMoveEvent(QMouseEvent* MouseEvent)
{
	if (!(MouseEvent->buttons() & Qt::LeftButton))
		return;

	if ((MouseEvent->pos() - mDragStartPosition).manhattanLength() < QApplication::startDragDistance())
		return;

	QMimeData* MimeData = new QMimeData;
	MimeData->setData("application/vnd.leocad-color", QString::number(mCells[mCurrentCell].ColorIndex).toLatin1());

	QDrag* Drag = new QDrag(this);
	Drag->setMimeData(MimeData);

	Drag->exec(Qt::CopyAction);
}

void swColorList::keyPressEvent(QKeyEvent *event)
{
	size_t NewCell = mCurrentCell;

	if (event->key() == Qt::Key_Left)
	{
		if (mCurrentCell > 0)
			NewCell = mCurrentCell - 1;
	}
	else if (event->key() == Qt::Key_Right)
	{
		if (mCurrentCell < mCells.size() - 1)
			NewCell = mCurrentCell + 1;
	}
	else if (event->key() == Qt::Key_Up || event->key() == Qt::Key_Down)
	{
		if (mCurrentCell >= mCells.size())
			mCurrentCell = 0;

		size_t CurGroup = 0;
		size_t NumCells = 0;

		for (CurGroup = 0; CurGroup < mGroups.size(); CurGroup++)
		{
			int NumColors = (int)mGroups[CurGroup].Cells.size();

			if (mCurrentCell < NumCells + NumColors)
				break;

			NumCells += NumColors;
		}

		size_t Row = (mCurrentCell - NumCells) / mColumns;
		size_t Column = (mCurrentCell - NumCells) % mColumns;

		if (event->key() == Qt::Key_Up)
		{
			if (Row > 0)
				NewCell = mCurrentCell - mColumns;
			else if (CurGroup > 0)
			{
				size_t NumColors = mGroups[CurGroup - 1].Cells.size();
				size_t NumColumns = NumColors % mColumns;

				if (NumColumns < Column + 1)
					NewCell = mCurrentCell - NumColumns - mColumns;
				else
					NewCell = mCurrentCell - NumColumns;
			}
		}
		else if (event->key() == Qt::Key_Down)
		{
			int NumColors = (int)mGroups[CurGroup].Cells.size();

			if (mCurrentCell + mColumns < NumCells + NumColors)
				NewCell = mCurrentCell + mColumns;
			else
			{
				size_t NumColumns = NumColors % mColumns;

				if (NumColumns > Column)
				{
					if (mCurrentCell + NumColumns < mCells.size())
						NewCell = mCurrentCell + NumColumns;
				}
				else
					NewCell = mCurrentCell + mColumns + NumColumns;
			}
		}
	}
	else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
	{
		emit ColorSelected(mCells[mCurrentCell].ColorIndex);
	}

	if (NewCell != mCurrentCell)
		SelectCell(NewCell);
	else
		QWidget::keyPressEvent(event);
}

void swColorList::resizeEvent(QResizeEvent* Event)
{
	if (mWidth == width() && mHeight == height())
		return;

	UpdateRects();

	mWidth = width();
	mHeight = height();

	QWidget::resizeEvent(Event);
}

void swColorList::paintEvent(QPaintEvent* Event)
{
	Q_UNUSED(Event);

	QPainter Painter(this);

	Painter.fillRect(rect(), palette().brush(QPalette::Window));

	Painter.setPen(palette().color(QPalette::Shadow));

	for (size_t CellIndex = 0; CellIndex < mCells.size(); CellIndex++)
	{
		const swColor* Color = &gColorList[mCells[CellIndex].ColorIndex];

		const QRect& Rect = mCells[CellIndex].Rect;

		if (Rect.isNull())
			continue;

		if (Color->Code != SW_COLOR_NOCOLOR)
		{
			QColor CellColor(Color->Value[0] * 255, Color->Value[1] * 255, Color->Value[2] * 255);

			Painter.setBrush(CellColor);
			Painter.drawRect(Rect);
		}
		else
			swDrawNoColorRect(Painter, Rect);
	}

	if (mCurrentCell < mCells.size() && !mCells[mCurrentCell].Rect.isNull())
	{
		const swColor* Color = &gColorList[mCells[mCurrentCell].ColorIndex];
		QColor EdgeColor(255 - Color->Value[0] * 255, 255 - Color->Value[1] * 255, 255 - Color->Value[2] * 255);

		Painter.setPen(EdgeColor);
		Painter.setBrush(Qt::NoBrush);

		QRect CellRect = mCells[mCurrentCell].Rect;
		CellRect.adjust(1, 1, -1, -1);
		Painter.drawRect(CellRect);
	}
}

void swColorList::SelectCell(size_t CellIndex)
{
	if (CellIndex >= mCells.size())
		return;

	if (CellIndex == mCurrentCell)
		return;

	mCurrentCell = CellIndex;
	mColorCode = swGetColorCode(mCells[CellIndex].ColorIndex);

	emit ColorChanged(mCells[mCurrentCell].ColorIndex);
	update();
}
