#include "Manager/DevPresetSeeder.h"

#include "Engine/DataTable.h"
#include "Core/CGGameInstance.h"
#include "Data/BuildingEnhancementData.h"
#include "Data/GameSaveData.h"
#include "Data/OperationData.h"
#include "Data/ShippedRecord.h"
#include "Entity/Building/BuildingBaseActor.h"
#include "Manager/CityAcquisitionManager.h"
#include "Manager/EmployeeManager.h"
#include "Manager/EntityManager.h"
#include "Manager/ProjectOperationManager.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/SpawnManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Office/StarterPresetSeeder.h"
#include "Table/BuildingData.h"
#include "Table/CityPlotData.h"
#include "Table/DevProgressPresetTable.h"

const FDevProgressPresetRow* UDevPresetSeeder::FindPreset(FName PresetRow) const
{
	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	UDataTable* DT = TableMgr ? TableMgr->GetDevProgressPresetTable() : nullptr;
	if (!DT)
	{
		UE_LOG(LogTemp, Error, TEXT("[PresetSeeder] DT_DevProgressPreset 미로드 ― 에셋이 없거나 경로가 다르다"));
		return nullptr;
	}
	return DT->FindRow<FDevProgressPresetRow>(PresetRow, TEXT("DevPresetSeeder"));
}

void UDevPresetSeeder::SeedStageA(FName PresetRow)
{
#if !UE_BUILD_SHIPPING
	if (bStageASeeded)
	{
		return;   // RestoreEntityDataFromLoad 에 중복 가드가 없어 두 번 돌면 건물이 두 배가 된다
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		// 래치를 여기서 태우면 안 된다 — 실패한 호출이 래치만 소모해 이후 정상 호출이 조용히 무시된다
		UE_LOG(LogTemp, Error, TEXT("[PresetSeeder] StageA 중단 ― World 없음"));
		return;
	}

	bStageASeeded = true;

	// 다음 틱으로 미룬다 ― OnGameDataLoaded 핸들러 호출 순서가 비보장이라, 즉시 시드하면
	// SaveLoadManager 가 나중에 세이브를 메모리에 올리며 프리셋을 덮어쓴다 (ApplyDevSandbox 와 같은 규율).
	World->GetTimerManager().SetTimerForNextTick(
		[WeakThis = TWeakObjectPtr<UDevPresetSeeder>(this), PresetRow]()
	{
		UDevPresetSeeder* Self = WeakThis.Get();
		if (Self)
		{
			Self->RunStageA(PresetRow);
		}
	});
#endif
}

