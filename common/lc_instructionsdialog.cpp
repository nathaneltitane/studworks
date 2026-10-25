#include "lc_global.h"
#include "lc_instructionsdialog.h"
#include "lc_pagesetupdialog.h"
#include "project.h"
#include "lc_model.h"
#include "lc_view.h"
#include "lc_collapsiblewidget.h"
#include "lc_qutils.h"

lcInstructionsStepImageItem::lcInstructionsStepImageItem(QGraphicsItem* Parent, lcInstructions* Instructions, lcModel* Model, lcStep Step)
	: QGraphicsPixmapItem(Parent), mInstructions(Instructions), mModel(Model), mStep(Step)
{
	setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsFocusable);
}

void lcInstructionsStepImageItem::Update()
{
	lcView View(lcViewType::View, mModel);

	View.SetOffscreenContext();
	View.MakeCurrent();
	View.SetBackgroundColorOverride(lcRGBAFromQColor(mInstructions->GetColorProperty(lcInstructionsPropertyType::StepBackgroundColor, mModel, mStep)));
	View.SetSize(mWidth, mHeight);

	std::vector<QImage> Images = View.GetStepImages(mStep, mStep);

	if (!Images.empty())
		setPixmap(QPixmap::fromImage(Images.front()));
	else
		setPixmap(QPixmap());
}

lcInstructionsStepNumberItem::lcInstructionsStepNumberItem(QGraphicsItem* Parent, lcInstructions* Instructions, lcModel* Model, lcStep Step)
	: QGraphicsSimpleTextItem(Parent), mInstructions(Instructions), mModel(Model), mStep(Step)
{
	setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsFocusable);
}

void lcInstructionsStepNumberItem::Update()
{
	bool Visible = mInstructions->GetBoolProperty(lcInstructionsPropertyType::ShowStepNumber, mModel, mStep);
	setVisible(Visible);

	QFont StepNumberFont = mInstructions->GetFontProperty(lcInstructionsPropertyType::StepNumberFont, mModel, mStep);

	setFont(StepNumberFont);
	setBrush(QBrush(mInstructions->GetColorProperty(lcInstructionsPropertyType::StepNumberColor, mModel, mStep)));
	setText(QString::number(mStep));
}

lcInstructionsPartsListItem::lcInstructionsPartsListItem(QGraphicsItem* Parent, lcInstructions* Instructions, lcModel* Model, lcStep Step)
	: QGraphicsPixmapItem(Parent), mInstructions(Instructions), mModel(Model), mStep(Step)
{
	setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsFocusable);
}

void lcInstructionsPartsListItem::Update()
{
	// the bill of materials uses the pieces list settings but is shown whenever its page exists
	bool Visible = mBillOfMaterials || mInstructions->GetBoolProperty(lcInstructionsPropertyType::ShowStepPLI, mModel, mStep);
	setVisible(Visible);

	if (!Visible)
		return;

	QColor BackgroundColor = mInstructions->GetColorProperty(lcInstructionsPropertyType::PLIBackgroundColor, mModel, mStep);
	QFont Font = mInstructions->GetFontProperty(lcInstructionsPropertyType::PLIFont, mModel, mStep);
	QColor TextColor = mInstructions->GetColorProperty(lcInstructionsPropertyType::PLITextColor, mModel, mStep);
	const bool ShowCounts = mInstructions->GetBoolProperty(lcInstructionsPropertyType::PLIShowCounts, mModel, mStep);
	const lcPartsListSort Sort = static_cast<lcPartsListSort>(mInstructions->GetIntProperty(lcInstructionsPropertyType::PLISortOrder, mModel, mStep));

	const lcResult<QImage> PartsImage = mModel->GetPartsListImage(mMaxWidth, mBillOfMaterials ? 0 : mStep, lcRGBAFromQColor(BackgroundColor), Font, TextColor, false, ShowCounts, Sort);

	if (!PartsImage)
	{
		setPixmap(QPixmap());

		QWidget* Parent = scene() && !scene()->views().isEmpty() ? scene()->views().front() : nullptr;
		QMessageBox::warning(Parent, QObject::tr("Create Pieces List"), PartsImage.error());

		return;
	}

	QImage Image = PartsImage.value();
	const int BorderWidth = mInstructions->GetIntProperty(lcInstructionsPropertyType::PLIBorderWidth, mModel, mStep);

	if (BorderWidth > 0 && !Image.isNull())
	{
		QPainter Painter(&Image);
		QPen Pen(mInstructions->GetColorProperty(lcInstructionsPropertyType::PLIBorderColor, mModel, mStep), BorderWidth);
		Pen.setJoinStyle(Qt::MiterJoin);
		Painter.setPen(Pen);
		Painter.drawRect(QRectF(BorderWidth / 2.0, BorderWidth / 2.0, Image.width() - BorderWidth, Image.height() - BorderWidth));
	}

	setPixmap(QPixmap::fromImage(Image));
}

lcInstructionsPageWidget::lcInstructionsPageWidget(QWidget* Parent, lcInstructions* Instructions, lcInstructionsPropertiesWidget* PropertiesWidget)
	: QGraphicsView(Parent), mInstructions(Instructions), mPropertiesWidget(PropertiesWidget)
{
	QGraphicsScene* Scene = new QGraphicsScene();
	setScene(Scene);

	connect(mInstructions, &lcInstructions::StepSettingsChanged, this, &lcInstructionsPageWidget::StepSettingsChanged);
	connect(Scene, &QGraphicsScene::selectionChanged, this, &lcInstructionsPageWidget::SelectionChanged);
}

