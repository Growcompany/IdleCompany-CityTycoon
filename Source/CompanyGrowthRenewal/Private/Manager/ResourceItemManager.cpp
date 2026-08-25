// ResourceItemManager.cpp

#include "Manager/ResourceItemManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/ItemInventoryManager.h"
#include "Table/TestResourceScenarioTable.h"
#include "Engine/DataTable.h"

void UResourceItemManager::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // TableManagerSubsystem이 먼저 초기화되도록 의존성 선언
    Collection.InitializeDependency<UTableManagerSubsystem>();
    // SaveLoadManager 도 먼저 init 돼야 OnGameDataLoaded 구독 안전 (Dev 빌드 자동 시나리오 필수)
    Collection.InitializeDependency<USaveLoadManager>();

    // TableManager에서 각 리소스의 초기값 읽어오기
    UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
    if (TableMgr)
    {
        // Brick, Money, Diamond 초기화
        for (int32 i = 1; i <= 3; ++i)
        {
            EResourceType Type = static_cast<EResourceType>(i);
            bool bSuccess = false;
            FResourceInfo Info = TableMgr->GetResourceInfo(Type, bSuccess);

            if (bSuccess)
            {
                ResourceBank.Add(Type, Info.InitialAmount);
                // UI 업데이트를 위해 OnResourceChanged 브로드캐스트
                OnResourceChanged.Broadcast(Type, Info.InitialAmount);
                UE_LOG(LogTemp, Log, TEXT("[ResourceItemManager] Initialized %d with amount: %lld"),
                    (int32)Type, Info.InitialAmount);
            }
        }
    }

    // 30초마다 자동 저장 타이머 시작
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            PeriodicSaveTimerHandle,
            this,
            &UResourceItemManager::PeriodicSave,
            30.0f,  // 30초
            true    // 반복
        );
        UE_LOG(LogTemp, Log, TEXT("[ResourceItemManager] Periodic auto-save timer started (30s)"));
    }

#if !UE_BUILD_SHIPPING
    // SaveLoad 끝나면 Test 자원 시나리오(Default) 자동 적용
    if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
    {
        SaveMgr->OnGameDataLoaded.AddUObject(this, &UResourceItemManager::HandleGameDataLoaded);
    }
#endif
}

void UResourceItemManager::HandleGameDataLoaded()
{
#if !UE_BUILD_SHIPPING
    // 일반 모드 기본 — 자동 시나리오 적용 안 함 (오프닝/미션이 실제 신규 게임 흐름으로 검증되도록).
    // 개발 중 재화가 필요하면 콘솔: GrantRes Rich (DT_TestResourceScenarios 행 이름).
    // TODO: 디버그 매니저 도입 시 dev 모드 플래그(전체 해금/재화/레벨 프리셋)로 이 훅을 재구성.
    UE_LOG(LogTemp, Log, TEXT("[ResourceItemManager] Normal mode - no auto resource scenario (console: GrantRes <name>)"));
#endif
}

void UResourceItemManager::Deinitialize()
{
    // 타이머 정리
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(PeriodicSaveTimerHandle);
    }

    // 종료 시 마지막 저장 + 델리게이트 정리
    if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
    {
        SaveMgr->OnGameDataLoaded.RemoveAll(this);
        SaveMgr->SaveGameData();
        UE_LOG(LogTemp, Log, TEXT("[ResourceItemManager] Final save on Deinitialize"));
    }

    Super::Deinitialize();
}

int64 UResourceItemManager::GetResourceAmount(EResourceType Type) const
{
    if (const int64* Found = ResourceBank.Find(Type))
    {
        return *Found;
    }
    return 0;
}

bool UResourceItemManager::HasResource(EResourceType Type, int64 Amount) const
{
    return GetResourceAmount(Type) >= Amount;
}

bool UResourceItemManager::CanAffordCosts(const TArray<FConstructionCost>& Costs, EResourceType& OutMissingType) const
{
    // 같은 자원 타입이 여러 항목으로 쪼개져 있어도 합산해 판정 — 개별 비교 시 중복 타입이 통과해 underpay 되는 것 방지
    TMap<EResourceType, int64> Needed;
    for (const FConstructionCost& Cost : Costs)
    {
        Needed.FindOrAdd(Cost.ResourceType) += Cost.Cost;
    }
    for (const TPair<EResourceType, int64>& Pair : Needed)
    {
        if (!HasResource(Pair.Key, Pair.Value))
        {
            OutMissingType = Pair.Key;
            return false;
        }
    }
    OutMissingType = EResourceType::None;
    return true;
}

void UResourceItemManager::StoreResource(
    EResourceType Type,
    int64 Amount,
    bool bShouldSave,
    bool bShouldBroadcast)
{
    int64& Curr = ResourceBank.FindOrAdd(Type);
    Curr += Amount;
    // Verbose — 방치 수익이 초당 여러 번 부르므로 Warning 이면 실제 경고가 로그에서 묻힌다.
    UE_LOG(LogTemp, Verbose, TEXT("[ResourceItemManager] StoreResource: Type=%d, Added=%lld, NewTotal=%lld, Save=%d"),
        (int32)Type, Amount, Curr, bShouldSave);
    if (bShouldBroadcast)
    {
        OnResourceChanged.Broadcast(Type, Curr);
    }

    // Money 수입 시 누적 매출 기록 (랭킹용)
    if (Type == EResourceType::Money && Amount > 0)
    {
        if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
        {
            if (USaveGame_GameData* SaveData = SaveMgr->GetCurrentSaveData())
            {
                SaveData->GameData.TotalRevenueEarned += Amount;
            }
        }
    }

    // 이벤트성 변경이면 즉시 저장 (자동 생산은 30초 타이머로 저장)
    if (bShouldSave)
    {
        if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
        {
            SaveMgr->SaveGameData();
            UE_LOG(LogTemp, Log, TEXT("[ResourceItemManager] Immediate save after StoreResource"));
        }
    }
}

