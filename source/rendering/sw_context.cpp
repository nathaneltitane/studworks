#include "sw_global.h"
#include "sw_context.h"
#include "sw_extensions_opengl.h"
#include "sw_mesh.h"
#include "sw_texture.h"
#include "sw_colors.h"
#include "sw_window_main.h"
#include "sw_library.h"
#include "sw_texture_font.h"
#include "sw_view.h"
#include "sw_view_sphere.h"
#include "sw_view_manipulator.h"
#include "sw_cache_string.h"
#include "sw_scene.h"

#ifdef SW_OPENGLES
#define GL_ARRAY_BUFFER_ARB GL_ARRAY_BUFFER
#define GL_ELEMENT_ARRAY_BUFFER_ARB GL_ELEMENT_ARRAY_BUFFER
#define GL_STATIC_DRAW_ARB GL_STATIC_DRAW
#endif

std::unique_ptr<QOpenGLContext> swContext::mOffscreenContext;
std::unique_ptr<QOffscreenSurface> swContext::mOffscreenSurface;
std::unique_ptr<swContext> swContext::mGlobalOffscreenContext;
bool swContext::mRenderResourcesCreated = false;
swProgram swContext::mPrograms[static_cast<int>(swMaterialType::Count)];

swScopedGLContextState::swScopedGLContextState()
	: mContext(QOpenGLContext::currentContext()), mSurface(mContext ? mContext->surface() : nullptr)
{
	if (!mContext)
		return;

	QOpenGLFunctions* Functions = mContext->functions();
	mSeparateFramebuffers = QOpenGLFramebufferObject::hasOpenGLFramebufferBlit();

	if (mSeparateFramebuffers)
	{
		Functions->glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &mDrawFramebuffer);
		Functions->glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &mReadFramebuffer);
	}
	else
		Functions->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &mDrawFramebuffer);
}

swScopedGLContextState::~swScopedGLContextState()
{
	QOpenGLContext* CurrentContext = QOpenGLContext::currentContext();

	if (!mContext)
	{
		if (CurrentContext)
			CurrentContext->doneCurrent();

		return;
	}

	if (CurrentContext != mContext || mContext->surface() != mSurface)
		if (!mContext->makeCurrent(mSurface))
			return;

	QOpenGLFunctions* Functions = mContext->functions();

	if (mSeparateFramebuffers)
	{
		Functions->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mDrawFramebuffer);
		Functions->glBindFramebuffer(GL_READ_FRAMEBUFFER, mReadFramebuffer);
	}
	else
		Functions->glBindFramebuffer(GL_FRAMEBUFFER, mDrawFramebuffer);
}

swContext::swContext()
{
	mVertexBufferObject = 0;
	mIndexBufferObject = 0;
	mVertexBufferPointer = nullptr;
	mIndexBufferPointer = nullptr;

	mTexture2D = 0;
	mTextureCubeMap = 0;
	mPolygonOffset = swPolygonOffset::None;
	mDepthWrite = true;
	mDepthFunction = swDepthFunction::LessEqual;
	mDepthTest = true;
	mColorWrite = true;
	mColorBlend = false;
	mCullFace = false;
	mLineWidth = 1.0f;

	mColor = swVector4(0.0f, 0.0f, 0.0f, 0.0f);
	mTextHaloColor = swVector4(0.0f, 0.0f, 0.0f, 0.0f);
	mWorldMatrix = swMatrix44Identity();
	mViewMatrix = swMatrix44Identity();
	mProjectionMatrix = swMatrix44Identity();
	mViewProjectionMatrix = swMatrix44Identity();
	mHighlightParams[0] = swVector4(0.0f, 0.0f, 0.0f, 0.0f);
	mHighlightParams[1] = swVector4(0.0f, 0.0f, 0.0f, 0.0f);
	mHighlightParams[2] = swVector4(0.0f, 0.0f, 0.0f, 0.0f);
	mHighlightParams[3] = swVector4(0.0f, 0.0f, 0.0f, 0.0f);
	mOutlineParams[0] = swVector4(0.0f, 0.0f, 0.0f, 0.0f);
	mOutlineParams[1] = swVector4(0.0f, 0.0f, 0.0f, 0.0f);
	mOutlineParams[2] = swVector4(0.0f, 0.0f, 0.0f, 0.0f);
	mOutlineParams[3] = swVector4(0.0f, 0.0f, 0.0f, 0.0f);
	mLightingStyle = swLightingStyle::Standard;
	mColorDirty = false;
	mTextHaloColorDirty = false;
	mWorldMatrixDirty = false;
	mViewMatrixDirty = false;
	mProjectionMatrixDirty = false;
	mViewProjectionMatrixDirty = false;
	mHighlightParamsDirty = false;
	mOutlineParamsDirty = false;

	mMaterialType = swMaterialType::Count;
}

swContext::~swContext()
{
}

swResult<void> swContext::InitializeRenderer(bool CreateRenderResources)
{
	const swResult<void> OffscreenResult = CreateOffscreenContext();

	if (!OffscreenResult)
		return swUnexpected(OffscreenResult.error());

	mGlobalOffscreenContext = std::unique_ptr<swContext>(new(swContext));

	swContext* Context = mGlobalOffscreenContext.get();

	Context->SetOffscreenContext();

	swInitializeGLExtensions(mOffscreenContext.get());

	if (!CreateRenderResources)
		return swResult<void>();

	if (!Context->hasOpenGLFeature(QOpenGLFunctions::Shaders))
	{
		mGlobalOffscreenContext.reset();
		DestroyOffscreenContext();

		return swUnexpected(QCoreApplication::translate("swContext", "OpenGL shader support is required."));
	}

	const swResult<void> ResourcesResult = Context->CreateResources();

	if (!ResourcesResult)
	{
		Context->DestroyResources();
		mGlobalOffscreenContext.reset();
		DestroyOffscreenContext();

		return swUnexpected(ResourcesResult.error());
	}

	gStringCache.Initialize(Context);

	if (SupportsMaterial(swMaterialType::TextSDF))
		gTexFont.Initialize(Context);

	swView::CreateResources(Context);
	swViewManipulator::CreateResources(Context);

	if (SupportsMaterial(swMaterialType::UnlitViewSphere))
		swViewSphere::CreateResources(Context);

	mRenderResourcesCreated = true;

	if (!SupportsSelectionOutline())
		swGetPreferences().mHighlightRenderMode = swHighlightRenderMode::Tint;

	return swResult<void>();
}

void swContext::ShutdownRenderer()
{
	if (!mGlobalOffscreenContext)
		return;

	mGlobalOffscreenContext->MakeCurrent();
	swContext* Context = mGlobalOffscreenContext.get();

	if (mRenderResourcesCreated)
	{
		gStringCache.Reset();
		gTexFont.Reset();

		swView::DestroyResources(Context);
		Context->DestroyResources();
		swViewManipulator::DestroyResources(Context);
		swViewSphere::DestroyResources(Context);

		mRenderResourcesCreated = false;
	}

	mGlobalOffscreenContext.reset();

	swContext::DestroyOffscreenContext();
}

swContext* swContext::GetGlobalOffscreenContext()
{
	return mGlobalOffscreenContext.get();
}

