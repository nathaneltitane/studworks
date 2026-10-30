#include "sw_global.h"
#include "sw_loader_asset.h"
#include "sw_colors.h"
#include "sw_file.h"
#include "sw_library.h"
#include "sw_mesh.h"
#include "sw_texture.h"
#include "sw_synthesis.h"
#include "sw_object_part.h"

constexpr QEvent::Type swAssetLoaderCompletionEvent = static_cast<QEvent::Type>(QEvent::User + 1);

// normal part flow
swAssetLoader::swAssetLoader(swPartsLibrary* Library)
	: mLibrary(Library)
{
	Q_ASSERT(QThread::currentThread() == Library->thread());
}

swAssetLoader::~swAssetLoader()
{
	Q_ASSERT(QThread::currentThread() == thread());

	Shutdown();
}

void swAssetLoader::RequestPartLoad(swPartData* PartData, swLoadPriority LoadPriority)
{
	Q_ASSERT(QThread::currentThread() == thread());

	if (mStopping)
		return;

	const auto Existing = mPartRequests.find(PartData);

	if (Existing != mPartRequests.end())
	{
		PromotePartPriority(Existing->second, LoadPriority);

		return;
	}

	std::shared_ptr<PartRequest> RequestedPart = std::make_shared<PartRequest>();

	RequestedPart->PartData = swPartDataReference(PartData);
	RequestedPart->Source = mLibrary->SnapshotPartDataSource(PartData);
	RequestedPart->LoadPriority = LoadPriority;
	RequestedPart->QueuedAt = QDateTime::currentMSecsSinceEpoch();

	mPartRequests.emplace(PartData, RequestedPart);

	DispatchJobs();

	mLibrary->NotifyAssetStatusChanged();
}

void swAssetLoader::PromotePartPriority(const std::shared_ptr<PartRequest>& RequestedPart, swLoadPriority LoadPriority)
{
	Q_ASSERT(QThread::currentThread() == thread());

	RequestedPart->LoadPriority = std::max(RequestedPart->LoadPriority, LoadPriority);

	if (!RequestedPart->StagedMesh)
		return;

	for (const swMeshLod& Lod : RequestedPart->StagedMesh->mLods)
	{
		for (int SectionIdx = 0; SectionIdx < Lod.NumSections; SectionIdx++)
		{
			const auto It = mTextureRequests.find(Lod.Sections[SectionIdx].Texture.Get());

			if (It != mTextureRequests.end())
				It->second->LoadPriority = std::max(It->second->LoadPriority, RequestedPart->LoadPriority);
		}
	}
}

void swAssetLoader::RequestSynthMesh(swPart* Part, swPartData* PartData, std::shared_ptr<const swSynthesisInformation> SynthInfo, std::vector<swPartControlPoint> ControlPoints)
{
	Q_ASSERT(QThread::currentThread() == thread());

	CancelSynthMesh(Part);

	if (!SynthInfo)
		return;

	std::shared_ptr<SynthRequest> RequestedSynth = std::make_shared<SynthRequest>();
	RequestedSynth->Part = Part;
	RequestedSynth->PartData = PartData;
	RequestedSynth->SynthInfo = std::move(SynthInfo);
	RequestedSynth->ControlPoints = std::move(ControlPoints);
	RequestedSynth->LoadPriority = swLoadPriority::Normal;
	RequestedSynth->QueuedAt = QDateTime::currentMSecsSinceEpoch();

	if (mStopping)
		return;

	mSynthRequests.emplace(Part, RequestedSynth);

	DispatchJobs();

	mLibrary->NotifyAssetStatusChanged();
}

void swAssetLoader::RequestTextureLoad(swTexture* Texture, swFileSource Source, swLoadPriority LoadPriority)
{
	Q_ASSERT(QThread::currentThread() == thread());

	if (mStopping || Texture->IsReady() || Texture->mState == swAssetState::Failed)
		return;

	const auto Existing = mTextureRequests.find(Texture);

	if (Existing != mTextureRequests.end())
	{
		if (Existing->second->LoadPriority < LoadPriority)
			Existing->second->LoadPriority = LoadPriority;
		return;
	}

	Texture->mState = swAssetState::Loading;

	std::shared_ptr<TextureRequest> RequestedTexture = std::make_shared<TextureRequest>();

	RequestedTexture->Texture = swTextureReference(Texture);
	RequestedTexture->Source = std::move(Source);
	RequestedTexture->LoadPriority = LoadPriority;
	RequestedTexture->QueuedAt = QDateTime::currentMSecsSinceEpoch();

	mTextureRequests.emplace(Texture, RequestedTexture);

	DispatchJobs();
}

