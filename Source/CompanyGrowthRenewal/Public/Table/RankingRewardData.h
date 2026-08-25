#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "RankingRewardData.generated.h"

/**
 * 주간 랭킹 보상 DataTable 행
 * 순위 범위별 Diamond 보상과 칭호를 정의
 */
USTRUCT(BlueprintType)
struct FRankingRewardData : public FTableRowBase
{
	GENERATED_BODY()

	// 순위 범위 시작 (포함)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ranking")
	int32 RankMin = 1;

	// 순위 범위 끝 (포함)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ranking")
	int32 RankMax = 1;

	// Diamond 보상량
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ranking")
	int32 DiamondReward = 0;

	// 부여할 칭호 (빈 문자열이면 칭호 없음)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ranking")
	FText TitleReward;
};
