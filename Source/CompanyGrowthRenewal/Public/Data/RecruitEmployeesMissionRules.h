#pragma once

#include "CoreMinimal.h"

struct FRecruitEmployeesMissionRules
{
	static bool HasReachedTarget(int64 CurrentEmployeeCount, int64 ConfiguredTarget)
	{
		return CurrentEmployeeCount >= FMath::Max<int64>(1, ConfiguredTarget);
	}
};
