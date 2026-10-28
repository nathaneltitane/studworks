#include "sw_global.h"
#include "sw_profile.h"
#include "sw_context.h"
#include "sw_model.h"
#include "sw_project.h"
#include "sw_view_sphere.h"
#include "sw_dialog_render.h"
#include "sw_library.h"
#include "sw_widget_selection_part.h"
#include "sw_colors.h"

swProfileEntry::swProfileEntry(const char* Section, const char* Key, int DefaultValue)
{
	mSection = Section;
	mKey = Key;
	mDefault = DefaultValue;
}

swProfileEntry::swProfileEntry(const char* Section, const char* Key, uint DefaultValue)
{
	mSection = Section;
	mKey = Key;
	mDefault = DefaultValue;
}

swProfileEntry::swProfileEntry(const char* Section, const char* Key, float DefaultValue)
{
	mSection = Section;
	mKey = Key;
	mDefault = DefaultValue;
}

swProfileEntry::swProfileEntry(const char* Section, const char* Key, const char* DefaultValue)
{
	mSection = Section;
	mKey = Key;
	mDefault = QString(DefaultValue);
}

swProfileEntry::swProfileEntry(const char* Section, const char* Key, const QStringList& /*StringList*/)
{
	mSection = Section;
	mKey = Key;
	mDefault = QStringList();
}

swProfileEntry::swProfileEntry(const char* Section, const char* Key)
{
	mSection = Section;
	mKey = Key;
	mDefault = QByteArray();
}

