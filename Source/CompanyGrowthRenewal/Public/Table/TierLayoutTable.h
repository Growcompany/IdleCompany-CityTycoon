#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Data/TierLayout.h"
#include "TierLayoutTable.generated.h"

// DT_TierLayout 행 = 티어 1개. RowName 은 "T1".."T11" 관례. 테이블이 없으면 FTierLayout 기본(10×10).
USTRUCT(BlueprintType)
struct FTierLayoutRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tier")
	FTierRange Range;
};
