#include "sw_global.h"
#include "sw_dialog_instructions_print.h"
#include "sw_dialog_instructions_page.h"
#include "sw_project.h"
#include "sw_model.h"
#include "sw_window_utilities.h"
#include <QPrinterInfo>

swInstructionsPrintWidget::swInstructionsPrintWidget(QWidget* Parent, swInstructions* Instructions)
	: QDockWidget(Parent), mInstructions(Instructions)
{
	setWindowTitle(tr("Print"));
	setWhatsThis(tr("<p><b>Print</b></p><p>Print the instruction pages on a printer or to a PDF file. The pages are printed on paper matching the page setup, the pages panel shows how they look.</p>"));
	swSetupWidgetHelp(this, nullptr);

	QWidget* CentralWidget = new QWidget(this);
	setWidget(CentralWidget);

	QVBoxLayout* Layout = new QVBoxLayout(CentralWidget);

	auto CreateRow = [CentralWidget, Layout](const QString& Name, QLabel** Label)
	{
		QWidget* Row = swCreatePanelRow(CentralWidget);
		Layout->addWidget(Row);

		*Label = new QLabel(Name, Row);
		Row->layout()->addWidget(*Label);

		return Row;
	};

	auto SetupEdit = [](QWidget* Edit, const QString& Name, const QString& ToolTip)
	{
		Edit->setToolTip(ToolTip);
		Edit->setWhatsThis(ToolTip);
		Edit->setAccessibleName(Name);
		Edit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	};

	auto CreateSpinBox = [SetupEdit](QWidget* Row, const QString& Name, const QString& ToolTip)
	{
		QSpinBox* SpinBox = new QSpinBox(Row);
		SetupEdit(SpinBox, Name, ToolTip);
		swSetupPanelSpinBox(SpinBox);
		static_cast<QHBoxLayout*>(Row->layout())->addWidget(SpinBox, 1);

		return SpinBox;
	};

	QLabel* PrinterLabel = nullptr;
	QWidget* PrinterRow = CreateRow(tr("Printer"), &PrinterLabel);

	mPrinterComboBox = new QComboBox(PrinterRow);
	SetupEdit(mPrinterComboBox, tr("Printer"), tr("Choose the printer, or print the pages to a PDF file."));
	swSetupPanelEdit(mPrinterComboBox);
	static_cast<QHBoxLayout*>(PrinterRow->layout())->addWidget(mPrinterComboBox, 1);
	PrinterLabel->setBuddy(mPrinterComboBox);

	QLabel* FileLabel = nullptr;
	mFileRow = CreateRow(tr("File"), &FileLabel);

	mFileEdit = new QLineEdit(mFileRow);
	SetupEdit(mFileEdit, tr("File"), tr("The PDF file the pages are printed to."));
	swSetupPanelEdit(mFileEdit);
	static_cast<QHBoxLayout*>(mFileRow->layout())->addWidget(mFileEdit, 1);
	FileLabel->setBuddy(mFileEdit);

	QToolButton* BrowseButton = new QToolButton(mFileRow);
	BrowseButton->setText(tr("..."));
	BrowseButton->setToolTip(tr("Choose the PDF file."));
	BrowseButton->setWhatsThis(BrowseButton->toolTip());
	BrowseButton->setAccessibleName(tr("Browse"));
	swSetupPanelButton(BrowseButton);
	mFileRow->layout()->addWidget(BrowseButton);

	QLabel* PagesLabel = nullptr;
	QWidget* PagesRow = CreateRow(tr("Pages"), &PagesLabel);

	mFromSpinBox = CreateSpinBox(PagesRow, tr("First Page"), tr("The first page to print."));
	PagesLabel->setBuddy(mFromSpinBox);

	QLabel* ToLabel = new QLabel(tr("to"), PagesRow);
	PagesRow->layout()->addWidget(ToLabel);

	mToSpinBox = CreateSpinBox(PagesRow, tr("Last Page"), tr("The last page to print."));

	// the range starts as all pages, it ends on the last page
	mFromSpinBox->setRange(1, 1);
	mToSpinBox->setRange(1, 1);

	QLabel* CopiesLabel = nullptr;
	QWidget* CopiesRow = CreateRow(tr("Copies"), &CopiesLabel);

	mCopiesSpinBox = CreateSpinBox(CopiesRow, tr("Copies"), tr("The number of copies to print."));
	mCopiesSpinBox->setRange(1, 999);
	CopiesLabel->setBuddy(mCopiesSpinBox);

	mCollateButton = new QToolButton(CopiesRow);
	mCollateButton->setText(tr("Collate"));
	mCollateButton->setCheckable(true);
	mCollateButton->setChecked(true);
	mCollateButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
	mCollateButton->setToolTip(tr("Print each copy completely before the next one instead of the copies of each page together."));
	mCollateButton->setWhatsThis(mCollateButton->toolTip());
	mCollateButton->setAccessibleName(tr("Collate"));
	swSetupPanelButton(mCollateButton);
	CopiesRow->layout()->addWidget(mCollateButton);

	// the print button spans the panel below its settings like add to model in the minifigure builder
	QWidget* PrintRow = swCreatePanelRow(CentralWidget);
	Layout->addWidget(PrintRow);

	QToolButton* PrintButton = new QToolButton(PrintRow);
	PrintButton->setText(tr("Print"));
	PrintButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
	PrintButton->setToolTip(tr("Print the chosen pages."));
	PrintButton->setWhatsThis(PrintButton->toolTip());
	PrintButton->setAccessibleName(tr("Print"));
	swSetupPanelButton(PrintButton);
	PrintButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	static_cast<QHBoxLayout*>(PrintRow->layout())->addWidget(PrintButton, 1);

	// the labels share one width so the inputs line up
	const int LabelWidth = qMax(qMax(PrinterLabel->sizeHint().width(), FileLabel->sizeHint().width()), qMax(PagesLabel->sizeHint().width(), CopiesLabel->sizeHint().width()));
	PrinterLabel->setFixedWidth(LabelWidth);
	FileLabel->setFixedWidth(LabelWidth);
	PagesLabel->setFixedWidth(LabelWidth);
	CopiesLabel->setFixedWidth(LabelWidth);

	connect(mPrinterComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, &swInstructionsPrintWidget::UpdateFileRow);
	connect(BrowseButton, &QToolButton::clicked, this, &swInstructionsPrintWidget::BrowseFile);
	connect(mFromSpinBox, qOverload<int>(&QSpinBox::valueChanged), mToSpinBox, [this](int Page)
	{
		mToSpinBox->setValue(qMax(Page, mToSpinBox->value()));
	});
	connect(mToSpinBox, qOverload<int>(&QSpinBox::valueChanged), mFromSpinBox, [this](int Page)
	{
		mFromSpinBox->setValue(qMin(Page, mFromSpinBox->value()));
	});
	connect(PrintButton, &QToolButton::clicked, this, &swInstructionsPrintWidget::Print);

	// the printers are listed again whenever the panel opens so new printers show up
	connect(this, &QDockWidget::visibilityChanged, this, [this](bool Visible)
	{
		if (Visible)
			UpdatePrinters();
	});

	connect(mInstructions, &swInstructions::PageSetupChanged, this, &swInstructionsPrintWidget::UpdatePages);
	connect(mInstructions, &swInstructions::PageLayoutChanged, this, &swInstructionsPrintWidget::UpdatePages);

	// the panel is exactly as tall as its rows wherever it is docked
	swFitHeightToContents(CentralWidget);

	UpdatePrinters();
	UpdatePages();
}

