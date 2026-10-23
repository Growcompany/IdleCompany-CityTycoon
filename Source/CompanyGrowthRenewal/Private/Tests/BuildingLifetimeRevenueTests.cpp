#include "Misc/AutomationTest.h"
#include "Data/GameSaveData.h"
#include "Data/RankingData.h"
#include "Manager/RankingManagerSubsystem.h"
#include "JsonObjectConverter.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/StrongObjectPtr.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildingLifetimeRevenuePersistenceTest,
	"CGR.Ranking.LifetimeRevenue.PersistenceAndSnapshot",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBuildingLifetimeRevenuePersistenceTest::RunTest(const FString& Parameters)
{
	FOfficeSaveData LargeVault;
	LargeVault.StoredRevenue = 16777216.f;
	LargeVault.AccrueStoredRevenue(0.25f);
	TestEqual(TEXT("Sub-ULP tick does not increase float vault"), LargeVault.StoredRevenue, 16777216.f);
	TestEqual(TEXT("Lost float increment cannot invent lifetime earnings"), LargeVault.LifetimeRevenueEarned, 0.0);
	FOfficeSaveData NormalVault;
	NormalVault.StoredRevenue = 42.f;
	NormalVault.AccrueStoredRevenue(0.5f);
	TestEqual(TEXT("Representable tick increases vault"), NormalVault.StoredRevenue, 42.5f);
	TestEqual(TEXT("Lifetime records actual vault increment"), NormalVault.LifetimeRevenueEarned, 0.5);

	TStrongObjectPtr<USaveGame_GameData> Save(NewObject<USaveGame_GameData>());
	FOfficeSaveData& Office = Save->GameData.OfficeDataMap.FindOrAdd(17);
	Office.RecordEarnedRevenue(16777216.0);
	Office.RecordEarnedRevenue(0.25);
	Office.RecordEarnedRevenue(1.0);
	Office.RecordEarnedRevenue(0.0);
	Office.RecordEarnedRevenue(-300.0);
	Office.RecordEarnedRevenue(std::numeric_limits<double>::quiet_NaN());
	Office.RecordEarnedRevenue(std::numeric_limits<double>::infinity());
	const double Expected = 16777217.25;
	TestEqual(TEXT("Fractional revenue retained beyond float precision, invalid gains rejected"), Office.LifetimeRevenueEarned, Expected);
	// 미수령액과 프로젝트 생애주기는 누적 기록의 소유자가 아니다.
	Office.StoredRevenue = 42.5f;
	TestEqual(TEXT("First collection drains accrued vault"), Office.TakeStoredRevenue(), 42.5f);
	TestEqual(TEXT("Second collection cannot duplicate vault"), Office.TakeStoredRevenue(), 0.f);
	TestEqual(TEXT("Collection never records earned revenue twice"), Office.LifetimeRevenueEarned, Expected);
	Office.CurrentOperation = FOperationData();
	Office.PendingReport = FProjectReportData();
	TArray<uint8> Bytes;
	if (!TestTrue(TEXT("Serialize save in memory"), UGameplayStatics::SaveGameToMemory(Save.Get(), Bytes))) return false;
	TStrongObjectPtr<USaveGame_GameData> Restored(Cast<USaveGame_GameData>(UGameplayStatics::LoadGameFromMemory(Bytes)));
	if (!TestNotNull(TEXT("Deserialize save in memory"), Restored.Get())) return false;
	const FOfficeSaveData* SavedOffice = Restored->GameData.OfficeDataMap.Find(17);
	if (!TestNotNull(TEXT("Building lifetime revenue survives save roundtrip"), SavedOffice)) return false;
	TestEqual(TEXT("Lifetime survives project reset and save"), SavedOffice->LifetimeRevenueEarned, Expected);
	TestEqual(TEXT("New building starts without invented history"), FOfficeSaveData().LifetimeRevenueEarned, 0.0);

	FBuildingEntitySaveData& Owned = Restored->GameData.Buildings.AddDefaulted_GetRef();
	Owned.BuildingIndex = 17;
	Owned.InteractableName = TEXT("OwnedBuilding");
	Restored->GameData.OfficeDataMap.FindOrAdd(99).RecordEarnedRevenue(1.e12);
	const FCitySnapshot Snapshot = URankingManagerSubsystem::BuildCitySnapshotFromSave(Restored->GameData);
	if (!TestEqual(TEXT("Only currently owned buildings enter snapshot"), Snapshot.Buildings.Num(), 1)) return false;
	TestEqual(TEXT("Revenue joined by BuildingIndex, orphan office excluded"), Snapshot.Buildings[0].LifetimeRevenueEarned, Expected);
	FString Json;
	TestTrue(TEXT("Serialize city snapshot"), FJsonObjectConverter::UStructToJsonObjectString(Snapshot, Json));
	FCitySnapshot Decoded;
	TestTrue(TEXT("Deserialize city snapshot"), FJsonObjectConverter::JsonObjectStringToUStruct(Json, &Decoded));
	if (TestEqual(TEXT("Snapshot retains building"), Decoded.Buildings.Num(), 1))
	{
		TestEqual(TEXT("Snapshot retains exact recorded revenue"), Decoded.Buildings[0].LifetimeRevenueEarned, Expected);
	}
	FOfficeSaveData MonumentA;
	FOfficeSaveData MonumentB;
	MonumentA.RecordProportionalEarnedRevenue(3.0, 1.0, 4.0);
	MonumentB.RecordProportionalEarnedRevenue(3.0, 3.0, 4.0);
	TestEqual(TEXT("Monument allocation retains sub-unit contribution"), MonumentA.LifetimeRevenueEarned, 0.75);
	TestEqual(TEXT("Monument allocations equal actual aggregate wallet payment"), MonumentA.LifetimeRevenueEarned + MonumentB.LifetimeRevenueEarned, 3.0);
	MonumentA.RecordProportionalEarnedRevenue(100.0, 1.0, 0.0);
	TestEqual(TEXT("Invalid monument ratio cannot invent revenue"), MonumentA.LifetimeRevenueEarned, 0.75);
	FCitySnapshotBuilding Unrecorded;
	TestTrue(TEXT("Snapshot without revenue is valid"), FJsonObjectConverter::JsonObjectStringToUStruct(TEXT("{}"), &Unrecorded));
	TestEqual(TEXT("Missing revenue uses zero, not inferred project income"), Unrecorded.LifetimeRevenueEarned, 0.0);
	return true;
}
#endif
