#pragma once

#include "sw_builder_minifigure.h"

class swColorPicker;
class swDoubleSpinBox;

namespace Ui
{
class swMinifigureBuilderWidget;
}

class swMinifigureBuilderWidget : public QWidget
{
	Q_OBJECT

public:
	explicit swMinifigureBuilderWidget(QWidget* Parent);
	virtual ~swMinifigureBuilderWidget();

	void ReloadParts();

	swMinifigureBuilder* mMinifigureBuilder;

protected slots:
	void AddToModel();
	void TemplateComboBoxActivated(int Index);
	void RandomizeButtonClicked();
	void TemplateSaveButtonClicked();
	void TemplateDeleteButtonClicked();
	void TemplateImportButtonClicked();
	void TemplateExportButtonClicked();
	void PartButtonClicked();
	void ColorChanged(int Index);
	void AngleChanged(double Value);

protected:
	bool eventFilter(QObject* Object, QEvent* Event) override;
	void FitPreview();
	void UpdatePartButton(int ItemIndex);
	void UpdateControls();
	void OptionsMenuAboutToShow();
	void UpdateTemplateCombo();
	void SetCurrentTemplateModified();

	Ui::swMinifigureBuilderWidget* ui;

	swView* mView;
	QWidget* mViewWidget = nullptr;
	std::array<QString, SW_MINIFIGURE_ITEM_COUNT> mSectionNames;
	std::array<QToolButton*, SW_MINIFIGURE_ITEM_COUNT> mPartButtons;
	std::array<swColorPicker*, SW_MINIFIGURE_ITEM_COUNT> mColorPickers;
	std::array<swDoubleSpinBox*, SW_MINIFIGURE_ITEM_COUNT> mSpinBoxes;

	QString mCurrentTemplateName;
	bool mCurrentTemplateModified = false;
};
