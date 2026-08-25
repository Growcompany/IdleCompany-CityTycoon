#pragma once

#include "CoreMinimal.h"

struct FCountedGoalProgressDecision
{
	int64 Current = 0;
	int64 Target = 1;
	bool bChanged = false;
	bool bReachedThisSignal = false;
};

struct FCountedGoalProgressRules
{
	static FCountedGoalProgressDecision ApplySignal(
		int64 Current,
		int64 ConfiguredTarget,
		bool bAlreadyCompleted)
	{
		FCountedGoalProgressDecision Decision;
		Decision.Current = Current;
		Decision.Target = FMath::Max<int64>(1, ConfiguredTarget);

		if (bAlreadyCompleted || Current >= Decision.Target)
		{
			return Decision;
		}

		Decision.Current = FMath::Min(Current + 1, Decision.Target);
		Decision.bChanged = true;
		Decision.bReachedThisSignal = Decision.Current >= Decision.Target;
		return Decision;
	}
};
