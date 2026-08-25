#pragma once

#include "CoreMinimal.h"
#include "Enum/CompanyType.h"
#include "Entity/Country/CountryActor.h"
#include "Enum/QualityGrade.h"
#include "Enum/RawMaterialType.h"
#include "Data/ProductionOrderData.h"
#include "WorldMapTypes.generated.h"

/**
 * WorldMapTypes
 * 월드맵 생산 시스템에서 사용하는 공용 USTRUCT 모음.
 *
 * Design rationale (아키텍처 결정):
 *  - USTRUCT 선택 이유: 40개 이상의 MiningFacility + 25개 이상의 FactoryLine.
 *    UObject 기반으로 하면 GC 부담이 크고 모바일 서스펜드/리줌에 불리함.
 *    USTRUCT로 하면 UPROPERTY(SaveGame)만으로 자동 직렬화.
 *  - Tick은 struct가 직접 안 돌리고, WorldMapManager가 중앙 Ticker로 처리.
 *  - 델리게이트는 struct에 두지 않음 (UObject가 아니므로). Manager가 broadcast.
 */

// ─────────────────────────────────────────────
// 채광소 (나라 × 원자재 조합마다 1개 인스턴스)
// ─────────────────────────────────────────────
USTRUCT(BlueprintType)
struct FMiningFacility
{
	GENERATED_BODY()

	/** 채광 국가 */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Mining")
	ECountryType CountryType = ECountryType::Korea;

	/** 채취 원자재 */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Mining")
	ERawMaterialType MaterialType = ERawMaterialType::IronOre;

	/** 채광소 레벨 (자동 채취 속도 스케일) */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Mining")
	int32 Level = 1;

	/** 분당 자동 채취량 (Level에 비례, 건설 직후 초기값) */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Mining")
	float AutoMineRatePerMin = 2.0f;

	/** 저장 한도 (한도 초과 시 자동 채취 중단) */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Mining")
	int64 VaultCapacity = 200;

	/** 현재 채광소에 쌓인 미수집 자원 */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Mining")
	int64 CurrentStorage = 0;

	/** 분수초 누적기 (1개 단위로 떨어지지 않는 자투리) */
	UPROPERTY(SaveGame)
	float AccumulatorSec = 0.0f;

	/** 부스트 종료 시각 (UTC). 이 시각 이전까지 x3 배속 */
	UPROPERTY(SaveGame)
	FDateTime BoostEndTime = FDateTime(0);

	/** 부스트 쿨타임 종료 시각. 이 시각 이후 재사용 가능 */
	UPROPERTY(SaveGame)
	FDateTime BoostCooldownEnd = FDateTime(0);

	/** 건설 여부 (false면 Tick에서 스킵) */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Mining")
	bool bConstructed = false;

	/** 저장 한도 도달 알림 중복 방지 (Transient - 저장 안됨) */
	UPROPERTY(Transient)
	bool bFullNotified = false;

	bool IsBoostActive(const FDateTime& Now) const { return Now < BoostEndTime; }
	bool IsBoostReady(const FDateTime& Now) const { return Now >= BoostCooldownEnd; }
	bool IsFull() const { return CurrentStorage >= VaultCapacity; }

	FMiningFacility() = default;
};

// ─────────────────────────────────────────────
// 공장 라인 (공장국 × LineIndex 마다 1개 인스턴스)
// ─────────────────────────────────────────────
USTRUCT(BlueprintType)
struct FFactoryLine
{
	GENERATED_BODY()

	/** 공장 국가 */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Factory")
	ECountryType CountryType = ECountryType::Korea;

	/** 나라 내 라인 번호 (0~4) */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Factory")
	int32 LineIndex = 0;

	/** 배정된 회사 타입 (None = 비어있음) */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Factory")
	ECompanyType AssignedCompanyType = ECompanyType::None;

	/** 배정된 프로젝트 인덱스 (1~100) */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Factory")
	int32 AssignedProjectIndex = INDEX_NONE;

