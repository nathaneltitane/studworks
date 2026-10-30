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

		// frame shows the output shape
		Painter.setPen(palette().color(QPalette::Mid));
		Painter.drawRect(Target.adjusted(-1, -1, 0, 0));
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

	mLastStep = qMax(Options->End, 1);
	mCurrentStep = qBound(1, Options->Start, mLastStep);
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

	mPlayButton = new QToolButton(PreviewFrame);
	mPlayButton->setAutoRaise(true);
	mPlayButton->setAccessibleName(tr("Play Animation"));
	CaptionLayout->addWidget(mPlayButton);

	CaptionLayout->addStretch(1);

	mSizeLabel = new QLabel(PreviewFrame);
	CaptionLayout->addWidget(mSizeLabel);

	// settings column
	QVBoxLayout* SettingsLayout = new QVBoxLayout();
	MainLayout->addLayout(SettingsLayout);

	const QString FileHelp = tr("<p><b>File</b></p><p>Choose an output folder, file name, and image extension, such as .png, .jpg, .bmp, or .gif for an animated turntable.</p><p>When saving several steps, the step number is added before the extension. For example, 'model.png' becomes 'model01.png', 'model02.png', and so on.</p>");

	QGroupBox* FileGroup = new QGroupBox(tr("File"), this);
	FileGroup->setWhatsThis(FileHelp);

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
	SizeGroup->setWhatsThis(tr("<p><b>Size</b></p><p>Set the saved image width and height in pixels. Each value must be between 1 and 32768. Larger images use more memory and may exceed the graphics hardware limits.</p><p>Ratio links width and height so changing one updates the other. View uses the shape of the current view and Free unlinks them. Landscape and Portrait turn the image on its side.</p>"));

	QGridLayout* SizeLayout = new QGridLayout(SizeGroup);
	mGridLayouts.append(SizeLayout);

	QLabel* RatioLabel = new QLabel(tr("Rati&o"), SizeGroup);
	SizeLayout->addWidget(RatioLabel, 0, 0);

	mRatioComboBox = new QComboBox(SizeGroup);
	mRatioComboBox->setToolTip(tr("Shape of the saved image, width to height."));
	mRatioComboBox->addItem(tr("Free"), 0.0);
	mRatioComboBox->addItem(tr("View"), -1.0);
	mRatioComboBox->insertSeparator(mRatioComboBox->count());
	mRatioComboBox->addItem(tr("1:1 Square"), 1.0);
	mRatioComboBox->addItem(QLatin1String("5:4"), 5.0 / 4.0);
	mRatioComboBox->addItem(QLatin1String("4:3"), 4.0 / 3.0);
	mRatioComboBox->addItem(QLatin1String("3:2"), 3.0 / 2.0);
	mRatioComboBox->addItem(QLatin1String("16:10"), 16.0 / 10.0);
	mRatioComboBox->addItem(QLatin1String("16:9"), 16.0 / 9.0);
	mRatioComboBox->addItem(QLatin1String("21:9"), 21.0 / 9.0);
	RatioLabel->setBuddy(mRatioComboBox);
	SizeLayout->addWidget(mRatioComboBox, 0, 1);

	QLabel* WidthLabel = new QLabel(tr("&Width"), SizeGroup);
	SizeLayout->addWidget(WidthLabel, 1, 0);

	mWidthSpinBox = new swRangeSpinBox(SizeGroup);
	mWidthSpinBox->setRange(1, SW_MAXIMUM_IMAGE_DIMENSION);
	mWidthSpinBox->setValue(Options->Width);
	mWidthSpinBox->setToolTip(tr("Width of the saved image, in pixels (1 to 32768)."));
	WidthLabel->setBuddy(mWidthSpinBox);
	SizeLayout->addWidget(mWidthSpinBox, 1, 1);

	QLabel* HeightLabel = new QLabel(tr("&Height"), SizeGroup);
	SizeLayout->addWidget(HeightLabel, 2, 0);

	mHeightSpinBox = new swRangeSpinBox(SizeGroup);
	mHeightSpinBox->setRange(1, SW_MAXIMUM_IMAGE_DIMENSION);
	mHeightSpinBox->setValue(Options->Height);
	mHeightSpinBox->setToolTip(tr("Height of the saved image, in pixels (1 to 32768)."));
	HeightLabel->setBuddy(mHeightSpinBox);
	SizeLayout->addWidget(mHeightSpinBox, 2, 1);

	QHBoxLayout* OrientationLayout = new QHBoxLayout();
	SizeLayout->addLayout(OrientationLayout, 3, 0, 1, 2);

	mLandscapeButton = new QPushButton(tr("&Landscape"), SizeGroup);
	mLandscapeButton->setCheckable(true);
	mLandscapeButton->setToolTip(tr("Make the image wider than it is tall."));
	mLandscapeButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	mLandscapeButton->setAutoDefault(false);
	OrientationLayout->addWidget(mLandscapeButton, 1);

	mPortraitButton = new QPushButton(tr("&Portrait"), SizeGroup);
	mPortraitButton->setCheckable(true);
	mPortraitButton->setToolTip(tr("Make the image taller than it is wide."));
	mPortraitButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	mPortraitButton->setAutoDefault(false);
	OrientationLayout->addWidget(mPortraitButton, 1);

	// pick the preset matching the size
	const int LongSide = qMax(Options->Width, Options->Height);
	const int ShortSide = qMax(qMin(Options->Width, Options->Height), 1);
	mPortrait = Options->Height > Options->Width;

	for (int ItemIndex = 3; ItemIndex < mRatioComboBox->count(); ItemIndex++)
	{
		if (qAbs(mRatioComboBox->itemData(ItemIndex).toDouble() - static_cast<double>(LongSide) / ShortSide) < 0.01)
			mRatioComboBox->setCurrentIndex(ItemIndex);
	}

	// steps group
	QGroupBox* StepsGroup = new QGroupBox(tr("Steps"), this);
	StepsGroup->setWhatsThis(tr("<p><b>Steps</b></p><p>Choose Current Step, All Steps, or Range. The range fields are used only for Range. Both ends are included, and they may be equal to save just one step.</p>"));

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

	// turntable group
	QGroupBox* TurntableGroup = new QGroupBox(tr("Turntable"), this);
	TurntableGroup->setWhatsThis(tr("<p><b>Turntable</b></p><p>Animated GIF saves the model turning around one axis through the Framing center, starting from the current view. The animation loops forever.</p><p>Full Turn makes one seamless revolution. Angle swings between two angles from the current view and back again.</p><p>Frames sets how many images make up the loop and Duration sets how long one loop plays.</p>"));

	QGridLayout* TurntableLayout = new QGridLayout(TurntableGroup);
	mGridLayouts.append(TurntableLayout);

	mAnimatedCheckBox = new QCheckBox(tr("Animated &GIF"), TurntableGroup);
	mAnimatedCheckBox->setToolTip(tr("Save a looping GIF of the model turning."));
	TurntableLayout->addWidget(mAnimatedCheckBox, 0, 0, 1, 2);

	QLabel* AxisLabel = new QLabel(tr("A&xis"), TurntableGroup);
	TurntableLayout->addWidget(AxisLabel, 1, 0);

	mAxisComboBox = new QComboBox(TurntableGroup);
	mAxisComboBox->setToolTip(tr("Axis the model turns around."));
	mAxisComboBox->addItem(QLatin1String("X"));
	mAxisComboBox->addItem(QLatin1String("Y"));
	mAxisComboBox->addItem(QLatin1String("Z"));
	mAxisComboBox->setCurrentIndex(static_cast<int>(Options->Turntable.Axis));
	AxisLabel->setBuddy(mAxisComboBox);
	TurntableLayout->addWidget(mAxisComboBox, 1, 1);

	mFullTurnRadio = new QRadioButton(tr("&Full Turn"), TurntableGroup);
	mFullTurnRadio->setToolTip(tr("Turn the model once all the way around."));
	mFullTurnRadio->setChecked(Options->Turntable.FullTurn);
	TurntableLayout->addWidget(mFullTurnRadio, 2, 0, 1, 2);

	mAngleRadio = new QRadioButton(tr("A&ngle"), TurntableGroup);
	mAngleRadio->setToolTip(tr("Swing the model between two angles and back."));
	mAngleRadio->setChecked(!Options->Turntable.FullTurn);
	TurntableLayout->addWidget(mAngleRadio, 3, 0);

	QHBoxLayout* AngleLayout = new QHBoxLayout();
	TurntableLayout->addLayout(AngleLayout, 3, 1);

	mAngleStartSpinBox = new swRangeSpinBox(TurntableGroup);
	mAngleStartSpinBox->setRange(-360, 360);
	mAngleStartSpinBox->setSuffix(QString::fromUtf8("°"));
	mAngleStartSpinBox->setValue(Options->Turntable.AngleStart);
	mAngleStartSpinBox->setToolTip(tr("First angle, measured from the current view."));
	mAngleStartSpinBox->setAccessibleName(tr("First Angle"));
	mAngleStartSpinBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	AngleLayout->addWidget(mAngleStartSpinBox, 1);

	mAngleEndSpinBox = new swRangeSpinBox(TurntableGroup);
	mAngleEndSpinBox->setRange(-360, 360);
	mAngleEndSpinBox->setSuffix(QString::fromUtf8("°"));
	mAngleEndSpinBox->setValue(Options->Turntable.AngleEnd);
	mAngleEndSpinBox->setToolTip(tr("Last angle, measured from the current view."));
	mAngleEndSpinBox->setAccessibleName(tr("Last Angle"));
	mAngleEndSpinBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	AngleLayout->addWidget(mAngleEndSpinBox, 1);

	QLabel* FramesLabel = new QLabel(tr("Fra&mes"), TurntableGroup);
	TurntableLayout->addWidget(FramesLabel, 4, 0);

	mFramesSpinBox = new swRangeSpinBox(TurntableGroup);
	mFramesSpinBox->setRange(2, 360);
	mFramesSpinBox->setValue(Options->Turntable.Frames);
	mFramesSpinBox->setToolTip(tr("Number of images in one loop (2 to 360)."));
	FramesLabel->setBuddy(mFramesSpinBox);
	TurntableLayout->addWidget(mFramesSpinBox, 4, 1);

	QLabel* DurationLabel = new QLabel(tr("&Duration"), TurntableGroup);
	TurntableLayout->addWidget(DurationLabel, 5, 0);

	mDurationSpinBox = new swRangeDoubleSpinBox(TurntableGroup);
	mDurationSpinBox->setRange(0.1, 60.0);
	mDurationSpinBox->setDecimals(1);
	mDurationSpinBox->setSingleStep(0.5);
	mDurationSpinBox->setSuffix(tr(" s"));
	mDurationSpinBox->setValue(Options->Turntable.Duration);
	mDurationSpinBox->setToolTip(tr("Time one loop takes to play, in seconds."));
	DurationLabel->setBuddy(mDurationSpinBox);
	TurntableLayout->addWidget(mDurationSpinBox, 5, 1);

	// framing group
	QGroupBox* FramingGroup = new QGroupBox(tr("Framing"), this);
	FramingGroup->setWhatsThis(tr("<p><b>Framing</b></p><p>Current View saves exactly what the view shows. Fit Model aims at the model and moves the camera back until every part fits inside the image at every turntable angle, so animations turn in place without zooming. Fit Each Step fits every step on its own parts. Fitted stills are centered on the model.</p><p>Center sets the point the turntable turns around and the camera aims at. World Origin is the 0, 0, 0 point of the scene, Model Origin is the middle of the model base and Model Center is the middle of the model.</p><p>Margin keeps empty space around the model. Bounds measures the fit on the whole model, so every step lines up, or only on the steps being saved.</p>"));

	QGridLayout* FramingLayout = new QGridLayout(FramingGroup);
	mGridLayouts.append(FramingLayout);

	QLabel* FitLabel = new QLabel(tr("Fram&ing"), FramingGroup);
	FramingLayout->addWidget(FitLabel, 0, 0);

	mFitComboBox = new QComboBox(FramingGroup);
	mFitComboBox->setToolTip(tr("How the camera frames the model."));
	mFitComboBox->addItem(tr("Current View"));
	mFitComboBox->addItem(tr("Fit Model"));
	mFitComboBox->addItem(tr("Fit Each Step"));
	mFitComboBox->setCurrentIndex(static_cast<int>(Options->Framing.Fit));
	FitLabel->setBuddy(mFitComboBox);
	FramingLayout->addWidget(mFitComboBox, 0, 1);

	QLabel* CenterLabel = new QLabel(tr("C&enter"), FramingGroup);
	FramingLayout->addWidget(CenterLabel, 1, 0);

	mCenterComboBox = new QComboBox(FramingGroup);
	mCenterComboBox->setToolTip(tr("Point the camera aims at and the turntable turns around."));
	mCenterComboBox->addItem(tr("World Origin"));
	mCenterComboBox->addItem(tr("Model Origin"));
	mCenterComboBox->addItem(tr("Model Center"));
	mCenterComboBox->setCurrentIndex(static_cast<int>(Options->Framing.Center));
	CenterLabel->setBuddy(mCenterComboBox);
	FramingLayout->addWidget(mCenterComboBox, 1, 1);

	QLabel* MarginLabel = new QLabel(tr("Margin"), FramingGroup);
	FramingLayout->addWidget(MarginLabel, 2, 0);

	mMarginSpinBox = new swRangeSpinBox(FramingGroup);
	mMarginSpinBox->setRange(0, 50);
	mMarginSpinBox->setSuffix(tr(" %"));
	mMarginSpinBox->setValue(Options->Framing.Margin);
	mMarginSpinBox->setToolTip(tr("Empty space around the model, in percent of the image (0 to 50)."));
	MarginLabel->setBuddy(mMarginSpinBox);
	FramingLayout->addWidget(mMarginSpinBox, 2, 1);

	QLabel* ScopeLabel = new QLabel(tr("&Bounds"), FramingGroup);
	FramingLayout->addWidget(ScopeLabel, 3, 0);

	mScopeComboBox = new QComboBox(FramingGroup);
	mScopeComboBox->setToolTip(tr("Parts the fit and center are measured on."));
	mScopeComboBox->addItem(tr("Whole Model"));
	mScopeComboBox->addItem(tr("Shown Steps"));
	mScopeComboBox->setCurrentIndex(static_cast<int>(Options->Framing.Scope));
	ScopeLabel->setBuddy(mScopeComboBox);
	FramingLayout->addWidget(mScopeComboBox, 3, 1);

	// background group
	QGroupBox* BackgroundGroup = new QGroupBox(tr("Background"), this);
	BackgroundGroup->setWhatsThis(tr("<p><b>Background</b></p><p>View Color fills the background with the view background. Transparent leaves it empty so the model can be placed over other images.</p><p>Only PNG and GIF files keep transparency. GIF edges are hard, without smoothing.</p>"));

	QGridLayout* BackgroundLayout = new QGridLayout(BackgroundGroup);
	mGridLayouts.append(BackgroundLayout);

	mViewColorRadio = new QRadioButton(tr("&View Color"), BackgroundGroup);
	mViewColorRadio->setToolTip(tr("Fill the background with the view background."));
	mViewColorRadio->setChecked(true);
	BackgroundLayout->addWidget(mViewColorRadio, 0, 0, 1, 2);

	mTransparentRadio = new QRadioButton(tr("&Transparent"), BackgroundGroup);
	BackgroundLayout->addWidget(mTransparentRadio, 1, 0, 1, 2);

	// background to file back to front
	SettingsLayout->addWidget(BackgroundGroup);
	SettingsLayout->addWidget(StepsGroup);
	SettingsLayout->addWidget(TurntableGroup);
	SettingsLayout->addWidget(FramingGroup);
	SettingsLayout->addWidget(SizeGroup);
	SettingsLayout->addStretch(1);
	SettingsLayout->addWidget(FileGroup);

	for (QGridLayout* GridLayout : mGridLayouts)
		GridLayout->setColumnStretch(1, 1);

	swSetupDialogHelp(this, nullptr);

	// redraw once edits settle
	mPreviewTimer = new QTimer(this);
	mPreviewTimer->setSingleShot(true);
	mPreviewTimer->setInterval(150);

	// step through animation frames
	mAnimationTimer = new QTimer(this);

	connect(mPreviewTimer, &QTimer::timeout, this, &swImageDialog::RenderPreview);
	connect(mAnimationTimer, &QTimer::timeout, this, &swImageDialog::AdvancePreviewFrame);
	connect(mPlayButton, &QToolButton::clicked, this, [this]() { SetPlaying(!mPlaying); });
	connect(mAnimatedCheckBox, &QCheckBox::toggled, this, &swImageDialog::AnimatedChanged);
	connect(mAxisComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &swImageDialog::UpdateTurntable);
	connect(mFullTurnRadio, &QRadioButton::toggled, this, &swImageDialog::UpdateTurntable);
	connect(mAngleStartSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &swImageDialog::UpdateTurntable);
	connect(mAngleEndSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &swImageDialog::UpdateTurntable);
	connect(mFramesSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &swImageDialog::UpdateTurntable);
	connect(mDurationSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &swImageDialog::UpdateTurntable);
	connect(mFitComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &swImageDialog::UpdateFraming);
	connect(mCenterComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &swImageDialog::UpdateFraming);
	connect(mMarginSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &swImageDialog::UpdateFraming);
	connect(mScopeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &swImageDialog::UpdateFraming);
	connect(BrowseButton, &QToolButton::clicked, this, &swImageDialog::BrowseFile);
	connect(SaveButton, &QPushButton::clicked, this, &swImageDialog::accept);
	connect(CancelButton, &QPushButton::clicked, this, &swImageDialog::reject);
	connect(mFileEdit, &QLineEdit::textChanged, this, &swImageDialog::UpdateBackground);
	connect(mWidthSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &swImageDialog::WidthChanged);
	connect(mHeightSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &swImageDialog::HeightChanged);
	connect(mRatioComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &swImageDialog::RatioChanged);
	connect(mLandscapeButton, &QPushButton::clicked, this, [this]() { SetPortrait(false); });
	connect(mPortraitButton, &QPushButton::clicked, this, [this]() { SetPortrait(true); });
	connect(mPreviousButton, &QToolButton::clicked, this, [this]() { ShowPreviewStep(-1); });
	connect(mNextButton, &QToolButton::clicked, this, [this]() { ShowPreviewStep(1); });
	connect(mViewColorRadio, &QRadioButton::toggled, this, &swImageDialog::UpdatePreview);
	connect(mViewColorRadio, &QRadioButton::clicked, this, [this]() { mTransparentChosen = false; });
	connect(mTransparentRadio, &QRadioButton::clicked, this, [this]() { mTransparentChosen = true; });

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

	UpdateOrientation();
	UpdateBackground();
	UpdateTurntable();
	UpdateRange();
	SetPlaying(true);
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
	mOptions->Background = mTransparentRadio->isChecked() ? swImageBackground::Transparent : swImageBackground::Opaque;
	mOptions->Turntable = GetTurntable();
	mOptions->Framing = GetFraming();

	QDialog::accept();
}

