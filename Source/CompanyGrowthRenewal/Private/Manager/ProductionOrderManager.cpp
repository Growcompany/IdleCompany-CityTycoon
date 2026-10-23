#include "Manager/ProductionOrderManager.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/WorldMapManager.h"
#include "Data/WorldMapTypes.h"
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
#include "Manager/ManufacturingDevelopmentManager.h"
#include "Manager/MissionManagerSubsystem.h"
#include "Manager/OfficeStageProgressManager.h"
#include "Data/ManufacturingReservationRules.h"
#include "Data/ProjectBoardData.h"
#include "Table/ProjectDataTable.h"
#include "Engine/World.h"
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

// ===== 생산 가능 수량 예약 =====

namespace
{
	struct FReservationContext
	{
		USaveLoadManager* SaveMgr = nullptr;
		USaveGame_GameData* SaveData = nullptr;
		UManufacturingDevelopmentManager* Development = nullptr;
		ECompanyType Industry = ECompanyType::None;
		FProjectData Project;
		FProjectTierProgress Tier;
		TArray<double> UnitCosts;
	};

	bool ResolveReservation(UGameInstance* Instance, int32 BuildingID, int32 ProjectIndex, FReservationContext& Out)
	{
		if (!Instance || BuildingID < 0 || ProjectIndex <= 0) return false;
		Out.SaveMgr = Instance->GetSubsystem<USaveLoadManager>();
		UTableManagerSubsystem* Tables = Instance->GetSubsystem<UTableManagerSubsystem>();
		Out.Development = Instance->GetSubsystem<UManufacturingDevelopmentManager>();
		if (!Out.SaveMgr || !Tables || !Out.Development) return false;
		Out.SaveData = Out.SaveMgr->GetCurrentSaveData();
		Out.Industry = Out.SaveMgr->GetBuildingCompanyType(BuildingID);
		if (!Out.SaveData || !IsManufacturingType(Out.Industry)) return false;
		bool bProjectFound = false;
		Out.Project = Tables->GetProjectData(Out.Industry, ProjectIndex, bProjectFound);
		bool bRecipeFound = false;
		const FProductRecipeTable Recipe = Tables->GetProductRecipe(Out.Industry, ProjectIndex, bRecipeFound);
		if (!bProjectFound || !bRecipeFound || Out.Project.CompanyType != Out.Industry
			|| Out.Project.ProjectIndex != ProjectIndex || Recipe.CompanyType != Out.Industry
			|| Recipe.ProjectIndex != ProjectIndex) return false;

		if (const FOfficeSaveData* Office = Out.SaveData->GameData.OfficeDataMap.Find(BuildingID))
		{
			Out.Tier = Office->TierProgress;
		}
		if (const UCGGameInstance* Game = Cast<UCGGameInstance>(Instance))
		{
			if (Game->GetCurrentManagedBuildingIndex() == BuildingID && Game->GetWorld()
				&& Game->GetWorld()->GetMapName().Contains(TEXT("OfficeMap")))
			{
				if (const UOfficeStageProgressManager* Stage = Game->GetWorld()->GetSubsystem<UOfficeStageProgressManager>())
				{
					Out.Tier = Stage->GetTierProgress();
				}
			}
		}
		const int32 RequiredTier = FProjectTierProgress::GetTierForProject(ProjectIndex);
		if (RequiredTier <= 0 || RequiredTier > Out.Tier.CurrentTier) return false;

		double Bonus = 0.0;
		if (const UBuildingTraitManagerSubsystem* Traits = Instance->GetSubsystem<UBuildingTraitManagerSubsystem>())
		{
			Bonus = Traits->GetAggregatedTraitPercent(BuildingID, EBuildingTraitTarget::ProductionCount);
		}
		Out.UnitCosts = ManufacturingReservationRules::DiscountedCosts(Recipe.DevelopmentPointCosts, Bonus);
		return ManufacturingReservationRules::HasValidCosts(Out.UnitCosts);
	}
}

TArray<double> UProductionOrderManager::GetDevelopmentPointCosts(int32 BuildingID, int32 ProjectIndex) const
{
	FReservationContext Context;
	return ResolveReservation(GetGameInstance(), BuildingID, ProjectIndex, Context) ? Context.UnitCosts : TArray<double>();
}

