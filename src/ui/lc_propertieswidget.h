#pragma once

#include "lc_objectproperty.h"
#include "lc_math.h"

class lcCollapsibleWidgetButton;
class lcDoubleSpinBox;
class lcKeyFrameWidget;

class lcPropertiesWidget : public QWidget
{
	Q_OBJECT;

public:
	lcPropertiesWidget(QWidget* Parent);

	void Update(const std::vector<lcObject*>& Selection, lcObject* Focus);

	QWidget* GetIdentityRowsWidget() const
	{
		return mIdentityRows;
	}

	QWidget* GetTransformRowsWidget() const
	{
		return mTransformRows;
	}

	QWidget* GetStepRowsWidget() const
	{
		return mStepRows;
	}

	bool HasVisibleRows() const;

signals:
	void LayoutChanged();

protected slots:
	void CategoryStateChanged(bool Expanded);
	void KeyFrameChanged();
	void BoolChanged();
	void FloatEditingStarted();
	void FloatEditingFinished();
	void FloatEditingCanceled();
	void FloatChanged(double Value);
	void IntegerChanged();
	void StepNumberChanged();
	void StringChanged();
	void StringListChanged(int Index);
	void ColorButtonClicked();
	void ColorChanged(QColor Color);
	void PieceColorButtonClicked();
	void PieceColorChanged(int ColorIndex);
	void PieceIdButtonClicked();

protected:
	enum class CategoryIndex
	{
		Piece,
		Camera,
		CameraTransform,
		Light,
		LightBlender,
		LightPOVRay,
		ObjectTransform,
		Count
	};

	enum class PropertyType
	{
		Bool,
		Float,
		Integer,
		StepNumber,
		String,
		StringList,
		Color,
		PieceColor,
		PieceId
	};

	enum class LayoutMode
	{
		Empty,
		Piece,
		Camera,
		Light,
		Multiple,
		Count
	};

	enum class IdentityIndex
	{
		Piece,
		Camera,
		Light,
		Count
	};

	struct CategoryWidgets
	{
		lcCollapsibleWidgetButton* Button = nullptr;
		QString Title;
		std::vector<lcObjectPropertyId> Properties;
		CategoryIndex Category;
	};

	struct PropertyWidgets
	{
		QWidget* Editor = nullptr;
		QWidget* Row = nullptr;
		lcKeyFrameWidget* KeyFrame = nullptr;
		CategoryIndex Category;
		bool Visible = true;
	};

	// Full width rows naming the object, with a block spanning them on the left that shows the color when there is one.
	struct IdentityBlock
	{
		std::vector<QWidget*> Slots;
		QToolButton* Swatch = nullptr;
		QColor Color;
	};

	// An editor in a row, followed by the keyframe button of a property when it is keyed on its own or for the row.
	struct RowEditor
	{
		QWidget* Editor;
		lcObjectPropertyId KeyFrameId = lcObjectPropertyId::Count;
	};

	lcObjectPropertyId GetEditorWidgetPropertyId(QWidget* Widget) const;
	lcObjectPropertyId GetKeyFrameWidgetPropertyId(QWidget* Widget) const;