static swProfileEntry gProfileEntries[SW_PROFILE_KEY_COUNT] =
{
	swProfileEntry("Settings", "FixedAxes", false),                                                        // SW_PROFILE_FIXED_AXES
	swProfileEntry("Settings", "LineWidth", 1.0f),                                                         // SW_PROFILE_LINE_WIDTH
	swProfileEntry("Settings", "AllowLOD", true),                                                          // SW_PROFILE_ALLOW_LOD
	swProfileEntry("Settings", "LODDistance", 750.0f),                                                     // SW_PROFILE_LOD_DISTANCE
	swProfileEntry("Settings", "FadeSteps", false),                                                        // SW_PROFILE_FADE_STEPS
	swProfileEntry("Settings", "FadeStepsColor", SW_RGBA(128, 128, 128, 128)),                             // SW_PROFILE_FADE_STEPS_COLOR
	swProfileEntry("Settings", "HighlightNewParts", 0),                                                   // SW_PROFILE_HIGHLIGHT_NEW_PARTS
	swProfileEntry("Settings", "HighlightNewPartsColor", SW_RGBA(255, 242, 0, 192)),                      // SW_PROFILE_HIGHLIGHT_NEW_PARTS_COLOR
	swProfileEntry("Settings", "ShadingMode", static_cast<int>(swShadingMode::DefaultLights)),             // SW_PROFILE_SHADING_MODE
	swProfileEntry("Settings", "LightingStyle", static_cast<int>(swLightingStyle::Standard)),               // SW_PROFILE_LIGHTING_STYLE
	swProfileEntry("Settings", "HighlightRenderMode", static_cast<int>(swHighlightRenderMode::Outline)),      // SW_PROFILE_HIGHLIGHT_RENDER_MODE
	swProfileEntry("Settings", "SelectionOutlineWidth", 4),                                                // SW_PROFILE_SELECTION_OUTLINE_WIDTH
	swProfileEntry("Settings", "BackgroundColor", SW_RGB(49, 52, 55)),                                     // SW_PROFILE_BACKGROUND_COLOR
	swProfileEntry("Settings", "DrawAxes", 0),                                                             // SW_PROFILE_DRAW_AXES
	swProfileEntry("Settings", "FollowStepViews", 1),                                                      // SW_PROFILE_FOLLOW_STEP_VIEWS
	swProfileEntry("Settings", "CutawaySize", 4),                                                          // SW_PROFILE_CUTAWAY_SIZE
	swProfileEntry("Settings", "TurntableIdleDuration", 300),                                                  // SW_PROFILE_TURNTABLE_IDLE_DURATION
	swProfileEntry("Settings", "TurntableTurnTime", 30),                                                   // SW_PROFILE_TURNTABLE_TURN_TIME
	swProfileEntry("Settings", "DrawAxesLocation", static_cast<int>(swAxisIconLocation::BottomLeft)),      // SW_PROFILE_DRAW_AXES_LOCATION
	swProfileEntry("Settings", "TextColor", SW_RGBA(214, 220, 227, 255)),                                  // SW_PROFILE_TEXT_COLOR
	swProfileEntry("Settings", "TextHaloColor", SW_RGBA(17, 19, 24, 160)),                                // SW_PROFILE_TEXT_HALO_COLOR
	swProfileEntry("Settings", "MarqueeBorderColor", SW_RGBA(64, 64, 255, 255)),                           // SW_PROFILE_MARQUEE_BORDER_COLOR
	swProfileEntry("Settings", "MarqueeFillColor", SW_RGBA(64, 64, 255, 64)),                              // SW_PROFILE_MARQUEE_FILL_COLOR
	swProfileEntry("Settings", "OverlayColor", SW_RGBA(0, 0, 0, 255)),                                     // SW_PROFILE_OVERLAY_COLOR
	swProfileEntry("Settings", "CutawayShadeColor", SW_RGB(255, 255, 255)),                                // SW_PROFILE_CUTAWAY_SHADE_COLOR
	swProfileEntry("Settings", "CutawayShadeTransparency", 95),                                            // SW_PROFILE_CUTAWAY_SHADE_TRANSPARENCY
	swProfileEntry("Settings", "ActiveViewColor", SW_RGBA(41, 128, 185, 255)),                             // SW_PROFILE_ACTIVE_VIEW_COLOR
	swProfileEntry("Settings", "InactiveViewColor", SW_RGBA(69, 69, 69, 255)),                             // SW_PROFILE_INACTIVE_VIEW_COLOR
	swProfileEntry("Settings", "DrawEdgeLines", 1),                                                        // SW_PROFILE_DRAW_EDGE_LINES
	swProfileEntry("Settings", "DrawConditionalLines", 1),                                                 // SW_PROFILE_DRAW_CONDITIONAL_LINES
	swProfileEntry("Settings", "PreviewSyncRendering", 1),                                                   // SW_PROFILE_PREVIEW_SYNCHRONIZED_RENDERING
	swProfileEntry("Settings", "PreviewShadingMode", static_cast<int>(swShadingMode::DefaultLights)),        // SW_PROFILE_PREVIEW_SHADING_MODE
	swProfileEntry("Settings", "PreviewDrawEdgeLines", 1),                                                   // SW_PROFILE_PREVIEW_DRAW_EDGE_LINES
	swProfileEntry("Settings", "PreviewDrawConditionalLines", 1),                                            // SW_PROFILE_PREVIEW_DRAW_CONDITIONAL_LINES
	swProfileEntry("Settings", "PreviewLineWidth", 1.0f),                                                    // SW_PROFILE_PREVIEW_LINE_WIDTH
	swProfileEntry("Settings", "PartsSyncRendering", 1),                                                    // SW_PROFILE_PARTS_SYNCHRONIZED_RENDERING
	swProfileEntry("Settings", "PartsShadingMode", static_cast<int>(swShadingMode::DefaultLights)),         // SW_PROFILE_PARTS_SHADING_MODE
	swProfileEntry("Settings", "PartsDrawEdgeLines", 1),                                                    // SW_PROFILE_PARTS_DRAW_EDGE_LINES
	swProfileEntry("Settings", "PartsDrawConditionalLines", 1),                                             // SW_PROFILE_PARTS_DRAW_CONDITIONAL_LINES
	swProfileEntry("Settings", "PartsLineWidth", 1.0f),                                                     // SW_PROFILE_PARTS_LINE_WIDTH
	swProfileEntry("Settings", "GridStuds", 1),                                                            // SW_PROFILE_GRID_STUDS
	swProfileEntry("Settings", "GridStudColor", SW_RGBA(24, 24, 24, 192)),                                 // SW_PROFILE_GRID_STUD_COLOR
	swProfileEntry("Settings", "GridLines", 1),                                                            // SW_PROFILE_GRID_LINES
	swProfileEntry("Settings", "GridLineSpacing", 5),                                                      // SW_PROFILE_GRID_LINE_SPACING
	swProfileEntry("Settings", "GridLineColor", SW_RGBA(24, 24, 24, 255)),                                 // SW_PROFILE_GRID_LINE_COLOR
	swProfileEntry("Settings", "GridOrigin", 0),                                                           // SW_PROFILE_GRID_ORIGIN
	swProfileEntry("Settings", "GridHeightXZ", 0),                                                         // SW_PROFILE_GRID_HEIGHT_XZ
	swProfileEntry("Settings", "GridHeightYZ", 0),                                                         // SW_PROFILE_GRID_HEIGHT_YZ
	swProfileEntry("Settings", "GridHeightBehindModel", 0),                                                // SW_PROFILE_GRID_HEIGHT_BEHIND_MODEL
	swProfileEntry("Settings", "GridAxisZ", 1),                                                            // SW_PROFILE_GRID_AXIS_Z
	swProfileEntry("Settings", "AASamples", 1),                                                            // SW_PROFILE_ANTIALIASING_SAMPLES
	swProfileEntry("Settings", "ViewSphereEnabled", 1),                                                    // SW_PROFILE_VIEW_SPHERE_ENABLED
	swProfileEntry("Settings", "ViewSphereLocation", (int)swViewSphereLocation::TopRight),                 // SW_PROFILE_VIEW_SPHERE_LOCATION
	swProfileEntry("Settings", "ViewSphereSize", 100),                                                     // SW_PROFILE_VIEW_SPHERE_SIZE
	swProfileEntry("Settings", "ViewSphereColor", SW_RGBA(27, 31, 36, 255)),                               // SW_PROFILE_VIEW_SPHERE_COLOR
	swProfileEntry("Settings", "ViewSphereTextColor", SW_RGBA(232, 237, 242, 255)),                        // SW_PROFILE_VIEW_SPHERE_TEXT_COLOR
	swProfileEntry("Settings", "ViewSphereHighlightColor", SW_RGBA(41, 128, 185, 255)),                    // SW_PROFILE_VIEW_SPHERE_HIGHLIGHT_COLOR
	swProfileEntry("Settings", "AxisXColor", SW_RGBA(200, 40, 60, 255)),                                   // SW_PROFILE_AXIS_X_COLOR
	swProfileEntry("Settings", "AxisYColor", SW_RGBA(100, 200, 20, 255)),                                  // SW_PROFILE_AXIS_Y_COLOR
	swProfileEntry("Settings", "AxisZColor", SW_RGBA(40, 60, 230, 255)),                                   // SW_PROFILE_AXIS_Z_COLOR
	swProfileEntry("Settings", "ObjectSelectedColor", static_cast<uint>(SW_RGBA(229, 76, 102, 255))),      // SW_PROFILE_OBJECT_SELECTED_COLOR
	swProfileEntry("Settings", "ObjectFocusedColor", static_cast<uint>(SW_RGBA(102, 76, 229, 255))),       // SW_PROFILE_OBJECT_FOCUSED_COLOR
	swProfileEntry("Settings", "CameraColor", static_cast<uint>(SW_RGBA(128, 204, 128, 255))),             // SW_PROFILE_CAMERA_COLOR
	swProfileEntry("Settings", "LightColor", static_cast<uint>(SW_RGBA(128, 204, 128, 255))),              // SW_PROFILE_LIGHT_COLOR
	swProfileEntry("Settings", "ControlPointColor", static_cast<uint>(SW_RGBA(128, 204, 128, 128))),       // SW_PROFILE_CONTROL_POINT_COLOR
	swProfileEntry("Settings", "ControlPointFocusedColor", static_cast<uint>(SW_RGBA(102, 76, 229, 128))), // SW_PROFILE_CONTROL_POINT_FOCUSED_COLOR

	swProfileEntry("Settings", "Language", ""),                                                // SW_PROFILE_LANGUAGE
	swProfileEntry("Settings", "ColorTheme", static_cast<int>(swColorTheme::Dark)),            // SW_PROFILE_COLOR_THEME
	swProfileEntry("Settings", "ProjectsPath", ""),                                            // SW_PROFILE_PROJECTS_PATH
	swProfileEntry("Settings", "PartsLibrary", ""),                                           // SW_PROFILE_PARTS_LIBRARY
	swProfileEntry("Settings", "PartPalettes"),                                               // SW_PROFILE_PART_PALETTES
	swProfileEntry("Settings", "MinifigSettings", ""),                                         // SW_PROFILE_MINIFIGURE_BUILDER_SETTINGS
	swProfileEntry("Settings", "ColorConfig", ""),                                             // SW_PROFILE_COLOR_CONFIGURATION
	swProfileEntry("Settings", "Shortcuts"),                                                   // SW_PROFILE_KEYBOARD_SHORTCUTS
	swProfileEntry("Settings", "MouseShortcuts", QStringList()),                               // SW_PROFILE_MOUSE_SHORTCUTS
	swProfileEntry("Settings", "Categories"),                                                  // SW_PROFILE_CATEGORIES
	swProfileEntry("Settings", "RecentFile1", ""),                                             // SW_PROFILE_RECENT_FILE1
	swProfileEntry("Settings", "RecentFile2", ""),                                             // SW_PROFILE_RECENT_FILE2
	swProfileEntry("Settings", "RecentFile3", ""),                                             // SW_PROFILE_RECENT_FILE3
	swProfileEntry("Settings", "RecentFile4", ""),                                             // SW_PROFILE_RECENT_FILE4
	swProfileEntry("Settings", "AutoLoadMostRecent", false),                                   // SW_PROFILE_AUTO_LOAD_MOST_RECENT
	swProfileEntry("Settings", "RestoreTabLayout", true),                                      // SW_PROFILE_RESTORE_TAB_LAYOUT
	swProfileEntry("Settings", "AngleSnapCustom", 7.5f),                                       // SW_PROFILE_ANGLE_SNAP_CUSTOM
	swProfileEntry("Settings", "OrbitSnap", false),                                            // SW_PROFILE_ORBIT_SNAP
	swProfileEntry("Settings", "FirstPartAtOrigin", false),                                   // SW_PROFILE_FIRST_PART_AT_ORIGIN
	swProfileEntry("Settings", "TerminalFollowModel", true),                                   // SW_PROFILE_TERMINAL_FOLLOW_MODEL
	swProfileEntry("Settings", "InsertColorMode", 0),                                          // SW_PROFILE_INSERT_COLOR_MODE
	swProfileEntry("Settings", "WatchFileChanges", true),                                      // SW_PROFILE_WATCH_FILE_CHANGES
	swProfileEntry("Settings", "LoadPartsBeforeShowing", false),                              // SW_PROFILE_LOAD_PARTS_BEFORE_SHOWING
	swProfileEntry("Settings", "MigrateMetaCommands", false),                                  // SW_PROFILE_MIGRATE_META_COMMANDS
	swProfileEntry("Settings", "AutosaveInterval", 10),                                        // SW_PROFILE_AUTO_SAVE_INTERVAL
	swProfileEntry("Settings", "MouseSensitivity", 11),                                        // SW_PROFILE_MOUSE_SENSITIVITY
	swProfileEntry("Settings", "ImageWidth", 1280),                                            // SW_PROFILE_IMAGE_WIDTH
	swProfileEntry("Settings", "ImageHeight", 720),                                            // SW_PROFILE_IMAGE_HEIGHT
	swProfileEntry("Settings", "ImageExtension", ".png"),                                      // SW_PROFILE_IMAGE_EXTENSION
	swProfileEntry("Settings", "PartsListIcons", 64),                                         // SW_PROFILE_PARTS_LIST_ICONS
	swProfileEntry("Settings", "PartsListNames", 1),                                          // SW_PROFILE_PARTS_LIST_NAMES
	swProfileEntry("Settings", "PartsListFixedColor", static_cast<uint>(SW_COLOR_NO_COLOR)),                                    // SW_PROFILE_PARTS_LIST_COLOR
	swProfileEntry("Settings", "PartsListColorLocked", false),                               // SW_PROFILE_PARTS_LIST_COLOR_LOCKED
	swProfileEntry("Settings", "PartsListDecorated", 1),                                      // SW_PROFILE_PARTS_LIST_DECORATED
	swProfileEntry("Settings", "PartsListAliases", 1),                                        // SW_PROFILE_PARTS_LIST_ALIASES
	swProfileEntry("Settings", "PartsListListMode", 0),                                       // SW_PROFILE_PARTS_LIST_LIST_MODE
	swProfileEntry("Settings", "PartsPopupIcons", 64),                                            // SW_PROFILE_PARTS_POPUP_ICONS
	swProfileEntry("Settings", "PartsPopupNames", 1),                                             // SW_PROFILE_PARTS_POPUP_NAMES
	swProfileEntry("Settings", "PartsPopupDecorated", 1),                                         // SW_PROFILE_PARTS_POPUP_DECORATED
	swProfileEntry("Settings", "PartsPopupAliases", 1),                                           // SW_PROFILE_PARTS_POPUP_ALIASES
	swProfileEntry("Settings", "PartsPopupListMode", 0),                                          // SW_PROFILE_PARTS_POPUP_LIST_MODE
	swProfileEntry("Settings", "PartsListPartFilter", static_cast<int>(swPartFilterType::Default)),                                    // SW_PROFILE_PARTS_LIST_PART_FILTER
	swProfileEntry("Settings", "PartsListCaseSensitiveFilter", 0),                            // SW_PROFILE_PARTS_LIST_CASE_SENSITIVE_FILTER
	swProfileEntry("Settings", "PartsListFileNameFilter", 1),                                 // SW_PROFILE_PARTS_LIST_FILE_NAME_FILTER
	swProfileEntry("Settings", "PartsListPartDescriptionFilter", 1),                         // SW_PROFILE_PARTS_LIST_PART_DESCRIPTION_FILTER
	swProfileEntry("Settings", "StudStyle", static_cast<int>(swStudStyle::Plain)),                                                // SW_PROFILE_STUD_STYLE

	swProfileEntry("Defaults", "Author", "Author"),                                            // SW_PROFILE_DEFAULT_AUTHOR_NAME
	swProfileEntry("Defaults", "AmbientColor", SW_RGB(75, 75, 75)),                            // SW_PROFILE_DEFAULT_AMBIENT_COLOR

	swProfileEntry("HTML", "Options", SW_HTML_SINGLE_PAGE),                                     // SW_PROFILE_HTML_OPTIONS
	swProfileEntry("HTML", "ImageOptions", SW_IMAGE_TRANSPARENT),                              // SW_PROFILE_HTML_IMAGE_OPTIONS
	swProfileEntry("HTML", "ImageWidth", 640),                                                 // SW_PROFILE_HTML_IMAGE_WIDTH
	swProfileEntry("HTML", "ImageHeight", 480),                                                // SW_PROFILE_HTML_IMAGE_HEIGHT

	swProfileEntry("POVRay", "Path", "/usr/bin/povray"),                                       // SW_PROFILE_POVRAY_PATH
	swProfileEntry("POVRay", "LGEOPath", ""),                                                  // SW_PROFILE_POVRAY_LGEO_PATH

	swProfileEntry("Render", "Width", 1280),                                                   // SW_PROFILE_RENDER_WIDTH
	swProfileEntry("Render", "Height", 720),                                                   // SW_PROFILE_RENDER_HEIGHT
	swProfileEntry("Render", "Quality", (int)swPOVRayRenderQuality::High),                     // SW_PROFILE_RENDER_QUALITY

	swProfileEntry("Blender", "Path", ""),                                                     // SW_PROFILE_BLENDER_PATH
	swProfileEntry("Blender", "LDrawConfigPath", ""),                                          // SW_PROFILE_BLENDER_LDRAW_CONFIGURATION_PATH
	swProfileEntry("Blender", "Version", ""),                                                  // SW_PROFILE_BLENDER_VERSION
	swProfileEntry("Blender", "AddonVersion", ""),                                             // SW_PROFILE_BLENDER_ADDON_VERSION
	swProfileEntry("Blender", "AddonVersionCheck", 1),                                         // SW_PROFILE_BLENDER_ADDON_VERSION_CHECK
	swProfileEntry("Blender", "ImportModule", ""),                                             // SW_PROFILE_BLENDER_IMPORT_MODULE

	swProfileEntry("Settings", "PreviewViewSphereEnabled", 0),                                    // SW_PROFILE_PREVIEW_VIEW_SPHERE_ENABLED
	swProfileEntry("Settings", "PreviewViewSphereSize", 75),                                      // SW_PROFILE_PREVIEW_VIEW_SPHERE_SIZE
	swProfileEntry("Settings", "PreviewViewSphereLocation", (int)swViewSphereLocation::TopRight), // SW_PROFILE_PREVIEW_VIEW_SPHERE_LOCATION
	swProfileEntry("Settings", "DrawPreviewAxis", 0),                                             // SW_PROFILE_PREVIEW_DRAW_AXES

	swProfileEntry("Settings", "StudCylinderColorEnabled", true),                              // SW_PROFILE_STUD_CYLINDER_COLOR_ENABLED
	swProfileEntry("Settings", "StudCylinderColor", SW_RGBA(27, 42, 52, 255)),                 // SW_PROFILE_STUD_CYLINDER_COLOR
	swProfileEntry("Settings", "PartEdgeColorEnabled", true),                                 // SW_PROFILE_PART_EDGE_COLOR_ENABLED
	swProfileEntry("Settings", "PartEdgeColor", SW_RGBA(0, 0, 0, 255)),                       // SW_PROFILE_PART_EDGE_COLOR
	swProfileEntry("Settings", "BlackEdgeColorEnabled", true),                                 // SW_PROFILE_BLACK_EDGE_COLOR_ENABLED
	swProfileEntry("Settings", "BlackEdgeColor", SW_RGBA(255, 255, 255, 255)),                 // SW_PROFILE_BLACK_EDGE_COLOR
	swProfileEntry("Settings", "DarkEdgeColorEnabled", true),                                  // SW_PROFILE_DARK_EDGE_COLOR_ENABLED
	swProfileEntry("Settings", "DarkEdgeColor", SW_RGBA(27, 42, 52, 255)),                     // SW_PROFILE_DARK_EDGE_COLOR
	swProfileEntry("Settings", "PartEdgeContrast", 0.5f),                                     // SW_PROFILE_PART_EDGE_CONTRAST
	swProfileEntry("Settings", "PartColorValueLDIndex", 0.5f),                                // SW_PROFILE_PART_COLOR_VALUE_LD_INDEX
	swProfileEntry("Settings", "AutomateEdgeColor", 0),                                        // SW_PROFILE_AUTOMATE_EDGE_COLOR

	swProfileEntry("Instructions", "PageWidth", 1008.0f),                                      // SW_PROFILE_INSTRUCTIONS_PAGE_WIDTH
	swProfileEntry("Instructions", "PageHeight", 612.0f),                                      // SW_PROFILE_INSTRUCTIONS_PAGE_HEIGHT
	swProfileEntry("Instructions", "MarginLeft", 36.0f),                                       // SW_PROFILE_INSTRUCTIONS_MARGIN_LEFT
	swProfileEntry("Instructions", "MarginRight", 36.0f),                                      // SW_PROFILE_INSTRUCTIONS_MARGIN_RIGHT
	swProfileEntry("Instructions", "MarginTop", 36.0f),                                        // SW_PROFILE_INSTRUCTIONS_MARGIN_TOP
	swProfileEntry("Instructions", "MarginBottom", 36.0f),                                     // SW_PROFILE_INSTRUCTIONS_MARGIN_BOTTOM
	swProfileEntry("Instructions", "PageUnits", 3),                                            // SW_PROFILE_INSTRUCTIONS_PAGE_UNITS
	swProfileEntry("Instructions", "MarginUnits", 3),                                          // SW_PROFILE_INSTRUCTIONS_MARGIN_UNITS
	swProfileEntry("Instructions", "ShowGrid", 1),                                             // SW_PROFILE_INSTRUCTIONS_SHOW_GRID
	swProfileEntry("Instructions", "SnapToGrid", 1),                                           // SW_PROFILE_INSTRUCTIONS_SNAP_TO_GRID
	swProfileEntry("Instructions", "GridSize", 72.0f),                                         // SW_PROFILE_INSTRUCTIONS_GRID_SIZE
	swProfileEntry("Instructions", "GridWithinMargins", 1),                                    // SW_PROFILE_INSTRUCTIONS_GRID_WITHIN_MARGINS
	swProfileEntry("Instructions", "ShowMargins", 1),                                          // SW_PROFILE_INSTRUCTIONS_SHOW_MARGINS
	swProfileEntry("Instructions", "EditorHighlight", 1),                                      // SW_PROFILE_INSTRUCTIONS_EDITOR_HIGHLIGHT
	swProfileEntry("Instructions", "EditorNumbers", 1),                                        // SW_PROFILE_INSTRUCTIONS_EDITOR_NUMBERS
	swProfileEntry("Instructions", "EditorWrap", 0),                                           // SW_PROFILE_INSTRUCTIONS_EDITOR_WRAP
	swProfileEntry("Instructions", "EditorMarkers", 1),                                        // SW_PROFILE_INSTRUCTIONS_EDITOR_MARKERS
	swProfileEntry("Instructions", "NewModelDefaults", 1),                                     // SW_PROFILE_INSTRUCTIONS_NEW_MODEL_DEFAULTS
	swProfileEntry("Instructions", "GridDivisions", 8),                                        // SW_PROFILE_INSTRUCTIONS_GRID_DIVISIONS
	swProfileEntry("Instructions", "GridPrimaryColor", SW_RGBA(61, 174, 233, 110)),            // SW_PROFILE_INSTRUCTIONS_GRID_PRIMARY_COLOR
	swProfileEntry("Instructions", "GridSecondaryColor", SW_RGBA(61, 174, 233, 40)),           // SW_PROFILE_INSTRUCTIONS_GRID_SECONDARY_COLOR
	swProfileEntry("Instructions", "MarginColor", SW_RGBA(61, 174, 233, 160)),                 // SW_PROFILE_INSTRUCTIONS_MARGIN_COLOR
	swProfileEntry("InstructionsDefaults", "PageWidth", 1008.0f),                              // SW_PROFILE_INSTRUCTIONS_DEFAULT_PAGE_WIDTH
	swProfileEntry("InstructionsDefaults", "PageHeight", 612.0f),                              // SW_PROFILE_INSTRUCTIONS_DEFAULT_PAGE_HEIGHT
	swProfileEntry("InstructionsDefaults", "MarginLeft", 36.0f),                               // SW_PROFILE_INSTRUCTIONS_DEFAULT_MARGIN_LEFT
	swProfileEntry("InstructionsDefaults", "MarginRight", 36.0f),                              // SW_PROFILE_INSTRUCTIONS_DEFAULT_MARGIN_RIGHT
	swProfileEntry("InstructionsDefaults", "MarginTop", 36.0f),                                // SW_PROFILE_INSTRUCTIONS_DEFAULT_MARGIN_TOP
	swProfileEntry("InstructionsDefaults", "MarginBottom", 36.0f),                             // SW_PROFILE_INSTRUCTIONS_DEFAULT_MARGIN_BOTTOM
	swProfileEntry("InstructionsDefaults", "PageUnits", 3),                                    // SW_PROFILE_INSTRUCTIONS_DEFAULT_PAGE_UNITS
	swProfileEntry("InstructionsDefaults", "MarginUnits", 3),                                  // SW_PROFILE_INSTRUCTIONS_DEFAULT_MARGIN_UNITS
	swProfileEntry("InstructionsDefaults", "ShowGrid", 1),                                     // SW_PROFILE_INSTRUCTIONS_DEFAULT_SHOW_GRID
	swProfileEntry("InstructionsDefaults", "SnapToGrid", 1),                                   // SW_PROFILE_INSTRUCTIONS_DEFAULT_SNAP_TO_GRID
	swProfileEntry("InstructionsDefaults", "GridSize", 72.0f),                                 // SW_PROFILE_INSTRUCTIONS_DEFAULT_GRID_SIZE
	swProfileEntry("InstructionsDefaults", "GridWithinMargins", 1),                            // SW_PROFILE_INSTRUCTIONS_DEFAULT_GRID_WITHIN_MARGINS
	swProfileEntry("InstructionsDefaults", "GridDivisions", 8),                                // SW_PROFILE_INSTRUCTIONS_DEFAULT_GRID_DIVISIONS
	swProfileEntry("Settings", "PreviewSquareViewport", 0),                                    // SW_PROFILE_PREVIEW_SQUARE_VIEWPORT
	swProfileEntry("Settings", "PreviewKeepSelection", 0),                                     // SW_PROFILE_PREVIEW_KEEP_SELECTION
	swProfileEntry("Settings", "MinifigSyncRendering", 1),                                     // SW_PROFILE_MINIFIGURE_BUILDER_SYNCHRONIZED_RENDERING
	swProfileEntry("Settings", "MinifigShadingMode", static_cast<int>(swShadingMode::DefaultLights)), // SW_PROFILE_MINIFIGURE_BUILDER_SHADING_MODE
	swProfileEntry("Settings", "MinifigDrawEdgeLines", 1),                                     // SW_PROFILE_MINIFIGURE_BUILDER_DRAW_EDGE_LINES
	swProfileEntry("Settings", "MinifigDrawConditionalLines", 1),                              // SW_PROFILE_MINIFIGURE_BUILDER_DRAW_CONDITIONAL_LINES
	swProfileEntry("Settings", "MinifigLineWidth", 1.0f),                                      // SW_PROFILE_MINIFIGURE_BUILDER_LINE_WIDTH
	swProfileEntry("Settings", "PartsSnapToTiles", 1),                                        // SW_PROFILE_PARTS_SNAP_TO_TILES
	swProfileEntry("Settings", "RebrickableApiKey", "")                                        // SW_PROFILE_REBRICKABLE_API_KEY
};

