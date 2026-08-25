#pragma once

#include "CoreMinimal.h"

inline int64 GetRevenueRateDisplayKey(double PerSec)
{
	if (!FMath::IsFinite(PerSec) || PerSec < 1.0)
	{
		return 0;
	}

	// Binary64 rounds MAX_int64 to the exact 2^63 boundary.
	constexpr double Int64UpperBoundExclusive = 9223372036854775808.0;
	if (PerSec >= Int64UpperBoundExclusive)
	{
		return MAX_int64;
	}

	return static_cast<int64>(PerSec);
}

inline bool IsRevenueRateActive(double PerSec)
{
	return GetRevenueRateDisplayKey(PerSec) >= 1;
}

inline FText FormatRevenueRateText(int64 DisplayRate, const FText& AbbreviatedRate)
{
	return DisplayRate < 1
		? FText::FromString(TEXT("0/s"))
		: FText::Format(FText::FromString(TEXT("+{0}/s")), AbbreviatedRate);
}

inline bool ShouldPulseRevenueRate(double Previous, double Next, double Threshold)
{
	if (!IsRevenueRateActive(Next))
	{
		return false;
	}
	if (!IsRevenueRateActive(Previous))
	{
		return true;
	}

	return FMath::Abs(Next - Previous) / FMath::Max(FMath::Abs(Previous), 1.0) >= Threshold;
}

inline bool ShouldRefreshRevenueRateText(int64 LastDisplayKey, double NextRate)
{
	return LastDisplayKey != GetRevenueRateDisplayKey(NextRate);
}

inline float ComputeRevenuePulseStrength(float Elapsed, float Duration)
{
	if (Duration <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	const float T = FMath::Clamp(Elapsed / Duration, 0.0f, 1.0f);
	const float R = 1.0f - T;
	return R * R;
}
