#include "sw_global.h"
#include "sw_math.h"
#include "sw_string.h"
#include <stdlib.h>
#include <math.h>
#include <float.h>
#include "sw_file.h"
#include "sw_camera.h"
#include "sw_application.h"
#include "sw_context.h"

#define SW_CAMERA_POSITION_EDGE 7.5f
#define SW_CAMERA_TARGET_EDGE 7.5f

#define SW_CAMERA_SAVE_VERSION 7 // file format of version 0.80

swCamera::swCamera()
    : swObject(swObjectType::Camera)
{
}

swCamera::swCamera(bool Simple)
    : swObject(swObjectType::Camera), mSimple(Simple)
{
	if (!Simple)
	{
		mPosition.SetValue(swVector3(-250.0f, -250.0f, 75.0f));
		mTargetPosition.SetValue(swVector3(0.0f, 0.0f, 0.0f));
		mUpVector.SetValue(swVector3(-0.2357f, -0.2357f, 0.94281f));

		swCamera::UpdatePosition(1);
	}
}

swCamera::swCamera(bool Simple, const swVector3& Position, const swVector3& TargetPosition)
    : swObject(swObjectType::Camera), mSimple(Simple)
{
	// Fix the up vector
	swVector3 UpVector(0, 0, 1), FrontVector(Position - TargetPosition), SideVector;
	FrontVector.Normalize();
	if (FrontVector == UpVector)
		SideVector = swVector3(1, 0, 0);
	else
		SideVector = swCross(FrontVector, UpVector);
	UpVector = swCross(SideVector, FrontVector);
	UpVector.Normalize();

	mPosition.SetValue(Position);
	mTargetPosition.SetValue(TargetPosition);
	mUpVector.SetValue(UpVector);

	swCamera::UpdatePosition(1);
}

swCamera::~swCamera()
{
}

QString swCamera::GetCameraProjectionString(swCameraProjection CameraProjection)
{
	switch (CameraProjection)
	{
	case swCameraProjection::Perspective:
		return QT_TRANSLATE_NOOP("Camera Projection", "Perspective");

	case swCameraProjection::Orthographic:
		return QT_TRANSLATE_NOOP("Camera Projection", "Orthographic");

	case swCameraProjection::Count:
		break;
	}

	return QString();
}

QStringList swCamera::GetCameraProjectionStrings()
{
	QStringList CameraProjections;

	for (int CameraProjectionIndex = 0; CameraProjectionIndex < static_cast<int>(swCameraProjection::Count); CameraProjectionIndex++)
		CameraProjections.push_back(GetCameraProjectionString(static_cast<swCameraProjection>(CameraProjectionIndex)));

	return CameraProjections;
}

swViewpoint swCamera::GetViewpoint(const QString& ViewpointName)
{
	const QLatin1String ViewpointNames[] =
	{
		QLatin1String("front"),
		QLatin1String("back"),
		QLatin1String("top"),
		QLatin1String("bottom"),
		QLatin1String("left"),
		QLatin1String("right"),
		QLatin1String("home")
	};

	SW_ARRAY_SIZE_CHECK(ViewpointNames, swViewpoint::Count);

	for (int ViewpointIndex = 0; ViewpointIndex < static_cast<int>(swViewpoint::Count); ViewpointIndex++)
		if (ViewpointNames[ViewpointIndex] == ViewpointName)
			return static_cast<swViewpoint>(ViewpointIndex);

	return swViewpoint::Count;
}

bool swCamera::SetName(const QString& Name)
{
	if (mName == Name)
		return false;

	mName = Name;

	return true;
}

void swCamera::CreateName(const std::vector<std::unique_ptr<swCamera>>& Cameras)
{
	if (!mName.isEmpty())
	{
		bool Found = false;

		for (const std::unique_ptr<swCamera>& Camera : Cameras)
		{
			if (Camera->GetName() == mName)
			{
				Found = true;
				break;
			}
		}

		if (!Found)
			return;
	}

	int MaxCameraNumber = 0;
	const QString Prefix = QLatin1String("Camera ");

	for (const std::unique_ptr<swCamera>& Camera : Cameras)
		MaxCameraNumber = qMax(MaxCameraNumber, swGetDefaultNameNumber(Camera->GetName(), Prefix));

	mName = swFormatDefaultName(Prefix, MaxCameraNumber + 1);
}

bool swCamera::SetProjection(swCameraProjection CameraProjection)
{
	if (static_cast<int>(CameraProjection) < 0 || CameraProjection >= swCameraProjection::Count)
		return false;

	if (GetProjection() == CameraProjection)
		return false;

	mProjection = CameraProjection;

	return true;
}

bool swCamera::SetFOV(float Fovy)
{
	if (m_fovy == Fovy)
		return false;

	m_fovy = Fovy;

	return true;
}

bool swCamera::SetNearPlane(float NearPlane)
{
	if (m_zNear == NearPlane)
		return false;

	m_zNear = NearPlane;

	return true;
}

bool swCamera::SetFarPlane(float FarPlane)
{
	if (m_zFar == FarPlane)
		return false;

	m_zFar = FarPlane;

	return true;
}

void swCamera::SaveLDraw(QTextStream& Stream) const
{
	const QLatin1String LineEnding("\r\n");

	Stream << QLatin1String("0 !LEOCAD CAMERA FOV ") << m_fovy << QLatin1String(" ZNEAR ") << m_zNear << QLatin1String(" ZFAR ") << m_zFar << LineEnding;

	mPosition.Save(Stream, "CAMERA", "POSITION", true);
	mTargetPosition.Save(Stream, "CAMERA", "TARGET_POSITION", true);
	mUpVector.Save(Stream, "CAMERA", "UP_VECTOR", true);

	Stream << QLatin1String("0 !LEOCAD CAMERA ");

	if (IsHidden())
		Stream << QLatin1String("HIDDEN");

	if (mProjection == swCameraProjection::Orthographic)
		Stream << QLatin1String("ORTHOGRAPHIC ");

	Stream << QLatin1String("NAME ") << mName << LineEnding;
}

