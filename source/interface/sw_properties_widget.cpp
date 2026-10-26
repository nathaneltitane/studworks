#include "sw_global.h"
#include "sw_properties_widget.h"
#include "sw_keyframe_widget.h"
#include "sw_double_spin_box.h"
#include "sw_object.h"
#include "sw_piece.h"
#include "sw_camera.h"
#include "sw_light.h"
#include "sw_piece_data.h"
#include "sw_main_window.h"
#include "sw_collapsible_widget.h"
#include "sw_color_picker.h"
#include "sw_utilities.h"
#include "sw_part_selection_popup.h"
#include "sw_model_history.h"

swPropertiesWidget::swPropertiesWidget(QWidget* Parent)
	: QWidget(Parent)
{
	CreateWidgets();

	for (PropertyWidgets& Property : mPropertyWidgets)
	{
		if (!Property.Editor)
			continue;

		Property.Editor->setWhatsThis(Property.Editor->toolTip());

		if (Property.KeyFrame)
		{
			Property.KeyFrame->setAccessibleName(tr("Keyframe for %1").arg(Property.Editor->accessibleName()));
			Property.KeyFrame->setWhatsThis(tr("<p><b>Property Keyframe</b></p><p>Enable or remove a keyframe for this property at the current building step. Keyframes let the property's value change between steps.</p>"));
		}
	}

	SetLayoutMode(LayoutMode::Piece);
	setEnabled(false);
}


swObjectPropertyId swPropertiesWidget::GetEditorWidgetPropertyId(QWidget* Widget) const
{
	if (!Widget)
		return swObjectPropertyId::Count;

	for (size_t Index = 0; Index < mPropertyWidgets.size(); Index++)
		if (mPropertyWidgets[Index].Editor == Widget)
			return static_cast<swObjectPropertyId>(Index);

	return swObjectPropertyId::Count;
}

swObjectPropertyId swPropertiesWidget::GetKeyFrameWidgetPropertyId(QWidget* Widget) const
{
	if (!Widget)
		return swObjectPropertyId::Count;

	for (size_t Index = 0; Index < mPropertyWidgets.size(); Index++)
		if (mPropertyWidgets[Index].KeyFrame == Widget)
			return static_cast<swObjectPropertyId>(Index);

	return swObjectPropertyId::Count;
}

bool swPropertiesWidget::event(QEvent* Event)
{
	const bool Result = QWidget::event(Event);

	// Shown rows changed, so the panel can be resized to fit them.
	if (Event->type() == QEvent::LayoutRequest)
		emit LayoutChanged();

	return Result;
}




void swPropertiesWidget::CategoryStateChanged(bool Expanded)
{
	QObject* Button = sender();

	for (CategoryWidgets& Category : mCategoryWidgets)
	{
		if (Category.Button == Button)
		{
			SetCategoryWidgetsVisible(Category, Expanded);
			break;
		}
	}
}




void swPropertiesWidget::KeyFrameChanged()
{
	QCheckBox* Widget = qobject_cast<QCheckBox*>(sender());
	swObjectPropertyId PropertyId = GetKeyFrameWidgetPropertyId(Widget);

	if (PropertyId == swObjectPropertyId::Count)
		return;

	swModel* Model = gMainWindow->GetActiveModel();

	if (!Model)
		return;

	if (mFocusObject)
		Model->SetObjectsKeyFrame({ mFocusObject }, PropertyId, Widget->isChecked());
	else
		Model->SetObjectsKeyFrame(mSelection, PropertyId, Widget->isChecked());
}

void swPropertiesWidget::UpdateKeyFrameWidget(swObjectPropertyId PropertyId)
{
	swKeyFrameWidget* Widget = mPropertyWidgets[static_cast<int>(PropertyId)].KeyFrame;

	if (Widget)
	{
		QSignalBlocker Blocker(Widget);
		swModel* Model = gMainWindow->GetActiveModel();

		if (Model)
		{
			const swStep Step = Model->GetCurrentStep();

			if (mFocusObject)
				Widget->setChecked(mFocusObject->HasKeyFrame(PropertyId, Step));
			else
			{
				int KeyFrameCount = 0, NonKeyFrameCount = 0;

				for (const swObject* Object : mSelection)
				{
					if (Object->HasKeyFrame(PropertyId, Step))
						KeyFrameCount++;
					else
						NonKeyFrameCount++;
				}

				if (KeyFrameCount && NonKeyFrameCount)
					Widget->setCheckState(Qt::PartiallyChecked);
				else
					Widget->setCheckState(KeyFrameCount != 0 ? Qt::Checked : Qt::Unchecked);
			}
		}
	}
}



std::pair<QVariant, bool> swPropertiesWidget::GetUpdateValue(swObjectPropertyId PropertyId)
{
	QVariant Value;
	bool Partial = false;

	if (mFocusObject)
		Value = mFocusObject->GetPropertyValue(PropertyId);
	else
	{
		bool First = true;

		for (const swObject* Object : mSelection)
		{
			const QVariant ObjectValue = Object->GetPropertyValue(PropertyId);

			if (First)
			{
				Value = ObjectValue;
				First = false;
			}
			else if (Value != ObjectValue)
			{
				Partial = true;
				break;
			}
		}
	}

	return { Value, Partial };
}

void swPropertiesWidget::BoolChanged()
{
	QAbstractButton* Widget = qobject_cast<QAbstractButton*>(sender());
	swObjectPropertyId PropertyId = GetEditorWidgetPropertyId(Widget);

	if (PropertyId == swObjectPropertyId::Count)
		return;

	swModel* Model = gMainWindow->GetActiveModel();

	if (!Model)
		return;

	const bool Value = Widget->isChecked();
	Model->SetObjectsProperty(mFocusObject ? std::vector<swObject*>{ mFocusObject } : mSelection, PropertyId, Value);
}

void swPropertiesWidget::UpdateBool(swObjectPropertyId PropertyId)
{
	QAbstractButton* Button = qobject_cast<QAbstractButton*>(mPropertyWidgets[static_cast<int>(PropertyId)].Editor);

	if (!Button)
		return;

	QSignalBlocker Blocker(Button);
	QVariant Value;
	bool Partial;

	std::tie(Value, Partial) = GetUpdateValue(PropertyId);

	// A toggle button has no third state, differing values are named instead.
	Button->setChecked(!Partial && Value.toBool());
	Button->setText(Partial ? tr("%1: Multiple Values").arg(Button->accessibleName()) : Button->accessibleName());

	UpdateKeyFrameWidget(PropertyId);
}


void swPropertiesWidget::FloatEditingStarted()
{
	swDoubleSpinBox* Widget = qobject_cast<swDoubleSpinBox*>(sender());
	swObjectPropertyId PropertyId = GetEditorWidgetPropertyId(Widget);

	if (PropertyId == swObjectPropertyId::Count)
		return;

	swModel* Model = gMainWindow->GetActiveModel();

	if (Model)
		Model->BeginPropertyEdit(PropertyId);
}

void swPropertiesWidget::FloatEditingFinished()
{
	swDoubleSpinBox* Widget = qobject_cast<swDoubleSpinBox*>(sender());
	swObjectPropertyId PropertyId = GetEditorWidgetPropertyId(Widget);

	if (PropertyId == swObjectPropertyId::Count)
		return;

	swModel* Model = gMainWindow->GetActiveModel();

	if (!Model)
		return;

	Model->EndPropertyEdit(PropertyId, true);
}

void swPropertiesWidget::FloatEditingCanceled()
{
	swDoubleSpinBox* Widget = qobject_cast<swDoubleSpinBox*>(sender());
	swObjectPropertyId PropertyId = GetEditorWidgetPropertyId(Widget);

	if (PropertyId == swObjectPropertyId::Count)
		return;

	swModel* Model = gMainWindow->GetActiveModel();

	if (!Model)
		return;

	Model->EndPropertyEdit(PropertyId, false);
}

void swPropertiesWidget::FloatChanged(double Value)
{
	swDoubleSpinBox* Widget = qobject_cast<swDoubleSpinBox*>(sender());
	swObjectPropertyId PropertyId = GetEditorWidgetPropertyId(Widget);

	ChangeFloatValue(PropertyId, Value);
}

