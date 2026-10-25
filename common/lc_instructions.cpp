#include "lc_global.h"
#include "lc_instructions.h"
#include "project.h"
#include "lc_model.h"
#include "piece.h"
#include "lc_piecedata.h"
#include "lc_profile.h"

const float lcInstructions::mDisplayDPI = 72.0f;

lcInstructions::lcInstructions(Project* Project)
	: mProject(Project)
{
	// Page sizes are stored in points, 1/72 inch, which is also one pixel of the page scene.
	mPageSetup.Width = lcGetProfileFloat(LC_PROFILE_INSTRUCTIONS_PAGE_WIDTH);
	mPageSetup.Height = lcGetProfileFloat(LC_PROFILE_INSTRUCTIONS_PAGE_HEIGHT);
	mPageSetup.MarginLeft = lcGetProfileFloat(LC_PROFILE_INSTRUCTIONS_MARGIN_LEFT);
	mPageSetup.MarginRight = lcGetProfileFloat(LC_PROFILE_INSTRUCTIONS_MARGIN_RIGHT);
	mPageSetup.MarginTop = lcGetProfileFloat(LC_PROFILE_INSTRUCTIONS_MARGIN_TOP);
	mPageSetup.MarginBottom = lcGetProfileFloat(LC_PROFILE_INSTRUCTIONS_MARGIN_BOTTOM);

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

	mStepProperties[static_cast<int>(lcInstructionsPropertyType::ShowStepNumber)].Value = true;
	mStepProperties[static_cast<int>(lcInstructionsPropertyType::ShowStepPLI)].Value = true;
	mStepProperties[static_cast<int>(lcInstructionsPropertyType::StepNumberFont)].Value = QFont("Arial", 72).toString();
	mStepProperties[static_cast<int>(lcInstructionsPropertyType::StepNumberColor)].Value = LC_RGBA(0, 0, 0, 255);
	mStepProperties[static_cast<int>(lcInstructionsPropertyType::StepBackgroundColor)].Value = LC_RGBA(255, 255, 255, 0);
	mStepProperties[static_cast<int>(lcInstructionsPropertyType::PLIBackgroundColor)].Value = LC_RGBA(255, 255, 255, 255);
	mStepProperties[static_cast<int>(lcInstructionsPropertyType::PLIFont)].Value = QFont("Arial", 16, QFont::Bold).toString();
	mStepProperties[static_cast<int>(lcInstructionsPropertyType::PLITextColor)].Value = LC_RGBA(0, 0, 0, 255);
	mStepProperties[static_cast<int>(lcInstructionsPropertyType::PLIBorderColor)].Value = LC_RGBA(0, 0, 0, 255);
	mStepProperties[static_cast<int>(lcInstructionsPropertyType::PLIBorderWidth)].Value = 0;
//	mStepProperties[static_cast<int>(lcInstructionsPropertyType::PLIBorderRound)].Value = true;
	mStepProperties[static_cast<int>(lcInstructionsPropertyType::PLIShowCounts)].Value = true;
	mStepProperties[static_cast<int>(lcInstructionsPropertyType::PLISortOrder)].Value = static_cast<int>(lcPartsListSort::Color);
	mStepProperties[static_cast<int>(lcInstructionsPropertyType::ShowStepDividers)].Value = true;
	mStepProperties[static_cast<int>(lcInstructionsPropertyType::StepDividerColor)].Value = LC_RGBA(0, 0, 0, 255);
	mStepProperties[static_cast<int>(lcInstructionsPropertyType::StepDividerWidth)].Value = 2;
	mStepProperties[static_cast<int>(lcInstructionsPropertyType::ShowBillOfMaterials)].Value = true;
	mStepProperties[static_cast<int>(lcInstructionsPropertyType::UseDocumentFont)].Value = false;
	mStepProperties[static_cast<int>(lcInstructionsPropertyType::DocumentFont)].Value = QFont("Arial", 16).toString();
	mStepProperties[static_cast<int>(lcInstructionsPropertyType::ShowCoverPage)].Value = true;
	mStepProperties[static_cast<int>(lcInstructionsPropertyType::CoverTitleFont)].Value = QFont("Arial", 48, QFont::Bold).toString();
	mStepProperties[static_cast<int>(lcInstructionsPropertyType::PageBackgroundColor)].Value = LC_RGBA(255, 255, 255, 255);

	static_assert(static_cast<int>(lcInstructionsPropertyType::Count) == 21, "Missing default property");

	CreatePages();
}