	void AddCategory(CategoryIndex Index, const QString& Title, bool Collapsible);
	bool event(QEvent* Event) override;
	bool eventFilter(QObject* Object, QEvent* Event) override;
	void RegisterEditor(lcObjectPropertyId PropertyId, QWidget* Editor, const QString& Name, const QString& ToolTip);
	lcKeyFrameWidget* CreateKeyFrameWidget(lcObjectPropertyId PropertyId);
	lcDoubleSpinBox* CreateFloatEditor(lcObjectPropertyId PropertyId, const QString& Name, const QString& ToolTip, float Min, float Max, bool ShowName, const QString& Suffix = QString());
	QSpinBox* CreateIntegerEditor(lcObjectPropertyId PropertyId, const QString& Name, const QString& ToolTip, int Min, int Max);
	QLineEdit* CreateStepNumberEditor(lcObjectPropertyId PropertyId, const QString& Name, const QString& ToolTip);
	QLineEdit* CreateStringEditor(lcObjectPropertyId PropertyId, const QString& Name, const QString& ToolTip);
	QComboBox* CreateStringListEditor(lcObjectPropertyId PropertyId, const QString& Name, const QString& ToolTip, const QStringList& Strings);
	QToolButton* CreateColorEditor(lcObjectPropertyId PropertyId, const QString& Name, const QString& ToolTip);
	QToolButton* CreateToggleEditor(lcObjectPropertyId PropertyId, const QString& Name, const QString& ToolTip);
	QToolButton* CreatePieceIdEditor(lcObjectPropertyId PropertyId, const QString& Name, const QString& ToolTip);
	QToolButton* CreatePieceColorEditor(lcObjectPropertyId PropertyId, const QString& Name, const QString& ToolTip);
	QWidget* AddValueRow(QWidget* Container, const QString& Letter, const QString& Name, const std::vector<RowEditor>& Editors);
	void AddVectorRow(QWidget* Container, const std::array<lcObjectPropertyId, 3>& PropertyIds, const QString& Letter, const QString& Name, const QString& ToolTip, bool SupportsKeyFrames, float Min, float Max);
	void AddIdentityRow(IdentityIndex Index, QWidget* Editor, lcObjectPropertyId KeyFrameId = lcObjectPropertyId::Count);
	QToolButton* CreateRowNameButton(QWidget* Row, const QString& Letter, const QString& Name);
	void UpdateIdentityBlock(IdentityBlock& Block);
	void UpdateRowVisible(QWidget* Row);

	std::pair<QVariant, bool> GetUpdateValue(lcObjectPropertyId PropertyId);

	void UpdateKeyFrameWidget(lcObjectPropertyId PropertyId);
	void UpdateBool(lcObjectPropertyId PropertyId);
	void UpdateFloat(lcObjectPropertyId PropertyId, float Value);
	void UpdateInteger(lcObjectPropertyId PropertyId);
	void UpdateStepNumber(lcObjectPropertyId PropertyId, lcStep Step, lcStep Min, lcStep Max);
	void UpdateString(lcObjectPropertyId PropertyId);
	void UpdateStringList(lcObjectPropertyId PropertyId);
	void UpdateColor(lcObjectPropertyId PropertyId);
	void UpdatePieceColor(lcObjectPropertyId PropertyId);
	void UpdatePieceId(lcObjectPropertyId PropertyId);

	void ChangeFloatValue(lcObjectPropertyId PropertyId, float Value);

	void SetEmpty();
	void SetPiece(const std::vector<lcObject*>& Selection, lcObject* Focus);
	void SetCamera(const std::vector<lcObject*>& Selection, lcObject* Focus);
	void SetLight(const std::vector<lcObject*>& Selection, lcObject* Focus);

	void CreateWidgets();
	void SetLayoutMode(LayoutMode Mode);
	void SetPropertyVisible(lcObjectPropertyId PropertyId, bool Visible);
	void SetPropertyWidgetsVisible(lcObjectPropertyId PropertyId, bool Visible);
	void SetCategoryVisible(CategoryIndex Index, bool Visible);
	void SetCategoryWidgetsVisible(CategoryWidgets& Category, bool Visible);

	std::vector<lcObject*> mSelection;
	lcObject* mFocusObject = nullptr;
	bool mDisableUpdates = false;
	std::optional<lcVector3> mLastRotation;

	std::array<PropertyWidgets, static_cast<int>(lcObjectPropertyId::Count)> mPropertyWidgets = {};
	std::array<CategoryWidgets, static_cast<int>(CategoryIndex::Count)> mCategoryWidgets = {};

	CategoryWidgets* mCurrentCategory = nullptr;
	QVBoxLayout* mSettingsLayout = nullptr;
	QWidget* mIdentityRows = nullptr;
	QWidget* mTransformRows = nullptr;
	QWidget* mStepRows = nullptr;
	std::array<IdentityBlock, static_cast<int>(IdentityIndex::Count)> mIdentityBlocks;

	LayoutMode mLayoutMode = LayoutMode::Count;
};
