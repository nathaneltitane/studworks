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

	const QString FileHelp = tr("<p><b>File</b></p><p>Choose an output folder, file name, and image extension, such as .png, .jpg, .bmp, or .gif for an animation.</p><p>When saving several steps, the step number is added before the extension. For example, 'model.png' becomes 'model01.png', 'model02.png', and so on.</p>");

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
	mPortrait = Options->Height > Options->Width;

	// last ratio used
	for (int ItemIndex = 0; ItemIndex < mRatioComboBox->count(); ItemIndex++)
	{
		if (!mRatioComboBox->itemData(ItemIndex).isNull() && qAbs(mRatioComboBox->itemData(ItemIndex).toDouble() - Options->Ratio) < 0.01)
			mRatioComboBox->setCurrentIndex(ItemIndex);
	}

	// steps group
	QGroupBox* StepsGroup = new QGroupBox(tr("Steps"), this);
	StepsGroup->setWhatsThis(tr("<p><b>Steps</b></p><p>Choose Current Step, All Steps, or Range. The range fields are used only for Range. Both ends are included, and they may be equal to save just one step.</p>"));

	QGridLayout* StepsLayout = new QGridLayout(StepsGroup);
	mGridLayouts.append(StepsLayout);

	mCurrentRadio = new QRadioButton(tr("&Current Step"), StepsGroup);
	mCurrentRadio->setToolTip(tr("Save only the current step."));
	mCurrentRadio->setChecked(Options->Steps == swImageSteps::Current);
	StepsLayout->addWidget(mCurrentRadio, 0, 0, 1, 2);

	mAllRadio = new QRadioButton(tr("&All Steps"), StepsGroup);
	mAllRadio->setToolTip(tr("Save an image for every step in the active model."));
	mAllRadio->setChecked(Options->Steps == swImageSteps::All);
	StepsLayout->addWidget(mAllRadio, 1, 0, 1, 2);

	mRangeRadio = new QRadioButton(tr("&Range"), StepsGroup);
	mRangeRadio->setToolTip(tr("Save the steps between the first and last step, including both."));
	mRangeRadio->setChecked(Options->Steps == swImageSteps::Range);
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

	// animation group
	QGroupBox* TurntableGroup = new QGroupBox(tr("Animation"), this);
	TurntableGroup->setWhatsThis(tr("<p><b>Animation</b></p><p>Animated GIF saves a looping animation. The animation loops forever.</p><p>Turntable turns the model around one axis through the Framing center, starting from the current view. Full Turn makes seamless revolutions. Angle swings between two angles from the current view and back again. Turns sets how many revolutions or swings make up the loop, so complex builds can be shown from every side more than once.</p><p>Next Step moves through the steps chosen in the Steps group while the model turns, one step every few turns or seconds. The loop ends on the last step and starts again.</p><p>Steps plays the chosen steps one per frame, for short animated instructions.</p><p>Frames sets how many images make up one turn and Frame Rate how many play each second. Hold Last keeps the finished model on screen before a loop through the steps restarts. Duration shows how long one loop plays.</p>"));

	QGridLayout* TurntableLayout = new QGridLayout(TurntableGroup);
	mGridLayouts.append(TurntableLayout);

	mAnimatedCheckBox = new QCheckBox(tr("Animated &GIF"), TurntableGroup);
	mAnimatedCheckBox->setToolTip(tr("Save a looping GIF of the model turning or of the steps."));
	TurntableLayout->addWidget(mAnimatedCheckBox, 0, 0, 1, 2);

	QLabel* MotionLabel = new QLabel(tr("Motion"), TurntableGroup);
	TurntableLayout->addWidget(MotionLabel, 1, 0);

	mMotionComboBox = new QComboBox(TurntableGroup);
	mMotionComboBox->setToolTip(tr("Turn the model or play the steps one per frame."));
	mMotionComboBox->addItem(tr("Turntable"));
	mMotionComboBox->addItem(tr("Steps"));
	mMotionComboBox->setCurrentIndex(static_cast<int>(Options->Turntable.Motion));
	MotionLabel->setBuddy(mMotionComboBox);
	TurntableLayout->addWidget(mMotionComboBox, 1, 1);

	QLabel* AxisLabel = new QLabel(tr("A&xis"), TurntableGroup);
	TurntableLayout->addWidget(AxisLabel, 2, 0);

	mAxisComboBox = new QComboBox(TurntableGroup);
	mAxisComboBox->setToolTip(tr("Axis the model turns around."));
	mAxisComboBox->addItem(QLatin1String("X"));
	mAxisComboBox->addItem(QLatin1String("Y"));
	mAxisComboBox->addItem(QLatin1String("Z"));
	mAxisComboBox->setCurrentIndex(static_cast<int>(Options->Turntable.Axis));
	AxisLabel->setBuddy(mAxisComboBox);
	TurntableLayout->addWidget(mAxisComboBox, 2, 1);

	mFullTurnRadio = new QRadioButton(tr("&Full Turn"), TurntableGroup);
	mFullTurnRadio->setToolTip(tr("Turn the model once all the way around."));
	mFullTurnRadio->setChecked(Options->Turntable.FullTurn);
	TurntableLayout->addWidget(mFullTurnRadio, 3, 0, 1, 2);

	mAngleRadio = new QRadioButton(tr("A&ngle"), TurntableGroup);
	mAngleRadio->setToolTip(tr("Swing the model between two angles and back."));
	mAngleRadio->setChecked(!Options->Turntable.FullTurn);
	TurntableLayout->addWidget(mAngleRadio, 4, 0);

	QHBoxLayout* AngleLayout = new QHBoxLayout();
	TurntableLayout->addLayout(AngleLayout, 4, 1);

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

	QLabel* NextStepLabel = new QLabel(tr("Next Step"), TurntableGroup);
	TurntableLayout->addWidget(NextStepLabel, 5, 0);

	QHBoxLayout* NextStepLayout = new QHBoxLayout();
	TurntableLayout->addLayout(NextStepLayout, 5, 1);

	mNextStepComboBox = new QComboBox(TurntableGroup);
	mNextStepComboBox->setToolTip(tr("Move to the next step while the model turns."));
	mNextStepComboBox->addItem(tr("Off"));
	mNextStepComboBox->addItem(tr("Turns"));
	mNextStepComboBox->addItem(tr("Seconds"));
	mNextStepComboBox->setCurrentIndex(static_cast<int>(Options->Turntable.NextStep));
	mNextStepComboBox->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
	NextStepLabel->setBuddy(mNextStepComboBox);
	NextStepLayout->addWidget(mNextStepComboBox, 1);

	mNextStepTurns = qBound(1, Options->Turntable.NextStepTurns, 10);
	mNextStepSeconds = qBound(0.1, Options->Turntable.NextStepSeconds, 60.0);

	mNextStepSpinBox = new swRangeDoubleSpinBox(TurntableGroup);
	mNextStepSpinBox->setAccessibleName(tr("Next Step Interval"));
	mNextStepSpinBox->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
	NextStepLayout->addWidget(mNextStepSpinBox, 1);

	QLabel* TurnsLabel = new QLabel(tr("T&urns"), TurntableGroup);
	TurntableLayout->addWidget(TurnsLabel, 6, 0);

	mTurnsSpinBox = new swRangeSpinBox(TurntableGroup);
	mTurnsSpinBox->setRange(1, 10);
	mTurns = qBound(1, Options->Turntable.Turns, 10);
	mTurnsSpinBox->setValue(mTurns);
	mTurnsSpinBox->setToolTip(tr("Number of revolutions or swings in one loop (1 to 10)."));
	TurnsLabel->setBuddy(mTurnsSpinBox);
	TurntableLayout->addWidget(mTurnsSpinBox, 6, 1);

	QLabel* FramesLabel = new QLabel(tr("Fra&mes"), TurntableGroup);
	TurntableLayout->addWidget(FramesLabel, 7, 0);

	mTurntableFrames = qBound(2, Options->Turntable.Frames, 360);

	mFramesSpinBox = new swRangeSpinBox(TurntableGroup);
	mFramesSpinBox->setRange(2, 360);
	mFramesSpinBox->setValue(mTurntableFrames);
	FramesLabel->setBuddy(mFramesSpinBox);
	TurntableLayout->addWidget(mFramesSpinBox, 7, 1);

	QLabel* FrameRateLabel = new QLabel(tr("Frame Rate"), TurntableGroup);
	TurntableLayout->addWidget(FrameRateLabel, 8, 0);

	mFrameRateSpinBox = new swRangeDoubleSpinBox(TurntableGroup);
	mFrameRateSpinBox->setRange(0.5, 50.0);
	mFrameRateSpinBox->setDecimals(1);
	mFrameRateSpinBox->setSingleStep(1.0);
	mFrameRateSpinBox->setSuffix(tr(" fps"));
	mFrameRateSpinBox->setValue(Options->Turntable.FrameRate);
	mFrameRateSpinBox->setToolTip(tr("Frames shown each second (0.5 to 50)."));
	FrameRateLabel->setBuddy(mFrameRateSpinBox);
	TurntableLayout->addWidget(mFrameRateSpinBox, 8, 1);

	QLabel* HoldLastLabel = new QLabel(tr("Hol&d Last"), TurntableGroup);
	TurntableLayout->addWidget(HoldLastLabel, 9, 0);

	mHoldLastSpinBox = new swRangeDoubleSpinBox(TurntableGroup);
	mHoldLastSpinBox->setRange(0.0, 10.0);
	mHoldLastSpinBox->setDecimals(1);
	mHoldLastSpinBox->setSingleStep(0.5);
	mHoldLastSpinBox->setSuffix(tr(" s"));
	mHoldLastSpinBox->setValue(Options->Turntable.HoldLast);
	mHoldLastSpinBox->setToolTip(tr("Seconds the last step stays on screen before the loop restarts (0 to 10)."));
	HoldLastLabel->setBuddy(mHoldLastSpinBox);
	TurntableLayout->addWidget(mHoldLastSpinBox, 9, 1);

	QLabel* DurationLabel = new QLabel(tr("Duration"), TurntableGroup);
	TurntableLayout->addWidget(DurationLabel, 10, 0);

	mDurationLabel = new QLabel(TurntableGroup);
	mDurationLabel->setToolTip(tr("Time one loop takes to play, from the frames, frame rate and hold."));
	TurntableLayout->addWidget(mDurationLabel, 10, 1);

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
	mTransparentChosen = Options->Background == swImageBackground::Transparent;
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
	connect(mMotionComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &swImageDialog::UpdateTurntable);
	connect(mAxisComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &swImageDialog::UpdateTurntable);
	connect(mFullTurnRadio, &QRadioButton::toggled, this, &swImageDialog::UpdateTurntable);
	connect(mAngleStartSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &swImageDialog::UpdateTurntable);
	connect(mAngleEndSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &swImageDialog::UpdateTurntable);
	connect(mTurnsSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int Turns)
	{
		mTurns = Turns;
		UpdateTurntable();
	});
	connect(mNextStepComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &swImageDialog::UpdateTurntable);

	// each interval keeps its own value
	connect(mNextStepSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double Interval)
	{
		if (GetNextStep() == swImageNextStep::Seconds)
			mNextStepSeconds = Interval;
		else
			mNextStepTurns = qRound(Interval);

		UpdateTurntable();
	});
	connect(mFrameRateSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &swImageDialog::UpdateTurntable);
	connect(mHoldLastSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &swImageDialog::UpdateTurntable);

	// step counts leave the turntable frames alone
	connect(mFramesSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int Frames)
	{
		if (!IsStepMotion())
			mTurntableFrames = Frames;

		UpdateTurntable();
	});
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

	RatioChanged();
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
		FitWindow();
	}
}

