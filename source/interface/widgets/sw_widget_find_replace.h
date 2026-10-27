#pragma once

class swElidableToolButton;

class swFindReplaceWidget : public QWidget
{
	Q_OBJECT

public:
	swFindReplaceWidget(QWidget* Parent, swModel* Model, bool Replace);

protected slots:
	void FindColorIndexChanged(int ColorIndex);
	void FindTextEdited(const QString& Text);
	void FindActivated(int Index);
	void ReplaceColorIndexChanged(int ColorIndex);
	void ReplaceButtonClicked();

protected:
	QComboBox* mFindPieceComboBox = nullptr;
	swElidableToolButton* mReplacePieceButton = nullptr;
};
