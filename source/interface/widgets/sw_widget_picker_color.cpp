#include "sw_global.h"
#include "sw_widget_picker_color.h"
#include "sw_widget_list_color.h"
#include "sw_colors.h"
#include "sw_window_utilities.h"

swColorPickerPopup::swColorPickerPopup(QWidget* Parent, int ColorIndex, bool AllowNoColor)
	: QFrame(Parent, Qt::Popup)
{
	setFrameStyle(QFrame::StyledPanel);
	setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

	setFocusPolicy(Qt::StrongFocus);
	setMouseTracking(true);

	QGridLayout *layout = new QGridLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	setLayout(layout);

	mColorList = new swColorList(this, AllowNoColor);
	connect(mColorList, &swColorList::ColorChanged, this, &swColorPickerPopup::ColorChanged);
	connect(mColorList, &swColorList::ColorSelected, this, &swColorPickerPopup::ColorSelected);
	layout->addWidget(mColorList);

	// Resize the popup when the tiles need a different height, so none are cut off.
	connect(mColorList, &swColorList::HeightChanged, this, [this]()
	{
		QTimer::singleShot(0, this, [this]()
		{
			adjustSize();
		});
	});

	mColorList->blockSignals(true);
	mColorList->SetCurrentColor(ColorIndex);
	mColorList->blockSignals(false);

	mEventLoop = nullptr;
}

swColorPickerPopup::~swColorPickerPopup()
{
	if (mEventLoop)
		mEventLoop->exit();
}

void swColorPickerPopup::exec()
{
	show();

	QEventLoop EventLoop;
	mEventLoop = &EventLoop;
	(void) EventLoop.exec();
	mEventLoop = nullptr;
}

void swColorPickerPopup::SetPosition(const QWidget* Anchor)
{
	ensurePolished();
	adjustSize();

	const QPoint Position = Anchor->mapToGlobal(Anchor->rect().bottomLeft());
	const QPoint ScreenPosition = Anchor->mapToGlobal(Anchor->rect().center());
	move(swGetPopupPosition(Position, size(), ScreenPosition));
}

void swColorPickerPopup::mouseReleaseEvent(QMouseEvent* MouseEvent)
{
	if (!rect().contains(MouseEvent->pos()))
		hide();
}

void swColorPickerPopup::hideEvent(QHideEvent* HideEvent)
{
	if (mEventLoop)
		mEventLoop->exit();

	emit Hid();
	QFrame::hideEvent(HideEvent);
}

void swColorPickerPopup::ColorChanged(int ColorIndex)
{
	emit Changed(ColorIndex);
}

void swColorPickerPopup::ColorSelected(int ColorIndex)
{
	emit Selected(ColorIndex);
	hide();
}

void swColorPickerPopup::showEvent(QShowEvent* ShowEvent)
{
	Q_UNUSED(ShowEvent);

	mColorList->setFocus();
}

swColorPicker::swColorPicker(QWidget* Parent, bool AllowNoColor)
	: QPushButton(Parent), mAllowNoColor(AllowNoColor)
{
	setFocusPolicy(Qt::StrongFocus);
	setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
	setAutoDefault(false);
	setCheckable(true);

	UpdateIcon();

	connect(this, &QPushButton::toggled, this, &swColorPicker::ButtonPressed);
}

swColorPicker::~swColorPicker()
{
}

void swColorPicker::SetCurrentColor(int ColorIndex)
{
	Selected(ColorIndex);
}

void swColorPicker::SetCurrentColorCode(int ColorCode)
{
	SetCurrentColor(swGetColorIndex(ColorCode));
}

int swColorPicker::GetCurrentColor() const
{
	return mCurrentColorIndex;
}

int swColorPicker::GetCurrentColorCode() const
{
	return gColorList[mCurrentColorIndex].Code;
}

void swColorPicker::ButtonPressed(bool Toggled)
{
	if (!Toggled)
		return;

	swColorPickerPopup* Popup = new swColorPickerPopup(this, mCurrentColorIndex, mAllowNoColor);
	connect(Popup, &swColorPickerPopup::Changed, this, &swColorPicker::Changed);
	connect(Popup, &swColorPickerPopup::Selected, this, &swColorPicker::Selected);
	connect(Popup, &swColorPickerPopup::Hid, this, &swColorPicker::PopupClosed);
	Popup->setMinimumSize(300, 200);
	Popup->SetPosition(this);

	clearFocus();
	update();

	Popup->setFocus();
	Popup->show();
}

void swColorPicker::UpdateIcon()
{
	const int IconSize = 14;//style()->pixelMetric(QStyle::PM_SmallIconSize);
	QPixmap Pixmap(IconSize, IconSize);

	QPainter Painter(&Pixmap);

	Painter.setPen(Qt::darkGray);

	const swColor* Color = &gColorList[mCurrentColorIndex];

	if (Color->Code != SW_COLOR_NO_COLOR)
	{
		Painter.setBrush(QColor::fromRgbF(Color->Value[0], Color->Value[1], Color->Value[2]));
		Painter.drawRect(0, 0, Pixmap.width() - 1, Pixmap.height() - 1);
	}
	else
		swDrawNoColorRectangle(Painter, QRect(0, 0, Pixmap.width() - 1, Pixmap.height() - 1));

	Painter.end();

	setIcon(QIcon(Pixmap));
}

void swColorPicker::PopupClosed()
{
	if (mInitialColorIndex != mCurrentColorIndex)
		Changed(mInitialColorIndex);

	setChecked(false);
	setFocus();
}

void swColorPicker::Changed(int ColorIndex)
{
	if (ColorIndex == mCurrentColorIndex)
		return;

	mCurrentColorIndex = ColorIndex;
	UpdateIcon();

	update();

	emit ColorChanged(mCurrentColorIndex);
}

void swColorPicker::Selected(int ColorIndex)
{
	mInitialColorIndex = ColorIndex;
	Changed(ColorIndex);
}
