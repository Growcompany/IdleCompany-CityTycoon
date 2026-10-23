#pragma once

#include "CoreMinimal.h"
#include "ManufacturingDevelopmentLedger.generated.h"

/** 이미 제조 개발 포인트를 지급한 온라인 UTC 구간. */
USTRUCT()
struct FManufacturingOnlineInterval
{
    GENERATED_BODY()

    FManufacturingOnlineInterval() = default;
    FManufacturingOnlineInterval(const FDateTime& InStartUtc, const FDateTime& InEndUtc)
        : StartUtc(InStartUtc), EndUtc(InEndUtc) {}

    UPROPERTY(SaveGame)
    FDateTime StartUtc;

    UPROPERTY(SaveGame)
    FDateTime EndUtc;
};
