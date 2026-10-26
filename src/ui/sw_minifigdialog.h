#pragma once

#include "minifig.h"

class swColorPicker;
class swDoubleSpinBox;

namespace Ui
{
class swMinifigDialog;
}

class swMinifigDialog : public QWidget
{
	Q_OBJECT

public:
	explicit swMinifigDialog(QWidget* Parent);
	virtual ~swMinifigDialog();

	MinifigWizard* mMinifigWizard;

protected slots:
	void AddToModel();
	void TemplateComboBoxCurrentTextChanged(const QString& TemplateName);
	void TemplateSaveButtonClicked();
	void TemplateDeleteButtonClicked();
	void TemplateImportButtonClicked();
	void TemplateExportButtonClicked();
	void PieceButtonClicked();
	void ColorChanged(int Index);
	void AngleChanged(double Value);

protected:
	bool eventFilter(QObject* Object, QEvent* Event) override;
	void FitPreview();
	void UpdatePieceButton(int ItemIndex);
	void OptionsMenuAboutToShow();
	void UpdateTemplateCombo();
	void SetCurrentTemplateModified();

	Ui::swMinifigDialog* ui;

	swView* mView;
	QWidget* mViewWidget = nullptr;
	std::array<QString, SW_MFW_NUMITEMS> mSectionNames;
	std::array<QToolButton*, SW_MFW_NUMITEMS> mPieceButtons;
	std::array<swColorPicker*, SW_MFW_NUMITEMS> mColorPickers;
	std::array<swDoubleSpinBox*, SW_MFW_NUMITEMS> mSpinBoxes;

	QString mCurrentTemplateName;
	bool mCurrentTemplateModified = false;
};
