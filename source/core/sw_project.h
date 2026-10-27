#pragma once

#include "sw_application.h"
#include "sw_types_asset.h"
#include "sw_result.h"

#define SW_HTML_SINGLE_PAGE    0x01
#define SW_HTML_INDEX         0x02
#define SW_HTML_LIST_END       0x08
#define SW_HTML_LIST_STEP      0x10
#define SW_HTML_SUBMODELS     0x40
#define SW_HTML_CURRENT_ONLY  0x80

class swHTMLExportOptions
{
public:
	swHTMLExportOptions(const Project* Project);
	void SaveDefaults();

	QString PathName;
	bool TransparentImages;
	bool SubModels;
	bool CurrentOnly;
	bool SinglePage;
	bool IndexPage;
	int StepImagesWidth;
	int StepImagesHeight;
	bool PartsListStep;
	bool PartsListEnd;
};

struct swImageDialogOptions
{
	QString FilePath;
	int Width;
	int Height;
	int Start;
	int End;
};

struct swSetInventoryItem
{
	QByteArray PartID;
	int Quantity;
	int ColorCode;
};

struct swEmbeddedData
{
	bool HasValidEncoding() const;
	QString FileName;
	QByteArray EncodedData;
	QByteArray RawBlock;
	QString Error;
};

class Project
{
public:
	Project(bool IsPreview = false);
	~Project();

	Project(const Project&) = delete;
	Project(Project&&) = delete;
	Project& operator=(const Project&) = delete;
	Project& operator=(Project&&) = delete;

	const std::vector<std::unique_ptr<swModel>>& GetModels() const
	{
		return mModels;
	}

	const std::vector<std::shared_ptr<const swEmbeddedData>>& GetEmbeddedData() const
	{
		return mEmbeddedData;
	}

	const std::map<QString, std::shared_ptr<const swEmbeddedData>>& GetEmbeddedDataLookup() const
	{
		return mEmbeddedDataIndex;
	}

	std::shared_ptr<const swEmbeddedData> FindEmbeddedData(const QString& FileName) const;
	swModel* GetModel(const QString& FileName) const;
	swPartData* FindPartData(const std::string& Name) const;
	void RegisterPartData(const std::string& Name, swPartData* PartData);
	void UnregisterPartData(swPartData* PartData);
	void ClearPartDataIndex();
	void TransferPartDataIndexTo(Project* Destination);

	swModel* GetActiveModel() const
	{
		return mActiveModel;
	}

	swModel* GetMainModel() const
	{
		return !mModels.empty() ? mModels[0].get() : nullptr;
	}

	bool IsPreview() const
	{
		return mIsPreview;
	}

	bool DefersModelMeshRequests() const
	{
		return mDeferModelMeshRequests;
	}

	void SetDeferModelMeshRequests(bool Defer)
	{
		mDeferModelMeshRequests = Defer;
	}

	bool IsModified() const;
	void MarkAsModified();
	QString GetTitle() const;

	QString GetFileName() const
	{
		return mFileName;
	}

	QString GetImageFileName(bool AllowCurrentFolder) const;
	void UpdateFileWatcher();

	swInstructions* GetInstructions();

	void SetActiveModel(swModel* Model, bool UpdateInterface);
	void SetActiveModel(int ModelIndex, bool UpdateInterface);
	void SetActiveModel(const QString& FileName, bool UpdateInterface);

	swModel* CreateNewModel(bool ShowModel);
	QString GetNewModelName(QWidget* ParentWidget, const QString& DialogTitle, const QString& CurrentName, const QStringList& ExistingModels) const;
	void ShowModelListDialog();

	bool Load(const QString& FileName, bool ShowErrors);
	void AddLoadWarning(const QString& Warning);
	std::vector<QString> GetLoadWarningDetails() const;
	swResult<void> Save(const QString& FileName);
	bool Save(QTextStream& Stream) const;
	swResult<void> Merge(const std::vector<Project*>& Sources);
	bool ImportLDD(const QString& FileName);
	bool ImportInventory(const std::vector<swSetInventoryItem>& SetInventory, const QString& Name, const QString& Description);

