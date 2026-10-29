#include "sw_global.h"
#include "sw_view_widget.h"
#include "sw_extensions_opengl.h"
#include "sw_project.h"
#include "sw_library.h"
#include "sw_application.h"
#include "sw_window_main.h"
#include "sw_context.h"
#include "sw_view.h"
#include "sw_texture.h"
#include "sw_mesh.h"
#include "sw_profile.h"
#include "sw_widget_preview.h"
#include "sw_window_utilities.h"
#include "sw_view_cube.h"
#include "sw_object_camera.h"

swViewWidget::swViewWidget(QWidget* Parent, swView* View)
	: QOpenGLWidget(Parent), mView(View)
{
	mView->SetWidget(this);

	setMouseTracking(true);

	if (View->GetViewType() == swViewType::View)
	{
		setFocusPolicy(Qt::StrongFocus);
		setAcceptDrops(true);

		mOverlay = new QWidget(this);
		mOverlay->setObjectName(QStringLiteral("ViewOverlay"));
		mOverlayLayout = new QVBoxLayout(mOverlay);
		mOverlayLayout->setContentsMargins(0, 0, 0, 0);
		mOverlayLayout->setSpacing(style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing));
		mOverlayLayout->setSizeConstraint(QLayout::SetNoConstraint);
		mOverlay->installEventFilter(this);

		// the camera name floats over the view laid out like the find and replace bar, each bar only as wide as its contents
		const int ButtonHeight = swGetPanelButtonSize().height();
		mCameraBar = new QWidget(mOverlay);
		mCameraBar->setObjectName(QStringLiteral("CameraBar"));
		mCameraBar->setAttribute(Qt::WA_StyledBackground, true);
		mCameraBar->setStyleSheet(QStringLiteral("#CameraBar { background: palette(button); border: 1px solid palette(mid); border-radius: 3px; }"));
		mCameraBar->setVisible(false);

		QHBoxLayout* CameraLayout = new QHBoxLayout(mCameraBar);
		CameraLayout->setContentsMargins(5, 5, 5, 5);

		// the bar starts with its close button like the find and replace bar
		QToolButton* CameraCloseButton = new QToolButton(mCameraBar);
		CameraCloseButton->setIcon(QIcon(":/stylesheet/close.svg"));
		CameraCloseButton->setAccessibleName(tr("Free Camera"));
		CameraCloseButton->setToolTip(tr("Stop looking through this camera."));
		CameraCloseButton->setAutoRaise(false);
		CameraCloseButton->setFocusPolicy(Qt::NoFocus);
		CameraCloseButton->setFixedSize(ButtonHeight, ButtonHeight);
		CameraLayout->addWidget(CameraCloseButton);

		mCameraLabel = new QLabel(mCameraBar);
		mCameraLabel->setFixedHeight(ButtonHeight);
		CameraLayout->addWidget(mCameraLabel);
		mOverlayLayout->addWidget(mCameraBar, 0, Qt::AlignLeft);

		connect(CameraCloseButton, &QToolButton::clicked, this, [this]()
		{
			setFocus(Qt::OtherFocusReason);
			mView->RemoveCamera();
			update();
		});

		// the editing path follows below it
		mModelEditingBar = new QWidget(mOverlay);
		mModelEditingBar->setObjectName(QStringLiteral("ModelEditingBar"));
		mModelEditingBar->setAttribute(Qt::WA_StyledBackground, true);
		mModelEditingBar->setStyleSheet(QStringLiteral("#ModelEditingBar { background: palette(button); border: 1px solid palette(mid); border-radius: 3px; }"));
		mModelEditingBar->setWhatsThis(tr("<p><b>In-place Editing</b></p><p>The path shows the model being edited inside this view. The last name is the current editing target. Choose an earlier name to edit that ancestor, or the close button to return to the tab's model.</p>"));

		QHBoxLayout* EditingLayout = new QHBoxLayout(mModelEditingBar);
		EditingLayout->setContentsMargins(5, 5, 5, 5);

		// the bar starts with its close button like the find and replace bar
		mEndEditingButton = new QToolButton(mModelEditingBar);
		mEndEditingButton->setIcon(QIcon(":/stylesheet/close.svg"));
		mEndEditingButton->setAccessibleName(tr("End Editing"));
		mEndEditingButton->setToolTip(tr("Return to editing the tab's model."));
		mEndEditingButton->setAutoRaise(false);
		mEndEditingButton->setFocusPolicy(Qt::NoFocus);
		mEndEditingButton->setFixedSize(ButtonHeight, ButtonHeight);
		EditingLayout->addWidget(mEndEditingButton);

		mModelEditingPathLayout = new QHBoxLayout();
		mModelEditingPathLayout->setContentsMargins(0, 0, 0, 0);
		EditingLayout->addLayout(mModelEditingPathLayout, 1);
		mOverlayLayout->addWidget(mModelEditingBar, 0, Qt::AlignLeft);

		connect(mEndEditingButton, &QToolButton::clicked, this, [this]()
		{
			setFocus(Qt::OtherFocusReason);
			mView->SetTopSubmodelActive();
		});

		swSetupWidgetHelp(mModelEditingBar, nullptr);
		UpdateModelEditingPath();
	}
}

