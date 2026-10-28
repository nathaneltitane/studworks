#include "sw_global.h"
#include "sw_dialog_instructions_editor.h"
#include "sw_instructions.h"
#include "sw_application.h"
#include "sw_colors.h"
#include "sw_profile.h"
#include "sw_project.h"
#include "sw_window_main.h"
#include "sw_window_utilities.h"

// use to find what is wrong with a line of an ldraw file
// use to list the meta commands written after a line type of 0
static const QStringList& swGetLDrawMetaKeywords()
{
	static const QStringList Keywords = { "STEP", "FILE", "NOFILE", "BFC", "CLEAR", "PAUSE", "SAVE", "PRINT", "WRITE", "ROTSTEP", "Name:", "Author:" };
	return Keywords;
}

// use to write the meta command of a line in capitals
static QString swGetLDrawMetaLine(const QString& Line)
{
	const QRegularExpression Expression(QLatin1String("^(\\s*0\\s+)(\\S+)"));
	const QRegularExpressionMatch Match = Expression.match(Line);

	if (!Match.hasMatch())
		return Line;

	const QString Command = Match.captured(2);

	if (Command.endsWith(':') || Command.startsWith(QLatin1String("//")))
		return Line;

	if (!Command.startsWith('!') && !swGetLDrawMetaKeywords().contains(Command, Qt::CaseInsensitive))
		return Line;

	QString Result = Line;
	Result.replace(Match.capturedStart(2), Command.size(), Command.toUpper());
	return Result;
}

static QString swGetLDrawLineError(const QString& Line)
{
	const QStringList Tokens = Line.simplified().split(' ', Qt::SkipEmptyParts);

	if (Tokens.isEmpty())
		return QString();

	// parameter count of each line type with the part file name last
	static const int TypeTokenCounts[] = { 0, 15, 8, 11, 14, 14 };
	bool Valid = false;
	const int TypeNumber = Tokens[0].toInt(&Valid);

	if (!Valid || TypeNumber < 0 || TypeNumber > 5 || Tokens[0].size() != 1)
		return QObject::tr("Unknown line type '%1'.").arg(Tokens[0]);

	if (TypeNumber == 0)
		return QString();

	const int TokenCount = TypeTokenCounts[TypeNumber];

	// part file names may hold spaces so a part line can be longer
	if (Tokens.size() < TokenCount || (TypeNumber != 1 && Tokens.size() > TokenCount))
		return QObject::tr("A line of type %1 has %2 values instead of %3.").arg(TypeNumber).arg(Tokens.size()).arg(TokenCount);

	Tokens[1].toUInt(&Valid, 0);

	if (!Valid)
		return QObject::tr("Invalid color code '%1'.").arg(Tokens[1]);

	const int NumberEnd = TypeNumber == 1 ? TokenCount - 1 : TokenCount;

	for (int Index = 2; Index < NumberEnd; Index++)
	{
		Tokens[Index].toDouble(&Valid);

		if (!Valid)
			return QObject::tr("Invalid number '%1'.").arg(Tokens[Index]);
	}

	return QString();
}

// colors ldraw lines by type and writes color codes in their own color
class swLDrawHighlighter : public QSyntaxHighlighter
{
public:
	explicit swLDrawHighlighter(QObject* Parent);

protected:
	void highlightBlock(const QString& Text) override;

	QTextCharFormat CreateFormat(const QColor& Color, bool Bold, bool Italic) const;
	QTextCharFormat GetColorCodeFormat(const QString& Code) const;

	QTextCharFormat mCommentFormat;
	QTextCharFormat mKeywordFormat;
	QTextCharFormat mValueFormat;
	QTextCharFormat mNumberFormat;
	QTextCharFormat mTypeFormat;
	QTextCharFormat mPartFormat;
	QTextCharFormat mDataFormat;
	QTextCharFormat mErrorFormat;
};

swLDrawHighlighter::swLDrawHighlighter(QObject* Parent)
	: QSyntaxHighlighter(Parent)
{
	// light tints read on dark themes and deep hues on light themes
	const bool DarkTheme = gApplication && gApplication->IsDarkTheme();

	mCommentFormat = CreateFormat(DarkTheme ? QColor(0x8a, 0x95, 0xa3) : QColor(0x6a, 0x73, 0x7d), false, true);
	mKeywordFormat = CreateFormat(DarkTheme ? QColor(0x82, 0xaa, 0xff) : QColor(0x00, 0x33, 0xb3), true, false);
	mValueFormat = CreateFormat(DarkTheme ? QColor(0xc3, 0xe8, 0x8d) : QColor(0x06, 0x7d, 0x17), false, false);
	mNumberFormat = CreateFormat(DarkTheme ? QColor(0xf7, 0x8c, 0x6c) : QColor(0x17, 0x50, 0xeb), false, false);
	mTypeFormat = CreateFormat(DarkTheme ? QColor(0xff, 0xcb, 0x6b) : QColor(0x9e, 0x6a, 0x00), true, false);
	mPartFormat = CreateFormat(DarkTheme ? QColor(0x89, 0xdd, 0xff) : QColor(0x00, 0x62, 0x7a), true, false);
	mDataFormat = CreateFormat(DarkTheme ? QColor(0x6a, 0x73, 0x80) : QColor(0x9a, 0xa0, 0xa6), false, false);
	mErrorFormat = CreateFormat(DarkTheme ? QColor(0xff, 0x53, 0x70) : QColor(0xd3, 0x2f, 0x2f), false, false);
	mErrorFormat.setUnderlineStyle(QTextCharFormat::WaveUnderline);
	mErrorFormat.setUnderlineColor(mErrorFormat.foreground().color());
}

