#include "sw_global.h"
#include "sw_mesh.h"
#include "sw_colors.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "sw_data_part.h"
#include "sw_object_part.h"
#include "sw_group.h"
#include "sw_file.h"
#include "sw_application.h"
#include "sw_library.h"
#include "sw_context.h"
#include "sw_scene.h"
#include "sw_window_utilities.h"
#include "sw_synthesis.h"
#include "sw_track_train.h"
#include "sw_string.h"
#include "sw_model.h"

swPart::swPart()
    : swObject(swObjectType::Part)
{
}

swPart::swPart(swPartData* PartData)
	: swPart(swPartDataReference(PartData))
{
}

swPart::swPart(swPartDataReference PartDataRef)
	: swObject(swObjectType::Part)
{
	SetPartData(std::move(PartDataRef), QString(), true, true);
	mColorIndex = gDefaultColor;
	mColorCode = 16;
	mPivotMatrix = swMatrix44Identity();
}

swPart::swPart(const swPart& Other)
	: swObject(swObjectType::Part)
{
	SetPartData(Other.mPartData, Other.mID, true, false);
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

swPart::~swPart()
{
	swGetPartsLibrary()->CancelSynthMesh(this);

	if (mPartData)
	{
		swPartsLibrary* Library = swGetPartsLibrary();
		mPartData.Reset();
		Library->NotifyAssetStatusChanged();
	}

	delete mMesh;
}

void swPart::SetPartData(swPartDataReference PartDataRef, const QString& ID, bool Wait, bool UpdateSynthInfo)
{
	swPartsLibrary* Library = swGetPartsLibrary();
	const bool DataChanged = mPartData != PartDataRef;

	if (DataChanged || UpdateSynthInfo)
		Library->CancelSynthMesh(this);

	if (DataChanged)
	{
		delete mMesh;
		mMesh = nullptr;
	}

	swPartDataReference PreviousData(std::move(mPartData));
	mPartData = std::move(PartDataRef);
	if (mPartData)
	{
		if (Wait)
			Library->WaitForPartData(mPartData);
		else
			Library->RequestPartDataLoad(mPartData.Get(), swLoadPriority::Normal);
	}

	if (!ID.isEmpty())
		mID = ID;
	else if (mPartData)
		mID = mPartData->mFileName;
	else
		mID.clear();

	if (UpdateSynthInfo)
	{
		mControlPoints.clear();

		const swSynthesisInformation* SynthInfo = mPartData ? mPartData->GetSynthInfo() : nullptr;

		if (SynthInfo)
			SynthInfo->GetDefaultControlPoints(mControlPoints);

		UpdateMesh();
	}
	else if (PreviousData && DataChanged)
		UpdateMesh();

	PreviousData.Reset();

	Library->NotifyAssetStatusChanged();
}

bool swPart::SetPartId(swPartData* PartData)
{
	if (mPartData == PartData)
		return false;

	SetPartData(swPartDataReference(PartData), QString(), true, true);

	return true;
}

void swPart::UpdateID()
{
	mID = mPartData->mFileName;
}

void swPart::SaveLDraw(QTextStream& Stream) const
{
	const QLatin1String LineEnding("\r\n");

	if (mStepHide != SW_STEP_MAXIMUM)
		Stream << QLatin1String("0 !SW STEP_HIDE PART ") << mStepHide << LineEnding;

	if (IsHidden())
		Stream << QLatin1String("0 !SW HIDDEN PART") << LineEnding;

	if (mPivotPointValid)
	{
		const float* PivotMatrix = mPivotMatrix.GetFloats();
		const float PivotNumbers[12] = { PivotMatrix[12], -PivotMatrix[14], PivotMatrix[13], PivotMatrix[0], -PivotMatrix[8], PivotMatrix[4], -PivotMatrix[2], PivotMatrix[10], -PivotMatrix[6], PivotMatrix[1], -PivotMatrix[9], PivotMatrix[5] };

		Stream << QLatin1String("0 !SW PIVOT PART ");

		for (int NumberIdx = 0; NumberIdx < 12; NumberIdx++)
			Stream << ' ' << swFormatValue(PivotNumbers[NumberIdx], NumberIdx < 3 ? 4 : 6);

		Stream << LineEnding;
	}

	mPosition.Save(Stream, "PART", "POSITION", false);
	mRotation.Save(Stream, "PART", "ORIENTATION", false);

	Stream << "1 " << mColorCode << ' ';

	const float* Matrix = mModelWorld.GetFloats();
	const float Numbers[12] = { Matrix[12], -Matrix[14], Matrix[13], Matrix[0], -Matrix[8], Matrix[4], -Matrix[2], Matrix[10], -Matrix[6], Matrix[1], -Matrix[9], Matrix[5] };

	for (int NumberIdx = 0; NumberIdx < 12; NumberIdx++)
		Stream << swFormatValue(Numbers[NumberIdx], NumberIdx < 3 ? 4 : 6) << ' ';

	Stream << mID << LineEnding;
}

bool swPart::ParseLDrawLine(QTextStream& Stream)
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
		else if (mRotation.Load(Stream, Token, "ORIENTATION"))
			continue;
	}

	return false;
}

bool swPart::FileLoad(swFile& file, Project* Project)
{
	quint8 version, ch;

	version = file.ReadU8();

	// file format of version 0.80
	if (version > 12)
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
				mRotation.ChangeKey(swMatrix33FromAxisAngle(swVector3(param[0], param[1], param[2]), param[3] * SW_DEGREES_TO_RADIANS), time, true);
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
					mRotation.ChangeKey(swMatrix33FromAxisAngle(swVector3(param[0], param[1], param[2]), param[3] * SW_DEGREES_TO_RADIANS), time, true);
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
						ModelWorld = swMultiply(swMatrix44RotationZ(Rotation[2] * SW_DEGREES_TO_RADIANS), swMultiply(swMatrix44RotationY(Rotation[1] * SW_DEGREES_TO_RADIANS), swMultiply(swMatrix44RotationX(Rotation[0] * SW_DEGREES_TO_RADIANS), ModelWorld)));
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
				ModelWorld = swMultiply(swMatrix44RotationZ(Rotation[2] * SW_DEGREES_TO_RADIANS), swMultiply(swMatrix44RotationY(Rotation[1] * SW_DEGREES_TO_RADIANS), swMultiply(swMatrix44RotationX(Rotation[0] * SW_DEGREES_TO_RADIANS), ModelWorld)));

				mPosition.ChangeKey(swVector3(ModelWorld.r[3][0], ModelWorld.r[3][1], ModelWorld.r[3][2]) * PositionScale, 1, true);
				mRotation.ChangeKey(swMatrix33(ModelWorld), 1, true);
			}
		}
	}

	// common to all versions
	char name[SW_PART_NAME_LENGTH];
	if (version < 10)
	{
		memset(name, 0, SW_PART_NAME_LENGTH);
		file.ReadBuffer(name, 9);
	}
	else
		file.ReadBuffer(name, SW_PART_NAME_LENGTH);
	swstrcat(name, ".dat");

	SetPartData(swGetPartsLibrary()->AcquirePartData(name, Project, true, false), QString(), true, true);

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
		mStepHide = Step == 255 ? SW_STEP_MAXIMUM : Step;
	}
	else
		mStepHide = SW_STEP_MAXIMUM;

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

