#pragma once

#include "sw_thumbnailmanager.h"

class swPartSelectionListModel;
class swPartSelectionListView;
class swPartSelectionWidget;

enum class swPartCategoryType
{
	AllParts,
	PartsInUse,
	Submodels,
	Palette,
	Category,
	Custom,
	Count
};

enum class swPartCategoryRole
{
	Type = Qt::UserRole,
	Index
};

enum class swPartFilterType
{
	Default,
	RegularExpression,
	Count
};

struct swPartPalette
{
	QString Name;
	std::vector<std::string> Parts;
};

class swPartSelectionItemDelegate : public QStyledItemDelegate
{
	Q_OBJECT

public:
	swPartSelectionItemDelegate(QObject* Parent, swPartSelectionListModel* ListModel)
		: QStyledItemDelegate(Parent), mListModel(ListModel)
	{
	}

	void paint(QPainter* Painter, const QStyleOptionViewItem& Option, const QModelIndex& Index) const override;
	QSize sizeHint(const QStyleOptionViewItem& Option, const QModelIndex& Index) const override;

protected:
	void initStyleOption(QStyleOptionViewItem* Option, const QModelIndex& Index) const override;

	swPartSelectionListModel* mListModel;
};

struct swPartSelectionListModelEntry
{
	swPieceData* PieceData = nullptr;
	std::string Description;
	int ColorIndex = -1;
	QPixmap Pixmap;
	swPartThumbnailId ThumbnailId = swPartThumbnailId::Invalid;
};

class swPartSelectionListModel : public QAbstractListModel
{
	Q_OBJECT

public:
	swPartSelectionListModel(QObject* Parent);
	~swPartSelectionListModel();

	int rowCount(const QModelIndex& Parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& Index, int Role = Qt::DisplayRole) const override;
	QVariant headerData(int Section, Qt::Orientation Orientation, int Role = Qt::DisplayRole) const override;
	Qt::ItemFlags flags(const QModelIndex& Index) const override;

	QModelIndex GetPieceDataIndex(swPieceData* PieceData) const;

	swPieceData* GetPieceData(const QModelIndex& Index) const
	{
		return Index.isValid() ? mParts[Index.row()].PieceData : nullptr;
	}

	swPieceData* GetPieceData(int Row) const
	{
		return mParts[Row].PieceData;
	}

	bool GetShowDecoratedParts() const
	{
		return mShowDecoratedParts;
	}

	bool GetShowPartAliases() const
	{
		return mShowPartAliases;
	}

	swPartFilterType GetPartFilterType() const
	{
		return mPartFilterType;
	}

	bool GetCaseSensitiveFilter() const
	{
		return mCaseSensitiveFilter;
	}

	bool GetFileNameFilter() const
	{
		return mFileNameFilter;
	}

	bool GetPartDescriptionFilter() const
	{
		return mPartDescriptionFilter;
	}

	int GetIconSize() const
	{
		return mIconSize;
	}

	bool GetShowPartNames() const
	{
		return mShowPartNames;
	}

	int GetColorIndex() const
	{
		return mColorIndex;
	}

	bool IsColorLocked() const
	{
		return mColorLocked;
	}

	bool IsListMode() const
	{
		return mListMode;
	}

	void UpdateThumbnails();
	void SetColorIndex(int ColorIndex);
	void ToggleColorLocked();
	void ToggleListMode();
	void SetCategory(int CategoryIndex);
	void SetModelsCategory();
	void SetPaletteCategory(int SetIndex);
	void SetCurrentModelCategory();
	void SetCustomParts(const std::vector<std::pair<swPieceData*, std::string>>& Parts, int ColorIndex);
	void SetFilter(const QString& FilterString);
	void RequestThumbnail(int PartIndex);
	void SetShowDecoratedParts(bool Show);
	void SetShowPartAliases(bool Show);
	void SetPartFilterType(swPartFilterType Option);
	void SetCaseSensitiveFilter(bool Option);
	void SetFileNameFilter(bool Option);
	void SetPartDescriptionFilter(bool Option);
	void SetIconSize(int Size, float DeviceScale);
	void SetShowPartNames(bool Show);

protected slots:
	void ThumbnailReady(swPartThumbnailId ThumbnailId, QPixmap Pixmap);

protected:
	void ReleaseThumbnails();

	swPartSelectionListView* mListView;
	std::vector<swPartSelectionListModelEntry> mParts;
	swPartFilterType mPartFilterType;
	int mIconSize;
	float mDeviceScale = 1.0f;
	bool mColorLocked;
	int mColorIndex;
	bool mShowPartNames;
	bool mListMode;
	bool mShowDecoratedParts;
	bool mShowPartAliases;
	bool mCaseSensitiveFilter;
	bool mFileNameFilter;
	bool mPartDescriptionFilter;
	QString mFilterString;
};

class swPartSelectionListView : public QListView
{
	Q_OBJECT

public:
	static constexpr int NoIconSize = 0;
	static constexpr int SmallIconSize = 32;
	static constexpr int MediumIconSize = 64;
	static constexpr int LargeIconSize = 96;
	static constexpr int ExtraLargeIconSize = 192;

	swPartSelectionListView(QWidget* Parent, swPartSelectionWidget* PartSelectionWidget);

	void startDrag(Qt::DropActions SupportedActions) override;
	QSize sizeHint() const override;
	QSize GetCellSize() const;
	QSize GetPreferredSize(int MaximumWidth) const;
	int GetTileSnapDelta() const;

