#include "sw_global.h"
#include "sw_widget_terminal.h"
#include "sw_window_utilities.h"
#include "sw_shortcuts.h"
#include "sw_window_main.h"
#include "sw_application.h"
#include <KParts/ReadOnlyPart>
#include <KPluginFactory>
#include <KPluginMetaData>
#include <KShell>
#include <KAcceleratorManager>
#include <kde_terminal_interface.h>

swTerminalWidget::swTerminalWidget(QWidget* Parent)
	: QWidget(Parent)
{
	mLayout = new QVBoxLayout(this);

	QWidget* OptionsRow = swCreatePanelRow(this);
	swAddPanelRowStretch(OptionsRow);
	mLayout->addWidget(OptionsRow);

	QAction* OpenFolderAction = gMainWindow->mActions[SW_VIEW_TERMINAL_OPEN_MODEL_FOLDER];
	QToolButton* OpenFolderButton = swAddPanelActionButton(OptionsRow, OpenFolderAction);
	OpenFolderButton->setWhatsThis(OpenFolderAction->whatsThis());

	QMenu* OptionsMenu = new QMenu(OptionsRow);
	swAddPanelOptionsButton(OptionsRow, OptionsMenu, tr("Configure the terminal."), tr("Terminal Options"));
	connect(OptionsMenu, &QMenu::aboutToShow, this, &swTerminalWidget::OptionsMenuAboutToShow);

	// float back at 121 by 25
	const QSize FloatingSize(1007, 511);
	Parent->setProperty("swFloatingSize", FloatingSize);

	mDirectory = QDir::homePath();

	qApp->installEventFilter(this);
}

bool swTerminalWidget::eventFilter(QObject* Object, QEvent* Event)
{
	const QEvent::Type Type = Event->type();

	// snap to whole cells on resize
	if (Type == QEvent::Resize && Object == mDisplay && !mSnapPending)
	{
		mSnapPending = true;
		QTimer::singleShot(0, this, &swTerminalWidget::SnapToCells);

		return false;
	}

	if (Type != QEvent::ShortcutOverride && Type != QEvent::MouseButtonPress && Type != QEvent::FocusIn && Type != QEvent::FocusOut)
		return false;

	QWidget* Widget = qobject_cast<QWidget*>(Object);

	if (!Widget || !isAncestorOf(Widget))
		return false;

	// keys only after a click
	if (Type == QEvent::MouseButtonPress)
	{
		mClickFocused = true;
		return false;
	}

	if (Type == QEvent::FocusOut)
	{
		const Qt::FocusReason Reason = static_cast<QFocusEvent*>(Event)->reason();

		if (Reason != Qt::ActiveWindowFocusReason && Reason != Qt::PopupFocusReason)
			mClickFocused = false;

		return false;
	}

	if (Type == QEvent::FocusIn)
	{
		// focus moves before the press
		if (static_cast<QFocusEvent*>(Event)->reason() == Qt::MouseFocusReason)
			mClickFocused = true;

		if (!mClickFocused)
		{
			QPointer<QWidget> FocusWidget = Widget;

			QTimer::singleShot(0, this, [this, FocusWidget]()
			{
				if (FocusWidget && FocusWidget->hasFocus() && !mClickFocused)
					FocusWidget->clearFocus();
			});
		}

		return false;
	}

	// keep keys from shortcuts
	QKeyEvent* KeyEvent = static_cast<QKeyEvent*>(Event);
	const QKeySequence KeySequence(static_cast<int>(KeyEvent->modifiers() & ~Qt::KeypadModifier) | KeyEvent->key());

	for (const QString& Shortcut : gKeyboardShortcuts.mShortcuts)
	{
		if (!Shortcut.isEmpty() && QKeySequence(Shortcut).matches(KeySequence) == QKeySequence::ExactMatch)
		{
			Event->accept();
			return true;
		}
	}

	return false;
}

void swTerminalWidget::showEvent(QShowEvent* Event)
{
	// start the shell when first shown
	if (!mPart)
		CreateTerminal();

	QWidget::showEvent(Event);
}

