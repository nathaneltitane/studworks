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
#include "sw_library_shadow.h"
#include "sw_colors.h"
#include "sw_widget_preview.h"
#include "sw_model_history.h"
#include "sw_widget_spinner.h"
#include <QPrintPreviewDialog>

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

	// snap once resizing settles
	mPartsSnapTimer.setSingleShot(true);
	mPartsSnapTimer.setInterval(300);
	connect(&mPartsSnapTimer, &QTimer::timeout, this, &swMainWindow::SnapPartsDock);

	// turntable starts when idle
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

	// clear parts shown from the selection
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

	// no terminal without kde frameworks
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

	// floating panels end on whole rows
	const int RowDelta = mPartsToolBar->isFloating() ? mPartSelectionWidget->GetRowSnapDelta() : 0;

	if (!Delta && !RowDelta)
		return;

	if (mPartsToolBar->isFloating())
		mPartsToolBar->resize(mPartsToolBar->width() + Delta, mPartsToolBar->height() + RowDelta);
	else
		resizeDocks({ mPartsToolBar }, { mPartsToolBar->width() + Delta }, Qt::Horizontal);

	// check again after relayout
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
	// input ends the turntable
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

		// ending input does nothing else
		if (StopTurntable())
			return true;
		break;

	case QEvent::ApplicationStateChange:
		// ignore brief deactivation
		if (QApplication::applicationState() != Qt::ApplicationActive && mTurntableWidget && mTurntableClock.elapsed() > 1000)
			StopTurntable();

		RestartIdleTimer();
		break;

	default:
		break;
	}

	// snap to whole tiles on release
	if (Object == mPartsToolBar && Event->type() == QEvent::Resize && mPartsToolBar->isFloating())
		mPartsSnapTimer.start();
	else if (Object == mPartsToolBar && Event->type() == QEvent::Resize && QApplication::mouseButtons() != Qt::NoButton)
		mPartsSnapPending = true;
	else if (Event->type() == QEvent::MouseButtonRelease && mPartsSnapPending)
	{
		mPartsSnapPending = false;
		QTimer::singleShot(0, this, &swMainWindow::SnapPartsDock);
	}

	// drop kde accelerators
	if (Event->type() == QEvent::WindowTitleChange)
	{
		QDockWidget* DockWidget = qobject_cast<QDockWidget*>(Object);

		if (DockWidget && DockWidget->windowTitle().contains('&'))
			DockWidget->setWindowTitle(DockWidget->windowTitle().remove('&'));
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

		// save instructions panels while shown
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
	const QString MimeType = QLatin1String("application/vnd.studworx-clipboard");
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

	// povray window stays open
	if (RenderDialogMode == swRenderDialogMode::RenderPOVRay)
	{
		if (mPOVRayDialog)
		{
			mPOVRayDialog->raise();
			mPOVRayDialog->activateWindow();
			return;
		}

		swRenderDialog* Dialog = new swRenderDialog(this, RenderDialogMode);
		Dialog->setAttribute(Qt::WA_DeleteOnClose);
		mPOVRayDialog = Dialog;
		Dialog->showMaximized();
		return;
	}

	swRenderDialog Dialog(this, RenderDialogMode);
	Dialog.exec();
#endif
}

void swMainWindow::ShowInstructionsDialog()
{
	swGetActiveProject()->GetMainModel()->WaitForAssets();

	// instructions window follows the model
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
		// save the panels first
		mInstructionsDialog->SaveLayout();
		delete mInstructionsDialog;
	}
}

// use to apply the instructions preferences
void swMainWindow::UpdateInstructionsOptions()
{
	if (mInstructionsDialog)
		mInstructionsDialog->UpdateOptions();
}

// use to paint the active model view on a printer
void swMainWindow::PrintModelView(QPrinter* Printer)
{
#ifndef QT_NO_PRINTER
	swView* ActiveView = GetActiveView();

	if (!ActiveView)
		return;

	// render at page resolution within the size limit
	const QSize ViewSize(qMax(ActiveView->GetWidth(), 1), qMax(ActiveView->GetHeight(), 1));
	const QSize PageSize = Printer->pageLayout().paintRectPixels(Printer->resolution()).size();
	const QSize ImageSize = ViewSize.scaled(PageSize.boundedTo(QSize(4096, 4096)), Qt::KeepAspectRatio);

	swModel* Model = ActiveView->GetActiveModel();
	const QImage Image = Model->GetStepImage(false, ImageSize.width(), ImageSize.height(), Model->GetCurrentStep());

	if (Image.isNull())
		return;

	QPainter Painter;

	if (!Painter.begin(Printer))
	{
		QMessageBox::warning(this, tr("Print"), tr("Error starting the print job."));
		return;
	}

	// center the image on the page
	QRect Target(QPoint(), Image.size().scaled(Painter.viewport().size(), Qt::KeepAspectRatio));
	Target.moveCenter(Painter.viewport().center());
	Painter.drawImage(Target, Image);
#endif
}

