#pragma once

#include "CoreMinimal.h"
#include "Table/CountryInfoTable.h"

namespace ManufacturingGuideCountryRules
{
	inline TArray<EResourceType> MissingMaterials(const TArray<TPair<EResourceType, int32>>& RecipeMaterials,
		const TMap<EResourceType, int64>& Inventory, int32 ProductQuantity)
	{
		TArray<EResourceType> Missing;
		for (const TPair<EResourceType, int32>& Material : RecipeMaterials)
		{
			const int64 Available = Inventory.FindRef(Material.Key);
			if (Available < static_cast<int64>(Material.Value) * ProductQuantity) { Missing.Add(Material.Key); }
		}
		return Missing;
	}

	inline bool ShouldRecoverMaterials(bool bHasMatchingFactoryLine, bool bHasPendingOrder,
		bool bHasRecipe, const TArray<EResourceType>& Missing)
	{
		return !bHasMatchingFactoryLine && bHasPendingOrder && bHasRecipe && !Missing.IsEmpty();
	}

	inline bool SupportsFactoryIndustry(const FCountryInfoTable& Country, ECompanyType Industry)
	{
		return Country.bSupportsFactory && (Industry == ECompanyType::None
			|| Country.SupportedIndustries.IsEmpty() || Country.SupportedIndustries.Contains(Industry));
	}

	// 관련 없는 광산은 후보에서 제외한다. 부족 자원을 생산하는 기존 라인을 가장 먼저 안내한다.
	inline int32 MinePriority(const FCountryInfoTable& Country,
		const TArray<TPair<EResourceType, int32>>& RecipeMaterials,
		const TArray<EResourceType>& NeededMaterials,
		const TArray<EResourceType>& ActiveResources)
	{
		if (!Country.bSupportsMine) { return INDEX_NONE; }
		bool bHasRecipeResource = false;
		bool bHasNeededResource = false;
		bool bHasRelevantLine = false;
		for (const TPair<EResourceType, int32>& Material : RecipeMaterials)
		{
			if (!Country.MinableResources.Contains(Material.Key)) { continue; }
			bHasRecipeResource = true;
			const bool bNeeded = NeededMaterials.Contains(Material.Key);
			bHasNeededResource |= bNeeded;
			bHasRelevantLine |= ActiveResources.Contains(Material.Key) && (bNeeded || NeededMaterials.IsEmpty());
		}
		if (!bHasRecipeResource) { return INDEX_NONE; }
		return bHasNeededResource ? (bHasRelevantLine ? 4 : 3) : (bHasRelevantLine ? 2 : 1);
	}
}
