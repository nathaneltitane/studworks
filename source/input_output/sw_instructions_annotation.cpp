#include "sw_global.h"
#include "sw_instructions.h"

const swInstructionsNotice* swInstructions::FindNotice(int Id) const
{
	for (const swInstructionsNotice& Notice : mNotices)
		if (Notice.Id == Id)
			return &Notice;

	return nullptr;
}

int swInstructions::AddNotice(size_t PageIndex, const QPointF& Position, const QString& Text)
{
	swInstructionsNotice Notice;

	Notice.Id = mNextNoticeId++;
	Notice.PageKey = GetPageElementsKey(PageIndex);
	Notice.Position = Position;
	Notice.Text = Text;

	mNotices.push_back(Notice);

	emit PageLayoutChanged();

	return Notice.Id;
}

void swInstructions::SetNoticeText(int Id, const QString& Text)
{
	for (swInstructionsNotice& Notice : mNotices)
	{
		if (Notice.Id != Id || Notice.Text == Text)
			continue;

		Notice.Text = Text;

		emit PageLayoutChanged();
		return;
	}
}

void swInstructions::RemoveNotice(int Id)
{
	const swInstructionsElementKey Key = { swInstructionsPlacementElement::Notice, nullptr, static_cast<swStep>(Id) };
	const size_t NoticeCount = mNotices.size();

	mNotices.erase(std::remove_if(mNotices.begin(), mNotices.end(), [Id](const swInstructionsNotice& Notice)
	{
		return Notice.Id == Id;
	}), mNotices.end());

	if (mNotices.size() == NoticeCount)
		return;

	// the pointers of the notice go with it, the pointers at it keep pointing at where it was
	mPointers.erase(std::remove_if(mPointers.begin(), mPointers.end(), [&Key](const swInstructionsPointer& Pointer)
	{
		return Pointer.Source == Key;
	}), mPointers.end());

	for (swInstructionsPointer& Pointer : mPointers)
		if (Pointer.TargetType == swInstructionsPointerTarget::Element && Pointer.TargetElement == Key)
			Pointer.TargetType = swInstructionsPointerTarget::Point;

	mElementOffsets.erase(Key);
	mLockedPositions.erase(Key);

	emit PageLayoutChanged();
}

const swInstructionsPointer* swInstructions::FindPointer(int Id) const
{
	for (const swInstructionsPointer& Pointer : mPointers)
		if (Pointer.Id == Id)
			return &Pointer;

	return nullptr;
}

int swInstructions::AddPointer(size_t PageIndex, const swInstructionsElementKey& Source, const QPointF& TipPosition)
{
	swInstructionsPointer Pointer;

	Pointer.Id = mNextPointerId++;
	Pointer.PageKey = GetPageElementsKey(PageIndex);
	Pointer.Source = Source;
	Pointer.TargetElement = { swInstructionsPlacementElement::None, nullptr, 0 };
	Pointer.TipPosition = TipPosition;

	mPointers.push_back(Pointer);

	emit PageLayoutChanged();

	return Pointer.Id;
}

void swInstructions::SetPointerTarget(int Id, swInstructionsPointerTarget TargetType, const swInstructionsElementKey& TargetElement, int TargetPart, const QPointF& TipPosition)
{
	for (swInstructionsPointer& Pointer : mPointers)
	{
		if (Pointer.Id != Id)
			continue;

		Pointer.TargetType = TargetType;
		Pointer.TargetElement = TargetElement;
		Pointer.TargetPart = TargetPart;
		Pointer.TipPosition = TipPosition;

		emit PageLayoutChanged();
		return;
	}
}

void swInstructions::SetPointerAnchor(int Id, int Anchor)
{
	for (swInstructionsPointer& Pointer : mPointers)
	{
		if (Pointer.Id != Id || Pointer.Anchor == Anchor)
			continue;

		Pointer.Anchor = Anchor;

		emit PageLayoutChanged();
		return;
	}
}

void swInstructions::SetPointerStyle(int Id, int Style, int Tip)
{
	for (swInstructionsPointer& Pointer : mPointers)
	{
		if (Pointer.Id != Id || (Pointer.Style == Style && Pointer.Tip == Tip))
			continue;

		Pointer.Style = Style;
		Pointer.Tip = Tip;

		emit PageLayoutChanged();
		return;
	}
}

void swInstructions::RemovePointer(int Id)
{
	const size_t PointerCount = mPointers.size();

	mPointers.erase(std::remove_if(mPointers.begin(), mPointers.end(), [Id](const swInstructionsPointer& Pointer)
	{
		return Pointer.Id == Id;
	}), mPointers.end());

	if (mPointers.size() != PointerCount)
		emit PageLayoutChanged();
}
