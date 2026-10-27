#include "sw_global.h"
#include "sw_dialog_properties.h"
#include "ui_sw_dialog_properties.h"
#include "sw_window_utilities.h"
#include "sw_colors.h"
#include "sw_application.h"
#include "sw_data_piece.h"
#include "sw_library.h"
#include "sw_manager_thumbnail.h"
#include "sw_bricklink.h"
#include "sw_project.h"

class swFrozenPiecesView : public QTableView
{
public:
	explicit swFrozenPiecesView(swPiecesTableWidget* PiecesTable)
		: QTableView(PiecesTable), mPiecesTable(PiecesTable)
	{
	}

protected:
	QItemSelectionModel::SelectionFlags selectionCommand(const QModelIndex& Index, const QEvent* Event = nullptr) const override
	{
		return mPiecesTable->TrackSelectionCommand(Index, Event, QTableView::selectionCommand(Index, Event));
	}

	void setSelection(const QRect& Rect, QItemSelectionModel::SelectionFlags Command) override
	{
		if (!mPiecesTable->SelectSharedRange(Command))
			QTableView::setSelection(Rect, Command);
	}

private:
	swPiecesTableWidget* mPiecesTable;
};

class swPiecesThumbnailDelegate : public QStyledItemDelegate
{
public:
	using QStyledItemDelegate::QStyledItemDelegate;

protected:
	void initStyleOption(QStyleOptionViewItem* Option, const QModelIndex& Index) const override
	{
		QStyledItemDelegate::initStyleOption(Option, Index);
		Option->decorationAlignment = Qt::AlignCenter;
	}
};

swPiecesTableWidget::swPiecesTableWidget(QWidget* Parent)
	: QTableWidget(Parent)
{
}

void swPiecesTableWidget::FreezePieceColumns()
{
	// The thumbnail sits in a column of its own so copied cells only hold text.
	setItemDelegateForColumn(0, new swPiecesThumbnailDelegate(this));
	setColumnWidth(0, qMax(iconSize().width() + 8, horizontalHeader()->sectionSizeHint(0)));
	setColumnWidth(1, qMax(sizeHintForColumn(1), horizontalHeader()->sectionSizeHint(1)));
	mPieceColumnWidth = qMax(sizeHintForColumn(2), horizontalHeader()->sectionSizeHint(2));
	for (int Column = 0; Column < 3; Column++)
		horizontalHeader()->setSectionResizeMode(Column, QHeaderView::Fixed);
	setTextElideMode(Qt::ElideRight);

	// Both views share the data and selection, so sorting and row highlights stay in sync.
	mFrozenColumn = new swFrozenPiecesView(this);
	mFrozenColumn->setModel(model());
	mFrozenColumn->setSelectionModel(selectionModel());
	mFrozenColumn->setSelectionBehavior(QAbstractItemView::SelectRows);
	mFrozenColumn->setSelectionMode(selectionMode());
	mFrozenColumn->setEditTriggers(QAbstractItemView::NoEditTriggers);
	mFrozenColumn->setTextElideMode(Qt::ElideRight);
	mFrozenColumn->setIconSize(iconSize());
	mFrozenColumn->setItemDelegateForColumn(0, new swPiecesThumbnailDelegate(mFrozenColumn));
	mFrozenColumn->setFocusPolicy(Qt::NoFocus);
	mFrozenColumn->setFocusProxy(this);
	mFrozenColumn->setFrameShape(QFrame::NoFrame);
	mFrozenColumn->verticalHeader()->hide();
	mFrozenColumn->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
	mFrozenColumn->horizontalHeader()->setSectionsClickable(true);
	mFrozenColumn->horizontalHeader()->setSortIndicatorShown(true);
	mFrozenColumn->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	mFrozenColumn->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	mFrozenColumn->viewport()->installEventFilter(this);
	mFrozenColumn->horizontalHeader()->viewport()->installEventFilter(this);
	setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
	setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
	mFrozenColumn->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

	for (int Column = 3; Column < columnCount(); Column++)
		mFrozenColumn->hideColumn(Column);

	for (int Row = 0; Row < rowCount(); Row++)
		mFrozenColumn->setRowHeight(Row, rowHeight(Row));

	// The total label spans the thumbnail, ID and piece columns, the total row always sorts last.
	if (rowCount() > 0)
	{
		setSpan(rowCount() - 1, 0, 1, 3);
		mFrozenColumn->setSpan(rowCount() - 1, 0, 1, 3);
	}

	connect(verticalScrollBar(), &QScrollBar::valueChanged, mFrozenColumn->verticalScrollBar(), &QScrollBar::setValue);
	connect(mFrozenColumn->verticalScrollBar(), &QScrollBar::valueChanged, verticalScrollBar(), &QScrollBar::setValue);
	connect(verticalHeader(), &QHeaderView::sectionResized, this, [this](int Row, int, int Height)
	{
		mFrozenColumn->setRowHeight(Row, Height);
	});
	connect(horizontalHeader(), &QHeaderView::sectionResized, this, [this](int Column, int, int)
	{
		if (Column < 3)
			UpdateFrozenGeometry();
	});
	connect(horizontalHeader(), &QHeaderView::sortIndicatorChanged, mFrozenColumn->horizontalHeader(), &QHeaderView::setSortIndicator);
	connect(mFrozenColumn->horizontalHeader(), &QHeaderView::sectionClicked, this, [this](int Column)
	{
		const Qt::SortOrder Order = horizontalHeader()->sortIndicatorSection() == Column && horizontalHeader()->sortIndicatorOrder() == Qt::AscendingOrder
			? Qt::DescendingOrder : Qt::AscendingOrder;
		horizontalHeader()->setSortIndicator(Column, Order);
	});
	mFrozenColumn->horizontalHeader()->setSortIndicator(horizontalHeader()->sortIndicatorSection(), horizontalHeader()->sortIndicatorOrder());

	viewport()->stackUnder(mFrozenColumn);
	UpdateFrozenGeometry();
	mFrozenColumn->show();
}

