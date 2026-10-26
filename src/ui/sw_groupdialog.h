#pragma once

namespace Ui {
class swGroupDialog;
}

class swGroupDialog : public QDialog
{
	Q_OBJECT

public:
	explicit swGroupDialog(QWidget* Parent, const QString& Name, const std::vector<std::unique_ptr<swGroup>>& Groups);
	virtual ~swGroupDialog();

	QString mName;

public slots:
	void accept() override;

private:
	const std::vector<std::unique_ptr<swGroup>>& mGroups;
	Ui::swGroupDialog* ui;
};
