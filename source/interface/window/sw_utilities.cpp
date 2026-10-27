#include "sw_global.h"
#include "sw_utilities.h"
#include "sw_application.h"
#include "sw_library.h"
#include "sw_model.h"
#include "sw_piece_data.h"
#include "sw_string.h"
#include <QPrintDialog>

#ifdef Q_OS_WIN
#include <windows.h>
#include <TlHelp32.h>
#endif

QPoint swGetPopupPosition(const QPoint& Position, const QSize& Size, const QPoint& ScreenPosition)
{
	QScreen* Screen = QGuiApplication::screenAt(ScreenPosition);

	if (!Screen)
		Screen = QGuiApplication::primaryScreen();

	if (!Screen)
		return Position;

	const QRect AvailableGeometry = Screen->availableGeometry();
	const int X = qBound(AvailableGeometry.left(), Position.x(), qMax(AvailableGeometry.left(), AvailableGeometry.right() - Size.width() + 1));
	const int Y = qBound(AvailableGeometry.top(), Position.y(), qMax(AvailableGeometry.top(), AvailableGeometry.bottom() - Size.height() + 1));

	return QPoint(X, Y);
}

bool swWindowGeometryFilter::eventFilter(QObject* Object, QEvent* Event)
{
	if (Event->type() != QEvent::Show || !Object->isWidgetType())
		return false;

	QWidget* Window = static_cast<QWidget*>(Object);

	if (!Window->isWindow() || Window->property("swInitialGeometry").toBool())
		return false;

	// Only dialogs and secondary main windows, the standard Qt dialogs size themselves and the main window restores its own geometry.
	const bool IsDialog = qobject_cast<QDialog*>(Window) && !qobject_cast<QMessageBox*>(Window) && !qobject_cast<QInputDialog*>(Window) && !qobject_cast<QProgressDialog*>(Window) &&
		!qobject_cast<QFileDialog*>(Window) && !qobject_cast<QColorDialog*>(Window) && !qobject_cast<QFontDialog*>(Window) && !qobject_cast<QPrintDialog*>(Window);
	const bool IsSecondaryWindow = qobject_cast<QMainWindow*>(Window) && Window->parentWidget();

	if (!IsDialog && !IsSecondaryWindow)
		return false;

	Window->setProperty("swInitialGeometry", true);

	QWidget* Parent = Window->parentWidget() ? Window->parentWidget()->window() : nullptr;
	QScreen* Screen = Parent ? Parent->screen() : Window->screen();

	if (!Screen)
		return false;

	// A quarter of the available screen, never smaller than the contents need and never larger than the screen.
	const QRect Available = Screen->availableGeometry();
	QSize Size = QSize(Available.width() / 4, Available.height() / 4).expandedTo(Window->sizeHint()).expandedTo(Window->minimumSizeHint());
	Size = Size.boundedTo(Available.size());

	QRect Geometry(QPoint(0, 0), Size);
	Geometry.moveCenter(Parent ? Parent->geometry().center() : Available.center());
	Geometry.moveLeft(qBound(Available.left(), Geometry.left(), Available.right() - Size.width() + 1));
	Geometry.moveTop(qBound(Available.top(), Geometry.top(), Available.bottom() - Size.height() + 1));

	Window->setGeometry(Geometry);

	return false;
}

class swFitHeightFilter : public QObject
{
public:
	explicit swFitHeightFilter(QWidget* Widget)
		: QObject(Widget)
	{
	}

protected:
	bool eventFilter(QObject* Object, QEvent* Event) override
	{
		if (Event->type() == QEvent::LayoutRequest)
		{
			QWidget* Widget = static_cast<QWidget*>(Object);
			Widget->setFixedHeight(Widget->sizeHint().height());
		}

		return false;
	}
};

void swFitHeightToContents(QWidget* Widget)
{
	Widget->setFixedHeight(Widget->sizeHint().height());
	Widget->installEventFilter(new swFitHeightFilter(Widget));
}

swBlockingProgress* swBlockingProgress::mActive = nullptr;

