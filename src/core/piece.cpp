#include "sw_global.h"
#include "sw_mesh.h"
#include "sw_colors.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "sw_piecedata.h"
#include "piece.h"
#include "group.h"
#include "sw_file.h"
#include "sw_application.h"
#include "sw_library.h"
#include "sw_context.h"
#include "sw_scene.h"
#include "sw_utils.h"
#include "sw_synth.h"
#include "sw_traintrack.h"
#include "sw_string.h"
#include "sw_model.h"

constexpr float SW_PIECE_CONTROL_POINT_SIZE = 10.0f;

swPiece::swPiece()
    : swObject(swObjectType::Piece)
{
}

swPiece::swPiece(swPieceData* PieceData)
	: swPiece(swPieceDataRef(PieceData))
{
}

swPiece::swPiece(swPieceDataRef PieceDataRef)
	: swObject(swObjectType::Piece)
{
	SetPieceData(std::move(PieceDataRef), QString(), true, true);
	mColorIndex = gDefaultColor;
	mColorCode = 16;
	mPivotMatrix = swMatrix44Identity();
}

swPiece::swPiece(const swPiece& Other)
	: swObject(swObjectType::Piece)
{
	SetPieceData(Other.mPieceData, Other.mID, true, false);
	mHidden = Other.mHidden;
	mSelected = Other.mSelected;
	mColorIndex = Other.mColorIndex;
	mColorCode = Other.mColorCode;
	mStepShow = Other.mStepShow;
	mStepHide = Other.mStepHide;
	mGroup = Other.mGroup;

	mPivotMatrix = Other.mPivotMatrix;
	mPivotPointValid = Other.mPivotPointValid;

	mPosition = Other.mPosition;
	mRotation = Other.mRotation;
	mControlPoints = Other.mControlPoints;

	UpdateMesh();
}

swPiece::~swPiece()
{
	swGetPiecesLibrary()->CancelSynthMesh(this);

	if (mPieceData)
	{
		swPiecesLibrary* Library = swGetPiecesLibrary();
		mPieceData.Reset();
		Library->NotifyAssetStatusChanged();
	}

	delete mMesh;
}

void swPiece::SetPieceData(swPieceDataRef PieceDataRef, const QString& ID, bool Wait, bool UpdateSynthInfo)
{
	swPiecesLibrary* Library = swGetPiecesLibrary();
	const bool DataChanged = mPieceData != PieceDataRef;

	if (DataChanged || UpdateSynthInfo)
		Library->CancelSynthMesh(this);

	if (DataChanged)
	{
		delete mMesh;
		mMesh = nullptr;
	}

	swPieceDataRef PreviousData(std::move(mPieceData));
	mPieceData = std::move(PieceDataRef);
	if (mPieceData)
	{
		if (Wait)
			Library->WaitForPieceData(mPieceData);
		else
			Library->RequestPieceDataLoad(mPieceData.Get(), swLoadPriority::Normal);
	}

	if (!ID.isEmpty())
		mID = ID;
	else if (mPieceData)
		mID = mPieceData->mFileName;
	else
		mID.clear();

	if (UpdateSynthInfo)
	{
		mControlPoints.clear();

		const swSynthInfo* SynthInfo = mPieceData ? mPieceData->GetSynthInfo() : nullptr;

		if (SynthInfo)
			SynthInfo->GetDefaultControlPoints(mControlPoints);

		UpdateMesh();
	}
	else if (PreviousData && DataChanged)
		UpdateMesh();

	PreviousData.Reset();

	Library->NotifyAssetStatusChanged();
}

bool swPiece::SetPieceId(swPieceData* PieceData)
{
	if (mPieceData == PieceData)
		return false;

	SetPieceData(swPieceDataRef(PieceData), QString(), true, true);

	return true;
}

void swPiece::UpdateID()
{
	mID = mPieceData->mFileName;
}

void swPiece::SaveLDraw(QTextStream& Stream) const
{
	const QLatin1String LineEnding("\r\n");

	if (mStepHide != SW_STEP_MAX)
		Stream << QLatin1String("0 !LEOCAD PIECE STEP_HIDE ") << mStepHide << LineEnding;

	if (IsHidden())
		Stream << QLatin1String("0 !LEOCAD PIECE HIDDEN") << LineEnding;

	if (mPivotPointValid)
	{
		const float* PivotMatrix = mPivotMatrix.GetFloats();
		const float PivotNumbers[12] = { PivotMatrix[12], -PivotMatrix[14], PivotMatrix[13], PivotMatrix[0], -PivotMatrix[8], PivotMatrix[4], -PivotMatrix[2], PivotMatrix[10], -PivotMatrix[6], PivotMatrix[1], -PivotMatrix[9], PivotMatrix[5] };

		Stream << QLatin1String("0 !LEOCAD PIECE PIVOT ");

		for (int NumberIdx = 0; NumberIdx < 12; NumberIdx++)
			Stream << ' ' << swFormatValue(PivotNumbers[NumberIdx], NumberIdx < 3 ? 4 : 6);

		Stream << LineEnding;
	}

	mPosition.Save(Stream, "PIECE", "POSITION", false);
	mRotation.Save(Stream, "PIECE", "ROTATION", false);

	Stream << "1 " << mColorCode << ' ';

	const float* Matrix = mModelWorld.GetFloats();
	const float Numbers[12] = { Matrix[12], -Matrix[14], Matrix[13], Matrix[0], -Matrix[8], Matrix[4], -Matrix[2], Matrix[10], -Matrix[6], Matrix[1], -Matrix[9], Matrix[5] };

	for (int NumberIdx = 0; NumberIdx < 12; NumberIdx++)
		Stream << swFormatValue(Numbers[NumberIdx], NumberIdx < 3 ? 4 : 6) << ' ';

	Stream << mID << LineEnding;
}

bool swPiece::ParseLDrawLine(QTextStream& Stream)
{
	while (!Stream.atEnd())
	{
		QString Token;
		Stream >> Token;

		if (Token == QLatin1String("STEP_HIDE"))
			Stream >> mStepHide;
		else if (Token == QLatin1String("HIDDEN"))
			SetHidden(true);
		else if (Token == QLatin1String("PIVOT"))
		{
			float PivotNumbers[12];
			for (int TokenIdx = 0; TokenIdx < 12; TokenIdx++)
				Stream >> PivotNumbers[TokenIdx];

			const swMatrix44 PivotMatrix(swVector4( PivotNumbers[3],  PivotNumbers[9], -PivotNumbers[6], 0.0f), swVector4(PivotNumbers[5], PivotNumbers[11], -PivotNumbers[8], 0.0f),
								         swVector4(-PivotNumbers[4], -PivotNumbers[10], PivotNumbers[7], 0.0f), swVector4(PivotNumbers[0], PivotNumbers[2],  -PivotNumbers[1], 1.0f));

			mPivotMatrix = PivotMatrix;
			mPivotPointValid = true;
		}
		else if (mPosition.Load(Stream, Token, "POSITION"))
			continue;
		else if (mRotation.Load(Stream, Token, "ROTATION"))
			continue;
	}

	return false;
}

