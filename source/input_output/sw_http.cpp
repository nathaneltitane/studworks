#include "sw_global.h"
#include "sw_http.h"

#ifdef Q_OS_WIN

#include <windows.h>
#include <wininet.h>

swHttpReply::swHttpReply(QObject* Parent, const QString& URL)
	: QThread(Parent)
{
	mError = true;
	mAbort = false;
	mURL = URL;
}

void swHttpReply::run()
{
	HINTERNET Session = nullptr;
	HINTERNET Request = nullptr;

	static_assert(sizeof(wchar_t) == sizeof(QChar), "Character size mismatch");

	Session = InternetOpen(L"StudWorx", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);

	if (!Session)
		return;

	Request = InternetOpenUrl(Session, (WCHAR*)mURL.data(), NULL, 0, INTERNET_FLAG_IGNORE_REDIRECT_TO_HTTP, 0);

	if (!Request)
	{
		InternetCloseHandle(Session);
		return;
	}

	for (;;)
	{
		char Buffer[1024];
		DWORD BytesRead;

		if (mAbort)
			break;

		if (!InternetReadFile(Request, Buffer, sizeof(Buffer), &BytesRead))
			break;

		if (BytesRead)
			mBuffer.append(Buffer, BytesRead);
		else
		{
			mError = false;
			break;
		}

		Sleep(0);
	}

	InternetCloseHandle(Request);
	InternetCloseHandle(Session);
}

swHttpManager::swHttpManager(QObject* Owner)
	: QObject(Owner)
{
}

swHttpReply* swHttpManager::DownloadFile(const QString& Url)
{
	swHttpReply* Reply = new swHttpReply(this, Url);
	connect(Reply, &QThread::finished, [this, Reply] { emit DownloadFinished(Reply); });
	Reply->start();
	return Reply;
}

#else

swHttpManager::swHttpManager(QObject* Owner)
	: QNetworkAccessManager(Owner)
{
	connect(this, &QNetworkAccessManager::finished, this, &swHttpManager::Finished);
}

swHttpReply* swHttpManager::swHttpManager::DownloadFile(const QString& Url)
{
	QNetworkRequest Request = QNetworkRequest(QUrl(Url));

#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0)) // default changed in Qt6
#if (QT_VERSION >= QT_VERSION_CHECK(5, 9, 0))
	Request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
#elif (QT_VERSION >= QT_VERSION_CHECK(5, 6, 0))
	Request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
#endif
#endif

	return (swHttpReply*)get(Request);
}

void swHttpManager::Finished(QNetworkReply* Reply)
{
	emit DownloadFinished((swHttpReply*)Reply);
}

#endif

// use to download a file with progress
std::optional<QByteArray> swDownloadWithProgress(QWidget* Parent, const QString& Url, const QString& Title, const QString& Label)
{
	QProgressDialog ProgressDialog(Label, QCoreApplication::translate("swHttpManager", "Cancel"), 0, 0, Parent);
	ProgressDialog.setWindowTitle(Title);
	ProgressDialog.setWindowModality(Qt::WindowModal);
	ProgressDialog.setMinimumDuration(0);
	ProgressDialog.setAutoClose(false);
	ProgressDialog.setAutoReset(false);

	// room for the full title
	ProgressDialog.setMinimumWidth(ProgressDialog.fontMetrics().averageCharWidth() * 60);

	swHttpManager HttpManager;
	QEventLoop EventLoop;
	swHttpReply* Reply = HttpManager.DownloadFile(Url);

	QObject::connect(&HttpManager, &swHttpManager::DownloadFinished, &EventLoop, &QEventLoop::quit);
	QObject::connect(&ProgressDialog, &QProgressDialog::canceled, &EventLoop, [Reply] { Reply->abort(); });

#ifndef Q_OS_WIN
	QObject::connect(Reply, &QNetworkReply::downloadProgress, &ProgressDialog, [&ProgressDialog](qint64 BytesReceived, qint64 BytesTotal)
	{
		if (BytesTotal <= 0)
			return;

		// kilobytes keep the range inside int
		ProgressDialog.setMaximum(static_cast<int>(BytesTotal / 1024));
		ProgressDialog.setValue(static_cast<int>(BytesReceived / 1024));
	});
#endif

	ProgressDialog.show();
	EventLoop.exec();
	ProgressDialog.hide();

	const bool Canceled = ProgressDialog.wasCanceled();
	const QByteArray Data = !Canceled && !Reply->error() ? Reply->readAll() : QByteArray();

	Reply->deleteLater();

	if (Canceled)
		return std::nullopt;

	return Data;
}