bool swCamera::ParseLDrawLine(QTextStream& Stream)
{
	while (!Stream.atEnd())
	{
		QString Token;
		Stream >> Token;

		if (Token == QLatin1String("HIDDEN"))
			SetHidden(true);
		else if (Token == QLatin1String("ORTHOGRAPHIC"))
			SetProjection(swCameraProjection::Orthographic);
		else if (Token == QLatin1String("FOV"))
			Stream >> m_fovy;
		else if (Token == QLatin1String("ZNEAR"))
			Stream >> m_zNear;
		else if (Token == QLatin1String("ZFAR"))
			Stream >> m_zFar;
		else if (mPosition.Load(Stream, Token, "POSITION"))
			continue;
		else if (mTargetPosition.Load(Stream, Token, "TARGET_POSITION"))
			continue;
		else if (mUpVector.Load(Stream, Token, "UP_VECTOR"))
			continue;
		else if (Token == QLatin1String("NAME"))
		{
			mName = Stream.readAll().trimmed();
			return true;
		}
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////
// Camera save/load

bool swCamera::FileLoad(swFile& file)
{
	quint8 version, ch;

	version = file.ReadU8();

	if (version > SW_CAMERA_SAVE_VERSION)
		return false;

	if (version > 5)
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
		}

		file.ReadU32(&n, 1);
		while (n--)
		{
			file.ReadU16(&time, 1);
			file.ReadFloats(param, 4);
			file.ReadU8(&type, 1);
		}
	}

	if (version == 4)
	{
		char Name[81];
		file.ReadBuffer(Name, 80);
	}
	else
	{
		ch = file.ReadU8();
		if (ch == 0xFF)
			return false;
		char Name[81];
		file.ReadBuffer(Name, ch);
	}

	if (version < 3)
	{
		double d[3];

		file.ReadDoubles(d, 3);
		file.ReadDoubles(d, 3);
		file.ReadDoubles(d, 3);
	}

	if (version == 3)
	{
		ch = file.ReadU8();

		while (ch--)
		{
			quint8 step;
			double eye[3], target[3], up[3];

			file.ReadDoubles(eye, 3);
			file.ReadDoubles(target, 3);
			file.ReadDoubles(up, 3);
			file.ReadU8(&step, 1);

			file.ReadS32(); // snapshot
			file.ReadS32(); // cam
		}
	}

	if (version < 4)
	{
		file.ReadDouble(); // m_fovy
		file.ReadDouble(); // m_zFar
		file.ReadDouble(); // m_zNear
	}
	else
	{
		qint32 n;

		if (version < 6)
		{
			quint16 time;
			float param[4];
			quint8 type;

			n = file.ReadS32();
			while (n--)
			{
				file.ReadU16(&time, 1);
				file.ReadFloats(param, 3);
				file.ReadU8(&type, 1);
			}

			n = file.ReadS32();
			while (n--)
			{
				file.ReadU16(&time, 1);
				file.ReadFloats(param, 3);
				file.ReadU8(&type, 1);
			}
		}

		float f;
		file.ReadFloats(&f, 1); // m_fovy
		file.ReadFloats(&f, 1); // m_zFar
		file.ReadFloats(&f, 1); // m_zNear

		if (version < 5)
		{
			file.ReadS32();
		}
		else
		{
			file.ReadU8();
			file.ReadU8();
		}
	}

	if ((version > 1) && (version < 4))
	{
		quint32 show;
		qint32 user;

		file.ReadU32(&show, 1);
//		if (version > 2)
		file.ReadS32(&user, 1);
	}

	return true;
}

void swCamera::CompareBoundingBox(swVector3& Min, swVector3& Max)
{
	const swVector3 Points[2] =
	{
		mPosition, mTargetPosition
	};

	for (int i = 0; i < 2; i++)
	{
		const swVector3& Point = Points[i];

		// TODO: this should check the entire mesh

		Min = swMinimum(Point, Min);
		Max = swMaximum(Point, Max);
	}
}

void swCamera::MoveSelected(swStep Step, bool AddKey, const swVector3& Distance)
{
	if (IsSimple())
		AddKey = false;

	const quint32 FocusSection = GetFocusSection();

	if (FocusSection == SW_CAMERA_SECTION_POSITION || FocusSection == SW_CAMERA_SECTION_INVALID)
	{
		mPosition.ChangeKey(mPosition + Distance, Step, AddKey);
	}

	if (FocusSection == SW_CAMERA_SECTION_TARGET || FocusSection == SW_CAMERA_SECTION_INVALID)
	{
		mTargetPosition.ChangeKey(mTargetPosition + Distance, Step, AddKey);
	}

	if (FocusSection == SW_CAMERA_SECTION_UP_VECTOR)
	{
		mUpVector.ChangeKey(swNormalize(mUpVector + Distance), Step, AddKey);
	}
}

void swCamera::MoveRelative(const swVector3& Distance, swStep Step, bool AddKey)
{
	if (IsSimple())
		AddKey = false;

	const swVector3 Relative = swMultiply30(Distance, swMatrix44Transpose(mWorldView)) * 5.0f;

	mPosition.ChangeKey(mPosition + Relative, Step, AddKey);
	mTargetPosition.ChangeKey(mTargetPosition + Relative, Step, AddKey);

	UpdatePosition(Step);
}

void swCamera::Rotate(swStep Step, bool AddKey, const swMatrix33& RotationMatrix, const swVector3& Center, const swMatrix33& RotationFrame)
{
	if (GetFocusSection() != SW_CAMERA_SECTION_POSITION && GetFocusSection() != SW_CAMERA_SECTION_INVALID)
		return;

	const swMatrix33 LocalToWorldMatrix = swMatrix33(mWorldView);

	const swMatrix33 LocalToFocusMatrix = swMultiply(LocalToWorldMatrix, RotationFrame);
	swMatrix33 NewLocalToWorldMatrix = swMultiply(LocalToFocusMatrix, RotationMatrix);

	const swMatrix33 WorldToLocalMatrix = swMatrix33AffineInverse(LocalToWorldMatrix);

	swVector3 Distance = mPosition - Center;
	Distance = swMultiply(Distance, WorldToLocalMatrix);
	Distance = swMultiply(Distance, NewLocalToWorldMatrix);

	SetPosition(Center + Distance, Step, AddKey);

	Distance = mTargetPosition - Center;
	Distance = swMultiply(Distance, WorldToLocalMatrix);
	Distance = swMultiply(Distance, NewLocalToWorldMatrix);

	SetTargetPosition(Center + Distance, Step, AddKey);

	swVector3 UpVector = mUpVector;

	UpVector = swMultiply(UpVector, WorldToLocalMatrix);
	UpVector = swMultiply(UpVector, NewLocalToWorldMatrix);

	SetUpVector(UpVector, Step, AddKey);
}

