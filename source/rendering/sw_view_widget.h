#pragma once

#include "sw_window_utilities.h"

// Elides the editing path.
class swModelEditingButton : public swElidableToolButton
{
public:
	using swElidableToolButton::swElidableToolButton;

	QSize sizeHint() const override
	{
		return QToolButton::sizeHint();
	}

	QSize minimumSizeHint() const override
	{
		return QSize(0, QToolButton::minimumSizeHint().height());
	}
};

class swViewWidget : public QOpenGLWidget
{
	Q_OBJECT

public:
	swViewWidget(QWidget* Parent, swView* View);
	~swViewWidget();

	swView* GetView() const;
	void SetView(swView* View);

	void UpdateMousePosition();
	void UpdateModelEditingPath();
	void SetFindReplaceWidget(QWidget* Widget);

	QSize sizeHint() const override;

	float GetDeviceScale() const
	{
#if (QT_VERSION >= QT_VERSION_CHECK(5, 6, 0))
		return devicePixelRatioF();
#else
		return devicePixelRatio();
#endif
	}

protected:
	void initializeGL() override;
	void resizeGL(int Width, int Height) override;
	void paintGL() override;
	bool event(QEvent* Event) override;
	bool eventFilter(QObject* Object, QEvent* Event) override;
	void resizeEvent(QResizeEvent* Event) override;
	void focusInEvent(QFocusEvent* FocusEvent) override;
	void focusOutEvent(QFocusEvent* FocusEvent) override;
	void keyPressEvent(QKeyEvent* KeyEvent) override;
	void keyReleaseEvent(QKeyEvent* KeyEvent) override;
	void mousePressEvent(QMouseEvent* MouseEvent) override;
	void mouseReleaseEvent(QMouseEvent* MouseEvent) override;
	void mouseDoubleClickEvent(QMouseEvent* MouseEvent) override;
	void mouseMoveEvent(QMouseEvent* MouseEvent) override;
	void contextMenuEvent(QContextMenuEvent* ContextMenuEvent) override;
	void wheelEvent(QWheelEvent* WheelEvent) override;
	void leaveEvent(QEvent* Event) override;
	void dragEnterEvent(QDragEnterEvent* DragEnterEvent) override;
	void dragLeaveEvent(QDragLeaveEvent* DragLeaveEvent) override;
	void dragMoveEvent(QDragMoveEvent* DragMoveEvent) override;
	void dropEvent(QDropEvent* DropEvent) override;

	void UpdateOverlayGeometry();
	void UpdateModelEditingButtons();
	void UpdateCameraName();
	static std::array<int, 4> GetOverlayCornerSizes();

	std::unique_ptr<swView> mView;
	QWidget* mOverlay = nullptr;
	QVBoxLayout* mOverlayLayout = nullptr;
	QWidget* mCameraBar = nullptr;
	QLabel* mCameraLabel = nullptr;
	QString mCameraName;
	QWidget* mModelEditingBar = nullptr;
	QHBoxLayout* mModelEditingPathLayout = nullptr;
	QToolButton* mEndEditingButton = nullptr;
	QPointer<QWidget> mFindReplaceWidget;
	QStringList mModelEditingNames;
	std::array<int, 4> mOverlayCornerSizes = {};
	QSize mPreferredSize;
	int mWheelAccumulator = 0;
};
