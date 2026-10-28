#include "sw_global.h"
#include "sw_dialog_preferences.h"
#include "ui_sw_dialog_preferences.h"
#include "sw_window_utilities.h"
#include "sw_dialog_category.h"
#include "sw_library.h"
#include "sw_application.h"
#include "sw_window_utilities.h"
#include "sw_extensions_opengl.h"
#include "sw_data_part.h"
#include "sw_dialog_color_edge.h"
#include "sw_dialog_preferences_blender.h"
#include "sw_window_main.h"
#include "sw_view_widget.h"
#include "sw_view.h"
#include "sw_context.h"
#include "sw_dialog_setup_page.h"
#include "sw_dialog_instructions_properties.h"
#include "sw_profile.h"

static const char* gLanguageLocales[] =
{
	"", "cs_CZ", "de_DE", "en_US", "fr_FR", "pt_PT", "es_ES", "ru_RU", "uk_UA", "zh_CN", "zh_TW"
};

// use to tell the controls that share the control column width, line edits stretch and plain buttons keep their size
static bool swIsColumnControl(QWidget* Widget)
{
	if (QToolButton* Button = qobject_cast<QToolButton*>(Widget))
		return Button->toolButtonStyle() != Qt::ToolButtonIconOnly && !Button->text().isEmpty();

	return qobject_cast<QComboBox*>(Widget) || qobject_cast<QAbstractSpinBox*>(Widget) || qobject_cast<QSlider*>(Widget);
}

// Lines up the label and control columns of all groups on all pages, so controls start at the same position on every tab.
static void swAlignPreferencesColumns(QTabWidget* TabWidget)
{
	int LabelWidth = 0;

	// the label column is shared by every page so switching tabs keeps the controls in place
	for (int PageIndex = 0; PageIndex < TabWidget->count(); PageIndex++)
	{
		for (QGroupBox* Group : TabWidget->widget(PageIndex)->findChildren<QGroupBox*>())
		{
			if (QFormLayout* FormLayout = qobject_cast<QFormLayout*>(Group->layout()))
			{
				for (int Row = 0; Row < FormLayout->rowCount(); Row++)
				{
					QLayoutItem* Item = FormLayout->itemAt(Row, QFormLayout::LabelRole);

					if (Item && Item->widget())
						LabelWidth = qMax(LabelWidth, Item->sizeHint().width());

					// checkboxes spanning the row end before the controls start
					Item = FormLayout->itemAt(Row, QFormLayout::SpanningRole);

					if (Item && qobject_cast<QCheckBox*>(Item->widget()))
						LabelWidth = qMax(LabelWidth, Item->sizeHint().width());
				}
			}
			else if (QGridLayout* GridLayout = qobject_cast<QGridLayout*>(Group->layout()))
			{
				for (int ItemIndex = 0; ItemIndex < GridLayout->count(); ItemIndex++)
				{
					int Row, Column, RowSpan, ColumnSpan;
					GridLayout->getItemPosition(ItemIndex, &Row, &Column, &RowSpan, &ColumnSpan);

					// checkboxes spanning the row end before the controls start
					QWidget* Widget = GridLayout->itemAt(ItemIndex)->widget();

					if (Column == 0 && Widget && (ColumnSpan == 1 || qobject_cast<QCheckBox*>(Widget)))
						LabelWidth = qMax(LabelWidth, GridLayout->itemAt(ItemIndex)->sizeHint().width());
				}
			}
		}
	}

	// the longest label keeps the button spacing clear of its control on top of the layout spacing
	LabelWidth += swGetLayoutSpacing(TabWidget, Qt::Horizontal);

	for (int PageIndex = 0; PageIndex < TabWidget->count(); PageIndex++)
	{
		QList<QGridLayout*> GridLayouts;
		QList<QFormLayout*> FormLayouts;
		QList<QWidget*> Controls;
		QList<QWidget*> FillControls;
		int ControlWidth = 0;

		for (QGroupBox* Group : TabWidget->widget(PageIndex)->findChildren<QGroupBox*>())
		{
			if (QFormLayout* FormLayout = qobject_cast<QFormLayout*>(Group->layout()))
			{
				for (int Row = 0; Row < FormLayout->rowCount(); Row++)
				{
					QLayoutItem* Item = FormLayout->itemAt(Row, QFormLayout::FieldRole);

					if (!Item || !FormLayout->itemAt(Row, QFormLayout::LabelRole) || !swIsColumnControl(Item->widget()))
						continue;

					Item->widget()->setSizePolicy(QSizePolicy::Expanding, Item->widget()->sizePolicy().verticalPolicy());
					Controls.append(Item->widget());
				}

				// the controls reach the right edge of their group like the path fields
				FormLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
				FormLayouts.append(FormLayout);
				continue;
			}

			QGridLayout* GridLayout = qobject_cast<QGridLayout*>(Group->layout());

			if (!GridLayout)
				continue;

			// groups of plain checkboxes keep their own layout, a checkbox beside a control is a label
			bool HasLabels = false;

			for (int ItemIndex = 0; ItemIndex < GridLayout->count(); ItemIndex++)
			{
				int Row, Column, RowSpan, ColumnSpan;
				GridLayout->getItemPosition(ItemIndex, &Row, &Column, &RowSpan, &ColumnSpan);
				QWidget* Widget = GridLayout->itemAt(ItemIndex)->widget();

				if (Column != 0 || ColumnSpan != 1)
					continue;

				if (qobject_cast<QLabel*>(Widget) || (qobject_cast<QCheckBox*>(Widget) && GridLayout->itemAtPosition(Row, 1)))
					HasLabels = true;
			}

			if (!HasLabels)
				continue;

			// the spacers no longer push the controls away from the right edge
			int LastColumn = 1;

			for (int ItemIndex = 0; ItemIndex < GridLayout->count(); ItemIndex++)
			{
				int Row, Column, RowSpan, ColumnSpan;
				GridLayout->getItemPosition(ItemIndex, &Row, &Column, &RowSpan, &ColumnSpan);

				if (QSpacerItem* Spacer = GridLayout->itemAt(ItemIndex)->spacerItem())
					Spacer->changeSize(0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
				else
					LastColumn = qMax(LastColumn, Column + ColumnSpan - 1);
			}

			QList<QWidget*> SpanControls;

			for (int ItemIndex = 0; ItemIndex < GridLayout->count(); ItemIndex++)
			{
				QLayoutItem* Item = GridLayout->itemAt(ItemIndex);
				QWidget* Widget = Item->widget();
				int Row, Column, RowSpan, ColumnSpan;
				GridLayout->getItemPosition(ItemIndex, &Row, &Column, &RowSpan, &ColumnSpan);

				if (!Widget || Column != 1)
					continue;

				if (qobject_cast<QCheckBox*>(Widget) || qobject_cast<QLabel*>(Widget))
				{
					Item->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
					continue;
				}

				Item->setAlignment(Qt::Alignment());
				Widget->setSizePolicy(QSizePolicy::Expanding, Widget->sizePolicy().verticalPolicy());

				if (swIsColumnControl(Widget))
					Controls.append(Widget);
				else
					FillControls.append(Widget);

				SpanControls.append(Widget);
			}

			// the values and units after the controls share a width so the sliders of a group end together
			int TrailingWidth = 0;

			for (QWidget* Widget : SpanControls)
			{
				int Row, Column, RowSpan, ColumnSpan;
				GridLayout->getItemPosition(GridLayout->indexOf(Widget), &Row, &Column, &RowSpan, &ColumnSpan);

				for (int TrailingColumn = Column + ColumnSpan; TrailingColumn <= LastColumn; TrailingColumn++)
					if (QLayoutItem* Trailing = GridLayout->itemAtPosition(Row, TrailingColumn))
						if (qobject_cast<QLabel*>(Trailing->widget()))
							TrailingWidth = qMax(TrailingWidth, Trailing->widget()->sizeHint().width());
			}

			// every control spans to the right edge, a value, unit or button after it sits at the edge
			for (QWidget* Widget : SpanControls)
			{
				int Row, Column, RowSpan, ColumnSpan;
				GridLayout->getItemPosition(GridLayout->indexOf(Widget), &Row, &Column, &RowSpan, &ColumnSpan);
				QList<QWidget*> Trailing;

				for (int TrailingColumn = Column + ColumnSpan; TrailingColumn <= LastColumn; TrailingColumn++)
					if (QLayoutItem* TrailingItem = GridLayout->itemAtPosition(Row, TrailingColumn))
						if (TrailingItem->widget() && !Trailing.contains(TrailingItem->widget()))
							Trailing.append(TrailingItem->widget());

				GridLayout->removeWidget(Widget);

				if (Trailing.isEmpty())
				{
					GridLayout->addWidget(Widget, Row, 1, RowSpan, LastColumn);
					continue;
				}

				QHBoxLayout* RowLayout = new QHBoxLayout();
				RowLayout->addWidget(Widget, 1);

				for (QWidget* TrailingWidget : Trailing)
				{
					GridLayout->removeWidget(TrailingWidget);

					if (qobject_cast<QLabel*>(TrailingWidget))
					{
						TrailingWidget->setMinimumWidth(TrailingWidth);
						static_cast<QLabel*>(TrailingWidget)->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
						RowLayout->addWidget(TrailingWidget, 0, Qt::AlignRight | Qt::AlignVCenter);
					}
					else
						RowLayout->addWidget(TrailingWidget);
				}

				GridLayout->addLayout(RowLayout, Row, 1, RowSpan, LastColumn);
			}

			GridLayouts.append(GridLayout);
		}

		for (QWidget* Control : Controls)
			ControlWidth = qMax(ControlWidth, Control->sizeHint().width());

		// every control of the page is as wide as the widest one, whatever its size policy
		for (QWidget* Control : Controls)
		{
			Control->setProperty("swColumnWidth", ControlWidth);
			Control->setMinimumWidth(ControlWidth);
		}

		// the other controls of the column, such as line edits and buttons, are never narrower
		for (QWidget* Control : FillControls)
			Control->setMinimumWidth(ControlWidth);

		for (QGridLayout* GridLayout : GridLayouts)
		{
			GridLayout->setColumnMinimumWidth(0, LabelWidth);
			GridLayout->setColumnMinimumWidth(1, ControlWidth);

			// only the control column grows so the controls reach the right edge of their group
			for (int Column = 0; Column < GridLayout->columnCount(); Column++)
				GridLayout->setColumnStretch(Column, Column == 1 ? 1 : 0);
		}

		for (QFormLayout* FormLayout : FormLayouts)
		{
			FormLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);

			for (int Row = 0; Row < FormLayout->rowCount(); Row++)
				if (QLayoutItem* Item = FormLayout->itemAt(Row, QFormLayout::LabelRole))
					if (QWidget* Label = Item->widget())
						Label->setMinimumWidth(LabelWidth);
		}
	}
}

