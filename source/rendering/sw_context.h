#pragma once

#include "sw_math.h"
#include "sw_mesh.h"
#include "sw_result.h"

enum class swLightingStyle;

class swVertexBuffer
{
public:
	constexpr swVertexBuffer()
		: Pointer(nullptr)
	{
	}

	bool IsValid() const
	{
		return Pointer != nullptr;
	}

	union
	{
		GLuint Object;
		void* Pointer;
	};
};

class swIndexBuffer
{
public:
	constexpr swIndexBuffer()
		: Pointer(nullptr)
	{
	}

	bool IsValid() const
	{
		return Pointer != nullptr;
	}

	union
	{
		GLuint Object;
		void* Pointer;
	};
};

enum class swMaterialType
{
	UnlitColor,
	UnlitColorConditional,
	UnlitTextureModulate,
	TextSDF,
	UnlitTextureDecal,
	UnlitVertexColor,
	UnlitViewCube,
	FakeLitColor,
	FakeLitTextureDecal,
	OutlineComposite,
	Count
};

enum class swProgramAttribute
{
	Position,
	Normal,
	TexCoord,
	Color,
	Count,
	ControlPoint1 = Position,
	ControlPoint2 = Normal,
	ControlPoint3 = TexCoord,
	ControlPoint4 = Color
};

enum class swCutawayShape
{
	None,
	Cube,
	Sphere,
	Cylinder
};

struct swProgram
{
	GLuint Object;
	GLint WorldViewProjectionMatrixLocation;
	GLint WorldMatrixLocation;
	GLint MaterialColorLocation;
	GLint LightPositionLocation;
	GLint EyePositionLocation;
	GLint HighlightParamsLocation;
	GLint OutlineParamsLocation;
	GLint TextHaloColorLocation;
	GLint LightingStyleLocation;
	GLint CutawayShapeLocation;
	GLint CutawayMatrixLocation;
	GLint WorldCutawayMatrixLocation;
	GLint CutawayEyeLocation;
};

enum class swPolygonOffset
{
	None,
	Opaque,
	Translucent
};

enum class swDepthFunction
{
	LessEqual,
	Always
};

struct swVertexAttributeState
{
	GLint Size = 0;
	GLenum Type = 0;
	GLboolean Normalized = 0;
	bool Enabled = 0;
	GLsizei Stride = 0;
	const void* Pointer = nullptr;
	GLuint VertexBufferObject = 0;
};

// Restores caller state after temporary rendering, including a caller with no current context.
class swScopedGLContextState
{
public:
	swScopedGLContextState();
	~swScopedGLContextState();

	swScopedGLContextState(const swScopedGLContextState&) = delete;
	swScopedGLContextState& operator=(const swScopedGLContextState&) = delete;

private:
	QOpenGLContext* mContext;
	QSurface* mSurface;
	GLint mDrawFramebuffer = 0;
	GLint mReadFramebuffer = 0;
	bool mSeparateFramebuffers = false;
};

class swContext : public QOpenGLFunctions
{
public:
	swContext();
	~swContext();

	swContext(const swContext&) = delete;
	swContext(swContext&&) = delete;
	swContext& operator=(const swContext&) = delete;
	swContext& operator=(swContext&&) = delete;

	static swResult<void> InitializeRenderer(bool CreateRenderResources);
	static void ShutdownRenderer();
	static swContext* GetGlobalOffscreenContext();

	swResult<void> CreateResources();
	void DestroyResources();

	void SetDefaultState();
	void ClearResources();

	void MakeCurrent();
	QOpenGLContext* GetGLContext() const { return mContext; }

	void SetGLContext(QOpenGLContext* GLContext, QOpenGLWidget* Widget);
	void SetOffscreenContext();

	bool IsOffscreen() const
	{
		return mWidget == nullptr;
	}

	void ClearColorAndDepth(const swVector4& ClearColor);
	void ClearDepth();

	static bool SupportsMaterial(swMaterialType MaterialType)
	{
		return mPrograms[static_cast<int>(MaterialType)].Object != 0;
	}

