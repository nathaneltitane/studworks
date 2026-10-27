#include "sw_global.h"
#include "sw_widget_preview.h"
#include "sw_data_piece.h"
#include "sw_object_piece.h"
#include "sw_project.h"
#include "sw_model.h"
#include "sw_library.h"
#include "sw_application.h"
#include "sw_view_widget.h"
#include "sw_view.h"
#include "sw_window_main.h"
#include "sw_profile.h"
#include "sw_window_utilities.h"

// a read-only box like the panel text boxes that shortens the name with an ellipsis to fit
class swPreviewNameEdit : public QLineEdit
{
public:
	swPreviewNameEdit()
	{
		setReadOnly(true);
		setFocusPolicy(Qt::NoFocus);
		setAlignment(Qt::AlignCenter);
	}

	void SetName(const QString& Name)
	{
		mName = Name;
		setToolTip(Name);
		UpdateText();
	}

	QSize minimumSizeHint() const override
	{
		return QSize(0, QLineEdit::minimumSizeHint().height());
	}

protected:
	void resizeEvent(QResizeEvent* Event) override
	{
		QLineEdit::resizeEvent(Event);
		UpdateText();
	}

	void UpdateText()
	{
		const QMargins Margins = textMargins();
		const int Width = contentsRect().width() - Margins.left() - Margins.right() - 8;

		setText(fontMetrics().elidedText(mName, Qt::ElideRight, qMax(Width, 0)));
	}

	QString mName;
};

swPreviewDockWidget::swPreviewDockWidget(QWidget* Parent)
	: QWidget(Parent)
{
	mPreview = new swPreview();

	// the view sits in a container that can keep it square
	mViewContainer = new QWidget(this);
	mViewContainer->installEventFilter(this);

	mViewWidget = new swViewWidget(mViewContainer, mPreview);

	// the preview blends into its panel without a viewport border
	mPreview->SetDrawViewportBorder(false);
	mViewWidget->setAccessibleName(tr("Piece Preview"));
	mViewWidget->setToolTip(tr("Inspect the previewed piece without placing it in the model."));
	// Small enough to stack with other panels, the header rows and a few rows of view stay visible.
	setMinimumSize(200, 4 * swGetPanelButtonSize().height());

	mLockAction = new QAction(tr("Lock Preview"), this);
	mLockAction->setWhatsThis(tr("<p><b>Lock Preview</b></p><p>Keep the current piece in the preview while you browse other pieces. Unlock Preview allows the display to update again. An empty preview cannot be locked.</p>"));
	mLockAction->setCheckable(true);
	mLockAction->setChecked(false);
	mLockAction->setShortcut(tr("Ctrl+L"));
	connect(mLockAction, &QAction::triggered, this, &swPreviewDockWidget::SetPreviewLock);
	SetPreviewLock();

	mLabel = new swPreviewNameEdit();
	mLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	swSetupPanelEdit(mLabel);

	swPiecesLibrary* Library = swGetPiecesLibrary();
	connect(Library, &swPiecesLibrary::PieceLoaded, this, [this](swPieceData*)
	{
		if (mAssetRefreshPending)
			RefreshPendingAssets();
	});
	connect(Library, &swPiecesLibrary::PieceLoadFailed, this, [this](swPieceData*, const QString&)
	{
		if (mAssetRefreshPending)
			RefreshPendingAssets();
	});
	connect(Library, &swPiecesLibrary::GeneratedMeshSettled, this, [this](swPieceData*)
	{
		if (mAssetRefreshPending)
			RefreshPendingAssets();
	});

	// The button row, the name of the previewed piece and the view are stacked like the other panels.
	QVBoxLayout* Layout = new QVBoxLayout(this);

	QWidget* ButtonRow = swCreatePanelRow(this);
	Layout->addWidget(ButtonRow);

	// Nothing else sits beside it, so the lock button reaches up to the options button.
	QToolButton* LockButton = swAddPanelActionButton(ButtonRow, mLockAction);
	LockButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
	LockButton->setWhatsThis(mLockAction->whatsThis());
	swSetupPanelButton(LockButton);
	LockButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

	QMenu* OptionsMenu = new QMenu(this);
	swAddPanelOptionsButton(ButtonRow, OptionsMenu, tr("Configure the pieces preview."), tr("Pieces Preview Options"));
	connect(OptionsMenu, &QMenu::aboutToShow, this, &swPreviewDockWidget::OptionsMenuAboutToShow);

	Layout->addWidget(mLabel);
	Layout->addWidget(mViewContainer, 1);
}

