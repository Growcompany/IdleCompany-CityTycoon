// Fill out your copyright notice in the Description page of Project Settings.

#include "Manager/CountryMarketManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/ResourceItemManager.h"
#include "Table/CountryDemandTable.h"
#include "Table/MarketBalanceTable.h"
#include "Enum/ResourceType.h"
#include "Engine/GameInstance.h"

float UCountryMarketManager::CalculateGrowthMultiplier(int64 MarketCap) const
{
	// 공식: Mul = clamp(BaseMul + Coef * log10(1 + MC / PivotMC), BaseMul, MaxMul)
	// 파라미터는 DT_MarketBalance "Default" row에서 조회 (단일 튜닝 지점).
	const UGameInstance* GI = GetGameInstance();
	const UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableMgr) return 0.02f;  // DT 못 읽으면 안전한 기본값(struct 기본과 동일)

	const FMarketBalanceData& B = TableMgr->GetMarketBalance();
	const int64 SafePivot = FMath::Max<int64>(1, B.PivotMC);
	const double Ratio = static_cast<double>(FMath::Max<int64>(0, MarketCap)) / static_cast<double>(SafePivot);
	const double LogTerm = FMath::LogX(10.0, 1.0 + Ratio);
	const float Raw = B.BaseMul + B.Coef * static_cast<float>(LogTerm);
	return FMath::Clamp(Raw, B.BaseMul, B.MaxMul);
}

float UCountryMarketManager::GetCurrentGrowthMultiplier() const
{
	const UGameInstance* GI = GetGameInstance();
	const UResourceItemManager* ResourceMgr = GI ? GI->GetSubsystem<UResourceItemManager>() : nullptr;
	if (!ResourceMgr) return CalculateGrowthMultiplier(0);

	const int64 MC = ResourceMgr->GetResourceAmount(EResourceType::MarketCap);
	return CalculateGrowthMultiplier(MC);
}

void UCountryMarketManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// ResourceItemManager가 먼저 초기화되도록 명시 (GetCurrentGrowthMultiplier 안전성 확보)
	Collection.InitializeDependency<UResourceItemManager>();

	InitializeStatesFromDT();

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UResourceItemManager* ResourceMgr = GI->GetSubsystem<UResourceItemManager>())
		{
			ResourceMgr->OnResourceChanged.AddUObject(this, &UCountryMarketManager::HandleResourceChanged);
		}
	}

	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UCountryMarketManager::TickInternal),
		WorldMapConstants::TickIntervalSec);

	UE_LOG(LogTemp, Log, TEXT("[CountryMarketManager] Initialized (%d states)"), States.Num());
}

void UCountryMarketManager::Deinitialize()
{
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
		TickerHandle.Reset();
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UResourceItemManager* ResourceMgr = GI->GetSubsystem<UResourceItemManager>())
		{
			ResourceMgr->OnResourceChanged.RemoveAll(this);
		}
	}

	Super::Deinitialize();
}

void UCountryMarketManager::InitializeStatesFromDT()
{
	States.Empty();

	const UGameInstance* GI = GetGameInstance();
	const UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableMgr) return;

	CachedGrowthMul = GetCurrentGrowthMultiplier();

	const TArray<FCountryDemandTable> All = TableMgr->GetAllCountryDemands();
	for (const FCountryDemandTable& Row : All)
	{
		FCountryMarketState S;
		S.Country = Row.Country;
		S.Industry = Row.Industry;
		// Capacity는 진행도 스케일 적용 (최소 1 보장)
		S.Capacity = FMath::Max(1, FMath::FloorToInt(Row.Capacity * CachedGrowthMul));
		S.Current = S.Capacity;  // 만수요로 시작
		// RecoveryPerMin은 DT 그대로 (회복 시간은 진행도 무관하게 일정 페이스)
		S.RecoveryPerMin = Row.RecoveryPerMin;
		S.AccumulatorSec = 0.0f;
		States.Add(MakeKey(Row.Country, Row.Industry), S);
	}

	UE_LOG(LogTemp, Log, TEXT("[CountryMarketManager] States initialized with GrowthMul=%.3f (states=%d)"),
		CachedGrowthMul, States.Num());
}

void UCountryMarketManager::HandleResourceChanged(EResourceType Type, int64 NewValue)
{
	if (Type != EResourceType::MarketCap) return;

	const float NewGrowthMul = CalculateGrowthMultiplier(NewValue);
	// 0.5% 미만 변화는 무시 — 33셀 × Broadcast 폭증 방지
	if (FMath::IsNearlyEqual(NewGrowthMul, CachedGrowthMul, 0.005f)) return;

	const UGameInstance* GI = GetGameInstance();
	const UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableMgr) return;

	for (TPair<FIntPoint, FCountryMarketState>& Pair : States)
	{
		FCountryMarketState& S = Pair.Value;
		bool bFound = false;
		const FCountryDemandTable Row = TableMgr->GetCountryDemand(S.Country, S.Industry, bFound);
		if (!bFound) continue;

		const int32 OldCapacity = S.Capacity;
		const int32 NewCapacity = FMath::Max(1, FMath::FloorToInt(Row.Capacity * NewGrowthMul));
		if (NewCapacity == OldCapacity) continue;

		// 비율 보존: Current를 같은 비율로 스케일 → DemandMul 유지
		const float Ratio = OldCapacity > 0 ? static_cast<float>(S.Current) / static_cast<float>(OldCapacity) : 1.0f;
		S.Capacity = NewCapacity;
		S.Current = FMath::Clamp(FMath::FloorToInt(NewCapacity * Ratio), 0, NewCapacity);

		OnDemandChanged.Broadcast(S.Country, S.Industry, S.GetRatio());
	}

	UE_LOG(LogTemp, Log, TEXT("[CountryMarketManager] GrowthMul rescaled: %.3f -> %.3f (MC=%lld)"),
		CachedGrowthMul, NewGrowthMul, NewValue);
	CachedGrowthMul = NewGrowthMul;
}

