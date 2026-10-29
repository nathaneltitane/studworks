#pragma once

#include "sw_parser_shadow.h"

// The connection shapes of library parts, read from the LDCad shadow library and inherited from their sub-files.
class swShadowLibrary
{
public:
	// An empty path, or one without a shadow library, leaves all parts without connection shapes.
	static bool SetPath(const QString& Path);
	static bool IsLoaded();

	// PartName is a library file name such as "3001.dat" or "s/3001s01.dat".
	static const std::vector<swConnectionCylinder>& GetCylinders(const char* PartName);

private:
	static std::string NormalizeName(const std::string& Name);
	static const std::vector<swShadowLine>& GetShadowLines(const std::string& Name);
	static std::vector<swConnectionCylinder> GetOwnCylinders(const std::string& Name);
	static std::vector<std::pair<std::string, swMatrix44>> ReadReferences(const std::string& Name);
	static void AddPlacedCylinders(const std::vector<swConnectionCylinder>& Cylinders, const swMatrix44& Transform, std::vector<swConnectionCylinder>& Result);
	static void ApplyShadowLine(const swShadowLine& Line, std::vector<swConnectionCylinder>& Cylinders);

	static std::map<std::string, QString> mShadowFiles;
	static std::map<std::string, std::vector<swShadowLine>> mShadowLines;
	static std::map<std::string, std::vector<swConnectionCylinder>> mCylinders;
};
