#include "sw_global.h"
#include "sw_track_train.h"
#include "sw_library.h"
#include "sw_data_part.h"
#include "sw_object_part.h"
#include "sw_model.h"
#include "sw_application.h"

// todo test part intersections when moving

std::map<size_t, swPartData*> swTrainTrackInformation::mSleepers;

void swTrainTrackInformation::Initialize(swPartsLibrary* Library)
{
	QFile ConfigFile(QLatin1String(":/resources/data/traintrack.json"));

	if (!ConfigFile.open(QIODevice::ReadOnly))
		return;

	QJsonParseError Error;
	QJsonDocument Document = QJsonDocument::fromJson(ConfigFile.readAll(), &Error);

	if (Error.error != QJsonParseError::NoError)
	{
		qDebug() << Error.errorString();
	}

	QJsonObject Root = Document.object();

	if (Root["Version"].toInt() != 1)
		return;

	QJsonArray JsonParts = Root["Parts"].toArray();

	for (QJsonArray::const_iterator PartsIt = JsonParts.constBegin(); PartsIt != JsonParts.constEnd(); ++PartsIt)
	{
		QJsonObject JsonPart = PartsIt->toObject();

		QJsonArray JsonConnections = JsonPart["Connections"].toArray();
		std::vector<swTrainTrackConnection> Connections;

		for (QJsonArray::const_iterator ConnectionIt = JsonConnections.constBegin(); ConnectionIt != JsonConnections.constEnd(); ++ConnectionIt)
		{
			QJsonObject JsonConnection = ConnectionIt->toObject();

			QJsonArray JsonPosition = JsonConnection["Position"].toArray();
			swVector3 Position(JsonPosition[0].toDouble(), JsonPosition[1].toDouble(), JsonPosition[2].toDouble());

			float Rotation = JsonConnection["Rotation"].toDouble() * SW_DEGREES_TO_RADIANS;
			QString ConnectionGroup = JsonConnection["Type"].toString();
			swTrainTrackConnectionSleeper ConnectionSleeper = swTrainTrackConnectionSleeper::None;

			if (ConnectionGroup.startsWith('+'))
			{
				ConnectionSleeper = swTrainTrackConnectionSleeper::NeedsSleeper;
				ConnectionGroup = ConnectionGroup.mid(1);
			}
			else if (ConnectionGroup.startsWith('-'))
			{
				ConnectionSleeper = swTrainTrackConnectionSleeper::HasSleeper;
				ConnectionGroup = ConnectionGroup.mid(1);
			}

			Connections.emplace_back(swTrainTrackConnection{ swMatrix44(swMatrix33RotationZ(Rotation), Position), { qHash(ConnectionGroup), ConnectionSleeper } });
		}

		int Color = JsonPart["Color"].toInt(16);
		QJsonArray JsonIDs = JsonPart["IDs"].toArray();

		for (QJsonArray::const_iterator IDIt = JsonIDs.constBegin(); IDIt != JsonIDs.constEnd(); ++IDIt)
		{
			swPartData* PartData = Library->FindPartData(IDIt->toString().toLatin1(), nullptr);

			if (PartData)
				PartData->SetTrainTrackInfo(new swTrainTrackInformation(Connections, Color, true));
		}

		JsonIDs = JsonPart["HiddenIDs"].toArray();

		for (QJsonArray::const_iterator IDIt = JsonIDs.constBegin(); IDIt != JsonIDs.constEnd(); ++IDIt)
		{
			swPartData* PartData = Library->FindPartData(IDIt->toString().toLatin1(), nullptr);

			if (PartData)
				PartData->SetTrainTrackInfo(new swTrainTrackInformation(Connections, Color, false));
		}
	}

	QJsonObject JsonSleepers = Root["Sleepers"].toObject();

	mSleepers.clear();

	for (QJsonObject::const_iterator SleepersIt = JsonSleepers.constBegin(); SleepersIt != JsonSleepers.constEnd(); ++SleepersIt)
	{
		size_t Group = qHash(SleepersIt.key());
		QString PartId = SleepersIt->toString();

		swPartData* SleeperPartData = Library->FindPartData(PartId.toLatin1(), nullptr);

		if (SleeperPartData)
			mSleepers[Group] = SleeperPartData;
	}
}

