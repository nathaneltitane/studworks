#pragma once

#include "sw_application.h"
#include "sw_types_asset.h"
#include "sw_result.h"

enum class swImageBackground;

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

enum class swImageMotion
{
	Turntable,
	Steps
};

enum class swImageNextStep
{
	Off,
	Turns,
	Seconds
};

// Turns the model around one axis or plays the steps in an animated GIF.
struct swImageTurntable
{
	bool Enabled = false;
	swImageMotion Motion = swImageMotion::Turntable;
	swTurntableAxis Axis = swTurntableAxis::Z;
	bool FullTurn = true;
	int AngleStart = -45;
	int AngleEnd = 45;
	int Turns = 1;
	int Frames = 36;
	double FrameRate = 12.0;
	swImageNextStep NextStep = swImageNextStep::Off;
	int NextStepTurns = 1;
	double NextStepSeconds = 2.0;
	double HoldLast = 1.0;

	// Steps play one per frame without turning.
	bool IsTurning() const
	{
		return Enabled && Motion == swImageMotion::Turntable;
	}

	// Steps share one file when the animation moves through them.
	bool IsStepping() const
	{
		return Enabled && (Motion == swImageMotion::Steps || NextStep != swImageNextStep::Off);
	}

	// Frames each step shows while the model turns.
	int GetStepFrames() const;

	// Frames in one loop over every turn and step.
	int GetFrameCount(int StepCount) const;

	// Steps past the first shown on a frame.
	int GetFrameStep(int Frame) const;

	// Angle sweeps go forward and back so the loop has no jump.
	float GetFrameAngle(int Frame) const;

	// Seconds the finished model stays before the loop restarts.
	double GetHoldLast() const
	{
		return IsStepping() ? HoldLast : 0.0;
	}

	// Delays are in hundredths of a second and follow the frame rate.
	int GetFrameDelay(int Frame, int FrameCount) const;
};

enum class swImageFit
{
	CurrentView,
	FitModel,
	FitEachStep
};

enum class swImageCenter
{
	WorldOrigin,
	ModelOrigin,
	ModelCenter
};

enum class swImageFitScope
{
	WholeModel,
	ShownSteps
};

// Places the camera for saved images and the turntable orbit.
struct swImageFraming
{
	swImageFit Fit = swImageFit::FitModel;
	swImageCenter Center = swImageCenter::ModelCenter;
	swImageFitScope Scope = swImageFitScope::WholeModel;

	// Empty space around the model in percent of the image.
	int Margin = 10;
};

enum class swImageSteps
{
	Current,
	All,
	Range
};

struct swImageDialogOptions
{
	QString FilePath;
	int Width;
	int Height;
	int Start;
	int End;
	double Ratio;
	swImageSteps Steps;
	swImageBackground Background;
	swImageTurntable Turntable;
	swImageFraming Framing;
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
	void MigrateMetaCommands();

	bool HasLeoCADMetaCommands() const
	{
		return mLeoCADMetaCommands;
	}
	QString GetTitle() const;

	QString GetFileName() const
	{
		return mFileName;
	}

	QString GetImageFileName(bool AllowCurrentFolder) const;
	void UpdateFileWatcher();

	swInstructions* GetInstructions();
	std::unique_ptr<swInstructions> TakeInstructions();
	void SetInstructions(std::unique_ptr<swInstructions> Instructions);

	void SetActiveModel(swModel* Model, bool UpdateInterface);
	void SetActiveModel(int ModelIndex, bool UpdateInterface);
	void SetActiveModel(const QString& FileName, bool UpdateInterface);

	swModel* CreateNewModel(bool ShowModel);
	QString GetNewModelName(QWidget* ParentWidget, const QString& DialogTitle, const QString& CurrentName, const QStringList& ExistingModels) const;
	void ShowModelListDialog();

	bool Load(const QString& FileName, bool ShowErrors);
	bool LoadText(const QByteArray& FileData, const QString& FileName, const QString& ModelName);
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

	// Caller owns the returned model.
	swResult<swModel*> ImportClipboard(const QByteArray& FileData);
	swResult<void> ExportModel(const QString& FileName, swModel* Model) const;
	swResult<void> Export3DStudio(const QString& FileName);
	swResult<void> ExportBrickLink();
	swResult<void> ExportCOLLADA(const QString& FileName);
	swResult<void> ExportCSV(const QString& FileName);
	swResult<void> ExportHTML(const swHTMLExportOptions& Options);
	swResult<void> ExportPOVRay(const QString& FileName, const swCamera* ExportCamera = nullptr, swStep Step = SW_STEP_MAXIMUM, bool Transparent = false);
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
	bool LoadDocument(const QByteArray& FileData, const QString& FileName, bool ShowErrors);
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

	std::vector<swModelPartsEntry> GetModelParts(swStep Step = SW_STEP_MAXIMUM);
	void SetFileName(const QString& FileName);

	QUuid mClipboardIdentity = QUuid::createUuid();
	bool mIsPreview;

	// External projects parse models first.
	bool mDeferModelMeshRequests = false;
	bool mModified;

	// LeoCAD meta commands are kept until migrated.
	bool mLeoCADMetaCommands = false;
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

	// The first definition wins.
	std::map<QString, std::shared_ptr<const swEmbeddedData>> mEmbeddedDataIndex;
	std::vector<std::unique_ptr<swModel>> mModels;

	// Local filenames to visible part data.
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
