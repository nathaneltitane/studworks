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
	// bar floats over the view
	setObjectName(QStringLiteral("FindReplaceBar"));
	setAttribute(Qt::WA_StyledBackground, true);
	setStyleSheet(QStringLiteral("#FindReplaceBar { background: palette(button); border: 1px solid palette(mid); border-radius: 3px; }"));

	// controls as tall as panel buttons
	const int ButtonHeight = swGetPanelButtonSize().height();
	const auto SetupRowControl = [ButtonHeight](QWidget* Control)
	{
		Control->setFixedHeight(ButtonHeight);
		Control->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	};

	QGridLayout* Layout = new QGridLayout(this);
	Layout->setContentsMargins(5, 5, 5, 5);

	// close button first
	QToolButton* CloseButton = new QToolButton(this);
	CloseButton->setAutoRaise(false);
	CloseButton->setIcon(QIcon(":/stylesheet/close.svg"));
	CloseButton->setToolTip(tr("Close"));
	CloseButton->setFocusPolicy(Qt::NoFocus);
	CloseButton->setFixedSize(ButtonHeight, ButtonHeight);
	Layout->addWidget(CloseButton, 0, 0);

	Layout->addWidget(new QLabel(tr("Find:")), 0, 1);

	swColorPicker* FindColorPicker = new swColorPicker(this, true);
	FindColorPicker->setToolTip(tr("Search Color"));
	FindColorPicker->setFixedSize(swGetPanelButtonSize());
	Layout->addWidget(FindColorPicker, 0, 2);

	mFindPartComboBox = new QComboBox(this);
	mFindPartComboBox->setToolTip(tr("Search Part"));
	mFindPartComboBox->setEditable(true);
	mFindPartComboBox->setInsertPolicy(QComboBox::NoInsert);

	// keep a steady box width
	mFindPartComboBox->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
	mFindPartComboBox->setMinimumContentsLength(16);
	mFindPartComboBox->lineEdit()->setPlaceholderText(tr("None"));

	// center the placeholder
	QLineEdit* FindLineEdit = mFindPartComboBox->lineEdit();
	const auto SetFindAlignment = [FindLineEdit](const QString& Text)
	{
		FindLineEdit->setAlignment(Text.isEmpty() ? Qt::AlignCenter : Qt::AlignLeft | Qt::AlignVCenter);
	};

	connect(FindLineEdit, &QLineEdit::textChanged, this, SetFindAlignment);
	SetFindAlignment(FindLineEdit->text());
	SetupRowControl(mFindPartComboBox);
	Layout->addWidget(mFindPartComboBox, 0, 3);

	QToolButton* FindNextButton = new QToolButton(this);
	FindNextButton->setAutoRaise(false);
	FindNextButton->setDefaultAction(gMainWindow->mActions[SW_EDIT_FIND_NEXT]);
	SetupRowControl(FindNextButton);
	Layout->addWidget(FindNextButton, 0, 4);

	QToolButton* FindAllButton = new QToolButton(this);
	FindAllButton->setAutoRaise(false);
	FindAllButton->setDefaultAction(gMainWindow->mActions[SW_EDIT_FIND_ALL]);
	SetupRowControl(FindAllButton);
	Layout->addWidget(FindAllButton, 0, 5);

	// chevron toggles the replace row
	QToolButton* ReplaceToggleButton = new QToolButton(this);
	ReplaceToggleButton->setAutoRaise(false);
	ReplaceToggleButton->setFocusPolicy(Qt::NoFocus);
	ReplaceToggleButton->setFixedSize(ButtonHeight, ButtonHeight);
	ReplaceToggleButton->setToolTip(tr("Show or hide the replace options."));
	ReplaceToggleButton->setAccessibleName(tr("Toggle Replace"));
	Layout->addWidget(ReplaceToggleButton, 0, 6);

	connect(FindColorPicker, &swColorPicker::ColorChanged, this, &swFindReplaceWidget::FindColorIndexChanged);
	connect(mFindPartComboBox->lineEdit(), &QLineEdit::returnPressed, gMainWindow->mActions[SW_EDIT_FIND_NEXT], &QAction::trigger);
	connect(mFindPartComboBox->lineEdit(), &QLineEdit::textEdited, this, &swFindReplaceWidget::FindTextEdited);
	connect(mFindPartComboBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), this, &swFindReplaceWidget::FindActivated);
	connect(CloseButton, &QToolButton::clicked, this, &QObject::deleteLater);

	QLabel* ReplaceLabel = new QLabel(tr("Replace:"), this);
	Layout->addWidget(ReplaceLabel, 1, 1);

	swColorPicker* ReplaceColorPicker = new swColorPicker(this, true);
	ReplaceColorPicker->setToolTip(tr("Replacement Color"));
	ReplaceColorPicker->setFixedSize(swGetPanelButtonSize());
	Layout->addWidget(ReplaceColorPicker, 1, 2);

	mReplacePartButton = new swElidableToolButton(this);
	mReplacePartButton->setToolTip(tr("Replacement Part"));
	SetupRowControl(mReplacePartButton);
	Layout->addWidget(mReplacePartButton, 1, 3);

	QToolButton* ReplaceNextButton = new QToolButton(this);
	ReplaceNextButton->setAutoRaise(false);
	ReplaceNextButton->setDefaultAction(gMainWindow->mActions[SW_EDIT_REPLACE_NEXT]);
	SetupRowControl(ReplaceNextButton);
	Layout->addWidget(ReplaceNextButton, 1, 4);

	QToolButton* ReplaceAllButton = new QToolButton(this);
	ReplaceAllButton->setAutoRaise(false);
	ReplaceAllButton->setDefaultAction(gMainWindow->mActions[SW_EDIT_REPLACE_ALL]);
	SetupRowControl(ReplaceAllButton);
	Layout->addWidget(ReplaceAllButton, 1, 5);

	connect(ReplaceColorPicker, &swColorPicker::ColorChanged, this, &swFindReplaceWidget::ReplaceColorIndexChanged);
	connect(mReplacePartButton, &swElidableToolButton::clicked, this, &swFindReplaceWidget::ReplaceButtonClicked);

	ReplaceColorPicker->SetCurrentColor(swGetColorIndex(SW_COLOR_NO_COLOR));
	mReplacePartButton->setText(tr("None"));

	// columns fit both rows
	for (int Column = 0; Column < Layout->columnCount(); Column++)
	{
		int ColumnWidth = 0;

		for (int Row = 0; Row < Layout->rowCount(); Row++)
			if (QLayoutItem* Item = Layout->itemAtPosition(Row, Column))
				ColumnWidth = qMax(ColumnWidth, Item->widget()->sizeHint().width());

		Layout->setColumnMinimumWidth(Column, ColumnWidth);
	}

	// submodel option spans the replace row
	QCheckBox* ReplaceSubModelsCheckBox = new QCheckBox(tr("Include Submodels"), this);
	ReplaceSubModelsCheckBox->setToolTip(tr("Replace All also replaces the matching parts inside every submodel used by this model."));
	ReplaceSubModelsCheckBox->setFocusPolicy(Qt::NoFocus);
	Layout->addWidget(ReplaceSubModelsCheckBox, 2, 3, 1, 3);

	connect(ReplaceSubModelsCheckBox, &QCheckBox::toggled, this, [](bool Checked)
	{
		swView::GetFindReplaceParams().ReplaceSubModels = Checked;
	});

	const QList<QWidget*> ReplaceControls = { ReplaceLabel, ReplaceColorPicker, mReplacePartButton, ReplaceNextButton, ReplaceAllButton, ReplaceSubModelsCheckBox };

	// use to toggle the replace row
	const auto SetReplaceVisible = [ReplaceToggleButton, ReplaceControls](bool Visible)
	{
		ReplaceToggleButton->setIcon(swCreateDisclosureIcon(Visible, ReplaceToggleButton->palette()));

		for (QWidget* Control : ReplaceControls)
			Control->setVisible(Visible);
	};

	connect(ReplaceToggleButton, &QToolButton::clicked, this, [this, ReplaceLabel, SetReplaceVisible]()
	{
		SetReplaceVisible(!ReplaceLabel->isVisibleTo(this));
	});

	SetReplaceVisible(Replace);

	swPartsList PartsList;
	Model->GetPartsList(gDefaultColor, false, true, PartsList);

	for (const auto& PartIt : PartsList)
		mFindPartComboBox->addItem(QString::fromLatin1(PartIt.first->m_strDescription), QVariant::fromValue((void*)PartIt.first));
	mFindPartComboBox->model()->sort(0);
	mFindPartComboBox->view()->setMinimumWidth(mFindPartComboBox->view()->sizeHintForColumn(0) + mFindPartComboBox->view()->verticalScrollBar()->sizeHint().width());

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
