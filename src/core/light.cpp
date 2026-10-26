#include "sw_global.h"
#include "sw_math.h"
#include "sw_string.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "light.h"
#include "sw_application.h"
#include "sw_context.h"

#define SW_LIGHT_SPHERE_RADIUS 5.0f
#define SW_LIGHT_TARGET_RADIUS 2.5f
#define SW_LIGHT_SPOT_CONE_HEIGHT 10.0f
#define SW_LIGHT_SPOT_CONE_RADIUS 7.5f
#define SW_LIGHT_DIRECTIONAL_RADIUS 5.0f
#define SW_LIGHT_DIRECTIONAL_HEIGHT 7.5f

#define SW_LIGHT_POSITION_EDGE 7.5f

static const std::array<QLatin1String, static_cast<int>(swLightType::Count)> gLightTypes = { QLatin1String("POINT"), QLatin1String("SPOT"), QLatin1String("DIRECTIONAL"), QLatin1String("AREA") };
static const std::array<QLatin1String, static_cast<int>(swLightAreaShape::Count)> gLightAreaShapes = { QLatin1String("RECTANGLE"), QLatin1String("SQUARE"), QLatin1String("DISK"), QLatin1String("ELLIPSE") };

swLight::swLight()
    : swObject(swObjectType::Light)
{
}

swLight::swLight(const swVector3& Position, swLightType LightType)
	: swObject(swObjectType::Light), mLightType(LightType)
{
	if (LightType == swLightType::Directional)
		mBlenderPower = swObjectProperty<float>(1.0f);

	mPosition.SetValue(Position);

	swLight::UpdatePosition(1);
}

QString swLight::GetLightTypeString(swLightType LightType)
{
	switch (LightType)
	{
	case swLightType::Point:
		return QT_TRANSLATE_NOOP("Light Types", "Point Light");

	case swLightType::Spot:
		return QT_TRANSLATE_NOOP("Light Types", "Spotlight");

	case swLightType::Directional:
		return QT_TRANSLATE_NOOP("Light Types", "Directional Light");

	case swLightType::Area:
		return QT_TRANSLATE_NOOP("Light Types", "Area Light");

	case swLightType::Count:
		break;
	}

	return QString();
}

QStringList swLight::GetLightTypeStrings()
{
	QStringList LightTypes;

	for (int LightTypeIndex = 0; LightTypeIndex < static_cast<int>(swLightType::Count); LightTypeIndex++)
		LightTypes.push_back(GetLightTypeString(static_cast<swLightType>(LightTypeIndex)));

	return LightTypes;
}

QString swLight::GetAreaShapeString(swLightAreaShape LightAreaShape)
{
	switch (LightAreaShape)
	{
	case swLightAreaShape::Rectangle:
		return QT_TRANSLATE_NOOP("Light Shapes", "Rectangle");

	case swLightAreaShape::Square:
		return QT_TRANSLATE_NOOP("Light Shapes", "Square");

	case swLightAreaShape::Disk:
		return QT_TRANSLATE_NOOP("Light Shapes", "Disk");

	case swLightAreaShape::Ellipse:
		return QT_TRANSLATE_NOOP("Light Shapes", "Ellipse");

	case swLightAreaShape::Count:
		break;
	}

	return QString();
}

QStringList swLight::GetAreaShapeStrings()
{
	QStringList AreaShapes;

	for (int AreaShapeIndex = 0; AreaShapeIndex < static_cast<int>(swLightAreaShape::Count); AreaShapeIndex++)
		AreaShapes.push_back(GetAreaShapeString(static_cast<swLightAreaShape>(AreaShapeIndex)));

	return AreaShapes;
}

void swLight::SaveLDraw(QTextStream& Stream) const
{
	const QLatin1String LineEnding("\r\n");

	if (!mCastShadow)
		Stream << QLatin1String("0 !LEOCAD LIGHT SHADOWLESS") << LineEnding;

	mPosition.Save(Stream, "LIGHT", "POSITION", true);
	mRotation.Save(Stream, "LIGHT", "ROTATION", true);
	mColor.Save(Stream, "LIGHT", "COLOR", true);
	mBlenderRadius.Save(Stream, "LIGHT", "BLENDER_RADIUS", true);
	mBlenderAngle.Save(Stream, "LIGHT", "BLENDER_ANGLE", true);
	mAreaSizeX.Save(Stream, "LIGHT", "AREA_SIZE_X", true);
	mAreaSizeY.Save(Stream, "LIGHT", "AREA_SIZE_Y", true);
	mBlenderPower.Save(Stream, "LIGHT", "BLENDER_POWER", true);
	mPOVRayPower.Save(Stream, "LIGHT", "POVRAY_POWER", true);
	mPOVRayFadeDistance.Save(Stream, "LIGHT", "POVRAY_FADE_DISTANCE", true);
	mPOVRayFadePower.Save(Stream, "LIGHT", "POVRAY_FADE_POWER", true);

	switch (mLightType)
	{
	case swLightType::Count:
	case swLightType::Point:
		break;

	case swLightType::Spot:
		mSpotConeAngle.Save(Stream, "LIGHT", "SPOT_CONE_ANGLE", true);
		mSpotPenumbraAngle.Save(Stream, "LIGHT", "SPOT_PENUMBRA_ANGLE", true);
		mPOVRaySpotTightness.Save(Stream, "LIGHT", "POVRAY_SPOT_TIGHTNESS", true);
		break;

	case swLightType::Directional:
		break;

	case swLightType::Area:
		Stream << QLatin1String("0 !LEOCAD LIGHT AREA_SHAPE ") << gLightAreaShapes[static_cast<int>(mAreaShape)] << LineEnding;
		mPOVRayAreaGridX.Save(Stream, "LIGHT", "POVRAY_AREA_GRID_X", true);
		mPOVRayAreaGridY.Save(Stream, "LIGHT", "POVRAY_AREA_GRID_Y", true);
		break;
	}

	Stream << QLatin1String("0 !LEOCAD LIGHT TYPE ") << gLightTypes[static_cast<int>(mLightType)] << QLatin1String(" NAME ") << mName << LineEnding;
}

void swLight::CreateName(const std::vector<std::unique_ptr<swLight>>& Lights)
{
	if (!mName.isEmpty())
	{
		bool Found = false;

		for (const std::unique_ptr<swLight>& Light : Lights)
		{
			if (Light->GetName() == mName)
			{
				Found = true;
				break;
			}
		}

		if (!Found)
			return;
	}

	int MaxLightNumber = 0;

	QString Prefix;

	switch (mLightType)
	{
	case swLightType::Point:
		Prefix = QLatin1String("Point Light ");
		break;

	case swLightType::Spot:
		Prefix = QLatin1String("Spot Light ");
		break;

	case swLightType::Directional:
		Prefix = QLatin1String("Directional Light ");
		break;

	case swLightType::Area:
		Prefix = QLatin1String("Area Light ");
		break;

	case swLightType::Count:
		break;
	}

	for (const std::unique_ptr<swLight>& Light : Lights)
		MaxLightNumber = qMax(MaxLightNumber, swGetDefaultNameNumber(Light->GetName(), Prefix));

	mName = swFormatDefaultName(Prefix, MaxLightNumber + 1);
}

