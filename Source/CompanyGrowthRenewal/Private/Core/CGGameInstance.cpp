// Fill out your copyright notice in the Description page of Project Settings.


#include "CGGameInstance.h"
#include "AssetCompilingManager.h"
#include "CoreGlobals.h"
#include "Misc/CoreDelegates.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "Kismet/GameplayStatics.h"
#include "Manager/UIManagerSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/SaveLoadManager.h"
#include "Data/CompanyDisplayNameRules.h"
#include "Manager/PlayFabManagerSubsystem.h"
#include "Manager/FirebaseAuthSubsystem.h"
#include "Manager/LiveOpsSubsystem.h"
#include "Manager/SoundManagerSubsystem.h"
#include "Manager/EntityManager.h"
#include "Manager/ResourceItemManager.h"
#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "Entity/Building/BuildingBaseActor.h"
#include "Entity/Factory/BrickFactory.h"
#include "Utils/CF1CaptureSavePolicy.h"
#include "Global/CGDevSettings.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Engine/StreamableRenderAsset.h"
#include "HAL/IConsoleManager.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "GameMode/OfficeGameMode.h"
#include "Office/OfficeDistantBackdrop.h"
#include "Office/OfficeRecruitmentFlowSubsystem.h"
#include "GameMode/CGGameModeBase.h"
#include "Manager/SettingsManagerSubsystem.h"
#include "MoviePlayer.h"
#include "PipelineStateCache.h"
#include "UI/Panel/InGameLayerWidget.h"
#include "UI/Panel/OfficeLayerWidget.h"
#include "UI/Panel/LegacyWorldMapLayerWidget.h"
#include "Engine/Font.h"
#include "Table/LoadingTipTable.h"
#include "UI/Slate/SLevelTransitionOverlay.h"
#include "Widgets/SViewport.h"
#include "WorldPartition/WorldPartitionSubsystem.h"
#include "UObject/Package.h"
#include "UObject/UObjectIterator.h"
#include "EngineUtils.h"

UCGGameInstance* UCGGameInstance::Instance = nullptr;

namespace CGLevelTransitionCVars
{
	TAutoConsoleVariable<int32> CityPhoto(
		TEXT("cg.Office.CityPhoto"), 1,
		TEXT("1=오피스 입장 배경에 도시 사진을 쓴다. 0=끈다(GLES 캡처 크래시 폴백)."), ECVF_Default);

	// 기본 0 — 켜려면 실기기 검증 후에. 미리 읽은 월드를 붙잡아도 LoadMap 의 퍼지와 엮이는 경로라 travel 실패 이력이 있다.
	TAutoConsoleVariable<int32> PreloadMap(
		TEXT("cg.Transition.PreloadMap"), 0,
		TEXT("1=건물 패널이 열린 동안 목적지 맵을 비동기로 읽어둔다."), ECVF_Default);

	TAutoConsoleVariable<float> SettleMaxSeconds(
		TEXT("cg.Transition.SettleMaxSeconds"), 0.4f,
		TEXT("목적지 준비 후 정착 프레임을 기다리는 시간 상한(초). 저프레임 기기에서 프레임 수 기준이 수 초로 늘어나는 것을 막는다."), ECVF_Default);

	TAutoConsoleVariable<float> FadeSeconds(
		TEXT("cg.Transition.FadeSeconds"), 0.2f,
		TEXT("전환 암전/복귀 페이드 1회 길이(초)."), ECVF_Default);

	TAutoConsoleVariable<float> ResourceWaitSeconds(
		TEXT("cg.Transition.ResourceWaitSeconds"), 1.0f,
		TEXT("패키지·월드 스트리밍이 끝난 뒤 렌더 에셋 스트리밍을 더 기다리는 상한(초). 0=안 기다림."), ECVF_Default);
}

namespace
{
	bool IsDestinationWorld(const UWorld* CandidateWorld, const FString& LevelName)
	{
		const FString DestinationName = FPackageName::GetShortName(FPackageName::ObjectPathToPackageName(LevelName));
		return CandidateWorld && UGameplayStatics::GetCurrentLevelName(CandidateWorld, true) == DestinationName;
	}

	class FLevelTransitionInputBlocker final : public IInputProcessor
	{
	public:
		explicit FLevelTransitionInputBlocker(TSharedPtr<SViewport> InViewport) : ViewportWidget(InViewport) {}
		virtual void Tick(float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override {}
		virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& Event) override { return HasGameFocus(); }
		virtual bool HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& Event) override { return HasGameFocus(); }
		virtual bool HandleAnalogInputEvent(FSlateApplication& SlateApp, const FAnalogInputEvent& Event) override { return HasGameFocus(); }
		virtual bool HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& Event) override { return IsGamePointer(Event); }
		virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& Event) override { return IsGamePointer(Event); }
		virtual bool HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& Event) override { return IsGamePointer(Event); }
		virtual bool HandleMouseButtonDoubleClickEvent(FSlateApplication& SlateApp, const FPointerEvent& Event) override { return IsGamePointer(Event); }
		virtual bool HandleMouseWheelOrGestureEvent(FSlateApplication& SlateApp, const FPointerEvent& WheelEvent, const FPointerEvent* GestureEvent) override { return IsGamePointer(WheelEvent); }
		virtual bool HandleMotionDetectedEvent(FSlateApplication& SlateApp, const FMotionEvent& Event) override { return HasGameFocus(); }

	private:
		bool HasGameFocus() const
		{
			const TSharedPtr<SViewport> Viewport = ViewportWidget.Pin();
			return Viewport.IsValid() && (Viewport->HasKeyboardFocus() || Viewport->HasFocusedDescendants());
		}
		bool IsGamePointer(const FPointerEvent& Event) const
		{
			const TSharedPtr<SViewport> Viewport = ViewportWidget.Pin();
			return Viewport.IsValid() && Viewport->GetCachedGeometry().IsUnderLocation(Event.GetScreenSpacePosition());
		}
		TWeakPtr<SViewport> ViewportWidget;
	};
}

UCGGameInstance::UCGGameInstance()
{
	GlobalAssetCache = NewObject<UGlobalAssetCache>(this, TEXT("GlobalAssetCache"));
}

