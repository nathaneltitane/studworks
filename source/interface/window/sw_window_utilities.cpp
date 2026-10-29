#include "sw_global.h"
#include "sw_window_utilities.h"
#include "sw_application.h"
#include "sw_library.h"
#include "sw_model.h"
#include "sw_data_part.h"
#include "sw_string.h"
#include <QPrintDialog>

#ifdef Q_OS_WIN
#include <windows.h>
#include <TlHelp32.h>
#endif

QPoint swGetPopupPosition(const QPoint& Position, const QSize& Size, const QPoint& ScreenPosition)
{
	QScreen* Screen = QGuiApplication::screenAt(ScreenPosition);

	if (!Screen)
		Screen = QGuiApplication::primaryScreen();

	if (!Screen)
		return Position;

	const QRect AvailableGeometry = Screen->availableGeometry();
	const int X = qBound(AvailableGeometry.left(), Position.x(), qMax(AvailableGeometry.left(), AvailableGeometry.right() - Size.width() + 1));
	const int Y = qBound(AvailableGeometry.top(), Position.y(), qMax(AvailableGeometry.top(), AvailableGeometry.bottom() - Size.height() + 1));

	return QPoint(X, Y);
}

bool swWindowGeometryFilter::eventFilter(QObject* Object, QEvent* Event)
{
	if (Event->type() != QEvent::Show || !Object->isWidgetType())
		return false;

	QWidget* Window = static_cast<QWidget*>(Object);

	if (!Window->isWindow() || Window->property("swInitialGeometry").toBool())
		return false;

	// Only dialogs and main windows, the standard Qt dialogs size themselves.
	const bool IsDialog = qobject_cast<QDialog*>(Window) && !qobject_cast<QMessageBox*>(Window) && !qobject_cast<QInputDialog*>(Window) && !qobject_cast<QProgressDialog*>(Window) &&
		!qobject_cast<QFileDialog*>(Window) && !qobject_cast<QColorDialog*>(Window) && !qobject_cast<QFontDialog*>(Window) && !qobject_cast<QPrintDialog*>(Window);
	const bool IsMainWindow = qobject_cast<QMainWindow*>(Window);
	const bool IsSecondaryWindow = IsMainWindow && Window->parentWidget();

	if (!IsDialog && !IsMainWindow)
		return false;

	Window->setProperty("swInitialGeometry", true);

	QWidget* Parent = Window->parentWidget() ? Window->parentWidget()->window() : nullptr;
	QScreen* Screen = Parent ? Parent->screen() : Window->screen();

	if (!Screen)
		return false;

	// A quarter of the available screen, never smaller than the contents need and never larger than the screen.
	const QRect Available = Screen->availableGeometry();
	const QSize MinimumSize = QSize(Available.width() / 4, Available.height() / 4).expandedTo(Window->minimumSizeHint()).boundedTo(Available.size());

	// Every window keeps at least that quarter when resized, the main window restores the rest of its geometry itself.
	Window->setMinimumSize(MinimumSize);

	if (!IsDialog && !IsSecondaryWindow)
		return false;

	// only the size is set, the window manager places the window and applies its rules and tiling
	QSize Size = MinimumSize.expandedTo(Window->sizeHint());
	Size = Size.boundedTo(Available.size());

	Window->resize(Size);

	return false;
}

// use to keep a scroll area that never scrolls sideways as wide as its rows
static void swFitScrollAreaWidth(QScrollArea* ScrollArea)
{
	if (ScrollArea->widget())
		ScrollArea->setMinimumWidth(ScrollArea->widget()->minimumSizeHint().width() + ScrollArea->verticalScrollBar()->sizeHint().width() + 2 * ScrollArea->frameWidth());
}

void swInterfaceStyle::drawControl(ControlElement Element, const QStyleOption* Option, QPainter* Painter, const QWidget* Widget) const
{
	const QStyleOptionComboBox* ComboBoxOption = qstyleoption_cast<const QStyleOptionComboBox*>(Option);

	if (Element != CE_ComboBoxLabel || !ComboBoxOption || ComboBoxOption->editable)
	{
		QProxyStyle::drawControl(Element, Option, Painter, Widget);
		return;
	}

	// the value and its icon sit in the middle of the box like a button label
	const QRect EditRect = subControlRect(CC_ComboBox, ComboBoxOption, SC_ComboBoxEditField, Widget).adjusted(1, 0, -1, 0);
	const QSize IconSize = ComboBoxOption->currentIcon.isNull() ? QSize() : ComboBoxOption->iconSize;
	const int Spacing = IconSize.isEmpty() ? 0 : pixelMetric(PM_ButtonIconSize, Option, Widget) / 4;
	const QString Text = ComboBoxOption->fontMetrics.elidedText(ComboBoxOption->currentText, Qt::ElideRight, EditRect.width() - IconSize.width() - Spacing);
	const int Width = IconSize.width() + Spacing + ComboBoxOption->fontMetrics.horizontalAdvance(Text);
	const int Left = EditRect.left() + qMax((EditRect.width() - Width) / 2, 0);

	Painter->save();
	Painter->setClipRect(EditRect);

	if (!IconSize.isEmpty())
	{
		const QIcon::Mode Mode = (ComboBoxOption->state & State_Enabled) ? QIcon::Normal : QIcon::Disabled;
		const QRect IconRect(Left, EditRect.top() + (EditRect.height() - IconSize.height()) / 2, IconSize.width(), IconSize.height());
		ComboBoxOption->currentIcon.paint(Painter, IconRect, Qt::AlignCenter, Mode);
	}

	const QRect TextRect(Left + IconSize.width() + Spacing, EditRect.top(), EditRect.right() - Left - IconSize.width() - Spacing + 1, EditRect.height());
	drawItemText(Painter, TextRect, Qt::AlignLeft | Qt::AlignVCenter, ComboBoxOption->palette, ComboBoxOption->state & State_Enabled, Text);

	Painter->restore();
}

QSize swInterfaceStyle::sizeFromContents(ContentsType Type, const QStyleOption* Option, const QSize& Size, const QWidget* Widget) const
{
	QSize Result = QProxyStyle::sizeFromContents(Type, Option, Size, Widget);

	if (Type != CT_LineEdit && Type != CT_SpinBox && Type != CT_ComboBox)
		return Result;

	// the text field inside a spin box is measured by the spin box itself
	if (Type == CT_LineEdit && Widget && qobject_cast<const QAbstractSpinBox*>(Widget->parentWidget()))
		return Result;

	// inputs are as tall as a standard button so they line up with the buttons beside them
	QStyleOptionButton ButtonOption;
	ButtonOption.QStyleOption::operator=(*Option);
	ButtonOption.text = QLatin1String("X");

	const QSize TextSize(ButtonOption.fontMetrics.horizontalAdvance(ButtonOption.text), ButtonOption.fontMetrics.height());
	const int ButtonHeight = QProxyStyle::sizeFromContents(CT_PushButton, &ButtonOption, TextSize, nullptr).height();

	Result.setHeight(qMax(Result.height(), ButtonHeight));

	return Result;
}

bool swInterfaceFitFilter::eventFilter(QObject* Object, QEvent* Event)
{
	if (!Object->isWidgetType())
		return false;

	const QEvent::Type Type = Event->type();

	if (Type != QEvent::Polish && Type != QEvent::Show && Type != QEvent::FontChange && Type != QEvent::StyleChange && Type != QEvent::LayoutRequest)
		return false;

	QWidget* Widget = static_cast<QWidget*>(Object);

	// spin boxes show their whole value and range beside the arrows
	if (QAbstractSpinBox* SpinBox = qobject_cast<QAbstractSpinBox*>(Widget))
	{
		if (Type != QEvent::LayoutRequest)
			swUpdatePanelSpinBoxWidth(SpinBox);

		return false;
	}

	// rows of a scroll area without a sideways scroll bar are never cut
	if (Type == QEvent::LayoutRequest)
	{
		QWidget* Viewport = Widget->parentWidget();
		QScrollArea* ScrollArea = Viewport ? qobject_cast<QScrollArea*>(Viewport->parentWidget()) : nullptr;

		if (ScrollArea && ScrollArea->widget() == Widget && ScrollArea->horizontalScrollBarPolicy() == Qt::ScrollBarAlwaysOff)
			swFitScrollAreaWidth(ScrollArea);

		return false;
	}

	if (QScrollArea* ScrollArea = qobject_cast<QScrollArea*>(Widget))
	{
		if (ScrollArea->horizontalScrollBarPolicy() == Qt::ScrollBarAlwaysOff)
			swFitScrollAreaWidth(ScrollArea);

		return false;
	}

	// panels of every window sit side by side, stacked or tabbed
	if (QMainWindow* MainWindow = qobject_cast<QMainWindow*>(Widget))
	{
		if (Type == QEvent::Polish)
			MainWindow->setDockOptions(QMainWindow::AnimatedDocks | QMainWindow::AllowNestedDocks | QMainWindow::AllowTabbedDocks);

		return false;
	}

	// floating panels fit their contents, docked panels follow the dock area
	if (QDockWidget* DockWidget = qobject_cast<QDockWidget*>(Widget))
	{
		// panels with a floating size undock at that size before the window manager maps them
		if (Type == QEvent::Show)
		{
			const QSize FloatingSize = DockWidget->property("swFloatingSize").toSize();

			if (FloatingSize.isValid() && DockWidget->isFloating() && !DockWidget->property("swShownFloating").toBool())
				DockWidget->resize(FloatingSize.expandedTo(DockWidget->minimumSizeHint()));

			DockWidget->setProperty("swShownFloating", DockWidget->isFloating());

			return false;
		}

		if (Type != QEvent::Polish || DockWidget->property("swFitFloating").toBool())
			return false;

		DockWidget->setProperty("swFitFloating", true);

		auto FitDock = [DockWidget]()
		{
			QTimer::singleShot(0, DockWidget, [DockWidget]()
			{
				swFitFloatingDock(DockWidget);
			});
		};

		QObject::connect(DockWidget, &QDockWidget::topLevelChanged, DockWidget, FitDock);
		QObject::connect(DockWidget, &QDockWidget::dockLocationChanged, DockWidget, FitDock);

		QTimer::singleShot(0, DockWidget, [DockWidget]()
		{
			swFitFloatingDock(DockWidget);
		});
	}

	return false;
}

