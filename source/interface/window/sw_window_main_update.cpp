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
#include "sw_widget_label_elided.h"
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

void swMainWindow::UpdateSelectedObjects(bool SelectionChanged)
{
	int Flags = 0;
	std::vector<swObject*> Selection;
	swObject* Focus = nullptr;

	swModel* ActiveModel = GetActiveModel();
	if (ActiveModel)
		ActiveModel->GetSelectionInformation(&Flags, Selection, &Focus);

	if (SelectionChanged)
	{
		mTimelineWidget->UpdateSelection();

		// Selecting a piece makes its color current; picking a color afterwards overrides it.
		if (swGetPreferences().mInsertColorMode == swInsertColorMode::SelectedPieceColor)
		{
			swObject* SelectedPiece = (Focus && Focus->IsPiece()) ? Focus : nullptr;

			for (size_t ObjectIndex = 0; !SelectedPiece && ObjectIndex < Selection.size(); ObjectIndex++)
				if (Selection[ObjectIndex]->IsPiece())
					SelectedPiece = Selection[ObjectIndex];

			if (SelectedPiece && static_cast<swPiece*>(SelectedPiece)->GetColorIndex() != mColorIndex)
				SetColorIndex(static_cast<swPiece*>(SelectedPiece)->GetColorIndex());
		}

		mActions[SW_EDIT_CUT]->setEnabled(Flags & SW_SELECTION_SELECTED);
		mActions[SW_EDIT_COPY]->setEnabled(Flags & SW_SELECTION_SELECTED);
		mActions[SW_EDIT_FIND]->setEnabled((Flags & SW_SELECTION_NO_PIECES) == 0);
		mActions[SW_EDIT_FIND_NEXT]->setEnabled((Flags & SW_SELECTION_NO_PIECES) == 0);
		mActions[SW_EDIT_FIND_PREVIOUS]->setEnabled((Flags & SW_SELECTION_NO_PIECES) == 0);
		mActions[SW_EDIT_FIND_ALL]->setEnabled((Flags & SW_SELECTION_NO_PIECES) == 0);
		mActions[SW_EDIT_REPLACE]->setEnabled((Flags & SW_SELECTION_NO_PIECES) == 0);
		mActions[SW_EDIT_REPLACE_NEXT]->setEnabled((Flags & SW_SELECTION_NO_PIECES) == 0);
		mActions[SW_EDIT_REPLACE_ALL]->setEnabled((Flags & SW_SELECTION_NO_PIECES) == 0);
		mActions[SW_EDIT_SELECT_INVERT]->setEnabled((Flags & SW_SELECTION_NO_PIECES) == 0);
		mActions[SW_EDIT_SELECT_BY_NAME]->setEnabled((Flags & SW_SELECTION_NO_PIECES) == 0);
		mActions[SW_EDIT_SELECT_NONE]->setEnabled(Flags & SW_SELECTION_SELECTED);
		mActions[SW_EDIT_SELECT_ALL]->setEnabled(Flags & SW_SELECTION_UNSELECTED);

		mActions[SW_PIECE_DELETE]->setEnabled(Flags & SW_SELECTION_SELECTED);
		mActions[SW_PIECE_DUPLICATE]->setEnabled(Flags & SW_SELECTION_SELECTED);
		mActions[SW_PIECE_PAINT_SELECTED]->setEnabled(Flags & SW_SELECTION_PIECE);
		mActions[SW_PIECE_RESET_PIVOT_POINT]->setEnabled(Flags & SW_SELECTION_SELECTED);
		mActions[SW_PIECE_REMOVE_KEY_FRAMES]->setEnabled(Flags & SW_SELECTION_SELECTED);
		mActions[SW_PIECE_ARRAY]->setEnabled(Flags & SW_SELECTION_PIECE);
		mActions[SW_PIECE_CONTROL_POINT_INSERT]->setEnabled(Flags & SW_SELECTION_CAN_ADD_CONTROL_POINT);
		mActions[SW_PIECE_CONTROL_POINT_REMOVE]->setEnabled(Flags & SW_SELECTION_CAN_REMOVE_CONTROL_POINT);
		mActions[SW_PIECE_TRAIN_TRACK_FOCUS_NEXT]->setEnabled(Flags & SW_SELECTION_TRAIN_TRACK_VISIBLE);
		mActions[SW_PIECE_TRAIN_TRACK_FOCUS_PREVIOUS]->setEnabled(Flags & SW_SELECTION_TRAIN_TRACK_VISIBLE);
		mActions[SW_PIECE_TRAIN_TRACK_ROTATE]->setEnabled(Flags & SW_SELECTION_TRAIN_TRACK_VISIBLE);
		mActions[SW_PIECE_HIDE_SELECTED]->setEnabled(Flags & SW_SELECTION_VISIBLE_SELECTED);
		mActions[SW_PIECE_HIDE_UNSELECTED]->setEnabled(Flags & SW_SELECTION_UNSELECTED);
		mActions[SW_PIECE_UNHIDE_SELECTED]->setEnabled(Flags & SW_SELECTION_HIDDEN_SELECTED);
		mActions[SW_PIECE_UNHIDE_ALL]->setEnabled(Flags & SW_SELECTION_HIDDEN);
		mActions[SW_PIECE_VIEW_SELECTED_MODEL]->setEnabled(Flags & SW_SELECTION_MODEL_SELECTED);
		mActions[SW_PIECE_MOVE_SELECTION_TO_MODEL]->setEnabled(Flags & SW_SELECTION_PIECE);
		mActions[SW_PIECE_INLINE_SELECTED_MODELS]->setEnabled(Flags & SW_SELECTION_MODEL_SELECTED);
		mActions[SW_PIECE_EDIT_SELECTED_SUBMODEL]->setEnabled(Flags & SW_SELECTION_MODEL_SELECTED);
		mActions[SW_PIECE_GROUP]->setEnabled(Flags & SW_SELECTION_CAN_GROUP);
		mActions[SW_PIECE_UNGROUP]->setEnabled(Flags & SW_SELECTION_GROUPED);
		mActions[SW_PIECE_GROUP_ADD]->setEnabled((Flags & (SW_SELECTION_GROUPED | SW_SELECTION_FOCUS_GROUPED)) == SW_SELECTION_GROUPED);
		mActions[SW_PIECE_GROUP_REMOVE]->setEnabled(Flags & SW_SELECTION_FOCUS_GROUPED);
		mActions[SW_PIECE_GROUP_EDIT]->setEnabled((Flags & SW_SELECTION_NO_PIECES) == 0);
		mActions[SW_PIECE_SHOW_EARLIER]->setEnabled(Flags & SW_SELECTION_PIECE); // FIXME: disable if current step is 1
		mActions[SW_PIECE_SHOW_LATER]->setEnabled(Flags & SW_SELECTION_PIECE);
		mActions[SW_TIMELINE_MOVE_SELECTION]->setEnabled(Flags & SW_SELECTION_PIECE);
		mActions[SW_TIMELINE_MOVE_SELECTION_BEFORE]->setEnabled(Flags & SW_SELECTION_PIECE);
		mActions[SW_TIMELINE_MOVE_SELECTION_AFTER]->setEnabled(Flags & SW_SELECTION_PIECE);

		mActions[SW_PIECE_EDIT_END_SUBMODEL]->setEnabled(GetCurrentTabModel() != ActiveModel);
	}

	mPropertiesWidget->Update(Selection, Focus);

	if (Focus && Focus->IsPiece())
	{
		swPiece* Piece = (swPiece*)Focus;
		int ColorIndex = Piece->GetColorIndex();
		quint32 ColorCode = swGetColorCode(ColorIndex);

		PreviewPiece(Piece->mPieceData.Get(), ColorCode, false);
	}

	QString Message;

	if ((Selection.size() == 1) && Focus)
	{
		if (Focus->IsPiece())
			Message = tr("%1 (ID: %2)").arg(Focus->GetName(), ((swPiece*)Focus)->GetID());
		else
			Message = Focus->GetName();
	}
	else if (Selection.size() > 0)
	{
		Message = tr("%n objects selected", nullptr, static_cast<int>(Selection.size()));

		if (Focus && Focus->IsPiece())
		{
			Message.append(tr(" - %1 (ID: %2)").arg(Focus->GetName(), ((swPiece*)Focus)->GetID()));

			const swGroup* Group = ((swPiece*)Focus)->GetGroup();
			if (Group && !Group->mName.isEmpty())
				Message.append(tr(" in group '%1'").arg(Group->mName));
		}
	}

	mStatusBarLabel->setText(Message);

	swVector3 Position(0.0f, 0.0f, 0.0f);

	if (ActiveModel)
		ActiveModel->GetFocusPosition(Position);

	// Name the focused object so the coordinates are clearly its position.
	QString Object;

	if (Focus && Focus->IsPiece())
		Object = tr("Piece");
	else if (Focus && Focus->IsCamera())
		Object = tr("Camera");
	else if (Focus && Focus->IsLight())
		Object = tr("Light");
	else
		Object = tr("Origin");

	QString Label = tr("%1 X: %2 Y: %3 Z: %4");
	Label = Label.arg(Object, QLocale::system().toString(Position[0], 'f', 2), QLocale::system().toString(Position[1], 'f', 2), QLocale::system().toString(Position[2], 'f', 2));
	mStatusPositionLabel->setText(Label);
}