void swCamera::UpdatePosition(swStep Step)
{
	mPosition.Update(Step);
	mTargetPosition.Update(Step);
	mUpVector.Update(Step);

	const swVector3 FrontVector(mPosition - mTargetPosition);
	const swVector3 SideVector = swCross(FrontVector, mUpVector);
	const swVector3 UpVector = swNormalize(swCross(SideVector, FrontVector));

	mWorldView = swMatrix44LookAt(mPosition, mTargetPosition, UpVector);
}

void swCamera::CopyPosition(const swCamera* Camera)
{
	m_fovy = Camera->m_fovy;
	m_zNear = Camera->m_zNear;
	m_zFar = Camera->m_zFar;

	mWorldView = Camera->mWorldView;
	mPosition = Camera->mPosition;
	mTargetPosition = Camera->mTargetPosition;
	mUpVector = Camera->mUpVector;
	mProjection = Camera->mProjection;
}

void swCamera::CopySettings(const swCamera* Camera)
{
	m_fovy = Camera->m_fovy;
	m_zNear = Camera->m_zNear;
	m_zFar = Camera->m_zFar;

	mProjection = Camera->mProjection;
}

void swCamera::DrawInterface(swContext* Context, const swScene& Scene) const
{
	Q_UNUSED(Scene);
	Context->SetMaterial(swMaterialType::UnlitColor);

	swMatrix44 ViewWorldMatrix = swMatrix44AffineInverse(mWorldView);
	ViewWorldMatrix.SetTranslation(swVector3(0, 0, 0));

	const swMatrix44 CameraViewMatrix = swMultiply(ViewWorldMatrix, swMatrix44Translation(mPosition));
	Context->SetWorldMatrix(CameraViewMatrix);

	float Verts[(12 + 8 + 8 + 3 + 4) * 3];
	float* CurVert = Verts;

	float Length = swLength(mPosition - mTargetPosition);

	*CurVert++ =  SW_CAMERA_POSITION_EDGE; *CurVert++ =  SW_CAMERA_POSITION_EDGE; *CurVert++ =  SW_CAMERA_POSITION_EDGE;
	*CurVert++ = -SW_CAMERA_POSITION_EDGE; *CurVert++ =  SW_CAMERA_POSITION_EDGE; *CurVert++ =  SW_CAMERA_POSITION_EDGE;
	*CurVert++ = -SW_CAMERA_POSITION_EDGE; *CurVert++ = -SW_CAMERA_POSITION_EDGE; *CurVert++ =  SW_CAMERA_POSITION_EDGE;
	*CurVert++ =  SW_CAMERA_POSITION_EDGE; *CurVert++ = -SW_CAMERA_POSITION_EDGE; *CurVert++ =  SW_CAMERA_POSITION_EDGE;
	*CurVert++ =  SW_CAMERA_POSITION_EDGE; *CurVert++ =  SW_CAMERA_POSITION_EDGE; *CurVert++ = -SW_CAMERA_POSITION_EDGE;
	*CurVert++ = -SW_CAMERA_POSITION_EDGE; *CurVert++ =  SW_CAMERA_POSITION_EDGE; *CurVert++ = -SW_CAMERA_POSITION_EDGE;
	*CurVert++ = -SW_CAMERA_POSITION_EDGE; *CurVert++ = -SW_CAMERA_POSITION_EDGE; *CurVert++ = -SW_CAMERA_POSITION_EDGE;
	*CurVert++ =  SW_CAMERA_POSITION_EDGE; *CurVert++ = -SW_CAMERA_POSITION_EDGE; *CurVert++ = -SW_CAMERA_POSITION_EDGE;
	*CurVert++ =  SW_CAMERA_POSITION_EDGE; *CurVert++ =  SW_CAMERA_POSITION_EDGE; *CurVert++ = -SW_CAMERA_POSITION_EDGE * 2;
	*CurVert++ = -SW_CAMERA_POSITION_EDGE; *CurVert++ =  SW_CAMERA_POSITION_EDGE; *CurVert++ = -SW_CAMERA_POSITION_EDGE * 2;
	*CurVert++ = -SW_CAMERA_POSITION_EDGE; *CurVert++ = -SW_CAMERA_POSITION_EDGE; *CurVert++ = -SW_CAMERA_POSITION_EDGE * 2;
	*CurVert++ =  SW_CAMERA_POSITION_EDGE; *CurVert++ = -SW_CAMERA_POSITION_EDGE; *CurVert++ = -SW_CAMERA_POSITION_EDGE * 2;

	*CurVert++ =  SW_CAMERA_TARGET_EDGE; *CurVert++ =  SW_CAMERA_TARGET_EDGE; *CurVert++ =  SW_CAMERA_TARGET_EDGE - Length;
	*CurVert++ = -SW_CAMERA_TARGET_EDGE; *CurVert++ =  SW_CAMERA_TARGET_EDGE; *CurVert++ =  SW_CAMERA_TARGET_EDGE - Length;
	*CurVert++ = -SW_CAMERA_TARGET_EDGE; *CurVert++ = -SW_CAMERA_TARGET_EDGE; *CurVert++ =  SW_CAMERA_TARGET_EDGE - Length;
	*CurVert++ =  SW_CAMERA_TARGET_EDGE; *CurVert++ = -SW_CAMERA_TARGET_EDGE; *CurVert++ =  SW_CAMERA_TARGET_EDGE - Length;
	*CurVert++ =  SW_CAMERA_TARGET_EDGE; *CurVert++ =  SW_CAMERA_TARGET_EDGE; *CurVert++ = -SW_CAMERA_TARGET_EDGE - Length;
	*CurVert++ = -SW_CAMERA_TARGET_EDGE; *CurVert++ =  SW_CAMERA_TARGET_EDGE; *CurVert++ = -SW_CAMERA_TARGET_EDGE - Length;
	*CurVert++ = -SW_CAMERA_TARGET_EDGE; *CurVert++ = -SW_CAMERA_TARGET_EDGE; *CurVert++ = -SW_CAMERA_TARGET_EDGE - Length;
	*CurVert++ =  SW_CAMERA_TARGET_EDGE; *CurVert++ = -SW_CAMERA_TARGET_EDGE; *CurVert++ = -SW_CAMERA_TARGET_EDGE - Length;

	*CurVert++ =  SW_CAMERA_TARGET_EDGE; *CurVert++ =  SW_CAMERA_TARGET_EDGE + 25.0f; *CurVert++ =  SW_CAMERA_TARGET_EDGE;
	*CurVert++ = -SW_CAMERA_TARGET_EDGE; *CurVert++ =  SW_CAMERA_TARGET_EDGE + 25.0f; *CurVert++ =  SW_CAMERA_TARGET_EDGE;
	*CurVert++ = -SW_CAMERA_TARGET_EDGE; *CurVert++ = -SW_CAMERA_TARGET_EDGE + 25.0f; *CurVert++ =  SW_CAMERA_TARGET_EDGE;
	*CurVert++ =  SW_CAMERA_TARGET_EDGE; *CurVert++ = -SW_CAMERA_TARGET_EDGE + 25.0f; *CurVert++ =  SW_CAMERA_TARGET_EDGE;
	*CurVert++ =  SW_CAMERA_TARGET_EDGE; *CurVert++ =  SW_CAMERA_TARGET_EDGE + 25.0f; *CurVert++ = -SW_CAMERA_TARGET_EDGE;
	*CurVert++ = -SW_CAMERA_TARGET_EDGE; *CurVert++ =  SW_CAMERA_TARGET_EDGE + 25.0f; *CurVert++ = -SW_CAMERA_TARGET_EDGE;
	*CurVert++ = -SW_CAMERA_TARGET_EDGE; *CurVert++ = -SW_CAMERA_TARGET_EDGE + 25.0f; *CurVert++ = -SW_CAMERA_TARGET_EDGE;
	*CurVert++ =  SW_CAMERA_TARGET_EDGE; *CurVert++ = -SW_CAMERA_TARGET_EDGE + 25.0f; *CurVert++ = -SW_CAMERA_TARGET_EDGE;

	*CurVert++ = 0.0f; *CurVert++ = 0.0f; *CurVert++ = 0.0f;
	*CurVert++ = 0.0f; *CurVert++ = 0.0f; *CurVert++ = -Length;
	*CurVert++ = 0.0f; *CurVert++ = 25.0f; *CurVert++ = 0.0f;

	const GLushort Indices[40 + 24 + 24 + 4 + 16] =
	{
		0, 1, 1, 2, 2, 3, 3, 0,
		4, 5, 5, 6, 6, 7, 7, 4,
		0, 4, 1, 5, 2, 6, 3, 7,
		8, 9, 9, 10, 10, 11, 11, 8,
		8, 28, 9, 28, 10, 28, 11, 28,
		12, 13, 13, 14, 14, 15, 15, 12,
		16, 17, 17, 18, 18, 19, 19, 16,
		12, 16, 13, 17, 14, 18, 15, 19,
		20, 21, 21, 22, 22, 23, 23, 20,
		24, 25, 25, 26, 26, 27, 27, 24,
		20, 24, 21, 25, 22, 26, 23, 27,
		28, 29, 28, 30,
		31, 32, 32, 33, 33, 34, 34, 31,
		28, 31, 28, 32, 28, 33, 28, 34
	};

	Context->SetVertexBufferPointer(Verts);
	Context->SetVertexFormatPosition(3);
	Context->SetIndexBufferPointer(Indices);

	const swPreferences& Preferences = swGetPreferences();
	const float LineWidth = Preferences.mLineWidth;
	const swVector4 SelectedColor = swVector4FromColor(Preferences.mObjectSelectedColor);
	const swVector4 FocusedColor = swVector4FromColor(Preferences.mObjectFocusedColor);
	const swVector4 CameraColor = swVector4FromColor(Preferences.mCameraColor);

	if (!IsSelected())
	{
		Context->SetLineWidth(LineWidth);
		Context->SetColor(CameraColor);

		Context->DrawIndexedPrimitives(GL_LINES, 40 + 24 + 24 + 4, GL_UNSIGNED_SHORT, 0);
	}
	else
	{
		if (IsSelected())
		{
			Context->SetLineWidth(2.0f * LineWidth);
			if (IsFocused(SW_CAMERA_SECTION_POSITION))
				Context->SetColor(FocusedColor);
			else
				Context->SetColor(SelectedColor);
		}
		else
		{
			Context->SetLineWidth(LineWidth);
			Context->SetColor(CameraColor);
		}

		Context->DrawIndexedPrimitives(GL_LINES, 40, GL_UNSIGNED_SHORT, 0);

		if (IsSelected())
		{
			Context->SetLineWidth(2.0f * LineWidth);
			if (IsFocused(SW_CAMERA_SECTION_TARGET))
				Context->SetColor(FocusedColor);
			else
				Context->SetColor(SelectedColor);
		}
		else
		{
			Context->SetLineWidth(LineWidth);
			Context->SetColor(CameraColor);
		}

		Context->DrawIndexedPrimitives(GL_LINES, 24, GL_UNSIGNED_SHORT, 40 * 2);

		if (IsSelected())
		{
			Context->SetLineWidth(2.0f * LineWidth);
			if (IsFocused(SW_CAMERA_SECTION_UP_VECTOR))
				Context->SetColor(FocusedColor);
			else
				Context->SetColor(SelectedColor);
		}
		else
		{
			Context->SetLineWidth(LineWidth);
			Context->SetColor(CameraColor);
		}

		Context->DrawIndexedPrimitives(GL_LINES, 24, GL_UNSIGNED_SHORT, (40 + 24) * 2);

		Context->SetColor(CameraColor);
		Context->SetLineWidth(LineWidth);

		float SizeY = tanf(SW_DEGREES_TO_RADIANS * m_fovy / 2) * Length;
		float SizeX = SizeY * 1.333f;

		*CurVert++ =  SizeX; *CurVert++ =  SizeY; *CurVert++ = -Length;
		*CurVert++ = -SizeX; *CurVert++ =  SizeY; *CurVert++ = -Length;
		*CurVert++ = -SizeX; *CurVert++ = -SizeY; *CurVert++ = -Length;
		*CurVert++ =  SizeX; *CurVert++ = -SizeY; *CurVert++ = -Length;

		Context->DrawIndexedPrimitives(GL_LINES, 4 + 16, GL_UNSIGNED_SHORT, (40 + 24 + 24) * 2);
	}
}