// use to size the window around the preview
void swImageDialog::FitWindow()
{
	QBoxLayout* MainLayout = qobject_cast<QBoxLayout*>(layout());
	QFrame* PreviewFrame = qobject_cast<QFrame*>(mPreview->parentWidget());
	QBoxLayout* PreviewLayout = qobject_cast<QBoxLayout*>(PreviewFrame->layout());
	QLayout* SettingsLayout = MainLayout->itemAt(1)->layout();
	QLayoutItem* CaptionItem = PreviewLayout->itemAt(1);

	const QMargins MainMargins = MainLayout->contentsMargins();
	const QMargins PreviewMargins = PreviewLayout->contentsMargins();
	const int FrameWidth = PreviewFrame->frameWidth();

	// no gap below the settings column
	const int ColumnHeight = SettingsLayout->minimumSize().height();
	const int PreviewHeight = ColumnHeight - 2 * FrameWidth - PreviewMargins.top() - PreviewMargins.bottom() - PreviewLayout->spacing() - CaptionItem->sizeHint().height();

	const double Ratio = static_cast<double>(mWidthSpinBox->value()) / qMax(mHeightSpinBox->value(), 1);
	const int PreviewWidth = qMax(qRound(PreviewHeight * Ratio), qMax(mPreview->minimumWidth(), CaptionItem->minimumSize().width()));
	int Width = MainMargins.left() + PreviewWidth + 2 * FrameWidth + PreviewMargins.left() + PreviewMargins.right() + MainLayout->spacing() + SettingsLayout->sizeHint().width() + MainMargins.right();

	// stay on the screen
	if (const QScreen* Screen = screen())
		Width = qMin(Width, Screen->availableGeometry().width());

	resize(Width, ColumnHeight + MainMargins.top() + MainMargins.bottom());
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
	mOptions->Ratio = mRatioComboBox->currentData().toDouble();
	mOptions->Steps = mAllRadio->isChecked() ? swImageSteps::All : mRangeRadio->isChecked() ? swImageSteps::Range : swImageSteps::Current;
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

	if (mColumnsAligned)
		FitWindow();
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

	if (mColumnsAligned)
		FitWindow();
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

	if (GetTurntable().IsStepping())
		UpdateTurntable();
	else
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
	const bool StepMotion = IsStepMotion();
	const bool Turning = Animated && !StepMotion;
	const swImageNextStep NextStep = GetNextStep();

	mMotionComboBox->setEnabled(Animated);
	mAxisComboBox->setEnabled(Turning);
	mFullTurnRadio->setEnabled(Turning);
	mAngleRadio->setEnabled(Turning);
	mAngleStartSpinBox->setEnabled(Turning && mAngleRadio->isChecked());
	mAngleEndSpinBox->setEnabled(Turning && mAngleRadio->isChecked());
	mNextStepComboBox->setEnabled(Turning);
	mNextStepSpinBox->setEnabled(Turning && NextStep != swImageNextStep::Off);
	mTurnsSpinBox->setEnabled(Turning && NextStep == swImageNextStep::Off);
	mFramesSpinBox->setEnabled(Turning);

	// interval in turns or seconds
	{
		const QSignalBlocker Blocker(mNextStepSpinBox);

		if (NextStep == swImageNextStep::Seconds)
		{
			mNextStepSpinBox->setDecimals(1);
			mNextStepSpinBox->setRange(0.1, 60.0);
			mNextStepSpinBox->setSingleStep(0.5);
			mNextStepSpinBox->setSuffix(tr(" s"));
			mNextStepSpinBox->setValue(mNextStepSeconds);
			mNextStepSpinBox->setToolTip(tr("Seconds each step shows (0.1 to 60)."));
		}
		else
		{
			mNextStepSpinBox->setDecimals(0);
			mNextStepSpinBox->setRange(1.0, 10.0);
			mNextStepSpinBox->setSingleStep(1.0);
			mNextStepSpinBox->setSuffix(QString());
			mNextStepSpinBox->setValue(mNextStepTurns);
			mNextStepSpinBox->setToolTip(tr("Turns each step shows (1 to 10)."));
		}
	}

	mFrameRateSpinBox->setEnabled(Animated);
	mHoldLastSpinBox->setEnabled(Animated && (StepMotion || NextStep != swImageNextStep::Off));
	mPlayButton->setVisible(Animated);

	// steps give one frame each
	{
		const QSignalBlocker Blocker(mFramesSpinBox);

		if (StepMotion)
		{
			mFramesSpinBox->setRange(1, qMax(mLastStep, 1));
			mFramesSpinBox->setValue(GetRangeEnd() - GetRangeStart() + 1);
			mFramesSpinBox->setToolTip(tr("Steps play one per frame, set by the Steps group."));
		}
		else
		{
			mFramesSpinBox->setRange(2, 360);
			mFramesSpinBox->setValue(mTurntableFrames);
			mFramesSpinBox->setToolTip(tr("Number of images in one turn (2 to 360)."));
		}
	}

	const swImageTurntable Turntable = GetTurntable();
	const int FrameCount = Turntable.GetFrameCount(GetRangeEnd() - GetRangeStart() + 1);

	// stepping sets the turn count
	{
		const QSignalBlocker Blocker(mTurnsSpinBox);

		if (Turning && NextStep != swImageNextStep::Off)
		{
			mTurnsSpinBox->setRange(1, 100000);
			mTurnsSpinBox->setValue(qMax(qRound(static_cast<double>(FrameCount) / mTurntableFrames), 1));
		}
		else
		{
			mTurnsSpinBox->setRange(1, 10);
			mTurnsSpinBox->setValue(mTurns);
		}
	}

	mDurationLabel->setEnabled(Animated);
	mDurationLabel->setText(tr("%1 s").arg(QLocale().toString(FrameCount / mFrameRateSpinBox->value() + Turntable.GetHoldLast(), 'f', 1)));

	UpdateFraming();
}

