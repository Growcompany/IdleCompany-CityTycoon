#pragma once

#include "CoreMinimal.h"
#include "Data/ManufacturingDevelopmentLedger.h"

namespace ManufacturingDevelopmentRules
{
    using FTimeInterval = FManufacturingOnlineInterval;

    // 입력 구간은 시간순으로 병합된 온라인 적립 구간이다.
    inline double UncoveredSeconds(const FDateTime& Start, const FDateTime& End,
        const TArray<FTimeInterval>& OnlineIntervals, const FDateTime& LastSettledEnd = FDateTime())
    {
        const FDateTime EffectiveStart = LastSettledEnd.GetTicks() > 0
            ? FMath::Max(Start, LastSettledEnd) : Start;
        if (End <= EffectiveStart) return 0.0;
        FDateTime Cursor = EffectiveStart;
        double Uncovered = 0.0;
        for (const FTimeInterval& Online : OnlineIntervals)
        {
            if (Online.EndUtc <= Cursor || Online.StartUtc >= End) continue;
            const FDateTime CoveredStart = FMath::Max(Cursor, Online.StartUtc);
            if (CoveredStart > Cursor) Uncovered += (CoveredStart - Cursor).GetTotalSeconds();
            Cursor = FMath::Max(Cursor, FMath::Min(End, Online.EndUtc));
            if (Cursor >= End) return Uncovered;
        }
        return Uncovered + (End - Cursor).GetTotalSeconds();
    }

    // 점수는 소수까지 보존하며 잘못된 시간·요율은 잔고를 바꾸지 않는다.
    inline double Accrue(double Current, double RatePerSecond, double Seconds)
    {
        const double Base = FMath::IsFinite(Current) && Current >= 0.0 ? Current : 0.0;
        if (!FMath::IsFinite(RatePerSecond) || RatePerSecond <= 0.0
            || !FMath::IsFinite(Seconds) || Seconds <= 0.0)
        {
            return Base;
        }
        const double Gain = RatePerSecond * Seconds;
        const double Result = Base + Gain;
        return FMath::IsFinite(Gain) && FMath::IsFinite(Result) ? Result : Base;
    }

    inline double SumRates(const TArray<double>& Rates)
    {
        double Total = 0.0;
        for (const double Rate : Rates)
        {
            if (FMath::IsFinite(Rate) && Rate > 0.0 && FMath::IsFinite(Total + Rate))
            {
                Total += Rate;
            }
        }
        return Total;
    }
}
