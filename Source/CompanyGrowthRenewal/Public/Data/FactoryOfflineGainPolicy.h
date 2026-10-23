#pragma once

#include "CoreMinimal.h"

/**
 * 자리를 비운 동안의 벽돌 지급 규칙.
 *
 * 액터 밖에 두는 이유: 상한·바닥 규칙을 액터 없이 테스트하기 위해서다(FactoryOfflineGainTests).
 */
namespace FactoryOfflineGainPolicy
{
	/**
	 * 1회 정산으로 실제 지급할 벽돌 수.
	 * 경과 시간은 호출 전에 이미 백엔드 상한(12h)으로 잘려 들어온다.
	 */
	inline int64 SettledBricks(float RatePerSecond, float OfflineSeconds, int32 Capacity, float MinSettleSeconds)
	{
		// 돈 경로와 같은 바닥 — 어긋나면 "벽돌만 있고 돈은 없는" 짧은 정산이 생긴다
		if (OfflineSeconds < MinSettleSeconds || RatePerSecond <= 0.f || Capacity <= 0)
		{
			return 0;
		}

		const int64 Raw = static_cast<int64>(RatePerSecond * OfflineSeconds);
		return FMath::Clamp<int64>(Raw, 0, static_cast<int64>(Capacity));
	}
}