swViewWidget::~swViewWidget()
{
	if (context())
		makeCurrent();

	mView.reset();

	if (context())
		doneCurrent();
}

QSize swViewWidget::sizeHint() const
{
	return mPreferredSize.isEmpty() ? QOpenGLWidget::sizeHint() : mPreferredSize;
}

swView* swViewWidget::GetView() const
{
	return mView.get();
}

void swViewWidget::SetView(swView* View)
{
	if (context())
		makeCurrent();

	if (View)
	{
		if (context())
		{
			View->mContext->SetGLContext(context(), this);
		}

		View->SetWidget(this);
		const float Scale = GetDeviceScale();
		View->SetSize(width() * Scale, height() * Scale);

		if (hasFocus())
			View->SetFocus(true);
	}

	mView = std::unique_ptr<swView>(View);
	UpdateCameraName();
	UpdateModelEditingPath();
}

// use to show the name of the camera the view looks through, free cameras have none
void swViewWidget::UpdateCameraName()
{
	if (!mOverlay || !mView)
		return;

	const QString CameraName = mView->GetCamera()->GetName();

	if (CameraName == mCameraName)
		return;

	mCameraName = CameraName;
	mCameraLabel->setText(CameraName);
	mCameraLabel->setAccessibleName(tr("Camera: %1").arg(CameraName));
	mCameraBar->setVisible(!CameraName.isEmpty());

	UpdateOverlayGeometry();
}

void swViewWidget::UpdateModelEditingPath()
{
	if (!mOverlay || !mView)
		return;

	QStringList Names;

	for (const swModel* Model : mView->GetModelEditingPath())
		Names.append(Model->GetProperties().mFileName);

	if (Names != mModelEditingNames)
	{
		mModelEditingNames = Names;
		mModelEditingBar->setAccessibleName(tr("Editing model: %1").arg(Names.join(QStringLiteral(" \u203a "))));
		mModelEditingBar->setVisible(Names.size() > 1);
		UpdateModelEditingButtons();
	}

	UpdateOverlayGeometry();
}

void swViewWidget::SetFindReplaceWidget(QWidget* Widget)
{
	if (!mOverlay)
		return;

	mFindReplaceWidget = Widget;
	mOverlayLayout->addWidget(Widget, 0, Qt::AlignLeft);
	UpdateOverlayGeometry();
}

