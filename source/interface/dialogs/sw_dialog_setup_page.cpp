#include "sw_global.h"
#include "sw_dialog_setup_page.h"
#include "ui_sw_dialog_setup_page.h"
#include "sw_instructions.h"
#include "sw_profile.h"

struct swPageSetupPreset
{
	const char* Name;
	float Width;
	float Height;
	swInstructionsPageUnits Units;
};

// paper sizes are portrait and screen sizes landscape, the orientation buttons swap them
static const swPageSetupPreset gPageSetupPresets[] =
{
	{ QT_TRANSLATE_NOOP("swPageSetupDialog", "A0"), 841.0f, 1189.0f, swInstructionsPageUnits::Millimeters },
	{ QT_TRANSLATE_NOOP("swPageSetupDialog", "A1"), 594.0f, 841.0f, swInstructionsPageUnits::Millimeters },
	{ QT_TRANSLATE_NOOP("swPageSetupDialog", "A2"), 420.0f, 594.0f, swInstructionsPageUnits::Millimeters },
	{ QT_TRANSLATE_NOOP("swPageSetupDialog", "A3"), 297.0f, 420.0f, swInstructionsPageUnits::Millimeters },
	{ QT_TRANSLATE_NOOP("swPageSetupDialog", "A4"), 210.0f, 297.0f, swInstructionsPageUnits::Millimeters },
	{ QT_TRANSLATE_NOOP("swPageSetupDialog", "A5"), 148.0f, 210.0f, swInstructionsPageUnits::Millimeters },
	{ QT_TRANSLATE_NOOP("swPageSetupDialog", "A6"), 105.0f, 148.0f, swInstructionsPageUnits::Millimeters },
	{ QT_TRANSLATE_NOOP("swPageSetupDialog", "B4"), 250.0f, 353.0f, swInstructionsPageUnits::Millimeters },
	{ QT_TRANSLATE_NOOP("swPageSetupDialog", "B5"), 176.0f, 250.0f, swInstructionsPageUnits::Millimeters },
	{ QT_TRANSLATE_NOOP("swPageSetupDialog", "B6"), 125.0f, 176.0f, swInstructionsPageUnits::Millimeters },
	{ QT_TRANSLATE_NOOP("swPageSetupDialog", "JIS B4"), 257.0f, 364.0f, swInstructionsPageUnits::Millimeters },
	{ QT_TRANSLATE_NOOP("swPageSetupDialog", "JIS B5"), 182.0f, 257.0f, swInstructionsPageUnits::Millimeters },
	{ nullptr, 0.0f, 0.0f, swInstructionsPageUnits::Pixels },
	{ QT_TRANSLATE_NOOP("swPageSetupDialog", "Letter"), 8.5f, 11.0f, swInstructionsPageUnits::Inches },
	{ QT_TRANSLATE_NOOP("swPageSetupDialog", "Legal"), 8.5f, 14.0f, swInstructionsPageUnits::Inches },
	{ QT_TRANSLATE_NOOP("swPageSetupDialog", "Tabloid"), 11.0f, 17.0f, swInstructionsPageUnits::Inches },
	{ QT_TRANSLATE_NOOP("swPageSetupDialog", "Executive"), 7.25f, 10.5f, swInstructionsPageUnits::Inches },
	{ QT_TRANSLATE_NOOP("swPageSetupDialog", "Statement"), 5.5f, 8.5f, swInstructionsPageUnits::Inches },
	{ QT_TRANSLATE_NOOP("swPageSetupDialog", "ANSI C"), 17.0f, 22.0f, swInstructionsPageUnits::Inches },
	{ nullptr, 0.0f, 0.0f, swInstructionsPageUnits::Pixels },
	{ QT_TRANSLATE_NOOP("swPageSetupDialog", "XGA"), 1024.0f, 768.0f, swInstructionsPageUnits::Pixels },
	{ QT_TRANSLATE_NOOP("swPageSetupDialog", "HD"), 1280.0f, 720.0f, swInstructionsPageUnits::Pixels },
	{ QT_TRANSLATE_NOOP("swPageSetupDialog", "WXGA"), 1366.0f, 768.0f, swInstructionsPageUnits::Pixels },
	{ QT_TRANSLATE_NOOP("swPageSetupDialog", "UXGA"), 1600.0f, 1200.0f, swInstructionsPageUnits::Pixels },
	{ QT_TRANSLATE_NOOP("swPageSetupDialog", "Full HD"), 1920.0f, 1080.0f, swInstructionsPageUnits::Pixels },
	{ QT_TRANSLATE_NOOP("swPageSetupDialog", "WUXGA"), 1920.0f, 1200.0f, swInstructionsPageUnits::Pixels },
	{ QT_TRANSLATE_NOOP("swPageSetupDialog", "Tablet"), 2048.0f, 1536.0f, swInstructionsPageUnits::Pixels },
	{ QT_TRANSLATE_NOOP("swPageSetupDialog", "QHD"), 2560.0f, 1440.0f, swInstructionsPageUnits::Pixels },
	{ QT_TRANSLATE_NOOP("swPageSetupDialog", "4K UHD"), 3840.0f, 2160.0f, swInstructionsPageUnits::Pixels },
};

