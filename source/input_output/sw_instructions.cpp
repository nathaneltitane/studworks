#include "sw_global.h"
#include "sw_instructions.h"
#include "sw_project.h"
#include "sw_model.h"
#include "sw_object_part.h"
#include "sw_data_part.h"
#include "sw_profile.h"
#include "sw_window_main.h"
#include "sw_window_utilities.h"

const float swInstructions::mDisplayDPI = 72.0f;
const swInstructionsStepKey swInstructions::mCoverBlankKey = { nullptr, 0 };
const swInstructionsStepKey swInstructions::mBillOfMaterialsBlankKey = { nullptr, 1 };

// the meta commands name the properties, elements and page types instead of numbering them
static const QLatin1String gInstructionsPropertyNames[] =
{
	QLatin1String("SHOW_STEP_NUMBER"), QLatin1String("SHOW_STEP_PARTS_LIST"), QLatin1String("STEP_NUMBER_FONT"),
	QLatin1String("STEP_NUMBER_COLOR"), QLatin1String("STEP_BACKGROUND_COLOR"), QLatin1String("PARTS_LIST_BACKGROUND_COLOR"),
	QLatin1String("PARTS_LIST_FONT"), QLatin1String("PARTS_LIST_TEXT_COLOR"), QLatin1String("PARTS_LIST_BORDER_COLOR"),
	QLatin1String("PARTS_LIST_BORDER_WIDTH"), QLatin1String("PARTS_LIST_SHOW_COUNTS"), QLatin1String("PARTS_LIST_SORT_ORDER"),
	QLatin1String("SHOW_STEP_DIVIDERS"), QLatin1String("STEP_DIVIDER_COLOR"), QLatin1String("STEP_DIVIDER_WIDTH"),
	QLatin1String("SHOW_BILL_OF_MATERIALS"), QLatin1String("USE_DOCUMENT_FONT"), QLatin1String("DOCUMENT_FONT"),
	QLatin1String("SHOW_COVER_PAGE"), QLatin1String("COVER_TITLE_FONT"), QLatin1String("COVER_TITLE_COLOR"),
	QLatin1String("SHOW_COVER_AUTHOR"), QLatin1String("PAGE_BACKGROUND_COLOR"), QLatin1String("STEP_NUMBER_PLACEMENT"),
	QLatin1String("STEP_NUMBER_ANCHOR"), QLatin1String("PARTS_LIST_PLACEMENT"), QLatin1String("PARTS_LIST_ANCHOR"),
	QLatin1String("COVER_TITLE_PLACEMENT"), QLatin1String("BILL_OF_MATERIALS_PLACEMENT"), QLatin1String("PARTS_LIST_THUMBNAIL_SIZE"),
	QLatin1String("PARTS_LIST_THUMBNAIL_BACKGROUND_COLOR"), QLatin1String("COVER_AUTHOR_PLACEMENT"), QLatin1String("COVER_IMAGE_PLACEMENT"),
	QLatin1String("STEP_IMAGE_PLACEMENT"), QLatin1String("COVER_BACKGROUND_COLOR"), QLatin1String("BILL_OF_MATERIALS_BACKGROUND_COLOR"),
	QLatin1String("SHOW_COVER_BLANK_PAGE"), QLatin1String("SHOW_BILL_OF_MATERIALS_BLANK_PAGE"), QLatin1String("USE_DOCUMENT_UNITS"),
	QLatin1String("DOCUMENT_UNITS"), QLatin1String("SHOW_PAGE_NUMBER"), QLatin1String("PAGE_NUMBER_FORMAT"),
	QLatin1String("PAGE_NUMBER_FONT"), QLatin1String("PAGE_NUMBER_COLOR"), QLatin1String("PAGE_NUMBER_PLACEMENT"),
	QLatin1String("SHOW_PAGE_NUMBER_ALL_PAGES"), QLatin1String("STEP_IMAGE_MARGIN"), QLatin1String("STEP_IMAGE_PADDING"),
	QLatin1String("STEP_IMAGE_BORDER_WIDTH"), QLatin1String("STEP_IMAGE_BORDER_COLOR"), QLatin1String("STEP_IMAGE_BACKGROUND_COLOR"),
	QLatin1String("STEP_NUMBER_MARGIN"), QLatin1String("STEP_NUMBER_PADDING"), QLatin1String("STEP_NUMBER_BORDER_WIDTH"),
	QLatin1String("STEP_NUMBER_BORDER_COLOR"), QLatin1String("STEP_NUMBER_BACKGROUND_COLOR"), QLatin1String("PARTS_LIST_MARGIN"),
	QLatin1String("PARTS_LIST_PADDING"), QLatin1String("PAGE_NUMBER_MARGIN"), QLatin1String("PAGE_NUMBER_PADDING"),
	QLatin1String("PAGE_NUMBER_BORDER_WIDTH"), QLatin1String("PAGE_NUMBER_BORDER_COLOR"), QLatin1String("PAGE_NUMBER_BACKGROUND_COLOR"),
	QLatin1String("COVER_TITLE_MARGIN"), QLatin1String("COVER_TITLE_PADDING"), QLatin1String("COVER_TITLE_BORDER_WIDTH"),
	QLatin1String("COVER_TITLE_BORDER_COLOR"), QLatin1String("COVER_TITLE_BACKGROUND_COLOR"), QLatin1String("COVER_AUTHOR_MARGIN"),
	QLatin1String("COVER_AUTHOR_PADDING"), QLatin1String("COVER_AUTHOR_BORDER_WIDTH"), QLatin1String("COVER_AUTHOR_BORDER_COLOR"),
	QLatin1String("COVER_AUTHOR_BACKGROUND_COLOR"), QLatin1String("COVER_IMAGE_MARGIN"), QLatin1String("COVER_IMAGE_PADDING"),
	QLatin1String("COVER_IMAGE_BORDER_WIDTH"), QLatin1String("COVER_IMAGE_BORDER_COLOR"), QLatin1String("COVER_IMAGE_BACKGROUND_COLOR"),
	QLatin1String("NOTICE_FONT"), QLatin1String("NOTICE_TEXT_COLOR"), QLatin1String("NOTICE_MARGIN"),
	QLatin1String("NOTICE_PADDING"), QLatin1String("NOTICE_BORDER_WIDTH"), QLatin1String("NOTICE_BORDER_COLOR"),
	QLatin1String("NOTICE_BACKGROUND_COLOR"), QLatin1String("POINTER_STYLE"), QLatin1String("POINTER_TIP"),
	QLatin1String("POINTER_COLOR"), QLatin1String("POINTER_WIDTH"), QLatin1String("POINTER_TIP_SIZE"),
	QLatin1String("PARTS_LIST_LAYOUT"), QLatin1String("PARTS_LIST_ALIGNMENT")
};