void swPreviewDockWidget::OptionsMenuAboutToShow()
{
	QMenu* Menu = qobject_cast<QMenu*>(sender());
	Menu->clear();

	const swPreferences& Preferences = swGetPreferences();

	gMainWindow->AddMenuCommand(Menu, tr("View Sphere"), SW_VIEW_TOGGLE_PREVIEW_VIEW_SPHERE, Preferences.mPreviewViewSphereEnabled);
	gMainWindow->AddMenuCommand(Menu, tr("Axis Icon"), SW_VIEW_TOGGLE_PREVIEW_AXIS_ICON, Preferences.mDrawPreviewAxis);
	gMainWindow->AddMenuCommand(Menu, tr("Square Viewport"), SW_VIEW_PREVIEW_TOGGLE_SQUARE, Preferences.mPreviewSquareViewport);

	Menu->addSeparator();

	gMainWindow->AddRenderTargetActions(Menu, swRenderTarget::Preview);
}

void swPreviewDockWidget::SetSquareViewport(bool SquareViewport)
{
	swPreferences& Preferences = swGetPreferences();

	Preferences.mPreviewSquareViewport = SquareViewport;
	swSetProfileInteger(SW_PROFILE_PREVIEW_SQUARE_VIEWPORT, SquareViewport);

	UpdateSquareViewport();
}

void swPreviewDockWidget::UpdateSquareViewport()
{
	UpdateViewGeometry();
}

void swPreviewDockWidget::UpdateViewGeometry()
{
	// a square preview sizes the panel to fit, the height follows the width like the colors panel
	if (swGetPreferences().mPreviewSquareViewport)
		setFixedHeight(height() - mViewContainer->height() + mViewContainer->width());
	else
	{
		setMinimumHeight(4 * swGetPanelButtonSize().height());
		setMaximumHeight(QWIDGETSIZE_MAX);
	}

	mViewWidget->setGeometry(mViewContainer->rect());
}

bool swPreviewDockWidget::eventFilter(QObject* Object, QEvent* Event)
{
	if (Object == mViewContainer && Event->type() == QEvent::Resize)
		UpdateViewGeometry();

	return QWidget::eventFilter(Object, Event);
}

bool swPreviewDockWidget::SetCurrentPiece(swPieceData* PieceData, int ColorCode)
{
	if (mLockAction->isChecked())
		return true;

	mAssetRefreshPending = false;
	mRefitAfterAssetLoad = true;
	mLabel->SetName(tr("Loading..."));

	if (mPreview->SetCurrentPiece(PieceData, ColorCode))
	{
		RefreshPendingAssets();
		return true;
	}

	mViewWidget->show();
	mLabel->SetName(tr("Preview unavailable"));

	return false;
}

void swPreviewDockWidget::UpdatePreview()
{
	mAssetRefreshPending = false;
	mRefitAfterAssetLoad = true;
	mPreview->UpdatePreview();
	RefreshPendingAssets();
}

void swPreviewDockWidget::RefreshDescription()
{
	mPreview->RefreshDescription();
	mLabel->SetName(mPreview->GetDescription());
}

