#pragma once

class swHttpReply;
class swHttpManager;
struct swSetInventoryItem;

namespace Ui {
class swSetsDatabaseDialog;
}

class swSetsDatabaseDialog : public QDialog
{
	Q_OBJECT

public:
	explicit swSetsDatabaseDialog(QWidget* Parent);
	virtual ~swSetsDatabaseDialog();

	QString GetSetName() const;
	QString GetSetDescription() const;
	std::vector<swSetInventoryItem> GetSetInventory() const;

	bool eventFilter(QObject* Object, QEvent* Event) override;

public slots:
	void DownloadFinished(swHttpReply* Reply);
	void SearchButtonClicked();
	void accept() override;
	void Finished(int Result);

protected:
	swHttpManager* mHttpManager;

	swHttpReply* mKeyListReply;
	swHttpReply* mSearchReply;
	swHttpReply* mInventoryReply;
	QStringList mKeys;
	QByteArray mInventory;

	Ui::swSetsDatabaseDialog* ui;
};
