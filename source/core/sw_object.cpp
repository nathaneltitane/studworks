#include "sw_global.h"
#include "sw_object.h"

swObjectId swObject::mNextId;

swObject::swObject(swObjectType ObjectType)
    : mObjectType(ObjectType), mId(mNextId)
{
	mNextId = static_cast<swObjectId>(static_cast<uint32_t>(mNextId) + 1);
}

swObject::~swObject()
{
}

QString swObject::GetCheckpointString(swObjectPropertyId PropertyId)
{
	switch (PropertyId)
	{
	case swObjectPropertyId::PieceId:
		return QT_TRANSLATE_NOOP("Checkpoint", "Change Piece");

	case swObjectPropertyId::PieceColor:
		return QT_TRANSLATE_NOOP("Checkpoint", "Change Piece Color");

	case swObjectPropertyId::PieceStepShow:
	case swObjectPropertyId::PieceStepHide:
		break;

	case swObjectPropertyId::CameraName:
		return QT_TRANSLATE_NOOP("Checkpoint", "Rename Camera");

	case swObjectPropertyId::CameraProjection:
		return QT_TRANSLATE_NOOP("Checkpoint", "Change Camera Projection");

	case swObjectPropertyId::CameraFOV:
		return QT_TRANSLATE_NOOP("Checkpoint", "Change Camera FOV");

	case swObjectPropertyId::CameraNear:
		return QT_TRANSLATE_NOOP("Checkpoint", "Change Camera Near Plane");

	case swObjectPropertyId::CameraFar:
		return QT_TRANSLATE_NOOP("Checkpoint", "Change Camera Far Plane");

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
		return QT_TRANSLATE_NOOP("Checkpoint", "Rename Light");

	case swObjectPropertyId::LightType:
		return QT_TRANSLATE_NOOP("Checkpoint", "Change Light Type");

	case swObjectPropertyId::LightColor:
		return QT_TRANSLATE_NOOP("Checkpoint", "Change Light Color");

	case swObjectPropertyId::LightBlenderPower:
		return QT_TRANSLATE_NOOP("Checkpoint", "Change Light Blender Power");

	case swObjectPropertyId::LightPOVRayPower:
		return QT_TRANSLATE_NOOP("Checkpoint", "Change Light POV-Ray Power");

	case swObjectPropertyId::LightCastShadow:
		return QT_TRANSLATE_NOOP("Checkpoint", "Change Light Shadow");

	case swObjectPropertyId::LightPOVRayFadeDistance:
		return QT_TRANSLATE_NOOP("Checkpoint", "Change Light POV-Ray Fade Distance");

	case swObjectPropertyId::LightPOVRayFadePower:
		return QT_TRANSLATE_NOOP("Checkpoint", "Change Light POV-Ray Fade Power");

	case swObjectPropertyId::LightBlenderRadius:
		return QT_TRANSLATE_NOOP("Checkpoint", "Change Light Blender Radius");

	case swObjectPropertyId::LightBlenderAngle:
		return QT_TRANSLATE_NOOP("Checkpoint", "Change Light Blender Angle");

	case swObjectPropertyId::LightAreaSizeX:
		return QT_TRANSLATE_NOOP("Checkpoint", "Change Area Light X Size");

	case swObjectPropertyId::LightAreaSizeY:
		return QT_TRANSLATE_NOOP("Checkpoint", "Change Area Light Y Size");

	case swObjectPropertyId::LightSpotConeAngle:
		return QT_TRANSLATE_NOOP("Checkpoint", "Change Spotlight Cone Angle");

	case swObjectPropertyId::LightSpotPenumbraAngle:
		return QT_TRANSLATE_NOOP("Checkpoint", "Change Spotlight Penumbra Angle");

	case swObjectPropertyId::LightPOVRaySpotTightness:
		return QT_TRANSLATE_NOOP("Checkpoint", "Change Spotlight POV-Ray Tightness");

	case swObjectPropertyId::LightAreaShape:
		return QT_TRANSLATE_NOOP("Checkpoint", "Change Area Light Shape");

	case swObjectPropertyId::LightPOVRayAreaGridX:
	case swObjectPropertyId::LightPOVRayAreaGridY:
		return QT_TRANSLATE_NOOP("Checkpoint", "Change Area Light POV-Ray Grid");

	case swObjectPropertyId::ObjectPositionX:
	case swObjectPropertyId::ObjectPositionY:
	case swObjectPropertyId::ObjectPositionZ:
	case swObjectPropertyId::ObjectRotationX:
	case swObjectPropertyId::ObjectRotationY:
	case swObjectPropertyId::ObjectRotationZ:
	case swObjectPropertyId::Count:
		break;
	}

	return QString();
}
