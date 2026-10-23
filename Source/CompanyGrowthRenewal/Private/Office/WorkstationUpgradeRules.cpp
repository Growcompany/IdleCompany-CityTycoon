#include "Office/WorkstationUpgradeRules.h"

namespace
{
	bool TryGetLevelCost(const FWorkstationUpgradeDefinition& Definition, int32 GradeNumber, int32 TargetLevel, int64& OutCost)
	{
		const double Cost = static_cast<double>(Definition.BaseCost)
			* FMath::Pow(Definition.CostGrowthRate, TargetLevel - 2)
			* FMath::Pow(Definition.GradeCostMultiplier, GradeNumber - 1);
		if (!FMath::IsFinite(Cost) || Cost < 0.0 || Cost >= static_cast<double>(MAX_int64))
		{
			return false;
		}
		OutCost = FMath::Max<int64>(1, FMath::RoundToInt64(Cost));
		return true;
	}
}

float WorkstationUpgradeRules::GetCumulativeBonus(
	const FWorkstationUpgradeDefinition& Definition,
	EComputerSetupLevel Grade,
	int32 ComponentLevel)
{
	if (!Definition.IsValidConfiguration() || ComponentLevel < 1 || ComponentLevel > Definition.MaxLevel
		|| static_cast<uint8>(Grade) > static_cast<uint8>(EComputerSetupLevel::Level6Twin))
	{
		return 0.0f;
	}
	const int32 GradeNumber = GetComputerSetupLevelNumber(Grade);
	const double CumulativeUnits = static_cast<double>(GradeNumber - 1) * (Definition.MaxLevel - 1)
		+ ComponentLevel - 1;
	if (Definition.EffectCurve == EWorkstationEffectCurve::Compound)
	{
		return static_cast<float>(FMath::Pow(1.0 + static_cast<double>(Definition.EffectPerLevel), CumulativeUnits) - 1.0);
	}
	return static_cast<float>(CumulativeUnits * Definition.EffectPerLevel);
}

bool WorkstationUpgradeRules::BuildQuote(
	const FWorkstationUpgradeDefinition& Definition,
	EComputerSetupLevel Grade,
	int32 CurrentLevel,
	int32 RequestedCount,
	bool bMaxAffordable,
	int64 Available,
	FWorkstationUpgradeQuote& OutQuote)
{
	OutQuote = FWorkstationUpgradeQuote();
	OutQuote.Available = FMath::Max<int64>(0, Available);
	if (!Definition.IsValidConfiguration() || CurrentLevel < 1 || CurrentLevel > Definition.MaxLevel
		|| static_cast<uint8>(Grade) > static_cast<uint8>(EComputerSetupLevel::Level6Twin)
		|| (!bMaxAffordable && RequestedCount < 1))
	{
		return false;
	}
	OutQuote.bValid = true;
	OutQuote.bMaxed = CurrentLevel == Definition.MaxLevel;
	OutQuote.CurrentBonus = GetCumulativeBonus(Definition, Grade, CurrentLevel);
	OutQuote.NextBonus = OutQuote.CurrentBonus;
	if (OutQuote.bMaxed)
	{
		return true;
	}
	const int32 Remaining = Definition.MaxLevel - CurrentLevel;
	const int32 Limit = bMaxAffordable ? Remaining : FMath::Min(Remaining, RequestedCount);
	for (int32 Offset = 1; Offset <= Limit; ++Offset)
	{
		int64 Cost = 0;
		if (!TryGetLevelCost(Definition, GetComputerSetupLevelNumber(Grade), CurrentLevel + Offset, Cost)
			|| Cost > MAX_int64 - OutQuote.TotalCost)
		{
			OutQuote.bValid = false;
			return false;
		}
		if (bMaxAffordable && Cost > OutQuote.Available - OutQuote.TotalCost)
		{
			if (OutQuote.Count == 0)
			{
				OutQuote.TotalCost = Cost;
			}
			break;
		}
		OutQuote.TotalCost += Cost;
		++OutQuote.Count;
		OutQuote.NextBonus = GetCumulativeBonus(Definition, Grade, CurrentLevel + OutQuote.Count);
	}
	return true;
}
