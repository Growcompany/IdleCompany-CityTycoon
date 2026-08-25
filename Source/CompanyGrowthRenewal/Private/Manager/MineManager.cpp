// Fill out your copyright notice in the Description page of Project Settings.

#include "Manager/MineManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/PlayFabManagerSubsystem.h"
#include "Data/MineUpgradeData.h"
#include "Table/CountryInfoTable.h"
#include "Engine/GameInstance.h"

void UMineManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UMineManager::TickInternal), MineTickInterval);

	// PlayFab 서버 시간 권위 catchup — 빌딩/WorldFactory 와 같은 listener 패턴
	if (UPlayFabManagerSubsystem* PFMgr = GetGameInstance()->GetSubsystem<UPlayFabManagerSubsystem>())
	{
		PFMgr->OnOfflineGainsRequested.AddUObject(this, &UMineManager::ApplyServerOfflineCatchup);
	}
}

void UMineManager::Deinitialize()
{
	if (UPlayFabManagerSubsystem* PFMgr = GetGameInstance()->GetSubsystem<UPlayFabManagerSubsystem>())
	{
		PFMgr->OnOfflineGainsRequested.RemoveAll(this);
	}

	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
		TickerHandle.Reset();
	}
	Super::Deinitialize();
}

FCountryMineData& UMineManager::GetOrCreateMine(ECountryType Country)
{
	FCountryMineData& Mine = CountryMines.FindOrAdd(Country);
	if (Mine.UpgradeLevels.Num() == 0)
	{
		Mine.UpgradeLevels.Add(EMineUpgradeType::MiningRate, 0);
		Mine.UpgradeLevels.Add(EMineUpgradeType::MiningStorage, 0);
		Mine.UpgradeLevels.Add(EMineUpgradeType::LineExpansion, 0);
		Mine.UpgradeLevels.Add(EMineUpgradeType::OfflineCatchupBoost, 0);
		Mine.UpgradeLevels.Add(EMineUpgradeType::AutoClaim, 0);
	}
	return Mine;
}

const FCountryMineData* UMineManager::FindMine(ECountryType Country) const
{
	return CountryMines.Find(Country);
}

void UMineManager::EnsureCountryLines(ECountryType Country)
{
	// 옵션 B (슬롯 모델): 라인 자동 생성하지 않음. 사용자가 CreateMineLine 으로 명시 추가.
	// 여기서는 빈 데이터 + 강화 레벨 0 초기화만 보장.
	const UGameInstance* GI = GetGameInstance();
	if (!GI) return;
	const UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	if (!TableMgr) return;

	bool bSucc = false;
	const FCountryInfoTable Info = TableMgr->GetCountryInfo(Country, bSucc);
	if (!bSucc || !Info.bSupportsMine) return;

	GetOrCreateMine(Country);  // UpgradeLevels 만 0 으로 초기화. 라인은 비어있음.
}

bool UMineManager::CreateMineLine(ECountryType Country, EResourceType Resource, int64 OverrideMaxStorage)
{
	if (Resource == EResourceType::None) return false;

	UGameInstance* GI = GetGameInstance();
	if (!GI) return false;
	const UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	if (!TableMgr) return false;

	bool bSucc = false;
	const FCountryInfoTable Info = TableMgr->GetCountryInfo(Country, bSucc);
	if (!bSucc || !Info.bSupportsMine) return false;

	// 풀 검증: 호주 풀에 다이아몬드 없으면 거부 (국가 정체성)
	const int32 PoolIndex = Info.MinableResources.IndexOfByKey(Resource);
	if (PoolIndex == INDEX_NONE) return false;

	FCountryMineData& Mine = GetOrCreateMine(Country);

	// 슬롯 한도 검증
	if (Mine.ActiveLines.Num() >= GetMaxLines(Country)) return false;

	// 중복 체크
	if (FindLineIndexByResource(Mine, Resource) != INDEX_NONE) return false;

	// Country MaxStorage 가 hard ceiling — Override 가 그보다 크면 ceiling 으로 clamp.
	const int64 CountryCap = GetMaxStorage(Country);
	int64 ClampedOverride = 0;
	if (OverrideMaxStorage > 0)
	{
		ClampedOverride = (CountryCap > 0) ? FMath::Min<int64>(OverrideMaxStorage, CountryCap) : OverrideMaxStorage;
	}

	// 비용 검증/차감 — 라인의 effective max 기준 (Override 있으면 그 값, 없으면 Country cap).
	const int64 CostQuantity = (ClampedOverride > 0) ? ClampedOverride : CountryCap;
	const int64 Cost = GetCreateLineCost(Country, Resource, CostQuantity);
	if (Cost > 0)
	{
		UResourceItemManager* ResMgr = GI->GetSubsystem<UResourceItemManager>();
		if (!ResMgr || !ResMgr->HasResource(EResourceType::Money, Cost))
		{
			return false;
		}
		ResMgr->SpendResource(EResourceType::Money, Cost, /*bShouldSave=*/true);
	}

	FMineLineState NewLine;
	NewLine.Resource = Resource;
	NewLine.BaseRatePerSecond = ComputeBaseRatePerSecondByIndex(PoolIndex);
	NewLine.OverrideMaxStorage = ClampedOverride;
	NewLine.StartWallUtc = FDateTime::UtcNow();  // wall-clock lazy 모델 — 이 시점부터 누적 시간 계산
	// CurrentQty/bSaturated/PendingFraction 은 lazy 계산 — 저장 안 함
	Mine.ActiveLines.Add(NewLine);

	OnLineCreated.Broadcast(Country, NewLine);
	OnLineStorageChanged.Broadcast(Country, Resource, 0);
	return true;
}

