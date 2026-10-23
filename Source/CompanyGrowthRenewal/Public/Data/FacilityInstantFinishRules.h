#pragma once

#include "CoreMinimal.h"
#include "Table/InstantFinishBalanceTable.h"

struct FFacilityInstantFinishRules
{
	static int64 CalculateCost(int64 RemainingSeconds, const FInstantFinishBalanceTable& Balance,
		int32 DiscountBasisPoints)
	{
		if (RemainingSeconds <= 0 || !FMath::IsFinite(Balance.SecondsPerDiamond)
			|| Balance.SecondsPerDiamond <= 0.0 || Balance.MaxCost <= 0
			|| Balance.MaxDiscountBasisPoints < 0 || Balance.MaxDiscountBasisPoints >= 10000
			|| DiscountBasisPoints < 0 || DiscountBasisPoints > Balance.MaxDiscountBasisPoints)
		{
			return 0;
		}
		const double RawCost = static_cast<double>(RemainingSeconds) * (10000 - DiscountBasisPoints)
			/ (Balance.SecondsPerDiamond * 10000.0);
		if (!FMath::IsFinite(RawCost) || RawCost <= 0.0
			|| RawCost > static_cast<double>(Balance.MaxCost) || RawCost >= static_cast<double>(MAX_int64))
		{
			return 0;
		}
		return FMath::Max<int64>(1, static_cast<int64>(FMath::CeilToDouble(RawCost)));
	}
};
