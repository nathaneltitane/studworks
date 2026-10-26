#include "sw_global.h"
#include "sw_terminal_widget.h"
#include "sw_utilities.h"
#include "sw_shortcuts.h"
#include "sw_main_window.h"
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

	QMenu* OptionsMenu = new QMenu(OptionsRow);
	swAddPanelOptionsButton(OptionsRow, OptionsMenu, tr("Configure the terminal."), tr("Terminal Options"));
	connect(OptionsMenu, &QMenu::aboutToShow, this, &swTerminalWidget::OptionsMenuAboutToShow);

	mDirectory = QDir::homePath();

	qApp->installEventFilter(this);
}

bool swTerminalWidget::eventFilter(QObject* Object, QEvent* Event)
{
	if (Event->type() != QEvent::ShortcutOverride)
		return false;

	QWidget* Widget = qobject_cast<QWidget*>(Object);

	if (!Widget || !isAncestorOf(Widget))
		return false;

	// Keys typed in the terminal go to the shell instead of triggering StudWorks shortcuts.
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
	// The shell starts the first time the panel is shown.
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

	// the konsole part adds keyboard accelerators to the panel titles, which show as '&' when a panel floats
	const QList<QDockWidget*> DockWidgets = window()->findChildren<QDockWidget*>();

	for (QDockWidget* DockWidget : DockWidgets)
	{
		KAcceleratorManager::setNoAccel(DockWidget);
		DockWidget->setWindowTitle(DockWidget->windowTitle().remove('&'));
	}

	// Never shorter than two lines of the fixed width font so the prompt is not clipped.
	const QFontMetrics Metrics(QFontDatabase::systemFont(QFontDatabase::FixedFont));
	mPart->widget()->setMinimumHeight(2 * Metrics.lineSpacing() + 8);

	mLayout->addWidget(mPart->widget());
	setFocusProxy(mPart->widget());

	if (TerminalInterface* Terminal = qobject_cast<TerminalInterface*>(mPart))
		Terminal->showShellInDir(mDirectory);

	// Start a new shell when the current one exits.
	connect(mPart, &QObject::destroyed, this, [this]()
	{
		if (isVisible())
			QTimer::singleShot(0, this, &swTerminalWidget::CreateTerminal);
	});
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
