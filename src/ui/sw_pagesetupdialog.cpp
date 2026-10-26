#include "sw_global.h"
#include "sw_pagesetupdialog.h"
#include "ui_sw_pagesetupdialog.h"
#include "sw_instructions.h"
#include "sw_profile.h"

struct lcPageSetupPreset
{
	const char* Name;
	float Width;
	float Height;
	lcInstructionsPageUnits Units;
};

// paper sizes are portrait and screen sizes landscape, the orientation buttons swap them
static const lcPageSetupPreset gPageSetupPresets[] =
{
	{ QT_TRANSLATE_NOOP("lcPageSetupDialog", "A0"), 841.0f, 1189.0f, lcInstructionsPageUnits::Millimeters },
	{ QT_TRANSLATE_NOOP("lcPageSetupDialog", "A1"), 594.0f, 841.0f, lcInstructionsPageUnits::Millimeters },
	{ QT_TRANSLATE_NOOP("lcPageSetupDialog", "A2"), 420.0f, 594.0f, lcInstructionsPageUnits::Millimeters },
	{ QT_TRANSLATE_NOOP("lcPageSetupDialog", "A3"), 297.0f, 420.0f, lcInstructionsPageUnits::Millimeters },
	{ QT_TRANSLATE_NOOP("lcPageSetupDialog", "A4"), 210.0f, 297.0f, lcInstructionsPageUnits::Millimeters },
	{ QT_TRANSLATE_NOOP("lcPageSetupDialog", "A5"), 148.0f, 210.0f, lcInstructionsPageUnits::Millimeters },
	{ QT_TRANSLATE_NOOP("lcPageSetupDialog", "A6"), 105.0f, 148.0f, lcInstructionsPageUnits::Millimeters },
	{ QT_TRANSLATE_NOOP("lcPageSetupDialog", "B4"), 250.0f, 353.0f, lcInstructionsPageUnits::Millimeters },
	{ QT_TRANSLATE_NOOP("lcPageSetupDialog", "B5"), 176.0f, 250.0f, lcInstructionsPageUnits::Millimeters },
	{ QT_TRANSLATE_NOOP("lcPageSetupDialog", "B6"), 125.0f, 176.0f, lcInstructionsPageUnits::Millimeters },
	{ QT_TRANSLATE_NOOP("lcPageSetupDialog", "JIS B4"), 257.0f, 364.0f, lcInstructionsPageUnits::Millimeters },
	{ QT_TRANSLATE_NOOP("lcPageSetupDialog", "JIS B5"), 182.0f, 257.0f, lcInstructionsPageUnits::Millimeters },
	{ nullptr, 0.0f, 0.0f, lcInstructionsPageUnits::Pixels },
	{ QT_TRANSLATE_NOOP("lcPageSetupDialog", "Letter"), 8.5f, 11.0f, lcInstructionsPageUnits::Inches },
	{ QT_TRANSLATE_NOOP("lcPageSetupDialog", "Legal"), 8.5f, 14.0f, lcInstructionsPageUnits::Inches },
	{ QT_TRANSLATE_NOOP("lcPageSetupDialog", "Tabloid"), 11.0f, 17.0f, lcInstructionsPageUnits::Inches },
	{ QT_TRANSLATE_NOOP("lcPageSetupDialog", "Executive"), 7.25f, 10.5f, lcInstructionsPageUnits::Inches },
	{ QT_TRANSLATE_NOOP("lcPageSetupDialog", "Statement"), 5.5f, 8.5f, lcInstructionsPageUnits::Inches },
	{ QT_TRANSLATE_NOOP("lcPageSetupDialog", "ANSI C"), 17.0f, 22.0f, lcInstructionsPageUnits::Inches },
	{ nullptr, 0.0f, 0.0f, lcInstructionsPageUnits::Pixels },
	{ QT_TRANSLATE_NOOP("lcPageSetupDialog", "XGA"), 1024.0f, 768.0f, lcInstructionsPageUnits::Pixels },
	{ QT_TRANSLATE_NOOP("lcPageSetupDialog", "HD"), 1280.0f, 720.0f, lcInstructionsPageUnits::Pixels },
	{ QT_TRANSLATE_NOOP("lcPageSetupDialog", "WXGA"), 1366.0f, 768.0f, lcInstructionsPageUnits::Pixels },
	{ QT_TRANSLATE_NOOP("lcPageSetupDialog", "UXGA"), 1600.0f, 1200.0f, lcInstructionsPageUnits::Pixels },
	{ QT_TRANSLATE_NOOP("lcPageSetupDialog", "Full HD"), 1920.0f, 1080.0f, lcInstructionsPageUnits::Pixels },
	{ QT_TRANSLATE_NOOP("lcPageSetupDialog", "WUXGA"), 1920.0f, 1200.0f, lcInstructionsPageUnits::Pixels },
	{ QT_TRANSLATE_NOOP("lcPageSetupDialog", "Tablet"), 2048.0f, 1536.0f, lcInstructionsPageUnits::Pixels },
	{ QT_TRANSLATE_NOOP("lcPageSetupDialog", "QHD"), 2560.0f, 1440.0f, lcInstructionsPageUnits::Pixels },
	{ QT_TRANSLATE_NOOP("lcPageSetupDialog", "4K UHD"), 3840.0f, 2160.0f, lcInstructionsPageUnits::Pixels },
};

