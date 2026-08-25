// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "Entity/Country/CountryActor.h"
#include "Enum/ResourceType.h"
#include "MineManager.generated.h"

class UTexture2D;

/**
 * 채광 강화 종류. UIE_UpgradeSlotHorizon 슬롯 이름과 매핑.
 * GDD_FACTORY.md A-1 (자동 채취 속도) + A-2 (저장 한도) 2종.
 * 메타데이터/밸런스는 DT_MineUpgradeDefinition (FMineUpgradeDefinition) 단일 진실 원천.
 */
UENUM(BlueprintType)
enum class EMineUpgradeType : uint8
{
	None                   UMETA(Hidden),
	MiningRate             UMETA(DisplayName = "채광 속도"),
	MiningStorage          UMETA(DisplayName = "저장 한도"),
	LineExpansion          UMETA(DisplayName = "라인 증설"),
	OfflineCatchupBoost    UMETA(DisplayName = "오프라인 효율"),
	AutoClaim              UMETA(DisplayName = "자동 수령"),
};

/**
 * 자원 라인 1개의 상태. 자원 종류별로 1라인.
 * 공장 라인과 달리 영구 상시 라인 — 한도 도달 시 정지하지만 사라지지 않음.
 */
USTRUCT(BlueprintType)
struct FMineLineState
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadOnly)
	EResourceType Resource = EResourceType::None;

	// 분당 기본 채취량 / 60. 강화로 배율 적용된 실효치는 매니저의 GetEffectiveRatePerMinute 참조.
	UPROPERTY(SaveGame, BlueprintReadOnly)
	float BaseRatePerSecond = 0.0f;

	// 라인별 한도 오버라이드. 0 이면 국가 MiningStorage 강화 값을 사용.
	UPROPERTY(SaveGame, BlueprintReadOnly)
	int64 OverrideMaxStorage = 0;

	// Wall-clock lazy 모델의 핵심 — 라인 생성 시점 UTC.
	// GetLineState 호출 시 (UtcNow - StartWallUtc) * Rate 로 CurrentQty 동적 계산.
	// SaveGame 마킹 → PIE 재시작 후 자동 catch-up (wall-clock 차이만큼 진행).
	UPROPERTY(SaveGame, BlueprintReadOnly)
	FDateTime StartWallUtc = FDateTime(0);

	// ── 아래 3개는 GetLineState 의 OutState 채우기용 (lazy 계산 결과). 직접 저장 안 함. ──

	UPROPERTY(BlueprintReadOnly)
	int64 CurrentQty = 0;

	UPROPERTY(BlueprintReadOnly)
	bool bSaturated = false;

	// 표시용 — 다음 1개까지 진행도 (0~1).
	UPROPERTY()
	float PendingFraction = 0.0f;
};

/**
 * 국가 1개의 채광 데이터.
 */
USTRUCT(BlueprintType)
struct FCountryMineData
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadOnly)
	TArray<FMineLineState> ActiveLines;

	UPROPERTY(SaveGame, BlueprintReadOnly)
	TMap<EMineUpgradeType, int32> UpgradeLevels;
};

