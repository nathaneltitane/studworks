#include "sw_global.h"
#include "sw_dialog_preferences.h"
#include "ui_sw_dialog_preferences.h"
#include "sw_library.h"

// mouse shortcuts read like keys
static QString swGetMouseShortcutText(Qt::MouseButton Button, Qt::KeyboardModifiers Modifiers)
{
	QString Shortcut = QKeySequence(Modifiers).toString(QKeySequence::NativeText);

	switch (Button)
	{
	case Qt::LeftButton:
		Shortcut += swPreferencesDialog::tr("Left Button");
		break;

	case Qt::MiddleButton:
		Shortcut += swPreferencesDialog::tr("Middle Button");
		break;

	case Qt::RightButton:
		Shortcut += swPreferencesDialog::tr("Right Button");
		break;

	default:
		Shortcut.clear();
	}

	return Shortcut;
}

bool swPreferencesDialog::eventFilter(QObject* Object, QEvent* Event)
{
	if (Object == ui->splitter->handle(1) && Event->type() == QEvent::MouseButtonRelease)
		QTimer::singleShot(0, this, &swPreferencesDialog::SnapCategoriesTree);

	// record the clicked button
	if (Object == ui->MouseShortcutEdit)
	{
		if (Event->type() == QEvent::MouseButtonPress || Event->type() == QEvent::MouseButtonDblClick)
		{
			QMouseEvent* MouseEvent = static_cast<QMouseEvent*>(Event);
			const Qt::MouseButton Button = MouseEvent->button();

			if (Button == Qt::LeftButton || Button == Qt::MiddleButton || Button == Qt::RightButton)
			{
				mMouseShortcutButton = Button;
				mMouseShortcutModifiers = MouseEvent->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier);
				ui->MouseShortcutEdit->setText(swGetMouseShortcutText(mMouseShortcutButton, mMouseShortcutModifiers));
			}

			ui->MouseShortcutEdit->setFocus(Qt::MouseFocusReason);
			MouseEvent->accept();

			return true;
		}

		if (Event->type() == QEvent::MouseButtonRelease || Event->type() == QEvent::MouseMove || Event->type() == QEvent::ContextMenu)
			return true;

		return QDialog::eventFilter(Object, Event);
	}

	if (Event->type() == QEvent::KeyPress)
	{
		QKeyEvent* KeyEvent = static_cast<QKeyEvent*>(Event);

		int NextKey = KeyEvent->key();

		if (NextKey == Qt::Key_Control || NextKey == Qt::Key_Shift || NextKey == Qt::Key_Meta || NextKey == Qt::Key_Alt)
			return true;

		Qt::KeyboardModifiers State = KeyEvent->modifiers();
		QString Text = QKeySequence(NextKey).toString();

		if ((State & Qt::ShiftModifier) && (Text.isEmpty() || !Text.at(0).isPrint() || Text.at(0).isLetter() || Text.at(0).isSpace()))
			NextKey |= Qt::SHIFT;
		if (State & Qt::ControlModifier)
			NextKey |= Qt::CTRL;
		if (State & Qt::MetaModifier)
			NextKey |= Qt::META;
		if (State & Qt::AltModifier)
			NextKey |= Qt::ALT;

		QKeySequence KeySequence(NextKey);
		ui->shortcutEdit->setText(KeySequence.toString(QKeySequence::NativeText));
		KeyEvent->accept();

		return true;
	}

	if (Event->type() == QEvent::Shortcut || Event->type() == QEvent::KeyRelease || Event->type() == QEvent::ShortcutOverride)
	{
		Event->accept();
		return true;
	}

	return QDialog::eventFilter(Object, Event);
}