void UCGGameInstance::Init()
{
	Super::Init();

#if !UE_BUILD_SHIPPING
	// 좌측 상단 화면 디버그 메시지 기본 OFF. 필요 시 콘솔에서 EnableAllScreenMessages
	GAreScreenMessagesEnabled = false;
#endif

	// GameInstance 자신을 GlobalAssetCache에 알려줍니다.
	GlobalAssetCache->SetGameInstance(this);
	Instance = this;

	// 기본 맵 타입 설정 (시작 맵이 MainMap인 경우)
	CurrentMapType = ECurrentMapType::MainMap;

	if (UTableManagerSubsystem* TableMgr = GetSubsystem<UTableManagerSubsystem>())
	{
		bool bFoundResource = false;
		const FResourceInfo BuildingResource = TableMgr->GetResourceInfo(EResourceType::Building, bFoundResource);
		if (bFoundResource)
		{
			LevelTransitionPattern = BuildingResource.Icon.LoadSynchronous();
		}
	}
	if (!LevelTransitionPattern)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LevelTransition] DT_Resource.Building icon missing"));
	}

	// CommonStyle/Text 의 스케일 스타일이 같은 UFont 를 하드참조하므로 쿠킹에 이미 들어간다
	TransitionTipBodyFont = TSoftObjectPtr<UFont>(FSoftObjectPath(
		TEXT("/Game/CompanyGrowth/Font/NEXONLv1GothicRegular_Font.NEXONLv1GothicRegular_Font"))).LoadSynchronous();
	TransitionTipLabelFont = TSoftObjectPtr<UFont>(FSoftObjectPath(
		TEXT("/Game/CompanyGrowth/Font/NEXONLv1GothicBold_Font.NEXONLv1GothicBold_Font"))).LoadSynchronous();
	// UIIcon 폴더는 DirectoriesToAlwaysCook 에 이미 있다 ― 추가 등록 불필요
	TransitionTipIcon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(
		TEXT("/Game/CompanyGrowth/UI/Textures/UIIcon/TipBulb.TipBulb"))).LoadSynchronous();
	if (!TransitionTipBodyFont)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LevelTransition] NEXON 폰트 미로드 ― 전환 팁 밴드가 안 그려짐"));
	}

	if (IsMoviePlayerEnabled())
	{
		PrepareLoadingScreenHandle = GetMoviePlayer()->OnPrepareLoadingScreen().AddUObject(
			this, &UCGGameInstance::PrepareLevelLoadingScreen);
	}

	// 자동 게스트 로그인 (나중에 로그인 레벨로 대체 예정)
	const bool bIsCf1IsolatedCapture = CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
		FCommandLine::Get(), FPaths::ProjectSavedDir());
	if (UCGDevSettings::IsOfflineMode())
	{
		UE_LOG(LogTemp, Log, TEXT("[GameInstance] CG Dev local mode: PlayFab/Firebase login disabled"));
	}
	else if (bIsCf1IsolatedCapture)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GameInstance] CF1 isolated capture: automatic PlayFab guest login skipped"));
	}
	else if (UPlayFabManagerSubsystem* PlayFabMgr = GetSubsystem<UPlayFabManagerSubsystem>())
	{
		PlayFabLoginHandle = PlayFabMgr->OnLoginComplete.AddUObject(this, &UCGGameInstance::HandlePlayFabLoginComplete);
		PlayFabMgr->LoginAsGuest();
	}

	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
		this, &UCGGameInstance::OnPostLoadMapWithWorld);

	// 앱 백그라운드/포그라운드 훅 — 데이터 손실 방지 + warm resume catchup
	EnterBackgroundHandle = FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddUObject(
		this, &UCGGameInstance::HandleAppEnterBackground);
	EnterForegroundHandle = FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddUObject(
		this, &UCGGameInstance::HandleAppEnterForeground);
}

void UCGGameInstance::OnStart()
{
	Super::OnStart();

	// PostLoadMapWithWorld 가 PIE 에서 broadcast 누락되는 케이스 보정.
	// 첫 부팅용 진입점 — 후속 레벨 전환은 PostLoadMapWithWorld 가 cover
	if (UWorld* World = GetWorld())
	{
		OnPostLoadMapWithWorld(World);
	}
}

void UCGGameInstance::Shutdown()
{
	if (!AwayFactoryProduction.IsEmpty() && BackgroundEnterRealtime < 0.0)
	{
		if (USaveLoadManager* SaveMgr = GetSubsystem<USaveLoadManager>())
		{
			SaveMgr->SaveGameData();
		}
	}
	StopAwayFactoryProduction();
	FTSTicker::GetCoreTicker().RemoveTicker(TickDelegateHandle);
	TickDelegateHandle.Reset();
	FinishLevelTransition();
#if !UE_BUILD_SHIPPING
	FinishPreviewTransition();
#endif
	if (IsMoviePlayerEnabled())
	{
		GetMoviePlayer()->OnPrepareLoadingScreen().Remove(PrepareLoadingScreenHandle);
	}
	PrepareLoadingScreenHandle.Reset();
	if (UPlayFabManagerSubsystem* PlayFabMgr = GetSubsystem<UPlayFabManagerSubsystem>())
	{
		PlayFabMgr->OnLoginComplete.Remove(PlayFabLoginHandle);
	}
	PlayFabLoginHandle.Reset();

	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
	FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Remove(EnterBackgroundHandle);
	FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Remove(EnterForegroundHandle);
	Super::Shutdown();
	if (Instance == this) Instance = nullptr;
}

void UCGGameInstance::HandlePlayFabLoginComplete(bool bSuccess)
{
	if (!bSuccess || UCGDevSettings::IsOfflineMode())
	{
		return;
	}

	if (UFirebaseAuthSubsystem* AuthMgr = GetSubsystem<UFirebaseAuthSubsystem>())
	{
		AuthMgr->Exchange();
	}
}

void UCGGameInstance::HandleAppEnterBackground()
{
	// 로컬 동기 작업만 — 서버 시간 조회 등 비동기는 서스펜드로 미전송이라 여기서 안 함
	BackgroundEnterRealtime = FPlatformTime::Seconds();
	AccrueAwayFactoryProduction();
	PauseAwayFactoryProduction();

	// 3초 debounce 우회 즉시 저장 — 홈버튼/강제종료 시 마지막 상태 보장 (최대 debounce 만큼 유실되던 버그)
	if (USaveLoadManager* SaveMgr = GetSubsystem<USaveLoadManager>())
	{
		SaveMgr->SaveGameData();
	}
}

void UCGGameInstance::HandleAppEnterForeground()
{
	// 백그라운드 체류가 짧으면(60초 미만) catchup 불필요 — 이중 정산/서버 조회 낭비 방지
	if (BackgroundEnterRealtime < 0.0)
	{
		return;
	}
	const double AwaySeconds = FPlatformTime::Seconds() - BackgroundEnterRealtime;
	BackgroundEnterRealtime = -1.0;
	if (!bAwayFactoryTravelPaused)
	{
		ResumeAwayFactoryProduction();
	}
	if (AwaySeconds < 60.0)
	{
		return;
	}
	if (UCGDevSettings::IsOfflineMode())
	{
		if (USaveLoadManager* SaveMgr = GetSubsystem<USaveLoadManager>())
		{
			SaveMgr->SettleLocalOfflineGains();
		}
		return;
	}

	// warm resume — 프로세스가 살아 있던 채 오래 백그라운드였으면 오프라인 정산이 안 돌던 버그.
	// StartOfflineGainsSequence 내부에 중복 시퀀스 가드(PendingTimeRequest/bAwaitingLastSyncResponse) 있어 재호출 안전.
	if (UPlayFabManagerSubsystem* PlayFabMgr = GetSubsystem<UPlayFabManagerSubsystem>())
	{
		PlayFabMgr->StartOfflineGainsSequence();
	}

	// 백그라운드 사이에 바뀐 운영 콘텐츠를 다시 받는다 (10분 스로틀은 그대로)
	if (ULiveOpsSubsystem* LiveOpsMgr = GetSubsystem<ULiveOpsSubsystem>())
	{
		LiveOpsMgr->RefreshFromServer(/*bForce=*/false);
	}
}

bool UCGGameInstance::TickAwayFactoryProduction(float /*DeltaTime*/)
{
	AccrueAwayFactoryProduction();
	return true;
}

void UCGGameInstance::BeginAwayFactoryProductionFromCity()
{
	StopAwayFactoryProduction();
	UWorld* SourceWorld = GetWorld();
	if (!SourceWorld) return;
	const double Now = FPlatformTime::Seconds();
	for (TActorIterator<ABrickFactory> It(SourceWorld); It; ++It)
	{
		const ABrickFactory* Factory = *It;
		if (!IsValid(Factory)) continue;
		const FFactorySaveData FactoryRecord = Factory->GetFactoryData();
		if (FactoryRecord.FactoryID.IsNone()) continue;
		FFactoryAwayProductionClock Clock;
		if (Clock.Start(Factory->GetOfflineBricksPerSecond(), FactoryRecord.AwayProductionFraction, Now))
		{
			Clock.Pause();
			AwayFactoryProduction.Add(FactoryRecord.FactoryID, Clock);
		}
	}
	bAwayFactoryTravelPaused = !AwayFactoryProduction.IsEmpty();
}

void UCGGameInstance::PauseAwayFactoryProduction()
{
	if (AwayFactoryTickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(AwayFactoryTickHandle);
		AwayFactoryTickHandle.Reset();
	}
	for (auto& Pair : AwayFactoryProduction)
	{
		Pair.Value.Pause();
	}
}

