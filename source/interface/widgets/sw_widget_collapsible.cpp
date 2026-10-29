#include "sw_global.h"
#include "sw_widget_collapsible.h"
#include "sw_window_utilities.h"

swCollapsibleWidgetButton::swCollapsibleWidgetButton(const QString& Title, QWidget* Parent)
	: QToolButton(Parent)
{
	setText(Title);
	setAutoRaise(false);
	setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Maximum);
	setFocusPolicy(Qt::NoFocus);
	setFixedHeight(swGetPanelButtonSize().height());

	UpdateIcon();

	connect(this, &QToolButton::clicked, this, &swCollapsibleWidgetButton::Clicked);
}

void swCollapsibleWidgetButton::Collapse()
{
	if (mExpanded)
		Clicked();
}

void swCollapsibleWidgetButton::SetCollapsible(bool Collapsible)
{
	mCollapsible = Collapsible;

	if (!mCollapsible && !mExpanded)
		Clicked();

	UpdateIcon();
}

void swCollapsibleWidgetButton::SetExpanded(bool Expanded)
{
	if (mExpanded == Expanded)
		return;

	mExpanded = Expanded;
	UpdateIcon();
}

void swCollapsibleWidgetButton::UpdateIcon()
{
	// Fixed headers show no expand arrow.
	if (!mCollapsible)
	{
		setIcon(QIcon());
		return;
	}

	setIcon(swCreateDisclosureIcon(mExpanded, palette()));
}

void swCollapsibleWidgetButton::Clicked()
{
	if (!mCollapsible && mExpanded)
		return;

	mExpanded = !mExpanded;
	UpdateIcon();

	emit StateChanged(mExpanded);
}

swCollapsibleWidget::swCollapsibleWidget(const QString& Title, QWidget* Parent)
	: QWidget(Parent)
{
	QVBoxLayout* Layout = new QVBoxLayout(this);
//	Layout->setSpacing(0);
	Layout->setContentsMargins(0, 0, 0, 0);

	QHBoxLayout* TitleLayout = new QHBoxLayout();
	TitleLayout->setContentsMargins(0, 0, 0, 0);
	Layout->addLayout(TitleLayout);

	mTitleButton = new swCollapsibleWidgetButton(Title, this);
	TitleLayout->addWidget(mTitleButton);

	connect(mTitleButton, &swCollapsibleWidgetButton::StateChanged, this, &swCollapsibleWidget::ButtonStateChanged);

	mChildWidget = new QWidget(this);
	Layout->addWidget(mChildWidget);
}

void swCollapsibleWidget::ButtonStateChanged(bool Expanded)
{
	mChildWidget->setVisible(Expanded);
}

void swCollapsibleWidget::Collapse()
{
	mTitleButton->Collapse();
}

void swCollapsibleWidget::SetChildLayout(QLayout* Layout)
{
	Layout->setContentsMargins(12, 0, 0, 0);
	mChildWidget->setLayout(Layout);
}
