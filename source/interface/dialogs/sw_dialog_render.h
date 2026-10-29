#pragma once

enum class swPOVRayRenderQuality
{
	Low,
	Medium,
	High
};

enum class swPOVRayLightRig
{
	Standard,
	Studio,
	Matte,
	Count
};

enum class swPOVRayFloorStyle
{
	Plain,
	ShadowCatcher,
	Reflective,
	Cyclorama,
	Count
};

enum class swRenderDialogMode
{
	RenderPOVRay,
	RenderBlender,
	OpenInBlender
};

#ifndef SW_DISABLE_RENDER_DIALOG

namespace Ui {
class swRenderDialog;
}

class swRenderProcess : public QProcess
{
	Q_OBJECT

public:
	explicit swRenderProcess(QObject* Parent)
		: QProcess(Parent)
	{
	}
	~swRenderProcess();
};

class swRenderPreviewWidget : public QWidget
{
	Q_OBJECT

public:
	explicit swRenderPreviewWidget(QWidget* Parent)
		: QWidget(Parent)
	{
	}

	void SetImage(QImage Image)
	{
		mImage = Image;
		mScaledImage = QImage();
		update();
	}

	// Sets the size of the icon shown in the middle of the canvas until an image is rendered.
	void SetPlaceholderSize(int Size)
	{
		mPlaceholderSize = Size;
		update();
	}

	// Sets the image size the empty canvas is shaped after.
	void SetCanvasSize(const QSize& Size)
	{
		mCanvasSize = Size;
		update();
	}

	// Shows the render region and lets it be drawn by dragging on the canvas.
	void SetRegionMode(bool RegionMode)
	{
		mRegionMode = RegionMode;
		setCursor(RegionMode ? Qt::CrossCursor : Qt::ArrowCursor);
		update();
	}

	// Sets the render region as a fraction of the image.
	void SetRegion(const QRectF& Region)
	{
		mRegion = Region;
		update();
	}

	QRectF GetRegion() const
	{
		return mRegion;
	}

signals:
	void RegionChanged(const QRectF& Region);

protected:
	void resizeEvent(QResizeEvent* Event) override;
	void paintEvent(QPaintEvent* PaintEvent) override;
	void mousePressEvent(QMouseEvent* MouseEvent) override;
	void mouseMoveEvent(QMouseEvent* MouseEvent) override;
	void mouseReleaseEvent(QMouseEvent* MouseEvent) override;
	QRect GetCanvasRect() const;
	QPointF GetCanvasPoint(const QPoint& Position) const;

	QImage mImage;
	QImage mScaledImage;
	int mPlaceholderSize = 0;
	QSize mCanvasSize;
	bool mRegionMode = false;
	bool mRegionDragging = false;
	QPointF mRegionStart;
	QRectF mRegion = QRectF(0.25, 0.25, 0.5, 0.5);
};

struct swRenderJob
{
	swStep Step;
	QString CameraName;
	QString FileName;
};

class swRenderDialog : public QDialog
{
	Q_OBJECT

public:
	explicit swRenderDialog(QWidget* Parent, swRenderDialogMode Mode);
	virtual ~swRenderDialog();

public slots:
	void reject() override;

private slots:
	void OutputBrowseButtonClicked();
	void Update();

protected slots:
	void RenderButtonClicked();
	void SettingsButtonClicked();
	void LogButtonClicked();
	void ReadStdOut();
	void WriteStdOut();

protected:
	QString GetStdOutFileName() const;
	QString GetStdErrFileName() const;
	QString GetPOVFileName() const;
	void CreatePOVRayPanels();
	void UpdateLogPanel();
	QString ReadStdErr(bool& Error) const;
	void RenderPOVRay();
	void RenderBlender();
	void CloseProcess();
	bool PromptCancel();
	void ShowResult();
	void StartNextJob();
	QRect GetRegionRect() const;
	QStringList GetPresetValues() const;
	void ApplyPreset(const QString& Preset);
	void UpdatePresets();
	void SavePresetClicked();
	void DeletePresetClicked();
	void UpdateCameras();

	QTimer mUpdateTimer;
	QElapsedTimer mRenderTime;
	QFile mOutputFile;
	void* mOutputBuffer = nullptr;
	QImage mImage;
	QStringList mStdErrList;
	QStringList mStdOutList;
	QAbstractButton* mRenderButton = nullptr;
	QPushButton* mSettingsButton = nullptr;
	QAbstractButton* mLogButton = nullptr;
	QMainWindow* mPanelWindow = nullptr;
	QDockWidget* mLogPanel = nullptr;
	QPlainTextEdit* mLogEdit = nullptr;
	swRenderProcess* mProcess = nullptr;
	QComboBox* mPresetComboBox = nullptr;
	QAbstractButton* mPresetDeleteButton = nullptr;
	QSpinBox* mWidthSpinBox = nullptr;
	QSpinBox* mHeightSpinBox = nullptr;
	QComboBox* mQualityComboBox = nullptr;
	QComboBox* mLightRigComboBox = nullptr;
	QComboBox* mFloorStyleComboBox = nullptr;
	QComboBox* mBackgroundComboBox = nullptr;
	QLineEdit* mEnvironmentEdit = nullptr;
	QComboBox* mAreaComboBox = nullptr;
	QComboBox* mStepsComboBox = nullptr;
	QComboBox* mCamerasComboBox = nullptr;
	std::vector<swRenderJob> mJobs;
	size_t mJobIndex = 0;
	QRect mJobRegion;
	QImage mResultImage;
	QString mResultFileName;
	QString mPresetName;

	int mWidth = 1280;
	int mHeight = 720;
	swPOVRayRenderQuality mQuality = swPOVRayRenderQuality::High;
	int mPreviewWidth;
	int mPreviewHeight;
	swRenderDialogMode mDialogMode;
	int mBlendProgValue;
	int mBlendProgMax;
	double mScale;
	QString mDataPath;

	Ui::swRenderDialog* ui;
};

#else

class swRenderPreviewWidget : public QWidget
{
public:
	explicit swRenderPreviewWidget(QWidget* Parent)
	    : QWidget(Parent)
	{
	}
};

#endif // SW_DISABLE_RENDER_DIALOG
