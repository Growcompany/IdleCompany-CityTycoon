#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Data/FatigueConfig.h"
#include "Utils/ManufacturingRecoveryRules.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
FManufacturingWorkerInput Worker(int32 EmployeeId = 1)
{
    FManufacturingWorkerInput Input;
    Input.EmployeeId = EmployeeId;
    return Input;
}

void AddEffect(FEquipmentEffectLoadout& Loadout, const TCHAR* Id,
    std::initializer_list<TPair<FName, float>> Values)
{
    FEmployeeEquipmentEffectRow Row;
    Row.EffectId = Id;
    for (const auto& Value : Values) Row.ScalarParameters.Add(Value.Key, Value.Value);
    Loadout.UniqueEffects.Add(Row.EffectId, Row);
}

FManufacturingWorkState Initialized(const FManufacturingWorkerInput& Input)
{
    FManufacturingWorkState State;
    State.Equipment.AttemptId = FGuid::NewGuid();
    auto& Runtime = State.Equipment.Employees.Add(Input.EmployeeId);
    Runtime.EmployeeId = Input.EmployeeId;
    Runtime.bStartParticipant = true;
    FEmployeeEquipmentEffectEvaluator::SyncLoadout(Runtime, Input.Loadout, true);
    State.Employees.Add(Input.EmployeeId);
    return State;
}