swPageSetupDialog::swPageSetupDialog(QWidget* Parent, swInstructionsPageSetup* PageSetup, bool Defaults)
	: QDialog(Parent), ui(new Ui::swPageSetupDialog), mPageSetup(PageSetup), mPoints(*PageSetup)
{
	// the preferences edit the default page setup with its own units
	mUnitsKey = Defaults ? SW_PROFILE_INSTRUCTIONS_DEFAULT_PAGE_UNITS : SW_PROFILE_INSTRUCTIONS_PAGE_UNITS;
	mMarginUnitsKey = Defaults ? SW_PROFILE_INSTRUCTIONS_DEFAULT_MARGIN_UNITS : SW_PROFILE_INSTRUCTIONS_MARGIN_UNITS;

	ui->setupUi(this);

	const QStringList UnitNames = QStringList() << tr("Pixels") << tr("Millimeters") << tr("Centimeters") << tr("Inches");
	ui->UnitsComboBox->addItems(UnitNames);
	ui->MarginUnitsComboBox->addItems(UnitNames);

	ui->PresetComboBox->addItem(tr("Custom"), -1);

	for (int PresetIndex = 0; PresetIndex < static_cast<int>(SW_ARRAY_COUNT(gPageSetupPresets)); PresetIndex++)
	{
		const swPageSetupPreset& Preset = gPageSetupPresets[PresetIndex];

		if (!Preset.Name)
		{
			ui->PresetComboBox->insertSeparator(ui->PresetComboBox->count());
			continue;
		}

		const int Decimals = Preset.Units == swInstructionsPageUnits::Inches ? 2 : 0;
		const QString Size = tr("%1 x %2 %3").arg(QLocale().toString(Preset.Width, 'f', Decimals), QLocale().toString(Preset.Height, 'f', Decimals), swGetPageUnitSuffix(Preset.Units));

		ui->PresetComboBox->addItem(tr("%1 (%2)").arg(tr(Preset.Name), Size), PresetIndex);
	}

	// values apply once typed so a partly typed page size does not clamp the margins
	for (QDoubleSpinBox* SpinBox : { ui->WidthSpinBox, ui->HeightSpinBox, ui->LeftSpinBox, ui->RightSpinBox, ui->TopSpinBox, ui->BottomSpinBox })
	{
		SpinBox->setKeyboardTracking(false);
		connect(SpinBox, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &swPageSetupDialog::SizeEdited);
	}

	auto GetProfileUnits = [](SW_PROFILE_KEY Key)
	{
		const int Units = swGetProfileInteger(Key);
		return Units >= 0 && Units <= static_cast<int>(swInstructionsPageUnits::Inches) ? static_cast<swInstructionsPageUnits>(Units) : swInstructionsPageUnits::Inches;
	};

	mUnits = GetProfileUnits(mUnitsKey);
	mMarginUnits = GetProfileUnits(mMarginUnitsKey);
	ui->UnitsComboBox->setCurrentIndex(static_cast<int>(mUnits));
	ui->MarginUnitsComboBox->setCurrentIndex(static_cast<int>(mMarginUnits));

	UpdateSpinBoxes();
	UpdatePreset();
	UpdateOrientation();

	connect(ui->PresetComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, &swPageSetupDialog::PresetChanged);
	connect(ui->UnitsComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, &swPageSetupDialog::UnitsChanged);
	connect(ui->MarginUnitsComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, &swPageSetupDialog::MarginUnitsChanged);
	connect(ui->PortraitButton, &QRadioButton::toggled, this, &swPageSetupDialog::OrientationChanged);
}

