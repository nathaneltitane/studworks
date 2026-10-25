#include "lc_global.h"
#include "lc_terminalwidget.h"
#include "lc_shortcuts.h"
#include "lc_mainwindow.h"
#include "lc_application.h"
#include <KParts/ReadOnlyPart>
#include <KPluginFactory>
#include <KPluginMetaData>
#include <KShell>
#include <KAcceleratorManager>
#include <kde_terminal_interface.h>

lcTerminalWidget::lcTerminalWidget(QWidget* Parent)
	: QWidget(Parent)
{
	mLayout = new QVBoxLayout(this);
	mLayout->setContentsMargins(0, 0, 0, 0);
	mLayout->setSpacing(0);

	QHBoxLayout* OptionsLayout = new QHBoxLayout();
	OptionsLayout->setContentsMargins(0, 0, 0, 0);
	OptionsLayout->addStretch(1);
	mLayout->addLayout(OptionsLayout);

	QToolButton* OptionsButton = new QToolButton(this);
	OptionsButton->setIcon(QIcon(":/resources/gear_in.png"));
	OptionsButton->setToolTip(tr("Configure the terminal."));
	OptionsButton->setAccessibleName(tr("Terminal Options"));
	OptionsButton->setPopupMode(QToolButton::InstantPopup);
	OptionsButton->setAutoRaise(true);
	OptionsLayout->addWidget(OptionsButton);

	QMenu* OptionsMenu = new QMenu(OptionsButton);
	OptionsButton->setMenu(OptionsMenu);
	connect(OptionsMenu, &QMenu::aboutToShow, this, &lcTerminalWidget::OptionsMenuAboutToShow);

	mDirectory = QDir::homePath();

	qApp->installEventFilter(this);
}

bool lcTerminalWidget::eventFilter(QObject* Object, QEvent* Event)
{
	if (Event->type() != QEvent::ShortcutOverride)
		return false;

	QWidget* Widget = qobject_cast<QWidget*>(Object);

	if (!Widget || !isAncestorOf(Widget))
		return false;

	// Keys typed in the terminal go to the shell instead of triggering LeoCAD shortcuts.
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

void lcTerminalWidget::showEvent(QShowEvent* Event)
{
	// The shell starts the first time the panel is shown.
	if (!mPart)
		CreateTerminal();

	QWidget::showEvent(Event);
}

void lcTerminalWidget::CreateTerminal()
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

	mLayout->addWidget(mPart->widget());
	setFocusProxy(mPart->widget());

	if (TerminalInterface* Terminal = qobject_cast<TerminalInterface*>(mPart))
		Terminal->showShellInDir(mDirectory);

	// Start a new shell when the current one exits.
	connect(mPart, &QObject::destroyed, this, [this]()
	{
		if (isVisible())
			QTimer::singleShot(0, this, &lcTerminalWidget::CreateTerminal);
	});
}

void lcTerminalWidget::OptionsMenuAboutToShow()
{
	QMenu* Menu = qobject_cast<QMenu*>(sender());
	Menu->clear();

	QAction* FollowModel = Menu->addAction(tr("Follow Model Folder"), this, []()
	{
		gMainWindow->HandleCommand(LC_VIEW_TERMINAL_TOGGLE_FOLLOW_MODEL);
	});
	FollowModel->setCheckable(true);
	FollowModel->setChecked(lcGetPreferences().mTerminalFollowModel);

	Menu->addAction(tr("Go to Model Folder"), this, []()
	{
		gMainWindow->HandleCommand(LC_VIEW_TERMINAL_GO_TO_MODEL_FOLDER);
	});
}

void lcTerminalWidget::SetDirectory(const QString& Directory, bool Force)
{
	if (Directory.isEmpty() || (Directory == mDirectory && !Force))
		return;

	mDirectory = Directory;

	if (TerminalInterface* Terminal = qobject_cast<TerminalInterface*>(mPart))
		Terminal->sendInput(QStringLiteral(" cd %1\n").arg(KShell::quoteArg(mDirectory)));
}