void swAssetLoader::DispatchJobs()
{
	Q_ASSERT(QThread::currentThread() == thread());

	mFutures.erase(std::remove_if(mFutures.begin(), mFutures.end(), [](const QFuture<void>& Future)
	{
		return Future.isFinished();
	}), mFutures.end());

	if (mPaused || mStopping)
		return;

	const int MaxJobs = qMax(1, QThread::idealThreadCount() / 2);

	while (mActiveJobs < MaxJobs)
	{
		const qint64 Now = QDateTime::currentMSecsSinceEpoch();

		const auto Score = [Now](const auto& Item)
		{
			// waiting raises the priority
			return qMin<qint64>(static_cast<qint64>(swLoadPriority::Blocking),
				static_cast<qint64>(Item->LoadPriority) + qMax<qint64>(0, Now - Item->QueuedAt) / 10000);
		};

		const auto Less = [&Score](const auto& Left, const auto& Right)
		{
			return Score(Left) < Score(Right) || (Score(Left) == Score(Right) && Left->QueuedAt > Right->QueuedAt);
		};

		const auto BestQueued = [&Less](const auto& Requests)
		{
			typename std::decay_t<decltype(Requests)>::mapped_type Best;

			for (const auto& [Key, Request] : Requests)
				if (Request->State == RequestState::Queued && (!Best || Less(Best, Request)))
					Best = Request;

			return Best;
		};

		std::shared_ptr<PartRequest> RequestedPart = BestQueued(mPartRequests);
		std::shared_ptr<TextureRequest> RequestedTexture = BestQueued(mTextureRequests);
		std::shared_ptr<SynthRequest> RequestedSynth = BestQueued(mSynthRequests);

		if (RequestedTexture && (!RequestedPart || !Less(RequestedTexture, RequestedPart)) &&
			(!RequestedSynth || !Less(RequestedTexture, RequestedSynth)))
		{
			RequestedPart.reset();
			RequestedSynth.reset();
			RequestedTexture->State = RequestState::AwaitingCompletion;
		}
		else if (RequestedSynth && (!RequestedPart || !Less(RequestedSynth, RequestedPart)))
		{
			RequestedPart.reset();
			RequestedTexture.reset();
			RequestedSynth->State = RequestState::AwaitingCompletion;
		}
		else if (RequestedPart)
		{
			RequestedTexture.reset();
			RequestedSynth.reset();
			RequestedPart->State = RequestState::AwaitingCompletion;
		}
		else
			break;

		mActiveJobs++;
		mFutures.emplace_back(QtConcurrent::run([this, RequestedPart, RequestedTexture, RequestedSynth]()
		{
			Completion Completed;

			if (RequestedPart)
			{
				Completed.RequestedPart = RequestedPart;
				Completed.PartResult = mLibrary->BuildPartData(RequestedPart->Source);
			}
			else if (RequestedTexture)
			{
				Completed.RequestedTexture = RequestedTexture;
				Completed.TextureResult = mLibrary->BuildTextureData(RequestedTexture->Source);
			}
			else
			{
				Completed.RequestedSynth = RequestedSynth;
				Completed.SynthMesh = RequestedSynth->SynthInfo->BuildMesh(RequestedSynth->ControlPoints, RequestedSynth->Cancelled);
			}

			mCompletionMutex.lock();
			mCompletions.push_back(std::move(Completed));
			mCompletionReady.wakeAll();
			mCompletionMutex.unlock();

			QCoreApplication::postEvent(this, new QEvent(swAssetLoaderCompletionEvent));
		}));
	}
}

void swAssetLoader::ProcessCompletions()
{
	// publish during scoped waits too
	Q_ASSERT(QThread::currentThread() == thread());

	constexpr int MaxCompletionsPerPump = 16;

	for (int Processed = 0; Processed < MaxCompletionsPerPump; Processed++)
	{
		Completion Completed;
		std::shared_ptr<TextureRequest> Retry;
		const qint64 Now = QDateTime::currentMSecsSinceEpoch();

		for (const TextureRequestMap::value_type& Entry : mTextureRequests)
		{
			const std::shared_ptr<TextureRequest>& Candidate = Entry.second;

			if (Candidate->RetryAt && Candidate->RetryAt <= Now &&
				(!Retry || Candidate->LoadPriority > Retry->LoadPriority ||
					(Candidate->LoadPriority == Retry->LoadPriority && Candidate->RetryAt < Retry->RetryAt)))
				Retry = Candidate;
		}

		const auto PriorityOf = [](const Completion& Item)
		{
			if (Item.RequestedPart)
				return Item.RequestedPart->LoadPriority;

			return Item.RequestedTexture ? Item.RequestedTexture->LoadPriority : Item.RequestedSynth->LoadPriority;
		};

		{
			QMutexLocker CompletionLock(&mCompletionMutex);

			const auto Best = std::max_element(mCompletions.begin(), mCompletions.end(), [&PriorityOf](const Completion& Left, const Completion& Right)
			{
				return PriorityOf(Left) < PriorityOf(Right);
			});

			if (Retry && (Best == mCompletions.end() || Retry->LoadPriority >= PriorityOf(*Best)))
				Retry->RetryAt = 0;
			else
			{
				Retry.reset();

				if (Best == mCompletions.end())
					break;

				Completed = std::move(*Best);
				mCompletions.erase(Best);
			}
		}

		if (Retry)
		{
			UploadTextureRequest(Retry);

			continue;
		}

		// finished builds free their slot
		mActiveJobs--;

		if (Completed.RequestedTexture)
			ProcessTextureCompletion(Completed);
		else if (Completed.RequestedSynth)
			ProcessSynthCompletion(Completed);
		else
			ProcessPartCompletion(Completed);
	}

	DispatchJobs();

	const qint64 Now = QDateTime::currentMSecsSinceEpoch();
	const bool RetryDue = std::any_of(mTextureRequests.begin(), mTextureRequests.end(), [Now](const auto& Entry)
	{
		return Entry.second->RetryAt && Entry.second->RetryAt <= Now;
	});

	mCompletionMutex.lock();
	const bool NeedsPump = !mCompletions.empty() || RetryDue;
	mCompletionMutex.unlock();

	if (NeedsPump)
		QCoreApplication::postEvent(this, new QEvent(swAssetLoaderCompletionEvent));
}

