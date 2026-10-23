#include "Utils/EmployeeEquipmentEffectEvaluator.h"
#include "Misc/ScopeExit.h"

namespace
{
float Parameter(const FEquipmentEffectRuntime& State, FName Key) { return State.Parameters.FindRef(Key); }
const FEquipmentEffectRuntime* EffectState(const FEquipmentAttemptRuntime& Attempt, int32 EmployeeId, FName Id)
{
    const auto* Employee = Attempt.Employees.Find(EmployeeId);
    return Employee ? Employee->Effects.Find(Id) : nullptr;
}
bool Ready(const FEquipmentAttemptRuntime& Attempt, int32 EmployeeId, FName Id)
{
    const auto* State = EffectState(Attempt, EmployeeId, Id);
    return State && State->bEquipped && State->CooldownUntil <= Attempt.ActiveTime;
}
bool Active(const FEquipmentAttemptRuntime& Attempt, int32 EmployeeId, FName Id)
{
    const auto* State = EffectState(Attempt, EmployeeId, Id);
    return State && State->bEquipped && State->ExpiresAt > Attempt.ActiveTime;
}
void Trigger(FEquipmentAttemptRuntime& Attempt, int32 EmployeeId, FName Id)
{
    auto& State = Attempt.Employees.FindChecked(EmployeeId).Effects.FindChecked(Id);
    State.ExpiresAt = Attempt.ActiveTime + Parameter(State, TEXT("Duration"));
    State.CooldownUntil = Attempt.ActiveTime + Parameter(State, TEXT("ICD"));
}
int32 Lowest(const TArray<FEquipmentDisciplineState>& States, int32 Excluded = INDEX_NONE, int32 ExcludedMask = 0, bool bBelowTarget = false)
{
    int32 Result = INDEX_NONE;
    float LowestRatio = TNumericLimits<float>::Max();
    for (const auto& State : States)
    {
        if (State.DisciplineSlot == Excluded || State.DisciplineSlot < 0 || State.DisciplineSlot >= 32 ||
            (ExcludedMask & (1 << State.DisciplineSlot)) || State.Target <= 0 || (bBelowTarget && State.Acquired >= State.Target)) continue;
        const float Ratio = State.Acquired / State.Target;
        if (Ratio < LowestRatio || (Ratio == LowestRatio && State.DisciplineSlot < Result))
        {
            Result = State.DisciplineSlot; LowestRatio = Ratio;
        }
    }
    return Result;
}
int32 ActiveMask(const TArray<FEquipmentDisciplineState>& States)
{
    int32 Mask = 0;
    for (const auto& State : States) if (State.DisciplineSlot >= 0 && State.DisciplineSlot < 32) Mask |= 1 << State.DisciplineSlot;
    return Mask;
}
bool IsActiveSlot(const TArray<FEquipmentDisciplineState>& States, int32 DisciplineSlot)
{
    return States.ContainsByPredicate([DisciplineSlot](const auto& State) { return State.DisciplineSlot == DisciplineSlot && State.Target > 0; });
}
void Merge(FEquipmentEffectLoadout& Destination, const FEquipmentEffectLoadout& Source)
{
    Destination.bValid &= Source.bValid;
    for (const auto& Pair : Source.BasicValues) Destination.BasicValues.FindOrAdd(Pair.Key) += Pair.Value;
    for (const auto& Pair : Source.UniqueEffects)
    {
        auto* Previous = Destination.UniqueEffects.Find(Pair.Key);
        if (!Previous || Pair.Value.RankPower > Previous->RankPower) Destination.UniqueEffects.Add(Pair.Key, Pair.Value);
    }
}
}

float FEquipmentEffectLoadout::Param(FName Id, FName Key) const
{
    const auto* Row = UniqueEffects.Find(Id);
    return Row ? Row->ScalarParameters.FindRef(Key) : 0.f;
}

FEquipmentEffectLoadout FEmployeeEquipmentEffectEvaluator::BuildItemEffects(const FEmployeeEquipmentDefinitionRow& Definition,
    int32 EnhanceLevel, const TMap<FName, FEmployeeEquipmentEffectRow>& Effects, const FEmployeeEquipmentEconomyRow& Economy)
{
    FEquipmentEffectLoadout Result;
    for (const auto& Pair : Effects) if (Pair.Value.bBasic) Result.BasicCaps.Add(Pair.Key, Pair.Value.AggregateCap);
    if (const auto* Rules = Effects.Find(TEXT("EQF_SCORE_GAIN"))) Result.Rules = Rules->ScalarParameters;
    const int32 Grade = static_cast<int32>(Definition.Grade);
    if (EnhanceLevel < 0 || EnhanceLevel > Economy.MaxEnhanceLevel) { Result.bValid = false; return Result; }
    const auto AddBasic = [&](FName Id, float Scale)
    {
        if (Id.IsNone()) return;
        const auto* Row = Effects.Find(Id);
        if (!Row || !Row->bBasic || !Row->GradeValues.IsValidIndex(Grade) || !FMath::IsFinite(Row->GradeValues[Grade]))
        { Result.bValid = false; return; }
        Result.BasicValues.FindOrAdd(Id) += Row->GradeValues[Grade] * Scale;
    };
    AddBasic(Definition.PrimaryEffectId, 1.f + Economy.PrimaryEnhancePerLevel * EnhanceLevel);
    AddBasic(Definition.SecondaryEffectId, Economy.SecondaryScale);
    for (FName Id : Definition.UniqueEffectIds)
    {
        const auto* Row = Effects.Find(Id);
        if (!Row || Row->bBasic) { Result.bValid = false; continue; }
        Result.UniqueEffects.Add(Id, *Row);
    }
    for (auto& Pair : Result.BasicValues) Pair.Value = FMath::Clamp(Pair.Value, 0.f, Result.BasicCaps.FindRef(Pair.Key));
    return Result;
}

FEquipmentEffectLoadout FEmployeeEquipmentEffectEvaluator::BuildLoadout(const FEmployeeEquipmentState& State, int32 EmployeeId,
    const TMap<FName, FEmployeeEquipmentDefinitionRow>& Definitions, const TMap<FName, FEmployeeEquipmentEffectRow>& Effects,
    const FEmployeeEquipmentEconomyRow& Economy, EEmployeeEquipmentSlot ReplaceSlot, const FGuid& CandidateId)
{
    FEquipmentEffectLoadout Result;
    for (const auto& Pair : Effects) if (Pair.Value.bBasic) Result.BasicCaps.Add(Pair.Key, Pair.Value.AggregateCap);
    if (const auto* Rules = Effects.Find(TEXT("EQF_SCORE_GAIN"))) Result.Rules = Rules->ScalarParameters;
    const auto* Slots = State.Loadouts.Find(EmployeeId);
    for (int32 Index = 0; Index < static_cast<int32>(EEmployeeEquipmentSlot::Count); ++Index)
    {
        const auto EquipmentSlot = static_cast<EEmployeeEquipmentSlot>(Index);
        FGuid Id = EquipmentSlot == ReplaceSlot ? CandidateId : (Slots ? Slots->Slots.FindRef(EquipmentSlot) : FGuid());
        if (!Id.IsValid()) continue;
        const auto* Instance = State.Instances.Find(Id);
        const auto* Definition = Instance ? Definitions.Find(Instance->DefinitionId) : nullptr;
        if (!Instance || !Definition || Definition->EquipmentSlot != EquipmentSlot) { Result.bValid = false; continue; }
        Merge(Result, BuildItemEffects(*Definition, Instance->EnhanceLevel, Effects, Economy));
    }
    for (auto& Pair : Result.BasicValues) Pair.Value = FMath::Clamp(Pair.Value, 0.f, Result.BasicCaps.FindRef(Pair.Key));
    return Result;
}

