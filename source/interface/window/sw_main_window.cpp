#include "sw_global.h"
#include "sw_main_window.h"
#include "sw_minifigure_builder_widget.h"
#include "sw_double_spin_box.h"
#include <QPrintDialog>
#include <QPrintPreviewDialog>
#include "sw_part_selection_widget.h"
#ifdef SW_ENABLE_TERMINAL
#include "sw_terminal_widget.h"
#endif
#include "sw_properties_widget.h"
#include "sw_timeline_widget.h"
#include "sw_view_widget.h"
#include "sw_color_list.h"
#include "sw_utilities.h"
#include "sw_about_dialog.h"
#include "sw_select_dialog.h"
#include "sw_sets_database_dialog.h"
#include "sw_html_dialog.h"
#include "sw_render_dialog.h"
#include "sw_instructions_dialog.h"
#include "sw_image_dialog.h"
#include "sw_profile.h"
#include "sw_view.h"
#include "sw_project.h"
#include "sw_piece.h"
#include "sw_camera.h"
#include "sw_group.h"
#include "sw_piece_data.h"
#include "sw_library.h"
#include "sw_colors.h"
#include "sw_preview_widget.h"
#include "sw_model_history.h"
#include "sw_spinner_widget.h"

#if SW_ENABLE_GAMEPAD
#include <QtGamepad/QGamepad>
#endif

swMainWindow* gMainWindow;
#define SW_TAB_LAYOUT_VERSION 0x0001

swMainWindow::swMainWindow()
{
	mTransformType = swTransformType::RelativeTranslation;

	mColorIndex = swGetColorIndex(7);
	mTool = swTool::Select;
	mMoveXYSnapIndex = 4;
	mMoveZSnapIndex = 3;
	mAngleSnapIndex = 5;
	mAngleSnapCustom = swGetProfileFloat(SW_PROFILE_ANGLE_SNAP_CUSTOM);
	mOrbitSnapEnabled = swGetProfileInteger(SW_PROFILE_ORBIT_SNAP);
	mSelectionMode = swSelectionMode::Single;

	for (size_t FileIndex = 0; FileIndex < mRecentFiles.size(); FileIndex++)
		mRecentFiles[FileIndex] = swGetProfileString((SW_PROFILE_KEY)(SW_PROFILE_RECENT_FILE1 + FileIndex));

#if (QT_VERSION >= QT_VERSION_CHECK(5, 8, 0))
	connect(&mGamepadTimer, &QTimer::timeout, this, &swMainWindow::UpdateGamepads);
	mLastGamepadUpdate = QDateTime::currentDateTime();
	mGamepadTimer.start(33);
#endif

	gMainWindow = this;
}

swMainWindow::~swMainWindow()
{
	swPiecesLibrary* Library = swGetPiecesLibrary();
	disconnect(Library, nullptr, this, nullptr);

	mInsertPieceData.Reset();

	for (size_t FileIndex = 0; FileIndex < mRecentFiles.size(); FileIndex++)
		swSetProfileString((SW_PROFILE_KEY)(SW_PROFILE_RECENT_FILE1 + FileIndex), mRecentFiles[FileIndex]);

	gMainWindow = nullptr;
}

void swMainWindow::CreateWidgets()
{
	setAcceptDrops(true);
	setWindowIcon(QIcon(":/resources/studworks.png"));
	setWindowFilePath(QString());

	CreateActions();
	CreateToolBars();
	CreateMenus();
	CreateStatusBar();

	// Let panels be docked next to, above or below each other on any side of the model view.
	setDockOptions(QMainWindow::AnimatedDocks | QMainWindow::AllowNestedDocks | QMainWindow::AllowTabbedDocks);

	mModelTabWidget = new QTabWidget();
	mModelTabWidget->tabBar()->installEventFilter(this);
	qApp->installEventFilter(this);
	mModelTabWidget->tabBar()->setMovable(true);
	mModelTabWidget->tabBar()->setTabsClosable(true);
	mModelTabWidget->tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);
	setCentralWidget(mModelTabWidget);

	// Model tabs and the view below them each get a complete outline so neither looks cut off.
	// They are as tall as the panel buttons and centered in a bar as tall as the panel title bars.
	const int TabHeight = swGetPanelButtonSize().height();
	const int TitleBarHeight = mModelTabWidget->fontMetrics().height() + 2 * style()->pixelMetric(QStyle::PM_DockWidgetTitleMargin);
	const int TabMargin = qMax((TitleBarHeight - TabHeight) / 2, 0);

	mModelTabWidget->setStyleSheet(QString(
		"QTabWidget::pane { border: 1px solid palette(mid); }"
		"QTabBar::tab { border: 1px solid palette(mid); border-radius: 3px; padding: 0px 8px; min-height: %1px; margin: %2px 2px %2px 0px; }"
		"QTabBar::tab:selected { background: palette(button); }"
		"QTabBar::tab:!selected { background: palette(window); }").arg(TabHeight - 2).arg(TabMargin));

	// The options button uses the same margins as the tabs so both line up in the tab bar.
	QWidget* ModelViewCorner = new QWidget(mModelTabWidget);
	QHBoxLayout* ModelViewCornerLayout = new QHBoxLayout(ModelViewCorner);
	ModelViewCornerLayout->setContentsMargins(0, TabMargin, TabMargin, TabMargin);

	QToolButton* ModelViewOptionsButton = new QToolButton(ModelViewCorner);
	ModelViewOptionsButton->setIcon(swCreateInterfaceIcon("widgets", "button_options"));
	ModelViewOptionsButton->setToolTip(tr("Configure the model view display and rendering."));
	ModelViewOptionsButton->setAccessibleName(tr("Model View Options"));
	ModelViewOptionsButton->setPopupMode(QToolButton::InstantPopup);
	swSetupPanelButton(ModelViewOptionsButton);
	ModelViewCornerLayout->addWidget(ModelViewOptionsButton, 0, Qt::AlignVCenter);

	QMenu* ModelViewOptionsMenu = new QMenu(ModelViewOptionsButton);
	ModelViewOptionsButton->setMenu(ModelViewOptionsMenu);
	mModelTabWidget->setCornerWidget(ModelViewCorner, Qt::TopRightCorner);

	connect(ModelViewOptionsMenu, &QMenu::aboutToShow, this, &swMainWindow::ModelViewOptionsMenuAboutToShow);

	connect(mModelTabWidget->tabBar(), &QTabBar::tabCloseRequested, this, &swMainWindow::ModelTabClosed);
	connect(mModelTabWidget->tabBar(), &QTabBar::customContextMenuRequested, this, &swMainWindow::ModelTabContextMenuRequested);
	connect(mModelTabWidget, &QTabWidget::currentChanged, this, &swMainWindow::ModelTabChanged);

	connect(QApplication::clipboard(), &QClipboard::dataChanged, this, &swMainWindow::ClipboardChanged);
	ClipboardChanged();

	QSettings Settings;
	Settings.beginGroup("MainWindow");
	resize(QSize(800, 600));
	move(QPoint(200, 200));
	restoreGeometry(Settings.value("Geometry").toByteArray());
	restoreState(Settings.value("State").toByteArray());
	mPartSelectionWidget->LoadState(Settings);
	Settings.endGroup();
}

void swMainWindow::CreateActions()
{
	for (int CommandIdx = 0; CommandIdx < SW_COMMAND_COUNT; CommandIdx++)
	{
		QAction* Action = new QAction(qApp->translate("Menu", gCommands[CommandIdx].MenuName), this);
		Action->setStatusTip(qApp->translate("Status", gCommands[CommandIdx].StatusText));
		if (gCommands[CommandIdx].WhatsThis[0])
			Action->setWhatsThis(qApp->translate("WhatsThis", gCommands[CommandIdx].WhatsThis));
		connect(Action, &QAction::triggered, this, &swMainWindow::ActionTriggered);
		addAction(Action);
		mActions[CommandIdx] = Action;
	}

	mActions[SW_HELP_WHATS_THIS]->setToolTip(mActions[SW_HELP_WHATS_THIS]->statusTip());
	mActions[SW_FILE_NEW]->setToolTip(tr("New Model"));
	mActions[SW_FILE_OPEN]->setToolTip(tr("Open Model"));
	mActions[SW_FILE_SAVE]->setToolTip(tr("Save Model"));

	mActions[SW_FILE_NEW]->setIcon(swCreateInterfaceIcon("toolbars/standard", "file_new"));

	mActions[SW_FILE_SAVE]->setIcon(swCreateInterfaceIcon("toolbars/standard", "file_save"));

	QIcon FileOpenIcon = swCreateInterfaceIcon("toolbars/standard", "file_open");
	mActions[SW_FILE_OPEN]->setIcon(FileOpenIcon);
	mActions[SW_VIEW_TERMINAL_OPEN_MODEL_FOLDER]->setIcon(FileOpenIcon);
	mActions[SW_VIEW_TERMINAL_OPEN_MODEL_FOLDER]->setToolTip(tr("Open Model Folder"));

	mActions[SW_EDIT_UNDO]->setIcon(swCreateInterfaceIcon("toolbars/standard", "edit_undo"));

	mActions[SW_EDIT_REDO]->setIcon(swCreateInterfaceIcon("toolbars/standard", "edit_redo"));

	mActions[SW_EDIT_ACTION_INSERT]->setIcon(swCreateInterfaceIcon("toolbars/tools", "action_insert"));

	mActions[SW_EDIT_ACTION_POINT_LIGHT]->setIcon(swCreateInterfaceIcon("toolbars/tools", "action_light_point"));

	mActions[SW_EDIT_ACTION_SPOTLIGHT]->setIcon(swCreateInterfaceIcon("toolbars/tools", "action_light_spot"));

	mActions[SW_EDIT_ACTION_DIRECTIONAL_LIGHT]->setIcon(swCreateInterfaceIcon("toolbars/tools", "action_light_directional"));

	mActions[SW_EDIT_ACTION_AREA_LIGHT]->setIcon(swCreateInterfaceIcon("toolbars/tools", "action_light_area"));

	mActions[SW_EDIT_ACTION_SELECT]->setIcon(swCreateInterfaceIcon("toolbars/tools", "action_select"));

	mActions[SW_EDIT_ACTION_MOVE]->setIcon(swCreateInterfaceIcon("toolbars/tools", "action_move"));

	mActions[SW_EDIT_ACTION_ROTATE]->setIcon(swCreateInterfaceIcon("toolbars/tools", "action_rotate"));

	mActions[SW_EDIT_ACTION_DELETE]->setIcon(swCreateInterfaceIcon("toolbars/tools", "action_delete"));

	mActions[SW_EDIT_ACTION_PAINT]->setIcon(swCreateInterfaceIcon("toolbars/tools", "action_paint"));

	mActions[SW_EDIT_ACTION_COLOR_PICKER]->setIcon(swCreateInterfaceIcon("toolbars/tools", "action_picker_color"));

	mActions[SW_EDIT_ACTION_ZOOM]->setIcon(swCreateInterfaceIcon("toolbars/tools", "action_zoom"));

	mActions[SW_EDIT_ACTION_PAN]->setIcon(swCreateInterfaceIcon("toolbars/tools", "action_pan"));

	mActions[SW_EDIT_ACTION_CAMERA]->setIcon(swCreateInterfaceIcon("toolbars/tools", "action_camera"));
	mActions[SW_EDIT_ACTION_ROTATE_VIEW]->setIcon(swCreateInterfaceIcon("toolbars/tools", "action_view_rotate"));
	mActions[SW_EDIT_ACTION_ROLL]->setIcon(swCreateInterfaceIcon("toolbars/tools", "action_roll"));
	mActions[SW_EDIT_ACTION_ZOOM_REGION]->setIcon(swCreateInterfaceIcon("toolbars/tools", "action_zoom_region"));
	mActions[SW_VIEW_TIME_FIRST]->setIcon(swCreateInterfaceIcon("toolbars/time", "time_first"));
	mActions[SW_VIEW_TIME_PREVIOUS]->setIcon(swCreateInterfaceIcon("toolbars/time", "time_previous"));
	mActions[SW_VIEW_TIME_NEXT]->setIcon(swCreateInterfaceIcon("toolbars/time", "time_next"));
	mActions[SW_VIEW_TIME_LAST]->setIcon(swCreateInterfaceIcon("toolbars/time", "time_last"));
	mActions[SW_VIEW_TIME_ADD_KEYS]->setIcon(swCreateInterfaceIcon("toolbars/time", "time_keys_add"));

	mActions[SW_EDIT_SNAP_MOVE_TOGGLE]->setCheckable(true);
	mActions[SW_EDIT_SNAP_ANGLE_TOGGLE]->setCheckable(true);
	mActions[SW_VIEW_CAMERA_NONE]->setCheckable(true);
	mActions[SW_VIEW_TIME_ADD_KEYS]->setCheckable(true);
	mActions[SW_VIEW_STEP_VIEW_TOGGLE_FOLLOW]->setCheckable(true);
	mActions[SW_VIEW_STEP_VIEW_TOGGLE_FOLLOW]->setChecked(swGetPreferences().mFollowStepViews);
	mActions[SW_PIECE_TOGGLE_FIRST_AT_ORIGIN]->setCheckable(true);

	for (int ActionIndex = SW_VIEW_TOOLBAR_FIRST; ActionIndex <= SW_VIEW_TOOLBAR_LAST; ActionIndex++)
		mActions[ActionIndex]->setCheckable(true);

	QActionGroup* ActionRelativeGroup = new QActionGroup(this);
	for (int ActionIdx = SW_EDIT_TRANSFORM_RELATIVE; ActionIdx <= SW_EDIT_TRANSFORM_ABSOLUTE; ActionIdx++)
	{
		mActions[ActionIdx]->setCheckable(true);
		ActionRelativeGroup->addAction(mActions[ActionIdx]);
	}

	QActionGroup* ActionSeparateGroup = new QActionGroup(this);
	for (int ActionIdx = SW_EDIT_TRANSFORM_SEPARATELY; ActionIdx <= SW_EDIT_TRANSFORM_TOGETHER; ActionIdx++)
	{
		mActions[ActionIdx]->setCheckable(true);
		ActionSeparateGroup->addAction(mActions[ActionIdx]);
	}

	QActionGroup* ActionSnapXYGroup = new QActionGroup(this);
	for (int ActionIdx = SW_EDIT_SNAP_MOVE_XY0; ActionIdx <= SW_EDIT_SNAP_MOVE_XY9; ActionIdx++)
	{
		mActions[ActionIdx]->setCheckable(true);
		ActionSnapXYGroup->addAction(mActions[ActionIdx]);
	}

	QActionGroup* ActionSnapZGroup = new QActionGroup(this);
	for (int ActionIdx = SW_EDIT_SNAP_MOVE_Z0; ActionIdx <= SW_EDIT_SNAP_MOVE_Z9; ActionIdx++)
	{
		mActions[ActionIdx]->setCheckable(true);
		ActionSnapZGroup->addAction(mActions[ActionIdx]);
	}

	QActionGroup* ActionSnapAngleGroup = new QActionGroup(this);
	for (int ActionIdx = SW_EDIT_SNAP_ANGLE0; ActionIdx <= SW_EDIT_SNAP_ANGLE_CUSTOM; ActionIdx++)
	{
		mActions[ActionIdx]->setCheckable(true);
		ActionSnapAngleGroup->addAction(mActions[ActionIdx]);
	}

	mActions[SW_EDIT_SNAP_ORBIT_TOGGLE]->setCheckable(true);

	QActionGroup* ActionTransformTypeGroup = new QActionGroup(this);
	for (int ActionIdx = SW_EDIT_TRANSFORM_ABSOLUTE_TRANSLATION; ActionIdx <= SW_EDIT_TRANSFORM_RELATIVE_ROTATION; ActionIdx++)
	{
		mActions[ActionIdx]->setCheckable(true);
		ActionTransformTypeGroup->addAction(mActions[ActionIdx]);
	}

	QActionGroup* ActionToolGroup = new QActionGroup(this);
	for (int ActionIdx = SW_EDIT_ACTION_FIRST; ActionIdx <= SW_EDIT_ACTION_LAST; ActionIdx++)
	{
		mActions[ActionIdx]->setCheckable(true);
		ActionToolGroup->addAction(mActions[ActionIdx]);
	}

	QActionGroup* ActionCameraGroup = new QActionGroup(this);
	ActionCameraGroup->addAction(mActions[SW_VIEW_CAMERA_NONE]);
	for (int ActionIdx = SW_VIEW_CAMERA_FIRST; ActionIdx <= SW_VIEW_CAMERA_LAST; ActionIdx++)
	{
		mActions[ActionIdx]->setCheckable(true);
		ActionCameraGroup->addAction(mActions[ActionIdx]);
	}

	QActionGroup* ActionPerspectiveGroup = new QActionGroup(this);
	for (int ActionIdx = SW_VIEW_PROJECTION_FIRST; ActionIdx <= SW_VIEW_PROJECTION_LAST; ActionIdx++)
	{
		mActions[ActionIdx]->setCheckable(true);
		ActionPerspectiveGroup->addAction(mActions[ActionIdx]);
	}

	QActionGroup* ActionShadingGroup = new QActionGroup(this);
	for (int ActionIdx = SW_VIEW_SHADING_FIRST; ActionIdx <= SW_VIEW_SHADING_LAST; ActionIdx++)
	{
		mActions[ActionIdx]->setCheckable(true);
		ActionShadingGroup->addAction(mActions[ActionIdx]);
	}

	QActionGroup* ActionLightingGroup = new QActionGroup(this);
	for (int ActionIdx = SW_VIEW_LIGHTING_FIRST; ActionIdx <= SW_VIEW_LIGHTING_LAST; ActionIdx++)
	{
		mActions[ActionIdx]->setCheckable(true);
		ActionLightingGroup->addAction(mActions[ActionIdx]);
	}

	QActionGroup* SelectionModeGroup = new QActionGroup(this);
	for (int ActionIdx = SW_EDIT_SELECTION_MODE_FIRST; ActionIdx <= SW_EDIT_SELECTION_MODE_LAST; ActionIdx++)
	{
		mActions[ActionIdx]->setCheckable(true);
		SelectionModeGroup->addAction(mActions[ActionIdx]);
	}

	QActionGroup* ModelGroup = new QActionGroup(this);
	for (int ActionIdx = SW_MODEL_FIRST; ActionIdx <= SW_MODEL_LAST; ActionIdx++)
	{
		mActions[ActionIdx]->setCheckable(true);
		ModelGroup->addAction(mActions[ActionIdx]);
	}

	UpdateShortcuts();
}

