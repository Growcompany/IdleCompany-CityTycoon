// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "RecruitmentProbabilityTable.generated.h"

/**
 * 채용 레벨별 확률 테이블
 * 각 채용 레벨에서 등급별 등장 가중치와 직급 범위를 정의
 */
USTRUCT(BlueprintType)
struct FRecruitmentProbabilityRow : public FTableRowBase
{
	GENERATED_BODY()

	// 채용 레벨 (1부터 시작)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level")
	int32 Level = 1;

	// 등급별 가중치 (1000 기준, Mythic 0.1% = 1)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weight")
	int32 CommonWeight = 700;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weight")
	int32 UnusualWeight = 200;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weight")
	int32 RareWeight = 80;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weight")
	int32 EpicWeight = 18;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weight")
	int32 LegendaryWeight = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weight")
	int32 MythicWeight = 0;

	// 직급 범위 (강화 레벨)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enhancement", meta = (ClampMin = "0"))
	int32 MinEnhancement = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enhancement", meta = (ClampMin = "0"))
	int32 MaxEnhancement = 2;

	// 정보 공개 확률 (0.0 ~ 1.0)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Info", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float InfoRevealChance = 0.3f;

	// 총 가중치 계산
	int32 GetTotalWeight() const
	{
		return CommonWeight + UnusualWeight + RareWeight + EpicWeight + LegendaryWeight + MythicWeight;
	}
};
