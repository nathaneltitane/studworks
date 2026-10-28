#pragma once

#include "sw_math.h"

class Project;
class swPartsLibrary;
enum class swViewSphereLocation;

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
	quint32 mTextColor;
	quint32 mTextHaloColor;
	quint32 mMarqueeBorderColor;
	quint32 mMarqueeFillColor;
	quint32 mOverlayColor;
	quint32 mCutawayShadeColor;
	int mCutawayShadeTransparency;
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
	bool mDrawGridHeightXZ;
	bool mDrawGridHeightYZ;
	bool mGridHeightBehindModel;
	bool mDrawGridAxisX;
	bool mDrawGridAxisY;
	bool mDrawGridAxisZ;
	bool mFixedAxes;
	bool mViewSphereEnabled;
	swViewSphereLocation mViewSphereLocation;
	int mViewSphereSize;
	quint32 mViewSphereColor;
	quint32 mViewSphereTextColor;
	quint32 mViewSphereHighlightColor;
	quint32 mAxisXColor;
	quint32 mAxisYColor;
	quint32 mAxisZColor;
	bool mAutoLoadMostRecent;
	bool mRestoreTabLayout;
	bool mFirstPartAtOrigin;
	bool mPartsSnapToTiles;
	bool mTerminalFollowModel;
	swInsertColorMode mInsertColorMode;
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


	int mPreviewViewSphereEnabled;
	int mPreviewViewSphereSize;
	swViewSphereLocation mPreviewViewSphereLocation;
	int mDrawPreviewAxis;
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
	bool SetCameraAngles = false;
	bool SetCameraPosition = false;
	bool Orthographic = false;
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
	QString SettingsImportName;
	QString SettingsExportName;
	QString KeyboardShortcutsImportName;
	QString KeyboardShortcutsExportName;
	QString MouseShortcutsImportName;
	QString MouseShortcutsExportName;
	QString CategoriesImportName;
	QString CategoriesExportName;
	QList<QPair<QString, bool>> LibraryPaths;
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

	void SetClipboard(const QByteArray& Clipboard);
	void ExportClipboard(const QByteArray& Clipboard);

	Project* mProject = nullptr;
	swPartsLibrary* mLibrary = nullptr;
	swPreferences mPreferences;
	QByteArray mClipboard;

signals:
	void RendererInitialized();

protected:
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