void swMainWindow::CreateMenus()
{
	QMenu* TransformMenu = new QMenu(tr("Transform"), this);
	TransformMenu->addAction(mActions[SW_EDIT_TRANSFORM_RELATIVE_TRANSLATION]);
	TransformMenu->addAction(mActions[SW_EDIT_TRANSFORM_ABSOLUTE_TRANSLATION]);
	TransformMenu->addAction(mActions[SW_EDIT_TRANSFORM_RELATIVE_ROTATION]);
	TransformMenu->addAction(mActions[SW_EDIT_TRANSFORM_ABSOLUTE_ROTATION]);
	mActions[SW_EDIT_TRANSFORM]->setMenu(TransformMenu);

	mCameraMenu = new QMenu(tr("C&ameras"), this);
	mCameraMenu->addAction(mActions[SW_VIEW_CAMERA_NONE]);

	for (int actionIdx = SW_VIEW_CAMERA_FIRST; actionIdx <= SW_VIEW_CAMERA_LAST; actionIdx++)
		mCameraMenu->addAction(mActions[actionIdx]);

	connect(mCameraMenu, &QMenu::aboutToShow, this, &swMainWindow::CameraMenuAboutToShow);

	mViewpointMenu = new QMenu(tr("&Viewpoints"), this);
	mViewpointMenu->addAction(mActions[SW_VIEW_VIEWPOINT_FRONT]);
	mViewpointMenu->addAction(mActions[SW_VIEW_VIEWPOINT_BACK]);
	mViewpointMenu->addAction(mActions[SW_VIEW_VIEWPOINT_LEFT]);
	mViewpointMenu->addAction(mActions[SW_VIEW_VIEWPOINT_RIGHT]);
	mViewpointMenu->addAction(mActions[SW_VIEW_VIEWPOINT_TOP]);
	mViewpointMenu->addAction(mActions[SW_VIEW_VIEWPOINT_BOTTOM]);
	mViewpointMenu->addAction(mActions[SW_VIEW_VIEWPOINT_HOME]);
	mViewpointMenu->addSeparator();
	mViewpointMenu->addAction(mActions[SW_VIEW_STEP_VIEW_EDIT]);
	mViewpointMenu->addAction(mActions[SW_VIEW_STEP_VIEW_TOGGLE_FOLLOW]);
	mViewpointMenu->addSeparator();
	mViewpointMenu->addAction(mActions[SW_VIEW_NORMAL_TO_FACE]);

	mProjectionMenu = new QMenu(tr("Projection"), this);
	mProjectionMenu->addAction(mActions[SW_VIEW_PROJECTION_PERSPECTIVE]);
	mProjectionMenu->addAction(mActions[SW_VIEW_PROJECTION_ORTHOGRAPHIC]);

	connect(mProjectionMenu, &QMenu::aboutToShow, this, &swMainWindow::ProjectionMenuAboutToShow);

	mShadingMenu = new QMenu(tr("Sh&ading"), this);
	mShadingMenu->addAction(mActions[SW_VIEW_SHADING_WIREFRAME]);
	mShadingMenu->addAction(mActions[SW_VIEW_SHADING_FLAT]);
	mShadingMenu->addAction(mActions[SW_VIEW_SHADING_DEFAULT_LIGHTS]);
	mShadingMenu->addSeparator();

	for (int ActionIdx = SW_VIEW_LIGHTING_FIRST; ActionIdx <= SW_VIEW_LIGHTING_LAST; ActionIdx++)
		mShadingMenu->addAction(mActions[ActionIdx]);

	mToolsMenu = new QMenu(tr("Tools"), this);
	mToolsMenu->addAction(mActions[SW_EDIT_ACTION_INSERT]);
	mToolsMenu->addAction(mActions[SW_EDIT_ACTION_POINT_LIGHT]);
	mToolsMenu->addAction(mActions[SW_EDIT_ACTION_SPOTLIGHT]);
	mToolsMenu->addAction(mActions[SW_EDIT_ACTION_DIRECTIONAL_LIGHT]);
	mToolsMenu->addAction(mActions[SW_EDIT_ACTION_AREA_LIGHT]);
	mToolsMenu->addAction(mActions[SW_EDIT_ACTION_CAMERA]);
	mToolsMenu->addSeparator();
	mToolsMenu->addAction(mActions[SW_EDIT_ACTION_SELECT]);
	mToolsMenu->addAction(mActions[SW_EDIT_ACTION_MOVE]);
	mToolsMenu->addAction(mActions[SW_EDIT_ACTION_ROTATE]);
	mToolsMenu->addAction(mActions[SW_EDIT_ACTION_DELETE]);
	mToolsMenu->addAction(mActions[SW_EDIT_ACTION_PAINT]);
	mToolsMenu->addAction(mActions[SW_EDIT_ACTION_COLOR_PICKER]);
	mToolsMenu->addSeparator();
	mToolsMenu->addAction(mActions[SW_EDIT_ACTION_ZOOM]);
	mToolsMenu->addAction(mActions[SW_EDIT_ACTION_PAN]);
	mToolsMenu->addAction(mActions[SW_EDIT_ACTION_ROTATE_VIEW]);
	mToolsMenu->addAction(mActions[SW_EDIT_ACTION_ROLL]);
	mToolsMenu->addAction(mActions[SW_EDIT_ACTION_ZOOM_REGION]);

	QMenu* FileMenu = menuBar()->addMenu(tr("&File"));
	connect(FileMenu, &QMenu::aboutToShow, this, &swMainWindow::FileMenuAboutToShow);
	FileMenu->addAction(mActions[SW_FILE_NEW]);
	FileMenu->addAction(mActions[SW_FILE_OPEN]);
	FileMenu->addAction(mActions[SW_FILE_MERGE]);
	FileMenu->addAction(mActions[SW_FILE_RELOAD]);
	FileMenu->addSeparator();
	FileMenu->addAction(mActions[SW_FILE_SAVE]);
	FileMenu->addAction(mActions[SW_FILE_SAVE_AS]);
	FileMenu->addAction(mActions[SW_FILE_SAVE_IMAGE]);
	QMenu* ImportMenu = FileMenu->addMenu(tr("&Import"));
	ImportMenu->addAction(mActions[SW_FILE_IMPORT_LDD]);
	ImportMenu->addAction(mActions[SW_FILE_IMPORT_INVENTORY]);
	QMenu* ExportMenu = FileMenu->addMenu(tr("&Export"));
	ExportMenu->addAction(mActions[SW_FILE_EXPORT_3DS]);
	ExportMenu->addAction(mActions[SW_FILE_EXPORT_BRICKLINK]);
	ExportMenu->addAction(mActions[SW_FILE_EXPORT_COLLADA]);
	ExportMenu->addAction(mActions[SW_FILE_EXPORT_CSV]);
	ExportMenu->addAction(mActions[SW_FILE_EXPORT_HTML]);
	ExportMenu->addAction(mActions[SW_FILE_EXPORT_POVRAY]);
	ExportMenu->addAction(mActions[SW_FILE_EXPORT_WAVEFRONT]);
	FileMenu->addSeparator();
#ifndef SW_DISABLE_RENDER_DIALOG
	QMenu* RenderMenu = FileMenu->addMenu(tr("&Render"));
	RenderMenu->addAction(mActions[SW_FILE_RENDER_POVRAY]);
	RenderMenu->addAction(mActions[SW_FILE_RENDER_BLENDER]);
	RenderMenu->addAction(mActions[SW_FILE_RENDER_OPEN_IN_BLENDER]);
#endif
	FileMenu->addAction(mActions[SW_FILE_PRINT]);
	FileMenu->addAction(mActions[SW_FILE_PRINT_PREVIEW]);
	FileMenu->addSeparator();
	FileMenu->addAction(mActions[SW_FILE_RECENT1]);
	FileMenu->addAction(mActions[SW_FILE_RECENT2]);
	FileMenu->addAction(mActions[SW_FILE_RECENT3]);
	FileMenu->addAction(mActions[SW_FILE_RECENT4]);
	mActionFileRecentSeparator = FileMenu->addSeparator();
	FileMenu->addAction(mActions[SW_FILE_EXIT]);

	QMenu* EditMenu = menuBar()->addMenu(tr("&Edit"));
	EditMenu->addAction(mActions[SW_VIEW_PREFERENCES]);
	EditMenu->addAction(mActions[SW_FILE_INSTRUCTIONS]);
	EditMenu->addSeparator();
	EditMenu->addAction(mActions[SW_EDIT_UNDO]);
	EditMenu->addAction(mActions[SW_EDIT_REDO]);
	EditMenu->addSeparator();
	EditMenu->addAction(mActions[SW_EDIT_CUT]);
	EditMenu->addAction(mActions[SW_EDIT_COPY]);
	EditMenu->addAction(mActions[SW_EDIT_PASTE]);
	EditMenu->addAction(mActions[SW_EDIT_PASTE_STEPS]);
	EditMenu->addSeparator();
	EditMenu->addAction(mActions[SW_EDIT_FIND]);
	EditMenu->addAction(mActions[SW_EDIT_FIND_NEXT]);
	EditMenu->addAction(mActions[SW_EDIT_FIND_PREVIOUS]);
	EditMenu->addAction(mActions[SW_EDIT_REPLACE]);
	EditMenu->addAction(mActions[SW_EDIT_REPLACE_NEXT]);
	EditMenu->addSeparator();
	EditMenu->addAction(mActions[SW_EDIT_SELECT_ALL]);
	EditMenu->addAction(mActions[SW_EDIT_SELECT_NONE]);
	EditMenu->addAction(mActions[SW_EDIT_SELECT_INVERT]);
	EditMenu->addAction(mActions[SW_EDIT_SELECT_BY_NAME]);
	EditMenu->addMenu(mSelectionModeMenu);
	EditMenu->addSeparator();
	EditMenu->addMenu(mTransformMenu);
	EditMenu->addMenu(mToolsMenu);

	QMenu* ViewMenu = menuBar()->addMenu(tr("&View"));
	ViewMenu->addAction(mActions[SW_VIEW_ZOOM_EXTENTS]);
	ViewMenu->addAction(mActions[SW_VIEW_LOOK_AT]);
	ViewMenu->addMenu(mViewpointMenu);
	ViewMenu->addMenu(mCameraMenu);
	ViewMenu->addMenu(mProjectionMenu);
	ViewMenu->addMenu(mShadingMenu);
	QMenu* StepMenu = ViewMenu->addMenu(tr("Ste&p"));
	StepMenu->addAction(mActions[SW_VIEW_TIME_FIRST]);
	StepMenu->addAction(mActions[SW_VIEW_TIME_PREVIOUS]);
	StepMenu->addAction(mActions[SW_VIEW_TIME_NEXT]);
	StepMenu->addAction(mActions[SW_VIEW_TIME_LAST]);
	StepMenu->addSeparator();
	StepMenu->addAction(mActions[SW_VIEW_TIME_INSERT_BEFORE]);
	StepMenu->addAction(mActions[SW_VIEW_TIME_INSERT_AFTER]);
	StepMenu->addAction(mActions[SW_VIEW_TIME_DELETE]);
	ViewMenu->addSeparator();
	ViewMenu->addAction(mActions[SW_VIEW_SPLIT_HORIZONTAL]);
	ViewMenu->addAction(mActions[SW_VIEW_SPLIT_VERTICAL]);
	ViewMenu->addAction(mActions[SW_VIEW_REMOVE_VIEW]);
	ViewMenu->addAction(mActions[SW_VIEW_RESET_VIEWS]);
	ViewMenu->addSeparator();
	QMenu* ToolBarsMenu = ViewMenu->addMenu(tr("T&oolbars"));
	connect(ToolBarsMenu, &QMenu::aboutToShow, this, &swMainWindow::UpdateDockWidgetActions);
	ToolBarsMenu->addAction(mActions[SW_VIEW_TOOLBAR_PARTS]);
	ToolBarsMenu->addAction(mActions[SW_VIEW_TOOLBAR_COLORS]);
	ToolBarsMenu->addAction(mActions[SW_VIEW_TOOLBAR_PROPERTIES]);
	ToolBarsMenu->addAction(mActions[SW_VIEW_TOOLBAR_TIMELINE]);
	ToolBarsMenu->addAction(mActions[SW_VIEW_TOOLBAR_PREVIEW]);
	ToolBarsMenu->addAction(mActions[SW_VIEW_TOOLBAR_MINIFIGURE_BUILDER]);
	ToolBarsMenu->addAction(mActions[SW_VIEW_TOOLBAR_TERMINAL]);
	ToolBarsMenu->addSeparator();
	ToolBarsMenu->addAction(mActions[SW_VIEW_TOOLBAR_STANDARD]);
	ToolBarsMenu->addAction(mActions[SW_VIEW_TOOLBAR_TOOLS]);
	ToolBarsMenu->addAction(mActions[SW_VIEW_TOOLBAR_TIME]);
	ViewMenu->addAction(mActions[SW_VIEW_FULL_SCREEN]);

	QMenu* PieceMenu = menuBar()->addMenu(tr("&Piece"));
	PieceMenu->addAction(mActions[SW_PIECE_INSERT]);
	PieceMenu->addAction(mActions[SW_PIECE_TOGGLE_FIRST_AT_ORIGIN]);
	connect(PieceMenu, &QMenu::aboutToShow, this, [this]()
	{
		mActions[SW_PIECE_TOGGLE_FIRST_AT_ORIGIN]->setChecked(swGetPreferences().mFirstPieceAtOrigin);
	});
	PieceMenu->addAction(mActions[SW_PIECE_DELETE]);
	PieceMenu->addAction(mActions[SW_PIECE_DUPLICATE]);
	PieceMenu->addAction(mActions[SW_PIECE_PAINT_SELECTED]);
	PieceMenu->addAction(mActions[SW_PIECE_ARRAY]);
	PieceMenu->addAction(mActions[SW_PIECE_RESET_PIVOT_POINT]);
	PieceMenu->addAction(mActions[SW_PIECE_REMOVE_KEY_FRAMES]);
	PieceMenu->addSeparator();
	PieceMenu->addAction(mActions[SW_PIECE_EDIT_SELECTED_SUBMODEL]);
	PieceMenu->addAction(mActions[SW_PIECE_EDIT_END_SUBMODEL]);
	PieceMenu->addAction(mActions[SW_PIECE_VIEW_SELECTED_MODEL]);
	PieceMenu->addAction(mActions[SW_PIECE_INLINE_SELECTED_MODELS]);
	PieceMenu->addAction(mActions[SW_PIECE_MOVE_SELECTION_TO_MODEL]);
	PieceMenu->addSeparator();
	PieceMenu->addAction(mActions[SW_PIECE_GROUP]);
	PieceMenu->addAction(mActions[SW_PIECE_UNGROUP]);
	PieceMenu->addAction(mActions[SW_PIECE_GROUP_REMOVE]);
	PieceMenu->addAction(mActions[SW_PIECE_GROUP_ADD]);
	PieceMenu->addAction(mActions[SW_PIECE_GROUP_EDIT]);
	PieceMenu->addSeparator();
	PieceMenu->addAction(mActions[SW_PIECE_HIDE_SELECTED]);
	PieceMenu->addAction(mActions[SW_PIECE_HIDE_UNSELECTED]);
	PieceMenu->addAction(mActions[SW_PIECE_UNHIDE_SELECTED]);
	PieceMenu->addAction(mActions[SW_PIECE_UNHIDE_ALL]);

	QMenu* ModelMenu = menuBar()->addMenu(tr("&Models"));
	ModelMenu->addAction(mActions[SW_MODEL_PROPERTIES]);
	ModelMenu->addAction(mActions[SW_MODEL_NEW]);
	ModelMenu->addAction(mActions[SW_MODEL_LIST]);
	ModelMenu->addAction(mActions[SW_PIECE_MINIFIGURE_BUILDER]);
	ModelMenu->addSeparator();
	for (int ModelIdx = SW_MODEL_FIRST; ModelIdx <= SW_MODEL_LAST; ModelIdx++)
		ModelMenu->addAction(mActions[ModelIdx]);

	QMenu* HelpMenu = menuBar()->addMenu(tr("&Help"));
	HelpMenu->addAction(mActions[SW_HELP_WHATS_THIS]);
	HelpMenu->addAction(mActions[SW_HELP_HOME_PAGE]);
	HelpMenu->addAction(mActions[SW_HELP_BUG_REPORT]);
#ifndef Q_OS_MACOS
	HelpMenu->addSeparator();
#endif
	HelpMenu->addAction(mActions[SW_HELP_ABOUT]);

	// The menu bar menus show text only, the toolbars keep the icons.
	std::function<void(QMenu*)> HideMenuIcons = [&HideMenuIcons](QMenu* Menu)
	{
		for (QAction* Action : Menu->actions())
		{
			Action->setIconVisibleInMenu(false);

			if (Action->menu())
				HideMenuIcons(Action->menu());
		}
	};

	for (QAction* MenuAction : menuBar()->actions())
		if (MenuAction->menu())
			HideMenuIcons(MenuAction->menu());
}

void swMainWindow::CreateToolBars()
{
	mSelectionModeMenu = new QMenu(tr("Selection Mode"), this);
	for (int ModeIdx = SW_EDIT_SELECTION_MODE_FIRST; ModeIdx <= SW_EDIT_SELECTION_MODE_LAST; ModeIdx++)
		mSelectionModeMenu->addAction(mActions[ModeIdx]);

	QAction* SelectionModeAction = new QAction(tr("Selection Mode"), this);
	SelectionModeAction->setStatusTip(tr("Change selection mode"));
	SelectionModeAction->setToolTip(tr("Choose which related pieces are included when selecting a piece."));
	SelectionModeAction->setWhatsThis(tr("<p><b>Selection Mode</b></p><p>Choose the rule applied when you pick a piece in the model view. Single selects that piece; the other modes include matching piece types, matching colors, or both. For example, color selection lets you pick matching red pieces without selecting each one separately.</p>"));
	SelectionModeAction->setIcon(swCreateInterfaceIcon("toolbars/tools", "action_select"));
	SelectionModeAction->setMenu(mSelectionModeMenu);

	mTransformMenu = new QMenu(tr("Transform"), this);
	mTransformMenu->addAction(mActions[SW_EDIT_TRANSFORM_RELATIVE]);
	mTransformMenu->addAction(mActions[SW_EDIT_TRANSFORM_ABSOLUTE]);
	mTransformMenu->addSeparator();
	mTransformMenu->addAction(mActions[SW_EDIT_TRANSFORM_TOGETHER]);
	mTransformMenu->addAction(mActions[SW_EDIT_TRANSFORM_SEPARATELY]);

	QAction* TransformAction = new QAction(tr("Transform"), this);
	TransformAction->setStatusTip(tr("Transform Options"));
	TransformAction->setToolTip(tr("Choose coordinate axes and whether selected pieces rotate together."));
	TransformAction->setWhatsThis(tr("<p><b>Transform Options</b></p><p>Choose the axes and pivot used for interactive transforms. Relative Transforms uses the focused object's orientation, so movement along X follows that object's local X axis. Absolute Transforms uses the model axes. Rotate Together rotates the selection around a shared pivot; Rotate Separately rotates each piece around its own origin.</p>"));
	TransformAction->setIcon(swCreateInterfaceIcon("toolbars/standard", "transform_translation_relative"));
	TransformAction->setMenu(mTransformMenu);

	QMenu* SnapXYMenu = new QMenu(tr("Snap XY"), this);
	for (int actionIdx = SW_EDIT_SNAP_MOVE_XY0; actionIdx <= SW_EDIT_SNAP_MOVE_XY9; actionIdx++)
		SnapXYMenu->addAction(mActions[actionIdx]);

	QMenu* SnapZMenu = new QMenu(tr("Snap Z"), this);
	for (int actionIdx = SW_EDIT_SNAP_MOVE_Z0; actionIdx <= SW_EDIT_SNAP_MOVE_Z9; actionIdx++)
		SnapZMenu->addAction(mActions[actionIdx]);

	QMenu* SnapMenu = new QMenu(tr("Snap Menu"), this);
	SnapMenu->addAction(mActions[SW_EDIT_SNAP_MOVE_TOGGLE]);
	SnapMenu->addSeparator();
	SnapMenu->addMenu(SnapXYMenu);
	SnapMenu->addMenu(SnapZMenu);

	QAction* MoveAction = new QAction(tr("Movement Snap"), this);
	MoveAction->setStatusTip(tr("Snap translations to fixed intervals"));
	MoveAction->setToolTip(tr("Choose movement snapping intervals for XY and Z."));
	MoveAction->setWhatsThis(tr("<p><b>Movement Snap</b></p><p>Choose how far objects move in each snapped increment. XY controls movement across the base plane; Z controls vertical movement, so they can use different intervals. Turn snapping off for free placement, then enable it again when you need consistent spacing.</p>"));
	MoveAction->setIcon(swCreateInterfaceIcon("toolbars/standard", "snap_move"));
	MoveAction->setMenu(SnapMenu);

	QMenu* SnapAngleMenu = new QMenu(tr("Snap Angle Menu"), this);
	SnapAngleMenu->addAction(mActions[SW_EDIT_SNAP_ANGLE_TOGGLE]);
	SnapAngleMenu->addSeparator();
	for (int actionIdx = SW_EDIT_SNAP_ANGLE0; actionIdx <= SW_EDIT_SNAP_ANGLE_CUSTOM; actionIdx++)
		SnapAngleMenu->addAction(mActions[actionIdx]);
	SnapAngleMenu->addSeparator();
	SnapAngleMenu->addAction(mActions[SW_EDIT_SNAP_ORBIT_TOGGLE]);

	QAction* AngleAction = new QAction(tr("Rotation Snap"), this);
	AngleAction->setStatusTip(tr("Snap rotations to fixed intervals"));
	AngleAction->setToolTip(tr("Choose the rotation snapping interval."));
	AngleAction->setWhatsThis(tr("<p><b>Rotation Snap</b></p><p>Choose an angle increment for interactive rotation. For example, a 90-degree increment is useful for quarter turns, while smaller increments allow finer alignment. Turn snapping off for free rotation without fixed angle increments.</p>"));
	AngleAction->setIcon(swCreateInterfaceIcon("toolbars/standard", "snap_angle"));
	AngleAction->setMenu(SnapAngleMenu);

	mStandardToolBar = addToolBar(tr("Standard"));
	mStandardToolBar->setObjectName("StandardToolbar");
	mStandardToolBar->setWhatsThis(tr("<p><b>Standard Toolbar</b></p><p>Use file controls to create, open, and save models, and Undo or Redo to navigate edit history. Before moving or rotating objects, choose the selection rule, coordinate reference, and snapping increments from the four drop-down controls. Help > What's This? explains individual controls.</p>"));
	mStandardToolBar->addAction(mActions[SW_FILE_NEW]);
	mStandardToolBar->addAction(mActions[SW_FILE_OPEN]);
	mStandardToolBar->addAction(mActions[SW_FILE_SAVE]);
	mStandardToolBar->addSeparator();
	mStandardToolBar->addAction(mActions[SW_EDIT_UNDO]);
	mStandardToolBar->addAction(mActions[SW_EDIT_REDO]);
	mStandardToolBar->addSeparator();
	mStandardToolBar->addAction(SelectionModeAction);
	mStandardToolBar->addAction(TransformAction);
	mStandardToolBar->addAction(MoveAction);
	mStandardToolBar->addAction(AngleAction);

	((QToolButton*)mStandardToolBar->widgetForAction(SelectionModeAction))->setPopupMode(QToolButton::InstantPopup);
	((QToolButton*)mStandardToolBar->widgetForAction(TransformAction))->setPopupMode(QToolButton::InstantPopup);
	((QToolButton*)mStandardToolBar->widgetForAction(MoveAction))->setPopupMode(QToolButton::InstantPopup);
	((QToolButton*)mStandardToolBar->widgetForAction(AngleAction))->setPopupMode(QToolButton::InstantPopup);

	mTimeToolBar = addToolBar(tr("Time"));
	mTimeToolBar->setObjectName("TimeToolbar");
	mTimeToolBar->setWhatsThis(tr("<p><b>Time Toolbar</b></p><p>The arrow buttons move through building steps so you can inspect how the model is assembled. They do not change the step assigned to a piece; use the Timeline for that. Add Keys controls whether edits create animation keys at the current step, allowing properties such as position to vary between steps.</p>"));
	mTimeToolBar->addAction(mActions[SW_VIEW_TIME_FIRST]);
	mTimeToolBar->addAction(mActions[SW_VIEW_TIME_PREVIOUS]);
	mTimeToolBar->addAction(mActions[SW_VIEW_TIME_NEXT]);
	mTimeToolBar->addAction(mActions[SW_VIEW_TIME_LAST]);
	mTimeToolBar->addAction(mActions[SW_VIEW_TIME_ADD_KEYS]);

	mToolsToolBar = addToolBar(tr("Tools"));
	mToolsToolBar->setObjectName("ToolsToolbar");
	mToolsToolBar->setWhatsThis(tr("<p><b>Tools Toolbar</b></p><p>Choose a tool, then use it in the model view. Insertion tools create pieces, lights, or cameras. Editing tools select, transform, remove, or recolor objects. Navigation tools change the viewpoint without changing model geometry. Mouse assignments can be customized in Preferences.</p>"));
	insertToolBarBreak(mToolsToolBar);
	mToolsToolBar->addAction(mActions[SW_EDIT_ACTION_INSERT]);
	mToolsToolBar->addAction(mActions[SW_EDIT_ACTION_POINT_LIGHT]);
	mToolsToolBar->addAction(mActions[SW_EDIT_ACTION_SPOTLIGHT]);
	mToolsToolBar->addAction(mActions[SW_EDIT_ACTION_DIRECTIONAL_LIGHT]);
	mToolsToolBar->addAction(mActions[SW_EDIT_ACTION_AREA_LIGHT]);
	mToolsToolBar->addAction(mActions[SW_EDIT_ACTION_CAMERA]);
	mToolsToolBar->addSeparator();
	mToolsToolBar->addAction(mActions[SW_EDIT_ACTION_SELECT]);
	mToolsToolBar->addAction(mActions[SW_EDIT_ACTION_MOVE]);
	mToolsToolBar->addAction(mActions[SW_EDIT_ACTION_ROTATE]);
	mToolsToolBar->addAction(mActions[SW_EDIT_ACTION_DELETE]);
	mToolsToolBar->addAction(mActions[SW_EDIT_ACTION_PAINT]);
	mToolsToolBar->addAction(mActions[SW_EDIT_ACTION_COLOR_PICKER]);
	mToolsToolBar->addSeparator();
	mToolsToolBar->addAction(mActions[SW_EDIT_ACTION_ZOOM]);
	mToolsToolBar->addAction(mActions[SW_EDIT_ACTION_PAN]);
	mToolsToolBar->addAction(mActions[SW_EDIT_ACTION_ROTATE_VIEW]);
	mToolsToolBar->addAction(mActions[SW_EDIT_ACTION_ROLL]);
	mToolsToolBar->addAction(mActions[SW_EDIT_ACTION_ZOOM_REGION]);
	mToolsToolBar->hide();

	for (QToolBar* ToolBar : { mStandardToolBar, mTimeToolBar, mToolsToolBar })
	{
		swSetupWidgetHelp(ToolBar, nullptr);

		for (QAction* Action : ToolBar->actions())
		{
			if (Action->isSeparator())
				continue;

			if (QWidget* Button = ToolBar->widgetForAction(Action))
			{
				Button->setWhatsThis(Action->whatsThis());
				Button->setAccessibleName(Action->text().remove('&'));
			}
		}
	}


	mPartsToolBar = new QDockWidget(tr("Pieces"), this);
	mPartsToolBar->installEventFilter(this);
	mPartsToolBar->setObjectName("PartsToolbar");
	mPartsToolBar->setWhatsThis(tr("<p><b>Pieces</b></p><p>Choose a category or palette and filter its pieces. Selecting a piece prepares it for insertion; drag it into the model view to place it. Double-click a piece to open its preview. Options controls filtering, thumbnails, and list display.</p>"));
	swSetupWidgetHelp(mPartsToolBar, nullptr);
	mPartSelectionWidget = new swPartSelectionWidget(false, mPartsToolBar);
	mPartsToolBar->setWidget(mPartSelectionWidget);
	addDockWidget(Qt::RightDockWidgetArea, mPartsToolBar);

	connect(mPartsToolBar, &QDockWidget::dockLocationChanged, mPartSelectionWidget, &swPartSelectionWidget::DockLocationChanged);
	connect(mPartSelectionWidget, &swPartSelectionWidget::PartPicked, this, &swMainWindow::PartListPicked);
	connect(mPartSelectionWidget, &swPartSelectionWidget::CurrentPartChanged, this, &swMainWindow::SetInsertPieceData);

	mColorsToolBar = new QDockWidget(tr("Colors"), this);
	mColorsToolBar->setObjectName("ColorsToolbar");
	mColorsToolBar->setWhatsThis(tr("<p><b>Colors</b></p><p>Choose the current LDraw color from the swatches. This color is used when inserting or painting pieces. To recolor selected pieces, use the color button above the swatches. Hover over a swatch for its name and color code.</p>"));
	swSetupWidgetHelp(mColorsToolBar, nullptr);
	mColorsToolBar->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);

	mColorList = new swColorList();
	connect(mColorList, &swColorList::ColorChanged, this, &swMainWindow::ColorChanged);

	QWidget* ColorWidget = new QWidget(mColorsToolBar);

	QVBoxLayout* ColorLayout = new QVBoxLayout(ColorWidget);

	QWidget* ColorRow = swCreatePanelRow(ColorWidget);
	ColorLayout->addWidget(ColorRow);

	mColorButton = new QToolButton(ColorWidget);
	mColorButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	mColorButton->setToolTip(tr("Apply the current color to selected pieces."));
	mColorButton->setAccessibleName(tr("Change Selection Color"));
	mColorButton->setWhatsThis(tr("<p><b>Change Selection Color</b></p><p>Apply the current color to selected pieces. Choosing a swatch sets the current color; this button applies that color to the selection.</p>"));
	swAddPanelRowWidget(ColorRow, mColorButton);
	swSetupPanelButton(mColorButton);

	// Nothing else sits in the header, so the button reaches up to the options button.
	mColorButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

	connect(mColorButton, &QToolButton::clicked, this, &swMainWindow::ColorButtonClicked);

	QMenu* ColorOptionsMenu = new QMenu(ColorRow);
	swAddPanelOptionsButton(ColorRow, ColorOptionsMenu, tr("Choose how inserted pieces are colored."), tr("Color Options"));
	connect(ColorOptionsMenu, &QMenu::aboutToShow, this, &swMainWindow::ColorOptionsMenuAboutToShow);

	ColorLayout->addWidget(mColorList);
	mColorList->setAccessibleName(tr("LDraw Colors"));

	// The panel is always exactly as tall as the color button and the tiles.
	connect(mColorList, &swColorList::HeightChanged, this, [this, ColorWidget, ColorLayout]()
	{
		// Recompute from the current rows, then let the dock follow so it grows and shrinks with the tiles.
		ColorLayout->invalidate();
		const int Height = ColorLayout->sizeHint().height();
		const int TitleHeight = mColorsToolBar->height() - ColorWidget->height();

		ColorWidget->setFixedHeight(Height);

		if (!mColorsToolBar->isFloating() && mColorsToolBar->isVisible())
			resizeDocks({ mColorsToolBar }, { Height + TitleHeight }, Qt::Vertical);
	});

	mColorsToolBar->setWidget(ColorWidget);
	addDockWidget(Qt::RightDockWidgetArea, mColorsToolBar);

	mPropertiesToolBar = new QDockWidget(tr("Properties"), this);
	mPropertiesToolBar->setObjectName("PropertiesToolbar");
	mPropertiesToolBar->setWhatsThis(tr("<p><b>Properties</b></p><p>Edit properties of the focused object or selection. Available fields depend on whether you select pieces, a camera, or a light. Expand a section to see its fields. Keyframe controls mark properties at the current building step.</p><p>The transform fields at the bottom apply an X/Y/Z translation or rotation using the chosen transform mode. Press Enter to apply the values.</p>"));
	swSetupWidgetHelp(mPropertiesToolBar, nullptr);

	QWidget* PropertiesWidget = new QWidget(mPropertiesToolBar);
	QVBoxLayout* PropertiesLayout = new QVBoxLayout(PropertiesWidget);

	// The rows naming the object come first, then the other values, then the position and rotation rows.
	// The other values scroll only when the panel cannot get the height of all its rows, without a frame so they line up with the rows around them.
	QScrollArea* PropertiesScrollArea = new QScrollArea(PropertiesWidget);
	PropertiesScrollArea->setFrameShape(QFrame::NoFrame);
	PropertiesScrollArea->setWidgetResizable(true);
	PropertiesLayout->addWidget(PropertiesScrollArea);

	mPropertiesWidget = new swPropertiesWidget(PropertiesScrollArea);
	PropertiesScrollArea->setWidget(mPropertiesWidget);
	PropertiesLayout->insertWidget(0, mPropertiesWidget->GetIdentityRowsWidget());

	QWidget* TransformWidget = swCreatePanelRow(PropertiesWidget);

	// A T names the row like the P and R buttons of the position and rotation rows.
	QToolButton* TransformNameButton = new QToolButton(TransformWidget);
	TransformNameButton->setText(tr("T"));
	TransformNameButton->setToolTip(tr("Transform"));
	TransformNameButton->setAccessibleName(tr("Transform"));
	TransformNameButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
	swAddPanelRowWidget(TransformWidget, TransformNameButton);
	swSetupPanelButton(TransformNameButton);
	TransformNameButton->setFixedSize(swGetPanelButtonSize());

	// Transform values use the same spin boxes as the position and rotation rows.
	mTransformXEdit = new swDoubleSpinBox(TransformWidget);
	mTransformXEdit->setRange(-FLT_MAX, FLT_MAX);
	mTransformXEdit->setAccessibleName(tr("Transform X"));
	mTransformXEdit->setToolTip(tr("Enter the X translation in LDraw units or rotation in degrees."));
	swSetupPanelSpinBox(mTransformXEdit);
	swAddPanelRowWidget(TransformWidget, mTransformXEdit);
	mTransformYEdit = new swDoubleSpinBox(TransformWidget);
	mTransformYEdit->setRange(-FLT_MAX, FLT_MAX);
	mTransformYEdit->setAccessibleName(tr("Transform Y"));
	mTransformYEdit->setToolTip(tr("Enter the Y translation in LDraw units or rotation in degrees."));
	swSetupPanelSpinBox(mTransformYEdit);
	swAddPanelRowWidget(TransformWidget, mTransformYEdit);
	mTransformZEdit = new swDoubleSpinBox(TransformWidget);
	mTransformZEdit->setRange(-FLT_MAX, FLT_MAX);
	mTransformZEdit->setAccessibleName(tr("Transform Z"));
	mTransformZEdit->setToolTip(tr("Enter the Z translation in LDraw units or rotation in degrees."));
	swSetupPanelSpinBox(mTransformZEdit);
	swAddPanelRowWidget(TransformWidget, mTransformZEdit);

	// The transform mode button sits under the keyframe buttons and shows the current mode.
	QToolButton* TransformButton = new QToolButton(TransformWidget);
	TransformButton->setDefaultAction(mActions[SW_EDIT_TRANSFORM]);
	TransformButton->setPopupMode(QToolButton::InstantPopup);
	TransformButton->setToolTip(tr("Choose absolute or relative translation or rotation."));
	TransformButton->setWhatsThis(tr("<p><b>Apply Transform</b></p><p>Choose the translation or rotation mode, enter X/Y/Z values, and press Enter to apply them. Translation values use LDraw units; rotation values use degrees.</p>"));
	swAddPanelRowWidget(TransformWidget, TransformButton);
	swSetupPanelButton(TransformButton);
	TransformButton->setFixedSize(swGetPanelButtonSize());

	PropertiesLayout->addWidget(mPropertiesWidget->GetTransformRowsWidget());
	PropertiesLayout->addWidget(TransformWidget);

	// The panel is as tall as the rows it shows, and shrinks with a scrolling grid when the panels around it need the room.
	auto UpdatePropertiesHeight = [this, PropertiesWidget, PropertiesLayout, PropertiesScrollArea, TransformWidget]()
	{
		// The scrolling rows are hidden when all shown properties are in the fixed rows.
		const bool ShowList = mPropertiesWidget->HasVisibleRows();
		const int ListHeight = ShowList ? mPropertiesWidget->sizeHint().height() : 0;
		const int MinimumListHeight = qMin(ListHeight, swGetPanelButtonSize().height());

		// The full height adds the layout margins and the standard spacing between the shown rows, measured
		// without growing any widget so the window never grows on the way.
		const QMargins Margins = PropertiesLayout->contentsMargins();
		const int Spacing = swGetLayoutSpacing(PropertiesWidget, Qt::Vertical);
		int PanelHeight = Margins.top() + Margins.bottom() + TransformWidget->sizeHint().height();

		for (const QWidget* Rows : { mPropertiesWidget->GetIdentityRowsWidget(), mPropertiesWidget->GetTransformRowsWidget() })
			if (!Rows->isHidden())
				PanelHeight += Rows->sizeHint().height() + Spacing;

		if (ShowList)
			PanelHeight += ListHeight + Spacing;

		PropertiesScrollArea->setVisible(ShowList);
		PropertiesScrollArea->setMinimumHeight(MinimumListHeight);
		PropertiesScrollArea->setMaximumHeight(ListHeight);
		PropertiesWidget->setMinimumHeight(PanelHeight - ListHeight + MinimumListHeight);
		PropertiesWidget->setMaximumHeight(PanelHeight);

		// A docked panel keeps its height when its rows change, so ask for all of it, the panels around it give up what they can spare.
		if (!mPropertiesToolBar->isFloating() && mPropertiesToolBar->isVisible())
			resizeDocks({ mPropertiesToolBar }, { PanelHeight + mPropertiesToolBar->height() - PropertiesWidget->height() }, Qt::Vertical);
	};

	connect(mPropertiesWidget, &swPropertiesWidget::LayoutChanged, this, [UpdatePropertiesHeight]()
	{
		QTimer::singleShot(0, UpdatePropertiesHeight);
	});

	connect(mTransformXEdit, &swDoubleSpinBox::ReturnKeyPressed, mActions[SW_EDIT_TRANSFORM], &QAction::trigger);
	connect(mTransformYEdit, &swDoubleSpinBox::ReturnKeyPressed, mActions[SW_EDIT_TRANSFORM], &QAction::trigger);
	connect(mTransformZEdit, &swDoubleSpinBox::ReturnKeyPressed, mActions[SW_EDIT_TRANSFORM], &QAction::trigger);

	mPropertiesToolBar->setWidget(PropertiesWidget);
	addDockWidget(Qt::RightDockWidgetArea, mPropertiesToolBar);

	mTimelineToolBar = new QDockWidget(tr("Timeline"), this);
	mTimelineToolBar->setObjectName("TimelineToolbar");
	mTimelineToolBar->setWhatsThis(tr("<p><b>Timeline</b></p><p>Select a step to make it current, or select piece rows to select those pieces in the model. Drag pieces between steps or reorder them within a step. Right-click for step insertion, removal, movement, and visibility commands.</p>"));
	swSetupWidgetHelp(mTimelineToolBar, nullptr);
	mTimelineToolBar->setAcceptDrops(true);

	mTimelineWidget = new swTimelineWidget(mTimelineToolBar);

	// The steps of the selected piece are edited below the timeline.
	QWidget* TimelinePanel = new QWidget(mTimelineToolBar);
	QVBoxLayout* TimelineLayout = new QVBoxLayout(TimelinePanel);
	TimelineLayout->addWidget(mTimelineWidget);
	TimelineLayout->addWidget(mPropertiesWidget->GetStepRowsWidget());

	mTimelineToolBar->setWidget(TimelinePanel);
	addDockWidget(Qt::RightDockWidgetArea, mTimelineToolBar);

	CreatePreviewWidget();
	CreateTerminalWidget();

	// the minifigure builder panel is filled when first shown, once the pieces library is loaded
	mMinifigureBuilderToolBar = new QDockWidget(tr("Minifigure Builder"), this);
	mMinifigureBuilderToolBar->setObjectName("MinifigToolbar");
	mMinifigureBuilderToolBar->setWhatsThis(tr("<p><b>Minifigure Builder</b></p><p>Choose the pieces, colors and pose of a minifigure, then add it to the current model. The panel keeps its settings so you can add several variations.</p>"));
	swSetupWidgetHelp(mMinifigureBuilderToolBar, nullptr);
	addDockWidget(Qt::RightDockWidgetArea, mMinifigureBuilderToolBar);
	mMinifigureBuilderToolBar->hide();

	connect(mMinifigureBuilderToolBar, &QDockWidget::visibilityChanged, this, [this](bool Visible)
	{
		if (Visible && !mMinifigureBuilderToolBar->widget())
		{
			// The panel is exactly as tall as the editor wherever it is docked.
			swMinifigureBuilderWidget* MinifigureBuilderWidget = new swMinifigureBuilderWidget(mMinifigureBuilderToolBar);
			swFitHeightToContents(MinifigureBuilderWidget);
			mMinifigureBuilderToolBar->setWidget(MinifigureBuilderWidget);
		}
	});

	tabifyDockWidget(mPartsToolBar, mPropertiesToolBar);
	tabifyDockWidget(mPropertiesToolBar, mTimelineToolBar);
	tabifyDockWidget(mTimelineToolBar, mPreviewToolBar);
	tabifyDockWidget(mPreviewToolBar, mMinifigureBuilderToolBar);

	connect(mPropertiesToolBar, &QDockWidget::topLevelChanged, this, &swMainWindow::EnableWindowFlags);
	connect(mTimelineToolBar, &QDockWidget::topLevelChanged, this, &swMainWindow::EnableWindowFlags);
	connect(mPartsToolBar, &QDockWidget::topLevelChanged, this, &swMainWindow::EnableWindowFlags);
	connect(mColorsToolBar, &QDockWidget::topLevelChanged, this, &swMainWindow::EnableWindowFlags);
	connect(mMinifigureBuilderToolBar, &QDockWidget::topLevelChanged, this, &swMainWindow::EnableWindowFlags);

	mPartsToolBar->raise();
}