void swInstructionsPrintWidget::UpdatePrinters()
{
	QSignalBlocker Blocker(mPrinterComboBox);

	// the printer names are the item data, the pdf file has none
	const QString CurrentPrinter = mPrinterComboBox->count() ? mPrinterComboBox->currentData().toString() : QPrinterInfo::defaultPrinterName();
	const bool CurrentFile = mPrinterComboBox->count() && CurrentPrinter.isEmpty();

	mPrinterComboBox->clear();

	for (const QString& PrinterName : QPrinterInfo::availablePrinterNames())
		mPrinterComboBox->addItem(PrinterName, PrinterName);

	mPrinterComboBox->addItem(tr("PDF File"), QString());

	const int PrinterIndex = CurrentFile ? -1 : mPrinterComboBox->findData(CurrentPrinter);
	mPrinterComboBox->setCurrentIndex(PrinterIndex != -1 ? PrinterIndex : mPrinterComboBox->count() - 1);

	UpdateFileRow();
}

void swInstructionsPrintWidget::UpdatePages()
{
	QSignalBlocker FromBlocker(mFromSpinBox);
	QSignalBlocker ToBlocker(mToSpinBox);

	// a range that ended on the last page keeps ending there when pages are added or removed
	const int PageCount = qMax(static_cast<int>(mInstructions->mPages.size()), 1);
	const bool LastPage = mToSpinBox->value() == mToSpinBox->maximum();

	mFromSpinBox->setRange(1, PageCount);
	mToSpinBox->setRange(1, PageCount);

	if (LastPage)
		mToSpinBox->setValue(PageCount);

	mFromSpinBox->setValue(qMin(mFromSpinBox->value(), mToSpinBox->value()));
}

void swInstructionsPrintWidget::UpdateFileRow()
{
	mFileRow->setVisible(mPrinterComboBox->currentData().toString().isEmpty());
}

bool swInstructionsPrintWidget::BrowseFile()
{
	const QString FileName = QFileDialog::getSaveFileName(this, tr("Print to File"), mFileEdit->text(), tr("PDF Files (*.pdf);;All Files (*.*)"));

	if (FileName.isEmpty())
		return false;

	mFileEdit->setText(FileName);

	return true;
}

