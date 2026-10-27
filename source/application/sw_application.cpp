#include "sw_global.h"
#include <stdio.h>
#include "sw_application.h"
#include "sw_library.h"
#include "sw_profile.h"
#include "sw_project.h"
#include "sw_window_main.h"
#include "sw_dialog_preferences.h"
#include "sw_widget_selection_part.h"
#include "sw_shortcuts.h"
#include "sw_view.h"
#include "sw_object_camera.h"
#include "sw_widget_preview.h"
#include "sw_colors.h"
#include "sw_window_utilities.h"

#ifdef Q_OS_WIN
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
#include <QtPlatformHeaders\QWindowsWindowFunctions>
#endif
#endif

swApplication* gApplication;

void swPreferences::LoadDefaults()
{
	mFixedAxes = swGetProfileInteger(SW_PROFILE_FIXED_AXES);
	mMouseSensitivity = swGetProfileInteger(SW_PROFILE_MOUSE_SENSITIVITY);
	mShadingMode = swGetProfileShadingMode();
	mLightingStyle = swGetProfileLightingStyle();
	mHighlightRenderMode = swGetProfileHighlightRenderMode();
	mSelectionOutlineWidth = qBound(SW_SELECTION_OUTLINE_WIDTH_MINIMUM, swGetProfileInteger(SW_PROFILE_SELECTION_OUTLINE_WIDTH), SW_SELECTION_OUTLINE_WIDTH_MAXIMUM);
	mBackgroundSolidColor = swGetProfileUnsignedInteger(SW_PROFILE_BACKGROUND_COLOR);
	mDrawAxes = swGetProfileInteger(SW_PROFILE_DRAW_AXES);
	mFollowStepViews = swGetProfileInteger(SW_PROFILE_FOLLOW_STEP_VIEWS);
	mAxisIconLocation = swGetProfileAxisIconLocation();
	mTextColor = swGetProfileUnsignedInteger(SW_PROFILE_TEXT_COLOR);
	mTextHaloColor = swGetProfileUnsignedInteger(SW_PROFILE_TEXT_HALO_COLOR);
	mMarqueeBorderColor = swGetProfileUnsignedInteger(SW_PROFILE_MARQUEE_BORDER_COLOR);
	mMarqueeFillColor = swGetProfileUnsignedInteger(SW_PROFILE_MARQUEE_FILL_COLOR);
	mOverlayColor = swGetProfileUnsignedInteger(SW_PROFILE_OVERLAY_COLOR);
	mActiveViewColor = swGetProfileUnsignedInteger(SW_PROFILE_ACTIVE_VIEW_COLOR);
	mInactiveViewColor = swGetProfileUnsignedInteger(SW_PROFILE_INACTIVE_VIEW_COLOR);
	mDrawEdgeLines = swGetProfileInteger(SW_PROFILE_DRAW_EDGE_LINES);
	mDrawConditionalLines = swGetProfileInteger(SW_PROFILE_DRAW_CONDITIONAL_LINES);
	mLineWidth = swGetProfileFloat(SW_PROFILE_LINE_WIDTH);
	LoadRenderTarget(swRenderTarget::Preview);
	LoadRenderTarget(swRenderTarget::Parts);
	LoadRenderTarget(swRenderTarget::MinifigureBuilder);
	mAllowLOD = swGetProfileInteger(SW_PROFILE_ALLOW_LOD);
	mMeshLODDistance = swGetProfileFloat(SW_PROFILE_LOD_DISTANCE);
	mFadeSteps = swGetProfileInteger(SW_PROFILE_FADE_STEPS);
	mFadeStepsColor = swGetProfileUnsignedInteger(SW_PROFILE_FADE_STEPS_COLOR);
	mHighlightNewParts = swGetProfileInteger(SW_PROFILE_HIGHLIGHT_NEW_PARTS);
	mHighlightNewPartsColor = swGetProfileUnsignedInteger(SW_PROFILE_HIGHLIGHT_NEW_PARTS_COLOR);
	mDrawGridStuds = swGetProfileInteger(SW_PROFILE_GRID_STUDS);
	mGridStudColor = swGetProfileUnsignedInteger(SW_PROFILE_GRID_STUD_COLOR);
	mDrawGridLines = swGetProfileInteger(SW_PROFILE_GRID_LINES);
	mGridLineSpacing = swGetProfileInteger(SW_PROFILE_GRID_LINE_SPACING);
	mGridLineColor = swGetProfileUnsignedInteger(SW_PROFILE_GRID_LINE_COLOR);
	mDrawGridOrigin = swGetProfileInteger(SW_PROFILE_GRID_ORIGIN);
	mDrawGridHeightXZ = swGetProfileInteger(SW_PROFILE_GRID_HEIGHT_XZ);
	mDrawGridHeightYZ = swGetProfileInteger(SW_PROFILE_GRID_HEIGHT_YZ);
	mGridHeightBehindModel = swGetProfileInteger(SW_PROFILE_GRID_HEIGHT_BEHIND_MODEL);
	mViewSphereEnabled = swGetProfileInteger(SW_PROFILE_VIEW_SPHERE_ENABLED);
	mViewSphereLocation = swGetProfileViewSphereLocation();
	mViewSphereSize = swGetProfileInteger(SW_PROFILE_VIEW_SPHERE_SIZE);
	mViewSphereColor = swGetProfileUnsignedInteger(SW_PROFILE_VIEW_SPHERE_COLOR);
	mViewSphereTextColor = swGetProfileUnsignedInteger(SW_PROFILE_VIEW_SPHERE_TEXT_COLOR);
	mViewSphereHighlightColor = swGetProfileUnsignedInteger(SW_PROFILE_VIEW_SPHERE_HIGHLIGHT_COLOR);
	mAxisXColor = swGetProfileUnsignedInteger(SW_PROFILE_AXIS_X_COLOR);
	mAxisYColor = swGetProfileUnsignedInteger(SW_PROFILE_AXIS_Y_COLOR);
	mAxisZColor = swGetProfileUnsignedInteger(SW_PROFILE_AXIS_Z_COLOR);
	mAutoLoadMostRecent = swGetProfileInteger(SW_PROFILE_AUTO_LOAD_MOST_RECENT);
	mRestoreTabLayout = swGetProfileInteger(SW_PROFILE_RESTORE_TAB_LAYOUT);
	mFirstPartAtOrigin = swGetProfileInteger(SW_PROFILE_FIRST_PART_AT_ORIGIN);
	mPartsSnapToTiles = swGetProfileInteger(SW_PROFILE_PARTS_SNAP_TO_TILES);
	mTerminalFollowModel = swGetProfileInteger(SW_PROFILE_TERMINAL_FOLLOW_MODEL);
	mInsertColorMode = static_cast<swInsertColorMode>(qBound(0, swGetProfileInteger(SW_PROFILE_INSERT_COLOR_MODE), static_cast<int>(swInsertColorMode::LastInsertedColor)));
	mWatchFileChanges = swGetProfileInteger(SW_PROFILE_WATCH_FILE_CHANGES);
	mLoadPartsBeforeShowing = swGetProfileInteger(SW_PROFILE_LOAD_PARTS_BEFORE_SHOWING);
	mColorTheme = swGetProfileColorTheme();
	mPreviewViewSphereEnabled = swGetProfileInteger(SW_PROFILE_PREVIEW_VIEW_SPHERE_ENABLED);
	mPreviewViewSphereSize = swGetProfileInteger(SW_PROFILE_PREVIEW_VIEW_SPHERE_SIZE);
	mPreviewViewSphereLocation = swGetProfilePreviewViewSphereLocation();
	mDrawPreviewAxis = swGetProfileInteger(SW_PROFILE_PREVIEW_DRAW_AXES);
	mPreviewSquareViewport = swGetProfileInteger(SW_PROFILE_PREVIEW_SQUARE_VIEWPORT);
	mStudCylinderColorEnabled = swGetProfileInteger(SW_PROFILE_STUD_CYLINDER_COLOR_ENABLED);
	mStudCylinderColor = swGetProfileUnsignedInteger(SW_PROFILE_STUD_CYLINDER_COLOR);
	mPartEdgeColorEnabled = swGetProfileInteger(SW_PROFILE_PART_EDGE_COLOR_ENABLED);
	mPartEdgeColor = swGetProfileUnsignedInteger(SW_PROFILE_PART_EDGE_COLOR);
	mBlackEdgeColorEnabled = swGetProfileInteger(SW_PROFILE_BLACK_EDGE_COLOR_ENABLED);
	mBlackEdgeColor = swGetProfileUnsignedInteger(SW_PROFILE_BLACK_EDGE_COLOR);
	mDarkEdgeColorEnabled = swGetProfileInteger(SW_PROFILE_DARK_EDGE_COLOR_ENABLED);
	mDarkEdgeColor = swGetProfileUnsignedInteger(SW_PROFILE_DARK_EDGE_COLOR);
	mPartEdgeContrast = swGetProfileFloat(SW_PROFILE_PART_EDGE_CONTRAST);
	mPartColorValueLDIndex = swGetProfileFloat(SW_PROFILE_PART_COLOR_VALUE_LD_INDEX);
	mAutomateEdgeColor = swGetProfileInteger(SW_PROFILE_AUTOMATE_EDGE_COLOR);
	mObjectSelectedColor = swGetProfileUnsignedInteger(SW_PROFILE_OBJECT_SELECTED_COLOR);
	mObjectFocusedColor = swGetProfileUnsignedInteger(SW_PROFILE_OBJECT_FOCUSED_COLOR);
	mCameraColor = swGetProfileUnsignedInteger(SW_PROFILE_CAMERA_COLOR);
	mLightColor = swGetProfileUnsignedInteger(SW_PROFILE_LIGHT_COLOR);
	mControlPointColor = swGetProfileUnsignedInteger(SW_PROFILE_CONTROL_POINT_COLOR);
	mControlPointFocusedColor = swGetProfileUnsignedInteger(SW_PROFILE_CONTROL_POINT_FOCUSED_COLOR);
}