static_assert(std::size(gInstructionsPropertyNames) == static_cast<size_t>(swInstructionsPropertyType::Count), "Missing property name");

static const QLatin1String gInstructionsElementNames[] =
{
	QLatin1String("NONE"), QLatin1String("STEP_NUMBER"), QLatin1String("PARTS_LIST"), QLatin1String("COVER_TITLE"), QLatin1String("COVER_AUTHOR"), QLatin1String("BILL_OF_MATERIALS"),
	QLatin1String("STEP_IMAGE"), QLatin1String("COVER_IMAGE"), QLatin1String("PAGE_NUMBER"), QLatin1String("NOTICE"), QLatin1String("POINTER")
};

static const QLatin1String gInstructionsPageTypeNames[] =
{
	QLatin1String("STEPS"), QLatin1String("COVER"), QLatin1String("BILL_OF_MATERIALS"), QLatin1String("BLANK")
};

static const QLatin1String gInstructionsTargetNames[] =
{
	QLatin1String("POINT"), QLatin1String("ELEMENT"), QLatin1String("PART")
};

static const QLatin1String gInstructionsPointerStyleNames[] =
{
	QLatin1String("LINE"), QLatin1String("WEDGE")
};

static const QLatin1String gInstructionsPointerTipNames[] =
{
	QLatin1String("ARROWHEAD"), QLatin1String("DOT")
};

// a pointer without its own style or tip writes the default
static const QLatin1String gInstructionsDefaultName("DEFAULT");
static const QLatin1String gInstructionsAutoName("AUTO");

template<size_t Count>
static int swFindInstructionsName(const QLatin1String (&Names)[Count], const QString& Name)
{
	for (size_t Index = 0; Index < Count; Index++)
		if (Names[Index] == Name)
			return static_cast<int>(Index);

	return -1;
}

// use to keep free text on one line without breaking the quoted model names
static QString swEscapeInstructionsText(const QString& Text)
{
	QString Escaped;
	Escaped.reserve(Text.size());

	for (const QChar Character : Text)
	{
		if (Character == QLatin1Char('\\'))
			Escaped += QLatin1String("\\\\");
		else if (Character == QLatin1Char('"'))
			Escaped += QLatin1String("\\\"");
		else if (Character == QLatin1Char('\n'))
			Escaped += QLatin1String("\\n");
		else if (Character != QLatin1Char('\r'))
			Escaped += Character;
	}

	return Escaped;
}

