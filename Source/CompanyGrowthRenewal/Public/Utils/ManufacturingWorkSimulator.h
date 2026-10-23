#pragma once

#include "CoreMinimal.h"
#include "Data/ManufacturingWorkData.h"
#include "Utils/EmployeeEquipmentEffectEvaluator.h"

class UFatigueConfig;

struct FManufacturingResearchSimulation
{
    FManufacturingResearchState* State = nullptr;
    TArray<int32> TeamCapability;
    TArray<int32> RequiredCapability;
    double RequiredWorkSeconds = 0.0;
};

struct FManufacturingWorkerInput
{
    int32 EmployeeId = INDEX_NONE;
    int32 Level = 1;
    int32 PrimarySlot = 0;
    TArray<float> Affinities = {1.f, 1.f, 1.f};
    TArray<int32> Capability = {0, 0, 0};
    float BaseOutput = 0;
    float WorkRate = 1;
    float ScoreMultiplier = 1;
    float CritChance = 0;
    float CritPower = 2;
    float Stamina = 0;
    float Composure = 0;
    float Environment = 0;
    float RecoveryMultiplier = 1;
    float ExperienceMultiplier = 1;
    FEquipmentEffectLoadout Loadout;
};

struct FManufacturingWorkResult
{
    TArray<double> Points = {0., 0., 0.};
    double WorkedSeconds = 0.0;
    TMap<int32, double> Experience;
    int32 CompletedResearchProjectIndex = INDEX_NONE;
};

/** 고정 시간 단위와 저장된 난수 상태를 사용해 맵·프레임·분할 정산에 독립적으로 계산한다. */
class COMPANYGROWTHRENEWAL_API FManufacturingWorkSimulator
{
public:
    static FManufacturingWorkResult Advance(FManufacturingWorkState& State, int32 BuildingId,
        const TArray<FManufacturingWorkerInput>& Workers, const UFatigueConfig& Config,
        float ProjectYield, double Seconds, FManufacturingResearchSimulation* Research = nullptr);
    static TArray<double> EstimateRates(const FManufacturingWorkState& State,
        const TArray<FManufacturingWorkerInput>& Workers, const UFatigueConfig& Config, float ProjectYield);
    static TArray<FEquipmentParticipant> Participants(const FManufacturingWorkState& State,
        const TArray<FManufacturingWorkerInput>& Workers);
    static float FatigueOutput(float Fatigue, const FEquipmentEffectLoadout& Loadout, const UFatigueConfig& Config);
};