static float lcGetPageUnitScale(lcInstructionsPageUnits Units)
{
	switch (Units)
	{
	case lcInstructionsPageUnits::Pixels:
		return 1.0f;

	case lcInstructionsPageUnits::Millimeters:
		return 72.0f / 25.4f;

	case lcInstructionsPageUnits::Centimeters:
		return 72.0f / 2.54f;

	case lcInstructionsPageUnits::Inches:
		return 72.0f;
	}

	return 1.0f;
}

static QString lcGetPageUnitSuffix(lcInstructionsPageUnits Units)
{
	switch (Units)
	{
	case lcInstructionsPageUnits::Pixels:
		return QCoreApplication::translate("lcPageSetupDialog", "px");

	case lcInstructionsPageUnits::Millimeters:
		return QCoreApplication::translate("lcPageSetupDialog", "mm");

	case lcInstructionsPageUnits::Centimeters:
		return QCoreApplication::translate("lcPageSetupDialog", "cm");

	case lcInstructionsPageUnits::Inches:
		return QCoreApplication::translate("lcPageSetupDialog", "in");
	}

	return QString();
}

lcPageSetupDialog::lcPageSetupDialog(QWidget* Parent, lcInstructionsPageSetup* PageSetup)
	: QDialog(Parent), ui(new Ui::lcPageSetupDialog), mPageSetup(PageSetup), mPoints(*PageSetup)
{
	ui->setupUi(this);

	const QStringList UnitNames = QStringList() << tr("Pixels") << tr("Millimeters") << tr("Centimeters") << tr("Inches");
	ui->UnitsComboBox->addItems(UnitNames);
	ui->MarginUnitsComboBox->addItems(UnitNames);

	ui->PresetComboBox->addItem(tr("Custom"), -1);

	for (int PresetIndex = 0; PresetIndex < static_cast<int>(LC_ARRAY_COUNT(gPageSetupPresets)); PresetIndex++)
	{
		const lcPageSetupPreset& Preset = gPageSetupPresets[PresetIndex];

		if (!Preset.Name)
		{
			ui->PresetComboBox->insertSeparator(ui->PresetComboBox->count());
			continue;
		}

		const int Decimals = Preset.Units == lcInstructionsPageUnits::Inches ? 2 : 0;
		const QString Size = tr("%1 x %2 %3").arg(QLocale().toString(Preset.Width, 'f', Decimals), QLocale().toString(Preset.Height, 'f', Decimals), lcGetPageUnitSuffix(Preset.Units));

		ui->PresetComboBox->addItem(tr("%1 (%2)").arg(tr(Preset.Name), Size), PresetIndex);
	}

	for (QDoubleSpinBox* SpinBox : { ui->WidthSpinBox, ui->HeightSpinBox, ui->LeftSpinBox, ui->RightSpinBox, ui->TopSpinBox, ui->BottomSpinBox })
		connect(SpinBox, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &lcPageSetupDialog::SizeEdited);

	auto GetProfileUnits = [](LC_PROFILE_KEY Key)
	{
		const int Units = lcGetProfileInt(Key);
		return Units >= 0 && Units <= static_cast<int>(lcInstructionsPageUnits::Inches) ? static_cast<lcInstructionsPageUnits>(Units) : lcInstructionsPageUnits::Inches;
	};

	mUnits = GetProfileUnits(LC_PROFILE_INSTRUCTIONS_PAGE_UNITS);
	mMarginUnits = GetProfileUnits(LC_PROFILE_INSTRUCTIONS_MARGIN_UNITS);
	ui->UnitsComboBox->setCurrentIndex(static_cast<int>(mUnits));
	ui->MarginUnitsComboBox->setCurrentIndex(static_cast<int>(mMarginUnits));

	UpdateSpinBoxes();
	UpdatePreset();
	UpdateOrientation();

	connect(ui->PresetComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, &lcPageSetupDialog::PresetChanged);
	connect(ui->UnitsComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, &lcPageSetupDialog::UnitsChanged);
	connect(ui->MarginUnitsComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, &lcPageSetupDialog::MarginUnitsChanged);
	connect(ui->PortraitButton, &QRadioButton::toggled, this, &lcPageSetupDialog::OrientationChanged);
}

