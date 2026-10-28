#pragma once

#include "sw_types_asset.h"
#include "sw_context.h"
#include "sw_math.h"
#include "sw_commands.h"
#include "sw_result.h"

struct swInsertPartInformation;
enum class swCameraProjection;

enum class swDragState
{
	None,
	Part,
	Color
};

enum class swCursor
{
	First,
	Hidden = First,
	Default,
	Brick,
	PointLight,
	SpotLight,
	DirectionalLight,
	AreaLight,
	Camera,
	Select,
	SelectAdd,
	SelectRemove,
	Move,
	MoveAxisX,
	MoveAxisY,
	MoveAxisZ,
	Rotate,
	RotateAxisX,
	RotateAxisY,
	RotateAxisZ,
	RotateX,
	RotateY,
	Delete,
	Paint,
	ColorPicker,
	Zoom,
	ZoomRegion,
	Pan,
	Roll,
	RotateView,
	Count
};

enum class swTrackButton
{
	None,
	Left,
	Middle,
	Right
};

enum class swTrackTool
{
	None,
	Insert,
	PointLight,
	SpotLight,
	DirectionalLight,
	AreaLight,
	Camera,
	Select,
	MoveX,
	MoveY,
	MoveZ,
	MoveXY,
	MoveXZ,
	MoveYZ,
	MoveXYZ,
	RotateX,
	RotateY,
	RotateZ,
	RotateTrackBall,
	RotateCamera,
	RotateTrainTrackRight,
	RotateTrainTrackLeft,
	InsertTrainTrack,
	SelectTrainTrack,
	ScalePlus,
	ScaleMinus,
	Eraser,
	Paint,
	ColorPicker,
	Zoom,
	Pan,
	OrbitX,
	OrbitY,
	OrbitXY,
	Roll,
	ZoomRegion,
	Count
};

enum class swViewType
{
	View,
	Preview,
	MinifigureBuilder,
	PartsList,
	Turntable,
	Count
};

struct swFindReplaceParameters
{
	swPartData* FindPartData = nullptr;
	QString FindString;
	int FindColorIndex = 0;
	swPartData* ReplacePartData = nullptr;
	int ReplaceColorIndex = 0;
};

class swView : public QObject
{
	Q_OBJECT

public:
	swView(swViewType ViewType, swModel* Model);
	~swView();

	swView(const swView&) = delete;
	swView(swView&&) = delete;
	swView& operator=(const swView&) = delete;
	swView& operator=(swView&&) = delete;

	static swFindReplaceParameters& GetFindReplaceParams()
	{
		return mFindReplaceParams;
	}

	swModel* GetModel() const
	{
		return mModel;
	}

	swViewType GetViewType() const
	{
		return mViewType;
	}

	swCamera* GetCamera() const
	{
		return mCamera;
	}

	bool IsLastFocused() const
	{
		return mLastFocusedView == this;
	}

	bool IsTracking() const
	{
		return mTrackButton != swTrackButton::None;
	}

	int GetWidth() const
	{
		return mWidth;
	}

	int GetHeight() const
	{
		return mHeight;
	}

	void SetSize(int Width, int Height);

	swViewWidget* GetWidget() const
	{
		return mWidget;
	}

	void SetWidget(swViewWidget* Widget)
	{
		mWidget = Widget;
	}

	int GetMouseX() const
	{
		return mMouseX;
	}

	int GetMouseY() const
	{
		return mMouseY;
	}

	int GetMouseDownX() const
	{
		return mMouseDownX;
	}

	int GetMouseDownY() const
	{
		return mMouseDownY;
	}

	float GetCameraRotationStartAngle() const
	{
		return mCameraRotationMouseAngle;
	}

	void SetDrawViewportBorder(bool DrawBorder)
	{
		mDrawViewportBorder = DrawBorder;
	}

	void SetBackgroundColorOverride(quint32 BackgroundColor)
	{
		mOverrideBackgroundColor = true;
		mBackgroundColor = BackgroundColor;
	}

	// use to keep the lines as wide as at the normal size when rendering larger images
	void SetLineWidthScale(float LineWidthScale)
	{
		mLineWidthScale = LineWidthScale;
	}

	swMatrix44 GetActiveSubmodelTransform() const
	{
		return mActiveSubmodelTransform;
	}

	static std::vector<swView*> GetModelViews(const swModel* Model);
	static void UpdateProjectViews(const Project* Project);
	static void UpdateAllViews();
	static void UpdateFittedViews();

	static void CreateResources(swContext* Context);
	static void DestroyResources(swContext* Context);

	void MakeCurrent();
	bool PrepareOutlineFramebuffers();
	void Redraw();

	void SetOffscreenContext();

	void SetFocus(bool Focus);
	void SetMousePosition(int MouseX, int MouseY);
	void SetMouseModifiers(Qt::KeyboardModifiers MouseModifiers);