void swProfileInitialize()
{
	gProfileEntries[SW_PROFILE_PROJECTS_PATH].mDefault = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
}

void swRemoveProfileKey(SW_PROFILE_KEY Key)
{
	swProfileEntry& Entry = gProfileEntries[Key];
	QSettings Settings;

	Settings.remove(QString("%1/%2").arg(Entry.mSection, Entry.mKey));
}

// use to get the value a key has before it is ever changed
int swGetProfileDefaultInteger(SW_PROFILE_KEY Key)
{
	const int* DefaultInt = std::get_if<int>(&gProfileEntries[Key].mDefault);

	return DefaultInt ? *DefaultInt : 0;
}

float swGetProfileDefaultFloat(SW_PROFILE_KEY Key)
{
	const float* DefaultFloat = std::get_if<float>(&gProfileEntries[Key].mDefault);

	return DefaultFloat ? *DefaultFloat : 0.0f;
}

uint swGetProfileDefaultUnsignedInteger(SW_PROFILE_KEY Key)
{
	const uint* DefaultUInt = std::get_if<uint>(&gProfileEntries[Key].mDefault);

	return DefaultUInt ? *DefaultUInt : 0;
}

int swGetProfileInteger(SW_PROFILE_KEY Key)
{
	swProfileEntry& Entry = gProfileEntries[Key];
	int* DefaultInt = std::get_if<int>(&Entry.mDefault);
	int DefaultValue = DefaultInt ? *DefaultInt : 0;

	const QVariant Stored = QSettings().value(QString("%1/%2").arg(Entry.mSection, Entry.mKey), DefaultValue);

	if (Stored.userType() == QMetaType::Float || Stored.userType() == QMetaType::Double)
		return DefaultValue;
	if (Stored.userType() == QMetaType::ULongLong && Stored.toULongLong() > static_cast<qulonglong>(std::numeric_limits<int>::max()))
		return DefaultValue;

	bool Valid = false;
	const qlonglong Value = Stored.toLongLong(&Valid);

	if (Valid && Value >= std::numeric_limits<int>::min() && Value <= std::numeric_limits<int>::max())
		return static_cast<int>(Value);

	return DefaultValue;
}

