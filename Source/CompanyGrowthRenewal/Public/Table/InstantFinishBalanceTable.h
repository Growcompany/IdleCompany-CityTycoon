#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "InstantFinishBalanceTable.generated.h"

/** Mine/Factory instant finish quote limits. Missing rows remain invalid. */
USTRUCT(BlueprintType)
struct FInstantFinishBalanceTable : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Instant Finish")
	double SecondsPerDiamond = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Instant Finish")
	int64 MaxCost = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Instant Finish")
	int32 MaxDiscountBasisPoints = 0;
};
