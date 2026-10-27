#include "sw_global.h"
#include "sw_widget_find_replace.h"
#include "sw_widget_picker_color.h"
#include "sw_window_main.h"
#include "sw_widget_selection_piece_popup.h"
#include "sw_data_piece.h"
#include "sw_object_piece.h"
#include "sw_model.h"
#include "sw_view.h"
#include "sw_window_utilities.h"

swFindReplaceWidget::swFindReplaceWidget(QWidget* Parent, swModel* Model, bool Replace)
	: QWidget(Parent)
{
	setAutoFillBackground(true);
	QPalette Palette = palette();
	Palette.setColor(QPalette::Window, QApplication::palette().color(QPalette::Button));
	setPalette(Palette);

	QGridLayout* Layout = new QGridLayout(this);
	Layout->setContentsMargins(5, 5, 5, 5);

	Layout->addWidget(new QLabel(tr("Find:")), 0, 0);

	swColorPicker* FindColorPicker = new swColorPicker(this, true);
	FindColorPicker->setToolTip(tr("Search Color"));
	Layout->addWidget(FindColorPicker, 0, 1);

	mFindPieceComboBox = new QComboBox(this);
	mFindPieceComboBox->setToolTip(tr("Search Piece"));
	mFindPieceComboBox->setEditable(true);
	mFindPieceComboBox->setInsertPolicy(QComboBox::NoInsert);
	Layout->addWidget(mFindPieceComboBox, 0, 2);

	QToolButton* FindNextButton = new QToolButton(this);
	FindNextButton->setAutoRaise(false);
	FindNextButton->setDefaultAction(gMainWindow->mActions[SW_EDIT_FIND_NEXT]);
	Layout->addWidget(FindNextButton, 0, 3);

	QToolButton* FindAllButton = new QToolButton(this);
	FindAllButton->setAutoRaise(false);
	FindAllButton->setDefaultAction(gMainWindow->mActions[SW_EDIT_FIND_ALL]);
	Layout->addWidget(FindAllButton, 0, 4);

	connect(FindColorPicker, &swColorPicker::ColorChanged, this, &swFindReplaceWidget::FindColorIndexChanged);
	connect(mFindPieceComboBox->lineEdit(), &QLineEdit::returnPressed, gMainWindow->mActions[SW_EDIT_FIND_NEXT], &QAction::trigger);
	connect(mFindPieceComboBox->lineEdit(), &QLineEdit::textEdited, this, &swFindReplaceWidget::FindTextEdited);
	connect(mFindPieceComboBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), this, &swFindReplaceWidget::FindActivated);

	swColorPicker* ReplaceColorPicker = nullptr;

	if (Replace)
	{
		Layout->addWidget(new QLabel(tr("Replace:")), 1, 0);

		ReplaceColorPicker = new swColorPicker(this, true);
		ReplaceColorPicker->setToolTip(tr("Replacement Color"));
		Layout->addWidget(ReplaceColorPicker, 1, 1);

		QPixmap Pixmap(1, 1);
		Pixmap.fill(QColor::fromRgba64(0, 0, 0, 0));

		mReplacePieceButton = new swElidableToolButton(this);
		mReplacePieceButton->setToolTip(tr("Replacement Piece"));

		QSizePolicy PieceButtonSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
		PieceButtonSizePolicy.setHorizontalStretch(2);
		PieceButtonSizePolicy.setVerticalStretch(0);
		mReplacePieceButton->setSizePolicy(PieceButtonSizePolicy);

		mReplacePieceButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
		mReplacePieceButton->setIcon(Pixmap);

		Layout->addWidget(mReplacePieceButton, 1, 2);

		QToolButton* ReplaceNextButton = new QToolButton(this);
		ReplaceNextButton->setAutoRaise(false);
		ReplaceNextButton->setDefaultAction(gMainWindow->mActions[SW_EDIT_REPLACE_NEXT]);
		Layout->addWidget(ReplaceNextButton, 1, 3);

		QToolButton* ReplaceAllButton = new QToolButton(this);
		ReplaceAllButton->setAutoRaise(false);
		ReplaceAllButton->setDefaultAction(gMainWindow->mActions[SW_EDIT_REPLACE_ALL]);
		Layout->addWidget(ReplaceAllButton, 1, 4);

		connect(ReplaceColorPicker, &swColorPicker::ColorChanged, this, &swFindReplaceWidget::ReplaceColorIndexChanged);
		connect(mReplacePieceButton, &swElidableToolButton::clicked, this, &swFindReplaceWidget::ReplaceButtonClicked);

		ReplaceColorPicker->SetCurrentColor(swGetColorIndex(SW_COLOR_NO_COLOR));
		mReplacePieceButton->setText(tr("None"));
	}

	QToolButton* CloseButton = new QToolButton(this);
	CloseButton->setAutoRaise(false);
	CloseButton->setIcon(QIcon(":/stylesheet/close.svg"));
	CloseButton->setToolTip(tr("Close"));
	Layout->addWidget(CloseButton, 0, 5);

	connect(CloseButton, &QToolButton::clicked, this, &QObject::deleteLater);

	swPiecesList PiecesList;
	Model->GetPiecesList(gDefaultColor, false, true, PiecesList);

	for (const auto& PieceIt : PiecesList)
		mFindPieceComboBox->addItem(QString::fromLatin1(PieceIt.first->m_strDescription), QVariant::fromValue((void*)PieceIt.first));
	mFindPieceComboBox->model()->sort(0);

	swPiece* Focus = dynamic_cast<swPiece*>(Model->GetFocusObject());

	if (Focus)
	{
		swFindReplaceParameters& Params = swView::GetFindReplaceParams();

		Params.FindPieceData = Focus->mPieceData.Get();
		Params.FindColorIndex = Focus->GetColorIndex();

		FindColorPicker->SetCurrentColor(Params.FindColorIndex);
		mFindPieceComboBox->setCurrentIndex(mFindPieceComboBox->findData(QVariant::fromValue((void*)Params.FindPieceData)));

	}
	else
	{
		FindColorPicker->SetCurrentColor(swGetColorIndex(SW_COLOR_NO_COLOR));
		mFindPieceComboBox->setEditText(QString());
	}

	mFindPieceComboBox->setFocus();
	adjustSize();
	show();
}