bool FEmployeeEquipmentEffectEvaluator::TryApplyPackets(TArray<FEquipmentDisciplineState>& Disciplines,
    const TArray<FEquipmentContributionPacket>& Packets, float ProjectYield)
{
    if (Packets.IsEmpty() || !FMath::IsFinite(ProjectYield) || ProjectYield <= 0) return false;
    TMap<int32, float> Changes;
    for (const auto& Packet : Packets)
    {
        if (!FMath::IsFinite(Packet.Score) || Packet.Score <= 0 || !IsActiveSlot(Disciplines, Packet.DisciplineSlot)) return false;
        float& Delta = Changes.FindOrAdd(Packet.DisciplineSlot);
        Delta += Packet.Score * ProjectYield;
        if (!FMath::IsFinite(Delta)) return false;
    }
    for (const auto& Discipline : Disciplines)
        if (!FMath::IsFinite(Discipline.Acquired + Changes.FindRef(Discipline.DisciplineSlot))) return false;
    for (auto& Discipline : Disciplines) Discipline.Acquired += Changes.FindRef(Discipline.DisciplineSlot);
    return true;
}

void FEmployeeEquipmentEffectEvaluator::SyncLoadout(FEquipmentEmployeeRuntime& Employee, const FEquipmentEffectLoadout& Loadout, bool bAtAttemptStart)
{
    for (auto& Pair : Employee.Effects)
    {
        if (Pair.Value.bEquipped && !Loadout.Has(Pair.Key) && !Loadout.BasicValues.Contains(Pair.Key))
        {
            Pair.Value.ExpiresAt = 0;
            Pair.Value.TargetSlot = INDEX_NONE;
            Pair.Value.StoredValue = 0;
        }
        Pair.Value.bEquipped = false;
    }
    for (const auto& Pair : Loadout.BasicValues) Employee.Effects.FindOrAdd(Pair.Key).bEquipped = true;
    for (const auto& Pair : Loadout.UniqueEffects)
    {
        auto& State = Employee.Effects.FindOrAdd(Pair.Key);
        State.bEquipped = true;
        State.Parameters = Pair.Value.ScalarParameters;
        if (bAtAttemptStart)
        {
            State.bStartedWithEffect = true;
            State.Charges = FMath::RoundToInt(Parameter(State, TEXT("Charges")));
        }
    }
}

void FEmployeeEquipmentEffectEvaluator::AdvanceTime(FEquipmentAttemptRuntime& Attempt, float DeltaTime,
    const TArray<FEquipmentParticipant>& Participants)
{
    if (Attempt.Outcome != EEquipmentAttemptOutcome::Active || DeltaTime <= 0 || !FMath::IsFinite(DeltaTime)) return;
    Attempt.ActiveTime += DeltaTime;
    for (auto& Pair : Attempt.Employees)
    {
        const auto* Participant = Participants.FindByPredicate([&](const auto& P) { return P.EmployeeId == Pair.Key; });
        if (!Participant)
        {
            SyncLoadout(Pair.Value, FEquipmentEffectLoadout(), false);
            continue;
        }
        SyncLoadout(Pair.Value, Participant->Loadout, false);
        if (Participant->bTyping) Pair.Value.TypedSeconds += DeltaTime;
        for (auto& Effect : Pair.Value.Effects)
        {
            auto& State = Effect.Value;
            if (!State.bEquipped) continue;
            State.SegmentExposure += DeltaTime;
            State.SegmentValueSeconds += Participant->Loadout.Basic(Effect.Key) * DeltaTime;
            if (Participant->bTyping) State.TypedSeconds += DeltaTime;
        }
    }
    Attempt.Supports.RemoveAll([&](const auto& Support) { return Support.ExpiresAt <= Attempt.ActiveTime; });
}

float FEmployeeEquipmentEffectEvaluator::SpeedPercent(const FEquipmentAttemptRuntime& Attempt, int32 EmployeeId,
    const FEquipmentEffectLoadout& Loadout)
{
    float Rate = Loadout.Basic(TEXT("EQF_WORK_RATE"));
    for (FName Id : {FName(TEXT("EQF_OPENING_SPRINT")), FName(TEXT("EQF_UNBROKEN_RHYTHM")),
        FName(TEXT("EQF_CATCH_MOMENTUM")), FName(TEXT("EQF_CRIT_ACCELERATION"))})
        if (Loadout.Has(Id) && Active(Attempt, EmployeeId, Id)) Rate += Loadout.Param(Id, TEXT("SpeedPercent"));
    if (Loadout.Has(TEXT("EQF_FINISHING_SPRINT")) && Attempt.SegmentDuration > 0 &&
        Attempt.SegmentDuration - (Attempt.ActiveTime - Attempt.SegmentStartTime) <=
        Attempt.SegmentDuration * Loadout.Param(TEXT("EQF_FINISHING_SPRINT"), TEXT("RemainingPercent")) / 100.f)
        Rate += Loadout.Param(TEXT("EQF_FINISHING_SPRINT"), TEXT("SpeedPercent"));
    float SupportRate = 0;
    for (const auto& Support : Attempt.Supports)
        if (Support.RecipientEmployeeId == EmployeeId && Support.EffectId == TEXT("EQF_TEAM_PACE") && Support.ExpiresAt > Attempt.ActiveTime)
            SupportRate = FMath::Max(SupportRate, Support.Value);
    return FMath::Clamp(Rate + SupportRate, 0.f, Loadout.BasicCaps.FindRef(TEXT("EQF_WORK_RATE")));
}

