#include "sw_global.h"
#include "sw_window_main.h"
#include "sw_widget_builder_minifigure.h"
#include "sw_widget_spin_box_double.h"
#include "sw_widget_selection_part.h"
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
#include "sw_object_part.h"
#include "sw_object_camera.h"
#include "sw_group.h"
#include "sw_data_part.h"
#include "sw_library.h"
#include "sw_colors.h"
#include "sw_widget_preview.h"
#include "sw_model_history.h"
#include "sw_widget_spinner.h"

#if SW_ENABLE_GAMEPAD
#include <QtGamepad/QGamepad>
#endif

swMainWindow* gMainWindow;

swMainWindow::swMainWindow()
{
	mTransformType = swTransformType::RelativeTranslation;

	mColorIndex = swGetColorIndex(7);
	mTool = swTool::Select;
	mMoveXYSnapIndex = 4;
	mMoveZSnapIndex = 3;
	mAngleSnapIndex = 5;
	mAngleSnapCustom = swGetProfileFloat(SW_PROFILE_ANGLE_SNAP_CUSTOM);
	mOrbitSnapEnabled = swGetProfileInteger(SW_PROFILE_ORBIT_SNAP);
	mSelectionMode = swSelectionMode::Single;

	for (size_t FileIndex = 0; FileIndex < mRecentFiles.size(); FileIndex++)
		mRecentFiles[FileIndex] = swGetProfileString((SW_PROFILE_KEY)(SW_PROFILE_RECENT_FILE1 + FileIndex));

#if (QT_VERSION >= QT_VERSION_CHECK(5, 8, 0))
	connect(&mGamepadTimer, &QTimer::timeout, this, &swMainWindow::UpdateGamepads);
	mLastGamepadUpdate = QDateTime::currentDateTime();
	mGamepadTimer.start(33);
#endif

	// the window manager resizes a floating parts panel without mouse events, so snap once the resizing settles
	mPartsSnapTimer.setSingleShot(true);
	mPartsSnapTimer.setInterval(300);
	connect(&mPartsSnapTimer, &QTimer::timeout, this, &swMainWindow::SnapPartsDock);

	// the turntable starts once the input has been idle long enough
	mIdleTimer.setSingleShot(true);
	connect(&mIdleTimer, &QTimer::timeout, this, &swMainWindow::StartTurntable);
	connect(&mTurntableTimer, &QTimer::timeout, this, &swMainWindow::UpdateTurntable);
	RestartIdleTimer();

	gMainWindow = this;
}

swMainWindow::~swMainWindow()
{
	swPartsLibrary* Library = swGetPartsLibrary();
	disconnect(Library, nullptr, this, nullptr);

	mInsertPartData.Reset();

	delete mTurntableWidget;

	for (size_t FileIndex = 0; FileIndex < mRecentFiles.size(); FileIndex++)
		swSetProfileString((SW_PROFILE_KEY)(SW_PROFILE_RECENT_FILE1 + FileIndex), mRecentFiles[FileIndex]);

	gMainWindow = nullptr;
}

// use to restart the idle time that starts the turntable
void swMainWindow::RestartIdleTimer()
{
	const int IdleTime = swGetPreferences().mTurntableIdleDuration;

	if (IdleTime > 0 && !mTurntableWidget && QApplication::applicationState() == Qt::ApplicationActive)
		mIdleTimer.start(IdleTime * 1000);
	else
		mIdleTimer.stop();
}

// use to show the model full screen turning around the z axis
void swMainWindow::StartTurntable()
{
	swModel* Model = GetCurrentTabModel();
	swView* ActiveView = GetActiveView();
	swVector3 Min, Max;

	if (mTurntableWidget || !Model || !ActiveView || QApplication::activeModalWidget() || QApplication::activePopupWidget() || QApplication::mouseButtons() != Qt::NoButton || !Model->GetVisiblePartsBoundingBox(Min, Max))
	{
		RestartIdleTimer();
		return;
	}

	mTurntableCenter = (Min + Max) * 0.5f;
	mTurntableRadius = qMax(swLength(Max - Min) * 0.5f, 10.0f);

	// the turn starts from the side the active view looks at
	const swVector3 Direction = ActiveView->GetCamera()->GetSectionPosition(SW_CAMERA_SECTION_POSITION) - mTurntableCenter;
	mTurntableStartAngle = atan2f(Direction.y, Direction.x);

	swView* View = new swView(swViewType::Turntable, Model);
	View->SetCamera(View->GetCamera(), true);
	View->GetCamera()->SetProjection(swCameraProjection::Perspective);

	mTurntableWidget = new swViewWidget(nullptr, View);
	mTurntableWidget->setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
	mTurntableWidget->setCursor(Qt::BlankCursor);
	mTurntableWidget->setGeometry(screen()->geometry());

	mIdleCursorPosition = QCursor::pos();
	mTurntableClock.start();
	UpdateTurntable();

	mTurntableWidget->showFullScreen();
	mTurntableTimer.start(16);
}

