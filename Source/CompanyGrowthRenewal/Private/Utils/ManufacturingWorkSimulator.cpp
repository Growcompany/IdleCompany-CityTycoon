#include "Utils/ManufacturingWorkSimulator.h"
#include "Data/ManufacturingResearchRules.h"

#include "Data/FatigueConfig.h"
#include "Data/EmployeeStatsData.h"
#include "Entity/Officeworker/EmployeeBehaviorComponent.h"
#include "Utils/ManufacturingRecoveryRules.h"

namespace
{
constexpr double SimulationStep = .25;
constexpr int32 FieldCount = 3;

float Affinity(const FManufacturingWorkerInput& Worker, int32 Field)
{
    return Worker.Affinities.IsValidIndex(Field) && FMath::IsFinite(Worker.Affinities[Field])
        ? FMath::Max(0.f, Worker.Affinities[Field]) : 0.f;
}

TArray<FEquipmentDisciplineState> Disciplines(const FManufacturingWorkState& State)
{
    TArray<FEquipmentDisciplineState> Result;
    for (int32 Field = 0; Field < FieldCount; ++Field)
    {
        const float Target = FMath::Max(1., State.PeriodTargets[Field]);
        Result.Add({Field, static_cast<float>(State.PeriodPoints[Field]), Target, Target * .5f});
    }
    return Result;
}

void BeginPeriod(FManufacturingWorkState& State, int32 BuildingId,
    const TArray<FManufacturingWorkerInput>& Workers, const UFatigueConfig& Config, float ProjectYield,
    const FEquipmentAttemptRuntime* Previous = nullptr)
{
    State.Equipment = FEquipmentAttemptRuntime();
    auto& Attempt = State.Equipment;
    Attempt.AttemptId = FGuid(static_cast<uint32>(BuildingId), static_cast<uint32>(++State.Period), 0x4D464757, 1);
    Attempt.BuildingId = BuildingId;
    Attempt.SegmentId = State.Period;
    Attempt.SegmentDuration = FMath::Max(static_cast<float>(SimulationStep), Config.ManufacturingPeriodSeconds);
    if (Previous)
    {
        Attempt.EventSequence = Previous->EventSequence;
        Attempt.LastNativeSequence = Previous->LastNativeSequence;
        Attempt.LastNativeEmployee = Previous->LastNativeEmployee;
        Attempt.LastNativeSlot = Previous->LastNativeSlot;
        Attempt.LastNativeTime = Previous->LastNativeTime - Previous->ActiveTime;
        for (auto Support : Previous->Supports)
        {
            Support.ExpiresAt -= Previous->ActiveTime;
            if (Support.ExpiresAt > 0) Attempt.Supports.Add(Support);
        }
    }
    State.PeriodPoints.Init(0., FieldCount);
    State.PeriodTargets.Init(0., FieldCount);
    for (const auto& Worker : Workers)
    {
        auto& Work = State.Employees.FindOrAdd(Worker.EmployeeId);
        auto& Runtime = Attempt.Employees.Add(Worker.EmployeeId);
        Runtime.EmployeeId = Worker.EmployeeId;
        Runtime.StartLevel = Worker.Level;
        Runtime.bStartParticipant = true;
        const auto* Old = Previous ? Previous->Employees.Find(Worker.EmployeeId) : nullptr;
        if (Old)
        {
            Runtime.bDrowsyArmed = Old->bDrowsyArmed;
            Runtime.bRestArmed = Old->bRestArmed;
            Runtime.LastNativeTarget = Old->LastNativeTarget;
            Runtime.NextGuaranteedEmission = FMath::Max(0, Old->NextGuaranteedEmission - Old->NativeCount);
            for (const auto& Pair : Old->Effects)
            {
                auto& Effect = Runtime.Effects.Add(Pair.Key, Pair.Value);
                Effect.CooldownUntil = FMath::Max(0., Effect.CooldownUntil - Previous->ActiveTime);
                Effect.ExpiresAt = FMath::Max(0., Effect.ExpiresAt - Previous->ActiveTime);
                Effect.bStartedWithEffect = false;
                Effect.bStartConsumed = false;
                Effect.NativeCount = 0;
                Effect.CatchCount = 0;
                Effect.TypedSeconds = 0;
                Effect.EligibleBaseXP = 0;
                Effect.SegmentExposure = 0;
                Effect.SegmentValueSeconds = 0;
                Effect.DisciplineCounts.Reset();
                if (Pair.Key == TEXT("EQF_COVERAGE_PLEDGE")) Effect.VisitedMask = 0;
            }
        }
        FEmployeeEquipmentEffectEvaluator::SyncLoadout(Runtime, Worker.Loadout, true);
        if (Old)
            for (auto& Pair : Runtime.Effects)
            {
                const auto* OldEffect = Old->Effects.Find(Pair.Key);
                const bool bPeriodCharge = Pair.Key == TEXT("EQF_FIRST_CRIT") || Pair.Key == TEXT("EQF_BOLT_GUARD")
                    || Pair.Key == TEXT("EQF_DROWSY_GUARD") || Pair.Key == TEXT("EQF_AUTO_CATCH");
                if (OldEffect && !bPeriodCharge) Pair.Value.Charges = OldEffect->Charges;
            }
        if (Work.Rest != EManufacturingRest::None && Work.Rest != EManufacturingRest::Breathing)
        {
            Runtime.DownId = FGuid(BuildingId, State.Period, Worker.EmployeeId, 2);
            Runtime.DownStartedAt = -Work.RestElapsed;
            Runtime.DownCount = 1;
        }
        if (Work.Rest == EManufacturingRest::Breathing) Runtime.RestUntil = Work.RestRemaining;
        float TotalAffinity = 0;
        for (int32 Field = 0; Field < FieldCount; ++Field) TotalAffinity += Affinity(Worker, Field);
        for (int32 Field = 0; Field < FieldCount && TotalAffinity > 0; ++Field)
        {
            const float A = Affinity(Worker, Field);
            State.PeriodTargets[Field] += Worker.BaseOutput * UEmployeeBehaviorComponent::ScoreNormInterval
                * Worker.WorkRate * Worker.ScoreMultiplier * ProjectYield * A * A / TotalAffinity * Attempt.SegmentDuration;
        }
    }
}

void FinalizePeriod(FManufacturingWorkState& State, FManufacturingWorkResult& Result,
    const TArray<FManufacturingWorkerInput>& Workers)
{
    auto& Attempt = State.Equipment;
    ++Attempt.CompletedSegments;
    // 상한은 공용 효과 DT의 규칙이며 장착 여부와 무관하게 같은 값을 사용한다.
    const float SelfCap = Workers.IsEmpty() ? 0.f : Workers[0].Loadout.Rule(TEXT("SelfXPCapPercent"));
    const float MentorCap = Workers.IsEmpty() ? 0.f : Workers[0].Loadout.Rule(TEXT("MentorXPCapPercent"));
    for (auto& Pair : State.Employees)
    {
        if (Pair.Value.PeriodBaselineExperience <= 0) continue;
        FEquipmentXPGrant Grant;
        if (FEmployeeEquipmentEffectEvaluator::RecordBaselineXP(Attempt, Pair.Key, 0,
            static_cast<float>(Pair.Value.PeriodBaselineExperience), SelfCap, Grant))
        {
            Result.Experience.FindOrAdd(Pair.Key) += Grant.EquipmentAmount;
            Pair.Value.PeriodBaselineExperience = 0;
        }
    }
    for (const auto& Grant : FEmployeeEquipmentEffectEvaluator::FinalizeXP(Attempt,
        EEquipmentAttemptOutcome::Launched, Disciplines(State), SelfCap, MentorCap, true))
        Result.Experience.FindOrAdd(Grant.EmployeeId) += Grant.EquipmentAmount;
}

void Contribute(FManufacturingWorkState& State, const FManufacturingWorkerInput& Worker,
    FEquipmentContributionContext& Context, const UFatigueConfig& Config,
    float ProjectYield, FRandomStream& Random, FManufacturingWorkResult& Result, bool bEarnSpendablePoints)
{
    auto& Attempt = State.Equipment;
    auto& Runtime = Attempt.Employees.FindChecked(Worker.EmployeeId);
    Context.RouteEffect = NAME_None;
    Context.GuaranteedEffect = NAME_None;
    Context.AttemptId = Attempt.AttemptId;
    Context.RootEventId = FGuid(State.Period, Worker.EmployeeId,
        static_cast<uint32>(Attempt.EventSequence + 1), 0x4D464743);
    Context.EmployeeId = Worker.EmployeeId;
    Context.PrimarySlot = Worker.PrimarySlot;
    Context.ProjectYield = ProjectYield;
    Context.Disciplines = Disciplines(State);
    if (Worker.Loadout.Has(TEXT("EQF_MANUAL_PRIORITY")))
    {
        const auto* Lowest = &Context.Disciplines[0];
        for (const auto& Field : Context.Disciplines)
            if (Field.Acquired / Field.Target < Lowest->Acquired / Lowest->Target) Lowest = &Field;
        FEmployeeEquipmentEffectEvaluator::RequestManualPriority(Attempt, Worker.EmployeeId,
            Lowest->DisciplineSlot, Worker.Loadout, Context.Disciplines);
    }
    Context.ChosenSlot = FEmployeeEquipmentEffectEvaluator::SelectRoute(Attempt, Worker.EmployeeId,
        Worker.Loadout, Context.Disciplines, Random.FRand(), Context.RouteEffect);
    if (Context.ChosenSlot == INDEX_NONE)
    {
        float TotalWeight = 0;
        const bool bRoundRobin = Worker.Loadout.Has(TEXT("EQF_ROUND_ROBIN"));
        for (int32 Field = 0; Field < FieldCount; ++Field)
            if (!bRoundRobin || Field != Runtime.LastNativeTarget) TotalWeight += Affinity(Worker, Field);
        if (TotalWeight <= 0) return;
        float Roll = Random.FRand() * TotalWeight;
        for (int32 Field = 0; Field < FieldCount; ++Field)
        {
            if (bRoundRobin && Field == Runtime.LastNativeTarget) continue;
            Roll -= Affinity(Worker, Field);
            if (Roll <= 0) { Context.ChosenSlot = Field; break; }
        }
    }
    if (Context.ChosenSlot == INDEX_NONE) return;
    const auto& Loadout = Worker.Loadout;
    const float RandomCeiling = Loadout.Has(TEXT("EQF_STEADY_OUTPUT"))
        ? Loadout.Param(TEXT("EQF_STEADY_OUTPUT"), TEXT("RandomCeiling")) : 1.2f;
    const float RandomFloor = Loadout.Has(TEXT("EQF_STEADY_OUTPUT"))
        ? FMath::Min(RandomCeiling, Loadout.Param(TEXT("EQF_STEADY_OUTPUT"), TEXT("RandomFloor"))) : .8f;
    float Score = Worker.BaseOutput * UEmployeeBehaviorComponent::ScoreNormInterval * Affinity(Worker, Context.ChosenSlot)
        * Random.FRandRange(RandomFloor, RandomCeiling) * Worker.ScoreMultiplier
        * FManufacturingWorkSimulator::FatigueOutput(State.Employees.FindChecked(Worker.EmployeeId).Fatigue, Loadout, Config);
    Score *= 1.f + FEmployeeEquipmentEffectEvaluator::PersonalScorePercent(Attempt, Context, Loadout) / 100.f;
    Score *= 1.f + FEmployeeEquipmentEffectEvaluator::TeamScorePercent(Context) / 100.f;
    Context.GuaranteedEffect = FEmployeeEquipmentEffectEvaluator::GuaranteedCrit(Attempt, Worker.EmployeeId, Loadout);
    const float CritChance = FEmployeeEquipmentEffectEvaluator::GeneralCritChance(Worker.CritChance,
        EmployeeStatTuning::MaxCritChance, Attempt, Worker.EmployeeId, Loadout);
    const float CritRoll = Random.FRand();
    Context.bCritical = !Context.GuaranteedEffect.IsNone() || CritRoll < CritChance;
    if (Context.bCritical) Score *= Worker.CritPower + FEmployeeEquipmentEffectEvaluator::CritPower(Attempt, Worker.EmployeeId, Loadout);
    if (!FMath::IsFinite(Score) || Score <= 0) return;
    Context.NativeQ = Score;
    const auto Packets = FEmployeeEquipmentEffectEvaluator::PlanPackets(Attempt, Context, Loadout, Loadout.Rule(TEXT("BonusScoreCapPercent")));
    auto Candidate = Context.Disciplines;
    if (!FEmployeeEquipmentEffectEvaluator::TryApplyPackets(Candidate, Packets, ProjectYield)) return;
    for (int32 Field = 0; Field < FieldCount; ++Field)
    {
        const double Gain = Candidate[Field].Acquired - Context.Disciplines[Field].Acquired;
        State.PeriodPoints[Field] += Gain;
        if (bEarnSpendablePoints) Result.Points[Field] += Gain;
    }
    TArray<FEquipmentReliefAction> Relief;
    FEmployeeEquipmentEffectEvaluator::CommitNative(Attempt, Context, Loadout, Relief);
    for (const auto& Action : Relief)
        if (auto* Work = State.Employees.Find(Action.EmployeeId)) Work->Fatigue = FMath::Max(0.f, Work->Fatigue - Action.Amount);
}
}