QString lcInstructions::GetPropertyLabel(lcInstructionsPropertyType Type)
{
	switch (Type)
	{
		case lcInstructionsPropertyType::ShowStepNumber:
			return tr("Show Step Number");
		case lcInstructionsPropertyType::ShowStepPLI:
			return tr("Show Pieces List");
		case lcInstructionsPropertyType::StepNumberFont:
			return tr("Font:");
		case lcInstructionsPropertyType::StepNumberColor:
			return tr("Text Color:");
		case lcInstructionsPropertyType::StepBackgroundColor:
			return tr("Background Color:");
		case lcInstructionsPropertyType::PLIBackgroundColor:
			return tr("Background Color:");
		case lcInstructionsPropertyType::PLIFont:
			return tr("Font:");
		case lcInstructionsPropertyType::PLITextColor:
			return tr("Text Color:");
		case lcInstructionsPropertyType::PLIBorderColor:
			return tr("Border Color:");
		case lcInstructionsPropertyType::PLIBorderWidth:
			return tr("Border Width:");
		case lcInstructionsPropertyType::PLIShowCounts:
			return tr("Show Piece Counts");
		case lcInstructionsPropertyType::PLISortOrder:
			return tr("Sort By:");
		case lcInstructionsPropertyType::ShowStepDividers:
			return tr("Show Step Dividers");
		case lcInstructionsPropertyType::StepDividerColor:
			return tr("Divider Color:");
		case lcInstructionsPropertyType::StepDividerWidth:
			return tr("Divider Width:");
		case lcInstructionsPropertyType::ShowBillOfMaterials:
			return tr("Show Bill of Materials");
		case lcInstructionsPropertyType::UseDocumentFont:
			return tr("Use Document Font");
		case lcInstructionsPropertyType::DocumentFont:
			return tr("Font:");
		case lcInstructionsPropertyType::ShowCoverPage:
			return tr("Show Cover Page");
		case lcInstructionsPropertyType::CoverTitleFont:
			return tr("Title Font:");
		case lcInstructionsPropertyType::PageBackgroundColor:
			return tr("Background Color:");
		case lcInstructionsPropertyType::Count:
			break;
	}

	return QString();
}

void lcInstructions::SetDefaultPageSettings(const lcInstructionsPageSettings& PageSettings)
{
	mPageSettings = PageSettings;
	mPageSettings.Direction = GetPageDirection();
	mPageSettingsOverrides.clear();

	CreatePages();

	emit PageLayoutChanged();
}

lcInstructionsDirection lcInstructions::GetPageDirection() const
{
	// landscape pages fill rows across first, portrait pages fill columns down first
	return mPageSetup.Width > mPageSetup.Height ? lcInstructionsDirection::Horizontal : lcInstructionsDirection::Vertical;
}

bool lcInstructions::GetPageKey(size_t PageIndex, lcInstructionsStepKey& Key) const
{
	// pages are found by their first step so their settings follow them when other pages change
	if (PageIndex >= mPages.size())
	{
		lcModel* Model = mProject ? mProject->GetMainModel() : nullptr;

		if (!Model)
			return false;

		Key = { Model, Model->GetLastStep() + 1 };
		return true;
	}

	const lcInstructionsPage& Page = mPages[PageIndex];

	switch (Page.Type)
	{
	case lcInstructionsPageType::Steps:
		Key = { Page.Steps.front().Model, Page.Steps.front().Step };
		return true;

	case lcInstructionsPageType::Blank:
		Key = Page.BlankKey;
		return true;

	case lcInstructionsPageType::BillOfMaterials:
		Key = { Page.Model, Page.Model->GetLastStep() + 1 };
		return true;

	case lcInstructionsPageType::Cover:
		break;
	}

	return false;
}