	/** 배정된 생산 주문서 OrderID */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Factory")
	int32 AssignedOrderId = INDEX_NONE;

	/** 남은 생산 수량 (0이면 주문 완료) */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Factory")
	int32 RemainingQuantity = 0;

	/** 현재 유닛 진행률 0.0~1.0 */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Factory")
	float Progress = 0.0f;

	/** 일시정지 여부 (원자재/에너지 부족) */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Factory")
	bool bPaused = false;

	/** 부스트 종료 시각 */
	UPROPERTY(SaveGame)
	FDateTime BoostEndTime = FDateTime(0);

	/** 부스트 쿨타임 종료 시각 */
	UPROPERTY(SaveGame)
	FDateTime BoostCooldownEnd = FDateTime(0);

	/** 완성품에 찍힐 품질 등급 (오피스 스테이지 결과) */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Factory")
	EQualityGrade StampedGrade = EQualityGrade::C;

	/** 라인 해금 여부 (공장 레벨로 결정) */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Factory")
	bool bUnlocked = false;

	bool IsIdle() const { return AssignedProjectIndex == INDEX_NONE; }
	bool IsBoostActive(const FDateTime& Now) const { return Now < BoostEndTime; }
	bool IsBoostReady(const FDateTime& Now) const { return Now >= BoostCooldownEnd; }

	FFactoryLine() = default;
};

// ─────────────────────────────────────────────
// 무역 주문서 카테고리
// ─────────────────────────────────────────────
UENUM(BlueprintType)
enum class ETradeOrderTier : uint8
{
	Normal   UMETA(DisplayName = "일반"),     // 6~12시간, x1.3~1.8
	Urgent   UMETA(DisplayName = "긴급"),     // 1~3시간, x2.0~3.0
	VIP      UMETA(DisplayName = "VIP")       // 24시간, x3.0+ + Diamond
};

// ─────────────────────────────────────────────
// 무역 주문서
// ─────────────────────────────────────────────
USTRUCT(BlueprintType)
struct FTradeOrder
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Trade")
	int32 OrderId = INDEX_NONE;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Trade")
	ETradeOrderTier Tier = ETradeOrderTier::Normal;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Trade")
	ECompanyType CompanyType = ECompanyType::None;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Trade")
	int32 ProjectIndex = 0;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Trade")
	int64 RequestedQuantity = 0;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Trade")
	int64 RemainingQuantity = 0;

	/** 보상 배율 (Normal 1.3~1.8, Urgent 2.0~3.0, VIP 3.0+) */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Trade")
	float RewardMultiplier = 1.0f;

	/** VIP 전용: 달성 시 추가 Diamond 보상 */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Trade")
	int32 DiamondBonus = 0;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Trade")
	FDateTime ExpireTime;

	/** 플레이어가 수락한 주문인지 여부 (수락 슬롯 최대 3개) */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Trade")
	bool bAccepted = false;

	/** 보드에서 최소 한 번 이상 렌더링(확인)된 적 있는지. [신규] 뱃지 토글용. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Trade")
	bool bViewed = false;

	bool IsExpired(const FDateTime& Now) const { return Now >= ExpireTime; }
	bool IsFulfilled() const { return RemainingQuantity <= 0; }

	FTradeOrder() = default;
};

// ─────────────────────────────────────────────
// 국가별 산업 수요 게이지 상태 (Country × Industry 조합마다 1개 인스턴스)
// 판매 시 Current 차감, 매분 RecoveryPerMin만큼 회복.
// DemandMul 곡선: 0.4 + 0.8 × (Current/Capacity) → 0.4× ~ 1.2×
// ─────────────────────────────────────────────
USTRUCT(BlueprintType)
struct FCountryMarketState
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Market")
	ECountryType Country = ECountryType::Korea;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Market")
	ECompanyType Industry = ECompanyType::None;

	/** 수요 용량 (시장 크기). DT_CountryDemand에서 초기 로드. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Market")
	int32 Capacity = 1000;

	/** 현재 수요 (소비되면 감소, 시간 지나면 회복). 0이면 시장 포화. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Market")
	int32 Current = 1000;

	/** 분당 자동 회복량. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Market")
	float RecoveryPerMin = 10.0f;

	/** 분수초 누적기 (1단위 미만 회복분 보존) */
	UPROPERTY(SaveGame)
	float AccumulatorSec = 0.0f;

	float GetRatio() const
	{
		return (Capacity > 0) ? FMath::Clamp(static_cast<float>(Current) / static_cast<float>(Capacity), 0.0f, 1.0f) : 0.0f;
	}

	FCountryMarketState() = default;
};

