#pragma once

// Writes looping GIF89a animations without image plugins.
class swGifWriter
{
public:
	swGifWriter(const QString& FileName, const QSize& Size);

	bool Open();

	// Delay is in hundredths of a second. Transparent frames keep clear pixels clear.
	bool AddFrame(const QImage& Image, int Delay, bool Transparent);
	bool Close();

	QString GetErrorString() const
	{
		return mFile.errorString();
	}

protected:
	void AppendWord(QByteArray& Data, int Value) const;
	void AppendImageData(QByteArray& Data, const std::vector<quint8>& Indices) const;

	QFile mFile;
	QSize mSize;
};
