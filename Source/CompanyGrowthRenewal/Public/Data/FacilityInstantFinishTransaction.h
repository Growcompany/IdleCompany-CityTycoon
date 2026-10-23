#pragma once

#include "CoreMinimal.h"
#include "FacilityInstantFinishTransaction.generated.h"

/** Durable request identity. A replay must use this exact quote and opId. */
USTRUCT()
struct FFacilityInstantFinishTransaction
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FString OpId;

	UPROPERTY(SaveGame)
	FString Pid;

	UPROPERTY(SaveGame)
	FName Reason;

	UPROPERTY(SaveGame)
	int32 Country = 0;

	UPROPERTY(SaveGame)
	int32 Identity = 0;

	UPROPERTY(SaveGame)
	FString CycleStart;

	UPROPERTY(SaveGame)
	int64 RemainingSeconds = 0;

	UPROPERTY(SaveGame)
	int32 DiscountBasisPoints = 0;

	UPROPERTY(SaveGame)
	int64 Amount = 0;
};