swPreferencesDialog::swPreferencesDialog(QWidget* Parent, swPreferencesDialogOptions* Options)
	: QDialog(Parent), mOptions(Options), ui(new Ui::swPreferencesDialog)
{
	ui->setupUi(this);
	swSetupDialogHelp(this, ui->buttonBox);

	ui->partsLibraryBrowse->setIcon(swCreateInterfaceIcon("toolbars/standard", "file_open"));
	ui->partsArchiveBrowse->setIcon(swCreateInterfaceIcon("dialogs", "library_archive"));

#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
	ui->povrayLabel->hide();
	ui->povrayExecutable->hide();
	ui->povrayExecutableBrowse->hide();
	delete ui->povrayLabel;
	delete ui->povrayLayout;
#endif

	ui->Language->addItem("\u010cesky");
	ui->Language->addItem("Deutsch");
	ui->Language->addItem("English");
	ui->Language->addItem("Fran\u00e7ais");
	ui->Language->addItem("Portugu\u00eas");
	ui->Language->addItem("Espa\u00f1ol");
	ui->Language->addItem("\u0420\u0443\u0441\u0441\u043a\u0438\u0439");
	ui->Language->addItem("\u0423\u043a\u0440\u0430\u0457\u043d\u0441\u044c\u043a\u0430");
	ui->Language->addItem("\u7b80\u4f53\u4e2d\u6587");
	ui->Language->addItem("\u7e41\u9ad4\u4e2d\u6587");

	connect(ui->partsLibraryBrowse, &QPushButton::clicked, this, &swPreferencesDialog::PartsLibraryBrowseClicked);
	connect(ui->partsArchiveBrowse, &QPushButton::clicked, this, &swPreferencesDialog::PartsArchiveBrowseClicked);
	connect(ui->ColorConfigBrowseButton, &QPushButton::clicked, this, &swPreferencesDialog::ColorConfigBrowseButtonClicked);
	connect(ui->MinifigureBuilderSettingsBrowseButton, &QPushButton::clicked, this, &swPreferencesDialog::MinifigureBuilderSettingsBrowseButtonClicked);
	connect(ui->povrayExecutableBrowse, &QPushButton::clicked, this, &swPreferencesDialog::PovrayExecutableBrowseClicked);
	connect(ui->lgeoPathBrowse, &QPushButton::clicked, this, &swPreferencesDialog::LgeoPathBrowseClicked);
	connect(ui->ColorTheme, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &swPreferencesDialog::ColorThemeCurrentIndexChanged);
	connect(ui->AutomateEdgeColor, &QCheckBox::toggled, this, &swPreferencesDialog::AutomateEdgeColorToggled);
	connect(ui->EdgeContrastSlider, &QSlider::valueChanged, this, &swPreferencesDialog::EdgeColorSliderValueChanged);
	connect(ui->EdgeSaturationSlider, &QSlider::valueChanged, this, &swPreferencesDialog::EdgeColorSliderValueChanged);
	connect(ui->BlenderAddonSettingsButton, &QPushButton::clicked, this, &swPreferencesDialog::BlenderAddonSettingsButtonClicked);
	connect(ui->studStyleCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &swPreferencesDialog::StudStyleComboCurrentIndexChanged);
	connect(ui->antiAliasing, &QCheckBox::toggled, this, &swPreferencesDialog::AntiAliasingToggled);
	connect(ui->edgeLines, &QCheckBox::toggled, this, &swPreferencesDialog::EdgeLinesToggled);
	connect(ui->ConditionalLinesCheckBox, &QCheckBox::toggled, this, &swPreferencesDialog::ConditionalLinesCheckBoxToggled);
	connect(ui->LineWidthSlider, QOverload<int>::of(&QSlider::valueChanged), this, &swPreferencesDialog::LineWidthSliderValueChanged);

	for (swRenderTarget Target : { swRenderTarget::Preview, swRenderTarget::Parts, swRenderTarget::MinifigureBuilder })
	{
		const swRenderTargetWidgets Widgets = GetRenderTargetWidgets(Target);

		connect(Widgets.SyncRendering, &QCheckBox::toggled, this, &swPreferencesDialog::RenderTargetChanged);
		connect(Widgets.EdgeLines, &QCheckBox::toggled, this, &swPreferencesDialog::RenderTargetChanged);
		connect(Widgets.ConditionalLines, &QCheckBox::toggled, this, &swPreferencesDialog::RenderTargetChanged);
		connect(Widgets.LineWidthSlider, QOverload<int>::of(&QSlider::valueChanged), this, &swPreferencesDialog::RenderTargetChanged);
	}

	connect(ui->MeshLODSlider, QOverload<int>::of(&QSlider::valueChanged), this, &swPreferencesDialog::MeshLODSliderValueChanged);
	connect(ui->FadeSteps, &QCheckBox::toggled, this, &swPreferencesDialog::FadeStepsToggled);
	connect(ui->HighlightNewParts, &QCheckBox::toggled, this, &swPreferencesDialog::HighlightNewPartsToggled);
	connect(ui->gridStuds, &QCheckBox::toggled, this, &swPreferencesDialog::GridStudsToggled);
	connect(ui->gridLines, &QCheckBox::toggled, this, &swPreferencesDialog::GridLinesToggled);
	connect(ui->ViewSphereSizeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &swPreferencesDialog::ViewSphereSizeComboCurrentIndexChanged);
	connect(ui->PreviewViewSphereSizeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &swPreferencesDialog::PreviewViewSphereSizeComboCurrentIndexChanged);
	connect(ui->newCategory, &QPushButton::clicked, this, &swPreferencesDialog::NewCategoryClicked);
	connect(ui->editCategory, &QPushButton::clicked, this, &swPreferencesDialog::EditCategoryClicked);
	connect(ui->deleteCategory, &QPushButton::clicked, this, &swPreferencesDialog::DeleteCategoryClicked);
	connect(ui->importCategories, &QPushButton::clicked, this, &swPreferencesDialog::ImportCategoriesClicked);
	connect(ui->exportCategories, &QPushButton::clicked, this, &swPreferencesDialog::ExportCategoriesClicked);
	connect(ui->resetCategories, &QPushButton::clicked, this, &swPreferencesDialog::ResetCategoriesClicked);
	connect(ui->shortcutAssign, &QPushButton::clicked, this, &swPreferencesDialog::ShortcutAssignClicked);
	connect(ui->shortcutRemove, &QPushButton::clicked, this, &swPreferencesDialog::ShortcutRemoveClicked);
	connect(ui->shortcutsImport, &QPushButton::clicked, this, &swPreferencesDialog::ShortcutsImportClicked);
	connect(ui->shortcutsExport, &QPushButton::clicked, this, &swPreferencesDialog::ShortcutsExportClicked);
	connect(ui->shortcutsReset, &QPushButton::clicked, this, &swPreferencesDialog::ShortcutsResetClicked);
	connect(ui->KeyboardFilterEdit, &QLineEdit::textEdited, this, &swPreferencesDialog::KeyboardFilterEditTextEdited);
	connect(ui->MouseFilterEdit, &QLineEdit::textEdited, this, &swPreferencesDialog::MouseFilterEditTextEdited);
	connect(ui->mouseAssign, &QPushButton::clicked, this, &swPreferencesDialog::MouseAssignClicked);
	connect(ui->mouseRemove, &QPushButton::clicked, this, &swPreferencesDialog::MouseRemoveClicked);
	connect(ui->MouseImportButton, &QPushButton::clicked, this, &swPreferencesDialog::MouseImportButtonClicked);
	connect(ui->MouseExportButton, &QPushButton::clicked, this, &swPreferencesDialog::MouseExportButtonClicked);
	connect(ui->mouseReset, &QPushButton::clicked, this, &swPreferencesDialog::MouseResetClicked);

	connect(ui->BackgroundSolidColorButton, &QToolButton::clicked, this, &swPreferencesDialog::ColorButtonClicked);
	connect(ui->ActiveViewColorButton, &QToolButton::clicked, this, &swPreferencesDialog::ColorButtonClicked);
	connect(ui->InactiveViewColorButton, &QToolButton::clicked, this, &swPreferencesDialog::ColorButtonClicked);
	connect(ui->TextHaloColorButton, &QToolButton::clicked, this, &swPreferencesDialog::ColorButtonClicked);
	connect(ui->TextColorButton, &QToolButton::clicked, this, &swPreferencesDialog::ColorButtonClicked);
	connect(ui->MarqueeBorderColorButton, &QToolButton::clicked, this, &swPreferencesDialog::ColorButtonClicked);
	connect(ui->MarqueeFillColorButton, &QToolButton::clicked, this, &swPreferencesDialog::ColorButtonClicked);
	connect(ui->OverlayColorButton, &QToolButton::clicked, this, &swPreferencesDialog::ColorButtonClicked);
	connect(ui->FadeStepsColor, &QToolButton::clicked, this, &swPreferencesDialog::ColorButtonClicked);
	connect(ui->HighlightNewPartsColor, &QToolButton::clicked, this, &swPreferencesDialog::ColorButtonClicked);
	connect(ui->gridStudColor, &QToolButton::clicked, this, &swPreferencesDialog::ColorButtonClicked);
	connect(ui->gridLineColor, &QToolButton::clicked, this, &swPreferencesDialog::ColorButtonClicked);
	connect(ui->ViewSphereColorButton, &QToolButton::clicked, this, &swPreferencesDialog::ColorButtonClicked);
	connect(ui->ViewSphereTextColorButton, &QToolButton::clicked, this, &swPreferencesDialog::ColorButtonClicked);
	connect(ui->ViewSphereHighlightColorButton, &QToolButton::clicked, this, &swPreferencesDialog::ColorButtonClicked);
	connect(ui->AxisXColorButton, &QToolButton::clicked, this, &swPreferencesDialog::ColorButtonClicked);
	connect(ui->AxisYColorButton, &QToolButton::clicked, this, &swPreferencesDialog::ColorButtonClicked);
	connect(ui->AxisZColorButton, &QToolButton::clicked, this, &swPreferencesDialog::ColorButtonClicked);
	connect(ui->ObjectSelectedColorButton, &QToolButton::clicked, this, &swPreferencesDialog::ColorButtonClicked);
	connect(ui->ObjectFocusedColorButton, &QToolButton::clicked, this, &swPreferencesDialog::ColorButtonClicked);
	connect(ui->CameraColorButton, &QToolButton::clicked, this, &swPreferencesDialog::ColorButtonClicked);
	connect(ui->LightColorButton, &QToolButton::clicked, this, &swPreferencesDialog::ColorButtonClicked);
	connect(ui->ControlPointColorButton, &QToolButton::clicked, this, &swPreferencesDialog::ColorButtonClicked);
	connect(ui->ControlPointFocusedColorButton, &QToolButton::clicked, this, &swPreferencesDialog::ColorButtonClicked);
	connect(ui->categoriesTree, &QTreeWidget::itemSelectionChanged, this, &swPreferencesDialog::UpdateParts);
	connect(ui->categoriesTree->model(), &QAbstractItemModel::rowsInserted, this, &swPreferencesDialog::CategoriesDropped);
	ui->shortcutEdit->installEventFilter(this);
	ui->MouseShortcutEdit->installEventFilter(this);
	connect(ui->commandList, &QTreeWidget::currentItemChanged, this, &swPreferencesDialog::CommandChanged);
	connect(ui->mouseTree, &QTreeWidget::currentItemChanged, this, &swPreferencesDialog::MouseTreeItemChanged);
	connect(ui->HighContrastButton, &QToolButton::clicked, this, &swPreferencesDialog::AutomateEdgeColor);

	ui->partsLibrary->setText(mOptions->LibraryPath);
	ui->ColorConfigEdit->setText(mOptions->ColorConfigPath);
	ui->MinifigureBuilderSettingsEdit->setText(mOptions->MinifigureBuilderSettingsPath);
	ui->povrayExecutable->setText(mOptions->POVRayPath);
	ui->lgeoPath->setText(mOptions->LGEOPath);
	ui->authorName->setText(mOptions->DefaultAuthor);

	connect(ui->mouseSensitivity, &QSlider::valueChanged, ui->MouseSensitivityLabel, [this](int Value)
	{
		ui->MouseSensitivityLabel->setText(locale().toString(Value));
	});
	ui->mouseSensitivity->setValue(mOptions->Preferences.mMouseSensitivity);
	ui->MouseSensitivityLabel->setText(locale().toString(ui->mouseSensitivity->value()));

	const bool ColorThemeBlocked = ui->ColorTheme->blockSignals(true);
	ui->ColorTheme->setCurrentIndex(static_cast<int>(mOptions->Preferences.mColorTheme));
	ui->ColorTheme->blockSignals(ColorThemeBlocked);
	for (unsigned int LanguageIdx = 0; LanguageIdx < SW_ARRAY_COUNT(gLanguageLocales); LanguageIdx++)
	{
		if (mOptions->Language == gLanguageLocales[LanguageIdx])
		{
			ui->Language->setCurrentIndex(LanguageIdx);
			break;
		}
	}
	ui->fixedDirectionKeys->setChecked(mOptions->Preferences.mFixedAxes);
	ui->autoLoadMostRecent->setChecked(mOptions->Preferences.mAutoLoadMostRecent);
	ui->RestoreTabLayout->setChecked(mOptions->Preferences.mRestoreTabLayout);
	ui->FirstPartAtOrigin->setChecked(mOptions->Preferences.mFirstPartAtOrigin);
	ui->PartsSnapToTiles->setChecked(mOptions->Preferences.mPartsSnapToTiles);
	ui->InsertColorMode->setCurrentIndex(static_cast<int>(mOptions->Preferences.mInsertColorMode));
	ui->WatchFileChanges->setChecked(mOptions->Preferences.mWatchFileChanges);
	ui->LoadPartsBeforeShowing->setChecked(mOptions->Preferences.mLoadPartsBeforeShowing);
	ui->MigrateMetaCommands->setChecked(mOptions->Preferences.mMigrateMetaCommands);
	ui->AutomateEdgeColor->setChecked(mOptions->Preferences.mAutomateEdgeColor);
	const int EdgeContrast = qRound(mOptions->Preferences.mPartEdgeContrast * 100.0f);
	const int EdgeSaturation = qRound(mOptions->Preferences.mPartColorValueLDIndex * 100.0f);
	ui->EdgeContrastSlider->setValue(EdgeContrast);
	ui->EdgeSaturationSlider->setValue(EdgeSaturation);

	ui->antiAliasing->setChecked(mOptions->AASamples != 1);
	if (mOptions->AASamples == 8)
		ui->antiAliasingSamples->setCurrentIndex(2);
	else if (mOptions->AASamples == 4)
		ui->antiAliasingSamples->setCurrentIndex(1);
	else
		ui->antiAliasingSamples->setCurrentIndex(0);
	ui->edgeLines->setChecked(mOptions->Preferences.mDrawEdgeLines);
	ui->ConditionalLinesCheckBox->setChecked(mOptions->Preferences.mDrawConditionalLines);

	swViewWidget* Widget = gMainWindow->GetActiveView()->GetWidget();
	QOpenGLContext* Context = Widget->context();
	QOpenGLFunctions* Functions = Context->functions();

#ifndef SW_OPENGLES
	if (QSurfaceFormat::defaultFormat().samples() > 1)
	{
		Functions->glGetFloatv(GL_SMOOTH_LINE_WIDTH_RANGE, mLineWidthRange);
		Functions->glGetFloatv(GL_SMOOTH_LINE_WIDTH_GRANULARITY, &mLineWidthGranularity);
	}
	else
#endif
	{
		Functions->glGetFloatv(GL_ALIASED_LINE_WIDTH_RANGE, mLineWidthRange);
		mLineWidthGranularity = 1.0f;
	}

	mLineWidthRange[1] = qMin(mLineWidthRange[1], 10.0f);
	mLineWidthGranularity = qMax(mLineWidthGranularity, 0.25f);

	ui->LineWidthSlider->setRange(0, (mLineWidthRange[1] - mLineWidthRange[0]) / mLineWidthGranularity);
	ui->LineWidthSlider->setValue((mOptions->Preferences.mLineWidth - mLineWidthRange[0]) / mLineWidthGranularity);

	for (swRenderTarget Target : { swRenderTarget::Preview, swRenderTarget::Parts, swRenderTarget::MinifigureBuilder })
	{
		const swRenderTargetWidgets Widgets = GetRenderTargetWidgets(Target);
		const swTargetRenderSettings& TargetSettings = mOptions->Preferences.GetTargetRenderSettings(Target);

		Widgets.SyncRendering->setChecked(TargetSettings.SyncRendering);
		Widgets.ShadingMode->setCurrentIndex(static_cast<int>(TargetSettings.Settings.ShadingMode));
		Widgets.EdgeLines->setChecked(TargetSettings.Settings.DrawEdgeLines);
		Widgets.ConditionalLines->setChecked(TargetSettings.Settings.DrawConditionalLines);
		Widgets.LineWidthSlider->setRange(0, (mLineWidthRange[1] - mLineWidthRange[0]) / mLineWidthGranularity);
		Widgets.LineWidthSlider->setValue((TargetSettings.Settings.LineWidth - mLineWidthRange[0]) / mLineWidthGranularity);
	}

	ui->MeshLOD->setChecked(mOptions->Preferences.mAllowLOD);

	ui->MeshLODSlider->setRange(0, 1500.0f / mMeshLODMultiplier);
	ui->MeshLODSlider->setValue(mOptions->Preferences.mMeshLODDistance / mMeshLODMultiplier);

	ui->FadeSteps->setChecked(mOptions->Preferences.mFadeSteps);
	ui->HighlightNewParts->setChecked(mOptions->Preferences.mHighlightNewParts);
	ui->gridStuds->setChecked(mOptions->Preferences.mDrawGridStuds);
	ui->gridLines->setChecked(mOptions->Preferences.mDrawGridLines);
	ui->gridLineSpacing->setValidator(new QIntValidator(1, SW_MAXIMUM_GRID_LINE_SPACING, ui->gridLineSpacing));
	ui->gridLineSpacing->setText(swFormatIntegerLocalized(mOptions->Preferences.mGridLineSpacing, ui->gridLineSpacing->validator()->locale()));
	ui->GridOriginCheckBox->setChecked(mOptions->Preferences.mDrawGridOrigin);
	ui->GridHeightXZCheckBox->setChecked(mOptions->Preferences.mDrawGridHeightXZ);
	ui->GridHeightYZCheckBox->setChecked(mOptions->Preferences.mDrawGridHeightYZ);
	ui->GridHeightBehindCheckBox->setChecked(mOptions->Preferences.mGridHeightBehindModel);
	ui->AxisIconCheckBox->setChecked(mOptions->Preferences.mDrawAxes);
	ui->FollowStepViewsCheckBox->setChecked(mOptions->Preferences.mFollowStepViews);

	ui->AxisIconLocationCombo->setCurrentIndex((int)mOptions->Preferences.mAxisIconLocation);

	ui->ViewSphereLocationCombo->setCurrentIndex((int)mOptions->Preferences.mViewSphereLocation);

	if (mOptions->Preferences.mViewSphereEnabled)
	{
		switch (mOptions->Preferences.mViewSphereSize)
		{
		case 200:
			ui->ViewSphereSizeCombo->setCurrentIndex(3);
			break;
		case 100:
			ui->ViewSphereSizeCombo->setCurrentIndex(2);
			break;
		case 50:
			ui->ViewSphereSizeCombo->setCurrentIndex(1);
			break;
		default:
			ui->ViewSphereSizeCombo->setCurrentIndex(0);
			break;
		}
	}
	else
		ui->ViewSphereSizeCombo->setCurrentIndex(0);

	ui->PreviewAxisIconCheckBox->setChecked(mOptions->Preferences.mDrawPreviewAxis);
	ui->PreviewSquareViewportCheckBox->setChecked(mOptions->Preferences.mPreviewSquareViewport);

	ui->PreviewViewSphereLocationCombo->setCurrentIndex((int)mOptions->Preferences.mPreviewViewSphereLocation);

	if (mOptions->Preferences.mPreviewViewSphereEnabled)
	{
		switch (mOptions->Preferences.mPreviewViewSphereSize)
		{
		case 100:
			ui->PreviewViewSphereSizeCombo->setCurrentIndex(3);
			break;
		case 75:
			ui->PreviewViewSphereSizeCombo->setCurrentIndex(2);
			break;
		case 50:
			ui->PreviewViewSphereSizeCombo->setCurrentIndex(1);
			break;
		default:
			ui->PreviewViewSphereSizeCombo->setCurrentIndex(0);
			break;
		}
	}
	else
		ui->PreviewViewSphereSizeCombo->setCurrentIndex(0);

	ui->studStyleCombo->setCurrentIndex(static_cast<int>(mOptions->StudStyle));

	ui->ShadingMode->setCurrentIndex(static_cast<int>(mOptions->Preferences.mShadingMode));
	ui->LightingStyle->setCurrentIndex(static_cast<int>(mOptions->Preferences.mLightingStyle));

	if (!swContext::SupportsDefaultLights())
	{
		QStandardItemModel* ShadingModel = qobject_cast<QStandardItemModel*>(ui->ShadingMode->model());
		QStandardItem* DefaultLightsItem = ShadingModel->item(static_cast<int>(swShadingMode::DefaultLights));
		DefaultLightsItem->setEnabled(false);

		const QString Message = tr("Default Lights is unavailable. Flat shading is used instead.");
		DefaultLightsItem->setToolTip(Message);
		ui->ShadingMode->setToolTip(Message);

		ui->LightingStyle->setEnabled(false);
		ui->LightingStyleLabel->setEnabled(false);
		ui->LightingStyle->setToolTip(Message);

		for (swRenderTarget Target : { swRenderTarget::Preview, swRenderTarget::Parts, swRenderTarget::MinifigureBuilder })
		{
			QComboBox* TargetShadingMode = GetRenderTargetWidgets(Target).ShadingMode;
			QStandardItemModel* TargetShadingModel = qobject_cast<QStandardItemModel*>(TargetShadingMode->model());
			QStandardItem* TargetDefaultLightsItem = TargetShadingModel->item(static_cast<int>(swShadingMode::DefaultLights));
			TargetDefaultLightsItem->setEnabled(false);
			TargetDefaultLightsItem->setToolTip(Message);
			TargetShadingMode->setToolTip(Message);
		}
	}

	if (!swContext::SupportsMaterial(swMaterialType::UnlitViewSphere))
	{
		const std::array<QWidget*, 7> Controls =
		{
			ui->ViewSphereSizeCombo, ui->ViewSphereLocationCombo,
			ui->PreviewViewSphereSizeCombo, ui->PreviewViewSphereLocationCombo,
			ui->ViewSphereColorButton, ui->ViewSphereTextColorButton, ui->ViewSphereHighlightColorButton
		};
		const QString Message = tr("View sphere rendering is unavailable.");

		for (QWidget* Control : Controls)
		{
			Control->setEnabled(false);
			Control->setToolTip(Message);
		}
	}

	bool OutlineSupported = false;

	{
		swScopedGLContextState GLState;
		swView* View = Widget->GetView();
		View->MakeCurrent();
		OutlineSupported = View->PrepareOutlineFramebuffers();
	}

	if (!OutlineSupported)
	{
		mOptions->Preferences.mHighlightRenderMode = swHighlightRenderMode::Tint;
		swGetPreferences().mHighlightRenderMode = swHighlightRenderMode::Tint;
		ui->HighlightRenderMode->setToolTip(tr("Outline rendering is unavailable; the Tint and Edges style will be used."));
		ui->HighlightRenderModeLabel->setToolTip(ui->HighlightRenderMode->toolTip());
	}

	ui->HighlightRenderMode->setCurrentIndex(static_cast<int>(mOptions->Preferences.mHighlightRenderMode));
	ui->HighlightRenderMode->setEnabled(OutlineSupported);
	ui->HighlightRenderModeLabel->setEnabled(OutlineSupported);

	ui->SelectionOutlineWidth->setRange(SW_SELECTION_OUTLINE_WIDTH_MINIMUM, SW_SELECTION_OUTLINE_WIDTH_MAXIMUM);
	ui->SelectionOutlineWidth->setValue(mOptions->Preferences.mSelectionOutlineWidth);

	// The width only applies to the outline style.
	auto UpdateSelectionOutlineWidth = [this, OutlineSupported]()
	{
		const bool Enable = OutlineSupported && ui->HighlightRenderMode->currentIndex() == static_cast<int>(swHighlightRenderMode::Outline);

		ui->SelectionOutlineWidth->setEnabled(Enable);
		ui->SelectionOutlineWidthLabel->setEnabled(Enable);
	};

	connect(ui->HighlightRenderMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, UpdateSelectionOutlineWidth);
	UpdateSelectionOutlineWidth();

	auto SetButtonPixmap = [](quint32 Color, QToolButton* Button)
	{
		// color buttons show their swatch beside its name like the instructions colors
		Button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
		swUpdateColorButton(Button, QColor(SW_RGBA_RED(Color), SW_RGBA_GREEN(Color), SW_RGBA_BLUE(Color), SW_RGBA_ALPHA(Color)));
	};

	SetButtonPixmap(mOptions->Preferences.mBackgroundSolidColor, ui->BackgroundSolidColorButton);
	SetButtonPixmap(mOptions->Preferences.mTextColor, ui->TextColorButton);
	SetButtonPixmap(mOptions->Preferences.mTextHaloColor, ui->TextHaloColorButton);
	SetButtonPixmap(mOptions->Preferences.mMarqueeBorderColor, ui->MarqueeBorderColorButton);
	SetButtonPixmap(mOptions->Preferences.mMarqueeFillColor, ui->MarqueeFillColorButton);
	SetButtonPixmap(mOptions->Preferences.mOverlayColor, ui->OverlayColorButton);
	SetButtonPixmap(mOptions->Preferences.mActiveViewColor, ui->ActiveViewColorButton);
	SetButtonPixmap(mOptions->Preferences.mInactiveViewColor, ui->InactiveViewColorButton);
	SetButtonPixmap(mOptions->Preferences.mFadeStepsColor, ui->FadeStepsColor);
	SetButtonPixmap(mOptions->Preferences.mHighlightNewPartsColor, ui->HighlightNewPartsColor);
	SetButtonPixmap(mOptions->Preferences.mGridStudColor, ui->gridStudColor);
	SetButtonPixmap(mOptions->Preferences.mGridLineColor, ui->gridLineColor);
	SetButtonPixmap(mOptions->Preferences.mViewSphereColor, ui->ViewSphereColorButton);
	SetButtonPixmap(mOptions->Preferences.mViewSphereTextColor, ui->ViewSphereTextColorButton);
	SetButtonPixmap(mOptions->Preferences.mViewSphereHighlightColor, ui->ViewSphereHighlightColorButton);
	SetButtonPixmap(mOptions->Preferences.mAxisXColor, ui->AxisXColorButton);
	SetButtonPixmap(mOptions->Preferences.mAxisYColor, ui->AxisYColorButton);
	SetButtonPixmap(mOptions->Preferences.mAxisZColor, ui->AxisZColorButton);
	SetButtonPixmap(mOptions->Preferences.mObjectSelectedColor, ui->ObjectSelectedColorButton);
	SetButtonPixmap(mOptions->Preferences.mObjectFocusedColor, ui->ObjectFocusedColorButton);
	SetButtonPixmap(mOptions->Preferences.mCameraColor, ui->CameraColorButton);
	SetButtonPixmap(mOptions->Preferences.mLightColor, ui->LightColorButton);
	SetButtonPixmap(mOptions->Preferences.mControlPointColor, ui->ControlPointColorButton);
	SetButtonPixmap(mOptions->Preferences.mControlPointFocusedColor, ui->ControlPointFocusedColorButton);

	StudStyleComboCurrentIndexChanged(ui->studStyleCombo->currentIndex());
	AntiAliasingToggled();
	AutomateEdgeColorToggled();
	EdgeColorSliderValueChanged();
	EdgeLinesToggled();
	LineWidthSliderValueChanged();
	RenderTargetChanged();
	MeshLODSliderValueChanged();
	FadeStepsToggled();
	HighlightNewPartsToggled();
	GridStudsToggled();
	GridLinesToggled();
	ViewSphereSizeComboCurrentIndexChanged(ui->ViewSphereSizeCombo->currentIndex());
	PreviewViewSphereSizeComboCurrentIndexChanged(ui->PreviewViewSphereSizeCombo->currentIndex());

	UpdateCategories();
	ui->categoriesTree->setCurrentItem(ui->categoriesTree->topLevelItem(0));

	// the categories list keeps its snapped width when the dialog is resized, the parts list takes the rest
	ui->splitter->setStretchFactor(0, 0);
	ui->splitter->setStretchFactor(1, 1);
	connect(ui->tabWidget, &QTabWidget::currentChanged, this, &swPreferencesDialog::SnapCategoriesTree);

	UpdateCommandList();
	new swTreeWidgetColumnStretcher(ui->commandList, 0);
	CommandChanged(nullptr);

	UpdateMouseTree();
	new swTreeWidgetColumnStretcher(ui->mouseTree, 0);
	MouseTreeItemChanged(nullptr);

	for (QLabel* Label : findChildren<QLabel*>())
		if (QWidget* Control = Label->buddy())
			Label->setToolTip(Control->toolTip());

	CreateInstructionsTab();
}