// use to tell lists, editors and views apart, they have no natural size
static bool swIsViewDock(const QDockWidget* DockWidget)
{
	const QList<QWidget*> Children = DockWidget->findChildren<QWidget*>();

	for (const QWidget* Child : Children)
	{
		// the lists of dropdowns are popups of their own, not part of the panel
		if (Child->window() != DockWidget->window())
			continue;

		const QAbstractScrollArea* View = qobject_cast<const QAbstractScrollArea*>(Child);

		if ((View && !qobject_cast<const QScrollArea*>(View)) || qobject_cast<const QOpenGLWidget*>(Child) || Child->inherits("QWindowContainer"))
			return true;
	}

	return false;
}

void swFitFloatingDock(QDockWidget* DockWidget)
{
	DockWidget->setMinimumSize(0, 0);
	DockWidget->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);

	if (!DockWidget->isFloating())
	{
		// panels across the top or bottom keep the width of their widest rows instead of stretching them
		const QMainWindow* MainWindow = qobject_cast<const QMainWindow*>(DockWidget->parentWidget());

		if (!MainWindow)
			return;

		for (const Qt::DockWidgetArea Area : { Qt::TopDockWidgetArea, Qt::BottomDockWidgetArea })
		{
			std::vector<QDockWidget*> AreaDockWidgets;
			int Width = 0;

			for (QDockWidget* AreaDockWidget : MainWindow->findChildren<QDockWidget*>(QString(), Qt::FindDirectChildrenOnly))
			{
				// panels with a floating size stretch like views
				if (AreaDockWidget->isFloating() || MainWindow->dockWidgetArea(AreaDockWidget) != Area || swIsViewDock(AreaDockWidget) || AreaDockWidget->property("swFloatingSize").toSize().isValid())
					continue;

				AreaDockWidget->setMaximumWidth(QWIDGETSIZE_MAX);
				AreaDockWidgets.push_back(AreaDockWidget);
				Width = qMax(Width, AreaDockWidget->minimumSizeHint().width());
			}

			for (QDockWidget* AreaDockWidget : AreaDockWidgets)
				AreaDockWidget->setMaximumWidth(Width);
		}

		return;
	}

	// panels with a floating size stay resizable and float back at that size
	const QSize FloatingSize = DockWidget->property("swFloatingSize").toSize();

	if (FloatingSize.isValid())
	{
		DockWidget->setMinimumSize(DockWidget->minimumSizeHint());
		DockWidget->resize(FloatingSize.expandedTo(DockWidget->minimumSizeHint()));
		return;
	}

	// lists, editors and views keep their least size and stay resizable
	if (swIsViewDock(DockWidget))
	{
		DockWidget->setMinimumSize(DockWidget->minimumSizeHint());
		return;
	}

	// the rows of a scrolled panel are measured inside it, the screen bounds the height
	int Height = DockWidget->sizeHint().height();
	int Width = DockWidget->minimumSizeHint().width();
	const QList<QScrollArea*> ScrollAreas = DockWidget->findChildren<QScrollArea*>();

	for (const QScrollArea* ScrollArea : ScrollAreas)
		if (ScrollArea->widget())
			Height += qMax(ScrollArea->widget()->sizeHint().height() - ScrollArea->sizeHint().height() + 2 * ScrollArea->frameWidth(), 0);

	if (const QScreen* Screen = DockWidget->screen())
	{
		const int ScreenHeight = Screen->availableGeometry().height() - DockWidget->frameGeometry().height() + DockWidget->height();

		// a panel cut by the screen scrolls, its scroll bar widens it
		if (Height > ScreenHeight)
		{
			Height = ScreenHeight;

			for (const QScrollArea* ScrollArea : ScrollAreas)
				if (ScrollArea->widget())
					Width += qMax(ScrollArea->widget()->minimumSizeHint().width() + ScrollArea->verticalScrollBar()->sizeHint().width() + 2 * ScrollArea->frameWidth() - ScrollArea->minimumSizeHint().expandedTo(ScrollArea->minimumSize()).width(), 0);
		}
	}

	DockWidget->setFixedSize(Width, qMax(Height, DockWidget->minimumSizeHint().height()));
}

class swFitHeightFilter : public QObject
{
public:
	explicit swFitHeightFilter(QWidget* Widget)
		: QObject(Widget)
	{
	}

protected:
	bool eventFilter(QObject* Object, QEvent* Event) override
	{
		if (Event->type() == QEvent::LayoutRequest)
		{
			QWidget* Widget = static_cast<QWidget*>(Object);
			Widget->setFixedHeight(Widget->sizeHint().height());
		}

		return false;
	}
};

void swFitHeightToContents(QWidget* Widget)
{
	Widget->setFixedHeight(Widget->sizeHint().height());
	Widget->installEventFilter(new swFitHeightFilter(Widget));
}

swBlockingProgress* swBlockingProgress::mActive = nullptr;

swBlockingProgress::swBlockingProgress(const QString& Title, const QString& Label, bool Cancelable)
	: mTitle(Title), mLabel(Label), mCancelable(Cancelable)
{
	// A wait started while another one shows its progress is part of the same task.
	mNested = mActive != nullptr;

	if (!mNested)
		mActive = this;

	mTimer.start();
}

swBlockingProgress::~swBlockingProgress()
{
	delete mDialog;

	if (mActive == this)
		mActive = nullptr;
}

void swBlockingProgress::SetProgress(int Value, int Maximum)
{
	if (mNested)
		return;

	if (!mDialog)
	{
		// Short tasks finish without a dialog flashing on screen.
		if (Value >= Maximum || mTimer.elapsed() < 400)
			return;

		mDialog = new QProgressDialog(QApplication::activeWindow());
		mDialog->setWindowTitle(mTitle);
		mDialog->setLabelText(mLabel);
		mDialog->setWindowModality(Qt::ApplicationModal);
		mDialog->setMinimumDuration(0);
		mDialog->setAutoClose(false);
		mDialog->setAutoReset(false);

		QProgressBar* Bar = new QProgressBar(mDialog);
		Bar->setFormat(tr("%v of %m"));
		mDialog->setBar(Bar);

		if (!mCancelable)
		{
			mDialog->setCancelButton(nullptr);
			mDialog->setWindowFlags(mDialog->windowFlags() & ~Qt::WindowCloseButtonHint);
		}

		mDialog->setRange(0, Maximum);
		mDialog->show();
		mUpdateTimer.invalidate();
	}

	// Updates are throttled, each one runs the event loop to repaint the dialog.
	if (mUpdateTimer.isValid() && mUpdateTimer.elapsed() < 50 && Value < Maximum)
		return;

	mUpdateTimer.start();
	mDialog->setMaximum(Maximum);
	mDialog->setValue(Value);
}

bool swBlockingProgress::IsCanceled() const
{
	return mDialog && mDialog->wasCanceled();
}

QSize swGetPanelButtonSize()
{
	// Every panel button is sized after the options gear button of a panel toolbar.
	static QSize ButtonSize;

	if (!ButtonSize.isValid())
	{
		QToolBar ToolBar;
		QToolButton* Button = new QToolButton(&ToolBar);

		Button->setIcon(swCreatePaintedIcon(swPaintedIcon::Gear));
		Button->setAutoRaise(true);
		ToolBar.addWidget(Button);

		ButtonSize = Button->sizeHint();
	}

	return ButtonSize;
}

void swSetupPanelButton(QToolButton* Button)
{
	// Panel buttons are outlined, icon buttons are as large as the gear and text buttons as tall.
	const QSize ButtonSize = swGetPanelButtonSize();

	Button->setAutoRaise(false);
	Button->setFocusPolicy(Qt::NoFocus);
	Button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

	if (Button->toolButtonStyle() == Qt::ToolButtonIconOnly)
		Button->setFixedSize(ButtonSize);
	else
	{
		Button->setMinimumWidth(0);
		Button->setMaximumWidth(QWIDGETSIZE_MAX);
		Button->setFixedHeight(ButtonSize.height());
	}
}