bool swPiece::FileLoad(swFile& file, Project* Project)
{
	quint8 version, ch;

	version = file.ReadU8();

	if (version > 12) // LeoCAD 0.80
		return false;

	const float PositionScale = (version < 12) ? 25.0f : 1.0f;

	if (version > 8)
	{
		if (file.ReadU8() != 1)
			return false;

		quint16 time;
		float param[4];
		quint8 type;
		quint32 n;

		file.ReadU32(&n, 1);
		while (n--)
		{
			file.ReadU16(&time, 1);
			file.ReadFloats(param, 4);
			file.ReadU8(&type, 1);

			if (type == 0)
				mPosition.ChangeKey(swVector3(param[0], param[1], param[2]) * PositionScale, time, true);
			else if (type == 1)
				mRotation.ChangeKey(swMatrix33FromAxisAngle(swVector3(param[0], param[1], param[2]), param[3] * SW_DTOR), time, true);
		}

		file.ReadU32(&n, 1);
		while (n--)
		{
			file.ReadU16(&time, 1);
			file.ReadFloats(param, 4);
			file.ReadU8(&type, 1);
		}
	}

	if (version < 9)
	{
		quint16 time;
		quint8 type;

		if (version > 5)
		{
			quint32 keys;
			float param[4];

			file.ReadU32(&keys, 1);
			while (keys--)
			{
				file.ReadFloats(param, 4);
				file.ReadU16(&time, 1);
				file.ReadU8(&type, 1);

				if (type == 0)
					mPosition.ChangeKey(swVector3(param[0], param[1], param[2]) * PositionScale, time, true);
				else if (type == 1)
					mRotation.ChangeKey(swMatrix33FromAxisAngle(swVector3(param[0], param[1], param[2]), param[3] * SW_DTOR), time, true);
			}

			file.ReadU32(&keys, 1);
			while (keys--)
			{
				file.ReadFloats(param, 4);
				file.ReadU16(&time, 1);
				file.ReadU8(&type, 1);
			}
		}
		else
		{
			if (version > 2)
			{
				file.ReadU8(&ch, 1);

				while (ch--)
				{
					swMatrix44 ModelWorld;

					if (version > 3)
					{
						file.ReadFloats(ModelWorld.GetFloats(), 16);
					}
					else
					{
						swVector3 Translation;
						float Rotation[3];
						file.ReadFloats(Translation.GetFloats(), 3);
						file.ReadFloats(Rotation, 3);
						ModelWorld = swMatrix44Translation(Translation);
						ModelWorld = swMul(swMatrix44RotationZ(Rotation[2] * SW_DTOR), swMul(swMatrix44RotationY(Rotation[1] * SW_DTOR), swMul(swMatrix44RotationX(Rotation[0] * SW_DTOR), ModelWorld)));
					}

					quint8 b;
					file.ReadU8(&b, 1);
					time = b;

					mPosition.ChangeKey(ModelWorld.GetTranslation() * PositionScale, 1, true);
					mRotation.ChangeKey(swMatrix33(ModelWorld), time, true);

					qint32 bl;
					file.ReadS32(&bl, 1);
				}
			}
			else
			{
				swVector3 Translation;
				float Rotation[3];
				file.ReadFloats(Translation.GetFloats(), 3);
				file.ReadFloats(Rotation, 3);
				swMatrix44 ModelWorld = swMatrix44Translation(Translation);
				ModelWorld = swMul(swMatrix44RotationZ(Rotation[2] * SW_DTOR), swMul(swMatrix44RotationY(Rotation[1] * SW_DTOR), swMul(swMatrix44RotationX(Rotation[0] * SW_DTOR), ModelWorld)));

				mPosition.ChangeKey(swVector3(ModelWorld.r[3][0], ModelWorld.r[3][1], ModelWorld.r[3][2]) * PositionScale, 1, true);
				mRotation.ChangeKey(swMatrix33(ModelWorld), 1, true);
			}
		}
	}

	// Common to all versions.
	char name[SW_PIECE_NAME_LEN];
	if (version < 10)
	{
		memset(name, 0, SW_PIECE_NAME_LEN);
		file.ReadBuffer(name, 9);
	}
	else
		file.ReadBuffer(name, SW_PIECE_NAME_LEN);
	swstrcat(name, ".dat");

	SetPieceData(swGetPiecesLibrary()->AcquirePieceData(name, Project, true, false), QString(), true, true);

	// 11 (0.77)
	if (version < 11)
	{
		quint8 Color;

		file.ReadU8(&Color, 1);

		if (version < 5)
			mColorCode = swGetColorCodeFromOriginalColor(Color);
		else
			mColorCode = swGetColorCodeFromExtendedColor(Color);
	}
	else
		file.ReadU32(&mColorCode, 1);
	mColorIndex = swGetColorIndex(mColorCode);

	quint8 Step;
	file.ReadU8(&Step, 1);
	mStepShow = Step;
	if (version > 1)
	{
		file.ReadU8(&Step, 1);
		mStepHide = Step == 255 ? SW_STEP_MAX : Step;
	}
	else
		mStepHide = SW_STEP_MAX;

	if (version > 5)
	{
		file.ReadU16(); // m_nFrameShow
		file.ReadU16(); // m_nFrameHide

		if (version > 7)
		{
			quint8 Hidden;
			file.ReadU8(&Hidden, 1);
			if (Hidden & 1)
				mHidden = true;
			file.ReadU8(&ch, 1);
			file.Seek(ch, SEEK_CUR);
		}
		else
		{
			qint32 hide;
			file.ReadS32(&hide, 1);
			if (hide != 0)
				mHidden = true;
			file.Seek(81, SEEK_CUR);
		}

		// 7 (0.64)
		qint32 i = -1;
		if (version > 6)
			file.ReadS32(&i, 1);
		mGroup = (swGroup*)(quintptr)i;
	}
	else
	{
		file.ReadU8(&ch, 1);
		if (ch == 0)
			mGroup = (swGroup*)-1;
		else
			mGroup = (swGroup*)(quintptr)ch;

		file.ReadU8(&ch, 1);
		if (ch & 0x01)
			mHidden = true;
	}

	return true;
}

void swPiece::Initialize(const swMatrix44& WorldMatrix, swStep Step)
{
	mStepShow = Step;

	mPosition.SetValue(WorldMatrix.GetTranslation());
	mRotation.SetValue(swMatrix33(WorldMatrix));

	UpdatePosition(Step);
}

void swPiece::InsertTime(swStep Start, swStep Time)
{
	if (mStepShow >= Start)
	{
		if (mStepShow < SW_STEP_MAX - Time)
			mStepShow += Time;
		else
			mStepShow = SW_STEP_MAX;
	}

	if (mStepHide >= Start)
	{
		if (mStepHide < SW_STEP_MAX - Time)
			mStepHide += Time;
		else
			mStepHide = SW_STEP_MAX;
	}

	if (mStepShow >= mStepHide)
	{
		if (mStepShow != SW_STEP_MAX)
			mStepHide = mStepShow + 1;
		else
		{
			mStepShow = SW_STEP_MAX - 1;
			mStepHide = SW_STEP_MAX;
		}
	}

	mPosition.InsertTime(Start, Time);
	mRotation.InsertTime(Start, Time);
}

void swPiece::RemoveTime(swStep Start, swStep Time)
{
	if (mStepShow >= Start)
	{
		if (mStepShow > Time)
			mStepShow -= Time;
		else
			mStepShow = 1;
	}

	if (mStepHide != SW_STEP_MAX)
	{
		if (mStepHide > Time)
			mStepHide -= Time;
		else
			mStepHide = 1;
	}

	if (mStepShow >= mStepHide)
	{
		if (mStepShow != SW_STEP_MAX)
			mStepHide = mStepShow + 1;
		else
		{
			mStepShow = SW_STEP_MAX - 1;
			mStepHide = SW_STEP_MAX;
		}
	}

	mPosition.RemoveTime(Start, Time);
	mRotation.RemoveTime(Start, Time);
}

