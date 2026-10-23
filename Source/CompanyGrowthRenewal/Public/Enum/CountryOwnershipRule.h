#pragma once

#include "CoreMinimal.h"
#include "CountryOwnershipRule.generated.h"

/**
 * 「진출(Owned)」을 무엇으로 판정할지. 국가마다 다르다.
 *
 * 불리언 한 개로 두지 않는 이유는 ECountryMapState 와 같다 ― 판정 축이 이름을 가져야
 * 호출부에서 true/false 가 어느 쪽 규칙인지 되짚을 필요가 없다.
 */
UENUM(BlueprintType)
enum class ECountryOwnershipRule : uint8
{
	// 공장이나 채광을 지을 수 있는 국가. 시설이 1개 이상이어야 진출이다.
	Facility,

	// 시설 자체를 지을 수 없는 무역 허브. 시설 수로 판정하면 영원히 0 이라 진출에 도달하지 못하므로,
	// 해금되는 순간이 곧 진출이다(진출 가능 상태를 거치지 않는다).
	TradeHub
};
