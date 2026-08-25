// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CostCurveType.generated.h"

/**
 * 강화/업그레이드 비용 곡선 형태 — 빌딩 강화와 벽돌공장이 공유한다.
 *
 * Power 는 실효 성장률이 `1 + Exponent / (LevelScale + Level)` 로 감쇠한다.
 * 초반에 가파르고 레벨이 오를수록 완만해져 **"초반 체감"과 "수천 레벨 도달"을 동시에** 만족시킨다.
 * 순수 등비(Geometric)로는 이 둘이 양립하지 않는다 — 성장률이 전 구간 상수라
 * 초반을 세우면 int64 가 포화하고, 후반을 살리면 첫 수십 레벨이 같은 가격으로 보인다.
 * 밴드 왕복 3회의 기록 = `docs/00_Core/DECISION_RECORDS.md` §3.1 / §3.16.
 */
UENUM(BlueprintType)
enum class ECostCurveType : uint8
{
	Geometric UMETA(DisplayName = "등비"),   // Cost = BaseCost × GrowthRate^Level
	Power     UMETA(DisplayName = "멱함수")  // Cost = BaseCost × (1 + Level/LevelScale)^Exponent
};
