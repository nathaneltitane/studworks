#include "sw_global.h"
#include "sw_widget_label_elided.h"

void swElidedLabel::paintEvent(QPaintEvent* event)
{
	QFrame::paintEvent(event);

	QPainter Painter(this);
	QFontMetrics FontMetrics = Painter.fontMetrics();

	int LineSpacing = FontMetrics.lineSpacing();
	int y = 0;

	QTextLayout TextLayout(mText, Painter.font());
	TextLayout.beginLayout();

	for (;;)
	{
		QTextLine Line = TextLayout.createLine();

		if (!Line.isValid())
			break;

		Line.setLineWidth(width());
		int NextLineY = y + LineSpacing;

		if (height() >= NextLineY + LineSpacing)
		{
			Line.draw(&Painter, QPoint(0, y));
			y = NextLineY;
		}
		else
		{
			QString LastLine = mText.mid(Line.textStart());
			QString ElidedLastLine = FontMetrics.elidedText(LastLine, Qt::ElideRight, width());
			Painter.drawText(QPoint(0, y + FontMetrics.ascent()), ElidedLastLine);
			Line = TextLayout.createLine();
			break;
		}
	}

	TextLayout.endLayout();
}
