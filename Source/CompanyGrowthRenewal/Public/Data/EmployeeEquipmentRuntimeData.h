#pragma once
#include "CoreMinimal.h"
#include "EmployeeEquipmentRuntimeData.generated.h"

UENUM()
enum class EEquipmentAttemptOutcome : uint8 { Active, Launched, Failed, Aborted };
UENUM()
enum class EEquipmentRecoverySource : uint8 { ManualTap, EquipmentAuto, Natural, ModeExit };

/** 직원+효과+Attempt의 영속 이력. 장비 인스턴스를 바꿔도 같은 이력을 사용한다. */
USTRUCT()
struct FEquipmentEffectRuntime
{
    GENERATED_BODY()
    UPROPERTY(SaveGame) bool bStartedWithEffect = false;
    UPROPERTY(SaveGame) bool bEquipped = false;
    UPROPERTY(SaveGame) bool bStartConsumed = false;
    UPROPERTY(SaveGame) int32 Counter = 0;
    UPROPERTY(SaveGame) int32 Charges = 0;
    UPROPERTY(SaveGame) int32 NativeCount = 0;
    UPROPERTY(SaveGame) int32 CatchCount = 0;
    UPROPERTY(SaveGame) int32 TargetSlot = INDEX_NONE;
    UPROPERTY(SaveGame) int32 VisitedMask = 0;
    UPROPERTY(SaveGame) int32 LastRecipient = INDEX_NONE;
    UPROPERTY(SaveGame) int64 ConsumedEventSequence = 0;
    UPROPERTY(SaveGame) TArray<int32> RecentSlots;
    UPROPERTY(SaveGame) TMap<int32, int32> DisciplineCounts;
    UPROPERTY(SaveGame) double CooldownUntil = 0;
    UPROPERTY(SaveGame) double ExpiresAt = 0;
    UPROPERTY(SaveGame) float StoredValue = 0;
    UPROPERTY(SaveGame) double SegmentExposure = 0;
    UPROPERTY(SaveGame) double SegmentValueSeconds = 0;
    UPROPERTY(SaveGame) double TypedSeconds = 0;
    UPROPERTY(SaveGame) double EligibleBaseXP = 0;
    UPROPERTY(SaveGame) TMap<FName, float> Parameters;
};

USTRUCT()
struct FEquipmentEmployeeRuntime
{
    GENERATED_BODY()
    UPROPERTY(SaveGame) int32 EmployeeId = INDEX_NONE;
    UPROPERTY(SaveGame) int32 StartLevel = 0;
    UPROPERTY(SaveGame) bool bStartParticipant = false;
    UPROPERTY(SaveGame) TMap<FName, FEquipmentEffectRuntime> Effects;
    UPROPERTY(SaveGame) int32 NativeCount = 0;
    UPROPERTY(SaveGame) int32 LastNativeTarget = INDEX_NONE;
    UPROPERTY(SaveGame) int32 DownCount = 0;
    UPROPERTY(SaveGame) int32 NextGuaranteedEmission = 0;
    UPROPERTY(SaveGame) double TypedSeconds = 0;
    UPROPERTY(SaveGame) double BaselineXP = 0;
    UPROPERTY(SaveGame) double SelfEquipmentXP = 0;
    UPROPERTY(SaveGame) double MentorXP = 0;
    UPROPERTY(SaveGame) FGuid DownId;
    UPROPERTY(SaveGame) double DownStartedAt = 0;
    UPROPERTY(SaveGame) bool bDrowsyArmed = true;
    UPROPERTY(SaveGame) bool bRestArmed = true;
    UPROPERTY(SaveGame) double RestUntil = 0;
    UPROPERTY(SaveGame) int64 ReliefBucket = -1;
    UPROPERTY(SaveGame) float ReliefInBucket = 0;
    UPROPERTY(SaveGame) TSet<FGuid> SeenManualDownIds;
};

USTRUCT()
struct FEquipmentSupportRuntime
{
    GENERATED_BODY()
    UPROPERTY(SaveGame) FName EffectId;
    UPROPERTY(SaveGame) int32 ProviderEmployeeId = INDEX_NONE;
    UPROPERTY(SaveGame) int32 RecipientEmployeeId = INDEX_NONE;
    UPROPERTY(SaveGame) float Value = 0;
    UPROPERTY(SaveGame) double ExpiresAt = 0;
};

USTRUCT()
struct FEquipmentXPGrant
{
    GENERATED_BODY()
    UPROPERTY(SaveGame) FString GrantId;
    UPROPERTY(SaveGame) int32 EmployeeId = INDEX_NONE;
    UPROPERTY(SaveGame) float BaselineAmount = 0;
    UPROPERTY(SaveGame) float EquipmentAmount = 0;
    UPROPERTY(SaveGame) FName EffectId;
};

USTRUCT()
struct FEquipmentAttemptRuntime
{
    GENERATED_BODY()
    UPROPERTY(SaveGame) FGuid AttemptId;
    UPROPERTY(SaveGame) int32 BuildingId = INDEX_NONE;
    UPROPERTY(SaveGame) int32 SegmentId = 0;
    UPROPERTY(SaveGame) int32 CompletedSegments = 0;
    UPROPERTY(SaveGame) EEquipmentAttemptOutcome Outcome = EEquipmentAttemptOutcome::Active;
    UPROPERTY(SaveGame) double ActiveTime = 0;
    UPROPERTY(SaveGame) double SegmentStartTime = 0;
    UPROPERTY(SaveGame) float SegmentDuration = 0;
    UPROPERTY(SaveGame) int64 EventSequence = 0;
    UPROPERTY(SaveGame) int64 LastNativeSequence = 0;
    UPROPERTY(SaveGame) int32 LastNativeEmployee = INDEX_NONE;
    UPROPERTY(SaveGame) int32 LastNativeSlot = INDEX_NONE;
    UPROPERTY(SaveGame) double LastNativeTime = 0;
    UPROPERTY(SaveGame) TMap<int32, FEquipmentEmployeeRuntime> Employees;
    UPROPERTY(SaveGame) TArray<FEquipmentSupportRuntime> Supports;
    UPROPERTY(SaveGame) TMap<FString, FEquipmentXPGrant> XPReceipts;
    UPROPERTY(SaveGame) TSet<FGuid> CommittedRootEvents;
};

/** 프로젝트형 런타임은 계정 장비 상태가 소유한다. 제조업 근무는 해당 오피스의 근무 원장이 같은 구조를 사용한다. */
USTRUCT()
struct FEmployeeEquipmentRuntimeData
{
    GENERATED_BODY()
    UPROPERTY(SaveGame) TMap<FGuid, FEquipmentAttemptRuntime> Attempts;
};
