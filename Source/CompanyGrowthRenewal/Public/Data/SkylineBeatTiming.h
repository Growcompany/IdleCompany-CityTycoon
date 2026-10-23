#pragma once

#include "CoreMinimal.h"

/** 등반 재생의 템포 노브. 위젯이 EditAnywhere 로 들고 있고 계산은 아래 순수 함수가 한다. */
struct COMPANYGROWTHRENEWAL_API FSkylineBeatTuning
{
	int32 WorkMs = 380;
	int32 SlamMs = 300;
	float AccelPerFloor = 0.93f;
	int32 MinFloorBeatMs = 420;
	float BossBeatMul = 1.6f;
};

struct COMPANYGROWTHRENEWAL_API FSkylineBeatDurations
{
	int32 WorkMs = 0;
	int32 SlamMs = 0;
};

struct COMPANYGROWTHRENEWAL_API FSkylineBeatTimeline
{
	/** PlayIndex 번째 층의 두 비트 길이. 하한은 비트별이 아니라 층 전체에 건다. */
	static FSkylineBeatDurations ComputeFloorBeats(
		const FSkylineBeatTuning& Tuning, int32 PlayIndex, bool bIsBoss);
};