// use to close the turntable, true when it was showing
bool swMainWindow::StopTurntable()
{
	if (!mTurntableWidget)
		return false;

	mTurntableTimer.stop();

	// the closing input may still be on its way to the turntable window
	mTurntableWidget->hide();
	mTurntableWidget->deleteLater();
	mTurntableWidget = nullptr;

	activateWindow();
	RestartIdleTimer();

	return true;
}

// use to turn the turntable camera for the time shown so far
void swMainWindow::UpdateTurntable()
{
	if (!mTurntableWidget)
		return;

	swView* View = mTurntableWidget->GetView();
	swCamera* Camera = View->GetCamera();

	// the distance keeps the model bounds inside the narrower field of view
	const float Aspect = static_cast<float>(qMax(mTurntableWidget->width(), 1)) / qMax(mTurntableWidget->height(), 1);
	const float HalfHeight = Camera->m_fovy * 0.5f * SW_DEGREES_TO_RADIANS;
	const float HalfField = qMin(HalfHeight, atanf(tanf(HalfHeight) * Aspect));
	const float Distance = mTurntableRadius / sinf(HalfField);

	const float Turn = mTurntableClock.elapsed() / 1000.0f / qMax(swGetPreferences().mTurntableTurnTime, 1);
	const float Angle = mTurntableStartAngle + Turn * 2.0f * SW_PI;
	const float Latitude = 25.0f * SW_DEGREES_TO_RADIANS;
	const swVector3 Position = mTurntableCenter + swVector3(cosf(Latitude) * cosf(Angle), cosf(Latitude) * sinf(Angle), sinf(Latitude)) * Distance;

	Camera->m_zFar = qMax(Camera->m_zFar, Distance + mTurntableRadius * 2.0f);
	Camera->SetViewpoint(Position, mTurntableCenter, swVector3(0.0f, 0.0f, 1.0f));
	View->Redraw();
}

swView* swMainWindow::CreateView(swModel* Model)
{
	swView* NewView = new swView(swViewType::View, Model);

	connect(NewView, &swView::FocusReceived, this, &swMainWindow::ViewFocusReceived);

	AddView(NewView);

	return NewView;
}

void swMainWindow::PreviewPart(swPartData* PartData, int ColorCode, bool ShowPreview)
{
	if (ShowPreview)
		mPreviewToolBar->show();

	// parts shown from the model selection are cleared again when the selection goes away
	mPreviewFromSelection = !ShowPreview;
	mPreviewWidget->SetCurrentPart(PartData, ColorCode);
}

void swMainWindow::CreateTerminalWidget()
{
#ifdef SW_ENABLE_TERMINAL
	mTerminalToolBar = new QDockWidget(tr("Terminal"), this);
	mTerminalToolBar->setObjectName("TerminalToolBar");
	mTerminalToolBar->setWhatsThis(tr("<p><b>Terminal</b></p><p>An embedded Konsole terminal that starts in the folder of the current model.</p>"));
	swSetupWidgetHelp(mTerminalToolBar, nullptr);
	mTerminalToolBar->setWidget(new swTerminalWidget(mTerminalToolBar));
	addDockWidget(Qt::BottomDockWidgetArea, mTerminalToolBar);
	mTerminalToolBar->hide();

	connect(mTerminalToolBar, &QDockWidget::topLevelChanged, this, &swMainWindow::EnableWindowFlags);
#else
	// Without KDE Frameworks there is no terminal to show.
	mActions[SW_VIEW_TOOLBAR_TERMINAL]->setVisible(false);
	mActions[SW_VIEW_TERMINAL_TOGGLE_FOLLOW_MODEL]->setVisible(false);
	mActions[SW_VIEW_TERMINAL_GO_TO_MODEL_FOLDER]->setVisible(false);
	mActions[SW_VIEW_TERMINAL_OPEN_MODEL_FOLDER]->setVisible(false);
#endif
}

void swMainWindow::UpdateTerminalDirectory(bool Force)
{
#ifdef SW_ENABLE_TERMINAL
	const QString FileName = swGetActiveProject()->GetFileName();

	if (mTerminalToolBar && !FileName.isEmpty())
		static_cast<swTerminalWidget*>(mTerminalToolBar->widget())->SetDirectory(QFileInfo(FileName).absolutePath(), Force);
#else
	Q_UNUSED(Force);
#endif
}