lcInstructionsPageSettings lcInstructions::GetStepPageSettings(const lcInstructionsStepKey& Key) const
{
	std::map<lcInstructionsStepKey, lcInstructionsPageSettings>::const_iterator SettingsIt = mPageSettingsOverrides.find(Key);
	lcInstructionsPageSettings PageSettings = SettingsIt != mPageSettingsOverrides.end() ? SettingsIt->second : mPageSettings;

	PageSettings.Direction = GetPageDirection();

	return PageSettings;
}

lcInstructionsPageSettings lcInstructions::GetPageSettings(size_t PageIndex) const
{
	if (PageIndex < mPages.size() && mPages[PageIndex].Type == lcInstructionsPageType::Steps)
		return mPages[PageIndex].Settings;

	return GetStepPageSettings({ nullptr, 0 });
}

void lcInstructions::SetPageSettings(size_t PageIndex, int Rows, int Columns)
{
	// each page can hold its own number of steps, following pages flow on from it
	if (PageIndex >= mPages.size() || mPages[PageIndex].Type != lcInstructionsPageType::Steps)
		return;

	lcInstructionsPageSettings PageSettings = mPages[PageIndex].Settings;

	if (PageSettings.Rows == Rows && PageSettings.Columns == Columns)
		return;

	lcInstructionsStepKey Key;
	GetPageKey(PageIndex, Key);

	PageSettings.Rows = Rows;
	PageSettings.Columns = Columns;
	mPageSettingsOverrides[Key] = PageSettings;

	CreatePages();

	emit PageLayoutChanged();
}

bool lcInstructions::CanInsertBlankPage(size_t PageIndex) const
{
	lcInstructionsStepKey Key;
	return GetPageKey(PageIndex, Key);
}

void lcInstructions::InsertBlankPage(size_t PageIndex)
{
	// the blank page goes before the page at this index and stays with its first step
	lcInstructionsStepKey Key;

	if (!GetPageKey(PageIndex, Key))
		return;

	mBlankPages.insert(Key);

	CreatePages();

	emit PageLayoutChanged();
}

void lcInstructions::RemoveBlankPage(size_t PageIndex)
{
	if (PageIndex >= mPages.size() || mPages[PageIndex].Type != lcInstructionsPageType::Blank)
		return;

	std::multiset<lcInstructionsStepKey>::iterator BlankIt = mBlankPages.find(mPages[PageIndex].BlankKey);

	if (BlankIt != mBlankPages.end())
		mBlankPages.erase(BlankIt);

	CreatePages();

	emit PageLayoutChanged();
}

void lcInstructions::Update()
{
	CreatePages();

	emit PageLayoutChanged();
}

void lcInstructions::SetPageSetup(const lcInstructionsPageSetup& PageSetup)
{
	mPageSetup = PageSetup;

	lcSetProfileFloat(LC_PROFILE_INSTRUCTIONS_PAGE_WIDTH, mPageSetup.Width);
	lcSetProfileFloat(LC_PROFILE_INSTRUCTIONS_PAGE_HEIGHT, mPageSetup.Height);
	lcSetProfileFloat(LC_PROFILE_INSTRUCTIONS_MARGIN_LEFT, mPageSetup.MarginLeft);
	lcSetProfileFloat(LC_PROFILE_INSTRUCTIONS_MARGIN_RIGHT, mPageSetup.MarginRight);
	lcSetProfileFloat(LC_PROFILE_INSTRUCTIONS_MARGIN_TOP, mPageSetup.MarginTop);
	lcSetProfileFloat(LC_PROFILE_INSTRUCTIONS_MARGIN_BOTTOM, mPageSetup.MarginBottom);

	mPageSettings.Direction = GetPageDirection();
	CreatePages();

	emit PageSetupChanged();
}