void UCGGameInstance::ResumeAwayFactoryProduction()
{
	if (AwayFactoryProduction.IsEmpty() || BackgroundEnterRealtime >= 0.0 || bIsVisitMode) return;
	const double Now = FPlatformTime::Seconds();
	for (auto& Pair : AwayFactoryProduction)
	{
		Pair.Value.Resume(Now);
	}
	if (!AwayFactoryTickHandle.IsValid())
	{
		AwayFactoryTickHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateUObject(this, &UCGGameInstance::TickAwayFactoryProduction), 1.0f);
	}
}

void UCGGameInstance::StopAwayFactoryProduction()
{
	PauseAwayFactoryProduction();
	AwayFactoryProduction.Reset();
	bAwayFactoryTravelPaused = false;
}

bool UCGGameInstance::AccrueAwayFactoryProduction()
{
	if (AwayFactoryProduction.IsEmpty()) return true;
	if (bAwayFactorySettlementInProgress) return false;
	UResourceItemManager* Resources = GetSubsystem<UResourceItemManager>();
	if (!Resources) return false;
	const double Now = FPlatformTime::Seconds();
	if (!FMath::IsFinite(Now) || Now < 0.0) return false;
	TGuardValue<bool> SettlementGuard(bAwayFactorySettlementInProgress, true);
	for (auto& Pair : AwayFactoryProduction)
	{
		if (!Pair.Value.bRunning) continue;
		const int64 CurrentBricks = Resources->GetResourceAmount(EResourceType::Brick);
		if (CurrentBricks < 0) return false;
		int64 Gained = 0;
		if (!Pair.Value.Accrue(Now, MAX_int64 - CurrentBricks, Gained)) return false;
		while (Gained > 0)
		{
			const int32 Chunk = static_cast<int32>(FMath::Min<int64>(Gained, MAX_int32));
			Resources->CollectFactoryBricks(Chunk);
			Gained -= Chunk;
		}
	}
	return true;
}

void UCGGameInstance::CopyAwayFactoryProductionFractions(TMap<FName, FFactorySaveData>& InOutFactories) const
{
	for (const auto& Pair : AwayFactoryProduction)
	{
		if (FFactorySaveData* FactoryRecord = InOutFactories.Find(Pair.Key))
		{
			FactoryRecord->AwayProductionFraction = Pair.Value.Fraction;
		}
	}
}

void UCGGameInstance::OnPostLoadMapWithWorld(UWorld* LoadedWorld)
{
	if (!LoadedWorld || !LoadedWorld->IsGameWorld() || LoadedWorld->GetGameInstance() != this)
	{
		return;
	}
	if (IsLevelTransitionActive() && bTransitionTravelRequested
		&& LoadedWorld != TransitionSourceWorld.Get() && IsDestinationWorld(LoadedWorld, PendingTransitionLevel))
	{
		bTransitionDestinationLoaded = true;
		// Blocking load time is unrestricted; the watchdog applies to readiness after loading.
		TransitionStartedAt = FPlatformTime::Seconds();
		UE_LOG(LogTemp, Log, TEXT("[LevelTransition] Loaded %s; waiting for presentation"), *PendingTransitionLevel);
		if (bClearVisitDataAfterTravel)
		{
			VisitCitySnapshot = FCitySnapshot();
			VisitTargetPlayFabId.Empty();
			VisitTargetDisplayName.Empty();
			bClearVisitDataAfterTravel = false;
		}
	}
	if (CurrentMapType == ECurrentMapType::MainMap && LoadedWorld->GetMapName().Contains(TEXT("Main")))
	{
		if (!IsLevelTransitionActive())
		{
			// Direct boot travel has already loaded the world without saving the loading-map state.
			BeginLevelTransition(UGameplayStatics::GetCurrentLevelName(LoadedWorld, true), 0, true);
		}
		// Keep the source Office context until the destination world has actually loaded.
		SetOfficeMode(EOfficeMode::Normal);
		SetCurrentManagedBuilding(nullptr);
	}

	// 부팅 로딩 맵(LoadingMap)은 무음 — Main BGM 이 로딩 화면에서 울리지 않게.
	// 첫 맵이 LoadingMap 이므로 Init 의 기본 MapType(MainMap)을 건드리지 않고 여기서만 가드.
	if (LoadedWorld->GetMapName().Contains(TEXT("LoadingMap")))
	{
		CurrentMapType = ECurrentMapType::None;
		if (USoundManagerSubsystem* LoadingSoundMgr = GetSubsystem<USoundManagerSubsystem>())
		{
			LoadingSoundMgr->StopMusic();
		}
		return;
	}
	if (bAwayFactoryTravelPaused)
	{
		if (CurrentMapType == ECurrentMapType::MainMap || bIsVisitMode)
		{
			StopAwayFactoryProduction();
		}
		else
		{
			bAwayFactoryTravelPaused = false;
			ResumeAwayFactoryProduction();
		}
	}

	USoundManagerSubsystem* SoundMgr = GetSubsystem<USoundManagerSubsystem>();
	if (!SoundMgr)
	{
		return;
	}

	const EMusicType MusicType = ResolveMusicTypeForCurrentMap();

	if (MusicType == EMusicType::None)
	{
		SoundMgr->StopMusic();
		return;
	}
	SoundMgr->PlayMusic(MusicType);
}

EMusicType UCGGameInstance::ResolveMusicTypeForCurrentMap() const
{
	switch (CurrentMapType)
	{
	case ECurrentMapType::MainMap:        return EMusicType::Main;
	case ECurrentMapType::WorldMap:       return EMusicType::WorldMap;
	case ECurrentMapType::RecruitmentMap: return EMusicType::None;
	case ECurrentMapType::OfficeMap:
		switch (CurrentBuildingCompanyType)
		{
		case ECompanyType::Game:           return EMusicType::OfficeGame;
		case ECompanyType::Electronics: return EMusicType::OfficeElectronics;
		case ECompanyType::Finance:        return EMusicType::OfficeFinance;
		case ECompanyType::IT:             return EMusicType::OfficeIT;
		case ECompanyType::Semiconductor:  return EMusicType::OfficeSemiconductor;
		case ECompanyType::Automobile:     return EMusicType::OfficeAutomobile;
		default:                           return EMusicType::None;
		}
	default: return EMusicType::None;
	}
}

void UCGGameInstance::SetCurrentLevelScript(ACGLevelScriptBase* cgLevelScriptBase)
{
	CurrentLevelScript = cgLevelScriptBase;
}


void UCGGameInstance::SetCurrentPlayerController(APlayerController* playerController)
{
	CurrentPlayerController = playerController;
}

APlayerController* UCGGameInstance::GetCurrentPlayerController()
{
	if (CurrentPlayerController == nullptr)
	{
		SetCurrentPlayerController(GetFirstLocalPlayerController());
	}

	return CurrentPlayerController;
}

void UCGGameInstance::TransitionToLevel(const FString& LevelName, int32 BackgroundIndex)
{
	if (const UWorld* TransitionWorld = GetWorld())
	{
		if (const UOfficeRecruitmentFlowSubsystem* Flow = TransitionWorld->GetSubsystem<UOfficeRecruitmentFlowSubsystem>())
		{
			if (Flow->IsBusy()) return;
		}
	}
	if (LevelName.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("[LevelTransition] Destination level is empty"));
		return;
	}
	if (IsLevelTransitionActive())
	{
		UE_LOG(LogTemp, Log, TEXT("[LevelTransition] Ignored duplicate transition to %s"), *LevelName);
		return;
	}
	if (BeginLevelTransition(LevelName, BackgroundIndex))
	{
		return;
	}
	PerformLevelTransition(LevelName, BackgroundIndex);
}