void swSetupPanelEdit(QWidget* Edit)
{
	// Text boxes in panel toolbars are as tall as the buttons so the rows have even spacing.
	Edit->setFixedHeight(swGetPanelButtonSize().height());
}

// use to select the whole value on a click anywhere in a spin box so typing replaces it
class swSpinBoxSelectFilter : public QObject
{
public:
	using QObject::QObject;

	bool eventFilter(QObject* Object, QEvent* Event) override
	{
		QLineEdit* LineEdit = qobject_cast<QLineEdit*>(Object);

		if (LineEdit && Event->type() == QEvent::MouseButtonRelease && static_cast<QMouseEvent*>(Event)->button() == Qt::LeftButton && !LineEdit->hasSelectedText())
		{
			// a dragged selection stays, a click inside an old selection clears it after the release
			QAbstractSpinBox* SpinBox = static_cast<QAbstractSpinBox*>(parent());
			SpinBox->selectAll();

			QTimer::singleShot(0, LineEdit, [LineEdit, SpinBox]()
			{
				if (!LineEdit->hasSelectedText())
					SpinBox->selectAll();
			});
		}

		return QObject::eventFilter(Object, Event);
	}
};

void swSetupSpinBoxSelection(QAbstractSpinBox* SpinBox)
{
	// the clicks go to the text inside the spin box
	if (SpinBox->property("SelectOnClick").toBool())
		return;

	SpinBox->setProperty("SelectOnClick", true);
	swSpinBoxSelectFilter* Filter = new swSpinBoxSelectFilter(SpinBox);

	if (QLineEdit* LineEdit = SpinBox->findChild<QLineEdit*>(QString(), Qt::FindDirectChildrenOnly))
		LineEdit->installEventFilter(Filter);
}

void swSetupPanelSpinBox(QAbstractSpinBox* SpinBox)
{
	swSetupSpinBoxSelection(SpinBox);

	// Equal stretch gives the boxes of a row the same width whatever their text.
	QSizePolicy SizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	SizePolicy.setHorizontalStretch(1);
	SpinBox->setSizePolicy(SizePolicy);
	swUpdatePanelSpinBoxWidth(SpinBox);
	swSetupPanelEdit(SpinBox);
}

void swUpdatePanelSpinBoxWidth(QAbstractSpinBox* SpinBox)
{
	// Spin boxes are wide enough for their largest value with the name and unit shown in them.
	QString Prefix, Suffix, Value = QLatin1String("-000");

	if (const QDoubleSpinBox* DoubleSpinBox = qobject_cast<const QDoubleSpinBox*>(SpinBox))
	{
		Prefix = DoubleSpinBox->prefix();
		Suffix = DoubleSpinBox->suffix();
		Value = DoubleSpinBox->textFromValue(DoubleSpinBox->maximum());
	}
	else if (const QSpinBox* IntegerSpinBox = qobject_cast<const QSpinBox*>(SpinBox))
	{
		Prefix = IntegerSpinBox->prefix();
		Suffix = IntegerSpinBox->suffix();
		Value = QString::number(IntegerSpinBox->maximum());
	}

	if (Value.size() < 4)
		Value = QLatin1String("-000");

	// unbounded ranges size for a typical value so the panel can still shrink
	const bool Unbounded = Value.size() > 8;

	if (Unbounded)
		Value = QLatin1String("-0000.00");

	QStyleOptionSpinBox Option;
	Option.initFrom(SpinBox);
	Option.rect = QRect(QPoint(0, 0), SpinBox->sizeHint());
	Option.subControls = QStyle::SC_SpinBoxUp | QStyle::SC_SpinBoxDown | QStyle::SC_SpinBoxFrame | QStyle::SC_SpinBoxEditField;
	Option.frame = SpinBox->hasFrame();

	const QRect EditRect = SpinBox->style()->subControlRect(QStyle::CC_SpinBox, &Option, QStyle::SC_SpinBoxEditField, SpinBox);
	const int Decoration = Option.rect.width() - EditRect.width();

	// the style sizes the box for its limits too, the larger width keeps the text clear of the arrows, a shared column keeps its width
	const int TextWidth = SpinBox->fontMetrics().horizontalAdvance(Prefix + Value + Suffix) + Decoration;
	const int Width = Unbounded ? TextWidth : qMax(TextWidth, SpinBox->sizeHint().width());
	SpinBox->setMinimumWidth(qMax(Width, SpinBox->property("swColumnWidth").toInt()));
}

int swGetLayoutSpacing(const QWidget* Widget, Qt::Orientation Orientation)
{
	// The spacing a standard layout uses, for widgets that place their children themselves.
	QStyle* Style = Widget->style();
	const int Spacing = Style->pixelMetric(Orientation == Qt::Vertical ? QStyle::PM_LayoutVerticalSpacing : QStyle::PM_LayoutHorizontalSpacing, nullptr, Widget);

	return Spacing >= 0 ? Spacing : Style->layoutSpacing(QSizePolicy::DefaultType, QSizePolicy::DefaultType, Orientation, nullptr, Widget);
}

QWidget* swCreatePanelRow(QWidget* Parent)
{
	// The row sits inside the panel layout, which provides the margins, the row keeps the standard spacing.
	QWidget* Row = new QWidget(Parent);
	QHBoxLayout* Layout = new QHBoxLayout(Row);
	Layout->setContentsMargins(0, 0, 0, 0);

	return Row;
}

void swAddPanelRowWidget(QWidget* Row, QWidget* Widget)
{
	Row->layout()->addWidget(Widget);
}

void swAddPanelRowStretch(QWidget* Row)
{
	static_cast<QHBoxLayout*>(Row->layout())->addStretch(1);
}

QToolButton* swAddPanelActionButton(QWidget* Row, QAction* Action)
{
	QToolButton* Button = new QToolButton(Row);

	Button->setDefaultAction(Action);
	swAddPanelRowWidget(Row, Button);
	swSetupPanelButton(Button);

	return Button;
}

QToolButton* swAddPanelOptionsButton(QWidget* Row, QMenu* Menu, const QString& ToolTip, const QString& AccessibleName)
{
	QToolButton* OptionsButton = new QToolButton(Row);

	OptionsButton->setIcon(swCreatePaintedIcon(swPaintedIcon::Gear));
	OptionsButton->setToolTip(ToolTip);
	OptionsButton->setAccessibleName(AccessibleName);
	OptionsButton->setPopupMode(QToolButton::InstantPopup);
	OptionsButton->setMenu(Menu);
	swAddPanelRowWidget(Row, OptionsButton);
	swSetupPanelButton(OptionsButton);

	return OptionsButton;
}

void swSetupWidgetHelp(QWidget* Root, QDialogButtonBox* ButtonBox)
{
	const auto ShowHelp = [Root](QWidget* Widget)
	{
		while (Widget && Widget->whatsThis().isEmpty() && Widget != Root)
			Widget = Widget->parentWidget();

		if (Widget && !Widget->whatsThis().isEmpty())
			QWhatsThis::showText(Widget->mapToGlobal(QPoint(0, Widget->height())), Widget->whatsThis(), Widget);
	};

	if (ButtonBox)
		QObject::connect(ButtonBox, &QDialogButtonBox::helpRequested, Root, [Root, ShowHelp]()
		{
			ShowHelp(Root);
		});

	QShortcut* HelpShortcut = new QShortcut(QKeySequence(Qt::Key_F1), Root);
	HelpShortcut->setContext(Qt::WidgetWithChildrenShortcut);

	const auto HelpActivated = [Root, ShowHelp]()
	{
		QWidget* Focus = QApplication::focusWidget();
		// Popup windows can share the shortcut scope across a QWidget window boundary.
		ShowHelp(Focus ? Focus : Root);
	};

	QObject::connect(HelpShortcut, &QShortcut::activated, Root, HelpActivated);
	// A chooser can overlap its owning dock's shortcut scope; both routes start at the focused control.
	QObject::connect(HelpShortcut, &QShortcut::activatedAmbiguously, Root, HelpActivated);
}

void swSetupDialogHelp(QDialog* Dialog, QDialogButtonBox* ButtonBox)
{
	swSetupWidgetHelp(Dialog, ButtonBox);
}

QString swFormatValue(float Value, int Precision)
{
	QString String = QString::number(Value, 'f', Precision);
	const qsizetype Dot = String.indexOf('.');

	if (Dot != -1)
	{
		while (String.endsWith('0'))
			String.chop(1);

		if (String.endsWith('.'))
			String.chop(1);
	}

	return String;
}

QString swFormatFloatLocalized(float Value, QLocale Locale)
{
	Locale.setNumberOptions(Locale.numberOptions() | QLocale::OmitGroupSeparator);
	QString DecimalPoint = Locale.decimalPoint();
	QString String = Locale.toString(Value, 'f', 4);

	if (String.indexOf(DecimalPoint) != -1)
	{
		while (String.endsWith(Locale.zeroDigit()))
			String.chop(1);

		if (String.endsWith(DecimalPoint))
			String.chop(1);
	}

	return String;
}

