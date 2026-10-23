#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "Manager/GoalBoardSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/ContentGuideStep.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CGRGoalTrackerDisplay
{
FName PreserveExpandedRow(const TArray<FGoalBoardEntry>& InAllEntries, FName InExpandedGoalID);
void BuildDisplaySet(const TArray<FGoalBoardEntry>& InAllEntries, FName InTrackedGoalID,
	FName InRecommendedGoalID, bool bInRowsExpanded,
	TArray<FGoalBoardEntry>& OutVisible, int32& OutHiddenCount);
}

struct FGoalTableRowOrderTestAccessor
{
	static void Initialize(UTableManagerSubsystem& TableMgr, UDataTable* GoalTable)
	{
		TableMgr.GoalDataTable = GoalTable;
		TableMgr.InitializeGoalTable();
	}
};

struct FGoalBoardEntryOrderTestAccessor
{
	static void BuildEntries(const UGoalBoardSubsystem& GoalBoard, const UTableManagerSubsystem& TableMgr,
		TArray<FGoalBoardEntry>& OutEntries)
	{
		GoalBoard.BuildBoardEntriesFromTableManager(TableMgr, OutEntries);
	}
};

namespace
{
FGoalBoardEntry MakeEntry(const TCHAR* InID, EGoalState InState = EGoalState::InProgress,
	bool bReady = true, bool bPracticed = false)
{
	FGoalBoardEntry Entry;
	Entry.GoalID = FName(InID);
	Entry.State = InState;
	Entry.bRecommendable = bReady;
	Entry.bPracticed = bPracticed;
	return Entry;
}

bool ContainsGoal(const TArray<FGoalBoardEntry>& Entries, const TCHAR* ID)
{
	return Entries.ContainsByPredicate([ID](const FGoalBoardEntry& Entry) { return Entry.GoalID == FName(ID); });
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGoalBoardDataTableRowOrderPreservedTest,
	"CGR.GoalBoard.DataTableRowOrderPreserved",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGoalBoardDataTableRowOrderPreservedTest::RunTest(const FString& Parameters)
{
	UDataTable* FixtureTable = NewObject<UDataTable>();
	FixtureTable->RowStruct = FGoalTable::StaticStruct();
	const TArray<FName> ExpectedOrder = { TEXT("G2_HQLevel3"), TEXT("G11_AcquirePlot"), TEXT("G1_RaiseFloor") };
	FGoalTable FixtureRow;
	FixtureRow.ConditionType = EMissionConditionType::None;
	for (const FName GoalID : ExpectedOrder) { FixtureTable->AddRow(GoalID, FixtureRow); }
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UTableManagerSubsystem* TableMgr = NewObject<UTableManagerSubsystem>(GameInstance);
	UGoalBoardSubsystem* GoalBoard = NewObject<UGoalBoardSubsystem>(GameInstance);
	FGoalTableRowOrderTestAccessor::Initialize(*TableMgr, FixtureTable);
	TArray<FGoalBoardEntry> ActualEntries;
	FGoalBoardEntryOrderTestAccessor::BuildEntries(*GoalBoard, *TableMgr, ActualEntries);
	if (TestEqual(TEXT("GoalBoard preserves DT row count"), ActualEntries.Num(), ExpectedOrder.Num()))
	{
		for (int32 Index = 0; Index < ExpectedOrder.Num(); ++Index)
		{
			TestEqual(TEXT("GoalBoard preserves DT row order"), ActualEntries[Index].GoalID, ExpectedOrder[Index]);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGoalTrackerAvailableOnlyTest,
	"CGR.GoalTracker.AvailableOnly",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGoalTrackerAvailableOnlyTest::RunTest(const FString& Parameters)
{
	const TArray<FGoalBoardEntry> Entries = {
		MakeEntry(TEXT("Locked"), EGoalState::Locked),
		MakeEntry(TEXT("Unaffordable"), EGoalState::InProgress, false),
		MakeEntry(TEXT("Earned"), EGoalState::ReadyToClaim, false),
		MakeEntry(TEXT("ReadyFirst")),
		MakeEntry(TEXT("Completed"), EGoalState::Completed),
		MakeEntry(TEXT("ReadyPracticed"), EGoalState::InProgress, true, true)
	};
	TArray<FGoalBoardEntry> Visible;
	int32 HiddenCount = -1;
	CGRGoalTrackerDisplay::BuildDisplaySet(Entries, NAME_None, TEXT("Unaffordable"),
		false, Visible, HiddenCount);
	if (TestEqual(TEXT("Only one quest appears"), Visible.Num(), 1))
	{
		TestEqual(TEXT("Earned reward stays visible"), Visible[0].GoalID, FName(TEXT("Earned")));
	}
	TestEqual(TEXT("Other available quests stay in the list"), HiddenCount, 2);
	CGRGoalTrackerDisplay::BuildDisplaySet(Entries, NAME_None, NAME_None,
		true, Visible, HiddenCount);
	TestEqual(TEXT("Optional list includes ready rewards and live quests"), Visible.Num(), 3);
	TestTrue(TEXT("Earned reward remains selectable"), ContainsGoal(Visible, TEXT("Earned")));
	TestTrue(TEXT("Practiced hint remains available"), ContainsGoal(Visible, TEXT("ReadyPracticed")));
	TestFalse(TEXT("Locked hint is absent"), ContainsGoal(Visible, TEXT("Locked")));
	TestFalse(TEXT("Unaffordable hint is absent"), ContainsGoal(Visible, TEXT("Unaffordable")));
	TestFalse(TEXT("Completed hint is absent"), ContainsGoal(Visible, TEXT("Completed")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGoalTrackerSelectionClearsTest,
	"CGR.GoalTracker.SelectionClears",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGoalTrackerSelectionClearsTest::RunTest(const FString& Parameters)
{
	TArray<FGoalBoardEntry> Entries = { MakeEntry(TEXT("Tracked")), MakeEntry(TEXT("Next")) };
	TArray<FGoalBoardEntry> Visible;
	int32 HiddenCount = -1;
	CGRGoalTrackerDisplay::BuildDisplaySet(Entries, TEXT("Tracked"), TEXT("Next"),
		false, Visible, HiddenCount);
	if (TestEqual(TEXT("Only active quest is shown"), Visible.Num(), 1))
	{
		TestEqual(TEXT("Saved active selection beats old tracking target"), Visible[0].GoalID, FName(TEXT("Next")));
	}
	Entries[0].State = EGoalState::ReadyToClaim;
	Entries[0].bRecommendable = false;
	TestEqual(TEXT("Earned expanded row survives"),
		CGRGoalTrackerDisplay::PreserveExpandedRow(Entries, TEXT("Tracked")), FName(TEXT("Tracked")));
	Entries[0].State = EGoalState::Completed;
	TestTrue(TEXT("Completed expanded row clears"),
		CGRGoalTrackerDisplay::PreserveExpandedRow(Entries, TEXT("Tracked")).IsNone());
	Entries[0].State = EGoalState::InProgress;
	Entries[0].bRecommendable = false;
	TestTrue(TEXT("Unavailable expanded row clears"),
		CGRGoalTrackerDisplay::PreserveExpandedRow(Entries, TEXT("Tracked")).IsNone());
	Entries[0].State = EGoalState::Completed;
	CGRGoalTrackerDisplay::BuildDisplaySet(Entries, TEXT("Tracked"), TEXT("Next"),
		false, Visible, HiddenCount);
	if (TestEqual(TEXT("Another ready hint replaces completed tracked hint"), Visible.Num(), 1))
	{
		TestEqual(TEXT("Replacement is ready"), Visible[0].GoalID, FName(TEXT("Next")));
	}
	Entries[0].State = EGoalState::ReadyToClaim;
	CGRGoalTrackerDisplay::BuildDisplaySet(Entries, NAME_None, TEXT("Tracked"),
		false, Visible, HiddenCount);
	if (TestEqual(TEXT("Claimable quest remains after map recreation"), Visible.Num(), 1))
	{
		TestEqual(TEXT("Claimable quest keeps its identity"), Visible[0].GoalID, FName(TEXT("Tracked")));
	}
	Entries[0].State = EGoalState::Completed;
	Entries[1].bRecommendable = false;
	CGRGoalTrackerDisplay::BuildDisplaySet(Entries, NAME_None, NAME_None,
		false, Visible, HiddenCount);
	TestEqual(TEXT("No unavailable fallback card"), Visible.Num(), 0);
	TestEqual(TEXT("No unavailable More row"), HiddenCount, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGoalTrackerBoardSelectionWinsAfterClaimTest,
	"CGR.GoalTracker.BoardSelectionWinsAfterClaim",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGoalTrackerBoardSelectionWinsAfterClaimTest::RunTest(const FString& Parameters)
{
	const TArray<FGoalBoardEntry> Entries = {
		MakeEntry(TEXT("Repeat")),
		MakeEntry(TEXT("Milestone"))
	};
	TestEqual(TEXT("Old repeated row can remain expanded in the list"),
		CGRGoalTrackerDisplay::PreserveExpandedRow(Entries, TEXT("Repeat")), FName(TEXT("Repeat")));
	TArray<FGoalBoardEntry> Visible;
	int32 HiddenCount = -1;
	CGRGoalTrackerDisplay::BuildDisplaySet(Entries, TEXT("Repeat"), TEXT("Milestone"),
		false, Visible, HiddenCount);
	if (TestEqual(TEXT("Only Board's newly selected goal is shown"), Visible.Num(), 1))
	{
		TestEqual(TEXT("Old repeated row does not override milestone"),
			Visible[0].GoalID, FName(TEXT("Milestone")));
	}
	TestEqual(TEXT("Repeated goal remains available under Other Quests"), HiddenCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGoalStepStatInvestTest,
	"CGR.GoalStep.EmployeeGrowthKeepsSeparateActions",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGoalStepStatInvestTest::RunTest(const FString& Parameters)
{
	using namespace CGRGoalGuideStep;
	TestEqual(TEXT("Another office requires travel"), ResolveEmployeeGrowthStep(false, true, false, false), 0);
	TestEqual(TEXT("Open employee window"), ResolveEmployeeGrowthStep(true, false, false, false), 1);
	TestEqual(TEXT("SP hint stays on investment step"), ResolveEmployeeGrowthStep(true, true, true, false), 2);
	TestEqual(TEXT("Star hint points to enhancement button"), ResolveEmployeeGrowthStep(true, true, false, true), 2);
	TestEqual(TEXT("Star modal points to attempt button"), ResolveEmployeeGrowthStep(true, true, true, true), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGoalStepRaiseFloorTest,
	"CGR.GoalStep.RaiseFloorUsesLiveUIState",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGoalStepRaiseFloorTest::RunTest(const FString& Parameters)
{
	using namespace CGRGoalGuideStep;
	TestEqual(TEXT("Leave office"), ResolveRaiseFloorStep(true, false, false), 0);
	TestEqual(TEXT("Select building"), ResolveRaiseFloorStep(false, false, false), 1);
	TestEqual(TEXT("Open enhancement tab"), ResolveRaiseFloorStep(false, true, false), 2);
	TestEqual(TEXT("Choose buildup"), ResolveRaiseFloorStep(false, true, true), 3);
	return true;
}

#endif