bool swLight::ParseLDrawLine(QTextStream& Stream)
{
	while (!Stream.atEnd())
	{
		QString Token;
		Stream >> Token;

		if (mPosition.Load(Stream, Token, "POSITION"))
			continue;
		else if (mRotation.Load(Stream, Token, "ROTATION"))
			continue;
		else if (mColor.Load(Stream, Token, "COLOR"))
			continue;
		else if (mBlenderRadius.Load(Stream, Token, "BLENDER_RADIUS"))
			continue;
		else if (mBlenderAngle.Load(Stream, Token, "BLENDER_ANGLE"))
			continue;
		else if (mAreaSizeX.Load(Stream, Token, "AREA_SIZE_X"))
			continue;
		else if (mAreaSizeY.Load(Stream, Token, "AREA_SIZE_Y"))
			continue;
		else if (mBlenderPower.Load(Stream, Token, "BLENDER_POWER"))
			continue;
		else if (mPOVRayPower.Load(Stream, Token, "POVRAY_POWER"))
			continue;
		else if (mPOVRayFadeDistance.Load(Stream, Token, "POVRAY_FADE_DISTANCE"))
			continue;
		else if (mPOVRayFadePower.Load(Stream, Token, "POVRAY_FADE_POWER"))
			continue;
		else if (mSpotConeAngle.Load(Stream, Token, "SPOT_CONE_ANGLE"))
			continue;
		else if (mSpotPenumbraAngle.Load(Stream, Token, "SPOT_PENUMBRA_ANGLE"))
			continue;
		else if (mPOVRaySpotTightness.Load(Stream, Token, "POVRAY_SPOT_TIGHTNESS"))
			continue;
		else if (mPOVRayAreaGridX.Load(Stream, Token, "POVRAY_AREA_GRID_X"))
			continue;
		else if (mPOVRayAreaGridY.Load(Stream, Token, "POVRAY_AREA_GRID_Y"))
			continue;
		else if (Token == QLatin1String("AREA_SHAPE"))
		{
			QString AreaShape;
			Stream >> AreaShape;

			for (size_t ShapeIndex = 0; ShapeIndex < gLightAreaShapes.size(); ShapeIndex++)
			{
				if (AreaShape == gLightAreaShapes[ShapeIndex])
				{
					mAreaShape = static_cast<swLightAreaShape>(ShapeIndex);
					break;
				}
			}
		}
		else if (Token == QLatin1String("TYPE"))
		{
			QString Type;
			Stream >> Type;

			for (size_t TypeIndex = 0; TypeIndex < gLightTypes.size(); TypeIndex++)
			{
				if (Type == gLightTypes[TypeIndex])
				{
					mLightType = static_cast<swLightType>(TypeIndex);
					break;
				}
			}
		}
		else if (Token == QLatin1String("SHADOWLESS"))
		{
			mCastShadow = false;
		}
		else if (Token == QLatin1String("NAME"))
		{
			mName = Stream.readAll().trimmed();
			mName.replace("\"", "");

			return true;
		}
	}

	return false;
}

void swLight::CompareBoundingBox(swVector3& Min, swVector3& Max)
{
	const swVector3 Point = mWorldMatrix.GetTranslation();

	// TODO: this should check the entire mesh

	Min = swMin(Point, Min);
	Max = swMax(Point, Max);
}

void swLight::RayTest(swObjectRayTest& ObjectRayTest) const
{
	if (IsPointLight())
	{
		float Distance;

		if (swSphereRayMinIntersectDistance(mWorldMatrix.GetTranslation(), SW_LIGHT_SPHERE_RADIUS, ObjectRayTest.Start, ObjectRayTest.End, &Distance) && (Distance < ObjectRayTest.Distance))
		{
			ObjectRayTest.ObjectSection.Object = const_cast<swLight*>(this);
			ObjectRayTest.ObjectSection.Section = SW_LIGHT_SECTION_POSITION;
			ObjectRayTest.Distance = Distance;
		}

		return;
	}

	if (mLightType == swLightType::Spot)
	{
		const swVector3 Direction = -swVector3(mWorldMatrix[2]);
		const swVector3 Position = mWorldMatrix.GetTranslation() - Direction * SW_LIGHT_SPOT_CONE_HEIGHT;
		float Distance;

		if (swConeRayMinIntersectDistance(Position, Direction, SW_LIGHT_SPOT_CONE_RADIUS, SW_LIGHT_SPOT_CONE_HEIGHT, ObjectRayTest.Start, ObjectRayTest.End, &Distance) && (Distance < ObjectRayTest.Distance))
		{
			ObjectRayTest.ObjectSection.Object = const_cast<swLight*>(this);
			ObjectRayTest.ObjectSection.Section = SW_LIGHT_SECTION_POSITION;
			ObjectRayTest.Distance = Distance;
		}
	}
	else if (mLightType == swLightType::Area)
	{
		const swVector3 Direction = -swVector3(mWorldMatrix[2]);
		const swVector3 Position = mWorldMatrix.GetTranslation();
		const swVector4 Plane(Direction, -swDot(Direction, Position));
		swVector3 Intersection;

		if (swLineSegmentPlaneIntersection(&Intersection, ObjectRayTest.Start, ObjectRayTest.End, Plane))
		{
			const swVector3 XAxis = swVector3(mWorldMatrix[0]);
			const swVector3 YAxis = swVector3(mWorldMatrix[1]);
			swVector3 IntersectionDirection = Intersection - Position;

			float x = swDot(IntersectionDirection, XAxis);
			float y = swDot(IntersectionDirection, YAxis);
			const swVector2 Size = { mAreaSizeX, mAreaSizeY };

			if (fabsf(x) < Size.x / 2.0f && fabsf(y) < Size.y / 2.0f)
			{
				float Distance = swLength(Intersection - ObjectRayTest.Start);

				if (Distance < ObjectRayTest.Distance)
				{
					ObjectRayTest.ObjectSection.Object = const_cast<swLight*>(this);
					ObjectRayTest.ObjectSection.Section = SW_LIGHT_SECTION_POSITION;
					ObjectRayTest.Distance = Distance;
				}
			}
		}
	}

	const swMatrix44 InverseWorldMatrix = swMatrix44AffineInverse(mWorldMatrix);
	swVector3 Start = swMul31(ObjectRayTest.Start, InverseWorldMatrix);
	swVector3 End = swMul31(ObjectRayTest.End, InverseWorldMatrix);

	float Distance;
	swVector3 Plane;

	if (mLightType == swLightType::Directional)
	{
		if (swCylinderRayMinIntersectDistance(SW_LIGHT_DIRECTIONAL_RADIUS, SW_LIGHT_DIRECTIONAL_HEIGHT, Start, End, &Distance) && (Distance < ObjectRayTest.Distance))
		{
			ObjectRayTest.ObjectSection.Object = const_cast<swLight*>(this);
			ObjectRayTest.ObjectSection.Section = SW_LIGHT_SECTION_POSITION;
			ObjectRayTest.Distance = Distance;
			ObjectRayTest.PieceRayTest.Plane = Plane;
		}
	}

	if (IsSelected())
	{
		if (swSphereRayMinIntersectDistance(swMul31(swVector3(0,0,-mTargetDistance), mWorldMatrix), SW_LIGHT_TARGET_RADIUS, ObjectRayTest.Start, ObjectRayTest.End, &Distance) && (Distance < ObjectRayTest.Distance))
		{
			ObjectRayTest.ObjectSection.Object = const_cast<swLight*>(this);
			ObjectRayTest.ObjectSection.Section = SW_LIGHT_SECTION_TARGET;
			ObjectRayTest.Distance = Distance;
		}
	}
}

