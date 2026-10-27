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

// Creates and indexes part identities and manages their references and lifetime.
// The library handles source resolution, loading and cancellation.
// Mutations and borrowed identities belong to the UI thread. Workers may copy
// file metadata and access sparse errors; registry locking ends before teardown.
class swPartManager
{
public:
	swPartManager() = default;
	~swPartManager();

	swPartManager(const swPartManager&) = delete;
	swPartManager(swPartManager&&) = delete;
	swPartManager& operator=(const swPartManager&) = delete;
	swPartManager& operator=(swPartManager&&) = delete;

	// Normalizes case and slashes, preserving the part name length limit.
	static std::string NormalizePartDataName(const char* PartName);

	// Deletes all managed identities, regardless of reference count.
	// Drain workers and release consumers before calling; the library must remain alive.
	void Clear();

	// Returns a borrowed project identity, falling back to the library. No creation or acquisition.
	// A null owner searches only the shared library registry.
	swPartData* FindPartData(const std::string& Name, Project* OwnerProject) const;

	// Creates a library identity with a discovery index key and source filename.
	// The returned identity is registered with zero references; source metadata is supplied by the library.
	swPartData* CreateLibraryPartData(const std::string& Name, const char* FileName);

	// Creates a non-library identity, registered locally or in the shared registry when ownerless.
	// Returns a borrowed identity with zero references. The caller supplies missing/external state.
	swPartData* CreatePartData(const char* PartName, Project* OwnerProject);

	// Creates or reuses a local identity and acquires one reference for the model.
	// The library adopts the reference and invalidates reused data after this returns.
	swPartData* AcquireModelPartData(const char* PartName, Project* OwnerProject, bool& Reused);

	// Renames the identity and its index key, rejecting collisions in the same scope.
	bool RenameModelPartData(swPartData* PartData, const char* NewName);

	// Keeps an identity alive for another owner. Never starts loading.
	void AddPartDataReference(swPartData* PartData);

	// Releases one reference, unloading library parts or deleting non-library identities at zero.
	// Returns the remaining count; zero may mean the pointer was deleted. Already-zero is a no-op.
	int ReleasePartData(swPartData* PartData);

	// Sparse errors are independently synchronized and cleared before resource teardown.
	void SetPartDataLoadError(const swPartData* PartData, QString Error);
	QString GetPartDataLoadError(const swPartData* PartData) const;
	void ClearPartDataLoadError(const swPartData* PartData);

	// Removes unreferenced non-library identities without changing library-part eviction policy.
	void RemoveTemporaryPartData();

	// Detaches the project's borrowed index and deletes its unreferenced local identities.
	void ReleaseProjectPartData(Project* OwnerProject);

	// Transfers registrations and borrowed indexes, preserving destination name collisions.
	// Deletes transferred identities that have no references.
	void TransferProjectPartData(Project* Source, Project* Destination);

	// Returns the number of library registrations, including projectless missing identities.
	size_t GetLibraryPartDataCount() const;

	// Returns the library index key for an identity, or an empty string if not registered there.
	std::string GetLibraryPartDataName(const swPartData* PartData) const;

	// Returns a borrowed snapshot in library name order. Caller prevents identity teardown.
	std::vector<swPartData*> GetLibraryPartData() const;

	// Returns a borrowed snapshot of library and local identities. Caller prevents identity teardown.
	std::vector<swPartData*> GetAllPartData() const;

	struct FileInfo
	{
		std::string FileName;
		swZipFileType ZipFileType;
		int ZipFileIndex;
	};

	// Copies library file metadata under the registry lock, or returns no value if missing.
	// Workers use the copy after unlocking instead of accessing the registered identity.
	std::optional<FileInfo> GetPartDataFileInfo(const std::string& Name) const;

private:
	// Register a newly created identity without changing its reference count.
	void RegisterLibraryPartData(const std::string& Name, swPartData* PartData);

	// Also updates the owning project's borrowed name index.
	void RegisterProjectPartData(Project* OwnerProject, const std::string& Name, swPartData* PartData);

	// Removes registry and borrowed project entries before destruction can reenter.
	void UnregisterPartData(swPartData* PartData);

	// Unregisters and deletes a non-library identity or an identity being cleared.
	void RemovePartData(swPartData* PartData);

	// Keeps library identities registered; deletes non-library identities.
	void UnloadPartData(swPartData* PartData);

	// Recursive for compound operations that reuse registry helpers. Never held during deletion.
	mutable swPartManagerMutex mMutex;
	std::map<std::string, swPartData*> mParts;

	// An owner may be null after its project is destroyed while references remain.
	std::unordered_map<swPartData*, Project*> mProjectParts;

	// No error lock is held while unloading or deleting identities.
	mutable QMutex mErrorMutex;
	std::unordered_map<const swPartData*, QString> mPartDataLoadErrors;
};