	static bool SupportsDefaultLights()
	{
		return SupportsMaterial(swMaterialType::FakeLitColor) && SupportsMaterial(swMaterialType::FakeLitTextureDecal);
	}

	static bool SupportsSelectionOutline();
	int GetMaxTextureSize();
	std::unique_ptr<QOpenGLFramebufferObject> CreateFramebuffer(int Width, int Height);
	bool GenerateOutlineMasks(const swScene& Scene, QOpenGLFramebufferObject& FullFramebuffer, QOpenGLFramebufferObject& VisibleFramebuffer);
	void DrawOutline(QOpenGLFramebufferObject& FullFramebuffer, QOpenGLFramebufferObject& VisibleFramebuffer, float DeviceScale, const swVector4& HighlightColor);

	void SetWorldMatrix(const swMatrix44& WorldMatrix)
	{
		mWorldMatrix = WorldMatrix;
		mWorldMatrixDirty = true;
	}

	void SetViewMatrix(const swMatrix44& ViewMatrix)
	{
		mViewMatrix = ViewMatrix;
		mViewMatrixDirty = true;
		mViewProjectionMatrixDirty = true;
	}

	void SetProjectionMatrix(const swMatrix44& ProjectionMatrix)
	{
		mProjectionMatrix = ProjectionMatrix;
		mProjectionMatrixDirty = true;
		mViewProjectionMatrixDirty = true;
	}

	const swMatrix44& GetProjectionMatrix()
	{
		return mProjectionMatrix;
	}

	void SetMaterial(swMaterialType MaterialType);
	void SetViewport(int x, int y, int Width, int Height);
	void SetPolygonOffset(swPolygonOffset PolygonOffset);
	void SetDepthWrite(bool Enable);
	void SetDepthFunction(swDepthFunction DepthFunction);
	void EnableDepthTest(bool Enable);
	void EnableColorWrite(bool Enable);
	void EnableColorBlend(bool Enable);
	void EnableCullFace(bool Enable);
	void SetLineWidth(float LineWidth);

	void BindTexture2D(const swTexture* Texture);
	void BindTextureCubeMap(const swTexture* Texture);
	void ClearTexture2D();
	void ClearTextureCubeMap();
	bool UploadTexture(swTexture* Texture);

	void SetColor(const swVector4& Color)
	{
		mColor = Color;
		mColorDirty = true;
	}

	void SetLightingStyle(swLightingStyle LightingStyle)
	{
		if (LightingStyle == mLightingStyle)
			return;

		mLightingStyle = LightingStyle;
		mLightingStyleDirty = true;
	}

	// use to hide the geometry inside a cube or sphere given by its world to unit matrix
	void SetCutaway(swCutawayShape CutawayShape, const swMatrix44& CutawayMatrix)
	{
		mCutawayShape = CutawayShape;
		mCutawayMatrix = CutawayMatrix;
		mCutawayDirty = true;
	}

	void SetTextHaloColor(const swVector4& Color)
	{
		mTextHaloColor = Color;
		mTextHaloColorDirty = true;
	}

	void SetHighlightParams(const swVector4& HighlightPosition, const swVector4& TextColor, const swVector4& BackgroundColor, const swVector4& HighlightColor)
	{
		mHighlightParams[0] = HighlightPosition;
		mHighlightParams[1] = TextColor;
		mHighlightParams[2] = BackgroundColor;
		mHighlightParams[3] = HighlightColor;
		mHighlightParamsDirty = true;
	}

	void SetOutlineParams(const swVector4& PixelSizeRadiusOpacity, const swVector4& SelectedColor, const swVector4& FocusedColor, const swVector4& HighlightedColor)
	{
		mOutlineParams[0] = PixelSizeRadiusOpacity;
		mOutlineParams[1] = SelectedColor;
		mOutlineParams[2] = FocusedColor;
		mOutlineParams[3] = HighlightedColor;
		mOutlineParamsDirty = true;
	}

	void SetColor(float Red, float Green, float Blue, float Alpha);
	void SetColorIndex(int ColorIndex);
	void SetColorIndexTinted(int ColorIndex, const swVector4& Tint, float Weight);
	void SetColorIndexTinted(int ColorIndex, const swVector4& Tint);
	void SetEdgeColorIndex(int ColorIndex);
	void SetEdgeColorIndexTinted(int ColorIndex, const swVector4& Tint);

