#pragma once

#include "sw_instructions.h"

class swInstructionsGridWidget;
class swInstructionsPageListWidget;
class swInstructionsPageWidget;
class swInstructionsPlacementWidget;
class swInstructionsPrintWidget;
class swInstructionsPropertiesWidget;

class swInstructionsDialog : public QMainWindow
{
	Q_OBJECT

public:
	swInstructionsDialog(QWidget* Parent, Project* Project);

public slots:
	void ModelChanged();
	void ShowPrintWidget();

protected slots:
	void PageSetupChanged();
	void CurrentThumbnailChanged(int Index);

protected:
	bool eventFilter(QObject* Object, QEvent* Event) override;
	void UpdatePageList();
	void SnapPageListDock();

	Project* mProject = nullptr;
	bool mUpdatePending = false;
	bool mPageListSnapPending = false;

	int mCurrentPageNumber;
	swInstructions* mInstructions;

	swInstructionsPageWidget* mPageWidget = nullptr;
	swInstructionsPageListWidget* mPageListWidget = nullptr;
	swInstructionsPropertiesWidget* mPropertiesWidget = nullptr;
	swInstructionsPlacementWidget* mPlacementWidget = nullptr;
	swInstructionsGridWidget* mGridWidget = nullptr;
	swInstructionsPrintWidget* mPrintWidget = nullptr;
};
