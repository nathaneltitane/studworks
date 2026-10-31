#include "sw_global.h"
#include <stdio.h>
#include "sw_application.h"
#include "sw_library.h"
#include "sw_library_shadow.h"
#include "sw_profile.h"
#include "sw_project.h"
#include "sw_model.h"
#include "sw_object_part.h"
#include "sw_window_main.h"
#include "sw_dialog_preferences.h"
#include "sw_widget_selection_part.h"
#include "sw_shortcuts.h"
#include "sw_category.h"
#include "sw_view.h"
#include "sw_object_camera.h"
#include "sw_widget_preview.h"
#include "sw_colors.h"
#include "sw_window_utilities.h"
#include "sw_http.h"
#include "sw_renderer_povray.h"
#include "sw_dialog_render.h"

#ifdef Q_OS_WIN
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
#include <QtPlatformHeaders\QWindowsWindowFunctions>
#endif
#endif

#define SW_LIBRARY_DOWNLOAD_URL "https://library.ldraw.org/library/updates/complete.zip"

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
	mProjectionScope = static_cast<swProjectionScope>(qBound(0, swGetProfileInteger(SW_PROFILE_PROJECTION_SCOPE), static_cast<int>(swProjectionScope::AllViews)));
	mCutawaySize = swGetProfileInteger(SW_PROFILE_CUTAWAY_SIZE);
	mTurntableIdleDuration = swGetProfileInteger(SW_PROFILE_TURNTABLE_IDLE_DURATION);
	mTurntableTurnTime = swGetProfileInteger(SW_PROFILE_TURNTABLE_TURN_TIME);
	mTurntableAxis = static_cast<swTurntableAxis>(qBound(0, swGetProfileInteger(SW_PROFILE_TURNTABLE_AXIS), static_cast<int>(swTurntableAxis::Z)));
	mAxisIconLocation = swGetProfileAxisIconLocation();
	mAxisIconSize = swSnapSize(swGetProfileInteger(SW_PROFILE_AXIS_ICON_SIZE), SW_AXIS_ICON_SIZES);
	mAxisIconLabels = swGetProfileInteger(SW_PROFILE_AXIS_ICON_LABELS);
	mAxisIconNegative = swGetProfileInteger(SW_PROFILE_AXIS_ICON_NEGATIVE);
	mAxisIconCubeOutline = swGetProfileInteger(SW_PROFILE_AXIS_ICON_CUBE_OUTLINE);
	mAxisIconCubeOutlineAlpha = swGetProfileInteger(SW_PROFILE_AXIS_ICON_CUBE_OUTLINE_ALPHA);
	mTextColor = swGetProfileUnsignedInteger(SW_PROFILE_TEXT_COLOR);
	mTextHaloColor = swGetProfileUnsignedInteger(SW_PROFILE_TEXT_HALO_COLOR);
	mMarqueeBorderColor = swGetProfileUnsignedInteger(SW_PROFILE_MARQUEE_BORDER_COLOR);
	mMarqueeFillColor = swGetProfileUnsignedInteger(SW_PROFILE_MARQUEE_FILL_COLOR);
	mOverlayColor = swGetProfileUnsignedInteger(SW_PROFILE_OVERLAY_COLOR);
	mCutawayShadeColor = swGetProfileUnsignedInteger(SW_PROFILE_CUTAWAY_SHADE_COLOR);
	mCutawayShadeAlpha = swGetProfileInteger(SW_PROFILE_CUTAWAY_SHADE_ALPHA);
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
	mFadeStepsColor = swClampStepColorAlpha(swGetProfileUnsignedInteger(SW_PROFILE_FADE_STEPS_COLOR));
	mHighlightNewParts = swGetProfileInteger(SW_PROFILE_HIGHLIGHT_NEW_PARTS);
	mHighlightNewPartsColor = swClampStepColorAlpha(swGetProfileUnsignedInteger(SW_PROFILE_HIGHLIGHT_NEW_PARTS_COLOR));
	mDrawGridStuds = swGetProfileInteger(SW_PROFILE_GRID_STUDS);
	mGridStudColor = swGetProfileUnsignedInteger(SW_PROFILE_GRID_STUD_COLOR);
	mDrawGridLines = swGetProfileInteger(SW_PROFILE_GRID_LINES);
	mGridLineSpacing = swGetProfileInteger(SW_PROFILE_GRID_LINE_SPACING);
	mGridLineColor = swGetProfileUnsignedInteger(SW_PROFILE_GRID_LINE_COLOR);
	mDrawGridOrigin = swGetProfileInteger(SW_PROFILE_GRID_ORIGIN);
	mDrawGridNegativeAxes = swGetProfileInteger(SW_PROFILE_GRID_NEGATIVE_AXES);
	mDrawGridHeightXZ = swGetProfileInteger(SW_PROFILE_GRID_HEIGHT_XZ);
	mDrawGridHeightYZ = swGetProfileInteger(SW_PROFILE_GRID_HEIGHT_YZ);
	mGridHeightBehindModel = swGetProfileInteger(SW_PROFILE_GRID_HEIGHT_BEHIND_MODEL);
	mDrawGridAxisX = swGetProfileInteger(SW_PROFILE_GRID_AXIS_X);
	mDrawGridAxisY = swGetProfileInteger(SW_PROFILE_GRID_AXIS_Y);
	mDrawGridAxisZ = swGetProfileInteger(SW_PROFILE_GRID_AXIS_Z);
	mViewCubeEnabled = swGetProfileInteger(SW_PROFILE_VIEW_CUBE_ENABLED);
	mViewCubeLocation = swGetProfileViewCubeLocation();
	mViewCubeSize = swSnapSize(swGetProfileInteger(SW_PROFILE_VIEW_CUBE_SIZE), SW_VIEW_CUBE_SIZES);
	mViewCubeColor = swGetProfileUnsignedInteger(SW_PROFILE_VIEW_CUBE_COLOR);
	mViewCubeTextColor = swGetProfileUnsignedInteger(SW_PROFILE_VIEW_CUBE_TEXT_COLOR);
	mViewCubeHighlightColor = swGetProfileUnsignedInteger(SW_PROFILE_VIEW_CUBE_HIGHLIGHT_COLOR);
	mAxisXColor = swGetProfileUnsignedInteger(SW_PROFILE_AXIS_X_COLOR);
	mAxisYColor = swGetProfileUnsignedInteger(SW_PROFILE_AXIS_Y_COLOR);
	mAxisZColor = swGetProfileUnsignedInteger(SW_PROFILE_AXIS_Z_COLOR);
	mAxisXNegativeColor = swGetProfileUnsignedInteger(SW_PROFILE_AXIS_X_NEGATIVE_COLOR);
	mAxisYNegativeColor = swGetProfileUnsignedInteger(SW_PROFILE_AXIS_Y_NEGATIVE_COLOR);
	mAxisZNegativeColor = swGetProfileUnsignedInteger(SW_PROFILE_AXIS_Z_NEGATIVE_COLOR);
	mAutoLoadMostRecent = swGetProfileInteger(SW_PROFILE_AUTO_LOAD_MOST_RECENT);
	mRestoreTabLayout = swGetProfileInteger(SW_PROFILE_RESTORE_TAB_LAYOUT);
	mFirstPartAtOrigin = swGetProfileInteger(SW_PROFILE_FIRST_PART_AT_ORIGIN);
	mShadowLibraryEnabled = swGetProfileInteger(SW_PROFILE_SHADOW_LIBRARY_ENABLED);
	mPartsSnapToTiles = swGetProfileInteger(SW_PROFILE_PARTS_SNAP_TO_TILES);
	mTerminalFollowModel = swGetProfileInteger(SW_PROFILE_TERMINAL_FOLLOW_MODEL);
	mInsertColorMode = static_cast<swInsertColorMode>(qBound(0, swGetProfileInteger(SW_PROFILE_INSERT_COLOR_MODE), static_cast<int>(swInsertColorMode::LastInsertedColor)));
	mFlexSpreadMode = static_cast<swFlexSpreadMode>(qBound(0, swGetProfileInteger(SW_PROFILE_FLEX_SPREAD_MODE), static_cast<int>(swFlexSpreadMode::DistributedWeight)));
	mFlexClampNewControlPoints = swGetProfileInteger(SW_PROFILE_FLEX_CLAMP_NEW_CONTROL_POINTS);
	mFlexMiddleControlPoint = swGetProfileInteger(SW_PROFILE_FLEX_MIDDLE_CONTROL_POINT);
	mWatchFileChanges = swGetProfileInteger(SW_PROFILE_WATCH_FILE_CHANGES);
	mLoadPartsBeforeShowing = swGetProfileInteger(SW_PROFILE_LOAD_PARTS_BEFORE_SHOWING);
	mMigrateMetaCommands = swGetProfileInteger(SW_PROFILE_MIGRATE_META_COMMANDS);
	mMarkColladaTransparency = swGetProfileInteger(SW_PROFILE_MARK_COLLADA_TRANSPARENCY);
	mColorTheme = swGetProfileColorTheme();
	mPreviewViewCubeEnabled = swGetProfileInteger(SW_PROFILE_PREVIEW_VIEW_CUBE_ENABLED);
	mPreviewViewCubeSize = swSnapSize(swGetProfileInteger(SW_PROFILE_PREVIEW_VIEW_CUBE_SIZE), SW_VIEW_CUBE_SIZES);
	mPreviewViewCubeLocation = swGetProfilePreviewViewCubeLocation();
	mDrawPreviewAxis = swGetProfileInteger(SW_PROFILE_PREVIEW_DRAW_AXES);
	mPreviewAxisIconSize = swSnapSize(swGetProfileInteger(SW_PROFILE_PREVIEW_AXIS_ICON_SIZE), SW_AXIS_ICON_SIZES);
	mPreviewSquareViewport = swGetProfileInteger(SW_PROFILE_PREVIEW_SQUARE_VIEWPORT);
	mPreviewKeepSelection = swGetProfileInteger(SW_PROFILE_PREVIEW_KEEP_SELECTION);
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
	mControlPointSeatedColor = swGetProfileUnsignedInteger(SW_PROFILE_CONTROL_POINT_SEATED_COLOR);
	mPathColor = swGetProfileUnsignedInteger(SW_PROFILE_PATH_COLOR);
	mPathOutOfReachColor = swGetProfileUnsignedInteger(SW_PROFILE_PATH_OUT_OF_REACH_COLOR);
	mSpreadHandleColor = swGetProfileUnsignedInteger(SW_PROFILE_SPREAD_HANDLE_COLOR);
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
	swSetProfileInteger(SW_PROFILE_PROJECTION_SCOPE, static_cast<int>(mProjectionScope));
	swSetProfileInteger(SW_PROFILE_CUTAWAY_SIZE, mCutawaySize);
	swSetProfileInteger(SW_PROFILE_TURNTABLE_IDLE_DURATION, mTurntableIdleDuration);
	swSetProfileInteger(SW_PROFILE_TURNTABLE_TURN_TIME, mTurntableTurnTime);
	swSetProfileInteger(SW_PROFILE_TURNTABLE_AXIS, static_cast<int>(mTurntableAxis));
	swSetProfileAxisIconLocation(mAxisIconLocation);
	swSetProfileInteger(SW_PROFILE_AXIS_ICON_SIZE, mAxisIconSize);
	swSetProfileInteger(SW_PROFILE_AXIS_ICON_LABELS, mAxisIconLabels);
	swSetProfileInteger(SW_PROFILE_AXIS_ICON_NEGATIVE, mAxisIconNegative);
	swSetProfileInteger(SW_PROFILE_AXIS_ICON_CUBE_OUTLINE, mAxisIconCubeOutline);
	swSetProfileInteger(SW_PROFILE_AXIS_ICON_CUBE_OUTLINE_ALPHA, mAxisIconCubeOutlineAlpha);
	swSetProfileUnsignedInteger(SW_PROFILE_TEXT_COLOR, mTextColor);
	swSetProfileUnsignedInteger(SW_PROFILE_TEXT_HALO_COLOR, mTextHaloColor);
	swSetProfileUnsignedInteger(SW_PROFILE_BACKGROUND_COLOR, mBackgroundSolidColor);
	swSetProfileUnsignedInteger(SW_PROFILE_MARQUEE_BORDER_COLOR, mMarqueeBorderColor);
	swSetProfileUnsignedInteger(SW_PROFILE_MARQUEE_FILL_COLOR, mMarqueeFillColor);
	swSetProfileUnsignedInteger(SW_PROFILE_OVERLAY_COLOR, mOverlayColor);
	swSetProfileUnsignedInteger(SW_PROFILE_CUTAWAY_SHADE_COLOR, mCutawayShadeColor);
	swSetProfileInteger(SW_PROFILE_CUTAWAY_SHADE_ALPHA, mCutawayShadeAlpha);
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
	swSetProfileInteger(SW_PROFILE_GRID_NEGATIVE_AXES, mDrawGridNegativeAxes);
	swSetProfileInteger(SW_PROFILE_GRID_HEIGHT_XZ, mDrawGridHeightXZ);
	swSetProfileInteger(SW_PROFILE_GRID_HEIGHT_YZ, mDrawGridHeightYZ);
	swSetProfileInteger(SW_PROFILE_GRID_HEIGHT_BEHIND_MODEL, mGridHeightBehindModel);
	swSetProfileInteger(SW_PROFILE_GRID_AXIS_X, mDrawGridAxisX);
	swSetProfileInteger(SW_PROFILE_GRID_AXIS_Y, mDrawGridAxisY);
	swSetProfileInteger(SW_PROFILE_GRID_AXIS_Z, mDrawGridAxisZ);
	swSetProfileInteger(SW_PROFILE_VIEW_CUBE_ENABLED, mViewCubeEnabled);
	swSetProfileViewCubeLocation(mViewCubeLocation);
	swSetProfileInteger(SW_PROFILE_VIEW_CUBE_SIZE, mViewCubeSize);
	swSetProfileUnsignedInteger(SW_PROFILE_VIEW_CUBE_COLOR, mViewCubeColor);
	swSetProfileUnsignedInteger(SW_PROFILE_VIEW_CUBE_TEXT_COLOR, mViewCubeTextColor);
	swSetProfileUnsignedInteger(SW_PROFILE_VIEW_CUBE_HIGHLIGHT_COLOR, mViewCubeHighlightColor);
	swSetProfileUnsignedInteger(SW_PROFILE_AXIS_X_COLOR, mAxisXColor);
	swSetProfileUnsignedInteger(SW_PROFILE_AXIS_Y_COLOR, mAxisYColor);
	swSetProfileUnsignedInteger(SW_PROFILE_AXIS_Z_COLOR, mAxisZColor);
	swSetProfileUnsignedInteger(SW_PROFILE_AXIS_X_NEGATIVE_COLOR, mAxisXNegativeColor);
	swSetProfileUnsignedInteger(SW_PROFILE_AXIS_Y_NEGATIVE_COLOR, mAxisYNegativeColor);
	swSetProfileUnsignedInteger(SW_PROFILE_AXIS_Z_NEGATIVE_COLOR, mAxisZNegativeColor);
	swSetProfileInteger(SW_PROFILE_AUTO_LOAD_MOST_RECENT, mAutoLoadMostRecent);
	swSetProfileInteger(SW_PROFILE_RESTORE_TAB_LAYOUT, mRestoreTabLayout);
	swSetProfileInteger(SW_PROFILE_FIRST_PART_AT_ORIGIN, mFirstPartAtOrigin);
	swSetProfileInteger(SW_PROFILE_SHADOW_LIBRARY_ENABLED, mShadowLibraryEnabled);
	swSetProfileInteger(SW_PROFILE_PARTS_SNAP_TO_TILES, mPartsSnapToTiles);
	swSetProfileInteger(SW_PROFILE_TERMINAL_FOLLOW_MODEL, mTerminalFollowModel);
	swSetProfileInteger(SW_PROFILE_INSERT_COLOR_MODE, static_cast<int>(mInsertColorMode));
	swSetProfileInteger(SW_PROFILE_FLEX_SPREAD_MODE, static_cast<int>(mFlexSpreadMode));
	swSetProfileInteger(SW_PROFILE_FLEX_CLAMP_NEW_CONTROL_POINTS, mFlexClampNewControlPoints);
	swSetProfileInteger(SW_PROFILE_FLEX_MIDDLE_CONTROL_POINT, mFlexMiddleControlPoint);
	swSetProfileInteger(SW_PROFILE_WATCH_FILE_CHANGES, mWatchFileChanges);
	swSetProfileInteger(SW_PROFILE_LOAD_PARTS_BEFORE_SHOWING, mLoadPartsBeforeShowing);
	swSetProfileInteger(SW_PROFILE_MIGRATE_META_COMMANDS, mMigrateMetaCommands);
	swSetProfileInteger(SW_PROFILE_MARK_COLLADA_TRANSPARENCY, mMarkColladaTransparency);
	swSetProfileColorTheme(mColorTheme);
	swSetProfileInteger(SW_PROFILE_PREVIEW_VIEW_CUBE_ENABLED, mPreviewViewCubeEnabled);
	swSetProfileInteger(SW_PROFILE_PREVIEW_VIEW_CUBE_SIZE, mPreviewViewCubeSize);
	swSetProfilePreviewViewCubeLocation(mPreviewViewCubeLocation);
	swSetProfileInteger(SW_PROFILE_PREVIEW_DRAW_AXES, mDrawPreviewAxis);
	swSetProfileInteger(SW_PROFILE_PREVIEW_AXIS_ICON_SIZE, mPreviewAxisIconSize);
	swSetProfileInteger(SW_PROFILE_PREVIEW_SQUARE_VIEWPORT, mPreviewSquareViewport);
	swSetProfileInteger(SW_PROFILE_PREVIEW_KEEP_SELECTION, mPreviewKeepSelection);
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
	swSetProfileUnsignedInteger(SW_PROFILE_CONTROL_POINT_SEATED_COLOR, mControlPointSeatedColor);
	swSetProfileUnsignedInteger(SW_PROFILE_PATH_COLOR, mPathColor);
	swSetProfileUnsignedInteger(SW_PROFILE_PATH_OUT_OF_REACH_COLOR, mPathOutOfReachColor);
	swSetProfileUnsignedInteger(SW_PROFILE_SPREAD_HANDLE_COLOR, mSpreadHandleColor);
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
		mViewCubeColor = SW_RGBA(27, 31, 36, 255);
		mViewCubeTextColor = SW_RGBA(232, 237, 242, 255);
		mViewCubeHighlightColor = SW_RGBA(41, 128, 185, 255);
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
		mViewCubeColor = SW_RGBA(248, 250, 252, 255);
		mViewCubeTextColor = SW_RGBA(24, 33, 43, 255);
		mViewCubeHighlightColor = SW_RGBA(41, 128, 185, 255);
	}
}