uint swGetProfileUnsignedInteger(SW_PROFILE_KEY Key)
{
	swProfileEntry& Entry = gProfileEntries[Key];
	uint* DefaultUInt = std::get_if<uint>(&Entry.mDefault);
	uint DefaultValue = DefaultUInt ? *DefaultUInt : 0;

	const QVariant Stored = QSettings().value(QString("%1/%2").arg(Entry.mSection, Entry.mKey), DefaultValue);

	if (Stored.userType() == QMetaType::Float || Stored.userType() == QMetaType::Double)
		return DefaultValue;

	if (Stored.userType() == QMetaType::Int || Stored.userType() == QMetaType::LongLong)
	{
		const qlonglong Value = Stored.toLongLong();

		if (Value >= std::numeric_limits<qint32>::min() && Value <= static_cast<qlonglong>(std::numeric_limits<uint>::max()))
			return static_cast<uint>(Value);
		return DefaultValue;
	}

	bool Valid = false;
	const qulonglong Value = Stored.toULongLong(&Valid);

	if (Valid && Value <= std::numeric_limits<uint>::max())
		return static_cast<uint>(Value);

	return DefaultValue;
}

float swGetProfileFloat(SW_PROFILE_KEY Key)
{
	swProfileEntry& Entry = gProfileEntries[Key];
	float* DefaultFloat = std::get_if<float>(&Entry.mDefault);
	float DefaultValue = DefaultFloat ? *DefaultFloat : 0.0f;

	bool Valid = false;
	const float Value = QSettings().value(QString("%1/%2").arg(Entry.mSection, Entry.mKey), DefaultValue).toFloat(&Valid);

	return Valid && std::isfinite(Value) ? Value : DefaultValue;
}

