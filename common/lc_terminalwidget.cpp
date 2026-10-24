#include "lc_global.h"
#include "lc_terminalwidget.h"
#include <KParts/ReadOnlyPart>
#include <KPluginFactory>
#include <KPluginMetaData>
#include <KShell>
#include <kde_terminal_interface.h>

lcTerminalWidget::lcTerminalWidget(QWidget* Parent)
	: QWidget(Parent)
{
	mLayout = new QVBoxLayout(this);
	mLayout->setContentsMargins(0, 0, 0, 0);

	mDirectory = QDir::homePath();
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

void lcTerminalWidget::SetDirectory(const QString& Directory)
{
	if (Directory.isEmpty() || Directory == mDirectory)
		return;

	mDirectory = Directory;

	if (TerminalInterface* Terminal = qobject_cast<TerminalInterface*>(mPart))
		Terminal->sendInput(QStringLiteral(" cd %1\n").arg(KShell::quoteArg(mDirectory)));
}