void swPropertiesWidget::ChangeFloatValue(swObjectPropertyId PropertyId, float Value)
{
	if (PropertyId == swObjectPropertyId::Count)
		return;

	swModel* Model = gMainWindow->GetActiveModel();

	if (!Model)
		return;

	swPiece* Piece = dynamic_cast<swPiece*>(mFocusObject);
	swCamera* Camera = dynamic_cast<swCamera*>(mFocusObject);
	swLight* Light = dynamic_cast<swLight*>(mFocusObject);

	mDisableUpdates = true;

	if (PropertyId == swObjectPropertyId::ObjectPositionX || PropertyId == swObjectPropertyId::ObjectPositionY || PropertyId == swObjectPropertyId::ObjectPositionZ)
	{
		swVector3 Center;
		swMatrix33 RelativeRotation;
		Model->GetMoveRotateTransform(Center, RelativeRotation);
		swVector3 Position = Center;

		if (PropertyId == swObjectPropertyId::ObjectPositionX)
			Position[0] = Value;
		else if (PropertyId == swObjectPropertyId::ObjectPositionY)
			Position[1] = Value;
		else if (PropertyId == swObjectPropertyId::ObjectPositionZ)
			Position[2] = Value;

		swVector3 Distance = Position - Center;

		Model->MoveSelectedObjects(Distance, swModelTransformFlag::FirstMove, swModelHistoryEditMerge::PropertiesMove);
	}
	else if (PropertyId == swObjectPropertyId::ObjectRotationX || PropertyId == swObjectPropertyId::ObjectRotationY || PropertyId == swObjectPropertyId::ObjectRotationZ)
	{
		swVector3 InitialRotation(0.0f, 0.0f, 0.0f);

		if (mLastRotation)
			InitialRotation = mLastRotation.value();
		else if (Piece)
			InitialRotation = swMatrix44ToEulerAngles(Piece->mModelWorld) * SW_RADIANS_TO_DEGREES;
		else if (Light)
			InitialRotation = swMatrix44ToEulerAngles(Light->GetWorldMatrix()) * SW_RADIANS_TO_DEGREES;

		swVector3 Rotation = InitialRotation;

		if (PropertyId == swObjectPropertyId::ObjectRotationX)
			Rotation[0] = Value;
		else if (PropertyId == swObjectPropertyId::ObjectRotationY)
			Rotation[1] = Value;
		else if (PropertyId == swObjectPropertyId::ObjectRotationZ)
			Rotation[2] = Value;

		Model->RotateSelectedObjects(Rotation - InitialRotation, swModelTransformFlag::Relative, swModelHistoryEditMerge::PropertiesRotate);

		mLastRotation = Rotation;
	}
	else if (PropertyId == swObjectPropertyId::CameraPositionX || PropertyId == swObjectPropertyId::CameraPositionY || PropertyId == swObjectPropertyId::CameraPositionZ)
	{
		quint32 FocusSection = SW_CAMERA_SECTION_INVALID;
		swVector3 Start;

		if (Camera)
		{
			FocusSection = Camera->GetFocusSection();
			Camera->SetFocused(FocusSection, false);
			Camera->SetFocused(SW_CAMERA_SECTION_POSITION, true);

			Start = Camera->mPosition;
		}
		else
		{
			Start = swVector3(0.0f, 0.0f, 0.0f);
		}

		swVector3 End = Start;

		if (PropertyId == swObjectPropertyId::CameraPositionX)
			End[0] = Value;
		else if (PropertyId == swObjectPropertyId::CameraPositionY)
			End[1] = Value;
		else if (PropertyId == swObjectPropertyId::CameraPositionZ)
			End[2] = Value;

		swVector3 Distance = End - Start;

		Model->MoveSelectedObjects(Distance, swModelTransformFlag::FirstMove, swModelHistoryEditMerge::PropertiesMove);

		if (Camera)
		{
			Camera->SetFocused(SW_CAMERA_SECTION_POSITION, false);
			Camera->SetFocused(FocusSection, true);
		}
	}
	else if (PropertyId == swObjectPropertyId::CameraTargetX || PropertyId == swObjectPropertyId::CameraTargetY || PropertyId == swObjectPropertyId::CameraTargetZ)
	{
		quint32 FocusSection = SW_CAMERA_SECTION_INVALID;
		swVector3 Start;

		if (Camera)
		{
			FocusSection = Camera->GetFocusSection();
			Camera->SetFocused(FocusSection, false);
			Camera->SetFocused(SW_CAMERA_SECTION_TARGET, true);

			Start = Camera->mTargetPosition;
		}
		else
		{
			Start = swVector3(0.0f, 0.0f, 0.0f);
		}

		swVector3 End = Start;

		if (PropertyId == swObjectPropertyId::CameraTargetX)
			End[0] = Value;
		else if (PropertyId == swObjectPropertyId::CameraTargetY)
			End[1] = Value;
		else if (PropertyId == swObjectPropertyId::CameraTargetZ)
			End[2] = Value;

		swVector3 Distance = End - Start;

		Model->MoveSelectedObjects(Distance, swModelTransformFlag::FirstMove, swModelHistoryEditMerge::PropertiesMove);

		if (Camera)
		{
			Camera->SetFocused(SW_CAMERA_SECTION_TARGET, false);
			Camera->SetFocused(FocusSection, true);
		}
	}
	else if (PropertyId == swObjectPropertyId::CameraUpX || PropertyId == swObjectPropertyId::CameraUpY || PropertyId == swObjectPropertyId::CameraUpZ)
	{
		quint32 FocusSection = SW_CAMERA_SECTION_INVALID;
		swVector3 Start;

		if (Camera)
		{
			FocusSection = Camera->GetFocusSection();
			Camera->SetFocused(FocusSection, false);
			Camera->SetFocused(SW_CAMERA_SECTION_UP_VECTOR, true);

			Start = Camera->mUpVector;
		}
		else
		{
			Start = swVector3(0.0f, 0.0f, 0.0f);
		}

		swVector3 End = Start;

		if (PropertyId == swObjectPropertyId::CameraUpX)
			End[0] = Value;
		else if (PropertyId == swObjectPropertyId::CameraUpY)
			End[1] = Value;
		else if (PropertyId == swObjectPropertyId::CameraUpZ)
			End[2] = Value;

		swVector3 Distance = End - Start;

		Model->MoveSelectedObjects(Distance, swModelTransformFlag::FirstMove, swModelHistoryEditMerge::PropertiesMove);

		if (Camera)
		{
			Camera->SetFocused(SW_CAMERA_SECTION_UP_VECTOR, false);
			Camera->SetFocused(FocusSection, true);
		}
	}
	else
	{
		Model->SetObjectsProperty(mFocusObject ? std::vector<swObject*>{ mFocusObject } : mSelection, PropertyId, Value);
	}

	mDisableUpdates = false;
}

void swPropertiesWidget::UpdateFloat(swObjectPropertyId PropertyId, float Value)
{
	swDoubleSpinBox* Widget = qobject_cast<swDoubleSpinBox*>(mPropertyWidgets[static_cast<int>(PropertyId)].Editor);

	if (Widget)
	{
		QSignalBlocker Blocker(Widget);

		switch (PropertyId)
		{
		case swObjectPropertyId::PieceId:
		case swObjectPropertyId::PieceColor:
		case swObjectPropertyId::PieceStepShow:
		case swObjectPropertyId::PieceStepHide:
		case swObjectPropertyId::CameraName:
		case swObjectPropertyId::CameraProjection:
			break;

		case swObjectPropertyId::CameraFOV:
		case swObjectPropertyId::CameraNear:
		case swObjectPropertyId::CameraFar:
			Widget->SetSnap(swFloatPropertySnap::Auto);
			break;

		case swObjectPropertyId::CameraPositionX:
		case swObjectPropertyId::CameraPositionY:
		case swObjectPropertyId::CameraPositionZ:
		case swObjectPropertyId::CameraTargetX:
		case swObjectPropertyId::CameraTargetY:
		case swObjectPropertyId::CameraTargetZ:
			Widget->SetSnap(swFloatPropertySnap::Position);
			break;

		case swObjectPropertyId::CameraUpX:
		case swObjectPropertyId::CameraUpY:
		case swObjectPropertyId::CameraUpZ:
			Widget->SetSnap(swFloatPropertySnap::Auto);
			break;

		case swObjectPropertyId::LightName:
		case swObjectPropertyId::LightType:
		case swObjectPropertyId::LightColor:
			break;

		case swObjectPropertyId::LightBlenderPower:
		case swObjectPropertyId::LightPOVRayPower:
		case swObjectPropertyId::LightCastShadow:
		case swObjectPropertyId::LightPOVRayFadeDistance:
		case swObjectPropertyId::LightPOVRayFadePower:
		case swObjectPropertyId::LightBlenderRadius:
		case swObjectPropertyId::LightBlenderAngle:
		case swObjectPropertyId::LightAreaSizeX:
		case swObjectPropertyId::LightAreaSizeY:
		case swObjectPropertyId::LightSpotConeAngle:
		case swObjectPropertyId::LightSpotPenumbraAngle:
		case swObjectPropertyId::LightPOVRaySpotTightness:
			Widget->SetSnap(swFloatPropertySnap::Auto);
			break;

		case swObjectPropertyId::LightAreaShape:
		case swObjectPropertyId::LightPOVRayAreaGridX:
		case swObjectPropertyId::LightPOVRayAreaGridY:
			break;

		case swObjectPropertyId::ObjectPositionX:
		case swObjectPropertyId::ObjectPositionY:
			Widget->SetSnap(mLayoutMode == LayoutMode::Piece ? swFloatPropertySnap::PiecePositionXY : swFloatPropertySnap::Position);
			break;

		case swObjectPropertyId::ObjectPositionZ:
			Widget->SetSnap(mLayoutMode == LayoutMode::Piece ? swFloatPropertySnap::PiecePositionZ : swFloatPropertySnap::Position);
			break;

		case swObjectPropertyId::ObjectRotationX:
		case swObjectPropertyId::ObjectRotationY:
		case swObjectPropertyId::ObjectRotationZ:
			Widget->SetSnap(swFloatPropertySnap::Rotation);
			break;

		case swObjectPropertyId::Count:
			break;
		}

		Widget->SetValue(Value);
	}

	UpdateKeyFrameWidget(PropertyId);
}