static QString swUnescapeInstructionsText(const QString& Text)
{
	QString Unescaped;
	Unescaped.reserve(Text.size());

	for (int Position = 0; Position < Text.size(); Position++)
	{
		const QChar Character = Text.at(Position);

		if (Character != QLatin1Char('\\') || Position + 1 == Text.size())
		{
			Unescaped += Character;
			continue;
		}

		const QChar Escaped = Text.at(++Position);
		Unescaped += Escaped == QLatin1Char('n') ? QLatin1Char('\n') : Escaped;
	}

	return Unescaped;
}

static QString swWriteInstructionsModel(const swModel* Model)
{
	return Model ? QLatin1Char('"') + Model->GetFileName() + QLatin1Char('"') : QStringLiteral("-");
}

static QString swWriteInstructionsStepKey(const swInstructionsStepKey& Key)
{
	return swWriteInstructionsModel(Key.first) + QLatin1Char(' ') + QString::number(Key.second);
}

static QString swWriteInstructionsElementKey(const swInstructionsElementKey& Key)
{
	return gInstructionsElementNames[static_cast<int>(std::get<0>(Key))] + QLatin1Char(' ') + swWriteInstructionsModel(std::get<1>(Key)) + QLatin1Char(' ') + QString::number(std::get<2>(Key));
}

static QString swWriteInstructionsPageKey(const swInstructionsPageKey& Key)
{
	return gInstructionsPageTypeNames[static_cast<int>(std::get<0>(Key))] + QLatin1Char(' ') + swWriteInstructionsModel(std::get<1>(Key)) + QLatin1Char(' ') + QString::number(std::get<2>(Key)) + QLatin1Char(' ') + QString::number(std::get<3>(Key));
}

static QString swWriteInstructionsPoint(const QPointF& Point)
{
	return swFormatValue(Point.x(), 2) + QLatin1Char(' ') + swFormatValue(Point.y(), 2);
}

// the type of the default value tells how a property is written, colors as #RRGGBBAA
static QString swWriteInstructionsValue(const QVariant& Value, const QVariant& DefaultValue)
{
	switch (DefaultValue.userType())
	{
	case QMetaType::Bool:
		return Value.toBool() ? QStringLiteral("TRUE") : QStringLiteral("FALSE");

	case QMetaType::UInt:
	{
		const quint32 Color = Value.toUInt();
		return QString::asprintf("#%02X%02X%02X%02X", SW_RGBA_RED(Color), SW_RGBA_GREEN(Color), SW_RGBA_BLUE(Color), SW_RGBA_ALPHA(Color));
	}

	case QMetaType::Int:
		return QString::number(Value.toInt());

	default:
		return swEscapeInstructionsText(Value.toString());
	}
}

// use to read the fields of an instructions meta command in order, a missing or wrong field makes it invalid
class swInstructionsCommandReader
{
public:
	swInstructionsCommandReader(const QString& Line, const Project* Project)
		: mLine(Line), mProject(Project)
	{
	}

	bool IsValid() const
	{
		return mValid;
	}

	QString ReadToken()
	{
		SkipSpace();
		mQuoted = mPosition < mLine.size() && mLine.at(mPosition) == QLatin1Char('"');

		if (mQuoted)
		{
			const int End = mLine.indexOf(QLatin1Char('"'), mPosition + 1);

			if (End == -1)
			{
				mValid = false;
				mPosition = mLine.size();
				return QString();
			}

			const QString Token = mLine.mid(mPosition + 1, End - mPosition - 1);
			mPosition = End + 1;
			return Token;
		}

		const int Begin = mPosition;

		while (mPosition < mLine.size() && !mLine.at(mPosition).isSpace())
			mPosition++;

		if (Begin == mPosition)
			mValid = false;

		return mLine.mid(Begin, mPosition - Begin);
	}

	QString ReadRest()
	{
		SkipSpace();
		const QString Rest = mLine.mid(mPosition);
		mPosition = mLine.size();
		return Rest;
	}

	void ReadKeyword(const char* Keyword)
	{
		if (ReadToken() != QLatin1String(Keyword))
			mValid = false;
	}

	int ReadInt()
	{
		bool Ok = false;
		const int Value = ReadToken().toInt(&Ok);
		mValid &= Ok;
		return Value;
	}

	qreal ReadNumber()
	{
		bool Ok = false;
		const qreal Value = ReadToken().toDouble(&Ok);
		mValid &= Ok;
		return Value;
	}

	QPointF ReadPoint()
	{
		const qreal X = ReadNumber();
		return QPointF(X, ReadNumber());
	}

	template<size_t Count>
	int ReadName(const QLatin1String (&Names)[Count])
	{
		const int Index = swFindInstructionsName(Names, ReadToken());

		if (Index == -1)
			mValid = false;

		return qMax(Index, 0);
	}

	// the default and the automatic choice are written by name, the others by number
	int ReadChoice(const QLatin1String& Name, int Value)
	{
		const QString Token = ReadToken();

		if (Token == Name)
			return Value;

		bool Ok = false;
		const int Choice = Token.toInt(&Ok);
		mValid &= Ok;
		return Choice;
	}

