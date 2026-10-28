#include "sw_global.h"
#include "sw_widget_selection_part.h"
#include "sw_window_utilities.h"
#include "sw_dialog_palette_part.h"
#include "sw_profile.h"
#include "sw_application.h"
#include "sw_window_main.h"
#include "sw_library.h"
#include "sw_manager_thumbnail.h"
#include "sw_project.h"
#include "sw_data_part.h"
#include "sw_extensions_opengl.h"
#include "sw_category.h"
#include "sw_track_train.h"
#include "sw_filter.h"
#include "sw_colors.h"

Q_DECLARE_METATYPE(QList<int>)

void swPartSelectionItemDelegate::initStyleOption(QStyleOptionViewItem* Option, const QModelIndex& Index) const
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

void swPartSelectionItemDelegate::paint(QPainter* Painter, const QStyleOptionViewItem& Option, const QModelIndex& Index) const
{
	mListModel->RequestThumbnail(Index.row());
	QStyledItemDelegate::paint(Painter, Option, Index);
}

QSize swPartSelectionItemDelegate::sizeHint(const QStyleOptionViewItem& Option, const QModelIndex& Index) const
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

swPartSelectionListModel::swPartSelectionListModel(QObject* Parent)
	: QAbstractListModel(Parent)
{
	mListView = (swPartSelectionListView*)Parent;
	const bool IsPopup = mListView->GetPartSelectionWidget()->GetIsPopup();

	mIconSize = 0;
	mShowPartNames = swGetProfileInteger(IsPopup ? SW_PROFILE_PARTS_POPUP_NAMES : SW_PROFILE_PARTS_LIST_NAMES);
	mListMode = swGetProfileInteger(IsPopup ? SW_PROFILE_PARTS_POPUP_LIST_MODE : SW_PROFILE_PARTS_LIST_LIST_MODE);
	mShowDecoratedParts = swGetProfileInteger(IsPopup ? SW_PROFILE_PARTS_POPUP_DECORATED : SW_PROFILE_PARTS_LIST_DECORATED);
	mShowPartAliases = swGetProfileInteger(IsPopup ? SW_PROFILE_PARTS_POPUP_ALIASES : SW_PROFILE_PARTS_LIST_ALIASES);
	mCaseSensitiveFilter = swGetProfileInteger(SW_PROFILE_PARTS_LIST_CASE_SENSITIVE_FILTER);
	mFileNameFilter = swGetProfileInteger(SW_PROFILE_PARTS_LIST_FILE_NAME_FILTER);
	mPartDescriptionFilter = swGetProfileInteger(SW_PROFILE_PARTS_LIST_PART_DESCRIPTION_FILTER);
	mPartFilterType = swGetProfilePartFilterType();

	uint ColorCode = IsPopup ? SW_COLOR_NO_COLOR : swGetProfileUnsignedInteger(SW_PROFILE_PARTS_LIST_COLOR);
	int LockedFlag = IsPopup ? 0 : swGetProfileInteger(SW_PROFILE_PARTS_LIST_COLOR_LOCKED);

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

	connect(swGetPartsLibrary()->GetThumbnailManager(), &swThumbnailManager::ThumbnailReady, this, &swPartSelectionListModel::ThumbnailReady);
}

swPartSelectionListModel::~swPartSelectionListModel()
{
	ReleaseThumbnails();
}

void swPartSelectionListModel::UpdateThumbnails()
{
	beginResetModel();

	ReleaseThumbnails();

	endResetModel();

	SetFilter(mFilterString);
}

void swPartSelectionListModel::SetColorIndex(int ColorIndex)
{
	if (mColorLocked || ColorIndex == mColorIndex)
		return;

	mColorIndex = ColorIndex;

	UpdateThumbnails();
}

void swPartSelectionListModel::ToggleColorLocked()
{
	if (mListView->GetPartSelectionWidget()->GetIsPopup())
		return;

	mColorLocked = !mColorLocked;

	SetColorIndex(gMainWindow->GetInsertColorIndex());
	if (mColorLocked)
	{
		swSetProfileUnsignedInteger(SW_PROFILE_PARTS_LIST_COLOR, swGetColorCode(mColorIndex));
		swSetProfileInteger(SW_PROFILE_PARTS_LIST_COLOR_LOCKED, 1);
	}
	else
	{
		swSetProfileInteger(SW_PROFILE_PARTS_LIST_COLOR_LOCKED, 0);
		swRemoveProfileKey(SW_PROFILE_PARTS_LIST_COLOR);
	}
}

void swPartSelectionListModel::ToggleListMode()
{
	mListMode = !mListMode;

	mListView->UpdateViewMode();
	swSetProfileInteger(mListView->GetPartSelectionWidget()->GetIsPopup() ? SW_PROFILE_PARTS_POPUP_LIST_MODE : SW_PROFILE_PARTS_LIST_LIST_MODE, mListMode);
}

void swPartSelectionListModel::SetCategory(int CategoryIndex)
{
	beginResetModel();

	ReleaseThumbnails();

	swPartsLibrary* Library = swGetPartsLibrary();
	std::vector<swPartData*> SingleParts, GroupedParts;

	if (CategoryIndex != -1)
		Library->GetCategoryEntries(CategoryIndex, false, SingleParts, GroupedParts);
	else
	{
		SingleParts = Library->GetParts();

		swModel* ActiveModel = gMainWindow->GetActiveModel();

		for (size_t PartIndex = 0; PartIndex < SingleParts.size(); )
		{
			swPartData* PartData = SingleParts[PartIndex];

			if (!PartData->IsModel() || !PartData->GetModel()->IncludesModel(ActiveModel))
				PartIndex++;
			else
				SingleParts.erase(SingleParts.begin() + PartIndex);
		}
	}

	auto swPartSortFunction=[](const swPartData* a, const swPartData* b)
	{
		return strcmp(a->m_strDescription, b->m_strDescription) < 0;
	};

	std::sort(SingleParts.begin(), SingleParts.end(), swPartSortFunction);

	mParts.resize(SingleParts.size());

	for (size_t PartIndex = 0; PartIndex < SingleParts.size(); PartIndex++)
		mParts[PartIndex].PartData = SingleParts[PartIndex];

	endResetModel();

	SetFilter(mFilterString);
}

void swPartSelectionListModel::SetModelsCategory()
{
	beginResetModel();

	ReleaseThumbnails();
	mParts.clear();

	const std::vector<std::unique_ptr<swModel>>& Models = swGetActiveProject()->GetModels();
	swModel* ActiveModel = gMainWindow->GetActiveModel();

	for (const std::unique_ptr<swModel>& Model : Models)
		if (!Model->IncludesModel(ActiveModel))
			mParts.emplace_back().PartData = Model->GetPartData();

	auto swPartSortFunction = [](const swPartSelectionListModelEntry& a, const swPartSelectionListModelEntry& b)
	{
		return strcmp(a.PartData->m_strDescription, b.PartData->m_strDescription) < 0;
	};

	std::sort(mParts.begin(), mParts.end(), swPartSortFunction);

	endResetModel();

	SetFilter(mFilterString);
}

