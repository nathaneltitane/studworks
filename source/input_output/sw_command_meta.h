#pragma once

// The meta command formats.

enum class swCommandMetaFormat
{
	StudWorx,
	LeoCAD
};

QString swReadCommandMeta(const QString& Line);
QByteArray swWriteCommandMeta(const QByteArray& Bytes, swCommandMetaFormat Format);
bool swHasLeoCADCommandMeta(const QByteArray& Bytes);
