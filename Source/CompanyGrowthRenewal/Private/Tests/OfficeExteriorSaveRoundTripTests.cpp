#include "Data/GameSaveData.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROfficeExteriorSaveGameMemoryRoundTripTest,
	"CGR.Office.Exterior.SaveGame.MemoryRoundTrip",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGROfficeExteriorSaveGameMemoryRoundTripTest::RunTest(const FString& Parameters)
{
	constexpr int32 SentinelBuildingIndex = 987654321;
	const FName ExpectedFloorRowName(TEXT("OfficeFloor_ExteriorSaveRoundTrip"));

	USaveGame_GameData* SourceSave = NewObject<USaveGame_GameData>(GetTransientPackage());
	if (!TestNotNull(TEXT("transient source save game"), SourceSave))
	{
		return false;
	}

	FOfficeSaveData SourceOfficeData;
	SourceOfficeData.TileCountX = 5;
	SourceOfficeData.TileCountY = 6;
	SourceOfficeData.StarterTileCountX = 2;
	SourceOfficeData.StarterTileCountY = 3;
	SourceOfficeData.CurrentFloorTileRowName = ExpectedFloorRowName;
	SourceSave->GameData.OfficeDataMap.Add(SentinelBuildingIndex, SourceOfficeData);

	TArray<uint8> SaveBytes;
	if (!TestTrue(
		TEXT("save game serializes to memory"),
		UGameplayStatics::SaveGameToMemory(SourceSave, SaveBytes)))
	{
		return false;
	}
	if (!TestTrue(TEXT("serialized save bytes are non-empty"), !SaveBytes.IsEmpty()))
	{
		return false;
	}

	USaveGame_GameData* LoadedSave = Cast<USaveGame_GameData>(
		UGameplayStatics::LoadGameFromMemory(SaveBytes));
	if (!TestNotNull(TEXT("memory payload loads as project save game"), LoadedSave))
	{
		return false;
	}

	const FOfficeSaveData* LoadedOfficeData =
		LoadedSave->GameData.OfficeDataMap.Find(SentinelBuildingIndex);
	if (!TestNotNull(TEXT("sentinel office survives serialization"), LoadedOfficeData))
	{
		return false;
	}

	TestEqual(TEXT("tile count X survives serialization"), LoadedOfficeData->TileCountX, 5);
	TestEqual(TEXT("tile count Y survives serialization"), LoadedOfficeData->TileCountY, 6);
	TestEqual(TEXT("starter tile count X survives serialization"), LoadedOfficeData->StarterTileCountX, 2);
	TestEqual(TEXT("starter tile count Y survives serialization"), LoadedOfficeData->StarterTileCountY, 3);
	TestEqual(
		TEXT("floor tile row survives serialization"),
		LoadedOfficeData->CurrentFloorTileRowName,
		ExpectedFloorRowName);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
