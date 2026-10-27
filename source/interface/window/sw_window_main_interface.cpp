#include "sw_global.h"
#include "sw_window_main.h"
#include "sw_widget_builder_minifigure.h"
#include "sw_widget_spin_box_double.h"
#include "sw_widget_selection_piece.h"
#ifdef SW_ENABLE_TERMINAL
#include "sw_widget_terminal.h"
#endif
#include "sw_widget_properties.h"
#include "sw_widget_timeline.h"
#include "sw_view_widget.h"
#include "sw_widget_list_color.h"
#include "sw_widget_label_elided.h"
#include "sw_window_utilities.h"
#include "sw_dialog_about.h"
#include "sw_dialog_select.h"
#include "sw_dialog_database_sets.h"
#include "sw_dialog_html.h"
#include "sw_dialog_render.h"
#include "sw_dialog_instructions.h"
#include "sw_dialog_image.h"
#include "sw_profile.h"
#include "sw_view.h"
#include "sw_project.h"
#include "sw_object_piece.h"
#include "sw_object_camera.h"
#include "sw_group.h"
#include "sw_data_piece.h"
#include "sw_library.h"
#include "sw_colors.h"
#include "sw_widget_preview.h"
#include "sw_model_history.h"
#include "sw_widget_spinner.h"

#if SW_ENABLE_GAMEPAD
#include <QtGamepad/QGamepad>
#endif

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
	QMenu* ImportMenu = FileMenu->addMenu(tr("&Import"));
	ImportMenu->addAction(mActions[SW_FILE_IMPORT_LDD]);
	ImportMenu->addAction(mActions[SW_FILE_IMPORT_INVENTORY]);
	QMenu* ExportMenu = FileMenu->addMenu(tr("&Export"));
	ExportMenu->addAction(mActions[SW_FILE_EXPORT_3DS]);
	ExportMenu->addAction(mActions[SW_FILE_EXPORT_BRICKLINK]);
	ExportMenu->addAction(mActions[SW_FILE_EXPORT_COLLADA]);
	ExportMenu->addAction(mActions[SW_FILE_EXPORT_CSV]);
	ExportMenu->addAction(mActions[SW_FILE_EXPORT_HTML]);
	ExportMenu->addAction(mActions[SW_FILE_SAVE_IMAGE]);
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
	FileMenu->addSeparator();
	FileMenu->addAction(mActions[SW_FILE_INSTRUCTIONS]);
	FileMenu->addAction(mActions[SW_MODEL_PROPERTIES]);
	FileMenu->addSeparator();
	FileMenu->addAction(mActions[SW_FILE_RECENT1]);
	FileMenu->addAction(mActions[SW_FILE_RECENT2]);
	FileMenu->addAction(mActions[SW_FILE_RECENT3]);
	FileMenu->addAction(mActions[SW_FILE_RECENT4]);
	mActionFileRecentSeparator = FileMenu->addSeparator();
	FileMenu->addAction(mActions[SW_FILE_EXIT]);

	QMenu* EditMenu = menuBar()->addMenu(tr("&Edit"));
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
	EditMenu->addSeparator();
	EditMenu->addAction(mActions[SW_VIEW_PREFERENCES]);

	QMenu* ViewMenu = menuBar()->addMenu(tr("&View"));
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
	ViewMenu->addSeparator();
	ViewMenu->addAction(mActions[SW_VIEW_ZOOM_EXTENTS]);
	ViewMenu->addAction(mActions[SW_VIEW_LOOK_AT]);
	ViewMenu->addMenu(mViewpointMenu);
	ViewMenu->addMenu(mCameraMenu);
	ViewMenu->addMenu(mProjectionMenu);
	ViewMenu->addMenu(mShadingMenu);
	ViewMenu->addSeparator();
	ViewMenu->addAction(mActions[SW_VIEW_SPLIT_HORIZONTAL]);
	ViewMenu->addAction(mActions[SW_VIEW_SPLIT_VERTICAL]);
	ViewMenu->addAction(mActions[SW_VIEW_REMOVE_VIEW]);
	ViewMenu->addAction(mActions[SW_VIEW_RESET_VIEWS]);
	ViewMenu->addSeparator();
	ViewMenu->addAction(mActions[SW_VIEW_FULL_SCREEN]);

	QMenu* PieceMenu = menuBar()->addMenu(tr("&Piece"));
	PieceMenu->addAction(mActions[SW_PIECE_INSERT]);
	PieceMenu->addAction(mActions[SW_PIECE_DELETE]);
	PieceMenu->addAction(mActions[SW_PIECE_DUPLICATE]);
	PieceMenu->addSeparator();
	PieceMenu->addAction(mActions[SW_PIECE_PAINT_SELECTED]);
	PieceMenu->addAction(mActions[SW_PIECE_ARRAY]);
	PieceMenu->addAction(mActions[SW_PIECE_RESET_PIVOT_POINT]);
	PieceMenu->addAction(mActions[SW_PIECE_REMOVE_KEY_FRAMES]);
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

	QMenu* StepMenu = menuBar()->addMenu(tr("&Steps"));
	StepMenu->addAction(mActions[SW_VIEW_TIME_FIRST]);
	StepMenu->addAction(mActions[SW_VIEW_TIME_PREVIOUS]);
	StepMenu->addAction(mActions[SW_VIEW_TIME_NEXT]);
	StepMenu->addAction(mActions[SW_VIEW_TIME_LAST]);
	StepMenu->addSeparator();
	StepMenu->addAction(mActions[SW_VIEW_TIME_INSERT_BEFORE]);
	StepMenu->addAction(mActions[SW_VIEW_TIME_INSERT_AFTER]);
	StepMenu->addAction(mActions[SW_VIEW_TIME_DELETE]);

	QMenu* ModelMenu = menuBar()->addMenu(tr("&Models"));
	ModelMenu->addAction(mActions[SW_PIECE_MINIFIGURE_BUILDER]);
	ModelMenu->addSeparator();
	ModelMenu->addAction(mActions[SW_MODEL_NEW]);
	ModelMenu->addAction(mActions[SW_MODEL_LIST]);
	ModelMenu->addSeparator();
	ModelMenu->addAction(mActions[SW_PIECE_EDIT_SELECTED_SUBMODEL]);
	ModelMenu->addAction(mActions[SW_PIECE_EDIT_END_SUBMODEL]);
	ModelMenu->addAction(mActions[SW_PIECE_VIEW_SELECTED_MODEL]);
	ModelMenu->addAction(mActions[SW_PIECE_INLINE_SELECTED_MODELS]);
	ModelMenu->addAction(mActions[SW_PIECE_MOVE_SELECTION_TO_MODEL]);
	ModelMenu->addSeparator();
	for (int ModelIdx = SW_MODEL_FIRST; ModelIdx <= SW_MODEL_LAST; ModelIdx++)
		ModelMenu->addAction(mActions[ModelIdx]);

	QMenu* HelpMenu = menuBar()->addMenu(tr("&Help"));
	HelpMenu->addAction(mActions[SW_HELP_WHATS_THIS]);
	HelpMenu->addSeparator();
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
