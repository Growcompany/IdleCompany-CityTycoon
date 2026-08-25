#pragma once

#include "CoreMinimal.h"
#include "FloatingTextType.generated.h"

// BP E_DamageTextValueTypes 순서와 동일하게 유지할 것!
UENUM(BlueprintType)
enum class EFloatingTextType : uint8
{
	Normal = 0   UMETA(DisplayName = "Normal"),
	Income = 1   UMETA(DisplayName = "Income"),
	Working = 2  UMETA(DisplayName = "Working"),
	Fatigue = 3  UMETA(DisplayName = "Fatigue"),
	Bonus = 4    UMETA(DisplayName = "Bonus"),
	Count        UMETA(Hidden)
};
