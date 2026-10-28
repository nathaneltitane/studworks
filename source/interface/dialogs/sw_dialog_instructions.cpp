#include "sw_global.h"
#include "sw_dialog_instructions.h"
#include "sw_dialog_instructions_editor.h"
#include "sw_dialog_instructions_page.h"
#include "sw_dialog_instructions_placement.h"
#include "sw_dialog_instructions_print.h"
#include "sw_dialog_instructions_properties.h"
#include "sw_project.h"
#include "sw_model.h"
#include "sw_window_utilities.h"

swInstructionsDialog::swInstructionsDialog(QWidget* Parent, Project* Project)
	: QMainWindow(Parent), mProject(Project)
{
	setWindowTitle(tr("Instructions"));

	mInstructions = mProject->GetInstructions();

	mPropertiesWidget = new swInstructionsPropertiesWidget(this, mInstructions);
	mPropertiesWidget->setObjectName("InstructionsProperties");
	addDockWidget(Qt::RightDockWidgetArea, mPropertiesWidget);

	mPlacementWidget = new swInstructionsPlacementWidget(this, mInstructions);
	mPlacementWidget->setObjectName("InstructionsPlacement");
	addDockWidget(Qt::RightDockWidgetArea, mPlacementWidget);
	splitDockWidget(mPropertiesWidget, mPlacementWidget, Qt::Vertical);

	mGridWidget = new swInstructionsGridWidget(this, mInstructions);
	mGridWidget->setObjectName("InstructionsGrid");
	addDockWidget(Qt::RightDockWidgetArea, mGridWidget);
	splitDockWidget(mPlacementWidget, mGridWidget, Qt::Vertical);

	mPageWidget = new swInstructionsPageWidget(this, mInstructions, mPropertiesWidget, mPlacementWidget);
	setCentralWidget(mPageWidget);

	mPageListWidget = new swInstructionsPageListWidget(this, mInstructions);
	mPageListWidget->setObjectName("InstructionsPageList");
	addDockWidget(Qt::LeftDockWidgetArea, mPageListWidget);

	// the print panel opens on demand below the grid like the minifigure builder, a panel of its own
	mPrintWidget = new swInstructionsPrintWidget(this, mInstructions);
	mPrintWidget->setObjectName("InstructionsPrint");
	addDockWidget(Qt::RightDockWidgetArea, mPrintWidget);
	splitDockWidget(mGridWidget, mPrintWidget, Qt::Vertical);
	mPrintWidget->hide();

	// the print button shows and hides the print panel and stays checked while it is shown
	QAction* PrintAction = mPrintWidget->toggleViewAction();
	PrintAction->setToolTip(tr("Show or hide the Print panel to print the instruction pages"));
	mPropertiesWidget->AddOptionsAction(PrintAction);

	// the editor panel opens on demand below the page, wide enough for the lines of the file
	mEditorWidget = new swInstructionsEditorWidget(this, mProject, mInstructions);
	mEditorWidget->setObjectName("InstructionsEditor");
	addDockWidget(Qt::BottomDockWidgetArea, mEditorWidget);
	mEditorWidget->hide();

	QAction* EditorAction = mEditorWidget->toggleViewAction();
	EditorAction->setShortcut(QKeySequence(tr("Ctrl+Shift+E")));
	addAction(EditorAction);
	EditorAction->setToolTip(tr("Show or hide the Editor panel to inspect and edit the text of the model file"));
	mPropertiesWidget->AddOptionsAction(EditorAction);

	connect(mPageListWidget->mThumbnailsWidget, &QListWidget::currentRowChanged, this, &swInstructionsDialog::CurrentThumbnailChanged);
	UpdatePageList();

	// the pages panel snaps to its longest page name like the categories of the parts panel
	mPageListWidget->installEventFilter(this);
	installEventFilter(this);
	connect(mPageListWidget, &QDockWidget::dockLocationChanged, this, [this]()
	{
		QTimer::singleShot(0, this, &swInstructionsDialog::SnapPageListDock);
	});
	QTimer::singleShot(0, this, &swInstructionsDialog::SnapPageListDock);

	connect(mInstructions, &swInstructions::PageSetupChanged, this, &swInstructionsDialog::PageSetupChanged);
	connect(mInstructions, &swInstructions::PageLayoutChanged, this, &swInstructionsDialog::PageSetupChanged);

	// the panels come back where they were left
	QSettings Settings;
	Settings.beginGroup("InstructionsWindow");
	restoreState(Settings.value("State").toByteArray());
	Settings.endGroup();

	// the panels without a toggle are never lost to a layout saved while they were hidden
	for (QDockWidget* DockWidget : { static_cast<QDockWidget*>(mPropertiesWidget), static_cast<QDockWidget*>(mPlacementWidget), static_cast<QDockWidget*>(mGridWidget), static_cast<QDockWidget*>(mPageListWidget) })
		DockWidget->show();
}

