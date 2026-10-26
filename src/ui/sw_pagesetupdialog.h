#pragma once

#include "sw_instructions.h"

namespace Ui
{
class swPageSetupDialog;
}

class swPageSetupDialog : public QDialog
{
	Q_OBJECT

public:
	swPageSetupDialog(QWidget* Parent, swInstructionsPageSetup* PageSetup);
	~swPageSetupDialog();

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

	Ui::swPageSetupDialog* ui;
	swInstructionsPageSetup* mPageSetup;
	swInstructionsPageSetup mPoints;
	swInstructionsPageUnits mUnits = swInstructionsPageUnits::Inches;
	swInstructionsPageUnits mMarginUnits = swInstructionsPageUnits::Inches;
};
