#include "sw_global.h"
#include "sw_dialog_render.h"
#include "ui_sw_dialog_render.h"
#include "sw_project.h"
#include "sw_application.h"
#include "sw_profile.h"
#include "sw_dialog_preferences_blender.h"
#include "sw_model.h"
#include "sw_object_camera.h"
#include "sw_window_utilities.h"
#include "sw_renderer_povray.h"

#ifndef SW_DISABLE_RENDER_DIALOG

#define SW_POVRAY_PREVIEW_WIDTH 768
#define SW_POVRAY_PREVIEW_HEIGHT 432

// raise to drop old panel layouts
#define SW_RENDER_WINDOW_STATE_VERSION 3
#define SW_RENDER_PANEL_WIDTH 380
#define SW_RENDER_LOG_HEIGHT 240
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
#define SW_POVRAY_MEMORY_MAPPED_FILE 1
#endif

void swRenderPreviewWidget::resizeEvent(QResizeEvent* Event)
{
	mScaledImage = QImage();

	QWidget::resizeEvent(Event);
}

// use to find the canvas in the preview
QRect swRenderPreviewWidget::GetCanvasRect() const
{
	const QSize ImageSize = !mImage.isNull() ? mImage.size() : mCanvasSize;

	if (ImageSize.isEmpty())
		return QRect();

	const int Margin = style()->pixelMetric(QStyle::PM_LayoutLeftMargin);
	const QRect AreaRect = rect().adjusted(Margin, Margin, -Margin, -Margin);
	QRect CanvasRect(QPoint(0, 0), ImageSize.scaled(AreaRect.size(), Qt::KeepAspectRatio));
	CanvasRect.moveCenter(AreaRect.center());

	return CanvasRect;
}

// use to get a canvas fraction
QPointF swRenderPreviewWidget::GetCanvasPoint(const QPoint& Position) const
{
	const QRect CanvasRect = GetCanvasRect();

	if (CanvasRect.isEmpty())
		return QPointF();

	return QPointF(std::clamp(qreal(Position.x() - CanvasRect.left()) / CanvasRect.width(), 0.0, 1.0), std::clamp(qreal(Position.y() - CanvasRect.top()) / CanvasRect.height(), 0.0, 1.0));
}

void swRenderPreviewWidget::mousePressEvent(QMouseEvent* MouseEvent)
{
	if (!mRegionMode || MouseEvent->button() != Qt::LeftButton || GetCanvasRect().isEmpty())
	{
		QWidget::mousePressEvent(MouseEvent);
		return;
	}

	mRegionDragging = true;
	mRegionStart = GetCanvasPoint(MouseEvent->pos());
	mRegion = QRectF(mRegionStart, QSizeF());
	update();
}

void swRenderPreviewWidget::mouseMoveEvent(QMouseEvent* MouseEvent)
{
	if (!mRegionDragging)
	{
		QWidget::mouseMoveEvent(MouseEvent);
		return;
	}

	mRegion = QRectF(mRegionStart, GetCanvasPoint(MouseEvent->pos())).normalized();
	update();
}

void swRenderPreviewWidget::mouseReleaseEvent(QMouseEvent* MouseEvent)
{
	if (!mRegionDragging || MouseEvent->button() != Qt::LeftButton)
	{
		QWidget::mouseReleaseEvent(MouseEvent);
		return;
	}

	mRegionDragging = false;
	mRegion = QRectF(mRegionStart, GetCanvasPoint(MouseEvent->pos())).normalized();

	// a click keeps the whole image
	if (mRegion.width() < 0.01 || mRegion.height() < 0.01)
		mRegion = QRectF(0.0, 0.0, 1.0, 1.0);

	update();

	emit RegionChanged(mRegion);
}

void swRenderPreviewWidget::paintEvent(QPaintEvent* PaintEvent)
{
	Q_UNUSED(PaintEvent);

	QPainter Painter(this);

	// canvas on the backdrop
	Painter.fillRect(rect(), palette().color(QPalette::Mid));

	const QRect CanvasRect = GetCanvasRect();

	if (CanvasRect.isEmpty())
		return;

	// empty canvas is a blank sheet
	Painter.fillRect(CanvasRect, Qt::white);

	if (!mImage.isNull())
	{
		if (mScaledImage.size() != CanvasRect.size())
			mScaledImage = mImage.scaled(CanvasRect.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

		Painter.drawImage(CanvasRect.topLeft(), mScaledImage);
	}
	else if (mPlaceholderSize > 0)
	{
		// paint the placeholder at its size
		const int IconSize = qMin(mPlaceholderSize, qMin(CanvasRect.width(), CanvasRect.height()) / 2);
		const qreal PixelRatio = devicePixelRatioF();
		QImage IconImage = swCreatePaintedIconImage(swPaintedIcon::RenderPlaceholder, qRound(IconSize * PixelRatio), palette().color(QPalette::Mid));
		IconImage.setDevicePixelRatio(PixelRatio);
		QRect IconRect(0, 0, IconSize, IconSize);
		IconRect.moveCenter(CanvasRect.center());
		Painter.drawImage(IconRect, IconImage);
	}

	if (mRegionMode)
	{
		// dim outside the region
		const QRectF RegionRect(CanvasRect.left() + mRegion.left() * CanvasRect.width(), CanvasRect.top() + mRegion.top() * CanvasRect.height(), mRegion.width() * CanvasRect.width(), mRegion.height() * CanvasRect.height());
		QPainterPath OutsidePath;
		OutsidePath.addRect(CanvasRect);
		OutsidePath.addRect(RegionRect);
		Painter.fillPath(OutsidePath, QColor(0, 0, 0, 96));

		QPen Pen(palette().color(QPalette::Highlight), 2);
		Pen.setCosmetic(true);
		Painter.setPen(Pen);
		Painter.setBrush(Qt::NoBrush);
		Painter.drawRect(RegionRect);
	}
}

swRenderDialog::swRenderDialog(QWidget* Parent, swRenderDialogMode RenderDialogMode)
	: QDialog(Parent), mDialogMode(RenderDialogMode), ui(new Ui::swRenderDialog)
{
	ui->setupUi(this);
	swSetupDialogHelp(this, ui->buttonBox);

	mWidth = swGetProfileInteger(SW_PROFILE_RENDER_WIDTH);
	mHeight = swGetProfileInteger(SW_PROFILE_RENDER_HEIGHT);
	mQuality = swGetProfileRenderQuality();
	mScale = 1.0f;

	ui->OutputEdit->setText(swGetActiveProject()->GetImageFileName(false));

	if (mDialogMode == swRenderDialogMode::RenderPOVRay)
	{
		// buttons in the render panel
		mRenderButton = new QToolButton(this);
		mRenderButton->setText(tr("Render"));
		mLogButton = new QToolButton(this);
		mLogButton->setText(tr("Log"));
	}
	else
	{
		QPushButton* RenderButton = new QPushButton(tr("Render"), ui->buttonBox);
		ui->buttonBox->addButton(RenderButton, QDialogButtonBox::ActionRole);
		mRenderButton = RenderButton;
	}

	mSettingsButton = new QPushButton(tr("Settings..."), ui->buttonBox);
	ui->buttonBox->addButton(mSettingsButton, QDialogButtonBox::ResetRole);

	if (mDialogMode != swRenderDialogMode::RenderPOVRay)
	{
		QPushButton* LogButton = new QPushButton(tr("View Log..."), ui->buttonBox);
		ui->buttonBox->addButton(LogButton, QDialogButtonBox::ResetRole);
		mLogButton = LogButton;
	}

	mLogButton->setEnabled(false);
	mLogButton->setToolTip(tr("Show the renderer output and diagnostic messages."));
	mLogButton->setWhatsThis(tr("<p><b>View Log</b></p><p>Open the renderer output to investigate a failed render or warnings. This button becomes available when a log has been captured.</p>"));
	mRenderButton->setWhatsThis(tr("<p><b>Render</b></p><p>Start the render using the current settings. While rendering, this button becomes Cancel so you can stop the operation.</p>"));

	connect(mRenderButton, &QAbstractButton::clicked, this, &swRenderDialog::RenderButtonClicked);
	connect(mSettingsButton, &QPushButton::clicked, this, &swRenderDialog::SettingsButtonClicked);
	connect(mLogButton, &QAbstractButton::clicked, this, &swRenderDialog::LogButtonClicked);
	connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &swRenderDialog::reject);
	connect(ui->OutputBrowseButton, &QPushButton::clicked, this, &swRenderDialog::OutputBrowseButtonClicked);

	if (mDialogMode == swRenderDialogMode::RenderPOVRay)
	{
		setWindowTitle(tr("POV-Ray Image Render"));

		setWhatsThis(tr("<p><b>POV-Ray Image Render</b></p><p>Render the current model step using POV-Ray. Choose the output image, its size and quality and the scene in the panels beside the preview, then select Render to start.</p><p>The preview is scaled to fit the window. View Log shows diagnostic output when available.</p>"));
		mRenderButton->setToolTip(tr("Render the current model step with POV-Ray."));
		ui->buttonBox->hide();

		CreatePOVRayPanels();
	}
	else
	{
		setWindowTitle(mDialogMode == swRenderDialogMode::OpenInBlender ? tr("Blender LDraw Import") : tr("Blender Image Render"));
		setWhatsThis(tr("<p><b>Blender Image Render</b></p><p>Render the current model step with the Blender LDraw add-on. Configure Blender and its add-on through Settings, choose an output image, then select Render.</p><p>The preview is scaled to fit the dialog. View Log shows diagnostic output when available.</p>"));
		mSettingsButton->setWhatsThis(tr("<p><b>Blender Settings</b></p><p>Configure the Blender executable, LDraw import modules, library paths, and render settings. Blender and an import module must be configured before rendering or opening the model.</p>"));

		bool BlenderConfigured = !swGetProfileString(SW_PROFILE_BLENDER_IMPORT_MODULE).isEmpty();

		QStringList const& DataPathList = QStandardPaths::standardLocations(QStandardPaths::AppLocalDataLocation);
		if (!QDir(QString("%1/Blender/addons/%2").arg(DataPathList.first(), SW_BLENDER_ADDON_RENDER_FOLDER)).isReadable())
		{
			BlenderConfigured = false;
			swSetProfileString(SW_PROFILE_BLENDER_IMPORT_MODULE, QString());
		}

		mRenderButton->setToolTip(BlenderConfigured ? tr("Render the current model step with Blender.") : tr("Blender not configured. Use Settings to configure."));
		mRenderButton->setEnabled(BlenderConfigured);

		mSettingsButton->setToolTip(tr("Configure Blender, its add-on, and render settings."));

		if (mDialogMode == swRenderDialogMode::OpenInBlender)
		{
			mSettingsButton->setToolTip(tr("Configure Blender and its LDraw import settings."));

			mRenderButton->setText(tr("Open in Blender"));
			setWhatsThis(tr("<p><b>Open in Blender</b></p><p>Export the current model step and open it in Blender for editing. Use Settings to configure Blender and its LDraw import add-on, then select Open in Blender.</p><p>This mode does not save a rendered image.</p>"));
			mRenderButton->setWhatsThis(tr("<p><b>Open in Blender</b></p><p>Import the current model step into Blender for editing. Configure Blender and its LDraw import add-on through Settings if this action is unavailable.</p>"));

			if (BlenderConfigured)
				mRenderButton->setToolTip(tr("Import the current model step and open it in Blender."));

			ui->outputLabel->hide();
			ui->OutputEdit->hide();
			ui->RenderProgress->hide();
			ui->OutputBrowseButton->hide();
			mLogButton->hide();

			adjustSize();
			setMinimumWidth(ui->preview->geometry().width());
			ui->preview->hide();
		}
	}

	if (mDialogMode != swRenderDialogMode::OpenInBlender)
	{
		// render icon in the empty preview
		ui->preview->SetPlaceholderSize(SW_PART_ICON_SIZES.back());
		ui->preview->SetCanvasSize(QSize(mWidth, mHeight));
	}

	connect(&mUpdateTimer, &QTimer::timeout, this, &swRenderDialog::Update);

	mUpdateTimer.start(500);
}

