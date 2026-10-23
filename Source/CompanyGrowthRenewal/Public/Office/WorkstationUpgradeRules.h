#pragma once

#include "CoreMinimal.h"
#include "Office/WorkstationTypes.h"
#include "Table/WorkstationTable.h"
#include "WorkstationUpgradeRules.generated.h"

USTRUCT(BlueprintType)
struct FWorkstationUpgradeQuote
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Workstation|Upgrade")
	bool bValid = false;
	UPROPERTY(BlueprintReadOnly, Category = "Workstation|Upgrade")
	bool bMaxed = false;
	UPROPERTY(BlueprintReadOnly, Category = "Workstation|Upgrade")
	int32 Count = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Workstation|Upgrade")
	int64 TotalCost = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Workstation|Upgrade")
	int64 Available = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Workstation|Upgrade")
	float CurrentBonus = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Workstation|Upgrade")
	float NextBonus = 0.0f;
};

namespace WorkstationUpgradeRules
{
	COMPANYGROWTHRENEWAL_API float GetCumulativeBonus(
		const FWorkstationUpgradeDefinition& Definition,
		EComputerSetupLevel Grade,
		int32 ComponentLevel);

	COMPANYGROWTHRENEWAL_API bool BuildQuote(
		const FWorkstationUpgradeDefinition& Definition,
		EComputerSetupLevel Grade,
		int32 CurrentLevel,
		int32 RequestedCount,
		bool bMaxAffordable,
		int64 Available,
		FWorkstationUpgradeQuote& OutQuote);
}
