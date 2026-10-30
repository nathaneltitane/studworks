#pragma once

#include "sw_project.h"

class swModel;
class swRangeSpinBox;
class swRangeDoubleSpinBox;
class swImagePreviewWidget;

class swImageDialog : public QDialog
{
	Q_OBJECT

public:
	swImageDialog(QWidget* Parent, swImageDialogOptions* Options, swModel* Model);

public slots:
	void accept() override;

protected:
	void showEvent(QShowEvent* Event) override;

	// Shares one label column across the groups.
	void AlignColumns();

	void BrowseFile();
	void WidthChanged(int Width);
	void HeightChanged(int Height);
	void RatioChanged();
	void SetPortrait(bool Portrait);
	void UpdateOrientation();
	double GetRatio() const;
	void UpdateRange();
	void UpdateBackground();
	void ShowPreviewStep(int Offset);
	void UpdatePreview();
	void RenderPreview();

	// Animated GIF settings and the playing preview.
	void AnimatedChanged(bool Animated);
	void UpdateTurntable();
	swImageTurntable GetTurntable() const;

	// Camera fit and orbit center.
	void UpdateFraming();
	swImageFraming GetFraming() const;
	void ShowPreviewFrame();
	void AdvancePreviewFrame();
	void SetPlaying(bool Playing);
	int GetRangeStart() const;
	int GetRangeEnd() const;

	swImageDialogOptions* mOptions = nullptr;
	swModel* mModel = nullptr;
	int mCurrentStep = 1;
	int mLastStep = 1;
	int mPreviewStep = 1;
	bool mPortrait = false;
	bool mColumnsAligned = false;
	bool mTransparentChosen = false;
	QString mStillSuffix = QLatin1String("png");

	QLineEdit* mFileEdit = nullptr;
	swRangeSpinBox* mWidthSpinBox = nullptr;
	swRangeSpinBox* mHeightSpinBox = nullptr;
	QComboBox* mRatioComboBox = nullptr;
	QPushButton* mLandscapeButton = nullptr;
	QPushButton* mPortraitButton = nullptr;
	QRadioButton* mCurrentRadio = nullptr;
	QRadioButton* mAllRadio = nullptr;
	QRadioButton* mRangeRadio = nullptr;
	swRangeSpinBox* mFromSpinBox = nullptr;
	swRangeSpinBox* mToSpinBox = nullptr;
	QRadioButton* mViewColorRadio = nullptr;
	QRadioButton* mTransparentRadio = nullptr;
	QCheckBox* mAnimatedCheckBox = nullptr;
	QComboBox* mAxisComboBox = nullptr;
	QRadioButton* mFullTurnRadio = nullptr;
	QRadioButton* mAngleRadio = nullptr;
	swRangeSpinBox* mAngleStartSpinBox = nullptr;
	swRangeSpinBox* mAngleEndSpinBox = nullptr;
	swRangeSpinBox* mFramesSpinBox = nullptr;
	swRangeDoubleSpinBox* mDurationSpinBox = nullptr;
	QComboBox* mFitComboBox = nullptr;
	QComboBox* mCenterComboBox = nullptr;
	swRangeSpinBox* mMarginSpinBox = nullptr;
	QComboBox* mScopeComboBox = nullptr;
	QList<QGridLayout*> mGridLayouts;

	swImagePreviewWidget* mPreview = nullptr;
	QToolButton* mPreviousButton = nullptr;
	QToolButton* mNextButton = nullptr;
	QLabel* mStepLabel = nullptr;
	QLabel* mSizeLabel = nullptr;
	QTimer* mPreviewTimer = nullptr;
	QToolButton* mPlayButton = nullptr;
	QTimer* mAnimationTimer = nullptr;
	QVector<QImage> mPreviewFrames;
	int mPreviewFrame = 0;
	bool mPlaying = true;
	bool mPreviewTransparent = false;
};
