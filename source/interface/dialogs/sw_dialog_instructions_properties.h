#pragma once

#include "sw_instructions.h"

class swCollapsibleWidgetButton;

// the length arrows move to the next whole increment so converted lengths line up with the unit
class swInstructionsLengthSpinBox : public QDoubleSpinBox
{
public:
	using QDoubleSpinBox::QDoubleSpinBox;

	void stepBy(int Steps) override
	{
		const double Step = singleStep();
		const double Increments = value() / Step;
		const double Base = Steps > 0 ? std::floor(Increments + 1e-6) : std::ceil(Increments - 1e-6);

		setValue((Base + Steps) * Step);
	}
};

// use to show the units of a length beside its input, pressing it overrides the page units for that input
class swInstructionsUnitButton : public QToolButton
{
	Q_OBJECT

public:
	swInstructionsUnitButton(QWidget* Parent, const QString& Name);

	swInstructionsPageUnits GetUnits(const swInstructions* Instructions) const;
	void UpdateUnits(const swInstructions* Instructions);

signals:
	void UnitsChanged();

protected:
	void ShowUnitsMenu();

	std::optional<swInstructionsPageUnits> mUnits;
};

class swInstructionsPropertiesWidget : public QDockWidget
{
	Q_OBJECT

public:
	swInstructionsPropertiesWidget(QWidget* Parent, swInstructions* Instructions);

	void AddOptionsAction(QAction* Action);
	void SelectionChanged(QGraphicsItem* FocusItem);
	void SetCurrentPage(int PageIndex);

protected:
	enum class CategoryIndex
	{
		Document,
		Cover,
		BillOfMaterials,
		PageLayout,
		Page,
		Step,
		PartsList,
		PageNumber,
		Count
	};

	enum class EditorType
	{
		Bool,
		Length,
		Choice,
		Color,
		Font
	};

	enum class FocusType
	{
		None,
		StepImage,
		StepNumber,
		PartsList,
		PageNumber
	};

	struct CategoryWidgets
	{
		swCollapsibleWidgetButton* Button = nullptr;
		QWidget* Rows = nullptr;
	};

	struct EditorWidgets
	{
		QWidget* Widget = nullptr;
		QLabel* Label = nullptr;
		swInstructionsUnitButton* UnitButton = nullptr;
		QDoubleSpinBox* SizeSpinBox = nullptr;
		EditorType Type = EditorType::Bool;
		QString Name;
		int Minimum = 0;
		int Maximum = 0;
	};

	void ShowPageSetupDialog();
	void CreateWidgets();
	void AddCategory(CategoryIndex Index, const QString& Title, const QString& WhatsThis);
	QWidget* AddRow(CategoryIndex Index, const std::vector<QWidget*>& Editors);
	QLabel* AddLabeledRow(CategoryIndex Index, const QString& Name, QWidget* Editor);
	QLabel* AddEditorRow(CategoryIndex Index, swInstructionsPropertyType Type);
	void RegisterEditor(swInstructionsPropertyType Type, EditorType Editor, QWidget* Widget, const QString& Name, const QString& ToolTip);
	QToolButton* CreateToggleEditor(swInstructionsPropertyType Type, const QString& Name, const QString& ToolTip);
	QDoubleSpinBox* CreateLengthEditor(swInstructionsPropertyType Type, const QString& Name, const QString& ToolTip, int Minimum, int Maximum);
	QComboBox* CreateChoiceEditor(swInstructionsPropertyType Type, const QString& Name, const QString& ToolTip, const QStringList& Choices);
	QToolButton* CreateColorEditor(swInstructionsPropertyType Type, const QString& Name, const QString& ToolTip);
	QToolButton* CreateFontEditor(swInstructionsPropertyType Type, const QString& Name, const QString& ToolTip);
	void ColorButtonClicked(swInstructionsPropertyType Type);
	void FontButtonClicked(swInstructionsPropertyType Type);
	void SetCategoriesExpanded(bool Expanded);
	int GetIntValue(swInstructionsPropertyType Type) const;
	swInstructionsPageUnits GetEditorUnits(swInstructionsPropertyType Type) const;
	QColor GetColorValue(swInstructionsPropertyType Type) const;
	void SetIntValue(swInstructionsPropertyType Type, int Value);
	void SetColorValue(swInstructionsPropertyType Type, const QColor& Color);
	void SetThumbnailPageScope(bool PageOnly);
	void DividerPresetClicked(int Preset);

	void UpdateWidget();
	void UpdateEditors();
	void UpdatePageLayout();
	void UpdateCategories();
	swInstructionsPageType GetPageType() const;

	static QIcon CreateColorIcon(const QColor& Color, int Size);
	static QIcon CreateDividerPresetIcon(int Rows, int Columns, const QPalette& Palette);

	swInstructions* mInstructions = nullptr;
	QWidget* mOptionsRow = nullptr;
	FocusType mFocus = FocusType::None;
	int mPageIndex = 0;
	swModel* mModel = nullptr;
	swStep mStep = 1;

	std::array<CategoryWidgets, static_cast<int>(CategoryIndex::Count)> mCategoryWidgets = {};
	std::array<EditorWidgets, static_cast<int>(swInstructionsPropertyType::Count)> mEditors;
	QSpinBox* mRowsSpinBox = nullptr;
	QSpinBox* mColumnsSpinBox = nullptr;
	QToolButton* mThumbnailPageButton = nullptr;
	QToolButton* mBackgroundPageButton = nullptr;
	std::vector<QToolButton*> mDividerPresetButtons;
	std::vector<QLabel*> mRowLabels;
	QVBoxLayout* mCategoriesLayout = nullptr;
};

// use to show a length stored in points in the page units
void swSetupLengthSpinBox(QDoubleSpinBox* SpinBox, swInstructionsPageUnits Units, double Minimum, double Maximum);
