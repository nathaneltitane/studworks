#pragma once

char* lcstrcasestr(const char* s, const char* find);
char* lcstrupr(char* string);
size_t lcstrcpy(char* dest, const char* source, size_t size);
size_t lcstrcat(char* dest, const char* source, size_t size);

template<size_t size>
size_t lcstrcpy(char (&dest)[size], const char* source)
{
	return lcstrcpy(dest, source, size);
}

template<size_t size>
size_t lcstrcat(char (&dest)[size], const char* source)
{
	return lcstrcat(dest, source, size);
}

QString lcFormatDefaultName(const QString& Prefix, int Number);
int lcGetDefaultNameNumber(const QString& Name, const QString& Prefix);
