#pragma once

#include <QObject>

class QLocale;
class swPartSelectionWidget;
class swPartSelectionListView;
struct swTrainTrackConnectionType;

constexpr int SW_MAXIMUM_IMAGE_DIMENSION = 32768;
constexpr int SW_MAXIMUM_GRID_LINE_SPACING = 10000;
constexpr int SW_MAXIMUM_AREA_LIGHT_GRID = 64;

QString swFormatValue(float Value, int Precision);
QString swFormatFloatLocalized(float Value, QLocale Locale);
QString swFormatDoubleLocalized(double Value, QLocale Locale, char Format, int Precision);
QString swFormatIntegerLocalized(qlonglong Value, QLocale Locale);
std::optional<float> swParseFloatLocalized(const QString& Value, QLocale Locale);

void swSetupDialogHelp(QDialog* Dialog, QDialogButtonBox* ButtonBox);
void swSetupWidgetHelp(QWidget* Root, QDialogButtonBox* ButtonBox);

// Gives dialogs and secondary windows a usable size the first time they are shown.
class swWindowGeometryFilter : public QObject
{
public:
	explicit swWindowGeometryFilter(QObject* Parent)
		: QObject(Parent)
	{
	}

protected:
	bool eventFilter(QObject* Object, QEvent* Event) override;
};

// Keeps a panel exactly as tall as its contents, updated whenever its layout changes.
void swFitHeightToContents(QWidget* Widget);

// Shows a modal progress dialog for a blocking task once it lasts longer than a moment.
class swBlockingProgress
{
	Q_DECLARE_TR_FUNCTIONS(swBlockingProgress);

public:
	swBlockingProgress(const QString& Title, const QString& Label, bool Cancelable);
	~swBlockingProgress();

	swBlockingProgress(const swBlockingProgress&) = delete;
	swBlockingProgress& operator=(const swBlockingProgress&) = delete;

	void SetProgress(int Value, int Maximum);
	bool IsCanceled() const;

protected:
	QString mTitle;
	QString mLabel;
	bool mCancelable;
	bool mNested;
	QElapsedTimer mTimer;
	QElapsedTimer mUpdateTimer;
	QProgressDialog* mDialog = nullptr;

	static swBlockingProgress* mActive;
};

// Panel rows are plain widgets with a standard row layout, so every panel has the spacing of the minifigure builder panel.
QSize swGetPanelButtonSize();
int swGetLayoutSpacing(const QWidget* Widget, Qt::Orientation Orientation);
void swSetupPanelButton(QToolButton* Button);
void swSetupPanelEdit(QWidget* Edit);
void swSetupPanelSpinBox(QAbstractSpinBox* SpinBox);
QWidget* swCreatePanelRow(QWidget* Parent);
void swAddPanelRowWidget(QWidget* Row, QWidget* Widget);
void swAddPanelRowStretch(QWidget* Row);
QToolButton* swAddPanelActionButton(QWidget* Row, QAction* Action);
QToolButton* swAddPanelOptionsButton(QWidget* Row, QMenu* Menu, const QString& ToolTip, const QString& AccessibleName);

// Interface icons are white on transparent and drawn in the palette text color, so they follow theme changes.
// Each folder keeps the Blender layout, png/<size>/<name>_<size>.png, for every size in SW_INTERFACE_ICON_SIZES.
constexpr int SW_INTERFACE_ICON_SIZES[] = { 16, 24, 32, 48 };

class swInterfaceIconEngine : public QIconEngine
{
public:
	swInterfaceIconEngine(const QString& Folder, const QString& Name);

	void paint(QPainter* Painter, const QRect& Rect, QIcon::Mode Mode, QIcon::State State) override;
	QPixmap pixmap(const QSize& Size, QIcon::Mode Mode, QIcon::State State) override;
	QSize actualSize(const QSize& Size, QIcon::Mode Mode, QIcon::State State) override;
	QIconEngine* clone() const override;

protected:
	QString mFolder;
	QString mName;
	QIcon mIcon;
};

QIcon swCreateInterfaceIcon(const QString& Folder, const QString& Name);

// ScreenPosition selects the monitor independently of the desired popup position.
QPoint swGetPopupPosition(const QPoint& Position, const QSize& Size, const QPoint& ScreenPosition);

#ifdef Q_OS_WIN

#include <windows.h>

int swTerminateChildProcess(QWidget* Parent, const qint64 Pid, const qint64 Ppid);
bool swRunElevatedProcess(const LPCWSTR ExeName, const LPCWSTR Arguments, const LPCWSTR WorkingFolder);

#endif

