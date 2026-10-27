#include "sw_global.h"
#include "sw_view.h"
#include "sw_view_widget.h"
#include <stdlib.h>
#include "sw_window_main.h"
#include "sw_object_light.h"
#include "sw_object_camera.h"
#include "sw_texture_font.h"
#include "sw_texture.h"
#include "sw_object_piece.h"
#include "sw_data_piece.h"
#include "sw_track_train.h"
#include "sw_scene.h"
#include "sw_context.h"
#include "sw_view_manipulator.h"
#include "sw_view_sphere.h"
#include "sw_widget_find_replace.h"
#include "sw_library.h"
#include "sw_window_utilities.h"

std::vector<QImage> swView::GetStepImages(swStep Start, swStep End)
{
	std::vector<QImage> Images;
	mModel->WaitForAssets();

	if (!BeginRenderToImage(mWidth, mHeight))
	{
		if (gMainWindow)
			QMessageBox::warning(gMainWindow, tr("Create Step Images"), tr("Error creating images."));

		return Images;
	}

	const swStep CurrentStep = mModel->GetCurrentStep();
	bool RenderFailed = false;

	for (swStep Step = Start; Step <= End; Step++)
	{
		mModel->SetTemporaryStep(Step);

		OnDraw();

		QImage Image = GetRenderImage();

		if (Image.isNull())
		{
			RenderFailed = true;
			Images.clear();
			break;
		}

		Images.emplace_back(std::move(Image));
	}

	EndRenderToImage();

	mModel->SetTemporaryStep(CurrentStep);

	if (!mModel->IsActive())
		mModel->CalculateStep(SW_STEP_MAXIMUM);

	if (RenderFailed && gMainWindow)
		QMessageBox::warning(gMainWindow, tr("Create Step Images"), tr("Error creating images."));

	return Images;
}

swResult<void> swView::SaveStepImages(const QString& BaseName, bool AddStepSuffix, swStep Start, swStep End, std::function<void(const QString&)> ProgressCallback)
{
	std::vector<QImage> Images = GetStepImages(Start, End);

	if (Images.size() != static_cast<size_t>(End - Start + 1))
		return swUnexpected(tr("Error creating images."));

	for (swStep Step = Start; Step <= End; Step++)
	{
		QString FileName;

		if (AddStepSuffix)
			FileName = BaseName.arg(Step, 2, 10, QLatin1Char('0'));
		else
			FileName = BaseName;

		QImageWriter Writer(FileName);

		if (Writer.format().isEmpty())
			Writer.setFormat("png");

		if (!Writer.write(Images[Step - Start]))
			return swUnexpected(tr("Error writing to file '%1':\n%2").arg(FileName, Writer.errorString()));

		if (ProgressCallback)
			ProgressCallback(FileName);
	}

	return swResult<void>();
}

bool swView::BeginRenderToImage(int Width, int Height)
{
	swContext* Context = swContext::GetGlobalOffscreenContext();
	int MaxTexture = qMin(Context->GetMaxTextureSize(), 2048);

	const int Samples = QSurfaceFormat::defaultFormat().samples();
	if (Samples > 1)
		MaxTexture /= Samples;

	int TileWidth = qMin(Width, MaxTexture);
	int TileHeight = qMin(Height, MaxTexture);

	mWidth = TileWidth;
	mHeight = TileHeight;
	mRenderImage = QImage(Width, Height, QImage::Format_ARGB32);

	QOpenGLFramebufferObjectFormat Format;
	Format.setAttachment(QOpenGLFramebufferObject::Depth);

	if (QSurfaceFormat::defaultFormat().samples() > 1)
		Format.setSamples(QSurfaceFormat::defaultFormat().samples());

	mRenderFramebuffer = std::unique_ptr<QOpenGLFramebufferObject>(new QOpenGLFramebufferObject(QSize(TileWidth, TileHeight), Format));

	if (!mRenderFramebuffer->bind())
	{
		mRenderFramebuffer.reset();
		mRenderImage = QImage();
		return false;
	}

	return true;
}

void swView::EndRenderToImage()
{
	mRenderFramebuffer.reset();
}

QImage swView::GetRenderImage() const
{
	return mRenderImage;
}

void swView::DestroyOutlineFramebuffers()
{
	mOutlineFullFramebuffer.reset();
	mOutlineVisibleFramebuffer.reset();
	mOutlineAttemptSize = QSize();
}

bool swView::PrepareOutlineFramebuffers()
{
	QOpenGLContext* GLContext = mContext->GetGLContext();
	
	// Framebuffer objects are context-local under core/ARB OpenGL.
	if (mOutlineGLContext != GLContext)
	{
		DestroyOutlineFramebuffers();
		mOutlineGLContext = GLContext;
	}

	if (!GLContext || !swContext::SupportsSelectionOutline() || mWidth <= 0 || mHeight <= 0)
		return false;

	const QSize Size(mWidth, mHeight);
	
	if (mOutlineAttemptSize == Size)
		return mOutlineFullFramebuffer && mOutlineVisibleFramebuffer;

	swScopedGLContextState GLState;
	
	DestroyOutlineFramebuffers();
	
	mOutlineAttemptSize = Size;
	mOutlineFullFramebuffer = mContext->CreateFramebuffer(mWidth, mHeight);
	mOutlineVisibleFramebuffer = mContext->CreateFramebuffer(mWidth, mHeight);

	if (!mOutlineFullFramebuffer || !mOutlineVisibleFramebuffer)
	{
		mOutlineFullFramebuffer.reset();
		mOutlineVisibleFramebuffer.reset();
		
		qWarning("Selection outline framebuffer creation failed; using face tint and colored edges.");
		
		return false;
	}

	return true;
}

bool swView::GenerateOutlineMasks()
{
	if (mOutlineFullFramebuffer && mOutlineVisibleFramebuffer &&
		mContext->GenerateOutlineMasks(*mScene, *mOutlineFullFramebuffer, *mOutlineVisibleFramebuffer))
		return true;

	mOutlineFullFramebuffer.reset();
	mOutlineVisibleFramebuffer.reset();

	qWarning("Selection outline pass failed; using face tint and colored edges.");

	return false;
}

void swView::BindRenderFramebuffer()
{
	mRenderFramebuffer->bind();
}

void swView::UnbindRenderFramebuffer()
{
	mRenderFramebuffer->release();
}

QImage swView::GetRenderFramebufferImage() const
{
	return mRenderFramebuffer->toImage();
}