bool UCountryMarketManager::TickInternal(float DeltaTime)
{
	// 모바일 서스펜드 등 큰 DeltaTime 클램프 (오프라인 회복은 LoadStates에서 별도 처리)
	const float ClampedDelta = FMath::Min(DeltaTime, 60.0f);

	for (TPair<FIntPoint, FCountryMarketState>& Pair : States)
	{
		FCountryMarketState& S = Pair.Value;
		if (S.Current >= S.Capacity) continue;  // 이미 만수요면 skip

		S.AccumulatorSec += ClampedDelta;

		// 분당 회복량 → 초당 환산 후 누적분만 적용
		const float UnitsToRecover = (S.RecoveryPerMin / 60.0f) * S.AccumulatorSec;
		const int32 IntUnits = FMath::FloorToInt(UnitsToRecover);
		if (IntUnits > 0)
		{
			const int32 Before = S.Current;
			S.Current = FMath::Min(S.Capacity, S.Current + IntUnits);
			S.AccumulatorSec -= IntUnits / (S.RecoveryPerMin / 60.0f);

			if (S.Current != Before)
			{
				OnDemandChanged.Broadcast(S.Country, S.Industry, S.GetRatio());
			}
		}
	}

	return true;
}

float UCountryMarketManager::GetDemandRatio(ECountryType Country, ECompanyType Industry) const
{
	if (const FCountryMarketState* S = States.Find(MakeKey(Country, Industry)))
	{
		return S->GetRatio();
	}
	return 1.0f;  // 미정의 셀은 만수요로 간주 (영향 없음)
}

float UCountryMarketManager::GetDemandMul(ECountryType Country, ECompanyType Industry) const
{
	const float Ratio = GetDemandRatio(Country, Industry);
	// 0.4× (포화) ~ 1.2× (만수요) 선형
	return 0.4f + 0.8f * Ratio;
}

void UCountryMarketManager::ConsumeDemand(ECountryType Country, ECompanyType Industry, int64 Qty)
{
	if (Qty <= 0) return;

	FCountryMarketState* S = States.Find(MakeKey(Country, Industry));
	if (!S) return;

	const int32 Before = S->Current;
	S->Current = FMath::Max(0, S->Current - static_cast<int32>(FMath::Min<int64>(Qty, MAX_int32)));

	if (S->Current != Before)
	{
		OnDemandChanged.Broadcast(Country, Industry, S->GetRatio());
	}
}

FCountryMarketState UCountryMarketManager::GetMarketState(ECountryType Country, ECompanyType Industry, bool& bOutFound) const
{
	if (const FCountryMarketState* S = States.Find(MakeKey(Country, Industry)))
	{
		bOutFound = true;
		return *S;
	}
	bOutFound = false;
	return FCountryMarketState();
}

TArray<FCountryMarketState> UCountryMarketManager::GetAllStates() const
{
	TArray<FCountryMarketState> Out;
	Out.Reserve(States.Num());
	for (const TPair<FIntPoint, FCountryMarketState>& Pair : States)
	{
		Out.Add(Pair.Value);
	}
	return Out;
}

void UCountryMarketManager::LoadStates(const TArray<FCountryMarketState>& InStates, const FDateTime& LastSaveTime)
{
	// 외부 상태로 덮어쓰기 (DT 셀에 없는 키는 무시).
	// 주의: InitializeStatesFromDT가 이미 GrowthMul 적용된 Capacity로 셀을 만들어 둠.
	//       SaveGame은 Current/Accumulator만 가져옴 — Capacity/RecoveryPerMin은 DT × 현재 GrowthMul 우선.
	for (const FCountryMarketState& Loaded : InStates)
	{
		const FIntPoint Key = MakeKey(Loaded.Country, Loaded.Industry);
		FCountryMarketState* Existing = States.Find(Key);
		if (!Existing) continue;
		Existing->Current = FMath::Clamp(Loaded.Current, 0, Existing->Capacity);
		Existing->AccumulatorSec = Loaded.AccumulatorSec;
	}

	// 오프라인 회복 시뮬레이션
	const FDateTime Now = FDateTime::UtcNow();
	if (LastSaveTime.GetTicks() > 0 && Now > LastSaveTime)
	{
		const FTimespan Elapsed = Now - LastSaveTime;
		// 24h 캡 (WorldMapConstants::OfflineCatchupMaxSec 재사용)
		const double ElapsedSec = FMath::Min(Elapsed.GetTotalSeconds(), static_cast<double>(WorldMapConstants::OfflineCatchupMaxSec));

		for (TPair<FIntPoint, FCountryMarketState>& Pair : States)
		{
			FCountryMarketState& S = Pair.Value;
			if (S.Current >= S.Capacity) continue;

			const double RecoverFloat = (S.RecoveryPerMin / 60.0) * ElapsedSec;
			const int32 RecoverInt = FMath::FloorToInt(RecoverFloat);
			S.Current = FMath::Min(S.Capacity, S.Current + RecoverInt);
		}
	}
}