void swFindReplaceWidget::FindColorIndexChanged(int ColorIndex)
{
	swFindReplaceParameters& Params = swView::GetFindReplaceParams();

	Params.FindColorIndex = ColorIndex;
}

void swFindReplaceWidget::FindTextEdited(const QString& Text)
{
	swFindReplaceParameters& Params = swView::GetFindReplaceParams();

	Params.FindString = Text;
	Params.FindPieceData = nullptr;
}

void swFindReplaceWidget::FindActivated(int Index)
{
	swFindReplaceParameters& Params = swView::GetFindReplaceParams();

	Params.FindString.clear();
	Params.FindPieceData = (swPieceData*)mFindPieceComboBox->itemData(Index).value<void*>();
}

void swFindReplaceWidget::ReplaceColorIndexChanged(int ColorIndex)
{
	swFindReplaceParameters& Params = swView::GetFindReplaceParams();

	Params.ReplaceColorIndex = ColorIndex;
}

void swFindReplaceWidget::ReplaceButtonClicked()
{
	swFindReplaceParameters& Params = swView::GetFindReplaceParams();

	std::optional<swPieceData*> Result = swShowPieceSelectionPopup(Params.ReplacePieceData, std::vector<std::pair<swPieceData*, std::string>>(),
                gDefaultColor, mReplacePieceButton, mReplacePieceButton->mapToGlobal(mReplacePieceButton->rect().bottomLeft()));

	if (!Result.has_value())
		return;

	Params.ReplacePieceData = Result.value();
	mReplacePieceButton->setText(Result.value()->m_strDescription);
}