	swModel* GetActiveModel() const;
	std::vector<swModel*> GetModelEditingPath() const;
	void RestoreModelEditingContext();
	void SetSubmodelActive(size_t Depth);
	void SetTopSubmodelActive();
	void SetSelectedSubmodelActive();

	void OnDraw();
	bool HasMissingAssets() const;
	void OnLeftButtonDown();
	void OnLeftButtonUp();
	void OnLeftButtonDoubleClick();
	void OnMiddleButtonDown();
	void OnMiddleButtonUp();
	void OnRightButtonDown();
	void OnRightButtonUp();
	void OnBackButtonDown();
	void OnBackButtonUp();
	void OnForwardButtonDown();
	void OnForwardButtonUp();
	void OnMouseMove();
	void OnMouseLeave();
	void OnMouseWheel(float Direction);
	void BeginDrag(swDragState DragState);
	void EndDrag(bool Accept);

	void UpdateCursor();
	void StartPanGesture();
	void UpdatePanGesture(int dx, int dy);
	void EndPanGesture(bool Accept);
	void StartOrbitTracking();
	void StopTracking(bool Accept);
	void CancelTrackingOrClearSelectionAction();
	void StartFaceNormalPick();
	void StartCutawayPick(swCutawayShape CutawayShape);
	void ResizeCutaway(float Scale);
	void MoveCutaway(float Distance);
	void RemoveCutaway();

	bool HasCutaway() const
	{
		return mCutawayShape != swCutawayShape::None;
	}

	void SetViewpoint(swViewpoint Viewpoint);
	void SetViewpoint(const swVector3& Position);
	void SetViewpoint(const swVector3& Position, const swVector3& Target, const swVector3& Up);
	void ApplyStepView();
	void ApplyStepView(swStep Step);
	swVector3 GetStepViewAngles() const;
	void SetCameraAngles(float Latitude, float Longitude);
	void SetDefaultCamera();
	void SetCamera(swCamera* Camera, bool ForceCopy);
	void SetCamera(const QString& CameraName);
	void SetCameraIndex(size_t CameraIndex);

	void SetCameraProjection(swCameraProjection CameraProjection);
	void LookAt();
	void MoveCamera(const swVector3& Direction);
	void Zoom(float Amount);
	void ZoomExtents(swGeometryBoundsMode Mode, bool AdjustFarPlane);
	bool IsFitted() const;
	void ZoomExtentsAtStep(swStep Step, swGeometryBoundsMode Mode, bool AdjustFarPlane);

	void RemoveCamera();
	void ShowContextMenu() const;
	bool CloseFindReplaceDialog();
	void ShowFindReplaceWidget(bool Replace);

	float GetUIScale() const;
	float GetOverlayScale() const;
	swVector3 GetMoveDirection(const swVector3& Direction) const;
	void UpdatePartPreview();
	std::pair<std::vector<swInsertPartInformation>, bool> GetMouseInsertPartInfo(bool IgnoreSelected, bool AllowNewParts, const swPartDataReference& PartData, swPart* MovingPart) const;
	swVector3 GetCameraLightInsertPosition() const;
	void GetRayUnderPointer(swVector3& Start, swVector3& End) const;
	swObjectRayTest RayTest(bool PartsOnly, bool IgnoreSelected) const;
	swObjectSection FindObjectUnderPointer(bool PartsOnly, bool IgnoreSelected) const;
	std::vector<swObject*> FindObjectsInBox(float x1, float y1, float x2, float y2) const;

	swVector3 ProjectPoint(const swVector3& Point) const;
	swVector3 UnprojectPoint(const swVector3& Point) const;
	void UnprojectPoints(swVector3* Points, int NumPoints) const;
	swMatrix44 GetProjectionMatrix() const;

	bool BeginRenderToImage(int Width, int Height);
	void EndRenderToImage();
	QImage GetRenderImage() const;
	void BindRenderFramebuffer();
	void UnbindRenderFramebuffer();
	QImage GetRenderFramebufferImage() const;
	std::vector<QImage> GetStepImages(swStep Start, swStep End);
	swResult<void> SaveStepImages(const QString& BaseName, bool AddStepSuffix, swStep Start, swStep End, std::function<void(const QString&)> ProgressCallback);

	swContext* mContext = nullptr;

signals:
	void FocusReceived();

protected:
	void DestroyOutlineFramebuffers();
	bool GenerateOutlineMasks();

	void DrawBackground() const;
	void DrawViewport() const;
	void DrawAxes() const;
	void DrawCutaway() const;
	void DrawCutawayGizmo() const;

	void DrawSelectZoomRegionOverlay();
	void DrawRotateViewOverlay();
	void DrawGrid();

	swMatrix44 GetTileProjectionMatrix(int CurrentRow, int CurrentColumn, int CurrentTileWidth, int CurrentTileHeight) const;