swBlockingProgress::swBlockingProgress(const QString& Title, const QString& Label, bool Cancelable)
	: mTitle(Title), mLabel(Label), mCancelable(Cancelable)
{
	// A wait started while another one shows its progress is part of the same task.
	mNested = mActive != nullptr;

	if (!mNested)
		mActive = this;

	mTimer.start();
}

swBlockingProgress::~swBlockingProgress()
{
	delete mDialog;

	if (mActive == this)
		mActive = nullptr;
}

void swBlockingProgress::SetProgress(int Value, int Maximum)
{
	if (mNested)
		return;

	if (!mDialog)
	{
		// Short tasks finish without a dialog flashing on screen.
		if (Value >= Maximum || mTimer.elapsed() < 400)
			return;

		mDialog = new QProgressDialog(QApplication::activeWindow());
		mDialog->setWindowTitle(mTitle);
		mDialog->setLabelText(mLabel);
		mDialog->setWindowModality(Qt::ApplicationModal);
		mDialog->setMinimumDuration(0);
		mDialog->setAutoClose(false);
		mDialog->setAutoReset(false);

		QProgressBar* Bar = new QProgressBar(mDialog);
		Bar->setFormat(tr("%v of %m"));
		mDialog->setBar(Bar);

		if (!mCancelable)
		{
			mDialog->setCancelButton(nullptr);
			mDialog->setWindowFlags(mDialog->windowFlags() & ~Qt::WindowCloseButtonHint);
		}

		mDialog->setRange(0, Maximum);
		mDialog->show();
		mUpdateTimer.invalidate();
	}

	// Updates are throttled, each one runs the event loop to repaint the dialog.
	if (mUpdateTimer.isValid() && mUpdateTimer.elapsed() < 50 && Value < Maximum)
		return;

	mUpdateTimer.start();
	mDialog->setMaximum(Maximum);
	mDialog->setValue(Value);
}

bool swBlockingProgress::IsCanceled() const
{
	return mDialog && mDialog->wasCanceled();
}

QSize swGetPanelButtonSize()
{
	// Every panel button is sized after the options gear button of a panel toolbar.
	static QSize ButtonSize;

	if (!ButtonSize.isValid())
	{
		QToolBar ToolBar;
		QToolButton* Button = new QToolButton(&ToolBar);

		Button->setIcon(swCreateInterfaceIcon("widgets", "button_options"));
		Button->setAutoRaise(true);
		ToolBar.addWidget(Button);

		ButtonSize = Button->sizeHint();
	}

	return ButtonSize;
}

void swSetupPanelButton(QToolButton* Button)
{
	// Panel buttons are outlined, icon buttons are as large as the gear and text buttons as tall.
	const QSize ButtonSize = swGetPanelButtonSize();

	Button->setAutoRaise(false);
	Button->setFocusPolicy(Qt::NoFocus);
	Button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

	if (Button->toolButtonStyle() == Qt::ToolButtonIconOnly)
		Button->setFixedSize(ButtonSize);
	else
	{
		Button->setMinimumWidth(0);
		Button->setMaximumWidth(QWIDGETSIZE_MAX);
		Button->setFixedHeight(ButtonSize.height());
	}
}

void swSetupPanelEdit(QWidget* Edit)
{
	// Text boxes in panel toolbars are as tall as the buttons so the rows have even spacing.
	Edit->setFixedHeight(swGetPanelButtonSize().height());
}

