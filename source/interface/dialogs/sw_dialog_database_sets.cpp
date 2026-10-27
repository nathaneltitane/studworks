#include "sw_global.h"
#include "sw_window_utilities.h"
#include "sw_dialog_database_sets.h"
#include "ui_sw_dialog_database_sets.h"
#include "sw_http.h"
#include "sw_project.h"
#include "sw_profile.h"

swSetsDatabaseDialog::swSetsDatabaseDialog(QWidget* Parent)
	: QDialog(Parent),
    ui(new Ui::swSetsDatabaseDialog)
{
	ui->setupUi(this);
	swSetupDialogHelp(this, ui->buttonBox);
	ui->SearchEdit->installEventFilter(this);

	mHttpManager = new swHttpManager(this);

	connect(ui->SetsTree, &QTreeWidget::itemDoubleClicked, this, &swSetsDatabaseDialog::accept);
	connect(this, &QDialog::finished, this, &swSetsDatabaseDialog::Finished);
	connect(mHttpManager, &swHttpManager::DownloadFinished, this, &swSetsDatabaseDialog::DownloadFinished);
	connect(ui->SearchButton, &QPushButton::clicked, this, &swSetsDatabaseDialog::SearchButtonClicked);

	mApiKey = swGetProfileString(SW_PROFILE_REBRICKABLE_API_KEY);
}

bool swSetsDatabaseDialog::EnsureApiKey()
{
	if (!mApiKey.isEmpty())
		return true;

	// Set inventories come from Rebrickable, which needs a personal API key from the account settings.
	bool Ok = false;
	const QString Key = QInputDialog::getText(this, tr("Rebrickable API Key"), tr("Set inventories are downloaded from Rebrickable.\nEnter your Rebrickable API key, available for free from your account settings at rebrickable.com:"), QLineEdit::Normal, QString(), &Ok).trimmed();

	if (!Ok || Key.isEmpty())
		return false;

	mApiKey = Key;
	swSetProfileString(SW_PROFILE_REBRICKABLE_API_KEY, mApiKey);

	return true;
}

swSetsDatabaseDialog::~swSetsDatabaseDialog()
{
	delete ui;
}

QString swSetsDatabaseDialog::GetSetName() const
{
	QTreeWidgetItem* Current = ui->SetsTree->currentItem();
	return Current ? Current->text(0) : QString();
}

QString swSetsDatabaseDialog::GetSetDescription() const
{
	QTreeWidgetItem* Current = ui->SetsTree->currentItem();
	return Current ? Current->text(1) : QString();
}

std::vector<swSetInventoryItem> swSetsDatabaseDialog::GetSetInventory() const
{
	std::vector<swSetInventoryItem> Inventory;

	if (mInventory.isEmpty())
		return Inventory;

	QJsonDocument Document = QJsonDocument::fromJson(mInventory);
	QJsonObject Root = Document.object();
	const QJsonArray Pieces = Root["results"].toArray();

	for (const QJsonValue& Piece : Pieces)
	{
		QJsonObject PieceObject = Piece.toObject();
		QByteArray PieceID = PieceObject["part"].toObject()["part_num"].toString().toLatin1();
		QJsonArray PieceIDArray = PieceObject["part"].toObject()["external_ids"].toObject()["LDraw"].toArray();

		if (!PieceIDArray.isEmpty())
			PieceID = PieceIDArray.first().toString().toLatin1();

		int Quantity = PieceObject["quantity"].toInt();
		int ColorCode = 16;

		QJsonArray ColorArray = PieceObject["color"].toObject()["external_ids"].toObject()["LDraw"].toObject()["ext_ids"].toArray();

		if (!ColorArray.isEmpty())
			ColorCode = ColorArray.first().toInt();

		Inventory.push_back({ PieceID, Quantity, ColorCode });
	}

	return Inventory;
}

bool swSetsDatabaseDialog::eventFilter(QObject* Object, QEvent* Event)
{
	if (Event->type() == QEvent::KeyPress)
	{
		QKeyEvent* KeyEvent = static_cast<QKeyEvent*>(Event);

		int Key = KeyEvent->key();
		if (Key == Qt::Key_Return || Key == Qt::Key_Enter)
		{
			KeyEvent->accept();
			SearchButtonClicked();
			return true;
		}
	}

	return QDialog::eventFilter(Object, Event);
}