// use to keep the panels where they are for the next time the window opens
void swInstructionsDialog::SaveLayout()
{
	QSettings Settings;
	Settings.beginGroup("InstructionsWindow");
	Settings.setValue("State", saveState());
	Settings.endGroup();
}

void swInstructionsDialog::closeEvent(QCloseEvent* Event)
{
	SaveLayout();

	QMainWindow::closeEvent(Event);
}

void swInstructionsDialog::ShowPrintWidget()
{
	mPrintWidget->show();
	mPrintWidget->raise();
}

void swInstructionsDialog::UpdatePageList()
{
	QListWidget* ThumbnailsWidget = mPageListWidget->mThumbnailsWidget;
	const int CurrentRow = qMax(ThumbnailsWidget->currentRow(), 0);

	// rebuild the page list and keep showing a page, the old pages no longer exist
	QSignalBlocker Blocker(ThumbnailsWidget);
	ThumbnailsWidget->clear();

	for (size_t PageNumber = 0; PageNumber < mInstructions->mPages.size(); PageNumber++)
	{
		QString Label = tr("Page %1").arg(swFormatIntegerLocalized(PageNumber + 1, locale()));

		switch (mInstructions->mPages[PageNumber].Type)
		{
		case swInstructionsPageType::Cover:
			Label = tr("%1 - Cover").arg(Label);
			break;

		case swInstructionsPageType::BillOfMaterials:
			Label = tr("%1 - Bill of Materials").arg(Label);
			break;

		case swInstructionsPageType::Blank:
			Label = tr("%1 - Blank").arg(Label);
			break;

		case swInstructionsPageType::Steps:
			break;
		}

		ThumbnailsWidget->addItem(Label);
	}

	const int Row = qMin(CurrentRow, ThumbnailsWidget->count() - 1);
	ThumbnailsWidget->setCurrentRow(Row);
	CurrentThumbnailChanged(Row);

	QTimer::singleShot(0, this, &swInstructionsDialog::SnapPageListDock);
}

bool swInstructionsDialog::eventFilter(QObject* Object, QEvent* Event)
{
	// resizing the pages panel by hand snaps it back once the mouse is released
	if (Object == mPageListWidget && Event->type() == QEvent::Resize && QApplication::mouseButtons() != Qt::NoButton)
		mPageListSnapPending = true;
	else if (Event->type() == QEvent::MouseButtonRelease && mPageListSnapPending)
	{
		mPageListSnapPending = false;
		QTimer::singleShot(0, this, &swInstructionsDialog::SnapPageListDock);
	}

	return QMainWindow::eventFilter(Object, Event);
}

void swInstructionsDialog::SnapPageListDock()
{
	if (mPageListWidget->isFloating())
		return;

	const Qt::DockWidgetArea Area = dockWidgetArea(mPageListWidget);

	if (Area != Qt::LeftDockWidgetArea && Area != Qt::RightDockWidgetArea)
		return;

	// other panels in the same stack keep their own width
	const QList<QDockWidget*> DockWidgets = findChildren<QDockWidget*>(QString(), Qt::FindDirectChildrenOnly);

	for (QDockWidget* DockWidget : DockWidgets)
		if (DockWidget != mPageListWidget && !DockWidget->isHidden() && !DockWidget->isFloating() && dockWidgetArea(DockWidget) == Area)
			return;

	// measure every page name, the list shows the longest one in full beside the scroll bar
	QListWidget* ThumbnailsWidget = mPageListWidget->mThumbnailsWidget;
	const int TextWidth = ThumbnailsWidget->sizeHintForColumn(0);

	if (TextWidth <= 0)
		return;

	const int Delta = TextWidth - ThumbnailsWidget->viewport()->width();

	if (!Delta)
		return;

	resizeDocks({ mPageListWidget }, { mPageListWidget->width() + Delta }, Qt::Horizontal);
}

void swInstructionsDialog::ModelChanged()
{
	// Several edits in a row rebuild the pages once, after the current event.
	if (mUpdatePending)
		return;

	mUpdatePending = true;

	QTimer::singleShot(0, this, [this]()
	{
		mUpdatePending = false;
		mInstructions->Update();
	});

	mEditorWidget->ScheduleUpdate();
}

void swInstructionsDialog::SetProject(Project* Project)
{
	mProject = Project;

	// the window stays open, the instructions are handed to the loaded project after this
	mPageWidget->scene()->clearSelection();
	mEditorWidget->SetProject(Project);
}

void swInstructionsDialog::PageSetupChanged()
{
	UpdatePageList();
}

void swInstructionsDialog::CurrentThumbnailChanged(int Index)
{
	if (Index < 0 || Index >= static_cast<int>(mInstructions->mPages.size()))
	{
		mPageWidget->SetCurrentPage(nullptr);
		return;
	}

	const swInstructionsPage* Page = &mInstructions->mPages[Index];

	mPageWidget->SetCurrentPage(Page);
	mPropertiesWidget->SetCurrentPage(Index);
}