TArray<FEquipmentParticipant> FManufacturingWorkSimulator::Participants(const FManufacturingWorkState& State,
    const TArray<FManufacturingWorkerInput>& Workers)
{
    TArray<FEquipmentParticipant> Result;
    for (const auto& Worker : Workers)
    {
        FEquipmentParticipant Participant;
        Participant.EmployeeId = Worker.EmployeeId;
        Participant.Level = Worker.Level;
        Participant.PrimarySlot = Worker.PrimarySlot;
        Participant.bSeated = true;
        const auto* Work = State.Employees.Find(Worker.EmployeeId);
        Participant.bTyping = !Work || Work->Rest == EManufacturingRest::None;
        Participant.bDown = Work && Work->Rest != EManufacturingRest::None && Work->Rest != EManufacturingRest::Breathing;
        Participant.Fatigue = Work ? Work->Fatigue : 0;
        Participant.Loadout = Worker.Loadout;
        Result.Add(MoveTemp(Participant));
    }
    return Result;
}

float FManufacturingWorkSimulator::FatigueOutput(float Fatigue, const FEquipmentEffectLoadout& Loadout, const UFatigueConfig& Config)
{
    const float Ratio = FMath::Clamp((Fatigue - Config.TiredThreshold) / FMath::Max(1.f, 100.f - Config.TiredThreshold), 0.f, 1.f);
    return 1.f - Config.MaxFatiguePenalty * Ratio * (1.f - Loadout.Basic(TEXT("EQF_FATIGUE_PENALTY")) / 100.f);
}

