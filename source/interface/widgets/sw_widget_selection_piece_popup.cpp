#include "sw_global.h"
#include "sw_widget_selection_piece_popup.h"
#include "sw_widget_selection_piece.h"
#include "sw_data_piece.h"
#include "sw_window_utilities.h"

swPieceSelectionPopup::swPieceSelectionPopup(swPieceData* InitialPart, QWidget* Parent)
	: QWidget(Parent), mInitialPart(InitialPart)
{
	setAccessibleName(tr("Choose Piece"));
	setWhatsThis(tr("<p><b>Choose Piece</b></p><p>Select a piece, then choose OK or double-click it to use it. Cancel keeps the original choice. Filter the list to find a piece; Options controls matching and list display. When None is available, select it to omit the piece.</p>"));

	QVBoxLayout* Layout = new QVBoxLayout(this);

	mPartSelectionWidget = new swPieceSelectionWidget(true, this);
	Layout->addWidget(mPartSelectionWidget);
	mPartSelectionWidget->SetOrientation(Qt::Horizontal);

	mPartSelectionWidget->SetDragEnabled(false);

	connect(mPartSelectionWidget, &swPieceSelectionWidget::PartPicked, this, &swPieceSelectionPopup::Accept);
	connect(mPartSelectionWidget, &swPieceSelectionWidget::LayoutChanged, this, &swPieceSelectionPopup::ResizeToContents, Qt::QueuedConnection);

	QDialogButtonBox* ButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Help, this);
	Layout->addWidget(ButtonBox);
	swSetupWidgetHelp(this, ButtonBox);

	QObject::connect(ButtonBox, &QDialogButtonBox::accepted, this, &swPieceSelectionPopup::Accept);
	QObject::connect(ButtonBox, &QDialogButtonBox::rejected, this, &swPieceSelectionPopup::Reject);
}

QSize swPieceSelectionPopup::GetPartsViewSizeHint(const swPieceSelectionListView* PartsView, const QSize& PopupSize) const
{
	const int WidthOverhead = PopupSize.width() - PartsView->sizeHint().width();
	const int ViewWidth = qMax(1, qMin(PopupSize.width(), maximumWidth()) - WidthOverhead);
	return PartsView->GetPreferredSize(ViewWidth);
}

QSize swPieceSelectionPopup::sizeHint() const
{
	QSize Size = QWidget::sizeHint();
	const swPieceSelectionListView* PartsView = mPartSelectionWidget->findChild<swPieceSelectionListView*>();
	if (!PartsView || PartsView->GetListModel()->GetIconSize() == swPieceSelectionListView::NoIconSize || PartsView->GetListModel()->IsListMode())
		return Size;

	const int RowHeight = PartsView->GetCellSize().height();
	if (RowHeight <= 0)
		return Size;

	const int Overhead = Size.height() - PartsView->sizeHint().height() + 2 * PartsView->frameWidth();
	const int ViewHeight = GetPartsViewSizeHint(PartsView, Size).height();
	const int PreferredHeight = Overhead + ViewHeight - 2 * PartsView->frameWidth();
	const int AvailableHeight = qMin(PreferredHeight, maximumHeight()) - Overhead;
	const int Rows = qMax(1, AvailableHeight / RowHeight);
	Size.setHeight(Overhead + Rows * RowHeight);

	return Size;
}

void swPieceSelectionPopup::showEvent(QShowEvent* ShowEvent)
{
	QWidget::showEvent(ShowEvent);

	mPartSelectionWidget->SetOrientation(Qt::Horizontal);
	mPartSelectionWidget->SetCurrentPart(mInitialPart);

	mPartSelectionWidget->FocusPartFilterWidget();
}

void swPieceSelectionPopup::SetHeightLimit(int Height)
{
	mHeightLimit = Height;
	UpdateHeightLimit();
}

