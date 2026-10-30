#include "sw_global.h"
#include "sw_widget_spinner.h"

swSpinnerWidget::swSpinnerWidget(QWidget* Parent)
	: QWidget(Parent)
{
	const int Size = qCeil(2.0 * (OrbitRadius + qMax(DiscRadius, 1.5) + 1.5));
	setFixedSize(Size, Size);
	setAccessibleName(tr("Loading"));
	mTimer.setInterval(16);
	connect(&mTimer, &QTimer::timeout, this, [this]() { update(); });
}

void swSpinnerWidget::showEvent(QShowEvent* Event)
{
	QWidget::showEvent(Event);
	mElapsed.start();
	mTimer.start();
}

void swSpinnerWidget::hideEvent(QHideEvent* Event)
{
	mTimer.stop();
	QWidget::hideEvent(Event);
}

void swSpinnerWidget::paintEvent(QPaintEvent* Event)
{
	Q_UNUSED(Event);

	QPainter Painter(this);
	Painter.setRenderHint(QPainter::Antialiasing);
	Painter.translate(width() / 2.0, height() / 2.0);

	// revolutions per second
	constexpr qreal SpinSpeed = 0.75;
	Painter.rotate(mElapsed.isValid() ? mElapsed.elapsed() * 0.36 * SpinSpeed : 0.0);

	const QRectF Orbit(-OrbitRadius, -OrbitRadius, 2.0 * OrbitRadius, 2.0 * OrbitRadius);

	// trail degrees behind the disc
	constexpr qreal TrailLength = 270.0;

	// trail fade rate
	constexpr qreal FadeDecay = 5.0;
	constexpr int Segments = 120;
	const qreal SegmentAngle = qBound(0.0, TrailLength, 360.0) / Segments;
	const qreal TailOpacity = qExp(-FadeDecay);

	// paint the oldest end first
	for (int Segment = Segments - 1; Segment >= 0 && SegmentAngle > 0.0; Segment--)
	{
		const qreal Age = static_cast<qreal>(Segment) / (Segments - 1);
		const qreal Opacity = (qExp(-FadeDecay * Age) - TailOpacity) / (1.0 - TailOpacity);
		QColor Color;
		Color.setRgbF(1.0, 1.0, 1.0, Opacity);
		Painter.setPen(QPen(Color, 3.0, Qt::SolidLine, Qt::RoundCap));
		Painter.drawArc(Orbit, qRound(Segment * SegmentAngle * 16.0), qRound(SegmentAngle * 16.0));
	}

	Painter.setPen(Qt::NoPen);
	Painter.setBrush(Qt::white);

//	Painter.drawEllipse(QPointF(OrbitRadius, 0.0), DiscRadius, DiscRadius);
}