void swMainWindow::SnapPartsDock()
{
	if (!swGetPreferences().mPartsSnapToTiles || !mPartsToolBar || !mPartSelectionWidget)
		return;

	const Qt::DockWidgetArea Area = dockWidgetArea(mPartsToolBar);

	if (!mPartsToolBar->isFloating() && Area != Qt::LeftDockWidgetArea && Area != Qt::RightDockWidgetArea)
	{
		mPartSelectionWidget->SnapCategories();
		return;
	}

	const int Delta = mPartSelectionWidget->GetTileSnapDelta();
	// a floating panel also ends on whole thumbnail or list rows
	const int RowDelta = mPartsToolBar->isFloating() ? mPartSelectionWidget->GetRowSnapDelta() : 0;

	if (!Delta && !RowDelta)
		return;

	if (mPartsToolBar->isFloating())
		mPartsToolBar->resize(mPartsToolBar->width() + Delta, mPartsToolBar->height() + RowDelta);
	else
		resizeDocks({ mPartsToolBar }, { mPartsToolBar->width() + Delta }, Qt::Horizontal);

	// Check once more after the tiles are laid out again in case the view wraps differently than measured.
	if (!mPartsSnapRetried)
	{
		mPartsSnapRetried = true;

		QTimer::singleShot(0, this, [this]()
		{
			SnapPartsDock();
			mPartsSnapRetried = false;
		});
	}
}

void swMainWindow::CreatePreviewWidget()
{
	mPreviewWidget  = new swPreviewDockWidget(this);

	mPreviewToolBar = new QDockWidget(tr("Preview"), this);
	mPreviewToolBar->setWindowTitle(tr("Preview"));
	mPreviewToolBar->setObjectName("PreviewToolBar");
	mPreviewToolBar->setWhatsThis(tr("<p><b>Preview</b></p><p>Inspect a part or model without placing it in the model. Double-click a part in the Parts dock to preview it. Lock Preview keeps the current preview when the source selection changes; unlock it to allow updates.</p>"));
	swSetupWidgetHelp(mPreviewToolBar, nullptr);
	mPreviewToolBar->setWidget(mPreviewWidget);
	addDockWidget(Qt::RightDockWidgetArea, mPreviewToolBar);

	connect(mPreviewToolBar, &QDockWidget::topLevelChanged, this, &swMainWindow::EnableWindowFlags);
}

bool swMainWindow::eventFilter(QObject* Object, QEvent* Event)
{
	// any input ends the turntable and restarts the idle time
	switch (Event->type())
	{
	case QEvent::MouseMove:
		if (QCursor::pos() == mIdleCursorPosition)
			break;

		mIdleCursorPosition = QCursor::pos();
		StopTurntable();
		RestartIdleTimer();
		break;

	case QEvent::KeyPress:
	case QEvent::MouseButtonPress:
	case QEvent::MouseButtonDblClick:
	case QEvent::Wheel:
	case QEvent::TouchBegin:
	case QEvent::TabletPress:
		RestartIdleTimer();

		// the input that ends the turntable does nothing else
		if (StopTurntable())
			return true;
		break;

	case QEvent::ApplicationStateChange:
		// showing the turntable window can briefly deactivate the application
		if (QApplication::applicationState() != Qt::ApplicationActive && mTurntableWidget && mTurntableClock.elapsed() > 1000)
			StopTurntable();

		RestartIdleTimer();
		break;

	default:
		break;
	}

	// Resizing the parts panel by hand snaps it to whole tiles once the mouse is released.
	if (Object == mPartsToolBar && Event->type() == QEvent::Resize && mPartsToolBar->isFloating())
		mPartsSnapTimer.start();
	else if (Object == mPartsToolBar && Event->type() == QEvent::Resize && QApplication::mouseButtons() != Qt::NoButton)
		mPartsSnapPending = true;
	else if (Event->type() == QEvent::MouseButtonRelease && mPartsSnapPending)
	{
		mPartsSnapPending = false;
		QTimer::singleShot(0, this, &swMainWindow::SnapPartsDock);
	}

	QTabBar* TabBar = mModelTabWidget ? mModelTabWidget->tabBar() : nullptr;

	if (TabBar && Object == TabBar)
	{
		if (Event->type() == QEvent::MouseButtonPress)
		{
			QMouseEvent* MouseEvent = static_cast<QMouseEvent*>(Event);
			mMousePressTabIndex = -1;

			if (MouseEvent->button() == Qt::MiddleButton)
				mMousePressTabIndex = TabBar->tabAt(MouseEvent->pos());
		}
		else if (Event->type() == QEvent::MouseButtonRelease)
		{
			QMouseEvent* MouseEvent = static_cast<QMouseEvent*>(Event);

			if (MouseEvent->button() == Qt::MiddleButton && TabBar->tabAt(MouseEvent->pos()) == mMousePressTabIndex)
				TabBar->tabCloseRequested(mMousePressTabIndex);
		}
	}

	return QMainWindow::eventFilter(Object, Event);
}

void swMainWindow::TogglePreviewWidget(bool Visible)
{
	if (mPreviewToolBar)
	{
		if (Visible)
			mPreviewToolBar->show();
		else
			mPreviewToolBar->hide();
	}
	else if (Visible)
	{
		CreatePreviewWidget();
	}
}