void UCGGameInstance::SetVisitMode(bool bInVisitMode)
{
	if (IsLevelTransitionActive() && !bTransitionTravelRequested)
	{
		// Destination mode must not affect the source world's save while the wipe is closing.
		bPendingVisitMode = bInVisitMode;
		bHasPendingVisitMode = true;
		return;
	}
	bIsVisitMode = bInVisitMode;
}

bool UCGGameInstance::IsVisitMode() const
{
	// Source callbacks may still run while OpenLevel tears down the old world.
	return IsLevelTransitionActive() && TransitionSourceWorld.IsValid() && GetWorld() == TransitionSourceWorld.Get()
		? bTransitionSourceVisitMode : bIsVisitMode;
}

bool UCGGameInstance::BeginLevelTransition(const FString& LevelName, int32 BackgroundIndex, bool bDestinationAlreadyLoaded)
{
	UGameViewportClient* GameViewport = GetGameViewportClient();
	if (!FSlateApplication::IsInitialized() || !GameViewport)
	{
		return false;
	}

	PendingTransitionLevel = LevelName;
	PendingTransitionBackground = BackgroundIndex;
	TransitionSourceMapType = CurrentMapType;
	TransitionStartedAt = TransitionPhaseStartedAt = FPlatformTime::Seconds();
	TransitionCoveredFrame = TransitionReadyFrame = 0;
	bTransitionTravelRequested = bDestinationAlreadyLoaded;
	bTransitionDestinationLoaded = bDestinationAlreadyLoaded;
	bTransitionResourcesPending = false;
	bTransitionSourceVisitMode = bIsVisitMode;
	TransitionSourceWorld = bDestinationAlreadyLoaded ? nullptr : GetWorld();
	bHasPendingVisitMode = bClearVisitDataAfterTravel = false;
	TransitionCoreResourcesReadyAt = TransitionReadyStartedAt = 0.0;

	// 페이드가 도는 동안 비동기 로더가 목적지 맵을 읽게 둔다. 블로킹 구간에서 디스크 I/O 를 빼는 유일한 창이다.
	if (!bDestinationAlreadyLoaded)
	{
		BeginLevelPreload(LevelName);
	}

	FLevelTransitionSnapshot Snapshot;
	Snapshot.bExit = CurrentMapType == ECurrentMapType::OfficeMap;
	Snapshot.bReduceMotion = USettingsManagerSubsystem::IsReduceMotion(this);
	Snapshot.PatternStartTime = TransitionStartedAt;
	if (bDestinationAlreadyLoaded)
	{
		Snapshot.Phase = ELevelTransitionPhase::Holding;
		Snapshot.Progress = 1.0f;
		Snapshot.HoldStartTime = TransitionStartedAt;
	}
	LevelTransitionState = MakeShared<FLevelTransitionVisualState, ESPMode::ThreadSafe>();
	LevelTransitionState->SetSnapshot(Snapshot);
	LevelTransitionViewport = GameViewport;
	PickTransitionTip(ContextForLevelName(LevelName));
	LevelTransitionViewportWidget = SNew(SLevelTransitionOverlay)
		.VisualState(LevelTransitionState)
		.PatternTexture(LevelTransitionPattern.Get())
		.TipText(CurrentTransitionTip)
		.TipBodyFont(TransitionTipBodyFont.Get())
		.TipLabelFont(TransitionTipLabelFont.Get())
		.TipIcon(TransitionTipIcon.Get());
	GameViewport->AddViewportWidgetContent(LevelTransitionViewportWidget.ToSharedRef(), 100000);
	LevelTransitionInputBlocker = MakeShared<FLevelTransitionInputBlocker>(GameViewport->GetGameViewportWidget());
	if (APlayerController* TransitionController = GetFirstLocalPlayerController())
	{
		// Releases will be consumed by the transition, so clear held gameplay actions first.
		TransitionController->FlushPressedKeys();
	}
	FSlateApplication::Get().RegisterInputPreProcessor(LevelTransitionInputBlocker, 0);
	TickDelegateHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UCGGameInstance::TickLevelTransition));
	UE_LOG(LogTemp, Log, TEXT("[LevelTransition] %s %s"),
		bDestinationAlreadyLoaded ? TEXT("Waiting for first presentation of") : TEXT("Closing for"), *LevelName);
	return true;
}

bool UCGGameInstance::TickLevelTransition(float DeltaTime)
{
	if (!LevelTransitionState.IsValid())
	{
		TickDelegateHandle.Reset();
		return false;
	}

	const double Now = FPlatformTime::Seconds();
	FLevelTransitionSnapshot Snapshot = LevelTransitionState->GetSnapshot();
	const double Duration = Snapshot.bReduceMotion
		? 0.001
		: FMath::Clamp(static_cast<double>(CGLevelTransitionCVars::FadeSeconds.GetValueOnGameThread()), 0.01, 2.0);
	if (Snapshot.Phase == ELevelTransitionPhase::Closing)
	{
		Snapshot.Progress = FMath::Clamp(static_cast<float>((Now - TransitionPhaseStartedAt) / Duration), 0.0f, 1.0f);
		if (Snapshot.Progress >= 1.0f)
		{
			Snapshot.Phase = ELevelTransitionPhase::Holding;
			Snapshot.HoldStartTime = Now;
			TransitionCoveredFrame = GFrameCounter;
			UE_LOG(LogTemp, Log, TEXT("[LevelTransition] Covered at frame %llu"), TransitionCoveredFrame);
		}
	}
	else if (Snapshot.Phase == ELevelTransitionPhase::Holding)
	{
		// Slate must submit a fully covered frame before synchronous saving or map loading begins.
		if (!bTransitionTravelRequested && GFrameCounter >= TransitionCoveredFrame + 2)
		{
			bTransitionTravelRequested = true;
			TransitionStartedAt = Now;
			UE_LOG(LogTemp, Log, TEXT("[LevelTransition] Traveling at frame %llu"), GFrameCounter);
			PerformLevelTransition(PendingTransitionLevel, PendingTransitionBackground);
		}
		else if (bTransitionTravelRequested && IsLevelTransitionDestinationReady())
		{
			if (TransitionReadyFrame == 0)
			{
				TransitionReadyFrame = GFrameCounter;
				TransitionReadyStartedAt = Now;
			}
			// Allow normal budgeted streaming to discover the restored camera view, then settle.
			const IConsoleVariable* StreamingFrames = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Streaming.FramesForFullUpdate"));
			const uint64 SettleFrames = StreamingFrames ? 2 * (FMath::Max(0, StreamingFrames->GetInt()) + 2) : 2;
			// 프레임 수만 세면 도착 직후 PSO 생성으로 기어가는 기기에서 수 초가 된다 (A9 실측 8프레임 = 3.6초)
			const double SettleCap = FMath::Max(0.0,
				static_cast<double>(CGLevelTransitionCVars::SettleMaxSeconds.GetValueOnGameThread()));
			if (GFrameCounter >= TransitionReadyFrame + SettleFrames || Now - TransitionReadyStartedAt >= SettleCap)
			{
				Snapshot.Phase = ELevelTransitionPhase::Opening;
				Snapshot.Progress = 0.0f;
				TransitionPhaseStartedAt = Now;
				UE_LOG(LogTemp, Log, TEXT("[LevelTransition] Destination ready, opening %s"), *PendingTransitionLevel);
			}
		}
		else
		{
			TransitionReadyFrame = 0;
			TransitionReadyStartedAt = 0.0;
		}
	}
	else
	{
		Snapshot.Progress = FMath::Clamp(static_cast<float>((Now - TransitionPhaseStartedAt) / Duration), 0.0f, 1.0f);
		if (Snapshot.Progress >= 1.0f)
		{
			UE_LOG(LogTemp, Log, TEXT("[LevelTransition] Finished %s"), *PendingTransitionLevel);
			FinishLevelTransition();
			TickDelegateHandle.Reset();
			return false;
		}
	}

	// Resource work can legitimately exceed the UI watchdog on a cold device.
	if (bTransitionResourcesPending)
	{
		TransitionStartedAt = Now;
	}
	if (bTransitionTravelRequested && Now - TransitionStartedAt > 30.0)
	{
		UE_LOG(LogTemp, Error, TEXT("[LevelTransition] Timed out waiting for %s; releasing transition input"), *PendingTransitionLevel);
		UWorld* TransitionWorld = GetWorld();
		if (!bTransitionDestinationLoaded || !IsDestinationWorld(TransitionWorld, PendingTransitionLevel))
		{
			CurrentMapType = TransitionSourceMapType;
			bIsVisitMode = bTransitionSourceVisitMode;
			if (TransitionSourceMapType == ECurrentMapType::OfficeMap)
			{
				PendingMainMapFocusBuildingIndex = INDEX_NONE;
			}
			if (CurrentMapType == ECurrentMapType::MainMap || bIsVisitMode)
			{
				StopAwayFactoryProduction();
			}
			else if (bAwayFactoryTravelPaused)
			{
				bAwayFactoryTravelPaused = false;
				ResumeAwayFactoryProduction();
			}
		}
		FinishLevelTransition();
		TickDelegateHandle.Reset();
		return false;
	}
	LevelTransitionState->SetSnapshot(Snapshot);
	return true;
}

