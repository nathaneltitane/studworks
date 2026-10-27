#pragma once

class swPieceData;
class swPieceSelectionWidget;
class swPieceSelectionListView;

class swPieceSelectionPopup : public QWidget
{
	Q_OBJECT

public:
	swPieceSelectionPopup(swPieceData* InitialPart, QWidget* Parent);
	virtual ~swPieceSelectionPopup() = default;
	QSize sizeHint() const override;
	void SetHeightLimit(int Height);

	std::optional<swPieceData*> GetPickedPart() const
	{
		return mAccepted ? std::optional<swPieceData*>(mPickedPiece) : std::nullopt;
	}

	swPieceSelectionWidget* GetPartSelectionWidget() const
	{
		return mPartSelectionWidget;
	}

protected slots:
	void Accept();
	void Reject();
	void ResizeToContents();

protected:
	void showEvent(QShowEvent* ShowEvent) override;
	void Close();
	void UpdateHeightLimit();
	QSize GetPartsViewSizeHint(const swPieceSelectionListView* PartsView, const QSize& PopupSize) const;

	swPieceSelectionWidget* mPartSelectionWidget = nullptr;
	swPieceData* mInitialPart = nullptr;
	swPieceData* mPickedPiece = nullptr;
	bool mAccepted = false;
	int mHeightLimit = QWIDGETSIZE_MAX;
};

std::optional<swPieceData*> swShowPieceSelectionPopup(swPieceData* InitialPart, const std::vector<std::pair<swPieceData*, std::string>>& CustomParts, int ColorIndex, QWidget* Parent, QPoint Position);
