#if WITH_DEV_AUTOMATION_TESTS

#include "Core/CGGameInstance.h"
#include "Data/GameSaveData.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Global/CGDevSettings.h"
#include "HAL/PlatformFileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Manager/GoalBoardSubsystem.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

struct FGuideQuestTestAccessor
{
	static void Signal(UGoalBoardSubsystem& Board, EMissionConditionType Type, int64 Amount)
	{
		Board.HandleConditionSignal(Type, Amount);
	}
};

struct FGuideQuestTableTestAccessor
{
	static void SwapRows(UTableManagerSubsystem& Tables, TMap<FName, FGoalTable>& Rows,
		TArray<FName>& RowOrder)
	{
		Swap(Tables.GoalDataMap, Rows);
		Swap(Tables.GoalRowOrder, RowOrder);
	}
};

namespace GuideQuestLifecycleTests
{
	constexpr int64 InitialMoney = 1000;
	constexpr int64 RewardMoney = 27;
	const FName GoalID(TEXT("GQ_VerifyBulk"));
	const FString SlotName(TEXT("GameSlot"));
	const FName ProductionGoalID(TEXT("G38_BrickProduction"));
	const FName ElectronicsGoalID(TEXT("G21_BuildElectronics"));

	struct FScopedGoalRows
	{
		UTableManagerSubsystem* Tables;
		TMap<FName, FGoalTable> PreviousRows;
		TArray<FName> PreviousOrder;

		explicit FScopedGoalRows(UTableManagerSubsystem& InTables) : Tables(&InTables)
		{
			FGoalTable Row;
			Row.ConditionType = EMissionConditionType::EnhanceBuildingStat;
			Row.ConditionAmount = 10;
			Row.RepeatLimit = 2;
			Row.bCountActions = true;
			Row.RewardMoney = RewardMoney;
			TMap<FName, FGoalTable> FixtureRows;
			FixtureRows.Add(GoalID, Row);
			TArray<FName> FixtureOrder{GoalID};
			FGuideQuestTableTestAccessor::SwapRows(InTables, FixtureRows, FixtureOrder);
			PreviousRows = MoveTemp(FixtureRows);
			PreviousOrder = MoveTemp(FixtureOrder);
		}

		~FScopedGoalRows()
		{
			if (Tables)
			{
				FGuideQuestTableTestAccessor::SwapRows(*Tables, PreviousRows, PreviousOrder);
			}
		}
	};

	struct FScopedProductionGoalRows
	{
		UTableManagerSubsystem* Tables;
		TMap<FName, FGoalTable> PreviousRows;
		TArray<FName> PreviousOrder;

		explicit FScopedProductionGoalRows(UTableManagerSubsystem& InTables) : Tables(&InTables)
		{
			FGoalTable Production;
			Production.ConditionType = EMissionConditionType::ReachFactoryProductionLevel;
			Production.ConditionAmount = 12;
			Production.PrereqGoalID = TEXT("G_HQ3");
			Production.RequiredHQLevel = 3;
			Production.RewardMoney = 100;
			Production.RecommendationPriority = 1;
			FGoalTable Electronics;
			Electronics.ConditionType = EMissionConditionType::BuildIndustry;
			Electronics.ConditionAmount = 1;
			Electronics.PrereqGoalID = ProductionGoalID;
			Electronics.RequiredHQLevel = 3;
			Electronics.CompanyType = ECompanyType::Electronics;
			TMap<FName, FGoalTable> FixtureRows;
			FixtureRows.Add(ProductionGoalID, Production);
			FixtureRows.Add(ElectronicsGoalID, Electronics);
			TArray<FName> FixtureOrder{ProductionGoalID, ElectronicsGoalID};
			FGuideQuestTableTestAccessor::SwapRows(InTables, FixtureRows, FixtureOrder);
			PreviousRows = MoveTemp(FixtureRows);
			PreviousOrder = MoveTemp(FixtureOrder);
		}

		~FScopedProductionGoalRows()
		{
			if (Tables) { FGuideQuestTableTestAccessor::SwapRows(*Tables, PreviousRows, PreviousOrder); }
		}
	};

	void CloseInstance(UCGGameInstance*& Instance)
	{
		if (!Instance) { return; }
		UWorld* TestWorld = static_cast<UGameInstance*>(Instance)->GetWorld();
		static_cast<UGameInstance*>(Instance)->Shutdown();
		if (TestWorld && GEngine)
		{
			TestWorld->SetGameInstance(nullptr);
			GEngine->DestroyWorldContext(TestWorld);
			TestWorld->DestroyWorld(false);
		}
		Instance = nullptr;
	}

