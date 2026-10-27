#include "sw_global.h"
#include "sw_widget_selection_piece.h"
#include "sw_window_utilities.h"
#include "sw_dialog_palette_piece.h"
#include "sw_profile.h"
#include "sw_application.h"
#include "sw_window_main.h"
#include "sw_library.h"
#include "sw_manager_thumbnail.h"
#include "sw_project.h"
#include "sw_data_piece.h"
#include "sw_extensions_opengl.h"
#include "sw_category.h"
#include "sw_track_train.h"
#include "sw_filter.h"
#include "sw_colors.h"

Q_DECLARE_METATYPE(QList<int>)

void swPieceSelectionItemDelegate::initStyleOption(QStyleOptionViewItem* Option, const QModelIndex& Index) const
{
	QStyledItemDelegate::initStyleOption(Option, Index);
	Option->showDecorationSelected = true;

	if (mListModel->GetIconSize())
		Option->decorationSize = QSize(mListModel->GetIconSize(), mListModel->GetIconSize());

	// Keep the rendered part colors when the style requests a selected icon.
	const QPixmap Pixmap = qvariant_cast<QPixmap>(Index.data(Qt::DecorationRole));
	if (!Pixmap.isNull())
		Option->icon.addPixmap(Pixmap, QIcon::Selected);
}

void swPieceSelectionItemDelegate::paint(QPainter* Painter, const QStyleOptionViewItem& Option, const QModelIndex& Index) const
{
	mListModel->RequestThumbnail(Index.row());
	QStyledItemDelegate::paint(Painter, Option, Index);
}

QSize swPieceSelectionItemDelegate::sizeHint(const QStyleOptionViewItem& Option, const QModelIndex& Index) const
{
	QSize Size = QStyledItemDelegate::sizeHint(Option, Index);
	int IconSize = mListModel->GetIconSize();

	if (IconSize && !mListModel->IsListMode())
	{
		const QWidget* Widget = Option.widget;
		if (!Widget)
			Widget = qobject_cast<QWidget*>(parent());
		QStyle* Style = Widget ? Widget->style() : QApplication::style();
		const int PixmapMargin = Style->pixelMetric(QStyle::PM_FocusFrameHMargin, &Option, Widget) + 1;
		int PixmapWidth = IconSize + 2 * PixmapMargin;
		Size.setWidth(PixmapWidth);
		Size.setHeight(qMax(PixmapWidth, Size.height()));
	}

	return Size;
}

swPieceSelectionListModel::swPieceSelectionListModel(QObject* Parent)
	: QAbstractListModel(Parent)
{
	mListView = (swPieceSelectionListView*)Parent;
	const bool IsPopup = mListView->GetPieceSelectionWidget()->GetIsPopup();

	mIconSize = 0;
	mShowPieceNames = swGetProfileInteger(IsPopup ? SW_PROFILE_PIECES_POPUP_NAMES : SW_PROFILE_PIECES_LIST_NAMES);
	mListMode = swGetProfileInteger(IsPopup ? SW_PROFILE_PIECES_POPUP_LIST_MODE : SW_PROFILE_PIECES_LIST_LIST_MODE);
	mShowDecoratedPieces = swGetProfileInteger(IsPopup ? SW_PROFILE_PIECES_POPUP_DECORATED : SW_PROFILE_PIECES_LIST_DECORATED);
	mShowPieceAliases = swGetProfileInteger(IsPopup ? SW_PROFILE_PIECES_POPUP_ALIASES : SW_PROFILE_PIECES_LIST_ALIASES);
	mCaseSensitiveFilter = swGetProfileInteger(SW_PROFILE_PIECES_LIST_CASE_SENSITIVE_FILTER);
	mFileNameFilter = swGetProfileInteger(SW_PROFILE_PIECES_LIST_FILE_NAME_FILTER);
	mPieceDescriptionFilter = swGetProfileInteger(SW_PROFILE_PIECES_LIST_PIECE_DESCRIPTION_FILTER);
	mPieceFilterType = swGetProfilePieceFilterType();

	uint ColorCode = IsPopup ? SW_COLOR_NO_COLOR : swGetProfileUnsignedInteger(SW_PROFILE_PIECES_LIST_COLOR);
	int LockedFlag = IsPopup ? 0 : swGetProfileInteger(SW_PROFILE_PIECES_LIST_COLOR_LOCKED);

	if (ColorCode == SW_COLOR_NO_COLOR && LockedFlag == 0)
	{
		mColorIndex = gMainWindow->mColorIndex;
		mColorLocked = false;
	}
	else
	{
		uint ColorCodeValue = (ColorCode == SW_COLOR_NO_COLOR) ? swGetColorCode(gMainWindow->mColorIndex) : ColorCode;

		mColorIndex = swGetColorIndex(ColorCodeValue);
		mColorLocked = (LockedFlag != 0) || (ColorCode != SW_COLOR_NO_COLOR);
	}

	connect(swGetPiecesLibrary()->GetThumbnailManager(), &swThumbnailManager::ThumbnailReady, this, &swPieceSelectionListModel::ThumbnailReady);
}

swPieceSelectionListModel::~swPieceSelectionListModel()
{
	ReleaseThumbnails();
}

void swPieceSelectionListModel::UpdateThumbnails()
{
	beginResetModel();

	ReleaseThumbnails();

	endResetModel();

	SetFilter(mFilterString);
}

void swPieceSelectionListModel::SetColorIndex(int ColorIndex)
{
	if (mColorLocked || ColorIndex == mColorIndex)
		return;

	mColorIndex = ColorIndex;

	UpdateThumbnails();
}

void swPieceSelectionListModel::ToggleColorLocked()
{
	if (mListView->GetPieceSelectionWidget()->GetIsPopup())
		return;

	mColorLocked = !mColorLocked;

	SetColorIndex(gMainWindow->GetInsertColorIndex());
	if (mColorLocked)
	{
		swSetProfileUnsignedInteger(SW_PROFILE_PIECES_LIST_COLOR, swGetColorCode(mColorIndex));
		swSetProfileInteger(SW_PROFILE_PIECES_LIST_COLOR_LOCKED, 1);
	}
	else
	{
		swSetProfileInteger(SW_PROFILE_PIECES_LIST_COLOR_LOCKED, 0);
		swRemoveProfileKey(SW_PROFILE_PIECES_LIST_COLOR);
	}
}

void swPieceSelectionListModel::ToggleListMode()
{
	mListMode = !mListMode;

	mListView->UpdateViewMode();
	swSetProfileInteger(mListView->GetPieceSelectionWidget()->GetIsPopup() ? SW_PROFILE_PIECES_POPUP_LIST_MODE : SW_PROFILE_PIECES_LIST_LIST_MODE, mListMode);
}

void swPieceSelectionListModel::SetCategory(int CategoryIndex)
{
	beginResetModel();

	ReleaseThumbnails();

	swPiecesLibrary* Library = swGetPiecesLibrary();
	std::vector<swPieceData*> SinglePieces, GroupedPieces;

	if (CategoryIndex != -1)
		Library->GetCategoryEntries(CategoryIndex, false, SinglePieces, GroupedPieces);
	else
	{
		SinglePieces = Library->GetPieces();

		swModel* ActiveModel = gMainWindow->GetActiveModel();

		for (size_t PieceIndex = 0; PieceIndex < SinglePieces.size(); )
		{
			swPieceData* PieceData = SinglePieces[PieceIndex];

			if (!PieceData->IsModel() || !PieceData->GetModel()->IncludesModel(ActiveModel))
				PieceIndex++;
			else
				SinglePieces.erase(SinglePieces.begin() + PieceIndex);
		}
	}

	auto swPieceSortFunction=[](const swPieceData* a, const swPieceData* b)
	{
		return strcmp(a->m_strDescription, b->m_strDescription) < 0;
	};

	std::sort(SinglePieces.begin(), SinglePieces.end(), swPieceSortFunction);

	mPieces.resize(SinglePieces.size());

	for (size_t PieceIndex = 0; PieceIndex < SinglePieces.size(); PieceIndex++)
		mPieces[PieceIndex].PieceData = SinglePieces[PieceIndex];

	endResetModel();

	SetFilter(mFilterString);
}

