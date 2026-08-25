#pragma once

#include "CoreMinimal.h"

struct FTutorialRevenueCompletionRules
{
	static bool IsPositiveCollection(int64 Amount)
	{
		return Amount > 0;
	}
};
