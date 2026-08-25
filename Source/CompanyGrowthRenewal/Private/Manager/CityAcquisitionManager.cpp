#include "Manager/CityAcquisitionManager.h"
#include "Entity/Plot/CityClickProxyActor.h"
#include "Entity/Plot/CityDemolitionActor.h"
#include "Manager/SoundManagerSubsystem.h"
#include "Table/VFXTable.h"
#include "Enum/VFXType.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/SpawnManager.h"
#include "Manager/CityCompanyDirector.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/MissionManagerSubsystem.h"
#include "Manager/LaunchLootManagerSubsystem.h"
#include "Manager/UIManagerSubsystem.h"
#include "Data/GameSaveData.h"
#include "Data/CityAcqSaveData.h"
#include "Table/CityCompanyData.h"
#include "Table/CityPlotData.h"
#include "Entity/Plot/CityPlotActor.h"
#include "Enum/ResourceType.h"
#include "Enum/WidgetType.h"
#include "UI/UIBase.h"
#include "UI/Panel/CityCompanyInfoWidget.h"
#include "UI/Panel/CityCompanyManageWidget.h"
#include "Player/PlayerCamera.h"

UResourceItemManager* UCityAcquisitionManager::ResMgr() const { return GetWorld()->GetGameInstance()->GetSubsystem<UResourceItemManager>(); }
UTableManagerSubsystem* UCityAcquisitionManager::TableMgr() const { return GetWorld()->GetGameInstance()->GetSubsystem<UTableManagerSubsystem>(); }
USpawnManager* UCityAcquisitionManager::SpawnMgr() const { return GetWorld()->GetSubsystem<USpawnManager>(); }
UCityCompanyDirector* UCityAcquisitionManager::Director() const { return GetWorld()->GetSubsystem<UCityCompanyDirector>(); }

void UCityAcquisitionManager::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);
    TickHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateUObject(this, &UCityAcquisitionManager::Tick), 1.0f); // 1초 간격
    if (USaveLoadManager* S = GetWorld()->GetGameInstance()->GetSubsystem<USaveLoadManager>())
    {
        S->OnGameDataLoaded.AddUObject(this, &UCityAcquisitionManager::LoadFromGame); // 미래 로드 대비
        LoadFromGame(); // 이미 로드 완료된 경우 즉시 복원
        S->OnOfflineGainsApplied.AddUObject(this, &UCityAcquisitionManager::SettleOffline);
    }
    // 프록시 스폰은 0.5s 지연 — Director(같은 OnWorldBeginPlay 단계)가 먼저 SkylineEntries 를
    // 채우도록 서브시스템 초기화 순서 의존을 제거.
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().SetTimer(
            ProxySpawnHandle, this, &UCityAcquisitionManager::SpawnClickProxies, 0.5f, false);
    }
}

void UCityAcquisitionManager::SpawnClickProxies()
{
    if (!Director() || !GetWorld()) { return; }
    int32 Spawned = 0;
    for (const FCitySkylineEntry& E : Director()->GetSkylineEntries())
    {
        if (GetState(E.BuildingKey) == EAcqState::Cleared) { continue; }
        AActor* B = E.Building.Get();
        if (!B) { continue; }
        FVector BOrigin, BExtent;
        B->GetActorBounds(false, BOrigin, BExtent);
        // 박스를 바운드 중심(BOrigin)에 — ActorLocation(피벗=바닥)이면 박스가 절반 지하로 어긋나 상단 클릭이 빗나감
        ACityClickProxyActor* Px = GetWorld()->SpawnActor<ACityClickProxyActor>(BOrigin, FRotator::ZeroRotator);
        if (Px) { Px->InitProxy(E.BuildingKey, BExtent); ClickProxies.Add(Px); ++Spawned; }
    }
    UE_LOG(LogTemp, Log, TEXT("[Acq] click proxies spawned: %d"), Spawned);
}
void UCityAcquisitionManager::Deinitialize() { if (TickHandle.IsValid()) { FTSTicker::GetCoreTicker().RemoveTicker(TickHandle); } Super::Deinitialize(); }

