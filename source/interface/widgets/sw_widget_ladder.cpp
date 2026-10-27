#include "sw_global.h"
#include "sw_widget_ladder.h"
#include "sw_object_property.h"
#include "sw_window_utilities.h"

swLadderWidget::swLadderWidget(QAbstractSpinBox* SpinBox, swFloatPropertySnap Snap)
	: QWidget(SpinBox, Qt::Popup | Qt::Sheet), mSpinBox(SpinBox)
{
	mSpinBox->installEventFilter(this);

	CalculateSteps(Snap);
}

void swLadderWidget::Show()
{
	int LineHeight = QFontMetrics(font()).height();
	int CellSize = LineHeight * 4;

	setFixedSize(CellSize, static_cast<int>(mSteps.size()) * CellSize);
	adjustSize();

	const QPoint CursorPosition = QCursor::pos();
	const QPoint Position = CursorPosition - QPoint(width() / 2, height() / 2);
	move(swGetPopupPosition(Position, size(), CursorPosition));

	mLastMousePositionX = mapFromGlobal(QCursor::pos()).x();
	UpdateMousePosition();

	show();
	grabMouse();
}

void swLadderWidget::CalculateSteps(swFloatPropertySnap Snap)
{
	QDoubleSpinBox* SpinBox = qobject_cast<QDoubleSpinBox*>(mSpinBox);

	switch (Snap)
	{
	case swFloatPropertySnap::Auto:
		if (SpinBox)
		{
			double Max = SpinBox->maximum();

			if (Max > 200.0)
				mSteps = { 1000.0, 100.0, 10.0, 1.0, 0.1 };
			else
				mSteps = { 30.0, 10.0, 1.0, 0.1, 0.01 };
		}
		break;

	case swFloatPropertySnap::PiecePositionXY:
		mSteps = { 80.0, 40.0, 24.0, 20.0, 10.0, 8.0, 1.0 };
		break;

	case swFloatPropertySnap::PiecePositionZ:
		mSteps = { 192.0, 96.0, 48.0, 24.0, 10.0, 8.0, 1.0 };
		break;

	case swFloatPropertySnap::Position:
		mSteps = { 1000.0, 100.0, 50.0, 10.0, 5.0, 1.0, 0.1 };
		break;

	case swFloatPropertySnap::Rotation:
		mSteps = { 45.0, 30.0, 22.5, 15.0, 5.0, 1.0, 0.1 };
		break;
	}

	if (mSteps.empty())
		mSteps = { 100.0, 10.0, 1.0, 0.1, 0.01 };
}

void swLadderWidget::UpdateMousePosition()
{
	QPoint MousePosition = mapFromGlobal(QCursor::pos());

	if (rect().contains(MousePosition))
	{
		int CellHeight = height() / static_cast<int>(mSteps.size());
		int CurrentStep = MousePosition.y() / CellHeight;

		if (CurrentStep != mCurrentStep)
		{
			mCurrentStep = CurrentStep;

			update();
		}
	}

	if (mCurrentStep != -1)
	{
		QDoubleSpinBox* SpinBox = qobject_cast<QDoubleSpinBox*>(mSpinBox);

		if (SpinBox)
		{
			const int Sensitivity = 15;
			int Steps = (MousePosition.x() - mLastMousePositionX) / Sensitivity;

			if (Steps)
			{
				mLastMousePositionX = MousePosition.x();

				SpinBox->setValue(SpinBox->value() + mSteps[mCurrentStep] * Steps);

				update();
			}
		}
	}
}

void swLadderWidget::CancelEditing()
{
	emit EditingCanceled();

	releaseMouse();
	deleteLater();
}

void swLadderWidget::FinishEditing()
{
	emit EditingFinished();

	releaseMouse();
	deleteLater();
}

void swLadderWidget::paintEvent(QPaintEvent* Event)
{
	Q_UNUSED(Event);

	QPainter Painter(this);

	int CellWidth = width();
	int CellHeight = height() / static_cast<int>(mSteps.size());

	Painter.fillRect(rect(), palette().brush(QPalette::Base));

	if (mCurrentStep != -1)
	{
		QRect Rect(0, mCurrentStep * CellHeight, CellWidth, CellHeight);

		Painter.fillRect(Rect, palette().brush(QPalette::Highlight));
	}

	Painter.setFont(font());
	Painter.setPen(palette().color(QPalette::Text));

	for (int Step = 0; Step < static_cast<int>(mSteps.size()); Step++)
	{
		QRect Rect(0, Step * CellHeight, CellWidth, CellHeight);
		QTextOption TextOption(Qt::AlignCenter);
		QString Text = swFormatDoubleLocalized(mSteps[Step], mSpinBox->locale(), 'g', 6);

		if (Step == mCurrentStep && mSpinBox)
			Text = QString("%1\n\n(%2)").arg(Text, mSpinBox->text());

		Painter.drawText(Rect, Text, TextOption);
	}

	Painter.setPen(palette().color(QPalette::Shadow));

	QRect Rect(rect());
	Rect.adjust(0, 0, -1, -1);

	Painter.drawRect(Rect);

	for (int Step = 1; Step < static_cast<int>(mSteps.size()); Step++)
		Painter.drawLine(0, Step * CellHeight, CellWidth, Step * CellHeight);
}

bool swLadderWidget::eventFilter(QObject* Object, QEvent* Event)
{
	if (Event->type() == QEvent::ShortcutOverride || Event->type() == QEvent::KeyPress)
	{
		QKeyEvent* KeyEvent = static_cast<QKeyEvent*>(Event);

		if (KeyEvent->key() == Qt::Key_Escape)
		{
			CancelEditing();

			KeyEvent->accept();

			return true;
		}
	}

	return QWidget::eventFilter(Object, Event);
}

void swLadderWidget::mousePressEvent(QMouseEvent* MouseEvent)
{
	CancelEditing();

	QWidget::mousePressEvent(MouseEvent);
}

void swLadderWidget::mouseMoveEvent(QMouseEvent* MouseEvent)
{
	UpdateMousePosition();

	QWidget::mouseMoveEvent(MouseEvent);
}

void swLadderWidget::mouseReleaseEvent(QMouseEvent* MouseEvent)
{
	FinishEditing();

	QWidget::mouseReleaseEvent(MouseEvent);
}