void swPart::Initialize(const swMatrix44& WorldMatrix, swStep Step)
{
	mStepShow = Step;

	mPosition.SetValue(WorldMatrix.GetTranslation());
	mRotation.SetValue(swMatrix33(WorldMatrix));

	UpdatePosition(Step);
}

void swPart::InsertTime(swStep Start, swStep Time)
{
	if (mStepShow >= Start)
	{
		if (mStepShow < SW_STEP_MAXIMUM - Time)
			mStepShow += Time;
		else
			mStepShow = SW_STEP_MAXIMUM;
	}

	if (mStepHide >= Start)
	{
		if (mStepHide < SW_STEP_MAXIMUM - Time)
			mStepHide += Time;
		else
			mStepHide = SW_STEP_MAXIMUM;
	}

	if (mStepShow >= mStepHide)
	{
		if (mStepShow != SW_STEP_MAXIMUM)
			mStepHide = mStepShow + 1;
		else
		{
			mStepShow = SW_STEP_MAXIMUM - 1;
			mStepHide = SW_STEP_MAXIMUM;
		}
	}

	mPosition.InsertTime(Start, Time);
	mRotation.InsertTime(Start, Time);
}

void swPart::RemoveTime(swStep Start, swStep Time)
{
	if (mStepShow >= Start)
	{
		if (mStepShow > Time)
			mStepShow -= Time;
		else
			mStepShow = 1;
	}

	if (mStepHide != SW_STEP_MAXIMUM)
	{
		if (mStepHide > Time)
			mStepHide -= Time;
		else
			mStepHide = 1;
	}

	if (mStepShow >= mStepHide)
	{
		if (mStepShow != SW_STEP_MAXIMUM)
			mStepHide = mStepShow + 1;
		else
		{
			mStepShow = SW_STEP_MAXIMUM - 1;
			mStepHide = SW_STEP_MAXIMUM;
		}
	}

	mPosition.RemoveTime(Start, Time);
	mRotation.RemoveTime(Start, Time);
}

void swPart::RayTest(swObjectRayTest& ObjectRayTest) const
{
	const swMatrix44 InverseWorldMatrix = swMatrix44AffineInverse(mModelWorld);
	const swVector3 Start = swMultiply31(ObjectRayTest.Start, InverseWorldMatrix);
	const swVector3 End = swMultiply31(ObjectRayTest.End, InverseWorldMatrix);

	if (MinIntersectDist(Start, End, ObjectRayTest.Distance, ObjectRayTest.PartRayTest))
	{
		ObjectRayTest.ObjectSection.Object = const_cast<swPart*>(this);
		ObjectRayTest.ObjectSection.Section = SW_PART_SECTION_POSITION;
		ObjectRayTest.PartRayTest.Transform = swMultiply(ObjectRayTest.PartRayTest.Transform, mModelWorld);
	}

	if (mPartData->GetSynthInfo() && AreControlPointsVisible())
	{
		const swVector3 Min(-SW_PART_CONTROL_POINT_SIZE, -SW_PART_CONTROL_POINT_SIZE, -SW_PART_CONTROL_POINT_SIZE);
		const swVector3 Max(SW_PART_CONTROL_POINT_SIZE, SW_PART_CONTROL_POINT_SIZE, SW_PART_CONTROL_POINT_SIZE);

		for (quint32 ControlPointIndex = 0; ControlPointIndex < mControlPoints.size(); ControlPointIndex++)
		{
			const swMatrix44 InverseTransform = swMatrix44AffineInverse(GetControlPointCubeTransform(ControlPointIndex));
			const swVector3 PointStart = swMultiply31(Start, InverseTransform);
			const swVector3 PointEnd = swMultiply31(End, InverseTransform);

			float Distance;
			swVector3 Plane;

			if (swBoundingBoxRayIntersectDistance(Min, Max, PointStart, PointEnd, &Distance, nullptr, &Plane))
			{
				ObjectRayTest.ObjectSection.Object = const_cast<swPart*>(this);
				ObjectRayTest.ObjectSection.Section = SW_PART_SECTION_CONTROL_POINT_FIRST + ControlPointIndex;
				ObjectRayTest.Distance = Distance;
				ObjectRayTest.PartRayTest.Plane = Plane;
			}
		}
	}
}

bool swPart::MinIntersectDist(const swVector3& Start, const swVector3& End, float& MinDistance, swPartRayTest& PartRayTest) const
{
	swMesh* Mesh = GetDisplayMesh();

	if (!Mesh || mPartData->IsLoading())
		return mPartData->MinIntersectDist(Start, End, MinDistance, PartRayTest);

	if (!Mesh->MinIntersectDist(Start, End, MinDistance, PartRayTest.Plane, PartRayTest.Normal))
		return false;

	PartRayTest.PartData = mPartData.Get();
	PartRayTest.Transform = swMatrix44Identity();

	return true;
}

void swPart::BoxTest(swObjectBoxTest& ObjectBoxTest) const
{
	if (IntersectsPlanes(mModelWorld, ObjectBoxTest.Planes))
		ObjectBoxTest.Objects.emplace_back(const_cast<swPart*>(this));
}