int32 UProductionOrderManager::GetMaxReservableQuantity(int32 BuildingID, int32 ProjectIndex) const
{
	FReservationContext Context;
	if (!ResolveReservation(GetGameInstance(), BuildingID, ProjectIndex, Context)
        || !Context.Tier.ClearedProjects.Contains(ProjectIndex)) return 0;
	return ManufacturingReservationRules::MaxQuantity(
		Context.Development->GetDisciplinePoints(BuildingID), Context.UnitCosts);
}

int32 UProductionOrderManager::ReserveProductionQuantity(int32 BuildingID, int32 ProjectIndex, int32 Quantity, FText& OutFailure)
{
	OutFailure = NSLOCTEXT("ManufacturingReservation", "Unavailable", "생산 수량을 확보할 수 없습니다");
	if (bReservationInProgress || Quantity <= 0 || NextOrderID <= 0 || NextOrderID == MAX_int32) return 0;
	TGuardValue<bool> ReservationGuard(bReservationInProgress, true);
	UGameInstance* Instance = GetGameInstance();
	if (const UCGGameInstance* Game = Cast<UCGGameInstance>(Instance))
	{
		if (Game->IsVisitMode()) return 0;
	}
	UManufacturingDevelopmentManager* Development = Instance ? Instance->GetSubsystem<UManufacturingDevelopmentManager>() : nullptr;
	if (!Development) return 0;
	Development->AccrueOnline();

	FReservationContext Context;
	if (!ResolveReservation(Instance, BuildingID, ProjectIndex, Context)) return 0;
    if (!Context.Tier.ClearedProjects.Contains(ProjectIndex))
    {
        OutFailure = NSLOCTEXT("ManufacturingReservation", "ResearchRequired", "먼저 제품 개발을 완료하세요");
        return 0;
    }
	const FOfficeSaveData* ExistingOffice = Context.SaveData->GameData.OfficeDataMap.Find(BuildingID);
	const TArray<double> PreviousPoints = ExistingOffice ? ExistingOffice->ManufacturingDevelopmentPoints : TArray<double>();
	if (!ManufacturingReservationRules::CanReserve(PreviousPoints, Context.UnitCosts, Quantity))
	{
		OutFailure = NSLOCTEXT("ManufacturingReservation", "InsufficientPoints", "개발 포인트가 부족합니다");
		return 0;
	}
	TArray<double> RemainingPoints = PreviousPoints;
	for (int32 Field = 0; Field < RemainingPoints.Num(); ++Field)
	{
		RemainingPoints[Field] -= Context.UnitCosts[Field] * static_cast<double>(Quantity);
		if (!FMath::IsFinite(RemainingPoints[Field]) || RemainingPoints[Field] < 0.0) return 0;
	}

	float Quality = 1.0f;
	if (const FBuildingEntitySaveData* Building = Context.SaveData->GameData.Buildings.FindByPredicate(
		[BuildingID](const FBuildingEntitySaveData& Entry) { return Entry.BuildingIndex == BuildingID; }))
	{
		const int32 Level = Building->BuildingData.EnhancementLevels.FindRef(EBuildingEnhancementType::ProjectGrade);
		Quality *= UBuildingEnhancementHelper::CalculateEffectMultiplier(EBuildingEnhancementType::ProjectGrade, Level);
	}
	if (const UBuildingTraitManagerSubsystem* Traits = Instance->GetSubsystem<UBuildingTraitManagerSubsystem>())
	{
		Quality *= Traits->GetTraitFactor(BuildingID, EBuildingTraitTarget::Quality);
	}
	if (!FMath::IsFinite(Quality)) return 0;

	FProductionOrder NewOrder;
	NewOrder.OrderID = NextOrderID;
	NewOrder.CompanyType = Context.Industry;
	NewOrder.ProjectIndex = ProjectIndex;
	NewOrder.ProductID = FName(*FString::Printf(TEXT("Product_%d_%d"), static_cast<int32>(Context.Industry), ProjectIndex));
	NewOrder.ProductName = Context.Project.ProjectName.ToString();
	NewOrder.SourceBuildingID = BuildingID;
	NewOrder.QualityScore = FMath::Clamp(Quality, 0.5f, 2.0f);
	NewOrder.Grade = ProductionScoreToGrade(NewOrder.QualityScore);
	NewOrder.Quantity = Quantity;
	NewOrder.RemainingQuantity = Quantity;

	FOfficeSaveData& Office = Context.SaveData->GameData.OfficeDataMap.FindOrAdd(BuildingID);
	Office.ManufacturingDevelopmentPoints = RemainingPoints;
	ActiveOrders.Add(NewOrder);
	++NextOrderID;
	if (!Context.SaveMgr->SaveGameData())
	{
		ActiveOrders.Pop();
		--NextOrderID;
        FOfficeSaveData& Restore = Context.SaveData->GameData.OfficeDataMap.FindChecked(BuildingID);
        for (int32 Field = 0; Field < PreviousPoints.Num(); ++Field)
        {
            Restore.ManufacturingDevelopmentPoints[Field] += PreviousPoints[Field] - RemainingPoints[Field];
        }
		return 0;
	}

	Context.Development->OnPointsChanged.Broadcast(BuildingID);
	OnOrderCreated.Broadcast(BuildingID, NewOrder);
	if (UMissionManagerSubsystem* Missions = Instance->GetSubsystem<UMissionManagerSubsystem>())
	{
		Missions->NotifyReserveManufacturingProduct(NewOrder.Quantity);
	}
	OutFailure = FText::GetEmpty();
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
	int64 Total = 0;
	for (const FProductionOrder& Order : ActiveOrders)
	{
		if (Order.SourceBuildingID == BuildingID)
		{
			Total += FMath::Max(0, Order.RemainingQuantity);
		}
	}
	return static_cast<int32>(FMath::Min<int64>(Total, MAX_int32));
}

