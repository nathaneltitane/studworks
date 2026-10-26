#include "sw_global.h"
#include "sw_collapsible_widget.h"
#include "sw_utilities.h"

QImage swCollapsibleWidgetButton::mExpandedIcon;
QImage swCollapsibleWidgetButton::mCollapsedIcon;

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

	if (mExpanded)
	{
		if (mExpandedIcon.isNull())
		{
			QImage Image(16, 16, QImage::Format::Format_ARGB32);
			Image.fill(QColor(0, 0, 0, 0));
			uint Color = palette().color(QPalette::Text).rgba();

			for (int y = 0; y < 8; y++)
				for (int x = y; x < 8 - y; x++)
					Image.setPixel(x + 4, y + 6, Color);

			mExpandedIcon = Image;
		}

		setIcon(QPixmap::fromImage(mExpandedIcon));
	}
	else
	{
		if (mCollapsedIcon.isNull())
		{
			QImage Image(16, 16, QImage::Format::Format_ARGB32);
			Image.fill(QColor(0, 0, 0, 0));
			uint Color = palette().color(QPalette::Text).rgba();

			for (int y = 0; y < 8; y++)
				for (int x = y; x < 8 - y; x++)
					Image.setPixel(y + 6, x + 4, Color);

			mCollapsedIcon = Image;
		}

		setIcon(QPixmap::fromImage(mCollapsedIcon));
	}
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
