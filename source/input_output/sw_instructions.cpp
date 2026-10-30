#include "sw_global.h"
#include "sw_instructions.h"
#include "sw_project.h"
#include "sw_model.h"
#include "sw_object_part.h"
#include "sw_object_camera.h"
#include "sw_data_part.h"
#include "sw_profile.h"
#include "sw_window_main.h"
#include "sw_window_utilities.h"

const float swInstructions::mDisplayDPI = 72.0f;
const swInstructionsStepKey swInstructions::mCoverBlankKey = { nullptr, 0 };
const swInstructionsStepKey swInstructions::mBillOfMaterialsBlankKey = { nullptr, 1 };

float swGetPageUnitScale(swInstructionsPageUnits Units)
{
	switch (Units)
	{
	case swInstructionsPageUnits::Pixels:
		return 1.0f;

	case swInstructionsPageUnits::Millimeters:
		return 72.0f / 25.4f;

	case swInstructionsPageUnits::Centimeters:
		return 72.0f / 2.54f;

	case swInstructionsPageUnits::Inches:
		return 72.0f;
	}

	return 1.0f;
}

QString swGetPageUnitSuffix(swInstructionsPageUnits Units)
{
	switch (Units)
	{
	case swInstructionsPageUnits::Pixels:
		return QCoreApplication::translate("swInstructions", "px");

	case swInstructionsPageUnits::Millimeters:
		return QCoreApplication::translate("swInstructions", "mm");

	case swInstructionsPageUnits::Centimeters:
		return QCoreApplication::translate("swInstructions", "cm");

	case swInstructionsPageUnits::Inches:
		return QCoreApplication::translate("swInstructions", "in");
	}

	return QString();
}

swInstructionsBoxTypes swGetBoxTypes(swInstructionsBoxElement Element)
{
	switch (Element)
	{
	case swInstructionsBoxElement::StepImage:
		return { swInstructionsPropertyType::StepImageMargin, swInstructionsPropertyType::StepImagePadding, swInstructionsPropertyType::StepImageBorderWidth, swInstructionsPropertyType::StepImageBorderColor, swInstructionsPropertyType::StepImageBackgroundColor };

	case swInstructionsBoxElement::StepNumber:
		return { swInstructionsPropertyType::StepNumberMargin, swInstructionsPropertyType::StepNumberPadding, swInstructionsPropertyType::StepNumberBorderWidth, swInstructionsPropertyType::StepNumberBorderColor, swInstructionsPropertyType::StepNumberBackgroundColor };

	case swInstructionsBoxElement::PartsList:
		return { swInstructionsPropertyType::PLIMargin, swInstructionsPropertyType::PLIPadding, swInstructionsPropertyType::PLIBorderWidth, swInstructionsPropertyType::PLIBorderColor, swInstructionsPropertyType::PLIBackgroundColor };

	case swInstructionsBoxElement::PageNumber:
		return { swInstructionsPropertyType::PageNumberMargin, swInstructionsPropertyType::PageNumberPadding, swInstructionsPropertyType::PageNumberBorderWidth, swInstructionsPropertyType::PageNumberBorderColor, swInstructionsPropertyType::PageNumberBackgroundColor };

	case swInstructionsBoxElement::CoverTitle:
		return { swInstructionsPropertyType::CoverTitleMargin, swInstructionsPropertyType::CoverTitlePadding, swInstructionsPropertyType::CoverTitleBorderWidth, swInstructionsPropertyType::CoverTitleBorderColor, swInstructionsPropertyType::CoverTitleBackgroundColor };

	case swInstructionsBoxElement::CoverAuthor:
		return { swInstructionsPropertyType::CoverAuthorMargin, swInstructionsPropertyType::CoverAuthorPadding, swInstructionsPropertyType::CoverAuthorBorderWidth, swInstructionsPropertyType::CoverAuthorBorderColor, swInstructionsPropertyType::CoverAuthorBackgroundColor };

	case swInstructionsBoxElement::CoverImage:
		return { swInstructionsPropertyType::CoverImageMargin, swInstructionsPropertyType::CoverImagePadding, swInstructionsPropertyType::CoverImageBorderWidth, swInstructionsPropertyType::CoverImageBorderColor, swInstructionsPropertyType::CoverImageBackgroundColor };

	case swInstructionsBoxElement::Notice:
	case swInstructionsBoxElement::Count:
		break;
	}

	return { swInstructionsPropertyType::NoticeMargin, swInstructionsPropertyType::NoticePadding, swInstructionsPropertyType::NoticeBorderWidth, swInstructionsPropertyType::NoticeBorderColor, swInstructionsPropertyType::NoticeBackgroundColor };
}

swInstructions::swInstructions(Project* Project)
	: mProject(Project)
{
	// page sizes are in points
	mPageSetup.Width = swGetProfileFloat(SW_PROFILE_INSTRUCTIONS_PAGE_WIDTH);
	mPageSetup.Height = swGetProfileFloat(SW_PROFILE_INSTRUCTIONS_PAGE_HEIGHT);
	mPageSetup.MarginLeft = swGetProfileFloat(SW_PROFILE_INSTRUCTIONS_MARGIN_LEFT);
	mPageSetup.MarginRight = swGetProfileFloat(SW_PROFILE_INSTRUCTIONS_MARGIN_RIGHT);
	mPageSetup.MarginTop = swGetProfileFloat(SW_PROFILE_INSTRUCTIONS_MARGIN_TOP);
	mPageSetup.MarginBottom = swGetProfileFloat(SW_PROFILE_INSTRUCTIONS_MARGIN_BOTTOM);

	if (mPageSetup.Width < 1.0f || mPageSetup.Height < 1.0f || mPageSetup.MarginLeft + mPageSetup.MarginRight >= mPageSetup.Width || mPageSetup.MarginTop + mPageSetup.MarginBottom >= mPageSetup.Height)
	{
		mPageSetup.Width = 11.0f * mDisplayDPI;
		mPageSetup.Height = 8.5f * mDisplayDPI;
		mPageSetup.MarginLeft = 0.5 * mDisplayDPI;
		mPageSetup.MarginRight = 0.5 * mDisplayDPI;
		mPageSetup.MarginTop = 0.5 * mDisplayDPI;
		mPageSetup.MarginBottom = 0.5 * mDisplayDPI;
	}

	mPageSettings.Rows = 1;
	mPageSettings.Columns = 1;
	mPageSettings.Direction = GetPageDirection();

	mShowGrid = swGetProfileInteger(SW_PROFILE_INSTRUCTIONS_SHOW_GRID) != 0;
	mSnapToGrid = swGetProfileInteger(SW_PROFILE_INSTRUCTIONS_SNAP_TO_GRID) != 0;
	mGridSize = qBound(SW_GRID_SIZE_MINIMUM, swGetProfileFloat(SW_PROFILE_INSTRUCTIONS_GRID_SIZE), SW_GRID_SIZE_MAXIMUM);
	mGridDivisions = qBound(1, swGetProfileInteger(SW_PROFILE_INSTRUCTIONS_GRID_DIVISIONS), SW_GRID_DIVISIONS_MAXIMUM);
	mGridWithinMargins = swGetProfileInteger(SW_PROFILE_INSTRUCTIONS_GRID_WITHIN_MARGINS) != 0;
	mShowMargins = swGetProfileInteger(SW_PROFILE_INSTRUCTIONS_SHOW_MARGINS) != 0;
	mGridPrimaryColor = swQColorFromRGBA(swGetProfileUnsignedInteger(SW_PROFILE_INSTRUCTIONS_GRID_PRIMARY_COLOR));
	mGridSecondaryColor = swQColorFromRGBA(swGetProfileUnsignedInteger(SW_PROFILE_INSTRUCTIONS_GRID_SECONDARY_COLOR));
	mMarginColor = swQColorFromRGBA(swGetProfileUnsignedInteger(SW_PROFILE_INSTRUCTIONS_MARGIN_COLOR));

	SetDefaultProperties();
	mDefaultProperties = mStepProperties;
	LoadCommandLines();

	CreatePages();

	// keep main model commands as written
	swModel* MainModel = mProject ? mProject->GetMainModel() : nullptr;

	if (MainModel)
		MainModel->SetInstructionLines(GetCommandLines());

	connect(this, &swInstructions::PageLayoutChanged, this, &swInstructions::StoreCommandLines);
	connect(this, &swInstructions::StepSettingsChanged, this, &swInstructions::StoreCommandLines);
	connect(this, &swInstructions::UnitsChanged, this, &swInstructions::StoreCommandLines);
}