class swTreeWidgetColumnStretcher : public QObject
{
	Q_OBJECT

public:
	swTreeWidgetColumnStretcher(QTreeWidget* TreeWidget, int ColumnToStretch);

	bool eventFilter(QObject* Object, QEvent* Event) override;

private slots:
	void SectionResized(int LogicalIndex, int OldSize, int NewSize);

private:
	const int mColumnToStretch;
	bool mInteractiveResize;
	int mStretchWidth;
};

class swSmallLineEdit : public QLineEdit
{
	Q_OBJECT

public:
	QSize sizeHint() const override
	{
		QFontMetrics FontMetrics(font());

#if (QT_VERSION >= QT_VERSION_CHECK(5, 11, 0))
		const int Width = FontMetrics.horizontalAdvance(QLatin1Char('x')) * 10;
#else
		const int Width = FontMetrics.width(QLatin1Char('x')) * 10;
#endif

		return QLineEdit::sizeHint() - QSize(Width, 0);
	}
};

class swTransformLineEdit : public swSmallLineEdit
{
	Q_OBJECT

protected:
	bool event(QEvent* Event) override
	{
		if (Event->type() == QEvent::ShortcutOverride)
		{
			const QKeyEvent* KeyEvent = (QKeyEvent*)Event;
			const int Key = KeyEvent->key();

			if (KeyEvent->modifiers() == Qt::NoModifier && Key >= Qt::Key_A && Key <= Qt::Key_Z)
				Event->accept();

			switch (Key)
			{
			case Qt::Key_Down:
			case Qt::Key_Up:
			case Qt::Key_Left:
			case Qt::Key_Right:
			case Qt::Key_Home:
			case Qt::Key_End:
			case Qt::Key_PageUp:
			case Qt::Key_PageDown:
			case Qt::Key_Plus:
			case Qt::Key_Minus:
			case Qt::Key_Enter:
				Event->accept();
				break;
			}
		}

		return QLineEdit::event(Event);
	}
};

class swStepValidator : public QIntValidator
{
	Q_OBJECT

public:
	swStepValidator(swStep Min, swStep Max, bool AllowEmpty, QObject* Parent)
		: QIntValidator(1, INT_MAX, Parent), mMin(Min), mMax(Max), mAllowEmpty(AllowEmpty)
	{
	}

	QValidator::State validate(QString& Input, int& Pos) const override
	{
		if (mAllowEmpty && Input.isEmpty())
			return Acceptable;

		bool Ok;
		swStep Step = locale().toUInt(Input, &Ok);

		if (Ok)
			return (Step >= mMin && Step <= mMax) ? Acceptable : Invalid;

		return QIntValidator::validate(Input, Pos);
	}

protected:
	swStep mMin;
	swStep mMax;
	bool mAllowEmpty;
};

class swElidableToolButton : public QToolButton
{
	Q_OBJECT

public:
	swElidableToolButton(QWidget* Parent)
		: QToolButton(Parent)
	{
	}

	QSize sizeHint() const override
	{
		QSize Size = QToolButton::sizeHint();

		Size.setWidth(0);

		return Size;
	}

protected:
	void paintEvent(QPaintEvent*) override
	{
		QStylePainter Painter(this);
		QStyleOptionToolButton Option;
		initStyleOption(&Option);

		QRect Button = style()->subControlRect(QStyle::CC_ToolButton, &Option, QStyle::SC_ToolButton, this);
		int Frame = style()->proxy()->pixelMetric(QStyle::PixelMetric::PM_DefaultFrameWidth, &Option, this);
		Button = Button.adjusted(Frame, Frame, -Frame, -Frame);

		QFontMetrics Metrics(font());
		QString ElidedText = Metrics.elidedText(text(), Qt::ElideMiddle, Button.width());

		Option.text = ElidedText;
		Painter.drawComplexControl(QStyle::CC_ToolButton, Option);
	}
};

class swPieceIdStringModel : public QAbstractListModel
{
	Q_OBJECT

public:
	swPieceIdStringModel(swModel* Model, QObject* Parent);

	QModelIndex Index(swPieceData* PieceData) const;
	std::vector<bool> GetFilteredRows(const QString& FilterText) const;

	int rowCount(const QModelIndex& Parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& Index, int Role = Qt::DisplayRole) const override;

protected:
	std::vector<swPieceData*> mSortedPieces;
};

class swColorDialogPopup : public QWidget
{
	Q_OBJECT

public:
	swColorDialogPopup(const QColor& InitialColor, QWidget* Parent);

signals:
	void ColorSelected(QColor Color);

protected slots:
	void Accept();
	void Reject();

protected:
	void Close();

	QColorDialog* mColorDialog = nullptr;
};