// use to lay out the povray window
void swRenderDialog::CreatePOVRayPanels()
{
	QMainWindow* PanelWindow = new QMainWindow(this);
	PanelWindow->setWindowFlags(Qt::Widget);
	PanelWindow->setDockOptions(QMainWindow::AnimatedDocks | QMainWindow::AllowNestedDocks);

	mPanelWindow = PanelWindow;

	// preview fills the middle
	PanelWindow->setCentralWidget(ui->preview);
	ui->MainLayout->addWidget(PanelWindow, 1);
	ui->MainLayout->setContentsMargins(0, 0, 0, 0);
	layout()->setContentsMargins(0, 0, 0, 0);
	ui->outputLabel->hide();

	QList<QLabel*> Labels;

	auto CreatePanel = [PanelWindow](const QString& Title, const QString& Name, const QString& WhatsThis)
	{
		QDockWidget* Panel = new QDockWidget(Title, PanelWindow);
		Panel->setObjectName(Name);
		Panel->setWhatsThis(WhatsThis);
		Panel->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

		QWidget* Widget = new QWidget(Panel);
		Panel->setWidget(Widget);
		new QVBoxLayout(Widget);

		// keep rows under the title
		Panel->layout()->setAlignment(Widget, Qt::AlignTop);

		// top and bottom panels span the window
		Panel->setProperty("swFloatingSize", QSize(SW_RENDER_PANEL_WIDTH, 0));
		PanelWindow->addDockWidget(Qt::RightDockWidgetArea, Panel);
		swSetupWidgetHelp(Panel, nullptr);

		return Widget;
	};

	auto CreateRow = [&Labels](QWidget* Panel, const QString& Name, QWidget* Control, const QString& ToolTip, const QString& WhatsThis)
	{
		QWidget* Row = swCreatePanelRow(Panel);
		Panel->layout()->addWidget(Row);

		QLabel* Label = new QLabel(Name, Row);
		Label->setBuddy(Control);
		Label->setToolTip(ToolTip);
		Label->setWhatsThis(WhatsThis);
		Labels.append(Label);
		Row->layout()->addWidget(Label);

		Control->setParent(Row);
		Control->setToolTip(ToolTip);
		Control->setWhatsThis(WhatsThis);
		Control->setAccessibleName(Name);
		Control->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		swSetupPanelEdit(Control);
		static_cast<QHBoxLayout*>(Row->layout())->addWidget(Control, 1);

		return Row;
	};

	auto CreateComboBox = [](const QStringList& Items, int Index)
	{
		QComboBox* ComboBox = new QComboBox();
		ComboBox->addItems(Items);
		ComboBox->setCurrentIndex(std::clamp(Index, 0, static_cast<int>(Items.size()) - 1));

		return ComboBox;
	};

	auto CreateSpinBox = [](int Value)
	{
		QSpinBox* SpinBox = new QSpinBox();
		SpinBox->setRange(1, SW_MAXIMUM_IMAGE_DIMENSION);
		SpinBox->setValue(Value);
		SpinBox->setSuffix(tr(" px"));
		swSetupPanelSpinBox(SpinBox);

		return SpinBox;
	};

	QWidget* PresetsPanel = CreatePanel(tr("Presets"), "RenderPresets", tr("<p><b>Presets</b></p><p>Keep the image size, quality and scene settings under a name and bring them back with one choice. Save stores the current settings as a preset and Delete removes the chosen one.</p>"));

	mPresetComboBox = new QComboBox();
	CreateRow(PresetsPanel, tr("Preset"), mPresetComboBox, tr("Choose a preset to apply its image size, quality and scene settings."), QString());

	// presets share the panel width
	QWidget* PresetButtonRow = swCreatePanelRow(PresetsPanel);
	PresetsPanel->layout()->addWidget(PresetButtonRow);

	auto CreatePresetButton = [PresetButtonRow](const QString& Text, const QString& ToolTip)
	{
		QToolButton* Button = new QToolButton(PresetButtonRow);
		Button->setText(Text);
		Button->setToolTip(ToolTip);
		Button->setWhatsThis(ToolTip);
		Button->setToolButtonStyle(Qt::ToolButtonTextOnly);
		swSetupPanelButton(Button);
		Button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		Button->setMinimumWidth(0);
		static_cast<QHBoxLayout*>(PresetButtonRow->layout())->addWidget(Button, 1);

		return Button;
	};

	QToolButton* PresetSaveButton = CreatePresetButton(tr("Save..."), tr("Save the current image size, quality and scene settings as a preset."));
	mPresetDeleteButton = CreatePresetButton(tr("Delete..."), tr("Delete the chosen preset."));

	QWidget* OutputPanel = CreatePanel(tr("Output"), "RenderOutput", tr("<p><b>Output</b></p><p>Choose where the rendered image is saved and its format, such as PNG, JPEG, or BMP.</p>"));

	QWidget* FileRow = CreateRow(OutputPanel, tr("File"), ui->OutputEdit, ui->OutputEdit->toolTip(), ui->OutputEdit->whatsThis());
	ui->OutputBrowseButton->setParent(FileRow);
	swSetupPanelButton(ui->OutputBrowseButton);
	FileRow->layout()->addWidget(ui->OutputBrowseButton);

	QWidget* ImagePanel = CreatePanel(tr("Image"), "RenderImage", tr("<p><b>Image</b></p><p>Set the size of the rendered image in pixels and the rendering quality. Larger images and higher quality take longer to render.</p>"));

	mWidthSpinBox = CreateSpinBox(mWidth);
	QSpinBox* WidthSpinBox = mWidthSpinBox;
	CreateRow(ImagePanel, tr("Width"), WidthSpinBox, tr("Set the rendered image width in pixels (1 to 32768)."), QString());

	mHeightSpinBox = CreateSpinBox(mHeight);
	QSpinBox* HeightSpinBox = mHeightSpinBox;
	CreateRow(ImagePanel, tr("Height"), HeightSpinBox, tr("Set the rendered image height in pixels (1 to 32768)."), QString());

	mQualityComboBox = CreateComboBox({ tr("Low"), tr("Medium"), tr("High") }, static_cast<int>(mQuality));
	QComboBox* QualityComboBox = mQualityComboBox;
	CreateRow(ImagePanel, tr("Quality"), QualityComboBox, tr("Choose the rendering quality; higher quality can take longer."), tr("<p><b>Quality</b></p><p>Low uses POV-Ray's default quality without additional antialiasing options. Medium uses quality level 5 with antialiasing and fast radiosity. High uses quality level 11 with additional antialiasing settings and full radiosity.</p>"));

	mAreaComboBox = CreateComboBox({ tr("Full Image"), tr("Region") }, swGetProfileInteger(SW_PROFILE_RENDER_AREA));
	CreateRow(ImagePanel, tr("Area"), mAreaComboBox, tr("Choose whether the whole image is rendered or only a region drawn on the preview."), tr("<p><b>Area</b></p><p>Full Image renders the whole image. Region renders only the part outlined on the preview, drag on the preview to draw it. A region rendered at the size of the last image is placed into that image so a detail can be refined without rendering everything again.</p>"));

	QWidget* BatchPanel = CreatePanel(tr("Batch"), "RenderBatch", tr("<p><b>Batch</b></p><p>Render several images in one go, one for each building step or each camera of the model. Each image is saved next to the output file with the step number or camera name added to its name.</p>"));

	mStepsComboBox = CreateComboBox({ tr("Model"), tr("Current Step"), tr("All Steps") }, swGetProfileInteger(SW_PROFILE_RENDER_STEPS));
	CreateRow(BatchPanel, tr("Steps"), mStepsComboBox, tr("Choose whether the finished model, the current step or every step is rendered."), tr("<p><b>Steps</b></p><p>Model renders the finished model. Current Step renders the model as built up to the step shown in the main window. All Steps renders one image for every step, numbered after the step.</p>"));

	mCamerasComboBox = new QComboBox();
	CreateRow(BatchPanel, tr("Cameras"), mCamerasComboBox, tr("Choose the view or a camera of the model to render from, or every camera."), tr("<p><b>Cameras</b></p><p>View renders from the view of the main window. A camera renders from that camera of the model. All Cameras renders one image for every camera, named after the camera.</p>"));
	UpdateCameras();

	QWidget* ScenePanel = CreatePanel(tr("Scene"), "RenderScene", tr("<p><b>Scene</b></p><p>Set the lights, floor and surroundings the model is rendered in.</p>"));

	QComboBox* PartsComboBox = CreateComboBox({ tr("LDraw"), tr("LGEO") }, swGetProfileInteger(SW_PROFILE_RENDER_PARTS));
	CreateRow(ScenePanel, tr("Parts"), PartsComboBox, tr("Choose whether the model is rendered with LDraw parts or the modeled LGEO parts."), tr("<p><b>Parts</b></p><p>LDraw renders the parts of the parts library. LGEO replaces the parts it has with the smooth modeled parts of the LGEO library by Lutz Uhlmann, StudWorks offers to download it the first time. Models that ask for LGEO in their file always use it.</p>"));

	mLightRigComboBox = CreateComboBox({ tr("Standard"), tr("Studio"), tr("Matte") }, swGetProfileInteger(SW_PROFILE_RENDER_LIGHT_RIG));
	QComboBox* LightRigComboBox = mLightRigComboBox;
	CreateRow(ScenePanel, tr("Lights"), LightRigComboBox, tr("Choose the lights placed around models that have no lights of their own."), tr("<p><b>Lights</b></p><p>Standard uses four point lights with crisp shadows. Studio uses a large soft key light, a fill light and a rim light behind the model. Matte uses a wide overhead light and a shadowless light from the camera for even, soft lighting. Lights placed in the model replace these.</p>"));

	mFloorStyleComboBox = CreateComboBox({ tr("Plain"), tr("Shadow Catcher"), tr("Reflective"), tr("Cyclorama") }, swGetProfileInteger(SW_PROFILE_RENDER_FLOOR_STYLE));
	QComboBox* FloorStyleComboBox = mFloorStyleComboBox;
	CreateRow(ScenePanel, tr("Floor"), FloorStyleComboBox, tr("Choose how the floor under the model looks."), tr("<p><b>Floor</b></p><p>Plain uses the floor color of the model. Shadow Catcher takes the background color so only the shadows show. Reflective adds a glossy mirror finish. Cyclorama fades the floor into the background with distance for a seamless backdrop.</p>"));

	mEnvironmentEdit = new QLineEdit();
	QLineEdit* EnvironmentEdit = mEnvironmentEdit;
	EnvironmentEdit->setText(QDir::toNativeSeparators(swGetProfileString(SW_PROFILE_RENDER_ENVIRONMENT_IMAGE)));
	EnvironmentEdit->setPlaceholderText(tr("Background Color"));
	EnvironmentEdit->setClearButtonEnabled(true);
	QWidget* EnvironmentRow = CreateRow(ScenePanel, tr("Environment"), EnvironmentEdit, tr("Choose a panoramic image that surrounds and lights the scene, or leave empty to use the background color."), tr("<p><b>Environment</b></p><p>Choose an equirectangular panorama, ideally an HDR or EXR image, that wraps around the scene as its sky and lights it through radiosity at Medium and High quality. Leave empty to use the background color.</p>"));

	QToolButton* EnvironmentButton = new QToolButton(EnvironmentRow);
	EnvironmentButton->setText(tr("..."));
	EnvironmentButton->setToolTip(tr("Choose an environment image."));
	EnvironmentButton->setWhatsThis(EnvironmentButton->toolTip());
	EnvironmentButton->setAccessibleName(tr("Browse"));
	swSetupPanelButton(EnvironmentButton);
	EnvironmentRow->layout()->addWidget(EnvironmentButton);

	mBackgroundComboBox = CreateComboBox({ tr("Visible"), tr("Transparent") }, swGetProfileInteger(SW_PROFILE_RENDER_TRANSPARENT) ? 1 : 0);
	QComboBox* BackgroundComboBox = mBackgroundComboBox;
	CreateRow(ScenePanel, tr("Background"), BackgroundComboBox, tr("Choose whether the background and floor are rendered or left as transparent pixels for compositing."), QString());

	QWidget* RenderPanel = CreatePanel(tr("Render"), "RenderRender", tr("<p><b>Render</b></p><p>Start the render and follow its progress. While rendering, Render becomes Cancel so you can stop it. Log shows the renderer output below the preview.</p>"));

	QWidget* ProgressRow = swCreatePanelRow(RenderPanel);
	RenderPanel->layout()->addWidget(ProgressRow);
	ui->RenderProgress->setParent(ProgressRow);
	ui->RenderProgress->setFixedHeight(swGetPanelButtonSize().height());
	ProgressRow->layout()->addWidget(ui->RenderProgress);

	// render button spans the panel
	QWidget* ButtonRow = swCreatePanelRow(RenderPanel);
	RenderPanel->layout()->addWidget(ButtonRow);

	QToolButton* RenderButton = static_cast<QToolButton*>(mRenderButton);
	RenderButton->setParent(ButtonRow);
	RenderButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
	RenderButton->setAccessibleName(tr("Render"));
	swSetupPanelButton(RenderButton);
	RenderButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	static_cast<QHBoxLayout*>(ButtonRow->layout())->addWidget(RenderButton, 1);

	QToolButton* LogButton = static_cast<QToolButton*>(mLogButton);
	LogButton->setParent(ButtonRow);
	LogButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
	LogButton->setAccessibleName(tr("Log"));
	swSetupPanelButton(LogButton);
	ButtonRow->layout()->addWidget(LogButton);

	// scene panel takes the rest
	static_cast<QVBoxLayout*>(ScenePanel->layout())->addStretch(1);

	// labels share one width
	int LabelWidth = 0;

	for (QLabel* Label : Labels)
		LabelWidth = std::max(LabelWidth, Label->sizeHint().width());

	for (QLabel* Label : Labels)
		Label->setFixedWidth(LabelWidth);

	// keep changes for the next render
	connect(WidthSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int Value)
	{
		mWidth = Value;
		swSetProfileInteger(SW_PROFILE_RENDER_WIDTH, Value);
		ui->preview->SetCanvasSize(QSize(mWidth, mHeight));
	});

	connect(HeightSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int Value)
	{
		mHeight = Value;
		swSetProfileInteger(SW_PROFILE_RENDER_HEIGHT, Value);
		ui->preview->SetCanvasSize(QSize(mWidth, mHeight));
	});

	connect(QualityComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int Index)
	{
		mQuality = static_cast<swPOVRayRenderQuality>(Index);
		swSetProfileRenderQuality(mQuality);
	});

	connect(PartsComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [](int Index)
	{
		swSetProfileInteger(SW_PROFILE_RENDER_PARTS, Index);
	});

	connect(LightRigComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [](int Index)
	{
		swSetProfileInteger(SW_PROFILE_RENDER_LIGHT_RIG, Index);
	});

	connect(FloorStyleComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [](int Index)
	{
		swSetProfileInteger(SW_PROFILE_RENDER_FLOOR_STYLE, Index);
	});

	connect(BackgroundComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [](int Index)
	{
		swSetProfileInteger(SW_PROFILE_RENDER_TRANSPARENT, Index);
	});

	connect(EnvironmentEdit, &QLineEdit::textChanged, this, [](const QString& Text)
	{
		swSetProfileString(SW_PROFILE_RENDER_ENVIRONMENT_IMAGE, QDir::fromNativeSeparators(Text.trimmed()));
	});

	// show custom on a mismatch
	for (QSpinBox* SpinBox : { WidthSpinBox, HeightSpinBox })
		connect(SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &swRenderDialog::UpdatePresets);

	for (QComboBox* ComboBox : { QualityComboBox, LightRigComboBox, FloorStyleComboBox, BackgroundComboBox })
		connect(ComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &swRenderDialog::UpdatePresets);

	connect(EnvironmentEdit, &QLineEdit::textChanged, this, &swRenderDialog::UpdatePresets);

	connect(mPresetComboBox, QOverload<int>::of(&QComboBox::activated), this, [this](int Index)
	{
		ApplyPreset(mPresetComboBox->itemData(Index).toString());
	});

	connect(PresetSaveButton, &QToolButton::clicked, this, &swRenderDialog::SavePresetClicked);
	connect(mPresetDeleteButton, &QToolButton::clicked, this, &swRenderDialog::DeletePresetClicked);

	UpdatePresets();

	// keep the drawn region
	const QStringList RegionValues = swGetProfileString(SW_PROFILE_RENDER_REGION).split(' ', Qt::SkipEmptyParts);

	if (RegionValues.size() == 4)
		ui->preview->SetRegion(QRectF(RegionValues[0].toDouble(), RegionValues[1].toDouble(), RegionValues[2].toDouble(), RegionValues[3].toDouble()) & QRectF(0.0, 0.0, 1.0, 1.0));

	ui->preview->SetRegionMode(mAreaComboBox->currentIndex() == 1);

	connect(mAreaComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int Index)
	{
		swSetProfileInteger(SW_PROFILE_RENDER_AREA, Index);
		ui->preview->SetRegionMode(Index == 1);
	});

	connect(ui->preview, &swRenderPreviewWidget::RegionChanged, this, [](const QRectF& Region)
	{
		swSetProfileString(SW_PROFILE_RENDER_REGION, QString("%1 %2 %3 %4").arg(Region.x()).arg(Region.y()).arg(Region.width()).arg(Region.height()));
	});

	connect(mStepsComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [](int Index)
	{
		swSetProfileInteger(SW_PROFILE_RENDER_STEPS, Index);
	});

	connect(mCamerasComboBox, QOverload<int>::of(&QComboBox::activated), this, [this](int Index)
	{
		swSetProfileString(SW_PROFILE_RENDER_CAMERAS, mCamerasComboBox->itemData(Index).toString());
	});

	connect(EnvironmentButton, &QToolButton::clicked, this, [this, EnvironmentEdit]()
	{
		const QString FileName = QFileDialog::getOpenFileName(this, tr("Select Environment Image"), EnvironmentEdit->text(), tr("Environment Images (*.hdr *.exr *.png *.jpg *.jpeg *.tif *.tiff);;All Files (*.*)"));

		if (!FileName.isEmpty())
			EnvironmentEdit->setText(QDir::toNativeSeparators(FileName));
	});

	// log below the preview
	mLogPanel = new QDockWidget(tr("Log"), PanelWindow);
	mLogPanel->setObjectName("RenderLog");
	mLogPanel->setProperty("swFloatingSize", QSize(2 * SW_RENDER_PANEL_WIDTH, SW_RENDER_LOG_HEIGHT));
	mLogPanel->setWhatsThis(tr("<p><b>Log</b></p><p>The output of the renderer, updated while it renders. Read it to investigate warnings or a failed render, select text and copy it with the standard keyboard shortcut when reporting a problem.</p>"));

	mLogEdit = new QPlainTextEdit(mLogPanel);
	mLogEdit->setReadOnly(true);
	mLogEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
	mLogEdit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
	mLogEdit->setPlaceholderText(tr("The renderer output shows here once a render starts."));
	mLogEdit->setAccessibleName(tr("Renderer Output"));
	mLogEdit->setToolTip(tr("Read and copy renderer output and diagnostic messages."));
	mLogEdit->setMinimumHeight(6 * QFontMetrics(mLogEdit->font()).height());
	mLogPanel->setWidget(mLogEdit);

	swSetupWidgetHelp(mLogPanel, nullptr);

	// log and render panels span the edges
	QDockWidget* RenderDock = qobject_cast<QDockWidget*>(RenderPanel->parentWidget());
	PanelWindow->addDockWidget(Qt::BottomDockWidgetArea, RenderDock);
	PanelWindow->addDockWidget(Qt::BottomDockWidgetArea, mLogPanel);
	PanelWindow->splitDockWidget(mLogPanel, RenderDock, Qt::Vertical);

	for (QDockWidget* EdgePanel : { RenderDock, mLogPanel })
		EdgePanel->setAllowedAreas(Qt::TopDockWidgetArea | Qt::BottomDockWidgetArea);

	mLogPanel->hide();

	// log button toggles the log panel
	mLogButton->setCheckable(true);
	mLogButton->setEnabled(true);
	mLogButton->setToolTip(tr("Show or hide the Log panel with the renderer output."));
	mLogButton->setWhatsThis(tr("<p><b>Log</b></p><p>Show or hide the Log panel below the preview with the renderer output and diagnostic messages.</p>"));

	connect(mLogButton, &QAbstractButton::toggled, mLogPanel, &QDockWidget::setVisible);
	connect(mLogPanel, &QDockWidget::visibilityChanged, this, [this](bool Visible)
	{
		if (mLogPanel->isHidden())
			mLogButton->setChecked(false);
		else if (Visible)
		{
			mLogButton->setChecked(true);
			UpdateLogPanel();
		}
	});

	for (QWidget* Panel : { PresetsPanel, OutputPanel, ImagePanel, BatchPanel, RenderPanel })
		Panel->setFixedHeight(Panel->sizeHint().height());

	// open moved panels at a usable size
	QList<QDockWidget*> CompactPanels;

	for (QWidget* Panel : { PresetsPanel, OutputPanel, ImagePanel, BatchPanel, RenderPanel })
		CompactPanels.append(qobject_cast<QDockWidget*>(Panel->parentWidget()));

	auto FitPanels = [PanelWindow, CompactPanels]()
	{
		// compact panels end at their last row
		for (QDockWidget* CompactPanel : CompactPanels)
			CompactPanel->setMaximumHeight(CompactPanel->isFloating() ? QWIDGETSIZE_MAX : CompactPanel->sizeHint().height());

		// restore squeezed panel sizes
		QList<QDockWidget*> SidePanels, EdgePanels;

		for (QDockWidget* AreaPanel : PanelWindow->findChildren<QDockWidget*>(QString(), Qt::FindDirectChildrenOnly))
		{
			if (AreaPanel->isFloating() || AreaPanel->isHidden())
				continue;

			const Qt::DockWidgetArea Area = PanelWindow->dockWidgetArea(AreaPanel);

			if ((Area == Qt::LeftDockWidgetArea || Area == Qt::RightDockWidgetArea) && AreaPanel->width() < SW_RENDER_PANEL_WIDTH)
				SidePanels.append(AreaPanel);
			else if ((Area == Qt::TopDockWidgetArea || Area == Qt::BottomDockWidgetArea) && AreaPanel->height() < SW_RENDER_LOG_HEIGHT && !CompactPanels.contains(AreaPanel))
				EdgePanels.append(AreaPanel);
		}

		if (!SidePanels.isEmpty())
			PanelWindow->resizeDocks(SidePanels, QVector<int>(SidePanels.size(), SW_RENDER_PANEL_WIDTH).toList(), Qt::Horizontal);

		if (!EdgePanels.isEmpty())
			PanelWindow->resizeDocks(EdgePanels, QVector<int>(EdgePanels.size(), SW_RENDER_LOG_HEIGHT).toList(), Qt::Vertical);
	};

	for (QDockWidget* Panel : PanelWindow->findChildren<QDockWidget*>(QString(), Qt::FindDirectChildrenOnly))
	{
		// size panels after the dock fitting
		auto FitLater = [Panel, FitPanels]()
		{
			QTimer::singleShot(0, Panel, [Panel, FitPanels]()
			{
				QTimer::singleShot(0, Panel, FitPanels);
			});
		};

		connect(Panel, &QDockWidget::dockLocationChanged, Panel, FitLater);
		connect(Panel, &QDockWidget::visibilityChanged, Panel, FitLater);
		connect(Panel, &QDockWidget::topLevelChanged, Panel, FitLater);
	}

	// fit a readable output path
	PanelWindow->resizeDocks({ qobject_cast<QDockWidget*>(OutputPanel->parentWidget()) }, { SW_RENDER_PANEL_WIDTH }, Qt::Horizontal);

	// restore the panel layout
	QSettings Settings;
	Settings.beginGroup("RenderWindow");
	PanelWindow->restoreState(Settings.value("State").toByteArray(), SW_RENDER_WINDOW_STATE_VERSION);
	Settings.endGroup();

	// keep untoggled panels visible
	for (QWidget* Panel : { PresetsPanel, OutputPanel, ImagePanel, BatchPanel, ScenePanel, RenderPanel })
		Panel->parentWidget()->show();

	mLogButton->setChecked(!mLogPanel->isHidden());

	// restore squeezed saved layouts
	QTimer::singleShot(0, PanelWindow, FitPanels);
}

