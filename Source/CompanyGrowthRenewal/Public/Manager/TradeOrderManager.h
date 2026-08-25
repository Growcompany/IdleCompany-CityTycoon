// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "Data/WorldMapTypes.h"
#include "Enum/CompanyType.h"
#include "Table/TradeOrderBalanceTable.h"
#include "TradeOrderManager.generated.h"

/**
 * UTradeOrderManager
 * 무역항 주문서(Trade Order) 관리 서브시스템.
 *
 * 역할:
 *  - 티어별(Normal/Urgent/VIP) 주문서를 자동 생성 / 만료 처리
 *  - TradePort에서 판매 시 매칭되는 주문서를 찾아 수량 차감
 *  - SaveLoadManager와 연동하여 저장/복원
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UTradeOrderManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// ────────────────────────────────────────────
	// Blueprint 바인딩용 Dynamic Multicast 델리게이트
	// ────────────────────────────────────────────
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnOrderGenerated, FTradeOrder, Order);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnOrderExpired, int32, OrderId);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnOrderCompleted, int32, OrderId);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnOrderFulfilledPartial, int32, OrderId, int64, RemainingQty);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnOrderAccepted, int32, OrderId);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnOrderDismissed, int32, OrderId, bool, bWasAccepted);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnComboChanged, int32, ComboCount, float, ComboMultiplier);

	UPROPERTY(BlueprintAssignable, Category = "TradeOrder|Events")
	FOnOrderGenerated OnOrderGenerated;

	UPROPERTY(BlueprintAssignable, Category = "TradeOrder|Events")
	FOnOrderExpired OnOrderExpired;

	UPROPERTY(BlueprintAssignable, Category = "TradeOrder|Events")
	FOnOrderCompleted OnOrderCompleted;

	UPROPERTY(BlueprintAssignable, Category = "TradeOrder|Events")
	FOnOrderFulfilledPartial OnOrderFulfilledPartial;

	UPROPERTY(BlueprintAssignable, Category = "TradeOrder|Events")
	FOnOrderAccepted OnOrderAccepted;

	UPROPERTY(BlueprintAssignable, Category = "TradeOrder|Events")
	FOnOrderDismissed OnOrderDismissed;

	UPROPERTY(BlueprintAssignable, Category = "TradeOrder|Events")
	FOnComboChanged OnComboChanged;

	// ────────────────────────────────────────────
	// Lifecycle
	// ────────────────────────────────────────────
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ────────────────────────────────────────────
	// Public API
	// ────────────────────────────────────────────
	UFUNCTION(BlueprintPure, Category = "TradeOrder")
	TArray<FTradeOrder> GetActiveOrders() const { return ActiveOrders; }

	UFUNCTION(BlueprintPure, Category = "TradeOrder")
	TArray<FTradeOrder> GetOrdersByTier(ETradeOrderTier Tier) const;

	// 수락한 주문만 반환
	UFUNCTION(BlueprintPure, Category = "TradeOrder")
	TArray<FTradeOrder> GetAcceptedOrders() const;

	// 미수락 주문만 반환
	UFUNCTION(BlueprintPure, Category = "TradeOrder")
	TArray<FTradeOrder> GetUnacceptedOrders() const;

	UFUNCTION(BlueprintPure, Category = "TradeOrder")
	int32 GetAcceptedCount() const;

	UFUNCTION(BlueprintPure, Category = "TradeOrder")
	int32 GetMaxAcceptedSlots() const;

	// 주문 수락 (최대 3개 슬롯)
	UFUNCTION(BlueprintCallable, Category = "TradeOrder")
	bool AcceptOrder(int32 OrderId);

	/**
	 * 주문 치우기 — 미수락은 "거절", 수락본은 "포기"로 동일 진입점.
	 * - !bAccepted: 보드에서만 제거 (페널티 없음, 다음 스폰 사이클에 보충)
	 * - bAccepted : 제거 + 콤보 리셋 (만료와 동일 패널티)
	 * @return 제거 성공 여부
	 */
	UFUNCTION(BlueprintCallable, Category = "TradeOrder")
	bool DismissOrder(int32 OrderId);

	/**
	 * 주문들을 "확인됨"으로 일괄 마킹. 보드에서 카드가 첫 렌더링될 때 UI 측이 호출.
	 * 이미 bViewed=true인 항목은 건너뛰고, 실제로 한 건이라도 바뀐 경우에만 저장.
	 * 델리게이트는 쏘지 않아 RefreshBoard 재진입 위험 없음.
	 */
	UFUNCTION(BlueprintCallable, Category = "TradeOrder")
	void MarkOrdersAsViewed(const TArray<int32>& OrderIds);

	// 콤보 정보
	UFUNCTION(BlueprintPure, Category = "TradeOrder")
	int32 GetComboCount() const { return ComboCount; }

	UFUNCTION(BlueprintPure, Category = "TradeOrder")
	float GetComboMultiplier() const;

	// 판매하려는 제품과 매칭되는 활성 주문서를 검색 (VIP>Urgent>Normal 우선순위)
	UFUNCTION(BlueprintPure, Category = "TradeOrder")
	FTradeOrder FindMatchingOrder(ECompanyType Company, int32 ProjectIndex, bool& bOutFound) const;

	// 수락한 주문에 대해 수량 차감. OnOrderCompleted 또는 OnOrderFulfilledPartial 브로드캐스트.
	UFUNCTION(BlueprintCallable, Category = "TradeOrder")
	bool ConsumeOrderQuantity(int32 OrderId, int64 FulfilledQty);

	// 티어 지정 즉시 생성 (초기 보드 채우기 / 이벤트 / 테스트용)
	UFUNCTION(BlueprintCallable, Category = "TradeOrder")
	FTradeOrder GenerateOrderByTier(ETradeOrderTier Tier, ECompanyType ForceCompany = ECompanyType::None);

	UFUNCTION(BlueprintCallable, Category = "TradeOrder")
	bool RegenerateIfExpired();

	UFUNCTION(BlueprintCallable, Category = "TradeOrder")
	void ClearAllOrders();

	// ────────────────────────────────────────────
	// SaveLoad 연동
	// ────────────────────────────────────────────
	void SerializeForSave(TArray<FTradeOrder>& OutOrders, int32& OutNextOrderId, int32& OutComboCount) const;
	void LoadFromSave(const TArray<FTradeOrder>& InOrders, int32 InNextOrderId, int32 InComboCount);

