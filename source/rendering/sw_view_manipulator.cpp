#include "sw_global.h"
#include "sw_application.h"
#include "sw_view_manipulator.h"
#include "sw_view.h"
#include "sw_model.h"
#include "sw_object.h"
#include "sw_object_piece.h"
#include "sw_object_camera.h"
#include "sw_piece_data.h"
#include "sw_synthesis.h"
#include "sw_train_track.h"
#include "sw_window_main.h"
#include "sw_texture_font.h"

swVertexBuffer swViewManipulator::mRotateMoveVertexBuffer;
swIndexBuffer swViewManipulator::mRotateMoveIndexBuffer;
swVector3 swViewManipulator::mRotateMoveVertices[51 + 138 + 10 + 74 + 28 + 24];

swVector4 swViewManipulator::GetAxisColor(int AxisIndex, bool Selected)
{
	const swPreferences& Preferences = swGetPreferences();
	const quint32 AxisColors[3] = { Preferences.mAxisXColor, Preferences.mAxisYColor, Preferences.mAxisZColor };
	swVector4 Color = swVector4FromColor(AxisColors[AxisIndex]);

	// Selected handles use a lighter version of the axis color.
	if (Selected)
	{
		Color.x += (1.0f - Color.x) * 0.45f;
		Color.y += (1.0f - Color.y) * 0.45f;
		Color.z += (1.0f - Color.z) * 0.45f;
	}

	return Color;
}
const GLushort swViewManipulator::mRotateMoveIndices[mIndexCount] =
{
	// Move X
	0, 1, 2, 0, 2, 3, 0, 3, 4, 0, 4, 5, 0, 5, 6, 0, 6, 7, 0, 7, 8, 0, 8, 1,
	9, 10, 14, 14, 13, 9, 11, 12, 15, 15, 16, 12,
	// Move Y
	17, 18, 19, 17, 19, 20, 17, 20, 21, 17, 21, 22, 17, 22, 23, 17, 23, 24, 17, 24, 25, 17, 25, 18,
	26, 27, 31, 31, 30, 26, 28, 29, 32, 32, 33, 29,
	// Move Z
	34, 35, 36, 34, 36, 37, 34, 37, 38, 34, 38, 39, 34, 39, 40, 34, 40, 41, 34, 41, 42, 34, 42, 35,
	43, 44, 48, 48, 47, 43, 45, 46, 49, 49, 50, 46,
	// Rotate X
	51, 52, 53, 51, 53, 54, 51, 54, 55, 51, 55, 56, 51, 56, 57, 51, 57, 58, 51, 58, 59, 51, 59, 52,
	60, 61, 62, 60, 62, 63, 60, 63, 64, 60, 64, 65, 60, 65, 66, 60, 66, 67, 60, 67, 68, 60, 68, 61,
	69, 70, 71, 71, 72, 70, 71, 72, 73, 73, 74, 72, 73, 74, 75, 75, 76, 74, 75, 76, 77, 77, 78, 76, 77, 78, 79, 79, 80, 78, 79, 80, 81, 81, 82, 80,
	83, 84, 85, 85, 86, 84, 85, 86, 87, 87, 88, 86, 87, 88, 89, 89, 90, 88, 89, 90, 91, 91, 92, 90, 91, 92, 93, 93, 94, 92, 93, 94, 95, 95, 96, 94,
	// Rotate Y
	97, 98, 99, 97, 99, 100, 97, 100, 101, 97, 101, 102, 97, 102, 103, 97, 103, 104, 97, 104, 105, 97, 105, 98,
	106, 107, 108, 106, 108, 109, 106, 109, 110, 106, 110, 111, 106, 111, 112, 106, 112, 113, 106, 113, 114, 106, 114, 107,
	115, 116, 117, 117, 118, 116, 117, 118, 119, 119, 120, 118, 119, 120, 121, 121, 122, 120, 121, 122, 123, 123, 124, 122, 123, 124, 125, 125, 126, 124, 125, 126, 127, 127, 128, 126,
	129, 130, 131, 131, 132, 130, 131, 132, 133, 133, 134, 132, 133, 134, 135, 135, 136, 134, 135, 136, 137, 137, 138, 136, 137, 138, 139, 139, 140, 138, 139, 140, 141, 141, 142, 140,
	// Rotate Z
	143, 144, 145, 143, 145, 146, 143, 146, 147, 143, 147, 148, 143, 148, 149, 143, 149, 150, 143, 150, 151, 143, 151, 144,
	152, 153, 154, 152, 154, 155, 152, 155, 156, 152, 156, 157, 152, 157, 158, 152, 158, 159, 152, 159, 160, 152, 160, 153,
	161, 162, 163, 163, 164, 162, 163, 164, 165, 165, 166, 164, 165, 166, 167, 167, 168, 166, 167, 168, 169, 169, 170, 168, 169, 170, 171, 171, 172, 170, 171, 172, 173, 173, 174, 172,
	175, 176, 177, 177, 178, 176, 177, 178, 179, 179, 180, 178, 179, 180, 181, 181, 182, 180, 181, 182, 183, 183, 184, 182, 183, 184, 185, 185, 186, 184, 185, 186, 187, 187, 188, 186,
	// Move Planes
	189, 190, 191, 192, 189, 193, 194, 195, 189, 196, 197, 198,
	// Train Track Rotate Right
	199, 200, 201, 199, 201, 202, 199, 202, 203, 199, 203, 204, 199, 204, 205, 199, 205, 206, 199, 206, 207, 199, 207, 200,
	208, 209, 210, 210, 211, 209, 210, 211, 212, 212, 213, 211, 212, 213, 214, 214, 215, 213, 214, 215, 216, 216, 217, 215, 216, 217, 218, 218, 219, 217, 218, 219, 220, 220, 221, 219,
	222, 223, 224, 224, 225, 223, 224, 225, 226, 226, 227, 225, 226, 227, 228, 228, 229, 227, 228, 229, 230, 230, 231, 229, 230, 231, 232, 232, 233, 231, 232, 233, 234, 234, 235, 233,
	// Train Track Rotate Left
	236, 237, 238, 236, 238, 239, 236, 239, 240, 236, 240, 241, 236, 241, 242, 236, 242, 243, 236, 243, 244, 236, 244, 237,
	245, 246, 247, 247, 248, 246, 247, 248, 249, 249, 250, 248, 249, 250, 251, 251, 252, 250, 251, 252, 253, 253, 254, 252, 253, 254, 255, 255, 256, 254, 255, 256, 257, 257, 258, 256,
	259, 260, 261, 261, 262, 260, 261, 262, 263, 263, 264, 262, 263, 264, 265, 265, 266, 264, 265, 266, 267, 267, 268, 266, 267, 268, 269, 269, 270, 268, 269, 270, 271, 271, 272, 270,
	// Train Track Insert
	273, 274, 275, 275, 276, 274, 275, 276, 277, 277, 278, 276, 277, 278, 279, 279, 280, 278, 279, 280, 281, 281, 282, 280, 281, 282, 283, 283, 284, 282, 283, 284, 285, 285, 286, 284,
	287, 288, 289, 289, 290, 288, 289, 290, 291, 291, 292, 290, 291, 292, 293, 293, 294, 292, 293, 294, 295, 295, 296, 294, 295, 296, 297, 297, 298, 296, 297, 298, 299, 299, 300, 298,
	// Train Track Connection
	301, 302, 303, 301, 303, 304, 308, 307, 306, 308, 306, 305, 306, 302, 301, 305, 306, 301,
	308, 304, 303, 307, 308, 303, 301, 304, 308, 301, 308, 305, 307, 303, 302, 306, 307, 302
};

swViewManipulator::swViewManipulator(swView* View)
	: mView(View)
{
}