void swSetsDatabaseDialog::accept()
{
	QTreeWidgetItem* Current = ui->SetsTree->currentItem();
	if (!Current)
	{
		QMessageBox::information(this, tr("Import Set Inventory"), tr("Please select a set from the list."));
		return;
	}

	QString SetNum = Current->text(0);

	if (!EnsureApiKey())
		return;

	QProgressDialog ProgressDialog(this);
	ProgressDialog.setWindowTitle(tr("Downloading"));
	ProgressDialog.setLabelText(tr("Downloading set inventory"));
	ProgressDialog.setMaximum(0);
	ProgressDialog.setMinimum(0);
	ProgressDialog.setValue(0);
	ProgressDialog.show();

	QString DownloadUrl = QString("https://rebrickable.com/api/v3/lego/sets/%1/parts/?key=%2&page_size=1000").arg(SetNum, mApiKey);

	mInventoryReply = mHttpManager->DownloadFile(DownloadUrl);

	while (mInventoryReply)
	{
		QApplication::processEvents();

		if (ProgressDialog.wasCanceled())
		{
			mInventoryReply->abort();
			mInventoryReply->deleteLater();
			mInventoryReply = nullptr;
			return;
		}
	}

	QDialog::accept();
}

void swSetsDatabaseDialog::Finished(int Result)
{
	Q_UNUSED(Result);
}

void swSetsDatabaseDialog::SearchButtonClicked()
{
	QString Keyword = ui->SearchEdit->text();

	if (Keyword.isEmpty())
	{
		QMessageBox::information(this, tr("Search Sets"), tr("Keyword cannot be empty."));
		return;
	}

	if (!EnsureApiKey())
		return;

	QProgressDialog ProgressDialog(this);
	ProgressDialog.setWindowTitle(tr("Searching"));
	ProgressDialog.setLabelText(tr("Connecting to server"));
	ProgressDialog.setMaximum(0);
	ProgressDialog.setMinimum(0);
	ProgressDialog.setValue(0);
	ProgressDialog.show();

	QString SearchUrl = QString("https://rebrickable.com/api/v3/lego/sets/?search=%1&key=%2").arg(Keyword, mApiKey);

	mSearchReply = mHttpManager->DownloadFile(SearchUrl);

	while (mSearchReply)
	{
		QApplication::processEvents();

		if (ProgressDialog.wasCanceled())
		{
			mSearchReply->abort();
			mSearchReply->deleteLater();
			mSearchReply = nullptr;
			break;
		}
	}
}

void swSetsDatabaseDialog::DownloadFinished(swHttpReply* Reply)
{
	if (Reply == mSearchReply)
	{
		QTreeWidget* SetsTree = ui->SetsTree;
		SetsTree->clear();

		if (!Reply->error())
		{
			QJsonDocument Document = QJsonDocument::fromJson(Reply->readAll());
			QJsonObject Root = Document.object();
			const QJsonArray Sets = Root["results"].toArray();

			for (const QJsonValue& Set : Sets)
			{
				QJsonObject SetObject = Set.toObject();
				QStringList SetInfo;

				SetInfo << SetObject["set_num"].toString();
				SetInfo << SetObject["name"].toString();
				SetInfo << QString::number(SetObject["year"].toInt());
				SetInfo << swFormatIntegerLocalized(SetObject["num_parts"].toInt(), locale());

				new QTreeWidgetItem(ui->SetsTree, SetInfo);
			}

			if (!Sets.isEmpty())
			{
				SetsTree->resizeColumnToContents(0);
				SetsTree->resizeColumnToContents(1);
				SetsTree->resizeColumnToContents(2);
				SetsTree->resizeColumnToContents(3);
				SetsTree->setCurrentItem(SetsTree->topLevelItem(0));
			}
		}

		mSearchReply = nullptr;
	}
	else if (Reply == mInventoryReply)
	{
		if (!Reply->error())
			mInventory = Reply->readAll();
		else
			QMessageBox::warning(this, tr("Download Set Inventory"), tr("Error downloading set inventory."));

		mInventoryReply = nullptr;
	}

	Reply->deleteLater();
}