	bool ReadEntry(FAutomationTestBase& Test, UGoalBoardSubsystem& Board, FGoalBoardEntry& OutEntry)
	{
		TArray<FGoalBoardEntry> Entries;
		Board.GetBoardEntries(Entries);
		const FGoalBoardEntry* Found = Entries.FindByPredicate([](const FGoalBoardEntry& Entry)
			{ return Entry.GoalID == GoalID; });
		if (!Test.TestNotNull(TEXT("The fixture goal is present on the actual board"), Found)) { return false; }
		OutEntry = *Found;
		return true;
	}

	USaveGame_GameData* MemoryRoundTrip(FAutomationTestBase& Test, USaveGame_GameData* Source)
	{
		TArray<uint8> Bytes;
		if (!Test.TestTrue(TEXT("SaveGameToMemory serializes the committed snapshot"),
			Source && UGameplayStatics::SaveGameToMemory(Source, Bytes))) { return nullptr; }
		USaveGame_GameData* Restored = Cast<USaveGame_GameData>(UGameplayStatics::LoadGameFromMemory(Bytes));
		Test.TestNotNull(TEXT("LoadGameFromMemory restores the goal snapshot"), Restored);
		return Restored;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGuideQuestLifecycleTest,
	"CGR.GoalBoard.GuideQuest.LifecycleAndRollback",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FGuideQuestLifecycleTest::RunTest(const FString& Parameters)
{
	using namespace GuideQuestLifecycleTests;
	if (!FPaths::ProjectSavedDir().Contains(TEXT("Saved_GuideQuestVerify")))
	{
		AddWarning(TEXT("Skipped: run with -saveddirsuffix=GuideQuestVerify to isolate fixture saves"));
		return true;
	}
	if (!TestTrue(TEXT("Engine is available"), GEngine != nullptr)
		|| !TestNull(TEXT("No playable game instance is active"), UCGGameInstance::GetInstance())
		|| !TestFalse(TEXT("The isolated fixture slot starts empty"),
			UGameplayStatics::DoesSaveGameExist(SlotName, 0))) { return false; }

	UCGDevSettings* Settings = GetMutableDefault<UCGDevSettings>();
	const ECGBackendMode PreviousMode = Settings->BackendMode;
	const bool bPreviousWipe = Settings->bWipeSaveOnLaunch;
	Settings->BackendMode = ECGBackendMode::Local;
	Settings->bWipeSaveOnLaunch = false;
	UCGGameInstance* Instance = nullptr;
	ON_SCOPE_EXIT
	{
		CloseInstance(Instance);
		UGameplayStatics::DeleteGameInSlot(SlotName, 0);
		Settings->BackendMode = PreviousMode;
		Settings->bWipeSaveOnLaunch = bPreviousWipe;
	};

	USaveGame_GameData* Seed = NewObject<USaveGame_GameData>(GetTransientPackage());
	Seed->GameData.bTutorialCompleted = true;
	Seed->GameData.bGoalBoardUnlocked = true;
	Seed->GameData.ResourceBank.Add(EResourceType::Money, InitialMoney);
	if (!TestTrue(TEXT("The isolated fixture save is created"),
		UGameplayStatics::SaveGameToSlot(Seed, SlotName, 0))) { return false; }

	Instance = NewObject<UCGGameInstance>(GEngine);
	Instance->InitializeStandalone();
	USaveLoadManager* Saves = Instance->GetSubsystem<USaveLoadManager>();
	UGoalBoardSubsystem* Board = Instance->GetSubsystem<UGoalBoardSubsystem>();
	UTableManagerSubsystem* Tables = Instance->GetSubsystem<UTableManagerSubsystem>();
	UResourceItemManager* Resources = Instance->GetSubsystem<UResourceItemManager>();
	if (!TestTrue(TEXT("Real goal, table, resource and save subsystems exist"),
		Saves && Board && Tables && Resources)) { return false; }
	{
		FScopedGoalRows FixtureRows(*Tables);
		if (!TestTrue(TEXT("Fixture save loads through SaveLoadManager"), Saves->LoadGameData())) { return false; }
		if (!TestTrue(TEXT("Board is unlocked from the loaded save"), Board->IsUnlocked())) { return false; }

		FGuideQuestTestAccessor::Signal(*Board, EMissionConditionType::EnhanceBuildingStat, 50);
		FGoalBoardEntry Entry;
		if (!ReadEntry(*this, *Board, Entry)) { return false; }
		TestEqual(TEXT("One x50 action credits all fifty successful upgrades"), Entry.ProgressCurrent, int64(50));
		TestEqual(TEXT("The first target is ten"), Entry.ProgressTarget, int64(10));
		TestEqual(TEXT("The goal waits for a manual claim"), Entry.State, EGoalState::ReadyToClaim);
		TestEqual(TEXT("Progress alone does not pay the reward"),
			Resources->GetResourceAmount(EResourceType::Money), InitialMoney);
		if (!TestTrue(TEXT("The ready goal can be selected as active guidance"), Board->SetActiveGoal(GoalID))) return false;
		if (!TestTrue(TEXT("Ready and active state saves through the normal API"), Saves->SaveGameData())) return false;
		USaveGame_GameData* ReadyMemory = MemoryRoundTrip(*this, Saves->GetCurrentSaveData());
		if (!ReadyMemory) return false;
		TestTrue(TEXT("ReadyGoalIDs survives memory serialization"), ReadyMemory->GameData.ReadyGoalIDs.Contains(GoalID));
		TestEqual(TEXT("ActiveGuideGoalID survives memory serialization"),
			ReadyMemory->GameData.ActiveGuideGoalID, GoalID);
		TestEqual(TEXT("x50 progress survives memory serialization"),
			ReadyMemory->GameData.GoalEventProgressByID.FindRef(GoalID), int64(50));

		if (!TestTrue(TEXT("Claim succeeds with the displayed zero count"), Board->ClaimGoal(GoalID, 0))) return false;
		if (!ReadEntry(*this, *Board, Entry)) return false;
		TestEqual(TEXT("Ten spent actions leave forty actions for the next cycle"), Entry.ProgressCurrent, int64(40));
		TestEqual(TEXT("The second cycle is already ready from carried progress"), Entry.State, EGoalState::ReadyToClaim);
		TestEqual(TEXT("Only one claim is recorded"), Entry.ClaimCount, 1);
		TestEqual(TEXT("The exact reward is credited once"),
			Resources->GetResourceAmount(EResourceType::Money), InitialMoney + RewardMoney);
		TestFalse(TEXT("A duplicate claim with stale count is rejected"), Board->ClaimGoal(GoalID, 0));
		TestFalse(TEXT("An arbitrary future count is rejected"), Board->ClaimGoal(GoalID, 3));
		TestEqual(TEXT("Rejected claims cannot pay again"),
			Resources->GetResourceAmount(EResourceType::Money), InitialMoney + RewardMoney);

		USaveGame_GameData* ClaimedDisk = Cast<USaveGame_GameData>(
			UGameplayStatics::LoadGameFromSlot(SlotName, 0));
		if (!TestNotNull(TEXT("Claim writes an actual save slot"), ClaimedDisk)) return false;
		TestEqual(TEXT("The committed save contains the claimed cycle"),
			ClaimedDisk->GameData.GoalSuccessCountsByID.FindRef(GoalID), 1);
		TestEqual(TEXT("The same committed save contains the reward"),
			ClaimedDisk->GameData.ResourceBank.FindRef(EResourceType::Money), InitialMoney + RewardMoney);
		TestEqual(TEXT("Quest rewards are not business sales revenue"),
			ClaimedDisk->GameData.TotalRevenueEarned, ReadyMemory->GameData.TotalRevenueEarned);
		TestEqual(TEXT("The same committed save contains the remaining progress"),
			ClaimedDisk->GameData.GoalEventProgressByID.FindRef(GoalID), int64(40));
	}
	CloseInstance(Instance);
	if (!TestNull(TEXT("Shutdown clears the game instance singleton"), UCGGameInstance::GetInstance())) return false;

	Instance = NewObject<UCGGameInstance>(GEngine);
	Instance->InitializeStandalone();
	Saves = Instance->GetSubsystem<USaveLoadManager>();
	Board = Instance->GetSubsystem<UGoalBoardSubsystem>();
	Tables = Instance->GetSubsystem<UTableManagerSubsystem>();
	Resources = Instance->GetSubsystem<UResourceItemManager>();
	if (!TestTrue(TEXT("Restarted subsystems exist"), Saves && Board && Tables && Resources)) return false;
	{
		FScopedGoalRows FixtureRows(*Tables);
		if (!TestTrue(TEXT("Claimed save reloads through SaveLoadManager"), Saves->LoadGameData())) return false;
		FGoalBoardEntry Entry;
		if (!ReadEntry(*this, *Board, Entry)) return false;
		TestEqual(TEXT("Reload keeps one claimed cycle"), Entry.ClaimCount, 1);
		TestEqual(TEXT("Reload keeps forty carried actions"), Entry.ProgressCurrent, int64(40));
		TestEqual(TEXT("Reload keeps the next cycle ready"), Entry.State, EGoalState::ReadyToClaim);
		TestEqual(TEXT("Reload keeps the reward and claim paired"),
			Resources->GetResourceAmount(EResourceType::Money), InitialMoney + RewardMoney);
		TestEqual(TEXT("Reload keeps active guidance"), Board->GetRecommendedGoalID(), GoalID);

		const FGameSaveData BeforeFailure = Saves->GetCurrentSaveData()->GameData;
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		const FString SavePath = FPaths::ProjectSavedDir() / TEXT("SaveGames/GameSlot.sav");
		if (!TestTrue(TEXT("The isolated slot is made read-only to reject a real disk write"),
			PlatformFile.SetReadOnly(*SavePath, true))) return false;
		ON_SCOPE_EXIT { PlatformFile.SetReadOnly(*SavePath, false); };
		AddExpectedError(TEXT("\\[SaveLoadManager\\] Failed to save game!"), EAutomationExpectedErrorFlags::Contains, 1);
		AddExpectedError(TEXT("\\[GoalBoard\\] Claim save failed; restored"), EAutomationExpectedErrorFlags::Contains, 1);
		TestFalse(TEXT("A ready second claim fails when durable saving is refused"), Board->ClaimGoal(GoalID, 1));
		if (!ReadEntry(*this, *Board, Entry)) return false;
		TestEqual(TEXT("Failed claim restores the ready state"), Entry.State, EGoalState::ReadyToClaim);
		TestEqual(TEXT("Failed claim restores the count"), Entry.ClaimCount, 1);
		TestEqual(TEXT("Failed claim restores all forty actions"), Entry.ProgressCurrent, int64(40));
		TestEqual(TEXT("Failed claim restores Money"),
			Resources->GetResourceAmount(EResourceType::Money), InitialMoney + RewardMoney);
		TestTrue(TEXT("Failed claim restores the complete cached snapshot"),
			FGameSaveData::StaticStruct()->CompareScriptStruct(
				&Saves->GetCurrentSaveData()->GameData, &BeforeFailure, 0));
		USaveGame_GameData* RejectedDisk = Cast<USaveGame_GameData>(
			UGameplayStatics::LoadGameFromSlot(SlotName, 0));
		if (!TestNotNull(TEXT("Previously committed save still exists"), RejectedDisk)) return false;
		TestEqual(TEXT("Failed claim does not overwrite the disk count"),
			RejectedDisk->GameData.GoalSuccessCountsByID.FindRef(GoalID), 1);
		TestEqual(TEXT("Failed claim does not overwrite disk Money"),
			RejectedDisk->GameData.ResourceBank.FindRef(EResourceType::Money), InitialMoney + RewardMoney);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGuideQuestFactoryProductionReachTest,
	"CGR.GoalBoard.GuideQuest.FactoryProductionReach",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FGuideQuestFactoryProductionReachTest::RunTest(const FString& Parameters)
{
	using namespace GuideQuestLifecycleTests;
	if (!FPaths::ProjectSavedDir().Contains(TEXT("Saved_GuideQuestVerify")))
	{
		AddWarning(TEXT("Skipped: run with -saveddirsuffix=GuideQuestVerify to isolate fixture saves"));
		return true;
	}
	if (!TestNull(TEXT("No playable game instance is active"), UCGGameInstance::GetInstance())
		|| !TestFalse(TEXT("The isolated fixture slot starts empty"), UGameplayStatics::DoesSaveGameExist(SlotName, 0)))
	{
		return false;
	}
	UCGDevSettings* Settings = GetMutableDefault<UCGDevSettings>();
	const ECGBackendMode PreviousMode = Settings->BackendMode;
	const bool bPreviousWipe = Settings->bWipeSaveOnLaunch;
	Settings->BackendMode = ECGBackendMode::Local;
	Settings->bWipeSaveOnLaunch = false;
	UCGGameInstance* Instance = nullptr;
	ON_SCOPE_EXIT
	{
		CloseInstance(Instance);
		UGameplayStatics::DeleteGameInSlot(SlotName, 0);
		Settings->BackendMode = PreviousMode;
		Settings->bWipeSaveOnLaunch = bPreviousWipe;
	};

	USaveGame_GameData* Seed = NewObject<USaveGame_GameData>(GetTransientPackage());
	Seed->GameData.bTutorialCompleted = true;
	Seed->GameData.bGoalBoardUnlocked = true;
	Seed->GameData.HQLevel = 3;
	Seed->GameData.CompletedGoalIDs.Add(TEXT("G_HQ3"));
	Seed->GameData.ResourceBank.Add(EResourceType::Money, InitialMoney);
	FFactorySaveData Factory(TEXT("FactoryProductionFixture"));
	Factory.UpgradeData.FindChecked(EFactoryUpgradeType::HoldProductionSpeed).Level = 20;
	Factory.UpgradeData.FindChecked(EFactoryUpgradeType::HoldProductionAmount).Level = 11;
	Seed->GameData.FactoryData.Add(Factory.FactoryID, Factory);
	if (!TestTrue(TEXT("HQ3 factory fixture is saved"), UGameplayStatics::SaveGameToSlot(Seed, SlotName, 0))) return false;

	Instance = NewObject<UCGGameInstance>(GEngine);
	Instance->InitializeStandalone();
	USaveLoadManager* Saves = Instance->GetSubsystem<USaveLoadManager>();
	UGoalBoardSubsystem* Board = Instance->GetSubsystem<UGoalBoardSubsystem>();
	UTableManagerSubsystem* Tables = Instance->GetSubsystem<UTableManagerSubsystem>();
	if (!TestTrue(TEXT("Goal subsystems exist"), Saves && Board && Tables)) return false;
	{
		FScopedProductionGoalRows FixtureRows(*Tables);
		if (!TestTrue(TEXT("Factory save loads"), Saves->LoadGameData())) return false;
		auto FindEntry = [Board](FName ID) -> FGoalBoardEntry
		{
			TArray<FGoalBoardEntry> Entries;
			Board->GetBoardEntries(Entries);
			const FGoalBoardEntry* Found = Entries.FindByPredicate([ID](const FGoalBoardEntry& Entry) { return Entry.GoalID == ID; });
			return Found ? *Found : FGoalBoardEntry();
		};
		FGoalBoardEntry Production = FindEntry(ProductionGoalID);
		TestEqual(TEXT("Fast speed cannot count as production level"), Production.ProgressCurrent, int64(11));
		TestEqual(TEXT("G38 is the next recommended goal"), Board->GetRecommendedGoalID(), ProductionGoalID);
		TestEqual(TEXT("Electronics waits for the production claim"), FindEntry(ElectronicsGoalID).State, EGoalState::Locked);
		FGuideQuestTestAccessor::Signal(*Board, EMissionConditionType::UpgradeFactoryStats, 100);
		TestEqual(TEXT("Repeated speed purchases cannot complete G38"), FindEntry(ProductionGoalID).State, EGoalState::InProgress);

		Saves->GetCurrentSaveData()->GameData.FactoryData.FindChecked(Factory.FactoryID)
			.UpgradeData.FindChecked(EFactoryUpgradeType::HoldProductionAmount).Level = 12;
		FGuideQuestTestAccessor::Signal(*Board, EMissionConditionType::ReachFactoryProductionLevel, 1);
		Production = FindEntry(ProductionGoalID);
		TestEqual(TEXT("A saved production level reaches the target"), Production.ProgressCurrent, int64(12));
		TestEqual(TEXT("G38 is ready to claim"), Production.State, EGoalState::ReadyToClaim);
		if (!TestTrue(TEXT("G38 claim succeeds"), Board->ClaimGoal(ProductionGoalID, 0))) return false;
		TestEqual(TEXT("G21 unlocks after G38 claim"), FindEntry(ElectronicsGoalID).State, EGoalState::InProgress);
		USaveGame_GameData* Committed = Cast<USaveGame_GameData>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
		if (!TestNotNull(TEXT("Claim save exists"), Committed)) return false;
		TestTrue(TEXT("Claim and factory level persist together"),
			Committed->GameData.CompletedGoalIDs.Contains(ProductionGoalID)
			&& Committed->GameData.FactoryData.FindChecked(Factory.FactoryID)
				.UpgradeData.FindChecked(EFactoryUpgradeType::HoldProductionAmount).Level >= 12);
	}
	return true;
}

#endif
