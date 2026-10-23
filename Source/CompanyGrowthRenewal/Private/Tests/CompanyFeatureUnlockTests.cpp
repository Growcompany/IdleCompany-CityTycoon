#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Data/GameSaveData.h"
#include "Data/SideContentRules.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/SideContentSubsystem.h"
#include "Table/SideDungeonLootTable.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCompanyLootEligibilityTest, "CGR.SideContent.CompanyFeatureUnlock.LootEligibility",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCompanyLootEligibilityTest::RunTest(const FString& Parameters)
{
	FSideDungeonLootRow Basic;
	Basic.TableKey = TEXT("Trip");
	Basic.ItemType = EItemType::BusinessCardPaper;
	Basic.ItemAmountMin = Basic.ItemAmountMax = 1;
	Basic.RequiredCompanyLevel = 3;
	Basic.RequiredHQLevel = 0;
	Basic.Weight = 1.0f;
	FSideDungeonLootRow Resource = Basic;
	Resource.ItemType = EItemType::None;
	Resource.ResourceType = EResourceType::Copper;
	Resource.ResourceAmountMin = Resource.ResourceAmountMax = 1;
	Resource.RequiredHQLevel = 5;
	TArray<FSideDungeonLootRow> Rows{Basic, Resource};
	FRandomStream Random(17);
	const TArray<FMissionReward> AtCompany2 = FSideContentRules::RollLoot(Rows, 1, 1, 10, Random, 2);
	TestEqual(TEXT("High HQ alone offers no trip loot"), AtCompany2.Num(), 0);
	const TArray<FMissionReward> AtCompany3 = FSideContentRules::RollLoot(Rows, 1, 1, 1, Random, 3);
	TestEqual(TEXT("HQ 1 company 3 has basic trip loot"), AtCompany3.Num(), 1);
	if (AtCompany3.Num() == 1) TestEqual(TEXT("HQ 5 resource remains gated"), AtCompany3[0].ItemType, EItemType::BusinessCardPaper);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCompanyFeatureUnlockTest, "CGR.SideContent.CompanyFeatureUnlock.IndependentBuildings",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCompanyFeatureUnlockTest::RunTest(const FString& Parameters)
{
	UGameInstance* GI = nullptr;
	if (GEngine)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE)
			{
				if (UWorld* GameWorld = Context.World()) { GI = GameWorld->GetGameInstance(); break; }
			}
		}
	}
	USaveLoadManager* SaveMgr = GI ? GI->GetSubsystem<USaveLoadManager>() : nullptr;
	USideContentSubsystem* Side = GI ? GI->GetSubsystem<USideContentSubsystem>() : nullptr;
	USaveGame_GameData* SaveObject = SaveMgr ? SaveMgr->GetCurrentSaveData() : nullptr;
	if (!SaveMgr || !Side || !SaveObject)
	{
		AddError(TEXT("Requires a running Game/PIE instance with a loaded save and imported feature DT."));
		return false;
	}

	const FGameSaveData OriginalGame = SaveObject->GameData;
	const FSideContentSaveData OriginalSide = Side->GetSideContentData();
	struct FRestore
	{
		USaveGame_GameData* SaveObject;
		USideContentSubsystem* Side;
		FGameSaveData Game;
		FSideContentSaveData SideData;
		~FRestore() { SaveObject->GameData = Game; Side->SetSideContentData(SideData); }
	} Restore{SaveObject, Side, OriginalGame, OriginalSide};

	constexpr int32 FirstBuilding = 900001;
	constexpr int32 SecondBuilding = 900002;
	for (int32 BuildingIndex : {FirstBuilding, SecondBuilding})
	{
		FBuildingEntitySaveData Building;
		Building.BuildingIndex = BuildingIndex;
		Building.BuildingData.CompanyType = ECompanyType::Game;
		SaveObject->GameData.Buildings.Add(Building);
	}
	SaveObject->GameData.OfficeDataMap.FindOrAdd(FirstBuilding).TierProgress.CurrentTier = 1;
	SaveObject->GameData.OfficeDataMap.FindOrAdd(SecondBuilding).TierProgress.CurrentTier = 3;
	SaveObject->GameData.HQLevel = 10;

	FSideContentSaveData TestSide = OriginalSide;
	FSideBuildingState LockedState;
	LockedState.BuildingIndex = FirstBuilding;
	LockedState.CourseEntries = 5;
	LockedState.CourseBest = 1;
	TestSide.Buildings.Add(LockedState);
	Side->SetSideContentData(TestSide);

	int32 Required = -1;
	FText Label;
	TestFalse(TEXT("High HQ cannot unlock research in a level 1 company"),
		Side->GetCompanyFeatureUnlock(FirstBuilding, TEXT("TrainingCenter"), Required, Label));
	TestEqual(TEXT("Research threshold comes from DT"), Required, 2);
	TestFalse(TEXT("Level 1 company cannot unlock trips"),
		Side->GetCompanyFeatureUnlock(FirstBuilding, TEXT("BusinessTrip"), Required, Label));
	TestEqual(TEXT("Trip threshold comes from DT"), Required, 3);
	TestTrue(TEXT("Other owned level 3 company independently unlocks trips"),
		Side->GetCompanyFeatureUnlock(SecondBuilding, TEXT("BusinessTrip"), Required, Label));
	TestFalse(TEXT("Unowned building is never unlocked"),
		Side->GetCompanyFeatureUnlock(900003, TEXT("TrainingCenter"), Required, Label));
	TestEqual(TEXT("Training domain reports company lock"), Side->GetTrainingBlockReason(FirstBuilding), ETrainingBlockReason::CompanyLocked);
	TestEqual(TEXT("Trip domain reports company lock"), Side->GetDispatchBlockReason(FirstBuilding, NAME_None, {}), EDispatchBlockReason::CompanyLocked);
	TestEqual(TEXT("Locked company receives no trip cards"), Side->GetDispatchCards(FirstBuilding).Num(), 0);

	const TMap<EResourceType, int64> ResourcesBefore = SaveObject->GameData.ResourceBank;
	const int32 EntriesBefore = Side->GetTrainingEntries(FirstBuilding);
	TArray<FMissionReward> Rewards;
	TestFalse(TEXT("Locked challenge fails"), Side->Challenge(FirstBuilding, Rewards));
	TestFalse(TEXT("Locked sweep fails"), Side->SweepTraining(FirstBuilding, Rewards));
	TestFalse(TEXT("Locked departure fails"), Side->Depart(FirstBuilding, NAME_None, {}));
	TestEqual(TEXT("Locked attempts spend no entries"), Side->GetTrainingEntries(FirstBuilding), EntriesBefore);
	TestTrue(TEXT("Locked attempts spend no resources"), SaveObject->GameData.ResourceBank.OrderIndependentCompareEqual(ResourcesBefore));
	TestFalse(TEXT("Locked attempts create no dispatch"), Side->IsDispatchActive(FirstBuilding));

	SaveObject->GameData.HQLevel = 1;
	SaveObject->GameData.OfficeDataMap.FindChecked(FirstBuilding).TierProgress.CurrentTier = 2;
	TestTrue(TEXT("Level 2 company unlocks research despite HQ 1"),
		Side->GetCompanyFeatureUnlock(FirstBuilding, TEXT("TrainingCenter"), Required, Label));
	TestFalse(TEXT("Level 2 company keeps trips locked"),
		Side->GetCompanyFeatureUnlock(FirstBuilding, TEXT("BusinessTrip"), Required, Label));
	SaveObject->GameData.OfficeDataMap.FindChecked(FirstBuilding).TierProgress.CurrentTier = 3;
	TestTrue(TEXT("Level 3 company unlocks trips despite HQ 1"),
		Side->GetCompanyFeatureUnlock(FirstBuilding, TEXT("BusinessTrip"), Required, Label));
	return true;
}

#endif