bool swPart::IntersectsPlanes(const swMatrix44& WorldMatrix, const swVector4 Planes[6]) const
{
	swMesh* Mesh = GetDisplayMesh();

	if (!Mesh || mPartData->IsLoading())
		return mPartData->BoxTest(WorldMatrix, Planes);

	const swMatrix44 InverseWorldMatrix = swMatrix44AffineInverse(WorldMatrix);
	swVector4 LocalPlanes[6];

	for (int PlaneIdx = 0; PlaneIdx < 6; PlaneIdx++)
	{
		const swVector3 PlaneNormal = swMultiply30(Planes[PlaneIdx], InverseWorldMatrix);
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

void swPart::DrawInterface(swContext* Context, const swScene& Scene) const
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

	if (IsFocused(SW_PART_SECTION_POSITION))
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

		Context->SetWorldMatrix(swMultiply(mPivotMatrix, WorldMatrix));

		Context->SetVertexBufferPointer(Verts);
		Context->SetVertexFormatPosition(3);
		Context->SetIndexBufferPointer(Indices);

		Context->DrawIndexedPrimitives(GL_LINES, 24, GL_UNSIGNED_SHORT, 0);
	}

	if (mPartData->GetSynthInfo())
		DrawSynthInterface(Context, WorldMatrix);
}

void swPart::DrawSynthInterface(swContext* Context, const swMatrix44& WorldMatrix) const
{
	if (mControlPoints.empty() || !AreControlPointsVisible())
		return;

	float Verts[8 * 3];
	float* CurVert = Verts;

	swVector3 CubeMin(-SW_PART_CONTROL_POINT_SIZE, -SW_PART_CONTROL_POINT_SIZE, -SW_PART_CONTROL_POINT_SIZE);
	swVector3 CubeMax(SW_PART_CONTROL_POINT_SIZE, SW_PART_CONTROL_POINT_SIZE, SW_PART_CONTROL_POINT_SIZE);

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
	const swVector4 ControlPointSeatedColor = swVector4FromColor(Preferences.mControlPointSeatedColor);

	for (quint32 ControlPointIndex = 0; ControlPointIndex < mControlPoints.size(); ControlPointIndex++)
	{
		Context->SetWorldMatrix(swMultiply(GetControlPointCubeTransform(ControlPointIndex), WorldMatrix));

		Context->SetVertexBufferPointer(Verts);
		Context->SetVertexFormatPosition(3);
		Context->SetIndexBufferPointer(Indices);

		// seating, focused, then seated color
		if (static_cast<int>(ControlPointIndex) == mControlPointSeatFound)
			Context->SetColor(ControlPointSeatedColor);
		else if (IsFocused(SW_PART_SECTION_CONTROL_POINT_FIRST + ControlPointIndex))
			Context->SetColor(ControlPointFocusedColor);
		else if (IsControlPointSeated(ControlPointIndex))
			Context->SetColor(ControlPointSeatedColor);
		else
			Context->SetColor(ControlPointColor);

		Context->DrawIndexedPrimitives(GL_TRIANGLES, 36, GL_UNSIGNED_SHORT, 0);
	}

	Context->EnableCullFace(false);

	// path line shows past the part length
	UpdatePathCache();

	if (mPathPoints.size() > 1)
	{
		Context->SetWorldMatrix(WorldMatrix);
		Context->SetVertexBufferPointer(mPathPoints.data());
		Context->SetVertexFormatPosition(3);
		Context->EnableDepthTest(false);
		Context->SetLineWidth(2.0f * Preferences.mLineWidth);

		Context->SetColor(swVector4FromColor(Preferences.mPathColor));
		Context->DrawPrimitives(GL_LINE_STRIP, 0, static_cast<GLsizei>(mPathReachedPointCount));

		if (mPathReachedPointCount < mPathPoints.size())
		{
			Context->SetColor(swVector4FromColor(Preferences.mPathOutOfReachColor));
			Context->DrawPrimitives(GL_LINE_STRIP, static_cast<GLint>(mPathReachedPointCount) - 1, static_cast<GLsizei>(mPathPoints.size() - mPathReachedPointCount + 1));
		}

		Context->SetLineWidth(Preferences.mLineWidth);
		Context->EnableDepthTest(true);
	}

	Context->EnableColorBlend(false);
}

