#pragma once

#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
using swPartManagerMutex = QRecursiveMutex;
#else

// Use recursive QMutex for Qt 5 compatibility.
class swPartManagerMutex : public QMutex
{
public:
	swPartManagerMutex()
		: QMutex(QMutex::Recursive)
	{
	}
};
#endif

enum class swZipFileType;

// Creates and manages part identities.
class swPartManager
{
public:
	swPartManager() = default;
	~swPartManager();

	swPartManager(const swPartManager&) = delete;
	swPartManager(swPartManager&&) = delete;
	swPartManager& operator=(const swPartManager&) = delete;
	swPartManager& operator=(swPartManager&&) = delete;

	// Normalizes case and slashes.
	static std::string NormalizePartDataName(const char* PartName);

	// Deletes all identities.
	void Clear();

	// Returns a borrowed project identity.
	swPartData* FindPartData(const std::string& Name, Project* OwnerProject) const;

	// Creates a library identity.
	swPartData* CreateLibraryPartData(const std::string& Name, const char* FileName);

	// Creates a non library identity.
	swPartData* CreatePartData(const char* PartName, Project* OwnerProject);

	// Returns a referenced local identity.
	swPartData* AcquireModelPartData(const char* PartName, Project* OwnerProject, bool& Reused);

	// Renames an identity.
	bool RenameModelPartData(swPartData* PartData, const char* NewName);

	// Keeps an identity alive.
	void AddPartDataReference(swPartData* PartData);

	// Releases one reference.
	int ReleasePartData(swPartData* PartData);

	// Sparse load errors.
	void SetPartDataLoadError(const swPartData* PartData, QString Error);
	QString GetPartDataLoadError(const swPartData* PartData) const;
	void ClearPartDataLoadError(const swPartData* PartData);

	// Removes unreferenced non-library identities without changing library-part eviction policy.
	void RemoveTemporaryPartData();

	// Removes the identity from the name indexes.
	void RetirePartData(swPartData* PartData);
	bool IsRetiredPartData(const swPartData* PartData) const;

	// Moves identities aside for a reload.
	void DetachLibraryPartData();

	// Retires identities no reload adopted and returns them.
	std::vector<swPartData*> RetireDetachedPartData();

	// Releases the project identities.
	void ReleaseProjectPartData(Project* OwnerProject);

	// Transfers registrations and indexes.
	void TransferProjectPartData(Project* Source, Project* Destination);

	// Returns the library registration count.
	size_t GetLibraryPartDataCount() const;

	// Returns the library index key.
	std::string GetLibraryPartDataName(const swPartData* PartData) const;

	// Returns the parts in name order.
	std::vector<swPartData*> GetLibraryPartData() const;

	// Returns the library and local identities.
	std::vector<swPartData*> GetAllPartData() const;

	struct FileInfo
	{
		std::string FileName;
		swZipFileType ZipFileType;
		int ZipFileIndex;
	};

	// Copies library file metadata.
	std::optional<FileInfo> GetPartDataFileInfo(const std::string& Name) const;

private:
	// Registers a new identity.
	void RegisterLibraryPartData(const std::string& Name, swPartData* PartData);

	// Also updates the owning project's borrowed name index.
	void RegisterProjectPartData(Project* OwnerProject, const std::string& Name, swPartData* PartData);

	// Removes registry entries first.
	void UnregisterPartData(swPartData* PartData);

	// Deletes a non library identity.
	void RemovePartData(swPartData* PartData);

	// Keeps library identities registered; deletes non-library identities.
	void UnloadPartData(swPartData* PartData);

	// Recursive registry lock.
	mutable swPartManagerMutex mMutex;
	std::map<std::string, swPartData*> mParts;

	// Owner may be null.
	std::unordered_map<swPartData*, Project*> mProjectParts;

	// Identities replaced by a reload.
	std::unordered_set<swPartData*> mRetiredParts;

	// Identities waiting for adoption.
	std::map<std::string, swPartData*> mDetachedParts;

	// No error lock while unloading.
	mutable QMutex mErrorMutex;
	std::unordered_map<const swPartData*, QString> mPartDataLoadErrors;
};