EAcqState UCityAcquisitionManager::GetState(int32 Key) const
{
    if (const FCityAcqRuntime* R = Runtime.Find(Key)) { return R->State; }
    return EAcqState::NotAcquired;
}

void UCityAcquisitionManager::GetYieldRange(int32 Key, int32& OutMin, int32& OutMax, int32& OutEv) const
{
    OutMin = OutMax = OutEv = 0;
    FCityCompanyData D;
    if (TableMgr() && TableMgr()->GetCityCompanyData(Key, D)) { OutMin = D.YieldMinPct; OutMax = D.YieldMaxPct; OutEv = D.GetEvPct(); }
}

bool UCityAcquisitionManager::GetCompanyProgress(int32 Key, int64& OutCost, int64& OutEarned, int64& OutRemaining, bool& OutDepleted) const
{
    OutCost = OutEarned = OutRemaining = 0;
    OutDepleted = false;
    const FCityAcqRuntime* R = Runtime.Find(Key);
    if (!R || (R->State != EAcqState::Milking && R->State != EAcqState::Depleted)) { return false; }
    FCityCompanyData D;
    if (TableMgr() && TableMgr()->GetCityCompanyData(Key, D)) { OutCost = D.AcquisitionCost; }
    OutEarned = R->RTotal - R->RRemaining;
    OutRemaining = R->RRemaining;
    OutDepleted = (R->State == EAcqState::Depleted);
    return true;
}

bool UCityAcquisitionManager::GetDripInfo(int32 Key, int64& OutChunkAmount, int64& OutExpectedPerSec, float& OutSecondsPerDrip) const
{
    OutChunkAmount = OutExpectedPerSec = 0;
    OutSecondsPerDrip = 0.f;
    const FCityAcqRuntime* R = Runtime.Find(Key);
    if (!R || (R->State != EAcqState::Milking && R->State != EAcqState::Depleted)) { return false; }
    FCityCompanyData D;
    if (!TableMgr() || !TableMgr()->GetCityCompanyData(Key, D)) { return false; }
    OutChunkAmount = (int64)(D.AcquisitionCost * D.DripChunkPct / 100.0);
    const double CritFactor = 1.0 + D.CritChance * (D.CritMult - 1.0);
    OutExpectedPerSec = (int64)((double)OutChunkAmount * (double)D.DripChance * CritFactor);
    OutSecondsPerDrip = (D.DripChance > 0.f) ? (1.f / D.DripChance) : 0.f; // 티커가 1초 간격이라 확률의 역수 = 기대 간격(초)
    return true;
}

int64 UCityAcquisitionManager::GetPotAmount(int32 Key) const
{
    const FCityAcqRuntime* R = Runtime.Find(Key);
    if (!R || (R->State != EAcqState::Milking && R->State != EAcqState::Depleted)) { return 0; }
    return R->RTotal - R->RRemaining;
}

AActor* UCityAcquisitionManager::GetCompanyActor(int32 Key) const
{
    if (!Director()) { return nullptr; }
    for (const FCitySkylineEntry& E : Director()->GetSkylineEntries())
    {
        if (E.BuildingKey == Key) { return E.Building.Get(); }
    }
    return nullptr;
}

void UCityAcquisitionManager::GetOccupantKeysForPlot(FName PlotId, TArray<int32>& OutKeys) const
{
    OutKeys.Reset();
    if (!TableMgr()) { return; }
    bool bOk = false;
    const FCityPlotData P = TableMgr()->GetCityPlotData(PlotId, bOk);
    if (!bOk) { return; }
    for (const int32 Key : P.OccupantCompanyKeys)
    {
        // 0 은 "입주 없음" 표기라 회사로 치지 않는다.
        if (Key != 0) { OutKeys.Add(Key); }
    }
}

