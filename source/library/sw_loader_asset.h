#pragma once

#include "sw_types_asset.h"

struct swPartControlPoint;

// Owned by swPartsLibrary. Instance methods run on the UI thread and schedule private worker builds;
// UI-thread completion processing resolves colors, uploads textures, and publishes meshes.
// See documentation/asset-loading.md for readiness, ownership, and caller policies.
class swAssetLoader : public QObject
{
public:
	swAssetLoader(swPartsLibrary* Library);
	~swAssetLoader() override;

	// Queue or promote a part the library has marked Loading. Adds no consumer reference.
	// Priority controls scheduling only; the library handles blocking waits and asset readiness.
	void RequestPartLoad(swPartData* PartData, swLoadPriority LoadPriority);

	// Promote and wait for requests active at entry; false means a request failed or was cancelled.
	// The library checks final asset readiness, including identities with no active request.
	// Adds no consumer references and does not traverse container children or generated meshes.
	// Progress receives the finished and total request counts while waiting.
	bool WaitForParts(const std::vector<swPartData*>& Parts, const std::function<void(int, int)>& Progress = nullptr);

	// Called when the last consumer releases the part. Cancels queued work and reports it to the library;
	// running work finishes privately and is discarded if the part is still unused.
	void CancelPartLoad(swPartData* PartData);

	// Discard pending load results and notifications after the library invalidates readiness.
	// Asset meshes, readiness, and errors belong to the library.
	void InvalidatePartLoad(swPartData* PartData);

	// Build this instance's generated mesh from definition/control-point snapshots supplied by the library.
	// Cancel any older request and keep the previous mesh visible while the replacement is built.
	void RequestSynthMesh(swPart* Part, swPartData* PartData, std::shared_ptr<const swSynthesisInformation> SynthInfo, std::vector<swPartControlPoint> ControlPoints);

	// Promote and wait for generations active at entry; false means a generation failed or was cancelled.
	// The library checks final generated-mesh readiness. The caller keeps the instances alive through the wait.
	bool WaitForSynthMeshes(const std::vector<swPart*>& Parts);

	// Cancel this instance's generation and suppress its deferred notifications.
	// Running jobs stop cooperatively; any published mesh is kept.
	void CancelSynthMesh(swPart* Part);

	// Start or promote decoding as needed, then wait through upload and retries. The caller retains the texture.
	bool WaitForTexture(swTexture* Texture, swFileSource Source);

	// Queue or promote decoding without waiting or acquiring a consumer reference.
	void RequestTextureLoad(swTexture* Texture, swFileSource Source, swLoadPriority LoadPriority);

	// True while part, texture, or generated-mesh requests or worker completions remain unsettled.
	// Includes paused work and upload retries; deferred notifications alone do not count.
	bool HasPendingWork();

	// Pause dispatch and drain assigned jobs before shared library data changes.
	// Discard meshes still awaiting textures so they can be rebuilt with the new settings.
	void PauseQueuedWork();

	// Refresh queued part and texture source snapshots and resume dispatch with the current settings.
	// Does not wait for pending work to finish.
	void ResumeQueuedWork();

private:
	// UI-thread request lifecycle; synth workers use a separate cooperative cancellation token.
	enum class RequestState
	{
		Queued,
		AwaitingCompletion, // A worker job or its completion is outstanding.
		AwaitingPublication, // UI-thread completion, texture dependencies, or upload retries.
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

	// Owns one definition reference through completion and deferred notification.
	// Reset on the UI thread before worker-shared request storage can be destroyed.
	// One worker job builds and converts geometry; textures settle before publication.
	struct PartRequest : RequestBase
	{
		swPartDataReference PartData;
		swPartSourceSnapshot Source;
		std::unique_ptr<swMesh> StagedMesh;
		bool EmptyGeometry = false;
		bool SaveCache = false;
		QString Error;
	};

	// Owns a texture reference through CPU decoding and UI-thread upload/retries.
	// Reset on the UI thread before worker-shared request storage can be destroyed.
	struct TextureRequest : RequestBase
	{
		swTextureReference Texture;
		swFileSource Source;
		int UploadAttempts = 0;
		qint64 RetryAt = 0;
	};

	// The instance cancels before destruction; workers use the copied definition/controls.
	// Cancelled is also checked during expensive generation and conversion.
	struct SynthRequest : RequestBase
	{
		swPart* Part;
		swPartData* PartData; // Borrowed from the instance until cancellation or publication.
		std::shared_ptr<const swSynthesisInformation> SynthInfo;
		std::vector<swPartControlPoint> ControlPoints;
		std::atomic_bool Cancelled = false;
		std::unique_ptr<swMesh> StagedMesh;
		QString Error;
	};

