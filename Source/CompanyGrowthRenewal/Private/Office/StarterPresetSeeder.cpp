#include "Office/StarterPresetSeeder.h"
#include "Entity/Building/BuildingBaseActor.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/SaveLoadManager.h"   // USaveGame_GameData는 Data/GameSaveData.h 선언 — SaveLoadManager.h가 이미 include함
#include "Table/BuildingData.h"
#include "Table/OfficeStarterPresetTable.h"

void FStarterPresetSeeder::SeedPendingForBuilding(ABuildingBaseActor* Building)
{
	if (!Building || Building->GetBuildingIndex() == INDEX_NONE)
	{
		return;
	}

	UWorld* World = Building->GetWorld();
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	USaveLoadManager* SaveMgr = GI ? GI->GetSubsystem<USaveLoadManager>() : nullptr;
	USaveGame_GameData* SaveData = SaveMgr ? SaveMgr->GetCurrentSaveData() : nullptr;
	if (!TableMgr || !SaveData)
	{
		UE_LOG(LogTemp, Warning, TEXT("[StarterPresetSeeder] TableMgr/SaveData 없음 - 시드 스킵"));
		return;
	}

	// 빌딩 footprint 칸수 조회 (BuildingDataTable RowName == BuildingID)
	int32 Cells = 1;
	bool bBData = false;
	const FBuildingData BData = TableMgr->GetBuildingData(Building->GetBuildingID(), bBData);
	if (bBData)
	{
		Cells = FMath::Max(1, BData.FootprintWidthCells * BData.FootprintDepthCells);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[StarterPresetSeeder] BuildingData 없음 (%s) - 1칸으로 진행"),
			*Building->GetBuildingID().ToString());
	}

	bool bFound = false;
	const FOfficeStarterPresetRow Preset = TableMgr->GetStarterPresetForBuilding(Cells, Building->GetBuildingIndex(), bFound);
	if (!bFound)
	{
		return;   // loud failure는 getter가 이미 로그
	}

	const int32 BuildingIndex = Building->GetBuildingIndex();
	FOfficeSaveData& Office = SaveData->GameData.OfficeDataMap.FindOrAdd(BuildingIndex);

	if (!Office.PendingStarterPreset.IsNone() || Office.PlacedDecorations.Num() > 0)
	{
		return;
	}

	Office.PendingStarterPreset = Preset.RowName;
	Office.TileCountX = Preset.TileCountX;
	Office.TileCountY = Preset.TileCountY;
	Office.StarterTileCountX = Preset.TileCountX;
	Office.StarterTileCountY = Preset.TileCountY;

	UE_LOG(LogTemp, Log, TEXT("[StarterPresetSeeder] Building %d <- %s (cells %d, tiles %dx%d)"),
		BuildingIndex, *Preset.RowName.ToString(), Cells, Preset.TileCountX, Preset.TileCountY);
}