bool UCGGameInstance::IsLevelTransitionDestinationReady()
{
	UWorld* DestinationWorld = GetWorld();
	if (!bTransitionDestinationLoaded || !DestinationWorld || !DestinationWorld->HasBegunPlay()
		|| !IsDestinationWorld(DestinationWorld, PendingTransitionLevel))
	{
		return false;
	}
	UUIManagerSubsystem* UIManager = GetSubsystem<UUIManagerSubsystem>();
	const bool bPackagesPending = IsAsyncLoading();
	UWorldPartitionSubsystem* Partition = DestinationWorld->GetSubsystem<UWorldPartitionSubsystem>();
	const bool bWorldStreamingPending = Partition && !Partition->IsAllStreamingCompleted();
	const int32 CompilingAssets = FAssetCompilingManager::Get().GetNumRemainingAssets();
	const uint32 PrecachingPSOs = PipelineStateCache::NumActivePrecacheRequests();
	const bool bCoreResourcesPending = bPackagesPending || bWorldStreamingPending || CompilingAssets > 0 || PrecachingPSOs > 0;

	// 렌더 에셋 스캔은 로드된 전 텍스처·메시를 훑으므로 목적지와 무관한 스트리밍 하나에도 계속 걸린다.
	// 코어 자원이 끝난 뒤로는 상한만큼만 더 기다리고, 남은 밉은 도착 후 채우게 둔다.
	bool bRenderAssetsPending = false;
	if (bCoreResourcesPending)
	{
		TransitionCoreResourcesReadyAt = 0.0;
	}
	else
	{
		const double Now = FPlatformTime::Seconds();
		if (TransitionCoreResourcesReadyAt <= 0.0)
		{
			TransitionCoreResourcesReadyAt = Now;
		}
		const double WaitCap = FMath::Max(0.0, static_cast<double>(CGLevelTransitionCVars::ResourceWaitSeconds.GetValueOnGameThread()));
		if (Now - TransitionCoreResourcesReadyAt < WaitCap)
		{
			for (TObjectIterator<UStreamableRenderAsset> AssetIt; AssetIt; ++AssetIt)
			{
				if (!AssetIt->IsTemplate() && AssetIt->HasPendingInitOrStreaming())
				{
					bRenderAssetsPending = true;
					break;
				}
			}
		}
	}
	const bool bResourcesPending = bCoreResourcesPending || bRenderAssetsPending;
	if (bResourcesPending != bTransitionResourcesPending)
	{
		UE_LOG(LogTemp, Log, TEXT("[LevelTransition] Resources pending=%d (packages=%d, world=%d, compiling=%d, PSOs=%u, render assets=%d)"),
			bResourcesPending, bPackagesPending, bWorldStreamingPending, CompilingAssets, PrecachingPSOs, bRenderAssetsPending);
	}
	bTransitionResourcesPending = bResourcesPending;
	if (bResourcesPending)
	{
		return false;
	}
	if (CurrentMapType == ECurrentMapType::OfficeMap)
	{
		const AOfficeGameMode* OfficeMode = DestinationWorld->GetAuthGameMode<AOfficeGameMode>();
		if (!OfficeMode || !OfficeMode->AreOfficeEmployeesReady() || !UIManager || !IsValid(UIManager->GetOfficeLayer()))
		{
			return false;
		}
	}
	else if (CurrentMapType == ECurrentMapType::MainMap)
	{
		if (bIsVisitMode)
		{
			const ACGGameModeBase* CityMode = DestinationWorld->GetAuthGameMode<ACGGameModeBase>();
			if (!CityMode || !CityMode->IsVisitPresentationReady())
			{
				return false;
			}
		}
		else if (!UIManager || !IsValid(UIManager->GetInGameLayer()))
		{
			return false;
		}
		else if (const ACGGameModeBase* CityMode = DestinationWorld->GetAuthGameMode<ACGGameModeBase>())
		{
			if (!CityMode->IsCityOpeningReadyForReveal()) return false;
		}
	}
	else if (CurrentMapType == ECurrentMapType::WorldMap && (!UIManager || !IsValid(UIManager->UILegacyWorldMapLayer)))
	{
		return false;
	}
	return true;
}

void UCGGameInstance::FinishLevelTransition()
{
	if (LevelTransitionState.IsValid())
	{
		if (APlayerController* TransitionController = GetFirstLocalPlayerController())
		{
			TransitionController->FlushPressedKeys();
		}
	}
	if (LevelTransitionState.IsValid() && IsMoviePlayerEnabled() && !GetMoviePlayer()->IsMovieCurrentlyPlaying())
	{
		// PIE may not play the prepared MoviePlayer screen. Do not leave it for a later travel.
		GetMoviePlayer()->SetupLoadingScreen(FLoadingScreenAttributes());
	}
	if (UGameViewportClient* GameViewport = LevelTransitionViewport.Get())
	{
		if (LevelTransitionViewportWidget.IsValid())
		{
			GameViewport->RemoveViewportWidgetContent(LevelTransitionViewportWidget.ToSharedRef());
		}
	}
	if (FSlateApplication::IsInitialized() && LevelTransitionInputBlocker.IsValid())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(LevelTransitionInputBlocker);
	}
	LevelTransitionInputBlocker.Reset();
	LevelTransitionViewportWidget.Reset();
	LevelTransitionViewport.Reset();
	LevelTransitionState.Reset();
	PendingTransitionLevel.Empty();
	// 목적지 월드는 엔진이 잡고 있으므로, 여기서 놓지 않으면 떠난 맵까지 계속 메모리에 남는다
	ReleaseLevelPreload();
	TransitionCoreResourcesReadyAt = TransitionReadyStartedAt = 0.0;
	bTransitionTravelRequested = false;
	bTransitionDestinationLoaded = false;
	bTransitionResourcesPending = false;
	TransitionSourceWorld.Reset();
	bHasPendingVisitMode = bClearVisitDataAfterTravel = false;
}