void swMainWindow::EnableWindowFlags(bool Detached)
{
	if (Detached)
	{
		QDockWidget* DockWidget = qobject_cast<QDockWidget*>(sender());

		DockWidget->setWindowFlags(Qt::CustomizeWindowHint | Qt::Window | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint);

		if (isVisible())
			DockWidget->show();
	}
}

void swMainWindow::ScheduleAssetUpdate()
{
	if (mAssetUpdateScheduled)
		return;

	mAssetUpdateScheduled = true;

	QTimer::singleShot(0, this, [this]()
	{
		mAssetUpdateScheduled = false;
		UpdateAssets();
	});
}

void swMainWindow::UpdateAssets()
{
	Project* Project = swGetActiveProject();
	mStatusLoadSpinner->setToolTip(QString());

	if (mStatusProject != Project)
	{
		mStatusProject = Project;
		mStatusRequiredAssets.clear();
		mStatusLoadFailureLabel->hide();
		mStatusLoadFailureLabel->setToolTip(QString());
		mStatusLoadFailureLabel->setAccessibleDescription(QString());
	}

	if (!Project || !Project->GetMainModel())
	{
		mStatusLoadSpinner->hide();
		mStatusLoadFailureLabel->hide();
		return;
	}

	std::vector<swModel*> UpdatedModels;

	for (const std::unique_ptr<swModel>& Model : Project->GetModels())
		Model->UpdatePartData(UpdatedModels);

	UpdateTimeline(false, true);

	swView::UpdateProjectViews(Project);

	const std::vector<swPartDataReference> RequiredPartData = Project->GetRequiredPartData();
	const std::vector<swPart*> GeneratedParts = Project->GetRequiredSynthParts();

	mStatusRequiredAssets.clear();
	for (const swPartDataReference& RequiredData : RequiredPartData)
		mStatusRequiredAssets.insert(RequiredData.Get());

	int PendingParts = 0;
	int PendingSynths = 0;
	int Failed = 0;
	QString FailureDetails;

	for (const QString& Warning : Project->GetLoadWarningDetails())
	{
		if (!FailureDetails.isEmpty())
			FailureDetails += QLatin1Char('\n');

		FailureDetails += Warning;
	}

	for (const swPartDataReference& RequiredData : RequiredPartData)
	{
		const swPartData* PartData = RequiredData.Get();
		if (PartData->mState == swAssetState::Failed)
		{
			Failed++;

			QString Error = swGetPartsLibrary()->GetPartDataLoadError(PartData);

			if (Error.isEmpty())
				Error = tr("Could not load part.");

			if (!FailureDetails.isEmpty())
				FailureDetails += QLatin1Char('\n');

			FailureDetails += QStringLiteral("'%1': %2").arg(QString::fromLatin1(PartData->mFileName), Error);
		}
		else if (PartData->mState != swAssetState::Loaded)
			PendingParts++;
	}

	for (const swPart* Part : GeneratedParts)
	{
		if (Part->IsGeneratedMeshPending())
			PendingSynths++;
		else if (!Part->HasGeneratedMesh())
		{
			Failed++;

			QString Error = swGetPartsLibrary()->GetSynthMeshError(Part);

			if (Error.isEmpty())
				Error = tr("Could not generate geometry.");

			if (!FailureDetails.isEmpty())
				FailureDetails += QLatin1Char('\n');

			FailureDetails += QStringLiteral("'%1': %2").arg(QString::fromLatin1(Part->mPartData->mFileName), Error);
		}
	}

	const int Total = static_cast<int>(RequiredPartData.size() + GeneratedParts.size());
	const int Pending = PendingParts + PendingSynths;
	QString LoadingText;

	if (!PendingParts && PendingSynths)
		LoadingText = tr("Regenerating synthesized part geometry.");
	else
		LoadingText = tr("Loading model parts (%1 / %2).").arg(Total - Pending - Failed).arg(Total);

	mStatusLoadSpinner->setToolTip(LoadingText);
	mStatusLoadSpinner->setAccessibleDescription(LoadingText);

	if (!FailureDetails.isEmpty())
	{
		QString FailureText = tr("The model could not be fully loaded. Some parts may be missing.\n\n%1").arg(FailureDetails);

		mStatusLoadFailureLabel->setToolTip(FailureText);
		mStatusLoadFailureLabel->setAccessibleDescription(FailureText);
		mStatusLoadFailureLabel->show();
	}
	else
	{
		mStatusLoadFailureLabel->hide();
		mStatusLoadFailureLabel->setToolTip(QString());
		mStatusLoadFailureLabel->setAccessibleDescription(QString());
	}

	mStatusLoadSpinner->setVisible(Pending > 0);
}

void swMainWindow::closeEvent(QCloseEvent* Event)
{
	if (SaveProjectIfModified())
	{
		Event->accept();

		// the instructions panels are saved while they are still shown
		if (mInstructionsDialog)
			mInstructionsDialog->SaveLayout();

		QSettings Settings;
		Settings.beginGroup("MainWindow");
		Settings.setValue("Geometry", saveGeometry());
		Settings.setValue("State", saveState());
		mPartSelectionWidget->SaveState(Settings);
		Settings.endGroup();

		gApplication->SaveTabLayout();
	}
	else
		Event->ignore();
}

