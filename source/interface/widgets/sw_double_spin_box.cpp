#include "sw_global.h"
#include "sw_double_spin_box.h"
#include "sw_ladder_widget.h"
#include "sw_utilities.h"
#include "sw_main_window.h"

swDoubleSpinBox::swDoubleSpinBox(QWidget* Parent)
	: QDoubleSpinBox(Parent)
{
	lineEdit()->installEventFilter(this);

	connect(this, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &swDoubleSpinBox::ValueChanged);
	connect(lineEdit(), &QLineEdit::returnPressed, this, &swDoubleSpinBox::ReturnPressed);
}

void swDoubleSpinBox::SetValue(double Value)
{
	mModified = false;

	setValue(Value);
}

void swDoubleSpinBox::SetSnap(swFloatPropertySnap Snap)
{
	mSnap = Snap;

	switch (Snap)
	{
	case swFloatPropertySnap::Auto:
		setSingleStep(10.0);
		break;

	case swFloatPropertySnap::PiecePositionXY:
		setSingleStep(gMainWindow->GetMoveXYSnap());
		break;

	case swFloatPropertySnap::PiecePositionZ:
		setSingleStep(gMainWindow->GetMoveZSnap());
		break;

	case swFloatPropertySnap::Position:
		setSingleStep(10.0);
		break;

	case swFloatPropertySnap::Rotation:
		setSingleStep(gMainWindow->GetAngleSnap());
		break;
	}
}

void swDoubleSpinBox::ShowLadderWidget()
{
	swLadderWidget* LadderWidget = new swLadderWidget(this, mSnap);

	mLadderWidgetInitialValue = value();

	connect(LadderWidget, &swLadderWidget::EditingCanceled, this, &swDoubleSpinBox::LadderWidgetCanceled);

	LadderWidget->Show();
}

void swDoubleSpinBox::LadderWidgetCanceled()
{
	setValue(mLadderWidgetInitialValue);
}

QString	swDoubleSpinBox::textFromValue(double Value) const
{
	return swFormatFloatLocalized(Value, locale());
}

void swDoubleSpinBox::ValueChanged()
{
	mModified = true;
}

void swDoubleSpinBox::CancelEditing()
{
	if (!mEditing)
		return;

	mEditing = false;
	mModified = false;
	emit EditingCanceled();
}

void swDoubleSpinBox::FinishEditing()
{
	if (!mEditing)
		return;

	mEditing = false;
	mModified = false;
	emit EditingFinished();
}

void swDoubleSpinBox::ReturnPressed()
{
	FinishEditing();
	emit ReturnKeyPressed();
}

bool swDoubleSpinBox::eventFilter(QObject* Object, QEvent* Event)
{
	if (mDragMode == DragMode::None && Event->type() == QEvent::MouseButtonPress && Object == lineEdit())
	{
		QMouseEvent* MouseEvent = reinterpret_cast<QMouseEvent*>(Event);

		if (MouseEvent->buttons() == Qt::MiddleButton)
			ShowLadderWidget();
	}

	return QDoubleSpinBox::eventFilter(Object, Event);
}

void swDoubleSpinBox::mousePressEvent(QMouseEvent* MouseEvent)
{
	if (mDragMode == DragMode::Dragging)
	{
		if (MouseEvent->buttons() != Qt::LeftButton)
		{
			CancelEditing();

			mDragMode = DragMode::None;
		}
	}
	else if (mDragMode == DragMode::None)
	{
		if (MouseEvent->buttons() == Qt::LeftButton)
		{
			mDragMode = DragMode::Start;
			mLastPosition = GetGlobalMousePosition(MouseEvent);
		}
		else if (MouseEvent->buttons() == Qt::MiddleButton)
		{
			ShowLadderWidget();
		}
	}
	else
	{
		mDragMode = DragMode::None;
	}

	QDoubleSpinBox::mousePressEvent(MouseEvent);
}

void swDoubleSpinBox::mouseMoveEvent(QMouseEvent* MouseEvent)
{
	if (mDragMode == DragMode::Dragging)
	{
		QPoint Position = QCursor::pos();

		stepBy(mLastPosition.y() - Position.y());

		QScreen* Screen = QGuiApplication::screenAt(Position);

		if (Screen)
		{
			int ScreenHeight = Screen->geometry().height();

			if (Position.y() <= 0)
			{
				Position.setY(ScreenHeight - 2);

				QCursor::setPos(Position);
			}
			else if (Position.y() >= ScreenHeight - 1)
			{
				Position.setY(1);

				QCursor::setPos(Position);
			}
		}

		mLastPosition = Position;
	}
	else if (mDragMode == DragMode::Start)
	{
		QPoint Position = GetGlobalMousePosition(MouseEvent);

		if (abs(mLastPosition.y() - Position.y()) > 3)
		{
			mDragMode = DragMode::Dragging;
			mLastPosition = QCursor::pos();
		}
	}

	QDoubleSpinBox::mouseMoveEvent(MouseEvent);
}

void swDoubleSpinBox::mouseReleaseEvent(QMouseEvent* MouseEvent)
{
	mDragMode = DragMode::None;

	QDoubleSpinBox::mouseReleaseEvent(MouseEvent);
}

bool swDoubleSpinBox::event(QEvent* Event)
{
	if (Event->type() == QEvent::KeyPress)
	{
		QKeyEvent* KeyEvent = static_cast<QKeyEvent*>(Event);

		if (KeyEvent->key() == Qt::Key_Escape)
		{
			CancelEditing();

			mDragMode = DragMode::None;

			clearFocus();
			KeyEvent->accept();

			return true;
		}
		else if (KeyEvent->key() == Qt::Key_Return || KeyEvent->key() == Qt::Key_Enter)
		{
			FinishEditing();

			KeyEvent->accept();

			return true;
		}
	}
	else if (Event->type() == QEvent::ShortcutOverride)
	{
		QKeyEvent* KeyEvent = static_cast<QKeyEvent*>(Event);

		switch (KeyEvent->key())
		{
		case Qt::Key_Escape:
		case Qt::Key_Up:
		case Qt::Key_Down:
		case Qt::Key_PageUp:
		case Qt::Key_PageDown:
			Event->accept();
			return true;

		default:
			break;
		}
	}

	return QDoubleSpinBox::event(Event);
}

void swDoubleSpinBox::focusInEvent(QFocusEvent* FocusEvent)
{
	mModified = false;
	mEditing = true;

	QDoubleSpinBox::focusInEvent(FocusEvent);

	emit EditingStarted();
}

void swDoubleSpinBox::focusOutEvent(QFocusEvent* FocusEvent)
{
	FinishEditing();

	QDoubleSpinBox::focusOutEvent(FocusEvent);
}