swPageSetupDialog::~swPageSetupDialog()
{
	delete ui;
}

void swPageSetupDialog::accept()
{
	if (mPoints.MarginLeft + mPoints.MarginRight >= mPoints.Width || mPoints.MarginTop + mPoints.MarginBottom >= mPoints.Height)
	{
		QMessageBox::information(this, tr("Page Setup"), tr("The margins must leave room for the page contents."));
		return;
	}

	*mPageSetup = mPoints;
	swSetProfileInteger(mUnitsKey, static_cast<int>(mUnits));
	swSetProfileInteger(mMarginUnitsKey, static_cast<int>(mMarginUnits));

	QDialog::accept();
}

void swPageSetupDialog::UpdateSpinBoxes()
{
	auto SetUnits = [](swRangeDoubleSpinBox* SpinBox, swInstructionsPageUnits Units)
	{
		const int Decimals = Units == swInstructionsPageUnits::Pixels ? 0 : Units == swInstructionsPageUnits::Millimeters ? 1 : 2;

		QSignalBlocker Blocker(SpinBox);
		SpinBox->setDecimals(Decimals);
		SpinBox->setSingleStep(Decimals == 0 ? 10.0 : Decimals == 1 ? 1.0 : 0.1);
	};

	auto SetSize = [](swRangeDoubleSpinBox* SpinBox, swInstructionsPageUnits Units, float Points)
	{
		const float Scale = swGetPageUnitScale(Units);

		QSignalBlocker Blocker(SpinBox);
		SpinBox->setRange(1.0 / Scale, 100000.0 / Scale);
		SpinBox->setSuffix(QLatin1Char(' ') + swGetPageUnitSuffix(Units));
		SpinBox->setValue(Points / Scale);
	};

	for (swRangeDoubleSpinBox* SpinBox : { ui->WidthSpinBox, ui->HeightSpinBox })
		SetUnits(SpinBox, mUnits);

	for (swRangeDoubleSpinBox* SpinBox : { ui->LeftSpinBox, ui->RightSpinBox, ui->TopSpinBox, ui->BottomSpinBox })
		SetUnits(SpinBox, mMarginUnits);

	SetSize(ui->WidthSpinBox, mUnits, mPoints.Width);
	SetSize(ui->HeightSpinBox, mUnits, mPoints.Height);

	UpdateMargins();
}

// use to cap each margin to the room left by the page size and the opposite margin
void swPageSetupDialog::UpdateMargins()
{
	const float ContentMinimum = 1.0f;

	mPoints.MarginLeft = qBound(0.0f, mPoints.MarginLeft, qMax(mPoints.Width - ContentMinimum, 0.0f));
	mPoints.MarginRight = qBound(0.0f, mPoints.MarginRight, qMax(mPoints.Width - mPoints.MarginLeft - ContentMinimum, 0.0f));
	mPoints.MarginTop = qBound(0.0f, mPoints.MarginTop, qMax(mPoints.Height - ContentMinimum, 0.0f));
	mPoints.MarginBottom = qBound(0.0f, mPoints.MarginBottom, qMax(mPoints.Height - mPoints.MarginTop - ContentMinimum, 0.0f));

	auto SetMargin = [this, ContentMinimum](swRangeDoubleSpinBox* SpinBox, float Points, float Room)
	{
		const float Scale = swGetPageUnitScale(mMarginUnits);

		QSignalBlocker Blocker(SpinBox);
		SpinBox->setRange(0.0, qMax(Room - ContentMinimum, 0.0f) / Scale);
		SpinBox->SetReference(QLatin1Char(' ') + swGetPageUnitSuffix(mMarginUnits));
		SpinBox->setValue(Points / Scale);
	};

	SetMargin(ui->LeftSpinBox, mPoints.MarginLeft, mPoints.Width - mPoints.MarginRight);
	SetMargin(ui->RightSpinBox, mPoints.MarginRight, mPoints.Width - mPoints.MarginLeft);
	SetMargin(ui->TopSpinBox, mPoints.MarginTop, mPoints.Height - mPoints.MarginBottom);
	SetMargin(ui->BottomSpinBox, mPoints.MarginBottom, mPoints.Height - mPoints.MarginTop);
}