void swAssetLoader::ProcessPartCompletion(Completion& Completed)
{
	Q_ASSERT(QThread::currentThread() == thread());

	const std::shared_ptr<PartRequest>& RequestedPart = Completed.RequestedPart;
	swPartData* PartData = RequestedPart->PartData.Get();
	const auto It = mPartRequests.find(PartData);
	const bool Current = It != mPartRequests.end() && It->second == RequestedPart;
	const bool Stale = mStopping || !Current;
	RequestedPart->State = Stale ? RequestState::Cancelled : RequestState::AwaitingPublication;

	const bool HasConsumer = RequestedPart->PartData.HasOtherReferences();

	if (Stale || !HasConsumer)
	{
		if (Current)
		{
			mPartRequests.erase(It);
			mLibrary->PartDataLoadCancelled(PartData);
		}

		RequestedPart->State = RequestState::Cancelled;

		RequestedPart->PartData.Reset();

		return;
	}

	RequestedPart->Error = Completed.PartResult.Error;
	RequestedPart->EmptyGeometry = Completed.PartResult.EmptyGeometry;

	if (RequestedPart->Error.isEmpty() && Completed.PartResult.Mesh)
	{
		RequestedPart->StagedMesh = std::move(Completed.PartResult.Mesh);

		RequestedPart->StagedMesh->FinalizeSections();

		RequestedPart->SaveCache = !Completed.PartResult.FromCache;
	}

	// publish once textures settle
	if (RequestedPart->StagedMesh)
	{
		const bool Ready = ResolveAndQueueMeshTextures(*RequestedPart->StagedMesh, RequestedPart->Source,
			RequestedPart->LoadPriority, RequestedPart->Error);

		if (RequestedPart->Error.isEmpty() && !Ready)
			return;
	}

	CompletePartRequest(RequestedPart);
}

void swAssetLoader::ProcessSynthCompletion(Completion& Completed)
{
	Q_ASSERT(QThread::currentThread() == thread());

	const std::shared_ptr<SynthRequest>& RequestedSynth = Completed.RequestedSynth;
	const SynthRequestMap::iterator It = mSynthRequests.find(RequestedSynth->Part);
	const bool Stale = mStopping || RequestedSynth->Cancelled.load(std::memory_order_relaxed) ||
		It == mSynthRequests.end() || It->second != RequestedSynth;
	if (Stale && It != mSynthRequests.end() && It->second == RequestedSynth)
		mSynthRequests.erase(It);
	RequestedSynth->State = Stale ? RequestState::Cancelled : RequestState::AwaitingPublication;

	if (Stale)
		return;

	if (!Completed.SynthMesh)
	{
		CompleteSynthRequest(RequestedSynth);
		return;
	}

	RequestedSynth->StagedMesh = std::move(Completed.SynthMesh);
	RequestedSynth->StagedMesh->FinalizeSections();

	const bool Ready = ResolveAndQueueMeshTextures(*RequestedSynth->StagedMesh, swPartSourceSnapshot(),
		RequestedSynth->LoadPriority, RequestedSynth->Error);

	if (!RequestedSynth->Error.isEmpty() || Ready)
		CompleteSynthRequest(RequestedSynth);
}

void swAssetLoader::ProcessTextureCompletion(Completion& Completed)
{
	Q_ASSERT(QThread::currentThread() == thread());

	const std::shared_ptr<TextureRequest>& RequestedTexture = Completed.RequestedTexture;
	swTexture* Texture = RequestedTexture->Texture.Get();
	const auto It = mTextureRequests.find(Texture);
	const bool Stale = mStopping || It == mTextureRequests.end() || It->second != RequestedTexture;
	if (Stale && It != mTextureRequests.end() && It->second == RequestedTexture)
		mTextureRequests.erase(It);
	RequestedTexture->State = Stale ? RequestState::Cancelled : RequestState::AwaitingPublication;

	if (Stale)
	{
		RequestedTexture->Texture.Reset();
		return;
	}

	mLibrary->PublishTexture(Texture, std::move(Completed.TextureResult));

	if (Texture->NeedsUpload())
		UploadTextureRequest(RequestedTexture);
	else
		CompleteTextureRequest(RequestedTexture);
}