void swInstructionsPrintWidget::Print()
{
#ifndef QT_NO_PRINTER
	const QString PrinterName = mPrinterComboBox->currentData().toString();

	if (PrinterName.isEmpty() && mFileEdit->text().isEmpty() && !BrowseFile())
		return;

	QPrinter Printer(QPrinter::HighResolution);

	if (PrinterName.isEmpty())
	{
		Printer.setOutputFormat(QPrinter::PdfFormat);
		Printer.setOutputFileName(mFileEdit->text());
	}
	else
		Printer.setPrinterName(PrinterName);

	swSetupInstructionsPrinter(mInstructions, &Printer);
	Printer.setFromTo(mFromSpinBox->value(), mToSpinBox->value());
	Printer.setCopyCount(mCopiesSpinBox->value());
	Printer.setCollateCopies(mCollateButton->isChecked());

	swPrintInstructions(mInstructions, &Printer);
#endif
}

#ifndef QT_NO_PRINTER
static void swSetInstructionsPageLayout(swInstructions* Instructions, QPrinter* Printer)
{
	// Print on paper matching the page setup, the pages already include their margins.
	const swInstructionsPageSetup& PageSetup = Instructions->mPageSetup;
	const QSizeF PageSize(qMin(PageSetup.Width, PageSetup.Height), qMax(PageSetup.Width, PageSetup.Height));
	const QPageLayout::Orientation Orientation = PageSetup.Width > PageSetup.Height ? QPageLayout::Landscape : QPageLayout::Portrait;

	Printer->setPageLayout(QPageLayout(QPageSize(PageSize, QPageSize::Point), Orientation, QMarginsF()));
	Printer->setFullPage(true);
}
#endif

void swSetupInstructionsPrinter(swInstructions* Instructions, QPrinter* Printer)
{
#ifndef QT_NO_PRINTER
	swSetInstructionsPageLayout(Instructions, Printer);
	Printer->setFromTo(1, static_cast<int>(Instructions->mPages.size()));
#endif
}

void swPrintInstructions(swInstructions* Instructions, QPrinter* Printer)
{
#ifndef QT_NO_PRINTER
	int DocCopies;
	int PageCopies;

	const int PageCount = static_cast<int>(Instructions->mPages.size());

	// The print dialog resets the paper when printing to a file, a file always gets the page setup paper.
	if (Printer->outputFormat() == QPrinter::PdfFormat)
		swSetInstructionsPageLayout(Instructions, Printer);

	if (Printer->collateCopies())
	{
		DocCopies = 1;
		PageCopies = Printer->supportsMultipleCopies() ? 1 : Printer->copyCount();
	}
	else
	{
		DocCopies = Printer->supportsMultipleCopies() ? 1 : Printer->copyCount();
		PageCopies = 1;
	}

	int FromPage = Printer->fromPage();
	int ToPage = Printer->toPage();
	bool Ascending = true;

	if (FromPage == 0 && ToPage == 0)
	{
		FromPage = 1;
		ToPage = PageCount;
	}

	FromPage = qMax(1, FromPage);
	ToPage = qMin(PageCount, ToPage);

	if (ToPage < FromPage)
		return;

	if (Printer->pageOrder() == QPrinter::LastPageFirst)
	{
		std::swap(FromPage, ToPage);
		Ascending = false;
	}

	// Pages keep their proportions on any paper, centered on a sheet filled with the page background.
	const swInstructionsPageSetup& PageSetup = Instructions->mPageSetup;
	const QRectF SheetRect(0.0, 0.0, Printer->width(), Printer->height());
	const QSizeF PageSize = QSizeF(PageSetup.Width, PageSetup.Height).scaled(SheetRect.size(), Qt::KeepAspectRatio);
	const QRectF PageRect(QPointF((SheetRect.width() - PageSize.width()) / 2.0, (SheetRect.height() - PageSize.height()) / 2.0), PageSize);

	const int SheetCount = DocCopies * PageCopies * (qAbs(ToPage - FromPage) + 1);
	int SheetsPrinted = 0;
	swBlockingProgress Progress(QCoreApplication::translate("swInstructionsDialog", "Printing"), QCoreApplication::translate("swInstructionsDialog", "Printing pages..."), true);

	QGraphicsScene Scene;
	QPainter Painter(Printer);
	bool FirstPage = true;

	for (int DocCopy = 0; DocCopy < DocCopies; DocCopy++)
	{
		int Page = FromPage;

		for (;;)
		{
			for (int PageCopy = 0; PageCopy < PageCopies; PageCopy++)
			{
				if (Printer->printerState() == QPrinter::Aborted || Printer->printerState() == QPrinter::Error)
					return;

				Progress.SetProgress(SheetsPrinted++, SheetCount);

				if (Progress.IsCanceled())
				{
					Printer->abort();
					Painter.end();

					// A canceled file would otherwise be left with only the pages printed so far.
					if (Printer->outputFormat() == QPrinter::PdfFormat)
						QFile::remove(Printer->outputFileName());

					return;
				}

				if (!FirstPage)
					Printer->newPage();
				else
					FirstPage = false;

				swUpdateInstructionsPageScene(Instructions, &Instructions->mPages[Page - 1], &Scene);

				Painter.fillRect(SheetRect, Instructions->GetPageBackgroundColor(Page - 1));
				Scene.render(&Painter, PageRect, Scene.sceneRect(), Qt::KeepAspectRatio);
			}

			if (Page == ToPage)
				break;

			if (Ascending)
				Page++;
			else
				Page--;
		}
	}
#endif
}