swResult<void> swContext::CreateOffscreenContext()
{
	std::unique_ptr<QOpenGLContext> OffscreenContext(new QOpenGLContext());

	OffscreenContext->setShareContext(QOpenGLContext::globalShareContext());

	if (!OffscreenContext->create() || !OffscreenContext->isValid())
		return swUnexpected(QCoreApplication::translate("swContext", "Could not create the OpenGL context."));

	std::unique_ptr<QOffscreenSurface> OffscreenSurface(new QOffscreenSurface());

	OffscreenSurface->create();

	if (!OffscreenSurface->isValid())
		return swUnexpected(QCoreApplication::translate("swContext", "Could not create the offscreen OpenGL surface."));

	if (!OffscreenContext->makeCurrent(OffscreenSurface.get()))
		return swUnexpected(QCoreApplication::translate("swContext", "Could not make the offscreen OpenGL context current."));

	mOffscreenContext = std::move(OffscreenContext);
	mOffscreenSurface = std::move(OffscreenSurface);

	return swResult<void>();
}

void swContext::DestroyOffscreenContext()
{
	mOffscreenSurface.reset();
	mOffscreenContext.reset();
}

swResult<void> swContext::CreateShaderPrograms()
{
	const char* ShaderPrefix =
	{
#ifndef SW_OPENGLES
"#version 110\n"
"#define mediump\n"
"#define SW_VERTEX_INPUT attribute\n"
"#define SW_VERTEX_OUTPUT varying\n"
"#define SW_PIXEL_INPUT varying\n"
"#define SW_PIXEL_OUTPUT\n"
"#define SW_SHADER_PRECISION\n"
#else
"#version 300 es\n"
"#define texture2D texture\n"
"#define SW_VERTEX_INPUT in\n"
"#define SW_VERTEX_OUTPUT out\n"
"#define SW_PIXEL_INPUT in mediump\n"
"#define gl_FragColor FragColor\n"
"#define SW_PIXEL_OUTPUT out mediump vec4 gl_FragColor;\n"
"#define SW_SHADER_PRECISION mediump\n"
#endif

// lighting styles follow swLightingStyle: standard, studio, matte, toon
"#define SW_PIXEL_FAKE_LIGHTING \\\n"
"		SW_SHADER_PRECISION vec3 Normal = normalize(PixelNormal); \\\n"
"		SW_SHADER_PRECISION vec3 LightDirection = normalize(PixelPosition - LightPosition); \\\n"
"		SW_SHADER_PRECISION vec3 VertexToEye = normalize(EyePosition - PixelPosition); \\\n"
"		SW_SHADER_PRECISION vec3 LightReflect = normalize(reflect(-LightDirection, Normal)); \\\n"
"		SW_SHADER_PRECISION float KeyLight = abs(dot(Normal, LightDirection)); \\\n"
"		SW_SHADER_PRECISION float Specular = abs(dot(VertexToEye, LightReflect)); \\\n"
"		SW_SHADER_PRECISION float Diffuse = min(KeyLight * 0.6 + 0.65, 1.0); \\\n"
"		SW_SHADER_PRECISION float Rim = 0.0; \\\n"
"		if (LightingStyle == 1) \\\n"
"		{ \\\n"
"			SW_SHADER_PRECISION float FillLight = abs(dot(Normal, VertexToEye)); \\\n"
"			Rim = pow(1.0 - FillLight, 4.0) * 0.18; \\\n"
"			Diffuse = min(0.42 + KeyLight * 0.4 + FillLight * 0.28, 1.0); \\\n"
"			Specular = min(pow(Specular, 24.0), 1.0) * 0.35; \\\n"
"		} \\\n"
"		else if (LightingStyle == 2) \\\n"
"		{ \\\n"
"			Diffuse = KeyLight * 0.4 + 0.6; \\\n"
"			Specular = 0.0; \\\n"
"		} \\\n"
"		else if (LightingStyle == 3) \\\n"
"		{ \\\n"
"			Diffuse = KeyLight > 0.8 ? 1.0 : (KeyLight > 0.4 ? 0.8 : 0.6); \\\n"
"			Specular = Specular > 0.985 ? 0.3 : 0.0; \\\n"
"		} \\\n"
"		else \\\n"
"			Specular = min(pow(Specular, 8.0), 1.0) * 0.25; \\\n"
"		SW_SHADER_PRECISION vec3 SpecularColor = vec3(Specular + Rim, Specular + Rim, Specular + Rim);\n"
	};

	const char* const VertexShaders[] =
	{
		":/resources/shaders/unlit_color_vs.glsl",             // UnlitColor
		":/resources/shaders/unlit_color_conditional_vs.glsl", // UnlitColorConditional
		":/resources/shaders/unlit_texture_modulate_vs.glsl",  // UnlitTextureModulate
		":/resources/shaders/unlit_texture_modulate_vs.glsl",  // TextSDF
		":/resources/shaders/unlit_texture_decal_vs.glsl",     // UnlitTextureDecal
		":/resources/shaders/unlit_vertex_color_vs.glsl",      // UnlitVertexColor
		":/resources/shaders/unlit_view_sphere_vs.glsl",       // UnlitViewSphere
		":/resources/shaders/fakelit_color_vs.glsl",           // FakeLitColor
		":/resources/shaders/fakelit_texture_decal_vs.glsl",   // FakeLitTextureDecal
		":/resources/shaders/unlit_texture_modulate_vs.glsl"   // OutlineComposite
	};

	SW_ARRAY_SIZE_CHECK(VertexShaders, swMaterialType::Count);

	const char* const FragmentShaders[] =
	{
		":/resources/shaders/unlit_color_ps.glsl",             // UnlitColor
		":/resources/shaders/unlit_color_conditional_ps.glsl", // UnlitColorConditional
		":/resources/shaders/unlit_texture_modulate_ps.glsl",  // UnlitTextureModulate
		":/resources/shaders/text_sdf_ps.glsl",                // TextSDF
		":/resources/shaders/unlit_texture_decal_ps.glsl",     // UnlitTextureDecal
		":/resources/shaders/unlit_vertex_color_ps.glsl",      // UnlitVertexColor
		":/resources/shaders/unlit_view_sphere_ps.glsl",       // UnlitViewSphere
		":/resources/shaders/fakelit_color_ps.glsl",           // FakeLitColor
		":/resources/shaders/fakelit_texture_decal_ps.glsl",   // FakeLitTextureDecal
		":/resources/shaders/outline_composite_ps.glsl"      // OutlineComposite
	};

	SW_ARRAY_SIZE_CHECK(FragmentShaders, swMaterialType::Count);

	const auto LoadShader = [this, ShaderPrefix](const char* FileName, GLuint ShaderType) -> swResult<GLuint>
	{
		QFile ShaderFile(FileName);

		if (!ShaderFile.open(QIODevice::ReadOnly))
			return swUnexpected(QCoreApplication::translate("swContext", "Could not read shader %1: %2").arg(QString::fromUtf8(FileName), ShaderFile.errorString()));

		QByteArray Data = ShaderPrefix + ShaderFile.readAll();
		const char* Source = Data.constData();

		const GLuint Shader = glCreateShader(ShaderType);

		if (!Shader)
			return swUnexpected(QCoreApplication::translate("swContext", "Could not create shader %1.").arg(QString::fromUtf8(FileName)));

		glShaderSource(Shader, 1, &Source, nullptr);
		glCompileShader(Shader);

		GLint ShaderCompiled = 0;
		glGetShaderiv(Shader, GL_COMPILE_STATUS, &ShaderCompiled);

		if (ShaderCompiled == GL_FALSE)
		{
			GLint Length = 0;
			glGetShaderiv(Shader, GL_INFO_LOG_LENGTH, &Length);

			QByteArray InfoLog(Length, '\0');

			if (Length > 0)
				glGetShaderInfoLog(Shader, Length, nullptr, InfoLog.data());

			glDeleteShader(Shader);

			return swUnexpected(QCoreApplication::translate("swContext", "Could not compile shader %1:\n%2").arg(QString::fromUtf8(FileName), QString::fromUtf8(InfoLog.constData())));
		}

		return Shader;
	};

	for (int MaterialType = 0; MaterialType < static_cast<int>(swMaterialType::Count); MaterialType++)
	{
		bool Optional = false;

		switch (static_cast<swMaterialType>(MaterialType))
		{
		case swMaterialType::UnlitColorConditional:
		case swMaterialType::TextSDF:
		case swMaterialType::UnlitViewSphere:
		case swMaterialType::FakeLitColor:
		case swMaterialType::FakeLitTextureDecal:
		case swMaterialType::OutlineComposite:
			Optional = true;
			break;

		case swMaterialType::UnlitColor:
		case swMaterialType::UnlitTextureModulate:
		case swMaterialType::UnlitTextureDecal:
		case swMaterialType::UnlitVertexColor:
		case swMaterialType::Count:
			break;
		}

		const swResult<GLuint> VertexResult = LoadShader(VertexShaders[MaterialType], GL_VERTEX_SHADER);

		if (!VertexResult)
		{
			if (Optional)
			{
				qWarning().noquote() << VertexResult.error();
				continue;
			}

			return swUnexpected(VertexResult.error());
		}

		const GLuint VertexShader = VertexResult.value();
		const swResult<GLuint> FragmentResult = LoadShader(FragmentShaders[MaterialType], GL_FRAGMENT_SHADER);

		if (!FragmentResult)
		{
			glDeleteShader(VertexShader);

			if (Optional)
			{
				qWarning().noquote() << FragmentResult.error();
				continue;
			}

			return swUnexpected(FragmentResult.error());
		}

		const GLuint FragmentShader = FragmentResult.value();
		const GLuint Program = glCreateProgram();

		if (!Program)
		{
			glDeleteShader(VertexShader);
			glDeleteShader(FragmentShader);

			const QString Error = QCoreApplication::translate("swContext", "Could not create the shader program for %1 and %2.").arg(QString::fromUtf8(VertexShaders[MaterialType]), QString::fromUtf8(FragmentShaders[MaterialType]));

			if (Optional)
			{
				qWarning().noquote() << Error;
				continue;
			}

			return swUnexpected(Error);
		}

		glAttachShader(Program, VertexShader);
		glAttachShader(Program, FragmentShader);

		glBindAttribLocation(Program, static_cast<int>(swProgramAttribute::Position), "VertexPosition");
		glBindAttribLocation(Program, static_cast<int>(swProgramAttribute::Normal), "VertexNormal");
		glBindAttribLocation(Program, static_cast<int>(swProgramAttribute::TexCoord), "VertexTexCoord");
		glBindAttribLocation(Program, static_cast<int>(swProgramAttribute::Color), "VertexColor");

		glBindAttribLocation(Program, static_cast<int>(swProgramAttribute::ControlPoint1), "VertexPosition1");
		glBindAttribLocation(Program, static_cast<int>(swProgramAttribute::ControlPoint2), "VertexPosition2");
		glBindAttribLocation(Program, static_cast<int>(swProgramAttribute::ControlPoint3), "VertexPosition3");
		glBindAttribLocation(Program, static_cast<int>(swProgramAttribute::ControlPoint4), "VertexPosition4");

		glLinkProgram(Program);

		glDetachShader(Program, VertexShader);
		glDetachShader(Program, FragmentShader);
		glDeleteShader(VertexShader);
		glDeleteShader(FragmentShader);

		GLint IsLinked = 0;
		glGetProgramiv(Program, GL_LINK_STATUS, &IsLinked);

		if (IsLinked == GL_FALSE)
		{
			GLint Length = 0;
			glGetProgramiv(Program, GL_INFO_LOG_LENGTH, &Length);

			QByteArray InfoLog(Length, '\0');

			if (Length > 0)
				glGetProgramInfoLog(Program, Length, nullptr, InfoLog.data());

			glDeleteProgram(Program);

			const QString Error = QCoreApplication::translate("swContext", "Could not link the shader program for %1 and %2:\n%3").arg(QString::fromUtf8(VertexShaders[MaterialType]), QString::fromUtf8(FragmentShaders[MaterialType]), QString::fromUtf8(InfoLog.constData()));

			if (Optional)
			{
				qWarning().noquote() << Error;
				continue;
			}

			return swUnexpected(Error);
		}

		mPrograms[MaterialType].Object = Program;

		mPrograms[MaterialType].WorldViewProjectionMatrixLocation = glGetUniformLocation(Program, "WorldViewProjectionMatrix");
		mPrograms[MaterialType].WorldMatrixLocation = glGetUniformLocation(Program, "WorldMatrix");
		mPrograms[MaterialType].MaterialColorLocation = glGetUniformLocation(Program, "MaterialColor");
		mPrograms[MaterialType].LightPositionLocation = glGetUniformLocation(Program, "LightPosition");
		mPrograms[MaterialType].EyePositionLocation = glGetUniformLocation(Program, "EyePosition");
		mPrograms[MaterialType].HighlightParamsLocation = glGetUniformLocation(Program, "HighlightParams");
		mPrograms[MaterialType].OutlineParamsLocation = glGetUniformLocation(Program, "OutlineParams");
		mPrograms[MaterialType].TextHaloColorLocation = glGetUniformLocation(Program, "TextHaloColor");
		mPrograms[MaterialType].LightingStyleLocation = glGetUniformLocation(Program, "LightingStyle");

		const GLint TextureLocation = glGetUniformLocation(Program, "Texture");

		if (TextureLocation != -1)
		{
			glUseProgram(Program);
			glUniform1i(TextureLocation, 0);
			glUseProgram(0);
		}

		const GLint VisibleTextureLocation = glGetUniformLocation(Program, "VisibleTexture");

		if (VisibleTextureLocation != -1)
		{
			glUseProgram(Program);
			glUniform1i(VisibleTextureLocation, 1);
			glUseProgram(0);
		}
	}

	return swResult<void>();
}