	swModel* ReadModel()
	{
		const QString Token = ReadToken();

		if (!mQuoted && Token == QLatin1String("-"))
			return nullptr;

		swModel* Model = mQuoted && mProject ? mProject->GetModel(Token) : nullptr;

		if (!Model)
			mValid = false;

		return Model;
	}

	swInstructionsStepKey ReadStepKey()
	{
		swModel* Model = ReadModel();
		return { Model, static_cast<swStep>(ReadInt()) };
	}

	swInstructionsElementKey ReadElementKey()
	{
		const swInstructionsPlacementElement Element = static_cast<swInstructionsPlacementElement>(ReadName(gInstructionsElementNames));
		swModel* Model = ReadModel();
		return { Element, Model, static_cast<swStep>(ReadInt()) };
	}

	swInstructionsPageKey ReadPageKey()
	{
		const swInstructionsPageType Type = static_cast<swInstructionsPageType>(ReadName(gInstructionsPageTypeNames));
		swModel* Model = ReadModel();
		const swStep Step = static_cast<swStep>(ReadInt());
		return { Type, Model, Step, ReadInt() };
	}

	QVariant ReadValue(const QVariant& DefaultValue)
	{
		switch (DefaultValue.userType())
		{
		case QMetaType::Bool:
		{
			const QString Token = ReadToken();

			if (Token != QLatin1String("TRUE") && Token != QLatin1String("FALSE"))
				mValid = false;

			return Token == QLatin1String("TRUE");
		}

		case QMetaType::UInt:
		{
			const QString Token = ReadToken();
			bool Ok = false;
			const quint32 Color = Token.mid(1).toUInt(&Ok, 16);

			if (!Ok || Token.size() != 9 || !Token.startsWith(QLatin1Char('#')))
			{
				mValid = false;
				return DefaultValue;
			}

			return SW_RGBA((Color >> 24) & 0xff, (Color >> 16) & 0xff, (Color >> 8) & 0xff, Color & 0xff);
		}

		case QMetaType::Int:
			return ReadInt();

		default:
			return swUnescapeInstructionsText(ReadRest());
		}
	}

protected:
	void SkipSpace()
	{
		while (mPosition < mLine.size() && mLine.at(mPosition).isSpace())
			mPosition++;
	}

	QString mLine;
	const Project* mProject;
	int mPosition = 0;
	bool mQuoted = false;
	bool mValid = true;
};

void swRenameInstructionsModels(QStringList& CommandLines, const std::map<QString, QString>& Names)
{
	// every quoted name is a model, free text escapes its quotes
	for (QString& CommandLine : CommandLines)
	{
		QString Renamed;
		int Position = 0;

		while (Position < CommandLine.size())
		{
			const QChar Character = CommandLine.at(Position);

			if (Character == QLatin1Char('\\'))
			{
				Renamed += CommandLine.mid(Position, 2);
				Position += 2;
				continue;
			}

			const int End = Character == QLatin1Char('"') ? CommandLine.indexOf(QLatin1Char('"'), Position + 1) : -1;

			if (End == -1)
			{
				Renamed += Character;
				Position++;
				continue;
			}

			const QString Name = CommandLine.mid(Position + 1, End - Position - 1);
			const std::map<QString, QString>::const_iterator NameIt = Names.find(Name);

			Renamed += QLatin1Char('"') + (NameIt != Names.end() ? NameIt->second : Name) + QLatin1Char('"');
			Position = End + 1;
		}

		CommandLine = Renamed;
	}
}

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
	// Page sizes are stored in points, 1/72 inch, which is also one pixel of the page scene.
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

	// the main model keeps the commands as they are written so opening the instructions changes nothing
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

	// the settings of the old project are dropped and the commands of the new one are read
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

