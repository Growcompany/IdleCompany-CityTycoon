#pragma once

#include "CoreMinimal.h"
#include "CountryMapState.generated.h"

/**
 * 전략 지도 판 위에서 한 국가가 놓인 상태.
 * 링 색과 영역 덮개가 구분해야 하는 것은 이 셋뿐이다 ― 불리언 두 개로 쪼개면
 * (진출했는데 잠김) 같은 존재하지 않는 조합이 생긴다.
 */
UENUM(BlueprintType)
enum class ECountryMapState : uint8
{
	Owned		UMETA(DisplayName = "진출"),
	Available	UMETA(DisplayName = "진출 가능"),
	Locked		UMETA(DisplayName = "잠김")
};