bool swPiecesTableWidget::eventFilter(QObject* Object, QEvent* Event)
{
	if (mFrozenColumn && Event->type() == QEvent::Wheel &&
		(Object == mFrozenColumn->viewport() || Object == mFrozenColumn->horizontalHeader()->viewport()))
	{
		QCoreApplication::sendEvent(viewport(), Event);
		return true;
	}

	return QTableWidget::eventFilter(Object, Event);
}

QItemSelectionModel::SelectionFlags swPiecesTableWidget::TrackSelectionCommand(const QModelIndex& Index, const QEvent* Event, QItemSelectionModel::SelectionFlags Command) const
{
	// Qt keeps a separate range anchor in each view, even with a shared selection model.
	mSelectionEnd = Index;
	if (Command.testFlag(QItemSelectionModel::Current))
	{
		if (!mSelectionAnchor.isValid())
			mSelectionAnchor = currentIndex().isValid() ? currentIndex() : Index;
	}
	else if (Command != QItemSelectionModel::NoUpdate || (Event && Event->type() == QEvent::MouseButtonPress))
		mSelectionAnchor = Index;

	return Command;
}

QItemSelectionModel::SelectionFlags swPiecesTableWidget::selectionCommand(const QModelIndex& Index, const QEvent* Event) const
{
	return TrackSelectionCommand(Index, Event, QTableWidget::selectionCommand(Index, Event));
}

bool swPiecesTableWidget::SelectSharedRange(QItemSelectionModel::SelectionFlags Command)
{
	if (!Command.testFlag(QItemSelectionModel::Current) || !mSelectionAnchor.isValid() || !mSelectionEnd.isValid())
		return false;

	const int FirstRow = qMin(mSelectionAnchor.row(), mSelectionEnd.row());
	const int LastRow = qMax(mSelectionAnchor.row(), mSelectionEnd.row());
	selectionModel()->select(QItemSelection(model()->index(FirstRow, 0), model()->index(LastRow, columnCount() - 1)), Command);
	return true;
}

void swPiecesTableWidget::setSelection(const QRect& Rect, QItemSelectionModel::SelectionFlags Command)
{
	if (!SelectSharedRange(Command))
		QTableWidget::setSelection(Rect, Command);
}

void swPiecesTableWidget::UpdateFrozenGeometry()
{
	if (!mFrozenColumn)
		return;

	// The piece column always shows the full description.
	if (columnWidth(2) != mPieceColumnWidth)
		setColumnWidth(2, mPieceColumnWidth);

	for (int Column = 0; Column < 3; Column++)
		mFrozenColumn->setColumnWidth(Column, columnWidth(Column));
	mFrozenColumn->horizontalHeader()->setFixedHeight(horizontalHeader()->height());
	mFrozenColumn->setGeometry(viewport()->geometry().x(), frameWidth(), GetFrozenWidth(), viewport()->height() + horizontalHeader()->height());
}

int swPiecesTableWidget::GetFrozenWidth() const
{
	return columnWidth(0) + columnWidth(1) + columnWidth(2);
}

