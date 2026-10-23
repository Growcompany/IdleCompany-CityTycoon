#pragma once

#include "CoreMinimal.h"
#include "Data/ManufacturingWorkData.h"

namespace ManufacturingResearchRules
{
    inline bool HasValidDefinition(const TArray<int32>& Required, double WorkSeconds)
    {
        if (Required.Num() != 3 || !FMath::IsFinite(WorkSeconds) || WorkSeconds <= 0.0) return false;
        bool bAnyRequired = false;
        for (const int32 Value : Required)
        {
            if (Value < 0) return false;
            bAnyRequired |= Value > 0;
        }
        return bAnyRequired;
    }

    inline bool HasCapability(const TArray<int32>& Team, const TArray<int32>& Required)
    {
        if (Team.Num() != 3 || Required.Num() != 3) return false;
        for (int32 Field = 0; Field < 3; ++Field)
            if (Team[Field] < 0 || Required[Field] < 0 || Team[Field] < Required[Field]) return false;
        return true;
    }

    // 한 명의 휴식은 팀 진행도를 그 사람의 비중만큼 늦춘다.
    inline bool Advance(FManufacturingResearchState& State, const TArray<int32>& Team,
        const TArray<int32>& Required, double RequiredSeconds, double WorkedSeconds, int32 WorkerCount)
    {
        if (State.ProjectIndex <= 0 || WorkerCount <= 0 || !HasValidDefinition(Required, RequiredSeconds)
            || !HasCapability(Team, Required) || !FMath::IsFinite(WorkedSeconds) || WorkedSeconds <= 0.0
            || !FMath::IsFinite(State.CompletedWorkSeconds) || State.CompletedWorkSeconds < 0.0) return false;
        State.CompletedWorkSeconds = FMath::Min(RequiredSeconds,
            State.CompletedWorkSeconds + WorkedSeconds / static_cast<double>(WorkerCount));
        return State.CompletedWorkSeconds >= RequiredSeconds;
    }
}
