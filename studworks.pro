QT += core gui widgets opengl network xml concurrent
QT *= printsupport
TEMPLATE = app

lessThan(QT_MAJOR_VERSION, 5) {
	error("StudWorks requires Qt5.4 or later.")
}

equals(QT_MAJOR_VERSION, 5):lessThan(QT_MINOR_VERSION, 4) {
	error("StudWorks requires Qt5.4 or later.")
}

greaterThan(QT_MAJOR_VERSION, 5) {
	QT += openglwidgets
	win32-msvc* {
		QMAKE_CXXFLAGS += /std:c++17
	}
}

qtHaveModule(gamepad) {
	QT += gamepad
	DEFINES += SW_ENABLE_GAMEPAD
}

# Optional embedded Konsole terminal on systems with KDE Frameworks, left out of bundled packages.
qtHaveModule(KParts):isEmpty(BUNDLE_LIBRARIES) {
	QT += KParts
	DEFINES += SW_ENABLE_TERMINAL
}

CONFIG += precompile_header incremental c++17 force_debug_info

# version from the release date in sw_global.h
VERSION_LINES = $$cat($$PWD/source/application/sw_global.h, lines)
VERSION_TEXT = $$find(VERSION_LINES, SW_VERSION_TEXT)
VERSION_TEXT = $$replace(VERSION_TEXT, ".*\"([0-9-]+)\".*", \\1)
VERSION_PARTS = $$split(VERSION_TEXT, -)
VERSION_RC = $$num_add($$member(VERSION_PARTS, 0), 0),$$num_add($$member(VERSION_PARTS, 1), 0),$$num_add($$member(VERSION_PARTS, 2), 0),0
DESCRIPTION_TEXT = StudWorks

win32 {
	DEFINES += NOMINMAX
	ICON_FILE = $$PWD/resources/studworks.ico

	# the resource file carries the release date as its version text
	version_rc.input = $$PWD/platform/studworks.rc.in
	version_rc.output = $$OUT_PWD/studworks.rc
	QMAKE_SUBSTITUTES += version_rc
	RC_FILE = $$OUT_PWD/studworks.rc

	LIBS += -lwininet
}
win32-msvc* {
	QMAKE_CXXFLAGS_WARN_ON += /W4 -wd4100
	QMAKE_CXXFLAGS *= /MP
	PRECOMPILED_HEADER = source/application/sw_global.h
	PRECOMPILED_SOURCE = source/application/sw_global.cpp
	DEFINES += _CRT_SECURE_NO_WARNINGS _CRT_SECURE_NO_DEPRECATE=1 _CRT_NONSTDC_NO_WARNINGS=1
	INCLUDEPATH += $$[QT_INSTALL_HEADERS]/QtZlib
	QMAKE_LFLAGS += /INCREMENTAL
	LIBS += -ladvapi32 -lshell32 -lopengl32 -luser32
	QMAKE_CXXFLAGS += /we4834
} else {
	PRECOMPILED_HEADER = source/application/sw_global.h
	LIBS += -lz
	QMAKE_CXXFLAGS_WARN_ON += -Wno-unused-parameter -Wshadow
	QMAKE_CXXFLAGS += -Werror=unused-result
}

isEmpty(QMAKE_LRELEASE) {
	win32:QMAKE_LRELEASE = $$[QT_INSTALL_BINS]\\lrelease.exe
	else:QMAKE_LRELEASE = $$[QT_INSTALL_BINS]/lrelease
	unix {
		!exists($$QMAKE_LRELEASE) { QMAKE_LRELEASE = lrelease-qt4 }
	} else {
		!exists($$QMAKE_LRELEASE) { QMAKE_LRELEASE = lrelease }
	}
}

TRAVIS_TAG = $$(TRAVIS_TAG)
TRAVIS_COMMIT = $$(TRAVIS_COMMIT)
!isEmpty(TRAVIS_COMMIT) {
	isEmpty(TRAVIS_TAG) {
		DEFINES += "SW_CONTINUOUS_BUILD=$$system(git rev-parse --short HEAD)"
	}
}

APPVEYOR_REPO_TAG = $$(APPVEYOR_REPO_TAG)
APPVEYOR_REPO_COMMIT = $$(APPVEYOR_REPO_COMMIT)
!isEmpty(APPVEYOR_REPO_COMMIT) {
	isEmpty(APPVEYOR_REPO_TAG) {
		DEFINES += "SW_CONTINUOUS_BUILD=$$system(git rev-parse --short HEAD)"
		DESCRIPTION_TEXT = "StudWorks Continuous Build $$system(git rev-parse --short HEAD)"
	}
}

TSFILES = resources/translations/studworks_cs.ts resources/translations/studworks_de.ts resources/translations/studworks_en.ts resources/translations/studworks_es.ts resources/translations/studworks_fr.ts resources/translations/studworks_pt.ts resources/translations/studworks_ru.ts resources/translations/studworks_uk.ts resources/translations/studworks_zh_CN.ts resources/translations/studworks_zh_TW.ts
lrelease.input = TSFILES
lrelease.output = ${QMAKE_FILE_PATH}/${QMAKE_FILE_BASE}.qm
lrelease.commands = $$QMAKE_LRELEASE -silent ${QMAKE_FILE_IN} -qm ${QMAKE_FILE_PATH}/${QMAKE_FILE_BASE}.qm
lrelease.CONFIG += no_link target_predeps
QMAKE_EXTRA_COMPILERS += lrelease

