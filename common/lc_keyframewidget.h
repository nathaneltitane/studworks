#pragma once

class lcKeyFrameWidget : public QCheckBox
{
	Q_OBJECT

public:
	lcKeyFrameWidget(QWidget* Parent);

	QSize sizeHint() const override;

	void paintEvent(QPaintEvent* PaintEvent) override;
};