	using PartRequestMap = std::map<swPartData*, std::shared_ptr<PartRequest>>;
	using TextureRequestMap = std::map<swTexture*, std::shared_ptr<TextureRequest>>;
	using SynthRequestMap = std::map<swPart*, std::shared_ptr<SynthRequest>>;

	// Workers hand off one private result here; no live asset is published by a worker.
	struct Completion
	{
		std::shared_ptr<PartRequest> RequestedPart;
		swPartBuildResult PartResult;
		std::shared_ptr<TextureRequest> RequestedTexture;
		swTextureBuildResult TextureResult;
		std::shared_ptr<SynthRequest> RequestedSynth;
		std::unique_ptr<swMesh> SynthMesh;
	};

	// Retains the base identity; cancellation makes delivery safe after instance deletion.
	struct SynthNotification
	{
		std::shared_ptr<SynthRequest> RequestedSynth;
		swPartDataReference PartData;
	};

	// Promote requests on the UI thread.
	void PromotePartPriority(const std::shared_ptr<PartRequest>& RequestedPart, swLoadPriority LoadPriority);

	// Fill available job slots using queued request priority and age.
	void DispatchJobs();

	// Process a bounded batch of results and upload retries, then start more jobs.
	// Consumer signals remain deferred.
	void ProcessCompletions();

	// Handle worker results on the UI thread.
	void ProcessPartCompletion(Completion& Completed);
	void ProcessSynthCompletion(Completion& Completed);
	void ProcessTextureCompletion(Completion& Completed);

	// Mesh texture dependencies: true means settled; failures are reported in Error.
	bool ResolveAndQueueMeshTextures(swMesh& Mesh, const swPartSourceSnapshot& Source, swLoadPriority LoadPriority, QString& Error);
	static bool MeshTexturesSettled(const swMesh& Mesh, QString& Error);

	// Texture upload, bounded retries, and dependency settlement.
	void UploadTextureRequest(const std::shared_ptr<TextureRequest>& RequestedTexture);
	void CompleteTextureRequest(const std::shared_ptr<TextureRequest>& RequestedTexture);

	// Publish parts and generated meshes after their texture dependencies settle.
	void FinishPartsWithSettledTextures();
	void FinishSynthsWithSettledTextures();
	void CompletePartRequest(const std::shared_ptr<PartRequest>& RequestedPart);
	void CompleteSynthRequest(const std::shared_ptr<SynthRequest>& RequestedSynth);

	// Sleep until a worker completion arrives or the next texture upload retry is due.
	void WaitForCompletionOrRetry();

	// Process completion events and deliver deferred consumer signals outside synchronous waits.
	bool event(QEvent* Event) override;
	void DispatchNotifications();

	// Remove a request; outstanding worker completions retain their load hold.
	void DiscardPartRequest(std::shared_ptr<PartRequest> Request);

	// Drop queued texture work and upload retries that have no remaining consumers.
	void CancelUnusedTextureRequests();

	// Drain pending work and join workers during shutdown; completion does not imply success.
	void WaitForAllWork();

	// Stop dispatch, cancel pending work, join workers, and release loader-owned references.
	void Shutdown();

	swPartsLibrary* const mLibrary;

	// UI-thread scheduling state.
	PartRequestMap mPartRequests;
	TextureRequestMap mTextureRequests;
	SynthRequestMap mSynthRequests;

	// UI-thread job bookkeeping; futures may include finished jobs awaiting cleanup.
	std::vector<QFuture<void>> mFutures;
	int mActiveJobs = 0; // Includes completed jobs until the UI consumes their results.

	// Temporarily suspend dispatch while shared library data changes.
	bool mPaused = false;

	// Permanently stop accepting work and discard results during shutdown.
	bool mStopping = false;

	// Worker-to-UI result handoff; the mutex protects the completion queue.
	// Workers wake synchronous UI waits when they enqueue a completion.
	QMutex mCompletionMutex;
	QWaitCondition mCompletionReady;
	std::deque<Completion> mCompletions;

	// UI-thread only; public signals are deferred until outside synchronous waits.
	// Retains each request and its definition reference through notification delivery.
	std::deque<std::shared_ptr<PartRequest>> mPartNotifications;
	std::deque<SynthNotification> mSynthNotifications;

	Q_DECLARE_TR_FUNCTIONS(swAssetLoader)
};
