#include "sw_global.h"
#include "sw_window_main.h"
#include "sw_widget_builder_minifigure.h"
#include "sw_widget_spin_box_double.h"
#include "sw_widget_selection_part.h"
#ifdef SW_ENABLE_TERMINAL
#include "sw_widget_terminal.h"
#endif
#include "sw_widget_properties.h"
#include "sw_widget_timeline.h"
#include "sw_view_widget.h"
#include "sw_widget_list_color.h"
#include "sw_window_utilities.h"
#include "sw_dialog_about.h"
#include "sw_dialog_select.h"
#include "sw_dialog_database_sets.h"
#include "sw_dialog_html.h"
#include "sw_dialog_render.h"
#include "sw_dialog_instructions.h"
#include "sw_dialog_image.h"
#include "sw_profile.h"
#include "sw_view.h"
#include "sw_project.h"
#include "sw_object_part.h"
#include "sw_object_camera.h"
#include "sw_group.h"
#include "sw_data_part.h"
#include "sw_library.h"
#include "sw_colors.h"
#include "sw_widget_preview.h"
#include "sw_model_history.h"
#include "sw_widget_spinner.h"

#if SW_ENABLE_GAMEPAD
#include <QtGamepad/QGamepad>
#endif

#define SW_TAB_LAYOUT_VERSION 0x0002

// use to give a new view its final size so its first fit is the one shown
static void swLayoutNewView(QTabWidget* ModelTabWidget, QWidget* TabWidget, swViewWidget* ViewWidget)
{
	QStackedWidget* TabStack = ModelTabWidget->findChild<QStackedWidget*>(QString(), Qt::FindDirectChildrenOnly);

	if (TabStack)
		TabWidget->setGeometry(TabStack->rect());

	TabWidget->layout()->activate();

	const float Scale = ViewWidget->GetDeviceScale();
	ViewWidget->GetView()->SetSize(ViewWidget->width() * Scale, ViewWidget->height() * Scale);
}

void swMainWindow::SetShadingMode(swShadingMode ShadingMode)
{
	swGetPreferences().mShadingMode = ShadingMode;
	swSetProfileShadingMode(ShadingMode);

	UpdateShadingMode();

	UpdatePartsRendering();
}

void swMainWindow::SetLightingStyle(swLightingStyle LightingStyle)
{
	swGetPreferences().mLightingStyle = LightingStyle;
	swSetProfileLightingStyle(LightingStyle);

	UpdateShadingMode();

	UpdatePartsRendering();
}

void swMainWindow::SetSelectionMode(swSelectionMode SelectionMode)
{
	mSelectionMode = SelectionMode;

	UpdateSelectionMode();
}

void swMainWindow::ToggleViewSphere()
{
	swGetPreferences().mViewSphereEnabled = !swGetPreferences().mViewSphereEnabled;
	swSetProfileInteger(SW_PROFILE_VIEW_SPHERE_ENABLED, swGetPreferences().mViewSphereEnabled);

	swView::UpdateAllViews();
}

void swMainWindow::ToggleAxisIcon()
{
	swGetPreferences().mDrawAxes = !swGetPreferences().mDrawAxes;
	swSetProfileInteger(SW_PROFILE_DRAW_AXES, swGetPreferences().mDrawAxes);

	swView::UpdateAllViews();
}

void swMainWindow::ToggleGrid()
{
	swGetPreferences().mGridEnabled = !swGetPreferences().mGridEnabled;

	swView::UpdateAllViews();
}

void swMainWindow::ToggleLoadPartsBeforeShowing()
{
	swGetPreferences().mLoadPartsBeforeShowing = !swGetPreferences().mLoadPartsBeforeShowing;
	swSetProfileInteger(SW_PROFILE_LOAD_PARTS_BEFORE_SHOWING, swGetPreferences().mLoadPartsBeforeShowing);
}

void swMainWindow::SetHighlightRenderMode(swHighlightRenderMode HighlightRenderMode)
{
	swGetPreferences().mHighlightRenderMode = HighlightRenderMode;
	swSetProfileHighlightRenderMode(HighlightRenderMode);

	swView::UpdateAllViews();
}