void swLight::BoxTest(swObjectBoxTest& ObjectBoxTest) const
{
	if (IsPointLight())
	{
		for (int PlaneIdx = 0; PlaneIdx < 6; PlaneIdx++)
			if (swDot3(mWorldMatrix.GetTranslation(), ObjectBoxTest.Planes[PlaneIdx]) + ObjectBoxTest.Planes[PlaneIdx][3] > SW_LIGHT_SPHERE_RADIUS)
				return;

		ObjectBoxTest.Objects.emplace_back(const_cast<swLight*>(this));
		return;
	}

	swMatrix44 InverseWorldMatrix = swMatrix44AffineInverse(mWorldMatrix);
	swVector4 LocalPlanes[6];

	for (int PlaneIndex = 0; PlaneIndex < 6; PlaneIndex++)
	{
		const swVector3 PlaneNormal = swMul30(ObjectBoxTest.Planes[PlaneIndex], InverseWorldMatrix);
		LocalPlanes[PlaneIndex] = swVector4(PlaneNormal, ObjectBoxTest.Planes[PlaneIndex][3] - swDot3(InverseWorldMatrix[3], PlaneNormal));
	}

	// TODO: Check meshes after the bounding fox test.
	if (mLightType == swLightType::Spot)
	{
		swVector3 Min(-SW_LIGHT_SPOT_CONE_RADIUS, -SW_LIGHT_SPOT_CONE_RADIUS, 0);
		swVector3 Max( SW_LIGHT_SPOT_CONE_RADIUS,  SW_LIGHT_SPOT_CONE_RADIUS, SW_LIGHT_SPOT_CONE_HEIGHT);

		if (swBoundingBoxIntersectsVolume(Min, Max, LocalPlanes))
		{
			ObjectBoxTest.Objects.emplace_back(const_cast<swLight*>(this));
			return;
		}
	}
	else if (mLightType == swLightType::Area)
	{
		swVector3 Min(-mAreaSizeX / 2.0f, -mAreaSizeY / 2.0f, 0);
		swVector3 Max( mAreaSizeX / 2.0f,  mAreaSizeY / 2.0f, 0);

		if (swBoundingBoxIntersectsVolume(Min, Max, LocalPlanes))
		{
			ObjectBoxTest.Objects.emplace_back(const_cast<swLight*>(this));
			return;
		}
	}
	else if (mLightType == swLightType::Directional)
	{
		swVector3 Min(-SW_LIGHT_DIRECTIONAL_RADIUS, -SW_LIGHT_DIRECTIONAL_RADIUS, 0);
		swVector3 Max( SW_LIGHT_DIRECTIONAL_RADIUS,  SW_LIGHT_DIRECTIONAL_RADIUS, SW_LIGHT_DIRECTIONAL_HEIGHT);

		if (swBoundingBoxIntersectsVolume(Min, Max, LocalPlanes))
		{
			ObjectBoxTest.Objects.emplace_back(const_cast<swLight*>(this));
			return;
		}
	}

	if (IsSelected())
	{
		swVector3 TargetPosition = swMul31(swVector3(0.0f, 0.0f, -mTargetDistance), mWorldMatrix);
		bool Outside = false;

		for (int PlaneIdx = 0; PlaneIdx < 6; PlaneIdx++)
		{
			if (swDot3(TargetPosition, ObjectBoxTest.Planes[PlaneIdx]) + ObjectBoxTest.Planes[PlaneIdx][3] > SW_LIGHT_TARGET_RADIUS)
			{
				Outside = true;
				break;
			}
		}

		if (!Outside)
		{
			ObjectBoxTest.Objects.emplace_back(const_cast<swLight*>(this));
			return;
		}
	}
}

void swLight::MoveSelected(swStep Step, bool AddKey, const swVector3& Distance, bool FirstMove)
{
	const quint32 Section = GetFocusSection();

	if (Section == SW_LIGHT_SECTION_POSITION || Section == SW_LIGHT_SECTION_INVALID)
	{
		const swVector3 Position = mWorldMatrix.GetTranslation() + Distance;

		SetPosition(Position, Step, AddKey);

		mWorldMatrix.SetTranslation(Position);
	}
	else
	{
		if (FirstMove)
			mTargetMovePosition = swMul31(swVector3(0.0f, 0.0f, -mTargetDistance), mWorldMatrix);

		mTargetMovePosition += Distance;

		swVector3 CurrentDirection = -swNormalize(mTargetMovePosition - mWorldMatrix.GetTranslation());
		swMatrix33 WorldMatrix;

		WorldMatrix.r[0] = swCross(swVector3(mWorldMatrix.r[1]), CurrentDirection);
		WorldMatrix.r[1] = swCross(CurrentDirection, WorldMatrix.r[0]);
		WorldMatrix.r[2] = CurrentDirection;

		WorldMatrix.Orthonormalize();

		SetRotation(WorldMatrix, Step, AddKey);

		mWorldMatrix = swMatrix44(WorldMatrix, mWorldMatrix.GetTranslation());
	}
}

