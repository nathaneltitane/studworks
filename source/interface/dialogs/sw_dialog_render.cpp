#include "sw_global.h"
#include "sw_dialog_render.h"
#include "ui_sw_dialog_render.h"
#include "sw_project.h"
#include "sw_application.h"
#include "sw_profile.h"
#include "sw_dialog_preferences_blender.h"
#include "sw_model.h"
#include "sw_window_utilities.h"

#ifndef SW_DISABLE_RENDER_DIALOG

#define SW_POVRAY_PREVIEW_WIDTH 768
#define SW_POVRAY_PREVIEW_HEIGHT 432
// raise to drop the panel layouts saved by older versions of the window
#define SW_RENDER_WINDOW_STATE_VERSION 1
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

void swRenderPreviewWidget::paintEvent(QPaintEvent* PaintEvent)
{
	Q_UNUSED(PaintEvent);

	QPainter Painter(this);

	if (!mImage.isNull())
	{
		QSize Size = size();

		if (mScaledImage.isNull())
			mScaledImage = mImage.scaled(Size, Qt::KeepAspectRatio, Qt::SmoothTransformation);

		Painter.drawImage((Size.width() - mScaledImage.width()) / 2, (Size.height() - mScaledImage.height()) / 2, mScaledImage);
	}
	else
		Painter.fillRect(rect(), Qt::transparent);
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
		// the povray window keeps its buttons in the render panel like the print panel of the instructions window
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
		// placeholder with the render picture centered on a transparent 16:9 frame
		QImage Image(1280, 720, QImage::Format_ARGB32_Premultiplied);
		Image.fill(Qt::transparent);
		QPainter Painter(&Image);
		Painter.drawImage(440, 160, swCreatePaintedIconImage(swPaintedIcon::RenderPlaceholder, 400, palette().color(QPalette::WindowText)));
		Painter.end();
		ui->preview->SetImage(Image);
	}

	connect(&mUpdateTimer, &QTimer::timeout, this, &swRenderDialog::Update);

	mUpdateTimer.start(500);
}

