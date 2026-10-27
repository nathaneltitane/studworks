#pragma once

#include "sw_math.h"
#include "sw_types_asset.h"
#include "sw_commands.h"
#include "sw_object_property.h"
#include "sw_result.h"

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

enum class swPiecesListSort
{
	Color,
	Name,
	PartId
};

enum class swCameraProjection;
struct swSetInventoryItem;
struct swModelHistoryEditState;
struct swGroupHistoryState;
struct swPieceHistoryState;
struct swCameraHistoryState;
struct swLightHistoryState;
template<typename StateType> struct swObjectHistory;
class swModelHistory;
class swModelHistorySelect;
class swModelHistoryEdit;
class swModelHistoryProperties;
enum class swModelHistoryEditMerge;

#define SW_SELECTION_NO_PIECES                0x0001 // No pieces in model
#define SW_SELECTION_PIECE                    0x0002 // At least 1 piece selected
#define SW_SELECTION_CAMERA                   0x0004 // At least 1 camera selected
#define SW_SELECTION_LIGHT                    0x0008 // At least 1 light selected
#define SW_SELECTION_SELECTED                 0x0010 // At least 1 object selected
#define SW_SELECTION_UNSELECTED               0x0020 // At least 1 piece unselected
#define SW_SELECTION_HIDDEN                   0x0040 // At least one piece hidden
#define SW_SELECTION_HIDDEN_SELECTED          0x0080 // At least one piece selected is hidden
#define SW_SELECTION_VISIBLE_SELECTED         0x0100 // At least one piece selected is not hidden
#define SW_SELECTION_GROUPED                  0x0200 // At least one piece selected is grouped
#define SW_SELECTION_FOCUS_GROUPED            0x0400 // Focused piece is grouped
#define SW_SELECTION_CAN_GROUP                0x0800 // Can make a new group
#define SW_SELECTION_MODEL_SELECTED           0x1000 // At least one model reference is selected
#define SW_SELECTION_CAN_ADD_CONTROL_POINT    0x2000 // Can add control points to focused piece
#define SW_SELECTION_CAN_REMOVE_CONTROL_POINT 0x4000 // Can remove control points from focused piece
#define SW_SELECTION_TRAIN_TRACK_VISIBLE      0x8000 // Focused piece has train track connections

enum class swSelectionMode
{
	Single,
	Piece,
	Color,
	PieceColor
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

struct swInsertPieceInformation
{
	swPieceData* PieceData;
	swMatrix44 Transform;
	int ColorIndex;
};

// Stored insertion previews own their definitions; swInsertPieceInformation remains borrowed.
struct swPreviewInsertPieceInformation
{
	explicit swPreviewInsertPieceInformation(const swInsertPieceInformation& Info)
		: PieceData(Info.PieceData), Transform(Info.Transform), ColorIndex(Info.ColorIndex)
	{
	}

	swPieceDataReference PieceData;
	swMatrix44 Transform;
	int ColorIndex;
};

struct swModelPieceRename
{
	swPieceData* PieceData;
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
	float FloorAmbient = 0.4f;
	float FloorDiffuse = 0.4f;
	swVector3 FloorColor = swVector3(0.8f,0.8f,0.8f);
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

	bool GetPieceWorldMatrix(swPiece* Piece, swMatrix44& ParentWorldMatrix) const;
	bool IncludesModel(const swModel* Model) const;
	void CreatePieceData(Project* Project);
	bool RebindPieceData(swPieceData* Previous, swPieceData* Replacement);
	void UpdatePieceData(std::vector<swModel*>& UpdatedModels);
	void QueueMeshBuild();
	void UpdateAllViews() const;
	void RefreshPreview() const;
	std::vector<swPieceDataReference> GetRequiredPieceData() const;
	std::vector<swPiece*> GetRequiredSynthPieces() const;
	void WaitForAssets() const;
	swResult<void> EnsureAssetsReady() const;

	swPieceData* GetPieceData() const
	{
		return mPieceData.Get();
	}