float FEmployeeEquipmentEffectEvaluator::PersonalScorePercent(const FEquipmentAttemptRuntime& Attempt,
    const FEquipmentContributionContext& Context, const FEquipmentEffectLoadout& Loadout)
{
    float Rate = Loadout.Basic(TEXT("EQF_SCORE_GAIN"));
    if (Context.ChosenSlot == Context.PrimarySlot) Rate += Loadout.Param(TEXT("EQF_PRIMARY_MASTERY"), TEXT("ScorePercent"));
    else Rate += Loadout.Param(TEXT("EQF_SECONDARY_SPECIALIST"), TEXT("ScorePercent"));
    if (Context.RouteEffect == TEXT("EQF_COMBO_FINISH") || Context.RouteEffect == TEXT("EQF_VACANCY_COVER"))
        Rate += Loadout.Param(Context.RouteEffect, TEXT("ScorePercent"));
    if (Active(Attempt, Context.EmployeeId, TEXT("EQF_FEVER_CONDUCTOR")))
    {
        const auto* State = EffectState(Attempt, Context.EmployeeId, TEXT("EQF_FEVER_CONDUCTOR"));
        if (State && State->Charges > 0) Rate += Loadout.Param(TEXT("EQF_FEVER_CONDUCTOR"), TEXT("ScorePercent"));
    }
    int32 Typing = 0, OtherDown = 0;
    for (const auto& P : Context.Participants)
    {
        Typing += P.bTyping ? 1 : 0;
        OtherDown += (P.EmployeeId != Context.EmployeeId && P.bDown) ? 1 : 0;
    }
    if (Loadout.Has(TEXT("EQF_LAST_WORKER")) && Typing == 1 &&
        OtherDown >= Loadout.Param(TEXT("EQF_LAST_WORKER"), TEXT("MinOtherDown")))
        Rate += Loadout.Param(TEXT("EQF_LAST_WORKER"), TEXT("ScorePercent"));
    return FMath::Clamp(Rate, 0.f, Loadout.BasicCaps.FindRef(TEXT("EQF_SCORE_GAIN")));
}

float FEmployeeEquipmentEffectEvaluator::TeamScorePercent(const FEquipmentContributionContext& Context)
{
    int32 Typing = 0; TSet<int32> Primaries;
    for (const auto& P : Context.Participants)
        if (P.bTyping) { ++Typing; if (IsActiveSlot(Context.Disciplines, P.PrimarySlot)) Primaries.Add(P.PrimarySlot); }
    float Rate = 0;
    for (const auto& P : Context.Participants)
    {
        const FName Id(TEXT("EQF_DIVERSITY_PACE"));
        if (P.bTyping && P.Loadout.Has(Id) && Typing >= P.Loadout.Param(Id, TEXT("MinTyping")) &&
            Primaries.Num() >= P.Loadout.Param(Id, TEXT("MinDisciplines")))
            Rate = FMath::Max(Rate, FMath::Min(P.Loadout.Param(Id, TEXT("ScorePercent")), P.Loadout.Rule(TEXT("TeamScoreCapPercent"))));
    }
    return Rate;
}

float FEmployeeEquipmentEffectEvaluator::CritRatePercent(const FEquipmentAttemptRuntime& Attempt, int32 EmployeeId,
    const FEquipmentEffectLoadout& Loadout)
{
    float Rate = Loadout.Basic(TEXT("EQF_CRIT_RATE"));
    if (Active(Attempt, EmployeeId, TEXT("EQF_PRECISION_CHAIN")))
        Rate += Loadout.Param(TEXT("EQF_PRECISION_CHAIN"), TEXT("CritPercent"));
    return FMath::Clamp(Rate, 0.f, Loadout.BasicCaps.FindRef(TEXT("EQF_CRIT_RATE")));
}

float FEmployeeEquipmentEffectEvaluator::CritPower(const FEquipmentAttemptRuntime& Attempt, int32 EmployeeId,
    const FEquipmentEffectLoadout& Loadout)
{
    float Power = Loadout.Basic(TEXT("EQF_CRIT_POWER"));
    if (Active(Attempt, EmployeeId, TEXT("EQF_CRIT_CONVERSION")))
        Power += EffectState(Attempt, EmployeeId, TEXT("EQF_CRIT_CONVERSION"))->Counter * Loadout.Param(TEXT("EQF_CRIT_CONVERSION"), TEXT("PowerPerStack"));
    return FMath::Clamp(Power, 0.f, Loadout.BasicCaps.FindRef(TEXT("EQF_CRIT_POWER")));
}

FName FEmployeeEquipmentEffectEvaluator::GuaranteedCrit(const FEquipmentAttemptRuntime& Attempt, int32 EmployeeId,
    const FEquipmentEffectLoadout& Loadout)
{
    const auto* Employee = Attempt.Employees.Find(EmployeeId);
    if (!Employee || Employee->NativeCount < Employee->NextGuaranteedEmission) return NAME_None;
    const FName First(TEXT("EQF_FIRST_CRIT"));
    const auto* FirstState = Employee->Effects.Find(First);
    if (Loadout.Has(First) && FirstState && FirstState->bStartedWithEffect && FirstState->Charges > 0) return First;
    const FName Pity(TEXT("EQF_CRIT_PITY"));
    const auto* PityState = Employee->Effects.Find(Pity);
    return Loadout.Has(Pity) && PityState && PityState->Charges > 0 && Active(Attempt, EmployeeId, Pity) ? Pity : NAME_None;
}

float FEmployeeEquipmentEffectEvaluator::GeneralCritChance(float ExistingChance, float ExistingCap,
    const FEquipmentAttemptRuntime& Attempt, int32 EmployeeId, const FEquipmentEffectLoadout& Loadout)
{
    const float Cap = Loadout.Rules.Contains(TEXT("GeneralCritCapPercent")) ? Loadout.Rule(TEXT("GeneralCritCapPercent")) / 100.f : ExistingCap;
    return FMath::Clamp(ExistingChance + CritRatePercent(Attempt, EmployeeId, Loadout) / 100.f, 0.f, Cap);
}

int32 FEmployeeEquipmentEffectEvaluator::SelectRoute(const FEquipmentAttemptRuntime& Attempt, int32 EmployeeId,
    const FEquipmentEffectLoadout& Loadout, const TArray<FEquipmentDisciplineState>& Disciplines, float DeficitRoll, FName& OutRouteEffect)
{
    OutRouteEffect = NAME_None;
    for (FName Id : {FName(TEXT("EQF_MANUAL_PRIORITY")), FName(TEXT("EQF_COMBO_FINISH")),
        FName(TEXT("EQF_COVERAGE_PLEDGE")), FName(TEXT("EQF_VACANCY_COVER"))})
    {
        if (!Loadout.Has(Id)) continue;
        const auto* State = EffectState(Attempt, EmployeeId, Id);
        if (!State) continue;
        int32 Target = INDEX_NONE;
        if (Id == TEXT("EQF_COVERAGE_PLEDGE"))
        {
            if (State->bStartedWithEffect) Target = Lowest(Disciplines, INDEX_NONE, State->VisitedMask);
        }
        else if (State->Charges > 0 && Active(Attempt, EmployeeId, Id) && IsActiveSlot(Disciplines, State->TargetSlot)) Target = State->TargetSlot;
        if (Target != INDEX_NONE) { OutRouteEffect = Id; return Target; }
    }
    const FName Deficit(TEXT("EQF_DEFICIT_ROUTER"));
    if (Disciplines.Num() >= 2 && Loadout.Has(Deficit) && DeficitRoll < Loadout.Param(Deficit, TEXT("ChancePercent")) / 100.f)
    { OutRouteEffect = Deficit; return Lowest(Disciplines); }
    return INDEX_NONE;
}