// use to pick the output file
void swImageDialog::BrowseFile()
{
	const QString FilePath = QFileDialog::getSaveFileName(this, tr("Save Image File"), mFileEdit->text(), tr("Supported Image Files (*.bmp *.png *.jpg *.gif);;BMP Files (*.bmp);;PNG Files (*.png);;JPEG Files (*.jpg);;Animated GIF Files (*.gif);;All Files (*.*)"));

	if (!FilePath.isEmpty())
		mFileEdit->setText(QDir::toNativeSeparators(FilePath));
}

void swImageDialog::WidthChanged(int Width)
{
	const double Ratio = GetRatio();

	if (Ratio > 0.0)
	{
		const QSignalBlocker Blocker(mHeightSpinBox);
		mHeightSpinBox->setValue(qBound(1, qRound(Width / Ratio), SW_MAXIMUM_IMAGE_DIMENSION));
	}
	else
		mPortrait = mHeightSpinBox->value() > Width;

	UpdateOrientation();
	UpdatePreview();
}

void swImageDialog::HeightChanged(int Height)
{
	const double Ratio = GetRatio();

	if (Ratio > 0.0)
	{
		const QSignalBlocker Blocker(mWidthSpinBox);
		mWidthSpinBox->setValue(qBound(1, qRound(Height * Ratio), SW_MAXIMUM_IMAGE_DIMENSION));
	}
	else
		mPortrait = Height > mWidthSpinBox->value();

	UpdateOrientation();
	UpdatePreview();
}