	const std::vector<std::unique_ptr<swPiece>>& GetPieces() const
	{
		return mPieces;
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

	swPiece* AddPiece(swPieceData* PieceData, quint32 Section);
	void AddPiece(std::unique_ptr<swPiece> Piece, size_t PieceIndex);
	void DeleteSelectedObjects();
	void ResetSelectedPiecesPivotPoint();
	void RemoveSelectedObjectsKeyFrames();
	void InsertControlPoint();
	void RemoveFocusedControlPoint();
	void FocusNextTrainTrack();
	void FocusPreviousTrainTrack();
	void RotateFocusedTrainTrack(int Direction);
	void ShowSelectedPiecesEarlier();
	void ShowSelectedPiecesLater();
	void InsertStepAndMoveSelectedPieces(swStep Step);
	void SetPieceSteps(const std::vector<std::pair<swPiece*, swStep>>& PieceSteps);
	void RenamePieces(const std::vector<swModelPieceRename>& Renames);

	void MoveSelectionToModel(swModel* Model);
	void InlineSelectedModels();

	swGroup* AddGroup(const QString& Prefix, swGroup* Parent);
	swGroup* GetGroup(const QString& Name, bool CreateIfMissing);
	void RemoveGroup(swGroup* Group);
	void GroupSelection();
	void UngroupSelection();
	void AddSelectedPiecesToGroup();
	void RemoveFocusPieceFromGroup();
	void ShowEditGroupsDialog();

	void SaveLDraw(QTextStream& Stream, bool SelectedOnly, swStep LastStep) const;
	void LoadLDraw(QIODevice& Device, Project* Project);
	bool LoadBinary(swFile* File);
	bool LoadLDD(const QString& FileData);
	bool LoadInventory(const std::vector<swSetInventoryItem>& SetInventory);
	void Merge(std::unique_ptr<swModel> Other);

	void SetMinifigure(const swMinifigure& Minifigure);
	void SetPreviewPieceData(swPieceData* PieceData, int ColorIndex);

	swResult<void> Cut();
	swResult<void> Copy();
	swResult<void> Paste(bool PasteToCurrentStep);
	void DuplicateSelectedPieces();
	void PaintSelectedPieces();
	void UpdateTrainTrackConnections(swPiece* TrackPiece, bool IgnoreSelected) const;
	void UpdateSelectedPiecesTrainTrackConnections();
	void SetPreviewInsertPieceInfo(std::vector<swInsertPieceInformation>&& PreviewInsertPieceInfo);

	const std::vector<swPreviewInsertPieceInformation>& GetPreviewInsertPieceInfo() const
	{
		return mPreviewInsertPieceInfo;
	}

	void GetScene(swScene* Scene, const swCamera* ViewCamera, bool AllowHighlight, bool AllowFade) const;
	void AddSubModelRenderMeshes(swScene* Scene, const swMatrix44& WorldMatrix, int DefaultColorIndex, swRenderMeshState RenderMeshState, bool ParentActive) const;
	QImage GetStepImage(bool Zoom, int Width, int Height, swStep Step);
	// Empty inventory succeeds with a null image; rendering failures return an error.
	swResult<QImage> GetPartsListImage(int MaxWidth, int ThumbnailSize, swStep Step, quint32 BackgroundColor, quint32 ThumbnailBackgroundColor, QFont Font, QColor TextColor, bool KeepMissingParts, bool ShowCounts = true, swPiecesListSort Sort = swPiecesListSort::Color) const;
	swResult<void> SaveStepImages(const QString& BaseName, bool AddStepSuffix, bool Zoom, int Width, int Height, swStep Start, swStep End, swImageBackground Background);

	void RayTest(swObjectRayTest& ObjectRayTest) const;
	void BoxTest(swObjectBoxTest& ObjectBoxTest) const;
	bool SubModelMinIntersectDist(const swVector3& WorldStart, const swVector3& WorldEnd, float& MinDistance, swPieceRayTest& PieceRayTest) const;
	bool SubModelBoxTest(const swVector4 Planes[6]) const;
	void SubModelCompareBoundingBox(const swMatrix44& WorldMatrix, swVector3& Min, swVector3& Max) const;
	void SubModelAddBoundingBoxPoints(const swMatrix44& WorldMatrix, std::vector<swVector3>& Points, swGeometryBoundsMode Mode) const;

	bool HasPieces() const
	{
		return !mPieces.empty();
	}

	bool AnyPiecesSelected() const;
	bool AnyObjectsSelected() const;
	swModel* GetFirstSelectedSubmodel() const;
	void GetSubModels(std::set<swModel*>& SubModels) const;
	bool GetMoveRotateTransform(swVector3& Center, swMatrix33& RelativeRotation) const;
	bool CanRotateSelection() const;
	bool GetPieceFocusOrSelectionCenter(swVector3& Center) const;
	swVector3 GetSelectionOrModelCenter() const;
	bool GetFocusPosition(swVector3& Position) const;
	swObject* GetFocusObject() const;
	bool GetSelectionCenter(swVector3& Center) const;
	swBoundingBox GetAllPiecesBoundingBox() const;
	bool GetVisiblePiecesBoundingBox(swVector3& Min, swVector3& Max) const;
	std::vector<swVector3> GetPiecesBoundingBoxPoints(swGeometryBoundsMode Mode) const;
	void GetPartsList(int DefaultColorIndex, bool ScanSubModels, bool AddSubModels, swPiecesList& PartsList) const;
	void GetPartsListForStep(swStep Step, int DefaultColorIndex, swPiecesList& PartsList, bool Cumulative) const;
	void GetModelParts(const swMatrix44& WorldMatrix, int DefaultColorIndex, std::vector<swModelPiecesEntry>& ModelParts) const;
	void GetSelectionInformation(int* Flags, std::vector<swObject*>& Selection, swObject** Focus) const;

	void ClearSelectionAction();
	void SetSelectionAndFocusAction(const std::vector<swObject*>& Selection, swObject* Focus, quint32 Section, swSelectionMode SelectionMode);
	void FocusOrDeselectObjectAction(swObject* Object, uint32_t Section, swSelectionMode SelectionMode);
	void SelectAllPiecesAction();
	void InvertPieceSelectionAction();
	void AddToSelectionAction(const std::vector<swObject*>& Objects);
	void RemoveFromSelectionAction(const std::vector<swObject*>& Objects);

	void HideSelectedPieces();
	void HideUnselectedPieces();
	void UnhideSelectedPieces();
	void UnhideAllPieces();

	void FindReplacePiece(bool SearchForward, bool FindAll, bool Replace);

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
	bool InsertPieceToolClicked();
	void InsertCameraToolClicked(const swVector3& Position);
	void InsertLightToolClicked(const swVector3& Position, swLightType LightType);
	void UpdateMoveTool(const swVector3& Distance, bool AllowRelative, bool AlternateButtonDrag);
	void UpdateFreeMoveTool(swPiece* MousePiece, const swMatrix44& StartTransform, const swMatrix44& NewTransform, bool IsConnection, bool AlternateButtonDrag);
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

	void MoveSelectedObjects(const swVector3& PieceDistance, const swVector3& ObjectDistance, swModelTransformFlags Flags, swModelHistoryEditMerge ModelHistoryEditMerge);
	void RotateSelectedObjects(const swVector3& Angles, swModelTransformFlags Flags, swModelHistoryEditMerge ModelHistoryEditMerge);
	void RotateSelectedObjects(const swMatrix33& RotationMatrix, swModelTransformFlags Flags, swModelHistoryEditMerge ModelHistoryEditMerge);
	void ScaleSelectedPieces(const float Scale);
	void TransformSelectedObjects(swTransformType TransformType, const swVector3& Transform);
	void SetObjectsKeyFrame(const std::vector<swObject*>& Objects, swObjectPropertyId PropertyId, bool KeyFrame);
	void SetSelectedPiecesColorIndex(int ColorIndex);
	void SetSelectedPiecesStepShow(swStep Step);
	void SetSelectedPiecesStepHide(swStep Step);

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
	void RestorePieces(const swObjectHistory<swPieceHistoryState>& History, const std::unordered_map<uint32_t, swGroup*>& GroupsById);
	void RestoreCameras(const swObjectHistory<swCameraHistoryState>& History);
	void RestoreLights(const swObjectHistory<swLightHistoryState>& History);

	void AddSelectionHistory(std::function<void()> Callback);
	void ClearSelection();
	void SetFocus(swObject* FocusObject, uint32_t FocusSection, swSelectionMode SelectionMode);
	void SetSelectionAndFocus(const std::vector<swObject*>& Objects, swObject* FocusObject, uint32_t FocusSection, swSelectionMode SelectionMode);
	void SelectAllPieces();
	void InvertPieceSelection();
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

	std::vector<swObject*> GetSelectionModePieces(swSelectionMode SelectionMode, const swPiece* SelectedPiece) const;
	void DeselectAllObjects();
	void SetFocusedObject(swObject* FocusObject, uint32_t FocusSection, swSelectionMode SelectionMode);
	void SetObjectsSelected(const std::vector<swObject*>& Objects, bool Selected);
	void SelectGroup(swGroup* TopGroup, bool Select);

	size_t AddPiece(swPiece* Piece);
	void InsertStep(swStep Step);

	swPOVRayOptions mPOVRayOptions;
	swModelProperties mProperties;
	Project* mProject;
	// Search roots retained when a model moves between project directories.
	// Their order and the current-directory preference are saved with the model.
	std::vector<QString> mAssetSourceDirectories;
	bool mPreferCurrentAssetDirectory = true;
	swPieceDataReference mPieceData;

	bool mIsPreview;
	bool mActive;
	swStep mCurrentStep;
	swVector3 mMouseToolDistance;
	swVector3 mMouseToolMoveStartPosition;
	swVector3 mMouseToolMoveEndPosition;
	bool mMouseToolMovePositionValid = false;
	bool mMouseToolMoveUsesFocus = false;
	bool mMouseToolFirstMove;
	std::vector<swPreviewInsertPieceInformation> mPreviewInsertPieceInfo;

	std::vector<std::unique_ptr<swPiece>> mPieces;
	std::vector<std::unique_ptr<swCamera>> mCameras;
	std::vector<std::unique_ptr<swLight>> mLights;
	std::vector<std::unique_ptr<swGroup>> mGroups;
	QStringList mFileLines;
	bool mHasDirectGeometry = false;

	std::vector<std::unique_ptr<swModelHistory>> mHistorySequence;
	swObjectPropertyId mActivePropertyEdit = swObjectPropertyId::Count;
	quint64 mSavedHistoryRevision = 0;
	quint64 mNextHistoryRevision = 1;
	std::vector<std::unique_ptr<swModelHistoryEntry>> mUndoHistory;
	std::vector<std::unique_ptr<swModelHistoryEntry>> mRedoHistory;

	Q_DECLARE_TR_FUNCTIONS(swModel);
};