FManufacturingWorkResult FManufacturingWorkSimulator::Advance(FManufacturingWorkState& State, int32 BuildingId,
    const TArray<FManufacturingWorkerInput>& InWorkers, const UFatigueConfig& Config, float ProjectYield, double Seconds,
    FManufacturingResearchSimulation* Research)
{
    FManufacturingWorkResult Result;
    if (!FMath::IsFinite(Seconds) || Seconds <= 0 || !FMath::IsFinite(ProjectYield) || ProjectYield <= 0 || InWorkers.IsEmpty()) return Result;
    TArray<FManufacturingWorkerInput> Workers = InWorkers;
    Workers.Sort([](const auto& A, const auto& B) { return A.EmployeeId < B.EmployeeId; });
    if (State.RandomSeed == 0) State.RandomSeed = static_cast<int32>(GetTypeHash(BuildingId) ^ 0x4D464731);
    FRandomStream Random(State.RandomSeed);
    if (!State.Equipment.AttemptId.IsValid() || State.Equipment.Outcome != EEquipmentAttemptOutcome::Active)
        BeginPeriod(State, BuildingId, Workers, Config, ProjectYield);
    for (const auto& Worker : Workers)
    {
        State.Employees.FindOrAdd(Worker.EmployeeId);
        if (!State.Equipment.Employees.Contains(Worker.EmployeeId))
        {
            auto& Runtime = State.Equipment.Employees.Add(Worker.EmployeeId);
            Runtime.EmployeeId = Worker.EmployeeId;
            Runtime.StartLevel = Worker.Level;
            FEmployeeEquipmentEffectEvaluator::SyncLoadout(Runtime, Worker.Loadout, false);
        }
    }
    FEquipmentContributionContext Context;
    Context.Participants = Participants(State, Workers);
    State.PendingSeconds += Seconds;
    while (State.PendingSeconds + 1.e-9 >= SimulationStep)
    {
        const bool bResearching = Research && Research->State && Research->State->ProjectIndex > 0;
        double StepWorkedSeconds = 0.0;
        State.PendingSeconds = FMath::Max(0., State.PendingSeconds - SimulationStep);
        auto& Attempt = State.Equipment;
        for (auto& Participant : Context.Participants)
        {
            const auto& Work = State.Employees.FindChecked(Participant.EmployeeId);
            Participant.bTyping = Work.Rest == EManufacturingRest::None;
            Participant.bDown = Work.Rest != EManufacturingRest::None && Work.Rest != EManufacturingRest::Breathing;
            Participant.Fatigue = Work.Fatigue;
        }
        const auto& CurrentParticipants = Context.Participants;
        const double BeforeTime = Attempt.ActiveTime;
        FEmployeeEquipmentEffectEvaluator::AdvanceTime(Attempt, SimulationStep, CurrentParticipants);
        const float ConcentrationInterval = FMath::Max(static_cast<float>(SimulationStep), Config.ManufacturingConcentrationInterval);
        if (FMath::FloorToInt(BeforeTime / ConcentrationInterval) < FMath::FloorToInt(Attempt.ActiveTime / ConcentrationInterval))
            FEmployeeEquipmentEffectEvaluator::RecordFever(Attempt, CurrentParticipants);
        for (const auto& Worker : Workers)
        {
            const bool bCanWork = FManufacturingRecoveryRules::Advance(State, Worker, Config,
                SimulationStep, Random.FRand(), CurrentParticipants);
            for (auto& Participant : Context.Participants)
            {
                const auto& UpdatedWork = State.Employees.FindChecked(Participant.EmployeeId);
                Participant.bTyping = UpdatedWork.Rest == EManufacturingRest::None;
                Participant.bDown = UpdatedWork.Rest != EManufacturingRest::None && UpdatedWork.Rest != EManufacturingRest::Breathing;
                Participant.Fatigue = UpdatedWork.Fatigue;
            }
            if (!bCanWork) continue;
            auto& Work = State.Employees.FindChecked(Worker.EmployeeId);
            const float Rate = Worker.WorkRate * (1.f + FEmployeeEquipmentEffectEvaluator::SpeedPercent(Attempt, Worker.EmployeeId, Worker.Loadout) / 100.f);
            if (!FMath::IsFinite(Rate) || Rate <= 0) continue;
            const double Experience = SimulationStep * FMath::Max(0.f, Config.ManufacturingExperiencePerSecond) * Worker.ExperienceMultiplier;
            if (FMath::IsFinite(Experience) && Experience > 0.0)
            {
                Work.PeriodBaselineExperience += Experience;
                Result.Experience.FindOrAdd(Worker.EmployeeId) += Experience;
            }
            Result.WorkedSeconds += SimulationStep;
            StepWorkedSeconds += SimulationStep;
            Work.ContributionProgress += SimulationStep * Rate;
            while (Work.ContributionProgress >= 1.)
            {
                Work.ContributionProgress -= 1.;
                Contribute(State, Worker, Context, Config, ProjectYield, Random, Result, !bResearching);
            }
        }
        if (bResearching && ManufacturingResearchRules::Advance(*Research->State, Research->TeamCapability,
            Research->RequiredCapability, Research->RequiredWorkSeconds, StepWorkedSeconds, Workers.Num()))
        {
            Result.CompletedResearchProjectIndex = Research->State->ProjectIndex;
            *Research->State = FManufacturingResearchState();
            // 연구 중 쌓인 기여 소수분은 포인트 생산 단계로 넘기지 않는다.
            for (const auto& Worker : Workers)
                State.Employees.FindChecked(Worker.EmployeeId).ContributionProgress = 0.0;
        }
        if (Attempt.ActiveTime + 1.e-6 >= Attempt.SegmentDuration)
        {
            const FEquipmentAttemptRuntime Previous = Attempt;
            FinalizePeriod(State, Result, Workers);
            BeginPeriod(State, BuildingId, Workers, Config, ProjectYield, &Previous);
        }
    }
    State.RandomSeed = Random.GetCurrentSeed();
    return Result;
}