swPreferencesDialog::~swPreferencesDialog()
{
	delete ui;
}

void swPreferencesDialog::showEvent(QShowEvent* Event)
{
	QDialog::showEvent(Event);

	if (Event->spontaneous())
		return;

	// the browse buttons span the two parts library buttons so every path field ends at the same place
	const int BrowseWidth = ui->partsLibraryBrowse->sizeHint().width() + ui->horizontalLayout_3->spacing() + ui->partsArchiveBrowse->sizeHint().width();

	for (QToolButton* Button : ui->CustomPathsGroup->findChildren<QToolButton*>())
		if (Button != ui->partsLibraryBrowse && Button != ui->partsArchiveBrowse)
			Button->setFixedWidth(BrowseWidth);

	// the controls only report their full width once shown, the columns line up after that
	swAlignPreferencesColumns(ui->tabWidget);
}

void swPreferencesDialog::accept()
{
	bool Ok;
	int GridLineSpacing = ui->gridLineSpacing->validator()->locale().toInt(ui->gridLineSpacing->text(), &Ok);
	if (!Ok || !ui->gridLineSpacing->hasAcceptableInput())
	{
		QMessageBox::information(this, windowTitle(), tr("Grid spacing must be between 1 and %1 studs.").arg(QString::number(SW_MAXIMUM_GRID_LINE_SPACING)));
		return;
	}

	mOptions->LibraryPath = ui->partsLibrary->text();
	mOptions->MinifigureBuilderSettingsPath = ui->MinifigureBuilderSettingsEdit->text();
	mOptions->ColorConfigPath = ui->ColorConfigEdit->text();
	mOptions->POVRayPath = ui->povrayExecutable->text();
	mOptions->LGEOPath = ui->lgeoPath->text();
	mOptions->DefaultAuthor = ui->authorName->text();
	mOptions->Preferences.mMouseSensitivity = ui->mouseSensitivity->value();
	mOptions->Preferences.mColorTheme = static_cast<swColorTheme>(ui->ColorTheme->currentIndex());
	mOptions->Preferences.mAutomateEdgeColor = ui->AutomateEdgeColor->isChecked();

	int Language = ui->Language->currentIndex();
	if (Language < 0 || Language >= static_cast<int>(SW_ARRAY_COUNT(gLanguageLocales)))
		Language = 0;
	mOptions->Language = gLanguageLocales[Language];

	mOptions->Preferences.mFixedAxes = ui->fixedDirectionKeys->isChecked();
	mOptions->Preferences.mAutoLoadMostRecent = ui->autoLoadMostRecent->isChecked();
	mOptions->Preferences.mRestoreTabLayout = ui->RestoreTabLayout->isChecked();
	mOptions->Preferences.mFirstPartAtOrigin = ui->FirstPartAtOrigin->isChecked();
	mOptions->Preferences.mPartsSnapToTiles = ui->PartsSnapToTiles->isChecked();
	mOptions->Preferences.mInsertColorMode = static_cast<swInsertColorMode>(ui->InsertColorMode->currentIndex());
	mOptions->Preferences.mWatchFileChanges = ui->WatchFileChanges->isChecked();
	mOptions->Preferences.mLoadPartsBeforeShowing = ui->LoadPartsBeforeShowing->isChecked();
	mOptions->Preferences.mMigrateMetaCommands = ui->MigrateMetaCommands->isChecked();

	if (!ui->antiAliasing->isChecked())
		mOptions->AASamples = 1;
	else if (ui->antiAliasingSamples->currentIndex() == 2)
		mOptions->AASamples = 8;
	else if (ui->antiAliasingSamples->currentIndex() == 1)
		mOptions->AASamples = 4;
	else
		mOptions->AASamples = 2;

	mOptions->Preferences.mDrawEdgeLines = ui->edgeLines->isChecked();
	mOptions->Preferences.mDrawConditionalLines = ui->ConditionalLinesCheckBox->isChecked();
	mOptions->Preferences.mLineWidth = mLineWidthRange[0] + static_cast<float>(ui->LineWidthSlider->value()) * mLineWidthGranularity;

	for (swRenderTarget Target : { swRenderTarget::Preview, swRenderTarget::Parts, swRenderTarget::MinifigureBuilder })
	{
		const swRenderTargetWidgets Widgets = GetRenderTargetWidgets(Target);
		swTargetRenderSettings& TargetSettings = mOptions->Preferences.GetTargetRenderSettings(Target);

		TargetSettings.SyncRendering = Widgets.SyncRendering->isChecked();
		TargetSettings.Settings.ShadingMode = static_cast<swShadingMode>(Widgets.ShadingMode->currentIndex());
		TargetSettings.Settings.DrawEdgeLines = Widgets.EdgeLines->isChecked();
		TargetSettings.Settings.DrawConditionalLines = Widgets.ConditionalLines->isChecked();
		TargetSettings.Settings.LineWidth = mLineWidthRange[0] + static_cast<float>(Widgets.LineWidthSlider->value()) * mLineWidthGranularity;
	}

	mOptions->Preferences.mAllowLOD = ui->MeshLOD->isChecked();
	mOptions->Preferences.mMeshLODDistance = ui->MeshLODSlider->value() * mMeshLODMultiplier;
	mOptions->Preferences.mFadeSteps = ui->FadeSteps->isChecked();
	mOptions->Preferences.mHighlightNewParts = ui->HighlightNewParts->isChecked();

	mOptions->Preferences.mDrawGridStuds = ui->gridStuds->isChecked();
	mOptions->Preferences.mDrawGridLines = ui->gridLines->isChecked();
	mOptions->Preferences.mGridLineSpacing = GridLineSpacing;
	mOptions->Preferences.mDrawGridOrigin = ui->GridOriginCheckBox->isChecked();
	mOptions->Preferences.mDrawGridHeightXZ = ui->GridHeightXZCheckBox->isChecked();
	mOptions->Preferences.mDrawGridHeightYZ = ui->GridHeightYZCheckBox->isChecked();
	mOptions->Preferences.mGridHeightBehindModel = ui->GridHeightBehindCheckBox->isChecked();

	mOptions->Preferences.mDrawAxes = ui->AxisIconCheckBox->isChecked();
	mOptions->Preferences.mFollowStepViews = ui->FollowStepViewsCheckBox->isChecked();
	mOptions->Preferences.mAxisIconLocation = (swAxisIconLocation)ui->AxisIconLocationCombo->currentIndex();
	mOptions->Preferences.mViewSphereEnabled = ui->ViewSphereSizeCombo->currentIndex() > 0;
	mOptions->Preferences.mViewSphereLocation = (swViewSphereLocation)ui->ViewSphereLocationCombo->currentIndex();

	switch (ui->ViewSphereSizeCombo->currentIndex())
	{
	case 3:
		mOptions->Preferences.mViewSphereSize = 200;
		break;
	case 2:
		mOptions->Preferences.mViewSphereSize = 100;
		break;
	case 1:
		mOptions->Preferences.mViewSphereSize = 50;
		break;
	}

	mOptions->Preferences.mShadingMode = (swShadingMode)ui->ShadingMode->currentIndex();
	mOptions->Preferences.mLightingStyle = static_cast<swLightingStyle>(ui->LightingStyle->currentIndex());
	mOptions->Preferences.mHighlightRenderMode = static_cast<swHighlightRenderMode>(ui->HighlightRenderMode->currentIndex());
	mOptions->Preferences.mSelectionOutlineWidth = ui->SelectionOutlineWidth->value();

	mOptions->StudStyle = static_cast<swStudStyle>(ui->studStyleCombo->currentIndex());

	mOptions->Preferences.mDrawPreviewAxis = ui->PreviewAxisIconCheckBox->isChecked();
	mOptions->Preferences.mPreviewSquareViewport = ui->PreviewSquareViewportCheckBox->isChecked();
	mOptions->Preferences.mPreviewViewSphereEnabled = ui->PreviewViewSphereSizeCombo->currentIndex() > 0;
	mOptions->Preferences.mPreviewViewSphereLocation = (swViewSphereLocation)ui->PreviewViewSphereLocationCombo->currentIndex();

	switch (ui->PreviewViewSphereSizeCombo->currentIndex())
	{
	case 3:
		mOptions->Preferences.mPreviewViewSphereSize = 100;
		break;
	case 2:
		mOptions->Preferences.mPreviewViewSphereSize = 75;
		break;
	case 1:
		mOptions->Preferences.mPreviewViewSphereSize = 50;
		break;
	}

	swInstructionsPreferences& Instructions = mOptions->Instructions;
	Instructions.GridSize = static_cast<float>(mInstructionsGridSize->value()) * swGetPageUnitScale(static_cast<swInstructionsPageUnits>(Instructions.MarginUnits));
	Instructions.GridDivisions = mInstructionsGridDivisions->value();
	Instructions.ShowGrid = mInstructionsShowGrid->isChecked();
	Instructions.SnapToGrid = mInstructionsSnapToGrid->isChecked();
	Instructions.GridWithinMargins = mInstructionsGridScope->currentIndex() == 0;
	Instructions.NewModelDefaults = mInstructionsNewModelDefaults->isChecked();
	Instructions.ShowMargins = mInstructionsShowMargins->isChecked();
	Instructions.EditorHighlight = mInstructionsEditorHighlight->isChecked();
	Instructions.EditorNumbers = mInstructionsEditorNumbers->isChecked();
	Instructions.EditorWrap = mInstructionsEditorWrap->isChecked();
	Instructions.EditorMarkers = mInstructionsEditorMarkers->isChecked();

	QDialog::accept();
}