bool FEmployeeEquipmentEffectEvaluator::RequestManualPriority(FEquipmentAttemptRuntime& Attempt, int32 EmployeeId,
    int32 DisciplineSlot, const FEquipmentEffectLoadout& Loadout, const TArray<FEquipmentDisciplineState>& Disciplines)
{
    const FName Id(TEXT("EQF_MANUAL_PRIORITY"));
    if (Attempt.Outcome != EEquipmentAttemptOutcome::Active || !Loadout.Has(Id) || !IsActiveSlot(Disciplines, DisciplineSlot) ||
        !Ready(Attempt, EmployeeId, Id)) return false;
    auto& State = Attempt.Employees.FindChecked(EmployeeId).Effects.FindChecked(Id);
    Trigger(Attempt, EmployeeId, Id);
    State.TargetSlot = DisciplineSlot;
    State.Charges = FMath::RoundToInt(Loadout.Param(Id, TEXT("Charges")));
    return true;
}

TArray<FEquipmentContributionPacket> FEmployeeEquipmentEffectEvaluator::PlanPackets(const FEquipmentAttemptRuntime& Attempt,
    const FEquipmentContributionContext& Context, const FEquipmentEffectLoadout& Loadout, float BonusCapPercent)
{
    TArray<FEquipmentContributionPacket> Packets;
    if (Context.NativeQ <= 0 || Context.ProjectYield <= 0 || !IsActiveSlot(Context.Disciplines, Context.ChosenSlot)) return Packets;
    Packets.Add({Context.ChosenSlot, Context.NativeQ, false});
    const int32 Other = Lowest(Context.Disciplines, Context.ChosenSlot);
    if (Other != INDEX_NONE && Loadout.Has(TEXT("EQF_DUAL_DELIVERY")))
    {
        const float Transfer = Context.NativeQ * Loadout.Param(TEXT("EQF_DUAL_DELIVERY"), TEXT("TransferPercent")) / 100.f;
        Packets[0].Score -= Transfer;
        if (Transfer > 0) Packets.Add({Other, Transfer, false});
    }
    const auto* Target = Context.Disciplines.FindByPredicate([&](const auto& S) { return S.DisciplineSlot == Context.ChosenSlot; });
    const int32 UnfilledOther = Lowest(Context.Disciplines, Context.ChosenSlot, 0, true);
    if (Target && UnfilledOther != INDEX_NONE && Loadout.Has(TEXT("EQF_OVERFLOW_RELAY")))
    {
        const float Overflow = FMath::Max(0.f, Packets[0].Score - FMath::Max(0.f, Target->Target - Target->Acquired) / Context.ProjectYield);
        const float Transfer = Overflow * Loadout.Param(TEXT("EQF_OVERFLOW_RELAY"), TEXT("TransferPercent")) / 100.f;
        Packets[0].Score -= Transfer;
        if (Transfer > 0) Packets.Add({UnfilledOther, Transfer, false});
    }
    TArray<FEquipmentContributionPacket> Bonuses;
    if (Target && Loadout.Has(TEXT("EQF_GATE_RESCUE")))
    {
        float OriginalToTarget = 0;
        for (const auto& Packet : Packets) if (Packet.DisciplineSlot == Context.ChosenSlot) OriginalToTarget += Packet.Score;
        const float Deficit = (Target->Minimum - Target->Acquired) / Context.ProjectYield - OriginalToTarget;
        const float Bonus = FMath::Min(FMath::Max(0.f, Deficit), Context.NativeQ * Loadout.Param(TEXT("EQF_GATE_RESCUE"), TEXT("ScorePercent")) / 100.f);
        if (Bonus > 0) Bonuses.Add({Context.ChosenSlot, Bonus, true});
    }
    if (Context.bCritical && Loadout.Has(TEXT("EQF_CRIT_ECHO")) && Ready(Attempt, Context.EmployeeId, TEXT("EQF_CRIT_ECHO")))
        Bonuses.Add({Context.ChosenSlot, Context.NativeQ * Loadout.Param(TEXT("EQF_CRIT_ECHO"), TEXT("ScorePercent")) / 100.f, true});
    const FName Relay(TEXT("EQF_TEAM_RELAY"));
    const auto* RelayState = EffectState(Attempt, Context.EmployeeId, Relay);
    if (Loadout.Has(Relay) && RelayState && Ready(Attempt, Context.EmployeeId, Relay) && Attempt.LastNativeEmployee != Context.EmployeeId &&
        Attempt.LastNativeEmployee != INDEX_NONE && Attempt.LastNativeSlot != Context.ChosenSlot &&
        Attempt.ActiveTime - Attempt.LastNativeTime <= Loadout.Param(Relay, TEXT("Window")) &&
        Attempt.LastNativeSequence > RelayState->ConsumedEventSequence && IsActiveSlot(Context.Disciplines, Attempt.LastNativeSlot))
    {
        TArray<FEquipmentDisciplineState> Pair;
        for (const auto& S : Context.Disciplines) if (S.DisciplineSlot == Context.ChosenSlot || S.DisciplineSlot == Attempt.LastNativeSlot) Pair.Add(S);
        Bonuses.Add({Lowest(Pair), Context.NativeQ * Loadout.Param(Relay, TEXT("ScorePercent")) / 100.f, true});
    }
    float Requested = 0;
    for (const auto& Bonus : Bonuses) Requested += Bonus.Score;
    const float Scale = Requested > 0 ? FMath::Min(1.f, Context.NativeQ * BonusCapPercent / 100.f / Requested) : 0;
    for (auto Bonus : Bonuses) { Bonus.Score *= Scale; if (Bonus.Score > 0) Packets.Add(Bonus); }
    return Packets;
}

float FEmployeeEquipmentEffectEvaluator::ClampInstantRelief(FEquipmentAttemptRuntime& Attempt, int32 EmployeeId, float Requested, float Cap)
{
    auto* Employee = Attempt.Employees.Find(EmployeeId);
    if (!Employee) return 0;
    const int64 Bucket = FMath::FloorToInt64(Attempt.ActiveTime);
    if (Employee->ReliefBucket != Bucket) { Employee->ReliefBucket = Bucket; Employee->ReliefInBucket = 0; }
    const float Allowed = FMath::Clamp(Requested, 0.f, FMath::Max(0.f, Cap - Employee->ReliefInBucket));
    Employee->ReliefInBucket += Allowed;
    return Allowed;
}

