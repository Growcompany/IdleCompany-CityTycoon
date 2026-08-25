#include "Manager/ProductionOrderManager.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/WorldMapManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Core/CGGameInstance.h"
#include "Manager/BuildingTraitManagerSubsystem.h"
#include "Enum/BuildingTraitTarget.h"
#include "Data/GameSaveData.h"
#include "Enum/CompanyType.h"
#include "Data/BuildingEnhancementData.h"
#include "Table/TestProductionOrderTable.h"
#include "Table/ProductRecipeTable.h"
#include "Manager/ResourceItemManager.h"
#include "Engine/DataTable.h"

UProductionOrderManager::UProductionOrderManager()
{
	NextOrderID = 1;
}

void UProductionOrderManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ActiveOrders.Empty();

	UE_LOG(LogTemp, Log, TEXT("[ProductionOrderManager] Initialized"));
}

void UProductionOrderManager::Deinitialize()
{
	Super::Deinitialize();
	UE_LOG(LogTemp, Log, TEXT("[ProductionOrderManager] Deinitialized"));
}

// ===== 주문서 생성 =====

int32 UProductionOrderManager::CreateOrder(const FStageProgressData& StageData, int32 BuildingID)
{
	FProductionOrder NewOrder;
	NewOrder.OrderID = NextOrderID++;
	NewOrder.CompanyType = StageData.CompanyType;
	NewOrder.ProjectIndex = StageData.ProjectNumber;
	NewOrder.ProductID = FName(*FString::Printf(TEXT("Product_%d_%d"), StageData.ProjectNumber, StageData.StageNumber));
	NewOrder.ProductName = StageData.ProjectName;
	NewOrder.SourceBuildingID = BuildingID;
	NewOrder.QualityScore = StageData.CalculateQualityScore();
	// ProjectGrade 배율 적용 (Phase 3) — 제조업 생산주문서 품질 등급 상승
	{
		float QualityMult = 1.0f;
		if (UCGGameInstance* GI = UCGGameInstance::GetInstance())
		{
			if (USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>())
			{
				if (USaveGame_GameData* SaveData = SaveMgr->GetCurrentSaveData())
				{
					for (const FBuildingEntitySaveData& B : SaveData->GameData.Buildings)
					{
						if (B.BuildingIndex == BuildingID)
						{
							const int32 Lv = B.BuildingData.EnhancementLevels.FindRef(EBuildingEnhancementType::ProjectGrade);
							QualityMult = UBuildingEnhancementHelper::CalculateEffectMultiplier(EBuildingEnhancementType::ProjectGrade, Lv);
							break;
						}
					}
				}
			}
		}
		// R&D 특성 — 출시 품질 배율 (강화 QualityMult 와 곱셈 결합)
		float QualityTraitMult = 1.0f;
		if (UCGGameInstance* QGI = UCGGameInstance::GetInstance())
		{
			if (UBuildingTraitManagerSubsystem* QTraitMgr = QGI->GetSubsystem<UBuildingTraitManagerSubsystem>())
			{
				QualityTraitMult = QTraitMgr->GetTraitFactor(BuildingID, EBuildingTraitTarget::Quality);
			}
		}
		NewOrder.QualityScore = FMath::Clamp(NewOrder.QualityScore * QualityMult * QualityTraitMult, 0.5f, 2.0f);
	}
	// 제조는 6등급 유지 — 프로젝트 4등급화(2026-07-30)는 출시 게이트 하한 논리라 게이트 없는 양산엔 적용 안 됨.
	NewOrder.Grade = ProductionScoreToGrade(NewOrder.QualityScore);
	NewOrder.Quantity = GetBaseProductionQuantity(NewOrder.Grade);
	// 제조전용 특성 — 개수 가산
	if (UCGGameInstance* ProdGI = UCGGameInstance::GetInstance())
	{
		if (UBuildingTraitManagerSubsystem* ProdTraitMgr = ProdGI->GetSubsystem<UBuildingTraitManagerSubsystem>())
		{
			NewOrder.Quantity += FMath::RoundToInt(ProdTraitMgr->GetAggregatedTraitPercent(BuildingID, EBuildingTraitTarget::ProductionCount));
		}
	}
	NewOrder.RemainingQuantity = NewOrder.Quantity;

	int32 OrderIndex = ActiveOrders.Add(NewOrder);

	OnOrderCreated.Broadcast(BuildingID, NewOrder);

	// 제조업 회사면 월드맵 공장 큐에 자동 추가 (자동 배정 활성화 나라는 즉시 라인에 배정됨)
	if (IsManufacturingType(NewOrder.CompanyType))
	{
		if (UWorldMapManager* WorldMapMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UWorldMapManager>() : nullptr)
		{
			WorldMapMgr->EnqueueProductionOrder(NewOrder);
			UE_LOG(LogTemp, Log, TEXT("[ProductionOrderManager] Enqueued to WorldMap: OrderID=%d"), NewOrder.OrderID);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[ProductionOrderManager] Created Order: ID=%d, Product=%s, Qty=%d, Grade=%s, Building=%d"),
		NewOrder.OrderID,
		*NewOrder.ProductName,
		NewOrder.Quantity,
		*QualityGradeToAlphabetString(NewOrder.Grade),
		BuildingID);

	return NewOrder.OrderID;
}

// ===== Test/Debug =====

int32 UProductionOrderManager::CreateTestOrder(ECompanyType InCompanyType, int32 InProjectIndex, const FString& InProductName,
	int32 InQuantity, EQualityGrade InGrade, int32 InSourceBuildingID)
{
	if (InQuantity <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ProductionOrderManager] CreateTestOrder: Quantity must be > 0"));
		return -1;
	}

	FProductionOrder NewOrder;
	NewOrder.OrderID = NextOrderID++;
	NewOrder.CompanyType = InCompanyType;
	NewOrder.ProjectIndex = InProjectIndex;
	NewOrder.ProductID = FName(*FString::Printf(TEXT("Test_%d_%d"), static_cast<int32>(InCompanyType), InProjectIndex));
	NewOrder.ProductName = InProductName;
	NewOrder.SourceBuildingID = InSourceBuildingID;
	NewOrder.Grade = InGrade;
	NewOrder.QualityScore = 1.0f;
	NewOrder.Quantity = InQuantity;
	NewOrder.RemainingQuantity = InQuantity;

	ActiveOrders.Add(NewOrder);
	OnOrderCreated.Broadcast(InSourceBuildingID, NewOrder);

	if (IsManufacturingType(NewOrder.CompanyType))
	{
		if (UWorldMapManager* WorldMapMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UWorldMapManager>() : nullptr)
		{
			WorldMapMgr->EnqueueProductionOrder(NewOrder);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[ProductionOrderManager] CreateTestOrder: ID=%d, Product=%s, Qty=%d"),
		NewOrder.OrderID, *NewOrder.ProductName, NewOrder.Quantity);

	return NewOrder.OrderID;
}

int32 UProductionOrderManager::SpawnTestOrdersFromTable(int32 RandomCount)
{
	UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableMgr) return 0;

	UDataTable* DT = TableMgr->GetTestProductionOrdersTable();
	if (!DT)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ProductionOrderManager] DT_TestProductionOrders not loaded — assets/CSV not yet created"));
		return 0;
	}

	// 유효 행만 1차 수집
	TArray<const FTestProductionOrderRow*> ValidRows;
	for (const TPair<FName, uint8*>& Pair : DT->GetRowMap())
	{
		const FTestProductionOrderRow* Row = reinterpret_cast<FTestProductionOrderRow*>(Pair.Value);
		if (!Row) continue;
		if (Row->CompanyType == ECompanyType::None || Row->Quantity <= 0) continue;
		ValidRows.Add(Row);
	}

	// RandomCount 양수면 무작위 sampling
	if (RandomCount > 0 && RandomCount < ValidRows.Num())
	{
		// Fisher-Yates 부분 셔플 — 앞 RandomCount 개만 셔플 결과로 채움
		for (int32 i = 0; i < RandomCount; ++i)
		{
			const int32 Swap = FMath::RandRange(i, ValidRows.Num() - 1);
			ValidRows.Swap(i, Swap);
		}
		ValidRows.SetNum(RandomCount);
	}

	int32 SpawnedCount = 0;
	for (const FTestProductionOrderRow* Row : ValidRows)
	{
		const int32 NewID = CreateTestOrder(Row->CompanyType, Row->ProjectIndex, Row->ProductName,
			Row->Quantity, Row->Grade, Row->SourceBuildingID);
		if (NewID > 0)
		{
			++SpawnedCount;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[ProductionOrderManager] SpawnTestOrdersFromTable: %d orders created (Random=%d)"),
		SpawnedCount, RandomCount);
	return SpawnedCount;
}