TArray<double> FManufacturingWorkSimulator::EstimateRates(const FManufacturingWorkState& State,
    const TArray<FManufacturingWorkerInput>& Workers, const UFatigueConfig& Config, float ProjectYield)
{
    TArray<double> Result = {0., 0., 0.};
    for (const auto& Worker : Workers)
    {
        const auto* Work = State.Employees.Find(Worker.EmployeeId);
        if (Work && Work->Rest != EManufacturingRest::None) continue;
        float TotalAffinity = 0;
        for (int32 Field = 0; Field < FieldCount; ++Field) TotalAffinity += Affinity(Worker, Field);
        if (TotalAffinity <= 0) continue;
        const auto& Loadout = Worker.Loadout;
        const float RandomMean = Loadout.Has(TEXT("EQF_STEADY_OUTPUT")) ?
            (Loadout.Param(TEXT("EQF_STEADY_OUTPUT"), TEXT("RandomFloor")) + Loadout.Param(TEXT("EQF_STEADY_OUTPUT"), TEXT("RandomCeiling"))) * .5f : 1.f;
        const float Chance = FEmployeeEquipmentEffectEvaluator::GeneralCritChance(Worker.CritChance,
            EmployeeStatTuning::MaxCritChance, State.Equipment, Worker.EmployeeId, Loadout);
        const float Crit = 1.f + Chance * (Worker.CritPower + FEmployeeEquipmentEffectEvaluator::CritPower(State.Equipment, Worker.EmployeeId, Loadout) - 1.f);
        const float Rate = Worker.WorkRate * (1.f + FEmployeeEquipmentEffectEvaluator::SpeedPercent(State.Equipment, Worker.EmployeeId, Loadout) / 100.f);
        for (int32 Field = 0; Field < FieldCount; ++Field)
        {
            FEquipmentContributionContext Context;
            Context.EmployeeId = Worker.EmployeeId; Context.PrimarySlot = Worker.PrimarySlot; Context.ChosenSlot = Field;
            Context.Participants = Participants(State, Workers);
            const double A = Affinity(Worker, Field);
            Result[Field] += Worker.BaseOutput * UEmployeeBehaviorComponent::ScoreNormInterval * Rate * Worker.ScoreMultiplier
                * ProjectYield * A * A / TotalAffinity * RandomMean * Crit
                * FatigueOutput(Work ? Work->Fatigue : 0, Loadout, Config)
                * (1.f + FEmployeeEquipmentEffectEvaluator::PersonalScorePercent(State.Equipment, Context, Loadout) / 100.f)
                * (1.f + FEmployeeEquipmentEffectEvaluator::TeamScorePercent(Context) / 100.f);
        }
    }
    return Result;
}
