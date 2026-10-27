#include "sw_global.h"
#include "sw_train_track.h"
#include "sw_library.h"
#include "sw_piece_data.h"
#include "sw_object_piece.h"
#include "sw_model.h"
#include "sw_application.h"

// todo:
// when moving existing pieces:
//   need to check for parts intersecting instead of mouse intersection because it's hard to connect tracks
//   look into the snapping that happens when there are no available connections
//   fix comments in UpdateFreeMoveTool
// hide some of the 12v tracks to avoid bloat
// auto replace cross when going over a straight section
// set focus connection after adding

std::map<size_t, swPieceData*> swTrainTrackInformation::mSleepers;

void swTrainTrackInformation::Initialize(swPiecesLibrary* Library)
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

	QJsonArray JsonPieces = Root["Pieces"].toArray();

	for (QJsonArray::const_iterator PiecesIt = JsonPieces.constBegin(); PiecesIt != JsonPieces.constEnd(); ++PiecesIt)
	{
		QJsonObject JsonPiece = PiecesIt->toObject();

		QJsonArray JsonConnections = JsonPiece["Connections"].toArray();
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

		int Color = JsonPiece["Color"].toInt(16);
		QJsonArray JsonIDs = JsonPiece["IDs"].toArray();

		for (QJsonArray::const_iterator IDIt = JsonIDs.constBegin(); IDIt != JsonIDs.constEnd(); ++IDIt)
		{
			swPieceData* PieceData = Library->FindPieceData(IDIt->toString().toLatin1(), nullptr);

			if (PieceData)
				PieceData->SetTrainTrackInfo(new swTrainTrackInformation(Connections, Color, true));
		}

		JsonIDs = JsonPiece["HiddenIDs"].toArray();

		for (QJsonArray::const_iterator IDIt = JsonIDs.constBegin(); IDIt != JsonIDs.constEnd(); ++IDIt)
		{
			swPieceData* PieceData = Library->FindPieceData(IDIt->toString().toLatin1(), nullptr);

			if (PieceData)
				PieceData->SetTrainTrackInfo(new swTrainTrackInformation(Connections, Color, false));
		}
	}

	QJsonObject JsonSleepers = Root["Sleepers"].toObject();

	mSleepers.clear();

	for (QJsonObject::const_iterator SleepersIt = JsonSleepers.constBegin(); SleepersIt != JsonSleepers.constEnd(); ++SleepersIt)
	{
		size_t Group = qHash(SleepersIt.key());
		QString PartId = SleepersIt->toString();

		swPieceData* SleeperPieceData = Library->FindPieceData(PartId.toLatin1(), nullptr);

		if (SleeperPieceData)
			mSleepers[Group] = SleeperPieceData;
	}
}

int swTrainTrackInformation::GetPieceConnectionIndex(const swPiece* Piece1, int ConnectionIndex1, const swPiece* Piece2)
{
	const swTrainTrackInformation* TrainTrackInfo1 = Piece1->mPieceData->GetTrainTrackInfo();
	const swTrainTrackInformation* TrainTrackInfo2 = Piece2->mPieceData->GetTrainTrackInfo();

	const std::vector<swTrainTrackConnection>& Connections2 = TrainTrackInfo2->GetConnections();
	swMatrix44 Transform1 = swMultiply(TrainTrackInfo1->GetConnections()[ConnectionIndex1].Transform, Piece1->mModelWorld);

	for (int ConnectionIndex2 = 0; ConnectionIndex2 < static_cast<int>(Connections2.size()); ConnectionIndex2++)
	{
		const swTrainTrackConnection& Connection2 = Connections2[ConnectionIndex2];
		const swMatrix44 Transform2 = swMultiply(Connection2.Transform, Piece2->mModelWorld);

		if (swLengthSquared(Transform1.GetTranslation() - Transform2.GetTranslation()) > 0.1f)
			continue;

		float Dot = swDot3(Transform1[0], Transform2[0]);

		if (Dot < -0.99f && Dot > -1.01f)
			return ConnectionIndex2;
	}

	return -1;
}

