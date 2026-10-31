#include "sw_global.h"
#include "sw_context.h"

// viewing directions around the model and the id image size of each
constexpr int SW_VISIBILITY_DIRECTIONS = 96;
constexpr int SW_VISIBILITY_SIZE = 2048;

std::vector<bool> swContext::FindVisibleTriangles(const std::vector<swVector3>& Positions, const std::vector<bool>& Translucent)
{
	const size_t TriangleCount = Positions.size() / 3;
	std::vector<bool> Visible(TriangleCount, false);

	if (!TriangleCount)
		return Visible;

	if (!SupportsMaterial(swMaterialType::UnlitVertexColor))
		return std::vector<bool>();

	// translucent triangles stay and never hide what is behind them
	struct swVisibilityVertex
	{
		swVector3 Position;
		quint32 ID;
	};

	std::vector<swVisibilityVertex> Vertices;
	std::vector<quint32> LineIndices;
	swVector3 Min(FLT_MAX, FLT_MAX, FLT_MAX), Max(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	for (size_t TriangleIndex = 0; TriangleIndex < TriangleCount; TriangleIndex++)
	{
		if (Translucent[TriangleIndex])
		{
			Visible[TriangleIndex] = true;
			continue;
		}

		const quint32 First = static_cast<quint32>(Vertices.size());
		const quint32 ID = qToLittleEndian(static_cast<quint32>(TriangleIndex + 1));

		for (int Corner = 0; Corner < 3; Corner++)
		{
			const swVector3& Position = Positions[TriangleIndex * 3 + Corner];

			Vertices.push_back({ Position, ID });
			Min = swMinimum(Min, Position);
			Max = swMaximum(Max, Position);
		}

		LineIndices.insert(LineIndices.end(), { First, First + 1, First + 1, First + 2, First + 2, First });
	}

	if (Vertices.empty())
		return Visible;

	swScopedGLContextState GLState;
	MakeCurrent();

	const int Size = qMin(GetMaxTextureSize(), SW_VISIBILITY_SIZE);
	QOpenGLFramebufferObject Framebuffer(QSize(Size, Size), QOpenGLFramebufferObject::Depth);

	if (!Framebuffer.bind() || glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		return std::vector<bool>();

	swVertexBuffer VertexBuffer = CreateVertexBuffer(static_cast<int>(Vertices.size() * sizeof(swVisibilityVertex)), Vertices.data());
	swIndexBuffer IndexBuffer = CreateIndexBuffer(static_cast<int>(LineIndices.size() * sizeof(quint32)), LineIndices.data());

	SetDefaultState();
	SetViewport(0, 0, Size, Size);
	SetMaterial(swMaterialType::UnlitVertexColor);
	SetWorldMatrix(swMatrix44Identity());
	EnableColorBlend(false);
	glDisable(GL_DITHER);
#ifndef SW_OPENGLES
	glDisable(GL_LINE_SMOOTH);
#endif

	SetVertexBuffer(VertexBuffer);
	SetIndexBuffer(IndexBuffer);
	SetVertexFormat(0, 3, 0, 0, 4, false);

	const swVector3 Center = (Min + Max) * 0.5f;
	const float Radius = qMax(swLength(Max - Min) * 0.5f, 1.0f);
	const swMatrix44 ProjectionMatrix = swMatrix44Orthographic(-Radius, Radius, -Radius, Radius, Radius, Radius * 3.0f);
	std::vector<quint32> Pixels(static_cast<size_t>(Size) * Size);

	const auto MarkVisible = [this, &Pixels, &Visible, Size]()
	{
		glReadPixels(0, 0, Size, Size, GL_RGBA, GL_UNSIGNED_BYTE, Pixels.data());

		for (const quint32 Pixel : Pixels)
		{
			const quint32 ID = qFromLittleEndian(Pixel);

			if (ID && ID <= Visible.size())
				Visible[ID - 1] = true;
		}
	};

	SetProjectionMatrix(ProjectionMatrix);

	for (int DirectionIndex = 0; DirectionIndex < SW_VISIBILITY_DIRECTIONS; DirectionIndex++)
	{
		// directions spread evenly over a sphere
		const float Step = DirectionIndex + 0.5f;
		const float Polar = acosf(1.0f - 2.0f * Step / SW_VISIBILITY_DIRECTIONS);
		const float Azimuth = static_cast<float>(M_PI) * (1.0f + sqrtf(5.0f)) * Step;
		const swVector3 Direction(cosf(Azimuth) * sinf(Polar), sinf(Azimuth) * sinf(Polar), cosf(Polar));
		const swVector3 Up = fabsf(Direction.z) < 0.9f ? swVector3(0.0f, 0.0f, 1.0f) : swVector3(0.0f, 1.0f, 0.0f);

		SetViewMatrix(swMatrix44LookAt(Center + Direction * (Radius * 2.0f), Center, Up));

		// filled faces sit slightly back so their own edges pass the depth test
		SetDepthWrite(true);
		ClearColorAndDepth(swVector4(0.0f, 0.0f, 0.0f, 0.0f));
		glEnable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(1.0f, 1.0f);
		DrawPrimitives(GL_TRIANGLES, 0, static_cast<GLsizei>(Vertices.size()));
		glDisable(GL_POLYGON_OFFSET_FILL);
		MarkVisible();

		// edges catch triangles thinner than a pixel
		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		SetDepthWrite(false);
		DrawIndexedPrimitives(GL_LINES, static_cast<GLsizei>(LineIndices.size()), GL_UNSIGNED_INT, 0);
		MarkVisible();
	}

	ClearIndexBuffer();
	ClearVertexBuffer();
	DestroyIndexBuffer(IndexBuffer);
	DestroyVertexBuffer(VertexBuffer);
	glEnable(GL_DITHER);
	SetDefaultState();

	Framebuffer.release();

	return Visible;
}
