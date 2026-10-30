#pragma once

#include <KIO/ThumbnailCreator>

// use to show ldraw models and parts in kde file managers, rendered by the studworx command line
class swThumbnailCreator : public KIO::ThumbnailCreator
{
	Q_OBJECT

public:
	swThumbnailCreator(QObject* Parent, const QVariantList& Arguments);

	KIO::ThumbnailResult create(const KIO::ThumbnailRequest& Request) override;

private:
	static bool IsLDrawFile(const QString& FileName);
	static QString GetProgramPath();
};
