# Main window, panels, dialogs and widgets.

INCLUDEPATH += $$PWD

SOURCES += \
	$$PWD/sw_aboutdialog.cpp \
	$$PWD/sw_arraydialog.cpp \
	$$PWD/sw_blenderpreferences.cpp \
	$$PWD/sw_categorydialog.cpp \
	$$PWD/sw_collapsiblewidget.cpp \
	$$PWD/sw_colorlist.cpp \
	$$PWD/sw_colorpicker.cpp \
	$$PWD/sw_doublespinbox.cpp \
	$$PWD/sw_edgecolordialog.cpp \
	$$PWD/sw_editgroupsdialog.cpp \
	$$PWD/sw_findreplacewidget.cpp \
	$$PWD/sw_groupdialog.cpp \
	$$PWD/sw_htmldialog.cpp \
	$$PWD/sw_imagedialog.cpp \
	$$PWD/sw_instructionsdialog.cpp \
	$$PWD/sw_keyframewidget.cpp \
	$$PWD/sw_ladderwidget.cpp \
	$$PWD/sw_mainwindow.cpp \
	$$PWD/sw_minifigdialog.cpp \
	$$PWD/sw_modellistdialog.cpp \
	$$PWD/sw_pagesetupdialog.cpp \
	$$PWD/sw_partpalettedialog.cpp \
	$$PWD/sw_partselectionpopup.cpp \
	$$PWD/sw_partselectionwidget.cpp \
	$$PWD/sw_previewwidget.cpp \
	$$PWD/sw_propertieswidget.cpp \
	$$PWD/sw_preferencesdialog.cpp \
	$$PWD/sw_propertiesdialog.cpp \
	$$PWD/sw_utils.cpp \
	$$PWD/sw_renderdialog.cpp \
	$$PWD/sw_selectdialog.cpp \
	$$PWD/sw_setsdatabasedialog.cpp \
	$$PWD/sw_spinnerwidget.cpp \
	$$PWD/sw_timelinewidget.cpp \
	$$PWD/sw_updatedialog.cpp

HEADERS += \
	$$PWD/sw_aboutdialog.h \
	$$PWD/sw_arraydialog.h \
	$$PWD/sw_blenderpreferences.h \
	$$PWD/sw_categorydialog.h \
	$$PWD/sw_collapsiblewidget.h \
	$$PWD/sw_colorlist.h \
	$$PWD/sw_colorpicker.h \
	$$PWD/sw_doublespinbox.h \
	$$PWD/sw_edgecolordialog.h \
	$$PWD/sw_editgroupsdialog.h \
	$$PWD/sw_findreplacewidget.h \
	$$PWD/sw_groupdialog.h \
	$$PWD/sw_htmldialog.h \
	$$PWD/sw_imagedialog.h \
	$$PWD/sw_instructionsdialog.h \
	$$PWD/sw_keyframewidget.h \
	$$PWD/sw_ladderwidget.h \
	$$PWD/sw_mainwindow.h \
	$$PWD/sw_minifigdialog.h \
	$$PWD/sw_modellistdialog.h \
	$$PWD/sw_pagesetupdialog.h \
	$$PWD/sw_partpalettedialog.h \
	$$PWD/sw_partselectionpopup.h \
	$$PWD/sw_partselectionwidget.h \
	$$PWD/sw_previewwidget.h \
	$$PWD/sw_propertieswidget.h \
	$$PWD/sw_preferencesdialog.h \
	$$PWD/sw_propertiesdialog.h \
	$$PWD/sw_utils.h \
	$$PWD/sw_renderdialog.h \
	$$PWD/sw_selectdialog.h \
	$$PWD/sw_setsdatabasedialog.h \
	$$PWD/sw_spinnerwidget.h \
	$$PWD/sw_timelinewidget.h \
	$$PWD/sw_updatedialog.h

FORMS += \
	$$PWD/sw_aboutdialog.ui \
	$$PWD/sw_arraydialog.ui \
	$$PWD/sw_categorydialog.ui \
	$$PWD/sw_editgroupsdialog.ui \
	$$PWD/sw_groupdialog.ui \
	$$PWD/sw_htmldialog.ui \
	$$PWD/sw_imagedialog.ui \
	$$PWD/sw_minifigdialog.ui \
	$$PWD/sw_modellistdialog.ui \
	$$PWD/sw_pagesetupdialog.ui \
	$$PWD/sw_partpalettedialog.ui \
	$$PWD/sw_preferencesdialog.ui \
	$$PWD/sw_propertiesdialog.ui \
	$$PWD/sw_renderdialog.ui \
	$$PWD/sw_selectdialog.ui \
	$$PWD/sw_setsdatabasedialog.ui \
	$$PWD/sw_updatedialog.ui

# Optional embedded Konsole terminal on systems with KDE Frameworks.
qtHaveModule(KParts) {
	SOURCES += $$PWD/sw_terminalwidget.cpp
	HEADERS += $$PWD/sw_terminalwidget.h
}