void swLight::Rotate(swStep Step, bool AddKey, const swMatrix33& RotationMatrix, const swVector3& Center, const swMatrix33& RotationFrame)
{
	if (IsPointLight())
		return;

	if (GetFocusSection() != SW_LIGHT_SECTION_POSITION && GetFocusSection() != SW_LIGHT_SECTION_INVALID)
		return;

	swVector3 Distance = mWorldMatrix.GetTranslation() - Center;
	const swMatrix33 LocalToWorldMatrix = swMatrix33(mWorldMatrix);

	const swMatrix33 LocalToFocusMatrix = swMul(LocalToWorldMatrix, RotationFrame);
	swMatrix33 NewLocalToWorldMatrix = swMul(LocalToFocusMatrix, RotationMatrix);

	const swMatrix33 WorldToLocalMatrix = swMatrix33AffineInverse(LocalToWorldMatrix);

	Distance = swMul(Distance, WorldToLocalMatrix);
	Distance = swMul(Distance, NewLocalToWorldMatrix);

	NewLocalToWorldMatrix.Orthonormalize();

	SetPosition(Center + Distance, Step, AddKey);
	SetRotation(NewLocalToWorldMatrix, Step, AddKey);
}

bool swLight::SetName(const QString& Name)
{
	if (mName == Name)
		return false;

	mName = Name;

	return true;
}

bool swLight::SetLightType(swLightType LightType)
{
	if (static_cast<int>(LightType) < 0 || LightType >= swLightType::Count)
		return false;

	if (mLightType == LightType)
		return false;

	mLightType = LightType;

	return true;
}

bool swLight::SetColor(const swVector3& Color, swStep Step, bool AddKey)
{
	return mColor.ChangeKey(Color, Step, AddKey);
}

bool swLight::SetPOVRayFadeDistance(float Distance, swStep Step, bool AddKey)
{
	return mPOVRayFadeDistance.ChangeKey(Distance, Step, AddKey);
}

bool swLight::SetPOVRayFadePower(float Power, swStep Step, bool AddKey)
{
	return mPOVRayFadePower.ChangeKey(Power, Step, AddKey);
}

bool swLight::SetSpotConeAngle(float Angle, swStep Step, bool AddKey)
{
	return mSpotConeAngle.ChangeKey(Angle, Step, AddKey);
}

bool swLight::SetSpotPenumbraAngle(float Angle, swStep Step, bool AddKey)
{
	return mSpotPenumbraAngle.ChangeKey(Angle, Step, AddKey);
}

bool swLight::SetSpotPOVRayTightness(float Tightness, swStep Step, bool AddKey)
{
	return mPOVRaySpotTightness.ChangeKey(Tightness, Step, AddKey);
}

bool swLight::SetAreaShape(swLightAreaShape AreaShape)
{
	if (static_cast<int>(AreaShape) < 0 || AreaShape >= swLightAreaShape::Count)
		return false;

	if (mAreaShape != AreaShape)
	{
		mAreaShape = AreaShape;
		return true;
	}

	return false;
}

bool swLight::SetAreaPOVRayGridX(int AreaGrid, swStep Step, bool AddKey)
{
	mPOVRayAreaGridX.ChangeKey(AreaGrid, Step, AddKey);

	return true;
}

bool swLight::SetAreaPOVRayGridY(int AreaGrid, swStep Step, bool AddKey)
{
	mPOVRayAreaGridY.ChangeKey(AreaGrid, Step, AddKey);

	return true;
}

bool swLight::SetBlenderRadius(float Radius, swStep Step, bool AddKey)
{
	return mBlenderRadius.ChangeKey(Radius, Step, AddKey);
}

bool swLight::SetBlenderAngle(float Angle, swStep Step, bool AddKey)
{
	return mBlenderAngle.ChangeKey(Angle * SW_DTOR, Step, AddKey);
}

bool swLight::SetAreaSizeX(float Size, swStep Step, bool AddKey)
{
	return mAreaSizeX.ChangeKey(Size, Step, AddKey);
}

bool swLight::SetAreaSizeY(float Size, swStep Step, bool AddKey)
{
	return mAreaSizeY.ChangeKey(Size, Step, AddKey);
}

bool swLight::SetBlenderPower(float Power, swStep Step, bool AddKey)
{
	return mBlenderPower.ChangeKey(Power, Step, AddKey);
}

bool swLight::SetPOVRayPower(float Power, swStep Step, bool AddKey)
{
	return mPOVRayPower.ChangeKey(Power, Step, AddKey);
}

bool swLight::SetCastShadow(bool CastShadow)
{
	if (mCastShadow != CastShadow)
	{
		mCastShadow = CastShadow;
		return true;
	}

	return false;
}

void swLight::InsertTime(swStep Start, swStep Time)
{
	mPosition.InsertTime(Start, Time);
	mRotation.InsertTime(Start, Time);
	mColor.InsertTime(Start, Time);
	mSpotConeAngle.InsertTime(Start, Time);
	mSpotPenumbraAngle.InsertTime(Start, Time);
	mPOVRaySpotTightness.InsertTime(Start, Time);
	mPOVRayAreaGridX.InsertTime(Start, Time);
	mPOVRayAreaGridY.InsertTime(Start, Time);
	mBlenderRadius.InsertTime(Start, Time);
	mBlenderAngle.InsertTime(Start, Time);
	mAreaSizeX.InsertTime(Start, Time);
	mAreaSizeY.InsertTime(Start, Time);
	mBlenderPower.InsertTime(Start, Time);
	mPOVRayPower.InsertTime(Start, Time);
	mPOVRayFadeDistance.InsertTime(Start, Time);
	mPOVRayFadePower.InsertTime(Start, Time);
}

void swLight::RemoveTime(swStep Start, swStep Time)
{
	mPosition.RemoveTime(Start, Time);
	mRotation.RemoveTime(Start, Time);
	mColor.RemoveTime(Start, Time);
	mSpotConeAngle.RemoveTime(Start, Time);
	mSpotPenumbraAngle.RemoveTime(Start, Time);
	mPOVRaySpotTightness.RemoveTime(Start, Time);
	mPOVRayAreaGridX.RemoveTime(Start, Time);
	mPOVRayAreaGridY.RemoveTime(Start, Time);
	mBlenderRadius.RemoveTime(Start, Time);
	mBlenderAngle.RemoveTime(Start, Time);
	mAreaSizeX.RemoveTime(Start, Time);
	mAreaSizeY.RemoveTime(Start, Time);
	mBlenderPower.RemoveTime(Start, Time);
	mPOVRayPower.RemoveTime(Start, Time);
	mPOVRayFadeDistance.RemoveTime(Start, Time);
	mPOVRayFadePower.RemoveTime(Start, Time);
}

void swLight::UpdatePosition(swStep Step)
{
	mPosition.Update(Step);
	mRotation.Update(Step);
	mColor.Update(Step);
	mSpotConeAngle.Update(Step);
	mSpotPenumbraAngle.Update(Step);
	mPOVRaySpotTightness.Update(Step);
	mPOVRayAreaGridX.Update(Step);
	mPOVRayAreaGridY.Update(Step);
	mBlenderRadius.Update(Step);
	mBlenderAngle.Update(Step);
	mAreaSizeX.Update(Step);
	mAreaSizeY.Update(Step);
	mBlenderPower.Update(Step);
	mPOVRayPower.Update(Step);
	mPOVRayFadeDistance.Update(Step);
	mPOVRayFadePower.Update(Step);

	if (IsPointLight())
	{
		mWorldMatrix = swMatrix44Translation(mPosition);
	}
	else
	{
		mWorldMatrix = swMatrix44(mRotation, mPosition);
	}
}