void swPiece::RayTest(swObjectRayTest& ObjectRayTest) const
{
	const swMatrix44 InverseWorldMatrix = swMatrix44AffineInverse(mModelWorld);
	const swVector3 Start = swMul31(ObjectRayTest.Start, InverseWorldMatrix);
	const swVector3 End = swMul31(ObjectRayTest.End, InverseWorldMatrix);

	if (MinIntersectDist(Start, End, ObjectRayTest.Distance, ObjectRayTest.PieceRayTest))
	{
		ObjectRayTest.ObjectSection.Object = const_cast<swPiece*>(this);
		ObjectRayTest.ObjectSection.Section = SW_PIECE_SECTION_POSITION;
		ObjectRayTest.PieceRayTest.Transform = swMul(ObjectRayTest.PieceRayTest.Transform, mModelWorld);
	}

	if (mPieceData->GetSynthInfo() && AreControlPointsVisible())
	{
		const swVector3 Min(-SW_PIECE_CONTROL_POINT_SIZE, -SW_PIECE_CONTROL_POINT_SIZE, -SW_PIECE_CONTROL_POINT_SIZE);
		const swVector3 Max(SW_PIECE_CONTROL_POINT_SIZE, SW_PIECE_CONTROL_POINT_SIZE, SW_PIECE_CONTROL_POINT_SIZE);

		for (quint32 ControlPointIndex = 0; ControlPointIndex < mControlPoints.size(); ControlPointIndex++)
		{
			const swMatrix44 InverseTransform = swMatrix44AffineInverse(mControlPoints[ControlPointIndex].Transform);
			const swVector3 PointStart = swMul31(Start, InverseTransform);
			const swVector3 PointEnd = swMul31(End, InverseTransform);

			float Distance;
			swVector3 Plane;

			if (swBoundingBoxRayIntersectDistance(Min, Max, PointStart, PointEnd, &Distance, nullptr, &Plane))
			{
				ObjectRayTest.ObjectSection.Object = const_cast<swPiece*>(this);
				ObjectRayTest.ObjectSection.Section = SW_PIECE_SECTION_CONTROL_POINT_FIRST + ControlPointIndex;
				ObjectRayTest.Distance = Distance;
				ObjectRayTest.PieceRayTest.Plane = Plane;
			}
		}
	}
}

bool swPiece::MinIntersectDist(const swVector3& Start, const swVector3& End, float& MinDistance, swPieceRayTest& PieceRayTest) const
{
	swMesh* Mesh = GetDisplayMesh();

	if (!Mesh || mPieceData->IsLoading())
		return mPieceData->MinIntersectDist(Start, End, MinDistance, PieceRayTest);

	if (!Mesh->MinIntersectDist(Start, End, MinDistance, PieceRayTest.Plane, PieceRayTest.Normal))
		return false;

	PieceRayTest.PieceData = mPieceData.Get();
	PieceRayTest.Transform = swMatrix44Identity();

	return true;
}

void swPiece::BoxTest(swObjectBoxTest& ObjectBoxTest) const
{
	if (IntersectsPlanes(mModelWorld, ObjectBoxTest.Planes))
		ObjectBoxTest.Objects.emplace_back(const_cast<swPiece*>(this));
}

bool swPiece::IntersectsPlanes(const swMatrix44& WorldMatrix, const swVector4 Planes[6]) const
{
	swMesh* Mesh = GetDisplayMesh();

	if (!Mesh || mPieceData->IsLoading())
		return mPieceData->BoxTest(WorldMatrix, Planes);

	const swMatrix44 InverseWorldMatrix = swMatrix44AffineInverse(WorldMatrix);
	swVector4 LocalPlanes[6];

	for (int PlaneIdx = 0; PlaneIdx < 6; PlaneIdx++)
	{
		const swVector3 PlaneNormal = swMul30(Planes[PlaneIdx], InverseWorldMatrix);
		LocalPlanes[PlaneIdx] = swVector4(PlaneNormal, Planes[PlaneIdx][3] - swDot3(InverseWorldMatrix[3], PlaneNormal));
	}

	swVector3 Corners[8];
	swGetBoxCorners(Mesh->mBoundingBox, Corners);

	int OutcodesOR = 0;
	int OutcodesAND = 0x3f;

	for (const swVector3& Corner : Corners)
	{
		int Outcode = 0;

		for (int PlaneIdx = 0; PlaneIdx < 6; PlaneIdx++)
			if (swDot3(Corner, LocalPlanes[PlaneIdx]) + LocalPlanes[PlaneIdx][3] > 0)
				Outcode |= 1 << PlaneIdx;

		OutcodesOR |= Outcode;
		OutcodesAND &= Outcode;
	}

	if (OutcodesAND != 0)
		return false;

	if (OutcodesOR == 0)
		return true;

	return Mesh->IntersectsPlanes(LocalPlanes);
}

void swPiece::DrawInterface(swContext* Context, const swScene& Scene) const
{
	const swPreferences& Preferences = swGetPreferences();
	const float LineWidth = Preferences.mLineWidth;

	Context->SetLineWidth(2.0f * LineWidth);

	const swBoundingBox& BoundingBox = GetBoundingBox();
	const swVector3& Min = BoundingBox.Min;
	const swVector3& Max = BoundingBox.Max;
	swVector3 Edge((Max - Min) * 0.33f);

	const float LineVerts[48][3] =
	{
		{ Max[0], Max[1], Max[2] }, { Max[0] - Edge[0], Max[1], Max[2] },
		{ Max[0], Max[1], Max[2] }, { Max[0], Max[1] - Edge[1], Max[2] },
		{ Max[0], Max[1], Max[2] }, { Max[0], Max[1], Max[2] - Edge[2] },

		{ Min[0], Max[1], Max[2] }, { Min[0] + Edge[0], Max[1], Max[2] },
		{ Min[0], Max[1], Max[2] }, { Min[0], Max[1] - Edge[1], Max[2] },
		{ Min[0], Max[1], Max[2] }, { Min[0], Max[1], Max[2] - Edge[2] },

		{ Max[0], Min[1], Max[2] }, { Max[0] - Edge[0], Min[1], Max[2] },
		{ Max[0], Min[1], Max[2] }, { Max[0], Min[1] + Edge[1], Max[2] },
		{ Max[0], Min[1], Max[2] }, { Max[0], Min[1], Max[2] - Edge[2] },

		{ Min[0], Min[1], Max[2] }, { Min[0] + Edge[0], Min[1], Max[2] },
		{ Min[0], Min[1], Max[2] }, { Min[0], Min[1] + Edge[1], Max[2] },
		{ Min[0], Min[1], Max[2] }, { Min[0], Min[1], Max[2] - Edge[2] },

		{ Max[0], Max[1], Min[2] }, { Max[0] - Edge[0], Max[1], Min[2] },
		{ Max[0], Max[1], Min[2] }, { Max[0], Max[1] - Edge[1], Min[2] },
		{ Max[0], Max[1], Min[2] }, { Max[0], Max[1], Min[2] + Edge[2] },

		{ Min[0], Max[1], Min[2] }, { Min[0] + Edge[0], Max[1], Min[2] },
		{ Min[0], Max[1], Min[2] }, { Min[0], Max[1] - Edge[1], Min[2] },
		{ Min[0], Max[1], Min[2] }, { Min[0], Max[1], Min[2] + Edge[2] },

		{ Max[0], Min[1], Min[2] }, { Max[0] - Edge[0], Min[1], Min[2] },
		{ Max[0], Min[1], Min[2] }, { Max[0], Min[1] + Edge[1], Min[2] },
		{ Max[0], Min[1], Min[2] }, { Max[0], Min[1], Min[2] + Edge[2] },

		{ Min[0], Min[1], Min[2] }, { Min[0] + Edge[0], Min[1], Min[2] },
		{ Min[0], Min[1], Min[2] }, { Min[0], Min[1] + Edge[1], Min[2] },
		{ Min[0], Min[1], Min[2] }, { Min[0], Min[1], Min[2] + Edge[2] },
	};

	const swMatrix44 WorldMatrix = Scene.ApplyActiveSubmodelTransform(mModelWorld);
	Context->SetMaterial(swMaterialType::UnlitColor);
	Context->SetWorldMatrix(WorldMatrix);

	if (IsFocused(SW_PIECE_SECTION_POSITION))
	{
		const swVector4 FocusedColor = swVector4FromColor(Preferences.mObjectFocusedColor);
		Context->SetColor(FocusedColor);
	}
	else
	{
		const swVector4 SelectedColor = swVector4FromColor(Preferences.mObjectSelectedColor);
		Context->SetColor(SelectedColor);
	}

	if (Scene.GetHighlightRenderMode() == swHighlightRenderMode::Tint)
	{
		Context->SetVertexBufferPointer(LineVerts);
		Context->SetVertexFormatPosition(3);

		Context->DrawPrimitives(GL_LINES, 0, 48);
	}

	if (IsPivotPointVisible())
	{
		constexpr float Size = 5.0f;
		constexpr float Verts[8 * 3] =
		{
			-Size, -Size, -Size, -Size,  Size, -Size, Size,  Size, -Size, Size, -Size, -Size,
			-Size, -Size,  Size, -Size,  Size,  Size, Size,  Size,  Size, Size, -Size,  Size
		};

		const GLushort Indices[24] =
		{
			0, 1, 1, 2, 2, 3, 3, 0, 4, 5, 5, 6, 6, 7, 7, 4, 0, 4, 1, 5, 2, 6, 3, 7
		};

		Context->SetWorldMatrix(swMul(mPivotMatrix, WorldMatrix));

		Context->SetVertexBufferPointer(Verts);
		Context->SetVertexFormatPosition(3);
		Context->SetIndexBufferPointer(Indices);

		Context->DrawIndexedPrimitives(GL_LINES, 24, GL_UNSIGNED_SHORT, 0);
	}

	if (mPieceData->GetSynthInfo())
		DrawSynthInterface(Context, WorldMatrix);
}

