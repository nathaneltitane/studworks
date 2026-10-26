#pragma once

#include "sw_object_property.h"

class swDoubleSpinBox : public QDoubleSpinBox
{
	Q_OBJECT

public:
	swDoubleSpinBox(QWidget* Parent);
	virtual ~swDoubleSpinBox() = default;

	void SetValue(double Value);
	void SetSnap(swFloatPropertySnap Snap);

	QString	textFromValue(double Value) const override;
	bool eventFilter(QObject* Object, QEvent* Event) override;

	void mousePressEvent(QMouseEvent* MouseEvent) override;
	void mouseMoveEvent(QMouseEvent* MouseEvent) override;
	void mouseReleaseEvent(QMouseEvent* MouseEvent) override;
	void focusInEvent(QFocusEvent* FocusEvent) override;
	void focusOutEvent(QFocusEvent* FocusEvent) override;

signals:
	void EditingStarted();
	void EditingCanceled();
	void EditingFinished();
	void ReturnKeyPressed();

protected slots:
	void ValueChanged();
	void CancelEditing();
	void FinishEditing();
	void ReturnPressed();
	void LadderWidgetCanceled();

protected:
	bool event(QEvent* Event) override;

	void ShowLadderWidget();

	static QPoint GetGlobalMousePosition(QMouseEvent* MouseEvent)
	{
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
		return MouseEvent->globalPos();
#else
		return MouseEvent->globalPosition().toPoint();
#endif
	}

	enum class DragMode
	{
		None,
		Start,
		Dragging
	};

	bool mModified = false;
	bool mEditing = false;
	double mLadderWidgetInitialValue = 0.0;
	QPoint mLastPosition;
	DragMode mDragMode = DragMode::None;
	swFloatPropertySnap mSnap = swFloatPropertySnap::Auto;
};