bool swAssetLoader::ResolveAndQueueMeshTextures(swMesh& Mesh, const swPartSourceSnapshot& Source, swLoadPriority LoadPriority, QString& Error)
{
	Q_ASSERT(QThread::currentThread() == thread());

	for (swMeshLod& Lod : Mesh.mLods)
		for (int SectionIdx = 0; SectionIdx < Lod.NumSections; SectionIdx++)
		{
			swMeshSection& Section = Lod.Sections[SectionIdx];

			if (Section.TextureName.isEmpty())
				continue;

			swFileSource TextureSource;
			Section.Texture = mLibrary->AcquireTexture(Section.TextureName.toLatin1().constData(), Source.TextureSearchDirectories, Source.EmbeddedData, TextureSource);

			if (Section.Texture && !Section.Texture->IsReady() && Section.Texture->mState != swAssetState::Failed)
				RequestTextureLoad(Section.Texture.Get(), std::move(TextureSource), LoadPriority);
		}

	return MeshTexturesSettled(Mesh, Error);
}

bool swAssetLoader::MeshTexturesSettled(const swMesh& Mesh, QString& Error)
{
	bool Ready = true;

	for (const swMeshLod& Lod : Mesh.mLods)
		for (int SectionIdx = 0; SectionIdx < Lod.NumSections; SectionIdx++)
		{
			const swMeshSection& Section = Lod.Sections[SectionIdx];
			swTexture* Texture = Section.Texture.Get();

			if (!Texture)
			{
				if (!Section.TextureName.isEmpty())
					Error = tr("Missing texture '%1'.").arg(Section.TextureName);
			}
			else if (Texture->mState == swAssetState::Failed)
				Error = Texture->GetLoadError();
			else if (!Texture->IsReady())
				Ready = false;
		}

	return Ready;
}

void swAssetLoader::UploadTextureRequest(const std::shared_ptr<TextureRequest>& RequestedTexture)
{
	Q_ASSERT(QThread::currentThread() == thread());

	swTexture* Texture = RequestedTexture->Texture.Get();

	RequestedTexture->UploadAttempts++;

	QOpenGLContext* PreviousContext = QOpenGLContext::currentContext();
	QSurface* PreviousSurface = PreviousContext ? PreviousContext->surface() : nullptr;

	if (swContext* Context = swContext::GetGlobalOffscreenContext())
	{
		Context->MakeCurrent();

		if (QOpenGLContext::currentContext() == Context->GetGLContext())
			Texture->Upload(Context);
		else
		{
			Texture->SetFailed(swTextureLoadError::ContextActivationFailed, QString());
		}
	}
	else
	{
		Texture->SetFailed(swTextureLoadError::ContextUnavailable, QString());
	}

	if (PreviousContext && PreviousSurface)
		PreviousContext->makeCurrent(PreviousSurface);
	else if (QOpenGLContext::currentContext())
		QOpenGLContext::currentContext()->doneCurrent();

	constexpr int MaxUploadAttempts = 3;

	if (!Texture->IsReady() && RequestedTexture->UploadAttempts < MaxUploadAttempts)
	{
		// only the loader retries
		Texture->mState = swAssetState::Loading;

		const int Delay = RequestedTexture->UploadAttempts == 1 ? 50 : 200;

		RequestedTexture->RetryAt = QDateTime::currentMSecsSinceEpoch() + Delay;

		QTimer::singleShot(Delay, this, [this]()
		{
			QCoreApplication::postEvent(this, new QEvent(swAssetLoaderCompletionEvent));
		});

		return;
	}

	if (!Texture->IsReady())
	{
		swTextureLoadError Error = Texture->mLoadFailure;

		if (Error == swTextureLoadError::None)
			Error = swTextureLoadError::UploadFailed;

		Texture->SetFailed(Error, Texture->mLoadFailureDetails);
		Texture->DiscardDecodedImage();
	}

	CompleteTextureRequest(RequestedTexture);
}

void swAssetLoader::CompleteTextureRequest(const std::shared_ptr<TextureRequest>& RequestedTexture)
{
	Q_ASSERT(QThread::currentThread() == thread());

	const auto It = mTextureRequests.find(RequestedTexture->Texture.Get());
	if (It != mTextureRequests.end() && It->second == RequestedTexture)
		mTextureRequests.erase(It);
	RequestedTexture->State = RequestedTexture->Texture->IsReady() ? RequestState::Succeeded : RequestState::Failed;

	RequestedTexture->Texture.Reset();

	FinishPartsWithSettledTextures();
	FinishSynthsWithSettledTextures();
}

