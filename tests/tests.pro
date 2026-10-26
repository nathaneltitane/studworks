TEMPLATE = app
TARGET = studworks_tests

CONFIG += console testcase c++17
CONFIG -= app_bundle

QT += concurrent opengl printsupport testlib widgets

greaterThan(QT_MAJOR_VERSION, 5) {
	QT += openglwidgets
}

INCLUDEPATH += ../src/app ../src/core ../src/library ../src/render ../src/io ../src/ui

HEADERS += sw_tests.h

SOURCES += \
	../src/core/sw_object_property.cpp \
	sw_object_history_test.cpp \
	sw_object_property_test.cpp \
	main.cpp