void swPropertiesWidget::IntegerChanged()
{
	QSpinBox* SpinBox = qobject_cast<QSpinBox*>(sender());
	swObjectPropertyId PropertyId = GetEditorWidgetPropertyId(SpinBox);

	if (PropertyId == swObjectPropertyId::Count)
		return;

	swModel* Model = gMainWindow->GetActiveModel();

	// The value below the range only stands for differing values.
	if (!Model || (SpinBox->value() == SpinBox->minimum() && !SpinBox->specialValueText().isEmpty()))
		return;

	Model->SetObjectsProperty(mFocusObject ? std::vector<swObject*>{ mFocusObject } : mSelection, PropertyId, SpinBox->value());
}

void swPropertiesWidget::UpdateInteger(swObjectPropertyId PropertyId)
{
	QSpinBox* SpinBox = qobject_cast<QSpinBox*>(mPropertyWidgets[static_cast<int>(PropertyId)].Editor);

	if (!SpinBox)
		return;

	QSignalBlocker Blocker(SpinBox);
	QVariant Value;
	bool Partial;

	std::tie(Value, Partial) = GetUpdateValue(PropertyId);

	// Differing values show as text on a value below the range, which leaves the range when edited.
	const int Minimum = SpinBox->specialValueText().isEmpty() ? SpinBox->minimum() : SpinBox->minimum() + 1;

	if (Partial)
	{
		SpinBox->setMinimum(Minimum - 1);
		SpinBox->setSpecialValueText(tr("%1Multiple Values").arg(SpinBox->prefix()));
		SpinBox->setValue(Minimum - 1);
	}
	else
	{
		SpinBox->setSpecialValueText(QString());
		SpinBox->setMinimum(Minimum);
		SpinBox->setValue(Value.toInt());
	}

	UpdateKeyFrameWidget(PropertyId);
}


void swPropertiesWidget::StepNumberChanged()
{
	QLineEdit* Widget = qobject_cast<QLineEdit*>(sender());
	swObjectPropertyId PropertyId = GetEditorWidgetPropertyId(Widget);

	if (PropertyId == swObjectPropertyId::Count)
		return;

	swModel* Model = gMainWindow->GetActiveModel();

	if (!Model)
		return;

	bool Ok = true;
	QString Text = Widget->text();
	swStep Step = Text.isEmpty() && PropertyId == swObjectPropertyId::PieceStepHide ? SW_STEP_MAXIMUM : Widget->validator()->locale().toUInt(Text, &Ok);

	if (!Ok || !Widget->hasAcceptableInput())
		return;

	if (PropertyId == swObjectPropertyId::PieceStepShow)
	{
		Model->SetSelectedPiecesStepShow(Step);
	}
	else if (PropertyId == swObjectPropertyId::PieceStepHide)
	{
		Model->SetSelectedPiecesStepHide(Step);
	}
}

void swPropertiesWidget::UpdateStepNumber(swObjectPropertyId PropertyId, swStep Step, swStep Min, swStep Max)
{
	QLineEdit* Widget = qobject_cast<QLineEdit*>(mPropertyWidgets[static_cast<int>(PropertyId)].Editor);

	if (Widget)
	{
		QSignalBlocker Blocker(Widget);

		Widget->setValidator(new swStepValidator(Min, Max, PropertyId == swObjectPropertyId::PieceStepHide, Widget));
		Widget->setText(Step == SW_STEP_MAXIMUM ? QString() : swFormatIntegerLocalized(Step, Widget->validator()->locale()));
	}

	UpdateKeyFrameWidget(PropertyId);
}






void swPropertiesWidget::StringChanged()
{
	QLineEdit* LineEdit = qobject_cast<QLineEdit*>(sender());
	swObjectPropertyId PropertyId = GetEditorWidgetPropertyId(LineEdit);

	if (PropertyId == swObjectPropertyId::Count)
		return;

	swModel* Model = gMainWindow->GetActiveModel();

	if (!Model)
		return;

	QString Value = LineEdit->text();
	Model->SetObjectsProperty(mFocusObject ? std::vector<swObject*>{ mFocusObject } : mSelection, PropertyId, Value);
}

void swPropertiesWidget::UpdateString(swObjectPropertyId PropertyId)
{
	QLineEdit* LineEdit = qobject_cast<QLineEdit*>(mPropertyWidgets[static_cast<int>(PropertyId)].Editor);

	if (!LineEdit)
		return;

	QSignalBlocker Blocker(LineEdit);
	QVariant Value;
	bool Partial;

	std::tie(Value, Partial) = GetUpdateValue(PropertyId);

	if (Partial)
	{
		LineEdit->clear();
		LineEdit->setPlaceholderText(tr("Multiple Values"));
	}
	else
	{
		LineEdit->setText(Value.toString());
		LineEdit->setPlaceholderText(LineEdit->accessibleName());
	}

	UpdateKeyFrameWidget(PropertyId);
}


void swPropertiesWidget::StringListChanged(int Value)
{
	QComboBox* ComboBox = qobject_cast<QComboBox*>(sender());
	swObjectPropertyId PropertyId = GetEditorWidgetPropertyId(ComboBox);

	if (PropertyId == swObjectPropertyId::Count)
		return;

	swModel* Model = gMainWindow->GetActiveModel();

	if (!Model)
		return;

	Model->SetObjectsProperty(mFocusObject ? std::vector<swObject*>{ mFocusObject } : mSelection, PropertyId, Value);
}

void swPropertiesWidget::UpdateStringList(swObjectPropertyId PropertyId)
{
	QComboBox* ComboBox = qobject_cast<QComboBox*>(mPropertyWidgets[static_cast<int>(PropertyId)].Editor);

	if (!ComboBox)
		return;

	QSignalBlocker Blocker(ComboBox);
	QVariant Value;
	bool Partial;

	std::tie(Value, Partial) = GetUpdateValue(PropertyId);
	bool HasMultiple = (ComboBox->itemText(ComboBox->count() - 1) == tr("Multiple Values"));

	if (Partial)
	{
		if (!HasMultiple)
			ComboBox->addItem(tr("Multiple Values"));

		ComboBox->setCurrentIndex(ComboBox->count() - 1);
	}
	else
	{
		if (HasMultiple)
			ComboBox->removeItem(ComboBox->count() - 1);

		ComboBox->setCurrentIndex(Value.toInt());
	}

	UpdateKeyFrameWidget(PropertyId);
}


void swPropertiesWidget::ColorButtonClicked()
{
	QToolButton* ColorButton = qobject_cast<QToolButton*>(sender());
	swObjectPropertyId PropertyId = GetEditorWidgetPropertyId(ColorButton);

	if (PropertyId == swObjectPropertyId::Count)
		return;

	QVariant Value;
	bool Partial;

	std::tie(Value, Partial) = GetUpdateValue(PropertyId);

	QColor InitialColor = Partial ? QColor(128, 128, 128) : swQColorFromVector3(Value.value<swVector3>());

	QMenu* Menu = new QMenu(ColorButton);

	QWidgetAction* Action = new QWidgetAction(Menu);
	swColorDialogPopup *Popup = new swColorDialogPopup(InitialColor, Menu);
	Action->setDefaultWidget(Popup);
	Menu->addAction(Action);

	connect(Popup, &swColorDialogPopup::ColorSelected, this, &swPropertiesWidget::ColorChanged);

	Menu->exec(ColorButton->mapToGlobal(ColorButton->rect().bottomLeft()));

	delete Menu;
}

void swPropertiesWidget::ColorChanged(QColor Color)
{
	swColorDialogPopup* Popup = qobject_cast<swColorDialogPopup*>(sender());
	QMenu* Menu = qobject_cast<QMenu*>(Popup->parent());
	QToolButton* ColorButton = qobject_cast<QToolButton*>(Menu->parent());
	swObjectPropertyId PropertyId = GetEditorWidgetPropertyId(ColorButton);

	if (!Color.isValid())
		return;

	swModel* Model = gMainWindow->GetActiveModel();

	if (!Model)
		return;

	const swVector3 FloatColor = swVector3FromQColor(Color);
	Model->SetObjectsProperty(mFocusObject ? std::vector<swObject*>{ mFocusObject } : mSelection, PropertyId, QVariant::fromValue<swVector3>(FloatColor));
}

void swPropertiesWidget::UpdateColor(swObjectPropertyId PropertyId)
{
	QToolButton* ColorButton = qobject_cast<QToolButton*>(mPropertyWidgets[static_cast<int>(PropertyId)].Editor);

	if (!ColorButton)
		return;

	QSignalBlocker Blocker(ColorButton);
	QVariant Value;
	bool Partial;

	std::tie(Value, Partial) = GetUpdateValue(PropertyId);

	QColor Color = Partial ? QColor(128, 128, 128) : swQColorFromVector3(Value.value<swVector3>());

	// The color shows in the light block so the name stays centered like the piece color.
	mIdentityBlocks[static_cast<int>(IdentityIndex::Light)].Color = Color;
	UpdateIdentityBlock(mIdentityBlocks[static_cast<int>(IdentityIndex::Light)]);

	ColorButton->setText(Partial ? tr("Multiple Colors") : Color.name());

	UpdateKeyFrameWidget(PropertyId);
}


