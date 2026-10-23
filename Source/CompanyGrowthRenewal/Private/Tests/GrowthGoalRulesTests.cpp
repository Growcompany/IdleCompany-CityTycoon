#include "Misc/AutomationTest.h"
#include "Data/GrowthGoalRules.h"
#include "Manager/GoalBoardSubsystem.h"
#include "UI/Panel/HQManagePanelWidget.h"
#include "UI/Element/Buttons/CostActionButtonWidget.h"
#include "UI/Element/Buttons/IconWithButtonWidget.h"
#include "Components/UniformGridPanel.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace CGRGrowthReadiness
{
bool IsFullAtPositiveCapacity(int32 EmployeeCount, int32 Capacity);
bool IsNextHQLevelGoal(int32 CurrentHQLevel, int64 TargetHQLevel, bool bHasNextHQLevel);
}

struct FHQGrowthGuideTargetTestAccessor
{
	static void SetState(UHQManagePanelWidget& Panel, int32 TabIndex, bool bReady,
		UIconWithButtonWidget* Tab, UCostActionButtonWidget* Button, UUniformGridPanel* Conditions)
	{
		Panel.CurrentTabIndex = TabIndex;
		Panel.bCanLevelUpNow = bReady;
		Panel.HQTab = Tab;
		Panel.LevelUpButton = Button;
		Panel.ConditionGrid = Conditions;
	}
};

namespace CGRGrowthRecommendation
{
FName Select(const TArray<FGoalBoardEntry>& Entries);
FName SelectAfterClaim(const TArray<FGoalBoardEntry>& Entries, FName ClaimedGoalID, int32 PreviousClaimCount);
FName SelectForRefresh(const TArray<FGoalBoardEntry>& Entries, FName ActiveGoalID,
	const TArray<FName>& LastAvailableGoalIDs);
}

