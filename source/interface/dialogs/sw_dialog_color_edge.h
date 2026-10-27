#pragma once

#include <QtWidgets>

class swPreferences;

class swAutomateEdgeColorDialog : public QDialog
{
	Q_OBJECT
public:
	swAutomateEdgeColorDialog(QWidget *Parent, bool ShowHighContrastDialog, const swPreferences& Preferences);
	bool mStudCylinderColorEnabled;
	quint32 mStudCylinderColor;
	bool mPieceEdgeColorEnabled;
	quint32 mPieceEdgeColor;
	bool mBlackEdgeColorEnabled;
	quint32 mBlackEdgeColor;
	bool mDarkEdgeColorEnabled;
	quint32 mDarkEdgeColor;
	float mPieceEdgeContrast;
	float mPieceColorValueLDIndex;

protected slots:
	void SliderValueChanged(int);
	void ResetSliderButtonClicked();
	void ColorButtonClicked();
	void ColorCheckBoxClicked();
	void ResetColorButtonClicked();

protected:
	bool mAutomateEdgeColor;

	QSlider* PieceColorValueLDIndexSlider;
	QSlider* PieceEdgeContrastSlider;

	QLabel* PieceEdgeContrast;
	QLabel* PieceColorValueLDIndex;

	QToolButton* ResetPieceEdgeContrastButton;
	QToolButton* ResetPieceColorValueLDIndexButton;

	QCheckBox* StudCylinderColorEnabledBox;
	QCheckBox* PieceEdgeColorEnabledBox;
	QCheckBox* BlackEdgeColorEnabledBox;
	QCheckBox* DarkEdgeColorEnabledBox;

	QToolButton* StudCylinderColorButton;
	QToolButton* PieceEdgeColorButton;
	QToolButton* BlackEdgeColorButton;
	QToolButton* DarkEdgeColorButton;

	QToolButton* ResetStudCylinderColorButton;
	QToolButton* ResetPieceEdgeColorButton;
	QToolButton* ResetBlackEdgeColorButton;
	QToolButton* ResetDarkEdgeColorButton;
};