void swLight::DrawInterface(swContext* Context, const swScene& Scene) const
{
	Q_UNUSED(Scene);
	Context->SetMaterial(swMaterialType::UnlitColor);

	switch (mLightType)
	{
	case swLightType::Point:
		DrawPointLight(Context);
		break;

	case swLightType::Spot:
		DrawSpotLight(Context);
		break;

	case swLightType::Directional:
		DrawDirectionalLight(Context);
		break;

	case swLightType::Area:
		DrawAreaLight(Context);
		break;

	case swLightType::Count:
		break;
	}
}

void swLight::DrawPointLight(swContext* Context) const
{
	SetupLightMatrix(Context);

	DrawSphere(Context, swVector3(0.0f, 0.0f, 0.0f), SW_LIGHT_SPHERE_RADIUS);
}

void swLight::DrawSpotLight(swContext* Context) const
{
	SetupLightMatrix(Context);

	constexpr int ConeEdges = 8;
	float Verts[(ConeEdges + 1) * 3];
	float* CurVert = Verts;

	for (int EdgeIndex = 0; EdgeIndex < ConeEdges; EdgeIndex++)
	{
		float c = cosf((float)EdgeIndex / ConeEdges * SW_2PI) * SW_LIGHT_SPOT_CONE_RADIUS;
		float s = sinf((float)EdgeIndex / ConeEdges * SW_2PI) * SW_LIGHT_SPOT_CONE_RADIUS;

		*CurVert++ = c;
		*CurVert++ = s;
		*CurVert++ = 0.0f;
	}

	*CurVert++ = 0.0f;
	*CurVert++ = 0.0f;
	*CurVert++ = SW_LIGHT_SPOT_CONE_HEIGHT;

	Context->SetVertexBufferPointer(Verts);
	Context->SetVertexFormatPosition(3);

	const GLushort Indices[(ConeEdges + 4) * 2] =
	{
		0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 0,
		0, 8, 2, 8, 4, 8, 6, 8,
	};

	Context->SetIndexBufferPointer(Indices);

	Context->DrawIndexedPrimitives(GL_LINES, (ConeEdges + 4) * 2, GL_UNSIGNED_SHORT, 0);

	if (IsSelected())
	{
		DrawCone(Context, mTargetDistance);
		DrawTarget(Context);
	}
}

void swLight::DrawDirectionalLight(swContext* Context) const
{
	SetupLightMatrix(Context);

	DrawCylinder(Context, SW_LIGHT_DIRECTIONAL_RADIUS, SW_LIGHT_DIRECTIONAL_HEIGHT);

	if (IsSelected())
		DrawTarget(Context);
}

void swLight::DrawAreaLight(swContext* Context) const
{
	SetupLightMatrix(Context);

	if (mAreaShape == swLightAreaShape::Square || mAreaShape == swLightAreaShape::Rectangle)
	{
		float Verts[4 * 3];
		float* CurVert = Verts;
		const swVector2 Size = { mAreaSizeX, mAreaShape == swLightAreaShape::Square ? mAreaSizeX : mAreaSizeY };

		*CurVert++ = -Size.x / 2.0f;
		*CurVert++ = -Size.y / 2.0f;
		*CurVert++ = 0.0f;

		*CurVert++ =  Size.x / 2.0f;
		*CurVert++ = -Size.y / 2.0f;
		*CurVert++ = 0.0f;

		*CurVert++ =  Size.x / 2.0f;
		*CurVert++ =  Size.y / 2.0f;
		*CurVert++ = 0.0f;

		*CurVert++ = -Size.x / 2.0f;
		*CurVert++ =  Size.y / 2.0f;
		*CurVert++ = 0.0f;

		Context->SetVertexBufferPointer(Verts);
		Context->SetVertexFormatPosition(3);

		const GLushort Indices[(4 + 2) * 2] =
		{
			0, 1, 1, 2, 2, 3, 3, 0,
			0, 2, 1, 3,
		};

		Context->SetIndexBufferPointer(Indices);

		Context->DrawIndexedPrimitives(GL_LINES, (4 + 2) * 2, GL_UNSIGNED_SHORT, 0);
	}
	else
	{
		constexpr int CircleEdges = 16;

		float Verts[CircleEdges * 3];
		float* CurVert = Verts;

		for (int EdgeIndex = 0; EdgeIndex < CircleEdges; EdgeIndex++)
		{
			const swVector2 Size = { mAreaSizeX, mAreaShape == swLightAreaShape::Disk ? mAreaSizeX : mAreaSizeY };
			float c = cosf((float)EdgeIndex / CircleEdges * SW_2PI) * Size.x / 2.0f;
			float s = sinf((float)EdgeIndex / CircleEdges * SW_2PI) * Size.y / 2.0f;

			*CurVert++ = c;
			*CurVert++ = s;
			*CurVert++ = 0.0f;
		}

		Context->SetVertexBufferPointer(Verts);
		Context->SetVertexFormatPosition(3);

		const GLushort Indices[(CircleEdges + 2) * 2] =
		{
			0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8,
			8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15, 0,
			0, 8, 4, 12
		};

		Context->SetIndexBufferPointer(Indices);

		Context->DrawIndexedPrimitives(GL_LINES, (CircleEdges + 2) * 2, GL_UNSIGNED_SHORT, 0);
	}

	if (IsSelected())
		DrawTarget(Context);
}

void swLight::SetupLightMatrix(swContext* Context) const
{
	Context->SetWorldMatrix(mWorldMatrix);

	const swPreferences& Preferences = swGetPreferences();
	const float LineWidth = Preferences.mLineWidth;

	if (IsSelected())
	{
		const swVector4 SelectedColor = swVector4FromColor(Preferences.mObjectSelectedColor);
		const swVector4 FocusedColor = swVector4FromColor(Preferences.mObjectFocusedColor);

		Context->SetLineWidth(2.0f * LineWidth);

		if (IsFocused(SW_LIGHT_SECTION_POSITION))
			Context->SetColor(FocusedColor);
		else
			Context->SetColor(SelectedColor);
	}
	else
	{
		const swVector4 LightColor = swVector4FromColor(Preferences.mLightColor);

		Context->SetLineWidth(LineWidth);
		Context->SetColor(LightColor);
	}
}