void swInstructions::SetProject(Project* Project)
{
	mProject = Project;

	// read the commands of the new project
	mPages.clear();
	mPageSettingsOverrides.clear();
	mPagePropertyOverrides.clear();
	mElementOffsets.clear();
	mLockedPositions.clear();
	mBlankPages.clear();
	mNotices.clear();
	mPointers.clear();
	mModels.clear();
	mNextNoticeId = 1;
	mNextPointerId = 1;

	mPageSettings.Rows = 1;
	mPageSettings.Columns = 1;
	mPageSettings.Direction = GetPageDirection();

	SetDefaultProperties();
	mDefaultProperties = mStepProperties;
	LoadCommandLines();

	CreatePages();

	swModel* MainModel = mProject ? mProject->GetMainModel() : nullptr;

	if (MainModel)
		MainModel->SetInstructionLines(GetCommandLines());

	emit UnitsChanged();
	emit PageSetupChanged();
	emit PageLayoutChanged();
}

swInstructionsProperties swInstructions::CreateDefaultProperties()
{
	swInstructionsProperties Properties;

	Properties[static_cast<int>(swInstructionsPropertyType::ShowStepNumber)].Value = true;
	Properties[static_cast<int>(swInstructionsPropertyType::ShowStepPLI)].Value = true;
	Properties[static_cast<int>(swInstructionsPropertyType::StepNumberFont)].Value = QFont("Arial", 72).toString();
	Properties[static_cast<int>(swInstructionsPropertyType::StepNumberColor)].Value = SW_RGBA(0, 0, 0, 255);
	Properties[static_cast<int>(swInstructionsPropertyType::StepBackgroundColor)].Value = SW_RGBA(255, 255, 255, 0);
	Properties[static_cast<int>(swInstructionsPropertyType::PLIBackgroundColor)].Value = SW_RGBA(255, 255, 255, 255);
	Properties[static_cast<int>(swInstructionsPropertyType::PLIFont)].Value = QFont("Arial", 16).toString();
	Properties[static_cast<int>(swInstructionsPropertyType::PLITextColor)].Value = SW_RGBA(0, 0, 0, 255);
	Properties[static_cast<int>(swInstructionsPropertyType::PLIBorderColor)].Value = SW_RGBA(0, 0, 0, 255);
	Properties[static_cast<int>(swInstructionsPropertyType::PLIBorderWidth)].Value = 0;
	Properties[static_cast<int>(swInstructionsPropertyType::PLIShowCounts)].Value = true;
	Properties[static_cast<int>(swInstructionsPropertyType::PLISortOrder)].Value = static_cast<int>(swPartsListSort::Color);
	Properties[static_cast<int>(swInstructionsPropertyType::ShowStepDividers)].Value = true;
	Properties[static_cast<int>(swInstructionsPropertyType::StepDividerColor)].Value = SW_RGBA(0, 0, 0, 255);
	Properties[static_cast<int>(swInstructionsPropertyType::StepDividerWidth)].Value = 2;
	Properties[static_cast<int>(swInstructionsPropertyType::ShowBillOfMaterials)].Value = true;
	Properties[static_cast<int>(swInstructionsPropertyType::UseDocumentFont)].Value = false;
	Properties[static_cast<int>(swInstructionsPropertyType::DocumentFont)].Value = QFont("Arial", 16).toString();
	Properties[static_cast<int>(swInstructionsPropertyType::ShowCoverPage)].Value = true;
	Properties[static_cast<int>(swInstructionsPropertyType::CoverTitleFont)].Value = QFont("Arial", 48).toString();
	Properties[static_cast<int>(swInstructionsPropertyType::CoverTitleColor)].Value = SW_RGBA(0, 0, 0, 255);
	Properties[static_cast<int>(swInstructionsPropertyType::ShowCoverAuthor)].Value = true;
	Properties[static_cast<int>(swInstructionsPropertyType::PageBackgroundColor)].Value = SW_RGBA(255, 255, 255, 255);
	Properties[static_cast<int>(swInstructionsPropertyType::StepNumberPlacement)].Value = SW_PLACEMENT_GRID_SIZE + 1;
	Properties[static_cast<int>(swInstructionsPropertyType::StepNumberAnchor)].Value = static_cast<int>(swInstructionsPlacementAnchor::Area);
	Properties[static_cast<int>(swInstructionsPropertyType::PLIPlacement)].Value = SW_PLACEMENT_GRID_SIZE + SW_PLACEMENT_GRID_SIZE - 1;
	Properties[static_cast<int>(swInstructionsPropertyType::PLIAnchor)].Value = static_cast<int>(swInstructionsPlacementAnchor::StepNumber);
	Properties[static_cast<int>(swInstructionsPropertyType::CoverTitlePlacement)].Value = SW_PLACEMENT_GRID_SIZE + 2;
	Properties[static_cast<int>(swInstructionsPropertyType::BillOfMaterialsPlacement)].Value = SW_PLACEMENT_GRID_SIZE + 1;
	Properties[static_cast<int>(swInstructionsPropertyType::PLIThumbnailSize)].Value = 300;
	Properties[static_cast<int>(swInstructionsPropertyType::PLIThumbnailBackgroundColor)].Value = SW_RGBA(255, 255, 255, 0);
	Properties[static_cast<int>(swInstructionsPropertyType::CoverAuthorPlacement)].Value = SW_PLACEMENT_GRID_SIZE + 2;
	Properties[static_cast<int>(swInstructionsPropertyType::CoverImagePlacement)].Value = SW_PLACEMENT_COUNT / 2;
	Properties[static_cast<int>(swInstructionsPropertyType::StepImagePlacement)].Value = SW_PLACEMENT_COUNT / 2;
	Properties[static_cast<int>(swInstructionsPropertyType::CoverBackgroundColor)].Value = SW_RGBA(255, 255, 255, 255);
	Properties[static_cast<int>(swInstructionsPropertyType::BillOfMaterialsBackgroundColor)].Value = SW_RGBA(255, 255, 255, 255);
	Properties[static_cast<int>(swInstructionsPropertyType::ShowCoverBlankPage)].Value = true;
	Properties[static_cast<int>(swInstructionsPropertyType::ShowBillOfMaterialsBlankPage)].Value = true;
	Properties[static_cast<int>(swInstructionsPropertyType::UseDocumentUnits)].Value = true;
	Properties[static_cast<int>(swInstructionsPropertyType::DocumentUnits)].Value = swGetProfileInteger(SW_PROFILE_INSTRUCTIONS_PAGE_UNITS);
	Properties[static_cast<int>(swInstructionsPropertyType::ShowPageNumber)].Value = true;
	Properties[static_cast<int>(swInstructionsPropertyType::PageNumberFormat)].Value = static_cast<int>(swInstructionsPageNumberFormat::Number);
	Properties[static_cast<int>(swInstructionsPropertyType::PageNumberFont)].Value = QFont("Arial", 16).toString();
	Properties[static_cast<int>(swInstructionsPropertyType::PageNumberColor)].Value = SW_RGBA(0, 0, 0, 255);
	Properties[static_cast<int>(swInstructionsPropertyType::PageNumberPlacement)].Value = SW_PLACEMENT_COUNT - SW_PLACEMENT_GRID_SIZE - 2;
	Properties[static_cast<int>(swInstructionsPropertyType::ShowPageNumberAllPages)].Value = false;

	// boxes keep the former spacing
	for (int Element = 0; Element < static_cast<int>(swInstructionsBoxElement::Count); Element++)
	{
		const swInstructionsBoxTypes BoxTypes = swGetBoxTypes(static_cast<swInstructionsBoxElement>(Element));

		Properties[static_cast<int>(BoxTypes.Margin)].Value = 6;
		Properties[static_cast<int>(BoxTypes.Padding)].Value = 6;

		if (static_cast<swInstructionsBoxElement>(Element) == swInstructionsBoxElement::PartsList)
			continue;

		Properties[static_cast<int>(BoxTypes.BorderWidth)].Value = 0;
		Properties[static_cast<int>(BoxTypes.BorderColor)].Value = SW_RGBA(0, 0, 0, 255);
		Properties[static_cast<int>(BoxTypes.BackgroundColor)].Value = SW_RGBA(255, 255, 255, 0);
	}

	// frame notices
	Properties[static_cast<int>(swInstructionsPropertyType::NoticeFont)].Value = QFont("Arial", 16).toString();
	Properties[static_cast<int>(swInstructionsPropertyType::NoticeTextColor)].Value = SW_RGBA(0, 0, 0, 255);
	Properties[static_cast<int>(swInstructionsPropertyType::NoticeBorderWidth)].Value = 2;
	Properties[static_cast<int>(swInstructionsPropertyType::NoticeBackgroundColor)].Value = SW_RGBA(255, 255, 255, 255);
	Properties[static_cast<int>(swInstructionsPropertyType::PointerStyle)].Value = static_cast<int>(swInstructionsPointerStyle::Line);
	Properties[static_cast<int>(swInstructionsPropertyType::PointerTip)].Value = static_cast<int>(swInstructionsPointerTip::Arrowhead);
	Properties[static_cast<int>(swInstructionsPropertyType::PointerColor)].Value = SW_RGBA(0, 0, 0, 255);

	// pointers follow their element border
	Properties[static_cast<int>(swInstructionsPropertyType::PointerWidth)].Value = 0;
	Properties[static_cast<int>(swInstructionsPropertyType::PointerTipSize)].Value = 0;

	// parts stand in rows on their bottom
	Properties[static_cast<int>(swInstructionsPropertyType::PLILayout)].Value = static_cast<int>(swPartsListLayout::Rows);
	Properties[static_cast<int>(swInstructionsPropertyType::PLIAlignment)].Value = static_cast<int>(swPartsListAlignment::End);

	// step models render in perspective
	Properties[static_cast<int>(swInstructionsPropertyType::ModelProjection)].Value = static_cast<int>(swCameraProjection::Perspective);

	// outline new parts in yellow
	Properties[static_cast<int>(swInstructionsPropertyType::FadePreviousSteps)].Value = true;
	Properties[static_cast<int>(swInstructionsPropertyType::FadeColor)].Value = SW_RGBA(128, 128, 128, 217);
	Properties[static_cast<int>(swInstructionsPropertyType::HighlightNewParts)].Value = true;
	Properties[static_cast<int>(swInstructionsPropertyType::HighlightColor)].Value = SW_RGBA(255, 242, 0, 230);

	static_assert(static_cast<int>(swInstructionsPropertyType::Count) == 97, "Missing default property");

	return Properties;
}

