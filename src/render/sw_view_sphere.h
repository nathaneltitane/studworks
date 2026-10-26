#pragma once

#include "sw_math.h"
#include "sw_context.h"
#include <bitset>

enum class swViewSphereLocation
{
	TopLeft,
	TopRight,
	BottomLeft,
	BottomRight
};

class swViewSphere
{
public:
	swViewSphere(swView* View);

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
	std::bitset<6> GetIntersectionFlags(swVector3& Intersection) const;

	swView* const mView = nullptr;

	int mSize = 1;
	bool mEnabled = false;
	swViewSphereLocation mLocation = swViewSphereLocation::TopRight;

	int mMouseDownX = 0;
	int mMouseDownY = 0;
	bool mMouseDown = false;

	swVector3 mIntersection;
	std::bitset<6> mIntersectionFlags;

	static swTexture* mTexture;
	static swVertexBuffer mVertexBuffer;
	static swIndexBuffer mIndexBuffer;
	static int mSphereIndexCount;
	static const float mRadius;
	static const float mHighlightRadius;
	static const int mSubdivisions;
};
