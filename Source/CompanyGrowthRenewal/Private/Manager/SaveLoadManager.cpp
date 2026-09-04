// Fill out your copyright notice in the Description page of Project Settings.


#include "Manager/SaveLoadManager.h"
#include "Kismet/GameplayStatics.h"
#include "Manager/EmployeeManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Table/CompanyInfoTable.h"
#include "Manager/RecruitmentManagerSubsystem.h"
#include "Manager/BuildingTraitManagerSubsystem.h"
#include "Enum/BuildingTraitTarget.h"
#include "Manager/BuildingSkinManagerSubsystem.h"
#include "Manager/ItemInventoryManager.h"
#include "Manager/ShopManagerSubsystem.h"
#include "Manager/OfficeStageProgressManager.h"
#include "Manager/ProjectOperationManager.h"
#include "Manager/ProductionOrderManager.h"
#include "Manager/RankingManagerSubsystem.h"
#include "Manager/WorldMapManager.h"
#include "Manager/MineManager.h"
#include "Manager/WorldFactoryManager.h"
#include "Manager/CountryMarketManager.h"
#include "Manager/TradeOrderManager.h"
#include "Manager/PlayFabManagerSubsystem.h"
#include "Manager/UIManagerSubsystem.h"
#include "Manager/MissionManagerSubsystem.h"
#include "Manager/GoalBoardSubsystem.h"
#include "Manager/PanelIntroSubsystem.h"
#include "Manager/LaunchLootManagerSubsystem.h"
#include "Global/GlobalUtilFunctions.h"
#include "Global/CGDevSettings.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Manager/SpawnManager.h"
#include "Entity/Factory/BrickFactory.h"
#include "Entity/Building/BuildingBaseActor.h"
#include "Office/OfficeInterior.h"
#include "Office/OfficeManager.h"
#include "Core/CGGameInstance.h"
#include "Data/BuildingEnhancementData.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Utils/CF1CaptureSavePolicy.h"

void USaveLoadManager::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    const bool bSuppressCf1CaptureWrites = CGR::CF1CaptureSavePolicy::ShouldSuppressSaveWrites(
        FCommandLine::Get(), FPaths::ProjectSavedDir());
    if (bSuppressCf1CaptureWrites)
    {
        SuppressSaving();
        UE_LOG(LogTemp, Warning, TEXT("[SaveLoadManager] CF1 isolated capture: save writes suppressed for this process"));
    }
    // 부스 시연 — LoadGameData 보다 먼저 지워서 매 실행이 "세이브 없음" 분기(=신규 게임)를 타게 한다.
    // CF1은 격리 시드가 장면 권위이므로 wipe 설정과 무관하게 이 분기를 타지 않는다.
    else if (UCGDevSettings::ShouldWipeSaveOnLaunch())
    {
        WipeAllSaveData();
    }

    // PlayFab 로그인 직후 브로드캐스트되는 오프라인 보상 요청 수신
    if (UPlayFabManagerSubsystem* PFMgr = GetGameInstance()->GetSubsystem<UPlayFabManagerSubsystem>())
    {
        PFMgr->OnOfflineGainsRequested.AddUObject(this, &USaveLoadManager::CalculateOfflineGains);
        UE_LOG(LogTemp, Log, TEXT("[SaveLoadManager] PlayFab OnOfflineGainsRequested 바인딩 완료"));
    }

    // 시총 변경 → 호칭 승격 자동 체크 + PlayFab 리더보드 업로드
    // Money 변경 → 주간매출 Delta 업로드
    if (UResourceItemManager* RMgr = GetGameInstance()->GetSubsystem<UResourceItemManager>())
    {
        RMgr->OnResourceChanged.AddUObject(this, &USaveLoadManager::OnResourceChangedHandler);
        LastMoneyValue = RMgr->GetResourceAmount(EResourceType::Money);  // Diff 기준선
        UE_LOG(LogTemp, Log, TEXT("[SaveLoadManager] ResourceItemManager OnResourceChanged 바인딩 완료 (Money 기준선 %lld)"), LastMoneyValue);
    }
}

void USaveLoadManager::Deinitialize()
{
    // 종료 경로 마지막 상태 보장 — 기존엔 3초 debounce 에만 의존해 마지막 변경이 유실될 수 있었음
    SaveGameData();

    if (UPlayFabManagerSubsystem* PFMgr = GetGameInstance()->GetSubsystem<UPlayFabManagerSubsystem>())
    {
        PFMgr->OnOfflineGainsRequested.RemoveAll(this);
    }

    if (UResourceItemManager* RMgr = GetGameInstance()->GetSubsystem<UResourceItemManager>())
    {
        RMgr->OnResourceChanged.RemoveAll(this);
    }

    Super::Deinitialize();
}

void USaveLoadManager::OnResourceChangedHandler(EResourceType Type, int64 NewValue)
{
    URankingManagerSubsystem* RankMgr = GetGameInstance()->GetSubsystem<URankingManagerSubsystem>();

    // dev 프리셋 시드 구간 — 업로드만 막는다. 승격 루프는 그대로 둬야 프리셋이 등급을 자연 승격시킨다.
    if (bLeaderboardUploadSuppressed)
    {
        RankMgr = nullptr;
    }

    if (Type == EResourceType::MarketCap)
    {
        // 1. 자동 승격 체크 — 조건 충족 시 연쇄 승격 가능 (시총 급증 시 한 번에 여러 단계)
        while (TryPromoteTitle())
        {
            // TryPromoteTitle 내부에서 호칭 승격 + 델리게이트 + 저장 수행
        }

        // 2. 시총 리더보드 업로드 (Last 모드 — 현재 값 그대로)
        if (RankMgr) RankMgr->UploadMarketCap(NewValue);
    }
    else if (Type == EResourceType::Money)
    {
        // Money 증가분만 주간매출 리더보드에 업로드 (Sum 모드). 지출(감소) 시엔 무시
        const int64 Delta = NewValue - LastMoneyValue;
        if (Delta > 0 && RankMgr)
        {
            RankMgr->UploadWeeklyRevenueDelta(Delta);
        }
        LastMoneyValue = NewValue;
    }
}

bool USaveLoadManager::ComputeOfflineGains(float OfflineSeconds, bool bApplyToSave, FOfflineGainsResult& OutResult)
{
    OutResult = FOfflineGainsResult();

    USaveGame_GameData* SaveData = GetCurrentSaveData();
    if (!SaveData)
    {
        return false;
    }

    // BuildingIndex -> FBuildingSaveData* 빠른 조회용 역색인 (TArray 순회 비용 절약)
    TMap<int32, FBuildingSaveData*> BuildingLookup;
    for (FBuildingEntitySaveData& Entry : SaveData->GameData.Buildings)
    {
        BuildingLookup.Add(Entry.BuildingIndex, &Entry.BuildingData);
    }

    for (TPair<int32, FOfficeSaveData>& OfficePair : SaveData->GameData.OfficeDataMap)
    {
        const int32 BuildingIndex = OfficePair.Key;
        FOfficeSaveData& OfficeData = OfficePair.Value;

        // 운영 중인 프로젝트가 있어야 오프라인 수익 발생
        if (!OfficeData.bHasActiveOperation || OfficeData.CurrentOperation.State != EOperationState::Operating)
        {
            continue;
        }

        const float OnlineRatePerSec = OfficeData.CurrentOperation.ActualRevenuePerSecond;
        if (OnlineRatePerSec <= 0.0f)
        {
            continue;
        }

        FBuildingSaveData** BuildingPtr = BuildingLookup.Find(BuildingIndex);

        // 운영 남은 시간을 초과한 오프라인 수익은 발생 불가 — 운영 종료 후엔 더 이상 못 벎
        const float EffectiveSeconds = FMath::Min(OfflineSeconds, OfficeData.CurrentOperation.RemainingTime);
        // 건물 특성 (Idle → 오프라인 수익 배율)
        float IdleTraitFactor = 1.0f;
        if (UCGGameInstance* CGI = UCGGameInstance::GetInstance())
        {
            if (UBuildingTraitManagerSubsystem* TraitMgr = CGI->GetSubsystem<UBuildingTraitManagerSubsystem>())
            {
                IdleTraitFactor = TraitMgr->GetTraitFactor(BuildingIndex, EBuildingTraitTarget::IdleIncome);
            }
        }
        // 오프라인 정산률은 온라인과 동률(1.0) — 강화 레벨에 연동되는 노브가 아니다.
        // RawIncome 은 실제 벌이(감쇠반영 ActualRevenuePerSecond) 유지 — 용량계산만 시간기반 정본 경유.
        const float RawIncome = OnlineRatePerSec * EffectiveSeconds * IdleTraitFactor;

        // VaultCap — 용량 단일 정본에 위임 (티어 기준레이트 × VaultSeconds, 운영 유무 무관)
        float VaultCap = ABuildingBaseActor::BaseVaultCapacity;
        if (UProjectOperationManager* OpMgr = GetGameInstance()->GetSubsystem<UProjectOperationManager>())
        {
            VaultCap = OpMgr->CalculateWarehouseCapacity(BuildingIndex);
        }
        const float AvailableSpace = FMath::Max(0.0f, VaultCap - OfficeData.StoredRevenue);
        const float ActualGain = FMath::Min(RawIncome, AvailableSpace);

        OutResult.TotalGained += ActualGain;
        ++OutResult.EligibleBuildings;

        // 6-2 정산 내역: 금고 초과 손실(강화 유도)과 운영 수명 만료(정상 종료)를 분리 기록.
        // RemainingTime 은 아래 진행 블록에서 갱신되므로 반드시 그 전에 원본값으로 판정.
        FOfflineGainEntry Entry;
        Entry.BuildingIndex = BuildingIndex;
        Entry.RawGain = RawIncome;
        Entry.ActualGain = ActualGain;
        Entry.LossByVault = FMath::Max(0.0f, RawIncome - ActualGain);
        Entry.VaultLevel = BuildingPtr ? (*BuildingPtr)->EnhancementLevels.FindRef(EBuildingEnhancementType::VaultCapacity) : 0;
        Entry.bLifespanBound = (OfficeData.CurrentOperation.RemainingTime < OfflineSeconds);
        OutResult.TotalLostToVaultCap += Entry.LossByVault;
        OutResult.Entries.Add(Entry);

        // 운영 시간도 오프라인 동안 일부 진행 — 수익 정산률(100%)과 분리된 별도 비율
        // 직원이 어느 정도는 일했다는 일관성 확보, 다만 Tick 만료 자연 처리 위해 TotalOperationTime 으로 클램핑
        constexpr float OfflineTimeProgressRate = 0.2f;
        const float TimeProgressed = OfflineSeconds * OfflineTimeProgressRate;
        OutResult.ElapsedAdvance = TimeProgressed;

        // dry-run 은 여기서 멈춘다 — 금고 적립도 운영 경과도 세이브에 쓰지 않는다.
        if (!bApplyToSave)
        {
            continue;
        }

        OfficeData.StoredRevenue += ActualGain;
        OfficeData.CurrentOperation.ElapsedTime = FMath::Min(
            OfficeData.CurrentOperation.ElapsedTime + TimeProgressed,
            OfficeData.CurrentOperation.TotalOperationTime);
        OfficeData.CurrentOperation.RemainingTime = FMath::Max(
            0.0f,
            OfficeData.CurrentOperation.TotalOperationTime - OfficeData.CurrentOperation.ElapsedTime);

        UE_LOG(LogTemp, Log, TEXT("[SaveLoadManager] 오프라인 보상 빌딩 %d: Rate=%.2f/s x EffSec=%.0fs (Off=%.0fs, Remain=%.0fs) = Raw=%.1f, Cap=%.1f, Gain=%.1f, TimeAdvanced=%.1fs"),
            BuildingIndex, OnlineRatePerSec, EffectiveSeconds, OfflineSeconds, OfficeData.CurrentOperation.RemainingTime, RawIncome, VaultCap, ActualGain, TimeProgressed);
    }

    return true;
}

