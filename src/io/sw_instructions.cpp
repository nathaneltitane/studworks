#include "sw_global.h"
#include "sw_instructions.h"
#include "project.h"
#include "sw_model.h"
#include "piece.h"
#include "sw_piecedata.h"
#include "sw_profile.h"

const float swInstructions::mDisplayDPI = 72.0f;

swInstructions::swInstructions(Project* Project)
	: mProject(Project)
{
	// Page sizes are stored in points, 1/72 inch, which is also one pixel of the page scene.
	mPageSetup.Width = swGetProfileFloat(SW_PROFILE_INSTRUCTIONS_PAGE_WIDTH);
	mPageSetup.Height = swGetProfileFloat(SW_PROFILE_INSTRUCTIONS_PAGE_HEIGHT);
	mPageSetup.MarginLeft = swGetProfileFloat(SW_PROFILE_INSTRUCTIONS_MARGIN_LEFT);
	mPageSetup.MarginRight = swGetProfileFloat(SW_PROFILE_INSTRUCTIONS_MARGIN_RIGHT);
	mPageSetup.MarginTop = swGetProfileFloat(SW_PROFILE_INSTRUCTIONS_MARGIN_TOP);
	mPageSetup.MarginBottom = swGetProfileFloat(SW_PROFILE_INSTRUCTIONS_MARGIN_BOTTOM);

	if (mPageSetup.Width < 1.0f || mPageSetup.Height < 1.0f || mPageSetup.MarginLeft + mPageSetup.MarginRight >= mPageSetup.Width || mPageSetup.MarginTop + mPageSetup.MarginBottom >= mPageSetup.Height)
	{
		mPageSetup.Width = 8.5f * mDisplayDPI;
		mPageSetup.Height = 11.0f * mDisplayDPI;
		mPageSetup.MarginLeft = 0.5 * mDisplayDPI;
		mPageSetup.MarginRight = 0.5 * mDisplayDPI;
		mPageSetup.MarginTop = 0.5 * mDisplayDPI;
		mPageSetup.MarginBottom = 0.5 * mDisplayDPI;
	}

	mPageSettings.Rows = 1;
	mPageSettings.Columns = 1;
	mPageSettings.Direction = GetPageDirection();

	mStepProperties[static_cast<int>(swInstructionsPropertyType::ShowStepNumber)].Value = true;
	mStepProperties[static_cast<int>(swInstructionsPropertyType::ShowStepPLI)].Value = true;
	mStepProperties[static_cast<int>(swInstructionsPropertyType::StepNumberFont)].Value = QFont("Arial", 72).toString();
	mStepProperties[static_cast<int>(swInstructionsPropertyType::StepNumberColor)].Value = SW_RGBA(0, 0, 0, 255);
	mStepProperties[static_cast<int>(swInstructionsPropertyType::StepBackgroundColor)].Value = SW_RGBA(255, 255, 255, 0);
	mStepProperties[static_cast<int>(swInstructionsPropertyType::PLIBackgroundColor)].Value = SW_RGBA(255, 255, 255, 255);
	mStepProperties[static_cast<int>(swInstructionsPropertyType::PLIFont)].Value = QFont("Arial", 16, QFont::Bold).toString();
	mStepProperties[static_cast<int>(swInstructionsPropertyType::PLITextColor)].Value = SW_RGBA(0, 0, 0, 255);
	mStepProperties[static_cast<int>(swInstructionsPropertyType::PLIBorderColor)].Value = SW_RGBA(0, 0, 0, 255);
	mStepProperties[static_cast<int>(swInstructionsPropertyType::PLIBorderWidth)].Value = 0;
//	mStepProperties[static_cast<int>(swInstructionsPropertyType::PLIBorderRound)].Value = true;
	mStepProperties[static_cast<int>(swInstructionsPropertyType::PLIShowCounts)].Value = true;
	mStepProperties[static_cast<int>(swInstructionsPropertyType::PLISortOrder)].Value = static_cast<int>(swPartsListSort::Color);
	mStepProperties[static_cast<int>(swInstructionsPropertyType::ShowStepDividers)].Value = true;
	mStepProperties[static_cast<int>(swInstructionsPropertyType::StepDividerColor)].Value = SW_RGBA(0, 0, 0, 255);
	mStepProperties[static_cast<int>(swInstructionsPropertyType::StepDividerWidth)].Value = 2;
	mStepProperties[static_cast<int>(swInstructionsPropertyType::ShowBillOfMaterials)].Value = true;
	mStepProperties[static_cast<int>(swInstructionsPropertyType::UseDocumentFont)].Value = false;
	mStepProperties[static_cast<int>(swInstructionsPropertyType::DocumentFont)].Value = QFont("Arial", 16).toString();
	mStepProperties[static_cast<int>(swInstructionsPropertyType::ShowCoverPage)].Value = true;
	mStepProperties[static_cast<int>(swInstructionsPropertyType::CoverTitleFont)].Value = QFont("Arial", 48, QFont::Bold).toString();
	mStepProperties[static_cast<int>(swInstructionsPropertyType::PageBackgroundColor)].Value = SW_RGBA(255, 255, 255, 255);

	static_assert(static_cast<int>(swInstructionsPropertyType::Count) == 21, "Missing default property");

	CreatePages();
}