bool UCityAcquisitionManager::ArePlotOccupantsCleared(FName PlotId) const
{
    TArray<int32> Keys;
    GetOccupantKeysForPlot(PlotId, Keys);
    for (const int32 Key : Keys)
    {
        if (GetState(Key) != EAcqState::Cleared) { return false; }
    }
    return true;
}

bool UCityAcquisitionManager::CanAcquire(int32 Key) const
{
    if (GetState(Key) != EAcqState::NotAcquired) { return false; }
    FCityCompanyData D;
    if (!TableMgr() || !TableMgr()->GetCityCompanyData(Key, D)) { return false; }
    if (!ResMgr() || !ResMgr()->HasResource(EResourceType::Money, D.AcquisitionCost)) { return false; }
    return true; // 인접 부지 게이트 제거 — 돈만 되면 어디든 인수 (사용자 결정 2026-06-28)
}

bool UCityAcquisitionManager::StepDrip(int32 Key, FCityAcqRuntime& R, const FCityCompanyData& D, bool bSilent, bool& bOutDripped)
{
    bOutDripped = false;
    bool bStateChanged = false;

    if (FMath::FRand() <= D.DripChance)
    {
        int64 Chunk = (int64)(D.AcquisitionCost * D.DripChunkPct / 100.0);
        const bool bCrit = FMath::FRand() <= D.CritChance;
        if (bCrit) { Chunk = (int64)(Chunk * D.CritMult); }
        Chunk = FMath::Min(Chunk, R.RRemaining);
        if (Chunk > 0)
        {
            R.RRemaining -= Chunk; // 금고 적립 — 지갑 지급은 Demolish(캐시아웃)가 유일 (D8)
            if (!bSilent)
            {
                PlayDripFeedback(Key, bCrit);
                OnCompanyProgressChanged.Broadcast(Key);
            }
            bOutDripped = true;
        }
        if (R.RRemaining <= 0)
        {
            R.State = EAcqState::Depleted; // 수입 고갈 -> 수동 철거 대기(건물 유지)
            bStateChanged = true;
            if (!bSilent) { OnCompanyProgressChanged.Broadcast(Key); } // 고갈 전환 — 패널 철거 버튼 라벨 갱신 트리거
        }
    }

    return bStateChanged;
}

bool UCityAcquisitionManager::Tick(float Dt)
{
    bool bDrippedThisTick = false;
    bool bStateChangedThisTick = false;
    for (TPair<int32, FCityAcqRuntime>& It : Runtime)
    {
        FCityAcqRuntime& R = It.Value;
        if (R.State != EAcqState::Milking) { continue; }
        FCityCompanyData D;
        if (!TableMgr() || !TableMgr()->GetCityCompanyData(It.Key, D)) { continue; }

        bool bDripped = false;
        // 저장은 루프 밖 1회 — 회사 수에 비례한 저장 폭주 방지
        if (StepDrip(It.Key, R, D, /*bSilent=*/false, bDripped)) { bStateChangedThisTick = true; }
        // 회사별이 아닌 틱당 1회 — 동시 Milking N개여도 저장 주기가 10/N초로 짧아지지 않게
        if (bDripped) { bDrippedThisTick = true; }
    }
    if (bDrippedThisTick) { ++DripTicksSinceSave; }
    if (bStateChangedThisTick) { DripTicksSinceSave = 0; SaveToGame(); } // 고갈 전환 저장이 이 틱의 드립까지 커버 -> 스로틀 카운터도 리셋
    if (DripTicksSinceSave >= 10)
    {
        DripTicksSinceSave = 0;
        SaveToGame();
    }
    return true; // keep ticking
}

int32 UCityAcquisitionManager::GetSkipDiamondCost(int32 Key) const
{
    FCityCompanyData D;
    if (!TableMgr() || !TableMgr()->GetCityCompanyData(Key, D)) { return 0; }
    return FMath::Max(10, D.Tier * 10);
}