void swViewWidget::UpdateOverlayGeometry()
{
	if (!mOverlay)
		return;

	const bool Editing = mModelEditingNames.size() > 1;
	const bool Camera = !mCameraName.isEmpty();

	mOverlay->setVisible(Editing || Camera || mFindReplaceWidget);

	if (!Editing && !Camera && !mFindReplaceWidget)
		return;

	int PreferredWidth = 0;

	if (Editing)
		PreferredWidth = mModelEditingBar->sizeHint().width();

	if (Camera)
		PreferredWidth = swMaximum(PreferredWidth, mCameraBar->sizeHint().width());

	if (mFindReplaceWidget)
		PreferredWidth = swMaximum(PreferredWidth, mFindReplaceWidget->sizeHint().width());

	// the overlay keeps the standard layout margins from the view edges and the corner widgets
	mOverlayCornerSizes = GetOverlayCornerSizes();
	const int Margin = style()->pixelMetric(QStyle::PM_LayoutLeftMargin);
	const int OverlayHeight = mOverlayLayout->sizeHint().height();
	const int FullWidth = swClamp(PreferredWidth, 1, swMaximum(1, width() - 2 * Margin));
	const int TopY = Margin;
	const int BottomY = height() - Margin - OverlayHeight;

	// use to check the overlay against the view cube and axis icon of one row, spaced by the margin
	const auto IsRowFree = [this, Margin, OverlayHeight](int Y, int Left, int Width, int CornerLeft, int CornerRight)
	{
		const QRect OverlayRect(Left, Y, Width, OverlayHeight);
		const int CornerY = Y == Margin ? 0 : height();
		const QRect LeftRect(0, CornerY - (CornerY ? CornerLeft : 0), CornerLeft, CornerLeft);
		const QRect RightRect(width() - CornerRight, CornerY - (CornerY ? CornerRight : 0), CornerRight, CornerRight);

		return !(CornerLeft && OverlayRect.intersects(LeftRect.adjusted(0, 0, Margin, Margin))) && !(CornerRight && OverlayRect.intersects(RightRect.adjusted(-Margin, -Margin, 0, 0)));
	};

	// it stays at the top left and only moves when a corner widget is actually in its way, first to the bottom then beside the widgets
	int OverlayX = Margin;
	int OverlayY = TopY;
	int OverlayWidth = FullWidth;

	if (!IsRowFree(TopY, Margin, FullWidth, mOverlayCornerSizes[0], mOverlayCornerSizes[1]))
	{
		if (IsRowFree(BottomY, Margin, FullWidth, mOverlayCornerSizes[2], mOverlayCornerSizes[3]))
			OverlayY = BottomY;
		else
		{
			const bool OverlayBottom = mOverlayCornerSizes[2] + mOverlayCornerSizes[3] < mOverlayCornerSizes[0] + mOverlayCornerSizes[1];
			const int CornerLeft = mOverlayCornerSizes[OverlayBottom ? 2 : 0];
			const int CornerRight = mOverlayCornerSizes[OverlayBottom ? 3 : 1];

			OverlayX = Margin + (CornerLeft ? CornerLeft + Margin : 0);
			OverlayY = OverlayBottom ? BottomY : TopY;
			OverlayWidth = swClamp(PreferredWidth, 1, swMaximum(1, width() - Margin - (CornerRight ? CornerRight + Margin : 0) - OverlayX));
		}
	}

	mOverlay->setGeometry(OverlayX, OverlayY, OverlayWidth, OverlayHeight);
	mOverlayLayout->activate();
	mModelEditingBar->layout()->activate();
}

// use to size the view cube and axis icon by corner in the order top left, top right, bottom left, bottom right
std::array<int, 4> swViewWidget::GetOverlayCornerSizes()
{
	const swPreferences& Preferences = swGetPreferences();
	std::array<int, 4> CornerSizes = {};

	if (Preferences.mViewCubeEnabled && Preferences.mViewCubeSize > 0)
	{
		int& CornerSize = CornerSizes[static_cast<int>(Preferences.mViewCubeLocation)];
		CornerSize = swMaximum(CornerSize, Preferences.mViewCubeSize);
	}

	if (Preferences.mDrawAxes && Preferences.mAxisIconSize > 0)
	{
		int& CornerSize = CornerSizes[static_cast<int>(Preferences.mAxisIconLocation)];
		CornerSize = swMaximum(CornerSize, Preferences.mAxisIconSize);
	}

	return CornerSizes;
}