void lcInstructionsPageWidget::StepSettingsChanged(lcModel* Model, lcStep Step)
{
	QGraphicsScene* Scene = scene();

	const QList<QGraphicsItem*> Items = Scene->items();

	for (QGraphicsItem* Item : Items)
	{
		lcInstructionsStepImageItem* ImageItem = dynamic_cast<lcInstructionsStepImageItem*>(Item);

		if (ImageItem)
		{
			if (!Model || (ImageItem->GetModel() == Model && ImageItem->GetStep() == Step))
				ImageItem->Update();

			continue;
		}

		lcInstructionsStepNumberItem* NumberItem = dynamic_cast<lcInstructionsStepNumberItem*>(Item);

		if (NumberItem)
		{
			if (!Model || (NumberItem->GetModel() == Model && NumberItem->GetStep() == Step))
				NumberItem->Update();

			continue;
		}

		lcInstructionsPartsListItem* PartsItem = dynamic_cast<lcInstructionsPartsListItem*>(Item);

		if (PartsItem)
		{
			if (!Model || (PartsItem->GetModel() == Model && PartsItem->GetStep() == Step))
				PartsItem->Update();

			continue;
		}
	}
}

void lcInstructionsPageWidget::SelectionChanged()
{
	QGraphicsScene* Scene = qobject_cast<QGraphicsScene*>(sender());
	QGraphicsItem* Focus = nullptr;

	if (Scene)
	{
		QList<QGraphicsItem*> SelectedItems = Scene->selectedItems();

		if (!SelectedItems.isEmpty())
			Focus = SelectedItems.first();
	}

	mPropertiesWidget->SelectionChanged(Focus);
}

static void lcUpdateInstructionsPageScene(lcInstructions* Instructions, const lcInstructionsPage* Page, QGraphicsScene* Scene)
{
	Scene->clear();

	if (!Page)
		return;

	const lcInstructionsPageSetup& PageSetup = Instructions->mPageSetup;
	Scene->setSceneRect(0, 0, PageSetup.Width, PageSetup.Height);

	QGraphicsRectItem* PageItem = Scene->addRect(QRectF(0.0f, 0.0f, PageSetup.Width, PageSetup.Height), QPen(Qt::black), QBrush(Qt::white));
	PageItem->setFlag(QGraphicsItem::ItemClipsChildrenToShape, true);

	QRectF MarginsRect(PageSetup.MarginLeft, PageSetup.MarginTop, PageSetup.Width - PageSetup.MarginLeft - PageSetup.MarginRight, PageSetup.Height - PageSetup.MarginTop - PageSetup.MarginBottom);

	if (Page->BillOfMaterials)
	{
		lcInstructionsPartsListItem* PartsImageItem = new lcInstructionsPartsListItem(PageItem, Instructions, Page->BillOfMaterials, 0);
		PartsImageItem->SetBillOfMaterials(MarginsRect.width());
		PartsImageItem->setPos(MarginsRect.topLeft());
		PartsImageItem->Update();

		return;
	}

	// draw a divider between neighboring steps, across rows and down columns
	if (!Page->Steps.empty())
	{
		const lcInstructionsStep& FirstStep = Page->Steps.front();

		if (Instructions->GetBoolProperty(lcInstructionsPropertyType::ShowStepDividers, FirstStep.Model, FirstStep.Step))
		{
			const int DividerWidth = Instructions->GetIntProperty(lcInstructionsPropertyType::StepDividerWidth, FirstStep.Model, FirstStep.Step);
			const QPen DividerPen(Instructions->GetColorProperty(lcInstructionsPropertyType::StepDividerColor, FirstStep.Model, FirstStep.Step), DividerWidth);
			const qreal Inset = 10.0;

			auto IsNear = [](qreal Value1, qreal Value2)
			{
				return qAbs(Value1 - Value2) < 0.0001;
			};

			for (const lcInstructionsStep& Step : Page->Steps)
			{
				const QRectF& Rect = Step.Rect;

				for (const lcInstructionsStep& Other : Page->Steps)
				{
					const QRectF& OtherRect = Other.Rect;

					if (IsNear(OtherRect.left(), Rect.right()) && IsNear(OtherRect.top(), Rect.top()))
					{
						const qreal X = MarginsRect.left() + MarginsRect.width() * Rect.right();
						Scene->addLine(X, MarginsRect.top() + MarginsRect.height() * Rect.top() + Inset, X, MarginsRect.top() + MarginsRect.height() * Rect.bottom() - Inset, DividerPen)->setParentItem(PageItem);
					}

					if (IsNear(OtherRect.top(), Rect.bottom()) && IsNear(OtherRect.left(), Rect.left()))
					{
						const qreal Y = MarginsRect.top() + MarginsRect.height() * Rect.bottom();
						Scene->addLine(MarginsRect.left() + MarginsRect.width() * Rect.left() + Inset, Y, MarginsRect.left() + MarginsRect.width() * Rect.right() - Inset, Y, DividerPen)->setParentItem(PageItem);
					}
				}
			}
		}
	}

	for (const lcInstructionsStep& Step : Page->Steps)
	{
		lcInstructionsStepImageItem* StepImageItem = new lcInstructionsStepImageItem(PageItem, Instructions, Step.Model, Step.Step);
		StepImageItem->setPos(MarginsRect.left() + MarginsRect.width() * Step.Rect.x(), MarginsRect.top() + MarginsRect.height() * Step.Rect.y());
		StepImageItem->SetImageSize(MarginsRect.width() * Step.Rect.width(), MarginsRect.height() * Step.Rect.height());
		StepImageItem->Update();

		lcInstructionsStepNumberItem* StepNumberItem = new lcInstructionsStepNumberItem(StepImageItem, Instructions, Step.Model, Step.Step);
		StepNumberItem->Update();

		lcInstructionsPartsListItem* PartsImageItem = new lcInstructionsPartsListItem(StepImageItem, Instructions, Step.Model, Step.Step);
		PartsImageItem->setPos(StepNumberItem->boundingRect().topRight());
		PartsImageItem->Update();
	}
}

