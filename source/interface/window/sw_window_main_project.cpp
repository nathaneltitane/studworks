#include "sw_global.h"
#include "sw_window_main.h"
#include "sw_instructions.h"
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

void swMainWindow::ProjectFileChanged(const QString& Path)
{
	static bool Ignore;
	Project* CurrentProject = swGetActiveProject();

	if (Ignore || !CurrentProject || Path.isEmpty() || QFileInfo(Path) != QFileInfo(CurrentProject->GetFileName()))
		return;

	QString Text = tr("The file '%1' has been modified by another application. Do you want to reload it?").arg(QDir::toNativeSeparators(Path));

	const QString CurrentFileName = CurrentProject->GetFileName();

	Ignore = true;

	if (QMessageBox::question(this, tr("File Changed"), Text, QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
	{
		Ignore = false;

		if (CurrentProject == swGetActiveProject() && CurrentFileName == CurrentProject->GetFileName())
		{
			CurrentProject->MarkAsModified();
			UpdateTitle();
		}

		return;
	}

	if (CurrentProject != swGetActiveProject() || CurrentFileName != CurrentProject->GetFileName())
	{
		Ignore = false;
		return;
	}

	ReloadProjectFile(Path);

	Ignore = false;
}

void swMainWindow::ToggleWatchFileChanges()
{
	swGetPreferences().mWatchFileChanges = !swGetPreferences().mWatchFileChanges;
	swSetProfileInteger(SW_PROFILE_WATCH_FILE_CHANGES, swGetPreferences().mWatchFileChanges);

	swGetActiveProject()->UpdateFileWatcher();
}

void swMainWindow::ReloadProject()
{
	Project* CurrentProject = swGetActiveProject();

	if (!CurrentProject)
		return;

	const QString FileName = CurrentProject->GetFileName();

	if (FileName.isEmpty())
	{
		QMessageBox::information(this, tr("Reload"), tr("The current model has not been saved to a file yet."));
		return;
	}

	if (CurrentProject->IsModified())
	{
		QString Text = tr("The model has unsaved changes. Do you want to discard them and reload '%1'?").arg(QDir::toNativeSeparators(FileName));

		if (QMessageBox::question(this, tr("Reload"), Text, QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
			return;
	}

	ReloadProjectFile(FileName);
}

// use to reload the library
void swMainWindow::ReloadPartsLibrary()
{
	swPartsLibrary* Library = swGetPartsLibrary();
	const QString LibraryPath = Library->GetLibraryPath();

	if (!gApplication->ReloadPartsLibrary(LibraryPath, Library->IsOfficialLibrary()))
		QMessageBox::warning(this, tr("Reload Library"), tr("Could not open the parts library '%1'.").arg(QDir::toNativeSeparators(LibraryPath)));
}

void swMainWindow::ReloadProjectFile(const QString& FileName)
{
	std::unique_ptr<Project> NewProject = std::make_unique<Project>();

	if (NewProject->Load(FileName, true))
	{
		QByteArray TabLayout = GetTabLayout();
		gApplication->SetProject(NewProject.release());
		RestoreTabLayout(TabLayout);
		swView::UpdateAllViews();
	}
}

bool swMainWindow::LoadProjectText(const QByteArray& FileData)
{
	Project* CurrentProject = swGetActiveProject();
	std::unique_ptr<Project> NewProject = std::make_unique<Project>();

	if (!NewProject->LoadText(FileData, CurrentProject->GetFileName(), CurrentProject->GetMainModel()->GetProperties().mFileName))
	{
		QMessageBox::warning(mInstructionsDialog ? static_cast<QWidget*>(mInstructionsDialog) : this, tr("Editor"), tr("The text is not a model file."));
		return false;
	}

	// instructions window keeps its focus
	QPointer<swInstructionsDialog> Dialog = mInstructionsDialog;
	QPointer<QWidget> FocusWidget = QApplication::focusWidget();
	std::unique_ptr<swInstructions> Instructions;

	if (Dialog)
		Instructions = CurrentProject->TakeInstructions();

	mInstructionsDialog = nullptr;

	QByteArray TabLayout = GetTabLayout();
	gApplication->SetProject(NewProject.release());
	RestoreTabLayout(TabLayout);
	swView::UpdateAllViews();
	UpdateTitle();

	if (!Dialog)
		return true;

	Project* ActiveProject = swGetActiveProject();
	mInstructionsDialog = Dialog;
	Dialog->SetProject(ActiveProject);
	ActiveProject->SetInstructions(std::move(Instructions));

	Dialog->raise();
	Dialog->activateWindow();

	if (FocusWidget)
		FocusWidget->setFocus();

	return true;
}

void swMainWindow::AddRecentFile(const QString& FileName)
{
	QString SavedName = FileName;
	int MaxRecentFiles = static_cast<int>(mRecentFiles.size());
	int FileIndex;

	QFileInfo FileInfo(FileName);

	for (FileIndex = 0; FileIndex < MaxRecentFiles; FileIndex++)
		if (QFileInfo(mRecentFiles[FileIndex]) == FileInfo)
			break;

	for (FileIndex = swMinimum(FileIndex, MaxRecentFiles - 1); FileIndex > 0; FileIndex--)
		mRecentFiles[FileIndex] = mRecentFiles[FileIndex - 1];

	mRecentFiles[0] = SavedName;
}

void swMainWindow::RemoveRecentFile(size_t FileIndex)
{
	for (; FileIndex < mRecentFiles.size() - 1; FileIndex++)
		mRecentFiles[FileIndex] = mRecentFiles[FileIndex + 1];

	mRecentFiles[mRecentFiles.size() - 1].clear();
}

void swMainWindow::NewProject()
{
	if (!SaveProjectIfModified())
		return;

	// start from the default page setup
	if (swGetProfileInteger(SW_PROFILE_INSTRUCTIONS_NEW_MODEL_DEFAULTS))
		swInstructions::LoadDefaultPageSetup();

	Project* NewProject = new Project();
	gApplication->SetProject(NewProject);
}

bool swMainWindow::OpenProject(const QString& FileName)
{
	if (!SaveProjectIfModified())
		return false;

	QString LoadFileName = FileName;

	if (LoadFileName.isEmpty())
	{
		LoadFileName = swGetActiveProject()->GetFileName();

		if (LoadFileName.isEmpty())
			LoadFileName = swGetProfileString(SW_PROFILE_PROJECTS_PATH);

		LoadFileName = QFileDialog::getOpenFileName(this, tr("Open Model"), LoadFileName, tr("Supported Files (*.lcd *.ldr *.dat *.mpd);;All Files (*.*)"));

		if (LoadFileName.isEmpty())
			return false;

		swSetProfileString(SW_PROFILE_PROJECTS_PATH, QFileInfo(LoadFileName).absolutePath());
	}

	return OpenProjectFile(LoadFileName);
}

void swMainWindow::OpenRecentProject(int RecentFileIndex)
{
	if (!SaveProjectIfModified())
		return;

	if (!OpenProjectFile(mRecentFiles[RecentFileIndex]))
		RemoveRecentFile(RecentFileIndex);
}

bool swMainWindow::OpenProjectFile(const QString& FileName)
{
	Project* NewProject = new Project();

	if (NewProject->Load(FileName, true))
	{
		if (NewProject->HasLeoCADMetaCommands() && MigrateMetaCommands(FileName))
			NewProject->MigrateMetaCommands();

		// show the model once complete
		if (swGetPreferences().mLoadPartsBeforeShowing)
		{
			QApplication::setOverrideCursor(Qt::WaitCursor);
			NewProject->GetMainModel()->WaitForAssets();
			QApplication::restoreOverrideCursor();
		}

		gApplication->SetProject(NewProject);

		AddRecentFile(FileName);
		swView::UpdateProjectViews(NewProject);

		return true;
	}

	delete NewProject;
	swGetPartsLibrary()->RemoveTemporaryPartData();
	return false;
}

bool swMainWindow::MigrateMetaCommands(const QString& FileName)
{
	// migrate without asking
	if (swGetPreferences().mMigrateMetaCommands)
		return true;

	QMessageBox MessageBox(QMessageBox::Question, tr("Migrate Model"), tr("'%1' uses LeoCAD meta commands. Do you want to migrate it to StudWorks meta commands?").arg(QFileInfo(FileName).fileName()), QMessageBox::NoButton, this);
	MessageBox.setInformativeText(tr("A migrated model is saved with StudWorks meta commands, LeoCAD then ignores its cameras, lights, groups and part settings. A model kept as it is stays readable in LeoCAD."));
	QPushButton* MigrateButton = MessageBox.addButton(tr("Migrate"), QMessageBox::AcceptRole);
	MessageBox.addButton(tr("Keep"), QMessageBox::RejectRole);
	MessageBox.setDefaultButton(MigrateButton);
	QCheckBox* AutomaticCheckBox = new QCheckBox(tr("Migrate LeoCAD models automatically"));
	MessageBox.setCheckBox(AutomaticCheckBox);
	MessageBox.exec();

	if (MessageBox.clickedButton() != MigrateButton)
		return false;

	if (AutomaticCheckBox->isChecked())
	{
		swGetPreferences().mMigrateMetaCommands = true;
		swSetProfileInteger(SW_PROFILE_MIGRATE_META_COMMANDS, true);
	}

	return true;
}

void swMainWindow::ShowMergeDialog()
{
	QString InitialFolder = swGetActiveProject()->GetFileName();

	if (InitialFolder.isEmpty())
		InitialFolder = swGetProfileString(SW_PROFILE_PROJECTS_PATH);
	else
		InitialFolder = QFileInfo(InitialFolder).absolutePath();

	QStringList LoadFileNames = QFileDialog::getOpenFileNames(this, tr("Merge Model"), InitialFolder, tr("Supported Files (*.lcd *.ldr *.dat *.mpd);;All Files (*.*)"));

	if (LoadFileNames.isEmpty())
		return;

	swSetProfileString(SW_PROFILE_PROJECTS_PATH, QFileInfo(LoadFileNames.first()).absolutePath());

	std::vector<std::unique_ptr<Project>> ProjectsToMerge;

	for (const QString& LoadFileName : std::as_const(LoadFileNames))
	{
		std::unique_ptr<Project>& NewProject = ProjectsToMerge.emplace_back(std::make_unique<Project>());

		if (!NewProject->Load(LoadFileName, true))
			return;
	}

	size_t ModelCount = 0;
	std::vector<Project*> Sources;
	Sources.reserve(ProjectsToMerge.size());

	for (const std::unique_ptr<Project>& ProjectToMerge : ProjectsToMerge)
	{
		ModelCount += ProjectToMerge->GetModels().size();
		Sources.push_back(ProjectToMerge.get());
	}

	const swResult<void> MergeResult = swGetActiveProject()->Merge(Sources);

	if (!MergeResult)
	{
		QMessageBox::warning(this, tr("Cannot Merge Model"), MergeResult.error());
		return;
	}

	QMessageBox::information(this, tr("Merge Model"), tr("Merged %n submodels.", nullptr, static_cast<int>(ModelCount)));

	UpdateModels();
}

void swMainWindow::ImportLDD()
{
	if (!SaveProjectIfModified())
		return;

	QString LoadFileName = QFileDialog::getOpenFileName(this, tr("Import"), QString(), tr("LEGO® Digital Designer Files (*.lxf);;All Files (*.*)"));
	if (LoadFileName.isEmpty())
		return;

	Project* NewProject = new Project();

	if (NewProject->ImportLDD(LoadFileName))
	{
		gApplication->SetProject(NewProject);
		swView::UpdateProjectViews(NewProject);
	}
	else
	{
		delete NewProject;
		swGetPartsLibrary()->RemoveTemporaryPartData();
	}
}

void swMainWindow::ImportInventory()
{
	if (!SaveProjectIfModified())
		return;

	swSetsDatabaseDialog Dialog(this);
	if (Dialog.exec() != QDialog::Accepted)
		return;

	Project* NewProject = new Project();
	std::vector<swSetInventoryItem> SetInventory = Dialog.GetSetInventory();

	if (NewProject->ImportInventory(SetInventory, Dialog.GetSetName(), Dialog.GetSetDescription()))
	{
		gApplication->SetProject(NewProject);
		swView::UpdateProjectViews(NewProject);
	}
	else
		delete NewProject;
}

bool swMainWindow::SaveProject(const QString& FileName)
{
	QString SaveFileName = FileName;
	Project* Project = swGetActiveProject();

	if (SaveFileName.isEmpty())
	{
		SaveFileName = Project->GetFileName();

		if (SaveFileName.isEmpty())
			SaveFileName = QFileInfo(QDir(swGetProfileString(SW_PROFILE_PROJECTS_PATH)), Project->GetTitle()).absoluteFilePath();

		QString Filter = (Project->GetModels().size() > 1) ? tr("Supported Files (*.mpd);;All Files (*.*)") : tr("Supported Files (*.ldr *.dat *.mpd);;All Files (*.*)");

		SaveFileName = QFileDialog::getSaveFileName(this, tr("Save Model"), SaveFileName, Filter);

		if (SaveFileName.isEmpty())
			return false;

		swSetProfileString(SW_PROFILE_PROJECTS_PATH, QFileInfo(SaveFileName).absolutePath());
	}

	if (QFileInfo(SaveFileName).suffix().toLower() == QLatin1String("lcd"))
	{
		QMessageBox::warning(this, tr("Save Model"), tr("Saving files in LCD format is no longer supported. Please use the LDR or MPD formats instead."));
		return false;
	}

	swResult<void> SaveResult = Project->Save(SaveFileName);

	if (!SaveResult)
	{
		QMessageBox::warning(this, tr("Save Model"), SaveResult.error());
		return false;
	}

	AddRecentFile(SaveFileName);
	UpdateTitle();

	return true;
}

bool swMainWindow::SaveProjectIfModified()
{
	Project* Project = swGetActiveProject();
	if (!Project->IsModified())
		return true;

	switch (QMessageBox::question(this, tr("Save Model"), tr("Save changes to '%1'?").arg(Project->GetTitle()), QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel))
	{
	default:
	case QMessageBox::Cancel:
		return false;

	case QMessageBox::Yes:
		if (!SaveProject(Project->GetFileName()))
			return false;
		break;

	case QMessageBox::No:
		break;
	}

	return true;
}

bool swMainWindow::SetModelFromFocus()
{
	swObject* FocusObject = GetActiveModel()->GetFocusObject();

	if (!FocusObject || !FocusObject->IsPart())
		return false;

	swModel* Model = ((swPart*)FocusObject)->mPartData->GetModel();

	if (Model)
	{
		Project* Project = swGetActiveProject();
		Project->SetActiveModel(Model, true);
		return true;
	}

	return false;
}

void swMainWindow::SetModelFromSelection()
{
	if (SetModelFromFocus())
		return;

	swModel* Model = GetActiveModel()->GetFirstSelectedSubmodel();

	if (Model)
	{
		Project* Project = swGetActiveProject();
		Project->SetActiveModel(Model, true);
	}
}