void swSetupPanelSpinBox(QAbstractSpinBox* SpinBox)
{
	// Spin boxes are wide enough for a short value with the name and unit shown in them.
	QString Prefix, Suffix;

	if (const QDoubleSpinBox* DoubleSpinBox = qobject_cast<const QDoubleSpinBox*>(SpinBox))
	{
		Prefix = DoubleSpinBox->prefix();
		Suffix = DoubleSpinBox->suffix();
	}
	else if (const QSpinBox* IntegerSpinBox = qobject_cast<const QSpinBox*>(SpinBox))
	{
		Prefix = IntegerSpinBox->prefix();
		Suffix = IntegerSpinBox->suffix();
	}

	QStyleOptionSpinBox Option;
	Option.initFrom(SpinBox);
	Option.rect = QRect(QPoint(0, 0), SpinBox->sizeHint());
	Option.subControls = QStyle::SC_SpinBoxUp | QStyle::SC_SpinBoxDown | QStyle::SC_SpinBoxFrame | QStyle::SC_SpinBoxEditField;
	Option.frame = SpinBox->hasFrame();

	const QRect EditRect = SpinBox->style()->subControlRect(QStyle::CC_SpinBox, &Option, QStyle::SC_SpinBoxEditField, SpinBox);
	const int Decoration = Option.rect.width() - EditRect.width();

	// Equal stretch gives the boxes of a row the same width whatever their text.
	QSizePolicy SizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	SizePolicy.setHorizontalStretch(1);
	SpinBox->setSizePolicy(SizePolicy);
	SpinBox->setMinimumWidth(SpinBox->fontMetrics().horizontalAdvance(Prefix + QLatin1String("-000") + Suffix) + Decoration);
	swSetupPanelEdit(SpinBox);
}

int swGetLayoutSpacing(const QWidget* Widget, Qt::Orientation Orientation)
{
	// The spacing a standard layout uses, for widgets that place their children themselves.
	QStyle* Style = Widget->style();
	const int Spacing = Style->pixelMetric(Orientation == Qt::Vertical ? QStyle::PM_LayoutVerticalSpacing : QStyle::PM_LayoutHorizontalSpacing, nullptr, Widget);

	return Spacing >= 0 ? Spacing : Style->layoutSpacing(QSizePolicy::DefaultType, QSizePolicy::DefaultType, Orientation, nullptr, Widget);
}

QWidget* swCreatePanelRow(QWidget* Parent)
{
	// The row sits inside the panel layout, which provides the margins, the row keeps the standard spacing.
	QWidget* Row = new QWidget(Parent);
	QHBoxLayout* Layout = new QHBoxLayout(Row);
	Layout->setContentsMargins(0, 0, 0, 0);

	return Row;
}

void swAddPanelRowWidget(QWidget* Row, QWidget* Widget)
{
	Row->layout()->addWidget(Widget);
}

void swAddPanelRowStretch(QWidget* Row)
{
	static_cast<QHBoxLayout*>(Row->layout())->addStretch(1);
}

QToolButton* swAddPanelActionButton(QWidget* Row, QAction* Action)
{
	QToolButton* Button = new QToolButton(Row);

	Button->setDefaultAction(Action);
	swAddPanelRowWidget(Row, Button);
	swSetupPanelButton(Button);

	return Button;
}

QToolButton* swAddPanelOptionsButton(QWidget* Row, QMenu* Menu, const QString& ToolTip, const QString& AccessibleName)
{
	QToolButton* OptionsButton = new QToolButton(Row);

	OptionsButton->setIcon(swCreateInterfaceIcon("widgets", "button_options"));
	OptionsButton->setToolTip(ToolTip);
	OptionsButton->setAccessibleName(AccessibleName);
	OptionsButton->setPopupMode(QToolButton::InstantPopup);
	OptionsButton->setMenu(Menu);
	swAddPanelRowWidget(Row, OptionsButton);
	swSetupPanelButton(OptionsButton);

	return OptionsButton;
}

void swSetupWidgetHelp(QWidget* Root, QDialogButtonBox* ButtonBox)
{
	const auto ShowHelp = [Root](QWidget* Widget)
	{
		while (Widget && Widget->whatsThis().isEmpty() && Widget != Root)
			Widget = Widget->parentWidget();

		if (Widget && !Widget->whatsThis().isEmpty())
			QWhatsThis::showText(Widget->mapToGlobal(QPoint(0, Widget->height())), Widget->whatsThis(), Widget);
	};

	if (ButtonBox)
		QObject::connect(ButtonBox, &QDialogButtonBox::helpRequested, Root, [Root, ShowHelp]()
		{
			ShowHelp(Root);
		});

	QShortcut* HelpShortcut = new QShortcut(QKeySequence(Qt::Key_F1), Root);
	HelpShortcut->setContext(Qt::WidgetWithChildrenShortcut);

	const auto HelpActivated = [Root, ShowHelp]()
	{
		QWidget* Focus = QApplication::focusWidget();
		// Popup windows can share the shortcut scope across a QWidget window boundary.
		ShowHelp(Focus ? Focus : Root);
	};

	QObject::connect(HelpShortcut, &QShortcut::activated, Root, HelpActivated);
	// A chooser can overlap its owning dock's shortcut scope; both routes start at the focused control.
	QObject::connect(HelpShortcut, &QShortcut::activatedAmbiguously, Root, HelpActivated);
}