void swPiece::DrawSynthInterface(swContext* Context, const swMatrix44& WorldMatrix) const
{
	if (mControlPoints.empty() || !AreControlPointsVisible())
		return;

	float Verts[8 * 3];
	float* CurVert = Verts;

	swVector3 CubeMin(-SW_PIECE_CONTROL_POINT_SIZE, -SW_PIECE_CONTROL_POINT_SIZE, -SW_PIECE_CONTROL_POINT_SIZE);
	swVector3 CubeMax(SW_PIECE_CONTROL_POINT_SIZE, SW_PIECE_CONTROL_POINT_SIZE, SW_PIECE_CONTROL_POINT_SIZE);

	*CurVert++ = CubeMin[0]; *CurVert++ = CubeMin[1]; *CurVert++ = CubeMin[2];
	*CurVert++ = CubeMin[0]; *CurVert++ = CubeMax[1]; *CurVert++ = CubeMin[2];
	*CurVert++ = CubeMax[0]; *CurVert++ = CubeMax[1]; *CurVert++ = CubeMin[2];
	*CurVert++ = CubeMax[0]; *CurVert++ = CubeMin[1]; *CurVert++ = CubeMin[2];
	*CurVert++ = CubeMin[0]; *CurVert++ = CubeMin[1]; *CurVert++ = CubeMax[2];
	*CurVert++ = CubeMin[0]; *CurVert++ = CubeMax[1]; *CurVert++ = CubeMax[2];
	*CurVert++ = CubeMax[0]; *CurVert++ = CubeMax[1]; *CurVert++ = CubeMax[2];
	*CurVert++ = CubeMax[0]; *CurVert++ = CubeMin[1]; *CurVert++ = CubeMax[2];

	const GLushort Indices[36] =
	{
		0, 1, 2, 0, 2, 3, 7, 6, 5, 7, 5, 4, 5, 1, 0, 4, 5, 0,
		7, 3, 2, 6, 7, 2, 0, 3, 7, 0, 7, 4, 6, 2, 1, 5, 6, 1
	};

	Context->EnableColorBlend(true);
	Context->EnableCullFace(true);

	const swPreferences& Preferences = swGetPreferences();
	const swVector4 ControlPointColor = swVector4FromColor(Preferences.mControlPointColor);
	const swVector4 ControlPointFocusedColor = swVector4FromColor(Preferences.mControlPointFocusedColor);

	for (quint32 ControlPointIndex = 0; ControlPointIndex < mControlPoints.size(); ControlPointIndex++)
	{
		Context->SetWorldMatrix(swMul(mControlPoints[ControlPointIndex].Transform, WorldMatrix));

		Context->SetVertexBufferPointer(Verts);
		Context->SetVertexFormatPosition(3);
		Context->SetIndexBufferPointer(Indices);

		if (IsFocused(SW_PIECE_SECTION_CONTROL_POINT_FIRST + ControlPointIndex))
			Context->SetColor(ControlPointFocusedColor);
		else
			Context->SetColor(ControlPointColor);

		Context->DrawIndexedPrimitives(GL_TRIANGLES, 36, GL_UNSIGNED_SHORT, 0);
	}

	Context->EnableCullFace(false);
	Context->EnableColorBlend(false);
}

QVariant swPiece::GetPropertyValue(swObjectPropertyId PropertyId) const
{
	switch (PropertyId)
	{
	case swObjectPropertyId::PieceId:
		return QVariant::fromValue<void*>(mPieceData.Get());

	case swObjectPropertyId::PieceColor:
		return GetColorIndex();

	case swObjectPropertyId::PieceStepShow:
	case swObjectPropertyId::PieceStepHide:
	case swObjectPropertyId::CameraName:
	case swObjectPropertyId::CameraProjection:
	case swObjectPropertyId::CameraFOV:
	case swObjectPropertyId::CameraNear:
	case swObjectPropertyId::CameraFar:
	case swObjectPropertyId::CameraPositionX:
	case swObjectPropertyId::CameraPositionY:
	case swObjectPropertyId::CameraPositionZ:
	case swObjectPropertyId::CameraTargetX:
	case swObjectPropertyId::CameraTargetY:
	case swObjectPropertyId::CameraTargetZ:
	case swObjectPropertyId::CameraUpX:
	case swObjectPropertyId::CameraUpY:
	case swObjectPropertyId::CameraUpZ:
	case swObjectPropertyId::LightName:
	case swObjectPropertyId::LightType:
	case swObjectPropertyId::LightColor:
	case swObjectPropertyId::LightBlenderPower:
	case swObjectPropertyId::LightPOVRayPower:
	case swObjectPropertyId::LightCastShadow:
	case swObjectPropertyId::LightPOVRayFadeDistance:
	case swObjectPropertyId::LightPOVRayFadePower:
	case swObjectPropertyId::LightBlenderRadius:
	case swObjectPropertyId::LightBlenderAngle:
	case swObjectPropertyId::LightAreaSizeX:
	case swObjectPropertyId::LightAreaSizeY:
	case swObjectPropertyId::LightSpotConeAngle:
	case swObjectPropertyId::LightSpotPenumbraAngle:
	case swObjectPropertyId::LightPOVRaySpotTightness:
	case swObjectPropertyId::LightAreaShape:
	case swObjectPropertyId::LightPOVRayAreaGridX:
	case swObjectPropertyId::LightPOVRayAreaGridY:
	case swObjectPropertyId::ObjectPositionX:
	case swObjectPropertyId::ObjectPositionY:
	case swObjectPropertyId::ObjectPositionZ:
	case swObjectPropertyId::ObjectRotationX:
	case swObjectPropertyId::ObjectRotationY:
	case swObjectPropertyId::ObjectRotationZ:
	case swObjectPropertyId::Count:
		break;
	}

	return QVariant();
}