void UDevPresetSeeder::RunStageA(FName PresetRow)
{
#if !UE_BUILD_SHIPPING
	const FDevProgressPresetRow* Preset = FindPreset(PresetRow);
	if (!Preset)
	{
		UE_LOG(LogTemp, Error, TEXT("[PresetSeeder] StageA 중단 ― 프리셋 행 없음: %s"), *PresetRow.ToString());
		return;
	}

	UGameInstance* GI = GetGameInstance();
	UWorld* World = GetWorld();
	USaveLoadManager* SaveMgr = GI ? GI->GetSubsystem<USaveLoadManager>() : nullptr;
	USaveGame_GameData* SD = SaveMgr ? SaveMgr->GetCurrentSaveData() : nullptr;
	if (!SaveMgr || !SD || !World)
	{
		UE_LOG(LogTemp, Error, TEXT("[PresetSeeder] StageA 중단 ― SaveLoadManager/세이브 없음"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[PresetSeeder] ===== StageA 시작 ― %s (Tier=%d) ====="),
		*PresetRow.ToString(), Preset->TargetTier);

	// 수십억 시드가 실제 PlayFab 랭킹을 오염시키는 것을 막는다
	SaveMgr->SetLeaderboardUploadSuppressed(true);

	// R2 (매번 같은 지점) ― 이전 세션 잔재를 비우고 프리셋만 남긴다.
	// 이 덕분에 아래 모든 시드가 "기존 상태 없음"을 가정할 수 있어 멱등성 처리가 불필요해진다.
	FGameSaveData& G = SD->GameData;
	G.Buildings.Empty();
	G.OfficeDataMap.Empty();
	G.OwnedPlotIds.Empty();
	G.CityAcq.Empty();
	G.ShippedProjects.Empty();
	G.DiscoveredCombos.Empty();
	G.TotalProjectsCompleted = 0;
	G.AGradeProjectsCompleted = 0;
	G.SGradeProjectsCompleted = 0;
	G.TotalRevenueEarned = 0;
	G.CompanyTitle = ECompanyTitle::Small;

	SeedPlotsAndCompanies(*Preset);
	SeedBuildings(PresetRow, *Preset);
	SeedEmployees(*Preset);
	SeedProjectHistory(*Preset);
	SeedOperations(*Preset);
	SeedWorldMap(*Preset);

	// 반드시 마지막 ― CanPromoteTitle 이 읽는 시총은 런타임 ResourceItemManager 값이고,
	// 승격 루프의 유일한 트리거가 MarketCap 변경 이벤트다.
	SeedResourcesAndPromote(*Preset);

	SeedOfflineGains(*Preset);

	SaveMgr->SetLeaderboardUploadSuppressed(false);
	SaveMgr->SaveGameData();

	UE_LOG(LogTemp, Warning, TEXT("[PresetSeeder] ===== StageA 완료 ====="));
	VerifyPreset(PresetRow);
#endif
}

void UDevPresetSeeder::SeedStageB()
{
	UE_LOG(LogTemp, Warning, TEXT("[PresetSeeder] SeedStageB 스텁"));
}

void UDevPresetSeeder::ReapplyPreset(FName PresetRow)
{
#if !UE_BUILD_SHIPPING
	// 래치를 풀고 StageA 를 다시 태운다. 재진입이 안전한 이유:
	// SeedBuildings 가 기존 빌딩 액터를 먼저 파괴하고, SetEmployeesForBuilding 은 해당 건물 직원을
	// 먼저 제거하는 replace 시맨틱이며, SetActiveOperations 는 배열을 통째 교체한다.
	bStageASeeded = false;
	SeedStageA(PresetRow);
#endif
}

int32 UDevPresetSeeder::VerifyPreset(FName PresetRow)
{
#if !UE_BUILD_SHIPPING
	const FDevProgressPresetRow* Preset = FindPreset(PresetRow);
	if (!Preset)
	{
		UE_LOG(LogTemp, Error, TEXT("[PresetVerify] 프리셋 행 없음: %s"), *PresetRow.ToString());
		return -1;
	}

	UGameInstance* GI = GetGameInstance();
	USaveLoadManager* SaveMgr = GI ? GI->GetSubsystem<USaveLoadManager>() : nullptr;
	USaveGame_GameData* SD = SaveMgr ? SaveMgr->GetCurrentSaveData() : nullptr;
	if (!SD)
	{
		UE_LOG(LogTemp, Error, TEXT("[PresetVerify] 세이브 데이터 없음 (최초 LoadGameData 전인지 확인)"));
		return -1;
	}

	int32 Failures = 0;
	auto Check = [&Failures](bool bOK, const TCHAR* Label, const FString& Detail)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PresetVerify] %s  %-24s  %s"),
			bOK ? TEXT("PASS") : TEXT("FAIL"), Label, *Detail);
		if (!bOK)
		{
			++Failures;
		}
	};

	const FGameSaveData& G = SD->GameData;

	UE_LOG(LogTemp, Warning, TEXT("[PresetVerify] ===== %s (TargetTier=%d) ====="),
		*PresetRow.ToString(), Preset->TargetTier);

	Check(G.Buildings.Num() > 0, TEXT("빌딩 존재(세이브)"),
		FString::Printf(TEXT("%d채"), G.Buildings.Num()));

	// 세이브 배열은 내가 직접 쓴 값이라 "액터가 실제로 월드에 섰는지"의 증거가 못 된다.
	// InteractableName 오타 사고(Building1 vs B1)가 세이브만 보면 안 잡히는 게 그 예 — 액터를 따로 센다.
	if (UWorld* W = GetWorld())
	{
		if (UEntityManager* EntityMgr = W->GetSubsystem<UEntityManager>())
		{
			const TArray<ABuildingBaseActor*>& Live = EntityMgr->GetBuildings();
			int32 Alive = 0;
			for (ABuildingBaseActor* B : Live)
			{
				if (!IsValid(B))
				{
					continue;
				}
				++Alive;
				const FVector L = B->GetActorLocation();
				const int32 Idx = B->GetBuildingIndex();
				const FBuildingEntitySaveData* Saved = G.Buildings.FindByPredicate(
					[Idx](const FBuildingEntitySaveData& E) { return E.BuildingIndex == Idx; });
				UE_LOG(LogTemp, Warning,
					TEXT("[PresetVerify]   액터 %s idx=%d 층(세이브)=%d loc=(%.0f, %.0f, %.0f)"),
					*B->GetName(), Idx,
					Saved ? Saved->BuildingData.Body_Module_Copies : -1, L.X, L.Y, L.Z);
			}
			Check(Alive == G.Buildings.Num(), TEXT("빌딩 액터(월드)"),
				FString::Printf(TEXT("%d채 스폰 / 세이브 %d채"), Alive, G.Buildings.Num()));
		}
	}


	Check(G.OwnedPlotIds.Num() >= Preset->OwnedPlotCount, TEXT("부지 소유"),
		FString::Printf(TEXT("%d/%d"), G.OwnedPlotIds.Num(), Preset->OwnedPlotCount));

	// 부지만 시드하고 그 위 회사를 Cleared 로 안 만들면 내 땅에 남의 건물이 서 있는 모순이 된다.
	int32 ClearedCount = 0;
	for (const TPair<int32, FCityAcqSave>& Pair : G.CityAcq)
	{
		if (Pair.Value.State == static_cast<uint8>(3))   // EAcqState::Cleared
		{
			++ClearedCount;
		}
	}
	Check(ClearedCount >= Preset->ClearedCompanyCount, TEXT("회사 인수(Cleared)"),
		FString::Printf(TEXT("%d/%d"), ClearedCount, Preset->ClearedCompanyCount));

	Check(G.ShippedProjects.Num() > 0 && G.TotalProjectsCompleted >= G.ShippedProjects.Num(),
		TEXT("이력↔누적건수"),
		FString::Printf(TEXT("Shipped=%d Total=%d"), G.ShippedProjects.Num(), G.TotalProjectsCompleted));

	// GrantTestResources 로만 시드하면 이 값이 0 으로 남아 랭킹/프로필에서 티가 난다.
	Check(G.TotalRevenueEarned > 0, TEXT("누적매출"),
		FString::Printf(TEXT("%lld"), G.TotalRevenueEarned));

	Check(G.DiscoveredCombos.Num() > 0, TEXT("도감 발견조합"),
		FString::Printf(TEXT("%d개"), G.DiscoveredCombos.Num()));

	// CanPromoteTitle 은 OfficeDataMap[].EmployeeList.Num() 만 센다 ― 착석 여부와 무관.
	int32 TotalEmployees = 0;
	int32 OperatingCount = 0;
	int32 StoredCount = 0;
	for (const TPair<int32, FOfficeSaveData>& Pair : G.OfficeDataMap)
	{
		TotalEmployees += Pair.Value.EmployeeList.Num();
		if (Pair.Value.bHasActiveOperation)
		{
			++OperatingCount;
		}
		if (Pair.Value.StoredRevenue > 0.0f)
		{
			++StoredCount;
		}
	}

	// 총합이 "건물당 N" 이상인지 보는 건 건물이 여러 채면 항상 참이라 사실상 검사가 아니었다.
	// 진짜 불변식은 "어느 건물도 자기 인원 상한을 넘지 않는다" — 상한이 칸수에 연동된 뒤로 깨지기 쉬워졌다.
	UEmployeeManager* VerifyEmpMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UEmployeeManager>() : nullptr;
	int32 OverCapCount = 0;
	for (const TPair<int32, FOfficeSaveData>& Pair : G.OfficeDataMap)
	{
		const int32 Cap = VerifyEmpMgr ? VerifyEmpMgr->GetBuildingEmployeeCapacity(Pair.Key, /*bLogIfZero*/ false) : 0;
		if (Pair.Value.EmployeeList.Num() > Cap)
		{
			++OverCapCount;
			UE_LOG(LogTemp, Warning, TEXT("[PresetVerify]   빌딩%d 로스터 %d명 > 인원 상한 %d명"),
				Pair.Key, Pair.Value.EmployeeList.Num(), Cap);
		}
	}
	Check(TotalEmployees > 0 && OverCapCount == 0, TEXT("직원 로스터"),
		FString::Printf(TEXT("%d명 / 상한초과 %d채"), TotalEmployees, OverCapCount));

	Check(OperatingCount >= Preset->OperatingProjectCount, TEXT("운영중 프로젝트"),
		FString::Printf(TEXT("%d/%d"), OperatingCount, Preset->OperatingProjectCount));

	Check(Preset->OfflineHours <= 0.0f || StoredCount > 0, TEXT("금고 적립"),
		FString::Printf(TEXT("%d채"), StoredCount));

	Check(G.HQLevel >= Preset->HQLevel, TEXT("HQ 레벨"),
		FString::Printf(TEXT("%d/%d"), G.HQLevel, Preset->HQLevel));

	// 등급은 직접 대입하지 않고 시총을 마지막에 넣어 자연 승격시킨다.
	// 그래서 승격을 기대하는 프리셋에서 FAIL 이면 = 프리셋 값 자체가 부정합(빌딩/업종/이력 부족)이라는 신호다.
	const FName ResRow = Preset->ResourceScenarioRow;
	const bool bExpectPromotion = !ResRow.IsNone()
		&& ResRow != FName(TEXT("Default")) && ResRow != FName(TEXT("Poor")) && ResRow != FName(TEXT("Empty"));
	Check(!bExpectPromotion || G.CompanyTitle != ECompanyTitle::Small, TEXT("회사 등급 승격"),
		FString::Printf(TEXT("%s (기대=%s)"), *CompanyTitleToString(G.CompanyTitle),
			bExpectPromotion ? TEXT("승격") : TEXT("중소기업 유지")));

	UE_LOG(LogTemp, Warning, TEXT("[PresetVerify] ===== %s : 실패 %d건 ====="),
		*PresetRow.ToString(), Failures);
	return Failures;
