#include "sw_global.h"
#include "sw_utils.h"
#include "sw_string.h"
#include "sw_modellistdialog.h"
#include "ui_sw_modellistdialog.h"
#include "project.h"
#include "sw_profile.h"
#include "sw_model.h"

enum class swModelListRole
{
	ExistingModel = Qt::UserRole,
	DuplicateModel
};

swModelListDialog::swModelListDialog(QWidget* Parent, const std::vector<std::unique_ptr<swModel>>& Models)
	: QDialog(Parent), ui(new Ui::swModelListDialog)
{
	mActiveModelItem = nullptr;

	ui->setupUi(this);
	swSetupDialogHelp(this, ui->buttonBox);

	connect(ui->NewModel, &QPushButton::clicked, this, &swModelListDialog::NewModelClicked);
	connect(ui->DeleteModel, &QPushButton::clicked, this, &swModelListDialog::DeleteModelClicked);
	connect(ui->RenameModel, &QPushButton::clicked, this, &swModelListDialog::RenameModelClicked);
	connect(ui->ExportModel, &QPushButton::clicked, this, &swModelListDialog::ExportModelClicked);
	connect(ui->DuplicateModel, &QPushButton::clicked, this, &swModelListDialog::DuplicateModelClicked);
	connect(ui->ModelList, &QListWidget::itemDoubleClicked, this, &swModelListDialog::ModelListItemDoubleClicked);
	connect(ui->ModelList, &QListWidget::itemSelectionChanged, this, &swModelListDialog::ModelListItemSelectionChanged);

	ui->ModelList->setDefaultDropAction(Qt::DropAction::MoveAction);
	ui->ModelList->setDragDropMode(QAbstractItemView::DragDropMode::InternalMove);

	swModel* ActiveModel = swGetActiveProject()->GetActiveModel();
	int ActiveModelIndex = -1;

	for (const std::unique_ptr<swModel>& Model : Models)
	{
		QListWidgetItem* Item = new QListWidgetItem(Model->GetProperties().mFileName);
		Item->setData(static_cast<int>(swModelListRole::ExistingModel), QVariant::fromValue<uintptr_t>((uintptr_t)Model.get()));
		ui->ModelList->addItem(Item);

		if (Model.get() == ActiveModel)
			ActiveModelIndex = ui->ModelList->count() - 1;
	}

	if (ActiveModelIndex != -1)
		ui->ModelList->setCurrentRow(ActiveModelIndex);

	QSettings Settings;
	ui->SetActiveModel->setChecked(Settings.value("Settings/ModelListSetActive", true).toBool());

	UpdateButtons();
}

swModelListDialog::~swModelListDialog()
{
	delete ui;
}

int swModelListDialog::GetActiveModelIndex() const
{
	return ui->ModelList->row(mActiveModelItem);
}

std::vector<swModelListDialogEntry> swModelListDialog::GetResults() const
{
	std::vector<swModelListDialogEntry> Models;
	Models.reserve(ui->ModelList->count());

	for (int ItemIdx = 0; ItemIdx < ui->ModelList->count(); ItemIdx++)
	{
		QListWidgetItem* Item = ui->ModelList->item(ItemIdx);

		swModelListDialogEntry Entry;

		Entry.Name = Item->text();
		Entry.ExistingModel = reinterpret_cast<swModel*>(Item->data(static_cast<int>(swModelListRole::ExistingModel)).value<uintptr_t>());
		Entry.DuplicateSource = reinterpret_cast<swModel*>(Item->data(static_cast<int>(swModelListRole::DuplicateModel)).value<uintptr_t>());

		Models.emplace_back(Entry);
	}

	return Models;
}

void swModelListDialog::UpdateButtons()
{
	int ModelCount = ui->ModelList->count();

	ui->DeleteModel->setEnabled(ModelCount > 1);
	ui->SetActiveModel->setEnabled(ui->ModelList->currentItem() != nullptr);
}

