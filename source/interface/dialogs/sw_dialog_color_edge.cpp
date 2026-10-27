#include "sw_global.h"
#include "sw_dialog_color_edge.h"
#include "sw_window_utilities.h"
#include "sw_application.h"

swAutomateEdgeColorDialog::swAutomateEdgeColorDialog(QWidget* Parent, bool ShowHighContrastDialog, const swPreferences& Preferences)
	:QDialog(Parent)
{
	mStudCylinderColorEnabled = Preferences.mStudCylinderColorEnabled;
	mStudCylinderColor = Preferences.mStudCylinderColor;
	mPieceEdgeColorEnabled = Preferences.mPieceEdgeColorEnabled;
	mPieceEdgeColor = Preferences.mPieceEdgeColor;
	mBlackEdgeColorEnabled = Preferences.mBlackEdgeColorEnabled;
	mBlackEdgeColor = Preferences.mBlackEdgeColor;
	mDarkEdgeColorEnabled = Preferences.mDarkEdgeColorEnabled;
	mDarkEdgeColor = Preferences.mDarkEdgeColor;
	mPieceEdgeContrast = Preferences.mPieceEdgeContrast;
	mPieceColorValueLDIndex = Preferences.mPieceColorValueLDIndex;
	mAutomateEdgeColor = Preferences.mAutomateEdgeColor;

	setWindowTitle(tr("Color Preferences"));
	setWhatsThis(ShowHighContrastDialog ?
		tr("<p><b>High Contrast Edge Colors</b></p><p>Choose custom stud and edge colors for high contrast stud styles. Enable an override with its checkbox, then choose its color. Light/Dark Value sets the threshold for dark-piece edge colors. Automated edge colors take precedence over the piece edge overrides.</p><p>Choose OK to return changes to Preferences, then choose OK in Preferences to keep them. Cancel discards changes in this dialog. Reset restores an individual control's default.</p>") :
		tr("<p><b>Automated Edge Colors</b></p><p>Adjust the contrast and saturation used to calculate piece edge colors. Both controls range from 0.00 to 1.00. Reset restores a control to 0.50.</p><p>Choose OK to return changes to Preferences, then choose OK in Preferences to keep them. Cancel discards changes in this dialog.</p>"));
	QVBoxLayout* MainLayout = new  QVBoxLayout(this);

	QGroupBox* EdgeSettingsBox = new QGroupBox(tr("Edge Colors"), this);
	MainLayout->addWidget(EdgeSettingsBox);
	QGridLayout* EdgeSettingsLayout = new QGridLayout(EdgeSettingsBox);
	EdgeSettingsBox->setLayout(EdgeSettingsLayout);

	int LDIndexRow = 0;
	PieceEdgeContrast = nullptr;
	PieceEdgeContrastSlider = nullptr;
	if (!ShowHighContrastDialog)
	{
		LDIndexRow = 1;
		QLabel* PieceEdgeContrastLabel = new QLabel(tr("Contrast:"), this);
		PieceEdgeContrast = new QLabel(this);
		PieceEdgeContrastSlider = new QSlider(Qt::Horizontal, this);
		PieceEdgeContrastSlider->setRange(0, 100);
		PieceEdgeContrastSlider->setValue(mPieceEdgeContrast * 100);
		PieceEdgeContrastSlider->setToolTip(tr("Set edge contrast from 0.00 to 1.00 (default 0.50)."));
		PieceEdgeContrastSlider->setWhatsThis(tr("<p><b>Edge Contrast</b></p><p>Adjust the contrast used by automated edge colors to distinguish edges from piece surfaces. The range is 0.00 to 1.00; Reset restores 0.50.</p>"));
		PieceEdgeContrastLabel->setBuddy(PieceEdgeContrastSlider);
		PieceEdgeContrastLabel->setToolTip(PieceEdgeContrastSlider->toolTip());
		PieceEdgeContrastLabel->setWhatsThis(PieceEdgeContrastSlider->whatsThis());
		connect(PieceEdgeContrastSlider, &QSlider::valueChanged, this, &swAutomateEdgeColorDialog::SliderValueChanged);
		emit PieceEdgeContrastSlider->valueChanged(PieceEdgeContrastSlider->value());

		ResetPieceEdgeContrastButton = new QToolButton(this);
		ResetPieceEdgeContrastButton->setText(tr("Reset"));
		ResetPieceEdgeContrastButton->setToolTip(tr("Restore edge contrast to 0.50."));
		ResetPieceEdgeContrastButton->setWhatsThis(PieceEdgeContrastSlider->whatsThis());
		connect(ResetPieceEdgeContrastButton, &QToolButton::clicked, this, &swAutomateEdgeColorDialog::ResetSliderButtonClicked);

		EdgeSettingsLayout->addWidget(PieceEdgeContrastLabel,0,0);
		EdgeSettingsLayout->addWidget(PieceEdgeContrastSlider,0,1);
		EdgeSettingsLayout->addWidget(PieceEdgeContrast,0,2);
		EdgeSettingsLayout->addWidget(ResetPieceEdgeContrastButton,0,3);
	}

	QLabel* PieceColorValueLDIndexLabel = new QLabel(ShowHighContrastDialog ? tr("Light/Dark Value:") : tr("Saturation:"), this);
	PieceColorValueLDIndex = new QLabel(this);
	PieceColorValueLDIndexSlider = new QSlider(Qt::Horizontal, this);
	PieceColorValueLDIndexSlider->setRange(0, 100);
	PieceColorValueLDIndexSlider->setValue(mPieceColorValueLDIndex * 100);
	PieceColorValueLDIndexSlider->setToolTip(ShowHighContrastDialog ?
		tr("Set the light/dark threshold from 0.00 to 1.00 (default 0.50).") :
		tr("Set edge-color saturation from 0.00 to 1.00 (default 0.50)."));
	PieceColorValueLDIndexSlider->setWhatsThis(ShowHighContrastDialog ?
		tr("<p><b>Light/Dark Value</b></p><p>Classify piece colors by brightness to choose which pieces receive the dark-piece edge override. Colors below the threshold are treated as dark. A separate enabled black-piece override takes priority for black pieces. The range is 0.00 to 1.00; Reset restores 0.50.</p>") :
		tr("<p><b>Edge Saturation</b></p><p>Adjust the tint or shade used to calculate automated edge colors from piece colors. The range is 0.00 to 1.00; Reset restores 0.50.</p>"));
	PieceColorValueLDIndexLabel->setBuddy(PieceColorValueLDIndexSlider);
	PieceColorValueLDIndexLabel->setToolTip(PieceColorValueLDIndexSlider->toolTip());
	PieceColorValueLDIndexLabel->setWhatsThis(PieceColorValueLDIndexSlider->whatsThis());
	connect(PieceColorValueLDIndexSlider, &QSlider::valueChanged, this, &swAutomateEdgeColorDialog::SliderValueChanged);
	emit PieceColorValueLDIndexSlider->valueChanged(PieceColorValueLDIndexSlider->value());

	ResetPieceColorValueLDIndexButton = new QToolButton(this);
	ResetPieceColorValueLDIndexButton->setText(tr("Reset"));
	ResetPieceColorValueLDIndexButton->setToolTip(ShowHighContrastDialog ? tr("Restore the light/dark threshold to 0.50.") : tr("Restore edge-color saturation to 0.50."));
	ResetPieceColorValueLDIndexButton->setWhatsThis(PieceColorValueLDIndexSlider->whatsThis());
	connect(ResetPieceColorValueLDIndexButton, &QToolButton::clicked, this, &swAutomateEdgeColorDialog::ResetSliderButtonClicked);

	EdgeSettingsLayout->addWidget(PieceColorValueLDIndexLabel,LDIndexRow,0);
	EdgeSettingsLayout->addWidget(PieceColorValueLDIndexSlider,LDIndexRow,1);
	EdgeSettingsLayout->addWidget(PieceColorValueLDIndex,LDIndexRow,2);
	EdgeSettingsLayout->addWidget(ResetPieceColorValueLDIndexButton,LDIndexRow,3);

	QGroupBox* HighContrastColorBox = new QGroupBox(tr("High Contrast"), this);
	HighContrastColorBox->setVisible(ShowHighContrastDialog);
	MainLayout->addWidget(HighContrastColorBox);
	QGridLayout* HighContrastColorLayout = new QGridLayout(HighContrastColorBox);
	HighContrastColorBox->setLayout(HighContrastColorLayout);

	auto SetButtonPixmap = [](quint32 Color, QToolButton* Button)
	{
		QPixmap Pixmap(12, 12);
		QColor ButtonColor(QColor(SW_RGBA_RED(Color), SW_RGBA_GREEN(Color), SW_RGBA_BLUE(Color)));
		Pixmap.fill(ButtonColor);
		Button->setIcon(Pixmap);
		Button->setToolTip(ButtonColor.name().toUpper());
	};

	StudCylinderColorEnabledBox = new QCheckBox(tr("Stud Cylinder Color:"), this);
	StudCylinderColorEnabledBox->setChecked(mStudCylinderColorEnabled);
	connect(StudCylinderColorEnabledBox, &QCheckBox::clicked, this, &swAutomateEdgeColorDialog::ColorCheckBoxClicked);

	StudCylinderColorButton = new QToolButton(this);
	StudCylinderColorButton->setEnabled(mStudCylinderColorEnabled);
	SetButtonPixmap(mStudCylinderColor, StudCylinderColorButton);
	connect(StudCylinderColorButton, &QToolButton::clicked, this, &swAutomateEdgeColorDialog::ColorButtonClicked);

	ResetStudCylinderColorButton = new QToolButton(this);
	ResetStudCylinderColorButton->setText(tr("Reset"));
	ResetStudCylinderColorButton->setEnabled(mStudCylinderColorEnabled);
	connect(ResetStudCylinderColorButton, &QToolButton::clicked, this, &swAutomateEdgeColorDialog::ResetColorButtonClicked);

	HighContrastColorLayout->addWidget(StudCylinderColorEnabledBox,0,0);
	HighContrastColorLayout->addWidget(StudCylinderColorButton,0,1);
	HighContrastColorLayout->addWidget(ResetStudCylinderColorButton,0,2);

	PieceEdgeColorEnabledBox = new QCheckBox(tr("Pieces Edge Color:"), this);
	PieceEdgeColorEnabledBox->setChecked(mPieceEdgeColorEnabled);
	connect(PieceEdgeColorEnabledBox, &QCheckBox::clicked, this, &swAutomateEdgeColorDialog::ColorCheckBoxClicked);

	PieceEdgeColorButton = new QToolButton(this);
	PieceEdgeColorButton->setEnabled(mPieceEdgeColorEnabled);
	SetButtonPixmap(mPieceEdgeColor, PieceEdgeColorButton);
	connect(PieceEdgeColorButton, &QToolButton::clicked, this, &swAutomateEdgeColorDialog::ColorButtonClicked);

	ResetPieceEdgeColorButton = new QToolButton(this);
	ResetPieceEdgeColorButton->setText(tr("Reset"));
	ResetPieceEdgeColorButton->setEnabled(mPieceEdgeColorEnabled);
	connect(ResetPieceEdgeColorButton, &QToolButton::clicked, this, &swAutomateEdgeColorDialog::ResetColorButtonClicked);

	HighContrastColorLayout->addWidget(PieceEdgeColorEnabledBox,1,0);
	HighContrastColorLayout->addWidget(PieceEdgeColorButton,1,1);
	HighContrastColorLayout->addWidget(ResetPieceEdgeColorButton,1,2);

	BlackEdgeColorEnabledBox = new QCheckBox(tr("Black Pieces Edge Color:"), this);
	BlackEdgeColorEnabledBox->setChecked(mBlackEdgeColorEnabled);
	connect(BlackEdgeColorEnabledBox, &QCheckBox::clicked, this, &swAutomateEdgeColorDialog::ColorCheckBoxClicked);

	BlackEdgeColorButton = new QToolButton(this);
	BlackEdgeColorButton->setEnabled(mBlackEdgeColorEnabled);
	SetButtonPixmap(mBlackEdgeColor, BlackEdgeColorButton);
	connect(BlackEdgeColorButton, &QToolButton::clicked, this, &swAutomateEdgeColorDialog::ColorButtonClicked);

	ResetBlackEdgeColorButton = new QToolButton(this);
	ResetBlackEdgeColorButton->setText(tr("Reset"));
	ResetBlackEdgeColorButton->setEnabled(mBlackEdgeColorEnabled);
	connect(ResetBlackEdgeColorButton, &QToolButton::clicked, this, &swAutomateEdgeColorDialog::ResetColorButtonClicked);

	HighContrastColorLayout->addWidget(BlackEdgeColorEnabledBox,2,0);
	HighContrastColorLayout->addWidget(BlackEdgeColorButton,2,1);
	HighContrastColorLayout->addWidget(ResetBlackEdgeColorButton,2,2);

	DarkEdgeColorEnabledBox = new QCheckBox(tr("Dark Pieces Edge Color:"), this);
	DarkEdgeColorEnabledBox->setChecked(mDarkEdgeColorEnabled);
	connect(DarkEdgeColorEnabledBox, &QCheckBox::clicked, this, &swAutomateEdgeColorDialog::ColorCheckBoxClicked);

	DarkEdgeColorButton = new QToolButton(this);
	DarkEdgeColorButton->setEnabled(mDarkEdgeColorEnabled);
	SetButtonPixmap(mDarkEdgeColor, DarkEdgeColorButton);
	connect(DarkEdgeColorButton, &QToolButton::clicked, this, &swAutomateEdgeColorDialog::ColorButtonClicked);

	ResetDarkEdgeColorButton = new QToolButton(this);
	ResetDarkEdgeColorButton->setText(tr("Reset"));
	ResetDarkEdgeColorButton->setEnabled(mDarkEdgeColorEnabled);
	connect(ResetDarkEdgeColorButton, &QToolButton::clicked, this, &swAutomateEdgeColorDialog::ResetColorButtonClicked);

	HighContrastColorLayout->addWidget(DarkEdgeColorEnabledBox,3,0);
	HighContrastColorLayout->addWidget(DarkEdgeColorButton,3,1);
	HighContrastColorLayout->addWidget(ResetDarkEdgeColorButton,3,2);

	auto SetupColorHelp = [](QCheckBox* CheckBox, QToolButton* ColorButton, QToolButton* ResetButton, const QString& Help)
	{
		QString Name = CheckBox->text();
		Name.remove(':');
		CheckBox->setToolTip(tr("Enable the custom color for %1.").arg(Name));
		CheckBox->setWhatsThis(Help);
		ColorButton->setAccessibleName(Name);
		ColorButton->setWhatsThis(Help);
		ResetButton->setToolTip(tr("Restore the default color for %1.").arg(Name));
		ResetButton->setWhatsThis(Help);
	};

	SetupColorHelp(StudCylinderColorEnabledBox, StudCylinderColorButton, ResetStudCylinderColorButton,
		tr("Enable a custom color for stud cylinders in high contrast stud styles, then use the color button to choose it."));
	SetupColorHelp(PieceEdgeColorEnabledBox, PieceEdgeColorButton, ResetPieceEdgeColorButton,
		tr("Enable a custom edge color for pieces not covered by the black or dark overrides. Automated edge colors take precedence."));
	SetupColorHelp(BlackEdgeColorEnabledBox, BlackEdgeColorButton, ResetBlackEdgeColorButton,
		tr("Enable a custom edge color for black pieces. This override takes priority over the dark-piece and general edge colors. Automated edge colors take precedence."));
	SetupColorHelp(DarkEdgeColorEnabledBox, DarkEdgeColorButton, ResetDarkEdgeColorButton,
		tr("Enable a custom edge color for pieces below the light/dark threshold, except black pieces with their own enabled override. Automated edge colors take precedence."));

	QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Help, Qt::Horizontal, this);
	MainLayout->addWidget(buttonBox);
	swSetupDialogHelp(this, buttonBox);
	QObject::connect(buttonBox, &QDialogButtonBox::accepted, this, &swAutomateEdgeColorDialog::accept);
	QObject::connect(buttonBox, &QDialogButtonBox::rejected, this, &swAutomateEdgeColorDialog::reject);

	setMinimumSize(220,100);
}