// use to show each model of the editing path as a button, the earlier ones edit that ancestor again
void swViewWidget::UpdateModelEditingButtons()
{
	// the clicked button may still be in its signal so the old entries go once it returns
	while (QLayoutItem* Item = mModelEditingPathLayout->takeAt(0))
	{
		Item->widget()->hide();
		Item->widget()->deleteLater();
		delete Item;
	}

	if (mModelEditingNames.size() < 2)
		return;

	const int ButtonHeight = swGetPanelButtonSize().height();
	const QPixmap Chevron = swCreateDisclosureIcon(false, mModelEditingBar->palette()).pixmap(16, 16);

	for (int Index = 0; Index < mModelEditingNames.size(); Index++)
	{
		if (Index)
		{
			QLabel* ChevronLabel = new QLabel(mModelEditingBar);
			ChevronLabel->setPixmap(Chevron);
			mModelEditingPathLayout->addWidget(ChevronLabel);
		}

		swModelEditingButton* Button = new swModelEditingButton(mModelEditingBar);
		Button->setText(mModelEditingNames[Index]);
		Button->setToolTip(mModelEditingNames[Index]);
		Button->setAutoRaise(false);
		Button->setFocusPolicy(Qt::NoFocus);
		Button->setFixedHeight(ButtonHeight);
		mModelEditingPathLayout->addWidget(Button);

		// the current model is bold and the earlier ones return to that model
		if (Index + 1 == mModelEditingNames.size())
		{
			QFont Font = Button->font();
			Font.setBold(true);
			Button->setFont(Font);
			continue;
		}

		connect(Button, &QToolButton::clicked, this, [this, Index]()
		{
			if (Index + 1 >= mModelEditingNames.size())
				return;

			setFocus(Qt::OtherFocusReason);
			mView->SetSubmodelActive(Index);
		});
	}
}

bool swViewWidget::eventFilter(QObject* Object, QEvent* Event)
{
	if (Object == mOverlay && Event->type() == QEvent::LayoutRequest)
		UpdateOverlayGeometry();

	return QOpenGLWidget::eventFilter(Object, Event);
}

void swViewWidget::resizeEvent(QResizeEvent* Event)
{
	QOpenGLWidget::resizeEvent(Event);
	UpdateOverlayGeometry();
}

void swViewWidget::UpdateMousePosition()
{
	QPoint MousePosition = mapFromGlobal(QCursor::pos());
	float DeviceScale = GetDeviceScale();

	mView->SetMousePosition(MousePosition.x() * DeviceScale, mView->GetHeight() - MousePosition.y() * DeviceScale - 1);
	mView->SetMouseModifiers(QApplication::keyboardModifiers());
}

void swViewWidget::initializeGL()
{
	mView->mContext->SetGLContext(context(), this);
}

void swViewWidget::resizeGL(int Width, int Height)
{
	const float Scale = GetDeviceScale();

	mView->SetSize(Width * Scale, Height * Scale);
}

void swViewWidget::paintGL()
{
	// corner widget settings and the viewed camera change without a resize, catch them on the next repaint
	if (mOverlay && mOverlay->isVisible() && GetOverlayCornerSizes() != mOverlayCornerSizes)
		UpdateOverlayGeometry();

	UpdateCameraName();

	mView->OnDraw();
}

bool swViewWidget::event(QEvent* Event)
{
	if (Event->type() == QEvent::NativeGesture)
	{
		QNativeGestureEvent* GestureEvent = static_cast<QNativeGestureEvent*>(Event);

		switch (GestureEvent->gestureType())
		{
		case Qt::ZoomNativeGesture:
			mView->Zoom(GestureEvent->value() * 100.0f);
			break;

		case Qt::SmartZoomNativeGesture:
			mView->ZoomExtents(swGeometryBoundsMode::IncludeFallback, false);
			break;

		default:
			break;
		}
	}

	return QOpenGLWidget::event(Event);
}