	swCursor GetCursor() const;
	void SetCursor(swCursor Cursor);
	swTool GetCurrentTool() const;
	void UpdateTrackTool();
	swTrackTool GetOverrideTrackTool(Qt::MouseButton Button) const;
	void StartTracking(swTrackButton TrackButton);
	void OnButtonDown(swTrackButton TrackButton);
	void StartPan(int x, int y);
	void UpdatePan(int x, int y);
	swVector3 GetTrackballPoint(int x, int y) const;

	void ShowTrainTrackPopup();

	swViewWidget* mWidget = nullptr;
	int mWidth = 1;
	int mHeight = 1;
	swCamera* mZoomExtentsCamera = nullptr;
	swMatrix44 mZoomExtentsWorldView;
	swGeometryBoundsMode mZoomExtentsMode = {};
	bool mZoomExtentsFarPlane = false;
	swViewType mViewType;

	int mMouseX = 0;
	int mMouseY = 0;
	int mMouseDownX = 0;
	int mMouseDownY = 0;
	float mCameraRotationMouseAngle = 0.0f;
	float mCameraRotationLastMouseAngle = 0.0f;
	float mCameraRotationAngle = 0.0f;
	swVector3 mTrackballCenter;
	swVector3 mTrackballVector;
	float mTrackballRadius = 1.0f;
	swMatrix33 mTrackballPendingRotation = swMatrix33Identity();
	bool mTrackballSnapping = false;
	bool mTrackballHasPendingRotation = false;
	Qt::KeyboardModifiers mMouseModifiers = Qt::NoModifier;

	bool mTrackUpdated = false;
	bool mToolClicked = false;
	swTrackTool mTrackTool = swTrackTool::None;
	swTrackButton mTrackButton = swTrackButton::None;
	swCursor mCursor = swCursor::Default;
	bool mPickingFaceNormal = false;

	void PickFaceNormal();
	void StopFaceNormalPick();
	void PickCutaway();
	void StopCutawayPick();
	swMatrix44 GetCutawayMatrix() const;
	bool GetCutawayRange(const swVector3& Start, const swVector3& End, float& Entry, float& Exit) const;
	float GetCutawayHandleLength() const;
	int FindCutawayHandle() const;
	void UpdateCutawayDrag();

	swCutawayShape mCutawayPickShape = swCutawayShape::None;
	swCutawayShape mCutawayShape = swCutawayShape::None;
	swVector3 mCutawayCenter = swVector3(0.0f, 0.0f, 0.0f);
	const swVector3 mCutawayAxes[3] = { swVector3(1.0f, 0.0f, 0.0f), swVector3(0.0f, 1.0f, 0.0f), swVector3(0.0f, 0.0f, 1.0f) };
	swVector3 mCutawayShapeAxes[3] = { swVector3(1.0f, 0.0f, 0.0f), swVector3(0.0f, 1.0f, 0.0f), swVector3(0.0f, 0.0f, 1.0f) };
	swVector3 mCutawayNormal = swVector3(0.0f, 0.0f, 1.0f);
	float mCutawaySize = 40.0f;
	int mCutawayHoverHandle = -1;
	int mCutawayDragHandle = -1;
	swVector3 mCutawayDragCenter = swVector3(0.0f, 0.0f, 0.0f);
	int mCutawayDragMouseX = 0;
	int mCutawayDragMouseY = 0;
	float mCutawayDragSize = 40.0f;

	swDragState mDragState;
	bool mTrackToolFromOverlay;
	quint32 mTrackToolSection = ~0U;
	swMatrix44 mMouseDownTransform;
	swPart* mMouseDownPart = nullptr;
	int mPanX = 0;
	int mPanY = 0;

	QImage mRenderImage;
	std::unique_ptr<QOpenGLFramebufferObject> mRenderFramebuffer;
	std::unique_ptr<QOpenGLFramebufferObject> mOutlineFullFramebuffer;
	std::unique_ptr<QOpenGLFramebufferObject> mOutlineVisibleFramebuffer;
	QSize mOutlineAttemptSize;
	QPointer<QOpenGLContext> mOutlineGLContext;
	bool mOverrideBackgroundColor = false;
	float mLineWidthScale = 1.0f;
	bool mDrawViewportBorder = true;
	quint32 mBackgroundColor = 0;

	std::unique_ptr<swScene> mScene;
	std::unique_ptr<swViewManipulator> mViewManipulator;
	std::unique_ptr<swViewCube> mViewCube;

	swModel* mModel = nullptr;
	swPart* mActiveSubmodelInstance = nullptr;
	swMatrix44 mActiveSubmodelTransform;
	std::vector<swPart*> mSubmodelEditingPath;

	swCamera* mCamera = nullptr;

	swVertexBuffer mGridBuffer;
	int mGridSettings[7];

	static QPointer<swFindReplaceWidget> mFindWidget;
	static swFindReplaceParameters mFindReplaceParams;

	static swView* mLastFocusedView;
	static std::vector<swView*> mViews;
	static swCameraProjection mLastProjection;
};
