#pragma once

class swCollapsibleWidgetButton;

struct swColorListCell
{
	QRect Rect;
	int ColorIndex;
};

struct swColorListGroup
{
	QRect Rect;
	QString Name;
	std::vector<size_t> Cells;
	swCollapsibleWidgetButton* Button = nullptr;
	bool Collapsed = false;
};

class swColorList : public QWidget
{
	Q_OBJECT

public:
	swColorList(QWidget* Parent = nullptr, bool AllowNoColor = false);
	~swColorList() = default;

	QSize sizeHint() const override;
	int GetHeightForWidth(int Width, int* Columns = nullptr, int* Rows = nullptr) const;

	void SetCurrentColor(int colorIndex);
	void ToggleGroupCollapsed(int GroupIndex);

signals:
	void ColorChanged(int colorIndex);
	void ColorSelected(int colorIndex);
	void HeightChanged();

protected slots:
	void ColorsLoaded();

protected:
	void UpdateCells();
	void SetGroupCollapsed(int GroupIndex, bool Collapsed);
	int GetGroupHeaderHeight(const swColorListGroup& Group, bool FirstGroup) const;
	void UpdateRects();
	void SelectCell(size_t CellIndex);

	bool event(QEvent* Event) override;
	void paintEvent(QPaintEvent* PaintEvent) override;
	void resizeEvent(QResizeEvent* ResizeEvent) override;
	void mousePressEvent(QMouseEvent* MouseEvent) override;
	void mouseMoveEvent(QMouseEvent* MouseEvent) override;
	void keyPressEvent(QKeyEvent* KeyEvent) override;

	std::vector<swColorListCell> mCells;
	std::vector<swColorListGroup> mGroups;

	size_t mCurrentCell = 0;
	quint32 mColorCode = 0;

	int mColumns = 0;
	int mRows = 0;
	int mWidth = 0;
	int mHeight = 0;
	int mPreferredHeight = 0;
	bool mAllowNoColor;

	QPoint mDragStartPosition;
};