void swPreferencesDialog::PartsLibraryBrowseClicked()
{
	QString Result = QFileDialog::getExistingDirectory(this, tr("Select Parts Library Folder"), ui->partsLibrary->text());

	if (!Result.isEmpty())
		ui->partsLibrary->setText(QDir::toNativeSeparators(Result));
}

void swPreferencesDialog::PartsArchiveBrowseClicked()
{
	QString Result = QFileDialog::getOpenFileName(this, tr("Select Parts Library Archive"), ui->partsLibrary->text(), tr("Supported Archives (*.zip *.bin);;All Files (*.*)"));

	if (!Result.isEmpty())
		ui->partsLibrary->setText(QDir::toNativeSeparators(Result));
}

void swPreferencesDialog::BlenderAddonSettingsButtonClicked()
{
#ifndef SW_DISABLE_BLENDER
	int Width = 1280;
	int Height = 720;
	double Scale = 1.0f;

	swBlenderPreferencesDialog::GetBlenderPreferences(
		Width,
		Height,
		Scale,
		this);
#endif // !SW_BLENDER_DISABLED
}

void swPreferencesDialog::ColorConfigBrowseButtonClicked()
{
	QString Result = QFileDialog::getOpenFileName(this, tr("Select Color Configuration File"), ui->ColorConfigEdit->text(), tr("Settings Files (*.ldr);;All Files (*.*)"));

	if (!Result.isEmpty())
		ui->ColorConfigEdit->setText(QDir::toNativeSeparators(Result));
}