void swMainWindow::dragEnterEvent(QDragEnterEvent* Event)
{
	if (Event->mimeData()->hasUrls())
		Event->acceptProposedAction();
}

void swMainWindow::dropEvent(QDropEvent* Event)
{
	const QMimeData* MimeData = Event->mimeData();
	const QList<QUrl> Urls = MimeData->urls();
	for (const QUrl& Url : Urls)
		if (OpenProject(Url.toLocalFile()))
			break;
}

QMenu* swMainWindow::createPopupMenu()
{
	QMenu* Menu = new QMenu(this);

	UpdateDockWidgetActions();

	Menu->addAction(mActions[SW_VIEW_TOOLBAR_PARTS]);
	Menu->addAction(mActions[SW_VIEW_TOOLBAR_COLORS]);
	Menu->addAction(mActions[SW_VIEW_TOOLBAR_PROPERTIES]);
	Menu->addAction(mActions[SW_VIEW_TOOLBAR_TIMELINE]);
	Menu->addAction(mActions[SW_VIEW_TOOLBAR_PREVIEW]);
	Menu->addAction(mActions[SW_VIEW_TOOLBAR_MINIFIGURE_BUILDER]);
	Menu->addAction(mActions[SW_VIEW_TOOLBAR_TERMINAL]);
	Menu->addSeparator();
	Menu->addAction(mActions[SW_VIEW_TOOLBAR_STANDARD]);
	Menu->addAction(mActions[SW_VIEW_TOOLBAR_TOOLS]);
	Menu->addAction(mActions[SW_VIEW_TOOLBAR_TIME]);

	return Menu;
}

void swMainWindow::UpdateDockWidgetActions()
{
	mActions[SW_VIEW_TOOLBAR_PARTS]->setChecked(mPartsToolBar->isVisible());
	mActions[SW_VIEW_TOOLBAR_COLORS]->setChecked(mColorsToolBar->isVisible());
	mActions[SW_VIEW_TOOLBAR_PROPERTIES]->setChecked(mPropertiesToolBar->isVisible());
	mActions[SW_VIEW_TOOLBAR_TIMELINE]->setChecked(mTimelineToolBar->isVisible());
	mActions[SW_VIEW_TOOLBAR_STANDARD]->setChecked(mStandardToolBar->isVisible());
	mActions[SW_VIEW_TOOLBAR_TOOLS]->setChecked(mToolsToolBar->isVisible());
	mActions[SW_VIEW_TOOLBAR_TIME]->setChecked(mTimeToolBar->isVisible());
	mActions[SW_VIEW_TOOLBAR_PREVIEW]->setChecked(mPreviewToolBar->isVisible());
	mActions[SW_VIEW_TOOLBAR_MINIFIGURE_BUILDER]->setChecked(mMinifigureBuilderToolBar->isVisible());
	mActions[SW_VIEW_TOOLBAR_TERMINAL]->setChecked(mTerminalToolBar && mTerminalToolBar->isVisible());
}

void swMainWindow::UpdateGamepads()
{
#if SW_ENABLE_GAMEPAD
	QDateTime Now = QDateTime::currentDateTime();
	quint64 Elapsed = mLastGamepadUpdate.msecsTo(Now);
	mLastGamepadUpdate = Now;

	if (!gMainWindow)
		return;

	swView* ActiveView = GetActiveView();
	if (!ActiveView)
		return;

	const QList<int> Gamepads = QGamepadManager::instance()->connectedGamepads();
	if (Gamepads.isEmpty())
		return;

	QGamepad Gamepad(Gamepads[0]);

	float Scale = (float)Elapsed / 20.0f;
	swVector3 Distance(Scale * Gamepad.axisLeftX(), 0.0f, -Scale * Gamepad.axisLeftY());

	if (fabsf(Distance.LengthSquared()) > 0.01f)
		ActiveView->MoveCamera(Distance);
#endif
}

void swMainWindow::ModelTabContextMenuRequested(const QPoint& Point)
{
	QMenu* Menu = new QMenu;

	mMousePressTabIndex = mModelTabWidget->tabBar()->tabAt(Point);

	if (mModelTabWidget->count() > 1)
		Menu->addAction(tr("Close Other Tabs"), this, &swMainWindow::ModelTabCloseOtherTabs);
	if (mMousePressTabIndex == mModelTabWidget->currentIndex())
		Menu->addAction(mActions[SW_VIEW_RESET_VIEWS]);

	Menu->exec(QCursor::pos());
	delete Menu;
}

void swMainWindow::ModelTabCloseOtherTabs()
{
	if (mMousePressTabIndex == -1)
		return;

	while (mModelTabWidget->count() - 1 > mMousePressTabIndex)
		delete mModelTabWidget->widget(mMousePressTabIndex + 1);

	while (mModelTabWidget->count() > 1)
		delete mModelTabWidget->widget(0);
}

