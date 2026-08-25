#pragma once

#include "CoreMinimal.h"
#include "TraitSortMode.generated.h"

UENUM(BlueprintType)
enum class ETraitSortMode : uint8
{
	Rarity   UMETA(DisplayName = "Rarity"),
	Quantity UMETA(DisplayName = "Quantity"),
	Name     UMETA(DisplayName = "Name"),
};