swView* swMainWindow::CreateView(swModel* Model)
{
	swView* NewView = new swView(swViewType::View, Model);

	connect(NewView, &swView::FocusReceived, this, &swMainWindow::ViewFocusReceived);

	AddView(NewView);

	return NewView;
}

void swMainWindow::PreviewPiece(swPieceData* PieceData, int ColorCode, bool ShowPreview)
{
	if (ShowPreview)
		mPreviewToolBar->show();

	mPreviewWidget->SetCurrentPiece(PieceData, ColorCode);
}

void swMainWindow::CreateTerminalWidget()
{
#ifdef SW_ENABLE_TERMINAL
	mTerminalToolBar = new QDockWidget(tr("Terminal"), this);
	mTerminalToolBar->setObjectName("TerminalToolBar");
	mTerminalToolBar->setWhatsThis(tr("<p><b>Terminal</b></p><p>An embedded Konsole terminal that starts in the folder of the current model.</p>"));
	swSetupWidgetHelp(mTerminalToolBar, nullptr);
	mTerminalToolBar->setWidget(new swTerminalWidget(mTerminalToolBar));
	addDockWidget(Qt::BottomDockWidgetArea, mTerminalToolBar);
	mTerminalToolBar->hide();

	connect(mTerminalToolBar, &QDockWidget::topLevelChanged, this, &swMainWindow::EnableWindowFlags);
#else
	// Without KDE Frameworks there is no terminal to show.
	mActions[SW_VIEW_TOOLBAR_TERMINAL]->setVisible(false);
	mActions[SW_VIEW_TERMINAL_TOGGLE_FOLLOW_MODEL]->setVisible(false);
	mActions[SW_VIEW_TERMINAL_GO_TO_MODEL_FOLDER]->setVisible(false);
	mActions[SW_VIEW_TERMINAL_OPEN_MODEL_FOLDER]->setVisible(false);
#endif
}

void swMainWindow::UpdateTerminalDirectory(bool Force)
{
#ifdef SW_ENABLE_TERMINAL
	const QString FileName = swGetActiveProject()->GetFileName();

	if (mTerminalToolBar && !FileName.isEmpty())
		static_cast<swTerminalWidget*>(mTerminalToolBar->widget())->SetDirectory(QFileInfo(FileName).absolutePath(), Force);
#else
	Q_UNUSED(Force);
#endif
}

void swMainWindow::SnapPartsDock()
{
	if (!swGetPreferences().mPiecesSnapToTiles || !mPartsToolBar || mPartsToolBar->isFloating() || !mPartSelectionWidget)
		return;

	const Qt::DockWidgetArea Area = dockWidgetArea(mPartsToolBar);

	if (Area != Qt::LeftDockWidgetArea && Area != Qt::RightDockWidgetArea)
	{
		mPartSelectionWidget->SnapCategoriesWidth();
		return;
	}

	const int Delta = mPartSelectionWidget->GetTileSnapDelta();

	if (!Delta)
		return;

	resizeDocks({ mPartsToolBar }, { mPartsToolBar->width() + Delta }, Qt::Horizontal);

	// Check once more after the tiles are laid out again in case the view wraps differently than measured.
	if (!mPartsSnapRetried)
	{
		mPartsSnapRetried = true;

		QTimer::singleShot(0, this, [this]()
		{
			SnapPartsDock();
			mPartsSnapRetried = false;
		});
	}
}

void swMainWindow::CreatePreviewWidget()
{
	mPreviewWidget  = new swPreviewDockWidget(this);

	mPreviewToolBar = new QDockWidget(tr("Preview"), this);
	mPreviewToolBar->setWindowTitle(tr("Preview"));
	mPreviewToolBar->setObjectName("PreviewToolBar");
	mPreviewToolBar->setWhatsThis(tr("<p><b>Preview</b></p><p>Inspect a piece without placing it in the model. Double-click a piece in the Pieces dock to preview it. Lock Preview keeps the current preview when the source selection changes; unlock it to allow updates.</p>"));
	swSetupWidgetHelp(mPreviewToolBar, nullptr);
	mPreviewToolBar->setWidget(mPreviewWidget);
	addDockWidget(Qt::RightDockWidgetArea, mPreviewToolBar);

	connect(mPreviewToolBar, &QDockWidget::topLevelChanged, this, &swMainWindow::EnableWindowFlags);
}

bool swMainWindow::eventFilter(QObject* Object, QEvent* Event)
{
	// Resizing the pieces panel by hand snaps it to whole tiles once the mouse is released.
	if (Object == mPartsToolBar && Event->type() == QEvent::Resize && QApplication::mouseButtons() != Qt::NoButton)
		mPartsSnapPending = true;
	else if (Event->type() == QEvent::MouseButtonRelease && mPartsSnapPending)
	{
		mPartsSnapPending = false;
		QTimer::singleShot(0, this, &swMainWindow::SnapPartsDock);
	}

	QTabBar* TabBar = mModelTabWidget ? mModelTabWidget->tabBar() : nullptr;

	if (TabBar && Object == TabBar)
	{
		if (Event->type() == QEvent::MouseButtonPress)
		{
			QMouseEvent* MouseEvent = static_cast<QMouseEvent*>(Event);
			mMousePressTabIndex = -1;

			if (MouseEvent->button() == Qt::MiddleButton)
				mMousePressTabIndex = TabBar->tabAt(MouseEvent->pos());
		}
		else if (Event->type() == QEvent::MouseButtonRelease)
		{
			QMouseEvent* MouseEvent = static_cast<QMouseEvent*>(Event);

			if (MouseEvent->button() == Qt::MiddleButton && TabBar->tabAt(MouseEvent->pos()) == mMousePressTabIndex)
				TabBar->tabCloseRequested(mMousePressTabIndex);
		}
	}

	return QMainWindow::eventFilter(Object, Event);
}

void swMainWindow::TogglePreviewWidget(bool Visible)
{
	if (mPreviewToolBar)
	{
		if (Visible)
			mPreviewToolBar->show();
		else
			mPreviewToolBar->hide();
	}
	else if (Visible)
	{
		CreatePreviewWidget();
	}
}

void swMainWindow::EnableWindowFlags(bool Detached)
{
	if (Detached)
	{
		QDockWidget* DockWidget = qobject_cast<QDockWidget*>(sender());

		DockWidget->setWindowFlags(Qt::CustomizeWindowHint | Qt::Window | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint);

		if (isVisible())
			DockWidget->show();
	}
}

class swElidedLabel : public QFrame
{
public:
	explicit swElidedLabel(QWidget* Parent = nullptr)
		: QFrame(Parent)
	{
		setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	}

	void setText(const QString& Text)
	{
		mText = Text;
		update();
	}

protected:
	void paintEvent(QPaintEvent* event) override;

	QString mText;
};

void swElidedLabel::paintEvent(QPaintEvent* event)
{
	QFrame::paintEvent(event);

	QPainter Painter(this);
	QFontMetrics FontMetrics = Painter.fontMetrics();

	int LineSpacing = FontMetrics.lineSpacing();
	int y = 0;

	QTextLayout TextLayout(mText, Painter.font());
	TextLayout.beginLayout();

	for (;;)
	{
		QTextLine Line = TextLayout.createLine();

		if (!Line.isValid())
			break;

		Line.setLineWidth(width());
		int NextLineY = y + LineSpacing;

		if (height() >= NextLineY + LineSpacing)
		{
			Line.draw(&Painter, QPoint(0, y));
			y = NextLineY;
		}
		else
		{
			QString LastLine = mText.mid(Line.textStart());
			QString ElidedLastLine = FontMetrics.elidedText(LastLine, Qt::ElideRight, width());
			Painter.drawText(QPoint(0, y + FontMetrics.ascent()), ElidedLastLine);
			Line = TextLayout.createLine();
			break;
		}
	}

	TextLayout.endLayout();
}