void swMainWindow::SetSelectionOutlineWidth(int SelectionOutlineWidth)
{
	SelectionOutlineWidth = qBound(SW_SELECTION_OUTLINE_WIDTH_MINIMUM, SelectionOutlineWidth, SW_SELECTION_OUTLINE_WIDTH_MAXIMUM);

	swGetPreferences().mSelectionOutlineWidth = SelectionOutlineWidth;
	swSetProfileInteger(SW_PROFILE_SELECTION_OUTLINE_WIDTH, SelectionOutlineWidth);

	swView::UpdateAllViews();
}

void swMainWindow::ToggleAutomateEdgeColor()
{
	swGetPreferences().mAutomateEdgeColor = !swGetPreferences().mAutomateEdgeColor;
	swSetProfileInteger(SW_PROFILE_AUTOMATE_EDGE_COLOR, swGetPreferences().mAutomateEdgeColor);

	swGetPartsLibrary()->LoadColors();
	swView::UpdateAllViews();
}

void swMainWindow::SetStudStyle(swStudStyle StudStyle)
{
	swSetProfileStudStyle(StudStyle);
	swGetPartsLibrary()->UpdateStudStyleAndColors(StudStyle, swGetPreferences().mStudCylinderColorEnabled);

	swView::UpdateAllViews();
}

void swMainWindow::ShowStepViewDialog()
{
	swModel* ActiveModel = GetActiveModel();
	swView* ActiveView = GetActiveView();

	if (!ActiveModel || !ActiveView)
		return;

	const swStep Step = ActiveModel->GetCurrentStep();
	const std::map<swStep, swStepView>& StepViews = ActiveModel->GetProperties().mStepViews;
	const std::map<swStep, swStepView>::const_iterator Current = StepViews.find(Step);

	QDialog Dialog(this);
	Dialog.setWindowTitle(tr("Step %1 View").arg(Step));

	QFormLayout* Layout = new QFormLayout(&Dialog);

	QComboBox* ModeCombo = new QComboBox(&Dialog);
	ModeCombo->addItems({ tr("No view change"), tr("Relative to the default view"), tr("Absolute"), tr("Added to the previous view"), tr("Back to the default view") });
	ModeCombo->setToolTip(tr("How this step turns the view, saved as an MLCAD ROTSTEP line ending the step."));
	Layout->addRow(tr("Rotation:"), ModeCombo);

	QDoubleSpinBox* AngleEdits[3];
	const QString AxisNames[3] = { tr("X:"), tr("Y:"), tr("Z:") };

	for (int Axis = 0; Axis < 3; Axis++)
	{
		AngleEdits[Axis] = new QDoubleSpinBox(&Dialog);
		AngleEdits[Axis]->setRange(-360.0, 360.0);
		AngleEdits[Axis]->setDecimals(2);
		AngleEdits[Axis]->setSuffix(tr(" deg."));
		Layout->addRow(AxisNames[Axis], AngleEdits[Axis]);
	}

	QPushButton* CurrentViewButton = new QPushButton(tr("Use Current View"), &Dialog);
	CurrentViewButton->setToolTip(tr("Fill in the absolute rotation that matches the current view."));
	Layout->addRow(QString(), CurrentViewButton);

	QDialogButtonBox* Buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &Dialog);
	Layout->addRow(Buttons);

	auto UpdateEnabled = [ModeCombo, AngleEdits]()
	{
		const bool HasAngles = ModeCombo->currentIndex() >= 1 && ModeCombo->currentIndex() <= 3;

		for (QDoubleSpinBox* AngleEdit : AngleEdits)
			AngleEdit->setEnabled(HasAngles);
	};

	if (Current != StepViews.end())
	{
		ModeCombo->setCurrentIndex(static_cast<int>(Current->second.Mode) + 1);

		for (int Axis = 0; Axis < 3; Axis++)
			AngleEdits[Axis]->setValue(Current->second.Angles[Axis]);
	}

	UpdateEnabled();

	connect(ModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), &Dialog, UpdateEnabled);
	connect(CurrentViewButton, &QPushButton::clicked, &Dialog, [ActiveView, ModeCombo, AngleEdits]()
	{
		const swVector3 Angles = ActiveView->GetStepViewAngles();

		ModeCombo->setCurrentIndex(static_cast<int>(swStepViewMode::Absolute) + 1);

		for (int Axis = 0; Axis < 3; Axis++)
			AngleEdits[Axis]->setValue(Angles[Axis]);
	});
	connect(Buttons, &QDialogButtonBox::accepted, &Dialog, &QDialog::accept);
	connect(Buttons, &QDialogButtonBox::rejected, &Dialog, &QDialog::reject);

	if (Dialog.exec() != QDialog::Accepted)
		return;

	std::optional<swStepView> StepView;

	if (ModeCombo->currentIndex() > 0)
	{
		StepView = swStepView();
		StepView->Mode = static_cast<swStepViewMode>(ModeCombo->currentIndex() - 1);

		if (StepView->Mode != swStepViewMode::End)
			StepView->Angles = swVector3(AngleEdits[0]->value(), AngleEdits[1]->value(), AngleEdits[2]->value());
	}

	ActiveModel->SetStepView(Step, StepView);
}

