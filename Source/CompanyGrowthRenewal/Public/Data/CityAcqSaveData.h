#pragma once
#include "CoreMinimal.h"
#include "CityAcqSaveData.generated.h"

USTRUCT()
struct FCityAcqSave
{
    GENERATED_BODY()
    UPROPERTY(SaveGame) uint8 State = 0;   // EAcqState as uint8
    UPROPERTY(SaveGame) int64 RTotal = 0;
    UPROPERTY(SaveGame) int64 RRemaining = 0;
};