swResult<void> swContext::CreateResources()
{
	return CreateShaderPrograms();
}

void swContext::DestroyResources()
{
	for (int MaterialType = 0; MaterialType < static_cast<int>(swMaterialType::Count); MaterialType++)
	{
		glDeleteProgram(mPrograms[MaterialType].Object);
		mPrograms[MaterialType].Object = 0;
	}
}

void swContext::MakeCurrent()
{
	if (mWidget)
		mWidget->makeCurrent();
	else
		mOffscreenContext->makeCurrent(mOffscreenSurface.get());
}

void swContext::SetGLContext(QOpenGLContext* Context, QOpenGLWidget* Widget)
{
	mContext = Context;
	mWidget = Widget;

	MakeCurrent();
	initializeOpenGLFunctions();
}

void swContext::SetOffscreenContext()
{
	SetGLContext(mOffscreenContext.get(), nullptr);
}

void swContext::SetDefaultState()
{
#ifndef SW_OPENGLES
	if (QSurfaceFormat::defaultFormat().samples() > 1)
		glEnable(GL_LINE_SMOOTH);
#endif

	glEnable(GL_DEPTH_TEST);
	mDepthTest = true;

	glDepthFunc(GL_LEQUAL);
	mDepthFunction = swDepthFunction::LessEqual;

	mColorWrite = true;
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

	mColorBlend = false;
	glDisable(GL_BLEND);

	if (gSupportsBlendFuncSeparate)
		glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE_MINUS_DST_ALPHA, GL_ONE);
	else
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	if (gSupportsVertexBufferObject)
	{
		glBindBuffer(GL_ARRAY_BUFFER_ARB, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
	}

	SetVertexAttribPointer(swProgramAttribute::Position, 3, GL_FLOAT, false, 0, nullptr);
	EnableVertexAttrib(swProgramAttribute::Position);

	DisableVertexAttrib(swProgramAttribute::Normal);
	SetVertexAttribPointer(swProgramAttribute::Normal, 4, GL_BYTE, true, 0, nullptr);
	DisableVertexAttrib(swProgramAttribute::TexCoord);
	SetVertexAttribPointer(swProgramAttribute::TexCoord, 2, GL_FLOAT, false, 0, nullptr);
	DisableVertexAttrib(swProgramAttribute::Color);
	SetVertexAttribPointer(swProgramAttribute::Color, 4, GL_FLOAT, false, 0, nullptr);

	mVertexBufferObject = 0;
	mIndexBufferObject = 0;
	mVertexBufferPointer = nullptr;
	mIndexBufferPointer = nullptr;

	for (int AttribIndex = 0; AttribIndex < static_cast<int>(swProgramAttribute::Count); AttribIndex++)
		mVertexAttribState[AttribIndex] = swVertexAttributeState();

	glBindTexture(GL_TEXTURE_2D, 0);
	mTexture2D = 0;
	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
	mTextureCubeMap = 0;

	glDisable(GL_POLYGON_OFFSET_FILL);
	mPolygonOffset = swPolygonOffset::None;

	mDepthWrite = true;
	glDepthMask(GL_TRUE);

	glDisable(GL_CULL_FACE);
	mCullFace = false;

	glLineWidth(1.0f);
	mLineWidth = 1.0f;

	glUseProgram(0);
	mMaterialType = swMaterialType::Count;
}

