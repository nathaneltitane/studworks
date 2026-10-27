#pragma once

#include "sw_math.h"

class swPart;
class swPartsLibrary;
class swPartData;
struct swInsertPartInformation;

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

class swTrainTrackInformation
{
public:
	swTrainTrackInformation(const std::vector<swTrainTrackConnection> &Connections, int Color, bool Visible)
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

	static void Initialize(swPartsLibrary* Library);
	static std::vector<swInsertPartInformation> GetInsertPartInfo(swPart* CurrentPart, swPartData* PartData, swPart* MovingPart, int ColorIndex, quint32 PreferredSection, bool AllowNewParts, std::optional<swVector3> ClosestPoint);
	static std::optional<swMatrix44> GetConnectionTransform(swPartData* CurrentPartData, const swMatrix44& CurrentTransform, quint32 CurrentConnectionIndex, swPartData* PartData, quint32 NewConnectionIndex);
	static std::optional<swMatrix44> CalculateTransformToConnection(const swMatrix44& ConnectionTransform, swPartData* PartData, quint32 ConnectionIndex);
	static int GetPartConnectionIndex(const swPart* Part1, int ConnectionIndex1, const swPart* Part2);

	const std::vector<swTrainTrackConnection>& GetConnections() const
	{
		return mConnections;
	}

	bool CanConnectTo(const swTrainTrackConnectionType& ConnectionType, bool AllowNewParts) const
	{
		for (const swTrainTrackConnection& Connection : mConnections)
			if (AreConnectionsCompatible(Connection.Type, ConnectionType, AllowNewParts))
				return true;

		return false;
	}

protected:
	static bool AreConnectionsCompatible(const swTrainTrackConnectionType& a, const swTrainTrackConnectionType& b, bool AllowNewParts)
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
			return b.Sleeper == swTrainTrackConnectionSleeper::HasSleeper || (AllowNewParts && b.Sleeper == swTrainTrackConnectionSleeper::NeedsSleeper);
		}

		return false;
	}

	std::vector<swTrainTrackConnection> mConnections;
	int mColorCode = 16;
	bool mVisible = false;

	static std::map<size_t, swPartData*> mSleepers;
};