void swPieceSelectionListModel::SetModelsCategory()
{
	beginResetModel();

	ReleaseThumbnails();
	mPieces.clear();

	const std::vector<std::unique_ptr<swModel>>& Models = swGetActiveProject()->GetModels();
	swModel* ActiveModel = gMainWindow->GetActiveModel();

	for (const std::unique_ptr<swModel>& Model : Models)
		if (!Model->IncludesModel(ActiveModel))
			mPieces.emplace_back().PieceData = Model->GetPieceData();

	auto swPieceSortFunction = [](const swPieceSelectionListModelEntry& a, const swPieceSelectionListModelEntry& b)
	{
		return strcmp(a.PieceData->m_strDescription, b.PieceData->m_strDescription) < 0;
	};

	std::sort(mPieces.begin(), mPieces.end(), swPieceSortFunction);

	endResetModel();

	SetFilter(mFilterString);
}

void swPieceSelectionListModel::SetPaletteCategory(int SetIndex)
{
	beginResetModel();

	ReleaseThumbnails();
	mPieces.clear();

	swPieceSelectionWidget* PieceSelectionWidget = mListView->GetPieceSelectionWidget();
	const std::vector<swPiecePalette>& Palettes = PieceSelectionWidget->GetPiecePalettes();
	std::vector<swPieceData*> PiecesList = swGetPiecesLibrary()->GetPiecesFromSet(Palettes[SetIndex].Pieces);

	auto swPieceSortFunction = [](const swPieceData* a, const swPieceData* b)
	{
		return strcmp(a->m_strDescription, b->m_strDescription) < 0;
	};

	std::sort(PiecesList.begin(), PiecesList.end(), swPieceSortFunction);

	mPieces.reserve(PiecesList.size());

	for (swPieceData* PieceData : PiecesList)
		mPieces.emplace_back().PieceData = PieceData;

	endResetModel();

	SetFilter(mFilterString);
}

void swPieceSelectionListModel::SetCurrentModelCategory()
{
	beginResetModel();

	ReleaseThumbnails();
	mPieces.clear();

	swModel* ActiveModel = gMainWindow->GetActiveModel();
	swPiecesList PiecesList;

	if (ActiveModel)
		ActiveModel->GetPiecesList(gDefaultColor, true, true, PiecesList);

	for (const auto& PieceIt : PiecesList)
		mPieces.emplace_back().PieceData = (swPieceData*)PieceIt.first;

	auto swPieceSortFunction = [](const swPieceSelectionListModelEntry& a, const swPieceSelectionListModelEntry& b)
	{
		return strcmp(a.PieceData->m_strDescription, b.PieceData->m_strDescription) < 0;
	};

	std::sort(mPieces.begin(), mPieces.end(), swPieceSortFunction);

	endResetModel();

	SetFilter(mFilterString);
}

void swPieceSelectionListModel::SetCustomPieces(const std::vector<std::pair<swPieceData*, std::string>>& Pieces, int ColorIndex)
{
	beginResetModel();

	mColorLocked = true;
	mColorIndex = ColorIndex;

	ReleaseThumbnails();
	mPieces.clear();

	for (const auto& [PieceData, Description] : Pieces)
	{
		swPieceSelectionListModelEntry& Entry = mPieces.emplace_back();

		Entry.PieceData = PieceData;
		Entry.Description = Description;

		if (PieceData && PieceData->GetTrainTrackInfo())
			Entry.ColorIndex = mColorIndex;
	}

	endResetModel();

	SetFilter(mFilterString);
}

void swPieceSelectionListModel::SetFilter(const QString& FilterString)
{
	mFilterString = FilterString;

	swFilter Filter(FilterString.toStdString());

#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
	QRegularExpression::PatternOption PatternOptions = mCaseSensitiveFilter ? QRegularExpression::NoPatternOption : QRegularExpression::CaseInsensitiveOption;
	QRegularExpression FilterRx = QRegularExpression(FilterString, PatternOptions);
#else
	Qt::CaseSensitivity CaseSensitive = mCaseSensitiveFilter ? Qt::CaseSensitive : Qt::CaseInsensitive;
	QRegExp FilterRx(FilterString, CaseSensitive, QRegExp::RegExp2);
#endif

	for (size_t PieceIdx = 0; PieceIdx < mPieces.size(); PieceIdx++)
	{
		swPieceData* PieceData = mPieces[PieceIdx].PieceData;
		bool Visible = true;

		if (!PieceData)
			Visible = true;
		else if (!mShowDecoratedPieces && PieceData->IsDecorated() && !PieceData->IsProjectPiece())
			Visible = false;
		else if (!mShowPieceAliases && PieceData->IsAlias())
			Visible = false;
		else if (mPieceFilterType == swPieceFilterType::Default)
		{
			const char* Description = mPieces[PieceIdx].Description.empty() ? PieceData->m_strDescription : mPieces[PieceIdx].Description.c_str();

			Visible = (mPieceDescriptionFilter && Filter.Match(Description)) || (mFileNameFilter && Filter.Match(PieceData->mFileName));
		}
		else if (mPieceFilterType == swPieceFilterType::RegularExpression)
		{
			QString Description = mPieces[PieceIdx].Description.empty() ? QString(PieceData->m_strDescription) : QString::fromStdString(mPieces[PieceIdx].Description);

			Visible = (mPieceDescriptionFilter && Description.contains(FilterRx)) || (mFileNameFilter && QString(PieceData->mFileName).contains(FilterRx));
		}

		mListView->setRowHidden((int)PieceIdx, !Visible);
	}
}

int swPieceSelectionListModel::rowCount(const QModelIndex& Parent) const
{
	Q_UNUSED(Parent);

	return (int)mPieces.size();
}

QVariant swPieceSelectionListModel::data(const QModelIndex& Index, int Role) const
{
	size_t InfoIndex = Index.row();

	if (Index.isValid() && InfoIndex < mPieces.size())
	{
		swPieceData* PieceData = mPieces[InfoIndex].PieceData;

		switch (Role)
		{
		case Qt::DisplayRole:
			if (!mIconSize || mShowPieceNames || mListMode)
				return QVariant(mPieces[InfoIndex].Description.empty() ? QString::fromLatin1(PieceData->m_strDescription) : QString::fromStdString(mPieces[InfoIndex].Description));
			break;

		case Qt::ToolTipRole:
			if (PieceData)
			{
				const QString Error = swGetPiecesLibrary()->GetPieceDataLoadError(PieceData);

				if (!Error.isEmpty())
					return QVariant(Error);

				if (PieceData->mState == swAssetState::Failed)
					return QVariant(tr("Could not load piece '%1'.").arg(QString::fromLatin1(PieceData->mFileName)));

				return QVariant(QString("'%1' ('%2')").arg(QString::fromLatin1(PieceData->m_strDescription), QString::fromLatin1(PieceData->mFileName)));
			}
			break;

		case Qt::DecorationRole:
			if (mIconSize && !mPieces[InfoIndex].Pixmap.isNull())
				return QVariant(mPieces[InfoIndex].Pixmap);
			else
				return QVariant(QColor(0, 0, 0, 0));

		default:
			break;
		}
	}

	return QVariant();
}

QVariant swPieceSelectionListModel::headerData(int Section, Qt::Orientation Orientation, int Role) const
{
	Q_UNUSED(Section);
	Q_UNUSED(Orientation);

	return Role == Qt::DisplayRole ? QVariant(QLatin1String("Image")) : QVariant();
}

Qt::ItemFlags swPieceSelectionListModel::flags(const QModelIndex& Index) const
{
	Qt::ItemFlags DefaultFlags = QAbstractListModel::flags(Index);

	if (Index.isValid())
		return Qt::ItemIsDragEnabled | DefaultFlags;
	else
		return DefaultFlags;
}

QModelIndex swPieceSelectionListModel::GetPieceDataIndex(swPieceData* PieceData) const
{
	if (PieceData)
	{
		for (int PieceIndex = 0; PieceIndex < static_cast<int>(mPieces.size()); PieceIndex++)
			if (mPieces[PieceIndex].PieceData == PieceData)
				return index(PieceIndex, 0);
	}

	return QModelIndex();
}

void swPieceSelectionListModel::ReleaseThumbnails()
{
	swThumbnailManager* ThumbnailManager = swGetPiecesLibrary()->GetThumbnailManager();

	for (swPieceSelectionListModelEntry& Piece : mPieces)
	{
		ThumbnailManager->ReleaseThumbnail(Piece.ThumbnailId);
		Piece.ThumbnailId = swPieceThumbnailId::Invalid;
		Piece.Pixmap = QPixmap();
	}
}