void swContext::ClearColorAndDepth(const swVector4& ClearColor)
{
	glClearColor(ClearColor[0], ClearColor[1], ClearColor[2], ClearColor[3]);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void swContext::ClearDepth()
{
	glClear(GL_DEPTH_BUFFER_BIT);
}

bool swContext::SupportsSelectionOutline()
{
	return gSupportsFramebufferObject &&
		mPrograms[static_cast<int>(swMaterialType::UnlitColor)].Object &&
		mPrograms[static_cast<int>(swMaterialType::OutlineComposite)].Object;
}

int swContext::GetMaxTextureSize()
{
	GLint MaxTextureSize = 0;
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &MaxTextureSize);
	return MaxTextureSize;
}

std::unique_ptr<QOpenGLFramebufferObject> swContext::CreateFramebuffer(int Width, int Height)
{
	if (!gSupportsFramebufferObject || Width <= 0 || Height <= 0)
		return nullptr;

	const int MaxTextureSize = GetMaxTextureSize();
	GLint MaxRenderbufferSize = 0;

	glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &MaxRenderbufferSize);

	if (Width > MaxTextureSize || Height > MaxTextureSize || Width > MaxRenderbufferSize || Height > MaxRenderbufferSize)
		return nullptr;

	swScopedGLContextState GLState;
	QOpenGLFramebufferObjectFormat Format;

	Format.setAttachment(QOpenGLFramebufferObject::Depth);

	std::unique_ptr<QOpenGLFramebufferObject> Framebuffer = std::make_unique<QOpenGLFramebufferObject>(QSize(Width, Height), Format);

	if (!Framebuffer->isValid())
	{
		glBindTexture(GL_TEXTURE_2D, mTexture2D);

		return nullptr;
	}

	glBindTexture(GL_TEXTURE_2D, Framebuffer->texture());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, mTexture2D);

	return Framebuffer;
}

bool swContext::TryShareDepthAttachment(GLuint SourceFramebuffer, QOpenGLFramebufferObject& TargetFramebuffer)
{
	// Share single-sample depth directly; other framebuffers use a depth prepass.
	if (!SourceFramebuffer)
		return false;

	glBindFramebuffer(GL_FRAMEBUFFER, SourceFramebuffer);

	GLint Samples = 0;

	glGetIntegerv(GL_SAMPLES, &Samples);

	if (Samples != 0)
		return false;

	GLint ObjectType = 0;
	GLint DepthBuffer = 0;

	glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &ObjectType);

	if (ObjectType != GL_RENDERBUFFER)
		return false;

	glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &DepthBuffer);

	GLint PreviousRenderbuffer = 0;
	GLint Width = 0;
	GLint Height = 0;

	glGetIntegerv(GL_RENDERBUFFER_BINDING, &PreviousRenderbuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, DepthBuffer);
	glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &Width);
	glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &Height);
	glBindRenderbuffer(GL_RENDERBUFFER, PreviousRenderbuffer);

	if (QSize(Width, Height) != TargetFramebuffer.size())
		return false;

	TargetFramebuffer.bind();
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, DepthBuffer);

	return glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
}

bool swContext::GenerateOutlineMasks(const swScene& Scene, QOpenGLFramebufferObject& FullFramebuffer, QOpenGLFramebufferObject& VisibleFramebuffer)
{
	swScopedGLContextState GLState;
	GLint SceneFramebuffer = 0;
	GLint Viewport[4];

	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &SceneFramebuffer);
	glGetIntegerv(GL_VIEWPORT, Viewport);

	const swMatrix44 WorldMatrix = mWorldMatrix;
	const swMatrix44 ViewMatrix = mViewMatrix;
	const swMatrix44 ProjectionMatrix = mProjectionMatrix;
	const float LineWidth = mLineWidth;

	if (!FullFramebuffer.bind() || glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		return false;

	SetDefaultState();
	SetViewport(0, 0, FullFramebuffer.width(), FullFramebuffer.height());
	ClearColorAndDepth(swVector4(0.0f, 0.0f, 0.0f, 0.0f));
	Scene.DrawSelectionMask(this);

	if (Scene.HasHighlightedMeshes())
	{
		// Keep highlight coverage independent of selection, including when they overlap.
		EnableDepthTest(false);
		glColorMask(GL_FALSE, GL_FALSE, GL_TRUE, GL_FALSE);

		Scene.DrawHighlightMask(this);

		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		EnableDepthTest(true);
	}

	const bool Success = VisibleFramebuffer.bind() && glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;

	if (Success && Scene.HasSelectedMeshes())
	{
		GLint OwnDepthBuffer = 0;

		glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &OwnDepthBuffer);

		const bool SharedDepth = Scene.CanReuseSceneDepth() && TryShareDepthAttachment(SceneFramebuffer, VisibleFramebuffer);

		VisibleFramebuffer.bind();

		if (!SharedDepth)
		{
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, OwnDepthBuffer);

			ClearColorAndDepth(swVector4(0.0f, 0.0f, 0.0f, 0.0f));

			EnableColorWrite(false);
			SetPolygonOffset(swPolygonOffset::Opaque);

			Scene.DrawOutlineOccluders(this);

			SetPolygonOffset(swPolygonOffset::None);
			EnableColorWrite(true);
		}
		else
		{
			glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
			glClear(GL_COLOR_BUFFER_BIT);
		}

		SetDepthWrite(false);

		Scene.DrawSelectionMask(this);

		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, OwnDepthBuffer);
	}
	else if (Success)
		ClearColorAndDepth(swVector4(0.0f, 0.0f, 0.0f, 0.0f));

	glBindFramebuffer(GL_FRAMEBUFFER, SceneFramebuffer);

	SetDefaultState();
	SetWorldMatrix(WorldMatrix);
	SetViewMatrix(ViewMatrix);
	SetProjectionMatrix(ProjectionMatrix);
	SetViewport(Viewport[0], Viewport[1], Viewport[2], Viewport[3]);
	SetLineWidth(LineWidth);

	return Success;
}