bool swPiece::SetPropertyValue(swObjectPropertyId PropertyId, swStep Step, bool AddKey, QVariant Value)
{
	Q_UNUSED(Step);
	Q_UNUSED(AddKey);

	switch (PropertyId)
	{
	case swObjectPropertyId::PieceId:
		return SetPieceId(static_cast<swPieceData*>(Value.value<void*>()));

	case swObjectPropertyId::PieceColor:
		return SetColorIndex(Value.toInt());

	case swObjectPropertyId::PieceStepShow:
	case swObjectPropertyId::PieceStepHide:
	case swObjectPropertyId::CameraName:
	case swObjectPropertyId::CameraProjection:
	case swObjectPropertyId::CameraFOV:
	case swObjectPropertyId::CameraNear:
	case swObjectPropertyId::CameraFar:
	case swObjectPropertyId::CameraPositionX:
	case swObjectPropertyId::CameraPositionY:
	case swObjectPropertyId::CameraPositionZ:
	case swObjectPropertyId::CameraTargetX:
	case swObjectPropertyId::CameraTargetY:
	case swObjectPropertyId::CameraTargetZ:
	case swObjectPropertyId::CameraUpX:
	case swObjectPropertyId::CameraUpY:
	case swObjectPropertyId::CameraUpZ:
	case swObjectPropertyId::LightName:
	case swObjectPropertyId::LightType:
	case swObjectPropertyId::LightColor:
	case swObjectPropertyId::LightBlenderPower:
	case swObjectPropertyId::LightPOVRayPower:
	case swObjectPropertyId::LightCastShadow:
	case swObjectPropertyId::LightPOVRayFadeDistance:
	case swObjectPropertyId::LightPOVRayFadePower:
	case swObjectPropertyId::LightBlenderRadius:
	case swObjectPropertyId::LightBlenderAngle:
	case swObjectPropertyId::LightAreaSizeX:
	case swObjectPropertyId::LightAreaSizeY:
	case swObjectPropertyId::LightSpotConeAngle:
	case swObjectPropertyId::LightSpotPenumbraAngle:
	case swObjectPropertyId::LightPOVRaySpotTightness:
	case swObjectPropertyId::LightAreaShape:
	case swObjectPropertyId::LightPOVRayAreaGridX:
	case swObjectPropertyId::LightPOVRayAreaGridY:
	case swObjectPropertyId::ObjectPositionX:
	case swObjectPropertyId::ObjectPositionY:
	case swObjectPropertyId::ObjectPositionZ:
	case swObjectPropertyId::ObjectRotationX:
	case swObjectPropertyId::ObjectRotationY:
	case swObjectPropertyId::ObjectRotationZ:
	case swObjectPropertyId::Count:
		break;
	}

	return false;
}

swStep swPiece::GetLastStep() const
{
	swStep Step = std::max({ mStepShow, mPosition.GetLastKeyFrame(), mRotation.GetLastKeyFrame() });

	if (mStepHide != SW_STEP_MAX)
		Step = swMax(Step, mStepHide);

	return Step;
}

bool swPiece::HasKeyFrame(swObjectPropertyId PropertyId, swStep Time) const
{
	switch (PropertyId)
	{
	case swObjectPropertyId::PieceId:
	case swObjectPropertyId::PieceColor:
	case swObjectPropertyId::PieceStepShow:
	case swObjectPropertyId::PieceStepHide:
	case swObjectPropertyId::CameraName:
	case swObjectPropertyId::CameraProjection:
	case swObjectPropertyId::CameraFOV:
	case swObjectPropertyId::CameraNear:
	case swObjectPropertyId::CameraFar:
	case swObjectPropertyId::CameraPositionX:
	case swObjectPropertyId::CameraPositionY:
	case swObjectPropertyId::CameraPositionZ:
	case swObjectPropertyId::CameraTargetX:
	case swObjectPropertyId::CameraTargetY:
	case swObjectPropertyId::CameraTargetZ:
	case swObjectPropertyId::CameraUpX:
	case swObjectPropertyId::CameraUpY:
	case swObjectPropertyId::CameraUpZ:
	case swObjectPropertyId::LightName:
	case swObjectPropertyId::LightType:
	case swObjectPropertyId::LightColor:
	case swObjectPropertyId::LightBlenderPower:
	case swObjectPropertyId::LightPOVRayPower:
	case swObjectPropertyId::LightCastShadow:
	case swObjectPropertyId::LightPOVRayFadeDistance:
	case swObjectPropertyId::LightPOVRayFadePower:
	case swObjectPropertyId::LightBlenderRadius:
	case swObjectPropertyId::LightBlenderAngle:
	case swObjectPropertyId::LightAreaSizeX:
	case swObjectPropertyId::LightAreaSizeY:
	case swObjectPropertyId::LightSpotConeAngle:
	case swObjectPropertyId::LightSpotPenumbraAngle:
	case swObjectPropertyId::LightPOVRaySpotTightness:
	case swObjectPropertyId::LightAreaShape:
	case swObjectPropertyId::LightPOVRayAreaGridX:
	case swObjectPropertyId::LightPOVRayAreaGridY:
		return false;

	case swObjectPropertyId::ObjectPositionX:
	case swObjectPropertyId::ObjectPositionY:
	case swObjectPropertyId::ObjectPositionZ:
		return mPosition.HasKeyFrame(Time);

	case swObjectPropertyId::ObjectRotationX:
	case swObjectPropertyId::ObjectRotationY:
	case swObjectPropertyId::ObjectRotationZ:
		return mRotation.HasKeyFrame(Time);

	case swObjectPropertyId::Count:
		return false;
	}

	return false;
}

bool swPiece::SetKeyFrame(swObjectPropertyId PropertyId, swStep Time, bool KeyFrame)
{
	switch (PropertyId)
	{
	case swObjectPropertyId::PieceId:
	case swObjectPropertyId::PieceColor:
	case swObjectPropertyId::PieceStepShow:
	case swObjectPropertyId::PieceStepHide:
	case swObjectPropertyId::CameraName:
	case swObjectPropertyId::CameraProjection:
	case swObjectPropertyId::CameraFOV:
	case swObjectPropertyId::CameraNear:
	case swObjectPropertyId::CameraFar:
	case swObjectPropertyId::CameraPositionX:
	case swObjectPropertyId::CameraPositionY:
	case swObjectPropertyId::CameraPositionZ:
	case swObjectPropertyId::CameraTargetX:
	case swObjectPropertyId::CameraTargetY:
	case swObjectPropertyId::CameraTargetZ:
	case swObjectPropertyId::CameraUpX:
	case swObjectPropertyId::CameraUpY:
	case swObjectPropertyId::CameraUpZ:
	case swObjectPropertyId::LightName:
	case swObjectPropertyId::LightType:
	case swObjectPropertyId::LightColor:
	case swObjectPropertyId::LightBlenderPower:
	case swObjectPropertyId::LightPOVRayPower:
	case swObjectPropertyId::LightCastShadow:
	case swObjectPropertyId::LightPOVRayFadeDistance:
	case swObjectPropertyId::LightPOVRayFadePower:
	case swObjectPropertyId::LightBlenderRadius:
	case swObjectPropertyId::LightBlenderAngle:
	case swObjectPropertyId::LightAreaSizeX:
	case swObjectPropertyId::LightAreaSizeY:
	case swObjectPropertyId::LightSpotConeAngle:
	case swObjectPropertyId::LightSpotPenumbraAngle:
	case swObjectPropertyId::LightPOVRaySpotTightness:
	case swObjectPropertyId::LightAreaShape:
	case swObjectPropertyId::LightPOVRayAreaGridX:
	case swObjectPropertyId::LightPOVRayAreaGridY:
		return false;

	case swObjectPropertyId::ObjectPositionX:
	case swObjectPropertyId::ObjectPositionY:
	case swObjectPropertyId::ObjectPositionZ:
		return mPosition.SetKeyFrame(Time, KeyFrame);

	case swObjectPropertyId::ObjectRotationX:
	case swObjectPropertyId::ObjectRotationY:
	case swObjectPropertyId::ObjectRotationZ:
		return mRotation.SetKeyFrame(Time, KeyFrame);

	case swObjectPropertyId::Count:
		return false;
	}

	return false;
}