void swPieceSelectionListModel::RequestThumbnail(int PieceIndex)
{
	if (!mIconSize || !mPieces[PieceIndex].Pixmap.isNull() || mPieces[PieceIndex].ThumbnailId != swPieceThumbnailId::Invalid)
		return;

	swPieceData* PieceData = mPieces[PieceIndex].PieceData;

	if (!PieceData)
		return;

	int ColorIndex = mPieces[PieceIndex].ColorIndex == -1 ? mColorIndex : mPieces[PieceIndex].ColorIndex;

	auto [ThumbnailId, Thumbnail] = swGetPiecesLibrary()->GetThumbnailManager()->RequestThumbnail(PieceData, ColorIndex, mIconSize, mDeviceScale);

	mPieces[PieceIndex].ThumbnailId = ThumbnailId;

	if (!Thumbnail.isNull())
	{
		mPieces[PieceIndex].Pixmap = Thumbnail;

		emit dataChanged(index(PieceIndex, 0), index(PieceIndex, 0), { Qt::DecorationRole });
	}
}

void swPieceSelectionListModel::ThumbnailReady(swPieceThumbnailId ThumbnailId, QPixmap Pixmap)
{
	for (int PieceIndex = 0; PieceIndex < static_cast<int>(mPieces.size()); PieceIndex++)
	{
		if (mPieces[PieceIndex].ThumbnailId == ThumbnailId)
		{
			mPieces[PieceIndex].Pixmap = Pixmap;

			emit dataChanged(index(PieceIndex, 0), index(PieceIndex, 0), { Qt::DecorationRole });
		}
	}
}

void swPieceSelectionListModel::SetShowDecoratedPieces(bool Show)
{
	if (Show == mShowDecoratedPieces)
		return;

	mShowDecoratedPieces = Show;

	SetFilter(mFilterString);
}

void swPieceSelectionListModel::SetShowPieceAliases(bool Show)
{
	if (Show == mShowPieceAliases)
		return;

	mShowPieceAliases = Show;

	SetFilter(mFilterString);
}

void swPieceSelectionListModel::SetPieceFilterType(swPieceFilterType Option)
{
	if (Option == mPieceFilterType)
		return;

	mPieceFilterType = Option;

	SetFilter(mFilterString);
}

void swPieceSelectionListModel::SetCaseSensitiveFilter(bool Option)
{
	if (Option == mCaseSensitiveFilter)
		return;

	mCaseSensitiveFilter = Option;

	SetFilter(mFilterString);
}

void swPieceSelectionListModel::SetFileNameFilter(bool Option)
{
	if (Option == mFileNameFilter)
		return;

	mFileNameFilter = Option;

	SetFilter(mFilterString);
}

void swPieceSelectionListModel::SetPieceDescriptionFilter(bool Option)
{
	if (Option == mPieceDescriptionFilter)
		return;

	mPieceDescriptionFilter = Option;

	SetFilter(mFilterString);
}

void swPieceSelectionListModel::SetIconSize(int Size, float DeviceScale)
{
	if (Size == mIconSize && DeviceScale == mDeviceScale)
		return;

	mIconSize = Size;
	mDeviceScale = DeviceScale;

	beginResetModel();

	ReleaseThumbnails();

	endResetModel();

	SetFilter(mFilterString);
}

void swPieceSelectionListModel::SetShowPieceNames(bool Show)
{
	if (Show == mShowPieceNames)
		return;

	mShowPieceNames = Show;

	beginResetModel();
	endResetModel();

	SetFilter(mFilterString);
}

swPieceSelectionListView::swPieceSelectionListView(QWidget* Parent, swPieceSelectionWidget* PieceSelectionWidget)
	: QListView(Parent), mPieceSelectionWidget(PieceSelectionWidget)
{
	setUniformItemSizes(true);
	setResizeMode(QListView::Adjust);
	setWordWrap(false);
	setDragEnabled(true);
	setContextMenuPolicy(Qt::CustomContextMenu);
	setAutoScroll(false);
	setTextElideMode(Qt::ElideMiddle);

	mListModel = new swPieceSelectionListModel(this);
	setModel(mListModel);
	swPieceSelectionItemDelegate* ItemDelegate = new swPieceSelectionItemDelegate(this, mListModel);
	setItemDelegate(ItemDelegate);

	// the popup picks with a single click
	if (mPieceSelectionWidget && mPieceSelectionWidget->GetIsPopup())
		connect(this, &QListView::clicked, this, &swPieceSelectionListView::DoubleClicked);
	else
		connect(this, &QListView::doubleClicked, this, &swPieceSelectionListView::DoubleClicked);
	connect(this, &QListView::customContextMenuRequested, this, &swPieceSelectionListView::CustomContextMenuRequested);

	connect(gApplication, &swApplication::RendererInitialized, this, &swPieceSelectionListView::UpdateIconSize);

	mRequestedIconSize = swGetProfileInteger(mPieceSelectionWidget->GetIsPopup() ? SW_PROFILE_PIECES_POPUP_ICONS : SW_PROFILE_PIECES_LIST_ICONS);
	UpdateIconSize();
}

void swPieceSelectionListView::keyPressEvent(QKeyEvent* KeyEvent)
{
	if (KeyEvent->key() == Qt::Key_Enter)
	{
		swPieceData* PieceData = GetCurrentPiece();

		emit PiecePicked(PieceData);

		return;
	}

	return QListView::keyPressEvent(KeyEvent);
}

void swPieceSelectionListView::DoubleClicked(const QModelIndex& Index)
{
	swPieceData* PieceData = GetListModel()->GetPieceData(Index.row());

	emit PiecePicked(PieceData);
}

void swPieceSelectionListView::CustomContextMenuRequested(QPoint Pos)
{
	if (!mPieceSelectionWidget || mPieceSelectionWidget->GetIsPopup())
		return;

	QMenu* Menu = new QMenu(this);

	QModelIndex Index = indexAt(Pos);
	mContextPieceData = Index.isValid() ? mListModel->GetPieceData(Index.row()) : nullptr;

	QMenu* SetMenu = Menu->addMenu(tr("Add to Palette"));

	const std::vector<swPiecePalette>& Palettes = mPieceSelectionWidget->GetPiecePalettes();

	if (!Palettes.empty())
	{
		for (const swPiecePalette& Palette : Palettes)
			SetMenu->addAction(Palette.Name, mPieceSelectionWidget, &swPieceSelectionWidget::AddToPalette);
	}
	else
	{
		QAction* Action = SetMenu->addAction(tr("None"));
		Action->setEnabled(false);
	}

	QAction* RemoveAction = Menu->addAction(tr("Remove from Palette"), mPieceSelectionWidget, &swPieceSelectionWidget::RemoveFromPalette);
	RemoveAction->setEnabled(mCategoryType == swPieceCategoryType::Palette);

	if (mContextPieceData && mContextPieceData->IsModel())
	{
		Menu->addSeparator();
		Menu->addAction(tr("Open Submodel"), mPieceSelectionWidget, &swPieceSelectionWidget::OpenSubmodel);
	}

	Menu->exec(viewport()->mapToGlobal(Pos));
	delete Menu;
}

void swPieceSelectionListView::SetCategory(swPieceCategoryType Type, int Index)
{
	QModelIndex CurrentIndex = currentIndex();

	if (CurrentIndex.isValid())
		mLastCategoryRow[{mCategoryType, mCategoryIndex}] = CurrentIndex.row();

	mCategoryType = Type;
	mCategoryIndex = Index;

	switch (Type)
	{
	case swPieceCategoryType::AllPieces:
		mListModel->SetCategory(-1);
		break;
	case swPieceCategoryType::PiecesInUse:
		mListModel->SetCurrentModelCategory();
		break;
	case swPieceCategoryType::Submodels:
		mListModel->SetModelsCategory();
		break;
	case swPieceCategoryType::Palette:
		mListModel->SetPaletteCategory(Index);
		break;
	case swPieceCategoryType::Category:
		mListModel->SetCategory(Index);
		break;
	case swPieceCategoryType::Custom:
	case swPieceCategoryType::Count:
		break;
	}

	auto CurrentIt = mLastCategoryRow.find({mCategoryType, mCategoryIndex});
	bool ScrollToTop = true;

	if (CurrentIt != mLastCategoryRow.end())
	{
		CurrentIndex = mListModel->index(CurrentIt->second, 0);

		if (!isRowHidden(CurrentIndex.row()))
		{
			scrollTo(CurrentIndex);
			setCurrentIndex(CurrentIndex);
			ScrollToTop = false;
		}
	}

	if (ScrollToTop)
	{
		scrollToTop();
		setCurrentIndex(indexAt(QPoint(0, 0)));
	}
}