void swViewManipulator::CreateResources(swContext* Context)
{
	float* CurVert = mRotateMoveVertices[0].GetFloats();

	const float OverlayMovePlaneSize = 0.5f;
	const float OverlayMoveArrowSize = 1.5f;
	const float OverlayMoveArrowCapSize = 0.9f;
	const float OverlayArrowCapRadius = 0.1f;
	const float OverlayMoveArrowBodySize = 1.2f;
	const float OverlayArrowBodyRadius = 0.05f;
	const float OverlayRotateArrowStart = 1.0f;
	const float OverlayRotateArrowEnd = 1.5f;
	const float OverlayRotateArrowCenter = 1.2f;

	*CurVert++ = OverlayMoveArrowSize; *CurVert++ = 0.0f; *CurVert++ = 0.0f;

	for (int EdgeIndex = 0; EdgeIndex < 8; EdgeIndex++)
	{
		*CurVert++ = OverlayMoveArrowCapSize;
		*CurVert++ = cosf(SW_TWO_PI * EdgeIndex / 8) * OverlayArrowCapRadius;
		*CurVert++ = sinf(SW_TWO_PI * EdgeIndex / 8) * OverlayArrowCapRadius;
	}

	*CurVert++ = 0.0f; *CurVert++ = -OverlayArrowBodyRadius; *CurVert++ = 0.0f;
	*CurVert++ = 0.0f; *CurVert++ = OverlayArrowBodyRadius; *CurVert++ = 0.0f;
	*CurVert++ = 0.0f; *CurVert++ = 0.0f; *CurVert++ = -OverlayArrowBodyRadius;
	*CurVert++ = 0.0f; *CurVert++ = 0.0f; *CurVert++ = OverlayArrowBodyRadius;
	*CurVert++ = OverlayMoveArrowBodySize; *CurVert++ = -OverlayArrowBodyRadius; *CurVert++ = 0.0f;
	*CurVert++ = OverlayMoveArrowBodySize; *CurVert++ = OverlayArrowBodyRadius; *CurVert++ = 0.0f;
	*CurVert++ = OverlayMoveArrowBodySize; *CurVert++ = 0.0f; *CurVert++ = -OverlayArrowBodyRadius;
	*CurVert++ = OverlayMoveArrowBodySize; *CurVert++ = 0.0f; *CurVert++ = OverlayArrowBodyRadius;

	for (int VertIdx = 0; VertIdx < 17; VertIdx++)
	{
		*CurVert = *(CurVert - 50); CurVert++;
		*CurVert = *(CurVert - 52); CurVert++;
		*CurVert = *(CurVert - 51); CurVert++;
	}

	for (int VertIdx = 0; VertIdx < 17; VertIdx++)
	{
		*CurVert = *(CurVert - 100); CurVert++;
		*CurVert = *(CurVert - 102); CurVert++;
		*CurVert = *(CurVert - 104); CurVert++;
	}

	// Rotate X
	*CurVert++ = 0.0f; *CurVert++ = OverlayRotateArrowEnd - OverlayArrowCapRadius; *CurVert++ = OverlayRotateArrowStart;

	for (int EdgeIndex = 0; EdgeIndex < 8; EdgeIndex++)
	{
		*CurVert++ = cosf(SW_TWO_PI * EdgeIndex / 8) * OverlayArrowCapRadius;
		*CurVert++ = sinf(SW_TWO_PI * EdgeIndex / 8) * OverlayArrowCapRadius + OverlayRotateArrowEnd - OverlayArrowCapRadius;
		*CurVert++ = OverlayRotateArrowCenter;
	}

	*CurVert++ = 0.0f; *CurVert++ = OverlayRotateArrowStart; *CurVert++ = OverlayRotateArrowEnd - OverlayArrowCapRadius;

	for (int EdgeIndex = 0; EdgeIndex < 8; EdgeIndex++)
	{
		*CurVert++ = cosf(SW_TWO_PI * EdgeIndex / 8) * OverlayArrowCapRadius;
		*CurVert++ = OverlayRotateArrowCenter;
		*CurVert++ = sinf(SW_TWO_PI * EdgeIndex / 8) * OverlayArrowCapRadius + OverlayRotateArrowEnd - OverlayArrowCapRadius;
	}

	for (int EdgeIndex = 0; EdgeIndex < 7; EdgeIndex++)
	{
		const float Radius1 = OverlayRotateArrowEnd - OverlayArrowCapRadius - OverlayRotateArrowCenter - OverlayArrowBodyRadius;
		const float Radius2 = OverlayRotateArrowEnd - OverlayArrowCapRadius - OverlayRotateArrowCenter + OverlayArrowBodyRadius;
		float x = cosf(SW_TWO_PI / 4 * EdgeIndex / 6);
		float y = sinf(SW_TWO_PI / 4 * EdgeIndex / 6);

		*CurVert++ = 0.0f;
		*CurVert++ = OverlayRotateArrowCenter + x * Radius1;
		*CurVert++ = OverlayRotateArrowCenter + y * Radius1;
		*CurVert++ = 0.0f;
		*CurVert++ = OverlayRotateArrowCenter + x * Radius2;
		*CurVert++ = OverlayRotateArrowCenter + y * Radius2;
	}

	for (int EdgeIndex = 0; EdgeIndex < 7; EdgeIndex++)
	{
		const float Radius = OverlayRotateArrowEnd - OverlayArrowCapRadius - OverlayRotateArrowCenter;
		float x = cosf(SW_TWO_PI / 4 * EdgeIndex / 6);
		float y = sinf(SW_TWO_PI / 4 * EdgeIndex / 6);

		*CurVert++ = -OverlayArrowBodyRadius;
		*CurVert++ = OverlayRotateArrowCenter + x * Radius;
		*CurVert++ = OverlayRotateArrowCenter + y * Radius;
		*CurVert++ = OverlayArrowBodyRadius;
		*CurVert++ = OverlayRotateArrowCenter + x * Radius;
		*CurVert++ = OverlayRotateArrowCenter + y * Radius;
	}

	// Rotate Y
	for (int VertIdx = 0; VertIdx < 46; VertIdx++)
	{
		*CurVert = *(CurVert - 137); CurVert++;
		*CurVert = *(CurVert - 139); CurVert++;
		*CurVert = *(CurVert - 138); CurVert++;
	}

	// Rotate Z
	for (int VertIdx = 0; VertIdx < 46; VertIdx++)
	{
		*CurVert = *(CurVert - 274); CurVert++;
		*CurVert = *(CurVert - 276); CurVert++;
		*CurVert = *(CurVert - 278); CurVert++;
	}

	// Move Planes
	*CurVert++ = 0.0f; *CurVert++ = 0.0f; *CurVert++ = 0.0f;
	*CurVert++ = 0.0f; *CurVert++ = OverlayMovePlaneSize; *CurVert++ = 0.0f;
	*CurVert++ = 0.0f; *CurVert++ = OverlayMovePlaneSize; *CurVert++ = OverlayMovePlaneSize;
	*CurVert++ = 0.0f; *CurVert++ = 0.0f; *CurVert++ = OverlayMovePlaneSize;
	*CurVert++ = OverlayMovePlaneSize; *CurVert++ = 0.0f; *CurVert++ = 0.0f;
	*CurVert++ = OverlayMovePlaneSize; *CurVert++ = 0.0f; *CurVert++ = OverlayMovePlaneSize;
	*CurVert++ = 0.0f; *CurVert++ = 0.0f; *CurVert++ = OverlayMovePlaneSize;
	*CurVert++ = 0.0f; *CurVert++ = OverlayMovePlaneSize; *CurVert++ = 0.0f;
	*CurVert++ = OverlayMovePlaneSize; *CurVert++ = OverlayMovePlaneSize; *CurVert++ = 0.0f;
	*CurVert++ = OverlayMovePlaneSize; *CurVert++ = 0.0f; *CurVert++ = 0.0f;

	const float OverlayTrainTrackStart = 0.0f;
	const float OverlayTrainTrackEnd = 0.5f;
	const float OverlayTrainTrackCenter = 0.2f;
	const float OverlayTrainTrackDistance = 0.5f;

	// Train Track Rotate Right
	*CurVert++ = OverlayTrainTrackStart; *CurVert++ = OverlayTrainTrackEnd + OverlayTrainTrackDistance - OverlayArrowCapRadius; *CurVert++ = 0.0f;

	for (int EdgeIndex = 0; EdgeIndex < 8; EdgeIndex++)
	{
		*CurVert++ = OverlayTrainTrackCenter;
		*CurVert++ = sinf(SW_TWO_PI * EdgeIndex / 8) * OverlayArrowCapRadius + OverlayTrainTrackEnd + OverlayTrainTrackDistance - OverlayArrowCapRadius;
		*CurVert++ = cosf(SW_TWO_PI * EdgeIndex / 8) * OverlayArrowCapRadius;
	}

	for (int EdgeIndex = 0; EdgeIndex < 7; EdgeIndex++)
	{
		const float Radius1 = OverlayTrainTrackEnd - OverlayArrowCapRadius - OverlayTrainTrackCenter - OverlayArrowBodyRadius;
		const float Radius2 = OverlayTrainTrackEnd - OverlayArrowCapRadius - OverlayTrainTrackCenter + OverlayArrowBodyRadius;
		float x = cosf(SW_TWO_PI / 4 * EdgeIndex / 6);
		float y = sinf(SW_TWO_PI / 4 * EdgeIndex / 6);

		*CurVert++ = OverlayTrainTrackCenter + y * Radius1;
		*CurVert++ = OverlayTrainTrackCenter + OverlayTrainTrackDistance + x * Radius1;
		*CurVert++ = 0.0f;
		*CurVert++ = OverlayTrainTrackCenter + y * Radius2;
		*CurVert++ = OverlayTrainTrackCenter + OverlayTrainTrackDistance + x * Radius2;
		*CurVert++ = 0.0f;
	}

	for (int EdgeIndex = 0; EdgeIndex < 7; EdgeIndex++)
	{
		const float Radius = OverlayTrainTrackEnd - OverlayArrowCapRadius - OverlayTrainTrackCenter;
		float x = cosf(SW_TWO_PI / 4 * EdgeIndex / 6);
		float y = sinf(SW_TWO_PI / 4 * EdgeIndex / 6);

		*CurVert++ = OverlayTrainTrackCenter + y * Radius;
		*CurVert++ = OverlayTrainTrackCenter + OverlayTrainTrackDistance + x * Radius;
		*CurVert++ = -OverlayArrowBodyRadius;
		*CurVert++ = OverlayTrainTrackCenter + y * Radius;
		*CurVert++ = OverlayTrainTrackCenter + OverlayTrainTrackDistance + x * Radius;
		*CurVert++ = OverlayArrowBodyRadius;
	}

	// Train Track Rotate Left
	for (int VertIdx = 0; VertIdx < 37; VertIdx++)
	{
		*CurVert = *(CurVert - 111); CurVert++;
		*CurVert = -*(CurVert - 111); CurVert++;
		*CurVert = *(CurVert - 111); CurVert++;
	}

	const float OverlayTrainTrackInsertDistance = OverlayTrainTrackEnd + 0.1f;
	const float OverlayTrainTrackInsertLength = 0.3f;
	const float OverlayTrainTrackInsertWidth = 0.07f;

	// Train Track Insert
	for (int EdgeIndex = 0; EdgeIndex < 7; EdgeIndex++)
	{
		const float Radius = OverlayTrainTrackInsertWidth;
		float x = cosf(SW_TWO_PI * EdgeIndex / 6);
		float y = sinf(SW_TWO_PI * EdgeIndex / 6);

		*CurVert++ = x * Radius + OverlayTrainTrackInsertDistance;
		*CurVert++ = OverlayTrainTrackInsertLength;
		*CurVert++ = y * Radius;

		*CurVert++ = x * Radius + OverlayTrainTrackInsertDistance;
		*CurVert++ = -OverlayTrainTrackInsertLength;
		*CurVert++ = y * Radius;
	}

	for (int EdgeIndex = 0; EdgeIndex < 7; EdgeIndex++)
	{
		const float Radius = OverlayTrainTrackInsertWidth;
		float x = cosf(SW_TWO_PI * EdgeIndex / 6);
		float y = sinf(SW_TWO_PI * EdgeIndex / 6);

		*CurVert++ = OverlayTrainTrackInsertLength + OverlayTrainTrackInsertDistance;
		*CurVert++ = x * Radius;
		*CurVert++ = y * Radius;

		*CurVert++ = -OverlayTrainTrackInsertLength + OverlayTrainTrackInsertDistance;
		*CurVert++ = x * Radius;
		*CurVert++ = y * Radius;
	}

	const float OverlayTrainTrackBoxSize = 0.15f;

	*CurVert++ = -OverlayTrainTrackBoxSize; *CurVert++ = -OverlayTrainTrackBoxSize; *CurVert++ = -OverlayTrainTrackBoxSize;
	*CurVert++ = -OverlayTrainTrackBoxSize; *CurVert++ =  OverlayTrainTrackBoxSize; *CurVert++ = -OverlayTrainTrackBoxSize;
	*CurVert++ =  OverlayTrainTrackBoxSize; *CurVert++ =  OverlayTrainTrackBoxSize; *CurVert++ = -OverlayTrainTrackBoxSize;
	*CurVert++ =  OverlayTrainTrackBoxSize; *CurVert++ = -OverlayTrainTrackBoxSize; *CurVert++ = -OverlayTrainTrackBoxSize;
	*CurVert++ = -OverlayTrainTrackBoxSize; *CurVert++ = -OverlayTrainTrackBoxSize; *CurVert++ =  OverlayTrainTrackBoxSize;
	*CurVert++ = -OverlayTrainTrackBoxSize; *CurVert++ =  OverlayTrainTrackBoxSize; *CurVert++ =  OverlayTrainTrackBoxSize;
	*CurVert++ =  OverlayTrainTrackBoxSize; *CurVert++ =  OverlayTrainTrackBoxSize; *CurVert++ =  OverlayTrainTrackBoxSize;
	*CurVert++ =  OverlayTrainTrackBoxSize; *CurVert++ = -OverlayTrainTrackBoxSize; *CurVert++ =  OverlayTrainTrackBoxSize;

	mRotateMoveVertexBuffer = Context->CreateVertexBuffer(sizeof(mRotateMoveVertices), mRotateMoveVertices[0].GetFloats());
	mRotateMoveIndexBuffer = Context->CreateIndexBuffer(sizeof(mRotateMoveIndices), mRotateMoveIndices);
}