void swInstructions::SetDefaultProperties()
{
	mStepProperties = CreateDefaultProperties();
}

void swInstructions::ResetProperties()
{
	// restore defaults and moved elements
	SetDefaultProperties();
	mPagePropertyOverrides.clear();
	mElementOffsets.clear();
	mLockedPositions.clear();
	CreatePages();

	emit PageLayoutChanged();
}

void swInstructions::SetDefaultPageSettings(const swInstructionsPageSettings& PageSettings)
{
	mPageSettings = PageSettings;
	mPageSettings.Direction = GetPageDirection();
	mPageSettingsOverrides.clear();

	CreatePages();

	emit PageLayoutChanged();
}

swInstructionsDirection swInstructions::GetPageDirection() const
{
	// landscape fills rows, portrait columns
	return mPageSetup.Width > mPageSetup.Height ? swInstructionsDirection::Horizontal : swInstructionsDirection::Vertical;
}

bool swInstructions::GetPageKey(size_t PageIndex, swInstructionsStepKey& Key) const
{
	// find pages by their first step
	if (PageIndex >= mPages.size())
	{
		swModel* Model = mProject ? mProject->GetMainModel() : nullptr;

		if (!Model)
			return false;

		Key = { Model, Model->GetLastStep() + 1 };
		return true;
	}

	const swInstructionsPage& Page = mPages[PageIndex];

	switch (Page.Type)
	{
	case swInstructionsPageType::Steps:
		Key = { Page.Steps.front().Model, Page.Steps.front().Step };
		return true;

	case swInstructionsPageType::Blank:
		Key = Page.BlankKey;
		return true;

	case swInstructionsPageType::BillOfMaterials:
		Key = { Page.Model, Page.Model->GetLastStep() + 1 };
		return true;

	case swInstructionsPageType::Cover:
		break;
	}

	return false;
}

swInstructionsPageSettings swInstructions::GetStepPageSettings(const swInstructionsStepKey& Key) const
{
	std::map<swInstructionsStepKey, swInstructionsPageSettings>::const_iterator SettingsIt = mPageSettingsOverrides.find(Key);
	swInstructionsPageSettings PageSettings = SettingsIt != mPageSettingsOverrides.end() ? SettingsIt->second : mPageSettings;

	PageSettings.Direction = GetPageDirection();

	return PageSettings;
}

swInstructionsPageSettings swInstructions::GetPageSettings(size_t PageIndex) const
{
	if (PageIndex < mPages.size() && mPages[PageIndex].Type == swInstructionsPageType::Steps)
		return mPages[PageIndex].Settings;

	return GetStepPageSettings({ nullptr, 0 });
}