void swPartSelectionListModel::SetPaletteCategory(int SetIndex)
{
	beginResetModel();

	ReleaseThumbnails();
	mParts.clear();

	swPartSelectionWidget* PartSelectionWidget = mListView->GetPartSelectionWidget();
	const std::vector<swPartPalette>& Palettes = PartSelectionWidget->GetPartPalettes();
	std::vector<swPartData*> PartsList = swGetPartsLibrary()->GetPartsFromSet(Palettes[SetIndex].Parts);

	auto swPartSortFunction = [](const swPartData* a, const swPartData* b)
	{
		return strcmp(a->m_strDescription, b->m_strDescription) < 0;
	};

	std::sort(PartsList.begin(), PartsList.end(), swPartSortFunction);

	mParts.reserve(PartsList.size());

	for (swPartData* PartData : PartsList)
		mParts.emplace_back().PartData = PartData;

	endResetModel();

	SetFilter(mFilterString);
}

void swPartSelectionListModel::SetCurrentModelCategory()
{
	beginResetModel();

	ReleaseThumbnails();
	mParts.clear();

	swModel* ActiveModel = gMainWindow->GetActiveModel();
	swPartsList PartsList;

	if (ActiveModel)
		ActiveModel->GetPartsList(gDefaultColor, true, true, PartsList);

	for (const auto& PartIt : PartsList)
		mParts.emplace_back().PartData = (swPartData*)PartIt.first;

	auto swPartSortFunction = [](const swPartSelectionListModelEntry& a, const swPartSelectionListModelEntry& b)
	{
		return strcmp(a.PartData->m_strDescription, b.PartData->m_strDescription) < 0;
	};

	std::sort(mParts.begin(), mParts.end(), swPartSortFunction);

	endResetModel();

	SetFilter(mFilterString);
}

void swPartSelectionListModel::SetCustomParts(const std::vector<std::pair<swPartData*, std::string>>& Parts, int ColorIndex)
{
	beginResetModel();

	mColorLocked = true;
	mColorIndex = ColorIndex;

	ReleaseThumbnails();
	mParts.clear();

	for (const auto& [PartData, Description] : Parts)
	{
		swPartSelectionListModelEntry& Entry = mParts.emplace_back();

		Entry.PartData = PartData;
		Entry.Description = Description;

		if (PartData && PartData->GetTrainTrackInfo())
			Entry.ColorIndex = mColorIndex;
	}

	endResetModel();

	SetFilter(mFilterString);
}

void swPartSelectionListModel::SetFilter(const QString& FilterString)
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

	for (size_t PartIdx = 0; PartIdx < mParts.size(); PartIdx++)
	{
		swPartData* PartData = mParts[PartIdx].PartData;
		bool Visible = true;

		if (!PartData)
			Visible = true;
		else if (!mShowDecoratedParts && PartData->IsDecorated() && !PartData->IsProjectPart())
			Visible = false;
		else if (!mShowPartAliases && PartData->IsAlias())
			Visible = false;
		else if (mPartFilterType == swPartFilterType::Default)
		{
			const char* Description = mParts[PartIdx].Description.empty() ? PartData->m_strDescription : mParts[PartIdx].Description.c_str();

			Visible = (mPartDescriptionFilter && Filter.Match(Description)) || (mFileNameFilter && Filter.Match(PartData->mFileName));
		}
		else if (mPartFilterType == swPartFilterType::RegularExpression)
		{
			QString Description = mParts[PartIdx].Description.empty() ? QString(PartData->m_strDescription) : QString::fromStdString(mParts[PartIdx].Description);

			Visible = (mPartDescriptionFilter && Description.contains(FilterRx)) || (mFileNameFilter && QString(PartData->mFileName).contains(FilterRx));
		}

		mListView->setRowHidden((int)PartIdx, !Visible);
	}
}

int swPartSelectionListModel::rowCount(const QModelIndex& Parent) const
{
	Q_UNUSED(Parent);

	return (int)mParts.size();
}

QVariant swPartSelectionListModel::data(const QModelIndex& Index, int Role) const
{
	size_t InfoIndex = Index.row();

	if (Index.isValid() && InfoIndex < mParts.size())
	{
		swPartData* PartData = mParts[InfoIndex].PartData;

		switch (Role)
		{
		case Qt::DisplayRole:
			if (!mIconSize || mShowPartNames || mListMode)
				return QVariant(mParts[InfoIndex].Description.empty() ? QString::fromLatin1(PartData->m_strDescription) : QString::fromStdString(mParts[InfoIndex].Description));
			break;

		case Qt::ToolTipRole:
			if (PartData)
			{
				const QString Error = swGetPartsLibrary()->GetPartDataLoadError(PartData);

				if (!Error.isEmpty())
					return QVariant(Error);

				if (PartData->mState == swAssetState::Failed)
					return QVariant(tr("Could not load part '%1'.").arg(QString::fromLatin1(PartData->mFileName)));

				return QVariant(QString("'%1' ('%2')").arg(QString::fromLatin1(PartData->m_strDescription), QString::fromLatin1(PartData->mFileName)));
			}
			break;

		case Qt::DecorationRole:
			if (mIconSize && !mParts[InfoIndex].Pixmap.isNull())
				return QVariant(mParts[InfoIndex].Pixmap);
			else
				return QVariant(QColor(0, 0, 0, 0));

		default:
			break;
		}
	}

	return QVariant();
}

QVariant swPartSelectionListModel::headerData(int Section, Qt::Orientation Orientation, int Role) const
{
	Q_UNUSED(Section);
	Q_UNUSED(Orientation);

	return Role == Qt::DisplayRole ? QVariant(QLatin1String("Image")) : QVariant();
}

Qt::ItemFlags swPartSelectionListModel::flags(const QModelIndex& Index) const
{
	Qt::ItemFlags DefaultFlags = QAbstractListModel::flags(Index);

	if (Index.isValid())
		return Qt::ItemIsDragEnabled | DefaultFlags;
	else
		return DefaultFlags;
}

QModelIndex swPartSelectionListModel::GetPartDataIndex(swPartData* PartData) const
{
	if (PartData)
	{
		for (int PartIndex = 0; PartIndex < static_cast<int>(mParts.size()); PartIndex++)
			if (mParts[PartIndex].PartData == PartData)
				return index(PartIndex, 0);
	}

	return QModelIndex();
}

void swPartSelectionListModel::ReleaseThumbnails()
{
	swThumbnailManager* ThumbnailManager = swGetPartsLibrary()->GetThumbnailManager();

	for (swPartSelectionListModelEntry& Part : mParts)
	{
		ThumbnailManager->ReleaseThumbnail(Part.ThumbnailId);
		Part.ThumbnailId = swPartThumbnailId::Invalid;
		Part.Pixmap = QPixmap();
	}
}

void swPartSelectionListModel::RequestThumbnail(int PartIndex)
{
	if (!mIconSize || !mParts[PartIndex].Pixmap.isNull() || mParts[PartIndex].ThumbnailId != swPartThumbnailId::Invalid)
		return;

	swPartData* PartData = mParts[PartIndex].PartData;

	if (!PartData)
		return;

	int ColorIndex = mParts[PartIndex].ColorIndex == -1 ? mColorIndex : mParts[PartIndex].ColorIndex;

	auto [ThumbnailId, Thumbnail] = swGetPartsLibrary()->GetThumbnailManager()->RequestThumbnail(PartData, ColorIndex, mIconSize, mDeviceScale);

	mParts[PartIndex].ThumbnailId = ThumbnailId;

	if (!Thumbnail.isNull())
	{
		mParts[PartIndex].Pixmap = Thumbnail;

		emit dataChanged(index(PartIndex, 0), index(PartIndex, 0), { Qt::DecorationRole });
	}
}