void swPreferences::SaveDefaults()
{
	swSetProfileInteger(SW_PROFILE_FIXED_AXES, mFixedAxes);
	swSetProfileInteger(SW_PROFILE_MOUSE_SENSITIVITY, mMouseSensitivity);
	swSetProfileShadingMode(mShadingMode);
	swSetProfileLightingStyle(mLightingStyle);
	swSetProfileHighlightRenderMode(mHighlightRenderMode);
	swSetProfileInteger(SW_PROFILE_SELECTION_OUTLINE_WIDTH, mSelectionOutlineWidth);
	swSetProfileInteger(SW_PROFILE_DRAW_AXES, mDrawAxes);
	swSetProfileInteger(SW_PROFILE_FOLLOW_STEP_VIEWS, mFollowStepViews);
	swSetProfileAxisIconLocation(mAxisIconLocation);
	swSetProfileUnsignedInteger(SW_PROFILE_TEXT_COLOR, mTextColor);
	swSetProfileUnsignedInteger(SW_PROFILE_TEXT_HALO_COLOR, mTextHaloColor);
	swSetProfileUnsignedInteger(SW_PROFILE_BACKGROUND_COLOR, mBackgroundSolidColor);
	swSetProfileUnsignedInteger(SW_PROFILE_MARQUEE_BORDER_COLOR, mMarqueeBorderColor);
	swSetProfileUnsignedInteger(SW_PROFILE_MARQUEE_FILL_COLOR, mMarqueeFillColor);
	swSetProfileUnsignedInteger(SW_PROFILE_OVERLAY_COLOR, mOverlayColor);
	swSetProfileUnsignedInteger(SW_PROFILE_ACTIVE_VIEW_COLOR, mActiveViewColor);
	swSetProfileUnsignedInteger(SW_PROFILE_INACTIVE_VIEW_COLOR, mInactiveViewColor);
	swSetProfileInteger(SW_PROFILE_DRAW_EDGE_LINES, mDrawEdgeLines);
	swSetProfileInteger(SW_PROFILE_DRAW_CONDITIONAL_LINES, mDrawConditionalLines);
	swSetProfileFloat(SW_PROFILE_LINE_WIDTH, mLineWidth);
	SaveRenderTarget(swRenderTarget::Preview);
	SaveRenderTarget(swRenderTarget::Parts);
	SaveRenderTarget(swRenderTarget::MinifigureBuilder);
	swSetProfileInteger(SW_PROFILE_ALLOW_LOD, mAllowLOD);
	swSetProfileFloat(SW_PROFILE_LOD_DISTANCE, mMeshLODDistance);
	swSetProfileInteger(SW_PROFILE_FADE_STEPS, mFadeSteps);
	swSetProfileUnsignedInteger(SW_PROFILE_FADE_STEPS_COLOR, mFadeStepsColor);
	swSetProfileInteger(SW_PROFILE_HIGHLIGHT_NEW_PARTS, mHighlightNewParts);
	swSetProfileUnsignedInteger(SW_PROFILE_HIGHLIGHT_NEW_PARTS_COLOR, mHighlightNewPartsColor);
	swSetProfileInteger(SW_PROFILE_GRID_STUDS, mDrawGridStuds);
	swSetProfileUnsignedInteger(SW_PROFILE_GRID_STUD_COLOR, mGridStudColor);
	swSetProfileInteger(SW_PROFILE_GRID_LINES, mDrawGridLines);
	swSetProfileInteger(SW_PROFILE_GRID_LINE_SPACING, mGridLineSpacing);
	swSetProfileUnsignedInteger(SW_PROFILE_GRID_LINE_COLOR, mGridLineColor);
	swSetProfileInteger(SW_PROFILE_GRID_ORIGIN, mDrawGridOrigin);
	swSetProfileInteger(SW_PROFILE_GRID_HEIGHT_XZ, mDrawGridHeightXZ);
	swSetProfileInteger(SW_PROFILE_GRID_HEIGHT_YZ, mDrawGridHeightYZ);
	swSetProfileInteger(SW_PROFILE_GRID_HEIGHT_BEHIND_MODEL, mGridHeightBehindModel);
	swSetProfileInteger(SW_PROFILE_VIEW_SPHERE_ENABLED, mViewSphereEnabled);
	swSetProfileViewSphereLocation(mViewSphereLocation);
	swSetProfileInteger(SW_PROFILE_VIEW_SPHERE_SIZE, mViewSphereSize);
	swSetProfileUnsignedInteger(SW_PROFILE_VIEW_SPHERE_COLOR, mViewSphereColor);
	swSetProfileUnsignedInteger(SW_PROFILE_VIEW_SPHERE_TEXT_COLOR, mViewSphereTextColor);
	swSetProfileUnsignedInteger(SW_PROFILE_VIEW_SPHERE_HIGHLIGHT_COLOR, mViewSphereHighlightColor);
	swSetProfileUnsignedInteger(SW_PROFILE_AXIS_X_COLOR, mAxisXColor);
	swSetProfileUnsignedInteger(SW_PROFILE_AXIS_Y_COLOR, mAxisYColor);
	swSetProfileUnsignedInteger(SW_PROFILE_AXIS_Z_COLOR, mAxisZColor);
	swSetProfileInteger(SW_PROFILE_AUTO_LOAD_MOST_RECENT, mAutoLoadMostRecent);
	swSetProfileInteger(SW_PROFILE_RESTORE_TAB_LAYOUT, mRestoreTabLayout);
	swSetProfileInteger(SW_PROFILE_FIRST_PART_AT_ORIGIN, mFirstPartAtOrigin);
	swSetProfileInteger(SW_PROFILE_PARTS_SNAP_TO_TILES, mPartsSnapToTiles);
	swSetProfileInteger(SW_PROFILE_TERMINAL_FOLLOW_MODEL, mTerminalFollowModel);
	swSetProfileInteger(SW_PROFILE_INSERT_COLOR_MODE, static_cast<int>(mInsertColorMode));
	swSetProfileInteger(SW_PROFILE_WATCH_FILE_CHANGES, mWatchFileChanges);
	swSetProfileInteger(SW_PROFILE_LOAD_PARTS_BEFORE_SHOWING, mLoadPartsBeforeShowing);
	swSetProfileColorTheme(mColorTheme);
	swSetProfileInteger(SW_PROFILE_PREVIEW_VIEW_SPHERE_ENABLED, mPreviewViewSphereEnabled);
	swSetProfileInteger(SW_PROFILE_PREVIEW_VIEW_SPHERE_SIZE, mPreviewViewSphereSize);
	swSetProfilePreviewViewSphereLocation(mPreviewViewSphereLocation);
	swSetProfileInteger(SW_PROFILE_PREVIEW_DRAW_AXES, mDrawPreviewAxis);
	swSetProfileInteger(SW_PROFILE_PREVIEW_SQUARE_VIEWPORT, mPreviewSquareViewport);
	swSetProfileInteger(SW_PROFILE_STUD_CYLINDER_COLOR_ENABLED, mStudCylinderColorEnabled);
	swSetProfileUnsignedInteger(SW_PROFILE_STUD_CYLINDER_COLOR, mStudCylinderColor);
	swSetProfileInteger(SW_PROFILE_PART_EDGE_COLOR_ENABLED, mPartEdgeColorEnabled);
	swSetProfileUnsignedInteger(SW_PROFILE_PART_EDGE_COLOR, mPartEdgeColor);
	swSetProfileInteger(SW_PROFILE_BLACK_EDGE_COLOR_ENABLED, mBlackEdgeColorEnabled);
	swSetProfileUnsignedInteger(SW_PROFILE_BLACK_EDGE_COLOR, mBlackEdgeColor);
	swSetProfileInteger(SW_PROFILE_DARK_EDGE_COLOR_ENABLED, mDarkEdgeColorEnabled);
	swSetProfileUnsignedInteger(SW_PROFILE_DARK_EDGE_COLOR, mDarkEdgeColor);
	swSetProfileFloat(SW_PROFILE_PART_EDGE_CONTRAST, mPartEdgeContrast);
	swSetProfileFloat(SW_PROFILE_PART_COLOR_VALUE_LD_INDEX, mPartColorValueLDIndex);
	swSetProfileInteger(SW_PROFILE_AUTOMATE_EDGE_COLOR, mAutomateEdgeColor);
	swSetProfileUnsignedInteger(SW_PROFILE_OBJECT_SELECTED_COLOR, mObjectSelectedColor);
	swSetProfileUnsignedInteger(SW_PROFILE_OBJECT_FOCUSED_COLOR, mObjectFocusedColor);
	swSetProfileUnsignedInteger(SW_PROFILE_CAMERA_COLOR, mCameraColor);
	swSetProfileUnsignedInteger(SW_PROFILE_LIGHT_COLOR, mLightColor);
	swSetProfileUnsignedInteger(SW_PROFILE_CONTROL_POINT_COLOR, mControlPointColor);
	swSetProfileUnsignedInteger(SW_PROFILE_CONTROL_POINT_FOCUSED_COLOR, mControlPointFocusedColor);
}

struct swRenderTargetProfileKeys
{
	SW_PROFILE_KEY SyncRendering;
	SW_PROFILE_KEY ShadingMode;
	SW_PROFILE_KEY DrawEdgeLines;
	SW_PROFILE_KEY DrawConditionalLines;
	SW_PROFILE_KEY LineWidth;
};

static swRenderTargetProfileKeys swGetRenderTargetProfileKeys(swRenderTarget Target)
{
	if (Target == swRenderTarget::MinifigureBuilder)
		return { SW_PROFILE_MINIFIGURE_BUILDER_SYNCHRONIZED_RENDERING, SW_PROFILE_MINIFIGURE_BUILDER_SHADING_MODE, SW_PROFILE_MINIFIGURE_BUILDER_DRAW_EDGE_LINES, SW_PROFILE_MINIFIGURE_BUILDER_DRAW_CONDITIONAL_LINES, SW_PROFILE_MINIFIGURE_BUILDER_LINE_WIDTH };

	if (Target == swRenderTarget::Parts)
		return { SW_PROFILE_PARTS_SYNCHRONIZED_RENDERING, SW_PROFILE_PARTS_SHADING_MODE, SW_PROFILE_PARTS_DRAW_EDGE_LINES, SW_PROFILE_PARTS_DRAW_CONDITIONAL_LINES, SW_PROFILE_PARTS_LINE_WIDTH };

	return { SW_PROFILE_PREVIEW_SYNCHRONIZED_RENDERING, SW_PROFILE_PREVIEW_SHADING_MODE, SW_PROFILE_PREVIEW_DRAW_EDGE_LINES, SW_PROFILE_PREVIEW_DRAW_CONDITIONAL_LINES, SW_PROFILE_PREVIEW_LINE_WIDTH };
}