void swPreviewDockWidget::RefreshModel(const swModel* Model)
{
	if (mLockAction->isChecked() || mRefreshingModel)
		return;

	const std::vector<swPieceDataReference> RequiredPieceData = mPreview->GetModel()->GetRequiredPieceData();

	if (std::find(RequiredPieceData.begin(), RequiredPieceData.end(), Model->GetPieceData()) == RequiredPieceData.end())
		return;

	mRefreshingModel = true;

	if (!mAssetRefreshPending)
		mRefitAfterAssetLoad = false;

	swPiecesLibrary* Library = swGetPiecesLibrary();

	for (const swPieceDataReference& RequiredData : RequiredPieceData)
	{
		if (RequiredData->mState != swAssetState::Unloaded)
			continue;

		Library->RequestPieceDataLoad(RequiredData.Get(), swLoadPriority::Normal);
	}

	std::vector<swModel*> UpdatedModels;
	mPreview->GetModel()->UpdatePieceData(UpdatedModels);

	RefreshPendingAssets();

	mRefreshingModel = false;
}

void swPreviewDockWidget::RefreshAfterAssetChange(const std::vector<swPieceData*>& ChangedPieces)
{
	if (mPreview->GetModel()->GetPieces().empty())
		return;

	const std::vector<swPieceDataReference> RequiredPieceData = mPreview->GetModel()->GetRequiredPieceData();
	bool Affected = std::any_of(RequiredPieceData.begin(), RequiredPieceData.end(), [&ChangedPieces](const swPieceDataReference& RequiredData)
	{
		return std::find(ChangedPieces.begin(), ChangedPieces.end(), RequiredData) != ChangedPieces.end();
	});

	if (!Affected)
	{
		for (const std::unique_ptr<swPiece>& Piece : mPreview->GetModel()->GetPieces())
		{
			if (std::find(ChangedPieces.begin(), ChangedPieces.end(), Piece->mPieceData) != ChangedPieces.end())
			{
				Affected = true;
				break;
			}
		}
	}

	if (!Affected)
		return;

	RefreshPendingAssets();
}

void swPreviewDockWidget::RefreshPendingAssets()
{
	const std::vector<swPieceDataReference> RequiredPieceData = mPreview->GetModel()->GetRequiredPieceData();

	for (const swPieceDataReference& RequiredData : RequiredPieceData)
	{
		if (RequiredData->mState == swAssetState::Failed || RequiredData->mState == swAssetState::Unloaded)
		{
			mAssetRefreshPending = false;
			mViewWidget->hide();
			mLabel->SetName(tr("Preview unavailable"));
			return;
		}

		if (RequiredData->mState != swAssetState::Loaded)
		{
			mAssetRefreshPending = true;
			mViewWidget->hide();
			mLabel->SetName(tr("Loading..."));
			return;
		}
	}

	for (const swPiece* Piece : mPreview->GetModel()->GetRequiredSynthPieces())
	{
		if (Piece->IsGeneratedMeshPending())
		{
			mAssetRefreshPending = true;
			mViewWidget->hide();
			mLabel->SetName(tr("Loading..."));
			return;
		}

		if (!Piece->HasGeneratedMesh())
		{
			mAssetRefreshPending = false;
			mViewWidget->hide();
			mLabel->SetName(tr("Preview unavailable"));
			return;
		}
	}

	if (mAssetRefreshPending && mRefitAfterAssetLoad)
		mPreview->ZoomExtents(swGeometryBoundsMode::IncludeFallback, false);

	mAssetRefreshPending = false;
	mRefitAfterAssetLoad = true;
	mViewWidget->show();
	mPreview->RefreshDescription();
	mPreview->Redraw();
	mLabel->SetName(mPreview->GetDescription());
}

void swPreviewDockWidget::RebindPieceData(swPieceData* Previous, swPieceData* Replacement)
{
	int ColorCode = -1;

	for (const std::unique_ptr<swPiece>& Piece : mPreview->GetModel()->GetPieces())
	{
		if (Piece->mPieceData == Previous)
		{
			ColorCode = Piece->GetColorCode();
			break;
		}
	}

	if (ColorCode == -1)
		return;

	mAssetRefreshPending = false;
	mRefitAfterAssetLoad = true;

	if (mPreview->SetCurrentPiece(Replacement, ColorCode))
		RefreshPendingAssets();
	else
	{
		mViewWidget->show();
		mLabel->SetName(tr("Preview unavailable"));
	}
}