void swMainWindow::ToggleFadePreviousSteps()
{
	swGetPreferences().mFadeSteps = !swGetPreferences().mFadeSteps;
	swSetProfileInteger(SW_PROFILE_FADE_STEPS, swGetPreferences().mFadeSteps);

	swView::UpdateAllViews();
}

void swMainWindow::ToggleEdgeLines(swRenderTarget Target)
{
	swPreferences& Preferences = swGetPreferences();

	if (Target == swRenderTarget::ModelView)
	{
		Preferences.mDrawEdgeLines = !Preferences.mDrawEdgeLines;
		swSetProfileInteger(SW_PROFILE_DRAW_EDGE_LINES, Preferences.mDrawEdgeLines);
	}
	else
	{
		swTargetRenderSettings& TargetSettings = Preferences.GetTargetRenderSettings(Target);

		// Changing a target setting stops following the model view settings.
		TargetSettings.Settings = Preferences.GetRenderSettings(Target);
		TargetSettings.Settings.DrawEdgeLines = !TargetSettings.Settings.DrawEdgeLines;
		TargetSettings.SyncRendering = false;
		Preferences.SaveRenderTarget(Target);
	}

	UpdatePartsRendering();
}

void swMainWindow::ToggleConditionalLines(swRenderTarget Target)
{
	swPreferences& Preferences = swGetPreferences();

	if (Target == swRenderTarget::ModelView)
	{
		Preferences.mDrawConditionalLines = !Preferences.mDrawConditionalLines;
		swSetProfileInteger(SW_PROFILE_DRAW_CONDITIONAL_LINES, Preferences.mDrawConditionalLines);
	}
	else
	{
		swTargetRenderSettings& TargetSettings = Preferences.GetTargetRenderSettings(Target);

		TargetSettings.Settings = Preferences.GetRenderSettings(Target);
		TargetSettings.Settings.DrawConditionalLines = !TargetSettings.Settings.DrawConditionalLines;
		TargetSettings.SyncRendering = false;
		Preferences.SaveRenderTarget(Target);
	}

	UpdatePartsRendering();
}

void swMainWindow::ToggleSyncRendering(swRenderTarget Target)
{
	swPreferences& Preferences = swGetPreferences();
	swTargetRenderSettings& TargetSettings = Preferences.GetTargetRenderSettings(Target);

	TargetSettings.SyncRendering = !TargetSettings.SyncRendering;
	Preferences.SaveRenderTarget(Target);

	UpdatePartsRendering();
}

void swMainWindow::SetTargetShadingMode(swRenderTarget Target, swShadingMode ShadingMode)
{
	swPreferences& Preferences = swGetPreferences();
	swTargetRenderSettings& TargetSettings = Preferences.GetTargetRenderSettings(Target);

	TargetSettings.Settings = Preferences.GetRenderSettings(Target);
	TargetSettings.Settings.ShadingMode = ShadingMode;
	TargetSettings.SyncRendering = false;
	Preferences.SaveRenderTarget(Target);

	UpdatePartsRendering();
}

QAction* swMainWindow::AddMenuCommand(QMenu* Menu, const QString& Text, swCommandId CommandId, bool Checked)
{
	// Menu entries run the matching command so keyboard shortcuts and menus stay in sync.
	QAction* Action = Menu->addAction(Text, this, [this, CommandId]()
	{
		HandleCommand(CommandId);
	});

	Action->setCheckable(true);
	Action->setChecked(Checked);
	Action->setShortcut(mActions[CommandId]->shortcut());
	Action->setShortcutContext(Qt::WidgetShortcut);

	return Action;
}