void swPreferencesDialog::MinifigureBuilderSettingsBrowseButtonClicked()
{
	QString Result = QFileDialog::getOpenFileName(this, tr("Select Minifigure Builder Settings File"), ui->MinifigureBuilderSettingsEdit->text(), tr("Settings Files (*.ini);;All Files (*.*)"));

	if (!Result.isEmpty())
		ui->MinifigureBuilderSettingsEdit->setText(QDir::toNativeSeparators(Result));
}

void swPreferencesDialog::PovrayExecutableBrowseClicked()
{
#ifdef Q_OS_WIN
	QString filter(tr("Executable Files (*.exe);;All Files (*.*)"));
#else
	QString filter(tr("All Files (*.*)"));
#endif

	QString Result = QFileDialog::getOpenFileName(this, tr("Select POV-Ray Executable"), ui->povrayExecutable->text(), filter);

	if (!Result.isEmpty())
		ui->povrayExecutable->setText(QDir::toNativeSeparators(Result));
}

void swPreferencesDialog::LgeoPathBrowseClicked()
{
	QString Result = QFileDialog::getExistingDirectory(this, tr("Open LGEO Folder"), ui->lgeoPath->text());

	if (!Result.isEmpty())
		ui->lgeoPath->setText(QDir::toNativeSeparators(Result));
}

void swPreferencesDialog::ColorThemeCurrentIndexChanged(int Index)
{
	Q_UNUSED(Index);

	if (QMessageBox::question(this, tr("Reset Colors"), tr("Would you like to also reset the interface colors to match the color theme?")) == QMessageBox::Yes)
		mOptions->Preferences.SetInterfaceColors(static_cast<swColorTheme>(ui->ColorTheme->currentIndex()));
}

void swPreferencesDialog::ColorButtonClicked()
{
	QObject* Button = sender();
	QString Title;
	quint32* Color = nullptr;
	QColorDialog::ColorDialogOptions DialogOptions;

	if (Button == ui->BackgroundSolidColorButton)
	{
		Color = &mOptions->Preferences.mBackgroundSolidColor;
		Title = tr("Select Background Color");
	}
	else if (Button == ui->TextColorButton)
	{
		Color = &mOptions->Preferences.mTextColor;
		Title = tr("Select Text Color");
	}
	else if (Button == ui->TextHaloColorButton)
	{
		Color = &mOptions->Preferences.mTextHaloColor;
		Title = tr("Select Text Halo Color");
		DialogOptions = QColorDialog::ShowAlphaChannel;
	}
	else if (Button == ui->MarqueeBorderColorButton)
	{
		Color = &mOptions->Preferences.mMarqueeBorderColor;
		Title = tr("Select Marquee Border Color");
	}
	else if (Button == ui->MarqueeFillColorButton)
	{
		Color = &mOptions->Preferences.mMarqueeFillColor;
		Title = tr("Select Marquee Fill Color");
		DialogOptions = QColorDialog::ShowAlphaChannel;
	}
	else if (Button == ui->OverlayColorButton)
	{
		Color = &mOptions->Preferences.mOverlayColor;
		Title = tr("Select Overlay Color");
	}
	else if (Button == ui->ActiveViewColorButton)
	{
		Color = &mOptions->Preferences.mActiveViewColor;
		Title = tr("Select Active View Color");
	}
	else if (Button == ui->InactiveViewColorButton)
	{
		Color = &mOptions->Preferences.mInactiveViewColor;
		Title = tr("Select Inactive View Color");
	}
	else if (Button == ui->FadeStepsColor)
	{
		Color = &mOptions->Preferences.mFadeStepsColor;
		Title = tr("Select Fade Color");
		DialogOptions = QColorDialog::ShowAlphaChannel;
	}
	else if (Button == ui->HighlightNewPartsColor)
	{
		Color = &mOptions->Preferences.mHighlightNewPartsColor;
		Title = tr("Select Highlight Color");
		DialogOptions = QColorDialog::ShowAlphaChannel;
	}
	else if (Button == ui->gridStudColor)
	{
		Color = &mOptions->Preferences.mGridStudColor;
		Title = tr("Select Grid Stud Color");
		DialogOptions = QColorDialog::ShowAlphaChannel;
	}
	else if (Button == ui->gridLineColor)
	{
		Color = &mOptions->Preferences.mGridLineColor;
		Title = tr("Select Grid Line Color");
	}
	else if (Button == ui->ViewSphereColorButton)
	{
		Color = &mOptions->Preferences.mViewSphereColor;
		Title = tr("Select View Sphere Color");
	}
	else if (Button == ui->ViewSphereTextColorButton)
	{
		Color = &mOptions->Preferences.mViewSphereTextColor;
		Title = tr("Select View Sphere Text Color");
	}
	else if (Button == ui->ViewSphereHighlightColorButton)
	{
		Color = &mOptions->Preferences.mViewSphereHighlightColor;
		Title = tr("Select View Sphere Highlight Color");
	}
	else if (Button == ui->AxisXColorButton)
	{
		Color = &mOptions->Preferences.mAxisXColor;
		Title = tr("Select X Axis Color");
	}
	else if (Button == ui->AxisYColorButton)
	{
		Color = &mOptions->Preferences.mAxisYColor;
		Title = tr("Select Y Axis Color");
	}
	else if (Button == ui->AxisZColorButton)
	{
		Color = &mOptions->Preferences.mAxisZColor;
		Title = tr("Select Z Axis Color");
	}
	else if (Button == ui->ObjectSelectedColorButton)
	{
		Color = &mOptions->Preferences.mObjectSelectedColor;
		Title = tr("Select Object Selected Color");
	}
	else if (Button == ui->ObjectFocusedColorButton)
	{
		Color = &mOptions->Preferences.mObjectFocusedColor;
		Title = tr("Select Object Focused Color");
	}
	else if (Button == ui->CameraColorButton)
	{
		Color = &mOptions->Preferences.mCameraColor;
		Title = tr("Select Camera Color");
	}
	else if (Button == ui->LightColorButton)
	{
		Color = &mOptions->Preferences.mLightColor;
		Title = tr("Select Light Color");
	}
	else if (Button == ui->ControlPointColorButton)
	{
		Color = &mOptions->Preferences.mControlPointColor;
		Title = tr("Select Control Point Color");
		DialogOptions = QColorDialog::ShowAlphaChannel;
	}
	else if (Button == ui->ControlPointFocusedColorButton)
	{
		Color = &mOptions->Preferences.mControlPointFocusedColor;
		Title = tr("Select Control Point Focused Color");
		DialogOptions = QColorDialog::ShowAlphaChannel;
	}
	else
		return;

	QColor OldColor = QColor(SW_RGBA_RED(*Color), SW_RGBA_GREEN(*Color), SW_RGBA_BLUE(*Color), SW_RGBA_ALPHA(*Color));
	QColor NewColor = QColorDialog::getColor(OldColor, this, Title, DialogOptions);

	if (NewColor == OldColor || !NewColor.isValid())
		return;

	*Color = SW_RGBA(NewColor.red(), NewColor.green(), NewColor.blue(), NewColor.alpha());

	swUpdateColorButton((QToolButton*)Button, NewColor);
}

void swPreferencesDialog::AntiAliasingToggled()
{
	ui->antiAliasingSamples->setEnabled(ui->antiAliasing->isChecked());
}

void swPreferencesDialog::EdgeLinesToggled()
{
	const bool Enable = ui->edgeLines->isChecked() || ui->ConditionalLinesCheckBox->isChecked();

	ui->LineWidthSlider->setEnabled(Enable);
	ui->LineWidthLabel->setEnabled(Enable);
}

void swPreferencesDialog::ConditionalLinesCheckBoxToggled()
{
	const bool Enable = ui->edgeLines->isChecked() || ui->ConditionalLinesCheckBox->isChecked();

	ui->LineWidthSlider->setEnabled(Enable);
	ui->LineWidthLabel->setEnabled(Enable);
}

void swPreferencesDialog::LineWidthSliderValueChanged()
{
	float Value = mLineWidthRange[0] + static_cast<float>(ui->LineWidthSlider->value()) * mLineWidthGranularity;
	ui->LineWidthLabel->setText(swFormatDoubleLocalized(Value, locale(), 'g', 6));
}

swPreferencesDialog::swRenderTargetWidgets swPreferencesDialog::GetRenderTargetWidgets(swRenderTarget Target) const
{
	if (Target == swRenderTarget::MinifigureBuilder)
		return { ui->MinifigureBuilderSyncRendering, ui->MinifigureBuilderShadingModeLabel, ui->MinifigureBuilderShadingMode, ui->MinifigureBuilderEdgeLines, ui->MinifigureBuilderConditionalLines, ui->MinifigureBuilderLineWidthTitle, ui->MinifigureBuilderLineWidthSlider, ui->MinifigureBuilderLineWidthLabel };

	if (Target == swRenderTarget::Parts)
		return { ui->PartsSyncRendering, ui->PartsShadingModeLabel, ui->PartsShadingMode, ui->PartsEdgeLines, ui->PartsConditionalLines, ui->PartsLineWidthTitle, ui->PartsLineWidthSlider, ui->PartsLineWidthLabel };

	return { ui->PreviewSyncRendering, ui->PreviewShadingModeLabel, ui->PreviewShadingMode, ui->PreviewEdgeLines, ui->PreviewConditionalLines, ui->PreviewLineWidthTitle, ui->PreviewLineWidthSlider, ui->PreviewLineWidthLabel };
}