void swInstructions::SetPageSettings(size_t PageIndex, int Rows, int Columns)
{
	// pages hold their own step count
	if (PageIndex >= mPages.size() || mPages[PageIndex].Type != swInstructionsPageType::Steps)
		return;

	swInstructionsPageSettings PageSettings = mPages[PageIndex].Settings;

	if (PageSettings.Rows == Rows && PageSettings.Columns == Columns)
		return;

	swInstructionsStepKey Key;
	GetPageKey(PageIndex, Key);

	PageSettings.Rows = Rows;
	PageSettings.Columns = Columns;
	mPageSettingsOverrides[Key] = PageSettings;

	CreatePages();

	emit PageLayoutChanged();
}

void swInstructions::SetPageDividerPreset(size_t PageIndex, int Rows, int Columns)
{
	// split pages show dividers
	swInstructionsProperty& Dividers = mStepProperties[static_cast<int>(swInstructionsPropertyType::ShowStepDividers)];
	const bool ShowDividers = Rows * Columns > 1 && !Dividers.Value.toBool();

	if (ShowDividers)
		Dividers.Value = true;

	const swInstructionsPageSettings PageSettings = GetPageSettings(PageIndex);

	if (PageSettings.Rows == Rows && PageSettings.Columns == Columns)
	{
		if (ShowDividers)
			emit PageLayoutChanged();

		return;
	}

	SetPageSettings(PageIndex, Rows, Columns);
}

bool swInstructions::CanInsertBlankPage(size_t PageIndex) const
{
	// default blank pages have no step
	swInstructionsStepKey Key;
	return GetPageKey(PageIndex, Key) && Key.first;
}

void swInstructions::InsertBlankPage(size_t PageIndex)
{
	// insert the blank page before this index
	swInstructionsStepKey Key;

	if (!GetPageKey(PageIndex, Key) || !Key.first)
		return;

	mBlankPages.insert(Key);

	CreatePages();

	emit PageLayoutChanged();
}

void swInstructions::RemoveBlankPage(size_t PageIndex)
{
	if (PageIndex >= mPages.size() || mPages[PageIndex].Type != swInstructionsPageType::Blank)
		return;

	// switch off default blank pages
	const swInstructionsStepKey& BlankKey = mPages[PageIndex].BlankKey;

	if (BlankKey == mCoverBlankKey)
	{
		SetDefaultBool(swInstructionsPropertyType::ShowCoverBlankPage, false);
		return;
	}

	if (BlankKey == mBillOfMaterialsBlankKey)
	{
		SetDefaultBool(swInstructionsPropertyType::ShowBillOfMaterialsBlankPage, false);
		return;
	}

	std::multiset<swInstructionsStepKey>::iterator BlankIt = mBlankPages.find(mPages[PageIndex].BlankKey);

	if (BlankIt != mBlankPages.end())
		mBlankPages.erase(BlankIt);

	CreatePages();

	emit PageLayoutChanged();
}

void swInstructions::Update()
{
	CreatePages();

	emit PageLayoutChanged();
}

void swInstructions::SetPageSetup(const swInstructionsPageSetup& PageSetup)
{
	mPageSetup = PageSetup;

	swSetProfileFloat(SW_PROFILE_INSTRUCTIONS_PAGE_WIDTH, mPageSetup.Width);
	swSetProfileFloat(SW_PROFILE_INSTRUCTIONS_PAGE_HEIGHT, mPageSetup.Height);
	swSetProfileFloat(SW_PROFILE_INSTRUCTIONS_MARGIN_LEFT, mPageSetup.MarginLeft);
	swSetProfileFloat(SW_PROFILE_INSTRUCTIONS_MARGIN_RIGHT, mPageSetup.MarginRight);
	swSetProfileFloat(SW_PROFILE_INSTRUCTIONS_MARGIN_TOP, mPageSetup.MarginTop);
	swSetProfileFloat(SW_PROFILE_INSTRUCTIONS_MARGIN_BOTTOM, mPageSetup.MarginBottom);

	mPageSettings.Direction = GetPageDirection();
	CreatePages();

	emit PageSetupChanged();
}

bool swInstructions::GetBoolProperty(swInstructionsPropertyType Type, swModel* Model, swStep Step) const
{
	QVariant Value = GetProperty(Type, Model, Step);
	return Value.toBool();
}

int swInstructions::GetIntProperty(swInstructionsPropertyType Type, swModel* Model, swStep Step) const
{
	QVariant Value = GetProperty(Type, Model, Step);
	return Value.toInt();
}

QColor swInstructions::GetColorProperty(swInstructionsPropertyType Type, swModel* Model, swStep Step) const
{
	QVariant Value = GetProperty(Type, Model, Step);
	return swQColorFromRGBA(Value.toUInt());
}

QFont swInstructions::GetFontProperty(swInstructionsPropertyType Type, swModel* Model, swStep Step) const
{
	// the document font replaces element fonts
	if (Type != swInstructionsPropertyType::DocumentFont && GetBoolProperty(swInstructionsPropertyType::UseDocumentFont, Model, Step))
		Type = swInstructionsPropertyType::DocumentFont;

	QVariant Value = GetProperty(Type, Model, Step);
	QFont Font;
	Font.fromString(Value.toString());
	return Font;
}

bool swInstructions::IsPageProperty(swInstructionsPropertyType Type)
{
	return Type == swInstructionsPropertyType::PLIThumbnailSize || Type == swInstructionsPropertyType::PLIThumbnailBackgroundColor || Type == swInstructionsPropertyType::PageBackgroundColor;
}

bool swInstructions::GetStepPageIndex(swModel* Model, swStep Step, size_t& PageIndex) const
{
	// step 0 is the whole model
	for (size_t Index = 0; Index < mPages.size(); Index++)
	{
		const swInstructionsPage& Page = mPages[Index];

		if (Step == 0 && Page.Type == swInstructionsPageType::BillOfMaterials && Page.Model == Model)
		{
			PageIndex = Index;
			return true;
		}

		for (const swInstructionsStep& PageStep : Page.Steps)
		{
			if (PageStep.Model == Model && PageStep.Step == Step)
			{
				PageIndex = Index;
				return true;
			}
		}
	}

	return false;
}

bool swInstructions::HasPageProperty(size_t PageIndex, swInstructionsPropertyType Type) const
{
	swInstructionsStepKey Key;

	if (!GetPageKey(PageIndex, Key) || PageIndex >= mPages.size())
		return false;

	std::map<swInstructionsStepKey, swInstructionsProperties>::const_iterator PropertiesIt = mPagePropertyOverrides.find(Key);

	return PropertiesIt != mPagePropertyOverrides.end() && PropertiesIt->second[static_cast<int>(Type)].Mode != swInstructionsPropertyMode::NotSet;
}

QVariant swInstructions::GetPageProperty(size_t PageIndex, swInstructionsPropertyType Type) const
{
	swInstructionsStepKey Key;

	if (HasPageProperty(PageIndex, Type) && GetPageKey(PageIndex, Key))
		return mPagePropertyOverrides.at(Key)[static_cast<int>(Type)].Value;

	return mStepProperties[static_cast<int>(Type)].Value;
}

int swInstructions::GetPageIntProperty(size_t PageIndex, swInstructionsPropertyType Type) const
{
	return GetPageProperty(PageIndex, Type).toInt();
}

QColor swInstructions::GetPageColorProperty(size_t PageIndex, swInstructionsPropertyType Type) const
{
	return swQColorFromRGBA(GetPageProperty(PageIndex, Type).toUInt());
}