void swMainWindow::AddRenderTargetActions(QMenu* Menu, swRenderTarget Target)
{
	struct swRenderTargetCommands
	{
		swCommandId SyncRendering;
		swCommandId Wireframe;
		swCommandId Flat;
		swCommandId DefaultLights;
		swCommandId EdgeLines;
		swCommandId ConditionalLines;
	};

	// The model view commands, the other targets replace them with their own.
	swRenderTargetCommands Commands = { SW_COMMAND_COUNT, SW_VIEW_SHADING_WIREFRAME, SW_VIEW_SHADING_FLAT, SW_VIEW_SHADING_DEFAULT_LIGHTS, SW_VIEW_TOGGLE_EDGE_LINES, SW_VIEW_TOGGLE_CONDITIONAL_LINES };

	switch (Target)
	{
	case swRenderTarget::ModelView:
		break;

	case swRenderTarget::Preview:
		Commands = { SW_VIEW_TOGGLE_PREVIEW_SYNCHRONIZED_RENDERING, SW_VIEW_PREVIEW_SHADING_WIREFRAME, SW_VIEW_PREVIEW_SHADING_FLAT, SW_VIEW_PREVIEW_SHADING_DEFAULT_LIGHTS, SW_VIEW_TOGGLE_PREVIEW_EDGE_LINES, SW_VIEW_TOGGLE_PREVIEW_CONDITIONAL_LINES };
		break;

	case swRenderTarget::MinifigureBuilder:
		Commands = { SW_VIEW_TOGGLE_MINIFIGURE_BUILDER_SYNCHRONIZED_RENDERING, SW_VIEW_MINIFIGURE_BUILDER_SHADING_WIREFRAME, SW_VIEW_MINIFIGURE_BUILDER_SHADING_FLAT, SW_VIEW_MINIFIGURE_BUILDER_SHADING_DEFAULT_LIGHTS, SW_VIEW_TOGGLE_MINIFIGURE_BUILDER_EDGE_LINES, SW_VIEW_TOGGLE_MINIFIGURE_BUILDER_CONDITIONAL_LINES };
		break;

	case swRenderTarget::Parts:
		Commands = { SW_VIEW_TOGGLE_PARTS_SYNCHRONIZED_RENDERING, SW_VIEW_PARTS_SHADING_WIREFRAME, SW_VIEW_PARTS_SHADING_FLAT, SW_VIEW_PARTS_SHADING_DEFAULT_LIGHTS, SW_VIEW_TOGGLE_PARTS_EDGE_LINES, SW_VIEW_TOGGLE_PARTS_CONDITIONAL_LINES };
		break;
	}

	const swPreferences& Preferences = swGetPreferences();
	const swRenderSettings RenderSettings = Preferences.GetRenderSettings(Target);

	// The model views have no settings to follow.
	if (Target != swRenderTarget::ModelView)
		AddMenuCommand(Menu, tr("Use Model View Settings"), Commands.SyncRendering, Preferences.GetTargetRenderSettings(Target).SyncRendering);

	QMenu* ShadingMenu = Menu->addMenu(tr("Shading"));
	QActionGroup* ShadingGroup = new QActionGroup(ShadingMenu);
	ShadingGroup->addAction(AddMenuCommand(ShadingMenu, tr("Wireframe"), Commands.Wireframe, RenderSettings.ShadingMode == swShadingMode::Wireframe));
	ShadingGroup->addAction(AddMenuCommand(ShadingMenu, tr("Flat Shading"), Commands.Flat, RenderSettings.ShadingMode == swShadingMode::Flat));
	ShadingGroup->addAction(AddMenuCommand(ShadingMenu, tr("Default Lights"), Commands.DefaultLights, RenderSettings.ShadingMode == swShadingMode::DefaultLights));
	ShadingMenu->addSeparator();

	// the lighting style is shared by every target
	const char* const LightingNames[] = { QT_TR_NOOP("Standard Lighting"), QT_TR_NOOP("Studio Lighting"), QT_TR_NOOP("Matte Lighting"), QT_TR_NOOP("Toon Lighting") };
	QActionGroup* LightingGroup = new QActionGroup(ShadingMenu);

	for (int ActionIdx = SW_VIEW_LIGHTING_FIRST; ActionIdx <= SW_VIEW_LIGHTING_LAST; ActionIdx++)
	{
		const int StyleIndex = ActionIdx - SW_VIEW_LIGHTING_FIRST;
		LightingGroup->addAction(AddMenuCommand(ShadingMenu, tr(LightingNames[StyleIndex]), static_cast<swCommandId>(ActionIdx), static_cast<int>(Preferences.mLightingStyle) == StyleIndex));
	}

	AddMenuCommand(Menu, tr("Edge Lines"), Commands.EdgeLines, RenderSettings.DrawEdgeLines);
	AddMenuCommand(Menu, tr("Conditional Lines"), Commands.ConditionalLines, RenderSettings.DrawConditionalLines);
}

