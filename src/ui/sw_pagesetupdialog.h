#pragma once

#include "sw_instructions.h"

namespace Ui
{
class lcPageSetupDialog;
}

class lcPageSetupDialog : public QDialog
{
	Q_OBJECT

public:
	lcPageSetupDialog(QWidget* Parent, lcInstructionsPageSetup* PageSetup);
	~lcPageSetupDialog();

public slots:
	void accept() override;

protected slots:
	void PresetChanged(int Index);
	void UnitsChanged(int Index);
	void MarginUnitsChanged(int Index);
	void OrientationChanged();
	void SizeEdited();

private:
	void UpdateSpinBoxes();
	void UpdatePreset();
	void UpdateOrientation();

	Ui::lcPageSetupDialog* ui;
	lcInstructionsPageSetup* mPageSetup;
	lcInstructionsPageSetup mPoints;
	lcInstructionsPageUnits mUnits = lcInstructionsPageUnits::Inches;
	lcInstructionsPageUnits mMarginUnits = lcInstructionsPageUnits::Inches;
};