QVariant swCamera::GetPropertyValue(swObjectPropertyId PropertyId) const
{
	switch (PropertyId)
	{
	case swObjectPropertyId::PieceId:
	case swObjectPropertyId::PieceColor:
	case swObjectPropertyId::PieceStepShow:
	case swObjectPropertyId::PieceStepHide:
		break;

	case swObjectPropertyId::CameraName:
		return GetName();

	case swObjectPropertyId::CameraProjection:
		return static_cast<int>(GetProjection());

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

bool swCamera::SetPropertyValue(swObjectPropertyId PropertyId, swStep Step, bool AddKey, QVariant Value)
{
	Q_UNUSED(Step);
	Q_UNUSED(AddKey);

	switch (PropertyId)
	{
	case swObjectPropertyId::PieceId:
	case swObjectPropertyId::PieceColor:
	case swObjectPropertyId::PieceStepShow:
	case swObjectPropertyId::PieceStepHide:
		break;

	case swObjectPropertyId::CameraName:
		return SetName(Value.toString());

	case swObjectPropertyId::CameraProjection:
		return SetProjection(static_cast<swCameraProjection>(Value.toInt()));

	case swObjectPropertyId::CameraFOV:
		return SetFOV(Value.toFloat());

	case swObjectPropertyId::CameraNear:
		return SetNearPlane(Value.toFloat());

	case swObjectPropertyId::CameraFar:
		return SetFarPlane(Value.toFloat());

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

swStep swCamera::GetLastStep() const
{
	return std::max({ mPosition.GetLastKeyFrame(), mTargetPosition.GetLastKeyFrame(), mUpVector.GetLastKeyFrame() });
}

bool swCamera::HasKeyFrame(swObjectPropertyId PropertyId, swStep Time) const
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
		return false;

	case swObjectPropertyId::CameraPositionX:
	case swObjectPropertyId::CameraPositionY:
	case swObjectPropertyId::CameraPositionZ:
		return mPosition.HasKeyFrame(Time);

	case swObjectPropertyId::CameraTargetX:
	case swObjectPropertyId::CameraTargetY:
	case swObjectPropertyId::CameraTargetZ:
		return mTargetPosition.HasKeyFrame(Time);

	case swObjectPropertyId::CameraUpX:
	case swObjectPropertyId::CameraUpY:
	case swObjectPropertyId::CameraUpZ:
		return mUpVector.HasKeyFrame(Time);

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
		return false;
	}

	return false;
}

bool swCamera::SetKeyFrame(swObjectPropertyId PropertyId, swStep Time, bool KeyFrame)
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
		return false;

	case swObjectPropertyId::CameraPositionX:
	case swObjectPropertyId::CameraPositionY:
	case swObjectPropertyId::CameraPositionZ:
		return mPosition.SetKeyFrame(Time, KeyFrame);

	case swObjectPropertyId::CameraTargetX:
	case swObjectPropertyId::CameraTargetY:
	case swObjectPropertyId::CameraTargetZ:
		return mTargetPosition.SetKeyFrame(Time, KeyFrame);

	case swObjectPropertyId::CameraUpX:
	case swObjectPropertyId::CameraUpY:
	case swObjectPropertyId::CameraUpZ:
		return mUpVector.SetKeyFrame(Time, KeyFrame);

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
		return false;
	}

	return false;
}