void swSetupDialogHelp(QDialog* Dialog, QDialogButtonBox* ButtonBox)
{
	swSetupWidgetHelp(Dialog, ButtonBox);
}

QString swFormatValue(float Value, int Precision)
{
	QString String = QString::number(Value, 'f', Precision);
	const qsizetype Dot = String.indexOf('.');

	if (Dot != -1)
	{
		while (String.endsWith('0'))
			String.chop(1);

		if (String.endsWith('.'))
			String.chop(1);
	}

	return String;
}

QString swFormatFloatLocalized(float Value, QLocale Locale)
{
	Locale.setNumberOptions(Locale.numberOptions() | QLocale::OmitGroupSeparator);
	QString DecimalPoint = Locale.decimalPoint();
	QString String = Locale.toString(Value, 'f', 4);

	if (String.indexOf(DecimalPoint) != -1)
	{
		while (String.endsWith(Locale.zeroDigit()))
			String.chop(1);

		if (String.endsWith(DecimalPoint))
			String.chop(1);
	}

	return String;
}

QString swFormatDoubleLocalized(double Value, QLocale Locale, char Format, int Precision)
{
	Locale.setNumberOptions(Locale.numberOptions() | QLocale::OmitGroupSeparator);
	return Locale.toString(Value, Format, Precision);
}

QString swFormatIntegerLocalized(qlonglong Value, QLocale Locale)
{
	Locale.setNumberOptions(Locale.numberOptions() | QLocale::OmitGroupSeparator);
	return Locale.toString(Value);
}

std::optional<float> swParseFloatLocalized(const QString& Value, QLocale Locale)
{
	bool Ok;
	const float Number = Locale.toFloat(Value, &Ok);

	if (!Ok || !std::isfinite(Number))
		return std::nullopt;

	return Number;
}

#ifdef Q_OS_WIN

int swTerminateChildProcess(QWidget* Parent, const qint64 Pid, const qint64 Ppid)
{
	DWORD PID        = DWORD(Pid);
	DWORD PPID       = DWORD(Ppid);
	HANDLE hSnapshot = INVALID_HANDLE_VALUE, hProcess = INVALID_HANDLE_VALUE;
	PROCESSENTRY32 PE32;

	if ((hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, PPID)) == INVALID_HANDLE_VALUE)
	{
		QMessageBox::warning(Parent, QObject::tr("Stop Blender Process"), QString("%1 failed: %1").arg("CreateToolhelp32Snapshot").arg(GetLastError()));
		return -1;
	}
	PE32.dwSize = sizeof(PROCESSENTRY32);
	if (Process32First(hSnapshot, &PE32) == FALSE)
	{
		QMessageBox::warning(Parent, QObject::tr("Stop Blender Process"), QString("%1 failed: %2").arg("Process32First").arg(GetLastError()));
		CloseHandle(hSnapshot);
		return -2;
	}
	do
	{
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
		QRegularExpression ExeExpression("^(?:cmd\\.exe|conhost\\.exe|blender\\.exe)$", QRegularExpression::CaseInsensitiveOption);
#else
		QRegExp ExeExpression("^(?:cmd\\.exe|conhost\\.exe|blender\\.exe)$", Qt::CaseInsensitive);
#endif

		if (QString::fromWCharArray(PE32.szExeFile).contains(ExeExpression))
		{
			if ((PE32.th32ProcessID == PID && PE32.th32ParentProcessID == PPID) || PE32.th32ParentProcessID == PID)
			{
				if ((hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, PE32.th32ProcessID)) == INVALID_HANDLE_VALUE)
				{
					QMessageBox::warning(Parent, QObject::tr("Stop Blender Process"), QObject::tr("%1 failed: %2").arg("OpenProcess").arg(GetLastError()));
					return -3;
				}
				else
				{
					TerminateProcess(hProcess, 9);
					CloseHandle(hProcess);
				}
			}
		}
	}
	while (Process32Next(hSnapshot, &PE32));

	CloseHandle(hSnapshot);

	return 0;
}