void lcInstructionsPageWidget::SetCurrentPage(const lcInstructionsPage* Page)
{
	QGraphicsScene* Scene = scene();

	mPage = Page;
	lcUpdateInstructionsPageScene(mInstructions, Page, Scene);
	FitPage();
}

void lcInstructionsPageWidget::resizeEvent(QResizeEvent* Event)
{
	QGraphicsView::resizeEvent(Event);
	FitPage();
}

void lcInstructionsPageWidget::FitPage()
{
	// show the whole page whatever its size, from small paper to large screen formats
	if (mPage)
		fitInView(sceneRect().adjusted(-10.0, -10.0, 10.0, 10.0), Qt::KeepAspectRatio);
}

lcInstructionsPageListWidget::lcInstructionsPageListWidget(QWidget* Parent, lcInstructions* Instructions)
	: QDockWidget(Parent), mInstructions(Instructions)
{
	QWidget* CentralWidget = new QWidget(this);
	setWidget(CentralWidget);
	setWindowTitle(tr("Pages"));

	QVBoxLayout* Layout = new QVBoxLayout(CentralWidget);
	Layout->setContentsMargins(0, 0, 0, 0);

	QHBoxLayout* ButtonsLayout = new QHBoxLayout();
	ButtonsLayout->setContentsMargins(0, 0, 0, 0);
	Layout->addLayout(ButtonsLayout);

	ButtonsLayout->addStretch(1);

	QToolButton* OptionsButton = new QToolButton();
	OptionsButton->setIcon(QIcon(":/resources/gear_in.png"));
	OptionsButton->setToolTip(tr("Page options"));
	OptionsButton->setAccessibleName(tr("Page Options"));
	OptionsButton->setPopupMode(QToolButton::InstantPopup);
	ButtonsLayout->addWidget(OptionsButton);

	QMenu* OptionsMenu = new QMenu(this);
	OptionsButton->setMenu(OptionsMenu);

	QAction* PageSetupAction = OptionsMenu->addAction(tr("Page Setup..."));
	PageSetupAction->setToolTip(tr("Choose the page size, orientation and margins"));
	connect(PageSetupAction, &QAction::triggered, this, &lcInstructionsPageListWidget::ShowPageSetupDialog);

	/*
	lcCollapsibleWidget* SetupWidget = new lcCollapsibleWidget(tr("Page Setup"), CentralWidget);
	Layout->addWidget(SetupWidget);

//	QVBoxLayout* SetupLayout = new QVBoxLayout();
//	SetupWidget->SetChildLayout(SetupLayout);

	QGridLayout* SetupLayout = new QGridLayout();
	SetupWidget->SetChildLayout(SetupLayout);

//	lcCollapsibleWidget* SizeWidget = new lcCollapsibleWidget(tr("Size"));
	QGroupBox* SizeWidget = new QGroupBox(tr("Size"));

	SetupLayout->addWidget(SizeWidget);

	QGridLayout* SizeLayout = new QGridLayout();
//	SizeWidget->SetChildLayout(SizeLayout);
	SizeWidget->setLayout(SizeLayout);

	mWidthEdit = new lcSmallLineEdit();
	SizeLayout->addWidget(new QLabel(tr("Width:")), 1, 0);
	SizeLayout->addWidget(mWidthEdit, 1, 1);

	mHeightEdit = new lcSmallLineEdit();
	SizeLayout->addWidget(new QLabel(tr("Height:")), 1, 2);
	SizeLayout->addWidget(mHeightEdit, 1, 3);

	mUnitsComboBox = new QComboBox();
	mUnitsComboBox->addItems(QStringList() << tr("Pixels") << tr("Centimeters") << tr("Inches"));
	SizeLayout->addWidget(new QLabel(tr("Units:")), 4, 0);
	SizeLayout->addWidget(mUnitsComboBox, 4, 1, 1, -1);

	mSizeComboBox = new QComboBox();
	SizeLayout->addWidget(new QLabel(tr("Preset:")), 5, 0);
	SizeLayout->addWidget(mSizeComboBox, 5, 1, 1, -1);


//	lcCollapsibleWidget* OrientationWidget = new lcCollapsibleWidget(tr("Orientation"));
//	SetupLayout->addWidget(OrientationWidget);
//
//	QVBoxLayout* OrientationLayout = new QVBoxLayout();
//	OrientationWidget->SetChildLayout(OrientationLayout);
//
//	mPortraitButton = new QRadioButton(tr("Portrait"));
//	OrientationLayout->addWidget(mPortraitButton);
//	mLandscapeButton = new QRadioButton(tr("Landscape"));
//	OrientationLayout->addWidget(mLandscapeButton);

	QGroupBox* MarginsWidget = new QGroupBox(tr("Margins"));
//	lcCollapsibleWidget* MarginsWidget = new lcCollapsibleWidget(tr("Margins"));
	SetupLayout->addWidget(MarginsWidget);

	QGridLayout* MarginsLayout = new QGridLayout();
//	MarginsWidget->SetChildLayout(MarginsLayout);
	MarginsWidget->setLayout(MarginsLayout);

	mLeftMarginEdit = new lcSmallLineEdit();
	MarginsLayout->addWidget(new QLabel(tr("Left:")), 2, 0);
	MarginsLayout->addWidget(mLeftMarginEdit, 2, 1);

	mRightMarginEdit = new lcSmallLineEdit();
	MarginsLayout->addWidget(new QLabel(tr("Right:")), 2, 2);
	MarginsLayout->addWidget(mRightMarginEdit, 2, 3);

	mTopMarginEdit = new lcSmallLineEdit();
	MarginsLayout->addWidget(new QLabel(tr("Top:")), 3, 0);
	MarginsLayout->addWidget(mTopMarginEdit, 3, 1);

	mBottomMarginEdit = new lcSmallLineEdit();
	MarginsLayout->addWidget(new QLabel(tr("Bottom:")), 3, 2);
	MarginsLayout->addWidget(mBottomMarginEdit, 3, 3);

//	lcCollapsibleWidget* UnitsWidget = new lcCollapsibleWidget(tr("Units"));
//	SetupLayout->addWidget(UnitsWidget);
//
//	QVBoxLayout* UnitsLayout = new QVBoxLayout();
//	UnitsWidget->SetChildLayout(UnitsLayout);

//	SetupWidget->Collapse();
*/
	mThumbnailsWidget = new QListWidget(CentralWidget);
	Layout->addWidget(mThumbnailsWidget);
}

