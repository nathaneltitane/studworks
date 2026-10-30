#pragma once

#include "sw_math.h"
#include "sw_types_asset.h"
#include "sw_commands.h"
#include "sw_object_property.h"
#include "sw_result.h"
#include "sw_connectivity.h"

enum class swGeometryBoundsMode
{
	IncludeFallback,
	AvailableOnly
};

enum class swImageBackground
{
	Default,
	Transparent,
	Opaque
};

enum class swPartsListSort
{
	Color,
	Name,
	PartId
};

// the parts line up in rows or in columns
enum class swPartsListLayout
{
	Rows,
	Columns
};

// the thumbnail edge the parts line up on, top or left first
enum class swPartsListAlignment
{
	Start,
	Center,
	End
};

enum class swCameraProjection;
struct swSetInventoryItem;
struct swModelHistoryEditState;
struct swGroupHistoryState;
struct swPartHistoryState;
struct swCameraHistoryState;
struct swLightHistoryState;
template<typename StateType> struct swObjectHistory;
class swModelHistory;
class swModelHistorySelect;
class swModelHistoryEdit;
class swModelHistoryProperties;
enum class swModelHistoryEditMerge;

#define SW_SELECTION_NO_PARTS                0x0001 // No parts in model
#define SW_SELECTION_PART                    0x0002 // At least 1 part selected
#define SW_SELECTION_CAMERA                   0x0004 // At least 1 camera selected
#define SW_SELECTION_LIGHT                    0x0008 // At least 1 light selected
#define SW_SELECTION_SELECTED                 0x0010 // At least 1 object selected
#define SW_SELECTION_UNSELECTED               0x0020 // At least 1 part unselected
#define SW_SELECTION_HIDDEN                   0x0040 // At least one part hidden
#define SW_SELECTION_HIDDEN_SELECTED          0x0080 // At least one part selected is hidden
#define SW_SELECTION_VISIBLE_SELECTED         0x0100 // At least one part selected is not hidden
#define SW_SELECTION_GROUPED                  0x0200 // At least one part selected is grouped
#define SW_SELECTION_FOCUS_GROUPED            0x0400 // Focused part is grouped
#define SW_SELECTION_CAN_GROUP                0x0800 // Can make a new group
#define SW_SELECTION_MODEL_SELECTED           0x1000 // At least one model reference is selected
#define SW_SELECTION_CAN_ADD_CONTROL_POINT    0x2000 // Can add control points to focused part
#define SW_SELECTION_CAN_REMOVE_CONTROL_POINT 0x4000 // Can remove control points from focused part
#define SW_SELECTION_TRAIN_TRACK_VISIBLE      0x8000 // Focused part has train track connections

enum class swSelectionMode
{
	Single,
	Part,
	Color,
	PartColor
};

enum class swTransformType
{
	First,
	AbsoluteTranslation = First,
	RelativeTranslation,
	AbsoluteRotation,
	RelativeRotation,
	Count
};

enum class swModelTransformFlag
{
	None = 0,
	Relative = 1 << 0,
	RotatePivotPoint = 1 << 1,
	Checkpoint = 1 << 2,
	FirstMove = 1 << 3
};

