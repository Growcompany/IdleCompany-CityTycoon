// Fill out your copyright notice in the Description page of Project Settings.

#include "Manager/WorldFactoryManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/PlayFabManagerSubsystem.h"
#include "Data/WorldFactoryUpgradeData.h"
#include "Engine/GameInstance.h"

void UWorldFactoryManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UWorldFactoryManager::TickInternal), 0.25f);

	// PlayFab 서버 시간 권위 catchup — SaveLoadManager 와 같은 listener 패턴
	if (UPlayFabManagerSubsystem* PFMgr = GetGameInstance()->GetSubsystem<UPlayFabManagerSubsystem>())
	{
		PFMgr->OnOfflineGainsRequested.AddUObject(this, &UWorldFactoryManager::ApplyServerOfflineCatchup);
	}
}

void UWorldFactoryManager::Deinitialize()
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

FCountryFactoryData& UWorldFactoryManager::GetOrCreateFactory(ECountryType Country)
{
	FCountryFactoryData& Factory = CountryFactories.FindOrAdd(Country);
	if (Factory.UpgradeLevels.Num() == 0)
	{
		Factory.UpgradeLevels.Add(EWorldFactoryUpgradeType::LineExpansion, 0);
		Factory.UpgradeLevels.Add(EWorldFactoryUpgradeType::Automation, 0);
		Factory.UpgradeLevels.Add(EWorldFactoryUpgradeType::QualityControl, 0);
		Factory.UpgradeLevels.Add(EWorldFactoryUpgradeType::StorageExpansion, 0);
		Factory.UpgradeLevels.Add(EWorldFactoryUpgradeType::RushProduction, 0);
	}
	return Factory;
}

const FCountryFactoryData* UWorldFactoryManager::FindFactory(ECountryType Country) const
{
	return CountryFactories.Find(Country);
}

int32 UWorldFactoryManager::StartProduction(ECountryType Country, int32 ProductIndex, const FText& ProductName,
	const TSoftObjectPtr<UTexture2D>& IconPath, float BaseRatePerSecond, int64 TargetQty)
{
	if (TargetQty <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WorldFactory] StartProduction 실패: TargetQty=%lld (<= 0)"), TargetQty);
		return INDEX_NONE;
	}

	FCountryFactoryData& Factory = GetOrCreateFactory(Country);
	const int32 MaxLines = GetMaxLines(Country);
	if (Factory.ActiveLines.Num() >= MaxLines)
	{
		const UEnum* CountryEnum = StaticEnum<ECountryType>();
		const FString CountryStr = CountryEnum ? CountryEnum->GetNameStringByValue(static_cast<int64>(Country)) : TEXT("?");
		UE_LOG(LogTemp, Warning, TEXT("[WorldFactory] StartProduction 실패: %s 라인 가득 (Active=%d / Max=%d). MaxLines=0 이면 DT_WorldFactoryUpgradeDefinition 의 LineExpansion row 확인."),
			*CountryStr, Factory.ActiveLines.Num(), MaxLines);
		return INDEX_NONE;
	}

	FWorldFactoryLineState Line;
	Line.LineId = Factory.NextLineId++;
	Line.ProductIndex = ProductIndex;
	Line.ProductName = ProductName;
	Line.IconPath = IconPath;
	Line.RatePerSecond = BaseRatePerSecond * GetSpeedMultiplier(Country);
	Line.TargetQty = TargetQty;
	Line.StartWallUtc = FDateTime::UtcNow();  // wall-clock lazy 모델 — 이 시점부터 누적 시간 계산
	// CurrentQty/LineElapsedSec/bCompleted 는 lazy 계산 — 저장 안 함

	Factory.ActiveLines.Add(Line);
	OnLineStarted.Broadcast(Country, Line);
	return Line.LineId;
}

bool UWorldFactoryManager::InstantFinishLine(ECountryType Country, int32 LineId)
{
	FCountryFactoryData* Factory = CountryFactories.Find(Country);
	if (!Factory) return false;

	const int32 Idx = FindLineIndexById(*Factory, LineId);
	if (Idx == INDEX_NONE) return false;

	FWorldFactoryLineState& Line = Factory->ActiveLines[Idx];
	if (Line.RatePerSecond <= 0.0f) return false;

	// Wall-clock 모델 — StartWallUtc 를 과거로 점프시켜 즉시 TargetQty 도달.
	const double NeededSec = static_cast<double>(Line.TargetQty) / static_cast<double>(Line.RatePerSecond);
	const FDateTime AlreadyDoneStart = FDateTime::UtcNow() - FTimespan::FromSeconds(NeededSec);

	if (Line.StartWallUtc <= AlreadyDoneStart) return false;  // 이미 완료 상태
	Line.StartWallUtc = AlreadyDoneStart;

	OnLineCompleted.Broadcast(Country, LineId);
	return true;
}