std::vector<swInsertPieceInformation> swTrainTrackInformation::GetInsertPieceInfo(swPiece* CurrentPiece, swPieceData* PieceData, swPiece* MovingPiece, int ColorIndex, quint32 PreferredSection, bool AllowNewPieces, std::optional<swVector3> ClosestPoint)
{
	std::vector<swInsertPieceInformation> Pieces;
	const swTrainTrackInformation* CurrentTrackInfo = CurrentPiece->mPieceData->GetTrainTrackInfo();

	if (!CurrentTrackInfo || CurrentTrackInfo->GetConnections().empty() || (MovingPiece && PieceData != MovingPiece->mPieceData))
		return Pieces;

	quint32 ConnectionIndex = 0;

	if (ClosestPoint)
	{
		swMatrix44 InverseMatrix = swMatrix44AffineInverse(CurrentPiece->mModelWorld);
		swVector3 LocalClosestPoint = swMultiply31(ClosestPoint.value(), InverseMatrix);
		float BestDistance = FLT_MAX;

		for (quint32 CheckIndex = 0; CheckIndex < CurrentTrackInfo->GetConnections().size(); CheckIndex++)
		{
			if (CurrentPiece->IsTrainTrackConnected(CheckIndex))
				continue;

			if (MovingPiece)
			{
				const std::vector<swTrainTrackConnection>& MovingConnections = PieceData->GetTrainTrackInfo()->GetConnections();
				bool CanConnect = false;

				for (quint32 MovingConnectionIndex = 0; MovingConnectionIndex < MovingConnections.size(); MovingConnectionIndex++)
				{
					if (!MovingPiece->IsTrainTrackConnected(MovingConnectionIndex) && AreConnectionsCompatible(CurrentTrackInfo->GetConnections()[CheckIndex].Type, MovingConnections[MovingConnectionIndex].Type, AllowNewPieces))
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
				if (!PieceData->GetTrainTrackInfo()->CanConnectTo(CurrentTrackInfo->GetConnections()[CheckIndex].Type, AllowNewPieces))
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
			return Pieces;
	}
	else
	{
		if (PreferredSection != SW_PIECE_SECTION_INVALID && PreferredSection >= SW_PIECE_SECTION_TRAIN_TRACK_CONNECTION_FIRST)
			ConnectionIndex = PreferredSection - SW_PIECE_SECTION_TRAIN_TRACK_CONNECTION_FIRST;

		quint32 CheckIndex;

		for (CheckIndex = 0; CheckIndex < CurrentTrackInfo->GetConnections().size(); CheckIndex++)
			if (!CurrentPiece->IsTrainTrackConnected((ConnectionIndex + CheckIndex) % CurrentTrackInfo->GetConnections().size()))
				break;

		if (CheckIndex == CurrentTrackInfo->GetConnections().size())
			return Pieces;

		ConnectionIndex = (ConnectionIndex + CheckIndex) % CurrentTrackInfo->GetConnections().size();
	}

	const swTrainTrackConnectionType& CurrentConnectionType = CurrentTrackInfo->GetConnections()[ConnectionIndex].Type;
	const std::vector<swTrainTrackConnection>& NewConnections = PieceData->GetTrainTrackInfo()->GetConnections();
	quint32 NewConnectionIndex;// = ConnectionIndex ? 0 : 1;

	for (NewConnectionIndex = 0; NewConnectionIndex < NewConnections.size(); NewConnectionIndex++)
	{
		if (MovingPiece)
		{
			if (MovingPiece->IsTrainTrackConnected(NewConnectionIndex))
				continue;

			const std::vector<swTrainTrackConnection>& MovingConnections = PieceData->GetTrainTrackInfo()->GetConnections();

			if (AreConnectionsCompatible(CurrentTrackInfo->GetConnections()[ConnectionIndex].Type, MovingConnections[NewConnectionIndex].Type, AllowNewPieces))
				break;
		}
		else
		{
			if (AreConnectionsCompatible(CurrentConnectionType, NewConnections[NewConnectionIndex].Type, AllowNewPieces))
				break;
		}
	}

	if (NewConnectionIndex == NewConnections.size())
		return Pieces;

	if (CurrentConnectionType.Sleeper == swTrainTrackConnectionSleeper::NeedsSleeper && NewConnections[NewConnectionIndex].Type.Sleeper == swTrainTrackConnectionSleeper::NeedsSleeper)
	{
		std::map<size_t, swPieceData*>::const_iterator SleeperIt = mSleepers.find(CurrentConnectionType.Group);

		if (SleeperIt == mSleepers.end())
			return Pieces;

		swPieceData* SleeperPieceData = SleeperIt->second;

		if (!SleeperPieceData)
			return Pieces;

		std::optional<swMatrix44> SleeperTransform = GetConnectionTransform(CurrentPiece->mPieceData.Get(), CurrentPiece->mModelWorld, ConnectionIndex, SleeperPieceData, 0);

		if (!SleeperTransform)
			return Pieces;

		std::optional<swMatrix44> Transform = GetConnectionTransform(SleeperPieceData, SleeperTransform.value(), 1, PieceData, NewConnectionIndex);

		if (Transform)
		{
			Pieces.emplace_back(swInsertPieceInformation{ SleeperPieceData, SleeperTransform.value(), swGetColorIndex(SleeperPieceData->GetTrainTrackInfo()->GetColorCode()) });
			Pieces.emplace_back(swInsertPieceInformation{ PieceData, Transform.value(), ColorIndex });
		}
	}
	else
	{
		std::optional<swMatrix44> Transform = GetConnectionTransform(CurrentPiece->mPieceData.Get(), CurrentPiece->mModelWorld, ConnectionIndex, PieceData, NewConnectionIndex);

		if (Transform)
			Pieces.emplace_back(swInsertPieceInformation{ PieceData, Transform.value(), ColorIndex });
	}

	return Pieces;
}

std::optional<swMatrix44> swTrainTrackInformation::GetConnectionTransform(swPieceData* CurrentPieceData, const swMatrix44& CurrentTransform, quint32 CurrentConnectionIndex, swPieceData* PieceData, quint32 NewConnectionIndex)
{
	if (!CurrentPieceData || !PieceData)
		return std::nullopt;

	const swTrainTrackInformation* CurrentTrackInfo = CurrentPieceData->GetTrainTrackInfo();

	if (!CurrentTrackInfo || CurrentTrackInfo->GetConnections().empty())
		return std::nullopt;

	if (CurrentConnectionIndex >= CurrentTrackInfo->GetConnections().size())
		return std::nullopt;

	swTrainTrackInformation* NewTrackInfo = PieceData->GetTrainTrackInfo();

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

std::optional<swMatrix44> swTrainTrackInformation::CalculateTransformToConnection(const swMatrix44& ConnectionTransform, swPieceData* PieceData, quint32 ConnectionIndex)
{
	swTrainTrackInformation* TrackInfo = PieceData->GetTrainTrackInfo();

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
//	Transform = swMultiply(Transform, CurrentPiece->mModelWorld);

	return Transform;
}