int swTrainTrackInformation::GetPartConnectionIndex(const swPart* Part1, int ConnectionIndex1, const swPart* Part2)
{
	const swTrainTrackInformation* TrainTrackInfo1 = Part1->mPartData->GetTrainTrackInfo();
	const swTrainTrackInformation* TrainTrackInfo2 = Part2->mPartData->GetTrainTrackInfo();

	const std::vector<swTrainTrackConnection>& Connections2 = TrainTrackInfo2->GetConnections();
	swMatrix44 Transform1 = swMultiply(TrainTrackInfo1->GetConnections()[ConnectionIndex1].Transform, Part1->mModelWorld);

	for (int ConnectionIndex2 = 0; ConnectionIndex2 < static_cast<int>(Connections2.size()); ConnectionIndex2++)
	{
		const swTrainTrackConnection& Connection2 = Connections2[ConnectionIndex2];
		const swMatrix44 Transform2 = swMultiply(Connection2.Transform, Part2->mModelWorld);

		if (swLengthSquared(Transform1.GetTranslation() - Transform2.GetTranslation()) > 0.1f)
			continue;

		float Dot = swDot3(Transform1[0], Transform2[0]);

		if (Dot < -0.99f && Dot > -1.01f)
			return ConnectionIndex2;
	}

	return -1;
}

std::vector<swInsertPartInformation> swTrainTrackInformation::GetInsertPartInfo(swPart* CurrentPart, swPartData* PartData, swPart* MovingPart, int ColorIndex, quint32 PreferredSection, bool AllowNewParts, std::optional<swVector3> ClosestPoint)
{
	std::vector<swInsertPartInformation> Parts;
	const swTrainTrackInformation* CurrentTrackInfo = CurrentPart->mPartData->GetTrainTrackInfo();

	if (!CurrentTrackInfo || CurrentTrackInfo->GetConnections().empty() || (MovingPart && PartData != MovingPart->mPartData))
		return Parts;

	quint32 ConnectionIndex = 0;

	if (ClosestPoint)
	{
		swMatrix44 InverseMatrix = swMatrix44AffineInverse(CurrentPart->mModelWorld);
		swVector3 LocalClosestPoint = swMultiply31(ClosestPoint.value(), InverseMatrix);
		float BestDistance = FLT_MAX;

		for (quint32 CheckIndex = 0; CheckIndex < CurrentTrackInfo->GetConnections().size(); CheckIndex++)
		{
			if (CurrentPart->IsTrainTrackConnected(CheckIndex))
				continue;

			if (MovingPart)
			{
				const std::vector<swTrainTrackConnection>& MovingConnections = PartData->GetTrainTrackInfo()->GetConnections();
				bool CanConnect = false;

				for (quint32 MovingConnectionIndex = 0; MovingConnectionIndex < MovingConnections.size(); MovingConnectionIndex++)
				{
					if (!MovingPart->IsTrainTrackConnected(MovingConnectionIndex) && AreConnectionsCompatible(CurrentTrackInfo->GetConnections()[CheckIndex].Type, MovingConnections[MovingConnectionIndex].Type, AllowNewParts))
					{
						CanConnect = true;
						break;
					}
				}

				if (!CanConnect)
					continue;
			}
			else
			{
				if (!PartData->GetTrainTrackInfo()->CanConnectTo(CurrentTrackInfo->GetConnections()[CheckIndex].Type, AllowNewParts))
					continue;
			}

			float Distance = (CurrentTrackInfo->GetConnections()[CheckIndex].Transform.GetTranslation() - LocalClosestPoint).LengthSquared();

			if (Distance < BestDistance)
			{
				ConnectionIndex = CheckIndex;
				BestDistance = Distance;
			}
		}

		if (BestDistance == FLT_MAX)
			return Parts;
	}
	else
	{
		if (PreferredSection != SW_PART_SECTION_INVALID && PreferredSection >= SW_PART_SECTION_TRAIN_TRACK_CONNECTION_FIRST)
			ConnectionIndex = PreferredSection - SW_PART_SECTION_TRAIN_TRACK_CONNECTION_FIRST;

		quint32 CheckIndex;

		for (CheckIndex = 0; CheckIndex < CurrentTrackInfo->GetConnections().size(); CheckIndex++)
			if (!CurrentPart->IsTrainTrackConnected((ConnectionIndex + CheckIndex) % CurrentTrackInfo->GetConnections().size()))
				break;

		if (CheckIndex == CurrentTrackInfo->GetConnections().size())
			return Parts;

		ConnectionIndex = (ConnectionIndex + CheckIndex) % CurrentTrackInfo->GetConnections().size();
	}

	const swTrainTrackConnectionType& CurrentConnectionType = CurrentTrackInfo->GetConnections()[ConnectionIndex].Type;
	const std::vector<swTrainTrackConnection>& NewConnections = PartData->GetTrainTrackInfo()->GetConnections();
	quint32 NewConnectionIndex;// = ConnectionIndex ? 0 : 1;

	for (NewConnectionIndex = 0; NewConnectionIndex < NewConnections.size(); NewConnectionIndex++)
	{
		if (MovingPart)
		{
			if (MovingPart->IsTrainTrackConnected(NewConnectionIndex))
				continue;

			const std::vector<swTrainTrackConnection>& MovingConnections = PartData->GetTrainTrackInfo()->GetConnections();

			if (AreConnectionsCompatible(CurrentTrackInfo->GetConnections()[ConnectionIndex].Type, MovingConnections[NewConnectionIndex].Type, AllowNewParts))
				break;
		}
		else
		{
			if (AreConnectionsCompatible(CurrentConnectionType, NewConnections[NewConnectionIndex].Type, AllowNewParts))
				break;
		}
	}

	if (NewConnectionIndex == NewConnections.size())
		return Parts;

	if (CurrentConnectionType.Sleeper == swTrainTrackConnectionSleeper::NeedsSleeper && NewConnections[NewConnectionIndex].Type.Sleeper == swTrainTrackConnectionSleeper::NeedsSleeper)
	{
		std::map<size_t, swPartData*>::const_iterator SleeperIt = mSleepers.find(CurrentConnectionType.Group);

		if (SleeperIt == mSleepers.end())
			return Parts;

		swPartData* SleeperPartData = SleeperIt->second;

		if (!SleeperPartData)
			return Parts;

		std::optional<swMatrix44> SleeperTransform = GetConnectionTransform(CurrentPart->mPartData.Get(), CurrentPart->mModelWorld, ConnectionIndex, SleeperPartData, 0);

		if (!SleeperTransform)
			return Parts;

		std::optional<swMatrix44> Transform = GetConnectionTransform(SleeperPartData, SleeperTransform.value(), 1, PartData, NewConnectionIndex);

		if (Transform)
		{
			Parts.emplace_back(swInsertPartInformation{ SleeperPartData, SleeperTransform.value(), swGetColorIndex(SleeperPartData->GetTrainTrackInfo()->GetColorCode()) });
			Parts.emplace_back(swInsertPartInformation{ PartData, Transform.value(), ColorIndex });
		}
	}
	else
	{
		std::optional<swMatrix44> Transform = GetConnectionTransform(CurrentPart->mPartData.Get(), CurrentPart->mModelWorld, ConnectionIndex, PartData, NewConnectionIndex);

		if (Transform)
			Parts.emplace_back(swInsertPartInformation{ PartData, Transform.value(), ColorIndex });
	}

	return Parts;
}

