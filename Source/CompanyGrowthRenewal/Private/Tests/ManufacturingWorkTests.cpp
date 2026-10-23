#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Utils/ManufacturingWorkSimulator.h"
#include "Data/FatigueConfig.h"
#include "Data/GameSaveData.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
FManufacturingWorkerInput WorkInput()
{
    FManufacturingWorkerInput Input;
    Input.EmployeeId = 12;
    Input.BaseOutput = 10;
    Input.Affinities = {3, 2, 1};
    Input.Loadout.BasicCaps.Add(TEXT("EQF_WORK_RATE"), 100);
    Input.Loadout.BasicCaps.Add(TEXT("EQF_SCORE_GAIN"), 100);
    Input.Loadout.BasicCaps.Add(TEXT("EQF_FATIGUE_GAIN"), 70);
    Input.Loadout.Rules.Add(TEXT("GeneralCritCapPercent"), 30);
    Input.Loadout.Rules.Add(TEXT("SelfXPCapPercent"), 100);
    Input.Loadout.Rules.Add(TEXT("MentorXPCapPercent"), 100);
    Input.Loadout.Rules.Add(TEXT("InstantReliefCap"), 60);
    return Input;
}
double SumXP(const FManufacturingWorkResult& Result)
{
    double Sum = 0;
    for (const auto& Pair : Result.Experience) Sum += Pair.Value;
    return Sum;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FManufacturingWorkPartitionTest,
    "CGR.Manufacturing.Work.PartitionAndRestart", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FManufacturingWorkPartitionTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UFatigueConfig> Config(NewObject<UFatigueConfig>());
    TArray<FManufacturingWorkerInput> Roster = {WorkInput()};
    FManufacturingWorkState Whole;
    const auto WholeResult = FManufacturingWorkSimulator::Advance(Whole, 7, Roster, *Config, 1, 180.13);
    TStrongObjectPtr<USaveGame_GameData> Save(NewObject<USaveGame_GameData>());
    auto& Before = Save->GameData.OfficeDataMap.FindOrAdd(7).ManufacturingWork;
    const auto First = FManufacturingWorkSimulator::Advance(Before, 7, Roster, *Config, 1, 47.17);
    TArray<uint8> Bytes;
    if (!TestTrue(TEXT("Serialize partial work period"), UGameplayStatics::SaveGameToMemory(Save.Get(), Bytes))) return false;
    TStrongObjectPtr<USaveGame_GameData> Restored(Cast<USaveGame_GameData>(UGameplayStatics::LoadGameFromMemory(Bytes)));
    if (!TestNotNull(TEXT("Reload work period"), Restored.Get())) return false;
    auto& Split = Restored->GameData.OfficeDataMap.FindChecked(7).ManufacturingWork;
    const auto Second = FManufacturingWorkSimulator::Advance(Split, 7, Roster, *Config, 1, 132.96);
    for (int32 Field = 0; Field < 3; ++Field)
        TestTrue(TEXT("Field yield survives partition and reload"), FMath::IsNearlyEqual(WholeResult.Points[Field], First.Points[Field] + Second.Points[Field], .001));
    TestTrue(TEXT("Personal XP survives partition and reload"), FMath::IsNearlyEqual(SumXP(WholeResult), SumXP(First) + SumXP(Second), .001));
    TestTrue(TEXT("Actual work time survives partition and reload"), FMath::IsNearlyEqual(WholeResult.WorkedSeconds, First.WorkedSeconds + Second.WorkedSeconds, .001));
    TestEqual(TEXT("Same random state after reload"), Whole.RandomSeed, Split.RandomSeed);
    TestEqual(TEXT("Same finite period after reload"), Whole.Period, Split.Period);
    TestTrue(TEXT("Substep time is retained"), FMath::IsNearlyEqual(Whole.PendingSeconds, Split.PendingSeconds, 1.e-8));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FManufacturingWorkGrowthTest,
    "CGR.Manufacturing.Work.ExperienceAndNoIdleGrant", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FManufacturingWorkGrowthTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UFatigueConfig> Config(NewObject<UFatigueConfig>());
    Config->GainPerSec = 0;
    Config->BoltChancePerSec = 0;
    FManufacturingWorkState BaseState, ImprovedState, EmptyState;
    auto Base = WorkInput();
    auto Improved = Base;
    Improved.ExperienceMultiplier = 2;
    const auto BaseResult = FManufacturingWorkSimulator::Advance(BaseState, 7, {Base}, *Config, 1, 120);
    const auto ImprovedResult = FManufacturingWorkSimulator::Advance(ImprovedState, 7, {Improved}, *Config, 1, 120);
    const auto EmptyResult = FManufacturingWorkSimulator::Advance(EmptyState, 7, {}, *Config, 1, 120);
    TestTrue(TEXT("Actual work grants experience to the worker"), BaseResult.Experience.FindRef(Base.EmployeeId) > 0);
    TestTrue(TEXT("Resolved employee growth applies once"), FMath::IsNearlyEqual(SumXP(ImprovedResult), SumXP(BaseResult) * 2, .001));
    TestEqual(TEXT("No seated workers means no XP"), SumXP(EmptyResult), 0.);
    TestEqual(TEXT("No seated workers means no research work"), EmptyResult.WorkedSeconds, 0.);
    TestEqual(TEXT("Healthy work counts actual time"), BaseResult.WorkedSeconds, 120.);
    TestEqual(TEXT("Unassigned employee has no record"), EmptyState.Employees.Num(), 0);
    TestTrue(TEXT("Off-specialty fields also accrue"), BaseResult.Points[2] > 0);
    FManufacturingWorkState ContinuousState;
    auto SecondWorker = Base;
    SecondWorker.EmployeeId = 13;
    SecondWorker.ExperienceMultiplier = 2;
    const auto ShortWork = FManufacturingWorkSimulator::Advance(ContinuousState, 7, {Base, SecondWorker}, *Config, 1, 1);
    TestTrue(TEXT("XP is available before the equipment period ends"), ShortWork.Experience.FindRef(Base.EmployeeId) > 0);
    TestTrue(TEXT("Workers keep separate resolved grants"), FMath::IsNearlyEqual(
        ShortWork.Experience.FindRef(SecondWorker.EmployeeId), ShortWork.Experience.FindRef(Base.EmployeeId) * 2, .001));
    const auto RemainingWork = FManufacturingWorkSimulator::Advance(ContinuousState, 7, {Base}, *Config, 1, 119);
    TestTrue(TEXT("Period close never pays the baseline twice"), FMath::IsNearlyEqual(
        ShortWork.Experience.FindRef(Base.EmployeeId) + RemainingWork.Experience.FindRef(Base.EmployeeId),
        BaseResult.Experience.FindRef(Base.EmployeeId), .001));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FManufacturingWorkPeriodCarryTest,
    "CGR.Manufacturing.Work.PeriodKeepsCooldownAndRecovery", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FManufacturingWorkPeriodCarryTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UFatigueConfig> Config(NewObject<UFatigueConfig>());
    Config->GainPerSec = 0;
    Config->BoltChancePerSec = 0;
    auto Worker = WorkInput();
    FEmployeeEquipmentEffectRow Break;
    Break.EffectId = TEXT("EQF_BREATHING_BREAK");
    Break.ScalarParameters.Add(TEXT("Threshold"), 65);
    Break.ScalarParameters.Add(TEXT("RearmBelow"), 40);
    Worker.Loadout.UniqueEffects.Add(Break.EffectId, Break);
    FEmployeeEquipmentEffectRow Shield;
    Shield.EffectId = TEXT("EQF_RECOVERY_SHIELD");
    Worker.Loadout.UniqueEffects.Add(Shield.EffectId, Shield);
    FManufacturingWorkState State;
    FManufacturingWorkSimulator::Advance(State, 7, {Worker}, *Config, 1, 59.75);
    auto& Runtime = State.Equipment.Employees.FindChecked(Worker.EmployeeId);
    Runtime.bRestArmed = false;
    Runtime.Effects.FindChecked(Break.EffectId).CooldownUntil = 80;
    Runtime.Effects.FindChecked(Shield.EffectId).ExpiresAt = 70;
    State.Employees.FindChecked(Worker.EmployeeId).Fatigue = 60;
    FManufacturingWorkSimulator::Advance(State, 7, {Worker}, *Config, 1, .25);
    const auto& Next = State.Equipment.Employees.FindChecked(Worker.EmployeeId);
    TestEqual(TEXT("Finite work period rolls forward"), State.Period, 2);
    TestFalse(TEXT("Rearm threshold is not bypassed by rollover"), Next.bRestArmed);
    TestEqual(TEXT("Remaining cooldown survives rollover"), Next.Effects.FindChecked(Break.EffectId).CooldownUntil, 20.);
    TestEqual(TEXT("Remaining recovery shield survives rollover"), Next.Effects.FindChecked(Shield.EffectId).ExpiresAt, 10.);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FManufacturingResearchWorkSwitchTest,
    "CGR.Manufacturing.Work.ResearchSwitch", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FManufacturingResearchWorkSwitchTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UFatigueConfig> Config(NewObject<UFatigueConfig>());
    Config->GainPerSec = 0;
    Config->BoltChancePerSec = 0;
    const TArray<FManufacturingWorkerInput> Roster = {WorkInput()};
    FManufacturingWorkState WholeWork;
    FManufacturingResearchState WholeResearch;
    WholeResearch.ProjectIndex = 1;
    FManufacturingResearchSimulation WholeContext{&WholeResearch, {5, 4, 3}, {5, 4, 3}, 2.0};
    const auto Whole = FManufacturingWorkSimulator::Advance(WholeWork, 7, Roster, *Config, 1, 10.13, &WholeContext);
    TestEqual(TEXT("Research completes once"), Whole.CompletedResearchProjectIndex, 1);
    TestEqual(TEXT("Research state is cleared"), WholeResearch.ProjectIndex, INDEX_NONE);
    TestTrue(TEXT("Personal XP continues through research"), SumXP(Whole) > 0);
    TestTrue(TEXT("Remaining time earns points"), Whole.Points[0] + Whole.Points[1] + Whole.Points[2] > 0);

    FManufacturingWorkState SplitWork;
    FManufacturingResearchState SplitResearch;
    SplitResearch.ProjectIndex = 1;
    FManufacturingResearchSimulation SplitContext{&SplitResearch, {5, 4, 3}, {5, 4, 3}, 2.0};
    const auto First = FManufacturingWorkSimulator::Advance(SplitWork, 7, Roster, *Config, 1, 1.13, &SplitContext);
    TestEqual(TEXT("Research does not create spendable points"), First.Points[0] + First.Points[1] + First.Points[2], 0.);
    TestTrue(TEXT("Research contributions still drive equipment period progress"),
        SplitWork.PeriodPoints[0] + SplitWork.PeriodPoints[1] + SplitWork.PeriodPoints[2] > 0.);
    TestTrue(TEXT("Research advances on actual work"), SplitResearch.CompletedWorkSeconds > 0);
    TestTrue(TEXT("Research still pays personal XP"), SumXP(First) > 0);
    TStrongObjectPtr<USaveGame_GameData> Save(NewObject<USaveGame_GameData>());
    auto& Office = Save->GameData.OfficeDataMap.FindOrAdd(7);
    Office.ManufacturingWork = SplitWork;
    Office.ManufacturingResearch = SplitResearch;
    TArray<uint8> Bytes;
    if (!TestTrue(TEXT("Serialize active research"), UGameplayStatics::SaveGameToMemory(Save.Get(), Bytes))) return false;
    TStrongObjectPtr<USaveGame_GameData> Loaded(Cast<USaveGame_GameData>(UGameplayStatics::LoadGameFromMemory(Bytes)));
    if (!TestNotNull(TEXT("Reload active research"), Loaded.Get())) return false;
    auto& Restored = Loaded->GameData.OfficeDataMap.FindChecked(7);
    FManufacturingResearchSimulation RestoredContext{&Restored.ManufacturingResearch, {5, 4, 3}, {5, 4, 3}, 2.0};
    const auto Second = FManufacturingWorkSimulator::Advance(Restored.ManufacturingWork, 7, Roster, *Config, 1, 9., &RestoredContext);
    TestEqual(TEXT("Reloaded research finishes once"), Second.CompletedResearchProjectIndex, 1);
    for (int32 Field = 0; Field < 3; ++Field)
        TestTrue(TEXT("Long and split settlement earn equal points"),
            FMath::IsNearlyEqual(Whole.Points[Field], First.Points[Field] + Second.Points[Field], .001));
    TestTrue(TEXT("Long and split settlement earn equal XP"), FMath::IsNearlyEqual(SumXP(Whole), SumXP(First) + SumXP(Second), .001));

    FManufacturingWorkState PausedWork;
    FManufacturingResearchState PausedResearch;
    PausedResearch.ProjectIndex = 2;
    FManufacturingResearchSimulation PausedContext{&PausedResearch, {5, 4, 2}, {5, 4, 3}, 2.0};
    const auto Paused = FManufacturingWorkSimulator::Advance(PausedWork, 8, Roster, *Config, 1, 4., &PausedContext);
    TestEqual(TEXT("Capability pause suppresses points"), Paused.Points[0] + Paused.Points[1] + Paused.Points[2], 0.);
    TestEqual(TEXT("Capability pause holds research"), PausedResearch.CompletedWorkSeconds, 0.);
    TestTrue(TEXT("Capability pause keeps XP and work"), SumXP(Paused) > 0 && Paused.WorkedSeconds > 0);

    FManufacturingWorkState RestWork;
    auto& Resting = RestWork.Employees.FindOrAdd(Roster[0].EmployeeId);
    Resting.Rest = EManufacturingRest::Breathing;
    Resting.RestRemaining = 1.0;
    FManufacturingResearchState RestResearch;
    RestResearch.ProjectIndex = 3;
    FManufacturingResearchSimulation RestContext{&RestResearch, {5, 4, 3}, {5, 4, 3}, 2.0};
    const auto RestResult = FManufacturingWorkSimulator::Advance(RestWork, 9, Roster, *Config, 1, 2., &RestContext);
    TestEqual(TEXT("Rest keeps research active"), RestResearch.ProjectIndex, 3);
    TestTrue(TEXT("Rest reduces research work"), RestResearch.CompletedWorkSeconds > 0. && RestResearch.CompletedWorkSeconds < 2.);
    TestEqual(TEXT("Resting research earns no points"), RestResult.Points[0] + RestResult.Points[1] + RestResult.Points[2], 0.);
    return true;
}
#endif