void lcInstructionsPageListWidget::ShowPageSetupDialog()
{
	lcInstructionsPageSetup PageSetup = mInstructions->mPageSetup;
	lcPageSetupDialog Dialog(this, &PageSetup);

	if (Dialog.exec() != QDialog::Accepted)
		return;

	mInstructions->SetPageSetup(PageSetup);
}

lcInstructionsPropertiesWidget::lcInstructionsPropertiesWidget(QWidget* Parent, lcInstructions* Instructions)
	: QDockWidget(Parent), mInstructions(Instructions)
{
	QWidget* CentralWidget = new QWidget(this);
	setWidget(CentralWidget);
	setWindowTitle(tr("Properties"));

	QGridLayout* Layout = new QGridLayout(CentralWidget);
	Layout->setContentsMargins(0, 0, 0, 0);

	QHBoxLayout* ButtonsLayout = new QHBoxLayout();
	ButtonsLayout->setContentsMargins(0, 0, 0, 0);
	ButtonsLayout->addStretch(1);
	Layout->addLayout(ButtonsLayout, 0, 0, 1, -1);

	QToolButton* OptionsButton = new QToolButton();
	OptionsButton->setIcon(QIcon(":/resources/gear_in.png"));
	OptionsButton->setToolTip(tr("Properties options"));
	OptionsButton->setAccessibleName(tr("Properties Options"));
	OptionsButton->setPopupMode(QToolButton::InstantPopup);
	ButtonsLayout->addWidget(OptionsButton);

	QMenu* OptionsMenu = new QMenu(this);
	OptionsButton->setMenu(OptionsMenu);

	// changes apply to the defaults for now, model and step overrides are planned
	QMenu* ScopeMenu = OptionsMenu->addMenu(tr("Apply Changes To"));
	QActionGroup* ScopeGroup = new QActionGroup(ScopeMenu);
	const QStringList ScopeNames = { tr("All Steps"), tr("Current Model"), tr("Current Step Only"), tr("Current Step Forward") };

	for (int ScopeIndex = 0; ScopeIndex < ScopeNames.size(); ScopeIndex++)
	{
		QAction* ScopeAction = ScopeMenu->addAction(ScopeNames[ScopeIndex]);
		ScopeAction->setCheckable(true);
		ScopeAction->setChecked(ScopeIndex == 0);
		ScopeAction->setEnabled(ScopeIndex == 0);
		ScopeGroup->addAction(ScopeAction);
	}

	Layout->setRowStretch(3, 1);

	SelectionChanged(nullptr);
}