QColor swInstructions::GetPageBackgroundColor(size_t PageIndex) const
{
	if (PageIndex < mPages.size())
	{
		switch (mPages[PageIndex].Type)
		{
		case swInstructionsPageType::Cover:
			return GetColorProperty(swInstructionsPropertyType::CoverBackgroundColor, nullptr, 0);

		case swInstructionsPageType::BillOfMaterials:
			return GetColorProperty(swInstructionsPropertyType::BillOfMaterialsBackgroundColor, nullptr, 0);

		case swInstructionsPageType::Steps:
		case swInstructionsPageType::Blank:
			break;
		}
	}

	return GetPageColorProperty(PageIndex, swInstructionsPropertyType::PageBackgroundColor);
}

void swInstructions::EmitPagePropertyChanged(swInstructionsPropertyType Type)
{
	// draw the background with the page
	if (Type == swInstructionsPropertyType::PageBackgroundColor)
		emit PageLayoutChanged();
	else
		emit StepSettingsChanged(nullptr, 0);
}

void swInstructions::SetPageScope(size_t PageIndex, swInstructionsPropertyType Type, bool PageOnly)
{
	swInstructionsStepKey Key;

	if (!IsPageProperty(Type) || PageIndex >= mPages.size() || !GetPageKey(PageIndex, Key) || HasPageProperty(PageIndex, Type) == PageOnly)
		return;

	// pages fall back to the all pages value
	swInstructionsProperty& Property = mPagePropertyOverrides[Key][static_cast<int>(Type)];

	if (PageOnly)
	{
		Property.Mode = swInstructionsPropertyMode::Default;
		Property.Value = mStepProperties[static_cast<int>(Type)].Value;
	}
	else
		Property = swInstructionsProperty();

	EmitPagePropertyChanged(Type);
}

void swInstructions::SetPageProperty(size_t PageIndex, swInstructionsPropertyType Type, const QVariant& Value)
{
	swInstructionsStepKey Key;

	if (!HasPageProperty(PageIndex, Type) || !GetPageKey(PageIndex, Key))
		return;

	swInstructionsProperty& Property = mPagePropertyOverrides[Key][static_cast<int>(Type)];

	if (Property.Value == Value)
		return;

	Property.Value = Value;

	EmitPagePropertyChanged(Type);
}

void swInstructions::SetPageInt(size_t PageIndex, swInstructionsPropertyType Type, int Value)
{
	SetPageProperty(PageIndex, Type, Value);
}

void swInstructions::SetPageColor(size_t PageIndex, swInstructionsPropertyType Type, const QColor& Color)
{
	SetPageProperty(PageIndex, Type, swRGBAFromQColor(Color));
}

QPointF swInstructions::GetElementOffset(const swInstructionsElementKey& Key) const
{
	const std::map<swInstructionsElementKey, QPointF>::const_iterator Offset = mElementOffsets.find(Key);

	if (Offset == mElementOffsets.end())
		return QPointF();

	return Offset->second;
}

bool swInstructions::HasElementOffset(const swInstructionsElementKey& Key) const
{
	return mElementOffsets.find(Key) != mElementOffsets.end();
}

void swInstructions::SetElementOffsets(const std::map<swInstructionsElementKey, QPointF>& Offsets)
{
	if (Offsets.empty())
		return;

	// drop the offset when back on placement
	for (const std::pair<const swInstructionsElementKey, QPointF>& Offset : Offsets)
	{
		if (Offset.second.isNull())
			mElementOffsets.erase(Offset.first);
		else
			mElementOffsets[Offset.first] = Offset.second;
	}

	emit PageLayoutChanged();
}

void swInstructions::ResetElementOffset(const swInstructionsElementKey& Key)
{
	if (mElementOffsets.erase(Key))
		emit PageLayoutChanged();
}

void swInstructions::SetElementPlacement(swInstructionsPropertyType Type, int Placement, const swInstructionsElementKey& Key)
{
	// a placed element drops its moved position
	const bool OffsetRemoved = mElementOffsets.erase(Key) > 0;

	if (mStepProperties[static_cast<int>(Type)].Value == QVariant(Placement))
	{
		if (OffsetRemoved)
			emit PageLayoutChanged();

		return;
	}

	SetDefaultProperty(Type, Placement);
}

swInstructionsBoxStyle swInstructions::GetBoxStyle(swInstructionsBoxElement Element, swModel* Model, swStep Step) const
{
	const swInstructionsBoxTypes BoxTypes = swGetBoxTypes(Element);
	swInstructionsBoxStyle BoxStyle;

	BoxStyle.Margin = GetIntProperty(BoxTypes.Margin, Model, Step);
	BoxStyle.Padding = GetIntProperty(BoxTypes.Padding, Model, Step);
	BoxStyle.BorderWidth = GetIntProperty(BoxTypes.BorderWidth, Model, Step);
	BoxStyle.BorderColor = GetColorProperty(BoxTypes.BorderColor, Model, Step);
	BoxStyle.BackgroundColor = GetColorProperty(BoxTypes.BackgroundColor, Model, Step);

	return BoxStyle;
}

swInstructionsPageKey swInstructions::GetPageElementsKey(size_t PageIndex) const
{
	if (PageIndex >= mPages.size())
		return { swInstructionsPageType::Blank, nullptr, 0, -1 };

	// tell pages apart by rank
	auto GetBaseKey = [this](size_t Index)
	{
		const swInstructionsPage& Page = mPages[Index];
		swInstructionsStepKey StepKey = { Page.Model, 0 };

		GetPageKey(Index, StepKey);

		return std::make_tuple(Page.Type, StepKey.first, StepKey.second);
	};

	const std::tuple<swInstructionsPageType, swModel*, swStep> BaseKey = GetBaseKey(PageIndex);
	int Rank = 0;

	for (size_t Index = 0; Index < PageIndex; Index++)
		if (GetBaseKey(Index) == BaseKey)
			Rank++;

	return { std::get<0>(BaseKey), std::get<1>(BaseKey), std::get<2>(BaseKey), Rank };
}

bool swInstructions::IsElementLocked(const swInstructionsElementKey& Key) const
{
	return mLockedPositions.find(Key) != mLockedPositions.end();
}

QPointF swInstructions::GetLockedPosition(const swInstructionsElementKey& Key) const
{
	const std::map<swInstructionsElementKey, QPointF>::const_iterator Position = mLockedPositions.find(Key);

	if (Position == mLockedPositions.end())
		return QPointF();

	return Position->second;
}

void swInstructions::LockElement(const swInstructionsElementKey& Key, const QPointF& Position)
{
	mLockedPositions[Key] = Position;

	emit PageLayoutChanged();
}

void swInstructions::UnlockElement(const swInstructionsElementKey& Key, const QPointF& Offset)
{
	if (!mLockedPositions.erase(Key))
		return;

	// keep the unlocked element in place
	if (Offset.isNull())
		mElementOffsets.erase(Key);
	else
		mElementOffsets[Key] = Offset;

	emit PageLayoutChanged();
}

