# kde thumbnail plugin rendering ldraw files through the studworks command line
TEMPLATE = lib
TARGET = studworksthumbnail

# kde names thumbnail plugins without the lib prefix
CONFIG += plugin no_plugin_name_prefix c++17
CONFIG -= create_prl

QT += KIOGui KCoreAddons

isEmpty(INSTALL_PREFIX):INSTALL_PREFIX = /usr
isEmpty(BIN_DIR):BIN_DIR = $$INSTALL_PREFIX/bin
isEmpty(PLUGIN_DIR):PLUGIN_DIR = $$[QT_INSTALL_PLUGINS]/kf5/thumbcreator

DEFINES += SW_THUMBNAILER_PROGRAM=\\\"$$BIN_DIR/studworks\\\"

HEADERS += sw_thumbnailer.h
SOURCES += sw_thumbnailer.cpp
DISTFILES += sw_thumbnailer.json

target.path = $$PLUGIN_DIR
INSTALLS += target
