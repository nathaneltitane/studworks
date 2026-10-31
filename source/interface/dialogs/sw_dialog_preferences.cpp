#include "sw_global.h"
#include "sw_dialog_preferences.h"
#include "ui_sw_dialog_preferences.h"
#include "sw_window_utilities.h"
#include "sw_library.h"
#include "sw_dialog_color_edge.h"
#include "sw_dialog_preferences_blender.h"
#include "sw_window_main.h"
#include "sw_view_widget.h"
#include "sw_view.h"

static const char* gLanguageLocales[] =
{
	"", "cs_CZ", "de_DE", "en_US", "fr_FR", "pt_PT", "es_ES", "ru_RU", "uk_UA", "zh_CN", "zh_TW"
};

// use to tell control column widgets
static bool swIsColumnControl(QWidget* Widget)
{
	if (QToolButton* Button = qobject_cast<QToolButton*>(Widget))
		return Button->toolButtonStyle() != Qt::ToolButtonIconOnly && !Button->text().isEmpty();

	return qobject_cast<QComboBox*>(Widget) || qobject_cast<QAbstractSpinBox*>(Widget) || qobject_cast<QSlider*>(Widget);
}

// line up label and control columns
static void swAlignPreferencesColumns(QTabWidget* TabWidget)
{
	int LabelWidth = 0;

	// share the label column across pages
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

					// end spanning checkboxes before controls
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

					// end spanning checkboxes before controls
					QWidget* Widget = GridLayout->itemAt(ItemIndex)->widget();

					if (Column == 0 && Widget && (ColumnSpan == 1 || qobject_cast<QCheckBox*>(Widget)))
						LabelWidth = qMax(LabelWidth, GridLayout->itemAt(ItemIndex)->sizeHint().width());
				}
			}
		}
	}

	// keep the longest label clear
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

				// controls reach the right edge
				FormLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
				FormLayouts.append(FormLayout);
				continue;
			}

			QGridLayout* GridLayout = qobject_cast<QGridLayout*>(Group->layout());

			if (!GridLayout)
				continue;

			// keep plain checkbox groups
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

			// stop spacers pushing controls
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

			// share the value and unit width
			int TrailingWidth = 0;

			for (QWidget* Widget : SpanControls)
			{
				int Row, Column, RowSpan, ColumnSpan;
				GridLayout->getItemPosition(GridLayout->indexOf(Widget), &Row, &Column, &RowSpan, &ColumnSpan);

				for (int TrailingColumn = Column + ColumnSpan; TrailingColumn <= LastColumn; TrailingColumn++)
					if (QLayoutItem* Trailing = GridLayout->itemAtPosition(Row, TrailingColumn))
						if (qobject_cast<QLabel*>(Trailing->widget()))
							TrailingWidth = qMax(TrailingWidth, qMax(Trailing->widget()->sizeHint().width(), Trailing->widget()->minimumWidth()));
			}

			// controls span to the right edge
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

				// slider value on the left
				const bool IsSlider = qobject_cast<QSlider*>(Widget) != nullptr;

				for (QWidget* TrailingWidget : Trailing)
				{
					GridLayout->removeWidget(TrailingWidget);

					if (IsSlider && qobject_cast<QLabel*>(TrailingWidget))
					{
						TrailingWidget->setMinimumWidth(TrailingWidth);
						static_cast<QLabel*>(TrailingWidget)->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
						RowLayout->insertWidget(RowLayout->indexOf(Widget), TrailingWidget, 0, Qt::AlignLeft | Qt::AlignVCenter);
					}
					else if (qobject_cast<QLabel*>(TrailingWidget))
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

		// match the widest control
		for (QWidget* Control : Controls)
		{
			Control->setProperty("swColumnWidth", ControlWidth);
			Control->setMinimumWidth(ControlWidth);
		}

		// never narrower column controls
		for (QWidget* Control : FillControls)
			Control->setMinimumWidth(ControlWidth);

		for (QGridLayout* GridLayout : GridLayouts)
		{
			GridLayout->setColumnMinimumWidth(0, LabelWidth);
			GridLayout->setColumnMinimumWidth(1, ControlWidth);

			// only the control column grows
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
	connect(ui->UnofficialLibraryBrowseButton, &QPushButton::clicked, this, &swPreferencesDialog::UnofficialLibraryBrowseButtonClicked);
	connect(ui->ShadowLibraryEnabled, &QCheckBox::toggled, this, &swPreferencesDialog::ShadowLibraryEnabledToggled);
	connect(ui->ShadowLibraryBrowseButton, &QPushButton::clicked, this, &swPreferencesDialog::ShadowLibraryBrowseButtonClicked);
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
	connect(ui->CutawayShadeAlphaSlider, QOverload<int>::of(&QSlider::valueChanged), this, &swPreferencesDialog::CutawayShadeAlphaChanged);
	connect(ui->AxisIconCubeOutlineAlphaSlider, QOverload<int>::of(&QSlider::valueChanged), this, &swPreferencesDialog::AxisIconCubeOutlineAlphaChanged);
	connect(ui->AxisIconCubeOutlineCheckBox, &QCheckBox::toggled, this, [this]() { AxisIconSizeComboCurrentIndexChanged(ui->AxisIconSizeCombo->currentIndex()); });

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
	connect(ui->ViewCubeSizeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &swPreferencesDialog::ViewCubeSizeComboCurrentIndexChanged);
	connect(ui->PreviewViewCubeSizeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &swPreferencesDialog::PreviewViewCubeSizeComboCurrentIndexChanged);
	connect(ui->AxisIconSizeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &swPreferencesDialog::AxisIconSizeComboCurrentIndexChanged);
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
	connect(ui->CutawayShadeColorButton, &QToolButton::clicked, this, &swPreferencesDialog::ColorButtonClicked);
	connect(ui->FadeStepsColor, &QToolButton::clicked, this, &swPreferencesDialog::ColorButtonClicked);
	connect(ui->HighlightNewPartsColor, &QToolButton::clicked, this, &swPreferencesDialog::ColorButtonClicked);
	connect(ui->gridStudColor, &QToolButton::clicked, this, &swPreferencesDialog::ColorButtonClicked);
	connect(ui->gridLineColor, &QToolButton::clicked, this, &swPreferencesDialog::ColorButtonClicked);
	connect(ui->ViewCubeColorButton, &QToolButton::clicked, this, &swPreferencesDialog::ColorButtonClicked);
	connect(ui->ViewCubeTextColorButton, &QToolButton::clicked, this, &swPreferencesDialog::ColorButtonClicked);
	connect(ui->ViewCubeHighlightColorButton, &QToolButton::clicked, this, &swPreferencesDialog::ColorButtonClicked);
	connect(ui->AxisXColorButton, &QToolButton::clicked, this, &swPreferencesDialog::ColorButtonClicked);
	connect(ui->AxisYColorButton, &QToolButton::clicked, this, &swPreferencesDialog::ColorButtonClicked);
	connect(ui->AxisZColorButton, &QToolButton::clicked, this, &swPreferencesDialog::ColorButtonClicked);
	connect(ui->AxisXNegativeColorButton, &QToolButton::clicked, this, &swPreferencesDialog::ColorButtonClicked);
	connect(ui->AxisYNegativeColorButton, &QToolButton::clicked, this, &swPreferencesDialog::ColorButtonClicked);
	connect(ui->AxisZNegativeColorButton, &QToolButton::clicked, this, &swPreferencesDialog::ColorButtonClicked);
	connect(ui->ObjectSelectedColorButton, &QToolButton::clicked, this, &swPreferencesDialog::ColorButtonClicked);
	connect(ui->ObjectFocusedColorButton, &QToolButton::clicked, this, &swPreferencesDialog::ColorButtonClicked);
	connect(ui->CameraColorButton, &QToolButton::clicked, this, &swPreferencesDialog::ColorButtonClicked);
	connect(ui->LightColorButton, &QToolButton::clicked, this, &swPreferencesDialog::ColorButtonClicked);
	connect(ui->ControlPointColorButton, &QToolButton::clicked, this, &swPreferencesDialog::ColorButtonClicked);
	connect(ui->ControlPointSeatedColorButton, &QToolButton::clicked, this, &swPreferencesDialog::ColorButtonClicked);
	connect(ui->SpreadHandleColorButton, &QToolButton::clicked, this, &swPreferencesDialog::ColorButtonClicked);
	connect(ui->ControlPointFocusedColorButton, &QToolButton::clicked, this, &swPreferencesDialog::ColorButtonClicked);
	connect(ui->PathColorButton, &QToolButton::clicked, this, &swPreferencesDialog::ColorButtonClicked);
	connect(ui->PathOutOfReachColorButton, &QToolButton::clicked, this, &swPreferencesDialog::ColorButtonClicked);
	connect(ui->categoriesTree, &QTreeWidget::itemSelectionChanged, this, &swPreferencesDialog::UpdateParts);
	connect(ui->categoriesTree, &QTreeWidget::itemChanged, this, &swPreferencesDialog::CategoryItemChanged);
	connect(ui->categoriesTree->model(), &QAbstractItemModel::rowsInserted, this, &swPreferencesDialog::CategoriesDropped);
	ui->shortcutEdit->installEventFilter(this);
	ui->MouseShortcutEdit->installEventFilter(this);
	connect(ui->commandList, &QTreeWidget::currentItemChanged, this, &swPreferencesDialog::CommandChanged);
	connect(ui->mouseTree, &QTreeWidget::currentItemChanged, this, &swPreferencesDialog::MouseTreeItemChanged);
	connect(ui->HighContrastButton, &QToolButton::clicked, this, &swPreferencesDialog::AutomateEdgeColor);

	ui->partsLibrary->setText(mOptions->LibraryPath);
	ui->UnofficialLibraryEdit->setText(mOptions->UnofficialLibraryPath);
	ui->ShadowLibraryEdit->setText(mOptions->ShadowLibraryPath);
	ui->ShadowLibraryEnabled->setChecked(mOptions->Preferences.mShadowLibraryEnabled);
	ShadowLibraryEnabledToggled(mOptions->Preferences.mShadowLibraryEnabled);
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
	ui->FlexSpreadMode->setCurrentIndex(static_cast<int>(mOptions->Preferences.mFlexSpreadMode));
	ui->FlexClampNewControlPoints->setChecked(mOptions->Preferences.mFlexClampNewControlPoints);
	ui->FlexMiddleControlPoint->setChecked(mOptions->Preferences.mFlexMiddleControlPoint);
	ui->WatchFileChanges->setChecked(mOptions->Preferences.mWatchFileChanges);
	ui->LoadPartsBeforeShowing->setChecked(mOptions->Preferences.mLoadPartsBeforeShowing);
	ui->MigrateMetaCommands->setChecked(mOptions->Preferences.mMigrateMetaCommands);
	ui->MarkColladaTransparency->setChecked(mOptions->Preferences.mMarkColladaTransparency);
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

	// fit the value label to the widest
	const QString MeshLODWidest = tr("%1 ldu").arg(swFormatIntegerLocalized(static_cast<int>(ui->MeshLODSlider->maximum() * mMeshLODMultiplier), locale()));
	ui->MeshLODLabel->setMinimumWidth(ui->MeshLODLabel->fontMetrics().horizontalAdvance(MeshLODWidest) + 2 * ui->MeshLODLabel->margin());

	ui->FadeSteps->setChecked(mOptions->Preferences.mFadeSteps);
	ui->HighlightNewParts->setChecked(mOptions->Preferences.mHighlightNewParts);
	ui->gridStuds->setChecked(mOptions->Preferences.mDrawGridStuds);
	ui->gridLines->setChecked(mOptions->Preferences.mDrawGridLines);
	ui->gridLineSpacing->setRange(1, SW_MAXIMUM_GRID_LINE_SPACING);
	ui->gridLineSpacing->setValue(mOptions->Preferences.mGridLineSpacing);
	UpdateStudSuffix(ui->gridLineSpacing);
	ui->GridOriginCheckBox->setChecked(mOptions->Preferences.mDrawGridOrigin);
	ui->GridNegativeAxesCheckBox->setChecked(mOptions->Preferences.mDrawGridNegativeAxes);
	ui->GridHeightXZCheckBox->setChecked(mOptions->Preferences.mDrawGridHeightXZ);
	ui->GridHeightYZCheckBox->setChecked(mOptions->Preferences.mDrawGridHeightYZ);
	ui->GridHeightBehindCheckBox->setChecked(mOptions->Preferences.mGridHeightBehindModel);
	ui->GridAxisXCheckBox->setChecked(mOptions->Preferences.mDrawGridAxisX);
	ui->GridAxisYCheckBox->setChecked(mOptions->Preferences.mDrawGridAxisY);
	ui->GridAxisZCheckBox->setChecked(mOptions->Preferences.mDrawGridAxisZ);
	ui->AxisIconSizeCombo->setCurrentIndex(mOptions->Preferences.mDrawAxes ? swSizeIndex(mOptions->Preferences.mAxisIconSize, SW_AXIS_ICON_SIZES) : 0);
	ui->FollowStepViewsCheckBox->setChecked(mOptions->Preferences.mFollowStepViews);
	ui->ProjectionScopeCombo->setCurrentIndex(static_cast<int>(mOptions->Preferences.mProjectionScope));
	ui->CutawaySizeSpinBox->setValue(mOptions->Preferences.mCutawaySize);
	UpdateStudSuffix(ui->CutawaySizeSpinBox);
	ui->CutawayShadeAlphaSlider->setValue(mOptions->Preferences.mCutawayShadeAlpha);
	CutawayShadeAlphaChanged();
	ui->TurntableIdleDurationSpinBox->SetReference(ui->TurntableIdleDurationSpinBox->suffix());
	ui->TurntableIdleDurationSpinBox->setValue(mOptions->Preferences.mTurntableIdleDuration);
	ui->TurntableTurnTimeSpinBox->SetReference(ui->TurntableTurnTimeSpinBox->suffix());
	ui->TurntableTurnTimeSpinBox->setValue(mOptions->Preferences.mTurntableTurnTime);
	ui->TurntableAxisCombo->setCurrentIndex(static_cast<int>(mOptions->Preferences.mTurntableAxis));

	ui->AxisIconLocationCombo->setCurrentIndex((int)mOptions->Preferences.mAxisIconLocation);
	ui->AxisIconLabelsCheckBox->setChecked(mOptions->Preferences.mAxisIconLabels);
	ui->AxisIconNegativeCheckBox->setChecked(mOptions->Preferences.mAxisIconNegative);
	ui->AxisIconCubeOutlineCheckBox->setChecked(mOptions->Preferences.mAxisIconCubeOutline);
	ui->AxisIconCubeOutlineAlphaSlider->setValue(mOptions->Preferences.mAxisIconCubeOutlineAlpha);
	AxisIconCubeOutlineAlphaChanged();

	ui->ViewCubeLocationCombo->setCurrentIndex((int)mOptions->Preferences.mViewCubeLocation);

	ui->ViewCubeSizeCombo->setCurrentIndex(mOptions->Preferences.mViewCubeEnabled ? swSizeIndex(mOptions->Preferences.mViewCubeSize, SW_VIEW_CUBE_SIZES) : 0);

	ui->PreviewAxisIconSizeCombo->setCurrentIndex(mOptions->Preferences.mDrawPreviewAxis ? swSizeIndex(mOptions->Preferences.mPreviewAxisIconSize, SW_AXIS_ICON_SIZES) : 0);
	ui->PreviewSquareViewportCheckBox->setChecked(mOptions->Preferences.mPreviewSquareViewport);

	ui->PreviewViewCubeLocationCombo->setCurrentIndex((int)mOptions->Preferences.mPreviewViewCubeLocation);

	ui->PreviewViewCubeSizeCombo->setCurrentIndex(mOptions->Preferences.mPreviewViewCubeEnabled ? swSizeIndex(mOptions->Preferences.mPreviewViewCubeSize, SW_VIEW_CUBE_SIZES) : 0);

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

	if (!swContext::SupportsMaterial(swMaterialType::UnlitViewCube))
	{
		const std::array<QWidget*, 7> Controls =
		{
			ui->ViewCubeSizeCombo, ui->ViewCubeLocationCombo,
			ui->PreviewViewCubeSizeCombo, ui->PreviewViewCubeLocationCombo,
			ui->ViewCubeColorButton, ui->ViewCubeTextColorButton, ui->ViewCubeHighlightColorButton
		};
		const QString Message = tr("View cube rendering is unavailable.");

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
	ui->SelectionOutlineWidth->SetReference(ui->SelectionOutlineWidth->suffix());
	ui->SelectionOutlineWidth->setValue(mOptions->Preferences.mSelectionOutlineWidth);

	// width applies to outlines only
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
		// swatch beside the color name
		Button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
		swUpdateColorButton(Button, QColor(SW_RGBA_RED(Color), SW_RGBA_GREEN(Color), SW_RGBA_BLUE(Color), SW_RGBA_ALPHA(Color)));
	};

	SetButtonPixmap(mOptions->Preferences.mBackgroundSolidColor, ui->BackgroundSolidColorButton);
	SetButtonPixmap(mOptions->Preferences.mTextColor, ui->TextColorButton);
	SetButtonPixmap(mOptions->Preferences.mTextHaloColor, ui->TextHaloColorButton);
	SetButtonPixmap(mOptions->Preferences.mMarqueeBorderColor, ui->MarqueeBorderColorButton);
	SetButtonPixmap(mOptions->Preferences.mMarqueeFillColor, ui->MarqueeFillColorButton);
	SetButtonPixmap(mOptions->Preferences.mOverlayColor, ui->OverlayColorButton);
	SetButtonPixmap(mOptions->Preferences.mCutawayShadeColor | SW_RGBA(0, 0, 0, 255), ui->CutawayShadeColorButton);
	SetButtonPixmap(mOptions->Preferences.mActiveViewColor, ui->ActiveViewColorButton);
	SetButtonPixmap(mOptions->Preferences.mInactiveViewColor, ui->InactiveViewColorButton);
	SetButtonPixmap(mOptions->Preferences.mFadeStepsColor, ui->FadeStepsColor);
	SetButtonPixmap(mOptions->Preferences.mHighlightNewPartsColor, ui->HighlightNewPartsColor);
	SetButtonPixmap(mOptions->Preferences.mGridStudColor, ui->gridStudColor);
	SetButtonPixmap(mOptions->Preferences.mGridLineColor, ui->gridLineColor);
	SetButtonPixmap(mOptions->Preferences.mViewCubeColor, ui->ViewCubeColorButton);
	SetButtonPixmap(mOptions->Preferences.mViewCubeTextColor, ui->ViewCubeTextColorButton);
	SetButtonPixmap(mOptions->Preferences.mViewCubeHighlightColor, ui->ViewCubeHighlightColorButton);
	SetButtonPixmap(mOptions->Preferences.mAxisXColor, ui->AxisXColorButton);
	SetButtonPixmap(mOptions->Preferences.mAxisYColor, ui->AxisYColorButton);
	SetButtonPixmap(mOptions->Preferences.mAxisZColor, ui->AxisZColorButton);
	SetButtonPixmap(mOptions->Preferences.mAxisXNegativeColor, ui->AxisXNegativeColorButton);
	SetButtonPixmap(mOptions->Preferences.mAxisYNegativeColor, ui->AxisYNegativeColorButton);
	SetButtonPixmap(mOptions->Preferences.mAxisZNegativeColor, ui->AxisZNegativeColorButton);
	SetButtonPixmap(mOptions->Preferences.mObjectSelectedColor, ui->ObjectSelectedColorButton);
	SetButtonPixmap(mOptions->Preferences.mObjectFocusedColor, ui->ObjectFocusedColorButton);
	SetButtonPixmap(mOptions->Preferences.mCameraColor, ui->CameraColorButton);
	SetButtonPixmap(mOptions->Preferences.mLightColor, ui->LightColorButton);
	SetButtonPixmap(mOptions->Preferences.mControlPointColor, ui->ControlPointColorButton);
	SetButtonPixmap(mOptions->Preferences.mControlPointFocusedColor, ui->ControlPointFocusedColorButton);
	SetButtonPixmap(mOptions->Preferences.mControlPointSeatedColor, ui->ControlPointSeatedColorButton);
	SetButtonPixmap(mOptions->Preferences.mSpreadHandleColor, ui->SpreadHandleColorButton);
	SetButtonPixmap(mOptions->Preferences.mPathColor, ui->PathColorButton);
	SetButtonPixmap(mOptions->Preferences.mPathOutOfReachColor, ui->PathOutOfReachColorButton);

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
	ViewCubeSizeComboCurrentIndexChanged(ui->ViewCubeSizeCombo->currentIndex());
	PreviewViewCubeSizeComboCurrentIndexChanged(ui->PreviewViewCubeSizeCombo->currentIndex());
	AxisIconSizeComboCurrentIndexChanged(ui->AxisIconSizeCombo->currentIndex());

	UpdateCategories();
	ui->categoriesTree->setCurrentItem(ui->categoriesTree->topLevelItem(0));

	// keep the categories width on resize
	ui->splitter->setStretchFactor(0, 0);
	ui->splitter->setStretchFactor(1, 1);
	connect(ui->tabWidget, &QTabWidget::currentChanged, this, &swPreferencesDialog::SnapCategoriesTree);

	// snap the categories back on release
	ui->splitter->handle(1)->installEventFilter(this);

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

	// line up columns once shown
	swAlignPreferencesColumns(ui->tabWidget);

	// grow digits to the left
	ui->MeshLODLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
}

void swPreferencesDialog::accept()
{
	const int GridLineSpacing = ui->gridLineSpacing->value();

	mOptions->LibraryPath = ui->partsLibrary->text();
	mOptions->UnofficialLibraryPath = ui->UnofficialLibraryEdit->text();
	mOptions->ShadowLibraryPath = ui->ShadowLibraryEdit->text();
	mOptions->Preferences.mShadowLibraryEnabled = ui->ShadowLibraryEnabled->isChecked();
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
	mOptions->Preferences.mFlexSpreadMode = static_cast<swFlexSpreadMode>(ui->FlexSpreadMode->currentIndex());
	mOptions->Preferences.mFlexClampNewControlPoints = ui->FlexClampNewControlPoints->isChecked();
	mOptions->Preferences.mFlexMiddleControlPoint = ui->FlexMiddleControlPoint->isChecked();
	mOptions->Preferences.mWatchFileChanges = ui->WatchFileChanges->isChecked();
	mOptions->Preferences.mLoadPartsBeforeShowing = ui->LoadPartsBeforeShowing->isChecked();
	mOptions->Preferences.mMigrateMetaCommands = ui->MigrateMetaCommands->isChecked();
	mOptions->Preferences.mMarkColladaTransparency = ui->MarkColladaTransparency->isChecked();

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
	mOptions->Preferences.mDrawGridNegativeAxes = ui->GridNegativeAxesCheckBox->isChecked();
	mOptions->Preferences.mDrawGridHeightXZ = ui->GridHeightXZCheckBox->isChecked();
	mOptions->Preferences.mDrawGridHeightYZ = ui->GridHeightYZCheckBox->isChecked();
	mOptions->Preferences.mGridHeightBehindModel = ui->GridHeightBehindCheckBox->isChecked();
	mOptions->Preferences.mDrawGridAxisX = ui->GridAxisXCheckBox->isChecked();
	mOptions->Preferences.mDrawGridAxisY = ui->GridAxisYCheckBox->isChecked();
	mOptions->Preferences.mDrawGridAxisZ = ui->GridAxisZCheckBox->isChecked();

	mOptions->Preferences.mDrawAxes = ui->AxisIconSizeCombo->currentIndex() > 0;

	if (ui->AxisIconSizeCombo->currentIndex() > 0)
		mOptions->Preferences.mAxisIconSize = SW_AXIS_ICON_SIZES[ui->AxisIconSizeCombo->currentIndex() - 1];

	mOptions->Preferences.mFollowStepViews = ui->FollowStepViewsCheckBox->isChecked();
	mOptions->Preferences.mProjectionScope = static_cast<swProjectionScope>(ui->ProjectionScopeCombo->currentIndex());
	mOptions->Preferences.mCutawaySize = ui->CutawaySizeSpinBox->value();
	mOptions->Preferences.mCutawayShadeAlpha = ui->CutawayShadeAlphaSlider->value();
	mOptions->Preferences.mTurntableIdleDuration = ui->TurntableIdleDurationSpinBox->value();
	mOptions->Preferences.mTurntableTurnTime = ui->TurntableTurnTimeSpinBox->value();
	mOptions->Preferences.mTurntableAxis = static_cast<swTurntableAxis>(ui->TurntableAxisCombo->currentIndex());
	mOptions->Preferences.mAxisIconLocation = (swAxisIconLocation)ui->AxisIconLocationCombo->currentIndex();
	mOptions->Preferences.mAxisIconLabels = ui->AxisIconLabelsCheckBox->isChecked();
	mOptions->Preferences.mAxisIconNegative = ui->AxisIconNegativeCheckBox->isChecked();
	mOptions->Preferences.mAxisIconCubeOutline = ui->AxisIconCubeOutlineCheckBox->isChecked();
	mOptions->Preferences.mAxisIconCubeOutlineAlpha = ui->AxisIconCubeOutlineAlphaSlider->value();
	mOptions->Preferences.mViewCubeEnabled = ui->ViewCubeSizeCombo->currentIndex() > 0;
	mOptions->Preferences.mViewCubeLocation = (swViewCubeLocation)ui->ViewCubeLocationCombo->currentIndex();

	if (ui->ViewCubeSizeCombo->currentIndex() > 0)
		mOptions->Preferences.mViewCubeSize = SW_VIEW_CUBE_SIZES[ui->ViewCubeSizeCombo->currentIndex() - 1];

	mOptions->Preferences.mShadingMode = (swShadingMode)ui->ShadingMode->currentIndex();
	mOptions->Preferences.mLightingStyle = static_cast<swLightingStyle>(ui->LightingStyle->currentIndex());
	mOptions->Preferences.mHighlightRenderMode = static_cast<swHighlightRenderMode>(ui->HighlightRenderMode->currentIndex());
	mOptions->Preferences.mSelectionOutlineWidth = ui->SelectionOutlineWidth->value();

	mOptions->StudStyle = static_cast<swStudStyle>(ui->studStyleCombo->currentIndex());

	mOptions->Preferences.mDrawPreviewAxis = ui->PreviewAxisIconSizeCombo->currentIndex() > 0;

	if (ui->PreviewAxisIconSizeCombo->currentIndex() > 0)
		mOptions->Preferences.mPreviewAxisIconSize = SW_AXIS_ICON_SIZES[ui->PreviewAxisIconSizeCombo->currentIndex() - 1];

	mOptions->Preferences.mPreviewSquareViewport = ui->PreviewSquareViewportCheckBox->isChecked();
	mOptions->Preferences.mPreviewViewCubeEnabled = ui->PreviewViewCubeSizeCombo->currentIndex() > 0;
	mOptions->Preferences.mPreviewViewCubeLocation = (swViewCubeLocation)ui->PreviewViewCubeLocationCombo->currentIndex();

	if (ui->PreviewViewCubeSizeCombo->currentIndex() > 0)
		mOptions->Preferences.mPreviewViewCubeSize = SW_VIEW_CUBE_SIZES[ui->PreviewViewCubeSizeCombo->currentIndex() - 1];

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

// use to choose a parts library
void swPreferencesDialog::PartsLibraryBrowseClicked()
{
	QString Result = QFileDialog::getOpenFileName(this, tr("Select Parts Library"), ui->partsLibrary->text(), tr("Parts Libraries (*.zip *.bin LDConfig.ldr ldconfig.ldr);;All Files (*.*)"));

	if (Result.isEmpty())
		return;

	// ldconfig stands for its folder
	const QFileInfo ResultInfo(Result);

	if (ResultInfo.fileName().compare(QLatin1String("ldconfig.ldr"), Qt::CaseInsensitive) == 0)
		Result = ResultInfo.absolutePath();

	ui->partsLibrary->setText(QDir::toNativeSeparators(Result));
}

void swPreferencesDialog::UnofficialLibraryBrowseButtonClicked()
{
	QString Result = QFileDialog::getOpenFileName(this, tr("Select Unofficial Library Archive"), ui->UnofficialLibraryEdit->text(), tr("Supported Archives (*.zip);;All Files (*.*)"));

	if (!Result.isEmpty())
		ui->UnofficialLibraryEdit->setText(QDir::toNativeSeparators(Result));
}

void swPreferencesDialog::ShadowLibraryBrowseButtonClicked()
{
	QString Result = QFileDialog::getExistingDirectory(this, tr("Select Part Connections Folder"), ui->ShadowLibraryEdit->text());

	if (!Result.isEmpty())
		ui->ShadowLibraryEdit->setText(QDir::toNativeSeparators(Result));
}

void swPreferencesDialog::ShadowLibraryEnabledToggled(bool Checked)
{
	ui->ShadowLibraryEdit->setEnabled(Checked);
	ui->ShadowLibraryBrowseButton->setEnabled(Checked);
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
	else if (Button == ui->CutawayShadeColorButton)
	{
		Color = &mOptions->Preferences.mCutawayShadeColor;
		Title = tr("Select Cutaway Shade Color");
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
	else if (Button == ui->ViewCubeColorButton)
	{
		Color = &mOptions->Preferences.mViewCubeColor;
		Title = tr("Select View Cube Color");
	}
	else if (Button == ui->ViewCubeTextColorButton)
	{
		Color = &mOptions->Preferences.mViewCubeTextColor;
		Title = tr("Select View Cube Text Color");
	}
	else if (Button == ui->ViewCubeHighlightColorButton)
	{
		Color = &mOptions->Preferences.mViewCubeHighlightColor;
		Title = tr("Select View Cube Highlight Color");
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
	else if (Button == ui->AxisXNegativeColorButton)
	{
		Color = &mOptions->Preferences.mAxisXNegativeColor;
		Title = tr("Select Negative X Axis Color");
	}
	else if (Button == ui->AxisYNegativeColorButton)
	{
		Color = &mOptions->Preferences.mAxisYNegativeColor;
		Title = tr("Select Negative Y Axis Color");
	}
	else if (Button == ui->AxisZNegativeColorButton)
	{
		Color = &mOptions->Preferences.mAxisZNegativeColor;
		Title = tr("Select Negative Z Axis Color");
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
	else if (Button == ui->ControlPointSeatedColorButton)
	{
		Color = &mOptions->Preferences.mControlPointSeatedColor;
		Title = tr("Select Control Point Seated Color");
		DialogOptions = QColorDialog::ShowAlphaChannel;
	}
	else if (Button == ui->SpreadHandleColorButton)
	{
		Color = &mOptions->Preferences.mSpreadHandleColor;
		Title = tr("Select Flex Path Spread Handle Color");
		DialogOptions = QColorDialog::ShowAlphaChannel;
	}
	else if (Button == ui->PathColorButton)
	{
		Color = &mOptions->Preferences.mPathColor;
		Title = tr("Select Flex Path Color");
		DialogOptions = QColorDialog::ShowAlphaChannel;
	}
	else if (Button == ui->PathOutOfReachColorButton)
	{
		Color = &mOptions->Preferences.mPathOutOfReachColor;
		Title = tr("Select Flex Path Out of Reach Color");
		DialogOptions = QColorDialog::ShowAlphaChannel;
	}
	else
		return;

	QColor OldColor = QColor(SW_RGBA_RED(*Color), SW_RGBA_GREEN(*Color), SW_RGBA_BLUE(*Color), SW_RGBA_ALPHA(*Color));
	QColor NewColor = QColorDialog::getColor(OldColor, this, Title, DialogOptions);

	if (NewColor == OldColor || !NewColor.isValid())
		return;

	*Color = SW_RGBA(NewColor.red(), NewColor.green(), NewColor.blue(), NewColor.alpha());

	// hide too transparent highlights
	if (Button == ui->HighlightNewPartsColor || Button == ui->FadeStepsColor)
	{
		*Color = swClampStepColorAlpha(*Color);
		NewColor.setAlpha(SW_RGBA_ALPHA(*Color));
	}

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

// use to show the maximum stud count
void swPreferencesDialog::UpdateStudSuffix(QSpinBox* SpinBox)
{
	SpinBox->setSuffix(QLatin1String(" / ") + tr("%n stud(s)", nullptr, SpinBox->maximum()));
}

// use to show the cube outline alpha
void swPreferencesDialog::AxisIconCubeOutlineAlphaChanged()
{
	ui->AxisIconCubeOutlineAlphaValueLabel->setText(locale().toString(ui->AxisIconCubeOutlineAlphaSlider->value() / 100.0, 'f', 2));
}

// use to show the cutaway shade alpha
void swPreferencesDialog::CutawayShadeAlphaChanged()
{
	ui->CutawayShadeAlphaValueLabel->setText(locale().toString(ui->CutawayShadeAlphaSlider->value() / 100.0, 'f', 2));
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
	ui->MeshLODLabel->setText(tr("%1 ldu").arg(swFormatIntegerLocalized(static_cast<int>(Value), locale())));
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

void swPreferencesDialog::PreviewViewCubeSizeComboCurrentIndexChanged(int Index)
{
	ui->PreviewViewCubeLocationCombo->setEnabled(Index != 0 && swContext::SupportsMaterial(swMaterialType::UnlitViewCube));
}

void swPreferencesDialog::ViewCubeSizeComboCurrentIndexChanged(int Index)
{
	ui->ViewCubeLocationCombo->setEnabled(Index != 0 && swContext::SupportsMaterial(swMaterialType::UnlitViewCube));
}

void swPreferencesDialog::AxisIconSizeComboCurrentIndexChanged(int Index)
{
	ui->AxisIconLocationCombo->setEnabled(Index != 0);
	ui->AxisIconLabelsCheckBox->setEnabled(Index != 0);
	ui->AxisIconNegativeCheckBox->setEnabled(Index != 0);
	ui->AxisIconCubeOutlineCheckBox->setEnabled(Index != 0);
	ui->AxisIconCubeOutlineAlphaLabel->setEnabled(Index != 0 && ui->AxisIconCubeOutlineCheckBox->isChecked());
	ui->AxisIconCubeOutlineAlphaSlider->setEnabled(Index != 0 && ui->AxisIconCubeOutlineCheckBox->isChecked());
	ui->AxisIconCubeOutlineAlphaValueLabel->setEnabled(Index != 0 && ui->AxisIconCubeOutlineCheckBox->isChecked());
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

		// light dark is the edge saturation
		ui->EdgeSaturationSlider->setValue(qRound(mOptions->Preferences.mPartColorValueLDIndex * 100.0f));
	}
}
