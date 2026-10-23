#pragma once

#include "CoreMinimal.h"
#include "Data/EmployeeEquipmentRuntimeData.h"
#include "ManufacturingWorkData.generated.h"

UENUM()
enum class EManufacturingRest : uint8 { None, Drowsy, Away, Breathing };

USTRUCT()
struct FManufacturingEmployeeWork
{
    GENERATED_BODY()
    UPROPERTY(SaveGame) float Fatigue = 0;
    UPROPERTY(SaveGame) EManufacturingRest Rest = EManufacturingRest::None;
    UPROPERTY(SaveGame) double RestRemaining = 0;
    UPROPERTY(SaveGame) double RestElapsed = 0;
    UPROPERTY(SaveGame) float RestRecoveryRate = 0;
    UPROPERTY(SaveGame) double ContributionProgress = 0;
    // 이미 개인에게 적립된 기본 경험치. 주기 종료 장비 보너스의 기준으로만 사용한다.
    UPROPERTY(SaveGame) double PeriodBaselineExperience = 0;
};

/** 제조업의 유일한 근무 원장. 액터는 이 상태를 표시하고 별도로 적립하지 않는다. */
USTRUCT()
struct FManufacturingWorkState
{
    GENERATED_BODY()
    UPROPERTY(SaveGame) int32 Period = 0;
    UPROPERTY(SaveGame) int32 RandomSeed = 0;
    UPROPERTY(SaveGame) double PendingSeconds = 0;
    UPROPERTY(SaveGame) TMap<int32, FManufacturingEmployeeWork> Employees;
    UPROPERTY(SaveGame) TArray<double> PeriodPoints = {0., 0., 0.};
    UPROPERTY(SaveGame) TArray<double> PeriodTargets = {0., 0., 0.};
    UPROPERTY(SaveGame) FEquipmentAttemptRuntime Equipment;
};

USTRUCT(BlueprintType)
struct FManufacturingResearchState
{
    GENERATED_BODY()
    UPROPERTY(SaveGame, BlueprintReadOnly) int32 ProjectIndex = INDEX_NONE;
    UPROPERTY(SaveGame, BlueprintReadOnly) double CompletedWorkSeconds = 0.0;
};