void UCGGameInstance::PrepareLevelLoadingScreen()
{
	if (!IsMoviePlayerEnabled() || !FSlateApplication::IsInitialized())
	{
		return;
	}
	FLevelTransitionVisualStatePtr LoadingState = LevelTransitionState;
	if (!LoadingState.IsValid())
	{
		// Engine/direct map loads use the same face even before a viewport transition has begun.
		FLevelTransitionSnapshot Snapshot;
		Snapshot.Phase = ELevelTransitionPhase::Holding;
		Snapshot.Progress = 1.0f;
		Snapshot.bReduceMotion = USettingsManagerSubsystem::IsReduceMotion(this);
		Snapshot.PatternStartTime = Snapshot.HoldStartTime = FPlatformTime::Seconds();
		LoadingState = MakeShared<FLevelTransitionVisualState, ESPMode::ThreadSafe>();
		LoadingState->SetSnapshot(Snapshot);
		PickTransitionTip(ContextForLevelName(PendingTransitionLevel));
	}
	FLoadingScreenAttributes LoadingScreen;
	LoadingScreen.WidgetLoadingScreen = SNew(SLevelTransitionOverlay)
		.VisualState(LoadingState)
		.PatternTexture(LevelTransitionPattern.Get())
		.TipText(CurrentTransitionTip)
		.TipBodyFont(TransitionTipBodyFont.Get())
		.TipLabelFont(TransitionTipLabelFont.Get())
		.TipIcon(TransitionTipIcon.Get());
	LoadingScreen.MinimumLoadingScreenDisplayTime = 0.0f;
	LoadingScreen.bAutoCompleteWhenLoadingCompletes = true;
	LoadingScreen.bMoviesAreSkippable = false;
	LoadingScreen.bWaitForManualStop = false;
	GetMoviePlayer()->SetupLoadingScreen(LoadingScreen);
}

ELoadingTipContext UCGGameInstance::ContextForLevelName(const FString& LevelName)
{
	// PerformLevelTransition 의 맵 타입 판정과 같은 문자열 규칙을 쓴다
	if (LevelName.Contains(TEXT("Office")))      return ELoadingTipContext::Office;
	if (LevelName.Contains(TEXT("Recruitment"))) return ELoadingTipContext::Recruitment;
	if (LevelName.Contains(TEXT("World")))       return ELoadingTipContext::WorldMap;
	if (LevelName.Contains(TEXT("Main")))        return ELoadingTipContext::City;
	return ELoadingTipContext::Any;
}

void UCGGameInstance::PickTransitionTip(ELoadingTipContext Context)
{
	// DT 미로드여도 밴드가 비지 않게 폴백부터 깐다
	CurrentTransitionTip = FLoadingTipRules::LastResort();
	const UTableManagerSubsystem* TableMgr = GetSubsystem<UTableManagerSubsystem>();
	if (!TableMgr)
	{
		return;
	}
	const TArray<FLoadingTipTable>& Rows = TableMgr->GetLoadingTips();
	const int32 Picked = FLoadingTipRules::Pick(Rows, Context, LastTransitionTipKey,
		static_cast<uint32>(FMath::Rand()));
	if (Rows.IsValidIndex(Picked))
	{
		CurrentTransitionTip = Rows[Picked].Body;
		LastTransitionTipKey = Rows[Picked].TipKey;
	}
}

#if !UE_BUILD_SHIPPING
void UCGGameInstance::PreviewLevelTransition(float Seconds, const FString& TipKey)
{
	UGameViewportClient* GameViewport = GetGameViewportClient();
	if (!FSlateApplication::IsInitialized() || !GameViewport || IsLevelTransitionActive())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LevelTransition] Preview skipped (transition active or no viewport)"));
		return;
	}
	FinishPreviewTransition();

	FLevelTransitionSnapshot Snapshot;
	Snapshot.Phase = ELevelTransitionPhase::Holding;
	Snapshot.Progress = 1.0f;
	Snapshot.bExit = CurrentMapType == ECurrentMapType::OfficeMap;
	Snapshot.bReduceMotion = USettingsManagerSubsystem::IsReduceMotion(this);
	Snapshot.PatternStartTime = Snapshot.HoldStartTime = FPlatformTime::Seconds();
	PreviewTransitionState = MakeShared<FLevelTransitionVisualState, ESPMode::ThreadSafe>();
	PreviewTransitionState->SetSnapshot(Snapshot);

	FText PreviewTip;
	if (!TipKey.IsEmpty())
	{
		const UTableManagerSubsystem* TableMgr = GetSubsystem<UTableManagerSubsystem>();
		const FName WantedKey(*TipKey);
		if (const FLoadingTipTable* Found = TableMgr
			? TableMgr->GetLoadingTips().FindByPredicate(
				[WantedKey](const FLoadingTipTable& Row) { return Row.TipKey == WantedKey; })
			: nullptr)
		{
			PreviewTip = Found->Body;
		}
		if (PreviewTip.IsEmpty())
		{
			UE_LOG(LogTemp, Warning, TEXT("[LevelTransition] Preview tip key not found: %s"), *TipKey);
		}
	}
	if (PreviewTip.IsEmpty())
	{
		PickTransitionTip(ELoadingTipContext::Any);
		PreviewTip = CurrentTransitionTip;
	}

	PreviewTransitionViewport = GameViewport;
	PreviewTransitionWidget = SNew(SLevelTransitionOverlay)
		.VisualState(PreviewTransitionState)
		.PatternTexture(LevelTransitionPattern.Get())
		.TipText(PreviewTip)
		.TipBodyFont(TransitionTipBodyFont.Get())
		.TipLabelFont(TransitionTipLabelFont.Get())
		.TipIcon(TransitionTipIcon.Get());
	// 입력 차단기를 달지 않는다 ― 미리보기에 갇히면 안 된다
	GameViewport->AddViewportWidgetContent(PreviewTransitionWidget.ToSharedRef(), 100000);
	const float HoldSeconds = FMath::Clamp(Seconds, 0.5f, 120.0f);
	PreviewTransitionEndTime = FPlatformTime::Seconds() + HoldSeconds;
	PreviewTickHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UCGGameInstance::TickPreviewTransition));
	UE_LOG(LogTemp, Log, TEXT("[LevelTransition] Preview for %.1fs: %s"),
		HoldSeconds, *PreviewTip.ToString());
}

bool UCGGameInstance::TickPreviewTransition(float DeltaTime)
{
	if (!PreviewTransitionState.IsValid() || FPlatformTime::Seconds() >= PreviewTransitionEndTime)
	{
		// return false 가 이 티커를 떼므로 핸들을 먼저 비운다 ― 자기 콜백 안에서 RemoveTicker 를 부르지 않기 위해
		PreviewTickHandle.Reset();
		FinishPreviewTransition();
		return false;
	}
	return true;
}

void UCGGameInstance::FinishPreviewTransition()
{
	if (PreviewTickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(PreviewTickHandle);
		PreviewTickHandle.Reset();
	}
	if (UGameViewportClient* GameViewport = PreviewTransitionViewport.Get())
	{
		if (PreviewTransitionWidget.IsValid())
		{
			GameViewport->RemoveViewportWidgetContent(PreviewTransitionWidget.ToSharedRef());
		}
	}
	PreviewTransitionWidget.Reset();
	PreviewTransitionViewport.Reset();
	PreviewTransitionState.Reset();
}
#endif

