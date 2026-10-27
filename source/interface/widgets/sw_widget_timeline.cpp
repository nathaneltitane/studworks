#include "sw_global.h"
#include "sw_widget_timeline.h"
#include "sw_model.h"
#include "sw_object_part.h"
#include "sw_data_part.h"
#include "sw_window_main.h"

swTimelineWidget::swTimelineWidget(QWidget* Parent)
	: QTreeWidget(Parent)
{
	setAccessibleName(tr("Building Steps and Parts"));
	setToolTip(tr("Select steps or parts; drag parts to reorder them or change their building step."));
	mCurrentStepItem = nullptr;
	mIgnoreUpdates = false;

	setSelectionMode(QAbstractItemView::ExtendedSelection);
	setDragEnabled(true);
	setDragDropMode(QAbstractItemView::InternalMove);
	setUniformRowHeights(true);
	setHeaderHidden(true);
	setContextMenuPolicy(Qt::CustomContextMenu);

	invisibleRootItem()->setFlags(invisibleRootItem()->flags() & ~Qt::ItemIsDropEnabled);

	connect(this, &QTreeWidget::currentItemChanged, this, &swTimelineWidget::CurrentItemChanged);
	connect(this, &QTreeWidget::itemSelectionChanged, this, &swTimelineWidget::ItemSelectionChanged);
	connect(this, &QWidget::customContextMenuRequested, this, &swTimelineWidget::CustomMenuRequested);
	connect(this, &QTreeWidget::itemDoubleClicked, this, [](QTreeWidgetItem* Item)
	{
		const QVariant ViewStep = Item ? Item->data(0, Qt::UserRole + 1) : QVariant();
		swModel* Model = gMainWindow->GetActiveModel();

		if (!ViewStep.isValid() || !Model)
			return;

		Model->SetCurrentStep(ViewStep.toUInt());
		gMainWindow->HandleCommand(SW_VIEW_STEP_VIEW_EDIT);
	});
}

swTimelineWidget::~swTimelineWidget()
{
}

void swTimelineWidget::CustomMenuRequested(QPoint Pos)
{
	// Right-clicking a step view row makes its step current, so the step view entries act on it.
	QTreeWidgetItem* ClickedItem = itemAt(Pos);
	const QVariant ViewStep = ClickedItem ? ClickedItem->data(0, Qt::UserRole + 1) : QVariant();

	if (ViewStep.isValid())
		gMainWindow->GetActiveModel()->SetCurrentStep(ViewStep.toUInt());

	QMenu* Menu = new QMenu(this);

	swObject* FocusObject = gMainWindow->GetActiveModel()->GetFocusObject();

	if (FocusObject && FocusObject->IsPart())
	{
		swPart* Part = (swPart*)FocusObject;

		if (Part->mPartData->IsModel())
		{
			Menu->addAction(gMainWindow->mActions[SW_PART_EDIT_SELECTED_SUBMODEL]);
			Menu->addAction(gMainWindow->mActions[SW_PART_VIEW_SELECTED_MODEL]);
			Menu->addAction(gMainWindow->mActions[SW_PART_INLINE_SELECTED_MODELS]);
			Menu->addSeparator();
		}
	}

	Menu->addAction(gMainWindow->mActions[SW_TIMELINE_SET_CURRENT]);
	Menu->addAction(gMainWindow->mActions[SW_TIMELINE_INSERT_BEFORE]);
	Menu->addAction(gMainWindow->mActions[SW_TIMELINE_INSERT_AFTER]);
	Menu->addAction(gMainWindow->mActions[SW_TIMELINE_DELETE]);
	Menu->addAction(gMainWindow->mActions[SW_VIEW_STEP_VIEW_EDIT]);

	// Removing is only offered on a step view row.
	if (ViewStep.isValid())
		Menu->addAction(gMainWindow->mActions[SW_VIEW_STEP_VIEW_REMOVE]);

	Menu->addSeparator();

	Menu->addAction(gMainWindow->mActions[SW_TIMELINE_MOVE_SELECTION]);
	Menu->addAction(gMainWindow->mActions[SW_TIMELINE_MOVE_SELECTION_BEFORE]);
	Menu->addAction(gMainWindow->mActions[SW_TIMELINE_MOVE_SELECTION_AFTER]);

	Menu->addSeparator();

	Menu->addAction(gMainWindow->mActions[SW_PART_HIDE_SELECTED]);
	Menu->addAction(gMainWindow->mActions[SW_PART_HIDE_UNSELECTED]);
	Menu->addAction(gMainWindow->mActions[SW_PART_UNHIDE_SELECTED]);
	Menu->addAction(gMainWindow->mActions[SW_PART_UNHIDE_ALL]);

	Menu->exec(viewport()->mapToGlobal(Pos));
	delete Menu;
}