void swPreviewDockWidget::ClearPreview()
{
	mAssetRefreshPending = false;
	mRefitAfterAssetLoad = true;
	mViewWidget->show();
	if (mPreview->GetModel()->GetPieces().size())
		mPreview->ClearPreview();

	mLabel->SetName(QString());
}

void swPreviewDockWidget::SetPreviewLock()
{
	bool Locked = mLockAction->isChecked();

	if (Locked && mPreview->GetModel()->GetPieces().empty())
	{
		mLockAction->setChecked(false);
		return;
	}

	QString StatusTip(Locked
		? tr("Unlock the preview display to enable updates")
		: tr("Lock the preview display to disable updates"));

	mLockAction->setText(Locked ? tr("Unlock Preview") : tr("Lock Preview"));
	mLockAction->setToolTip(mLockAction->text());
	mLockAction->setStatusTip(StatusTip);
}

swPreview::swPreview()
	: swView(swViewType::Preview, nullptr), mLoader(new Project(true))
{
	mLoader->SetActiveModel(0, false);
	mModel = mLoader->GetActiveModel();
}

bool swPreview::SetCurrentPiece(swPieceData* PieceData, int ColorCode)
{
	if (!PieceData)
	{
		ClearPreview();
		return false;
	}

	for (const std::unique_ptr<swPiece>& ModelPiece : mModel->GetPieces())
	{
		if (PieceData == ModelPiece->mPieceData)
		{
			int ModelColorCode = ModelPiece->GetColorCode();

			if (ModelColorCode == ColorCode)
			{
			RefreshDescription();
				return true;
			}
		}
	}

	swPiecesLibrary* Library = swGetPiecesLibrary();
	const swPieceDataReference PieceDataRef(PieceData);

	bool Ready = Library->WaitForPieceData(PieceDataRef);

	if (Ready && PieceData->IsModel())
		Ready = static_cast<bool>(PieceData->GetModel()->EnsureAssetsReady());
	else if (Ready && PieceData->IsProject())
		Ready = static_cast<bool>(PieceData->GetProject()->EnsureAssetsReady());

	if (!Ready)
	{
		ClearPreview();

		return false;
	}

	mIsModel = PieceData->IsModel();
	mDescription = PieceData->m_strDescription;

	mModel->SelectAllPiecesAction();
	mModel->DeleteSelectedObjects();
	mModel->SetPreviewPieceData(PieceData, swGetColorIndex(ColorCode));

	std::vector<swModel*> UpdatedModels;

	mModel->UpdatePieceData(UpdatedModels);

	ZoomExtents(swGeometryBoundsMode::IncludeFallback, true);

	return true;
}

void swPreview::ClearPreview()
{
	mDescription.clear();
	mIsModel = false;
	mLoader = std::unique_ptr<Project>(new Project(true/*IsPreview*/));
	mLoader->SetActiveModel(0, false);
	mModel = mLoader->GetActiveModel();

	swPiecesLibrary* Library = swGetPiecesLibrary();

	Library->RemoveTemporaryPieceData();
	Redraw();
}

void swPreview::UpdatePreview()
{
	swPieceData* PieceData = nullptr;
	int ColorCode = -1;

	for (const std::unique_ptr<swPiece>& ModelPiece : mModel->GetPieces())
	{
		if (ModelPiece->mPieceData)
		{
			PieceData = ModelPiece->mPieceData.Get();
			ColorCode = ModelPiece->GetColorCode();
			break;
		}
	}

	swPieceDataReference PreviousPieceData(PieceData);

	ClearPreview();

	if (PieceData && ColorCode > -1)
		SetCurrentPiece(PieceData, ColorCode);

}

void swPreview::RefreshDescription()
{
	for (const std::unique_ptr<swPiece>& Piece : mModel->GetPieces())
	{
		if (!Piece->mPieceData)
			continue;

		mDescription = QString::fromLatin1(Piece->mPieceData->m_strDescription);
		return;
	}

	mDescription.clear();
}