void swPiecesTableWidget::focusInEvent(QFocusEvent* Event)
{
	const bool HasCurrent = currentIndex().isValid();
	QTableWidget::focusInEvent(Event);

	// Receiving focus should not mark the first cell before the user chooses a row.
	if (!HasCurrent)
		selectionModel()->clearCurrentIndex();
}

void swPiecesTableWidget::resizeEvent(QResizeEvent* Event)
{
	QTableWidget::resizeEvent(Event);
	UpdateFrozenGeometry();
}

void swPiecesTableWidget::EnsureCountVisible(const QModelIndex& Index)
{
	if (mFrozenColumn && Index.column() > 2 && visualRect(Index).left() < GetFrozenWidth())
		horizontalScrollBar()->setValue(horizontalScrollBar()->value() + visualRect(Index).left() - GetFrozenWidth());
}

QModelIndex swPiecesTableWidget::moveCursor(CursorAction Action, Qt::KeyboardModifiers Modifiers)
{
	const QModelIndex Index = QTableWidget::moveCursor(Action, Modifiers);
	EnsureCountVisible(Index);
	return Index;
}

void swPiecesTableWidget::scrollTo(const QModelIndex& Index, ScrollHint Hint)
{
	if (!mFrozenColumn || Index.column() > 2)
	{
		QTableWidget::scrollTo(Index, Hint);
		EnsureCountVisible(Index);
	}
	else
		mFrozenColumn->scrollTo(Index, Hint);
}

class swPiecesTableWidgetItem : public QTableWidgetItem
{
public:
	explicit swPiecesTableWidgetItem(const QString& Text, int Type = QTableWidgetItem::Type)
		: QTableWidgetItem(Text, Type)
	{
		mLast = false;
		setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	}

	swPiecesTableWidgetItem(int Count, QLocale Locale)
		: QTableWidgetItem(swFormatIntegerLocalized(Count, Locale)), mLast(false)
	{
		setData(Qt::UserRole, Count);
	}

	bool operator<(const QTableWidgetItem& Other) const override
	{
		// Keep the total row at the bottom in both sort orders.
		const bool Descending = tableWidget() && tableWidget()->horizontalHeader()->sortIndicatorOrder() == Qt::DescendingOrder;

		if (mLast)
			return Descending;

		if (((const swPiecesTableWidgetItem&)Other).mLast)
			return !Descending;

		if (column() == 0)
			return data(Qt::UserRole).toString() < Other.data(Qt::UserRole).toString();

		if (column() > 2)
		{
			int Count = data(Qt::UserRole).toInt();
			int OtherCount = Other.data(Qt::UserRole).toInt();
			return Count < OtherCount;
		}

		return QTableWidgetItem::operator<(Other);
	}

	bool mLast;
};