void swViewManipulator::DestroyResources(swContext* Context)
{
	Context->DestroyVertexBuffer(mRotateMoveVertexBuffer);
	Context->DestroyIndexBuffer(mRotateMoveIndexBuffer);
}

void swViewManipulator::DrawSelectMove(swTrackButton TrackButton, swTrackTool TrackTool, quint32 TrackToolSection)
{
	const swCamera* Camera = mView->GetCamera();
	swContext* Context = mView->mContext;

	Context->SetMaterial(swMaterialType::UnlitColor);
	Context->SetViewMatrix(Camera->mWorldView);
	Context->SetProjectionMatrix(mView->GetProjectionMatrix());

	Context->EnableDepthTest(false);

	swVector3 OverlayCenter;
	swMatrix33 RelativeRotation;
	swModel* ActiveModel = mView->GetActiveModel();
	ActiveModel->GetMoveRotateTransform(OverlayCenter, RelativeRotation);
	bool AnyPiecesSelected = ActiveModel->AnyPiecesSelected();
	swObject* Focus = ActiveModel->GetFocusObject();

	swMatrix44 WorldMatrix = swMatrix44(RelativeRotation, OverlayCenter);

	if (ActiveModel != mView->GetModel())
		WorldMatrix = swMultiply(WorldMatrix, mView->GetActiveSubmodelTransform());

	const float OverlayScale = mView->GetOverlayScale();
	WorldMatrix = swMultiply(swMatrix44Scale(swVector3(OverlayScale, OverlayScale, OverlayScale)), WorldMatrix);

	Context->SetWorldMatrix(WorldMatrix);
	Context->SetLineWidth(1.0f);

	Context->SetIndexBuffer(mRotateMoveIndexBuffer);
	Context->SetVertexBuffer(mRotateMoveVertexBuffer);
	Context->SetVertexFormatPosition(3);

	quint32 AllowedTransforms = Focus ? Focus->GetAllowedTransforms() : SW_OBJECT_TRANSFORM_MOVE_X | SW_OBJECT_TRANSFORM_MOVE_Y | SW_OBJECT_TRANSFORM_MOVE_Z | SW_OBJECT_TRANSFORM_ROTATE_X | SW_OBJECT_TRANSFORM_ROTATE_Y | SW_OBJECT_TRANSFORM_ROTATE_Z;

	if (TrackButton == swTrackButton::None)
	{
		if (AllowedTransforms & SW_OBJECT_TRANSFORM_MOVE_X)
		{
			if ((TrackTool == swTrackTool::MoveX) || (TrackTool == swTrackTool::MoveXY) || (TrackTool == swTrackTool::MoveXZ))
			{
				Context->SetColor(GetAxisColor(0, true));
				Context->DrawIndexedPrimitives(GL_TRIANGLES, 36, GL_UNSIGNED_SHORT, 0);
			}
			else
			{
				Context->SetColor(GetAxisColor(0, false));
				Context->DrawIndexedPrimitives(GL_TRIANGLES, 36, GL_UNSIGNED_SHORT, 0);
			}
		}

		if (AllowedTransforms & SW_OBJECT_TRANSFORM_MOVE_Y)
		{
			if (((TrackTool == swTrackTool::MoveY) || (TrackTool == swTrackTool::MoveXY) || (TrackTool == swTrackTool::MoveYZ)) && (AllowedTransforms & SW_OBJECT_TRANSFORM_MOVE_Y))
			{
				Context->SetColor(GetAxisColor(1, true));
				Context->DrawIndexedPrimitives(GL_TRIANGLES, 36, GL_UNSIGNED_SHORT, 36 * 2);
			}
			else
			{
				Context->SetColor(GetAxisColor(1, false));
				Context->DrawIndexedPrimitives(GL_TRIANGLES, 36, GL_UNSIGNED_SHORT, 36 * 2);
			}
		}

		if (AllowedTransforms & SW_OBJECT_TRANSFORM_MOVE_Z)
		{
			if (((TrackTool == swTrackTool::MoveZ) || (TrackTool == swTrackTool::MoveXZ) || (TrackTool == swTrackTool::MoveYZ)) && (AllowedTransforms & SW_OBJECT_TRANSFORM_MOVE_Z))
			{
				Context->SetColor(GetAxisColor(2, true));
				Context->DrawIndexedPrimitives(GL_TRIANGLES, 36, GL_UNSIGNED_SHORT, 72 * 2);
			}
			else
			{
				Context->SetColor(GetAxisColor(2, false));
				Context->DrawIndexedPrimitives(GL_TRIANGLES, 36, GL_UNSIGNED_SHORT, 72 * 2);
			}
		}
	}

	if (gMainWindow->GetTool() == swTool::Select && TrackButton == swTrackButton::None && AnyPiecesSelected)
	{
		if (AllowedTransforms & SW_OBJECT_TRANSFORM_ROTATE_X)
		{
			if (TrackTool == swTrackTool::RotateX)
				Context->SetColor(GetAxisColor(0, true));
			else
				Context->SetColor(GetAxisColor(0, false));

			Context->DrawIndexedPrimitives(GL_TRIANGLES, 120, GL_UNSIGNED_SHORT, 108 * 2);
		}

		if (AllowedTransforms & SW_OBJECT_TRANSFORM_ROTATE_Y)
		{
			if (TrackTool == swTrackTool::RotateY)
				Context->SetColor(GetAxisColor(1, true));
			else
				Context->SetColor(GetAxisColor(1, false));

			Context->DrawIndexedPrimitives(GL_TRIANGLES, 120, GL_UNSIGNED_SHORT, (108 + 120) * 2);
		}

		if (AllowedTransforms & SW_OBJECT_TRANSFORM_ROTATE_Z)
		{
			if (TrackTool == swTrackTool::RotateZ)
				Context->SetColor(GetAxisColor(2, true));
			else
				Context->SetColor(GetAxisColor(2, false));

			Context->DrawIndexedPrimitives(GL_TRIANGLES, 120, GL_UNSIGNED_SHORT, (108 + 240) * 2);
		}
	}

	if ((TrackButton == swTrackButton::None) && ((TrackTool == swTrackTool::MoveXY) || (TrackTool == swTrackTool::MoveXZ) || (TrackTool == swTrackTool::MoveYZ)))
	{
		Context->EnableColorBlend(true);

		Context->SetColor(0.8f, 0.8f, 0.0f, 0.3f);

		if (TrackTool == swTrackTool::MoveXY)
			Context->DrawIndexedPrimitives(GL_TRIANGLE_FAN, 4, GL_UNSIGNED_SHORT, (108 + 360 + 8) * 2);
		else if (TrackTool == swTrackTool::MoveXZ)
			Context->DrawIndexedPrimitives(GL_TRIANGLE_FAN, 4, GL_UNSIGNED_SHORT, (108 + 360 + 4) * 2);
		else if (TrackTool == swTrackTool::MoveYZ)
			Context->DrawIndexedPrimitives(GL_TRIANGLE_FAN, 4, GL_UNSIGNED_SHORT, (108 + 360) * 2);

		Context->EnableColorBlend(false);
	}

	if (Focus && Focus->IsPiece())
	{
		swPiece* Piece = (swPiece*)Focus;
		quint32 Section = Piece->GetFocusSection();

		if (Section >= SW_PIECE_SECTION_CONTROL_POINT_FIRST && Section <= SW_PIECE_SECTION_CONTROL_POINT_LAST && Piece->mPieceData->GetSynthInfo() && Piece->mPieceData->GetSynthInfo()->IsCurve())
		{
			int ControlPointIndex = Section - SW_PIECE_SECTION_CONTROL_POINT_FIRST;
			float Strength = Piece->GetControlPoints()[ControlPointIndex].Scale;
			const float ScaleStart = 2.0f;
			float Length = ScaleStart + Strength / OverlayScale;
			const float OverlayScaleInnerRadius = 0.075f;
			const float OverlayScaleRadius = 0.125f;

			swVector3 Verts[38];
			int NumVerts = 0;

			Verts[NumVerts++] = swVector3(Length - OverlayScaleRadius, 0.0f, 0.0f);
			Verts[NumVerts++] = swVector3(OverlayScaleRadius - Length, 0.0f, 0.0f);

			float SinTable[9], CosTable[9];

			for (int Step = 0; Step <= 8; Step++)
			{
				SinTable[Step] = sinf((float)Step / 8.0f * SW_TWO_PI);
				CosTable[Step] = cosf((float)Step / 8.0f * SW_TWO_PI);
			}

			for (int Step = 0; Step <= 8; Step++)
			{
				float x = CosTable[Step];
				float y = SinTable[Step];

				Verts[NumVerts++] = swVector3(Length + x * OverlayScaleInnerRadius, 0.0f, y * OverlayScaleInnerRadius);
				Verts[NumVerts++] = swVector3(Length + x * OverlayScaleRadius, 0.0f, y * OverlayScaleRadius);
			}

			for (int Step = 0; Step <= 8; Step++)
			{
				float x = CosTable[Step];
				float y = SinTable[Step];

				Verts[NumVerts++] = swVector3(-Length + x * OverlayScaleInnerRadius, 0.0f, y * OverlayScaleInnerRadius);
				Verts[NumVerts++] = swVector3(-Length + x * OverlayScaleRadius, 0.0f, y * OverlayScaleRadius);
			}

			if (TrackTool == swTrackTool::ScalePlus || TrackTool == swTrackTool::ScaleMinus)
				Context->SetColor(0.8f, 0.8f, 0.0f, 0.3f);
			else
				Context->SetColor(0.0f, 0.0f, 0.8f, 1.0f);

			Context->SetVertexBufferPointer(Verts);
			Context->ClearIndexBuffer();
			Context->SetVertexFormatPosition(3);

			Context->DrawPrimitives(GL_LINES, 0, 2);
			Context->DrawPrimitives(GL_TRIANGLE_STRIP, 2, 18);
			Context->DrawPrimitives(GL_TRIANGLE_STRIP, 20, 18);
		}
		else if (Piece->mPieceData->GetTrainTrackInfo() && TrackButton == swTrackButton::None)
		{
			DrawTrainTrack(Piece, Context, TrackTool, TrackToolSection);
		}
	}

	swVector3 MoveStartPosition, MoveEndPosition;
	if (ActiveModel->GetMouseToolMovePositions(MoveStartPosition, MoveEndPosition))
	{
		const float MoveDistance = swLength(MoveEndPosition - MoveStartPosition);

		if (ActiveModel != mView->GetModel())
		{
			MoveStartPosition = swMultiply31(MoveStartPosition, mView->GetActiveSubmodelTransform());
			MoveEndPosition = swMultiply31(MoveEndPosition, mView->GetActiveSubmodelTransform());
		}

		DrawMoveDistance(TrackButton, TrackTool, MoveStartPosition, MoveEndPosition, MoveDistance);
	}

	Context->EnableDepthTest(true);
}

