#pragma once

#include "sw_instructions.h"

class swInstructionsUnitButton;

// the selection anchors follow the anchors of swInstructionsPlacementAnchor, they only exist in the panel
constexpr int SW_ANCHOR_FIRST_SELECTION = 3;
constexpr int SW_ANCHOR_LAST_SELECTION = 4;

class swInstructionsPlacementWidget : public QDockWidget
{
	Q_OBJECT

public:
	swInstructionsPlacementWidget(QWidget* Parent, swInstructions* Instructions);

	void SelectionChanged(QGraphicsItem* FocusItem, int SelectionCount);

signals:
	void AlignSelection(int Placement, bool LastSelection);

protected:
	void AnchorClicked(int Anchor);
	void PlacementClicked(int Placement);
	void ResetPositionClicked();
	void LockClicked(bool Locked);
	void UpdateWidget();
	swInstructionsPropertyType GetPlacementType() const;
	swInstructionsPropertyType GetAnchorType() const;
	bool IsInsideAnchor() const;

	static QIcon CreatePlacementIcon(int Placement, const QPalette& Palette);

	swInstructions* mInstructions = nullptr;
	swInstructionsPlacementElement mElement = swInstructionsPlacementElement::None;
	swInstructionsElementKey mElementKey = { swInstructionsPlacementElement::None, nullptr, 0 };
	QPointF mElementPosition;
	QPointF mPlacementPosition;
	int mSelectionCount = 0;
	bool mLastSelection = false;

	QLabel* mElementLabel = nullptr;
	QToolButton* mResetPositionButton = nullptr;
	QToolButton* mLockButton = nullptr;
	QButtonGroup* mAnchorGroup = nullptr;
	QWidget* mGridWidget = nullptr;
	std::array<QToolButton*, SW_PLACEMENT_COUNT> mPlacementButtons = {};
};

class swInstructionsGridWidget : public QDockWidget
{
	Q_OBJECT

public:
	swInstructionsGridWidget(QWidget* Parent, swInstructions* Instructions);

protected:
	void UpdateGrid();
	void GridSizeChanged(double Size);

	swInstructions* mInstructions = nullptr;

	QToolButton* mShowGridButton = nullptr;
	QToolButton* mSnapToGridButton = nullptr;
	QDoubleSpinBox* mGridSizeSpinBox = nullptr;
	swInstructionsUnitButton* mGridSizeUnitButton = nullptr;
	QComboBox* mGridScopeComboBox = nullptr;
};
