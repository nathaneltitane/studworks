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
	bool Visible = mInstructions->GetBoolProperty(lcInstructionsPropertyType::ShowStepPLI, mModel, mStep);
	setVisible(Visible);

	if (!Visible)
		return;

	QColor BackgroundColor = mInstructions->GetColorProperty(lcInstructionsPropertyType::PLIBackgroundColor, mModel, mStep);
	QFont Font = mInstructions->GetFontProperty(lcInstructionsPropertyType::PLIFont, mModel, mStep);
	QColor TextColor = mInstructions->GetColorProperty(lcInstructionsPropertyType::PLITextColor, mModel, mStep);

	const lcResult<QImage> PartsImage = mModel->GetPartsListImage(300, mStep, lcRGBAFromQColor(BackgroundColor), Font, TextColor, false);

	if (!PartsImage)
	{
		setPixmap(QPixmap());

		QWidget* Parent = scene() && !scene()->views().isEmpty() ? scene()->views().front() : nullptr;
		QMessageBox::warning(Parent, QObject::tr("Create Pieces List"), PartsImage.error());

		return;
	}

	setPixmap(QPixmap::fromImage(PartsImage.value()));
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

	QToolButton* PageSetupButton = new QToolButton();
	PageSetupButton->setText(tr("Page Setup"));
	ButtonsLayout->addWidget(PageSetupButton);

	connect(PageSetupButton, &QToolButton::clicked, this, &lcInstructionsPageListWidget::ShowPageSetupDialog);

	ButtonsLayout->addStretch(1);

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

	QComboBox* ScopeComboBox = new QComboBox(CentralWidget);
	ScopeComboBox->addItems(QStringList() << tr("Default") << tr("Current Model") << tr("Current Step Only") << tr("Current Step Forward"));

	Layout->addWidget(new QLabel(tr("Scope:")), 0, 0);
	Layout->addWidget(ScopeComboBox, 0, 1);

	QComboBox* PresetComboBox = new QComboBox(CentralWidget);

	Layout->addWidget(new QLabel(tr("Preset:")), 1, 0);
	Layout->addWidget(PresetComboBox, 1, 1);

	Layout->setRowStretch(3, 1);
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

			case lcInstructionsPropertyType::ShowStepNumber:
			case lcInstructionsPropertyType::ShowStepPLI:
			case lcInstructionsPropertyType::StepNumberFont:
			case lcInstructionsPropertyType::PLIFont:
//			case lcInstructionsPropertyType::StepPLIBorderWidth:
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

			case lcInstructionsPropertyType::ShowStepNumber:
			case lcInstructionsPropertyType::ShowStepPLI:
			case lcInstructionsPropertyType::StepNumberColor:
			case lcInstructionsPropertyType::StepBackgroundColor:
			case lcInstructionsPropertyType::PLIBackgroundColor:
			case lcInstructionsPropertyType::PLITextColor:
			case lcInstructionsPropertyType::PLIBorderColor:
			case lcInstructionsPropertyType::Count:
				break;
		}

		bool Ok = false;

		QFont Font = mInstructions->GetFontProperty(Type, mModel, mStep);
		Font = QFontDialog::getFont(&Ok, Font, this, tr("Select Step Number Font"));

		if (Ok)
		{
			UpdateButton();
			mInstructions->SetDefaultFont(Type, Font);
		}
	});
}

void lcInstructionsPropertiesWidget::SelectionChanged(QGraphicsItem* FocusItem)
{
	if (mFocusItem == FocusItem)
		return;

	delete mWidget;
	mWidget = nullptr;
	mFocusItem = FocusItem;
	mModel = nullptr;
	mStep = 1;

	if (!FocusItem)
		return;

	auto CreatePropertyWidget = [this](const QString& Title)
	{
		mWidget = new lcCollapsibleWidget(Title); // todo: disable collapse

		QGridLayout* WidgetLayout = qobject_cast<QGridLayout*>(widget()->layout());
		WidgetLayout->addWidget(mWidget, 2, 0, 1, -1);

		mPropertiesLayout = new QGridLayout();
		mWidget->SetChildLayout(mPropertiesLayout);
	};

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
//		PLIBorderWidth,
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

	mPageSettingsToolBar->addSeparator();
	mPageSettingsToolBar->addWidget(new QLabel(tr("Steps per page:"), mPageSettingsToolBar));

	mRowsSpinBox = new QSpinBox(mPageSettingsToolBar);
	mRowsSpinBox->setRange(1, 10);
	mRowsSpinBox->setPrefix(tr("Rows: "));
	mRowsSpinBox->setToolTip(tr("Number of step rows on each page"));
	mPageSettingsToolBar->addWidget(mRowsSpinBox);

	mColumnsSpinBox = new QSpinBox(mPageSettingsToolBar);
	mColumnsSpinBox->setRange(1, 10);
	mColumnsSpinBox->setPrefix(tr("Columns: "));
	mColumnsSpinBox->setToolTip(tr("Number of step columns on each page"));
	mPageSettingsToolBar->addWidget(mColumnsSpinBox);
	mColumnsSpinBox->setWhatsThis(tr("Steps fill landscape pages across each row and portrait pages down each column, following the orientation chosen in Page Setup."));
	mRowsSpinBox->setWhatsThis(mColumnsSpinBox->whatsThis());

	for (size_t PageNumber = 0; PageNumber < mInstructions->mPages.size(); PageNumber++)
		mPageListWidget->mThumbnailsWidget->addItem(QString(tr("Page %1")).arg(lcFormatIntegerLocalized(PageNumber + 1, locale())));

	connect(mPageListWidget->mThumbnailsWidget, &QListWidget::currentRowChanged, this, &lcInstructionsDialog::CurrentThumbnailChanged);
	mPageListWidget->mThumbnailsWidget->setCurrentRow(0);

	connect(mPrintAction, &QAction::triggered, this, &lcInstructionsDialog::ShowPrintDialog);
	connect(mInstructions, &lcInstructions::PageSetupChanged, this, &lcInstructionsDialog::PageSetupChanged);
	connect(mRowsSpinBox, qOverload<int>(&QSpinBox::valueChanged), this, &lcInstructionsDialog::UpdatePageSettings);
	connect(mColumnsSpinBox, qOverload<int>(&QSpinBox::valueChanged), this, &lcInstructionsDialog::UpdatePageSettings);
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

void lcInstructionsDialog::UpdatePageSettings()
{
	lcInstructionsPageSettings PageSettings;

	PageSettings.Rows = mRowsSpinBox->value();
	PageSettings.Columns = mColumnsSpinBox->value();

	mInstructions->SetDefaultPageSettings(PageSettings);

	UpdatePageList();
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
//	const lcInstructionsPageSettings& PageSettings = Page->Settings;
	const lcInstructionsPageSettings& PageSettings = mInstructions->mPageSettings;

	mPageWidget->SetCurrentPage(Page);

	mRowsSpinBox->blockSignals(true);
	mRowsSpinBox->setValue(PageSettings.Rows);
	mRowsSpinBox->blockSignals(false);

	mColumnsSpinBox->blockSignals(true);
	mColumnsSpinBox->setValue(PageSettings.Columns);
	mColumnsSpinBox->blockSignals(false);
}
