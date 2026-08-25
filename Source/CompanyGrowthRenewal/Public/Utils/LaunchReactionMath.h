#pragma once

#include "CoreMinimal.h"

// 출시 반응 드립 스케줄 — 월드 의존 0. 시드 = 프로젝트 ID 로 재현 가능(같은 출시는 같은 박자).
namespace LaunchReactionMath
{
	inline TArray<float> BuildReactionSchedule(int32 Count, int32 Seed, float FirstDelay = 4.f, float MinGap = 8.f, float MaxGap = 12.f)
	{
		TArray<float> Times;
		if (Count <= 0) { return Times; }
		FRandomStream Rng(Seed);
		float Acc = FirstDelay;
		for (int32 i = 0; i < Count; ++i)
		{
			Times.Add(Acc);
			Acc += Rng.FRandRange(MinGap, MaxGap);
		}
		return Times;
	}
}