void swPreferencesDialog::RenderTargetChanged()
{
	for (swRenderTarget Target : { swRenderTarget::Preview, swRenderTarget::Parts, swRenderTarget::MinifigureBuilder })
	{
		const swRenderTargetWidgets Widgets = GetRenderTargetWidgets(Target);
		const bool Enable = !Widgets.SyncRendering->isChecked();
		const bool EnableLineWidth = Enable && (Widgets.EdgeLines->isChecked() || Widgets.ConditionalLines->isChecked());

		Widgets.ShadingModeLabel->setEnabled(Enable);
		Widgets.ShadingMode->setEnabled(Enable);
		Widgets.EdgeLines->setEnabled(Enable);
		Widgets.ConditionalLines->setEnabled(Enable);
		Widgets.LineWidthTitle->setEnabled(EnableLineWidth);
		Widgets.LineWidthSlider->setEnabled(EnableLineWidth);
		Widgets.LineWidthLabel->setEnabled(EnableLineWidth);

		float Value = mLineWidthRange[0] + static_cast<float>(Widgets.LineWidthSlider->value()) * mLineWidthGranularity;
		Widgets.LineWidthLabel->setText(swFormatDoubleLocalized(Value, locale(), 'g', 6));
	}
}

void swPreferencesDialog::MeshLODSliderValueChanged()
{
	float Value = ui->MeshLODSlider->value() * mMeshLODMultiplier;
	ui->MeshLODLabel->setText(swFormatIntegerLocalized(static_cast<int>(Value), locale()));
}

void swPreferencesDialog::FadeStepsToggled()
{
	ui->FadeStepsColor->setEnabled(ui->FadeSteps->isChecked());
}

void swPreferencesDialog::HighlightNewPartsToggled()
{
	ui->HighlightNewPartsColor->setEnabled(ui->HighlightNewParts->isChecked());
}

void swPreferencesDialog::GridStudsToggled()
{
	ui->gridStudColor->setEnabled(ui->gridStuds->isChecked());
}

void swPreferencesDialog::GridLinesToggled()
{
	ui->gridLineColor->setEnabled(ui->gridLines->isChecked());
	ui->gridLineSpacing->setEnabled(ui->gridLines->isChecked());
}

void swPreferencesDialog::PreviewViewSphereSizeComboCurrentIndexChanged(int Index)
{
	ui->PreviewViewSphereLocationCombo->setEnabled(Index != 0 && swContext::SupportsMaterial(swMaterialType::UnlitViewSphere));
}

void swPreferencesDialog::ViewSphereSizeComboCurrentIndexChanged(int Index)
{
	ui->ViewSphereLocationCombo->setEnabled(Index != 0 && swContext::SupportsMaterial(swMaterialType::UnlitViewSphere));
}

void swPreferencesDialog::AutomateEdgeColorToggled()
{
	const bool Enable = ui->AutomateEdgeColor->isChecked();

	mOptions->Preferences.mAutomateEdgeColor = Enable;
	ui->EdgeContrastTitle->setEnabled(Enable);
	ui->EdgeContrastSlider->setEnabled(Enable);
	ui->EdgeContrastLabel->setEnabled(Enable);
	ui->EdgeSaturationTitle->setEnabled(Enable);
	ui->EdgeSaturationSlider->setEnabled(Enable);
	ui->EdgeSaturationLabel->setEnabled(Enable);
}

void swPreferencesDialog::EdgeColorSliderValueChanged()
{
	mOptions->Preferences.mPartEdgeContrast = static_cast<float>(ui->EdgeContrastSlider->value()) * 0.01f;
	mOptions->Preferences.mPartColorValueLDIndex = static_cast<float>(ui->EdgeSaturationSlider->value()) * 0.01f;

	ui->EdgeContrastLabel->setText(swFormatDoubleLocalized(mOptions->Preferences.mPartEdgeContrast, locale(), 'f', 2));
	ui->EdgeSaturationLabel->setText(swFormatDoubleLocalized(mOptions->Preferences.mPartColorValueLDIndex, locale(), 'f', 2));
}

void swPreferencesDialog::StudStyleComboCurrentIndexChanged(int Index)
{
	ui->HighContrastButton->setEnabled(swIsHighContrast(static_cast<swStudStyle>(Index)));
}

void swPreferencesDialog::AutomateEdgeColor()
{
	swAutomateEdgeColorDialog Dialog(this, true, mOptions->Preferences);

	if (Dialog.exec() == QDialog::Accepted)
	{
		mOptions->Preferences.mStudCylinderColorEnabled = Dialog.mStudCylinderColorEnabled;
		mOptions->Preferences.mStudCylinderColor = Dialog.mStudCylinderColor;
		mOptions->Preferences.mPartEdgeColorEnabled = Dialog.mPartEdgeColorEnabled;
		mOptions->Preferences.mPartEdgeColor = Dialog.mPartEdgeColor;
		mOptions->Preferences.mBlackEdgeColorEnabled = Dialog.mBlackEdgeColorEnabled;
		mOptions->Preferences.mBlackEdgeColor = Dialog.mBlackEdgeColor;
		mOptions->Preferences.mDarkEdgeColorEnabled = Dialog.mDarkEdgeColorEnabled;
		mOptions->Preferences.mDarkEdgeColor = Dialog.mDarkEdgeColor;
		mOptions->Preferences.mPartEdgeContrast = Dialog.mPartEdgeContrast;
		mOptions->Preferences.mPartColorValueLDIndex = Dialog.mPartColorValueLDIndex;

		// The light/dark value is the same setting as the edge saturation.
		ui->EdgeSaturationSlider->setValue(qRound(mOptions->Preferences.mPartColorValueLDIndex * 100.0f));
	}
}

void swPreferencesDialog::UpdateCategories()
{
	QTreeWidgetItem* CategoryItem;
	QTreeWidget* CategoriesTree = ui->categoriesTree;

	disconnect(ui->categoriesTree->model(), &QAbstractItemModel::rowsInserted, this, &swPreferencesDialog::CategoriesDropped);

	CategoriesTree->clear();

	for (int CategoryIndex = 0; CategoryIndex < static_cast<int>(mOptions->Categories.size()); CategoryIndex++)
	{
		CategoryItem = new QTreeWidgetItem(CategoriesTree, QStringList(mOptions->Categories[CategoryIndex].Name));
		CategoryItem->setData(0, CategoryRole, QVariant(CategoryIndex));
		CategoryItem->setFlags(CategoryItem->flags() & ~Qt::ItemFlag::ItemIsDropEnabled);
	}

	CategoryItem = new QTreeWidgetItem(CategoriesTree, QStringList(tr("Unassigned")));
	CategoryItem->setData(0, CategoryRole, QVariant(-1));
	CategoryItem->setFlags(CategoryItem->flags() & ~Qt::ItemFlag::ItemIsDropEnabled);

	connect(ui->categoriesTree->model(), &QAbstractItemModel::rowsInserted, this, &swPreferencesDialog::CategoriesDropped);

	QTimer::singleShot(0, this, &swPreferencesDialog::SnapCategoriesTree);
}

// use to fit the categories list to its longest name beside the scroll bar
void swPreferencesDialog::SnapCategoriesTree()
{
	QTreeWidget* CategoriesTree = ui->categoriesTree;

	if (!CategoriesTree->isVisible())
		return;

	int TextWidth = CategoriesTree->header()->sectionSizeHint(0);

	for (int Row = 0; Row < CategoriesTree->topLevelItemCount(); Row++)
		TextWidth = qMax(TextWidth, CategoriesTree->sizeHintForIndex(CategoriesTree->model()->index(Row, 0)).width());

	const int ScrollBarWidth = CategoriesTree->verticalScrollBar()->isVisible() ? CategoriesTree->verticalScrollBar()->width() : 0;
	const int Width = TextWidth + ScrollBarWidth + CategoriesTree->frameWidth() * 2;
	const QList<int> Sizes = ui->splitter->sizes();

	if (Sizes.size() != 2 || Sizes[0] == Width)
		return;

	ui->splitter->setSizes({ Width, Sizes[0] + Sizes[1] - Width });
}

void swPreferencesDialog::UpdateParts()
{
	swPartsLibrary* Library = swGetPartsLibrary();
	QTreeWidget* Tree = ui->partsTree;

	Tree->clear();

	QList<QTreeWidgetItem*> SelectedItems = ui->categoriesTree->selectedItems();

	if (SelectedItems.empty())
		return;

	QTreeWidgetItem* CategoryItem = SelectedItems.first();
	int CategoryIndex = CategoryItem->data(0, CategoryRole).toInt();

	if (CategoryIndex != -1)
	{
		std::vector<swPartData*> SingleParts, GroupedParts;

		Library->GetCategoryEntries(mOptions->Categories[CategoryIndex].Keywords.constData(), false, SingleParts, GroupedParts);

		for (swPartData* PartData : SingleParts)
		{
			QStringList RowList(PartData->m_strDescription);
			RowList.append(PartData->mFileName);

			new QTreeWidgetItem(Tree, RowList);
		}
	}
	else
	{
		for (swPartData* PartData : Library->GetParts())
		{
			for (CategoryIndex = 0; CategoryIndex < static_cast<int>(mOptions->Categories.size()); CategoryIndex++)
			{
				if (Library->PartDataInCategory(PartData, mOptions->Categories[CategoryIndex].Keywords.constData()))
					break;
			}

			if (CategoryIndex == static_cast<int>(mOptions->Categories.size()))
			{
				QStringList RowList(PartData->m_strDescription);
				RowList.append(PartData->mFileName);

				new QTreeWidgetItem(Tree, RowList);
			}
		}
	}

	Tree->resizeColumnToContents(0);
	Tree->resizeColumnToContents(1);
}

void swPreferencesDialog::CategoriesDropped(const QModelIndex& Parent, int First, int Last)
{
	Q_UNUSED(Parent);
	Q_UNUSED(First);
	Q_UNUSED(Last);

	std::vector<swLibraryCategory> Categories;

	for (int Row = 0; Row < ui->categoriesTree->topLevelItemCount(); Row++)
	{
		QTreeWidgetItem* CategoryItem = ui->categoriesTree->topLevelItem(Row);
		int CategoryIndex = CategoryItem->data(0, CategoryRole).toInt();

		if (CategoryIndex >= 0 && CategoryIndex < static_cast<int>(mOptions->Categories.size()))
		{
			Categories.emplace_back(mOptions->Categories[CategoryIndex]);
			CategoryItem->setData(0, CategoryRole, Row);
		}
	}

	mOptions->CategoriesModified = true;
	mOptions->CategoriesDefault = false;
	mOptions->Categories = std::move(Categories);
}

void swPreferencesDialog::NewCategoryClicked()
{
	swLibraryCategory Category;

	swCategoryDialog Dialog(this, &Category);
	if (Dialog.exec() != QDialog::Accepted)
		return;

	mOptions->CategoriesModified = true;
	mOptions->CategoriesDefault = false;
	mOptions->Categories.emplace_back(std::move(Category));

	UpdateCategories();
	ui->categoriesTree->setCurrentItem(ui->categoriesTree->topLevelItem(static_cast<int>(mOptions->Categories.size()) - 1));
}

void swPreferencesDialog::EditCategoryClicked()
{
	QList<QTreeWidgetItem*> SelectedItems = ui->categoriesTree->selectedItems();

	if (SelectedItems.empty())
		return;

	QTreeWidgetItem* CategoryItem = SelectedItems.first();
	int CategoryIndex = CategoryItem->data(0, CategoryRole).toInt();

	if (CategoryIndex == -1)
		return;

	swCategoryDialog Dialog(this, &mOptions->Categories[CategoryIndex]);

	if (Dialog.exec() != QDialog::Accepted)
		return;

	mOptions->CategoriesModified = true;
	mOptions->CategoriesDefault = false;

	UpdateCategories();
	ui->categoriesTree->setCurrentItem(ui->categoriesTree->topLevelItem(CategoryIndex));
}

