#include "sw_global.h"
#include "sw_findreplacewidget.h"
#include "sw_colorpicker.h"
#include "sw_mainwindow.h"
#include "sw_partselectionpopup.h"
#include "sw_piecedata.h"
#include "piece.h"
#include "sw_model.h"
#include "sw_view.h"
#include "sw_utils.h"

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

	mFindPartComboBox = new QComboBox(this);
	mFindPartComboBox->setToolTip(tr("Search Piece"));
	mFindPartComboBox->setEditable(true);
	mFindPartComboBox->setInsertPolicy(QComboBox::NoInsert);
	Layout->addWidget(mFindPartComboBox, 0, 2);

	QToolButton* FindNextButton = new QToolButton(this);
	FindNextButton->setAutoRaise(false);
	FindNextButton->setDefaultAction(gMainWindow->mActions[SW_EDIT_FIND_NEXT]);
	Layout->addWidget(FindNextButton, 0, 3);

	QToolButton* FindAllButton = new QToolButton(this);
	FindAllButton->setAutoRaise(false);
	FindAllButton->setDefaultAction(gMainWindow->mActions[SW_EDIT_FIND_ALL]);
	Layout->addWidget(FindAllButton, 0, 4);

	connect(FindColorPicker, &swColorPicker::ColorChanged, this, &swFindReplaceWidget::FindColorIndexChanged);
	connect(mFindPartComboBox->lineEdit(), &QLineEdit::returnPressed, gMainWindow->mActions[SW_EDIT_FIND_NEXT], &QAction::trigger);
	connect(mFindPartComboBox->lineEdit(), &QLineEdit::textEdited, this, &swFindReplaceWidget::FindTextEdited);
	connect(mFindPartComboBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), this, &swFindReplaceWidget::FindActivated);

	swColorPicker* ReplaceColorPicker = nullptr;

	if (Replace)
	{
		Layout->addWidget(new QLabel(tr("Replace:")), 1, 0);

		ReplaceColorPicker = new swColorPicker(this, true);
		ReplaceColorPicker->setToolTip(tr("Replacement Color"));
		Layout->addWidget(ReplaceColorPicker, 1, 1);

		QPixmap Pixmap(1, 1);
		Pixmap.fill(QColor::fromRgba64(0, 0, 0, 0));

		mReplacePartButton = new swElidableToolButton(this);
		mReplacePartButton->setToolTip(tr("Replacement Piece"));

		QSizePolicy PieceButtonSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
		PieceButtonSizePolicy.setHorizontalStretch(2);
		PieceButtonSizePolicy.setVerticalStretch(0);
		mReplacePartButton->setSizePolicy(PieceButtonSizePolicy);

		mReplacePartButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
		mReplacePartButton->setIcon(Pixmap);

		Layout->addWidget(mReplacePartButton, 1, 2);

		QToolButton* ReplaceNextButton = new QToolButton(this);
		ReplaceNextButton->setAutoRaise(false);
		ReplaceNextButton->setDefaultAction(gMainWindow->mActions[SW_EDIT_REPLACE_NEXT]);
		Layout->addWidget(ReplaceNextButton, 1, 3);

		QToolButton* ReplaceAllButton = new QToolButton(this);
		ReplaceAllButton->setAutoRaise(false);
		ReplaceAllButton->setDefaultAction(gMainWindow->mActions[SW_EDIT_REPLACE_ALL]);
		Layout->addWidget(ReplaceAllButton, 1, 4);

		connect(ReplaceColorPicker, &swColorPicker::ColorChanged, this, &swFindReplaceWidget::ReplaceColorIndexChanged);
		connect(mReplacePartButton, &swElidableToolButton::clicked, this, &swFindReplaceWidget::ReplaceButtonClicked);

		ReplaceColorPicker->SetCurrentColor(swGetColorIndex(SW_COLOR_NOCOLOR));
		mReplacePartButton->setText(tr("None"));
	}

	QToolButton* CloseButton = new QToolButton(this);
	CloseButton->setAutoRaise(false);
	CloseButton->setIcon(QIcon(":/stylesheet/close.svg"));
	CloseButton->setToolTip(tr("Close"));
	Layout->addWidget(CloseButton, 0, 5);

	connect(CloseButton, &QToolButton::clicked, this, &QObject::deleteLater);

	swPartsList PartsList;
	Model->GetPartsList(gDefaultColor, false, true, PartsList);

	for (const auto& PartIt : PartsList)
		mFindPartComboBox->addItem(QString::fromLatin1(PartIt.first->m_strDescription), QVariant::fromValue((void*)PartIt.first));
	mFindPartComboBox->model()->sort(0);

	swPiece* Focus = dynamic_cast<swPiece*>(Model->GetFocusObject());

	if (Focus)
	{
		swFindReplaceParams& Params = swView::GetFindReplaceParams();

		Params.FindPieceData = Focus->mPieceData.Get();
		Params.FindColorIndex = Focus->GetColorIndex();

		FindColorPicker->SetCurrentColor(Params.FindColorIndex);
		mFindPartComboBox->setCurrentIndex(mFindPartComboBox->findData(QVariant::fromValue((void*)Params.FindPieceData)));

	}
	else
	{
		FindColorPicker->SetCurrentColor(swGetColorIndex(SW_COLOR_NOCOLOR));
		mFindPartComboBox->setEditText(QString());
	}

	mFindPartComboBox->setFocus();
	adjustSize();
	show();
}

void swFindReplaceWidget::FindColorIndexChanged(int ColorIndex)
{
	swFindReplaceParams& Params = swView::GetFindReplaceParams();

	Params.FindColorIndex = ColorIndex;
}

void swFindReplaceWidget::FindTextEdited(const QString& Text)
{
	swFindReplaceParams& Params = swView::GetFindReplaceParams();

	Params.FindString = Text;
	Params.FindPieceData = nullptr;
}

void swFindReplaceWidget::FindActivated(int Index)
{
	swFindReplaceParams& Params = swView::GetFindReplaceParams();

	Params.FindString.clear();
	Params.FindPieceData = (swPieceData*)mFindPartComboBox->itemData(Index).value<void*>();
}

void swFindReplaceWidget::ReplaceColorIndexChanged(int ColorIndex)
{
	swFindReplaceParams& Params = swView::GetFindReplaceParams();

	Params.ReplaceColorIndex = ColorIndex;
}

void swFindReplaceWidget::ReplaceButtonClicked()
{
	swFindReplaceParams& Params = swView::GetFindReplaceParams();

	std::optional<swPieceData*> Result = swShowPartSelectionPopup(Params.ReplacePieceData, std::vector<std::pair<swPieceData*, std::string>>(),
                gDefaultColor, mReplacePartButton, mReplacePartButton->mapToGlobal(mReplacePartButton->rect().bottomLeft()));

	if (!Result.has_value())
		return;

	Params.ReplacePieceData = Result.value();
	mReplacePartButton->setText(Result.value()->m_strDescription);
}