// use to apply the preferences page defaults
void swInstructions::LoadDefaultPageSetup()
{
	const std::pair<SW_PROFILE_KEY, SW_PROFILE_KEY> FloatKeys[] =
	{
		{ SW_PROFILE_INSTRUCTIONS_PAGE_WIDTH, SW_PROFILE_INSTRUCTIONS_DEFAULT_PAGE_WIDTH },
		{ SW_PROFILE_INSTRUCTIONS_PAGE_HEIGHT, SW_PROFILE_INSTRUCTIONS_DEFAULT_PAGE_HEIGHT },
		{ SW_PROFILE_INSTRUCTIONS_MARGIN_LEFT, SW_PROFILE_INSTRUCTIONS_DEFAULT_MARGIN_LEFT },
		{ SW_PROFILE_INSTRUCTIONS_MARGIN_RIGHT, SW_PROFILE_INSTRUCTIONS_DEFAULT_MARGIN_RIGHT },
		{ SW_PROFILE_INSTRUCTIONS_MARGIN_TOP, SW_PROFILE_INSTRUCTIONS_DEFAULT_MARGIN_TOP },
		{ SW_PROFILE_INSTRUCTIONS_MARGIN_BOTTOM, SW_PROFILE_INSTRUCTIONS_DEFAULT_MARGIN_BOTTOM },
		{ SW_PROFILE_INSTRUCTIONS_GRID_SIZE, SW_PROFILE_INSTRUCTIONS_DEFAULT_GRID_SIZE }
	};

	const std::pair<SW_PROFILE_KEY, SW_PROFILE_KEY> IntegerKeys[] =
	{
		{ SW_PROFILE_INSTRUCTIONS_PAGE_UNITS, SW_PROFILE_INSTRUCTIONS_DEFAULT_PAGE_UNITS },
		{ SW_PROFILE_INSTRUCTIONS_MARGIN_UNITS, SW_PROFILE_INSTRUCTIONS_DEFAULT_MARGIN_UNITS },
		{ SW_PROFILE_INSTRUCTIONS_SHOW_GRID, SW_PROFILE_INSTRUCTIONS_DEFAULT_SHOW_GRID },
		{ SW_PROFILE_INSTRUCTIONS_SNAP_TO_GRID, SW_PROFILE_INSTRUCTIONS_DEFAULT_SNAP_TO_GRID },
		{ SW_PROFILE_INSTRUCTIONS_GRID_WITHIN_MARGINS, SW_PROFILE_INSTRUCTIONS_DEFAULT_GRID_WITHIN_MARGINS },
		{ SW_PROFILE_INSTRUCTIONS_GRID_DIVISIONS, SW_PROFILE_INSTRUCTIONS_DEFAULT_GRID_DIVISIONS }
	};

	for (const std::pair<SW_PROFILE_KEY, SW_PROFILE_KEY>& Keys : FloatKeys)
		swSetProfileFloat(Keys.first, swGetProfileFloat(Keys.second));

	for (const std::pair<SW_PROFILE_KEY, SW_PROFILE_KEY>& Keys : IntegerKeys)
		swSetProfileInteger(Keys.first, swGetProfileInteger(Keys.second));
}

// use to restore the preferences page defaults
void swInstructions::ResetPageSetup()
{
	LoadDefaultPageSetup();

	SetShowGrid(swGetProfileInteger(SW_PROFILE_INSTRUCTIONS_SHOW_GRID) != 0);
	SetSnapToGrid(swGetProfileInteger(SW_PROFILE_INSTRUCTIONS_SNAP_TO_GRID) != 0);
	SetGridSize(swGetProfileFloat(SW_PROFILE_INSTRUCTIONS_GRID_SIZE));
	SetGridDivisions(swGetProfileInteger(SW_PROFILE_INSTRUCTIONS_GRID_DIVISIONS));
	SetGridWithinMargins(swGetProfileInteger(SW_PROFILE_INSTRUCTIONS_GRID_WITHIN_MARGINS) != 0);

	// follow the paper units
	SetDefaultInt(swInstructionsPropertyType::DocumentUnits, swGetProfileInteger(SW_PROFILE_INSTRUCTIONS_PAGE_UNITS));
	SetDefaultBool(swInstructionsPropertyType::UseDocumentUnits, true);

	SetPageSetup({ swGetProfileFloat(SW_PROFILE_INSTRUCTIONS_PAGE_WIDTH), swGetProfileFloat(SW_PROFILE_INSTRUCTIONS_PAGE_HEIGHT), swGetProfileFloat(SW_PROFILE_INSTRUCTIONS_MARGIN_LEFT), swGetProfileFloat(SW_PROFILE_INSTRUCTIONS_MARGIN_RIGHT), swGetProfileFloat(SW_PROFILE_INSTRUCTIONS_MARGIN_TOP), swGetProfileFloat(SW_PROFILE_INSTRUCTIONS_MARGIN_BOTTOM) });
}

void swInstructions::SetShowGrid(bool ShowGrid)
{
	if (mShowGrid == ShowGrid)
		return;

	mShowGrid = ShowGrid;
	swSetProfileInteger(SW_PROFILE_INSTRUCTIONS_SHOW_GRID, mShowGrid);

	emit GridChanged();
}

void swInstructions::SetSnapToGrid(bool SnapToGrid)
{
	if (mSnapToGrid == SnapToGrid)
		return;

	mSnapToGrid = SnapToGrid;
	swSetProfileInteger(SW_PROFILE_INSTRUCTIONS_SNAP_TO_GRID, mSnapToGrid);

	emit GridChanged();
}

void swInstructions::SetGridSize(float GridSize)
{
	GridSize = qBound(SW_GRID_SIZE_MINIMUM, GridSize, SW_GRID_SIZE_MAXIMUM);

	if (qFuzzyCompare(mGridSize, GridSize))
		return;

	mGridSize = GridSize;
	swSetProfileFloat(SW_PROFILE_INSTRUCTIONS_GRID_SIZE, mGridSize);

	emit GridChanged();
}

void swInstructions::SetGridDivisions(int Divisions)
{
	Divisions = qBound(1, Divisions, SW_GRID_DIVISIONS_MAXIMUM);

	if (mGridDivisions == Divisions)
		return;

	mGridDivisions = Divisions;
	swSetProfileInteger(SW_PROFILE_INSTRUCTIONS_GRID_DIVISIONS, mGridDivisions);

	emit GridChanged();
}

void swInstructions::SetGridPrimaryColor(const QColor& Color)
{
	if (mGridPrimaryColor == Color)
		return;

	mGridPrimaryColor = Color;
	swSetProfileUnsignedInteger(SW_PROFILE_INSTRUCTIONS_GRID_PRIMARY_COLOR, swRGBAFromQColor(Color));

	emit GridChanged();
}

void swInstructions::SetGridSecondaryColor(const QColor& Color)
{
	if (mGridSecondaryColor == Color)
		return;

	mGridSecondaryColor = Color;
	swSetProfileUnsignedInteger(SW_PROFILE_INSTRUCTIONS_GRID_SECONDARY_COLOR, swRGBAFromQColor(Color));

	emit GridChanged();
}

void swInstructions::SetMarginColor(const QColor& Color)
{
	if (mMarginColor == Color)
		return;

	mMarginColor = Color;
	swSetProfileUnsignedInteger(SW_PROFILE_INSTRUCTIONS_MARGIN_COLOR, swRGBAFromQColor(Color));

	emit GridChanged();
}

// use to follow the preferences grid colors
void swInstructions::UpdateColors()
{
	SetGridPrimaryColor(swQColorFromRGBA(swGetProfileUnsignedInteger(SW_PROFILE_INSTRUCTIONS_GRID_PRIMARY_COLOR)));
	SetGridSecondaryColor(swQColorFromRGBA(swGetProfileUnsignedInteger(SW_PROFILE_INSTRUCTIONS_GRID_SECONDARY_COLOR)));
	SetMarginColor(swQColorFromRGBA(swGetProfileUnsignedInteger(SW_PROFILE_INSTRUCTIONS_MARGIN_COLOR)));
}

