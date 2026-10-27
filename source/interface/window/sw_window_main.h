#pragma once

#include "sw_application.h"
#include "sw_types_asset.h"
#include "sw_shortcuts.h"
#include "sw_result.h"
#include "sw_commands.h"
#include "sw_model.h"

class swPartSelectionWidget;
class swInstructionsDialog;
class swPreviewDockWidget;
class PiecePreview;
class swQPartsTree;
class swColorList;
class swPropertiesWidget;
class swTimelineWidget;
class swElidedLabel;
class swDoubleSpinBox;
class swSpinnerWidget;
enum class swRenderDialogMode;

class swModelTabWidget : public QWidget
{
	Q_OBJECT

public:
	swModelTabWidget(swModel* Model)
	{
		mModel = Model;
		mActiveView = nullptr;
	}

	QWidget* GetAnyViewWidget()
	{
		QWidget* Widget = layout()->itemAt(0)->widget();

		while (Widget->metaObject() == &QSplitter::staticMetaObject)
			Widget = ((QSplitter*)Widget)->widget(0);

		return Widget;
	}

	swView* GetActiveView() const
	{
		return mActiveView;
	}

	void SetActiveView(swView* ActiveView)
	{
		mActiveView = ActiveView;
	}

	void RemoveView(const swView* View)
	{
		if (View == mActiveView)
			mActiveView = nullptr;
	}

	swModel* GetModel() const
	{
		return mModel;
	}

protected:
	swModel* mModel;
	swView* mActiveView;
};

class swMainWindow : public QMainWindow
{
	Q_OBJECT

public:
	swMainWindow();
	~swMainWindow();

	void CreateWidgets();
	void ScheduleAssetUpdate();

	swTool GetTool() const
	{
		return mTool;
	}

	swTransformType GetTransformType() const
	{
		return mTransformType;
	}

	bool GetAddKeys() const
	{
		return mAddKeys;
	}

	float GetMoveXYSnap() const
	{
		const float SnapXYTable[] = { 0.0f, 1.0f, 5.0f, 8.0f, 10.0f, 20.0f, 40.0f, 60.0f, 80.0f, 160.0f };
		return mMoveSnapEnabled ? SnapXYTable[mMoveXYSnapIndex] : 0.0f;
	}

	float GetMoveZSnap() const
	{
		const float SnapZTable[] = { 0.0f, 1.0f, 5.0f, 8.0f, 10.0f, 20.0f, 24.0f, 48.0f, 96.0f, 192.0f };
		return mMoveSnapEnabled ? SnapZTable[mMoveZSnapIndex] : 0.0f;
	}

	float GetAngleSnap() const
	{
		return mAngleSnapEnabled ? GetAngleSnapIncrement() : 0.0f;
	}

	float GetAngleSnapIncrement() const
	{
		const float AngleTable[] = { 0.0f, 1.0f, 5.0f, 15.0f, 22.5f, 30.0f, 45.0f, 60.0f, 90.0f, 180.0f };
		return mAngleSnapIndex < static_cast<int>(SW_ARRAY_COUNT(AngleTable)) ? AngleTable[mAngleSnapIndex] : mAngleSnapCustom;
	}

	float GetOrbitSnap() const
	{
		return mOrbitSnapEnabled ? GetAngleSnapIncrement() : 0.0f;
	}

	QString GetMoveXYSnapText() const
	{
		QString SnapXYText[] = { tr("0"), tr("1/20S"), tr("1/4S"), tr("1F"), tr("1/2S"), tr("1S"), tr("2S"), tr("3S"), tr("4S"), tr("8S") };
		return mMoveSnapEnabled ? SnapXYText[mMoveXYSnapIndex] : tr("None");
	}

	QString GetMoveZSnapText() const
	{
		QString SnapZText[] = { tr("0"), tr("1/20S"), tr("1/4S"), tr("1F"), tr("1/2S"), tr("1S"), tr("1B"), tr("2B"), tr("4B"), tr("8B") };
		return mMoveSnapEnabled ? SnapZText[mMoveZSnapIndex] : tr("None");
	}