QTextCharFormat swLDrawHighlighter::CreateFormat(const QColor& Color, bool Bold, bool Italic) const
{
	QTextCharFormat Format;

	Format.setForeground(Color);
	Format.setFontItalic(Italic);

	if (Bold)
		Format.setFontWeight(QFont::Bold);

	return Format;
}

QTextCharFormat swLDrawHighlighter::GetColorCodeFormat(const QString& Code) const
{
	bool Valid = false;
	const quint32 Value = Code.toUInt(&Valid, 0);

	if (!Valid)
		return mErrorFormat;

	QColor Color;

	// direct colors carry their rgb value and codes are looked up without adding them
	if ((Value & 0xff000000) == 0x02000000)
		Color = QColor::fromRgb(Value & 0xffffff);
	else
	{
		for (const swColor& ListColor : gColorList)
		{
			if (ListColor.Code == Value)
			{
				Color = QColor::fromRgbF(ListColor.Value[0], ListColor.Value[1], ListColor.Value[2]);
				break;
			}
		}
	}

	if (!Color.isValid())
		return mNumberFormat;

	// colors too close to the background are lifted or dimmed to stay readable
	const int BaseLightness = QApplication::palette().color(QPalette::Base).lightness();

	if (qAbs(Color.lightness() - BaseLightness) < 80)
		Color = QColor::fromHsl(Color.hslHue(), Color.hslSaturation(), BaseLightness < 128 ? qMin(BaseLightness + 80, 255) : qMax(BaseLightness - 80, 0));

	QTextCharFormat Format;
	Format.setForeground(Color);
	Format.setFontWeight(QFont::Bold);

	return Format;
}

void swLDrawHighlighter::highlightBlock(const QString& Text)
{
	std::vector<std::pair<int, int>> Tokens;

	for (int Position = 0; Position < Text.size(); )
	{
		if (Text[Position].isSpace())
		{
			Position++;
			continue;
		}

		const int Start = Position;

		while (Position < Text.size() && !Text[Position].isSpace())
			Position++;

		Tokens.emplace_back(Start, Position - Start);
	}

	if (Tokens.empty())
		return;

	const auto GetToken = [&Text, &Tokens](size_t Index)
	{
		return Text.mid(Tokens[Index].first, Tokens[Index].second);
	};

	const auto SetTokenFormat = [this, &Tokens](size_t Index, const QTextCharFormat& Format)
	{
		setFormat(Tokens[Index].first, Tokens[Index].second, Format);
	};

	const auto SetRestFormat = [this, &Text, &Tokens](size_t Index, const QTextCharFormat& Format)
	{
		if (Index < Tokens.size())
			setFormat(Tokens[Index].first, Text.size() - Tokens[Index].first, Format);
	};

	const QString Type = GetToken(0);

	if (Type == QLatin1String("0"))
	{
		SetTokenFormat(0, mTypeFormat);

		if (Tokens.size() < 2)
			return;

		const QString Command = GetToken(1);

		// comments and embedded data are dimmed and metas show their command
		if (Command.startsWith(QLatin1String("//")))
			SetRestFormat(1, mCommentFormat);
		else if (Command == QLatin1String("!:"))
		{
			SetTokenFormat(1, mKeywordFormat);
			SetRestFormat(2, mDataFormat);
		}
		else if (Command.startsWith('!') || swGetLDrawMetaKeywords().contains(Command, Qt::CaseInsensitive))
		{
			SetRestFormat(2, mValueFormat);
			SetTokenFormat(1, mKeywordFormat);

			// program metas name their command after the program like 0 !SW PROP
			if (Command.startsWith('!') && Tokens.size() > 2 && GetToken(2) == GetToken(2).toUpper() && GetToken(2).at(0).isLetter())
				SetTokenFormat(2, mKeywordFormat);
		}
		else
			SetRestFormat(1, mCommentFormat);

		return;
	}

	// parameter count of each line type with the part file name last
	static const size_t TypeTokenCounts[] = { 0, 15, 8, 11, 14, 14 };
	bool Valid = false;
	const int TypeNumber = Type.toInt(&Valid);

	if (!Valid || TypeNumber < 1 || TypeNumber > 5 || Type.size() != 1)
	{
		SetRestFormat(0, mErrorFormat);
		return;
	}

	SetTokenFormat(0, mTypeFormat);

	const size_t TokenCount = TypeTokenCounts[TypeNumber];
	const size_t NumberEnd = TypeNumber == 1 ? TokenCount - 1 : TokenCount;

	if (Tokens.size() > 1)
		SetTokenFormat(1, GetColorCodeFormat(GetToken(1)));

	for (size_t Index = 2; Index < qMin(NumberEnd, Tokens.size()); Index++)
	{
		bool Number = false;
		GetToken(Index).toDouble(&Number);
		SetTokenFormat(Index, Number ? mNumberFormat : mErrorFormat);
	}

	if (TypeNumber == 1)
		SetRestFormat(NumberEnd, mPartFormat);
	else
		SetRestFormat(TokenCount, mErrorFormat);

	if (Tokens.size() < TokenCount)
		setFormat(0, 1, mErrorFormat);
}