private:
	// ────────────────────────────────────────────
	// 상태
	// ────────────────────────────────────────────
	UPROPERTY()
	TArray<FTradeOrder> ActiveOrders;

	int32 NextOrderId = 1;

	FTSTicker::FDelegateHandle TickerHandle;

	// 다음 생성 체크까지 누적된 시간(초)
	float NextGenCheckSec = 0.0f;

	// TrySpawnOrders 재진입 방지
	bool bReentryGuard = false;

	// 연속 납품 콤보 카운터
	int32 ComboCount = 0;

	// ────────────────────────────────────────────
	// 내부 헬퍼
	// ────────────────────────────────────────────
	bool TickInternal(float DeltaTime);
	void ExpireOldOrders();
	void TrySpawnOrders();
	FTradeOrder CreateRandomOrder(ETradeOrderTier Tier, ECompanyType ForceCompany = ECompanyType::None);

	// Balance DT lookup. TableManager 미초기화 시 struct 기본값 fallback.
	const FTradeOrderBalanceData& GetBalance() const;

	// 해당 회사 타입의 빌딩들 중 가장 높은 해금 Tier 반환 (OfficeDataMap 순회).
	// 해당 산업 빌딩이 없거나 아무도 Office 진입 전이면 1.
	// const 아님: SaveLoadManager::GetCurrentSaveData 가 non-const.
	int32 GetHighestUnlockedTier(ECompanyType Company);

	// 해금된 티어 [1..MaxTier] 중 가중 랜덤으로 하나 선택.
	// Weight[T] = pow(TierWeightBase, T - 1) → 최신 티어일수록 뽑힐 확률 증가.
	int32 PickWeightedRandomTier(int32 MaxTier) const;

	void SaveGameData();
};