void lcInstructionsPropertiesWidget::AddPageLayoutProperties()
{
	const lcInstructionsPageSettings PageSettings = mInstructions->GetPageSettings(mPageIndex);
	int Row = mPropertiesLayout->rowCount();

	auto AddSpinBox = [this, &Row](const QString& Label, int Value, const QString& ToolTip)
	{
		QLabel* SpinBoxLabel = new QLabel(Label);
		mPropertiesLayout->addWidget(SpinBoxLabel, Row, 0);

		QSpinBox* SpinBox = new QSpinBox();
		SpinBox->setRange(1, 10);
		SpinBox->setValue(Value);
		SpinBox->setKeyboardTracking(false);
		SpinBox->setToolTip(ToolTip);
		SpinBox->setWhatsThis(tr("Steps fill landscape pages across each row and portrait pages down each column, following the orientation chosen in Page Setup."));
		SpinBoxLabel->setBuddy(SpinBox);
		mPropertiesLayout->addWidget(SpinBox, Row, 1);
		Row++;

		return SpinBox;
	};

	QSpinBox* RowsSpinBox = AddSpinBox(tr("Rows:"), PageSettings.Rows, tr("Number of step rows on this page"));
	QSpinBox* ColumnsSpinBox = AddSpinBox(tr("Columns:"), PageSettings.Columns, tr("Number of step columns on this page"));

	auto PageLayoutChanged = [this, RowsSpinBox, ColumnsSpinBox]()
	{
		mInstructions->SetPageSettings(mPageIndex, RowsSpinBox->value(), ColumnsSpinBox->value());
	};

	connect(RowsSpinBox, qOverload<int>(&QSpinBox::valueChanged), this, PageLayoutChanged);
	connect(ColumnsSpinBox, qOverload<int>(&QSpinBox::valueChanged), this, PageLayoutChanged);

	QPushButton* ApplyAllButton = new QPushButton(tr("Apply to All Pages"));
	ApplyAllButton->setToolTip(tr("Use this number of rows and columns on every page"));
	mPropertiesLayout->addWidget(ApplyAllButton, Row, 0, 1, -1);

	connect(ApplyAllButton, &QPushButton::clicked, [this, RowsSpinBox, ColumnsSpinBox]()
	{
		lcInstructionsPageSettings DefaultPageSettings;

		DefaultPageSettings.Rows = RowsSpinBox->value();
		DefaultPageSettings.Columns = ColumnsSpinBox->value();

		mInstructions->SetDefaultPageSettings(DefaultPageSettings);
	});
}

void lcInstructionsPropertiesWidget::AddBoolProperty(lcInstructionsPropertyType Type)
{
	const QString Label = mInstructions->GetPropertyLabel(Type);
	const int Row = mPropertiesLayout->rowCount();

	QCheckBox* CheckBox = new QCheckBox(Label);
	mPropertiesLayout->addWidget(CheckBox, Row, 0, 1, -1);

	bool Enabled = mInstructions->GetBoolProperty(Type, mModel, mStep);
	CheckBox->setChecked(Enabled);

	connect(CheckBox, &QToolButton::toggled, [this, Type](bool Checked)
	{
		mInstructions->SetDefaultBool(Type, Checked);

		// the element fonts are only editable without the document font
		if (Type == lcInstructionsPropertyType::UseDocumentFont)
			UpdateWidget();
	});
}

void lcInstructionsPropertiesWidget::AddIntProperty(lcInstructionsPropertyType Type, int Minimum, int Maximum)
{
	const QString Label = mInstructions->GetPropertyLabel(Type);
	const int Row = mPropertiesLayout->rowCount();

	mPropertiesLayout->addWidget(new QLabel(Label), Row, 0);

	QSpinBox* SpinBox = new QSpinBox();
	SpinBox->setRange(Minimum, Maximum);
	SpinBox->setValue(mInstructions->GetIntProperty(Type, mModel, mStep));
	mPropertiesLayout->addWidget(SpinBox, Row, 1);

	connect(SpinBox, qOverload<int>(&QSpinBox::valueChanged), [this, Type](int Value)
	{
		mInstructions->SetDefaultInt(Type, Value);
	});
}

void lcInstructionsPropertiesWidget::AddSortProperty(lcInstructionsPropertyType Type)
{
	const QString Label = mInstructions->GetPropertyLabel(Type);
	const int Row = mPropertiesLayout->rowCount();

	mPropertiesLayout->addWidget(new QLabel(Label), Row, 0);

	QComboBox* ComboBox = new QComboBox();
	ComboBox->addItems(QStringList() << tr("Color") << tr("Name") << tr("Part ID"));
	ComboBox->setCurrentIndex(mInstructions->GetIntProperty(Type, mModel, mStep));
	mPropertiesLayout->addWidget(ComboBox, Row, 1);

	connect(ComboBox, qOverload<int>(&QComboBox::currentIndexChanged), [this, Type](int Index)
	{
		mInstructions->SetDefaultInt(Type, Index);
	});
}

void lcInstructionsPropertiesWidget::AddColorProperty(lcInstructionsPropertyType Type)
{
	const QString Label = mInstructions->GetPropertyLabel(Type);
	const int Row = mPropertiesLayout->rowCount();

	mPropertiesLayout->addWidget(new QLabel(Label), Row, 0);

	QToolButton* ColorButton = new QToolButton();
	mPropertiesLayout->addWidget(ColorButton, Row, 1);

	auto UpdateButton = [this, Type, ColorButton]()
	{
		QPixmap Pixmap(12, 12);
		QColor Color = mInstructions->GetColorProperty(Type, mModel, mStep);
		Pixmap.fill(Color);
		ColorButton->setIcon(Pixmap);
	};

	UpdateButton();

	connect(ColorButton, &QToolButton::clicked, [this, Type, UpdateButton]()
	{
		QString Title;

		switch (Type)
		{
			case lcInstructionsPropertyType::StepNumberColor:
				Title = tr("Select Step Number Color");
				break;

			case lcInstructionsPropertyType::StepBackgroundColor:
				Title = tr("Select Step Background Color");
				break;

			case lcInstructionsPropertyType::PLIBackgroundColor:
				Title = tr("Select Pieces List Background Color");
				break;

			case lcInstructionsPropertyType::PLIBorderColor:
				Title = tr("Select Pieces List Border Color");
				break;

			case lcInstructionsPropertyType::PLITextColor:
				Title = tr("Select Pieces List Text Color");
				break;

			case lcInstructionsPropertyType::StepDividerColor:
				Title = tr("Select Step Divider Color");
				break;

			case lcInstructionsPropertyType::ShowStepNumber:
			case lcInstructionsPropertyType::ShowStepPLI:
			case lcInstructionsPropertyType::StepNumberFont:
			case lcInstructionsPropertyType::PLIFont:
			case lcInstructionsPropertyType::PLIBorderWidth:
			case lcInstructionsPropertyType::PLIShowCounts:
			case lcInstructionsPropertyType::PLISortOrder:
			case lcInstructionsPropertyType::ShowStepDividers:
			case lcInstructionsPropertyType::StepDividerWidth:
			case lcInstructionsPropertyType::ShowBillOfMaterials:
			case lcInstructionsPropertyType::UseDocumentFont:
			case lcInstructionsPropertyType::DocumentFont:
//			case lcInstructionsPropertyType::StepPLIBorderRound:
			case lcInstructionsPropertyType::Count:
				break;
		}

		QColor Color = mInstructions->GetColorProperty(Type, mModel, mStep);
		Color = QColorDialog::getColor(Color, this, Title);

		if (Color.isValid())
		{
			mInstructions->SetDefaultColor(Type, Color);
			UpdateButton();
		}
	});
}