// ===== 주문서 조회 =====

TArray<FProductionOrder> UProductionOrderManager::GetOrdersByBuilding(int32 BuildingID) const
{
	TArray<FProductionOrder> Result;
	for (const FProductionOrder& Order : ActiveOrders)
	{
		if (Order.SourceBuildingID == BuildingID && !Order.IsConsumed())
		{
			Result.Add(Order);
		}
	}
	return Result;
}

FProductionOrder* UProductionOrderManager::GetOrderByID(int32 OrderID)
{
	for (FProductionOrder& Order : ActiveOrders)
	{
		if (Order.OrderID == OrderID)
		{
			return &Order;
		}
	}
	return nullptr;
}

int32 UProductionOrderManager::GetTotalRemainingQuantityByBuilding(int32 BuildingID) const
{
	int32 Total = 0;
	for (const FProductionOrder& Order : ActiveOrders)
	{
		if (Order.SourceBuildingID == BuildingID)
		{
			Total += Order.RemainingQuantity;
		}
	}
	return Total;
}

int32 UProductionOrderManager::GetTotalRemainingQuantity() const
{
	int32 Total = 0;
	for (const FProductionOrder& Order : ActiveOrders)
	{
		Total += Order.RemainingQuantity;
	}
	return Total;
}

int32 UProductionOrderManager::GetMaxProducible(const FProductionOrder& Order,
	EResourceType& OutBottleneck, bool& bOutMaterialBound) const
{
	OutBottleneck = EResourceType::None;
	bOutMaterialBound = false;

	const int32 OrderCap = FMath::Max(0, Order.RemainingQuantity);
	if (OrderCap <= 0) return 0;

	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	UResourceItemManager* ResMgr = GI ? GI->GetSubsystem<UResourceItemManager>() : nullptr;
	if (!TableMgr || !ResMgr) return 0;

	bool bRecipeOk = false;
	const FProductRecipeTable Recipe = TableMgr->GetProductRecipe(Order.CompanyType, Order.ProjectIndex, bRecipeOk);
	if (!bRecipeOk)
	{
		// Recipe 누락은 어뷰즈 통로 — fail-closed (ProductionCard 의 bRecipeMissing 과 같은 판단)
		UE_LOG(LogTemp, Warning,
			TEXT("[ProductionOrder] DT_Recipe 누락 — CompanyType=%d, ProjectIndex=%d"),
			static_cast<int32>(Order.CompanyType), Order.ProjectIndex);
		return 0;
	}

	// 재료 목록은 FProductRecipeTable 이 단일 출처 — 여기서 10종을 다시 나열하면 원자재가 늘 때 어긋난다
	TArray<TPair<EResourceType, int32>> Materials;
	Recipe.CollectMaterials(Materials);

	int32 MaterialCap = MAX_int32;
	double TightestRatio = TNumericLimits<double>::Max();

	for (const TPair<EResourceType, int32>& M : Materials)
	{
		const int32 PerUnit = M.Value;
		const int64 Have = ResMgr->GetResourceAmount(M.Key);
		const int32 Can = static_cast<int32>(FMath::Min<int64>(Have / PerUnit, MAX_int32));
		const double Ratio = static_cast<double>(Have) / static_cast<double>(PerUnit);

		// 동률이면 실수 비율이 더 빠듯한 쪽을 병목으로 — 정수 내림만 보면 한 개만 더 캐도
		// 풀리는 재료를 상한으로 잘못 지목한다 (62/10=6.2 vs 108/18=6.0 둘 다 6)
		if (Can < MaterialCap || (Can == MaterialCap && Ratio < TightestRatio))
		{
			MaterialCap = Can;
			TightestRatio = Ratio;
			OutBottleneck = M.Key;
		}
	}

	// 재료 0종 = 무료 주문(의도된 디자인). 주문서 잔여만 상한.
	if (Materials.Num() == 0)
	{
		OutBottleneck = EResourceType::None;
		return OrderCap;
	}

	bOutMaterialBound = (MaterialCap < OrderCap);
	if (!bOutMaterialBound)
	{
		OutBottleneck = EResourceType::None;
	}
	return FMath::Min(MaterialCap, OrderCap);
}