void swViewWidget::focusInEvent(QFocusEvent* FocusEvent)
{
	if (mView)
		mView->SetFocus(true);

	QOpenGLWidget::focusInEvent(FocusEvent);
}

void swViewWidget::focusOutEvent(QFocusEvent* FocusEvent)
{
	if (mView)
		mView->SetFocus(false);

	QOpenGLWidget::focusOutEvent(FocusEvent);
}

void swViewWidget::keyPressEvent(QKeyEvent* KeyEvent)
{
	if (KeyEvent->key() == Qt::Key_Control || KeyEvent->key() == Qt::Key_Shift)
	{
		mView->SetMouseModifiers(KeyEvent->modifiers());
		mView->UpdateCursor();
	}

	QOpenGLWidget::keyPressEvent(KeyEvent);
}

void swViewWidget::keyReleaseEvent(QKeyEvent* KeyEvent)
{
	if (KeyEvent->key() == Qt::Key_Control || KeyEvent->key() == Qt::Key_Shift)
	{
		mView->SetMouseModifiers(KeyEvent->modifiers());
		mView->UpdateCursor();
	}

	QOpenGLWidget::keyReleaseEvent(KeyEvent);
}

void swViewWidget::mousePressEvent(QMouseEvent* MouseEvent)
{
	float DeviceScale = GetDeviceScale();

	mView->SetMousePosition(MouseEvent->x() * DeviceScale, mView->GetHeight() - MouseEvent->y() * DeviceScale - 1);
	mView->SetMouseModifiers(MouseEvent->modifiers());

	switch (MouseEvent->button())
	{
	case Qt::LeftButton:
		mView->OnLeftButtonDown();
		break;

	case Qt::MiddleButton:
		mView->OnMiddleButtonDown();
		break;

	case Qt::RightButton:
		mView->OnRightButtonDown();
		break;

	case Qt::BackButton:
		mView->OnBackButtonDown();
		break;

	case Qt::ForwardButton:
		mView->OnForwardButtonDown();
		break;

	default:
		break;
	}
}

void swViewWidget::mouseReleaseEvent(QMouseEvent* MouseEvent)
{
	float DeviceScale = GetDeviceScale();

	mView->SetMousePosition(MouseEvent->x() * DeviceScale, mView->GetHeight() - MouseEvent->y() * DeviceScale - 1);
	mView->SetMouseModifiers(MouseEvent->modifiers());

	switch (MouseEvent->button())
	{
	case Qt::LeftButton:
		mView->OnLeftButtonUp();
		break;

	case Qt::MiddleButton:
		mView->OnMiddleButtonUp();
		break;

	case Qt::RightButton:
		mView->OnRightButtonUp();
		break;

	case Qt::BackButton:
		mView->OnBackButtonUp();
		break;

	case Qt::ForwardButton:
		mView->OnForwardButtonUp();
		break;

	default:
		break;
	}
}

void swViewWidget::mouseDoubleClickEvent(QMouseEvent* MouseEvent)
{
	float DeviceScale = GetDeviceScale();

	mView->SetMousePosition(MouseEvent->x() * DeviceScale, mView->GetHeight() - MouseEvent->y() * DeviceScale - 1);
	mView->SetMouseModifiers(MouseEvent->modifiers());

	switch (MouseEvent->button())
	{
	case Qt::LeftButton:
		mView->OnLeftButtonDoubleClick();
		break;

	default:
		break;
	}
}

void swViewWidget::contextMenuEvent(QContextMenuEvent* ContextMenuEvent)
{
	// Views show their own menu when the right button is released, so the press is never a panel menu that would stop a drag.
	ContextMenuEvent->accept();
}

void swViewWidget::mouseMoveEvent(QMouseEvent* MouseEvent)
{
	float DeviceScale = GetDeviceScale();

	mView->SetMousePosition(MouseEvent->x() * DeviceScale, mView->GetHeight() - MouseEvent->y() * DeviceScale - 1);
	mView->SetMouseModifiers(MouseEvent->modifiers());

	mView->OnMouseMove();
}