QString swGetProfileString(SW_PROFILE_KEY Key)
{
	swProfileEntry& Entry = gProfileEntries[Key];
	QString* DefaultString = std::get_if<QString>(&Entry.mDefault);
	QString DefaultValue = DefaultString ? *DefaultString : QString();

	return QSettings().value(QString("%1/%2").arg(Entry.mSection, Entry.mKey), DefaultValue).toString();
}

QStringList swGetProfileStringList(SW_PROFILE_KEY Key)
{
	swProfileEntry& Entry = gProfileEntries[Key];
	QStringList* DefaultStringList = std::get_if<QStringList>(&Entry.mDefault);
	QStringList DefaultValue = DefaultStringList ? *DefaultStringList : QStringList();

	return QSettings().value(QString("%1/%2").arg(Entry.mSection, Entry.mKey), DefaultValue).toStringList();
}

QByteArray swGetProfileBuffer(SW_PROFILE_KEY Key)
{
	swProfileEntry& Entry = gProfileEntries[Key];
	QSettings Settings;

	return Settings.value(QString("%1/%2").arg(Entry.mSection, Entry.mKey)).toByteArray();
}

void swSetProfileInteger(SW_PROFILE_KEY Key, int Value)
{
	swProfileEntry& Entry = gProfileEntries[Key];
	QSettings Settings;

	Settings.setValue(QString("%1/%2").arg(Entry.mSection, Entry.mKey), Value);
}