void lcInstructionsPropertiesWidget::AddFontProperty(lcInstructionsPropertyType Type)
{
	const QString Label = mInstructions->GetPropertyLabel(Type);
	const int Row = mPropertiesLayout->rowCount();

	mPropertiesLayout->addWidget(new QLabel(Label), Row, 0);

	QToolButton* FontButton = new QToolButton();
	mPropertiesLayout->addWidget(FontButton, Row, 1);

	if (Type != lcInstructionsPropertyType::DocumentFont && mInstructions->GetBoolProperty(lcInstructionsPropertyType::UseDocumentFont, mModel, mStep))
	{
		FontButton->setEnabled(false);
		FontButton->setToolTip(tr("The document font is used for all text"));
	}

	auto UpdateButton = [this, Type, FontButton]()
	{
		QFont Font = mInstructions->GetFontProperty(Type, mModel, mStep);
		QString FontName = QString("%1 %2").arg(Font.family(), lcFormatIntegerLocalized(Font.pointSize(), locale()));
		FontButton->setText(FontName);
	};

	UpdateButton();

	connect(FontButton, &QToolButton::clicked, [this, Type, UpdateButton]()
	{
		QString Title;

		switch (Type)
		{
			case lcInstructionsPropertyType::StepNumberFont:
				Title = tr("Select Step Number Font");
				break;

			case lcInstructionsPropertyType::PLIFont:
				Title = tr("Select Pieces List Font");
				break;

			case lcInstructionsPropertyType::DocumentFont:
				Title = tr("Select Document Font");
				break;

			case lcInstructionsPropertyType::ShowStepNumber:
			case lcInstructionsPropertyType::ShowStepPLI:
			case lcInstructionsPropertyType::StepNumberColor:
			case lcInstructionsPropertyType::StepBackgroundColor:
			case lcInstructionsPropertyType::PLIBackgroundColor:
			case lcInstructionsPropertyType::PLITextColor:
			case lcInstructionsPropertyType::PLIBorderColor:
			case lcInstructionsPropertyType::StepDividerColor:
			case lcInstructionsPropertyType::PLIBorderWidth:
			case lcInstructionsPropertyType::PLIShowCounts:
			case lcInstructionsPropertyType::PLISortOrder:
			case lcInstructionsPropertyType::ShowStepDividers:
			case lcInstructionsPropertyType::StepDividerWidth:
			case lcInstructionsPropertyType::ShowBillOfMaterials:
			case lcInstructionsPropertyType::UseDocumentFont:
			case lcInstructionsPropertyType::Count:
				break;
		}

		bool Ok = false;

		QFont Font = mInstructions->GetFontProperty(Type, mModel, mStep);
		Font = QFontDialog::getFont(&Ok, Font, this, Title);

		if (Ok)
		{
			UpdateButton();
			mInstructions->SetDefaultFont(Type, Font);
		}
	});
}

void lcInstructionsPropertiesWidget::SelectionChanged(QGraphicsItem* FocusItem)
{
	if (mWidget && mFocusItem == FocusItem)
		return;

	mFocusItem = FocusItem;
	UpdateWidget();
}

void lcInstructionsPropertiesWidget::SetCurrentPage(int PageIndex)
{
	mPageIndex = PageIndex;

	// the page properties show the layout of the current page
	if (!mFocusItem)
		UpdateWidget();
}