bool swRunElevatedProcess(const LPCWSTR ExeName, const LPCWSTR Arguments, const LPCWSTR WorkingFolder)
{
	SHELLEXECUTEINFO ShellExecuteInfo = {};

	ShellExecuteInfo.cbSize = sizeof(SHELLEXECUTEINFO);
	ShellExecuteInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
	ShellExecuteInfo.hwnd = NULL;
	ShellExecuteInfo.lpVerb = L"runas";
	ShellExecuteInfo.lpFile = ExeName;
	ShellExecuteInfo.lpParameters = Arguments;
	ShellExecuteInfo.lpDirectory = WorkingFolder;
	ShellExecuteInfo.nShow = SW_SHOW;
	ShellExecuteInfo.hInstApp = NULL;

	if (!ShellExecuteEx(&ShellExecuteInfo) || !ShellExecuteInfo.hProcess)
		return false;

	if (WaitForSingleObject(ShellExecuteInfo.hProcess, INFINITE))
		return false;

	DWORD ExitCode;

	if (!GetExitCodeProcess(ShellExecuteInfo.hProcess, &ExitCode))
		return false;

	CloseHandle(ShellExecuteInfo.hProcess);

	return ExitCode == 0;
}

#endif

// Resize all columns to content except for one stretching column. (taken from QT creator)
swTreeWidgetColumnStretcher::swTreeWidgetColumnStretcher(QTreeWidget* TreeWidget, int ColumnToStretch)
	: QObject(TreeWidget->header()), mColumnToStretch(ColumnToStretch), mInteractiveResize(false), mStretchWidth(0)
{
	parent()->installEventFilter(this);
	connect(TreeWidget->header(), &QHeaderView::sectionResized, this, &swTreeWidgetColumnStretcher::SectionResized);

	QHideEvent Fake;
	swTreeWidgetColumnStretcher::eventFilter(parent(), &Fake);
}

void swTreeWidgetColumnStretcher::SectionResized(int LogicalIndex, int OldSize, int NewSize)
{
	Q_UNUSED(OldSize)

	if (LogicalIndex == mColumnToStretch)
	{
		QHeaderView* HeaderView = qobject_cast<QHeaderView*>(parent());

		if (HeaderView->isVisible())
			mInteractiveResize = true;

		mStretchWidth = NewSize;
	}
}

bool swTreeWidgetColumnStretcher::eventFilter(QObject* Object, QEvent* Event)
{
	if (Object == parent())
	{
		QHeaderView* HeaderView = qobject_cast<QHeaderView*>(Object);

		if (Event->type() == QEvent::Show)
		{
			for (int i = 0; i < HeaderView->count(); ++i)
				HeaderView->setSectionResizeMode(i, QHeaderView::Interactive);

			mStretchWidth = HeaderView->sectionSize(mColumnToStretch);

		}
		else if (Event->type() == QEvent::Hide)
		{
			if (!mInteractiveResize)
				for (int i = 0; i < HeaderView->count(); ++i)
					HeaderView->setSectionResizeMode(i, i == mColumnToStretch ? QHeaderView::Stretch : QHeaderView::ResizeToContents);
		}
		else if (Event->type() == QEvent::Resize)
		{
			if (HeaderView->sectionResizeMode(mColumnToStretch) == QHeaderView::Interactive) {

				const int StretchWidth = HeaderView->isVisible() ? mStretchWidth : 32;

				HeaderView->resizeSection(mColumnToStretch, StretchWidth);
			}
		}
	}

	return false;
}