void swMainWindow::UpdateTimeline(bool Clear, bool UpdateItems)
{
	mTimelineWidget->Update(Clear, UpdateItems);

	emit ModelContentsChanged();
}

void swMainWindow::UpdatePaste(bool Enabled)
{
	if (mActions[SW_EDIT_PASTE])
		mActions[SW_EDIT_PASTE]->setEnabled(Enabled);

	if (mActions[SW_EDIT_PASTE_STEPS])
		mActions[SW_EDIT_PASTE_STEPS]->setEnabled(Enabled);
}

void swMainWindow::UpdateCurrentStep()
{
	swModel* ActiveModel = GetActiveModel();

	if (!ActiveModel)
		return;

	swStep CurrentStep = ActiveModel->GetCurrentStep();
	swStep LastStep = ActiveModel->GetLastStep();

	mActions[SW_VIEW_TIME_FIRST]->setEnabled(CurrentStep != 1);
	mActions[SW_VIEW_TIME_PREVIOUS]->setEnabled(CurrentStep > 1);
	mActions[SW_VIEW_TIME_NEXT]->setEnabled(CurrentStep < SW_STEP_MAXIMUM);
	mActions[SW_VIEW_TIME_LAST]->setEnabled(CurrentStep != LastStep);

	mStatusTimeLabel->setText(QString(tr("Step %1")).arg(swFormatIntegerLocalized(CurrentStep, locale())));
}

