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
	bool mPartEdgeColorEnabled;
	quint32 mPartEdgeColor;
	bool mBlackEdgeColorEnabled;
	quint32 mBlackEdgeColor;
	bool mDarkEdgeColorEnabled;
	quint32 mDarkEdgeColor;
	float mPartEdgeContrast;
	float mPartColorValueLDIndex;

protected slots:
	void SliderValueChanged(int);
	void ResetSliderButtonClicked();
	void ColorButtonClicked();
	void ColorCheckBoxClicked();
	void ResetColorButtonClicked();

protected:
	bool mAutomateEdgeColor;

	QSlider* PartColorValueLDIndexSlider;
	QSlider* PartEdgeContrastSlider;

	QLabel* PartEdgeContrast;
	QLabel* PartColorValueLDIndex;

	QToolButton* ResetPartEdgeContrastButton;
	QToolButton* ResetPartColorValueLDIndexButton;

	QCheckBox* StudCylinderColorEnabledBox;
	QCheckBox* PartEdgeColorEnabledBox;
	QCheckBox* BlackEdgeColorEnabledBox;
	QCheckBox* DarkEdgeColorEnabledBox;

	QToolButton* StudCylinderColorButton;
	QToolButton* PartEdgeColorButton;
	QToolButton* BlackEdgeColorButton;
	QToolButton* DarkEdgeColorButton;

	QToolButton* ResetStudCylinderColorButton;
	QToolButton* ResetPartEdgeColorButton;
	QToolButton* ResetBlackEdgeColorButton;
	QToolButton* ResetDarkEdgeColorButton;
};