bool UCityAcquisitionManager::SkipRecovery(int32 Key)
{
    FCityAcqRuntime* R = Runtime.Find(Key);
    if (!R || R->State != EAcqState::Milking) { return false; }

    FCityCompanyData D;
    if (!TableMgr() || !TableMgr()->GetCityCompanyData(Key, D)) { return false; }

    const int32 Cost = GetSkipDiamondCost(Key);
    UResourceItemManager* Res = ResMgr();
    if (!Res || !Res->HasResource(EResourceType::Diamond, Cost)) { return false; }
    Res->SpendResource(EResourceType::Diamond, Cost, /*bShouldSave=*/false);

    // 온라인 티커와 같은 StepDrip 을 남은 구간만큼 압축 실행한다 — 파는 것은 시간뿐이다.
    // 갈라진 구현을 쓰면 "건너뛴 결과"가 기다린 결과와 달라진다.
    // 가드: 드립 확률이 0 이거나 청크가 0 인 DT 행이 들어와도 프레임을 잡아먹지 않게 상한을 둔다.
    // T10 은 기대 24시간(=86400틱)이라 상한은 그 몇 배로 잡아야 정상 완주가 상한에 걸리지 않는다.
    constexpr int32 MaxSteps = 2000000;
    int32 Steps = 0;
    bool bDrippedIgnored = false;
    while (R->State == EAcqState::Milking && Steps++ < MaxSteps)
    {
        StepDrip(Key, *R, D, /*bSilent=*/true, bDrippedIgnored);
    }

    if (Steps >= MaxSteps)
    {
        // 여기 오면 DT 가 진행 불가 조합(DripChance 0 등) — 보석을 먹고 멈추지 않도록 강제 고갈시킨다
        UE_LOG(LogTemp, Warning, TEXT("[Acq] SkipRecovery 상한 도달 key=%d — DT 드립 파라미터 확인 필요"), Key);
        R->RRemaining = 0;
        R->State = EAcqState::Depleted;
    }

    UE_LOG(LogTemp, Log, TEXT("[Acq] skip key=%d cost=%d steps=%d state=%d pot=%lld"),
        Key, Cost, Steps, (int32)R->State, R->RTotal - R->RRemaining);

    // 압축 중에는 침묵했으므로 결과를 한 번에 알린다
    OnCompanyProgressChanged.Broadcast(Key);
    SaveToGame();
    return true;
}

