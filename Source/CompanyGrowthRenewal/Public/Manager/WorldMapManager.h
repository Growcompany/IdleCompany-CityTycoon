// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "Data/WorldMapTypes.h"
#include "Data/ProductionOrderData.h"
#include "Enum/RawMaterialType.h"
#include "Enum/QualityGrade.h"
#include "Enum/CompanyType.h"
#include "Entity/Country/CountryActor.h"
#include "WorldMapManager.generated.h"

struct FProductRecipeTable;

/**
 * UWorldMapManager
 * 세계지도 생산/무역 시스템의 중앙 매니저 (GameInstanceSubsystem).
 *
 * 역할:
 *  - 원자재 인벤토리 / 에너지 / 정제유 관리
 *  - 채광소(Mines) / 공장라인(Lines) 배열 보유 및 Tick 구동
 *  - 완성품 인벤토리 (TMap<FIntPoint(CompanyType,ProjectIndex), int64>)
 *  - 생산 주문서 큐 (ProductionOrderManager가 EnqueueProductionOrder로 투입)
 *  - SaveLoad 연동 (Serialize/Load/OfflineCatchup)
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UWorldMapManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// ────────────────────────────────────────────
	// Blueprint 바인딩용 Dynamic Multicast 델리게이트
	// ────────────────────────────────────────────
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMaterialChanged, ERawMaterialType, Mat, int64, NewAmount);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnergyChanged, int64, NewEnergy);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRefinedOilChanged, int64, NewOil);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnFactoryUnitCompleted, ECountryType, Country, int32, LineIndex, FIntPoint, ProductKey);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnProductAdded, FIntPoint, ProductKey, int64, NewAmount);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnProductConsumed, FIntPoint, ProductKey, int64, NewAmount);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMiningStorageFull, ECountryType, Country, ERawMaterialType, Mat);

	UPROPERTY(BlueprintAssignable, Category = "WorldMap|Events")
	FOnMaterialChanged OnMaterialChanged;

	UPROPERTY(BlueprintAssignable, Category = "WorldMap|Events")
	FOnEnergyChanged OnEnergyChanged;

	UPROPERTY(BlueprintAssignable, Category = "WorldMap|Events")
	FOnRefinedOilChanged OnRefinedOilChanged;

	UPROPERTY(BlueprintAssignable, Category = "WorldMap|Events")
	FOnFactoryUnitCompleted OnFactoryUnitCompleted;

	UPROPERTY(BlueprintAssignable, Category = "WorldMap|Events")
	FOnProductAdded OnProductAdded;

	UPROPERTY(BlueprintAssignable, Category = "WorldMap|Events")
	FOnProductConsumed OnProductConsumed;

	UPROPERTY(BlueprintAssignable, Category = "WorldMap|Events")
	FOnMiningStorageFull OnMiningStorageFull;

	// ────────────────────────────────────────────
	// Lifecycle
	// ────────────────────────────────────────────
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ────────────────────────────────────────────
	// 원자재 인벤토리
	// ────────────────────────────────────────────
	UFUNCTION(BlueprintCallable, Category = "WorldMap|Material")
	void AddMaterial(ERawMaterialType Mat, int64 Amount, bool bShouldSave = true);

	UFUNCTION(BlueprintCallable, Category = "WorldMap|Material")
	bool ConsumeMaterial(ERawMaterialType Mat, int64 Amount, bool bShouldSave = true);

	// Recipe * Units 만큼 보유 여부 체크 (차감 없음)
	bool HasMaterials(const FProductRecipeTable& Recipe, int32 Units = 1) const;

	// Recipe * Units 만큼 일괄 소비. 하나라도 부족하면 false 반환하며 차감 없음.
	bool TryConsumeRecipe(const FProductRecipeTable& Recipe, int32 Units = 1, bool bShouldSave = true);

	UFUNCTION(BlueprintPure, Category = "WorldMap|Material")
	int64 GetMaterialAmount(ERawMaterialType Mat) const;

	// ────────────────────────────────────────────
	// 에너지 / 정제유
	// ────────────────────────────────────────────
	UFUNCTION(BlueprintCallable, Category = "WorldMap|Energy")
	void AddEnergy(int64 Amount, bool bShouldSave = true);

	UFUNCTION(BlueprintCallable, Category = "WorldMap|Energy")
	bool ConsumeEnergy(int64 Amount, bool bShouldSave = true);

	UFUNCTION(BlueprintPure, Category = "WorldMap|Energy")
	int64 GetEnergy() const { return EnergyCount; }

	UFUNCTION(BlueprintCallable, Category = "WorldMap|Energy")
	void AddRefinedOil(int64 Amount, bool bShouldSave = true);

	UFUNCTION(BlueprintPure, Category = "WorldMap|Energy")
	int64 GetRefinedOil() const { return RefinedOilCount; }

	// ────────────────────────────────────────────
	// 채광소 (Mining)
	// ────────────────────────────────────────────
	UFUNCTION(BlueprintCallable, Category = "WorldMap|Mining")
	bool StartMiningBoost(ECountryType Country, ERawMaterialType Mat);

	UFUNCTION(BlueprintCallable, Category = "WorldMap|Mining")
	bool UpgradeMine(ECountryType Country, ERawMaterialType Mat);

	UFUNCTION(BlueprintPure, Category = "WorldMap|Mining")
	FMiningFacility GetMine(ECountryType Country, ERawMaterialType Mat) const;

	UFUNCTION(BlueprintPure, Category = "WorldMap|Mining")
	TArray<FMiningFacility> GetMinesInCountry(ECountryType Country) const;

	// 채광소에 쌓인 자원을 인벤토리로 회수. 반환값 = 회수된 수량.
	UFUNCTION(BlueprintCallable, Category = "WorldMap|Mining")
	int64 CollectMine(ECountryType Country, ERawMaterialType Mat);

	// 건설되지 않은 채광소를 Lv.1로 건설
	UFUNCTION(BlueprintCallable, Category = "WorldMap|Mining")
	bool ConstructMine(ECountryType Country, ERawMaterialType Mat);

	// ────────────────────────────────────────────
	// 공장 (Factory)
	// ────────────────────────────────────────────
	UFUNCTION(BlueprintCallable, Category = "WorldMap|Factory")
	bool AssignLineAssignment(ECountryType Country, int32 LineIndex, ECompanyType CompanyType, int32 ProjectIndex, int32 OrderId, int32 Quantity, EQualityGrade Grade);

	UFUNCTION(BlueprintCallable, Category = "WorldMap|Factory")
	bool ClearLine(ECountryType Country, int32 LineIndex);

	UFUNCTION(BlueprintCallable, Category = "WorldMap|Factory")
	bool StartFactoryBoost(ECountryType Country, int32 LineIndex);

	UFUNCTION(BlueprintPure, Category = "WorldMap|Factory")
	FFactoryLine GetLine(ECountryType Country, int32 LineIndex) const;

	UFUNCTION(BlueprintPure, Category = "WorldMap|Factory")
	TArray<FFactoryLine> GetFactoryLines(ECountryType Country) const;

	UFUNCTION(BlueprintCallable, Category = "WorldMap|Factory")
	void SetAutoAssign(ECountryType Country, bool bEnable);

	UFUNCTION(BlueprintPure, Category = "WorldMap|Factory")
	bool IsAutoAssignEnabled(ECountryType Country) const;

	// ────────────────────────────────────────────
	// 생산 주문서 큐
	// ────────────────────────────────────────────
	UFUNCTION(BlueprintCallable, Category = "WorldMap|Production")
	void EnqueueProductionOrder(const FProductionOrder& Order);

	UFUNCTION(BlueprintPure, Category = "WorldMap|Production")
	TArray<FProductionOrder> GetPendingOrders() const { return PendingProductionQueue; }

	UFUNCTION(BlueprintCallable, Category = "WorldMap|Production")
	bool TryAutoAssignForCountry(ECountryType Country);

	// ────────────────────────────────────────────
	// 완성품 인벤토리
	// ────────────────────────────────────────────
	UFUNCTION(BlueprintCallable, Category = "WorldMap|Product")
	void AddProduct(FIntPoint ProductKey, int64 Qty, EQualityGrade Grade, bool bShouldSave = true);

	UFUNCTION(BlueprintCallable, Category = "WorldMap|Product")
	bool ConsumeProduct(FIntPoint ProductKey, int64 Qty, bool bShouldSave = true);

	UFUNCTION(BlueprintPure, Category = "WorldMap|Product")
	int64 GetProductAmount(FIntPoint ProductKey) const;

	UFUNCTION(BlueprintPure, Category = "WorldMap|Product")
	EQualityGrade GetProductGrade(FIntPoint ProductKey) const;

	UFUNCTION(BlueprintPure, Category = "WorldMap|Product")
	TArray<FIntPoint> GetAllProductKeys() const;

	// 보유 완성품 전체를 Money로 환산한 값 (좌중 창고 가치 위젯에 사용)
	UFUNCTION(BlueprintPure, Category = "WorldMap|Product")
	int64 GetTotalWarehouseValue() const;

	// ────────────────────────────────────────────
	// Debug: 판매 모달 테스트용 랜덤 시드. 산업 1~6 x 프로젝트 1~5 조합 NumSlots개 + 랜덤 등급/수량.
	// 저장 X (디버그 호출). 인벤이 비어있을 때 모달 진입 시 자동 호출됨.
	// ────────────────────────────────────────────
	UFUNCTION(BlueprintCallable, Category = "WorldMap|Debug")
	void DebugSeedRandomProducts(int32 NumSlots = 24, int64 MinQty = 100, int64 MaxQty = 8000);

	// 원자재의 주력 산출국 반환 (자원 패널 → 나라 점프 매핑)
	// 여러 나라에서 산출되면 가장 채취량이 많은 나라 (호주 철광석 등)
	UFUNCTION(BlueprintPure, Category = "WorldMap|Mining")
	ECountryType GetPrimaryCountryForMaterial(ERawMaterialType Mat) const;

	// ────────────────────────────────────────────
	// SaveLoad 연동 (SaveLoadManager가 호출)
	// ────────────────────────────────────────────
	void SerializeForSave(
		TMap<ERawMaterialType, int64>& OutMats,
		int64& OutEnergy,
		int64& OutOil,
		TArray<FMiningFacility>& OutMines,
		TArray<FFactoryLine>& OutLines,
		TMap<ECountryType, bool>& OutAutoAssign,
		TArray<FProductionOrder>& OutPending,
		TMap<FIntPoint, int64>& OutProducts,
		TMap<FIntPoint, uint8>& OutGrades,
		FDateTime& OutLastSaveUtc) const;

	void LoadFromSave(
		const TMap<ERawMaterialType, int64>& Mats,
		int64 Energy,
		int64 Oil,
		const TArray<FMiningFacility>& InMines,
		const TArray<FFactoryLine>& InLines,
		const TMap<ECountryType, bool>& InAutoAssign,
		const TArray<FProductionOrder>& InPending,
		const TMap<FIntPoint, int64>& InProducts,
		const TMap<FIntPoint, uint8>& InGrades,
		FDateTime LastSaveUtc);

	// 오프라인 누적 시간만큼 채광/공장 진행 catchup
	void ApplyOfflineCatchup(float ElapsedSec);

private:
	// ────────────────────────────────────────────
	// 상태
	// ────────────────────────────────────────────
	UPROPERTY()
	TMap<ERawMaterialType, int64> RawMaterialInventory;

	UPROPERTY()
	int64 EnergyCount = 0;

	UPROPERTY()
	int64 RefinedOilCount = 0;

	UPROPERTY()
	TArray<FMiningFacility> Mines;

	UPROPERTY()
	TArray<FFactoryLine> Lines;

	UPROPERTY()
	TMap<ECountryType, bool> AutoAssignMap;

	UPROPERTY()
	TArray<FProductionOrder> PendingProductionQueue;

	UPROPERTY()
	TMap<FIntPoint, int64> ProductInventory;

	UPROPERTY()
	TMap<FIntPoint, EQualityGrade> ProductGrades;

	// Ticker 핸들 (GameInstanceSubsystem은 Tick 가상함수가 없어 FTSTicker 사용)
	FTSTicker::FDelegateHandle TickerHandle;

	// TryAutoAssign 재진입 방지 플래그
	bool bReentryGuard_AutoAssign = false;

	// ────────────────────────────────────────────
	// 내부 Tick / 헬퍼
	// ────────────────────────────────────────────
	bool TickInternal(float DeltaTime);

	// DeltaTime 경과만큼 모든 채광소 누적 채취 진행. bOutDirty = 변경 여부.
	void TickMines(float DeltaTime, bool& bOutDirty);

	// DeltaTime 경과만큼 모든 공장 라인 진행. bOutDirty = 변경 여부.
	void TickFactories(float DeltaTime, bool& bOutDirty);

	FMiningFacility* FindMineMutable(ECountryType Country, ERawMaterialType Mat);
	FFactoryLine* FindLineMutable(ECountryType Country, int32 LineIndex);

	// 초기 채광소/공장라인 배열 채움 (건설/해금 전 플레이스홀더)
	void InitializeDefaultFacilities();

	// SaveLoadManager 호출 (ItemInventoryManager와 동일 패턴)
	void SaveGameData();
};
