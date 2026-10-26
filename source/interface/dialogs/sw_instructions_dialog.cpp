#include "sw_global.h"
#include "sw_instructions_dialog.h"
#include "sw_page_setup_dialog.h"
#include "sw_project.h"
#include "sw_model.h"
#include "sw_view.h"
#include "sw_collapsible_widget.h"
#include "sw_utilities.h"

swInstructionsStepImageItem::swInstructionsStepImageItem(QGraphicsItem* Parent, swInstructions* Instructions, swModel* Model, swStep Step)
	: QGraphicsPixmapItem(Parent), mInstructions(Instructions), mModel(Model), mStep(Step)
{
	setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsFocusable);
}

void swInstructionsStepImageItem::Update()
{
	swView View(swViewType::View, mModel);

	View.SetOffscreenContext();
	View.MakeCurrent();
	View.SetBackgroundColorOverride(swRGBAFromQColor(mInstructions->GetColorProperty(swInstructionsPropertyType::StepBackgroundColor, mModel, mStep)));
	View.SetSize(mWidth, mHeight);

	// Work on a copy of the main view camera, fitting the image must not move a model camera.
	View.SetCamera(View.GetCamera(), true);

	// show each step from its rotation step view, like the model view does while stepping
	View.ApplyStepView(mStep);

	// Frame the complete model whatever the main view zoom, so no step or cover image is clipped.
	View.ZoomExtentsAtStep(mModel->GetLastStep(), swGeometryBoundsMode::IncludeFallback, false);

	std::vector<QImage> Images = View.GetStepImages(mStep, mStep);

	if (!Images.empty())
		setPixmap(QPixmap::fromImage(Images.front()));
	else
		setPixmap(QPixmap());
}

swInstructionsStepNumberItem::swInstructionsStepNumberItem(QGraphicsItem* Parent, swInstructions* Instructions, swModel* Model, swStep Step)
	: QGraphicsSimpleTextItem(Parent), mInstructions(Instructions), mModel(Model), mStep(Step)
{
	setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsFocusable);
}

void swInstructionsStepNumberItem::Update()
{
	bool Visible = mInstructions->GetBoolProperty(swInstructionsPropertyType::ShowStepNumber, mModel, mStep);
	setVisible(Visible);

	QFont StepNumberFont = mInstructions->GetFontProperty(swInstructionsPropertyType::StepNumberFont, mModel, mStep);

	setFont(StepNumberFont);
	setBrush(QBrush(mInstructions->GetColorProperty(swInstructionsPropertyType::StepNumberColor, mModel, mStep)));
	setText(QString::number(mStep));
}

swInstructionsPartsListItem::swInstructionsPartsListItem(QGraphicsItem* Parent, swInstructions* Instructions, swModel* Model, swStep Step)
	: QGraphicsPixmapItem(Parent), mInstructions(Instructions), mModel(Model), mStep(Step)
{
	setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsFocusable);
}

void swInstructionsPartsListItem::Update()
{
	// the bill of materials uses the pieces list settings but is shown whenever its page exists
	bool Visible = mBillOfMaterials || mInstructions->GetBoolProperty(swInstructionsPropertyType::ShowStepPLI, mModel, mStep);
	setVisible(Visible);

	if (!Visible)
		return;

	QColor BackgroundColor = mInstructions->GetColorProperty(swInstructionsPropertyType::PLIBackgroundColor, mModel, mStep);
	QFont Font = mInstructions->GetFontProperty(swInstructionsPropertyType::PLIFont, mModel, mStep);
	QColor TextColor = mInstructions->GetColorProperty(swInstructionsPropertyType::PLITextColor, mModel, mStep);
	const bool ShowCounts = mInstructions->GetBoolProperty(swInstructionsPropertyType::PLIShowCounts, mModel, mStep);
	const swPartsListSort Sort = static_cast<swPartsListSort>(mInstructions->GetIntProperty(swInstructionsPropertyType::PLISortOrder, mModel, mStep));

	const swResult<QImage> PartsImage = mModel->GetPartsListImage(mMaxWidth, mBillOfMaterials ? 0 : mStep, swRGBAFromQColor(BackgroundColor), Font, TextColor, false, ShowCounts, Sort);

	if (!PartsImage)
	{
		setPixmap(QPixmap());

		QWidget* Parent = scene() && !scene()->views().isEmpty() ? scene()->views().front() : nullptr;
		QMessageBox::warning(Parent, QObject::tr("Create Pieces List"), PartsImage.error());

		return;
	}

	QImage Image = PartsImage.value();
	const int BorderWidth = mInstructions->GetIntProperty(swInstructionsPropertyType::PLIBorderWidth, mModel, mStep);

	if (BorderWidth > 0 && !Image.isNull())
	{
		QPainter Painter(&Image);
		QPen Pen(mInstructions->GetColorProperty(swInstructionsPropertyType::PLIBorderColor, mModel, mStep), BorderWidth);
		Pen.setJoinStyle(Qt::MiterJoin);
		Painter.setPen(Pen);
		Painter.drawRect(QRectF(BorderWidth / 2.0, BorderWidth / 2.0, Image.width() - BorderWidth, Image.height() - BorderWidth));
	}

	setPixmap(QPixmap::fromImage(Image));
}