void swViewManipulator::DrawTrainTrack(swPiece* Piece, swContext* Context, swTrackTool TrackTool, quint32 TrackToolSection)
{
	const swPreferences& Preferences = swGetPreferences();
	const swVector4 TrainTrackColor = swVector4FromColor(Preferences.mControlPointColor);

	if (Piece->GetFocusSection() >= SW_PIECE_SECTION_TRAIN_TRACK_CONNECTION_FIRST)
	{
		quint32 ConnectionIndex = Piece->GetFocusSection() - SW_PIECE_SECTION_TRAIN_TRACK_CONNECTION_FIRST;
		bool CanAdd = !Piece->IsTrainTrackConnected(ConnectionIndex);

		Context->SetColor(TrainTrackColor);

		if (TrackTool == swTrackTool::RotateTrainTrackRight)
		{
			Context->DrawIndexedPrimitives(GL_TRIANGLES, 96, GL_UNSIGNED_SHORT, (108 + 360 + 12 + 96) * 2);

			if (CanAdd)
				Context->DrawIndexedPrimitives(GL_TRIANGLES, 72, GL_UNSIGNED_SHORT, (108 + 360 + 12 + 192) * 2);

			Context->SetColor(0.8f, 0.8f, 0.0f, 1.0f);
			Context->DrawIndexedPrimitives(GL_TRIANGLES, 96, GL_UNSIGNED_SHORT, (108 + 360 + 12) * 2);
		}
		else if (TrackTool == swTrackTool::RotateTrainTrackLeft)
		{
			Context->DrawIndexedPrimitives(GL_TRIANGLES, 96, GL_UNSIGNED_SHORT, (108 + 360 + 12) * 2);

			if (CanAdd)
				Context->DrawIndexedPrimitives(GL_TRIANGLES, 72, GL_UNSIGNED_SHORT, (108 + 360 + 12 + 192) * 2);

			Context->SetColor(0.8f, 0.8f, 0.0f, 1.0f);
			Context->DrawIndexedPrimitives(GL_TRIANGLES, 96, GL_UNSIGNED_SHORT, (108 + 360 + 12 + 96) * 2);
		}
		else if (TrackTool == swTrackTool::InsertTrainTrack)
		{
			Context->DrawIndexedPrimitives(GL_TRIANGLES, 192, GL_UNSIGNED_SHORT, (108 + 360 + 12) * 2);

			if (CanAdd)
			{
				Context->SetColor(0.8f, 0.8f, 0.0f, 1.0f);
				Context->DrawIndexedPrimitives(GL_TRIANGLES, 72, GL_UNSIGNED_SHORT, (108 + 360 + 12 + 192) * 2);
			}
		}
		else
		{
			Context->DrawIndexedPrimitives(GL_TRIANGLES, 192, GL_UNSIGNED_SHORT, (108 + 360 + 12) * 2);

			if (CanAdd)
				Context->DrawIndexedPrimitives(GL_TRIANGLES, 72, GL_UNSIGNED_SHORT, (108 + 360 + 12 + 192) * 2);
		}
	}
	else
	{
		const swTrainTrackInformation* TrainTrackInfo = Piece->mPieceData->GetTrainTrackInfo();
		const std::vector<swTrainTrackConnection>& Connections = TrainTrackInfo->GetConnections();

		for (quint32 ConnectionIndex = 0; ConnectionIndex < Connections.size(); ConnectionIndex++)
		{
			if (Piece->IsTrainTrackConnected(ConnectionIndex))
				continue;

			swMatrix44 WorldMatrix = swMultiply(Connections[ConnectionIndex].Transform, Piece->mModelWorld);
			swModel* ActiveModel = mView->GetActiveModel();

			if (ActiveModel != mView->GetModel())
				WorldMatrix = swMultiply(WorldMatrix, mView->GetActiveSubmodelTransform());

			const float OverlayScale = mView->GetOverlayScale();
			WorldMatrix = swMultiply(swMatrix44Scale(swVector3(OverlayScale, OverlayScale, OverlayScale)), WorldMatrix);

			Context->SetWorldMatrix(WorldMatrix);

			if (TrackToolSection == SW_PIECE_SECTION_TRAIN_TRACK_CONNECTION_FIRST + ConnectionIndex && TrackTool != swTrackTool::SelectTrainTrack)
				Context->SetColor(0.8f, 0.8f, 0.0f, 1.0f);
			else
				Context->SetColor(TrainTrackColor);

			Context->DrawIndexedPrimitives(GL_TRIANGLES, 72, GL_UNSIGNED_SHORT, (108 + 360 + 12 + 192) * 2);
		}
	}

	const swVector4 ConnectionColor = swVector4FromColor(Preferences.mControlPointColor);
	const swVector4 ConnectionFocusedColor = swVector4FromColor(Preferences.mControlPointFocusedColor);

	const swTrainTrackInformation* TrainTrackInfo = Piece->mPieceData->GetTrainTrackInfo();
	const std::vector<swTrainTrackConnection>& Connections = TrainTrackInfo->GetConnections();

	for (quint32 ConnectionIndex = 0; ConnectionIndex < Connections.size(); ConnectionIndex++)
	{
		swMatrix44 WorldMatrix = swMultiply(Connections[ConnectionIndex].Transform, Piece->mModelWorld);
		swModel* ActiveModel = mView->GetActiveModel();

		if (ActiveModel != mView->GetModel())
			WorldMatrix = swMultiply(WorldMatrix, mView->GetActiveSubmodelTransform());

		const float OverlayScale = mView->GetOverlayScale();
		WorldMatrix = swMultiply(swMatrix44Scale(swVector3(OverlayScale, OverlayScale, OverlayScale)), WorldMatrix);

		Context->SetWorldMatrix(WorldMatrix);

		if (Piece->IsFocused(SW_PIECE_SECTION_TRAIN_TRACK_CONNECTION_FIRST + ConnectionIndex))
			Context->SetColor(ConnectionFocusedColor);
		else if (TrackToolSection == SW_PIECE_SECTION_TRAIN_TRACK_CONNECTION_FIRST + ConnectionIndex && TrackTool == swTrackTool::SelectTrainTrack)
			Context->SetColor(0.8f, 0.8f, 0.0f, 1.0f);
		else
			Context->SetColor(ConnectionColor);

		Context->DrawIndexedPrimitives(GL_TRIANGLES, mTrainTrackConnectionIndexCount, GL_UNSIGNED_SHORT, mTrainTrackConnectionIndexStart * 2);
	}
}

bool swViewManipulator::GetRotationWorldMatrix(swMatrix44& WorldMatrix) const
{
	swVector3 OverlayCenter;
	swMatrix33 RelativeRotation;
	swModel* ActiveModel = mView->GetActiveModel();

	if (!ActiveModel->GetMoveRotateTransform(OverlayCenter, RelativeRotation))
		return false;

	WorldMatrix = swMatrix44(RelativeRotation, OverlayCenter);

	if (ActiveModel != mView->GetModel())
		WorldMatrix = swMultiply(WorldMatrix, mView->GetActiveSubmodelTransform());

	return true;
}

swTrackTool swViewManipulator::GetRotationAxis(const swVector3& LocalIntersection, float Epsilon)
{
	const float dx = fabsf(LocalIntersection[0]);
	const float dy = fabsf(LocalIntersection[1]);
	const float dz = fabsf(LocalIntersection[2]);

	if (dx < dy)
	{
		if (dx < dz)
			return dx < Epsilon ? swTrackTool::RotateX : swTrackTool::None;

		return dz < Epsilon ? swTrackTool::RotateZ : swTrackTool::None;
	}

	if (dy < dz)
		return dy < Epsilon ? swTrackTool::RotateY : swTrackTool::None;

	return dz < Epsilon ? swTrackTool::RotateZ : swTrackTool::None;
}

std::optional<swViewManipulator::swRotationDiscInformation> swViewManipulator::GetRotationDiscInfo(swTrackTool TrackTool)
{
	switch (TrackTool)
	{
	case swTrackTool::RotateX:
		return swRotationDiscInformation{ 0, mOverlayRotateRadius, swVector4(0.0f, 0.0f, 0.0f, 1.0f), GetAxisColor(0, false), GetAxisColor(0, true), false };

	case swTrackTool::RotateY:
		return swRotationDiscInformation{ 1, mOverlayRotateRadius, swVector4(90.0f, 0.0f, 0.0f, 1.0f), GetAxisColor(1, false), GetAxisColor(1, true), false };

	case swTrackTool::RotateZ:
		return swRotationDiscInformation{ 2, mOverlayRotateRadius, swVector4(90.0f, 0.0f, -1.0f, 0.0f), GetAxisColor(2, false), GetAxisColor(2, true), false };

	case swTrackTool::RotateCamera:
		return swRotationDiscInformation{ 0, mOverlayRotateCameraRadius, swVector4(0.0f, 0.0f, 0.0f, 1.0f), mColorCamera, mColorCameraSelected, true };

	default:
		return std::nullopt;
	}
}

std::optional<swViewManipulator::swMoveAxisInformation> swViewManipulator::GetMoveAxisInfo(swTrackTool TrackTool)
{
	switch (TrackTool)
	{
	case swTrackTool::MoveX:
		return swMoveAxisInformation{ GetAxisColor(0, true) };

	case swTrackTool::MoveY:
		return swMoveAxisInformation{ GetAxisColor(1, true) };

	case swTrackTool::MoveZ:
		return swMoveAxisInformation{ GetAxisColor(2, true) };

	default:
		return std::nullopt;
	}
}

swMatrix44 swViewManipulator::GetRotationDiscWorldMatrix(const swRotationDiscInformation& DiscInfo, const swMatrix44& WorldMatrix) const
{
	if (!DiscInfo.CameraFacing)
		return swMultiply(swMatrix44FromAxisAngle(swVector3(DiscInfo.Rotation[1], DiscInfo.Rotation[2], DiscInfo.Rotation[3]), DiscInfo.Rotation[0] * SW_DEGREES_TO_RADIANS), WorldMatrix);

	swMatrix44 CameraWorldMatrix = swMatrix44AffineInverse(mView->GetCamera()->mWorldView);
	CameraWorldMatrix.SetTranslation(WorldMatrix.GetTranslation());
	return CameraWorldMatrix;
}

