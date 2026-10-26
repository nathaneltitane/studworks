#pragma once

char* swstrcasestr(const char* s, const char* find);
char* swstrupr(char* string);
size_t swstrcpy(char* dest, const char* source, size_t size);
size_t swstrcat(char* dest, const char* source, size_t size);

template<size_t size>
size_t swstrcpy(char (&dest)[size], const char* source)
{
	return swstrcpy(dest, source, size);
}

template<size_t size>
size_t swstrcat(char (&dest)[size], const char* source)
{
	return swstrcat(dest, source, size);
}

QString swFormatDefaultName(const QString& Prefix, int Number);
int swGetDefaultNameNumber(const QString& Name, const QString& Prefix);
