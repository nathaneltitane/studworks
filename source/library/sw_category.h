#pragma once

struct swLibraryCategory
{
	QString Name;
	QByteArray Keywords;
	bool Enabled = true;
};

extern std::vector<swLibraryCategory> gCategories;

void swResetDefaultCategories();
void swLoadDefaultCategories(bool BuiltInLibrary = false);
void swSaveDefaultCategories();

void swResetCategories(std::vector<swLibraryCategory>& Categories, bool BuiltInLibrary = false);
bool swLoadCategories(const QString& FileName, std::vector<swLibraryCategory>& Categories);
bool swLoadCategories(const QByteArray& Buffer, std::vector<swLibraryCategory>& Categories);
bool swSaveCategories(const QString& FileName, const std::vector<swLibraryCategory>& Categories);
bool swSaveCategories(QTextStream& Stream, const std::vector<swLibraryCategory>& Categories);

bool swMatchCategory(const char* PartName, const char* Expression);
