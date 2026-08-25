#pragma once
#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "HQLevelData.generated.h"

USTRUCT(BlueprintType)
struct FHQLevelData : public FTableRowBase
{
    GENERATED_BODY()

    // ===== 레벨업 조건 (0 = 해당 조건 없음) =====

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition")
    int64 MoneyCost = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition")
    int32 RequiredBuildingCount = 0;

    // 깊이 조건 — 이 티어 이상인 빌딩이 RequiredTierBuildingCount 개 이상 필요 (0 = 조건 없음).
    // RequiredBuildingCount(폭)와 별도 컬럼인 이유: 한 컬럼으로 합치면 합이 곱이 되어 폭발한다
    // (T10 빌딩 15개 = 63클리어 x 15 = 945클리어).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition")
    int32 RequiredTier = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition")
    int32 RequiredTierBuildingCount = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition")
    int32 RequiredEmployeeCount = 0;

    // 시총 조건 (0 = 없음). 2막(제조 개방) 레벨의 측정 조건 — 2-경제 분리(진출=시총, DECISION_RECORDS §1.3) 보존
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition")
    int64 RequiredMarketCap = 0;

    // ===== 레벨업 보상 =====

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward")
    FText UnlockDescription;
};