// the text edit numbers its lines and marks each line end with a return arrow
class swEditorTextEdit : public QPlainTextEdit
{
public:
	swEditorTextEdit(QWidget* Parent)
		: QPlainTextEdit(Parent)
	{
		mLineNumberArea = new QWidget(this);
		mLineNumberArea->installEventFilter(this);

		connect(this, &QPlainTextEdit::blockCountChanged, this, &swEditorTextEdit::UpdateLineNumberWidth);
		connect(this, &QPlainTextEdit::updateRequest, this, &swEditorTextEdit::UpdateLineNumberArea);
		connect(this, &QPlainTextEdit::cursorPositionChanged, mLineNumberArea, QOverload<>::of(&QWidget::update));
	}

	void SetLineMarkersVisible(bool Visible)
	{
		mLineMarkersVisible = Visible;
		viewport()->update();
	}

	void SetLineNumbersVisible(bool Visible)
	{
		mLineNumbersVisible = Visible;
		mLineNumberArea->setVisible(Visible);
		UpdateLineNumberWidth();
	}

protected:
	int GetLineNumberWidth() const
	{
		if (!mLineNumbersVisible)
			return 0;

		const QFontMetrics Metrics(font());
		const int Digits = QString::number(qMax(blockCount(), 1)).size();

		return Metrics.horizontalAdvance(QLatin1Char('9')) * Digits + Metrics.horizontalAdvance(QLatin1Char(' ')) * 2;
	}

	void UpdateLineNumberWidth()
	{
		setViewportMargins(GetLineNumberWidth(), 0, 0, 0);
		ResizeLineNumberArea();
	}

	void UpdateLineNumberArea(const QRect& Rect, int ScrollY)
	{
		if (ScrollY)
			mLineNumberArea->scroll(0, ScrollY);
		else
			mLineNumberArea->update(0, Rect.y(), mLineNumberArea->width(), Rect.height());

		if (Rect.contains(viewport()->rect()))
			UpdateLineNumberWidth();
	}

	void ResizeLineNumberArea()
	{
		const QRect Rect = contentsRect();
		mLineNumberArea->setGeometry(Rect.left(), Rect.top(), GetLineNumberWidth(), Rect.height());
	}

	void resizeEvent(QResizeEvent* Event) override
	{
		QPlainTextEdit::resizeEvent(Event);
		ResizeLineNumberArea();
	}

	void changeEvent(QEvent* Event) override
	{
		QPlainTextEdit::changeEvent(Event);

		if (Event->type() == QEvent::FontChange)
			UpdateLineNumberWidth();
	}

	bool eventFilter(QObject* Object, QEvent* Event) override
	{
		if (Object == mLineNumberArea && Event->type() == QEvent::Paint)
		{
			PaintLineNumbers(static_cast<QPaintEvent*>(Event)->rect());
			return true;
		}

		return QPlainTextEdit::eventFilter(Object, Event);
	}

	void PaintLineNumbers(const QRect& Rect)
	{
		QPainter Painter(mLineNumberArea);
		Painter.fillRect(Rect, palette().color(QPalette::Window));
		Painter.setFont(font());

		// the line of the cursor is numbered in the text color
		const int CursorBlock = textCursor().blockNumber();
		const int Margin = QFontMetrics(font()).horizontalAdvance(QLatin1Char(' '));
		const int Width = mLineNumberArea->width() - Margin;
		const int Height = QFontMetrics(font()).height();
		const QPointF Offset = contentOffset();

		for (QTextBlock Block = firstVisibleBlock(); Block.isValid(); Block = Block.next())
		{
			const int Top = qRound(blockBoundingGeometry(Block).translated(Offset).top());

			if (Top > Rect.bottom())
				break;

			if (!Block.isVisible())
				continue;

			Painter.setPen(palette().color(Block.blockNumber() == CursorBlock ? QPalette::Text : QPalette::PlaceholderText));
			Painter.drawText(0, Top, Width, Height, Qt::AlignRight | Qt::AlignVCenter, QString::number(Block.blockNumber() + 1));
		}
	}

