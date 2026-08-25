#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Data/ProductionOrderData.h"
#include "Data/StageProgressData.h"
#include "Enum/ResourceType.h"
#include "ProductionOrderManager.generated.h"

// 주문서 생성 시
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnProductionOrderCreated, int32, BuildingID, const FProductionOrder&, Order);

// 주문서 수량 소비 시
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnProductionOrderConsumed, int32, OrderID, int32, ConsumedAmount, int32, RemainingQuantity);

// 주문서 완전 소진 시
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnProductionOrderDepleted, int32, OrderID);

/**
 * 생산 주문서 관리 매니저
 *
 * 제조업 회사의 양산 확정 시 생성되는 주문서를 관리
 * - 주문서 생성 (양산 확정 시)
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

	// ===== 주문서 생성 =====

	/**
	 * 양산 확정 시 주문서 생성
	 * @param StageData 스테이지 진행 데이터 (품질 정보 포함)
	 * @param BuildingID 개발한 건물 ID
	 * @return 생성된 주문서의 OrderID (-1: 실패)
	 */
	UFUNCTION(BlueprintCallable, Category = "Production")
	int32 CreateOrder(const FStageProgressData& StageData, int32 BuildingID);

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
	 * 재료 기준 제작 가능 최대 수량. 주문서 잔여로 한 번 더 clamp.
	 *
	 * 라인 여유는 여기 들어가지 않는다 — 라인은 수량 상한이 아니라 시작 게이트다
	 * (StartProduction 이 라인 1개를 점유하고 TargetQty 만큼 돌린다).
	 *
	 * @param OutBottleneck 상한을 정한 재료. 재료가 주문서보다 먼저 막지 않으면 None.
	 * @param bOutMaterialBound 재료가 주문서 잔여보다 먼저 막았는지 (UI 문구 분기용)
	 * @return 0 이면 1개도 만들 수 없음
	 */
	UFUNCTION(BlueprintPure, Category = "Production|Query")
	int32 GetMaxProducible(const FProductionOrder& Order, EResourceType& OutBottleneck, bool& bOutMaterialBound) const;

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
};