bool lcInstructions::GetBoolProperty(lcInstructionsPropertyType Type, lcModel* Model, lcStep Step) const
{
	QVariant Value = GetProperty(Type, Model, Step);
	return Value.toBool();
}

int lcInstructions::GetIntProperty(lcInstructionsPropertyType Type, lcModel* Model, lcStep Step) const
{
	QVariant Value = GetProperty(Type, Model, Step);
	return Value.toInt();
}

QColor lcInstructions::GetColorProperty(lcInstructionsPropertyType Type, lcModel* Model, lcStep Step) const
{
	QVariant Value = GetProperty(Type, Model, Step);
	return lcQColorFromRGBA(Value.toUInt());
}

QFont lcInstructions::GetFontProperty(lcInstructionsPropertyType Type, lcModel* Model, lcStep Step) const
{
	// the document font replaces every element font to keep the instructions uniform
	if (Type != lcInstructionsPropertyType::DocumentFont && GetBoolProperty(lcInstructionsPropertyType::UseDocumentFont, Model, Step))
		Type = lcInstructionsPropertyType::DocumentFont;

	QVariant Value = GetProperty(Type, Model, Step);
	QFont Font;
	Font.fromString(Value.toString());
	return Font;
}

QVariant lcInstructions::GetProperty(lcInstructionsPropertyType Type, lcModel* Model, lcStep Step) const
{
	QVariant Value = mStepProperties[static_cast<int>(Type)].Value;

	std::map<lcModel*, lcInstructionsModel>::const_iterator ModelIt = mModels.find(Model);

	if (ModelIt == mModels.end())
		return Value;

	const lcInstructionsModel& InstructionModel = ModelIt->second;

	for (lcStep StepIndex = 0; StepIndex <= Step; StepIndex++)
	{
		const lcInstructionsProperties& Properties = InstructionModel.StepProperties[StepIndex];
		const lcInstructionsProperty& Property = Properties[static_cast<int>(Type)];

		if (Property.Mode == lcInstructionsPropertyMode::NotSet || (Property.Mode == lcInstructionsPropertyMode::StepOnly && StepIndex != Step))
			continue;

		Value = Property.Value;
	}

	return Value;
}

void lcInstructions::SetDefaultBool(lcInstructionsPropertyType Type, bool Enabled)
{
	SetDefaultProperty(Type, Enabled);
}

void lcInstructions::SetDefaultInt(lcInstructionsPropertyType Type, int Value)
{
	SetDefaultProperty(Type, Value);
}

void lcInstructions::SetDefaultColor(lcInstructionsPropertyType Type, const QColor& Color)
{
	SetDefaultProperty(Type, lcRGBAFromQColor(Color));
}

void lcInstructions::SetDefaultFont(lcInstructionsPropertyType Type, const QFont& Font)
{
	SetDefaultProperty(Type, Font.toString());
}

void lcInstructions::SetDefaultProperty(lcInstructionsPropertyType Type, const QVariant& Value)
{
	lcInstructionsProperty& Property = mStepProperties[static_cast<int>(Type)];

	if (Property.Value == Value)
		return;

	Property.Value = Value;

	// dividers and the bill of materials change the page layout instead of a single step
	switch (Type)
	{
	case lcInstructionsPropertyType::ShowBillOfMaterials:
	case lcInstructionsPropertyType::ShowCoverPage:
		CreatePages();
		emit PageLayoutChanged();
		break;

	case lcInstructionsPropertyType::UseDocumentFont:
	case lcInstructionsPropertyType::DocumentFont:
	case lcInstructionsPropertyType::CoverTitleFont:
	case lcInstructionsPropertyType::PageBackgroundColor:
	case lcInstructionsPropertyType::ShowStepDividers:
	case lcInstructionsPropertyType::StepDividerColor:
	case lcInstructionsPropertyType::StepDividerWidth:
		emit PageLayoutChanged();
		break;

	default:
		emit StepSettingsChanged(nullptr, 0);
		break;
	}
}