void UCGGameInstance::PerformLevelTransition(const FString& LevelName, int32 BackgroundIndex)
{
	UE_LOG(LogTemp, Warning, TEXT("[GameInstance] Starting transition to: %s (BG: %d)"), *LevelName, BackgroundIndex);
	const bool bGoingToMainMap = LevelName.Contains(TEXT("Main"));
	const bool bEnteringVisitMode = bHasPendingVisitMode && bPendingVisitMode;
	const bool bLeavingOwnCity = CurrentMapType == ECurrentMapType::MainMap
		&& !bGoingToMainMap && !bIsVisitMode && !bEnteringVisitMode;

	// Mine/Factory line 같이 중간 시점 저장 hook 이 없는 매니저들을 위해 떠날 때 1번 저장.
	// PIE 초기 진입 시는 이 함수 호출 안 되니 회귀 없음.
	bool bSavedBeforeTravel = false;
	if (!bIsVisitMode)
	{
		bSavedBeforeTravel = TrySaveGameBeforeLevelTransition();
	}
	if (bLeavingOwnCity && bSavedBeforeTravel)
	{
		BeginAwayFactoryProductionFromCity();
	}
	else if (!AwayFactoryProduction.IsEmpty())
	{
		PauseAwayFactoryProduction();
		bAwayFactoryTravelPaused = true;
	}
	if (bHasPendingVisitMode)
	{
		bIsVisitMode = bPendingVisitMode;
		bHasPendingVisitMode = false;
	}

	// 맵 타입 설정
	if (LevelName.Contains(TEXT("Office")))
	{
		// 일반 건물 패널 선택 때는 수집하지 않고 MainMap에서 Office로 이동할 때 한 번 캡처한다.
		// 채용 맵에서 돌아오는 경우에는 같은 입장 스냅샷을 유지한다.
		if (CurrentMapType == ECurrentMapType::MainMap)
		{
			UWorld* SourceWorld = GetWorld();
			UEntityManager* SourceEntities = SourceWorld ? SourceWorld->GetSubsystem<UEntityManager>() : nullptr;
			ABuildingBaseActor* ManagedBuilding = SourceEntities
				? SourceEntities->GetBuildingByIndex(CurrentManagedBuildingIndex) : nullptr;
			BuildOfficeNeighborhoodSnapshot(ManagedBuilding);
		}
		CurrentMapType = ECurrentMapType::OfficeMap;
	}
	else if (LevelName.Contains(TEXT("Main")))
	{
		// 오피스에서 나오는 길이면 방금 나온 건물을 기억해 둔다 — 도착한 도시 카메라가 그 건물을 프레이밍한다.
		// 월드맵·랭킹 방문에서 들어오는 길은 CurrentMapType 이 OfficeMap 이 아니라 걸리지 않는다.
		if (CurrentMapType == ECurrentMapType::OfficeMap && CurrentManagedBuildingIndex != INDEX_NONE)
		{
			PendingMainMapFocusBuildingIndex = CurrentManagedBuildingIndex;
		}
		CurrentMapType = ECurrentMapType::MainMap;
	}
	else if (LevelName.Contains(TEXT("Recruitment")))
	{
		CurrentMapType = ECurrentMapType::RecruitmentMap;
	}
	else if (LevelName.Contains(TEXT("World")))
	{
		CurrentMapType = ECurrentMapType::WorldMap;
	}
	else
	{
		CurrentMapType = ECurrentMapType::None;
	}
	UE_LOG(LogTemp, Log, TEXT("[GameInstance] CurrentMapType set to: %d"), (int32)CurrentMapType);

	if (IsMoviePlayerEnabled() && GetMoviePlayer()->IsInitialized())
	{
		// MoviePlayer uses its own widget tree while the game thread is blocked by OpenLevel.
		PrepareLevelLoadingScreen();
	}

	UGameplayStatics::OpenLevel(this, FName(*LevelName));
}

void UCGGameInstance::BuildOfficeNeighborhoodSnapshot(ABuildingBaseActor* Building)
{
	if (CGLevelTransitionCVars::CityPhoto.GetValueOnGameThread() == 0
		|| !IsValid(Building) || Building->IsActorBeingDestroyed())
	{
		OfficeNeighborhoodSnapshot = FOfficeNeighborhoodSnapshot();
		return;
	}

	UWorld* SourceWorld = Building->GetWorld();
	UEntityManager* SourceEntities = SourceWorld ? SourceWorld->GetSubsystem<UEntityManager>() : nullptr;
	// 도시에 건물이 늘거나 줄면 사진이 낡는다. 그 외에는 같은 사진을 계속 쓴다.
	const int32 CityStamp = SourceEntities ? SourceEntities->GetBuildings().Num() : INDEX_NONE;
	const int32 BuildingIndex = Building->GetBuildingIndex();
	const FTransform BuildingTransform = Building->GetActorTransform();
	FIntPoint OfficeTileCount = FIntPoint::NoneValue;
	if (USaveLoadManager* SaveManager = GetSubsystem<USaveLoadManager>())
	{
		if (USaveGame_GameData* SaveData = SaveManager->GetCurrentSaveData())
		{
			if (const FOfficeSaveData* OfficeData = SaveData->GameData.OfficeDataMap.Find(BuildingIndex))
			{
				OfficeTileCount = FIntPoint(OfficeData->TileCountX, OfficeData->TileCountY);
			}
		}
	}
	float OfficeFloorHeightCm = 0.f;
	const bool bSameOfficeHeight = Building->TryGetOfficeFloorHeight(OfficeFloorHeightCm)
		&& FMath::IsNearlyEqual(OfficeFloorHeightCm * FMath::Abs(BuildingTransform.GetScale3D().Z),
			CachedNeighborhoodSnapshot.OfficeHeightCm, 1.f);

	if (CachedNeighborhoodSnapshot.bValid
		&& CachedNeighborhoodBuildingIndex == BuildingIndex
		&& CachedNeighborhoodCityStamp == CityStamp
		&& bSameOfficeHeight
		&& CachedNeighborhoodTileCount == OfficeTileCount
		&& CachedNeighborhoodBuildingTransform.Equals(BuildingTransform, 0.1)
		&& IsValid(CachedNeighborhoodSnapshot.SourceBackdrop.Get()))
	{
		OfficeNeighborhoodSnapshot = CachedNeighborhoodSnapshot;
		UE_LOG(LogTemp, Log, TEXT("[LevelTransition] City photo reused (building %d)"), BuildingIndex);
		return;
	}

	OfficeNeighborhoodSnapshot = FOfficeNeighborhoodSnapshotBuilder::Capture(Building);
	CachedNeighborhoodSnapshot = OfficeNeighborhoodSnapshot;
	CachedNeighborhoodBuildingIndex = BuildingIndex;
	CachedNeighborhoodCityStamp = CityStamp;
	CachedNeighborhoodBuildingTransform = BuildingTransform;
	CachedNeighborhoodTileCount = OfficeTileCount;
}

FString UCGGameInstance::ResolveLevelPackageName(const FString& LevelName)
{
	return LevelName.StartsWith(TEXT("/"))
		? FPackageName::ObjectPathToPackageName(LevelName)
		: FString::Printf(TEXT("/Game/CompanyGrowth/Level/%s"), *LevelName);
}

void UCGGameInstance::BeginLevelPreload(const FString& LevelName)
{
	// PIE 는 에디터 월드를 복제해 쓰므로 선로딩이 이득도 없고 복제 경로와 엉킨다
	if (GIsEditor || CGLevelTransitionCVars::PreloadMap.GetValueOnGameThread() == 0 || LevelName.IsEmpty())
	{
		return;
	}
	const FString PackageName = ResolveLevelPackageName(LevelName);
	if (PreloadedLevelPackageName == PackageName)
	{
		return;
	}
	ReleaseLevelPreload();
	if (!FPackageName::DoesPackageExist(PackageName))
	{
		UE_LOG(LogTemp, Warning, TEXT("[LevelTransition] Preload skipped, package not found: %s"), *PackageName);
		return;
	}
	PreloadedLevelPackageName = PackageName;
	LoadPackageAsync(PackageName, FLoadPackageAsyncDelegate::CreateWeakLambda(this,
		[this, PackageName](const FName&, UPackage* LoadedPackage, EAsyncLoadingResult::Type LoadResult)
		{
			if (LoadResult != EAsyncLoadingResult::Succeeded || PreloadedLevelPackageName != PackageName)
			{
				return;
			}
			PreloadedLevelPackage = LoadedPackage;
			PreloadedLevelWorld = UWorld::FindWorldInPackage(LoadedPackage);
			UE_LOG(LogTemp, Log, TEXT("[LevelTransition] Preloaded %s (world=%d)"),
				*PackageName, PreloadedLevelWorld != nullptr);
		}));
}

void UCGGameInstance::ReleaseLevelPreload()
{
	PreloadedLevelPackage = nullptr;
	PreloadedLevelWorld = nullptr;
	PreloadedLevelPackageName.Empty();
}

