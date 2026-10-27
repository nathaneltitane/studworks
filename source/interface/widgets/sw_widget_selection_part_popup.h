#pragma once

class swPartData;
class swPartSelectionWidget;
class swPartSelectionListView;

class swPartSelectionPopup : public QWidget
{
	Q_OBJECT

public:
	swPartSelectionPopup(swPartData* InitialPart, QWidget* Parent);
	virtual ~swPartSelectionPopup() = default;
	QSize sizeHint() const override;
	void SetHeightLimit(int Height);

	std::optional<swPartData*> GetPickedPart() const
	{
		return mAccepted ? std::optional<swPartData*>(mPickedPart) : std::nullopt;
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
	swPartData* mInitialPart = nullptr;
	swPartData* mPickedPart = nullptr;
	bool mAccepted = false;
	int mHeightLimit = QWIDGETSIZE_MAX;
};

std::optional<swPartData*> swShowPartSelectionPopup(swPartData* InitialPart, const std::vector<std::pair<swPartData*, std::string>>& CustomParts, int ColorIndex, QWidget* Parent, QPoint Position);