void swSetProfileUnsignedInteger(SW_PROFILE_KEY Key, uint Value)
{
	swProfileEntry& Entry = gProfileEntries[Key];
	QSettings Settings;

	Settings.setValue(QString("%1/%2").arg(Entry.mSection, Entry.mKey), Value);
}

void swSetProfileFloat(SW_PROFILE_KEY Key, float Value)
{
	if (!std::isfinite(Value))
		return;

	swProfileEntry& Entry = gProfileEntries[Key];
	QSettings Settings;

	Settings.setValue(QString("%1/%2").arg(Entry.mSection, Entry.mKey), Value);
}

void swSetProfileString(SW_PROFILE_KEY Key, const QString& Value)
{
	swProfileEntry& Entry = gProfileEntries[Key];
	QSettings Settings;

	Settings.setValue(QString("%1/%2").arg(Entry.mSection, Entry.mKey), Value);
}

void swSetProfileStringList(SW_PROFILE_KEY Key, const QStringList& Value)
{
	swProfileEntry& Entry = gProfileEntries[Key];
	QSettings Settings;

	Settings.setValue(QString("%1/%2").arg(Entry.mSection, Entry.mKey), Value);
}

void swSetProfileBuffer(SW_PROFILE_KEY Key, const QByteArray& Buffer)
{
	swProfileEntry& Entry = gProfileEntries[Key];
	QSettings Settings;

	Settings.setValue(QString("%1/%2").arg(Entry.mSection, Entry.mKey), Buffer);
}