void swAutomateEdgeColorDialog::ColorCheckBoxClicked()
{
	QObject* CheckBox = sender();
	if (CheckBox == StudCylinderColorEnabledBox)
	{
		mStudCylinderColorEnabled = StudCylinderColorEnabledBox->isChecked();
		StudCylinderColorButton->setEnabled(mStudCylinderColorEnabled);
		ResetStudCylinderColorButton->setEnabled(mStudCylinderColorEnabled);
	}
	else if (CheckBox == PieceEdgeColorEnabledBox)
	{
		mPieceEdgeColorEnabled = PieceEdgeColorEnabledBox->isChecked();
		PieceEdgeColorButton->setEnabled(mPieceEdgeColorEnabled);
		ResetPieceEdgeColorButton->setEnabled(mPieceEdgeColorEnabled);
	}
	else if (CheckBox == BlackEdgeColorEnabledBox)
	{
		mBlackEdgeColorEnabled = BlackEdgeColorEnabledBox->isChecked();
		BlackEdgeColorButton->setEnabled(mBlackEdgeColorEnabled);
		ResetBlackEdgeColorButton->setEnabled(mBlackEdgeColorEnabled);
	}
	else if (CheckBox == DarkEdgeColorEnabledBox)
	{
		mDarkEdgeColorEnabled = DarkEdgeColorEnabledBox->isChecked();
		DarkEdgeColorButton->setEnabled(mDarkEdgeColorEnabled);
		ResetDarkEdgeColorButton->setEnabled(mDarkEdgeColorEnabled);
	}
}

