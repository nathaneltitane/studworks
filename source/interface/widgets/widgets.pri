# Panels, popups and reusable widgets.

INCLUDEPATH += $$PWD

SOURCES += \
	$$PWD/sw_widget_collapsible.cpp \
	$$PWD/sw_widget_color_list.cpp \
	$$PWD/sw_widget_color_picker.cpp \
	$$PWD/sw_widget_double_spin_box.cpp \
	$$PWD/sw_widget_find_replace.cpp \
	$$PWD/sw_widget_keyframe.cpp \
	$$PWD/sw_widget_label_elided.cpp \
	$$PWD/sw_widget_ladder.cpp \
	$$PWD/sw_widget_minifigure_builder.cpp \
	$$PWD/sw_widget_part_selection.cpp \
	$$PWD/sw_widget_part_selection_popup.cpp \
	$$PWD/sw_widget_preview.cpp \
	$$PWD/sw_widget_properties.cpp \
	$$PWD/sw_widget_spinner.cpp \
	$$PWD/sw_widget_timeline.cpp

HEADERS += \
	$$PWD/sw_widget_collapsible.h \
	$$PWD/sw_widget_color_list.h \
	$$PWD/sw_widget_color_picker.h \
	$$PWD/sw_widget_double_spin_box.h \
	$$PWD/sw_widget_find_replace.h \
	$$PWD/sw_widget_keyframe.h \
	$$PWD/sw_widget_label_elided.h \
	$$PWD/sw_widget_ladder.h \
	$$PWD/sw_widget_minifigure_builder.h \
	$$PWD/sw_widget_part_selection.h \
	$$PWD/sw_widget_part_selection_popup.h \
	$$PWD/sw_widget_preview.h \
	$$PWD/sw_widget_properties.h \
	$$PWD/sw_widget_spinner.h \
	$$PWD/sw_widget_timeline.h

FORMS += \
	$$PWD/sw_widget_minifigure_builder.ui

# Optional embedded Konsole terminal on systems with KDE Frameworks, left out of bundled packages.
qtHaveModule(KParts):isEmpty(BUNDLE_LIBRARIES) {
	SOURCES += $$PWD/sw_widget_terminal.cpp
	HEADERS += $$PWD/sw_widget_terminal.h
}