	void paintEvent(QPaintEvent* Event) override
	{
		QPlainTextEdit::paintEvent(Event);

		if (!mLineMarkersVisible)
			return;

		QPainter Painter(viewport());
		Painter.setFont(font());
		Painter.setPen(palette().color(QPalette::PlaceholderText));

		const QPointF Offset = contentOffset();
		const int Bottom = Event->rect().bottom();

		for (QTextBlock Block = firstVisibleBlock(); Block.isValid(); Block = Block.next())
		{
			const QRectF Geometry = blockBoundingGeometry(Block).translated(Offset);

			if (Geometry.top() > Bottom)
				break;

			const QTextLayout* Layout = Block.layout();

			// an empty last line has no line end to mark
			if (!Block.isVisible() || !Layout || !Layout->lineCount() || (!Block.next().isValid() && Block.length() <= 1))
				continue;

			// the arrow follows the last character of the last line
			const QTextLine Line = Layout->lineAt(Layout->lineCount() - 1);
			const qreal X = Geometry.left() + Line.cursorToX(Block.length() - 1);
			const qreal Y = Geometry.top() + Line.y() + Line.ascent();

			Painter.drawText(QPointF(X, Y), QString(QChar(0x21b5)));
		}
	}

	QWidget* mLineNumberArea = nullptr;
	bool mLineMarkersVisible = false;
	bool mLineNumbersVisible = false;
};