void swAutomateEdgeColorDialog::SliderValueChanged(int Value)
{
	if (sender() == PieceEdgeContrastSlider)
	{
		mPieceEdgeContrast = Value * 0.01f;
		PieceEdgeContrast->setText(swFormatDoubleLocalized(mPieceEdgeContrast, locale(), 'f', 2));
	}
	else if (sender() == PieceColorValueLDIndexSlider)
	{
		mPieceColorValueLDIndex = Value * 0.01f;
		PieceColorValueLDIndex->setText(swFormatDoubleLocalized(mPieceColorValueLDIndex, locale(), 'f', 2));
	}
}

void swAutomateEdgeColorDialog::ColorButtonClicked()
{
	QObject* Button = sender();
	QString Title;
	quint32* Color = nullptr;
	QColorDialog::ColorDialogOptions DialogOptions;

	if (Button == StudCylinderColorButton)
	{
		Title = tr("Select Stud Cylinder Color");
		Color = &mStudCylinderColor;
	}
	else if (Button == PieceEdgeColorButton)
	{
		Title = tr("Select Piece Edge Color");
		Color = &mPieceEdgeColor;
	}
	else if (Button == BlackEdgeColorButton)
	{
		if (mAutomateEdgeColor)
		{
			QMessageBox msgBox;
			msgBox.setWindowTitle(tr("Select Black Edge Color"));
			msgBox.setText(tr("Automate edge color appears to be enabled.<br>Black pieces edge color will not be accessible.<br>Do you want to continue?"));
			if (msgBox.exec() != QMessageBox::Accepted)
				return;
		}
		Title = tr("Select Black Edge Color");
		Color = &mBlackEdgeColor;
	}
	else if (Button == DarkEdgeColorButton)
	{
		if (mAutomateEdgeColor)
		{
			QMessageBox msgBox;
			msgBox.setWindowTitle(tr("Select Dark Edge Color"));
			msgBox.setText(tr("Automate edge color appears to be enabled.<br>Dark pieces edge color will not be accessible.<br>Do you want to continue?"));
			if (msgBox.exec() != QMessageBox::Accepted)
				return;
		}
		Title = tr("Select Dark Edge Color");
		Color = &mDarkEdgeColor;
	}
	else
		return;

	QColor OldColor = QColor(SW_RGBA_RED(*Color), SW_RGBA_GREEN(*Color), SW_RGBA_BLUE(*Color), SW_RGBA_ALPHA(*Color));
	QColor NewColor = QColorDialog::getColor(OldColor, this, Title, DialogOptions);

	if (NewColor == OldColor || !NewColor.isValid())
		return;

	*Color = SW_RGBA(NewColor.red(), NewColor.green(), NewColor.blue(), NewColor.alpha());

	QPixmap Pix(12, 12);
	NewColor.setAlpha(255);
	Pix.fill(NewColor);
	((QToolButton*)Button)->setIcon(Pix);
	((QToolButton*)Button)->setToolTip(NewColor.name().toUpper());
}