void UCityAcquisitionManager::Demolish(int32 Key)
{
    FCityAcqRuntime* R = Runtime.Find(Key);
    if (!R) { return; }
    if (R->State != EAcqState::Milking && R->State != EAcqState::Depleted) { return; }
    const int64 Pot = R->RTotal - R->RRemaining;
    if (Pot > 0 && ResMgr()) { ResMgr()->StoreResource(EResourceType::Money, Pot, /*bShouldSave=*/false); } // 캐시아웃 — 유일한 지갑 지급 지점 (D8)
    UE_LOG(LogTemp, Log, TEXT("[Acq] cashout key=%d pot=%lld state=%d"), Key, Pot, (int32)R->State);
    R->State = EAcqState::Cleared;
	bool bVisualEventDeferred = false;
	if (AActor* A = GetCompanyActor(Key))
	{
        // 붕괴를 지켜보도록 카메라를 대상 건물로 중앙 포커스(관리 패널은 닫히는 중)
        if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
        {
            if (APlayerCamera* Cam = Cast<APlayerCamera>(PC->GetPawn()))
            {
                // 붕괴 관전은 고스트 범위 밖(설계 §6.1) — 등록하면 안전망이 연출 0.15초쯤에 걷어가 깜빡인다
                Cam->FocusOnActor(A, 0.5f, false);
            }
        }

        // 철거 연출 액터가 층별 붕괴를 굴리고 끝에서 건물을 숨긴다(자가 소멸). 스폰 실패 시 즉시 숨김 폴백.
        bool bStarted = false;
        if (UWorld* W = GetWorld())
        {
            int32 Floors = 6;
            FCityCompanyData D;
            if (TableMgr() && TableMgr()->GetCityCompanyData(Key, D) && D.Stories > 0) { Floors = D.Stories; }

            FActorSpawnParameters Params;
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (ACityDemolitionActor* FX = W->SpawnActor<ACityDemolitionActor>(
					A->GetActorLocation(), FRotator::ZeroRotator, Params))
			{
				FX->OnVisualCleared.AddUObject(this, &UCityAcquisitionManager::HandleCompanyVisualCleared);
				FX->BeginDemolition(A, Floors, Key);
				bStarted = true;
				bVisualEventDeferred = true;
			}
		}
		if (!bStarted)
		{
			A->SetActorHiddenInGame(true);
			A->SetActorEnableCollision(false);
			OnCompanyVisualCleared.Broadcast(Key);
		}
	}
	if (!bVisualEventDeferred && !GetCompanyActor(Key))
	{
		// 매핑된 Actor가 이미 없으면 시각 점유도 이미 없으므로 즉시 완료로 알린다.
		OnCompanyVisualCleared.Broadcast(Key);
	}
    // 클릭 프록시 제거 — 안 그러면 빈 부지 위에서 프록시 콜리전이 부지 구매 클릭을 가로챔
    for (int32 i = ClickProxies.Num() - 1; i >= 0; --i)
    {
        if (ACityClickProxyActor* Px = Cast<ACityClickProxyActor>(ClickProxies[i]))
        {
            if (Px->GetCompanyKey() == Key) { Px->Destroy(); ClickProxies.RemoveAt(i); }
        }
    }
    OnCompanyCleared.Broadcast(Key);
    SaveToGame();
}

int32 UCityAcquisitionManager::DebugClearAllCompanies()
{
    if (!Director()) { return 0; }

    int32 Count = 0;
    for (const FCitySkylineEntry& E : Director()->GetSkylineEntries())
    {
        const int32 Key = E.BuildingKey;
        FCityAcqRuntime& R = Runtime.FindOrAdd(Key);
        if (R.State == EAcqState::Cleared) { continue; }

        R.State = EAcqState::Cleared;
        R.RRemaining = 0;

        // 데모 연출 없이 즉시 "이미 정리됨" — LoadFromGame 의 Cleared 복원과 동일(hidden + 콜리전 off)
		if (AActor* A = E.Building.Get())
		{
			A->SetActorHiddenInGame(true);
			A->SetActorEnableCollision(false);
		}
		OnCompanyVisualCleared.Broadcast(Key);

		OnCompanyCleared.Broadcast(Key); // 옥상 비콘 제거 + 부지 게이트/가격 배지 갱신
        ++Count;
    }

    // 빈 부지 클릭이 프록시 콜리전에 가로채이지 않도록 클릭 프록시 전부 제거(Demolish 의 부분 제거를 전체로)
    for (AActor* Px : ClickProxies) { if (Px) { Px->Destroy(); } }
    ClickProxies.Empty();

    SaveToGame();
    return Count;
}

void UCityAcquisitionManager::HandleCompanyVisualCleared(int32 Key)
{
	OnCompanyVisualCleared.Broadcast(Key);
}

void UCityAcquisitionManager::DebugFillPot(int32 Key, int32 Pct)
{
    FCityAcqRuntime* R = Runtime.Find(Key);
    if (!R || R->State != EAcqState::Milking || R->RTotal <= 0) { return; }
    const int64 TargetRemaining = R->RTotal * (int64)FMath::Clamp(100 - Pct, 0, 100) / 100;
    R->RRemaining = FMath::Min(R->RRemaining, TargetRemaining);
    if (R->RRemaining <= 0) { R->State = EAcqState::Depleted; }
    SaveToGame();
    OnCompanyProgressChanged.Broadcast(Key);
}