std::optional<swMatrix44> swTrainTrackInformation::GetConnectionTransform(swPartData* CurrentPartData, const swMatrix44& CurrentTransform, quint32 CurrentConnectionIndex, swPartData* PartData, quint32 NewConnectionIndex)
{
	if (!CurrentPartData || !PartData)
		return std::nullopt;

	const swTrainTrackInformation* CurrentTrackInfo = CurrentPartData->GetTrainTrackInfo();

	if (!CurrentTrackInfo || CurrentTrackInfo->GetConnections().empty())
		return std::nullopt;

	if (CurrentConnectionIndex >= CurrentTrackInfo->GetConnections().size())
		return std::nullopt;

	swTrainTrackInformation* NewTrackInfo = PartData->GetTrainTrackInfo();

	if (!NewTrackInfo || NewConnectionIndex >= NewTrackInfo->mConnections.size())
		return std::nullopt;

	swMatrix44 Transform;

//	if (TrainTrackType != swTrainTrackType::Left)
//		Transform = NewTrackInfo->mConnections[NewConnectionIndex].Transform;
//	else
//	{
		Transform = swMatrix44AffineInverse(NewTrackInfo->mConnections[NewConnectionIndex].Transform);
		Transform = swMultiply(Transform, swMatrix44RotationZ(SW_PI));

//	}

	Transform = swMultiply(Transform, CurrentTrackInfo->GetConnections()[CurrentConnectionIndex].Transform);
	Transform = swMultiply(Transform, CurrentTransform);

	return Transform;
}

std::optional<swMatrix44> swTrainTrackInformation::CalculateTransformToConnection(const swMatrix44& ConnectionTransform, swPartData* PartData, quint32 ConnectionIndex)
{
	swTrainTrackInformation* TrackInfo = PartData->GetTrainTrackInfo();

	if (!TrackInfo || ConnectionIndex >= TrackInfo->mConnections.size())
		return std::nullopt;

	swMatrix44 Transform;

//	if (TrainTrackType != swTrainTrackType::Left)
//		Transform = NewTrackInfo->mConnections[NewConnectionIndex].Transform;
//	else
//	{
		Transform = swMatrix44AffineInverse(TrackInfo->mConnections[ConnectionIndex].Transform);

//		Transform = swMultiply(Transform, swMatrix44RotationZ(SW_PI));
//	}

	Transform = swMultiply(Transform, ConnectionTransform);

//	Transform = swMultiply(Transform, CurrentTrackInfo->GetConnections()[CurrentConnectionIndex].Transform);
//	Transform = swMultiply(Transform, CurrentPart->mModelWorld);

	return Transform;
}