lcPageSetupDialog::~lcPageSetupDialog()
{
	delete ui;
}

void lcPageSetupDialog::accept()
{
	if (mPoints.MarginLeft + mPoints.MarginRight >= mPoints.Width || mPoints.MarginTop + mPoints.MarginBottom >= mPoints.Height)
	{
		QMessageBox::information(this, tr("Page Setup"), tr("The margins must leave room for the page contents."));
		return;
	}

	*mPageSetup = mPoints;
	lcSetProfileInt(LC_PROFILE_INSTRUCTIONS_PAGE_UNITS, static_cast<int>(mUnits));
	lcSetProfileInt(LC_PROFILE_INSTRUCTIONS_MARGIN_UNITS, static_cast<int>(mMarginUnits));

	QDialog::accept();
}

void lcPageSetupDialog::UpdateSpinBoxes()
{
	auto SetValue = [](QDoubleSpinBox* SpinBox, lcInstructionsPageUnits Units, float Points, float Minimum)
	{
		const float Scale = lcGetPageUnitScale(Units);
		const int Decimals = Units == lcInstructionsPageUnits::Pixels ? 0 : Units == lcInstructionsPageUnits::Millimeters ? 1 : 2;
		const QString Suffix = QLatin1Char(' ') + lcGetPageUnitSuffix(Units);

		QSignalBlocker Blocker(SpinBox);
		SpinBox->setDecimals(Decimals);
		SpinBox->setSuffix(Suffix);
		SpinBox->setRange(Minimum / Scale, 100000.0 / Scale);
		SpinBox->setSingleStep(Decimals == 0 ? 10.0 : Decimals == 1 ? 1.0 : 0.1);
		SpinBox->setValue(Points / Scale);
	};

	SetValue(ui->WidthSpinBox, mUnits, mPoints.Width, 1.0f);
	SetValue(ui->HeightSpinBox, mUnits, mPoints.Height, 1.0f);
	SetValue(ui->LeftSpinBox, mMarginUnits, mPoints.MarginLeft, 0.0f);
	SetValue(ui->RightSpinBox, mMarginUnits, mPoints.MarginRight, 0.0f);
	SetValue(ui->TopSpinBox, mMarginUnits, mPoints.MarginTop, 0.0f);
	SetValue(ui->BottomSpinBox, mMarginUnits, mPoints.MarginBottom, 0.0f);
}

void lcPageSetupDialog::UpdatePreset()
{
	int Match = 0;

	for (int Index = 1; Index < ui->PresetComboBox->count(); Index++)
	{
		const int PresetIndex = ui->PresetComboBox->itemData(Index).toInt();

		if (PresetIndex < 0 || !ui->PresetComboBox->itemData(Index).isValid())
			continue;

		const lcPageSetupPreset& Preset = gPageSetupPresets[PresetIndex];
		const float Scale = lcGetPageUnitScale(Preset.Units);
		const float Width = Preset.Width * Scale, Height = Preset.Height * Scale;

		if ((qAbs(mPoints.Width - Width) < 0.5f && qAbs(mPoints.Height - Height) < 0.5f) || (qAbs(mPoints.Width - Height) < 0.5f && qAbs(mPoints.Height - Width) < 0.5f))
		{
			Match = Index;
			break;
		}
	}

	QSignalBlocker Blocker(ui->PresetComboBox);
	ui->PresetComboBox->setCurrentIndex(Match);
}

