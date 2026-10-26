QT += core gui widgets opengl network xml concurrent
QT *= printsupport
TEMPLATE = app

lessThan(QT_MAJOR_VERSION, 5) {
	error("LeoCAD requires Qt5.4 or later.")
}

equals(QT_MAJOR_VERSION, 5):lessThan(QT_MINOR_VERSION, 4) {
	error("LeoCAD requires Qt5.4 or later.")
}

greaterThan(QT_MAJOR_VERSION, 5) {
	QT += openglwidgets
	win32-msvc* {
		QMAKE_CXXFLAGS += /std:c++17
	}
}

qtHaveModule(gamepad) {
	QT += gamepad
	DEFINES += LC_ENABLE_GAMEPAD
}

# Optional embedded Konsole terminal on systems with KDE Frameworks.
qtHaveModule(KParts) {
	QT += KParts
	DEFINES += LC_ENABLE_TERMINAL
}

CONFIG += precompile_header incremental c++17 force_debug_info

win32 {
	DEFINES += NOMINMAX
	RC_ICONS = resources/leocad.ico
	VERSION = 26.9.0.0
	QMAKE_TARGET_COMPANY = LeoCAD Software
	QMAKE_TARGET_DESCRIPTION = LeoCAD
	QMAKE_TARGET_COPYRIGHT = "Copyright (C) LeoCAD.org"
	QMAKE_TARGET_PRODUCT = LeoCAD
	RC_CODEPAGE = 1252
	RC_LANG = 0x0409

	LIBS += -lwininet
}
win32-msvc* {
	QMAKE_CXXFLAGS_WARN_ON += /W4 -wd4100
	QMAKE_CXXFLAGS *= /MP
	PRECOMPILED_HEADER = src/app/sw_global.h
	PRECOMPILED_SOURCE = src/app/sw_global.cpp
	DEFINES += _CRT_SECURE_NO_WARNINGS _CRT_SECURE_NO_DEPRECATE=1 _CRT_NONSTDC_NO_WARNINGS=1
	INCLUDEPATH += $$[QT_INSTALL_HEADERS]/QtZlib
	QMAKE_LFLAGS += /INCREMENTAL
	LIBS += -ladvapi32 -lshell32 -lopengl32 -luser32
	QMAKE_CXXFLAGS += /we4834
} else {
	PRECOMPILED_HEADER = src/app/sw_global.h
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
		DEFINES += "LC_CONTINUOUS_BUILD=$$system(git rev-parse --short HEAD)"
	}
}

APPVEYOR_REPO_TAG = $$(APPVEYOR_REPO_TAG)
APPVEYOR_REPO_COMMIT = $$(APPVEYOR_REPO_COMMIT)
!isEmpty(APPVEYOR_REPO_COMMIT) {
	isEmpty(APPVEYOR_REPO_TAG) {
		DEFINES += "LC_CONTINUOUS_BUILD=$$system(git rev-parse --short HEAD)"
		VERSION = 0.0.0.1
		QMAKE_TARGET_DESCRIPTION = "LeoCAD Continuous Build $$system(git rev-parse --short HEAD)"
	}
}

TSFILES = resources/leocad_cs.ts resources/leocad_de.ts resources/leocad_en.ts resources/leocad_es.ts resources/leocad_fr.ts resources/leocad_pt.ts resources/leocad_ru.ts resources/leocad_uk.ts resources/leocad_zh_CN.ts
lrelease.input = TSFILES
lrelease.output = ${QMAKE_FILE_PATH}/${QMAKE_FILE_BASE}.qm
lrelease.commands = $$QMAKE_LRELEASE -silent ${QMAKE_FILE_IN} -qm ${QMAKE_FILE_PATH}/${QMAKE_FILE_BASE}.qm
lrelease.CONFIG += no_link target_predeps
QMAKE_EXTRA_COMPILERS += lrelease

system($$QMAKE_LRELEASE -silent $$TSFILES)

unix:!macx {
	TARGET = leocad
} else {
	TARGET = LeoCAD
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

unix:!macx {
	isEmpty(INSTALL_PREFIX):INSTALL_PREFIX = /usr
	isEmpty(BIN_DIR):BIN_DIR = $$INSTALL_PREFIX/bin
	isEmpty(DOCS_DIR):DOCS_DIR = $$INSTALL_PREFIX/share/doc/leocad
	isEmpty(ICON_DIR):ICON_DIR = $$INSTALL_PREFIX/share/icons
	isEmpty(MAN_DIR):MAN_DIR = $$INSTALL_PREFIX/share/man/man1
	isEmpty(DESKTOP_DIR):DESKTOP_DIR = $$INSTALL_PREFIX/share/applications
	isEmpty(MIME_DIR):MIME_DIR = $$INSTALL_PREFIX/share/mime/packages
	isEmpty(APPDATA_DIR):APPDATA_DIR = $$INSTALL_PREFIX/share/metainfo

	target.path = $$BIN_DIR
	docs.path = $$DOCS_DIR
	docs.files = docs/README.md docs/CREDITS.txt docs/COPYING.txt
	man.path = $$MAN_DIR
	man.files = docs/leocad.1
	desktop.path = $$DESKTOP_DIR
	desktop.files = platform/leocad.desktop
	icons.path = $$ICON_DIR/hicolor
	icons.files += tools/icon/128x128
	icons.files += tools/icon/16x16
	icons.files += tools/icon/24x24
	icons.files += tools/icon/256x256
	icons.files += tools/icon/32x32
	icons.files += tools/icon/48x48
	icons.files += tools/icon/512x512
	icons.files += tools/icon/64x64
	icons.files += tools/icon/scalable
	mime.path = $$MIME_DIR
	mime.files = platform/leocad.xml
	appdata.path = $$APPDATA_DIR
	appdata.files = tools/setup/org.leocad.LeoCAD.appdata.xml

	INSTALLS += target docs man desktop icons mime appdata

	!isEmpty(DISABLE_UPDATE_CHECK) {
		DEFINES += LC_DISABLE_UPDATE_CHECK=$$DISABLE_UPDATE_CHECK
	}

	!isEmpty(LDRAW_LIBRARY_PATH) {
		DEFINES += LC_LDRAW_LIBRARY_PATH=\\\"$$LDRAW_LIBRARY_PATH\\\"
	}
}

macx {
	ICON = resources/leocad.icns
	QMAKE_INFO_PLIST = platform/Info.plist

	document_icon.files += $$_PRO_FILE_PWD_/resources/leocad_document.icns
	document_icon.path = Contents/Resources
	library.files += $$_PRO_FILE_PWD_/library.bin
	library.path = Contents/Resources
	povray.files += $$_PRO_FILE_PWD_/povray
	povray.path = Contents/MacOS

	QMAKE_BUNDLE_DATA += document_icon library povray
	DEFINES += LC_DISABLE_UPDATE_CHECK=1
}

ios {
	DEFINES += LC_DISABLE_RENDER_DIALOG=1
	DEFINES += LC_DISABLE_BLENDER=1
}

include(src/app/app.pri)
include(src/core/core.pri)
include(src/library/library.pri)
include(src/render/render.pri)
include(src/io/io.pri)
include(src/ui/ui.pri)

OTHER_FILES +=
RESOURCES += leocad.qrc resources/stylesheet/stylesheet.qrc

!win32 {
	TRANSLATIONS = resources/leocad_cs.ts resources/leocad_de.ts resources/leocad_en.ts resources/leocad_es.ts resources/leocad_fr.ts resources/leocad_pt.ts resources/leocad_ru.ts resources/leocad_uk.ts resources/leocad_zh_CN.ts
}
