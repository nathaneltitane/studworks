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

	void SetDirectory(const QString& Directory);

protected:
	void CreateTerminal();
	void showEvent(QShowEvent* Event) override;

	QPointer<KParts::ReadOnlyPart> mPart;
	QVBoxLayout* mLayout = nullptr;
	QString mDirectory;
};