void swPiece::RemoveKeyFrames()
{
	mPosition.RemoveAllKeys();
	mRotation.RemoveAllKeys();
}

swPieceHistoryState swPiece::GetHistoryState() const
{
	swPieceHistoryState State;

	State.Id = mId;
	State.Hidden = mHidden;
	State.FileLine = mFileLine;
	State.PieceId = mID;
	State.GroupId = mGroup ? static_cast<uint32_t>(mGroup->GetId()) : SW_GROUP_ID_INVALID;
	State.ColorIndex = mColorIndex;
	State.ColorCode = mColorCode;
	State.StepShow = mStepShow;
	State.StepHide = mStepHide;
	State.PivotMatrix = mPivotMatrix;
	State.PivotPointValid = mPivotPointValid;
	State.ControlPoints = mControlPoints;
	State.Position = mPosition;
	State.Rotation = mRotation;

	return State;
}

void swPiece::SetHistoryState(const swPieceHistoryState& State, Project* Project, swGroup* Group)
{
	mId = State.Id;
	mHidden = State.Hidden;
	mFileLine = State.FileLine;
	mID = State.PieceId;
	mGroup = Group;
	mColorIndex = State.ColorIndex;
	mColorCode = State.ColorCode;
	mStepShow = State.StepShow;
	mStepHide = State.StepHide;
	mPivotMatrix = State.PivotMatrix;
	mPivotPointValid = State.PivotPointValid;
	mControlPoints = State.ControlPoints;
	mPosition = State.Position;
	mRotation = State.Rotation;

	swPieceDataRef RestoredData = swGetPiecesLibrary()->AcquirePieceData(mID.toLatin1(), Project, true, true);

	if (mPieceData != RestoredData)
	{
		const bool NewPiece = !mPieceData;

		SetPieceData(std::move(RestoredData), mID, true, false);

		if (NewPiece)
			UpdateMesh();
	}
	else
		UpdateMesh();

	//	std::vector<bool> mTrainTrackConnections;
}

void swPiece::AddMainModelRenderMeshes(swScene* Scene, bool Highlight, bool Fade) const
{
	swRenderMeshState RenderMeshState = swRenderMeshState::Default;
	bool ParentActive = false;

	if (Highlight)
		RenderMeshState = swRenderMeshState::Highlighted;

	if (Fade)
		RenderMeshState = swRenderMeshState::Faded;

	if (Scene->GetDrawInterface())
	{
		const swPiece* ActiveSubmodelInstance = Scene->GetActiveSubmodelInstance();

		if (!ActiveSubmodelInstance)
			RenderMeshState = IsFocused() ? swRenderMeshState::Focused : (IsSelected() ? swRenderMeshState::Selected : RenderMeshState);
		else if (ActiveSubmodelInstance == this)
			ParentActive = true;
		else
			RenderMeshState = swRenderMeshState::Faded;
	}

	if (mPieceData->GetSynthInfo() && !HasGeneratedMesh() && Scene->GetRequireGeneratedMeshes())
	{
		Scene->MarkMissingAssets();
		return;
	}

	if (mSynthPending && Scene->GetRequireCompleteAssets())
		Scene->MarkMissingAssets();
	else if (swMesh* Mesh = GetDisplayMesh(); Mesh && !mPieceData->IsLoading())
		Scene->AddMesh(Mesh, mModelWorld, mColorIndex, RenderMeshState);
	else
		mPieceData->AddRenderMeshes(Scene, mModelWorld, mColorIndex, RenderMeshState, ParentActive);

	if (RenderMeshState == swRenderMeshState::Focused || RenderMeshState == swRenderMeshState::Selected)
		Scene->AddInterfaceObject(this);
}

void swPiece::AddSubModelRenderMeshes(swScene* Scene, const swMatrix44& WorldMatrix, int DefaultColorIndex, swRenderMeshState RenderMeshState, bool ParentActive) const
{
	int ColorIndex = mColorIndex;

	if (ColorIndex == gDefaultColor)
		ColorIndex = DefaultColorIndex;

	const swPiece* ActiveSubmodelInstance = Scene->GetActiveSubmodelInstance();

	if (ActiveSubmodelInstance == this)
		RenderMeshState = swRenderMeshState::Default;
	else if (ParentActive)
		RenderMeshState = IsFocused() ? swRenderMeshState::Focused : (IsSelected() ? swRenderMeshState::Selected : swRenderMeshState::Default);

	if (mPieceData->GetSynthInfo() && !HasGeneratedMesh() && Scene->GetRequireGeneratedMeshes())
	{
		Scene->MarkMissingAssets();
		return;
	}

	if (mSynthPending && Scene->GetRequireCompleteAssets())
		Scene->MarkMissingAssets();
	else if (swMesh* Mesh = GetDisplayMesh(); Mesh && !mPieceData->IsLoading())
		Scene->AddMesh(Mesh, swMul(mModelWorld, WorldMatrix), ColorIndex, RenderMeshState);
	else
		mPieceData->AddRenderMeshes(Scene, swMul(mModelWorld, WorldMatrix), ColorIndex, RenderMeshState, ActiveSubmodelInstance == this);

	if (ParentActive && (RenderMeshState == swRenderMeshState::Focused || RenderMeshState == swRenderMeshState::Selected))
		Scene->AddInterfaceObject(this);
}

void swPiece::SubModelCompareBoundingBox(const swMatrix44& WorldMatrix, swVector3& Min, swVector3& Max) const
{
	const swMatrix44 ModelWorldMatrix = swMul(mModelWorld, WorldMatrix);
	swMesh* Mesh = GetDisplayMesh();

	if (!Mesh || mPieceData->IsLoading())
	{
		mPieceData->CompareBoundingBox(ModelWorldMatrix, Min, Max);
		return;
	}

	swVector3 Points[8];
	swGetBoxCorners(Mesh->mBoundingBox, Points);

	for (const swVector3& Corner : Points)
	{
		const swVector3 Point = swMul31(Corner, ModelWorldMatrix);
		Min = swMin(Point, Min);
		Max = swMax(Point, Max);
	}
}

void swPiece::SubModelAddBoundingBoxPoints(const swMatrix44& WorldMatrix, std::vector<swVector3>& Points, swGeometryBoundsMode Mode) const
{
	if (Mode == swGeometryBoundsMode::AvailableOnly && mPieceData->GetSynthInfo() && !HasGeneratedMesh())
		return;

	swMesh* Mesh = GetDisplayMesh();

	if (!Mesh || mPieceData->IsLoading())
		mPieceData->AddSubModelBoundingBoxPoints(swMul(mModelWorld, WorldMatrix), Points, Mode);
	else
	{
		swVector3 BoxPoints[8];
		const swMatrix44 ModelWorldMatrix = swMul(mModelWorld, WorldMatrix);

		swGetBoxCorners(Mesh->mBoundingBox, BoxPoints);

		for (int i = 0; i < 8; i++)
			Points.emplace_back(swMul31(BoxPoints[i], ModelWorldMatrix));
	}
}