// use to list preset settings
QStringList swRenderDialog::GetPresetValues() const
{
	return { QString::number(mWidthSpinBox->value()), QString::number(mHeightSpinBox->value()), QString::number(mQualityComboBox->currentIndex()), QString::number(mLightRigComboBox->currentIndex()), QString::number(mFloorStyleComboBox->currentIndex()), QString::number(mBackgroundComboBox->currentIndex()), QDir::fromNativeSeparators(mEnvironmentEdit->text().trimmed()) };
}

// use to apply a stored preset
void swRenderDialog::ApplyPreset(const QString& Preset)
{
	const QStringList Values = Preset.split('|');

	if (Values.size() < 8)
		return;

	mPresetName = Values[0];

	mWidthSpinBox->setValue(Values[1].toInt());
	mHeightSpinBox->setValue(Values[2].toInt());
	mQualityComboBox->setCurrentIndex(std::clamp(Values[3].toInt(), 0, mQualityComboBox->count() - 1));
	mLightRigComboBox->setCurrentIndex(std::clamp(Values[4].toInt(), 0, mLightRigComboBox->count() - 1));
	mFloorStyleComboBox->setCurrentIndex(std::clamp(Values[5].toInt(), 0, mFloorStyleComboBox->count() - 1));
	mBackgroundComboBox->setCurrentIndex(std::clamp(Values[6].toInt(), 0, mBackgroundComboBox->count() - 1));
	mEnvironmentEdit->setText(QDir::toNativeSeparators(Values.mid(7).join('|')));

	UpdatePresets();
}