void swPreferencesDialog::UpdateCommandList()
{
	ui->commandList->clear();
	QMap<QString, QTreeWidgetItem*> Sections;

	for (unsigned int ActionIndex = 0; ActionIndex < SW_COMMAND_COUNT; ActionIndex++)
	{
		if (!gCommands[ActionIndex].ID[0])
			continue;

		const QString Identifier = tr(gCommands[ActionIndex].ID);

		qsizetype Pos = Identifier.indexOf(QLatin1Char('.'));
		qsizetype SubPos = Identifier.indexOf(QLatin1Char('.'), Pos + 1);

		if (SubPos == -1)
			SubPos = Pos;

		const QString ParentSection = Identifier.left(Pos);

		if (SubPos != Pos)
		{
			if (!Sections.contains(ParentSection))
			{
				QTreeWidgetItem* CategoryItem = new QTreeWidgetItem(ui->commandList, QStringList(ParentSection));
				QFont Font = CategoryItem->font(0);
				Font.setBold(true);
				CategoryItem->setFont(0, Font);
				Sections.insert(ParentSection, CategoryItem);
				ui->commandList->expandItem(CategoryItem);
			}
		}

		const QString Section = Identifier.left(SubPos);
		const QString SubId = Identifier.mid(SubPos + 1);

		if (!Sections.contains(Section))
		{
			QTreeWidgetItem* Parent = Sections[ParentSection];
			QTreeWidgetItem* CategoryItem;
			QString SubSection;

			if (Pos != SubPos)
				SubSection = Identifier.mid(Pos + 1, SubPos - Pos - 1);
			else
				SubSection = Section;

			if (Parent)
				CategoryItem = new QTreeWidgetItem(Parent, QStringList(SubSection));
			else
				CategoryItem = new QTreeWidgetItem(ui->commandList, QStringList(SubSection));

			QFont Font = CategoryItem->font(0);
			Font.setBold(true);
			CategoryItem->setFont(0, Font);
			Sections.insert(Section, CategoryItem);
			ui->commandList->expandItem(CategoryItem);
		}

		QTreeWidgetItem* Item = new QTreeWidgetItem;
		QKeySequence Sequence(mOptions->KeyboardShortcuts.mShortcuts[ActionIndex]);
		Item->setText(0, qApp->translate("Menu", gCommands[ActionIndex].MenuName).remove('&').remove(QLatin1String("...")));
		Item->setText(1, Sequence.toString(QKeySequence::NativeText));
		Item->setData(0, Qt::UserRole, QVariant::fromValue(ActionIndex));

		if (mOptions->KeyboardShortcuts.mShortcuts[ActionIndex] != gCommands[ActionIndex].DefaultShortcut)
			SetShortcutModified(Item, true);

		Sections[Section]->addChild(Item);
	}
}

void swPreferencesDialog::SetShortcutModified(QTreeWidgetItem* TreeItem, bool Modified)
{
	QFont Font = TreeItem->font(0);
	Font.setItalic(Modified);
	TreeItem->setFont(0, Font);
	Font.setBold(Modified);

	for (int Column = 1; Column < TreeItem->columnCount(); Column++)
		TreeItem->setFont(Column, Font);
}

void swPreferencesDialog::CommandChanged(QTreeWidgetItem* Current)
{
	if (!Current || !Current->data(0, Qt::UserRole).isValid())
	{
		ui->shortcutEdit->setText(QString());
		ui->shortcutGroup->setEnabled(false);
		return;
	}

	ui->shortcutGroup->setEnabled(true);

	int ShortcutIndex = qvariant_cast<int>(Current->data(0, Qt::UserRole));
	QKeySequence Key(mOptions->KeyboardShortcuts.mShortcuts[ShortcutIndex]);
	ui->shortcutEdit->setText(Key.toString(QKeySequence::NativeText));
}