int64 UMineManager::GetCreateLineCost(ECountryType Country, EResourceType Resource, int64 Quantity) const
{
	if (Quantity <= 0) return 0;

	const UGameInstance* GI = GetGameInstance();
	const UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableMgr) return 0;

	bool bSucc = false;
	const FResourceInfo Info = TableMgr->GetResourceInfo(Resource, bSucc);
	if (!bSucc || Info.BasePrice <= 0) return 0;

	return static_cast<int64>(Info.BasePrice) * Quantity;
}

TArray<ECountryType> UMineManager::GetActiveCountries() const
{
	TArray<ECountryType> Result;
	CountryMines.GetKeys(Result);
	return Result;
}

TArray<EResourceType> UMineManager::GetAvailableResources(ECountryType Country) const
{
	TArray<EResourceType> Result;

	const UGameInstance* GI = GetGameInstance();
	if (!GI) return Result;
	const UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	if (!TableMgr) return Result;

	bool bSucc = false;
	const FCountryInfoTable Info = TableMgr->GetCountryInfo(Country, bSucc);
	if (!bSucc) return Result;

	const FCountryMineData* Mine = FindMine(Country);

	for (EResourceType Resource : Info.MinableResources)
	{
		if (Resource == EResourceType::None) continue;
		// 이미 활성 라인인 자원은 제외
		if (Mine && FindLineIndexByResource(*Mine, Resource) != INDEX_NONE) continue;
		Result.Add(Resource);
	}
	return Result;
}

bool UMineManager::ClaimResource(ECountryType Country, EResourceType Resource)
{
	FCountryMineData* Mine = CountryMines.Find(Country);
	if (!Mine) return false;

	const int32 Idx = FindLineIndexByResource(*Mine, Resource);
	if (Idx == INDEX_NONE) return false;

	// wall-clock lazy 결과로 수령량 계산
	FMineLineState LazyState;
	if (!GetLineState(Country, Resource, LazyState)) return false;
	if (LazyState.CurrentQty <= 0) return false;

	const int64 Claimed = LazyState.CurrentQty;

	// 자원 인벤토리로 이전
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UResourceItemManager* ResMgr = GI->GetSubsystem<UResourceItemManager>())
		{
			ResMgr->StoreResource(Resource, Claimed, /*bShouldSave=*/true);
		}
	}

	// 주문형 채광 모델 — 수령 후 라인 제거 (자원/N 1회 비용 지불 후 1회 수확).
	Mine->ActiveLines.RemoveAt(Idx);

	OnResourceClaimed.Broadcast(Country, Resource, Claimed);
	OnLineRemoved.Broadcast(Country, Resource);
	return true;
}

bool UMineManager::InstantFinishLine(ECountryType Country, EResourceType Resource)
{
	FCountryMineData* Mine = CountryMines.Find(Country);
	if (!Mine) return false;

	const int32 Idx = FindLineIndexByResource(*Mine, Resource);
	if (Idx == INDEX_NONE) return false;

	const int64 LineMax = GetEffectiveMaxStorage(Country, Resource);
	if (LineMax <= 0) return false;

	FMineLineState& Line = Mine->ActiveLines[Idx];

	// wall-clock 모델 — StartWallUtc 를 과거로 점프시켜 즉시 LineMax 도달.
	// 다음 GetLineState 호출 시 lazy 계산이 LineMax 그대로 반환.
	const float RateMul = GetRateMultiplier(Country);
	const double EffectiveRatePerSec = static_cast<double>(Line.BaseRatePerSecond) * static_cast<double>(RateMul);
	if (EffectiveRatePerSec <= 0.0) return false;

	const double NeededSec = static_cast<double>(LineMax) / EffectiveRatePerSec;
	const FDateTime AlreadyDoneStart = FDateTime::UtcNow() - FTimespan::FromSeconds(NeededSec);

	if (Line.StartWallUtc > AlreadyDoneStart) return false;  // 이미 더 과거면 의미 없음 (한도 도달 상태)
	Line.StartWallUtc = AlreadyDoneStart;

	OnLineStorageChanged.Broadcast(Country, Resource, LineMax);
	OnLineSaturated.Broadcast(Country, Resource, true);
	return true;
}

