// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "Entity/Country/CountryActor.h"
#include "WorldFactoryManager.generated.h"

class UTexture2D;

/**
 * 공장 업그레이드 종류. UIE_UpgradeSlotHorizon 슬롯 이름과 매핑.
 * 메타데이터/밸런스는 DT_WorldFactoryUpgradeDefinition (FWorldFactoryUpgradeDefinition) 단일 진실 원천.
 */
UENUM(BlueprintType)
enum class EWorldFactoryUpgradeType : uint8
{
	None             UMETA(Hidden),
	LineExpansion    UMETA(DisplayName = "라인 증설"),
	Automation       UMETA(DisplayName = "자동화"),
	QualityControl   UMETA(DisplayName = "품질 관리"),
	StorageExpansion UMETA(DisplayName = "창고 증축"),
	RushProduction   UMETA(DisplayName = "특급 가공"),
};

/**
 * 공장 라인 1개의 상태.
 * Country는 외부 TMap 키로 보관되므로 필드에 없음.
 */
USTRUCT(BlueprintType)
struct FWorldFactoryLineState
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadOnly)
	int32 LineId = 0;

	UPROPERTY(SaveGame, BlueprintReadOnly)
	int32 ProductIndex = 0;

	UPROPERTY(SaveGame, BlueprintReadOnly)
	FText ProductName;

	UPROPERTY(SaveGame, BlueprintReadOnly)
	TSoftObjectPtr<UTexture2D> IconPath;

	UPROPERTY(SaveGame, BlueprintReadOnly)
	float RatePerSecond = 1.0f;

	UPROPERTY(SaveGame, BlueprintReadOnly)
	int64 TargetQty = 0;

	// Wall-clock lazy 모델의 핵심 — 라인 시작 시점 UTC.
	// GetLineState 호출 시 (UtcNow - StartWallUtc) * RatePerSecond 로 CurrentQty 동적 계산.
	// SaveGame 마킹 → PIE 재시작 후 자동 catch-up (wall-clock 차이만큼 진행).
	UPROPERTY(SaveGame, BlueprintReadOnly)
	FDateTime StartWallUtc = FDateTime(0);

	// ── 아래 3개는 GetLineState 의 OutState 채우기용 (lazy 계산 결과). 직접 저장 안 함. ──

	UPROPERTY(BlueprintReadOnly)
	int64 CurrentQty = 0;

	UPROPERTY(BlueprintReadOnly)
	double LineElapsedSec = 0.0;

	UPROPERTY(BlueprintReadOnly)
	bool bCompleted = false;
};

/**
 * 국가 1개의 공장 데이터. 라인 배열 + 강화 레벨 + LineId 시퀀스.
 */
USTRUCT(BlueprintType)
struct FCountryFactoryData
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadOnly)
	TArray<FWorldFactoryLineState> ActiveLines;

	UPROPERTY(SaveGame, BlueprintReadOnly)
	TMap<EWorldFactoryUpgradeType, int32> UpgradeLevels;

	// 국가별로 LineId 독립 시퀀스
	UPROPERTY(SaveGame)
	int32 NextLineId = 1;
};

