#include "sw_global.h"
#include "sw_widget_find_replace.h"
#include "sw_widget_picker_color.h"
#include "sw_window_main.h"
#include "sw_widget_selection_part_popup.h"
#include "sw_data_part.h"
#include "sw_object_part.h"
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

	mFindPartComboBox = new QComboBox(this);
	mFindPartComboBox->setToolTip(tr("Search Part"));
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
		mReplacePartButton->setToolTip(tr("Replacement Part"));

		QSizePolicy PartButtonSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
		PartButtonSizePolicy.setHorizontalStretch(2);
		PartButtonSizePolicy.setVerticalStretch(0);
		mReplacePartButton->setSizePolicy(PartButtonSizePolicy);

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

		ReplaceColorPicker->SetCurrentColor(swGetColorIndex(SW_COLOR_NO_COLOR));
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

	swPart* Focus = dynamic_cast<swPart*>(Model->GetFocusObject());

	if (Focus)
	{
		swFindReplaceParameters& Params = swView::GetFindReplaceParams();

		Params.FindPartData = Focus->mPartData.Get();
		Params.FindColorIndex = Focus->GetColorIndex();

		FindColorPicker->SetCurrentColor(Params.FindColorIndex);
		mFindPartComboBox->setCurrentIndex(mFindPartComboBox->findData(QVariant::fromValue((void*)Params.FindPartData)));

	}
	else
	{
		FindColorPicker->SetCurrentColor(swGetColorIndex(SW_COLOR_NO_COLOR));
		mFindPartComboBox->setEditText(QString());
	}

	mFindPartComboBox->setFocus();
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
	Params.FindPartData = nullptr;
}

void swFindReplaceWidget::FindActivated(int Index)
{
	swFindReplaceParameters& Params = swView::GetFindReplaceParams();

	Params.FindString.clear();
	Params.FindPartData = (swPartData*)mFindPartComboBox->itemData(Index).value<void*>();
}

void swFindReplaceWidget::ReplaceColorIndexChanged(int ColorIndex)
{
	swFindReplaceParameters& Params = swView::GetFindReplaceParams();

	Params.ReplaceColorIndex = ColorIndex;
}

void swFindReplaceWidget::ReplaceButtonClicked()
{
	swFindReplaceParameters& Params = swView::GetFindReplaceParams();

	std::optional<swPartData*> Result = swShowPartSelectionPopup(Params.ReplacePartData, std::vector<std::pair<swPartData*, std::string>>(),
                gDefaultColor, mReplacePartButton, mReplacePartButton->mapToGlobal(mReplacePartButton->rect().bottomLeft()));

	if (!Result.has_value())
		return;

	Params.ReplacePartData = Result.value();
	mReplacePartButton->setText(Result.value()->m_strDescription);
}