QString swFormatDoubleLocalized(double Value, QLocale Locale, char Format, int Precision)
{
	Locale.setNumberOptions(Locale.numberOptions() | QLocale::OmitGroupSeparator);
	return Locale.toString(Value, Format, Precision);
}

QString swFormatIntegerLocalized(qlonglong Value, QLocale Locale)
{
	Locale.setNumberOptions(Locale.numberOptions() | QLocale::OmitGroupSeparator);
	return Locale.toString(Value);
}

std::optional<float> swParseFloatLocalized(const QString& Value, QLocale Locale)
{
	bool Ok;
	const float Number = Locale.toFloat(Value, &Ok);

	if (!Ok || !std::isfinite(Number))
		return std::nullopt;

	return Number;
}

#ifdef Q_OS_WIN

int swTerminateChildProcess(QWidget* Parent, const qint64 Pid, const qint64 Ppid)
{
	DWORD PID        = DWORD(Pid);
	DWORD PPID       = DWORD(Ppid);
	HANDLE hSnapshot = INVALID_HANDLE_VALUE, hProcess = INVALID_HANDLE_VALUE;
	PROCESSENTRY32 PE32;

	if ((hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, PPID)) == INVALID_HANDLE_VALUE)
	{
		QMessageBox::warning(Parent, QObject::tr("Stop Blender Process"), QString("%1 failed: %1").arg("CreateToolhelp32Snapshot").arg(GetLastError()));
		return -1;
	}
	PE32.dwSize = sizeof(PROCESSENTRY32);
	if (Process32First(hSnapshot, &PE32) == FALSE)
	{
		QMessageBox::warning(Parent, QObject::tr("Stop Blender Process"), QString("%1 failed: %2").arg("Process32First").arg(GetLastError()));
		CloseHandle(hSnapshot);
		return -2;
	}
	do
	{
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
		QRegularExpression ExeExpression("^(?:cmd\\.exe|conhost\\.exe|blender\\.exe)$", QRegularExpression::CaseInsensitiveOption);
#else
		QRegExp ExeExpression("^(?:cmd\\.exe|conhost\\.exe|blender\\.exe)$", Qt::CaseInsensitive);
#endif

		if (QString::fromWCharArray(PE32.szExeFile).contains(ExeExpression))
		{
			if ((PE32.th32ProcessID == PID && PE32.th32ParentProcessID == PPID) || PE32.th32ParentProcessID == PID)
			{
				if ((hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, PE32.th32ProcessID)) == INVALID_HANDLE_VALUE)
				{
					QMessageBox::warning(Parent, QObject::tr("Stop Blender Process"), QObject::tr("%1 failed: %2").arg("OpenProcess").arg(GetLastError()));
					return -3;
				}
				else
				{
					TerminateProcess(hProcess, 9);
					CloseHandle(hProcess);
				}
			}
		}
	}
	while (Process32Next(hSnapshot, &PE32));

	CloseHandle(hSnapshot);

	return 0;
}

bool swRunElevatedProcess(const LPCWSTR ExeName, const LPCWSTR Arguments, const LPCWSTR WorkingFolder)
{
	SHELLEXECUTEINFO ShellExecuteInfo = {};

	ShellExecuteInfo.cbSize = sizeof(SHELLEXECUTEINFO);
	ShellExecuteInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
	ShellExecuteInfo.hwnd = NULL;
	ShellExecuteInfo.lpVerb = L"runas";
	ShellExecuteInfo.lpFile = ExeName;
	ShellExecuteInfo.lpParameters = Arguments;
	ShellExecuteInfo.lpDirectory = WorkingFolder;
	ShellExecuteInfo.nShow = SW_SHOW;
	ShellExecuteInfo.hInstApp = NULL;

	if (!ShellExecuteEx(&ShellExecuteInfo) || !ShellExecuteInfo.hProcess)
		return false;

	if (WaitForSingleObject(ShellExecuteInfo.hProcess, INFINITE))
		return false;

	DWORD ExitCode;

	if (!GetExitCodeProcess(ShellExecuteInfo.hProcess, &ExitCode))
		return false;

	CloseHandle(ShellExecuteInfo.hProcess);

	return ExitCode == 0;
}

#endif

// Resize all columns to content except for one stretching column. (taken from QT creator)
swTreeWidgetColumnStretcher::swTreeWidgetColumnStretcher(QTreeWidget* TreeWidget, int ColumnToStretch)
	: QObject(TreeWidget->header()), mColumnToStretch(ColumnToStretch), mInteractiveResize(false), mStretchWidth(0)
{
	parent()->installEventFilter(this);
	connect(TreeWidget->header(), &QHeaderView::sectionResized, this, &swTreeWidgetColumnStretcher::SectionResized);

	QHideEvent Fake;
	swTreeWidgetColumnStretcher::eventFilter(parent(), &Fake);
}

void swTreeWidgetColumnStretcher::SectionResized(int LogicalIndex, int OldSize, int NewSize)
{
	Q_UNUSED(OldSize)

	if (LogicalIndex == mColumnToStretch)
	{
		QHeaderView* HeaderView = qobject_cast<QHeaderView*>(parent());

		if (HeaderView->isVisible())
			mInteractiveResize = true;

		mStretchWidth = NewSize;
	}
}

bool swTreeWidgetColumnStretcher::eventFilter(QObject* Object, QEvent* Event)
{
	if (Object == parent())
	{
		QHeaderView* HeaderView = qobject_cast<QHeaderView*>(Object);

		if (Event->type() == QEvent::Show)
		{
			for (int i = 0; i < HeaderView->count(); ++i)
				HeaderView->setSectionResizeMode(i, QHeaderView::Interactive);

			mStretchWidth = HeaderView->sectionSize(mColumnToStretch);

		}
		else if (Event->type() == QEvent::Hide)
		{
			if (!mInteractiveResize)
				for (int i = 0; i < HeaderView->count(); ++i)
					HeaderView->setSectionResizeMode(i, i == mColumnToStretch ? QHeaderView::Stretch : QHeaderView::ResizeToContents);
		}
		else if (Event->type() == QEvent::Resize)
		{
			if (HeaderView->sectionResizeMode(mColumnToStretch) == QHeaderView::Interactive) {

				const int StretchWidth = HeaderView->isVisible() ? mStretchWidth : 32;

				HeaderView->resizeSection(mColumnToStretch, StretchWidth);
			}
		}
	}

	return false;
}

swPartIdStringModel::swPartIdStringModel(swModel* Model, QObject* Parent)
	: QAbstractListModel(Parent)
{
	swPartsLibrary* Library = swGetPartsLibrary();
	mSortedParts = Library->GetParts();

	mSortedParts.erase(std::remove_if(mSortedParts.begin(), mSortedParts.end(), [Model](swPartData* PartData)
	{
		return PartData->IsModel() && PartData->GetModel()->IncludesModel(Model);
	}), mSortedParts.end());

	auto PartCompare = [](swPartData* PartData1, swPartData* PartData2)
	{
		return strcmp(PartData1->m_strDescription, PartData2->m_strDescription) < 0;
	};

	std::sort(mSortedParts.begin(), mSortedParts.end(), PartCompare);
}

QModelIndex swPartIdStringModel::Index(swPartData* PartData) const
{
	for (size_t PartDataIndex = 0; PartDataIndex < mSortedParts.size(); PartDataIndex++)
		if (mSortedParts[PartDataIndex] == PartData)
			return index(static_cast<int>(PartDataIndex), 0);

	return QModelIndex();
}

std::vector<bool> swPartIdStringModel::GetFilteredRows(const QString& FilterText) const
{
	const std::string Text = FilterText.toStdString();
	std::vector<bool> FilteredRows(mSortedParts.size());

	for (size_t PartDataIndex = 0; PartDataIndex < mSortedParts.size(); PartDataIndex++)
	{
		const swPartData* PartData = mSortedParts[PartDataIndex];

		FilteredRows[PartDataIndex] = (swstrcasestr(PartData->m_strDescription, Text.c_str()) || swstrcasestr(PartData->mFileName, Text.c_str()));
	}

	return FilteredRows;
}

int swPartIdStringModel::rowCount(const QModelIndex& Parent) const
{
	Q_UNUSED(Parent);

	return static_cast<int>(mSortedParts.size());
}

QVariant swPartIdStringModel::data(const QModelIndex& Index, int Role) const
{
	if (Index.row() < static_cast<int>(mSortedParts.size()))
	{
		if (Role == Qt::DisplayRole)
			return QString::fromLatin1(mSortedParts[Index.row()]->m_strDescription);
		else if (Role == Qt::UserRole)
			return QVariant::fromValue(reinterpret_cast<void*>(mSortedParts[Index.row()]));
	}

	return QVariant();
}