	void SetCategory(swPartCategoryType Type, int Index);
	void SetCustomParts(const std::vector<std::pair<swPieceData*, std::string>>& Parts, int ColorIndex);
	void SetCurrentPart(swPieceData* PieceData);

	swPieceData* GetCurrentPart() const
	{
		return mListModel->GetPieceData(currentIndex());
	}

	swPartSelectionListModel* GetListModel() const
	{
		return mListModel;
	}

	swPartSelectionWidget* GetPartSelectionWidget() const
	{
		return mPartSelectionWidget;
	}

	swPieceData* GetContextInfo() const
	{
		return mContextPieceData;
	}

	void UpdateInUseCategory();
	void UpdateViewMode();

signals:
	void PartPicked(swPieceData* PieceData);
	void LayoutChanged();

public slots:
	void CustomContextMenuRequested(QPoint Pos);
	void SetNoIcons();
	void SetSmallIcons();
	void SetMediumIcons();
	void SetLargeIcons();
	void SetExtraLargeIcons();
	void ToggleRegularExpressionFilter();
	void TogglePartNames();
	void ToggleDecoratedParts();
	void TogglePartAliases();
	void ToggleCaseSensitiveFilter();
	void ToggleFileNameFilter();
	void TogglePartDescriptionFilter();
	void ToggleListMode();
	void ToggleFixedColor();

protected slots:
	void DoubleClicked(const QModelIndex& Index);

protected:
	void SetIconSize(int Size);
	void UpdateIconSize();
	void UpdateItemGeometry();
	void SetPartFilterType(swPartFilterType Option);
	void keyPressEvent(QKeyEvent* KeyEvent) override;

	swPartSelectionListModel* mListModel = nullptr;
	swPartSelectionWidget* mPartSelectionWidget = nullptr;
	swPieceData* mContextPieceData = nullptr;
	int mRequestedIconSize = NoIconSize;
	swPartCategoryType mCategoryType = swPartCategoryType::AllParts;
	int mCategoryIndex = 0;
	std::map<std::pair<swPartCategoryType, int>, int> mLastCategoryRow;
};

class swPartSelectionWidget : public QWidget
{
	Q_OBJECT

public:
	swPartSelectionWidget(bool IsPopup, QWidget* Parent);

	void UpdateThumbnails();
	void SetDefaultPart();
	void UpdateModels();
	void UpdateInUseCategory();
	void UpdateCategories();
	void LoadState(QSettings& Settings);
	void SaveState(QSettings& Settings);
	void SetOrientation(Qt::Orientation Orientation);
	void SetCurrentPart(swPieceData* PieceData);
	void SetCategory(swPartCategoryType Type, int Index);
	void SetCustomParts(const std::vector<std::pair<swPieceData*, std::string>>& Parts, int ColorIndex);

	bool GetIsPopup() const
	{
		return mIsPopup;
	}

	int GetTileSnapDelta() const
	{
		return mPartsWidget ? mPartsWidget->GetTileSnapDelta() : 0;
	}

	int GetColorIndex() const
	{
		return mPartsWidget->GetListModel()->GetColorIndex();
	}

	void SetColorIndex(int ColorIndex)
	{
		mPartsWidget->GetListModel()->SetColorIndex(ColorIndex);
	}

	bool IsColorLocked() const
	{
		return mPartsWidget->GetListModel()->IsColorLocked();
	}

	void ToggleColorLocked()
	{
		mPartsWidget->GetListModel()->ToggleColorLocked();
	}

	const std::vector<swPartPalette>& GetPartPalettes() const
	{
		return mPartPalettes;
	}

	swPieceData* GetCurrentPart() const
	{
		return mPartsWidget->GetCurrentPart();
	}

	void SetDragEnabled(bool Enabled)
	{
		mPartsWidget->setDragEnabled(Enabled);
	}

	void FocusPartFilterWidget() const
	{
		mFilterWidget->setFocus();
		mFilterWidget->selectAll();
	}

signals:
	void PartPicked(swPieceData* PieceData);
	void CurrentPartChanged(swPieceData* PieceData);
	void LayoutChanged();

public slots:
	void AddToPalette();
	void RemoveFromPalette();
	void OpenSubmodel();
	void DockLocationChanged(Qt::DockWidgetArea Area);

protected slots:
	void FilterChanged(const QString& Text);
	void FilterCategoriesChanged(const QString& Text);
	void FilterTriggered();
	void FilterCaseTriggered();
	void FilterCategoriesTriggered();
	void CategoryChanged(QTreeWidgetItem* Current, QTreeWidgetItem* Previous);
	void PartViewSelectionChanged(const QModelIndex& Current, const QModelIndex& Previous);
	void PartViewPartPicked(swPieceData* PieceData);
	void OptionsMenuAboutToShow();
	void EditPartPalettes();

protected:
	void LoadPartPalettes();
	void SavePartPalettes();

	void resizeEvent(QResizeEvent* Event) override;
	bool event(QEvent* Event) override;

	QTreeWidget* mCategoriesWidget = nullptr;
	QLineEdit* mFilterCategoriesWidget = nullptr;
	QAction* mFilterCategoriesAction = nullptr;
	QAction* mFilterCaseAction = nullptr;
	QLineEdit* mFilterWidget = nullptr;
	QAction* mFilterAction = nullptr;
	swPartSelectionListView* mPartsWidget = nullptr;
	QSplitter* mSplitter = nullptr;
	QTreeWidgetItem* mAllPartsCategoryItem = nullptr;
	std::vector<swPartPalette> mPartPalettes;
	bool mIsPopup = false;
};