void swPropertiesWidget::PieceColorChanged(int ColorIndex)
{
	swColorPickerPopup* Popup = qobject_cast<swColorPickerPopup*>(sender());
	QMenu* Menu = qobject_cast<QMenu*>(Popup->parent());
	QToolButton* ColorButton = qobject_cast<QToolButton*>(Menu->parent());
	swObjectPropertyId PropertyId = GetEditorWidgetPropertyId(ColorButton);

	Menu->close();

	if (PropertyId == swObjectPropertyId::Count)
		return;

	swModel* Model = gMainWindow->GetActiveModel();

	if (!Model)
		return;

	Model->SetObjectsProperty(mFocusObject ? std::vector<swObject*>{ mFocusObject } : mSelection, PropertyId, ColorIndex);
}

void swPropertiesWidget::PieceColorButtonClicked()
{
	QToolButton* ColorButton = qobject_cast<QToolButton*>(sender());
	swObjectPropertyId PropertyId = GetEditorWidgetPropertyId(ColorButton);

	if (!ColorButton)
		return;

	QVariant Value;
	bool Partial;

	std::tie(Value, Partial) = GetUpdateValue(PropertyId);

	int ColorIndex = Partial ? gDefaultColor : Value.toInt();

	QMenu* Menu = new QMenu(ColorButton);

	QWidgetAction* Action = new QWidgetAction(Menu);
	swColorPickerPopup* Popup = new swColorPickerPopup(Menu, ColorIndex);
	Action->setDefaultWidget(Popup);
	Menu->addAction(Action);

	connect(Popup, &swColorPickerPopup::Selected, this, &swPropertiesWidget::PieceColorChanged);

	Menu->exec(ColorButton->mapToGlobal(ColorButton->rect().bottomLeft()));

	delete Menu;
}

void swPropertiesWidget::UpdatePieceColor(swObjectPropertyId PropertyId)
{
	QToolButton* ColorButton = qobject_cast<QToolButton*>(mPropertyWidgets[static_cast<int>(PropertyId)].Editor);

	if (!ColorButton)
		return;

	QSignalBlocker Blocker(ColorButton);
	QVariant Value;
	bool Partial;

	std::tie(Value, Partial) = GetUpdateValue(PropertyId);

	// With nothing selected the button only names what it edits.
	if (!mFocusObject && mSelection.empty())
	{
		mIdentityBlocks[static_cast<int>(IdentityIndex::Piece)].Color = QColor();
		UpdateIdentityBlock(mIdentityBlocks[static_cast<int>(IdentityIndex::Piece)]);

		ColorButton->setText(tr("Color"));
		UpdateKeyFrameWidget(PropertyId);
		return;
	}

	const int ColorIndex = Value.toInt();
	QColor Color = Partial ? QColor(128, 128, 128) : QColor::fromRgbF(gColorList[ColorIndex].Value[0], gColorList[ColorIndex].Value[1], gColorList[ColorIndex].Value[2]);
	// The color shows in the swatch button so the name stays centered.
	mIdentityBlocks[static_cast<int>(IdentityIndex::Piece)].Color = Color;
	UpdateIdentityBlock(mIdentityBlocks[static_cast<int>(IdentityIndex::Piece)]);

	ColorButton->setText(Partial ? tr("Multiple Colors") : gColorList[ColorIndex].Name);

	UpdateKeyFrameWidget(PropertyId);
}


void swPropertiesWidget::UpdatePieceId(swObjectPropertyId PropertyId)
{
	swElidableToolButton* PieceIdButton = qobject_cast<swElidableToolButton*>(mPropertyWidgets[static_cast<int>(PropertyId)].Editor);

	if (!PieceIdButton)
		return;

	QSignalBlocker Blocker(PieceIdButton);
	QVariant Value;
	bool Partial;

	std::tie(Value, Partial) = GetUpdateValue(PropertyId);

	swPieceData* PieceData = static_cast<swPieceData*>(Value.value<void*>());

	if (Partial)
		PieceIdButton->setText(tr("Multiple Pieces"));
	else if (PieceData)
		PieceIdButton->setText(PieceData->m_strDescription);
	else
		PieceIdButton->setText(tr("Piece"));

	UpdateKeyFrameWidget(PropertyId);
}

void swPropertiesWidget::PieceIdButtonClicked()
{
	QToolButton* PieceIdButton = qobject_cast<QToolButton*>(sender());
	swObjectPropertyId PropertyId = GetEditorWidgetPropertyId(PieceIdButton);

	if (!PieceIdButton)
		return;

	QVariant Value;
	bool Partial;

	std::tie(Value, Partial) = GetUpdateValue(PropertyId);

	swPieceData* PieceData = Partial ? nullptr : static_cast<swPieceData*>(Value.value<void*>());

	std::tie(Value, Partial) = GetUpdateValue(swObjectPropertyId::PieceColor);

	int ColorIndex = Partial ? gDefaultColor : Value.toInt();

	std::optional<swPieceData*> Result = swShowPartSelectionPopup(PieceData, std::vector<std::pair<swPieceData*, std::string>>(), ColorIndex, PieceIdButton, PieceIdButton->mapToGlobal(PieceIdButton->rect().bottomLeft()));

	if (Result.has_value())
	{
		swModel* Model = gMainWindow->GetActiveModel();
		PieceData = Result.value();

		if (!Model || !PieceData)
			return;

		Model->SetObjectsProperty(mFocusObject ? std::vector<swObject*>{ mFocusObject } : mSelection, PropertyId, QVariant::fromValue<void*>(PieceData));
	}
}



bool swPropertiesWidget::HasVisibleRows() const
{
	// Only the scrolling rows count, the identity and transform rows sit outside the list.
	for (int Index = 0; Index < mSettingsLayout->count(); Index++)
	{
		const QWidget* Widget = mSettingsLayout->itemAt(Index)->widget();

		if (Widget && !Widget->isHidden())
			return true;
	}

	return false;
}

bool swPropertiesWidget::eventFilter(QObject* Object, QEvent* Event)
{
	// The identity and transform rows sit outside the list, so their changes also resize the panel.
	if ((Object == mIdentityRows || Object == mTransformRows) && Event->type() == QEvent::LayoutRequest)
		emit LayoutChanged();

	// The rows move as rows above them show or hide, the block follows the rows holding its slots.
	for (IdentityBlock& Block : mIdentityBlocks)
	{
		if (std::find_if(Block.Slots.begin(), Block.Slots.end(), [Object](const QWidget* Slot) { return Slot->parentWidget() == Object; }) == Block.Slots.end())
			continue;

		switch (Event->type())
		{
		case QEvent::Move:
		case QEvent::Resize:
		case QEvent::Show:
		case QEvent::Hide:
			UpdateIdentityBlock(Block);
			break;

		default:
			break;
		}
	}

	return QWidget::eventFilter(Object, Event);
}

void swPropertiesWidget::UpdateIdentityBlock(IdentityBlock& Block)
{
	if (!Block.Swatch)
		return;

	// The block spans the shown identity rows, the color fills the button inside its border.
	QWidget* FirstSlot = nullptr;
	QWidget* LastSlot = nullptr;

	for (QWidget* Slot : Block.Slots)
	{
		if (Slot->parentWidget()->isHidden())
			continue;

		if (!FirstSlot)
			FirstSlot = Slot;

		LastSlot = Slot;
	}

	if (!FirstSlot || !LastSlot->isVisible())
	{
		Block.Swatch->hide();
		return;
	}

	const QRect SwatchRect(FirstSlot->mapTo(mIdentityRows, QPoint(0, 0)), LastSlot->mapTo(mIdentityRows, LastSlot->rect().bottomRight()));
	const int Inset = swGetLayoutSpacing(this, Qt::Horizontal);
	const QSize IconSize = (SwatchRect.size() - QSize(Inset, Inset)).expandedTo(QSize(1, 1));

	Block.Swatch->setGeometry(SwatchRect);
	Block.Swatch->setIconSize(IconSize);

	if (Block.Color.isValid())
	{
		QPixmap Pixmap(IconSize);
		Pixmap.fill(Block.Color);
		Block.Swatch->setIcon(Pixmap);
	}
	else
		Block.Swatch->setIcon(QIcon());

	Block.Swatch->show();
	Block.Swatch->raise();
}

void swPropertiesWidget::UpdateRowVisible(QWidget* Row)
{
	if (!Row)
		return;

	// A row shows while any of its values is shown, a lone value fills the row.
	bool Visible = false;

	for (const PropertyWidgets& Property : mPropertyWidgets)
		if (Property.Row == Row && Property.Editor && !Property.Editor->isHidden())
			Visible = true;

	Row->setVisible(Visible);
}

void swPropertiesWidget::AddCategory(CategoryIndex Index, const QString& Title, bool Collapsible)
{
	mCurrentCategory = &mCategoryWidgets[static_cast<int>(Index)];
	mCurrentCategory->Category = Index;

	mCurrentCategory->Title = Title;

	// Only collapsible categories have a header, the title still names their properties for accessibility.
	if (!Collapsible)
		return;

	swCollapsibleWidgetButton* CategoryButton = new swCollapsibleWidgetButton(Title, this);
	CategoryButton->setToolTip(tr("Expand or collapse the %1 properties.").arg(Title));

	mCurrentCategory->Button = CategoryButton;

	connect(CategoryButton, &swCollapsibleWidgetButton::StateChanged, this, &swPropertiesWidget::CategoryStateChanged);

	mSettingsLayout->addWidget(CategoryButton);
}