void USaveLoadManager::CalculateOfflineGains(float OfflineSeconds)
{
    // 60초 미만은 네트워크/재접속 오차로 간주, 무시
    if (OfflineSeconds < 60.0f)
    {
        UE_LOG(LogTemp, Log, TEXT("[SaveLoadManager] 오프라인 시간 짧음 (%.0f초) — 보상 스킵"), OfflineSeconds);
        OnOfflineGainsApplied.Broadcast(0.0f, OfflineSeconds);
        return;
    }

    FOfflineGainsResult GainsResult;
    if (!ComputeOfflineGains(OfflineSeconds, /*bApplyToSave=*/true, GainsResult))
    {
        UE_LOG(LogTemp, Warning, TEXT("[SaveLoadManager] 오프라인 보상 계산 실패: 세이브 데이터 없음"));
        return;
    }

    const float TotalGained = GainsResult.TotalGained;
    const int32 EligibleBuildings = GainsResult.EligibleBuildings;
    const float TotalLostToVaultCap = GainsResult.TotalLostToVaultCap;
    TArray<FOfflineGainEntry> Entries = MoveTemp(GainsResult.Entries);

    if (TotalGained > 0.0f)
    {
        SaveGameData();
    }

    UE_LOG(LogTemp, Log, TEXT("[SaveLoadManager] 오프라인 보상 완료: %d개 빌딩, 총 %.1f원, %.0f초"),
        EligibleBuildings, TotalGained, OfflineSeconds);

    OnOfflineGainsApplied.Broadcast(TotalGained, OfflineSeconds);

    // 정산 보고는 보관만, 인라인 Broadcast 금지 (로그인 = UIBase 부재, 복귀 = Consume과 이중 표시)
    // → 두 경로 모두 UI 준비 확인 시 Consume 1회. CapReached = 12h 상한(OfflineCapSeconds) 도달
    bHasPendingOfflineReport = (TotalGained > 0.0f || Entries.Num() > 0);
    PendingOfflineTotal = TotalGained;
    PendingOfflineSeconds = OfflineSeconds;
    bPendingOfflineCapReached = (OfflineSeconds >= static_cast<float>(UPlayFabManagerSubsystem::OfflineCapSeconds));
    PendingOfflineEntries = MoveTemp(Entries);

    UE_LOG(LogTemp, Log, TEXT("[SaveLoadManager] 오프라인 정산 상세 보관: 금고초과 손실합=%.1f원, 캡도달=%s, 내역 %d건 (Consume 대기)"),
        TotalLostToVaultCap, bPendingOfflineCapReached ? TEXT("Y") : TEXT("N"), PendingOfflineEntries.Num());

    // 로그인 시 토스트 알림 (블로킹 없는 피드백) — 운영 중 빌딩이 없으면 스킵
    if (TotalGained > 0.0f)
    {
        if (UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
        {
            const int32 TotalMinutes = FMath::FloorToInt(OfflineSeconds / 60.0f);
            const int32 Hours = TotalMinutes / 60;
            const int32 Minutes = TotalMinutes % 60;

            // 시간 한글화 + 안내톤 + 실제 귀속처('금고 적립', 지급 아님) 명시
            FString TimeStr = (Hours > 0)
                ? FString::Printf(TEXT("%d시간 %d분"), Hours, Minutes)
                : FString::Printf(TEXT("%d분"), FMath::Max(1, Minutes));

            const FString Msg = FString::Printf(TEXT("%s 동안 %s원이 금고에 적립되었습니다"), *TimeStr,
                *UGlobalUtilFunctions::AbbreviateNumber(static_cast<int64>(TotalGained),
                    ENumberAbbrevStyle::Auto, ENumberRoundMode::Floor).ToString());

            UIMgr->ShowNotification(FText::FromString(Msg), 4.0f, ENotificationType::Success);
        }
    }

    // 정산 모달 표시 시도 — 포그라운드 복귀 경로(UI 이미 준비)를 커버. 로그인 경로는 UIBase 가 아직
    // 없어 여기서 조용히 무시되고, ShowMainMapUI 끝의 동일 호출이 flush 한다(멱등).
    if (UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
    {
        UIMgr->TryShowOfflineReport();
    }
}

void USaveLoadManager::DebugSetPendingOfflineReport(float InTotal, float InSeconds,
    const TArray<FOfflineGainEntry>& InEntries, bool bInCapReached)
{
    bHasPendingOfflineReport = true;
    PendingOfflineTotal = InTotal;
    PendingOfflineSeconds = InSeconds;
    bPendingOfflineCapReached = bInCapReached;
    PendingOfflineEntries = InEntries;
}

void USaveLoadManager::ConsumePendingOfflineReport()
{
    // 보류 없으면 무동작. 여러 진입점에서 불려도 안전 (멱등)
    if (!bHasPendingOfflineReport)
    {
        return;
    }

    // MainMap UI 준비 시점 재발화 (로그인 콜백 때 UIBase 부재로 놓친 모달)
    OnOfflineGainsDetailed.Broadcast(PendingOfflineTotal, PendingOfflineSeconds, PendingOfflineEntries, bPendingOfflineCapReached);

    // 1회성 소비 — 재바인딩/재진입으로 중복 표시되지 않도록 상태 초기화
    bHasPendingOfflineReport = false;
    PendingOfflineTotal = 0.0f;
    PendingOfflineSeconds = 0.0f;
    bPendingOfflineCapReached = false;
    PendingOfflineEntries.Reset();
}

// 모든 저장의 단일 관문. 아래 게이트 2개 통과 후 디스크 기록
bool USaveLoadManager::SaveGameData()
{
    // 데이터 초기화 중 세이브 억제
    if (bSaveSuppressed)
    {
        UE_LOG(LogTemp, Warning, TEXT("SaveGameData blocked: data reset in progress"));
        return false;
    }

    // 최초 로드 전 저장 차단: Initialize 단계 SaveGameData가 빈 값(미션 None·빌딩 0)으로 세이브를 덮음
    // (튜토리얼 진행 유실의 진범)
    if (!bInitialLoadComplete)
    {
        UE_LOG(LogTemp, Warning, TEXT("[SaveLoadManager] SaveGameData 무시 — 최초 로드 완료 전(서브시스템 초기화 단계). 세이브 보호."));
        return false;
    }

    // 기존 데이터 가져오기 (없으면 새로 생성)
    USaveGame_GameData* SaveGameInstance = GetCurrentSaveData();

    if (!SaveGameInstance)
    {
        SaveGameInstance = Cast<USaveGame_GameData>(
            UGameplayStatics::CreateSaveGameObject(USaveGame_GameData::StaticClass()));

        // 새 게임 시작 시 기본 스킨 100 지급
        if (SaveGameInstance)
        {
            FBuildingSkinInstance DefaultSkin(100);
            SaveGameInstance->GameData.OwnedBuildingSkins.Add(DefaultSkin);
            UE_LOG(LogTemp, Log, TEXT("[SaveLoadManager] New save file created with default skin 100"));
        }
    }

    if (!SaveGameInstance) return false;

    // EmployeeManager에서 NextEmployeeID만 저장 (전역 관리)
    // 직원 목록은 건물별로 FOfficeSaveData에 저장됨
    UEmployeeManager* EmployeeManager = GetGameInstance()->GetSubsystem<UEmployeeManager>();
    if (EmployeeManager)
    {
        SaveGameInstance->GameData.NextEmployeeID = EmployeeManager->GetNextInstanceID();
    }

    // ResourceItemManager에서 데이터 가져오기
    UResourceItemManager* ResourceManager = GetGameInstance()->GetSubsystem<UResourceItemManager>();
    if (ResourceManager)
    {
        SaveGameInstance->GameData.ResourceBank = ResourceManager->GetAllResources();
    }

    // ItemInventoryManager에서 아이템 데이터 가져오기
    UItemInventoryManager* ItemInvMgr = GetGameInstance()->GetSubsystem<UItemInventoryManager>();
    if (ItemInvMgr)
    {
        SaveGameInstance->GameData.ItemInventory = ItemInvMgr->GetAllItems();
        UE_LOG(LogTemp, Log, TEXT("[SaveLoadManager] Saved ItemInventory: %d types"),
            SaveGameInstance->GameData.ItemInventory.Num());
    }

    // RecruitmentManagerSubsystem에서 오피스 채용 + 가챠 데이터 가져오기
    URecruitmentManagerSubsystem* RecruitmentMgr = GetGameInstance()->GetSubsystem<URecruitmentManagerSubsystem>();
    if (RecruitmentMgr)
    {
        SaveGameInstance->GameData.OfficeRecruitmentMap = RecruitmentMgr->GetOfficeRecruitmentMap();
        SaveGameInstance->GameData.GachaRecruitmentData = RecruitmentMgr->GetGachaData();

        UE_LOG(LogTemp, Log, TEXT("[SaveLoadManager] Saved OfficeRecruitmentMap: %d entries, GachaData: AdvPity=%d, PremPity=%d, Mileage=%d"),
            SaveGameInstance->GameData.OfficeRecruitmentMap.Num(),
            SaveGameInstance->GameData.GachaRecruitmentData.AdvancedPity.PullsSinceLastGuaranteed,
            SaveGameInstance->GameData.GachaRecruitmentData.PremiumPity.PullsSinceLastGuaranteed,
            SaveGameInstance->GameData.GachaRecruitmentData.Mileage.CurrentPoints);
    }

    // ShopManagerSubsystem에서 상점 구매 상태 가져오기
    if (UShopManagerSubsystem* ShopMgr = GetGameInstance()->GetSubsystem<UShopManagerSubsystem>())
    {
        SaveGameInstance->GameData.ShopSaveData = ShopMgr->GetShopData();
    }

    // BuildingTraitManager에서 글로벌 특성 인벤토리 가져오기
    UBuildingTraitManagerSubsystem* TraitMgr = GetGameInstance()->GetSubsystem<UBuildingTraitManagerSubsystem>();
    if (TraitMgr)
    {
        SaveGameInstance->GameData.BuildingTraitInventory = TraitMgr->GetInventory();
        UE_LOG(LogTemp, Log, TEXT("[SaveLoadManager] Saved BuildingTraitInventory: %d traits owned, %d encyclopedia, dust=%d, mileage=%d"),
            SaveGameInstance->GameData.BuildingTraitInventory.OwnedTraits.Num(),
            SaveGameInstance->GameData.BuildingTraitInventory.EncyclopediaUnlocked.Num(),
            SaveGameInstance->GameData.BuildingTraitInventory.DismantleDust,
            SaveGameInstance->GameData.BuildingTraitInventory.GachaData.MileagePoints);
    }

    // BuildingSkinManager 에서 스킨 가챠 천장/마일리지 (소유 스킨은 OwnedBuildingSkins 가 직접 저장)
    UBuildingSkinManagerSubsystem* SkinMgr = GetGameInstance()->GetSubsystem<UBuildingSkinManagerSubsystem>();
    if (SkinMgr)
    {
        SaveGameInstance->GameData.BuildingSkinGachaInventory = SkinMgr->GetInventory();
    }

    // NextBuildingIndex 저장 (CGGameInstance에서 가져오기)
    UCGGameInstance* GameInst = Cast<UCGGameInstance>(GetGameInstance());
    if (GameInst)
    {
        SaveGameInstance->GameData.NextBuildingIndex = GameInst->GetNextBuildingIndex();
        UE_LOG(LogTemp, Log, TEXT("[SaveLoadManager] Saved NextBuildingIndex: %d"),
            SaveGameInstance->GameData.NextBuildingIndex);
    }

    // MissionManager에서 현재 미션 진행 수집
    if (UMissionManagerSubsystem* MissionMgr = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>())
    {
        SaveGameInstance->GameData.CurrentMissionID = MissionMgr->GetActiveMissionID();
        SaveGameInstance->GameData.CurrentMissionProgress = 0;
        if (MissionMgr->GetActiveConditionType() == EMissionConditionType::PlaceDesks)
        {
            int64 CurrentProgress = 0;
            int64 TargetProgress = 0;
            if (MissionMgr->GetMissionProgress(CurrentProgress, TargetProgress))
            {
                SaveGameInstance->GameData.CurrentMissionProgress = static_cast<int32>(FMath::Clamp(
                    CurrentProgress,
                    static_cast<int64>(0),
                    static_cast<int64>(MAX_int32)));
            }
        }
        SaveGameInstance->GameData.bCurrentMissionReadyToClaim = MissionMgr->IsReadyToClaim();
        SaveGameInstance->GameData.bTutorialCompleted = MissionMgr->IsTutorialCompleted();
    }

    // GoalBoard(미션판) 상태 수집
    if (UGoalBoardSubsystem* GoalMgr = GetGameInstance()->GetSubsystem<UGoalBoardSubsystem>())
    {
        GoalMgr->CollectSaveData(SaveGameInstance->GameData);
    }

    // 패널 최초 진입 코치마크 — 본 패널 목록 수집
    if (UPanelIntroSubsystem* IntroMgr = GetGameInstance()->GetSubsystem<UPanelIntroSubsystem>())
    {
        IntroMgr->CollectSaveData(SaveGameInstance->GameData);
    }

    // EntityManager에서 데이터 가져오기 (MainMap에서만 업데이트)
    if (UWorld* World = GetWorld())
    {
        FString CurrentLevelName = World->GetMapName();

        // MainMap에서만 Building 데이터 업데이트 (다른 레벨에서는 기존 데이터 유지)
        if (CurrentLevelName.Contains("MainMap") || CurrentLevelName.Contains("GameLevel"))
        {
            UEntityManager* EntityManager = World->GetSubsystem<UEntityManager>();
            if (EntityManager)
            {
                // EntityManager가 내부 데이터를 수집하도록 요청
                EntityManager->CollectEntityDataForSave();

                // 수집된 건물 데이터를 SaveGameInstance에 복사
                SaveGameInstance->GameData.Buildings = EntityManager->GetBuildingsData();

                // 건설 레벨 저장
                SaveGameInstance->GameData.UnlockedConstructionLevel = EntityManager->GetUnlockedConstructionLevel();

                UE_LOG(LogTemp, Log, TEXT("[SaveLoadManager] SaveGameData - Buildings updated in %s: %d, ConstructionLevel: %d"),
                    *CurrentLevelName, SaveGameInstance->GameData.Buildings.Num(),
                    SaveGameInstance->GameData.UnlockedConstructionLevel);
            }

            // Factory 데이터 수집
            SaveGameInstance->GameData.FactoryData.Empty();
            for (TActorIterator<ABrickFactory> It(World); It; ++It)
            {
                ABrickFactory* Factory = *It;
                if (Factory)
                {
                    FFactorySaveData FactoryData = Factory->GetFactoryData();
                    SaveGameInstance->GameData.FactoryData.Add(FactoryData.FactoryID, FactoryData);
                    UE_LOG(LogTemp, Log, TEXT("[SaveLoadManager] Saved Factory: %s"), *FactoryData.FactoryID.ToString());
                }
            }

            // 도시 부지 소유 직렬화 — SpawnedPlots 중 IsOwned() 인 것을 단일 소스로 수집한다.
            // (구매 부지 + bOwnedAtStart 부지 모두 SetOwnedState(true) 라 한 번에 채워진다.)
            // 부지가 아직 스폰되기 전(빈 SpawnedPlots)에는 디스크의 기존 OwnedPlotIds 를 덮어쓰지 않는다.
            if (USpawnManager* SpawnManager = World->GetSubsystem<USpawnManager>())
            {
                TArray<FName> GatheredOwnedPlots;
                SpawnManager->GatherOwnedPlotIds(GatheredOwnedPlots);
                if (GatheredOwnedPlots.Num() > 0)
                {
                    SaveGameInstance->GameData.OwnedPlotIds = GatheredOwnedPlots;
                    UE_LOG(LogTemp, Log, TEXT("[SaveLoadManager] SaveGameData - OwnedPlots: %d"),
                        SaveGameInstance->GameData.OwnedPlotIds.Num());
                }
            }
        }
        // OfficeMap에서만 Office 데이터 업데이트 (OfficeDataMap에 별도 저장)
        else if (CurrentLevelName.Contains("OfficeMap"))
        {
            // 현재 관리 중인 건물 Index 가져오기 (위에서 선언한 GameInst 재사용)
            int32 ManagedBuildingIndex = GameInst ? GameInst->GetCurrentManagedBuildingIndex() : -1;

            if (ManagedBuildingIndex < 0)
            {
                UE_LOG(LogTemp, Warning, TEXT("[SaveLoadManager] OfficeMap save skipped - No managed building index"));
            }
            else
            {
                // OfficeDataMap에서 해당 건물의 Office 데이터 가져오기 (없으면 새로 생성)
                FOfficeSaveData& OfficeData = SaveGameInstance->GameData.OfficeDataMap.FindOrAdd(ManagedBuildingIndex);

                // ApplyOfficeSaveData 완료 전엔 월드가 빈 기본값 → 인테리어 수집 시 디스크 데이터가 0으로 덮임
                // (재진입 시 결산 모달이 복원 전 SaveGameData 호출). 복원 전이면 인테리어 수집 생략
                UOfficeManager* OfficeMgr = World->GetSubsystem<UOfficeManager>();
                const bool bOfficeInteriorRestored = OfficeMgr && OfficeMgr->IsInteriorRestored();

                if (bOfficeInteriorRestored)
                {
                    // Office 확장/타일 데이터 수집
                    AOfficeInterior* OfficeInterior = Cast<AOfficeInterior>(
                        UGameplayStatics::GetActorOfClass(World, AOfficeInterior::StaticClass()));
                    if (OfficeInterior)
                    {
                        OfficeData.TileCountX = OfficeInterior->GetTileCount().X;
                        OfficeData.TileCountY = OfficeInterior->GetTileCount().Y;
                        OfficeData.CurrentFloorTileRowName = OfficeInterior->GetCurrentFloorTileRowName();
                    }

                    // Decoration + Workstation 데이터 수집 (OfficeManager에서 통합 관리)
                    OfficeMgr->FillOfficeSaveData(OfficeData);

                    // StageProgress 데이터 수집 (WorldSubsystem이므로 World에서 가져옴)
                    UOfficeStageProgressManager* StageMgr = World->GetSubsystem<UOfficeStageProgressManager>();
                    UE_LOG(LogTemp, Warning, TEXT("[SaveLoadManager] StageProgress Save - StageMgr: %s"), StageMgr ? TEXT("Valid") : TEXT("NULL"));
                    if (StageMgr)
                    {
                        OfficeData.StageProgress = StageMgr->GetProgressData();
                        UE_LOG(LogTemp, Warning, TEXT("[SaveLoadManager] Saved StageProgress: Step=%d"),
                            OfficeData.StageProgress.CurrentStep);
                    }
                    else
                    {
                        UE_LOG(LogTemp, Error, TEXT("[SaveLoadManager] StageProgress Save FAILED - StageMgr is NULL!"));
                    }

                    // Operation 데이터 수집
                    if (UProjectOperationManager* OpMgr = GameInst->GetSubsystem<UProjectOperationManager>())
                    {
                        FOperationData* Op = OpMgr->GetOperationByBuildingID(ManagedBuildingIndex);
                        if (Op)
                        {
                            OfficeData.CurrentOperation = *Op;
                            OfficeData.bHasActiveOperation = true;
                            UE_LOG(LogTemp, Log, TEXT("[SaveLoadManager] Saved Operation: %s, State=%d"),
                                *Op->ProjectName, (int32)Op->State);
                        }
                        else
                        {
                            OfficeData.bHasActiveOperation = false;
                        }
                    }
                }
                else
                {
                    UE_LOG(LogTemp, Warning, TEXT("[SaveLoadManager] OfficeMap save - 인테리어 복원 전이라 인테리어/데코/책상/스테이지 수집 건너뜀 (기존 데이터 보존). Building %d"),
                        ManagedBuildingIndex);
                }

                // 직원은 인테리어 복원과 무관하게 항상 수집 (채용·해고는 복원 전에도 발생). 건물별 저장
                if (EmployeeManager)
                {
                    OfficeData.EmployeeList = EmployeeManager->GetEmployeesByBuilding(ManagedBuildingIndex);
                    OfficeData.EmployeeAppearances = EmployeeManager->GetAppearancesByBuilding(ManagedBuildingIndex);
                    UE_LOG(LogTemp, Log, TEXT("[SaveLoadManager] Saved Employees for Building %d: Count=%d"),
                        ManagedBuildingIndex, OfficeData.EmployeeList.Num());
                }

                UE_LOG(LogTemp, Log, TEXT("[SaveLoadManager] SaveGameData - Office data for Building %d: TileX=%d, TileY=%d, Decorations=%d, Workstations=%d, Employees=%d"),
                    ManagedBuildingIndex,
                    OfficeData.TileCountX,
                    OfficeData.TileCountY,
                    OfficeData.PlacedDecorations.Num(),
                    OfficeData.OfficeWorkstations.Num(),
                    OfficeData.EmployeeList.Num());
            }
        }
        else
        {
            // 다른 레벨에서는 EntityManager를 건드리지 않음 (기존 데이터 자동 유지)
            UE_LOG(LogTemp, Log, TEXT("[SaveLoadManager] SaveGameData - Skipping building update in %s, preserving existing data: %d"),
                *CurrentLevelName, SaveGameInstance->GameData.Buildings.Num());
        }
    }

    // ProductionOrderManager 데이터 저장
    if (UProductionOrderManager* OrderMgr = GetGameInstance()->GetSubsystem<UProductionOrderManager>())
    {
        SaveGameInstance->GameData.ProductionOrders = OrderMgr->GetAllOrders();
        SaveGameInstance->GameData.NextProductionOrderID = OrderMgr->GetNextOrderID();
        UE_LOG(LogTemp, Log, TEXT("[SaveLoadManager] Saved %d production orders"),
            SaveGameInstance->GameData.ProductionOrders.Num());
    }

    // WorldMapManager 데이터 저장 (원자재/에너지/채광소/공장라인/완성품/큐)
    if (UWorldMapManager* WorldMapMgr = GetGameInstance()->GetSubsystem<UWorldMapManager>())
    {
        WorldMapMgr->SerializeForSave(
            SaveGameInstance->GameData.WorldMap_RawMaterialInventory,
            SaveGameInstance->GameData.WorldMap_EnergyCount,
            SaveGameInstance->GameData.WorldMap_RefinedOilCount,
            SaveGameInstance->GameData.WorldMap_Mines,
            SaveGameInstance->GameData.WorldMap_Lines,
            SaveGameInstance->GameData.WorldMap_AutoAssign,
            SaveGameInstance->GameData.WorldMap_PendingQueue,
            SaveGameInstance->GameData.WorldMap_ProductInventory,
            SaveGameInstance->GameData.WorldMap_ProductGrades,
            SaveGameInstance->GameData.WorldMap_LastSaveUtc);
        UE_LOG(LogTemp, Log, TEXT("[SaveLoadManager] Saved WorldMap: Mines=%d, Lines=%d, Products=%d"),
            SaveGameInstance->GameData.WorldMap_Mines.Num(),
            SaveGameInstance->GameData.WorldMap_Lines.Num(),
            SaveGameInstance->GameData.WorldMap_ProductInventory.Num());
    }

    // CountryMarketManager 데이터 저장 (시장 수요 게이지)
    if (UCountryMarketManager* MarketMgr = GetGameInstance()->GetSubsystem<UCountryMarketManager>())
    {
        SaveGameInstance->GameData.WorldMap_CountryMarketStates = MarketMgr->GetAllStates();
        UE_LOG(LogTemp, Log, TEXT("[SaveLoadManager] Saved CountryMarketStates: %d cells"),
            SaveGameInstance->GameData.WorldMap_CountryMarketStates.Num());
    }

    // MineManager (신규 채광 시스템) 데이터 저장 — catchup 은 PlayFab 권위
    if (UMineManager* MineMgr = GetGameInstance()->GetSubsystem<UMineManager>())
    {
        MineMgr->SerializeForSave(SaveGameInstance->GameData.WorldMap_MineData);
        UE_LOG(LogTemp, Log, TEXT("[SaveLoadManager] Saved Mine: %d countries"),
            SaveGameInstance->GameData.WorldMap_MineData.Num());
    }

    // WorldFactoryManager 데이터 저장 (LineElapsedSec 누적 모델 — 자체 LastTick 없음, catchup 은 PlayFab 권위)
    if (UWorldFactoryManager* FactoryMgr = GetGameInstance()->GetSubsystem<UWorldFactoryManager>())
    {
        FactoryMgr->SerializeForSave(SaveGameInstance->GameData.WorldMap_FactoryData);
        int32 TotalLines = 0;
        for (const auto& Pair : SaveGameInstance->GameData.WorldMap_FactoryData)
        {
            TotalLines += Pair.Value.ActiveLines.Num();
        }
        UE_LOG(LogTemp, Log, TEXT("[SaveLoadManager] Saved WorldFactory: %d countries, %d lines"),
            SaveGameInstance->GameData.WorldMap_FactoryData.Num(), TotalLines);
    }

    // TradeOrderManager 데이터 저장 (활성 주문서)
    if (UTradeOrderManager* TradeMgr = GetGameInstance()->GetSubsystem<UTradeOrderManager>())
    {
        TradeMgr->SerializeForSave(
            SaveGameInstance->GameData.TradeOrders_Active,
            SaveGameInstance->GameData.TradeOrders_NextOrderId,
            SaveGameInstance->GameData.TradeOrders_ComboCount);
        UE_LOG(LogTemp, Log, TEXT("[SaveLoadManager] Saved TradeOrders: %d active"),
            SaveGameInstance->GameData.TradeOrders_Active.Num());
    }

    // [Perf] DebugPrintSaveData() 제거 — 매 세이브마다 디스크에서 전체 세이브를 되읽어(LoadGameFromSlot)
    // 직원별 로그까지 찍던 비용. 세이브는 WorldMap에서 최대 ~1Hz라 게임스레드 블로킹 I/O를 두 배로 냈음.
    bool bSuccess = UGameplayStatics::SaveGameToSlot(SaveGameInstance, SaveSlotName, 0);

    // 저장 성공 시 캐시 유지 (다른 시스템이 수정한 데이터 보존)
    if (bSuccess)
    {
        CachedSaveData = SaveGameInstance;
        UE_LOG(LogTemp, Warning, TEXT("[SaveLoadManager] Game saved successfully!"));

        // 방금 전체 저장했으므로 대기 중인 지연 저장은 불필요 — 해제해 중복 디스크 쓰기 방지
        if (UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr)
        {
            World->GetTimerManager().ClearTimer(DeferredSaveTimerHandle);
        }

        // 도시 스냅샷만 업로드 (쓰로틀 적용, 60초 이내 중복 무시)
        // 매출 점수는 랭킹 패널 진입 시에만 업로드
        if (URankingManagerSubsystem* RankingMgr = GetGameInstance()->GetSubsystem<URankingManagerSubsystem>())
        {
            RankingMgr->UploadCitySnapshot();
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("[SaveLoadManager] Failed to save game!"));
    }

    return bSuccess;
}

void USaveLoadManager::RequestDeferredSave()
{
    UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
    if (!World)
    {
        return;
    }

    // 스로틀: 이미 대기 중이면 유지 — 디바운스(매번 리셋)면 홀드 연타 중 저장이 무한 연기됨
    FTimerManager& TimerMgr = World->GetTimerManager();
    if (TimerMgr.IsTimerActive(DeferredSaveTimerHandle))
    {
        return;
    }

    TimerMgr.SetTimer(DeferredSaveTimerHandle, this, &USaveLoadManager::OnDeferredSaveTimer, DeferredSaveDelaySeconds, false);
}

void USaveLoadManager::OnDeferredSaveTimer()
{
    SaveGameData();
}

void USaveLoadManager::WipeAllSaveData()
{
    const bool bSlotDeleted = UGameplayStatics::DeleteGameInSlot(SaveSlotName, 0);

    // 초상화는 세이브와 같은 EmployeeID 공간을 쓴다 — 한쪽만 지우면 얼굴이 다음 플레이어에게 상속된다.
    const FString PortraitDir = FPaths::ProjectSavedDir() / TEXT("Portraits");
    const bool bPortraitsDeleted = IFileManager::Get().DeleteDirectory(*PortraitDir, false, true);

    UE_LOG(LogTemp, Warning, TEXT("[SaveLoadManager] 세이브 전체 삭제 — 슬롯:%s 초상화:%s (%s)"),
        bSlotDeleted ? TEXT("삭제됨") : TEXT("없음"),
        bPortraitsDeleted ? TEXT("삭제됨") : TEXT("없음"),
        *PortraitDir);
}

bool USaveLoadManager::LoadGameData()
{
    if (!UGameplayStatics::DoesSaveGameExist(SaveSlotName, 0))
    {
        // 저장 없는 첫 실행도 OnGameDataLoaded broadcast — Dev 빌드 자동 시나리오 적용
        // (ResourceItemManager.HandleGameDataLoaded 가 Rich/Default 등 즉시 세팅하도록).
        UE_LOG(LogTemp, Log, TEXT("[SaveLoadManager] No save file - broadcasting OnGameDataLoaded with empty state"));
        OnGameDataLoaded.Broadcast();
        // 신규 게임 — 복원 브로드캐스트(미션 M1 시작 등) 이후부터 저장 허용
        bInitialLoadComplete = true;
        return false;
    }

    USaveGame_GameData* LoadGameInstance = Cast<USaveGame_GameData>(
        UGameplayStatics::LoadGameFromSlot(SaveSlotName, 0));

    if (!LoadGameInstance)
    {
        // 로드 실패도 broadcast (자동 시나리오 보장)
        OnGameDataLoaded.Broadcast();
        // 로드 실패 = 빈 상태로 진행 — 이후 저장 허용
        bInitialLoadComplete = true;
        return false;
    }

    // EmployeeManager에 NextEmployeeID만 복원 (전역 관리)
    // 직원 목록은 건물별로 FOfficeSaveData에서 로드됨
    UEmployeeManager* EmployeeManager = GetGameInstance()->GetSubsystem<UEmployeeManager>();
    if (EmployeeManager)
    {
        EmployeeManager->SetNextInstanceID(LoadGameInstance->GameData.NextEmployeeID);

        // 마이그레이션: 기존 전역 EmployeeList가 있고 OfficeDataMap의 직원이 비어있으면 마이그레이션
        if (LoadGameInstance->GameData.EmployeeList.Num() > 0)
        {
            bool bNeedsMigration = true;
            for (const auto& Pair : LoadGameInstance->GameData.OfficeDataMap)
            {
                if (Pair.Value.EmployeeList.Num() > 0)
                {
                    bNeedsMigration = false;
                    break;
                }
            }

            if (bNeedsMigration)
            {
                UE_LOG(LogTemp, Warning, TEXT("[SaveLoadManager] Migrating %d employees from global list to building-based storage"),
                    LoadGameInstance->GameData.EmployeeList.Num());

                // 직원들을 해당 건물에 배정
                for (const FEmployeeInstance& Employee : LoadGameInstance->GameData.EmployeeList)
                {
                    int32 BuildingIdx = Employee.AssignedBuildingIndex;
                    if (BuildingIdx >= 0)
                    {
                        FOfficeSaveData& OfficeData = LoadGameInstance->GameData.OfficeDataMap.FindOrAdd(BuildingIdx);
                        OfficeData.EmployeeList.Add(Employee);

                        // 외모 데이터도 마이그레이션
                        if (const FEmployeeAppearanceData* AppData = LoadGameInstance->GameData.EmployeeAppearances.Find(Employee.EmployeeID))
                        {
                            OfficeData.EmployeeAppearances.Add(Employee.EmployeeID, AppData->Appearance);
                        }
                    }
                }

                // 마이그레이션 후 전역 리스트 비우기 (다음 저장 시 정리됨)
                LoadGameInstance->GameData.EmployeeList.Empty();
                LoadGameInstance->GameData.EmployeeAppearances.Empty();

                UE_LOG(LogTemp, Warning, TEXT("[SaveLoadManager] Migration completed"));
            }
        }

        // NextEmployeeID 무결성 검증: OfficeDataMap 전체의 최대 ID와 대조
        int32 MaxEmployeeID = 0;
        for (const auto& Pair : LoadGameInstance->GameData.OfficeDataMap)
        {
            for (const FEmployeeInstance& Emp : Pair.Value.EmployeeList)
            {
                if (Emp.EmployeeID > MaxEmployeeID)
                {
                    MaxEmployeeID = Emp.EmployeeID;
                }
            }
        }

        int32 SavedNextID = LoadGameInstance->GameData.NextEmployeeID;
        int32 RequiredNextID = MaxEmployeeID + 1;
        if (RequiredNextID > SavedNextID)
        {
            EmployeeManager->SetNextInstanceID(RequiredNextID);
            UE_LOG(LogTemp, Warning,
                TEXT("[SaveLoadManager] NextEmployeeID corrected: saved %d -> %d (max existing ID: %d)"),
                SavedNextID, RequiredNextID, MaxEmployeeID);
        }
    }

    // ResourceItemManager에 데이터 설정
    UResourceItemManager* ResourceManager = GetGameInstance()->GetSubsystem<UResourceItemManager>();
    if (ResourceManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("[SaveLoadManager] ResourceBank in save: Num=%d"),
            LoadGameInstance->GameData.ResourceBank.Num());

        // ResourceBank가 비어있지 않을 때만 로드 (비어있으면 초기값 유지)
        if (LoadGameInstance->GameData.ResourceBank.Num() > 0)
        {
            for (const auto& Pair : LoadGameInstance->GameData.ResourceBank)
            {
                UE_LOG(LogTemp, Warning, TEXT("[SaveLoadManager] Loading Resource: Type=%d, Amount=%d"),
                    (int32)Pair.Key, Pair.Value);
            }
            ResourceManager->SetAllResources(LoadGameInstance->GameData.ResourceBank);
            UE_LOG(LogTemp, Log, TEXT("[SaveLoadManager] Loaded %d resources from save"),
                LoadGameInstance->GameData.ResourceBank.Num());
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[SaveLoadManager] ResourceBank is empty in save file, keeping initial values"));
        }
    }

    // NextBuildingIndex 복원 (CGGameInstance에 설정)
    UCGGameInstance* GameInst = Cast<UCGGameInstance>(GetGameInstance());
    if (GameInst)
    {
        GameInst->SetNextBuildingIndex(LoadGameInstance->GameData.NextBuildingIndex);
        UE_LOG(LogTemp, Log, TEXT("[SaveLoadManager] Loaded NextBuildingIndex: %d"),
            LoadGameInstance->GameData.NextBuildingIndex);
    }

    // ItemInventoryManager에 아이템 데이터 설정
    UItemInventoryManager* ItemInvMgr = GetGameInstance()->GetSubsystem<UItemInventoryManager>();
    if (ItemInvMgr && LoadGameInstance->GameData.ItemInventory.Num() > 0)
    {
        ItemInvMgr->SetAllItems(LoadGameInstance->GameData.ItemInventory);
        UE_LOG(LogTemp, Log, TEXT("[SaveLoadManager] Loaded ItemInventory: %d types"),
            LoadGameInstance->GameData.ItemInventory.Num());
    }

    // RecruitmentManagerSubsystem에 오피스 채용 + 가챠 데이터 설정
    URecruitmentManagerSubsystem* RecruitmentMgr = GetGameInstance()->GetSubsystem<URecruitmentManagerSubsystem>();
    if (RecruitmentMgr)
    {
        if (LoadGameInstance->GameData.OfficeRecruitmentMap.Num() > 0)
        {
            RecruitmentMgr->SetOfficeRecruitmentMap(LoadGameInstance->GameData.OfficeRecruitmentMap);
            UE_LOG(LogTemp, Log, TEXT("[SaveLoadManager] Loaded OfficeRecruitmentMap: %d entries"),
                LoadGameInstance->GameData.OfficeRecruitmentMap.Num());
        }

        RecruitmentMgr->SetGachaData(LoadGameInstance->GameData.GachaRecruitmentData);
        UE_LOG(LogTemp, Log, TEXT("[SaveLoadManager] Loaded GachaData: AdvPity=%d, PremPity=%d, Mileage=%d"),
            LoadGameInstance->GameData.GachaRecruitmentData.AdvancedPity.PullsSinceLastGuaranteed,
            LoadGameInstance->GameData.GachaRecruitmentData.PremiumPity.PullsSinceLastGuaranteed,
            LoadGameInstance->GameData.GachaRecruitmentData.Mileage.CurrentPoints);
    }

    // ShopManagerSubsystem에 상점 구매 상태 설정 (평구조체 — 무조건 복원)
    if (UShopManagerSubsystem* ShopMgr = GetGameInstance()->GetSubsystem<UShopManagerSubsystem>())
    {
        ShopMgr->SetShopData(LoadGameInstance->GameData.ShopSaveData);
    }

    // BuildingTraitManager에 글로벌 특성 인벤토리 설정
    UBuildingTraitManagerSubsystem* TraitMgr = GetGameInstance()->GetSubsystem<UBuildingTraitManagerSubsystem>();
    if (TraitMgr)
    {
        TraitMgr->SetInventory(LoadGameInstance->GameData.BuildingTraitInventory);
        UE_LOG(LogTemp, Log, TEXT("[SaveLoadManager] Loaded BuildingTraitInventory: %d traits owned, %d encyclopedia, dust=%d, mileage=%d"),
            LoadGameInstance->GameData.BuildingTraitInventory.OwnedTraits.Num(),
            LoadGameInstance->GameData.BuildingTraitInventory.EncyclopediaUnlocked.Num(),
            LoadGameInstance->GameData.BuildingTraitInventory.DismantleDust,
            LoadGameInstance->GameData.BuildingTraitInventory.GachaData.MileagePoints);
    }

    // BuildingSkinManager 에 스킨 가챠 천장/마일리지 설정
    UBuildingSkinManagerSubsystem* SkinMgr = GetGameInstance()->GetSubsystem<UBuildingSkinManagerSubsystem>();
    if (SkinMgr)
    {
        SkinMgr->SetInventory(LoadGameInstance->GameData.BuildingSkinGachaInventory);
    }

    // EntityManager에 데이터 설정 (MainMap에서만!)
    if (UWorld* World = GetWorld())
    {
        FString CurrentLevelName = World->GetMapName();

        // MainMap/GameLevel에서만 Entity 데이터 복원
        if (CurrentLevelName.Contains("MainMap") || CurrentLevelName.Contains("GameLevel"))
        {
            UEntityManager* EntityManager = World->GetSubsystem<UEntityManager>();
            if (EntityManager)
            {
                // 로드된 건물 데이터를 EntityManager에 설정
                EntityManager->SetBuildingsData(LoadGameInstance->GameData.Buildings);

                // 건설 레벨 복원
                EntityManager->SetUnlockedConstructionLevel(LoadGameInstance->GameData.UnlockedConstructionLevel);

                // EntityManager가 데이터 복원 처리
                EntityManager->RestoreEntityDataFromLoad();

                UE_LOG(LogTemp, Log, TEXT("[SaveLoadManager] LoadGameData - Buildings: %d, ConstructionLevel: %d"),
                    LoadGameInstance->GameData.Buildings.Num(),
                    LoadGameInstance->GameData.UnlockedConstructionLevel);
            }

            // 도시 부지 소유 복원 — 부지 액터는 OnWorldBeginPlay 에서 이미 스폰됨(SpawnManager).
            // 로드된 OwnedPlotIds 로 소유 상태만 갱신한다(건물 복원과 동일 타이밍/맵 게이트).
            if (USpawnManager* SpawnManager = World->GetSubsystem<USpawnManager>())
            {
                SpawnManager->RestorePlotOwnership(LoadGameInstance->GameData.OwnedPlotIds);
                UE_LOG(LogTemp, Log, TEXT("[SaveLoadManager] LoadGameData - OwnedPlots: %d"),
                    LoadGameInstance->GameData.OwnedPlotIds.Num());

                // 부지 가격뷰는 온디맨드(미소유 부지 탭 시에만) — 소유 복원 시점에는 띄울 칩이 없다.
            }

            // MainMap에서도 모든 건물의 직원 데이터를 EmployeeManager에 로드
            if (EmployeeManager)
            {
                for (const auto& Pair : LoadGameInstance->GameData.OfficeDataMap)
                {
                    int32 BIdx = Pair.Key;
                    const FOfficeSaveData& OD = Pair.Value;
                    if (OD.EmployeeList.Num() > 0)
                    {
                        EmployeeManager->SetEmployeesForBuilding(BIdx, OD.EmployeeList);
                        EmployeeManager->SetAppearancesForBuilding(BIdx, OD.EmployeeAppearances);
                    }
                }
                UE_LOG(LogTemp, Log, TEXT("[SaveLoadManager] Loaded employees for all buildings on MainMap"));
            }

            // Factory 데이터 복원 (MainMap에서만)
            for (TActorIterator<ABrickFactory> It(World); It; ++It)
            {
                ABrickFactory* Factory = *It;
                if (Factory)
                {
                    // Factory ID를 키로 저장된 데이터 찾기
                    FFactorySaveData* FoundData = LoadGameInstance->GameData.FactoryData.Find(Factory->GetFactoryData().FactoryID);
                    if (FoundData)
                    {
                        Factory->SetFactoryData(*FoundData);
                        UE_LOG(LogTemp, Log, TEXT("[SaveLoadManager] Loaded Factory: %s"), *FoundData->FactoryID.ToString());
                    }
                }
            }
        }
        else
        {
            UE_LOG(LogTemp, Log, TEXT("[SaveLoadManager] Skipping Entity restore in %s (not MainMap)"), *CurrentLevelName);
        }

        // OfficeMap에서 Office 데이터 복원
        if (CurrentLevelName.Contains("OfficeMap"))
        {
            // 현재 관리 중인 건물 Index 가져오기 (위에서 선언한 GameInst 재사용)
            int32 ManagedBuildingIndex = GameInst ? GameInst->GetCurrentManagedBuildingIndex() : -1;

            UE_LOG(LogTemp, Warning, TEXT("[SaveLoadManager] OfficeMap detected - ManagedBuildingIndex: %d"), ManagedBuildingIndex);
            UE_LOG(LogTemp, Warning, TEXT("[SaveLoadManager] OfficeDataMap count: %d"), LoadGameInstance->GameData.OfficeDataMap.Num());

            if (ManagedBuildingIndex < 0)
            {
                UE_LOG(LogTemp, Warning, TEXT("[SaveLoadManager] OfficeMap load skipped - No managed building index"));
            }
            else
            {
                // OfficeDataMap에서 해당 건물의 Office 데이터 찾기
                FOfficeSaveData* OfficeData = LoadGameInstance->GameData.OfficeDataMap.Find(ManagedBuildingIndex);

                if (OfficeData)
                {
                    // 스타터 프리셋 2단계: Pending이면 데코/바닥타일 데이터를 부풀린 뒤 아래 기존 복원기에 맡김
                    if (!OfficeData->PendingStarterPreset.IsNone())
                    {
                        ECompanyType BuildingCompanyType = ECompanyType::None;
                        for (const FBuildingEntitySaveData& Building : LoadGameInstance->GameData.Buildings)
                        {
                            if (Building.BuildingIndex == ManagedBuildingIndex)
                            {
                                BuildingCompanyType = Building.BuildingData.CompanyType;
                                break;
                            }
                        }
                        if (UOfficeManager* PresetMgr = World->GetSubsystem<UOfficeManager>())
                        {
                            PresetMgr->ApplyStarterPresetToData(*OfficeData, BuildingCompanyType);
                        }
                    }

                    UE_LOG(LogTemp, Warning, TEXT("[SaveLoadManager] Found OfficeData for Building %d: TileX=%d, TileY=%d"),
                        ManagedBuildingIndex, OfficeData->TileCountX, OfficeData->TileCountY);

                    // Office 확장/타일 데이터 복원
                    AOfficeInterior* OfficeInterior = Cast<AOfficeInterior>(
                        UGameplayStatics::GetActorOfClass(World, AOfficeInterior::StaticClass()));
                    if (OfficeInterior)
                    {
                        UE_LOG(LogTemp, Warning, TEXT("[SaveLoadManager] OfficeInterior found, calling ApplyOfficeSaveData..."));
                        OfficeInterior->ApplyOfficeSaveData(*OfficeData);
                        UE_LOG(LogTemp, Warning, TEXT("[SaveLoadManager] ApplyOfficeSaveData completed"));
                    }
                    else
                    {
                        UE_LOG(LogTemp, Error, TEXT("[SaveLoadManager] OfficeInterior NOT FOUND in level!"));
                    }

                    // Decoration + Workstation 데이터 복원 (OfficeManager에서 통합 관리)
                    if (UOfficeManager* OfficeMgr = World->GetSubsystem<UOfficeManager>())
                    {
                        OfficeMgr->ApplyOfficeSaveData(*OfficeData);
                    }

                    // StageProgress 데이터 복원 (WorldSubsystem이므로 World에서 가져옴)
                    // bStageInProgress는 저장하지 않음 - 로드 후 사용자가 수동으로 시작
                    if (UOfficeStageProgressManager* StageMgr = World->GetSubsystem<UOfficeStageProgressManager>())
                    {
                        StageMgr->SetProgressData(OfficeData->StageProgress);
                        UE_LOG(LogTemp, Log, TEXT("[SaveLoadManager] Loaded StageProgress: Step=%d"),
                            OfficeData->StageProgress.CurrentStep);
                    }

                    // 직원 데이터 복원 (건물별 로드)
                    if (EmployeeManager)
                    {
                        EmployeeManager->SetEmployeesForBuilding(ManagedBuildingIndex, OfficeData->EmployeeList);
                        EmployeeManager->SetAppearancesForBuilding(ManagedBuildingIndex, OfficeData->EmployeeAppearances);
                        UE_LOG(LogTemp, Log, TEXT("[SaveLoadManager] Loaded Employees for Building %d: Count=%d"),
                            ManagedBuildingIndex, OfficeData->EmployeeList.Num());
                    }

                    UE_LOG(LogTemp, Log, TEXT("[SaveLoadManager] LoadGameData - Office data for Building %d: TileX=%d, TileY=%d, Decorations=%d, Workstations=%d, Employees=%d"),
                        ManagedBuildingIndex,
                        OfficeData->TileCountX,
                        OfficeData->TileCountY,
                        OfficeData->PlacedDecorations.Num(),
                        OfficeData->OfficeWorkstations.Num(),
                        OfficeData->EmployeeList.Num());
                }
                else
                {
                    UE_LOG(LogTemp, Warning, TEXT("[SaveLoadManager] OfficeMap load - No OfficeData found for Building %d, resetting managers"), ManagedBuildingIndex);

                    // 새 건물이므로 StageProgressManager 초기화 (WorldSubsystem이므로 World에서 가져옴)
                    if (UOfficeStageProgressManager* StageMgr = World->GetSubsystem<UOfficeStageProgressManager>())
                    {
                        StageMgr->SetProgressData(FStageProgressData());
                        UE_LOG(LogTemp, Log, TEXT("[SaveLoadManager] Reset StageProgress for new building"));
                    }

                    // OfficeManager 초기화 — 프리셋 도입 후 신규 빌딩은 항상 OfficeData가 있으므로 이 분기는 구 세이브 전용
                    if (UOfficeManager* OfficeMgr = World->GetSubsystem<UOfficeManager>())
                    {
                        OfficeMgr->ApplyOfficeSaveData(FOfficeSaveData());
                        OfficeMgr->SpawnDefaultDecorations();
                    }

                    // 직원 목록 초기화 (새 건물)
                    if (EmployeeManager)
                    {
                        EmployeeManager->ClearEmployeesForBuilding(ManagedBuildingIndex);
                        UE_LOG(LogTemp, Log, TEXT("[SaveLoadManager] Cleared Employees for new Building %d"), ManagedBuildingIndex);
                    }
                }
            }
        }
    }

    // 로드 완료 이벤트 발생
    UE_LOG(LogTemp, Warning, TEXT("[SaveLoadManager] Broadcasting OnGameDataLoaded event"));
    OnGameDataLoaded.Broadcast();

    DebugPrintSaveData();

    // 캐시 설정 (로드한 데이터 재사용)
    CachedSaveData = LoadGameInstance;
    UE_LOG(LogTemp, Log, TEXT("[SaveLoadManager] Save data cached after load"));

    // 로드 + 복원 브로드캐스트 완료 — 이 시점부터 저장 허용. (브로드캐스트 도중 핸들러가 저장을 시도해도
    // 위 게이트가 막아, 미션 복원 전 None 값이 디스크에 써지는 것을 차단한다.)
    bInitialLoadComplete = true;

    // 본사 레벨 마이그레이션 (기존 세이브 호환)
    if (CachedSaveData && CachedSaveData->GameData.HQLevel < 1)
    {
        CachedSaveData->GameData.HQLevel = 1;
        UE_LOG(LogTemp, Warning, TEXT("[SaveLoadManager] HQLevel migrated to 1"));
    }

    // Operation 복원 (이미 메모리에 있으면 스킵됨 - 레벨 전환 시 중복 로드 방지)
    if (UProjectOperationManager* OpMgr = GameInst->GetSubsystem<UProjectOperationManager>())
    {
        OpMgr->LoadAllOperationsFromSave();
        // PendingReport 는 OfficeDataMap 이 곧 런타임 저장소 — 별도 복원 불필요
    }

    // ProductionOrder 복원
    if (UProductionOrderManager* OrderMgr = GameInst->GetSubsystem<UProductionOrderManager>())
    {
        OrderMgr->LoadOrdersFromSave();
    }

    // WorldMapManager 복원 (원자재/채광소/공장라인/완성품 + 오프라인 catchup)
    if (UWorldMapManager* WorldMapMgr = GameInst->GetSubsystem<UWorldMapManager>())
    {
        WorldMapMgr->LoadFromSave(
            LoadGameInstance->GameData.WorldMap_RawMaterialInventory,
            LoadGameInstance->GameData.WorldMap_EnergyCount,
            LoadGameInstance->GameData.WorldMap_RefinedOilCount,
            LoadGameInstance->GameData.WorldMap_Mines,
            LoadGameInstance->GameData.WorldMap_Lines,
            LoadGameInstance->GameData.WorldMap_AutoAssign,
            LoadGameInstance->GameData.WorldMap_PendingQueue,
            LoadGameInstance->GameData.WorldMap_ProductInventory,
            LoadGameInstance->GameData.WorldMap_ProductGrades,
            LoadGameInstance->GameData.WorldMap_LastSaveUtc);
        UE_LOG(LogTemp, Log, TEXT("[SaveLoadManager] Loaded WorldMap: Mines=%d, Lines=%d, Products=%d"),
            LoadGameInstance->GameData.WorldMap_Mines.Num(),
            LoadGameInstance->GameData.WorldMap_Lines.Num(),
            LoadGameInstance->GameData.WorldMap_ProductInventory.Num());
    }

    // CountryMarketManager 복원 (수요 게이지 + 오프라인 회복 시뮬)
    if (UCountryMarketManager* MarketMgr = GameInst->GetSubsystem<UCountryMarketManager>())
    {
        MarketMgr->LoadStates(
            LoadGameInstance->GameData.WorldMap_CountryMarketStates,
            LoadGameInstance->GameData.WorldMap_LastSaveUtc);
        UE_LOG(LogTemp, Log, TEXT("[SaveLoadManager] Loaded CountryMarketStates: %d cells"),
            LoadGameInstance->GameData.WorldMap_CountryMarketStates.Num());
    }

    // MineManager (신규 채광 시스템) 복원 — 오프라인 catchup 은 PlayFab OnOfflineGainsRequested 가 별도로 트리거
    if (UMineManager* MineMgr = GameInst->GetSubsystem<UMineManager>())
    {
        MineMgr->LoadFromSave(LoadGameInstance->GameData.WorldMap_MineData);
        UE_LOG(LogTemp, Log, TEXT("[SaveLoadManager] Loaded Mine: %d countries"),
            LoadGameInstance->GameData.WorldMap_MineData.Num());
    }

    // WorldFactoryManager 복원 — 오프라인 catchup 은 PlayFab OnOfflineGainsRequested 가 별도로 트리거 (12h 캡 + 0.2x 효율)
    if (UWorldFactoryManager* FactoryMgr = GameInst->GetSubsystem<UWorldFactoryManager>())
    {
        FactoryMgr->LoadFromSave(LoadGameInstance->GameData.WorldMap_FactoryData);
        int32 TotalLines = 0;
        for (const auto& Pair : LoadGameInstance->GameData.WorldMap_FactoryData)
        {
            TotalLines += Pair.Value.ActiveLines.Num();
        }
        UE_LOG(LogTemp, Log, TEXT("[SaveLoadManager] Loaded WorldFactory: %d countries, %d lines"),
            LoadGameInstance->GameData.WorldMap_FactoryData.Num(), TotalLines);
    }

    // TradeOrderManager 복원
    if (UTradeOrderManager* TradeMgr = GameInst->GetSubsystem<UTradeOrderManager>())
    {
        TradeMgr->LoadFromSave(
            LoadGameInstance->GameData.TradeOrders_Active,
            LoadGameInstance->GameData.TradeOrders_NextOrderId,
            LoadGameInstance->GameData.TradeOrders_ComboCount);
        UE_LOG(LogTemp, Log, TEXT("[SaveLoadManager] Loaded TradeOrders: %d active"),
            LoadGameInstance->GameData.TradeOrders_Active.Num());
    }

    return true;
}

USaveGame_GameData* USaveLoadManager::GetCurrentSaveData()
{
    // 캐시가 없으면 로드
    if (!CachedSaveData)
    {
        if (UGameplayStatics::DoesSaveGameExist(SaveSlotName, 0))
        {
            CachedSaveData = Cast<USaveGame_GameData>(
                UGameplayStatics::LoadGameFromSlot(SaveSlotName, 0));

            if (CachedSaveData)
            {
                UE_LOG(LogTemp, Log, TEXT("[SaveLoadManager] Save data loaded and cached"));
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[SaveLoadManager] No save file exists"));
        }
    }

    return CachedSaveData;
}

void USaveLoadManager::InvalidateCache()
{
    CachedSaveData = nullptr;
    UE_LOG(LogTemp, Log, TEXT("[SaveLoadManager] Cache invalidated"));
}

// ========== 건물 EXP/레벨 시스템 ==========

FBuildingSaveData* USaveLoadManager::FindBuildingSaveData(int32 BuildingIndex)
{
    USaveGame_GameData* SaveData = GetCurrentSaveData();
    if (!SaveData)
    {
        return nullptr;
    }

    for (FBuildingEntitySaveData& BuildingSave : SaveData->GameData.Buildings)
    {
        if (BuildingSave.BuildingIndex == BuildingIndex)
        {
            return &BuildingSave.BuildingData;
        }
    }

    return nullptr;
}

int32 USaveLoadManager::GetBuildingTier(int32 BuildingIndex)
{
    USaveGame_GameData* SaveData = GetCurrentSaveData();
    if (!SaveData) { return 1; }

    // 오피스에 한 번도 진입하지 않은 신축 빌딩은 OfficeDataMap 에 엔트리가 없다.
    // 0 을 주면 CountBuildingsAtTier(1) 같은 조건에서 신축 빌딩이 통째로 누락된다.
    const FOfficeSaveData* Office = SaveData->GameData.OfficeDataMap.Find(BuildingIndex);
    return Office ? FMath::Max(1, Office->TierProgress.CurrentTier) : 1;
}

int32 USaveLoadManager::GetMaxBuildingTier()
{
    USaveGame_GameData* SaveData = GetCurrentSaveData();
    if (!SaveData) { return 0; }

    int32 MaxTier = 0;
    for (const FBuildingEntitySaveData& Building : SaveData->GameData.Buildings)
    {
        MaxTier = FMath::Max(MaxTier, GetBuildingTier(Building.BuildingIndex));
    }
    return MaxTier;
}

int32 USaveLoadManager::CountBuildingsAtTier(int32 MinTier)
{
    USaveGame_GameData* SaveData = GetCurrentSaveData();
    if (!SaveData) { return 0; }

    int32 Count = 0;
    for (const FBuildingEntitySaveData& Building : SaveData->GameData.Buildings)
    {
        if (GetBuildingTier(Building.BuildingIndex) >= MinTier)
        {
            Count++;
        }
    }
    return Count;
}

ECompanyType USaveLoadManager::GetBuildingCompanyType(int32 BuildingIndex)
{
    const FBuildingSaveData* Building = FindBuildingSaveData(BuildingIndex);
    return Building ? Building->CompanyType : ECompanyType::None;
}

// ========== 본사 레벨 시스템 ==========

int32 USaveLoadManager::GetHQLevel()
{
    USaveGame_GameData* SaveData = GetCurrentSaveData();
    if (!SaveData || SaveData->GameData.HQLevel < 1)
    {
        return 1;
    }
    return SaveData->GameData.HQLevel;
}

bool USaveLoadManager::CanLevelUpHQ()
{
    USaveGame_GameData* SaveData = GetCurrentSaveData();
    if (!SaveData) return false;

    UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
    if (!TableMgr) return false;

    int32 CurrentLevel = SaveData->GameData.HQLevel;
    int32 NextLevel = CurrentLevel + 1;

    bool bSuccess = false;
    FHQLevelData NextLevelData = TableMgr->GetHQLevelData(NextLevel, bSuccess);
    if (!bSuccess) return false;

    // 빌딩 수 조건
    if (NextLevelData.RequiredBuildingCount > 0)
    {
        int32 BuildingCount = SaveData->GameData.Buildings.Num();
        if (BuildingCount < NextLevelData.RequiredBuildingCount)
        {
            return false;
        }
    }

    // 티어 조건 (RequiredTier 이상인 빌딩이 RequiredTierBuildingCount 개 이상)
    if (NextLevelData.RequiredTier > 0 && NextLevelData.RequiredTierBuildingCount > 0)
    {
        if (CountBuildingsAtTier(NextLevelData.RequiredTier) < NextLevelData.RequiredTierBuildingCount)
        {
            return false;
        }
    }

    // 직원 수 조건 (전체 빌딩 합산)
    if (NextLevelData.RequiredEmployeeCount > 0)
    {
        int32 TotalEmployees = 0;
        for (const auto& Pair : SaveData->GameData.OfficeDataMap)
        {
            TotalEmployees += Pair.Value.EmployeeList.Num();
        }
        if (TotalEmployees < NextLevelData.RequiredEmployeeCount)
        {
            return false;
        }
    }

    // 시총 조건 (재는 것 — 소비하지 않음)
    if (NextLevelData.RequiredMarketCap > 0)
    {
        UResourceItemManager* ResourceMgr = GetGameInstance()->GetSubsystem<UResourceItemManager>();
        if (!ResourceMgr) return false;

        if (ResourceMgr->GetResourceAmount(EResourceType::MarketCap) < NextLevelData.RequiredMarketCap)
        {
            return false;
        }
    }

    // Money 비용 확인 (소비하지 않고 보유 여부만 체크)
    if (NextLevelData.MoneyCost > 0)
    {
        UResourceItemManager* ResourceMgr = GetGameInstance()->GetSubsystem<UResourceItemManager>();
        if (!ResourceMgr) return false;

        int64 CurrentMoney = ResourceMgr->GetResourceAmount(EResourceType::Money);
        if (CurrentMoney < NextLevelData.MoneyCost)
        {
            return false;
        }
    }

    return true;
}

bool USaveLoadManager::TryLevelUpHQ()
{
    if (!CanLevelUpHQ())
    {
        UE_LOG(LogTemp, Warning, TEXT("[SaveLoadManager] HQ LevelUp failed: conditions not met"));
        return false;
    }

    USaveGame_GameData* SaveData = GetCurrentSaveData();
    if (!SaveData) return false;

    UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
    if (!TableMgr) return false;

    int32 NextLevel = SaveData->GameData.HQLevel + 1;
    bool bSuccess = false;
    FHQLevelData NextLevelData = TableMgr->GetHQLevelData(NextLevel, bSuccess);
    if (!bSuccess) return false;

    // Money 비용 차감
    if (NextLevelData.MoneyCost > 0)
    {
        UResourceItemManager* ResourceMgr = GetGameInstance()->GetSubsystem<UResourceItemManager>();
        if (ResourceMgr)
        {
            ResourceMgr->SpendResource(EResourceType::Money, NextLevelData.MoneyCost);
        }
    }

    // 레벨 증가
    SaveData->GameData.HQLevel = NextLevel;

    // 새로 열린 산업(RequiredHQLevel == NextLevel)당 프리미엄 채용권 1 고정 + 상위 테이블 확정 롤.
    // OnHQLevelUp 델리게이트 구독 방식 금지 — 치트/프리셋 3곳이 직접 Broadcast 해 중복 지급된다.
    for (uint8 TypeIdx = 1; TypeIdx < (uint8)ECompanyType::Max; ++TypeIdx)
    {
        const ECompanyType UnlockedType = (ECompanyType)TypeIdx;
        if (GetIndustryRequiredHQLevel(UnlockedType) != NextLevel)
        {
            continue;
        }
        if (UItemInventoryManager* ItemMgr = GetGameInstance()->GetSubsystem<UItemInventoryManager>())
        {
            ItemMgr->AddItem(EItemType::RecruitTicketPremium, 1, /*bShouldSave=*/false);
        }
        if (ULaunchLootManagerSubsystem* Loot = GetGameInstance()->GetSubsystem<ULaunchLootManagerSubsystem>())
        {
            // 저장은 루프 뒤 SaveGameData() 한 번으로 — 켜두면 동시 해금 산업 수만큼 전체 세이브가 돈다
            Loot->RollAndGrant(FName(TEXT("High")), Loot->GetProgressionTierContext(), /*ScaleBasis=*/0,
                /*ExtraRolls=*/0, /*bGrant=*/true, /*bSaveAfterGrant=*/false);
        }
        UE_LOG(LogTemp, Log, TEXT("[LaunchLoot] 산업 해금 보상: CompanyType=%d (HQ Lv.%d)"), TypeIdx, NextLevel);
    }

    UE_LOG(LogTemp, Log, TEXT("[SaveLoadManager] HQ LEVEL UP! Lv.%d -> Lv.%d"),
        NextLevel - 1, NextLevel);

    // 저장
    SaveGameData();

    // 이벤트 브로드캐스트
    OnHQLevelUp.Broadcast(NextLevel);

    // M15 ReachHQLevel 미션 통지
    if (UMissionManagerSubsystem* M = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>())
    {
        M->NotifyHQLevelChanged();
    }

    return true;
}

int32 USaveLoadManager::GetIndustryRequiredHQLevel(ECompanyType CompanyType)
{
    UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
    if (!TableMgr) return 0;

    bool bSuccess = false;
    const FCompanyInfoTable Info = TableMgr->GetCompanyInfo(CompanyType, bSuccess);
    if (!bSuccess)
    {
        // 행 결손 시 0(해금) — 잠그면 DT 누락이 진행을 차단하므로 여는 쪽이 안전. 단 리임포트 누락을 조용히 묻지 않도록 가시화.
        UE_LOG(LogTemp, Warning, TEXT("GetIndustryRequiredHQLevel: DT_CompanyInfo 행 없음 (CompanyType=%d) — 기본 해금(0) 처리"), static_cast<int32>(CompanyType));
        return 0;
    }
    return Info.RequiredHQLevel;
}

bool USaveLoadManager::IsIndustryUnlocked(ECompanyType CompanyType)
{
    return GetHQLevel() >= GetIndustryRequiredHQLevel(CompanyType);
}

bool USaveLoadManager::HasNextHQLevel()
{
    UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
    if (!TableMgr) return false;

    bool bSuccess = false;
    TableMgr->GetHQLevelData(GetHQLevel() + 1, bSuccess);
    return bSuccess;
}

// ========== 회사 등급 시스템 ==========

ECompanyTitle USaveLoadManager::GetCompanyTitle()
{
    USaveGame_GameData* SaveData = GetCurrentSaveData();
    if (!SaveData) return ECompanyTitle::Small;
    return SaveData->GameData.CompanyTitle;
}

bool USaveLoadManager::CanPromoteTitle()
{
    USaveGame_GameData* SaveData = GetCurrentSaveData();
    if (!SaveData) return false;

    ECompanyTitle CurrentTitle = SaveData->GameData.CompanyTitle;

    // 최고 등급이면 승격 불가
    if (CurrentTitle == ECompanyTitle::Elite) return false;

    // 공통 데이터 수집
    const int32 BuildingCount = SaveData->GameData.Buildings.Num();

    // 업종별 빌딩 수 집계
    TSet<ECompanyType> DistinctCompanyTypes;
    int32 TotalEmployees = 0;
    int32 ManagerCount = 0;

    for (const FBuildingEntitySaveData& Building : SaveData->GameData.Buildings)
    {
        if (Building.BuildingData.CompanyType != ECompanyType::None)
        {
            DistinctCompanyTypes.Add(Building.BuildingData.CompanyType);
        }
    }

    // 오피스 데이터에서 직원/매니저 집계 (수익은 시총으로 대체)
    for (const auto& Pair : SaveData->GameData.OfficeDataMap)
    {
        const FOfficeSaveData& Office = Pair.Value;
        TotalEmployees += Office.EmployeeList.Num();

        for (const FEmployeeInstance& Emp : Office.EmployeeList)
        {
            if (Emp.Department == EEmployeeDepartment::Management)
            {
                ManagerCount++;
            }
        }
    }

    // 현재 시총 — 2026-04-24 누적 매출(CumulativeRevenue) 대체. 시총이 더 자연스런 기업 규모 지표
    int64 CurrentMarketCap = 0;
    if (UResourceItemManager* ResourceMgr = GetGameInstance()->GetSubsystem<UResourceItemManager>())
    {
        CurrentMarketCap = ResourceMgr->GetResourceAmount(EResourceType::MarketCap);
    }

    // 누적 통계
    const int32 TotalProjects = SaveData->GameData.TotalProjectsCompleted;
    const int32 AGradeProjects = SaveData->GameData.AGradeProjectsCompleted;
    const int32 SGradeProjects = SaveData->GameData.SGradeProjectsCompleted;

    switch (CurrentTitle)
    {
    case ECompanyTitle::Small:
    {
        // 중견기업 조건
        if (BuildingCount < 3) return false;
        if (DistinctCompanyTypes.Num() < 2) return false;
        if (CountBuildingsAtTier(3) < 1) return false;
        if (CurrentMarketCap < 100'000'000LL) return false;  // 시총 1억
        if (TotalProjects < 10) return false;
        return true;
    }
    case ECompanyTitle::MidSize:
    {
        // 대기업 조건
        if (BuildingCount < 5) return false;
        if (CountBuildingsAtTier(5) < 3) return false;
        if (TotalEmployees < 30) return false;
        if (AGradeProjects < 10) return false;
        if (CurrentMarketCap < 10'000'000'000LL) return false;  // 시총 100억
        return true;
    }
    case ECompanyTitle::Large:
    {
        // 글로벌 기업 조건: 6종 업종 모두 운영
        if (DistinctCompanyTypes.Num() < 6) return false;
        if (CountBuildingsAtTier(7) < 1) return false;
        if (ManagerCount < 10) return false;
        if (SGradeProjects < 5) return false;
        if (CurrentMarketCap < 1'000'000'000'000LL) return false;  // 시총 1조
        return true;
    }
    case ECompanyTitle::Global:
    {
        // 초일류 기업 조건
        if (BuildingCount < 10) return false;
        if (CountBuildingsAtTier(9) < 3) return false;
        if (ManagerCount < 20) return false;
        if (SGradeProjects < 30) return false;
        if (CurrentMarketCap < 100'000'000'000'000LL) return false;  // 시총 100조
        return true;
    }
    default:
        return false;
    }
}

bool USaveLoadManager::TryPromoteTitle()
{
    if (!CanPromoteTitle()) return false;

    USaveGame_GameData* SaveData = GetCurrentSaveData();
    if (!SaveData) return false;

    int32 CurrentIdx = static_cast<int32>(SaveData->GameData.CompanyTitle);
    SaveData->GameData.CompanyTitle = static_cast<ECompanyTitle>(CurrentIdx + 1);

    UE_LOG(LogTemp, Log, TEXT("[SaveLoadManager] Company Title PROMOTED! -> %s"),
        *CompanyTitleToString(SaveData->GameData.CompanyTitle));

    SaveGameData();
    OnCompanyTitleChanged.Broadcast(SaveData->GameData.CompanyTitle);
    return true;
}

void USaveLoadManager::RecordProjectCompletion(bool bIsAGrade, bool bIsSGrade)
{
    USaveGame_GameData* SaveData = GetCurrentSaveData();
    if (!SaveData) return;

    SaveData->GameData.TotalProjectsCompleted++;
    if (bIsAGrade) SaveData->GameData.AGradeProjectsCompleted++;
    if (bIsSGrade) SaveData->GameData.SGradeProjectsCompleted++;

    SaveGameData();
}