float swViewManipulator::GetRotationDiscStartAngle(const swRotationDiscInformation& DiscInfo, const swMatrix44& DiscWorldMatrix) const
{
	if (DiscInfo.CameraFacing)
		return mView->GetCameraRotationStartAngle() * SW_RADIANS_TO_DEGREES;

	swVector3 MouseDownRay[2] =
	{
		swVector3((float)mView->GetMouseDownX(), (float)mView->GetMouseDownY(), 0.0f),
		swVector3((float)mView->GetMouseDownX(), (float)mView->GetMouseDownY(), 1.0f)
	};
	mView->UnprojectPoints(MouseDownRay, 2);

	const swVector3 Center = DiscWorldMatrix.GetTranslation();
	const swVector3 Normal = swNormalize(swMultiply30(swVector3(1.0f, 0.0f, 0.0f), DiscWorldMatrix));
	const swVector4 Plane(Normal, -swDot(Normal, Center));
	swVector3 Intersection;

	if (!swLineSegmentPlaneIntersection(&Intersection, MouseDownRay[0], MouseDownRay[1], Plane))
		return 0.0f;

	const swVector3 LocalPoint = swMultiply(Intersection - Center, swMatrix33AffineInverse(swMatrix33(DiscWorldMatrix)));
	return -atan2f(LocalPoint[2], LocalPoint[1]) * SW_RADIANS_TO_DEGREES;
}

void swViewManipulator::DrawTrackballHover(const swMatrix44& WorldMatrix, float OverlayScale) const
{
	swContext* Context = mView->mContext;
	swMatrix44 Mat = swMatrix44AffineInverse(mView->GetCamera()->mWorldView);
	Mat.SetTranslation(WorldMatrix.GetTranslation());

	constexpr int SegmentCount = 32;
	swVector3 Verts[SegmentCount + 2];
	Verts[0] = swVector3(0.0f, 0.0f, 0.0f);

	for (int SegmentIndex = 0; SegmentIndex <= SegmentCount; SegmentIndex++)
	{
		const float Angle = SW_TWO_PI * SegmentIndex / SegmentCount;
		Verts[SegmentIndex + 1] = swVector3(cosf(Angle) * mOverlayRotateRadius * OverlayScale, sinf(Angle) * mOverlayRotateRadius * OverlayScale, 0.0f);
	}

	Context->SetColor(mColorTrackball);
	Context->SetWorldMatrix(Mat);
	Context->EnableColorBlend(true);
	Context->SetVertexBufferPointer(Verts);
	Context->SetVertexFormatPosition(3);
	Context->DrawPrimitives(GL_TRIANGLE_FAN, 0, SegmentCount + 2);
	Context->EnableColorBlend(false);
}

bool swViewManipulator::DrawRotationDisc(swTrackButton TrackButton, const swVector3& MouseToolDistance, const std::optional<swRotationDiscInformation>& RotationDisc, const swMatrix44& WorldMatrix, float OverlayScale) const
{
	if (swIsZero(MouseToolDistance) || TrackButton == swTrackButton::None || !RotationDisc)
		return false;

	swContext* Context = mView->mContext;
	const swRotationDiscInformation& DiscInfo = *RotationDisc;
	const float Radius = DiscInfo.Radius;
	const swMatrix44 RotatedWorldMatrix = GetRotationDiscWorldMatrix(DiscInfo, WorldMatrix);

	Context->SetColor(DiscInfo.FillColor[0], DiscInfo.FillColor[1], DiscInfo.FillColor[2], 0.3f);
	Context->SetWorldMatrix(RotatedWorldMatrix);
	Context->EnableColorBlend(true);

	swVector3 Verts[33];
	Verts[0] = swVector3(0.0f, 0.0f, 0.0f);
	int NumVerts = 1;

	Context->SetVertexBufferPointer(Verts);
	Context->SetVertexFormatPosition(3);

	const float StartAngle = GetRotationDiscStartAngle(DiscInfo, RotatedWorldMatrix);
	const float StartVectorAngle = DiscInfo.CameraFacing ? StartAngle : -StartAngle;
	const float SignedRotationAngle = MouseToolDistance[DiscInfo.AxisIndex];
	const int SegmentCount = std::max(1, (int)ceilf(fabsf(SignedRotationAngle) / (360.0f / 32.0f)));

	for (int SegmentIndex = 0; SegmentIndex <= SegmentCount; SegmentIndex++)
	{
		const float VertexAngle = StartVectorAngle + (DiscInfo.CameraFacing ? -1.0f : 1.0f) * SignedRotationAngle * SegmentIndex / SegmentCount;
		const float x = cosf(VertexAngle * SW_DEGREES_TO_RADIANS) * Radius * OverlayScale;
		const float y = sinf(VertexAngle * SW_DEGREES_TO_RADIANS) * Radius * OverlayScale;
		Verts[NumVerts++] = DiscInfo.CameraFacing ? swVector3(x, y, 0.0f) : swVector3(0.0f, x, y);

		if (NumVerts == 33)
		{
			Context->DrawPrimitives(GL_TRIANGLE_FAN, 0, NumVerts);
			Verts[1] = Verts[32];
			NumVerts = 2;
		}
	}

	if (NumVerts > 2)
		Context->DrawPrimitives(GL_TRIANGLE_FAN, 0, NumVerts);

	const float VectorRadius = Radius * OverlayScale;
	const float VectorWidth = OverlayScale * 0.035f;
	auto DrawVector = [&](float VectorAngle)
	{
		const float AngleRadians = VectorAngle * SW_DEGREES_TO_RADIANS;
		const float Cos = cosf(AngleRadians);
		const float Sin = sinf(AngleRadians);
		swVector3 Direction = DiscInfo.CameraFacing ? swVector3(Cos, Sin, 0.0f) : swVector3(0.0f, Cos, Sin);
		swVector3 Perpendicular = DiscInfo.CameraFacing ? swVector3(-Sin, Cos, 0.0f) : swVector3(0.0f, -Sin, Cos);
		Direction *= VectorRadius;
		Perpendicular *= VectorWidth;
		const swVector3 VectorVerts[6] =
		{
			Perpendicular, Direction + Perpendicular, Direction - Perpendicular,
			Perpendicular, Direction - Perpendicular, -Perpendicular
		};
		Context->SetVertexBufferPointer(VectorVerts);
		Context->SetVertexFormatPosition(3);
		Context->DrawPrimitives(GL_TRIANGLES, 0, 6);
	};

	Context->SetColor(DiscInfo.HighlightColor);
	DrawVector(StartVectorAngle);
	DrawVector(StartVectorAngle + (DiscInfo.CameraFacing ? -1.0f : 1.0f) * SignedRotationAngle);
	Context->EnableColorBlend(false);

	return true;
}

void swViewManipulator::DrawCameraRotationRing(swTrackButton TrackButton, swTrackTool TrackTool, const swMatrix44& WorldMatrix, float OverlayScale) const
{
	if (gMainWindow->GetTool() != swTool::Rotate || (TrackButton != swTrackButton::None && TrackTool != swTrackTool::RotateCamera))
		return;

	swContext* Context = mView->mContext;
	swMatrix44 Mat = swMatrix44AffineInverse(mView->GetCamera()->mWorldView);
	Mat.SetTranslation(WorldMatrix.GetTranslation());

	const float HalfWidth = OverlayScale * 0.035f;
	constexpr int SegmentCount = 48;
	swVector3 Verts[(SegmentCount + 1) * 2];
	int NumVerts = 0;

	for (int SegmentIndex = 0; SegmentIndex <= SegmentCount; SegmentIndex++)
	{
		const float Sin = sinf(SW_TWO_PI * SegmentIndex / SegmentCount);
		const float Cos = cosf(SW_TWO_PI * SegmentIndex / SegmentCount);
		Verts[NumVerts++] = swMultiply31(swVector3(Sin * (mOverlayRotateCameraRadius * OverlayScale - HalfWidth), Cos * (mOverlayRotateCameraRadius * OverlayScale - HalfWidth), 0.0f), Mat);
		Verts[NumVerts++] = swMultiply31(swVector3(Sin * (mOverlayRotateCameraRadius * OverlayScale + HalfWidth), Cos * (mOverlayRotateCameraRadius * OverlayScale + HalfWidth), 0.0f), Mat);
	}

	Context->SetColor(TrackTool == swTrackTool::RotateCamera ? mColorCameraSelected : mColorCamera);
	Context->SetWorldMatrix(swMatrix44Identity());
	Context->SetVertexBufferPointer(Verts);
	Context->SetVertexFormatPosition(3);
	Context->DrawPrimitives(GL_TRIANGLE_STRIP, 0, NumVerts);
}