void swPreferences::LoadRenderTarget(swRenderTarget Target)
{
	const swRenderTargetProfileKeys Keys = swGetRenderTargetProfileKeys(Target);
	swTargetRenderSettings& TargetSettings = GetTargetRenderSettings(Target);

	TargetSettings.SyncRendering = swGetProfileInteger(Keys.SyncRendering);
	TargetSettings.Settings.ShadingMode = swGetProfileShadingMode(Keys.ShadingMode);
	TargetSettings.Settings.DrawEdgeLines = swGetProfileInteger(Keys.DrawEdgeLines);
	TargetSettings.Settings.DrawConditionalLines = swGetProfileInteger(Keys.DrawConditionalLines);
	TargetSettings.Settings.LineWidth = swGetProfileFloat(Keys.LineWidth);
}

void swPreferences::SaveRenderTarget(swRenderTarget Target) const
{
	const swRenderTargetProfileKeys Keys = swGetRenderTargetProfileKeys(Target);
	const swTargetRenderSettings& TargetSettings = GetTargetRenderSettings(Target);

	swSetProfileInteger(Keys.SyncRendering, TargetSettings.SyncRendering);
	swSetProfileShadingMode(TargetSettings.Settings.ShadingMode, Keys.ShadingMode);
	swSetProfileInteger(Keys.DrawEdgeLines, TargetSettings.Settings.DrawEdgeLines);
	swSetProfileInteger(Keys.DrawConditionalLines, TargetSettings.Settings.DrawConditionalLines);
	swSetProfileFloat(Keys.LineWidth, TargetSettings.Settings.LineWidth);
}

void swPreferences::SetInterfaceColors(swColorTheme ColorTheme)
{
	const bool SystemDark = ColorTheme == swColorTheme::System && gApplication && gApplication->IsSystemPaletteDark();

	if (ColorTheme == swColorTheme::Dark || SystemDark)
	{
		mTextColor = SW_RGBA(214, 220, 227, 255);
		mTextHaloColor = SW_RGBA(17, 19, 24, 160);
		mBackgroundSolidColor = SW_RGB(49, 52, 55);
		mOverlayColor = swGetProfileUnsignedInteger(SW_PROFILE_OVERLAY_COLOR);
		mActiveViewColor = SW_RGBA(41, 128, 185, 255);
		mGridStudColor = SW_RGBA(24, 24, 24, 192);
		mGridLineColor = SW_RGBA(24, 24, 24, 255);
		mViewSphereColor = SW_RGBA(27, 31, 36, 255);
		mViewSphereTextColor = SW_RGBA(232, 237, 242, 255);
		mViewSphereHighlightColor = SW_RGBA(41, 128, 185, 255);
	}
	else
	{
		mTextColor = SW_RGBA(24, 33, 43, 255);
		mTextHaloColor = SW_RGBA(248, 250, 252, 204);
		mBackgroundSolidColor = SW_RGB(255, 255, 255);
		mOverlayColor = SW_RGBA(0, 0, 0, 255);
		mActiveViewColor = SW_RGBA(255, 0, 0, 255);
		mGridStudColor = SW_RGBA(64, 64, 64, 192);
		mGridLineColor = SW_RGBA(0, 0, 0, 255);
		mViewSphereColor = SW_RGBA(248, 250, 252, 255);
		mViewSphereTextColor = SW_RGBA(24, 33, 43, 255);
		mViewSphereHighlightColor = SW_RGBA(41, 128, 185, 255);
	}
}

swApplication::swApplication(int& Argc, char** Argv)
	: QApplication(Argc, Argv)
{
	setApplicationDisplayName(QLatin1String("StudWorks"));

	gApplication = this;
	mDefaultStyle = style()->objectName();
	mDefaultPalette = QApplication::palette();

	swProfileInitialize();

	mPreferences.LoadDefaults();

	installEventFilter(new swWindowGeometryFilter(this));
}

swApplication::~swApplication()
{
	gApplication = nullptr;
}

void swApplication::UpdateStyle()
{
	if (mPreferences.mColorTheme == swColorTheme::Dark)
	{
		if (!QApplication::setStyle("fusion"))
			return;

		QPalette Palette = QApplication::palette();

		Palette.setColor(QPalette::Window, QColor(49, 52, 55));
		Palette.setColor(QPalette::WindowText, QColor(240, 240, 240));
		Palette.setColor(QPalette::Base, QColor(35, 38, 41));
		Palette.setColor(QPalette::AlternateBase, QColor(44, 47, 50));
		Palette.setColor(QPalette::ToolTipBase, QColor(224, 224, 244));
		Palette.setColor(QPalette::ToolTipText, QColor(58, 58, 58));
#if (QT_VERSION >= QT_VERSION_CHECK(5, 12, 0))
		Palette.setColor(QPalette::PlaceholderText, QColor(100, 100, 100));
#endif
		Palette.setColor(QPalette::Text, QColor(224, 224, 224));
		Palette.setColor(QPalette::Button, QColor(45, 48, 51));
		Palette.setColor(QPalette::ButtonText, QColor(224, 224, 244));
		Palette.setColor(QPalette::Light, QColor(65, 65, 65));
		Palette.setColor(QPalette::Midlight, QColor(62, 62, 62));
		Palette.setColor(QPalette::Dark, QColor(35, 35, 35));
		Palette.setColor(QPalette::Mid, QColor(50, 50, 50));
		Palette.setColor(QPalette::Shadow, QColor(20, 20, 20));
//		Palette.setColor(QPalette::Highlight, QColor(46, 108, 219));
		Palette.setColor(QPalette::Highlight, QColor(41, 128, 185));
		Palette.setColor(QPalette::HighlightedText, QColor(232, 232, 232));
		Palette.setColor(QPalette::Link, QColor(41, 128, 185));

		Palette.setColor(QPalette::Disabled, QPalette::Text, QColor(128, 128, 128));
		Palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(128, 128, 128));
		Palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(128, 128, 128));

		QApplication::setPalette(Palette);
		mCustomStyleApplied = true;

		QFile StylesheetFile(QLatin1String(":/stylesheet/stylesheet.qss"));

		if (StylesheetFile.open(QIODevice::ReadOnly))
		{
			QString Stylesheet = QString::fromLatin1(StylesheetFile.readAll());
			qApp->setStyleSheet(Stylesheet);
		}
	}
	else if (mCustomStyleApplied)
	{
		// Restore the desktop style and colors exactly as the system provided them.
		QApplication::setStyle(mDefaultStyle);
		QApplication::setPalette(mDefaultPalette);
		qApp->setStyleSheet(QString());

		mCustomStyleApplied = false;
	}
}

bool swApplication::IsDarkTheme() const
{
	if (mPreferences.mColorTheme == swColorTheme::Dark)
		return true;

	return IsSystemPaletteDark();
}

void swApplication::SaveTabLayout() const
{
	if (!gMainWindow || !mProject || mProject->GetFileName().isEmpty())
		return;

	QSettings Settings;
	QByteArray TabLayout = gMainWindow->GetTabLayout();

	Settings.setValue(GetTabLayoutKey(), TabLayout);
}

QString swApplication::GetTabLayoutKey() const
{
	if (mProject)
	{
		QString FileName = mProject->GetFileName();
		if (!FileName.isEmpty())
		{
			FileName.replace('\\', '?');
			FileName.replace('/', '?');
			return QString("TabLayouts/%1").arg(FileName);
		}
	}

	return QString();
}

void swApplication::SetProject(Project* Project)
{
	SaveTabLayout();

	if (gMainWindow)
	{
		gMainWindow->RemoveAllModelTabs();

		// The instructions belong to the project being replaced.
		gMainWindow->CloseInstructionsDialog();

		if (gMainWindow->GetPreviewWidget())
			gMainWindow->GetPreviewWidget()->ClearPreview();
	}

	delete mProject;
	mProject = Project;
	mProject->UpdateFileWatcher();

	if (gMainWindow)
		gMainWindow->ScheduleAssetUpdate();

	Project->SetActiveModel(0, true);
	swGetPartsLibrary()->RemoveTemporaryPartData();

	if (mProject && !mProject->GetFileName().isEmpty() && mPreferences.mRestoreTabLayout)
	{
		QSettings Settings;
		QByteArray TabLayout = Settings.value(GetTabLayoutKey()).toByteArray();

		if (gMainWindow)
			gMainWindow->RestoreTabLayout(TabLayout);
	}
}

void swApplication::SetClipboard(const QByteArray& Clipboard)
{
	mClipboard = Clipboard;
	gMainWindow->UpdatePaste(!mClipboard.isEmpty());
}

void swApplication::ExportClipboard(const QByteArray& Clipboard)
{
	QMimeData* MimeData = new QMimeData();

	MimeData->setData("application/vnd.studworks-clipboard", Clipboard);
	QApplication::clipboard()->setMimeData(MimeData);

	SetClipboard(Clipboard);
}

bool swApplication::LoadPartsLibrary(const QList<QPair<QString, bool>>& LibraryPaths, bool OnlyUsePaths, swStudStyle StudStyle, bool StudCylinderColorEnabled)
{
	if (mLibrary == nullptr)
		mLibrary = new swPartsLibrary(StudStyle, StudCylinderColorEnabled);

	const bool ShowProgress = gMainWindow != nullptr;

	if (!OnlyUsePaths)
	{
		char* EnvPath = getenv("STUDWORKS_LIB");

		if (EnvPath && EnvPath[0])
			return mLibrary->Load(EnvPath, ShowProgress, false);

		QString CustomPath = swGetProfileString(SW_PROFILE_PARTS_LIBRARY);

		if (!CustomPath.isEmpty())
			return mLibrary->Load(CustomPath, ShowProgress, false);
	}

	for (const QPair<QString, bool>& LibraryPathEntry : LibraryPaths)
		if (mLibrary->Load(LibraryPathEntry.first, ShowProgress, LibraryPathEntry.second))
			return true;

	return false;
}