// use to print the active model view
void swMainWindow::ShowPrintDialog()
{
#ifndef QT_NO_PRINTER
	swView* ActiveView = GetActiveView();

	if (!ActiveView)
		return;

	QPrinter Printer(QPrinter::HighResolution);

	// view shape picks the orientation
	Printer.setPageOrientation(ActiveView->GetWidth() > ActiveView->GetHeight() ? QPageLayout::Landscape : QPageLayout::Portrait);

	QPrintDialog PrintDialog(&Printer, this);
	PrintDialog.setOptions(QAbstractPrintDialog::PrintToFile | QAbstractPrintDialog::PrintShowPageSize);

	// native dialogs have no button box
	QDialogButtonBox* ButtonBox = PrintDialog.findChild<QDialogButtonBox*>();

	if (ButtonBox)
	{
		QPushButton* PreviewButton = ButtonBox->addButton(tr("Preview..."), QDialogButtonBox::ActionRole);

		connect(PreviewButton, &QPushButton::clicked, &PrintDialog, [this, &PrintDialog, &Printer]()
		{
			QPrintPreviewDialog PreviewDialog(&Printer, &PrintDialog);
			connect(&PreviewDialog, &QPrintPreviewDialog::paintRequested, this, &swMainWindow::PrintModelView);

			// printing from the preview ends the print dialog
			if (PreviewDialog.exec() == QDialog::Accepted)
				PrintDialog.reject();
		});
	}

	if (PrintDialog.exec() == QDialog::Accepted)
		PrintModelView(&Printer);
#endif
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
	Options.Ratio = swGetProfileFloat(SW_PROFILE_IMAGE_RATIO);
	Options.Steps = static_cast<swImageSteps>(qBound(0, swGetProfileInteger(SW_PROFILE_IMAGE_STEPS), 2));
	Options.Background = swGetProfileInteger(SW_PROFILE_IMAGE_TRANSPARENT) ? swImageBackground::Transparent : swImageBackground::Default;
	Options.Turntable.Axis = static_cast<swTurntableAxis>(qBound(0, swGetProfileInteger(SW_PROFILE_IMAGE_AXIS), 2));
	Options.Turntable.FullTurn = swGetProfileInteger(SW_PROFILE_IMAGE_FULL_TURN);
	Options.Turntable.AngleStart = swGetProfileInteger(SW_PROFILE_IMAGE_ANGLE_START);
	Options.Turntable.AngleEnd = swGetProfileInteger(SW_PROFILE_IMAGE_ANGLE_END);
	Options.Turntable.Frames = swGetProfileInteger(SW_PROFILE_IMAGE_FRAMES);
	Options.Turntable.FrameRate = swGetProfileFloat(SW_PROFILE_IMAGE_FRAME_RATE);
	Options.Turntable.Motion = static_cast<swImageMotion>(qBound(0, swGetProfileInteger(SW_PROFILE_IMAGE_MOTION), 1));
	Options.Turntable.Turns = swGetProfileInteger(SW_PROFILE_IMAGE_TURNS);
	Options.Turntable.NextStep = static_cast<swImageNextStep>(qBound(0, swGetProfileInteger(SW_PROFILE_IMAGE_NEXT_STEP), 2));
	Options.Turntable.NextStepTurns = swGetProfileInteger(SW_PROFILE_IMAGE_NEXT_STEP_TURNS);
	Options.Turntable.NextStepSeconds = swGetProfileFloat(SW_PROFILE_IMAGE_NEXT_STEP_SECONDS);
	Options.Framing.Fit = static_cast<swImageFit>(qBound(0, swGetProfileInteger(SW_PROFILE_IMAGE_FIT), 2));
	Options.Framing.Center = static_cast<swImageCenter>(qBound(0, swGetProfileInteger(SW_PROFILE_IMAGE_CENTER), 2));
	Options.Framing.Scope = static_cast<swImageFitScope>(qBound(0, swGetProfileInteger(SW_PROFILE_IMAGE_FIT_SCOPE), 1));
	Options.Framing.Margin = swGetProfileInteger(SW_PROFILE_IMAGE_MARGIN);

	swImageDialog Dialog(this, &Options, Model);

	if (Dialog.exec() != QDialog::Accepted)
		return;

	swSetProfileInteger(SW_PROFILE_IMAGE_WIDTH, Options.Width);
	swSetProfileInteger(SW_PROFILE_IMAGE_HEIGHT, Options.Height);
	swSetProfileFloat(SW_PROFILE_IMAGE_RATIO, static_cast<float>(Options.Ratio));
	swSetProfileInteger(SW_PROFILE_IMAGE_STEPS, static_cast<int>(Options.Steps));
	swSetProfileInteger(SW_PROFILE_IMAGE_TRANSPARENT, Options.Background == swImageBackground::Transparent);
	swSetProfileInteger(SW_PROFILE_IMAGE_AXIS, static_cast<int>(Options.Turntable.Axis));
	swSetProfileInteger(SW_PROFILE_IMAGE_FULL_TURN, Options.Turntable.FullTurn);
	swSetProfileInteger(SW_PROFILE_IMAGE_ANGLE_START, Options.Turntable.AngleStart);
	swSetProfileInteger(SW_PROFILE_IMAGE_ANGLE_END, Options.Turntable.AngleEnd);
	swSetProfileInteger(SW_PROFILE_IMAGE_FRAMES, Options.Turntable.Frames);
	swSetProfileFloat(SW_PROFILE_IMAGE_FRAME_RATE, static_cast<float>(Options.Turntable.FrameRate));
	swSetProfileInteger(SW_PROFILE_IMAGE_MOTION, static_cast<int>(Options.Turntable.Motion));
	swSetProfileInteger(SW_PROFILE_IMAGE_TURNS, Options.Turntable.Turns);
	swSetProfileInteger(SW_PROFILE_IMAGE_NEXT_STEP, static_cast<int>(Options.Turntable.NextStep));
	swSetProfileInteger(SW_PROFILE_IMAGE_NEXT_STEP_TURNS, Options.Turntable.NextStepTurns);
	swSetProfileFloat(SW_PROFILE_IMAGE_NEXT_STEP_SECONDS, static_cast<float>(Options.Turntable.NextStepSeconds));
	swSetProfileInteger(SW_PROFILE_IMAGE_FIT, static_cast<int>(Options.Framing.Fit));
	swSetProfileInteger(SW_PROFILE_IMAGE_CENTER, static_cast<int>(Options.Framing.Center));
	swSetProfileInteger(SW_PROFILE_IMAGE_FIT_SCOPE, static_cast<int>(Options.Framing.Scope));
	swSetProfileInteger(SW_PROFILE_IMAGE_MARGIN, Options.Framing.Margin);

	QApplication::setOverrideCursor(Qt::WaitCursor);
	const swResult<void> Saved = swGetActiveProject()->SaveImage(Options);
	QApplication::restoreOverrideCursor();

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

void swMainWindow::ToggleShadowLibrary()
{
	swGetPreferences().mShadowLibraryEnabled = !swGetPreferences().mShadowLibraryEnabled;
	swSetProfileInteger(SW_PROFILE_SHADOW_LIBRARY_ENABLED, swGetPreferences().mShadowLibraryEnabled);

	UpdateShadowLibraryAction();
}

void swMainWindow::UpdateShadowLibraryAction()
{
	mActions[SW_EDIT_TOGGLE_SHADOW_LIBRARY]->setChecked(swGetPreferences().mShadowLibraryEnabled);
	mActions[SW_EDIT_TOGGLE_SHADOW_LIBRARY]->setEnabled(swShadowLibrary::IsLoaded());

	int Flags = 0;
	std::vector<swObject*> Selection;
	swObject* Focus = nullptr;
	swModel* ActiveModel = GetActiveModel();

	if (ActiveModel)
		ActiveModel->GetSelectionInformation(&Flags, Selection, &Focus);

	UpdateConnectedPartActions(Flags);
}

// use to gate connected part selection
void swMainWindow::UpdateConnectedPartActions(int Flags)
{
	const bool Enabled = (Flags & SW_SELECTION_PART) && (Flags & SW_SELECTION_UNSELECTED) && swGetPreferences().mShadowLibraryEnabled && swShadowLibrary::IsLoaded();

	mActions[SW_EDIT_SELECT_ADJACENT]->setEnabled(Enabled);
	mActions[SW_EDIT_SELECT_CONNECTED]->setEnabled(Enabled);
}

void swMainWindow::SetColorIndex(int ColorIndex)
{
	mColorIndex = ColorIndex;

	// picked color becomes the insert color
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
		// selected part set the color
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

	// entries run the matching command
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
	// show the insert color unless locked
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

	// keep at most two decimals
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

	constexpr swPaintedIcon IconNames[] =
	{
		swPaintedIcon::TransformTranslationAbsolute,
		swPaintedIcon::TransformTranslationRelative,
		swPaintedIcon::TransformRotationAbsolute,
		swPaintedIcon::TransformRotationRelative
	};

	SW_ARRAY_SIZE_CHECK(IconNames, swTransformType::Count);

	int TransformIndex = static_cast<int>(TransformType);
	mActions[SW_EDIT_TRANSFORM_ABSOLUTE_TRANSLATION + TransformIndex]->setChecked(true);
	mActions[SW_EDIT_TRANSFORM]->setIcon(swCreatePaintedIcon(IconNames[TransformIndex]));
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
	// todo fullscreen toolbar and esc key
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
