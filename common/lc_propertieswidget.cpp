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
		if (!Property.Editor || !Property.Label)
			continue;

		const QString Name = tr("%1: %2").arg(mCategoryWidgets[static_cast<int>(Property.Category)].Title, Property.Label->text());
		Property.Label->setBuddy(Property.Editor);

		if (Property.Editor->accessibleName().isEmpty())
			Property.Editor->setAccessibleName(Name);

		Property.Editor->setWhatsThis(Property.Editor->toolTip());
		Property.Label->setWhatsThis(Property.Editor->whatsThis());

		if (Property.KeyFrame)
		{
			Property.KeyFrame->setAccessibleName(tr("Keyframe for %1").arg(Name));
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

void lcPropertiesWidget::UpdatePieceColorSwatch()
{
	if (!mPieceColorSwatch || mPieceRowSlots.size() < 2)
		return;

	// The swatch spans the slots at the start of the piece and color rows.
	QWidget* FirstSlot = mPieceRowSlots.front();
	QWidget* LastSlot = mPieceRowSlots.back();
	const QRect SwatchRect(FirstSlot->mapTo(mTransformRows, QPoint(0, 0)), LastSlot->mapTo(mTransformRows, LastSlot->rect().bottomRight()));
	// The color fills the button inside its border.
	const QSize IconSize = (SwatchRect.size() - QSize(8, 8)).expandedTo(QSize(1, 1));

	mPieceColorSwatch->setGeometry(SwatchRect);
	mPieceColorSwatch->setIconSize(IconSize);

	if (mPieceColorSwatchColor.isValid())
	{
		QPixmap Pixmap(IconSize);
		Pixmap.fill(mPieceColorSwatchColor);
		mPieceColorSwatch->setIcon(Pixmap);
	}
	else
		mPieceColorSwatch->setIcon(QIcon());

	mPieceColorSwatch->setVisible(LastSlot->isVisible());
	mPieceColorSwatch->raise();
}

bool lcPropertiesWidget::HasVisibleRows() const
{
	// Rows moved below the panel or the timeline do not count, only the rows of the list itself.
	for (const PropertyWidgets& Property : mPropertyWidgets)
		if (Property.Editor && Property.Editor->parentWidget() == this && !Property.Editor->isHidden())
			return true;

	for (const CategoryWidgets& Category : mCategoryWidgets)
		if (Category.Button && !Category.Button->isHidden())
			return true;

	return false;
}

bool lcPropertiesWidget::eventFilter(QObject* Object, QEvent* Event)
{
	// The transform rows sit below the panel, so their changes also resize it.
	if (Object == mTransformRows && Event->type() == QEvent::LayoutRequest)
		emit LayoutChanged();

	if (std::find(mPieceRowSlots.begin(), mPieceRowSlots.end(), Object) != mPieceRowSlots.end())
	{
		switch (Event->type())
		{
		case QEvent::Move:
		case QEvent::Resize:
		case QEvent::Show:
		case QEvent::Hide:
			UpdatePieceColorSwatch();
			break;

		default:
			break;
		}
	}

	return QWidget::eventFilter(Object, Event);
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

void lcPropertiesWidget::AddCategory(CategoryIndex Index, const QString& Title, bool Collapsible)
{
	mCurrentCategory = &mCategoryWidgets[static_cast<int>(Index)];
	mCurrentCategory->Category = Index;

	mCurrentCategory->Title = Title;

	// Only collapsible categories have a header, the title still names their properties for accessibility.
	if (!Collapsible)
		return;

	lcCollapsibleWidgetButton* CategoryButton = new lcCollapsibleWidgetButton(Title);
	CategoryButton->setToolTip(tr("Expand or collapse the %1 properties.").arg(Title));

	mCurrentCategory->Button = CategoryButton;

	connect(CategoryButton, &lcCollapsibleWidgetButton::StateChanged, this, &lcPropertiesWidget::CategoryStateChanged);

	mLayout->addWidget(CategoryButton, mLayoutRow, 0, 1, -1);
	mLayoutRow++;
}

void lcPropertiesWidget::AddSpacing()
{
	mLayout->setRowMinimumHeight(mLayoutRow, 5);
	mCurrentCategory->SpacingRows.push_back(mLayoutRow);
	mLayoutRow++;
}

void lcPropertiesWidget::AddLabel(lcObjectPropertyId PropertyId, const QString& Text, const QString& ToolTip)
{
	QLabel* Label = new QLabel(Text, this);
	Label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
	Label->setToolTip(ToolTip);

	mLayout->addWidget(Label, mLayoutRow, 1);

	mPropertyWidgets[static_cast<int>(PropertyId)].Label = Label;
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

lcKeyFrameWidget* lcPropertiesWidget::CreateKeyFrameWidget(lcObjectPropertyId PropertyId)
{
	lcKeyFrameWidget* Widget = new lcKeyFrameWidget(this);
	Widget->setToolTip(tr("Toggle Keyframe"));

	connect(Widget, &QCheckBox::stateChanged, this, &lcPropertiesWidget::KeyFrameChanged);

	mPropertyWidgets[static_cast<int>(PropertyId)].KeyFrame = Widget;

	return Widget;
}

void lcPropertiesWidget::AddKeyFrameWidget(lcObjectPropertyId PropertyId)
{
	mLayout->addWidget(CreateKeyFrameWidget(PropertyId), mLayoutRow, 3);
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
	QCheckBox* Widget = qobject_cast<QCheckBox*>(sender());
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
	QCheckBox* CheckBox = qobject_cast<QCheckBox*>(mPropertyWidgets[static_cast<int>(PropertyId)].Editor);

	if (!CheckBox)
		return;

	QSignalBlocker Blocker(CheckBox);
	QVariant Value;
	bool Partial;

	std::tie(Value, Partial) = GetUpdateValue(PropertyId);

	if (Partial)
		CheckBox->setCheckState(Qt::PartiallyChecked);
	else
		CheckBox->setCheckState(Value.toBool() ? Qt::Checked : Qt::Unchecked);

	UpdateKeyFrameWidget(PropertyId);
}

void lcPropertiesWidget::AddBoolProperty(lcObjectPropertyId PropertyId, const QString& Text, const QString& ToolTip, bool SupportsKeyFrames)
{
	AddLabel(PropertyId, Text, ToolTip);

	QCheckBox* Widget = new QCheckBox(this);
	Widget->setToolTip(ToolTip);

	connect(Widget, &QCheckBox::stateChanged, this, &lcPropertiesWidget::BoolChanged);

	mLayout->addWidget(Widget, mLayoutRow, 2);

	mCurrentCategory->Properties.push_back(PropertyId);
	mPropertyWidgets[static_cast<int>(PropertyId)].Category = mCurrentCategory->Category;
	mPropertyWidgets[static_cast<int>(PropertyId)].Editor = Widget;

	if (SupportsKeyFrames)
		AddKeyFrameWidget(PropertyId);

	mLayoutRow++;
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

lcDoubleSpinBox* lcPropertiesWidget::CreateFloatEditor(lcObjectPropertyId PropertyId, const QString& ToolTip, float Min, float Max)
{
	lcDoubleSpinBox* Widget = new lcDoubleSpinBox(this);
	Widget->setToolTip(ToolTip);

	Widget->setRange(Min, Max);

	connect(Widget, &lcDoubleSpinBox::EditingStarted, this, &lcPropertiesWidget::FloatEditingStarted);
	connect(Widget, &lcDoubleSpinBox::EditingCanceled, this, &lcPropertiesWidget::FloatEditingCanceled);
	connect(Widget, &lcDoubleSpinBox::EditingFinished, this, &lcPropertiesWidget::FloatEditingFinished);

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	connect(Widget, QOverload<double>::of(&lcDoubleSpinBox::valueChanged), this, &lcPropertiesWidget::FloatChanged);
#else
	connect(Widget, &lcDoubleSpinBox::valueChanged, this, &lcPropertiesWidget::FloatChanged);
#endif

	mCurrentCategory->Properties.push_back(PropertyId);
	mPropertyWidgets[static_cast<int>(PropertyId)].Editor = Widget;
	mPropertyWidgets[static_cast<int>(PropertyId)].Category = mCurrentCategory->Category;

	return Widget;
}

void lcPropertiesWidget::AddFloatProperty(lcObjectPropertyId PropertyId, const QString& Text, const QString& ToolTip, bool SupportsKeyFrames, float Min, float Max)
{
	AddLabel(PropertyId, Text, ToolTip);

	lcDoubleSpinBox* Widget = CreateFloatEditor(PropertyId, ToolTip, Min, Max);
	mLayout->addWidget(Widget, mLayoutRow, 2);

	if (SupportsKeyFrames)
		AddKeyFrameWidget(PropertyId);

	mLayoutRow++;
}

void lcPropertiesWidget::AddVectorProperty(const std::array<lcObjectPropertyId, 3>& PropertyIds, const QString& Text, const QString& ToolTip, bool SupportsKeyFrames, float Min, float Max)
{
	// The X, Y and Z values share a row below the panel like the transform boxes, with one keyframe since they are keyed together.
	QToolBar* ToolBar = lcCreatePanelToolBar(mTransformRows);
	mTransformRowsLayout->addWidget(ToolBar);

	CreateRowNameButton(ToolBar, Text);

	const QString AxisNames[3] = { tr("X"), tr("Y"), tr("Z") };

	for (int Axis = 0; Axis < 3; Axis++)
	{
		lcDoubleSpinBox* Widget = CreateFloatEditor(PropertyIds[Axis], tr("%1 (%2)").arg(ToolTip, AxisNames[Axis]), Min, Max);
		Widget->setAccessibleName(tr("%1 %2").arg(Text, AxisNames[Axis]));

		// Share the row equally instead of reserving room for the largest possible value.
		Widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		Widget->setMinimumWidth(Widget->fontMetrics().horizontalAdvance(QLatin1String("-000")) + 24);
		lcSetupPanelEdit(Widget);
		ToolBar->addWidget(Widget);
	}

	if (SupportsKeyFrames)
		ToolBar->addWidget(CreateKeyFrameWidget(PropertyIds[0]));
}

void lcPropertiesWidget::IntegerChanged()
{
	// todo: switch to spinner and support mouse drag
	QLineEdit* LineEdit = qobject_cast<QLineEdit*>(sender());
	lcObjectPropertyId PropertyId = GetEditorWidgetPropertyId(LineEdit);

	if (PropertyId == lcObjectPropertyId::Count)
		return;

	lcModel* Model = gMainWindow->GetActiveModel();

	if (!Model)
		return;

	bool Ok;
	const int Value = LineEdit->validator()->locale().toInt(LineEdit->text(), &Ok);

	if (!Ok || !LineEdit->hasAcceptableInput())
		return;

	Model->SetObjectsProperty(mFocusObject ? std::vector<lcObject*>{ mFocusObject } : mSelection, PropertyId, Value);
}

void lcPropertiesWidget::UpdateInteger(lcObjectPropertyId PropertyId)
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
		LineEdit->setText(lcFormatIntegerLocalized(Value.toInt(), LineEdit->validator()->locale()));
		LineEdit->setPlaceholderText(QString());
	}

	UpdateKeyFrameWidget(PropertyId);
}

void lcPropertiesWidget::AddIntegerProperty(lcObjectPropertyId PropertyId, const QString& Text, const QString& ToolTip, bool SupportsKeyFrames, int Min, int Max)
{
	AddLabel(PropertyId, Text, ToolTip);

	QLineEdit* Widget = new QLineEdit(this);
	Widget->setToolTip(ToolTip);

	Widget->setValidator(new QIntValidator(Min, Max, Widget));

	connect(Widget, &QLineEdit::editingFinished, this, &lcPropertiesWidget::IntegerChanged);

	mLayout->addWidget(Widget, mLayoutRow, 2);

	mCurrentCategory->Properties.push_back(PropertyId);
	mPropertyWidgets[static_cast<int>(PropertyId)].Editor = Widget;
	mPropertyWidgets[static_cast<int>(PropertyId)].Category = mCurrentCategory->Category;

	if (SupportsKeyFrames)
		AddKeyFrameWidget(PropertyId);

	mLayoutRow++;
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

QLineEdit* lcPropertiesWidget::CreateStepNumberEditor(lcObjectPropertyId PropertyId, const QString& ToolTip)
{
	QLineEdit* Widget = new QLineEdit(this);
	Widget->setToolTip(ToolTip);

	connect(Widget, &QLineEdit::editingFinished, this, &lcPropertiesWidget::StepNumberChanged);

	// An empty removal step keeps the piece for the rest of the model.
	if (PropertyId == lcObjectPropertyId::PieceStepHide)
		Widget->setPlaceholderText(tr("Never"));

	mCurrentCategory->Properties.push_back(PropertyId);
	mPropertyWidgets[static_cast<int>(PropertyId)].Editor = Widget;
	mPropertyWidgets[static_cast<int>(PropertyId)].Category = mCurrentCategory->Category;

	return Widget;
}

void lcPropertiesWidget::AddStepNumberProperty(lcObjectPropertyId PropertyId, const QString& Text, const QString& ToolTip, bool SupportsKeyFrames)
{
	AddLabel(PropertyId, Text, ToolTip);

	mLayout->addWidget(CreateStepNumberEditor(PropertyId, ToolTip), mLayoutRow, 2);

	if (SupportsKeyFrames)
		AddKeyFrameWidget(PropertyId);

	mLayoutRow++;
}

void lcPropertiesWidget::AddStepRangeProperty(lcObjectPropertyId ShowPropertyId, lcObjectPropertyId HidePropertyId)
{
	// The steps a piece is added and removed in share a row below the timeline, formatted like the transform rows.
	QToolBar* ToolBar = lcCreatePanelToolBar(mStepRows);
	mStepRows->layout()->addWidget(ToolBar);

	CreateRowNameButton(ToolBar, tr("Steps"));

	const std::pair<lcObjectPropertyId, QString> Editors[2] =
	{
		{ ShowPropertyId, tr("Added in Step") },
		{ HidePropertyId, tr("Removed in Step") }
	};

	for (const auto& [PropertyId, Name] : Editors)
	{
		const QString ToolTip = PropertyId == HidePropertyId ? tr("Step when the piece is removed from the model, leave empty to keep it until the end") : tr("Step when the piece is added to the model");
		QLineEdit* Widget = CreateStepNumberEditor(PropertyId, tr("%1: %2").arg(Name, ToolTip));
		Widget->setAccessibleName(Name);
		Widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		lcSetupPanelEdit(Widget);
		ToolBar->addWidget(Widget);
	}
}

QToolBar* lcPropertiesWidget::AddTransformRowButton(QToolButton* Button)
{
	// A button filling a whole row below the panel, without a label like the transform rows.
	QToolBar* ToolBar = lcCreatePanelToolBar(mTransformRows);
	mTransformRowsLayout->addWidget(ToolBar);

	// An empty slot keeps the button in line with the transform boxes, the color swatch covers the slots.
	QWidget* Slot = new QWidget(ToolBar);
	Slot->setFixedSize(lcGetPanelButtonSize());
	Slot->installEventFilter(this);
	ToolBar->addWidget(Slot);
	mPieceRowSlots.push_back(Slot);

	ToolBar->addWidget(Button);
	lcSetupPanelButton(Button);
	Button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

	return ToolBar;
}

QToolButton* lcPropertiesWidget::CreateRowNameButton(QToolBar* ToolBar, const QString& Name)
{
	// A button with the first letter of the name takes the place of the transform button so the boxes line up.
	QToolButton* NameButton = new QToolButton(ToolBar);
	NameButton->setText(Name.left(1));
	NameButton->setToolTip(Name);
	NameButton->setAccessibleName(Name);
	NameButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
	ToolBar->addWidget(NameButton);
	lcSetupPanelButton(NameButton);
	NameButton->setFixedSize(lcGetPanelButtonSize());

	return NameButton;
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
		LineEdit->setPlaceholderText(QString());
	}

	UpdateKeyFrameWidget(PropertyId);
}

void lcPropertiesWidget::AddStringProperty(lcObjectPropertyId PropertyId, const QString& Text, const QString& ToolTip, bool SupportsKeyFrames)
{
	AddLabel(PropertyId, Text, ToolTip);

	QLineEdit* Widget = new QLineEdit(this);
	Widget->setToolTip(ToolTip);

	connect(Widget, &QLineEdit::editingFinished, this, &lcPropertiesWidget::StringChanged);

	mLayout->addWidget(Widget, mLayoutRow, 2);

	mCurrentCategory->Properties.push_back(PropertyId);
	mPropertyWidgets[static_cast<int>(PropertyId)].Editor = Widget;
	mPropertyWidgets[static_cast<int>(PropertyId)].Category = mCurrentCategory->Category;

	if (SupportsKeyFrames)
		AddKeyFrameWidget(PropertyId);

	mLayoutRow++;
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

void lcPropertiesWidget::AddStringListProperty(lcObjectPropertyId PropertyId, const QString& Text, const QString& ToolTip, bool SupportsKeyFrames, const QStringList& Strings)
{
	AddLabel(PropertyId, Text, ToolTip);

	QComboBox* Widget = new QComboBox(this);
	Widget->setToolTip(ToolTip);
	Widget->addItems(Strings);

	connect(Widget, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &lcPropertiesWidget::StringListChanged);

	mLayout->addWidget(Widget, mLayoutRow, 2);

	mCurrentCategory->Properties.push_back(PropertyId);
	mPropertyWidgets[static_cast<int>(PropertyId)].Editor = Widget;
	mPropertyWidgets[static_cast<int>(PropertyId)].Category = mCurrentCategory->Category;

	if (SupportsKeyFrames)
		AddKeyFrameWidget(PropertyId);

	mLayoutRow++;
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
	QPixmap Pixmap(14, 14);
	Pixmap.fill(Color);

	ColorButton->setIcon(Pixmap);
	ColorButton->setText(Partial ? tr(" Multiple Colors") : QString("  ") + Color.name());

	UpdateKeyFrameWidget(PropertyId);
}

void lcPropertiesWidget::AddColorProperty(lcObjectPropertyId PropertyId, const QString& Text, const QString& ToolTip, bool SupportsKeyFrames)
{
	AddLabel(PropertyId, Text, ToolTip);

	QToolButton* Widget = new QToolButton(this);
	Widget->setToolTip(ToolTip);
	Widget->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	Widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

	connect(Widget, &QToolButton::clicked, this, &lcPropertiesWidget::ColorButtonClicked);

	mLayout->addWidget(Widget, mLayoutRow, 2);

	mCurrentCategory->Properties.push_back(PropertyId);
	mPropertyWidgets[static_cast<int>(PropertyId)].Editor = Widget;
	mPropertyWidgets[static_cast<int>(PropertyId)].Category = mCurrentCategory->Category;

	if (SupportsKeyFrames)
		AddKeyFrameWidget(PropertyId);

	mLayoutRow++;
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
		mPieceColorSwatchColor = QColor();
		UpdatePieceColorSwatch();

		ColorButton->setText(tr("Color"));
		UpdateKeyFrameWidget(PropertyId);
		return;
	}

	const int ColorIndex = Value.toInt();
	QColor Color = Partial ? QColor(128, 128, 128) : QColor::fromRgbF(gColorList[ColorIndex].Value[0], gColorList[ColorIndex].Value[1], gColorList[ColorIndex].Value[2]);
	QPixmap Pixmap(14, 14);
	Pixmap.fill(Color);

	// The color shows in the swatch button so the name stays centered.
	mPieceColorSwatchColor = Color;
	UpdatePieceColorSwatch();

	ColorButton->setText(Partial ? tr("Multiple Colors") : gColorList[ColorIndex].Name);

	UpdateKeyFrameWidget(PropertyId);
}

void lcPropertiesWidget::AddPieceColorProperty(lcObjectPropertyId PropertyId, const QString& Text, const QString& ToolTip, bool SupportsKeyFrames)
{
	QToolButton* Widget = new QToolButton(this);
	Widget->setToolTip(ToolTip);
	Widget->setAccessibleName(Text);
	Widget->setToolButtonStyle(Qt::ToolButtonTextOnly);

	connect(Widget, &QToolButton::clicked, this, &lcPropertiesWidget::PieceColorButtonClicked);

	AddTransformRowButton(Widget);

	// A button left of the piece and color rows shows the color and opens the same color choice.
	mPieceColorSwatch = new QToolButton(mTransformRows);
	mPieceColorSwatch->setToolTip(ToolTip);
	mPieceColorSwatch->setAccessibleName(tr("%1 Swatch").arg(Text));
	mPieceColorSwatch->setFocusPolicy(Qt::NoFocus);

	connect(mPieceColorSwatch, &QToolButton::clicked, Widget, &QToolButton::click);

	mCurrentCategory->Properties.push_back(PropertyId);
	mPropertyWidgets[static_cast<int>(PropertyId)].Editor = Widget;
	mPropertyWidgets[static_cast<int>(PropertyId)].Category = mCurrentCategory->Category;

	if (SupportsKeyFrames)
		AddKeyFrameWidget(PropertyId);

	mLayoutRow++;
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

void lcPropertiesWidget::AddPieceIdProperty(lcObjectPropertyId PropertyId, const QString& Text, const QString& ToolTip, bool SupportsKeyFrames)
{
	lcElidableToolButton* Widget = new lcElidableToolButton(this);
	Widget->setToolTip(ToolTip);
	Widget->setAccessibleName(Text);
	Widget->setToolButtonStyle(Qt::ToolButtonTextOnly);

	connect(Widget, &QToolButton::clicked, this, &lcPropertiesWidget::PieceIdButtonClicked);

	AddTransformRowButton(Widget);

	mCurrentCategory->Properties.push_back(PropertyId);
	mPropertyWidgets[static_cast<int>(PropertyId)].Editor = Widget;
	mPropertyWidgets[static_cast<int>(PropertyId)].Category = mCurrentCategory->Category;

	if (SupportsKeyFrames)
		AddKeyFrameWidget(PropertyId);

	mLayoutRow++;
}

void lcPropertiesWidget::CreateWidgets()
{
	mLayout = new QGridLayout(this);
	mLayout->setVerticalSpacing(1);

	mTransformRows = new QWidget(this);
	mTransformRows->installEventFilter(this);
	mTransformRowsLayout = new QVBoxLayout(mTransformRows);
	mTransformRowsLayout->setContentsMargins(0, 0, 0, 0);
	mTransformRowsLayout->setSpacing(0);

	mStepRows = new QWidget(this);
	QVBoxLayout* StepRowsLayout = new QVBoxLayout(mStepRows);
	StepRowsLayout->setContentsMargins(0, 0, 0, 0);
	StepRowsLayout->setSpacing(0);
	mLayout->setColumnStretch(2, 1);

	AddCategory(CategoryIndex::Piece, tr("Piece"), false);

	AddPieceIdProperty(lcObjectPropertyId::PieceId, tr("Piece"), tr("Piece ID"), false);
	AddPieceColorProperty(lcObjectPropertyId::PieceColor, tr("Color"), tr("Piece color"), false);

	AddSpacing();

	AddStepRangeProperty(lcObjectPropertyId::PieceStepShow, lcObjectPropertyId::PieceStepHide);

	AddSpacing();

	AddCategory(CategoryIndex::Camera, tr("Camera"), false);

	AddStringProperty(lcObjectPropertyId::CameraName, tr("Name"), tr("Camera name"), false);
	AddStringListProperty(lcObjectPropertyId::CameraProjection, tr("Projection"), tr("Camera projection"), false, lcCamera::GetCameraProjectionStrings());

	AddSpacing();

	AddFloatProperty(lcObjectPropertyId::CameraFOV, tr("FOV"), tr("Field of view in degrees"), false, 0.1f, 179.9f);
	AddFloatProperty(lcObjectPropertyId::CameraNear, tr("Near"), tr("Near clipping distance"), false, 0.1f, FLT_MAX);
	AddFloatProperty(lcObjectPropertyId::CameraFar, tr("Far"), tr("Far clipping distance"), false, 0.1f, FLT_MAX);

	AddSpacing();

	AddCategory(CategoryIndex::CameraTransform, tr("Transform"), false);

	AddFloatProperty(lcObjectPropertyId::CameraPositionX, tr("Position X"), tr("Camera position"), true, -FLT_MAX, FLT_MAX);
	AddFloatProperty(lcObjectPropertyId::CameraPositionY, tr("Y"), tr("Camera position"), true, -FLT_MAX, FLT_MAX);
	AddFloatProperty(lcObjectPropertyId::CameraPositionZ, tr("Z"), tr("Camera position"), true, -FLT_MAX, FLT_MAX);

	AddSpacing();

	AddFloatProperty(lcObjectPropertyId::CameraTargetX, tr("Target X"), tr("Camera target position"), true, -FLT_MAX, FLT_MAX);
	AddFloatProperty(lcObjectPropertyId::CameraTargetY, tr("Y"), tr("Camera target position"), true, -FLT_MAX, FLT_MAX);
	AddFloatProperty(lcObjectPropertyId::CameraTargetZ, tr("Z"), tr("Camera target position"), true, -FLT_MAX, FLT_MAX);

	AddSpacing();

	AddFloatProperty(lcObjectPropertyId::CameraUpX, tr("Up X"), tr("Camera up direction"), true, -FLT_MAX, FLT_MAX);
	AddFloatProperty(lcObjectPropertyId::CameraUpY, tr("Y"), tr("Camera up direction"), true, -FLT_MAX, FLT_MAX);
	AddFloatProperty(lcObjectPropertyId::CameraUpZ, tr("Z"), tr("Camera up direction"), true, -FLT_MAX, FLT_MAX);

	AddSpacing();

	AddCategory(CategoryIndex::Light, tr("Light"), false);

	AddStringProperty(lcObjectPropertyId::LightName, tr("Name"), tr("Light name"), false);
	AddStringListProperty(lcObjectPropertyId::LightType, tr("Type"), tr("Light type"), false, lcLight::GetLightTypeStrings());

	AddSpacing();

	AddColorProperty(lcObjectPropertyId::LightColor, tr("Color"), tr("Light color"), true);
	AddBoolProperty(lcObjectPropertyId::LightCastShadow, tr("Cast Shadow"), tr("Cast a shadow from this light"), false);

	AddSpacing();

	AddFloatProperty(lcObjectPropertyId::LightSpotConeAngle, tr("Cone Angle"), tr("Angle in degrees of the spotlight's beam"), true, 0.0f, 179.9f);
	AddFloatProperty(lcObjectPropertyId::LightSpotPenumbraAngle, tr("Penumbra Angle"), tr("Angle in degrees over which the intensity of the spotlight falls off to zero"), true, 0.0f, 179.9f);
	AddStringListProperty(lcObjectPropertyId::LightAreaShape, tr("Area Shape"), tr("The shape of the area light"), false, lcLight::GetAreaShapeStrings());
	AddFloatProperty(lcObjectPropertyId::LightAreaSizeX, tr("Size X"), tr("The width of the area light"), true, 0.0f, FLT_MAX);
	AddFloatProperty(lcObjectPropertyId::LightAreaSizeY, tr("Y"), tr("The height of the area light"), true, 0.0f, FLT_MAX);

	AddSpacing();

	AddCategory(CategoryIndex::LightBlender, tr("Blender Settings"), true);

	AddFloatProperty(lcObjectPropertyId::LightBlenderPower, tr("Power"), tr("Power of the light in watts"), true, 0.0f, FLT_MAX);
	AddFloatProperty(lcObjectPropertyId::LightBlenderRadius, tr("Radius"), tr("Shadow soft size"), true, 0.0f, FLT_MAX);
	AddFloatProperty(lcObjectPropertyId::LightBlenderAngle, tr("Angle"), tr("Angular diameter of the light"), true, 0.0f, 180.0f);

	AddSpacing();

	AddCategory(CategoryIndex::LightPOVRay, tr("POV-Ray Settings"), true);

	AddFloatProperty(lcObjectPropertyId::LightPOVRayPower, tr("Power"), tr("Power of the light (multiplicative factor)"), true, 0.0f, FLT_MAX);
	AddFloatProperty(lcObjectPropertyId::LightPOVRayFadeDistance, tr("Fade Distance"), tr("The distance at which the full light intensity arrives"), true, 0.0f, FLT_MAX);
	AddFloatProperty(lcObjectPropertyId::LightPOVRayFadePower, tr("Fade Power"), tr("Light falloff rate"), true, 0.0f, FLT_MAX);
	AddFloatProperty(lcObjectPropertyId::LightPOVRaySpotTightness, tr("Tightness"), tr("Additional exponential spotlight edge softening"), true, 0.0f, FLT_MAX);
	AddIntegerProperty(lcObjectPropertyId::LightPOVRayAreaGridX, tr("Grid X"), tr("Number of point sources along the X axis (1 to 64)."), true, 1, LC_MAX_AREA_LIGHT_GRID);
	AddIntegerProperty(lcObjectPropertyId::LightPOVRayAreaGridY, tr("Y"), tr("Number of point sources along the Y axis (1 to 64)."), true, 1, LC_MAX_AREA_LIGHT_GRID);

	AddSpacing();

	AddCategory(CategoryIndex::ObjectTransform, tr("Transform"), false);

	AddVectorProperty({ lcObjectPropertyId::ObjectPositionX, lcObjectPropertyId::ObjectPositionY, lcObjectPropertyId::ObjectPositionZ }, tr("Position"), tr("Position of the object"), true, -FLT_MAX, FLT_MAX);
	AddVectorProperty({ lcObjectPropertyId::ObjectRotationX, lcObjectPropertyId::ObjectRotationY, lcObjectPropertyId::ObjectRotationZ }, tr("Rotation"), tr("Rotation of the object in degrees"), true, -FLT_MAX, FLT_MAX);

	AddSpacing();

	// Editors without a keyframe button reach into its column so every row ends at the same right edge.
	for (PropertyWidgets& Property : mPropertyWidgets)
	{
		if (!Property.Editor || Property.KeyFrame)
			continue;

		const int Index = mLayout->indexOf(Property.Editor);

		if (Index < 0)
			continue;

		int Row, Column, RowSpan, ColumnSpan;
		mLayout->getItemPosition(Index, &Row, &Column, &RowSpan, &ColumnSpan);

		if (Column != 2 || ColumnSpan != 1)
			continue;

		mLayout->removeWidget(Property.Editor);
		mLayout->addWidget(Property.Editor, Row, 2, 1, 2);
	}

	mLayout->setRowStretch(mLayout->rowCount(), 1);
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
}

void lcPropertiesWidget::SetCategoryWidgetsVisible(CategoryWidgets& Category, bool Visible)
{
	for (lcObjectPropertyId PropertyId : Category.Properties)
		SetPropertyWidgetsVisible(PropertyId, Visible && mPropertyWidgets[static_cast<int>(PropertyId)].Visible);

	for (int Row : Category.SpacingRows)
		mLayout->setRowMinimumHeight(Row, Visible ? 5 : 0);
}

void lcPropertiesWidget::SetPropertyVisible(lcObjectPropertyId PropertyId, bool Visible)
{
	PropertyWidgets& Property = mPropertyWidgets[static_cast<int>(PropertyId)];
	Property.Visible = Visible;

	const CategoryWidgets& Category = mCategoryWidgets[static_cast<int>(Property.Category)];

	SetPropertyWidgetsVisible(PropertyId, Visible && (!Category.Button || Category.Button->IsExpanded()));
}

void lcPropertiesWidget::SetPropertyWidgetsVisible(lcObjectPropertyId PropertyId, bool Visible)
{
	PropertyWidgets& Property = mPropertyWidgets[static_cast<int>(PropertyId)];

	if (Property.Label)
		Property.Label->setVisible(Visible);

	if (Property.Editor)
	{
		Property.Editor->setVisible(Visible);

		// Editors sharing a row also show or hide the row.
		if (Property.Editor->parentWidget() != this)
			Property.Editor->parentWidget()->setVisible(Visible);
	}

	if (Property.KeyFrame)
		Property.KeyFrame->setVisible(Visible);
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