swInstructionsPageWidget::swInstructionsPageWidget(QWidget* Parent, swInstructions* Instructions, swInstructionsPropertiesWidget* PropertiesWidget)
	: QGraphicsView(Parent), mInstructions(Instructions), mPropertiesWidget(PropertiesWidget)
{
	QGraphicsScene* Scene = new QGraphicsScene();
	setScene(Scene);
	setBackgroundBrush(palette().color(QPalette::Mid));

	connect(mInstructions, &swInstructions::StepSettingsChanged, this, &swInstructionsPageWidget::StepSettingsChanged);
	connect(Scene, &QGraphicsScene::selectionChanged, this, &swInstructionsPageWidget::SelectionChanged);
}

void swInstructionsPageWidget::StepSettingsChanged(swModel* Model, swStep Step)
{
	QGraphicsScene* Scene = scene();

	const QList<QGraphicsItem*> Items = Scene->items();

	for (QGraphicsItem* Item : Items)
	{
		swInstructionsStepImageItem* ImageItem = dynamic_cast<swInstructionsStepImageItem*>(Item);

		if (ImageItem)
		{
			if (!Model || (ImageItem->GetModel() == Model && ImageItem->GetStep() == Step))
				ImageItem->Update();

			continue;
		}

		swInstructionsStepNumberItem* NumberItem = dynamic_cast<swInstructionsStepNumberItem*>(Item);

		if (NumberItem)
		{
			if (!Model || (NumberItem->GetModel() == Model && NumberItem->GetStep() == Step))
				NumberItem->Update();

			continue;
		}

		swInstructionsPartsListItem* PartsItem = dynamic_cast<swInstructionsPartsListItem*>(Item);

		if (PartsItem)
		{
			if (!Model || (PartsItem->GetModel() == Model && PartsItem->GetStep() == Step))
				PartsItem->Update();

			continue;
		}
	}
}

void swInstructionsPageWidget::SelectionChanged()
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