void swMainWindow::SetAddKeys(bool AddKeys)
{
	QAction* Action = mActions[SW_VIEW_TIME_ADD_KEYS];

	if (Action)
		Action->setChecked(AddKeys);

	mAddKeys = AddKeys;
}

void swMainWindow::UpdateLockSnap()
{
	if (GetRelativeTransform())
		mActions[SW_EDIT_TRANSFORM_RELATIVE]->setChecked(true);
	else
		mActions[SW_EDIT_TRANSFORM_ABSOLUTE]->setChecked(true);

	if (GetSeparateTransform())
		mActions[SW_EDIT_TRANSFORM_SEPARATELY]->setChecked(true);
	else
		mActions[SW_EDIT_TRANSFORM_TOGETHER]->setChecked(true);

	UpdateSnap();
}

void swMainWindow::UpdateSnap()
{
	mActions[SW_EDIT_SNAP_MOVE_TOGGLE]->setChecked(mMoveSnapEnabled);
	mActions[SW_EDIT_SNAP_ANGLE_TOGGLE]->setChecked(mAngleSnapEnabled);
	mActions[SW_EDIT_SNAP_MOVE_XY0 + mMoveXYSnapIndex]->setChecked(true);
	mActions[SW_EDIT_SNAP_MOVE_Z0 + mMoveZSnapIndex]->setChecked(true);
	mActions[SW_EDIT_SNAP_ANGLE0 + mAngleSnapIndex]->setChecked(true);
	mActions[SW_EDIT_SNAP_ORBIT_TOGGLE]->setChecked(mOrbitSnapEnabled);

	// Spell out the transform and snap settings from the toolbar, using the same names as their menus.
	auto MenuText = [this](int CommandId)
	{
		return mActions[CommandId]->text().remove(QLatin1Char('&'));
	};

	const bool MoveSnapped = mMoveSnapEnabled && (mMoveXYSnapIndex || mMoveZSnapIndex);
	const QString MoveXY = MoveSnapped ? MenuText(SW_EDIT_SNAP_MOVE_XY0 + mMoveXYSnapIndex) : tr("free");
	const QString MoveZ = MoveSnapped ? MenuText(SW_EDIT_SNAP_MOVE_Z0 + mMoveZSnapIndex) : tr("free");
	const QString Axes = mRelativeTransform ? tr("relative") : tr("absolute");
	const QString Angle = GetAngleSnap() != 0.0f ? tr("%1 deg.").arg(swFormatDoubleLocalized(GetAngleSnap(), locale(), 'g', 6)) : tr("free");
	const QString Pivot = GetSeparateTransform() ? tr("separately") : tr("together");

	mStatusSnapLabel->setText(tr("- Move XY: %1 Z: %2 - Axes: %3 - Rotate: %4 / %5 -").arg(MoveXY, MoveZ, Axes, Angle, Pivot));
	mStatusSnapLabel->setToolTip(tr("Movement snap, rotation snap, transform axes and rotation pivot. Change them with the last buttons of the standard toolbar."));
}

