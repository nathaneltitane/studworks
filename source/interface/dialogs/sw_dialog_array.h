#pragma once

#include "sw_math.h"

namespace Ui {
class swArrayDialog;
}

class swArrayDialog : public QDialog
{
	Q_OBJECT

public:
	swArrayDialog(QWidget* Parent);
	virtual ~swArrayDialog();

	int mCounts[3];
	swVector3 mOffsets[3];
	swVector3 mRotations[3];

public slots:
	void accept() override;

private:
	Ui::swArrayDialog* ui;
};

