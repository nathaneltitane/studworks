#pragma once

#include "sw_view.h"

class swPreview;
class swPreviewNameEdit;

class swPreviewDockWidget : public QWidget
{
	Q_OBJECT

public:
	explicit swPreviewDockWidget(QWidget* Parent);

	bool SetCurrentPart(swPartData* PartData, int ColorCode);
	void ClearPreview();
	void UpdatePreview();
	void RefreshDescription();
	void RefreshModel(const swModel* Model);
	void RefreshAfterAssetChange(const std::vector<swPartData*>& ChangedParts);
	void RebindPartData(swPartData* Previous, swPartData* Replacement);
	void SetSquareViewport(bool SquareViewport);
	void UpdateSquareViewport();

protected slots:
	void SetPreviewLock();
	void OptionsMenuAboutToShow();

protected:
	bool eventFilter(QObject* Object, QEvent* Event) override;
	void RefreshPendingAssets();
	void UpdateViewGeometry();

	QAction* mLockAction;
	QWidget* mViewContainer;
	swPreviewNameEdit* mLabel;
	swPreview* mPreview;
	swViewWidget* mViewWidget;
	bool mRefreshingModel = false;
	bool mAssetRefreshPending = false;
	bool mRefitAfterAssetLoad = true;
};

class swPreview : public swView
{
public:
	swPreview();

	QString GetDescription() const
	{
		return mDescription;
	}

	bool IsModel() const
	{
		return mIsModel;
	}

	void ClearPreview();
	void UpdatePreview();
	void RefreshDescription();
	bool SetCurrentPart(swPartData* PartData, int ColorCode);

protected:
	std::unique_ptr<Project> mLoader;

	QString mDescription;
	bool mIsModel = false;
};
