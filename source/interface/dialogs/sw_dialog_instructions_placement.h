#pragma once

#include "sw_instructions.h"

class swInstructionsUnitButton;

class swInstructionsPlacementWidget : public QDockWidget
{
	Q_OBJECT

public:
	swInstructionsPlacementWidget(QWidget* Parent, swInstructions* Instructions);

	void SelectionChanged(QGraphicsItem* FocusItem);

protected:
	void AnchorChanged(int Index);
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

	QLabel* mElementLabel = nullptr;
	QToolButton* mResetPositionButton = nullptr;
	QToolButton* mLockButton = nullptr;
	QComboBox* mAnchorComboBox = nullptr;
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
