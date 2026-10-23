#include "Utils/ManufacturingRecoveryRules.h"
#include "Data/FatigueConfig.h"

namespace
{
void ApplyRelief(FManufacturingWorkState& WorkState, int32 EmployeeId, float Amount)
{
    if (auto* Employee = WorkState.Employees.Find(EmployeeId))
        Employee->Fatigue = FMath::Clamp(Employee->Fatigue - Amount, 0.f, 100.f);
}

void ReturnFromDown(FManufacturingWorkState& WorkState, const FManufacturingWorkerInput& Worker,
    const UFatigueConfig& Config, const TArray<FEquipmentParticipant>& Participants, bool bAssisted)
{
    auto& Attempt = WorkState.Equipment;
    auto& Employee = WorkState.Employees.FindChecked(Worker.EmployeeId);
    const auto* Runtime = Attempt.Employees.Find(Worker.EmployeeId);
    if (Runtime && Runtime->DownId.IsValid())
    {
        float SelfRelief = 0;
        TArray<FEquipmentReliefAction> TeamRelief;
        // 자연 복귀도 자동 근무의 캐치 사건이다. 기본 탭 회복은 장비가 실제로 복귀를 앞당긴 경우에만 준다.
        if (FEmployeeEquipmentEffectEvaluator::RecordManualCatch(Attempt, Worker.EmployeeId, Runtime->DownId,
            bAssisted ? Config.TapRelief : 0.f, Employee.Fatigue, Worker.Loadout,
            Participants, SelfRelief, TeamRelief))
        {
            ApplyRelief(WorkState, Worker.EmployeeId, SelfRelief);
            for (const auto& Relief : TeamRelief) ApplyRelief(WorkState, Relief.EmployeeId, Relief.Amount);
        }
        FEmployeeEquipmentEffectEvaluator::RecordRecovery(Attempt, Worker.EmployeeId,
            bAssisted ? EEquipmentRecoverySource::EquipmentAuto : EEquipmentRecoverySource::Natural, Worker.Loadout);
    }
    Employee.Rest = EManufacturingRest::None;
    Employee.RestRemaining = 0;
    Employee.RestElapsed = 0;
    Employee.RestRecoveryRate = 0;
}

double DownDuration(EManufacturingRest Rest, const FManufacturingWorkerInput& Worker, const UFatigueConfig& Config)
{
    const FName Id = Rest == EManufacturingRest::Drowsy ? TEXT("EQF_DROWSY_RECOVERY") : TEXT("EQF_BOLT_RETURN");
    const float Base = Rest == EManufacturingRest::Drowsy ? Config.SlackSlumpDuration : Config.BoltDuration;
    const float Shortened = Base * (1.f - FMath::Clamp(Worker.Loadout.Basic(Id), 0.f, 100.f) / 100.f);
    const FName MinimumKey = Rest == EManufacturingRest::Drowsy ? TEXT("DrowsyMinimumDuration") : TEXT("BoltMinimumDuration");
    const float Minimum = FMath::Max(Worker.Loadout.Param(Id, TEXT("MinimumDuration")), Worker.Loadout.Rule(MinimumKey));
    return (Rest == EManufacturingRest::Drowsy ? FMath::Max(0.f, Config.SlackTelegraphDuration) : 0.f)
        + FMath::Max(Minimum, Shortened);
}

bool BeginDown(FManufacturingWorkState& WorkState, const FManufacturingWorkerInput& Worker,
    const UFatigueConfig& Config, const TArray<FEquipmentParticipant>& Participants, bool bBolt)
{
    auto& Attempt = WorkState.Equipment;
    auto& Employee = WorkState.Employees.FindChecked(Worker.EmployeeId);
    auto& Runtime = Attempt.Employees.FindChecked(Worker.EmployeeId);
    if (!bBolt) Runtime.bDrowsyArmed = false;
    float Relief = 0;
    if (FEmployeeEquipmentEffectEvaluator::TryVetoDown(Attempt, Worker.EmployeeId, bBolt,
        Worker.Loadout, Participants, Relief))
    {
        ApplyRelief(WorkState, Worker.EmployeeId, Relief);
        return false;
    }
    const FGuid DownId(static_cast<uint32>(WorkState.Period + 1), static_cast<uint32>(Worker.EmployeeId),
        static_cast<uint32>(Runtime.DownCount + 1), static_cast<uint32>(Attempt.EventSequence + 1));
    FEmployeeEquipmentEffectEvaluator::RecordDown(Attempt, Worker.EmployeeId, Worker.PrimarySlot, DownId, Participants);
    Employee.Rest = bBolt ? EManufacturingRest::Away : EManufacturingRest::Drowsy;
    Employee.RestElapsed = 0;
    Employee.RestRemaining = DownDuration(Employee.Rest, Worker, Config);
    const float OriginalRecoverySeconds = bBolt ? Config.BoltDuration : Config.SlackSlumpDuration;
    const float Telegraph = bBolt ? 0.f : FMath::Max(0.f, Config.SlackTelegraphDuration);
    const float ShortenedRecoverySeconds = FMath::Max(0.f, static_cast<float>(Employee.RestRemaining) - Telegraph);
    Employee.RestRecoveryRate = ShortenedRecoverySeconds > KINDA_SMALL_NUMBER
        ? Config.SlumpRecoverPerSec * FMath::Max(0.f, Worker.RecoveryMultiplier) *
            FMath::Max(0.f, OriginalRecoverySeconds) / ShortenedRecoverySeconds
        : 0.f;
    return true;
}
}