void swMainWindow::ModelViewOptionsMenuAboutToShow()
{
	QMenu* Menu = qobject_cast<QMenu*>(sender());
	Menu->clear();

	const swPreferences& Preferences = swGetPreferences();

	AddMenuCommand(Menu, tr("Fade Previous Steps"), SW_VIEW_FADE_PREVIOUS_STEPS, Preferences.mFadeSteps);
	AddMenuCommand(Menu, tr("Insert First Part at Origin"), SW_PART_TOGGLE_FIRST_AT_ORIGIN, Preferences.mFirstPartAtOrigin);

	Menu->addSeparator();

	AddMenuCommand(Menu, tr("View Sphere"), SW_VIEW_TOGGLE_VIEW_SPHERE, Preferences.mViewSphereEnabled);
	AddMenuCommand(Menu, tr("Axis Icon"), SW_VIEW_TOGGLE_AXIS_ICON, Preferences.mDrawAxes);
	AddMenuCommand(Menu, tr("Base Grid"), SW_VIEW_TOGGLE_GRID, Preferences.mGridEnabled);
	AddMenuCommand(Menu, tr("XZ Height Grid"), SW_VIEW_GRID_TOGGLE_HEIGHT_XZ, Preferences.mDrawGridHeightXZ);
	AddMenuCommand(Menu, tr("YZ Height Grid"), SW_VIEW_GRID_TOGGLE_HEIGHT_YZ, Preferences.mDrawGridHeightYZ);
	AddMenuCommand(Menu, tr("Height Grids Behind Model"), SW_VIEW_GRID_TOGGLE_HEIGHT_BEHIND, Preferences.mGridHeightBehindModel);

	Menu->addSeparator();

	AddRenderTargetActions(Menu, swRenderTarget::ModelView);
}

void swMainWindow::TogglePreviewViewSphere()
{
	swGetPreferences().mPreviewViewSphereEnabled = !swGetPreferences().mPreviewViewSphereEnabled;
	swSetProfileInteger(SW_PROFILE_PREVIEW_VIEW_SPHERE_ENABLED, swGetPreferences().mPreviewViewSphereEnabled);

	swView::UpdateAllViews();
}

void swMainWindow::TogglePreviewAxisIcon()
{
	swGetPreferences().mDrawPreviewAxis = !swGetPreferences().mDrawPreviewAxis;
	swSetProfileInteger(SW_PROFILE_PREVIEW_DRAW_AXES, swGetPreferences().mDrawPreviewAxis);

	swView::UpdateAllViews();
}

void swMainWindow::UpdatePartsRendering()
{
	swView::UpdateAllViews();

	if (mPartSelectionWidget)
		mPartSelectionWidget->UpdateThumbnails();
}

