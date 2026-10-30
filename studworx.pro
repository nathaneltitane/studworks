QT += core gui widgets opengl network xml concurrent
QT *= printsupport
TEMPLATE = app

lessThan(QT_MAJOR_VERSION, 5) {
	error("StudWorx requires Qt5.4 or later.")
}

equals(QT_MAJOR_VERSION, 5):lessThan(QT_MINOR_VERSION, 4) {
	error("StudWorx requires Qt5.4 or later.")
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

win32 {
	DEFINES += NOMINMAX
	ICON_FILE = $$PWD/resources/studworx.ico

	# the resource file carries the release date as its version text
	version_rc.input = $$PWD/platform/studworx.rc.in
	version_rc.output = $$OUT_PWD/studworx.rc
	QMAKE_SUBSTITUTES += version_rc
	RC_FILE = $$OUT_PWD/studworx.rc

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

TSFILES = resources/translations/studworx_cs.ts resources/translations/studworx_de.ts resources/translations/studworx_en.ts resources/translations/studworx_es.ts resources/translations/studworx_fr.ts resources/translations/studworx_pt.ts resources/translations/studworx_ru.ts resources/translations/studworx_uk.ts resources/translations/studworx_zh_CN.ts resources/translations/studworx_zh_TW.ts
lrelease.input = TSFILES
lrelease.output = ${QMAKE_FILE_PATH}/${QMAKE_FILE_BASE}.qm
lrelease.commands = $$QMAKE_LRELEASE -silent ${QMAKE_FILE_IN} -qm ${QMAKE_FILE_PATH}/${QMAKE_FILE_BASE}.qm
lrelease.CONFIG += no_link target_predeps
QMAKE_EXTRA_COMPILERS += lrelease

system($$QMAKE_LRELEASE -silent $$TSFILES)

unix:!macx {
	TARGET = studworx
} else {
	TARGET = StudWorx
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
	# povray is copied beside the executable only when present
	exists($$_PRO_FILE_PWD_/povconsole32-sse2.exe) {
		povray.files = $$_PRO_FILE_PWD_/povconsole32-sse2.exe
		povray.path = $$OUT_PWD/$$DESTDIR
		COPIES += povray
	}
}

unix:!macx {
	isEmpty(INSTALL_PREFIX):INSTALL_PREFIX = /usr
	isEmpty(BIN_DIR):BIN_DIR = $$INSTALL_PREFIX/bin
	isEmpty(DOCS_DIR):DOCS_DIR = $$INSTALL_PREFIX/share/doc/studworx
	isEmpty(ICON_DIR):ICON_DIR = $$INSTALL_PREFIX/share/icons
	isEmpty(MAN_DIR):MAN_DIR = $$INSTALL_PREFIX/share/man/man1
	isEmpty(DESKTOP_DIR):DESKTOP_DIR = $$INSTALL_PREFIX/share/applications
	isEmpty(MIME_DIR):MIME_DIR = $$INSTALL_PREFIX/share/mime/packages
	isEmpty(APPDATA_DIR):APPDATA_DIR = $$INSTALL_PREFIX/share/metainfo
	isEmpty(THUMBNAILER_DIR):THUMBNAILER_DIR = $$INSTALL_PREFIX/share/thumbnailers

	target.path = $$BIN_DIR
	docs.path = $$DOCS_DIR
	docs.files = README.md LICENSE.md documentation/CREDITS.txt
	man.path = $$MAN_DIR
	man.files = documentation/studworx.1
	desktop.path = $$DESKTOP_DIR
	desktop.files = platform/studworx.desktop
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
	mime.files = platform/studworx.xml
	appdata.path = $$APPDATA_DIR
	appdata.files = packaging/io.github.nathaneltitane.StudWorx.appdata.xml
	thumbnailer.path = $$THUMBNAILER_DIR
	thumbnailer.files = platform/studworx.thumbnailer

	INSTALLS += target docs man desktop icons mime appdata thumbnailer

	# find the bundled qt libraries first
	!isEmpty(BUNDLE_LIBRARIES) {
		QMAKE_LFLAGS += -Wl,--disable-new-dtags
		QMAKE_RPATHDIR += $ORIGIN/../lib
	}

	!isEmpty(LDRAW_LIBRARY_PATH) {
		DEFINES += SW_LDRAW_LIBRARY_PATH=\\\"$$LDRAW_LIBRARY_PATH\\\"
	}
}

macx {
	ICON = resources/studworx.icns
	QMAKE_INFO_PLIST = platform/Info.plist

	document_icon.files += $$_PRO_FILE_PWD_/resources/studworx_document.icns
	document_icon.path = Contents/Resources
	QMAKE_BUNDLE_DATA += document_icon

	# povray is bundled only when present
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
RESOURCES += studworx.qrc resources/interface/stylesheet/stylesheet.qrc

!win32 {
	TRANSLATIONS = resources/translations/studworx_cs.ts resources/translations/studworx_de.ts resources/translations/studworx_en.ts resources/translations/studworx_es.ts resources/translations/studworx_fr.ts resources/translations/studworx_pt.ts resources/translations/studworx_ru.ts resources/translations/studworx_uk.ts resources/translations/studworx_zh_CN.ts resources/translations/studworx_zh_TW.ts
}