int32 UProductionOrderManager::GetTotalRemainingQuantity() const
{
	int64 Total = 0;
	for (const FProductionOrder& Order : ActiveOrders)
	{
		Total += FMath::Max(0, Order.RemainingQuantity);
	}
	return static_cast<int32>(FMath::Min<int64>(Total, MAX_int32));
}

int32 UProductionOrderManager::GetMaxProducible(const FProductionOrder& Order,
	EResourceType& OutBottleneck, bool& bOutSupplyBound) const
{
	OutBottleneck = EResourceType::None;
	bOutSupplyBound = false;

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

	// 제품 입력(칩) 상한 — 재료와 별개 축이다. 제품이 더 빠듯하면 자원을 병목으로 지목하면 안 된다
	// ("광석을 캐라"는 틀린 안내가 나간다).
	int32 ProductCap = MAX_int32;
	{
		int32 NeedIndex = 0;
		int32 NeedQty = 0;
		if (Recipe.GetProductInput(NeedIndex, NeedQty))
		{
			UWorldMapManager* WorldMap = GI->GetSubsystem<UWorldMapManager>();
			const int64 Have = WorldMap ? WorldMap->GetProductAmount(MakeProductKey(Order.CompanyType, NeedIndex)) : 0;
			ProductCap = static_cast<int32>(FMath::Min<int64>(Have / NeedQty, MAX_int32));
		}
	}

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

	// 재료 0종 = 무료 주문(의도된 디자인). 주문서 잔여와 제품 상한만 상한.
	if (Materials.Num() == 0)
	{
		return ArbitrateProducibleCap(MAX_int32, ProductCap, OrderCap, OutBottleneck, bOutSupplyBound);
	}

	return ArbitrateProducibleCap(MaterialCap, ProductCap, OrderCap, OutBottleneck, bOutSupplyBound);
}

bool UProductionOrderManager::IsProductInputShort(const FProductionOrder& Order, int32 DesiredQty,
	int32& OutNeededProjectIndex, int32& OutShortBy) const
{
	OutNeededProjectIndex = 0;
	OutShortBy = 0;
	if (DesiredQty <= 0) return false;

	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	UWorldMapManager* WorldMap = GI ? GI->GetSubsystem<UWorldMapManager>() : nullptr;
	if (!TableMgr || !WorldMap) return false;

	bool bRecipeOk = false;
	const FProductRecipeTable Recipe = TableMgr->GetProductRecipe(Order.CompanyType, Order.ProjectIndex, bRecipeOk);
	if (!bRecipeOk) return false;

	int32 NeedIndex = 0;
	int32 NeedQty = 0;
	if (!Recipe.GetProductInput(NeedIndex, NeedQty)) return false;

	const int64 Required = static_cast<int64>(NeedQty) * static_cast<int64>(DesiredQty);
	const int64 Have = WorldMap->GetProductAmount(MakeProductKey(Order.CompanyType, NeedIndex));
	if (Have >= Required) return false;

	OutNeededProjectIndex = NeedIndex;
	OutShortBy = static_cast<int32>(FMath::Min<int64>(Required - Have, MAX_int32));
	return true;
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

#endif
}