void swCamera::RemoveKeyFrames()
{
	mPosition.RemoveAllKeys();
	mTargetPosition.RemoveAllKeys();
	mUpVector.RemoveAllKeys();
}

swCameraHistoryState swCamera::GetHistoryState() const
{
	swCameraHistoryState State;

	State.Id = mId;
	State.Hidden = mHidden;
	State.Simple = mSimple;
	State.Fovy = m_fovy;
	State.NearPlane = m_zNear;
	State.FarPlane = m_zFar;
	State.Projection = mProjection;
	State.Position = mPosition;
	State.TargetPosition = mTargetPosition;
	State.UpVector = mUpVector;
	State.Name = mName;

	return State;
}

void swCamera::SetHistoryState(const swCameraHistoryState& State)
{
	mId = State.Id;
	mHidden = State.Hidden;
	mSimple = State.Simple;
	m_fovy = State.Fovy;
	m_zNear = State.NearPlane;
	m_zFar = State.FarPlane;
	mProjection = State.Projection;
	mPosition = State.Position;
	mTargetPosition = State.TargetPosition;
	mUpVector = State.UpVector;
	mName = State.Name;
}

void swCamera::RayTest(swObjectRayTest& ObjectRayTest) const
{
	swVector3 Min = swVector3(-SW_CAMERA_POSITION_EDGE, -SW_CAMERA_POSITION_EDGE, -SW_CAMERA_POSITION_EDGE);
	swVector3 Max = swVector3(SW_CAMERA_POSITION_EDGE, SW_CAMERA_POSITION_EDGE, SW_CAMERA_POSITION_EDGE);

	swVector3 Start = swMultiply31(ObjectRayTest.Start, mWorldView);
	swVector3 End = swMultiply31(ObjectRayTest.End, mWorldView);

	float Distance;
	swVector3 Plane;

	if (swBoundingBoxRayIntersectDistance(Min, Max, Start, End, &Distance, nullptr, &Plane) && (Distance < ObjectRayTest.Distance))
	{
		ObjectRayTest.ObjectSection.Object = const_cast<swCamera*>(this);
		ObjectRayTest.ObjectSection.Section = SW_CAMERA_SECTION_POSITION;
		ObjectRayTest.Distance = Distance;
		ObjectRayTest.PieceRayTest.Plane = Plane;
	}

	Min = swVector3(-SW_CAMERA_TARGET_EDGE, -SW_CAMERA_TARGET_EDGE, -SW_CAMERA_TARGET_EDGE);
	Max = swVector3(SW_CAMERA_TARGET_EDGE, SW_CAMERA_TARGET_EDGE, SW_CAMERA_TARGET_EDGE);

	swMatrix44 WorldView = mWorldView;
	WorldView.SetTranslation(swMultiply30(-mTargetPosition, WorldView));

	Start = swMultiply31(ObjectRayTest.Start, WorldView);
	End = swMultiply31(ObjectRayTest.End, WorldView);

	if (swBoundingBoxRayIntersectDistance(Min, Max, Start, End, &Distance, nullptr, &Plane) && (Distance < ObjectRayTest.Distance))
	{
		ObjectRayTest.ObjectSection.Object = const_cast<swCamera*>(this);
		ObjectRayTest.ObjectSection.Section = SW_CAMERA_SECTION_TARGET;
		ObjectRayTest.Distance = Distance;
		ObjectRayTest.PieceRayTest.Plane = Plane;
	}

	const swMatrix44 ViewWorld = swMatrix44AffineInverse(mWorldView);
	const swVector3 UpVectorPosition = swMultiply31(swVector3(0, 25, 0), ViewWorld);

	WorldView = mWorldView;
	WorldView.SetTranslation(swMultiply30(-UpVectorPosition, WorldView));

	Start = swMultiply31(ObjectRayTest.Start, WorldView);
	End = swMultiply31(ObjectRayTest.End, WorldView);

	if (swBoundingBoxRayIntersectDistance(Min, Max, Start, End, &Distance, nullptr, &Plane) && (Distance < ObjectRayTest.Distance))
	{
		ObjectRayTest.ObjectSection.Object = const_cast<swCamera*>(this);
		ObjectRayTest.ObjectSection.Section = SW_CAMERA_SECTION_UP_VECTOR;
		ObjectRayTest.Distance = Distance;
		ObjectRayTest.PieceRayTest.Plane = Plane;
	}
}