swCommandLineOptions swApplication::ParseCommandLineOptions()
{
	swPreferences Preferences;
	Preferences.LoadDefaults();

	swCommandLineOptions Options;

	Options.FadeSteps = Preferences.mFadeSteps;
	Options.ImageHighlight = Preferences.mHighlightNewParts;
	Options.ImageWidth = swGetProfileInteger(SW_PROFILE_IMAGE_WIDTH);
	Options.ImageHeight = swGetProfileInteger(SW_PROFILE_IMAGE_HEIGHT);
	Options.ShadingMode = Preferences.mShadingMode;
	Options.LightingStyle = Preferences.mLightingStyle;
	Options.LineWidth = Preferences.mLineWidth;
	Options.AASamples = swGetProfileInteger(SW_PROFILE_ANTIALIASING_SAMPLES);
	Options.StudStyle = swGetProfileStudStyle();
	Options.ImageStart = 0;
	Options.ImageEnd = 0;
	Options.CameraPosition[0] = swVector3(0.0f, 0.0f, 0.0f);
	Options.CameraPosition[1] = swVector3(0.0f, 0.0f, 0.0f);
	Options.CameraPosition[2] = swVector3(0.0f, 0.0f, 0.0f);
	Options.CameraLatLon = swVector2(0.0f, 0.0f);
	Options.FoV = 0.0f;
	Options.ZPlanes = swVector2(0.0f, 0.0f);
	Options.Viewpoint = swViewpoint::Count;
	Options.FadeStepsColor = Preferences.mFadeStepsColor;
	Options.HighlightColor = Preferences.mHighlightNewPartsColor;
	Options.StudCylinderColorEnabled = Preferences.mStudCylinderColorEnabled;
	Options.StudCylinderColor = Preferences.mStudCylinderColor;
	Options.PartEdgeColorEnabled = Preferences.mPartEdgeColorEnabled;
	Options.PartEdgeColor = Preferences.mPartEdgeColor;
	Options.BlackEdgeColorEnabled = Preferences.mBlackEdgeColorEnabled;
	Options.BlackEdgeColor = Preferences.mBlackEdgeColor;
	Options.DarkEdgeColorEnabled = Preferences.mDarkEdgeColorEnabled;
	Options.DarkEdgeColor = Preferences.mDarkEdgeColor;
	Options.PartEdgeContrast = Preferences.mPartEdgeContrast;
	Options.PartColorValueLDIndex = Preferences.mPartColorValueLDIndex;
	Options.AutomateEdgeColor = Preferences.mAutomateEdgeColor;

	QStringList Arguments = arguments();

	if (Arguments.isEmpty())
		return Options;

	Arguments.removeFirst();

	while (!Arguments.isEmpty())
	{
		QString Option = Arguments.takeFirst();

		if (Option.isEmpty())
			continue;

		auto ParseString = [&Option, &Arguments, &Options](QString& Value, bool Required)
		{
			if (!Arguments.isEmpty() && Arguments.front()[0] != '-')
			{
				QString Parameter = Arguments.takeFirst();
				Value = Parameter;
			}
			else if (Required)
			{
				Options.StdErr += tr("Not enough parameters for the '%1' option.\n").arg(Option);
				Options.ParseOK = false;
				return false;
			}

			return true;
		};

		auto ParseInteger = [&Option, &Arguments, &Options](int& Value, int Min, int Max)
		{
			if (!Arguments.isEmpty() && Arguments.front()[0] != '-')
			{
				bool Ok = false;
				QString Parameter = Arguments.takeFirst();
				int NewValue = Parameter.toInt(&Ok);

				if (Ok && NewValue >= Min && NewValue <= Max)
				{
					Value = NewValue;
					return true;
				}
				else
					Options.StdErr += tr("Invalid parameter value specified for the '%1' option: '%2'.\n").arg(Option, Parameter);
			}
			else
				Options.StdErr += tr("Not enough parameters for the '%1' option.\n").arg(Option);

			Options.ParseOK = false;
			return false;
		};

		auto ParseUnsigned = [&Option, &Arguments, &Options](uint& Value, uint Min, uint Max)
		{
			if (!Arguments.isEmpty() && Arguments.front()[0] != '-')
			{
				bool Ok = false;
				QString Parameter = Arguments.takeFirst();
				uint NewValue = Parameter.toUInt(&Ok);

				if (Ok && NewValue >= Min && NewValue <= Max)
				{
					Value = NewValue;
					return true;
				}
				else
					Options.StdErr += tr("Invalid parameter value specified for the '%1' option: '%2'.\n").arg(Option, Parameter);
			}
			else
				Options.StdErr += tr("Not enough parameters for the '%1' option.\n").arg(Option);

			Options.ParseOK = false;
			return false;
		};

		auto ParseFloat = [&Option, &Arguments, &Options](float& Value, float Min, float Max)
		{
			if (!Arguments.isEmpty() && Arguments.front()[0] != '-')
			{
				bool Ok = false;
				QString Parameter = Arguments.takeFirst();
				float NewValue = Parameter.toFloat(&Ok);

				if (Ok && NewValue >= Min && NewValue <= Max)
				{
					Value = NewValue;
					return true;
				}
				else
					Options.StdErr += tr("Invalid parameter value specified for the '%1' option: '%2'.\n").arg(Option, Parameter);
			}
			else
				Options.StdErr += tr("Not enough parameters for the '%1' option.\n").arg(Option);

			Options.ParseOK = false;
			return false;
		};

		auto ParseFloatArray = [&Option, &Arguments, &Options](int Count, float* ValueArray, bool NegativesValid)
		{
			if (Arguments.size() < Count)
			{
				Options.StdErr += tr("Not enough parameters for the '%1' option.\n").arg(Option);
				Arguments.clear();
				Options.ParseOK = false;
				return false;
			}

			for (int ParseIndex = 0; ParseIndex < Count; ParseIndex++)
			{
				if (NegativesValid || Arguments.front()[0] != '-')
				{
					bool Ok = false;
					QString Parameter = Arguments.takeFirst();
					float NewValue = Parameter.toFloat(&Ok);

					if (Ok)
					{
						*(ValueArray++) = NewValue;
						continue;
					}

					Options.StdErr += tr("Invalid parameter value specified for the '%1' option: '%2'.\n").arg(Option, Parameter);
				}
				else
					Options.StdErr += tr("Not enough parameters for the '%1' option.\n").arg(Option);

				Options.ParseOK = false;
				return false;
			}

			return true;
		};

		auto ParseColor = [&Option, &Arguments, &Options](quint32& Color)
		{
			if (!Arguments.isEmpty() && Arguments.front()[0] != '-')
			{
				QString Parameter = Arguments.takeFirst();
				QColor ParsedColor = QColor(Parameter);

				if (ParsedColor.isValid())
				{
					Color = SW_RGBA(ParsedColor.red(), ParsedColor.green(), ParsedColor.blue(), ParsedColor.alpha());
					return true;
				}
				else
					Options.StdErr += tr("Invalid parameter value specified for the '%1' option: '%2'.\n").arg(Option, Parameter);
			}
			else
				Options.StdErr += tr("Not enough parameters for the '%1' option.\n").arg(Option);

			Options.ParseOK = false;
			return false;
		};

		if (Option[0] != '-')
		{
			if (QFileInfo(Option).isReadable())
				Options.ProjectName = Option;
			else
				Options.StdErr += tr("The file '%1' is not readable.\n").arg(Option);

			continue;
		}

		if (Option == QLatin1String("-l") || Option == QLatin1String("--libpath"))
		{
			QString LibPath;

			if (ParseString(LibPath, true))
				Options.LibraryPaths += QPair<QString, bool>(LibPath, false);
		}
		else if (Option == QLatin1String("-i") || Option == QLatin1String("--image"))
		{
			Options.SaveImage = true;
			ParseString(Options.ImageName, false);
		}
		else if (Option == QLatin1String("-w") || Option == QLatin1String("--width"))
			ParseInteger(Options.ImageWidth, 1, SW_MAXIMUM_IMAGE_DIMENSION);
		else if (Option == QLatin1String("-h") || Option == QLatin1String("--height"))
			ParseInteger(Options.ImageHeight, 1, SW_MAXIMUM_IMAGE_DIMENSION);
		else if (Option == QLatin1String("-f") || Option == QLatin1String("--from"))
			ParseUnsigned(Options.ImageStart, 1, SW_STEP_MAXIMUM);
		else if (Option == QLatin1String("-t") || Option == QLatin1String("--to"))
			ParseUnsigned(Options.ImageEnd, 1, SW_STEP_MAXIMUM);
		else if (Option == QLatin1String("-s") || Option == QLatin1String("--submodel"))
			ParseString(Options.ModelName, true);
		else if (Option == QLatin1String("-c") || Option == QLatin1String("--camera"))
			ParseString(Options.CameraName, true);
		else if (Option == QLatin1String("--viewpoint"))
		{
			QString ViewpointName;

			if (ParseString(ViewpointName, true))
			{
				Options.Viewpoint = swCamera::GetViewpoint(ViewpointName);

				if (Options.Viewpoint == swViewpoint::Count)
				{
					Options.StdErr += tr("Invalid parameter value specified for the '%1' option: '%2'.\n").arg(Option, ViewpointName);
					Options.ParseOK = false;
				}
			}
		}
		else if (Option == QLatin1String("--camera-angles"))
		{
			if ((Options.SetCameraAngles = ParseFloatArray(2, Options.CameraLatLon.GetFloats(), true)) && (fabsf(Options.CameraLatLon[0]) > 360.0f || fabsf(Options.CameraLatLon[1]) > 360.0f))
			{
				Options.StdErr += tr("Invalid parameter values specified for the '%1' option: limits are +/- 360.\n").arg(Option);
				Options.ParseOK = false;
			}
		}
		else if (Option == QLatin1String("--camera-position") || Option == QLatin1String("--camera-position-ldraw"))
		{
			if ((Options.SetCameraPosition = ParseFloatArray(9, Options.CameraPosition[0].GetFloats(), true)))
			{
				if (Option == QLatin1String("--camera-position-ldraw"))
				{
					Options.CameraPosition[0] = swVector3LDrawToStudWorks(Options.CameraPosition[0]);
					Options.CameraPosition[1] = swVector3LDrawToStudWorks(Options.CameraPosition[1]);
					Options.CameraPosition[2] = swVector3LDrawToStudWorks(Options.CameraPosition[2]);
				}

				swVector3 Front = Options.CameraPosition[1] - Options.CameraPosition[0];

				if (Front.LengthSquared() < 0.1f)
				{
					Options.StdErr += tr("The camera position is too close to the camera target of the '%1' option.\n").arg(Option);
					Options.ParseOK = false;
				}
				else if (Options.CameraPosition[2].LengthSquared() < 0.1f)
				{
					Options.StdErr += tr("Invalid up vector length specified for the '%1' option.\n").arg(Option);
					Options.ParseOK = false;
				}
				else if (fabsf(swDot(swNormalize(Front), swNormalize(Options.CameraPosition[2]))) > 0.99f)
				{
					Options.StdErr += tr("The up vector is parallel to the front vector specified for the '%1' option.\n").arg(Option);
					Options.ParseOK = false;
				}
			}
		}
		else if (Option == QLatin1String("--orthographic"))
			Options.Orthographic = true;
		else if (Option == QLatin1String("--fov"))
			Options.SetFoV = ParseFloat(Options.FoV, 1.0f, 180.0f);
		else if (Option == QLatin1String("--zplanes"))
		{
			if ((Options.SetZPlanes = ParseFloatArray(2, Options.ZPlanes.GetFloats(), false)) && (Options.ZPlanes[0] < 1.0 || Options.ZPlanes[0] >= Options.ZPlanes[1]))
			{
				Options.StdErr += tr("Invalid parameter values specified for the '%1' option: requirements are: 1 <= <near> < <far>.\n").arg(Option);
				Options.ParseOK = false;
			}
		}
		else if (Option == QLatin1String("-nscc") || Option == QLatin1String("--disable-stud-cylinder-color"))
			Options.StudCylinderColorEnabled = false;
		else if (Option == QLatin1String("-scc") || Option == QLatin1String("--stud-cylinder-color"))
			ParseColor(Options.StudCylinderColor);
		else if (Option == QLatin1String("-nec") || Option == QLatin1String("--disable-edge-color"))
			Options.PartEdgeColorEnabled = false;
		else if (Option == QLatin1String("-ec") || Option == QLatin1String("--edge-color"))
			ParseColor(Options.PartEdgeColor);
		else if (Option == QLatin1String("-nbec") || Option == QLatin1String("--disable-black-edge-color"))
			Options.BlackEdgeColorEnabled = false;
		else if (Option == QLatin1String("-bec") || Option == QLatin1String("--black-edge-color"))
			ParseColor(Options.BlackEdgeColor);
		else if (Option == QLatin1String("-ndec") || Option == QLatin1String("--disable-dark-edge-color"))
			Options.DarkEdgeColorEnabled = false;
		else if (Option == QLatin1String("-dec") || Option == QLatin1String("--dark-edge-color"))
			ParseColor(Options.DarkEdgeColor);
		else if (Option == QLatin1String("-aec") || Option == QLatin1String("--automate-edge-color"))
			Options.AutomateEdgeColor = true;
		else if (Option == QLatin1String("-cc") || Option == QLatin1String("--color-contrast"))
			ParseFloat(Options.PartEdgeContrast, 0.0f, 1.0f);
		else if (Option == QLatin1String("-ldv") || Option == QLatin1String("--light-dark-value"))
			ParseFloat(Options.PartColorValueLDIndex, 0.0f, 1.0f);
		else if (Option == QLatin1String("--fade-steps"))
			Options.FadeSteps = true;
		else if (Option == QLatin1String("--no-fade-steps"))
			Options.FadeSteps = false;
		else if (Option == QLatin1String("--fade-steps-color"))
		{
			if (ParseColor(Options.FadeStepsColor))
			{
				Options.SetFadeStepsColor = true;
				Options.FadeSteps = true;
			}
		}
		else if (Option == QLatin1String("--highlight"))
			Options.ImageHighlight = true;
		else if (Option == QLatin1String("--no-highlight"))
			Options.ImageHighlight = false;
		else if (Option == QLatin1String("--highlight-color"))
		{
			if (ParseColor(Options.HighlightColor))
			{
				Options.SetHighlightColor = true;
				Options.ImageHighlight = true;
			}
		}
		else if (Option == QLatin1String("--shading"))
		{
			QString ShadingString;

			if (ParseString(ShadingString, true))
			{
				if (ShadingString == QLatin1String("wireframe"))
					Options.ShadingMode = swShadingMode::Wireframe;
				else if (ShadingString == QLatin1String("flat"))
					Options.ShadingMode = swShadingMode::Flat;
				else if (ShadingString == QLatin1String("default"))
					Options.ShadingMode = swShadingMode::DefaultLights;
				else if (ShadingString == QLatin1String("full"))
					Options.ShadingMode = swShadingMode::Full;
				else
				{
					Options.StdErr += tr("Invalid parameter value specified for the '%1' option: '%2'.\n").arg(Option, ShadingString);
					Options.ParseOK = false;
				}
			}
		}
		else if (Option == QLatin1String("--lighting"))
		{
			QString LightingString;

			if (ParseString(LightingString, true))
			{
				if (LightingString == QLatin1String("standard"))
					Options.LightingStyle = swLightingStyle::Standard;
				else if (LightingString == QLatin1String("studio"))
					Options.LightingStyle = swLightingStyle::Studio;
				else if (LightingString == QLatin1String("matte"))
					Options.LightingStyle = swLightingStyle::Matte;
				else if (LightingString == QLatin1String("toon"))
					Options.LightingStyle = swLightingStyle::Toon;
				else
				{
					Options.StdErr += tr("Invalid parameter value specified for the '%1' option: '%2'.\n").arg(Option, LightingString);
					Options.ParseOK = false;
				}
			}
		}
		else if (Option == QLatin1String("--line-width"))
			ParseFloat(Options.LineWidth, 0.0f, 10.0f);
		else if (Option == QLatin1String("--aa-samples"))
		{
			if (ParseInteger(Options.AASamples, 1, 8) && Options.AASamples != 1 && Options.AASamples != 2 && Options.AASamples != 4 && Options.AASamples != 8)
			{
				Options.StdErr += tr("Invalid parameter value specified for the '%1' option: '%2'.\n").arg(Option, QString::number(Options.AASamples));
				Options.ParseOK = false;
			}
		}
		else if (Option == QLatin1String("-ss") || Option == QLatin1String("--stud-style"))
		{
			int StudStyle;

			if (ParseInteger(StudStyle, 0, static_cast<int>(swStudStyle::Count) - 1))
				Options.StudStyle = static_cast<swStudStyle>(StudStyle);
		}
		else if (Option == QLatin1String("-obj") || Option == QLatin1String("--export-wavefront"))
		{
			Options.SaveWavefront = true;
			ParseString(Options.SaveWavefrontName, false);
		}
		else if (Option == QLatin1String("-3ds") || Option == QLatin1String("--export-3ds"))
		{
			Options.Save3DS = true;
			ParseString(Options.Save3DSName, false);
		}
		else if (Option == QLatin1String("-dae") || Option == QLatin1String("--export-collada"))
		{
			Options.SaveCOLLADA = true;
			ParseString(Options.SaveCOLLADAName, false);
		}
		else if (Option == QLatin1String("-csv") || Option == QLatin1String("--export-csv"))
		{
			Options.SaveCSV = true;
			ParseString(Options.SaveCSVName, false);
		}
		else if (Option == QLatin1String("-html") || Option == QLatin1String("--export-html"))
		{
			Options.SaveHTML = true;
			ParseString(Options.SaveHTMLName, false);
		}
		else if (Option == QLatin1String("-v") || Option == QLatin1String("--version"))
		{
			Options.StdOut += tr("StudWorks Version %1\n").arg(SW_VERSION_TEXT);
			Options.StdOut += tr("Compiled %1\n").arg(__DATE__);
			Options.Exit = true;
		}
		else if (Option == QLatin1String("-?") || Option == QLatin1String("--help"))
		{
			Options.StdOut += tr("Usage: studworks [options] [file]\n");
			Options.StdOut += tr("  [options] can be:\n");
			Options.StdOut += tr("  -l, --libpath <path>: Set the Parts Library location to path.\n");
			Options.StdOut += tr("  -i, --image <outfile.ext>: Save a picture in the format specified by ext and exit.\n");
			Options.StdOut += tr("  -w, --width <width>: Set the picture width.\n");
			Options.StdOut += tr("  -h, --height <height>: Set the picture height.\n");
			Options.StdOut += tr("  -f, --from <step>: Set the first step to save pictures.\n");
			Options.StdOut += tr("  -t, --to <step>: Set the last step to save pictures.\n");
			Options.StdOut += tr("  -s, --submodel <submodel>: Set the active submodel.\n");
			Options.StdOut += tr("  -c, --camera <camera name>: Set the active camera.\n");
			Options.StdOut += tr("  --viewpoint <viewpoint>: Set the view to one of the preset viewpoints (front, back, left, right, top, bottom, home).\n");
			Options.StdOut += tr("  --camera-angles <latitude> <longitude>: Set the camera angles in degrees around the model.\n");
			Options.StdOut += tr("  --camera-position <x> <y> <z> <tx> <ty> <tz> <ux> <uy> <uz>: Set the camera position, target and up vector.\n");
			Options.StdOut += tr("  --camera-position-ldraw <x> <y> <z> <tx> <ty> <tz> <ux> <uy> <uz>: Set the camera position, target and up vector using LDraw coordinates.\n");
			Options.StdOut += tr("  --orthographic: Render images using an orthographic projection.\n");
			Options.StdOut += tr("  --fov <degrees>: Set the vertical field of view used to render images (< 180).\n");
			Options.StdOut += tr("  --zplanes <near> <far>: Set the near and far clipping planes used to render images (1 <= <near> < <far>).\n");
			Options.StdOut += tr("  --fade-steps: Render parts from prior steps faded.\n");
			Options.StdOut += tr("  --no-fade-steps: Do not render parts from prior steps faded.\n");
			Options.StdOut += tr("  --fade-steps-color <#AARRGGBB>: Rendering color for prior step parts.\n");
			Options.StdOut += tr("  --highlight: Highlight parts in the steps they appear.\n");
			Options.StdOut += tr("  --no-highlight: Do not highlight parts in the steps they appear.\n");
			Options.StdOut += tr("  --highlight-color <#AARRGGBB>: Rendering color for highlighted parts.\n");
			Options.StdOut += tr("  --shading <mode>: Select shading mode for rendering (wireframe, flat, default, full).\n");
			Options.StdOut += tr("  --lighting <style>: Select the lighting style of the default shading (standard, studio, matte, toon).\n");
			Options.StdOut += tr("  --line-width <width>: Set the width of the edge lines.\n");
			Options.StdOut += tr("  --aa-samples <samples>: Anti-aliasing sample size (1, 2, 4, or 8).\n");
			Options.StdOut += tr("  -ss, --stud-style <style>: Set the stud style (0=No style, 1=LDraw single wire, 2=LDraw double wire, 3=LDraw raised floating, 4=LDraw raised rounded, 5=LDraw subtle rounded, 6=LEGO® no logo, 7=LEGO® single wire).\n");
			Options.StdOut += tr("  -nscc, --disable-stud-cylinder-color: Disable high contrast stud cylinder color.\n");
			Options.StdOut += tr("  -scc, --stud-cylinder-color <#AARRGGBB>: High contrast stud cylinder color.\n");
			Options.StdOut += tr("  -nec, --disable-edge-color: Disable high contrast edge color.\n");
			Options.StdOut += tr("  -ec, --edge-color <#AARRGGBB>: High contrast edge color.\n");
			Options.StdOut += tr("  -nbec, --disable-black-edge-color: Disable high contrast edge color for black parts.\n");
			Options.StdOut += tr("  -bec, --black-edge-color <#AARRGGBB>: High contrast edge color for black parts.\n");
			Options.StdOut += tr("  -ndec, --disable-dark-edge-color: Disable high contrast edge color for dark color parts.\n");
			Options.StdOut += tr("  -dec, --dark-edge-color <#AARRGGBB>: High contrast edge color for dark color parts.\n");
			Options.StdOut += tr("  -aec, --automate-edge-color: Enable automatically adjusted edge colors.\n");
			Options.StdOut += tr("  -cc, --color-contrast <contrast>: Color contrast value between 0.0 and 1.0.\n");
			Options.StdOut += tr("  -ldv, --light-dark-value <value>: Light/Dark color value between 0.0 and 1.0.\n");
			Options.StdOut += tr("  -obj, --export-wavefront <outfile.obj>: Export the model to Wavefront OBJ format.\n");
			Options.StdOut += tr("  -3ds, --export-3ds <outfile.3ds>: Export the model to 3D Studio 3DS format.\n");
			Options.StdOut += tr("  -dae, --export-collada <outfile.dae>: Export the model to COLLADA DAE format.\n");
			Options.StdOut += tr("  -csv, --export-csv <outfile.csv>: Export the list of parts used in CSV format.\n");
			Options.StdOut += tr("  -html, --export-html <folder>: Create an HTML page for the model.\n");
			Options.StdOut += tr("  -v, --version: Output version information and exit.\n");
			Options.StdOut += tr("  -?, --help: Display this help message and exit.\n");
			Options.StdOut += QLatin1String("\n");
			Options.Exit = true;
		}
		else
		{
			Options.StdErr += tr("Unknown option: '%1'.\n").arg(Option);
			Options.ParseOK = false;
		}
	}

	if (Options.AutomateEdgeColor && swIsHighContrast(Options.StudStyle))
	{
		Options.StdErr += tr("High contrast stud and edge color settings are ignored when -aec or --automate-edge-color is set.\n");
	}

	if (!Options.AutomateEdgeColor && !Preferences.mAutomateEdgeColor)
	{
		QString const Message = tr("Automate edge color is required for the '%1' option but is not enabled.\n");

		if (Options.PartEdgeContrast != Preferences.mPartEdgeContrast)
			Options.StdErr += Message.arg(QString("--color-contrast %1").arg(Options.PartEdgeContrast));

		if (Options.PartColorValueLDIndex != Preferences.mPartColorValueLDIndex)
			Options.StdErr += Message.arg(QString("--light-dark-value %1").arg(Options.PartColorValueLDIndex));
	}

	if (!swIsHighContrast(Options.StudStyle) && !swIsHighContrast(swGetProfileStudStyle()))
	{
		QString const Message = tr("High contrast stud style is required for the '%1' option but is not enabled.\n");

		if (Options.StudCylinderColorEnabled != Preferences.mStudCylinderColorEnabled)
			Options.StdErr += Message.arg(QLatin1String("--disable-stud-cylinder-color"));

		if (Options.StudCylinderColor != Preferences.mStudCylinderColor)
			Options.StdErr += Message.arg(QString("--stud-cylinder-color %1").arg(Options.StudCylinderColor));

		if (Options.PartEdgeColorEnabled != Preferences.mPartEdgeColorEnabled)
			Options.StdErr += Message.arg(QLatin1String("--disable-edge-color"));

		if (Options.PartEdgeColor != Preferences.mPartEdgeColor)
			Options.StdErr += Message.arg(QString("--edge-color %1").arg(Options.PartEdgeColor));

		if (Options.BlackEdgeColorEnabled != Preferences.mBlackEdgeColorEnabled)
			Options.StdErr += Message.arg(QLatin1String("--disable-black-edge-color"));

		if (Options.BlackEdgeColor != Preferences.mBlackEdgeColor)
			Options.StdErr += Message.arg(QString("--black-edge-color %1").arg(Options.BlackEdgeColor));

		if (Options.DarkEdgeColorEnabled != Preferences.mDarkEdgeColorEnabled)
			Options.StdErr += Message.arg(QLatin1String("--disable-dark-edge-color"));

		if (Options.DarkEdgeColor != Preferences.mDarkEdgeColor)
			Options.StdErr += Message.arg(QString("--dark-edge-color %1").arg(Options.DarkEdgeColor));
	}

	if (!Options.CameraName.isEmpty())
	{
		if (Options.Viewpoint != swViewpoint::Count)
			Options.StdErr += tr("--viewpoint is ignored when --camera is set.\n");

		if (Options.Orthographic)
			Options.StdErr += tr("--orthographic is ignored when --camera is set.\n");

		if (Options.SetCameraAngles)
			Options.StdErr += tr("--camera-angles is ignored when --camera is set.\n");

		if (Options.SetCameraPosition)
			Options.StdErr += tr("--camera-position is ignored when --camera is set.\n");
	}
	else if (Options.Viewpoint != swViewpoint::Count)
	{
		if (Options.SetCameraAngles)
			Options.StdErr += tr("--camera-angles is ignored when --viewpoint is set.\n");

		if (Options.SetCameraPosition)
			Options.StdErr += tr("--camera-position is ignored when --viewpoint is set.\n");
	}
	else if (Options.SetCameraAngles)
	{
		if (Options.SetCameraPosition)
			Options.StdErr += tr("--camera-position is ignored when --camera-angles is set.\n");
	}

	const bool SaveAndExit = (Options.SaveImage || Options.SaveWavefront || Options.Save3DS || Options.SaveCOLLADA || Options.SaveCSV || Options.SaveHTML);

	if (SaveAndExit && Options.ProjectName.isEmpty())
	{
		Options.StdErr += tr("No file name specified.\n");
		Options.ParseOK = false;
	}

	return Options;
}