void swPiece::MoveSelected(swStep Step, bool AddKey, const swVector3& Distance)
{
	const quint32 Section = GetFocusSection();

	if (Section == SW_PIECE_SECTION_POSITION || Section == SW_PIECE_SECTION_INVALID)
	{
		const swVector3 Position = mModelWorld.GetTranslation() + Distance;

		SetPosition(Position, Step, AddKey);

		mModelWorld.SetTranslation(Position);
	}
	else if (Section >= SW_PIECE_SECTION_CONTROL_POINT_FIRST)
	{
		const quint32 ControlPointIndex = Section - SW_PIECE_SECTION_CONTROL_POINT_FIRST;

		if (ControlPointIndex < mControlPoints.size())
		{
			const swMatrix33 InverseWorldMatrix = swMatrix33AffineInverse(swMatrix33(mModelWorld));
			swMatrix44& Transform = mControlPoints[ControlPointIndex].Transform;

			Transform.SetTranslation(Transform.GetTranslation() + swMul(Distance, InverseWorldMatrix));
		}

		UpdateMesh();
	}
}

void swPiece::Rotate(swStep Step, bool AddKey, const swMatrix33& RotationMatrix, const swVector3& Center, const swMatrix33& RotationFrame)
{
	const quint32 Section = GetFocusSection();

	if (Section == SW_PIECE_SECTION_POSITION || Section == SW_PIECE_SECTION_INVALID)
	{
		swVector3 Distance = mModelWorld.GetTranslation() - Center;
		const swMatrix33 LocalToWorldMatrix = swMatrix33(mModelWorld);

		const swMatrix33 LocalToFocusMatrix = swMul(LocalToWorldMatrix, RotationFrame);
		swMatrix33 NewLocalToWorldMatrix = swMul(LocalToFocusMatrix, RotationMatrix);

		const swMatrix33 WorldToLocalMatrix = swMatrix33AffineInverse(LocalToWorldMatrix);

		Distance = swMul(Distance, WorldToLocalMatrix);
		Distance = swMul(Distance, NewLocalToWorldMatrix);

		NewLocalToWorldMatrix.Orthonormalize();

		SetPosition(Center + Distance, Step, AddKey);
		SetRotation(NewLocalToWorldMatrix, Step, AddKey);
	}
	else if (Section >= SW_PIECE_SECTION_CONTROL_POINT_FIRST)
	{
		const quint32 ControlPointIndex = Section - SW_PIECE_SECTION_CONTROL_POINT_FIRST;

		if (ControlPointIndex < mControlPoints.size())
		{
			swMatrix44& Transform = mControlPoints[ControlPointIndex].Transform;
			const swMatrix33 PieceWorldMatrix(mModelWorld);
			const swMatrix33 LocalToWorldMatrix = swMul(swMatrix33(Transform), PieceWorldMatrix);

			const swMatrix33 LocalToFocusMatrix = swMul(LocalToWorldMatrix, RotationFrame);
			swMatrix33 NewLocalToWorldMatrix = swMul(swMul(LocalToFocusMatrix, RotationMatrix), swMatrix33AffineInverse(PieceWorldMatrix));

			NewLocalToWorldMatrix.Orthonormalize();
			Transform = swMatrix44(NewLocalToWorldMatrix, Transform.GetTranslation());
		}

		UpdateMesh();
	}
}

void swPiece::MovePivotPoint(const swVector3& Distance)
{
	if (!IsFocused(SW_PIECE_SECTION_POSITION))
		return;

	mPivotMatrix.SetTranslation(mPivotMatrix.GetTranslation() + swMul30(Distance, swMatrix44AffineInverse(mModelWorld)));
	mPivotPointValid = true;
}

void swPiece::RotatePivotPoint(const swMatrix33& RotationMatrix)
{
	if (!IsFocused(SW_PIECE_SECTION_POSITION))
		return;

	swMatrix33 NewPivotRotationMatrix = swMul(RotationMatrix, swMatrix33(mPivotMatrix));
	NewPivotRotationMatrix.Orthonormalize();

	mPivotMatrix = swMatrix44(NewPivotRotationMatrix, mPivotMatrix.GetTranslation());
	mPivotPointValid = true;
}

quint32 swPiece::GetAllowedTransforms() const
{
	const quint32 Section = GetFocusSection();

	if (Section == SW_PIECE_SECTION_POSITION || Section == SW_PIECE_SECTION_INVALID)
		return SW_OBJECT_TRANSFORM_MOVE_XYZ | SW_OBJECT_TRANSFORM_ROTATE_XYZ;

	const swSynthInfo* SynthInfo = mPieceData->GetSynthInfo();

	if (SynthInfo)
	{
		if (SynthInfo->IsUnidirectional())
			return SW_OBJECT_TRANSFORM_MOVE_Z;

		if (SynthInfo->IsCurve())
			return SW_OBJECT_TRANSFORM_MOVE_XYZ | SW_OBJECT_TRANSFORM_ROTATE_XYZ | SW_OBJECT_TRANSFORM_SCALE_X;

		if (SynthInfo->IsNondirectional())
			return SW_OBJECT_TRANSFORM_MOVE_XYZ;
	}

	return 0;
}

swVector3 swPiece::GetSectionPosition(quint32 Section) const
{
	if (Section == SW_PIECE_SECTION_POSITION)
	{
		if (mPivotPointValid)
			return swMul(mPivotMatrix, mModelWorld).GetTranslation();
		else
			return mModelWorld.GetTranslation();
	}

	if (mPieceData->GetSynthInfo())
	{
		if (Section >= SW_PIECE_SECTION_CONTROL_POINT_FIRST)
		{
			const quint32 ControlPointIndex = Section - SW_PIECE_SECTION_CONTROL_POINT_FIRST;

			if (ControlPointIndex < mControlPoints.size())
			{
				const swMatrix44& Transform = mControlPoints[ControlPointIndex].Transform;
				return swMul(Transform, mModelWorld).GetTranslation();
			}
		}
	}

	if (mPieceData->GetTrainTrackInfo())
	{
		if (Section >= SW_PIECE_SECTION_TRAIN_TRACK_CONNECTION_FIRST)
		{
			const quint32 ConnectionIndex = Section - SW_PIECE_SECTION_TRAIN_TRACK_CONNECTION_FIRST;
			const std::vector<swTrainTrackConnection>& Connections = mPieceData->GetTrainTrackInfo()->GetConnections();

			if (ConnectionIndex < Connections.size())
			{
				const swMatrix44& Transform = Connections[ConnectionIndex].Transform;
				return swMul(Transform, mModelWorld).GetTranslation();
			}
		}
	}

	return swVector3(0.0f, 0.0f, 0.0f);
}

swVector3 swPiece::GetRotationCenter() const
{
	const quint32 Section = GetFocusSection();

	if (Section == SW_PIECE_SECTION_POSITION || Section == SW_PIECE_SECTION_INVALID)
	{
		if (mPivotPointValid)
			return swMul31(mPivotMatrix.GetTranslation(), mModelWorld);
	}
	else if (mPieceData->GetSynthInfo())
	{
		if (Section >= SW_PIECE_SECTION_CONTROL_POINT_FIRST)
		{
			const quint32 ControlPointIndex = Section - SW_PIECE_SECTION_CONTROL_POINT_FIRST;

			if (ControlPointIndex < mControlPoints.size())
			{
				const swMatrix44& Transform = mControlPoints[ControlPointIndex].Transform;
				return swMul31(Transform.GetTranslation(), mModelWorld);
			}
		}
	}
	else if (mPieceData->GetTrainTrackInfo())
	{
		if (Section >= SW_PIECE_SECTION_TRAIN_TRACK_CONNECTION_FIRST)
		{
			const quint32 ConnectionIndex = Section - SW_PIECE_SECTION_TRAIN_TRACK_CONNECTION_FIRST;
			const std::vector<swTrainTrackConnection>& Connections = mPieceData->GetTrainTrackInfo()->GetConnections();

			if (ConnectionIndex < Connections.size())
			{
				const swMatrix44& Transform = Connections[ConnectionIndex].Transform;
				return swMul(Transform, mModelWorld).GetTranslation();
			}
		}
	}

	return mModelWorld.GetTranslation();
}