bool FManufacturingRecoveryRules::Advance(FManufacturingWorkState& State, const FManufacturingWorkerInput& Worker,
    const UFatigueConfig& Config, float DeltaTime, float BoltRoll,
    const TArray<FEquipmentParticipant>& Participants)
{
    if (Worker.EmployeeId == INDEX_NONE || !FMath::IsFinite(DeltaTime) || DeltaTime <= 0 ||
        !State.Employees.Contains(Worker.EmployeeId) || !State.Equipment.Employees.Contains(Worker.EmployeeId)) return false;

    auto& Employee = State.Employees.FindChecked(Worker.EmployeeId);
    auto& Attempt = State.Equipment;
    auto& Runtime = Attempt.Employees.FindChecked(Worker.EmployeeId);
    FEmployeeEquipmentEffectEvaluator::SyncLoadout(Runtime, Worker.Loadout, false);
    Employee.Fatigue = FMath::Clamp(FMath::IsFinite(Employee.Fatigue) ? Employee.Fatigue : 0.f, 0.f, 100.f);

    if (Employee.Rest != EManufacturingRest::None)
    {
        const double PreviousElapsed = Employee.RestElapsed;
        const double RestEndElapsed = Employee.RestElapsed + Employee.RestRemaining;
        Employee.RestElapsed += DeltaTime;
        Employee.RestRemaining = FMath::Max(0., Employee.RestRemaining - DeltaTime);
        if (Employee.Rest == EManufacturingRest::Breathing)
        {
            if (Employee.RestRemaining <= 0)
            {
                ApplyRelief(State, Worker.EmployeeId, FEmployeeEquipmentEffectEvaluator::ClampInstantRelief(Attempt,
                    Worker.EmployeeId, Worker.Loadout.Param(TEXT("EQF_BREATHING_BREAK"), TEXT("Relief")),
                    Worker.Loadout.Rule(TEXT("InstantReliefCap"))));
                Employee.Rest = EManufacturingRest::None;
                Employee.RestElapsed = 0;
                Employee.RestRecoveryRate = 0;
                Runtime.RestUntil = 0;
            }
            return false;
        }

        const float Telegraph = Employee.Rest == EManufacturingRest::Drowsy ? FMath::Max(0.f, Config.SlackTelegraphDuration) : 0.f;
        const float RecoverySeconds = static_cast<float>(FMath::Max(0.,
            FMath::Min(Employee.RestElapsed, RestEndElapsed) - FMath::Max<double>(PreviousElapsed, Telegraph)));
        const float RawRecovery = -Employee.RestRecoveryRate * RecoverySeconds;
        Employee.Fatigue = FMath::Clamp(Employee.Fatigue + FEmployeeEquipmentEffectEvaluator::ResolveFatigueDelta(Attempt,
            Worker.EmployeeId, RawRecovery, Employee.Fatigue, Config.TiredThreshold, Worker.Loadout), 0.f, 100.f);

        bool bAssisted = false;
        if (Employee.Rest == EManufacturingRest::Drowsy)
        {
            const FName Early(TEXT("EQF_EARLY_CATCH"));
            const auto* EarlyState = Runtime.Effects.Find(Early);
            if (Worker.Loadout.Has(Early) && EarlyState && EarlyState->CooldownUntil <= Attempt.ActiveTime &&
                Worker.Loadout.Param(Early, TEXT("Window")) > 0 &&
                Employee.RestElapsed >= Worker.Loadout.Param(Early, TEXT("Window")) &&
                Employee.RestElapsed <= Worker.Loadout.Param(Early, TEXT("Window")) + UE_KINDA_SMALL_NUMBER)
                bAssisted = true;
            const FName Auto(TEXT("EQF_AUTO_CATCH"));
            auto* AutoState = Runtime.Effects.Find(Auto);
            if (!bAssisted && Worker.Loadout.Has(Auto) && AutoState && AutoState->bStartedWithEffect &&
                AutoState->Charges > 0 && Employee.RestElapsed >= Worker.Loadout.Param(Auto, TEXT("Delay")))
            {
                --AutoState->Charges;
                bAssisted = true;
            }
        }
        if (bAssisted || Employee.RestRemaining <= 0) ReturnFromDown(State, Worker, Config, Participants, bAssisted);
        return false;
    }

    for (FName Id : {FName(TEXT("EQF_FRESH_START")), FName(TEXT("EQF_OPENING_SPRINT"))})
    {
        auto* Effect = Runtime.Effects.Find(Id);
        if (!Worker.Loadout.Has(Id) || !Effect || !Effect->bStartedWithEffect || Effect->bStartConsumed) continue;
        Effect->bStartConsumed = true;
        if (Id == TEXT("EQF_FRESH_START")) ApplyRelief(State, Worker.EmployeeId,
            FEmployeeEquipmentEffectEvaluator::ClampInstantRelief(Attempt, Worker.EmployeeId,
                Worker.Loadout.Param(Id, TEXT("Relief")), Worker.Loadout.Rule(TEXT("InstantReliefCap"))));
        else Effect->ExpiresAt = Attempt.ActiveTime + Worker.Loadout.Param(Id, TEXT("Duration"));
    }

    const FName Break(TEXT("EQF_BREATHING_BREAK"));
    if (Worker.Loadout.Has(Break))
    {
        if (Employee.Fatigue < Worker.Loadout.Param(Break, TEXT("RearmBelow"))) Runtime.bRestArmed = true;
        auto* BreakState = Runtime.Effects.Find(Break);
        if (Runtime.bRestArmed && BreakState && Employee.Fatigue >= Worker.Loadout.Param(Break, TEXT("Threshold")) &&
            BreakState->CooldownUntil <= Attempt.ActiveTime)
        {
            Runtime.bRestArmed = false;
            BreakState->CooldownUntil = Attempt.ActiveTime + Worker.Loadout.Param(Break, TEXT("ICD"));
            if (auto* Rhythm = Runtime.Effects.Find(TEXT("EQF_UNBROKEN_RHYTHM"))) Rhythm->Counter = 0;
            Employee.Rest = EManufacturingRest::Breathing;
            Employee.RestElapsed = 0;
            Employee.RestRemaining = FMath::Max(0.f, Worker.Loadout.Param(Break, TEXT("Duration")));
            Runtime.RestUntil = Attempt.ActiveTime + Employee.RestRemaining;
            return false;
        }
    }

    const float StaminaScale = 1.f / (1.f + FMath::Max(0.f, Worker.Stamina) * Config.StaminaGainScale);
    const float RawGain = Config.GainPerSec * StaminaScale * DeltaTime / (1.f + FMath::Max(0.f, Worker.Environment));
    Employee.Fatigue = FMath::Clamp(Employee.Fatigue + FEmployeeEquipmentEffectEvaluator::ResolveFatigueDelta(Attempt,
        Worker.EmployeeId, RawGain, Employee.Fatigue, Config.TiredThreshold, Worker.Loadout), 0.f, 100.f);
    if (Employee.Fatigue < Config.SlackingThreshold) Runtime.bDrowsyArmed = true;
    if (Employee.Fatigue >= Config.SlackingThreshold && BeginDown(State, Worker, Config, Participants, false)) return false;

    const float Composure = FMath::Max(0.f, Worker.Composure);
    const float HalfPoint = FMath::Max(1.f, Config.MaxBoltReduction / FMath::Max(KINDA_SMALL_NUMBER, Config.ComposureBoltScale));
    const float Reduction = Config.MaxBoltReduction * Composure / (Composure + HalfPoint);
    const float PerSecond = FMath::Clamp(Config.BoltChancePerSec * (1.f - Reduction) *
        (1.f - Worker.Loadout.Basic(TEXT("EQF_BOLT_RESIST")) / 100.f), 0.f, 1.f);
    const float StepChance = 1.f - FMath::Pow(1.f - PerSecond, DeltaTime);
    if (FMath::IsFinite(BoltRoll) && BoltRoll < StepChance && BeginDown(State, Worker, Config, Participants, true)) return false;
    return true;
}