void swMainWindow::ModelTabClosed(int Index)
{
	if (mModelTabWidget->count() != 1)
		delete mModelTabWidget->widget(Index);
	else
		NewProject();
}

void swMainWindow::ModelTabChanged(int Index)
{
	Project* Project = swGetActiveProject();
	swModelTabWidget* CurrentTab = (swModelTabWidget*)mModelTabWidget->widget(Index);

	Project->SetActiveModel(CurrentTab ? CurrentTab->GetModel() : nullptr, true);
}

void swMainWindow::ClipboardChanged()
{
	const QString MimeType = QLatin1String("application/vnd.studworks-clipboard");
	const QMimeData* MimeData = QApplication::clipboard()->mimeData();
	QByteArray ClipboardData;

	if (MimeData && MimeData->hasFormat(MimeType))
		ClipboardData = MimeData->data(MimeType);

	gApplication->SetClipboard(ClipboardData);
}

void swMainWindow::ActionTriggered()
{
	QObject* Action = sender();

	for (int CommandIdx = 0; CommandIdx < SW_COMMAND_COUNT; CommandIdx++)
	{
		if (Action == mActions[CommandIdx])
		{
			HandleCommand((swCommandId)CommandIdx);
			break;
		}
	}
}

void swMainWindow::ColorChanged(int ColorIndex)
{
	SetColorIndex(ColorIndex);
}

void swMainWindow::PartListPicked(swPartData* PartData)
{
	if (!PartData)
		return;

	quint32 ColorCode = swGetColorCode(mPartSelectionWidget->GetColorIndex());

	PreviewPart(PartData, ColorCode, true);
}

void swMainWindow::ColorButtonClicked()
{
	swModel* ActiveModel = GetActiveModel();

	if (ActiveModel)
		ActiveModel->PaintSelectedParts();
}

void swMainWindow::ShowAboutDialog()
{
	swAboutDialog Dialog(this);
	Dialog.exec();
}

void swMainWindow::ShowHTMLDialog()
{
	swHTMLExportOptions Options(swGetActiveProject());
	swHTMLDialog Dialog(this, &Options);

	if (Dialog.exec() != QDialog::Accepted)
		return;

	Options.SaveDefaults();
	ShowResultError(swGetActiveProject()->ExportHTML(Options), tr("Export HTML"));
}

void swMainWindow::ShowExportPOVRayDialog()
{
	swResult<void> ExportResult = swGetActiveProject()->ExportPOVRay(QString());

	if (!ExportResult && !ExportResult.error().isEmpty())
		QMessageBox::warning(this, tr("POV-Ray Export" ), ExportResult.error());
}

void swMainWindow::ShowRenderDialog(swRenderDialogMode RenderDialogMode)
{
#ifndef SW_DISABLE_RENDER_DIALOG
	swRenderDialog Dialog(this, RenderDialogMode);
	Dialog.exec();
#endif
}

void swMainWindow::ShowInstructionsDialog()
{
	swGetActiveProject()->GetMainModel()->WaitForAssets();

	// The instructions window stays open beside the model and follows its changes.
	if (mInstructionsDialog)
	{
		mInstructionsDialog->raise();
		mInstructionsDialog->activateWindow();
		return;
	}

	swInstructionsDialog* Dialog = new swInstructionsDialog(this, swGetActiveProject());
	Dialog->setAttribute(Qt::WA_DeleteOnClose);
	connect(this, &swMainWindow::ModelContentsChanged, Dialog, &swInstructionsDialog::ModelChanged);
	mInstructionsDialog = Dialog;
	Dialog->showMaximized();
}

void swMainWindow::CloseInstructionsDialog()
{
	if (mInstructionsDialog)
	{
		// the panels are saved before the window goes with the old project
		mInstructionsDialog->SaveLayout();
		delete mInstructionsDialog;
	}
}

// use to show the instructions options changed in the preferences in the open window
void swMainWindow::UpdateInstructionsOptions()
{
	if (mInstructionsDialog)
		mInstructionsDialog->UpdateOptions();
}

void swMainWindow::ShowPrintWidget()
{
	// printing has one panel in the instructions window beside the pages it prints
	ShowInstructionsDialog();

	if (mInstructionsDialog)
		mInstructionsDialog->ShowPrintWidget();
}