swApplication::swApplication(int& Argc, char** Argv)
	: QApplication(Argc, Argv)
{
	setApplicationDisplayName(QLatin1String("StudWorx"));

	gApplication = this;
	mDefaultStyle = style()->objectName();
	mDefaultPalette = QApplication::palette();

	swProfileInitialize();

	mPreferences.LoadDefaults();

	installEventFilter(new swWindowGeometryFilter(this));
	installEventFilter(new swInterfaceFitFilter(this));
}

swApplication::~swApplication()
{
	gApplication = nullptr;
}

void swApplication::UpdateStyle()
{
	if (mPreferences.mColorTheme == swColorTheme::Dark)
	{
		QStyle* Style = QStyleFactory::create(QLatin1String("fusion"));

		if (!Style)
			return;

		QApplication::setStyle(new swInterfaceStyle(Style));

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
	else if (mCustomStyleApplied || !dynamic_cast<swInterfaceStyle*>(QApplication::style()))
	{
		// restore the system style and colors
		QApplication::setStyle(new swInterfaceStyle(QStyleFactory::create(mDefaultStyle)));

		if (mCustomStyleApplied)
		{
			QApplication::setPalette(mDefaultPalette);
			qApp->setStyleSheet(QString());
		}

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

		// instructions belong to the replaced project
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

	MimeData->setData("application/vnd.studworx-clipboard", Clipboard);
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
		char* PathEnvironment = getenv("STUDWORX_LIBRARY");

		if (PathEnvironment && PathEnvironment[0])
			return mLibrary->Load(PathEnvironment, ShowProgress, false);

		QString CustomPath = swGetProfileString(SW_PROFILE_PARTS_LIBRARY);

		if (!CustomPath.isEmpty())
			return mLibrary->Load(CustomPath, ShowProgress, false);
	}

	for (const QPair<QString, bool>& LibraryPathEntry : LibraryPaths)
		if (mLibrary->Load(LibraryPathEntry.first, ShowProgress, LibraryPathEntry.second))
			return true;

	return false;
}

// use to name the library archive
QString swApplication::GetDownloadLibraryPath()
{
	return QDir::cleanPath(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/complete.zip");
}

// use to download the library archive
bool swApplication::DownloadPartsLibrary(const QString& FileName)
{
	const std::optional<QByteArray> Data = swDownloadWithProgress(gMainWindow, QLatin1String(SW_LIBRARY_DOWNLOAD_URL), tr("Download Parts Library"), tr("Downloading the LDraw parts library..."));

	if (!Data)
		return false;

	if (Data->isEmpty())
	{
		QMessageBox::warning(gMainWindow, tr("Download Parts Library"), tr("Could not download the parts library from %1.").arg(QLatin1String(SW_LIBRARY_DOWNLOAD_URL)));
		return false;
	}

	QDir().mkpath(QFileInfo(FileName).absolutePath());

	QSaveFile File(FileName);

	if (!File.open(QIODevice::WriteOnly) || File.write(*Data) != Data->size() || !File.commit())
	{
		QMessageBox::warning(gMainWindow, tr("Download Parts Library"), tr("Could not save the parts library to '%1'.").arg(FileName));
		return false;
	}

	return true;
}

// use to get a missing library
bool swApplication::SetupPartsLibrary()
{
	const QString DownloadPath = GetDownloadLibraryPath();
	const QString Question = tr("StudWorx could not find an LDraw parts library.\n\nDownload the official library (about 140 MB) from library.ldraw.org into '%1'?").arg(QDir::toNativeSeparators(DownloadPath));

	if (QMessageBox::question(gMainWindow, tr("Load Parts Library"), Question, QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes && DownloadPartsLibrary(DownloadPath))
	{
		if (mLibrary->Load(DownloadPath, true, true))
			return true;

		QMessageBox::warning(gMainWindow, tr("Load Parts Library"), tr("Could not open the parts library '%1'.").arg(DownloadPath));
	}

	for (;;)
	{
		const QString FileName = QFileDialog::getOpenFileName(gMainWindow, tr("Open Parts Library Archive"), QDir::homePath(), tr("Parts Library Archives (*.zip);;All Files (*.*)"));

		if (FileName.isEmpty())
			break;

		if (mLibrary->Load(FileName, true, false))
		{
			swSetProfileString(SW_PROFILE_PARTS_LIBRARY, FileName);
			return true;
		}

		QMessageBox::warning(gMainWindow, tr("Load Parts Library"), tr("Could not open the parts library '%1'.").arg(FileName));
	}

	QMessageBox::warning(gMainWindow, tr("Load Parts Library"), tr("No parts library was downloaded or set.\n\nDownload one from %1 and set it under Preferences \u203a Parts Library the next time StudWorx is started.").arg(QLatin1String(SW_LIBRARY_DOWNLOAD_URL)));

	return false;
}

// use to reload the library
bool swApplication::ReloadPartsLibrary(const QString& LibraryPath, bool OfficialLibrary)
{
	if (!mLibrary->Reload(LibraryPath, OfficialLibrary))
		return false;

	std::vector<Project*> Projects = mLibrary->GetExternalProjects();
	Projects.insert(Projects.begin(), mProject);

	std::vector<std::pair<swPartDataReference, swPartDataReference>> Replacements;

	for (Project* CurrentProject : Projects)
	{
		std::vector<std::pair<swPartDataReference, swPartDataReference>> ProjectReplacements = mLibrary->RebindProjectPartData(CurrentProject);

		if (ProjectReplacements.empty())
			continue;

		std::vector<swModel*> UpdatedModels;

		for (const std::unique_ptr<swModel>& Model : CurrentProject->GetModels())
			Model->UpdatePartData(UpdatedModels);

		std::move(ProjectReplacements.begin(), ProjectReplacements.end(), std::back_inserter(Replacements));
	}

	if (!gMainWindow)
		return true;

	for (const std::pair<swPartDataReference, swPartDataReference>& Replacement : Replacements)
	{
		if (gMainWindow->GetInsertPartData() == Replacement.first)
			gMainWindow->SetInsertPartData(Replacement.second.Get());

		gMainWindow->GetPreviewWidget()->RebindPartData(Replacement.first.Get(), Replacement.second.Get());
	}

	// vanished insert part
	if (gMainWindow->GetInsertPartData() && mLibrary->IsRetiredPartData(gMainWindow->GetInsertPartData().Get()))
		gMainWindow->SetInsertPartData(nullptr);

	gMainWindow->UpdateCategories();
	gMainWindow->UpdateInUseCategory();
	gMainWindow->UpdateMinifigureBuilder();
	swView::UpdateProjectViews(mProject);

	return true;
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
		else if (Option == QLatin1String("--unofficial-library"))
			ParseString(Options.UnofficialLibraryPath, true);
		else if (Option == QLatin1String("--shadow-library"))
			ParseString(Options.ShadowLibraryPath, true);
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
					Options.CameraPosition[0] = swVector3LDrawToStudWorx(Options.CameraPosition[0]);
					Options.CameraPosition[1] = swVector3LDrawToStudWorx(Options.CameraPosition[1]);
					Options.CameraPosition[2] = swVector3LDrawToStudWorx(Options.CameraPosition[2]);
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
		else if (Option == QLatin1String("--transparent"))
			Options.TransparentImage = true;
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
		else if (Option == QLatin1String("-bl") || Option == QLatin1String("--export-bricklink"))
		{
			Options.SaveBrickLink = true;
			ParseString(Options.SaveBrickLinkName, false);
		}
		else if (Option == QLatin1String("-html") || Option == QLatin1String("--export-html"))
		{
			Options.SaveHTML = true;
			ParseString(Options.SaveHTMLName, false);
		}
		else if (Option == QLatin1String("-pov") || Option == QLatin1String("--export-povray"))
		{
			Options.SavePOVRay = true;
			ParseString(Options.SavePOVRayName, false);
		}
		else if (Option == QLatin1String("--povray-render"))
		{
			Options.RenderPOVRay = true;
			ParseString(Options.RenderPOVRayName, false);
		}
		else if (Option == QLatin1String("--import-settings"))
			ParseString(Options.SettingsImportName, true);
		else if (Option == QLatin1String("--export-settings"))
			ParseString(Options.SettingsExportName, true);
		else if (Option == QLatin1String("--import-keyboard-shortcuts"))
			ParseString(Options.KeyboardShortcutsImportName, true);
		else if (Option == QLatin1String("--export-keyboard-shortcuts"))
			ParseString(Options.KeyboardShortcutsExportName, true);
		else if (Option == QLatin1String("--import-mouse-shortcuts"))
			ParseString(Options.MouseShortcutsImportName, true);
		else if (Option == QLatin1String("--export-mouse-shortcuts"))
			ParseString(Options.MouseShortcutsExportName, true);
		else if (Option == QLatin1String("--import-categories"))
			ParseString(Options.CategoriesImportName, true);
		else if (Option == QLatin1String("--export-categories"))
			ParseString(Options.CategoriesExportName, true);
		else if (Option == QLatin1String("-v") || Option == QLatin1String("--version"))
		{
			Options.StdOut += tr("StudWorx Version %1\n").arg(SW_VERSION_TEXT);
			Options.StdOut += tr("Compiled %1\n").arg(__DATE__);
			Options.Exit = true;
		}
		else if (Option == QLatin1String("-?") || Option == QLatin1String("--help"))
		{
			Options.StdOut += tr("Usage: studworx [options] [file]\n");
			Options.StdOut += tr("  [options] can be:\n");
			Options.StdOut += tr("  -l, --libpath <path>: Set the Parts Library location to path.\n");
			Options.StdOut += tr("  --unofficial-library <path>: Set the Unofficial Parts Library archive to path.\n");
			Options.StdOut += tr("  --shadow-library <path>: Set the LDCad Shadow Library location to path.\n");
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
			Options.StdOut += tr("  --transparent: Render images with a transparent background (PNG and GIF only).\n");
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
			Options.StdOut += tr("  -ss, --stud-style <style>: Set the stud style (0=Plain, 1=Thin Lines Logo, 2=Outline Logo, 3=Sharp Top Logo, 4=Rounded Top Logo, 5=Flattened Logo, 6=High Contrast, 7=High Contrast with Logo).\n");
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
			Options.StdOut += tr("  -bl, --export-bricklink <outfile.xml>: Export the list of parts used in BrickLink XML format.\n");
			Options.StdOut += tr("  -html, --export-html <folder>: Create an HTML page for the model.\n");
			Options.StdOut += tr("  -pov, --export-povray <outfile.pov>: Export the model to POV-Ray format.\n");
			Options.StdOut += tr("  --povray-render <outfile.png>: Render the model with POV-Ray using the POV-Ray window settings, the picture width and height, and the transparent background.\n");
			Options.StdOut += tr("  --import-settings <infile.ini>: Import the application settings and exit.\n");
			Options.StdOut += tr("  --export-settings <outfile.ini>: Export the application settings and exit.\n");
			Options.StdOut += tr("  --import-keyboard-shortcuts <infile.txt>: Import the keyboard shortcuts and exit.\n");
			Options.StdOut += tr("  --export-keyboard-shortcuts <outfile.txt>: Export the keyboard shortcuts and exit.\n");
			Options.StdOut += tr("  --import-mouse-shortcuts <infile.txt>: Import the mouse shortcuts and exit.\n");
			Options.StdOut += tr("  --export-mouse-shortcuts <outfile.txt>: Export the mouse shortcuts and exit.\n");
			Options.StdOut += tr("  --import-categories <infile.txt>: Import the part categories and exit.\n");
			Options.StdOut += tr("  --export-categories <outfile.txt>: Export the part categories and exit.\n");
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

	const bool SaveAndExit = Options.HasSaveAndExit();

	if (SaveAndExit && Options.ProjectName.isEmpty())
	{
		Options.StdErr += tr("No file name specified.\n");
		Options.ParseOK = false;
	}

	return Options;
}

// use to transfer settings without the interface
static bool swTransferSettings(const swCommandLineOptions& Options, QTextStream& StdErr)
{
	bool TransferOK = true;

	auto ReportError = [&StdErr, &TransferOK](const QString& Message)
	{
		StdErr << Message;
		StdErr.flush();
		TransferOK = false;
	};

	// import before export
	if (!Options.SettingsImportName.isEmpty())
	{
		QSettings SettingsFile(Options.SettingsImportName, QSettings::IniFormat);

		if (!QFileInfo(Options.SettingsImportName).isReadable() || SettingsFile.status() != QSettings::NoError)
			ReportError(swApplication::tr("Error loading settings file '%1'.\n").arg(Options.SettingsImportName));
		else
		{
			QSettings Settings;
			const QStringList Keys = SettingsFile.allKeys();

			for (const QString& Key : Keys)
				Settings.setValue(Key, SettingsFile.value(Key));

			Settings.sync();
		}
	}

	if (!Options.KeyboardShortcutsImportName.isEmpty())
	{
		if (!gKeyboardShortcuts.Load(Options.KeyboardShortcutsImportName))
			ReportError(swApplication::tr("Error loading keyboard shortcuts file '%1'.\n").arg(Options.KeyboardShortcutsImportName));
		else
			swSaveDefaultKeyboardShortcuts();
	}

	if (!Options.MouseShortcutsImportName.isEmpty())
	{
		if (!gMouseShortcuts.Load(Options.MouseShortcutsImportName))
			ReportError(swApplication::tr("Error loading mouse shortcuts file '%1'.\n").arg(Options.MouseShortcutsImportName));
		else
			swSaveDefaultMouseShortcuts();
	}

	if (!Options.CategoriesImportName.isEmpty())
	{
		std::vector<swLibraryCategory> Categories;

		if (!swLoadCategories(Options.CategoriesImportName, Categories) || Categories.empty())
			ReportError(swApplication::tr("Error loading categories file '%1'.\n").arg(Options.CategoriesImportName));
		else
		{
			gCategories = Categories;
			swSaveDefaultCategories();
		}
	}

	if (!Options.SettingsExportName.isEmpty())
	{
		QSettings Settings;
		QSettings SettingsFile(Options.SettingsExportName, QSettings::IniFormat);
		const QStringList Keys = Settings.allKeys();

		SettingsFile.clear();

		for (const QString& Key : Keys)
			SettingsFile.setValue(Key, Settings.value(Key));

		SettingsFile.sync();

		if (SettingsFile.status() != QSettings::NoError)
			ReportError(swApplication::tr("Error saving settings file '%1'.\n").arg(Options.SettingsExportName));
	}

	if (!Options.KeyboardShortcutsExportName.isEmpty())
	{
		swLoadDefaultKeyboardShortcuts();

		if (!gKeyboardShortcuts.Save(Options.KeyboardShortcutsExportName))
			ReportError(swApplication::tr("Error saving keyboard shortcuts file '%1'.\n").arg(Options.KeyboardShortcutsExportName));
	}

	if (!Options.MouseShortcutsExportName.isEmpty())
	{
		swLoadDefaultMouseShortcuts();

		if (!gMouseShortcuts.Save(Options.MouseShortcutsExportName))
			ReportError(swApplication::tr("Error saving mouse shortcuts file '%1'.\n").arg(Options.MouseShortcutsExportName));
	}

	if (!Options.CategoriesExportName.isEmpty())
	{
		swLoadDefaultCategories();

		if (!swSaveCategories(Options.CategoriesExportName, gCategories))
			ReportError(swApplication::tr("Error saving categories file '%1'.\n").arg(Options.CategoriesExportName));
	}

	return TransferOK;
}

swStartupMode swApplication::Initialize(const QList<QPair<QString, bool>>& LibraryPaths)
{
	mDefaultLibraryPaths = LibraryPaths;

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

	if (Options.HasSettingsTransfer())
		return swTransferSettings(Options, StdErr) ? swStartupMode::Success : swStartupMode::Error;

	const bool SaveAndExit = Options.HasSaveAndExit();

#ifdef Q_OS_IOS
	if (Options.SaveImage)
	{
		StdErr << tr("Command line image export is not supported on iOS.\n");
		return swStartupMode::Error;
	}
#endif

#ifndef Q_OS_IOS
	const bool CreateRenderResources = !SaveAndExit || Options.SaveImage || Options.SaveHTML || Options.SavePOVRay || Options.RenderPOVRay;
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

	const QString UnofficialLibraryPath = Options.UnofficialLibraryPath.isEmpty() ? swGetProfileString(SW_PROFILE_UNOFFICIAL_LIBRARY) : Options.UnofficialLibraryPath;

	if (mLibrary == nullptr)
		mLibrary = new swPartsLibrary(Options.StudStyle, Options.StudCylinderColorEnabled);

	mLibrary->SetUnofficialArchivePath(UnofficialLibraryPath);

	if (!LoadPartsLibrary(Options.LibraryPaths.isEmpty() ? LibraryPaths : Options.LibraryPaths, !Options.LibraryPaths.isEmpty(), Options.StudStyle, Options.StudCylinderColorEnabled))
	{
		if (!gMainWindow)
		{
			StdErr << tr("StudWorx could not find an LDraw parts library.\n\nDownload one from %1 and pass it with --libpath.").arg(QLatin1String(SW_LIBRARY_DOWNLOAD_URL)) << "\n";
			StdErr.flush();
			mLibrary->LoadEmptyLibrary();
		}
		else if (!SetupPartsLibrary())
			mLibrary->LoadEmptyLibrary();
	}

	if (!UnofficialLibraryPath.isEmpty() && !mLibrary->HasUnofficialArchive())
	{
		StdErr << tr("Could not open the unofficial library archive '%1'.").arg(UnofficialLibraryPath) << "\n";
		StdErr.flush();
	}

	const QString ShadowLibraryPath = Options.ShadowLibraryPath.isEmpty() ? swGetProfileString(SW_PROFILE_SHADOW_LIBRARY) : Options.ShadowLibraryPath;

	if (!ShadowLibraryPath.isEmpty() && !swShadowLibrary::SetPath(ShadowLibraryPath))
	{
		StdErr << tr("Could not find a shadow library in '%1'.").arg(ShadowLibraryPath) << "\n";
		StdErr.flush();
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

		if (Options.SaveImage || Options.SavePOVRay || Options.RenderPOVRay)
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

		if (ActiveView)
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

			const QString ImageSuffix = QFileInfo(Options.ImageName).suffix().toLower();
			swImageBackground ImageBackground = Options.TransparentImage ? swImageBackground::Transparent : swImageBackground::Opaque;

			// only png and gif files keep transparency
			if (Options.TransparentImage && ImageSuffix != QLatin1String("png") && ImageSuffix != QLatin1String("gif"))
			{
				StdErr << tr("--transparent is ignored for '%1' files: only PNG and GIF files keep transparency.\n").arg(ImageSuffix);
				ImageBackground = swImageBackground::Opaque;
			}

			if (ImageBackground == swImageBackground::Transparent)
				ActiveView->SetBackgroundColorOverride(SW_RGBA(0, 0, 0, 0));

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

			// skip pictures of files without parts
			if (ActiveModel->GetPartsBoundingBoxPoints(swGeometryBoundsMode::AvailableOnly).empty())
			{
				StdErr << tr("The file '%1' has no parts to render.").arg(Options.ProjectName) << '\n';

				return swStartupMode::Error;
			}

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

			const swResult<void> ImagesSaved = ActiveView->SaveStepImages(Frame, Options.ImageStart != Options.ImageEnd, Options.ImageStart, Options.ImageEnd, ImageBackground, ProgressCallback);

			if (!ImagesSaved)
			{
				StdErr << ImagesSaved.error() << '\n';

				return swStartupMode::Error;
			}
		}

		if (Options.SavePOVRay)
		{
			swModel* ActiveModel = ActiveView->GetModel();
			QString FileName = !Options.SavePOVRayName.isEmpty() ? Options.SavePOVRayName : Options.ProjectName;
			const QString Extension = QFileInfo(FileName).suffix().toLower();

			if (Extension.isEmpty())
				FileName += QLatin1String(".pov");
			else if (Extension != QLatin1String("pov"))
				FileName = FileName.left(FileName.length() - Extension.length() - 1) + QLatin1String(".pov");

			ActiveModel->WaitForAssets();

			const bool UsesCameraPosition = Options.SetCameraPosition && Options.Viewpoint == swViewpoint::Count && !Options.SetCameraAngles;

			if (Options.CameraName.isEmpty() && !UsesCameraPosition)
				ActiveView->ZoomExtentsAtStep(ActiveModel->GetCurrentStep(), swGeometryBoundsMode::AvailableOnly, !Options.SetZPlanes);

			swResult<void> ExportResult = mProject->ExportPOVRay(FileName, ActiveView->GetCamera());

			if (ExportResult)
				StdOut << tr("Saved '%1'.\n").arg(FileName);
			else
			{
				OutputFailed = true;

				if (!ExportResult.error().isEmpty())
					StdErr << ExportResult.error() << '\n';
			}
		}

		if (Options.RenderPOVRay)
		{
			swModel* ActiveModel = ActiveView->GetModel();
			QString FileName = !Options.RenderPOVRayName.isEmpty() ? Options.RenderPOVRayName : Options.ProjectName;
			const QString Extension = QFileInfo(FileName).suffix().toLower();

			if (Extension.isEmpty())
				FileName += QLatin1String(".png");
			else if (Extension != QLatin1String("png"))
				FileName = FileName.left(FileName.length() - Extension.length() - 1) + QLatin1String(".png");

			const swPOVRayProgram Program = swFindPOVRay();
			const bool UseLGEO = (ActiveModel->GetPOVRayOptions().UseLGEO || swGetProfileInteger(SW_PROFILE_RENDER_PARTS) == 1) && !swFindLGEOPath().isEmpty();
			QTemporaryDir TemporaryDir;

			if (!Program.Supported)
			{
				OutputFailed = true;
				StdErr << swGetPOVRayMissingMessage(Program) << '\n';
			}
			else if (UseLGEO && !swPOVRayHasIncludeFiles(Program))
			{
				OutputFailed = true;
				StdErr << tr("LGEO parts need the standard POV-Ray include files such as colors.inc, which POV-Ray %1 could not find.\n").arg(Program.Version);
			}
			else if (!TemporaryDir.isValid())
			{
				OutputFailed = true;
				StdErr << tr("Error creating a temporary folder for the POV-Ray scene.\n");
			}
			else
			{
				const QString SceneName = TemporaryDir.filePath(QLatin1String("scene.pov"));

				ActiveModel->WaitForAssets();

				const bool UsesCameraPosition = Options.SetCameraPosition && Options.Viewpoint == swViewpoint::Count && !Options.SetCameraAngles;

				if (Options.CameraName.isEmpty() && !UsesCameraPosition)
					ActiveView->ZoomExtentsAtStep(ActiveModel->GetCurrentStep(), swGeometryBoundsMode::AvailableOnly, !Options.SetZPlanes);

				swResult<void> ExportResult = mProject->ExportPOVRay(SceneName, ActiveView->GetCamera(), SW_STEP_MAXIMUM, Options.TransparentImage);

				if (!ExportResult)
				{
					OutputFailed = true;

					if (!ExportResult.error().isEmpty())
						StdErr << ExportResult.error() << '\n';
				}
				else
				{
					QStringList Arguments;

					Arguments.append(QString::fromLatin1("+I\"%1\"").arg(SceneName));
					Arguments.append(QString::fromLatin1("+O\"%1\"").arg(QFileInfo(FileName).absoluteFilePath()));
					Arguments.append(QString::fromLatin1("+W%1").arg(Options.ImageWidth));
					Arguments.append(QString::fromLatin1("+H%1").arg(Options.ImageHeight));

					if (Options.TransparentImage || swGetProfileInteger(SW_PROFILE_RENDER_TRANSPARENT))
						Arguments.append("+UA");

					Arguments.append(swGetPOVRayArguments(Program, swGetProfileRenderQuality(), UseLGEO));
#ifndef Q_OS_LINUX
					Arguments.append("+FN");
					Arguments.append("-D");
#endif

					QProcess Process;

					Process.setEnvironment(swGetPOVRayEnvironment());
					Process.setProcessChannelMode(QProcess::MergedChannels);
					Process.start(Program.Path, Arguments);

					// only the render start is timed
					if (!Process.waitForStarted() || !Process.waitForFinished(-1) || Process.exitStatus() != QProcess::NormalExit || Process.exitCode() != 0 || !QFileInfo::exists(FileName))
					{
						OutputFailed = true;
						StdErr << tr("Error rendering '%1' with POV-Ray:\n").arg(FileName);

						const QStringList OutputLines = QString::fromLocal8Bit(Process.readAll()).trimmed().split('\n');

						StdErr << OutputLines.mid(std::max(0, static_cast<int>(OutputLines.size()) - 20)).join('\n') << '\n';
					}
					else
						StdOut << tr("Saved '%1'.\n").arg(FileName);
				}
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

		if (Options.SaveBrickLink)
		{
			QString FileName = !Options.SaveBrickLinkName.isEmpty() ? Options.SaveBrickLinkName : Options.ProjectName;
			const QString Extension = QFileInfo(FileName).suffix().toLower();

			if (Extension.isEmpty())
				FileName += QLatin1String(".xml");
			else if (Extension != QLatin1String("xml"))
				FileName = FileName.left(FileName.length() - Extension.length() - 1) + QLatin1String(".xml");

			swResult<void> ExportResult = mProject->ExportBrickLink(FileName);

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
	Options.UnofficialLibraryPath = swGetProfileString(SW_PROFILE_UNOFFICIAL_LIBRARY);
	Options.ShadowLibraryPath = swGetProfileString(SW_PROFILE_SHADOW_LIBRARY);
	Options.MinifigureBuilderSettingsPath = swGetProfileString(SW_PROFILE_MINIFIGURE_BUILDER_SETTINGS);
	Options.ColorConfigPath = swGetProfileString(SW_PROFILE_COLOR_CONFIGURATION);
	Options.POVRayPath = swGetProfileString(SW_PROFILE_POVRAY_PATH);
	Options.LGEOPath = swGetProfileString(SW_PROFILE_POVRAY_LGEO_PATH);
	Options.DefaultAuthor = swGetProfileString(SW_PROFILE_DEFAULT_AUTHOR_NAME);
	Options.Language = swGetProfileString(SW_PROFILE_LANGUAGE);
	Options.Instructions.Load();

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
	bool UnofficialLibraryChanged = Options.UnofficialLibraryPath != swGetProfileString(SW_PROFILE_UNOFFICIAL_LIBRARY);
	bool ShadowLibraryChanged = Options.ShadowLibraryPath != swGetProfileString(SW_PROFILE_SHADOW_LIBRARY);
	bool ShadowLibraryEnabledChanged = Options.Preferences.mShadowLibraryEnabled != mPreferences.mShadowLibraryEnabled;
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
	const bool FlexSpreadModeChanged = Options.Preferences.mFlexSpreadMode != mPreferences.mFlexSpreadMode;
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

	// redraw flexible part paths
	if (FlexSpreadModeChanged && mProject)
		for (const std::unique_ptr<swModel>& Model : mProject->GetModels())
			for (const std::unique_ptr<swPart>& Part : Model->GetParts())
				if (!Part->GetControlPoints().empty())
					Part->SetControlPoints(Part->GetControlPoints());
	if (PartsRenderingChanged && gMainWindow && gMainWindow->GetPartSelectionWidget())
		gMainWindow->GetPartSelectionWidget()->UpdateThumbnails();
	if (gMainWindow && gMainWindow->GetPreviewWidget())
		gMainWindow->GetPreviewWidget()->UpdateSquareViewport();

	swSetProfileString(SW_PROFILE_DEFAULT_AUTHOR_NAME, Options.DefaultAuthor);
	swSetProfileString(SW_PROFILE_PARTS_LIBRARY, Options.LibraryPath);
	swSetProfileString(SW_PROFILE_UNOFFICIAL_LIBRARY, Options.UnofficialLibraryPath);
	swSetProfileString(SW_PROFILE_SHADOW_LIBRARY, Options.ShadowLibraryPath);
	swSetProfileString(SW_PROFILE_MINIFIGURE_BUILDER_SETTINGS, Options.MinifigureBuilderSettingsPath);
	swSetProfileString(SW_PROFILE_POVRAY_PATH, Options.POVRayPath);
	swSetProfileString(SW_PROFILE_POVRAY_LGEO_PATH, Options.LGEOPath);
	swSetProfileString(SW_PROFILE_LANGUAGE, Options.Language);
	swSetProfileInteger(SW_PROFILE_ANTIALIASING_SAMPLES, Options.AASamples);
	swSetProfileStudStyle(Options.StudStyle);
	Options.Instructions.Save();

	if (gMainWindow)
		gMainWindow->UpdateInstructionsOptions();

	// libraries reload while running
	if (LibraryChanged || UnofficialLibraryChanged)
	{
		const char* PathEnvironment = getenv("STUDWORX_LIBRARY");
		const bool LibraryOverridden = PathEnvironment && PathEnvironment[0];

		mLibrary->SetUnofficialArchivePath(Options.UnofficialLibraryPath);

		// the environment library stays in use
		if (LibraryChanged && LibraryOverridden)
		{
			if (UnofficialLibraryChanged && !ReloadPartsLibrary(mLibrary->GetLibraryPath(), mLibrary->IsOfficialLibrary()))
				QMessageBox::warning(gMainWindow, tr("Load Parts Library"), tr("Could not reload the parts library '%1'.").arg(mLibrary->GetLibraryPath()));
		}
		// an empty path falls back to the default libraries
		else if (LibraryChanged && Options.LibraryPath.isEmpty())
		{
			bool LibraryLoaded = false;

			for (const QPair<QString, bool>& LibraryPathEntry : mDefaultLibraryPaths)
			{
				if (ReloadPartsLibrary(LibraryPathEntry.first, LibraryPathEntry.second))
				{
					LibraryLoaded = true;
					break;
				}
			}

			if (!LibraryLoaded)
				QMessageBox::warning(gMainWindow, tr("Load Parts Library"), tr("Could not find a default parts library, '%1' stays in use until StudWorx starts again and offers to download one.").arg(mLibrary->GetLibraryPath()));
		}
		else
		{
			const QString LibraryPath = LibraryChanged ? Options.LibraryPath : mLibrary->GetLibraryPath();
			const bool OfficialLibrary = LibraryChanged ? false : mLibrary->IsOfficialLibrary();

			if (!ReloadPartsLibrary(LibraryPath, OfficialLibrary))
				QMessageBox::warning(gMainWindow, tr("Load Parts Library"), tr("Could not open the parts library '%1'.").arg(LibraryPath));
		}

		if (UnofficialLibraryChanged && !Options.UnofficialLibraryPath.isEmpty() && !mLibrary->HasUnofficialArchive())
			QMessageBox::warning(gMainWindow, tr("Load Parts Library"), tr("Could not open the unofficial library archive '%1'.").arg(Options.UnofficialLibraryPath));
	}

	// shadow library reloads while running
	if (ShadowLibraryChanged)
	{
		if (!swShadowLibrary::SetPath(Options.ShadowLibraryPath) && !Options.ShadowLibraryPath.isEmpty())
			QMessageBox::warning(gMainWindow, tr("Load Part Connections"), tr("Could not find a shadow library in '%1'.").arg(Options.ShadowLibraryPath));
	}

	if ((ShadowLibraryChanged || ShadowLibraryEnabledChanged) && gMainWindow)
		gMainWindow->UpdateShadowLibraryAction();

	// name the settings that wait for a restart
	if (LanguageChanged && AAChanged)
		QMessageBox::information(gMainWindow, tr("Preferences"), tr("The new language and antialiasing settings apply the next time you start StudWorx."));
	else if (LanguageChanged)
		QMessageBox::information(gMainWindow, tr("Preferences"), tr("The new language applies the next time you start StudWorx."));
	else if (AAChanged)
		QMessageBox::information(gMainWindow, tr("Preferences"), tr("The new antialiasing setting applies the next time you start StudWorx."));

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

	// both paths need the new config
	if (ColorConfigChanged)
		swSetProfileString(SW_PROFILE_COLOR_CONFIGURATION, Options.ColorConfigPath);

	if (StudStyleChanged || StudCylinderColorEnabledChanged)
		swGetPartsLibrary()->UpdateStudStyleAndColors(Options.StudStyle, Options.Preferences.mStudCylinderColorEnabled);
	else if (ColorConfigChanged || EdgeColorSettingsChanged)
		swGetPartsLibrary()->LoadColors();

	gMainWindow->SetShadingMode(Options.Preferences.mShadingMode);
	swView::UpdateAllViews();
}