void FEmployeeEquipmentEffectEvaluator::CommitNative(FEquipmentAttemptRuntime& Attempt,
    const FEquipmentContributionContext& Context, const FEquipmentEffectLoadout& Loadout, TArray<FEquipmentReliefAction>& OutRelief)
{
    auto* Employee = Attempt.Employees.Find(Context.EmployeeId);
    if (!Employee || Attempt.CommittedRootEvents.Contains(Context.RootEventId)) return;
    Attempt.CommittedRootEvents.Add(Context.RootEventId);
    ++Attempt.EventSequence;
    ++Employee->NativeCount;
    Employee->LastNativeTarget = Context.ChosenSlot;
    if (Loadout.Has(TEXT("EQF_COVERAGE_PLEDGE")))
    {
        auto& Coverage = Employee->Effects.FindChecked(TEXT("EQF_COVERAGE_PLEDGE"));
        if (Coverage.bStartedWithEffect) Coverage.VisitedMask |= 1 << Context.ChosenSlot;
    }
    if (!Context.GuaranteedEffect.IsNone())
    {
        auto& Token = Employee->Effects.FindChecked(Context.GuaranteedEffect);
        Token.Charges = FMath::Max(0, Token.Charges - 1);
        Employee->NextGuaranteedEmission = Employee->NativeCount + FMath::RoundToInt(Loadout.Rule(TEXT("GuaranteedGap"))) - 1;
    }
    for (auto& Pair : Employee->Effects)
    {
        if (!Pair.Value.bEquipped) continue;
        ++Pair.Value.NativeCount;
        Pair.Value.DisciplineCounts.FindOrAdd(Context.ChosenSlot)++;
    }
    if (!Context.RouteEffect.IsNone())
    {
        if (auto* Route = Employee->Effects.Find(Context.RouteEffect))
        {
            if (Context.RouteEffect == TEXT("EQF_COVERAGE_PLEDGE")) Route->VisitedMask |= 1 << Context.ChosenSlot;
            else Route->Charges = FMath::Max(0, Route->Charges - 1);
            if (Context.RouteEffect == TEXT("EQF_COMBO_FINISH")) Route->VisitedMask = 0;
        }
    }
    const FName Pity(TEXT("EQF_CRIT_PITY"));
    if (Loadout.Has(Pity))
    {
        auto& State = Employee->Effects.FindChecked(Pity);
        if (State.ExpiresAt <= Attempt.ActiveTime) State.Charges = 0;
        State.Counter = Context.bCritical ? 0 : State.Counter + 1;
        if (State.Counter >= Loadout.Param(Pity, TEXT("Count")) && State.Charges == 0)
        { State.Charges = 1; State.ExpiresAt = Attempt.ActiveTime + Loadout.Param(Pity, TEXT("Duration")); State.Counter = 0; }
    }
    const FName Conversion(TEXT("EQF_CRIT_CONVERSION"));
    if (Loadout.Has(Conversion))
    {
        auto& State = Employee->Effects.FindChecked(Conversion);
        if (State.ExpiresAt <= Attempt.ActiveTime) State.Counter = 0;
        if (Context.bCritical) { State.Counter = 0; State.ExpiresAt = 0; }
        else
        {
            if (State.Counter == 0) State.ExpiresAt = Attempt.ActiveTime + Loadout.Param(Conversion, TEXT("Duration"));
            State.Counter = FMath::Min(State.Counter + 1, FMath::RoundToInt(Loadout.Param(Conversion, TEXT("MaxStacks"))));
        }
    }
    for (FName Id : {FName(TEXT("EQF_UNBROKEN_RHYTHM")), FName(TEXT("EQF_TEAM_STABILITY"))})
    {
        if (!Loadout.Has(Id) || !Ready(Attempt, Context.EmployeeId, Id) || Active(Attempt, Context.EmployeeId, Id)) continue;
        auto& State = Employee->Effects.FindChecked(Id);
        if (++State.Counter >= Loadout.Param(Id, TEXT("Count")))
        { State.Counter = 0; Trigger(Attempt, Context.EmployeeId, Id); if (Id == TEXT("EQF_TEAM_STABILITY")) State.Charges = 1; }
    }
    const FName Precision(TEXT("EQF_PRECISION_CHAIN"));
    if (Loadout.Has(Precision))
    {
        auto& State = Employee->Effects.FindChecked(Precision);
        if (State.ExpiresAt > Attempt.ActiveTime)
        { State.ExpiresAt = 0; State.RecentSlots.Reset(); }
        else if (Ready(Attempt, Context.EmployeeId, Precision))
        {
            State.RecentSlots.Add(Context.ChosenSlot);
            const int32 Required = FMath::RoundToInt(Loadout.Param(Precision, TEXT("Count")));
            while (State.RecentSlots.Num() > Required && !State.RecentSlots.IsEmpty()) State.RecentSlots.RemoveAt(0);
            TSet<int32> Unique; for (int32 Value : State.RecentSlots) Unique.Add(Value);
            if (Required > 0 && Unique.Num() == Required) Trigger(Attempt, Context.EmployeeId, Precision);
        }
    }
    const FName Combo(TEXT("EQF_COMBO_FINISH"));
    if (Loadout.Has(Combo) && Context.RouteEffect != Combo)
    {
        auto& State = Employee->Effects.FindChecked(Combo);
        if (Ready(Attempt, Context.EmployeeId, Combo) && !Active(Attempt, Context.EmployeeId, Combo))
        {
            State.VisitedMask |= 1 << Context.ChosenSlot;
            const int32 Mask = ActiveMask(Context.Disciplines);
            if (Context.Disciplines.Num() >= Loadout.Param(Combo, TEXT("MinimumDisciplines")) &&
                (State.VisitedMask & Mask) == Mask && IsActiveSlot(Context.Disciplines, Context.PrimarySlot))
            { Trigger(Attempt, Context.EmployeeId, Combo); State.Charges = 1; State.TargetSlot = Context.PrimarySlot; }
        }
    }
    const FName Fever(TEXT("EQF_FEVER_CONDUCTOR"));
    if (Loadout.Has(Fever) && Active(Attempt, Context.EmployeeId, Fever))
        Employee->Effects.FindChecked(Fever).Charges = FMath::Max(0, Employee->Effects.FindChecked(Fever).Charges - 1);
    if (Context.bCritical)
    {
        for (FName Id : {FName(TEXT("EQF_CRIT_ECHO")), FName(TEXT("EQF_CRIT_RELIEF")), FName(TEXT("EQF_CRIT_ACCELERATION"))})
        {
            if (!Loadout.Has(Id) || !Ready(Attempt, Context.EmployeeId, Id)) continue;
            Trigger(Attempt, Context.EmployeeId, Id);
            if (Id == TEXT("EQF_CRIT_RELIEF")) OutRelief.Add({Context.EmployeeId,
                ClampInstantRelief(Attempt, Context.EmployeeId, Loadout.Param(Id, TEXT("Relief")), Loadout.Rule(TEXT("InstantReliefCap")))});
        }
        const FName Pace(TEXT("EQF_TEAM_PACE"));
        if (Loadout.Has(Pace) && Ready(Attempt, Context.EmployeeId, Pace))
        {
            TArray<int32> Targets;
            for (const auto& P : Context.Participants) if (P.EmployeeId != Context.EmployeeId && P.bTyping) Targets.Add(P.EmployeeId);
            Targets.Sort();
            if (!Targets.IsEmpty())
            {
                auto& State = Employee->Effects.FindChecked(Pace);
                int32 Target = Targets[0];
                for (int32 Candidate : Targets) if (Candidate > State.LastRecipient) { Target = Candidate; break; }
                Trigger(Attempt, Context.EmployeeId, Pace); State.LastRecipient = Target;
                FEquipmentSupportRuntime Support;
                Support.EffectId = Pace; Support.ProviderEmployeeId = Context.EmployeeId; Support.RecipientEmployeeId = Target;
                Support.Value = Loadout.Param(Pace, TEXT("SpeedPercent")); Support.ExpiresAt = State.ExpiresAt;
                Attempt.Supports.Add(Support);
            }
        }
    }
    const FName Relay(TEXT("EQF_TEAM_RELAY"));
    if (Loadout.Has(Relay) && Ready(Attempt, Context.EmployeeId, Relay) && Attempt.LastNativeEmployee != Context.EmployeeId &&
        Attempt.LastNativeEmployee != INDEX_NONE && Attempt.LastNativeSlot != Context.ChosenSlot &&
        Attempt.ActiveTime - Attempt.LastNativeTime <= Loadout.Param(Relay, TEXT("Window")) &&
        IsActiveSlot(Context.Disciplines, Attempt.LastNativeSlot))
    {
        auto& State = Employee->Effects.FindChecked(Relay);
        if (Attempt.LastNativeSequence > State.ConsumedEventSequence)
        { State.ConsumedEventSequence = Attempt.LastNativeSequence; Trigger(Attempt, Context.EmployeeId, Relay); }
    }
    Attempt.LastNativeSequence = Attempt.EventSequence;
    Attempt.LastNativeEmployee = Context.EmployeeId; Attempt.LastNativeSlot = Context.ChosenSlot; Attempt.LastNativeTime = Attempt.ActiveTime;
}