	swResult<void> SaveImage(const swImageDialogOptions& Options);
	std::vector<swPartDataReference> GetRequiredPartData() const;
	std::vector<swPart*> GetRequiredSynthParts() const;
	swResult<void> EnsureAssetsReady() const;
	swResult<void> ExportCurrentStep(const QString& FileName);
	swResult<QByteArray> SerializeModel(const swModel* Model, bool SelectedOnly, swStep LastStep) const;
	// Caller owns the returned model; imported dependencies remain in this project.
	swResult<swModel*> ImportClipboard(const QByteArray& FileData);
	swResult<void> ExportModel(const QString& FileName, swModel* Model) const;
	swResult<void> Export3DStudio(const QString& FileName);
	swResult<void> ExportBrickLink();
	swResult<void> ExportCOLLADA(const QString& FileName);
	swResult<void> ExportCSV(const QString& FileName);
	swResult<void> ExportHTML(const swHTMLExportOptions& Options);
	swResult<void> ExportPOVRay(const QString& FileName);
	swResult<void> ExportWavefront(const QString& FileName);

	void UpdatePartData(swPartData* PartData) const;

private:
	struct LoadWarning
	{
		QString FileName;
		QString Message;
	};
	void AddLoadWarning(const LoadWarning& Warning);

	struct FileState
	{
		QByteArray Hash;
		bool Exists = false;
		bool Readable = false;
	};
	struct DocumentBlock
	{
		swModel* Model = nullptr;
		std::shared_ptr<const swEmbeddedData> Data;
		QByteArray Raw;
		QByteArray Baseline;
		QString OriginalName;
		bool ExplicitModel = false;
		bool DuplicateModel = false;
	};
	static QByteArray SerializeModelText(const swModel* Model, bool SelectedOnly, swStep LastStep);
	static QByteArray SerializeStoredModel(const swModel* Model, const QString& Name, const DocumentBlock* Original, bool NeedsHeader, bool SelectedOnly, swStep LastStep);
	static bool WriteRaw(QTextStream& Stream, const QByteArray& Bytes);
	static bool SameEmbeddedData(const swEmbeddedData& First, const swEmbeddedData& Second);
	static QString NormalizeEmbeddedDataName(const QString& FileName);
	void LoadLDrawDocument(const QByteArray& FileData, const Project* ReferenceProject, bool ReuseExistingModels);
	std::vector<QString> GetMergedAssetDirectories(const swModel* Model) const;
	swResult<void> ValidateMergedTextureBindings(const Project& Source, const std::map<QString, std::shared_ptr<const swEmbeddedData>>& MergedData, const swModel* ClipboardRoot) const;
	swResult<void> MergeInternal(const std::vector<Project*>& Sources, const swModel* ClipboardRoot);
	static FileState ReadFileState(const QString& FileName);
	bool HasFileChanged(const QString& Path);
	static QString MakeExportNameFragment(const QString& Name);
	static QString MakeUniqueExportIdentifier(const QString& BaseName, std::set<QString>& UsedNames, const QString& ReservedSuffix);

protected:
	static bool CanShareMergePartData(const swPartData* Existing, const swPartData* Incoming);
	QString GetExportFileName(const QString& FileName, const QString& DefaultExtension, const QString& DialogTitle, const QString& DialogFilter) const;

	std::vector<swModelPartsEntry> GetModelParts();
	void SetFileName(const QString& FileName);

	QUuid mClipboardIdentity = QUuid::createUuid();
	bool mIsPreview;
	// External projects can parse their models before queuing direct meshes.
	bool mDeferModelMeshRequests = false;
	bool mModified;
	QString mFileName;
	QByteArray mFileHash;
	bool mFileExists = false;
	bool mFileReadable = false;
	int mFileReadAttempts = 0;
	QFileSystemWatcher mFileWatcher;
	QTimer mFileCheckTimer;

	std::vector<DocumentBlock> mDocumentBlocks;
	std::vector<LoadWarning> mLoadWarnings;
	std::vector<std::shared_ptr<const swEmbeddedData>> mEmbeddedData;
	// First definition wins; retain duplicate blocks in mEmbeddedData for saving.
	std::map<QString, std::shared_ptr<const swEmbeddedData>> mEmbeddedDataIndex;
	std::vector<std::unique_ptr<swModel>> mModels;
	// Maps normalized local filenames to swPartData objects visible in this project.
	// This map does not own them; swPartsLibrary tracks and deletes them.
	std::map<std::string, swPartData*> mPartIndex;
	swModel* mActiveModel;
	std::unique_ptr<swInstructions> mInstructions;

	Q_DECLARE_TR_FUNCTIONS(Project);
};

inline swModel* swGetActiveModel()
{
	const Project* const Project = swGetActiveProject();
	return Project ? Project->GetActiveModel() : nullptr;
}

swResult<void> swExportCSV(const QString& FileName, const swPartsList& PartsList);