void lcInstructionsPropertiesWidget::UpdateWidget()
{
	QGraphicsItem* FocusItem = mFocusItem;

	// the old widget may be the one whose signal led here, so delete it later
	if (mWidget)
	{
		mWidget->hide();
		mWidget->deleteLater();
	}

	mWidget = nullptr;
	mModel = nullptr;
	mStep = 1;

	auto CreatePropertyWidget = [this](const QString& Title)
	{
		mWidget = new lcCollapsibleWidget(Title); // todo: disable collapse

		QGridLayout* WidgetLayout = qobject_cast<QGridLayout*>(widget()->layout());
		WidgetLayout->addWidget(mWidget, 2, 0, 1, -1);

		mPropertiesLayout = new QGridLayout();
		mWidget->SetChildLayout(mPropertiesLayout);
	};

	auto AddSection = [this](const QString& Title)
	{
		QLabel* Label = new QLabel(Title);
		QFont Font = Label->font();
		Font.setBold(true);
		Label->setFont(Font);
		mPropertiesLayout->addWidget(Label, mPropertiesLayout->rowCount(), 0, 1, -1);
	};

	// with nothing selected, show the defaults for every page
	if (!FocusItem)
	{
		CreatePropertyWidget(tr("Page Properties"));

		AddSection(tr("Document"));
		AddBoolProperty(lcInstructionsPropertyType::UseDocumentFont);
		AddFontProperty(lcInstructionsPropertyType::DocumentFont);

		AddSection(tr("Steps per Page"));
		AddPageLayoutProperties();

		AddSection(tr("Page"));
		AddBoolProperty(lcInstructionsPropertyType::ShowStepDividers);
		AddColorProperty(lcInstructionsPropertyType::StepDividerColor);
		AddIntProperty(lcInstructionsPropertyType::StepDividerWidth, 1, 20);
		AddBoolProperty(lcInstructionsPropertyType::ShowBillOfMaterials);

		AddSection(tr("Step"));
		AddBoolProperty(lcInstructionsPropertyType::ShowStepNumber);
		AddFontProperty(lcInstructionsPropertyType::StepNumberFont);
		AddColorProperty(lcInstructionsPropertyType::StepNumberColor);
		AddColorProperty(lcInstructionsPropertyType::StepBackgroundColor);

		AddSection(tr("Pieces List"));
		AddBoolProperty(lcInstructionsPropertyType::ShowStepPLI);
		AddBoolProperty(lcInstructionsPropertyType::PLIShowCounts);
		AddSortProperty(lcInstructionsPropertyType::PLISortOrder);
		AddFontProperty(lcInstructionsPropertyType::PLIFont);
		AddColorProperty(lcInstructionsPropertyType::PLITextColor);
		AddColorProperty(lcInstructionsPropertyType::PLIBackgroundColor);
		AddColorProperty(lcInstructionsPropertyType::PLIBorderColor);
		AddIntProperty(lcInstructionsPropertyType::PLIBorderWidth, 0, 20);

		return;
	}

	lcInstructionsStepImageItem* ImageItem = dynamic_cast<lcInstructionsStepImageItem*>(FocusItem);

	if (ImageItem)
	{
		CreatePropertyWidget(tr("Step Properties"));

		mModel = ImageItem->GetModel();
		mStep = ImageItem->GetStep();

		AddBoolProperty(lcInstructionsPropertyType::ShowStepNumber);
		AddBoolProperty(lcInstructionsPropertyType::ShowStepPLI);
		AddColorProperty(lcInstructionsPropertyType::StepBackgroundColor);

		return;
	}

	lcInstructionsStepNumberItem* NumberItem = dynamic_cast<lcInstructionsStepNumberItem*>(FocusItem);

	if (NumberItem)
	{
		CreatePropertyWidget(tr("Step Number Properties"));

		mModel = NumberItem->GetModel();
		mStep = NumberItem->GetStep();

		AddFontProperty(lcInstructionsPropertyType::StepNumberFont);
		AddColorProperty(lcInstructionsPropertyType::StepNumberColor);

		return;
	}

	lcInstructionsPartsListItem* PartsItem = dynamic_cast<lcInstructionsPartsListItem*>(FocusItem);

	if (PartsItem)
	{
		CreatePropertyWidget(tr("Pieces List Properties"));

		mModel = PartsItem->GetModel();
		mStep = PartsItem->GetStep();

		AddColorProperty(lcInstructionsPropertyType::PLIBackgroundColor);
		AddFontProperty(lcInstructionsPropertyType::PLIFont);
		AddColorProperty(lcInstructionsPropertyType::PLITextColor);
		AddColorProperty(lcInstructionsPropertyType::PLIBorderColor);
		AddIntProperty(lcInstructionsPropertyType::PLIBorderWidth, 0, 20);
		AddBoolProperty(lcInstructionsPropertyType::PLIShowCounts);
		AddSortProperty(lcInstructionsPropertyType::PLISortOrder);
//		PLIBorderRound,

		return;
	}
}

lcInstructionsDialog::lcInstructionsDialog(QWidget* Parent, Project* Project)
	: QMainWindow(Parent), mProject(Project)
{
	setWindowTitle(tr("Instructions"));

	mInstructions = mProject->GetInstructions();

	mPropertiesWidget = new lcInstructionsPropertiesWidget(this, mInstructions);
	mPropertiesWidget->setObjectName("InstructionsProperties");
	addDockWidget(Qt::RightDockWidgetArea, mPropertiesWidget);

	mPageWidget = new lcInstructionsPageWidget(this, mInstructions, mPropertiesWidget);
	setCentralWidget(mPageWidget);

	mPageListWidget = new lcInstructionsPageListWidget(this, mInstructions);
	mPageListWidget->setObjectName("InstructionsPageList");
	addDockWidget(Qt::LeftDockWidgetArea, mPageListWidget);

	mPageSettingsToolBar = addToolBar(tr("Page Settings"));
	mPageSettingsToolBar->setObjectName("PageSettings");
	mPageSettingsToolBar->setFloatable(false);
	mPageSettingsToolBar->setMovable(false);

	mPrintAction = mPageSettingsToolBar->addAction(tr("Print..."));
	mPrintAction->setToolTip(tr("Print the instruction pages"));

	for (size_t PageNumber = 0; PageNumber < mInstructions->mPages.size(); PageNumber++)
		mPageListWidget->mThumbnailsWidget->addItem(QString(tr("Page %1")).arg(lcFormatIntegerLocalized(PageNumber + 1, locale())));

	connect(mPageListWidget->mThumbnailsWidget, &QListWidget::currentRowChanged, this, &lcInstructionsDialog::CurrentThumbnailChanged);
	mPageListWidget->mThumbnailsWidget->setCurrentRow(0);

	connect(mPrintAction, &QAction::triggered, this, &lcInstructionsDialog::ShowPrintDialog);
	connect(mInstructions, &lcInstructions::PageSetupChanged, this, &lcInstructionsDialog::PageSetupChanged);
	connect(mInstructions, &lcInstructions::PageLayoutChanged, this, &lcInstructionsDialog::PageSetupChanged);
}