swColorDialogPopup::swColorDialogPopup(const QColor& InitialColor, bool ShowAlphaChannel, QWidget* Parent)
	: QWidget(Parent)
{
	setAccessibleName(tr("Choose Color"));
	setWhatsThis(tr("<p><b>Choose Color</b></p><p>Pick a color using the swatches or enter its component values. Choose OK to use the selected color, or Cancel to keep the original color. Editing the controls alone does not apply the color.</p>"));

	QVBoxLayout* Layout = new QVBoxLayout(this);
	Layout->setContentsMargins(0, 0, 0, 0);

	mColorDialog = new QColorDialog(InitialColor, this);
	mColorDialog->setWindowFlags(Qt::Widget);
	mColorDialog->setOptions(QColorDialog::DontUseNativeDialog | QColorDialog::NoButtons);
	mColorDialog->setOption(QColorDialog::ShowAlphaChannel, ShowAlphaChannel);

	// a color picked from a transparent start becomes opaque, the alpha channel can still make it transparent
	mTransparentStart = ShowAlphaChannel && InitialColor.alpha() == 0;

	QObject::connect(mColorDialog, &QColorDialog::currentColorChanged, this, [this](const QColor& Color)
	{
		if (!mTransparentStart || Color.alpha() != 0)
			return;

		mTransparentStart = false;

		QColor OpaqueColor(Color);
		OpaqueColor.setAlpha(255);
		mColorDialog->setCurrentColor(OpaqueColor);
	});

	Layout->addWidget(mColorDialog);
	mColorDialog->setAccessibleName(tr("Custom Color"));
	mColorDialog->setToolTip(tr("Choose a color or enter its component values."));

	QDialogButtonBox* ButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel|QDialogButtonBox::Help, this);
	Layout->addWidget(ButtonBox);
	swSetupWidgetHelp(this, ButtonBox);

	QObject::connect(ButtonBox, &QDialogButtonBox::accepted, this, &swColorDialogPopup::Accept);
	QObject::connect(ButtonBox, &QDialogButtonBox::rejected, this, &swColorDialogPopup::Reject);
}

void swColorDialogPopup::Accept()
{
	QColor Color = mColorDialog->currentColor();

	emit ColorSelected(Color);

	Close();
}

void swColorDialogPopup::Reject()
{
	Close();
}

void swColorDialogPopup::Close()
{
	QMenu* Menu = qobject_cast<QMenu*>(parent());

	if (Menu)
		Menu->close();
}

// use to draw the open or closed chevron of sections and the find bar like the tree branches
QIcon swCreateDisclosureIcon(bool Expanded, const QPalette& Palette)
{
	QPixmap Pixmap(16, 16);
	Pixmap.fill(Qt::transparent);

	QStyleOption Option;
	Option.rect = Pixmap.rect();
	Option.palette = Palette;
	Option.state = QStyle::State_Enabled;

	QPainter Painter(&Pixmap);
	QApplication::style()->drawPrimitive(Expanded ? QStyle::PE_IndicatorArrowDown : QStyle::PE_IndicatorArrowRight, &Option, &Painter);
	Painter.end();

	return QIcon(Pixmap);
}

// use to show a color as a square over a checkerboard that shows how transparent it is
QIcon swCreateColorIcon(const QColor& Color, int Size)
{
	QPixmap Pixmap(Size, Size);
	QPainter Painter(&Pixmap);

	const int CellSize = qMax(Size / 4, 2);

	for (int Y = 0; Y < Size; Y += CellSize)
		for (int X = 0; X < Size; X += CellSize)
			Painter.fillRect(X, Y, CellSize, CellSize, ((X + Y) / CellSize) % 2 ? QColor(204, 204, 204) : QColor(255, 255, 255));

	Painter.fillRect(Pixmap.rect(), Color);
	Painter.setPen(QColor(128, 128, 128));
	Painter.drawRect(Pixmap.rect().adjusted(0, 0, -1, -1));
	Painter.end();

	return QIcon(Pixmap);
}

// use to show a color on a button with its swatch and its name
void swUpdateColorButton(QToolButton* Button, const QColor& Color)
{
	Button->setIcon(swCreateColorIcon(Color, Button->iconSize().height()));
	Button->setText(Color.alpha() ? Color.name().toUpper() : QCoreApplication::translate("swInstructionsPropertiesWidget", "Transparent"));
}

// use to pick a color with its alpha channel below a button, an invalid color means the pick was canceled
QColor swPickColor(QToolButton* Button, const QColor& Color)
{
	QColor SelectedColor;

	QMenu Menu(Button);
	QWidgetAction* Action = new QWidgetAction(&Menu);
	swColorDialogPopup* Popup = new swColorDialogPopup(Color, true, &Menu);
	Action->setDefaultWidget(Popup);
	Menu.addAction(Action);

	QObject::connect(Popup, &swColorDialogPopup::ColorSelected, &Menu, [&SelectedColor](QColor PickedColor)
	{
		SelectedColor = PickedColor;
	});

	Menu.exec(Button->mapToGlobal(Button->rect().bottomLeft()));

	return SelectedColor;
}

// use to color interface icons with the text of the current palette
static QColor swGetInterfaceIconColor(QIcon::Mode Mode)
{
	const QPalette Palette = QApplication::palette();

	switch (Mode)
	{
	case QIcon::Disabled:
		return Palette.color(QPalette::Disabled, QPalette::WindowText);

	case QIcon::Selected:
		return Palette.color(QPalette::Active, QPalette::HighlightedText);

	case QIcon::Normal:
	case QIcon::Active:
		break;
	}

	return Palette.color(QPalette::Active, QPalette::WindowText);
}

// use to lay out the gear of the options buttons on a 32 pixel grid, small sizes get fewer teeth to stay readable
static QPainterPath swCreateGearPath(const QSizeF& Size)
{
	const qreal Extent = qMin(Size.width(), Size.height());
	const qreal Scale = Extent / 32.0;
	const bool Small = Extent <= 16.0;
	const int ToothCount = Small ? 6 : 8;
	const qreal RadiusOuter = 14.5 * Scale;
	const qreal RadiusRoot = 10.5 * Scale;
	const qreal RadiusHole = (Small ? 5.5 : 4.5) * Scale;
	const qreal AngleTip = Small ? 0.26 : 0.19;
	const qreal AngleBase = Small ? 0.40 : 0.30;
	const QPointF Center(Size.width() / 2.0, Size.height() / 2.0);

	QPolygonF Outline;

	for (int Tooth = 0; Tooth < ToothCount; Tooth++)
	{
		const qreal Angle = Tooth * 2.0 * M_PI / ToothCount;

		for (const QPointF& Corner : { QPointF(RadiusRoot, -AngleBase), QPointF(RadiusOuter, -AngleTip), QPointF(RadiusOuter, AngleTip), QPointF(RadiusRoot, AngleBase) })
			Outline << Center + QPointF(qCos(Angle + Corner.y()) * Corner.x(), qSin(Angle + Corner.y()) * Corner.x());
	}

	QPainterPath Path;
	Path.addPolygon(Outline);
	Path.closeSubpath();
	Path.addEllipse(Center, RadiusHole, RadiusHole);
	Path.setFillRule(Qt::OddEvenFill);

	return Path;
}

// use to lay out the folder of the open buttons on a 16 unit grid snapped to whole pixels
static QPainterPath swCreateFolderPath(const QSizeF& Size)
{
	const qreal Extent = qMin(Size.width(), Size.height());
	const qreal Unit = Extent / 16.0;
	const QPointF Origin((Size.width() - Extent) / 2.0, (Size.height() - Extent) / 2.0);
	const auto Snap = [Unit](qreal Value) { return qreal(qRound(Value * Unit)); };
	const qreal Radius = qRound(Unit) * 0.5;
	const qreal Left = Snap(1);
	const qreal Right = Snap(15);
	const qreal TabTop = Snap(2);
	const qreal TabRight = Snap(6);
	const qreal BackTop = Snap(3);
	const qreal GapTop = Snap(5);
	const qreal FrontTop = qMax(Snap(6), GapTop + 1.0);
	const qreal Bottom = Snap(14);

	// the tab and the back are split from the front by a gap
	QPainterPath Tab;
	Tab.addRoundedRect(QRectF(Left, TabTop, TabRight - Left, GapTop - TabTop), Radius, Radius);

	QPainterPath Back;
	Back.addRoundedRect(QRectF(Left, BackTop, Right - Left, GapTop - BackTop), Radius, Radius);

	QPainterPath Front;
	Front.addRoundedRect(QRectF(Left, FrontTop, Right - Left, Bottom - FrontTop), Radius, Radius);

	return Tab.united(Back).united(Front).translated(Origin);
}

// use to build an open line through points of the 24 unit icon grid
static QPainterPath swCreateIconLine(std::initializer_list<QPointF> Points, bool Closed = false)
{
	QPainterPath Path;
	QPolygonF Polygon(Points);

	Path.addPolygon(Polygon);

	if (Closed)
		Path.closeSubpath();

	return Path;
}

static QPainterPath swCreateIconCircle(qreal X, qreal Y, qreal Radius)
{
	QPainterPath Path;
	Path.addEllipse(QPointF(X, Y), Radius, Radius);

	return Path;
}

