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

protected:
	void resizeEvent(QResizeEvent* Event) override;
	void paintEvent(QPaintEvent* PaintEvent) override;

	QImage mImage;
	QImage mScaledImage;
	int mPlaceholderSize = 0;
	QSize mCanvasSize;
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
