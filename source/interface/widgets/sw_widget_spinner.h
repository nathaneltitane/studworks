#pragma once

class swSpinnerWidget : public QWidget
{
	Q_OBJECT

public:
	swSpinnerWidget(QWidget* Parent = nullptr);

protected:
	void paintEvent(QPaintEvent* Event) override;
	void showEvent(QShowEvent* Event) override;
	void hideEvent(QHideEvent* Event) override;

private:
	// Distance from the center.
	static constexpr qreal OrbitRadius = 7.0;

	// Size in logical pixels.
	static constexpr qreal DiscRadius = 3;

	QTimer mTimer;
	QElapsedTimer mElapsed;
};