TArray<FMineLineState> UMineManager::GetMineLines(ECountryType Country) const
{
	if (const FCountryMineData* Mine = FindMine(Country))
	{
		return Mine->ActiveLines;
	}
	return TArray<FMineLineState>();
}

bool UMineManager::GetLineState(ECountryType Country, EResourceType Resource, FMineLineState& OutState) const
{
	const FCountryMineData* Mine = FindMine(Country);
	if (!Mine) return false;
	const int32 Idx = FindLineIndexByResource(*Mine, Resource);
	if (Idx == INDEX_NONE) return false;

	OutState = Mine->ActiveLines[Idx];

	// Wall-clock lazy 계산 — 라인 시작 후 누적 시간 × Rate = 현재 누적량.
	// hidden/탭전환/레벨전환 무관, ticker 의존 폐기. PIE 재시작 후에도 wall-clock 차이로 자동 catch-up.
	const double Elapsed = (FDateTime::UtcNow() - OutState.StartWallUtc).GetTotalSeconds();
	const float RateMul = GetRateMultiplier(Country);
	const double EffectiveRatePerSec = static_cast<double>(OutState.BaseRatePerSecond) * static_cast<double>(RateMul);

	const int64 LineMax = GetEffectiveMaxStorage(Country, Resource);
	const double RawQty = (Elapsed > 0.0) ? Elapsed * EffectiveRatePerSec : 0.0;
	OutState.CurrentQty = FMath::Min<int64>(static_cast<int64>(FMath::FloorToDouble(RawQty)), LineMax);
	OutState.bSaturated = (LineMax > 0) && (OutState.CurrentQty >= LineMax);
	OutState.PendingFraction = OutState.bSaturated
		? 0.0f
		: static_cast<float>(RawQty - FMath::FloorToDouble(RawQty));

	return true;
}

int32 UMineManager::GetActiveLineCount(ECountryType Country) const
{
	if (const FCountryMineData* Mine = FindMine(Country))
	{
		return Mine->ActiveLines.Num();
	}
	return 0;
}

int32 UMineManager::GetUpgradeLevel(ECountryType Country, EMineUpgradeType UpgradeType) const
{
	if (const FCountryMineData* Mine = FindMine(Country))
	{
		if (const int32* Lv = Mine->UpgradeLevels.Find(UpgradeType))
		{
			return *Lv;
		}
	}
	return 0;
}

bool UMineManager::GetUpgradeDef(EMineUpgradeType Type, FMineUpgradeDefinition& OutDef) const
{
	const UGameInstance* GI = GetGameInstance();
	if (!GI) return false;
	const UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	if (!TableMgr) return false;
	return TableMgr->GetMineUpgradeDefinition(Type, OutDef);
}

bool UMineManager::UpgradeLevelUp(ECountryType Country, EMineUpgradeType UpgradeType)
{
	FCountryMineData& Mine = GetOrCreateMine(Country);
	int32& Level = Mine.UpgradeLevels.FindOrAdd(UpgradeType);

	// MaxLevel 가드 — DT 단일 진실. 0 = 무제한, 양수면 상한.
	FMineUpgradeDefinition Def;
	if (GetUpgradeDef(UpgradeType, Def) && Def.MaxLevel > 0 && Level >= Def.MaxLevel)
	{
		return false;
	}

	Level++;
	OnUpgradeChanged.Broadcast(Country, UpgradeType, Level);
	return true;
}

int64 UMineManager::GetUpgradeCost(ECountryType Country, EMineUpgradeType UpgradeType) const
{
	FMineUpgradeDefinition Def;
	if (!GetUpgradeDef(UpgradeType, Def))
	{
		return 0;
	}
	const int32 Level = GetUpgradeLevel(Country, UpgradeType);

	// double Pow 로 정확도 확보 + int64 범위 clamp (idle clicker 라 Lv 600+ 에서 오버플로 가능)
	const double CostD = static_cast<double>(Def.BaseCost)
		* FMath::Pow(static_cast<double>(Def.CostGrowthRate), static_cast<double>(Level));
	const double Clamped = FMath::Clamp(CostD, 0.0, static_cast<double>(MAX_int64));
	return static_cast<int64>(Clamped);
}