#else
	return 0;
#endif
}

// ===== 시드 단계 =====

void UDevPresetSeeder::SeedPlotsAndCompanies(const FDevProgressPresetRow& Preset)
{
#if !UE_BUILD_SHIPPING
	UGameInstance* GI = GetGameInstance();
	UWorld* World = GetWorld();
	USaveLoadManager* SaveMgr = GI ? GI->GetSubsystem<USaveLoadManager>() : nullptr;
	USaveGame_GameData* SD = SaveMgr ? SaveMgr->GetCurrentSaveData() : nullptr;
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!SD || !TableMgr || !World)
	{
		return;
	}

	// USpawnManager 는 MainMap 에서만 생성된다 — 다른 맵에서 시작하면 부지/빌딩 축이 통째로 빠진다.
	USpawnManager* SpawnMgr = World->GetSubsystem<USpawnManager>();
	if (!SpawnMgr)
	{
		UE_LOG(LogTemp, Error, TEXT("[PresetSeeder] SpawnManager 없음 — MainMap 이 아니면 부지/빌딩 시드 불가"));
		return;
	}

	TArray<FName> AllPlots;
	TableMgr->GetAllCityPlotRows(AllPlots);
	if (AllPlots.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("[PresetSeeder] DT_CityPlot 행이 0개 — 리임포트 상태 확인"));
		return;
	}

	// 안쪽(싼) 부지부터 소유 — 실제 플레이의 확장 순서와 같아야 '말이 되는' 배치가 된다.
	AllPlots.Sort([TableMgr](const FName& A, const FName& B)
	{
		bool bOkA = false, bOkB = false;
		const FCityPlotData DA = TableMgr->GetCityPlotData(A, bOkA);
		const FCityPlotData DB = TableMgr->GetCityPlotData(B, bOkB);
		return DA.MoneyPrice < DB.MoneyPrice;
	});

	TArray<FName> Owned;
	TSet<int32> CompaniesToClear;

	const int32 PlotTarget = FMath::Clamp(Preset.OwnedPlotCount, 0, AllPlots.Num());
	for (int32 i = 0; i < PlotTarget; ++i)
	{
		bool bOk = false;
		const FCityPlotData Data = TableMgr->GetCityPlotData(AllPlots[i], bOk);
		if (!bOk)
		{
			continue;
		}
		Owned.Add(AllPlots[i]);
		// 부지와 회사는 반드시 짝 — 인수 안 된 부지 위엔 남의 건물이 그대로 서 있다.
		// 병합 부지는 여러 블록에 걸쳐 회사도 여럿이라 전원을 정리 대상에 넣는다.
		for (const int32 OccKey : Data.OccupantCompanyKeys)
		{
			if (OccKey != 0)
			{
				CompaniesToClear.Add(OccKey);
			}
		}
	}

	// 소유 부지 밖에서도 프리셋 목표치까지 회사를 더 인수해 둔다(인수는 부지와 별개 축).
	for (int32 i = PlotTarget; i < AllPlots.Num() && CompaniesToClear.Num() < Preset.ClearedCompanyCount; ++i)
	{
		bool bOk = false;
		const FCityPlotData Data = TableMgr->GetCityPlotData(AllPlots[i], bOk);
		if (!bOk)
		{
			continue;
		}
		for (const int32 OccKey : Data.OccupantCompanyKeys)
		{
			if (OccKey != 0)
			{
				CompaniesToClear.Add(OccKey);
			}
		}
	}

	SpawnMgr->RestorePlotOwnership(Owned);
	SD->GameData.OwnedPlotIds = Owned;

	for (int32 Key : CompaniesToClear)
	{
		FCityAcqSave Entry;
		Entry.State = static_cast<uint8>(EAcqState::Cleared);
		Entry.RTotal = 0;
		Entry.RRemaining = 0;
		SD->GameData.CityAcq.Add(Key, Entry);
	}

	// LoadFromGame 은 OnCompanyCleared 를 안 쏜다. 항공장애등/클릭프록시가 BeginPlay+0.5s 에
	// GetState()==Cleared 를 읽으므로 그 창 안에 시드되면 자동 정합이다.
	if (UCityAcquisitionManager* AcqMgr = World->GetSubsystem<UCityAcquisitionManager>())
	{
		AcqMgr->LoadFromGame();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[PresetSeeder] CityAcquisitionManager 없음 — 인수 상태는 세이브에만 기록"));
	}

	UE_LOG(LogTemp, Warning, TEXT("[PresetSeeder]   부지 %d개 소유 / 회사 %d개 Cleared"),
		Owned.Num(), CompaniesToClear.Num());