void UCityAcquisitionManager::PlayDripFeedback(int32 Key, bool bCrit)
{
    AActor* Building = GetCompanyActor(Key);
    if (!Building) { return; }

    FVector BOrigin, BExtent;
    Building->GetActorBounds(false, BOrigin, BExtent);
    const FVector Top(BOrigin.X, BOrigin.Y, Building->GetActorLocation().Z + BExtent.Z * 2.f);

    // 코인 분출 Niagara — DT_VFX(CityDrip) 행/에셋 미배치면 GetVFXAsset 가 nullptr → 스폰 생략(안전)
    if (UTableManagerSubsystem* TM = TableMgr())
    {
        bool bHasRow = false;
        const FVFXTableRow RowData = TM->GetVFXData(EVFXType::CityDrip, bHasRow);
        if (UNiagaraSystem* DripSystem = TM->GetVFXAsset(EVFXType::CityDrip))
        {
            const FVector SpawnPos = Top + (bHasRow ? RowData.LocationOffset : FVector(0.f, 0.f, 40.f));
            const float SpawnScale = (bHasRow ? RowData.DefaultScale : 1.0f) * (bCrit ? 1.8f : 1.0f);
            UNiagaraFunctionLibrary::SpawnSystemAtLocation(
                GetWorld(), DripSystem, SpawnPos, FRotator::ZeroRotator,
                FVector(SpawnScale), /*bAutoDestroy*/ true);
        }
    }

    // 코인 음 — 클립 미배정이면 무음(죽은 태그 아님)
    if (UGameInstance* GI = GetWorld()->GetGameInstance())
    {
        if (USoundManagerSubsystem* SoundMgr = GI->GetSubsystem<USoundManagerSubsystem>())
        {
            SoundMgr->PlaySoundAtLocation(FName("City_Drip"), Top);
        }
    }
}

