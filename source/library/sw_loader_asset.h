#pragma once

#include "sw_types_asset.h"

struct swPartControlPoint;

// Loads parts, textures and generated meshes.
class swAssetLoader : public QObject
{
public:
	swAssetLoader(swPartsLibrary* Library);
	~swAssetLoader() override;

	// Queues or promotes a part load.
	void RequestPartLoad(swPartData* PartData, swLoadPriority LoadPriority);

	// Waits for the active requests.
	bool WaitForParts(const std::vector<swPartData*>& Parts, const std::function<void(int, int)>& Progress = nullptr);

	// Cancels work for a released part.
	void CancelPartLoad(swPartData* PartData);

	// Discards pending load results.
	void InvalidatePartLoad(swPartData* PartData);

	// Builds a generated mesh.
	void RequestSynthMesh(swPart* Part, swPartData* PartData, std::shared_ptr<const swSynthesisInformation> SynthInfo, std::vector<swPartControlPoint> ControlPoints);

	// Waits for the active generations.
	bool WaitForSynthMeshes(const std::vector<swPart*>& Parts);

	// Cancels a generation.
	void CancelSynthMesh(swPart* Part);

	// Decodes a texture and waits.
	bool WaitForTexture(swTexture* Texture, swFileSource Source);

	// Queues a texture decode.
	void RequestTextureLoad(swTexture* Texture, swFileSource Source, swLoadPriority LoadPriority);

	// True while work is outstanding.
	bool HasPendingWork();

	// Pauses dispatch and drains jobs.
	void PauseQueuedWork();

	// Refreshes snapshots and resumes dispatch.
	void ResumeQueuedWork();

private:
	// Request lifecycle on the ui thread.
	enum class RequestState
	{
		Queued,

		// Worker job outstanding.
		AwaitingCompletion,

		// Completion, dependencies or retries.
		AwaitingPublication,
		Succeeded,
		Failed,
		Cancelled
	};

	struct RequestBase
	{
		swLoadPriority LoadPriority;
		qint64 QueuedAt;
		RequestState State = RequestState::Queued;

		bool IsFinished() const
		{
			return State == RequestState::Succeeded || State == RequestState::Failed || State == RequestState::Cancelled;
		}
	};

	// Part load request.
	struct PartRequest : RequestBase
	{
		swPartDataReference PartData;
		swPartSourceSnapshot Source;
		std::unique_ptr<swMesh> StagedMesh;
		bool EmptyGeometry = false;
		bool SaveCache = false;
		QString Error;
	};

	// Texture load request.
	struct TextureRequest : RequestBase
	{
		swTextureReference Texture;
		swFileSource Source;
		int UploadAttempts = 0;
		qint64 RetryAt = 0;
	};

	// Generated mesh request.
	struct SynthRequest : RequestBase
	{
		swPart* Part;

		// Borrowed from the instance.
		swPartData* PartData;
		std::shared_ptr<const swSynthesisInformation> SynthInfo;
		std::vector<swPartControlPoint> ControlPoints;
		std::atomic_bool Cancelled = false;
		std::unique_ptr<swMesh> StagedMesh;
		QString Error;
	};

	using PartRequestMap = std::map<swPartData*, std::shared_ptr<PartRequest>>;
	using TextureRequestMap = std::map<swTexture*, std::shared_ptr<TextureRequest>>;
	using SynthRequestMap = std::map<swPart*, std::shared_ptr<SynthRequest>>;

	// Private worker result.
	struct Completion
	{
		std::shared_ptr<PartRequest> RequestedPart;
		swPartBuildResult PartResult;
		std::shared_ptr<TextureRequest> RequestedTexture;
		swTextureBuildResult TextureResult;
		std::shared_ptr<SynthRequest> RequestedSynth;
		std::unique_ptr<swMesh> SynthMesh;
	};

	// Retains the base identity.
	struct SynthNotification
	{
		std::shared_ptr<SynthRequest> RequestedSynth;
		swPartDataReference PartData;
	};

	// Promote requests on the UI thread.
	void PromotePartPriority(const std::shared_ptr<PartRequest>& RequestedPart, swLoadPriority LoadPriority);

	// Fills free job slots.
	void DispatchJobs();

	// Processes a batch of results.
	void ProcessCompletions();

	// Handle worker results on the UI thread.
	void ProcessPartCompletion(Completion& Completed);
	void ProcessSynthCompletion(Completion& Completed);
	void ProcessTextureCompletion(Completion& Completed);

	// Settles mesh texture dependencies.
	bool ResolveAndQueueMeshTextures(swMesh& Mesh, const swPartSourceSnapshot& Source, swLoadPriority LoadPriority, QString& Error);
	static bool MeshTexturesSettled(const swMesh& Mesh, QString& Error);

	// Uploads textures with retries.
	void UploadTextureRequest(const std::shared_ptr<TextureRequest>& RequestedTexture);
	void CompleteTextureRequest(const std::shared_ptr<TextureRequest>& RequestedTexture);

	// Publishes meshes once textures settle.
	void FinishPartsWithSettledTextures();
	void FinishSynthsWithSettledTextures();
	void CompletePartRequest(const std::shared_ptr<PartRequest>& RequestedPart);
	void CompleteSynthRequest(const std::shared_ptr<SynthRequest>& RequestedSynth);

	// Sleeps until the next completion.
	void WaitForCompletionOrRetry();

	// Delivers deferred signals.
	bool event(QEvent* Event) override;
	void DispatchNotifications();

	// Removes a request.
	void DiscardPartRequest(std::shared_ptr<PartRequest> Request);

	// Drops unused texture work.
	void CancelUnusedTextureRequests();

	// Drains and joins workers.
	void WaitForAllWork();

	// Stops and releases everything.
	void Shutdown();

	swPartsLibrary* const mLibrary;

	// UI-thread scheduling state.
	PartRequestMap mPartRequests;
	TextureRequestMap mTextureRequests;
	SynthRequestMap mSynthRequests;

	// Job bookkeeping on the ui thread.
	std::vector<QFuture<void>> mFutures;

	// Includes completed jobs.
	int mActiveJobs = 0;

	// Temporarily suspend dispatch while shared library data changes.
	bool mPaused = false;

	// Permanently stop accepting work and discard results during shutdown.
	bool mStopping = false;

	// Worker to ui result handoff.
	QMutex mCompletionMutex;
	QWaitCondition mCompletionReady;
	std::deque<Completion> mCompletions;

	// Deferred public signals.
	std::deque<std::shared_ptr<PartRequest>> mPartNotifications;
	std::deque<SynthNotification> mSynthNotifications;

	Q_DECLARE_TR_FUNCTIONS(swAssetLoader)
};