void swPartSelectionListModel::ThumbnailReady(swPartThumbnailId ThumbnailId, QPixmap Pixmap)
{
	for (int PartIndex = 0; PartIndex < static_cast<int>(mParts.size()); PartIndex++)
	{
		if (mParts[PartIndex].ThumbnailId == ThumbnailId)
		{
			mParts[PartIndex].Pixmap = Pixmap;

			emit dataChanged(index(PartIndex, 0), index(PartIndex, 0), { Qt::DecorationRole });
		}
	}
}

void swPartSelectionListModel::SetShowDecoratedParts(bool Show)
{
	if (Show == mShowDecoratedParts)
		return;

	mShowDecoratedParts = Show;

	SetFilter(mFilterString);
}

void swPartSelectionListModel::SetShowPartAliases(bool Show)
{
	if (Show == mShowPartAliases)
		return;

	mShowPartAliases = Show;

	SetFilter(mFilterString);
}

void swPartSelectionListModel::SetPartFilterType(swPartFilterType Option)
{
	if (Option == mPartFilterType)
		return;

	mPartFilterType = Option;

	SetFilter(mFilterString);
}

void swPartSelectionListModel::SetCaseSensitiveFilter(bool Option)
{
	if (Option == mCaseSensitiveFilter)
		return;

	mCaseSensitiveFilter = Option;

	SetFilter(mFilterString);
}

void swPartSelectionListModel::SetFileNameFilter(bool Option)
{
	if (Option == mFileNameFilter)
		return;

	mFileNameFilter = Option;

	SetFilter(mFilterString);
}

void swPartSelectionListModel::SetPartDescriptionFilter(bool Option)
{
	if (Option == mPartDescriptionFilter)
		return;

	mPartDescriptionFilter = Option;

	SetFilter(mFilterString);
}

void swPartSelectionListModel::SetIconSize(int Size, float DeviceScale)
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

void swPartSelectionListModel::SetShowPartNames(bool Show)
{
	if (Show == mShowPartNames)
		return;

	mShowPartNames = Show;

	beginResetModel();
	endResetModel();

	SetFilter(mFilterString);
}

swPartSelectionListView::swPartSelectionListView(QWidget* Parent, swPartSelectionWidget* PartSelectionWidget)
	: QListView(Parent), mPartSelectionWidget(PartSelectionWidget)
{
	setUniformItemSizes(true);
	setResizeMode(QListView::Adjust);
	setWordWrap(false);
	setDragEnabled(true);
	setContextMenuPolicy(Qt::CustomContextMenu);
	setAutoScroll(false);
	setTextElideMode(Qt::ElideMiddle);

	mListModel = new swPartSelectionListModel(this);
	setModel(mListModel);
	swPartSelectionItemDelegate* ItemDelegate = new swPartSelectionItemDelegate(this, mListModel);
	setItemDelegate(ItemDelegate);

	// the popup picks with a single click
	if (mPartSelectionWidget && mPartSelectionWidget->GetIsPopup())
		connect(this, &QListView::clicked, this, &swPartSelectionListView::DoubleClicked);
	else
		connect(this, &QListView::doubleClicked, this, &swPartSelectionListView::DoubleClicked);
	connect(this, &QListView::customContextMenuRequested, this, &swPartSelectionListView::CustomContextMenuRequested);
	connect(verticalScrollBar(), &QScrollBar::valueChanged, this, &swPartSelectionListView::AlignScrollToRows);

	connect(gApplication, &swApplication::RendererInitialized, this, &swPartSelectionListView::UpdateIconSize);

	mRequestedIconSize = swGetProfileInteger(mPartSelectionWidget->GetIsPopup() ? SW_PROFILE_PARTS_POPUP_ICONS : SW_PROFILE_PARTS_LIST_ICONS);
	UpdateIconSize();
}

void swPartSelectionListView::keyPressEvent(QKeyEvent* KeyEvent)
{
	if (KeyEvent->key() == Qt::Key_Enter)
	{
		swPartData* PartData = GetCurrentPart();

		emit PartPicked(PartData);

		return;
	}

	return QListView::keyPressEvent(KeyEvent);
}

void swPartSelectionListView::DoubleClicked(const QModelIndex& Index)
{
	swPartData* PartData = GetListModel()->GetPartData(Index.row());

	emit PartPicked(PartData);
}

void swPartSelectionListView::CustomContextMenuRequested(QPoint Pos)
{
	if (!mPartSelectionWidget || mPartSelectionWidget->GetIsPopup())
		return;

	QMenu* Menu = new QMenu(this);

	QModelIndex Index = indexAt(Pos);
	mContextPartData = Index.isValid() ? mListModel->GetPartData(Index.row()) : nullptr;

	QMenu* SetMenu = Menu->addMenu(tr("Add to Palette"));

	const std::vector<swPartPalette>& Palettes = mPartSelectionWidget->GetPartPalettes();

	if (!Palettes.empty())
	{
		for (const swPartPalette& Palette : Palettes)
			SetMenu->addAction(Palette.Name, mPartSelectionWidget, &swPartSelectionWidget::AddToPalette);
	}
	else
	{
		QAction* Action = SetMenu->addAction(tr("None"));
		Action->setEnabled(false);
	}

	QAction* RemoveAction = Menu->addAction(tr("Remove from Palette"), mPartSelectionWidget, &swPartSelectionWidget::RemoveFromPalette);
	RemoveAction->setEnabled(mCategoryType == swPartCategoryType::Palette);

	if (mContextPartData && mContextPartData->IsModel())
	{
		Menu->addSeparator();
		Menu->addAction(tr("Open Submodel"), mPartSelectionWidget, &swPartSelectionWidget::OpenSubmodel);
	}

	Menu->exec(viewport()->mapToGlobal(Pos));
	delete Menu;
}