void swTimelineWidget::Update(bool Clear, bool UpdateItems)
{
	if (mIgnoreUpdates)
		return;

	swModel* Model = gMainWindow->GetActiveModel();

	bool Blocked = blockSignals(true);

	if (!Model)
	{
		mCurrentStepItem = nullptr;
		mItems.clear();
		clear();
		blockSignals(Blocked);
		return;
	}

	if (Clear)
	{
		mCurrentStepItem = nullptr;
		mItems.clear();
		clear();
	}

	// Step view rows are rebuilt after the part rows.
	for (int TopLevelItemIdx = 0; TopLevelItemIdx < topLevelItemCount(); TopLevelItemIdx++)
	{
		QTreeWidgetItem* StepItem = topLevelItem(TopLevelItemIdx);

		for (int ChildIdx = StepItem->childCount() - 1; ChildIdx >= 0; ChildIdx--)
			if (StepItem->child(ChildIdx)->data(0, Qt::UserRole + 1).isValid())
				delete StepItem->child(ChildIdx);
	}

	swStep LastStep = Model->GetLastStep();
	if (Model->HasParts())
		LastStep++;
	LastStep  = swMaximum(LastStep, Model->GetCurrentStep());

	for (int TopLevelItemIdx = LastStep; TopLevelItemIdx < topLevelItemCount(); )
	{
		QTreeWidgetItem* StepItem = topLevelItem(TopLevelItemIdx);

		while (StepItem->childCount())
		{
			QTreeWidgetItem* PartItem = StepItem->child(0);
			swPart* Part = (swPart*)PartItem->data(0, Qt::UserRole).value<uintptr_t>();
			mItems.remove(Part);
			delete PartItem;
		}

		if (mCurrentStepItem == StepItem)
			mCurrentStepItem = nullptr;

		delete StepItem;
	}

	for (unsigned int TopLevelItemIdx = topLevelItemCount(); TopLevelItemIdx < LastStep; TopLevelItemIdx++)
	{
		QTreeWidgetItem* StepItem = new QTreeWidgetItem(this, QStringList(tr("Step %1").arg(TopLevelItemIdx + 1)));
		StepItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsDropEnabled);
		addTopLevelItem(StepItem);
		StepItem->setExpanded(true);
	}

	const std::vector<std::unique_ptr<swPart>>& Parts = Model->GetParts();
	QTreeWidgetItem* StepItem = nullptr;
	int PartItemIndex = 0;
	swStep Step = 0;

	for (const std::unique_ptr<swPart>& Part : Parts)
	{
		while (Step != Part->GetStepShow())
		{
			if (StepItem)
			{
				while (PartItemIndex < StepItem->childCount())
				{
					QTreeWidgetItem* PartItem = StepItem->child(PartItemIndex);
					swPart* RemovePart = (swPart*)PartItem->data(0, Qt::UserRole).value<uintptr_t>();
					auto RemoveIt = std::find_if(Parts.begin(), Parts.end(), [RemovePart](const std::unique_ptr<swPart>& CheckPart){ return CheckPart.get() == RemovePart; });

					if (RemoveIt == Parts.end())
					{
						mItems.remove(RemovePart);
						delete PartItem;
					}
					else
					{
						PartItem->parent()->removeChild(PartItem);
						topLevelItem(RemovePart->GetStepShow() - 1)->addChild(PartItem);
					}
				}
			}

			Step++;
			StepItem = topLevelItem(Step - 1);
			PartItemIndex = 0;
		}

		QTreeWidgetItem* PartItem = mItems.value(Part.get());
		bool UpdateItem = UpdateItems;

		if (StepItem)
		{
			if (!PartItem)
			{
				PartItem = new QTreeWidgetItem();
				PartItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled);
				PartItem->setData(0, Qt::UserRole, QVariant::fromValue<uintptr_t>((uintptr_t)Part.get()));
				StepItem->insertChild(PartItemIndex, PartItem);
				mItems[Part.get()] = PartItem;

				UpdateItem = true;
			}
			else
			{
				if (PartItemIndex >= StepItem->childCount() || PartItem != StepItem->child(PartItemIndex))
				{
					QTreeWidgetItem* PartParent = PartItem->parent();

					if (PartParent)
						PartParent->removeChild(PartItem);

					StepItem->insertChild(PartItemIndex, PartItem);
				}
			}
		}

		if (UpdateItem)
		{
			PartItem->setText(0, Part->mPartData->m_strDescription);

			int ColorIndex = Part->GetColorIndex();
			if (!mIcons.contains(ColorIndex))
			{
				int Size = rowHeight(indexFromItem(PartItem));

				QImage Image(Size, Size, QImage::Format_ARGB32);
				Image.fill(0);
				const swVector4& Color = gColorList[ColorIndex].Value;
				QPainter Painter(&Image);
				Painter.setPen(Qt::darkGray);
				Painter.setBrush(QColor::fromRgbF(Color[0], Color[1], Color[2]));
				Painter.drawEllipse(0, 0, Size - 1, Size - 1);

				mIcons[ColorIndex] = QIcon(QPixmap::fromImage(Image));
			}

			PartItem->setIcon(0, mIcons[ColorIndex]);

			QColor Color = palette().text().color();

			if (Part->mPartData->mState == swAssetState::Failed)
				Color = QColor(208, 0, 0);
			else if (Part->IsHidden())
				Color.setAlpha(128);

			PartItem->setForeground(0, Color);
		}

		PartItem->setSelected(Part->IsSelected());
		PartItemIndex++;
	}

	if (Step == 0)
	{
		Step = 1;
		StepItem = topLevelItem(0);
	}

	while (Step <= LastStep && StepItem)
	{
		while (PartItemIndex < StepItem->childCount())
		{
			QTreeWidgetItem* PartItem = StepItem->child(PartItemIndex);
			swPart* RemovePart = (swPart*)PartItem->data(0, Qt::UserRole).value<uintptr_t>();

			mItems.remove(RemovePart);
			delete PartItem;
		}

		Step++;
		StepItem = topLevelItem(Step - 1);
		PartItemIndex = 0;
	}

	// A step view is its own row closing the step, like the ROTSTEP line that ends it.
	const std::map<swStep, swStepView>& StepViews = Model->GetProperties().mStepViews;

	for (const std::pair<const swStep, swStepView>& StepView : StepViews)
	{
		QTreeWidgetItem* ViewStepItem = StepView.first <= static_cast<swStep>(topLevelItemCount()) ? topLevelItem(StepView.first - 1) : nullptr;

		if (!ViewStepItem)
			continue;

		const QString Modes[] = { tr("relative"), tr("absolute"), tr("added") };
		const swVector3& Angles = StepView.second.Angles;
		QString Label;

		if (StepView.second.Mode == swStepViewMode::End)
			Label = tr("View: back to default");
		else
			Label = tr("View: %1, %2, %3 %4").arg(Angles.x).arg(Angles.y).arg(Angles.z).arg(Modes[static_cast<int>(StepView.second.Mode)]);

		QTreeWidgetItem* ViewItem = new QTreeWidgetItem(QStringList(Label));
		ViewItem->setFlags(Qt::ItemIsEnabled);
		ViewItem->setData(0, Qt::UserRole + 1, QVariant::fromValue<uint>(StepView.first));
		ViewItem->setToolTip(0, tr("Step view saved as an MLCAD ROTSTEP line. Double-click to edit it."));

		ViewStepItem->addChild(ViewItem);
	}

	UpdateCurrentStepItem();

	blockSignals(Blocked);
}