float UMineManager::GetRateMultiplier(ECountryType Country) const
{
	FMineUpgradeDefinition Def;
	if (!GetUpgradeDef(EMineUpgradeType::MiningRate, Def))
	{
		return 1.0f;
	}
	return 1.0f + Def.PerLevel * GetUpgradeLevel(Country, EMineUpgradeType::MiningRate);
}

int64 UMineManager::GetMaxStorage(ECountryType Country) const
{
	FMineUpgradeDefinition Def;
	if (!GetUpgradeDef(EMineUpgradeType::MiningStorage, Def))
	{
		return 0;
	}
	const int32 Level = GetUpgradeLevel(Country, EMineUpgradeType::MiningStorage);
	return static_cast<int64>(Def.BaseValue + Def.PerLevel * Level);
}

int64 UMineManager::GetEffectiveMaxStorage(ECountryType Country, EResourceType Resource) const
{
	if (Resource == EResourceType::None) return 0;
	const int64 CountryCap = GetMaxStorage(Country);

	const FCountryMineData* Mine = FindMine(Country);
	if (!Mine) return CountryCap;

	const int32 Idx = FindLineIndexByResource(*Mine, Resource);
	if (Idx == INDEX_NONE) return CountryCap;

	const int64 Override = Mine->ActiveLines[Idx].OverrideMaxStorage;
	if (Override <= 0) return CountryCap;

	// Country cap 이 hard ceiling — Override 가 더 커도 cap 우선. cap=0 이면 그대로 Override 통과.
	return (CountryCap > 0) ? FMath::Min<int64>(Override, CountryCap) : Override;
}

int32 UMineManager::GetMaxLines(ECountryType Country) const
{
	FMineUpgradeDefinition Def;
	if (!GetUpgradeDef(EMineUpgradeType::LineExpansion, Def))
	{
		return 1;  // DT 미연결 시 안전 기본값 (최소 라인 1개)
	}
	const int32 Level = GetUpgradeLevel(Country, EMineUpgradeType::LineExpansion);
	return FMath::Max(1, FMath::FloorToInt32(Def.BaseValue + Def.PerLevel * Level));
}

float UMineManager::GetOfflineCatchupBoost(ECountryType Country) const
{
	FMineUpgradeDefinition Def;
	if (!GetUpgradeDef(EMineUpgradeType::OfflineCatchupBoost, Def))
	{
		return 0.0f;
	}
	const int32 Level = GetUpgradeLevel(Country, EMineUpgradeType::OfflineCatchupBoost);
	const float Boost = Def.PerLevel * Level;
	return FMath::Clamp(Boost, 0.0f, OfflineCatchupBoostHardCap);
}

bool UMineManager::IsAutoClaimEnabled(ECountryType Country) const
{
	return GetUpgradeLevel(Country, EMineUpgradeType::AutoClaim) >= 1;
}

float UMineManager::GetEffectiveRatePerMinute(ECountryType Country, EResourceType Resource) const
{
	// 라인이 이미 있으면 라인의 BaseRatePerSecond, 없으면 풀 인덱스 기반 신규 rate (MinePicker preview 용).
	float BaseRate = 0.0f;
	FMineLineState State;
	if (GetLineState(Country, Resource, State))
	{
		BaseRate = State.BaseRatePerSecond;
	}
	else
	{
		const UGameInstance* GI = GetGameInstance();
		const UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
		if (!TableMgr) return 0.0f;

		bool bSucc = false;
		const FCountryInfoTable Info = TableMgr->GetCountryInfo(Country, bSucc);
		if (!bSucc) return 0.0f;

		const int32 PoolIdx = Info.MinableResources.IndexOfByKey(Resource);
		if (PoolIdx == INDEX_NONE) return 0.0f;
		BaseRate = ComputeBaseRatePerSecondByIndex(PoolIdx);
	}
	return BaseRate * GetRateMultiplier(Country) * 60.0f;
}