bool FEmployeeEquipmentEffectEvaluator::TryVetoDown(FEquipmentAttemptRuntime& Attempt, int32 EmployeeId, bool bBolted,
    const FEquipmentEffectLoadout& Loadout, const TArray<FEquipmentParticipant>& Participants, float& OutRelief)
{
    OutRelief = 0;
    if (Loadout.Has(bBolted ? TEXT("EQF_BOLT_IMMUNE") : TEXT("EQF_DROWSY_IMMUNE"))) return true;
    if (Active(Attempt, EmployeeId, TEXT("EQF_RECOVERY_SHIELD"))) return true;
    const FName Guard = bBolted ? TEXT("EQF_BOLT_GUARD") : TEXT("EQF_DROWSY_GUARD");
    if (Loadout.Has(Guard))
    {
        auto& State = Attempt.Employees.FindChecked(EmployeeId).Effects.FindChecked(Guard);
        if (State.bStartedWithEffect && State.Charges > 0)
        {
            --State.Charges;
            OutRelief = ClampInstantRelief(Attempt, EmployeeId, Loadout.Param(Guard, TEXT("Relief")), Loadout.Rule(TEXT("InstantReliefCap")));
            return true;
        }
    }
    if (!bBolted) return false;
    TArray<int32> Providers;
    const FName Team(TEXT("EQF_TEAM_STABILITY"));
    for (const auto& P : Participants)
        if (P.EmployeeId != EmployeeId && P.bSeated && P.Loadout.Has(Team) && Active(Attempt, P.EmployeeId, Team))
        {
            const auto* State = EffectState(Attempt, P.EmployeeId, Team);
            if (State && State->Charges > 0) Providers.Add(P.EmployeeId);
        }
    Providers.Sort();
    if (Providers.IsEmpty()) return false;
    --Attempt.Employees.FindChecked(Providers[0]).Effects.FindChecked(Team).Charges;
    return true;
}

void FEmployeeEquipmentEffectEvaluator::RecordDown(FEquipmentAttemptRuntime& Attempt, int32 EmployeeId, int32 PrimarySlot,
    const FGuid& DownId, const TArray<FEquipmentParticipant>& Participants)
{
    auto* Employee = Attempt.Employees.Find(EmployeeId);
    if (!Employee || Employee->DownId == DownId) return;
    ++Attempt.EventSequence; ++Employee->DownCount;
    Employee->DownId = DownId; Employee->DownStartedAt = Attempt.ActiveTime; Employee->RestUntil = 0;
    if (auto* Rhythm = Employee->Effects.Find(TEXT("EQF_UNBROKEN_RHYTHM"))) Rhythm->Counter = 0;
    const FName Vacancy(TEXT("EQF_VACANCY_COVER"));
    for (const auto& P : Participants)
    {
        if (P.EmployeeId == EmployeeId || !P.bSeated || !P.Loadout.Has(Vacancy) || !Ready(Attempt, P.EmployeeId, Vacancy) ||
            Active(Attempt, P.EmployeeId, Vacancy) || PrimarySlot == INDEX_NONE) continue;
        auto& State = Attempt.Employees.FindChecked(P.EmployeeId).Effects.FindChecked(Vacancy);
        Trigger(Attempt, P.EmployeeId, Vacancy); State.TargetSlot = PrimarySlot; State.Charges = 1;
    }
}

void FEmployeeEquipmentEffectEvaluator::RecordRecovery(FEquipmentAttemptRuntime& Attempt, int32 EmployeeId,
    EEquipmentRecoverySource Source, const FEquipmentEffectLoadout& Loadout)
{
    auto* Employee = Attempt.Employees.Find(EmployeeId);
    if (!Employee || !Employee->DownId.IsValid()) return;
    Employee->DownId.Invalidate();
    if (Source == EEquipmentRecoverySource::ModeExit) return;
    const FName Shield(TEXT("EQF_RECOVERY_SHIELD"));
    if (Loadout.Has(Shield) && Ready(Attempt, EmployeeId, Shield) && !Active(Attempt, EmployeeId, Shield)) Trigger(Attempt, EmployeeId, Shield);
}