#endif
}

void UDevPresetSeeder::SeedBuildings(FName PresetRow, const FDevProgressPresetRow& Preset)
{
#if !UE_BUILD_SHIPPING
	UGameInstance* GI = GetGameInstance();
	UWorld* World = GetWorld();
	USaveLoadManager* SaveMgr = GI ? GI->GetSubsystem<USaveLoadManager>() : nullptr;
	USaveGame_GameData* SD = SaveMgr ? SaveMgr->GetCurrentSaveData() : nullptr;
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	UDataTable* BuildingDT = TableMgr ? TableMgr->GetDevPresetBuildingTable() : nullptr;
	if (!SD || !World || !BuildingDT)
	{
		UE_LOG(LogTemp, Error, TEXT("[PresetSeeder] 빌딩 시드 불가 — DT_DevPresetBuilding 미로드"));
		return;
	}

	USpawnManager* SpawnMgr = World->GetSubsystem<USpawnManager>();
	UEntityManager* EntityMgr = World->GetSubsystem<UEntityManager>();
	if (!SpawnMgr || !EntityMgr)
	{
		UE_LOG(LogTemp, Error, TEXT("[PresetSeeder] 빌딩 시드 불가 — SpawnManager/EntityManager 없음 (MainMap 아님?)"));
		return;
	}

	// 세이브 배열을 비우는 것만으론 부족하다 — SaveGameData 가 라이브 EntityManager 를 진실로 삼아
	// 재수집하므로, 이미 스폰된 액터를 먼저 치워야 프리셋 빌딩만 남는다 (R2: 매번 같은 지점).
	{
		TArray<ABuildingBaseActor*> Existing = EntityMgr->GetBuildings();
		for (ABuildingBaseActor* Old : Existing)
		{
			if (Old)
			{
				EntityMgr->RemoveBuilding(Old);
				Old->Destroy();
			}
		}
		if (Existing.Num() > 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("[PresetSeeder]   기존 빌딩 액터 %d채 제거"), Existing.Num());
		}
	}

	TArray<FBuildingEntitySaveData> Seeded;
	int32 NextIndex = 1;

	// 좌표를 전부 계산한 뒤 일괄 스폰하므로, 루프 중에는 GetBuildingsOnPlot 이 비어 있다.
	// 같은 부지에 두 채 이상이면 예정 위치를 넘겨야 서로 안 겹친다.
	TMap<FName, TArray<FVector>> PlannedByPlot;

	for (const FName& RowName : BuildingDT->GetRowNames())
	{
		const FDevPresetBuildingRow* Row = BuildingDT->FindRow<FDevPresetBuildingRow>(RowName, TEXT("PresetSeeder"));
		if (!Row || Row->PresetID != PresetRow)
		{
			continue;
		}

		TArray<FVector>& PlannedOnThisPlot = PlannedByPlot.FindOrAdd(Row->PlotId);

		// footprint 는 DT_Building 의 칸수 × FootprintCellSize(4300) 가 단일 진실 소스다.
		// 이 값이 탐색 스텝이자 겹침 임계값 둘 다로 쓰이므로, 작게 넘기면 건물이 서로 파고들고
		// 부지의 나머지 칸이 통째로 안 쓰인다(예: 700 을 넘기면 4300 폭 건물이 84% 겹친다).
		bool bBuildingDataOk = false;
		const FBuildingData BD = TableMgr->GetBuildingData(Row->InteractableName, bBuildingDataOk);
		if (!bBuildingDataOk)
		{
			UE_LOG(LogTemp, Warning, TEXT("[PresetSeeder] DT_Building 행 없음: %s — 1x1 로 가정"),
				*Row->InteractableName.ToString());
		}
		const FVector2D Footprint(
			FMath::Max(1, bBuildingDataOk ? BD.FootprintWidthCells : 1) * FootprintCellSize,
			FMath::Max(1, bBuildingDataOk ? BD.FootprintDepthCells : 1) * FootprintCellSize);

		FTransform Placement;
		if (!SpawnMgr->FindFreeFootprintOnPlot(Row->PlotId, Footprint, Placement, PlannedOnThisPlot))
		{
			UE_LOG(LogTemp, Warning, TEXT("[PresetSeeder] 빌딩 배치 실패 row=%s plot=%s"),
				*RowName.ToString(), *Row->PlotId.ToString());
			continue;
		}

		FBuildingEntitySaveData Entry;
		Entry.BuildingIndex = NextIndex++;
		Entry.InteractableName = Row->InteractableName;
		Entry.Location = Placement.GetLocation();
		Entry.Rotation = Placement.Rotator();
		PlannedOnThisPlot.Add(Entry.Location);
		Entry.Scale = FVector::OneVector;   // 오토핏은 '땅에 박힘' 회귀로 제거됨 — Scale 손대지 말 것
		Entry.PlotId = Row->PlotId;
		Entry.BuildingData.Body_Module_Copies = Row->Floors;
		Entry.BuildingData.CompanyType = Row->CompanyType;
		Entry.BuildingData.AppliedSkinID = Row->SkinID;
		Entry.BuildingData.AppliedLightID = Row->LightID;

		// 강화 레벨 직접 세터가 없어 InitializeFromSaveData 경유가 유일 경로다
		if (!Row->Enhancements.IsEmpty())
		{
			TArray<FString> Pairs;
			Row->Enhancements.ParseIntoArray(Pairs, TEXT(";"), true);
			const UEnum* EnumPtr = StaticEnum<EBuildingEnhancementType>();
			for (const FString& Pair : Pairs)
			{
				FString Key, ValueStr;
				if (!Pair.Split(TEXT(":"), &Key, &ValueStr))
				{
					continue;
				}
				const int64 Found = EnumPtr ? EnumPtr->GetValueByNameString(Key.TrimStartAndEnd()) : INDEX_NONE;
				if (Found == INDEX_NONE)
				{
					UE_LOG(LogTemp, Warning, TEXT("[PresetSeeder] 알 수 없는 강화 타입: %s"), *Key);
					continue;
				}
				Entry.BuildingData.EnhancementLevels.Add(
					static_cast<EBuildingEnhancementType>(Found), FCString::Atoi(*ValueStr));
			}
		}

		Seeded.Add(Entry);
	}

	SD->GameData.Buildings = Seeded;
	EntityMgr->SetBuildingsData(Seeded);
	EntityMgr->RestoreEntityDataFromLoad();

	// 정상 건설 경로가 확정 시점에 부르는 예약을 시더도 똑같이 태운다.
	// 이걸 빼면 첫 오피스 입장 시 ApplyStarterPresetToData 가 배치할 게 없어 책상/데코가 통째로 빈다.
	{
		int32 Reserved = 0;
		for (ABuildingBaseActor* B : EntityMgr->GetBuildings())
		{
			if (IsValid(B))
			{
				FStarterPresetSeeder::SeedPendingForBuilding(B);
				++Reserved;
			}
		}
		UE_LOG(LogTemp, Warning, TEXT("[PresetSeeder]   스타터 프리셋 예약 %d채"), Reserved);
	}

	// AddBuilding 은 인덱스 충돌 시 Error 로그만 남기고 진행한다 — 발급기를 반드시 밀어둔다
	if (UCGGameInstance* CGI = UCGGameInstance::GetInstance())
	{
		CGI->SetNextBuildingIndex(NextIndex);
	}
	SD->GameData.NextBuildingIndex = NextIndex;

	UE_LOG(LogTemp, Warning, TEXT("[PresetSeeder]   빌딩 %d채 스폰"), Seeded.Num());