void swMainWindow::UpdateColor()
{
	QPixmap Pixmap(14, 14);
	Pixmap.fill(QColor::fromRgbF(gColorList[mColorIndex].Value[0], gColorList[mColorIndex].Value[1], gColorList[mColorIndex].Value[2]));

	mColorButton->setIcon(Pixmap);
	mColorButton->setText(QString("  ") + gColorList[mColorIndex].Name);

	mColorList->SetCurrentColor(mColorIndex);
}

void swMainWindow::UpdateUndoRedo(const QString& UndoText, const QString& RedoText)
{
	QAction* UndoAction = mActions[SW_EDIT_UNDO];
	QAction* RedoAction = mActions[SW_EDIT_REDO];

	if (!UndoText.isEmpty())
	{
		UndoAction->setEnabled(true);
		UndoAction->setText(QString(tr("&Undo %1")).arg(UndoText));
	}
	else
	{
		UndoAction->setEnabled(false);
		UndoAction->setText(tr("&Undo"));
	}

	if (!RedoText.isEmpty())
	{
		RedoAction->setEnabled(true);
		RedoAction->setText(QString(tr("&Redo %1")).arg(RedoText));
	}
	else
	{
		RedoAction->setEnabled(false);
		RedoAction->setText(tr("&Redo"));
	}
}

void swMainWindow::ViewFocusReceived()
{
	SetActiveView(qobject_cast<swView*>(sender()));
}

void swMainWindow::FileMenuAboutToShow()
{
	for (int ActionIdx = SW_FILE_RECENT_FIRST; ActionIdx <= SW_FILE_RECENT_LAST; ActionIdx++)
	{
		int FileIdx = ActionIdx - SW_FILE_RECENT_FIRST;
		QAction* Action = mActions[ActionIdx];

		if (!mRecentFiles[FileIdx].isEmpty())
		{
			Action->setText(QString("&%1 %2").arg(QString::number(FileIdx + 1), QDir::toNativeSeparators(mRecentFiles[FileIdx])));
			Action->setVisible(true);
		}
		else
			Action->setVisible(false);
	}

	mActionFileRecentSeparator->setVisible(!mRecentFiles[0].isEmpty());
}

void swMainWindow::CameraMenuAboutToShow()
{
	const std::vector<std::unique_ptr<swCamera>>& Cameras = swGetActiveModel()->GetCameras();
	swView* ActiveView = GetActiveView();
	const swCamera* CurrentCamera = ActiveView ? ActiveView->GetCamera() : nullptr;
	bool CurrentSet = false;

	for (size_t ActionIndex = SW_VIEW_CAMERA_FIRST; ActionIndex <= SW_VIEW_CAMERA_LAST; ActionIndex++)
	{
		QAction* Action = mActions[ActionIndex];
		size_t CameraIndex = ActionIndex - SW_VIEW_CAMERA_FIRST;

		if (CameraIndex < Cameras.size())
		{
			if (CurrentCamera == Cameras[CameraIndex].get())
			{
				Action->setChecked(true);
				CurrentSet = true;
			}

			Action->setText(Cameras[CameraIndex]->GetName());
			Action->setVisible(true);
		}
		else
			Action->setVisible(false);
	}

	if (!CurrentSet)
		mActions[SW_VIEW_CAMERA_NONE]->setChecked(true);
}

