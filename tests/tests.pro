TEMPLATE = app
TARGET = studworks_tests

CONFIG += console testcase c++17
CONFIG -= app_bundle

QT += concurrent opengl printsupport testlib widgets

greaterThan(QT_MAJOR_VERSION, 5) {
	QT += openglwidgets
}

INCLUDEPATH += ../source/application ../source/core ../source/library ../source/rendering ../source/input_output ../source/interface/dialogs ../source/interface/widgets ../source/interface/window

HEADERS += sw_tests.h

SOURCES += \
	../source/core/sw_object_property.cpp \
	../source/input_output/sw_command_meta.cpp \
	sw_command_meta_test.cpp \
	sw_object_history_test.cpp \
	sw_object_property_test.cpp \
	main.cpp
