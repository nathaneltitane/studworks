#include "sw_global.h"
#include "sw_dialog_image.h"
#include "sw_widget_spin_box.h"
#include "sw_window_main.h"
#include "sw_window_utilities.h"
#include "sw_project.h"
#include "sw_model.h"
#include "sw_view.h"

class swImagePreviewWidget : public QWidget
{
public:
	explicit swImagePreviewWidget(QWidget* Parent)
		: QWidget(Parent)
	{
		setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
		setMinimumSize(320, 240);
	}

	QSize sizeHint() const override
	{
		return QSize(480, 400);
	}

	void SetImage(const QImage& Image, bool Transparent)
	{
		mImage = Image;
		mTransparent = Transparent;
		update();
	}

	std::function<void()> mResized;

protected:
	void paintEvent(QPaintEvent* Event) override
	{
		Q_UNUSED(Event);

		if (mImage.isNull())
			return;

		QPainter Painter(this);
		Painter.setRenderHint(QPainter::SmoothPixmapTransform);

		QRect Target(QPoint(), mImage.size().scaled(size(), Qt::KeepAspectRatio));
		Target.moveCenter(rect().center());

		// checkers show transparency
		if (mTransparent)
		{
			QPixmap Checkers(16, 16);
			Checkers.fill(palette().color(QPalette::Base));

			QPainter CheckersPainter(&Checkers);
			CheckersPainter.fillRect(0, 0, 8, 8, palette().color(QPalette::AlternateBase));
			CheckersPainter.fillRect(8, 8, 8, 8, palette().color(QPalette::AlternateBase));
			CheckersPainter.end();

			Painter.fillRect(Target, QBrush(Checkers));
		}

		Painter.drawImage(Target, mImage);
	}

	void resizeEvent(QResizeEvent* Event) override
	{
		QWidget::resizeEvent(Event);

		if (mResized)
			mResized();
	}

	QImage mImage;
	bool mTransparent = false;
};

