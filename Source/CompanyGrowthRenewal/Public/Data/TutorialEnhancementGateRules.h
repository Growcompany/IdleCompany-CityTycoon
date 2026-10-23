#pragma once

#include "CoreMinimal.h"
#include "Data/BuildingEnhancementData.h"

struct FTutorialEnhancementGateRules
{
	// M7 수령 뒤 도출형 체인(M8~)부터 자금 강화를 가르치므로 튜토리얼 완료 전에도 연다
	static bool IsCompanyEnhancementUnlocked(
		bool bTutorialCompleted,
		bool bDerivedGuideActive)
	{
		return bTutorialCompleted || bDerivedGuideActive;
	}

	static bool IsPurchaseAllowed(
		bool bCompanyEnhancementUnlocked,
		EBuildingEnhancementType EnhancementType,
		bool bUnlockedByTier)
	{
		if (!bCompanyEnhancementUnlocked)
		{
			return EnhancementType == EBuildingEnhancementType::BuildingFloor;
		}

		return EnhancementType == EBuildingEnhancementType::BuildingFloor || bUnlockedByTier;
	}
};