void swAssetLoader::FinishPartsWithSettledTextures()
{
	Q_ASSERT(QThread::currentThread() == thread());

	std::vector<std::shared_ptr<PartRequest>> Waiting;

	for (const PartRequestMap::value_type& Entry : mPartRequests)
		if (Entry.second->StagedMesh)
			Waiting.push_back(Entry.second);

	for (const std::shared_ptr<PartRequest>& RequestedPart : Waiting)
	{
		const bool Ready = MeshTexturesSettled(*RequestedPart->StagedMesh, RequestedPart->Error);

		if (!RequestedPart->Error.isEmpty() || Ready)
			CompletePartRequest(RequestedPart);
	}
}

void swAssetLoader::FinishSynthsWithSettledTextures()
{
	Q_ASSERT(QThread::currentThread() == thread());

	std::vector<std::shared_ptr<SynthRequest>> Waiting;

	for (const SynthRequestMap::value_type& Entry : mSynthRequests)
		if (Entry.second->StagedMesh)
			Waiting.push_back(Entry.second);

	for (const std::shared_ptr<SynthRequest>& RequestedSynth : Waiting)
	{
		const SynthRequestMap::iterator It = mSynthRequests.find(RequestedSynth->Part);
		const bool Current = It != mSynthRequests.end() && It->second == RequestedSynth;

		if (!Current)
			continue;

		const bool Ready = MeshTexturesSettled(*RequestedSynth->StagedMesh, RequestedSynth->Error);

		if (!RequestedSynth->Error.isEmpty() || Ready)
			CompleteSynthRequest(RequestedSynth);
	}
}

void swAssetLoader::CompletePartRequest(const std::shared_ptr<PartRequest>& RequestedPart)
{
	Q_ASSERT(QThread::currentThread() == thread());

	swPartData* PartData = RequestedPart->PartData.Get();
	QString& LoadError = RequestedPart->Error;

	if (LoadError.isEmpty() && !RequestedPart->StagedMesh && !RequestedPart->EmptyGeometry)
		LoadError = tr("Could not load part '%1'.").arg(RequestedPart->Source.FileName);

	if (LoadError.isEmpty() && RequestedPart->SaveCache && RequestedPart->StagedMesh)
		mLibrary->SaveBuiltPartDataCache(RequestedPart->Source, *RequestedPart->StagedMesh);

	const bool Loaded = mLibrary->PublishPartDataMesh(PartData, std::move(RequestedPart->StagedMesh), LoadError);

	if (!Loaded)
		CancelUnusedTextureRequests();

	const auto It = mPartRequests.find(PartData);
	if (It != mPartRequests.end() && It->second == RequestedPart)
		mPartRequests.erase(It);
	RequestedPart->State = Loaded ? RequestState::Succeeded : RequestState::Failed;

	mPartNotifications.push_back(RequestedPart);

	QCoreApplication::postEvent(this, new QEvent(swAssetLoaderCompletionEvent));
}

void swAssetLoader::CompleteSynthRequest(const std::shared_ptr<SynthRequest>& RequestedSynth)
{
	Q_ASSERT(QThread::currentThread() == thread());

	swPart* Part = RequestedSynth->Part;
	swPartData* PartData = RequestedSynth->PartData;
	const bool Loaded = mLibrary->PublishSynthMesh(Part, std::move(RequestedSynth->StagedMesh), RequestedSynth->Error);

	if (!Loaded)
		CancelUnusedTextureRequests();

	const SynthRequestMap::iterator It = mSynthRequests.find(Part);

	if (It != mSynthRequests.end() && It->second == RequestedSynth)
		mSynthRequests.erase(It);

	RequestedSynth->State = Loaded ? RequestState::Succeeded : RequestState::Failed;

	mSynthNotifications.push_back({ RequestedSynth, swPartDataReference(PartData) });

	QCoreApplication::postEvent(this, new QEvent(swAssetLoaderCompletionEvent));
}

