#pragma once
#include "CoreMinimal.h"
#include "Data/EmployeeEquipmentData.h"
#include "Table/EmployeeEquipmentTable.h"

struct FEquipmentEffectLoadout
{
    TMap<FName, float> BasicValues;
    TMap<FName, float> BasicCaps;
    TMap<FName, float> Rules;
    TMap<FName, FEmployeeEquipmentEffectRow> UniqueEffects;
    bool bValid = true;
    float Basic(FName Id) const { return BasicValues.FindRef(Id); }
    bool Has(FName Id) const { return UniqueEffects.Contains(Id); }
    float Param(FName Id, FName Key) const;
    float Rule(FName Key) const { return Rules.FindRef(Key); }
};

struct FEquipmentDisciplineState
{
    int32 DisciplineSlot = INDEX_NONE;
    float Acquired = 0;
    float Target = 0;
    float Minimum = 0;
};
struct FEquipmentContributionPacket
{
    int32 DisciplineSlot = INDEX_NONE;
    float Score = 0;
    bool bEquipmentBonus = false;
};
struct FEquipmentParticipant
{
    int32 EmployeeId = INDEX_NONE;
    int32 Level = 1;
    int32 PrimarySlot = INDEX_NONE;
    bool bSeated = false;
    bool bTyping = false;
    bool bDown = false;
    float Fatigue = 0;
    FEquipmentEffectLoadout Loadout;
};
struct FEquipmentContributionContext
{
    FGuid RootEventId;
    FGuid AttemptId;
    int64 LoadoutRevision = 0;
    int32 EmployeeId = INDEX_NONE;
    int32 PrimarySlot = INDEX_NONE;
    int32 ChosenSlot = INDEX_NONE;
    bool bCritical = false;
    FName GuaranteedEffect;
    FName RouteEffect;
    float NativeQ = 0;
    float ProjectYield = 1;
    TArray<FEquipmentDisciplineState> Disciplines;
    TArray<FEquipmentParticipant> Participants;
};
struct FEquipmentReliefAction
{
    int32 EmployeeId = INDEX_NONE;
    float Amount = 0;
};