void swContext::DrawOutline(QOpenGLFramebufferObject& FullFramebuffer, QOpenGLFramebufferObject& VisibleFramebuffer, float DeviceScale)
{
	const swMatrix44 WorldMatrix = mWorldMatrix;
	const swMatrix44 ViewMatrix = mViewMatrix;
	const swMatrix44 ProjectionMatrix = mProjectionMatrix;
	const float LineWidth = mLineWidth;

	SetDepthWrite(false);
	EnableDepthTest(false);
	EnableColorBlend(true);
	SetWorldMatrix(swMatrix44Identity());
	SetViewMatrix(swMatrix44Identity());
	SetProjectionMatrix(swMatrix44Identity());
	SetMaterial(swMaterialType::OutlineComposite);

	const swPreferences& Preferences = swGetPreferences();

	SetOutlineParams(swVector4(1.0f / FullFramebuffer.width(), 1.0f / FullFramebuffer.height(), static_cast<float>(Preferences.mSelectionOutlineWidth) * DeviceScale, 0.3f),
		swVector4FromColor(Preferences.mObjectSelectedColor), swVector4FromColor(Preferences.mObjectFocusedColor), swVector4FromColor(Preferences.mHighlightNewPiecesColor));

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, VisibleFramebuffer.texture());
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, FullFramebuffer.texture());
	mTexture2D = FullFramebuffer.texture();

	const float Vertices[] =
	{
		-1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
		 1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
		-1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
		 1.0f,  1.0f, 0.0f, 1.0f, 1.0f
	};

	SetVertexBufferPointer(Vertices);
	SetVertexFormat(0, 3, 0, 2, 0, false);

	DrawPrimitives(GL_TRIANGLE_STRIP, 0, 4);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0);

	SetDefaultState();
	SetWorldMatrix(WorldMatrix);
	SetViewMatrix(ViewMatrix);
	SetProjectionMatrix(ProjectionMatrix);
	SetLineWidth(LineWidth);
}

void swContext::ClearResources()
{
	ClearVertexBuffer();
	ClearIndexBuffer();
	ClearTexture2D();
}

void swContext::SetMaterial(swMaterialType MaterialType)
{
	if (MaterialType == mMaterialType)
		return;

	mMaterialType = MaterialType;

	glUseProgram(mPrograms[static_cast<int>(MaterialType)].Object);
	mColorDirty = true;
	mWorldMatrixDirty = true; // todo: change dirty to a bitfield and set the lighting constants dirty here
	mViewMatrixDirty = true;
	mHighlightParamsDirty = true;
	mOutlineParamsDirty = true;
	mTextHaloColorDirty = true;
	mLightingStyleDirty = true;
}

void swContext::SetViewport(int x, int y, int Width, int Height)
{
	glViewport(x, y, Width, Height);
}

void swContext::SetPolygonOffset(swPolygonOffset PolygonOffset)
{
	if (mPolygonOffset == PolygonOffset)
		return;

	switch (PolygonOffset)
	{
	case swPolygonOffset::None:
		glDisable(GL_POLYGON_OFFSET_FILL);
		break;

	case swPolygonOffset::Opaque:
		glPolygonOffset(0.5f, 0.1f);
		glEnable(GL_POLYGON_OFFSET_FILL);
		break;

	case swPolygonOffset::Translucent:
		glPolygonOffset(0.25f, 0.1f);
		glEnable(GL_POLYGON_OFFSET_FILL);
		break;
	}

	mPolygonOffset = PolygonOffset;
}

void swContext::SetDepthWrite(bool Enable)
{
	if (Enable == mDepthWrite)
		return;

	glDepthMask(Enable ? GL_TRUE : GL_FALSE);
	mDepthWrite = Enable;
}

void swContext::SetDepthFunction(swDepthFunction DepthFunction)
{
	if (DepthFunction == mDepthFunction)
		return;

	switch (DepthFunction)
	{
	case swDepthFunction::Always:
		glDepthFunc(GL_ALWAYS);
		break;

	case swDepthFunction::LessEqual:
		glDepthFunc(GL_LEQUAL);
		break;
	}

	mDepthFunction = DepthFunction;
}

void swContext::EnableDepthTest(bool Enable)
{
	if (Enable == mDepthTest)
		return;

	if (Enable)
		glEnable(GL_DEPTH_TEST);
	else
		glDisable(GL_DEPTH_TEST);

	mDepthTest = Enable;
}

void swContext::EnableColorWrite(bool Enable)
{
	if (Enable == mColorWrite)
		return;

	if (Enable)
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	else
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

	mColorWrite = Enable;
}

void swContext::EnableColorBlend(bool Enable)
{
	if (Enable == mColorBlend)
		return;

	if (Enable)
		glEnable(GL_BLEND);
	else
		glDisable(GL_BLEND);

	mColorBlend = Enable;
}

void swContext::EnableCullFace(bool Enable)
{
	if (Enable == mCullFace)
		return;

	if (Enable)
		glEnable(GL_CULL_FACE);
	else
		glDisable(GL_CULL_FACE);

	mCullFace = Enable;
}

void swContext::SetLineWidth(float LineWidth)
{
	if (LineWidth == mLineWidth)
		return;

	glLineWidth(LineWidth);
	mLineWidth = LineWidth;
}

void swContext::BindTexture2D(const swTexture* Texture)
{
	GLuint TextureObject = Texture->mTexture;

	if (mTexture2D == TextureObject)
		return;

	glBindTexture(GL_TEXTURE_2D, TextureObject);
	mTexture2D = TextureObject;
}

void swContext::BindTextureCubeMap(const swTexture* Texture)
{
	GLuint TextureObject = Texture->mTexture;

	if (mTextureCubeMap == TextureObject)
		return;

	glBindTexture(GL_TEXTURE_CUBE_MAP, TextureObject);
	mTextureCubeMap = TextureObject;
}

void swContext::ClearTexture2D()
{
	if (mTexture2D == 0)
		return;

	glBindTexture(GL_TEXTURE_2D, 0);
	mTexture2D = 0;
}

void swContext::ClearTextureCubeMap()
{
	if (mTextureCubeMap == 0)
		return;

	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
	mTextureCubeMap = 0;
}

void swContext::UploadTextureImage(GLenum Target, int Level, GLenum Format, const QImage& Image)
{
	const size_t RowBytes = static_cast<size_t>(Image.width()) * (Image.depth() / 8);
	const size_t AlignedRowBytes = (RowBytes + 3) & ~size_t(3);

	if (static_cast<size_t>(Image.bytesPerLine()) == AlignedRowBytes)
	{
		glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
		glTexImage2D(Target, Level, Format, Image.width(), Image.height(), 0, Format, GL_UNSIGNED_BYTE, Image.constBits());
	}
	else
	{
		std::vector<quint8> PackedPixels(RowBytes * Image.height());
		for (int Row = 0; Row < Image.height(); Row++)
			memcpy(PackedPixels.data() + Row * RowBytes, Image.constScanLine(Row), RowBytes);

		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glTexImage2D(Target, Level, Format, Image.width(), Image.height(), 0, Format, GL_UNSIGNED_BYTE, PackedPixels.data());
	}
}