void UCGGameInstance::CancelLevelPreloadIfIdle()
{
	// 전환이 시작됐으면 그 선로딩은 지금 쓰이는 중이다
	if (!IsLevelTransitionActive())
	{
		ReleaseLevelPreload();
	}
}

void UCGGameInstance::SaveGameBeforeLevelTransition()
{
	TrySaveGameBeforeLevelTransition();
}

bool UCGGameInstance::TrySaveGameBeforeLevelTransition()
{
    USaveLoadManager* SaveLoadManager = GetSubsystem<USaveLoadManager>();
    if (SaveLoadManager)
    {
        if (SaveLoadManager->SaveGameData())
        {
            UE_LOG(LogTemp, Warning, TEXT("[GameInstance] Game data saved before level transition"));
			return true;
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("[GameInstance] Failed to save game data"));
        }
    }
	return false;
}

void UCGGameInstance::LoadGameAfterLevelStart()
{
    // 방문 모드: 스냅샷 데이터로 빌딩만 스폰 (세이브 로드 스킵)
    if (bIsVisitMode)
    {
        UE_LOG(LogTemp, Log, TEXT("[GameInstance] Visit mode - loading city snapshot instead of save data"));

        UWorld* World = GetWorld();
        if (!World) return;

        UEntityManager* EntityMgr = World->GetSubsystem<UEntityManager>();
        if (!EntityMgr) return;

        // 스냅샷 빌딩 → FBuildingEntitySaveData 변환
        TArray<FBuildingEntitySaveData> VisitBuildings;
        for (int32 i = 0; i < VisitCitySnapshot.Buildings.Num(); ++i)
        {
            const FCitySnapshotBuilding& SnapBldg = VisitCitySnapshot.Buildings[i];

            FBuildingEntitySaveData EntityData;
            EntityData.BuildingIndex = i + 1;
            EntityData.InteractableName = SnapBldg.InteractableName;
            EntityData.Location = SnapBldg.Location;
            EntityData.Rotation = SnapBldg.Rotation;
            EntityData.Scale = SnapBldg.Scale;
            EntityData.BuildingData.Body_Module_Copies = SnapBldg.Body_Module_Copies;
            EntityData.BuildingData.AppliedSkinID = SnapBldg.AppliedSkinID;
            EntityData.BuildingData.CompanyType = SnapBldg.CompanyType;

            VisitBuildings.Add(MoveTemp(EntityData));
        }

        // 세이브 경로와 같은 복원 API 합류 (방문 도시도 동일 스폰 코드)
        EntityMgr->SetBuildingsData(VisitBuildings);
        EntityMgr->RestoreEntityDataFromLoad();

        UE_LOG(LogTemp, Log, TEXT("[GameInstance] Visit mode - spawned %d buildings from snapshot"),
            VisitBuildings.Num());
        return;
    }

    USaveLoadManager* SaveLoadManager = GetSubsystem<USaveLoadManager>();
    if (SaveLoadManager)
    {
        if (SaveLoadManager->LoadGameData())
        {
            UE_LOG(LogTemp, Warning, TEXT("[GameInstance] Game data loaded after level start"));
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[GameInstance] No save data found or failed to load"));
        }
    }
}


void UCGGameInstance::TestClothingRank(int32 RankValue)
{
    UEmployeeManager* EmployeeManager = GetSubsystem<UEmployeeManager>();
    if (EmployeeManager)
    {
        EmployeeManager->TestClothingRank(RankValue);
    }
}

void UCGGameInstance::ListEmployees()
{
    UEmployeeManager* EmployeeManager = GetSubsystem<UEmployeeManager>();
    if (EmployeeManager)
    {
        EmployeeManager->ListEmployees();
    }
}

// ========== Recruitment System ==========

void UCGGameInstance::TransitionToRecruitmentMap(const FGachaResultData& Result, int32 BackgroundIndex)
{
	PendingGachaResult = Result;
	bHasPendingGachaResult = true;

	UE_LOG(LogTemp, Log, TEXT("[GameInstance] 채용맵 전환 - 등급: %d, 티어: %d"),
		static_cast<uint8>(Result.PotentialRarity), static_cast<uint8>(Result.UsedTier));

	TransitionToLevel(TEXT("RecruitmentMap"), BackgroundIndex);
}

void UCGGameInstance::ReturnFromRecruitmentMap()
{
	bHasPendingGachaResult = false;

	UE_LOG(LogTemp, Log, TEXT("[GameInstance] 채용맵에서 오피스맵으로 복귀 (BuildingIndex: %d)"),
		CurrentManagedBuildingIndex);

	TransitionToLevel(TEXT("OfficeMap"));
}

// ========== Office System ==========

void UCGGameInstance::SetCurrentManagedBuilding(ABuildingBaseActor* Building)
{
	if (IsValid(Building) && !Building->IsActorBeingDestroyed())
	{
		SetCurrentManagedBuildingIndex(Building->GetBuildingIndex());
		UE_LOG(LogTemp, Warning, TEXT("[GameInstance] CurrentManagedBuilding set to: %s (BuildingIndex: %d)"),
			*Building->GetName(), CurrentManagedBuildingIndex);
	}
	else
	{
		CurrentManagedBuildingIndex = INDEX_NONE;
		OfficeNeighborhoodSnapshot = FOfficeNeighborhoodSnapshot();
		UE_LOG(LogTemp, Log, TEXT("[GameInstance] CurrentManagedBuilding cleared"));
	}
}

void UCGGameInstance::SetCurrentManagedBuildingIndex(int32 InIndex)
{
	if (CurrentManagedBuildingIndex != InIndex)
	{
		OfficeNeighborhoodSnapshot = FOfficeNeighborhoodSnapshot();
	}
	CurrentManagedBuildingIndex = InIndex;
}

FText UCGGameInstance::GetCompanyDisplayName(int32 BuildingIndex) const
{
	USaveLoadManager* SaveMgr = GetSubsystem<USaveLoadManager>();
	const USaveGame_GameData* Save = SaveMgr ? SaveMgr->GetCurrentSaveData() : nullptr;
	const UTableManagerSubsystem* TableMgr = GetSubsystem<UTableManagerSubsystem>();
	if (!Save || !TableMgr) { return FText::GetEmpty(); }

	TArray<FCompanyDisplayNameRules::FCompanyEntry> Companies;
	Companies.Reserve(Save->GameData.Buildings.Num());
	for (const FBuildingEntitySaveData& Building : Save->GameData.Buildings)
	{
		Companies.Add({ Building.BuildingIndex, Building.BuildingData.CompanyType });
	}
	ECompanyType Industry = ECompanyType::None;
	int32 Ordinal = 0;
	if (!FCompanyDisplayNameRules::FindOrdinal(Companies, BuildingIndex, Industry, Ordinal)) { return FText::GetEmpty(); }

	bool bFound = false;
	const FCompanyInfoTable Info = TableMgr->GetCompanyInfo(Industry, bFound);
	if (!bFound || Info.DisplayName.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[GameInstance] DT_CompanyInfo 에 %s 업종 이름이 없어 회사 이름을 비웁니다"), *UEnum::GetValueAsString(Industry));
		return FText::GetEmpty();
	}
	return FCompanyDisplayNameRules::Format(Info.DisplayName, Ordinal);
}

int32 UCGGameInstance::ConsumePendingMainMapFocusBuilding()
{
	const int32 Index = PendingMainMapFocusBuildingIndex;
	PendingMainMapFocusBuildingIndex = INDEX_NONE;
	return Index;
}

void UCGGameInstance::SetOfficeMode(EOfficeMode Mode)
{
	CurrentOfficeMode = Mode;
	UE_LOG(LogTemp, Log, TEXT("[GameInstance] OfficeMode set to: %d"), static_cast<uint8>(Mode));
}

EOfficeMode UCGGameInstance::GetOfficeMode() const
{
	return CurrentOfficeMode;
}
