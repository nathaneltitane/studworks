#pragma once

#include "minifig.h"

class lcColorPicker;
class lcDoubleSpinBox;

namespace Ui
{
class lcMinifigDialog;
}

class lcMinifigDialog : public QWidget
{
	Q_OBJECT

public:
	explicit lcMinifigDialog(QWidget* Parent);
	virtual ~lcMinifigDialog();

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

	Ui::lcMinifigDialog* ui;

	lcView* mView;
	QWidget* mViewWidget = nullptr;
	std::array<QString, LC_MFW_NUMITEMS> mSectionNames;
	std::array<QToolButton*, LC_MFW_NUMITEMS> mPieceButtons;
	std::array<lcColorPicker*, LC_MFW_NUMITEMS> mColorPickers;
	std::array<lcDoubleSpinBox*, LC_MFW_NUMITEMS> mSpinBoxes;

	QString mCurrentTemplateName;
	bool mCurrentTemplateModified = false;
};