void swPieceSelectionListView::SetCustomPieces(const std::vector<std::pair<swPieceData*, std::string>>& Pieces, int ColorIndex)
{
	mCategoryType = swPieceCategoryType::Custom;

	mListModel->SetCustomPieces(Pieces, ColorIndex);

	setCurrentIndex(mListModel->index(0, 0));
}

void swPieceSelectionListView::SetCurrentPiece(swPieceData* PieceData)
{
	QModelIndex Index = mListModel->GetPieceDataIndex(PieceData);

	if (Index.isValid())
	{
		setCurrentIndex(Index);
		scrollTo(Index, QAbstractItemView::EnsureVisible);
	}
}

void swPieceSelectionListView::SetNoIcons()
{
	SetIconSize(NoIconSize);
}

void swPieceSelectionListView::SetSmallIcons()
{
	SetIconSize(SmallIconSize);
}

void swPieceSelectionListView::SetMediumIcons()
{
	SetIconSize(MediumIconSize);
}

void swPieceSelectionListView::SetLargeIcons()
{
	SetIconSize(LargeIconSize);
}

void swPieceSelectionListView::SetExtraLargeIcons()
{
	SetIconSize(ExtraLargeIconSize);
}

void swPieceSelectionListView::TogglePieceNames()
{
	bool Show = !mListModel->GetShowPieceNames();
	mListModel->SetShowPieceNames(Show);
	swSetProfileInteger(mPieceSelectionWidget->GetIsPopup() ? SW_PROFILE_PIECES_POPUP_NAMES : SW_PROFILE_PIECES_LIST_NAMES, Show);
	UpdateItemGeometry();
}

void swPieceSelectionListView::ToggleDecoratedPieces()
{
	bool Show = !mListModel->GetShowDecoratedPieces();
	mListModel->SetShowDecoratedPieces(Show);
	swSetProfileInteger(mPieceSelectionWidget->GetIsPopup() ? SW_PROFILE_PIECES_POPUP_DECORATED : SW_PROFILE_PIECES_LIST_DECORATED, Show);
}

void swPieceSelectionListView::TogglePieceAliases()
{
	bool Show = !mListModel->GetShowPieceAliases();
	mListModel->SetShowPieceAliases(Show);
	swSetProfileInteger(mPieceSelectionWidget->GetIsPopup() ? SW_PROFILE_PIECES_POPUP_ALIASES : SW_PROFILE_PIECES_LIST_ALIASES, Show);
}

void swPieceSelectionListView::ToggleRegularExpressionFilter()
{
	bool Option = mListModel->GetPieceFilterType() == swPieceFilterType::RegularExpression;
	SetPieceFilterType(Option ? swPieceFilterType::Default : swPieceFilterType::RegularExpression);
}

void swPieceSelectionListView::ToggleCaseSensitiveFilter()
{
	bool Option = !mListModel->GetCaseSensitiveFilter();
	mListModel->SetCaseSensitiveFilter(Option);
	swSetProfileInteger(SW_PROFILE_PIECES_LIST_CASE_SENSITIVE_FILTER, Option);
}

void swPieceSelectionListView::ToggleFileNameFilter()
{
	bool Option = !mListModel->GetFileNameFilter();
	mListModel->SetFileNameFilter(Option);
	swSetProfileInteger(SW_PROFILE_PIECES_LIST_FILE_NAME_FILTER, Option);
}

void swPieceSelectionListView::TogglePieceDescriptionFilter()
{
	bool Option = !mListModel->GetPieceDescriptionFilter();
	mListModel->SetPieceDescriptionFilter(Option);
	swSetProfileInteger(SW_PROFILE_PIECES_LIST_PIECE_DESCRIPTION_FILTER, Option);
}

void swPieceSelectionListView::SetPieceFilterType(swPieceFilterType Option)
{
	mListModel->SetPieceFilterType(Option);
	swSetProfilePieceFilterType(Option);
}

void swPieceSelectionListView::ToggleListMode()
{
	mListModel->ToggleListMode();
	UpdateItemGeometry();
}

void swPieceSelectionListView::ToggleFixedColor()
{
	mListModel->ToggleColorLocked();
}

void swPieceSelectionListView::UpdateInUseCategory()
{
	if (mCategoryType == swPieceCategoryType::PiecesInUse)
		mListModel->SetCurrentModelCategory();
}

void swPieceSelectionListView::UpdateViewMode()
{
	setViewMode(mListModel->GetIconSize() && !mListModel->IsListMode() ? QListView::IconMode : QListView::ListMode);
	setWordWrap(mListModel->IsListMode());
	setDragEnabled(true);
}

void swPieceSelectionListView::SetIconSize(int Size)
{
	mRequestedIconSize = Size;
	swSetProfileInteger(mPieceSelectionWidget->GetIsPopup() ? SW_PROFILE_PIECES_POPUP_ICONS : SW_PROFILE_PIECES_LIST_ICONS, Size);
	UpdateIconSize();
}

void swPieceSelectionListView::UpdateIconSize()
{
	const int Size = gSupportsFramebufferObject ? mRequestedIconSize : NoIconSize;
	setIconSize(QSize(Size, Size));

#if (QT_VERSION >= QT_VERSION_CHECK(5, 6, 0))
	float DeviceScale = devicePixelRatioF();
#else
	float DeviceScale = devicePixelRatio();
#endif

	mListModel->SetIconSize(Size, DeviceScale);
	UpdateViewMode();

	const int PixmapMargin = style()->pixelMetric(QStyle::PM_FocusFrameHMargin, nullptr, this) + 1;
	int Width = Size + 2 * frameWidth() + 2 * PixmapMargin;
	if (verticalScrollBar())
		Width += verticalScrollBar()->sizeHint().width();
	int Height = qMax(Size, fontMetrics().height()) + 2 * frameWidth() + 2 * PixmapMargin;
	if (horizontalScrollBar())
		Height += horizontalScrollBar()->sizeHint().height();
	setMinimumSize(mPieceSelectionWidget->GetIsPopup() ? QSize(0, 0) : QSize(Width, Height));
	UpdateItemGeometry();
}

void swPieceSelectionListView::UpdateItemGeometry()
{
	updateGeometry();

	if (mPieceSelectionWidget->GetIsPopup())
	{
		// Invalidate the layout item caches through the picker splitter.
		for (QWidget* Widget = parentWidget(); Widget; Widget = Widget->parentWidget())
		{
			Widget->updateGeometry();
			if (Widget == mPieceSelectionWidget)
				break;
		}
	}

	emit LayoutChanged();
}

void swPieceSelectionListView::startDrag(Qt::DropActions SupportedActions)
{
	Q_UNUSED(SupportedActions);

	swPieceData* PieceData = GetCurrentPiece();

	if (!PieceData)
		return;

	QByteArray ItemData;
	QDataStream DataStream(&ItemData, QIODevice::WriteOnly);
	DataStream << QString(PieceData->mFileName);

	QMimeData* MimeData = new QMimeData;
	MimeData->setData("application/vnd.studworks-part", ItemData);

	QDrag* Drag = new QDrag(this);
	Drag->setMimeData(MimeData);

	Drag->exec(Qt::CopyAction);
}

QSize swPieceSelectionListView::GetCellSize() const
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	QStyleOptionViewItem Option;
	initViewItemOption(&Option);
#else
	QStyleOptionViewItem Option = viewOptions();
#endif

	return itemDelegate()->sizeHint(Option, model()->index(0, 0));
}

QSize swPieceSelectionListView::sizeHint() const
{
	return GetPreferredSize(QWIDGETSIZE_MAX);
}