swMatrix33 swPiece::GetRelativeRotation() const
{
	const quint32 Section = GetFocusSection();

	if (Section == SW_PIECE_SECTION_POSITION || Section == SW_PIECE_SECTION_INVALID)
	{
		if (mPivotPointValid)
			return swMatrix33(swMul(mModelWorld, mPivotMatrix));
		else
			return swMatrix33(mModelWorld);
	}
	else if (mPieceData->GetSynthInfo())
	{
		if (Section >= SW_PIECE_SECTION_CONTROL_POINT_FIRST)
		{
			const quint32 ControlPointIndex = Section - SW_PIECE_SECTION_CONTROL_POINT_FIRST;

			if (ControlPointIndex < mControlPoints.size())
			{
				const swMatrix44& Transform = mControlPoints[ControlPointIndex].Transform;
				return swMatrix33(swMul(Transform, mModelWorld));
			}
		}
	}
	else if (mPieceData->GetTrainTrackInfo())
	{
		if (Section >= SW_PIECE_SECTION_TRAIN_TRACK_CONNECTION_FIRST)
		{
			const quint32 ConnectionIndex = Section - SW_PIECE_SECTION_TRAIN_TRACK_CONNECTION_FIRST;
			const std::vector<swTrainTrackConnection>& Connections = mPieceData->GetTrainTrackInfo()->GetConnections();

			if (ConnectionIndex < Connections.size())
			{
				const swMatrix44& Transform = Connections[ConnectionIndex].Transform;
				return swMatrix33(swMul(Transform, mModelWorld));
			}
		}
	}

	return swMatrix33Identity();
}

bool swPiece::CanAddControlPoint() const
{
	if (mControlPoints.size() >= SW_MAX_CONTROL_POINTS)
		return false;

	const swSynthInfo* SynthInfo = mPieceData->GetSynthInfo();
	return SynthInfo && SynthInfo->CanAddControlPoints();
}

bool swPiece::CanRemoveControlPoint() const
{
	const quint32 Section = GetFocusSection();
	return Section >= SW_PIECE_SECTION_CONTROL_POINT_FIRST && Section <= SW_PIECE_SECTION_CONTROL_POINT_LAST && mControlPoints.size() > 2;
}

bool swPiece::InsertControlPoint(const swVector3& WorldStart, const swVector3& WorldEnd)
{
	if (!CanAddControlPoint())
		return false;

	const swMatrix44 InverseWorldMatrix = swMatrix44AffineInverse(mModelWorld);
	const swVector3 Start = swMul31(WorldStart, InverseWorldMatrix);
	const swVector3 End = swMul31(WorldEnd, InverseWorldMatrix);

	const swSynthInfo* SynthInfo = mPieceData->GetSynthInfo();
	const int ControlPointIndex = SynthInfo->InsertControlPoint(mControlPoints, Start, End);

	if (ControlPointIndex)
	{
		SetFocused(GetFocusSection(), false);
		SetFocused(SW_PIECE_SECTION_CONTROL_POINT_FIRST + ControlPointIndex, true);
		UpdateMesh();
		return true;
	}

	return false;
}

bool swPiece::RemoveFocusedControlPoint()
{
	quint32 Section = GetFocusSection();

	if (Section < SW_PIECE_SECTION_CONTROL_POINT_FIRST)
		return false;

	const quint32 ControlPointIndex = Section - SW_PIECE_SECTION_CONTROL_POINT_FIRST;

	if (ControlPointIndex >= mControlPoints.size() || mControlPoints.size() <= 2)
		return false;

	SetFocused(GetFocusSection(), false);
	SetFocused(SW_PIECE_SECTION_POSITION, true);
	mControlPoints.erase(mControlPoints.begin() + ControlPointIndex);

	UpdateMesh();

	return true;
}

void swPiece::VerifyControlPoints(std::vector<swPieceControlPoint>& ControlPoints) const
{
	const swSynthInfo* SynthInfo = mPieceData->GetSynthInfo();

	if (!SynthInfo)
	{
		ControlPoints.clear();
	}
	else
	{
		if (ControlPoints.size() > SW_MAX_CONTROL_POINTS)
			ControlPoints.resize(SW_MAX_CONTROL_POINTS);

		SynthInfo->VerifyControlPoints(ControlPoints);
	}
}

QString swPiece::GetName() const
{
	return QString::fromLatin1(mPieceData->m_strDescription);
}

bool swPiece::IsVisible(swStep Step) const
{
	return !mHidden && (mStepShow <= Step) && (mStepHide > Step || mStepHide == SW_STEP_MAX);
}

bool swPiece::IsVisibleInSubModel() const
{
	return (mStepHide == SW_STEP_MAX) && !mHidden;
}

void swPiece::GetModelParts(const swMatrix44& WorldMatrix, int DefaultColorIndex, std::vector<swModelPartsEntry>& ModelParts) const
{
	if (!IsVisibleInSubModel())
		return;

	int ColorIndex = mColorIndex;

	if (ColorIndex == gDefaultColor)
		ColorIndex = DefaultColorIndex;

	if (!mMesh || mPieceData->mState != swAssetState::Loaded)
		mPieceData->GetModelParts(swMul(mModelWorld, WorldMatrix), ColorIndex, ModelParts);
	else
		ModelParts.emplace_back(swModelPartsEntry{ swMul(mModelWorld, WorldMatrix), mPieceData.Get(), mMesh, ColorIndex });
}

const swBoundingBox& swPiece::GetBoundingBox() const
{
	swMesh* Mesh = GetDisplayMesh();

	if (!Mesh || mPieceData->IsLoading())
		return mPieceData->GetBoundingBox();
	else
		return Mesh->mBoundingBox;
}

void swPiece::CompareBoundingBox(swVector3& Min, swVector3& Max) const
{
	swMesh* Mesh = GetDisplayMesh();

	if (!Mesh || mPieceData->IsLoading())
		mPieceData->CompareBoundingBox(mModelWorld, Min, Max);
	else
	{
		swVector3 Points[8];

		swGetBoxCorners(Mesh->mBoundingBox, Points);

		for (int i = 0; i < 8; i++)
		{
			const swVector3 Point = swMul31(Points[i], mModelWorld);

			Min = swMin(Point, Min);
			Max = swMax(Point, Max);
		}
	}
}

swGroup* swPiece::GetTopGroup()
{
	return mGroup ? mGroup->GetTopGroup() : nullptr;
}

void swPiece::UpdatePosition(swStep Step)
{
	mPosition.Update(Step);
	mRotation.Update(Step);

	mModelWorld = swMatrix44(mRotation, mPosition);
}

void swPiece::UpdateMesh()
{
	mSynthPending = mPieceData && mPieceData->GetSynthInfo();

	if (mSynthPending)
		swGetPiecesLibrary()->RequestSynthMesh(this);
	else
	{
		delete mMesh;
		mMesh = nullptr;
		swGetPiecesLibrary()->CancelSynthMesh(this);
	}
}

swMesh* swPiece::GetDisplayMesh() const
{
	if (!mPieceData || mPieceData->mState != swAssetState::Loaded)
		return nullptr;

	if (mMesh)
		return mMesh;

	if (mSynthPending && mPieceData && mPieceData->GetSynthInfo())
		return swGetPiecesLibrary()->GetLoadingMesh();

	return nullptr;
}

void swPiece::SetGeneratedMesh(swMesh* Mesh)
{
	delete mMesh;
	mMesh = Mesh;
	mSynthPending = false;
}

void swPiece::RefreshGeneratedMesh()
{
	delete mMesh;
	mMesh = nullptr;

	UpdateMesh();
}