void swLight::DrawSphere(swContext* Context, const swVector3& Center, float Radius) const
{
	constexpr int Slices = 6;
	constexpr int NumIndices = 3 * Slices + 6 * Slices * (Slices - 2) + 3 * Slices;
	constexpr int NumVertices = (Slices - 1) * Slices + 2;
	swVector3 Vertices[NumVertices];
	quint16 Indices[NumIndices];

	swVector3* Vertex = Vertices;
	quint16* Index = Indices;

	*Vertex++ = Center + swVector3(0, 0, Radius);

	for (int i = 1; i < Slices; i++)
	{
		const float r0 = Radius * sinf(i * (SW_PI / Slices));
		const float z0 = Radius * cosf(i * (SW_PI / Slices));

		for (int j = 0; j < Slices; j++)
		{
			const float x0 = r0 * sinf(j * (SW_2PI / Slices));
			const float y0 = r0 * cosf(j * (SW_2PI / Slices));

			*Vertex++ = Center + swVector3(x0, y0, z0);
		}
	}

	*Vertex++ = Center + swVector3(0, 0, -Radius);

	for (quint16 i = 0; i < Slices - 1; i++)
	{
		*Index++ = 0;
		*Index++ = 1 + i;
		*Index++ = 1 + i + 1;
	}

	*Index++ = 0;
	*Index++ = 1;
	*Index++ = 1 + Slices - 1;

	for (quint16 i = 0; i < Slices - 2; i++)
	{
		quint16 Row1 = 1 + i * Slices;
		quint16 Row2 = 1 + (i + 1) * Slices;

		for (quint16 j = 0; j < Slices - 1; j++)
		{
			*Index++ = Row1 + j;
			*Index++ = Row2 + j + 1;
			*Index++ = Row2 + j;

			*Index++ = Row1 + j;
			*Index++ = Row1 + j + 1;
			*Index++ = Row2 + j + 1;
		}

		*Index++ = Row1 + Slices - 1;
		*Index++ = Row2 + 0;
		*Index++ = Row2 + Slices - 1;

		*Index++ = Row1 + Slices - 1;
		*Index++ = Row2 + 0;
		*Index++ = Row1 + 0;
	}

	for (quint16 i = 0; i < Slices - 1; i++)
	{
		*Index++ = (Slices - 1) * Slices + 1;
		*Index++ = (Slices - 1) * (Slices - 1) + i;
		*Index++ = (Slices - 1) * (Slices - 1) + i + 1;
	}

	*Index++ = (Slices - 1) * Slices + 1;
	*Index++ = (Slices - 1) * (Slices - 1) + (Slices - 2) + 1;
	*Index++ = (Slices - 1) * (Slices - 1);

	Context->SetVertexBufferPointer(Vertices);
	Context->SetVertexFormatPosition(3);
	Context->SetIndexBufferPointer(Indices);

	Context->DrawIndexedPrimitives(GL_TRIANGLES, NumIndices, GL_UNSIGNED_SHORT, 0);
}

void swLight::DrawCylinder(swContext* Context, float Radius, float Height) const
{
	constexpr int Slices = 8;

	float Verts[(Slices * 2) * 3];
	float* CurVert = Verts;

	for (int EdgeIndex = 0; EdgeIndex < Slices; EdgeIndex++)
	{
		float c = cosf((float)EdgeIndex / Slices * SW_2PI) * Radius;
		float s = sinf((float)EdgeIndex / Slices * SW_2PI) * Radius;

		*CurVert++ = c;
		*CurVert++ = s;
		*CurVert++ = Height;
		*CurVert++ = c;
		*CurVert++ = s;
		*CurVert++ = 0.0f;
	}

	const GLushort Indices[48] =
	{
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
		0, 2, 2, 4, 4, 6, 6, 8, 8, 10, 10, 12, 12, 14, 14, 0,
		1, 3, 3, 5, 5, 7, 7, 9, 9, 11, 11, 13, 13, 15, 15, 1,
	};

	Context->SetVertexBufferPointer(Verts);
	Context->SetVertexFormatPosition(3);
	Context->SetIndexBufferPointer(Indices);

	Context->DrawIndexedPrimitives(GL_LINES, 48, GL_UNSIGNED_SHORT, 0);
}

void swLight::DrawTarget(swContext* Context) const
{
	float Verts[2 * 3];
	float* CurVert = Verts;

	*CurVert++ = 0.0f; *CurVert++ = 0.0f; *CurVert++ = 0.0f;
	*CurVert++ = 0.0f; *CurVert++ = 0.0f; *CurVert++ = -mTargetDistance;

	Context->SetVertexBufferPointer(Verts);
	Context->SetVertexFormatPosition(3);

	const GLushort Indices[2] =
	{
		0, 1
	};

	Context->SetIndexBufferPointer(Indices);

	Context->DrawIndexedPrimitives(GL_LINES, 2, GL_UNSIGNED_SHORT, 0);

	const swPreferences& Preferences = swGetPreferences();
	const float LineWidth = Preferences.mLineWidth;

	if (IsSelected())
	{
		const swVector4 SelectedColor = swVector4FromColor(Preferences.mObjectSelectedColor);
		const swVector4 FocusedColor = swVector4FromColor(Preferences.mObjectFocusedColor);

		Context->SetLineWidth(2.0f * LineWidth);

		if (IsFocused(SW_LIGHT_SECTION_TARGET))
			Context->SetColor(FocusedColor);
		else
			Context->SetColor(SelectedColor);
	}
	else
	{
		const swVector4 LightColor = swVector4FromColor(Preferences.mLightColor);

		Context->SetLineWidth(LineWidth);
		Context->SetColor(LightColor);
	}

	DrawSphere(Context, swVector3(0.0f, 0.0f, -mTargetDistance), SW_LIGHT_TARGET_RADIUS);
}

void swLight::DrawCone(swContext* Context, float TargetDistance) const
{
	constexpr int ConeEdges = 16;
	const float OuterRadius = tanf(SW_DTOR * mSpotConeAngle / 2.0f) * TargetDistance;

	float Verts[(ConeEdges * 2 + 1) * 3];
	float* CurVert = Verts;

	for (int EdgeIndex = 0; EdgeIndex < ConeEdges; EdgeIndex++)
	{
		const float c = cosf((float)EdgeIndex / ConeEdges * SW_2PI);
		const float s = sinf((float)EdgeIndex / ConeEdges * SW_2PI);

		*CurVert++ = c * OuterRadius;
		*CurVert++ = s * OuterRadius;
		*CurVert++ = -TargetDistance;
	}

	*CurVert++ = 0.0f;
	*CurVert++ = 0.0f;
	*CurVert++ = 0.0f;

	const bool DrawPenumbra = mSpotPenumbraAngle > 1.0f;

	if (DrawPenumbra)
	{
		const float InnerRadius = tanf(SW_DTOR * (mSpotConeAngle / 2.0f - mSpotPenumbraAngle)) * TargetDistance;

		for (int EdgeIndex = 0; EdgeIndex < ConeEdges; EdgeIndex++)
		{
			const float c = cosf((float)EdgeIndex / ConeEdges * SW_2PI);
			const float s = sinf((float)EdgeIndex / ConeEdges * SW_2PI);

			*CurVert++ = c * InnerRadius;
			*CurVert++ = s * InnerRadius;
			*CurVert++ = -TargetDistance;
		}
	}

	Context->SetVertexBufferPointer(Verts);
	Context->SetVertexFormatPosition(3);

	constexpr GLushort Indices[(ConeEdges * 2 + 4) * 2] =
	{
		0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8,
		8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15, 0,
		16, 0, 16, 4, 16, 8, 16, 12,
		17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 23, 24, 24, 25,
		25, 26, 26, 27, 27, 28, 28, 29, 29, 30, 30, 31, 31, 32, 32, 17
	};

	Context->SetIndexBufferPointer(Indices);

	Context->DrawIndexedPrimitives(GL_LINES, DrawPenumbra ? (ConeEdges * 2 + 4) * 2 : (ConeEdges + 4) * 2, GL_UNSIGNED_SHORT, 0);
}

