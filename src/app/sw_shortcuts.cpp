#include "sw_global.h"
#include "sw_shortcuts.h"
#include "sw_profile.h"

swKeyboardShortcuts gKeyboardShortcuts;
swMouseShortcuts gMouseShortcuts;

void swLoadDefaultKeyboardShortcuts()
{
	QByteArray Buffer = swGetProfileBuffer(SW_PROFILE_KEYBOARD_SHORTCUTS);
	QTextStream Stream(Buffer, QIODevice::ReadOnly);

	if (Buffer.isEmpty() || !gKeyboardShortcuts.Load(Stream))
		gKeyboardShortcuts.Reset();
}

void swSaveDefaultKeyboardShortcuts()
{
	QByteArray Buffer;
	QTextStream Stream(&Buffer, QIODevice::WriteOnly);

	gKeyboardShortcuts.Save(Stream);

	swSetProfileBuffer(SW_PROFILE_KEYBOARD_SHORTCUTS, Buffer);
}

void swResetDefaultKeyboardShortcuts()
{
	gKeyboardShortcuts.Reset();

	swRemoveProfileKey(SW_PROFILE_KEYBOARD_SHORTCUTS);
}

void swLoadDefaultMouseShortcuts()
{
	QStringList Shortcuts = swGetProfileStringList(SW_PROFILE_MOUSE_SHORTCUTS);

	if (Shortcuts.isEmpty() || !gMouseShortcuts.Load(Shortcuts))
		gMouseShortcuts.Reset();
}

void swSaveDefaultMouseShortcuts()
{
	QStringList Shortcuts;

	gMouseShortcuts.Save(Shortcuts);

	swSetProfileStringList(SW_PROFILE_MOUSE_SHORTCUTS, Shortcuts);
}

void swResetDefaultMouseShortcuts()
{
	gMouseShortcuts.Reset();

	swRemoveProfileKey(SW_PROFILE_MOUSE_SHORTCUTS);
}

void swKeyboardShortcuts::Reset()
{
	for (int CommandIdx = 0; CommandIdx < SW_COMMAND_COUNT; CommandIdx++)
		mShortcuts[CommandIdx] = gCommands[CommandIdx].DefaultShortcut;
}

bool swKeyboardShortcuts::Save(const QString& FileName)
{
	QFile File(FileName);

	if (!File.open(QIODevice::WriteOnly))
		return false;

	QTextStream Stream(&File);

	return Save(Stream);
}

bool swKeyboardShortcuts::Save(QTextStream& Stream)
{
	for (int CommandIdx = 0; CommandIdx < SW_COMMAND_COUNT; CommandIdx++)
	{
		if (mShortcuts[CommandIdx].isEmpty())
			continue;

		Stream << gCommands[CommandIdx].ID << QLatin1String("=") << mShortcuts[CommandIdx] << QLatin1String("\n");
	}

	Stream.flush();

	return true;
}

bool swKeyboardShortcuts::Load(const QString& FileName)
{
	QFile File(FileName);

	if (!File.open(QIODevice::ReadOnly))
		return false;

	QTextStream Stream(&File);

	return Load(Stream);
}

bool swKeyboardShortcuts::Load(QTextStream& Stream)
{
	for (int CommandIdx = 0; CommandIdx < SW_COMMAND_COUNT; CommandIdx++)
		mShortcuts[CommandIdx].clear();

	for (QString Line = Stream.readLine(); !Line.isNull(); Line = Stream.readLine())
	{
		qsizetype Equals = Line.indexOf('=');

		if (Equals == -1)
			continue;

		QString Key = Line.left(Equals);

		int CommandIdx;
		for (CommandIdx = 0; CommandIdx < SW_COMMAND_COUNT; CommandIdx++)
			if (gCommands[CommandIdx].ID == Key)
				break;

		if (CommandIdx == SW_COMMAND_COUNT)
			continue;

		mShortcuts[CommandIdx] = Line.mid(Equals + 1);
	}

	return true;
}

void swMouseShortcuts::Clear()
{
	for (int ShortcutIndex = 0; ShortcutIndex < static_cast<int>(swTool::Count); ShortcutIndex++)
	{
		mShortcuts[ShortcutIndex].Modifiers1 = Qt::NoModifier;
		mShortcuts[ShortcutIndex].Button1 = Qt::NoButton;
		mShortcuts[ShortcutIndex].Modifiers2 = Qt::NoModifier;
		mShortcuts[ShortcutIndex].Button2 = Qt::NoButton;
	}
}

