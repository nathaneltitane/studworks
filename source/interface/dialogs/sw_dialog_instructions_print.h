#pragma once

#include "sw_instructions.h"

// File printing and the instructions window print the same instruction pages.
void swSetupInstructionsPrinter(swInstructions* Instructions, QPrinter* Printer);
void swPrintInstructions(swInstructions* Instructions, QPrinter* Printer);

// the print panel is the only print dialog of the application, the pages beside it are its preview
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
	QSpinBox* mFromSpinBox = nullptr;
	QSpinBox* mToSpinBox = nullptr;
	QSpinBox* mCopiesSpinBox = nullptr;
	QToolButton* mCollateButton = nullptr;
};
