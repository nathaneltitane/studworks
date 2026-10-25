#pragma once

#include "lc_instructions.h"

class lcInstructionsPropertiesWidget;

class lcInstructionsStepImageItem : public QGraphicsPixmapItem
{
public:
	lcInstructionsStepImageItem(QGraphicsItem* Parent, lcInstructions* Instructions, lcModel* Model, lcStep Step);

	lcModel* GetModel() const
	{
		return mModel;
	}

	lcStep GetStep() const
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
	lcInstructions* mInstructions = nullptr;
	lcModel* mModel = nullptr;
	lcStep mStep = 1;
	int mWidth = 1;
	int mHeight = 1;
};

class lcInstructionsStepNumberItem : public QGraphicsSimpleTextItem
{
public:
	lcInstructionsStepNumberItem(QGraphicsItem* Parent, lcInstructions* Instructions, lcModel* Model, lcStep Step);

	lcModel* GetModel() const
	{
		return mModel;
	}

	lcStep GetStep() const
	{
		return mStep;
	}

	void Update();

protected:
	lcInstructions* mInstructions = nullptr;
	lcModel* mModel = nullptr;
	lcStep mStep = 1;
};

class lcInstructionsPartsListItem : public QGraphicsPixmapItem
{
public:
	lcInstructionsPartsListItem(QGraphicsItem* Parent, lcInstructions* Instructions, lcModel* Model, lcStep Step);

	lcModel* GetModel() const
	{
		return mModel;
	}

	lcStep GetStep() const
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
	lcInstructions* mInstructions = nullptr;
	lcModel* mModel = nullptr;
	lcStep mStep = 1;
	bool mBillOfMaterials = false;
	int mMaxWidth = 300;
};

class lcInstructionsPageWidget : public QGraphicsView
{
	Q_OBJECT

public:
	lcInstructionsPageWidget(QWidget* Parent, lcInstructions* Instructions, lcInstructionsPropertiesWidget* PropertiesWidget);

	void SetCurrentPage(const lcInstructionsPage* Page);

protected:
	void resizeEvent(QResizeEvent* Event) override;
	void FitPage();

protected slots:
	void StepSettingsChanged(lcModel* Model, lcStep Step);
	void SelectionChanged();

protected:
	lcInstructions* mInstructions;
	lcInstructionsPropertiesWidget* mPropertiesWidget;
	const lcInstructionsPage* mPage = nullptr;
};

class lcInstructionsPageListWidget : public QDockWidget
{
	Q_OBJECT

public:
	lcInstructionsPageListWidget(QWidget* Parent, lcInstructions* Instructions);

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

	lcInstructions* mInstructions;
	QAction* mInsertBlankBeforeAction = nullptr;
	QAction* mInsertBlankAfterAction = nullptr;
	QAction* mRemoveBlankAction = nullptr;
};

class lcInstructionsPropertiesWidget : public QDockWidget
{
	Q_OBJECT

public:
	lcInstructionsPropertiesWidget(QWidget* Parent, lcInstructions* Instructions);

	void SelectionChanged(QGraphicsItem* FocusItem);
	void SetCurrentPage(int PageIndex);

protected:
	void UpdateWidget();
	void AddPageLayoutProperties();
	void AddBoolProperty(lcInstructionsPropertyType Type);
	void AddIntProperty(lcInstructionsPropertyType Type, int Minimum, int Maximum);
	void AddSortProperty(lcInstructionsPropertyType Type);
	void AddColorProperty(lcInstructionsPropertyType Type);
	void AddFontProperty(lcInstructionsPropertyType Type);

	lcCollapsibleWidget* mWidget = nullptr;
	QGridLayout* mPropertiesLayout = nullptr;
	lcInstructions* mInstructions = nullptr;
	QGraphicsItem* mFocusItem = nullptr;
	int mPageIndex = 0;
	lcModel* mModel = nullptr;
	lcStep mStep = 1;
};

class lcInstructionsDialog : public QMainWindow
{
	Q_OBJECT

public:
	lcInstructionsDialog(QWidget* Parent, Project* Project);

protected slots:
	void ShowPrintDialog();
	void PageSetupChanged();
	void CurrentThumbnailChanged(int Index);

protected:
	void Print(QPrinter* Printer);
	void UpdatePageList();

	Project* mProject = nullptr;

	int mCurrentPageNumber;
	lcInstructions* mInstructions;

	lcInstructionsPageWidget* mPageWidget = nullptr;
	lcInstructionsPageListWidget* mPageListWidget = nullptr;
	lcInstructionsPropertiesWidget* mPropertiesWidget = nullptr;

	QToolBar* mPageSettingsToolBar = nullptr;
	QAction* mPrintAction = nullptr;
};