void swModelListDialog::accept()
{
	if (ui->SetActiveModel->isChecked())
		mActiveModelItem = ui->ModelList->currentItem();

	QSettings Settings;
	Settings.setValue("Settings/ModelListSetActive", ui->SetActiveModel->isChecked());

	QDialog::accept();
}

void swModelListDialog::NewModelClicked()
{
	QStringList ModelNames;

	for (int ItemIdx = 0; ItemIdx < ui->ModelList->count(); ItemIdx++)
		ModelNames.append(ui->ModelList->item(ItemIdx)->text());

	QString Name = swGetActiveProject()->GetNewModelName(this, tr("New Submodel"), QString(), ModelNames);

	if (Name.isEmpty())
		return;

	QListWidgetItem* Item = new QListWidgetItem(Name);
	ui->ModelList->addItem(Item);
	UpdateButtons();
}

void swModelListDialog::DeleteModelClicked()
{
	if (ui->ModelList->count() == 1)
	{
		QMessageBox::information(this, tr("Delete Submodel"), tr("The model cannot be empty."));
		return;
	}

	const QList<QListWidgetItem*> SelectedItems = ui->ModelList->selectedItems();

	if (SelectedItems.isEmpty())
	{
		QMessageBox::information(this, tr("Delete Submodel"), tr("No submodel selected."));
		return;
	}

	QString Prompt;
	if (SelectedItems.size() == 1)
		Prompt = tr("Are you sure you want to delete the submodel '%1'?").arg(SelectedItems[0]->text());
	else
		Prompt = tr("Are you sure you want to delete %n submodels?", nullptr, static_cast<int>(SelectedItems.size()));

	if (QMessageBox::question(this, tr("Delete Submodel"), Prompt, QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
		return;

	for (QListWidgetItem* SelectedItem : SelectedItems)
	{
		if (mActiveModelItem == SelectedItem)
			mActiveModelItem = nullptr;

		delete SelectedItem;
	}

	UpdateButtons();
}

void swModelListDialog::RenameModelClicked()
{
	const QList<QListWidgetItem*> SelectedItems = ui->ModelList->selectedItems();

	if (SelectedItems.isEmpty())
	{
		QMessageBox::information(this, tr("Rename Submodel"), tr("No submodel selected."));
		return;
	}

	for (QListWidgetItem* CurrentItem : SelectedItems)
	{
		QStringList ModelNames;

		for (int ItemIdx = 0; ItemIdx < ui->ModelList->count(); ItemIdx++)
			ModelNames.append(ui->ModelList->item(ItemIdx)->text());

		QString Name = swGetActiveProject()->GetNewModelName(this, tr("Rename Submodel"), CurrentItem->text(), ModelNames);

		if (!Name.isEmpty())
			CurrentItem->setText(Name);
	}
}

void swModelListDialog::ExportModelClicked()
{
	const QList<QListWidgetItem*> SelectedItems = ui->ModelList->selectedItems();

	if (SelectedItems.isEmpty())
	{
		QMessageBox::information(this, tr("Export Submodel"), tr("No submodel selected."));
		return;
	}

	if (SelectedItems.size() == 1)
	{
		QListWidgetItem* CurrentItem = SelectedItems[0];
		swModel* Model = (swModel*)CurrentItem->data(static_cast<int>(swModelListRole::ExistingModel)).value<uintptr_t>();

		if (!Model)
		{
			Model = (swModel*)CurrentItem->data(static_cast<int>(swModelListRole::DuplicateModel)).value<uintptr_t>();

			if (!Model)
			{
				QMessageBox::information(this, tr("Export Submodel"), tr("Nothing to export."));
				return;
			}
		}

		QString SaveFileName = QFileInfo(QDir(swGetProfileString(SW_PROFILE_PROJECTS_PATH)), CurrentItem->text()).absoluteFilePath();

		SaveFileName = QFileDialog::getSaveFileName(this, tr("Export Model"), SaveFileName, tr("Supported Files (*.ldr *.dat);;All Files (*.*)"));

		if (SaveFileName.isEmpty())
			return;

		swResult<void> ExportResult = swGetActiveProject()->ExportModel(SaveFileName, Model);

		if (!ExportResult && !ExportResult.error().isEmpty())
			QMessageBox::warning(this, tr("Export Submodel"), ExportResult.error());

		swSetProfileString(SW_PROFILE_PROJECTS_PATH, QFileInfo(SaveFileName).absolutePath());
	}
	else
	{
		QString Folder = QFileDialog::getExistingDirectory(this, tr("Select Export Folder"), swGetProfileString(SW_PROFILE_PROJECTS_PATH));

		for (QListWidgetItem* CurrentItem : SelectedItems)
		{
			swModel* Model = (swModel*)CurrentItem->data(static_cast<int>(swModelListRole::ExistingModel)).value<uintptr_t>();

			if (!Model)
				Model = (swModel*)CurrentItem->data(static_cast<int>(swModelListRole::DuplicateModel)).value<uintptr_t>();

			if (Model)
			{
				QString SaveFileName = QFileInfo(QDir(Folder), CurrentItem->text()).absoluteFilePath();
				swResult<void> ExportResult = swGetActiveProject()->ExportModel(SaveFileName, Model);

				if (!ExportResult && !ExportResult.error().isEmpty())
				{
					QMessageBox::warning(this, tr("Export Submodel"), ExportResult.error());
					break;
				}
			}
		}

		swSetProfileString(SW_PROFILE_PROJECTS_PATH, Folder);
	}
}

void swModelListDialog::DuplicateModelClicked()
{
	const QList<QListWidgetItem*> SelectedItems = ui->ModelList->selectedItems();

	if (SelectedItems.isEmpty())
	{
		QMessageBox::information(this, tr("Duplicate Submodel"), tr("No submodel selected."));
		return;
	}

	for (QListWidgetItem* CurrentItem : SelectedItems)
	{
		QStringList ModelNames;

		for (int ItemIdx = 0; ItemIdx < ui->ModelList->count(); ItemIdx++)
			ModelNames.append(ui->ModelList->item(ItemIdx)->text());

		// A copy keeps the name and takes the next free three digit number, Car.ldr becomes Car 001.ldr.
		QString BaseName = CurrentItem->text();
		QString Extension;
		const qsizetype ExtensionStart = BaseName.lastIndexOf(QLatin1Char('.'));

		if (ExtensionStart > 0)
		{
			Extension = BaseName.mid(ExtensionStart);
			BaseName.truncate(ExtensionStart);
		}

		static const QRegularExpression NumberSuffix(QLatin1String(" #?\\d+$"));
		BaseName.remove(NumberSuffix);

		const QString Prefix = BaseName + QLatin1Char(' ');
		int Number = 0;

		for (const QString& ModelName : ModelNames)
			Number = qMax(Number, swGetDefaultNameNumber(ModelName, Prefix));

		QString Name;

		do
		{
			Name = swFormatDefaultName(Prefix, ++Number) + Extension;
		} while (ModelNames.contains(Name, Qt::CaseInsensitive));

		Name = swGetActiveProject()->GetNewModelName(this, tr("Duplicate Submodel"), Name, ModelNames);

		if (Name.isEmpty())
			continue;

		QListWidgetItem* Item = new QListWidgetItem(Name);
		uintptr_t ExistingModel = CurrentItem->data(static_cast<int>(swModelListRole::ExistingModel)).value<uintptr_t>();
		if (!ExistingModel)
			ExistingModel = CurrentItem->data(static_cast<int>(swModelListRole::DuplicateModel)).value<uintptr_t>();
		Item->setData(static_cast<int>(swModelListRole::DuplicateModel), QVariant::fromValue<uintptr_t>(ExistingModel));
		ui->ModelList->addItem(Item);
	}

	UpdateButtons();
}

void swModelListDialog::ModelListItemDoubleClicked(QListWidgetItem* Item)
{
	mActiveModelItem = Item;

	accept();
}

void swModelListDialog::ModelListItemSelectionChanged()
{
	UpdateButtons();
}