QString swInstructions::GetPropertyLabel(swInstructionsPropertyType Type)
{
	switch (Type)
	{
		case swInstructionsPropertyType::ShowStepNumber:
			return tr("Show Step Number");
		case swInstructionsPropertyType::ShowStepPLI:
			return tr("Show Pieces List");
		case swInstructionsPropertyType::StepNumberFont:
			return tr("Font:");
		case swInstructionsPropertyType::StepNumberColor:
			return tr("Text Color:");
		case swInstructionsPropertyType::StepBackgroundColor:
			return tr("Background Color:");
		case swInstructionsPropertyType::PLIBackgroundColor:
			return tr("Background Color:");
		case swInstructionsPropertyType::PLIFont:
			return tr("Font:");
		case swInstructionsPropertyType::PLITextColor:
			return tr("Text Color:");
		case swInstructionsPropertyType::PLIBorderColor:
			return tr("Border Color:");
		case swInstructionsPropertyType::PLIBorderWidth:
			return tr("Border Width:");
		case swInstructionsPropertyType::PLIShowCounts:
			return tr("Show Piece Counts");
		case swInstructionsPropertyType::PLISortOrder:
			return tr("Sort By:");
		case swInstructionsPropertyType::ShowStepDividers:
			return tr("Show Step Dividers");
		case swInstructionsPropertyType::StepDividerColor:
			return tr("Divider Color:");
		case swInstructionsPropertyType::StepDividerWidth:
			return tr("Divider Width:");
		case swInstructionsPropertyType::ShowBillOfMaterials:
			return tr("Show Bill of Materials");
		case swInstructionsPropertyType::UseDocumentFont:
			return tr("Use Document Font");
		case swInstructionsPropertyType::DocumentFont:
			return tr("Font:");
		case swInstructionsPropertyType::ShowCoverPage:
			return tr("Show Cover Page");
		case swInstructionsPropertyType::CoverTitleFont:
			return tr("Title Font:");
		case swInstructionsPropertyType::PageBackgroundColor:
			return tr("Background Color:");
		case swInstructionsPropertyType::Count:
			break;
	}

	return QString();
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
	// landscape pages fill rows across first, portrait pages fill columns down first
	return mPageSetup.Width > mPageSetup.Height ? swInstructionsDirection::Horizontal : swInstructionsDirection::Vertical;
}

bool swInstructions::GetPageKey(size_t PageIndex, swInstructionsStepKey& Key) const
{
	// pages are found by their first step so their settings follow them when other pages change
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
	// each page can hold its own number of steps, following pages flow on from it
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

bool swInstructions::CanInsertBlankPage(size_t PageIndex) const
{
	swInstructionsStepKey Key;
	return GetPageKey(PageIndex, Key);
}

void swInstructions::InsertBlankPage(size_t PageIndex)
{
	// the blank page goes before the page at this index and stays with its first step
	swInstructionsStepKey Key;

	if (!GetPageKey(PageIndex, Key))
		return;

	mBlankPages.insert(Key);

	CreatePages();

	emit PageLayoutChanged();
}

void swInstructions::RemoveBlankPage(size_t PageIndex)
{
	if (PageIndex >= mPages.size() || mPages[PageIndex].Type != swInstructionsPageType::Blank)
		return;

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
	// the document font replaces every element font to keep the instructions uniform
	if (Type != swInstructionsPropertyType::DocumentFont && GetBoolProperty(swInstructionsPropertyType::UseDocumentFont, Model, Step))
		Type = swInstructionsPropertyType::DocumentFont;

	QVariant Value = GetProperty(Type, Model, Step);
	QFont Font;
	Font.fromString(Value.toString());
	return Font;
}

QVariant swInstructions::GetProperty(swInstructionsPropertyType Type, swModel* Model, swStep Step) const
{
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

	// dividers and the bill of materials change the page layout instead of a single step
	switch (Type)
	{
	case swInstructionsPropertyType::ShowBillOfMaterials:
	case swInstructionsPropertyType::ShowCoverPage:
		CreatePages();
		emit PageLayoutChanged();
		break;

	case swInstructionsPropertyType::UseDocumentFont:
	case swInstructionsPropertyType::DocumentFont:
	case swInstructionsPropertyType::CoverTitleFont:
	case swInstructionsPropertyType::PageBackgroundColor:
	case swInstructionsPropertyType::ShowStepDividers:
	case swInstructionsPropertyType::StepDividerColor:
	case swInstructionsPropertyType::StepDividerWidth:
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
			}

			AddDefaultPages(Model, AddedModels);

			if (GetBoolProperty(swInstructionsPropertyType::ShowBillOfMaterials, nullptr, 0))
			{
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

	// Models keep their step settings and gain entries for steps added since.
	std::vector<swInstructionsProperties>& StepProperties = mModels[Model].StepProperties;

	if (StepProperties.size() < LastStep + 1)
		StepProperties.resize(LastStep + 1);

	for (swStep Step = 1; Step <= LastStep; Step++)
	{
		std::set<swModel*> StepSubModels;

		for (const std::unique_ptr<swPiece>& Piece : Model->GetPieces())
		{
			if (!Piece->IsHidden() && Piece->GetStepShow() == Step && Piece->mPieceData->IsModel())
			{
				swModel* SubModel = Piece->mPieceData->GetModel();

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

	// a blank page breaks the flow, the steps before it end their page
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