void swCamera::BoxTest(swObjectBoxTest& ObjectBoxTest) const
{
	swVector3 Min(-SW_CAMERA_POSITION_EDGE, -SW_CAMERA_POSITION_EDGE, -SW_CAMERA_POSITION_EDGE);
	swVector3 Max(SW_CAMERA_POSITION_EDGE, SW_CAMERA_POSITION_EDGE, SW_CAMERA_POSITION_EDGE);

	swVector4 LocalPlanes[6];

	for (int PlaneIdx = 0; PlaneIdx < 6; PlaneIdx++)
	{
		const swVector3 Normal = swMultiply30(ObjectBoxTest.Planes[PlaneIdx], mWorldView);
		LocalPlanes[PlaneIdx] = swVector4(Normal, ObjectBoxTest.Planes[PlaneIdx][3] - swDot3(mWorldView[3], Normal));
	}

	if (swBoundingBoxIntersectsVolume(Min, Max, LocalPlanes))
	{
		ObjectBoxTest.Objects.emplace_back(const_cast<swCamera*>(this));
		return;
	}

	Min = swVector3(-SW_CAMERA_TARGET_EDGE, -SW_CAMERA_TARGET_EDGE, -SW_CAMERA_TARGET_EDGE);
	Max = swVector3(SW_CAMERA_TARGET_EDGE, SW_CAMERA_TARGET_EDGE, SW_CAMERA_TARGET_EDGE);

	swMatrix44 WorldView = mWorldView;
	WorldView.SetTranslation(swMultiply30(-mTargetPosition, WorldView));

	for (int PlaneIdx = 0; PlaneIdx < 6; PlaneIdx++)
	{
		const swVector3 Normal = swMultiply30(ObjectBoxTest.Planes[PlaneIdx], WorldView);
		LocalPlanes[PlaneIdx] = swVector4(Normal, ObjectBoxTest.Planes[PlaneIdx][3] - swDot3(WorldView[3], Normal));
	}

	if (swBoundingBoxIntersectsVolume(Min, Max, LocalPlanes))
	{
		ObjectBoxTest.Objects.emplace_back(const_cast<swCamera*>(this));
		return;
	}

	const swMatrix44 ViewWorld = swMatrix44AffineInverse(mWorldView);
	const swVector3 UpVectorPosition = swMultiply31(swVector3(0, 25, 0), ViewWorld);

	WorldView = mWorldView;
	WorldView.SetTranslation(swMultiply30(-UpVectorPosition, WorldView));

	for (int PlaneIdx = 0; PlaneIdx < 6; PlaneIdx++)
	{
		const swVector3 Normal = swMultiply30(ObjectBoxTest.Planes[PlaneIdx], WorldView);
		LocalPlanes[PlaneIdx] = swVector4(Normal, ObjectBoxTest.Planes[PlaneIdx][3] - swDot3(WorldView[3], Normal));
	}

	if (swBoundingBoxIntersectsVolume(Min, Max, LocalPlanes))
	{
		ObjectBoxTest.Objects.emplace_back(const_cast<swCamera*>(this));
		return;
	}
}

void swCamera::InsertTime(swStep Start, swStep Time)
{
	mPosition.InsertTime(Start, Time);
	mTargetPosition.InsertTime(Start, Time);
	mUpVector.InsertTime(Start, Time);
}

void swCamera::RemoveTime(swStep Start, swStep Time)
{
	mPosition.RemoveTime(Start, Time);
	mTargetPosition.RemoveTime(Start, Time);
	mUpVector.RemoveTime(Start, Time);
}

void swCamera::ExpandFarPlane(const std::vector<swVector3>& Points)
{
	if (!IsSimple())
		return;

	float FarDistance = 0.0f;

	for (const swVector3& Point : Points)
		FarDistance = swMaximum(FarDistance, -swMultiply31(Point, mWorldView).z);

	// Orthographic views use four times the camera's far plane.
	if (GetProjection() == swCameraProjection::Orthographic)
		FarDistance /= 4.0f;

	if (FarDistance > DefaultFarPlane && FarDistance > m_zFar)
		m_zFar = ceilf(FarDistance * 1.1f / 10000.0f) * 10000.0f;
}