static QPainterPath swCreateIconRect(qreal Left, qreal Top, qreal Right, qreal Bottom, qreal Radius = 0.0)
{
	QPainterPath Path;
	Path.addRoundedRect(QRectF(Left, Top, Right - Left, Bottom - Top), Radius, Radius);

	return Path;
}

// use to point an open arrow head along a direction given in degrees clockwise from the right
static QPainterPath swCreateIconArrowHead(const QPointF& Tip, qreal Direction, qreal Length = 4.0)
{
	const qreal Back = qDegreesToRadians(Direction + 180.0);
	const qreal Spread = qDegreesToRadians(45.0);

	return swCreateIconLine({ Tip + QPointF(qCos(Back - Spread), qSin(Back - Spread)) * Length, Tip, Tip + QPointF(qCos(Back + Spread), qSin(Back + Spread)) * Length });
}

static QPainterPath swCreateIconArrow(const QPointF& Start, const QPointF& End, qreal Length = 4.0)
{
	const QPointF Delta = End - Start;
	QPainterPath Path = swCreateIconLine({ Start, End });

	Path.addPath(swCreateIconArrowHead(End, qRadiansToDegrees(qAtan2(Delta.y(), Delta.x())), Length));

	return Path;
}

// use to sweep an elliptic arc in degrees clockwise from the right, tilted around its center, with an optional arrow head at its end
static QPainterPath swCreateIconArc(qreal X, qreal Y, qreal RadiusX, qreal RadiusY, qreal Start, qreal Sweep, qreal Tilt = 0.0, qreal HeadLength = 0.0)
{
	QPainterPath Path;
	const QRectF Rect(-RadiusX, -RadiusY, RadiusX * 2.0, RadiusY * 2.0);

	Path.arcMoveTo(Rect, -Start);
	Path.arcTo(Rect, -Start, -Sweep);

	if (HeadLength > 0.0)
	{
		const qreal End = qDegreesToRadians(Start + Sweep);
		const QPointF Tip(qCos(End) * RadiusX, qSin(End) * RadiusY);
		const QPointF Tangent = QPointF(-qSin(End) * RadiusX, qCos(End) * RadiusY) * (Sweep < 0.0 ? -1.0 : 1.0);

		Path.addPath(swCreateIconArrowHead(Tip, qRadiansToDegrees(qAtan2(Tangent.y(), Tangent.x())), HeadLength));
	}

	QTransform Transform;
	Transform.translate(X, Y);
	Transform.rotate(Tilt);

	return Transform.map(Path);
}

// use to draw the strokes, fills and cut outs of a painted icon in one color
class swIconPainter
{
public:
	swIconPainter(QPainter* Painter, const QColor& Color)
		: mPainter(Painter), mColor(Color)
	{
	}

