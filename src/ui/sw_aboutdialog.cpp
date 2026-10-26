#include "sw_global.h"
#include "sw_utils.h"
#include "sw_aboutdialog.h"
#include "ui_sw_aboutdialog.h"
#include "sw_mainwindow.h"
#include "sw_view.h"
#include "sw_glextensions.h"
#include "sw_viewwidget.h"

swAboutDialog::swAboutDialog(QWidget* Parent)
	: QDialog(Parent), ui(new Ui::swAboutDialog)
{
	ui->setupUi(this);
	swSetupDialogHelp(this, ui->buttonBox);

#ifdef SW_CONTINUOUS_BUILD
	ui->version->setText(tr("StudWorks Continuous Build %1").arg(QString::fromLatin1(QT_STRINGIFY(SW_CONTINUOUS_BUILD))));
#else
	ui->version->setText(tr("StudWorks Version %1").arg(QString::fromLatin1(SW_VERSION_TEXT)));
#endif

	swViewWidget* Widget = gMainWindow->GetActiveView()->GetWidget();
	QOpenGLContext* Context = Widget->context();
	QOpenGLFunctions* Functions = Context->functions();
	QSurfaceFormat Format = Context->format();

	int ColorDepth = Format.redBufferSize() + Format.greenBufferSize() + Format.blueBufferSize() + Format.alphaBufferSize();

	const QString QtVersionFormat = tr("Qt Version %1 (compiled with %2)\n\n");
	const QString QtVersion = QtVersionFormat.arg(qVersion(), QT_VERSION_STR);

	const QString VersionFormat = tr("OpenGL Version %1 (GLSL %2)\n%3 - %4\n\n");
	const QString GLVersion(reinterpret_cast<const char*>(Functions->glGetString(GL_VERSION)));
	const QString ShaderVersion(reinterpret_cast<const char*>(Functions->glGetString(GL_SHADING_LANGUAGE_VERSION)));
	const QString Renderer(reinterpret_cast<const char*>(Functions->glGetString(GL_RENDERER)));
	const QString Vendor(reinterpret_cast<const char*>(Functions->glGetString(GL_VENDOR)));
	const QString Version = VersionFormat.arg(GLVersion, ShaderVersion, Renderer, Vendor);
	const QString BuffersFormat = tr("Color Buffer: %1 bits\nDepth Buffer: %2 bits\nStencil Buffer: %3 bits\n\n");
	const QString Buffers = BuffersFormat.arg(QString::number(ColorDepth), QString::number(Format.depthBufferSize()), QString::number(Format.stencilBufferSize()));

	const QString ExtensionsFormat = tr("Buffers: %1\nFramebuffers: %2\nBlendFuncSeparate: %3\nAnisotropic: %4\n");
	const QString VertexBuffers = gSupportsVertexBufferObject ? tr("Supported") : tr("Not supported");
	const QString Framebuffers = gSupportsFramebufferObject ? tr("Supported") : tr("Not supported");
	const QString BlendFuncSeparate = gSupportsBlendFuncSeparate ? tr("Supported") : tr("Not supported");
	const QString Anisotropic = gSupportsAnisotropic ? tr("Supported (max %1)").arg(gMaxAnisotropy) : tr("Not supported");

	const QString Extensions = ExtensionsFormat.arg(VertexBuffers, Framebuffers, BlendFuncSeparate, Anisotropic);

	ui->info->setText(QtVersion + Version + Buffers + Extensions);
}

swAboutDialog::~swAboutDialog()
{
	delete ui;
}