	QString GetAngleSnapText() const;

	bool GetRelativeTransform() const
	{
		return mRelativeTransform;
	}

	bool GetSeparateTransform() const
	{
		return mLocalTransform;
	}

	swSelectionMode GetSelectionMode() const
	{
		return mSelectionMode;
	}

	const swPieceDataReference& GetInsertPieceData() const
	{
		return mInsertPieceData;
	}

	swView* GetActiveView() const
	{
		const swModelTabWidget* const CurrentTab = mModelTabWidget ? (swModelTabWidget*)mModelTabWidget->currentWidget() : nullptr;
		return CurrentTab ? CurrentTab->GetActiveView() : nullptr;
	}

	swModel* GetActiveModel() const;
	swModelTabWidget* GetTabForView(swView* View) const;

	swModel* GetCurrentTabModel() const
	{
		const swModelTabWidget* const CurrentTab = (swModelTabWidget*)mModelTabWidget->currentWidget();
		return CurrentTab ? CurrentTab->GetModel() : nullptr;
	}

	swPartSelectionWidget* GetPartSelectionWidget() const
	{
		return mPartSelectionWidget;
	}

	swPreviewDockWidget* GetPreviewWidget() const
	{
		return mPreviewWidget;
	}

	QMenu* GetToolsMenu() const
	{
		return mToolsMenu;
	}

	QMenu* GetViewpointMenu() const
	{
		return mViewpointMenu;
	}

	QMenu* GetCameraMenu() const
	{
		return mCameraMenu;
	}

	QMenu* GetProjectionMenu() const
	{
		return mProjectionMenu;
	}

	QMenu* GetShadingMenu() const
	{
		return mShadingMenu;
	}

	QByteArray GetTabLayout();
	void RestoreTabLayout(const QByteArray& TabLayout);
	void RemoveAllModelTabs();
	void CloseCurrentModelTab();
	void SetCurrentModelTab(swModel* Model, bool AdjustFarPlane);
	void AddView(swView* View);
	void RemoveView(swView* View);

	void SetTool(swTool Tool);
	void SetTransformType(swTransformType TransformType);
	void SetColorIndex(int ColorIndex);
	void ToggleFirstPieceAtOrigin();
	int GetInsertColorIndex() const;
	void SetLastInsertedColorIndex(int ColorIndex);
	void SetInsertColorMode(swInsertColorMode InsertColorMode);
	void UpdatePiecesColor();
	void ColorOptionsMenuAboutToShow();
	void SetMoveSnapEnabled(bool Enabled);
	void SetAngleSnapEnabled(bool Enabled);
	void SetMoveXYSnapIndex(int Index);
	void SetMoveZSnapIndex(int Index);
	void SetAngleSnapIndex(int Index);
	void SetCustomAngleSnap();
	void SetRelativeTransform(bool RelativeTransform);
	void SetSeparateTransform(bool SelectionTransform);
	void SetShadingMode(swShadingMode ShadingMode);
	void SetLightingStyle(swLightingStyle LightingStyle);
	void SetSelectionMode(swSelectionMode SelectionMode);
	void ToggleViewSphere();
	void ToggleAxisIcon();
	void ToggleGrid();
	void ToggleFadePreviousSteps();
	void ShowStepViewDialog();
	void SetHighlightRenderMode(swHighlightRenderMode HighlightRenderMode);
	void SetSelectionOutlineWidth(int SelectionOutlineWidth);
	void ToggleAutomateEdgeColor();
	void SetStudStyle(swStudStyle StudStyle);
	void ToggleEdgeLines(swRenderTarget Target);
	void ToggleConditionalLines(swRenderTarget Target);
	void ToggleSyncRendering(swRenderTarget Target);
	void SetTargetShadingMode(swRenderTarget Target, swShadingMode ShadingMode);
	void AddRenderTargetActions(QMenu* Menu, swRenderTarget Target);
	QAction* AddMenuCommand(QMenu* Menu, const QString& Text, swCommandId CommandId, bool Checked);
	void TogglePreviewViewSphere();
	void TogglePreviewAxisIcon();
	void UpdatePartsRendering();
	void ToggleLoadPiecesBeforeShowing();