void swAssetLoader::DispatchNotifications()
{
	// defer delivery outside waits
	Q_ASSERT(QThread::currentThread() == thread());

	constexpr int MaxNotificationsPerPump = 32;

	for (int Delivered = 0; Delivered < MaxNotificationsPerPump && !mPartNotifications.empty(); Delivered++)
	{
		const std::shared_ptr<PartRequest> RequestedPart = std::move(mPartNotifications.front());

		mPartNotifications.pop_front();

		swPartData* PartData = RequestedPart->PartData.Get();

		if (RequestedPart->State == RequestState::Cancelled || !RequestedPart->PartData.HasOtherReferences())
		{
			RequestedPart->PartData.Reset();

			continue;
		}

		QPointer<swAssetLoader> LoaderGuard(this);
		QPointer<swPartsLibrary> LibraryGuard(mLibrary);

		if (RequestedPart->State == RequestState::Succeeded)
			emit mLibrary->PartLoaded(PartData);
		else
			emit mLibrary->PartLoadFailed(PartData, RequestedPart->Error);

		if (!LoaderGuard || !LibraryGuard)
		{
			// teardown can delete the definition
			RequestedPart->PartData.Release();
			return;
		}

		RequestedPart->PartData.Reset();
	}

	std::unordered_set<swPartData*> SettledSynths;

	for (int Delivered = 0; Delivered < MaxNotificationsPerPump && !mSynthNotifications.empty(); Delivered++)
	{
		SynthNotification Notification = std::move(mSynthNotifications.front());

		mSynthNotifications.pop_front();

		swPartData* PartData = Notification.PartData.Get();

		if (!Notification.RequestedSynth->Cancelled.load(std::memory_order_relaxed) && Notification.PartData.HasOtherReferences() && SettledSynths.insert(PartData).second)
		{
			QPointer<swAssetLoader> LoaderGuard(this);
			QPointer<swPartsLibrary> LibraryGuard(mLibrary);

			emit mLibrary->GeneratedMeshSettled(PartData);

			if (!LoaderGuard || !LibraryGuard)
			{
				// teardown can delete the definition
				Notification.PartData.Release();
				return;
			}
		}
	}

	if (!mPartNotifications.empty() || !mSynthNotifications.empty())
		QCoreApplication::postEvent(this, new QEvent(swAssetLoaderCompletionEvent));

	if (!SettledSynths.empty())
		mLibrary->NotifyAssetStatusChanged();
}

bool swAssetLoader::event(QEvent* Event)
{
	Q_ASSERT(QThread::currentThread() == thread());

	if (Event->type() == swAssetLoaderCompletionEvent)
	{
		ProcessCompletions();
		DispatchNotifications();

		return true;
	}

	return QObject::event(Event);
}

void swAssetLoader::WaitForCompletionOrRetry()
{
	Q_ASSERT(QThread::currentThread() == thread());

	qint64 EarliestRetry = -1;

	for (const TextureRequestMap::value_type& Entry : mTextureRequests)
	{
		const qint64 RetryAt = Entry.second->RetryAt;

		if (RetryAt && (EarliestRetry == -1 || RetryAt < EarliestRetry))
			EarliestRetry = RetryAt;
	}

	// wait under the handoff mutex
	QMutexLocker CompletionLock(&mCompletionMutex);

	if (!mCompletions.empty())
		return;

	if (EarliestRetry == -1)
		mCompletionReady.wait(&mCompletionMutex);
	else
	{
		const qint64 Delay = EarliestRetry - QDateTime::currentMSecsSinceEpoch();

		if (Delay > 0)
			mCompletionReady.wait(&mCompletionMutex, static_cast<unsigned long>(Delay));
	}
}

bool swAssetLoader::WaitForParts(const std::vector<swPartData*>& Parts, const std::function<void(int, int)>& Progress)
{
	Q_ASSERT(QThread::currentThread() == thread());

	std::vector<swPartData*> Required = Parts;
	std::sort(Required.begin(), Required.end());

	Required.erase(std::unique(Required.begin(), Required.end()), Required.end());

	std::vector<std::shared_ptr<PartRequest>> Requests;

	for (swPartData* PartData : Required)
	{
		const auto It = mPartRequests.find(PartData);

		if (It == mPartRequests.end())
			continue;

		PromotePartPriority(It->second, swLoadPriority::Blocking);
		Requests.push_back(It->second);
	}

	DispatchJobs();

	QOpenGLContext* const PreviousContext = QOpenGLContext::currentContext();
	QSurface* const PreviousSurface = PreviousContext ? PreviousContext->surface() : nullptr;

	bool Ready = true;

	for (const std::shared_ptr<PartRequest>& RequestedPart : Requests)
	{
		for (;;)
		{
			ProcessCompletions();

			if (Progress)
			{
				const int Finished = static_cast<int>(std::count_if(Requests.begin(), Requests.end(), [](const std::shared_ptr<PartRequest>& Request)
				{
					return Request->IsFinished();
				}));

				Progress(Finished, static_cast<int>(Requests.size()));
			}

			if (mStopping || RequestedPart->IsFinished())
				break;

			WaitForCompletionOrRetry();
		}

		Ready = RequestedPart->State == RequestState::Succeeded && Ready;
	}

	if (PreviousContext && PreviousSurface && (QOpenGLContext::currentContext() != PreviousContext || PreviousContext->surface() != PreviousSurface))
		PreviousContext->makeCurrent(PreviousSurface);

	return Ready;
}

