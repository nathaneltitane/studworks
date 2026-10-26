#pragma once

#include "sw_mesh.h"

enum class swHighlightRenderMode;

enum class swRenderMeshState : int
{
	Default,
	Selected,
	Focused,
	Preview,
	Faded,
	Highlighted
};

struct swRenderMesh
{
	swMatrix44 WorldMatrix;
	swMesh* Mesh;
	int ColorIndex;
	int LodIndex;
	swRenderMeshState State;
};

struct swTranslucentMeshInstance
{
	const swMeshSection* Section;
	int RenderMeshIndex;
	float Distance;
};

class swScene
{
public:
	swScene();

	void SetActiveSubmodelInstance(swPiece* ActiveSubmodelInstance, const swMatrix44& ActiveSubmodelTransform, const std::vector<swModel*>& ModelEditingPath)
	{
		mActiveSubmodelInstance = ActiveSubmodelInstance;
		mActiveSubmodelTransform = ActiveSubmodelTransform;
		mModelEditingPath = ModelEditingPath;
	}

	bool IsModelEditing(const swModel* Model) const
	{
		return mDrawInterface && mActiveSubmodelInstance && std::find(mModelEditingPath.begin(), mModelEditingPath.end(), Model) != mModelEditingPath.end();
	}

	swPiece* GetActiveSubmodelInstance() const
	{
		return mActiveSubmodelInstance;
	}

	const swMatrix44& GetViewMatrix() const
	{
		return mViewMatrix;
	}

	void SetDrawInterface(bool DrawInterface)
	{
		mDrawInterface = DrawInterface;
	}

	bool GetDrawInterface() const
	{
		return mDrawInterface;
	}

	void SetRequireCompleteAssets(bool RequireCompleteAssets)
	{
		mRequireCompleteAssets = RequireCompleteAssets;
	}

	bool GetRequireCompleteAssets() const
	{
		return mRequireCompleteAssets;
	}

	void SetRequireGeneratedMeshes(bool RequireGeneratedMeshes)
	{
		mRequireGeneratedMeshes = RequireGeneratedMeshes;
	}

	bool GetRequireGeneratedMeshes() const
	{
		return mRequireGeneratedMeshes;
	}

	void MarkMissingAssets()
	{
		mMissingAssets = true;
	}

	bool HasMissingAssets() const
	{
		return mMissingAssets;
	}

	void SetDrawInsertPreview(bool DrawInsertPreview)
	{
		mDrawInsertPreview = DrawInsertPreview;
	}

	bool GetDrawInsertPreview() const
	{
		return mDrawInsertPreview;
	}

	void SetShadingMode(swShadingMode ShadingMode)
	{
		mShadingMode = ShadingMode;
	}

	void SetLineSettings(bool DrawEdgeLines, bool DrawConditionalLines, float LineWidth)
	{
		mDrawEdgeLines = DrawEdgeLines;
		mDrawConditionalLines = DrawConditionalLines;
		mLineWidth = LineWidth;
	}

	void SetAllowLOD(bool AllowLOD)
	{
		mAllowLOD = AllowLOD;
	}

	void SetLODDistance(float Distance)
	{
		mMeshLODDistance = Distance;
	}

	swMatrix44 ApplyActiveSubmodelTransform(const swMatrix44& WorldMatrix) const
	{
		return !mActiveSubmodelInstance ? WorldMatrix : swMultiply(WorldMatrix, mActiveSubmodelTransform);
	}

	void Begin(const swMatrix44& ViewMatrix);
	void End();
	void AddMesh(swMesh* Mesh, const swMatrix44& WorldMatrix, int ColorIndex, swRenderMeshState State);

	void AddInterfaceObject(const swObject* Object)
	{
		mInterfaceObjects.emplace_back(Object);
	}

	void Draw(swContext* Context, const std::function<void()>& PreTranslucentCallback) const;
	bool HasSelectedMeshes() const;
	bool HasHighlightedMeshes() const;
	bool CanReuseSceneDepth() const;
	void DrawSelectionMask(swContext* Context) const;
	void DrawHighlightMask(swContext* Context) const;
	void DrawOutlineOccluders(swContext* Context) const;
	void SetHighlightRenderMode(swHighlightRenderMode HighlightRenderMode)
	{
		mHighlightRenderMode = HighlightRenderMode;
	}

	swHighlightRenderMode GetHighlightRenderMode() const
	{
		return mHighlightRenderMode;
	}

	void DrawInterfaceObjects(swContext* Context) const;

protected:
	enum class swOutlineGeometry
	{
		Selection,
		Highlight,
		Occluders
	};

	void DrawOutlineGeometry(swContext* Context, swOutlineGeometry Geometry) const;
	void DrawOpaqueMeshes(swContext* Context, bool DrawLit, int PrimitiveTypes, bool DrawFaded, bool DrawNonFaded) const;
	void DrawTranslucentMeshes(swContext* Context, bool DrawLit, bool DrawFadePrepass, bool DrawFaded, bool DrawNonFaded) const;
	void DrawDebugNormals(swContext* Context, const swMesh* Mesh) const;

	swMatrix44 mViewMatrix;
	swMatrix44 mActiveSubmodelTransform;
	swPiece* mActiveSubmodelInstance = nullptr;
	std::vector<swModel*> mModelEditingPath;
	swShadingMode mShadingMode;
	bool mDrawInterface = false;
	swHighlightRenderMode mHighlightRenderMode;
	bool mRequireCompleteAssets = false;
	bool mRequireGeneratedMeshes = false;
	bool mMissingAssets = false;
	bool mDrawInsertPreview = false;
	bool mAllowLOD = true;
	bool mDrawEdgeLines = true;
	bool mDrawConditionalLines = true;
	float mLineWidth = 1.0f;
	float mMeshLODDistance;

	swVector4 mFadeColor;
	swVector4 mHighlightColor;
	bool mHasFadedParts = false;
	bool mTranslucentFade = false;

	std::vector<swRenderMesh> mRenderMeshes;
	std::vector<int> mOpaqueMeshes;
	std::vector<swTranslucentMeshInstance> mTranslucentMeshes;
	std::vector<const swObject*> mInterfaceObjects;
};
