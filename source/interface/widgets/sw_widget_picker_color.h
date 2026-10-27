#pragma once

class swColorList;

class swColorPickerPopup : public QFrame
{
	Q_OBJECT

public:
	swColorPickerPopup(QWidget* Parent = nullptr, int ColorIndex = 0, bool AllowNoColor = false);
	virtual ~swColorPickerPopup();

	void exec();
	void SetPosition(const QWidget* Anchor);

signals:
	void Changed(int ColorIndex);
	void Selected(int ColorIndex);
	void Hid();

public slots:
	void ColorChanged(int ColorIndex);
	void ColorSelected(int ColorIndex);

protected:
	void showEvent(QShowEvent* ShowEvent) override;
	void hideEvent(QHideEvent* HideEvent) override;
	void mouseReleaseEvent(QMouseEvent* MouseEvent) override;

private:
	QEventLoop* mEventLoop = nullptr;
	swColorList* mColorList = nullptr;
};

class swColorPicker : public QPushButton
{
	Q_OBJECT

public:
	swColorPicker(QWidget* Parent = nullptr, bool AllowNoColor = false);
	virtual ~swColorPicker();

	int GetCurrentColor() const;
	int GetCurrentColorCode() const;
	void SetCurrentColor(int ColorIndex);
	void SetCurrentColorCode(int ColorCode);

public slots:
	void Changed(int ColorIndex);
	void Selected(int ColorIndex);

signals:
	void ColorChanged(int ColorIndex);

private slots:
	void ButtonPressed(bool Toggled);
	void PopupClosed();

protected:
	void UpdateIcon();

	int mCurrentColorIndex = 0;
	int mInitialColorIndex = 0;
	bool mAllowNoColor = false;
};
