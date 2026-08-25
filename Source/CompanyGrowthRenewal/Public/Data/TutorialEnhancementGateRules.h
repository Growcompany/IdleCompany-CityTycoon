#pragma once

#include "CoreMinimal.h"
#include "Data/BuildingEnhancementData.h"

struct FTutorialEnhancementGateRules
{
	static bool IsPurchaseAllowed(
		bool bTutorialCompleted,
		EBuildingEnhancementType EnhancementType,
		bool bUnlockedByTier,
		bool bIsKeystone)
	{
		if (!bTutorialCompleted)
		{
			return EnhancementType == EBuildingEnhancementType::BuildingFloor;
		}

		if (bIsKeystone)
		{
			return EnhancementType == EBuildingEnhancementType::BuildingFloor
				|| EnhancementType == EBuildingEnhancementType::KeystoneAuraPower;
		}

		if (EnhancementType == EBuildingEnhancementType::KeystoneAuraPower)
		{
			return false;
		}

		return EnhancementType == EBuildingEnhancementType::BuildingFloor || bUnlockedByTier;
	}
};
