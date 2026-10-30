#pragma once

#include "sw_math.h"
#include <array>

class Project;
class swPartsLibrary;
enum class swViewCubeLocation;

enum class swShadingMode
{
	Wireframe,
	Flat,
	DefaultLights,
	Full
};

enum class swLightingStyle
{
	Standard,
	Studio,
	Matte,
	Toon
};

enum class swColorTheme
{
	Dark,
	System
};

// The outline shader samples at most 8 pixels around each pixel.
#define SW_SELECTION_OUTLINE_WIDTH_MINIMUM 1
#define SW_SELECTION_OUTLINE_WIDTH_MAXIMUM 8

// sizes from extra small to extra large on a 16px baseline
constexpr std::array<int, 5> SW_VIEW_CUBE_SIZES = { 48, 64, 96, 128, 192 };
constexpr std::array<int, 5> SW_AXIS_ICON_SIZES = SW_VIEW_CUBE_SIZES;
// the view cube and axis icon share a corner box whose half spans the boundary and the cube faces sit at the half size
constexpr float SW_VIEW_CUBE_BOUNDARY = 1.25f;
constexpr float SW_VIEW_CUBE_HALF_SIZE = 0.8f;
constexpr float SW_VIEW_CUBE_CHAMFER = 0.22f;
// the alpha settings read 0 to 1 over a base range of 95% to 85% transparency so an overlay never disappears
constexpr float SW_OVERLAY_ALPHA_MIN = 0.05f;
constexpr float SW_OVERLAY_ALPHA_MAX = 0.15f;
constexpr std::array<int, 5> SW_PART_ICON_SIZES = { 32, 48, 64, 96, 128 };

// use to snap a stored size to the nearest listed size
inline int swSnapSize(int Size, const std::array<int, 5>& Sizes)
{
	int Nearest = Sizes[0];

	for (int Listed : Sizes)
		if (std::abs(Listed - Size) < std::abs(Nearest - Size))
			Nearest = Listed;

	return Nearest;
}

// use to find the combo index of a size, after the disabled entry
inline int swSizeIndex(int Size, const std::array<int, 5>& Sizes)
{
	for (size_t Index = 0; Index < Sizes.size(); Index++)
		if (Sizes[Index] == swSnapSize(Size, Sizes))
			return static_cast<int>(Index) + 1;

	return 0;
}

// use to turn a 0 to 100 alpha setting into the overlay alpha
inline float swOverlayAlpha(int Alpha)
{
	return SW_OVERLAY_ALPHA_MIN + (SW_OVERLAY_ALPHA_MAX - SW_OVERLAY_ALPHA_MIN) * (Alpha < 0 ? 0 : Alpha > 100 ? 100 : Alpha) / 100.0f;
}

enum class swHighlightRenderMode
{
	Outline,
	Tint
};

enum class swInsertColorMode
{
	CurrentColor,
	SelectedPartColor,
	LastInsertedColor
};

enum class swFlexSpreadMode
{
	Normal,
	DistributedWeight
};

enum class swAxisIconLocation
{
	TopLeft,
	TopRight,
	BottomLeft,
	BottomRight
};

// the axis the idle turntable turns the model around
enum class swTurntableAxis
{
	X,
	Y,
	Z
};

// the views a projection change applies to
enum class swProjectionScope
{
	PerView,
	AllViews
};

struct swRenderSettings
{
	swShadingMode ShadingMode;
	bool DrawEdgeLines;
	bool DrawConditionalLines;
	float LineWidth;
};

enum class swRenderTarget
{
	ModelView,
	Preview,
	Parts,
	MinifigureBuilder
};

struct swTargetRenderSettings
{
	bool SyncRendering;
	swRenderSettings Settings;
};

class swPreferences
{
public:
	void LoadDefaults();
	void SaveDefaults();
	void SetInterfaceColors(swColorTheme ColorTheme);

	void LoadRenderTarget(swRenderTarget Target);
	void SaveRenderTarget(swRenderTarget Target) const;

	// The preview and the parts list use their own settings unless they are synced with the model views.
	swRenderSettings GetRenderSettings(swRenderTarget Target) const
	{
		if (Target != swRenderTarget::ModelView)
		{
			const swTargetRenderSettings& TargetSettings = GetTargetRenderSettings(Target);

			if (!TargetSettings.SyncRendering)
				return TargetSettings.Settings;
		}

		return { mShadingMode, mDrawEdgeLines, mDrawConditionalLines, mLineWidth };
	}

	swTargetRenderSettings& GetTargetRenderSettings(swRenderTarget Target)
	{
		return Target == swRenderTarget::Parts ? mPartsRendering : Target == swRenderTarget::MinifigureBuilder ? mMinifigureBuilderRendering : mPreviewRendering;
	}

