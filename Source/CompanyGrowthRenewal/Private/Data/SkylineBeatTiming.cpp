#include "Data/SkylineBeatTiming.h"

FSkylineBeatDurations FSkylineBeatTimeline::ComputeFloorBeats(
	const FSkylineBeatTuning& Tuning, const int32 PlayIndex, const bool bIsBoss)
{
	FSkylineBeatDurations Out;
	if (Tuning.WorkMs <= 0 && Tuning.SlamMs <= 0) { return Out; }

	const float Scale = FMath::Pow(
		FMath::Clamp(Tuning.AccelPerFloor, 0.5f, 1.0f), static_cast<float>(FMath::Max(0, PlayIndex)));
	const float BossMul = bIsBoss ? FMath::Max(1.0f, Tuning.BossBeatMul) : 1.0f;

	float Work = Tuning.WorkMs * Scale * BossMul;
	float Slam = Tuning.SlamMs * Scale * BossMul;

	// 하한을 비트별로 걸면 짧은 쪽만 먼저 바닥에 닿아 개발:돌파 비율이 무너진다
	const float FloorMs = FMath::Max(0, Tuning.MinFloorBeatMs) * BossMul;
	const float Total = Work + Slam;
	if (Total > 0.0f && Total < FloorMs)
	{
		const float Lift = FloorMs / Total;
		Work *= Lift;
		Slam *= Lift;
	}

	Out.WorkMs = FMath::RoundToInt(Work);
	Out.SlamMs = FMath::RoundToInt(Slam);
	return Out;
}