void swPartSelectionListView::SetCategory(swPartCategoryType Type, int Index)
{
	QModelIndex CurrentIndex = currentIndex();

	if (CurrentIndex.isValid())
		mLastCategoryRow[{mCategoryType, mCategoryIndex}] = CurrentIndex.row();

	mCategoryType = Type;
	mCategoryIndex = Index;

	switch (Type)
	{
	case swPartCategoryType::AllParts:
		mListModel->SetCategory(-1);
		break;
	case swPartCategoryType::PartsInUse:
		mListModel->SetCurrentModelCategory();
		break;
	case swPartCategoryType::Submodels:
		mListModel->SetModelsCategory();
		break;
	case swPartCategoryType::Palette:
		mListModel->SetPaletteCategory(Index);
		break;
	case swPartCategoryType::Category:
		mListModel->SetCategory(Index);
		break;
	case swPartCategoryType::Custom:
	case swPartCategoryType::Count:
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

void swPartSelectionListView::SetCustomParts(const std::vector<std::pair<swPartData*, std::string>>& Parts, int ColorIndex)
{
	mCategoryType = swPartCategoryType::Custom;

	mListModel->SetCustomParts(Parts, ColorIndex);

	setCurrentIndex(mListModel->index(0, 0));
}

void swPartSelectionListView::SetCurrentPart(swPartData* PartData)
{
	QModelIndex Index = mListModel->GetPartDataIndex(PartData);

	if (Index.isValid())
	{
		setCurrentIndex(Index);
		scrollTo(Index, QAbstractItemView::EnsureVisible);
	}
}

void swPartSelectionListView::SetNoIcons()
{
	SetIconSize(NoIconSize);
}

void swPartSelectionListView::SetSmallIcons()
{
	SetIconSize(SmallIconSize);
}

void swPartSelectionListView::SetMediumIcons()
{
	SetIconSize(MediumIconSize);
}

void swPartSelectionListView::SetLargeIcons()
{
	SetIconSize(LargeIconSize);
}

void swPartSelectionListView::SetExtraLargeIcons()
{
	SetIconSize(ExtraLargeIconSize);
}

void swPartSelectionListView::TogglePartNames()
{
	bool Show = !mListModel->GetShowPartNames();
	mListModel->SetShowPartNames(Show);
	swSetProfileInteger(mPartSelectionWidget->GetIsPopup() ? SW_PROFILE_PARTS_POPUP_NAMES : SW_PROFILE_PARTS_LIST_NAMES, Show);
	UpdateItemGeometry();
}

void swPartSelectionListView::ToggleDecoratedParts()
{
	bool Show = !mListModel->GetShowDecoratedParts();
	mListModel->SetShowDecoratedParts(Show);
	swSetProfileInteger(mPartSelectionWidget->GetIsPopup() ? SW_PROFILE_PARTS_POPUP_DECORATED : SW_PROFILE_PARTS_LIST_DECORATED, Show);
}

void swPartSelectionListView::TogglePartAliases()
{
	bool Show = !mListModel->GetShowPartAliases();
	mListModel->SetShowPartAliases(Show);
	swSetProfileInteger(mPartSelectionWidget->GetIsPopup() ? SW_PROFILE_PARTS_POPUP_ALIASES : SW_PROFILE_PARTS_LIST_ALIASES, Show);
}

void swPartSelectionListView::ToggleRegularExpressionFilter()
{
	bool Option = mListModel->GetPartFilterType() == swPartFilterType::RegularExpression;
	SetPartFilterType(Option ? swPartFilterType::Default : swPartFilterType::RegularExpression);
}

void swPartSelectionListView::ToggleCaseSensitiveFilter()
{
	bool Option = !mListModel->GetCaseSensitiveFilter();
	mListModel->SetCaseSensitiveFilter(Option);
	swSetProfileInteger(SW_PROFILE_PARTS_LIST_CASE_SENSITIVE_FILTER, Option);
}

void swPartSelectionListView::ToggleFileNameFilter()
{
	bool Option = !mListModel->GetFileNameFilter();
	mListModel->SetFileNameFilter(Option);
	swSetProfileInteger(SW_PROFILE_PARTS_LIST_FILE_NAME_FILTER, Option);
}

void swPartSelectionListView::TogglePartDescriptionFilter()
{
	bool Option = !mListModel->GetPartDescriptionFilter();
	mListModel->SetPartDescriptionFilter(Option);
	swSetProfileInteger(SW_PROFILE_PARTS_LIST_PART_DESCRIPTION_FILTER, Option);
}

void swPartSelectionListView::SetPartFilterType(swPartFilterType Option)
{
	mListModel->SetPartFilterType(Option);
	swSetProfilePartFilterType(Option);
}

void swPartSelectionListView::ToggleListMode()
{
	mListModel->ToggleListMode();
	UpdateItemGeometry();
	SnapPanel();
}

// use to fit the parts panel to the new tiles or rows
void swPartSelectionListView::SnapPanel()
{
	if (!mPartSelectionWidget->GetIsPopup())
		QTimer::singleShot(0, gMainWindow, &swMainWindow::SnapPartsDock);
}

void swPartSelectionListView::ToggleFixedColor()
{
	mListModel->ToggleColorLocked();
}

void swPartSelectionListView::UpdateInUseCategory()
{
	if (mCategoryType == swPartCategoryType::PartsInUse)
		mListModel->SetCurrentModelCategory();
}

void swPartSelectionListView::UpdateViewMode()
{
	setViewMode(mListModel->GetIconSize() && !mListModel->IsListMode() ? QListView::IconMode : QListView::ListMode);
	setWordWrap(mListModel->IsListMode());
	setDragEnabled(true);
}

void swPartSelectionListView::SetIconSize(int Size)
{
	mRequestedIconSize = Size;
	swSetProfileInteger(mPartSelectionWidget->GetIsPopup() ? SW_PROFILE_PARTS_POPUP_ICONS : SW_PROFILE_PARTS_LIST_ICONS, Size);
	UpdateIconSize();
	SnapPanel();
}

void swPartSelectionListView::UpdateIconSize()
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
	setMinimumSize(mPartSelectionWidget->GetIsPopup() ? QSize(0, 0) : QSize(Width, Height));
	UpdateItemGeometry();
}

void swPartSelectionListView::UpdateItemGeometry()
{
	updateGeometry();

	if (mPartSelectionWidget->GetIsPopup())
	{
		// Invalidate the layout item caches through the picker splitter.
		for (QWidget* Widget = parentWidget(); Widget; Widget = Widget->parentWidget())
		{
			Widget->updateGeometry();
			if (Widget == mPartSelectionWidget)
				break;
		}
	}

	emit LayoutChanged();
}

void swPartSelectionListView::startDrag(Qt::DropActions SupportedActions)
{
	Q_UNUSED(SupportedActions);

	swPartData* PartData = GetCurrentPart();

	if (!PartData)
		return;

	QByteArray ItemData;
	QDataStream DataStream(&ItemData, QIODevice::WriteOnly);
	DataStream << QString(PartData->mFileName);

	QMimeData* MimeData = new QMimeData;
	MimeData->setData("application/vnd.studworks-part", ItemData);

	QDrag* Drag = new QDrag(this);
	Drag->setMimeData(MimeData);

	Drag->exec(Qt::CopyAction);
}

QSize swPartSelectionListView::GetCellSize() const
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	QStyleOptionViewItem Option;
	initViewItemOption(&Option);
#else
	QStyleOptionViewItem Option = viewOptions();
#endif

	return itemDelegate()->sizeHint(Option, model()->index(0, 0));
}

QSize swPartSelectionListView::sizeHint() const
{
	return GetPreferredSize(QWIDGETSIZE_MAX);
}

int swPartSelectionListView::GetTileSnapDelta() const
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

// use to measure the distance from one thumbnail or list row to the next
int swPartSelectionListView::GetRowPitch() const
{
	if (model()->rowCount() < 1)
		return 0;

	// content rects ignore the scrolling
	const QRect FirstRect = rectForIndex(model()->index(0, 0));

	for (int Row = 1; Row < model()->rowCount(); Row++)
	{
		const QRect Rect = rectForIndex(model()->index(Row, 0));

		if (Rect.top() != FirstRect.top())
			return Rect.top() - FirstRect.top();
	}

	return FirstRect.height() + spacing();
}