	const swTargetRenderSettings& GetTargetRenderSettings(swRenderTarget Target) const
	{
		return Target == swRenderTarget::Parts ? mPartsRendering : Target == swRenderTarget::MinifigureBuilder ? mMinifigureBuilderRendering : mPreviewRendering;
	}

	int mMouseSensitivity;
	swShadingMode mShadingMode;
	swLightingStyle mLightingStyle;
	swHighlightRenderMode mHighlightRenderMode;
	int mSelectionOutlineWidth;
	quint32 mBackgroundSolidColor;
	bool mDrawAxes;
	bool mFollowStepViews;
	swProjectionScope mProjectionScope;
	int mCutawaySize;
	int mTurntableIdleDuration;
	int mTurntableTurnTime;
	swTurntableAxis mTurntableAxis;
	swAxisIconLocation mAxisIconLocation;
	int mAxisIconSize;
	bool mAxisIconLabels;
	bool mAxisIconNegative;
	bool mAxisIconCubeOutline;
	int mAxisIconCubeOutlineAlpha;
	quint32 mTextColor;
	quint32 mTextHaloColor;
	quint32 mMarqueeBorderColor;
	quint32 mMarqueeFillColor;
	quint32 mOverlayColor;
	quint32 mCutawayShadeColor;
	int mCutawayShadeAlpha;
	quint32 mActiveViewColor;
	quint32 mInactiveViewColor;
	bool mDrawEdgeLines;
	bool mDrawConditionalLines;
	float mLineWidth;
	bool mAllowLOD;
	swTargetRenderSettings mPreviewRendering;
	swTargetRenderSettings mPartsRendering;
	swTargetRenderSettings mMinifigureBuilderRendering;
	float mMeshLODDistance;
	bool mFadeSteps;
	quint32 mFadeStepsColor;
	bool mHighlightNewParts;
	quint32 mHighlightNewPartsColor;
	bool mGridEnabled = true;
	bool mDrawGridStuds;
	quint32 mGridStudColor;
	bool mDrawGridLines;
	int mGridLineSpacing;
	quint32 mGridLineColor;
	bool mDrawGridOrigin;
	bool mDrawGridNegativeAxes;
	bool mDrawGridHeightXZ;
	bool mDrawGridHeightYZ;
	bool mGridHeightBehindModel;
	bool mDrawGridAxisX;
	bool mDrawGridAxisY;
	bool mDrawGridAxisZ;
	bool mFixedAxes;
	bool mViewCubeEnabled;
	swViewCubeLocation mViewCubeLocation;
	int mViewCubeSize;
	quint32 mViewCubeColor;
	quint32 mViewCubeTextColor;
	quint32 mViewCubeHighlightColor;
	quint32 mAxisXColor;
	quint32 mAxisYColor;
	quint32 mAxisZColor;
	quint32 mAxisXNegativeColor;
	quint32 mAxisYNegativeColor;
	quint32 mAxisZNegativeColor;
	bool mAutoLoadMostRecent;
	bool mRestoreTabLayout;
	bool mFirstPartAtOrigin;
	bool mShadowLibraryEnabled;
	bool mPartsSnapToTiles;
	bool mTerminalFollowModel;
	swInsertColorMode mInsertColorMode;
	swFlexSpreadMode mFlexSpreadMode;
	bool mFlexClampNewControlPoints;
	bool mFlexMiddleControlPoint;
	bool mWatchFileChanges;
	bool mLoadPartsBeforeShowing;
	bool mMigrateMetaCommands;
	swColorTheme mColorTheme;
	quint32 mObjectSelectedColor;
	quint32 mObjectFocusedColor;
	quint32 mCameraColor;
	quint32 mLightColor;
	quint32 mControlPointColor;
	quint32 mControlPointFocusedColor;
	quint32 mControlPointSeatedColor;
	quint32 mPathColor;
	quint32 mPathOutOfReachColor;
	quint32 mSpreadHandleColor;


	int mPreviewViewCubeEnabled;
	int mPreviewViewCubeSize;
	swViewCubeLocation mPreviewViewCubeLocation;
	int mDrawPreviewAxis;
	int mPreviewAxisIconSize;
	bool mPreviewSquareViewport;
	bool mPreviewKeepSelection;

	bool mStudCylinderColorEnabled;
	quint32 mStudCylinderColor;
	bool mPartEdgeColorEnabled;
	quint32 mPartEdgeColor;
	bool mBlackEdgeColorEnabled;
	quint32 mBlackEdgeColor;
	bool mDarkEdgeColorEnabled;
	quint32 mDarkEdgeColor;
	float mPartEdgeContrast;
	float mPartColorValueLDIndex;
	bool  mAutomateEdgeColor;
};

