#pragma once

#include "sw_instructions.h"

class swInstructionsPropertiesWidget;

// File printing and the instructions window print the same instruction pages.
void swSetupInstructionsPrinter(swInstructions* Instructions, QPrinter* Printer);
void swPrintInstructions(swInstructions* Instructions, QPrinter* Printer);

class swInstructionsStepImageItem : public QGraphicsPixmapItem
{
public:
	swInstructionsStepImageItem(QGraphicsItem* Parent, swInstructions* Instructions, swModel* Model, swStep Step);

	swModel* GetModel() const
	{
		return mModel;
	}

	swStep GetStep() const
	{
		return mStep;
	}

	void SetImageSize(int Width, int Height)
	{
		mWidth = Width;
		mHeight = Height;
	}

	void Update();

protected:
	swInstructions* mInstructions = nullptr;
	swModel* mModel = nullptr;
	swStep mStep = 1;
	int mWidth = 1;
	int mHeight = 1;
};

class swInstructionsStepNumberItem : public QGraphicsSimpleTextItem
{
public:
	swInstructionsStepNumberItem(QGraphicsItem* Parent, swInstructions* Instructions, swModel* Model, swStep Step);

	swModel* GetModel() const
	{
		return mModel;
	}

	swStep GetStep() const
	{
		return mStep;
	}

	void Update();

protected:
	swInstructions* mInstructions = nullptr;
	swModel* mModel = nullptr;
	swStep mStep = 1;
};

class swInstructionsPartsListItem : public QGraphicsPixmapItem
{
public:
	swInstructionsPartsListItem(QGraphicsItem* Parent, swInstructions* Instructions, swModel* Model, swStep Step);

	swModel* GetModel() const
	{
		return mModel;
	}

	swStep GetStep() const
	{
		return mStep;
	}

	void SetBillOfMaterials(int MaxWidth)
	{
		mBillOfMaterials = true;
		mMaxWidth = MaxWidth;
	}

	void Update();

protected:
	swInstructions* mInstructions = nullptr;
	swModel* mModel = nullptr;
	swStep mStep = 1;
	bool mBillOfMaterials = false;
	int mMaxWidth = 300;
};

class swInstructionsPageWidget : public QGraphicsView
{
	Q_OBJECT

public:
	swInstructionsPageWidget(QWidget* Parent, swInstructions* Instructions, swInstructionsPropertiesWidget* PropertiesWidget);

	void SetCurrentPage(const swInstructionsPage* Page);

protected:
	void resizeEvent(QResizeEvent* Event) override;
	void FitPage();

protected slots:
	void StepSettingsChanged(swModel* Model, swStep Step);
	void SelectionChanged();

protected:
	swInstructions* mInstructions;
	swInstructionsPropertiesWidget* mPropertiesWidget;
	const swInstructionsPage* mPage = nullptr;
};

class swInstructionsPageListWidget : public QDockWidget
{
	Q_OBJECT

public:
	swInstructionsPageListWidget(QWidget* Parent, swInstructions* Instructions);

protected slots:
	void ShowPageSetupDialog();

public:
//protected:
//	QComboBox* mSizeComboBox = nullptr;
//	QLineEdit* mWidthEdit = nullptr;
//	QLineEdit* mHeightEdit = nullptr;
//
//	QRadioButton* mPortraitButton = nullptr;
//	QRadioButton* mLandscapeButton = nullptr;
//
//	QLineEdit* mLeftMarginEdit = nullptr;
//	QLineEdit* mRightMarginEdit = nullptr;
//	QLineEdit* mTopMarginEdit = nullptr;
//	QLineEdit* mBottomMarginEdit = nullptr;
//
//	QComboBox* mUnitsComboBox = nullptr;

	QListWidget* mThumbnailsWidget = nullptr;

protected:
	void OptionsMenuAboutToShow();

	swInstructions* mInstructions;
	QAction* mInsertBlankBeforeAction = nullptr;
	QAction* mInsertBlankAfterAction = nullptr;
	QAction* mRemoveBlankAction = nullptr;
};

class swInstructionsPropertiesWidget : public QDockWidget
{
	Q_OBJECT

public:
	swInstructionsPropertiesWidget(QWidget* Parent, swInstructions* Instructions);

	void SelectionChanged(QGraphicsItem* FocusItem);
	void SetCurrentPage(int PageIndex);

protected:
	void UpdateWidget();
	void AddPageLayoutProperties();
	void AddBoolProperty(swInstructionsPropertyType Type);
	void AddIntProperty(swInstructionsPropertyType Type, int Minimum, int Maximum);
	void AddSortProperty(swInstructionsPropertyType Type);
	void AddColorProperty(swInstructionsPropertyType Type);
	void AddFontProperty(swInstructionsPropertyType Type);

	swCollapsibleWidget* mWidget = nullptr;
	QGridLayout* mPropertiesLayout = nullptr;
	swInstructions* mInstructions = nullptr;
	QGraphicsItem* mFocusItem = nullptr;
	int mPageIndex = 0;
	swModel* mModel = nullptr;
	swStep mStep = 1;
};

class swInstructionsDialog : public QMainWindow
{
	Q_OBJECT

public:
	swInstructionsDialog(QWidget* Parent, Project* Project);

public slots:
	void ModelChanged();

protected slots:
	void ShowPrintDialog();
	void PageSetupChanged();
	void CurrentThumbnailChanged(int Index);

protected:
	void Print(QPrinter* Printer);
	void UpdatePageList();

	Project* mProject = nullptr;
	bool mUpdatePending = false;

	int mCurrentPageNumber;
	swInstructions* mInstructions;

	swInstructionsPageWidget* mPageWidget = nullptr;
	swInstructionsPageListWidget* mPageListWidget = nullptr;
	swInstructionsPropertiesWidget* mPropertiesWidget = nullptr;

	QToolBar* mPageSettingsToolBar = nullptr;
	QAction* mPrintAction = nullptr;
};
