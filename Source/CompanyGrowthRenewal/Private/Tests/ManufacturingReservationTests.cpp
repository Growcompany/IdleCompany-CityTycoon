#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Data/ManufacturingReservationRules.h"
#include "Core/CGGameInstance.h"
#include "Data/GameSaveData.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Global/CGDevSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Manager/ProductionOrderManager.h"
#include "Manager/ManufacturingDevelopmentManager.h"
#include "Manager/EmployeeManager.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/WorldMapManager.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FManufacturingReservationRulesTest,
	"CGR.Manufacturing.ReservationRules", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FManufacturingReservationRulesTest::RunTest(const FString& Parameters)
{
	using namespace ManufacturingReservationRules;
	TestEqual(TEXT("ProductionCount bonus discounts the per-unit price"), DiscountedUnitCost(16.0, 8.0), 8.0);
	TestEqual(TEXT("Negative bonus never raises the price"), DiscountedUnitCost(16.0, -8.0), 16.0);
	const TArray<double> Costs = { 4.0, 4.0, 2.0 };
	const TArray<double> RichTotalButShortField = { 100.0, 3.0, 100.0 };
	TestEqual(TEXT("One deficient required field limits quantity despite rich total"), MaxQuantity(RichTotalButShortField, Costs), 0);
	TestFalse(TEXT("A rich total cannot cover a deficient required field"), CanReserve(RichTotalButShortField, Costs, 1));
	const TArray<double> FractionalBalances = { 10.5, 9.0, 5.0 };
	TestEqual(TEXT("Each required field floors its own affordable quantity"), MaxQuantity(FractionalBalances, Costs), 2);
	TestTrue(TEXT("Exact two-unit field cost fits while fractional remainder stays"), CanReserve(FractionalBalances, Costs, 2));
	TestFalse(TEXT("Three units exceed the second field"), CanReserve(FractionalBalances, Costs, 3));
	TestEqual(TEXT("Discount is applied to each positive field"), DiscountedCosts(Costs, 8.0)[0], 2.0);
	TestEqual(TEXT("Large affordable quantities stop at int32 capacity"), MaxQuantity(TArray<double>{ 1.0e30, 0, 0 }, TArray<double>{ 1, 0, 0 }), MAX_int32);
	TestFalse(TEXT("Zero quantity is rejected"), CanReserve(FractionalBalances, Costs, 0));
	TestFalse(TEXT("Negative quantity is rejected"), CanReserve(FractionalBalances, Costs, -1));
	TestFalse(TEXT("All-zero cost is rejected"), CanReserve(FractionalBalances, TArray<double>{ 0, 0, 0 }, 1));
	TestFalse(TEXT("Malformed cost length is rejected"), CanReserve(FractionalBalances, TArray<double>{ 4.0 }, 1));
	TestFalse(TEXT("NaN cost is rejected"), CanReserve(FractionalBalances, TArray<double>{ std::numeric_limits<double>::quiet_NaN(), 4, 0 }, 1));
	TestFalse(TEXT("Infinite cost is rejected"), CanReserve(FractionalBalances, TArray<double>{ 4, std::numeric_limits<double>::infinity(), 2 }, 1));
	TestFalse(TEXT("Infinite balance is rejected"), CanReserve(TArray<double>{ std::numeric_limits<double>::infinity(), 9, 5 }, Costs, 1));
	TestFalse(TEXT("Negative balance is rejected"), CanReserve(TArray<double>{ 10.5, -1, 5 }, Costs, 1));
	TestFalse(TEXT("Negative cost is rejected"), CanReserve(FractionalBalances, TArray<double>{ -1, 4, 2 }, 1));
	UGameInstance* TestInstance = NewObject<UGameInstance>(GetTransientPackage());
	UProductionOrderManager* OrderManager = NewObject<UProductionOrderManager>(TestInstance);
	FProductionOrder FirstOrder;
	FirstOrder.SourceBuildingID = 7;
	FirstOrder.RemainingQuantity = MAX_int32;
	FProductionOrder SecondOrder = FirstOrder;
	TArray<FProductionOrder> LargeOrders;
	LargeOrders.Add(FirstOrder);
	LargeOrders.Add(SecondOrder);
	OrderManager->SetActiveOrders(LargeOrders);
	TestEqual(TEXT("Building quantity query saturates before int32 overflow"),
		OrderManager->GetTotalRemainingQuantityByBuilding(7), MAX_int32);
	TestEqual(TEXT("Global quantity query saturates before int32 overflow"),
		OrderManager->GetTotalRemainingQuantity(), MAX_int32);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FManufacturingReservationTransactionTest,
	"CGR.Manufacturing.ReservationTransaction", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FManufacturingReservationTransactionTest::RunTest(const FString& Parameters)
{
	if (!FPaths::ProjectSavedDir().Contains(TEXT("Saved_ManufacturingReservation")))
	{
		AddWarning(TEXT("Skipped: run with -saveddirsuffix=ManufacturingReservation to isolate fixture saves"));
		return true;
	}
	if (!TestTrue(TEXT("Engine is available"), GEngine != nullptr)
		|| !TestNull(TEXT("No playable game instance is active"), UCGGameInstance::GetInstance())) return false;

	const FString SlotName(TEXT("GameSlot"));
	if (!TestFalse(TEXT("Isolated fixture slot starts empty"), UGameplayStatics::DoesSaveGameExist(SlotName, 0))) return false;
	UCGDevSettings* Settings = GetMutableDefault<UCGDevSettings>();
	const ECGBackendMode PreviousMode = Settings->BackendMode;
	const bool bPreviousWipeOnLaunch = Settings->bWipeSaveOnLaunch;
	Settings->BackendMode = ECGBackendMode::Local;
	Settings->bWipeSaveOnLaunch = false;
	UCGGameInstance* Instance = nullptr;
	ON_SCOPE_EXIT
	{
		if (Instance)
		{
			UWorld* TestWorld = static_cast<UGameInstance*>(Instance)->GetWorld();
			static_cast<UGameInstance*>(Instance)->Shutdown();
			if (TestWorld) GEngine->DestroyWorldContext(TestWorld);
		}
		UGameplayStatics::DeleteGameInSlot(SlotName, 0);
		Settings->BackendMode = PreviousMode;
		Settings->bWipeSaveOnLaunch = bPreviousWipeOnLaunch;
	};
	USaveGame_GameData* Seed = NewObject<USaveGame_GameData>(GetTransientPackage());
	Seed->GameData.bTutorialCompleted = true;
	FBuildingEntitySaveData Building;
	Building.BuildingIndex = 1;
	Building.BuildingData.CompanyType = ECompanyType::Electronics;
	Seed->GameData.Buildings.Add(Building);
	FBuildingEntitySaveData ServiceBuilding;
	ServiceBuilding.BuildingIndex = 2;
	ServiceBuilding.BuildingData.CompanyType = ECompanyType::Game;
	Seed->GameData.Buildings.Add(ServiceBuilding);
	FOfficeSaveData Office;
	Office.ManufacturingDevelopmentPoints = { 100000.0, 100000.0, 100000.0 };
	Seed->GameData.OfficeDataMap.Add(1, Office);
	if (!TestTrue(TEXT("Fixture save is written in isolated slot"), UGameplayStatics::SaveGameToSlot(Seed, SlotName, 0))) return false;

	Instance = NewObject<UCGGameInstance>(GEngine);
	Instance->InitializeStandalone();
	USaveLoadManager* Saves = Instance->GetSubsystem<USaveLoadManager>();
	UProductionOrderManager* Orders = Instance->GetSubsystem<UProductionOrderManager>();
	UWorldMapManager* WorldMap = Instance->GetSubsystem<UWorldMapManager>();
	if (!TestTrue(TEXT("Real reservation subsystems exist"), Saves && Orders && WorldMap)) return false;
	if (!TestTrue(TEXT("Fixture save loads"), Saves->LoadGameData())) return false;
	const TArray<double> UnitCosts = Orders->GetDevelopmentPointCosts(1, 1);
	if (!TestTrue(TEXT("The first Electronics product has valid three-field costs"), UnitCosts.Num() == 3 && UnitCosts[0] > 0.0 && UnitCosts[1] > 0.0)) return false;
	const TArray<double> StartingPoints = Saves->GetCurrentSaveData()->GameData.OfficeDataMap.FindChecked(1).ManufacturingDevelopmentPoints;
	const FIntPoint ProductKey(static_cast<int32>(ECompanyType::Electronics), 1);
	const int64 StartingStock = WorldMap->GetProductAmount(ProductKey);
	FText Failure;
	const FGameSaveData BeforeRejections = Saves->GetCurrentSaveData()->GameData;
	TestEqual(TEXT("Missing building cannot reserve"), Orders->ReserveProductionQuantity(999, 1, 1, Failure), 0);
	TestEqual(TEXT("Nonmanufacturing building cannot reserve"), Orders->ReserveProductionQuantity(2, 1, 1, Failure), 0);
	TestEqual(TEXT("Locked tier cannot reserve"), Orders->ReserveProductionQuantity(1, 11, 1, Failure), 0);
	TestEqual(TEXT("Zero quantity cannot reserve"), Orders->ReserveProductionQuantity(1, 1, 0, Failure), 0);
	TestEqual(TEXT("Negative quantity cannot reserve"), Orders->ReserveProductionQuantity(1, 1, -1, Failure), 0);
	TestEqual(TEXT("Unaffordable quantity cannot reserve"), Orders->ReserveProductionQuantity(1, 1, MAX_int32, Failure), 0);
	TestTrue(TEXT("Rejected reservations do not change the save"), FGameSaveData::StaticStruct()->CompareScriptStruct(
		&Saves->GetCurrentSaveData()->GameData, &BeforeRejections, 0));
    TestEqual(TEXT("Banked points cannot reserve an undeveloped product"), Orders->ReserveProductionQuantity(1, 1, 1, Failure), 0);
    TestEqual(TEXT("Undeveloped product has no reservable quantity"), Orders->GetMaxReservableQuantity(1, 1), 0);
    auto* Development = Instance->GetSubsystem<UManufacturingDevelopmentManager>();
    auto* Employees = Instance->GetSubsystem<UEmployeeManager>();
    if (!TestTrue(TEXT("Research subsystems exist"), Development && Employees)) return false;
    TestFalse(TEXT("Banked points cannot start research without seated staff"), Development->StartResearch(1, 1, Failure));
    FEmployeeInstance Researcher;
    Researcher.EmployeeID = 42;
    Researcher.AssignedBuildingIndex = 1;
    Researcher.bIsAssigned = true;
    Researcher.DisciplinePoints = {100, 100, 100};
    Researcher.Stats.Stamina = 100;
    Researcher.Stats.Composure = 100;
    Employees->SetEmployeesForBuilding(1, {Researcher});
    FWorkstationSaveData Desk;
    FChairSlotState Chair;
    Chair.OccupantEmployeeID = Researcher.EmployeeID;
    Desk.ChairStates.Add(Chair);
    Saves->GetCurrentSaveData()->GameData.OfficeDataMap.FindChecked(1).OfficeWorkstations.Add(Desk);
    if (!TestTrue(TEXT("Qualified seated team starts actual product development"), Development->StartResearch(1, 1, Failure))) return false;
    TestFalse(TEXT("A second product cannot start concurrently"), Development->StartResearch(1, 2, Failure));
    TestFalse(TEXT("Starting with rich points does not complete research"), Development->GetResearchStatus(1, 1).bResearched);
    const auto* ResearchDisk = Cast<USaveGame_GameData>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
    if (!TestNotNull(TEXT("Research start is persisted"), ResearchDisk)) return false;
    TestEqual(TEXT("Saved active research identifies the product"), ResearchDisk->GameData.OfficeDataMap.FindChecked(1).ManufacturingResearch.ProjectIndex, 1);
    Development->ApplyOfflineProgress(7200.f, FDateTime::UtcNow());
    if (!TestTrue(TEXT("Qualified real offline work completes research"), Development->GetResearchStatus(1, 1).bResearched)) return false;
    TestEqual(TEXT("First development alone records one tier clear"), Saves->GetCurrentSaveData()->GameData.OfficeDataMap.FindChecked(1).TierProgress.ClearedProjects.Num(), 1);
    TestFalse(TEXT("Completed product cannot be researched twice"), Development->StartResearch(1, 1, Failure));
    Employees->SetEmployeesForBuilding(1, {});
    Saves->GetCurrentSaveData()->GameData.OfficeDataMap.FindChecked(1).OfficeWorkstations.Reset();
    TestFalse(TEXT("Finished research leaves no active development"), Development->GetActiveResearchStatus(1).bInProgress);
    TestEqual(TEXT("Offline completion queues the product result"), Development->GetPendingResearchResult(1), 1);
    if (!TestTrue(TEXT("Completed research and its pending result save together"), Saves->SaveGameData())) return false;
    const auto* CompletedDisk = Cast<USaveGame_GameData>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
    if (!TestNotNull(TEXT("Completed research reloads from disk"), CompletedDisk)) return false;
    TestTrue(TEXT("Offline result survives until explicit acknowledgement"),
        CompletedDisk->GameData.OfficeDataMap.FindChecked(1).PendingManufacturingResearchResults.Contains(1));
    Saves->GetCurrentSaveData()->GameData.OfficeDataMap.FindChecked(1).ManufacturingDevelopmentPoints = StartingPoints;
	FOfficeSaveData& ShortOffice = Saves->GetCurrentSaveData()->GameData.OfficeDataMap.FindChecked(1);
	ShortOffice.ManufacturingDevelopmentPoints[1] = 0.0;
	const FGameSaveData BeforeFieldShortage = Saves->GetCurrentSaveData()->GameData;
	TestEqual(TEXT("A missing required field rejects reservation despite sufficient total"),
		Orders->ReserveProductionQuantity(1, 1, 1, Failure), 0);
	TestTrue(TEXT("Field shortage leaves all balances, orders and tier unchanged"), FGameSaveData::StaticStruct()->CompareScriptStruct(
		&Saves->GetCurrentSaveData()->GameData, &BeforeFieldShortage, 0));
	Saves->GetCurrentSaveData()->GameData.OfficeDataMap.FindChecked(1).ManufacturingDevelopmentPoints = StartingPoints;
	const int32 OrderID = Orders->ReserveProductionQuantity(1, 1, 3, Failure);
	if (!TestTrue(TEXT("Three units are reserved through the real save path"), OrderID > 0)) return false;
	const FProductionOrder* Order = Orders->GetOrderByID(OrderID);
	if (!TestTrue(TEXT("Reservation creates a production order"), Order != nullptr)) return false;
	TestEqual(TEXT("Reserved order has exactly three units"), Order->RemainingQuantity, 3);
	for (int32 Field = 0; Field < 3; ++Field)
	{
		TestTrue(FString::Printf(TEXT("Reservation deducts field %d three times"), Field), FMath::IsNearlyEqual(
			Saves->GetCurrentSaveData()->GameData.OfficeDataMap.FindChecked(1).ManufacturingDevelopmentPoints[Field],
			StartingPoints[Field] - UnitCosts[Field] * 3.0));
	}
	TestEqual(TEXT("Reserving does not grant finished goods"), WorldMap->GetProductAmount(ProductKey), StartingStock);
	const USaveGame_GameData* Disk = Cast<USaveGame_GameData>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
	if (!TestTrue(TEXT("Committed reservation is persisted"), Disk != nullptr)) return false;
	TestEqual(TEXT("Disk records the reserved order"), Disk->GameData.ProductionOrders.Num(), 1);
	for (int32 Field = 0; Field < 3; ++Field)
	{
		TestTrue(FString::Printf(TEXT("Disk records reduced field %d"), Field), FMath::IsNearlyEqual(
			Disk->GameData.OfficeDataMap.FindChecked(1).ManufacturingDevelopmentPoints[Field], StartingPoints[Field] - UnitCosts[Field] * 3.0));
	}
	const int32 SplitOrderID = Orders->ReserveProductionQuantity(1, 1, 2, Failure);
	if (!TestTrue(TEXT("A second reservation of the same product succeeds"), SplitOrderID > OrderID)) return false;
	TestEqual(TEXT("Split reservations issue five units in total"), Orders->GetTotalRemainingQuantityByBuilding(1), 5);
	for (int32 Field = 0; Field < 3; ++Field)
	{
		TestTrue(FString::Printf(TEXT("Split reservations spend five field %d costs"), Field), FMath::IsNearlyEqual(
			Saves->GetCurrentSaveData()->GameData.OfficeDataMap.FindChecked(1).ManufacturingDevelopmentPoints[Field],
			StartingPoints[Field] - UnitCosts[Field] * 5.0));
	}
	TestEqual(TEXT("Repeated reservation records the product only once for tier progress"),
		Saves->GetCurrentSaveData()->GameData.OfficeDataMap.FindChecked(1).TierProgress.ClearedProjects.Num(), 1);
	TestEqual(TEXT("Split reservations still grant no goods"), WorldMap->GetProductAmount(ProductKey), StartingStock);

	const FGameSaveData BeforeFailedSave = Saves->GetCurrentSaveData()->GameData;
	const int32 BeforeNextOrderID = Orders->GetNextOrderID();
	Saves->SuppressSaving();
	TestEqual(TEXT("Suppressed save rejects another reservation"), Orders->ReserveProductionQuantity(1, 1, 2, Failure), 0);
	TestTrue(TEXT("Failed save restores the entire cached save data"), FGameSaveData::StaticStruct()->CompareScriptStruct(
		&Saves->GetCurrentSaveData()->GameData, &BeforeFailedSave, 0));
	TestEqual(TEXT("Failed save restores the order list"), Orders->GetAllOrders().Num(), 2);
	TestEqual(TEXT("Failed save restores the next order ID"), Orders->GetNextOrderID(), BeforeNextOrderID);
	TestEqual(TEXT("Failed save leaves finished goods unchanged"), WorldMap->GetProductAmount(ProductKey), StartingStock);
	TestFalse(TEXT("Suppressed save rejects result acknowledgement"), Development->AcknowledgeResearchResult(1, 1));
	TestEqual(TEXT("Failed acknowledgement preserves the pending product"), Development->GetPendingResearchResult(1), 1);
	return true;
}

#endif
