#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Data/ProductionOrderData.h"
#include "Enum/ResourceType.h"
#include "ProductionOrderManager.generated.h"

// 주문서 생성 시
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnProductionOrderCreated, int32, BuildingID, const FProductionOrder&, Order);

// 주문서 수량 소비 시
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnProductionOrderConsumed, int32, OrderID, int32, ConsumedAmount, int32, RemainingQuantity);

// 주문서 완전 소진 시
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnProductionOrderDepleted, int32, OrderID);

// 상한 3개 중재 + 병목 이름 처리. 제품(칩) 병목이면 자원 이름을 비운다 —
// 그러지 않으면 재료는 충분한데 칩만 모자란 경우에 "광석을 캐라"는 틀린 안내가 나간다.
// GetMaxProducible 이 서브시스템 3개를 타서 테스트가 불가하므로 이 중재만 떼어낸다.
inline int32 ArbitrateProducibleCap(int32 MaterialCap, int32 ProductCap, int32 OrderCap,
	EResourceType& InOutBottleneck, bool& bOutSupplyBound)
{
	int32 Cap = MaterialCap;
	if (ProductCap < Cap)
	{
		Cap = ProductCap;
		InOutBottleneck = EResourceType::None;
	}

	bOutSupplyBound = (Cap < OrderCap);
	if (!bOutSupplyBound)
	{
		InOutBottleneck = EResourceType::None;
	}
	return FMath::Min(Cap, OrderCap);
}