bool UCityAcquisitionManager::Acquire(int32 Key)
{
    if (!CanAcquire(Key)) { return false; }
    FCityCompanyData D;
    TableMgr()->GetCityCompanyData(Key, D);
    ResMgr()->SpendResource(EResourceType::Money, D.AcquisitionCost, /*bShouldSave=*/false);
    const int32 Pct = FMath::RandRange(D.YieldMinPct, D.YieldMaxPct);
    FCityAcqRuntime R;
    R.State = EAcqState::Milking;
    R.RTotal = (int64)(D.AcquisitionCost * Pct / 100.0);
    R.RRemaining = R.RTotal;
    Runtime.Add(Key, R);
    SaveToGame();

    // M19 — 성공 경로에서만 통지(CanAcquire 실패는 위에서 이미 return). 델리게이트 없이 직접 호출하는 유일한 신호선.
    if (UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
    {
        if (UMissionManagerSubsystem* MissionMgr = GI->GetSubsystem<UMissionManagerSubsystem>())
        {
            MissionMgr->NotifyCompanyAcquired(Key);
        }

        // 인수 성공 보너스 — 상위 테이블 확정 롤 (스케일 기준 = 인수가)
        // ⚠ 여기만 bSaveAfterGrant 기본값(저장 O) 유지 — 위 SaveToGame() 이 롤보다 앞서라 끄면 지급 아이템이 미저장으로 남는다
        if (ULaunchLootManagerSubsystem* Loot = GI->GetSubsystem<ULaunchLootManagerSubsystem>())
        {
            Loot->RollAndGrant(FName(TEXT("High")), Loot->GetProgressionTierContext(), D.AcquisitionCost);
        }
    }
    return true;
}

void UCityAcquisitionManager::SaveToGame()
{
    USaveLoadManager* S = GetWorld()->GetGameInstance()->GetSubsystem<USaveLoadManager>();
    if (!S || !S->GetCurrentSaveData()) { return; }
    FGameSaveData& G = S->GetCurrentSaveData()->GameData;
    G.CityAcq.Empty();
    for (const TPair<int32, FCityAcqRuntime>& It : Runtime)
    {
        FCityAcqSave Rec;
        Rec.State = (uint8)It.Value.State;
        Rec.RTotal = It.Value.RTotal;
        Rec.RRemaining = It.Value.RRemaining;
        G.CityAcq.Add(It.Key, Rec);
    }
    S->SaveGameData();
}

void UCityAcquisitionManager::LoadFromGame()
{
    USaveLoadManager* S = GetWorld()->GetGameInstance()->GetSubsystem<USaveLoadManager>();
    if (!S || !S->GetCurrentSaveData()) { return; }
    const FGameSaveData& G = S->GetCurrentSaveData()->GameData;
    Runtime.Empty();
    for (const TPair<int32, FCityAcqSave>& It : G.CityAcq)
    {
        FCityAcqRuntime R;
        R.State = (EAcqState)It.Value.State;
        R.RTotal = It.Value.RTotal;
        R.RRemaining = It.Value.RRemaining;
        // 세이브의 State 는 uint8 원본이라 열거형 범위 밖 값이 들어올 수 있다(폐지된 상태가 적힌 세이브 등).
        // 방치하면 클릭 switch·GetCompanyProgress·Demolish·부지 게이트가 전부 그 값에서 빠져나가
        // 건물이 못 사라지고 그 부지가 영구 잠긴다. 잔여가 남았으면 회수를 이어가는 쪽으로 되돌린다.
        if ((uint8)R.State > (uint8)EAcqState::Cleared)
        {
            UE_LOG(LogTemp, Warning, TEXT("[Acq] 알 수 없는 저장 상태 key=%d state=%d — 회수 상태로 복구"), It.Key, It.Value.State);
            R.State = (R.RRemaining > 0) ? EAcqState::Milking : EAcqState::Depleted;
        }
        Runtime.Add(It.Key, R);
        if (R.State == EAcqState::Cleared)
        {
            if (AActor* A = GetCompanyActor(It.Key)) { A->SetActorHiddenInGame(true); A->SetActorEnableCollision(false); }
        }
    }
}

void UCityAcquisitionManager::SettleOffline(float /*TotalGained*/, float OfflineSeconds)
{
    for (TPair<int32, FCityAcqRuntime>& It : Runtime)
    {
        FCityAcqRuntime& R = It.Value;
        if (R.State != EAcqState::Milking) { continue; }
        FCityCompanyData D;
        if (!TableMgr() || !TableMgr()->GetCityCompanyData(It.Key, D)) { continue; }
        const int64 Chunk = (int64)(D.AcquisitionCost * D.DripChunkPct / 100.0);
        const double CritFactor = 1.0 + D.CritChance * (D.CritMult - 1.0);
        const int64 Expected = (int64)(OfflineSeconds * D.DripChance * Chunk * CritFactor);
        // 오프라인도 온라인과 같은 속도로 100% 까지 — 회수는 방치로 굴러가는 게 본체다.
        // 굴림은 인수 시점에 끝났으므로 여기서 더 굴릴 것도, 막을 것도 없다.
        const int64 Accrue = FMath::Min(Expected, R.RRemaining);
        if (Accrue > 0) { R.RRemaining -= Accrue; } // 금고 적립만 — 오프라인도 지갑 직행 없음 (D8)
        if (R.RRemaining <= 0) { R.State = EAcqState::Depleted; } // 오프라인 고갈 -> 수동 철거 대기
    }
    SaveToGame();
}

void UCityAcquisitionManager::OnCompanyClicked(int32 Key)
{
    // 인수 대상 BP_MB는 Static mobility(베이크 라이팅)라 런타임 회전 불가 → 와블 생략 (사용자 결정 2026-06-27)
    switch (GetState(Key))
    {
    case EAcqState::NotAcquired:
    {
        // 인수 모달=중앙(0.5) 프레이밍. 매니지 패널(0.3)은 OpenManagePanel 이 자체 책임진다.
        if (AActor* CompanyActor = GetCompanyActor(Key))
        {
            APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
            if (APawn* CamPawn = PC ? PC->GetPawn() : nullptr)
            {
                if (APlayerCamera* Cam = Cast<APlayerCamera>(CamPawn))
                {
                    Cam->FocusOnActor(CompanyActor, 0.5f);
                }
            }
        }
        UGameInstance* GI = GetWorld()->GetGameInstance();
        UUIManagerSubsystem* UIM = GI ? GI->GetSubsystem<UUIManagerSubsystem>() : nullptr;
        UTableManagerSubsystem* TM = TableMgr();
        UUIBase* UIBase = UIM ? UIM->GetUIBase() : nullptr;
        if (!UIBase || !TM) { break; }
        if (UIBase->GetPromptStackCount() > 0) { break; }
        TSubclassOf<UUserWidget> Cls = TM->GetWidgetClass(EWidgetType::CityCompanyInfo);
        if (!Cls)
        {
            UE_LOG(LogTemp, Error, TEXT("[Acq] CityCompanyInfo widget class not found (DT_WidgetClass 행 확인)"));
            break;
        }
        if (UCityCompanyInfoWidget* Modal = Cast<UCityCompanyInfoWidget>(UIBase->PushPromptClass(Cls.Get())))
        {
            Modal->ConfigureForCompany(Key);
        }
        // 입력 모드는 Modal 의 NativeOnActivated 가 담당 — 여기서 GoToUIMode 금지
        break;
    }
    case EAcqState::Milking:
    case EAcqState::Depleted:
        OpenManagePanel(Key);
        break;
    default:
        break; // Cleared: 프록시 제거됨 -> 클릭 안 옴
    }
}

void UCityAcquisitionManager::OpenManagePanel(int32 Key)
{
    // 포커싱이 오픈과 한 몸이어야 호출자가 늘어도(재클릭 / 리빌 확인 체인 등) 매번 좌측 프레이밍이 보장된다.
    if (AActor* CompanyActor = GetCompanyActor(Key))
    {
        APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
        if (APawn* CamPawn = PC ? PC->GetPawn() : nullptr)
        {
            if (APlayerCamera* Cam = Cast<APlayerCamera>(CamPawn))
            {
                Cam->FocusOnActor(CompanyActor, 0.3f); // 매니지 패널=우측 도킹 → 좌측 프레이밍
            }
        }
    }

    UGameInstance* GI = GetWorld()->GetGameInstance();
    UUIManagerSubsystem* UIM = GI ? GI->GetSubsystem<UUIManagerSubsystem>() : nullptr;
    UTableManagerSubsystem* TM = TableMgr();
    UUIBase* UIBase = UIM ? UIM->GetUIBase() : nullptr;
    if (!UIBase || !TM) { return; }
    // 중복 push 방지는 입력모드로: 패널 NativeOnActivated 가 GoToUIMode → 프록시 클릭이 Normal 게이트에서 막힘.
    // (BottomStack 은 BuildOpen 바가 상주해 항상 >0 이므로 카운트 가드는 못 씀 — BuildingManagePanel 과 동일 패턴)
    TSubclassOf<UUserWidget> Cls = TM->GetWidgetClass(EWidgetType::CityCompanyManage);
    if (!Cls)
    {
        UE_LOG(LogTemp, Error, TEXT("[Acq] CityCompanyManage widget class not found (DT_WidgetClass 행 확인)"));
        return;
    }
    if (UCityCompanyManageWidget* P = Cast<UCityCompanyManageWidget>(UIBase->PushBottomClass(Cls.Get())))
    {
        P->ConfigureForCompany(Key);
    }
}