void swTimelineWidget::UpdateCurrentStepItem()
{
	swModel* Model = gMainWindow->GetActiveModel();
	swStep CurrentStep = Model->GetCurrentStep();
	QTreeWidgetItem* CurrentStepItem = topLevelItem(CurrentStep - 1);

	if (CurrentStepItem != mCurrentStepItem)
	{
		if (mCurrentStepItem)
		{
			QFont Font = mCurrentStepItem->font(0);
			Font.setBold(false);
			mCurrentStepItem->setFont(0, Font);
		}

		if (CurrentStepItem)
		{
			QFont Font = CurrentStepItem->font(0);
			Font.setBold(true);
			CurrentStepItem->setFont(0, Font);
			setCurrentItem(CurrentStepItem);
		}

		mCurrentStepItem = CurrentStepItem;
	}
}

void swTimelineWidget::UpdateSelection()
{
	if (mIgnoreUpdates)
		return;

	QItemSelection ItemSelection;

	for (int TopLevelItemIdx = 0; TopLevelItemIdx < topLevelItemCount(); TopLevelItemIdx++)
	{
		QTreeWidgetItem* StepItem = topLevelItem(TopLevelItemIdx);

		for (int PartItemIdx = 0; PartItemIdx < StepItem->childCount(); PartItemIdx++)
		{
			QTreeWidgetItem* PartItem = StepItem->child(PartItemIdx);
			swPart* Part = (swPart*)PartItem->data(0, Qt::UserRole).value<uintptr_t>();

			if (Part && Part->IsSelected())
			{
				QModelIndex Index = indexFromItem(PartItem);
				ItemSelection.select(Index, Index);
			}
		}
	}

	bool Blocked = blockSignals(true);

	selectionModel()->select(ItemSelection, QItemSelectionModel::ClearAndSelect);

	blockSignals(Blocked);
}

