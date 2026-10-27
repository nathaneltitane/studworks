#pragma once

#include "sw_manager_thumbnail.h"

class swPieceSelectionListModel;
class swPieceSelectionListView;
class swPieceSelectionWidget;

enum class swPieceCategoryType
{
	AllPieces,
	PiecesInUse,
	Submodels,
	Palette,
	Category,
	Custom,
	Count
};

enum class swPieceCategoryRole
{
	Type = Qt::UserRole,
	Index
};

enum class swPieceFilterType
{
	Default,
	RegularExpression,
	Count
};

struct swPiecePalette
{
	QString Name;
	std::vector<std::string> Pieces;
};

class swPieceSelectionItemDelegate : public QStyledItemDelegate
{
	Q_OBJECT

public:
	swPieceSelectionItemDelegate(QObject* Parent, swPieceSelectionListModel* ListModel)
		: QStyledItemDelegate(Parent), mListModel(ListModel)
	{
	}

	void paint(QPainter* Painter, const QStyleOptionViewItem& Option, const QModelIndex& Index) const override;
	QSize sizeHint(const QStyleOptionViewItem& Option, const QModelIndex& Index) const override;

protected:
	void initStyleOption(QStyleOptionViewItem* Option, const QModelIndex& Index) const override;

	swPieceSelectionListModel* mListModel;
};

struct swPieceSelectionListModelEntry
{
	swPieceData* PieceData = nullptr;
	std::string Description;
	int ColorIndex = -1;
	QPixmap Pixmap;
	swPieceThumbnailId ThumbnailId = swPieceThumbnailId::Invalid;
};

class swPieceSelectionListModel : public QAbstractListModel
{
	Q_OBJECT

public:
	swPieceSelectionListModel(QObject* Parent);
	~swPieceSelectionListModel();

	int rowCount(const QModelIndex& Parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& Index, int Role = Qt::DisplayRole) const override;
	QVariant headerData(int Section, Qt::Orientation Orientation, int Role = Qt::DisplayRole) const override;
	Qt::ItemFlags flags(const QModelIndex& Index) const override;

	QModelIndex GetPieceDataIndex(swPieceData* PieceData) const;

	swPieceData* GetPieceData(const QModelIndex& Index) const
	{
		return Index.isValid() ? mPieces[Index.row()].PieceData : nullptr;
	}

	swPieceData* GetPieceData(int Row) const
	{
		return mPieces[Row].PieceData;
	}

	bool GetShowDecoratedPieces() const
	{
		return mShowDecoratedPieces;
	}

	bool GetShowPieceAliases() const
	{
		return mShowPieceAliases;
	}

	swPieceFilterType GetPieceFilterType() const
	{
		return mPieceFilterType;
	}

	bool GetCaseSensitiveFilter() const
	{
		return mCaseSensitiveFilter;
	}

	bool GetFileNameFilter() const
	{
		return mFileNameFilter;
	}

	bool GetPieceDescriptionFilter() const
	{
		return mPieceDescriptionFilter;
	}

	int GetIconSize() const
	{
		return mIconSize;
	}

	bool GetShowPieceNames() const
	{
		return mShowPieceNames;
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
	void SetCustomPieces(const std::vector<std::pair<swPieceData*, std::string>>& Pieces, int ColorIndex);
	void SetFilter(const QString& FilterString);
	void RequestThumbnail(int PieceIndex);
	void SetShowDecoratedPieces(bool Show);
	void SetShowPieceAliases(bool Show);
	void SetPieceFilterType(swPieceFilterType Option);
	void SetCaseSensitiveFilter(bool Option);
	void SetFileNameFilter(bool Option);
	void SetPieceDescriptionFilter(bool Option);
	void SetIconSize(int Size, float DeviceScale);
	void SetShowPieceNames(bool Show);

protected slots:
	void ThumbnailReady(swPieceThumbnailId ThumbnailId, QPixmap Pixmap);

protected:
	void ReleaseThumbnails();

	swPieceSelectionListView* mListView;
	std::vector<swPieceSelectionListModelEntry> mPieces;
	swPieceFilterType mPieceFilterType;
	int mIconSize;
	float mDeviceScale = 1.0f;
	bool mColorLocked;
	int mColorIndex;
	bool mShowPieceNames;
	bool mListMode;
	bool mShowDecoratedPieces;
	bool mShowPieceAliases;
	bool mCaseSensitiveFilter;
	bool mFileNameFilter;
	bool mPieceDescriptionFilter;
	QString mFilterString;
};

class swPieceSelectionListView : public QListView
{
	Q_OBJECT

public:
	static constexpr int NoIconSize = 0;
	static constexpr int SmallIconSize = 32;
	static constexpr int MediumIconSize = 64;
	static constexpr int LargeIconSize = 96;
	static constexpr int ExtraLargeIconSize = 192;

	swPieceSelectionListView(QWidget* Parent, swPieceSelectionWidget* PieceSelectionWidget);

	void startDrag(Qt::DropActions SupportedActions) override;
	QSize sizeHint() const override;
	QSize GetCellSize() const;
	QSize GetPreferredSize(int MaximumWidth) const;
	int GetTileSnapDelta() const;

	void SetCategory(swPieceCategoryType Type, int Index);
	void SetCustomPieces(const std::vector<std::pair<swPieceData*, std::string>>& Pieces, int ColorIndex);
	void SetCurrentPiece(swPieceData* PieceData);