swPieceIdStringModel::swPieceIdStringModel(swModel* Model, QObject* Parent)
	: QAbstractListModel(Parent)
{
	swPiecesLibrary* Library = swGetPiecesLibrary();
	mSortedPieces = Library->GetParts();

	mSortedPieces.erase(std::remove_if(mSortedPieces.begin(), mSortedPieces.end(), [Model](swPieceData* PieceData)
	{
		return PieceData->IsModel() && PieceData->GetModel()->IncludesModel(Model);
	}), mSortedPieces.end());

	auto PieceCompare = [](swPieceData* PieceData1, swPieceData* PieceData2)
	{
		return strcmp(PieceData1->m_strDescription, PieceData2->m_strDescription) < 0;
	};

	std::sort(mSortedPieces.begin(), mSortedPieces.end(), PieceCompare);
}

QModelIndex swPieceIdStringModel::Index(swPieceData* PieceData) const
{
	for (size_t PieceDataIndex = 0; PieceDataIndex < mSortedPieces.size(); PieceDataIndex++)
		if (mSortedPieces[PieceDataIndex] == PieceData)
			return index(static_cast<int>(PieceDataIndex), 0);

	return QModelIndex();
}

std::vector<bool> swPieceIdStringModel::GetFilteredRows(const QString& FilterText) const
{
	const std::string Text = FilterText.toStdString();
	std::vector<bool> FilteredRows(mSortedPieces.size());

	for (size_t PieceDataIndex = 0; PieceDataIndex < mSortedPieces.size(); PieceDataIndex++)
	{
		const swPieceData* PieceData = mSortedPieces[PieceDataIndex];

		FilteredRows[PieceDataIndex] = (swstrcasestr(PieceData->m_strDescription, Text.c_str()) || swstrcasestr(PieceData->mFileName, Text.c_str()));
	}

	return FilteredRows;
}

int swPieceIdStringModel::rowCount(const QModelIndex& Parent) const
{
	Q_UNUSED(Parent);

	return static_cast<int>(mSortedPieces.size());
}

QVariant swPieceIdStringModel::data(const QModelIndex& Index, int Role) const
{
	if (Index.row() < static_cast<int>(mSortedPieces.size()))
	{
		if (Role == Qt::DisplayRole)
			return QString::fromLatin1(mSortedPieces[Index.row()]->m_strDescription);
		else if (Role == Qt::UserRole)
			return QVariant::fromValue(reinterpret_cast<void*>(mSortedPieces[Index.row()]));
	}

	return QVariant();
}

swColorDialogPopup::swColorDialogPopup(const QColor& InitialColor, bool ShowAlphaChannel, QWidget* Parent)
	: QWidget(Parent)
{
	setAccessibleName(tr("Choose Color"));
	setWhatsThis(tr("<p><b>Choose Color</b></p><p>Pick a color using the swatches or enter its component values. Choose OK to use the selected color, or Cancel to keep the original color. Editing the controls alone does not apply the color.</p>"));

	QVBoxLayout* Layout = new QVBoxLayout(this);
	Layout->setContentsMargins(0, 0, 0, 0);

	mColorDialog = new QColorDialog(InitialColor, this);
	mColorDialog->setWindowFlags(Qt::Widget);
	mColorDialog->setOptions(QColorDialog::DontUseNativeDialog | QColorDialog::NoButtons);
	mColorDialog->setOption(QColorDialog::ShowAlphaChannel, ShowAlphaChannel);

	Layout->addWidget(mColorDialog);
	mColorDialog->setAccessibleName(tr("Custom Color"));
	mColorDialog->setToolTip(tr("Choose a color or enter its component values."));

	QDialogButtonBox* ButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel|QDialogButtonBox::Help, this);
	Layout->addWidget(ButtonBox);
	swSetupWidgetHelp(this, ButtonBox);

	QObject::connect(ButtonBox, &QDialogButtonBox::accepted, this, &swColorDialogPopup::Accept);
	QObject::connect(ButtonBox, &QDialogButtonBox::rejected, this, &swColorDialogPopup::Reject);
}