void swTimelineWidget::InsertStepBefore()
{
	QTreeWidgetItem* CurrentItem = currentItem();

	if (!CurrentItem)
		return;

	if (CurrentItem->parent())
		CurrentItem = CurrentItem->parent();

	int Step = indexOfTopLevelItem(CurrentItem);

	if (Step == -1)
		return;

	gMainWindow->GetActiveModel()->InsertStepAction(Step + 1);
}

void swTimelineWidget::InsertStepAfter()
{
	QTreeWidgetItem* CurrentItem = currentItem();

	if (!CurrentItem)
		return;

	if (CurrentItem->parent())
		CurrentItem = CurrentItem->parent();

	int Step = indexOfTopLevelItem(CurrentItem);

	if (Step == -1)
		return;

	gMainWindow->GetActiveModel()->InsertStepAction(Step + 2);
}

void swTimelineWidget::RemoveStep()
{
	QTreeWidgetItem* CurrentItem = currentItem();

	if (!CurrentItem)
		return;

	if (CurrentItem->parent())
		CurrentItem = CurrentItem->parent();

	int Step = indexOfTopLevelItem(CurrentItem);

	if (Step == -1)
		return;

	gMainWindow->GetActiveModel()->RemoveStepAction(Step + 1);
}

void swTimelineWidget::MoveSelection()
{
	QTreeWidgetItem* CurrentItem = currentItem();

	if (!CurrentItem)
		return;

	if (CurrentItem->parent())
		CurrentItem = CurrentItem->parent();

	int Step = indexOfTopLevelItem(CurrentItem);

	if (Step == -1)
		return;
	Step++;

	const QList<QTreeWidgetItem*> SelectedItems = selectedItems();

	for (QTreeWidgetItem* PartItem : SelectedItems)
	{
		QTreeWidgetItem* Parent = PartItem->parent();

		if (!Parent)
			continue;

		int ChildIndex = Parent->indexOfChild(PartItem);
		CurrentItem->addChild(Parent->takeChild(ChildIndex));
	}

	UpdateModel();

	swModel* Model = gMainWindow->GetActiveModel();

	if (Step > static_cast<int>(Model->GetCurrentStep()))
		Model->SetCurrentStep(Step);
}

