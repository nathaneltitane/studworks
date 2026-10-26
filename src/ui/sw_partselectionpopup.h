#pragma once

class swPieceData;
class swPartSelectionWidget;
class swPartSelectionListView;

class swPartSelectionPopup : public QWidget
{
	Q_OBJECT

public:
	swPartSelectionPopup(swPieceData* InitialPart, QWidget* Parent);
	virtual ~swPartSelectionPopup() = default;
	QSize sizeHint() const override;
	void SetHeightLimit(int Height);

	std::optional<swPieceData*> GetPickedPart() const
	{
		return mAccepted ? std::optional<swPieceData*>(mPickedPiece) : std::nullopt;
	}

	swPartSelectionWidget* GetPartSelectionWidget() const
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
	QSize GetPartsViewSizeHint(const swPartSelectionListView* PartsView, const QSize& PopupSize) const;

	swPartSelectionWidget* mPartSelectionWidget = nullptr;
	swPieceData* mInitialPart = nullptr;
	swPieceData* mPickedPiece = nullptr;
	bool mAccepted = false;
	int mHeightLimit = QWIDGETSIZE_MAX;
};

std::optional<swPieceData*> swShowPartSelectionPopup(swPieceData* InitialPart, const std::vector<std::pair<swPieceData*, std::string>>& CustomParts, int ColorIndex, QWidget* Parent, QPoint Position);