void swInstructions::LoadCommandLines()
{
	swModel* MainModel = mProject ? mProject->GetMainModel() : nullptr;

	if (!MainModel)
		return;

	// a command naming a missing model or holding a wrong field is left out
	for (const QString& CommandLine : MainModel->GetInstructionLines())
	{
		swInstructionsCommandReader Reader(CommandLine, mProject);
		const QString Command = Reader.ReadToken();

		if (Command == QLatin1String("PROPERTY") || Command == QLatin1String("PAGE_PROPERTY"))
		{
			const bool PageProperty = Command == QLatin1String("PAGE_PROPERTY");
			swInstructionsStepKey Key;

			if (PageProperty)
			{
				Reader.ReadKeyword("STEP");
				Key = Reader.ReadStepKey();
			}

			const int Type = Reader.ReadName(gInstructionsPropertyNames);

			if (!Reader.IsValid() || (PageProperty && !IsPageProperty(static_cast<swInstructionsPropertyType>(Type))))
				continue;

			const QVariant Value = Reader.ReadValue(mDefaultProperties[Type].Value);

			if (!Reader.IsValid())
				continue;

			if (!PageProperty)
				mStepProperties[Type].Value = Value;
			else
			{
				swInstructionsProperty& Property = mPagePropertyOverrides[Key][Type];
				Property.Mode = swInstructionsPropertyMode::Default;
				Property.Value = Value;
			}
		}
		else if (Command == QLatin1String("STEPS_PER_PAGE") || Command == QLatin1String("PAGE_STEPS_PER_PAGE"))
		{
			const bool PageSettings = Command == QLatin1String("PAGE_STEPS_PER_PAGE");
			swInstructionsStepKey Key;

			if (PageSettings)
			{
				Reader.ReadKeyword("STEP");
				Key = Reader.ReadStepKey();
			}

			swInstructionsPageSettings Settings = mPageSettings;
			Settings.Rows = Reader.ReadInt();
			Settings.Columns = Reader.ReadInt();

			if (!Reader.IsValid() || Settings.Rows < 1 || Settings.Columns < 1)
				continue;

			if (PageSettings)
				mPageSettingsOverrides[Key] = Settings;
			else
				mPageSettings = Settings;
		}
		else if (Command == QLatin1String("BLANK_PAGE"))
		{
			Reader.ReadKeyword("STEP");
			const swInstructionsStepKey Key = Reader.ReadStepKey();

			if (Reader.IsValid() && Key.first)
				mBlankPages.insert(Key);
		}
		else if (Command == QLatin1String("OFFSET") || Command == QLatin1String("LOCK"))
		{
			const swInstructionsElementKey Key = Reader.ReadElementKey();
			const QPointF Position = Reader.ReadPoint();

			if (!Reader.IsValid())
				continue;

			if (Command == QLatin1String("OFFSET"))
				mElementOffsets[Key] = Position;
			else
				mLockedPositions[Key] = Position;
		}
		else if (Command == QLatin1String("NOTICE"))
		{
			swInstructionsNotice Notice;

			Notice.Id = Reader.ReadInt();
			Reader.ReadKeyword("PAGE");
			Notice.PageKey = Reader.ReadPageKey();
			Reader.ReadKeyword("POSITION");
			Notice.Position = Reader.ReadPoint();
			Reader.ReadKeyword("TEXT");
			Notice.Text = swUnescapeInstructionsText(Reader.ReadRest());

			if (!Reader.IsValid() || Notice.Id < 1 || FindNotice(Notice.Id))
				continue;

			mNotices.push_back(Notice);
			mNextNoticeId = qMax(mNextNoticeId, Notice.Id + 1);
		}
		else if (Command == QLatin1String("POINTER"))
		{
			swInstructionsPointer Pointer;

			Pointer.Id = Reader.ReadInt();
			Reader.ReadKeyword("PAGE");
			Pointer.PageKey = Reader.ReadPageKey();
			Reader.ReadKeyword("SOURCE");
			Pointer.Source = Reader.ReadElementKey();
			Reader.ReadKeyword("ANCHOR");
			Pointer.Anchor = Reader.ReadChoice(gInstructionsAutoName, SW_POINTER_ANCHOR_AUTO);
			Reader.ReadKeyword("STYLE");
			const QString Style = Reader.ReadToken();
			Reader.ReadKeyword("TIP");
			const QString Tip = Reader.ReadToken();
			Reader.ReadKeyword("TARGET");
			Pointer.TargetType = static_cast<swInstructionsPointerTarget>(Reader.ReadName(gInstructionsTargetNames));
			Pointer.TargetElement = Reader.ReadElementKey();
			Reader.ReadKeyword("PART");
			Pointer.TargetPart = Reader.ReadInt();
			Reader.ReadKeyword("POSITION");
			Pointer.TipPosition = Reader.ReadPoint();

			Pointer.Style = Style == gInstructionsDefaultName ? -1 : swFindInstructionsName(gInstructionsPointerStyleNames, Style);
			Pointer.Tip = Tip == gInstructionsDefaultName ? -1 : swFindInstructionsName(gInstructionsPointerTipNames, Tip);

			if (!Reader.IsValid() || Pointer.Id < 1 || FindPointer(Pointer.Id) || Pointer.Anchor < SW_POINTER_ANCHOR_AUTO || Pointer.Anchor >= SW_POINTER_ANCHOR_COUNT)
				continue;

			mPointers.push_back(Pointer);
			mNextPointerId = qMax(mNextPointerId, Pointer.Id + 1);
		}
	}
}