bool Step(FManufacturingWorkState& State, const FManufacturingWorkerInput& Input,
    const UFatigueConfig& Config, float Roll = 1.f)
{
    FEquipmentParticipant Participant;
    Participant.EmployeeId = Input.EmployeeId;
    Participant.bSeated = true;
    Participant.bTyping = State.Employees.FindRef(Input.EmployeeId).Rest == EManufacturingRest::None;
    Participant.Fatigue = State.Employees.FindRef(Input.EmployeeId).Fatigue;
    Participant.Loadout = Input.Loadout;
    TArray<FEquipmentParticipant> Participants{Participant};
    FEmployeeEquipmentEffectEvaluator::AdvanceTime(State.Equipment, .25f, Participants);
    return FManufacturingRecoveryRules::Advance(State, Input, Config, .25f, Roll, Participants);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FManufacturingRecoveryAutomaticTest,
    "CGR.Manufacturing.Recovery.AutomaticReturn",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FManufacturingRecoveryAutomaticTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UFatigueConfig> Config(NewObject<UFatigueConfig>());
    Config->BoltChancePerSec = 0;
    Config->SlackTelegraphDuration = .5f;
    Config->SlackSlumpDuration = 1.f;
    auto Input = Worker();
    auto State = Initialized(Input);
    State.Employees.FindChecked(1).Fatigue = Config->SlackingThreshold;
    TestFalse(TEXT("Threshold starts a bounded rest"), Step(State, Input, *Config));
    TestEqual(TEXT("Drowsy state saved"), State.Employees.FindChecked(1).Rest, EManufacturingRest::Drowsy);
    for (int32 Index = 0; Index < 6; ++Index) Step(State, Input, *Config);
    TestEqual(TEXT("No input needed to return"), State.Employees.FindChecked(1).Rest, EManufacturingRest::None);
    TestTrue(TEXT("Worker contributes after automatic return"), Step(State, Input, *Config));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FManufacturingRecoveryStatsTest,
    "CGR.Manufacturing.Recovery.Stats",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FManufacturingRecoveryStatsTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UFatigueConfig> Config(NewObject<UFatigueConfig>());
    Config->BoltChancePerSec = .08f;
    Config->GainPerSec = 8.f;
    auto Baseline = Worker();
    auto Resilient = Worker();
    Resilient.Stamina = 100;
    Resilient.Environment = 1;
    auto Low = Initialized(Baseline);
    auto High = Initialized(Resilient);
    TestTrue(TEXT("Low stamina works"), Step(Low, Baseline, *Config));
    TestTrue(TEXT("High stamina works"), Step(High, Resilient, *Config));
    TestTrue(TEXT("Stamina and Environment reduce fatigue gain"),
        High.Employees.FindChecked(1).Fatigue < Low.Employees.FindChecked(1).Fatigue);

    const float FixedRoll = .01f;
    auto Agitated = Initialized(Baseline);
    auto Calm = Worker(); Calm.Composure = 140;
    auto Composed = Initialized(Calm);
    TestFalse(TEXT("Low composure bolted on fixed roll"), Step(Agitated, Baseline, *Config, FixedRoll));
    TestEqual(TEXT("Bolt starts away rest"), Agitated.Employees.FindChecked(1).Rest, EManufacturingRest::Away);
    TestTrue(TEXT("High composure resists same roll"), Step(Composed, Calm, *Config, FixedRoll));

    auto FastRecovery = Worker(); FastRecovery.RecoveryMultiplier = 2.f;
    auto SlowRest = Initialized(Baseline);
    auto FastRest = Initialized(FastRecovery);
    SlowRest.Employees.FindChecked(1).Fatigue = 80.f;
    FastRest.Employees.FindChecked(1).Fatigue = 80.f;
    Step(SlowRest, Baseline, *Config, 0.f);
    Step(FastRest, FastRecovery, *Config, 0.f);
    Step(SlowRest, Baseline, *Config);
    Step(FastRest, FastRecovery, *Config);
    TestTrue(TEXT("Welfare recovery multiplier reduces fatigue while away"),
        FastRest.Employees.FindChecked(1).Fatigue < SlowRest.Employees.FindChecked(1).Fatigue);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FManufacturingRecoveryEquipmentTest,
    "CGR.Manufacturing.Recovery.EquipmentAndLoadout",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FManufacturingRecoveryEquipmentTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UFatigueConfig> Config(NewObject<UFatigueConfig>());
    Config->BoltChancePerSec = 0;
    Config->SlackTelegraphDuration = 2;
    Config->SlackSlumpDuration = 2;
    auto Input = Worker();
    Input.Loadout.BasicValues.Add(TEXT("EQF_DROWSY_RECOVERY"), 50);
    Input.Loadout.BasicValues.Add(TEXT("EQF_TAP_RELIEF"), 10);
    Input.Loadout.Rules.Add(TEXT("InstantReliefCap"), 100);
    AddEffect(Input.Loadout, TEXT("EQF_EARLY_CATCH"), {{TEXT("Window"), 1}, {TEXT("Relief"), 15}, {TEXT("ICD"), 8}});
    AddEffect(Input.Loadout, TEXT("EQF_CATCH_LEARNING"), {});
    auto State = Initialized(Input);
    State.Employees.FindChecked(1).Fatigue = 100;
    Step(State, Input, *Config);
    const double Before = State.Employees.FindChecked(1).RestRemaining;
    const float SavedRecoveryRate = State.Employees.FindChecked(1).RestRecoveryRate;
    TestTrue(TEXT("Basic return duration is shortened"), Before < 4.0);
    Input.Loadout.UniqueEffects.Remove(TEXT("EQF_EARLY_CATCH"));
    Input.Loadout.BasicValues.Add(TEXT("EQF_DROWSY_RECOVERY"), 90);
    Step(State, Input, *Config);
    TestTrue(TEXT("Changing loadout does not reset rest clock"), State.Employees.FindChecked(1).RestRemaining < Before);
    TestEqual(TEXT("Changing duration equipment does not grant a new recovery budget"),
        State.Employees.FindChecked(1).RestRecoveryRate, SavedRecoveryRate);
    Input.Loadout.BasicValues.Add(TEXT("EQF_DROWSY_RECOVERY"), 50);
    AddEffect(Input.Loadout, TEXT("EQF_EARLY_CATCH"), {{TEXT("Window"), 1}, {TEXT("Relief"), 15}, {TEXT("ICD"), 8}});
    for (int32 Index = 0; Index < 3; ++Index) Step(State, Input, *Config);
    TestEqual(TEXT("Early catch returns automatically"), State.Employees.FindChecked(1).Rest, EManufacturingRest::None);
    TestTrue(TEXT("Return applies tap and catch relief"), State.Employees.FindChecked(1).Fatigue <= 15.f);
    TestEqual(TEXT("Catch learning receives one event"),
        State.Equipment.Employees.FindChecked(1).Effects.FindChecked(TEXT("EQF_CATCH_LEARNING")).CatchCount, 1);

    auto LateInput = Input;
    LateInput.Loadout.UniqueEffects.Remove(TEXT("EQF_EARLY_CATCH"));
    auto LateState = Initialized(LateInput);
    LateState.Employees.FindChecked(1).Fatigue = 100;
    Step(LateState, LateInput, *Config);
    for (int32 Index = 0; Index < 5; ++Index) Step(LateState, LateInput, *Config);
    LateInput.Loadout = Input.Loadout;
    Step(LateState, LateInput, *Config);
    TestEqual(TEXT("Equipping EarlyCatch after its real window does not return early"),
        LateState.Employees.FindChecked(1).Rest, EManufacturingRest::Drowsy);

    auto BreakInput = Worker();
    AddEffect(BreakInput.Loadout, TEXT("EQF_BREATHING_BREAK"),
        {{TEXT("Threshold"), 80}, {TEXT("RearmBelow"), 40}, {TEXT("Duration"), .5f},
         {TEXT("ICD"), 2}, {TEXT("Relief"), 35}});
    BreakInput.Loadout.Rules.Add(TEXT("InstantReliefCap"), 100);
    auto BreakState = Initialized(BreakInput);
    BreakState.Employees.FindChecked(1).Fatigue = 80;
    Step(BreakState, BreakInput, *Config);
    TestTrue(TEXT("Breathing creates an equipment rest deadline"),
        BreakState.Equipment.Employees.FindChecked(1).RestUntil > 0);
    Step(BreakState, BreakInput, *Config);
    Step(BreakState, BreakInput, *Config);
    TestEqual(TEXT("Breathing completion clears equipment rest deadline"),
        BreakState.Equipment.Employees.FindChecked(1).RestUntil, 0.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FManufacturingRecoveryDurationValueTest,
    "CGR.Manufacturing.Recovery.ShorterRestImprovesWork",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FManufacturingRecoveryDurationValueTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UFatigueConfig> Config(NewObject<UFatigueConfig>());
    Config->BoltChancePerSec = 0;
    Config->SlackTelegraphDuration = 3;
    Config->SlackSlumpDuration = 6;
    Config->GainPerSec = 12;
    Config->SlumpRecoverPerSec = 10;
    auto Base = Worker();
    auto Quick = Worker();
    Quick.Loadout.BasicValues.Add(TEXT("EQF_DROWSY_RECOVERY"), 50);
    Quick.Loadout.Rules.Add(TEXT("DrowsyMinimumDuration"), 2);
    auto BaseState = Initialized(Base);
    auto QuickState = Initialized(Quick);
    BaseState.Employees.FindChecked(1).Fatigue = 100;
    QuickState.Employees.FindChecked(1).Fatigue = 100;
    Step(BaseState, Base, *Config);
    Step(QuickState, Quick, *Config);
    TestTrue(TEXT("Shorter rest stores a faster recovery rate"),
        QuickState.Employees.FindChecked(1).RestRecoveryRate > BaseState.Employees.FindChecked(1).RestRecoveryRate);
    for (int32 Index = 0; Index < 36; ++Index) Step(BaseState, Base, *Config);
    for (int32 Index = 0; Index < 24; ++Index) Step(QuickState, Quick, *Config);
    TestEqual(TEXT("Both natural returns preserve the same fatigue relief"),
        QuickState.Employees.FindChecked(1).Fatigue, BaseState.Employees.FindChecked(1).Fatigue);

    int32 BaseWorking = 0;
    int32 QuickWorking = 0;
    for (int32 Index = 0; Index < 240; ++Index)
    {
        BaseWorking += Step(BaseState, Base, *Config) ? 1 : 0;
        QuickWorking += Step(QuickState, Quick, *Config) ? 1 : 0;
    }
    TestTrue(TEXT("Shorter rest yields more productive fixed steps over a minute"), QuickWorking > BaseWorking);

    Config->BoltChancePerSec = 1;
    Config->BoltDuration = 6;
    auto SlowBolt = Worker();
    auto FastBolt = Worker();
    FastBolt.Loadout.BasicValues.Add(TEXT("EQF_BOLT_RETURN"), 50);
    FastBolt.Loadout.Rules.Add(TEXT("BoltMinimumDuration"), 2);
    auto SlowBoltState = Initialized(SlowBolt);
    auto FastBoltState = Initialized(FastBolt);
    SlowBoltState.Employees.FindChecked(1).Fatigue = 80;
    FastBoltState.Employees.FindChecked(1).Fatigue = 80;
    Step(SlowBoltState, SlowBolt, *Config, 0.f);
    Step(FastBoltState, FastBolt, *Config, 0.f);
    for (int32 Index = 0; Index < 24; ++Index) Step(SlowBoltState, SlowBolt, *Config);
    for (int32 Index = 0; Index < 12; ++Index) Step(FastBoltState, FastBolt, *Config);
    TestEqual(TEXT("Bolt return shortening keeps the natural fatigue recovery budget"),
        FastBoltState.Employees.FindChecked(1).Fatigue, SlowBoltState.Employees.FindChecked(1).Fatigue);
    return true;
}

#endif
