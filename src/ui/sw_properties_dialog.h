#pragma once

#include "sw_model.h"
#include "sw_thumbnail_manager.h"

struct swPropertiesDialogOptions
{
	swModelProperties Properties;
	swPartsList PartsList;
	swBoundingBox BoundingBox;
};

namespace Ui
{
class swPropertiesDialog;
}

class swPartsTableWidget : public QTableWidget
{
public:
	explicit swPartsTableWidget(QWidget* Parent = nullptr);
	void FreezePartColumns();
	void scrollTo(const QModelIndex& Index, ScrollHint Hint = EnsureVisible) override;

protected:
	bool eventFilter(QObject* Object, QEvent* Event) override;
	void focusInEvent(QFocusEvent* Event) override;
	void resizeEvent(QResizeEvent* Event) override;
	QModelIndex moveCursor(CursorAction Action, Qt::KeyboardModifiers Modifiers) override;
	QItemSelectionModel::SelectionFlags selectionCommand(const QModelIndex& Index, const QEvent* Event = nullptr) const override;
	void setSelection(const QRect& Rect, QItemSelectionModel::SelectionFlags Command) override;

private:
	friend class swFrozenPartsView;
	QItemSelectionModel::SelectionFlags TrackSelectionCommand(const QModelIndex& Index, const QEvent* Event, QItemSelectionModel::SelectionFlags Command) const;
	bool SelectSharedRange(QItemSelectionModel::SelectionFlags Command);
	void UpdateFrozenGeometry();
	void EnsureCountVisible(const QModelIndex& Index);
	QTableView* mFrozenColumn = nullptr;
	int GetFrozenWidth() const;
	int mPartColumnWidth = 0;
	mutable QPersistentModelIndex mSelectionAnchor;
	mutable QPersistentModelIndex mSelectionEnd;
};

class swPropertiesDialog : public QDialog
{
	Q_OBJECT

public:
	swPropertiesDialog(QWidget* Parent, swPropertiesDialogOptions* Options);
	~swPropertiesDialog();

	swPropertiesDialogOptions* mOptions;

public slots:
	void accept() override;

protected slots:
	void ThumbnailReady(swPartThumbnailId ThumbnailId, QPixmap Pixmap);
	void ExportBrickLink();
	void ExportCSV();

private:
	QString GetExportFileName(const QString& Extension, const QString& Title, const QString& Filter);

	Ui::swPropertiesDialog* ui;
	std::vector<std::pair<swPartThumbnailId, QTableWidgetItem*>> mThumbnails;
};
