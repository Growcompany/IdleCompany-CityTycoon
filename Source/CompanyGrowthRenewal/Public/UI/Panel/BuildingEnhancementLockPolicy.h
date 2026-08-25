#pragma once

#include "CoreMinimal.h"
#include "Data/BuildingEnhancementData.h"

namespace CGR::UI
{
	inline bool ShouldRefreshEnhancementLocksOnTutorialStateChange(
		bool bPreviousTutorialCompleted,
		bool bCurrentTutorialCompleted)
	{
		return bPreviousTutorialCompleted != bCurrentTutorialCompleted;
	}

	enum class EBuildingEnhancementLockReason : uint8
	{
		None,
		Tutorial,
		Tier,
		Authority
	};

	inline EBuildingEnhancementLockReason ResolveEnhancementLockReason(
		EBuildingEnhancementType EnhancementType,
		bool bTutorialCompleted,
		bool bUnlockedByTier,
		bool bAuthorityAllowsPurchase)
	{
		if (!bTutorialCompleted && EnhancementType != EBuildingEnhancementType::BuildingFloor)
		{
			return EBuildingEnhancementLockReason::Tutorial;
		}

		if (!bUnlockedByTier)
		{
			return EBuildingEnhancementLockReason::Tier;
		}

		return bAuthorityAllowsPurchase
			? EBuildingEnhancementLockReason::None
			: EBuildingEnhancementLockReason::Authority;
	}
}