	void NewProject();
	bool OpenProject(const QString& FileName);
	void OpenRecentProject(int RecentFileIndex);
	void ShowMergeDialog();
	void ImportLDD();
	void ImportInventory();
	bool SaveProject(const QString& FileName);
	bool SaveProjectIfModified();
	void ReloadProject();
	void ToggleWatchFileChanges();
	void ReloadProjectFile(const QString& FileName);
	bool SetModelFromFocus();
	void SetModelFromSelection();
	void HandleCommand(swCommandId CommandId);

	void SplitHorizontal();
	void SplitVertical();
	void RemoveActiveView();
	void ResetViews();

	void ToggleFullScreen();

	void UpdateSelectedObjects(bool SelectionChanged);
	void UpdateTimeline(bool Clear, bool UpdateItems);
	void UpdatePaste(bool Enabled);
	void UpdateCurrentStep();
	void SetAddKeys(bool AddKeys);
	void UpdateLockSnap();
	void UpdateSnap();
	void UpdateColor();
	void UpdateUndoRedo(const QString& UndoText, const QString& RedoText);
	void UpdateShadingMode();
	void UpdateSelectionMode();
	void UpdateModels();
	void UpdateInUseCategory();
	void UpdateCategories();
	void UpdateTitle();
	void UpdateModified(bool Modified);
	void UpdateShortcuts();

	std::optional<swVector3> GetTransformAmount();

	bool eventFilter(QObject* Object, QEvent* Event) override;
	void SnapPartsDock();

	int mColorIndex;
	int mLastInsertedColorIndex = -1;
	QAction* mActions[SW_COMMAND_COUNT] = {};

signals:
	void ModelContentsChanged();

public:
	void CloseInstructionsDialog();

public slots:
	void ProjectFileChanged(const QString& Path);
	void ModelViewOptionsMenuAboutToShow();
	void PreviewPiece(swPieceData* PieceData, int ColorCode, bool ShowPreview);
	void TogglePreviewWidget(bool Visible);
	void SetInsertPieceData(swPieceData* PieceData);

protected slots:
	void FileMenuAboutToShow();
	void CameraMenuAboutToShow();
	void ProjectionMenuAboutToShow();
	void ViewFocusReceived();
	void UpdateDockWidgetActions();
	void UpdateGamepads();
	void ModelTabContextMenuRequested(const QPoint& Point);
	void ModelTabCloseOtherTabs();
	void ModelTabClosed(int Index);
	void ModelTabChanged(int Index);
	void ClipboardChanged();
	void ActionTriggered();
	void ColorChanged(int ColorIndex);
	void PartListPicked(swPieceData* PieceData);
	void ColorButtonClicked();
	void EnableWindowFlags(bool);

protected:
	void closeEvent(QCloseEvent *event) override;
	void dragEnterEvent(QDragEnterEvent* Event) override;
	void dropEvent(QDropEvent* Event) override;
	QMenu* createPopupMenu() override;

	void CreateActions();
	void CreateMenus();
	void CreateToolBars();
	void CreateStatusBar();
	void UpdateAssets();
	swView* CreateView(swModel* Model);
	void SetActiveView(swView* ActiveView);
	void ToggleDockWidget(QWidget* DockWidget);
	void SplitView(Qt::Orientation Orientation);
	void ShowAboutDialog();
	void ShowHTMLDialog();
	void ShowExportPOVRayDialog();
	void ShowRenderDialog(swRenderDialogMode RenderDialogMode);
	void ShowInstructionsDialog();
	void ShowPrintWidget();
	void ShowImageDialog();
	void ShowSelectDialog();
	void ShowResultError(const swResult<void>& Result, const QString& Title);
	void CreatePreviewWidget();
	void CreateTerminalWidget();
	void UpdateTerminalDirectory(bool Force);