struct swCommandLineOptions
{
	bool ParseOK = true;
	bool Exit = false;
	bool SaveImage = false;
	bool SaveWavefront = false;
	bool Save3DS = false;
	bool SaveCOLLADA = false;
	bool SaveCSV = false;
	bool SaveHTML = false;
	bool SavePOVRay = false;
	bool RenderPOVRay = false;
	bool SetCameraAngles = false;
	bool SetCameraPosition = false;
	bool Orthographic = false;
	bool TransparentImage = false;
	bool SetFoV = false;
	bool SetZPlanes = false;
	bool SetFadeStepsColor = false;
	bool SetHighlightColor = false;
	bool FadeSteps = false;
	bool ImageHighlight = false;
	bool AutomateEdgeColor = false;
	int ImageWidth;
	int ImageHeight;
	int AASamples;
	swShadingMode ShadingMode;
	swLightingStyle LightingStyle;
	float LineWidth;
	swStudStyle StudStyle;
	swStep ImageStart;
	swStep ImageEnd;
	swVector3 CameraPosition[3];
	swVector2 CameraLatLon;
	float FoV;
	float PartEdgeContrast;
	float PartColorValueLDIndex;
	swVector2 ZPlanes;
	swViewpoint Viewpoint;
	bool StudCylinderColorEnabled;
	quint32 StudCylinderColor;
	bool PartEdgeColorEnabled;
	quint32 PartEdgeColor;
	bool BlackEdgeColorEnabled;
	quint32 BlackEdgeColor;
	bool DarkEdgeColorEnabled;
	quint32 DarkEdgeColor;
	quint32 FadeStepsColor;
	quint32	HighlightColor;
	QString ImageName;
	QString ModelName;
	QString CameraName;
	QString ProjectName;
	QString SaveWavefrontName;
	QString Save3DSName;
	QString SaveCOLLADAName;
	QString SaveCSVName;
	QString SaveHTMLName;
	QString SavePOVRayName;
	QString RenderPOVRayName;
	QString SettingsImportName;
	QString SettingsExportName;
	QString KeyboardShortcutsImportName;
	QString KeyboardShortcutsExportName;
	QString MouseShortcutsImportName;
	QString MouseShortcutsExportName;
	QString CategoriesImportName;
	QString CategoriesExportName;
	QList<QPair<QString, bool>> LibraryPaths;
	QString UnofficialLibraryPath;
	QString ShadowLibraryPath;
	QString StdOut;
	QString StdErr;

	// use to tell if the options only transfer settings to and from files
	bool HasSettingsTransfer() const
	{
		return !SettingsImportName.isEmpty() || !SettingsExportName.isEmpty() || !KeyboardShortcutsImportName.isEmpty() || !KeyboardShortcutsExportName.isEmpty() || !MouseShortcutsImportName.isEmpty() || !MouseShortcutsExportName.isEmpty() || !CategoriesImportName.isEmpty() || !CategoriesExportName.isEmpty();
	}
};

enum class swStartupMode
{
	ShowWindow,
	Success,
	Error
};

class swApplication : public QApplication
{
	Q_OBJECT

public:
	bool IsDarkTheme() const;
	bool IsSystemPaletteDark() const
	{
		return mDefaultPalette.color(QPalette::Window).lightness() < 128;
	}
	swApplication(int& Argc, char** Argv);
	~swApplication();

	swApplication(const swApplication&) = delete;
	swApplication(swApplication&&) = delete;
	swApplication& operator=(const swApplication&) = delete;
	swApplication& operator=(swApplication&&) = delete;

	void SetProject(Project* Project);
	static swCommandLineOptions ParseCommandLineOptions();
	swStartupMode Initialize(const QList<QPair<QString, bool>>& LibraryPaths);
	void Shutdown();
	void ShowPreferencesDialog();
	void SaveTabLayout() const;

	bool LoadPartsLibrary(const QList<QPair<QString, bool>>& LibraryPaths, bool OnlyUsePaths, swStudStyle StudStyle, bool StudCylinderColorEnabled);
	bool ReloadPartsLibrary(const QString& LibraryPath, bool OfficialLibrary);
	static QString GetDownloadLibraryPath();

	void SetClipboard(const QByteArray& Clipboard);
	void ExportClipboard(const QByteArray& Clipboard);

	Project* mProject = nullptr;
	swPartsLibrary* mLibrary = nullptr;
	swPreferences mPreferences;
	QByteArray mClipboard;

signals:
	void RendererInitialized();

protected:
	bool SetupPartsLibrary();
	bool DownloadPartsLibrary(const QString& FileName);
	void UpdateStyle();
	QString GetTabLayoutKey() const;

	QString mDefaultStyle;
	QPalette mDefaultPalette;
	bool mCustomStyleApplied = false;
};

extern swApplication* gApplication;

inline swPartsLibrary* swGetPartsLibrary()
{
	return gApplication->mLibrary;
}

inline Project* swGetActiveProject()
{
	return gApplication->mProject;
}

inline swPreferences& swGetPreferences()
{
	return gApplication->mPreferences;
}