// ===== 수량 소비 =====

int32 UProductionOrderManager::ConsumeQuantity(int32 OrderID, int32 Amount)
{
	FProductionOrder* Order = GetOrderByID(OrderID);
	if (!Order)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ProductionOrderManager] Order not found: ID=%d"), OrderID);
		return 0;
	}

	int32 Consumed = Order->ConsumeQuantity(Amount);

	OnOrderConsumed.Broadcast(OrderID, Consumed, Order->RemainingQuantity);

	UE_LOG(LogTemp, Log, TEXT("[ProductionOrderManager] Consumed: OrderID=%d, Amount=%d, Remaining=%d"),
		OrderID, Consumed, Order->RemainingQuantity);

	if (Order->IsConsumed())
	{
		OnOrderDepleted.Broadcast(OrderID);
		UE_LOG(LogTemp, Log, TEXT("[ProductionOrderManager] Order depleted: ID=%d"), OrderID);
	}

	return Consumed;
}

void UProductionOrderManager::CleanupDepletedOrders()
{
	int32 RemovedCount = ActiveOrders.RemoveAll([](const FProductionOrder& Order)
	{
		return Order.IsConsumed();
	});

	if (RemovedCount > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[ProductionOrderManager] Cleaned up %d depleted orders"), RemovedCount);
	}
}