QVariant swPart::GetPropertyValue(swObjectPropertyId PropertyId) const
{
	switch (PropertyId)
	{
	case swObjectPropertyId::PartId:
		return QVariant::fromValue<void*>(mPartData.Get());

	case swObjectPropertyId::PartColor:
		return GetColorIndex();

	case swObjectPropertyId::PartStepShow:
	case swObjectPropertyId::PartStepHide:
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

bool swPart::SetPropertyValue(swObjectPropertyId PropertyId, swStep Step, bool AddKey, QVariant Value)
{
	Q_UNUSED(Step);
	Q_UNUSED(AddKey);

	switch (PropertyId)
	{
	case swObjectPropertyId::PartId:
		return SetPartId(static_cast<swPartData*>(Value.value<void*>()));

	case swObjectPropertyId::PartColor:
		return SetColorIndex(Value.toInt());

	case swObjectPropertyId::PartStepShow:
	case swObjectPropertyId::PartStepHide:
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

swStep swPart::GetLastStep() const
{
	swStep Step = std::max({ mStepShow, mPosition.GetLastKeyFrame(), mRotation.GetLastKeyFrame() });

	if (mStepHide != SW_STEP_MAXIMUM)
		Step = swMaximum(Step, mStepHide);

	return Step;
}

bool swPart::HasKeyFrame(swObjectPropertyId PropertyId, swStep Time) const
{
	switch (PropertyId)
	{
	case swObjectPropertyId::PartId:
	case swObjectPropertyId::PartColor:
	case swObjectPropertyId::PartStepShow:
	case swObjectPropertyId::PartStepHide:
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

bool swPart::SetKeyFrame(swObjectPropertyId PropertyId, swStep Time, bool KeyFrame)
{
	switch (PropertyId)
	{
	case swObjectPropertyId::PartId:
	case swObjectPropertyId::PartColor:
	case swObjectPropertyId::PartStepShow:
	case swObjectPropertyId::PartStepHide:
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

void swPart::RemoveKeyFrames()
{
	mPosition.RemoveAllKeys();
	mRotation.RemoveAllKeys();
}

swPartHistoryState swPart::GetHistoryState() const
{
	swPartHistoryState State;

	State.Id = mId;
	State.Hidden = mHidden;
	State.FileLine = mFileLine;
	State.PartId = mID;
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

void swPart::SetHistoryState(const swPartHistoryState& State, Project* Project, swGroup* Group)
{
	mId = State.Id;
	mHidden = State.Hidden;
	mFileLine = State.FileLine;
	mID = State.PartId;
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

	swPartDataReference RestoredData = swGetPartsLibrary()->AcquirePartData(mID.toLatin1(), Project, true, true);

	if (mPartData != RestoredData)
	{
		const bool NewPart = !mPartData;

		SetPartData(std::move(RestoredData), mID, true, false);

		if (NewPart)
			UpdateMesh();
	}
	else
		UpdateMesh();

	//	std::vector<bool> mTrainTrackConnections;
}

void swPart::AddMainModelRenderMeshes(swScene* Scene, bool Highlight, bool Fade) const
{
	swRenderMeshState RenderMeshState = swRenderMeshState::Default;
	bool ParentActive = false;

	if (Highlight)
		RenderMeshState = swRenderMeshState::Highlighted;

	if (Fade)
		RenderMeshState = swRenderMeshState::Faded;

	if (Scene->GetDrawInterface())
	{
		const swPart* ActiveSubmodelInstance = Scene->GetActiveSubmodelInstance();

		if (!ActiveSubmodelInstance)
			RenderMeshState = IsFocused() ? swRenderMeshState::Focused : (IsSelected() ? swRenderMeshState::Selected : RenderMeshState);
		else if (ActiveSubmodelInstance == this)
			ParentActive = true;
		else
			RenderMeshState = swRenderMeshState::Faded;
	}

	if (mPartData->GetSynthInfo() && !HasGeneratedMesh() && Scene->GetRequireGeneratedMeshes())
	{
		Scene->MarkMissingAssets();
		return;
	}

	if (mSynthPending && Scene->GetRequireCompleteAssets())
		Scene->MarkMissingAssets();
	else if (swMesh* Mesh = GetDisplayMesh(); Mesh && !mPartData->IsLoading())
		Scene->AddMesh(Mesh, mModelWorld, mColorIndex, RenderMeshState);
	else
		mPartData->AddRenderMeshes(Scene, mModelWorld, mColorIndex, RenderMeshState, ParentActive);

	if (RenderMeshState == swRenderMeshState::Focused || RenderMeshState == swRenderMeshState::Selected)
		Scene->AddInterfaceObject(this);
}

void swPart::AddSubModelRenderMeshes(swScene* Scene, const swMatrix44& WorldMatrix, int DefaultColorIndex, swRenderMeshState RenderMeshState, bool ParentActive) const
{
	int ColorIndex = mColorIndex;

	if (ColorIndex == gDefaultColor)
		ColorIndex = DefaultColorIndex;

	const swPart* ActiveSubmodelInstance = Scene->GetActiveSubmodelInstance();

	if (ActiveSubmodelInstance == this)
		RenderMeshState = swRenderMeshState::Default;
	else if (ParentActive)
		RenderMeshState = IsFocused() ? swRenderMeshState::Focused : (IsSelected() ? swRenderMeshState::Selected : swRenderMeshState::Default);

	if (mPartData->GetSynthInfo() && !HasGeneratedMesh() && Scene->GetRequireGeneratedMeshes())
	{
		Scene->MarkMissingAssets();
		return;
	}

	if (mSynthPending && Scene->GetRequireCompleteAssets())
		Scene->MarkMissingAssets();
	else if (swMesh* Mesh = GetDisplayMesh(); Mesh && !mPartData->IsLoading())
		Scene->AddMesh(Mesh, swMultiply(mModelWorld, WorldMatrix), ColorIndex, RenderMeshState);
	else
		mPartData->AddRenderMeshes(Scene, swMultiply(mModelWorld, WorldMatrix), ColorIndex, RenderMeshState, ActiveSubmodelInstance == this);

	if (ParentActive && (RenderMeshState == swRenderMeshState::Focused || RenderMeshState == swRenderMeshState::Selected))
		Scene->AddInterfaceObject(this);
}

void swPart::SubModelCompareBoundingBox(const swMatrix44& WorldMatrix, swVector3& Min, swVector3& Max) const
{
	const swMatrix44 ModelWorldMatrix = swMultiply(mModelWorld, WorldMatrix);
	swMesh* Mesh = GetDisplayMesh();

	if (!Mesh || mPartData->IsLoading())
	{
		mPartData->CompareBoundingBox(ModelWorldMatrix, Min, Max);
		return;
	}

	swVector3 Points[8];
	swGetBoxCorners(Mesh->mBoundingBox, Points);

	for (const swVector3& Corner : Points)
	{
		const swVector3 Point = swMultiply31(Corner, ModelWorldMatrix);
		Min = swMinimum(Point, Min);
		Max = swMaximum(Point, Max);
	}
}

void swPart::SubModelAddBoundingBoxPoints(const swMatrix44& WorldMatrix, std::vector<swVector3>& Points, swGeometryBoundsMode Mode) const
{
	if (Mode == swGeometryBoundsMode::AvailableOnly && mPartData->GetSynthInfo() && !HasGeneratedMesh())
		return;

	swMesh* Mesh = GetDisplayMesh();

	if (!Mesh || mPartData->IsLoading())
		mPartData->AddSubModelBoundingBoxPoints(swMultiply(mModelWorld, WorldMatrix), Points, Mode);
	else
	{
		swVector3 BoxPoints[8];
		const swMatrix44 ModelWorldMatrix = swMultiply(mModelWorld, WorldMatrix);

		swGetBoxCorners(Mesh->mBoundingBox, BoxPoints);

		for (int i = 0; i < 8; i++)
			Points.emplace_back(swMultiply31(BoxPoints[i], ModelWorldMatrix));
	}
}

void swPart::MoveSelected(swStep Step, bool AddKey, const swVector3& Distance)
{
	const quint32 Section = GetTransformSection();

	if (Section == SW_PART_SECTION_POSITION || Section == SW_PART_SECTION_INVALID)
	{
		const swVector3 Position = mModelWorld.GetTranslation() + Distance;

		SetPosition(Position, Step, AddKey);

		mModelWorld.SetTranslation(Position);
	}
	else if (Section >= SW_PART_SECTION_CONTROL_POINT_FIRST)
	{
		const quint32 ControlPointIndex = Section - SW_PART_SECTION_CONTROL_POINT_FIRST;

		if (ControlPointIndex < mControlPoints.size())
		{
			const swMatrix33 InverseWorldMatrix = swMatrix33AffineInverse(swMatrix33(mModelWorld));
			const swSynthesisInformation* SynthInfo = mPartData->GetSynthInfo();
			swMatrix44& Transform = mControlPoints[ControlPointIndex].Transform;
			const swVector3 Position = Transform.GetTranslation();
			const swVector3 LocalDistance = swMultiply(Distance, InverseWorldMatrix);
			const bool WasReachable = !SynthInfo || SynthInfo->IsReachable(mControlPoints);

			Transform.SetTranslation(Position + LocalDistance);

			// stop control points at the part length
			if (WasReachable && !SynthInfo->IsReachable(mControlPoints))
			{
				float LowFactor = 0.0f;
				float HighFactor = 1.0f;

				for (int Iteration = 0; Iteration < 16; Iteration++)
				{
					const float MiddleFactor = (LowFactor + HighFactor) / 2.0f;
					Transform.SetTranslation(Position + LocalDistance * MiddleFactor);

					if (SynthInfo->IsReachable(mControlPoints))
						LowFactor = MiddleFactor;
					else
						HighFactor = MiddleFactor;
				}

				Transform.SetTranslation(Position + LocalDistance * LowFactor);
			}
		}

		UpdateMesh();
	}
}

void swPart::Rotate(swStep Step, bool AddKey, const swMatrix33& RotationMatrix, const swVector3& Center, const swMatrix33& RotationFrame)
{
	const quint32 Section = GetTransformSection();

	if (Section == SW_PART_SECTION_POSITION || Section == SW_PART_SECTION_INVALID)
	{
		swVector3 Distance = mModelWorld.GetTranslation() - Center;
		const swMatrix33 LocalToWorldMatrix = swMatrix33(mModelWorld);

		const swMatrix33 LocalToFocusMatrix = swMultiply(LocalToWorldMatrix, RotationFrame);
		swMatrix33 NewLocalToWorldMatrix = swMultiply(LocalToFocusMatrix, RotationMatrix);

		const swMatrix33 WorldToLocalMatrix = swMatrix33AffineInverse(LocalToWorldMatrix);

		Distance = swMultiply(Distance, WorldToLocalMatrix);
		Distance = swMultiply(Distance, NewLocalToWorldMatrix);

		NewLocalToWorldMatrix.Orthonormalize();

		SetPosition(Center + Distance, Step, AddKey);
		SetRotation(NewLocalToWorldMatrix, Step, AddKey);
	}
	else if (Section >= SW_PART_SECTION_CONTROL_POINT_FIRST)
	{
		const quint32 ControlPointIndex = Section - SW_PART_SECTION_CONTROL_POINT_FIRST;

		if (ControlPointIndex < mControlPoints.size())
		{
			swMatrix44& Transform = mControlPoints[ControlPointIndex].Transform;
			const swMatrix33 PartWorldMatrix(mModelWorld);
			const swMatrix33 LocalToWorldMatrix = swMultiply(swMatrix33(Transform), PartWorldMatrix);

			const swMatrix33 LocalToFocusMatrix = swMultiply(LocalToWorldMatrix, RotationFrame);
			swMatrix33 NewLocalToWorldMatrix = swMultiply(swMultiply(LocalToFocusMatrix, RotationMatrix), swMatrix33AffineInverse(PartWorldMatrix));

			NewLocalToWorldMatrix.Orthonormalize();

			// swing around the selection center
			const swVector3 WorldPosition = swMultiply31(Transform.GetTranslation(), mModelWorld);
			const swVector3 Distance = swMultiply(swMultiply(WorldPosition - Center, swMatrix33AffineInverse(LocalToWorldMatrix)), swMultiply(LocalToFocusMatrix, RotationMatrix));
			const swVector3 Position = swMultiply31(Center + Distance, swMatrix44AffineInverse(mModelWorld));

			const swSynthesisInformation* SynthInfo = mPartData->GetSynthInfo();
			const swMatrix44 PreviousTransform = Transform;
			const bool WasReachable = !SynthInfo || SynthInfo->IsReachable(mControlPoints);

			Transform = swMatrix44(NewLocalToWorldMatrix, Position);

			// stop rotations past the part length
			if (WasReachable && !SynthInfo->IsReachable(mControlPoints))
				Transform = PreviousTransform;
		}

		UpdateMesh();
	}
}

void swPart::MovePivotPoint(const swVector3& Distance)
{
	if (!IsFocused(SW_PART_SECTION_POSITION))
		return;

	mPivotMatrix.SetTranslation(mPivotMatrix.GetTranslation() + swMultiply30(Distance, swMatrix44AffineInverse(mModelWorld)));
	mPivotPointValid = true;
}

void swPart::RotatePivotPoint(const swMatrix33& RotationMatrix)
{
	if (!IsFocused(SW_PART_SECTION_POSITION))
		return;

	swMatrix33 NewPivotRotationMatrix = swMultiply(RotationMatrix, swMatrix33(mPivotMatrix));
	NewPivotRotationMatrix.Orthonormalize();

	mPivotMatrix = swMatrix44(NewPivotRotationMatrix, mPivotMatrix.GetTranslation());
	mPivotPointValid = true;
}

quint32 swPart::GetAllowedTransforms() const
{
	const quint32 Section = GetFocusSection();

	if (Section == SW_PART_SECTION_POSITION || Section == SW_PART_SECTION_INVALID)
		return SW_OBJECT_TRANSFORM_MOVE_XYZ | SW_OBJECT_TRANSFORM_ROTATE_XYZ;

	const swSynthesisInformation* SynthInfo = mPartData->GetSynthInfo();

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

swVector3 swPart::GetSectionPosition(quint32 Section) const
{
	if (Section == SW_PART_SECTION_POSITION)
	{
		if (mPivotPointValid)
			return swMultiply(mPivotMatrix, mModelWorld).GetTranslation();
		else
			return mModelWorld.GetTranslation();
	}

	if (mPartData->GetSynthInfo())
	{
		if (Section >= SW_PART_SECTION_CONTROL_POINT_FIRST)
		{
			const quint32 ControlPointIndex = Section - SW_PART_SECTION_CONTROL_POINT_FIRST;

			if (ControlPointIndex < mControlPoints.size())
			{
				const swMatrix44& Transform = mControlPoints[ControlPointIndex].Transform;
				return swMultiply(Transform, mModelWorld).GetTranslation();
			}
		}
	}

	if (mPartData->GetTrainTrackInfo())
	{
		if (Section >= SW_PART_SECTION_TRAIN_TRACK_CONNECTION_FIRST)
		{
			const quint32 ConnectionIndex = Section - SW_PART_SECTION_TRAIN_TRACK_CONNECTION_FIRST;
			const std::vector<swTrainTrackConnection>& Connections = mPartData->GetTrainTrackInfo()->GetConnections();

			if (ConnectionIndex < Connections.size())
			{
				const swMatrix44& Transform = Connections[ConnectionIndex].Transform;
				return swMultiply(Transform, mModelWorld).GetTranslation();
			}
		}
	}

	return swVector3(0.0f, 0.0f, 0.0f);
}

swVector3 swPart::GetRotationCenter() const
{
	const quint32 Section = GetFocusSection();

	if (Section == SW_PART_SECTION_POSITION || Section == SW_PART_SECTION_INVALID)
	{
		if (mPivotPointValid)
			return swMultiply31(mPivotMatrix.GetTranslation(), mModelWorld);
	}
	else if (mPartData->GetSynthInfo())
	{
		if (Section >= SW_PART_SECTION_CONTROL_POINT_FIRST)
		{
			const quint32 ControlPointIndex = Section - SW_PART_SECTION_CONTROL_POINT_FIRST;

			if (ControlPointIndex < mControlPoints.size())
			{
				const swMatrix44& Transform = mControlPoints[ControlPointIndex].Transform;
				return swMultiply31(Transform.GetTranslation(), mModelWorld);
			}
		}
	}
	else if (mPartData->GetTrainTrackInfo())
	{
		if (Section >= SW_PART_SECTION_TRAIN_TRACK_CONNECTION_FIRST)
		{
			const quint32 ConnectionIndex = Section - SW_PART_SECTION_TRAIN_TRACK_CONNECTION_FIRST;
			const std::vector<swTrainTrackConnection>& Connections = mPartData->GetTrainTrackInfo()->GetConnections();

			if (ConnectionIndex < Connections.size())
			{
				const swMatrix44& Transform = Connections[ConnectionIndex].Transform;
				return swMultiply(Transform, mModelWorld).GetTranslation();
			}
		}
	}

	return mModelWorld.GetTranslation();
}

swMatrix33 swPart::GetRelativeRotation() const
{
	const quint32 Section = GetFocusSection();

	if (Section == SW_PART_SECTION_POSITION || Section == SW_PART_SECTION_INVALID)
	{
		if (mPivotPointValid)
			return swMatrix33(swMultiply(mModelWorld, mPivotMatrix));
		else
			return swMatrix33(mModelWorld);
	}
	else if (mPartData->GetSynthInfo())
	{
		if (Section >= SW_PART_SECTION_CONTROL_POINT_FIRST)
		{
			const quint32 ControlPointIndex = Section - SW_PART_SECTION_CONTROL_POINT_FIRST;

			if (ControlPointIndex < mControlPoints.size())
			{
				const swMatrix44& Transform = mControlPoints[ControlPointIndex].Transform;
				return swMatrix33(swMultiply(Transform, mModelWorld));
			}
		}
	}
	else if (mPartData->GetTrainTrackInfo())
	{
		if (Section >= SW_PART_SECTION_TRAIN_TRACK_CONNECTION_FIRST)
		{
			const quint32 ConnectionIndex = Section - SW_PART_SECTION_TRAIN_TRACK_CONNECTION_FIRST;
			const std::vector<swTrainTrackConnection>& Connections = mPartData->GetTrainTrackInfo()->GetConnections();

			if (ConnectionIndex < Connections.size())
			{
				const swMatrix44& Transform = Connections[ConnectionIndex].Transform;
				return swMatrix33(swMultiply(Transform, mModelWorld));
			}
		}
	}

	return swMatrix33Identity();
}

bool swPart::CanAddControlPoint() const
{
	if (mControlPoints.size() >= SW_MAXIMUM_CONTROL_POINTS)
		return false;

	const swSynthesisInformation* SynthInfo = mPartData->GetSynthInfo();
	return SynthInfo && SynthInfo->CanAddControlPoints();
}

bool swPart::CanRemoveControlPoint() const
{
	const quint32 Section = GetFocusSection();
	return Section >= SW_PART_SECTION_CONTROL_POINT_FIRST && Section <= SW_PART_SECTION_CONTROL_POINT_LAST && mControlPoints.size() > 2;
}

bool swPart::InsertControlPoint(const swVector3& WorldStart, const swVector3& WorldEnd)
{
	if (!CanAddControlPoint())
		return false;

	const swMatrix44 InverseWorldMatrix = swMatrix44AffineInverse(mModelWorld);
	const swVector3 Start = swMultiply31(WorldStart, InverseWorldMatrix);
	const swVector3 End = swMultiply31(WorldEnd, InverseWorldMatrix);

	const swSynthesisInformation* SynthInfo = mPartData->GetSynthInfo();
	const int ControlPointIndex = SynthInfo->InsertControlPoint(mControlPoints, Start, End);

	if (ControlPointIndex)
	{
		SetFocused(GetFocusSection(), false);
		SetFocused(SW_PART_SECTION_CONTROL_POINT_FIRST + ControlPointIndex, true);
		UpdateMesh();
		return true;
	}

	return false;
}

// use to add the middle control point
void swPart::AddInsertedControlPoints()
{
	if (!swGetPreferences().mFlexMiddleControlPoint || mControlPoints.size() != 2 || !CanAddControlPoint())
		return;

	mPartData->GetSynthInfo()->AddMiddleControlPoint(mControlPoints);
	UpdateMesh();
}

// use to remember the seat of an end
void swPart::SetControlPointSeated(quint32 ControlPointIndex, const swVector3& SurfacePosition, const swVector3& SurfaceNormal)
{
	if (ControlPointIndex >= mControlPoints.size())
		return;

	const swControlPointSeat Seat = { ControlPointIndex, swMultiply(mControlPoints[ControlPointIndex].Transform, mModelWorld), SurfacePosition, SurfaceNormal };

	for (swControlPointSeat& OtherSeat : mControlPointSeats)
	{
		if (OtherSeat.ControlPointIndex == ControlPointIndex)
		{
			OtherSeat = Seat;
			return;
		}
	}

	mControlPointSeats.push_back(Seat);
}

// use to get the seat of an end
const swPart::swControlPointSeat* swPart::GetControlPointSeat(quint32 ControlPointIndex) const
{
	if (ControlPointIndex >= mControlPoints.size())
		return nullptr;

	const swMatrix44 WorldTransform = swMultiply(mControlPoints[ControlPointIndex].Transform, mModelWorld);

	for (const swControlPointSeat& Seat : mControlPointSeats)
	{
		if (Seat.ControlPointIndex != ControlPointIndex)
			continue;

		const swVector3 Axis = swMultiply30(swVector3(0.0f, 0.0f, 1.0f), WorldTransform);
		const swVector3 SeatAxis = swMultiply30(swVector3(0.0f, 0.0f, 1.0f), Seat.WorldTransform);

		if (swLength(WorldTransform.GetTranslation() - Seat.WorldTransform.GetTranslation()) < 0.01f && swDot(Axis, SeatAxis) > 0.9999f)
			return &Seat;

		return nullptr;
	}

	return nullptr;
}

bool swPart::IsControlPointSeated(quint32 ControlPointIndex) const
{
	return GetControlPointSeat(ControlPointIndex) != nullptr;
}

// use to rest a seated cube on the surface
swMatrix44 swPart::GetControlPointCubeTransform(quint32 ControlPointIndex) const
{
	swMatrix44 Transform = mControlPoints[ControlPointIndex].Transform;
	const swControlPointSeat* Seat = GetControlPointSeat(ControlPointIndex);

	if (!Seat)
		return Transform;

	// turn the closest side flush
	int NearestAxisIndex = 0;
	float NearestCosine = 0.0f;

	for (int AxisIndex = 0; AxisIndex < 3; AxisIndex++)
	{
		const float Cosine = swDot(swNormalize(swVector3(Transform[AxisIndex])), Seat->SurfaceNormal);

		if (fabsf(Cosine) > fabsf(NearestCosine))
		{
			NearestCosine = Cosine;
			NearestAxisIndex = AxisIndex;
		}
	}

	const swVector3 NearestAxis = swNormalize(swVector3(Transform[NearestAxisIndex])) * (NearestCosine < 0.0f ? -1.0f : 1.0f);
	const swVector3 TurnAxis = swCross(NearestAxis, Seat->SurfaceNormal);
	const float Angle = acosf(swClamp(swDot(NearestAxis, Seat->SurfaceNormal), -1.0f, 1.0f));
	const swVector3 Position = Transform.GetTranslation();

	if (Angle > 1e-4f && swLength(TurnAxis) > 1e-6f)
	{
		const swVector3 TurnNormal = swNormalize(TurnAxis);
		swMatrix33 Turn = swMatrix33FromAxisAngle(TurnNormal, Angle);

		if (swDot(swMultiply(NearestAxis, Turn), Seat->SurfaceNormal) < 0.999f)
			Turn = swMatrix33FromAxisAngle(TurnNormal, -Angle);

		swMatrix33 Rotation = swMultiply(swMatrix33(Transform), Turn);
		Rotation.Orthonormalize();

		Transform = swMatrix44(Rotation, Position);
	}

	const float Height = swDot(Position - Seat->SurfacePosition, Seat->SurfaceNormal);
	Transform.SetTranslation(Position + Seat->SurfaceNormal * (SW_PART_CONTROL_POINT_SIZE - Height));

	return Transform;
}

bool swPart::RemoveFocusedControlPoint()
{
	quint32 Section = GetFocusSection();

	if (Section < SW_PART_SECTION_CONTROL_POINT_FIRST)
		return false;

	const quint32 ControlPointIndex = Section - SW_PART_SECTION_CONTROL_POINT_FIRST;

	if (ControlPointIndex >= mControlPoints.size() || mControlPoints.size() <= 2)
		return false;

	SetFocused(GetFocusSection(), false);
	SetFocused(SW_PART_SECTION_POSITION, true);
	mControlPoints.erase(mControlPoints.begin() + ControlPointIndex);

	UpdateMesh();

	return true;
}

void swPart::UpdatePathCache() const
{
	if (mPathControlPoints == mControlPoints || !mPartData || !mPartData->GetSynthInfo())
		return;

	mPathControlPoints = mControlPoints;
	mPartData->GetSynthInfo()->GetPath(mControlPoints, mPathPoints, mPathReachedPointCount, mPathHandleLengths);
}

float swPart::GetControlPointHandleLength(quint32 ControlPointIndex) const
{
	UpdatePathCache();

	if (ControlPointIndex < mPathHandleLengths.size())
		return mPathHandleLengths[ControlPointIndex];

	return ControlPointIndex < mControlPoints.size() ? mControlPoints[ControlPointIndex].Scale : 0.0f;
}

void swPart::SetControlPointHandleLength(quint32 ControlPointIndex, float HandleLength)
{
	if (ControlPointIndex >= mControlPoints.size() || !mPartData || !mPartData->GetSynthInfo())
		return;

	const float CurrentHandleLength = GetControlPointHandleLength(ControlPointIndex);
	mControlPoints[ControlPointIndex].Scale = mPartData->GetSynthInfo()->GetSpreadForHandleLength(mControlPoints, ControlPointIndex, HandleLength, CurrentHandleLength);
	UpdateMesh();
}

bool swPart::CanClampControlPointTangent() const
{
	const quint32 Section = GetFocusSection();

	return Section > SW_PART_SECTION_CONTROL_POINT_FIRST && Section <= SW_PART_SECTION_CONTROL_POINT_LAST && Section - SW_PART_SECTION_CONTROL_POINT_FIRST + 1 < mControlPoints.size();
}

bool swPart::IsControlPointTangentClamped() const
{
	return CanClampControlPointTangent() && mControlPoints[GetFocusSection() - SW_PART_SECTION_CONTROL_POINT_FIRST].TangentClamped;
}

bool swPart::ToggleFocusedControlPointTangent()
{
	if (!CanClampControlPointTangent())
		return false;

	swPartControlPoint& ControlPoint = mControlPoints[GetFocusSection() - SW_PART_SECTION_CONTROL_POINT_FIRST];
	ControlPoint.TangentClamped = !ControlPoint.TangentClamped;

	UpdateMesh();

	return true;
}

bool swPart::AlignFocusedControlPointTangent()
{
	if (!CanClampControlPointTangent())
		return false;

	const std::vector<swPartControlPoint> ControlPoints = mControlPoints;
	mPartData->GetSynthInfo()->AlignControlPointTangent(mControlPoints, GetFocusSection() - SW_PART_SECTION_CONTROL_POINT_FIRST);

	if (mControlPoints == ControlPoints)
		return false;

	UpdateMesh();

	return true;
}

bool swPart::CanAlignControlPoint() const
{
	const quint32 Section = GetFocusSection();

	return Section >= SW_PART_SECTION_CONTROL_POINT_FIRST && Section <= SW_PART_SECTION_CONTROL_POINT_LAST && Section - SW_PART_SECTION_CONTROL_POINT_FIRST < mControlPoints.size();
}

bool swPart::AlignFocusedControlPointAxis(const swVector3& Axis)
{
	if (!CanAlignControlPoint())
		return false;

	const quint32 ControlPointIndex = GetFocusSection() - SW_PART_SECTION_CONTROL_POINT_FIRST;
	const std::vector<swPartControlPoint> ControlPoints = mControlPoints;

	// model axis to part space
	const swVector3 Direction = swMultiply(Axis, swMatrix33(swMatrix44AffineInverse(mModelWorld)));

	mControlPoints[ControlPointIndex].TangentClamped = false;
	mPartData->GetSynthInfo()->AlignControlPointDirection(mControlPoints, ControlPointIndex, Direction);

	if (mControlPoints == ControlPoints)
		return false;

	UpdateMesh();

	return true;
}

void swPart::VerifyControlPoints(std::vector<swPartControlPoint>& ControlPoints) const
{
	const swSynthesisInformation* SynthInfo = mPartData->GetSynthInfo();

	if (!SynthInfo)
	{
		ControlPoints.clear();
	}
	else
	{
		if (ControlPoints.size() > SW_MAXIMUM_CONTROL_POINTS)
			ControlPoints.resize(SW_MAXIMUM_CONTROL_POINTS);

		SynthInfo->VerifyControlPoints(ControlPoints);
	}
}

QString swPart::GetName() const
{
	return QString::fromLatin1(mPartData->m_strDescription);
}

bool swPart::IsVisible(swStep Step) const
{
	return !mHidden && (mStepShow <= Step) && (mStepHide > Step || mStepHide == SW_STEP_MAXIMUM);
}

bool swPart::IsVisibleInSubModel() const
{
	return (mStepHide == SW_STEP_MAXIMUM) && !mHidden;
}

void swPart::GetModelParts(const swMatrix44& WorldMatrix, int DefaultColorIndex, std::vector<swModelPartsEntry>& ModelParts) const
{
	if (!IsVisibleInSubModel())
		return;

	int ColorIndex = mColorIndex;

	if (ColorIndex == gDefaultColor)
		ColorIndex = DefaultColorIndex;

	if (!mMesh || mPartData->mState != swAssetState::Loaded)
		mPartData->GetModelParts(swMultiply(mModelWorld, WorldMatrix), ColorIndex, ModelParts);
	else
		ModelParts.emplace_back(swModelPartsEntry{ swMultiply(mModelWorld, WorldMatrix), mPartData.Get(), mMesh, ColorIndex });
}

const swBoundingBox& swPart::GetBoundingBox() const
{
	swMesh* Mesh = GetDisplayMesh();

	if (!Mesh || mPartData->IsLoading())
		return mPartData->GetBoundingBox();
	else
		return Mesh->mBoundingBox;
}

void swPart::CompareBoundingBox(swVector3& Min, swVector3& Max) const
{
	swMesh* Mesh = GetDisplayMesh();

	if (!Mesh || mPartData->IsLoading())
		mPartData->CompareBoundingBox(mModelWorld, Min, Max);
	else
	{
		swVector3 Points[8];

		swGetBoxCorners(Mesh->mBoundingBox, Points);

		for (int i = 0; i < 8; i++)
		{
			const swVector3 Point = swMultiply31(Points[i], mModelWorld);

			Min = swMinimum(Point, Min);
			Max = swMaximum(Point, Max);
		}
	}
}

swGroup* swPart::GetTopGroup()
{
	return mGroup ? mGroup->GetTopGroup() : nullptr;
}

void swPart::UpdatePosition(swStep Step)
{
	mPosition.Update(Step);
	mRotation.Update(Step);

	mModelWorld = swMatrix44(mRotation, mPosition);
}

void swPart::UpdateMesh()
{
	mSynthPending = mPartData && mPartData->GetSynthInfo();
	mPathControlPoints.clear();

	if (mSynthPending)
	{
		// clamped control points follow neighbors
		mPartData->GetSynthInfo()->ClampControlPointTangents(mControlPoints);
		swGetPartsLibrary()->RequestSynthMesh(this);
	}
	else
	{
		delete mMesh;
		mMesh = nullptr;
		swGetPartsLibrary()->CancelSynthMesh(this);
	}
}

swMesh* swPart::GetDisplayMesh() const
{
	if (!mPartData || mPartData->mState != swAssetState::Loaded)
		return nullptr;

	if (mMesh)
		return mMesh;

	if (mSynthPending && mPartData && mPartData->GetSynthInfo())
		return swGetPartsLibrary()->GetLoadingMesh();

	return nullptr;
}

void swPart::SetGeneratedMesh(swMesh* Mesh)
{
	delete mMesh;
	mMesh = Mesh;
	mSynthPending = false;
}

void swPart::RefreshGeneratedMesh()
{
	delete mMesh;
	mMesh = nullptr;

	UpdateMesh();
}
