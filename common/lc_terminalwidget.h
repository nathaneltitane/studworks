#pragma once

namespace KParts
{
	class ReadOnlyPart;
}

class lcTerminalWidget : public QWidget
{
	Q_OBJECT

public:
	explicit lcTerminalWidget(QWidget* Parent);

	void SetDirectory(const QString& Directory, bool Force);

protected:
	void CreateTerminal();
	void showEvent(QShowEvent* Event) override;
	bool eventFilter(QObject* Object, QEvent* Event) override;
	void OptionsMenuAboutToShow();

	QPointer<KParts::ReadOnlyPart> mPart;
	QVBoxLayout* mLayout = nullptr;
	QString mDirectory;
};
