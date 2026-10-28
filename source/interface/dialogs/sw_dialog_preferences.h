#pragma once

#include "sw_application.h"
#include "sw_shortcuts.h"
#include "sw_category.h"
#include "sw_instructions.h"

// the default page setup and grid of new instructions and the options of the instructions window
struct swInstructionsPreferences
{
	swInstructionsPageSetup PageSetup;
	int PageUnits;
	int MarginUnits;
	bool ShowGrid;
	bool SnapToGrid;
	float GridSize;
	int GridDivisions;
	bool GridWithinMargins;
	bool NewModelDefaults;
	bool ShowMargins;
	QColor GridPrimaryColor;
	QColor GridSecondaryColor;
	QColor MarginColor;
	bool EditorHighlight;
	bool EditorNumbers;
	bool EditorWrap;
	bool EditorMarkers;

	void Load();
	void Save() const;
};

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

	swInstructionsPreferences Instructions;

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

protected:
	void showEvent(QShowEvent* Event) override;

public slots:
	void accept() override;

private slots:
	void PartsLibraryBrowseClicked();
	void PartsArchiveBrowseClicked();
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
	void CutawayShadeTransparencyChanged();
	void RenderTargetChanged();
	void MeshLODSliderValueChanged();
	void FadeStepsToggled();
	void HighlightNewPartsToggled();
	void GridStudsToggled();
	void GridLinesToggled();
	void ViewCubeSizeComboCurrentIndexChanged(int Index);
	void PreviewViewCubeSizeComboCurrentIndexChanged(int Index);
	void AxisIconSizeComboCurrentIndexChanged(int Index);
	void UpdateParts();
	void CategoriesDropped(const QModelIndex& Parent, int First, int Last);
	void CategoryItemChanged(QTreeWidgetItem* CategoryItem, int Column);
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
	void SnapCategoriesTree();
	void UpdateCommandList();
	void UpdateMouseTree();
	void UpdateMouseTreeItem(int ItemIndex);
	QTreeWidgetItem* FindMouseTreeItem(int ItemIndex) const;
	static void UpdateStudSuffix(QSpinBox* SpinBox);
	void SetShortcutModified(QTreeWidgetItem* TreeItem, bool Modified);
	void CreateInstructionsTab();
	void UpdateInstructionsPage();

	QPushButton* mInstructionsPageButton = nullptr;
	QCheckBox* mInstructionsShowGrid = nullptr;
	QCheckBox* mInstructionsSnapToGrid = nullptr;
	QDoubleSpinBox* mInstructionsGridSize = nullptr;
	QComboBox* mInstructionsGridScope = nullptr;
	QSpinBox* mInstructionsGridDivisions = nullptr;
	QToolButton* mInstructionsGridPrimaryColor = nullptr;
	QToolButton* mInstructionsGridSecondaryColor = nullptr;
	QToolButton* mInstructionsMarginColor = nullptr;
	QCheckBox* mInstructionsNewModelDefaults = nullptr;
	QCheckBox* mInstructionsShowMargins = nullptr;
	QCheckBox* mInstructionsEditorHighlight = nullptr;
	QCheckBox* mInstructionsEditorNumbers = nullptr;
	QCheckBox* mInstructionsEditorWrap = nullptr;
	QCheckBox* mInstructionsEditorMarkers = nullptr;

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