/**
 * UWorldFactoryManager
 * 월드맵 국가별 배치 생산 백엔드. 국가마다 독립된 라인/강화 데이터를 관리.
 *
 * 모든 API는 ECountryType을 첫 인자로 받아 해당 국가 공장을 조작.
 * 델리게이트도 국가 정보 포함하여 브로드캐스트.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UWorldFactoryManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWorldFactoryLineStarted,
		ECountryType, Country, FWorldFactoryLineState, LineState);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnWorldFactoryLineProgressed,
		ECountryType, Country, int32, LineId, int64, CurrentQty);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWorldFactoryLineCompleted,
		ECountryType, Country, int32, LineId);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnWorldFactoryLineClaimed,
		ECountryType, Country, int32, LineId, int64, FinalQty);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnWorldFactoryUpgradeChanged,
		ECountryType, Country, EWorldFactoryUpgradeType, UpgradeType, int32, NewLevel);

	UPROPERTY(BlueprintAssignable, Category = "WorldFactory|Events")
	FOnWorldFactoryLineStarted OnLineStarted;

	UPROPERTY(BlueprintAssignable, Category = "WorldFactory|Events")
	FOnWorldFactoryLineProgressed OnLineProgressed;

	UPROPERTY(BlueprintAssignable, Category = "WorldFactory|Events")
	FOnWorldFactoryLineCompleted OnLineCompleted;

	UPROPERTY(BlueprintAssignable, Category = "WorldFactory|Events")
	FOnWorldFactoryLineClaimed OnLineClaimed;

	UPROPERTY(BlueprintAssignable, Category = "WorldFactory|Events")
	FOnWorldFactoryUpgradeChanged OnUpgradeChanged;

	// UE5 기본 자동 등록이 일부 환경에서 skip 되는 케이스 차단 — 명시적 true 반환으로 강제 등록.
	// (프로젝트 컨벤션: ProjectOperationManager / ProductionOrderManager 와 동일 패턴)
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── 생산 라인 (Country별) ──
	UFUNCTION(BlueprintCallable, Category = "WorldFactory")
	int32 StartProduction(ECountryType Country, int32 ProductIndex, const FText& ProductName,
		const TSoftObjectPtr<UTexture2D>& IconPath, float BaseRatePerSecond, int64 TargetQty);

	UFUNCTION(BlueprintCallable, Category = "WorldFactory")
	bool InstantFinishLine(ECountryType Country, int32 LineId);

	// [dev 프리셋] 라인 진행도를 임의 비율로 되돌린다.
	// 진행도의 유일한 원천이 시작 시각이라 백데이트 말고는 중간 % 를 만들 방법이 없다
	// (StartProduction 은 항상 UtcNow 를 박고, InstantFinishLine 은 100% 로만 점프).
	bool SeedLineProgress(ECountryType Country, int32 LineId, float ProgressRatio);

	UFUNCTION(BlueprintCallable, Category = "WorldFactory")
	bool ClaimLine(ECountryType Country, int32 LineId);

	UFUNCTION(BlueprintPure, Category = "WorldFactory")
	TArray<FWorldFactoryLineState> GetActiveLines(ECountryType Country) const;

	UFUNCTION(BlueprintPure, Category = "WorldFactory")
	int32 GetActiveLineCount(ECountryType Country) const;

	// 공장 데이터가 등록된 모든 국가 반환 — CollectAll 같은 전역 순회에 사용.
	UFUNCTION(BlueprintPure, Category = "WorldFactory")
	TArray<ECountryType> GetActiveCountries() const;

	UFUNCTION(BlueprintPure, Category = "WorldFactory")
	bool GetLineState(ECountryType Country, int32 LineId, FWorldFactoryLineState& OutState) const;

	// ── 강화 (Country별) ──
	UFUNCTION(BlueprintPure, Category = "WorldFactory|Upgrade")
	int32 GetUpgradeLevel(ECountryType Country, EWorldFactoryUpgradeType UpgradeType) const;

	UFUNCTION(BlueprintCallable, Category = "WorldFactory|Upgrade")
	bool UpgradeLevelUp(ECountryType Country, EWorldFactoryUpgradeType UpgradeType);

	UFUNCTION(BlueprintPure, Category = "WorldFactory|Upgrade")
	int32 GetMaxLines(ECountryType Country) const;

	UFUNCTION(BlueprintPure, Category = "WorldFactory|Upgrade")
	float GetSpeedMultiplier(ECountryType Country) const;

	UFUNCTION(BlueprintPure, Category = "WorldFactory|Upgrade")
	float GetRewardMultiplier(ECountryType Country) const;

	UFUNCTION(BlueprintPure, Category = "WorldFactory|Upgrade")
	int64 GetVaultCapacity(ECountryType Country) const;

	UFUNCTION(BlueprintPure, Category = "WorldFactory|Upgrade")
	float GetInstantFinishDiscount(ECountryType Country) const;

	// 다음 레벨 업그레이드 비용 (DT 의 BaseCost * CostGrowthRate^CurrentLevel)
	UFUNCTION(BlueprintPure, Category = "WorldFactory|Upgrade")
	int64 GetUpgradeCost(ECountryType Country, EWorldFactoryUpgradeType UpgradeType) const;

	// ── SaveLoad 직렬화 ──
	// LineElapsedSec 모델 → 절대 timestamp 가 아니라 catchup 자동 흐름 없음. 단순 TMap 카피.
	void SerializeForSave(TMap<ECountryType, FCountryFactoryData>& OutData) const;
	void LoadFromSave(const TMap<ECountryType, FCountryFactoryData>& InData);

	// PlayFab OnOfflineGainsRequested 리스너 — 서버 권위 시간 기반 catchup. 빌딩과 동일 listener 패턴.
	// EffectiveOffline = OfflineSeconds * 0.2 (빌딩 OfflineTimeProgressRate 와 통일).
	// 시간 캡(12h)은 PlayFab 측에서 이미 적용됨 — 여기선 받은 값 그대로 사용.
	void ApplyServerOfflineCatchup(float OfflineSeconds);

private:
	// 오프라인 효율 — 빌딩 SaveLoadManager.cpp:174 의 OfflineTimeProgressRate 와 통일
	static constexpr double OfflineEfficiencyMultiplier = 0.2;
	// 국가별 공장 데이터 저장
	UPROPERTY()
	TMap<ECountryType, FCountryFactoryData> CountryFactories;

	FTSTicker::FDelegateHandle TickerHandle;

	// RushProduction 할인 안전 상한 (전액 무료 차단). 그 외 모든 밸런스는 DT 단일 진실.
	static constexpr float RushDiscountHardCap = 0.95f;

	FCountryFactoryData& GetOrCreateFactory(ECountryType Country);
	const FCountryFactoryData* FindFactory(ECountryType Country) const;

	// DT 행 조회 헬퍼. row 없으면 false → 호출부가 0 / 기본 multiplier 폴백.
	bool GetUpgradeDef(EWorldFactoryUpgradeType Type, struct FWorldFactoryUpgradeDefinition& OutDef) const;

	bool TickInternal(float DeltaTime);
	int32 FindLineIndexById(const FCountryFactoryData& Factory, int32 LineId) const;
};