// use to measure how far the view height is from showing whole thumbnail or list rows
int swPartSelectionListView::GetRowSnapDelta() const
{
	const int Pitch = GetRowPitch();

	if (Pitch <= 0)
		return 0;

	// the first row gives the top margin
	const QRect FirstRect = rectForIndex(model()->index(0, 0));

	// the space after the last whole row beyond a bottom margin matching the top one
	const int Rows = qMax(1, (viewport()->height() - 2 * FirstRect.top()) / Pitch);
	const int Leftover = viewport()->height() - 2 * FirstRect.top() - Rows * Pitch;

	// trim a small leftover or grow to fit one more row when most of one already fits
	return Leftover * 2 < Pitch ? -Leftover : Pitch - Leftover;
}

// use to scroll by whole rows so a snapped height never cuts the last row
void swPartSelectionListView::updateGeometries()
{
	QListView::updateGeometries();

	const int Pitch = GetRowPitch();

	if (Pitch > 0 && !mPartSelectionWidget->GetIsPopup() && swGetPreferences().mPartsSnapToTiles)
	{
		verticalScrollBar()->setSingleStep(Pitch);
		verticalScrollBar()->setPageStep(qMax(Pitch, viewport()->height() / Pitch * Pitch));
	}
}

// use to keep the scrolling on a row edge, rounding the way it moved so the row asked for stays in view
void swPartSelectionListView::AlignScrollToRows(int Value)
{
	const int Pitch = GetRowPitch();
	const int LastValue = mLastScrollValue;
	mLastScrollValue = Value;

	if (Pitch <= 0 || Value % Pitch == 0 || Value == verticalScrollBar()->maximum() || mPartSelectionWidget->GetIsPopup() || !swGetPreferences().mPartsSnapToTiles)
		return;

	const int Aligned = Value > LastValue ? (Value / Pitch + 1) * Pitch : Value / Pitch * Pitch;
	verticalScrollBar()->setValue(qMin(Aligned, verticalScrollBar()->maximum()));
}

QSize swPartSelectionListView::GetPreferredSize(int MaximumWidth) const
{
	if (model()->rowCount() == 0 && (mListModel->GetIconSize() == NoIconSize || mListModel->IsListMode()))
		return QListView::sizeHint();

	if (mListModel->GetIconSize() == NoIconSize)
		return QSize(500, 350);

	int Columns = mListModel->IsListMode() ? 1 : 4;
	int MaximumRows = 4;

	QSize CellSize = GetCellSize();
	if (mPartSelectionWidget->GetIsPopup() && !mListModel->IsListMode())
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
	if (mPartSelectionWidget->GetIsPopup() && !mListModel->IsListMode() && CellSize.width() > 0)
		Columns = qMin(Columns, qMax(1, (Width - WidthOverhead) / CellSize.width()));

	int Rows = model()->rowCount() ? qMin(MaximumRows, (model()->rowCount() + Columns - 1) / Columns) : MaximumRows;
	QSize Size(Width, CellSize.height() * Rows + frameWidth() * 2);

	return Size;
}

swPartSelectionWidget::swPartSelectionWidget(bool IsPopup, QWidget* Parent)
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
	connect(mFilterCaseAction, &QAction::triggered, this, &swPartSelectionWidget::FilterCaseTriggered);

	swAddPanelActionButton(FilterCategoriesRow, mFilterCaseAction);

	mCategoriesWidget = new QTreeWidget(mSplitter);
	mCategoriesWidget->setHeaderHidden(true);
	mCategoriesWidget->setAccessibleName(tr("Part Categories"));
	mCategoriesWidget->setToolTip(tr("Choose a category or palette to display its parts."));
	mCategoriesWidget->setUniformRowHeights(true);
	mCategoriesWidget->setRootIsDecorated(false);

	CategoriesLayout->addWidget(mCategoriesWidget);

	QWidget* PartsGroupWidget = new QWidget(mSplitter);

	QVBoxLayout* PartsLayout = new QVBoxLayout();
	PartsLayout->setContentsMargins(0, 0, 0, 0);
	PartsGroupWidget->setLayout(PartsLayout);

	QWidget* SearchRow = swCreatePanelRow(PartsGroupWidget);
	PartsLayout->addWidget(SearchRow);

	mFilterWidget = new QLineEdit(PartsGroupWidget);
	mFilterWidget->setPlaceholderText(tr("Filter Parts"));
	mFilterWidget->setAccessibleName(tr("Filter Parts"));
	mFilterWidget->setToolTip(tr("Filter the displayed parts using the matching options."));
	swSetupPanelEdit(mFilterWidget);
	swAddPanelRowWidget(SearchRow, mFilterWidget);

	QMenu* OptionsMenu = new QMenu(this);
	swAddPanelOptionsButton(SearchRow, OptionsMenu, tr("Configure part filtering and list display."), tr("Part List Options"));
	connect(OptionsMenu, &QMenu::aboutToShow, this, &swPartSelectionWidget::OptionsMenuAboutToShow);

	mPartsWidget = new swPartSelectionListView(PartsGroupWidget, this);
	mPartsWidget->setAccessibleName(tr("Available Parts"));
	mPartsWidget->setToolTip(tr("Select a part from the filtered list."));
	if (mIsPopup)
		mPartsWidget->setMinimumSize(0, 0);
	PartsLayout->addWidget(mPartsWidget);

	QHBoxLayout* Layout = new QHBoxLayout(this);
	Layout->addWidget(mSplitter);
	setLayout(Layout);

	connect(mPartsWidget, &swPartSelectionListView::PartPicked, this, &swPartSelectionWidget::PartViewPartPicked);
	connect(mPartsWidget, &swPartSelectionListView::LayoutChanged, this, &swPartSelectionWidget::LayoutChanged);
	connect(mPartsWidget->selectionModel(), &QItemSelectionModel::currentChanged, this, &swPartSelectionWidget::PartViewSelectionChanged);
	connect(mFilterWidget, &QLineEdit::textChanged, this, &swPartSelectionWidget::FilterChanged);
	connect(mCategoriesWidget, &QTreeWidget::currentItemChanged, this, &swPartSelectionWidget::CategoryChanged);
	connect(mFilterCategoriesWidget, &QLineEdit::textChanged, this, &swPartSelectionWidget::FilterCategoriesChanged);

	LoadPartPalettes();
	UpdateCategories();

	mSplitter->setStretchFactor(0, 0);
	mSplitter->setStretchFactor(1, 1);

	// Dragging the splitter by hand snaps the categories back to the longest name once the mouse is released.
	mSplitter->handle(1)->installEventFilter(this);
}

bool swPartSelectionWidget::eventFilter(QObject* Object, QEvent* Event)
{
	if (Object == mSplitter->handle(1) && Event->type() == QEvent::MouseButtonRelease)
		QTimer::singleShot(0, this, &swPartSelectionWidget::SnapCategories);

	return QWidget::eventFilter(Object, Event);
}

bool swPartSelectionWidget::event(QEvent* Event)
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