QVariant swLight::GetPropertyValue(swObjectPropertyId PropertyId) const
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
		break;

	case swObjectPropertyId::LightName:
		return GetName();

	case swObjectPropertyId::LightType:
		return static_cast<int>(GetLightType());

	case swObjectPropertyId::LightColor:
		return QVariant::fromValue<swVector3>(GetColor());

	case swObjectPropertyId::LightBlenderPower:
		return GetBlenderPower();

	case swObjectPropertyId::LightPOVRayPower:
		return GetPOVRayPower();

	case swObjectPropertyId::LightCastShadow:
		return GetCastShadow();

	case swObjectPropertyId::LightPOVRayFadeDistance:
		return GetPOVRayFadeDistance();

	case swObjectPropertyId::LightPOVRayFadePower:
		return GetPOVRayFadePower();

	case swObjectPropertyId::LightBlenderRadius:
		return GetBlenderRadius();

	case swObjectPropertyId::LightBlenderAngle:
		return GetBlenderAngle();

	case swObjectPropertyId::LightAreaSizeX:
		return GetAreaSizeX();

	case swObjectPropertyId::LightAreaSizeY:
		return GetAreaSizeY();

	case swObjectPropertyId::LightSpotConeAngle:
		return GetSpotConeAngle();

	case swObjectPropertyId::LightSpotPenumbraAngle:
		return GetSpotPenumbraAngle();

	case swObjectPropertyId::LightPOVRaySpotTightness:
		return GetSpotPOVRayTightness();

	case swObjectPropertyId::LightAreaShape:
		return static_cast<int>(GetAreaShape());

	case swObjectPropertyId::LightPOVRayAreaGridX:
		return GetAreaPOVRayGridX();

	case swObjectPropertyId::LightPOVRayAreaGridY:
		return GetAreaPOVRayGridY();

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

bool swLight::SetPropertyValue(swObjectPropertyId PropertyId, swStep Step, bool AddKey, QVariant Value)
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
		break;

	case swObjectPropertyId::LightName:
		return SetName(Value.toString());

	case swObjectPropertyId::LightType:
		return SetLightType(static_cast<swLightType>(Value.toInt()));

	case swObjectPropertyId::LightColor:
		return SetColor(Value.value<swVector3>(), Step, AddKey);

	case swObjectPropertyId::LightBlenderPower:
		return SetBlenderPower(Value.toFloat(), Step, AddKey);

	case swObjectPropertyId::LightPOVRayPower:
		return SetPOVRayPower(Value.toFloat(), Step, AddKey);

	case swObjectPropertyId::LightCastShadow:
		return SetCastShadow(Value.toBool());

	case swObjectPropertyId::LightPOVRayFadeDistance:
		return SetPOVRayFadeDistance(Value.toFloat(), Step, AddKey);

	case swObjectPropertyId::LightPOVRayFadePower:
		return SetPOVRayFadePower(Value.toFloat(), Step, AddKey);

	case swObjectPropertyId::LightBlenderRadius:
		return SetBlenderRadius(Value.toFloat(), Step, AddKey);

	case swObjectPropertyId::LightBlenderAngle:
		return SetBlenderAngle(Value.toFloat(), Step, AddKey);

	case swObjectPropertyId::LightAreaSizeX:
		return SetAreaSizeX(Value.toFloat(), Step, AddKey);

	case swObjectPropertyId::LightAreaSizeY:
		return SetAreaSizeY(Value.toFloat(), Step, AddKey);

	case swObjectPropertyId::LightSpotConeAngle:
		return SetSpotConeAngle(Value.toFloat(), Step, AddKey);

	case swObjectPropertyId::LightSpotPenumbraAngle:
		return SetSpotPenumbraAngle(Value.toFloat(), Step, AddKey);

	case swObjectPropertyId::LightPOVRaySpotTightness:
		return SetSpotPOVRayTightness(Value.toFloat(), Step, AddKey);

	case swObjectPropertyId::LightAreaShape:
		return SetAreaShape(static_cast<swLightAreaShape>(Value.toInt()));

	case swObjectPropertyId::LightPOVRayAreaGridX:
		return SetAreaPOVRayGridX(Value.toInt(), Step, AddKey);

	case swObjectPropertyId::LightPOVRayAreaGridY:
		return SetAreaPOVRayGridY(Value.toInt(), Step, AddKey);

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

swStep swLight::GetLastStep() const
{
	return std::max({ mPosition.GetLastKeyFrame(), mRotation.GetLastKeyFrame(), mColor.GetLastKeyFrame(),
		mBlenderPower.GetLastKeyFrame(), mBlenderRadius.GetLastKeyFrame(), mBlenderAngle.GetLastKeyFrame(),
		mPOVRayPower.GetLastKeyFrame(), mPOVRayFadeDistance.GetLastKeyFrame(), mPOVRayFadePower.GetLastKeyFrame(),
		mSpotConeAngle.GetLastKeyFrame(), mSpotPenumbraAngle.GetLastKeyFrame(), mPOVRaySpotTightness.GetLastKeyFrame(),
		mAreaSizeX.GetLastKeyFrame(), mAreaSizeY.GetLastKeyFrame(), mPOVRayAreaGridX.GetLastKeyFrame(), mPOVRayAreaGridY.GetLastKeyFrame() });
}