/**
 * 생산 주문서 관리 매니저
 *
 * 제조업 회사의 포인트 예약으로 생성되는 주문서를 관리
 * - 생산 가능 수량 예약
 * - 주문서 조회 (공장 생산 시)
 * - 주문서 수량 소비 (공장 생산 완료 시)
 * - 세이브/로드 연동
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UProductionOrderManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UProductionOrderManager();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }

	// 제품 1개를 확보하는 데 필요한 직능별 개발 포인트. 유효하지 않으면 빈 배열.
	UFUNCTION(BlueprintPure, Category = "Production|Reservation")
	TArray<double> GetDevelopmentPointCosts(int32 BuildingID, int32 ProjectIndex) const;

	UFUNCTION(BlueprintPure, Category = "Production|Reservation")
	int32 GetMaxReservableQuantity(int32 BuildingID, int32 ProjectIndex) const;

	// 포인트를 생산 가능 수량으로 교환한다. 반환값은 생성한 주문 ID, 실패 시 0.
	UFUNCTION(BlueprintCallable, Category = "Production|Reservation")
	int32 ReserveProductionQuantity(int32 BuildingID, int32 ProjectIndex, int32 Quantity, FText& OutFailure);

	// ===== 주문서 조회 =====

	/**
	 * 특정 건물의 활성 주문서 목록 조회
	 */
	UFUNCTION(BlueprintPure, Category = "Production|Query")
	TArray<FProductionOrder> GetOrdersByBuilding(int32 BuildingID) const;

	/**
	 * 모든 활성 주문서 조회
	 */
	UFUNCTION(BlueprintPure, Category = "Production|Query")
	const TArray<FProductionOrder>& GetAllOrders() const { return ActiveOrders; }

	/**
	 * 주문 ID로 주문서 조회
	 */
	FProductionOrder* GetOrderByID(int32 OrderID);

	/**
	 * 특정 건물의 총 생산 가능 수량
	 */
	UFUNCTION(BlueprintPure, Category = "Production|Query")
	int32 GetTotalRemainingQuantityByBuilding(int32 BuildingID) const;

	/**
	 * 모든 주문서의 총 남은 수량
	 */
	UFUNCTION(BlueprintPure, Category = "Production|Query")
	int32 GetTotalRemainingQuantity() const;

	/**
	 * 재료·제품 입력 기준 제작 가능 최대 수량. 주문서 잔여로 한 번 더 clamp.
	 *
	 * 라인 여유는 여기 들어가지 않는다 — 라인은 수량 상한이 아니라 시작 게이트다
	 * (StartProduction 이 라인 1개를 점유하고 TargetQty 만큼 돌린다).
	 *
	 * @param OutBottleneck 상한을 정한 재료. 제품 입력(칩)이 병목이거나 주문서가 먼저 막으면 None.
	 * @param bOutSupplyBound 공급(재료 또는 제품 입력)이 주문서 잔여보다 먼저 막았는지 (UI 문구 분기용).
	 *        true 인데 OutBottleneck 이 None 이면 제품 입력이 병목 — IsProductInputShort 로 무엇이 모자란지 묻는다.
	 * @return 0 이면 1개도 만들 수 없음
	 */
	UFUNCTION(BlueprintPure, Category = "Production|Query")
	int32 GetMaxProducible(const FProductionOrder& Order, EResourceType& OutBottleneck, bool& bOutSupplyBound) const;

	// 원하는 수량에 제품 입력(칩)이 모자라는가. 모자라면 무엇이 몇 개 부족한지 돌려준다 —
	// 자원 부족 안내와 문구가 갈라져야 플레이어가 광석을 캐러 가지 않는다.
	UFUNCTION(BlueprintPure, Category = "Production|Query")
	bool IsProductInputShort(const FProductionOrder& Order, int32 DesiredQty,
		int32& OutNeededProjectIndex, int32& OutShortBy) const;

	// ===== 수량 소비 =====

	/**
	 * 주문서 수량 소비 (공장 생산 시)
	 * @param OrderID 주문 ID
	 * @param Amount 소비할 수량
	 * @return 실제 소비된 수량
	 */
	UFUNCTION(BlueprintCallable, Category = "Production")
	int32 ConsumeQuantity(int32 OrderID, int32 Amount);

	/**
	 * 소진된 주문서 정리
	 */
	UFUNCTION(BlueprintCallable, Category = "Production")
	void CleanupDepletedOrders();

	// ===== Delegates =====

	UPROPERTY(BlueprintAssignable, Category = "Production|Events")
	FOnProductionOrderCreated OnOrderCreated;

	UPROPERTY(BlueprintAssignable, Category = "Production|Events")
	FOnProductionOrderConsumed OnOrderConsumed;

	UPROPERTY(BlueprintAssignable, Category = "Production|Events")
	FOnProductionOrderDepleted OnOrderDepleted;

	// ===== Save/Load =====

	UFUNCTION(BlueprintCallable, Category = "Production|SaveLoad")
	void SetActiveOrders(const TArray<FProductionOrder>& InOrders);

	UFUNCTION(BlueprintPure, Category = "Production|SaveLoad")
	int32 GetNextOrderID() const { return NextOrderID; }

	UFUNCTION(BlueprintCallable, Category = "Production|SaveLoad")
	void SetNextOrderID(int32 InID) { NextOrderID = InID; }

	/**
	 * SaveData에서 주문서 로드
	 */
	UFUNCTION(BlueprintCallable, Category = "Production|SaveLoad")
	void LoadOrdersFromSave();

	// ===== Test/Debug =====

	/**
	 * 테스트용 직접 주문 생성 (사무실 양산 흐름 우회).
	 * @return 생성된 OrderID. -1 실패.
	 */
	UFUNCTION(BlueprintCallable, Category = "Production|Debug")
	int32 CreateTestOrder(ECompanyType InCompanyType, int32 InProjectIndex, const FString& InProductName,
		int32 InQuantity, EQualityGrade InGrade = EQualityGrade::C, int32 InSourceBuildingID = -1);

	/**
	 * DT_TestProductionOrders 일괄 spawn.
	 * @param RandomCount -1=전체 행 spawn. 양수=무작위 N개만 spawn.
	 * @return 생성된 주문 개수
	 */
	UFUNCTION(BlueprintCallable, Exec, Category = "Production|Debug")
	int32 SpawnTestOrdersFromTable(int32 RandomCount = -1);

private:
	UPROPERTY()
	TArray<FProductionOrder> ActiveOrders;

	UPROPERTY()
	int32 NextOrderID = 1;

	bool bReservationInProgress = false;
};
