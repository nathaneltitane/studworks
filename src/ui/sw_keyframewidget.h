#pragma once

class swKeyFrameWidget : public QCheckBox
{
	Q_OBJECT

public:
	swKeyFrameWidget(QWidget* Parent);

	QSize sizeHint() const override;

	void paintEvent(QPaintEvent* PaintEvent) override;
};
