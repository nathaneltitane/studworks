#pragma once

#include "sw_math.h"
#include "sw_context.h"
#include <bitset>

enum class swViewCubeLocation
{
	TopLeft,
	TopRight,
	BottomLeft,
	BottomRight
};

class swViewCube
{
public:
	swViewCube(swView* View);

	void Draw();
	bool OnMouseMove();
	bool OnLeftButtonUp();
	bool OnLeftButtonDown();
	bool IsDragging() const;

	static void CreateResources(swContext* Context);
	static void DestroyResources(swContext* Context);

protected:
	void UpdateSettings();
	swMatrix44 GetViewMatrix() const;
	swMatrix44 GetProjectionMatrix() const;
	std::bitset<6> GetIntersectionFlags() const;
	void DrawFacets(int FirstFacet, int FacetCount) const;

	swView* const mView = nullptr;

	int mSize = 1;
	bool mEnabled = false;
	swViewCubeLocation mLocation = swViewCubeLocation::TopRight;

	int mMouseDownX = 0;
	int mMouseDownY = 0;
	bool mMouseDown = false;

	std::bitset<6> mIntersectionFlags;

	// Faces, edges and corners.
	struct swViewCubeFacet
	{
		std::bitset<6> Flags;
		int IndexStart;
		int IndexCount;
	};

	static constexpr int mFaceCount = 6;
	static constexpr int mEdgeCount = 12;
	static constexpr int mCornerCount = 8;
	static constexpr int mFacetCount = mFaceCount + mEdgeCount + mCornerCount;

	static swTexture* mTexture;
	static swVertexBuffer mVertexBuffer;
	static swIndexBuffer mIndexBuffer;
	static int mIndexCount;
	static swViewCubeFacet mFacets[mFacetCount];
	static const float mHalfSize;
	static const float mChamfer;
};