swStartupMode swApplication::Initialize(const QList<QPair<QString, bool>>& LibraryPaths)
{
	swCommandLineOptions Options = ParseCommandLineOptions();
	QTextStream StdErr(stderr, QIODevice::WriteOnly);
	QTextStream StdOut(stdout, QIODevice::WriteOnly);

	if (!Options.StdErr.isEmpty())
	{
		StdErr << Options.StdErr;
		StdErr.flush();
	}

	if (!Options.StdOut.isEmpty())
	{
		StdOut << Options.StdOut;
		StdOut.flush();
	}

	if (!Options.ParseOK)
		return swStartupMode::Error;

	if (Options.Exit)
		return swStartupMode::Success;

	const bool SaveAndExit = (Options.SaveImage || Options.SaveWavefront || Options.Save3DS || Options.SaveCOLLADA || Options.SaveCSV || Options.SaveHTML);

#ifdef Q_OS_IOS
	if (Options.SaveImage)
	{
		StdErr << tr("Command line image export is not supported on iOS.\n");
		return swStartupMode::Error;
	}
#endif

#ifndef Q_OS_IOS
	const bool CreateRenderResources = !SaveAndExit || Options.SaveImage || Options.SaveHTML;
	const swResult<void> RendererResult = swContext::InitializeRenderer(CreateRenderResources);

	if (!RendererResult)
	{
		StdErr << RendererResult.error() << '\n';

		if (!SaveAndExit)
			QMessageBox::critical(nullptr, tr("Initialize Renderer"), RendererResult.error());

		return swStartupMode::Error;
	}

	emit RendererInitialized();
#endif

	bool OutputFailed = false;

	if (!SaveAndExit)
	{
		UpdateStyle();

		gMainWindow = new swMainWindow();

		swLoadDefaultKeyboardShortcuts();
		swLoadDefaultMouseShortcuts();
	}

	mPreferences.mShadingMode = Options.ShadingMode;
	mPreferences.mLightingStyle = Options.LightingStyle;
	mPreferences.mLineWidth = Options.LineWidth;
	mPreferences.mStudCylinderColorEnabled = Options.StudCylinderColorEnabled;
	mPreferences.mStudCylinderColor = Options.StudCylinderColor;
	mPreferences.mPartEdgeColorEnabled = Options.PartEdgeColorEnabled;
	mPreferences.mPartEdgeColor = Options.PartEdgeColor;
	mPreferences.mBlackEdgeColorEnabled = Options.BlackEdgeColorEnabled;
	mPreferences.mBlackEdgeColor = Options.BlackEdgeColor;
	mPreferences.mDarkEdgeColorEnabled = Options.DarkEdgeColorEnabled;
	mPreferences.mDarkEdgeColor = Options.DarkEdgeColor;
	mPreferences.mPartEdgeContrast = Options.PartEdgeContrast;
	mPreferences.mPartColorValueLDIndex = Options.PartColorValueLDIndex;
	mPreferences.mAutomateEdgeColor = Options.AutomateEdgeColor;

	if (!LoadPartsLibrary(Options.LibraryPaths.isEmpty() ? LibraryPaths : Options.LibraryPaths, !Options.LibraryPaths.isEmpty(), Options.StudStyle, Options.StudCylinderColorEnabled))
	{
		QString Message;

		if (mLibrary->LoadBuiltinPartLibrary())
			Message = tr("StudWorks could not find a compatible Parts Library so only a small number of parts will be available.\n\nPlease visit https://library.ldraw.org for information on how to download and install a library.");
		else
			Message = tr("StudWorks could not load Parts Library.\n\nPlease visit https://library.ldraw.org for information on how to download and install a library.");

		if (gMainWindow)
			QMessageBox::warning(gMainWindow, tr("Load Parts Library"), Message);
		else
		{
			StdErr << Message << "\n";
			StdErr.flush();
		}
	}

	if (!SaveAndExit)
		gMainWindow->CreateWidgets();

	Project* NewProject = new Project();
	SetProject(NewProject);

	if (!SaveAndExit && Options.ProjectName.isEmpty() && swGetProfileInteger(SW_PROFILE_AUTO_LOAD_MOST_RECENT))
		Options.ProjectName = swGetProfileString(SW_PROFILE_RECENT_FILE1);

	bool ProjectLoaded = false;

	if (!Options.ProjectName.isEmpty())
	{
		if (gMainWindow)
			gMainWindow->OpenProject(Options.ProjectName);
		else
		{
			Project* LoadedProject = new Project();

			if (LoadedProject->Load(Options.ProjectName, false))
			{
				SetProject(LoadedProject);
				ProjectLoaded = true;
			}
			else
			{
				delete LoadedProject;
			}
		}
	}

	if (ProjectLoaded)
	{
		if (!Options.ModelName.isEmpty())
			mProject->SetActiveModel(Options.ModelName, true);

		std::unique_ptr<swView> ActiveView;

		if (Options.SaveImage)
		{
			swModel* Model;

			if (!Options.ModelName.isEmpty())
			{
				Model = mProject->GetModel(Options.ModelName);

				if (!Model)
				{
					StdErr << tr("Error: model '%1' does not exist.\n").arg(Options.ModelName);
					return swStartupMode::Error;
				}
			}
			else
				Model = mProject->GetMainModel();

			ActiveView = std::unique_ptr<swView>(new swView(swViewType::View, Model));

			ActiveView->SetOffscreenContext();
			ActiveView->MakeCurrent();
		}

		if (Options.SaveImage)
			ActiveView->SetSize(Options.ImageWidth, Options.ImageHeight);

		if (ActiveView)
		{
			if (!Options.CameraName.isEmpty())
				ActiveView->SetCamera(Options.CameraName);
			else
			{
				ActiveView->SetCameraProjection(Options.Orthographic ? swCameraProjection::Orthographic : swCameraProjection::Perspective);

				if (Options.SetFoV)
					ActiveView->GetCamera()->m_fovy = Options.FoV;

				if (Options.SetZPlanes)
				{
					swCamera* Camera = ActiveView->GetCamera();

					Camera->m_zNear = Options.ZPlanes[0];
					Camera->m_zFar = Options.ZPlanes[1];
				}

				if (Options.Viewpoint != swViewpoint::Count)
					ActiveView->SetViewpoint(Options.Viewpoint);
				else if (Options.SetCameraAngles)
					ActiveView->SetCameraAngles(Options.CameraLatLon[0], Options.CameraLatLon[1]);
				else if (Options.SetCameraPosition)
					ActiveView->SetViewpoint(Options.CameraPosition[0], Options.CameraPosition[1], Options.CameraPosition[2]);
			}
		}

		if (Options.SaveImage)
		{
			swModel* ActiveModel = ActiveView->GetModel();

			if (Options.ImageName.isEmpty())
				Options.ImageName = mProject->GetImageFileName(true);

			if (Options.ImageEnd < Options.ImageStart)
				Options.ImageEnd = Options.ImageStart;
			else if (Options.ImageStart > Options.ImageEnd)
				Options.ImageStart = Options.ImageEnd;

			if ((Options.ImageStart == 0) && (Options.ImageEnd == 0))
				Options.ImageStart = Options.ImageEnd = ActiveModel->GetCurrentStep();
			else if ((Options.ImageStart == 0) && (Options.ImageEnd != 0))
				Options.ImageStart = Options.ImageEnd;
			else if ((Options.ImageStart != 0) && (Options.ImageEnd == 0))
				Options.ImageEnd = Options.ImageStart;

			if (Options.ImageStart > 255)
				Options.ImageStart = 255;

			if (Options.ImageEnd > 255)
				Options.ImageEnd = 255;

			QString Frame;

			if (Options.ImageStart != Options.ImageEnd)
			{
				QString Extension = QFileInfo(Options.ImageName).suffix();
				Frame = Options.ImageName.left(Options.ImageName.length() - Extension.length() - 1) + QLatin1String("%1.") + Extension;
			}
			else
				Frame = Options.ImageName;

			mPreferences.mFadeSteps = Options.FadeSteps;
			if (Options.SetFadeStepsColor)
				mPreferences.mFadeStepsColor = Options.FadeStepsColor;
			mPreferences.mHighlightNewParts = Options.ImageHighlight;
			if (Options.SetHighlightColor)
				mPreferences.mHighlightNewPartsColor = Options.HighlightColor;

			ActiveModel->WaitForAssets();

			const bool UsesCameraPosition = Options.SetCameraPosition && Options.Viewpoint == swViewpoint::Count && !Options.SetCameraAngles;

			if (Options.CameraName.isEmpty() && !UsesCameraPosition)
			{
				const swStep FitStep = Options.ImageStart == Options.ImageEnd ? Options.ImageStart : ActiveModel->GetLastStep();

				ActiveView->ZoomExtentsAtStep(FitStep, swGeometryBoundsMode::AvailableOnly, !Options.SetZPlanes);
			}

			auto ProgressCallback = [&StdOut](const QString& FileName)
			{
				StdOut << tr("Saved '%1'.\n").arg(FileName);
			};

			const swResult<void> ImagesSaved = ActiveView->SaveStepImages(Frame, Options.ImageStart != Options.ImageEnd, Options.ImageStart, Options.ImageEnd, ProgressCallback);

			if (!ImagesSaved)
			{
				StdErr << ImagesSaved.error() << '\n';

				return swStartupMode::Error;
			}
		}

		if (Options.SaveWavefront)
		{
			QString FileName;

			if (!Options.SaveWavefrontName.isEmpty())
				FileName = Options.SaveWavefrontName;
			else
				FileName = Options.ProjectName;

			QString Extension = QFileInfo(FileName).suffix().toLower();

			if (Extension.isEmpty())
			{
				FileName += ".obj";
			}
			else if (Extension != "obj")
			{
				FileName = FileName.left(FileName.length() - Extension.length() - 1);
				FileName += ".obj";
			}

			swResult<void> ExportResult = mProject->ExportWavefront(FileName);

			if (ExportResult)
				StdOut << tr("Saved '%1'.\n").arg(FileName);
			else
			{
				OutputFailed = true;

				if (!ExportResult.error().isEmpty())
					StdErr << ExportResult.error() << '\n';
			}
		}

		if (Options.Save3DS)
		{
			QString FileName;

			if (!Options.Save3DSName.isEmpty())
				FileName = Options.Save3DSName;
			else
				FileName = Options.ProjectName;

			QString Extension = QFileInfo(FileName).suffix().toLower();

			if (Extension.isEmpty())
			{
				FileName += ".3ds";
			}
			else if (Extension != "3ds")
			{
				FileName = FileName.left(FileName.length() - Extension.length() - 1);
				FileName += ".3ds";
			}

			swResult<void> ExportResult = mProject->Export3DStudio(FileName);

			if (ExportResult)
				StdOut << tr("Saved '%1'.\n").arg(FileName);
			else
			{
				OutputFailed = true;

				if (!ExportResult.error().isEmpty())
					StdErr << ExportResult.error() << '\n';
			}
		}

		if (Options.SaveCOLLADA)
		{
			QString FileName;

			if (!Options.SaveCOLLADAName.isEmpty())
				FileName = Options.SaveCOLLADAName;
			else
				FileName = Options.ProjectName;

			QString Extension = QFileInfo(FileName).suffix().toLower();

			if (Extension.isEmpty())
			{
				FileName += ".dae";
			}
			else if (Extension != "dae")
			{
				FileName = FileName.left(FileName.length() - Extension.length() - 1);
				FileName += ".dae";
			}

			swResult<void> ExportResult = mProject->ExportCOLLADA(FileName);

			if (ExportResult)
				StdOut << tr("Saved '%1'.\n").arg(FileName);
			else
			{
				OutputFailed = true;

				if (!ExportResult.error().isEmpty())
					StdErr << ExportResult.error() << '\n';
			}
		}

		if (Options.SaveCSV)
		{
			QString FileName;

			if (!Options.SaveCSVName.isEmpty())
				FileName = Options.SaveCSVName;
			else
				FileName = Options.ProjectName;

			QString Extension = QFileInfo(FileName).suffix().toLower();

			if (Extension.isEmpty())
			{
				FileName += ".csv";
			}
			else if (Extension != "csv")
			{
				FileName = FileName.left(FileName.length() - Extension.length() - 1);
				FileName += ".csv";
			}

			swResult<void> ExportResult = mProject->ExportCSV(FileName);

			if (ExportResult)
				StdOut << tr("Saved '%1'.\n").arg(FileName);
			else
			{
				OutputFailed = true;

				if (!ExportResult.error().isEmpty())
					StdErr << ExportResult.error() << '\n';
			}
		}

		if (Options.SaveHTML)
		{
			swHTMLExportOptions HTMLOptions(mProject);

			if (!Options.SaveHTMLName.isEmpty())
				HTMLOptions.PathName = Options.SaveHTMLName;

			swResult<void> ExportResult = mProject->ExportHTML(HTMLOptions);

			if (ExportResult)
				StdOut << tr("Saved '%1'.\n").arg(HTMLOptions.PathName);
			else
			{
				OutputFailed = true;

				if (!ExportResult.error().isEmpty())
					StdErr << ExportResult.error() << '\n';
			}
		}
	}

	if (!SaveAndExit)
	{
		gMainWindow->SetColorIndex(swGetColorIndex(7));
		gMainWindow->GetPartSelectionWidget()->SetDefaultPart();

#ifdef Q_OS_IOS
		const swResult<void> RendererResult = swContext::InitializeRenderer(true);

		if (!RendererResult)
		{
			StdErr << RendererResult.error() << '\n';
			QMessageBox::critical(gMainWindow, tr("Initialize Renderer"), RendererResult.error());
			return swStartupMode::Error;
		}

		emit RendererInitialized();
#endif

		gMainWindow->show();

#ifdef Q_OS_WIN
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
		QWindowsWindowFunctions::setHasBorderInFullScreen(gMainWindow->windowHandle(), true);
#endif
#endif
	}

	if (OutputFailed)
		return swStartupMode::Error;

	return SaveAndExit ? swStartupMode::Success : swStartupMode::ShowWindow;
}