void swPropertiesWidget::RegisterEditor(swObjectPropertyId PropertyId, QWidget* Editor, const QString& Name, const QString& ToolTip)
{
	Editor->setToolTip(ToolTip);
	Editor->setAccessibleName(Name);

	mCurrentCategory->Properties.push_back(PropertyId);
	mPropertyWidgets[static_cast<int>(PropertyId)].Editor = Editor;
	mPropertyWidgets[static_cast<int>(PropertyId)].Category = mCurrentCategory->Category;
}

swKeyFrameWidget* swPropertiesWidget::CreateKeyFrameWidget(swObjectPropertyId PropertyId)
{
	swKeyFrameWidget* Widget = new swKeyFrameWidget(this);
	Widget->setToolTip(tr("Toggle Keyframe"));

	connect(Widget, &QCheckBox::stateChanged, this, &swPropertiesWidget::KeyFrameChanged);

	mPropertyWidgets[static_cast<int>(PropertyId)].KeyFrame = Widget;

	return Widget;
}

swDoubleSpinBox* swPropertiesWidget::CreateFloatEditor(swObjectPropertyId PropertyId, const QString& Name, const QString& ToolTip, float Min, float Max, bool ShowName, const QString& Suffix)
{
	swDoubleSpinBox* Widget = new swDoubleSpinBox(this);
	RegisterEditor(PropertyId, Widget, Name, ToolTip);

	Widget->setRange(Min, Max);

	// Single values carry their name and unit, the letter of a vector row names its boxes.
	if (ShowName)
		Widget->setPrefix(Name + QLatin1Char(' '));

	Widget->setSuffix(Suffix);
	swSetupPanelSpinBox(Widget);

	connect(Widget, &swDoubleSpinBox::EditingStarted, this, &swPropertiesWidget::FloatEditingStarted);
	connect(Widget, &swDoubleSpinBox::EditingCanceled, this, &swPropertiesWidget::FloatEditingCanceled);
	connect(Widget, &swDoubleSpinBox::EditingFinished, this, &swPropertiesWidget::FloatEditingFinished);

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	connect(Widget, QOverload<double>::of(&swDoubleSpinBox::valueChanged), this, &swPropertiesWidget::FloatChanged);
#else
	connect(Widget, &swDoubleSpinBox::valueChanged, this, &swPropertiesWidget::FloatChanged);
#endif

	return Widget;
}

QSpinBox* swPropertiesWidget::CreateIntegerEditor(swObjectPropertyId PropertyId, const QString& Name, const QString& ToolTip, int Min, int Max)
{
	QSpinBox* Widget = new QSpinBox(this);
	RegisterEditor(PropertyId, Widget, Name, ToolTip);

	Widget->setRange(Min, Max);
	Widget->setPrefix(Name + QLatin1Char(' '));
	swSetupPanelSpinBox(Widget);

	// Applied on every change like the other value boxes, so the arrows take effect at once.
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	connect(Widget, QOverload<int>::of(&QSpinBox::valueChanged), this, &swPropertiesWidget::IntegerChanged);
#else
	connect(Widget, &QSpinBox::valueChanged, this, &swPropertiesWidget::IntegerChanged);
#endif

	return Widget;
}

QLineEdit* swPropertiesWidget::CreateStepNumberEditor(swObjectPropertyId PropertyId, const QString& Name, const QString& ToolTip)
{
	QLineEdit* Widget = new QLineEdit(this);
	RegisterEditor(PropertyId, Widget, Name, ToolTip);
	Widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	swSetupPanelEdit(Widget);

	connect(Widget, &QLineEdit::editingFinished, this, &swPropertiesWidget::StepNumberChanged);

	// An empty removal step keeps the piece for the rest of the model.
	if (PropertyId == swObjectPropertyId::PieceStepHide)
		Widget->setPlaceholderText(tr("Never"));

	return Widget;
}

QLineEdit* swPropertiesWidget::CreateStringEditor(swObjectPropertyId PropertyId, const QString& Name, const QString& ToolTip)
{
	QLineEdit* Widget = new QLineEdit(this);
	RegisterEditor(PropertyId, Widget, Name, ToolTip);
	Widget->setPlaceholderText(Name);
	Widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	swSetupPanelEdit(Widget);

	connect(Widget, &QLineEdit::editingFinished, this, &swPropertiesWidget::StringChanged);

	return Widget;
}

QComboBox* swPropertiesWidget::CreateStringListEditor(swObjectPropertyId PropertyId, const QString& Name, const QString& ToolTip, const QStringList& Strings)
{
	QComboBox* Widget = new QComboBox(this);
	RegisterEditor(PropertyId, Widget, Name, ToolTip);
	Widget->addItems(Strings);
	Widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	swSetupPanelEdit(Widget);

	connect(Widget, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &swPropertiesWidget::StringListChanged);

	return Widget;
}

QToolButton* swPropertiesWidget::CreateColorEditor(swObjectPropertyId PropertyId, const QString& Name, const QString& ToolTip)
{
	QToolButton* Widget = new QToolButton(this);
	RegisterEditor(PropertyId, Widget, Name, ToolTip);
	Widget->setToolButtonStyle(Qt::ToolButtonTextOnly);
	swSetupPanelButton(Widget);
	Widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

	connect(Widget, &QToolButton::clicked, this, &swPropertiesWidget::ColorButtonClicked);

	return Widget;
}

QToolButton* swPropertiesWidget::CreateToggleEditor(swObjectPropertyId PropertyId, const QString& Name, const QString& ToolTip)
{
	QToolButton* Widget = new QToolButton(this);
	RegisterEditor(PropertyId, Widget, Name, ToolTip);
	Widget->setText(Name);
	Widget->setCheckable(true);
	Widget->setToolButtonStyle(Qt::ToolButtonTextOnly);
	swSetupPanelButton(Widget);
	Widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

	connect(Widget, &QToolButton::toggled, this, &swPropertiesWidget::BoolChanged);

	return Widget;
}

QToolButton* swPropertiesWidget::CreatePieceIdEditor(swObjectPropertyId PropertyId, const QString& Name, const QString& ToolTip)
{
	swElidableToolButton* Widget = new swElidableToolButton(this);
	RegisterEditor(PropertyId, Widget, Name, ToolTip);
	Widget->setToolButtonStyle(Qt::ToolButtonTextOnly);
	swSetupPanelButton(Widget);
	Widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

	connect(Widget, &QToolButton::clicked, this, &swPropertiesWidget::PieceIdButtonClicked);

	return Widget;
}

QToolButton* swPropertiesWidget::CreatePieceColorEditor(swObjectPropertyId PropertyId, const QString& Name, const QString& ToolTip)
{
	QToolButton* Widget = new QToolButton(this);
	RegisterEditor(PropertyId, Widget, Name, ToolTip);
	Widget->setToolButtonStyle(Qt::ToolButtonTextOnly);
	swSetupPanelButton(Widget);
	Widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

	connect(Widget, &QToolButton::clicked, this, &swPropertiesWidget::PieceColorButtonClicked);

	return Widget;
}

QToolButton* swPropertiesWidget::CreateRowNameButton(QWidget* Row, const QString& Letter, const QString& Name)
{
	// A button with a letter for the row takes the place of the transform button so the boxes line up.
	QToolButton* NameButton = new QToolButton(Row);
	NameButton->setText(Letter);
	NameButton->setToolTip(Name);
	NameButton->setAccessibleName(Name);
	NameButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
	swAddPanelRowWidget(Row, NameButton);
	swSetupPanelButton(NameButton);
	NameButton->setFixedSize(swGetPanelButtonSize());

	return NameButton;
}

QWidget* swPropertiesWidget::AddValueRow(QWidget* Container, const QString& Letter, const QString& Name, const std::vector<RowEditor>& Editors)
{
	QWidget* Row = swCreatePanelRow(Container);
	Container->layout()->addWidget(Row);

	CreateRowNameButton(Row, Letter, Name);

	for (const RowEditor& Editor : Editors)
	{
		Editor.Editor->setParent(Row);
		swAddPanelRowWidget(Row, Editor.Editor);
		mPropertyWidgets[static_cast<int>(GetEditorWidgetPropertyId(Editor.Editor))].Row = Row;

		if (Editor.KeyFrameId != swObjectPropertyId::Count)
			swAddPanelRowWidget(Row, CreateKeyFrameWidget(Editor.KeyFrameId));
	}

	return Row;
}

void swPropertiesWidget::AddVectorRow(QWidget* Container, const std::array<swObjectPropertyId, 3>& PropertyIds, const QString& Letter, const QString& Name, const QString& ToolTip, bool SupportsKeyFrames, float Min, float Max)
{
	// The X, Y and Z values share a row with one keyframe since they are keyed together.
	const QString AxisNames[3] = { tr("X"), tr("Y"), tr("Z") };
	std::vector<RowEditor> Editors;

	for (int Axis = 0; Axis < 3; Axis++)
	{
		swDoubleSpinBox* Widget = CreateFloatEditor(PropertyIds[Axis], tr("%1 %2").arg(Name, AxisNames[Axis]), tr("%1 (%2)").arg(ToolTip, AxisNames[Axis]), Min, Max, false);
		Editors.push_back({ Widget, Axis == 2 && SupportsKeyFrames ? PropertyIds[0] : swObjectPropertyId::Count });
	}

	AddValueRow(Container, Letter, Name, Editors);
}

