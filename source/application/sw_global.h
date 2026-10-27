#ifndef SW_GLOBAL_H
#define SW_GLOBAL_H

#ifdef __cplusplus

#define QT_NO_DEPRECATED_WARNINGS
#include <QtGlobal>
#include <QtWidgets>
#include <QtConcurrent>
#include <QtOpenGL>
#include <QtGui>
#include <QWidget>
#include <QOpenGLWidget>
#include <QCryptographicHash>
#include <QPrinter>
#include <QPrintDialog>
#include <QSaveFile>
#include <map>
#include <vector>
#include <array>
#include <atomic>
#include <set>
#include <unordered_set>
#include <deque>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <cmath>

#if _MSC_VER
#pragma warning(default : 4062) // enumerator 'identifier' in switch of enum 'enumeration' is not handled
#pragma warning(default : 4388) // 'token' : signed/unsigned mismatch
#pragma warning(default : 4389) // 'equality-operator' : signed/unsigned mismatch
#pragma warning(default : 5038) // data member 'A::y' will be initialized after data member 'A::x'
#endif

#ifdef QT_NO_PRINTER
class QPrinter;
#endif

#ifndef Q_FALLTHROUGH
#define Q_FALLTHROUGH();
#endif

#ifndef QT_STRINGIFY
#define QT_STRINGIFY2(x) #x
#define QT_STRINGIFY(x) QT_STRINGIFY2(x)
#endif

#define SW_ARRAY_COUNT(x) ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))
#define SW_ARRAY_SIZE_CHECK(a,s) static_assert(SW_ARRAY_COUNT(a) == static_cast<int>(s), QT_STRINGIFY(a) " size mismatch.")

#if defined(EGL_VERSION_1_0) || defined(GL_ES_VERSION_2_0) || defined(GL_ES_VERSION_3_0) || defined(QT_OPENGL_ES)
#define SW_OPENGLES 1
#endif

// Old defines and declarations.
#define SW_MAXIMUM_PATH 1024
#define SW_MAXIMUM_NAME 1000

typedef quint32 swStep;
#define SW_STEP_MAXIMUM 0xffffffff

// Version number.
#define SW_VERSION_TEXT "2026-10-27"

// Forward declarations.
class Project;
class swModel;
enum class swGeometryBoundsMode;
class swObject;
class swPiece;
class swCamera;
class swLight;
enum class swLightType;
enum class swLightAreaShape;
class swGroup;
class swModelHistory;
class swPieceData;
class swSynthesisInformation;
class swTrainTrackInformation;
typedef std::map<const swPieceData*, std::map<int, int>> swPartsList;
struct swModelPartsEntry;
struct swMinifigure;
enum class swViewpoint;
enum class swShadingMode;
enum class swStudStyle;
class swInstructions;
struct swInstructionsPageSetup;
struct swObjectRayTest;
struct swObjectBoxTest;

class swVector2;
class swVector3;
class swVector4;
class swMatrix33;
class swMatrix44;

class swFindReplaceWidget;
struct swFindReplaceParameters;
class swCollapsibleWidget;
class swViewWidget;
class swView;
class swContext;
class swMesh;
struct swMeshSection;
struct swRenderMesh;
struct swObjectSection;
struct swPieceRayTest;
class swTexture;
class swScene;
class swViewManipulator;
class swViewSphere;
enum class swRenderMeshState : int;
enum class swTrackTool;
enum class swTrackButton;

class swThumbnailManager;
class swAssetLoader;
class swTextureManager;
class swPieceManager;

class swFile;
class swMemoryFile;
class swDiskFile;

#endif

#endif // SW_GLOBAL_H