QByteArray swMainWindow::GetTabLayout()
{
	QByteArray TabLayoutData;
	QDataStream DataStream(&TabLayoutData, QIODevice::WriteOnly);

	DataStream << (quint32)SW_TAB_LAYOUT_VERSION;
	qint32 NumTabs = mModelTabWidget->count();
	DataStream << NumTabs;
	DataStream << ((swModelTabWidget*)mModelTabWidget->currentWidget())->GetModel()->GetProperties().mFileName;

	for (int TabIdx = 0; TabIdx < NumTabs; TabIdx++)
	{
		swModelTabWidget* TabWidget = (swModelTabWidget*)mModelTabWidget->widget(TabIdx);

		DataStream << TabWidget->GetModel()->GetProperties().mFileName;

		std::function<void (QWidget*)> SaveWidget = [&DataStream, &SaveWidget, &TabWidget](QWidget* Widget)
		{
			if (Widget->metaObject() == &swViewWidget::staticMetaObject)
			{
				swView* CurrentView = ((swViewWidget*)Widget)->GetView();

				DataStream << (qint32)0;
				DataStream << (qint32)(TabWidget->GetActiveView() == CurrentView ? 1 : 0);

				const swCamera* Camera = CurrentView->GetCamera();

				// a view still on its fit is fitted again when restored
				if (CurrentView->IsFitted())
				{
					DataStream << (qint32)2;
					DataStream << Camera->m_fovy;
					DataStream << Camera->m_zNear;
					DataStream << Camera->m_zFar;
				}
				else if (Camera->IsSimple())
				{
					DataStream << (qint32)0;
					DataStream << Camera->m_fovy;
					DataStream << Camera->m_zNear;
					DataStream << Camera->m_zFar;
					DataStream << Camera->mPosition;
					DataStream << Camera->mTargetPosition;
					DataStream << Camera->mUpVector;
				}
				else
				{
					DataStream << (qint32)1;
					DataStream << Camera->GetName();
				}
			}
			else
			{
				QSplitter* Splitter = (QSplitter*)Widget;

				DataStream << (qint32)(Splitter->orientation() == Qt::Horizontal ? 1 : 2);
				DataStream << Splitter->sizes();

				SaveWidget(Splitter->widget(0));
				SaveWidget(Splitter->widget(1));
			}
		};

		QLayout* TabLayout = TabWidget->layout();
		SaveWidget(TabLayout->itemAt(0)->widget());
	}

	return TabLayoutData;
}

void swMainWindow::RestoreTabLayout(const QByteArray& TabLayout)
{
	if (TabLayout.isEmpty())
		return;

	QDataStream DataStream(TabLayout);

	quint32 Version;
	DataStream >> Version;

	if (Version != SW_TAB_LAYOUT_VERSION)
		return;

	qint32 NumTabs;
	DataStream >> NumTabs;
	QString CurrentTabName;
	DataStream >> CurrentTabName;

	RemoveAllModelTabs();

	for (int TabIdx = 0; TabIdx < NumTabs; TabIdx++)
	{
		QString ModelName;
		DataStream >> ModelName;

		swModel* Model = swGetActiveProject()->GetModel(ModelName);
		swModelTabWidget* TabWidget = nullptr;

		if (Model)
		{
			SetCurrentModelTab(Model, false);
			TabWidget = (swModelTabWidget*)mModelTabWidget->widget(mModelTabWidget->count() - 1);
		}

		QWidget* ActiveWidget = nullptr;

		std::function<void(QWidget*)> LoadWidget = [&DataStream, &LoadWidget, &ActiveWidget, this](QWidget* ParentWidget)
		{
			qint32 WidgetType;
			DataStream >> WidgetType;

			if (WidgetType == 0)
			{
				qint32 IsActive;
				DataStream >> IsActive;

				if (IsActive)
					ActiveWidget = ParentWidget;

				qint32 CameraType;
				DataStream >> CameraType;

				swView* CurrentView = nullptr;

				if (ParentWidget)
					CurrentView = ((swViewWidget*)ParentWidget)->GetView();

				if (CameraType == 0)
				{
					float FoV, ZNear, ZFar;
					swVector3 Position, TargetPosition, UpVector;

					DataStream >> FoV;
					DataStream >> ZNear;
					DataStream >> ZFar;
					DataStream >> Position;
					DataStream >> TargetPosition;
					DataStream >> UpVector;

					if (CurrentView)
					{
						swCamera* Camera = CurrentView->GetCamera();
						if (!std::isnan(FoV))
							Camera->m_fovy = FoV;
						if (!std::isnan(ZNear))
							Camera->m_zNear = ZNear;
						if (!std::isnan(ZFar))
							Camera->m_zFar = ZFar;
						if (!Position.IsNan() && !TargetPosition.IsNan() && !UpVector.IsNan())
						{
							Camera->SetPosition(Position, 1, false);
							Camera->SetTargetPosition(TargetPosition, 1, false);
							Camera->SetUpVector(UpVector, 1, false);
						}
						Camera->UpdatePosition(1);
						Camera->ExpandFarPlane(CurrentView->GetModel()->GetPartsBoundingBoxPoints(swGeometryBoundsMode::IncludeFallback));
					}
				}
				else if (CameraType == 2)
				{
					float FoV, ZNear, ZFar;

					DataStream >> FoV;
					DataStream >> ZNear;
					DataStream >> ZFar;

					if (CurrentView && CurrentView->GetCamera()->IsSimple())
					{
						swCamera* Camera = CurrentView->GetCamera();
						if (!std::isnan(FoV))
							Camera->m_fovy = FoV;
						if (!std::isnan(ZNear))
							Camera->m_zNear = ZNear;
						if (!std::isnan(ZFar))
							Camera->m_zFar = ZFar;
						CurrentView->ZoomExtents(swGeometryBoundsMode::IncludeFallback, true);
					}
				}
				else
				{
					QByteArray CameraName;
					DataStream >> CameraName;

					if (CurrentView)
						CurrentView->SetCamera(CameraName);
				}
			}
			else
			{
				QList<int> Sizes;
				DataStream >> Sizes;

				if (ParentWidget)
				{
					ParentWidget->setFocus();

					if (WidgetType == 1)
						SplitVertical();
					else
						SplitHorizontal();

					QSplitter* Splitter = (QSplitter*)ParentWidget->parentWidget();
					Splitter->setSizes(Sizes);

					LoadWidget(Splitter->widget(0));
					LoadWidget(Splitter->widget(1));
				}
				else
				{
					LoadWidget(nullptr);
					LoadWidget(nullptr);
				}
			}
		};

		LoadWidget(TabWidget ? TabWidget->layout()->itemAt(0)->widget() : nullptr);

		if (ActiveWidget && TabWidget)
			ActiveWidget->setFocus();
	}

	if (!mModelTabWidget->count())
		swGetActiveProject()->SetActiveModel(0, true);
	else
		swGetActiveProject()->SetActiveModel(CurrentTabName, true);
}