swInstructionsEditorWidget::swInstructionsEditorWidget(QWidget* Parent, Project* Project, swInstructions* Instructions)
	: QDockWidget(Parent), mProject(Project), mInstructions(Instructions)
{
	setWindowTitle(tr("Editor"));
	setWhatsThis(tr("<p><b>Editor</b></p><p>The text of the model file as it is saved, its parts, steps and instructions. Edit it and apply to load the model from the text, the change is unsaved until the model is saved.</p>"));
	swSetupWidgetHelp(this, nullptr);

	QWidget* CentralWidget = new QWidget(this);
	setWidget(CentralWidget);

	QVBoxLayout* Layout = new QVBoxLayout(CentralWidget);

	QWidget* ButtonRow = swCreatePanelRow(CentralWidget);

	auto CreateButton = [](QWidget* Row, const QString& Name, const QString& ToolTip)
	{
		QToolButton* Button = new QToolButton(Row);
		Button->setText(Name);
		Button->setToolButtonStyle(Qt::ToolButtonTextOnly);
		Button->setToolTip(ToolTip);
		Button->setWhatsThis(ToolTip);
		Button->setAccessibleName(Name);
		swSetupPanelButton(Button);
		Button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		static_cast<QHBoxLayout*>(Row->layout())->addWidget(Button, 1);

		return Button;
	};

	mApplyButton = CreateButton(ButtonRow, tr("Apply"), tr("Check the syntax of the edited text and load the model from it, the change is unsaved until the model is saved."));
	mRevertButton = CreateButton(ButtonRow, tr("Revert"), tr("Discard the edits and show the text of the model again."));

	// the options button sits at the top right like the other panels
	QWidget* OptionsRow = swCreatePanelRow(CentralWidget);
	swAddPanelRowStretch(OptionsRow);
	Layout->addWidget(OptionsRow);

	QMenu* OptionsMenu = new QMenu(OptionsRow);
	swAddPanelOptionsButton(OptionsRow, OptionsMenu, tr("Configure the editor."), tr("Editor Options"));

	mHighlightAction = OptionsMenu->addAction(tr("&Syntax Highlighting"));
	mHighlightAction->setCheckable(true);
	mHighlightAction->setChecked(swGetProfileInteger(SW_PROFILE_INSTRUCTIONS_EDITOR_HIGHLIGHT) != 0);
	connect(mHighlightAction, &QAction::toggled, this, &swInstructionsEditorWidget::SetHighlightEnabled);

	mNumbersAction = OptionsMenu->addAction(tr("Line &Numbers"));
	mNumbersAction->setCheckable(true);
	mNumbersAction->setChecked(swGetProfileInteger(SW_PROFILE_INSTRUCTIONS_EDITOR_NUMBERS) != 0);
	connect(mNumbersAction, &QAction::toggled, this, &swInstructionsEditorWidget::SetNumbersEnabled);

	mWrapAction = OptionsMenu->addAction(tr("&Wrap Lines"));
	mWrapAction->setCheckable(true);
	mWrapAction->setChecked(swGetProfileInteger(SW_PROFILE_INSTRUCTIONS_EDITOR_WRAP) != 0);
	connect(mWrapAction, &QAction::toggled, this, &swInstructionsEditorWidget::SetWrapEnabled);

	mMarkersAction = OptionsMenu->addAction(tr("Whitespace &Markers"));
	mMarkersAction->setCheckable(true);
	mMarkersAction->setChecked(swGetProfileInteger(SW_PROFILE_INSTRUCTIONS_EDITOR_MARKERS) != 0);
	connect(mMarkersAction, &QAction::toggled, this, &swInstructionsEditorWidget::SetMarkersEnabled);

	OptionsMenu->addSeparator();

	mFindAction = OptionsMenu->addAction(tr("&Find and Replace"));
	mFindAction->setCheckable(true);
	mFindAction->setShortcut(QKeySequence::Find);
	mFindAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
	addAction(mFindAction);
	connect(mFindAction, &QAction::toggled, this, &swInstructionsEditorWidget::SetFindVisible);

	QAction* MatchCaseAction = OptionsMenu->addAction(tr("Match &Case"));
	MatchCaseAction->setCheckable(true);
	connect(MatchCaseAction, &QAction::toggled, this, [this](bool Checked)
	{
		mMatchCase = Checked;
		UpdateMatches();
	});

	QAction* WholeWordsAction = OptionsMenu->addAction(tr("Wh&ole Words"));
	WholeWordsAction->setCheckable(true);
	connect(WholeWordsAction, &QAction::toggled, this, [this](bool Checked)
	{
		mWholeWords = Checked;
		UpdateMatches();
	});

	// the find and replace rows open on demand below the text
	mFindWidget = new QWidget(CentralWidget);
	QVBoxLayout* FindLayout = new QVBoxLayout(mFindWidget);
	FindLayout->setContentsMargins(0, 0, 0, 0);
	mFindWidget->hide();

	QWidget* FindRow = swCreatePanelRow(mFindWidget);
	FindLayout->addWidget(FindRow);

	mFindEdit = new QLineEdit(FindRow);
	mFindEdit->setPlaceholderText(tr("Find"));
	mFindEdit->setAccessibleName(tr("Find"));
	mFindEdit->setToolTip(tr("Text to find in the model file, enter finds the next match and shift enter the previous one."));
	mFindEdit->setClearButtonEnabled(true);
	swSetupPanelEdit(mFindEdit);
	static_cast<QHBoxLayout*>(FindRow->layout())->addWidget(mFindEdit, 2);

	QToolButton* PreviousButton = CreateButton(FindRow, tr("Previous"), tr("Select the previous match."));
	QToolButton* NextButton = CreateButton(FindRow, tr("Next"), tr("Select the next match."));

	QWidget* ReplaceRow = swCreatePanelRow(mFindWidget);
	FindLayout->addWidget(ReplaceRow);

	mReplaceEdit = new QLineEdit(ReplaceRow);
	mReplaceEdit->setPlaceholderText(tr("Replace"));
	mReplaceEdit->setAccessibleName(tr("Replace"));
	mReplaceEdit->setToolTip(tr("Text that replaces the matches, enter replaces the selected match."));
	swSetupPanelEdit(mReplaceEdit);
	static_cast<QHBoxLayout*>(ReplaceRow->layout())->addWidget(mReplaceEdit, 2);

	QToolButton* ReplaceButton = CreateButton(ReplaceRow, tr("Replace"), tr("Replace the selected match and select the next one."));
	QToolButton* ReplaceAllButton = CreateButton(ReplaceRow, tr("Replace All"), tr("Replace every match in the text."));

	mMatchLabel = new QLabel(FindRow);
	mMatchLabel->setMinimumWidth(mMatchLabel->fontMetrics().horizontalAdvance(tr("No Matches")));
	mMatchLabel->setAlignment(Qt::AlignCenter);
	static_cast<QHBoxLayout*>(FindRow->layout())->insertWidget(1, mMatchLabel);

	QLabel* ReplaceSpacer = new QLabel(ReplaceRow);
	ReplaceSpacer->setMinimumWidth(mMatchLabel->minimumWidth());
	static_cast<QHBoxLayout*>(ReplaceRow->layout())->insertWidget(1, ReplaceSpacer);

	connect(mFindEdit, &QLineEdit::textChanged, this, [this]()
	{
		// typing finds the first match from the start of the selection
		QTextCursor Cursor = mTextEdit->textCursor();
		Cursor.setPosition(Cursor.selectionStart());
		mTextEdit->setTextCursor(Cursor);
		FindText(false);
	});
	connect(mFindEdit, &QLineEdit::returnPressed, this, [this]()
	{
		FindText(QApplication::keyboardModifiers() & Qt::ShiftModifier);
	});
	connect(mReplaceEdit, &QLineEdit::returnPressed, this, &swInstructionsEditorWidget::ReplaceClicked);
	connect(PreviousButton, &QToolButton::clicked, this, [this]() { FindText(true); });
	connect(NextButton, &QToolButton::clicked, this, [this]() { FindText(false); });
	connect(ReplaceButton, &QToolButton::clicked, this, &swInstructionsEditorWidget::ReplaceClicked);
	connect(ReplaceAllButton, &QToolButton::clicked, this, &swInstructionsEditorWidget::ReplaceAllClicked);

	QShortcut* CloseFindShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), mFindWidget, nullptr, nullptr, Qt::WidgetWithChildrenShortcut);
	connect(CloseFindShortcut, &QShortcut::activated, mFindAction, [this]() { mFindAction->setChecked(false); });

	// the text keeps its columns in a fixed font
	mTextEdit = new swEditorTextEdit(CentralWidget);
	mTextEdit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
	mTextEdit->setLineWrapMode(mWrapAction->isChecked() ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);
	mTextEdit->setToolTip(tr("Text of the model file."));
	mTextEdit->setAccessibleName(tr("Editor"));
	// the editor buttons sit below the text
	Layout->addWidget(mTextEdit, 1);
	Layout->addWidget(mFindWidget);
	Layout->addWidget(ButtonRow);

	SetMarkersEnabled(mMarkersAction->isChecked());
	mTextEdit->SetLineNumbersVisible(mNumbersAction->isChecked());

	mHighlighter = new swLDrawHighlighter(this);

	if (mHighlightAction->isChecked())
		mHighlighter->setDocument(mTextEdit->document());

	connect(mApplyButton, &QToolButton::clicked, this, &swInstructionsEditorWidget::ApplyClicked);

	// the save shortcut applies the edited text and saves the model while the panel has the focus
	QAction* SaveAction = new QAction(tr("Save"), this);
	SaveAction->setShortcuts(gMainWindow->mActions[SW_FILE_SAVE]->shortcuts());
	SaveAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
	addAction(SaveAction);
	connect(SaveAction, &QAction::triggered, this, [this]() { ApplyText(true); });
	connect(mRevertButton, &QToolButton::clicked, this, &swInstructionsEditorWidget::RevertClicked);
	connect(mTextEdit->document(), &QTextDocument::modificationChanged, this, &swInstructionsEditorWidget::UpdateButtons);
	connect(mTextEdit, &QPlainTextEdit::textChanged, this, &swInstructionsEditorWidget::UpdateMatches);
	connect(mTextEdit, &QPlainTextEdit::cursorPositionChanged, this, &swInstructionsEditorWidget::UpdateMatches);

	// the text follows the model and its instructions while it is shown
	connect(this, &QDockWidget::visibilityChanged, this, [this](bool Visible)
	{
		if (Visible)
			ScheduleUpdate();
	});
	connect(mInstructions, &swInstructions::PageSetupChanged, this, &swInstructionsEditorWidget::ScheduleUpdate);
	connect(mInstructions, &swInstructions::PageLayoutChanged, this, &swInstructionsEditorWidget::ScheduleUpdate);
	connect(mInstructions, &swInstructions::StepSettingsChanged, this, &swInstructionsEditorWidget::ScheduleUpdate);

	UpdateButtons();
}