swImageDialog::swImageDialog(QWidget* Parent, swImageDialogOptions* Options, swModel* Model)
	: QDialog(Parent), mOptions(Options), mModel(Model)
{
	setWindowTitle(tr("Save Model Image"));
	setWindowFlag(Qt::WindowContextHelpButtonHint, true);
	setWhatsThis(tr("<p><b>Save Model Image</b></p><p>Save images of the active model as seen from the current view. The preview shows exactly what will be saved.</p><p>Saving one step produces one image. Saving several steps produces a separate numbered image for each step.</p>"));

	mCurrentStep = Options->Start;
	mLastStep = qMax(Options->End, 1);
	mPreviewStep = mCurrentStep;

	QHBoxLayout* MainLayout = new QHBoxLayout(this);

	// preview column
	QFrame* PreviewFrame = new QFrame(this);
	PreviewFrame->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
	PreviewFrame->setWhatsThis(tr("<p><b>Preview</b></p><p>Shows the image as it will be saved, using the chosen size, step and background.</p><p>When several steps are saved, use the arrows to look through them.</p>"));
	MainLayout->addWidget(PreviewFrame, 1);

	QVBoxLayout* PreviewLayout = new QVBoxLayout(PreviewFrame);

	mPreview = new swImagePreviewWidget(PreviewFrame);
	PreviewLayout->addWidget(mPreview, 1);

	QHBoxLayout* CaptionLayout = new QHBoxLayout();
	PreviewLayout->addLayout(CaptionLayout);

	mPreviousButton = new QToolButton(PreviewFrame);
	mPreviousButton->setIcon(swCreatePaintedIcon(swPaintedIcon::TimePrevious));
	mPreviousButton->setAutoRaise(true);
	mPreviousButton->setToolTip(tr("Preview the previous step."));
	mPreviousButton->setAccessibleName(tr("Previous Step"));
	CaptionLayout->addWidget(mPreviousButton);

	mStepLabel = new QLabel(PreviewFrame);
	mStepLabel->setAlignment(Qt::AlignCenter);
	CaptionLayout->addWidget(mStepLabel);

	mNextButton = new QToolButton(PreviewFrame);
	mNextButton->setIcon(swCreatePaintedIcon(swPaintedIcon::TimeNext));
	mNextButton->setAutoRaise(true);
	mNextButton->setToolTip(tr("Preview the next step."));
	mNextButton->setAccessibleName(tr("Next Step"));
	CaptionLayout->addWidget(mNextButton);

	CaptionLayout->addStretch(1);

	mSizeLabel = new QLabel(PreviewFrame);
	CaptionLayout->addWidget(mSizeLabel);

	// settings column
	QVBoxLayout* SettingsLayout = new QVBoxLayout();
	MainLayout->addLayout(SettingsLayout);

	const QString FileHelp = tr("<p><b>File</b></p><p>Choose an output folder, file name, and image extension, such as .png, .jpg, or .bmp.</p><p>When saving several steps, the step number is added before the extension. For example, 'model.png' becomes 'model01.png', 'model02.png', and so on.</p>");

	QGroupBox* FileGroup = new QGroupBox(tr("File"), this);
	FileGroup->setWhatsThis(FileHelp);
	SettingsLayout->addWidget(FileGroup);

	QVBoxLayout* FileLayout = new QVBoxLayout(FileGroup);
	QHBoxLayout* PathLayout = new QHBoxLayout();
	FileLayout->addLayout(PathLayout);

	mFileEdit = new QLineEdit(Options->FilePath, FileGroup);
	mFileEdit->setToolTip(tr("Path and file name for the saved image."));
	mFileEdit->setAccessibleName(tr("File Name"));
	PathLayout->addWidget(mFileEdit, 1);

	QToolButton* BrowseButton = new QToolButton(FileGroup);
	BrowseButton->setText(QLatin1String("..."));
	BrowseButton->setToolTip(tr("Choose the image file and format."));
	BrowseButton->setAccessibleName(tr("Choose Image File"));
	BrowseButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
	PathLayout->addWidget(BrowseButton);

	QHBoxLayout* ButtonLayout = new QHBoxLayout();
	FileLayout->addLayout(ButtonLayout);

	QPushButton* SaveButton = new QPushButton(tr("&Save"), FileGroup);
	SaveButton->setDefault(true);
	SaveButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	ButtonLayout->addWidget(SaveButton, 1);

	QPushButton* CancelButton = new QPushButton(tr("Cancel"), FileGroup);
	CancelButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	ButtonLayout->addWidget(CancelButton, 1);

	// size group
	QGroupBox* SizeGroup = new QGroupBox(tr("Size"), this);
	SizeGroup->setWhatsThis(tr("<p><b>Size</b></p><p>Set the saved image width and height in pixels. Each value must be between 1 and 32768. Larger images use more memory and may exceed the graphics hardware limits.</p><p>Match View sets the height so the image has the shape of the current view. Keep Ratio keeps width and height proportional while either one changes.</p>"));
	SettingsLayout->addWidget(SizeGroup);

	QGridLayout* SizeLayout = new QGridLayout(SizeGroup);
	mGridLayouts.append(SizeLayout);

	QLabel* WidthLabel = new QLabel(tr("&Width"), SizeGroup);
	SizeLayout->addWidget(WidthLabel, 0, 0);

	mWidthSpinBox = new swRangeSpinBox(SizeGroup);
	mWidthSpinBox->setRange(1, SW_MAXIMUM_IMAGE_DIMENSION);
	mWidthSpinBox->setValue(Options->Width);
	mWidthSpinBox->setToolTip(tr("Width of the saved image, in pixels (1 to 32768)."));
	WidthLabel->setBuddy(mWidthSpinBox);
	SizeLayout->addWidget(mWidthSpinBox, 0, 1);

	QLabel* HeightLabel = new QLabel(tr("&Height"), SizeGroup);
	SizeLayout->addWidget(HeightLabel, 1, 0);

	mHeightSpinBox = new swRangeSpinBox(SizeGroup);
	mHeightSpinBox->setRange(1, SW_MAXIMUM_IMAGE_DIMENSION);
	mHeightSpinBox->setValue(Options->Height);
	mHeightSpinBox->setToolTip(tr("Height of the saved image, in pixels (1 to 32768)."));
	HeightLabel->setBuddy(mHeightSpinBox);
	SizeLayout->addWidget(mHeightSpinBox, 1, 1);

	QHBoxLayout* RatioLayout = new QHBoxLayout();
	SizeLayout->addLayout(RatioLayout, 2, 0, 1, 2);

	QPushButton* MatchButton = new QPushButton(tr("&Match View"), SizeGroup);
	MatchButton->setToolTip(tr("Set the height so the image has the shape of the current view."));
	MatchButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	MatchButton->setAutoDefault(false);
	RatioLayout->addWidget(MatchButton, 1);

	mRatioButton = new QPushButton(tr("&Keep Ratio"), SizeGroup);
	mRatioButton->setCheckable(true);
	mRatioButton->setToolTip(tr("Keep width and height proportional while either one changes."));
	mRatioButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	mRatioButton->setAutoDefault(false);
	RatioLayout->addWidget(mRatioButton, 1);

	// steps group
	QGroupBox* StepsGroup = new QGroupBox(tr("Steps"), this);
	StepsGroup->setWhatsThis(tr("<p><b>Steps</b></p><p>Choose Current Step, All Steps, or Range. The range fields are used only for Range. Both ends are included, and they may be equal to save just one step.</p>"));
	SettingsLayout->addWidget(StepsGroup);

	QGridLayout* StepsLayout = new QGridLayout(StepsGroup);
	mGridLayouts.append(StepsLayout);

	mCurrentRadio = new QRadioButton(tr("&Current Step"), StepsGroup);
	mCurrentRadio->setToolTip(tr("Save only the current step."));
	mCurrentRadio->setChecked(true);
	StepsLayout->addWidget(mCurrentRadio, 0, 0, 1, 2);

	mAllRadio = new QRadioButton(tr("&All Steps"), StepsGroup);
	mAllRadio->setToolTip(tr("Save an image for every step in the active model."));
	StepsLayout->addWidget(mAllRadio, 1, 0, 1, 2);

	mRangeRadio = new QRadioButton(tr("&Range"), StepsGroup);
	mRangeRadio->setToolTip(tr("Save the steps between the first and last step, including both."));
	StepsLayout->addWidget(mRangeRadio, 2, 0);

	QHBoxLayout* RangeLayout = new QHBoxLayout();
	StepsLayout->addLayout(RangeLayout, 2, 1);

	mFromSpinBox = new swRangeSpinBox(StepsGroup);
	mFromSpinBox->setRange(1, mLastStep);
	mFromSpinBox->setValue(mCurrentStep);
	mFromSpinBox->setToolTip(tr("First step to save."));
	mFromSpinBox->setAccessibleName(tr("First Step"));
	mFromSpinBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	RangeLayout->addWidget(mFromSpinBox, 1);

	mToSpinBox = new swRangeSpinBox(StepsGroup);
	mToSpinBox->setRange(1, mLastStep);
	mToSpinBox->setValue(mLastStep);
	mToSpinBox->setToolTip(tr("Last step to save."));
	mToSpinBox->setAccessibleName(tr("Last Step"));
	mToSpinBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	RangeLayout->addWidget(mToSpinBox, 1);

	// background group
	QGroupBox* BackgroundGroup = new QGroupBox(tr("Background"), this);
	BackgroundGroup->setWhatsThis(tr("<p><b>Background</b></p><p>View Color fills the background with the view background. Transparent leaves it empty so the model can be placed over other images.</p><p>Only PNG files keep transparency.</p>"));
	SettingsLayout->addWidget(BackgroundGroup);

	QGridLayout* BackgroundLayout = new QGridLayout(BackgroundGroup);
	mGridLayouts.append(BackgroundLayout);

	mViewColorRadio = new QRadioButton(tr("&View Color"), BackgroundGroup);
	mViewColorRadio->setToolTip(tr("Fill the background with the view background."));
	mViewColorRadio->setChecked(true);
	BackgroundLayout->addWidget(mViewColorRadio, 0, 0, 1, 2);

	mTransparentRadio = new QRadioButton(tr("&Transparent"), BackgroundGroup);
	mTransparentRadio->setToolTip(tr("Leave the background transparent, PNG files only."));
	BackgroundLayout->addWidget(mTransparentRadio, 1, 0);

	mTransparentLabel = new QLabel(QLatin1String("PNG"), BackgroundGroup);
	mTransparentLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
	BackgroundLayout->addWidget(mTransparentLabel, 1, 1);

	SettingsLayout->addStretch(1);

	for (QGridLayout* GridLayout : mGridLayouts)
		GridLayout->setColumnStretch(1, 1);

	swSetupDialogHelp(this, nullptr);

	// redraw once edits settle
	mPreviewTimer = new QTimer(this);
	mPreviewTimer->setSingleShot(true);
	mPreviewTimer->setInterval(150);

	mRatio = static_cast<double>(mWidthSpinBox->value()) / mHeightSpinBox->value();

	connect(mPreviewTimer, &QTimer::timeout, this, &swImageDialog::RenderPreview);
	connect(BrowseButton, &QToolButton::clicked, this, &swImageDialog::BrowseFile);
	connect(SaveButton, &QPushButton::clicked, this, &swImageDialog::accept);
	connect(CancelButton, &QPushButton::clicked, this, &swImageDialog::reject);
	connect(mFileEdit, &QLineEdit::textChanged, this, &swImageDialog::UpdateBackground);
	connect(mWidthSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &swImageDialog::WidthChanged);
	connect(mHeightSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &swImageDialog::HeightChanged);
	connect(MatchButton, &QPushButton::clicked, this, &swImageDialog::MatchView);
	connect(mPreviousButton, &QToolButton::clicked, this, [this]() { ShowPreviewStep(-1); });
	connect(mNextButton, &QToolButton::clicked, this, [this]() { ShowPreviewStep(1); });
	connect(mViewColorRadio, &QRadioButton::toggled, this, &swImageDialog::UpdatePreview);

	connect(mRatioButton, &QPushButton::toggled, this, [this]()
	{
		mRatio = static_cast<double>(mWidthSpinBox->value()) / mHeightSpinBox->value();
	});

	for (QRadioButton* Radio : { mCurrentRadio, mAllRadio, mRangeRadio })
		connect(Radio, &QRadioButton::toggled, this, &swImageDialog::UpdateRange);

	// keep the range ends in order
	connect(mFromSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int From)
	{
		if (mToSpinBox->value() < From)
			mToSpinBox->setValue(From);

		UpdateRange();
	});

	connect(mToSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int To)
	{
		if (mFromSpinBox->value() > To)
			mFromSpinBox->setValue(To);

		UpdateRange();
	});

	mPreview->mResized = [this]()
	{
		mPreviewTimer->start();
	};

	UpdateBackground();
	UpdateRange();
}

