#include "sw_global.h"
#include <stdlib.h>
#include "sw_string.h"
#include "group.h"
#include "sw_file.h"

lcGroupId lcGroup::mNextId;

lcGroup::lcGroup()
    : mId(mNextId)
{
	mNextId = static_cast<lcGroupId>(static_cast<uint32_t>(mNextId) + 1);
}

void lcGroup::FileLoad(lcFile* File)
{
	qint32 GroupIndex;
	char Name[LC_MAX_GROUP_NAME + 1];

	File->ReadU8();
	File->ReadBuffer(Name, sizeof(Name));
	mName = QString::fromUtf8(Name);
	File->ReadVector3();
	File->ReadS32(&GroupIndex, 1);
	mGroup = (lcGroup*)(quintptr)GroupIndex;
}

void lcGroup::CreateName(const std::vector<std::unique_ptr<lcGroup>>& Groups)
{
	if (!mName.isEmpty())
	{
		bool Found = false;
		for (const std::unique_ptr<lcGroup>& Group : Groups)
		{
			if (Group->mName == mName)
			{
				Found = true;
				break;
			}
		}

		if (!Found)
			return;
	}

	int Max = 0;
	const QString Prefix = QApplication::tr("Group ");

	for (const std::unique_ptr<lcGroup>& Group : Groups)
		Max = qMax(Max, lcGetDefaultNameNumber(Group->mName, Prefix));

	mName = lcFormatDefaultName(Prefix, Max + 1);
}

lcGroupHistoryState lcGroup::GetHistoryState() const
{
	lcGroupHistoryState State;

	State.Id = mId;
	State.ParentId = mGroup ? static_cast<uint32_t>(mGroup->GetId()) : LC_GROUP_ID_INVALID;
	State.Name = mName;

	return State;
}

void lcGroup::SetHistoryState(const lcGroupHistoryState& State, lcGroup* Parent)
{
	mId = State.Id;
	mName = State.Name;
	mGroup = Parent;
}