void swViewManipulator::DrawAxisRotationRings(swTrackButton TrackButton, swTrackTool TrackTool, bool HasAngle, const swMatrix44& WorldMatrix, float OverlayScale) const
{
	const swCamera* Camera = mView->GetCamera();
	swContext* Context = mView->mContext;
	swVector3 ViewDir = swNormalize(Camera->mTargetPosition - Camera->mPosition);
	const swMatrix33 WorldToLocalMatrix = swMatrix33AffineInverse(swMatrix33(WorldMatrix));
	ViewDir = swMultiply(ViewDir, WorldToLocalMatrix);
	const swVector3 FrontVector = swNormalize(swMultiply(swNormalize(Camera->mTargetPosition - Camera->mPosition), WorldToLocalMatrix));

	Context->SetWorldMatrix(WorldMatrix);

	for (int PlaneIndex = 0; PlaneIndex < 3; PlaneIndex++)
	{
		const bool Selected = static_cast<int>(TrackTool) == static_cast<int>(swTrackTool::RotateX) + PlaneIndex;
		if (!Selected && (gMainWindow->GetTool() != swTool::Rotate || HasAngle || TrackButton != swTrackButton::None))
			continue;

		switch (PlaneIndex)
		{
		case 0:
			Context->SetColor(Selected ? GetAxisColor(0, true) : GetAxisColor(0, false));
			break;
		case 1:
			Context->SetColor(Selected ? GetAxisColor(1, true) : GetAxisColor(1, false));
			break;
		case 2:
			Context->SetColor(Selected ? GetAxisColor(2, true) : GetAxisColor(2, false));
			break;
		}

		const float HalfWidth = OverlayScale * 0.035f;
		constexpr int SegmentCount = 32;
		swVector3 Verts[SegmentCount * 6];
		int NumVerts = 0;

		for (int SegmentIndex = 0; SegmentIndex < SegmentCount; SegmentIndex++)
		{
			const float Sin1 = sinf(SW_TWO_PI * SegmentIndex / SegmentCount);
			const float Cos1 = cosf(SW_TWO_PI * SegmentIndex / SegmentCount);
			const float Sin2 = sinf(SW_TWO_PI * (SegmentIndex + 1) / SegmentCount);
			const float Cos2 = cosf(SW_TWO_PI * (SegmentIndex + 1) / SegmentCount);
			swVector3 v1, v2, t1, t2;

			switch (PlaneIndex)
			{
			case 0:
				v1 = swVector3(0.0f, Cos1, Sin1);
				v2 = swVector3(0.0f, Cos2, Sin2);
				t1 = swVector3(0.0f, -Sin1, Cos1);
				t2 = swVector3(0.0f, -Sin2, Cos2);
				break;
			case 1:
				v1 = swVector3(Cos1, 0.0f, Sin1);
				v2 = swVector3(Cos2, 0.0f, Sin2);
				t1 = swVector3(-Sin1, 0.0f, Cos1);
				t2 = swVector3(-Sin2, 0.0f, Cos2);
				break;
			case 2:
				v1 = swVector3(Cos1, Sin1, 0.0f);
				v2 = swVector3(Cos2, Sin2, 0.0f);
				t1 = swVector3(-Sin1, Cos1, 0.0f);
				t2 = swVector3(-Sin2, Cos2, 0.0f);
				break;
			}

			if (gMainWindow->GetTool() == swTool::Rotate && !HasAngle && TrackButton == swTrackButton::None && swDot(ViewDir, v1 + v2) > 0.0f)
				continue;

			const swVector3 NodeCenter1 = v1 * (mOverlayRotateRadius * OverlayScale);
			const swVector3 NodeCenter2 = v2 * (mOverlayRotateRadius * OverlayScale);
			swVector3 ScreenPerpendicular1 = swCross(FrontVector, t1);
			swVector3 ScreenPerpendicular2 = swCross(FrontVector, t2);
			const float ScreenPerpendicularLengthSquared1 = ScreenPerpendicular1.LengthSquared();
			const float ScreenPerpendicularLengthSquared2 = ScreenPerpendicular2.LengthSquared();

			if (ScreenPerpendicularLengthSquared1 < 0.000001f)
				ScreenPerpendicular1 = ScreenPerpendicular2;
			else if (ScreenPerpendicularLengthSquared2 < 0.000001f)
				ScreenPerpendicular2 = ScreenPerpendicular1;

			ScreenPerpendicular1 = swNormalize(ScreenPerpendicular1);
			ScreenPerpendicular2 = swNormalize(ScreenPerpendicular2);

			if (swDot(ScreenPerpendicular1, ScreenPerpendicular2) < 0.0f)
				ScreenPerpendicular2 = -ScreenPerpendicular2;

			const swVector3 Left1 = NodeCenter1 - (ScreenPerpendicular1 * HalfWidth);
			const swVector3 Right1 = NodeCenter1 + (ScreenPerpendicular1 * HalfWidth);
			const swVector3 Left2 = NodeCenter2 - (ScreenPerpendicular2 * HalfWidth);
			const swVector3 Right2 = NodeCenter2 + (ScreenPerpendicular2 * HalfWidth);

			Verts[NumVerts++] = Left1;
			Verts[NumVerts++] = Left2;
			Verts[NumVerts++] = Right1;
			Verts[NumVerts++] = Right1;
			Verts[NumVerts++] = Left2;
			Verts[NumVerts++] = Right2;
		}

		Context->SetVertexBufferPointer(Verts);
		Context->SetVertexFormatPosition(3);
		Context->DrawPrimitives(GL_TRIANGLES, 0, NumVerts);
	}
}

void swViewManipulator::DrawRotationText(swTrackButton TrackButton, const swVector3& MouseToolDistance, const std::optional<swRotationDiscInformation>& RotationDisc, const swMatrix44& WorldMatrix, float OverlayScale) const
{
	if (TrackButton == swTrackButton::None || !RotationDisc || !gTexFont.IsLoaded())
		return;

	swContext* Context = mView->mContext;
	const swRotationDiscInformation& DiscInfo = *RotationDisc;
	const float Angle = MouseToolDistance[DiscInfo.AxisIndex];
	if (swIsZero(Angle))
		return;

	const swMatrix44 RotatedWorldMatrix = GetRotationDiscWorldMatrix(DiscInfo, WorldMatrix);
	const float StartAngle = GetRotationDiscStartAngle(DiscInfo, RotatedWorldMatrix);
	const float StartVectorAngle = DiscInfo.CameraFacing ? StartAngle : -StartAngle;
	const float MidAngle = StartVectorAngle + (DiscInfo.CameraFacing ? -0.5f : 0.5f) * Angle;
	const float Radius = DiscInfo.Radius * OverlayScale * 0.5f;
	const float MidAngleRadians = MidAngle * SW_DEGREES_TO_RADIANS;
	const swVector3 TextPosition = DiscInfo.CameraFacing ? swVector3(cosf(MidAngleRadians) * Radius, sinf(MidAngleRadians) * Radius, 0.0f) : swVector3(0.0f, cosf(MidAngleRadians) * Radius, sinf(MidAngleRadians) * Radius);
	const swVector3 ScreenPos = mView->ProjectPoint(swMultiply31(TextPosition, RotatedWorldMatrix));
	const float UIScale = mView->GetUIScale();

	Context->SetMaterial(swMaterialType::TextSDF);
	Context->SetWorldMatrix(swMatrix44Identity());
	Context->SetViewMatrix(swMatrix44Translation(swVector3(0.375, 0.375, 0.0)));
	Context->SetProjectionMatrix(swMatrix44Orthographic(0.0f, mView->GetWidth() / UIScale, 0.0f, mView->GetHeight() / UIScale, -1.0f, 1.0f));
	Context->BindTexture2D(gTexFont.GetTexture());
	Context->EnableColorBlend(true);

	char Buffer[32];
	snprintf(Buffer, sizeof(Buffer), "%.2f", fabsf(Angle));

	int Width, Height;
	gTexFont.GetStringDimensions(&Width, &Height, Buffer);
	Context->SetColor(swVector4FromColor(swGetPreferences().mTextColor));
	Context->SetTextHaloColor(swVector4FromColor(swGetPreferences().mTextHaloColor));
	gTexFont.PrintText(Context, ScreenPos[0] / UIScale - (Width / 2), ScreenPos[1] / UIScale + (Height / 2), 0.0f, Buffer);
	Context->EnableColorBlend(false);
}

void swViewManipulator::DrawMoveDistance(swTrackButton TrackButton, swTrackTool TrackTool, const swVector3& StartPosition, const swVector3& EndPosition, float MoveDistance) const
{
	if (TrackButton == swTrackButton::None)
		return;

	const std::optional<swMoveAxisInformation> MoveAxis = GetMoveAxisInfo(TrackTool);
	if (!MoveAxis)
		return;

	swContext* Context = mView->mContext;
	const swMoveAxisInformation& AxisInfo = *MoveAxis;
	const swVector3 Distance = EndPosition - StartPosition;
	if (swIsZero(Distance))
		return;

	const swVector3 LineStart = StartPosition;
	const swVector3 LineEnd = EndPosition;
	const swVector3 TextPosition = (LineStart + LineEnd) * 0.5f;
	const swVector3 Direction = swNormalize(Distance);
	const swVector3 CameraFront = swNormalize(mView->GetCamera()->mTargetPosition - mView->GetCamera()->mPosition);
	swVector3 ScreenPerpendicular = swCross(CameraFront, Direction);

	if (swIsZero(ScreenPerpendicular))
		ScreenPerpendicular = swCross(swVector3(0.0f, 1.0f, 0.0f), Direction);

	if (swIsZero(ScreenPerpendicular))
		ScreenPerpendicular = swCross(swVector3(1.0f, 0.0f, 0.0f), Direction);

	ScreenPerpendicular = swNormalize(ScreenPerpendicular) * (mView->GetOverlayScale() * 0.035f);
	const swVector3 TickHalfLength = ScreenPerpendicular * 3.0f;
	const swVector3 TickHalfWidth = Direction * (mView->GetOverlayScale() * 0.035f);
	const swVector3 TextNormal = TickHalfLength;
	const swVector3 LineVertices[18] =
	{
		LineStart - ScreenPerpendicular, LineEnd - ScreenPerpendicular, LineStart + ScreenPerpendicular,
		LineStart + ScreenPerpendicular, LineEnd - ScreenPerpendicular, LineEnd + ScreenPerpendicular,
		LineStart - TickHalfLength - TickHalfWidth, LineStart + TickHalfLength - TickHalfWidth, LineStart - TickHalfLength + TickHalfWidth,
		LineStart - TickHalfLength + TickHalfWidth, LineStart + TickHalfLength - TickHalfWidth, LineStart + TickHalfLength + TickHalfWidth,
		LineEnd - TickHalfLength - TickHalfWidth, LineEnd + TickHalfLength - TickHalfWidth, LineEnd - TickHalfLength + TickHalfWidth,
		LineEnd - TickHalfLength + TickHalfWidth, LineEnd + TickHalfLength - TickHalfWidth, LineEnd + TickHalfLength + TickHalfWidth
	};
	Context->SetMaterial(swMaterialType::UnlitColor);
	Context->SetColor(AxisInfo.Color);
	Context->SetWorldMatrix(swMatrix44Identity());
	Context->SetVertexBufferPointer(LineVertices);
	Context->ClearIndexBuffer();
	Context->SetVertexFormatPosition(3);
	Context->DrawPrimitives(GL_TRIANGLES, 0, 18);

	if (!gTexFont.IsLoaded())
		return;

	const swVector3 ScreenPos = mView->ProjectPoint(TextPosition);
	const float UIScale = mView->GetUIScale();

	Context->SetMaterial(swMaterialType::TextSDF);
	Context->SetWorldMatrix(swMatrix44Identity());
	Context->SetViewMatrix(swMatrix44Translation(swVector3(0.375, 0.375, 0.0)));
	Context->SetProjectionMatrix(swMatrix44Orthographic(0.0f, mView->GetWidth() / UIScale, 0.0f, mView->GetHeight() / UIScale, -1.0f, 1.0f));
	Context->BindTexture2D(gTexFont.GetTexture());
	Context->EnableColorBlend(true);

	char Buffer[32];
	snprintf(Buffer, sizeof(Buffer), "%.2f", MoveDistance);

	int Width, Height;
	gTexFont.GetStringDimensions(&Width, &Height, Buffer);
	swVector3 TextScreenPos(ScreenPos[0] / UIScale, ScreenPos[1] / UIScale, 0.0f);

	const swVector3 NormalScreenPos = mView->ProjectPoint(TextPosition + TextNormal);
	swVector3 ScreenNormal(NormalScreenPos[0] - ScreenPos[0], NormalScreenPos[1] - ScreenPos[1], 0.0f);
	const float ScreenNormalLength = swLength(ScreenNormal);

	if (!swIsZero(ScreenNormalLength))
	{
		ScreenNormal /= ScreenNormalLength;
		const float TextRadius = fabsf(ScreenNormal[0]) * Width * 0.5f + fabsf(ScreenNormal[1]) * Height * 0.5f;
		const float LineRadius = ScreenNormalLength / UIScale;
		TextScreenPos += ScreenNormal * (TextRadius + LineRadius + 4.0f);
	}

	Context->SetColor(swVector4FromColor(swGetPreferences().mTextColor));
	Context->SetTextHaloColor(swVector4FromColor(swGetPreferences().mTextHaloColor));
	gTexFont.PrintText(Context, TextScreenPos[0] - (Width / 2), TextScreenPos[1] + (Height / 2), 0.0f, Buffer);
	Context->EnableColorBlend(false);
}

