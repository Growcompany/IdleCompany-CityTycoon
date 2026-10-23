#pragma once

#include "CoreMinimal.h"
#include "TimeCycle/TimeCycleCode.h"
#include "TimeCycleContinuation.generated.h"

// 월드 참조 없이 레벨 사이에서 유지하는 시계와 구동 설정.
USTRUCT()
struct COMPANYGROWTHRENEWAL_API FTimeCycleContinuation
{
	GENERATED_BODY()

	UPROPERTY() bool bValid = false;
	UPROPERTY() float CurrentTime = 0.f;
	UPROPERTY() int32 NumCycles = 0;
	UPROPERTY() bool bIsCyclePaused = true;
	UPROPERTY() FTimeCycleCode InitialTime;
	UPROPERTY() bool bHasMaxTime = false;
	UPROPERTY() FTimeCycleCode MaxTime;
	UPROPERTY() FTimeCycleCode CycleDuration;
	UPROPERTY() FTimeCycleCode SunRiseTime;
	UPROPERTY() FTimeCycleCode SunSetTime;
	UPROPERTY() double CapturedRealtimeSeconds = 0.0;

	bool Advance(double ElapsedSeconds);
};