void swInstructionsEditorWidget::SetProject(Project* Project)
{
	mProject = Project;

	// the applied text is the text of the loaded project
	mTextEdit->document()->setModified(false);
	ScheduleUpdate();
}

void swInstructionsEditorWidget::ScheduleUpdate()
{
	// several changes in a row write the text once, after the current event
	if (mUpdatePending)
		return;

	mUpdatePending = true;

	QTimer::singleShot(0, this, [this]()
	{
		mUpdatePending = false;
		UpdateText();
	});
}

void swInstructionsEditorWidget::UpdateText()
{
	// edits not applied yet are kept, revert shows the model again
	if (!isVisible() || mTextEdit->document()->isModified())
		return;

	QByteArray Bytes;
	QTextStream SaveStream(&Bytes, QIODevice::WriteOnly);

	if (!mProject->Save(SaveStream))
		return;

	SaveStream.flush();

	QTextStream ReadStream(&Bytes, QIODevice::ReadOnly);
	const QString Text = ReadStream.readAll();

	if (Text == mTextEdit->toPlainText())
		return;

	const int Position = GetTextPosition();
	const int Scroll = GetTextScroll();

	mTextEdit->setPlainText(Text);
	SetTextPosition(Position, Scroll);
}

void swInstructionsEditorWidget::UpdateButtons()
{
	const bool Modified = mTextEdit->document()->isModified();

	mApplyButton->setEnabled(Modified);
	mRevertButton->setEnabled(Modified);
}

int swInstructionsEditorWidget::GetTextPosition() const
{
	return mTextEdit->textCursor().position();
}

int swInstructionsEditorWidget::GetTextScroll() const
{
	return mTextEdit->verticalScrollBar()->value();
}

void swInstructionsEditorWidget::SetTextPosition(int Position, int Scroll)
{
	QTextCursor Cursor = mTextEdit->textCursor();
	Cursor.setPosition(qBound(0, Position, mTextEdit->document()->characterCount() - 1));
	mTextEdit->setTextCursor(Cursor);
	mTextEdit->verticalScrollBar()->setValue(Scroll);
}