// use to get width over height
double swImageDialog::GetRatio() const
{
	const double Preset = mRatioComboBox->currentData().toDouble();

	// view gives its own shape
	if (Preset < 0.0)
	{
		const swView* ActiveView = gMainWindow ? gMainWindow->GetActiveView() : nullptr;

		if (!ActiveView || ActiveView->GetWidth() < 1 || ActiveView->GetHeight() < 1)
			return 0.0;

		return static_cast<double>(ActiveView->GetWidth()) / ActiveView->GetHeight();
	}

	if (Preset == 0.0)
		return 0.0;

	return mPortrait ? 1.0 / Preset : Preset;
}

void swImageDialog::RatioChanged()
{
	if (mRatioComboBox->currentData().toDouble() < 0.0)
		mPortrait = GetRatio() > 0.0 && GetRatio() < 1.0;

	const double Ratio = GetRatio();

	if (Ratio > 0.0)
	{
		const QSignalBlocker Blocker(mHeightSpinBox);
		mHeightSpinBox->setValue(qBound(1, qRound(mWidthSpinBox->value() / Ratio), SW_MAXIMUM_IMAGE_DIMENSION));
	}

	UpdateOrientation();
	UpdatePreview();
}

// use to turn the image on its side
void swImageDialog::SetPortrait(bool Portrait)
{
	if (Portrait != mPortrait)
	{
		mPortrait = Portrait;

		const int Width = mWidthSpinBox->value();
		const QSignalBlocker WidthBlocker(mWidthSpinBox);
		const QSignalBlocker HeightBlocker(mHeightSpinBox);

		mWidthSpinBox->setValue(mHeightSpinBox->value());
		mHeightSpinBox->setValue(Width);
	}

	UpdateOrientation();
	UpdatePreview();
}

