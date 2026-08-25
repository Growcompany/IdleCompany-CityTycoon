#pragma once

#include "CoreMinimal.h"
#include "Enum/CompanyType.h"
#include "TrendState.generated.h"

// 산업별 트렌드 상태 — 소재 1개가 "지금 유행", 착수 N회마다 교체 (specs/2026-07-02-gds-light-loop-design.md §3)
USTRUCT(BlueprintType)
struct FTrendState
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Trend")
	FName TrendMaterial = NAME_None;

	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Trend")
	int32 LaunchesLeft = 3;
};
