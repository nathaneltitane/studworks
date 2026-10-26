#include "lc_global.h"
#include "lc_propertieswidget.h"
#include "lc_keyframewidget.h"
#include "lc_doublespinbox.h"
#include "object.h"
#include "piece.h"
#include "camera.h"
#include "light.h"
#include "lc_piecedata.h"
#include "lc_mainwindow.h"
#include "lc_collapsiblewidget.h"
#include "lc_colorpicker.h"
#include "lc_qutils.h"
#include "lc_partselectionpopup.h"
#include "lc_modelhistory.h"

lcPropertiesWidget::lcPropertiesWidget(QWidget* Parent)
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


lcObjectPropertyId lcPropertiesWidget::GetEditorWidgetPropertyId(QWidget* Widget) const
{
	if (!Widget)
		return lcObjectPropertyId::Count;

	for (size_t Index = 0; Index < mPropertyWidgets.size(); Index++)
		if (mPropertyWidgets[Index].Editor == Widget)
			return static_cast<lcObjectPropertyId>(Index);

	return lcObjectPropertyId::Count;
}

lcObjectPropertyId lcPropertiesWidget::GetKeyFrameWidgetPropertyId(QWidget* Widget) const
{
	if (!Widget)
		return lcObjectPropertyId::Count;

	for (size_t Index = 0; Index < mPropertyWidgets.size(); Index++)
		if (mPropertyWidgets[Index].KeyFrame == Widget)
			return static_cast<lcObjectPropertyId>(Index);

	return lcObjectPropertyId::Count;
}

bool lcPropertiesWidget::event(QEvent* Event)
{
	const bool Result = QWidget::event(Event);

	// Shown rows changed, so the panel can be resized to fit them.
	if (Event->type() == QEvent::LayoutRequest)
		emit LayoutChanged();

	return Result;
}




void lcPropertiesWidget::CategoryStateChanged(bool Expanded)
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




void lcPropertiesWidget::KeyFrameChanged()
{
	QCheckBox* Widget = qobject_cast<QCheckBox*>(sender());
	lcObjectPropertyId PropertyId = GetKeyFrameWidgetPropertyId(Widget);

	if (PropertyId == lcObjectPropertyId::Count)
		return;

	lcModel* Model = gMainWindow->GetActiveModel();

	if (!Model)
		return;

	if (mFocusObject)
		Model->SetObjectsKeyFrame({ mFocusObject }, PropertyId, Widget->isChecked());
	else
		Model->SetObjectsKeyFrame(mSelection, PropertyId, Widget->isChecked());
}

