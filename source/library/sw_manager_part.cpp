#include "sw_global.h"
#include "sw_manager_part.h"
#include "sw_data_part.h"
#include "sw_string.h"
#include "sw_project.h"

swPartManager::~swPartManager()
{
	// Library teardown must unregister identities before destroying this registry.
	Q_ASSERT(mParts.empty() && mProjectParts.empty() && mPartDataLoadErrors.empty());
}

std::string swPartManager::NormalizePartDataName(const char* PartName)
{
	std::string Name;

	for (const char* Src = PartName; *Src && Name.size() < SW_PART_NAME_LENGTH - 1; Src++)
	{
		if (*Src == '\\')
			Name.push_back('/');
		else if (*Src >= 'a' && *Src <= 'z')
			Name.push_back(*Src + 'A' - 'a');
		else
			Name.push_back(*Src);
	}

	return Name;
}

void swPartManager::Clear()
{
	// Workers have drained. External project destruction may reenter through the facade.
	std::unordered_set<Project*> ExternalProjects;
	mMutex.lock();

	for (const auto& [PartData, Owner] : mProjectParts)
		if (PartData->IsProject())
			ExternalProjects.insert(PartData->GetProject());

	mMutex.unlock();

	for (;;)
	{
		mMutex.lock();

		const auto External = std::find_if(mProjectParts.begin(), mProjectParts.end(), [&ExternalProjects](const auto& Entry)
		{
			return Entry.first->IsProject() && ExternalProjects.find(Entry.second) == ExternalProjects.end();
		});
		swPartData* PartData = nullptr;

		if (External != mProjectParts.end())
			PartData = External->first;
		else if (!mProjectParts.empty())
			PartData = mProjectParts.begin()->first;
		else if (!mParts.empty())
			PartData = mParts.begin()->second;
		else if (!mDetachedParts.empty())
			PartData = mDetachedParts.begin()->second;
		else if (!mRetiredParts.empty())
			PartData = *mRetiredParts.begin();

		mMutex.unlock();

		if (!PartData)
			break;

		RemovePartData(PartData);
	}

	mErrorMutex.lock();
	mPartDataLoadErrors.clear();
	mErrorMutex.unlock();
}

swPartData* swPartManager::FindPartData(const std::string& Name, Project* OwnerProject) const
{
	QMutexLocker Lock(&mMutex);

	if (OwnerProject)
		if (swPartData* PartData = OwnerProject->FindPartData(Name))
			return PartData;

	const auto PartIt = mParts.find(Name);

	return PartIt != mParts.end() ? PartIt->second : nullptr;
}

swPartData* swPartManager::CreateLibraryPartData(const std::string& Name, const char* FileName)
{
	QMutexLocker Lock(&mMutex);

	swPartData* PartData = nullptr;
	const auto DetachedIt = mDetachedParts.find(Name);

	// adopt the identity from before a reload
	if (DetachedIt != mDetachedParts.end())
	{
		PartData = DetachedIt->second;
		mDetachedParts.erase(DetachedIt);
	}
	else
		PartData = new swPartData(true);

	swstrcpy(PartData->mFileName, FileName);

	RegisterLibraryPartData(Name, PartData);

	return PartData;
}

swPartData* swPartManager::CreatePartData(const char* PartName, Project* OwnerProject)
{
	QMutexLocker Lock(&mMutex);

	swPartData* PartData = new swPartData(false);
	const std::string Name = NormalizePartDataName(PartName);

	PartData->CreatePart(PartName);

	if (OwnerProject)
		RegisterProjectPartData(OwnerProject, Name, PartData);
	else
		RegisterLibraryPartData(Name, PartData);

	return PartData;
}

swPartData* swPartManager::AcquireModelPartData(const char* PartName, Project* OwnerProject, bool& Reused)
{
	QMutexLocker Lock(&mMutex);

	swPartData* PartData = OwnerProject->FindPartData(NormalizePartDataName(PartName));

	Reused = PartData && !PartData->IsModel();

	if (!Reused)
		PartData = CreatePartData(PartName, OwnerProject);

	// Keep the identity alive through invalidation and adoption by the model.
	AddPartDataReference(PartData);

	return PartData;
}