/** 에셋/월드/거래에 의존하지 않는 계산 계층. 평가 중 소비는 후보 원장에만 기록한다. */
class COMPANYGROWTHRENEWAL_API FEmployeeEquipmentEffectEvaluator
{
public:
    static FEquipmentEffectLoadout BuildItemEffects(const FEmployeeEquipmentDefinitionRow& Definition, int32 EnhanceLevel,
        const TMap<FName, FEmployeeEquipmentEffectRow>& Effects, const FEmployeeEquipmentEconomyRow& Economy);
    static FEquipmentEffectLoadout BuildLoadout(const FEmployeeEquipmentState& State, int32 EmployeeId,
        const TMap<FName, FEmployeeEquipmentDefinitionRow>& Definitions, const TMap<FName, FEmployeeEquipmentEffectRow>& Effects,
        const FEmployeeEquipmentEconomyRow& Economy, EEmployeeEquipmentSlot ReplaceSlot = EEmployeeEquipmentSlot::Count,
        const FGuid& CandidateId = FGuid());
    static bool TryApplyPackets(TArray<FEquipmentDisciplineState>& Disciplines,
        const TArray<FEquipmentContributionPacket>& Packets, float ProjectYield);
    static void SyncLoadout(FEquipmentEmployeeRuntime& Employee, const FEquipmentEffectLoadout& Loadout, bool bAtAttemptStart);
    static void AdvanceTime(FEquipmentAttemptRuntime& Attempt, float DeltaTime, const TArray<FEquipmentParticipant>& Participants);
    static float SpeedPercent(const FEquipmentAttemptRuntime& Attempt, int32 EmployeeId, const FEquipmentEffectLoadout& Loadout);
    static float PersonalScorePercent(const FEquipmentAttemptRuntime& Attempt, const FEquipmentContributionContext& Context, const FEquipmentEffectLoadout& Loadout);
    static float TeamScorePercent(const FEquipmentContributionContext& Context);
    static float CritRatePercent(const FEquipmentAttemptRuntime& Attempt, int32 EmployeeId, const FEquipmentEffectLoadout& Loadout);
    static float CritPower(const FEquipmentAttemptRuntime& Attempt, int32 EmployeeId, const FEquipmentEffectLoadout& Loadout);
    static float GeneralCritChance(float ExistingChance, float ExistingCap, const FEquipmentAttemptRuntime& Attempt,
        int32 EmployeeId, const FEquipmentEffectLoadout& Loadout);
    static FName GuaranteedCrit(const FEquipmentAttemptRuntime& Attempt, int32 EmployeeId, const FEquipmentEffectLoadout& Loadout);
    static int32 SelectRoute(const FEquipmentAttemptRuntime& Attempt, int32 EmployeeId, const FEquipmentEffectLoadout& Loadout,
        const TArray<FEquipmentDisciplineState>& Disciplines, float DeficitRoll, FName& OutRouteEffect);
    static bool RequestManualPriority(FEquipmentAttemptRuntime& Attempt, int32 EmployeeId, int32 DisciplineSlot,
        const FEquipmentEffectLoadout& Loadout, const TArray<FEquipmentDisciplineState>& Disciplines);
    static TArray<FEquipmentContributionPacket> PlanPackets(const FEquipmentAttemptRuntime& Attempt,
        const FEquipmentContributionContext& Context, const FEquipmentEffectLoadout& Loadout, float BonusCapPercent);
    static void CommitNative(FEquipmentAttemptRuntime& Attempt, const FEquipmentContributionContext& Context,
        const FEquipmentEffectLoadout& Loadout, TArray<FEquipmentReliefAction>& OutRelief);
    static bool TryVetoDown(FEquipmentAttemptRuntime& Attempt, int32 EmployeeId, bool bBolted,
        const FEquipmentEffectLoadout& Loadout, const TArray<FEquipmentParticipant>& Participants, float& OutRelief);
    static void RecordDown(FEquipmentAttemptRuntime& Attempt, int32 EmployeeId, int32 PrimarySlot, const FGuid& DownId,
        const TArray<FEquipmentParticipant>& Participants);
    static void RecordRecovery(FEquipmentAttemptRuntime& Attempt, int32 EmployeeId, EEquipmentRecoverySource Source,
        const FEquipmentEffectLoadout& Loadout);
    static bool RecordManualCatch(FEquipmentAttemptRuntime& Attempt, int32 EmployeeId, const FGuid& DownId,
        float BaseRelief, float Fatigue, const FEquipmentEffectLoadout& Loadout, const TArray<FEquipmentParticipant>& Participants,
        float& OutSelfRelief, TArray<FEquipmentReliefAction>& OutTeamRelief);
    static float ResolveFatigueDelta(FEquipmentAttemptRuntime& Attempt, int32 EmployeeId, float Delta,
        float Fatigue, float TiredThreshold, const FEquipmentEffectLoadout& Loadout);
    static float ClampInstantRelief(FEquipmentAttemptRuntime& Attempt, int32 EmployeeId, float Requested, float Cap);
    static void RecordFever(FEquipmentAttemptRuntime& Attempt, const TArray<FEquipmentParticipant>& Participants);
    static bool RecordBaselineXP(FEquipmentAttemptRuntime& Attempt, int32 EmployeeId, int32 BucketIndex,
        float BaselineXP, float SelfCapPercent, FEquipmentXPGrant& OutGrant);
    static TArray<FEquipmentXPGrant> FinalizeXP(FEquipmentAttemptRuntime& Attempt, EEquipmentAttemptOutcome Outcome,
        const TArray<FEquipmentDisciplineState>& Disciplines, float SelfCapPercent, float MentorCapPercent,
        bool bAutomaticWorkPeriod = false);
};