void swCamera::ZoomExtents(float AspectRatio, const swVector3& Center, const std::vector<swVector3>& Points, swStep Step, bool AddKey, bool AdjustFarPlane)
{
	swVector3 Position, TargetPosition;

	if (GetProjection() == swCameraProjection::Orthographic)
	{
		float MinX = FLT_MAX, MaxX = -FLT_MAX, MinY = FLT_MAX, MaxY = -FLT_MAX;

		for (swVector3 Point : Points)
		{
			Point = swMultiply30(Point, mWorldView);

			MinX = swMinimum(MinX, Point.x);
			MinY = swMinimum(MinY, Point.y);
			MaxX = swMaximum(MaxX, Point.x);
			MaxY = swMaximum(MaxY, Point.y);
		}

		float Width = fabsf(MaxX - MinX);
		float Height = fabsf(MaxY - MinY);

		const swVector3 ViewCenter = swMultiply30(Center, mWorldView);

		float OffsetX = (fabsf(MaxX - ViewCenter.x) - fabsf(ViewCenter.x - MinX)) / 2.0f;
		float OffsetY = (fabsf(MaxY - ViewCenter.y) - fabsf(ViewCenter.y - MinY)) / 2.0f;
		swVector3 ViewOffset(OffsetX, OffsetY, 0.0f);

		swMatrix44 ViewWorldMatrix = swMatrix44AffineInverse(mWorldView);
		ViewWorldMatrix.SetTranslation(swVector3(0, 0, 0));

		swVector3 WorldOffset = swMultiply30(ViewOffset, ViewWorldMatrix);

		if (Width > Height * AspectRatio)
			Height = Width / AspectRatio;

		const float f = Height / (m_fovy * (SW_PI / 180.0f));

		const swVector3 FrontVector(mTargetPosition - mPosition);

		TargetPosition = Center + WorldOffset;
		Position = TargetPosition - swNormalize(FrontVector) * f;
	}
	else
	{
		const swVector3 CenterPosition(mPosition + Center - mTargetPosition);
		const swMatrix44 ProjectionMatrix = swMatrix44Perspective(m_fovy, AspectRatio, m_zNear, m_zFar);

		std::tie(Position, std::ignore) = swZoomExtents(CenterPosition, mWorldView, ProjectionMatrix, Points.data(), Points.size());
		TargetPosition = Center;
	}

	// Back away a little so the fitted geometry never touches the view edges.
	Position = TargetPosition + (Position - TargetPosition) * 1.05f;

	if (IsSimple())
		AddKey = false;

	mPosition.ChangeKey(Position, Step, AddKey);
	mTargetPosition.ChangeKey(TargetPosition, Step, AddKey);

	UpdatePosition(Step);

	if (AdjustFarPlane)
		ExpandFarPlane(Points);
}

void swCamera::ZoomRegion(float AspectRatio, const swVector3& Position, const swVector3& TargetPosition, const swVector3* Corners, swStep Step, bool AddKey)
{
	swVector3 NewPosition;

	if (GetProjection() == swCameraProjection::Orthographic)
	{
		float MinX = FLT_MAX, MaxX = -FLT_MAX, MinY = FLT_MAX, MaxY = -FLT_MAX;

		for (int PointIdx = 0; PointIdx < 2; PointIdx++)
		{
			const swVector3 Point = swMultiply30(Corners[PointIdx], mWorldView);

			MinX = swMinimum(MinX, Point.x);
			MinY = swMinimum(MinY, Point.y);
			MaxX = swMaximum(MaxX, Point.x);
			MaxY = swMaximum(MaxY, Point.y);
		}

		float Width = MaxX - MinX;
		float Height = MaxY - MinY;

		if (Width > Height * AspectRatio)
			Height = Width / AspectRatio;

		const float f = Height / (m_fovy * (SW_PI / 180.0f));

		const swVector3 FrontVector(mTargetPosition - mPosition);
		NewPosition = TargetPosition - swNormalize(FrontVector) * f;
	}
	else
	{
		const swMatrix44 WorldView = swMatrix44LookAt(Position, TargetPosition, mUpVector);
		const swMatrix44 ProjectionMatrix = swMatrix44Perspective(m_fovy, AspectRatio, m_zNear, m_zFar);

		std::tie(NewPosition, std::ignore) = swZoomExtents(Position, WorldView, ProjectionMatrix, Corners, 2);
	}

	if (IsSimple())
		AddKey = false;

	mPosition.ChangeKey(NewPosition, Step, AddKey);
	mTargetPosition.ChangeKey(TargetPosition, Step, AddKey);

	UpdatePosition(Step);
}

void swCamera::Zoom(float Distance, swStep Step, bool AddKey)
{
	swVector3 FrontVector(mPosition - mTargetPosition);
	FrontVector.Normalize();
	FrontVector *= -5.0f * Distance;

	// Don't zoom ortho in if it would cross the ortho focal plane.
	if (GetProjection() == swCameraProjection::Orthographic)
	{
		if ((Distance > 0) && (swDot(mPosition + FrontVector - mTargetPosition, mPosition - mTargetPosition) <= 0))
			return;
	}

	if (IsSimple())
		AddKey = false;

	mPosition.ChangeKey(mPosition + FrontVector, Step, AddKey);

	if (GetProjection() != swCameraProjection::Orthographic)
		mTargetPosition.ChangeKey(mTargetPosition + FrontVector, Step, AddKey);

	UpdatePosition(Step);
}

void swCamera::Pan(const swVector3& Distance, swStep Step, bool AddKey)
{
	if (IsSimple())
		AddKey = false;

	mPosition.ChangeKey(mPosition + Distance, Step, AddKey);
	mTargetPosition.ChangeKey(mTargetPosition + Distance, Step, AddKey);

	UpdatePosition(Step);
}

void swCamera::Orbit(float DistanceX, float DistanceY, const swVector3& CenterPosition, swStep Step, bool AddKey)
{
	const swMatrix44 Inverse = swMatrix44AffineInverse(mWorldView);
	const swMatrix44 TransformY = swMatrix44FromAxisAngle(swVector3(Inverse[0]), DistanceY);

	swVector3 Position = swMultiply31(mPosition - CenterPosition, TransformY) + CenterPosition;
	swVector3 TargetPosition = swMultiply31(mTargetPosition - CenterPosition, TransformY) + CenterPosition;
	swVector3 UpVector = swMultiply31(mUpVector, TransformY);

	const swMatrix44 TransformX = swMatrix44RotationZ(-DistanceX);

	Position = swMultiply31(Position - CenterPosition, TransformX) + CenterPosition;
	TargetPosition = swMultiply31(TargetPosition - CenterPosition, TransformX) + CenterPosition;
	UpVector = swMultiply31(UpVector, TransformX);

	if (IsSimple())
		AddKey = false;

	mPosition.ChangeKey(Position, Step, AddKey);
	mTargetPosition.ChangeKey(TargetPosition, Step, AddKey);
	mUpVector.ChangeKey(UpVector, Step, AddKey);

	UpdatePosition(Step);
}

void swCamera::Roll(float Distance, swStep Step, bool AddKey)
{
	const swVector3 FrontVector(mPosition - mTargetPosition);
	const swMatrix44 Rotation = swMatrix44FromAxisAngle(FrontVector, Distance);

	const swVector3 UpVector = swMultiply30(mUpVector, Rotation);

	if (IsSimple())
		AddKey = false;

	mUpVector.ChangeKey(UpVector, Step, AddKey);

	UpdatePosition(Step);
}