void swPartManager::AddPartDataReference(swPartData* PartData)
{
	QMutexLocker Lock(&mMutex);

	PartData->AddRef();
}

int swPartManager::ReleasePartData(swPartData* PartData)
{
	QMutexLocker Lock(&mMutex);

	if (PartData->GetRefCount() == 0)
		return 0;

	const int Remaining = PartData->Release();

	Lock.unlock();

	if (Remaining == 0)
		UnloadPartData(PartData);

	return Remaining;
}

void swPartManager::RemoveTemporaryPartData()
{
	// Requery after each deletion, since external projects can release other identities.
	for (;;)
	{
		mMutex.lock();
		swPartData* Unused = nullptr;

		for (const auto& [Name, PartData] : mParts)
			if (!PartData->IsLibraryPart() && PartData->GetRefCount() == 0)
			{
				Unused = PartData;
				break;
			}

		if (!Unused)
			for (const auto& [PartData, Owner] : mProjectParts)
				if (PartData->GetRefCount() == 0)
				{
					Unused = PartData;
					break;
				}

		mMutex.unlock();

		if (!Unused)
			break;

		RemovePartData(Unused);
	}
}

// use to drop names on reload
void swPartManager::RetirePartData(swPartData* PartData)
{
	QMutexLocker Lock(&mMutex);

	UnregisterPartData(PartData);
	mRetiredParts.insert(PartData);
}

bool swPartManager::IsRetiredPartData(const swPartData* PartData) const
{
	QMutexLocker Lock(&mMutex);

	return mRetiredParts.find(const_cast<swPartData*>(PartData)) != mRetiredParts.end();
}

// use to free names before a reload
void swPartManager::DetachLibraryPartData()
{
	QMutexLocker Lock(&mMutex);

	mDetachedParts.merge(mParts);
	mParts.clear();
}

// use to drop parts a reload left behind
std::vector<swPartData*> swPartManager::RetireDetachedPartData()
{
	QMutexLocker Lock(&mMutex);

	std::vector<swPartData*> Detached;
	Detached.reserve(mDetachedParts.size());

	for (const auto& [Name, PartData] : mDetachedParts)
	{
		Detached.push_back(PartData);
		mRetiredParts.insert(PartData);
	}

	mDetachedParts.clear();

	return Detached;
}

void swPartManager::RemovePartData(swPartData* PartData)
{
	mMutex.lock();
	UnregisterPartData(PartData);
	mMutex.unlock();

	delete PartData;
}

void swPartManager::UnloadPartData(swPartData* PartData)
{
	if (PartData->IsLibraryPart())
		PartData->Unload();
	else
		RemovePartData(PartData);
}

void swPartManager::RegisterLibraryPartData(const std::string& Name, swPartData* PartData)
{
	mParts[Name] = PartData;
}

void swPartManager::RegisterProjectPartData(Project* OwnerProject, const std::string& Name, swPartData* PartData)
{
	OwnerProject->RegisterPartData(Name, PartData);

	mProjectParts[PartData] = OwnerProject;
}

void swPartManager::UnregisterPartData(swPartData* PartData)
{
	mRetiredParts.erase(PartData);

	const auto DetachedIt = std::find_if(mDetachedParts.begin(), mDetachedParts.end(), [PartData](const auto& Entry)
	{
		return Entry.second == PartData;
	});

	if (DetachedIt != mDetachedParts.end())
		mDetachedParts.erase(DetachedIt);

	const auto Local = mProjectParts.find(PartData);

	if (Local != mProjectParts.end())
	{
		if (Local->second)
			Local->second->UnregisterPartData(PartData);

		mProjectParts.erase(Local);
	}
	else
	{
		const auto PartIt = std::find_if(mParts.begin(), mParts.end(), [PartData](const auto& Entry)
		{
			return Entry.second == PartData;
		});

		if (PartIt != mParts.end())
			mParts.erase(PartIt);
	}
}