void swTimelineWidget::MoveSelectionBefore()
{
	QTreeWidgetItem* CurrentItem = currentItem();

	if (!CurrentItem)
		return;

	if (CurrentItem->parent())
		CurrentItem = CurrentItem->parent();

	int Step = indexOfTopLevelItem(CurrentItem);

	if (Step == -1)
		return;

	Step++;

	swModel* Model = gMainWindow->GetActiveModel();

	Model->InsertStepAndMoveSelectedParts(Step);

	if (Step > static_cast<int>(Model->GetCurrentStep()))
		Model->SetCurrentStep(Step);
}

void swTimelineWidget::MoveSelectionAfter()
{
	QTreeWidgetItem* CurrentItem = currentItem();

	if (!CurrentItem)
		return;

	if (CurrentItem->parent())
		CurrentItem = CurrentItem->parent();

	int Step = indexOfTopLevelItem(CurrentItem);

	if (Step == -1)
		return;

	Step += 2;

	swModel* Model = gMainWindow->GetActiveModel();

	Model->InsertStepAndMoveSelectedParts(Step);

	if (Step > static_cast<int>(Model->GetCurrentStep()))
		Model->SetCurrentStep(Step);
}

void swTimelineWidget::SetCurrentStep()
{
	QTreeWidgetItem* CurrentItem = currentItem();

	if (!CurrentItem)
		return;

	if (CurrentItem->parent())
		CurrentItem = CurrentItem->parent();

	int Step = indexOfTopLevelItem(CurrentItem);

	if (Step == -1)
		return;

	gMainWindow->GetActiveModel()->SetCurrentStep(Step + 1);
}

void swTimelineWidget::CurrentItemChanged(QTreeWidgetItem* Current, QTreeWidgetItem* Previous)
{
	Q_UNUSED(Previous);

	if (Current && !Current->parent())
		SetCurrentStep();
}

void swTimelineWidget::ItemSelectionChanged()
{
	std::vector<swObject*> Selection;
	swStep LastStep = 1;
	const QList<QTreeWidgetItem*> SelectedItems = selectedItems();

	for (QTreeWidgetItem* PartItem : SelectedItems)
	{
		swPart* Part = (swPart*)PartItem->data(0, Qt::UserRole).value<uintptr_t>();
		if (Part)
		{
			LastStep = swMaximum(LastStep, Part->GetStepShow());
			Selection.emplace_back(Part);
		}
	}

	swPart* CurrentPart = nullptr;
	QTreeWidgetItem* CurrentItem = currentItem();

	if (CurrentItem && CurrentItem->isSelected())
		CurrentPart = (swPart*)CurrentItem->data(0, Qt::UserRole).value<uintptr_t>();

	bool Blocked = blockSignals(true);
	mIgnoreUpdates = true;
	swModel* Model = gMainWindow->GetActiveModel();

	if (LastStep > Model->GetCurrentStep())
	{
		Model->SetCurrentStep(LastStep);
		UpdateCurrentStepItem();
	}

	Model->SetSelectionAndFocusAction(Selection, CurrentPart, SW_PART_SECTION_POSITION, swSelectionMode::Single);

	mIgnoreUpdates = false;
	blockSignals(Blocked);

	UpdateSelection();
}