void swTerminalWidget::CreateTerminal()
{
	const KPluginFactory::Result<KPluginFactory> Factory = KPluginFactory::loadFactory(KPluginMetaData(QStringLiteral("konsolepart")));

	if (!Factory)
	{
		QLabel* Label = new QLabel(tr("The Konsole terminal component is not installed."), this);
		Label->setAlignment(Qt::AlignCenter);
		mLayout->addWidget(Label);

		return;
	}

	mPart = Factory.plugin->create<KParts::ReadOnlyPart>(this);

	if (!mPart)
		return;

	// drop konsole accelerators
	const QList<QDockWidget*> DockWidgets = window()->findChildren<QDockWidget*>();

	for (QDockWidget* DockWidget : DockWidgets)
	{
		KAcceleratorManager::setNoAccel(DockWidget);
		DockWidget->setWindowTitle(DockWidget->windowTitle().remove('&'));
	}

	mLayout->addWidget(mPart->widget());
	setFocusProxy(mPart->widget());

	const QList<QWidget*> PartWidgets = mPart->widget()->findChildren<QWidget*>();

	for (QWidget* PartWidget : PartWidgets)
		if (PartWidget->inherits("Konsole::TerminalDisplay"))
			mDisplay = PartWidget;

	// at least four lines tall
	if (mDisplay)
		mDisplay->setMinimumHeight(4 * QFontMetrics(mDisplay->font()).height() + 2);

	if (TerminalInterface* Terminal = qobject_cast<TerminalInterface*>(mPart))
		Terminal->showShellInDir(mDirectory);

	// restart the exited shell
	connect(mPart, &QObject::destroyed, this, [this]()
	{
		if (isVisible())
			QTimer::singleShot(0, this, &swTerminalWidget::CreateTerminal);
	});
}

// use to fit the panel to whole cells
void swTerminalWidget::SnapToCells()
{
	mSnapPending = false;

	QDockWidget* DockWidget = qobject_cast<QDockWidget*>(parentWidget());
	QMainWindow* MainWindow = DockWidget ? qobject_cast<QMainWindow*>(DockWidget->parentWidget()) : nullptr;

	if (!mDisplay || !MainWindow || !DockWidget->isVisible())
		return;

	// add the margin and scroll bar
	const QFontMetrics Metrics(mDisplay->font());
	const QSize CellSize(Metrics.horizontalAdvance(QLatin1Char('M')), Metrics.height());
	QSize CellsSize = mDisplay->contentsRect().size() - QSize(2, 2);

	for (const QScrollBar* ScrollBar : mDisplay->findChildren<QScrollBar*>(QString(), Qt::FindDirectChildrenOnly))
		if (ScrollBar->isVisible() && ScrollBar->orientation() == Qt::Vertical)
			CellsSize.rwidth() -= ScrollBar->width();

	if (CellSize.isEmpty() || CellsSize.isEmpty())
		return;

	// use to round to whole cells
	auto GetDelta = [](int Length, int Cell)
	{
		const int Excess = Length % Cell;

		return Excess > Cell / 2 ? Cell - Excess : -Excess;
	};

	const QSize Delta(GetDelta(CellsSize.width(), CellSize.width()), GetDelta(CellsSize.height(), CellSize.height()));

	if (DockWidget->isFloating())
	{
		// resize floating cell by cell
		DockWidget->setBaseSize(DockWidget->size() - CellsSize);
		DockWidget->setSizeIncrement(CellSize);

		if (!Delta.isNull())
			DockWidget->resize(DockWidget->size() + Delta);
	}
	else
	{
		DockWidget->setBaseSize(QSize());
		DockWidget->setSizeIncrement(QSize());

		// snap only across the dock area
		const Qt::DockWidgetArea Area = MainWindow->dockWidgetArea(DockWidget);

		if ((Area == Qt::LeftDockWidgetArea || Area == Qt::RightDockWidgetArea) && Delta.width())
			MainWindow->resizeDocks({ DockWidget }, { DockWidget->width() + Delta.width() }, Qt::Horizontal);

		if ((Area == Qt::TopDockWidgetArea || Area == Qt::BottomDockWidgetArea) && Delta.height())
			MainWindow->resizeDocks({ DockWidget }, { DockWidget->height() + Delta.height() }, Qt::Vertical);
	}
}

void swTerminalWidget::OptionsMenuAboutToShow()
{
	QMenu* Menu = qobject_cast<QMenu*>(sender());
	Menu->clear();

	QAction* FollowModel = Menu->addAction(tr("Follow Model Folder"), this, []()
	{
		gMainWindow->HandleCommand(SW_VIEW_TERMINAL_TOGGLE_FOLLOW_MODEL);
	});
	FollowModel->setCheckable(true);
	FollowModel->setChecked(swGetPreferences().mTerminalFollowModel);

	Menu->addAction(tr("Go to Model Folder"), this, []()
	{
		gMainWindow->HandleCommand(SW_VIEW_TERMINAL_GO_TO_MODEL_FOLDER);
	});
}

void swTerminalWidget::SetDirectory(const QString& Directory, bool Force)
{
	if (Directory.isEmpty() || (Directory == mDirectory && !Force))
		return;

	mDirectory = Directory;

	if (TerminalInterface* Terminal = qobject_cast<TerminalInterface*>(mPart))
		Terminal->sendInput(QStringLiteral(" cd %1\n").arg(KShell::quoteArg(mDirectory)));
}