bool swPartManager::RenameModelPartData(swPartData* PartData, const char* NewName)
{
	QMutexLocker Lock(&mMutex);

	const std::string Name = NormalizePartDataName(NewName);
	const auto Local = mProjectParts.find(PartData);

	if (Local != mProjectParts.end())
	{
		if (Project* OwnerProject = Local->second)
		{
			swPartData* Existing = OwnerProject->FindPartData(Name);

			if (Existing && Existing != PartData)
				return false;

			OwnerProject->UnregisterPartData(PartData);
			OwnerProject->RegisterPartData(Name, PartData);
		}
	}
	else
	{
		swPartData* Existing = FindPartData(Name, nullptr);

		if (Existing && Existing != PartData)
			return false;

		UnregisterPartData(PartData);
		RegisterLibraryPartData(Name, PartData);
	}

	swstrcpy(PartData->mFileName, NewName);
	swstrcpy(PartData->m_strDescription, NewName);

	return true;
}

void swPartManager::ReleaseProjectPartData(Project* OwnerProject)
{
	QMutexLocker Lock(&mMutex);

	std::vector<swPartData*> Unused;

	OwnerProject->ClearPartDataIndex();

	for (auto& [PartData, Owner] : mProjectParts)
	{
		if (Owner == OwnerProject)
		{
			Owner = nullptr;

			if (PartData->GetRefCount() == 0)
				Unused.push_back(PartData);
		}
	}

	Lock.unlock();

	for (swPartData* PartData : Unused)
		UnloadPartData(PartData);
}

void swPartManager::TransferProjectPartData(Project* Source, Project* Destination)
{
	QMutexLocker Lock(&mMutex);

	std::vector<swPartData*> Unused;

	Source->TransferPartDataIndexTo(Destination);

	for (auto& [PartData, Owner] : mProjectParts)
	{
		if (Owner == Source)
		{
			Owner = Destination;

			if (PartData->GetRefCount() == 0)
				Unused.push_back(PartData);
		}
	}

	Lock.unlock();

	for (swPartData* PartData : Unused)
		UnloadPartData(PartData);
}

size_t swPartManager::GetLibraryPartDataCount() const
{
	QMutexLocker Lock(&mMutex);

	return mParts.size();
}

std::string swPartManager::GetLibraryPartDataName(const swPartData* PartData) const
{
	QMutexLocker Lock(&mMutex);

	const auto PartIt = std::find_if(mParts.begin(), mParts.end(), [PartData](const auto& Entry)
	{
		return Entry.second == PartData;
	});

	return PartIt != mParts.end() ? PartIt->first : std::string();
}

std::vector<swPartData*> swPartManager::GetLibraryPartData() const
{
	QMutexLocker Lock(&mMutex);

	std::vector<swPartData*> LibraryPartData;

	LibraryPartData.reserve(mParts.size());

	for (const auto& [Name, LibraryData] : mParts)
		LibraryPartData.push_back(LibraryData);

	return LibraryPartData;
}

std::vector<swPartData*> swPartManager::GetAllPartData() const
{
	QMutexLocker Lock(&mMutex);

	std::vector<swPartData*> AllPartData = GetLibraryPartData();
	AllPartData.reserve(AllPartData.size() + mProjectParts.size());

	for (const auto& [LocalData, Owner] : mProjectParts)
		AllPartData.push_back(LocalData);

	return AllPartData;
}

std::optional<swPartManager::FileInfo> swPartManager::GetPartDataFileInfo(const std::string& Name) const
{
	QMutexLocker Lock(&mMutex);

	const swPartData* PartData = FindPartData(Name, nullptr);

	if (!PartData)
		return std::nullopt;

	return FileInfo{ PartData->mFileName, PartData->mZipFileType, PartData->mZipFileIndex };
}

void swPartManager::SetPartDataLoadError(const swPartData* PartData, QString Error)
{
	QMutexLocker ErrorLock(&mErrorMutex);

	if (Error.isEmpty())
		mPartDataLoadErrors.erase(PartData);
	else
		mPartDataLoadErrors.insert_or_assign(PartData, std::move(Error));
}

QString swPartManager::GetPartDataLoadError(const swPartData* PartData) const
{
	QMutexLocker ErrorLock(&mErrorMutex);

	const auto It = mPartDataLoadErrors.find(PartData);

	return It == mPartDataLoadErrors.end() ? QString() : It->second;
}

void swPartManager::ClearPartDataLoadError(const swPartData* PartData)
{
	QMutexLocker ErrorLock(&mErrorMutex);

	mPartDataLoadErrors.erase(PartData);
}