bool FEmployeeEquipmentEffectEvaluator::RecordManualCatch(FEquipmentAttemptRuntime& Attempt, int32 EmployeeId,
    const FGuid& DownId, float BaseRelief, float Fatigue, const FEquipmentEffectLoadout& Loadout,
    const TArray<FEquipmentParticipant>& Participants, float& OutSelfRelief, TArray<FEquipmentReliefAction>& OutTeamRelief)
{
    OutSelfRelief = 0;
    auto* Employee = Attempt.Employees.Find(EmployeeId);
    if (!Employee || !DownId.IsValid() || Employee->DownId != DownId || Employee->SeenManualDownIds.Contains(DownId)) return false;
    Employee->SeenManualDownIds.Add(DownId); ++Attempt.EventSequence;
    OutSelfRelief = BaseRelief + Loadout.Basic(TEXT("EQF_TAP_RELIEF"));
    const FName Early(TEXT("EQF_EARLY_CATCH"));
    if (Loadout.Has(Early) && Ready(Attempt, EmployeeId, Early) &&
        Attempt.ActiveTime - Employee->DownStartedAt <= Loadout.Param(Early, TEXT("Window")) + UE_KINDA_SMALL_NUMBER)
    {
        Trigger(Attempt, EmployeeId, Early);
        OutSelfRelief += ClampInstantRelief(Attempt, EmployeeId, Loadout.Param(Early, TEXT("Relief")), Loadout.Rule(TEXT("InstantReliefCap")));
    }
    const FName Momentum(TEXT("EQF_CATCH_MOMENTUM"));
    if (Loadout.Has(Momentum) && Ready(Attempt, EmployeeId, Momentum) && !Active(Attempt, EmployeeId, Momentum)) Trigger(Attempt, EmployeeId, Momentum);
    const FName Reserve(TEXT("EQF_RELIEF_RESERVE"));
    if (Loadout.Has(Reserve) && OutSelfRelief > Fatigue)
    {
        auto& State = Employee->Effects.FindChecked(Reserve);
        const float Previous = Active(Attempt, EmployeeId, Reserve) ? State.StoredValue : 0;
        State.StoredValue = FMath::Min(Loadout.Param(Reserve, TEXT("Capacity")), Previous + OutSelfRelief - Fatigue);
        State.ExpiresAt = Attempt.ActiveTime + Loadout.Param(Reserve, TEXT("Duration"));
    }
    if (Loadout.Has(TEXT("EQF_CATCH_LEARNING"))) ++Employee->Effects.FindChecked(TEXT("EQF_CATCH_LEARNING")).CatchCount;
    const FName Refresh(TEXT("EQF_TEAM_REFRESH"));
    if (Loadout.Has(Refresh) && Ready(Attempt, EmployeeId, Refresh))
    {
        TArray<FEquipmentParticipant> Targets;
        for (const auto& P : Participants) if (P.EmployeeId != EmployeeId && P.bSeated) Targets.Add(P);
        Targets.Sort([](const auto& A, const auto& B) { return A.Fatigue != B.Fatigue ? A.Fatigue > B.Fatigue : A.EmployeeId < B.EmployeeId; });
        const int32 Count = FMath::Min(Targets.Num(), FMath::RoundToInt(Loadout.Param(Refresh, TEXT("Targets"))));
        if (Count > 0) Trigger(Attempt, EmployeeId, Refresh);
        for (int32 Index = 0; Index < Count; ++Index)
            OutTeamRelief.Add({Targets[Index].EmployeeId, ClampInstantRelief(Attempt, Targets[Index].EmployeeId,
                Loadout.Param(Refresh, TEXT("Relief")), Loadout.Rule(TEXT("InstantReliefCap")))});
    }
    return true;
}

float FEmployeeEquipmentEffectEvaluator::ResolveFatigueDelta(FEquipmentAttemptRuntime& Attempt, int32 EmployeeId,
    float Delta, float Fatigue, float TiredThreshold, const FEquipmentEffectLoadout& Loadout)
{
    if (Delta < 0) return Delta * (1.f + Loadout.Basic(TEXT("EQF_FATIGUE_RECOVERY")) / 100.f);
    float Reduction = Loadout.Basic(TEXT("EQF_FATIGUE_GAIN"));
    if (Fatigue >= TiredThreshold) Reduction += Loadout.Param(TEXT("EQF_SLOW_BURN"), TEXT("ReductionPercent"));
    float Resolved = Delta * (1.f - FMath::Clamp(Reduction, 0.f, Loadout.BasicCaps.FindRef(TEXT("EQF_FATIGUE_GAIN"))) / 100.f);
    const FName Reserve(TEXT("EQF_RELIEF_RESERVE"));
    if (Loadout.Has(Reserve) && Active(Attempt, EmployeeId, Reserve))
    {
        auto& State = Attempt.Employees.FindChecked(EmployeeId).Effects.FindChecked(Reserve);
        const float Absorbed = FMath::Min(Resolved, State.StoredValue);
        State.StoredValue -= Absorbed; Resolved -= Absorbed;
    }
    return Resolved;
}

void FEmployeeEquipmentEffectEvaluator::RecordFever(FEquipmentAttemptRuntime& Attempt, const TArray<FEquipmentParticipant>& Participants)
{
    const FName Fever(TEXT("EQF_FEVER_CONDUCTOR"));
    for (const auto& P : Participants)
    {
        if (!P.bTyping || !P.Loadout.Has(Fever) || !Ready(Attempt, P.EmployeeId, Fever)) continue;
        Trigger(Attempt, P.EmployeeId, Fever);
        Attempt.Employees.FindChecked(P.EmployeeId).Effects.FindChecked(Fever).Charges = FMath::RoundToInt(P.Loadout.Param(Fever, TEXT("Charges")));
    }
}

bool FEmployeeEquipmentEffectEvaluator::RecordBaselineXP(FEquipmentAttemptRuntime& Attempt, int32 EmployeeId,
    int32 BucketIndex, float BaselineXP, float SelfCapPercent, FEquipmentXPGrant& OutGrant)
{
    OutGrant = FEquipmentXPGrant();
    auto* Employee = Attempt.Employees.Find(EmployeeId);
    if (!Employee || BaselineXP < 0 || !FMath::IsFinite(BaselineXP) || Attempt.Outcome != EEquipmentAttemptOutcome::Active) return false;
    const FString Id = FString::Printf(TEXT("%s:%d:%d:base:%d"), *Attempt.AttemptId.ToString(), EmployeeId, Attempt.SegmentId, BucketIndex);
    if (Attempt.XPReceipts.Contains(Id)) return false;
    const double Duration = Attempt.ActiveTime - Attempt.SegmentStartTime;
    float EquipmentXP = 0;
    if (Duration > 0)
    {
        for (auto& Pair : Employee->Effects)
        {
            auto& State = Pair.Value;
            State.EligibleBaseXP += BaselineXP * FMath::Clamp(State.SegmentExposure / Duration, 0., 1.);
            if (Pair.Key == TEXT("EQF_EXP_GAIN")) EquipmentXP = BaselineXP * State.SegmentValueSeconds / Duration / 100.;
        }
    }
    Employee->BaselineXP += BaselineXP;
    EquipmentXP = FMath::Min(EquipmentXP, static_cast<float>(FMath::Max(0., Employee->BaselineXP * SelfCapPercent / 100. - Employee->SelfEquipmentXP)));
    Employee->SelfEquipmentXP += EquipmentXP;
    OutGrant.GrantId = Id; OutGrant.EmployeeId = EmployeeId; OutGrant.BaselineAmount = BaselineXP;
    OutGrant.EquipmentAmount = EquipmentXP; OutGrant.EffectId = TEXT("EQF_EXP_GAIN");
    Attempt.XPReceipts.Add(Id, OutGrant);
    return true;
}