bool UMineManager::TickInternal(float DeltaTime)
{
	// Wall-clock lazy 모델 — CurrentQty 누적 계산 폐기. 이 ticker 는 AutoClaim 처리만 담당.
	// 1초마다 한 번씩 saturated 라인 체크 (매 frame 부담 0).
	static double AutoClaimAccum = 0.0;
	AutoClaimAccum += DeltaTime;
	if (AutoClaimAccum < 1.0) return true;
	AutoClaimAccum = 0.0;

	for (auto& Pair : CountryMines)
	{
		const ECountryType Country = Pair.Key;
		if (!IsAutoClaimEnabled(Country)) continue;

		// lazy GetLineState 로 현재 saturated 라인 수집 후 일괄 수령.
		TArray<EResourceType> ToAutoClaim;
		for (const FMineLineState& Line : Pair.Value.ActiveLines)
		{
			FMineLineState LazyState;
			if (GetLineState(Country, Line.Resource, LazyState) && LazyState.bSaturated && LazyState.CurrentQty > 0)
			{
				ToAutoClaim.Add(Line.Resource);
			}
		}
		for (EResourceType Res : ToAutoClaim)
		{
			ClaimResource(Country, Res);  // 라인 제거 + StoreResource + broadcast
		}
	}
	return true;
}

int32 UMineManager::FindLineIndexByResource(const FCountryMineData& Mine, EResourceType Resource) const
{
	for (int32 i = 0; i < Mine.ActiveLines.Num(); ++i)
	{
		if (Mine.ActiveLines[i].Resource == Resource)
		{
			return i;
		}
	}
	return INDEX_NONE;
}

float UMineManager::ComputeBaseRatePerSecondByIndex(int32 IndexInList)
{
	// 임시 룰: 0번(주력) = 2/min, 그 외(보조) = 1/min.
	// GDD_FACTORY 524-541 표의 예외(남아공 금=0.5, 다이아=0.3)는 추후 DT_CountryMineResource 로 분리.
	return (IndexInList == 0) ? (2.0f / 60.0f) : (1.0f / 60.0f);
}

// ===== SaveLoad =====

void UMineManager::SerializeForSave(TMap<ECountryType, FCountryMineData>& OutData)
{
	OutData = CountryMines;
}

void UMineManager::LoadFromSave(const TMap<ECountryType, FCountryMineData>& InData)
{
	CountryMines = InData;
	// PendingFraction 은 SaveGame 미적용이라 0 으로 시작 (struct 기본값) — 의도된 슬리피지.
	// 오프라인 catchup 은 PlayFab OnOfflineGainsRequested 가 별도로 처리 (ApplyServerOfflineCatchup).
}

void UMineManager::ApplyServerOfflineCatchup(float OfflineSeconds)
{
	// Wall-clock lazy 모델에선 별도 catchup 불필요 — StartWallUtc 와 UtcNow 차이로 자동 진행.
	// 단, OfflineCatchupBoost 강화 효과는 wall-clock 누적 시간을 1.0~1.5x 로 늘려야 적용됨.
	// → 각 라인의 StartWallUtc 를 (OfflineSeconds × Boost) 만큼 과거로 점프.
	if (OfflineSeconds <= 0.0f) return;

	for (auto& Pair : CountryMines)
	{
		const ECountryType Country = Pair.Key;
		FCountryMineData& Mine = Pair.Value;

		const float Boost = GetOfflineCatchupBoost(Country);  // 0.0~0.5
		if (Boost <= 0.0f) continue;

		// Boost 만큼만 추가 점프 (기본 1.0x 는 wall-clock 자체로 이미 적용).
		const double BonusSec = static_cast<double>(OfflineSeconds) * static_cast<double>(Boost);

		for (FMineLineState& Line : Mine.ActiveLines)
		{
			Line.StartWallUtc -= FTimespan::FromSeconds(BonusSec);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[Mine] Server offline catchup: %d countries, OfflineSec=%.0f (wall-clock 모델 → StartWallUtc 점프로 Boost 만 적용)"),
		CountryMines.Num(), OfflineSeconds);
}

// ===== Debug =====

void UMineManager::DebugFillAllLines(ECountryType Country)
{
	FCountryMineData* Mine = CountryMines.Find(Country);
	if (!Mine) return;

	// Wall-clock 모델 — InstantFinishLine 과 같은 패턴 (StartWallUtc 점프).
	for (FMineLineState& Line : Mine->ActiveLines)
	{
		InstantFinishLine(Country, Line.Resource);
	}
}

bool UMineManager::SeedLineProgress(ECountryType Country, EResourceType Resource, float ProgressRatio)
{
#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Warning, TEXT("[PresetSeeder] Mine SeedLineProgress 미구현 (country=%d res=%d ratio=%.2f)"),
		static_cast<int32>(Country), static_cast<int32>(Resource), ProgressRatio);
#endif
	return false;
}