bool swAssetLoader::WaitForSynthMeshes(const std::vector<swPart*>& Parts)
{
	Q_ASSERT(QThread::currentThread() == thread());

	std::vector<std::shared_ptr<SynthRequest>> Required;

	for (swPart* Part : Parts)
	{
		const SynthRequestMap::iterator It = mSynthRequests.find(Part);

		if (It == mSynthRequests.end())
			continue;

		const std::shared_ptr<SynthRequest>& RequestedSynth = It->second;
		RequestedSynth->LoadPriority = swLoadPriority::Blocking;
		Required.push_back(RequestedSynth);

		if (RequestedSynth->StagedMesh)
			for (const swMeshLod& Lod : RequestedSynth->StagedMesh->mLods)
				for (int SectionIdx = 0; SectionIdx < Lod.NumSections; SectionIdx++)
				{
					const TextureRequestMap::iterator TextureIt = mTextureRequests.find(Lod.Sections[SectionIdx].Texture.Get());

					if (TextureIt != mTextureRequests.end())
						TextureIt->second->LoadPriority = swLoadPriority::Blocking;
				}
	}

	DispatchJobs();

	QOpenGLContext* const PreviousContext = QOpenGLContext::currentContext();
	QSurface* const PreviousSurface = PreviousContext ? PreviousContext->surface() : nullptr;

	bool Ready = true;

	for (const std::shared_ptr<SynthRequest>& RequestedSynth : Required)
	{
		for (;;)
		{
			ProcessCompletions();

			if (mStopping || RequestedSynth->IsFinished())
				break;

			WaitForCompletionOrRetry();
		}

		Ready = RequestedSynth->State == RequestState::Succeeded && Ready;
	}

	if (PreviousContext && PreviousSurface && (QOpenGLContext::currentContext() != PreviousContext || PreviousContext->surface() != PreviousSurface))
		PreviousContext->makeCurrent(PreviousSurface);

	return Ready;
}

bool swAssetLoader::WaitForTexture(swTexture* Texture, swFileSource Source)
{
	Q_ASSERT(QThread::currentThread() == thread());

	if (Texture->IsReady())
		return true;

	RequestTextureLoad(Texture, std::move(Source), swLoadPriority::Blocking);

	for (;;)
	{
		ProcessCompletions();

		if (Texture->IsReady() || Texture->mState == swAssetState::Failed)
			break;

		if (mStopping || mTextureRequests.find(Texture) == mTextureRequests.end())
			break;

		WaitForCompletionOrRetry();
	}

	return Texture->IsReady();
}

void swAssetLoader::WaitForAllWork()
{
	Q_ASSERT(QThread::currentThread() == thread());

	for (;;)
	{
		ProcessCompletions();

		if (mPartRequests.empty() && mTextureRequests.empty() && mSynthRequests.empty() && mActiveJobs == 0)
			break;

		WaitForCompletionOrRetry();
	}

	for (QFuture<void>& Future : mFutures)
		Future.waitForFinished();

	mFutures.clear();
}

bool swAssetLoader::HasPendingWork()
{
	Q_ASSERT(QThread::currentThread() == thread());

	return !mPartRequests.empty() || !mTextureRequests.empty() || !mSynthRequests.empty() || mActiveJobs != 0;
}

void swAssetLoader::InvalidatePartLoad(swPartData* PartData)
{
	Q_ASSERT(QThread::currentThread() == thread());

	for (const std::shared_ptr<PartRequest>& Pending : mPartNotifications)
		if (Pending->PartData == PartData)
			Pending->State = RequestState::Cancelled;

	const auto It = mPartRequests.find(PartData);

	if (It != mPartRequests.end())
		DiscardPartRequest(It->second);

	mLibrary->NotifyAssetStatusChanged();
}

void swAssetLoader::CancelPartLoad(swPartData* PartData)
{
	// discard builds without consumers
	Q_ASSERT(QThread::currentThread() == thread());

	const auto It = mPartRequests.find(PartData);

	if (It == mPartRequests.end() || It->second->State == RequestState::AwaitingCompletion)
		return;

	mLibrary->PartDataLoadCancelled(PartData);

	DiscardPartRequest(It->second);
}

void swAssetLoader::CancelSynthMesh(swPart* Part)
{
	// remove queued work at once
	Q_ASSERT(QThread::currentThread() == thread());

	for (SynthNotification& Pending : mSynthNotifications)
		if (Pending.RequestedSynth->Part == Part)
			Pending.RequestedSynth->Cancelled.store(true, std::memory_order_relaxed);

	const SynthRequestMap::iterator It = mSynthRequests.find(Part);

	if (It == mSynthRequests.end())
		return;

	const std::shared_ptr<SynthRequest> Removed = It->second;
	Removed->Cancelled.store(true, std::memory_order_relaxed);
	const bool HasOutstandingWork = Removed->State == RequestState::AwaitingCompletion;
	Removed->State = RequestState::Cancelled;
	mSynthRequests.erase(It);

	if (!HasOutstandingWork)
	{
		Removed->StagedMesh.reset();
		CancelUnusedTextureRequests();
	}
}

void swAssetLoader::DiscardPartRequest(std::shared_ptr<PartRequest> Request)
{
	Q_ASSERT(QThread::currentThread() == thread());

	const bool HasOutstandingWork = Request->State == RequestState::AwaitingCompletion;

	Request->State = RequestState::Cancelled;
	mPartRequests.erase(Request->PartData.Get());

	// running builds keep their hold
	if (!HasOutstandingWork)
	{
		Request->StagedMesh.reset();
		Request->PartData.Reset();
	}

	CancelUnusedTextureRequests();
}

