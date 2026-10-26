#include "sw_global.h"
#include "sw_keyframe_widget.h"
#include "sw_utilities.h"

swKeyFrameWidget::swKeyFrameWidget(QWidget* Parent)
	: QCheckBox(Parent)
{
	setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	setFocusPolicy(Qt::NoFocus);
	setAttribute(Qt::WA_Hover);
	setCheckable(true);
}

QSize swKeyFrameWidget::sizeHint() const
{
	return swGetPanelButtonSize();
}

void swKeyFrameWidget::paintEvent(QPaintEvent* PaintEvent)
{
	Q_UNUSED(PaintEvent);

	QPainter Painter(this);

	// Drawn as an outlined tool button the size of the panel options buttons, with the keyframe diamond as its icon.
	QStyleOptionToolButton Option;
	Option.initFrom(this);
	Option.subControls = QStyle::SC_ToolButton;
	Option.toolButtonStyle = Qt::ToolButtonIconOnly;
	Option.state |= QStyle::State_Raised;

	if (isDown())
		Option.state |= QStyle::State_Sunken;

	style()->drawComplexControl(QStyle::CC_ToolButton, &Option, &Painter, this);

	Qt::CheckState State = checkState();
	Painter.setRenderHint(QPainter::Antialiasing);
	Painter.setPen(palette().color(QPalette::Shadow));

	if (State != Qt::PartiallyChecked)
		Painter.setBrush(palette().color(QPalette::Text));

	const QPoint ButtonCenter = rect().center();
	const int Size = (State != Qt::Unchecked) ? 13 : 7;
	QRect Rect(0, 0, Size, Size);
	Rect.moveCenter(ButtonCenter);
	const QPoint Center = Rect.center();

	QPoint Points[4] =
	{
		QPoint(Rect.left(), Center.y()),
		QPoint(Center.x(), Rect.bottom()),
		QPoint(Rect.right(), Center.y()),
		QPoint(Center.x(), Rect.top())
	};

	Painter.drawPolygon(Points, 4);
}