QStringList swInstructions::GetCommandLines() const
{
	QStringList CommandLines;

	// only the properties changed from their defaults are written
	for (int Type = 0; Type < static_cast<int>(swInstructionsPropertyType::Count); Type++)
	{
		const QVariant& DefaultValue = mDefaultProperties[Type].Value;
		const QVariant& Value = mStepProperties[Type].Value;

		if (Value.isValid() && Value != DefaultValue)
			CommandLines.append(QLatin1String("PROPERTY ") + gInstructionsPropertyNames[Type] + QLatin1Char(' ') + swWriteInstructionsValue(Value, DefaultValue));
	}

	for (const std::pair<const swInstructionsStepKey, swInstructionsProperties>& PageProperties : mPagePropertyOverrides)
	{
		for (int Type = 0; Type < static_cast<int>(swInstructionsPropertyType::Count); Type++)
		{
			const swInstructionsProperty& Property = PageProperties.second[Type];

			if (Property.Mode != swInstructionsPropertyMode::NotSet)
				CommandLines.append(QLatin1String("PAGE_PROPERTY STEP ") + swWriteInstructionsStepKey(PageProperties.first) + QLatin1Char(' ') + gInstructionsPropertyNames[Type] + QLatin1Char(' ') + swWriteInstructionsValue(Property.Value, mDefaultProperties[Type].Value));
		}
	}

	if (mPageSettings.Rows != 1 || mPageSettings.Columns != 1)
		CommandLines.append(QStringLiteral("STEPS_PER_PAGE %1 %2").arg(mPageSettings.Rows).arg(mPageSettings.Columns));

	for (const std::pair<const swInstructionsStepKey, swInstructionsPageSettings>& PageSettings : mPageSettingsOverrides)
		CommandLines.append(QLatin1String("PAGE_STEPS_PER_PAGE STEP ") + swWriteInstructionsStepKey(PageSettings.first) + QStringLiteral(" %1 %2").arg(PageSettings.second.Rows).arg(PageSettings.second.Columns));

	for (const swInstructionsStepKey& BlankPage : mBlankPages)
		CommandLines.append(QLatin1String("BLANK_PAGE STEP ") + swWriteInstructionsStepKey(BlankPage));

	for (const std::pair<const swInstructionsElementKey, QPointF>& Offset : mElementOffsets)
		CommandLines.append(QLatin1String("OFFSET ") + swWriteInstructionsElementKey(Offset.first) + QLatin1Char(' ') + swWriteInstructionsPoint(Offset.second));

	for (const std::pair<const swInstructionsElementKey, QPointF>& Position : mLockedPositions)
		CommandLines.append(QLatin1String("LOCK ") + swWriteInstructionsElementKey(Position.first) + QLatin1Char(' ') + swWriteInstructionsPoint(Position.second));

	for (const swInstructionsNotice& Notice : mNotices)
		CommandLines.append(QStringLiteral("NOTICE %1 PAGE ").arg(Notice.Id) + swWriteInstructionsPageKey(Notice.PageKey) + QLatin1String(" POSITION ") + swWriteInstructionsPoint(Notice.Position) + QLatin1String(" TEXT ") + swEscapeInstructionsText(Notice.Text));

	for (const swInstructionsPointer& Pointer : mPointers)
	{
		const QString Anchor = Pointer.Anchor == SW_POINTER_ANCHOR_AUTO ? QString(gInstructionsAutoName) : QString::number(Pointer.Anchor);
		const QString Style = Pointer.Style < 0 ? QString(gInstructionsDefaultName) : QString(gInstructionsPointerStyleNames[Pointer.Style]);
		const QString Tip = Pointer.Tip < 0 ? QString(gInstructionsDefaultName) : QString(gInstructionsPointerTipNames[Pointer.Tip]);

		CommandLines.append(QStringLiteral("POINTER %1 PAGE ").arg(Pointer.Id) + swWriteInstructionsPageKey(Pointer.PageKey) + QLatin1String(" SOURCE ") + swWriteInstructionsElementKey(Pointer.Source) +
		                    QLatin1String(" ANCHOR ") + Anchor + QLatin1String(" STYLE ") + Style + QLatin1String(" TIP ") + Tip +
		                    QLatin1String(" TARGET ") + gInstructionsTargetNames[static_cast<int>(Pointer.TargetType)] + QLatin1Char(' ') + swWriteInstructionsElementKey(Pointer.TargetElement) +
		                    QStringLiteral(" PART %1 POSITION ").arg(Pointer.TargetPart) + swWriteInstructionsPoint(Pointer.TipPosition));
	}

	return CommandLines;
}