// use to fill the preset list
void swRenderDialog::UpdatePresets()
{
	if (!mPresetComboBox)
		return;

	const QStringList Presets = swGetProfileStringList(SW_PROFILE_RENDER_PRESETS);
	const QStringList Values = GetPresetValues();
	int CurrentIndex = -1;

	QSignalBlocker Blocker(mPresetComboBox);
	mPresetComboBox->clear();

	for (const QString& Preset : Presets)
	{
		const QStringList PresetValues = Preset.split('|');

		if (PresetValues.size() < 8)
			continue;

		// show the last chosen duplicate
		if ((CurrentIndex == -1 || PresetValues[0] == mPresetName) && PresetValues.mid(1, 6) == Values.mid(0, 6) && PresetValues.mid(7).join('|') == Values[6])
			CurrentIndex = mPresetComboBox->count();

		mPresetComboBox->addItem(PresetValues[0], Preset);
	}

	// unmatched settings show as custom
	if (CurrentIndex == -1)
	{
		mPresetComboBox->insertItem(0, tr("Custom"));
		CurrentIndex = 0;
	}

	mPresetComboBox->setCurrentIndex(CurrentIndex);
	mPresetDeleteButton->setEnabled(!mPresetComboBox->itemData(CurrentIndex).toString().isEmpty());
}