bool UWorldFactoryManager::ClaimLine(ECountryType Country, int32 LineId)
{
	FCountryFactoryData* Factory = CountryFactories.Find(Country);
	if (!Factory) return false;

	const int32 Idx = FindLineIndexById(*Factory, LineId);
	if (Idx == INDEX_NONE) return false;

	// wall-clock lazy 결과로 완료 여부 + 수령량 결정
	FWorldFactoryLineState LazyState;
	if (!GetLineState(Country, LineId, LazyState)) return false;
	if (!LazyState.bCompleted) return false;

	Factory->ActiveLines.RemoveAt(Idx);
	OnLineClaimed.Broadcast(Country, LineId, LazyState.CurrentQty);
	return true;
}

TArray<FWorldFactoryLineState> UWorldFactoryManager::GetActiveLines(ECountryType Country) const
{
	if (const FCountryFactoryData* Factory = FindFactory(Country))
	{
		return Factory->ActiveLines;
	}
	return TArray<FWorldFactoryLineState>();
}

int32 UWorldFactoryManager::GetActiveLineCount(ECountryType Country) const
{
	if (const FCountryFactoryData* Factory = FindFactory(Country))
	{
		return Factory->ActiveLines.Num();
	}
	return 0;
}

TArray<ECountryType> UWorldFactoryManager::GetActiveCountries() const
{
	TArray<ECountryType> Result;
	CountryFactories.GetKeys(Result);
	return Result;
}

bool UWorldFactoryManager::GetLineState(ECountryType Country, int32 LineId, FWorldFactoryLineState& OutState) const
{
	const FCountryFactoryData* Factory = FindFactory(Country);
	if (!Factory) return false;

	const int32 Idx = FindLineIndexById(*Factory, LineId);
	if (Idx == INDEX_NONE) return false;

	OutState = Factory->ActiveLines[Idx];

	// Wall-clock lazy 계산 — 라인 시작 후 wall-clock 흐른 시간 × Rate = 현재 누적량.
	// hidden/탭전환/레벨전환/PIE재시작 무관 자동 catch-up. ticker 의존 폐기.
	const double Elapsed = (FDateTime::UtcNow() - OutState.StartWallUtc).GetTotalSeconds();
	OutState.LineElapsedSec = FMath::Max(Elapsed, 0.0);

	const double RawQty = OutState.LineElapsedSec * static_cast<double>(OutState.RatePerSecond);
	OutState.CurrentQty = FMath::Min<int64>(static_cast<int64>(FMath::FloorToDouble(RawQty)), OutState.TargetQty);
	OutState.bCompleted = (OutState.TargetQty > 0) && (OutState.CurrentQty >= OutState.TargetQty);

	return true;
}

int32 UWorldFactoryManager::GetUpgradeLevel(ECountryType Country, EWorldFactoryUpgradeType UpgradeType) const
{
	if (const FCountryFactoryData* Factory = FindFactory(Country))
	{
		if (const int32* Lv = Factory->UpgradeLevels.Find(UpgradeType))
		{
			return *Lv;
		}
	}
	return 0;
}

bool UWorldFactoryManager::GetUpgradeDef(EWorldFactoryUpgradeType Type, FWorldFactoryUpgradeDefinition& OutDef) const
{
	const UGameInstance* GI = GetGameInstance();
	if (!GI) return false;
	const UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	if (!TableMgr) return false;
	return TableMgr->GetWorldFactoryUpgradeDefinition(Type, OutDef);
}

bool UWorldFactoryManager::UpgradeLevelUp(ECountryType Country, EWorldFactoryUpgradeType UpgradeType)
{
	FCountryFactoryData& Factory = GetOrCreateFactory(Country);
	int32& Level = Factory.UpgradeLevels.FindOrAdd(UpgradeType);

	// MaxLevel 가드 — DT 단일 진실. 0 = 무제한, 양수면 상한.
	FWorldFactoryUpgradeDefinition Def;
	if (GetUpgradeDef(UpgradeType, Def) && Def.MaxLevel > 0 && Level >= Def.MaxLevel)
	{
		return false;
	}

	Level++;
	OnUpgradeChanged.Broadcast(Country, UpgradeType, Level);
	return true;
}

int32 UWorldFactoryManager::GetMaxLines(ECountryType Country) const
{
	FWorldFactoryUpgradeDefinition Def;
	if (!GetUpgradeDef(EWorldFactoryUpgradeType::LineExpansion, Def))
	{
		UE_LOG(LogTemp, Warning, TEXT("[WorldFactory] DT_WorldFactoryUpgradeDefinition 의 LineExpansion row 미발견. CSV reimport 또는 DT 등록 확인 필요."));
		return 0;
	}
	const int32 Level = GetUpgradeLevel(Country, EWorldFactoryUpgradeType::LineExpansion);
	return FMath::FloorToInt32(Def.BaseValue + Def.PerLevel * Level);
}

float UWorldFactoryManager::GetSpeedMultiplier(ECountryType Country) const
{
	FWorldFactoryUpgradeDefinition Def;
	if (!GetUpgradeDef(EWorldFactoryUpgradeType::Automation, Def))
	{
		return 1.0f;
	}
	return 1.0f + Def.PerLevel * GetUpgradeLevel(Country, EWorldFactoryUpgradeType::Automation);
}