void swImageDialog::showEvent(QShowEvent* Event)
{
	QDialog::showEvent(Event);

	// widths are real once shown
	if (!mColumnsAligned)
	{
		mColumnsAligned = true;
		AlignColumns();
	}
}

// use to share the label column
void swImageDialog::AlignColumns()
{
	int LabelWidth = 0;

	for (QGridLayout* GridLayout : mGridLayouts)
	{
		for (int ItemIndex = 0; ItemIndex < GridLayout->count(); ItemIndex++)
		{
			int Row, Column, RowSpan, ColumnSpan;
			GridLayout->getItemPosition(ItemIndex, &Row, &Column, &RowSpan, &ColumnSpan);

			if (Column == 0 && ColumnSpan == 1)
				LabelWidth = qMax(LabelWidth, GridLayout->itemAt(ItemIndex)->sizeHint().width());
		}
	}

	// keep the longest label clear
	LabelWidth += swGetLayoutSpacing(this, Qt::Horizontal);

	for (QGridLayout* GridLayout : mGridLayouts)
		GridLayout->setColumnMinimumWidth(0, LabelWidth);
}

void swImageDialog::accept()
{
	const QString FilePath = mFileEdit->text();

	if (FilePath.isEmpty())
	{
		QMessageBox::information(this, windowTitle(), tr("Output file cannot be empty."));
		return;
	}

	mOptions->FilePath = FilePath;
	mOptions->Width = mWidthSpinBox->value();
	mOptions->Height = mHeightSpinBox->value();
	mOptions->Start = GetRangeStart();
	mOptions->End = GetRangeEnd();
	mOptions->Background = mTransparentRadio->isChecked() ? swImageBackground::Transparent : swImageBackground::Default;

	QDialog::accept();
}

