# Panels, popups and reusable widgets.

INCLUDEPATH += $$PWD

SOURCES += \
	$$PWD/sw_collapsible_widget.cpp \
	$$PWD/sw_color_list.cpp \
	$$PWD/sw_color_picker.cpp \
	$$PWD/sw_double_spin_box.cpp \
	$$PWD/sw_find_replace_widget.cpp \
	$$PWD/sw_keyframe_widget.cpp \
	$$PWD/sw_ladder_widget.cpp \
	$$PWD/sw_minifigure_builder_widget.cpp \
	$$PWD/sw_part_selection_popup.cpp \
	$$PWD/sw_part_selection_widget.cpp \
	$$PWD/sw_preview_widget.cpp \
	$$PWD/sw_properties_widget.cpp \
	$$PWD/sw_spinner_widget.cpp \
	$$PWD/sw_timeline_widget.cpp

HEADERS += \
	$$PWD/sw_collapsible_widget.h \
	$$PWD/sw_color_list.h \
	$$PWD/sw_color_picker.h \
	$$PWD/sw_double_spin_box.h \
	$$PWD/sw_find_replace_widget.h \
	$$PWD/sw_keyframe_widget.h \
	$$PWD/sw_ladder_widget.h \
	$$PWD/sw_minifigure_builder_widget.h \
	$$PWD/sw_part_selection_popup.h \
	$$PWD/sw_part_selection_widget.h \
	$$PWD/sw_preview_widget.h \
	$$PWD/sw_properties_widget.h \
	$$PWD/sw_spinner_widget.h \
	$$PWD/sw_timeline_widget.h

FORMS += \
	$$PWD/sw_minifigure_builder_widget.ui

# Optional embedded Konsole terminal on systems with KDE Frameworks.
qtHaveModule(KParts) {
	SOURCES += $$PWD/sw_terminal_widget.cpp
	HEADERS += $$PWD/sw_terminal_widget.h
}