void swPartSelectionWidget::LoadState(QSettings& Settings)
{
	QList<int> Sizes = Settings.value("PartSelectionSplitter").value<QList<int>>();

	if (Sizes.size() != 2)
	{
		int Length = mSplitter->orientation() == Qt::Horizontal ? mSplitter->width() : mSplitter->height();
		Sizes = { Length / 3, 2 * Length / 3 };
	}

	mSplitter->setSizes(Sizes);

	QTimer::singleShot(0, this, &swPartSelectionWidget::SnapCategories);
}

void swPartSelectionWidget::SaveState(QSettings& Settings)
{
	QList<int> Sizes = mSplitter->sizes();
	Settings.setValue("PartSelectionSplitter", QVariant::fromValue(Sizes));
}

void swPartSelectionWidget::SetCurrentPart(swPartData* PartData)
{
	mPartsWidget->SetCurrentPart(PartData);
	mPartsWidget->setFocus();
}

void swPartSelectionWidget::SetCategory(swPartCategoryType Type, int Index)
{
	for (int Row = 0; Row < mCategoriesWidget->topLevelItemCount(); Row++)
	{
		QTreeWidgetItem* Item = mCategoriesWidget->topLevelItem(Row);
		swPartCategoryType ItemType = static_cast<swPartCategoryType>(Item->data(0, static_cast<int>(swPartCategoryRole::Type)).toInt());

		if (ItemType != Type)
			continue;

		if (Type == swPartCategoryType::Palette || Type == swPartCategoryType::Category)
		{
			int ItemIndex = Item->data(0, static_cast<int>(swPartCategoryRole::Index)).toInt();

			if (ItemIndex != Index)
				continue;
		}

		mCategoriesWidget->setCurrentItem(Item);
		break;
	}
}

void swPartSelectionWidget::SetCustomParts(const std::vector<std::pair<swPartData*, std::string>>& Parts, int ColorIndex)
{
	mSplitter->widget(0)->setVisible(false);

	mPartsWidget->SetCustomParts(Parts, ColorIndex);
}

void swPartSelectionWidget::SetOrientation(Qt::Orientation Orientation)
{
	mSplitter->setOrientation(Orientation);

	int Length = mSplitter->orientation() == Qt::Horizontal ? mSplitter->width() : mSplitter->height();
	QList<int> Sizes = { Length / 3, 2 * Length / 3 };

	mSplitter->setSizes(Sizes);
}

void swPartSelectionWidget::SnapCategories()
{
	SnapCategoriesWidth();
	SnapCategoriesHeight();
}

void swPartSelectionWidget::SnapCategoriesWidth()
{
	// docked at the top or bottom or floating wide, the categories sit beside the parts
	if (mIsPopup || !swGetPreferences().mPartsSnapToTiles || mSplitter->orientation() != Qt::Horizontal || mSplitter->widget(0)->isHidden())
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

void swPartSelectionWidget::SnapCategoriesHeight()
{
	if (mIsPopup || mSplitter->orientation() != Qt::Vertical || mSplitter->widget(0)->isHidden())
		return;

	// Measure the first shown row, the filter can hide the others.
	int RowHeight = 0;

	for (int Row = 0; Row < mCategoriesWidget->topLevelItemCount() && !RowHeight; Row++)
		if (!mCategoriesWidget->topLevelItem(Row)->isHidden())
			RowHeight = mCategoriesWidget->visualItemRect(mCategoriesWidget->topLevelItem(Row)).height();

	QList<int> Sizes = mSplitter->sizes();

	if (RowHeight <= 0 || Sizes.size() != 2)
		return;

	// The categories end on a whole row instead of cutting the last one.
	const int ViewportHeight = mCategoriesWidget->viewport()->height();
	const int Rows = qMax(1, (ViewportHeight + RowHeight / 2) / RowHeight);
	int Height = Sizes[0] + Rows * RowHeight - ViewportHeight;

	const int Length = Sizes[0] + Sizes[1];

	if (Height > Length - mSplitter->widget(1)->minimumSizeHint().height())
		Height -= RowHeight;

	if (Height < mSplitter->widget(0)->minimumSizeHint().height())
		Height += RowHeight;

	if (Height != Sizes[0])
		mSplitter->setSizes({ Height, Length - Height });
}

void swPartSelectionWidget::DockLocationChanged(Qt::DockWidgetArea Area)
{
	mDockArea = Area;

	if (Area == Qt::LeftDockWidgetArea || Area == Qt::RightDockWidgetArea)
		mSplitter->setOrientation(Qt::Vertical);
	else
		mSplitter->setOrientation(Qt::Horizontal);

	QTimer::singleShot(0, this, &swPartSelectionWidget::SnapCategories);
}

void swPartSelectionWidget::resizeEvent(QResizeEvent* Event)
{
	QDockWidget* DockWidget = qobject_cast<QDockWidget*>(parent());

	if (DockWidget && DockWidget->isFloating())
	{
		if (Event->size().width() > Event->size().height())
			mSplitter->setOrientation(Qt::Horizontal);
		else
			mSplitter->setOrientation(Qt::Vertical);
	}

	// a floating panel can turn the categories beside or above the parts on any resize
	QTimer::singleShot(0, this, &swPartSelectionWidget::SnapCategories);

	QWidget::resizeEvent(Event);
}

void swPartSelectionWidget::FilterCategoriesChanged(const QString& Text)
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
			connect(mFilterCategoriesAction, &QAction::triggered, this, &swPartSelectionWidget::FilterCategoriesTriggered);
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

void swPartSelectionWidget::FilterChanged(const QString& Text)
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
			connect(mFilterAction, &QAction::triggered, this, &swPartSelectionWidget::FilterTriggered);
		}
	}

	mPartsWidget->GetListModel()->SetFilter(Text);
}

void swPartSelectionWidget::FilterCategoriesTriggered()
{
	mFilterCategoriesWidget->clear();
}

void swPartSelectionWidget::FilterCaseTriggered()
{
	if (!mFilterCategoriesWidget->text().isEmpty())
		FilterCategoriesChanged(mFilterCategoriesWidget->text());
}

void swPartSelectionWidget::FilterTriggered()
{
	mFilterWidget->clear();
}

void swPartSelectionWidget::CategoryChanged(QTreeWidgetItem* Current, QTreeWidgetItem* Previous)
{
	Q_UNUSED(Previous);

	if (!Current)
		return;

	int Type = Current->data(0, static_cast<int>(swPartCategoryRole::Type)).toInt();
	int Index = Current->data(0, static_cast<int>(swPartCategoryRole::Index)).toInt();

	mPartsWidget->SetCategory(static_cast<swPartCategoryType>(Type), Index);
}

void swPartSelectionWidget::PartViewSelectionChanged(const QModelIndex& Current, const QModelIndex& Previous)
{
	Q_UNUSED(Current);
	Q_UNUSED(Previous);

	emit CurrentPartChanged(mPartsWidget->GetCurrentPart());
}

void swPartSelectionWidget::PartViewPartPicked(swPartData* PartData)
{
	emit PartPicked(PartData);
}

