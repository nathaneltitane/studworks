#pragma once

// Meta commands are "0 !SW <PROPERTY> <OBJECT>", see references/command-meta.md.
// LeoCAD meta commands are "0 !LEOCAD <OBJECT> <PROPERTY>".

enum class swCommandMetaFormat
{
	StudWorks,
	LeoCAD
};

QString swReadCommandMeta(const QString& Line);
QByteArray swWriteCommandMeta(const QByteArray& Bytes, swCommandMetaFormat Format);
bool swHasLeoCADCommandMeta(const QByteArray& Bytes);
