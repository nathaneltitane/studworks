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

static void swTogglePreference(bool& Value, SW_PROFILE_KEY Key)
{
	Value = !Value;
	swSetProfileInteger(Key, Value);

	swView::UpdateAllViews();
}

void swMainWindow::HandleCommand(swCommandId CommandId)
{
	swView* ActiveView = GetActiveView();

	if (ActiveView && CommandId != SW_EDIT_CANCEL)
		ActiveView->StopTracking(true);

	swModel* ActiveModel = ActiveView ? ActiveView->GetActiveModel() : nullptr;

	switch (CommandId)
	{
	case SW_FILE_NEW:
		NewProject();
		break;

	case SW_FILE_OPEN:
		OpenProject(QString());
		break;

	case SW_FILE_MERGE:
		ShowMergeDialog();
		break;

	case SW_FILE_RELOAD:
		ReloadProject();
		break;

	case SW_FILE_TOGGLE_WATCH_FILE_CHANGES:
		ToggleWatchFileChanges();
		break;

	case SW_FILE_SAVE:
		SaveProject(swGetActiveProject()->GetFileName());
		break;

	case SW_FILE_SAVE_AS:
		SaveProject(QString());
		break;

	case SW_FILE_SAVE_IMAGE:
		ShowImageDialog();
		break;

	case SW_FILE_IMPORT_LDD:
		ImportLDD();
		break;

	case SW_FILE_IMPORT_INVENTORY:
		ImportInventory();
		break;

	case SW_FILE_EXPORT_3DS:
		ShowResultError(swGetActiveProject()->Export3DStudio(QString()), tr("Export 3DS"));
		break;

	case SW_FILE_EXPORT_COLLADA:
		ShowResultError(swGetActiveProject()->ExportCOLLADA(QString()), tr("Export COLLADA"));
		break;

	case SW_FILE_EXPORT_HTML:
		ShowHTMLDialog();
		break;

	case SW_FILE_EXPORT_BRICKLINK:
		ShowResultError(swGetActiveProject()->ExportBrickLink(), tr("Export BrickLink"));
		break;

	case SW_FILE_EXPORT_CSV:
		ShowResultError(swGetActiveProject()->ExportCSV(QString()), tr("Export CSV"));
		break;

	case SW_FILE_EXPORT_POVRAY:
		ShowExportPOVRayDialog();
		break;

	case SW_FILE_EXPORT_WAVEFRONT:
		ShowResultError(swGetActiveProject()->ExportWavefront(QString()), tr("Export Wavefront"));
		break;

	case SW_FILE_RENDER_POVRAY:
		ShowRenderDialog(swRenderDialogMode::RenderPOVRay);
		break;

	case SW_FILE_RENDER_BLENDER:
		ShowRenderDialog(swRenderDialogMode::RenderBlender);
		break;

	case SW_FILE_RENDER_OPEN_IN_BLENDER:
		ShowRenderDialog(swRenderDialogMode::OpenInBlender);
		break;

	case SW_FILE_INSTRUCTIONS:
		ShowInstructionsDialog();
		break;

	case SW_FILE_PRINT:
		ShowPrintWidget();
		break;

	case SW_FILE_RECENT1:
	case SW_FILE_RECENT2:
	case SW_FILE_RECENT3:
	case SW_FILE_RECENT4:
		OpenRecentProject(CommandId - SW_FILE_RECENT1);
		break;

	case SW_FILE_EXIT:
		close();
		break;

	case SW_EDIT_UNDO:
		if (ActiveModel)
			ActiveModel->UndoAction();
		break;

	case SW_EDIT_REDO:
		if (ActiveModel)
			ActiveModel->RedoAction();
		break;

	case SW_EDIT_CUT:
		if (ActiveModel)
			ShowResultError(ActiveModel->Cut(), tr("Cannot Cut Model"));
		break;

	case SW_EDIT_COPY:
		if (ActiveModel)
			ShowResultError(ActiveModel->Copy(), tr("Cannot Copy Model"));
		break;

	case SW_EDIT_PASTE:
		if (ActiveModel)
			ShowResultError(ActiveModel->Paste(true), tr("Cannot Paste Model"));
		break;

	case SW_EDIT_PASTE_STEPS:
		if (ActiveModel)
			ShowResultError(ActiveModel->Paste(false), tr("Cannot Paste Model"));
		break;

	case SW_EDIT_FIND:
		if (ActiveView)
			ActiveView->ShowFindReplaceWidget(false);
		break;

	case SW_EDIT_FIND_NEXT:
		if (ActiveModel)
			ActiveModel->FindReplacePiece(true, false, false);
		break;

	case SW_EDIT_FIND_PREVIOUS:
		if (ActiveModel)
			ActiveModel->FindReplacePiece(false, false, false);
		break;

	case SW_EDIT_FIND_ALL:
		if (ActiveModel)
			ActiveModel->FindReplacePiece(true, true, false);
		break;

	case SW_EDIT_REPLACE:
		if (ActiveView)
			ActiveView->ShowFindReplaceWidget(true);
		break;

	case SW_EDIT_REPLACE_ALL:
		if (ActiveModel)
			ActiveModel->FindReplacePiece(true, true, true);
		break;

	case SW_EDIT_REPLACE_NEXT:
		if (ActiveModel)
			ActiveModel->FindReplacePiece(true, false, true);
		break;

	case SW_EDIT_SELECT_ALL:
		if (ActiveModel)
			ActiveModel->SelectAllPiecesAction();
		break;

	case SW_EDIT_SELECT_NONE:
		if (ActiveModel)
			ActiveModel->ClearSelectionAction();
		break;

	case SW_EDIT_SELECT_INVERT:
		if (ActiveModel)
			ActiveModel->InvertPieceSelectionAction();
		break;

	case SW_EDIT_SELECT_BY_NAME:
		ShowSelectDialog();
		break;

	case SW_EDIT_SELECTION_SINGLE:
		SetSelectionMode(swSelectionMode::Single);
		break;

	case SW_EDIT_SELECTION_PIECE:
		SetSelectionMode(swSelectionMode::Piece);
		break;

	case SW_EDIT_SELECTION_COLOR:
		SetSelectionMode(swSelectionMode::Color);
		break;

	case SW_EDIT_SELECTION_PIECE_COLOR:
		SetSelectionMode(swSelectionMode::PieceColor);
		break;

	case SW_VIEW_SPLIT_HORIZONTAL:
		SplitHorizontal();
		break;

	case SW_VIEW_SPLIT_VERTICAL:
		SplitVertical();
		break;

	case SW_VIEW_REMOVE_VIEW:
		RemoveActiveView();
		break;

	case SW_VIEW_RESET_VIEWS:
		ResetViews();
		break;

	case SW_VIEW_TOOLBAR_STANDARD:
		ToggleDockWidget(mStandardToolBar);
		break;

	case SW_VIEW_TOOLBAR_TOOLS:
		ToggleDockWidget(mToolsToolBar);
		break;

	case SW_VIEW_TOOLBAR_TIME:
		ToggleDockWidget(mTimeToolBar);
		break;

	case SW_VIEW_TOOLBAR_PARTS:
		ToggleDockWidget(mPartsToolBar);
		if (mPartsToolBar->isVisible())
			mPartSelectionWidget->FocusPartFilterWidget();
		break;

	case SW_VIEW_TOOLBAR_COLORS:
		ToggleDockWidget(mColorsToolBar);
		break;

	case SW_VIEW_TOOLBAR_PROPERTIES:
		ToggleDockWidget(mPropertiesToolBar);
		break;

	case SW_VIEW_TOOLBAR_TIMELINE:
		ToggleDockWidget(mTimelineToolBar);
		break;

	case SW_VIEW_TOOLBAR_PREVIEW:
		ToggleDockWidget(mPreviewToolBar);
		break;

	case SW_VIEW_TOOLBAR_TERMINAL:
		if (mTerminalToolBar)
			ToggleDockWidget(mTerminalToolBar);
		break;

	case SW_VIEW_TOOLBAR_MINIFIGURE_BUILDER:
		ToggleDockWidget(mMinifigureBuilderToolBar);
		break;

	case SW_VIEW_FULL_SCREEN:
		ToggleFullScreen();
		break;

	case SW_VIEW_CLOSE_CURRENT_TAB:
		CloseCurrentModelTab();
		break;

	case SW_VIEW_SHADING_WIREFRAME:
		SetShadingMode(swShadingMode::Wireframe);
		break;

	case SW_VIEW_SHADING_FLAT:
		SetShadingMode(swShadingMode::Flat);
		break;

	case SW_VIEW_SHADING_DEFAULT_LIGHTS:
		SetShadingMode(swShadingMode::DefaultLights);
		break;

	case SW_VIEW_LIGHTING_STANDARD:
		SetLightingStyle(swLightingStyle::Standard);
		break;

	case SW_VIEW_LIGHTING_STUDIO:
		SetLightingStyle(swLightingStyle::Studio);
		break;

	case SW_VIEW_LIGHTING_MATTE:
		SetLightingStyle(swLightingStyle::Matte);
		break;

	case SW_VIEW_LIGHTING_TOON:
		SetLightingStyle(swLightingStyle::Toon);
		break;

	case SW_VIEW_PROJECTION_PERSPECTIVE:
		if (ActiveView)
			ActiveView->SetCameraProjection(swCameraProjection::Perspective);
		break;

	case SW_VIEW_PROJECTION_ORTHOGRAPHIC:
		if (ActiveView)
			ActiveView->SetCameraProjection(swCameraProjection::Orthographic);
		break;

	case SW_VIEW_TOGGLE_VIEW_SPHERE:
		ToggleViewSphere();
		break;

	case SW_VIEW_TOGGLE_AXIS_ICON:
		ToggleAxisIcon();
		break;

	case SW_VIEW_TOGGLE_GRID:
		ToggleGrid();
		break;

	case SW_VIEW_FADE_PREVIOUS_STEPS:
		ToggleFadePreviousSteps();
		break;

	case SW_VIEW_STEP_VIEW_EDIT:
		ShowStepViewDialog();
		break;

	case SW_VIEW_STEP_VIEW_REMOVE:
		if (ActiveModel)
			ActiveModel->SetStepView(ActiveModel->GetCurrentStep(), std::nullopt);
		break;

	case SW_VIEW_STEP_VIEW_TOGGLE_FOLLOW:
		swGetPreferences().mFollowStepViews = !swGetPreferences().mFollowStepViews;
		swSetProfileInteger(SW_PROFILE_FOLLOW_STEP_VIEWS, swGetPreferences().mFollowStepViews);
		mActions[SW_VIEW_STEP_VIEW_TOGGLE_FOLLOW]->setChecked(swGetPreferences().mFollowStepViews);

		if (swGetPreferences().mFollowStepViews && ActiveView)
			ActiveView->ApplyStepView();
		break;

	case SW_VIEW_NORMAL_TO_FACE:
		if (ActiveView)
			ActiveView->StartFaceNormalPick();
		break;

	case SW_VIEW_GRID_TOGGLE_HEIGHT_XZ:
		swGetPreferences().mDrawGridHeightXZ = !swGetPreferences().mDrawGridHeightXZ;
		swSetProfileInteger(SW_PROFILE_GRID_HEIGHT_XZ, swGetPreferences().mDrawGridHeightXZ);
		swView::UpdateAllViews();
		break;

	case SW_VIEW_GRID_TOGGLE_HEIGHT_YZ:
		swGetPreferences().mDrawGridHeightYZ = !swGetPreferences().mDrawGridHeightYZ;
		swSetProfileInteger(SW_PROFILE_GRID_HEIGHT_YZ, swGetPreferences().mDrawGridHeightYZ);
		swView::UpdateAllViews();
		break;

	case SW_VIEW_GRID_TOGGLE_HEIGHT_BEHIND:
		swGetPreferences().mGridHeightBehindModel = !swGetPreferences().mGridHeightBehindModel;
		swSetProfileInteger(SW_PROFILE_GRID_HEIGHT_BEHIND_MODEL, swGetPreferences().mGridHeightBehindModel);
		swView::UpdateAllViews();
		break;

	case SW_VIEW_TERMINAL_TOGGLE_FOLLOW_MODEL:
		swGetPreferences().mTerminalFollowModel = !swGetPreferences().mTerminalFollowModel;
		swSetProfileInteger(SW_PROFILE_TERMINAL_FOLLOW_MODEL, swGetPreferences().mTerminalFollowModel);

		if (swGetPreferences().mTerminalFollowModel)
			UpdateTerminalDirectory(false);
		break;

	case SW_VIEW_TERMINAL_GO_TO_MODEL_FOLDER:
		UpdateTerminalDirectory(true);
		break;

	case SW_VIEW_TERMINAL_OPEN_MODEL_FOLDER:
		if (!swGetActiveProject()->GetFileName().isEmpty())
			QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(swGetActiveProject()->GetFileName()).absolutePath()));
		break;

	case SW_VIEW_COLORS_TOGGLE_SOLID:
		mColorList->ToggleGroupCollapsed(SW_COLOR_GROUP_SOLID);
		break;

	case SW_VIEW_COLORS_TOGGLE_TRANSLUCENT:
		mColorList->ToggleGroupCollapsed(SW_COLOR_GROUP_TRANSLUCENT);
		break;

	case SW_VIEW_COLORS_TOGGLE_SPECIAL:
		mColorList->ToggleGroupCollapsed(SW_COLOR_GROUP_SPECIAL);
		break;

	case SW_PIECE_INSERT_COLOR_CURRENT:
		SetInsertColorMode(swInsertColorMode::CurrentColor);
		break;

	case SW_PIECE_INSERT_COLOR_SELECTED:
		SetInsertColorMode(swInsertColorMode::SelectedPieceColor);
		break;

	case SW_PIECE_INSERT_COLOR_LAST_INSERTED:
		SetInsertColorMode(swInsertColorMode::LastInsertedColor);
		break;

	case SW_VIEW_PIECES_TOGGLE_LOCK_COLOR:
		if (mPartSelectionWidget)
		{
			mPartSelectionWidget->ToggleColorLocked();
			UpdatePiecesColor();
		}
		break;

	case SW_FILE_TOGGLE_OPEN_MOST_RECENT:
		swTogglePreference(swGetPreferences().mAutoLoadMostRecent, SW_PROFILE_AUTO_LOAD_MOST_RECENT);
		break;

	case SW_FILE_TOGGLE_RESTORE_TAB_LAYOUT:
		swTogglePreference(swGetPreferences().mRestoreTabLayout, SW_PROFILE_RESTORE_TAB_LAYOUT);
		break;

	case SW_EDIT_TOGGLE_FIXED_DIRECTION_KEYS:
		swTogglePreference(swGetPreferences().mFixedAxes, SW_PROFILE_FIXED_AXES);
		break;

	case SW_VIEW_TOGGLE_MESH_LOD:
		swTogglePreference(swGetPreferences().mAllowLOD, SW_PROFILE_ALLOW_LOD);
		break;

	case SW_VIEW_TOGGLE_HIGHLIGHT_NEW_PIECES:
		swTogglePreference(swGetPreferences().mHighlightNewParts, SW_PROFILE_HIGHLIGHT_NEW_PARTS);
		break;

	case SW_VIEW_TOGGLE_GRID_STUDS:
		swTogglePreference(swGetPreferences().mDrawGridStuds, SW_PROFILE_GRID_STUDS);
		break;

	case SW_VIEW_TOGGLE_GRID_LINES:
		swTogglePreference(swGetPreferences().mDrawGridLines, SW_PROFILE_GRID_LINES);
		break;

	case SW_VIEW_TOGGLE_GRID_ORIGIN:
		swTogglePreference(swGetPreferences().mDrawGridOrigin, SW_PROFILE_GRID_ORIGIN);
		break;

	case SW_VIEW_TOGGLE_AUTOMATE_EDGE_COLOR:
		ToggleAutomateEdgeColor();
		break;

	case SW_VIEW_HIGHLIGHT_STYLE_OUTLINE:
		SetHighlightRenderMode(swHighlightRenderMode::Outline);
		break;

	case SW_VIEW_HIGHLIGHT_STYLE_TINT:
		SetHighlightRenderMode(swHighlightRenderMode::Tint);
		break;

	case SW_VIEW_SELECTION_OUTLINE_WIDTH_INCREASE:
		SetSelectionOutlineWidth(swGetPreferences().mSelectionOutlineWidth + 1);
		break;

	case SW_VIEW_SELECTION_OUTLINE_WIDTH_DECREASE:
		SetSelectionOutlineWidth(swGetPreferences().mSelectionOutlineWidth - 1);
		break;

	case SW_VIEW_STUD_STYLE_PLAIN:
		SetStudStyle(swStudStyle::Plain);
		break;

	case SW_VIEW_STUD_STYLE_THIN_LINES_LOGO:
		SetStudStyle(swStudStyle::ThinLinesLogo);
		break;

	case SW_VIEW_STUD_STYLE_OUTLINE_LOGO:
		SetStudStyle(swStudStyle::OutlineLogo);
		break;

	case SW_VIEW_STUD_STYLE_SHARP_TOP_LOGO:
		SetStudStyle(swStudStyle::SharpTopLogo);
		break;

	case SW_VIEW_STUD_STYLE_ROUNDED_TOP_LOGO:
		SetStudStyle(swStudStyle::RoundedTopLogo);
		break;

	case SW_VIEW_STUD_STYLE_FLATTENED_LOGO:
		SetStudStyle(swStudStyle::FlattenedLogo);
		break;

	case SW_VIEW_STUD_STYLE_HIGH_CONTRAST:
		SetStudStyle(swStudStyle::HighContrast);
		break;

	case SW_VIEW_STUD_STYLE_HIGH_CONTRAST_LOGO:
		SetStudStyle(swStudStyle::HighContrastLogo);
		break;

	case SW_VIEW_TOGGLE_EDGE_LINES:
		ToggleEdgeLines(swRenderTarget::ModelView);
		break;

	case SW_VIEW_TOGGLE_CONDITIONAL_LINES:
		ToggleConditionalLines(swRenderTarget::ModelView);
		break;

	case SW_VIEW_TOGGLE_PREVIEW_SYNCHRONIZED_RENDERING:
		ToggleSyncRendering(swRenderTarget::Preview);
		break;

	case SW_VIEW_PREVIEW_SHADING_WIREFRAME:
		SetTargetShadingMode(swRenderTarget::Preview, swShadingMode::Wireframe);
		break;

	case SW_VIEW_PREVIEW_SHADING_FLAT:
		SetTargetShadingMode(swRenderTarget::Preview, swShadingMode::Flat);
		break;

	case SW_VIEW_PREVIEW_SHADING_DEFAULT_LIGHTS:
		SetTargetShadingMode(swRenderTarget::Preview, swShadingMode::DefaultLights);
		break;

	case SW_VIEW_TOGGLE_PREVIEW_EDGE_LINES:
		ToggleEdgeLines(swRenderTarget::Preview);
		break;

	case SW_VIEW_TOGGLE_PREVIEW_CONDITIONAL_LINES:
		ToggleConditionalLines(swRenderTarget::Preview);
		break;

	case SW_VIEW_TOGGLE_PIECES_SYNCHRONIZED_RENDERING:
		ToggleSyncRendering(swRenderTarget::Pieces);
		break;

	case SW_VIEW_TOGGLE_MINIFIGURE_BUILDER_SYNCHRONIZED_RENDERING:
		ToggleSyncRendering(swRenderTarget::MinifigureBuilder);
		break;

	case SW_VIEW_MINIFIGURE_BUILDER_SHADING_WIREFRAME:
		SetTargetShadingMode(swRenderTarget::MinifigureBuilder, swShadingMode::Wireframe);
		break;

	case SW_VIEW_MINIFIGURE_BUILDER_SHADING_FLAT:
		SetTargetShadingMode(swRenderTarget::MinifigureBuilder, swShadingMode::Flat);
		break;

	case SW_VIEW_MINIFIGURE_BUILDER_SHADING_DEFAULT_LIGHTS:
		SetTargetShadingMode(swRenderTarget::MinifigureBuilder, swShadingMode::DefaultLights);
		break;

	case SW_VIEW_TOGGLE_MINIFIGURE_BUILDER_EDGE_LINES:
		ToggleEdgeLines(swRenderTarget::MinifigureBuilder);
		break;

	case SW_VIEW_TOGGLE_MINIFIGURE_BUILDER_CONDITIONAL_LINES:
		ToggleConditionalLines(swRenderTarget::MinifigureBuilder);
		break;

	case SW_VIEW_PIECES_TOGGLE_SNAP_TO_TILES:
		swGetPreferences().mPiecesSnapToTiles = !swGetPreferences().mPiecesSnapToTiles;
		swSetProfileInteger(SW_PROFILE_PIECES_SNAP_TO_TILES, swGetPreferences().mPiecesSnapToTiles);
		SnapPartsDock();
		break;

	case SW_VIEW_PIECES_SHADING_WIREFRAME:
		SetTargetShadingMode(swRenderTarget::Pieces, swShadingMode::Wireframe);
		break;

	case SW_VIEW_PIECES_SHADING_FLAT:
		SetTargetShadingMode(swRenderTarget::Pieces, swShadingMode::Flat);
		break;

	case SW_VIEW_PIECES_SHADING_DEFAULT_LIGHTS:
		SetTargetShadingMode(swRenderTarget::Pieces, swShadingMode::DefaultLights);
		break;

	case SW_VIEW_TOGGLE_PIECES_EDGE_LINES:
		ToggleEdgeLines(swRenderTarget::Pieces);
		break;

	case SW_VIEW_TOGGLE_PIECES_CONDITIONAL_LINES:
		ToggleConditionalLines(swRenderTarget::Pieces);
		break;

	case SW_VIEW_TOGGLE_PREVIEW_VIEW_SPHERE:
		TogglePreviewViewSphere();
		break;

	case SW_VIEW_TOGGLE_PREVIEW_AXIS_ICON:
		TogglePreviewAxisIcon();
		break;

	case SW_FILE_TOGGLE_LOAD_PIECES_BEFORE_SHOWING:
		ToggleLoadPiecesBeforeShowing();
		break;

	case SW_VIEW_PREVIEW_TOGGLE_SQUARE:
		if (mPreviewWidget)
			mPreviewWidget->SetSquareViewport(!swGetPreferences().mPreviewSquareViewport);
		break;

	case SW_PIECE_INSERT:
		if (ActiveModel)
			ActiveModel->AddPiece(nullptr, SW_PIECE_SECTION_INVALID);
		break;

	case SW_PIECE_TOGGLE_FIRST_AT_ORIGIN:
		ToggleFirstPieceAtOrigin();
		break;

	case SW_PIECE_DELETE:
		if (ActiveModel)
			ActiveModel->DeleteSelectedObjects();
		break;

	case SW_PIECE_DUPLICATE:
		if (ActiveModel)
			ActiveModel->DuplicateSelectedPieces();
		break;

	case SW_PIECE_PAINT_SELECTED:
		if (ActiveModel)
			ActiveModel->PaintSelectedPieces();
		break;

	case SW_PIECE_RESET_PIVOT_POINT:
		if (ActiveModel)
			ActiveModel->ResetSelectedPiecesPivotPoint();
		break;

	case SW_PIECE_REMOVE_KEY_FRAMES:
		if (ActiveModel)
			ActiveModel->RemoveSelectedObjectsKeyFrames();
		break;

	case SW_PIECE_CONTROL_POINT_INSERT:
		if (ActiveModel)
			ActiveModel->InsertControlPoint();
		break;

	case SW_PIECE_CONTROL_POINT_REMOVE:
		if (ActiveModel)
			ActiveModel->RemoveFocusedControlPoint();
		break;

	case SW_PIECE_TRAIN_TRACK_FOCUS_NEXT:
		if (ActiveModel)
			ActiveModel->FocusNextTrainTrack();
		break;

	case SW_PIECE_TRAIN_TRACK_FOCUS_PREVIOUS:
		if (ActiveModel)
			ActiveModel->FocusPreviousTrainTrack();
		break;

	case SW_PIECE_TRAIN_TRACK_ROTATE:
		if (ActiveModel)
			ActiveModel->RotateFocusedTrainTrack(1);
		break;

	case SW_PIECE_MOVE_PLUS_X:
		if (ActiveModel)
			ActiveModel->MoveSelectedObjects(ActiveView->GetMoveDirection(swVector3(swMaximum(GetMoveXYSnap(), 0.1f), 0.0f, 0.0f)), swModelTransformFlag::Relative | swModelTransformFlag::Checkpoint | swModelTransformFlag::FirstMove, swModelHistoryEditMerge::KeyboardMove);
		break;

	case SW_PIECE_MOVE_MINUS_X:
		if (ActiveModel)
			ActiveModel->MoveSelectedObjects(ActiveView->GetMoveDirection(swVector3(-swMaximum(GetMoveXYSnap(), 0.1f), 0.0f, 0.0f)), swModelTransformFlag::Relative | swModelTransformFlag::Checkpoint | swModelTransformFlag::FirstMove, swModelHistoryEditMerge::KeyboardMove);
		break;

	case SW_PIECE_MOVE_PLUS_Y:
		if (ActiveModel)
			ActiveModel->MoveSelectedObjects(ActiveView->GetMoveDirection(swVector3(0.0f, swMaximum(GetMoveXYSnap(), 0.1f), 0.0f)), swModelTransformFlag::Relative | swModelTransformFlag::Checkpoint | swModelTransformFlag::FirstMove, swModelHistoryEditMerge::KeyboardMove);
		break;

	case SW_PIECE_MOVE_MINUS_Y:
		if (ActiveModel)
			ActiveModel->MoveSelectedObjects(ActiveView->GetMoveDirection(swVector3(0.0f, -swMaximum(GetMoveXYSnap(), 0.1f), 0.0f)), swModelTransformFlag::Relative | swModelTransformFlag::Checkpoint | swModelTransformFlag::FirstMove, swModelHistoryEditMerge::KeyboardMove);
		break;

	case SW_PIECE_MOVE_PLUS_Z:
		if (ActiveModel)
			ActiveModel->MoveSelectedObjects(ActiveView->GetMoveDirection(swVector3(0.0f, 0.0f, swMaximum(GetMoveZSnap(), 0.1f))), swModelTransformFlag::Relative | swModelTransformFlag::Checkpoint | swModelTransformFlag::FirstMove, swModelHistoryEditMerge::KeyboardMove);
		break;

	case SW_PIECE_MOVE_MINUS_Z:
		if (ActiveModel)
			ActiveModel->MoveSelectedObjects(ActiveView->GetMoveDirection(swVector3(0.0f, 0.0f, -swMaximum(GetMoveZSnap(), 0.1f))), swModelTransformFlag::Relative | swModelTransformFlag::Checkpoint | swModelTransformFlag::FirstMove, swModelHistoryEditMerge::KeyboardMove);
		break;

	case SW_PIECE_ROTATE_PLUS_X:
		if (ActiveModel)
			ActiveModel->RotateSelectedObjects(ActiveView->GetMoveDirection(swVector3(swMaximum(GetAngleSnap(), 1.0f), 0.0f, 0.0f)), swModelTransformFlag::Relative | swModelTransformFlag::Checkpoint, swModelHistoryEditMerge::KeyboardRotate);
		break;

	case SW_PIECE_ROTATE_MINUS_X:
		if (ActiveModel)
			ActiveModel->RotateSelectedObjects(ActiveView->GetMoveDirection(swVector3(-swMaximum(GetAngleSnap(), 1.0f), 0.0f, 0.0f)), swModelTransformFlag::Relative | swModelTransformFlag::Checkpoint, swModelHistoryEditMerge::KeyboardRotate);
		break;

	case SW_PIECE_ROTATE_PLUS_Y:
		if (ActiveModel)
			ActiveModel->RotateSelectedObjects(ActiveView->GetMoveDirection(swVector3(0.0f, swMaximum(GetAngleSnap(), 1.0f), 0.0f)), swModelTransformFlag::Relative | swModelTransformFlag::Checkpoint, swModelHistoryEditMerge::KeyboardRotate);
		break;

	case SW_PIECE_ROTATE_MINUS_Y:
		if (ActiveModel)
			ActiveModel->RotateSelectedObjects(ActiveView->GetMoveDirection(swVector3(0.0f, -swMaximum(GetAngleSnap(), 1.0f), 0.0f)), swModelTransformFlag::Relative | swModelTransformFlag::Checkpoint, swModelHistoryEditMerge::KeyboardRotate);
		break;

	case SW_PIECE_ROTATE_PLUS_Z:
		if (ActiveModel)
			ActiveModel->RotateSelectedObjects(ActiveView->GetMoveDirection(swVector3(0.0f, 0.0f, swMaximum(GetAngleSnap(), 1.0f))), swModelTransformFlag::Relative | swModelTransformFlag::Checkpoint, swModelHistoryEditMerge::KeyboardRotate);
		break;

	case SW_PIECE_ROTATE_MINUS_Z:
		if (ActiveModel)
			ActiveModel->RotateSelectedObjects(ActiveView->GetMoveDirection(swVector3(0.0f, 0.0f, -swMaximum(GetAngleSnap(), 1.0f))), swModelTransformFlag::Relative | swModelTransformFlag::Checkpoint, swModelHistoryEditMerge::KeyboardRotate);
		break;

	case SW_PIECE_MINIFIGURE_BUILDER:
		mMinifigureBuilderToolBar->show();
		mMinifigureBuilderToolBar->raise();
		break;

	case SW_PIECE_ARRAY:
		if (ActiveModel)
			ActiveModel->ShowArrayDialog();
		break;

	case SW_PIECE_VIEW_SELECTED_MODEL:
		SetModelFromSelection();
		break;

	case SW_PIECE_MOVE_SELECTION_TO_MODEL:
		if (ActiveModel)
			ActiveModel->MoveSelectionToModel(swGetActiveProject()->CreateNewModel(false));
		break;

	case SW_PIECE_INLINE_SELECTED_MODELS:
		if (ActiveModel)
			ActiveModel->InlineSelectedModels();
		break;

	case SW_PIECE_EDIT_END_SUBMODEL:
		if (ActiveView)
			ActiveView->SetTopSubmodelActive();
		break;

	case SW_PIECE_EDIT_SELECTED_SUBMODEL:
		if (ActiveView)
			ActiveView->SetSelectedSubmodelActive();
		break;

	case SW_PIECE_GROUP:
		if (ActiveModel)
			ActiveModel->GroupSelection();
		break;

	case SW_PIECE_UNGROUP:
		if (ActiveModel)
			ActiveModel->UngroupSelection();
		break;

	case SW_PIECE_GROUP_ADD:
		if (ActiveModel)
			ActiveModel->AddSelectedPiecesToGroup();
		break;

	case SW_PIECE_GROUP_REMOVE:
		if (ActiveModel)
			ActiveModel->RemoveFocusPieceFromGroup();
		break;

	case SW_PIECE_GROUP_EDIT:
		if (ActiveModel)
			ActiveModel->ShowEditGroupsDialog();
		break;

	case SW_PIECE_HIDE_SELECTED:
		if (ActiveModel)
			ActiveModel->HideSelectedPieces();
		break;

	case SW_PIECE_HIDE_UNSELECTED:
		if (ActiveModel)
			ActiveModel->HideUnselectedPieces();
		break;

	case SW_PIECE_UNHIDE_SELECTED:
		if (ActiveModel)
			ActiveModel->UnhideSelectedPieces();
		break;

	case SW_PIECE_UNHIDE_ALL:
		if (ActiveModel)
			ActiveModel->UnhideAllPieces();
		break;

	case SW_PIECE_SHOW_EARLIER:
		if (ActiveModel)
			ActiveModel->ShowSelectedPiecesEarlier();
		break;

	case SW_PIECE_SHOW_LATER:
		if (ActiveModel)
			ActiveModel->ShowSelectedPiecesLater();
		break;

	case SW_VIEW_PREFERENCES:
		gApplication->ShowPreferencesDialog();
		break;

	case SW_VIEW_ZOOM_IN:
		if (ActiveView)
			ActiveView->Zoom(10.0f);
		break;

	case SW_VIEW_ZOOM_OUT:
		if (ActiveView)
			ActiveView->Zoom(-10.0f);
		break;

	case SW_VIEW_ZOOM_EXTENTS:
		if (ActiveView)
			ActiveView->ZoomExtents(swGeometryBoundsMode::IncludeFallback, false);
		break;

	case SW_VIEW_LOOK_AT:
		if (ActiveView)
			ActiveView->LookAt();
		break;

	case SW_VIEW_MOVE_FORWARD:
		if (ActiveView)
			ActiveView->MoveCamera(swVector3(0.0f, 0.0f, -1.0f));
		break;

	case SW_VIEW_MOVE_BACKWARD:
		if (ActiveView)
			ActiveView->MoveCamera(swVector3(0.0f, 0.0f, 1.0f));
		break;

	case SW_VIEW_MOVE_LEFT:
		if (ActiveView)
			ActiveView->MoveCamera(swVector3(-1.0f, 0.0f, 0.0f));
		break;

	case SW_VIEW_MOVE_RIGHT:
		if (ActiveView)
			ActiveView->MoveCamera(swVector3(1.0f, 0.0f, 0.0f));
		break;

	case SW_VIEW_MOVE_UP:
		if (ActiveView)
			ActiveView->MoveCamera(swVector3(0.0f, 1.0f, 0.0f));
		break;

	case SW_VIEW_MOVE_DOWN:
		if (ActiveView)
			ActiveView->MoveCamera(swVector3(0.0f, -1.0f, 0.0f));
		break;

	case SW_VIEW_TIME_NEXT:
		if (ActiveModel)
			ActiveModel->ShowNextStep();
		break;

	case SW_VIEW_TIME_PREVIOUS:
		if (ActiveModel)
			ActiveModel->ShowPreviousStep();
		break;

	case SW_VIEW_TIME_FIRST:
		if (ActiveModel)
			ActiveModel->ShowFirstStep();
		break;

	case SW_VIEW_TIME_LAST:
		if (ActiveModel)
			ActiveModel->ShowLastStep();
		break;

	case SW_VIEW_TIME_INSERT_BEFORE:
		if (ActiveModel)
			ActiveModel->InsertStepAction(ActiveModel->GetCurrentStep());
		break;

	case SW_VIEW_TIME_INSERT_AFTER:
		if (ActiveModel)
			ActiveModel->InsertStepAction(ActiveModel->GetCurrentStep() + 1);
		break;

	case SW_VIEW_TIME_DELETE:
		if (ActiveModel)
			ActiveModel->RemoveStepAction(ActiveModel->GetCurrentStep());
		break;

	case SW_VIEW_VIEWPOINT_FRONT:
		if (ActiveView)
			ActiveView->SetViewpoint(swViewpoint::Front);
		break;

	case SW_VIEW_VIEWPOINT_BACK:
		if (ActiveView)
			ActiveView->SetViewpoint(swViewpoint::Back);
		break;

	case SW_VIEW_VIEWPOINT_TOP:
		if (ActiveView)
			ActiveView->SetViewpoint(swViewpoint::Top);
		break;

	case SW_VIEW_VIEWPOINT_BOTTOM:
		if (ActiveView)
			ActiveView->SetViewpoint(swViewpoint::Bottom);
		break;

	case SW_VIEW_VIEWPOINT_LEFT:
		if (ActiveView)
			ActiveView->SetViewpoint(swViewpoint::Left);
		break;

	case SW_VIEW_VIEWPOINT_RIGHT:
		if (ActiveView)
			ActiveView->SetViewpoint(swViewpoint::Right);
		break;

	case SW_VIEW_VIEWPOINT_HOME:
		if (ActiveView)
			ActiveView->SetViewpoint(swViewpoint::Home);
		break;

	case SW_VIEW_CAMERA_NONE:
		if (ActiveView)
			ActiveView->RemoveCamera();
		break;

	case SW_VIEW_CAMERA1:
	case SW_VIEW_CAMERA2:
	case SW_VIEW_CAMERA3:
	case SW_VIEW_CAMERA4:
	case SW_VIEW_CAMERA5:
	case SW_VIEW_CAMERA6:
	case SW_VIEW_CAMERA7:
	case SW_VIEW_CAMERA8:
	case SW_VIEW_CAMERA9:
	case SW_VIEW_CAMERA10:
	case SW_VIEW_CAMERA11:
	case SW_VIEW_CAMERA12:
	case SW_VIEW_CAMERA13:
	case SW_VIEW_CAMERA14:
	case SW_VIEW_CAMERA15:
	case SW_VIEW_CAMERA16:
		if (ActiveView)
			ActiveView->SetCameraIndex(CommandId - SW_VIEW_CAMERA1);
		break;

	case SW_MODEL_NEW:
		swGetActiveProject()->CreateNewModel(true);
		break;

	case SW_MODEL_PROPERTIES:
		if (ActiveModel)
			ActiveModel->ShowPropertiesDialog();
		break;

	case SW_MODEL_LIST:
		swGetActiveProject()->ShowModelListDialog();
		break;

	case SW_MODEL_01:
	case SW_MODEL_02:
	case SW_MODEL_03:
	case SW_MODEL_04:
	case SW_MODEL_05:
	case SW_MODEL_06:
	case SW_MODEL_07:
	case SW_MODEL_08:
	case SW_MODEL_09:
	case SW_MODEL_10:
	case SW_MODEL_11:
	case SW_MODEL_12:
	case SW_MODEL_13:
	case SW_MODEL_14:
	case SW_MODEL_15:
	case SW_MODEL_16:
	case SW_MODEL_17:
	case SW_MODEL_18:
	case SW_MODEL_19:
	case SW_MODEL_20:
	case SW_MODEL_21:
	case SW_MODEL_22:
	case SW_MODEL_23:
	case SW_MODEL_24:
	case SW_MODEL_25:
	case SW_MODEL_26:
	case SW_MODEL_27:
	case SW_MODEL_28:
	case SW_MODEL_29:
	case SW_MODEL_30:
	case SW_MODEL_31:
	case SW_MODEL_32:
	case SW_MODEL_33:
	case SW_MODEL_34:
	case SW_MODEL_35:
	case SW_MODEL_36:
	case SW_MODEL_37:
	case SW_MODEL_38:
	case SW_MODEL_39:
	case SW_MODEL_40:
		swGetActiveProject()->SetActiveModel(CommandId - SW_MODEL_01, true);
		break;

	case SW_HELP_WHATS_THIS:
		QWhatsThis::enterWhatsThisMode();
		break;

	case SW_HELP_HOME_PAGE:
		QDesktopServices::openUrl(QUrl("https://github.com/nathaneltitane/studworks"));
		break;

	case SW_HELP_BUG_REPORT:
		QDesktopServices::openUrl(QUrl("https://github.com/nathaneltitane/studworks/issues"));
		break;

	case SW_HELP_ABOUT:
		ShowAboutDialog();
		break;

	case SW_VIEW_TIME_ADD_KEYS:
		SetAddKeys(!GetAddKeys());
		break;

	case SW_EDIT_TRANSFORM_RELATIVE:
		SetRelativeTransform(true);
		break;

	case SW_EDIT_TRANSFORM_ABSOLUTE:
		SetRelativeTransform(false);
		break;

	case SW_EDIT_TRANSFORM_TOGGLE_RELATIVE:
		SetRelativeTransform(!GetRelativeTransform());
		break;

	case SW_EDIT_TRANSFORM_SEPARATELY:
		SetSeparateTransform(true);
		break;

	case SW_EDIT_TRANSFORM_TOGETHER:
		SetSeparateTransform(false);
		break;

	case SW_EDIT_TRANSFORM_TOGGLE_SEPARATE:
		SetSeparateTransform(!GetSeparateTransform());
		break;

	case SW_EDIT_SNAP_MOVE_TOGGLE:
		SetMoveSnapEnabled(!mMoveSnapEnabled);
		break;

	case SW_EDIT_SNAP_MOVE_XY0:
	case SW_EDIT_SNAP_MOVE_XY1:
	case SW_EDIT_SNAP_MOVE_XY2:
	case SW_EDIT_SNAP_MOVE_XY3:
	case SW_EDIT_SNAP_MOVE_XY4:
	case SW_EDIT_SNAP_MOVE_XY5:
	case SW_EDIT_SNAP_MOVE_XY6:
	case SW_EDIT_SNAP_MOVE_XY7:
	case SW_EDIT_SNAP_MOVE_XY8:
	case SW_EDIT_SNAP_MOVE_XY9:
		SetMoveXYSnapIndex(CommandId - SW_EDIT_SNAP_MOVE_XY0);
		break;

	case SW_EDIT_SNAP_MOVE_Z0:
	case SW_EDIT_SNAP_MOVE_Z1:
	case SW_EDIT_SNAP_MOVE_Z2:
	case SW_EDIT_SNAP_MOVE_Z3:
	case SW_EDIT_SNAP_MOVE_Z4:
	case SW_EDIT_SNAP_MOVE_Z5:
	case SW_EDIT_SNAP_MOVE_Z6:
	case SW_EDIT_SNAP_MOVE_Z7:
	case SW_EDIT_SNAP_MOVE_Z8:
	case SW_EDIT_SNAP_MOVE_Z9:
		SetMoveZSnapIndex(CommandId - SW_EDIT_SNAP_MOVE_Z0);
		break;

	case SW_EDIT_SNAP_ANGLE_TOGGLE:
		SetAngleSnapEnabled(!mAngleSnapEnabled);
		break;

	case SW_EDIT_SNAP_ANGLE0:
	case SW_EDIT_SNAP_ANGLE1:
	case SW_EDIT_SNAP_ANGLE2:
	case SW_EDIT_SNAP_ANGLE3:
	case SW_EDIT_SNAP_ANGLE4:
	case SW_EDIT_SNAP_ANGLE5:
	case SW_EDIT_SNAP_ANGLE6:
	case SW_EDIT_SNAP_ANGLE7:
	case SW_EDIT_SNAP_ANGLE8:
	case SW_EDIT_SNAP_ANGLE9:
		SetAngleSnapIndex(CommandId - SW_EDIT_SNAP_ANGLE0);
		break;

	case SW_EDIT_SNAP_ANGLE_CUSTOM:
		SetCustomAngleSnap();
		break;

	case SW_EDIT_SNAP_ORBIT_TOGGLE:
		mOrbitSnapEnabled = !mOrbitSnapEnabled;
		swSetProfileInteger(SW_PROFILE_ORBIT_SNAP, mOrbitSnapEnabled);
		UpdateSnap();
		break;

	case SW_EDIT_TRANSFORM:
		if (ActiveModel)
		{
			const std::optional<swVector3> Transform = GetTransformAmount();

			if (Transform)
				ActiveModel->TransformSelectedObjects(GetTransformType(), *Transform);
		}
		break;

	case SW_EDIT_TRANSFORM_ABSOLUTE_TRANSLATION:
		SetTransformType(swTransformType::AbsoluteTranslation);
		break;

	case SW_EDIT_TRANSFORM_RELATIVE_TRANSLATION:
		SetTransformType(swTransformType::RelativeTranslation);
		break;

	case SW_EDIT_TRANSFORM_ABSOLUTE_ROTATION:
		SetTransformType(swTransformType::AbsoluteRotation);
		break;

	case SW_EDIT_TRANSFORM_RELATIVE_ROTATION:
		SetTransformType(swTransformType::RelativeRotation);
		break;

	case SW_EDIT_ACTION_SELECT:
		SetTool(swTool::Select);
		break;

	case SW_EDIT_ACTION_INSERT:
		SetTool(swTool::Insert);
		break;

	case SW_EDIT_ACTION_POINT_LIGHT:
		SetTool(swTool::PointLight);
		break;

	case SW_EDIT_ACTION_AREA_LIGHT:
		SetTool(swTool::AreaLight);
		break;

	case SW_EDIT_ACTION_DIRECTIONAL_LIGHT:
		SetTool(swTool::DirectionalLight);
		break;

	case SW_EDIT_ACTION_SPOTLIGHT:
		SetTool(swTool::SpotLight);
		break;

	case SW_EDIT_ACTION_CAMERA:
		SetTool(swTool::Camera);
		break;

	case SW_EDIT_ACTION_MOVE:
		SetTool(swTool::Move);
		break;

	case SW_EDIT_ACTION_ROTATE:
		SetTool(swTool::Rotate);
		break;

	case SW_EDIT_ACTION_DELETE:
		SetTool(swTool::Eraser);
		break;

	case SW_EDIT_ACTION_PAINT:
		SetTool(swTool::Paint);
		break;

	case SW_EDIT_ACTION_COLOR_PICKER:
		SetTool(swTool::ColorPicker);
		break;

	case SW_EDIT_ACTION_ZOOM:
		SetTool(swTool::Zoom);
		break;

	case SW_EDIT_ACTION_ZOOM_REGION:
		SetTool(swTool::ZoomRegion);
		break;

	case SW_EDIT_ACTION_PAN:
		SetTool(swTool::Pan);
		break;

	case SW_EDIT_ACTION_ROTATE_VIEW:
		SetTool(swTool::RotateView);
		break;

	case SW_EDIT_ACTION_ROLL:
		SetTool(swTool::Roll);
		break;

	case SW_EDIT_CANCEL:
		if (ActiveView && !ActiveView->CloseFindReplaceDialog())
			ActiveView->CancelTrackingOrClearSelectionAction();
		break;

	case SW_TIMELINE_INSERT_BEFORE:
		mTimelineWidget->InsertStepBefore();
		break;

	case SW_TIMELINE_INSERT_AFTER:
		mTimelineWidget->InsertStepAfter();
		break;

	case SW_TIMELINE_DELETE:
		mTimelineWidget->RemoveStep();
		break;

	case SW_TIMELINE_MOVE_SELECTION:
		mTimelineWidget->MoveSelection();
		break;

	case SW_TIMELINE_MOVE_SELECTION_BEFORE:
		mTimelineWidget->MoveSelectionBefore();
		break;

	case SW_TIMELINE_MOVE_SELECTION_AFTER:
		mTimelineWidget->MoveSelectionAfter();
		break;

	case SW_TIMELINE_SET_CURRENT:
		mTimelineWidget->SetCurrentStep();
		break;

	case SW_COMMAND_COUNT:
		break;
	}
}
