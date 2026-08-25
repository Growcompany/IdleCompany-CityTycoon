#pragma once

#include "CoreMinimal.h"
#include "FacilityType.generated.h"

UENUM(BlueprintType)
enum class EFacilityType : uint8
{
	None        UMETA(DisplayName = "None"),
	Mine        UMETA(DisplayName = "채광소"),
	Factory     UMETA(DisplayName = "공장"),
	Refinery    UMETA(DisplayName = "정유시설"),
	PowerPlant  UMETA(DisplayName = "발전소"),
	TradePort   UMETA(DisplayName = "무역항"),
};