void swInstructionsEditorWidget::ApplyClicked()
{
	ApplyText(false);
}

void swInstructionsEditorWidget::ApplyText(bool Save)
{
	// saving an unedited text saves the model as it is
	if (!mTextEdit->document()->isModified())
	{
		if (Save)
			gMainWindow->SaveProject(swGetActiveProject()->GetFileName());

		return;
	}

	if (!ValidateText())
		return;

	// meta commands are read in capitals only
	QStringList Lines = mTextEdit->toPlainText().split('\n');

	for (QString& Line : Lines)
		Line = swGetLDrawMetaLine(Line);

	QByteArray Bytes;
	QTextStream Stream(&Bytes, QIODevice::WriteOnly);
	Stream << Lines.join('\n');
	Stream.flush();

	// loading replaces the project so it runs once this event is done
	QTimer::singleShot(0, gMainWindow, [Bytes, Save]()
	{
		if (gMainWindow->LoadProjectText(Bytes) && Save)
			gMainWindow->SaveProject(swGetActiveProject()->GetFileName());
	});
}

bool swInstructionsEditorWidget::ValidateText()
{
	QStringList Errors;
	int FirstErrorLine = -1;
	const QStringList Lines = mTextEdit->toPlainText().split('\n');

	for (int LineIndex = 0; LineIndex < Lines.size(); LineIndex++)
	{
		const QString Error = swGetLDrawLineError(Lines[LineIndex]);

		if (Error.isEmpty())
			continue;

		if (FirstErrorLine < 0)
			FirstErrorLine = LineIndex;

		Errors.append(tr("Line %1: %2").arg(LineIndex + 1).arg(Error));
	}

	if (Errors.isEmpty())
		return true;

	// the first wrong line is selected and the message lists the first ten
	QTextCursor Cursor(mTextEdit->document()->findBlockByNumber(FirstErrorLine));
	Cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
	mTextEdit->setTextCursor(Cursor);
	mTextEdit->setFocus();

	const int ErrorCount = Errors.size();

	if (ErrorCount > 10)
	{
		Errors = Errors.mid(0, 10);
		Errors.append(tr("And %n more.", nullptr, ErrorCount - 10));
	}

	QMessageBox::warning(this, tr("Editor"), tr("The text was not applied, %n line(s) are not valid LDraw syntax:", nullptr, ErrorCount) + QLatin1String("\n\n") + Errors.join('\n'));

	return false;
}

void swInstructionsEditorWidget::RevertClicked()
{
	mTextEdit->document()->setModified(false);
	UpdateText();
}

// use to follow the editor options stored by the preferences, each toggled action applies its option
void swInstructionsEditorWidget::UpdateOptions()
{
	mHighlightAction->setChecked(swGetProfileInteger(SW_PROFILE_INSTRUCTIONS_EDITOR_HIGHLIGHT) != 0);
	mNumbersAction->setChecked(swGetProfileInteger(SW_PROFILE_INSTRUCTIONS_EDITOR_NUMBERS) != 0);
	mWrapAction->setChecked(swGetProfileInteger(SW_PROFILE_INSTRUCTIONS_EDITOR_WRAP) != 0);
	mMarkersAction->setChecked(swGetProfileInteger(SW_PROFILE_INSTRUCTIONS_EDITOR_MARKERS) != 0);
}

void swInstructionsEditorWidget::SetHighlightEnabled(bool Enabled)
{
	swSetProfileInteger(SW_PROFILE_INSTRUCTIONS_EDITOR_HIGHLIGHT, Enabled);
	mHighlighter->setDocument(Enabled ? mTextEdit->document() : nullptr);
}

void swInstructionsEditorWidget::SetNumbersEnabled(bool Enabled)
{
	swSetProfileInteger(SW_PROFILE_INSTRUCTIONS_EDITOR_NUMBERS, Enabled);
	mTextEdit->SetLineNumbersVisible(Enabled);
}

void swInstructionsEditorWidget::SetWrapEnabled(bool Enabled)
{
	swSetProfileInteger(SW_PROFILE_INSTRUCTIONS_EDITOR_WRAP, Enabled);
	mTextEdit->setLineWrapMode(Enabled ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);
}

void swInstructionsEditorWidget::SetMarkersEnabled(bool Enabled)
{
	swSetProfileInteger(SW_PROFILE_INSTRUCTIONS_EDITOR_MARKERS, Enabled);

	// tabs and spaces show their markers and line ends show a return arrow
	QTextDocument* Document = mTextEdit->document();
	QTextOption Option = Document->defaultTextOption();
	Option.setFlags(Enabled ? Option.flags() | QTextOption::ShowTabsAndSpaces : Option.flags() & ~QTextOption::ShowTabsAndSpaces);
	Document->setDefaultTextOption(Option);
	mTextEdit->SetLineMarkersVisible(Enabled);
}

