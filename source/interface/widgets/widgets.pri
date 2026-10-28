# Panels, popups and reusable widgets.

INCLUDEPATH += $$PWD

SOURCES += \
	$$PWD/sw_widget_builder_minifigure.cpp \
	$$PWD/sw_widget_collapsible.cpp \
	$$PWD/sw_widget_find_replace.cpp \
	$$PWD/sw_widget_keyframe.cpp \
	$$PWD/sw_widget_label_elided.cpp \
	$$PWD/sw_widget_ladder.cpp \
	$$PWD/sw_widget_list_color.cpp \
	$$PWD/sw_widget_picker_color.cpp \
	$$PWD/sw_widget_preview.cpp \
	$$PWD/sw_widget_properties.cpp \
	$$PWD/sw_widget_selection_part.cpp \
	$$PWD/sw_widget_selection_part_popup.cpp \
	$$PWD/sw_widget_spin_box_double.cpp \
	$$PWD/sw_widget_spinner.cpp \
	$$PWD/sw_widget_timeline.cpp

HEADERS += \
	$$PWD/sw_widget_builder_minifigure.h \
	$$PWD/sw_widget_collapsible.h \
	$$PWD/sw_widget_find_replace.h \
	$$PWD/sw_widget_keyframe.h \
	$$PWD/sw_widget_label_elided.h \
	$$PWD/sw_widget_ladder.h \
	$$PWD/sw_widget_list_color.h \
	$$PWD/sw_widget_picker_color.h \
	$$PWD/sw_widget_preview.h \
	$$PWD/sw_widget_properties.h \
	$$PWD/sw_widget_selection_part.h \
	$$PWD/sw_widget_selection_part_popup.h \
	$$PWD/sw_widget_spin_box.h \
	$$PWD/sw_widget_spin_box_double.h \
	$$PWD/sw_widget_spinner.h \
	$$PWD/sw_widget_timeline.h

FORMS += \
	$$PWD/sw_widget_builder_minifigure.ui

# Optional embedded Konsole terminal on systems with KDE Frameworks, left out of bundled packages.
qtHaveModule(KParts):isEmpty(BUNDLE_LIBRARIES) {
	SOURCES += $$PWD/sw_widget_terminal.cpp
	HEADERS += $$PWD/sw_widget_terminal.h
}
