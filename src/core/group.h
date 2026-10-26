#pragma once

#define SW_MAX_GROUP_NAME 64

enum class swGroupId : uint32_t;

constexpr uint32_t SW_GROUP_ID_INVALID = std::numeric_limits<uint32_t>::max();

struct swGroupHistoryState
{
	swGroupId Id;
	uint32_t ParentId;
	QString Name;

	bool operator==(const swGroupHistoryState& Other) const
	{
		return Id == Other.Id && ParentId == Other.ParentId && Name == Other.Name;
	}
};

class swGroup
{
public:
	swGroup();

	swGroup* GetTopGroup()
	{
		return mGroup ? mGroup->GetTopGroup() : this;
	}

	swGroupId GetId() const
	{
		return mId;
	}

	void FileLoad(swFile* File);
	void CreateName(const std::vector<std::unique_ptr<swGroup>>& Groups);

	swGroupHistoryState GetHistoryState() const;
	void SetHistoryState(const swGroupHistoryState& State, swGroup* Parent);

	swGroup* mGroup = nullptr;
	QString mName;

protected:
	swGroupId mId = static_cast<swGroupId>(0);

	static swGroupId mNextId;
};