void swPartSelectionWidget::OptionsMenuAboutToShow()
{
	QMenu* Menu = (QMenu*)sender();
	Menu->clear();

	if (!mIsPopup)
	{
		Menu->addAction(tr("Edit Palettes..."), this, &swPartSelectionWidget::EditPartPalettes);

		Menu->addSeparator();
	}

	swPartSelectionListModel* ListModel = mPartsWidget->GetListModel();

	if (ListModel->GetIconSize() != swPartSelectionListView::NoIconSize && !ListModel->IsListMode())
	{
		QAction* PartNames = Menu->addAction(tr("Show Part Names"), mPartsWidget, &swPartSelectionListView::TogglePartNames);
		PartNames->setCheckable(true);
		PartNames->setChecked(ListModel->GetShowPartNames());
	}

	QAction* DecoratedParts = Menu->addAction(tr("Show Decorated Parts"), mPartsWidget, &swPartSelectionListView::ToggleDecoratedParts);
	DecoratedParts->setCheckable(true);
	DecoratedParts->setChecked(ListModel->GetShowDecoratedParts());

	QAction* PartAliases = Menu->addAction(tr("Show Part Aliases"), mPartsWidget, &swPartSelectionListView::TogglePartAliases);
	PartAliases->setCheckable(true);
	PartAliases->setChecked(ListModel->GetShowPartAliases());

	if (!mIsPopup)
		gMainWindow->AddMenuCommand(Menu, tr("Snap Panel Width to Tiles"), SW_VIEW_PARTS_TOGGLE_SNAP_TO_TILES, swGetPreferences().mPartsSnapToTiles);

	Menu->addSeparator();

	if (gSupportsFramebufferObject)
	{
		QMenu* IconMenu = Menu->addMenu(tr("Icons"));

		QActionGroup* IconGroup = new QActionGroup(Menu);

		QAction* NoIcons = IconMenu->addAction(tr("No Icons"), mPartsWidget, &swPartSelectionListView::SetNoIcons);
		NoIcons->setCheckable(true);
		NoIcons->setChecked(ListModel->GetIconSize() == swPartSelectionListView::NoIconSize);
		IconGroup->addAction(NoIcons);

		QAction* SmallIcons = IconMenu->addAction(tr("Small Icons"), mPartsWidget, &swPartSelectionListView::SetSmallIcons);
		SmallIcons->setCheckable(true);
		SmallIcons->setChecked(ListModel->GetIconSize() == swPartSelectionListView::SmallIconSize);
		IconGroup->addAction(SmallIcons);

		QAction* MediumIcons = IconMenu->addAction(tr("Medium Icons"), mPartsWidget, &swPartSelectionListView::SetMediumIcons);
		MediumIcons->setCheckable(true);
		MediumIcons->setChecked(ListModel->GetIconSize() == swPartSelectionListView::MediumIconSize);
		IconGroup->addAction(MediumIcons);

		QAction* LargeIcons = IconMenu->addAction(tr("Large Icons"), mPartsWidget, &swPartSelectionListView::SetLargeIcons);
		LargeIcons->setCheckable(true);
		LargeIcons->setChecked(ListModel->GetIconSize() == swPartSelectionListView::LargeIconSize);
		IconGroup->addAction(LargeIcons);

		QAction* ExtraLargeIcons = IconMenu->addAction(tr("Extra Large Icons"), mPartsWidget, &swPartSelectionListView::SetExtraLargeIcons);
		ExtraLargeIcons->setCheckable(true);
		ExtraLargeIcons->setChecked(ListModel->GetIconSize() == swPartSelectionListView::ExtraLargeIconSize);
		IconGroup->addAction(ExtraLargeIcons);

		if (ListModel->GetIconSize() != swPartSelectionListView::NoIconSize)
		{
			IconMenu->addSeparator();

			QAction* ListMode = IconMenu->addAction(tr("List Mode"), mPartsWidget, &swPartSelectionListView::ToggleListMode);
			ListMode->setCheckable(true);
			ListMode->setChecked(ListModel->IsListMode());

			if (!mIsPopup)
			{
				QAction* FixedColor = IconMenu->addAction(tr("Lock Color"), mPartsWidget, &swPartSelectionListView::ToggleFixedColor);
				FixedColor->setCheckable(true);
				FixedColor->setChecked(ListModel->IsColorLocked());
			}
		}
	}

	QMenu* FilterMenu = Menu->addMenu(tr("Filter"));

	QAction* RegularExpressionFilter = FilterMenu->addAction(tr("Regular Expression"), mPartsWidget, &swPartSelectionListView::ToggleRegularExpressionFilter);
	RegularExpressionFilter->setCheckable(true);
	RegularExpressionFilter->setChecked(ListModel->GetPartFilterType() == swPartFilterType::RegularExpression);

	QAction* CaseSensitiveFilter = FilterMenu->addAction(tr("Match Case"), mPartsWidget, &swPartSelectionListView::ToggleCaseSensitiveFilter);
	CaseSensitiveFilter->setCheckable(true);
	CaseSensitiveFilter->setChecked(ListModel->GetCaseSensitiveFilter());

	FilterMenu->addSeparator();

	QAction* FileNameFilter = FilterMenu->addAction(tr("Part ID"), mPartsWidget, &swPartSelectionListView::ToggleFileNameFilter);
	FileNameFilter->setCheckable(true);
	FileNameFilter->setChecked(ListModel->GetFileNameFilter());
	FileNameFilter->setEnabled(ListModel->GetPartDescriptionFilter());

	QAction* PartDescriptionFilter = FilterMenu->addAction(tr("Part Description"), mPartsWidget, &swPartSelectionListView::TogglePartDescriptionFilter);
	PartDescriptionFilter->setCheckable(true);
	PartDescriptionFilter->setChecked(ListModel->GetPartDescriptionFilter());
	PartDescriptionFilter->setEnabled(ListModel->GetFileNameFilter());

	if (gSupportsFramebufferObject && gMainWindow)
	{
		QMenu* RenderingMenu = Menu->addMenu(tr("Rendering"));
		gMainWindow->AddRenderTargetActions(RenderingMenu, swRenderTarget::Parts);
	}
}

void swPartSelectionWidget::EditPartPalettes()
{
	swPartPaletteDialog Dialog(this, mPartPalettes);

	if (Dialog.exec() != QDialog::Accepted)
		return;

	SavePartPalettes();
	UpdateCategories();
}

void swPartSelectionWidget::UpdateThumbnails()
{
	mPartsWidget->GetListModel()->UpdateThumbnails();
}

void swPartSelectionWidget::SetDefaultPart()
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

void swPartSelectionWidget::LoadPartPalettes()
{
	QByteArray Buffer = swGetProfileBuffer(SW_PROFILE_PART_PALETTES);
	QJsonDocument Document = QJsonDocument::fromJson(Buffer);

	if (Document.isNull())
		Document = QJsonDocument::fromJson((QString("{ \"Version\":1, \"Palettes\": { \"%1\": [] } }").arg(tr("Favorites"))).toUtf8());

	QJsonObject RootObject = Document.object();
	mPartPalettes.clear();

	int Version = RootObject["Version"].toInt(0);
	if (Version != 1)
		return;

	QJsonObject PalettesObject = RootObject["Palettes"].toObject();

	for (QJsonObject::const_iterator ElementIt = PalettesObject.constBegin(); ElementIt != PalettesObject.constEnd(); ElementIt++)
	{
		if (!ElementIt.value().isArray())
			continue;

		swPartPalette Palette;
		Palette.Name = ElementIt.key();

		const QJsonArray Parts = ElementIt.value().toArray();

		for (const QJsonValue& Part : Parts)
			Palette.Parts.emplace_back(Part.toString().toStdString());

		mPartPalettes.emplace_back(std::move(Palette));
	}
}