int swPieceSelectionListView::GetTileSnapDelta() const
{
	// Measure the laid out first row, so margins, spacing, scaling and the wrapping of the view are all included.
	if (viewMode() != QListView::IconMode || model()->rowCount() < 1)
		return 0;

	const QRect FirstRect = visualRect(model()->index(0, 0));
	const QRect SecondRect = model()->rowCount() > 1 ? visualRect(model()->index(1, 0)) : QRect();
	const bool SingleColumn = !SecondRect.isValid() || SecondRect.top() != FirstRect.top();

	// With a single column the tile width and spacing give the pitch, otherwise the distance between tiles does.
	const int Pitch = SingleColumn ? FirstRect.width() + spacing() : SecondRect.left() - FirstRect.left();

	if (Pitch <= 0)
		return 0;

	QRect LastRect = SingleColumn ? FirstRect : SecondRect;

	for (int Row = 2; !SingleColumn && Row < model()->rowCount(); Row++)
	{
		const QRect Rect = visualRect(model()->index(Row, 0));

		if (Rect.top() != FirstRect.top())
			break;

		LastRect = Rect;
	}

	// The space after the last tile of the row, beyond a right margin matching the left one. The view only fits
	// another tile with at least one pixel to spare, so the snapped width keeps that pixel.
	const int Leftover = viewport()->width() - (LastRect.left() + Pitch) - FirstRect.left() - 1;

	// Trim a small leftover, or widen to fit one more tile when most of one already fits.
	return Leftover * 2 < Pitch ? -Leftover : Pitch - Leftover;
}

QSize swPieceSelectionListView::GetPreferredSize(int MaximumWidth) const
{
	if (model()->rowCount() == 0 && (mListModel->GetIconSize() == NoIconSize || mListModel->IsListMode()))
		return QListView::sizeHint();

	if (mListModel->GetIconSize() == NoIconSize)
		return QSize(500, 350);

	int Columns = mListModel->IsListMode() ? 1 : 4;
	int MaximumRows = 4;

	QSize CellSize = GetCellSize();
	if (mPieceSelectionWidget->GetIsPopup() && !mListModel->IsListMode())
	{
		switch (mListModel->GetIconSize())
		{
		case SmallIconSize:
			Columns = 9;
			MaximumRows = 5;
			break;

		case MediumIconSize:
			Columns = 5;
			MaximumRows = 4;
			break;

		case LargeIconSize:
			Columns = 4;
			MaximumRows = 3;
			break;

		case ExtraLargeIconSize:
			Columns = 3;
			MaximumRows = 4;
			break;
		}
	}

	int WidthOverhead = frameWidth() * 2;
	if (verticalScrollBar())
		WidthOverhead += verticalScrollBar()->sizeHint().width() + 1;

	const int Width = qMin(CellSize.width() * Columns + WidthOverhead, MaximumWidth);
	if (mPieceSelectionWidget->GetIsPopup() && !mListModel->IsListMode() && CellSize.width() > 0)
		Columns = qMin(Columns, qMax(1, (Width - WidthOverhead) / CellSize.width()));

	int Rows = model()->rowCount() ? qMin(MaximumRows, (model()->rowCount() + Columns - 1) / Columns) : MaximumRows;
	QSize Size(Width, CellSize.height() * Rows + frameWidth() * 2);

	return Size;
}

swPieceSelectionWidget::swPieceSelectionWidget(bool IsPopup, QWidget* Parent)
	: QWidget(Parent), mIsPopup(IsPopup)
{
	mSplitter = new QSplitter(this);
	mSplitter->setOrientation(Qt::Vertical);
	mSplitter->setChildrenCollapsible(false);

	// The handle leaves the gap a standard layout leaves between rows, like everywhere else in the panels.
	mSplitter->setHandleWidth(swGetLayoutSpacing(this, Qt::Vertical));

	QWidget* CategoriesGroupWidget = new QWidget(mSplitter);

	// Both halves sit in the splitter inside the panel layout, which provides the margins.
	QVBoxLayout* CategoriesLayout = new QVBoxLayout();
	CategoriesLayout->setContentsMargins(0, 0, 0, 0);
	CategoriesGroupWidget->setLayout(CategoriesLayout);

	QWidget* FilterCategoriesRow = swCreatePanelRow(CategoriesGroupWidget);
	CategoriesLayout->addWidget(FilterCategoriesRow);

	mFilterCategoriesWidget = new QLineEdit(CategoriesGroupWidget);
	mFilterCategoriesWidget->setPlaceholderText(tr("Filter Categories"));
	mFilterCategoriesWidget->setAccessibleName(tr("Filter Categories"));
	mFilterCategoriesWidget->setToolTip(tr("Filter category names to narrow the category list."));
	swSetupPanelEdit(mFilterCategoriesWidget);
	swAddPanelRowWidget(FilterCategoriesRow, mFilterCategoriesWidget);

	mFilterCaseAction = new QAction();
	mFilterCaseAction->setIcon(swCreateInterfaceIcon("widgets", "filter_case"));
	mFilterCaseAction->setToolTip(tr("Match Case"));
	mFilterCaseAction->setCheckable(true);
	mFilterCaseAction->setChecked(false);
	connect(mFilterCaseAction, &QAction::triggered, this, &swPieceSelectionWidget::FilterCaseTriggered);

	swAddPanelActionButton(FilterCategoriesRow, mFilterCaseAction);

	mCategoriesWidget = new QTreeWidget(mSplitter);
	mCategoriesWidget->setHeaderHidden(true);
	mCategoriesWidget->setAccessibleName(tr("Piece Categories"));
	mCategoriesWidget->setToolTip(tr("Choose a category or palette to display its pieces."));
	mCategoriesWidget->setUniformRowHeights(true);
	mCategoriesWidget->setRootIsDecorated(false);

	CategoriesLayout->addWidget(mCategoriesWidget);

	QWidget* PiecesGroupWidget = new QWidget(mSplitter);

	QVBoxLayout* PiecesLayout = new QVBoxLayout();
	PiecesLayout->setContentsMargins(0, 0, 0, 0);
	PiecesGroupWidget->setLayout(PiecesLayout);

	QWidget* SearchRow = swCreatePanelRow(PiecesGroupWidget);
	PiecesLayout->addWidget(SearchRow);

	mFilterWidget = new QLineEdit(PiecesGroupWidget);
	mFilterWidget->setPlaceholderText(tr("Filter Pieces"));
	mFilterWidget->setAccessibleName(tr("Filter Pieces"));
	mFilterWidget->setToolTip(tr("Filter the displayed pieces using the matching options."));
	swSetupPanelEdit(mFilterWidget);
	swAddPanelRowWidget(SearchRow, mFilterWidget);

	QMenu* OptionsMenu = new QMenu(this);
	swAddPanelOptionsButton(SearchRow, OptionsMenu, tr("Configure piece filtering and list display."), tr("Piece List Options"));
	connect(OptionsMenu, &QMenu::aboutToShow, this, &swPieceSelectionWidget::OptionsMenuAboutToShow);

	mPiecesWidget = new swPieceSelectionListView(PiecesGroupWidget, this);
	mPiecesWidget->setAccessibleName(tr("Available Pieces"));
	mPiecesWidget->setToolTip(tr("Select a piece from the filtered list."));
	if (mIsPopup)
		mPiecesWidget->setMinimumSize(0, 0);
	PiecesLayout->addWidget(mPiecesWidget);

	QHBoxLayout* Layout = new QHBoxLayout(this);
	Layout->addWidget(mSplitter);
	setLayout(Layout);

	connect(mPiecesWidget, &swPieceSelectionListView::PiecePicked, this, &swPieceSelectionWidget::PieceViewPiecePicked);
	connect(mPiecesWidget, &swPieceSelectionListView::LayoutChanged, this, &swPieceSelectionWidget::LayoutChanged);
	connect(mPiecesWidget->selectionModel(), &QItemSelectionModel::currentChanged, this, &swPieceSelectionWidget::PieceViewSelectionChanged);
	connect(mFilterWidget, &QLineEdit::textChanged, this, &swPieceSelectionWidget::FilterChanged);
	connect(mCategoriesWidget, &QTreeWidget::currentItemChanged, this, &swPieceSelectionWidget::CategoryChanged);
	connect(mFilterCategoriesWidget, &QLineEdit::textChanged, this, &swPieceSelectionWidget::FilterCategoriesChanged);

	LoadPiecePalettes();
	UpdateCategories();

	mSplitter->setStretchFactor(0, 0);
	mSplitter->setStretchFactor(1, 1);

	// Dragging the splitter by hand snaps the categories back to the longest name once the mouse is released.
	mSplitter->handle(1)->installEventFilter(this);
}