void swImageDialog::UpdateFraming()
{
	const swImageFraming Framing = GetFraming();
	const bool Fit = Framing.Fit != swImageFit::CurrentView;

	// settings without effect stay off
	const bool Turning = mAnimatedCheckBox->isChecked() && !IsStepMotion();

	mCenterComboBox->setEnabled(Turning);
	mMarginSpinBox->setEnabled(Fit);
	mScopeComboBox->setEnabled(Framing.Fit == swImageFit::FitModel || (!Fit && Turning && Framing.Center != swImageCenter::WorldOrigin));

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
	Turntable.Motion = static_cast<swImageMotion>(mMotionComboBox->currentIndex());
	Turntable.Axis = static_cast<swTurntableAxis>(mAxisComboBox->currentIndex());
	Turntable.FullTurn = mFullTurnRadio->isChecked();
	Turntable.AngleStart = mAngleStartSpinBox->value();
	Turntable.AngleEnd = mAngleEndSpinBox->value();
	Turntable.Turns = mTurns;
	Turntable.Frames = mTurntableFrames;
	Turntable.FrameRate = mFrameRateSpinBox->value();
	Turntable.NextStep = GetNextStep();
	Turntable.NextStepTurns = mNextStepTurns;
	Turntable.NextStepSeconds = mNextStepSeconds;
	Turntable.HoldLast = mHoldLastSpinBox->value();

	return Turntable;
}