void lcInstructions::CreatePages()
{
	mPages.clear();

	if (mProject)
	{
		std::vector<const lcModel*> AddedModels;

		lcModel* Model = mProject->GetMainModel();

		if (Model)
		{
			if (GetBoolProperty(lcInstructionsPropertyType::ShowCoverPage, nullptr, 0))
			{
				lcInstructionsPage Page;
				Page.Type = lcInstructionsPageType::Cover;
				Page.Model = Model;
				mPages.emplace_back(std::move(Page));
			}

			AddDefaultPages(Model, AddedModels);

			if (GetBoolProperty(lcInstructionsPropertyType::ShowBillOfMaterials, nullptr, 0))
			{
				lcInstructionsPage Page;
				Page.Type = lcInstructionsPageType::BillOfMaterials;
				Page.Model = Model;
				mPages.emplace_back(std::move(Page));
			}
		}
	}
}

void lcInstructions::AddDefaultPages(lcModel* Model, std::vector<const lcModel*>& AddedModels)
{
	if (std::find(AddedModels.begin(), AddedModels.end(), Model) != AddedModels.end())
		return;

	AddedModels.push_back(Model);

	const lcStep LastStep = Model->GetLastStep();
	lcInstructionsPage Page;
	int Row = 0, Column = 0;

	// Models keep their step settings and gain entries for steps added since.
	std::vector<lcInstructionsProperties>& StepProperties = mModels[Model].StepProperties;

	if (StepProperties.size() < LastStep + 1)
		StepProperties.resize(LastStep + 1);

	for (lcStep Step = 1; Step <= LastStep; Step++)
	{
		std::set<lcModel*> StepSubModels;

		for (const std::unique_ptr<lcPiece>& Piece : Model->GetPieces())
		{
			if (!Piece->IsHidden() && Piece->GetStepShow() == Step && Piece->mPieceData->IsModel())
			{
				lcModel* SubModel = Piece->mPieceData->GetModel();

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

			for (lcModel* SubModel : StepSubModels)
				AddDefaultPages(SubModel, AddedModels);
		}

		AddBlankPages(Page, { Model, Step });

		if (Page.Steps.empty())
		{
			Page.Settings = GetStepPageSettings({ Model, Step });
			Row = 0;
			Column = 0;
		}

		const lcInstructionsPageSettings PageSettings = Page.Settings;
		lcInstructionsStep InstructionsStep;

		InstructionsStep.Model = Model;
		InstructionsStep.Step = Step;

		const double Width = 1.0 / (double)PageSettings.Columns;
		const double Height = 1.0 / (double)PageSettings.Rows;
		InstructionsStep.Rect = QRectF(Column * Width, Row * Height, Width, Height);

		Page.Steps.push_back(std::move(InstructionsStep));

		if (PageSettings.Direction == lcInstructionsDirection::Horizontal)
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

void lcInstructions::AddBlankPages(lcInstructionsPage& Page, const lcInstructionsStepKey& Key)
{
	const size_t BlankCount = mBlankPages.count(Key);

	if (!BlankCount)
		return;

	// a blank page breaks the flow, the steps before it end their page
	if (!Page.Steps.empty())
	{
		mPages.emplace_back(std::move(Page));
		Page = lcInstructionsPage();
	}

	for (size_t BlankIndex = 0; BlankIndex < BlankCount; BlankIndex++)
	{
		lcInstructionsPage BlankPage;
		BlankPage.Type = lcInstructionsPageType::Blank;
		BlankPage.BlankKey = Key;
		mPages.emplace_back(std::move(BlankPage));
	}
}
