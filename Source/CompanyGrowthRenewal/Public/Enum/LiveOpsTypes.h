#pragma once

#include "CoreMinimal.h"
#include "LiveOpsTypes.generated.h"

UENUM(BlueprintType)
enum class ELiveOpsEventType : uint8
{
	AttendanceEvent UMETA(DisplayName = "누적 출석"),
	CollectEvent    UMETA(DisplayName = "수집"),
	HotTimeEvent    UMETA(DisplayName = "핫타임"),
};
