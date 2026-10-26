# Main window, panels, dialogs and widgets.

INCLUDEPATH += $$PWD

SOURCES += \
	$$PWD/lc_aboutdialog.cpp \
	$$PWD/lc_arraydialog.cpp \
	$$PWD/lc_blenderpreferences.cpp \
	$$PWD/lc_categorydialog.cpp \
	$$PWD/lc_collapsiblewidget.cpp \
	$$PWD/lc_colorlist.cpp \
	$$PWD/lc_colorpicker.cpp \
	$$PWD/lc_doublespinbox.cpp \
	$$PWD/lc_edgecolordialog.cpp \
	$$PWD/lc_editgroupsdialog.cpp \
	$$PWD/lc_findreplacewidget.cpp \
	$$PWD/lc_groupdialog.cpp \
	$$PWD/lc_htmldialog.cpp \
	$$PWD/lc_imagedialog.cpp \
	$$PWD/lc_instructionsdialog.cpp \
	$$PWD/lc_keyframewidget.cpp \
	$$PWD/lc_ladderwidget.cpp \
	$$PWD/lc_mainwindow.cpp \
	$$PWD/lc_minifigdialog.cpp \
	$$PWD/lc_modellistdialog.cpp \
	$$PWD/lc_pagesetupdialog.cpp \
	$$PWD/lc_partpalettedialog.cpp \
	$$PWD/lc_partselectionpopup.cpp \
	$$PWD/lc_partselectionwidget.cpp \
	$$PWD/lc_previewwidget.cpp \
	$$PWD/lc_propertieswidget.cpp \
	$$PWD/lc_qpreferencesdialog.cpp \
	$$PWD/lc_qpropertiesdialog.cpp \
	$$PWD/lc_qutils.cpp \
	$$PWD/lc_renderdialog.cpp \
	$$PWD/lc_selectdialog.cpp \
	$$PWD/lc_setsdatabasedialog.cpp \
	$$PWD/lc_spinnerwidget.cpp \
	$$PWD/lc_timelinewidget.cpp \
	$$PWD/lc_updatedialog.cpp

HEADERS += \
	$$PWD/lc_aboutdialog.h \
	$$PWD/lc_arraydialog.h \
	$$PWD/lc_blenderpreferences.h \
	$$PWD/lc_categorydialog.h \
	$$PWD/lc_collapsiblewidget.h \
	$$PWD/lc_colorlist.h \
	$$PWD/lc_colorpicker.h \
	$$PWD/lc_doublespinbox.h \
	$$PWD/lc_edgecolordialog.h \
	$$PWD/lc_editgroupsdialog.h \
	$$PWD/lc_findreplacewidget.h \
	$$PWD/lc_groupdialog.h \
	$$PWD/lc_htmldialog.h \
	$$PWD/lc_imagedialog.h \
	$$PWD/lc_instructionsdialog.h \
	$$PWD/lc_keyframewidget.h \
	$$PWD/lc_ladderwidget.h \
	$$PWD/lc_mainwindow.h \
	$$PWD/lc_minifigdialog.h \
	$$PWD/lc_modellistdialog.h \
	$$PWD/lc_pagesetupdialog.h \
	$$PWD/lc_partpalettedialog.h \
	$$PWD/lc_partselectionpopup.h \
	$$PWD/lc_partselectionwidget.h \
	$$PWD/lc_previewwidget.h \
	$$PWD/lc_propertieswidget.h \
	$$PWD/lc_qpreferencesdialog.h \
	$$PWD/lc_qpropertiesdialog.h \
	$$PWD/lc_qutils.h \
	$$PWD/lc_renderdialog.h \
	$$PWD/lc_selectdialog.h \
	$$PWD/lc_setsdatabasedialog.h \
	$$PWD/lc_spinnerwidget.h \
	$$PWD/lc_timelinewidget.h \
	$$PWD/lc_updatedialog.h

FORMS += \
	$$PWD/lc_aboutdialog.ui \
	$$PWD/lc_arraydialog.ui \
	$$PWD/lc_categorydialog.ui \
	$$PWD/lc_editgroupsdialog.ui \
	$$PWD/lc_groupdialog.ui \
	$$PWD/lc_htmldialog.ui \
	$$PWD/lc_imagedialog.ui \
	$$PWD/lc_minifigdialog.ui \
	$$PWD/lc_modellistdialog.ui \
	$$PWD/lc_pagesetupdialog.ui \
	$$PWD/lc_partpalettedialog.ui \
	$$PWD/lc_qpreferencesdialog.ui \
	$$PWD/lc_qpropertiesdialog.ui \
	$$PWD/lc_renderdialog.ui \
	$$PWD/lc_selectdialog.ui \
	$$PWD/lc_setsdatabasedialog.ui \
	$$PWD/lc_updatedialog.ui

# Optional embedded Konsole terminal on systems with KDE Frameworks.
qtHaveModule(KParts) {
	SOURCES += $$PWD/lc_terminalwidget.cpp
	HEADERS += $$PWD/lc_terminalwidget.h
}