void swPropertiesWidget::AddIdentityRow(IdentityIndex Index, QWidget* Editor, swObjectPropertyId KeyFrameId)
{
	// A full width row without a name, the block on the left spans the slots of all the identity rows of the object.
	IdentityBlock& Block = mIdentityBlocks[static_cast<int>(Index)];
	QWidget* Row = swCreatePanelRow(mIdentityRows);
	mIdentityRows->layout()->addWidget(Row);

	QWidget* Slot = new QWidget(Row);
	Slot->setFixedSize(swGetPanelButtonSize());
	swAddPanelRowWidget(Row, Slot);
	Row->installEventFilter(this);
	Block.Slots.push_back(Slot);

	Editor->setParent(Row);
	swAddPanelRowWidget(Row, Editor);

	const swObjectPropertyId PropertyId = GetEditorWidgetPropertyId(Editor);
	mPropertyWidgets[static_cast<int>(PropertyId)].Row = Row;

	if (KeyFrameId != swObjectPropertyId::Count)
		swAddPanelRowWidget(Row, CreateKeyFrameWidget(KeyFrameId));

	if (!Block.Swatch)
	{
		// Placed over the slots once the rows are laid out, hidden until then.
		Block.Swatch = new QToolButton(mIdentityRows);
		Block.Swatch->setFocusPolicy(Qt::NoFocus);
		Block.Swatch->setAttribute(Qt::WA_TransparentForMouseEvents);
		Block.Swatch->hide();
	}

	// Objects with a color open the same color choice from the block.
	if (PropertyId == swObjectPropertyId::PieceColor || PropertyId == swObjectPropertyId::LightColor)
	{
		Block.Swatch->setAttribute(Qt::WA_TransparentForMouseEvents, false);
		Block.Swatch->setToolTip(Editor->toolTip());
		Block.Swatch->setAccessibleName(tr("%1 Swatch").arg(Editor->accessibleName()));
		connect(Block.Swatch, &QToolButton::clicked, static_cast<QToolButton*>(Editor), &QToolButton::click);
	}
}

void swPropertiesWidget::CreateWidgets()
{
	// The rows sit in the panel layout, which provides the margins.
	mSettingsLayout = new QVBoxLayout(this);
	mSettingsLayout->setContentsMargins(0, 0, 0, 0);

	mIdentityRows = new QWidget(this);
	mIdentityRows->installEventFilter(this);
	QVBoxLayout* IdentityRowsLayout = new QVBoxLayout(mIdentityRows);
	IdentityRowsLayout->setContentsMargins(0, 0, 0, 0);

	mTransformRows = new QWidget(this);
	mTransformRows->installEventFilter(this);
	QVBoxLayout* TransformRowsLayout = new QVBoxLayout(mTransformRows);
	TransformRowsLayout->setContentsMargins(0, 0, 0, 0);

	mStepRows = new QWidget(this);
	QVBoxLayout* StepRowsLayout = new QVBoxLayout(mStepRows);
	StepRowsLayout->setContentsMargins(0, 0, 0, 0);

	AddCategory(CategoryIndex::Piece, tr("Piece"), false);

	AddIdentityRow(IdentityIndex::Piece, CreatePieceIdEditor(swObjectPropertyId::PieceId, tr("Piece"), tr("Piece ID")));
	AddIdentityRow(IdentityIndex::Piece, CreatePieceColorEditor(swObjectPropertyId::PieceColor, tr("Color"), tr("Piece color")));

	AddValueRow(mStepRows, tr("S"), tr("Steps"),
	{
		{ CreateStepNumberEditor(swObjectPropertyId::PieceStepShow, tr("Added in Step"), tr("Added in Step: Step when the piece is added to the model")) },
		{ CreateStepNumberEditor(swObjectPropertyId::PieceStepHide, tr("Removed in Step"), tr("Removed in Step: Step when the piece is removed from the model, leave empty to keep it until the end")) }
	});

	AddCategory(CategoryIndex::Camera, tr("Camera"), false);

	AddIdentityRow(IdentityIndex::Camera, CreateStringEditor(swObjectPropertyId::CameraName, tr("Name"), tr("Camera name")));
	AddIdentityRow(IdentityIndex::Camera, CreateStringListEditor(swObjectPropertyId::CameraProjection, tr("Projection"), tr("Camera projection"), swCamera::GetCameraProjectionStrings()));

	AddValueRow(this, tr("F"), tr("Field of View and Clipping"),
	{
		{ CreateFloatEditor(swObjectPropertyId::CameraFOV, tr("FOV"), tr("Field of view in degrees"), 0.1f, 179.9f, true, tr("°")) },
		{ CreateFloatEditor(swObjectPropertyId::CameraNear, tr("Near"), tr("Near clipping distance"), 0.1f, FLT_MAX, true) },
		{ CreateFloatEditor(swObjectPropertyId::CameraFar, tr("Far"), tr("Far clipping distance"), 0.1f, FLT_MAX, true) }
	});

	AddCategory(CategoryIndex::CameraTransform, tr("Transform"), false);

	AddVectorRow(mTransformRows, { swObjectPropertyId::CameraPositionX, swObjectPropertyId::CameraPositionY, swObjectPropertyId::CameraPositionZ }, tr("P"), tr("Position"), tr("Camera position"), true, -FLT_MAX, FLT_MAX);
	AddVectorRow(mTransformRows, { swObjectPropertyId::CameraTargetX, swObjectPropertyId::CameraTargetY, swObjectPropertyId::CameraTargetZ }, tr("L"), tr("Look At"), tr("Camera target position"), true, -FLT_MAX, FLT_MAX);
	AddVectorRow(mTransformRows, { swObjectPropertyId::CameraUpX, swObjectPropertyId::CameraUpY, swObjectPropertyId::CameraUpZ }, tr("U"), tr("Up"), tr("Camera up direction"), true, -FLT_MAX, FLT_MAX);

	AddCategory(CategoryIndex::Light, tr("Light"), false);

	AddIdentityRow(IdentityIndex::Light, CreateStringEditor(swObjectPropertyId::LightName, tr("Name"), tr("Light name")));
	AddIdentityRow(IdentityIndex::Light, CreateStringListEditor(swObjectPropertyId::LightType, tr("Type"), tr("Light type"), swLight::GetLightTypeStrings()));
	AddIdentityRow(IdentityIndex::Light, CreateStringListEditor(swObjectPropertyId::LightAreaShape, tr("Area Shape"), tr("The shape of the area light"), swLight::GetAreaShapeStrings()));
	AddIdentityRow(IdentityIndex::Light, CreateColorEditor(swObjectPropertyId::LightColor, tr("Color"), tr("Light color")), swObjectPropertyId::LightColor);
	AddIdentityRow(IdentityIndex::Light, CreateToggleEditor(swObjectPropertyId::LightCastShadow, tr("Cast Shadow"), tr("Cast a shadow from this light")));

	AddValueRow(this, tr("A"), tr("Spotlight Angles"),
	{
		{ CreateFloatEditor(swObjectPropertyId::LightSpotConeAngle, tr("Cone"), tr("Angle in degrees of the spotlight's beam"), 0.0f, 179.9f, true, tr("°")), swObjectPropertyId::LightSpotConeAngle },
		{ CreateFloatEditor(swObjectPropertyId::LightSpotPenumbraAngle, tr("Penumbra"), tr("Angle in degrees over which the intensity of the spotlight falls off to zero"), 0.0f, 179.9f, true, tr("°")), swObjectPropertyId::LightSpotPenumbraAngle }
	});

	AddValueRow(this, tr("S"), tr("Area Size"),
	{
		{ CreateFloatEditor(swObjectPropertyId::LightAreaSizeX, tr("Size X"), tr("The width of the area light"), 0.0f, FLT_MAX, true), swObjectPropertyId::LightAreaSizeX },
		{ CreateFloatEditor(swObjectPropertyId::LightAreaSizeY, tr("Y"), tr("The height of the area light"), 0.0f, FLT_MAX, true), swObjectPropertyId::LightAreaSizeY }
	});

	AddCategory(CategoryIndex::LightBlender, tr("Blender Settings"), true);

	AddValueRow(this, tr("B"), tr("Blender"),
	{
		{ CreateFloatEditor(swObjectPropertyId::LightBlenderPower, tr("Power"), tr("Power of the light in watts"), 0.0f, FLT_MAX, true, tr(" W")), swObjectPropertyId::LightBlenderPower },
		{ CreateFloatEditor(swObjectPropertyId::LightBlenderRadius, tr("Radius"), tr("Shadow soft size"), 0.0f, FLT_MAX, true), swObjectPropertyId::LightBlenderRadius },
		{ CreateFloatEditor(swObjectPropertyId::LightBlenderAngle, tr("Angle"), tr("Angular diameter of the light"), 0.0f, 180.0f, true, tr("°")), swObjectPropertyId::LightBlenderAngle }
	});

	AddCategory(CategoryIndex::LightPOVRay, tr("POV-Ray Settings"), true);

	AddValueRow(this, tr("V"), tr("POV-Ray"),
	{
		{ CreateFloatEditor(swObjectPropertyId::LightPOVRayPower, tr("Power"), tr("Power of the light (multiplicative factor)"), 0.0f, FLT_MAX, true), swObjectPropertyId::LightPOVRayPower },
		{ CreateFloatEditor(swObjectPropertyId::LightPOVRaySpotTightness, tr("Tightness"), tr("Additional exponential spotlight edge softening"), 0.0f, FLT_MAX, true), swObjectPropertyId::LightPOVRaySpotTightness }
	});

	AddValueRow(this, tr("F"), tr("Fade"),
	{
		{ CreateFloatEditor(swObjectPropertyId::LightPOVRayFadeDistance, tr("Fade Distance"), tr("The distance at which the full light intensity arrives"), 0.0f, FLT_MAX, true), swObjectPropertyId::LightPOVRayFadeDistance },
		{ CreateFloatEditor(swObjectPropertyId::LightPOVRayFadePower, tr("Fade Power"), tr("Light falloff rate"), 0.0f, FLT_MAX, true), swObjectPropertyId::LightPOVRayFadePower }
	});

	AddValueRow(this, tr("G"), tr("Area Grid"),
	{
		{ CreateIntegerEditor(swObjectPropertyId::LightPOVRayAreaGridX, tr("Grid X"), tr("Number of point sources along the X axis (1 to 64)."), 1, SW_MAXIMUM_AREA_LIGHT_GRID), swObjectPropertyId::LightPOVRayAreaGridX },
		{ CreateIntegerEditor(swObjectPropertyId::LightPOVRayAreaGridY, tr("Y"), tr("Number of point sources along the Y axis (1 to 64)."), 1, SW_MAXIMUM_AREA_LIGHT_GRID), swObjectPropertyId::LightPOVRayAreaGridY }
	});

	AddCategory(CategoryIndex::ObjectTransform, tr("Transform"), false);

	AddVectorRow(mTransformRows, { swObjectPropertyId::ObjectPositionX, swObjectPropertyId::ObjectPositionY, swObjectPropertyId::ObjectPositionZ }, tr("P"), tr("Position"), tr("Position of the object"), true, -FLT_MAX, FLT_MAX);
	AddVectorRow(mTransformRows, { swObjectPropertyId::ObjectRotationX, swObjectPropertyId::ObjectRotationY, swObjectPropertyId::ObjectRotationZ }, tr("R"), tr("Rotation"), tr("Rotation of the object in degrees"), true, -FLT_MAX, FLT_MAX);

	mSettingsLayout->addStretch(1);
}