void swMainWindow::ShowImageDialog()
{
	swImageDialogOptions Options;
	Project* Project = swGetActiveProject();
	swModel* Model = Project->GetActiveModel();

	Options.Width = swGetProfileInteger(SW_PROFILE_IMAGE_WIDTH);
	Options.Height = swGetProfileInteger(SW_PROFILE_IMAGE_HEIGHT);
	Options.Start = Model->GetCurrentStep();
	Options.End = Model->GetLastStep();
	Options.FilePath = Project->GetImageFileName(false);

	swImageDialog Dialog(this, &Options);

	if (Dialog.exec() != QDialog::Accepted)
		return;

	swSetProfileInteger(SW_PROFILE_IMAGE_WIDTH, Options.Width);
	swSetProfileInteger(SW_PROFILE_IMAGE_HEIGHT, Options.Height);

	const swResult<void> Saved = swGetActiveProject()->SaveImage(Options);

	if (!Saved)
		QMessageBox::warning(this, tr("Save Image"), Saved.error());
}

void swMainWindow::ShowSelectDialog()
{
	swModel* ActiveModel = GetActiveModel();

	if (!ActiveModel)
		return;

	if (ActiveModel->GetParts().empty() && ActiveModel->GetCameras().empty() && ActiveModel->GetLights().empty())
	{
		QMessageBox::information(this, tr("Select Objects"), tr("Nothing to select."));
		return;
	}

	swSelectDialog Dialog(this, ActiveModel);

	if (Dialog.exec() != QDialog::Accepted)
		return;

	ActiveModel->SetSelectionAndFocusAction(Dialog.mObjects, nullptr, 0, swSelectionMode::Single);
}

void swMainWindow::ShowResultError(const swResult<void>& Result, const QString& Title)
{
	if (!Result && !Result.error().isEmpty())
		QMessageBox::warning(this, Title, Result.error());
}

void swMainWindow::ToggleFirstPartAtOrigin()
{
	swGetPreferences().mFirstPartAtOrigin = !swGetPreferences().mFirstPartAtOrigin;
	swSetProfileInteger(SW_PROFILE_FIRST_PART_AT_ORIGIN, swGetPreferences().mFirstPartAtOrigin);

	mActions[SW_PART_TOGGLE_FIRST_AT_ORIGIN]->setChecked(swGetPreferences().mFirstPartAtOrigin);
}

void swMainWindow::SetColorIndex(int ColorIndex)
{
	mColorIndex = ColorIndex;

	// Picking a color explicitly also becomes the color to insert next.
	mLastInsertedColorIndex = ColorIndex;

	UpdatePartsColor();
	UpdateColor();
}

int swMainWindow::GetInsertColorIndex() const
{
	switch (swGetPreferences().mInsertColorMode)
	{
	case swInsertColorMode::CurrentColor:
		break;

	case swInsertColorMode::SelectedPartColor:
		// Selecting a part already made its color the current color.
		break;

	case swInsertColorMode::LastInsertedColor:
		if (mLastInsertedColorIndex != -1)
			return mLastInsertedColorIndex;
		break;
	}

	return mColorIndex;
}

void swMainWindow::SetLastInsertedColorIndex(int ColorIndex)
{
	mLastInsertedColorIndex = ColorIndex;

	UpdatePartsColor();
}

void swMainWindow::SetInsertColorMode(swInsertColorMode InsertColorMode)
{
	swGetPreferences().mInsertColorMode = InsertColorMode;
	swSetProfileInteger(SW_PROFILE_INSERT_COLOR_MODE, static_cast<int>(InsertColorMode));

	UpdatePartsColor();
}

void swMainWindow::ColorOptionsMenuAboutToShow()
{
	QMenu* Menu = qobject_cast<QMenu*>(sender());
	Menu->clear();

	// Each entry runs the matching command so keyboard shortcuts and the menu stay in sync.
	auto AddCommand = [this](QMenu* TargetMenu, const QString& Text, swCommandId CommandId, bool Checked)
	{
		QAction* Action = TargetMenu->addAction(Text, this, [this, CommandId]()
		{
			HandleCommand(CommandId);
		});

		Action->setCheckable(true);
		Action->setChecked(Checked);
		Action->setShortcut(mActions[CommandId]->shortcut());
		Action->setShortcutContext(Qt::WidgetShortcut);

		return Action;
	};

	const swInsertColorMode InsertColorMode = swGetPreferences().mInsertColorMode;

	QMenu* InsertMenu = Menu->addMenu(tr("Insert Color"));
	QActionGroup* InsertGroup = new QActionGroup(InsertMenu);
	InsertGroup->addAction(AddCommand(InsertMenu, tr("Current Color"), SW_PART_INSERT_COLOR_CURRENT, InsertColorMode == swInsertColorMode::CurrentColor));
	InsertGroup->addAction(AddCommand(InsertMenu, tr("Selected Part Color"), SW_PART_INSERT_COLOR_SELECTED, InsertColorMode == swInsertColorMode::SelectedPartColor));
	InsertGroup->addAction(AddCommand(InsertMenu, tr("Last Inserted Part Color"), SW_PART_INSERT_COLOR_LAST_INSERTED, InsertColorMode == swInsertColorMode::LastInsertedColor));

	AddCommand(Menu, tr("Lock Parts Color"), SW_VIEW_PARTS_TOGGLE_LOCK_COLOR, mPartSelectionWidget && mPartSelectionWidget->IsColorLocked());
}

