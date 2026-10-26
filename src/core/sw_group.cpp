#include "sw_global.h"
#include <stdlib.h>
#include "sw_string.h"
#include "sw_group.h"
#include "sw_file.h"

swGroupId swGroup::mNextId;

swGroup::swGroup()
    : mId(mNextId)
{
	mNextId = static_cast<swGroupId>(static_cast<uint32_t>(mNextId) + 1);
}

void swGroup::FileLoad(swFile* File)
{
	qint32 GroupIndex;
	char Name[SW_MAXIMUM_GROUP_NAME + 1];

	File->ReadU8();
	File->ReadBuffer(Name, sizeof(Name));
	mName = QString::fromUtf8(Name);
	File->ReadVector3();
	File->ReadS32(&GroupIndex, 1);
	mGroup = (swGroup*)(quintptr)GroupIndex;
}

void swGroup::CreateName(const std::vector<std::unique_ptr<swGroup>>& Groups)
{
	if (!mName.isEmpty())
	{
		bool Found = false;
		for (const std::unique_ptr<swGroup>& Group : Groups)
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

	for (const std::unique_ptr<swGroup>& Group : Groups)
		Max = qMax(Max, swGetDefaultNameNumber(Group->mName, Prefix));

	mName = swFormatDefaultName(Prefix, Max + 1);
}

swGroupHistoryState swGroup::GetHistoryState() const
{
	swGroupHistoryState State;

	State.Id = mId;
	State.ParentId = mGroup ? static_cast<uint32_t>(mGroup->GetId()) : SW_GROUP_ID_INVALID;
	State.Name = mName;

	return State;
}

void swGroup::SetHistoryState(const swGroupHistoryState& State, swGroup* Parent)
{
	mId = State.Id;
	mName = State.Name;
	mGroup = Parent;
}