static void swUpdateInstructionsPageScene(swInstructions* Instructions, const swInstructionsPage* Page, QGraphicsScene* Scene)
{
	Scene->clear();

	if (!Page)
		return;

	const swInstructionsPageSetup& PageSetup = Instructions->mPageSetup;
	Scene->setSceneRect(0, 0, PageSetup.Width, PageSetup.Height);

	// the background fills the whole page, margins included, so prints have no white border
	const QColor BackgroundColor = Instructions->GetColorProperty(swInstructionsPropertyType::PageBackgroundColor, nullptr, 0);
	QGraphicsRectItem* PageItem = Scene->addRect(QRectF(0.0f, 0.0f, PageSetup.Width, PageSetup.Height), QPen(Qt::NoPen), QBrush(BackgroundColor));
	PageItem->setFlag(QGraphicsItem::ItemClipsChildrenToShape, true);

	QRectF MarginsRect(PageSetup.MarginLeft, PageSetup.MarginTop, PageSetup.Width - PageSetup.MarginLeft - PageSetup.MarginRight, PageSetup.Height - PageSetup.MarginTop - PageSetup.MarginBottom);

	if (Page->Type == swInstructionsPageType::Blank)
		return;

	if (Page->Type == swInstructionsPageType::Cover)
	{
		swModel* Model = Page->Model;
		const swModelProperties& Properties = Model->GetProperties();
		QString Title = Properties.mDescription.trimmed();

		if (Title.isEmpty())
			Title = Properties.mModelName.trimmed();

		if (Title.isEmpty())
			Title = QFileInfo(Properties.mFileName).completeBaseName();

		const QFont TitleFont = Instructions->GetFontProperty(swInstructionsPropertyType::CoverTitleFont, Model, 0);
		const QColor TextColor = Instructions->GetColorProperty(swInstructionsPropertyType::StepNumberColor, Model, 0);

		QGraphicsTextItem* TitleItem = new QGraphicsTextItem(PageItem);
		TitleItem->setFont(TitleFont);
		TitleItem->setDefaultTextColor(TextColor);
		TitleItem->setTextWidth(MarginsRect.width());
		TitleItem->setPlainText(Title);

		QTextOption TextOption = TitleItem->document()->defaultTextOption();
		TextOption.setAlignment(Qt::AlignHCenter);
		TitleItem->document()->setDefaultTextOption(TextOption);
		TitleItem->setPos(MarginsRect.topLeft());

		qreal Top = MarginsRect.top() + TitleItem->boundingRect().height();
		const QString Author = Properties.mAuthor.trimmed();

		if (!Author.isEmpty())
		{
			QFont AuthorFont = TitleFont;
			AuthorFont.setPointSizeF(TitleFont.pointSizeF() / 2.0);
			AuthorFont.setBold(false);

			QGraphicsTextItem* AuthorItem = new QGraphicsTextItem(PageItem);
			AuthorItem->setFont(AuthorFont);
			AuthorItem->setDefaultTextColor(TextColor);
			AuthorItem->setTextWidth(MarginsRect.width());
			AuthorItem->document()->setDefaultTextOption(TextOption);
			AuthorItem->setPlainText(Author);
			AuthorItem->setPos(MarginsRect.left(), Top);

			Top += AuthorItem->boundingRect().height();
		}

		// the finished model fills the rest of the cover
		const qreal ImageHeight = MarginsRect.bottom() - Top;

		if (ImageHeight > 1.0)
		{
			swInstructionsStepImageItem* ModelImageItem = new swInstructionsStepImageItem(PageItem, Instructions, Model, Model->GetLastStep());
			ModelImageItem->setPos(MarginsRect.left(), Top);
			ModelImageItem->SetImageSize(MarginsRect.width(), ImageHeight);
			ModelImageItem->Update();
		}

		return;
	}

	if (Page->Type == swInstructionsPageType::BillOfMaterials)
	{
		swInstructionsPartsListItem* PartsImageItem = new swInstructionsPartsListItem(PageItem, Instructions, Page->Model, 0);
		PartsImageItem->SetBillOfMaterials(MarginsRect.width());
		PartsImageItem->setPos(MarginsRect.topLeft());
		PartsImageItem->Update();

		return;
	}

	// draw a divider between neighboring steps, across rows and down columns
	if (!Page->Steps.empty())
	{
		const swInstructionsStep& FirstStep = Page->Steps.front();

		if (Instructions->GetBoolProperty(swInstructionsPropertyType::ShowStepDividers, FirstStep.Model, FirstStep.Step))
		{
			const int DividerWidth = Instructions->GetIntProperty(swInstructionsPropertyType::StepDividerWidth, FirstStep.Model, FirstStep.Step);
			const QPen DividerPen(Instructions->GetColorProperty(swInstructionsPropertyType::StepDividerColor, FirstStep.Model, FirstStep.Step), DividerWidth);
			const qreal Inset = 10.0;

			auto IsNear = [](qreal Value1, qreal Value2)
			{
				return qAbs(Value1 - Value2) < 0.0001;
			};

			for (const swInstructionsStep& Step : Page->Steps)
			{
				const QRectF& Rect = Step.Rect;

				for (const swInstructionsStep& Other : Page->Steps)
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

	for (const swInstructionsStep& Step : Page->Steps)
	{
		swInstructionsStepImageItem* StepImageItem = new swInstructionsStepImageItem(PageItem, Instructions, Step.Model, Step.Step);
		StepImageItem->setPos(MarginsRect.left() + MarginsRect.width() * Step.Rect.x(), MarginsRect.top() + MarginsRect.height() * Step.Rect.y());
		StepImageItem->SetImageSize(MarginsRect.width() * Step.Rect.width(), MarginsRect.height() * Step.Rect.height());
		StepImageItem->Update();

		swInstructionsStepNumberItem* StepNumberItem = new swInstructionsStepNumberItem(StepImageItem, Instructions, Step.Model, Step.Step);
		StepNumberItem->Update();

		swInstructionsPartsListItem* PartsImageItem = new swInstructionsPartsListItem(StepImageItem, Instructions, Step.Model, Step.Step);
		PartsImageItem->setPos(StepNumberItem->boundingRect().topRight());
		PartsImageItem->Update();
	}
}

void swInstructionsPageWidget::SetCurrentPage(const swInstructionsPage* Page)
{
	QGraphicsScene* Scene = scene();

	mPage = Page;
	swUpdateInstructionsPageScene(mInstructions, Page, Scene);
	FitPage();
}

void swInstructionsPageWidget::resizeEvent(QResizeEvent* Event)
{
	QGraphicsView::resizeEvent(Event);
	FitPage();
}

void swInstructionsPageWidget::FitPage()
{
	// show the whole page whatever its size, from small paper to large screen formats
	if (mPage)
		fitInView(sceneRect().adjusted(-10.0, -10.0, 10.0, 10.0), Qt::KeepAspectRatio);
}

swInstructionsPageListWidget::swInstructionsPageListWidget(QWidget* Parent, swInstructions* Instructions)
	: QDockWidget(Parent), mInstructions(Instructions)
{
	QWidget* CentralWidget = new QWidget(this);
	setWidget(CentralWidget);
	setWindowTitle(tr("Pages"));

	QVBoxLayout* Layout = new QVBoxLayout(CentralWidget);

	QWidget* OptionsRow = swCreatePanelRow(CentralWidget);
	swAddPanelRowStretch(OptionsRow);
	Layout->addWidget(OptionsRow);

	QMenu* OptionsMenu = new QMenu(this);
	swAddPanelOptionsButton(OptionsRow, OptionsMenu, tr("Page options"), tr("Page Options"));

	QAction* PageSetupAction = OptionsMenu->addAction(tr("Page Setup..."));
	PageSetupAction->setToolTip(tr("Choose the page size, orientation and margins"));
	connect(PageSetupAction, &QAction::triggered, this, &swInstructionsPageListWidget::ShowPageSetupDialog);

	OptionsMenu->addSeparator();

	mInsertBlankBeforeAction = OptionsMenu->addAction(tr("Insert Blank Page Before"));
	mInsertBlankAfterAction = OptionsMenu->addAction(tr("Insert Blank Page After"));
	mRemoveBlankAction = OptionsMenu->addAction(tr("Remove Blank Page"));

	connect(OptionsMenu, &QMenu::aboutToShow, this, &swInstructionsPageListWidget::OptionsMenuAboutToShow);

	connect(mInsertBlankBeforeAction, &QAction::triggered, [this]()
	{
		mInstructions->InsertBlankPage(qMax(mThumbnailsWidget->currentRow(), 0));
	});

	connect(mInsertBlankAfterAction, &QAction::triggered, [this]()
	{
		mInstructions->InsertBlankPage(qMax(mThumbnailsWidget->currentRow(), 0) + 1);
	});

	connect(mRemoveBlankAction, &QAction::triggered, [this]()
	{
		mInstructions->RemoveBlankPage(qMax(mThumbnailsWidget->currentRow(), 0));
	});

	/*
	swCollapsibleWidget* SetupWidget = new swCollapsibleWidget(tr("Page Setup"), CentralWidget);
	Layout->addWidget(SetupWidget);

//	QVBoxLayout* SetupLayout = new QVBoxLayout();
//	SetupWidget->SetChildLayout(SetupLayout);

	QGridLayout* SetupLayout = new QGridLayout();
	SetupWidget->SetChildLayout(SetupLayout);

//	swCollapsibleWidget* SizeWidget = new swCollapsibleWidget(tr("Size"));
	QGroupBox* SizeWidget = new QGroupBox(tr("Size"));

	SetupLayout->addWidget(SizeWidget);

	QGridLayout* SizeLayout = new QGridLayout();
//	SizeWidget->SetChildLayout(SizeLayout);
	SizeWidget->setLayout(SizeLayout);

	mWidthEdit = new swSmallLineEdit();
	SizeLayout->addWidget(new QLabel(tr("Width:")), 1, 0);
	SizeLayout->addWidget(mWidthEdit, 1, 1);

	mHeightEdit = new swSmallLineEdit();
	SizeLayout->addWidget(new QLabel(tr("Height:")), 1, 2);
	SizeLayout->addWidget(mHeightEdit, 1, 3);

	mUnitsComboBox = new QComboBox();
	mUnitsComboBox->addItems(QStringList() << tr("Pixels") << tr("Centimeters") << tr("Inches"));
	SizeLayout->addWidget(new QLabel(tr("Units:")), 4, 0);
	SizeLayout->addWidget(mUnitsComboBox, 4, 1, 1, -1);

	mSizeComboBox = new QComboBox();
	SizeLayout->addWidget(new QLabel(tr("Preset:")), 5, 0);
	SizeLayout->addWidget(mSizeComboBox, 5, 1, 1, -1);


//	swCollapsibleWidget* OrientationWidget = new swCollapsibleWidget(tr("Orientation"));
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
//	swCollapsibleWidget* MarginsWidget = new swCollapsibleWidget(tr("Margins"));
	SetupLayout->addWidget(MarginsWidget);

	QGridLayout* MarginsLayout = new QGridLayout();
//	MarginsWidget->SetChildLayout(MarginsLayout);
	MarginsWidget->setLayout(MarginsLayout);

	mLeftMarginEdit = new swSmallLineEdit();
	MarginsLayout->addWidget(new QLabel(tr("Left:")), 2, 0);
	MarginsLayout->addWidget(mLeftMarginEdit, 2, 1);

	mRightMarginEdit = new swSmallLineEdit();
	MarginsLayout->addWidget(new QLabel(tr("Right:")), 2, 2);
	MarginsLayout->addWidget(mRightMarginEdit, 2, 3);

	mTopMarginEdit = new swSmallLineEdit();
	MarginsLayout->addWidget(new QLabel(tr("Top:")), 3, 0);
	MarginsLayout->addWidget(mTopMarginEdit, 3, 1);

	mBottomMarginEdit = new swSmallLineEdit();
	MarginsLayout->addWidget(new QLabel(tr("Bottom:")), 3, 2);
	MarginsLayout->addWidget(mBottomMarginEdit, 3, 3);

//	swCollapsibleWidget* UnitsWidget = new swCollapsibleWidget(tr("Units"));
//	SetupLayout->addWidget(UnitsWidget);
//
//	QVBoxLayout* UnitsLayout = new QVBoxLayout();
//	UnitsWidget->SetChildLayout(UnitsLayout);

//	SetupWidget->Collapse();
*/
	mThumbnailsWidget = new QListWidget(CentralWidget);
	Layout->addWidget(mThumbnailsWidget);
}

void swInstructionsPageListWidget::OptionsMenuAboutToShow()
{
	const size_t PageIndex = qMax(mThumbnailsWidget->currentRow(), 0);
	const bool IsBlank = PageIndex < mInstructions->mPages.size() && mInstructions->mPages[PageIndex].Type == swInstructionsPageType::Blank;

	mInsertBlankBeforeAction->setEnabled(mInstructions->CanInsertBlankPage(PageIndex));
	mInsertBlankAfterAction->setEnabled(mInstructions->CanInsertBlankPage(PageIndex + 1));
	mRemoveBlankAction->setEnabled(IsBlank);
}

void swInstructionsPageListWidget::ShowPageSetupDialog()
{
	swInstructionsPageSetup PageSetup = mInstructions->mPageSetup;
	swPageSetupDialog Dialog(this, &PageSetup);

	if (Dialog.exec() != QDialog::Accepted)
		return;

	mInstructions->SetPageSetup(PageSetup);
}

swInstructionsPropertiesWidget::swInstructionsPropertiesWidget(QWidget* Parent, swInstructions* Instructions)
	: QDockWidget(Parent), mInstructions(Instructions)
{
	QWidget* CentralWidget = new QWidget(this);
	setWidget(CentralWidget);
	setWindowTitle(tr("Properties"));

	QGridLayout* Layout = new QGridLayout(CentralWidget);

	QWidget* OptionsRow = swCreatePanelRow(CentralWidget);
	swAddPanelRowStretch(OptionsRow);
	Layout->addWidget(OptionsRow, 0, 0, 1, -1);

	QMenu* OptionsMenu = new QMenu(this);
	swAddPanelOptionsButton(OptionsRow, OptionsMenu, tr("Properties options"), tr("Properties Options"));

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

void swInstructionsPropertiesWidget::AddPageLayoutProperties()
{
	const swInstructionsPageSettings PageSettings = mInstructions->GetPageSettings(mPageIndex);
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
		swInstructionsPageSettings DefaultPageSettings;

		DefaultPageSettings.Rows = RowsSpinBox->value();
		DefaultPageSettings.Columns = ColumnsSpinBox->value();

		mInstructions->SetDefaultPageSettings(DefaultPageSettings);
	});
}

void swInstructionsPropertiesWidget::AddBoolProperty(swInstructionsPropertyType Type)
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
		if (Type == swInstructionsPropertyType::UseDocumentFont)
			UpdateWidget();
	});
}