void swMainWindow::ProjectionMenuAboutToShow()
{
	swView* ActiveView = GetActiveView();

	if (ActiveView)
	{
		if (ActiveView->GetCamera()->GetProjection() == swCameraProjection::Orthographic)
			mActions[SW_VIEW_PROJECTION_ORTHOGRAPHIC]->setChecked(true);
		else
			mActions[SW_VIEW_PROJECTION_PERSPECTIVE]->setChecked(true);
	}
}

void swMainWindow::UpdateShadingMode()
{
	mActions[SW_VIEW_SHADING_FIRST + static_cast<int>(swGetPreferences().mShadingMode)]->setChecked(true);
	mActions[SW_VIEW_LIGHTING_FIRST + static_cast<int>(swGetPreferences().mLightingStyle)]->setChecked(true);
}

void swMainWindow::UpdateSelectionMode()
{
	switch (mSelectionMode)
	{
	case swSelectionMode::Single:
		mActions[SW_EDIT_SELECTION_SINGLE]->setChecked(true);
		break;

	case swSelectionMode::Piece:
		mActions[SW_EDIT_SELECTION_PIECE]->setChecked(true);
		break;

	case swSelectionMode::Color:
		mActions[SW_EDIT_SELECTION_COLOR]->setChecked(true);
		break;

	case swSelectionMode::PieceColor:
		mActions[SW_EDIT_SELECTION_PIECE_COLOR]->setChecked(true);
		break;
	}
}

void swMainWindow::UpdateModels()
{
	const std::vector<std::unique_ptr<swModel>>& Models = swGetActiveProject()->GetModels();
	swModel* CurrentModel = swGetActiveModel();

	for (size_t ActionIndex = SW_MODEL_FIRST; ActionIndex <= SW_MODEL_LAST; ActionIndex++)
	{
		QAction* Action = mActions[ActionIndex];
		size_t ModelIndex = ActionIndex - SW_MODEL_FIRST;

		if (ModelIndex < Models.size())
		{
			Action->setChecked(CurrentModel == Models[ModelIndex].get());
			Action->setText(QString::fromLatin1("%1%2 %3").arg(ModelIndex < 9 ? QString("&") : QString(), QString::number(ModelIndex + 1), Models[ModelIndex]->GetProperties().mFileName));
			Action->setVisible(true);
		}
		else
			Action->setVisible(false);
	}

	auto HasModel = [&Models](swModel* Model)
	{
		return std::find_if(Models.begin(), Models.end(), [Model](const std::unique_ptr<swModel>& CheckModel) { return CheckModel.get() == Model; }) != Models.end();
	};

	for (int TabIdx = 0; TabIdx < mModelTabWidget->count(); )
	{
		swModelTabWidget* TabWidget = (swModelTabWidget*)mModelTabWidget->widget(TabIdx);
		swModel* Model = TabWidget->GetModel();

		if (!Model)
			TabIdx++;
		else if (HasModel(Model))
		{
			mModelTabWidget->setTabText(TabIdx, Model->GetProperties().mFileName);
			TabIdx++;
		}
		else
			delete TabWidget;
	}

	mPartSelectionWidget->UpdateModels();

	if (mInsertPieceData && mInsertPieceData->IsModel())
		if (!HasModel(mInsertPieceData->GetModel()))
			SetInsertPieceData(nullptr);
}

void swMainWindow::UpdateInUseCategory()
{
	mPartSelectionWidget->UpdateInUseCategory();
}

void swMainWindow::UpdateCategories()
{
	mPartSelectionWidget->UpdateCategories();
}

void swMainWindow::UpdateTitle()
{
	setWindowModified(swGetActiveProject()->IsModified());
	setWindowFilePath(swGetActiveProject()->GetTitle());

	// An unsaved model has no folder to open.
	mActions[SW_VIEW_TERMINAL_OPEN_MODEL_FOLDER]->setEnabled(!swGetActiveProject()->GetFileName().isEmpty());

	if (swGetPreferences().mTerminalFollowModel)
		UpdateTerminalDirectory(false);
}

void swMainWindow::UpdateModified(bool Modified)
{
	setWindowModified(Modified);
}

void swMainWindow::UpdateShortcuts()
{
	for (int ActionIdx = 0; ActionIdx < SW_COMMAND_COUNT; ActionIdx++)
		mActions[ActionIdx]->setShortcut(QKeySequence(gKeyboardShortcuts.mShortcuts[ActionIdx]));
}