void swApplication::Shutdown()
{
	delete gMainWindow;
	gMainWindow = nullptr;

	delete mProject;
	mProject = nullptr;

	delete mLibrary;
	mLibrary = nullptr;

	swContext::ShutdownRenderer();
}

void swApplication::ShowPreferencesDialog()
{
	swPreferencesDialogOptions Options;
	int CurrentAASamples = swGetProfileInteger(SW_PROFILE_ANTIALIASING_SAMPLES);
	swStudStyle CurrentStudStyle = swGetPartsLibrary()->GetStudStyle();

	Options.Preferences = mPreferences;

	Options.LibraryPath = swGetProfileString(SW_PROFILE_PARTS_LIBRARY);
	Options.MinifigureBuilderSettingsPath = swGetProfileString(SW_PROFILE_MINIFIGURE_BUILDER_SETTINGS);
	Options.ColorConfigPath = swGetProfileString(SW_PROFILE_COLOR_CONFIGURATION);
	Options.POVRayPath = swGetProfileString(SW_PROFILE_POVRAY_PATH);
	Options.LGEOPath = swGetProfileString(SW_PROFILE_POVRAY_LGEO_PATH);
	Options.DefaultAuthor = swGetProfileString(SW_PROFILE_DEFAULT_AUTHOR_NAME);
	Options.Language = swGetProfileString(SW_PROFILE_LANGUAGE);

	Options.AASamples = CurrentAASamples;
	Options.StudStyle = CurrentStudStyle;

	Options.Categories = gCategories;
	Options.CategoriesModified = false;
	Options.CategoriesDefault = false;

	Options.KeyboardShortcuts = gKeyboardShortcuts;
	Options.KeyboardShortcutsModified = false;
	Options.KeyboardShortcutsDefault = false;
	Options.MouseShortcuts = gMouseShortcuts;
	Options.MouseShortcutsModified = false;
	Options.MouseShortcutsDefault = false;

	swPreferencesDialog Dialog(gMainWindow, &Options);
	if (Dialog.exec() != QDialog::Accepted)
		return;

	bool LanguageChanged = Options.Language != swGetProfileString(SW_PROFILE_LANGUAGE);
	bool LibraryChanged = Options.LibraryPath != swGetProfileString(SW_PROFILE_PARTS_LIBRARY);
	bool ColorConfigChanged = Options.ColorConfigPath != swGetProfileString(SW_PROFILE_COLOR_CONFIGURATION);
	bool AAChanged = CurrentAASamples != Options.AASamples;
	bool StudStyleChanged = CurrentStudStyle != Options.StudStyle;
	bool StudCylinderColorEnabledChanged = Options.Preferences.mStudCylinderColorEnabled != mPreferences.mStudCylinderColorEnabled;
	bool EdgeColorSettingsChanged = Options.Preferences.mAutomateEdgeColor != mPreferences.mAutomateEdgeColor;
	EdgeColorSettingsChanged |= Options.Preferences.mPartEdgeContrast != mPreferences.mPartEdgeContrast;
	EdgeColorSettingsChanged |= Options.Preferences.mPartColorValueLDIndex != mPreferences.mPartColorValueLDIndex;
	EdgeColorSettingsChanged |= Options.Preferences.mStudCylinderColorEnabled != mPreferences.mStudCylinderColorEnabled;
	EdgeColorSettingsChanged |= Options.Preferences.mStudCylinderColor != mPreferences.mStudCylinderColor;
	EdgeColorSettingsChanged |= Options.Preferences.mPartEdgeColorEnabled != mPreferences.mPartEdgeColorEnabled;
	EdgeColorSettingsChanged |= Options.Preferences.mPartEdgeColor != mPreferences.mPartEdgeColor;
	EdgeColorSettingsChanged |= Options.Preferences.mBlackEdgeColorEnabled != mPreferences.mBlackEdgeColorEnabled;
	EdgeColorSettingsChanged |= Options.Preferences.mBlackEdgeColor != mPreferences.mBlackEdgeColor;
	EdgeColorSettingsChanged |= Options.Preferences.mDarkEdgeColorEnabled != mPreferences.mDarkEdgeColorEnabled;
	EdgeColorSettingsChanged |= Options.Preferences.mDarkEdgeColor != mPreferences.mDarkEdgeColor;
	bool WatchFileChangesChanged = Options.Preferences.mWatchFileChanges != mPreferences.mWatchFileChanges;
	const swRenderSettings OldPartsRendering = mPreferences.GetRenderSettings(swRenderTarget::Parts);
	const swRenderSettings NewPartsRendering = Options.Preferences.GetRenderSettings(swRenderTarget::Parts);
	const bool PartsRenderingChanged = OldPartsRendering.ShadingMode != NewPartsRendering.ShadingMode || OldPartsRendering.DrawEdgeLines != NewPartsRendering.DrawEdgeLines ||
		OldPartsRendering.DrawConditionalLines != NewPartsRendering.DrawConditionalLines || OldPartsRendering.LineWidth != NewPartsRendering.LineWidth ||
		Options.Preferences.mLightingStyle != mPreferences.mLightingStyle;


	mPreferences = Options.Preferences;

	mPreferences.SaveDefaults();
	UpdateStyle();

	if (WatchFileChangesChanged && mProject)
		mProject->UpdateFileWatcher();
	if (PartsRenderingChanged && gMainWindow && gMainWindow->GetPartSelectionWidget())
		gMainWindow->GetPartSelectionWidget()->UpdateThumbnails();
	if (gMainWindow && gMainWindow->GetPreviewWidget())
		gMainWindow->GetPreviewWidget()->UpdateSquareViewport();

	swSetProfileString(SW_PROFILE_DEFAULT_AUTHOR_NAME, Options.DefaultAuthor);
	swSetProfileString(SW_PROFILE_PARTS_LIBRARY, Options.LibraryPath);
	swSetProfileString(SW_PROFILE_MINIFIGURE_BUILDER_SETTINGS, Options.MinifigureBuilderSettingsPath);
	swSetProfileString(SW_PROFILE_POVRAY_PATH, Options.POVRayPath);
	swSetProfileString(SW_PROFILE_POVRAY_LGEO_PATH, Options.LGEOPath);
	swSetProfileString(SW_PROFILE_LANGUAGE, Options.Language);
	swSetProfileInteger(SW_PROFILE_ANTIALIASING_SAMPLES, Options.AASamples);
	swSetProfileStudStyle(Options.StudStyle);

	if (LanguageChanged || LibraryChanged || AAChanged)
		QMessageBox::information(gMainWindow, tr("Preferences"), tr("Some changes will only take effect the next time you start StudWorks."));

	if (Options.CategoriesModified)
	{
		if (Options.CategoriesDefault)
			swResetDefaultCategories();
		else
		{
			gCategories = Options.Categories;
			swSaveDefaultCategories();
		}

		gMainWindow->UpdateCategories();
	}

	if (Options.KeyboardShortcutsModified)
	{
		if (Options.KeyboardShortcutsDefault)
			swResetDefaultKeyboardShortcuts();
		else
		{
			gKeyboardShortcuts = Options.KeyboardShortcuts;
			swSaveDefaultKeyboardShortcuts();
		}

		gMainWindow->UpdateShortcuts();
	}

	if (Options.MouseShortcutsModified)
	{
		if (Options.MouseShortcutsDefault)
			swResetDefaultMouseShortcuts();
		else
		{
			gMouseShortcuts = Options.MouseShortcuts;
			swSaveDefaultMouseShortcuts();
		}
	}

	// UpdateStudStyleAndColors() reloads colors too; both paths must see the newly selected config.
	if (ColorConfigChanged)
		swSetProfileString(SW_PROFILE_COLOR_CONFIGURATION, Options.ColorConfigPath);

	if (StudStyleChanged || StudCylinderColorEnabledChanged)
		swGetPartsLibrary()->UpdateStudStyleAndColors(Options.StudStyle, Options.Preferences.mStudCylinderColorEnabled);
	else if (ColorConfigChanged || EdgeColorSettingsChanged)
		swGetPartsLibrary()->LoadColors();

	gMainWindow->SetShadingMode(Options.Preferences.mShadingMode);
	swView::UpdateAllViews();
}