float UWorldFactoryManager::GetRewardMultiplier(ECountryType Country) const
{
	FWorldFactoryUpgradeDefinition Def;
	if (!GetUpgradeDef(EWorldFactoryUpgradeType::QualityControl, Def))
	{
		return 1.0f;
	}
	return 1.0f + Def.PerLevel * GetUpgradeLevel(Country, EWorldFactoryUpgradeType::QualityControl);
}

int64 UWorldFactoryManager::GetVaultCapacity(ECountryType Country) const
{
	FWorldFactoryUpgradeDefinition Def;
	if (!GetUpgradeDef(EWorldFactoryUpgradeType::StorageExpansion, Def))
	{
		return 0;
	}
	const int32 Level = GetUpgradeLevel(Country, EWorldFactoryUpgradeType::StorageExpansion);
	return static_cast<int64>(Def.BaseValue + Def.PerLevel * Level);
}

float UWorldFactoryManager::GetInstantFinishDiscount(ECountryType Country) const
{
	FWorldFactoryUpgradeDefinition Def;
	if (!GetUpgradeDef(EWorldFactoryUpgradeType::RushProduction, Def))
	{
		return 0.0f;
	}
	const float Disc = Def.PerLevel * GetUpgradeLevel(Country, EWorldFactoryUpgradeType::RushProduction);
	return FMath::Clamp(Disc, 0.0f, RushDiscountHardCap);
}

int64 UWorldFactoryManager::GetUpgradeCost(ECountryType Country, EWorldFactoryUpgradeType UpgradeType) const
{
	FWorldFactoryUpgradeDefinition Def;
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

void UWorldFactoryManager::SerializeForSave(TMap<ECountryType, FCountryFactoryData>& OutData) const
{
	// LineElapsedSec 모델은 절대 timestamp 가 아니라 catchup 자동 흐름 없음 — 단순 카피
	OutData = CountryFactories;
}

void UWorldFactoryManager::LoadFromSave(const TMap<ECountryType, FCountryFactoryData>& InData)
{
	CountryFactories = InData;
	// 진행도는 LineElapsedSec 누적치 그대로 — 게임 종료 동안 추가 흐름 없음.
	// 오프라인 보상은 PlayFab 의 ApplyServerOfflineCatchup 가 별도로 처리.
}

void UWorldFactoryManager::ApplyServerOfflineCatchup(float OfflineSeconds)
{
	// Wall-clock lazy 모델 — 기본 1.0x 는 wall-clock 차이로 이미 자동 적용.
	// 추가 OfflineEfficiencyMultiplier (0.2x) 효율 보정 = 점프 양 = OfflineSec × 0.2 만큼 추가 진행.
	// 단 효율이 1.0 미만이면 wall-clock 자체가 너무 빠르니 음수 점프로 보정 필요.
	// 현재 OfflineEfficiencyMultiplier=0.2 → 자체 진행분의 (0.2-1.0) = -0.8x 보정 필요 → 양 차이 = -OfflineSec × 0.8 만큼 미래로 (덜 진행).
	if (OfflineSeconds <= 0.0f) return;

	const double AdjustSec = static_cast<double>(OfflineSeconds) * (OfflineEfficiencyMultiplier - 1.0);  // 음수

	for (auto& Pair : CountryFactories)
	{
		for (FWorldFactoryLineState& Line : Pair.Value.ActiveLines)
		{
			// AdjustSec 음수 → StartWallUtc 를 미래로 (덜 진행한 상태로 보정)
			Line.StartWallUtc -= FTimespan::FromSeconds(AdjustSec);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[WorldFactory] Server offline catchup: %d countries, OfflineSec=%.0f Adjust=%.0fs (efficiency=%.0f%%)"),
		CountryFactories.Num(), OfflineSeconds, AdjustSec, OfflineEfficiencyMultiplier * 100.0);
}

bool UWorldFactoryManager::TickInternal(float /*DeltaTime*/)
{
	// Wall-clock lazy 모델 — 별도 tick 필요 없음 (GetLineState 가 동적 계산).
	// ticker 자체는 폐기 안 하고 비우기만 함. 향후 broadcast 트리거 등 확장 여지.
	return true;
}

int32 UWorldFactoryManager::FindLineIndexById(const FCountryFactoryData& Factory, int32 LineId) const
{
	for (int32 i = 0; i < Factory.ActiveLines.Num(); ++i)
	{
		if (Factory.ActiveLines[i].LineId == LineId)
		{
			return i;
		}
	}
	return INDEX_NONE;
}

// ApplyProgressToLine 폐기 — wall-clock lazy 모델에서 GetLineState 가 직접 계산.

bool UWorldFactoryManager::SeedLineProgress(ECountryType Country, int32 LineId, float ProgressRatio)
{
#if !UE_BUILD_SHIPPING
	// 진행도의 유일한 원천은 라인 시작 시각이라 백데이트가 유일한 경로다. 자원 선행이 필요해 후속 작업으로 남긴다.
	UE_LOG(LogTemp, Warning, TEXT("[PresetSeeder] WorldFactory SeedLineProgress 미구현 (country=%d line=%d ratio=%.2f)"),
		static_cast<int32>(Country), LineId, ProgressRatio);
#endif
	return false;
}