bool swContext::UploadTexture(swTexture* Texture)
{
	for (int Error = 0; Error < 16 && glGetError() != GL_NO_ERROR; Error++)
	{
	}

	GLint UnpackAlignment;
	glGetIntegerv(GL_UNPACK_ALIGNMENT, &UnpackAlignment);

	if (!Texture->mTexture)
		glGenTextures(1, &Texture->mTexture);

	constexpr int Filters[2][5] =
	{
		{ GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST_MIPMAP_LINEAR, GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR_MIPMAP_LINEAR },
		{ GL_NEAREST, GL_LINEAR, GL_LINEAR, GL_LINEAR, GL_LINEAR  },
	};

	const int Flags = Texture->GetFlags();
	const int FilterFlags = Flags & SW_TEXTURE_FILTER_MASK;
	const int FilterIndex = FilterFlags >> SW_TEXTURE_FILTER_SHIFT;
	const int MipIndex = Flags & SW_TEXTURE_MIPMAPS ? 0 : 1;

	const auto FinishUpload = [this, Flags, UnpackAlignment](bool Success)
	{
		if ((Flags & SW_TEXTURE_CUBE_MAP) == 0)
			ClearTexture2D();
		else
			ClearTextureCubeMap();

		glPixelStorei(GL_UNPACK_ALIGNMENT, UnpackAlignment);

		const bool NoError = glGetError() == GL_NO_ERROR;
		return Success && NoError;
	};

	unsigned int Faces, Target;

	if ((Flags & SW_TEXTURE_CUBE_MAP) == 0)
	{
		Faces = 1;
		Target = GL_TEXTURE_2D;
		BindTexture2D(Texture);
	}
	else
	{
		Faces = 6;
		Target = GL_TEXTURE_CUBE_MAP;
		BindTextureCubeMap(Texture);
	}

	glTexParameteri(Target, GL_TEXTURE_WRAP_S, (Flags & SW_TEXTURE_WRAP_U) ? GL_REPEAT : GL_CLAMP_TO_EDGE);
	glTexParameteri(Target, GL_TEXTURE_WRAP_T, (Flags & SW_TEXTURE_WRAP_V) ? GL_REPEAT : GL_CLAMP_TO_EDGE);
	glTexParameteri(Target, GL_TEXTURE_MIN_FILTER, Filters[MipIndex][FilterIndex]);
	glTexParameteri(Target, GL_TEXTURE_MAG_FILTER, Filters[1][FilterIndex]);

	if (Flags & SW_TEXTURE_CUBE_MAP)
	{
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	}

	if (gSupportsAnisotropic && FilterFlags == SW_TEXTURE_ANISOTROPIC)
		glTexParameterf(Target, GL_TEXTURE_MAX_ANISOTROPY_EXT, swMinimum(4.0f, gMaxAnisotropy));

	int Format;
	switch (Texture->GetImage(0).format())
	{
	default:
		return FinishUpload(false);
	case QImage::Format_Alpha8:
		Format = GL_ALPHA;
		break;
	case QImage::Format_RGB888:
		Format = GL_RGB;
		break;
	case QImage::Format_RGBA8888:
		Format = GL_RGBA;
		break;
	}

	if (Flags & SW_TEXTURE_CUBE_MAP)
		Target = GL_TEXTURE_CUBE_MAP_POSITIVE_X;

	const bool LoadMipmaps = (Flags & SW_TEXTURE_MIPMAPS) || FilterFlags >= SW_TEXTURE_BILINEAR;
	const bool HasMipmaps = Texture->GetImageCount() > Faces;
	int CurrentImage = 0;

	for (size_t FaceIdx = 0; FaceIdx < Faces; FaceIdx++)
	{
		const QImage& Image = Texture->GetImage(CurrentImage);

		UploadTextureImage(Target, 0, Format, Image);

		if (LoadMipmaps)
		{
			int Width = Texture->mWidth;
			int Height = Texture->mHeight;
			int Components = Image.depth() / 8;
			GLubyte* MipBuffer = nullptr;

			if (!HasMipmaps)
			{
				MipBuffer = new GLubyte[Width * Height * Components];

				if (!MipBuffer)
					return FinishUpload(false);

				const size_t RowBytes = static_cast<size_t>(Width) * Components;
				for (int Row = 0; Row < Height; Row++)
					memcpy(MipBuffer + Row * RowBytes, Image.constScanLine(Row), RowBytes);
			}

			for (int Level = 1; ((Width != 1) || (Height != 1)); Level++)
			{
				int PrevRowStride = Width * Components;
				int PrevWidth = Width;
				int PrevHeight = Height;

				Width = swMaximum(1, Width >> 1);
				Height = swMaximum(1, Height >> 1);
				int RowStride = Width * Components;

				if (!HasMipmaps)
				{
					for (int y = 0; y < Height; y++)
					{
						int SrcY0 = y * 2;
						int SrcY1 = swMinimum(SrcY0 + 1, PrevHeight - 1);

						for (int x = 0; x < Width; x++)
						{
							int SrcX0 = x * 2;
							int SrcX1 = swMinimum(SrcX0 + 1, PrevWidth - 1);

							for (int c = 0; c < Components; c++)
							{
								GLubyte c00 = MipBuffer[SrcX0 * Components + SrcY0 * PrevRowStride + c];
								GLubyte c01 = MipBuffer[SrcX0 * Components + SrcY1 * PrevRowStride + c];
								GLubyte c10 = MipBuffer[SrcX1 * Components + SrcY0 * PrevRowStride + c];
								GLubyte c11 = MipBuffer[SrcX1 * Components + SrcY1 * PrevRowStride + c];

								MipBuffer[x * Components + y * RowStride + c] = (c00 + c01 + c10 + c11) / 4;
							}
						}
					}

					glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
					glTexImage2D(Target, Level, Format, Width, Height, 0, Format, GL_UNSIGNED_BYTE, MipBuffer);
				}
				else
				{
					const QImage& MipImage = Texture->GetImage(++CurrentImage);

					UploadTextureImage(Target, Level, Format, MipImage);
				}
			}

			delete[] MipBuffer;
		}

		CurrentImage++;
		Target++;
	}

	return FinishUpload(true);
}

void swContext::SetColor(float Red, float Green, float Blue, float Alpha)
{
	SetColor(swVector4(Red, Green, Blue, Alpha));
}

void swContext::SetColorIndex(int ColorIndex)
{
	SetColor(gColorList[ColorIndex].Value);
}

void swContext::SetColorIndexTinted(int ColorIndex, const swVector4& Tint, float Weight)
{
	const swVector3 Color(gColorList[ColorIndex].Value * Weight + Tint * (1.0f - Weight));
	SetColor(swVector4(Color, gColorList[ColorIndex].Value.w));
}

void swContext::SetColorIndexTinted(int ColorIndex, const swVector4& Tint)
{
	SetColor(gColorList[ColorIndex].Value * Tint);
}

void swContext::SetEdgeColorIndex(int ColorIndex)
{
	SetColor(gColorList[ColorIndex].Edge);
}

void swContext::SetEdgeColorIndexTinted(int ColorIndex, const swVector4& Tint)
{
	SetColor(gColorList[ColorIndex].Edge * Tint);
}

