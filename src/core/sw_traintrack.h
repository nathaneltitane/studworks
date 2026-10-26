#pragma once

#include "sw_math.h"

class swPiece;
class swPiecesLibrary;
class swPieceData;
struct swInsertPieceInfo;

enum class swTrainTrackConnectionSleeper
{
	None,
	HasSleeper,
	NeedsSleeper
};

struct swTrainTrackConnectionType
{
	size_t Group;
	swTrainTrackConnectionSleeper Sleeper;
};

struct swTrainTrackConnection
{
	swMatrix44 Transform;
	swTrainTrackConnectionType Type;
};

class swTrainTrackInfo
{
public:
	swTrainTrackInfo(const std::vector<swTrainTrackConnection> &Connections, int Color, bool Visible)
		: mConnections(Connections), mColorCode(Color), mVisible(Visible)
	{
	}

	int GetColorCode() const
	{
		return mColorCode;
	}

	bool IsVisible() const
	{
		return mVisible;
	}

	static void Initialize(swPiecesLibrary* Library);
	static std::vector<swInsertPieceInfo> GetInsertPieceInfo(swPiece* CurrentPiece, swPieceData* PieceData, swPiece* MovingPiece, int ColorIndex, quint32 PreferredSection, bool AllowNewPieces, std::optional<swVector3> ClosestPoint);
	static std::optional<swMatrix44> GetConnectionTransform(swPieceData* CurrentPieceData, const swMatrix44& CurrentTransform, quint32 CurrentConnectionIndex, swPieceData* PieceData, quint32 NewConnectionIndex);
	static std::optional<swMatrix44> CalculateTransformToConnection(const swMatrix44& ConnectionTransform, swPieceData* PieceData, quint32 ConnectionIndex);
	static int GetPieceConnectionIndex(const swPiece* Piece1, int ConnectionIndex1, const swPiece* Piece2);

	const std::vector<swTrainTrackConnection>& GetConnections() const
	{
		return mConnections;
	}

	bool CanConnectTo(const swTrainTrackConnectionType& ConnectionType, bool AllowNewPieces) const
	{
		for (const swTrainTrackConnection& Connection : mConnections)
			if (AreConnectionsCompatible(Connection.Type, ConnectionType, AllowNewPieces))
				return true;

		return false;
	}

protected:
	static bool AreConnectionsCompatible(const swTrainTrackConnectionType& a, const swTrainTrackConnectionType& b, bool AllowNewPieces)
	{
		if (a.Group != b.Group)
			return false;

		switch (a.Sleeper)
		{
		case swTrainTrackConnectionSleeper::None:
			return b.Sleeper == swTrainTrackConnectionSleeper::None;

		case swTrainTrackConnectionSleeper::HasSleeper:
			return b.Sleeper == swTrainTrackConnectionSleeper::NeedsSleeper;

		case swTrainTrackConnectionSleeper::NeedsSleeper:
			return b.Sleeper == swTrainTrackConnectionSleeper::HasSleeper || (AllowNewPieces && b.Sleeper == swTrainTrackConnectionSleeper::NeedsSleeper);
		}

		return false;
	}

	std::vector<swTrainTrackConnection> mConnections;
	int mColorCode = 16;
	bool mVisible = false;

	static std::map<size_t, swPieceData*> mSleepers;
};