// filter the tree items
static void swFilterShortcutTree(QTreeWidget* TreeWidget, const QString& Text)
{
	if (Text.isEmpty())
	{
		std::function<void(QTreeWidgetItem*)> ShowItems = [&ShowItems](QTreeWidgetItem* ParentItem)
		{
			for (int ChildIdx = 0; ChildIdx < ParentItem->childCount(); ChildIdx++)
				ShowItems(ParentItem->child(ChildIdx));

			ParentItem->setHidden(false);
		};

		ShowItems(TreeWidget->invisibleRootItem());
	}
	else
	{
		std::function<bool(QTreeWidgetItem*,bool)> ShowItems = [&ShowItems, &Text, TreeWidget](QTreeWidgetItem* ParentItem, bool ForceVisible)
		{
			for (int Column = 0; Column < TreeWidget->columnCount(); Column++)
				ForceVisible |= ParentItem->text(Column).contains(Text, Qt::CaseInsensitive);

			bool Visible = ForceVisible;

			for (int ChildIdx = 0; ChildIdx < ParentItem->childCount(); ChildIdx++)
				Visible |= ShowItems(ParentItem->child(ChildIdx), ForceVisible);

			ParentItem->setHidden(!Visible);

			return Visible;
		};

		ShowItems(TreeWidget->invisibleRootItem(), false);
	}
}

void swPreferencesDialog::KeyboardFilterEditTextEdited(const QString& Text)
{
	swFilterShortcutTree(ui->commandList, Text);
}