#endif
}

void UDevPresetSeeder::SeedEmployees(const FDevProgressPresetRow& Preset)
{
#if !UE_BUILD_SHIPPING
	UGameInstance* GI = GetGameInstance();
	USaveLoadManager* SaveMgr = GI ? GI->GetSubsystem<USaveLoadManager>() : nullptr;
	USaveGame_GameData* SD = SaveMgr ? SaveMgr->GetCurrentSaveData() : nullptr;
	UEmployeeManager* EmpMgr = GI ? GI->GetSubsystem<UEmployeeManager>() : nullptr;
	if (!SD || !EmpMgr)
	{
		return;
	}

	int32 Total = 0;
	for (const FBuildingEntitySaveData& B : SD->GameData.Buildings)
	{
		EmpMgr->SeedEmployeesForBuilding(B.BuildingIndex, Preset.EmployeesPerBuilding,
			Preset.EmployeeLevelMin, Preset.EmployeeLevelMax,
			Preset.EmployeeEnhanceMin, Preset.EmployeeEnhanceMax, Preset.RandomSeed);

		// SaveGameData 는 MainMap 에서 EmployeeList 를 수집하지 않는다 — 세이브에 직접 기록해야 남는다
		FOfficeSaveData& Office = SD->GameData.OfficeDataMap.FindOrAdd(B.BuildingIndex);
		Office.EmployeeList = EmpMgr->GetEmployeesByBuilding(B.BuildingIndex);
		Total += Office.EmployeeList.Num();
	}

	UE_LOG(LogTemp, Warning, TEXT("[PresetSeeder]   직원 %d명"), Total);
#endif
}