	swPieceData* GetCurrentPiece() const
	{
		return mListModel->GetPieceData(currentIndex());
	}

	swPieceSelectionListModel* GetListModel() const
	{
		return mListModel;
	}

	swPieceSelectionWidget* GetPieceSelectionWidget() const
	{
		return mPieceSelectionWidget;
	}

	swPieceData* GetContextInfo() const
	{
		return mContextPieceData;
	}

	void UpdateInUseCategory();
	void UpdateViewMode();

signals:
	void PiecePicked(swPieceData* PieceData);
	void LayoutChanged();

public slots:
	void CustomContextMenuRequested(QPoint Pos);
	void SetNoIcons();
	void SetSmallIcons();
	void SetMediumIcons();
	void SetLargeIcons();
	void SetExtraLargeIcons();
	void ToggleRegularExpressionFilter();
	void TogglePieceNames();
	void ToggleDecoratedPieces();
	void TogglePieceAliases();
	void ToggleCaseSensitiveFilter();
	void ToggleFileNameFilter();
	void TogglePieceDescriptionFilter();
	void ToggleListMode();
	void ToggleFixedColor();

protected slots:
	void DoubleClicked(const QModelIndex& Index);

protected:
	void SetIconSize(int Size);
	void UpdateIconSize();
	void UpdateItemGeometry();
	void SetPieceFilterType(swPieceFilterType Option);
	void keyPressEvent(QKeyEvent* KeyEvent) override;

	swPieceSelectionListModel* mListModel = nullptr;
	swPieceSelectionWidget* mPieceSelectionWidget = nullptr;
	swPieceData* mContextPieceData = nullptr;
	int mRequestedIconSize = NoIconSize;
	swPieceCategoryType mCategoryType = swPieceCategoryType::AllPieces;
	int mCategoryIndex = 0;
	std::map<std::pair<swPieceCategoryType, int>, int> mLastCategoryRow;
};

class swPieceSelectionWidget : public QWidget
{
	Q_OBJECT

public:
	swPieceSelectionWidget(bool IsPopup, QWidget* Parent);

	void UpdateThumbnails();
	void SetDefaultPiece();
	void UpdateModels();
	void UpdateInUseCategory();
	void UpdateCategories();
	void LoadState(QSettings& Settings);
	void SaveState(QSettings& Settings);
	void SetOrientation(Qt::Orientation Orientation);
	void SnapCategoriesWidth();
	void SetCurrentPiece(swPieceData* PieceData);
	void SetCategory(swPieceCategoryType Type, int Index);
	void SetCustomPieces(const std::vector<std::pair<swPieceData*, std::string>>& Pieces, int ColorIndex);

	bool GetIsPopup() const
	{
		return mIsPopup;
	}

	int GetTileSnapDelta() const
	{
		return mPiecesWidget ? mPiecesWidget->GetTileSnapDelta() : 0;
	}

	int GetColorIndex() const
	{
		return mPiecesWidget->GetListModel()->GetColorIndex();
	}

	void SetColorIndex(int ColorIndex)
	{
		mPiecesWidget->GetListModel()->SetColorIndex(ColorIndex);
	}

	bool IsColorLocked() const
	{
		return mPiecesWidget->GetListModel()->IsColorLocked();
	}

	void ToggleColorLocked()
	{
		mPiecesWidget->GetListModel()->ToggleColorLocked();
	}

	const std::vector<swPiecePalette>& GetPiecePalettes() const
	{
		return mPiecePalettes;
	}

	swPieceData* GetCurrentPiece() const
	{
		return mPiecesWidget->GetCurrentPiece();
	}

	void SetDragEnabled(bool Enabled)
	{
		mPiecesWidget->setDragEnabled(Enabled);
	}

	void FocusPieceFilterWidget() const
	{
		mFilterWidget->setFocus();
		mFilterWidget->selectAll();
	}

signals:
	void PiecePicked(swPieceData* PieceData);
	void CurrentPieceChanged(swPieceData* PieceData);
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
	void PieceViewSelectionChanged(const QModelIndex& Current, const QModelIndex& Previous);
	void PieceViewPiecePicked(swPieceData* PieceData);
	void OptionsMenuAboutToShow();
	void EditPiecePalettes();

protected:
	void LoadPiecePalettes();
	void SavePiecePalettes();

	void resizeEvent(QResizeEvent* Event) override;
	bool event(QEvent* Event) override;
	bool eventFilter(QObject* Object, QEvent* Event) override;

	QTreeWidget* mCategoriesWidget = nullptr;
	QLineEdit* mFilterCategoriesWidget = nullptr;
	QAction* mFilterCategoriesAction = nullptr;
	QAction* mFilterCaseAction = nullptr;
	QLineEdit* mFilterWidget = nullptr;
	QAction* mFilterAction = nullptr;
	swPieceSelectionListView* mPiecesWidget = nullptr;
	QSplitter* mSplitter = nullptr;
	QTreeWidgetItem* mAllPiecesCategoryItem = nullptr;
	std::vector<swPiecePalette> mPiecePalettes;
	Qt::DockWidgetArea mDockArea = Qt::NoDockWidgetArea;
	bool mIsPopup = false;
};