swPropertiesDialog::swPropertiesDialog(QWidget* Parent, swPropertiesDialogOptions* Options)
	: QDialog(Parent), mOptions(Options), ui(new Ui::swPropertiesDialog)
{
	ui->setupUi(this);
	swSetupDialogHelp(this, ui->buttonBox);

	setWindowTitle(tr("%1 Properties").arg(mOptions->Properties.mFileName));
	setWindowState(windowState() | Qt::WindowMaximized);

	ui->DescriptionEdit->setText(mOptions->Properties.mDescription);
	ui->AuthorEdit->setText(mOptions->Properties.mAuthor);
	ui->CommentsEdit->setText(mOptions->Properties.mComments);

	const swVector3 Dimensions = Options->BoundingBox.Max - Options->BoundingBox.Min;
	QString Format = tr("Metric: %1 x %2 x %3 cm - Imperial: %4 x %5 x %6 inches - LDraw: %7 x %8 x %9 LDU");
	QString Measurements = Format.arg(swFormatDoubleLocalized(Dimensions.x * 0.04, locale(), 'f', 2), swFormatDoubleLocalized(Dimensions.y * 0.04, locale(), 'f', 2), swFormatDoubleLocalized(Dimensions.z * 0.04, locale(), 'f', 2),
	                                  swFormatDoubleLocalized(Dimensions.x / 64.0, locale(), 'f', 2), swFormatDoubleLocalized(Dimensions.y / 64.0, locale(), 'f', 2), swFormatDoubleLocalized(Dimensions.z / 64.0, locale(), 'f', 2),
	                                  swFormatDoubleLocalized(Dimensions.x, locale(), 'f', 2), swFormatDoubleLocalized(Dimensions.y, locale(), 'f', 2), swFormatDoubleLocalized(Dimensions.z, locale(), 'f', 2));

	ui->MeasurementsLabel->setText(Measurements);

	const swPiecesList& PiecesList = mOptions->PiecesList;
	QStringList HorizontalLabels;

	std::vector<bool> ColorsUsed(gColorList.size());

	for (const auto& PieceIt : PiecesList)
		for (const auto& ColorIt : PieceIt.second)
			ColorsUsed[ColorIt.first] = true;

	std::vector<int> ColorColumns(gColorList.size());
	int ColorCount = 0;

	HorizontalLabels.append(tr("Preview"));
	HorizontalLabels.append(tr("ID"));
	HorizontalLabels.append(tr("Piece"));

	for (size_t ColorIndex = 0; ColorIndex < gColorList.size(); ColorIndex++)
	{
		if (ColorsUsed[ColorIndex])
		{
			ColorColumns[ColorIndex] = ColorCount++;
			HorizontalLabels.append(gColorList[ColorIndex].Name);
		}
	}

	HorizontalLabels.append(tr("Total"));

	QTableWidget* PiecesTable = ui->PiecesTable;
	const int ThumbnailSize = 32;
	PiecesTable->setIconSize(QSize(ThumbnailSize, ThumbnailSize));
	PiecesTable->verticalHeader()->setDefaultSectionSize(qMax(PiecesTable->verticalHeader()->defaultSectionSize(), ThumbnailSize + 4));
	PiecesTable->setColumnCount(ColorCount + 4);
	PiecesTable->setRowCount((int)PiecesList.size() + 1);
	PiecesTable->setHorizontalHeaderLabels(HorizontalLabels);
	PiecesTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

	std::vector<int> InfoTotals(PiecesList.size());
	std::vector<int> ColorTotals(ColorCount);
	int Row = 0, Total = 0, UniqueCount = 0;
	swThumbnailManager* ThumbnailManager = swGetPiecesLibrary()->GetThumbnailManager();

	connect(ThumbnailManager, &swThumbnailManager::ThumbnailReady, this, &swPropertiesDialog::ThumbnailReady);

	for (const auto& PieceIt : PiecesList)
	{
		const swPieceData* PieceData = PieceIt.first;
		QString PieceId = PieceData->mFileName;

		if (PieceId.endsWith(QLatin1String(".dat"), Qt::CaseInsensitive))
			PieceId.chop(4);

		swPiecesTableWidgetItem* ThumbnailItem = new swPiecesTableWidgetItem(QString());
		ThumbnailItem->setData(Qt::UserRole, PieceId);
		PiecesTable->setItem(Row, 0, ThumbnailItem);
		PiecesTable->setItem(Row, 1, new swPiecesTableWidgetItem(PieceId));
		PiecesTable->setItem(Row, 2, new swPiecesTableWidgetItem(PieceData->m_strDescription));

		int ThumbnailColor = gDefaultColor, ThumbnailCount = 0;

		for (const auto& ColorIt : PieceIt.second)
		{
			int ColorIndex = ColorIt.first;
			int Count = ColorIt.second;

			swPiecesTableWidgetItem* Item = new swPiecesTableWidgetItem(Count, locale());
			Item->setTextAlignment(Qt::AlignCenter);
			PiecesTable->setItem(Row, ColorColumns[ColorIndex] + 3, Item);

			if (Count > ThumbnailCount)
			{
				ThumbnailColor = ColorIndex;
				ThumbnailCount = Count;
			}

			UniqueCount++;
			InfoTotals[Row] += Count;
			ColorTotals[ColorColumns[ColorIndex]] += Count;
			Total += Count;
		}

		// The thumbnail shows the piece in the color it is used the most.
		auto [ThumbnailId, Thumbnail] = ThumbnailManager->RequestThumbnail(const_cast<swPieceData*>(PieceData), ThumbnailColor, ThumbnailSize, devicePixelRatioF());
		mThumbnails.emplace_back(ThumbnailId, ThumbnailItem);

		if (!Thumbnail.isNull())
			ThumbnailItem->setData(Qt::DecorationRole, Thumbnail);

		for (int Column = 0; Column <= ColorCount + 2; Column++)
			if (!PiecesTable->item(Row, Column))
				PiecesTable->setItem(Row, Column, new swPiecesTableWidgetItem(QString()));

		Row++;
	}

	swPiecesTableWidgetItem* Item = new swPiecesTableWidgetItem(tr("Total"));
	Item->mLast = true;
	PiecesTable->setItem((int)InfoTotals.size(), 0, Item);

	Item = new swPiecesTableWidgetItem(QString());
	Item->mLast = true;
	PiecesTable->setItem((int)InfoTotals.size(), 1, Item);

	Item = new swPiecesTableWidgetItem(QString());
	Item->mLast = true;
	PiecesTable->setItem((int)InfoTotals.size(), 2, Item);

	for (Row = 0; Row < (int)InfoTotals.size(); Row++)
	{
		Item = new swPiecesTableWidgetItem(InfoTotals[Row], locale());
		Item->setTextAlignment(Qt::AlignCenter);
		PiecesTable->setItem(Row, ColorCount + 3, Item);
	}

	for (int ColorIndex = 0; ColorIndex < ColorCount; ColorIndex++)
	{
		Item = new swPiecesTableWidgetItem(ColorTotals[ColorIndex], locale());
		Item->mLast = true;
		Item->setTextAlignment(Qt::AlignCenter);
		PiecesTable->setItem((int)InfoTotals.size(), ColorIndex + 3, Item);
	}

	Item = new swPiecesTableWidgetItem(Total, locale());
	Item->mLast = true;
	Item->setTextAlignment(Qt::AlignCenter);
	PiecesTable->setItem((int)InfoTotals.size(), ColorCount + 3, Item);

	ui->PiecesTable->FreezePieceColumns();

	ui->PiecesCountLabel->setText(tr("Unique pieces: %1 - Total pieces: %2").arg(swFormatIntegerLocalized(UniqueCount, locale()), swFormatIntegerLocalized(Total, locale())));

	ui->ExportBrickLinkButton->setEnabled(!PiecesList.empty());
	ui->ExportCSVButton->setEnabled(!PiecesList.empty());

	connect(ui->ExportBrickLinkButton, &QPushButton::clicked, this, &swPropertiesDialog::ExportBrickLink);
	connect(ui->ExportCSVButton, &QPushButton::clicked, this, &swPropertiesDialog::ExportCSV);
}

