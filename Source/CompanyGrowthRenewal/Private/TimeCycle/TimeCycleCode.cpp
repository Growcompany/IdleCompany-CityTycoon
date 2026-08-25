// TimeCycleSystem 플러그인에서 통합됨
// Original Copyright Grumpy Duck Games 2025 All Rights Reserved.

#include "TimeCycle/TimeCycleCode.h"

FTimeCycleCode FTimeCycleCode::FromSeconds(int32 InSeconds)
{
	InSeconds = InSeconds % 86400;

	FTimeCycleCode TimeCycleCode;
	TimeCycleCode.Hours = InSeconds / 3600;
	TimeCycleCode.Minutes = (InSeconds % 3600) / 60;
	TimeCycleCode.Seconds = InSeconds % 60;
	return TimeCycleCode;
}

int32 FTimeCycleCode::ToSeconds() const
{
	return Seconds + Minutes * 60 + Hours * 3600;
}

bool FTimeCycleCode::IsValid() const
{
	return Hours >= 0 && Minutes >= 0 && Seconds >= 0;
}

bool FTimeCycleCode::operator==(const FTimeCycleCode& Other) const
{
	return Hours == Other.Hours && Minutes == Other.Minutes && Seconds == Other.Seconds;
}