// use to pick the output file
void swImageDialog::BrowseFile()
{
	const QString FilePath = QFileDialog::getSaveFileName(this, tr("Save Image File"), mFileEdit->text(), tr("Supported Image Files (*.bmp *.png *.jpg);;BMP Files (*.bmp);;PNG Files (*.png);;JPEG Files (*.jpg);;All Files (*.*)"));

	if (!FilePath.isEmpty())
		mFileEdit->setText(QDir::toNativeSeparators(FilePath));
}

void swImageDialog::WidthChanged(int Width)
{
	if (mRatioButton->isChecked())
	{
		const QSignalBlocker Blocker(mHeightSpinBox);
		mHeightSpinBox->setValue(qBound(1, qRound(Width / mRatio), SW_MAXIMUM_IMAGE_DIMENSION));
	}

	UpdatePreview();
}

void swImageDialog::HeightChanged(int Height)
{
	if (mRatioButton->isChecked())
	{
		const QSignalBlocker Blocker(mWidthSpinBox);
		mWidthSpinBox->setValue(qBound(1, qRound(Height * mRatio), SW_MAXIMUM_IMAGE_DIMENSION));
	}

	UpdatePreview();
}

// use to give the image the view shape
void swImageDialog::MatchView()
{
	const swView* ActiveView = gMainWindow ? gMainWindow->GetActiveView() : nullptr;

	if (!ActiveView || ActiveView->GetWidth() < 1 || ActiveView->GetHeight() < 1)
		return;

	const int Height = qRound(static_cast<double>(mWidthSpinBox->value()) * ActiveView->GetHeight() / ActiveView->GetWidth());

	{
		const QSignalBlocker Blocker(mHeightSpinBox);
		mHeightSpinBox->setValue(qBound(1, Height, SW_MAXIMUM_IMAGE_DIMENSION));
	}

	mRatio = static_cast<double>(mWidthSpinBox->value()) / mHeightSpinBox->value();
	UpdatePreview();
}