void swCamera::Center(const swVector3& NewCenter, swStep Step, bool AddKey)
{
	const swMatrix44 Inverse = swMatrix44AffineInverse(mWorldView);
	const swVector3 Direction = -swVector3(Inverse[2]);

//	float Yaw, Pitch, Roll;
	float Roll;

	if (fabsf(Direction.z) < 0.9999f)
	{
//		Yaw = atan2f(Direction.y, Direction.x);
//		Pitch = asinf(Direction.z);
		Roll = atan2f(Inverse[0][2], Inverse[1][2]);
	}
	else
	{
//		Yaw = 0.0f;
//		Pitch = asinf(Direction.z);
		Roll = atan2f(Inverse[0][1], Inverse[1][1]);
	}

	swVector3 FrontVector(mPosition - NewCenter);
	const swMatrix44 Rotation = swMatrix44FromAxisAngle(FrontVector, Roll);

	swVector3 UpVector(0, 0, 1), SideVector;
	FrontVector.Normalize();
	if (fabsf(swDot(UpVector, FrontVector)) > 0.99f)
		SideVector = swVector3(-1, 0, 0);
	else
		SideVector = swCross(FrontVector, UpVector);
	UpVector = swCross(SideVector, FrontVector);
	UpVector.Normalize();
	UpVector = swMultiply30(UpVector, Rotation);

	if (IsSimple())
		AddKey = false;

	mTargetPosition.ChangeKey(NewCenter, Step, AddKey);
	mUpVector.ChangeKey(UpVector, Step, AddKey);

	UpdatePosition(Step);
}

void swCamera::SetViewpoint(swViewpoint Viewpoint)
{
	const swVector3 Positions[] =
	{
		swVector3(    0.0f, -1250.0f,     0.0f), // swViewpoint::Front
		swVector3(    0.0f,  1250.0f,     0.0f), // swViewpoint::Back
		swVector3(    0.0f,     0.0f,  1250.0f), // swViewpoint::Top
		swVector3(    0.0f,     0.0f, -1250.0f), // swViewpoint::Bottom
		swVector3( 1250.0f,     0.0f,     0.0f), // swViewpoint::Left
		swVector3(-1250.0f,     0.0f,     0.0f), // swViewpoint::Right
		swVector3(  375.0f,  -375.0f,   187.5f)  // swViewpoint::Home
	};

	const swVector3 Ups[] =
	{
		swVector3(0.0f, 0.0f, 1.0f),
		swVector3(0.0f, 0.0f, 1.0f),
		swVector3(0.0f, 1.0f, 0.0f),
		swVector3(0.0f,-1.0f, 0.0f),
		swVector3(0.0f, 0.0f, 1.0f),
		swVector3(0.0f, 0.0f, 1.0f),
		swVector3(0.2357f, -0.2357f, 0.94281f)
	};

	mPosition.ChangeKey(Positions[static_cast<int>(Viewpoint)], 1, false);
	mTargetPosition.ChangeKey(swVector3(0, 0, 0), 1, false);
	mUpVector.ChangeKey(Ups[static_cast<int>(Viewpoint)], 1, false);

	UpdatePosition(1);
}

void swCamera::SetViewpoint(const swVector3& Position)
{
	swVector3 UpVector(0, 0, 1), FrontVector(Position), SideVector;
	FrontVector.Normalize();
	if (fabsf(swDot(UpVector, FrontVector)) > 0.99f)
		SideVector = swVector3(-1, 0, 0);
	else
		SideVector = swCross(FrontVector, UpVector);
	UpVector = swCross(SideVector, FrontVector);
	UpVector.Normalize();

	mPosition.ChangeKey(Position, 1, false);
	mTargetPosition.ChangeKey(swVector3(0, 0, 0), 1, false);
	mUpVector.ChangeKey(UpVector, 1, false);

	UpdatePosition(1);
}

void swCamera::SetViewpoint(const swVector3& Position, const swVector3& TargetPosition, const swVector3& Up)
{
	const swVector3 Direction = TargetPosition - Position;
	swVector3 UpVector, SideVector;
	SideVector = swCross(Direction, Up);
	UpVector = swCross(SideVector, Direction);
	UpVector.Normalize();

	mPosition.ChangeKey(Position, 1, false);
	mTargetPosition.ChangeKey(TargetPosition, 1, false);
	mUpVector.ChangeKey(UpVector, 1, false);

	UpdatePosition(1);
}

void swCamera::SetAngles(float Latitude, float Longitude, float Distance)
{
	swVector3 Position = swVector3(0, -1, 0);
	swVector3 TargetPosition = swVector3(0, 0, 0);
	swVector3 UpVector = swVector3(0, 0, 1);

	const swMatrix33 LongitudeMatrix = swMatrix33RotationZ(SW_DEGREES_TO_RADIANS * Longitude);
	Position = swMultiply(Position, LongitudeMatrix);

	const swVector3 SideVector = swMultiply(swVector3(-1, 0, 0), LongitudeMatrix);
	const swMatrix33 LatitudeMatrix = swMatrix33FromAxisAngle(SideVector, SW_DEGREES_TO_RADIANS * Latitude);
	Position = swMultiply(Position, LatitudeMatrix) * Distance;
	UpVector = swMultiply(UpVector, LatitudeMatrix);

	mPosition.ChangeKey(Position, 1, false);
	mTargetPosition.ChangeKey(TargetPosition, 1, false);
	mUpVector.ChangeKey(UpVector, 1, false);

	UpdatePosition(1);
}

void swCamera::GetAngles(float& Latitude, float& Longitude, float& Distance) const
{
	swVector3 FrontVector(mPosition - mTargetPosition);
	const swVector3 X(1, 0, 0);
	const swVector3 Y(0, 1, 0);
	const swVector3 Z(0, 0, 1);

	FrontVector.Normalize();
	Latitude = acos(swDot(-FrontVector, Z)) * SW_RADIANS_TO_DEGREES - 90.0f;

	const swVector3 CameraXY = -swNormalize(swVector3(FrontVector.x, FrontVector.y, 0.0f));
	Longitude = acos(swDot(CameraXY, Y)) * SW_RADIANS_TO_DEGREES;

	if (swDot(CameraXY, X) > 0)
		Longitude = -Longitude;

	Distance = swLength(mPosition);
}