void swMainWindow::UpdatePartsColor()
{
	// The parts list shows the color new parts will be inserted with, unless its color is locked.
	if (mPartSelectionWidget)
		mPartSelectionWidget->SetColorIndex(GetInsertColorIndex());
}

void swMainWindow::SetMoveSnapEnabled(bool Enabled)
{
	mMoveSnapEnabled = Enabled;
	UpdateSnap();
}

void swMainWindow::SetAngleSnapEnabled(bool Enabled)
{
	mAngleSnapEnabled = Enabled;
	UpdateSnap();
}

QString swMainWindow::GetAngleSnapText() const
{
	return mAngleSnapEnabled ? swFormatDoubleLocalized(GetAngleSnap(), locale(), 'g', 6) : tr("None");
}

void swMainWindow::SetMoveXYSnapIndex(int Index)
{
	mMoveXYSnapIndex = Index;
	UpdateSnap();
}

void swMainWindow::SetMoveZSnapIndex(int Index)
{
	mMoveZSnapIndex = Index;
	UpdateSnap();
}

void swMainWindow::SetAngleSnapIndex(int Index)
{
	mAngleSnapIndex = Index;
	UpdateSnap();
}

void swMainWindow::SetCustomAngleSnap()
{
	bool Ok = false;
	const double Angle = QInputDialog::getDouble(this, tr("Custom Rotation Snap"), tr("Snap angle in degrees:"), mAngleSnapCustom, 0.01, 360.0, 2, &Ok);

	if (!Ok)
	{
		UpdateSnap();
		return;
	}

	// Keep at most two decimals.
	mAngleSnapCustom = static_cast<float>(qRound(Angle * 100.0) / 100.0);
	swSetProfileFloat(SW_PROFILE_ANGLE_SNAP_CUSTOM, mAngleSnapCustom);

	mActions[SW_EDIT_SNAP_ANGLE_CUSTOM]->setText(tr("Custom (%1 Degrees)...").arg(swFormatDoubleLocalized(mAngleSnapCustom, locale(), 'g', 6)));

	SetAngleSnapIndex(SW_EDIT_SNAP_ANGLE_CUSTOM - SW_EDIT_SNAP_ANGLE0);
}

void swMainWindow::SetRelativeTransform(bool RelativeTransform)
{
	mRelativeTransform = RelativeTransform;

	UpdateLockSnap();
	swView::UpdateAllViews();
}

void swMainWindow::SetSeparateTransform(bool SelectionTransform)
{
	mLocalTransform = SelectionTransform;

	UpdateLockSnap();
}

void swMainWindow::SetTransformType(swTransformType TransformType)
{
	if (TransformType < swTransformType::First || TransformType >= swTransformType::Count)
		return;

	mTransformType = TransformType;

	constexpr const char* IconNames[] =
	{
		"transform_translation_absolute",
		"transform_translation_relative",
		"transform_rotation_absolute",
		"transform_rotation_relative"
	};

	SW_ARRAY_SIZE_CHECK(IconNames, swTransformType::Count);

	int TransformIndex = static_cast<int>(TransformType);
	mActions[SW_EDIT_TRANSFORM_ABSOLUTE_TRANSLATION + TransformIndex]->setChecked(true);
	mActions[SW_EDIT_TRANSFORM]->setIcon(swCreateInterfaceIcon("toolbars/standard", IconNames[TransformIndex]));
}

void swMainWindow::SetInsertPartData(swPartData* PartData)
{
	mInsertPartData = swPartDataReference(PartData);

	if (mInsertPartData)
		swGetPartsLibrary()->RequestPartDataLoad(mInsertPartData.Get(), swLoadPriority::Normal);
}

std::optional<swVector3> swMainWindow::GetTransformAmount()
{
	return swVector3(mTransformXEdit->value(), mTransformYEdit->value(), mTransformZEdit->value());
}

void swMainWindow::ToggleDockWidget(QWidget* DockWidget)
{
	if (DockWidget->isHidden())
	{
		DockWidget->show();
		DockWidget->raise();
	}
	else
		DockWidget->close();
}

void swMainWindow::ToggleFullScreen()
{
	// todo: hide toolbars and menu
	// todo: create fullscreen toolbar or support esc key to go back
	if (isFullScreen())
		showNormal();
	else
		showFullScreen();
}

swModel* swMainWindow::GetActiveModel() const
{
	swView* ActiveView = GetActiveView();
	return ActiveView ? ActiveView->GetActiveModel() : nullptr;
}

swModelTabWidget* swMainWindow::GetTabForView(swView* View) const
{
	QWidget* Widget = View->GetWidget();

	while (Widget)
	{
		swModelTabWidget* TabWidget = qobject_cast<swModelTabWidget*>(Widget);

		if (TabWidget)
			return TabWidget;
		else
			Widget = Widget->parentWidget();
	}

	return nullptr;
}