void swInstructions::SetGridWithinMargins(bool WithinMargins)
{
	if (mGridWithinMargins == WithinMargins)
		return;

	mGridWithinMargins = WithinMargins;
	swSetProfileInteger(SW_PROFILE_INSTRUCTIONS_GRID_WITHIN_MARGINS, mGridWithinMargins);

	emit GridChanged();
}

void swInstructions::SetShowMargins(bool ShowMargins)
{
	if (mShowMargins == ShowMargins)
		return;

	mShowMargins = ShowMargins;
	swSetProfileInteger(SW_PROFILE_INSTRUCTIONS_SHOW_MARGINS, mShowMargins);

	emit GridChanged();
}

QRectF swInstructions::GetGridRect() const
{
	if (mGridWithinMargins)
		return QRectF(mPageSetup.MarginLeft, mPageSetup.MarginTop, mPageSetup.Width - mPageSetup.MarginLeft - mPageSetup.MarginRight, mPageSetup.Height - mPageSetup.MarginTop - mPageSetup.MarginBottom);

	return QRectF(0.0, 0.0, mPageSetup.Width, mPageSetup.Height);
}

swInstructionsPageUnits swInstructions::GetPageUnits() const
{
	const int Units = swGetProfileInteger(SW_PROFILE_INSTRUCTIONS_MARGIN_UNITS);

	if (Units < 0 || Units > static_cast<int>(swInstructionsPageUnits::Inches))
		return swInstructionsPageUnits::Inches;

	return static_cast<swInstructionsPageUnits>(Units);
}

std::optional<swInstructionsPageUnits> swInstructions::GetDocumentUnits() const
{
	if (!GetBoolProperty(swInstructionsPropertyType::UseDocumentUnits, nullptr, 0))
		return std::nullopt;

	const int Units = GetIntProperty(swInstructionsPropertyType::DocumentUnits, nullptr, 0);

	if (Units < 0 || Units > static_cast<int>(swInstructionsPageUnits::Inches))
		return swInstructionsPageUnits::Millimeters;

	return static_cast<swInstructionsPageUnits>(Units);
}

QVariant swInstructions::GetProperty(swInstructionsPropertyType Type, swModel* Model, swStep Step) const
{
	size_t PageIndex;

	// a page value overrides its steps
	if (!mPagePropertyOverrides.empty() && IsPageProperty(Type) && GetStepPageIndex(Model, Step, PageIndex) && HasPageProperty(PageIndex, Type))
		return GetPageProperty(PageIndex, Type);

	QVariant Value = mStepProperties[static_cast<int>(Type)].Value;

	std::map<swModel*, swInstructionsModel>::const_iterator ModelIt = mModels.find(Model);

	if (ModelIt == mModels.end())
		return Value;

	const swInstructionsModel& InstructionModel = ModelIt->second;

	for (swStep StepIndex = 0; StepIndex <= Step; StepIndex++)
	{
		const swInstructionsProperties& Properties = InstructionModel.StepProperties[StepIndex];
		const swInstructionsProperty& Property = Properties[static_cast<int>(Type)];

		if (Property.Mode == swInstructionsPropertyMode::NotSet || (Property.Mode == swInstructionsPropertyMode::StepOnly && StepIndex != Step))
			continue;

		Value = Property.Value;
	}

	return Value;
}

void swInstructions::SetDefaultBool(swInstructionsPropertyType Type, bool Enabled)
{
	SetDefaultProperty(Type, Enabled);
}

void swInstructions::SetDefaultInt(swInstructionsPropertyType Type, int Value)
{
	SetDefaultProperty(Type, Value);
}

void swInstructions::SetDefaultColor(swInstructionsPropertyType Type, const QColor& Color)
{
	SetDefaultProperty(Type, swRGBAFromQColor(Color));
}

void swInstructions::SetDefaultFont(swInstructionsPropertyType Type, const QFont& Font)
{
	SetDefaultProperty(Type, Font.toString());
}

void swInstructions::SetDefaultProperty(swInstructionsPropertyType Type, const QVariant& Value)
{
	swInstructionsProperty& Property = mStepProperties[static_cast<int>(Type)];

	if (Property.Value == Value)
		return;

	Property.Value = Value;

	// dividers and lists change the layout
	switch (Type)
	{
	case swInstructionsPropertyType::ShowBillOfMaterials:
	case swInstructionsPropertyType::ShowCoverPage:
	case swInstructionsPropertyType::ShowCoverBlankPage:
	case swInstructionsPropertyType::ShowBillOfMaterialsBlankPage:
		CreatePages();
		emit PageLayoutChanged();
		break;

	case swInstructionsPropertyType::UseDocumentUnits:
	case swInstructionsPropertyType::DocumentUnits:
		emit UnitsChanged();
		break;

	case swInstructionsPropertyType::UseDocumentFont:
	case swInstructionsPropertyType::DocumentFont:
	case swInstructionsPropertyType::CoverTitleFont:
	case swInstructionsPropertyType::CoverTitleColor:
	case swInstructionsPropertyType::ShowCoverAuthor:
	case swInstructionsPropertyType::PageBackgroundColor:
	case swInstructionsPropertyType::CoverBackgroundColor:
	case swInstructionsPropertyType::BillOfMaterialsBackgroundColor:
	case swInstructionsPropertyType::ShowStepDividers:
	case swInstructionsPropertyType::StepDividerColor:
	case swInstructionsPropertyType::StepDividerWidth:
	case swInstructionsPropertyType::StepNumberPlacement:
	case swInstructionsPropertyType::StepNumberAnchor:
	case swInstructionsPropertyType::PLIPlacement:
	case swInstructionsPropertyType::PLIAnchor:
	case swInstructionsPropertyType::CoverTitlePlacement:
	case swInstructionsPropertyType::BillOfMaterialsPlacement:
	case swInstructionsPropertyType::CoverAuthorPlacement:
	case swInstructionsPropertyType::CoverImagePlacement:
	case swInstructionsPropertyType::StepImagePlacement:
	case swInstructionsPropertyType::StepBackgroundColor:
	case swInstructionsPropertyType::ShowPageNumber:
	case swInstructionsPropertyType::PageNumberFormat:
	case swInstructionsPropertyType::PageNumberFont:
	case swInstructionsPropertyType::PageNumberColor:
	case swInstructionsPropertyType::PageNumberPlacement:
	case swInstructionsPropertyType::ShowPageNumberAllPages:
	case swInstructionsPropertyType::PLIBackgroundColor:
	case swInstructionsPropertyType::PLIBorderColor:
	case swInstructionsPropertyType::PLIBorderWidth:
	case swInstructionsPropertyType::StepImageMargin:
	case swInstructionsPropertyType::StepImagePadding:
	case swInstructionsPropertyType::StepImageBorderWidth:
	case swInstructionsPropertyType::StepImageBorderColor:
	case swInstructionsPropertyType::StepImageBackgroundColor:
	case swInstructionsPropertyType::StepNumberMargin:
	case swInstructionsPropertyType::StepNumberPadding:
	case swInstructionsPropertyType::StepNumberBorderWidth:
	case swInstructionsPropertyType::StepNumberBorderColor:
	case swInstructionsPropertyType::StepNumberBackgroundColor:
	case swInstructionsPropertyType::PLIMargin:
	case swInstructionsPropertyType::PLIPadding:
	case swInstructionsPropertyType::PageNumberMargin:
	case swInstructionsPropertyType::PageNumberPadding:
	case swInstructionsPropertyType::PageNumberBorderWidth:
	case swInstructionsPropertyType::PageNumberBorderColor:
	case swInstructionsPropertyType::PageNumberBackgroundColor:
	case swInstructionsPropertyType::CoverTitleMargin:
	case swInstructionsPropertyType::CoverTitlePadding:
	case swInstructionsPropertyType::CoverTitleBorderWidth:
	case swInstructionsPropertyType::CoverTitleBorderColor:
	case swInstructionsPropertyType::CoverTitleBackgroundColor:
	case swInstructionsPropertyType::CoverAuthorMargin:
	case swInstructionsPropertyType::CoverAuthorPadding:
	case swInstructionsPropertyType::CoverAuthorBorderWidth:
	case swInstructionsPropertyType::CoverAuthorBorderColor:
	case swInstructionsPropertyType::CoverAuthorBackgroundColor:
	case swInstructionsPropertyType::CoverImageMargin:
	case swInstructionsPropertyType::CoverImagePadding:
	case swInstructionsPropertyType::CoverImageBorderWidth:
	case swInstructionsPropertyType::CoverImageBorderColor:
	case swInstructionsPropertyType::CoverImageBackgroundColor:
	case swInstructionsPropertyType::NoticeFont:
	case swInstructionsPropertyType::NoticeTextColor:
	case swInstructionsPropertyType::NoticeMargin:
	case swInstructionsPropertyType::NoticePadding:
	case swInstructionsPropertyType::NoticeBorderWidth:
	case swInstructionsPropertyType::NoticeBorderColor:
	case swInstructionsPropertyType::NoticeBackgroundColor:
	case swInstructionsPropertyType::PointerStyle:
	case swInstructionsPropertyType::PointerTip:
	case swInstructionsPropertyType::PointerColor:
	case swInstructionsPropertyType::PointerWidth:
	case swInstructionsPropertyType::PointerTipSize:
		emit PageLayoutChanged();
		break;

	default:
		emit StepSettingsChanged(nullptr, 0);
		break;
	}
}