// use to save a named preset
void swRenderDialog::SavePresetClicked()
{
	const QString CurrentName = mPresetComboBox->currentData().toString().isEmpty() ? QString() : mPresetComboBox->currentText();
	bool Ok = false;
	const QString Name = QInputDialog::getText(this, tr("Save Preset"), tr("Preset Name:"), QLineEdit::Normal, CurrentName, &Ok).trimmed().remove('|');

	if (!Ok)
		return;

	if (Name.isEmpty())
	{
		QMessageBox::information(this, tr("Save Preset"), tr("Preset name cannot be empty."));
		return;
	}

	QStringList Presets = swGetProfileStringList(SW_PROFILE_RENDER_PRESETS);
	const QString Preset = QStringList({ Name }).join('|') + '|' + GetPresetValues().join('|');
	int PresetIndex = -1;

	for (int Index = 0; Index < Presets.size(); Index++)
		if (Presets[Index].section('|', 0, 0) == Name)
			PresetIndex = Index;

	if (PresetIndex != -1)
	{
		if (QMessageBox::question(this, tr("Save Preset"), tr("A preset with the name '%1' already exists, do you want to overwrite it?").arg(Name), QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
			return;

		Presets[PresetIndex] = Preset;
	}
	else
		Presets.append(Preset);

	swSetProfileStringList(SW_PROFILE_RENDER_PRESETS, Presets);
	mPresetName = Name;
	UpdatePresets();
}

// use to remove the chosen preset
void swRenderDialog::DeletePresetClicked()
{
	const QString Preset = mPresetComboBox->currentData().toString();

	if (Preset.isEmpty())
		return;

	if (QMessageBox::question(this, tr("Delete Preset"), tr("Are you sure you want to delete the preset '%1'?").arg(mPresetComboBox->currentText()), QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
		return;

	QStringList Presets = swGetProfileStringList(SW_PROFILE_RENDER_PRESETS);
	Presets.removeAll(Preset);
	swSetProfileStringList(SW_PROFILE_RENDER_PRESETS, Presets);
	UpdatePresets();
}

// use to list views and cameras
void swRenderDialog::UpdateCameras()
{
	const QString CameraName = swGetProfileString(SW_PROFILE_RENDER_CAMERAS);
	swModel* Model = swGetActiveModel();

	mCamerasComboBox->clear();
	mCamerasComboBox->addItem(tr("View"), QString());

	if (Model && !Model->GetCameras().empty())
	{
		for (const std::unique_ptr<swCamera>& Camera : Model->GetCameras())
			mCamerasComboBox->addItem(Camera->GetName(), Camera->GetName());

		mCamerasComboBox->addItem(tr("All Cameras"), QString("*"));
	}

	mCamerasComboBox->setCurrentIndex(std::max(mCamerasComboBox->findData(CameraName), 0));
}

// use to get the region in pixels
QRect swRenderDialog::GetRegionRect() const
{
	if (!mAreaComboBox || mAreaComboBox->currentIndex() != 1)
		return QRect();

	const QRectF Region = ui->preview->GetRegion();
	const int Left = std::clamp(qRound(Region.left() * mWidth), 0, mWidth - 2);
	const int Top = std::clamp(qRound(Region.top() * mHeight), 0, mHeight - 2);
	const int Right = std::clamp(qRound(Region.right() * mWidth), Left + 2, mWidth);
	const int Bottom = std::clamp(qRound(Region.bottom() * mHeight), Top + 2, mHeight);
	const QRect RegionRect(QPoint(Left, Top), QPoint(Right - 1, Bottom - 1));

	// a full region is a full render
	return RegionRect == QRect(0, 0, mWidth, mHeight) ? QRect() : RegionRect;
}

// use to run the next batch image
void swRenderDialog::StartNextJob()
{
	if (mJobIndex >= mJobs.size())
	{
		mJobs.clear();
		mJobIndex = 0;
		ui->RenderProgress->setFormat("%p%");
		return;
	}

	mRenderTime.start();
	RenderPOVRay();
}

// use to fill the log panel
void swRenderDialog::UpdateLogPanel()
{
	if (!mLogPanel || mLogPanel->isHidden())
		return;

	QFile File(GetStdErrFileName());
	const QString Text = File.open(QFile::ReadOnly | QFile::Text) ? QString::fromLocal8Bit(File.readAll()) : QString();

	if (Text == mLogEdit->toPlainText())
		return;

	mLogEdit->setPlainText(Text);
	mLogEdit->moveCursor(QTextCursor::End);
}

swRenderDialog::~swRenderDialog()
{
	// save the panel layout
	if (mPanelWindow)
	{
		QSettings Settings;
		Settings.beginGroup("RenderWindow");
		Settings.setValue("State", mPanelWindow->saveState(SW_RENDER_WINDOW_STATE_VERSION));
		Settings.endGroup();
	}

	delete ui;
}

QString swRenderDialog::GetStdOutFileName() const
{
	QString LogFile = mDialogMode == swRenderDialogMode::RenderPOVRay ? QLatin1String("studworks-povray-render.out") : QLatin1String("studworks-blender-render.out");

	return QDir(QDir::tempPath()).absoluteFilePath(LogFile);
}

QString swRenderDialog::GetStdErrFileName() const
{
	QString LogFile = mDialogMode == swRenderDialogMode::RenderPOVRay ? QLatin1String("studworks-povray-render.err") : QLatin1String("studworks-blender-render.err");

	return QDir(QDir::tempPath()).absoluteFilePath(LogFile);
}

QString swRenderDialog::GetPOVFileName() const
{
	return QDir(QDir::tempPath()).absoluteFilePath("studworks-render.pov");
}

void swRenderDialog::CloseProcess()
{
	delete mProcess;
	mProcess = nullptr;

	if (mDialogMode == swRenderDialogMode::RenderPOVRay)
	{
#if SW_POVRAY_MEMORY_MAPPED_FILE
		mOutputFile.unmap((uchar*)mOutputBuffer);
		mOutputBuffer = nullptr;
		mOutputFile.close();

		QFile::remove(GetStdOutFileName());
#endif

		QFile::remove(GetPOVFileName());
	}

	if (mDialogMode != swRenderDialogMode::OpenInBlender)
		mRenderButton->setText(tr("Render"));
}

bool swRenderDialog::PromptCancel()
{
	if (!mProcess)
		return true;

	if (QMessageBox::question(this, tr("Cancel Render"), tr("Are you sure you want to cancel the current render?"), QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
		return false;

	if (mProcess)
	{
#ifdef Q_OS_WIN
		swTerminateChildProcess(this, mProcess->processId(), QCoreApplication::applicationPid());
#endif
		mProcess->kill();
	}

	// cancel waiting batch images
	mJobs.clear();
	mJobIndex = 0;
	ui->RenderProgress->setFormat("%p%");

	CloseProcess();

	if (mStdOutList.size())
	{
		WriteStdOut();
		mLogButton->setEnabled(true);
	}

	return true;
}

void swRenderDialog::RenderButtonClicked()
{
	if (mProcess)
	{
		PromptCancel();
		return;
	}

	mLogButton->setEnabled(mLogPanel != nullptr);

	if (mLogEdit)
		mLogEdit->clear();

	mPreviewWidth  = ui->preview->width();
	mPreviewHeight = ui->preview->height();

	mRenderTime.start();

	if (mDialogMode != swRenderDialogMode::RenderPOVRay)
	{
		RenderBlender();
		return;
	}

	// set up pov-ray and lgeo first
	mPOVRayProgram = swFindPOVRay();

	if (!mPOVRayProgram.Supported)
	{
		mPOVRayProgram = swSetupPOVRay(this, mPOVRayProgram);

		if (!mPOVRayProgram.Supported)
			return;
	}

	const bool UseLGEO = swGetActiveProject()->GetModels()[0]->GetPOVRayOptions().UseLGEO || swGetProfileInteger(SW_PROFILE_RENDER_PARTS) == 1;

	if (UseLGEO && swFindLGEOPath().isEmpty())
		swSetupLGEO(this);

	if (UseLGEO && !swFindLGEOPath().isEmpty() && !swPOVRayHasIncludeFiles(mPOVRayProgram))
	{
		QMessageBox::warning(this, windowTitle(), tr("LGEO parts need the standard POV-Ray include files such as colors.inc, which POV-Ray %1 could not find.\n\nInstall the povray-includes package or the include files of your POV-Ray version, or choose LDraw parts in the Scene panel.").arg(mPOVRayProgram.Version));
		return;
	}

	// one image per step and camera
	const swModel* Model = swGetActiveProject()->GetModels()[0].get();
	std::vector<swStep> Steps;

	switch (mStepsComboBox->currentIndex())
	{
	case 1:
		Steps.push_back(Model->GetCurrentStep());
		break;

	case 2:
		for (swStep Step = 1; Step <= Model->GetLastStep(); Step++)
			Steps.push_back(Step);
		break;

	default:
		Steps.push_back(SW_STEP_MAXIMUM);
		break;
	}

	QStringList CameraNames;
	const QString CameraData = mCamerasComboBox->currentData().toString();

	if (CameraData == QLatin1String("*"))
	{
		for (int Index = 1; Index < mCamerasComboBox->count() - 1; Index++)
			CameraNames.append(mCamerasComboBox->itemData(Index).toString());
	}
	else
		CameraNames.append(CameraData);

	const QString OutputName = ui->OutputEdit->text();
	const QFileInfo OutputInfo(OutputName);
	const int StepDigits = std::max(2, static_cast<int>(QString::number(Model->GetLastStep()).size()));

	mJobs.clear();
	mJobIndex = 0;
	mJobRegion = GetRegionRect();

	for (swStep Step : Steps)
	{
		for (const QString& CameraName : CameraNames)
		{
			QString Suffix;

			if (Steps.size() > 1)
				Suffix += QString("-%1").arg(Step, StepDigits, 10, QLatin1Char('0'));

			if (CameraNames.size() > 1)
				Suffix += '-' + QString(CameraName).replace(QRegularExpression("[^\\w.-]+"), "_");

			const QString FileName = OutputName.isEmpty() || Suffix.isEmpty() ? OutputName : QDir(OutputInfo.path()).filePath(OutputInfo.completeBaseName() + Suffix + (OutputInfo.suffix().isEmpty() ? QString() : '.' + OutputInfo.suffix()));
			mJobs.push_back({ Step, CameraName, QDir::toNativeSeparators(FileName) });
		}
	}

	StartNextJob();
}

void swRenderDialog::reject()
{
	if (PromptCancel())
		QDialog::reject();
}

void swRenderDialog::RenderPOVRay()
{
	QString FileName = GetPOVFileName();

	ui->preview->SetImage(QImage());
	ui->RenderProgress->setValue(ui->RenderProgress->minimum());
	const swRenderJob& Job = mJobs[mJobIndex];
	const QString JobFormat = mJobs.size() > 1 ? tr("Image %1 of %2").arg(mJobIndex + 1).arg(mJobs.size()) : QString();
	ui->RenderProgress->setFormat(JobFormat.isEmpty() ? tr("Exporting Model") : tr("%1, Exporting Model").arg(JobFormat));

	const swCamera* Camera = nullptr;
	swModel* ActiveModel = swGetActiveModel();

	if (!Job.CameraName.isEmpty() && ActiveModel)
		for (const std::unique_ptr<swCamera>& ModelCamera : ActiveModel->GetCameras())
			if (ModelCamera->GetName() == Job.CameraName)
				Camera = ModelCamera.get();

	QApplication::setOverrideCursor(Qt::WaitCursor);

	swResult<void> ExportResult = swGetActiveProject()->ExportPOVRay(FileName, Camera, Job.Step);

	QGuiApplication::restoreOverrideCursor();

	ui->RenderProgress->setFormat(JobFormat.isEmpty() ? QString("%p%") : tr("%1, %p%").arg(JobFormat));

	if (!ExportResult)
	{
		mJobs.clear();
		mJobIndex = 0;

		if (!ExportResult.error().isEmpty())
			QMessageBox::warning(this, windowTitle(), ExportResult.error());

		return;
	}

	QStringList Arguments;

	Arguments.append(QString::fromLatin1("+I\"%1\"").arg(FileName));
	Arguments.append(QString::fromLatin1("+W%1").arg(QString::number(mWidth)));
	Arguments.append(QString::fromLatin1("+H%1").arg(QString::number(mHeight)));
	Arguments.append("-O-");

	if (swGetProfileInteger(SW_PROFILE_RENDER_TRANSPARENT))
		Arguments.append("+UA");

	// region in one based pixels
	if (mJobRegion.isValid())
	{
		auto RegionStart = [](int Pixel)
		{
			return Pixel > 0 ? QString::number(Pixel + 1) : QString("0.0");
		};

		Arguments.append("+SC" + RegionStart(mJobRegion.left()));
		Arguments.append("+EC" + QString::number(mJobRegion.right() + 1));
		Arguments.append("+SR" + RegionStart(mJobRegion.top()));
		Arguments.append("+ER" + QString::number(mJobRegion.bottom() + 1));
	}

#if SW_POVRAY_MEMORY_MAPPED_FILE
	Arguments.append(QString::fromLatin1("+SM\"%1\"").arg(GetStdOutFileName()));
#endif

	const bool UseLGEO = swGetActiveProject()->GetModels()[0]->GetPOVRayOptions().UseLGEO || swGetProfileInteger(SW_PROFILE_RENDER_PARTS) == 1;
	Arguments.append(swGetPOVRayArguments(mPOVRayProgram, mQuality, UseLGEO));

	swRenderProcess* Process = new swRenderProcess(this);
#ifdef Q_OS_LINUX
	connect(Process, &QProcess::readyReadStandardError, this, [this]() { bool Error; ReadStdErr(Error); });
#endif
	Process->setEnvironment(swGetPOVRayEnvironment());
	Process->setStandardErrorFile(GetStdErrFileName());
	Process->start(mPOVRayProgram.Path, Arguments);

	mImage = QImage(mWidth, mHeight, QImage::Format_ARGB32);
	mImage.fill(Qt::transparent);
	ui->preview->SetImage(mImage);

	if (Process->waitForStarted())
	{
		mProcess = Process;
		mRenderButton->setText(tr("Cancel"));
		ui->RenderProgress->setValue(ui->RenderProgress->minimum());
		mStdErrList.clear();
	}
	else
	{
		delete Process;
		QMessageBox::warning(this, windowTitle(), tr("Error starting POV-Ray."));
		CloseProcess();
	}
}

void swRenderDialog::RenderBlender()
{
	const QString BlenderLDrawConfigFile = swGetProfileString(SW_PROFILE_BLENDER_LDRAW_CONFIGURATION_PATH);
	const QString BlenderImportModule = swGetProfileString(SW_PROFILE_BLENDER_IMPORT_MODULE);

	if (!QFileInfo(BlenderLDrawConfigFile).isReadable() && !BlenderImportModule.isEmpty())
		swBlenderPreferences::SaveSettings();

	ui->RenderProgress->setFormat(tr("Saving Blender Model"));
	QApplication::processEvents();

	mBlendProgValue = 0;
	mBlendProgMax = 0;

	const QStringList DataPathList = QStandardPaths::standardLocations(QStandardPaths::AppLocalDataLocation);
	mDataPath = DataPathList.first();
	const QString DefaultBlendFile = QString("%1/blender/config/%2").arg(mDataPath, SW_BLENDER_ADDON_BLEND_FILE);

	swModel* Model = swGetActiveProject()->GetActiveModel();
	const QString ModelFileName = QFileInfo(QDir(swGetProfileString(SW_PROFILE_PROJECTS_PATH)), QString("%1_Step_%2.ldr").arg(QFileInfo(Model->GetProperties().mFileName).baseName()).arg(Model->GetCurrentStep())).absoluteFilePath();

	swResult<void> ExportResult = swGetActiveProject()->ExportCurrentStep(ModelFileName);

	if (!ExportResult)
	{
		QMessageBox::warning(this, windowTitle(), ExportResult.error());
		return;
	}

	ui->RenderProgress->setFormat("%p%");
	QApplication::processEvents();

	if (!QFileInfo(ModelFileName).isReadable())
		return;

	bool SearchCustomDir = true;

	QString Message;
	QStringList Arguments;
	QString PythonExpression = QString("\"import bpy; bpy.ops.render_scene.lpub3d_render_ldraw("
										"'EXEC_DEFAULT', "
										"resolution_width=%1, resolution_height=%2, "
										"render_percentage=%3, model_file=r'%4', "
										"image_file=r'%5'")
										.arg(mWidth).arg(mHeight)
										.arg(mScale * 100)
										.arg(QDir::toNativeSeparators(ModelFileName).replace("\\","\\\\"),
	                                         QDir::toNativeSeparators(ui->OutputEdit->text()).replace("\\","\\\\"));
	if (BlenderImportModule == QLatin1String("MM"))
		PythonExpression.append(", use_ldraw_import_mm=True");
	if (SearchCustomDir)
		PythonExpression.append(", search_additional_paths=True");

	if (mDialogMode == swRenderDialogMode::OpenInBlender)
	{
		PythonExpression.append(", import_only=True");

		Arguments << QLatin1String("--window-geometry");
		Arguments << QLatin1String("200 100 1440 900");
	}
	else
	{
		Arguments << QLatin1String("--background");
	}

	PythonExpression.append(", cli_render=True)\"");

	if (QFileInfo::exists(DefaultBlendFile))
		Arguments << QDir::toNativeSeparators(DefaultBlendFile);
	Arguments << QString("--python-expr");
	Arguments << PythonExpression;

	QString ScriptName, ScriptCommand, ShellProgram;

#ifdef Q_OS_WIN
	ScriptName =  QLatin1String("render_ldraw_model.bat");
#else
	ScriptName =  QLatin1String("render_ldraw_model.sh");
#endif
	ScriptCommand = QString("\"%1\" %2").arg(swGetProfileString(SW_PROFILE_BLENDER_PATH), Arguments.join(" "));

	if (mDialogMode == swRenderDialogMode::OpenInBlender)
		ScriptCommand.append(QString(" > %1").arg(QDir::toNativeSeparators(GetStdOutFileName())));

	const QLatin1String LineEnding("\r\n");

	QFile ScriptFile(QString("%1/%2").arg(QDir::tempPath(), ScriptName));
	if (ScriptFile.open(QIODevice::WriteOnly | QIODevice::Text))
	{
		QTextStream Stream(&ScriptFile);
#ifdef Q_OS_WIN
		Stream << QLatin1String("@ECHO OFF &SETLOCAL") << LineEnding;
#else
		Stream << QLatin1String("#!/bin/bash") << LineEnding;
#endif
		Stream << ScriptCommand << LineEnding;
		ScriptFile.close();
	}
	else
	{
		QMessageBox::warning(this, windowTitle(), tr("Cannot write Blender render script file '%1':\n%2.").arg(ScriptFile.fileName(), ScriptFile.errorString()));

		return;
	}

	QThread::sleep(2);

#ifdef Q_OS_WIN
	ShellProgram = QLatin1String(SW_WINDOWS_SHELL);
#else
	ShellProgram = QLatin1String(SW_UNIX_SHELL);
#endif

	mProcess = new swRenderProcess(this);

	connect(mProcess, &QProcess::readyReadStandardOutput, this, &swRenderDialog::ReadStdOut);

	const QString LDrawLibPath = QFileInfo(swGetProfileString(SW_PROFILE_PARTS_LIBRARY)).absolutePath();
	QStringList SystemEnvironment = QProcess::systemEnvironment();
	SystemEnvironment.prepend("LDRAW_DIRECTORY=" + LDrawLibPath);

	mProcess->setEnvironment(SystemEnvironment);

	mProcess->setWorkingDirectory(QDir::toNativeSeparators(QString("%1/blender").arg(mDataPath)));

	mProcess->setStandardErrorFile(GetStdErrFileName());

	if (mDialogMode == swRenderDialogMode::OpenInBlender)
	{
		QFileInfo Info(GetStdOutFileName());
		if (Info.exists())
			QFile::remove(Info.absoluteFilePath());
#ifdef Q_OS_WIN
		mProcess->startDetached(ShellProgram, QStringList() << "/C" << ScriptFile.fileName());
#else
		mProcess->startDetached(ShellProgram, QStringList() << ScriptFile.fileName());
#endif
		if (mProcess)
		{
			mProcess->kill();
			CloseProcess();
			if (mStdOutList.size())
				WriteStdOut();
			if (Info.exists())
			{
				QFile Log(Info.absoluteFilePath());
				QTime Wait = QTime::currentTime().addSecs(3);
				while (!Log.size() || QTime::currentTime() < Wait)
					QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
				if (Log.size()) {
					if (Log.open(QFile::ReadOnly | QFile::Text))
					{
						QByteArray Ba = Log.readAll();
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
						const bool Error = QString(Ba).contains(QRegularExpression("(?:\\w)*ERROR: ", QRegularExpression::CaseInsensitiveOption));
						const bool Warning = QString(Ba).contains(QRegularExpression("(?:\\w)*WARNING: ", QRegularExpression::CaseInsensitiveOption));
#else
						const bool Error = QString(Ba).contains(QRegExp("(?:\\w)*ERROR: ", Qt::CaseInsensitive));
						const bool Warning = QString(Ba).contains(QRegExp("(?:\\w)*WARNING: ", Qt::CaseInsensitive));
#endif
						if (Error || Warning)
						{
							QMessageBox::Icon Icon = QMessageBox::Warning;
							const QString Items = Error ? tr("errors%1").arg(Warning ? tr(" and warnings") : "") : Warning ? tr("warnings") : "";
							const QString Title = tr("Open in Blender Output");
							const QString Body = tr("Open in Blender encountered %1. See Show Details...").arg(Items);
							swBlenderPreferences::ShowMessage(this, Body, Title, QString(), QString(Ba), 0, Icon);
						}
					}
				}
			}
			close();
			return;
		}
	}
	else
	{
		ui->RenderProgress->setRange(mBlendProgValue, mBlendProgMax);
		ui->RenderProgress->setValue(1);
#ifdef Q_OS_WIN
		mProcess->start(ShellProgram, QStringList() << "/C" << ScriptFile.fileName());
#else
		mProcess->start(ShellProgram, QStringList() << ScriptFile.fileName());
#endif
	}

	if (mProcess->waitForStarted())
	{
		mRenderButton->setText(tr("Cancel"));
		ui->RenderProgress->setValue(ui->RenderProgress->minimum());
		QApplication::processEvents();
	}
	else
	{
		Message = tr("Error starting Blender render process.");
		QMessageBox::warning(this, windowTitle(), Message);
		CloseProcess();
	}
}

void swRenderDialog::ReadStdOut()
{
	if (mDialogMode == swRenderDialogMode::RenderPOVRay)
		return;

	QString StdOut = QString(mProcess->readAllStandardOutput());
	mStdOutList.append(StdOut);

#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
	QRegularExpression RxRenderProgress;
	RxRenderProgress.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
#else
	QRegExp RxRenderProgress;
	RxRenderProgress.setCaseSensitivity(Qt::CaseInsensitive);
#endif
	int BlenderVersionNum = QString(swGetProfileString(SW_PROFILE_BLENDER_VERSION).mid(1, 1)).toInt();
	bool BlenderVersion3OrGreater = BlenderVersionNum > 2;

	if (BlenderVersion3OrGreater)
		RxRenderProgress.setPattern("Sample (\\d+)\\/(\\d+)");
	else
		RxRenderProgress.setPattern("(\\d+)\\/(\\d+) Tiles");

#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
	QRegularExpressionMatch Match = RxRenderProgress.match(StdOut);
	if (Match.hasMatch())
	{
		mBlendProgValue = Match.captured(1).toInt();
		mBlendProgMax   = Match.captured(2).toInt();
		ui->RenderProgress->setMaximum(mBlendProgMax);
		ui->RenderProgress->setValue(mBlendProgValue);
	}
#else
	if (StdOut.contains(RxRenderProgress))
	{
		mBlendProgValue = RxRenderProgress.cap(1).toInt();
		mBlendProgMax   = RxRenderProgress.cap(2).toInt();
		ui->RenderProgress->setMaximum(mBlendProgMax);
		ui->RenderProgress->setValue(mBlendProgValue);
	}
#endif
}

QString swRenderDialog::ReadStdErr(bool& HasError) const
{
	HasError = mDialogMode == swRenderDialogMode::RenderBlender ? false : true;

	QFile File;
	QStringList ReturnLines;

	File.setFileName(GetStdErrFileName());

	if (!File.open(QFile::ReadOnly | QFile::Text))
	{
		const QString Message = tr("Failed to open log file: '%1':\n%2").arg(File.fileName(), File.errorString());
		return Message;
	}

	QTextStream In(&File);
	while (!In.atEnd())
	{
		QString Line = In.readLine(0);
		ReturnLines << Line.trimmed() + "<br>";

		if (mDialogMode == swRenderDialogMode::RenderPOVRay)
		{
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
			if (Line.contains(QRegularExpression("^POV-Ray finished$", QRegularExpression::CaseInsensitiveOption)))
				HasError = false;
#else
			if (Line.contains(QRegExp("^POV-Ray finished$", Qt::CaseSensitive)))
				HasError = false;
#endif
		}
		else if (mDialogMode == swRenderDialogMode::RenderBlender)
		{
			if (!HasError && !Line.isEmpty())
				HasError = true;
		}
	}

	return ReturnLines.join(" ");
}

void swRenderDialog::WriteStdOut()
{
	QFile File(GetStdOutFileName());

	if (File.open(QFile::WriteOnly | QIODevice::Truncate | QFile::Text))
	{
		QTextStream Out(&File);

		for (const QString& Line : std::as_const(mStdOutList))
			Out << Line;

		File.close();

		mLogButton->setEnabled(true);
	}
}

void swRenderDialog::Update()
{
	if (!mProcess)
		return;

	UpdateLogPanel();

	const bool ProcessFinished = mProcess->state() == QProcess::NotRunning;

	if (mDialogMode == swRenderDialogMode::RenderPOVRay)
	{
#if SW_POVRAY_MEMORY_MAPPED_FILE
		if (!mOutputBuffer)
		{
			mOutputFile.setFileName(GetStdOutFileName());

			if (!mOutputFile.open(QFile::ReadWrite))
			{
				if (ProcessFinished)
				{
					ShowResult();
					CloseProcess();
				}

				return;
			}

			mOutputBuffer = mOutputFile.map(0, mOutputFile.size());

			if (!mOutputBuffer)
			{
				mOutputFile.close();

				if (ProcessFinished)
				{
					ShowResult();
					CloseProcess();
				}

				return;
			}
		}

		struct swSharedMemoryHeader
		{
			quint32 Version;
			quint32 Width;
			quint32 Height;
			quint32 PixelsWritten;
			quint32 PixelsRead;
		};

		swSharedMemoryHeader* Header = (swSharedMemoryHeader*)mOutputBuffer;

		if (Header->PixelsWritten == Header->PixelsRead && !ProcessFinished)
			return;

		int Width = Header->Width;
		int Height = Header->Height;
		int PixelsWritten = Header->PixelsWritten;

		if (!Header->PixelsRead)
			mImage = QImage(Width, Height, QImage::Format_ARGB32);

		quint8* Pixels = (quint8*)(Header + 1);

		for (int y = 0; y < Height; y++)
		{
			for (int x = 0; x < Width; x++)
			{
				mImage.setPixel(x, y, qRgba(Pixels[0], Pixels[1], Pixels[2], Pixels[3]));
				Pixels += 4;
			}
		}

		Header->PixelsRead = PixelsWritten;

		ui->RenderProgress->setMaximum(mImage.width() * mImage.height());
		ui->RenderProgress->setValue(int(Header->PixelsRead));

		if (PixelsWritten == Width * Height)
			ui->RenderProgress->setValue(ui->RenderProgress->maximum());

		ui->preview->SetImage(mImage.scaled(mPreviewWidth, mPreviewHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation));
#endif
	}

	if (ProcessFinished)
	{
#ifdef Q_OS_LINUX
		QByteArray Output = mProcess->readAllStandardOutput();
		mImage = QImage::fromData(Output);
#endif

		ShowResult();
		CloseProcess();

		if (!mJobs.empty())
		{
			mJobIndex++;
			StartNextJob();
		}
	}
}

void swRenderDialog::ShowResult()
{
	bool Error;
	const QString StdErrLog = ReadStdErr(Error);

	UpdateLogPanel();

	if (mProcess->exitStatus() != QProcess::NormalExit || mProcess->exitCode() != 0 || Error)
	{
		ui->RenderProgress->setRange(0, 1);
		ui->RenderProgress->setValue(0);
		mJobs.clear();
		mJobIndex = 0;
		ui->RenderProgress->setFormat("%p%");

		const QString Body  = tr("An error occurred while rendering.");
		swBlenderPreferences::ShowMessage(this, Body, windowTitle(), QString(), StdErrLog, 0, QMessageBox::Warning);
		return;
	}
	else
	{
		ui->RenderProgress->setValue(ui->RenderProgress->maximum());
	}

	bool Success = false;
	QString FileName = mDialogMode == swRenderDialogMode::RenderPOVRay && mJobIndex < mJobs.size() ? mJobs[mJobIndex].FileName : ui->OutputEdit->text();

	if (mDialogMode == swRenderDialogMode::RenderPOVRay)
	{
		if (mJobRegion.isValid() && !mImage.isNull())
		{
			// place regions into the saved image
			const QSize ImageSize(mWidth, mHeight);
			QImage BaseImage = mResultFileName == FileName && mResultImage.size() == ImageSize ? mResultImage : QImage();

			if (BaseImage.isNull() && !FileName.isEmpty())
			{
				QImageReader Reader(FileName);

				if (Reader.size() == ImageSize)
					BaseImage = Reader.read();
			}

			if (BaseImage.size() == ImageSize)
			{
				BaseImage = BaseImage.convertToFormat(QImage::Format_ARGB32);
				QPainter Painter(&BaseImage);
				Painter.setCompositionMode(QPainter::CompositionMode_Source);

				if (mImage.size() == ImageSize)
					Painter.drawImage(mJobRegion, mImage, mJobRegion);
				else if (mImage.size() == mJobRegion.size())
					Painter.drawImage(mJobRegion.topLeft(), mImage);

				Painter.end();
				mImage = BaseImage;
			}
		}

		mResultImage = mImage;
		mResultFileName = FileName;
		ui->preview->SetImage(mImage.scaled(mPreviewWidth, mPreviewHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation));

		if (!FileName.isEmpty())
		{
			QImageWriter Writer(FileName);

			Success = Writer.write(mImage);

			if (!Success)
				QMessageBox::warning(this, windowTitle(), tr("Error writing to file '%1':\n%2").arg(FileName, Writer.errorString()));
		}

	}
	else if (mDialogMode == swRenderDialogMode::RenderBlender)
	{
		Success = QFileInfo::exists(FileName);
		if (Success)
		{
			setMinimumSize(100, 100);
			QImageReader Reader(FileName);
			mImage = Reader.read();
			mImage = mImage.convertToFormat(QImage::Format_ARGB32_Premultiplied);

			mPreviewWidth  = mImage.width();
			mPreviewHeight = mImage.height();

			ui->preview->SetImage(mImage.scaled(mPreviewWidth, mPreviewHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation));
		}
	}

	if (!Success)
	{
		mJobs.clear();
		mJobIndex = 0;
		QMessageBox::warning(this, windowTitle(), tr("Render failed."));
	}

	WriteStdOut();
}

void swRenderDialog::OutputBrowseButtonClicked()
{
	const QString Result = QFileDialog::getSaveFileName(this, tr("Select Output File"), ui->OutputEdit->text(), tr("Supported Image Files (*.bmp *.png *.jpg);;BMP Files (*.bmp);;PNG Files (*.png);;JPEG Files (*.jpg);;All Files (*.*)"));

	if (!Result.isEmpty())
		ui->OutputEdit->setText(QDir::toNativeSeparators(Result));
}

void swRenderDialog::SettingsButtonClicked()
{
	swBlenderPreferencesDialog::GetBlenderPreferences(mWidth, mHeight, mScale, this);

	if (swGetProfileString(SW_PROFILE_BLENDER_IMPORT_MODULE).isEmpty())
		mRenderButton->setToolTip(tr("Blender not configured. Use Settings to configure."));
	else
		mRenderButton->setEnabled(true);
}

void swRenderDialog::LogButtonClicked()
{
	// show the log in the log panel
	if (mLogPanel)
		return;

	QFileInfo FileInfo;
	QString Message ;

	if (mDialogMode == swRenderDialogMode::RenderPOVRay)
	{
		FileInfo.setFile(GetStdErrFileName());
		Message = tr("POV-Ray standard error file not found: '%1'.").arg(FileInfo.absoluteFilePath());
	}
	else
	{
		FileInfo.setFile(GetStdOutFileName());
		Message = tr("Blender standard output file not found: '%1'.").arg(FileInfo.absoluteFilePath());
	}

	if (!FileInfo.exists())
	{
		QMessageBox::warning(this, tr("Render Log"), Message);
		return;
	}

	QFile File(FileInfo.absoluteFilePath());

	if (!File.open(QFile::ReadOnly | QFile::Text))
	{
		QMessageBox::warning(this, tr("Render Log"), tr("Error opening log file '%1':\n%2.").arg(FileInfo.absoluteFilePath(), File.errorString()));
		return;
	}

	QString Text = File.readAll();
	File.close();

	QDialog Dialog(this);

	Dialog.setWindowTitle(tr("Render Log"));
	Dialog.setWhatsThis(tr("<p><b>Render Log</b></p><p>Read the renderer output to investigate warnings or a failed render. Select text and copy it with the standard keyboard shortcut when reporting a problem. Close returns to the Render dialog.</p>"));
	Dialog.resize(800, 600);

	QVBoxLayout* Layout = new QVBoxLayout(&Dialog);

	QTextBrowser* TextBrowser = new QTextBrowser(&Dialog);
	Layout->addWidget(TextBrowser);
	TextBrowser->setAccessibleName(tr("Renderer Output"));
	TextBrowser->setToolTip(tr("Read and copy renderer output and diagnostic messages."));

	TextBrowser->setText(Text);

	QDialogButtonBox* ButtonBox = new QDialogButtonBox(QDialogButtonBox::Close | QDialogButtonBox::Help, Qt::Horizontal, &Dialog);
	Layout->addWidget(ButtonBox);
	swSetupDialogHelp(&Dialog, ButtonBox);

	QObject::connect(ButtonBox, &QDialogButtonBox::rejected, &Dialog, &QDialog::reject);
	QObject::connect(ButtonBox, &QDialogButtonBox::accepted, &Dialog, &QDialog::accept);

	Dialog.exec();
}

swRenderProcess::~swRenderProcess()
{
	if (state() == QProcess::Running || state() == QProcess::Starting)
	{
		terminate();
		waitForFinished();
	}
}

#endif // SW_DISABLE_RENDER_DIALOG
