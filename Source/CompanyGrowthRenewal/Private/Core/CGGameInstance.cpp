// Fill out your copyright notice in the Description page of Project Settings.


#include "CGGameInstance.h"
#include "Misc/CoreDelegates.h"
#include "Kismet/GameplayStatics.h"
#include "Manager/UIManagerSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/PlayFabManagerSubsystem.h"
#include "Manager/SoundManagerSubsystem.h"
#include "Manager/EntityManager.h"
#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "Entity/Building/BuildingBaseActor.h"

// LoadingScreen Plugin
#include "AsyncLoadingScreenLibrary.h"

UCGGameInstance* UCGGameInstance::Instance = nullptr;

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

	// 자동 게스트 로그인 (나중에 로그인 레벨로 대체 예정)
	if (UPlayFabManagerSubsystem* PlayFabMgr = GetSubsystem<UPlayFabManagerSubsystem>())
	{
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
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
	FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Remove(EnterBackgroundHandle);
	FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Remove(EnterForegroundHandle);
	Super::Shutdown();
}

void UCGGameInstance::HandleAppEnterBackground()
{
	// 로컬 동기 작업만 — 서버 시간 조회 등 비동기는 서스펜드로 미전송이라 여기서 안 함
	BackgroundEnterRealtime = FPlatformTime::Seconds();

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
	if (AwaySeconds < 60.0)
	{
		return;
	}

	// warm resume — 프로세스가 살아 있던 채 오래 백그라운드였으면 오프라인 정산이 안 돌던 버그.
	// StartOfflineGainsSequence 내부에 중복 시퀀스 가드(PendingTimeRequest/bAwaitingLastSyncResponse) 있어 재호출 안전.
	if (UPlayFabManagerSubsystem* PlayFabMgr = GetSubsystem<UPlayFabManagerSubsystem>())
	{
		PlayFabMgr->StartOfflineGainsSequence();
	}
}

void UCGGameInstance::OnPostLoadMapWithWorld(UWorld* LoadedWorld)
{
	if (!LoadedWorld || !LoadedWorld->IsGameWorld())
	{
		return;
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
	UE_LOG(LogTemp, Warning, TEXT("[GameInstance] Starting transition to: %s (BG: %d)"), *LevelName, BackgroundIndex);

	// Mine/Factory line 같이 중간 시점 저장 hook 이 없는 매니저들을 위해 떠날 때 1번 저장.
	// PIE 초기 진입 시는 이 함수 호출 안 되니 회귀 없음.
	SaveGameBeforeLevelTransition();

	// 맵 타입 설정
	if (LevelName.Contains(TEXT("Office")))
	{
		CurrentMapType = ECurrentMapType::OfficeMap;
	}
	else if (LevelName.Contains(TEXT("Main")))
	{
		CurrentMapType = ECurrentMapType::MainMap;
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

	// 배경과 팁 설정
	UAsyncLoadingScreenLibrary::SetDisplayBackgroundIndex(BackgroundIndex);
	UAsyncLoadingScreenLibrary::SetDisplayTipTextIndex(BackgroundIndex);

	UGameplayStatics::OpenLevel(this, FName(*LevelName));
}

void UCGGameInstance::SaveGameBeforeLevelTransition()
{
    USaveLoadManager* SaveLoadManager = GetSubsystem<USaveLoadManager>();
    if (SaveLoadManager)
    {
        if (SaveLoadManager->SaveGameData())
        {
            UE_LOG(LogTemp, Warning, TEXT("[GameInstance] Game data saved before level transition"));
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("[GameInstance] Failed to save game data"));
        }
    }
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


// ========== Office System ==========

void UCGGameInstance::SetCurrentManagedBuilding(ABuildingBaseActor* Building)
{
	if (Building)
	{
		CurrentManagedBuildingIndex = Building->GetBuildingIndex();
		UE_LOG(LogTemp, Warning, TEXT("[GameInstance] CurrentManagedBuilding set to: %s (BuildingIndex: %d)"),
			*Building->GetName(), CurrentManagedBuildingIndex);
	}
	else
	{
		CurrentManagedBuildingIndex = INDEX_NONE;
		UE_LOG(LogTemp, Log, TEXT("[GameInstance] CurrentManagedBuilding cleared"));
	}
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
