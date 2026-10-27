# Views, OpenGL context, scenes and view widgets.

INCLUDEPATH += $$PWD

SOURCES += \
	$$PWD/sw_context.cpp \
	$$PWD/sw_opengl_extensions.cpp \
	$$PWD/sw_scene.cpp \
	$$PWD/sw_string_cache.cpp \
	$$PWD/sw_texture_font.cpp \
	$$PWD/sw_view.cpp \
	$$PWD/sw_view_draw.cpp \
	$$PWD/sw_view_image.cpp \
	$$PWD/sw_view_manipulator.cpp \
	$$PWD/sw_view_sphere.cpp \
	$$PWD/sw_view_widget.cpp

HEADERS += \
	$$PWD/sw_context.h \
	$$PWD/sw_opengl_extensions.h \
	$$PWD/sw_scene.h \
	$$PWD/sw_string_cache.h \
	$$PWD/sw_texture_font.h \
	$$PWD/sw_view.h \
	$$PWD/sw_view_manipulator.h \
	$$PWD/sw_view_sphere.h \
	$$PWD/sw_view_widget.h