void swViewManipulator::DrawRotate(swTrackButton TrackButton, swTrackTool TrackTool)
{
	const swCamera* Camera = mView->GetCamera();
	swContext* Context = mView->mContext;

	const float OverlayScale = mView->GetOverlayScale();

	Context->SetMaterial(swMaterialType::UnlitColor);
	Context->SetViewMatrix(Camera->mWorldView);
	Context->SetProjectionMatrix(mView->GetProjectionMatrix());
	Context->SetLineWidth(1.0f);

	Context->EnableDepthTest(false);

	swModel* ActiveModel = mView->GetActiveModel();
	swVector3 MouseToolDistance = ActiveModel->SnapRotation(ActiveModel->GetMouseToolDistance());
	swMatrix44 WorldMatrix;
	if (!GetRotationWorldMatrix(WorldMatrix))
	{
		Context->EnableDepthTest(true);
		return;
	}

	if (TrackButton == swTrackButton::None && TrackTool == swTrackTool::RotateTrackBall)
		DrawTrackballHover(WorldMatrix, OverlayScale);

	const std::optional<swRotationDiscInformation> RotationDisc = GetRotationDiscInfo(TrackTool);

	const bool HasAngle = DrawRotationDisc(TrackButton, MouseToolDistance, RotationDisc, WorldMatrix, OverlayScale);

	DrawCameraRotationRing(TrackButton, TrackTool, WorldMatrix, OverlayScale);

	DrawAxisRotationRings(TrackButton, TrackTool, HasAngle, WorldMatrix, OverlayScale);

	DrawRotationText(TrackButton, MouseToolDistance, RotationDisc, WorldMatrix, OverlayScale);

	Context->EnableDepthTest(true);
}

bool swViewManipulator::IsTrackToolAllowed(swTrackTool TrackTool, quint32 AllowedTransforms)
{
	switch (TrackTool)
	{
		case swTrackTool::None:
		case swTrackTool::Insert:
		case swTrackTool::PointLight:
		case swTrackTool::SpotLight:
		case swTrackTool::DirectionalLight:
		case swTrackTool::AreaLight:
		case swTrackTool::Camera:
		case swTrackTool::Select:
			return true;

		case swTrackTool::MoveX:
			return AllowedTransforms & SW_OBJECT_TRANSFORM_MOVE_X;

		case swTrackTool::MoveY:
			return AllowedTransforms & SW_OBJECT_TRANSFORM_MOVE_Y;

		case swTrackTool::MoveZ:
			return AllowedTransforms & SW_OBJECT_TRANSFORM_MOVE_Z;

		case swTrackTool::MoveXY:
			return (AllowedTransforms & (SW_OBJECT_TRANSFORM_MOVE_X | SW_OBJECT_TRANSFORM_MOVE_Y)) == (SW_OBJECT_TRANSFORM_MOVE_X | SW_OBJECT_TRANSFORM_MOVE_Y);

		case swTrackTool::MoveXZ:
			return (AllowedTransforms & (SW_OBJECT_TRANSFORM_MOVE_X | SW_OBJECT_TRANSFORM_MOVE_Z)) == (SW_OBJECT_TRANSFORM_MOVE_X | SW_OBJECT_TRANSFORM_MOVE_Z);

		case swTrackTool::MoveYZ:
			return (AllowedTransforms & (SW_OBJECT_TRANSFORM_MOVE_Y | SW_OBJECT_TRANSFORM_MOVE_Z)) == (SW_OBJECT_TRANSFORM_MOVE_Y | SW_OBJECT_TRANSFORM_MOVE_Z);

		case swTrackTool::MoveXYZ:
			return (AllowedTransforms & (SW_OBJECT_TRANSFORM_MOVE_X | SW_OBJECT_TRANSFORM_MOVE_Y | SW_OBJECT_TRANSFORM_MOVE_Z)) == (SW_OBJECT_TRANSFORM_MOVE_X | SW_OBJECT_TRANSFORM_MOVE_Y | SW_OBJECT_TRANSFORM_MOVE_Z);

		case swTrackTool::RotateX:
			return AllowedTransforms & SW_OBJECT_TRANSFORM_ROTATE_X;

		case swTrackTool::RotateY:
			return AllowedTransforms & SW_OBJECT_TRANSFORM_ROTATE_Y;

		case swTrackTool::RotateZ:
			return AllowedTransforms & SW_OBJECT_TRANSFORM_ROTATE_Z;

		case swTrackTool::RotateTrackBall:
			return (AllowedTransforms & (SW_OBJECT_TRANSFORM_ROTATE_X | SW_OBJECT_TRANSFORM_ROTATE_Y | SW_OBJECT_TRANSFORM_ROTATE_Z)) == (SW_OBJECT_TRANSFORM_ROTATE_X | SW_OBJECT_TRANSFORM_ROTATE_Y | SW_OBJECT_TRANSFORM_ROTATE_Z);

		case swTrackTool::RotateCamera:
			return (AllowedTransforms & (SW_OBJECT_TRANSFORM_ROTATE_X | SW_OBJECT_TRANSFORM_ROTATE_Y | SW_OBJECT_TRANSFORM_ROTATE_Z)) == (SW_OBJECT_TRANSFORM_ROTATE_X | SW_OBJECT_TRANSFORM_ROTATE_Y | SW_OBJECT_TRANSFORM_ROTATE_Z);

		case swTrackTool::RotateTrainTrackRight:
		case swTrackTool::RotateTrainTrackLeft:
		case swTrackTool::InsertTrainTrack:
		case swTrackTool::SelectTrainTrack:
			return true;

		case swTrackTool::ScalePlus:
		case swTrackTool::ScaleMinus:
			return AllowedTransforms & (SW_OBJECT_TRANSFORM_SCALE_X | SW_OBJECT_TRANSFORM_SCALE_Y | SW_OBJECT_TRANSFORM_SCALE_Z);

		case swTrackTool::Eraser:
		case swTrackTool::Paint:
		case swTrackTool::ColorPicker:
		case swTrackTool::Zoom:
		case swTrackTool::Pan:
		case swTrackTool::OrbitX:
		case swTrackTool::OrbitY:
		case swTrackTool::OrbitXY:
		case swTrackTool::Roll:
		case swTrackTool::ZoomRegion:
			return true;

		case swTrackTool::Count:
			return false;
	}

	return false;
}

std::pair<swTrackTool, quint32> swViewManipulator::UpdateSelectMove(swTrackButton TrackButton)
{
	swModel* ActiveModel = mView->GetActiveModel();
	const float OverlayScale = mView->GetOverlayScale();
	const float OverlayMovePlaneSize = 0.5f * OverlayScale;
	const float OverlayMoveArrowSize = 1.5f * OverlayScale;
	const float OverlayMoveArrowCapRadius = 0.1f * OverlayScale;
	const float OverlayRotateArrowStart = 1.0f * OverlayScale;
	const float OverlayRotateArrowEnd = 1.5f * OverlayScale;
	const float OverlayScaleRadius = 0.125f;

	swTool CurrentTool = gMainWindow->GetTool();
	swTrackTool NewTrackTool = (CurrentTool == swTool::Move) ? swTrackTool::MoveXYZ : swTrackTool::Select;
	quint32 NewTrackSection = ~0U;

	swVector3 OverlayCenter;
	swMatrix33 RelativeRotation;

	if (!ActiveModel->GetMoveRotateTransform(OverlayCenter, RelativeRotation))
		return { NewTrackTool, NewTrackSection };

	swMatrix44 WorldMatrix = swMatrix44(RelativeRotation, OverlayCenter);

	if (ActiveModel != mView->GetModel())
		WorldMatrix = swMultiply(WorldMatrix, mView->GetActiveSubmodelTransform());
	OverlayCenter = WorldMatrix.GetTranslation();

	swVector3 PlaneNormals[3] =
	{
		swVector3(1.0f, 0.0f, 0.0f),
		swVector3(0.0f, 1.0f, 0.0f),
		swVector3(0.0f, 0.0f, 1.0f),
	};

	for (int i = 0; i < 3; i++)
		PlaneNormals[i] = swMultiply30(PlaneNormals[i], WorldMatrix);

	const int x = mView->GetMouseX();
	const int y = mView->GetMouseY();
	swVector3 StartEnd[2] = { swVector3((float)x, (float)y, 0.0f), swVector3((float)x, (float)y, 1.0f) };
	mView->UnprojectPoints(StartEnd, 2);
	const swVector3& Start = StartEnd[0];
	const swVector3& End = StartEnd[1];
	float ClosestIntersectionDistance = FLT_MAX;

	swObject* Focus = ActiveModel->GetFocusObject();
	int ControlPointIndex = -1;

	if (Focus && Focus->IsPiece())
	{
		swPiece* Piece = (swPiece*)Focus;

		if (Piece->mPieceData->GetSynthInfo())
		{
			quint32 Section = Piece->GetFocusSection();

			if (Section >= SW_PIECE_SECTION_CONTROL_POINT_FIRST && Section <= SW_PIECE_SECTION_CONTROL_POINT_LAST)
				ControlPointIndex = Section - SW_PIECE_SECTION_CONTROL_POINT_FIRST;
		}
	}

	quint32 AllowedTransforms = Focus ? Focus->GetAllowedTransforms() : SW_OBJECT_TRANSFORM_MOVE_XYZ | SW_OBJECT_TRANSFORM_ROTATE_XYZ;

	for (int AxisIndex = 0; AxisIndex < 3; AxisIndex++)
	{
		swVector4 Plane(PlaneNormals[AxisIndex], -swDot(PlaneNormals[AxisIndex], OverlayCenter));
		swVector3 Intersection;

		if (!swLineSegmentPlaneIntersection(&Intersection, Start, End, Plane))
			continue;

		float IntersectionDistance = swLengthSquared(Intersection - Start);

		if (IntersectionDistance > ClosestIntersectionDistance)
			continue;

		swVector3 Dir(Intersection - OverlayCenter);

		float Proj1 = swDot(Dir, PlaneNormals[(AxisIndex + 1) % 3]);
		float Proj2 = swDot(Dir, PlaneNormals[(AxisIndex + 2) % 3]);

		if (Proj1 > 0.0f && Proj1 < OverlayMovePlaneSize && Proj2 > 0.0f && Proj2 < OverlayMovePlaneSize)
		{
			swTrackTool PlaneModes[] = { swTrackTool::MoveYZ, swTrackTool::MoveXZ, swTrackTool::MoveXY };

			if (IsTrackToolAllowed(PlaneModes[AxisIndex], AllowedTransforms))
			{
				NewTrackTool = PlaneModes[AxisIndex];
				ClosestIntersectionDistance = IntersectionDistance;
			}
		}

		if (CurrentTool == swTool::Select && Proj1 > OverlayRotateArrowStart && Proj1 < OverlayRotateArrowEnd && Proj2 > OverlayRotateArrowStart && Proj2 < OverlayRotateArrowEnd && ActiveModel->AnyPiecesSelected())
		{
			swTrackTool PlaneModes[] = { swTrackTool::RotateX, swTrackTool::RotateY, swTrackTool::RotateZ };

			if (IsTrackToolAllowed(PlaneModes[AxisIndex], AllowedTransforms))
			{
				NewTrackTool = PlaneModes[AxisIndex];
				ClosestIntersectionDistance = IntersectionDistance;
			}
		}

		if (fabs(Proj1) < OverlayMoveArrowCapRadius && Proj2 > 0.0f && Proj2 < OverlayMoveArrowSize)
		{
			swTrackTool DirModes[] = { swTrackTool::MoveZ, swTrackTool::MoveX, swTrackTool::MoveY };

			if (IsTrackToolAllowed(DirModes[AxisIndex], AllowedTransforms))
			{
				NewTrackTool = DirModes[AxisIndex];
				ClosestIntersectionDistance = IntersectionDistance;
			}
		}

		if (fabs(Proj2) < OverlayMoveArrowCapRadius && Proj1 > 0.0f && Proj1 < OverlayMoveArrowSize)
		{
			swTrackTool DirModes[] = { swTrackTool::MoveY, swTrackTool::MoveZ, swTrackTool::MoveX };

			if (IsTrackToolAllowed(DirModes[AxisIndex], AllowedTransforms))
			{
				NewTrackTool = DirModes[AxisIndex];
				ClosestIntersectionDistance = IntersectionDistance;
			}
		}

		swPiece* Piece = (swPiece*)Focus;

		if (ControlPointIndex != -1 && Piece->mPieceData->GetSynthInfo()->IsCurve())
		{
			float Strength = Piece->GetControlPoints()[ControlPointIndex].Scale;
			const float ScaleStart = (2.0f - OverlayScaleRadius) * OverlayScale + Strength;
			const float ScaleEnd = (2.0f + OverlayScaleRadius) * OverlayScale + Strength;

			if (AxisIndex == 1 && fabs(Proj1) < OverlayScaleRadius * OverlayScale)
			{
				if (Proj2 > ScaleStart && Proj2 < ScaleEnd)
				{
					if (IsTrackToolAllowed(swTrackTool::ScalePlus, AllowedTransforms))
					{
						NewTrackTool = swTrackTool::ScalePlus;
						ClosestIntersectionDistance = IntersectionDistance;
					}
				}
				else if (Proj2 < -ScaleStart && Proj2 > -ScaleEnd)
				{
					if (IsTrackToolAllowed(swTrackTool::ScaleMinus, AllowedTransforms))
					{
						NewTrackTool = swTrackTool::ScaleMinus;
						ClosestIntersectionDistance = IntersectionDistance;
					}
				}
			}
			else if (AxisIndex == 2 && fabs(Proj2) < OverlayScaleRadius * OverlayScale)
			{
				if (Proj1 > ScaleStart && Proj1 < ScaleEnd)
				{
					if (IsTrackToolAllowed(swTrackTool::ScalePlus, AllowedTransforms))
					{
						NewTrackTool = swTrackTool::ScalePlus;
						ClosestIntersectionDistance = IntersectionDistance;
					}
				}
				else if (Proj1 < -ScaleStart && Proj1 > -ScaleEnd)
				{
					if (IsTrackToolAllowed(swTrackTool::ScaleMinus, AllowedTransforms))
					{
						NewTrackTool = swTrackTool::ScaleMinus;
						ClosestIntersectionDistance = IntersectionDistance;
					}
				}
			}
		}
	}

	if (CurrentTool == swTool::Select && Focus && Focus->IsPiece() && TrackButton == swTrackButton::None)
	{
		auto [TrainTrackTool, TrainTrackSection, TrainDistance] = UpdateSelectMoveTrainTrack((swPiece*)Focus, OverlayScale, Start, End);

		if (TrainDistance < ClosestIntersectionDistance)
		{
			NewTrackTool = TrainTrackTool;
			NewTrackSection = TrainTrackSection;
		}
	}

	return { NewTrackTool, NewTrackSection };
}