void swMouseShortcuts::Reset()
{
	Clear();

	swToolShortcut& RotateViewShortcut = mShortcuts[static_cast<int>(swTool::RotateView)];
	RotateViewShortcut.Modifiers1 = Qt::AltModifier;
	RotateViewShortcut.Button1 = Qt::LeftButton;
	RotateViewShortcut.Modifiers2 = Qt::NoModifier;
	RotateViewShortcut.Button2 = Qt::RightButton;

	swToolShortcut& PanShortcut = mShortcuts[static_cast<int>(swTool::Pan)];
	PanShortcut.Modifiers1 = Qt::AltModifier;
	PanShortcut.Button1 = Qt::MiddleButton;
	PanShortcut.Modifiers2 = Qt::ShiftModifier;
	PanShortcut.Button2 = Qt::RightButton;

	swToolShortcut& ZoomShortcut = mShortcuts[static_cast<int>(swTool::Zoom)];
	ZoomShortcut.Modifiers1 = Qt::AltModifier;
	ZoomShortcut.Button1 = Qt::RightButton;
}

bool swMouseShortcuts::Save(const QString& FileName)
{
	QStringList Shortcuts;

	if (!Save(Shortcuts))
		return false;

	QFile File(FileName);

	if (!File.open(QIODevice::WriteOnly))
		return false;

	QTextStream Stream(&File);

	for (const QString& Shortcut : std::as_const(Shortcuts))
		Stream << Shortcut << QLatin1String("\n");

	Stream.flush();

	return true;
}
bool swMouseShortcuts::Save(QStringList& Shortcuts)
{
	Shortcuts.clear();

	for (int ShortcutIndex = 0; ShortcutIndex < static_cast<int>(swTool::Count); ShortcutIndex++)
	{
		int ButtonIndex1 = 0;
		for (int Button1 = mShortcuts[ShortcutIndex].Button1; Button1; Button1 >>= 1)
			ButtonIndex1++;

		if (!ButtonIndex1)
			continue;

		QString Shortcut = QKeySequence(mShortcuts[ShortcutIndex].Modifiers1 | (Qt::Key_0 + ButtonIndex1)).toString(QKeySequence::PortableText);

		int ButtonIndex2 = 0;
		for (int Button2 = mShortcuts[ShortcutIndex].Button2; Button2; Button2 >>= 1)
			ButtonIndex2++;

		if (ButtonIndex2)
			Shortcut += ',' + QKeySequence(mShortcuts[ShortcutIndex].Modifiers2 | (Qt::Key_0 + ButtonIndex2)).toString(QKeySequence::PortableText);

		Shortcuts << QString::fromLatin1(gToolNames[ShortcutIndex]) + QLatin1String("=") + Shortcut;
	}

	return true;
}

bool swMouseShortcuts::Load(const QString& FileName)
{
	QFile File(FileName);

	if (!File.open(QIODevice::ReadOnly))
		return false;

	QTextStream Stream(&File);
	QStringList Lines;

	while (!Stream.atEnd())
		Lines += Stream.readLine();

	return Load(Lines);
}

bool swMouseShortcuts::Load(const QStringList& FullShortcuts)
{
	Clear();

	for (const QString& FullShortcut : FullShortcuts)
	{
		qsizetype Equals = FullShortcut.indexOf('=');

		if (Equals == -1)
			continue;

		QString Key = FullShortcut.left(Equals);

		int ToolIdx;
		for (ToolIdx = 0; ToolIdx < static_cast<int>(swTool::Count); ToolIdx++)
			if (Key == gToolNames[ToolIdx])
				break;

		if (ToolIdx == static_cast<int>(swTool::Count))
			continue;

		const QStringList Shortcuts = FullShortcut.mid(Equals + 1).split(',');
		bool AddedShortcut = false;

		for (const QString& Shortcut : Shortcuts)
		{
			QKeySequence KeySequence(Shortcut);

			if (KeySequence.isEmpty())
				continue;

			int ShortcutKey = KeySequence[0];
			Qt::KeyboardModifiers Modifiers = (Qt::KeyboardModifier)(ShortcutKey & Qt::KeyboardModifierMask);
			Qt::MouseButton Button = (Qt::MouseButton)(1 << ((ShortcutKey & ~Qt::KeyboardModifierMask) - Qt::Key_0 - 1));

			if (!AddedShortcut)
			{
				mShortcuts[ToolIdx].Modifiers1 = Modifiers;
				mShortcuts[ToolIdx].Button1 = Button;
				AddedShortcut = true;
			}
			else
			{
				mShortcuts[ToolIdx].Modifiers2 = Modifiers;
				mShortcuts[ToolIdx].Button2 = Button;
			}
		}
	}

	return true;
}

swTool swMouseShortcuts::GetTool(Qt::MouseButton Button, Qt::KeyboardModifiers Modifiers) const
{
	for (int ToolIdx = 0; ToolIdx < static_cast<int>(swTool::Count); ToolIdx++)
		if ((mShortcuts[ToolIdx].Button1 == Button && mShortcuts[ToolIdx].Modifiers1 == Modifiers) || (mShortcuts[ToolIdx].Button2 == Button && mShortcuts[ToolIdx].Modifiers2 == Modifiers))
			return (swTool)ToolIdx;

	return swTool::Count;
}
