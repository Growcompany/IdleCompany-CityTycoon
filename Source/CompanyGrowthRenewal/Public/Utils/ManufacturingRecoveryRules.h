#pragma once

#include "CoreMinimal.h"
#include "Utils/ManufacturingWorkSimulator.h"

/** 저장된 제조업 근무자 상태의 피로와 자동 복귀를 한 고정 시간 단위만큼 전진시킨다. */
class COMPANYGROWTHRENEWAL_API FManufacturingRecoveryRules
{
public:
    // 호출자는 같은 Participants로 장비 AdvanceTime을 먼저 수행한다. true면 이 단위에 기여할 수 있다.
    static bool Advance(FManufacturingWorkState& State, const FManufacturingWorkerInput& Worker,
        const UFatigueConfig& Config, float DeltaTime, float BoltRoll,
        const TArray<FEquipmentParticipant>& Participants);
};
