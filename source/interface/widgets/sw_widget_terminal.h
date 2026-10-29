#pragma once

namespace KParts
{
	class ReadOnlyPart;
}

class swTerminalWidget : public QWidget
{
	Q_OBJECT

public:
	explicit swTerminalWidget(QWidget* Parent);

	void SetDirectory(const QString& Directory, bool Force);

protected:
	void CreateTerminal();
	void showEvent(QShowEvent* Event) override;
	bool eventFilter(QObject* Object, QEvent* Event) override;
	void OptionsMenuAboutToShow();
	void SnapToCells();

	QPointer<KParts::ReadOnlyPart> mPart;
	QPointer<QWidget> mDisplay;
	QVBoxLayout* mLayout = nullptr;
	QString mDirectory;
	bool mClickFocused = false;
	bool mSnapPending = false;
};