template<typename Enum>
struct swProfileEnumerationValue
{
	Enum Value;
	const char* Token;
	int LegacyValue;
};

// Tokens and legacy numbers remain stable when enum members are reordered.
constexpr swProfileEnumerationValue<swShadingMode> ShadingModeValues[] =
{
	{ swShadingMode::Wireframe, "Wireframe", 0 },
	{ swShadingMode::Flat, "Flat", 1 },
	{ swShadingMode::DefaultLights, "DefaultLights", 2 },
	{ swShadingMode::Full, "Full", 3 }
};

constexpr swProfileEnumerationValue<swLightingStyle> LightingStyleValues[] =
{
	{ swLightingStyle::Standard, "Standard", 0 },
	{ swLightingStyle::Studio, "Studio", 1 },
	{ swLightingStyle::Matte, "Matte", 2 },
	{ swLightingStyle::Toon, "Toon", 3 }
};

constexpr swProfileEnumerationValue<swHighlightRenderMode> HighlightRenderModeValues[] =
{
	{ swHighlightRenderMode::Outline, "Outline", 0 },
	{ swHighlightRenderMode::Tint, "Tint", 1 }
};

constexpr swProfileEnumerationValue<swAxisIconLocation> AxisIconLocationValues[] =
{
	{ swAxisIconLocation::TopLeft, "TopLeft", 0 },
	{ swAxisIconLocation::TopRight, "TopRight", 1 },
	{ swAxisIconLocation::BottomLeft, "BottomLeft", 2 },
	{ swAxisIconLocation::BottomRight, "BottomRight", 3 }
};

constexpr swProfileEnumerationValue<swViewSphereLocation> ViewSphereLocationValues[] =
{
	{ swViewSphereLocation::TopLeft, "TopLeft", 0 },
	{ swViewSphereLocation::TopRight, "TopRight", 1 },
	{ swViewSphereLocation::BottomLeft, "BottomLeft", 2 },
	{ swViewSphereLocation::BottomRight, "BottomRight", 3 }
};

constexpr swProfileEnumerationValue<swColorTheme> ColorThemeValues[] =
{
	{ swColorTheme::Dark, "Dark", 0 },
	{ swColorTheme::System, "System", 1 }
};

constexpr swProfileEnumerationValue<swStudStyle> StudStyleValues[] =
{
	{ swStudStyle::Plain, "Plain", 0 },
	{ swStudStyle::ThinLinesLogo, "ThinLinesLogo", 1 },
	{ swStudStyle::OutlineLogo, "OutlineLogo", 2 },
	{ swStudStyle::SharpTopLogo, "SharpTopLogo", 3 },
	{ swStudStyle::RoundedTopLogo, "RoundedTopLogo", 4 },
	{ swStudStyle::FlattenedLogo, "FlattenedLogo", 5 },
	{ swStudStyle::HighContrast, "HighContrast", 6 },
	{ swStudStyle::HighContrastLogo, "HighContrastLogo", 7 }
};