system($$QMAKE_LRELEASE -silent $$TSFILES)

unix:!macx {
	TARGET = studworks
} else {
	TARGET = StudWorks
}

CONFIG(debug, debug|release) {
	DESTDIR = build/debug
} else {
	DESTDIR = build/release
}

OBJECTS_DIR = $$DESTDIR/.obj
MOC_DIR = $$DESTDIR/.moc
RCC_DIR = $$DESTDIR/.qrc
UI_DIR = $$DESTDIR/.ui

win32 {
	# the parts library and povray are copied beside the executable only when present
	exists($$_PRO_FILE_PWD_/library.bin) {
		library.files = $$_PRO_FILE_PWD_/library.bin
		library.path = $$OUT_PWD/$$DESTDIR
		COPIES += library
	}

	exists($$_PRO_FILE_PWD_/povconsole32-sse2.exe) {
		povray.files = $$_PRO_FILE_PWD_/povconsole32-sse2.exe
		povray.path = $$OUT_PWD/$$DESTDIR
		COPIES += povray
	}
}

unix:!macx {
	isEmpty(INSTALL_PREFIX):INSTALL_PREFIX = /usr
	isEmpty(BIN_DIR):BIN_DIR = $$INSTALL_PREFIX/bin
	isEmpty(DOCS_DIR):DOCS_DIR = $$INSTALL_PREFIX/share/doc/studworks
	isEmpty(ICON_DIR):ICON_DIR = $$INSTALL_PREFIX/share/icons
	isEmpty(MAN_DIR):MAN_DIR = $$INSTALL_PREFIX/share/man/man1
	isEmpty(DESKTOP_DIR):DESKTOP_DIR = $$INSTALL_PREFIX/share/applications
	isEmpty(MIME_DIR):MIME_DIR = $$INSTALL_PREFIX/share/mime/packages
	isEmpty(APPDATA_DIR):APPDATA_DIR = $$INSTALL_PREFIX/share/metainfo

	target.path = $$BIN_DIR
	docs.path = $$DOCS_DIR
	docs.files = documentation/README.md documentation/CREDITS.txt documentation/COPYING.txt
	man.path = $$MAN_DIR
	man.files = documentation/studworks.1
	desktop.path = $$DESKTOP_DIR
	desktop.files = platform/studworks.desktop
	icons.path = $$ICON_DIR/hicolor
	icons.files += resources/icons/128x128
	icons.files += resources/icons/16x16
	icons.files += resources/icons/24x24
	icons.files += resources/icons/256x256
	icons.files += resources/icons/32x32
	icons.files += resources/icons/48x48
	icons.files += resources/icons/512x512
	icons.files += resources/icons/64x64
	icons.files += resources/icons/scalable
	mime.path = $$MIME_DIR
	mime.files = platform/studworks.xml
	appdata.path = $$APPDATA_DIR
	appdata.files = packaging/io.github.nathaneltitane.StudWorks.appdata.xml

	INSTALLS += target docs man desktop icons mime appdata

	# the parts library is installed only when present, povray comes from the system
	exists($$_PRO_FILE_PWD_/library.bin) {
		library.path = $$INSTALL_PREFIX/share/studworks
		library.files = $$_PRO_FILE_PWD_/library.bin
		INSTALLS += library
	}

	# find the bundled qt libraries first
	!isEmpty(BUNDLE_LIBRARIES) {
		QMAKE_LFLAGS += -Wl,--disable-new-dtags
		QMAKE_RPATHDIR += $$INSTALL_PREFIX/lib
	}

	!isEmpty(LDRAW_LIBRARY_PATH) {
		DEFINES += SW_LDRAW_LIBRARY_PATH=\\\"$$LDRAW_LIBRARY_PATH\\\"
	}
}

macx {
	ICON = resources/studworks.icns
	QMAKE_INFO_PLIST = platform/Info.plist

	document_icon.files += $$_PRO_FILE_PWD_/resources/studworks_document.icns
	document_icon.path = Contents/Resources
	QMAKE_BUNDLE_DATA += document_icon

	# the parts library and povray are bundled only when present
	exists($$_PRO_FILE_PWD_/library.bin) {
		library.files += $$_PRO_FILE_PWD_/library.bin
		library.path = Contents/Resources
		QMAKE_BUNDLE_DATA += library
	}

	exists($$_PRO_FILE_PWD_/povray) {
		povray.files += $$_PRO_FILE_PWD_/povray
		povray.path = Contents/MacOS
		QMAKE_BUNDLE_DATA += povray
	}
}

ios {
	DEFINES += SW_DISABLE_RENDER_DIALOG=1
	DEFINES += SW_DISABLE_BLENDER=1
}

include(source/application/application.pri)
include(source/core/core.pri)
include(source/library/library.pri)
include(source/rendering/rendering.pri)
include(source/input_output/input_output.pri)
include(source/interface/interface.pri)

OTHER_FILES +=
RESOURCES += studworks.qrc resources/interface/stylesheet/stylesheet.qrc

!win32 {
	TRANSLATIONS = resources/translations/studworks_cs.ts resources/translations/studworks_de.ts resources/translations/studworks_en.ts resources/translations/studworks_es.ts resources/translations/studworks_fr.ts resources/translations/studworks_pt.ts resources/translations/studworks_ru.ts resources/translations/studworks_uk.ts resources/translations/studworks_zh_CN.ts resources/translations/studworks_zh_TW.ts
}