void swViewWidget::wheelEvent(QWheelEvent* WheelEvent)
{
	if (WheelEvent->source() == Qt::MouseEventSynthesizedBySystem)
	{
		switch (WheelEvent->phase())
		{
		case Qt::NoScrollPhase:
			break;

		case Qt::ScrollBegin:
			mView->StartPanGesture();
			WheelEvent->accept();
			return;

		case Qt::ScrollUpdate:
#if (QT_VERSION >= QT_VERSION_CHECK(5, 12, 0))
		case Qt::ScrollMomentum:
#endif
			mView->UpdatePanGesture(WheelEvent->pixelDelta().x(), -WheelEvent->pixelDelta().y());
			WheelEvent->accept();
			return;

		case Qt::ScrollEnd:
			mView->EndPanGesture(true);
			WheelEvent->accept();
			return;
		}
	}

	if (WheelEvent->angleDelta().y() == 0)
	{
		WheelEvent->ignore();
		return;
	}

	float DeviceScale = GetDeviceScale();

#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
	mView->SetMousePosition(WheelEvent->position().x() * DeviceScale, mView->GetHeight() - WheelEvent->position().y() * DeviceScale - 1);
#else
	mView->SetMousePosition(WheelEvent->x() * DeviceScale, mView->GetHeight() - WheelEvent->y() * DeviceScale - 1);
#endif
	mView->SetMouseModifiers(WheelEvent->modifiers());

	mWheelAccumulator += WheelEvent->angleDelta().y();
	int Steps = mWheelAccumulator / 8;

	if (Steps)
	{
		mView->OnMouseWheel(Steps / 15);
		mWheelAccumulator -= Steps * 8;
	}

	WheelEvent->accept();
}

void swViewWidget::leaveEvent(QEvent* Event)
{
	mView->OnMouseLeave();

	QOpenGLWidget::leaveEvent(Event);
}

void swViewWidget::dragEnterEvent(QDragEnterEvent* DragEnterEvent)
{
	const QMimeData* MimeData = DragEnterEvent->mimeData();

	if (MimeData->hasFormat("application/vnd.studworks-part"))
	{
		DragEnterEvent->acceptProposedAction();
		mView->BeginDrag(swDragState::Part);
		return;
	}
	else if (MimeData->hasFormat("application/vnd.studworks-color"))
	{
		DragEnterEvent->acceptProposedAction();
		mView->BeginDrag(swDragState::Color);
		return;
	}

	DragEnterEvent->ignore();
}

void swViewWidget::dragLeaveEvent(QDragLeaveEvent* DragLeaveEvent)
{
	mView->EndDrag(false);
	DragLeaveEvent->accept();
}

void swViewWidget::dragMoveEvent(QDragMoveEvent* DragMoveEvent)
{
	const QMimeData* MimeData = DragMoveEvent->mimeData();

	if (MimeData->hasFormat("application/vnd.studworks-part") || MimeData->hasFormat("application/vnd.studworks-color"))
	{
		float DeviceScale = GetDeviceScale();

		mView->SetMousePosition(DragMoveEvent->pos().x() * DeviceScale, mView->GetHeight() - DragMoveEvent->pos().y() * DeviceScale - 1);
		mView->SetMouseModifiers(DragMoveEvent->keyboardModifiers());

		mView->OnMouseMove();

		DragMoveEvent->accept();
		return;
	}

	QOpenGLWidget::dragMoveEvent(DragMoveEvent);
}

void swViewWidget::dropEvent(QDropEvent* DropEvent)
{
	const QMimeData* MimeData = DropEvent->mimeData();

	if (MimeData->hasFormat("application/vnd.studworks-part") || MimeData->hasFormat("application/vnd.studworks-color"))
	{
		mView->EndDrag(true);
		window()->activateWindow();
		setFocus(Qt::MouseFocusReason);

		DropEvent->accept();
		return;
	}

	QOpenGLWidget::dropEvent(DropEvent);
}