bool swLight::HasKeyFrame(swObjectPropertyId PropertyId, swStep Time) const
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
		return false;

	case swObjectPropertyId::LightColor:
		return mColor.HasKeyFrame(Time);

	case swObjectPropertyId::LightBlenderPower:
		return mBlenderPower.HasKeyFrame(Time);

	case swObjectPropertyId::LightPOVRayPower:
		return mPOVRayPower.HasKeyFrame(Time);

	case swObjectPropertyId::LightCastShadow:
		return false;

	case swObjectPropertyId::LightPOVRayFadeDistance:
		return mPOVRayFadeDistance.HasKeyFrame(Time);

	case swObjectPropertyId::LightPOVRayFadePower:
		return mPOVRayFadePower.HasKeyFrame(Time);

	case swObjectPropertyId::LightBlenderRadius:
		return mBlenderRadius.HasKeyFrame(Time);

	case swObjectPropertyId::LightBlenderAngle:
		return mBlenderAngle.HasKeyFrame(Time);

	case swObjectPropertyId::LightAreaSizeX:
		return mAreaSizeX.HasKeyFrame(Time);

	case swObjectPropertyId::LightAreaSizeY:
		return mAreaSizeY.HasKeyFrame(Time);

	case swObjectPropertyId::LightSpotConeAngle:
		return mSpotConeAngle.HasKeyFrame(Time);

	case swObjectPropertyId::LightSpotPenumbraAngle:
		return mSpotPenumbraAngle.HasKeyFrame(Time);

	case swObjectPropertyId::LightPOVRaySpotTightness:
		return mPOVRaySpotTightness.HasKeyFrame(Time);

	case swObjectPropertyId::LightAreaShape:
		return false;

	case swObjectPropertyId::LightPOVRayAreaGridX:
		return mPOVRayAreaGridX.HasKeyFrame(Time);

	case swObjectPropertyId::LightPOVRayAreaGridY:
		return mPOVRayAreaGridY.HasKeyFrame(Time);

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

bool swLight::SetKeyFrame(swObjectPropertyId PropertyId, swStep Time, bool KeyFrame)
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
		return false;

	case swObjectPropertyId::LightColor:
		return mColor.SetKeyFrame(Time, KeyFrame);

	case swObjectPropertyId::LightBlenderPower:
		return mBlenderPower.SetKeyFrame(Time, KeyFrame);

	case swObjectPropertyId::LightPOVRayPower:
		return mPOVRayPower.SetKeyFrame(Time, KeyFrame);

	case swObjectPropertyId::LightCastShadow:
		return false;

	case swObjectPropertyId::LightPOVRayFadeDistance:
		return mPOVRayFadeDistance.SetKeyFrame(Time, KeyFrame);

	case swObjectPropertyId::LightPOVRayFadePower:
		return mPOVRayFadePower.SetKeyFrame(Time, KeyFrame);

	case swObjectPropertyId::LightBlenderRadius:
		return mBlenderRadius.SetKeyFrame(Time, KeyFrame);

	case swObjectPropertyId::LightBlenderAngle:
		return mBlenderAngle.SetKeyFrame(Time, KeyFrame);

	case swObjectPropertyId::LightAreaSizeX:
		return mAreaSizeX.SetKeyFrame(Time, KeyFrame);

	case swObjectPropertyId::LightAreaSizeY:
		return mAreaSizeY.SetKeyFrame(Time, KeyFrame);

	case swObjectPropertyId::LightSpotConeAngle:
		return mSpotConeAngle.SetKeyFrame(Time, KeyFrame);

	case swObjectPropertyId::LightSpotPenumbraAngle:
		return mSpotPenumbraAngle.SetKeyFrame(Time, KeyFrame);

	case swObjectPropertyId::LightPOVRaySpotTightness:
		return mPOVRaySpotTightness.SetKeyFrame(Time, KeyFrame);

	case swObjectPropertyId::LightAreaShape:
		return false;

	case swObjectPropertyId::LightPOVRayAreaGridX:
		return mPOVRayAreaGridX.SetKeyFrame(Time, KeyFrame);

	case swObjectPropertyId::LightPOVRayAreaGridY:
		return mPOVRayAreaGridY.SetKeyFrame(Time, KeyFrame);

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

void swLight::RemoveKeyFrames()
{
	mPosition.RemoveAllKeys();
	mRotation.RemoveAllKeys();
	mColor.RemoveAllKeys();
	mSpotConeAngle.RemoveAllKeys();
	mSpotPenumbraAngle.RemoveAllKeys();
	mPOVRaySpotTightness.RemoveAllKeys();
	mPOVRayAreaGridX.RemoveAllKeys();
	mPOVRayAreaGridY.RemoveAllKeys();
	mBlenderRadius.RemoveAllKeys();
	mBlenderAngle.RemoveAllKeys();
	mAreaSizeX.RemoveAllKeys();
	mAreaSizeY.RemoveAllKeys();
	mBlenderPower.RemoveAllKeys();
	mPOVRayPower.RemoveAllKeys();
	mPOVRayFadeDistance.RemoveAllKeys();
	mPOVRayFadePower.RemoveAllKeys();
}

swLightHistoryState swLight::GetHistoryState() const
{
	swLightHistoryState State;

	State.Id = mId;
	State.Hidden = mHidden;
	State.Name = mName;
	State.LightType = mLightType;
	State.CastShadow = mCastShadow;
	State.Position = mPosition;
	State.Rotation = mRotation;
	State.Color = mColor;
	State.BlenderPower = mBlenderPower;
	State.BlenderRadius = mBlenderRadius;
	State.BlenderAngle = mBlenderAngle;
	State.POVRayPower = mPOVRayPower;
	State.POVRayFadeDistance = mPOVRayFadeDistance;
	State.POVRayFadePower = mPOVRayFadePower;
	State.SpotConeAngle = mSpotConeAngle;
	State.SpotPenumbraAngle = mSpotPenumbraAngle;
	State.POVRaySpotTightness = mPOVRaySpotTightness;
	State.AreaShape = mAreaShape;
	State.AreaSizeX = mAreaSizeX;
	State.AreaSizeY = mAreaSizeY;
	State.POVRayAreaGridX = mPOVRayAreaGridX;
	State.POVRayAreaGridY = mPOVRayAreaGridY;

	return State;
}

void swLight::SetHistoryState(const swLightHistoryState& State)
{
	mId = State.Id;
	mHidden = State.Hidden;
	mName = State.Name;
	mLightType = State.LightType;
	mCastShadow = State.CastShadow;
	mPosition = State.Position;
	mRotation = State.Rotation;
	mColor = State.Color;
	mBlenderPower = State.BlenderPower;
	mBlenderRadius = State.BlenderRadius;
	mBlenderAngle = State.BlenderAngle;
	mPOVRayPower = State.POVRayPower;
	mPOVRayFadeDistance = State.POVRayFadeDistance;
	mPOVRayFadePower = State.POVRayFadePower;
	mSpotConeAngle = State.SpotConeAngle;
	mSpotPenumbraAngle = State.SpotPenumbraAngle;
	mPOVRaySpotTightness = State.POVRaySpotTightness;
	mAreaShape = State.AreaShape;
	mAreaSizeX = State.AreaSizeX;
	mAreaSizeY = State.AreaSizeY;
	mPOVRayAreaGridX = State.POVRayAreaGridX;
	mPOVRayAreaGridY = State.POVRayAreaGridY;
}
