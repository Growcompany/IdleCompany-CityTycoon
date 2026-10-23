#pragma once

#include "CoreMinimal.h"
#include "Data/BuildingEnhancementData.h"

namespace CGR::UI
{
	inline bool ShouldRefreshEnhancementLocksOnTutorialStateChange(
		bool bPreviousCompanyEnhancementUnlocked,
		bool bCurrentCompanyEnhancementUnlocked)
	{
		return bPreviousCompanyEnhancementUnlocked != bCurrentCompanyEnhancementUnlocked;
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
		bool bCompanyEnhancementUnlocked,
		bool bUnlockedByTier,
		bool bAuthorityAllowsPurchase)
	{
		if (!bCompanyEnhancementUnlocked && EnhancementType != EBuildingEnhancementType::BuildingFloor)
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
