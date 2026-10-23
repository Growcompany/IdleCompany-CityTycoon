#pragma once

#include "CoreMinimal.h"

struct FAmbientAircraftBonusRules
{
    static constexpr int64 CooldownSeconds = 60;
    static constexpr double RewardIncomeSeconds = 2.5;

    static int64 TryClaim(TMap<FName, int64>& LastClaimByAircraft, FName AircraftKey,
        int64 NowUnix, double ReferenceRate, int64 CurrentMoney, int64 TotalRevenue)
    {
        if (AircraftKey.IsNone() || NowUnix <= 0 || !FMath::IsFinite(ReferenceRate)
            || ReferenceRate <= 0.0 || CurrentMoney < 0 || TotalRevenue < 0)
        {
            return 0;
        }
        if (const int64* LastClaim = LastClaimByAircraft.Find(AircraftKey))
        {
            if (*LastClaim < 0 || NowUnix < *LastClaim || NowUnix - *LastClaim < CooldownSeconds)
            {
                return 0;
            }
        }

        const double RewardValue = ReferenceRate * RewardIncomeSeconds;
        if (!FMath::IsFinite(RewardValue) || RewardValue < 1.0
            || RewardValue >= static_cast<double>(MAX_int64))
        {
            return 0;
        }
        const int64 Reward = FMath::FloorToInt64(RewardValue);
        if (Reward > MAX_int64 - CurrentMoney || Reward > MAX_int64 - TotalRevenue)
        {
            return 0;
        }

        // 자원 변경 델리게이트가 다시 수령을 요청해도 같은 기체는 이미 제한된 상태다.
        LastClaimByAircraft.Add(AircraftKey, NowUnix);
        return Reward;
    }
};