void UDevPresetSeeder::SeedProjectHistory(const FDevProgressPresetRow& Preset)
{
#if !UE_BUILD_SHIPPING
	UGameInstance* GI = GetGameInstance();
	USaveLoadManager* SaveMgr = GI ? GI->GetSubsystem<USaveLoadManager>() : nullptr;
	USaveGame_GameData* SD = SaveMgr ? SaveMgr->GetCurrentSaveData() : nullptr;
	if (!SD)
	{
		return;
	}

	FRandomStream Rng(Preset.RandomSeed);
	FGameSaveData& G = SD->GameData;

	// 티어 해금이 7/10 클리어라 현재 티어는 미완이어야 자연스럽다
	const int32 HistoryCount = FMath::Max(0, (Preset.TargetTier - 1) * 10 + Rng.RandRange(0, 6));

	int64 Cumulative = 0;
	int32 AGrade = 0;
	int32 SGrade = 0;

	for (int32 i = 0; i < HistoryCount; ++i)
	{
		const float Progress = (HistoryCount > 1)
			? static_cast<float>(i) / static_cast<float>(HistoryCount - 1)
			: 1.0f;

		// 상승 곡선 — 데뷔작 14점대에서 최근작 32점대로. 전부 만점이면 가짜 티가 난다.
		int32 Review = FMath::RoundToInt(FMath::Lerp(14.0f, 32.0f, Progress)) + Rng.RandRange(-4, 4);

		// 실패작 1~2건 — 성장 서사에 굴곡을 준다
		if (HistoryCount >= 6 && (i == HistoryCount / 3 || i == (HistoryCount * 2) / 3))
		{
			Review = Rng.RandRange(6, 12);
		}
		Review = FMath::Clamp(Review, 1, 34);

		// 산업/장르/소재를 돌려가며 넣는다 — 전부 같으면 도감 조합이 1개로 뭉개진다(AddUnique).
		static const ECompanyType Industries[] = {
			ECompanyType::Game, ECompanyType::IT, ECompanyType::Electronics,
			ECompanyType::Finance, ECompanyType::Automobile, ECompanyType::Semiconductor };

		// 등급은 평점과 같은 축에서 파생 — 이력과 등급이 따로 놀면 도감이 가짜로 보인다
		const EQualityGrade SeedGrade = (Review >= 30) ? EQualityGrade::S
			: (Review >= 25) ? EQualityGrade::A
			: (Review >= 18) ? EQualityGrade::B : EQualityGrade::C;

		FShippedProjectRecord Rec;
		Rec.ProjectIndex = i + 1;
		Rec.ReviewScore = Review;
		Rec.QualityGrade = SeedGrade;
		Rec.Industry = Industries[i % UE_ARRAY_COUNT(Industries)];
		Rec.Genre = FName(*FString::Printf(TEXT("Genre_%d"), (i % 7) + 1));
		Rec.Material = FName(*FString::Printf(TEXT("Mat_%d"), (i % 5) + 1));
		Rec.ProjectName = FString::Printf(TEXT("Project %02d"), i + 1);
		// 매출을 평점에 비례시킨다 — 이력과 누적매출이 따로 놀면 티가 난다
		Rec.CumulativeRevenue = static_cast<int64>(Review) * 12000000LL + Rng.RandRange(0, 4000000);

		// (ProjectIndex, Industry) upsert — blind Add 는 같은 인덱스 2행을 만든다
		FShippedProjectRecord* Existing = G.ShippedProjects.FindByPredicate(
			[&Rec](const FShippedProjectRecord& R)
			{ return R.ProjectIndex == Rec.ProjectIndex && R.Industry == Rec.Industry; });
		if (Existing)
		{
			*Existing = Rec;
		}
		else
		{
			G.ShippedProjects.Add(Rec);
		}

		Cumulative += Rec.CumulativeRevenue;
		if (SeedGrade == EQualityGrade::S) { ++SGrade; }
		else if (SeedGrade == EQualityGrade::A) { ++AGrade; }

		// 도감 — 안 채우면 진척률이 0 으로 남아 중반처럼 안 보인다
		G.DiscoveredCombos.AddUnique(FString::Printf(TEXT("%d|%s|%s"),
			static_cast<int32>(Rec.Industry), *Rec.Genre.ToString(), *Rec.Material.ToString()));
	}

	G.TotalProjectsCompleted = G.ShippedProjects.Num();
	G.AGradeProjectsCompleted = AGrade;
	G.SGradeProjectsCompleted = SGrade;
	G.TotalRevenueEarned = Cumulative;

	// 티어는 빌딩별 OfficeDataMap[].TierProgress 에 산다 (OfficeStageProgressManager 는 OfficeMap 전용)
	for (const FBuildingEntitySaveData& B : G.Buildings)
	{
		FOfficeSaveData& Office = G.OfficeDataMap.FindOrAdd(B.BuildingIndex);
		Office.TierProgress.CurrentTier = Preset.TargetTier;
		Office.TierProgress.ClearedProjects.Empty();
		for (const FShippedProjectRecord& R : G.ShippedProjects)
		{
			Office.TierProgress.ClearedProjects.AddUnique(R.ProjectIndex);
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[PresetSeeder]   이력 %d건 (A=%d S=%d) 누적매출 %lld"),
		G.ShippedProjects.Num(), AGrade, SGrade, Cumulative);
#endif
}

void UDevPresetSeeder::SeedOperations(const FDevProgressPresetRow& Preset)
{
#if !UE_BUILD_SHIPPING
	UGameInstance* GI = GetGameInstance();
	USaveLoadManager* SaveMgr = GI ? GI->GetSubsystem<USaveLoadManager>() : nullptr;
	USaveGame_GameData* SD = SaveMgr ? SaveMgr->GetCurrentSaveData() : nullptr;
	UProjectOperationManager* OpMgr = GI ? GI->GetSubsystem<UProjectOperationManager>() : nullptr;
	if (!SD || !OpMgr)
	{
		return;
	}

	FRandomStream Rng(Preset.RandomSeed + 7);
	TArray<FOperationData> Ops;
	int32 Made = 0;

	for (const FBuildingEntitySaveData& B : SD->GameData.Buildings)
	{
		if (Made >= Preset.OperatingProjectCount)
		{
			break;
		}

		// 운영(Operation) 페이즈는 프로젝트형(게임/금융/IT) 전용이다.
		// 제조업(반도체/자동차/전자)은 공장 생산 라인 경로라 운영 중이 될 수 없고,
		// 오프라인 정산도 운영 중 빌딩만 순회하므로 여기 섞으면 실제 플레이로 불가능한 상태가 된다.
		if (!IsProjectType(B.BuildingData.CompanyType))
		{
			continue;
		}

		FOperationData Op;
		Op.BuildingID = B.BuildingIndex;
		Op.ProjectID = 100 + Made;
		Op.ProjectNumber = Made + 1;
		Op.ProjectName = FString::Printf(TEXT("Live %d"), Made + 1);
		Op.TotalOperationTime = 3600.0f;
		// 완료 직전으로 시드하면 몇 초 만에 완료 연쇄(결산/시총/미션)가 터진다 — 중간쯤에 둔다
		Op.ElapsedTime = Op.TotalOperationTime * Rng.FRandRange(0.25f, 0.55f);
		Op.RemainingTime = Op.TotalOperationTime - Op.ElapsedTime;
		Op.BaseRevenuePerSecond = 25000.0f;
		Op.ActualRevenuePerSecond = 25000.0f;
		Op.State = EOperationState::Operating;

		Ops.Add(Op);

		FOfficeSaveData& Office = SD->GameData.OfficeDataMap.FindOrAdd(B.BuildingIndex);
		Office.bHasActiveOperation = true;
		Office.CurrentOperation = Op;
		++Made;
	}

	// SetActiveOperations 는 아무 델리게이트도 쏘지 않는다 — UI 는 다음 1초 틱에서 따라온다
	OpMgr->SetActiveOperations(Ops);

	// 프로젝트형 빌딩이 모자라 목표치를 못 채웠으면 조용히 넘어가지 말 것 (프리셋 값이 부정합하다는 신호)
	if (Ops.Num() < Preset.OperatingProjectCount)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[PresetSeeder]   운영중 %d/%d 건만 시드 — 프로젝트형(게임/금융/IT) 빌딩이 부족하다. "
				"DT_DevPresetBuilding 의 CompanyType 또는 OperatingProjectCount 를 맞출 것"),
			Ops.Num(), Preset.OperatingProjectCount);
	}

	UE_LOG(LogTemp, Warning, TEXT("[PresetSeeder]   운영중 프로젝트 %d건 (프로젝트형만)"), Ops.Num());
