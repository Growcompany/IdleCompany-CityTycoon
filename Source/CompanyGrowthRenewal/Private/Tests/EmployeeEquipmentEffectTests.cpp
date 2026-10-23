#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Utils/EmployeeEquipmentEffectEvaluator.h"
#include "UI/Element/Employee/EmployeeEquipmentDetailBuilder.h"
#include "Manager/OfficeStageProgressManager.h"
#include "GameMode/OfficeGameMode.h"
#include "Entity/Officeworker/Officeworker.h"
#include "Entity/Officeworker/EmployeeBehaviorComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEquipmentEffectValueDisplayTest,
    "CompanyGrowth.Equipment.Effects.DisplayValueDirections", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEquipmentEffectValueDisplayTest::RunTest(const FString& Parameters)
{
    FEmployeeEquipmentEffectRow Effect;
    Effect.Unit = FText::FromString(TEXT("%"));
    Effect.ValueDisplay = EEquipmentEffectValueDisplay::SpeedToInterval;
    TestEqual(TEXT("Three speed converts to shorter interval"), CGEquipmentDetail::FormatEffectValue(Effect, 3.f), FString(TEXT("-2.91%")));
    TestEqual(TEXT("Fifty speed converts to one third shorter interval"), CGEquipmentDetail::FormatEffectValue(Effect, 50.f), FString(TEXT("-33.33%")));
    TestEqual(TEXT("Additive six speed is converted once"), CGEquipmentDetail::FormatEffectValue(Effect, 3.f + 3.f), FString(TEXT("-5.66%")));
    TestEqual(TEXT("Negligible reduction never displays negative zero"), CGEquipmentDetail::FormatEffectValue(Effect, .001f), FString(TEXT("0%")));
    Effect.ValueDisplay = EEquipmentEffectValueDisplay::Decrease;
    TestEqual(TEXT("Reduction displays a minus sign"), CGEquipmentDetail::FormatEffectValue(Effect, 10.f), FString(TEXT("-10%")));
    Effect.ValueDisplay = EEquipmentEffectValueDisplay::Increase;
    Effect.Unit = FText::FromString(TEXT("%p"));
    TestEqual(TEXT("Critical chance increase keeps percentage point unit"), CGEquipmentDetail::FormatEffectValue(Effect, .5f), FString(TEXT("+0.5%p")));
    TestEqual(TEXT("Zero has no sign"), CGEquipmentDetail::FormatEffectValue(Effect, 0.f), FString(TEXT("0%p")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEquipmentWorkerOwnershipTest,
    "CompanyGrowth.Equipment.Effects.WorkerCacheRejectsPreviewAndDuplicateIdentity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEquipmentWorkerOwnershipTest::RunTest(const FString& Parameters)
{
    UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Transient test world"), TestWorld)) return false;
    if (GEngine) GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(TestWorld);
    ON_SCOPE_EXIT
    {
        if (GEngine) GEngine->DestroyWorldContext(TestWorld);
        TestWorld->DestroyWorld(false);
    };
    AOfficeGameMode* OfficeMode = TestWorld->SpawnActor<AOfficeGameMode>();
    if (!TestNotNull(TEXT("Office game mode"), OfficeMode)) return false;
    FObjectPropertyBase* AuthorityProperty = FindFProperty<FObjectPropertyBase>(UWorld::StaticClass(), TEXT("AuthorityGameMode"));
    FArrayProperty* EmployeesProperty = FindFProperty<FArrayProperty>(AOfficeGameMode::StaticClass(), TEXT("SpawnedEmployees"));
    if (!TestNotNull(TEXT("World authority property"), AuthorityProperty) || !TestNotNull(TEXT("Registered employee property"), EmployeesProperty)) return false;
    AuthorityProperty->SetObjectPropertyValue_InContainer(TestWorld, OfficeMode);
    auto* RegisteredEmployees = EmployeesProperty->ContainerPtrToValuePtr<TArray<AOfficeworker*>>(OfficeMode);
    AOfficeworker* ActualWorker = TestWorld->SpawnActor<AOfficeworker>();
    AOfficeworker* PreviewWorker = TestWorld->SpawnActor<AOfficeworker>();
    AOfficeworker* UnregisteredWorker = TestWorld->SpawnActor<AOfficeworker>();
    if (!TestNotNull(TEXT("Actual worker"), ActualWorker) || !TestNotNull(TEXT("Preview worker"), PreviewWorker) || !TestNotNull(TEXT("Unregistered worker"), UnregisteredWorker)) return false;
    ActualWorker->SetEmployeeID(42);
    PreviewWorker->SetEmployeeID(42);
    PreviewWorker->bIsPortraitMode = false;
    PreviewWorker->BehaviorComponent->bSkipAutoInit = true;
    UnregisteredWorker->SetEmployeeID(43);
    RegisteredEmployees->Add(ActualWorker);
    UOfficeStageProgressManager* Stage = NewObject<UOfficeStageProgressManager>(TestWorld);
    Stage->RefreshEquipmentWorkerCache();
    TestEqual(TEXT("Same-ID preview cannot replace the registered employee"), Stage->EquipmentWorkerCache.FindRef(42).Get(), ActualWorker);
    TestFalse(TEXT("Unregistered worker never receives equipment effects"), Stage->EquipmentWorkerCache.Contains(43));
    Stage->HandleEquipmentActorSpawned(PreviewWorker);
    Stage->RefreshEquipmentWorkerCache();
    TestEqual(TEXT("Preview spawn invalidation preserves the authoritative worker"), Stage->EquipmentWorkerCache.FindRef(42).Get(), ActualWorker);
    RegisteredEmployees->Reset();
    Stage->HandleEquipmentActorSpawned(PreviewWorker);
    Stage->RefreshEquipmentWorkerCache();
    TestEqual(TEXT("An unregistered same-ID actor cannot fill the departed employee's cache slot"), Stage->EquipmentWorkerCache.Num(), 0);
    return true;
}

namespace
{
void AddUnique(FEquipmentEffectLoadout& Loadout, const TCHAR* Id, std::initializer_list<TPair<FName, float>> Values)
{
    FEmployeeEquipmentEffectRow Row;
    Row.EffectId = Id;
    for (const auto& Value : Values) Row.ScalarParameters.Add(Value.Key, Value.Value);
    Loadout.UniqueEffects.Add(Row.EffectId, Row);
}
FEquipmentAttemptRuntime MakeAttempt(int32 Id, const FEquipmentEffectLoadout& Loadout)
{
    FEquipmentAttemptRuntime Attempt; Attempt.AttemptId = FGuid::NewGuid(); Attempt.SegmentId = 1; Attempt.SegmentDuration = 30;
    auto& Employee = Attempt.Employees.Add(Id); Employee.EmployeeId = Id; Employee.StartLevel = 1; Employee.bStartParticipant = true;
    FEmployeeEquipmentEffectEvaluator::SyncLoadout(Employee, Loadout, true);
    return Attempt;
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEquipmentBasicCompositionTest,
    "CompanyGrowth.Equipment.Effects.PrimaryEnhancementAndCaps", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEquipmentBasicCompositionTest::RunTest(const FString& Parameters)
{
    TMap<FName, FEmployeeEquipmentEffectRow> Effects;
    FEmployeeEquipmentEffectRow Rate;
    Rate.EffectId = TEXT("EQF_WORK_RATE"); Rate.bBasic = true;
    Rate.GradeValues = {3,4,5,6,8,10}; Rate.AggregateCap = 100;
    Effects.Add(Rate.EffectId, Rate);
    FEmployeeEquipmentDefinitionRow Def;
    Def.Grade = EEmployeeEquipmentGrade::Ascendant; Def.PrimaryEffectId = Rate.EffectId;
    FEmployeeEquipmentEconomyRow Economy;
    Economy.MaxEnhanceLevel = 5; Economy.PrimaryEnhancePerLevel = .03f; Economy.SecondaryScale = .5f;
    const auto Resolved = FEmployeeEquipmentEffectEvaluator::BuildItemEffects(Def, 5, Effects, Economy);
    TestEqual(TEXT("Only primary receives +15% magnitude"), Resolved.BasicValues.FindRef(Rate.EffectId), 11.5f);
    Def.PrimaryEffectId = NAME_None; Def.SecondaryEffectId = Rate.EffectId;
    TestEqual(TEXT("Secondary does not get enhancement"), FEmployeeEquipmentEffectEvaluator::BuildItemEffects(Def, 5, Effects, Economy).BasicValues.FindRef(Rate.EffectId), 5.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEquipmentAtomicContributionTest,
    "CompanyGrowth.Equipment.Effects.RejectedBatchHasNoPartialScore", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEquipmentAtomicContributionTest::RunTest(const FString& Parameters)
{
    TArray<FEquipmentDisciplineState> Disciplines;
    Disciplines.Add({0, 90.f, 100.f, 50.f}); Disciplines.Add({1, 0.f, 100.f, 50.f});
    TArray<FEquipmentContributionPacket> Packets;
    Packets.Add({0, 20.f, false}); Packets.Add({5, 10.f, true});
    TestFalse(TEXT("Inactive destination rejects entire batch"), FEmployeeEquipmentEffectEvaluator::TryApplyPackets(Disciplines, Packets, 2.f));
    TestEqual(TEXT("First valid packet is not applied before invalid second"), Disciplines[0].Acquired, 90.f);
    Packets[1].DisciplineSlot = 1;
    TestTrue(TEXT("Valid batch commits"), FEmployeeEquipmentEffectEvaluator::TryApplyPackets(Disciplines, Packets, 2.f));
    TestEqual(TEXT("ProjectYield applied once to primary"), Disciplines[0].Acquired, 130.f);
    TestEqual(TEXT("ProjectYield applied once to bonus"), Disciplines[1].Acquired, 20.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEquipmentExposureTest,
    "CompanyGrowth.Equipment.Effects.LateEquipAndReceiptIdempotency", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEquipmentExposureTest::RunTest(const FString& Parameters)
{
    FEquipmentAttemptRuntime Attempt;
    Attempt.AttemptId = FGuid::NewGuid(); Attempt.SegmentId = 1; Attempt.ActiveTime = 30;
    FEquipmentEmployeeRuntime& Employee = Attempt.Employees.Add(7);
    Employee.EmployeeId = 7;
    FEquipmentEffectRuntime& Effect = Employee.Effects.Add(TEXT("EQF_EXP_GAIN"));
    Effect.SegmentExposure = 1; Effect.SegmentValueSeconds = 10;
    FEquipmentXPGrant Grant;
    TestTrue(TEXT("First baseline receipt accepted"), FEmployeeEquipmentEffectEvaluator::RecordBaselineXP(Attempt, 7, 1, 75, 50, Grant));
    TestEqual(TEXT("One second of +10% over 30 seconds yields .25 XP"), Grant.EquipmentAmount, .25f);
    TestFalse(TEXT("Same segment/bucket receipt cannot pay twice"), FEmployeeEquipmentEffectEvaluator::RecordBaselineXP(Attempt, 7, 1, 75, 50, Grant));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEquipmentDownPriorityTest,
    "CompanyGrowth.Equipment.Effects.ImmunityBeforeFinitePersonalAndTeamGuards", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEquipmentDownPriorityTest::RunTest(const FString& Parameters)
{
    FEquipmentEffectLoadout Loadout;
    Loadout.Rules.Add(TEXT("InstantReliefCap"), 40);
    AddUnique(Loadout, TEXT("EQF_BOLT_IMMUNE"), {{TEXT("Enabled"),1}});
    AddUnique(Loadout, TEXT("EQF_BOLT_GUARD"), {{TEXT("Charges"),1}});
    AddUnique(Loadout, TEXT("EQF_DROWSY_GUARD"), {{TEXT("Charges"),1},{TEXT("Relief"),40}});
    auto Attempt = MakeAttempt(1, Loadout);
    float Relief = 0; TArray<FEquipmentParticipant> Participants;
    TestTrue(TEXT("Permanent immunity vetoes bolt"), FEmployeeEquipmentEffectEvaluator::TryVetoDown(Attempt, 1, true, Loadout, Participants, Relief));
    TestEqual(TEXT("Immunity did not consume personal guard"), Attempt.Employees[1].Effects[TEXT("EQF_BOLT_GUARD")].Charges, 1);
    Loadout.UniqueEffects.Remove(TEXT("EQF_BOLT_IMMUNE"));
    FEmployeeEquipmentEffectEvaluator::SyncLoadout(Attempt.Employees[1], Loadout, false);
    TestTrue(TEXT("First unprotected bolt consumes guard"), FEmployeeEquipmentEffectEvaluator::TryVetoDown(Attempt, 1, true, Loadout, Participants, Relief));
    FEmployeeEquipmentEffectEvaluator::SyncLoadout(Attempt.Employees[1], FEquipmentEffectLoadout(), false);
    FEmployeeEquipmentEffectEvaluator::SyncLoadout(Attempt.Employees[1], Loadout, false);
    TestFalse(TEXT("Reequip cannot mint a second first guard"), FEmployeeEquipmentEffectEvaluator::TryVetoDown(Attempt, 1, true, Loadout, Participants, Relief));
    TestTrue(TEXT("Drowsy guard is independent of bolt guard"), FEmployeeEquipmentEffectEvaluator::TryVetoDown(Attempt, 1, false, Loadout, Participants, Relief));
    TestEqual(TEXT("Drowsy guard actually relieves 40"), Relief, 40.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEquipmentNativeCommitTest,
    "CompanyGrowth.Equipment.Effects.FirstCritCommitsOnceAndCannotReissue", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEquipmentNativeCommitTest::RunTest(const FString& Parameters)
{
    FEquipmentEffectLoadout Loadout;
    Loadout.Rules.Add(TEXT("GuaranteedGap"), 6);
    AddUnique(Loadout,TEXT("EQF_FIRST_CRIT"),{{TEXT("Charges"),1}});
    AddUnique(Loadout,TEXT("EQF_COVERAGE_PLEDGE"),{{TEXT("Enabled"),1}});
    auto Attempt = MakeAttempt(1, Loadout);
    FEquipmentContributionContext Context;
    Context.RootEventId = FGuid::NewGuid(); Context.EmployeeId = 1; Context.ChosenSlot = 1; Context.bCritical = true;
    Context.GuaranteedEffect = FEmployeeEquipmentEffectEvaluator::GuaranteedCrit(Attempt,1,Loadout);
    TestEqual(TEXT("First native has guaranteed token"), Context.GuaranteedEffect, FName(TEXT("EQF_FIRST_CRIT")));
    TestEqual(TEXT("Planning did not consume token"), FEmployeeEquipmentEffectEvaluator::GuaranteedCrit(Attempt,1,Loadout), Context.GuaranteedEffect);
    TArray<FEquipmentReliefAction> Relief;
    FEmployeeEquipmentEffectEvaluator::CommitNative(Attempt,Context,Loadout,Relief);
    FEmployeeEquipmentEffectEvaluator::CommitNative(Attempt,Context,Loadout,Relief);
    TestEqual(TEXT("Duplicate root is still one normal contribution"), Attempt.Employees[1].NativeCount, 1);
    TestTrue(TEXT("Consumed first crit no longer guaranteed"), FEmployeeEquipmentEffectEvaluator::GuaranteedCrit(Attempt,1,Loadout).IsNone());
    TestEqual(TEXT("Coverage counts actual manual/ordinary target too"), Attempt.Employees[1].Effects[TEXT("EQF_COVERAGE_PLEDGE")].VisitedMask, 2);
    FEquipmentEffectLoadout Empty;
    auto LateAttempt = MakeAttempt(2,Empty);
    FEmployeeEquipmentEffectEvaluator::SyncLoadout(LateAttempt.Employees[2],Loadout,false);
    TestTrue(TEXT("Mid-attempt equip does not mint first crit"), FEmployeeEquipmentEffectEvaluator::GuaranteedCrit(LateAttempt,2,Loadout).IsNone());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEquipmentPacketRoutingTest,
    "CompanyGrowth.Equipment.Effects.TransfersConserveAndBonusesShareNativeBudget", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEquipmentPacketRoutingTest::RunTest(const FString& Parameters)
{
    FEquipmentEffectLoadout Loadout;
    AddUnique(Loadout,TEXT("EQF_OVERFLOW_RELAY"),{{TEXT("TransferPercent"),30}});
    auto Attempt = MakeAttempt(1,Loadout);
    FEquipmentContributionContext Context;
    Context.EmployeeId=1; Context.ChosenSlot=0; Context.NativeQ=20; Context.ProjectYield=1;
    Context.Disciplines={{0,90,100,50},{1,0,100,50}};
    auto Packets = FEmployeeEquipmentEffectEvaluator::PlanPackets(Attempt,Context,Loadout,50);
    TestEqual(TEXT("Overflow leaves 17 on original"),Packets[0].Score,17.f);
    TestEqual(TEXT("Only three points transferred"),Packets[1].Score,3.f);
    Context.ProjectYield=2; Context.NativeQ=10;
    Packets=FEmployeeEquipmentEffectEvaluator::PlanPackets(Attempt,Context,Loadout,50);
    TestEqual(TEXT("Building unit deficiency converts back once"),Packets[1].Score,1.5f);
    Loadout.UniqueEffects.Reset();
    AddUnique(Loadout,TEXT("EQF_GATE_RESCUE"),{{TEXT("ScorePercent"),40}});
    AddUnique(Loadout,TEXT("EQF_CRIT_ECHO"),{{TEXT("ScorePercent"),25},{TEXT("ICD"),3}});
    FEmployeeEquipmentEffectEvaluator::SyncLoadout(Attempt.Employees[1],Loadout,false);
    Context.NativeQ=10; Context.ProjectYield=1; Context.bCritical=true; Context.Disciplines[0].Acquired=35;
    Packets=FEmployeeEquipmentEffectEvaluator::PlanPackets(Attempt,Context,Loadout,50);
    float Total=0; for(const auto& Packet:Packets) Total+=Packet.Score;
    TestTrue(TEXT("6.5 requested bonus is proportionally capped to five"), FMath::IsNearlyEqual(Total,15.f));
    TestTrue(TEXT("Neither bonus arbitrarily consumes all shared budget"), Packets[1].Score>0 && Packets[2].Score>0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEquipmentManualCatchTest,
    "CompanyGrowth.Equipment.Effects.ManualCatchIdentityReserveAndNoRecursiveRecovery", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEquipmentManualCatchTest::RunTest(const FString& Parameters)
{
    FEquipmentEffectLoadout Loadout;
    Loadout.BasicCaps.Add(TEXT("EQF_FATIGUE_GAIN"),60); Loadout.Rules.Add(TEXT("InstantReliefCap"),40);
    AddUnique(Loadout,TEXT("EQF_RELIEF_RESERVE"),{{TEXT("Capacity"),20},{TEXT("Duration"),12}});
    AddUnique(Loadout,TEXT("EQF_CATCH_LEARNING"),{{TEXT("XPPercentPerCatch"),3},{TEXT("MaxCatches"),5}});
    auto Attempt=MakeAttempt(1,Loadout);
    const FGuid DownId=FGuid::NewGuid();
    FEmployeeEquipmentEffectEvaluator::RecordDown(Attempt,1,0,DownId,{});
    float Self=0; TArray<FEquipmentReliefAction> Others;
    TestFalse(TEXT("Wrong down cannot be caught"), FEmployeeEquipmentEffectEvaluator::RecordManualCatch(Attempt,1,FGuid::NewGuid(),60,30,Loadout,{},Self,Others));
    TestTrue(TEXT("Actual down can be caught once"),FEmployeeEquipmentEffectEvaluator::RecordManualCatch(Attempt,1,DownId,60,30,Loadout,{},Self,Others));
    TestFalse(TEXT("Double tap does not grant learning twice"),FEmployeeEquipmentEffectEvaluator::RecordManualCatch(Attempt,1,DownId,60,30,Loadout,{},Self,Others));
    TestEqual(TEXT("One genuine learning record"),Attempt.Employees[1].Effects[TEXT("EQF_CATCH_LEARNING")].CatchCount,1);
    TestEqual(TEXT("Excess relief absorbs 20 of next 25 fatigue"),FEmployeeEquipmentEffectEvaluator::ResolveFatigueDelta(Attempt,1,25,0,40,Loadout),5.f);
    TestEqual(TEXT("Reserve is finite, next 25 is unchanged"),FEmployeeEquipmentEffectEvaluator::ResolveFatigueDelta(Attempt,1,25,5,40,Loadout),25.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEquipmentManualRouteTest,
    "CompanyGrowth.Equipment.Effects.ManualRoutingPreservesLowerPriorityReservation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEquipmentManualRouteTest::RunTest(const FString& Parameters)
{
    FEquipmentEffectLoadout Loadout;
    AddUnique(Loadout,TEXT("EQF_MANUAL_PRIORITY"),{{TEXT("Charges"),3},{TEXT("Duration"),8},{TEXT("ICD"),15}});
    AddUnique(Loadout,TEXT("EQF_COMBO_FINISH"),{{TEXT("Duration"),8},{TEXT("ICD"),10},{TEXT("MinimumDisciplines"),2}});
    auto Attempt=MakeAttempt(1,Loadout);
    auto& Combo=Attempt.Employees[1].Effects[TEXT("EQF_COMBO_FINISH")]; Combo.Charges=1; Combo.TargetSlot=0; Combo.ExpiresAt=8;
    TArray<FEquipmentDisciplineState> Slots={{0,0,100,50},{1,0,100,50}};
    TestFalse(TEXT("Inactive instruction does not start cooldown"),FEmployeeEquipmentEffectEvaluator::RequestManualPriority(Attempt,1,4,Loadout,Slots));
    TestTrue(TEXT("Valid manual instruction accepted"),FEmployeeEquipmentEffectEvaluator::RequestManualPriority(Attempt,1,1,Loadout,Slots));
    TestFalse(TEXT("Immediate replacement blocked by ICD"),FEmployeeEquipmentEffectEvaluator::RequestManualPriority(Attempt,1,0,Loadout,Slots));
    FName Route;
    TestEqual(TEXT("Manual route outranks pending combo"),FEmployeeEquipmentEffectEvaluator::SelectRoute(Attempt,1,Loadout,Slots,1,Route),1);
    FEquipmentContributionContext Context; Context.RootEventId=FGuid::NewGuid(); Context.EmployeeId=1; Context.ChosenSlot=1; Context.RouteEffect=Route;
    TArray<FEquipmentReliefAction> Relief;
    FEmployeeEquipmentEffectEvaluator::CommitNative(Attempt,Context,Loadout,Relief);
    TestEqual(TEXT("Unused combo is preserved"),Attempt.Employees[1].Effects[TEXT("EQF_COMBO_FINISH")].Charges,1);
    TestEqual(TEXT("Only one manual use consumed"),Attempt.Employees[1].Effects[TEXT("EQF_MANUAL_PRIORITY")].Charges,2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEquipmentConditionalXPTest,
    "CompanyGrowth.Equipment.Effects.FinalXPUsesHistoricalExposureAndClosesOnce", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEquipmentConditionalXPTest::RunTest(const FString& Parameters)
{
    FEquipmentEffectLoadout Loadout;
    AddUnique(Loadout,TEXT("EQF_ACTIVE_LEARNING"),{{TEXT("CountPerTier"),5},{TEXT("XPPercentPerTier"),2},{TEXT("MaxTiers"),6}});
    auto Attempt=MakeAttempt(1,Loadout); Attempt.ActiveTime=30; Attempt.CompletedSegments=1;
    auto& Employee=Attempt.Employees[1]; Employee.BaselineXP=100; Employee.SelfEquipmentXP=45;
    auto& Learned=Employee.Effects[TEXT("EQF_ACTIVE_LEARNING")]; Learned.NativeCount=30; Learned.EligibleBaseXP=100;
    FEmployeeEquipmentEffectEvaluator::SyncLoadout(Employee,FEquipmentEffectLoadout(),false);
    auto Grants=FEmployeeEquipmentEffectEvaluator::FinalizeXP(Attempt,EEquipmentAttemptOutcome::Launched,{},50,20);
    TestEqual(TEXT("Historical equipment learning survives unequip"),Grants.Num(),1);
    if (Grants.Num()==1) TestEqual(TEXT("Only remaining five percent self budget can pay"),Grants[0].EquipmentAmount,5.f);
    TestEqual(TEXT("Final receipt cannot pay twice"),FEmployeeEquipmentEffectEvaluator::FinalizeXP(Attempt,EEquipmentAttemptOutcome::Launched,{},50,20).Num(),0);
    auto Abort=MakeAttempt(2,Loadout); Abort.Employees[2].Effects[TEXT("EQF_ACTIVE_LEARNING")].NativeCount=30;
    Abort.Employees[2].Effects[TEXT("EQF_ACTIVE_LEARNING")].EligibleBaseXP=100;
    TestEqual(TEXT("Map abort never pays completion bonus"),FEmployeeEquipmentEffectEvaluator::FinalizeXP(Abort,EEquipmentAttemptOutcome::Aborted,{},50,20).Num(),0);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEquipmentFinalizationCleanupTest,
    "CompanyGrowth.Equipment.Effects.FinalizationClearsTransientStateAndPreservesReceipts", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEquipmentFinalizationCleanupTest::RunTest(const FString& Parameters)
{
    FEquipmentEffectLoadout Loadout;
    AddUnique(Loadout,TEXT("EQF_ACTIVE_LEARNING"),{{TEXT("CountPerTier"),5},{TEXT("XPPercentPerTier"),2},{TEXT("MaxTiers"),6}});
    AddUnique(Loadout,TEXT("EQF_MENTOR_LINK"),{{TEXT("XPPercent"),8},{TEXT("TypedPercent"),50},{TEXT("MinContributions"),5}});
    for (const auto Outcome : {EEquipmentAttemptOutcome::Launched, EEquipmentAttemptOutcome::Failed, EEquipmentAttemptOutcome::Aborted})
    {
        auto Attempt=MakeAttempt(1,Loadout); Attempt.ActiveTime=30; Attempt.CompletedSegments=1;
        auto& Provider=Attempt.Employees[1]; Provider.StartLevel=5;
        Provider.SeenManualDownIds.Add(FGuid::NewGuid());
        for (auto& Pair : Provider.Effects) Pair.Value.SegmentExposure=30;
        Provider.Effects[TEXT("EQF_ACTIVE_LEARNING")].NativeCount=30;
        auto& Recipient=Attempt.Employees.Add(2); Recipient.EmployeeId=2; Recipient.StartLevel=1;
        Recipient.NativeCount=5; Recipient.TypedSeconds=30;
        Attempt.CommittedRootEvents.Add(FGuid::NewGuid()); Attempt.Supports.AddDefaulted();
        FEquipmentXPGrant ProviderBase, RecipientBase;
        TestTrue(TEXT("Provider baseline receipt recorded"),FEmployeeEquipmentEffectEvaluator::RecordBaselineXP(Attempt,1,1,100,50,ProviderBase));
        TestTrue(TEXT("Recipient baseline receipt recorded"),FEmployeeEquipmentEffectEvaluator::RecordBaselineXP(Attempt,2,1,100,50,RecipientBase));
        const auto Grants=FEmployeeEquipmentEffectEvaluator::FinalizeXP(Attempt,Outcome,{},50,20);
        const int32 ExpectedFinalCount=Outcome==EEquipmentAttemptOutcome::Aborted ? 0 : (Outcome==EEquipmentAttemptOutcome::Launched ? 2 : 1);
        TestEqual(TEXT("Final XP is calculated before participant history is released"),Grants.Num(),ExpectedFinalCount);
        float FinalAmount=0; for (const auto& Grant : Grants) FinalAmount+=Grant.EquipmentAmount;
        TestEqual(TEXT("Mentor calculation retains participant history until all grants resolve"),FinalAmount,Outcome==EEquipmentAttemptOutcome::Aborted ? 0.f : (Outcome==EEquipmentAttemptOutcome::Launched ? 20.f : 12.f));
        TestEqual(TEXT("Final outcome remains recorded"),Attempt.Outcome,Outcome);
        TestEqual(TEXT("Employee effects, parameters and seen down IDs are released"),Attempt.Employees.Num(),0);
        TestEqual(TEXT("Temporary support effects are released"),Attempt.Supports.Num(),0);
        TestEqual(TEXT("Temporary root identities are released"),Attempt.CommittedRootEvents.Num(),0);
        TestEqual(TEXT("Baseline and final receipts remain recorded"),Attempt.XPReceipts.Num(),2+ExpectedFinalCount);
        TestTrue(TEXT("Existing baseline receipt is preserved"),Attempt.XPReceipts.Contains(ProviderBase.GrantId));
        TestEqual(TEXT("Retry after cleanup cannot issue additional grants"),FEmployeeEquipmentEffectEvaluator::FinalizeXP(Attempt,EEquipmentAttemptOutcome::Launched,{},50,20).Num(),0);
        TestEqual(TEXT("Retry cannot overwrite the recorded outcome"),Attempt.Outcome,Outcome);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEquipmentCritChainTest,
    "CompanyGrowth.Equipment.Effects.PityConversionPrecisionAndCritRecovery", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEquipmentCritChainTest::RunTest(const FString& Parameters)
{
    FEquipmentEffectLoadout Loadout;
    Loadout.BasicCaps.Add(TEXT("EQF_CRIT_RATE"),10); Loadout.BasicCaps.Add(TEXT("EQF_CRIT_POWER"),1);
    Loadout.BasicValues.Add(TEXT("EQF_CRIT_RATE"),2); Loadout.Rules.Add(TEXT("GeneralCritCapPercent"),30);
    Loadout.Rules.Add(TEXT("GuaranteedGap"),6); Loadout.Rules.Add(TEXT("InstantReliefCap"),40);
    AddUnique(Loadout,TEXT("EQF_CRIT_PITY"),{{TEXT("Count"),6},{TEXT("Duration"),10}});
    AddUnique(Loadout,TEXT("EQF_CRIT_CONVERSION"),{{TEXT("PowerPerStack"),.1f},{TEXT("MaxStacks"),5},{TEXT("Duration"),20}});
    AddUnique(Loadout,TEXT("EQF_PRECISION_CHAIN"),{{TEXT("Count"),3},{TEXT("CritPercent"),10},{TEXT("Duration"),8},{TEXT("ICD"),8}});
    AddUnique(Loadout,TEXT("EQF_CRIT_RELIEF"),{{TEXT("Relief"),6},{TEXT("ICD"),3}});
    auto Attempt=MakeAttempt(1,Loadout);
    for(int32 Index=0;Index<3;++Index)
    {
        FEquipmentContributionContext Context; Context.RootEventId=FGuid::NewGuid(); Context.EmployeeId=1; Context.ChosenSlot=Index;
        TArray<FEquipmentReliefAction> Relief;
        FEmployeeEquipmentEffectEvaluator::CommitNative(Attempt,Context,Loadout,Relief);
    }
    TestTrue(TEXT("Three noncrits add .30 crit power"),FMath::IsNearlyEqual(FEmployeeEquipmentEffectEvaluator::CritPower(Attempt,1,Loadout),.3f));
    TestEqual(TEXT("Precision plus base equipment is capped at 10pp"),FEmployeeEquipmentEffectEvaluator::CritRatePercent(Attempt,1,Loadout),10.f);
    const float OrdinaryCritChance=FEmployeeEquipmentEffectEvaluator::GeneralCritChance(.29f,.3f,Attempt,1,Loadout);
    // /fp:fast의 퍼센트 변환은 0.3f 리터럴과 한 ULP 차이가 날 수 있다.
    TestEqual(TEXT("29% existing plus gear remains 30% ordinary"),OrdinaryCritChance,.3f,1.e-6f);
    TestTrue(TEXT("Ordinary crit never exceeds the configured 30% cap"),OrdinaryCritChance<=.3f);
    for(int32 Index=0;Index<3;++Index)
    {
        FEquipmentContributionContext Context; Context.RootEventId=FGuid::NewGuid(); Context.EmployeeId=1; Context.ChosenSlot=0;
        TArray<FEquipmentReliefAction> Relief; FEmployeeEquipmentEffectEvaluator::CommitNative(Attempt,Context,Loadout,Relief);
    }
    TestEqual(TEXT("Six consecutive noncrits reserve pity"),FEmployeeEquipmentEffectEvaluator::GuaranteedCrit(Attempt,1,Loadout),FName(TEXT("EQF_CRIT_PITY")));
    TestEqual(TEXT("Conversion stack does not exceed five"),Attempt.Employees[1].Effects[TEXT("EQF_CRIT_CONVERSION")].Counter,5);
    FEquipmentContributionContext Crit; Crit.RootEventId=FGuid::NewGuid(); Crit.EmployeeId=1; Crit.ChosenSlot=0; Crit.bCritical=true; Crit.GuaranteedEffect=TEXT("EQF_CRIT_PITY");
    TArray<FEquipmentReliefAction> Relief; FEmployeeEquipmentEffectEvaluator::CommitNative(Attempt,Crit,Loadout,Relief);
    TestEqual(TEXT("Committed crit consumes all conversion stacks"),FEmployeeEquipmentEffectEvaluator::CritPower(Attempt,1,Loadout),0.f);
    TestEqual(TEXT("Only genuine root crit requests relief"),Relief.Num(),1);
    if(Relief.Num()==1) TestEqual(TEXT("Crit relief actual amount"),Relief[0].Amount,6.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEquipmentTeamTest,
    "CompanyGrowth.Equipment.Effects.TeamDiversityLastWorkerAndRoundRobinSupport", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEquipmentTeamTest::RunTest(const FString& Parameters)
{
    FEquipmentEffectLoadout Loadout;
    Loadout.BasicCaps.Add(TEXT("EQF_SCORE_GAIN"),100); Loadout.BasicCaps.Add(TEXT("EQF_WORK_RATE"),100);
    Loadout.Rules.Add(TEXT("TeamScoreCapPercent"),30);
    AddUnique(Loadout,TEXT("EQF_DIVERSITY_PACE"),{{TEXT("MinTyping"),4},{TEXT("MinDisciplines"),4},{TEXT("ScorePercent"),8}});
    AddUnique(Loadout,TEXT("EQF_LAST_WORKER"),{{TEXT("MinOtherDown"),2},{TEXT("ScorePercent"),35}});
    AddUnique(Loadout,TEXT("EQF_TEAM_PACE"),{{TEXT("SpeedPercent"),10},{TEXT("Duration"),4},{TEXT("ICD"),8}});
    auto Attempt=MakeAttempt(1,Loadout);
    FEquipmentContributionContext Context; Context.EmployeeId=1; Context.ChosenSlot=0; Context.PrimarySlot=0;
    for(int32 Index=0;Index<4;++Index)
    {
        FEquipmentParticipant P; P.EmployeeId=Index+1; P.PrimarySlot=Index; P.bSeated=true; P.bTyping=true; P.Loadout=Loadout;
        Context.Participants.Add(P); Context.Disciplines.Add({Index,0,100,50});
        if(Index>0) { auto& E=Attempt.Employees.Add(Index+1); E.EmployeeId=Index+1; FEmployeeEquipmentEffectEvaluator::SyncLoadout(E,Loadout,true); }
    }
    TestEqual(TEXT("Four providers do not multiply diversity four times"),FEmployeeEquipmentEffectEvaluator::TeamScorePercent(Context),8.f);
    Context.Participants[3].bTyping=false;
    TestEqual(TEXT("Dropping to three typing removes diversity immediately"),FEmployeeEquipmentEffectEvaluator::TeamScorePercent(Context),0.f);
    Context.Participants[1].bTyping=false; Context.Participants[2].bTyping=false;
    Context.Participants[1].bDown=true; Context.Participants[2].bDown=true;
    TestEqual(TEXT("One typing and two real down grant 35%"),FEmployeeEquipmentEffectEvaluator::PersonalScorePercent(Attempt,Context,Loadout),35.f);
    Context.Participants[2].bDown=false;
    TestEqual(TEXT("Resting or missing worker is not a second down"),FEmployeeEquipmentEffectEvaluator::PersonalScorePercent(Attempt,Context,Loadout),0.f);
    Context.Participants[1].bTyping=true; Context.Participants[2].bTyping=true;
    Context.Participants[1].bDown=false; Context.Participants[2].bDown=false;
    Context.RootEventId=FGuid::NewGuid(); Context.bCritical=true;
    TArray<FEquipmentReliefAction> Relief; FEmployeeEquipmentEffectEvaluator::CommitNative(Attempt,Context,Loadout,Relief);
    TestEqual(TEXT("First pace creates one support"),Attempt.Supports.Num(),1);
    if(Attempt.Supports.Num()==1) TestEqual(TEXT("First pace target is lowest employee ID"),Attempt.Supports[0].RecipientEmployeeId,2);
    Attempt.ActiveTime=8; Context.RootEventId=FGuid::NewGuid(); FEmployeeEquipmentEffectEvaluator::CommitNative(Attempt,Context,Loadout,Relief);
    TestEqual(TEXT("Second pace creates its own support record"),Attempt.Supports.Num(),2);
    if(Attempt.Supports.Num()==2) TestEqual(TEXT("Next pace target rotates to next employee"),Attempt.Supports.Last().RecipientEmployeeId,3);
    TestEqual(TEXT("Recipient gets actual speed support"),FEmployeeEquipmentEffectEvaluator::SpeedPercent(Attempt,3,Loadout),10.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEquipmentTimeTest,
    "CompanyGrowth.Equipment.Effects.ActiveClockExpiresRhythmAndShieldWithoutPauseLoss", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEquipmentTimeTest::RunTest(const FString& Parameters)
{
    FEquipmentEffectLoadout Loadout;
    Loadout.BasicCaps.Add(TEXT("EQF_WORK_RATE"),100);
    AddUnique(Loadout,TEXT("EQF_UNBROKEN_RHYTHM"),{{TEXT("Count"),8},{TEXT("SpeedPercent"),20},{TEXT("Duration"),6},{TEXT("ICD"),10}});
    AddUnique(Loadout,TEXT("EQF_FINISHING_SPRINT"),{{TEXT("SpeedPercent"),25},{TEXT("RemainingPercent"),20}});
    AddUnique(Loadout,TEXT("EQF_RECOVERY_SHIELD"),{{TEXT("Duration"),4},{TEXT("ICD"),12}});
    auto Attempt=MakeAttempt(1,Loadout);
    for(int32 Index=0;Index<8;++Index)
    {
        FEquipmentContributionContext Context; Context.RootEventId=FGuid::NewGuid(); Context.EmployeeId=1; Context.ChosenSlot=0;
        TArray<FEquipmentReliefAction> Relief; FEmployeeEquipmentEffectEvaluator::CommitNative(Attempt,Context,Loadout,Relief);
    }
    TestEqual(TEXT("Eight native commits produce speed20"),FEmployeeEquipmentEffectEvaluator::SpeedPercent(Attempt,1,Loadout),20.f);
    const FGuid Down=FGuid::NewGuid(); FEmployeeEquipmentEffectEvaluator::RecordDown(Attempt,1,0,Down,{});
    FEmployeeEquipmentEffectEvaluator::RecordRecovery(Attempt,1,EEquipmentRecoverySource::Natural,Loadout);
    float Relief=0;
    TestTrue(TEXT("Actual recovery starts shield"),FEmployeeEquipmentEffectEvaluator::TryVetoDown(Attempt,1,true,Loadout,{},Relief));
    FEquipmentParticipant P; P.EmployeeId=1; P.bSeated=true; P.bTyping=true; P.Loadout=Loadout;
    FEmployeeEquipmentEffectEvaluator::AdvanceTime(Attempt,0,{P});
    TestEqual(TEXT("Pause does not advance equipment clock"),Attempt.ActiveTime,0.);
    FEmployeeEquipmentEffectEvaluator::AdvanceTime(Attempt,4,{P});
    TestFalse(TEXT("Shield expires after four active seconds"),FEmployeeEquipmentEffectEvaluator::TryVetoDown(Attempt,1,true,Loadout,{},Relief));
    FEmployeeEquipmentEffectEvaluator::AdvanceTime(Attempt,20,{P});
    TestEqual(TEXT("Last six of thirty seconds use finishing sprint only"),FEmployeeEquipmentEffectEvaluator::SpeedPercent(Attempt,1,Loadout),25.f);
    return true;
}
#endif