void swInstructionsPropertiesWidget::AddIntProperty(swInstructionsPropertyType Type, int Minimum, int Maximum)
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

void swInstructionsPropertiesWidget::AddSortProperty(swInstructionsPropertyType Type)
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

void swInstructionsPropertiesWidget::AddColorProperty(swInstructionsPropertyType Type)
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
			case swInstructionsPropertyType::StepNumberColor:
				Title = tr("Select Step Number Color");
				break;

			case swInstructionsPropertyType::StepBackgroundColor:
				Title = tr("Select Step Background Color");
				break;

			case swInstructionsPropertyType::PLIBackgroundColor:
				Title = tr("Select Pieces List Background Color");
				break;

			case swInstructionsPropertyType::PLIBorderColor:
				Title = tr("Select Pieces List Border Color");
				break;

			case swInstructionsPropertyType::PLITextColor:
				Title = tr("Select Pieces List Text Color");
				break;

			case swInstructionsPropertyType::StepDividerColor:
				Title = tr("Select Step Divider Color");
				break;

			case swInstructionsPropertyType::PageBackgroundColor:
				Title = tr("Select Page Background Color");
				break;

			case swInstructionsPropertyType::ShowStepNumber:
			case swInstructionsPropertyType::ShowStepPLI:
			case swInstructionsPropertyType::StepNumberFont:
			case swInstructionsPropertyType::PLIFont:
			case swInstructionsPropertyType::PLIBorderWidth:
			case swInstructionsPropertyType::PLIShowCounts:
			case swInstructionsPropertyType::PLISortOrder:
			case swInstructionsPropertyType::ShowStepDividers:
			case swInstructionsPropertyType::StepDividerWidth:
			case swInstructionsPropertyType::ShowBillOfMaterials:
			case swInstructionsPropertyType::UseDocumentFont:
			case swInstructionsPropertyType::DocumentFont:
			case swInstructionsPropertyType::ShowCoverPage:
			case swInstructionsPropertyType::CoverTitleFont:
//			case swInstructionsPropertyType::StepPLIBorderRound:
			case swInstructionsPropertyType::Count:
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

void swInstructionsPropertiesWidget::AddFontProperty(swInstructionsPropertyType Type)
{
	const QString Label = mInstructions->GetPropertyLabel(Type);
	const int Row = mPropertiesLayout->rowCount();

	mPropertiesLayout->addWidget(new QLabel(Label), Row, 0);

	QToolButton* FontButton = new QToolButton();
	mPropertiesLayout->addWidget(FontButton, Row, 1);

	if (Type != swInstructionsPropertyType::DocumentFont && mInstructions->GetBoolProperty(swInstructionsPropertyType::UseDocumentFont, mModel, mStep))
	{
		FontButton->setEnabled(false);
		FontButton->setToolTip(tr("The document font is used for all text"));
	}

	auto UpdateButton = [this, Type, FontButton]()
	{
		QFont Font = mInstructions->GetFontProperty(Type, mModel, mStep);
		QString FontName = QString("%1 %2").arg(Font.family(), swFormatIntegerLocalized(Font.pointSize(), locale()));
		FontButton->setText(FontName);
	};

	UpdateButton();

	connect(FontButton, &QToolButton::clicked, [this, Type, UpdateButton]()
	{
		QString Title;

		switch (Type)
		{
			case swInstructionsPropertyType::StepNumberFont:
				Title = tr("Select Step Number Font");
				break;

			case swInstructionsPropertyType::PLIFont:
				Title = tr("Select Pieces List Font");
				break;

			case swInstructionsPropertyType::DocumentFont:
				Title = tr("Select Document Font");
				break;

			case swInstructionsPropertyType::CoverTitleFont:
				Title = tr("Select Cover Title Font");
				break;

			case swInstructionsPropertyType::ShowStepNumber:
			case swInstructionsPropertyType::ShowStepPLI:
			case swInstructionsPropertyType::StepNumberColor:
			case swInstructionsPropertyType::StepBackgroundColor:
			case swInstructionsPropertyType::PLIBackgroundColor:
			case swInstructionsPropertyType::PLITextColor:
			case swInstructionsPropertyType::PLIBorderColor:
			case swInstructionsPropertyType::StepDividerColor:
			case swInstructionsPropertyType::PLIBorderWidth:
			case swInstructionsPropertyType::PLIShowCounts:
			case swInstructionsPropertyType::PLISortOrder:
			case swInstructionsPropertyType::ShowStepDividers:
			case swInstructionsPropertyType::StepDividerWidth:
			case swInstructionsPropertyType::ShowBillOfMaterials:
			case swInstructionsPropertyType::UseDocumentFont:
			case swInstructionsPropertyType::ShowCoverPage:
			case swInstructionsPropertyType::PageBackgroundColor:
			case swInstructionsPropertyType::Count:
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

void swInstructionsPropertiesWidget::SelectionChanged(QGraphicsItem* FocusItem)
{
	if (mWidget && mFocusItem == FocusItem)
		return;

	mFocusItem = FocusItem;
	UpdateWidget();
}

void swInstructionsPropertiesWidget::SetCurrentPage(int PageIndex)
{
	mPageIndex = PageIndex;

	// the page properties show the layout of the current page
	if (!mFocusItem)
		UpdateWidget();
}

void swInstructionsPropertiesWidget::UpdateWidget()
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
		mWidget = new swCollapsibleWidget(Title); // todo: disable collapse

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
		AddBoolProperty(swInstructionsPropertyType::UseDocumentFont);
		AddFontProperty(swInstructionsPropertyType::DocumentFont);

		AddBoolProperty(swInstructionsPropertyType::ShowCoverPage);
		AddFontProperty(swInstructionsPropertyType::CoverTitleFont);
		AddBoolProperty(swInstructionsPropertyType::ShowBillOfMaterials);

		const bool IsStepsPage = mPageIndex < static_cast<int>(mInstructions->mPages.size()) && mInstructions->mPages[mPageIndex].Type == swInstructionsPageType::Steps;

		if (IsStepsPage)
		{
			AddSection(tr("Steps per Page"));
			AddPageLayoutProperties();
		}

		AddSection(tr("Page"));
		AddColorProperty(swInstructionsPropertyType::PageBackgroundColor);
		AddBoolProperty(swInstructionsPropertyType::ShowStepDividers);
		AddColorProperty(swInstructionsPropertyType::StepDividerColor);
		AddIntProperty(swInstructionsPropertyType::StepDividerWidth, 1, 20);

		AddSection(tr("Step"));
		AddBoolProperty(swInstructionsPropertyType::ShowStepNumber);
		AddFontProperty(swInstructionsPropertyType::StepNumberFont);
		AddColorProperty(swInstructionsPropertyType::StepNumberColor);
		AddColorProperty(swInstructionsPropertyType::StepBackgroundColor);

		AddSection(tr("Pieces List"));
		AddBoolProperty(swInstructionsPropertyType::ShowStepPLI);
		AddBoolProperty(swInstructionsPropertyType::PLIShowCounts);
		AddSortProperty(swInstructionsPropertyType::PLISortOrder);
		AddFontProperty(swInstructionsPropertyType::PLIFont);
		AddColorProperty(swInstructionsPropertyType::PLITextColor);
		AddColorProperty(swInstructionsPropertyType::PLIBackgroundColor);
		AddColorProperty(swInstructionsPropertyType::PLIBorderColor);
		AddIntProperty(swInstructionsPropertyType::PLIBorderWidth, 0, 20);

		return;
	}

	swInstructionsStepImageItem* ImageItem = dynamic_cast<swInstructionsStepImageItem*>(FocusItem);

	if (ImageItem)
	{
		CreatePropertyWidget(tr("Step Properties"));

		mModel = ImageItem->GetModel();
		mStep = ImageItem->GetStep();

		AddBoolProperty(swInstructionsPropertyType::ShowStepNumber);
		AddBoolProperty(swInstructionsPropertyType::ShowStepPLI);
		AddColorProperty(swInstructionsPropertyType::StepBackgroundColor);

		return;
	}

	swInstructionsStepNumberItem* NumberItem = dynamic_cast<swInstructionsStepNumberItem*>(FocusItem);

	if (NumberItem)
	{
		CreatePropertyWidget(tr("Step Number Properties"));

		mModel = NumberItem->GetModel();
		mStep = NumberItem->GetStep();

		AddFontProperty(swInstructionsPropertyType::StepNumberFont);
		AddColorProperty(swInstructionsPropertyType::StepNumberColor);

		return;
	}

	swInstructionsPartsListItem* PartsItem = dynamic_cast<swInstructionsPartsListItem*>(FocusItem);

	if (PartsItem)
	{
		CreatePropertyWidget(tr("Pieces List Properties"));

		mModel = PartsItem->GetModel();
		mStep = PartsItem->GetStep();

		AddColorProperty(swInstructionsPropertyType::PLIBackgroundColor);
		AddFontProperty(swInstructionsPropertyType::PLIFont);
		AddColorProperty(swInstructionsPropertyType::PLITextColor);
		AddColorProperty(swInstructionsPropertyType::PLIBorderColor);
		AddIntProperty(swInstructionsPropertyType::PLIBorderWidth, 0, 20);
		AddBoolProperty(swInstructionsPropertyType::PLIShowCounts);
		AddSortProperty(swInstructionsPropertyType::PLISortOrder);
//		PLIBorderRound,

		return;
	}
}

swInstructionsDialog::swInstructionsDialog(QWidget* Parent, Project* Project)
	: QMainWindow(Parent), mProject(Project)
{
	setWindowTitle(tr("Instructions"));

	mInstructions = mProject->GetInstructions();

	mPropertiesWidget = new swInstructionsPropertiesWidget(this, mInstructions);
	mPropertiesWidget->setObjectName("InstructionsProperties");
	addDockWidget(Qt::RightDockWidgetArea, mPropertiesWidget);

	mPageWidget = new swInstructionsPageWidget(this, mInstructions, mPropertiesWidget);
	setCentralWidget(mPageWidget);

	mPageListWidget = new swInstructionsPageListWidget(this, mInstructions);
	mPageListWidget->setObjectName("InstructionsPageList");
	addDockWidget(Qt::LeftDockWidgetArea, mPageListWidget);

	mPageSettingsToolBar = addToolBar(tr("Page Settings"));
	mPageSettingsToolBar->setObjectName("PageSettings");
	mPageSettingsToolBar->setFloatable(false);
	mPageSettingsToolBar->setMovable(false);

	mPrintAction = mPageSettingsToolBar->addAction(tr("Print..."));
	mPrintAction->setToolTip(tr("Print the instruction pages"));

	connect(mPageListWidget->mThumbnailsWidget, &QListWidget::currentRowChanged, this, &swInstructionsDialog::CurrentThumbnailChanged);
	UpdatePageList();

	connect(mPrintAction, &QAction::triggered, this, &swInstructionsDialog::ShowPrintDialog);
	connect(mInstructions, &swInstructions::PageSetupChanged, this, &swInstructionsDialog::PageSetupChanged);
	connect(mInstructions, &swInstructions::PageLayoutChanged, this, &swInstructionsDialog::PageSetupChanged);
}

#ifndef QT_NO_PRINTER
static void swSetInstructionsPageLayout(swInstructions* Instructions, QPrinter* Printer)
{
	// Print on paper matching the page setup, the pages already include their margins.
	const swInstructionsPageSetup& PageSetup = Instructions->mPageSetup;
	const QSizeF PageSize(qMin(PageSetup.Width, PageSetup.Height), qMax(PageSetup.Width, PageSetup.Height));
	const QPageLayout::Orientation Orientation = PageSetup.Width > PageSetup.Height ? QPageLayout::Landscape : QPageLayout::Portrait;

	Printer->setPageLayout(QPageLayout(QPageSize(PageSize, QPageSize::Point), Orientation, QMarginsF()));
	Printer->setFullPage(true);
}
#endif

void swSetupInstructionsPrinter(swInstructions* Instructions, QPrinter* Printer)
{
#ifndef QT_NO_PRINTER
	swSetInstructionsPageLayout(Instructions, Printer);
	Printer->setFromTo(1, static_cast<int>(Instructions->mPages.size()));
#endif
}

void swPrintInstructions(swInstructions* Instructions, QPrinter* Printer)
{
#ifndef QT_NO_PRINTER
	int DocCopies;
	int PageCopies;

	const int PageCount = static_cast<int>(Instructions->mPages.size());

	// The print dialog resets the paper when printing to a file, a file always gets the page setup paper.
	if (Printer->outputFormat() == QPrinter::PdfFormat)
		swSetInstructionsPageLayout(Instructions, Printer);

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

	// Pages keep their proportions on any paper, centered on a sheet filled with the page background.
	const swInstructionsPageSetup& PageSetup = Instructions->mPageSetup;
	const QRectF SheetRect(0.0, 0.0, Printer->width(), Printer->height());
	const QSizeF PageSize = QSizeF(PageSetup.Width, PageSetup.Height).scaled(SheetRect.size(), Qt::KeepAspectRatio);
	const QRectF PageRect(QPointF((SheetRect.width() - PageSize.width()) / 2.0, (SheetRect.height() - PageSize.height()) / 2.0), PageSize);
	const QColor BackgroundColor = Instructions->GetColorProperty(swInstructionsPropertyType::PageBackgroundColor, nullptr, 0);

	const int SheetCount = DocCopies * PageCopies * (qAbs(ToPage - FromPage) + 1);
	int SheetsPrinted = 0;
	swBlockingProgress Progress(QCoreApplication::translate("swInstructionsDialog", "Printing"), QCoreApplication::translate("swInstructionsDialog", "Printing pages..."), true);

	QGraphicsScene Scene;
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
					return;

				Progress.SetProgress(SheetsPrinted++, SheetCount);

				if (Progress.IsCanceled())
				{
					Printer->abort();
					Painter.end();

					// A canceled file would otherwise be left with only the pages printed so far.
					if (Printer->outputFormat() == QPrinter::PdfFormat)
						QFile::remove(Printer->outputFileName());

					return;
				}

				if (!FirstPage)
					Printer->newPage();
				else
					FirstPage = false;

				swUpdateInstructionsPageScene(Instructions, &Instructions->mPages[Page - 1], &Scene);

				Painter.fillRect(SheetRect, BackgroundColor);
				Scene.render(&Painter, PageRect, Scene.sceneRect(), Qt::KeepAspectRatio);
			}

			if (Page == ToPage)
				break;

			if (Ascending)
				Page++;
			else
				Page--;
		}
	}