// ─────────────────────────────────────────────
// 완성품 판매 결과
// ─────────────────────────────────────────────
USTRUCT(BlueprintType)
struct FSellResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Sell")
	int64 QuantitySold = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Sell")
	int64 MoneyGained = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Sell")
	int64 MarketCapGained = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Sell")
	int32 DiamondGained = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Sell")
	bool bMatchedOrder = false;

	UPROPERTY(BlueprintReadOnly, Category = "Sell")
	int32 MatchedOrderId = INDEX_NONE;

	FSellResult() = default;
};

// ─────────────────────────────────────────────
// 상수 (튜닝값 모음)
// ─────────────────────────────────────────────
namespace WorldMapConstants
{
	// 부스트 지속시간 / 쿨타임
	constexpr float MiningBoostDurationSec = 30.0f;
	constexpr float MiningBoostCooldownSec = 300.0f;   // 5분
	constexpr float MiningBoostMultiplier  = 3.0f;

	constexpr float FactoryBoostDurationSec = 30.0f;
	constexpr float FactoryBoostCooldownSec = 300.0f;
	constexpr float FactoryBoostMultiplier  = 2.0f;

	// 라인 해금 레벨 기준
	constexpr int32 LineUnlockLevels[5] = { 1, 5, 10, 20, 50 };

	// 무역항 보너스
	constexpr float USAPriceBonus      = 1.35f;  // 미국 +35% 단가
	constexpr float SingaporeFeeBonus  = 0.50f;  // 싱가포르 수수료 -50%
	constexpr float SingaporeSpeedBonus = 2.0f;  // 싱가포르 2배 속도

	// 일괄 판매 보너스
	constexpr float BulkBonus10  = 1.10f;
	constexpr float BulkBonus50  = 1.25f;
	constexpr float BulkBonus100 = 1.50f;

	// Tick 간격 (초). 모바일 성능 고려.
	constexpr float TickIntervalSec = 1.0f;

	// 오프라인 캐치업 상한 (24시간)
	constexpr float OfflineCatchupMaxSec = 86400.0f;

	// 시장 성장 곡선 파라미터 → DT_MarketBalance ("Default" row, FMarketBalanceData) 참조.
}

/**
 * 제품 키 헬퍼: (CompanyType, ProjectIndex) → FIntPoint
 * TMap<FIntPoint, int64> 완성품 인벤토리의 키로 사용.
 */
FORCEINLINE FIntPoint MakeProductKey(ECompanyType Company, int32 ProjectIndex)
{
	return FIntPoint(static_cast<int32>(Company), ProjectIndex);
}

/**
 * 물품거래소 모달이 다루는 인벤 아이템 1단위.
 * (Industry, ProjectIndex) = TradePort 키 (BasePrice/Recipe lookup용).
 */
USTRUCT(BlueprintType)
struct FSellableItem
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Sellable")
	ECompanyType Industry = ECompanyType::None;

	UPROPERTY(BlueprintReadWrite, Category = "Sellable")
	int32 ProjectIndex = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Sellable")
	int64 Quantity = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Sellable")
	int64 BasePrice = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Sellable")
	FText DisplayName;

	UPROPERTY(BlueprintReadWrite, Category = "Sellable")
	EQualityGrade Grade = EQualityGrade::C;

	bool IsValidItem() const { return Industry != ECompanyType::None && ProjectIndex > 0; }
};