void swInstructions::StoreCommandLines()
{
	swModel* MainModel = mProject ? mProject->GetMainModel() : nullptr;

	if (!MainModel)
		return;

	const QStringList CommandLines = GetCommandLines();

	if (CommandLines == MainModel->GetInstructionLines())
		return;

	MainModel->SetInstructionLines(CommandLines);
	mProject->MarkAsModified();

	if (gMainWindow)
		gMainWindow->UpdateTitle();
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

	// the boxes keep the former spacing around the elements, without a border or a background
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

	// a notice stands out from the page with its frame
	Properties[static_cast<int>(swInstructionsPropertyType::NoticeFont)].Value = QFont("Arial", 16).toString();
	Properties[static_cast<int>(swInstructionsPropertyType::NoticeTextColor)].Value = SW_RGBA(0, 0, 0, 255);
	Properties[static_cast<int>(swInstructionsPropertyType::NoticeBorderWidth)].Value = 2;
	Properties[static_cast<int>(swInstructionsPropertyType::NoticeBackgroundColor)].Value = SW_RGBA(255, 255, 255, 255);
	Properties[static_cast<int>(swInstructionsPropertyType::PointerStyle)].Value = static_cast<int>(swInstructionsPointerStyle::Line);
	Properties[static_cast<int>(swInstructionsPropertyType::PointerTip)].Value = static_cast<int>(swInstructionsPointerTip::Arrowhead);
	Properties[static_cast<int>(swInstructionsPropertyType::PointerColor)].Value = SW_RGBA(0, 0, 0, 255);
	// a pointer follows the border of its element and its tip is three times its width
	Properties[static_cast<int>(swInstructionsPropertyType::PointerWidth)].Value = 0;
	Properties[static_cast<int>(swInstructionsPropertyType::PointerTipSize)].Value = 0;
	// the parts stand in rows on their bottom edge so the counts line up
	Properties[static_cast<int>(swInstructionsPropertyType::PLILayout)].Value = static_cast<int>(swPartsListLayout::Rows);
	Properties[static_cast<int>(swInstructionsPropertyType::PLIAlignment)].Value = static_cast<int>(swPartsListAlignment::End);

	static_assert(static_cast<int>(swInstructionsPropertyType::Count) == 92, "Missing default property");

	return Properties;
}

void swInstructions::SetDefaultProperties()
{
	mStepProperties = CreateDefaultProperties();
}