void swPreferencesDialog::ShortcutAssignClicked()
{
	QTreeWidgetItem* CurrentItem = ui->commandList->currentItem();

	if (!CurrentItem || !CurrentItem->data(0, Qt::UserRole).isValid())
		return;

	uint ShortcutIndex = CurrentItem->data(0, Qt::UserRole).toUInt();
	QString (&Shortcuts)[SW_COMMAND_COUNT] = mOptions->KeyboardShortcuts.mShortcuts;

	if (ShortcutIndex >= SW_ARRAY_COUNT(Shortcuts))
		return;

	QString NewShortcut = ui->shortcutEdit->text();

	if (!NewShortcut.isEmpty())
	{
		for (uint ExistingIndex = 0; ExistingIndex < SW_ARRAY_COUNT(Shortcuts); ExistingIndex++)
		{
			QKeySequence Sequence(Shortcuts[ExistingIndex]);

			if (NewShortcut == Sequence.toString(QKeySequence::NativeText) && ExistingIndex != ShortcutIndex)
			{
				QString ActionText = qApp->translate("Menu", gCommands[ExistingIndex].MenuName).remove('&').remove(QLatin1String("..."));
				QString QuestionText = tr("The shortcut '%1' is already assigned to '%2'. Do you want to replace it?").arg(NewShortcut, ActionText);

				if (QMessageBox::question(this, tr("Override Shortcut"), QuestionText, QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
					return;

				mOptions->KeyboardShortcuts.mShortcuts[ExistingIndex].clear();

				std::function<QTreeWidgetItem* (QTreeWidgetItem*)> FindItem = [&FindItem, ExistingIndex](QTreeWidgetItem* ParentItem) -> QTreeWidgetItem*
				{
					for (int ChildIdx = 0; ChildIdx < ParentItem->childCount(); ChildIdx++)
					{
						QTreeWidgetItem* ChildItem = ParentItem->child(ChildIdx);
						uint ChildIndex = ChildItem->data(0, Qt::UserRole).toUInt();

						if (ChildIndex == ExistingIndex)
							return ChildItem;

						QTreeWidgetItem* ExistingItem = FindItem(ChildItem);

						if (ExistingItem)
							return ExistingItem;
					}

					return nullptr;
				};

				QTreeWidgetItem* ExistingItem = FindItem(ui->commandList->invisibleRootItem());

				if (ExistingItem)
				{
					ExistingItem->setText(1, QString());
					SetShortcutModified(ExistingItem, gCommands[ShortcutIndex].DefaultShortcut[0] != 0);
				}
			}
		}
	}

	mOptions->KeyboardShortcuts.mShortcuts[ShortcutIndex] = NewShortcut;
	CurrentItem->setText(1, NewShortcut);

	SetShortcutModified(CurrentItem, mOptions->KeyboardShortcuts.mShortcuts[ShortcutIndex] != gCommands[ShortcutIndex].DefaultShortcut);

	mOptions->KeyboardShortcutsModified = true;
	mOptions->KeyboardShortcutsDefault = false;
}

void swPreferencesDialog::ShortcutRemoveClicked()
{
	ui->shortcutEdit->setText(QString());

	ShortcutAssignClicked();
}

void swPreferencesDialog::ShortcutsImportClicked()
{
	QString FileName = QFileDialog::getOpenFileName(this, tr("Import Shortcuts"), "", tr("Text Files (*.txt);;All Files (*.*)"));

	if (FileName.isEmpty())
		return;

	swKeyboardShortcuts Shortcuts;
	if (!Shortcuts.Load(FileName))
	{
		QMessageBox::warning(this, tr("Import Keyboard Shortcuts"), tr("Error loading keyboard shortcuts file."));
		return;
	}

	mOptions->KeyboardShortcuts = Shortcuts;

	mOptions->KeyboardShortcutsModified = true;
	mOptions->KeyboardShortcutsDefault = false;
}

void swPreferencesDialog::ShortcutsExportClicked()
{
	QString FileName = QFileDialog::getSaveFileName(this, tr("Export Shortcuts"), "", tr("Text Files (*.txt);;All Files (*.*)"));

	if (FileName.isEmpty())
		return;

	if (!mOptions->KeyboardShortcuts.Save(FileName))
	{
		QMessageBox::warning(this, tr("Export Keyboard Shortcuts"), tr("Error saving keyboard shortcuts file."));
		return;
	}
}

void swPreferencesDialog::ShortcutsResetClicked()
{
	if (QMessageBox::question(this, tr("Reset Keyboard Shortcuts"), tr("Are you sure you want to load the default keyboard shortcuts?"), QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
		return;

	mOptions->KeyboardShortcuts.Reset();
	UpdateCommandList();

	mOptions->KeyboardShortcutsModified = true;
	mOptions->KeyboardShortcutsDefault = true;
}

// group mouse tools like commands
static QString swGetToolText(int ToolIndex)
{
	return qApp->translate("Menu", gCommands[SW_EDIT_ACTION_FIRST + ToolIndex].MenuName).remove('&').remove(QLatin1String("..."));
}

void swPreferencesDialog::UpdateMouseTree()
{
	ui->mouseTree->clear();

	struct swToolCategory
	{
		const char* Name;
		swTool LastTool;
	};

	constexpr swToolCategory Categories[] =
	{
		{ QT_TRANSLATE_NOOP("swPreferencesDialog", "Insert"), swTool::Camera },
		{ QT_TRANSLATE_NOOP("swPreferencesDialog", "Edit"), swTool::ColorPicker },
		{ QT_TRANSLATE_NOOP("swPreferencesDialog", "View"), swTool::ZoomRegion }
	};

	int ToolIdx = 0;

	for (const swToolCategory& Category : Categories)
	{
		QTreeWidgetItem* CategoryItem = new QTreeWidgetItem(ui->mouseTree, QStringList(tr(Category.Name)));
		QFont Font = CategoryItem->font(0);
		Font.setBold(true);
		CategoryItem->setFont(0, Font);

		for (; ToolIdx <= static_cast<int>(Category.LastTool); ToolIdx++)
		{
			QTreeWidgetItem* Item = new QTreeWidgetItem(CategoryItem, QStringList(swGetToolText(ToolIdx)));
			Item->setData(0, Qt::UserRole, QVariant::fromValue(ToolIdx));
			UpdateMouseTreeItem(ToolIdx);
		}

		ui->mouseTree->expandItem(CategoryItem);
	}

	MouseFilterEditTextEdited(ui->MouseFilterEdit->text());
}

QTreeWidgetItem* swPreferencesDialog::FindMouseTreeItem(int ItemIndex) const
{
	for (QTreeWidgetItemIterator It(ui->mouseTree); *It; ++It)
	{
		const QVariant ItemData = (*It)->data(0, Qt::UserRole);

		if (ItemData.isValid() && ItemData.toInt() == ItemIndex)
			return *It;
	}

	return nullptr;
}

void swPreferencesDialog::UpdateMouseTreeItem(int ItemIndex)
{

	QTreeWidgetItem* Item = FindMouseTreeItem(ItemIndex);

	if (!Item)
		return;

	const swMouseShortcuts::swToolShortcut& Shortcut = mOptions->MouseShortcuts.mShortcuts[ItemIndex];
	Item->setText(1, swGetMouseShortcutText(Shortcut.Button1, Shortcut.Modifiers1));
	Item->setText(2, swGetMouseShortcutText(Shortcut.Button2, Shortcut.Modifiers2));

	swMouseShortcuts DefaultShortcuts;
	DefaultShortcuts.Reset();
	const swMouseShortcuts::swToolShortcut& Default = DefaultShortcuts.mShortcuts[ItemIndex];
	SetShortcutModified(Item, Shortcut.Button1 != Default.Button1 || Shortcut.Modifiers1 != Default.Modifiers1 || Shortcut.Button2 != Default.Button2 || Shortcut.Modifiers2 != Default.Modifiers2);
}

void swPreferencesDialog::MouseAssignClicked()
{
	QTreeWidgetItem* Current = ui->mouseTree->currentItem();

	if (!Current || !Current->data(0, Qt::UserRole).isValid())
		return;

	const int ItemIndex = Current->data(0, Qt::UserRole).toInt();
	const Qt::MouseButton Button = mMouseShortcutButton;
	const Qt::KeyboardModifiers Modifiers = mMouseShortcutModifiers;

	if (Button != Qt::NoButton)
	{
		for (int ToolIdx = 0; ToolIdx < static_cast<int>(swTool::Count); ToolIdx++)
		{
			if (ToolIdx == ItemIndex)
				continue;

			if (mOptions->MouseShortcuts.mShortcuts[ToolIdx].Button2 == Button && mOptions->MouseShortcuts.mShortcuts[ToolIdx].Modifiers2 == Modifiers)
			{
				if (QMessageBox::question(this, tr("Override Shortcut"), tr("This shortcut is already assigned to '%1'. Do you want to replace it?").arg(swGetToolText(ToolIdx)), QMessageBox::Yes | QMessageBox::No) == QMessageBox::No)
					return;

				mOptions->MouseShortcuts.mShortcuts[ToolIdx].Button2 = Qt::NoButton;
				mOptions->MouseShortcuts.mShortcuts[ToolIdx].Modifiers2 = Qt::NoModifier;
			}

			if (mOptions->MouseShortcuts.mShortcuts[ToolIdx].Button1 == Button && mOptions->MouseShortcuts.mShortcuts[ToolIdx].Modifiers1 == Modifiers)
			{
				if (QMessageBox::question(this, tr("Override Shortcut"), tr("This shortcut is already assigned to '%1'. Do you want to replace it?").arg(swGetToolText(ToolIdx)), QMessageBox::Yes | QMessageBox::No) == QMessageBox::No)
					return;

				mOptions->MouseShortcuts.mShortcuts[ToolIdx].Button1 = mOptions->MouseShortcuts.mShortcuts[ToolIdx].Button2;
				mOptions->MouseShortcuts.mShortcuts[ToolIdx].Modifiers1 = mOptions->MouseShortcuts.mShortcuts[ToolIdx].Modifiers2;
				mOptions->MouseShortcuts.mShortcuts[ToolIdx].Button2 = Qt::NoButton;
				mOptions->MouseShortcuts.mShortcuts[ToolIdx].Modifiers2 = Qt::NoModifier;

				UpdateMouseTreeItem(ToolIdx);
			}
		}
	}

	mOptions->MouseShortcuts.mShortcuts[ItemIndex].Button2 = mOptions->MouseShortcuts.mShortcuts[ItemIndex].Button1;
	mOptions->MouseShortcuts.mShortcuts[ItemIndex].Modifiers2 = mOptions->MouseShortcuts.mShortcuts[ItemIndex].Modifiers1;
	mOptions->MouseShortcuts.mShortcuts[ItemIndex].Button1 = Button;
	mOptions->MouseShortcuts.mShortcuts[ItemIndex].Modifiers1 = Modifiers;

	mOptions->MouseShortcutsModified = true;
	mOptions->MouseShortcutsDefault = false;

	UpdateMouseTreeItem(ItemIndex);
}

void swPreferencesDialog::MouseRemoveClicked()
{
	QTreeWidgetItem* Current = ui->mouseTree->currentItem();

	if (!Current || !Current->data(0, Qt::UserRole).isValid())
		return;

	const int ItemIndex = Current->data(0, Qt::UserRole).toInt();
	mOptions->MouseShortcuts.mShortcuts[ItemIndex].Button1 = mOptions->MouseShortcuts.mShortcuts[ItemIndex].Button2;
	mOptions->MouseShortcuts.mShortcuts[ItemIndex].Modifiers1 = mOptions->MouseShortcuts.mShortcuts[ItemIndex].Modifiers2;
	mOptions->MouseShortcuts.mShortcuts[ItemIndex].Button2 = Qt::NoButton;
	mOptions->MouseShortcuts.mShortcuts[ItemIndex].Modifiers2 = Qt::NoModifier;

	mOptions->MouseShortcutsModified = true;
	mOptions->MouseShortcutsDefault = false;

	UpdateMouseTreeItem(ItemIndex);
	MouseTreeItemChanged(Current);
}

void swPreferencesDialog::MouseImportButtonClicked()
{
	QString FileName = QFileDialog::getOpenFileName(this, tr("Import Shortcuts"), "", tr("Text Files (*.txt);;All Files (*.*)"));

	if (FileName.isEmpty())
		return;

	swMouseShortcuts Shortcuts;
	if (!Shortcuts.Load(FileName))
	{
		QMessageBox::warning(this, tr("Import Mouse Shortcuts"), tr("Error loading mouse shortcuts file."));
		return;
	}

	mOptions->MouseShortcuts = Shortcuts;
	UpdateMouseTree();

	mOptions->MouseShortcutsModified = true;
	mOptions->MouseShortcutsDefault = false;
}

void swPreferencesDialog::MouseExportButtonClicked()
{
	QString FileName = QFileDialog::getSaveFileName(this, tr("Export Shortcuts"), "", tr("Text Files (*.txt);;All Files (*.*)"));

	if (FileName.isEmpty())
		return;

	if (!mOptions->MouseShortcuts.Save(FileName))
		QMessageBox::warning(this, tr("Export Mouse Shortcuts"), tr("Error saving mouse shortcuts file."));
}

void swPreferencesDialog::MouseResetClicked()
{
	if (QMessageBox::question(this, tr("Reset Mouse Shortcuts"), tr("Are you sure you want to load the default mouse shortcuts?"), QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
		return;

	mOptions->MouseShortcuts.Reset();
	UpdateMouseTree();

	mOptions->MouseShortcutsModified = true;
	mOptions->MouseShortcutsDefault = true;
}

void swPreferencesDialog::MouseTreeItemChanged(QTreeWidgetItem* Current)
{
	if (!Current || !Current->data(0, Qt::UserRole).isValid())
	{
		ui->MouseShortcutGroup->setEnabled(false);
		return;
	}

	ui->MouseShortcutGroup->setEnabled(true);

	const int ToolIndex = Current->data(0, Qt::UserRole).toInt();

	mMouseShortcutButton = mOptions->MouseShortcuts.mShortcuts[ToolIndex].Button1;
	mMouseShortcutModifiers = mOptions->MouseShortcuts.mShortcuts[ToolIndex].Modifiers1;
	ui->MouseShortcutEdit->setText(swGetMouseShortcutText(mMouseShortcutButton, mMouseShortcutModifiers));
}

void swPreferencesDialog::MouseFilterEditTextEdited(const QString& Text)
{
	swFilterShortcutTree(ui->mouseTree, Text);
}