void swMainWindow::RemoveAllModelTabs()
{
	while (mModelTabWidget->count())
		delete mModelTabWidget->widget(0);
}

void swMainWindow::CloseCurrentModelTab()
{
	if (mModelTabWidget->count() > 1)
		delete mModelTabWidget->currentWidget();
	else
		NewProject();
}

void swMainWindow::SetCurrentModelTab(swModel* Model, bool AdjustFarPlane)
{
	for (int TabIdx = 0; TabIdx < mModelTabWidget->count(); TabIdx++)
	{
		swModelTabWidget* TabWidget = (swModelTabWidget*)mModelTabWidget->widget(TabIdx);

		if (TabWidget->GetModel() == Model)
		{
			mModelTabWidget->setCurrentIndex(TabIdx);
			return;
		}
	}

	swModelTabWidget* TabWidget = new swModelTabWidget(Model);
	mModelTabWidget->addTab(TabWidget, Model->GetProperties().mFileName);

	QVBoxLayout* CentralLayout = new QVBoxLayout(TabWidget);
	CentralLayout->setContentsMargins(0, 0, 0, 0);

	swView* NewView = CreateView(Model);
	swViewWidget* ViewWidget = new swViewWidget(TabWidget, NewView);
	CentralLayout->addWidget(ViewWidget);

	mModelTabWidget->setCurrentWidget(TabWidget);

	ViewWidget->show();
	ViewWidget->setFocus();
	swLayoutNewView(mModelTabWidget, TabWidget, ViewWidget);
	NewView->ZoomExtents(swGeometryBoundsMode::IncludeFallback, AdjustFarPlane);
}

void swMainWindow::AddView(swView* View)
{
	swModelTabWidget* TabWidget = GetTabWidgetForModel(View->GetModel());

	if (!TabWidget)
		return;

	if (!TabWidget->GetActiveView())
		TabWidget->SetActiveView(View);
}

void swMainWindow::RemoveView(swView* View)
{
	swModelTabWidget* TabWidget = GetTabForView(View);

	if (TabWidget)
		TabWidget->RemoveView(View);
}

void swMainWindow::SetActiveView(swView* ActiveView)
{
	swModelTabWidget* TabWidget = GetTabForView(ActiveView);

	if (!TabWidget)
		return;

	swView* CurrentActiveView = TabWidget->GetActiveView();

	if (CurrentActiveView == ActiveView)
		return;

	if (CurrentActiveView)
		CurrentActiveView->SetTopSubmodelActive();

	TabWidget->SetActiveView(ActiveView);
}