void swImageDialog::UpdateOrientation()
{
	const double Preset = mRatioComboBox->currentData().toDouble();

	// view and square keep their shape
	const bool Turnable = Preset >= 0.0 && Preset != 1.0;

	mLandscapeButton->setEnabled(Turnable);
	mPortraitButton->setEnabled(Turnable);
	mLandscapeButton->setChecked(Turnable && !mPortrait);
	mPortraitButton->setChecked(Turnable && mPortrait);
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

// use to match the settings to the file type
void swImageDialog::UpdateBackground()
{
	const QString Suffix = QFileInfo(mFileEdit->text()).suffix().toLower();
	const bool Animated = Suffix == QLatin1String("gif");
	const bool Transparency = Animated || Suffix == QLatin1String("png");

	// gif files are always animated
	if (Animated != mAnimatedCheckBox->isChecked())
	{
		const QSignalBlocker Blocker(mAnimatedCheckBox);
		mAnimatedCheckBox->setChecked(Animated);
		UpdateTurntable();
	}

	if (!Animated && !Suffix.isEmpty())
		mStillSuffix = Suffix;

	mTransparentRadio->setEnabled(Transparency);

	// keep the chosen background through typing
	if (!Transparency && mTransparentRadio->isChecked())
		mViewColorRadio->setChecked(true);
	else if (Transparency && mTransparentChosen && !mTransparentRadio->isChecked())
		mTransparentRadio->setChecked(true);

	if (Transparency)
		mTransparentRadio->setToolTip(tr("Leave the background transparent."));
	else
		mTransparentRadio->setToolTip(tr("Transparent backgrounds need a PNG or GIF file."));
}

// use to swap the file extension
void swImageDialog::AnimatedChanged(bool Animated)
{
	const QString FilePath = mFileEdit->text();
	const QString Suffix = QFileInfo(FilePath).suffix();
	const QString NewSuffix = Animated ? QLatin1String("gif") : mStillSuffix;
	QString BaseName = FilePath;

	if (!Suffix.isEmpty())
		BaseName.chop(Suffix.length() + 1);

	mFileEdit->setText(BaseName + QLatin1Char('.') + NewSuffix);
	UpdateTurntable();
}

void swImageDialog::UpdateTurntable()
{
	const bool Animated = mAnimatedCheckBox->isChecked();

	mAxisComboBox->setEnabled(Animated);
	mFullTurnRadio->setEnabled(Animated);
	mAngleRadio->setEnabled(Animated);
	mAngleStartSpinBox->setEnabled(Animated && mAngleRadio->isChecked());
	mAngleEndSpinBox->setEnabled(Animated && mAngleRadio->isChecked());
	mFramesSpinBox->setEnabled(Animated);
	mDurationSpinBox->setEnabled(Animated);
	mPlayButton->setVisible(Animated);

	UpdateFraming();
}

void swImageDialog::UpdateFraming()
{
	const swImageFraming Framing = GetFraming();
	const bool Fit = Framing.Fit != swImageFit::CurrentView;

	// settings without effect stay off
	mCenterComboBox->setEnabled(mAnimatedCheckBox->isChecked());
	mMarginSpinBox->setEnabled(Fit);
	mScopeComboBox->setEnabled(Framing.Fit == swImageFit::FitModel || (!Fit && mAnimatedCheckBox->isChecked() && Framing.Center != swImageCenter::WorldOrigin));

	UpdatePreview();
}

swImageFraming swImageDialog::GetFraming() const
{
	swImageFraming Framing;

	Framing.Fit = static_cast<swImageFit>(mFitComboBox->currentIndex());
	Framing.Center = static_cast<swImageCenter>(mCenterComboBox->currentIndex());
	Framing.Scope = static_cast<swImageFitScope>(mScopeComboBox->currentIndex());
	Framing.Margin = mMarginSpinBox->value();

	return Framing;
}

swImageTurntable swImageDialog::GetTurntable() const
{
	swImageTurntable Turntable;

	Turntable.Enabled = mAnimatedCheckBox->isChecked();
	Turntable.Axis = static_cast<swTurntableAxis>(mAxisComboBox->currentIndex());
	Turntable.FullTurn = mFullTurnRadio->isChecked();
	Turntable.AngleStart = mAngleStartSpinBox->value();
	Turntable.AngleEnd = mAngleEndSpinBox->value();
	Turntable.Frames = mFramesSpinBox->value();
	Turntable.Duration = mDurationSpinBox->value();

	return Turntable;
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

	if (mAnimatedCheckBox->isChecked())
		mSizeLabel->setText(QString::fromUtf8("%1 × %2 · %3").arg(mWidthSpinBox->value()).arg(mHeightSpinBox->value()).arg(tr("%1 frames").arg(mFramesSpinBox->value())));
	else
		mSizeLabel->setText(QString::fromUtf8("%1 × %2").arg(mWidthSpinBox->value()).arg(mHeightSpinBox->value()));

	// settings changes drop old frames
	mAnimationTimer->stop();
	mPreviewFrames.clear();

	mPreviewTimer->start();
}

// use to render the preview at widget size
void swImageDialog::RenderPreview()
{
	if (!mModel || !isVisible())
		return;

	mPreviewTransparent = mTransparentRadio->isChecked();

	if (mAnimatedCheckBox->isChecked())
	{
		mPreviewFrames.resize(mFramesSpinBox->value());
		mPreviewFrame = qBound(0, mPreviewFrame, mPreviewFrames.size() - 1);
		ShowPreviewFrame();

		if (mPlaying)
			mAnimationTimer->start(GetTurntable().GetFrameDelay(mPreviewFrame) * 10);

		return;
	}

	const qreal PixelRatio = mPreview->devicePixelRatioF();
	const QSize ImageSize(mWidthSpinBox->value(), mHeightSpinBox->value());
	const QSize RenderSize = ImageSize.scaled(mPreview->size() * PixelRatio, Qt::KeepAspectRatio).expandedTo(QSize(1, 1));
	const swImageBackground Background = mPreviewTransparent ? swImageBackground::Transparent : swImageBackground::Opaque;

	QImage Image = mModel->GetFramedImage(RenderSize.width(), RenderSize.height(), mPreviewStep, GetRangeEnd(), Background, GetFraming(), GetTurntable(), 0);
	Image.setDevicePixelRatio(PixelRatio);

	mPreview->SetImage(Image, mPreviewTransparent);
}

// use to show one turntable frame
void swImageDialog::ShowPreviewFrame()
{
	if (mPreviewFrame >= mPreviewFrames.size())
		return;

	QImage& Image = mPreviewFrames[mPreviewFrame];

	// frames render on first use
	if (Image.isNull())
	{
		const qreal PixelRatio = mPreview->devicePixelRatioF();
		const QSize ImageSize(mWidthSpinBox->value(), mHeightSpinBox->value());
		const QSize RenderSize = ImageSize.scaled(mPreview->size() * PixelRatio, Qt::KeepAspectRatio).expandedTo(QSize(1, 1));
		const swImageBackground Background = mPreviewTransparent ? swImageBackground::Transparent : swImageBackground::Opaque;

		Image = mModel->GetFramedImage(RenderSize.width(), RenderSize.height(), mPreviewStep, GetRangeEnd(), Background, GetFraming(), GetTurntable(), mPreviewFrame);
		Image.setDevicePixelRatio(PixelRatio);
	}

	mPreview->SetImage(Image, mPreviewTransparent);
}

void swImageDialog::AdvancePreviewFrame()
{
	if (mPreviewFrames.isEmpty())
		return;

	mPreviewFrame = (mPreviewFrame + 1) % mPreviewFrames.size();
	ShowPreviewFrame();

	// frames may differ by a hundredth
	mAnimationTimer->start(GetTurntable().GetFrameDelay(mPreviewFrame) * 10);
}

void swImageDialog::SetPlaying(bool Playing)
{
	mPlaying = Playing;

	if (Playing)
	{
		mPlayButton->setIcon(swCreatePaintedIcon(swPaintedIcon::TimePause));
		mPlayButton->setToolTip(tr("Pause the animation."));

		if (!mPreviewFrames.isEmpty())
			mAnimationTimer->start(GetTurntable().GetFrameDelay(mPreviewFrame) * 10);
	}
	else
	{
		mPlayButton->setIcon(swCreatePaintedIcon(swPaintedIcon::TimePlay));
		mPlayButton->setToolTip(tr("Play the animation."));
		mAnimationTimer->stop();
	}
}