/**
 * UMineManager
 * 월드맵 국가별 채광 백엔드. 자원이 분당 N개씩 자동 누적, 저장 한도 도달 시 정지.
 *
 * 모든 API는 ECountryType을 첫 인자로 받아 해당 국가 채광장을 조작.
 * UWorldFactoryManager(배치 모델)와 다른 연속 누적 모델.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UMineManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMineLineCreated,
		ECountryType, Country, FMineLineState, LineState);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnMineLineStorageChanged,
		ECountryType, Country, EResourceType, Resource, int64, NewQty);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnMineLineSaturated,
		ECountryType, Country, EResourceType, Resource, bool, bSaturated);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnMineResourceClaimed,
		ECountryType, Country, EResourceType, Resource, int64, ClaimedQty);
	// 수령 후 라인이 ActiveLines 에서 제거됨 — UI 가 위젯 destroy 트리거.
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMineLineRemoved,
		ECountryType, Country, EResourceType, Resource);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnMineUpgradeChanged,
		ECountryType, Country, EMineUpgradeType, UpgradeType, int32, NewLevel);

	UPROPERTY(BlueprintAssignable, Category = "Mine|Events")
	FOnMineLineCreated OnLineCreated;

	UPROPERTY(BlueprintAssignable, Category = "Mine|Events")
	FOnMineLineStorageChanged OnLineStorageChanged;

	UPROPERTY(BlueprintAssignable, Category = "Mine|Events")
	FOnMineLineSaturated OnLineSaturated;

	UPROPERTY(BlueprintAssignable, Category = "Mine|Events")
	FOnMineResourceClaimed OnResourceClaimed;

	UPROPERTY(BlueprintAssignable, Category = "Mine|Events")
	FOnMineLineRemoved OnLineRemoved;

	UPROPERTY(BlueprintAssignable, Category = "Mine|Events")
	FOnMineUpgradeChanged OnUpgradeChanged;

	// UE5 기본 자동 등록이 일부 환경에서 skip 되는 케이스 차단 — 명시적 true 반환으로 강제 등록.
	// (프로젝트 컨벤션: ProjectOperationManager / ProductionOrderManager 와 동일 패턴)
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// 국가 데이터 초기화 (라인은 자동 생성하지 않음, 사용자가 CreateMineLine 으로 명시 추가).
	UFUNCTION(BlueprintCallable, Category = "Mine")
	void EnsureCountryLines(ECountryType Country);

	// 라인 신규 생성 (슬롯 구조). MaxLines 한도 + Resource pool 검증 + 중복 체크 후 추가.
	// OverrideMaxStorage>0 이면 라인별 한도. 0 이면 국가 MiningStorage 강화 값 사용.
	// Country MaxStorage 가 hard ceiling — Override 가 그보다 크면 ceiling 으로 clamp.
	UFUNCTION(BlueprintCallable, Category = "Mine")
	bool CreateMineLine(ECountryType Country, EResourceType Resource, int64 OverrideMaxStorage = 0);

	// 풀에 있지만 아직 활성화 안 된 자원 목록 (Picker UI 채우기용).
	UFUNCTION(BlueprintPure, Category = "Mine")
	TArray<EResourceType> GetAvailableResources(ECountryType Country) const;

	// 채광 데이터가 등록된 모든 국가 반환 — CollectAll 같은 전역 순회에 사용.
	UFUNCTION(BlueprintPure, Category = "Mine")
	TArray<ECountryType> GetActiveCountries() const;

	UFUNCTION(BlueprintCallable, Category = "Mine")
	bool ClaimResource(ECountryType Country, EResourceType Resource);

	// 한도까지 즉시 채움 (다이아 결제 등 비용은 호출자 책임 — 매니저는 상태 변경만).
	// 이미 saturated 또는 MaxStorage<=0 이면 false 반환.
	UFUNCTION(BlueprintCallable, Category = "Mine")
	bool InstantFinishLine(ECountryType Country, EResourceType Resource);

	// [dev 프리셋] 채광 라인 적재량을 한도의 임의 비율로 만든다 (InstantFinishLine 은 100% 로만 점프).
	bool SeedLineProgress(ECountryType Country, EResourceType Resource, float ProgressRatio);

	UFUNCTION(BlueprintPure, Category = "Mine")
	TArray<FMineLineState> GetMineLines(ECountryType Country) const;

	UFUNCTION(BlueprintPure, Category = "Mine")
	bool GetLineState(ECountryType Country, EResourceType Resource, FMineLineState& OutState) const;

	UFUNCTION(BlueprintPure, Category = "Mine")
	int32 GetActiveLineCount(ECountryType Country) const;

	// ── 강화 (Country별) ──
	UFUNCTION(BlueprintPure, Category = "Mine|Upgrade")
	int32 GetUpgradeLevel(ECountryType Country, EMineUpgradeType UpgradeType) const;

	UFUNCTION(BlueprintCallable, Category = "Mine|Upgrade")
	bool UpgradeLevelUp(ECountryType Country, EMineUpgradeType UpgradeType);

	UFUNCTION(BlueprintPure, Category = "Mine|Upgrade")
	int64 GetUpgradeCost(ECountryType Country, EMineUpgradeType UpgradeType) const;

	UFUNCTION(BlueprintPure, Category = "Mine|Upgrade")
	float GetRateMultiplier(ECountryType Country) const;

	UFUNCTION(BlueprintPure, Category = "Mine|Upgrade")
	int64 GetMaxStorage(ECountryType Country) const;

	// 라인 1개의 실효 한도. OverrideMaxStorage>0 이면 그 값(Country MaxStorage 로 clamp), 아니면 Country MaxStorage.
	// 라인이 없거나 Resource==None 이면 0 반환.
	UFUNCTION(BlueprintPure, Category = "Mine|Upgrade")
	int64 GetEffectiveMaxStorage(ECountryType Country, EResourceType Resource) const;

	// 동시 활성 가능한 라인 수 (LineExpansion 강화로 증가).
	UFUNCTION(BlueprintPure, Category = "Mine|Upgrade")
	int32 GetMaxLines(ECountryType Country) const;

	// 오프라인 catchup 효율 보너스 (0.0~0.5). 1.0x 기본에 곱해짐 → 강화 시 최대 1.5x.
	UFUNCTION(BlueprintPure, Category = "Mine|Upgrade")
	float GetOfflineCatchupBoost(ECountryType Country) const;

	// 자동 수령 활성 여부 (마일스톤 강화). 활성 시 saturated 라인이 매 tick 자동 ClaimResource.
	UFUNCTION(BlueprintPure, Category = "Mine|Upgrade")
	bool IsAutoClaimEnabled(ECountryType Country) const;

	UFUNCTION(BlueprintPure, Category = "Mine")
	float GetEffectiveRatePerMinute(ECountryType Country, EResourceType Resource) const;

	// 라인 생성 비용 = ResourceInfo.BasePrice * Quantity. BasePrice 0 이면 무료.
	// MinePicker 카드 표시 + CreateMineLine 차감 양쪽에서 사용 (단일 진실 원천).
	UFUNCTION(BlueprintPure, Category = "Mine")
	int64 GetCreateLineCost(ECountryType Country, EResourceType Resource, int64 Quantity) const;

	// ── SaveLoad (SaveLoadManager 전용) ──
	// 자체 catchup 폐기 — PlayFab OnOfflineGainsRequested 가 권위. 단순 TMap 카피만.
	UFUNCTION(BlueprintCallable, Category = "Mine|SaveLoad")
	void SerializeForSave(TMap<ECountryType, FCountryMineData>& OutData);

	UFUNCTION(BlueprintCallable, Category = "Mine|SaveLoad")
	void LoadFromSave(const TMap<ECountryType, FCountryMineData>& InData);

	// PlayFab OnOfflineGainsRequested 리스너 — 서버 권위 시간 기반 catchup (PlayFab 가 12h 캡 이미 적용).
	// Mine 은 Storage 한도 자연 캡 → 1.0x 효율 (빌딩 VaultCap 과 같은 논리).
	void ApplyServerOfflineCatchup(float OfflineSeconds);

	// ── Debug ──
	// 모든 라인을 MaxStorage 까지 채움 (수령 흐름 테스트용).
	UFUNCTION(BlueprintCallable, Category = "Mine|Debug")
	void DebugFillAllLines(ECountryType Country);

private:
	UPROPERTY()
	TMap<ECountryType, FCountryMineData> CountryMines;

	FTSTicker::FDelegateHandle TickerHandle;

	static constexpr float MineTickInterval = 0.25f;

	// OfflineCatchupBoost hard cap (50% 보너스 한계)
	static constexpr float OfflineCatchupBoostHardCap = 0.5f;

	FCountryMineData& GetOrCreateMine(ECountryType Country);
	const FCountryMineData* FindMine(ECountryType Country) const;

	bool GetUpgradeDef(EMineUpgradeType Type, struct FMineUpgradeDefinition& OutDef) const;

	bool TickInternal(float DeltaTime);

	int32 FindLineIndexByResource(const FCountryMineData& Mine, EResourceType Resource) const;

	// GDD_FACTORY 524-541 표 임시 룰: 첫번째 자원 = 주력(2/min), 그 외 = 보조(1/min).
	// 추후 DT_CountryMineResource 로 분리 (남아공 금/다이아 0.5/0.3 같은 예외 처리용).
	static float ComputeBaseRatePerSecondByIndex(int32 IndexInList);
};
