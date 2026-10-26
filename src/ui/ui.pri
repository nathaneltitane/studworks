# Main window, panels, dialogs and widgets.

INCLUDEPATH += $$PWD

SOURCES += \
	$$PWD/sw_about_dialog.cpp \
	$$PWD/sw_array_dialog.cpp \
	$$PWD/sw_blender_preferences.cpp \
	$$PWD/sw_category_dialog.cpp \
	$$PWD/sw_collapsible_widget.cpp \
	$$PWD/sw_color_list.cpp \
	$$PWD/sw_color_picker.cpp \
	$$PWD/sw_double_spin_box.cpp \
	$$PWD/sw_edge_color_dialog.cpp \
	$$PWD/sw_edit_groups_dialog.cpp \
	$$PWD/sw_find_replace_widget.cpp \
	$$PWD/sw_group_dialog.cpp \
	$$PWD/sw_html_dialog.cpp \
	$$PWD/sw_image_dialog.cpp \
	$$PWD/sw_instructions_dialog.cpp \
	$$PWD/sw_keyframe_widget.cpp \
	$$PWD/sw_ladder_widget.cpp \
	$$PWD/sw_main_window.cpp \
	$$PWD/sw_minifigure_dialog.cpp \
	$$PWD/sw_model_list_dialog.cpp \
	$$PWD/sw_page_setup_dialog.cpp \
	$$PWD/sw_part_palette_dialog.cpp \
	$$PWD/sw_part_selection_popup.cpp \
	$$PWD/sw_part_selection_widget.cpp \
	$$PWD/sw_preferences_dialog.cpp \
	$$PWD/sw_preview_widget.cpp \
	$$PWD/sw_properties_dialog.cpp \
	$$PWD/sw_properties_widget.cpp \
	$$PWD/sw_render_dialog.cpp \
	$$PWD/sw_select_dialog.cpp \
	$$PWD/sw_sets_database_dialog.cpp \
	$$PWD/sw_spinner_widget.cpp \
	$$PWD/sw_timeline_widget.cpp \
	$$PWD/sw_utilities.cpp

HEADERS += \
	$$PWD/sw_about_dialog.h \
	$$PWD/sw_array_dialog.h \
	$$PWD/sw_blender_preferences.h \
	$$PWD/sw_category_dialog.h \
	$$PWD/sw_collapsible_widget.h \
	$$PWD/sw_color_list.h \
	$$PWD/sw_color_picker.h \
	$$PWD/sw_double_spin_box.h \
	$$PWD/sw_edge_color_dialog.h \
	$$PWD/sw_edit_groups_dialog.h \
	$$PWD/sw_find_replace_widget.h \
	$$PWD/sw_group_dialog.h \
	$$PWD/sw_html_dialog.h \
	$$PWD/sw_image_dialog.h \
	$$PWD/sw_instructions_dialog.h \
	$$PWD/sw_keyframe_widget.h \
	$$PWD/sw_ladder_widget.h \
	$$PWD/sw_main_window.h \
	$$PWD/sw_minifigure_dialog.h \
	$$PWD/sw_model_list_dialog.h \
	$$PWD/sw_page_setup_dialog.h \
	$$PWD/sw_part_palette_dialog.h \
	$$PWD/sw_part_selection_popup.h \
	$$PWD/sw_part_selection_widget.h \
	$$PWD/sw_preferences_dialog.h \
	$$PWD/sw_preview_widget.h \
	$$PWD/sw_properties_dialog.h \
	$$PWD/sw_properties_widget.h \
	$$PWD/sw_render_dialog.h \
	$$PWD/sw_select_dialog.h \
	$$PWD/sw_sets_database_dialog.h \
	$$PWD/sw_spinner_widget.h \
	$$PWD/sw_timeline_widget.h \
	$$PWD/sw_utilities.h

FORMS += \
	$$PWD/sw_about_dialog.ui \
	$$PWD/sw_array_dialog.ui \
	$$PWD/sw_category_dialog.ui \
	$$PWD/sw_edit_groups_dialog.ui \
	$$PWD/sw_group_dialog.ui \
	$$PWD/sw_html_dialog.ui \
	$$PWD/sw_image_dialog.ui \
	$$PWD/sw_minifigure_dialog.ui \
	$$PWD/sw_model_list_dialog.ui \
	$$PWD/sw_page_setup_dialog.ui \
	$$PWD/sw_part_palette_dialog.ui \
	$$PWD/sw_preferences_dialog.ui \
	$$PWD/sw_properties_dialog.ui \
	$$PWD/sw_render_dialog.ui \
	$$PWD/sw_select_dialog.ui \
	$$PWD/sw_sets_database_dialog.ui

# Optional embedded Konsole terminal on systems with KDE Frameworks.
qtHaveModule(KParts) {
	SOURCES += $$PWD/sw_terminal_widget.cpp
	HEADERS += $$PWD/sw_terminal_widget.h
}