namespace CGRGoalGuideCopy
{
FText RecruitFullOfficeLine(const FText& DefaultLine, const FText& TargetCompanyName,
	bool bInOffice, int32 TargetBuildingIndex, int32 OpenBuildingIndex);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrowthGoalLearningTest,
	"CGR.GoalBoard.Growth.SeparateLearning", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGrowthGoalLearningTest::RunTest(const FString& Parameters)
{
	FGameSaveData Data;
	FEmployeeInstance Employee;
	Employee.Level = 1;
	Employee.Experience = 1000.0;
	TArray<FEmployeeInstance> Employees{Employee};
	FGoalTable Row;
	Row.ConditionType = EMissionConditionType::LevelUpFirstEmployee;
	int64 Current = 0, Target = 0;
	GrowthGoalRules::GetProgress(Row, Data, Employees, Current, Target);
	TestEqual(TEXT("Training experience alone does not complete level-up learning"), Current, int64(0));
	Employees[0].Level = 2;
	GrowthGoalRules::GetProgress(Row, Data, Employees, Current, Target);
	TestEqual(TEXT("An actual employee level-up completes learning"), Current, int64(1));
	Row.ConditionType = EMissionConditionType::EnhanceFirstEmployee;
	GrowthGoalRules::GetProgress(Row, Data, Employees, Current, Target);
	TestEqual(TEXT("Employee level-up does not complete star learning"), Current, int64(0));
	FWorkstationSaveData Desk;
	Desk.ComputerSetupLevel = EComputerSetupLevel::Level2;
	Desk.UpgradeLevels.Add(EWorkstationUpgradeType::Processing, 1);
	Data.OfficeDataMap.FindOrAdd(7).OfficeWorkstations.Add(Desk);
	Row.ConditionType = EMissionConditionType::UpgradeFirstWorkstation;
	GrowthGoalRules::GetProgress(Row, Data, Employees, Current, Target);
	TestEqual(TEXT("Promotion keeps first growth credited after component reset"), Current, int64(1));
	Row.ConditionType = EMissionConditionType::PromoteFirstWorkstation;
	GrowthGoalRules::GetProgress(Row, Data, Employees, Current, Target);
	TestEqual(TEXT("An existing promoted desk is credited"), Current, int64(1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrowthGoalCompanyTest,
	"CGR.GoalBoard.Growth.CompanyIsolation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGrowthGoalCompanyTest::RunTest(const FString& Parameters)
{
	FGameSaveData Data;
	FBuildingEntitySaveData Building;
	Building.BuildingIndex = 7;
	Building.BuildingData.CompanyType = ECompanyType::Game;
	Data.Buildings.Add(Building);
	Building.BuildingIndex = 8;
	Data.Buildings.Add(Building);
	FShippedProjectRecord Record;
	Record.BuildingIndex = 7;
	Record.Industry = ECompanyType::Game;
	Record.ProjectIndex = 1;
	Data.ShippedProjects.Add(Record);
	FEmployeeInstance Employee;
	Employee.AssignedBuildingIndex = 8;
	Employee.DisciplinePoints.Init(0, static_cast<int32>(EProductionDiscipline::Count));
	Employee.DisciplinePoints[static_cast<int32>(EProductionDiscipline::Plan)] = 10;
	TArray<FEmployeeInstance> Employees{Employee};
	FGoalTable Row;
	Row.ConditionType = EMissionConditionType::RecruitGoalDiscipline;
	Row.RequiredDiscipline = EProductionDiscipline::Plan;
	Row.RequiredProjectIndex = 1;
	int64 Current = 0, Target = 0;
	GrowthGoalRules::GetProgress(Row, Data, Employees, Current, Target);
	TestEqual(TEXT("Planner in another company does not satisfy next-project team"), Current, int64(0));
	Employees[0].AssignedBuildingIndex = 7;
	Employees[0].DisciplinePoints[static_cast<int32>(EProductionDiscipline::Plan)] = 0;
	Employees[0].DisciplinePoints[static_cast<int32>(EProductionDiscipline::Dev)] = 10;
	GrowthGoalRules::GetProgress(Row, Data, Employees, Current, Target);
	TestEqual(TEXT("Hiring development in the target company keeps planning guidance incomplete"), Current, int64(0));
	Employees[0].DisciplinePoints[static_cast<int32>(EProductionDiscipline::Dev)] = 0;
	Employees[0].DisciplinePoints[static_cast<int32>(EProductionDiscipline::Plan)] = 10;
	FProjectData Project;
	Project.Weight_Plan = 1;
	Project.Weight_Dev = 1;
	TestFalse(TEXT("A planner alone cannot staff a project needing development"),
		GrowthGoalRules::HasProjectStaff(Project, 7, Employees));
	FEmployeeInstance Developer = Employees[0];
	Developer.DisciplinePoints[static_cast<int32>(EProductionDiscipline::Plan)] = 0;
	Developer.DisciplinePoints[static_cast<int32>(EProductionDiscipline::Dev)] = 10;
	Developer.AssignedBuildingIndex = 8;
	Employees.Add(Developer);
	TestFalse(TEXT("A developer in another company cannot staff this project"),
		GrowthGoalRules::HasProjectStaff(Project, 7, Employees));
	Employees.Last().AssignedBuildingIndex = 7;
	TestTrue(TEXT("All required disciplines in the target company enable its project hint"),
		GrowthGoalRules::HasProjectStaff(Project, 7, Employees));
	GrowthGoalRules::GetProgress(Row, Data, Employees, Current, Target);
	TestEqual(TEXT("Existing planner in the eligible company is credited"), Current, int64(1));
	Row.ConditionType = EMissionConditionType::LaunchGoalProject;
	Row.TargetProjectIndex = 2;
	GrowthGoalRules::GetProgress(Row, Data, Employees, Current, Target);
	TestEqual(TEXT("First launch does not complete second project"), Current, int64(0));
	Record.Industry = ECompanyType::IT;
	Record.ProjectIndex = 2;
	Data.ShippedProjects.Add(Record);
	GrowthGoalRules::GetProgress(Row, Data, Employees, Current, Target);
	TestEqual(TEXT("Project index in another industry does not count"), Current, int64(0));
	Record.Industry = ECompanyType::Game;
	Record.BuildingIndex = 8;
	Data.ShippedProjects.Add(Record);
	GrowthGoalRules::GetProgress(Row, Data, Employees, Current, Target);
	TestEqual(TEXT("Prerequisite and target must be shipped in the same company"), Current, int64(0));
	Record.BuildingIndex = 7;
	Data.ShippedProjects.Add(Record);
	GrowthGoalRules::GetProgress(Row, Data, Employees, Current, Target);
	TestEqual(TEXT("Correct shipped project is credited"), Current, int64(1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrowthGoalRecruitTargetCopyTest,
	"CGR.GoalBoard.Growth.RecruitTargetCopy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGrowthGoalRecruitTargetCopyTest::RunTest(const FString& Parameters)
{
	const FText BaseLine = FText::FromString(TEXT("정원이 찼어요. 도시에서 회사를 키워요."));
	const FText TargetName = FText::FromString(TEXT("내 회사"));
	const FText DifferentOffice = CGRGoalGuideCopy::RecruitFullOfficeLine(BaseLine, TargetName, true, 1, 2);
	TestTrue(TEXT("A full first company is identified while the second company is open"),
		DifferentOffice.ToString().Contains(TargetName.ToString()));
	TestTrue(TEXT("The guide still directs the player to grow the target company in the city"),
		DifferentOffice.ToString().Contains(TEXT("도시")));
	TestEqual(TEXT("A full current office retains its DataTable line"),
		CGRGoalGuideCopy::RecruitFullOfficeLine(BaseLine, TargetName, true, 1, 1).ToString(), BaseLine.ToString());
	TestEqual(TEXT("The city keeps its DataTable line while spotlighting the target building"),
		CGRGoalGuideCopy::RecruitFullOfficeLine(BaseLine, TargetName, false, 1, 2).ToString(), BaseLine.ToString());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrowthGoalRecommendationTest,
	"CGR.GoalBoard.Growth.Recommendation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGrowthGoalRecommendationTest::RunTest(const FString& Parameters)
{
	FGoalBoardEntry Locked;
	Locked.GoalID = TEXT("Locked");
	Locked.State = EGoalState::Locked;
	Locked.Row.RecommendationPriority = 0;
	FGoalBoardEntry NotReady;
	NotReady.GoalID = TEXT("NotReady");
	NotReady.bRecommendable = false;
	NotReady.Row.RecommendationPriority = 1;
	FGoalBoardEntry Ready;
	Ready.GoalID = TEXT("Ready");
	Ready.Row.RecommendationPriority = 2;
	TArray<FGoalBoardEntry> Entries{Locked, NotReady, Ready};
	TestEqual(TEXT("Locked and unaffordable targets are skipped"), CGRGrowthRecommendation::Select(Entries), Ready.GoalID);
	Entries[1].bRecommendable = true;
	TestEqual(TEXT("Newly available learning takes priority"), CGRGrowthRecommendation::Select(Entries), NotReady.GoalID);
	Entries[2].State = EGoalState::Completed;
	TestEqual(TEXT("Completed guidance does not displace available learning"), CGRGrowthRecommendation::Select(Entries), NotReady.GoalID);
	Entries[1].State = EGoalState::Completed;
	TestTrue(TEXT("No impossible fallback is recommended"), CGRGrowthRecommendation::Select(Entries).IsNone());
	Entries[1].State = EGoalState::InProgress;
	Entries[1].Row.RepeatLimit = 0;
	Entries[1].ClaimCount = 2;
	Entries[2].State = EGoalState::InProgress;
	Entries[2].Row.RepeatLimit = 0;
	Entries[2].ClaimCount = 0;
	TestEqual(TEXT("Less practiced investments rotate ahead"), CGRGrowthRecommendation::Select(Entries), Ready.GoalID);
	Entries[1].State = EGoalState::ReadyToClaim;
	TestEqual(TEXT("Earned rewards precede new investment"), CGRGrowthRecommendation::Select(Entries), NotReady.GoalID);
	Entries[1].State = EGoalState::InProgress;
	Entries[1].Row.RepeatLimit = 1;
	TestEqual(TEXT("Actionable expansion precedes repetition"), CGRGrowthRecommendation::Select(Entries), NotReady.GoalID);

	FGoalBoardEntry Repeat;
	Repeat.GoalID = TEXT("SelectedRepeat");
	Repeat.State = EGoalState::InProgress;
	Repeat.bRecommendable = true;
	Repeat.Row.RepeatLimit = 0;
	FGoalBoardEntry Milestone;
	Milestone.GoalID = TEXT("ReadyMilestone");
	Milestone.State = EGoalState::ReadyToClaim;
	Milestone.bRecommendable = true;
	Milestone.Row.RepeatLimit = 1;
	const TArray<FGoalBoardEntry> RefreshEntries{Repeat, Milestone};
	const TArray<FName> KnownAvailable{Repeat.GoalID, Milestone.GoalID};
	const TArray<FName> RepeatOnly{Repeat.GoalID};
	const TArray<FName> NoAvailabilityBaseline;
	TestEqual(TEXT("Manual repeat selection survives an already known ready milestone"),
		CGRGrowthRecommendation::SelectForRefresh(RefreshEntries, Repeat.GoalID, KnownAvailable), Repeat.GoalID);
	TestEqual(TEXT("Newly available milestone replaces an in-progress repeat"),
		CGRGrowthRecommendation::SelectForRefresh(RefreshEntries, Repeat.GoalID, RepeatOnly), Milestone.GoalID);
	TArray<FGoalBoardEntry> ReadyRepeatEntries = RefreshEntries;
	ReadyRepeatEntries[0].State = EGoalState::ReadyToClaim;
	TestEqual(TEXT("A ready repeat keeps its claim despite a new milestone"),
		CGRGrowthRecommendation::SelectForRefresh(ReadyRepeatEntries, Repeat.GoalID, RepeatOnly), Repeat.GoalID);
	TestEqual(TEXT("Initial availability baseline preserves a reloaded active goal"),
		CGRGrowthRecommendation::SelectForRefresh(RefreshEntries, Repeat.GoalID, NoAvailabilityBaseline), Repeat.GoalID);
	TestEqual(TEXT("Invalid active goal falls back to the normal recommendation"),
		CGRGrowthRecommendation::SelectForRefresh(RefreshEntries, TEXT("RemovedGoal"), KnownAvailable), Milestone.GoalID);

	FGoalBoardEntry ReadyBacklog = Repeat;
	ReadyBacklog.GoalID = TEXT("ReadyDeskReward");
	ReadyBacklog.State = EGoalState::ReadyToClaim;
	FGoalBoardEntry DirectSuccessor = Milestone;
	DirectSuccessor.GoalID = TEXT("G33_CollectProduct");
	DirectSuccessor.State = EGoalState::InProgress;
	DirectSuccessor.Row.PrereqGoalID = TEXT("G32_CollectOre");
	FGoalBoardEntry UnrelatedMilestone = Milestone;
	UnrelatedMilestone.GoalID = TEXT("UnrelatedMilestone");
	UnrelatedMilestone.State = EGoalState::InProgress;
	UnrelatedMilestone.Row.RecommendationPriority = 0;
	UnrelatedMilestone.Row.PrereqGoalID = TEXT("OtherGoal");
	FGoalBoardEntry UnavailableSuccessor = DirectSuccessor;
	UnavailableSuccessor.GoalID = TEXT("UnavailableSuccessor");
	UnavailableSuccessor.bRecommendable = false;
	FGoalBoardEntry LockedSuccessor = DirectSuccessor;
	LockedSuccessor.GoalID = TEXT("LockedSuccessor");
	LockedSuccessor.State = EGoalState::Locked;
	FGoalBoardEntry RepeatingSuccessor = Repeat;
	RepeatingSuccessor.GoalID = TEXT("RepeatingSuccessor");
	RepeatingSuccessor.Row.PrereqGoalID = TEXT("G32_CollectOre");
	TArray<FGoalBoardEntry> AfterClaimEntries{ReadyBacklog, UnrelatedMilestone,
		UnavailableSuccessor, LockedSuccessor, RepeatingSuccessor, DirectSuccessor};
	TestEqual(TEXT("First claim follows its actionable direct one-time successor past ready repeat rewards"),
		CGRGrowthRecommendation::SelectAfterClaim(AfterClaimEntries, TEXT("G32_CollectOre"), 0), DirectSuccessor.GoalID);
	TestEqual(TEXT("Later repeat claims retain the normal ready-reward ordering"),
		CGRGrowthRecommendation::SelectAfterClaim(AfterClaimEntries, TEXT("G32_CollectOre"), 1), ReadyBacklog.GoalID);
	AfterClaimEntries.RemoveAt(5);
	TestEqual(TEXT("An unavailable or locked successor falls back to the normal selection"),
		CGRGrowthRecommendation::SelectAfterClaim(AfterClaimEntries, TEXT("G32_CollectOre"), 0), ReadyBacklog.GoalID);
	DirectSuccessor.State = EGoalState::ReadyToClaim;
	AfterClaimEntries.Add(DirectSuccessor);
	TestEqual(TEXT("A direct successor already ready to claim still advances the chain"),
		CGRGrowthRecommendation::SelectAfterClaim(AfterClaimEntries, TEXT("G32_CollectOre"), 0), DirectSuccessor.GoalID);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrowthGoalRecruitIntroTest,
	"CGR.GoalBoard.Growth.RecruitIntroOverlap", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGrowthGoalRecruitIntroTest::RunTest(const FString& Parameters)
{
	FGoalTable Row;
	Row.ConditionType = EMissionConditionType::RecruitGoalDiscipline;
	Row.RequiredDiscipline = EProductionDiscipline::Plan;
	TestTrue(TEXT("Planning goal in the open company replaces the automatic planning intro"),
		GrowthGoalRules::SuppressPlanningRecruitIntro(Row, 7, 7));
	TestFalse(TEXT("Another company's planning goal leaves this intro available"),
		GrowthGoalRules::SuppressPlanningRecruitIntro(Row, 8, 7));
	Row.RequiredDiscipline = EProductionDiscipline::Graphics;
	TestFalse(TEXT("Graphics guidance does not suppress the planning intro"),
		GrowthGoalRules::SuppressPlanningRecruitIntro(Row, 7, 7));
	Row.ConditionType = EMissionConditionType::LaunchGoalProject;
	Row.RequiredDiscipline = EProductionDiscipline::Plan;
	TestFalse(TEXT("A project goal does not suppress recruitment help"),
		GrowthGoalRules::SuppressPlanningRecruitIntro(Row, 7, 7));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrowthGoalPracticePolicyTest,
	"CGR.GoalBoard.Growth.PracticePolicy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGrowthGoalPracticePolicyTest::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("An empty or unresolved company capacity cannot suggest floor growth"),
		CGRGrowthReadiness::IsFullAtPositiveCapacity(0, 0));
	TestFalse(TEXT("One employee with a free position cannot suggest floor growth"),
		CGRGrowthReadiness::IsFullAtPositiveCapacity(1, 2));
	TestTrue(TEXT("The first company can suggest growth after its positive capacity is filled"),
		CGRGrowthReadiness::IsFullAtPositiveCapacity(1, 1));
	TestFalse(TEXT("After upgrading, repeated floor practice waits until the new capacity is filled"),
		CGRGrowthReadiness::IsFullAtPositiveCapacity(1, 2));

	FGoalTable Repeated;
	Repeated.RepeatLimit = 3;
	Repeated.ConditionType = EMissionConditionType::RaiseBuildingFloor;
	Repeated.ConditionAmount = 1;
	Repeated.bCountActions = true;
	TestEqual(TEXT("Bulk purchase records every level"), GrowthGoalRules::AddProgress(0, 50), int64(50));
	TestEqual(TEXT("Negative actions cannot erase progress"), GrowthGoalRules::AddProgress(50, -10), int64(50));
	TestEqual(TEXT("Accumulation saturates"), GrowthGoalRules::AddProgress(MAX_int64 - 1, 50), MAX_int64);
	FGoalTable Once;
	TestTrue(TEXT("One-time reach guidance may credit existing progress"), GrowthGoalRules::ShouldEvaluateSnapshot(Once));
	TestFalse(TEXT("A loaded level cannot count as a repeated action"), GrowthGoalRules::ShouldEvaluateSnapshot(Repeated));
	TestTrue(TEXT("The first real action counts"), GrowthGoalRules::CanRecordSuccess(Repeated, 0));
	TestTrue(TEXT("The second real action counts"), GrowthGoalRules::CanRecordSuccess(Repeated, 1));
	TestTrue(TEXT("The third real action counts"), GrowthGoalRules::CanRecordSuccess(Repeated, 2));
	TestFalse(TEXT("Practice stops at the table cap"), GrowthGoalRules::CanRecordSuccess(Repeated, 3));
	TArray<FName> FirstSuccessIDs;
	TestFalse(TEXT("Prerequisite begins locked"), GrowthGoalRules::PrerequisiteMet(TEXT("LearnLevel"), FirstSuccessIDs));
	FirstSuccessIDs.Add(TEXT("LearnLevel"));
	TestTrue(TEXT("First success unlocks dependent guidance before mastery"),
		GrowthGoalRules::PrerequisiteMet(TEXT("LearnLevel"), FirstSuccessIDs));
	Repeated.RepeatLimit = 0;
	Repeated.ConditionAmount = 10;
	Repeated.RepeatTargetIncrement = 5;
	Repeated.RepeatTargetCap = 30;
	TestTrue(TEXT("Zero repeat limit supports sustained growth"), GrowthGoalRules::CanRecordSuccess(Repeated, 1000));
	TestEqual(TEXT("Initial target"), GrowthGoalRules::TargetForClaim(Repeated, 0), int64(10));
	TestEqual(TEXT("Next target grows"), GrowthGoalRules::TargetForClaim(Repeated, 1), int64(15));
	TestEqual(TEXT("Target cap"), GrowthGoalRules::TargetForClaim(Repeated, 1000), int64(30));
	Repeated.RepeatTargetCap = 0;
	Repeated.RepeatTargetIncrement = MAX_int64;
	TestEqual(TEXT("Large targets cannot wrap"), GrowthGoalRules::TargetForClaim(Repeated, MAX_int32), MAX_int64);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrowthGoalHQPreparationTest,
	"CGR.GoalBoard.Growth.HQPreparation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGrowthGoalHQPreparationTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("HQ2 can recommend reachable HQ3 while the purchase conditions remain unmet"),
		CGRGrowthReadiness::IsNextHQLevelGoal(2, 3, true));
	TestFalse(TEXT("A future HQ4 goal cannot displace the next HQ3 preparation"),
		CGRGrowthReadiness::IsNextHQLevelGoal(2, 4, true));
	TestFalse(TEXT("The current HQ2 goal is already behind the player"),
		CGRGrowthReadiness::IsNextHQLevelGoal(2, 2, true));
	TestFalse(TEXT("The last HQ level has no preparation goal"),
		CGRGrowthReadiness::IsNextHQLevelGoal(2, 3, false));

	UHQManagePanelWidget* Panel = NewObject<UHQManagePanelWidget>();
	UIconWithButtonWidget* GrowthTab = NewObject<UIconWithButtonWidget>(Panel);
	UCostActionButtonWidget* LevelUp = NewObject<UCostActionButtonWidget>(Panel);
	UUniformGridPanel* Conditions = NewObject<UUniformGridPanel>(Panel);
	FHQGrowthGuideTargetTestAccessor::SetState(*Panel, 0, false, GrowthTab, LevelUp, Conditions);
	bool bConditionsUnmet = false;
	TestTrue(TEXT("Unmet requirements lead to the existing condition cards"),
		Panel->GetGrowthGuideTargetWidget(&bConditionsUnmet) == Conditions);
	TestTrue(TEXT("Preparation is an observation step"), bConditionsUnmet);
	FHQGrowthGuideTargetTestAccessor::SetState(*Panel, 0, true, GrowthTab, LevelUp, Conditions);
	TestTrue(TEXT("Once ready, guidance returns to the actionable level-up button"),
		Panel->GetGrowthGuideTargetWidget(&bConditionsUnmet) == LevelUp);
	TestFalse(TEXT("Ready button remains an action step"), bConditionsUnmet);
	FHQGrowthGuideTargetTestAccessor::SetState(*Panel, 1, false, GrowthTab, LevelUp, Conditions);
	TestTrue(TEXT("Another tab still directs the player to the growth tab"),
		Panel->GetGrowthGuideTargetWidget(&bConditionsUnmet) == GrowthTab);
	TestFalse(TEXT("Opening the growth tab remains an action step"), bConditionsUnmet);
	return true;
}
#endif