swVertexBuffer swContext::CreateVertexBuffer(int Size, const void* Data)
{
	swVertexBuffer VertexBuffer;

	if (gSupportsVertexBufferObject)
	{
		glGenBuffers(1, &VertexBuffer.Object);
		glBindBuffer(GL_ARRAY_BUFFER_ARB, VertexBuffer.Object);
		glBufferData(GL_ARRAY_BUFFER_ARB, Size, Data, GL_STATIC_DRAW_ARB);

		glBindBuffer(GL_ARRAY_BUFFER_ARB, 0); // context remove
		mVertexBufferObject = 0;
	}
	else
	{
		VertexBuffer.Pointer = malloc(Size);
		if (VertexBuffer.Pointer)
			memcpy(VertexBuffer.Pointer, Data, Size);
	}

	return VertexBuffer;
}

void swContext::DestroyVertexBuffer(swVertexBuffer& VertexBuffer)
{
	if (!VertexBuffer.IsValid())
		return;

	if (gSupportsVertexBufferObject)
	{
		if (mVertexBufferObject == VertexBuffer.Object)
		{
			glBindBuffer(GL_ARRAY_BUFFER_ARB, 0);
			mVertexBufferObject = 0;
		}

		glDeleteBuffers(1, &VertexBuffer.Object);
	}
	else
	{
		free(VertexBuffer.Pointer);
	}

	VertexBuffer.Pointer = nullptr;
}

swIndexBuffer swContext::CreateIndexBuffer(int Size, const void* Data)
{
	swIndexBuffer IndexBuffer;

	if (gSupportsVertexBufferObject)
	{
		glGenBuffers(1, &IndexBuffer.Object);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER_ARB, IndexBuffer.Object);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER_ARB, Size, Data, GL_STATIC_DRAW_ARB);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER_ARB, 0); // context remove
		mIndexBufferObject = 0;
	}
	else
	{
		IndexBuffer.Pointer = malloc(Size);
		if (IndexBuffer.Pointer)
			memcpy(IndexBuffer.Pointer, Data, Size);
	}

	return IndexBuffer;
}

void swContext::DestroyIndexBuffer(swIndexBuffer& IndexBuffer)
{
	if (!IndexBuffer.IsValid())
		return;

	if (gSupportsVertexBufferObject)
	{
		if (mIndexBufferObject == IndexBuffer.Object)
		{
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
			mIndexBufferObject = 0;
		}

		glDeleteBuffers(1, &IndexBuffer.Object);
	}
	else
	{
		free(IndexBuffer.Pointer);
	}

	IndexBuffer.Pointer = nullptr;
}

void swContext::ClearVertexBuffer()
{
	mVertexBufferPointer = nullptr;

	if (mVertexBufferObject)
	{
		glBindBuffer(GL_ARRAY_BUFFER_ARB, 0);
		mVertexBufferObject = 0;
	}

	SetVertexAttribPointer(swProgramAttribute::Position, 3, GL_FLOAT, false, 0, nullptr);

	DisableVertexAttrib(swProgramAttribute::Normal);
	SetVertexAttribPointer(swProgramAttribute::Normal, 4, GL_BYTE, true, 0, nullptr);
	DisableVertexAttrib(swProgramAttribute::TexCoord);
	SetVertexAttribPointer(swProgramAttribute::TexCoord, 2, GL_FLOAT, false, 0, nullptr);
	DisableVertexAttrib(swProgramAttribute::Color);
	SetVertexAttribPointer(swProgramAttribute::Color, 4, GL_FLOAT, false, 0, nullptr);
}

void swContext::SetVertexBuffer(swVertexBuffer VertexBuffer)
{
	if (gSupportsVertexBufferObject)
	{
		const GLuint VertexBufferObject = VertexBuffer.Object;
		mVertexBufferPointer = nullptr;

		if (VertexBufferObject != mVertexBufferObject)
		{
			glBindBuffer(GL_ARRAY_BUFFER_ARB, VertexBufferObject);
			mVertexBufferObject = VertexBufferObject;
		}
	}
	else
	{
		mVertexBufferPointer = (char*)VertexBuffer.Pointer;
	}
}

void swContext::SetVertexBufferPointer(const void* VertexBuffer)
{
	if (gSupportsVertexBufferObject && mVertexBufferObject)
	{
		glBindBuffer(GL_ARRAY_BUFFER_ARB, 0);
		mVertexBufferObject = 0;
	}

	mVertexBufferPointer = (char*)VertexBuffer;
}

void swContext::SetVertexAttribPointer(swProgramAttribute Attrib, GLint Size, GLenum Type, GLboolean Normalized, GLsizei Stride, const void* Pointer)
{
	const int Index = static_cast<int>(Attrib);
	swVertexAttributeState& State = mVertexAttribState[Index];

	if (State.Size != Size || State.Type != Type || State.Normalized != Normalized || State.Stride != Stride || State.Pointer != Pointer || State.VertexBufferObject != mVertexBufferObject)
	{
		glVertexAttribPointer(Index, Size, Type, Normalized, Stride, Pointer);

		State.Size = Size;
		State.Type = Type;
		State.Normalized = Normalized;
		State.Stride = Stride;
		State.Pointer = Pointer;
		State.VertexBufferObject = mVertexBufferObject;
	}
}

void swContext::EnableVertexAttrib(swProgramAttribute Attrib)
{
	const int Index = static_cast<int>(Attrib);
	swVertexAttributeState& State = mVertexAttribState[Index];

	if (!State.Enabled)
	{
		glEnableVertexAttribArray(Index);
		State.Enabled = true;
	}
}

void swContext::DisableVertexAttrib(swProgramAttribute Attrib)
{
	const int Index = static_cast<int>(Attrib);
	swVertexAttributeState& State = mVertexAttribState[Index];

	if (State.Enabled)
	{
		glDisableVertexAttribArray(Index);
		State.Enabled = false;
	}
}

void swContext::SetVertexFormatPosition(int PositionSize)
{
	const int VertexSize = PositionSize * sizeof(float);
	const char* VertexBufferPointer = mVertexBufferPointer;

	SetVertexAttribPointer(swProgramAttribute::Position, PositionSize, GL_FLOAT, false, VertexSize, VertexBufferPointer);
	DisableVertexAttrib(swProgramAttribute::Normal);
	DisableVertexAttrib(swProgramAttribute::TexCoord);
	DisableVertexAttrib(swProgramAttribute::Color);
}

void swContext::SetVertexFormatConditional(int BufferOffset)
{
	constexpr int VertexSize = 12 * sizeof(float);
	const char* VertexBufferPointer = mVertexBufferPointer + BufferOffset;

	SetVertexAttribPointer(swProgramAttribute::ControlPoint1, 3, GL_FLOAT, false, VertexSize, VertexBufferPointer);
	EnableVertexAttrib(swProgramAttribute::ControlPoint1);
	SetVertexAttribPointer(swProgramAttribute::ControlPoint2, 3, GL_FLOAT, false, VertexSize, VertexBufferPointer + 3 * sizeof(float));
	EnableVertexAttrib(swProgramAttribute::ControlPoint2);
	SetVertexAttribPointer(swProgramAttribute::ControlPoint3, 3, GL_FLOAT, false, VertexSize, VertexBufferPointer + 6 * sizeof(float));
	EnableVertexAttrib(swProgramAttribute::ControlPoint3);
	SetVertexAttribPointer(swProgramAttribute::ControlPoint4, 3, GL_FLOAT, false, VertexSize, VertexBufferPointer + 9 * sizeof(float));
	EnableVertexAttrib(swProgramAttribute::ControlPoint4);
}