void lcPageSetupDialog::UpdateOrientation()
{
	QSignalBlocker PortraitBlocker(ui->PortraitButton);
	QSignalBlocker LandscapeBlocker(ui->LandscapeButton);

	if (mPoints.Width > mPoints.Height)
		ui->LandscapeButton->setChecked(true);
	else
		ui->PortraitButton->setChecked(true);
}

void lcPageSetupDialog::PresetChanged(int Index)
{
	const QVariant Data = ui->PresetComboBox->itemData(Index);

	if (!Data.isValid() || Data.toInt() < 0)
		return;

	const lcPageSetupPreset& Preset = gPageSetupPresets[Data.toInt()];
	const float Scale = lcGetPageUnitScale(Preset.Units);
	const float Short = qMin(Preset.Width, Preset.Height) * Scale, Long = qMax(Preset.Width, Preset.Height) * Scale;
	const bool SwitchUnits = (Preset.Units == lcInstructionsPageUnits::Pixels) != (mUnits == lcInstructionsPageUnits::Pixels);

	// moving between paper and screen sizes starts from the preset's own orientation
	if (SwitchUnits)
	{
		mPoints.Width = Preset.Width * Scale;
		mPoints.Height = Preset.Height * Scale;
	}
	else
	{
		const bool Landscape = ui->LandscapeButton->isChecked();
		mPoints.Width = Landscape ? Long : Short;
		mPoints.Height = Landscape ? Short : Long;
	}

	// metric presets use metric units and imperial presets inches, keeping centimeters if already chosen
	auto GetPresetUnits = [&Preset](lcInstructionsPageUnits Units)
	{
		if (Preset.Units == lcInstructionsPageUnits::Millimeters && Units == lcInstructionsPageUnits::Centimeters)
			return Units;

		return Preset.Units;
	};

	mUnits = GetPresetUnits(mUnits);
	mMarginUnits = GetPresetUnits(mMarginUnits);

	QSignalBlocker UnitsBlocker(ui->UnitsComboBox);
	ui->UnitsComboBox->setCurrentIndex(static_cast<int>(mUnits));
	QSignalBlocker MarginUnitsBlocker(ui->MarginUnitsComboBox);
	ui->MarginUnitsComboBox->setCurrentIndex(static_cast<int>(mMarginUnits));

	UpdateSpinBoxes();
	UpdateOrientation();
}

void lcPageSetupDialog::UnitsChanged(int Index)
{
	mUnits = static_cast<lcInstructionsPageUnits>(Index);

	UpdateSpinBoxes();
}

void lcPageSetupDialog::MarginUnitsChanged(int Index)
{
	mMarginUnits = static_cast<lcInstructionsPageUnits>(Index);

	UpdateSpinBoxes();
}

void lcPageSetupDialog::OrientationChanged()
{
	const bool Landscape = ui->LandscapeButton->isChecked();

	if (Landscape == (mPoints.Width > mPoints.Height))
		return;

	std::swap(mPoints.Width, mPoints.Height);

	UpdateSpinBoxes();
}

void lcPageSetupDialog::SizeEdited()
{
	const float Scale = lcGetPageUnitScale(mUnits);
	const float MarginScale = lcGetPageUnitScale(mMarginUnits);

	mPoints.Width = ui->WidthSpinBox->value() * Scale;
	mPoints.Height = ui->HeightSpinBox->value() * Scale;
	mPoints.MarginLeft = ui->LeftSpinBox->value() * MarginScale;
	mPoints.MarginRight = ui->RightSpinBox->value() * MarginScale;
	mPoints.MarginTop = ui->TopSpinBox->value() * MarginScale;
	mPoints.MarginBottom = ui->BottomSpinBox->value() * MarginScale;

	UpdatePreset();
	UpdateOrientation();
}