void swPageSetupDialog::UpdatePreset()
{
	int Match = 0;

	for (int Index = 1; Index < ui->PresetComboBox->count(); Index++)
	{
		const int PresetIndex = ui->PresetComboBox->itemData(Index).toInt();

		if (PresetIndex < 0 || !ui->PresetComboBox->itemData(Index).isValid())
			continue;

		const swPageSetupPreset& Preset = gPageSetupPresets[PresetIndex];
		const float Scale = swGetPageUnitScale(Preset.Units);
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

void swPageSetupDialog::UpdateOrientation()
{
	QSignalBlocker PortraitBlocker(ui->PortraitButton);
	QSignalBlocker LandscapeBlocker(ui->LandscapeButton);

	if (mPoints.Width > mPoints.Height)
		ui->LandscapeButton->setChecked(true);
	else
		ui->PortraitButton->setChecked(true);
}

void swPageSetupDialog::PresetChanged(int Index)
{
	const QVariant Data = ui->PresetComboBox->itemData(Index);

	if (!Data.isValid() || Data.toInt() < 0)
		return;

	const swPageSetupPreset& Preset = gPageSetupPresets[Data.toInt()];
	const float Scale = swGetPageUnitScale(Preset.Units);
	const float Short = qMin(Preset.Width, Preset.Height) * Scale, Long = qMax(Preset.Width, Preset.Height) * Scale;
	const bool SwitchUnits = (Preset.Units == swInstructionsPageUnits::Pixels) != (mUnits == swInstructionsPageUnits::Pixels);

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
	auto GetPresetUnits = [&Preset](swInstructionsPageUnits Units)
	{
		if (Preset.Units == swInstructionsPageUnits::Millimeters && Units == swInstructionsPageUnits::Centimeters)
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

void swPageSetupDialog::UnitsChanged(int Index)
{
	mUnits = static_cast<swInstructionsPageUnits>(Index);

	UpdateSpinBoxes();
}

void swPageSetupDialog::MarginUnitsChanged(int Index)
{
	mMarginUnits = static_cast<swInstructionsPageUnits>(Index);

	UpdateSpinBoxes();
}

void swPageSetupDialog::OrientationChanged()
{
	const bool Landscape = ui->LandscapeButton->isChecked();

	if (Landscape == (mPoints.Width > mPoints.Height))
		return;

	std::swap(mPoints.Width, mPoints.Height);

	UpdateSpinBoxes();
}

void swPageSetupDialog::SizeEdited()
{
	const float Scale = swGetPageUnitScale(mUnits);
	const float MarginScale = swGetPageUnitScale(mMarginUnits);

	mPoints.Width = ui->WidthSpinBox->value() * Scale;
	mPoints.Height = ui->HeightSpinBox->value() * Scale;
	mPoints.MarginLeft = ui->LeftSpinBox->value() * MarginScale;
	mPoints.MarginRight = ui->RightSpinBox->value() * MarginScale;
	mPoints.MarginTop = ui->TopSpinBox->value() * MarginScale;
	mPoints.MarginBottom = ui->BottomSpinBox->value() * MarginScale;

	UpdateMargins();
	UpdatePreset();
	UpdateOrientation();
}