void swMainWindow::SetTool(swTool Tool)
{
	mTool = Tool;

	QAction* Action = mActions[SW_EDIT_ACTION_FIRST + static_cast<int>(mTool)];

	if (Action)
		Action->setChecked(true);

	swView::UpdateAllViews();
}

void swMainWindow::SplitView(Qt::Orientation Orientation)
{
	QWidget* Focus = focusWidget();

	if (Focus->metaObject() != &swViewWidget::staticMetaObject)
		return;

	QWidget* Parent = Focus->parentWidget();
	QSplitter* Splitter;
	QList<int> Sizes;

	if (Parent->metaObject() == &swModelTabWidget::staticMetaObject)
	{
		Splitter = new QSplitter(Orientation, Parent);
		Parent->layout()->addWidget(Splitter);
		Splitter->addWidget(Focus);
		Splitter->addWidget(new swViewWidget(mModelTabWidget->currentWidget(), CreateView(GetCurrentTabModel())));
	}
	else
	{
		QSplitter* ParentSplitter = (QSplitter*)Parent;
		Sizes = ParentSplitter->sizes();
		int FocusIndex = ParentSplitter->indexOf(Focus);

		Splitter = new QSplitter(Orientation, Parent);
		ParentSplitter->insertWidget(FocusIndex, Splitter);
		Splitter->addWidget(Focus);
		Splitter->addWidget(new swViewWidget(mModelTabWidget->currentWidget(), CreateView(GetCurrentTabModel())));

		ParentSplitter->setSizes(Sizes);
	}

	Sizes.clear();
	Sizes.append(10);
	Sizes.append(10);
	Splitter->setSizes(Sizes);
	Focus->setFocus();
}

void swMainWindow::SplitHorizontal()
{
	SplitView(Qt::Vertical);
}

void swMainWindow::SplitVertical()
{
	SplitView(Qt::Horizontal);
}

void swMainWindow::RemoveActiveView()
{
	QWidget* Focus = focusWidget();

	if (Focus->metaObject() != &swViewWidget::staticMetaObject)
		return;

	QWidget* Parent = Focus->parentWidget();

	if (Parent->metaObject() == &swModelTabWidget::staticMetaObject)
		return;

	QWidget* ParentParentWidget = Parent->parentWidget();
	QSplitter* ParentSplitter = (QSplitter*)Parent;
	int FocusIndex = ParentSplitter->indexOf(Focus);
	QWidget* OtherWidget = ParentSplitter->widget(!FocusIndex);

	if (ParentParentWidget->metaObject() == &swModelTabWidget::staticMetaObject)
	{
		QLayout* CentralLayout = ParentParentWidget->layout();

		CentralLayout->addWidget(OtherWidget);
		CentralLayout->removeWidget(Parent);
	}
	else
	{
		QSplitter* ParentParentSplitter = (QSplitter*)ParentParentWidget;
		QList<int> Sizes = ParentParentSplitter->sizes();

		int ParentIndex = ParentParentSplitter->indexOf(Parent);
		Parent->setParent(nullptr);
		ParentParentSplitter->insertWidget(ParentIndex, OtherWidget);

		ParentParentSplitter->setSizes(Sizes);
	}

	Parent->deleteLater();

	if (OtherWidget->metaObject() != &swViewWidget::staticMetaObject)
	{
		swModelTabWidget* TabWidget = (swModelTabWidget*)mModelTabWidget->currentWidget();

		if (TabWidget)
			OtherWidget = TabWidget->GetAnyViewWidget();
	}

	OtherWidget->setFocus();
}

void swMainWindow::ResetViews()
{
	swModelTabWidget* TabWidget = (swModelTabWidget*)mModelTabWidget->currentWidget();

	if (!TabWidget)
		return;

	QLayout* TabLayout = TabWidget->layout();
	QWidget* TopWidget = TabLayout->itemAt(0)->widget();
	TopWidget->hide();
	TopWidget->deleteLater();

	swView* NewView = CreateView(TabWidget->GetModel());
	swViewWidget* ViewWidget = new swViewWidget(TabWidget, NewView);
	TabLayout->addWidget(ViewWidget);

	ViewWidget->show();
	ViewWidget->setFocus();
	swLayoutNewView(mModelTabWidget, TabWidget, ViewWidget);
	NewView->ZoomExtents(swGeometryBoundsMode::IncludeFallback, true);
}
