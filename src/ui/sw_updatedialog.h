#pragma once

class swHttpReply;
class swHttpManager;

namespace Ui {
class swUpdateDialog;
}

void swDoInitialUpdateCheck();

class swUpdateDialog : public QDialog
{
	Q_OBJECT

public:
	swUpdateDialog(QWidget* Parent, bool InitialUpdate);
	virtual ~swUpdateDialog();

	void ParseUpdate(const char* Update);

public slots:
	void DownloadFinished(swHttpReply* Reply);
	void accept() override;
	void finished(int result);

private:
	Ui::swUpdateDialog* ui;

	swHttpManager* mHttpManager;
	QByteArray mVersionData;
	bool mInitialUpdate;
};
