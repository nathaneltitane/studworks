#pragma once

class swPieceData;
class swPieceSelectionWidget;
class swPieceSelectionListView;

class swPieceSelectionPopup : public QWidget
{
	Q_OBJECT

public:
	swPieceSelectionPopup(swPieceData* InitialPiece, QWidget* Parent);
	virtual ~swPieceSelectionPopup() = default;
	QSize sizeHint() const override;
	void SetHeightLimit(int Height);

	std::optional<swPieceData*> GetPickedPiece() const
	{
		return mAccepted ? std::optional<swPieceData*>(mPickedPiece) : std::nullopt;
	}

	swPieceSelectionWidget* GetPieceSelectionWidget() const
	{
		return mPieceSelectionWidget;
	}

protected slots:
	void Accept();
	void Reject();
	void ResizeToContents();

protected:
	void showEvent(QShowEvent* ShowEvent) override;
	void Close();
	void UpdateHeightLimit();
	QSize GetPiecesViewSizeHint(const swPieceSelectionListView* PiecesView, const QSize& PopupSize) const;

	swPieceSelectionWidget* mPieceSelectionWidget = nullptr;
	swPieceData* mInitialPiece = nullptr;
	swPieceData* mPickedPiece = nullptr;
	bool mAccepted = false;
	int mHeightLimit = QWIDGETSIZE_MAX;
};

std::optional<swPieceData*> swShowPieceSelectionPopup(swPieceData* InitialPiece, const std::vector<std::pair<swPieceData*, std::string>>& CustomPieces, int ColorIndex, QWidget* Parent, QPoint Position);
