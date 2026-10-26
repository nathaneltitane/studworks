#pragma once

#include "sw_context.h"

#include <optional>

class swViewManipulator
{
public:
	swViewManipulator(swView* View);

	void DrawSelectMove(swTrackButton TrackButton, swTrackTool TrackTool, quint32 TrackToolSection);
	void DrawRotate(swTrackButton TrackButton, swTrackTool TrackTool);

	std::pair<swTrackTool, quint32> UpdateSelectMove(swTrackButton TrackButton);
	swTrackTool UpdateRotate();

	static void CreateResources(swContext* Context);
	static void DestroyResources(swContext* Context);

protected:
	struct swRotationDiscInfo
	{
		int AxisIndex;
		float Radius;
		swVector4 Rotation;
		swVector4 FillColor;
		swVector4 HighlightColor;
		bool CameraFacing;
	};

	struct swMoveAxisInfo
	{
		swVector4 Color;
	};

	void DrawTrainTrack(swPiece* Piece, swContext* Context, swTrackTool TrackTool, quint32 TrackToolSection);
	std::tuple<swTrackTool, quint32, float> UpdateSelectMoveTrainTrack(swPiece* Piece, float OverlayScale, const swVector3& Start, const swVector3& End) const;

	static bool IsTrackToolAllowed(swTrackTool TrackTool, quint32 AllowedTransforms);
	bool GetRotationWorldMatrix(swMatrix44& WorldMatrix) const;
	static swTrackTool GetRotationAxis(const swVector3& LocalIntersection, float Epsilon);
	static std::optional<swRotationDiscInfo> GetRotationDiscInfo(swTrackTool TrackTool);
	static std::optional<swMoveAxisInfo> GetMoveAxisInfo(swTrackTool TrackTool);
	swMatrix44 GetRotationDiscWorldMatrix(const swRotationDiscInfo& DiscInfo, const swMatrix44& WorldMatrix) const;
	float GetRotationDiscStartAngle(const swRotationDiscInfo& DiscInfo, const swMatrix44& DiscWorldMatrix) const;
	void DrawTrackballHover(const swMatrix44& WorldMatrix, float OverlayScale) const;
	bool DrawRotationDisc(swTrackButton TrackButton, const swVector3& MouseToolDistance, const std::optional<swRotationDiscInfo>& RotationDisc, const swMatrix44& WorldMatrix, float OverlayScale) const;
	void DrawCameraRotationRing(swTrackButton TrackButton, swTrackTool TrackTool, const swMatrix44& WorldMatrix, float OverlayScale) const;
	void DrawAxisRotationRings(swTrackButton TrackButton, swTrackTool TrackTool, bool HasAngle, const swMatrix44& WorldMatrix, float OverlayScale) const;
	void DrawRotationText(swTrackButton TrackButton, const swVector3& MouseToolDistance, const std::optional<swRotationDiscInfo>& RotationDisc, const swMatrix44& WorldMatrix, float OverlayScale) const;
	void DrawMoveDistance(swTrackButton TrackButton, swTrackTool TrackTool, const swVector3& StartPosition, const swVector3& EndPosition, float Distance) const;

	swView* mView = nullptr;

	static swVertexBuffer mRotateMoveVertexBuffer;
	static swIndexBuffer mRotateMoveIndexBuffer;

	static swVector4 GetAxisColor(int AxisIndex, bool Selected);
	static constexpr swVector4 mColorCamera = swVector4(190.0f / 255.0f, 190.0f / 255.0f, 190.0f / 255.0f, 255.0f / 255.0f);
	static constexpr swVector4 mColorTrackball = swVector4(120.0f / 255.0f, 120.0f / 255.0f, 120.0f / 255.0f, 30.0f / 255.0f);
	static constexpr swVector4 mColorCameraSelected = swVector4(230.0f / 255.0f, 230.0f / 255.0f, 230.0f / 255.0f, 255.0f / 255.0f);

	static constexpr float mOverlayRotateRadius = 2.0f;
	static constexpr float mOverlayRotateCameraRadius = 2.5f;

	static constexpr int mMoveIndexStart = 0;
	static constexpr int mMoveIndexCount = 36;
	static constexpr int mRotateIndexStart = mMoveIndexStart + mMoveIndexCount * 3;
	static constexpr int mRotateIndexCount = 120;
	static constexpr int mMovePlaneIndexStart = mRotateIndexStart + mRotateIndexCount * 3;
	static constexpr int mMovePlaneIndexCount = 4;
	static constexpr int mTrainTrackRotateIndexStart = mMovePlaneIndexStart + mMovePlaneIndexCount * 3;
	static constexpr int mTrainTrackRotateIndexCount = 96;
	static constexpr int mTrainTrackInsertIndexStart = mTrainTrackRotateIndexStart + mTrainTrackRotateIndexCount * 2;
	static constexpr int mTrainTrackInsertIndexCount = 72;
	static constexpr int mTrainTrackConnectionIndexStart = mTrainTrackInsertIndexStart + mTrainTrackInsertIndexCount;
	static constexpr int mTrainTrackConnectionIndexCount = 36;
	static constexpr int mIndexCount = mTrainTrackConnectionIndexStart + mTrainTrackConnectionIndexCount;
	static const GLushort mRotateMoveIndices[mIndexCount];
	static swVector3 mRotateMoveVertices[51 + 138 + 10 + 74 + 28 + 24];
};