swImageNextStep swImageDialog::GetNextStep() const
{
	return static_cast<swImageNextStep>(mNextStepComboBox->currentIndex());
}

bool swImageDialog::IsStepMotion() const
{
	return mAnimatedCheckBox->isChecked() && mMotionComboBox->currentIndex() == static_cast<int>(swImageMotion::Steps);
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

	const swImageTurntable Turntable = GetTurntable();
	const int FrameCount = Turntable.GetFrameCount(GetRangeEnd() - GetRangeStart() + 1);

	// playing steps pick their own
	mPreviousButton->setVisible(!Turntable.IsStepping());
	mNextButton->setVisible(!Turntable.IsStepping());

	if (mAnimatedCheckBox->isChecked())
		mSizeLabel->setText(QString::fromUtf8("%1 × %2 · %3").arg(mWidthSpinBox->value()).arg(mHeightSpinBox->value()).arg(tr("%1 frames").arg(FrameCount)));
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
		mPreviewFrames.resize(GetTurntable().GetFrameCount(GetRangeEnd() - GetRangeStart() + 1));
		mPreviewFrame = qBound(0, mPreviewFrame, mPreviewFrames.size() - 1);
		ShowPreviewFrame();

		if (mPlaying)
			mAnimationTimer->start(GetTurntable().GetFrameDelay(mPreviewFrame, mPreviewFrames.size()) * 10);

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

		const swImageTurntable Turntable = GetTurntable();
		const int Step = Turntable.IsStepping() ? GetRangeStart() + Turntable.GetFrameStep(mPreviewFrame) : mPreviewStep;

		Image = mModel->GetFramedImage(RenderSize.width(), RenderSize.height(), Step, GetRangeEnd(), Background, GetFraming(), Turntable, mPreviewFrame);
		Image.setDevicePixelRatio(PixelRatio);
	}

	mPreview->SetImage(Image, mPreviewTransparent);

	const swImageTurntable Turntable = GetTurntable();

	if (Turntable.IsStepping())
		mStepLabel->setText(tr("Step %1 of %2").arg(GetRangeStart() + Turntable.GetFrameStep(mPreviewFrame)).arg(mLastStep));
}

void swImageDialog::AdvancePreviewFrame()
{
	if (mPreviewFrames.isEmpty())
		return;

	mPreviewFrame = (mPreviewFrame + 1) % mPreviewFrames.size();
	ShowPreviewFrame();

	// frames may differ by a hundredth
	mAnimationTimer->start(GetTurntable().GetFrameDelay(mPreviewFrame, mPreviewFrames.size()) * 10);
}

void swImageDialog::SetPlaying(bool Playing)
{
	mPlaying = Playing;

	if (Playing)
	{
		mPlayButton->setIcon(swCreatePaintedIcon(swPaintedIcon::TimePause));
		mPlayButton->setToolTip(tr("Pause the animation."));

		if (!mPreviewFrames.isEmpty())
			mAnimationTimer->start(GetTurntable().GetFrameDelay(mPreviewFrame, mPreviewFrames.size()) * 10);
	}
	else
	{
		mPlayButton->setIcon(swCreatePaintedIcon(swPaintedIcon::TimePlay));
		mPlayButton->setToolTip(tr("Play the animation."));
		mAnimationTimer->stop();
	}
}