bool swPieceSelectionWidget::eventFilter(QObject* Object, QEvent* Event)
{
	if (Object == mSplitter->handle(1) && Event->type() == QEvent::MouseButtonRelease)
		QTimer::singleShot(0, this, &swPieceSelectionWidget::SnapCategoriesWidth);

	return QWidget::eventFilter(Object, Event);
}

bool swPieceSelectionWidget::event(QEvent* Event)
{
	if (Event->type() == QEvent::ShortcutOverride)
	{
		QKeyEvent* KeyEvent = (QKeyEvent*)Event;
		int Key = KeyEvent->key();

		if (KeyEvent->modifiers() == Qt::NoModifier && Key >= Qt::Key_A && Key <= Qt::Key_Z)
			Event->accept();

		switch (Key)
		{
		case Qt::Key_Down:
		case Qt::Key_Up:
		case Qt::Key_Left:
		case Qt::Key_Right:
		case Qt::Key_Home:
		case Qt::Key_End:
		case Qt::Key_PageUp:
		case Qt::Key_PageDown:
		case Qt::Key_Asterisk:
		case Qt::Key_Plus:
		case Qt::Key_Minus:
			Event->accept();
			break;
		}
	}

	return QWidget::event(Event);
}

void swPieceSelectionWidget::LoadState(QSettings& Settings)
{
	QList<int> Sizes = Settings.value("PieceSelectionSplitter").value<QList<int>>();

	if (Sizes.size() != 2)
	{
		int Length = mSplitter->orientation() == Qt::Horizontal ? mSplitter->width() : mSplitter->height();
		Sizes = { Length / 3, 2 * Length / 3 };
	}

	mSplitter->setSizes(Sizes);
}

void swPieceSelectionWidget::SaveState(QSettings& Settings)
{
	QList<int> Sizes = mSplitter->sizes();
	Settings.setValue("PieceSelectionSplitter", QVariant::fromValue(Sizes));
}

void swPieceSelectionWidget::SetCurrentPiece(swPieceData* PieceData)
{
	mPiecesWidget->SetCurrentPiece(PieceData);
	mPiecesWidget->setFocus();
}

void swPieceSelectionWidget::SetCategory(swPieceCategoryType Type, int Index)
{
	for (int Row = 0; Row < mCategoriesWidget->topLevelItemCount(); Row++)
	{
		QTreeWidgetItem* Item = mCategoriesWidget->topLevelItem(Row);
		swPieceCategoryType ItemType = static_cast<swPieceCategoryType>(Item->data(0, static_cast<int>(swPieceCategoryRole::Type)).toInt());

		if (ItemType != Type)
			continue;

		if (Type == swPieceCategoryType::Palette || Type == swPieceCategoryType::Category)
		{
			int ItemIndex = Item->data(0, static_cast<int>(swPieceCategoryRole::Index)).toInt();

			if (ItemIndex != Index)
				continue;
		}

		mCategoriesWidget->setCurrentItem(Item);
		break;
	}
}

void swPieceSelectionWidget::SetCustomPieces(const std::vector<std::pair<swPieceData*, std::string>>& Pieces, int ColorIndex)
{
	mSplitter->widget(0)->setVisible(false);

	mPiecesWidget->SetCustomPieces(Pieces, ColorIndex);
}

void swPieceSelectionWidget::SetOrientation(Qt::Orientation Orientation)
{
	mSplitter->setOrientation(Orientation);

	int Length = mSplitter->orientation() == Qt::Horizontal ? mSplitter->width() : mSplitter->height();
	QList<int> Sizes = { Length / 3, 2 * Length / 3 };

	mSplitter->setSizes(Sizes);
}

void swPieceSelectionWidget::SnapCategoriesWidth()
{
	QDockWidget* DockWidget = qobject_cast<QDockWidget*>(parent());

	if (mIsPopup || !swGetPreferences().mPiecesSnapToTiles || !DockWidget || DockWidget->isFloating())
		return;

	if ((mDockArea != Qt::TopDockWidgetArea && mDockArea != Qt::BottomDockWidgetArea) || mSplitter->orientation() != Qt::Horizontal || mSplitter->widget(0)->isHidden())
		return;

	// Measure every category, including the ones hidden by the filter, so filtering never changes the width.
	QStyleOptionViewItem Option;
	Option.initFrom(mCategoriesWidget);
	Option.widget = mCategoriesWidget;
	Option.font = mCategoriesWidget->font();

	QAbstractItemModel* Model = mCategoriesWidget->model();
	int TextWidth = 0;

	for (int Row = 0; Row < Model->rowCount(); Row++)
		TextWidth = qMax(TextWidth, mCategoriesWidget->itemDelegate()->sizeHint(Option, Model->index(Row, 0)).width());

	if (!TextWidth)
		return;

	int Width = TextWidth + 2 * mCategoriesWidget->frameWidth();
	if (mCategoriesWidget->verticalScrollBar()->isVisible())
		Width += mCategoriesWidget->verticalScrollBar()->width();

	Width = qMax(Width, mSplitter->widget(0)->minimumSizeHint().width());

	QList<int> Sizes = mSplitter->sizes();

	if (Sizes.size() != 2 || Sizes[0] == Width)
		return;

	const int Length = Sizes[0] + Sizes[1];
	mSplitter->setSizes({ Width, Length - Width });
}

void swPieceSelectionWidget::DockLocationChanged(Qt::DockWidgetArea Area)
{
	mDockArea = Area;

	if (Area == Qt::LeftDockWidgetArea || Area == Qt::RightDockWidgetArea)
		mSplitter->setOrientation(Qt::Vertical);
	else
		mSplitter->setOrientation(Qt::Horizontal);

	QTimer::singleShot(0, this, &swPieceSelectionWidget::SnapCategoriesWidth);
}

void swPieceSelectionWidget::resizeEvent(QResizeEvent* Event)
{
	QDockWidget* DockWidget = qobject_cast<QDockWidget*>(parent());

	if (DockWidget && DockWidget->isFloating())
	{
		if (Event->size().width() > Event->size().height())
			mSplitter->setOrientation(Qt::Horizontal);
		else
			mSplitter->setOrientation(Qt::Vertical);
	}

	QWidget::resizeEvent(Event);
}

void swPieceSelectionWidget::FilterCategoriesChanged(const QString& Text)
{
	if (mFilterCategoriesAction)
	{
		if (Text.isEmpty())
		{
			delete mFilterCategoriesAction;
			mFilterCategoriesAction = nullptr;
		}
	}
	else
	{
		if (!Text.isEmpty())
		{
			mFilterCategoriesAction = mFilterCategoriesWidget->addAction(QIcon(":/stylesheet/close.svg"), QLineEdit::TrailingPosition);
			connect(mFilterCategoriesAction, &QAction::triggered, this, &swPieceSelectionWidget::FilterCategoriesTriggered);
		}
	}

	bool Hide = true;
	Qt::CaseSensitivity MatchCase = mFilterCaseAction->isChecked() ? Qt::CaseSensitive : Qt::CaseInsensitive;
	mCategoriesWidget->setUpdatesEnabled(false);
	for (int CategoryIdx = 0; CategoryIdx < mCategoriesWidget->topLevelItemCount(); CategoryIdx++)
	{
		QTreeWidgetItem* CategoryItem = mCategoriesWidget->topLevelItem(CategoryIdx);
		Hide = false;
		if (!CategoryItem->text(0).contains(Text, MatchCase))
			Hide = true;
		CategoryItem->setHidden(Hide);
	}
	mCategoriesWidget->setUpdatesEnabled(true);
	mCategoriesWidget->update();
}

void swPieceSelectionWidget::FilterChanged(const QString& Text)
{
	if (mFilterAction)
	{
		if (Text.isEmpty())
		{
			delete mFilterAction;
			mFilterAction = nullptr;
		}
	}
	else
	{
		if (!Text.isEmpty())
		{
			mFilterAction = mFilterWidget->addAction(QIcon(":/stylesheet/close.svg"), QLineEdit::TrailingPosition);
			connect(mFilterAction, &QAction::triggered, this, &swPieceSelectionWidget::FilterTriggered);
		}
	}

	mPiecesWidget->GetListModel()->SetFilter(Text);
}

void swPieceSelectionWidget::FilterCategoriesTriggered()
{
	mFilterCategoriesWidget->clear();
}