// use to lay out the povray window like the instructions window with its settings in panels beside the preview
void swRenderDialog::CreatePOVRayPanels()
{
	QMainWindow* PanelWindow = new QMainWindow(this);
	PanelWindow->setWindowFlags(Qt::Widget);
	PanelWindow->setDockOptions(QMainWindow::AnimatedDocks | QMainWindow::AllowNestedDocks);

	mPanelWindow = PanelWindow;

	// the preview fills the middle of the window like the pages of the instructions window
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

		// a panel taller than its rows keeps them under its title
		Panel->layout()->setAlignment(Widget, Qt::AlignTop);

		// panels across the top or bottom span the window like the terminal, floating they keep the panel width
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

	QWidget* OutputPanel = CreatePanel(tr("Output"), "RenderOutput", tr("<p><b>Output</b></p><p>Choose where the rendered image is saved and its format, such as PNG, JPEG, or BMP.</p>"));

	QWidget* FileRow = CreateRow(OutputPanel, tr("File"), ui->OutputEdit, ui->OutputEdit->toolTip(), ui->OutputEdit->whatsThis());
	ui->OutputBrowseButton->setParent(FileRow);
	swSetupPanelButton(ui->OutputBrowseButton);
	FileRow->layout()->addWidget(ui->OutputBrowseButton);

	QWidget* ImagePanel = CreatePanel(tr("Image"), "RenderImage", tr("<p><b>Image</b></p><p>Set the size of the rendered image in pixels and the rendering quality. Larger images and higher quality take longer to render.</p>"));

	QSpinBox* WidthSpinBox = CreateSpinBox(mWidth);
	CreateRow(ImagePanel, tr("Width"), WidthSpinBox, tr("Set the rendered image width in pixels (1 to 32768)."), QString());

	QSpinBox* HeightSpinBox = CreateSpinBox(mHeight);
	CreateRow(ImagePanel, tr("Height"), HeightSpinBox, tr("Set the rendered image height in pixels (1 to 32768)."), QString());

	QComboBox* QualityComboBox = CreateComboBox({ tr("Low"), tr("Medium"), tr("High") }, static_cast<int>(mQuality));
	CreateRow(ImagePanel, tr("Quality"), QualityComboBox, tr("Choose the rendering quality; higher quality can take longer."), tr("<p><b>Quality</b></p><p>Low uses POV-Ray's default quality without additional antialiasing options. Medium uses quality level 5 with antialiasing and fast radiosity. High uses quality level 11 with additional antialiasing settings and full radiosity.</p>"));

	QWidget* ScenePanel = CreatePanel(tr("Scene"), "RenderScene", tr("<p><b>Scene</b></p><p>Set the lights, floor and surroundings the model is rendered in.</p>"));

	QComboBox* LightRigComboBox = CreateComboBox({ tr("Standard"), tr("Studio"), tr("Matte") }, swGetProfileInteger(SW_PROFILE_RENDER_LIGHT_RIG));
	CreateRow(ScenePanel, tr("Lights"), LightRigComboBox, tr("Choose the lights placed around models that have no lights of their own."), tr("<p><b>Lights</b></p><p>Standard uses four point lights with crisp shadows. Studio uses a large soft key light, a fill light and a rim light behind the model. Matte uses a wide overhead light and a shadowless light from the camera for even, soft lighting. Lights placed in the model replace these.</p>"));

	QComboBox* FloorStyleComboBox = CreateComboBox({ tr("Plain"), tr("Shadow Catcher"), tr("Reflective"), tr("Cyclorama") }, swGetProfileInteger(SW_PROFILE_RENDER_FLOOR_STYLE));
	CreateRow(ScenePanel, tr("Floor"), FloorStyleComboBox, tr("Choose how the floor under the model looks."), tr("<p><b>Floor</b></p><p>Plain uses the floor color of the model. Shadow Catcher takes the background color so only the shadows show. Reflective adds a glossy mirror finish. Cyclorama fades the floor into the background with distance for a seamless backdrop.</p>"));

	QLineEdit* EnvironmentEdit = new QLineEdit();
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

	QComboBox* BackgroundComboBox = CreateComboBox({ tr("Visible"), tr("Transparent") }, swGetProfileInteger(SW_PROFILE_RENDER_TRANSPARENT) ? 1 : 0);
	CreateRow(ScenePanel, tr("Background"), BackgroundComboBox, tr("Choose whether the background and floor are rendered or left as transparent pixels for compositing."), QString());

	QWidget* RenderPanel = CreatePanel(tr("Render"), "RenderRender", tr("<p><b>Render</b></p><p>Start the render and follow its progress. While rendering, Render becomes Cancel so you can stop it. Log shows the renderer output below the preview.</p>"));

	QWidget* ProgressRow = swCreatePanelRow(RenderPanel);
	RenderPanel->layout()->addWidget(ProgressRow);
	ui->RenderProgress->setParent(ProgressRow);
	ui->RenderProgress->setFixedHeight(swGetPanelButtonSize().height());
	ProgressRow->layout()->addWidget(ui->RenderProgress);

	// the render button spans the panel below its progress like print in the print panel
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

	// the panels end at their last row and the scene panel takes the remaining height
	static_cast<QVBoxLayout*>(ScenePanel->layout())->addStretch(1);

	// the labels of all panels share one width so the inputs line up
	int LabelWidth = 0;

	for (QLabel* Label : Labels)
		LabelWidth = std::max(LabelWidth, Label->sizeHint().width());

	for (QLabel* Label : Labels)
		Label->setFixedWidth(LabelWidth);

	// changes are kept right away for the next render
	connect(WidthSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int Value)
	{
		mWidth = Value;
		swSetProfileInteger(SW_PROFILE_RENDER_WIDTH, Value);
	});

	connect(HeightSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int Value)
	{
		mHeight = Value;
		swSetProfileInteger(SW_PROFILE_RENDER_HEIGHT, Value);
	});

	connect(QualityComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int Index)
	{
		mQuality = static_cast<swPOVRayRenderQuality>(Index);
		swSetProfileRenderQuality(mQuality);
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

	connect(EnvironmentButton, &QToolButton::clicked, this, [this, EnvironmentEdit]()
	{
		const QString FileName = QFileDialog::getOpenFileName(this, tr("Select Environment Image"), EnvironmentEdit->text(), tr("Environment Images (*.hdr *.exr *.png *.jpg *.jpeg *.tif *.tiff);;All Files (*.*)"));

		if (!FileName.isEmpty())
			EnvironmentEdit->setText(QDir::toNativeSeparators(FileName));
	});

	// the log sits below the preview like the terminal below the model and follows the renderer output
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
	PanelWindow->addDockWidget(Qt::BottomDockWidgetArea, mLogPanel);
	mLogPanel->hide();

	// the log button shows and hides the log panel and stays checked while it is shown
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

	for (QWidget* Panel : { OutputPanel, ImagePanel, RenderPanel })
		Panel->setFixedHeight(Panel->sizeHint().height());

	// panels moved to another area open at a usable size instead of collapsing to their least size
	const QList<QDockWidget*> CompactPanels = { qobject_cast<QDockWidget*>(OutputPanel->parentWidget()), qobject_cast<QDockWidget*>(ImagePanel->parentWidget()), qobject_cast<QDockWidget*>(RenderPanel->parentWidget()) };
	const QList<QDockWidget*> StretchPanels = { qobject_cast<QDockWidget*>(ScenePanel->parentWidget()), mLogPanel };

	auto FitPanels = [PanelWindow, CompactPanels, StretchPanels]()
	{
		// the compact panels end at their last row when an area has a panel to take the remaining height
		for (QDockWidget* CompactPanel : CompactPanels)
		{
			const Qt::DockWidgetArea Area = PanelWindow->dockWidgetArea(CompactPanel);
			bool Shared = false;

			for (QDockWidget* StretchPanel : StretchPanels)
				if (!StretchPanel->isFloating() && !StretchPanel->isHidden() && PanelWindow->dockWidgetArea(StretchPanel) == Area)
					Shared = true;

			CompactPanel->setMaximumHeight(Shared && !CompactPanel->isFloating() ? CompactPanel->sizeHint().height() : QWIDGETSIZE_MAX);
		}

		// panels squeezed by a panel docked next to them get their usable size back too
		QList<QDockWidget*> SidePanels, EdgePanels;

		for (QDockWidget* AreaPanel : PanelWindow->findChildren<QDockWidget*>(QString(), Qt::FindDirectChildrenOnly))
		{
			if (AreaPanel->isFloating() || AreaPanel->isHidden())
				continue;

			const Qt::DockWidgetArea Area = PanelWindow->dockWidgetArea(AreaPanel);

			if ((Area == Qt::LeftDockWidgetArea || Area == Qt::RightDockWidgetArea) && AreaPanel->width() < SW_RENDER_PANEL_WIDTH)
				SidePanels.append(AreaPanel);
			else if ((Area == Qt::TopDockWidgetArea || Area == Qt::BottomDockWidgetArea) && AreaPanel->height() < SW_RENDER_LOG_HEIGHT)
				EdgePanels.append(AreaPanel);
		}

		if (!SidePanels.isEmpty())
			PanelWindow->resizeDocks(SidePanels, QVector<int>(SidePanels.size(), SW_RENDER_PANEL_WIDTH).toList(), Qt::Horizontal);

		if (!EdgePanels.isEmpty())
			PanelWindow->resizeDocks(EdgePanels, QVector<int>(EdgePanels.size(), SW_RENDER_LOG_HEIGHT).toList(), Qt::Vertical);
	};

	for (QDockWidget* Panel : PanelWindow->findChildren<QDockWidget*>(QString(), Qt::FindDirectChildrenOnly))
	{
		// the panel sizes are set after the shared dock fitting has reset them
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

	// the panels are wide enough for a readable output path
	PanelWindow->resizeDocks({ qobject_cast<QDockWidget*>(OutputPanel->parentWidget()) }, { SW_RENDER_PANEL_WIDTH }, Qt::Horizontal);

	// the panels come back where they were left
	QSettings Settings;
	Settings.beginGroup("RenderWindow");
	PanelWindow->restoreState(Settings.value("State").toByteArray(), SW_RENDER_WINDOW_STATE_VERSION);
	Settings.endGroup();

	// the panels without a toggle are never lost to a layout saved while they were hidden
	for (QWidget* Panel : { OutputPanel, ImagePanel, ScenePanel, RenderPanel })
		Panel->parentWidget()->show();

	mLogButton->setChecked(!mLogPanel->isHidden());

	// a layout saved with squeezed panels opens at a usable size once the window is laid out
	QTimer::singleShot(0, PanelWindow, FitPanels);
}

// use to fill the log panel with the renderer output written so far
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
	// the panels are kept where they are for the next time the window opens
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

	if (mDialogMode == swRenderDialogMode::RenderPOVRay)
		RenderPOVRay();
	else
		RenderBlender();
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
	ui->RenderProgress->setFormat(tr("Exporting Model"));

	QApplication::setOverrideCursor(Qt::WaitCursor);

	swResult<void> ExportResult = swGetActiveProject()->ExportPOVRay(FileName);

	QGuiApplication::restoreOverrideCursor();

	ui->RenderProgress->setFormat("%p%");

	if (!ExportResult)
	{
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

#if SW_POVRAY_MEMORY_MAPPED_FILE
	Arguments.append(QString::fromLatin1("+SM\"%1\"").arg(GetStdOutFileName()));
#endif

	switch (mQuality)
	{
	case swPOVRayRenderQuality::High:
		Arguments.append("+Q11");
		Arguments.append("+R3");
		Arguments.append("+A0.1");
		Arguments.append("+J0.5");
		break;

	case swPOVRayRenderQuality::Medium:
		Arguments.append("+Q5");
		Arguments.append("+A0.1");
		break;

	case swPOVRayRenderQuality::Low:
		break;
	}

	QString POVRayPath;

#ifdef Q_OS_WIN
	POVRayPath = QDir::cleanPath(QCoreApplication::applicationDirPath() + QLatin1String("/povconsole32-sse2.exe"));
#endif

#ifdef Q_OS_LINUX
	POVRayPath = swGetProfileString(SW_PROFILE_POVRAY_PATH);
	Arguments.append("+FN");
	Arguments.append("-D");
#endif

#ifdef Q_OS_MACOS
	POVRayPath = QDir::cleanPath(QCoreApplication::applicationDirPath() + QLatin1String("/povray"));
#endif

	const QString POVRayDir = QFileInfo(POVRayPath).absolutePath();
	const QString IncludePath = QDir::cleanPath(POVRayDir + "/include");

	if (QFileInfo::exists(IncludePath))
		Arguments.append(QString("+L\"%1\"").arg(IncludePath));

	const QString IniPath = QDir::cleanPath(POVRayDir + "/ini");

	if (QFileInfo::exists(IniPath))
		Arguments.append(QString("+L\"%1\"").arg(IniPath));

	if (swGetActiveProject()->GetModels()[0]->GetPOVRayOptions().UseLGEO)
	{
		const QString LGEOPath = swGetProfileString(SW_PROFILE_POVRAY_LGEO_PATH);

		if (QFileInfo::exists(LGEOPath))
		{
			const QString LgPath = QDir::cleanPath(LGEOPath + "/lg");
			if (QFileInfo::exists(LgPath))
				Arguments.append(QString("+L\"%1\"").arg(LgPath));
			const QString ArPath = QDir::cleanPath(LGEOPath + "/ar");
			if (QFileInfo::exists(ArPath))
				Arguments.append(QString("+L\"%1\"").arg(ArPath));
			const QString StlPath = QDir::cleanPath(LGEOPath + "/stl");
			if (QFileInfo::exists(StlPath))
				Arguments.append(QString("+L\"%1\"").arg(StlPath));
		}
	}

	swRenderProcess* Process = new swRenderProcess(this);
#ifdef Q_OS_LINUX
	connect(Process, &QProcess::readyReadStandardError, this, [this]() { bool Error; ReadStdErr(Error); });
#endif
	QStringList POVEnv = QProcess::systemEnvironment();
	POVEnv.prepend("POV_IGNORE_SYSCONF_MSG=1");
	Process->setEnvironment(POVEnv);
	Process->setStandardErrorFile(GetStdErrFileName());
	Process->start(POVRayPath, Arguments);

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

		const QString Body  = tr("An error occurred while rendering.");
		swBlenderPreferences::ShowMessage(this, Body, windowTitle(), QString(), StdErrLog, 0, QMessageBox::Warning);
		return;
	}
	else
	{
		ui->RenderProgress->setValue(ui->RenderProgress->maximum());
	}

	bool Success = false;
	QString FileName = ui->OutputEdit->text();

	if (mDialogMode == swRenderDialogMode::RenderPOVRay)
	{
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
		QMessageBox::warning(this, windowTitle(), tr("Render failed."));

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
	// the povray window shows its log in the log panel
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