void swPreferencesDialog::DeleteCategoryClicked()
{
	QList<QTreeWidgetItem*> selectedItems = ui->categoriesTree->selectedItems();

	if (selectedItems.empty())
		return;

	QTreeWidgetItem* CategoryItem = selectedItems.first();
	int CategoryIndex = CategoryItem->data(0, CategoryRole).toInt();

	if (CategoryIndex == -1)
		return;

	QString Question = tr("Are you sure you want to delete the category '%1'?").arg(mOptions->Categories[CategoryIndex].Name);
	if (QMessageBox::question(this, tr("Delete Category"), Question, QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
		return;

	mOptions->CategoriesModified = true;
	mOptions->CategoriesDefault = false;
	mOptions->Categories.erase(mOptions->Categories.begin() + CategoryIndex);

	UpdateCategories();
}

void swPreferencesDialog::ImportCategoriesClicked()
{
	QString FileName = QFileDialog::getOpenFileName(this, tr("Import Categories"), "", tr("Text Files (*.txt);;All Files (*.*)"));

	if (FileName.isEmpty())
		return;

	std::vector<swLibraryCategory> Categories;
	if (!swLoadCategories(FileName, Categories))
	{
		QMessageBox::warning(this, tr("Import Categories"), tr("Error loading categories file."));
		return;
	}

	mOptions->Categories = Categories;
	mOptions->CategoriesModified = true;
	mOptions->CategoriesDefault = false;
}

void swPreferencesDialog::ExportCategoriesClicked()
{
	QString FileName = QFileDialog::getSaveFileName(this, tr("Export Categories"), "", tr("Text Files (*.txt);;All Files (*.*)"));

	if (FileName.isEmpty())
		return;

	if (!swSaveCategories(FileName, mOptions->Categories))
	{
		QMessageBox::warning(this, tr("Export Categories"), tr("Error saving categories file."));
		return;
	}
}

void swPreferencesDialog::ResetCategoriesClicked()
{
	if (QMessageBox::question(this, tr("Reset Categories"), tr("Are you sure you want to load the default categories?"), QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
		return;

	swResetCategories(mOptions->Categories);

	mOptions->CategoriesModified = true;
	mOptions->CategoriesDefault = true;

	UpdateCategories();
}

// Mouse shortcuts read like key sequences, with the button name after the modifiers.
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
	// The mouse sequence field records the button and modifiers it is clicked with.
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

// Shows the tree items whose text in any column contains the filter, with their categories.
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

// The mouse tools show their menu names, grouped like the keyboard commands.
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

void swInstructionsPreferences::Load()
{
	PageSetup = { swGetProfileFloat(SW_PROFILE_INSTRUCTIONS_DEFAULT_PAGE_WIDTH), swGetProfileFloat(SW_PROFILE_INSTRUCTIONS_DEFAULT_PAGE_HEIGHT), swGetProfileFloat(SW_PROFILE_INSTRUCTIONS_DEFAULT_MARGIN_LEFT), swGetProfileFloat(SW_PROFILE_INSTRUCTIONS_DEFAULT_MARGIN_RIGHT), swGetProfileFloat(SW_PROFILE_INSTRUCTIONS_DEFAULT_MARGIN_TOP), swGetProfileFloat(SW_PROFILE_INSTRUCTIONS_DEFAULT_MARGIN_BOTTOM) };
	PageUnits = swGetProfileInteger(SW_PROFILE_INSTRUCTIONS_DEFAULT_PAGE_UNITS);
	MarginUnits = swGetProfileInteger(SW_PROFILE_INSTRUCTIONS_DEFAULT_MARGIN_UNITS);
	ShowGrid = swGetProfileInteger(SW_PROFILE_INSTRUCTIONS_DEFAULT_SHOW_GRID) != 0;
	SnapToGrid = swGetProfileInteger(SW_PROFILE_INSTRUCTIONS_DEFAULT_SNAP_TO_GRID) != 0;
	GridSize = swGetProfileFloat(SW_PROFILE_INSTRUCTIONS_DEFAULT_GRID_SIZE);
	GridDivisions = swGetProfileInteger(SW_PROFILE_INSTRUCTIONS_DEFAULT_GRID_DIVISIONS);
	GridWithinMargins = swGetProfileInteger(SW_PROFILE_INSTRUCTIONS_DEFAULT_GRID_WITHIN_MARGINS) != 0;
	NewModelDefaults = swGetProfileInteger(SW_PROFILE_INSTRUCTIONS_NEW_MODEL_DEFAULTS) != 0;
	ShowMargins = swGetProfileInteger(SW_PROFILE_INSTRUCTIONS_SHOW_MARGINS) != 0;
	GridPrimaryColor = swQColorFromRGBA(swGetProfileUnsignedInteger(SW_PROFILE_INSTRUCTIONS_GRID_PRIMARY_COLOR));
	GridSecondaryColor = swQColorFromRGBA(swGetProfileUnsignedInteger(SW_PROFILE_INSTRUCTIONS_GRID_SECONDARY_COLOR));
	MarginColor = swQColorFromRGBA(swGetProfileUnsignedInteger(SW_PROFILE_INSTRUCTIONS_MARGIN_COLOR));
	EditorHighlight = swGetProfileInteger(SW_PROFILE_INSTRUCTIONS_EDITOR_HIGHLIGHT) != 0;
	EditorNumbers = swGetProfileInteger(SW_PROFILE_INSTRUCTIONS_EDITOR_NUMBERS) != 0;
	EditorWrap = swGetProfileInteger(SW_PROFILE_INSTRUCTIONS_EDITOR_WRAP) != 0;
	EditorMarkers = swGetProfileInteger(SW_PROFILE_INSTRUCTIONS_EDITOR_MARKERS) != 0;
}

void swInstructionsPreferences::Save() const
{
	swSetProfileFloat(SW_PROFILE_INSTRUCTIONS_DEFAULT_PAGE_WIDTH, PageSetup.Width);
	swSetProfileFloat(SW_PROFILE_INSTRUCTIONS_DEFAULT_PAGE_HEIGHT, PageSetup.Height);
	swSetProfileFloat(SW_PROFILE_INSTRUCTIONS_DEFAULT_MARGIN_LEFT, PageSetup.MarginLeft);
	swSetProfileFloat(SW_PROFILE_INSTRUCTIONS_DEFAULT_MARGIN_RIGHT, PageSetup.MarginRight);
	swSetProfileFloat(SW_PROFILE_INSTRUCTIONS_DEFAULT_MARGIN_TOP, PageSetup.MarginTop);
	swSetProfileFloat(SW_PROFILE_INSTRUCTIONS_DEFAULT_MARGIN_BOTTOM, PageSetup.MarginBottom);
	swSetProfileInteger(SW_PROFILE_INSTRUCTIONS_DEFAULT_PAGE_UNITS, PageUnits);
	swSetProfileInteger(SW_PROFILE_INSTRUCTIONS_DEFAULT_MARGIN_UNITS, MarginUnits);
	swSetProfileInteger(SW_PROFILE_INSTRUCTIONS_DEFAULT_SHOW_GRID, ShowGrid);
	swSetProfileInteger(SW_PROFILE_INSTRUCTIONS_DEFAULT_SNAP_TO_GRID, SnapToGrid);
	swSetProfileFloat(SW_PROFILE_INSTRUCTIONS_DEFAULT_GRID_SIZE, GridSize);
	swSetProfileInteger(SW_PROFILE_INSTRUCTIONS_DEFAULT_GRID_DIVISIONS, GridDivisions);
	swSetProfileInteger(SW_PROFILE_INSTRUCTIONS_DEFAULT_GRID_WITHIN_MARGINS, GridWithinMargins);
	swSetProfileInteger(SW_PROFILE_INSTRUCTIONS_NEW_MODEL_DEFAULTS, NewModelDefaults);
	swSetProfileInteger(SW_PROFILE_INSTRUCTIONS_SHOW_MARGINS, ShowMargins);
	swSetProfileUnsignedInteger(SW_PROFILE_INSTRUCTIONS_GRID_PRIMARY_COLOR, swRGBAFromQColor(GridPrimaryColor));
	swSetProfileUnsignedInteger(SW_PROFILE_INSTRUCTIONS_GRID_SECONDARY_COLOR, swRGBAFromQColor(GridSecondaryColor));
	swSetProfileUnsignedInteger(SW_PROFILE_INSTRUCTIONS_MARGIN_COLOR, swRGBAFromQColor(MarginColor));
	swSetProfileInteger(SW_PROFILE_INSTRUCTIONS_EDITOR_HIGHLIGHT, EditorHighlight);
	swSetProfileInteger(SW_PROFILE_INSTRUCTIONS_EDITOR_NUMBERS, EditorNumbers);
	swSetProfileInteger(SW_PROFILE_INSTRUCTIONS_EDITOR_WRAP, EditorWrap);
	swSetProfileInteger(SW_PROFILE_INSTRUCTIONS_EDITOR_MARKERS, EditorMarkers);
}

// use to build the instructions tab after the colors, its values are kept in the dialog options until they are accepted
void swPreferencesDialog::CreateInstructionsTab()
{
	swInstructionsPreferences& Instructions = mOptions->Instructions;

	QWidget* Tab = new QWidget(ui->tabWidget);
	Tab->setWhatsThis(tr("<p><b>Instructions Preferences</b></p><p>Set the default page setup and grid that new models and the Default Page Setup option of the Properties panel load, and the options of the Instructions window.</p>"));
	QFormLayout* TabLayout = new QFormLayout(Tab);
	TabLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

	auto CreateCheckBox = [](QWidget* Parent, const QString& Text, const QString& ToolTip, bool Checked)
	{
		QCheckBox* CheckBox = new QCheckBox(Text, Parent);
		CheckBox->setToolTip(ToolTip);
		CheckBox->setChecked(Checked);
		return CheckBox;
	};

	QGroupBox* PageGroup = new QGroupBox(tr("Default Page Setup"), Tab);
	QFormLayout* PageLayout = new QFormLayout(PageGroup);
	TabLayout->addRow(PageGroup);

	QHBoxLayout* PageRowLayout = new QHBoxLayout();
	// the button shows the default page setup and opens it
	mInstructionsPageButton = new QPushButton(PageGroup);
	mInstructionsPageButton->setToolTip(tr("Choose the default page size, orientation, margins and their units."));
	mInstructionsPageButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	PageRowLayout->addWidget(mInstructionsPageButton, 1);

	QPushButton* PageResetButton = new QPushButton(tr("Reset"), PageGroup);
	PageResetButton->setToolTip(tr("Bring back the built-in default page setup, grid and line colors."));
	PageRowLayout->addWidget(PageResetButton);

	QLabel* PageLabel = new QLabel(tr("Page"), PageGroup);
	PageLabel->setToolTip(tr("Page size, orientation and margins of new instructions."));
	PageLayout->addRow(PageLabel, PageRowLayout);

	mInstructionsNewModelDefaults = CreateCheckBox(PageGroup, tr("Load on New Models"), tr("Start every new model with the default page setup and grid."), Instructions.NewModelDefaults);
	PageLayout->addRow(mInstructionsNewModelDefaults);

	QGroupBox* GridGroup = new QGroupBox(tr("Default Grid"), Tab);
	QFormLayout* GridLayout = new QFormLayout(GridGroup);
	TabLayout->addRow(GridGroup);

	mInstructionsShowGrid = CreateCheckBox(GridGroup, tr("Show Grid"), tr("Show the grid on the pages of new instructions, it is never printed."), Instructions.ShowGrid);
	GridLayout->addRow(mInstructionsShowGrid);
	mInstructionsSnapToGrid = CreateCheckBox(GridGroup, tr("Snap to Grid"), tr("Snap dragged elements to the grid of new instructions."), Instructions.SnapToGrid);
	GridLayout->addRow(mInstructionsSnapToGrid);

	mInstructionsGridSize = new QDoubleSpinBox(GridGroup);
	mInstructionsGridSize->setToolTip(tr("Main unit of the grid drawn with the primary color, in the margin units of the default page setup."));
	mInstructionsGridSize->setAccessibleName(tr("Grid Size"));
	QLabel* GridSizeLabel = new QLabel(tr("Grid Size"), GridGroup);
	GridSizeLabel->setBuddy(mInstructionsGridSize);
	GridSizeLabel->setToolTip(mInstructionsGridSize->toolTip());
	GridLayout->addRow(GridSizeLabel, mInstructionsGridSize);

	mInstructionsGridDivisions = new QSpinBox(GridGroup);
	mInstructionsGridDivisions->setRange(1, SW_GRID_DIVISIONS_MAXIMUM);
	mInstructionsGridDivisions->setPrefix(QLatin1String("1/"));
	mInstructionsGridDivisions->setValue(Instructions.GridDivisions);
	mInstructionsGridDivisions->setToolTip(tr("Split each grid unit into fractions drawn with the secondary color, dragged elements snap to the fractions."));
	mInstructionsGridDivisions->setAccessibleName(tr("Divisions"));
	QLabel* GridDivisionsLabel = new QLabel(tr("Divisions"), GridGroup);
	GridDivisionsLabel->setBuddy(mInstructionsGridDivisions);
	GridDivisionsLabel->setToolTip(mInstructionsGridDivisions->toolTip());
	GridLayout->addRow(GridDivisionsLabel, mInstructionsGridDivisions);

	mInstructionsGridScope = new QComboBox(GridGroup);
	mInstructionsGridScope->addItems({ tr("Margins"), tr("Page") });
	mInstructionsGridScope->setToolTip(tr("Lay the grid out within the margins or across the whole page."));
	mInstructionsGridScope->setAccessibleName(tr("Grid Area"));
	mInstructionsGridScope->setCurrentIndex(Instructions.GridWithinMargins ? 0 : 1);
	QLabel* GridScopeLabel = new QLabel(tr("Grid Area"), GridGroup);
	GridScopeLabel->setBuddy(mInstructionsGridScope);
	GridScopeLabel->setToolTip(mInstructionsGridScope->toolTip());
	GridLayout->addRow(GridScopeLabel, mInstructionsGridScope);

	QGroupBox* WindowGroup = new QGroupBox(tr("Instructions Page"), Tab);
	QFormLayout* WindowLayout = new QFormLayout(WindowGroup);
	TabLayout->addRow(WindowGroup);

	mInstructionsShowMargins = CreateCheckBox(WindowGroup, tr("Show Margins"), tr("Outline the page margins on screen, they are never printed."), Instructions.ShowMargins);
	WindowLayout->addRow(mInstructionsShowMargins);

	// the line colors keep their transparency, they are picked below their button
	auto CreateColorButton = [this, WindowLayout, WindowGroup](const QString& Name, const QString& ToolTip, QColor* Color)
	{
		QToolButton* Button = new QToolButton(WindowGroup);
		Button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
		Button->setToolTip(ToolTip);
		Button->setAccessibleName(Name);
		swUpdateColorButton(Button, *Color);

		QLabel* Label = new QLabel(Name, WindowGroup);
		Label->setBuddy(Button);
		Label->setToolTip(ToolTip);
		WindowLayout->addRow(Label, Button);

		connect(Button, &QToolButton::clicked, this, [Button, Color]()
		{
			const QColor SelectedColor = swPickColor(Button, *Color);

			if (!SelectedColor.isValid())
				return;

			*Color = SelectedColor;
			swUpdateColorButton(Button, SelectedColor);
		});

		return Button;
	};

	mInstructionsMarginColor = CreateColorButton(tr("Margin Color"), tr("Color of the dashed outline of the page margins."), &Instructions.MarginColor);
	mInstructionsGridPrimaryColor = CreateColorButton(tr("Grid Primary Color"), tr("Color of the grid lines at every grid unit."), &Instructions.GridPrimaryColor);
	mInstructionsGridSecondaryColor = CreateColorButton(tr("Grid Secondary Color"), tr("Color of the grid lines at the fractions of a grid unit."), &Instructions.GridSecondaryColor);

	QGroupBox* EditorGroup = new QGroupBox(tr("Editor"), Tab);
	QFormLayout* EditorLayout = new QFormLayout(EditorGroup);
	TabLayout->addRow(EditorGroup);

	mInstructionsEditorHighlight = CreateCheckBox(EditorGroup, tr("Syntax Highlighting"), tr("Color the commands of the model file in the Editor panel."), Instructions.EditorHighlight);
	EditorLayout->addRow(mInstructionsEditorHighlight);
	mInstructionsEditorNumbers = CreateCheckBox(EditorGroup, tr("Line Numbers"), tr("Number the lines of the model file in the Editor panel."), Instructions.EditorNumbers);
	EditorLayout->addRow(mInstructionsEditorNumbers);
	mInstructionsEditorWrap = CreateCheckBox(EditorGroup, tr("Wrap Lines"), tr("Wrap long lines of the model file in the Editor panel."), Instructions.EditorWrap);
	EditorLayout->addRow(mInstructionsEditorWrap);
	mInstructionsEditorMarkers = CreateCheckBox(EditorGroup, tr("Whitespace Markers"), tr("Mark tabs, spaces and line ends in the Editor panel."), Instructions.EditorMarkers);
	EditorLayout->addRow(mInstructionsEditorMarkers);

	ui->tabWidget->insertTab(ui->tabWidget->indexOf(ui->tabCategories), Tab, tr("Instructions"));

	connect(mInstructionsPageButton, &QPushButton::clicked, this, [this]()
	{
		swInstructionsPreferences& Instructions = mOptions->Instructions;

		// the dialog stores its units right away, they are kept with the other values until the preferences are accepted
		const int PageUnits = swGetProfileInteger(SW_PROFILE_INSTRUCTIONS_DEFAULT_PAGE_UNITS);
		const int MarginUnits = swGetProfileInteger(SW_PROFILE_INSTRUCTIONS_DEFAULT_MARGIN_UNITS);
		swSetProfileInteger(SW_PROFILE_INSTRUCTIONS_DEFAULT_PAGE_UNITS, Instructions.PageUnits);
		swSetProfileInteger(SW_PROFILE_INSTRUCTIONS_DEFAULT_MARGIN_UNITS, Instructions.MarginUnits);

		Instructions.GridSize = static_cast<float>(mInstructionsGridSize->value()) * swGetPageUnitScale(static_cast<swInstructionsPageUnits>(Instructions.MarginUnits));

		swPageSetupDialog Dialog(this, &Instructions.PageSetup, true);

		if (Dialog.exec() == QDialog::Accepted)
		{
			Instructions.PageUnits = swGetProfileInteger(SW_PROFILE_INSTRUCTIONS_DEFAULT_PAGE_UNITS);
			Instructions.MarginUnits = swGetProfileInteger(SW_PROFILE_INSTRUCTIONS_DEFAULT_MARGIN_UNITS);
		}

		swSetProfileInteger(SW_PROFILE_INSTRUCTIONS_DEFAULT_PAGE_UNITS, PageUnits);
		swSetProfileInteger(SW_PROFILE_INSTRUCTIONS_DEFAULT_MARGIN_UNITS, MarginUnits);

		UpdateInstructionsPage();
	});

	connect(PageResetButton, &QPushButton::clicked, this, [this]()
	{
		swInstructionsPreferences& Instructions = mOptions->Instructions;

		Instructions.PageSetup = { swGetProfileDefaultFloat(SW_PROFILE_INSTRUCTIONS_DEFAULT_PAGE_WIDTH), swGetProfileDefaultFloat(SW_PROFILE_INSTRUCTIONS_DEFAULT_PAGE_HEIGHT), swGetProfileDefaultFloat(SW_PROFILE_INSTRUCTIONS_DEFAULT_MARGIN_LEFT), swGetProfileDefaultFloat(SW_PROFILE_INSTRUCTIONS_DEFAULT_MARGIN_RIGHT), swGetProfileDefaultFloat(SW_PROFILE_INSTRUCTIONS_DEFAULT_MARGIN_TOP), swGetProfileDefaultFloat(SW_PROFILE_INSTRUCTIONS_DEFAULT_MARGIN_BOTTOM) };
		Instructions.PageUnits = swGetProfileDefaultInteger(SW_PROFILE_INSTRUCTIONS_DEFAULT_PAGE_UNITS);
		Instructions.MarginUnits = swGetProfileDefaultInteger(SW_PROFILE_INSTRUCTIONS_DEFAULT_MARGIN_UNITS);
		Instructions.GridSize = swGetProfileDefaultFloat(SW_PROFILE_INSTRUCTIONS_DEFAULT_GRID_SIZE);

		mInstructionsShowGrid->setChecked(swGetProfileDefaultInteger(SW_PROFILE_INSTRUCTIONS_DEFAULT_SHOW_GRID) != 0);
		mInstructionsSnapToGrid->setChecked(swGetProfileDefaultInteger(SW_PROFILE_INSTRUCTIONS_DEFAULT_SNAP_TO_GRID) != 0);
		mInstructionsGridScope->setCurrentIndex(swGetProfileDefaultInteger(SW_PROFILE_INSTRUCTIONS_DEFAULT_GRID_WITHIN_MARGINS) ? 0 : 1);
		mInstructionsGridDivisions->setValue(swGetProfileDefaultInteger(SW_PROFILE_INSTRUCTIONS_DEFAULT_GRID_DIVISIONS));

		Instructions.GridPrimaryColor = swQColorFromRGBA(swGetProfileDefaultUnsignedInteger(SW_PROFILE_INSTRUCTIONS_GRID_PRIMARY_COLOR));
		Instructions.GridSecondaryColor = swQColorFromRGBA(swGetProfileDefaultUnsignedInteger(SW_PROFILE_INSTRUCTIONS_GRID_SECONDARY_COLOR));
		Instructions.MarginColor = swQColorFromRGBA(swGetProfileDefaultUnsignedInteger(SW_PROFILE_INSTRUCTIONS_MARGIN_COLOR));
		swUpdateColorButton(mInstructionsGridPrimaryColor, Instructions.GridPrimaryColor);
		swUpdateColorButton(mInstructionsGridSecondaryColor, Instructions.GridSecondaryColor);
		swUpdateColorButton(mInstructionsMarginColor, Instructions.MarginColor);

		UpdateInstructionsPage();
	});

	UpdateInstructionsPage();
}

// use to show the default page and the grid size in the units of the default page setup
void swPreferencesDialog::UpdateInstructionsPage()
{
	const swInstructionsPreferences& Instructions = mOptions->Instructions;
	const swInstructionsPageSetup& PageSetup = Instructions.PageSetup;
	const swInstructionsPageUnits PageUnits = static_cast<swInstructionsPageUnits>(qBound(0, Instructions.PageUnits, static_cast<int>(swInstructionsPageUnits::Inches)));
	const swInstructionsPageUnits MarginUnits = static_cast<swInstructionsPageUnits>(qBound(0, Instructions.MarginUnits, static_cast<int>(swInstructionsPageUnits::Inches)));

	auto FormatLength = [this](float Points, swInstructionsPageUnits Units)
	{
		return locale().toString(Points / swGetPageUnitScale(Units), 'g', 6);
	};

	const QString Orientation = PageSetup.Width > PageSetup.Height ? tr("Landscape") : tr("Portrait");
	QString Margins = FormatLength(PageSetup.MarginLeft, MarginUnits);

	if (PageSetup.MarginRight != PageSetup.MarginLeft || PageSetup.MarginTop != PageSetup.MarginLeft || PageSetup.MarginBottom != PageSetup.MarginLeft)
		Margins = tr("%1, %2, %3, %4").arg(Margins, FormatLength(PageSetup.MarginRight, MarginUnits), FormatLength(PageSetup.MarginTop, MarginUnits), FormatLength(PageSetup.MarginBottom, MarginUnits));

	mInstructionsPageButton->setText(tr("%1 x %2 %3 %4, margins %5 %6...").arg(FormatLength(PageSetup.Width, PageUnits), FormatLength(PageSetup.Height, PageUnits), swGetPageUnitSuffix(PageUnits), Orientation, Margins, swGetPageUnitSuffix(MarginUnits)));

	QSignalBlocker Blocker(mInstructionsGridSize);
	swSetupLengthSpinBox(mInstructionsGridSize, MarginUnits, SW_GRID_SIZE_MINIMUM, SW_GRID_SIZE_MAXIMUM);
	mInstructionsGridSize->setValue(Instructions.GridSize / swGetPageUnitScale(MarginUnits));
}