void swInstructions::ResetProperties()
{
	// only the defaults, page properties and moved elements are restored, the page layouts and blank pages stay
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

void swInstructions::SetPageDividerPreset(size_t PageIndex, int Rows, int Columns)
{
	// a split page shows its dividers, the preset changes both and rebuilds the pages once
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
	// the default blank pages belong to no step, a blank page goes before the page that follows them
	swInstructionsStepKey Key;
	return GetPageKey(PageIndex, Key) && Key.first;
}

void swInstructions::InsertBlankPage(size_t PageIndex)
{
	// the blank page goes before the page at this index and stays with its first step
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

	// a default blank page is removed by switching it off
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
	// the document font replaces every element font to keep the instructions uniform
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
	// step 0 is the whole model, shown by its bill of materials
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
	// the background is drawn with the page, the thumbnails with their step
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

	// a page starts from the value of all pages and returns to it when it no longer has its own
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

	// an element moved back onto its placement no longer keeps an offset
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
	// a placed element leaves the position it was moved to
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

	// pages sharing a type and a first step, like blank pages inserted together, are told apart by their rank
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

const swInstructionsNotice* swInstructions::FindNotice(int Id) const
{
	for (const swInstructionsNotice& Notice : mNotices)
		if (Notice.Id == Id)
			return &Notice;

	return nullptr;
}

int swInstructions::AddNotice(size_t PageIndex, const QPointF& Position, const QString& Text)
{
	swInstructionsNotice Notice;

	Notice.Id = mNextNoticeId++;
	Notice.PageKey = GetPageElementsKey(PageIndex);
	Notice.Position = Position;
	Notice.Text = Text;

	mNotices.push_back(Notice);

	emit PageLayoutChanged();

	return Notice.Id;
}

void swInstructions::SetNoticeText(int Id, const QString& Text)
{
	for (swInstructionsNotice& Notice : mNotices)
	{
		if (Notice.Id != Id || Notice.Text == Text)
			continue;

		Notice.Text = Text;

		emit PageLayoutChanged();
		return;
	}
}

void swInstructions::RemoveNotice(int Id)
{
	const swInstructionsElementKey Key = { swInstructionsPlacementElement::Notice, nullptr, static_cast<swStep>(Id) };
	const size_t NoticeCount = mNotices.size();

	mNotices.erase(std::remove_if(mNotices.begin(), mNotices.end(), [Id](const swInstructionsNotice& Notice)
	{
		return Notice.Id == Id;
	}), mNotices.end());

	if (mNotices.size() == NoticeCount)
		return;

	// the pointers of the notice go with it, the pointers at it keep pointing at where it was
	mPointers.erase(std::remove_if(mPointers.begin(), mPointers.end(), [&Key](const swInstructionsPointer& Pointer)
	{
		return Pointer.Source == Key;
	}), mPointers.end());

	for (swInstructionsPointer& Pointer : mPointers)
		if (Pointer.TargetType == swInstructionsPointerTarget::Element && Pointer.TargetElement == Key)
			Pointer.TargetType = swInstructionsPointerTarget::Point;

	mElementOffsets.erase(Key);
	mLockedPositions.erase(Key);

	emit PageLayoutChanged();
}

const swInstructionsPointer* swInstructions::FindPointer(int Id) const
{
	for (const swInstructionsPointer& Pointer : mPointers)
		if (Pointer.Id == Id)
			return &Pointer;

	return nullptr;
}

int swInstructions::AddPointer(size_t PageIndex, const swInstructionsElementKey& Source, const QPointF& TipPosition)
{
	swInstructionsPointer Pointer;

	Pointer.Id = mNextPointerId++;
	Pointer.PageKey = GetPageElementsKey(PageIndex);
	Pointer.Source = Source;
	Pointer.TargetElement = { swInstructionsPlacementElement::None, nullptr, 0 };
	Pointer.TipPosition = TipPosition;

	mPointers.push_back(Pointer);

	emit PageLayoutChanged();

	return Pointer.Id;
}

void swInstructions::SetPointerTarget(int Id, swInstructionsPointerTarget TargetType, const swInstructionsElementKey& TargetElement, int TargetPart, const QPointF& TipPosition)
{
	for (swInstructionsPointer& Pointer : mPointers)
	{
		if (Pointer.Id != Id)
			continue;

		Pointer.TargetType = TargetType;
		Pointer.TargetElement = TargetElement;
		Pointer.TargetPart = TargetPart;
		Pointer.TipPosition = TipPosition;

		emit PageLayoutChanged();
		return;
	}
}

void swInstructions::SetPointerAnchor(int Id, int Anchor)
{
	for (swInstructionsPointer& Pointer : mPointers)
	{
		if (Pointer.Id != Id || Pointer.Anchor == Anchor)
			continue;

		Pointer.Anchor = Anchor;

		emit PageLayoutChanged();
		return;
	}
}

void swInstructions::SetPointerStyle(int Id, int Style, int Tip)
{
	for (swInstructionsPointer& Pointer : mPointers)
	{
		if (Pointer.Id != Id || (Pointer.Style == Style && Pointer.Tip == Tip))
			continue;

		Pointer.Style = Style;
		Pointer.Tip = Tip;

		emit PageLayoutChanged();
		return;
	}
}

void swInstructions::RemovePointer(int Id)
{
	const size_t PointerCount = mPointers.size();

	mPointers.erase(std::remove_if(mPointers.begin(), mPointers.end(), [Id](const swInstructionsPointer& Pointer)
	{
		return Pointer.Id == Id;
	}), mPointers.end());

	if (mPointers.size() != PointerCount)
		emit PageLayoutChanged();
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

	// the unlocked element stays where it was locked, as an offset from its placement
	if (Offset.isNull())
		mElementOffsets.erase(Key);
	else
		mElementOffsets[Key] = Offset;

	emit PageLayoutChanged();
}

// use to make the default page, grid and units of the preferences the current ones
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

// use to bring back the default page, grid and units of the preferences
void swInstructions::ResetPageSetup()
{
	LoadDefaultPageSetup();

	SetShowGrid(swGetProfileInteger(SW_PROFILE_INSTRUCTIONS_SHOW_GRID) != 0);
	SetSnapToGrid(swGetProfileInteger(SW_PROFILE_INSTRUCTIONS_SNAP_TO_GRID) != 0);
	SetGridSize(swGetProfileFloat(SW_PROFILE_INSTRUCTIONS_GRID_SIZE));
	SetGridDivisions(swGetProfileInteger(SW_PROFILE_INSTRUCTIONS_GRID_DIVISIONS));
	SetGridWithinMargins(swGetProfileInteger(SW_PROFILE_INSTRUCTIONS_GRID_WITHIN_MARGINS) != 0);

	// every value follows the units of the paper
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

// use to follow the grid and margin colors stored by the preferences
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

	// a page with its own value overrides every other value of its steps
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

	// dividers and the bill of materials change the page layout instead of a single step
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

	// Models keep their step settings and gain entries for steps added since.
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

void swInstructions::AddDefaultBlankPage(const swInstructionsStepKey& Key)
{
	swInstructionsPage BlankPage;
	BlankPage.Type = swInstructionsPageType::Blank;
	BlankPage.BlankKey = Key;
	mPages.emplace_back(std::move(BlankPage));
}
