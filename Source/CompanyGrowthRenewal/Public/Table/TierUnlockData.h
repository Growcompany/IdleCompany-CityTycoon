#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Data/BuildingEnhancementData.h"
#include "TierUnlockData.generated.h"

/**
 * 티어별 해금 데이터 (DataTable Row)
 *
 * RowName: "1" ~ "10" (티어 숫자)
 * 승급 조건(7/10 클리어)은 TierConstants::CLEAR_TO_UNLOCK 이 소유한다 —
 * 이 테이블은 "무엇을 주는가"만 답하고 "언제 주는가"는 코드가 답한다.
 */
USTRUCT(BlueprintType)
struct FTierUnlockData : public FTableRowBase
{
	GENERATED_BODY()

	// 이 티어에 도달할 때 해금되는 강화 슬롯 (빈 배열이면 해금 없음)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tier Unlock")
	TArray<EBuildingEnhancementType> UnlockedEnhancements;
};