constexpr swProfileEnumerationValue<swPartFilterType> PartFilterTypeValues[] =
{
	{ swPartFilterType::Default, "Default", 0 },
	{ swPartFilterType::RegularExpression, "RegularExpression", 1 }
};

constexpr swProfileEnumerationValue<swPOVRayRenderQuality> RenderQualityValues[] =
{
	{ swPOVRayRenderQuality::Low, "Low", 0 },
	{ swPOVRayRenderQuality::Medium, "Medium", 1 },
	{ swPOVRayRenderQuality::High, "High", 2 }
};

template<typename Enum, size_t N>
static Enum swReadProfileEnumeration(SW_PROFILE_KEY Key, const swProfileEnumerationValue<Enum> (&Values)[N])
{
	const swProfileEntry& Entry = gProfileEntries[Key];
	const Enum Default = static_cast<Enum>(std::get<int>(Entry.mDefault));
	const QString Token = QSettings().value(QString("%1/%2").arg(Entry.mSection, Entry.mKey)).toString();

	for (const swProfileEnumerationValue<Enum>& Value : Values)
	{
		if (Token == QLatin1String(Value.Token))
			return Value.Value;
	}

	bool Valid = false;
	const int LegacyValue = Token.toInt(&Valid);

	if (Valid)
	{
		for (const swProfileEnumerationValue<Enum>& Value : Values)
		{
			if (LegacyValue == Value.LegacyValue)
				return Value.Value;
		}
	}

	return Default;
}

template<typename Enum, size_t N>
static void swWriteProfileEnumeration(SW_PROFILE_KEY Key, Enum Value, const swProfileEnumerationValue<Enum> (&Values)[N])
{
	for (const swProfileEnumerationValue<Enum>& Entry : Values)
	{
		if (Value == Entry.Value)
		{
			swSetProfileString(Key, QString::fromLatin1(Entry.Token));
			return;
		}
	}
}

swShadingMode swGetProfileShadingMode(SW_PROFILE_KEY Key)
{
	return swReadProfileEnumeration(Key, ShadingModeValues);
}

void swSetProfileShadingMode(swShadingMode Value, SW_PROFILE_KEY Key)
{
	swWriteProfileEnumeration(Key, Value, ShadingModeValues);
}

swLightingStyle swGetProfileLightingStyle()
{
	return swReadProfileEnumeration(SW_PROFILE_LIGHTING_STYLE, LightingStyleValues);
}

void swSetProfileLightingStyle(swLightingStyle Value)
{
	swWriteProfileEnumeration(SW_PROFILE_LIGHTING_STYLE, Value, LightingStyleValues);
}

swHighlightRenderMode swGetProfileHighlightRenderMode()
{
	return swReadProfileEnumeration(SW_PROFILE_HIGHLIGHT_RENDER_MODE, HighlightRenderModeValues);
}

void swSetProfileHighlightRenderMode(swHighlightRenderMode Value)
{
	swWriteProfileEnumeration(SW_PROFILE_HIGHLIGHT_RENDER_MODE, Value, HighlightRenderModeValues);
}

swAxisIconLocation swGetProfileAxisIconLocation()
{
	return swReadProfileEnumeration(SW_PROFILE_DRAW_AXES_LOCATION, AxisIconLocationValues);
}

void swSetProfileAxisIconLocation(swAxisIconLocation Value)
{
	swWriteProfileEnumeration(SW_PROFILE_DRAW_AXES_LOCATION, Value, AxisIconLocationValues);
}

swViewSphereLocation swGetProfileViewSphereLocation()
{
	return swReadProfileEnumeration(SW_PROFILE_VIEW_SPHERE_LOCATION, ViewSphereLocationValues);
}

void swSetProfileViewSphereLocation(swViewSphereLocation Value)
{
	swWriteProfileEnumeration(SW_PROFILE_VIEW_SPHERE_LOCATION, Value, ViewSphereLocationValues);
}

swColorTheme swGetProfileColorTheme()
{
	return swReadProfileEnumeration(SW_PROFILE_COLOR_THEME, ColorThemeValues);
}

void swSetProfileColorTheme(swColorTheme Value)
{
	swWriteProfileEnumeration(SW_PROFILE_COLOR_THEME, Value, ColorThemeValues);
}

swStudStyle swGetProfileStudStyle()
{
	return swReadProfileEnumeration(SW_PROFILE_STUD_STYLE, StudStyleValues);
}

void swSetProfileStudStyle(swStudStyle Value)
{
	swWriteProfileEnumeration(SW_PROFILE_STUD_STYLE, Value, StudStyleValues);
}

swPartFilterType swGetProfilePartFilterType()
{
	return swReadProfileEnumeration(SW_PROFILE_PARTS_LIST_PART_FILTER, PartFilterTypeValues);
}

void swSetProfilePartFilterType(swPartFilterType Value)
{
	swWriteProfileEnumeration(SW_PROFILE_PARTS_LIST_PART_FILTER, Value, PartFilterTypeValues);
}

swPOVRayRenderQuality swGetProfileRenderQuality()
{
	return swReadProfileEnumeration(SW_PROFILE_RENDER_QUALITY, RenderQualityValues);
}

void swSetProfileRenderQuality(swPOVRayRenderQuality Value)
{
	swWriteProfileEnumeration(SW_PROFILE_RENDER_QUALITY, Value, RenderQualityValues);
}

swViewSphereLocation swGetProfilePreviewViewSphereLocation()
{
	return swReadProfileEnumeration(SW_PROFILE_PREVIEW_VIEW_SPHERE_LOCATION, ViewSphereLocationValues);
}

void swSetProfilePreviewViewSphereLocation(swViewSphereLocation Value)
{
	swWriteProfileEnumeration(SW_PROFILE_PREVIEW_VIEW_SPHERE_LOCATION, Value, ViewSphereLocationValues);
}