void swPieceSelectionWidget::FilterCaseTriggered()
{
	if (!mFilterCategoriesWidget->text().isEmpty())
		FilterCategoriesChanged(mFilterCategoriesWidget->text());
}

void swPieceSelectionWidget::FilterTriggered()
{
	mFilterWidget->clear();
}

void swPieceSelectionWidget::CategoryChanged(QTreeWidgetItem* Current, QTreeWidgetItem* Previous)
{
	Q_UNUSED(Previous);

	if (!Current)
		return;

	int Type = Current->data(0, static_cast<int>(swPieceCategoryRole::Type)).toInt();
	int Index = Current->data(0, static_cast<int>(swPieceCategoryRole::Index)).toInt();

	mPiecesWidget->SetCategory(static_cast<swPieceCategoryType>(Type), Index);
}

void swPieceSelectionWidget::PieceViewSelectionChanged(const QModelIndex& Current, const QModelIndex& Previous)
{
	Q_UNUSED(Current);
	Q_UNUSED(Previous);

	emit CurrentPieceChanged(mPiecesWidget->GetCurrentPiece());
}

void swPieceSelectionWidget::PieceViewPiecePicked(swPieceData* PieceData)
{
	emit PiecePicked(PieceData);
}

void swPieceSelectionWidget::OptionsMenuAboutToShow()
{
	QMenu* Menu = (QMenu*)sender();
	Menu->clear();

	if (!mIsPopup)
	{
		Menu->addAction(tr("Edit Palettes..."), this, &swPieceSelectionWidget::EditPiecePalettes);

		Menu->addSeparator();
	}

	swPieceSelectionListModel* ListModel = mPiecesWidget->GetListModel();

	if (ListModel->GetIconSize() != swPieceSelectionListView::NoIconSize && !ListModel->IsListMode())
	{
		QAction* PieceNames = Menu->addAction(tr("Show Piece Names"), mPiecesWidget, &swPieceSelectionListView::TogglePieceNames);
		PieceNames->setCheckable(true);
		PieceNames->setChecked(ListModel->GetShowPieceNames());
	}

	QAction* DecoratedPieces = Menu->addAction(tr("Show Decorated Pieces"), mPiecesWidget, &swPieceSelectionListView::ToggleDecoratedPieces);
	DecoratedPieces->setCheckable(true);
	DecoratedPieces->setChecked(ListModel->GetShowDecoratedPieces());

	QAction* PieceAliases = Menu->addAction(tr("Show Piece Aliases"), mPiecesWidget, &swPieceSelectionListView::TogglePieceAliases);
	PieceAliases->setCheckable(true);
	PieceAliases->setChecked(ListModel->GetShowPieceAliases());

	if (!mIsPopup)
		gMainWindow->AddMenuCommand(Menu, tr("Snap Panel Width to Tiles"), SW_VIEW_PIECES_TOGGLE_SNAP_TO_TILES, swGetPreferences().mPiecesSnapToTiles);

	Menu->addSeparator();

	if (gSupportsFramebufferObject)
	{
		QMenu* IconMenu = Menu->addMenu(tr("Icons"));

		QActionGroup* IconGroup = new QActionGroup(Menu);

		QAction* NoIcons = IconMenu->addAction(tr("No Icons"), mPiecesWidget, &swPieceSelectionListView::SetNoIcons);
		NoIcons->setCheckable(true);
		NoIcons->setChecked(ListModel->GetIconSize() == swPieceSelectionListView::NoIconSize);
		IconGroup->addAction(NoIcons);

		QAction* SmallIcons = IconMenu->addAction(tr("Small Icons"), mPiecesWidget, &swPieceSelectionListView::SetSmallIcons);
		SmallIcons->setCheckable(true);
		SmallIcons->setChecked(ListModel->GetIconSize() == swPieceSelectionListView::SmallIconSize);
		IconGroup->addAction(SmallIcons);

		QAction* MediumIcons = IconMenu->addAction(tr("Medium Icons"), mPiecesWidget, &swPieceSelectionListView::SetMediumIcons);
		MediumIcons->setCheckable(true);
		MediumIcons->setChecked(ListModel->GetIconSize() == swPieceSelectionListView::MediumIconSize);
		IconGroup->addAction(MediumIcons);

		QAction* LargeIcons = IconMenu->addAction(tr("Large Icons"), mPiecesWidget, &swPieceSelectionListView::SetLargeIcons);
		LargeIcons->setCheckable(true);
		LargeIcons->setChecked(ListModel->GetIconSize() == swPieceSelectionListView::LargeIconSize);
		IconGroup->addAction(LargeIcons);

		QAction* ExtraLargeIcons = IconMenu->addAction(tr("Extra Large Icons"), mPiecesWidget, &swPieceSelectionListView::SetExtraLargeIcons);
		ExtraLargeIcons->setCheckable(true);
		ExtraLargeIcons->setChecked(ListModel->GetIconSize() == swPieceSelectionListView::ExtraLargeIconSize);
		IconGroup->addAction(ExtraLargeIcons);

		if (ListModel->GetIconSize() != swPieceSelectionListView::NoIconSize)
		{
			IconMenu->addSeparator();

			QAction* ListMode = IconMenu->addAction(tr("List Mode"), mPiecesWidget, &swPieceSelectionListView::ToggleListMode);
			ListMode->setCheckable(true);
			ListMode->setChecked(ListModel->IsListMode());

			if (!mIsPopup)
			{
				QAction* FixedColor = IconMenu->addAction(tr("Lock Color"), mPiecesWidget, &swPieceSelectionListView::ToggleFixedColor);
				FixedColor->setCheckable(true);
				FixedColor->setChecked(ListModel->IsColorLocked());
			}
		}
	}

	QMenu* FilterMenu = Menu->addMenu(tr("Filter"));

	QAction* RegularExpressionFilter = FilterMenu->addAction(tr("Regular Expression"), mPiecesWidget, &swPieceSelectionListView::ToggleRegularExpressionFilter);
	RegularExpressionFilter->setCheckable(true);
	RegularExpressionFilter->setChecked(ListModel->GetPieceFilterType() == swPieceFilterType::RegularExpression);

	QAction* CaseSensitiveFilter = FilterMenu->addAction(tr("Match Case"), mPiecesWidget, &swPieceSelectionListView::ToggleCaseSensitiveFilter);
	CaseSensitiveFilter->setCheckable(true);
	CaseSensitiveFilter->setChecked(ListModel->GetCaseSensitiveFilter());

	FilterMenu->addSeparator();

	QAction* FileNameFilter = FilterMenu->addAction(tr("Piece ID"), mPiecesWidget, &swPieceSelectionListView::ToggleFileNameFilter);
	FileNameFilter->setCheckable(true);
	FileNameFilter->setChecked(ListModel->GetFileNameFilter());
	FileNameFilter->setEnabled(ListModel->GetPieceDescriptionFilter());

	QAction* PieceDescriptionFilter = FilterMenu->addAction(tr("Piece Description"), mPiecesWidget, &swPieceSelectionListView::TogglePieceDescriptionFilter);
	PieceDescriptionFilter->setCheckable(true);
	PieceDescriptionFilter->setChecked(ListModel->GetPieceDescriptionFilter());
	PieceDescriptionFilter->setEnabled(ListModel->GetFileNameFilter());

	if (gSupportsFramebufferObject && gMainWindow)
	{
		QMenu* RenderingMenu = Menu->addMenu(tr("Rendering"));
		gMainWindow->AddRenderTargetActions(RenderingMenu, swRenderTarget::Pieces);
	}
}

void swPieceSelectionWidget::EditPiecePalettes()
{
	swPiecePaletteDialog Dialog(this, mPiecePalettes);

	if (Dialog.exec() != QDialog::Accepted)
		return;

	SavePiecePalettes();
	UpdateCategories();
}

void swPieceSelectionWidget::UpdateThumbnails()
{
	mPiecesWidget->GetListModel()->UpdateThumbnails();
}

void swPieceSelectionWidget::SetDefaultPiece()
{
	for (int CategoryIdx = 0; CategoryIdx < mCategoriesWidget->topLevelItemCount(); CategoryIdx++)
	{
		QTreeWidgetItem* CategoryItem = mCategoriesWidget->topLevelItem(CategoryIdx);

		if (CategoryItem->text(0) == "Brick")
		{
			mCategoriesWidget->setCurrentItem(CategoryItem);
			break;
		}
	}
}

