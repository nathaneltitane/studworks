#pragma once

#include <QNetworkRequest>
#include <QNetworkReply>

#ifdef Q_OS_WIN

class swHttpReply : public QThread
{
	Q_OBJECT

public:
	swHttpReply(QObject* Parent, const QString& URL);

	void run() override;

	bool error() const
	{
		return mError;
	}

	void abort()
	{
		mAbort = true;
	}

	QByteArray readAll() const
	{
		return mBuffer;
	}

protected:
	bool mError;
	bool mAbort;
	QByteArray mBuffer;
	QString mURL;
};

class swHttpManager : public QObject
{
	Q_OBJECT

public:
	swHttpManager(QObject* Owner = nullptr);

	swHttpReply* DownloadFile(const QString& Url);

signals:
	void DownloadFinished(swHttpReply* Reply);
};

#else

class swHttpReply : public QNetworkReply
{
	Q_OBJECT

	swHttpReply(QObject* Parent)
		: QNetworkReply(Parent)
	{
	}
};

class swHttpManager : public QNetworkAccessManager
{
	Q_OBJECT

public:
	swHttpManager(QObject* Owner = nullptr);

	swHttpReply* DownloadFile(const QString& Url);

signals:
	void DownloadFinished(swHttpReply* Reply);

protected slots:
	void Finished(QNetworkReply* Reply);
};

#endif
