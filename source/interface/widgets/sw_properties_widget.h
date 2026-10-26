#pragma once

#include "sw_object_property.h"
#include "sw_math.h"

class swCollapsibleWidgetButton;
class swDoubleSpinBox;
class swKeyFrameWidget;

class swPropertiesWidget : public QWidget
{
	Q_OBJECT;

public:
	swPropertiesWidget(QWidget* Parent);

	void Update(const std::vector<swObject*>& Selection, swObject* Focus);

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
		swCollapsibleWidgetButton* Button = nullptr;
		QString Title;
		std::vector<swObjectPropertyId> Properties;
		CategoryIndex Category;
	};

	struct PropertyWidgets
	{
		QWidget* Editor = nullptr;
		QWidget* Row = nullptr;
		swKeyFrameWidget* KeyFrame = nullptr;
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
		swObjectPropertyId KeyFrameId = swObjectPropertyId::Count;
	};

	swObjectPropertyId GetEditorWidgetPropertyId(QWidget* Widget) const;
	swObjectPropertyId GetKeyFrameWidgetPropertyId(QWidget* Widget) const;

	void AddCategory(CategoryIndex Index, const QString& Title, bool Collapsible);
	bool event(QEvent* Event) override;
	bool eventFilter(QObject* Object, QEvent* Event) override;
	void RegisterEditor(swObjectPropertyId PropertyId, QWidget* Editor, const QString& Name, const QString& ToolTip);
	swKeyFrameWidget* CreateKeyFrameWidget(swObjectPropertyId PropertyId);
	swDoubleSpinBox* CreateFloatEditor(swObjectPropertyId PropertyId, const QString& Name, const QString& ToolTip, float Min, float Max, bool ShowName, const QString& Suffix = QString());
	QSpinBox* CreateIntegerEditor(swObjectPropertyId PropertyId, const QString& Name, const QString& ToolTip, int Min, int Max);
	QLineEdit* CreateStepNumberEditor(swObjectPropertyId PropertyId, const QString& Name, const QString& ToolTip);
	QLineEdit* CreateStringEditor(swObjectPropertyId PropertyId, const QString& Name, const QString& ToolTip);
	QComboBox* CreateStringListEditor(swObjectPropertyId PropertyId, const QString& Name, const QString& ToolTip, const QStringList& Strings);
	QToolButton* CreateColorEditor(swObjectPropertyId PropertyId, const QString& Name, const QString& ToolTip);
	QToolButton* CreateToggleEditor(swObjectPropertyId PropertyId, const QString& Name, const QString& ToolTip);
	QToolButton* CreatePieceIdEditor(swObjectPropertyId PropertyId, const QString& Name, const QString& ToolTip);
	QToolButton* CreatePieceColorEditor(swObjectPropertyId PropertyId, const QString& Name, const QString& ToolTip);
	QWidget* AddValueRow(QWidget* Container, const QString& Letter, const QString& Name, const std::vector<RowEditor>& Editors);
	void AddVectorRow(QWidget* Container, const std::array<swObjectPropertyId, 3>& PropertyIds, const QString& Letter, const QString& Name, const QString& ToolTip, bool SupportsKeyFrames, float Min, float Max);
	void AddIdentityRow(IdentityIndex Index, QWidget* Editor, swObjectPropertyId KeyFrameId = swObjectPropertyId::Count);
	QToolButton* CreateRowNameButton(QWidget* Row, const QString& Letter, const QString& Name);
	void UpdateIdentityBlock(IdentityBlock& Block);
	void UpdateRowVisible(QWidget* Row);

	std::pair<QVariant, bool> GetUpdateValue(swObjectPropertyId PropertyId);

	void UpdateKeyFrameWidget(swObjectPropertyId PropertyId);
	void UpdateBool(swObjectPropertyId PropertyId);
	void UpdateFloat(swObjectPropertyId PropertyId, float Value);
	void UpdateInteger(swObjectPropertyId PropertyId);
	void UpdateStepNumber(swObjectPropertyId PropertyId, swStep Step, swStep Min, swStep Max);
	void UpdateString(swObjectPropertyId PropertyId);
	void UpdateStringList(swObjectPropertyId PropertyId);
	void UpdateColor(swObjectPropertyId PropertyId);
	void UpdatePieceColor(swObjectPropertyId PropertyId);
	void UpdatePieceId(swObjectPropertyId PropertyId);

	void ChangeFloatValue(swObjectPropertyId PropertyId, float Value);

	void SetEmpty();
	void SetPiece(const std::vector<swObject*>& Selection, swObject* Focus);
	void SetCamera(const std::vector<swObject*>& Selection, swObject* Focus);
	void SetLight(const std::vector<swObject*>& Selection, swObject* Focus);

	void CreateWidgets();
	void SetLayoutMode(LayoutMode Mode);
	void SetPropertyVisible(swObjectPropertyId PropertyId, bool Visible);
	void SetPropertyWidgetsVisible(swObjectPropertyId PropertyId, bool Visible);
	void SetCategoryVisible(CategoryIndex Index, bool Visible);
	void SetCategoryWidgetsVisible(CategoryWidgets& Category, bool Visible);

	std::vector<swObject*> mSelection;
	swObject* mFocusObject = nullptr;
	bool mDisableUpdates = false;
	std::optional<swVector3> mLastRotation;

	std::array<PropertyWidgets, static_cast<int>(swObjectPropertyId::Count)> mPropertyWidgets = {};
	std::array<CategoryWidgets, static_cast<int>(CategoryIndex::Count)> mCategoryWidgets = {};

	CategoryWidgets* mCurrentCategory = nullptr;
	QVBoxLayout* mSettingsLayout = nullptr;
	QWidget* mIdentityRows = nullptr;
	QWidget* mTransformRows = nullptr;
	QWidget* mStepRows = nullptr;
	std::array<IdentityBlock, static_cast<int>(IdentityIndex::Count)> mIdentityBlocks;

	LayoutMode mLayoutMode = LayoutMode::Count;
};