void swPieceSelectionPopup::UpdateHeightLimit()
{
	int Height = mHeightLimit;
	const swPieceSelectionListView* PartsView = mPartSelectionWidget->findChild<swPieceSelectionListView*>();
	if (PartsView && PartsView->GetListModel()->GetIconSize() && !PartsView->GetListModel()->IsListMode())
	{
		const int RowHeight = PartsView->GetCellSize().height();
		if (RowHeight > 0)
		{
			const QSize PopupSize = QWidget::sizeHint();
			const int ViewHeight = GetPartsViewSizeHint(PartsView, PopupSize).height();
			const int Overhead = PopupSize.height() - PartsView->sizeHint().height() + 2 * PartsView->frameWidth();
			const int Rows = qMin(2, (ViewHeight - 2 * PartsView->frameWidth()) / RowHeight);
			Height = qMax(Height, Overhead + Rows * RowHeight);
		}
	}

	setMaximumHeight(Height);
	QMenu* Menu = qobject_cast<QMenu*>(parent());
	if (Menu)
	{
		QStyleOption Option;
		Option.initFrom(Menu);
		QStyle* Style = Menu->style();
		const int Margin = Style->pixelMetric(QStyle::PM_MenuPanelWidth, &Option, Menu) + Style->pixelMetric(QStyle::PM_MenuVMargin, &Option, Menu);
		const QSize MenuSize = Style->sizeFromContents(QStyle::CT_Menu, &Option, QSize(0, Height + 2 * Margin), Menu);
		Menu->setMaximumHeight(qMin(QWIDGETSIZE_MAX, MenuSize.height()));
	}
}

void swPieceSelectionPopup::Accept()
{
	mPickedPiece = mPartSelectionWidget->GetCurrentPart();
	mAccepted = true;

	Close();
}

void swPieceSelectionPopup::Reject()
{
	Close();
}

void swPieceSelectionPopup::ResizeToContents()
{
	QMenu* Menu = qobject_cast<QMenu*>(parent());
	if (!Menu || !Menu->isVisible())
		return;

	updateGeometry();
	UpdateHeightLimit();

	// Refresh QMenu's cached widget action geometry as well as its own size.
	for (QAction* Action : Menu->actions())
	{
		QWidgetAction* WidgetAction = qobject_cast<QWidgetAction*>(Action);
		if (WidgetAction && WidgetAction->defaultWidget() == this)
		{
			QActionEvent Event(QEvent::ActionChanged, Action);
			QCoreApplication::sendEvent(Menu, &Event);
			break;
		}
	}

	Menu->move(swGetPopupPosition(Menu->pos(), Menu->size(), Menu->pos()));
}

void swPieceSelectionPopup::Close()
{
	QMenu* Menu = qobject_cast<QMenu*>(parent());

	if (Menu)
		Menu->close();
}

std::optional<swPieceData*> swShowPieceSelectionPopup(swPieceData* InitialPart, const std::vector<std::pair<swPieceData*, std::string>>& CustomParts, int ColorIndex, QWidget* Parent, QPoint Position)
{
	std::unique_ptr<QMenu> Menu(new QMenu(Parent));
	QWidgetAction* Action = new QWidgetAction(Menu.get());
	swPieceSelectionPopup* Popup = new swPieceSelectionPopup(InitialPart, Menu.get());
	swPieceSelectionWidget* PartSelectionWidget = Popup->GetPartSelectionWidget();

	if (CustomParts.empty())
	{
		PartSelectionWidget->SetCategory(swPieceCategoryType::AllParts, 0);
		PartSelectionWidget->SetColorIndex(ColorIndex);
	}
	else
		PartSelectionWidget->SetCustomParts(CustomParts, ColorIndex);

	QScreen* PopupScreen = QGuiApplication::primaryScreen();
	for (QScreen* Screen : QGuiApplication::screens())
	{
		if (Screen->geometry().contains(Position))
		{
			PopupScreen = Screen;
			break;
		}
	}

	if (PopupScreen)
	{
		const QSize ScreenSize = PopupScreen->availableGeometry().size();
		const QSize MaximumSize(ScreenSize.width() * 3 / 5, ScreenSize.height() * 3 / 5);
		Popup->setMaximumWidth(MaximumSize.width());
		Menu->setMaximumWidth(MaximumSize.width());
		Popup->SetHeightLimit(MaximumSize.height());
	}

	Action->setDefaultWidget(Popup);
	Menu->addAction(Action);

	Menu->exec(Position);

	return Popup->GetPickedPart();
}