void swAutomateEdgeColorDialog::ResetSliderButtonClicked()
{
	if (sender() == ResetPieceEdgeContrastButton)
	{
		PieceEdgeContrastSlider->setValue(0.5f * 100);
	}
	else if (sender() == ResetPieceColorValueLDIndexButton)
	{
		PieceColorValueLDIndexSlider->setValue(0.5f * 100);
	}
}

void swAutomateEdgeColorDialog::ResetColorButtonClicked()
{
	quint32* Color = nullptr;
	QPixmap Pix(12, 12);
	QColor ResetColor;

	if (sender() == ResetStudCylinderColorButton)
	{
		Color = &mStudCylinderColor;
		*Color = SW_RGBA(27, 42, 52, 255);
		ResetColor = QColor(SW_RGBA_RED(*Color), SW_RGBA_GREEN(*Color), SW_RGBA_BLUE(*Color), SW_RGBA_ALPHA(*Color));
		Pix.fill(ResetColor);
		StudCylinderColorButton->setIcon(Pix);
		StudCylinderColorButton->setToolTip(ResetColor.name().toUpper());
	}
	else if (sender() == ResetPieceEdgeColorButton)
	{
		Color = &mPieceEdgeColor;
		*Color = SW_RGBA(0, 0, 0, 255);
		ResetColor = QColor(SW_RGBA_RED(*Color), SW_RGBA_GREEN(*Color), SW_RGBA_BLUE(*Color), SW_RGBA_ALPHA(*Color));
		Pix.fill(ResetColor);
		PieceEdgeColorButton->setIcon(Pix);
		PieceEdgeColorButton->setToolTip(ResetColor.name().toUpper());
	}
	else if (sender() == ResetBlackEdgeColorButton)
	{
		Color = &mBlackEdgeColor;
		*Color = SW_RGBA(255, 255, 255, 255);
		ResetColor = QColor(SW_RGBA_RED(*Color), SW_RGBA_GREEN(*Color), SW_RGBA_BLUE(*Color), SW_RGBA_ALPHA(*Color));
		Pix.fill(ResetColor);
		BlackEdgeColorButton->setIcon(Pix);
		BlackEdgeColorButton->setToolTip(ResetColor.name().toUpper());
	}
	else if (sender() == ResetDarkEdgeColorButton)
	{
		Color = &mDarkEdgeColor;
		*Color = SW_RGBA(27, 42, 52, 255);
		ResetColor = QColor(SW_RGBA_RED(*Color), SW_RGBA_GREEN(*Color), SW_RGBA_BLUE(*Color), SW_RGBA_ALPHA(*Color));
		Pix.fill(ResetColor);
		DarkEdgeColorButton->setIcon(Pix);
		DarkEdgeColorButton->setToolTip(ResetColor.name().toUpper());
	}
}
