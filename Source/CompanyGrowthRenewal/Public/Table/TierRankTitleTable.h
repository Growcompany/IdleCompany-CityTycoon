#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "TierRankTitleTable.generated.h"

// 산업별 티어 랭크 칭호 (포트폴리오/도감 헤더 랭크 표시). CSV: Name,Industry,Tier,Title.
// enum import 깨짐 방지로 Industry 는 FString("Game"/"IT"/...) — 로드 시 StringToCompanyType 파싱(FProjectGenreRow 관례).
USTRUCT(BlueprintType)
struct FTierRankTitleRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RankTitle")
	FString Industry;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RankTitle")
	int32 Tier = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RankTitle")
	FText Title;
};