void swPropertiesWidget::SetCategoryWidgetsVisible(CategoryWidgets& Category, bool Visible)
{
	for (swObjectPropertyId PropertyId : Category.Properties)
		SetPropertyWidgetsVisible(PropertyId, Visible && mPropertyWidgets[static_cast<int>(PropertyId)].Visible);
}

void swPropertiesWidget::SetPropertyWidgetsVisible(swObjectPropertyId PropertyId, bool Visible)
{
	PropertyWidgets& Property = mPropertyWidgets[static_cast<int>(PropertyId)];

	if (Property.Editor)
		Property.Editor->setVisible(Visible);

	if (Property.KeyFrame)
		Property.KeyFrame->setVisible(Visible);

	UpdateRowVisible(Property.Row);
}

void swPropertiesWidget::SetLayoutMode(LayoutMode Mode)
{
	if (mLayoutMode == Mode)
		return;

	mLayoutMode = Mode;
	const bool IsPiece = (mLayoutMode == LayoutMode::Piece);
	const bool IsCamera = (mLayoutMode == LayoutMode::Camera);
	const bool IsLight = (mLayoutMode == LayoutMode::Light);

	SetCategoryVisible(CategoryIndex::Piece, IsPiece);
	SetCategoryVisible(CategoryIndex::Camera, IsCamera);
	SetCategoryVisible(CategoryIndex::CameraTransform, IsCamera);
	SetCategoryVisible(CategoryIndex::Light, IsLight);
	SetCategoryVisible(CategoryIndex::LightBlender, IsLight);
	SetCategoryVisible(CategoryIndex::LightPOVRay, IsLight);
	SetCategoryVisible(CategoryIndex::ObjectTransform, IsPiece || IsLight);

	for (IdentityBlock& Block : mIdentityBlocks)
		UpdateIdentityBlock(Block);
}


void swPropertiesWidget::SetPropertyVisible(swObjectPropertyId PropertyId, bool Visible)
{
	PropertyWidgets& Property = mPropertyWidgets[static_cast<int>(PropertyId)];
	Property.Visible = Visible;

	const CategoryWidgets& Category = mCategoryWidgets[static_cast<int>(Property.Category)];

	SetPropertyWidgetsVisible(PropertyId, Visible && (!Category.Button || Category.Button->IsExpanded()));
}


void swPropertiesWidget::SetCategoryVisible(CategoryIndex Index, bool Visible)
{
	CategoryWidgets& Category = mCategoryWidgets[static_cast<int>(Index)];

	if (Category.Button)
		Category.Button->setVisible(Visible);

	SetCategoryWidgetsVisible(Category, Visible && (!Category.Button || Category.Button->IsExpanded()));
}

void swPropertiesWidget::SetEmpty()
{
	// Keep the piece rows on screen, disabled, so the panel keeps its height.
	SetPiece({}, nullptr);
	setEnabled(false);

	mFocusObject = nullptr;
	mSelection.clear();
}

void swPropertiesWidget::SetPiece(const std::vector<swObject*>& Selection, swObject* Focus)
{
	SetLayoutMode(LayoutMode::Piece);

	swPiece* Piece = dynamic_cast<swPiece*>(Focus);
	mSelection = Selection;
	mFocusObject = Piece;

	swVector3 Position;
	swMatrix33 RelativeRotation;
	swModel* Model = gMainWindow->GetActiveModel();

	if (!Model)
		return;

	Model->GetMoveRotateTransform(Position, RelativeRotation);

	UpdateFloat(swObjectPropertyId::ObjectPositionX, Position[0]);
	UpdateFloat(swObjectPropertyId::ObjectPositionY, Position[1]);
	UpdateFloat(swObjectPropertyId::ObjectPositionZ, Position[2]);

	swVector3 Rotation;

	if (Piece)
		Rotation = swMatrix44ToEulerAngles(Piece->mModelWorld) * SW_RADIANS_TO_DEGREES;
	else
		Rotation = swVector3(0.0f, 0.0f, 0.0f);

	UpdateFloat(swObjectPropertyId::ObjectRotationX, Rotation[0]);
	UpdateFloat(swObjectPropertyId::ObjectRotationY, Rotation[1]);
	UpdateFloat(swObjectPropertyId::ObjectRotationZ, Rotation[2]);

	swStep StepShow = 1;
	swStep StepHide = SW_STEP_MAXIMUM;

	if (Piece)
	{
		StepShow = Piece->GetStepShow();
		StepHide = Piece->GetStepHide();
	}
	else
	{
		bool FirstPiece = true;

		for (swObject* Object : Selection)
		{
			if (!Object->IsPiece())
				continue;

			swPiece* SelectedPiece = (swPiece*)Object;

			if (FirstPiece)
			{
				StepShow = SelectedPiece->GetStepShow();
				StepHide = SelectedPiece->GetStepHide();

				FirstPiece = false;
			}
			else
			{
				if (SelectedPiece->GetStepShow() != StepShow)
					StepShow = 0;

				if (SelectedPiece->GetStepHide() != StepHide)
					StepHide = 0;
			}
		}
	}

	UpdatePieceId(swObjectPropertyId::PieceId);
	UpdatePieceColor(swObjectPropertyId::PieceColor);
	UpdateStepNumber(swObjectPropertyId::PieceStepShow, StepShow ? StepShow : SW_STEP_MAXIMUM, 1, StepHide - 1);
	UpdateStepNumber(swObjectPropertyId::PieceStepHide, StepHide ? StepHide : SW_STEP_MAXIMUM, StepShow + 1, SW_STEP_MAXIMUM);
}

void swPropertiesWidget::SetCamera(const std::vector<swObject*>& Selection, swObject* Focus)
{
	SetLayoutMode(LayoutMode::Camera);

	swCamera* Camera = dynamic_cast<swCamera*>(Focus);
	mSelection = Selection;
	mFocusObject = Camera;

	swVector3 Position(0.0f, 0.0f, 0.0f);
	swVector3 Target(0.0f, 0.0f, 0.0f);
	swVector3 UpVector(0.0f, 0.0f, 0.0f);
	float FoV = 60.0f;
	float ZNear = 1.0f;
	float ZFar = 100.0f;

	if (Camera)
	{
		Position = Camera->mPosition;
		Target = Camera->mTargetPosition;
		UpVector = Camera->mUpVector;

		FoV = Camera->m_fovy;
		ZNear = Camera->m_zNear;
		ZFar = Camera->m_zFar;
	}

	UpdateString(swObjectPropertyId::CameraName);
	UpdateStringList(swObjectPropertyId::CameraProjection);

	UpdateFloat(swObjectPropertyId::CameraFOV, FoV);
	UpdateFloat(swObjectPropertyId::CameraNear, ZNear);
	UpdateFloat(swObjectPropertyId::CameraFar, ZFar);

	UpdateFloat(swObjectPropertyId::CameraPositionX, Position[0]);
	UpdateFloat(swObjectPropertyId::CameraPositionY, Position[1]);
	UpdateFloat(swObjectPropertyId::CameraPositionZ, Position[2]);

	UpdateFloat(swObjectPropertyId::CameraTargetX, Target[0]);
	UpdateFloat(swObjectPropertyId::CameraTargetY, Target[1]);
	UpdateFloat(swObjectPropertyId::CameraTargetZ, Target[2]);

	UpdateFloat(swObjectPropertyId::CameraUpX, UpVector[0]);
	UpdateFloat(swObjectPropertyId::CameraUpY, UpVector[1]);
	UpdateFloat(swObjectPropertyId::CameraUpZ, UpVector[2]);
}