swPropertiesDialog::~swPropertiesDialog()
{
	swThumbnailManager* ThumbnailManager = swGetPiecesLibrary()->GetThumbnailManager();

	for (const auto& [ThumbnailId, Item] : mThumbnails)
		ThumbnailManager->ReleaseThumbnail(ThumbnailId);

	delete ui;
}

void swPropertiesDialog::ThumbnailReady(swPieceThumbnailId ThumbnailId, QPixmap Pixmap)
{
	for (const auto& [Id, Item] : mThumbnails)
		if (Id == ThumbnailId)
			Item->setData(Qt::DecorationRole, Pixmap);
}

QString swPropertiesDialog::GetExportFileName(const QString& Extension, const QString& Title, const QString& Filter)
{
	const Project* Project = swGetActiveProject();
	const QString ProjectFileName = Project ? Project->GetFileName() : QString();
	QString FileName = QFileInfo(mOptions->Properties.mFileName).completeBaseName();

	if (FileName.isEmpty())
		FileName = tr("Pieces");

	FileName += "." + Extension;

	if (!ProjectFileName.isEmpty())
		FileName = QFileInfo(ProjectFileName).absoluteDir().filePath(FileName);

	return QFileDialog::getSaveFileName(this, Title, FileName, Filter);
}

void swPropertiesDialog::ExportBrickLink()
{
	const QString FileName = GetExportFileName("xml", tr("Export BrickLink"), tr("XML Files (*.xml);;All Files (*.*)"));

	if (FileName.isEmpty())
		return;

	swResult<void> Result = swExportBrickLink(FileName, mOptions->PiecesList);

	if (!Result && !Result.error().isEmpty())
		QMessageBox::warning(this, tr("Export BrickLink"), Result.error());
}

void swPropertiesDialog::ExportCSV()
{
	const QString FileName = GetExportFileName("csv", tr("Export CSV"), tr("CSV Files (*.csv);;All Files (*.*)"));

	if (FileName.isEmpty())
		return;

	swResult<void> Result = swExportCSV(FileName, mOptions->PiecesList);

	if (!Result && !Result.error().isEmpty())
		QMessageBox::warning(this, tr("Export CSV"), Result.error());
}

void swPropertiesDialog::accept()
{
	mOptions->Properties.mDescription = ui->DescriptionEdit->text();
	mOptions->Properties.mAuthor = ui->AuthorEdit->text();
	mOptions->Properties.mComments = ui->CommentsEdit->toPlainText();

	QDialog::accept();
}