void lcInstructionsDialog::ShowPrintDialog()
{
#ifndef QT_NO_PRINTER
	int PageCount = static_cast<int>(mInstructions->mPages.size());

	QPrinter Printer(QPrinter::HighResolution);
	Printer.setFromTo(1, PageCount + 1);

	// print on paper matching the page setup, the page already includes its margins
	const lcInstructionsPageSetup& PageSetup = mInstructions->mPageSetup;
	const QSizeF PageSize(qMin(PageSetup.Width, PageSetup.Height), qMax(PageSetup.Width, PageSetup.Height));
	const QPageLayout::Orientation Orientation = PageSetup.Width > PageSetup.Height ? QPageLayout::Landscape : QPageLayout::Portrait;

	Printer.setPageLayout(QPageLayout(QPageSize(PageSize, QPageSize::Point), Orientation, QMarginsF()));
	Printer.setFullPage(true);

	QPrintDialog PrintDialog(&Printer, this);

	if (PrintDialog.exec() == QDialog::Accepted)
		Print(&Printer);
#endif
}

void lcInstructionsDialog::Print(QPrinter* Printer)
{
#ifndef QT_NO_PRINTER
	int DocCopies;
	int PageCopies;

	const int PageCount = static_cast<int>(mInstructions->mPages.size());

	if (Printer->collateCopies())
	{
		DocCopies = 1;
		PageCopies = Printer->supportsMultipleCopies() ? 1 : Printer->copyCount();
	}
	else
	{
		DocCopies = Printer->supportsMultipleCopies() ? 1 : Printer->copyCount();
		PageCopies = 1;
	}

	int FromPage = Printer->fromPage();
	int ToPage = Printer->toPage();
	bool Ascending = true;

	if (FromPage == 0 && ToPage == 0)
	{
		FromPage = 1;
		ToPage = PageCount;
	}

	FromPage = qMax(1, FromPage);
	ToPage = qMin(PageCount, ToPage);

	if (ToPage < FromPage)
		return;

	if (Printer->pageOrder() == QPrinter::LastPageFirst)
	{
		std::swap(FromPage, ToPage);
		Ascending = false;
	}

	QGraphicsScene* Scene = new QGraphicsScene();
	QPainter Painter(Printer);
	bool FirstPage = true;

	for (int DocCopy = 0; DocCopy < DocCopies; DocCopy++)
	{
		int Page = FromPage;

		for (;;)
		{
			for (int PageCopy = 0; PageCopy < PageCopies; PageCopy++)
			{
				if (Printer->printerState() == QPrinter::Aborted || Printer->printerState() == QPrinter::Error)
				{
					delete Scene;

					return;
				}

				if (!FirstPage)
					Printer->newPage();
				else
					FirstPage = false;

				const lcInstructionsPage* InstructionsPage = &mInstructions->mPages[Page - 1];
				lcUpdateInstructionsPageScene(mInstructions, InstructionsPage, Scene);

				Scene->render(&Painter);
			}

			if (Page == ToPage)
				break;

			if (Ascending)
				Page++;
			else
				Page--;
		}
	}

	delete Scene;
#endif
}

void lcInstructionsDialog::UpdatePageList()
{
	QListWidget* ThumbnailsWidget = mPageListWidget->mThumbnailsWidget;
	const int CurrentRow = qMax(ThumbnailsWidget->currentRow(), 0);

	// rebuild the page list and keep showing a page, the old pages no longer exist
	QSignalBlocker Blocker(ThumbnailsWidget);
	ThumbnailsWidget->clear();

	for (size_t PageNumber = 0; PageNumber < mInstructions->mPages.size(); PageNumber++)
		ThumbnailsWidget->addItem(tr("Page %1").arg(lcFormatIntegerLocalized(PageNumber + 1, locale())));

	const int Row = qMin(CurrentRow, ThumbnailsWidget->count() - 1);
	ThumbnailsWidget->setCurrentRow(Row);
	CurrentThumbnailChanged(Row);
}

void lcInstructionsDialog::PageSetupChanged()
{
	UpdatePageList();
}

void lcInstructionsDialog::CurrentThumbnailChanged(int Index)
{
	if (Index < 0 || Index >= static_cast<int>(mInstructions->mPages.size()))
	{
		mPageWidget->SetCurrentPage(nullptr);
		return;
	}

	const lcInstructionsPage* Page = &mInstructions->mPages[Index];

	mPageWidget->SetCurrentPage(Page);
	mPropertiesWidget->SetCurrentPage(Index);
}