#endif
}

void swInstructionsDialog::ShowPrintDialog()
{
#ifndef QT_NO_PRINTER
	QPrinter Printer(QPrinter::HighResolution);
	swSetupInstructionsPrinter(mInstructions, &Printer);

	QPrintDialog PrintDialog(&Printer, this);

	if (PrintDialog.exec() == QDialog::Accepted)
		Print(&Printer);
#endif
}

void swInstructionsDialog::Print(QPrinter* Printer)
{
	swPrintInstructions(mInstructions, Printer);
}

void swInstructionsDialog::UpdatePageList()
{
	QListWidget* ThumbnailsWidget = mPageListWidget->mThumbnailsWidget;
	const int CurrentRow = qMax(ThumbnailsWidget->currentRow(), 0);

	// rebuild the page list and keep showing a page, the old pages no longer exist
	QSignalBlocker Blocker(ThumbnailsWidget);
	ThumbnailsWidget->clear();

	for (size_t PageNumber = 0; PageNumber < mInstructions->mPages.size(); PageNumber++)
	{
		QString Label = tr("Page %1").arg(swFormatIntegerLocalized(PageNumber + 1, locale()));

		switch (mInstructions->mPages[PageNumber].Type)
		{
		case swInstructionsPageType::Cover:
			Label = tr("%1 - Cover").arg(Label);
			break;

		case swInstructionsPageType::BillOfMaterials:
			Label = tr("%1 - Bill of Materials").arg(Label);
			break;

		case swInstructionsPageType::Blank:
			Label = tr("%1 - Blank").arg(Label);
			break;

		case swInstructionsPageType::Steps:
			break;
		}

		ThumbnailsWidget->addItem(Label);
	}

	const int Row = qMin(CurrentRow, ThumbnailsWidget->count() - 1);
	ThumbnailsWidget->setCurrentRow(Row);
	CurrentThumbnailChanged(Row);
}

void swInstructionsDialog::ModelChanged()
{
	// Several edits in a row rebuild the pages once, after the current event.
	if (mUpdatePending)
		return;

	mUpdatePending = true;

	QTimer::singleShot(0, this, [this]()
	{
		mUpdatePending = false;
		mInstructions->Update();
	});
}

void swInstructionsDialog::PageSetupChanged()
{
	UpdatePageList();
}

void swInstructionsDialog::CurrentThumbnailChanged(int Index)
{
	if (Index < 0 || Index >= static_cast<int>(mInstructions->mPages.size()))
	{
		mPageWidget->SetCurrentPage(nullptr);
		return;
	}

	const swInstructionsPage* Page = &mInstructions->mPages[Index];

	mPageWidget->SetCurrentPage(Page);
	mPropertiesWidget->SetCurrentPage(Index);
}