	bool OpenProjectFile(const QString& FileName);
	void AddRecentFile(const QString& FileName);
	void RemoveRecentFile(size_t FileIndex);

	swModelTabWidget* GetTabWidgetForModel(const swModel* Model) const
	{
		for (int TabIdx = 0; TabIdx < mModelTabWidget->count(); TabIdx++)
		{
			swModelTabWidget* TabWidget = (swModelTabWidget*)mModelTabWidget->widget(TabIdx);

			if (TabWidget->GetModel() == Model)
				return TabWidget;
		}

		return nullptr;
	}

	QTimer mGamepadTimer;
	QDateTime mLastGamepadUpdate;

	bool mAddKeys = false;
	swTool mTool;
	swTransformType mTransformType;
	bool mMoveSnapEnabled = true;
	bool mAngleSnapEnabled = true;
	int mMoveXYSnapIndex;
	int mMoveZSnapIndex;
	int mAngleSnapIndex;
	float mAngleSnapCustom = 7.5f;
	bool mOrbitSnapEnabled = false;
	bool mRelativeTransform = true;
	bool mLocalTransform = false;
	swPieceDataReference mInsertPieceData;
	swSelectionMode mSelectionMode;
	int mMousePressTabIndex = -1;

	QAction* mActionFileRecentSeparator = nullptr;

	QTabWidget* mModelTabWidget = nullptr;
	QToolBar* mStandardToolBar = nullptr;
	QToolBar* mToolsToolBar = nullptr;
	QToolBar* mTimeToolBar = nullptr;
	QDockWidget* mPreviewToolBar = nullptr;
	QDockWidget* mTerminalToolBar = nullptr;
	QDockWidget* mPartsToolBar = nullptr;
	QPointer<swInstructionsDialog> mInstructionsDialog;
	bool mPartsSnapPending = false;
	bool mPartsSnapRetried = false;
	QDockWidget* mColorsToolBar = nullptr;
	QDockWidget* mPropertiesToolBar = nullptr;
	QDockWidget* mTimelineToolBar = nullptr;
	QDockWidget* mMinifigureBuilderToolBar = nullptr;

	swPartSelectionWidget* mPartSelectionWidget = nullptr;
	swColorList* mColorList = nullptr;
	QToolButton* mColorButton = nullptr;
	swPropertiesWidget* mPropertiesWidget = nullptr;
	swTimelineWidget* mTimelineWidget = nullptr;
	swDoubleSpinBox* mTransformXEdit = nullptr;
	swDoubleSpinBox* mTransformYEdit = nullptr;
	swDoubleSpinBox* mTransformZEdit = nullptr;
	swPreviewDockWidget* mPreviewWidget = nullptr;

	swElidedLabel* mStatusBarLabel = nullptr;
	QLabel* mStatusPositionLabel = nullptr;
	QLabel* mStatusSnapLabel = nullptr;
	QLabel* mStatusTimeLabel = nullptr;
	swSpinnerWidget* mStatusLoadSpinner = nullptr;
	QLabel* mStatusLoadFailureLabel = nullptr;
	const Project* mStatusProject = nullptr;
	std::unordered_set<const swPieceData*> mStatusRequiredAssets;
	bool mAssetUpdateScheduled = false;

	QMenu* mTransformMenu = nullptr;
	QMenu* mToolsMenu = nullptr;
	QMenu* mViewpointMenu = nullptr;
	QMenu* mCameraMenu = nullptr;
	QMenu* mProjectionMenu = nullptr;
	QMenu* mShadingMenu = nullptr;
	QMenu* mSelectionModeMenu = nullptr;

	std::array<QString, 4> mRecentFiles;
};

extern class swMainWindow* gMainWindow;