void lcPropertiesWidget::UpdateKeyFrameWidget(lcObjectPropertyId PropertyId)
{
	lcKeyFrameWidget* Widget = mPropertyWidgets[static_cast<int>(PropertyId)].KeyFrame;

	if (Widget)
	{
		QSignalBlocker Blocker(Widget);
		lcModel* Model = gMainWindow->GetActiveModel();

		if (Model)
		{
			const lcStep Step = Model->GetCurrentStep();

			if (mFocusObject)
				Widget->setChecked(mFocusObject->HasKeyFrame(PropertyId, Step));
			else
			{
				int KeyFrameCount = 0, NonKeyFrameCount = 0;

				for (const lcObject* Object : mSelection)
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



std::pair<QVariant, bool> lcPropertiesWidget::GetUpdateValue(lcObjectPropertyId PropertyId)
{
	QVariant Value;
	bool Partial = false;

	if (mFocusObject)
		Value = mFocusObject->GetPropertyValue(PropertyId);
	else
	{
		bool First = true;

		for (const lcObject* Object : mSelection)
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

void lcPropertiesWidget::BoolChanged()
{
	QAbstractButton* Widget = qobject_cast<QAbstractButton*>(sender());
	lcObjectPropertyId PropertyId = GetEditorWidgetPropertyId(Widget);

	if (PropertyId == lcObjectPropertyId::Count)
		return;

	lcModel* Model = gMainWindow->GetActiveModel();

	if (!Model)
		return;

	const bool Value = Widget->isChecked();
	Model->SetObjectsProperty(mFocusObject ? std::vector<lcObject*>{ mFocusObject } : mSelection, PropertyId, Value);
}

void lcPropertiesWidget::UpdateBool(lcObjectPropertyId PropertyId)
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


void lcPropertiesWidget::FloatEditingStarted()
{
	lcDoubleSpinBox* Widget = qobject_cast<lcDoubleSpinBox*>(sender());
	lcObjectPropertyId PropertyId = GetEditorWidgetPropertyId(Widget);

	if (PropertyId == lcObjectPropertyId::Count)
		return;

	lcModel* Model = gMainWindow->GetActiveModel();

	if (Model)
		Model->BeginPropertyEdit(PropertyId);
}

void lcPropertiesWidget::FloatEditingFinished()
{
	lcDoubleSpinBox* Widget = qobject_cast<lcDoubleSpinBox*>(sender());
	lcObjectPropertyId PropertyId = GetEditorWidgetPropertyId(Widget);

	if (PropertyId == lcObjectPropertyId::Count)
		return;

	lcModel* Model = gMainWindow->GetActiveModel();

	if (!Model)
		return;

	Model->EndPropertyEdit(PropertyId, true);
}

void lcPropertiesWidget::FloatEditingCanceled()
{
	lcDoubleSpinBox* Widget = qobject_cast<lcDoubleSpinBox*>(sender());
	lcObjectPropertyId PropertyId = GetEditorWidgetPropertyId(Widget);

	if (PropertyId == lcObjectPropertyId::Count)
		return;

	lcModel* Model = gMainWindow->GetActiveModel();

	if (!Model)
		return;

	Model->EndPropertyEdit(PropertyId, false);
}

void lcPropertiesWidget::FloatChanged(double Value)
{
	lcDoubleSpinBox* Widget = qobject_cast<lcDoubleSpinBox*>(sender());
	lcObjectPropertyId PropertyId = GetEditorWidgetPropertyId(Widget);

	ChangeFloatValue(PropertyId, Value);
}

void lcPropertiesWidget::ChangeFloatValue(lcObjectPropertyId PropertyId, float Value)
{
	if (PropertyId == lcObjectPropertyId::Count)
		return;

	lcModel* Model = gMainWindow->GetActiveModel();

	if (!Model)
		return;

	lcPiece* Piece = dynamic_cast<lcPiece*>(mFocusObject);
	lcCamera* Camera = dynamic_cast<lcCamera*>(mFocusObject);
	lcLight* Light = dynamic_cast<lcLight*>(mFocusObject);

	mDisableUpdates = true;

	if (PropertyId == lcObjectPropertyId::ObjectPositionX || PropertyId == lcObjectPropertyId::ObjectPositionY || PropertyId == lcObjectPropertyId::ObjectPositionZ)
	{
		lcVector3 Center;
		lcMatrix33 RelativeRotation;
		Model->GetMoveRotateTransform(Center, RelativeRotation);
		lcVector3 Position = Center;

		if (PropertyId == lcObjectPropertyId::ObjectPositionX)
			Position[0] = Value;
		else if (PropertyId == lcObjectPropertyId::ObjectPositionY)
			Position[1] = Value;
		else if (PropertyId == lcObjectPropertyId::ObjectPositionZ)
			Position[2] = Value;

		lcVector3 Distance = Position - Center;

		Model->MoveSelectedObjects(Distance, lcModelTransformFlag::FirstMove, lcModelHistoryEditMerge::PropertiesMove);
	}
	else if (PropertyId == lcObjectPropertyId::ObjectRotationX || PropertyId == lcObjectPropertyId::ObjectRotationY || PropertyId == lcObjectPropertyId::ObjectRotationZ)
	{
		lcVector3 InitialRotation(0.0f, 0.0f, 0.0f);

		if (mLastRotation)
			InitialRotation = mLastRotation.value();
		else if (Piece)
			InitialRotation = lcMatrix44ToEulerAngles(Piece->mModelWorld) * LC_RTOD;
		else if (Light)
			InitialRotation = lcMatrix44ToEulerAngles(Light->GetWorldMatrix()) * LC_RTOD;

		lcVector3 Rotation = InitialRotation;

		if (PropertyId == lcObjectPropertyId::ObjectRotationX)
			Rotation[0] = Value;
		else if (PropertyId == lcObjectPropertyId::ObjectRotationY)
			Rotation[1] = Value;
		else if (PropertyId == lcObjectPropertyId::ObjectRotationZ)
			Rotation[2] = Value;

		Model->RotateSelectedObjects(Rotation - InitialRotation, lcModelTransformFlag::Relative, lcModelHistoryEditMerge::PropertiesRotate);

		mLastRotation = Rotation;
	}
	else if (PropertyId == lcObjectPropertyId::CameraPositionX || PropertyId == lcObjectPropertyId::CameraPositionY || PropertyId == lcObjectPropertyId::CameraPositionZ)
	{
		quint32 FocusSection = LC_CAMERA_SECTION_INVALID;
		lcVector3 Start;

		if (Camera)
		{
			FocusSection = Camera->GetFocusSection();
			Camera->SetFocused(FocusSection, false);
			Camera->SetFocused(LC_CAMERA_SECTION_POSITION, true);

			Start = Camera->mPosition;
		}
		else
		{
			Start = lcVector3(0.0f, 0.0f, 0.0f);
		}

		lcVector3 End = Start;

		if (PropertyId == lcObjectPropertyId::CameraPositionX)
			End[0] = Value;
		else if (PropertyId == lcObjectPropertyId::CameraPositionY)
			End[1] = Value;
		else if (PropertyId == lcObjectPropertyId::CameraPositionZ)
			End[2] = Value;

		lcVector3 Distance = End - Start;

		Model->MoveSelectedObjects(Distance, lcModelTransformFlag::FirstMove, lcModelHistoryEditMerge::PropertiesMove);

		if (Camera)
		{
			Camera->SetFocused(LC_CAMERA_SECTION_POSITION, false);
			Camera->SetFocused(FocusSection, true);
		}
	}
	else if (PropertyId == lcObjectPropertyId::CameraTargetX || PropertyId == lcObjectPropertyId::CameraTargetY || PropertyId == lcObjectPropertyId::CameraTargetZ)
	{
		quint32 FocusSection = LC_CAMERA_SECTION_INVALID;
		lcVector3 Start;

		if (Camera)
		{
			FocusSection = Camera->GetFocusSection();
			Camera->SetFocused(FocusSection, false);
			Camera->SetFocused(LC_CAMERA_SECTION_TARGET, true);

			Start = Camera->mTargetPosition;
		}
		else
		{
			Start = lcVector3(0.0f, 0.0f, 0.0f);
		}

		lcVector3 End = Start;

		if (PropertyId == lcObjectPropertyId::CameraTargetX)
			End[0] = Value;
		else if (PropertyId == lcObjectPropertyId::CameraTargetY)
			End[1] = Value;
		else if (PropertyId == lcObjectPropertyId::CameraTargetZ)
			End[2] = Value;

		lcVector3 Distance = End - Start;

		Model->MoveSelectedObjects(Distance, lcModelTransformFlag::FirstMove, lcModelHistoryEditMerge::PropertiesMove);

		if (Camera)
		{
			Camera->SetFocused(LC_CAMERA_SECTION_TARGET, false);
			Camera->SetFocused(FocusSection, true);
		}
	}
	else if (PropertyId == lcObjectPropertyId::CameraUpX || PropertyId == lcObjectPropertyId::CameraUpY || PropertyId == lcObjectPropertyId::CameraUpZ)
	{
		quint32 FocusSection = LC_CAMERA_SECTION_INVALID;
		lcVector3 Start;

		if (Camera)
		{
			FocusSection = Camera->GetFocusSection();
			Camera->SetFocused(FocusSection, false);
			Camera->SetFocused(LC_CAMERA_SECTION_UPVECTOR, true);

			Start = Camera->mUpVector;
		}
		else
		{
			Start = lcVector3(0.0f, 0.0f, 0.0f);
		}

		lcVector3 End = Start;

		if (PropertyId == lcObjectPropertyId::CameraUpX)
			End[0] = Value;
		else if (PropertyId == lcObjectPropertyId::CameraUpY)
			End[1] = Value;
		else if (PropertyId == lcObjectPropertyId::CameraUpZ)
			End[2] = Value;

		lcVector3 Distance = End - Start;

		Model->MoveSelectedObjects(Distance, lcModelTransformFlag::FirstMove, lcModelHistoryEditMerge::PropertiesMove);

		if (Camera)
		{
			Camera->SetFocused(LC_CAMERA_SECTION_UPVECTOR, false);
			Camera->SetFocused(FocusSection, true);
		}
	}
	else
	{
		Model->SetObjectsProperty(mFocusObject ? std::vector<lcObject*>{ mFocusObject } : mSelection, PropertyId, Value);
	}

	mDisableUpdates = false;
}

void lcPropertiesWidget::UpdateFloat(lcObjectPropertyId PropertyId, float Value)
{
	lcDoubleSpinBox* Widget = qobject_cast<lcDoubleSpinBox*>(mPropertyWidgets[static_cast<int>(PropertyId)].Editor);

	if (Widget)
	{
		QSignalBlocker Blocker(Widget);

		switch (PropertyId)
		{
		case lcObjectPropertyId::PieceId:
		case lcObjectPropertyId::PieceColor:
		case lcObjectPropertyId::PieceStepShow:
		case lcObjectPropertyId::PieceStepHide:
		case lcObjectPropertyId::CameraName:
		case lcObjectPropertyId::CameraProjection:
			break;

		case lcObjectPropertyId::CameraFOV:
		case lcObjectPropertyId::CameraNear:
		case lcObjectPropertyId::CameraFar:
			Widget->SetSnap(lcFloatPropertySnap::Auto);
			break;

		case lcObjectPropertyId::CameraPositionX:
		case lcObjectPropertyId::CameraPositionY:
		case lcObjectPropertyId::CameraPositionZ:
		case lcObjectPropertyId::CameraTargetX:
		case lcObjectPropertyId::CameraTargetY:
		case lcObjectPropertyId::CameraTargetZ:
			Widget->SetSnap(lcFloatPropertySnap::Position);
			break;

		case lcObjectPropertyId::CameraUpX:
		case lcObjectPropertyId::CameraUpY:
		case lcObjectPropertyId::CameraUpZ:
			Widget->SetSnap(lcFloatPropertySnap::Auto);
			break;

		case lcObjectPropertyId::LightName:
		case lcObjectPropertyId::LightType:
		case lcObjectPropertyId::LightColor:
			break;

		case lcObjectPropertyId::LightBlenderPower:
		case lcObjectPropertyId::LightPOVRayPower:
		case lcObjectPropertyId::LightCastShadow:
		case lcObjectPropertyId::LightPOVRayFadeDistance:
		case lcObjectPropertyId::LightPOVRayFadePower:
		case lcObjectPropertyId::LightBlenderRadius:
		case lcObjectPropertyId::LightBlenderAngle:
		case lcObjectPropertyId::LightAreaSizeX:
		case lcObjectPropertyId::LightAreaSizeY:
		case lcObjectPropertyId::LightSpotConeAngle:
		case lcObjectPropertyId::LightSpotPenumbraAngle:
		case lcObjectPropertyId::LightPOVRaySpotTightness:
			Widget->SetSnap(lcFloatPropertySnap::Auto);
			break;

		case lcObjectPropertyId::LightAreaShape:
		case lcObjectPropertyId::LightPOVRayAreaGridX:
		case lcObjectPropertyId::LightPOVRayAreaGridY:
			break;

		case lcObjectPropertyId::ObjectPositionX:
		case lcObjectPropertyId::ObjectPositionY:
			Widget->SetSnap(mLayoutMode == LayoutMode::Piece ? lcFloatPropertySnap::PiecePositionXY : lcFloatPropertySnap::Position);
			break;

		case lcObjectPropertyId::ObjectPositionZ:
			Widget->SetSnap(mLayoutMode == LayoutMode::Piece ? lcFloatPropertySnap::PiecePositionZ : lcFloatPropertySnap::Position);
			break;

		case lcObjectPropertyId::ObjectRotationX:
		case lcObjectPropertyId::ObjectRotationY:
		case lcObjectPropertyId::ObjectRotationZ:
			Widget->SetSnap(lcFloatPropertySnap::Rotation);
			break;

		case lcObjectPropertyId::Count:
			break;
		}

		Widget->SetValue(Value);
	}

	UpdateKeyFrameWidget(PropertyId);
}




void lcPropertiesWidget::IntegerChanged()
{
	QSpinBox* SpinBox = qobject_cast<QSpinBox*>(sender());
	lcObjectPropertyId PropertyId = GetEditorWidgetPropertyId(SpinBox);

	if (PropertyId == lcObjectPropertyId::Count)
		return;

	lcModel* Model = gMainWindow->GetActiveModel();

	// The value below the range only stands for differing values.
	if (!Model || (SpinBox->value() == SpinBox->minimum() && !SpinBox->specialValueText().isEmpty()))
		return;

	Model->SetObjectsProperty(mFocusObject ? std::vector<lcObject*>{ mFocusObject } : mSelection, PropertyId, SpinBox->value());
}

void lcPropertiesWidget::UpdateInteger(lcObjectPropertyId PropertyId)
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


void lcPropertiesWidget::StepNumberChanged()
{
	QLineEdit* Widget = qobject_cast<QLineEdit*>(sender());
	lcObjectPropertyId PropertyId = GetEditorWidgetPropertyId(Widget);

	if (PropertyId == lcObjectPropertyId::Count)
		return;

	lcModel* Model = gMainWindow->GetActiveModel();

	if (!Model)
		return;

	bool Ok = true;
	QString Text = Widget->text();
	lcStep Step = Text.isEmpty() && PropertyId == lcObjectPropertyId::PieceStepHide ? LC_STEP_MAX : Widget->validator()->locale().toUInt(Text, &Ok);

	if (!Ok || !Widget->hasAcceptableInput())
		return;

	if (PropertyId == lcObjectPropertyId::PieceStepShow)
	{
		Model->SetSelectedPiecesStepShow(Step);
	}
	else if (PropertyId == lcObjectPropertyId::PieceStepHide)
	{
		Model->SetSelectedPiecesStepHide(Step);
	}
}

void lcPropertiesWidget::UpdateStepNumber(lcObjectPropertyId PropertyId, lcStep Step, lcStep Min, lcStep Max)
{
	QLineEdit* Widget = qobject_cast<QLineEdit*>(mPropertyWidgets[static_cast<int>(PropertyId)].Editor);

	if (Widget)
	{
		QSignalBlocker Blocker(Widget);

		Widget->setValidator(new lcStepValidator(Min, Max, PropertyId == lcObjectPropertyId::PieceStepHide, Widget));
		Widget->setText(Step == LC_STEP_MAX ? QString() : lcFormatIntegerLocalized(Step, Widget->validator()->locale()));
	}

	UpdateKeyFrameWidget(PropertyId);
}






void lcPropertiesWidget::StringChanged()
{
	QLineEdit* LineEdit = qobject_cast<QLineEdit*>(sender());
	lcObjectPropertyId PropertyId = GetEditorWidgetPropertyId(LineEdit);

	if (PropertyId == lcObjectPropertyId::Count)
		return;

	lcModel* Model = gMainWindow->GetActiveModel();

	if (!Model)
		return;

	QString Value = LineEdit->text();
	Model->SetObjectsProperty(mFocusObject ? std::vector<lcObject*>{ mFocusObject } : mSelection, PropertyId, Value);
}

void lcPropertiesWidget::UpdateString(lcObjectPropertyId PropertyId)
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


void lcPropertiesWidget::StringListChanged(int Value)
{
	QComboBox* ComboBox = qobject_cast<QComboBox*>(sender());
	lcObjectPropertyId PropertyId = GetEditorWidgetPropertyId(ComboBox);

	if (PropertyId == lcObjectPropertyId::Count)
		return;

	lcModel* Model = gMainWindow->GetActiveModel();

	if (!Model)
		return;

	Model->SetObjectsProperty(mFocusObject ? std::vector<lcObject*>{ mFocusObject } : mSelection, PropertyId, Value);
}

void lcPropertiesWidget::UpdateStringList(lcObjectPropertyId PropertyId)
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


void lcPropertiesWidget::ColorButtonClicked()
{
	QToolButton* ColorButton = qobject_cast<QToolButton*>(sender());
	lcObjectPropertyId PropertyId = GetEditorWidgetPropertyId(ColorButton);

	if (PropertyId == lcObjectPropertyId::Count)
		return;

	QVariant Value;
	bool Partial;

	std::tie(Value, Partial) = GetUpdateValue(PropertyId);

	QColor InitialColor = Partial ? QColor(128, 128, 128) : lcQColorFromVector3(Value.value<lcVector3>());

	QMenu* Menu = new QMenu(ColorButton);

	QWidgetAction* Action = new QWidgetAction(Menu);
	lcColorDialogPopup *Popup = new lcColorDialogPopup(InitialColor, Menu);
	Action->setDefaultWidget(Popup);
	Menu->addAction(Action);

	connect(Popup, &lcColorDialogPopup::ColorSelected, this, &lcPropertiesWidget::ColorChanged);

	Menu->exec(ColorButton->mapToGlobal(ColorButton->rect().bottomLeft()));

	delete Menu;
}

void lcPropertiesWidget::ColorChanged(QColor Color)
{
	lcColorDialogPopup* Popup = qobject_cast<lcColorDialogPopup*>(sender());
	QMenu* Menu = qobject_cast<QMenu*>(Popup->parent());
	QToolButton* ColorButton = qobject_cast<QToolButton*>(Menu->parent());
	lcObjectPropertyId PropertyId = GetEditorWidgetPropertyId(ColorButton);

	if (!Color.isValid())
		return;

	lcModel* Model = gMainWindow->GetActiveModel();

	if (!Model)
		return;

	const lcVector3 FloatColor = lcVector3FromQColor(Color);
	Model->SetObjectsProperty(mFocusObject ? std::vector<lcObject*>{ mFocusObject } : mSelection, PropertyId, QVariant::fromValue<lcVector3>(FloatColor));
}

void lcPropertiesWidget::UpdateColor(lcObjectPropertyId PropertyId)
{
	QToolButton* ColorButton = qobject_cast<QToolButton*>(mPropertyWidgets[static_cast<int>(PropertyId)].Editor);

	if (!ColorButton)
		return;

	QSignalBlocker Blocker(ColorButton);
	QVariant Value;
	bool Partial;

	std::tie(Value, Partial) = GetUpdateValue(PropertyId);

	QColor Color = Partial ? QColor(128, 128, 128) : lcQColorFromVector3(Value.value<lcVector3>());

	// The color shows in the light block so the name stays centered like the piece color.
	mIdentityBlocks[static_cast<int>(IdentityIndex::Light)].Color = Color;
	UpdateIdentityBlock(mIdentityBlocks[static_cast<int>(IdentityIndex::Light)]);

	ColorButton->setText(Partial ? tr("Multiple Colors") : Color.name());

	UpdateKeyFrameWidget(PropertyId);
}


void lcPropertiesWidget::PieceColorChanged(int ColorIndex)
{
	lcColorPickerPopup* Popup = qobject_cast<lcColorPickerPopup*>(sender());
	QMenu* Menu = qobject_cast<QMenu*>(Popup->parent());
	QToolButton* ColorButton = qobject_cast<QToolButton*>(Menu->parent());
	lcObjectPropertyId PropertyId = GetEditorWidgetPropertyId(ColorButton);

	Menu->close();

	if (PropertyId == lcObjectPropertyId::Count)
		return;

	lcModel* Model = gMainWindow->GetActiveModel();

	if (!Model)
		return;

	Model->SetObjectsProperty(mFocusObject ? std::vector<lcObject*>{ mFocusObject } : mSelection, PropertyId, ColorIndex);
}

void lcPropertiesWidget::PieceColorButtonClicked()
{
	QToolButton* ColorButton = qobject_cast<QToolButton*>(sender());
	lcObjectPropertyId PropertyId = GetEditorWidgetPropertyId(ColorButton);

	if (!ColorButton)
		return;

	QVariant Value;
	bool Partial;

	std::tie(Value, Partial) = GetUpdateValue(PropertyId);

	int ColorIndex = Partial ? gDefaultColor : Value.toInt();

	QMenu* Menu = new QMenu(ColorButton);

	QWidgetAction* Action = new QWidgetAction(Menu);
	lcColorPickerPopup* Popup = new lcColorPickerPopup(Menu, ColorIndex);
	Action->setDefaultWidget(Popup);
	Menu->addAction(Action);

	connect(Popup, &lcColorPickerPopup::Selected, this, &lcPropertiesWidget::PieceColorChanged);

	Menu->exec(ColorButton->mapToGlobal(ColorButton->rect().bottomLeft()));

	delete Menu;
}

void lcPropertiesWidget::UpdatePieceColor(lcObjectPropertyId PropertyId)
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


void lcPropertiesWidget::UpdatePieceId(lcObjectPropertyId PropertyId)
{
	lcElidableToolButton* PieceIdButton = qobject_cast<lcElidableToolButton*>(mPropertyWidgets[static_cast<int>(PropertyId)].Editor);

	if (!PieceIdButton)
		return;

	QSignalBlocker Blocker(PieceIdButton);
	QVariant Value;
	bool Partial;

	std::tie(Value, Partial) = GetUpdateValue(PropertyId);

	lcPieceData* PieceData = static_cast<lcPieceData*>(Value.value<void*>());

	if (Partial)
		PieceIdButton->setText(tr("Multiple Pieces"));
	else if (PieceData)
		PieceIdButton->setText(PieceData->m_strDescription);
	else
		PieceIdButton->setText(tr("Piece"));

	UpdateKeyFrameWidget(PropertyId);
}

void lcPropertiesWidget::PieceIdButtonClicked()
{
	QToolButton* PieceIdButton = qobject_cast<QToolButton*>(sender());
	lcObjectPropertyId PropertyId = GetEditorWidgetPropertyId(PieceIdButton);

	if (!PieceIdButton)
		return;

	QVariant Value;
	bool Partial;

	std::tie(Value, Partial) = GetUpdateValue(PropertyId);

	lcPieceData* PieceData = Partial ? nullptr : static_cast<lcPieceData*>(Value.value<void*>());

	std::tie(Value, Partial) = GetUpdateValue(lcObjectPropertyId::PieceColor);

	int ColorIndex = Partial ? gDefaultColor : Value.toInt();

	std::optional<lcPieceData*> Result = lcShowPartSelectionPopup(PieceData, std::vector<std::pair<lcPieceData*, std::string>>(), ColorIndex, PieceIdButton, PieceIdButton->mapToGlobal(PieceIdButton->rect().bottomLeft()));

	if (Result.has_value())
	{
		lcModel* Model = gMainWindow->GetActiveModel();
		PieceData = Result.value();

		if (!Model || !PieceData)
			return;

		Model->SetObjectsProperty(mFocusObject ? std::vector<lcObject*>{ mFocusObject } : mSelection, PropertyId, QVariant::fromValue<void*>(PieceData));
	}
}



bool lcPropertiesWidget::HasVisibleRows() const
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

bool lcPropertiesWidget::eventFilter(QObject* Object, QEvent* Event)
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

void lcPropertiesWidget::UpdateIdentityBlock(IdentityBlock& Block)
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
	const int Inset = lcGetLayoutSpacing(this, Qt::Horizontal);
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

void lcPropertiesWidget::UpdateRowVisible(QWidget* Row)
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

void lcPropertiesWidget::AddCategory(CategoryIndex Index, const QString& Title, bool Collapsible)
{
	mCurrentCategory = &mCategoryWidgets[static_cast<int>(Index)];
	mCurrentCategory->Category = Index;

	mCurrentCategory->Title = Title;

	// Only collapsible categories have a header, the title still names their properties for accessibility.
	if (!Collapsible)
		return;

	lcCollapsibleWidgetButton* CategoryButton = new lcCollapsibleWidgetButton(Title, this);
	CategoryButton->setToolTip(tr("Expand or collapse the %1 properties.").arg(Title));

	mCurrentCategory->Button = CategoryButton;

	connect(CategoryButton, &lcCollapsibleWidgetButton::StateChanged, this, &lcPropertiesWidget::CategoryStateChanged);

	mSettingsLayout->addWidget(CategoryButton);
}

void lcPropertiesWidget::RegisterEditor(lcObjectPropertyId PropertyId, QWidget* Editor, const QString& Name, const QString& ToolTip)
{
	Editor->setToolTip(ToolTip);
	Editor->setAccessibleName(Name);

	mCurrentCategory->Properties.push_back(PropertyId);
	mPropertyWidgets[static_cast<int>(PropertyId)].Editor = Editor;
	mPropertyWidgets[static_cast<int>(PropertyId)].Category = mCurrentCategory->Category;
}

lcKeyFrameWidget* lcPropertiesWidget::CreateKeyFrameWidget(lcObjectPropertyId PropertyId)
{
	lcKeyFrameWidget* Widget = new lcKeyFrameWidget(this);
	Widget->setToolTip(tr("Toggle Keyframe"));

	connect(Widget, &QCheckBox::stateChanged, this, &lcPropertiesWidget::KeyFrameChanged);

	mPropertyWidgets[static_cast<int>(PropertyId)].KeyFrame = Widget;

	return Widget;
}

lcDoubleSpinBox* lcPropertiesWidget::CreateFloatEditor(lcObjectPropertyId PropertyId, const QString& Name, const QString& ToolTip, float Min, float Max, bool ShowName, const QString& Suffix)
{
	lcDoubleSpinBox* Widget = new lcDoubleSpinBox(this);
	RegisterEditor(PropertyId, Widget, Name, ToolTip);

	Widget->setRange(Min, Max);

	// Single values carry their name and unit, the letter of a vector row names its boxes.
	if (ShowName)
		Widget->setPrefix(Name + QLatin1Char(' '));

	Widget->setSuffix(Suffix);
	lcSetupPanelSpinBox(Widget);

	connect(Widget, &lcDoubleSpinBox::EditingStarted, this, &lcPropertiesWidget::FloatEditingStarted);
	connect(Widget, &lcDoubleSpinBox::EditingCanceled, this, &lcPropertiesWidget::FloatEditingCanceled);
	connect(Widget, &lcDoubleSpinBox::EditingFinished, this, &lcPropertiesWidget::FloatEditingFinished);

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	connect(Widget, QOverload<double>::of(&lcDoubleSpinBox::valueChanged), this, &lcPropertiesWidget::FloatChanged);
#else
	connect(Widget, &lcDoubleSpinBox::valueChanged, this, &lcPropertiesWidget::FloatChanged);
#endif

	return Widget;
}

QSpinBox* lcPropertiesWidget::CreateIntegerEditor(lcObjectPropertyId PropertyId, const QString& Name, const QString& ToolTip, int Min, int Max)
{
	QSpinBox* Widget = new QSpinBox(this);
	RegisterEditor(PropertyId, Widget, Name, ToolTip);

	Widget->setRange(Min, Max);
	Widget->setPrefix(Name + QLatin1Char(' '));
	lcSetupPanelSpinBox(Widget);

	// Applied on every change like the other value boxes, so the arrows take effect at once.
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	connect(Widget, QOverload<int>::of(&QSpinBox::valueChanged), this, &lcPropertiesWidget::IntegerChanged);
#else
	connect(Widget, &QSpinBox::valueChanged, this, &lcPropertiesWidget::IntegerChanged);
#endif

	return Widget;
}

QLineEdit* lcPropertiesWidget::CreateStepNumberEditor(lcObjectPropertyId PropertyId, const QString& Name, const QString& ToolTip)
{
	QLineEdit* Widget = new QLineEdit(this);
	RegisterEditor(PropertyId, Widget, Name, ToolTip);
	Widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	lcSetupPanelEdit(Widget);

	connect(Widget, &QLineEdit::editingFinished, this, &lcPropertiesWidget::StepNumberChanged);

	// An empty removal step keeps the piece for the rest of the model.
	if (PropertyId == lcObjectPropertyId::PieceStepHide)
		Widget->setPlaceholderText(tr("Never"));

	return Widget;
}

QLineEdit* lcPropertiesWidget::CreateStringEditor(lcObjectPropertyId PropertyId, const QString& Name, const QString& ToolTip)
{
	QLineEdit* Widget = new QLineEdit(this);
	RegisterEditor(PropertyId, Widget, Name, ToolTip);
	Widget->setPlaceholderText(Name);
	Widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	lcSetupPanelEdit(Widget);

	connect(Widget, &QLineEdit::editingFinished, this, &lcPropertiesWidget::StringChanged);

	return Widget;
}

QComboBox* lcPropertiesWidget::CreateStringListEditor(lcObjectPropertyId PropertyId, const QString& Name, const QString& ToolTip, const QStringList& Strings)
{
	QComboBox* Widget = new QComboBox(this);
	RegisterEditor(PropertyId, Widget, Name, ToolTip);
	Widget->addItems(Strings);
	Widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	lcSetupPanelEdit(Widget);

	connect(Widget, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &lcPropertiesWidget::StringListChanged);

	return Widget;
}

QToolButton* lcPropertiesWidget::CreateColorEditor(lcObjectPropertyId PropertyId, const QString& Name, const QString& ToolTip)
{
	QToolButton* Widget = new QToolButton(this);
	RegisterEditor(PropertyId, Widget, Name, ToolTip);
	Widget->setToolButtonStyle(Qt::ToolButtonTextOnly);
	lcSetupPanelButton(Widget);
	Widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

	connect(Widget, &QToolButton::clicked, this, &lcPropertiesWidget::ColorButtonClicked);

	return Widget;
}

QToolButton* lcPropertiesWidget::CreateToggleEditor(lcObjectPropertyId PropertyId, const QString& Name, const QString& ToolTip)
{
	QToolButton* Widget = new QToolButton(this);
	RegisterEditor(PropertyId, Widget, Name, ToolTip);
	Widget->setText(Name);
	Widget->setCheckable(true);
	Widget->setToolButtonStyle(Qt::ToolButtonTextOnly);
	lcSetupPanelButton(Widget);
	Widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

	connect(Widget, &QToolButton::toggled, this, &lcPropertiesWidget::BoolChanged);

	return Widget;
}

QToolButton* lcPropertiesWidget::CreatePieceIdEditor(lcObjectPropertyId PropertyId, const QString& Name, const QString& ToolTip)
{
	lcElidableToolButton* Widget = new lcElidableToolButton(this);
	RegisterEditor(PropertyId, Widget, Name, ToolTip);
	Widget->setToolButtonStyle(Qt::ToolButtonTextOnly);
	lcSetupPanelButton(Widget);
	Widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

	connect(Widget, &QToolButton::clicked, this, &lcPropertiesWidget::PieceIdButtonClicked);

	return Widget;
}

QToolButton* lcPropertiesWidget::CreatePieceColorEditor(lcObjectPropertyId PropertyId, const QString& Name, const QString& ToolTip)
{
	QToolButton* Widget = new QToolButton(this);
	RegisterEditor(PropertyId, Widget, Name, ToolTip);
	Widget->setToolButtonStyle(Qt::ToolButtonTextOnly);
	lcSetupPanelButton(Widget);
	Widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

	connect(Widget, &QToolButton::clicked, this, &lcPropertiesWidget::PieceColorButtonClicked);

	return Widget;
}

QToolButton* lcPropertiesWidget::CreateRowNameButton(QWidget* Row, const QString& Letter, const QString& Name)
{
	// A button with a letter for the row takes the place of the transform button so the boxes line up.
	QToolButton* NameButton = new QToolButton(Row);
	NameButton->setText(Letter);
	NameButton->setToolTip(Name);
	NameButton->setAccessibleName(Name);
	NameButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
	lcAddPanelRowWidget(Row, NameButton);
	lcSetupPanelButton(NameButton);
	NameButton->setFixedSize(lcGetPanelButtonSize());

	return NameButton;
}

QWidget* lcPropertiesWidget::AddValueRow(QWidget* Container, const QString& Letter, const QString& Name, const std::vector<RowEditor>& Editors)
{
	QWidget* Row = lcCreatePanelRow(Container);
	Container->layout()->addWidget(Row);

	CreateRowNameButton(Row, Letter, Name);

	for (const RowEditor& Editor : Editors)
	{
		Editor.Editor->setParent(Row);
		lcAddPanelRowWidget(Row, Editor.Editor);
		mPropertyWidgets[static_cast<int>(GetEditorWidgetPropertyId(Editor.Editor))].Row = Row;

		if (Editor.KeyFrameId != lcObjectPropertyId::Count)
			lcAddPanelRowWidget(Row, CreateKeyFrameWidget(Editor.KeyFrameId));
	}

	return Row;
}

void lcPropertiesWidget::AddVectorRow(QWidget* Container, const std::array<lcObjectPropertyId, 3>& PropertyIds, const QString& Letter, const QString& Name, const QString& ToolTip, bool SupportsKeyFrames, float Min, float Max)
{
	// The X, Y and Z values share a row with one keyframe since they are keyed together.
	const QString AxisNames[3] = { tr("X"), tr("Y"), tr("Z") };
	std::vector<RowEditor> Editors;

	for (int Axis = 0; Axis < 3; Axis++)
	{
		lcDoubleSpinBox* Widget = CreateFloatEditor(PropertyIds[Axis], tr("%1 %2").arg(Name, AxisNames[Axis]), tr("%1 (%2)").arg(ToolTip, AxisNames[Axis]), Min, Max, false);
		Editors.push_back({ Widget, Axis == 2 && SupportsKeyFrames ? PropertyIds[0] : lcObjectPropertyId::Count });
	}

	AddValueRow(Container, Letter, Name, Editors);
}

void lcPropertiesWidget::AddIdentityRow(IdentityIndex Index, QWidget* Editor, lcObjectPropertyId KeyFrameId)
{
	// A full width row without a name, the block on the left spans the slots of all the identity rows of the object.
	IdentityBlock& Block = mIdentityBlocks[static_cast<int>(Index)];
	QWidget* Row = lcCreatePanelRow(mIdentityRows);
	mIdentityRows->layout()->addWidget(Row);

	QWidget* Slot = new QWidget(Row);
	Slot->setFixedSize(lcGetPanelButtonSize());
	lcAddPanelRowWidget(Row, Slot);
	Row->installEventFilter(this);
	Block.Slots.push_back(Slot);

	Editor->setParent(Row);
	lcAddPanelRowWidget(Row, Editor);

	const lcObjectPropertyId PropertyId = GetEditorWidgetPropertyId(Editor);
	mPropertyWidgets[static_cast<int>(PropertyId)].Row = Row;

	if (KeyFrameId != lcObjectPropertyId::Count)
		lcAddPanelRowWidget(Row, CreateKeyFrameWidget(KeyFrameId));

	if (!Block.Swatch)
	{
		// Placed over the slots once the rows are laid out, hidden until then.
		Block.Swatch = new QToolButton(mIdentityRows);
		Block.Swatch->setFocusPolicy(Qt::NoFocus);
		Block.Swatch->setAttribute(Qt::WA_TransparentForMouseEvents);
		Block.Swatch->hide();
	}

	// Objects with a color open the same color choice from the block.
	if (PropertyId == lcObjectPropertyId::PieceColor || PropertyId == lcObjectPropertyId::LightColor)
	{
		Block.Swatch->setAttribute(Qt::WA_TransparentForMouseEvents, false);
		Block.Swatch->setToolTip(Editor->toolTip());
		Block.Swatch->setAccessibleName(tr("%1 Swatch").arg(Editor->accessibleName()));
		connect(Block.Swatch, &QToolButton::clicked, static_cast<QToolButton*>(Editor), &QToolButton::click);
	}
}

void lcPropertiesWidget::CreateWidgets()
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

	AddIdentityRow(IdentityIndex::Piece, CreatePieceIdEditor(lcObjectPropertyId::PieceId, tr("Piece"), tr("Piece ID")));
	AddIdentityRow(IdentityIndex::Piece, CreatePieceColorEditor(lcObjectPropertyId::PieceColor, tr("Color"), tr("Piece color")));

	AddValueRow(mStepRows, tr("S"), tr("Steps"),
	{
		{ CreateStepNumberEditor(lcObjectPropertyId::PieceStepShow, tr("Added in Step"), tr("Added in Step: Step when the piece is added to the model")) },
		{ CreateStepNumberEditor(lcObjectPropertyId::PieceStepHide, tr("Removed in Step"), tr("Removed in Step: Step when the piece is removed from the model, leave empty to keep it until the end")) }
	});

	AddCategory(CategoryIndex::Camera, tr("Camera"), false);

	AddIdentityRow(IdentityIndex::Camera, CreateStringEditor(lcObjectPropertyId::CameraName, tr("Name"), tr("Camera name")));
	AddIdentityRow(IdentityIndex::Camera, CreateStringListEditor(lcObjectPropertyId::CameraProjection, tr("Projection"), tr("Camera projection"), lcCamera::GetCameraProjectionStrings()));

	AddValueRow(this, tr("F"), tr("Field of View and Clipping"),
	{
		{ CreateFloatEditor(lcObjectPropertyId::CameraFOV, tr("FOV"), tr("Field of view in degrees"), 0.1f, 179.9f, true, tr("°")) },
		{ CreateFloatEditor(lcObjectPropertyId::CameraNear, tr("Near"), tr("Near clipping distance"), 0.1f, FLT_MAX, true) },
		{ CreateFloatEditor(lcObjectPropertyId::CameraFar, tr("Far"), tr("Far clipping distance"), 0.1f, FLT_MAX, true) }
	});

	AddCategory(CategoryIndex::CameraTransform, tr("Transform"), false);

	AddVectorRow(mTransformRows, { lcObjectPropertyId::CameraPositionX, lcObjectPropertyId::CameraPositionY, lcObjectPropertyId::CameraPositionZ }, tr("P"), tr("Position"), tr("Camera position"), true, -FLT_MAX, FLT_MAX);
	AddVectorRow(mTransformRows, { lcObjectPropertyId::CameraTargetX, lcObjectPropertyId::CameraTargetY, lcObjectPropertyId::CameraTargetZ }, tr("L"), tr("Look At"), tr("Camera target position"), true, -FLT_MAX, FLT_MAX);
	AddVectorRow(mTransformRows, { lcObjectPropertyId::CameraUpX, lcObjectPropertyId::CameraUpY, lcObjectPropertyId::CameraUpZ }, tr("U"), tr("Up"), tr("Camera up direction"), true, -FLT_MAX, FLT_MAX);

	AddCategory(CategoryIndex::Light, tr("Light"), false);

	AddIdentityRow(IdentityIndex::Light, CreateStringEditor(lcObjectPropertyId::LightName, tr("Name"), tr("Light name")));
	AddIdentityRow(IdentityIndex::Light, CreateStringListEditor(lcObjectPropertyId::LightType, tr("Type"), tr("Light type"), lcLight::GetLightTypeStrings()));
	AddIdentityRow(IdentityIndex::Light, CreateStringListEditor(lcObjectPropertyId::LightAreaShape, tr("Area Shape"), tr("The shape of the area light"), lcLight::GetAreaShapeStrings()));
	AddIdentityRow(IdentityIndex::Light, CreateColorEditor(lcObjectPropertyId::LightColor, tr("Color"), tr("Light color")), lcObjectPropertyId::LightColor);
	AddIdentityRow(IdentityIndex::Light, CreateToggleEditor(lcObjectPropertyId::LightCastShadow, tr("Cast Shadow"), tr("Cast a shadow from this light")));

	AddValueRow(this, tr("A"), tr("Spotlight Angles"),
	{
		{ CreateFloatEditor(lcObjectPropertyId::LightSpotConeAngle, tr("Cone"), tr("Angle in degrees of the spotlight's beam"), 0.0f, 179.9f, true, tr("°")), lcObjectPropertyId::LightSpotConeAngle },
		{ CreateFloatEditor(lcObjectPropertyId::LightSpotPenumbraAngle, tr("Penumbra"), tr("Angle in degrees over which the intensity of the spotlight falls off to zero"), 0.0f, 179.9f, true, tr("°")), lcObjectPropertyId::LightSpotPenumbraAngle }
	});

	AddValueRow(this, tr("S"), tr("Area Size"),
	{
		{ CreateFloatEditor(lcObjectPropertyId::LightAreaSizeX, tr("Size X"), tr("The width of the area light"), 0.0f, FLT_MAX, true), lcObjectPropertyId::LightAreaSizeX },
		{ CreateFloatEditor(lcObjectPropertyId::LightAreaSizeY, tr("Y"), tr("The height of the area light"), 0.0f, FLT_MAX, true), lcObjectPropertyId::LightAreaSizeY }
	});

	AddCategory(CategoryIndex::LightBlender, tr("Blender Settings"), true);

	AddValueRow(this, tr("B"), tr("Blender"),
	{
		{ CreateFloatEditor(lcObjectPropertyId::LightBlenderPower, tr("Power"), tr("Power of the light in watts"), 0.0f, FLT_MAX, true, tr(" W")), lcObjectPropertyId::LightBlenderPower },
		{ CreateFloatEditor(lcObjectPropertyId::LightBlenderRadius, tr("Radius"), tr("Shadow soft size"), 0.0f, FLT_MAX, true), lcObjectPropertyId::LightBlenderRadius },
		{ CreateFloatEditor(lcObjectPropertyId::LightBlenderAngle, tr("Angle"), tr("Angular diameter of the light"), 0.0f, 180.0f, true, tr("°")), lcObjectPropertyId::LightBlenderAngle }
	});

	AddCategory(CategoryIndex::LightPOVRay, tr("POV-Ray Settings"), true);

	AddValueRow(this, tr("V"), tr("POV-Ray"),
	{
		{ CreateFloatEditor(lcObjectPropertyId::LightPOVRayPower, tr("Power"), tr("Power of the light (multiplicative factor)"), 0.0f, FLT_MAX, true), lcObjectPropertyId::LightPOVRayPower },
		{ CreateFloatEditor(lcObjectPropertyId::LightPOVRaySpotTightness, tr("Tightness"), tr("Additional exponential spotlight edge softening"), 0.0f, FLT_MAX, true), lcObjectPropertyId::LightPOVRaySpotTightness }
	});

	AddValueRow(this, tr("F"), tr("Fade"),
	{
		{ CreateFloatEditor(lcObjectPropertyId::LightPOVRayFadeDistance, tr("Fade Distance"), tr("The distance at which the full light intensity arrives"), 0.0f, FLT_MAX, true), lcObjectPropertyId::LightPOVRayFadeDistance },
		{ CreateFloatEditor(lcObjectPropertyId::LightPOVRayFadePower, tr("Fade Power"), tr("Light falloff rate"), 0.0f, FLT_MAX, true), lcObjectPropertyId::LightPOVRayFadePower }
	});

	AddValueRow(this, tr("G"), tr("Area Grid"),
	{
		{ CreateIntegerEditor(lcObjectPropertyId::LightPOVRayAreaGridX, tr("Grid X"), tr("Number of point sources along the X axis (1 to 64)."), 1, LC_MAX_AREA_LIGHT_GRID), lcObjectPropertyId::LightPOVRayAreaGridX },
		{ CreateIntegerEditor(lcObjectPropertyId::LightPOVRayAreaGridY, tr("Y"), tr("Number of point sources along the Y axis (1 to 64)."), 1, LC_MAX_AREA_LIGHT_GRID), lcObjectPropertyId::LightPOVRayAreaGridY }
	});

	AddCategory(CategoryIndex::ObjectTransform, tr("Transform"), false);

	AddVectorRow(mTransformRows, { lcObjectPropertyId::ObjectPositionX, lcObjectPropertyId::ObjectPositionY, lcObjectPropertyId::ObjectPositionZ }, tr("P"), tr("Position"), tr("Position of the object"), true, -FLT_MAX, FLT_MAX);
	AddVectorRow(mTransformRows, { lcObjectPropertyId::ObjectRotationX, lcObjectPropertyId::ObjectRotationY, lcObjectPropertyId::ObjectRotationZ }, tr("R"), tr("Rotation"), tr("Rotation of the object in degrees"), true, -FLT_MAX, FLT_MAX);

	mSettingsLayout->addStretch(1);
}

void lcPropertiesWidget::SetCategoryWidgetsVisible(CategoryWidgets& Category, bool Visible)
{
	for (lcObjectPropertyId PropertyId : Category.Properties)
		SetPropertyWidgetsVisible(PropertyId, Visible && mPropertyWidgets[static_cast<int>(PropertyId)].Visible);
}

void lcPropertiesWidget::SetPropertyWidgetsVisible(lcObjectPropertyId PropertyId, bool Visible)
{
	PropertyWidgets& Property = mPropertyWidgets[static_cast<int>(PropertyId)];

	if (Property.Editor)
		Property.Editor->setVisible(Visible);

	if (Property.KeyFrame)
		Property.KeyFrame->setVisible(Visible);

	UpdateRowVisible(Property.Row);
}

void lcPropertiesWidget::SetLayoutMode(LayoutMode Mode)
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


void lcPropertiesWidget::SetPropertyVisible(lcObjectPropertyId PropertyId, bool Visible)
{
	PropertyWidgets& Property = mPropertyWidgets[static_cast<int>(PropertyId)];
	Property.Visible = Visible;

	const CategoryWidgets& Category = mCategoryWidgets[static_cast<int>(Property.Category)];

	SetPropertyWidgetsVisible(PropertyId, Visible && (!Category.Button || Category.Button->IsExpanded()));
}


void lcPropertiesWidget::SetCategoryVisible(CategoryIndex Index, bool Visible)
{
	CategoryWidgets& Category = mCategoryWidgets[static_cast<int>(Index)];

	if (Category.Button)
		Category.Button->setVisible(Visible);

	SetCategoryWidgetsVisible(Category, Visible && (!Category.Button || Category.Button->IsExpanded()));
}

void lcPropertiesWidget::SetEmpty()
{
	// Keep the piece rows on screen, disabled, so the panel keeps its height.
	SetPiece({}, nullptr);
	setEnabled(false);

	mFocusObject = nullptr;
	mSelection.clear();
}

void lcPropertiesWidget::SetPiece(const std::vector<lcObject*>& Selection, lcObject* Focus)
{
	SetLayoutMode(LayoutMode::Piece);

	lcPiece* Piece = dynamic_cast<lcPiece*>(Focus);
	mSelection = Selection;
	mFocusObject = Piece;

	lcVector3 Position;
	lcMatrix33 RelativeRotation;
	lcModel* Model = gMainWindow->GetActiveModel();

	if (!Model)
		return;

	Model->GetMoveRotateTransform(Position, RelativeRotation);

	UpdateFloat(lcObjectPropertyId::ObjectPositionX, Position[0]);
	UpdateFloat(lcObjectPropertyId::ObjectPositionY, Position[1]);
	UpdateFloat(lcObjectPropertyId::ObjectPositionZ, Position[2]);

	lcVector3 Rotation;

	if (Piece)
		Rotation = lcMatrix44ToEulerAngles(Piece->mModelWorld) * LC_RTOD;
	else
		Rotation = lcVector3(0.0f, 0.0f, 0.0f);

	UpdateFloat(lcObjectPropertyId::ObjectRotationX, Rotation[0]);
	UpdateFloat(lcObjectPropertyId::ObjectRotationY, Rotation[1]);
	UpdateFloat(lcObjectPropertyId::ObjectRotationZ, Rotation[2]);

	lcStep StepShow = 1;
	lcStep StepHide = LC_STEP_MAX;

	if (Piece)
	{
		StepShow = Piece->GetStepShow();
		StepHide = Piece->GetStepHide();
	}
	else
	{
		bool FirstPiece = true;

		for (lcObject* Object : Selection)
		{
			if (!Object->IsPiece())
				continue;

			lcPiece* SelectedPiece = (lcPiece*)Object;

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

	UpdatePieceId(lcObjectPropertyId::PieceId);
	UpdatePieceColor(lcObjectPropertyId::PieceColor);
	UpdateStepNumber(lcObjectPropertyId::PieceStepShow, StepShow ? StepShow : LC_STEP_MAX, 1, StepHide - 1);
	UpdateStepNumber(lcObjectPropertyId::PieceStepHide, StepHide ? StepHide : LC_STEP_MAX, StepShow + 1, LC_STEP_MAX);
}

void lcPropertiesWidget::SetCamera(const std::vector<lcObject*>& Selection, lcObject* Focus)
{
	SetLayoutMode(LayoutMode::Camera);

	lcCamera* Camera = dynamic_cast<lcCamera*>(Focus);
	mSelection = Selection;
	mFocusObject = Camera;

	lcVector3 Position(0.0f, 0.0f, 0.0f);
	lcVector3 Target(0.0f, 0.0f, 0.0f);
	lcVector3 UpVector(0.0f, 0.0f, 0.0f);
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

	UpdateString(lcObjectPropertyId::CameraName);
	UpdateStringList(lcObjectPropertyId::CameraProjection);

	UpdateFloat(lcObjectPropertyId::CameraFOV, FoV);
	UpdateFloat(lcObjectPropertyId::CameraNear, ZNear);
	UpdateFloat(lcObjectPropertyId::CameraFar, ZFar);

	UpdateFloat(lcObjectPropertyId::CameraPositionX, Position[0]);
	UpdateFloat(lcObjectPropertyId::CameraPositionY, Position[1]);
	UpdateFloat(lcObjectPropertyId::CameraPositionZ, Position[2]);

	UpdateFloat(lcObjectPropertyId::CameraTargetX, Target[0]);
	UpdateFloat(lcObjectPropertyId::CameraTargetY, Target[1]);
	UpdateFloat(lcObjectPropertyId::CameraTargetZ, Target[2]);

	UpdateFloat(lcObjectPropertyId::CameraUpX, UpVector[0]);
	UpdateFloat(lcObjectPropertyId::CameraUpY, UpVector[1]);
	UpdateFloat(lcObjectPropertyId::CameraUpZ, UpVector[2]);
}

void lcPropertiesWidget::SetLight(const std::vector<lcObject*>& Selection, lcObject* Focus)
{
	SetLayoutMode(LayoutMode::Light);

	lcLight* Light = dynamic_cast<lcLight*>(Focus);
	mSelection = Selection;
	mFocusObject = Light;

	lcLightType LightType = lcLightType::Count;
	lcLightAreaShape LightAreaShape = lcLightAreaShape::Count;
	float BlenderRadius = 0.0f, BlenderAngle = 0.0f;
	float AreaSizeX = 0.0f, AreaSizeY = 0.0f;
	float BlenderPower = 0.0f, POVRayPower = 0.0f;
	float POVRayFadeDistance = 0.0f;
	float POVRayFadePower = 0.0f;
	lcVector3 Position(0.0f, 0.0f, 0.0f);
	lcVector3 Rotation = lcVector3(0.0f, 0.0f, 0.0f);
	float SpotConeAngle = 0.0f, SpotPenumbraAngle = 0.0f, SpotTightness = 0.0f;

	if (Light)
	{
		LightType = Light->GetLightType();
		LightAreaShape = Light->GetAreaShape();

		Position = Light->GetPosition();
		Rotation = lcMatrix44ToEulerAngles(Light->GetWorldMatrix()) * LC_RTOD;
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

		for (const lcObject* Object : mSelection)
		{
			const lcLight* CurrentLight = dynamic_cast<const lcLight*>(Object);

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
			LightType = lcLightType::Count;

		if (PartialAreaShape)
			LightAreaShape = lcLightAreaShape::Count;
	}

	UpdateString(lcObjectPropertyId::LightName);
	UpdateStringList(lcObjectPropertyId::LightType);
	UpdateColor(lcObjectPropertyId::LightColor);

	UpdateFloat(lcObjectPropertyId::LightBlenderPower, BlenderPower);
	UpdateFloat(lcObjectPropertyId::LightPOVRayPower, POVRayPower);
	UpdateBool(lcObjectPropertyId::LightCastShadow);

	UpdateFloat(lcObjectPropertyId::LightPOVRayFadeDistance, POVRayFadeDistance);
	UpdateFloat(lcObjectPropertyId::LightPOVRayFadePower, POVRayFadePower);

	const bool IsPointLight = (LightType == lcLightType::Point);
	const bool IsSpotLight = (LightType == lcLightType::Spot);

	SetPropertyVisible(lcObjectPropertyId::LightBlenderRadius, IsPointLight || IsSpotLight);

	if (IsPointLight || IsSpotLight)
		UpdateFloat(lcObjectPropertyId::LightBlenderRadius, BlenderRadius);

	SetPropertyVisible(lcObjectPropertyId::LightSpotConeAngle, IsSpotLight);
	SetPropertyVisible(lcObjectPropertyId::LightSpotPenumbraAngle, IsSpotLight);
	SetPropertyVisible(lcObjectPropertyId::LightPOVRaySpotTightness, IsSpotLight);

	if (IsSpotLight)
	{
		UpdateFloat(lcObjectPropertyId::LightSpotConeAngle, SpotConeAngle);
		UpdateFloat(lcObjectPropertyId::LightSpotPenumbraAngle, SpotPenumbraAngle);
		UpdateFloat(lcObjectPropertyId::LightPOVRaySpotTightness, SpotTightness);
	}

	const bool IsDirectionalLight = (LightType == lcLightType::Directional);
	SetPropertyVisible(lcObjectPropertyId::LightBlenderAngle, IsDirectionalLight);

	if (IsDirectionalLight)
		UpdateFloat(lcObjectPropertyId::LightBlenderAngle, BlenderAngle);

	const bool IsAreaLight = (LightType == lcLightType::Area);
	SetPropertyVisible(lcObjectPropertyId::LightAreaShape, IsAreaLight);

	const bool IsSquare = (LightAreaShape == lcLightAreaShape::Square || LightAreaShape == lcLightAreaShape::Disk);
	SetPropertyVisible(lcObjectPropertyId::LightAreaSizeX, IsAreaLight);
	SetPropertyVisible(lcObjectPropertyId::LightAreaSizeY, IsAreaLight && !IsSquare);

	// Square and disk shapes have a single size.
	if (lcDoubleSpinBox* SizeXEditor = qobject_cast<lcDoubleSpinBox*>(mPropertyWidgets[static_cast<int>(lcObjectPropertyId::LightAreaSizeX)].Editor))
		SizeXEditor->setPrefix(IsSquare ? tr("Size ") : tr("Size X "));

	SetPropertyVisible(lcObjectPropertyId::LightPOVRayAreaGridX, IsAreaLight);
	SetPropertyVisible(lcObjectPropertyId::LightPOVRayAreaGridY, IsAreaLight);

	if (IsAreaLight)
	{
		UpdateStringList(lcObjectPropertyId::LightAreaShape);
		UpdateFloat(lcObjectPropertyId::LightAreaSizeX, AreaSizeX);
		UpdateFloat(lcObjectPropertyId::LightAreaSizeY, AreaSizeY);
		UpdateInteger(lcObjectPropertyId::LightPOVRayAreaGridX);
		UpdateInteger(lcObjectPropertyId::LightPOVRayAreaGridY);
	}

	UpdateFloat(lcObjectPropertyId::ObjectPositionX, Position[0]);
	UpdateFloat(lcObjectPropertyId::ObjectPositionY, Position[1]);
	UpdateFloat(lcObjectPropertyId::ObjectPositionZ, Position[2]);

	UpdateFloat(lcObjectPropertyId::ObjectRotationX, Rotation[0]);
	UpdateFloat(lcObjectPropertyId::ObjectRotationY, Rotation[1]);
	UpdateFloat(lcObjectPropertyId::ObjectRotationZ, Rotation[2]);
}

void lcPropertiesWidget::Update(const std::vector<lcObject*>& Selection, lcObject* Focus)
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
		case lcObjectType::Piece:
			Mode = LayoutMode::Piece;
			break;

		case lcObjectType::Camera:
			Mode = LayoutMode::Camera;
			break;

		case lcObjectType::Light:
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
			case lcObjectType::Piece:
				if (Mode == LayoutMode::Empty)
					Mode = LayoutMode::Piece;
				else if (Mode != LayoutMode::Piece)
				{
					Mode = LayoutMode::Multiple;
					ObjectIdx = Selection.size();
				}
				break;

			case lcObjectType::Camera:
				if (Mode == LayoutMode::Empty)
					Mode = LayoutMode::Camera;
				else if (Mode != LayoutMode::Camera)
				{
					Mode = LayoutMode::Multiple;
					ObjectIdx = Selection.size();
				}
				break;

			case lcObjectType::Light:
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
