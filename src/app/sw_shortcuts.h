#pragma once

#include "sw_commands.h"

class swKeyboardShortcuts
{
public:
	void Reset();
	bool Save(const QString& FileName);
	bool Save(QTextStream& Stream);
	bool Load(const QString& FileName);
	bool Load(QTextStream& Stream);

	QString mShortcuts[SW_NUM_COMMANDS];
};

extern swKeyboardShortcuts gKeyboardShortcuts;

void swLoadDefaultKeyboardShortcuts();
void swSaveDefaultKeyboardShortcuts();
void swResetDefaultKeyboardShortcuts();

class swMouseShortcuts
{
public:
	void Reset();
	bool Save(const QString& FileName);
	bool Save(QStringList& Shortcuts);
	bool Load(const QString& FileName);
	bool Load(const QStringList& Shortcuts);

	swTool GetTool(Qt::MouseButton Button, Qt::KeyboardModifiers Modifiers) const;

	struct swToolShortcut
	{
		Qt::KeyboardModifiers Modifiers1;
		Qt::MouseButton Button1;
		Qt::KeyboardModifiers Modifiers2;
		Qt::MouseButton Button2;
	};

	swToolShortcut mShortcuts[static_cast<int>(swTool::Count)];

private:
	void Clear();
};

extern swMouseShortcuts gMouseShortcuts;

void swLoadDefaultMouseShortcuts();
void swSaveDefaultMouseShortcuts();
void swResetDefaultMouseShortcuts();