void swTimelineWidget::dropEvent(QDropEvent* Event)
{
	QTreeWidgetItem* DropItem = itemAt(Event->pos());

	if (!DropItem)
		return;

	QList<QTreeWidgetItem*> SelectedItems = selectedItems();
	clearSelection();

	QTreeWidgetItem* ParentItem = DropItem->parent();
	swStep Step = indexOfTopLevelItem(ParentItem ? ParentItem : DropItem) + 1;
	swModel* Model = gMainWindow->GetActiveModel();

	if (Step > Model->GetCurrentStep())
		Model->SetCurrentStep(Step);

	auto SortItems = [this](QTreeWidgetItem* Item1, QTreeWidgetItem* Item2)
	{
		QTreeWidgetItem* StepItem1 = Item1->parent();
		QTreeWidgetItem* StepItem2 = Item2->parent();

		if (StepItem1 == StepItem2)
			return StepItem1->indexOfChild(Item1) < StepItem1->indexOfChild(Item2);

		return indexOfTopLevelItem(StepItem1) < indexOfTopLevelItem(StepItem2);
	};

	std::sort(SelectedItems.begin(), SelectedItems.end(), SortItems);

	for (QTreeWidgetItem* SelectedItem : std::as_const(SelectedItems))
		SelectedItem->setSelected(true);

	QTreeWidget::dropEvent(Event);

	UpdateModel();
	Update(false, false);
}

void swTimelineWidget::mousePressEvent(QMouseEvent* Event)
{
	if (Event->button() == Qt::RightButton)
	{
		QItemSelection Selection = selectionModel()->selection();

		bool Blocked = blockSignals(true);
		QTreeWidget::mousePressEvent(Event);
		blockSignals(Blocked);

		selectionModel()->select(Selection, QItemSelectionModel::ClearAndSelect);
	}
	else
		QTreeWidget::mousePressEvent(Event);
}

void swTimelineWidget::mouseDoubleClickEvent(QMouseEvent* MouseEvent)
{
	if (MouseEvent->button() == Qt::LeftButton)
	{
		QTreeWidgetItem* CurrentItem = currentItem();
		PreviewSelection(CurrentItem);
	}

	QTreeWidget::mouseDoubleClickEvent(MouseEvent);
}

void swTimelineWidget::PreviewSelection(QTreeWidgetItem* CurrentItem)
{
	if (!CurrentItem)
		return;

	swPart* Part = (swPart*)CurrentItem->data(0, Qt::UserRole).value<uintptr_t>();

	if (!Part)
		return;

	swPartData* PartData = Part->mPartData.Get();

	if (!PartData)
		return;

	gMainWindow->PreviewPart(PartData, Part->GetColorCode(), false);
}

void swTimelineWidget::UpdateModel()
{
	std::vector<std::pair<swPart*, swStep>> PartSteps;

	for (int TopLevelItemIdx = 0; TopLevelItemIdx < topLevelItemCount(); TopLevelItemIdx++)
	{
		QTreeWidgetItem* StepItem = topLevelItem(TopLevelItemIdx);

		for (int PartItemIdx = 0; PartItemIdx < StepItem->childCount(); PartItemIdx++)
		{
			QTreeWidgetItem* PartItem = StepItem->child(PartItemIdx);
			swPart* Part = (swPart*)PartItem->data(0, Qt::UserRole).value<uintptr_t>();

			if (!Part)
				continue;

			PartSteps.emplace_back(std::pair<swPart*, swStep>(Part, TopLevelItemIdx + 1));
		}
	}

	mIgnoreUpdates = true;
	gMainWindow->GetActiveModel()->SetPartSteps(PartSteps);
	mIgnoreUpdates = false;
}