void swPropertiesWidget::SetLight(const std::vector<swObject*>& Selection, swObject* Focus)
{
	SetLayoutMode(LayoutMode::Light);

	swLight* Light = dynamic_cast<swLight*>(Focus);
	mSelection = Selection;
	mFocusObject = Light;

	swLightType LightType = swLightType::Count;
	swLightAreaShape LightAreaShape = swLightAreaShape::Count;
	float BlenderRadius = 0.0f, BlenderAngle = 0.0f;
	float AreaSizeX = 0.0f, AreaSizeY = 0.0f;
	float BlenderPower = 0.0f, POVRayPower = 0.0f;
	float POVRayFadeDistance = 0.0f;
	float POVRayFadePower = 0.0f;
	swVector3 Position(0.0f, 0.0f, 0.0f);
	swVector3 Rotation = swVector3(0.0f, 0.0f, 0.0f);
	float SpotConeAngle = 0.0f, SpotPenumbraAngle = 0.0f, SpotTightness = 0.0f;

	if (Light)
	{
		LightType = Light->GetLightType();
		LightAreaShape = Light->GetAreaShape();

		Position = Light->GetPosition();
		Rotation = swMatrix44ToEulerAngles(Light->GetWorldMatrix()) * SW_RADIANS_TO_DEGREES;
		BlenderPower = Light->GetBlenderPower();
		POVRayPower = Light->GetPOVRayPower();
		POVRayFadeDistance = Light->GetPOVRayFadeDistance();
		POVRayFadePower = Light->GetPOVRayFadePower();
		SpotConeAngle = Light->GetSpotConeAngle();
		SpotPenumbraAngle = Light->GetSpotPenumbraAngle();
		SpotTightness = Light->GetSpotPOVRayTightness();
		BlenderRadius = Light->GetBlenderRadius();
		BlenderAngle = Light->GetBlenderAngle();
		AreaSizeX = Light->GetAreaSizeX();
		AreaSizeY = Light->GetAreaSizeY();
	}
	else
	{
		bool First = true;
		bool PartialLightType = false, PartialAreaShape = false;

		for (const swObject* Object : mSelection)
		{
			const swLight* CurrentLight = dynamic_cast<const swLight*>(Object);

			if (!CurrentLight)
				continue;

			if (First)
			{
				LightType = CurrentLight->GetLightType();
				LightAreaShape = CurrentLight->GetAreaShape();
				First = false;
			}
			else
			{
				if (LightType != CurrentLight->GetLightType())
					PartialLightType = true;

				if (LightAreaShape != CurrentLight->GetAreaShape())
					PartialAreaShape = true;
			}
		}

		if (PartialLightType)
			LightType = swLightType::Count;

		if (PartialAreaShape)
			LightAreaShape = swLightAreaShape::Count;
	}

	UpdateString(swObjectPropertyId::LightName);
	UpdateStringList(swObjectPropertyId::LightType);
	UpdateColor(swObjectPropertyId::LightColor);

	UpdateFloat(swObjectPropertyId::LightBlenderPower, BlenderPower);
	UpdateFloat(swObjectPropertyId::LightPOVRayPower, POVRayPower);
	UpdateBool(swObjectPropertyId::LightCastShadow);

	UpdateFloat(swObjectPropertyId::LightPOVRayFadeDistance, POVRayFadeDistance);
	UpdateFloat(swObjectPropertyId::LightPOVRayFadePower, POVRayFadePower);

	const bool IsPointLight = (LightType == swLightType::Point);
	const bool IsSpotLight = (LightType == swLightType::Spot);

	SetPropertyVisible(swObjectPropertyId::LightBlenderRadius, IsPointLight || IsSpotLight);

	if (IsPointLight || IsSpotLight)
		UpdateFloat(swObjectPropertyId::LightBlenderRadius, BlenderRadius);

	SetPropertyVisible(swObjectPropertyId::LightSpotConeAngle, IsSpotLight);
	SetPropertyVisible(swObjectPropertyId::LightSpotPenumbraAngle, IsSpotLight);
	SetPropertyVisible(swObjectPropertyId::LightPOVRaySpotTightness, IsSpotLight);

	if (IsSpotLight)
	{
		UpdateFloat(swObjectPropertyId::LightSpotConeAngle, SpotConeAngle);
		UpdateFloat(swObjectPropertyId::LightSpotPenumbraAngle, SpotPenumbraAngle);
		UpdateFloat(swObjectPropertyId::LightPOVRaySpotTightness, SpotTightness);
	}

	const bool IsDirectionalLight = (LightType == swLightType::Directional);
	SetPropertyVisible(swObjectPropertyId::LightBlenderAngle, IsDirectionalLight);

	if (IsDirectionalLight)
		UpdateFloat(swObjectPropertyId::LightBlenderAngle, BlenderAngle);

	const bool IsAreaLight = (LightType == swLightType::Area);
	SetPropertyVisible(swObjectPropertyId::LightAreaShape, IsAreaLight);

	const bool IsSquare = (LightAreaShape == swLightAreaShape::Square || LightAreaShape == swLightAreaShape::Disk);
	SetPropertyVisible(swObjectPropertyId::LightAreaSizeX, IsAreaLight);
	SetPropertyVisible(swObjectPropertyId::LightAreaSizeY, IsAreaLight && !IsSquare);

	// Square and disk shapes have a single size.
	if (swDoubleSpinBox* SizeXEditor = qobject_cast<swDoubleSpinBox*>(mPropertyWidgets[static_cast<int>(swObjectPropertyId::LightAreaSizeX)].Editor))
		SizeXEditor->setPrefix(IsSquare ? tr("Size ") : tr("Size X "));

	SetPropertyVisible(swObjectPropertyId::LightPOVRayAreaGridX, IsAreaLight);
	SetPropertyVisible(swObjectPropertyId::LightPOVRayAreaGridY, IsAreaLight);

	if (IsAreaLight)
	{
		UpdateStringList(swObjectPropertyId::LightAreaShape);
		UpdateFloat(swObjectPropertyId::LightAreaSizeX, AreaSizeX);
		UpdateFloat(swObjectPropertyId::LightAreaSizeY, AreaSizeY);
		UpdateInteger(swObjectPropertyId::LightPOVRayAreaGridX);
		UpdateInteger(swObjectPropertyId::LightPOVRayAreaGridY);
	}

	UpdateFloat(swObjectPropertyId::ObjectPositionX, Position[0]);
	UpdateFloat(swObjectPropertyId::ObjectPositionY, Position[1]);
	UpdateFloat(swObjectPropertyId::ObjectPositionZ, Position[2]);

	UpdateFloat(swObjectPropertyId::ObjectRotationX, Rotation[0]);
	UpdateFloat(swObjectPropertyId::ObjectRotationY, Rotation[1]);
	UpdateFloat(swObjectPropertyId::ObjectRotationZ, Rotation[2]);
}

void swPropertiesWidget::Update(const std::vector<swObject*>& Selection, swObject* Focus)
{
	if (mDisableUpdates)
		return;

	mLastRotation.reset();
	mFocusObject = nullptr;
	mSelection.clear();
	setEnabled(true);

	LayoutMode Mode = LayoutMode::Empty;

	if (Focus)
	{
		switch (Focus->GetType())
		{
		case swObjectType::Piece:
			Mode = LayoutMode::Piece;
			break;

		case swObjectType::Camera:
			Mode = LayoutMode::Camera;
			break;

		case swObjectType::Light:
			Mode = LayoutMode::Light;
			break;
		}
	}
	else
	{
		for (size_t ObjectIdx = 0; ObjectIdx < Selection.size(); ObjectIdx++)
		{
			switch (Selection[ObjectIdx]->GetType())
			{
			case swObjectType::Piece:
				if (Mode == LayoutMode::Empty)
					Mode = LayoutMode::Piece;
				else if (Mode != LayoutMode::Piece)
				{
					Mode = LayoutMode::Multiple;
					ObjectIdx = Selection.size();
				}
				break;

			case swObjectType::Camera:
				if (Mode == LayoutMode::Empty)
					Mode = LayoutMode::Camera;
				else if (Mode != LayoutMode::Camera)
				{
					Mode = LayoutMode::Multiple;
					ObjectIdx = Selection.size();
				}
				break;

			case swObjectType::Light:
				if (Mode == LayoutMode::Empty)
					Mode = LayoutMode::Light;
				else if (Mode != LayoutMode::Light)
				{
					Mode = LayoutMode::Multiple;
					ObjectIdx = Selection.size();
				}
				break;
			}
		}
	}

	switch (Mode)
	{
	case LayoutMode::Empty:
	case LayoutMode::Multiple:
	case LayoutMode::Count:
		SetEmpty();
		break;

	case LayoutMode::Piece:
		SetPiece(Selection, Focus);
		break;

	case LayoutMode::Camera:
		SetCamera(Selection, Focus);
		break;

	case LayoutMode::Light:
		SetLight(Selection, Focus);
		break;
	}
}