Q_DECLARE_FLAGS(swModelTransformFlags, swModelTransformFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(swModelTransformFlags)

struct swInsertPartInformation
{
	swPartData* PartData;
	swMatrix44 Transform;
	int ColorIndex;
};

// Stored insertion previews own their definitions; swInsertPartInformation remains borrowed.
struct swPreviewInsertPartInformation
{
	explicit swPreviewInsertPartInformation(const swInsertPartInformation& Info)
		: PartData(Info.PartData), Transform(Info.Transform), ColorIndex(Info.ColorIndex)
	{
	}

	swPartDataReference PartData;
	swMatrix44 Transform;
	int ColorIndex;
};

struct swModelPartRename
{
	swPartData* PartData;
	QString OldName;
	QString NewName;
};

struct swModelAssetSourceState
{
	std::vector<QString> Directories;
	bool PreferCurrentDirectory;
};

enum class swStepViewMode
{
	Relative,
	Absolute,
	Add,
	End
};

// View rotation of a step, stored as an MLCAD 0 ROTSTEP line ending that step.
struct swStepView
{
	swStepViewMode Mode = swStepViewMode::Relative;
	swVector3 Angles = swVector3(0.0f, 0.0f, 0.0f);

	bool operator==(const swStepView& Other) const
	{
		return Mode == Other.Mode && Angles == Other.Angles;
	}

	bool operator!=(const swStepView& Other) const
	{
		return !(*this == Other);
	}
};

class swModelProperties
{
public:
	void LoadDefaults();
	void SaveDefaults();

	bool operator==(const swModelProperties& Properties) const
	{
		if (mFileName != Properties.mFileName || mModelName != Properties.mModelName || mAuthor != Properties.mAuthor ||
			mDescription != Properties.mDescription || mComments != Properties.mComments)
			return false;

		if (mAmbientColor != Properties.mAmbientColor)
			return false;

		if (mStepViews != Properties.mStepViews)
			return false;

		return true;
	}

	bool operator!=(const swModelProperties& Properties) const
	{
		return !(*this == Properties);
	}

	void SaveLDraw(QTextStream& Stream) const;
	bool ParseLDrawHeader(QString Line, bool FirstLine);
	void ParseLDrawLine(QTextStream& Stream);

	QString mFileName;
	QString mDescription;
	QString mModelName;
	QString mAuthor;
	QString mComments;

	swVector3 mAmbientColor;
	std::map<swStep, swStepView> mStepViews;
};

class swPOVRayOptions
{
public:
	void ParseLDrawLine(QTextStream& LineStream);
	void SaveLDraw(QTextStream& Stream) const;

	bool UseLGEO = false;
	bool ExcludeFloor = false;
	bool ExcludeBackground = false;
	bool NoReflection = false;
	bool NoShadow = false;
	int FloorAxis = 1;
	std::optional<float> FloorAmbient;
	std::optional<float> FloorDiffuse;
	std::optional<swVector3> FloorColor;
	QString HeaderIncludeFile;
	QString FooterIncludeFile;
};

struct swModelHistoryEntry
{
	std::vector<std::unique_ptr<swModelHistory>> HistorySequence;
	QString Description;
	quint64 Revision = 0;
};

class swModel
{
public:
	swModel(const QString& FileName, Project* Project, bool Preview);
	~swModel();

	swModel(const swModel&) = delete;
	swModel(swModel&&) = delete;
	swModel& operator=(const swModel&) = delete;
	swModel& operator=(swModel&&) = delete;

	Project* GetProject() const
	{
		return mProject;
	}

	void SetProject(Project* Project);
	void PreserveAssetSourceDirectory(const QString& Directory, bool PreferCurrentDirectory);
	swModelAssetSourceState GetAssetSourceState() const;
	void RestoreAssetSourceState(const swModelAssetSourceState& State);
	std::vector<QString> GetAssetSearchDirectories() const;

	bool IsActive() const
	{
		return mActive;
	}

	bool IsModified() const;
	void SetSaved();

	bool GetPartWorldMatrix(swPart* Part, swMatrix44& ParentWorldMatrix) const;
	bool IncludesModel(const swModel* Model) const;
	void CreatePartData(Project* Project);
	bool RebindPartData(swPartData* Previous, swPartData* Replacement);
	void UpdatePartData(std::vector<swModel*>& UpdatedModels);
	void QueueMeshBuild();
	void UpdateAllViews() const;
	void RefreshPreview() const;
	std::vector<swPartDataReference> GetRequiredPartData() const;
	std::vector<swPart*> GetRequiredSynthParts() const;
	void WaitForAssets() const;
	swResult<void> EnsureAssetsReady() const;

	swPartData* GetPartData() const
	{
		return mPartData.Get();
	}

	const std::vector<std::unique_ptr<swPart>>& GetParts() const
	{
		return mParts;
	}

	const std::vector<std::unique_ptr<swCamera>>& GetCameras() const
	{
		return mCameras;
	}

	const std::vector<std::unique_ptr<swLight>>& GetLights() const
	{
		return mLights;
	}

	const std::vector<std::unique_ptr<swGroup>>& GetGroups() const
	{
		return mGroups;
	}

	const swModelProperties& GetProperties() const
	{
		return mProperties;
	}

	const swPOVRayOptions& GetPOVRayOptions() const
	{
		return mPOVRayOptions;
	}

	void SetFileName(const QString& FileName)
	{
		if (mProperties.mModelName == mProperties.mFileName)
			mProperties.mModelName = FileName;
		mProperties.mFileName = FileName;
	}

	const QString& GetFileName() const
	{
		return mProperties.mFileName;
	}

	void SetDescription(const QString& Description)
	{
		mProperties.mDescription = Description;
	}

	const QStringList& GetFileLines() const
	{
		return mFileLines;
	}

	// the instructions meta commands of the model without their marker and object
	const QStringList& GetInstructionLines() const
	{
		return mInstructionLines;
	}

	void SetInstructionLines(const QStringList& InstructionLines)
	{
		mInstructionLines = InstructionLines;
	}

	bool HasDirectGeometry() const
	{
		return mHasDirectGeometry;
	}

	swStep GetLastStep() const;

	swStep GetCurrentStep() const
	{
		return mCurrentStep;
	}

	void SetActive(bool Active);
	void CalculateStep(swStep Step);
	void SetCurrentStep(swStep Step);
	void SetTemporaryStep(swStep Step)
	{
		mCurrentStep = Step;
		CalculateStep(Step);
	}

	void ShowFirstStep();
	void ShowLastStep();
	void ShowPreviousStep();
	void ShowNextStep();
	void InsertStepAction(swStep Step);
	void ShiftStepViews(swStep Step, int Offset);
	void SetStepView(swStep Step, const std::optional<swStepView>& StepView);
	std::optional<swStepView> GetEffectiveStepView(swStep Step) const;
	void RemoveStepAction(swStep Step);

	void LoadEditHistoryState(const swModelHistoryEditState& HistoryState);
	void LoadModelPropertiesState(const swModelProperties& ModelProperties);

	swPart* AddPart(swPartData* PartData, quint32 Section);
	void AddPart(std::unique_ptr<swPart> Part, size_t PartIndex);
	void DeleteSelectedObjects();
	void ResetSelectedPartsPivotPoint();
	void RemoveSelectedObjectsKeyFrames();
	void InsertControlPoint();
	void RemoveFocusedControlPoint();
	void FocusNextTrainTrack();
	void FocusPreviousTrainTrack();
	void RotateFocusedTrainTrack(int Direction);
	void ShowSelectedPartsEarlier();
	void ShowSelectedPartsLater();
	void InsertStepAndMoveSelectedParts(swStep Step);
	void SetPartSteps(const std::vector<std::pair<swPart*, swStep>>& PartSteps);
	void RenameParts(const std::vector<swModelPartRename>& Renames);

	void MoveSelectionToModel(swModel* Model);
	void InlineSelectedModels();

	swGroup* AddGroup(const QString& Prefix, swGroup* Parent);
	swGroup* GetGroup(const QString& Name, bool CreateIfMissing);
	void RemoveGroup(swGroup* Group);
	void GroupSelection();
	void UngroupSelection();
	void AddSelectedPartsToGroup();
	void RemoveFocusPartFromGroup();
	void ShowEditGroupsDialog();

	void SaveLDraw(QTextStream& Stream, bool SelectedOnly, swStep LastStep) const;
	void LoadLDraw(QIODevice& Device, Project* Project);
	bool LoadBinary(swFile* File);
	bool LoadLDD(const QString& FileData);
	bool LoadInventory(const std::vector<swSetInventoryItem>& SetInventory);
	void Merge(std::unique_ptr<swModel> Other);

	void SetMinifigure(const swMinifigure& Minifigure);
	void SetPreviewPartData(swPartData* PartData, int ColorIndex);

	swResult<void> Cut();
	swResult<void> Copy();
	swResult<void> Paste(bool PasteToCurrentStep);
	void DuplicateSelectedParts();
	void PaintSelectedParts();
	void UpdateTrainTrackConnections(swPart* TrackPart, bool IgnoreSelected) const;
	void UpdateSelectedPartsTrainTrackConnections();
	void SetPreviewInsertPartInfo(std::vector<swInsertPartInformation>&& PreviewInsertPartInfo);

	const std::vector<swPreviewInsertPartInformation>& GetPreviewInsertPartInfo() const
	{
		return mPreviewInsertPartInfo;
	}

	void GetScene(swScene* Scene, const swCamera* ViewCamera, bool AllowHighlight, bool AllowFade) const;
	void AddSubModelRenderMeshes(swScene* Scene, const swMatrix44& WorldMatrix, int DefaultColorIndex, swRenderMeshState RenderMeshState, bool ParentActive) const;
	QImage GetStepImage(bool Zoom, int Width, int Height, swStep Step);
	// Empty inventory succeeds with a null image; rendering failures return an error.
	swResult<QImage> GetPartsListImage(int MaxWidth, int ThumbnailSize, swStep Step, quint32 BackgroundColor, quint32 ThumbnailBackgroundColor, QFont Font, QColor TextColor, bool KeepMissingParts, bool ShowCounts = true, swPartsListSort Sort = swPartsListSort::Color, swPartsListLayout Layout = swPartsListLayout::Rows, swPartsListAlignment Alignment = swPartsListAlignment::End, int MaxHeight = INT_MAX, float Scale = 1.0f) const;
	swResult<void> SaveStepImages(const QString& BaseName, bool AddStepSuffix, bool Zoom, int Width, int Height, swStep Start, swStep End, swImageBackground Background);

	void RayTest(swObjectRayTest& ObjectRayTest) const;
	void BoxTest(swObjectBoxTest& ObjectBoxTest) const;
	bool SubModelMinIntersectDist(const swVector3& WorldStart, const swVector3& WorldEnd, float& MinDistance, swPartRayTest& PartRayTest) const;
	bool SubModelBoxTest(const swVector4 Planes[6]) const;
	void SubModelCompareBoundingBox(const swMatrix44& WorldMatrix, swVector3& Min, swVector3& Max) const;
	void SubModelAddBoundingBoxPoints(const swMatrix44& WorldMatrix, std::vector<swVector3>& Points, swGeometryBoundsMode Mode) const;

	bool HasParts() const
	{
		return !mParts.empty();
	}

	bool AnyPartsSelected() const;
	bool AnyObjectsSelected() const;
	swModel* GetFirstSelectedSubmodel() const;
	void GetSubModels(std::set<swModel*>& SubModels) const;
	bool GetMoveRotateTransform(swVector3& Center, swMatrix33& RelativeRotation) const;
	bool CanRotateSelection() const;
	bool GetPartFocusOrSelectionCenter(swVector3& Center) const;
	swVector3 GetSelectionOrModelCenter() const;
	bool GetFocusPosition(swVector3& Position) const;
	swObject* GetFocusObject() const;
	bool GetSelectionCenter(swVector3& Center) const;
	swBoundingBox GetAllPartsBoundingBox() const;
	bool GetVisiblePartsBoundingBox(swVector3& Min, swVector3& Max) const;
	std::vector<swVector3> GetPartsBoundingBoxPoints(swGeometryBoundsMode Mode) const;
	void GetPartsList(int DefaultColorIndex, bool ScanSubModels, bool AddSubModels, swPartsList& PartsList) const;
	void GetPartsListForStep(swStep Step, int DefaultColorIndex, swPartsList& PartsList, bool Cumulative) const;
	void GetModelParts(const swMatrix44& WorldMatrix, int DefaultColorIndex, std::vector<swModelPartsEntry>& ModelParts) const;
	void GetSelectionInformation(int* Flags, std::vector<swObject*>& Selection, swObject** Focus) const;

	void ClearSelectionAction();
	void SetSelectionAndFocusAction(const std::vector<swObject*>& Selection, swObject* Focus, quint32 Section, swSelectionMode SelectionMode);
	void FocusOrDeselectObjectAction(swObject* Object, uint32_t Section, swSelectionMode SelectionMode);
	void SelectAllPartsAction();
	void InvertPartSelectionAction();
	void SelectConnectedPartsAction(bool AllConnected);
	void AddToSelectionAction(const std::vector<swObject*>& Objects);
	void RemoveFromSelectionAction(const std::vector<swObject*>& Objects);

	void HideSelectedParts();
	void HideUnselectedParts();
	void UnhideSelectedParts();
	void UnhideAllParts();

	void FindReplacePart(bool SearchForward, bool FindAll, bool Replace);

	void UndoAction();
	void RedoAction();

	swVector3 SnapPosition(const swVector3& Delta) const;
	swVector3 SnapRotation(const swVector3& Delta) const;
	swMatrix33 GetRelativeRotation() const;

	const swVector3& GetMouseToolDistance() const
	{
		return mMouseToolDistance;
	}

	bool GetMouseToolMovePositions(swVector3& Start, swVector3& End) const
	{
		if (!mMouseToolMovePositionValid)
			return false;

		Start = mMouseToolMoveStartPosition;
		End = mMouseToolMoveEndPosition;

		return true;
	}

	void BeginMouseTool(swTool Tool, swView* View);
	void EndMouseTool(swTool Tool, swView* View, bool Accept);
	bool InsertPartToolClicked();
	void InsertCameraToolClicked(const swVector3& Position);
	void InsertLightToolClicked(const swVector3& Position, swLightType LightType);
	void UpdateMoveTool(const swVector3& Distance, bool AllowRelative, bool AlternateButtonDrag);
	void UpdateFreeMoveTool(swPart* MousePart, const swMatrix44& StartTransform, const swMatrix44& NewTransform, bool IsConnection, bool AlternateButtonDrag);
	void UpdateRotateTool(const swVector3& Axis, float Angle, bool AlternateButtonDrag, bool Relative);
	void UpdateRotateTool(const swMatrix33& RotationMatrix, bool AlternateButtonDrag, bool Relative);
	void UpdateScaleTool(const float Scale);
	void EraserToolClicked(swObject* Object);
	void PaintToolClicked(swObject* Object);
	void ColorPickerToolClicked(const swObject* Object);
	void UpdateZoomTool(swCamera* Camera, float Mouse);
	void UpdatePanTool(swCamera* Camera, const swVector3& Distance);
	void UpdateOrbitTool(swCamera* Camera, float MouseX, float MouseY);
	void UpdateRollTool(swCamera* Camera, float Mouse);
	void ZoomRegionToolClicked(swView* View, float AspectRatio, const swVector3& Position, const swVector3& TargetPosition, const swVector3* Corners);
	void LookAt(swCamera* Camera);
	void MoveCamera(swCamera* Camera, const swVector3& Direction);
	void ZoomExtents(swCamera* Camera, float Aspect, const swMatrix44& WorldMatrix, swGeometryBoundsMode Mode, bool AdjustFarPlane);
	void Zoom(swCamera* Camera, float Amount);

	void MoveSelectedObjects(const swVector3& Distance, swModelTransformFlags Flags, swModelHistoryEditMerge ModelHistoryEditMerge)
	{
		MoveSelectedObjects(Distance, Distance, Flags, ModelHistoryEditMerge);
	}

	void MoveSelectedObjects(const swVector3& PartDistance, const swVector3& ObjectDistance, swModelTransformFlags Flags, swModelHistoryEditMerge ModelHistoryEditMerge);
	void RotateSelectedObjects(const swVector3& Angles, swModelTransformFlags Flags, swModelHistoryEditMerge ModelHistoryEditMerge);
	void RotateSelectedObjects(const swMatrix33& RotationMatrix, swModelTransformFlags Flags, swModelHistoryEditMerge ModelHistoryEditMerge);
	void ScaleSelectedParts(const float Scale);
	void TransformSelectedObjects(swTransformType TransformType, const swVector3& Transform);
	void SetObjectsKeyFrame(const std::vector<swObject*>& Objects, swObjectPropertyId PropertyId, bool KeyFrame);
	void SetSelectedPartsColorIndex(int ColorIndex);
	void SetSelectedPartsStepShow(swStep Step);
	void SetSelectedPartsStepHide(swStep Step);

	void SetObjectsProperty(const std::vector<swObject*>& Objects, swObjectPropertyId PropertyId, QVariant Value);
	void BeginPropertyEdit(swObjectPropertyId PropertyId);
	void EndPropertyEdit(swObjectPropertyId PropertyId, bool Accept);

	void SetCameraProjection(swCamera* Camera, swCameraProjection CameraProjection);

	void ShowPropertiesDialog();
	void ShowArrayDialog();
	void AddMinifigure(const swMinifigure& Minifigure);
	void UpdateInterface();

protected:
	swResult<QImage> RenderStepImageWithReadyAssets(bool Zoom, int Width, int Height, swStep Step, swCamera* OutputCamera, swImageBackground Background);

	void DeleteModel();

	std::unordered_map<uint32_t, swGroup*> RestoreGroups(const swObjectHistory<swGroupHistoryState>& History);
	void RestoreParts(const swObjectHistory<swPartHistoryState>& History, const std::unordered_map<uint32_t, swGroup*>& GroupsById);
	void RestoreCameras(const swObjectHistory<swCameraHistoryState>& History);
	void RestoreLights(const swObjectHistory<swLightHistoryState>& History);

	void AddSelectionHistory(std::function<void()> Callback);
	void ClearSelection();
	void SetFocus(swObject* FocusObject, uint32_t FocusSection, swSelectionMode SelectionMode);
	void SetSelectionAndFocus(const std::vector<swObject*>& Objects, swObject* FocusObject, uint32_t FocusSection, swSelectionMode SelectionMode);
	void SelectAllParts();
	void InvertPartSelection();
	std::vector<swObject*> FindConnectedParts(bool AllConnected) const;
	static void AddPartShapes(const swPart* Part, const swMatrix44& Transform, swConnectionShapes& Shapes);
	// Seats the fitting at a dropped end control point of a flexible part on the nearest connection that takes it.
	bool SnapControlPointToConnection(swPart* Part, quint32 ControlPointIndex) const;
	void AddToSelection(const std::vector<swObject*>& Objects);
	void RemoveFromSelection(const std::vector<swObject*>& Objects);
	void BeginEditHistory();
	void BeginMergeableEditHistory(swModelHistoryEditMerge ModelHistoryEditMerge, swObjectPropertyId PropertyId);
	void EndEditHistory();
	void SetModelProperties(const swModelProperties& ModelProperties);

	void RunHistorySequence(const std::vector<std::unique_ptr<swModelHistory>>& HistorySequence, bool Apply);
	void BeginHistorySequence();
	void EndHistorySequence(const QString& Description);
	void DiscardHistorySequence();
	void RevertHistorySequence();
	void RemoveFirstUndoIfUnchanged();
	quint64 GetCurrentHistoryRevision() const;

	QString GetGroupName(const QString& Prefix);
	void RemoveEmptyGroups();
	bool RemoveSelectedObjects();
	void RemoveCameraFromViews(swCamera* Camera);

	std::vector<swObject*> GetSelectionModeParts(swSelectionMode SelectionMode, const swPart* SelectedPart) const;
	void DeselectAllObjects();
	void SetFocusedObject(swObject* FocusObject, uint32_t FocusSection, swSelectionMode SelectionMode);
	void SetObjectsSelected(const std::vector<swObject*>& Objects, bool Selected);
	void SelectGroup(swGroup* TopGroup, bool Select);

	size_t AddPart(swPart* Part);
	void InsertStep(swStep Step);

	swPOVRayOptions mPOVRayOptions;
	swModelProperties mProperties;
	Project* mProject;
	// Search roots retained when a model moves between project directories.
	// Their order and the current-directory preference are saved with the model.
	std::vector<QString> mAssetSourceDirectories;
	bool mPreferCurrentAssetDirectory = true;
	swPartDataReference mPartData;

	bool mIsPreview;
	bool mActive;
	swStep mCurrentStep;
	swVector3 mMouseToolDistance;
	swVector3 mMouseToolMoveStartPosition;
	swVector3 mMouseToolMoveEndPosition;
	bool mMouseToolMovePositionValid = false;
	bool mMouseToolMoveUsesFocus = false;
	bool mMouseToolFirstMove;
	std::vector<swPreviewInsertPartInformation> mPreviewInsertPartInfo;

	std::vector<std::unique_ptr<swPart>> mParts;
	std::vector<std::unique_ptr<swCamera>> mCameras;
	std::vector<std::unique_ptr<swLight>> mLights;
	std::vector<std::unique_ptr<swGroup>> mGroups;
	QStringList mFileLines;
	QStringList mInstructionLines;
	bool mHasDirectGeometry = false;

	std::vector<std::unique_ptr<swModelHistory>> mHistorySequence;
	swObjectPropertyId mActivePropertyEdit = swObjectPropertyId::Count;
	quint64 mSavedHistoryRevision = 0;
	quint64 mNextHistoryRevision = 1;
	std::vector<std::unique_ptr<swModelHistoryEntry>> mUndoHistory;
	std::vector<std::unique_ptr<swModelHistoryEntry>> mRedoHistory;

	Q_DECLARE_TR_FUNCTIONS(swModel);
};