void swPieceSelectionWidget::LoadPiecePalettes()
{
	QByteArray Buffer = swGetProfileBuffer(SW_PROFILE_PIECE_PALETTES);
	QJsonDocument Document = QJsonDocument::fromJson(Buffer);

	if (Document.isNull())
		Document = QJsonDocument::fromJson((QString("{ \"Version\":1, \"Palettes\": { \"%1\": [] } }").arg(tr("Favorites"))).toUtf8());

	QJsonObject RootObject = Document.object();
	mPiecePalettes.clear();

	int Version = RootObject["Version"].toInt(0);
	if (Version != 1)
		return;

	QJsonObject PalettesObject = RootObject["Palettes"].toObject();

	for (QJsonObject::const_iterator ElementIt = PalettesObject.constBegin(); ElementIt != PalettesObject.constEnd(); ElementIt++)
	{
		if (!ElementIt.value().isArray())
			continue;

		swPiecePalette Palette;
		Palette.Name = ElementIt.key();

		const QJsonArray Pieces = ElementIt.value().toArray();

		for (const QJsonValue& Piece : Pieces)
			Palette.Pieces.emplace_back(Piece.toString().toStdString());

		mPiecePalettes.emplace_back(std::move(Palette));
	}
}

void swPieceSelectionWidget::SavePiecePalettes()
{
	QJsonObject RootObject;

	RootObject["Version"] = 1;
	QJsonObject PalettesObject;

	for (const swPiecePalette& Palette : mPiecePalettes)
	{
		QJsonArray Pieces;

		for (const std::string& PieceId : Palette.Pieces)
			Pieces.append(QString::fromStdString(PieceId));

		PalettesObject[Palette.Name] = Pieces;
	}

	RootObject["Palettes"] = PalettesObject;

	QByteArray Buffer = QJsonDocument(RootObject).toJson();
	swSetProfileBuffer(SW_PROFILE_PIECE_PALETTES, Buffer);
}

void swPieceSelectionWidget::AddToPalette()
{
	swPieceData* PieceData = mPiecesWidget->GetContextInfo();
	if (!PieceData)
		return;

	QString SetName = ((QAction*)sender())->text();

	std::vector<swPiecePalette>::iterator SetIt = std::find_if(mPiecePalettes.begin(), mPiecePalettes.end(), [&SetName](const swPiecePalette& Set)
	{
		return Set.Name == SetName;
	});

	if (SetIt == mPiecePalettes.end())
		return;

	std::string PieceId = swGetPiecesLibrary()->GetPieceId(PieceData);
	std::vector<std::string>& Pieces = SetIt->Pieces;

	if (std::find(Pieces.begin(), Pieces.end(), PieceId) == Pieces.end())
	{
		Pieces.emplace_back(PieceId);
		SavePiecePalettes();
	}
}

void swPieceSelectionWidget::RemoveFromPalette()
{
	swPieceData* PieceData = mPiecesWidget->GetContextInfo();
	if (!PieceData)
		return;

	QTreeWidgetItem* CurrentItem = mCategoriesWidget->currentItem();
	if (!CurrentItem || CurrentItem->data(0, static_cast<int>(swPieceCategoryRole::Type)) != static_cast<int>(swPieceCategoryType::Palette))
		return;

	int SetIndex = CurrentItem->data(0, static_cast<int>(swPieceCategoryRole::Index)).toInt();
	swPiecePalette& Palette = mPiecePalettes[SetIndex];

	std::string PieceId = swGetPiecesLibrary()->GetPieceId(PieceData);
	std::vector<std::string>::iterator PieceIt = std::find(Palette.Pieces.begin(), Palette.Pieces.end(), PieceId);

	if (PieceIt != Palette.Pieces.end())
	{
		Palette.Pieces.erase(PieceIt);
		mPiecesWidget->SetCategory(swPieceCategoryType::Palette, SetIndex);
		SavePiecePalettes();
	}
}

void swPieceSelectionWidget::OpenSubmodel()
{
	swPieceData* PieceData = mPiecesWidget->GetContextInfo();

	if (!PieceData || !PieceData->IsModel())
		return;

	swModel* Model = PieceData->GetModel();

	swGetActiveProject()->SetActiveModel(Model, true);
}

void swPieceSelectionWidget::UpdateInUseCategory()
{
	mPiecesWidget->UpdateInUseCategory();
}

void swPieceSelectionWidget::UpdateCategories()
{
	QTreeWidgetItem* CurrentItem = mCategoriesWidget->currentItem();
	swPieceCategoryType CurrentType = swPieceCategoryType::Count;
	int CurrentIndex = -1;

	if (CurrentItem)
	{
		CurrentType = static_cast<swPieceCategoryType>(CurrentItem->data(0, static_cast<int>(swPieceCategoryRole::Type)).toInt());
		CurrentIndex = CurrentItem->data(0, static_cast<int>(swPieceCategoryRole::Index)).toInt();
		CurrentItem = nullptr;
	}

	mCategoriesWidget->clear();

	mAllPiecesCategoryItem = new QTreeWidgetItem(mCategoriesWidget, QStringList(tr("All Pieces")));
	mAllPiecesCategoryItem->setData(0, static_cast<int>(swPieceCategoryRole::Type), static_cast<int>(swPieceCategoryType::AllPieces));

	if (CurrentType == swPieceCategoryType::AllPieces && CurrentIndex == 0)
		CurrentItem = mAllPiecesCategoryItem;

	QTreeWidgetItem* CurrentModelCategoryItem = new QTreeWidgetItem(mCategoriesWidget, QStringList(tr("In Use")));
	CurrentModelCategoryItem->setData(0, static_cast<int>(swPieceCategoryRole::Type), static_cast<int>(swPieceCategoryType::PiecesInUse));

	if (CurrentType == swPieceCategoryType::PiecesInUse && CurrentIndex == 0)
		CurrentItem = CurrentModelCategoryItem;

	QTreeWidgetItem* SubmodelsCategoryItem = new QTreeWidgetItem(mCategoriesWidget, QStringList(tr("Submodels")));
	SubmodelsCategoryItem->setData(0, static_cast<int>(swPieceCategoryRole::Type), static_cast<int>(swPieceCategoryType::Submodels));

	if (CurrentType == swPieceCategoryType::Submodels && CurrentIndex == 0)
		CurrentItem = SubmodelsCategoryItem;

	for (int PaletteIdx = 0; PaletteIdx < static_cast<int>(mPiecePalettes.size()); PaletteIdx++)
	{
		const swPiecePalette& Set = mPiecePalettes[PaletteIdx];
		QTreeWidgetItem* PaletteCategoryItem = new QTreeWidgetItem(mCategoriesWidget, QStringList(Set.Name));
		PaletteCategoryItem->setData(0, static_cast<int>(swPieceCategoryRole::Type), static_cast<int>(swPieceCategoryType::Palette));
		PaletteCategoryItem->setData(0, static_cast<int>(swPieceCategoryRole::Index), PaletteIdx);

		if (CurrentType == swPieceCategoryType::Palette && CurrentIndex == PaletteIdx)
			CurrentItem = PaletteCategoryItem;
	}

	for (int CategoryIdx = 0; CategoryIdx < static_cast<int>(gCategories.size()); CategoryIdx++)
	{
		QTreeWidgetItem* CategoryItem = new QTreeWidgetItem(mCategoriesWidget, QStringList(gCategories[CategoryIdx].Name));
		CategoryItem->setData(0, static_cast<int>(swPieceCategoryRole::Type), static_cast<int>(swPieceCategoryType::Category));
		CategoryItem->setData(0, static_cast<int>(swPieceCategoryRole::Index), CategoryIdx);

		if (CurrentType == swPieceCategoryType::Category && CurrentIndex == CategoryIdx)
			CurrentItem = CategoryItem;
	}

	if (CurrentItem)
		mCategoriesWidget->setCurrentItem(CurrentItem);

	QTimer::singleShot(0, this, &swPieceSelectionWidget::SnapCategoriesWidth);
}

void swPieceSelectionWidget::UpdateModels()
{
	QTreeWidgetItem* CurrentItem = mCategoriesWidget->currentItem();

	if (CurrentItem && CurrentItem->data(0, static_cast<int>(swPieceCategoryRole::Type)) == static_cast<int>(swPieceCategoryType::Submodels))
		mPiecesWidget->SetCategory(swPieceCategoryType::Submodels, 0);
}