void swMainWindow::CreateStatusBar()
{
	QStatusBar* StatusBar = new QStatusBar(this);
	setStatusBar(StatusBar);

	// Keep the rightmost item one standard spacing away from the window edge.
	StatusBar->setContentsMargins(0, 0, swGetLayoutSpacing(StatusBar, Qt::Horizontal), 0);

	mStatusBarLabel = new swElidedLabel();
	StatusBar->addWidget(mStatusBarLabel, 1);

	mStatusLoadSpinner = new swSpinnerWidget();
	StatusBar->addPermanentWidget(mStatusLoadSpinner);
	mStatusLoadSpinner->hide();

	mStatusLoadFailureLabel = new QLabel();
	constexpr int WarningIconSize = 14;
	const QSize WarningPixelSize = QSize(WarningIconSize, WarningIconSize) * devicePixelRatioF();
	QPixmap WarningPixmap = swCreateInterfaceIcon("widgets", "status_warning").pixmap(WarningPixelSize);
	WarningPixmap = WarningPixmap.scaled(WarningPixelSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
	WarningPixmap.setDevicePixelRatio(devicePixelRatioF());
	mStatusLoadFailureLabel->setPixmap(WarningPixmap);
	mStatusLoadFailureLabel->setAccessibleName(tr("Asset loading failed"));
	StatusBar->addPermanentWidget(mStatusLoadFailureLabel);
	mStatusLoadFailureLabel->hide();

	mStatusPositionLabel = new QLabel();
	StatusBar->addPermanentWidget(mStatusPositionLabel);

	mStatusSnapLabel = new QLabel();
	StatusBar->addPermanentWidget(mStatusSnapLabel);

	mStatusTimeLabel = new QLabel();
	StatusBar->addPermanentWidget(mStatusTimeLabel);

	swPiecesLibrary* Library = swGetPiecesLibrary();
	connect(Library, &swPiecesLibrary::AssetStatusChanged, this, &swMainWindow::ScheduleAssetUpdate);
	connect(Library, &swPiecesLibrary::PartLoaded, this, [this](swPieceData* PieceData)
	{
		if (mStatusRequiredAssets.find(PieceData) != mStatusRequiredAssets.end())
			ScheduleAssetUpdate();
	});
	connect(Library, &swPiecesLibrary::PartLoadFailed, this, [this](swPieceData* PieceData, const QString&)
	{
		if (mStatusRequiredAssets.find(PieceData) != mStatusRequiredAssets.end())
			ScheduleAssetUpdate();
	});
}

void swMainWindow::ScheduleAssetUpdate()
{
	if (mAssetUpdateScheduled)
		return;

	mAssetUpdateScheduled = true;

	QTimer::singleShot(0, this, [this]()
	{
		mAssetUpdateScheduled = false;
		UpdateAssets();
	});
}

void swMainWindow::UpdateAssets()
{
	Project* Project = swGetActiveProject();
	mStatusLoadSpinner->setToolTip(QString());

	if (mStatusProject != Project)
	{
		mStatusProject = Project;
		mStatusRequiredAssets.clear();
		mStatusLoadFailureLabel->hide();
		mStatusLoadFailureLabel->setToolTip(QString());
		mStatusLoadFailureLabel->setAccessibleDescription(QString());
	}

	if (!Project || !Project->GetMainModel())
	{
		mStatusLoadSpinner->hide();
		mStatusLoadFailureLabel->hide();
		return;
	}

	std::vector<swModel*> UpdatedModels;

	for (const std::unique_ptr<swModel>& Model : Project->GetModels())
		Model->UpdatePieceData(UpdatedModels);

	UpdateTimeline(false, true);

	swView::UpdateProjectViews(Project);

	const std::vector<swPieceDataReference> RequiredPieceData = Project->GetRequiredPieceData();
	const std::vector<swPiece*> GeneratedPieces = Project->GetRequiredSynthPieces();

	mStatusRequiredAssets.clear();
	for (const swPieceDataReference& RequiredData : RequiredPieceData)
		mStatusRequiredAssets.insert(RequiredData.Get());

	int PendingParts = 0;
	int PendingSynths = 0;
	int Failed = 0;
	QString FailureDetails;

	for (const QString& Warning : Project->GetLoadWarningDetails())
	{
		if (!FailureDetails.isEmpty())
			FailureDetails += QLatin1Char('\n');

		FailureDetails += Warning;
	}

	for (const swPieceDataReference& RequiredData : RequiredPieceData)
	{
		const swPieceData* PieceData = RequiredData.Get();
		if (PieceData->mState == swAssetState::Failed)
		{
			Failed++;

			QString Error = swGetPiecesLibrary()->GetPieceDataLoadError(PieceData);

			if (Error.isEmpty())
				Error = tr("Could not load piece.");

			if (!FailureDetails.isEmpty())
				FailureDetails += QLatin1Char('\n');

			FailureDetails += QStringLiteral("'%1': %2").arg(QString::fromLatin1(PieceData->mFileName), Error);
		}
		else if (PieceData->mState != swAssetState::Loaded)
			PendingParts++;
	}

	for (const swPiece* Piece : GeneratedPieces)
	{
		if (Piece->IsGeneratedMeshPending())
			PendingSynths++;
		else if (!Piece->HasGeneratedMesh())
		{
			Failed++;

			QString Error = swGetPiecesLibrary()->GetSynthMeshError(Piece);

			if (Error.isEmpty())
				Error = tr("Could not generate geometry.");

			if (!FailureDetails.isEmpty())
				FailureDetails += QLatin1Char('\n');

			FailureDetails += QStringLiteral("'%1': %2").arg(QString::fromLatin1(Piece->mPieceData->mFileName), Error);
		}
	}

	const int Total = static_cast<int>(RequiredPieceData.size() + GeneratedPieces.size());
	const int Pending = PendingParts + PendingSynths;
	QString LoadingText;

	if (!PendingParts && PendingSynths)
		LoadingText = tr("Regenerating synthesized piece geometry.");
	else
		LoadingText = tr("Loading model pieces (%1 / %2).").arg(Total - Pending - Failed).arg(Total);

	mStatusLoadSpinner->setToolTip(LoadingText);
	mStatusLoadSpinner->setAccessibleDescription(LoadingText);

	if (!FailureDetails.isEmpty())
	{
		QString FailureText = tr("The model could not be fully loaded. Some pieces may be missing.\n\n%1").arg(FailureDetails);

		mStatusLoadFailureLabel->setToolTip(FailureText);
		mStatusLoadFailureLabel->setAccessibleDescription(FailureText);
		mStatusLoadFailureLabel->show();
	}
	else
	{
		mStatusLoadFailureLabel->hide();
		mStatusLoadFailureLabel->setToolTip(QString());
		mStatusLoadFailureLabel->setAccessibleDescription(QString());
	}

	mStatusLoadSpinner->setVisible(Pending > 0);
}

void swMainWindow::closeEvent(QCloseEvent* Event)
{
	if (SaveProjectIfModified())
	{
		Event->accept();

		QSettings Settings;
		Settings.beginGroup("MainWindow");
		Settings.setValue("Geometry", saveGeometry());
		Settings.setValue("State", saveState());
		mPartSelectionWidget->SaveState(Settings);
		Settings.endGroup();

		gApplication->SaveTabLayout();
	}
	else
		Event->ignore();
}

void swMainWindow::dragEnterEvent(QDragEnterEvent* Event)
{
	if (Event->mimeData()->hasUrls())
		Event->acceptProposedAction();
}

void swMainWindow::dropEvent(QDropEvent* Event)
{
	const QMimeData* MimeData = Event->mimeData();
	const QList<QUrl> Urls = MimeData->urls();
	for (const QUrl& Url : Urls)
		if (OpenProject(Url.toLocalFile()))
			break;
}

QMenu* swMainWindow::createPopupMenu()
{
	QMenu* Menu = new QMenu(this);

	UpdateDockWidgetActions();

	Menu->addAction(mActions[SW_VIEW_TOOLBAR_PARTS]);
	Menu->addAction(mActions[SW_VIEW_TOOLBAR_COLORS]);
	Menu->addAction(mActions[SW_VIEW_TOOLBAR_PROPERTIES]);
	Menu->addAction(mActions[SW_VIEW_TOOLBAR_TIMELINE]);
	Menu->addAction(mActions[SW_VIEW_TOOLBAR_PREVIEW]);
	Menu->addAction(mActions[SW_VIEW_TOOLBAR_MINIFIGURE_BUILDER]);
	Menu->addAction(mActions[SW_VIEW_TOOLBAR_TERMINAL]);
	Menu->addSeparator();
	Menu->addAction(mActions[SW_VIEW_TOOLBAR_STANDARD]);
	Menu->addAction(mActions[SW_VIEW_TOOLBAR_TOOLS]);
	Menu->addAction(mActions[SW_VIEW_TOOLBAR_TIME]);

	return Menu;
}

void swMainWindow::UpdateDockWidgetActions()
{
	mActions[SW_VIEW_TOOLBAR_PARTS]->setChecked(mPartsToolBar->isVisible());
	mActions[SW_VIEW_TOOLBAR_COLORS]->setChecked(mColorsToolBar->isVisible());
	mActions[SW_VIEW_TOOLBAR_PROPERTIES]->setChecked(mPropertiesToolBar->isVisible());
	mActions[SW_VIEW_TOOLBAR_TIMELINE]->setChecked(mTimelineToolBar->isVisible());
	mActions[SW_VIEW_TOOLBAR_STANDARD]->setChecked(mStandardToolBar->isVisible());
	mActions[SW_VIEW_TOOLBAR_TOOLS]->setChecked(mToolsToolBar->isVisible());
	mActions[SW_VIEW_TOOLBAR_TIME]->setChecked(mTimeToolBar->isVisible());
	mActions[SW_VIEW_TOOLBAR_PREVIEW]->setChecked(mPreviewToolBar->isVisible());
	mActions[SW_VIEW_TOOLBAR_MINIFIGURE_BUILDER]->setChecked(mMinifigureBuilderToolBar->isVisible());
	mActions[SW_VIEW_TOOLBAR_TERMINAL]->setChecked(mTerminalToolBar && mTerminalToolBar->isVisible());
}

void swMainWindow::UpdateGamepads()
{
#if SW_ENABLE_GAMEPAD
	QDateTime Now = QDateTime::currentDateTime();
	quint64 Elapsed = mLastGamepadUpdate.msecsTo(Now);
	mLastGamepadUpdate = Now;

	if (!gMainWindow)
		return;

	swView* ActiveView = GetActiveView();
	if (!ActiveView)
		return;

	const QList<int> Gamepads = QGamepadManager::instance()->connectedGamepads();
	if (Gamepads.isEmpty())
		return;

	QGamepad Gamepad(Gamepads[0]);

	float Scale = (float)Elapsed / 20.0f;
	swVector3 Distance(Scale * Gamepad.axisLeftX(), 0.0f, -Scale * Gamepad.axisLeftY());

	if (fabsf(Distance.LengthSquared()) > 0.01f)
		ActiveView->MoveCamera(Distance);
#endif
}

void swMainWindow::ModelTabContextMenuRequested(const QPoint& Point)
{
	QMenu* Menu = new QMenu;

	mMousePressTabIndex = mModelTabWidget->tabBar()->tabAt(Point);

	if (mModelTabWidget->count() > 1)
		Menu->addAction(tr("Close Other Tabs"), this, &swMainWindow::ModelTabCloseOtherTabs);
	if (mMousePressTabIndex == mModelTabWidget->currentIndex())
		Menu->addAction(mActions[SW_VIEW_RESET_VIEWS]);

	Menu->exec(QCursor::pos());
	delete Menu;
}

void swMainWindow::ModelTabCloseOtherTabs()
{
	if (mMousePressTabIndex == -1)
		return;

	while (mModelTabWidget->count() - 1 > mMousePressTabIndex)
		delete mModelTabWidget->widget(mMousePressTabIndex + 1);

	while (mModelTabWidget->count() > 1)
		delete mModelTabWidget->widget(0);
}

void swMainWindow::ModelTabClosed(int Index)
{
	if (mModelTabWidget->count() != 1)
		delete mModelTabWidget->widget(Index);
	else
		NewProject();
}

void swMainWindow::ModelTabChanged(int Index)
{
	Project* Project = swGetActiveProject();
	swModelTabWidget* CurrentTab = (swModelTabWidget*)mModelTabWidget->widget(Index);

	Project->SetActiveModel(CurrentTab ? CurrentTab->GetModel() : nullptr, true);
}

void swMainWindow::ClipboardChanged()
{
	const QString MimeType = QLatin1String("application/vnd.studworks-clipboard");
	const QMimeData* MimeData = QApplication::clipboard()->mimeData();
	QByteArray ClipboardData;

	if (MimeData && MimeData->hasFormat(MimeType))
		ClipboardData = MimeData->data(MimeType);

	gApplication->SetClipboard(ClipboardData);
}

void swMainWindow::ActionTriggered()
{
	QObject* Action = sender();

	for (int CommandIdx = 0; CommandIdx < SW_COMMAND_COUNT; CommandIdx++)
	{
		if (Action == mActions[CommandIdx])
		{
			HandleCommand((swCommandId)CommandIdx);
			break;
		}
	}
}

void swMainWindow::ColorChanged(int ColorIndex)
{
	SetColorIndex(ColorIndex);
}

void swMainWindow::PartListPicked(swPieceData* PieceData)
{
	if (!PieceData)
		return;

	quint32 ColorCode = swGetColorCode(mPartSelectionWidget->GetColorIndex());

	PreviewPiece(PieceData, ColorCode, true);
}

void swMainWindow::ColorButtonClicked()
{
	swModel* ActiveModel = GetActiveModel();

	if (ActiveModel)
		ActiveModel->PaintSelectedPieces();
}

void swMainWindow::ProjectFileChanged(const QString& Path)
{
	static bool Ignore;
	Project* CurrentProject = swGetActiveProject();

	if (Ignore || !CurrentProject || Path.isEmpty() || QFileInfo(Path) != QFileInfo(CurrentProject->GetFileName()))
		return;

	QString Text = tr("The file '%1' has been modified by another application. Do you want to reload it?").arg(QDir::toNativeSeparators(Path));

	const QString CurrentFileName = CurrentProject->GetFileName();

	Ignore = true;

	if (QMessageBox::question(this, tr("File Changed"), Text, QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
	{
		Ignore = false;

		if (CurrentProject == swGetActiveProject() && CurrentFileName == CurrentProject->GetFileName())
		{
			CurrentProject->MarkAsModified();
			UpdateTitle();
		}

		return;
	}

	if (CurrentProject != swGetActiveProject() || CurrentFileName != CurrentProject->GetFileName())
	{
		Ignore = false;
		return;
	}

	ReloadProjectFile(Path);

	Ignore = false;
}

void swMainWindow::ToggleWatchFileChanges()
{
	swGetPreferences().mWatchFileChanges = !swGetPreferences().mWatchFileChanges;
	swSetProfileInteger(SW_PROFILE_WATCH_FILE_CHANGES, swGetPreferences().mWatchFileChanges);

	swGetActiveProject()->UpdateFileWatcher();
}

void swMainWindow::ReloadProject()
{
	Project* CurrentProject = swGetActiveProject();

	if (!CurrentProject)
		return;

	const QString FileName = CurrentProject->GetFileName();

	if (FileName.isEmpty())
	{
		QMessageBox::information(this, tr("Reload"), tr("The current model has not been saved to a file yet."));
		return;
	}

	if (CurrentProject->IsModified())
	{
		QString Text = tr("The model has unsaved changes. Do you want to discard them and reload '%1'?").arg(QDir::toNativeSeparators(FileName));

		if (QMessageBox::question(this, tr("Reload"), Text, QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
			return;
	}

	ReloadProjectFile(FileName);
}

void swMainWindow::ReloadProjectFile(const QString& FileName)
{
	std::unique_ptr<Project> NewProject = std::make_unique<Project>();

	if (NewProject->Load(FileName, true))
	{
		QByteArray TabLayout = GetTabLayout();
		gApplication->SetProject(NewProject.release());
		RestoreTabLayout(TabLayout);
		swView::UpdateAllViews();
	}
}

void swMainWindow::Print(QPrinter* Printer)
{
	swGetActiveProject()->GetMainModel()->WaitForAssets();

	swPrintInstructions(swGetActiveProject()->GetInstructions(), Printer);
}

void swMainWindow::ShowAboutDialog()
{
	swAboutDialog Dialog(this);
	Dialog.exec();
}

void swMainWindow::ShowHTMLDialog()
{
	swHTMLExportOptions Options(swGetActiveProject());
	swHTMLDialog Dialog(this, &Options);

	if (Dialog.exec() != QDialog::Accepted)
		return;

	Options.SaveDefaults();
	ShowResultError(swGetActiveProject()->ExportHTML(Options), tr("Export HTML"));
}

void swMainWindow::ShowExportPOVRayDialog()
{
	swResult<void> ExportResult = swGetActiveProject()->ExportPOVRay(QString());

	if (!ExportResult && !ExportResult.error().isEmpty())
		QMessageBox::warning(this, tr("POV-Ray Export" ), ExportResult.error());
}

void swMainWindow::ShowRenderDialog(swRenderDialogMode RenderDialogMode)
{
#ifndef SW_DISABLE_RENDER_DIALOG
	swRenderDialog Dialog(this, RenderDialogMode);
	Dialog.exec();
#endif
}

void swMainWindow::ShowInstructionsDialog()
{
	swGetActiveProject()->GetMainModel()->WaitForAssets();

	// The instructions window stays open beside the model and follows its changes.
	if (mInstructionsDialog)
	{
		mInstructionsDialog->raise();
		mInstructionsDialog->activateWindow();
		return;
	}

	swInstructionsDialog* Dialog = new swInstructionsDialog(this, swGetActiveProject());
	Dialog->setAttribute(Qt::WA_DeleteOnClose);
	connect(this, &swMainWindow::ModelContentsChanged, Dialog, &swInstructionsDialog::ModelChanged);
	mInstructionsDialog = Dialog;
	Dialog->show();
}

void swMainWindow::CloseInstructionsDialog()
{
	if (mInstructionsDialog)
		delete mInstructionsDialog;
}

void swMainWindow::ShowPrintDialog()
{
	swGetActiveProject()->GetMainModel()->WaitForAssets();

#ifndef QT_NO_PRINTER
	// Printing uses the instruction pages and their page setup, rebuilt from the current model.
	swInstructions* Instructions = swGetActiveProject()->GetInstructions();
	Instructions->Update();

	QPrinter Printer(QPrinter::HighResolution);
	swSetupInstructionsPrinter(Instructions, &Printer);

	QPrintDialog PrintDialog(&Printer, this);

	if (PrintDialog.exec() == QDialog::Accepted)
		Print(&Printer);
#endif
}

void swMainWindow::ShowImageDialog()
{
	swImageDialogOptions Options;
	Project* Project = swGetActiveProject();
	swModel* Model = Project->GetActiveModel();

	Options.Width = swGetProfileInteger(SW_PROFILE_IMAGE_WIDTH);
	Options.Height = swGetProfileInteger(SW_PROFILE_IMAGE_HEIGHT);
	Options.Start = Model->GetCurrentStep();
	Options.End = Model->GetLastStep();
	Options.FilePath = Project->GetImageFileName(false);

	swImageDialog Dialog(this, &Options);

	if (Dialog.exec() != QDialog::Accepted)
		return;

	swSetProfileInteger(SW_PROFILE_IMAGE_WIDTH, Options.Width);
	swSetProfileInteger(SW_PROFILE_IMAGE_HEIGHT, Options.Height);

	const swResult<void> Saved = swGetActiveProject()->SaveImage(Options);

	if (!Saved)
		QMessageBox::warning(this, tr("Save Image"), Saved.error());
}

void swMainWindow::ShowSelectDialog()
{
	swModel* ActiveModel = GetActiveModel();

	if (!ActiveModel)
		return;

	if (ActiveModel->GetPieces().empty() && ActiveModel->GetCameras().empty() && ActiveModel->GetLights().empty())
	{
		QMessageBox::information(this, tr("Select Objects"), tr("Nothing to select."));
		return;
	}

	swSelectDialog Dialog(this, ActiveModel);

	if (Dialog.exec() != QDialog::Accepted)
		return;

	ActiveModel->SetSelectionAndFocusAction(Dialog.mObjects, nullptr, 0, swSelectionMode::Single);
}

void swMainWindow::ShowResultError(const swResult<void>& Result, const QString& Title)
{
	if (!Result && !Result.error().isEmpty())
		QMessageBox::warning(this, Title, Result.error());
}

void swMainWindow::SetShadingMode(swShadingMode ShadingMode)
{
	swGetPreferences().mShadingMode = ShadingMode;
	swSetProfileShadingMode(ShadingMode);

	UpdateShadingMode();

	UpdatePartsRendering();
}

void swMainWindow::SetLightingStyle(swLightingStyle LightingStyle)
{
	swGetPreferences().mLightingStyle = LightingStyle;
	swSetProfileLightingStyle(LightingStyle);

	UpdateShadingMode();

	UpdatePartsRendering();
}

void swMainWindow::SetSelectionMode(swSelectionMode SelectionMode)
{
	mSelectionMode = SelectionMode;

	UpdateSelectionMode();
}

void swMainWindow::ToggleViewSphere()
{
	swGetPreferences().mViewSphereEnabled = !swGetPreferences().mViewSphereEnabled;
	swSetProfileInteger(SW_PROFILE_VIEW_SPHERE_ENABLED, swGetPreferences().mViewSphereEnabled);

	swView::UpdateAllViews();
}

void swMainWindow::ToggleAxisIcon()
{
	swGetPreferences().mDrawAxes = !swGetPreferences().mDrawAxes;
	swSetProfileInteger(SW_PROFILE_DRAW_AXES, swGetPreferences().mDrawAxes);

	swView::UpdateAllViews();
}

void swMainWindow::ToggleGrid()
{
	swGetPreferences().mGridEnabled = !swGetPreferences().mGridEnabled;

	swView::UpdateAllViews();
}

void swMainWindow::ToggleLoadPiecesBeforeShowing()
{
	swGetPreferences().mLoadPiecesBeforeShowing = !swGetPreferences().mLoadPiecesBeforeShowing;
	swSetProfileInteger(SW_PROFILE_LOAD_PIECES_BEFORE_SHOWING, swGetPreferences().mLoadPiecesBeforeShowing);
}

static void swTogglePreference(bool& Value, SW_PROFILE_KEY Key)
{
	Value = !Value;
	swSetProfileInteger(Key, Value);

	swView::UpdateAllViews();
}

void swMainWindow::SetHighlightRenderMode(swHighlightRenderMode HighlightRenderMode)
{
	swGetPreferences().mHighlightRenderMode = HighlightRenderMode;
	swSetProfileHighlightRenderMode(HighlightRenderMode);

	swView::UpdateAllViews();
}

void swMainWindow::SetSelectionOutlineWidth(int SelectionOutlineWidth)
{
	SelectionOutlineWidth = qBound(SW_SELECTION_OUTLINE_WIDTH_MINIMUM, SelectionOutlineWidth, SW_SELECTION_OUTLINE_WIDTH_MAXIMUM);

	swGetPreferences().mSelectionOutlineWidth = SelectionOutlineWidth;
	swSetProfileInteger(SW_PROFILE_SELECTION_OUTLINE_WIDTH, SelectionOutlineWidth);

	swView::UpdateAllViews();
}

void swMainWindow::ToggleAutomateEdgeColor()
{
	swGetPreferences().mAutomateEdgeColor = !swGetPreferences().mAutomateEdgeColor;
	swSetProfileInteger(SW_PROFILE_AUTOMATE_EDGE_COLOR, swGetPreferences().mAutomateEdgeColor);

	swGetPiecesLibrary()->LoadColors();
	swView::UpdateAllViews();
}

void swMainWindow::SetStudStyle(swStudStyle StudStyle)
{
	swSetProfileStudStyle(StudStyle);
	swGetPiecesLibrary()->UpdateStudStyleAndColors(StudStyle, swGetPreferences().mStudCylinderColorEnabled);

	swView::UpdateAllViews();
}

void swMainWindow::ShowStepViewDialog()
{
	swModel* ActiveModel = GetActiveModel();
	swView* ActiveView = GetActiveView();

	if (!ActiveModel || !ActiveView)
		return;

	const swStep Step = ActiveModel->GetCurrentStep();
	const std::map<swStep, swStepView>& StepViews = ActiveModel->GetProperties().mStepViews;
	const std::map<swStep, swStepView>::const_iterator Current = StepViews.find(Step);

	QDialog Dialog(this);
	Dialog.setWindowTitle(tr("Step %1 View").arg(Step));

	QFormLayout* Layout = new QFormLayout(&Dialog);

	QComboBox* ModeCombo = new QComboBox(&Dialog);
	ModeCombo->addItems({ tr("No view change"), tr("Relative to the default view"), tr("Absolute"), tr("Added to the previous view"), tr("Back to the default view") });
	ModeCombo->setToolTip(tr("How this step turns the view, saved as an MLCAD ROTSTEP line ending the step."));
	Layout->addRow(tr("Rotation:"), ModeCombo);

	QDoubleSpinBox* AngleEdits[3];
	const QString AxisNames[3] = { tr("X:"), tr("Y:"), tr("Z:") };

	for (int Axis = 0; Axis < 3; Axis++)
	{
		AngleEdits[Axis] = new QDoubleSpinBox(&Dialog);
		AngleEdits[Axis]->setRange(-360.0, 360.0);
		AngleEdits[Axis]->setDecimals(2);
		AngleEdits[Axis]->setSuffix(tr(" deg."));
		Layout->addRow(AxisNames[Axis], AngleEdits[Axis]);
	}

	QPushButton* CurrentViewButton = new QPushButton(tr("Use Current View"), &Dialog);
	CurrentViewButton->setToolTip(tr("Fill in the absolute rotation that matches the current view."));
	Layout->addRow(QString(), CurrentViewButton);

	QDialogButtonBox* Buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &Dialog);
	Layout->addRow(Buttons);

	auto UpdateEnabled = [ModeCombo, AngleEdits]()
	{
		const bool HasAngles = ModeCombo->currentIndex() >= 1 && ModeCombo->currentIndex() <= 3;

		for (QDoubleSpinBox* AngleEdit : AngleEdits)
			AngleEdit->setEnabled(HasAngles);
	};

	if (Current != StepViews.end())
	{
		ModeCombo->setCurrentIndex(static_cast<int>(Current->second.Mode) + 1);

		for (int Axis = 0; Axis < 3; Axis++)
			AngleEdits[Axis]->setValue(Current->second.Angles[Axis]);
	}

	UpdateEnabled();

	connect(ModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), &Dialog, UpdateEnabled);
	connect(CurrentViewButton, &QPushButton::clicked, &Dialog, [ActiveView, ModeCombo, AngleEdits]()
	{
		const swVector3 Angles = ActiveView->GetStepViewAngles();

		ModeCombo->setCurrentIndex(static_cast<int>(swStepViewMode::Absolute) + 1);

		for (int Axis = 0; Axis < 3; Axis++)
			AngleEdits[Axis]->setValue(Angles[Axis]);
	});
	connect(Buttons, &QDialogButtonBox::accepted, &Dialog, &QDialog::accept);
	connect(Buttons, &QDialogButtonBox::rejected, &Dialog, &QDialog::reject);

	if (Dialog.exec() != QDialog::Accepted)
		return;

	std::optional<swStepView> StepView;

	if (ModeCombo->currentIndex() > 0)
	{
		StepView = swStepView();
		StepView->Mode = static_cast<swStepViewMode>(ModeCombo->currentIndex() - 1);

		if (StepView->Mode != swStepViewMode::End)
			StepView->Angles = swVector3(AngleEdits[0]->value(), AngleEdits[1]->value(), AngleEdits[2]->value());
	}

	ActiveModel->SetStepView(Step, StepView);
}

void swMainWindow::ToggleFadePreviousSteps()
{
	swGetPreferences().mFadeSteps = !swGetPreferences().mFadeSteps;
	swSetProfileInteger(SW_PROFILE_FADE_STEPS, swGetPreferences().mFadeSteps);

	swView::UpdateAllViews();
}

void swMainWindow::ToggleEdgeLines(swRenderTarget Target)
{
	swPreferences& Preferences = swGetPreferences();

	if (Target == swRenderTarget::ModelView)
	{
		Preferences.mDrawEdgeLines = !Preferences.mDrawEdgeLines;
		swSetProfileInteger(SW_PROFILE_DRAW_EDGE_LINES, Preferences.mDrawEdgeLines);
	}
	else
	{
		swTargetRenderSettings& TargetSettings = Preferences.GetTargetRenderSettings(Target);

		// Changing a target setting stops following the model view settings.
		TargetSettings.Settings = Preferences.GetRenderSettings(Target);
		TargetSettings.Settings.DrawEdgeLines = !TargetSettings.Settings.DrawEdgeLines;
		TargetSettings.SyncRendering = false;
		Preferences.SaveRenderTarget(Target);
	}

	UpdatePartsRendering();
}

void swMainWindow::ToggleConditionalLines(swRenderTarget Target)
{
	swPreferences& Preferences = swGetPreferences();

	if (Target == swRenderTarget::ModelView)
	{
		Preferences.mDrawConditionalLines = !Preferences.mDrawConditionalLines;
		swSetProfileInteger(SW_PROFILE_DRAW_CONDITIONAL_LINES, Preferences.mDrawConditionalLines);
	}
	else
	{
		swTargetRenderSettings& TargetSettings = Preferences.GetTargetRenderSettings(Target);

		TargetSettings.Settings = Preferences.GetRenderSettings(Target);
		TargetSettings.Settings.DrawConditionalLines = !TargetSettings.Settings.DrawConditionalLines;
		TargetSettings.SyncRendering = false;
		Preferences.SaveRenderTarget(Target);
	}

	UpdatePartsRendering();
}

void swMainWindow::ToggleSyncRendering(swRenderTarget Target)
{
	swPreferences& Preferences = swGetPreferences();
	swTargetRenderSettings& TargetSettings = Preferences.GetTargetRenderSettings(Target);

	TargetSettings.SyncRendering = !TargetSettings.SyncRendering;
	Preferences.SaveRenderTarget(Target);

	UpdatePartsRendering();
}

void swMainWindow::SetTargetShadingMode(swRenderTarget Target, swShadingMode ShadingMode)
{
	swPreferences& Preferences = swGetPreferences();
	swTargetRenderSettings& TargetSettings = Preferences.GetTargetRenderSettings(Target);

	TargetSettings.Settings = Preferences.GetRenderSettings(Target);
	TargetSettings.Settings.ShadingMode = ShadingMode;
	TargetSettings.SyncRendering = false;
	Preferences.SaveRenderTarget(Target);

	UpdatePartsRendering();
}

QAction* swMainWindow::AddMenuCommand(QMenu* Menu, const QString& Text, swCommandId CommandId, bool Checked)
{
	// Menu entries run the matching command so keyboard shortcuts and menus stay in sync.
	QAction* Action = Menu->addAction(Text, this, [this, CommandId]()
	{
		HandleCommand(CommandId);
	});

	Action->setCheckable(true);
	Action->setChecked(Checked);
	Action->setShortcut(mActions[CommandId]->shortcut());
	Action->setShortcutContext(Qt::WidgetShortcut);

	return Action;
}

void swMainWindow::AddRenderTargetActions(QMenu* Menu, swRenderTarget Target)
{
	struct swRenderTargetCommands
	{
		swCommandId SyncRendering;
		swCommandId Wireframe;
		swCommandId Flat;
		swCommandId DefaultLights;
		swCommandId EdgeLines;
		swCommandId ConditionalLines;
	};

	// The model view commands, the other targets replace them with their own.
	swRenderTargetCommands Commands = { SW_COMMAND_COUNT, SW_VIEW_SHADING_WIREFRAME, SW_VIEW_SHADING_FLAT, SW_VIEW_SHADING_DEFAULT_LIGHTS, SW_VIEW_TOGGLE_EDGE_LINES, SW_VIEW_TOGGLE_CONDITIONAL_LINES };

	switch (Target)
	{
	case swRenderTarget::ModelView:
		break;

	case swRenderTarget::Preview:
		Commands = { SW_VIEW_TOGGLE_PREVIEW_SYNCHRONIZED_RENDERING, SW_VIEW_PREVIEW_SHADING_WIREFRAME, SW_VIEW_PREVIEW_SHADING_FLAT, SW_VIEW_PREVIEW_SHADING_DEFAULT_LIGHTS, SW_VIEW_TOGGLE_PREVIEW_EDGE_LINES, SW_VIEW_TOGGLE_PREVIEW_CONDITIONAL_LINES };
		break;

	case swRenderTarget::MinifigureBuilder:
		Commands = { SW_VIEW_TOGGLE_MINIFIGURE_BUILDER_SYNCHRONIZED_RENDERING, SW_VIEW_MINIFIGURE_BUILDER_SHADING_WIREFRAME, SW_VIEW_MINIFIGURE_BUILDER_SHADING_FLAT, SW_VIEW_MINIFIGURE_BUILDER_SHADING_DEFAULT_LIGHTS, SW_VIEW_TOGGLE_MINIFIGURE_BUILDER_EDGE_LINES, SW_VIEW_TOGGLE_MINIFIGURE_BUILDER_CONDITIONAL_LINES };
		break;

	case swRenderTarget::Pieces:
		Commands = { SW_VIEW_TOGGLE_PIECES_SYNCHRONIZED_RENDERING, SW_VIEW_PIECES_SHADING_WIREFRAME, SW_VIEW_PIECES_SHADING_FLAT, SW_VIEW_PIECES_SHADING_DEFAULT_LIGHTS, SW_VIEW_TOGGLE_PIECES_EDGE_LINES, SW_VIEW_TOGGLE_PIECES_CONDITIONAL_LINES };
		break;
	}

	const swPreferences& Preferences = swGetPreferences();
	const swRenderSettings RenderSettings = Preferences.GetRenderSettings(Target);

	// The model views have no settings to follow.
	if (Target != swRenderTarget::ModelView)
		AddMenuCommand(Menu, tr("Use Model View Settings"), Commands.SyncRendering, Preferences.GetTargetRenderSettings(Target).SyncRendering);

	QMenu* ShadingMenu = Menu->addMenu(tr("Shading"));
	QActionGroup* ShadingGroup = new QActionGroup(ShadingMenu);
	ShadingGroup->addAction(AddMenuCommand(ShadingMenu, tr("Wireframe"), Commands.Wireframe, RenderSettings.ShadingMode == swShadingMode::Wireframe));
	ShadingGroup->addAction(AddMenuCommand(ShadingMenu, tr("Flat Shading"), Commands.Flat, RenderSettings.ShadingMode == swShadingMode::Flat));
	ShadingGroup->addAction(AddMenuCommand(ShadingMenu, tr("Default Lights"), Commands.DefaultLights, RenderSettings.ShadingMode == swShadingMode::DefaultLights));
	ShadingMenu->addSeparator();

	// the lighting style is shared by every target
	const char* const LightingNames[] = { QT_TR_NOOP("Standard Lighting"), QT_TR_NOOP("Studio Lighting"), QT_TR_NOOP("Matte Lighting"), QT_TR_NOOP("Toon Lighting") };
	QActionGroup* LightingGroup = new QActionGroup(ShadingMenu);

	for (int ActionIdx = SW_VIEW_LIGHTING_FIRST; ActionIdx <= SW_VIEW_LIGHTING_LAST; ActionIdx++)
	{
		const int StyleIndex = ActionIdx - SW_VIEW_LIGHTING_FIRST;
		LightingGroup->addAction(AddMenuCommand(ShadingMenu, tr(LightingNames[StyleIndex]), static_cast<swCommandId>(ActionIdx), static_cast<int>(Preferences.mLightingStyle) == StyleIndex));
	}

	AddMenuCommand(Menu, tr("Edge Lines"), Commands.EdgeLines, RenderSettings.DrawEdgeLines);
	AddMenuCommand(Menu, tr("Conditional Lines"), Commands.ConditionalLines, RenderSettings.DrawConditionalLines);
}

void swMainWindow::ModelViewOptionsMenuAboutToShow()
{
	QMenu* Menu = qobject_cast<QMenu*>(sender());
	Menu->clear();

	const swPreferences& Preferences = swGetPreferences();

	AddMenuCommand(Menu, tr("View Sphere"), SW_VIEW_TOGGLE_VIEW_SPHERE, Preferences.mViewSphereEnabled);
	AddMenuCommand(Menu, tr("Axis Icon"), SW_VIEW_TOGGLE_AXIS_ICON, Preferences.mDrawAxes);
	AddMenuCommand(Menu, tr("Base Grid"), SW_VIEW_TOGGLE_GRID, Preferences.mGridEnabled);
	AddMenuCommand(Menu, tr("XZ Height Grid"), SW_VIEW_GRID_TOGGLE_HEIGHT_XZ, Preferences.mDrawGridHeightXZ);
	AddMenuCommand(Menu, tr("YZ Height Grid"), SW_VIEW_GRID_TOGGLE_HEIGHT_YZ, Preferences.mDrawGridHeightYZ);
	AddMenuCommand(Menu, tr("Height Grids Behind Model"), SW_VIEW_GRID_TOGGLE_HEIGHT_BEHIND, Preferences.mGridHeightBehindModel);
	AddMenuCommand(Menu, tr("Fade Previous Steps"), SW_VIEW_FADE_PREVIOUS_STEPS, Preferences.mFadeSteps);
	AddMenuCommand(Menu, tr("Insert First Piece at Origin"), SW_PIECE_TOGGLE_FIRST_AT_ORIGIN, Preferences.mFirstPieceAtOrigin);

	Menu->addSeparator();

	AddRenderTargetActions(Menu, swRenderTarget::ModelView);
}

void swMainWindow::TogglePreviewViewSphere()
{
	swGetPreferences().mPreviewViewSphereEnabled = !swGetPreferences().mPreviewViewSphereEnabled;
	swSetProfileInteger(SW_PROFILE_PREVIEW_VIEW_SPHERE_ENABLED, swGetPreferences().mPreviewViewSphereEnabled);

	swView::UpdateAllViews();
}

void swMainWindow::TogglePreviewAxisIcon()
{
	swGetPreferences().mDrawPreviewAxis = !swGetPreferences().mDrawPreviewAxis;
	swSetProfileInteger(SW_PROFILE_PREVIEW_DRAW_AXES, swGetPreferences().mDrawPreviewAxis);

	swView::UpdateAllViews();
}

void swMainWindow::UpdatePartsRendering()
{
	swView::UpdateAllViews();

	if (mPartSelectionWidget)
		mPartSelectionWidget->UpdateThumbnails();
}

QByteArray swMainWindow::GetTabLayout()
{
	QByteArray TabLayoutData;
	QDataStream DataStream(&TabLayoutData, QIODevice::WriteOnly);

	DataStream << (quint32)SW_TAB_LAYOUT_VERSION;
	qint32 NumTabs = mModelTabWidget->count();
	DataStream << NumTabs;
	DataStream << ((swModelTabWidget*)mModelTabWidget->currentWidget())->GetModel()->GetProperties().mFileName;

	for (int TabIdx = 0; TabIdx < NumTabs; TabIdx++)
	{
		swModelTabWidget* TabWidget = (swModelTabWidget*)mModelTabWidget->widget(TabIdx);

		DataStream << TabWidget->GetModel()->GetProperties().mFileName;

		std::function<void (QWidget*)> SaveWidget = [&DataStream, &SaveWidget, &TabWidget](QWidget* Widget)
		{
			if (Widget->metaObject() == &swViewWidget::staticMetaObject)
			{
				swView* CurrentView = ((swViewWidget*)Widget)->GetView();

				DataStream << (qint32)0;
				DataStream << (qint32)(TabWidget->GetActiveView() == CurrentView ? 1 : 0);

				const swCamera* Camera = CurrentView->GetCamera();

				if (Camera->IsSimple())
				{
					DataStream << (qint32)0;
					DataStream << Camera->m_fovy;
					DataStream << Camera->m_zNear;
					DataStream << Camera->m_zFar;
					DataStream << Camera->mPosition;
					DataStream << Camera->mTargetPosition;
					DataStream << Camera->mUpVector;
				}
				else
				{
					DataStream << (qint32)1;
					DataStream << Camera->GetName();
				}
			}
			else
			{
				QSplitter* Splitter = (QSplitter*)Widget;

				DataStream << (qint32)(Splitter->orientation() == Qt::Horizontal ? 1 : 2);
				DataStream << Splitter->sizes();

				SaveWidget(Splitter->widget(0));
				SaveWidget(Splitter->widget(1));
			}
		};

		QLayout* TabLayout = TabWidget->layout();
		SaveWidget(TabLayout->itemAt(0)->widget());
	}

	return TabLayoutData;
}

void swMainWindow::RestoreTabLayout(const QByteArray& TabLayout)
{
	if (TabLayout.isEmpty())
		return;

	QDataStream DataStream(TabLayout);

	quint32 Version;
	DataStream >> Version;

	if (Version != SW_TAB_LAYOUT_VERSION)
		return;

	qint32 NumTabs;
	DataStream >> NumTabs;
	QString CurrentTabName;
	DataStream >> CurrentTabName;

	RemoveAllModelTabs();

	for (int TabIdx = 0; TabIdx < NumTabs; TabIdx++)
	{
		QString ModelName;
		DataStream >> ModelName;

		swModel* Model = swGetActiveProject()->GetModel(ModelName);
		swModelTabWidget* TabWidget = nullptr;

		if (Model)
		{
			SetCurrentModelTab(Model, false);
			TabWidget = (swModelTabWidget*)mModelTabWidget->widget(mModelTabWidget->count() - 1);
		}

		QWidget* ActiveWidget = nullptr;

		std::function<void(QWidget*)> LoadWidget = [&DataStream, &LoadWidget, &ActiveWidget, this](QWidget* ParentWidget)
		{
			qint32 WidgetType;
			DataStream >> WidgetType;

			if (WidgetType == 0)
			{
				qint32 IsActive;
				DataStream >> IsActive;

				if (IsActive)
					ActiveWidget = ParentWidget;

				qint32 CameraType;
				DataStream >> CameraType;

				swView* CurrentView = nullptr;

				if (ParentWidget)
					CurrentView = ((swViewWidget*)ParentWidget)->GetView();

				if (CameraType == 0)
				{
					float FoV, ZNear, ZFar;
					swVector3 Position, TargetPosition, UpVector;

					DataStream >> FoV;
					DataStream >> ZNear;
					DataStream >> ZFar;
					DataStream >> Position;
					DataStream >> TargetPosition;
					DataStream >> UpVector;

					if (CurrentView)
					{
						swCamera* Camera = CurrentView->GetCamera();
						if (!std::isnan(FoV))
							Camera->m_fovy = FoV;
						if (!std::isnan(ZNear))
							Camera->m_zNear = ZNear;
						if (!std::isnan(ZFar))
							Camera->m_zFar = ZFar;
						if (!Position.IsNan() && !TargetPosition.IsNan() && !UpVector.IsNan())
						{
							Camera->SetPosition(Position, 1, false);
							Camera->SetTargetPosition(TargetPosition, 1, false);
							Camera->SetUpVector(UpVector, 1, false);
						}
						Camera->UpdatePosition(1);
						Camera->ExpandFarPlane(CurrentView->GetModel()->GetPiecesBoundingBoxPoints(swGeometryBoundsMode::IncludeFallback));
					}
				}
				else
				{
					QByteArray CameraName;
					DataStream >> CameraName;

					if (CurrentView)
						CurrentView->SetCamera(CameraName);
				}
			}
			else
			{
				QList<int> Sizes;
				DataStream >> Sizes;

				if (ParentWidget)
				{
					ParentWidget->setFocus();

					if (WidgetType == 1)
						SplitVertical();
					else
						SplitHorizontal();

					QSplitter* Splitter = (QSplitter*)ParentWidget->parentWidget();
					Splitter->setSizes(Sizes);

					LoadWidget(Splitter->widget(0));
					LoadWidget(Splitter->widget(1));
				}
				else
				{
					LoadWidget(nullptr);
					LoadWidget(nullptr);
				}
			}
		};

		LoadWidget(TabWidget ? TabWidget->layout()->itemAt(0)->widget() : nullptr);

		if (ActiveWidget && TabWidget)
			ActiveWidget->setFocus();
	}

	if (!mModelTabWidget->count())
		swGetActiveProject()->SetActiveModel(0, true);
	else
		swGetActiveProject()->SetActiveModel(CurrentTabName, true);
}

void swMainWindow::RemoveAllModelTabs()
{
	while (mModelTabWidget->count())
		delete mModelTabWidget->widget(0);
}

void swMainWindow::CloseCurrentModelTab()
{
	if (mModelTabWidget->count() > 1)
		delete mModelTabWidget->currentWidget();
	else
		NewProject();
}

void swMainWindow::SetCurrentModelTab(swModel* Model, bool AdjustFarPlane)
{
	for (int TabIdx = 0; TabIdx < mModelTabWidget->count(); TabIdx++)
	{
		swModelTabWidget* TabWidget = (swModelTabWidget*)mModelTabWidget->widget(TabIdx);

		if (TabWidget->GetModel() == Model)
		{
			mModelTabWidget->setCurrentIndex(TabIdx);
			return;
		}
	}

	swModelTabWidget* TabWidget = new swModelTabWidget(Model);
	mModelTabWidget->addTab(TabWidget, Model->GetProperties().mFileName);

	QVBoxLayout* CentralLayout = new QVBoxLayout(TabWidget);
	CentralLayout->setContentsMargins(0, 0, 0, 0);

	swView* NewView = CreateView(Model);
	swViewWidget* ViewWidget = new swViewWidget(TabWidget, NewView);
	CentralLayout->addWidget(ViewWidget);

	mModelTabWidget->setCurrentWidget(TabWidget);

	ViewWidget->show();
	ViewWidget->setFocus();
	NewView->ZoomExtents(swGeometryBoundsMode::IncludeFallback, AdjustFarPlane);
}

void swMainWindow::AddView(swView* View)
{
	swModelTabWidget* TabWidget = GetTabWidgetForModel(View->GetModel());

	if (!TabWidget)
		return;

	if (!TabWidget->GetActiveView())
		TabWidget->SetActiveView(View);
}

void swMainWindow::RemoveView(swView* View)
{
	swModelTabWidget* TabWidget = GetTabForView(View);

	if (TabWidget)
		TabWidget->RemoveView(View);
}

void swMainWindow::SetActiveView(swView* ActiveView)
{
	swModelTabWidget* TabWidget = GetTabForView(ActiveView);

	if (!TabWidget)
		return;

	swView* CurrentActiveView = TabWidget->GetActiveView();

	if (CurrentActiveView == ActiveView)
		return;

	if (CurrentActiveView)
		CurrentActiveView->SetTopSubmodelActive();

	TabWidget->SetActiveView(ActiveView);
}

void swMainWindow::SetTool(swTool Tool)
{
	mTool = Tool;

	QAction* Action = mActions[SW_EDIT_ACTION_FIRST + static_cast<int>(mTool)];

	if (Action)
		Action->setChecked(true);

	swView::UpdateAllViews();
}

void swMainWindow::ToggleFirstPieceAtOrigin()
{
	swGetPreferences().mFirstPieceAtOrigin = !swGetPreferences().mFirstPieceAtOrigin;
	swSetProfileInteger(SW_PROFILE_FIRST_PIECE_AT_ORIGIN, swGetPreferences().mFirstPieceAtOrigin);

	mActions[SW_PIECE_TOGGLE_FIRST_AT_ORIGIN]->setChecked(swGetPreferences().mFirstPieceAtOrigin);
}

void swMainWindow::SetColorIndex(int ColorIndex)
{
	mColorIndex = ColorIndex;

	// Picking a color explicitly also becomes the color to insert next.
	mLastInsertedColorIndex = ColorIndex;

	UpdatePiecesColor();
	UpdateColor();
}

int swMainWindow::GetInsertColorIndex() const
{
	switch (swGetPreferences().mInsertColorMode)
	{
	case swInsertColorMode::CurrentColor:
		break;

	case swInsertColorMode::SelectedPieceColor:
		// Selecting a piece already made its color the current color.
		break;

	case swInsertColorMode::LastInsertedColor:
		if (mLastInsertedColorIndex != -1)
			return mLastInsertedColorIndex;
		break;
	}

	return mColorIndex;
}

void swMainWindow::SetLastInsertedColorIndex(int ColorIndex)
{
	mLastInsertedColorIndex = ColorIndex;

	UpdatePiecesColor();
}

void swMainWindow::SetInsertColorMode(swInsertColorMode InsertColorMode)
{
	swGetPreferences().mInsertColorMode = InsertColorMode;
	swSetProfileInteger(SW_PROFILE_INSERT_COLOR_MODE, static_cast<int>(InsertColorMode));

	UpdatePiecesColor();
}

void swMainWindow::ColorOptionsMenuAboutToShow()
{
	QMenu* Menu = qobject_cast<QMenu*>(sender());
	Menu->clear();

	// Each entry runs the matching command so keyboard shortcuts and the menu stay in sync.
	auto AddCommand = [this](QMenu* TargetMenu, const QString& Text, swCommandId CommandId, bool Checked)
	{
		QAction* Action = TargetMenu->addAction(Text, this, [this, CommandId]()
		{
			HandleCommand(CommandId);
		});

		Action->setCheckable(true);
		Action->setChecked(Checked);
		Action->setShortcut(mActions[CommandId]->shortcut());
		Action->setShortcutContext(Qt::WidgetShortcut);

		return Action;
	};

	const swInsertColorMode InsertColorMode = swGetPreferences().mInsertColorMode;

	QMenu* InsertMenu = Menu->addMenu(tr("Insert Color"));
	QActionGroup* InsertGroup = new QActionGroup(InsertMenu);
	InsertGroup->addAction(AddCommand(InsertMenu, tr("Current Color"), SW_PIECE_INSERT_COLOR_CURRENT, InsertColorMode == swInsertColorMode::CurrentColor));
	InsertGroup->addAction(AddCommand(InsertMenu, tr("Selected Piece Color"), SW_PIECE_INSERT_COLOR_SELECTED, InsertColorMode == swInsertColorMode::SelectedPieceColor));
	InsertGroup->addAction(AddCommand(InsertMenu, tr("Last Inserted Piece Color"), SW_PIECE_INSERT_COLOR_LAST_INSERTED, InsertColorMode == swInsertColorMode::LastInsertedColor));

	AddCommand(Menu, tr("Lock Pieces Color"), SW_VIEW_PIECES_TOGGLE_LOCK_COLOR, mPartSelectionWidget && mPartSelectionWidget->IsColorLocked());
}

void swMainWindow::UpdatePiecesColor()
{
	// The pieces list shows the color new pieces will be inserted with, unless its color is locked.
	if (mPartSelectionWidget)
		mPartSelectionWidget->SetColorIndex(GetInsertColorIndex());
}

void swMainWindow::SetMoveSnapEnabled(bool Enabled)
{
	mMoveSnapEnabled = Enabled;
	UpdateSnap();
}

void swMainWindow::SetAngleSnapEnabled(bool Enabled)
{
	mAngleSnapEnabled = Enabled;
	UpdateSnap();
}

QString swMainWindow::GetAngleSnapText() const
{
	return mAngleSnapEnabled ? swFormatDoubleLocalized(GetAngleSnap(), locale(), 'g', 6) : tr("None");
}

void swMainWindow::SetMoveXYSnapIndex(int Index)
{
	mMoveXYSnapIndex = Index;
	UpdateSnap();
}

void swMainWindow::SetMoveZSnapIndex(int Index)
{
	mMoveZSnapIndex = Index;
	UpdateSnap();
}

void swMainWindow::SetAngleSnapIndex(int Index)
{
	mAngleSnapIndex = Index;
	UpdateSnap();
}

void swMainWindow::SetCustomAngleSnap()
{
	bool Ok = false;
	const double Angle = QInputDialog::getDouble(this, tr("Custom Rotation Snap"), tr("Snap angle in degrees:"), mAngleSnapCustom, 0.01, 360.0, 2, &Ok);

	if (!Ok)
	{
		UpdateSnap();
		return;
	}

	// Keep at most two decimals.
	mAngleSnapCustom = static_cast<float>(qRound(Angle * 100.0) / 100.0);
	swSetProfileFloat(SW_PROFILE_ANGLE_SNAP_CUSTOM, mAngleSnapCustom);

	mActions[SW_EDIT_SNAP_ANGLE_CUSTOM]->setText(tr("Custom (%1 Degrees)...").arg(swFormatDoubleLocalized(mAngleSnapCustom, locale(), 'g', 6)));

	SetAngleSnapIndex(SW_EDIT_SNAP_ANGLE_CUSTOM - SW_EDIT_SNAP_ANGLE0);
}

void swMainWindow::SetRelativeTransform(bool RelativeTransform)
{
	mRelativeTransform = RelativeTransform;

	UpdateLockSnap();
	swView::UpdateAllViews();
}

void swMainWindow::SetSeparateTransform(bool SelectionTransform)
{
	mLocalTransform = SelectionTransform;

	UpdateLockSnap();
}

void swMainWindow::SetTransformType(swTransformType TransformType)
{
	if (TransformType < swTransformType::First || TransformType >= swTransformType::Count)
		return;

	mTransformType = TransformType;

	constexpr const char* IconNames[] =
	{
		"transform_translation_absolute",
		"transform_translation_relative",
		"transform_rotation_absolute",
		"transform_rotation_relative"
	};

	SW_ARRAY_SIZE_CHECK(IconNames, swTransformType::Count);

	int TransformIndex = static_cast<int>(TransformType);
	mActions[SW_EDIT_TRANSFORM_ABSOLUTE_TRANSLATION + TransformIndex]->setChecked(true);
	mActions[SW_EDIT_TRANSFORM]->setIcon(swCreateInterfaceIcon("toolbars/standard", IconNames[TransformIndex]));
}

void swMainWindow::SetInsertPieceData(swPieceData* PieceData)
{
	mInsertPieceData = swPieceDataReference(PieceData);

	if (mInsertPieceData)
		swGetPiecesLibrary()->RequestPieceDataLoad(mInsertPieceData.Get(), swLoadPriority::Normal);
}

std::optional<swVector3> swMainWindow::GetTransformAmount()
{
	return swVector3(mTransformXEdit->value(), mTransformYEdit->value(), mTransformZEdit->value());
}

void swMainWindow::SplitView(Qt::Orientation Orientation)
{
	QWidget* Focus = focusWidget();

	if (Focus->metaObject() != &swViewWidget::staticMetaObject)
		return;

	QWidget* Parent = Focus->parentWidget();
	QSplitter* Splitter;
	QList<int> Sizes;

	if (Parent->metaObject() == &swModelTabWidget::staticMetaObject)
	{
		Splitter = new QSplitter(Orientation, Parent);
		Parent->layout()->addWidget(Splitter);
		Splitter->addWidget(Focus);
		Splitter->addWidget(new swViewWidget(mModelTabWidget->currentWidget(), CreateView(GetCurrentTabModel())));
	}
	else
	{
		QSplitter* ParentSplitter = (QSplitter*)Parent;
		Sizes = ParentSplitter->sizes();
		int FocusIndex = ParentSplitter->indexOf(Focus);

		Splitter = new QSplitter(Orientation, Parent);
		ParentSplitter->insertWidget(FocusIndex, Splitter);
		Splitter->addWidget(Focus);
		Splitter->addWidget(new swViewWidget(mModelTabWidget->currentWidget(), CreateView(GetCurrentTabModel())));

		ParentSplitter->setSizes(Sizes);
	}

	Sizes.clear();
	Sizes.append(10);
	Sizes.append(10);
	Splitter->setSizes(Sizes);
	Focus->setFocus();
}

void swMainWindow::SplitHorizontal()
{
	SplitView(Qt::Vertical);
}

void swMainWindow::SplitVertical()
{
	SplitView(Qt::Horizontal);
}

void swMainWindow::RemoveActiveView()
{
	QWidget* Focus = focusWidget();

	if (Focus->metaObject() != &swViewWidget::staticMetaObject)
		return;

	QWidget* Parent = Focus->parentWidget();

	if (Parent->metaObject() == &swModelTabWidget::staticMetaObject)
		return;

	QWidget* ParentParentWidget = Parent->parentWidget();
	QSplitter* ParentSplitter = (QSplitter*)Parent;
	int FocusIndex = ParentSplitter->indexOf(Focus);
	QWidget* OtherWidget = ParentSplitter->widget(!FocusIndex);

	if (ParentParentWidget->metaObject() == &swModelTabWidget::staticMetaObject)
	{
		QLayout* CentralLayout = ParentParentWidget->layout();

		CentralLayout->addWidget(OtherWidget);
		CentralLayout->removeWidget(Parent);
	}
	else
	{
		QSplitter* ParentParentSplitter = (QSplitter*)ParentParentWidget;
		QList<int> Sizes = ParentParentSplitter->sizes();

		int ParentIndex = ParentParentSplitter->indexOf(Parent);
		Parent->setParent(nullptr);
		ParentParentSplitter->insertWidget(ParentIndex, OtherWidget);

		ParentParentSplitter->setSizes(Sizes);
	}

	Parent->deleteLater();

	if (OtherWidget->metaObject() != &swViewWidget::staticMetaObject)
	{
		swModelTabWidget* TabWidget = (swModelTabWidget*)mModelTabWidget->currentWidget();

		if (TabWidget)
			OtherWidget = TabWidget->GetAnyViewWidget();
	}

	OtherWidget->setFocus();
}

void swMainWindow::ResetViews()
{
	swModelTabWidget* TabWidget = (swModelTabWidget*)mModelTabWidget->currentWidget();

	if (!TabWidget)
		return;

	QLayout* TabLayout = TabWidget->layout();
	QWidget* TopWidget = TabLayout->itemAt(0)->widget();
	TopWidget->deleteLater();

	swView* NewView = CreateView(TabWidget->GetModel());
	swViewWidget* ViewWidget = new swViewWidget(TabWidget, NewView);
	TabLayout->addWidget(ViewWidget);

	ViewWidget->show();
	ViewWidget->setFocus();
	NewView->ZoomExtents(swGeometryBoundsMode::IncludeFallback, true);
}

void swMainWindow::ToggleDockWidget(QWidget* DockWidget)
{
	if (DockWidget->isHidden())
	{
		DockWidget->show();
		DockWidget->raise();
	}
	else
		DockWidget->close();
}

void swMainWindow::TogglePrintPreview()
{
#ifndef QT_NO_PRINTER
	// todo: print preview inside main window

	swGetActiveProject()->GetMainModel()->WaitForAssets();

	swInstructions* Instructions = swGetActiveProject()->GetInstructions();
	Instructions->Update();

	QPrinter Printer(QPrinter::ScreenResolution);
	swSetupInstructionsPrinter(Instructions, &Printer);

	QPrintPreviewDialog Preview(&Printer, this);
	connect(&Preview, &QPrintPreviewDialog::paintRequested, this, &swMainWindow::Print);
	Preview.exec();
#endif
}

void swMainWindow::ToggleFullScreen()
{
	// todo: hide toolbars and menu
	// todo: create fullscreen toolbar or support esc key to go back
	if (isFullScreen())
		showNormal();
	else
		showFullScreen();
}

void swMainWindow::AddRecentFile(const QString& FileName)
{
	QString SavedName = FileName;
	int MaxRecentFiles = static_cast<int>(mRecentFiles.size());
	int FileIndex;

	QFileInfo FileInfo(FileName);

	for (FileIndex = 0; FileIndex < MaxRecentFiles; FileIndex++)
		if (QFileInfo(mRecentFiles[FileIndex]) == FileInfo)
			break;

	for (FileIndex = swMinimum(FileIndex, MaxRecentFiles - 1); FileIndex > 0; FileIndex--)
		mRecentFiles[FileIndex] = mRecentFiles[FileIndex - 1];

	mRecentFiles[0] = SavedName;
}

void swMainWindow::RemoveRecentFile(size_t FileIndex)
{
	for (; FileIndex < mRecentFiles.size() - 1; FileIndex++)
		mRecentFiles[FileIndex] = mRecentFiles[FileIndex + 1];

	mRecentFiles[mRecentFiles.size() - 1].clear();
}

void swMainWindow::UpdateSelectedObjects(bool SelectionChanged)
{
	int Flags = 0;
	std::vector<swObject*> Selection;
	swObject* Focus = nullptr;

	swModel* ActiveModel = GetActiveModel();
	if (ActiveModel)
		ActiveModel->GetSelectionInformation(&Flags, Selection, &Focus);

	if (SelectionChanged)
	{
		mTimelineWidget->UpdateSelection();

		// Selecting a piece makes its color current; picking a color afterwards overrides it.
		if (swGetPreferences().mInsertColorMode == swInsertColorMode::SelectedPieceColor)
		{
			swObject* SelectedPiece = (Focus && Focus->IsPiece()) ? Focus : nullptr;

			for (size_t ObjectIndex = 0; !SelectedPiece && ObjectIndex < Selection.size(); ObjectIndex++)
				if (Selection[ObjectIndex]->IsPiece())
					SelectedPiece = Selection[ObjectIndex];

			if (SelectedPiece && static_cast<swPiece*>(SelectedPiece)->GetColorIndex() != mColorIndex)
				SetColorIndex(static_cast<swPiece*>(SelectedPiece)->GetColorIndex());
		}

		mActions[SW_EDIT_CUT]->setEnabled(Flags & SW_SELECTION_SELECTED);
		mActions[SW_EDIT_COPY]->setEnabled(Flags & SW_SELECTION_SELECTED);
		mActions[SW_EDIT_FIND]->setEnabled((Flags & SW_SELECTION_NO_PIECES) == 0);
		mActions[SW_EDIT_FIND_NEXT]->setEnabled((Flags & SW_SELECTION_NO_PIECES) == 0);
		mActions[SW_EDIT_FIND_PREVIOUS]->setEnabled((Flags & SW_SELECTION_NO_PIECES) == 0);
		mActions[SW_EDIT_FIND_ALL]->setEnabled((Flags & SW_SELECTION_NO_PIECES) == 0);
		mActions[SW_EDIT_REPLACE]->setEnabled((Flags & SW_SELECTION_NO_PIECES) == 0);
		mActions[SW_EDIT_REPLACE_NEXT]->setEnabled((Flags & SW_SELECTION_NO_PIECES) == 0);
		mActions[SW_EDIT_REPLACE_ALL]->setEnabled((Flags & SW_SELECTION_NO_PIECES) == 0);
		mActions[SW_EDIT_SELECT_INVERT]->setEnabled((Flags & SW_SELECTION_NO_PIECES) == 0);
		mActions[SW_EDIT_SELECT_BY_NAME]->setEnabled((Flags & SW_SELECTION_NO_PIECES) == 0);
		mActions[SW_EDIT_SELECT_NONE]->setEnabled(Flags & SW_SELECTION_SELECTED);
		mActions[SW_EDIT_SELECT_ALL]->setEnabled(Flags & SW_SELECTION_UNSELECTED);

		mActions[SW_PIECE_DELETE]->setEnabled(Flags & SW_SELECTION_SELECTED);
		mActions[SW_PIECE_DUPLICATE]->setEnabled(Flags & SW_SELECTION_SELECTED);
		mActions[SW_PIECE_PAINT_SELECTED]->setEnabled(Flags & SW_SELECTION_PIECE);
		mActions[SW_PIECE_RESET_PIVOT_POINT]->setEnabled(Flags & SW_SELECTION_SELECTED);
		mActions[SW_PIECE_REMOVE_KEY_FRAMES]->setEnabled(Flags & SW_SELECTION_SELECTED);
		mActions[SW_PIECE_ARRAY]->setEnabled(Flags & SW_SELECTION_PIECE);
		mActions[SW_PIECE_CONTROL_POINT_INSERT]->setEnabled(Flags & SW_SELECTION_CAN_ADD_CONTROL_POINT);
		mActions[SW_PIECE_CONTROL_POINT_REMOVE]->setEnabled(Flags & SW_SELECTION_CAN_REMOVE_CONTROL_POINT);
		mActions[SW_PIECE_TRAIN_TRACK_FOCUS_NEXT]->setEnabled(Flags & SW_SELECTION_TRAIN_TRACK_VISIBLE);
		mActions[SW_PIECE_TRAIN_TRACK_FOCUS_PREVIOUS]->setEnabled(Flags & SW_SELECTION_TRAIN_TRACK_VISIBLE);
		mActions[SW_PIECE_TRAIN_TRACK_ROTATE]->setEnabled(Flags & SW_SELECTION_TRAIN_TRACK_VISIBLE);
		mActions[SW_PIECE_HIDE_SELECTED]->setEnabled(Flags & SW_SELECTION_VISIBLE_SELECTED);
		mActions[SW_PIECE_HIDE_UNSELECTED]->setEnabled(Flags & SW_SELECTION_UNSELECTED);
		mActions[SW_PIECE_UNHIDE_SELECTED]->setEnabled(Flags & SW_SELECTION_HIDDEN_SELECTED);
		mActions[SW_PIECE_UNHIDE_ALL]->setEnabled(Flags & SW_SELECTION_HIDDEN);
		mActions[SW_PIECE_VIEW_SELECTED_MODEL]->setEnabled(Flags & SW_SELECTION_MODEL_SELECTED);
		mActions[SW_PIECE_MOVE_SELECTION_TO_MODEL]->setEnabled(Flags & SW_SELECTION_PIECE);
		mActions[SW_PIECE_INLINE_SELECTED_MODELS]->setEnabled(Flags & SW_SELECTION_MODEL_SELECTED);
		mActions[SW_PIECE_EDIT_SELECTED_SUBMODEL]->setEnabled(Flags & SW_SELECTION_MODEL_SELECTED);
		mActions[SW_PIECE_GROUP]->setEnabled(Flags & SW_SELECTION_CAN_GROUP);
		mActions[SW_PIECE_UNGROUP]->setEnabled(Flags & SW_SELECTION_GROUPED);
		mActions[SW_PIECE_GROUP_ADD]->setEnabled((Flags & (SW_SELECTION_GROUPED | SW_SELECTION_FOCUS_GROUPED)) == SW_SELECTION_GROUPED);
		mActions[SW_PIECE_GROUP_REMOVE]->setEnabled(Flags & SW_SELECTION_FOCUS_GROUPED);
		mActions[SW_PIECE_GROUP_EDIT]->setEnabled((Flags & SW_SELECTION_NO_PIECES) == 0);
		mActions[SW_PIECE_SHOW_EARLIER]->setEnabled(Flags & SW_SELECTION_PIECE); // FIXME: disable if current step is 1
		mActions[SW_PIECE_SHOW_LATER]->setEnabled(Flags & SW_SELECTION_PIECE);
		mActions[SW_TIMELINE_MOVE_SELECTION]->setEnabled(Flags & SW_SELECTION_PIECE);
		mActions[SW_TIMELINE_MOVE_SELECTION_BEFORE]->setEnabled(Flags & SW_SELECTION_PIECE);
		mActions[SW_TIMELINE_MOVE_SELECTION_AFTER]->setEnabled(Flags & SW_SELECTION_PIECE);

		mActions[SW_PIECE_EDIT_END_SUBMODEL]->setEnabled(GetCurrentTabModel() != ActiveModel);
	}

	mPropertiesWidget->Update(Selection, Focus);

	if (Focus && Focus->IsPiece())
	{
		swPiece* Piece = (swPiece*)Focus;
		int ColorIndex = Piece->GetColorIndex();
		quint32 ColorCode = swGetColorCode(ColorIndex);

		PreviewPiece(Piece->mPieceData.Get(), ColorCode, false);
	}

	QString Message;

	if ((Selection.size() == 1) && Focus)
	{
		if (Focus->IsPiece())
			Message = tr("%1 (ID: %2)").arg(Focus->GetName(), ((swPiece*)Focus)->GetID());
		else
			Message = Focus->GetName();
	}
	else if (Selection.size() > 0)
	{
		Message = tr("%n objects selected", nullptr, static_cast<int>(Selection.size()));

		if (Focus && Focus->IsPiece())
		{
			Message.append(tr(" - %1 (ID: %2)").arg(Focus->GetName(), ((swPiece*)Focus)->GetID()));

			const swGroup* Group = ((swPiece*)Focus)->GetGroup();
			if (Group && !Group->mName.isEmpty())
				Message.append(tr(" in group '%1'").arg(Group->mName));
		}
	}

	mStatusBarLabel->setText(Message);

	swVector3 Position(0.0f, 0.0f, 0.0f);

	if (ActiveModel)
		ActiveModel->GetFocusPosition(Position);

	// Name the focused object so the coordinates are clearly its position.
	QString Object;

	if (Focus && Focus->IsPiece())
		Object = tr("Piece");
	else if (Focus && Focus->IsCamera())
		Object = tr("Camera");
	else if (Focus && Focus->IsLight())
		Object = tr("Light");
	else
		Object = tr("Origin");

	QString Label = tr("%1 X: %2 Y: %3 Z: %4");
	Label = Label.arg(Object, QLocale::system().toString(Position[0], 'f', 2), QLocale::system().toString(Position[1], 'f', 2), QLocale::system().toString(Position[2], 'f', 2));
	mStatusPositionLabel->setText(Label);
}

void swMainWindow::UpdateTimeline(bool Clear, bool UpdateItems)
{
	mTimelineWidget->Update(Clear, UpdateItems);

	emit ModelContentsChanged();
}

void swMainWindow::UpdatePaste(bool Enabled)
{
	if (mActions[SW_EDIT_PASTE])
		mActions[SW_EDIT_PASTE]->setEnabled(Enabled);

	if (mActions[SW_EDIT_PASTE_STEPS])
		mActions[SW_EDIT_PASTE_STEPS]->setEnabled(Enabled);
}

void swMainWindow::UpdateCurrentStep()
{
	swModel* ActiveModel = GetActiveModel();

	if (!ActiveModel)
		return;

	swStep CurrentStep = ActiveModel->GetCurrentStep();
	swStep LastStep = ActiveModel->GetLastStep();

	mActions[SW_VIEW_TIME_FIRST]->setEnabled(CurrentStep != 1);
	mActions[SW_VIEW_TIME_PREVIOUS]->setEnabled(CurrentStep > 1);
	mActions[SW_VIEW_TIME_NEXT]->setEnabled(CurrentStep < SW_STEP_MAXIMUM);
	mActions[SW_VIEW_TIME_LAST]->setEnabled(CurrentStep != LastStep);

	mStatusTimeLabel->setText(QString(tr("Step %1")).arg(swFormatIntegerLocalized(CurrentStep, locale())));
}

void swMainWindow::SetAddKeys(bool AddKeys)
{
	QAction* Action = mActions[SW_VIEW_TIME_ADD_KEYS];

	if (Action)
		Action->setChecked(AddKeys);

	mAddKeys = AddKeys;
}

void swMainWindow::UpdateLockSnap()
{
	if (GetRelativeTransform())
		mActions[SW_EDIT_TRANSFORM_RELATIVE]->setChecked(true);
	else
		mActions[SW_EDIT_TRANSFORM_ABSOLUTE]->setChecked(true);

	if (GetSeparateTransform())
		mActions[SW_EDIT_TRANSFORM_SEPARATELY]->setChecked(true);
	else
		mActions[SW_EDIT_TRANSFORM_TOGETHER]->setChecked(true);

	UpdateSnap();
}

void swMainWindow::UpdateSnap()
{
	mActions[SW_EDIT_SNAP_MOVE_TOGGLE]->setChecked(mMoveSnapEnabled);
	mActions[SW_EDIT_SNAP_ANGLE_TOGGLE]->setChecked(mAngleSnapEnabled);
	mActions[SW_EDIT_SNAP_MOVE_XY0 + mMoveXYSnapIndex]->setChecked(true);
	mActions[SW_EDIT_SNAP_MOVE_Z0 + mMoveZSnapIndex]->setChecked(true);
	mActions[SW_EDIT_SNAP_ANGLE0 + mAngleSnapIndex]->setChecked(true);
	mActions[SW_EDIT_SNAP_ORBIT_TOGGLE]->setChecked(mOrbitSnapEnabled);

	// Spell out the transform and snap settings from the toolbar, using the same names as their menus.
	auto MenuText = [this](int CommandId)
	{
		return mActions[CommandId]->text().remove(QLatin1Char('&'));
	};

	const bool MoveSnapped = mMoveSnapEnabled && (mMoveXYSnapIndex || mMoveZSnapIndex);
	const QString MoveXY = MoveSnapped ? MenuText(SW_EDIT_SNAP_MOVE_XY0 + mMoveXYSnapIndex) : tr("free");
	const QString MoveZ = MoveSnapped ? MenuText(SW_EDIT_SNAP_MOVE_Z0 + mMoveZSnapIndex) : tr("free");
	const QString Axes = mRelativeTransform ? tr("relative") : tr("absolute");
	const QString Angle = GetAngleSnap() != 0.0f ? tr("%1 deg.").arg(swFormatDoubleLocalized(GetAngleSnap(), locale(), 'g', 6)) : tr("free");
	const QString Pivot = GetSeparateTransform() ? tr("separately") : tr("together");

	mStatusSnapLabel->setText(tr("- Move XY: %1 Z: %2 - Axes: %3 - Rotate: %4 / %5 -").arg(MoveXY, MoveZ, Axes, Angle, Pivot));
	mStatusSnapLabel->setToolTip(tr("Movement snap, rotation snap, transform axes and rotation pivot. Change them with the last buttons of the standard toolbar."));
}

void swMainWindow::UpdateColor()
{
	QPixmap Pixmap(14, 14);
	Pixmap.fill(QColor::fromRgbF(gColorList[mColorIndex].Value[0], gColorList[mColorIndex].Value[1], gColorList[mColorIndex].Value[2]));

	mColorButton->setIcon(Pixmap);
	mColorButton->setText(QString("  ") + gColorList[mColorIndex].Name);

	mColorList->SetCurrentColor(mColorIndex);
}

void swMainWindow::UpdateUndoRedo(const QString& UndoText, const QString& RedoText)
{
	QAction* UndoAction = mActions[SW_EDIT_UNDO];
	QAction* RedoAction = mActions[SW_EDIT_REDO];

	if (!UndoText.isEmpty())
	{
		UndoAction->setEnabled(true);
		UndoAction->setText(QString(tr("&Undo %1")).arg(UndoText));
	}
	else
	{
		UndoAction->setEnabled(false);
		UndoAction->setText(tr("&Undo"));
	}

	if (!RedoText.isEmpty())
	{
		RedoAction->setEnabled(true);
		RedoAction->setText(QString(tr("&Redo %1")).arg(RedoText));
	}
	else
	{
		RedoAction->setEnabled(false);
		RedoAction->setText(tr("&Redo"));
	}
}

void swMainWindow::ViewFocusReceived()
{
	SetActiveView(qobject_cast<swView*>(sender()));
}

void swMainWindow::FileMenuAboutToShow()
{
	for (int ActionIdx = SW_FILE_RECENT_FIRST; ActionIdx <= SW_FILE_RECENT_LAST; ActionIdx++)
	{
		int FileIdx = ActionIdx - SW_FILE_RECENT_FIRST;
		QAction* Action = mActions[ActionIdx];

		if (!mRecentFiles[FileIdx].isEmpty())
		{
			Action->setText(QString("&%1 %2").arg(QString::number(FileIdx + 1), QDir::toNativeSeparators(mRecentFiles[FileIdx])));
			Action->setVisible(true);
		}
		else
			Action->setVisible(false);
	}

	mActionFileRecentSeparator->setVisible(!mRecentFiles[0].isEmpty());
}

void swMainWindow::CameraMenuAboutToShow()
{
	const std::vector<std::unique_ptr<swCamera>>& Cameras = swGetActiveModel()->GetCameras();
	swView* ActiveView = GetActiveView();
	const swCamera* CurrentCamera = ActiveView ? ActiveView->GetCamera() : nullptr;
	bool CurrentSet = false;

	for (size_t ActionIndex = SW_VIEW_CAMERA_FIRST; ActionIndex <= SW_VIEW_CAMERA_LAST; ActionIndex++)
	{
		QAction* Action = mActions[ActionIndex];
		size_t CameraIndex = ActionIndex - SW_VIEW_CAMERA_FIRST;

		if (CameraIndex < Cameras.size())
		{
			if (CurrentCamera == Cameras[CameraIndex].get())
			{
				Action->setChecked(true);
				CurrentSet = true;
			}

			Action->setText(Cameras[CameraIndex]->GetName());
			Action->setVisible(true);
		}
		else
			Action->setVisible(false);
	}

	if (!CurrentSet)
		mActions[SW_VIEW_CAMERA_NONE]->setChecked(true);
}

void swMainWindow::ProjectionMenuAboutToShow()
{
	swView* ActiveView = GetActiveView();

	if (ActiveView)
	{
		if (ActiveView->GetCamera()->GetProjection() == swCameraProjection::Orthographic)
			mActions[SW_VIEW_PROJECTION_ORTHOGRAPHIC]->setChecked(true);
		else
			mActions[SW_VIEW_PROJECTION_PERSPECTIVE]->setChecked(true);
	}
}

void swMainWindow::UpdateShadingMode()
{
	mActions[SW_VIEW_SHADING_FIRST + static_cast<int>(swGetPreferences().mShadingMode)]->setChecked(true);
	mActions[SW_VIEW_LIGHTING_FIRST + static_cast<int>(swGetPreferences().mLightingStyle)]->setChecked(true);
}

void swMainWindow::UpdateSelectionMode()
{
	switch (mSelectionMode)
	{
	case swSelectionMode::Single:
		mActions[SW_EDIT_SELECTION_SINGLE]->setChecked(true);
		break;

	case swSelectionMode::Piece:
		mActions[SW_EDIT_SELECTION_PIECE]->setChecked(true);
		break;

	case swSelectionMode::Color:
		mActions[SW_EDIT_SELECTION_COLOR]->setChecked(true);
		break;

	case swSelectionMode::PieceColor:
		mActions[SW_EDIT_SELECTION_PIECE_COLOR]->setChecked(true);
		break;
	}
}

void swMainWindow::UpdateModels()
{
	const std::vector<std::unique_ptr<swModel>>& Models = swGetActiveProject()->GetModels();
	swModel* CurrentModel = swGetActiveModel();

	for (size_t ActionIndex = SW_MODEL_FIRST; ActionIndex <= SW_MODEL_LAST; ActionIndex++)
	{
		QAction* Action = mActions[ActionIndex];
		size_t ModelIndex = ActionIndex - SW_MODEL_FIRST;

		if (ModelIndex < Models.size())
		{
			Action->setChecked(CurrentModel == Models[ModelIndex].get());
			Action->setText(QString::fromLatin1("%1%2 %3").arg(ModelIndex < 9 ? QString("&") : QString(), QString::number(ModelIndex + 1), Models[ModelIndex]->GetProperties().mFileName));
			Action->setVisible(true);
		}
		else
			Action->setVisible(false);
	}

	auto HasModel = [&Models](swModel* Model)
	{
		return std::find_if(Models.begin(), Models.end(), [Model](const std::unique_ptr<swModel>& CheckModel) { return CheckModel.get() == Model; }) != Models.end();
	};

	for (int TabIdx = 0; TabIdx < mModelTabWidget->count(); )
	{
		swModelTabWidget* TabWidget = (swModelTabWidget*)mModelTabWidget->widget(TabIdx);
		swModel* Model = TabWidget->GetModel();

		if (!Model)
			TabIdx++;
		else if (HasModel(Model))
		{
			mModelTabWidget->setTabText(TabIdx, Model->GetProperties().mFileName);
			TabIdx++;
		}
		else
			delete TabWidget;
	}

	mPartSelectionWidget->UpdateModels();

	if (mInsertPieceData && mInsertPieceData->IsModel())
		if (!HasModel(mInsertPieceData->GetModel()))
			SetInsertPieceData(nullptr);
}

void swMainWindow::UpdateInUseCategory()
{
	mPartSelectionWidget->UpdateInUseCategory();
}

void swMainWindow::UpdateCategories()
{
	mPartSelectionWidget->UpdateCategories();
}

void swMainWindow::UpdateTitle()
{
	setWindowModified(swGetActiveProject()->IsModified());
	setWindowFilePath(swGetActiveProject()->GetTitle());

	// An unsaved model has no folder to open.
	mActions[SW_VIEW_TERMINAL_OPEN_MODEL_FOLDER]->setEnabled(!swGetActiveProject()->GetFileName().isEmpty());

	if (swGetPreferences().mTerminalFollowModel)
		UpdateTerminalDirectory(false);
}

void swMainWindow::UpdateModified(bool Modified)
{
	setWindowModified(Modified);
}

void swMainWindow::UpdateShortcuts()
{
	for (int ActionIdx = 0; ActionIdx < SW_COMMAND_COUNT; ActionIdx++)
		mActions[ActionIdx]->setShortcut(QKeySequence(gKeyboardShortcuts.mShortcuts[ActionIdx]));
}

void swMainWindow::NewProject()
{
	if (!SaveProjectIfModified())
		return;

	Project* NewProject = new Project();
	gApplication->SetProject(NewProject);
}

bool swMainWindow::OpenProject(const QString& FileName)
{
	if (!SaveProjectIfModified())
		return false;

	QString LoadFileName = FileName;

	if (LoadFileName.isEmpty())
	{
		LoadFileName = swGetActiveProject()->GetFileName();

		if (LoadFileName.isEmpty())
			LoadFileName = swGetProfileString(SW_PROFILE_PROJECTS_PATH);

		LoadFileName = QFileDialog::getOpenFileName(this, tr("Open Model"), LoadFileName, tr("Supported Files (*.lcd *.ldr *.dat *.mpd);;All Files (*.*)"));

		if (LoadFileName.isEmpty())
			return false;

		swSetProfileString(SW_PROFILE_PROJECTS_PATH, QFileInfo(LoadFileName).absolutePath());
	}

	return OpenProjectFile(LoadFileName);
}

void swMainWindow::OpenRecentProject(int RecentFileIndex)
{
	if (!SaveProjectIfModified())
		return;

	if (!OpenProjectFile(mRecentFiles[RecentFileIndex]))
		RemoveRecentFile(RecentFileIndex);
}

bool swMainWindow::OpenProjectFile(const QString& FileName)
{
	Project* NewProject = new Project();

	if (NewProject->Load(FileName, true))
	{
		gApplication->SetProject(NewProject);

		// Show the model complete instead of drawing pieces as they finish loading.
		if (swGetPreferences().mLoadPiecesBeforeShowing)
		{
			QApplication::setOverrideCursor(Qt::WaitCursor);
			NewProject->GetMainModel()->WaitForAssets();
			QApplication::restoreOverrideCursor();
		}

		AddRecentFile(FileName);
		swView::UpdateProjectViews(NewProject);

		return true;
	}

	delete NewProject;
	swGetPiecesLibrary()->RemoveTemporaryPieceData();
	return false;
}

void swMainWindow::ShowMergeDialog()
{
	QString InitialFolder = swGetActiveProject()->GetFileName();

	if (InitialFolder.isEmpty())
		InitialFolder = swGetProfileString(SW_PROFILE_PROJECTS_PATH);
	else
		InitialFolder = QFileInfo(InitialFolder).absolutePath();

	QStringList LoadFileNames = QFileDialog::getOpenFileNames(this, tr("Merge Model"), InitialFolder, tr("Supported Files (*.lcd *.ldr *.dat *.mpd);;All Files (*.*)"));

	if (LoadFileNames.isEmpty())
		return;

	swSetProfileString(SW_PROFILE_PROJECTS_PATH, QFileInfo(LoadFileNames.first()).absolutePath());

	std::vector<std::unique_ptr<Project>> ProjectsToMerge;

	for (const QString& LoadFileName : std::as_const(LoadFileNames))
	{
		std::unique_ptr<Project>& NewProject = ProjectsToMerge.emplace_back(std::make_unique<Project>());

		if (!NewProject->Load(LoadFileName, true))
			return;
	}

	size_t ModelCount = 0;
	std::vector<Project*> Sources;
	Sources.reserve(ProjectsToMerge.size());

	for (const std::unique_ptr<Project>& ProjectToMerge : ProjectsToMerge)
	{
		ModelCount += ProjectToMerge->GetModels().size();
		Sources.push_back(ProjectToMerge.get());
	}

	const swResult<void> MergeResult = swGetActiveProject()->Merge(Sources);

	if (!MergeResult)
	{
		QMessageBox::warning(this, tr("Cannot Merge Model"), MergeResult.error());
		return;
	}

	QMessageBox::information(this, tr("Merge Model"), tr("Merged %n submodels.", nullptr, static_cast<int>(ModelCount)));

	UpdateModels();
}

void swMainWindow::ImportLDD()
{
	if (!SaveProjectIfModified())
		return;

	QString LoadFileName = QFileDialog::getOpenFileName(this, tr("Import"), QString(), tr("LEGO Digital Designer Files (*.lxf);;All Files (*.*)"));
	if (LoadFileName.isEmpty())
		return;

	Project* NewProject = new Project();

	if (NewProject->ImportLDD(LoadFileName))
	{
		gApplication->SetProject(NewProject);
		swView::UpdateProjectViews(NewProject);
	}
	else
	{
		delete NewProject;
		swGetPiecesLibrary()->RemoveTemporaryPieceData();
	}
}

void swMainWindow::ImportInventory()
{
	if (!SaveProjectIfModified())
		return;

	swSetsDatabaseDialog Dialog(this);
	if (Dialog.exec() != QDialog::Accepted)
		return;

	Project* NewProject = new Project();
	std::vector<swSetInventoryItem> SetInventory = Dialog.GetSetInventory();

	if (NewProject->ImportInventory(SetInventory, Dialog.GetSetName(), Dialog.GetSetDescription()))
	{
		gApplication->SetProject(NewProject);
		swView::UpdateProjectViews(NewProject);
	}
	else
		delete NewProject;
}

bool swMainWindow::SaveProject(const QString& FileName)
{
	QString SaveFileName = FileName;
	Project* Project = swGetActiveProject();

	if (SaveFileName.isEmpty())
	{
		SaveFileName = Project->GetFileName();

		if (SaveFileName.isEmpty())
			SaveFileName = QFileInfo(QDir(swGetProfileString(SW_PROFILE_PROJECTS_PATH)), Project->GetTitle()).absoluteFilePath();

		QString Filter = (Project->GetModels().size() > 1) ? tr("Supported Files (*.mpd);;All Files (*.*)") : tr("Supported Files (*.ldr *.dat *.mpd);;All Files (*.*)");

		SaveFileName = QFileDialog::getSaveFileName(this, tr("Save Model"), SaveFileName, Filter);

		if (SaveFileName.isEmpty())
			return false;

		swSetProfileString(SW_PROFILE_PROJECTS_PATH, QFileInfo(SaveFileName).absolutePath());
	}

	if (QFileInfo(SaveFileName).suffix().toLower() == QLatin1String("lcd"))
	{
		QMessageBox::warning(this, tr("Save Model"), tr("Saving files in LCD format is no longer supported. Please use the LDR or MPD formats instead."));
		return false;
	}

	swResult<void> SaveResult = Project->Save(SaveFileName);

	if (!SaveResult)
	{
		QMessageBox::warning(this, tr("Save Model"), SaveResult.error());
		return false;
	}

	AddRecentFile(SaveFileName);
	UpdateTitle();

	return true;
}

bool swMainWindow::SaveProjectIfModified()
{
	Project* Project = swGetActiveProject();
	if (!Project->IsModified())
		return true;

	switch (QMessageBox::question(this, tr("Save Model"), tr("Save changes to '%1'?").arg(Project->GetTitle()), QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel))
	{
	default:
	case QMessageBox::Cancel:
		return false;

	case QMessageBox::Yes:
		if (!SaveProject(Project->GetFileName()))
			return false;
		break;

	case QMessageBox::No:
		break;
	}

	return true;
}

bool swMainWindow::SetModelFromFocus()
{
	swObject* FocusObject = GetActiveModel()->GetFocusObject();

	if (!FocusObject || !FocusObject->IsPiece())
		return false;

	swModel* Model = ((swPiece*)FocusObject)->mPieceData->GetModel();

	if (Model)
	{
		Project* Project = swGetActiveProject();
		Project->SetActiveModel(Model, true);
		return true;
	}

	return false;
}

void swMainWindow::SetModelFromSelection()
{
	if (SetModelFromFocus())
		return;

	swModel* Model = GetActiveModel()->GetFirstSelectedSubmodel();

	if (Model)
	{
		Project* Project = swGetActiveProject();
		Project->SetActiveModel(Model, true);
	}
}

swModel* swMainWindow::GetActiveModel() const
{
	swView* ActiveView = GetActiveView();
	return ActiveView ? ActiveView->GetActiveModel() : nullptr;
}

swModelTabWidget* swMainWindow::GetTabForView(swView* View) const
{
	QWidget* Widget = View->GetWidget();

	while (Widget)
	{
		swModelTabWidget* TabWidget = qobject_cast<swModelTabWidget*>(Widget);

		if (TabWidget)
			return TabWidget;
		else
			Widget = Widget->parentWidget();
	}

	return nullptr;
}

void swMainWindow::HandleCommand(swCommandId CommandId)
{
	swView* ActiveView = GetActiveView();

	if (ActiveView && CommandId != SW_EDIT_CANCEL)
		ActiveView->StopTracking(true);

	swModel* ActiveModel = ActiveView ? ActiveView->GetActiveModel() : nullptr;

	switch (CommandId)
	{
	case SW_FILE_NEW:
		NewProject();
		break;

	case SW_FILE_OPEN:
		OpenProject(QString());
		break;

	case SW_FILE_MERGE:
		ShowMergeDialog();
		break;

	case SW_FILE_RELOAD:
		ReloadProject();
		break;

	case SW_FILE_TOGGLE_WATCH_FILE_CHANGES:
		ToggleWatchFileChanges();
		break;

	case SW_FILE_SAVE:
		SaveProject(swGetActiveProject()->GetFileName());
		break;

	case SW_FILE_SAVE_AS:
		SaveProject(QString());
		break;

	case SW_FILE_SAVE_IMAGE:
		ShowImageDialog();
		break;

	case SW_FILE_IMPORT_LDD:
		ImportLDD();
		break;

	case SW_FILE_IMPORT_INVENTORY:
		ImportInventory();
		break;

	case SW_FILE_EXPORT_3DS:
		ShowResultError(swGetActiveProject()->Export3DStudio(QString()), tr("Export 3DS"));
		break;

	case SW_FILE_EXPORT_COLLADA:
		ShowResultError(swGetActiveProject()->ExportCOLLADA(QString()), tr("Export COLLADA"));
		break;

	case SW_FILE_EXPORT_HTML:
		ShowHTMLDialog();
		break;

	case SW_FILE_EXPORT_BRICKLINK:
		ShowResultError(swGetActiveProject()->ExportBrickLink(), tr("Export BrickLink"));
		break;

	case SW_FILE_EXPORT_CSV:
		ShowResultError(swGetActiveProject()->ExportCSV(QString()), tr("Export CSV"));
		break;

	case SW_FILE_EXPORT_POVRAY:
		ShowExportPOVRayDialog();
		break;

	case SW_FILE_EXPORT_WAVEFRONT:
		ShowResultError(swGetActiveProject()->ExportWavefront(QString()), tr("Export Wavefront"));
		break;

	case SW_FILE_RENDER_POVRAY:
		ShowRenderDialog(swRenderDialogMode::RenderPOVRay);
		break;

	case SW_FILE_RENDER_BLENDER:
		ShowRenderDialog(swRenderDialogMode::RenderBlender);
		break;

	case SW_FILE_RENDER_OPEN_IN_BLENDER:
		ShowRenderDialog(swRenderDialogMode::OpenInBlender);
		break;

	case SW_FILE_INSTRUCTIONS:
		ShowInstructionsDialog();
		break;

	case SW_FILE_PRINT_PREVIEW:
		TogglePrintPreview();
		break;

	case SW_FILE_PRINT:
		ShowPrintDialog();
		break;

	case SW_FILE_RECENT1:
	case SW_FILE_RECENT2:
	case SW_FILE_RECENT3:
	case SW_FILE_RECENT4:
		OpenRecentProject(CommandId - SW_FILE_RECENT1);
		break;

	case SW_FILE_EXIT:
		close();
		break;

	case SW_EDIT_UNDO:
		if (ActiveModel)
			ActiveModel->UndoAction();
		break;

	case SW_EDIT_REDO:
		if (ActiveModel)
			ActiveModel->RedoAction();
		break;

	case SW_EDIT_CUT:
		if (ActiveModel)
			ShowResultError(ActiveModel->Cut(), tr("Cannot Cut Model"));
		break;

	case SW_EDIT_COPY:
		if (ActiveModel)
			ShowResultError(ActiveModel->Copy(), tr("Cannot Copy Model"));
		break;

	case SW_EDIT_PASTE:
		if (ActiveModel)
			ShowResultError(ActiveModel->Paste(true), tr("Cannot Paste Model"));
		break;

	case SW_EDIT_PASTE_STEPS:
		if (ActiveModel)
			ShowResultError(ActiveModel->Paste(false), tr("Cannot Paste Model"));
		break;

	case SW_EDIT_FIND:
		if (ActiveView)
			ActiveView->ShowFindReplaceWidget(false);
		break;

	case SW_EDIT_FIND_NEXT:
		if (ActiveModel)
			ActiveModel->FindReplacePiece(true, false, false);
		break;

	case SW_EDIT_FIND_PREVIOUS:
		if (ActiveModel)
			ActiveModel->FindReplacePiece(false, false, false);
		break;

	case SW_EDIT_FIND_ALL:
		if (ActiveModel)
			ActiveModel->FindReplacePiece(true, true, false);
		break;

	case SW_EDIT_REPLACE:
		if (ActiveView)
			ActiveView->ShowFindReplaceWidget(true);
		break;

	case SW_EDIT_REPLACE_ALL:
		if (ActiveModel)
			ActiveModel->FindReplacePiece(true, true, true);
		break;

	case SW_EDIT_REPLACE_NEXT:
		if (ActiveModel)
			ActiveModel->FindReplacePiece(true, false, true);
		break;

	case SW_EDIT_SELECT_ALL:
		if (ActiveModel)
			ActiveModel->SelectAllPiecesAction();
		break;

	case SW_EDIT_SELECT_NONE:
		if (ActiveModel)
			ActiveModel->ClearSelectionAction();
		break;

	case SW_EDIT_SELECT_INVERT:
		if (ActiveModel)
			ActiveModel->InvertPieceSelectionAction();
		break;

	case SW_EDIT_SELECT_BY_NAME:
		ShowSelectDialog();
		break;

	case SW_EDIT_SELECTION_SINGLE:
		SetSelectionMode(swSelectionMode::Single);
		break;

	case SW_EDIT_SELECTION_PIECE:
		SetSelectionMode(swSelectionMode::Piece);
		break;

	case SW_EDIT_SELECTION_COLOR:
		SetSelectionMode(swSelectionMode::Color);
		break;

	case SW_EDIT_SELECTION_PIECE_COLOR:
		SetSelectionMode(swSelectionMode::PieceColor);
		break;

	case SW_VIEW_SPLIT_HORIZONTAL:
		SplitHorizontal();
		break;

	case SW_VIEW_SPLIT_VERTICAL:
		SplitVertical();
		break;

	case SW_VIEW_REMOVE_VIEW:
		RemoveActiveView();
		break;

	case SW_VIEW_RESET_VIEWS:
		ResetViews();
		break;

	case SW_VIEW_TOOLBAR_STANDARD:
		ToggleDockWidget(mStandardToolBar);
		break;

	case SW_VIEW_TOOLBAR_TOOLS:
		ToggleDockWidget(mToolsToolBar);
		break;

	case SW_VIEW_TOOLBAR_TIME:
		ToggleDockWidget(mTimeToolBar);
		break;

	case SW_VIEW_TOOLBAR_PARTS:
		ToggleDockWidget(mPartsToolBar);
		if (mPartsToolBar->isVisible())
			mPartSelectionWidget->FocusPartFilterWidget();
		break;

	case SW_VIEW_TOOLBAR_COLORS:
		ToggleDockWidget(mColorsToolBar);
		break;

	case SW_VIEW_TOOLBAR_PROPERTIES:
		ToggleDockWidget(mPropertiesToolBar);
		break;

	case SW_VIEW_TOOLBAR_TIMELINE:
		ToggleDockWidget(mTimelineToolBar);
		break;

	case SW_VIEW_TOOLBAR_PREVIEW:
		ToggleDockWidget(mPreviewToolBar);
		break;

	case SW_VIEW_TOOLBAR_TERMINAL:
		if (mTerminalToolBar)
			ToggleDockWidget(mTerminalToolBar);
		break;

	case SW_VIEW_TOOLBAR_MINIFIGURE_BUILDER:
		ToggleDockWidget(mMinifigureBuilderToolBar);
		break;

	case SW_VIEW_FULL_SCREEN:
		ToggleFullScreen();
		break;

	case SW_VIEW_CLOSE_CURRENT_TAB:
		CloseCurrentModelTab();
		break;

	case SW_VIEW_SHADING_WIREFRAME:
		SetShadingMode(swShadingMode::Wireframe);
		break;

	case SW_VIEW_SHADING_FLAT:
		SetShadingMode(swShadingMode::Flat);
		break;

	case SW_VIEW_SHADING_DEFAULT_LIGHTS:
		SetShadingMode(swShadingMode::DefaultLights);
		break;

	case SW_VIEW_LIGHTING_STANDARD:
		SetLightingStyle(swLightingStyle::Standard);
		break;

	case SW_VIEW_LIGHTING_STUDIO:
		SetLightingStyle(swLightingStyle::Studio);
		break;

	case SW_VIEW_LIGHTING_MATTE:
		SetLightingStyle(swLightingStyle::Matte);
		break;

	case SW_VIEW_LIGHTING_TOON:
		SetLightingStyle(swLightingStyle::Toon);
		break;

	case SW_VIEW_PROJECTION_PERSPECTIVE:
		if (ActiveView)
			ActiveView->SetCameraProjection(swCameraProjection::Perspective);
		break;

	case SW_VIEW_PROJECTION_ORTHOGRAPHIC:
		if (ActiveView)
			ActiveView->SetCameraProjection(swCameraProjection::Orthographic);
		break;

	case SW_VIEW_TOGGLE_VIEW_SPHERE:
		ToggleViewSphere();
		break;

	case SW_VIEW_TOGGLE_AXIS_ICON:
		ToggleAxisIcon();
		break;

	case SW_VIEW_TOGGLE_GRID:
		ToggleGrid();
		break;

	case SW_VIEW_FADE_PREVIOUS_STEPS:
		ToggleFadePreviousSteps();
		break;

	case SW_VIEW_STEP_VIEW_EDIT:
		ShowStepViewDialog();
		break;

	case SW_VIEW_STEP_VIEW_REMOVE:
		if (ActiveModel)
			ActiveModel->SetStepView(ActiveModel->GetCurrentStep(), std::nullopt);
		break;

	case SW_VIEW_STEP_VIEW_TOGGLE_FOLLOW:
		swGetPreferences().mFollowStepViews = !swGetPreferences().mFollowStepViews;
		swSetProfileInteger(SW_PROFILE_FOLLOW_STEP_VIEWS, swGetPreferences().mFollowStepViews);
		mActions[SW_VIEW_STEP_VIEW_TOGGLE_FOLLOW]->setChecked(swGetPreferences().mFollowStepViews);

		if (swGetPreferences().mFollowStepViews && ActiveView)
			ActiveView->ApplyStepView();
		break;

	case SW_VIEW_NORMAL_TO_FACE:
		if (ActiveView)
			ActiveView->StartFaceNormalPick();
		break;

	case SW_VIEW_GRID_TOGGLE_HEIGHT_XZ:
		swGetPreferences().mDrawGridHeightXZ = !swGetPreferences().mDrawGridHeightXZ;
		swSetProfileInteger(SW_PROFILE_GRID_HEIGHT_XZ, swGetPreferences().mDrawGridHeightXZ);
		swView::UpdateAllViews();
		break;

	case SW_VIEW_GRID_TOGGLE_HEIGHT_YZ:
		swGetPreferences().mDrawGridHeightYZ = !swGetPreferences().mDrawGridHeightYZ;
		swSetProfileInteger(SW_PROFILE_GRID_HEIGHT_YZ, swGetPreferences().mDrawGridHeightYZ);
		swView::UpdateAllViews();
		break;

	case SW_VIEW_GRID_TOGGLE_HEIGHT_BEHIND:
		swGetPreferences().mGridHeightBehindModel = !swGetPreferences().mGridHeightBehindModel;
		swSetProfileInteger(SW_PROFILE_GRID_HEIGHT_BEHIND_MODEL, swGetPreferences().mGridHeightBehindModel);
		swView::UpdateAllViews();
		break;

	case SW_VIEW_TERMINAL_TOGGLE_FOLLOW_MODEL:
		swGetPreferences().mTerminalFollowModel = !swGetPreferences().mTerminalFollowModel;
		swSetProfileInteger(SW_PROFILE_TERMINAL_FOLLOW_MODEL, swGetPreferences().mTerminalFollowModel);

		if (swGetPreferences().mTerminalFollowModel)
			UpdateTerminalDirectory(false);
		break;

	case SW_VIEW_TERMINAL_GO_TO_MODEL_FOLDER:
		UpdateTerminalDirectory(true);
		break;

	case SW_VIEW_TERMINAL_OPEN_MODEL_FOLDER:
		if (!swGetActiveProject()->GetFileName().isEmpty())
			QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(swGetActiveProject()->GetFileName()).absolutePath()));
		break;

	case SW_VIEW_COLORS_TOGGLE_SOLID:
		mColorList->ToggleGroupCollapsed(SW_COLOR_GROUP_SOLID);
		break;

	case SW_VIEW_COLORS_TOGGLE_TRANSLUCENT:
		mColorList->ToggleGroupCollapsed(SW_COLOR_GROUP_TRANSLUCENT);
		break;

	case SW_VIEW_COLORS_TOGGLE_SPECIAL:
		mColorList->ToggleGroupCollapsed(SW_COLOR_GROUP_SPECIAL);
		break;

	case SW_PIECE_INSERT_COLOR_CURRENT:
		SetInsertColorMode(swInsertColorMode::CurrentColor);
		break;

	case SW_PIECE_INSERT_COLOR_SELECTED:
		SetInsertColorMode(swInsertColorMode::SelectedPieceColor);
		break;

	case SW_PIECE_INSERT_COLOR_LAST_INSERTED:
		SetInsertColorMode(swInsertColorMode::LastInsertedColor);
		break;

	case SW_VIEW_PIECES_TOGGLE_LOCK_COLOR:
		if (mPartSelectionWidget)
		{
			mPartSelectionWidget->ToggleColorLocked();
			UpdatePiecesColor();
		}
		break;

	case SW_FILE_TOGGLE_OPEN_MOST_RECENT:
		swTogglePreference(swGetPreferences().mAutoLoadMostRecent, SW_PROFILE_AUTO_LOAD_MOST_RECENT);
		break;

	case SW_FILE_TOGGLE_RESTORE_TAB_LAYOUT:
		swTogglePreference(swGetPreferences().mRestoreTabLayout, SW_PROFILE_RESTORE_TAB_LAYOUT);
		break;

	case SW_EDIT_TOGGLE_FIXED_DIRECTION_KEYS:
		swTogglePreference(swGetPreferences().mFixedAxes, SW_PROFILE_FIXED_AXES);
		break;

	case SW_VIEW_TOGGLE_MESH_LOD:
		swTogglePreference(swGetPreferences().mAllowLOD, SW_PROFILE_ALLOW_LOD);
		break;

	case SW_VIEW_TOGGLE_HIGHLIGHT_NEW_PIECES:
		swTogglePreference(swGetPreferences().mHighlightNewParts, SW_PROFILE_HIGHLIGHT_NEW_PARTS);
		break;

	case SW_VIEW_TOGGLE_GRID_STUDS:
		swTogglePreference(swGetPreferences().mDrawGridStuds, SW_PROFILE_GRID_STUDS);
		break;

	case SW_VIEW_TOGGLE_GRID_LINES:
		swTogglePreference(swGetPreferences().mDrawGridLines, SW_PROFILE_GRID_LINES);
		break;

	case SW_VIEW_TOGGLE_GRID_ORIGIN:
		swTogglePreference(swGetPreferences().mDrawGridOrigin, SW_PROFILE_GRID_ORIGIN);
		break;

	case SW_VIEW_TOGGLE_AUTOMATE_EDGE_COLOR:
		ToggleAutomateEdgeColor();
		break;

	case SW_VIEW_HIGHLIGHT_STYLE_OUTLINE:
		SetHighlightRenderMode(swHighlightRenderMode::Outline);
		break;

	case SW_VIEW_HIGHLIGHT_STYLE_TINT:
		SetHighlightRenderMode(swHighlightRenderMode::Tint);
		break;

	case SW_VIEW_SELECTION_OUTLINE_WIDTH_INCREASE:
		SetSelectionOutlineWidth(swGetPreferences().mSelectionOutlineWidth + 1);
		break;

	case SW_VIEW_SELECTION_OUTLINE_WIDTH_DECREASE:
		SetSelectionOutlineWidth(swGetPreferences().mSelectionOutlineWidth - 1);
		break;

	case SW_VIEW_STUD_STYLE_PLAIN:
		SetStudStyle(swStudStyle::Plain);
		break;

	case SW_VIEW_STUD_STYLE_THIN_LINES_LOGO:
		SetStudStyle(swStudStyle::ThinLinesLogo);
		break;

	case SW_VIEW_STUD_STYLE_OUTLINE_LOGO:
		SetStudStyle(swStudStyle::OutlineLogo);
		break;

	case SW_VIEW_STUD_STYLE_SHARP_TOP_LOGO:
		SetStudStyle(swStudStyle::SharpTopLogo);
		break;

	case SW_VIEW_STUD_STYLE_ROUNDED_TOP_LOGO:
		SetStudStyle(swStudStyle::RoundedTopLogo);
		break;

	case SW_VIEW_STUD_STYLE_FLATTENED_LOGO:
		SetStudStyle(swStudStyle::FlattenedLogo);
		break;

	case SW_VIEW_STUD_STYLE_HIGH_CONTRAST:
		SetStudStyle(swStudStyle::HighContrast);
		break;

	case SW_VIEW_STUD_STYLE_HIGH_CONTRAST_LOGO:
		SetStudStyle(swStudStyle::HighContrastLogo);
		break;

	case SW_VIEW_TOGGLE_EDGE_LINES:
		ToggleEdgeLines(swRenderTarget::ModelView);
		break;

	case SW_VIEW_TOGGLE_CONDITIONAL_LINES:
		ToggleConditionalLines(swRenderTarget::ModelView);
		break;

	case SW_VIEW_TOGGLE_PREVIEW_SYNCHRONIZED_RENDERING:
		ToggleSyncRendering(swRenderTarget::Preview);
		break;

	case SW_VIEW_PREVIEW_SHADING_WIREFRAME:
		SetTargetShadingMode(swRenderTarget::Preview, swShadingMode::Wireframe);
		break;

	case SW_VIEW_PREVIEW_SHADING_FLAT:
		SetTargetShadingMode(swRenderTarget::Preview, swShadingMode::Flat);
		break;

	case SW_VIEW_PREVIEW_SHADING_DEFAULT_LIGHTS:
		SetTargetShadingMode(swRenderTarget::Preview, swShadingMode::DefaultLights);
		break;

	case SW_VIEW_TOGGLE_PREVIEW_EDGE_LINES:
		ToggleEdgeLines(swRenderTarget::Preview);
		break;

	case SW_VIEW_TOGGLE_PREVIEW_CONDITIONAL_LINES:
		ToggleConditionalLines(swRenderTarget::Preview);
		break;

	case SW_VIEW_TOGGLE_PIECES_SYNCHRONIZED_RENDERING:
		ToggleSyncRendering(swRenderTarget::Pieces);
		break;

	case SW_VIEW_TOGGLE_MINIFIGURE_BUILDER_SYNCHRONIZED_RENDERING:
		ToggleSyncRendering(swRenderTarget::MinifigureBuilder);
		break;

	case SW_VIEW_MINIFIGURE_BUILDER_SHADING_WIREFRAME:
		SetTargetShadingMode(swRenderTarget::MinifigureBuilder, swShadingMode::Wireframe);
		break;

	case SW_VIEW_MINIFIGURE_BUILDER_SHADING_FLAT:
		SetTargetShadingMode(swRenderTarget::MinifigureBuilder, swShadingMode::Flat);
		break;

	case SW_VIEW_MINIFIGURE_BUILDER_SHADING_DEFAULT_LIGHTS:
		SetTargetShadingMode(swRenderTarget::MinifigureBuilder, swShadingMode::DefaultLights);
		break;

	case SW_VIEW_TOGGLE_MINIFIGURE_BUILDER_EDGE_LINES:
		ToggleEdgeLines(swRenderTarget::MinifigureBuilder);
		break;

	case SW_VIEW_TOGGLE_MINIFIGURE_BUILDER_CONDITIONAL_LINES:
		ToggleConditionalLines(swRenderTarget::MinifigureBuilder);
		break;

	case SW_VIEW_PIECES_TOGGLE_SNAP_TO_TILES:
		swGetPreferences().mPiecesSnapToTiles = !swGetPreferences().mPiecesSnapToTiles;
		swSetProfileInteger(SW_PROFILE_PIECES_SNAP_TO_TILES, swGetPreferences().mPiecesSnapToTiles);
		SnapPartsDock();
		break;

	case SW_VIEW_PIECES_SHADING_WIREFRAME:
		SetTargetShadingMode(swRenderTarget::Pieces, swShadingMode::Wireframe);
		break;

	case SW_VIEW_PIECES_SHADING_FLAT:
		SetTargetShadingMode(swRenderTarget::Pieces, swShadingMode::Flat);
		break;

	case SW_VIEW_PIECES_SHADING_DEFAULT_LIGHTS:
		SetTargetShadingMode(swRenderTarget::Pieces, swShadingMode::DefaultLights);
		break;

	case SW_VIEW_TOGGLE_PIECES_EDGE_LINES:
		ToggleEdgeLines(swRenderTarget::Pieces);
		break;

	case SW_VIEW_TOGGLE_PIECES_CONDITIONAL_LINES:
		ToggleConditionalLines(swRenderTarget::Pieces);
		break;

	case SW_VIEW_TOGGLE_PREVIEW_VIEW_SPHERE:
		TogglePreviewViewSphere();
		break;

	case SW_VIEW_TOGGLE_PREVIEW_AXIS_ICON:
		TogglePreviewAxisIcon();
		break;

	case SW_FILE_TOGGLE_LOAD_PIECES_BEFORE_SHOWING:
		ToggleLoadPiecesBeforeShowing();
		break;

	case SW_VIEW_PREVIEW_TOGGLE_SQUARE:
		if (mPreviewWidget)
			mPreviewWidget->SetSquareViewport(!swGetPreferences().mPreviewSquareViewport);
		break;

	case SW_PIECE_INSERT:
		if (ActiveModel)
			ActiveModel->AddPiece(nullptr, SW_PIECE_SECTION_INVALID);
		break;

	case SW_PIECE_TOGGLE_FIRST_AT_ORIGIN:
		ToggleFirstPieceAtOrigin();
		break;

	case SW_PIECE_DELETE:
		if (ActiveModel)
			ActiveModel->DeleteSelectedObjects();
		break;

	case SW_PIECE_DUPLICATE:
		if (ActiveModel)
			ActiveModel->DuplicateSelectedPieces();
		break;

	case SW_PIECE_PAINT_SELECTED:
		if (ActiveModel)
			ActiveModel->PaintSelectedPieces();
		break;

	case SW_PIECE_RESET_PIVOT_POINT:
		if (ActiveModel)
			ActiveModel->ResetSelectedPiecesPivotPoint();
		break;

	case SW_PIECE_REMOVE_KEY_FRAMES:
		if (ActiveModel)
			ActiveModel->RemoveSelectedObjectsKeyFrames();
		break;

	case SW_PIECE_CONTROL_POINT_INSERT:
		if (ActiveModel)
			ActiveModel->InsertControlPoint();
		break;

	case SW_PIECE_CONTROL_POINT_REMOVE:
		if (ActiveModel)
			ActiveModel->RemoveFocusedControlPoint();
		break;

	case SW_PIECE_TRAIN_TRACK_FOCUS_NEXT:
		if (ActiveModel)
			ActiveModel->FocusNextTrainTrack();
		break;

	case SW_PIECE_TRAIN_TRACK_FOCUS_PREVIOUS:
		if (ActiveModel)
			ActiveModel->FocusPreviousTrainTrack();
		break;

	case SW_PIECE_TRAIN_TRACK_ROTATE:
		if (ActiveModel)
			ActiveModel->RotateFocusedTrainTrack(1);
		break;

	case SW_PIECE_MOVE_PLUS_X:
		if (ActiveModel)
			ActiveModel->MoveSelectedObjects(ActiveView->GetMoveDirection(swVector3(swMaximum(GetMoveXYSnap(), 0.1f), 0.0f, 0.0f)), swModelTransformFlag::Relative | swModelTransformFlag::Checkpoint | swModelTransformFlag::FirstMove, swModelHistoryEditMerge::KeyboardMove);
		break;

	case SW_PIECE_MOVE_MINUS_X:
		if (ActiveModel)
			ActiveModel->MoveSelectedObjects(ActiveView->GetMoveDirection(swVector3(-swMaximum(GetMoveXYSnap(), 0.1f), 0.0f, 0.0f)), swModelTransformFlag::Relative | swModelTransformFlag::Checkpoint | swModelTransformFlag::FirstMove, swModelHistoryEditMerge::KeyboardMove);
		break;

	case SW_PIECE_MOVE_PLUS_Y:
		if (ActiveModel)
			ActiveModel->MoveSelectedObjects(ActiveView->GetMoveDirection(swVector3(0.0f, swMaximum(GetMoveXYSnap(), 0.1f), 0.0f)), swModelTransformFlag::Relative | swModelTransformFlag::Checkpoint | swModelTransformFlag::FirstMove, swModelHistoryEditMerge::KeyboardMove);
		break;

	case SW_PIECE_MOVE_MINUS_Y:
		if (ActiveModel)
			ActiveModel->MoveSelectedObjects(ActiveView->GetMoveDirection(swVector3(0.0f, -swMaximum(GetMoveXYSnap(), 0.1f), 0.0f)), swModelTransformFlag::Relative | swModelTransformFlag::Checkpoint | swModelTransformFlag::FirstMove, swModelHistoryEditMerge::KeyboardMove);
		break;

	case SW_PIECE_MOVE_PLUS_Z:
		if (ActiveModel)
			ActiveModel->MoveSelectedObjects(ActiveView->GetMoveDirection(swVector3(0.0f, 0.0f, swMaximum(GetMoveZSnap(), 0.1f))), swModelTransformFlag::Relative | swModelTransformFlag::Checkpoint | swModelTransformFlag::FirstMove, swModelHistoryEditMerge::KeyboardMove);
		break;

	case SW_PIECE_MOVE_MINUS_Z:
		if (ActiveModel)
			ActiveModel->MoveSelectedObjects(ActiveView->GetMoveDirection(swVector3(0.0f, 0.0f, -swMaximum(GetMoveZSnap(), 0.1f))), swModelTransformFlag::Relative | swModelTransformFlag::Checkpoint | swModelTransformFlag::FirstMove, swModelHistoryEditMerge::KeyboardMove);
		break;

	case SW_PIECE_ROTATE_PLUS_X:
		if (ActiveModel)
			ActiveModel->RotateSelectedObjects(ActiveView->GetMoveDirection(swVector3(swMaximum(GetAngleSnap(), 1.0f), 0.0f, 0.0f)), swModelTransformFlag::Relative | swModelTransformFlag::Checkpoint, swModelHistoryEditMerge::KeyboardRotate);
		break;

	case SW_PIECE_ROTATE_MINUS_X:
		if (ActiveModel)
			ActiveModel->RotateSelectedObjects(ActiveView->GetMoveDirection(swVector3(-swMaximum(GetAngleSnap(), 1.0f), 0.0f, 0.0f)), swModelTransformFlag::Relative | swModelTransformFlag::Checkpoint, swModelHistoryEditMerge::KeyboardRotate);
		break;

	case SW_PIECE_ROTATE_PLUS_Y:
		if (ActiveModel)
			ActiveModel->RotateSelectedObjects(ActiveView->GetMoveDirection(swVector3(0.0f, swMaximum(GetAngleSnap(), 1.0f), 0.0f)), swModelTransformFlag::Relative | swModelTransformFlag::Checkpoint, swModelHistoryEditMerge::KeyboardRotate);
		break;

	case SW_PIECE_ROTATE_MINUS_Y:
		if (ActiveModel)
			ActiveModel->RotateSelectedObjects(ActiveView->GetMoveDirection(swVector3(0.0f, -swMaximum(GetAngleSnap(), 1.0f), 0.0f)), swModelTransformFlag::Relative | swModelTransformFlag::Checkpoint, swModelHistoryEditMerge::KeyboardRotate);
		break;

	case SW_PIECE_ROTATE_PLUS_Z:
		if (ActiveModel)
			ActiveModel->RotateSelectedObjects(ActiveView->GetMoveDirection(swVector3(0.0f, 0.0f, swMaximum(GetAngleSnap(), 1.0f))), swModelTransformFlag::Relative | swModelTransformFlag::Checkpoint, swModelHistoryEditMerge::KeyboardRotate);
		break;

	case SW_PIECE_ROTATE_MINUS_Z:
		if (ActiveModel)
			ActiveModel->RotateSelectedObjects(ActiveView->GetMoveDirection(swVector3(0.0f, 0.0f, -swMaximum(GetAngleSnap(), 1.0f))), swModelTransformFlag::Relative | swModelTransformFlag::Checkpoint, swModelHistoryEditMerge::KeyboardRotate);
		break;

	case SW_PIECE_MINIFIGURE_BUILDER:
		mMinifigureBuilderToolBar->show();
		mMinifigureBuilderToolBar->raise();
		break;

	case SW_PIECE_ARRAY:
		if (ActiveModel)
			ActiveModel->ShowArrayDialog();
		break;

	case SW_PIECE_VIEW_SELECTED_MODEL:
		SetModelFromSelection();
		break;

	case SW_PIECE_MOVE_SELECTION_TO_MODEL:
		if (ActiveModel)
			ActiveModel->MoveSelectionToModel(swGetActiveProject()->CreateNewModel(false));
		break;

	case SW_PIECE_INLINE_SELECTED_MODELS:
		if (ActiveModel)
			ActiveModel->InlineSelectedModels();
		break;

	case SW_PIECE_EDIT_END_SUBMODEL:
		if (ActiveView)
			ActiveView->SetTopSubmodelActive();
		break;

	case SW_PIECE_EDIT_SELECTED_SUBMODEL:
		if (ActiveView)
			ActiveView->SetSelectedSubmodelActive();
		break;

	case SW_PIECE_GROUP:
		if (ActiveModel)
			ActiveModel->GroupSelection();
		break;

	case SW_PIECE_UNGROUP:
		if (ActiveModel)
			ActiveModel->UngroupSelection();
		break;

	case SW_PIECE_GROUP_ADD:
		if (ActiveModel)
			ActiveModel->AddSelectedPiecesToGroup();
		break;

	case SW_PIECE_GROUP_REMOVE:
		if (ActiveModel)
			ActiveModel->RemoveFocusPieceFromGroup();
		break;

	case SW_PIECE_GROUP_EDIT:
		if (ActiveModel)
			ActiveModel->ShowEditGroupsDialog();
		break;

	case SW_PIECE_HIDE_SELECTED:
		if (ActiveModel)
			ActiveModel->HideSelectedPieces();
		break;

	case SW_PIECE_HIDE_UNSELECTED:
		if (ActiveModel)
			ActiveModel->HideUnselectedPieces();
		break;

	case SW_PIECE_UNHIDE_SELECTED:
		if (ActiveModel)
			ActiveModel->UnhideSelectedPieces();
		break;

	case SW_PIECE_UNHIDE_ALL:
		if (ActiveModel)
			ActiveModel->UnhideAllPieces();
		break;

	case SW_PIECE_SHOW_EARLIER:
		if (ActiveModel)
			ActiveModel->ShowSelectedPiecesEarlier();
		break;

	case SW_PIECE_SHOW_LATER:
		if (ActiveModel)
			ActiveModel->ShowSelectedPiecesLater();
		break;

	case SW_VIEW_PREFERENCES:
		gApplication->ShowPreferencesDialog();
		break;

	case SW_VIEW_ZOOM_IN:
		if (ActiveView)
			ActiveView->Zoom(10.0f);
		break;

	case SW_VIEW_ZOOM_OUT:
		if (ActiveView)
			ActiveView->Zoom(-10.0f);
		break;

	case SW_VIEW_ZOOM_EXTENTS:
		if (ActiveView)
			ActiveView->ZoomExtents(swGeometryBoundsMode::IncludeFallback, false);
		break;

	case SW_VIEW_LOOK_AT:
		if (ActiveView)
			ActiveView->LookAt();
		break;

	case SW_VIEW_MOVE_FORWARD:
		if (ActiveView)
			ActiveView->MoveCamera(swVector3(0.0f, 0.0f, -1.0f));
		break;

	case SW_VIEW_MOVE_BACKWARD:
		if (ActiveView)
			ActiveView->MoveCamera(swVector3(0.0f, 0.0f, 1.0f));
		break;

	case SW_VIEW_MOVE_LEFT:
		if (ActiveView)
			ActiveView->MoveCamera(swVector3(-1.0f, 0.0f, 0.0f));
		break;

	case SW_VIEW_MOVE_RIGHT:
		if (ActiveView)
			ActiveView->MoveCamera(swVector3(1.0f, 0.0f, 0.0f));
		break;

	case SW_VIEW_MOVE_UP:
		if (ActiveView)
			ActiveView->MoveCamera(swVector3(0.0f, 1.0f, 0.0f));
		break;

	case SW_VIEW_MOVE_DOWN:
		if (ActiveView)
			ActiveView->MoveCamera(swVector3(0.0f, -1.0f, 0.0f));
		break;

	case SW_VIEW_TIME_NEXT:
		if (ActiveModel)
			ActiveModel->ShowNextStep();
		break;

	case SW_VIEW_TIME_PREVIOUS:
		if (ActiveModel)
			ActiveModel->ShowPreviousStep();
		break;

	case SW_VIEW_TIME_FIRST:
		if (ActiveModel)
			ActiveModel->ShowFirstStep();
		break;

	case SW_VIEW_TIME_LAST:
		if (ActiveModel)
			ActiveModel->ShowLastStep();
		break;

	case SW_VIEW_TIME_INSERT_BEFORE:
		if (ActiveModel)
			ActiveModel->InsertStepAction(ActiveModel->GetCurrentStep());
		break;

	case SW_VIEW_TIME_INSERT_AFTER:
		if (ActiveModel)
			ActiveModel->InsertStepAction(ActiveModel->GetCurrentStep() + 1);
		break;

	case SW_VIEW_TIME_DELETE:
		if (ActiveModel)
			ActiveModel->RemoveStepAction(ActiveModel->GetCurrentStep());
		break;

	case SW_VIEW_VIEWPOINT_FRONT:
		if (ActiveView)
			ActiveView->SetViewpoint(swViewpoint::Front);
		break;

	case SW_VIEW_VIEWPOINT_BACK:
		if (ActiveView)
			ActiveView->SetViewpoint(swViewpoint::Back);
		break;

	case SW_VIEW_VIEWPOINT_TOP:
		if (ActiveView)
			ActiveView->SetViewpoint(swViewpoint::Top);
		break;

	case SW_VIEW_VIEWPOINT_BOTTOM:
		if (ActiveView)
			ActiveView->SetViewpoint(swViewpoint::Bottom);
		break;

	case SW_VIEW_VIEWPOINT_LEFT:
		if (ActiveView)
			ActiveView->SetViewpoint(swViewpoint::Left);
		break;

	case SW_VIEW_VIEWPOINT_RIGHT:
		if (ActiveView)
			ActiveView->SetViewpoint(swViewpoint::Right);
		break;

	case SW_VIEW_VIEWPOINT_HOME:
		if (ActiveView)
			ActiveView->SetViewpoint(swViewpoint::Home);
		break;

	case SW_VIEW_CAMERA_NONE:
		if (ActiveView)
			ActiveView->RemoveCamera();
		break;

	case SW_VIEW_CAMERA1:
	case SW_VIEW_CAMERA2:
	case SW_VIEW_CAMERA3:
	case SW_VIEW_CAMERA4:
	case SW_VIEW_CAMERA5:
	case SW_VIEW_CAMERA6:
	case SW_VIEW_CAMERA7:
	case SW_VIEW_CAMERA8:
	case SW_VIEW_CAMERA9:
	case SW_VIEW_CAMERA10:
	case SW_VIEW_CAMERA11:
	case SW_VIEW_CAMERA12:
	case SW_VIEW_CAMERA13:
	case SW_VIEW_CAMERA14:
	case SW_VIEW_CAMERA15:
	case SW_VIEW_CAMERA16:
		if (ActiveView)
			ActiveView->SetCameraIndex(CommandId - SW_VIEW_CAMERA1);
		break;

	case SW_MODEL_NEW:
		swGetActiveProject()->CreateNewModel(true);
		break;

	case SW_MODEL_PROPERTIES:
		if (ActiveModel)
			ActiveModel->ShowPropertiesDialog();
		break;

	case SW_MODEL_LIST:
		swGetActiveProject()->ShowModelListDialog();
		break;

	case SW_MODEL_01:
	case SW_MODEL_02:
	case SW_MODEL_03:
	case SW_MODEL_04:
	case SW_MODEL_05:
	case SW_MODEL_06:
	case SW_MODEL_07:
	case SW_MODEL_08:
	case SW_MODEL_09:
	case SW_MODEL_10:
	case SW_MODEL_11:
	case SW_MODEL_12:
	case SW_MODEL_13:
	case SW_MODEL_14:
	case SW_MODEL_15:
	case SW_MODEL_16:
	case SW_MODEL_17:
	case SW_MODEL_18:
	case SW_MODEL_19:
	case SW_MODEL_20:
	case SW_MODEL_21:
	case SW_MODEL_22:
	case SW_MODEL_23:
	case SW_MODEL_24:
	case SW_MODEL_25:
	case SW_MODEL_26:
	case SW_MODEL_27:
	case SW_MODEL_28:
	case SW_MODEL_29:
	case SW_MODEL_30:
	case SW_MODEL_31:
	case SW_MODEL_32:
	case SW_MODEL_33:
	case SW_MODEL_34:
	case SW_MODEL_35:
	case SW_MODEL_36:
	case SW_MODEL_37:
	case SW_MODEL_38:
	case SW_MODEL_39:
	case SW_MODEL_40:
		swGetActiveProject()->SetActiveModel(CommandId - SW_MODEL_01, true);
		break;

	case SW_HELP_WHATS_THIS:
		QWhatsThis::enterWhatsThisMode();
		break;

	case SW_HELP_HOME_PAGE:
		QDesktopServices::openUrl(QUrl("https://github.com/nathaneltitane/studworks"));
		break;

	case SW_HELP_BUG_REPORT:
		QDesktopServices::openUrl(QUrl("https://github.com/nathaneltitane/studworks/issues"));
		break;

	case SW_HELP_ABOUT:
		ShowAboutDialog();
		break;

	case SW_VIEW_TIME_ADD_KEYS:
		SetAddKeys(!GetAddKeys());
		break;

	case SW_EDIT_TRANSFORM_RELATIVE:
		SetRelativeTransform(true);
		break;

	case SW_EDIT_TRANSFORM_ABSOLUTE:
		SetRelativeTransform(false);
		break;

	case SW_EDIT_TRANSFORM_TOGGLE_RELATIVE:
		SetRelativeTransform(!GetRelativeTransform());
		break;

	case SW_EDIT_TRANSFORM_SEPARATELY:
		SetSeparateTransform(true);
		break;

	case SW_EDIT_TRANSFORM_TOGETHER:
		SetSeparateTransform(false);
		break;

	case SW_EDIT_TRANSFORM_TOGGLE_SEPARATE:
		SetSeparateTransform(!GetSeparateTransform());
		break;

	case SW_EDIT_SNAP_MOVE_TOGGLE:
		SetMoveSnapEnabled(!mMoveSnapEnabled);
		break;

	case SW_EDIT_SNAP_MOVE_XY0:
	case SW_EDIT_SNAP_MOVE_XY1:
	case SW_EDIT_SNAP_MOVE_XY2:
	case SW_EDIT_SNAP_MOVE_XY3:
	case SW_EDIT_SNAP_MOVE_XY4:
	case SW_EDIT_SNAP_MOVE_XY5:
	case SW_EDIT_SNAP_MOVE_XY6:
	case SW_EDIT_SNAP_MOVE_XY7:
	case SW_EDIT_SNAP_MOVE_XY8:
	case SW_EDIT_SNAP_MOVE_XY9:
		SetMoveXYSnapIndex(CommandId - SW_EDIT_SNAP_MOVE_XY0);
		break;

	case SW_EDIT_SNAP_MOVE_Z0:
	case SW_EDIT_SNAP_MOVE_Z1:
	case SW_EDIT_SNAP_MOVE_Z2:
	case SW_EDIT_SNAP_MOVE_Z3:
	case SW_EDIT_SNAP_MOVE_Z4:
	case SW_EDIT_SNAP_MOVE_Z5:
	case SW_EDIT_SNAP_MOVE_Z6:
	case SW_EDIT_SNAP_MOVE_Z7:
	case SW_EDIT_SNAP_MOVE_Z8:
	case SW_EDIT_SNAP_MOVE_Z9:
		SetMoveZSnapIndex(CommandId - SW_EDIT_SNAP_MOVE_Z0);
		break;

	case SW_EDIT_SNAP_ANGLE_TOGGLE:
		SetAngleSnapEnabled(!mAngleSnapEnabled);
		break;

	case SW_EDIT_SNAP_ANGLE0:
	case SW_EDIT_SNAP_ANGLE1:
	case SW_EDIT_SNAP_ANGLE2:
	case SW_EDIT_SNAP_ANGLE3:
	case SW_EDIT_SNAP_ANGLE4:
	case SW_EDIT_SNAP_ANGLE5:
	case SW_EDIT_SNAP_ANGLE6:
	case SW_EDIT_SNAP_ANGLE7:
	case SW_EDIT_SNAP_ANGLE8:
	case SW_EDIT_SNAP_ANGLE9:
		SetAngleSnapIndex(CommandId - SW_EDIT_SNAP_ANGLE0);
		break;

	case SW_EDIT_SNAP_ANGLE_CUSTOM:
		SetCustomAngleSnap();
		break;

	case SW_EDIT_SNAP_ORBIT_TOGGLE:
		mOrbitSnapEnabled = !mOrbitSnapEnabled;
		swSetProfileInteger(SW_PROFILE_ORBIT_SNAP, mOrbitSnapEnabled);
		UpdateSnap();
		break;

	case SW_EDIT_TRANSFORM:
		if (ActiveModel)
		{
			const std::optional<swVector3> Transform = GetTransformAmount();

			if (Transform)
				ActiveModel->TransformSelectedObjects(GetTransformType(), *Transform);
		}
		break;

	case SW_EDIT_TRANSFORM_ABSOLUTE_TRANSLATION:
		SetTransformType(swTransformType::AbsoluteTranslation);
		break;

	case SW_EDIT_TRANSFORM_RELATIVE_TRANSLATION:
		SetTransformType(swTransformType::RelativeTranslation);
		break;

	case SW_EDIT_TRANSFORM_ABSOLUTE_ROTATION:
		SetTransformType(swTransformType::AbsoluteRotation);
		break;

	case SW_EDIT_TRANSFORM_RELATIVE_ROTATION:
		SetTransformType(swTransformType::RelativeRotation);
		break;

	case SW_EDIT_ACTION_SELECT:
		SetTool(swTool::Select);
		break;

	case SW_EDIT_ACTION_INSERT:
		SetTool(swTool::Insert);
		break;

	case SW_EDIT_ACTION_POINT_LIGHT:
		SetTool(swTool::PointLight);
		break;

	case SW_EDIT_ACTION_AREA_LIGHT:
		SetTool(swTool::AreaLight);
		break;

	case SW_EDIT_ACTION_DIRECTIONAL_LIGHT:
		SetTool(swTool::DirectionalLight);
		break;

	case SW_EDIT_ACTION_SPOTLIGHT:
		SetTool(swTool::SpotLight);
		break;

	case SW_EDIT_ACTION_CAMERA:
		SetTool(swTool::Camera);
		break;

	case SW_EDIT_ACTION_MOVE:
		SetTool(swTool::Move);
		break;

	case SW_EDIT_ACTION_ROTATE:
		SetTool(swTool::Rotate);
		break;

	case SW_EDIT_ACTION_DELETE:
		SetTool(swTool::Eraser);
		break;

	case SW_EDIT_ACTION_PAINT:
		SetTool(swTool::Paint);
		break;

	case SW_EDIT_ACTION_COLOR_PICKER:
		SetTool(swTool::ColorPicker);
		break;

	case SW_EDIT_ACTION_ZOOM:
		SetTool(swTool::Zoom);
		break;

	case SW_EDIT_ACTION_ZOOM_REGION:
		SetTool(swTool::ZoomRegion);
		break;

	case SW_EDIT_ACTION_PAN:
		SetTool(swTool::Pan);
		break;

	case SW_EDIT_ACTION_ROTATE_VIEW:
		SetTool(swTool::RotateView);
		break;

	case SW_EDIT_ACTION_ROLL:
		SetTool(swTool::Roll);
		break;

	case SW_EDIT_CANCEL:
		if (ActiveView && !ActiveView->CloseFindReplaceDialog())
			ActiveView->CancelTrackingOrClearSelectionAction();
		break;

	case SW_TIMELINE_INSERT_BEFORE:
		mTimelineWidget->InsertStepBefore();
		break;

	case SW_TIMELINE_INSERT_AFTER:
		mTimelineWidget->InsertStepAfter();
		break;

	case SW_TIMELINE_DELETE:
		mTimelineWidget->RemoveStep();
		break;

	case SW_TIMELINE_MOVE_SELECTION:
		mTimelineWidget->MoveSelection();
		break;

	case SW_TIMELINE_MOVE_SELECTION_BEFORE:
		mTimelineWidget->MoveSelectionBefore();
		break;

	case SW_TIMELINE_MOVE_SELECTION_AFTER:
		mTimelineWidget->MoveSelectionAfter();
		break;

	case SW_TIMELINE_SET_CURRENT:
		mTimelineWidget->SetCurrentStep();
		break;

	case SW_COMMAND_COUNT:
		break;
	}
}