void swContext::SetVertexFormat(int BufferOffset, int PositionSize, int NormalSize, int TexCoordSize, int ColorSize, bool EnableNormals)
{
	const int VertexSize = (PositionSize + TexCoordSize) * sizeof(float) + NormalSize * sizeof(quint32) + ColorSize;
	const char* VertexBufferPointer = mVertexBufferPointer + BufferOffset;

	int Offset = 0;

	SetVertexAttribPointer(swProgramAttribute::Position, PositionSize, GL_FLOAT, false, VertexSize, VertexBufferPointer);
	EnableVertexAttrib(swProgramAttribute::Position);

	Offset += PositionSize * sizeof(float);

	if (NormalSize && EnableNormals)
	{
		SetVertexAttribPointer(swProgramAttribute::Normal, 4, GL_BYTE, true, VertexSize, VertexBufferPointer + Offset);
		EnableVertexAttrib(swProgramAttribute::Normal);
	}
	else
		DisableVertexAttrib(swProgramAttribute::Normal);

	Offset += NormalSize * sizeof(quint32);

	if (TexCoordSize)
	{
		SetVertexAttribPointer(swProgramAttribute::TexCoord, TexCoordSize, GL_FLOAT, false, VertexSize, VertexBufferPointer + Offset);
		EnableVertexAttrib(swProgramAttribute::TexCoord);
	}
	else
		DisableVertexAttrib(swProgramAttribute::TexCoord);

	Offset += TexCoordSize * sizeof(float);

	if (ColorSize)
	{
		SetVertexAttribPointer(swProgramAttribute::Color, ColorSize, GL_UNSIGNED_BYTE, true, VertexSize, VertexBufferPointer + Offset);
		EnableVertexAttrib(swProgramAttribute::Color);
	}
	else
		DisableVertexAttrib(swProgramAttribute::Color);
}

void swContext::ClearIndexBuffer()
{
	if (mIndexBufferObject)
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
		mIndexBufferObject = 0;
	}
}

void swContext::SetIndexBuffer(swIndexBuffer IndexBuffer)
{
	if (gSupportsVertexBufferObject)
	{
		const GLuint IndexBufferObject = IndexBuffer.Object;
		mIndexBufferPointer = nullptr;

		if (IndexBufferObject != mIndexBufferObject)
		{
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER_ARB, IndexBufferObject);
			mIndexBufferObject = IndexBufferObject;
		}
	}
	else
	{
		mIndexBufferPointer = (char*)IndexBuffer.Pointer;
	}
}

void swContext::SetIndexBufferPointer(const void* IndexBuffer)
{
	if (mIndexBufferObject)
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
		mIndexBufferObject = 0;
	}

	mIndexBufferPointer = (char*)IndexBuffer;
}

void swContext::BindMesh(const swMesh* Mesh)
{
	if (Mesh->mVertexCacheOffset != -1)
	{
		const auto [VertexBuffer, IndexBuffer] = swGetPiecesLibrary()->GetBuffers();
		const GLuint VertexBufferObject = VertexBuffer.Object;
		const GLuint IndexBufferObject = IndexBuffer.Object;

		if (VertexBufferObject != mVertexBufferObject)
		{
			glBindBuffer(GL_ARRAY_BUFFER_ARB, VertexBufferObject);
			mVertexBufferObject = VertexBufferObject;
			mVertexBufferPointer = nullptr;
		}

		if (IndexBufferObject != mIndexBufferObject)
		{
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER_ARB, IndexBufferObject);
			mIndexBufferObject = IndexBufferObject;
			mIndexBufferPointer = nullptr;
		}
	}
	else
	{
		if (mVertexBufferObject)
		{
			glBindBuffer(GL_ARRAY_BUFFER_ARB, 0);
			mVertexBufferObject = 0;
		}

		if (mIndexBufferObject)
		{
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
			mIndexBufferObject = 0;
		}

		mVertexBufferPointer = (char*)Mesh->mVertexData;
		mIndexBufferPointer = (char*)Mesh->mIndexData;
	}
}

void swContext::FlushState()
{
	const swProgram& Program = mPrograms[static_cast<int>(mMaterialType)];

	if (mWorldMatrixDirty || mViewMatrixDirty || mProjectionMatrixDirty)
	{
		if (mViewProjectionMatrixDirty)
		{
			mViewProjectionMatrix = swMultiply(mViewMatrix, mProjectionMatrix);
			mViewProjectionMatrixDirty = false;
		}

		if (mWorldMatrixDirty)
		{
			if (Program.WorldMatrixLocation != -1)
				glUniformMatrix4fv(Program.WorldMatrixLocation, 1, false, mWorldMatrix.GetFloats());
		}

		if (mViewMatrixDirty)
		{
			const swMatrix44 InverseViewMatrix = swMatrix44AffineInverse(mViewMatrix);
			swVector3 ViewPosition = swMultiply30(-mViewMatrix.GetTranslation(), InverseViewMatrix);

			if (Program.LightPositionLocation != -1)
			{
				swVector3 LightPosition = ViewPosition + swMultiply30(swVector3(300.0f, 300.0f, 0.0f), InverseViewMatrix);
				glUniform3fv(Program.LightPositionLocation, 1, LightPosition.GetFloats());
			}

			if (Program.EyePositionLocation != -1)
				glUniform3fv(Program.EyePositionLocation, 1, ViewPosition.GetFloats());
		}

		glUniformMatrix4fv(Program.WorldViewProjectionMatrixLocation, 1, false, swMultiply(mWorldMatrix, mViewProjectionMatrix).GetFloats());
		mWorldMatrixDirty = false;
		mViewMatrixDirty = false;
		mProjectionMatrixDirty = false;
	}

	if (mColorDirty && Program.MaterialColorLocation != -1)
	{
		glUniform4fv(Program.MaterialColorLocation, 1, mColor.GetFloats());
		mColorDirty = false;
	}

	if (mLightingStyleDirty && Program.LightingStyleLocation != -1)
	{
		glUniform1i(Program.LightingStyleLocation, static_cast<GLint>(mLightingStyle));
		mLightingStyleDirty = false;
	}

	if (mTextHaloColorDirty && Program.TextHaloColorLocation != -1)
	{
		glUniform4fv(Program.TextHaloColorLocation, 1, mTextHaloColor.GetFloats());
		mTextHaloColorDirty = false;
	}

	if (mHighlightParamsDirty && Program.HighlightParamsLocation != -1)
	{
		glUniform4fv(Program.HighlightParamsLocation, 4, mHighlightParams[0].GetFloats());
		mHighlightParamsDirty = false;
	}

	if (mOutlineParamsDirty && Program.OutlineParamsLocation != -1)
	{
		glUniform4fv(Program.OutlineParamsLocation, 4, mOutlineParams[0].GetFloats());
		mOutlineParamsDirty = false;
	}
}

void swContext::DrawPrimitives(GLenum Mode, GLint First, GLsizei Count)
{
	FlushState();
	glDrawArrays(Mode, First, Count);
}

void swContext::DrawIndexedPrimitives(GLenum Mode, GLsizei Count, GLenum Type, int Offset)
{
	FlushState();
	glDrawElements(Mode, Count, Type, mIndexBufferPointer + Offset);
}