int swImageDialog::GetRangeStart() const
{
	if (mAllRadio->isChecked())
		return 1;

	if (mRangeRadio->isChecked())
		return mFromSpinBox->value();

	return mCurrentStep;
}

int swImageDialog::GetRangeEnd() const
{
	if (mAllRadio->isChecked())
		return mLastStep;

	if (mRangeRadio->isChecked())
		return mToSpinBox->value();

	return mCurrentStep;
}

void swImageDialog::UpdateRange()
{
	mFromSpinBox->setEnabled(mRangeRadio->isChecked());
	mToSpinBox->setEnabled(mRangeRadio->isChecked());

	// preview within the range
	mPreviewStep = qBound(GetRangeStart(), mPreviewStep, GetRangeEnd());
	UpdatePreview();
}

// use to allow transparency for png files
void swImageDialog::UpdateBackground()
{
	const bool Transparency = QFileInfo(mFileEdit->text()).suffix().compare(QLatin1String("png"), Qt::CaseInsensitive) == 0;

	if (!Transparency && mTransparentRadio->isChecked())
		mViewColorRadio->setChecked(true);

	mTransparentRadio->setEnabled(Transparency);
	mTransparentLabel->setEnabled(Transparency);
}

void swImageDialog::ShowPreviewStep(int Offset)
{
	mPreviewStep = qBound(GetRangeStart(), mPreviewStep + Offset, GetRangeEnd());
	UpdatePreview();
}

void swImageDialog::UpdatePreview()
{
	mPreviousButton->setEnabled(mPreviewStep > GetRangeStart());
	mNextButton->setEnabled(mPreviewStep < GetRangeEnd());
	mStepLabel->setText(tr("Step %1 of %2").arg(mPreviewStep).arg(mLastStep));
	mSizeLabel->setText(QString::fromUtf8("%1 × %2").arg(mWidthSpinBox->value()).arg(mHeightSpinBox->value()));

	mPreviewTimer->start();
}

// use to render the preview at widget size
void swImageDialog::RenderPreview()
{
	if (!mModel || !isVisible())
		return;

	const qreal PixelRatio = mPreview->devicePixelRatioF();
	const QSize ImageSize(mWidthSpinBox->value(), mHeightSpinBox->value());
	const QSize RenderSize = ImageSize.scaled(mPreview->size() * PixelRatio, Qt::KeepAspectRatio).expandedTo(QSize(1, 1));
	const swImageBackground Background = mTransparentRadio->isChecked() ? swImageBackground::Transparent : swImageBackground::Default;

	QImage Image = mModel->GetStepImage(false, RenderSize.width(), RenderSize.height(), mPreviewStep, Background);
	Image.setDevicePixelRatio(PixelRatio);

	mPreview->SetImage(Image, Background == swImageBackground::Transparent);
}