void swInstructionsEditorWidget::SetFindVisible(bool Visible)
{
	QSignalBlocker Blocker(mFindAction);
	mFindAction->setChecked(Visible);
	mFindWidget->setVisible(Visible);

	if (!Visible)
	{
		mTextEdit->setExtraSelections({});
		mTextEdit->setFocus();
		return;
	}

	// a selection on one line becomes the text to find
	const QString Selection = mTextEdit->textCursor().selectedText();

	if (!Selection.isEmpty() && !Selection.contains(QChar::ParagraphSeparator))
		mFindEdit->setText(Selection);

	mFindEdit->setFocus();
	mFindEdit->selectAll();
	UpdateMatches();
}

QTextDocument::FindFlags swInstructionsEditorWidget::GetFindFlags() const
{
	QTextDocument::FindFlags Flags;

	if (mMatchCase)
		Flags |= QTextDocument::FindCaseSensitively;

	if (mWholeWords)
		Flags |= QTextDocument::FindWholeWords;

	return Flags;
}

void swInstructionsEditorWidget::FindText(bool Backward)
{
	const QString Text = mFindEdit->text();

	if (Text.isEmpty())
	{
		UpdateMatches();
		return;
	}

	QTextDocument* Document = mTextEdit->document();
	const QTextDocument::FindFlags Flags = GetFindFlags() | (Backward ? QTextDocument::FindBackward : QTextDocument::FindFlags());
	QTextCursor Cursor = Document->find(Text, mTextEdit->textCursor(), Flags);

	// the search wraps around at the start and end of the text
	if (Cursor.isNull())
		Cursor = Document->find(Text, Backward ? Document->characterCount() - 1 : 0, Flags);

	if (!Cursor.isNull())
		mTextEdit->setTextCursor(Cursor);

	UpdateMatches();
}

void swInstructionsEditorWidget::ReplaceClicked()
{
	const QString Text = mFindEdit->text();
	QTextCursor Cursor = mTextEdit->textCursor();

	if (Text.isEmpty())
		return;

	// only a selection that is a match is replaced
	const QTextCursor Match = mTextEdit->document()->find(Text, Cursor.selectionStart(), GetFindFlags());

	if (Cursor.hasSelection() && Match.selectionStart() == Cursor.selectionStart() && Match.selectionEnd() == Cursor.selectionEnd())
	{
		Cursor.insertText(mReplaceEdit->text());
		mTextEdit->setTextCursor(Cursor);
	}

	FindText(false);
}

void swInstructionsEditorWidget::ReplaceAllClicked()
{
	const QString Text = mFindEdit->text();

	if (Text.isEmpty())
		return;

	// every replacement is one step to undo
	QTextDocument* Document = mTextEdit->document();
	QTextCursor Cursor(Document);
	int Count = 0;

	Cursor.beginEditBlock();

	for (QTextCursor Match = Document->find(Text, 0, GetFindFlags()); !Match.isNull(); Match = Document->find(Text, Cursor.position(), GetFindFlags()))
	{
		Cursor.setPosition(Match.selectionStart());
		Cursor.setPosition(Match.selectionEnd(), QTextCursor::KeepAnchor);
		Cursor.insertText(mReplaceEdit->text());
		Count++;
	}

	Cursor.endEditBlock();

	mMatchLabel->setText(tr("%n Replaced", nullptr, Count));
}

void swInstructionsEditorWidget::UpdateMatches()
{
	if (!mFindWidget->isVisible())
		return;

	const QString Text = mFindEdit->text();
	QList<QTextEdit::ExtraSelection> Selections;

	if (Text.isEmpty())
	{
		mMatchLabel->clear();
		mTextEdit->setExtraSelections(Selections);
		return;
	}

	// every match is marked and the label counts them
	QTextDocument* Document = mTextEdit->document();
	const QTextCursor Current = mTextEdit->textCursor();
	QColor Color = palette().color(QPalette::Highlight);
	Color.setAlpha(80);
	int CurrentIndex = 0;

	for (QTextCursor Match = Document->find(Text, 0, GetFindFlags()); !Match.isNull(); Match = Document->find(Text, Match, GetFindFlags()))
	{
		QTextEdit::ExtraSelection Selection;
		Selection.cursor = Match;
		Selection.format.setBackground(Color);
		Selections.append(Selection);

		if (Match.selectionStart() == Current.selectionStart() && Match.selectionEnd() == Current.selectionEnd())
			CurrentIndex = Selections.size();
	}

	mTextEdit->setExtraSelections(Selections);

	if (Selections.isEmpty())
		mMatchLabel->setText(tr("No Matches"));
	else if (CurrentIndex)
		mMatchLabel->setText(tr("%1 of %2").arg(CurrentIndex).arg(Selections.size()));
	else
		mMatchLabel->setText(tr("%n Matches", nullptr, Selections.size()));
}