void swAssetLoader::CancelUnusedTextureRequests()
{
	Q_ASSERT(QThread::currentThread() == thread());

	std::vector<std::shared_ptr<TextureRequest>> Cancelled;

	for (auto It = mTextureRequests.begin(); It != mTextureRequests.end();)
	{
		const std::shared_ptr<TextureRequest>& RequestedTexture = It->second;

		// the request owns one reference
		if (RequestedTexture->State == RequestState::AwaitingCompletion || RequestedTexture->Texture.HasOtherReferences())
		{
			++It;
			continue;
		}


		RequestedTexture->State = RequestState::Cancelled;
		Cancelled.push_back(RequestedTexture);

		It = mTextureRequests.erase(It);
	}

	for (const std::shared_ptr<TextureRequest>& RequestedTexture : Cancelled)
	{
		swTexture* Texture = RequestedTexture->Texture.Get();

		Texture->DiscardDecodedImage();

		RequestedTexture->Texture.Reset();
	}
}

void swAssetLoader::PauseQueuedWork()
{
	Q_ASSERT(QThread::currentThread() == thread());

	// keep jobs queued until dispatch resumes
	mPaused = true;

	for (;;)
	{
		ProcessCompletions();

		if (mActiveJobs == 0)
			break;

		WaitForCompletionOrRetry();
	}

	// rebuild meshes with old stud settings
	for (const PartRequestMap::value_type& Entry : mPartRequests)
	{
		const std::shared_ptr<PartRequest>& RequestedPart = Entry.second;

		if (!RequestedPart->StagedMesh)
			continue;

		RequestedPart->StagedMesh.reset();
		RequestedPart->State = RequestState::Queued;
	}

	// restart generated meshes after the stud change
	for (const SynthRequestMap::value_type& Entry : mSynthRequests)
	{
		const std::shared_ptr<SynthRequest>& RequestedSynth = Entry.second;

		if (!RequestedSynth->StagedMesh)
			continue;

		RequestedSynth->StagedMesh.reset();
		RequestedSynth->State = RequestState::Queued;
	}
}

void swAssetLoader::ResumeQueuedWork()
{
	Q_ASSERT(QThread::currentThread() == thread());

	for (const PartRequestMap::value_type& Entry : mPartRequests)
		if (Entry.second->State == RequestState::Queued)
			Entry.second->Source = mLibrary->SnapshotPartDataSource(Entry.second->PartData.Get());

	// a reload may move texture sources
	for (const TextureRequestMap::value_type& Entry : mTextureRequests)
		if (Entry.second->State == RequestState::Queued)
			Entry.second->Source = Entry.second->Texture->mSource;

	mPaused = false;
	DispatchJobs();
}

void swAssetLoader::Shutdown()
{
	// join obsolete workers on shutdown
	Q_ASSERT(QThread::currentThread() == thread());

	const bool WasCancelled = mLibrary->mCancelLoading.exchange(true);
	mLibrary->mPrimitiveLoaded.wakeAll();

	std::deque<std::shared_ptr<PartRequest>> Cancelled;
	std::deque<std::shared_ptr<TextureRequest>> CancelledTextures;

	mStopping = true;

	for (auto It = mPartRequests.begin(); It != mPartRequests.end();)
	{
		if (It->second->State == RequestState::AwaitingCompletion)
			++It;
		else
		{
			It->second->State = RequestState::Cancelled;
			mLibrary->PartDataLoadCancelled(It->second->PartData.Get());

			Cancelled.push_back(It->second);

			It = mPartRequests.erase(It);
		}
	}

	for (auto It = mTextureRequests.begin(); It != mTextureRequests.end();)
	{
		if (It->second->State == RequestState::AwaitingCompletion)
			++It;
		else
		{
			It->second->State = RequestState::Cancelled;
			CancelledTextures.push_back(It->second);

			It = mTextureRequests.erase(It);
		}
	}

	for (SynthRequestMap::iterator It = mSynthRequests.begin(); It != mSynthRequests.end();)
	{
		if (It->second->State == RequestState::AwaitingCompletion)
		{
			It->second->Cancelled.store(true, std::memory_order_relaxed);
			++It;
		}
		else
		{
			It->second->State = RequestState::Cancelled;
			It = mSynthRequests.erase(It);
		}
	}

	for (const std::shared_ptr<PartRequest>& RequestedPart : Cancelled)
		RequestedPart->PartData.Reset();

	for (const std::shared_ptr<TextureRequest>& RequestedTexture : CancelledTextures)
		RequestedTexture->Texture.Reset();

	WaitForAllWork();

	while (!mPartNotifications.empty())
	{
		const std::shared_ptr<PartRequest> RequestedPart = std::move(mPartNotifications.front());

		mPartNotifications.pop_front();

		RequestedPart->PartData.Reset();
	}

	while (!mSynthNotifications.empty())
		mSynthNotifications.pop_front();

	mLibrary->mCancelLoading.store(WasCancelled);
}