void UResourceItemManager::SpendResource(EResourceType Type, int64 Amount, bool bShouldSave)
{
    if (!HasResource(Type, Amount))
    {
        UE_LOG(LogTemp, Warning, TEXT("[ResourceItemManager] SpendResource failed: Not enough resources! Type=%d, Requested=%lld"),
            (int32)Type, Amount);
        return;
    }
    int64& Curr = ResourceBank.FindOrAdd(Type);
    Curr -= Amount;
    UE_LOG(LogTemp, Warning, TEXT("[ResourceItemManager] SpendResource: Type=%d, Spent=%lld, NewTotal=%lld, Save=%d"),
        (int32)Type, Amount, Curr, bShouldSave);
    OnResourceChanged.Broadcast(Type, Curr);

    // 이벤트성 변경이면 즉시 저장 (자동 생산은 30초 타이머로 저장)
    if (bShouldSave)
    {
        if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
        {
            SaveMgr->SaveGameData();
            UE_LOG(LogTemp, Log, TEXT("[ResourceItemManager] Immediate save after SpendResource"));
        }
    }
}

bool UResourceItemManager::ExtractResource(EResourceType Type, int64 Requested, int64& Withdrawn, bool bShouldSave)
{
    if (!HasResource(Type, Requested))
    {
        Withdrawn = 0;
        return false;
    }
    Withdrawn = Requested;
    SpendResource(Type, Requested, bShouldSave);  // bShouldSave 전달
    return true;
}

void UResourceItemManager::SetAllResources(
    const TMap<EResourceType, int64>& InResources,
    bool bShouldBroadcast)
{
    ResourceBank = InResources;

    if (bShouldBroadcast)
    {
        // 각 자원별로 UI 업데이트 이벤트 발생
        for (const auto& Pair : ResourceBank)
        {
            OnResourceChanged.Broadcast(Pair.Key, Pair.Value);
        }
    }
}

bool UResourceItemManager::GrantTestResources(FName ScenarioRowName)
{
	UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableMgr) return false;

	UDataTable* DT = TableMgr->GetTestResourceScenariosTable();
	if (!DT)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ResourceItemManager] DT_TestResourceScenarios not loaded"));
		return false;
	}

	const FTestResourceScenarioRow* Row = DT->FindRow<FTestResourceScenarioRow>(ScenarioRowName, TEXT("GrantTestResources"));
	if (!Row)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ResourceItemManager] Scenario not found: %s"), *ScenarioRowName.ToString());
		return false;
	}

	TMap<EResourceType, int64> NewBank;
	NewBank.Add(EResourceType::Money,      Row->Money);
	NewBank.Add(EResourceType::Brick,      Row->Brick);
	NewBank.Add(EResourceType::Diamond,    Row->Diamond);
	NewBank.Add(EResourceType::MarketCap,  Row->MarketCap);
	NewBank.Add(EResourceType::IronOre,    Row->IronOre);
	NewBank.Add(EResourceType::Aluminum,   Row->Aluminum);
	NewBank.Add(EResourceType::Copper,     Row->Copper);
	NewBank.Add(EResourceType::Oil,        Row->Oil);
	NewBank.Add(EResourceType::RareEarth,  Row->RareEarth);
	NewBank.Add(EResourceType::Gold,       Row->Gold);
	NewBank.Add(EResourceType::Silicon,    Row->Silicon);
	NewBank.Add(EResourceType::Wood,       Row->Wood);
	NewBank.Add(EResourceType::Lithium,    Row->Lithium);
	NewBank.Add(EResourceType::DiamondOre, Row->DiamondOre);

	SetAllResources(NewBank);

	// 뽑기권은 EItemType 이라 ItemInventoryManager 로 별도 일괄 set (재화 bank 와 분리된 저장소)
	if (UItemInventoryManager* ItemMgr = GetGameInstance()->GetSubsystem<UItemInventoryManager>())
	{
		TMap<EItemType, int32> NewItems;
		NewItems.Add(EItemType::RecruitTicketNormal,         Row->RecruitTicketNormal);
		NewItems.Add(EItemType::RecruitTicketAdvanced,       Row->RecruitTicketAdvanced);
		NewItems.Add(EItemType::RecruitTicketPremium,        Row->RecruitTicketPremium);
		NewItems.Add(EItemType::DepartmentTicket,            Row->DepartmentTicket);
		NewItems.Add(EItemType::BuildingTraitTicketNormal,   Row->BuildingTraitTicketNormal);
		NewItems.Add(EItemType::BuildingTraitTicketAdvanced, Row->BuildingTraitTicketAdvanced);
		NewItems.Add(EItemType::SkinTicketNormal,            Row->SkinTicketNormal);
		NewItems.Add(EItemType::SkinTicketAdvanced,          Row->SkinTicketAdvanced);

		ItemMgr->SetAllItems(NewItems);
	}

	UE_LOG(LogTemp, Log, TEXT("[ResourceItemManager] GrantTestResources: scenario=%s applied"), *ScenarioRowName.ToString());
	return true;
}

void UResourceItemManager::PeriodicSave()
{
    if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
    {
        SaveMgr->SaveGameData();
        UE_LOG(LogTemp, Log, TEXT("[ResourceItemManager] Periodic auto-save executed"));
    }
}
