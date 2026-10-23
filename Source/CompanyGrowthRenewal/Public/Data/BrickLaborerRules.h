#pragma once

#include "CoreMinimal.h"

/**
 * 잡부 생산 공식. 액터에서 떼어낸 이유는 이 산수가 경제의 단일 회귀 지점이기 때문이다 —
 * 월드 없이 테스트할 수 있어야 "생산율이 강화 곡선과 같은가"를 상시 검증할 수 있다.
 */
struct FBrickLaborerRules
{
	// 인부 수 강화값을 현재 공장 단계의 고용 정원으로 제한한다.
	static int32 LaborerCountFromValue(float UpgradeValue, int32 LaborerCapacity)
	{
		return LaborerCapacity > 0 ? FMath::Clamp(FMath::RoundToInt(UpgradeValue), 1, LaborerCapacity) : 0;
	}

	// 한 구간(문→문)에 걸리는 시간. 강화 곡선이 아니라 **거리 ÷ 걷는 속도**로 정한다.
	// 곡선에서 뽑으면 문 간격 62m 를 1.2초에 가게 돼(초속 50m) 발이 땅에서 미끄러진다.
	static float LegSeconds(float PathLengthCm, float WalkSpeedCmS)
	{
		return FMath::Max(0.5f, PathLengthCm / FMath::Max(20.f, WalkSpeedCmS));
	}

	// 인부 1명이 문에 닿을 때마다 뱉을 개수. **인원수로 나누지 않는다** — 각자 자기 능력치만큼 만든다.
	// 이동 속도는 사람처럼 보이는 범위로 묶여 있으므로, 곡선이 요구하는 나머지 배수는 여기가 가져간다.
	static int32 HarvestPerTrip(int32 PerLaborerOutput, float InLegSeconds, float CurveCycleSeconds,
		float SpeedMultiplier, float ProductionScale)
	{
		const float Per = static_cast<float>(FMath::Max(1, PerLaborerOutput)) * InLegSeconds * ProductionScale
			/ (FMath::Max(CurveCycleSeconds, KINDA_SMALL_NUMBER) * FMath::Max(SpeedMultiplier, KINDA_SMALL_NUMBER));
		return FMath::Max(1, FMath::RoundToInt(Per));
	}

	// 초당 생산 개수.
	static float BricksPerSecond(int32 HarvestAmount, int32 LaborerCount, float InLegSeconds, float SpeedMultiplier)
	{
		const float Seconds = FMath::Max(InLegSeconds, KINDA_SMALL_NUMBER) / FMath::Max(SpeedMultiplier, KINDA_SMALL_NUMBER);
		return static_cast<float>(HarvestAmount) * static_cast<float>(FMath::Max(0, LaborerCount)) / Seconds;
	}

	// 보행 클립 재생 속도. 지면 속도와 클립 기준 속도의 비 — 속도만 올리면 발이 끌린다.
	static float ClipPlayRate(float GroundSpeedCmS, float ClipRefSpeedCmS)
	{
		return FMath::Max(0.05f, GroundSpeedCmS / FMath::Max(ClipRefSpeedCmS, 20.f));
	}
};