TArray<FEquipmentXPGrant> FEmployeeEquipmentEffectEvaluator::FinalizeXP(FEquipmentAttemptRuntime& Attempt,
    EEquipmentAttemptOutcome Outcome, const TArray<FEquipmentDisciplineState>& Disciplines, float SelfCapPercent, float MentorCapPercent,
    bool bAutomaticWorkPeriod)
{
    TArray<FEquipmentXPGrant> Grants;
    if (Attempt.Outcome != EEquipmentAttemptOutcome::Active || Outcome == EEquipmentAttemptOutcome::Active) return Grants;
    Attempt.Outcome = Outcome;
    ON_SCOPE_EXIT
    {
        // 멘토를 포함한 모든 정산이 끝난 뒤 임시 이력만 해제하고 결과와 영수증은 보존한다.
        Attempt.Employees.Empty();
        Attempt.Supports.Empty();
        Attempt.CommittedRootEvents.Empty();
    };
    if (Outcome == EEquipmentAttemptOutcome::Aborted) return Grants;
    const auto AppendGrant = [&](int32 EmployeeId, FName EffectId, float Amount)
    {
        if (Amount <= 0 || !FMath::IsFinite(Amount)) return;
        FEquipmentXPGrant Grant;
        Grant.GrantId = FString::Printf(TEXT("%s:%d:final:%s"), *Attempt.AttemptId.ToString(), EmployeeId, *EffectId.ToString());
        if (Attempt.XPReceipts.Contains(Grant.GrantId)) return;
        Grant.EmployeeId = EmployeeId; Grant.EffectId = EffectId; Grant.EquipmentAmount = Amount;
        Attempt.XPReceipts.Add(Grant.GrantId, Grant); Grants.Add(Grant);
    };
    TArray<int32> EmployeeIds;
    Attempt.Employees.GetKeys(EmployeeIds); EmployeeIds.Sort();
    for (int32 Id : EmployeeIds)
    {
        auto& Employee = Attempt.Employees.FindChecked(Id);
        TMap<FName, float> Requests;
        for (const auto& Pair : Employee.Effects)
        {
            const FName Effect = Pair.Key; const auto& State = Pair.Value;
            float Percent = 0;
            if (Effect == TEXT("EQF_ACTIVE_LEARNING"))
            {
                const int32 PerTier = FMath::RoundToInt(Parameter(State, TEXT("CountPerTier")));
                if (PerTier > 0) Percent = FMath::Min(State.NativeCount / PerTier, FMath::RoundToInt(Parameter(State, TEXT("MaxTiers")))) * Parameter(State, TEXT("XPPercentPerTier"));
            }
            else if (Effect == TEXT("EQF_CATCH_LEARNING")) Percent = FMath::Min(State.CatchCount,
                FMath::RoundToInt(Parameter(State, TEXT("MaxCatches")))) * Parameter(State, TEXT("XPPercentPerCatch"));
            else if (Effect == TEXT("EQF_CLEAN_RUN_BONUS") || Effect == TEXT("EQF_FAILURE_LESSON"))
            {
                const bool bResult = Effect == TEXT("EQF_CLEAN_RUN_BONUS") ?
                    Outcome == EEquipmentAttemptOutcome::Launched && Employee.DownCount == 0 :
                    (bAutomaticWorkPeriod ? Employee.DownCount > 0 : Outcome == EEquipmentAttemptOutcome::Failed)
                    && Attempt.CompletedSegments > 0;
                if (bResult && State.bStartedWithEffect && Employee.bStartParticipant && Attempt.ActiveTime > 0 &&
                    State.TypedSeconds / Attempt.ActiveTime >= Parameter(State, TEXT("TypedPercent")) / 100. &&
                    State.NativeCount >= Parameter(State, TEXT("MinContributions"))) Percent = Parameter(State, TEXT("XPPercent"));
            }
            else if (Effect == TEXT("EQF_MULTISKILL_LESSON") && Outcome == EEquipmentAttemptOutcome::Launched &&
                Disciplines.Num() >= Parameter(State, TEXT("MinDisciplines")))
            {
                bool bComplete = true;
                for (const auto& D : Disciplines) bComplete &= State.DisciplineCounts.FindRef(D.DisciplineSlot) >= Parameter(State, TEXT("PerDisciplineCount"));
                if (bComplete) Percent = Parameter(State, TEXT("XPPercent"));
            }
            if (Percent > 0) Requests.Add(Effect, State.EligibleBaseXP * Percent / 100.);
        }
        float Requested = 0; for (const auto& Pair : Requests) Requested += Pair.Value;
        const float Budget = FMath::Max(0., Employee.BaselineXP * SelfCapPercent / 100. - Employee.SelfEquipmentXP);
        const float Scale = Requested > 0 ? FMath::Min(1.f, Budget / Requested) : 0;
        for (const auto& Pair : Requests) { AppendGrant(Id, Pair.Key, Pair.Value * Scale); Employee.SelfEquipmentXP += Pair.Value * Scale; }
    }
    if (Outcome == EEquipmentAttemptOutcome::Launched)
    {
        TMap<int32, float> MentorRequests;
        const FName Mentor(TEXT("EQF_MENTOR_LINK"));
        for (int32 Id : EmployeeIds)
        {
            const auto& Provider = Attempt.Employees.FindChecked(Id);
            const auto* Effect = Provider.Effects.Find(Mentor);
            if (!Effect || !Effect->bStartedWithEffect || !Provider.bStartParticipant || Effect->EligibleBaseXP <= 0) continue;
            int32 RecipientId = INDEX_NONE; int32 LowestLevel = MAX_int32;
            for (int32 CandidateId : EmployeeIds)
            {
                const auto& Candidate = Attempt.Employees.FindChecked(CandidateId);
                if (CandidateId == Id || Candidate.StartLevel >= Provider.StartLevel || Candidate.BaselineXP <= 0 || Attempt.ActiveTime <= 0 ||
                    Candidate.TypedSeconds / Attempt.ActiveTime < Parameter(*Effect, TEXT("TypedPercent")) / 100. ||
                    Candidate.NativeCount < Parameter(*Effect, TEXT("MinContributions"))) continue;
                if (Candidate.StartLevel < LowestLevel) { LowestLevel = Candidate.StartLevel; RecipientId = CandidateId; }
            }
            if (RecipientId != INDEX_NONE) MentorRequests.FindOrAdd(RecipientId) += Effect->EligibleBaseXP * Parameter(*Effect, TEXT("XPPercent")) / 100.;
        }
        for (const auto& Pair : MentorRequests)
        {
            auto& Employee = Attempt.Employees.FindChecked(Pair.Key);
            const float Amount = FMath::Min(Pair.Value, static_cast<float>(FMath::Max(0., Employee.BaselineXP * MentorCapPercent / 100. - Employee.MentorXP)));
            AppendGrant(Pair.Key, Mentor, Amount); Employee.MentorXP += Amount;
        }
    }
    return Grants;
}