void swColorDialogPopup::Accept()
{
	QColor Color = mColorDialog->currentColor();

	emit ColorSelected(Color);

	Close();
}

void swColorDialogPopup::Reject()
{
	Close();
}

void swColorDialogPopup::Close()
{
	QMenu* Menu = qobject_cast<QMenu*>(parent());

	if (Menu)
		Menu->close();
}

swInterfaceIconEngine::swInterfaceIconEngine(const QString& Folder, const QString& Name)
	: mFolder(Folder), mName(Name)
{
	for (int Size : SW_INTERFACE_ICON_SIZES)
	{
		const QString FileName = QString::fromLatin1(":/resources/interface/%1/png/%2/%3_%2.png").arg(Folder).arg(Size).arg(Name);

		if (QFile::exists(FileName))
			mIcon.addFile(FileName, QSize(Size, Size));
	}
}

void swInterfaceIconEngine::paint(QPainter* Painter, const QRect& Rect, QIcon::Mode Mode, QIcon::State State)
{
	const qreal PixelRatio = Painter->device() ? Painter->device()->devicePixelRatioF() : 1.0;
	QPixmap Pixmap = pixmap(Rect.size() * PixelRatio, Mode, State);

	Pixmap.setDevicePixelRatio(PixelRatio);
	Painter->drawPixmap(Rect, Pixmap);
}

QPixmap swInterfaceIconEngine::pixmap(const QSize& Size, QIcon::Mode Mode, QIcon::State State)
{
	const QPalette Palette = QApplication::palette();
	QColor Color;

	switch (Mode)
	{
	case QIcon::Disabled:
		Color = Palette.color(QPalette::Disabled, QPalette::WindowText);
		break;

	case QIcon::Selected:
		Color = Palette.color(QPalette::Active, QPalette::HighlightedText);
		break;

	case QIcon::Normal:
	case QIcon::Active:
		Color = Palette.color(QPalette::Active, QPalette::WindowText);
		break;
	}

	const QString CacheKey = QString::fromLatin1("swInterfaceIcon:%1/%2:%3x%4:%5:%6").arg(mFolder, mName).arg(Size.width()).arg(Size.height()).arg(static_cast<int>(State)).arg(Color.rgba(), 0, 16);
	QPixmap Pixmap;

	if (QPixmapCache::find(CacheKey, &Pixmap))
		return Pixmap;

	QImage Image = mIcon.pixmap(Size, QIcon::Normal, State).toImage().convertToFormat(QImage::Format_ARGB32);

	// grey pixels take the text color and colored accents stay unless the icon is disabled
	for (int Y = 0; Y < Image.height(); Y++)
	{
		QRgb* Line = reinterpret_cast<QRgb*>(Image.scanLine(Y));

		for (int X = 0; X < Image.width(); X++)
		{
			const QRgb Pixel = Line[X];
			const int Maximum = qMax(qRed(Pixel), qMax(qGreen(Pixel), qBlue(Pixel)));
			const int Minimum = qMin(qRed(Pixel), qMin(qGreen(Pixel), qBlue(Pixel)));

			if (Mode == QIcon::Disabled || Maximum - Minimum < 32)
				Line[X] = qRgba(Color.red(), Color.green(), Color.blue(), qAlpha(Pixel) * Color.alpha() / 255);
		}
	}

	Pixmap = QPixmap::fromImage(Image);
	QPixmapCache::insert(CacheKey, Pixmap);

	return Pixmap;
}

QSize swInterfaceIconEngine::actualSize(const QSize& Size, QIcon::Mode Mode, QIcon::State State)
{
	return mIcon.actualSize(Size, Mode, State);
}

QIconEngine* swInterfaceIconEngine::clone() const
{
	return new swInterfaceIconEngine(mFolder, mName);
}

QIcon swCreateInterfaceIcon(const QString& Folder, const QString& Name)
{
	return QIcon(new swInterfaceIconEngine(Folder, Name));
}