#endif
}

void UDevPresetSeeder::SeedWorldMap(const FDevProgressPresetRow& Preset)
{
#if !UE_BUILD_SHIPPING
	// 국가 해금은 저장되지 않는 파생값(MarketCap vs RequiredMarketCap)이라 별도 시드 축이 없다.
	// 공장/채광 라인 생성은 자원을 실제 차감하므로 자원 시드 뒤에 도는 별도 패스가 필요하다 — 후속 작업.
	UE_LOG(LogTemp, Warning, TEXT("[PresetSeeder]   월드맵 라인 시드 미구현 (자원 선행 필요 — 후속)"));
#endif
}

void UDevPresetSeeder::SeedResourcesAndPromote(const FDevProgressPresetRow& Preset)
{
#if !UE_BUILD_SHIPPING
	UGameInstance* GI = GetGameInstance();
	USaveLoadManager* SaveMgr = GI ? GI->GetSubsystem<USaveLoadManager>() : nullptr;
	USaveGame_GameData* SD = SaveMgr ? SaveMgr->GetCurrentSaveData() : nullptr;
	if (!SD || !SaveMgr)
	{
		return;
	}

	// R2(매번 같은 지점) — max 가 아니라 대입이어야 이전 세션의 높은 값이 남지 않는다
	SD->GameData.HQLevel = Preset.HQLevel;
	SaveMgr->OnHQLevelUp.Broadcast(Preset.HQLevel);

	if (!Preset.ResourceScenarioRow.IsNone())
	{
		if (UResourceItemManager* Res = GI->GetSubsystem<UResourceItemManager>())
		{
			// MarketCap 변경 이벤트가 while(TryPromoteTitle()) 연쇄 승격을 발화한다
			Res->GrantTestResources(Preset.ResourceScenarioRow);
		}
	}

	// 시나리오에 MarketCap 이 없어 이벤트가 안 떴을 경우 대비
	while (SaveMgr->TryPromoteTitle()) {}

	UE_LOG(LogTemp, Warning, TEXT("[PresetSeeder]   자원=%s HQ=%d 등급=%s"),
		*Preset.ResourceScenarioRow.ToString(), SD->GameData.HQLevel,
		*CompanyTitleToString(SD->GameData.CompanyTitle));
#endif
}

void UDevPresetSeeder::SeedOfflineGains(const FDevProgressPresetRow& Preset)
{
#if !UE_BUILD_SHIPPING
	if (Preset.OfflineHours <= 0.0f)
	{
		return;
	}

	UGameInstance* GI = GetGameInstance();
	USaveLoadManager* SaveMgr = GI ? GI->GetSubsystem<USaveLoadManager>() : nullptr;
	if (!SaveMgr)
	{
		return;
	}

	// 운영중 프로젝트가 있는 빌딩만 순회하므로 SeedOperations 뒤여야 한다. 60초 미만은 무시된다.
	SaveMgr->CalculateOfflineGains(Preset.OfflineHours * 3600.0f);

	UE_LOG(LogTemp, Warning, TEXT("[PresetSeeder]   오프라인 %0.1f시간 정산"), Preset.OfflineHours);
#endif
}
