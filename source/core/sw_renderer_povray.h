#pragma once

enum class swPOVRayRenderQuality;

// A POV-Ray program and its version.
struct swPOVRayProgram
{
	QString Path;
	QString Version;
	int VersionMajor = 0;
	int VersionMinor = 0;
	bool Supported = false;
};

// Finds a supported POV-Ray.
swPOVRayProgram swFindPOVRay();
swPOVRayProgram swGetPOVRayProgram(const QString& Path);
QString swGetPOVRayMissingMessage(const swPOVRayProgram& Program);

// Asks for a missing POV-Ray.
swPOVRayProgram swSetupPOVRay(QWidget* Parent, const swPOVRayProgram& Found);

// Finds the standard include files.
bool swPOVRayHasIncludeFiles(const swPOVRayProgram& Program);

// Finds the LGEO library.
QString swFindLGEOPath();

// Offers to get a missing LGEO.
QString swSetupLGEO(QWidget* Parent);

// Arguments shared by every render.
QStringList swGetPOVRayArguments(const swPOVRayProgram& Program, swPOVRayRenderQuality Quality, bool UseLGEO);
QStringList swGetPOVRayEnvironment();