void swInstructions::CreatePages()
{
	mPages.clear();

	if (mProject)
	{
		std::vector<const swModel*> AddedModels;

		swModel* Model = mProject->GetMainModel();

		if (Model)
		{
			if (GetBoolProperty(swInstructionsPropertyType::ShowCoverPage, nullptr, 0))
			{
				swInstructionsPage Page;
				Page.Type = swInstructionsPageType::Cover;
				Page.Model = Model;
				mPages.emplace_back(std::move(Page));

				if (GetBoolProperty(swInstructionsPropertyType::ShowCoverBlankPage, nullptr, 0))
					AddDefaultBlankPage(mCoverBlankKey);
			}

			AddDefaultPages(Model, AddedModels);

			if (GetBoolProperty(swInstructionsPropertyType::ShowBillOfMaterials, nullptr, 0))
			{
				if (GetBoolProperty(swInstructionsPropertyType::ShowBillOfMaterialsBlankPage, nullptr, 0))
					AddDefaultBlankPage(mBillOfMaterialsBlankKey);

				swInstructionsPage Page;
				Page.Type = swInstructionsPageType::BillOfMaterials;
				Page.Model = Model;
				mPages.emplace_back(std::move(Page));
			}
		}
	}
}

void swInstructions::AddDefaultPages(swModel* Model, std::vector<const swModel*>& AddedModels)
{
	if (std::find(AddedModels.begin(), AddedModels.end(), Model) != AddedModels.end())
		return;

	AddedModels.push_back(Model);

	const swStep LastStep = Model->GetLastStep();
	swInstructionsPage Page;
	int Row = 0, Column = 0;

	// add entries for new steps
	std::vector<swInstructionsProperties>& StepProperties = mModels[Model].StepProperties;

	if (StepProperties.size() < LastStep + 1)
		StepProperties.resize(LastStep + 1);

	for (swStep Step = 1; Step <= LastStep; Step++)
	{
		std::set<swModel*> StepSubModels;

		for (const std::unique_ptr<swPart>& Part : Model->GetParts())
		{
			if (!Part->IsHidden() && Part->GetStepShow() == Step && Part->mPartData->IsModel())
			{
				swModel* SubModel = Part->mPartData->GetModel();

				if (std::find(AddedModels.begin(), AddedModels.end(), SubModel) == AddedModels.end())
					StepSubModels.insert(SubModel);
			}
		}

		if (!StepSubModels.empty())
		{
			if (!Page.Steps.empty())
			{
				mPages.emplace_back(std::move(Page));
				Row = 0;
				Column = 0;
			}

			for (swModel* SubModel : StepSubModels)
				AddDefaultPages(SubModel, AddedModels);
		}

		AddBlankPages(Page, { Model, Step });

		if (Page.Steps.empty())
		{
			Page.Settings = GetStepPageSettings({ Model, Step });
			Row = 0;
			Column = 0;
		}

		const swInstructionsPageSettings PageSettings = Page.Settings;
		swInstructionsStep InstructionsStep;

		InstructionsStep.Model = Model;
		InstructionsStep.Step = Step;

		const double Width = 1.0 / (double)PageSettings.Columns;
		const double Height = 1.0 / (double)PageSettings.Rows;
		InstructionsStep.Rect = QRectF(Column * Width, Row * Height, Width, Height);

		Page.Steps.push_back(std::move(InstructionsStep));

		if (PageSettings.Direction == swInstructionsDirection::Horizontal)
		{
			Column++;

			if (Column == PageSettings.Columns)
			{
				Row++;
				Column = 0;
			}

			if (Row == PageSettings.Rows)
			{
				mPages.emplace_back(std::move(Page));
				Row = 0;
				Column = 0;
			}
		}
		else
		{
			Row++;

			if (Row == PageSettings.Rows)
			{
				Row = 0;
				Column++;
			}

			if (Column == PageSettings.Columns)
			{
				mPages.emplace_back(std::move(Page));
				Row = 0;
				Column = 0;
			}
		}
	}

	AddBlankPages(Page, { Model, LastStep + 1 });

	if (!Page.Steps.empty())
		mPages.emplace_back(std::move(Page));
}

void swInstructions::AddBlankPages(swInstructionsPage& Page, const swInstructionsStepKey& Key)
{
	const size_t BlankCount = mBlankPages.count(Key);

	if (!BlankCount)
		return;

	// a blank page ends the page before it
	if (!Page.Steps.empty())
	{
		mPages.emplace_back(std::move(Page));
		Page = swInstructionsPage();
	}

	for (size_t BlankIndex = 0; BlankIndex < BlankCount; BlankIndex++)
	{
		swInstructionsPage BlankPage;
		BlankPage.Type = swInstructionsPageType::Blank;
		BlankPage.BlankKey = Key;
		mPages.emplace_back(std::move(BlankPage));
	}
}

void swInstructions::AddDefaultBlankPage(const swInstructionsStepKey& Key)
{
	swInstructionsPage BlankPage;
	BlankPage.Type = swInstructionsPageType::Blank;
	BlankPage.BlankKey = Key;
	mPages.emplace_back(std::move(BlankPage));
}