// ===== Save/Load =====

void UProductionOrderManager::SetActiveOrders(const TArray<FProductionOrder>& InOrders)
{
	ActiveOrders = InOrders;
	UE_LOG(LogTemp, Log, TEXT("[ProductionOrderManager] Loaded %d orders"), ActiveOrders.Num());
}

void UProductionOrderManager::LoadOrdersFromSave()
{
	if (ActiveOrders.Num() > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[ProductionOrderManager] LoadOrdersFromSave - Skipped, already have %d orders"),
			ActiveOrders.Num());
		return;
	}

	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	if (!GI) return;

	USaveLoadManager* SaveLoadMgr = GI->GetSubsystem<USaveLoadManager>();
	if (!SaveLoadMgr) return;

	USaveGame_GameData* SaveData = SaveLoadMgr->GetCurrentSaveData();
	if (!SaveData) return;

	ActiveOrders = SaveData->GameData.ProductionOrders;
	NextOrderID = SaveData->GameData.NextProductionOrderID;

	UE_LOG(LogTemp, Log, TEXT("[ProductionOrderManager] Loaded %d orders from save, NextID=%d"),
		ActiveOrders.Num(), NextOrderID);

	// 소진 주문은 세이브에 남길 이유 없음
	CleanupDepletedOrders();

#if !UE_BUILD_SHIPPING
	// 테스트 시드는 세션-로컬 — 이전 세션이 저장한 시드부터 제거 후 새로 뿌린다.
	// 시드 판정 = CreateTestOrder 가 박는 "Test_" ProductID 마커 (SourceBuildingID 는 DT 행이 유효 ID 를 넣으면 뚫리는 간접 프록시).
	// 제거 없이 로드마다 전량 재시드하면 무한 누적 (2026-07-10 세이브 9.6MB 중 97%가 시드 13,656개였던 사고)
	const int32 PurgedSeeds = ActiveOrders.RemoveAll([](const FProductionOrder& Order)
	{
		return Order.ProductID.ToString().StartsWith(TEXT("Test_"));
	});
	if (PurgedSeeds > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[ProductionOrderManager] Purged %d stale test-seed orders from save"), PurgedSeeds);
	}

	const int32 TestSpawned = SpawnTestOrdersFromTable();
	if (TestSpawned > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[ProductionOrderManager] Auto-spawned %d test orders on load"), TestSpawned);
	}
#endif
}
