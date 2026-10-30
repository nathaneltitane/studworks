#pragma once

enum class swPOVRayRenderQuality;

// A POV-Ray program and the version it reports, older than 3.7 is not supported.
struct swPOVRayProgram
{
	QString Path;
	QString Version;
	int VersionMajor = 0;
	int VersionMinor = 0;
	bool Supported = false;
};

// Finds POV-Ray in the Preferences path, the PATH, Flatpak and Snap, a supported program found elsewhere becomes the Preferences path.
swPOVRayProgram swFindPOVRay();
swPOVRayProgram swGetPOVRayProgram(const QString& Path);
QString swGetPOVRayMissingMessage(const swPOVRayProgram& Program);

// Asks to install or choose POV-Ray when none was found, like the parts library setup on the first start.
swPOVRayProgram swSetupPOVRay(QWidget* Parent, const swPOVRayProgram& Found);

// The standard include files (colors.inc and the rest) are a separate package on some systems and LGEO parts need them.
bool swPOVRayHasIncludeFiles(const swPOVRayProgram& Program);

// Finds the LGEO library in the Preferences path or beside the parts library and in the usual folders, a library found elsewhere becomes the Preferences path.
QString swFindLGEOPath();

// Offers to download LGEO or choose its folder when none was found, an empty path means the render uses LDraw parts.
QString swSetupLGEO(QWidget* Parent);

// Arguments shared by every render: quality, display, output format and the include folders.
QStringList swGetPOVRayArguments(const swPOVRayProgram& Program, swPOVRayRenderQuality Quality, bool UseLGEO);
QStringList swGetPOVRayEnvironment();