	void Stroke(const QPainterPath& Path, qreal Width = 2.0)
	{
		mPainter->strokePath(Path, QPen(mColor, Width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
	}

	// round joins soften the corners of filled shapes like the strokes
	void Fill(const QPainterPath& Path)
	{
		mPainter->fillPath(Path, mColor);
		Stroke(Path, 1.0);
	}

	void Erase(const QPainterPath& Path, qreal Width = 0.0)
	{
		const QPainter::CompositionMode Mode = mPainter->compositionMode();

		mPainter->setCompositionMode(QPainter::CompositionMode_Clear);

		if (Width > 0.0)
			mPainter->strokePath(Path, QPen(Qt::black, Width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
		else
			mPainter->fillPath(Path, Qt::black);

		mPainter->setCompositionMode(Mode);
	}

protected:
	QPainter* mPainter;
	QColor mColor;
};

// use to draw a painted icon centered in a size, the drawings are laid out on a 24 unit grid with 2 unit strokes
static void swDrawPaintedIcon(QPainter* Painter, swPaintedIcon Icon, const QSizeF& Size, const QColor& Color)
{
	Painter->save();
	Painter->setRenderHint(QPainter::Antialiasing);

	if (Icon == swPaintedIcon::Gear || Icon == swPaintedIcon::Folder)
	{
		Painter->fillPath(Icon == swPaintedIcon::Gear ? swCreateGearPath(Size) : swCreateFolderPath(Size), Color);
		Painter->restore();

		return;
	}

	const qreal Extent = qMin(Size.width(), Size.height());

	Painter->translate((Size.width() - Extent) / 2.0, (Size.height() - Extent) / 2.0);
	Painter->scale(Extent / 24.0, Extent / 24.0);

	swIconPainter Draw(Painter, Color);

	switch (Icon)
	{
	case swPaintedIcon::Gear:
	case swPaintedIcon::Folder:
	case swPaintedIcon::Count:
		break;

	case swPaintedIcon::FileNew:
		Draw.Stroke(swCreateIconLine({ { 14, 3 }, { 6, 3 }, { 6, 21 }, { 19, 21 }, { 19, 8 }, { 14, 3 }, { 14, 8 }, { 19, 8 } }));
		Draw.Stroke(swCreateIconLine({ { 12.5, 11 }, { 12.5, 17 } }));
		Draw.Stroke(swCreateIconLine({ { 9.5, 14 }, { 15.5, 14 } }));
		break;

	case swPaintedIcon::FileSave:
		Draw.Stroke(swCreateIconLine({ { 4, 4 }, { 16, 4 }, { 20, 8 }, { 20, 20 }, { 4, 20 } }, true));
		Draw.Stroke(swCreateIconLine({ { 8, 4 }, { 8, 9 }, { 15, 9 }, { 15, 4 } }));
		Draw.Stroke(swCreateIconLine({ { 7, 20 }, { 7, 14 }, { 17, 14 }, { 17, 20 } }));
		break;

	case swPaintedIcon::EditUndo:
	case swPaintedIcon::EditRedo:
	{
		QPainterPath Path = swCreateIconLine({ { 9, 4 }, { 4, 9 }, { 9, 14 } });
		Path.addPath(swCreateIconLine({ { 4, 9 }, { 14.5, 9 } }));
		Path.addPath(swCreateIconArc(14.5, 14.5, 5.5, 5.5, -90, 180));
		Path.addPath(swCreateIconLine({ { 14.5, 20 }, { 10, 20 } }));

		// redo mirrors undo
		if (Icon == swPaintedIcon::EditRedo)
			Path = QTransform(-1, 0, 0, 1, 24, 0).map(Path);

		Draw.Stroke(Path);
		break;
	}

	case swPaintedIcon::ActionInsert:
		Draw.Fill(swCreateIconCircle(12, 12, 10));
		Draw.Erase(swCreateIconLine({ { 12, 7 }, { 12, 17 } }), 2.5);
		Draw.Erase(swCreateIconLine({ { 7, 12 }, { 17, 12 } }), 2.5);
		break;

	case swPaintedIcon::ActionSelect:
		Draw.Fill(swCreateIconLine({ { 5, 4 }, { 20, 11 }, { 13, 13 }, { 11, 20 } }, true));
		Draw.Stroke(swCreateIconLine({ { 5, 4 }, { 20, 11 }, { 13, 13 }, { 11, 20 } }, true));
		break;

	case swPaintedIcon::ActionMove:
		Draw.Stroke(swCreateIconArrow({ 9, 15 }, { 9, 3 }));
		Draw.Stroke(swCreateIconArrow({ 9, 15 }, { 21, 15 }));
		Draw.Stroke(swCreateIconArrow({ 9, 15 }, { 3, 21 }, 3.5));
		break;

	case swPaintedIcon::ActionRotate:
		Draw.Stroke(swCreateIconArc(12, 12, 8.5, 8.5, -60, 300, 0, 4));
		Draw.Fill(swCreateIconCircle(12, 12, 2));
		break;

	case swPaintedIcon::ActionDelete:
		Draw.Stroke(swCreateIconLine({ { 3, 6 }, { 21, 6 } }));
		Draw.Stroke(swCreateIconLine({ { 9, 6 }, { 9, 3 }, { 15, 3 }, { 15, 6 } }));
		Draw.Stroke(swCreateIconLine({ { 5, 6 }, { 6, 21 }, { 18, 21 }, { 19, 6 } }));
		Draw.Stroke(swCreateIconLine({ { 10, 10 }, { 10, 17 } }));
		Draw.Stroke(swCreateIconLine({ { 14, 10 }, { 14, 17 } }));
		break;

	case swPaintedIcon::ActionPaint:
		Draw.Fill(swCreateIconRect(10, 2, 14, 9, 2));
		Draw.Fill(swCreateIconRect(5, 10, 19, 21, 1));
		Draw.Erase(swCreateIconLine({ { 5, 13 }, { 19, 13 } }), 1.5);
		Draw.Erase(swCreateIconLine({ { 9.5, 17 }, { 9.5, 23 } }), 1.5);
		Draw.Erase(swCreateIconLine({ { 14.5, 17 }, { 14.5, 23 } }), 1.5);
		break;

	case swPaintedIcon::ActionPickerColor:
		Draw.Stroke(swCreateIconLine({ { 13, 11 }, { 5, 19 } }));
		Draw.Fill(swCreateIconCircle(4, 20, 1.5));
		Draw.Stroke(swCreateIconLine({ { 9.5, 8.5 }, { 15.5, 14.5 } }), 2.5);
		Draw.Stroke(swCreateIconLine({ { 14, 10 }, { 18, 6 } }), 6);
		break;

	case swPaintedIcon::ActionZoom:
		Draw.Stroke(swCreateIconCircle(10, 10, 7));
		Draw.Stroke(swCreateIconLine({ { 15.5, 15.5 }, { 21, 21 } }), 3);
		Draw.Stroke(swCreateIconLine({ { 10, 7 }, { 10, 13 } }));
		Draw.Stroke(swCreateIconLine({ { 7, 10 }, { 13, 10 } }));
		break;

	case swPaintedIcon::ActionZoomRegion:
		Draw.Stroke(swCreateIconLine({ { 3, 8 }, { 3, 3 }, { 8, 3 } }));
		Draw.Stroke(swCreateIconLine({ { 16, 3 }, { 21, 3 }, { 21, 8 } }));
		Draw.Stroke(swCreateIconLine({ { 3, 16 }, { 3, 21 }, { 8, 21 } }));
		Draw.Stroke(swCreateIconCircle(12, 12, 4.5));
		Draw.Stroke(swCreateIconLine({ { 15.5, 15.5 }, { 21, 21 } }), 3);
		break;

	case swPaintedIcon::ActionPan:
	{
		QPainterPath Fingers;
		Fingers.addPath(swCreateIconLine({ { 7.5, 13 }, { 7.5, 7 } }));
		Fingers.addPath(swCreateIconLine({ { 11, 11 }, { 11, 4 } }));
		Fingers.addPath(swCreateIconLine({ { 14.5, 11 }, { 14.5, 5 } }));
		Fingers.addPath(swCreateIconLine({ { 18, 12 }, { 18, 8 } }));
		Fingers.addPath(swCreateIconLine({ { 7.5, 17 }, { 4, 13 } }));
		Draw.Stroke(Fingers, 3);

		QPainterPath Palm;
		Palm.moveTo(6, 13);
		Palm.lineTo(19.5, 11);
		Palm.lineTo(19.5, 15);
		Palm.cubicTo(19.5, 19, 17, 21.5, 13, 21.5);
		Palm.cubicTo(10, 21.5, 8, 20, 6, 17);
		Palm.closeSubpath();
		Draw.Fill(Palm);
		break;
	}

	case swPaintedIcon::ActionCamera:
		Draw.Stroke(swCreateIconRect(3, 10, 15, 20, 1.5));
		Draw.Stroke(swCreateIconLine({ { 15, 13.5 }, { 21, 10.5 }, { 21, 19.5 }, { 15, 16.5 } }, true));
		Draw.Stroke(swCreateIconCircle(6.5, 6, 2.5));
		Draw.Stroke(swCreateIconCircle(12.5, 6, 2.5));
		break;

	case swPaintedIcon::ActionViewRotate:
		Draw.Stroke(swCreateIconArc(12, 12, 10, 4.5, 20, 300, -35, 3.5));
		Draw.Fill(swCreateIconCircle(12, 12, 3));
		break;

	case swPaintedIcon::ActionRoll:
		Draw.Stroke(swCreateIconArc(12, 12, 8.5, 8.5, 200, 150, 0, 3.5));
		Draw.Stroke(swCreateIconArc(12, 12, 8.5, 8.5, 20, 150, 0, 3.5));
		break;

	case swPaintedIcon::LightPoint:
	{
		QPainterPath Bulb;
		Bulb.moveTo(9, 16);
		Bulb.cubicTo(9, 14, 5.5, 13, 5.5, 9.5);
		Bulb.arcTo(QRectF(5.5, 3, 13, 13), 180, -180);
		Bulb.cubicTo(18.5, 13, 15, 14, 15, 16);
		Bulb.closeSubpath();
		Draw.Stroke(Bulb);
		Draw.Stroke(swCreateIconLine({ { 9, 19 }, { 15, 19 } }));
		Draw.Stroke(swCreateIconLine({ { 10, 22 }, { 14, 22 } }));
		break;
	}

	case swPaintedIcon::LightSpot:
		Draw.Fill(swCreateIconRect(10, 2, 14, 5, 0.5));
		Draw.Stroke(swCreateIconLine({ { 10, 5 }, { 5, 13 }, { 19, 13 }, { 14, 5 } }, true));
		Draw.Stroke(swCreateIconLine({ { 12, 16 }, { 12, 21 } }));
		Draw.Stroke(swCreateIconLine({ { 7.5, 16 }, { 5.5, 20 } }));
		Draw.Stroke(swCreateIconLine({ { 16.5, 16 }, { 18.5, 20 } }));
		break;

	case swPaintedIcon::LightDirectional:
		Draw.Stroke(swCreateIconCircle(12, 12, 4));

		for (int Ray = 0; Ray < 8; Ray++)
		{
			const qreal Angle = qDegreesToRadians(Ray * 45.0);
			const QPointF Direction(qCos(Angle), qSin(Angle));

			Draw.Stroke(swCreateIconLine({ QPointF(12, 12) + Direction * 7.5, QPointF(12, 12) + Direction * 9.5 }));
		}
		break;

	case swPaintedIcon::LightArea:
		Draw.Stroke(swCreateIconRect(3, 3, 21, 10, 1));
		Draw.Stroke(swCreateIconLine({ { 7, 14 }, { 6, 20 } }));
		Draw.Stroke(swCreateIconLine({ { 12, 14 }, { 12, 21 } }));
		Draw.Stroke(swCreateIconLine({ { 17, 14 }, { 18, 20 } }));
		break;

	case swPaintedIcon::TimeFirst:
		Draw.Stroke(swCreateIconLine({ { 5, 5 }, { 5, 19 } }), 2.5);
		Draw.Fill(swCreateIconLine({ { 19, 5 }, { 19, 19 }, { 9, 12 } }, true));
		break;

	case swPaintedIcon::TimePrevious:
		Draw.Fill(swCreateIconLine({ { 15, 5 }, { 15, 19 }, { 5, 12 } }, true));
		Draw.Stroke(swCreateIconLine({ { 19, 5 }, { 19, 19 } }), 2.5);
		break;

	case swPaintedIcon::TimeNext:
		Draw.Stroke(swCreateIconLine({ { 5, 5 }, { 5, 19 } }), 2.5);
		Draw.Fill(swCreateIconLine({ { 9, 5 }, { 9, 19 }, { 19, 12 } }, true));
		break;

	case swPaintedIcon::TimeLast:
		Draw.Fill(swCreateIconLine({ { 5, 5 }, { 5, 19 }, { 15, 12 } }, true));
		Draw.Stroke(swCreateIconLine({ { 19, 5 }, { 19, 19 } }), 2.5);
		break;

	case swPaintedIcon::TimeKeysAdd:
		Draw.Stroke(swCreateIconLine({ { 10, 8 }, { 16, 14 }, { 10, 20 }, { 4, 14 } }, true));
		Draw.Stroke(swCreateIconLine({ { 18, 2 }, { 18, 9 } }));
		Draw.Stroke(swCreateIconLine({ { 14.5, 5.5 }, { 21.5, 5.5 } }));
		break;

	case swPaintedIcon::TransformTranslationAbsolute:
		Draw.Stroke(swCreateIconLine({ { 4, 3 }, { 4, 20 }, { 21, 20 } }));
		Draw.Stroke(swCreateIconArrow({ 4, 20 }, { 17, 7 }));
		break;

	case swPaintedIcon::TransformTranslationRelative:
		Draw.Stroke(swCreateIconRect(3, 15, 9, 21, 1));
		Draw.Stroke(swCreateIconArrow({ 9, 15 }, { 20, 4 }));
		break;

	case swPaintedIcon::TransformRotationAbsolute:
		Draw.Stroke(swCreateIconLine({ { 4, 3 }, { 4, 20 }, { 21, 20 } }));
		Draw.Stroke(swCreateIconArc(4, 20, 13, 13, 0, -80, 0, 3.5));
		break;

	case swPaintedIcon::TransformRotationRelative:
		Draw.Stroke(QTransform().translate(9, 15).rotate(-20).map(swCreateIconRect(-4, -4, 4, 4, 1)));
		Draw.Stroke(swCreateIconArc(9, 15, 11, 11, 15, -100, 0, 3.5));
		break;

	case swPaintedIcon::SnapMove:
		Draw.Fill(swCreateIconRect(9, 5, 15, 11, 1));
		Draw.Stroke(swCreateIconLine({ { 3, 16 }, { 21, 16 } }));
		Draw.Stroke(swCreateIconLine({ { 4, 16 }, { 4, 21 } }));
		Draw.Stroke(swCreateIconLine({ { 12, 16 }, { 12, 21 } }));
		Draw.Stroke(swCreateIconLine({ { 20, 16 }, { 20, 21 } }));
		Draw.Stroke(swCreateIconLine({ { 8, 16 }, { 8, 19 } }));
		Draw.Stroke(swCreateIconLine({ { 16, 16 }, { 16, 19 } }));
		break;

	case swPaintedIcon::SnapAngle:
		Draw.Stroke(swCreateIconLine({ { 4, 3 }, { 4, 20 }, { 21, 20 } }));

		for (int Step = 0; Step < 3; Step++)
			Draw.Stroke(swCreateIconArc(4, 20, 12, 12, -90 + Step * 33, 20));
		break;

	case swPaintedIcon::StatusWarning:
		Draw.Fill(swCreateIconLine({ { 12, 3 }, { 22, 20.5 }, { 2, 20.5 } }, true));
		Draw.Stroke(swCreateIconLine({ { 12, 3 }, { 22, 20.5 }, { 2, 20.5 } }, true));
		Draw.Erase(swCreateIconLine({ { 12, 9 }, { 12, 13.5 } }), 2.25);
		Draw.Erase(swCreateIconCircle(12, 17, 1.25));
		break;

	case swPaintedIcon::FilterCase:
		Draw.Stroke(swCreateIconLine({ { 3, 5 }, { 13, 5 } }));
		Draw.Stroke(swCreateIconLine({ { 8, 5 }, { 8, 20 } }));
		Draw.Stroke(swCreateIconLine({ { 14, 11 }, { 21, 11 } }));
		Draw.Stroke(swCreateIconLine({ { 17, 8 }, { 17, 18 }, { 18, 20 }, { 20, 20 } }));
		break;

	case swPaintedIcon::LibraryArchive:
		Draw.Stroke(swCreateIconRect(3, 4, 21, 9, 1));
		Draw.Stroke(swCreateIconLine({ { 5, 9 }, { 5, 20 }, { 19, 20 }, { 19, 9 } }));
		Draw.Stroke(swCreateIconLine({ { 10, 13 }, { 14, 13 } }));
		break;

	case swPaintedIcon::RenderPlaceholder:
		Draw.Stroke(swCreateIconRect(3, 4, 21, 20, 2));
		Draw.Fill(swCreateIconCircle(8, 7.5, 1.5));
		// the hills meet the straight sides above the rounded corners
		Draw.Stroke(swCreateIconLine({ { 3, 16 }, { 8, 11 }, { 13, 16 }, { 16, 13 }, { 21, 18 } }));
		break;

	case swPaintedIcon::ThumbnailFlexible:
	{
		QPainterPath Wave;
		Wave.moveTo(3, 12);
		Wave.cubicTo(6, 2, 9, 2, 12, 12);
		Wave.cubicTo(15, 22, 18, 22, 21, 12);
		Draw.Stroke(Wave);
		break;
	}

	case swPaintedIcon::ThumbnailTrackTrain:
		Draw.Stroke(swCreateIconLine({ { 8, 2 }, { 8, 22 } }));
		Draw.Stroke(swCreateIconLine({ { 16, 2 }, { 16, 22 } }));
		Draw.Stroke(swCreateIconLine({ { 4, 6 }, { 20, 6 } }));
		Draw.Stroke(swCreateIconLine({ { 4, 12 }, { 20, 12 } }));
		Draw.Stroke(swCreateIconLine({ { 4, 18 }, { 20, 18 } }));
		break;

	case swPaintedIcon::CursorBrick:
		Draw.Stroke(swCreateIconLine({ { 12, 3 }, { 20, 7.5 }, { 20, 16.5 }, { 12, 21 }, { 4, 16.5 }, { 4, 7.5 } }, true));
		Draw.Stroke(swCreateIconLine({ { 4, 7.5 }, { 12, 12 }, { 20, 7.5 } }));
		Draw.Stroke(swCreateIconLine({ { 12, 12 }, { 12, 21 } }));
		break;

	case swPaintedIcon::CursorMove:
		Draw.Stroke(swCreateIconArrow({ 12, 12 }, { 12, 3 }));
		Draw.Stroke(swCreateIconArrow({ 12, 12 }, { 21, 12 }));
		Draw.Stroke(swCreateIconArrow({ 12, 12 }, { 12, 21 }));
		Draw.Stroke(swCreateIconArrow({ 12, 12 }, { 3, 12 }));
		break;

	case swPaintedIcon::CursorMoveAxisX:
	case swPaintedIcon::CursorMoveAxisY:
	case swPaintedIcon::CursorMoveAxisZ:
	{
		// the axes follow the gizmo on screen, x 18 degrees down to the right, y 18 degrees up to the right and z up
		const qreal Angle = Icon == swPaintedIcon::CursorMoveAxisX ? 18.0 : Icon == swPaintedIcon::CursorMoveAxisY ? -18.0 : -90.0;
		const QPointF Direction = QPointF(qCos(qDegreesToRadians(Angle)), qSin(qDegreesToRadians(Angle))) * 9.0;

		Draw.Stroke(swCreateIconArrow({ 12, 12 }, QPointF(12, 12) + Direction));
		Draw.Stroke(swCreateIconArrow({ 12, 12 }, QPointF(12, 12) - Direction));
		Draw.Fill(swCreateIconCircle(12, 12, 2));
		break;
	}

	case swPaintedIcon::CursorRotateAxisX:
	case swPaintedIcon::CursorRotateAxisY:
	case swPaintedIcon::CursorRotateAxisZ:
	{
		// each ring stands across its axis
		const qreal Tilt = Icon == swPaintedIcon::CursorRotateAxisX ? -72.0 : Icon == swPaintedIcon::CursorRotateAxisY ? 72.0 : 0.0;

		Draw.Stroke(swCreateIconArc(12, 12, 9.5, 4, 30, 300, Tilt, 3.5));
		Draw.Fill(swCreateIconCircle(12, 12, 2));
		break;
	}

	case swPaintedIcon::CursorRotateX:
	case swPaintedIcon::CursorRotateY:
	{
		QPainterPath Path = swCreateIconArc(12, 24, 13, 13, -140, 100, 0, 3.5);
		Path.addPath(swCreateIconArc(12, 24, 13, 13, -40, -100, 0, 3.5));
		Path.translate(0, -1);

		// the vertical turn is the horizontal one on its side
		if (Icon == swPaintedIcon::CursorRotateY)
			Path = QTransform(0, 1, 1, 0, 0, 0).map(Path);

		Draw.Stroke(Path);
		break;
	}

	case swPaintedIcon::BadgeAdd:
		Draw.Stroke(swCreateIconLine({ { 12, 4 }, { 12, 20 } }), 4);
		Draw.Stroke(swCreateIconLine({ { 4, 12 }, { 20, 12 } }), 4);
		break;

	case swPaintedIcon::BadgeRemove:
		Draw.Stroke(swCreateIconLine({ { 4, 12 }, { 20, 12 } }), 4);
		break;
	}

	Painter->restore();
}

// use to draw the interface icons painted from a shape at any size in the text color
class swPaintedIconEngine : public QIconEngine
{
public:
	explicit swPaintedIconEngine(swPaintedIcon Icon)
		: mIcon(Icon)
	{
	}

	void paint(QPainter* Painter, const QRect& Rect, QIcon::Mode Mode, QIcon::State State) override
	{
		const qreal PixelRatio = Painter->device() ? Painter->device()->devicePixelRatioF() : 1.0;
		QPixmap Pixmap = pixmap(Rect.size() * PixelRatio, Mode, State);

		Pixmap.setDevicePixelRatio(PixelRatio);
		Painter->drawPixmap(Rect, Pixmap);
	}

	QPixmap pixmap(const QSize& Size, QIcon::Mode Mode, QIcon::State State) override
	{
		Q_UNUSED(State);

		const QColor Color = swGetInterfaceIconColor(Mode);
		const QString CacheKey = QString::fromLatin1("swPaintedIcon:%1:%2x%3:%4").arg(static_cast<int>(mIcon)).arg(Size.width()).arg(Size.height()).arg(Color.rgba(), 0, 16);
		QPixmap Pixmap;

		if (QPixmapCache::find(CacheKey, &Pixmap))
			return Pixmap;

		Pixmap = QPixmap(Size);
		Pixmap.fill(Qt::transparent);

		QPainter Painter(&Pixmap);
		swDrawPaintedIcon(&Painter, mIcon, Size, Color);
		Painter.end();

		QPixmapCache::insert(CacheKey, Pixmap);

		return Pixmap;
	}

	QIconEngine* clone() const override
	{
		return new swPaintedIconEngine(mIcon);
	}

protected:
	swPaintedIcon mIcon;
};

QIcon swCreatePaintedIcon(swPaintedIcon Icon)
{
	return QIcon(new swPaintedIconEngine(Icon));
}

// use to paint an icon into an image for cursors, thumbnails and previews
QImage swCreatePaintedIconImage(swPaintedIcon Icon, int Size, const QColor& Color)
{
	QImage Image(Size, Size, QImage::Format_ARGB32_Premultiplied);
	Image.fill(Qt::transparent);

	QPainter Painter(&Image);
	swDrawPaintedIcon(&Painter, Icon, QSizeF(Size, Size), Color);
	Painter.end();

	return Image;
}
