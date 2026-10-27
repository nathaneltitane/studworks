#pragma once

#include "sw_application.h"
#include "sw_shortcuts.h"
#include "sw_category.h"

struct swPreferencesDialogOptions
{
	swPreferences Preferences;

	QString LibraryPath;
	QString ColorConfigPath;
	QString MinifigureBuilderSettingsPath;
	QString POVRayPath;
	QString LGEOPath;
	QString DefaultAuthor;
	QString Language;

	int AASamples;
	swStudStyle StudStyle;

	std::vector<swLibraryCategory> Categories;
	bool CategoriesModified;
	bool CategoriesDefault;

	swKeyboardShortcuts KeyboardShortcuts;
	bool KeyboardShortcutsModified;
	bool KeyboardShortcutsDefault;

	swMouseShortcuts MouseShortcuts;
	bool MouseShortcutsModified;
	bool MouseShortcutsDefault;
};

namespace Ui
{
class swPreferencesDialog;
}

class swPreferencesDialog : public QDialog
{
	Q_OBJECT

public:
	swPreferencesDialog(QWidget* Parent, swPreferencesDialogOptions* Options);
	~swPreferencesDialog();

	swPreferencesDialogOptions* mOptions;

	enum
	{
		CategoryRole = Qt::UserRole
	};

	bool eventFilter(QObject* Object, QEvent* Event) override;

public slots:
	void accept() override;

private slots:
	void PiecesLibraryBrowseClicked();
	void PiecesArchiveBrowseClicked();
	void ColorConfigBrowseButtonClicked();
	void MinifigureBuilderSettingsBrowseButtonClicked();
	void PovrayExecutableBrowseClicked();
	void LgeoPathBrowseClicked();
	void ColorThemeCurrentIndexChanged(int Index);
	void ColorButtonClicked();
	void AutomateEdgeColor();
	void AutomateEdgeColorToggled();
	void EdgeColorSliderValueChanged();
	void BlenderAddonSettingsButtonClicked();
	void StudStyleComboCurrentIndexChanged(int Index);
	void AntiAliasingToggled();
	void EdgeLinesToggled();
	void ConditionalLinesCheckBoxToggled();
	void LineWidthSliderValueChanged();
	void RenderTargetChanged();
	void MeshLODSliderValueChanged();
	void FadeStepsToggled();
	void HighlightNewPiecesToggled();
	void GridStudsToggled();
	void GridLinesToggled();
	void ViewSphereSizeComboCurrentIndexChanged(int Index);
	void PreviewViewSphereSizeComboCurrentIndexChanged(int Index);
	void UpdatePieces();
	void CategoriesDropped(const QModelIndex& Parent, int First, int Last);
	void NewCategoryClicked();
	void EditCategoryClicked();
	void DeleteCategoryClicked();
	void ImportCategoriesClicked();
	void ExportCategoriesClicked();
	void ResetCategoriesClicked();
	void ShortcutAssignClicked();
	void ShortcutRemoveClicked();
	void ShortcutsImportClicked();
	void ShortcutsExportClicked();
	void ShortcutsResetClicked();
	void CommandChanged(QTreeWidgetItem* Current);
	void KeyboardFilterEditTextEdited(const QString& Text);
	void MouseAssignClicked();
	void MouseRemoveClicked();
	void MouseImportButtonClicked();
	void MouseExportButtonClicked();
	void MouseResetClicked();
	void MouseTreeItemChanged(QTreeWidgetItem* Current);
	void MouseFilterEditTextEdited(const QString& Text);

private:
	Ui::swPreferencesDialog* ui;

	void UpdateCategories();
	void UpdateCommandList();
	void UpdateMouseTree();
	void UpdateMouseTreeItem(int ItemIndex);
	QTreeWidgetItem* FindMouseTreeItem(int ItemIndex) const;
	void SetShortcutModified(QTreeWidgetItem* TreeItem, bool Modified);

	struct swRenderTargetWidgets
	{
		QCheckBox* SyncRendering;
		QLabel* ShadingModeLabel;
		QComboBox* ShadingMode;
		QCheckBox* EdgeLines;
		QCheckBox* ConditionalLines;
		QLabel* LineWidthTitle;
		QSlider* LineWidthSlider;
		QLabel* LineWidthLabel;
	};

	swRenderTargetWidgets GetRenderTargetWidgets(swRenderTarget Target) const;

	Qt::MouseButton mMouseShortcutButton = Qt::NoButton;
	Qt::KeyboardModifiers mMouseShortcutModifiers = Qt::NoModifier;
	float mLineWidthRange[2];
	float mLineWidthGranularity;
	static constexpr float mMeshLODMultiplier = 25.0f;
};
