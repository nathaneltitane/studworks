#pragma once

enum class swFloatPropertySnap;

class swLadderWidget: public QWidget
{
	Q_OBJECT

public:
	swLadderWidget(QAbstractSpinBox* SpinBox, swFloatPropertySnap Snap);
	virtual ~swLadderWidget() = default;

	void Show();

	bool eventFilter(QObject* Object, QEvent* Event) override;
	void mousePressEvent(QMouseEvent* MouseEvent) override;
	void mouseMoveEvent(QMouseEvent* MouseEvent) override;
	void mouseReleaseEvent(QMouseEvent* MouseEvent) override;

signals:
	void EditingCanceled();
	void EditingFinished();

protected slots:
	void CancelEditing();
	void FinishEditing();

protected:
	void CalculateSteps(swFloatPropertySnap Snap);
	void UpdateMousePosition();

	void paintEvent(QPaintEvent* PaintEvent) override;

	QPointer<QAbstractSpinBox> mSpinBox = nullptr;
	std::vector<double> mSteps;
	int mCurrentStep = -1;
	int mLastMousePositionX = 0;
};
