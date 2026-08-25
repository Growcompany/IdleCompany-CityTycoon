// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "MarketBalanceTable.generated.h"

/**
 * FMarketBalanceData
 * 시장 수요/성장 시스템 밸런스 노브 모음 (단일 row "Default").
 *
 * 적용처: UCountryMarketManager
 * 공식: Mul = clamp(BaseMul + Coef * log10(1 + MC / PivotMC), BaseMul, MaxMul)
 *       Effective Capacity = DT_CountryDemand.Capacity * Mul
 *       (RecoveryPerMin은 스케일 안함 - 진행도 무관 일정 페이스)
 *
 * Why DT 분리: 디자이너가 코드 빌드 없이 CSV reimport 로 진행 곡선 튜닝.
 *              PlayFab Title Data로 라이브 밸런싱도 동일 4개 값만 내려주면 됨.
 */
USTRUCT(BlueprintType)
struct FMarketBalanceData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Growth",
		meta = (ClampMin = "0.0", ClampMax = "1.0",
		ToolTip = "MarketCap=0 시점의 시장 크기 배율 (DT_CountryDemand.Capacity 대비). 낮출수록 초반이 어려움. 0.02 = DT 기본치의 2%."))
	float BaseMul = 0.02f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Growth",
		meta = (ClampMin = "0.0", ClampMax = "5.0",
		ToolTip = "곡선 가팔기. 높일수록 진행에 따라 시장이 더 가파르게 성장. 0.6 = 표준."))
	float Coef = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Growth",
		meta = (ClampMin = "1",
		ToolTip = "곡선 변곡 시점의 MarketCap. 이 값 부근에서 시장이 본격 성장 시작. 낮출수록 일찍 시장 확장."))
	int64 PivotMC = 50000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Growth",
		meta = (ClampMin = "1.0", ClampMax = "20.0",
		ToolTip = "후반 시장 크기 상한. 인플레이션 방지. 3.0 = DT 기본치의 300%까지만 성장."))
	float MaxMul = 3.0f;
};