	swVertexBuffer CreateVertexBuffer(int Size, const void* Data);
	void DestroyVertexBuffer(swVertexBuffer& VertexBuffer);
	swIndexBuffer CreateIndexBuffer(int Size, const void* Data);
	void DestroyIndexBuffer(swIndexBuffer& IndexBuffer);

	void ClearVertexBuffer();
	void SetVertexBuffer(swVertexBuffer VertexBuffer);
	void SetVertexBufferPointer(const void* VertexBuffer);

	void ClearIndexBuffer();
	void SetIndexBuffer(swIndexBuffer IndexBuffer);
	void SetIndexBufferPointer(const void* IndexBuffer);

	void SetVertexFormat(int BufferOffset, int PositionSize, int NormalSize, int TexCoordSize, int ColorSize, bool EnableNormals);
	void SetVertexFormatPosition(int PositionSize);
	void SetVertexFormatConditional(int BufferOffset);
	void DrawPrimitives(GLenum Mode, GLint First, GLsizei Count);
	void DrawIndexedPrimitives(GLenum Mode, GLsizei Count, GLenum Type, int Offset);

	void BindMesh(const swMesh* Mesh);

protected:
	static swResult<void> CreateOffscreenContext();
	static void DestroyOffscreenContext();

	swResult<void> CreateShaderPrograms();
	void UploadTextureImage(GLenum Target, int Level, GLenum Format, const QImage& Image);
	void FlushState();
	bool TryShareDepthAttachment(GLuint SourceFramebuffer, QOpenGLFramebufferObject& TargetFramebuffer);

	void SetVertexAttribPointer(swProgramAttribute Attrib, GLint Size, GLenum Type, GLboolean Normalized, GLsizei Stride, const void* Pointer);
	void EnableVertexAttrib(swProgramAttribute Attrib);
	void DisableVertexAttrib(swProgramAttribute Attrib);

	QOpenGLWidget* mWidget = nullptr;
	QOpenGLContext* mContext = nullptr;

	GLuint mVertexBufferObject;
	GLuint mIndexBufferObject;
	const char* mVertexBufferPointer;
	const char* mIndexBufferPointer;

	swMaterialType mMaterialType;

	swVertexAttributeState mVertexAttribState[static_cast<int>(swProgramAttribute::Count)];

	GLuint mTexture2D;
	GLuint mTextureCubeMap;
	swPolygonOffset mPolygonOffset;
	bool mDepthWrite;
	swDepthFunction mDepthFunction;
	bool mDepthTest;
	bool mColorWrite;
	bool mColorBlend;
	bool mCullFace;
	float mLineWidth;

	swVector4 mColor;
	swVector4 mTextHaloColor;
	swMatrix44 mWorldMatrix;
	swMatrix44 mViewMatrix;
	swMatrix44 mProjectionMatrix;
	swMatrix44 mViewProjectionMatrix;
	swVector4 mHighlightParams[4];
	swVector4 mOutlineParams[4];
	swLightingStyle mLightingStyle;
	swCutawayShape mCutawayShape;
	swMatrix44 mCutawayMatrix;
	bool mColorDirty;
	bool mTextHaloColorDirty;
	bool mWorldMatrixDirty;
	bool mViewMatrixDirty;
	bool mProjectionMatrixDirty;
	bool mViewProjectionMatrixDirty;
	bool mHighlightParamsDirty;
	bool mOutlineParamsDirty;
	bool mLightingStyleDirty;
	bool mCutawayDirty;

	static std::unique_ptr<QOpenGLContext> mOffscreenContext;
	static std::unique_ptr<QOffscreenSurface> mOffscreenSurface;
	static std::unique_ptr<swContext> mGlobalOffscreenContext;

	static bool mRenderResourcesCreated;
	static swProgram mPrograms[static_cast<int>(swMaterialType::Count)];

	Q_DECLARE_TR_FUNCTIONS(swContext);
};
