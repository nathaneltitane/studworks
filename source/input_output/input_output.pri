# File formats, network access and instruction pages.

INCLUDEPATH += $$PWD

SOURCES += \
	$$PWD/sw_bricklink.cpp \
	$$PWD/sw_command_meta.cpp \
	$$PWD/sw_file.cpp \
	$$PWD/sw_file_gif.cpp \
	$$PWD/sw_http.cpp \
	$$PWD/sw_instructions.cpp \
	$$PWD/sw_instructions_annotation.cpp \
	$$PWD/sw_instructions_file.cpp \
	$$PWD/sw_lxf.cpp

HEADERS += \
	$$PWD/sw_bricklink.h \
	$$PWD/sw_command_meta.h \
	$$PWD/sw_file.h \
	$$PWD/sw_file_gif.h \
	$$PWD/sw_http.h \
	$$PWD/sw_instructions.h \
	$$PWD/sw_lxf.h
