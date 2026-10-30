#include "sw_global.h"
#include "sw_instructions.h"
#include "sw_project.h"
#include "sw_window_main.h"
#include "sw_window_utilities.h"

// metas name properties instead of numbers
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
	QLatin1String("PARTS_LIST_LAYOUT"), QLatin1String("PARTS_LIST_ALIGNMENT"),
	QLatin1String("MODEL_PROJECTION"), QLatin1String("FADE_PREVIOUS_STEPS"), QLatin1String("FADE_COLOR"), QLatin1String("HIGHLIGHT_NEW_PARTS"), QLatin1String("HIGHLIGHT_COLOR")
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

// default pointers write the default
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

// use to keep free text on one line
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

// the default type sets the format
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

// use to read the meta command fields
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

	// write defaults by name, others by number
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
	// quoted names are models
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

void swInstructions::LoadCommandLines()
{
	swModel* MainModel = mProject ? mProject->GetMainModel() : nullptr;

	if (!MainModel)
		return;

	// skip commands with missing models
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

	// write changed properties only
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
