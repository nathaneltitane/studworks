#pragma once

#include "sw_instructions.h"

class swRangeSpinBox;

// File and window printing share pages.
void swSetupInstructionsPrinter(swInstructions* Instructions, QPrinter* Printer);
void swPrintInstructions(swInstructions* Instructions, QPrinter* Printer);

// The only print dialog.
class swInstructionsPrintWidget : public QDockWidget
{
	Q_OBJECT

public:
	swInstructionsPrintWidget(QWidget* Parent, swInstructions* Instructions);

protected:
	void UpdatePrinters();
	void UpdatePages();
	void UpdateFileRow();
	bool BrowseFile();
	void Print();

	swInstructions* mInstructions = nullptr;

	QComboBox* mPrinterComboBox = nullptr;
	QWidget* mFileRow = nullptr;
	QLineEdit* mFileEdit = nullptr;
	swRangeSpinBox* mFromSpinBox = nullptr;
	swRangeSpinBox* mToSpinBox = nullptr;
	swRangeSpinBox* mCopiesSpinBox = nullptr;
	QToolButton* mCollateButton = nullptr;
};
