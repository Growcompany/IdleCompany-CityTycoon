#pragma once

#include "CoreMinimal.h"

// 회수 「즉시 완료」 다이아 값 ― 남은 회수만큼만 낸다. 다 끝나 가는데 전액을 내지 않게
struct FSkipCostRules
{
	// 서버 지출 규칙(firebase-chat-server spendRules.json city.skipRecovery opCap)과 같아야 한다 ― 넘으면 서버가 거절한다
	static constexpr int32 MaxDiamonds = 100;

	// 처음부터 끝까지 사면 티어 × 10. 소요 시간은 티어에 지수로 늘고 가격은 선형이라 후반일수록 이득인 것은 의도된 기울기다
	static int32 FullDiamonds(int32 Tier)
	{
		return FMath::Clamp(Tier * 10, 10, MaxDiamonds);
	}

	// RemainingFraction = 남은 회수 ÷ 굴린 총액(0~1). 남은 것이 있으면 최소 1
	static int32 Diamonds(int32 Tier, double RemainingFraction)
	{
		if (RemainingFraction <= 0.0)
		{
			return 0;
		}
		const double Fraction = FMath::Min(RemainingFraction, 1.0);
		return FMath::Clamp(FMath::CeilToInt32(double(FullDiamonds(Tier)) * Fraction), 1, MaxDiamonds);
	}
};
