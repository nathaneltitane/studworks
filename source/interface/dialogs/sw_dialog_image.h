#pragma once

struct swImageDialogOptions;
class swModel;
class swRangeSpinBox;
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
	void MatchView();
	void UpdateRange();
	void UpdateBackground();
	void ShowPreviewStep(int Offset);
	void UpdatePreview();
	void RenderPreview();
	int GetRangeStart() const;
	int GetRangeEnd() const;

	swImageDialogOptions* mOptions = nullptr;
	swModel* mModel = nullptr;
	int mCurrentStep = 1;
	int mLastStep = 1;
	int mPreviewStep = 1;
	double mRatio = 1.0;
	bool mColumnsAligned = false;

	QLineEdit* mFileEdit = nullptr;
	swRangeSpinBox* mWidthSpinBox = nullptr;
	swRangeSpinBox* mHeightSpinBox = nullptr;
	QPushButton* mRatioButton = nullptr;
	QRadioButton* mCurrentRadio = nullptr;
	QRadioButton* mAllRadio = nullptr;
	QRadioButton* mRangeRadio = nullptr;
	swRangeSpinBox* mFromSpinBox = nullptr;
	swRangeSpinBox* mToSpinBox = nullptr;
	QRadioButton* mViewColorRadio = nullptr;
	QRadioButton* mTransparentRadio = nullptr;
	QLabel* mTransparentLabel = nullptr;
	QList<QGridLayout*> mGridLayouts;

	swImagePreviewWidget* mPreview = nullptr;
	QToolButton* mPreviousButton = nullptr;
	QToolButton* mNextButton = nullptr;
	QLabel* mStepLabel = nullptr;
	QLabel* mSizeLabel = nullptr;
	QTimer* mPreviewTimer = nullptr;
};