std::tuple<swTrackTool, quint32, float> swViewManipulator::UpdateSelectMoveTrainTrack(swPiece* Piece, float OverlayScale, const swVector3& Start, const swVector3& End) const
{
	swTrackTool NewTrackTool = swTrackTool::Select;
	quint32 NewTrackSection = ~0U;
	float ClosestIntersectionDistance = FLT_MAX;

	const swTrainTrackInformation* TrainTrackInfo = Piece->mPieceData->GetTrainTrackInfo();

	if (!TrainTrackInfo)
		return { NewTrackTool, NewTrackSection, ClosestIntersectionDistance };

	const std::vector<swTrainTrackConnection>& Connections = TrainTrackInfo->GetConnections();

	auto MinIntersectDist=[&ClosestIntersectionDistance](int FirstIndex, int IndexCount, const swVector3& LocalStart, const swVector3& LocalEnd)
	{
		swVector3 Intersection;
		bool Hit = false;

		for (int Index = FirstIndex; Index < FirstIndex + IndexCount; Index += 3)
		{
			const swVector3& v1 = mRotateMoveVertices[mRotateMoveIndices[Index]];
			const swVector3& v2 = mRotateMoveVertices[mRotateMoveIndices[Index + 1]];
			const swVector3& v3 = mRotateMoveVertices[mRotateMoveIndices[Index + 2]];

			if (swLineTriangleMinimumIntersection(v1, v2, v3, LocalStart, LocalEnd, &ClosestIntersectionDistance, &Intersection))
				Hit = true;
		}

		return Hit;
	};

	for (quint32 ConnectionIndex = 0; ConnectionIndex < Connections.size(); ConnectionIndex++)
	{
		swMatrix44 WorldMatrix = swMultiply(Connections[ConnectionIndex].Transform, Piece->mModelWorld);
		swModel* ActiveModel = mView->GetActiveModel();

		if (ActiveModel != mView->GetModel())
			WorldMatrix = swMultiply(WorldMatrix, mView->GetActiveSubmodelTransform());

		swMatrix44 InverseWorldMatrix = swMatrix44AffineInverse(WorldMatrix);
		InverseWorldMatrix = swMultiply(InverseWorldMatrix, swMatrix44Scale(swVector3(1.0f / OverlayScale, 1.0f / OverlayScale, 1.0f / OverlayScale)));

		swVector3 LocalStart = swMultiply31(Start, InverseWorldMatrix);
		swVector3 LocalEnd = swMultiply31(End, InverseWorldMatrix);

		if (!Piece->IsTrainTrackConnected(ConnectionIndex))
		{
			if (MinIntersectDist(mTrainTrackInsertIndexStart, mTrainTrackInsertIndexCount, LocalStart, LocalEnd))
			{
				NewTrackTool = swTrackTool::InsertTrainTrack;
				NewTrackSection = SW_PIECE_SECTION_TRAIN_TRACK_CONNECTION_FIRST + ConnectionIndex;
			}
		}

		if (Piece->GetFocusSection() >= SW_PIECE_SECTION_TRAIN_TRACK_CONNECTION_FIRST && ConnectionIndex == Piece->GetFocusSection() - SW_PIECE_SECTION_TRAIN_TRACK_CONNECTION_FIRST)
		{
			if (MinIntersectDist(mTrainTrackRotateIndexStart, mTrainTrackRotateIndexCount, LocalStart, LocalEnd))
			{
				NewTrackTool = swTrackTool::RotateTrainTrackRight;
				NewTrackSection = SW_PIECE_SECTION_TRAIN_TRACK_CONNECTION_FIRST + ConnectionIndex;
			}

			if (MinIntersectDist(mTrainTrackRotateIndexStart + mTrainTrackRotateIndexCount, mTrainTrackRotateIndexCount, LocalStart, LocalEnd))
			{
				NewTrackTool = swTrackTool::RotateTrainTrackLeft;
				NewTrackSection = SW_PIECE_SECTION_TRAIN_TRACK_CONNECTION_FIRST + ConnectionIndex;
			}
		}

		if (MinIntersectDist(mTrainTrackConnectionIndexStart, mTrainTrackConnectionIndexCount, LocalStart, LocalEnd))
		{
			NewTrackTool = swTrackTool::SelectTrainTrack;
			NewTrackSection = SW_PIECE_SECTION_TRAIN_TRACK_CONNECTION_FIRST + ConnectionIndex;
		}
	}

	return { NewTrackTool, NewTrackSection, ClosestIntersectionDistance };
}

swTrackTool swViewManipulator::UpdateRotate()
{
	const float OverlayScale = mView->GetOverlayScale();
	swMatrix44 WorldMatrix;
	if (!GetRotationWorldMatrix(WorldMatrix))
		return swTrackTool::None;
	const swVector3 OverlayCenter = WorldMatrix.GetTranslation();

	const int x = mView->GetMouseX();
	const int y = mView->GetMouseY();
	swVector3 StartEnd[2] = { swVector3((float)x, (float)y, 0.0f), swVector3((float)x, (float)y, 1.0f) };
	mView->UnprojectPoints(StartEnd, 2);

	swVector3 Intersection;
	const swMatrix44 InverseWorldMatrix = swMatrix44AffineInverse(WorldMatrix);
	const swVector3 LocalStart = swMultiply31(StartEnd[0], InverseWorldMatrix);
	const swVector3 LocalEnd = swMultiply31(StartEnd[1], InverseWorldMatrix);
	swVector3 LocalIntersection;
	const bool TrackballHit = swSphereRayIntersection(swVector3(0.0f, 0.0f, 0.0f), mOverlayRotateRadius * OverlayScale, LocalStart, LocalEnd, LocalIntersection);
	if (TrackballHit)
	{
		const float Epsilon = 0.25f * OverlayScale;
		const swTrackTool Axis = GetRotationAxis(LocalIntersection, Epsilon);
		if (Axis != swTrackTool::None)
			return Axis;
	}

	// Test the camera-facing ring itself, rather than its bounding sphere, so the
	// inner gizmo does not accidentally activate this mode.
	const swCamera* Camera = mView->GetCamera();
	const swVector3 CameraAxis = swNormalize(Camera->mTargetPosition - Camera->mPosition);
	const swVector4 CameraPlane(CameraAxis, -swDot(CameraAxis, OverlayCenter));

	if (swLineSegmentPlaneIntersection(&Intersection, StartEnd[0], StartEnd[1], CameraPlane))
	{
		const float RingRadius = mOverlayRotateCameraRadius * OverlayScale;
		const float HalfWidth = OverlayScale * 0.075f;
		const float Distance = swLength(Intersection - OverlayCenter);

		if (fabsf(Distance - RingRadius) <= HalfWidth)
			return swTrackTool::RotateCamera;
	}

	return TrackballHit ? swTrackTool::RotateTrackBall : swTrackTool::None;
}