void swPartSelectionWidget::SavePartPalettes()
{
	QJsonObject RootObject;

	RootObject["Version"] = 1;
	QJsonObject PalettesObject;

	for (const swPartPalette& Palette : mPartPalettes)
	{
		QJsonArray Parts;

		for (const std::string& PartId : Palette.Parts)
			Parts.append(QString::fromStdString(PartId));

		PalettesObject[Palette.Name] = Parts;
	}

	RootObject["Palettes"] = PalettesObject;

	QByteArray Buffer = QJsonDocument(RootObject).toJson();
	swSetProfileBuffer(SW_PROFILE_PART_PALETTES, Buffer);
}

void swPartSelectionWidget::AddToPalette()
{
	swPartData* PartData = mPartsWidget->GetContextInfo();
	if (!PartData)
		return;

	QString SetName = ((QAction*)sender())->text();

	std::vector<swPartPalette>::iterator SetIt = std::find_if(mPartPalettes.begin(), mPartPalettes.end(), [&SetName](const swPartPalette& Set)
	{
		return Set.Name == SetName;
	});

	if (SetIt == mPartPalettes.end())
		return;

	std::string PartId = swGetPartsLibrary()->GetPartId(PartData);
	std::vector<std::string>& Parts = SetIt->Parts;

	if (std::find(Parts.begin(), Parts.end(), PartId) == Parts.end())
	{
		Parts.emplace_back(PartId);
		SavePartPalettes();
	}
}

void swPartSelectionWidget::RemoveFromPalette()
{
	swPartData* PartData = mPartsWidget->GetContextInfo();
	if (!PartData)
		return;

	QTreeWidgetItem* CurrentItem = mCategoriesWidget->currentItem();
	if (!CurrentItem || CurrentItem->data(0, static_cast<int>(swPartCategoryRole::Type)) != static_cast<int>(swPartCategoryType::Palette))
		return;

	int SetIndex = CurrentItem->data(0, static_cast<int>(swPartCategoryRole::Index)).toInt();
	swPartPalette& Palette = mPartPalettes[SetIndex];

	std::string PartId = swGetPartsLibrary()->GetPartId(PartData);
	std::vector<std::string>::iterator PartIt = std::find(Palette.Parts.begin(), Palette.Parts.end(), PartId);

	if (PartIt != Palette.Parts.end())
	{
		Palette.Parts.erase(PartIt);
		mPartsWidget->SetCategory(swPartCategoryType::Palette, SetIndex);
		SavePartPalettes();
	}
}

void swPartSelectionWidget::OpenSubmodel()
{
	swPartData* PartData = mPartsWidget->GetContextInfo();

	if (!PartData || !PartData->IsModel())
		return;

	swModel* Model = PartData->GetModel();

	swGetActiveProject()->SetActiveModel(Model, true);
}

void swPartSelectionWidget::UpdateInUseCategory()
{
	mPartsWidget->UpdateInUseCategory();
}

void swPartSelectionWidget::UpdateCategories()
{
	QTreeWidgetItem* CurrentItem = mCategoriesWidget->currentItem();
	swPartCategoryType CurrentType = swPartCategoryType::Count;
	int CurrentIndex = -1;

	if (CurrentItem)
	{
		CurrentType = static_cast<swPartCategoryType>(CurrentItem->data(0, static_cast<int>(swPartCategoryRole::Type)).toInt());
		CurrentIndex = CurrentItem->data(0, static_cast<int>(swPartCategoryRole::Index)).toInt();
		CurrentItem = nullptr;
	}

	mCategoriesWidget->clear();

	mAllPartsCategoryItem = new QTreeWidgetItem(mCategoriesWidget, QStringList(tr("All Parts")));
	mAllPartsCategoryItem->setData(0, static_cast<int>(swPartCategoryRole::Type), static_cast<int>(swPartCategoryType::AllParts));

	if (CurrentType == swPartCategoryType::AllParts && CurrentIndex == 0)
		CurrentItem = mAllPartsCategoryItem;

	QTreeWidgetItem* CurrentModelCategoryItem = new QTreeWidgetItem(mCategoriesWidget, QStringList(tr("In Use")));
	CurrentModelCategoryItem->setData(0, static_cast<int>(swPartCategoryRole::Type), static_cast<int>(swPartCategoryType::PartsInUse));

	if (CurrentType == swPartCategoryType::PartsInUse && CurrentIndex == 0)
		CurrentItem = CurrentModelCategoryItem;

	QTreeWidgetItem* SubmodelsCategoryItem = new QTreeWidgetItem(mCategoriesWidget, QStringList(tr("Submodels")));
	SubmodelsCategoryItem->setData(0, static_cast<int>(swPartCategoryRole::Type), static_cast<int>(swPartCategoryType::Submodels));

	if (CurrentType == swPartCategoryType::Submodels && CurrentIndex == 0)
		CurrentItem = SubmodelsCategoryItem;

	for (int PaletteIdx = 0; PaletteIdx < static_cast<int>(mPartPalettes.size()); PaletteIdx++)
	{
		const swPartPalette& Set = mPartPalettes[PaletteIdx];
		QTreeWidgetItem* PaletteCategoryItem = new QTreeWidgetItem(mCategoriesWidget, QStringList(Set.Name));
		PaletteCategoryItem->setData(0, static_cast<int>(swPartCategoryRole::Type), static_cast<int>(swPartCategoryType::Palette));
		PaletteCategoryItem->setData(0, static_cast<int>(swPartCategoryRole::Index), PaletteIdx);

		if (CurrentType == swPartCategoryType::Palette && CurrentIndex == PaletteIdx)
			CurrentItem = PaletteCategoryItem;
	}

	for (int CategoryIdx = 0; CategoryIdx < static_cast<int>(gCategories.size()); CategoryIdx++)
	{
		if (!gCategories[CategoryIdx].Enabled)
			continue;

		QTreeWidgetItem* CategoryItem = new QTreeWidgetItem(mCategoriesWidget, QStringList(gCategories[CategoryIdx].Name));
		CategoryItem->setData(0, static_cast<int>(swPartCategoryRole::Type), static_cast<int>(swPartCategoryType::Category));
		CategoryItem->setData(0, static_cast<int>(swPartCategoryRole::Index), CategoryIdx);

		if (CurrentType == swPartCategoryType::Category && CurrentIndex == CategoryIdx)
			CurrentItem = CategoryItem;
	}

	if (CurrentItem)
		mCategoriesWidget->setCurrentItem(CurrentItem);

	QTimer::singleShot(0, this, &swPartSelectionWidget::SnapCategories);
}

void swPartSelectionWidget::UpdateModels()
{
	QTreeWidgetItem* CurrentItem = mCategoriesWidget->currentItem();

	if (CurrentItem && CurrentItem->data(0, static_cast<int>(swPartCategoryRole::Type)) == static_cast<int>(swPartCategoryType::Submodels))
		mPartsWidget->SetCategory(swPartCategoryType::Submodels, 0);
}
