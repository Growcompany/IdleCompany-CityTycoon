#pragma once

#include "CoreMinimal.h"

namespace ManufacturingReservationRules
{
	inline double DiscountedUnitCost(double BaseCost, double ProductionCountBonus)
	{
		if (!FMath::IsFinite(BaseCost) || BaseCost <= 0.0 || !FMath::IsFinite(ProductionCountBonus)) return 0.0;
		const double Bonus = FMath::Max(0.0, ProductionCountBonus);
		return BaseCost * (8.0 / (8.0 + Bonus));
	}

	inline bool HasValidCosts(const TArray<double>& Costs)
	{
		if (Costs.Num() != 3) return false;
		bool bHasRequiredField = false;
		for (const double Cost : Costs)
		{
			if (!FMath::IsFinite(Cost) || Cost < 0.0) return false;
			bHasRequiredField |= Cost > 0.0;
		}
		return bHasRequiredField;
	}

	inline TArray<double> DiscountedCosts(const TArray<double>& BaseCosts, double ProductionCountBonus)
	{
		TArray<double> Result;
		if (!HasValidCosts(BaseCosts) || !FMath::IsFinite(ProductionCountBonus)) return Result;
		Result.Reserve(BaseCosts.Num());
		for (const double Cost : BaseCosts)
		{
			const double Discounted = Cost > 0.0 ? DiscountedUnitCost(Cost, ProductionCountBonus) : 0.0;
			if (!FMath::IsFinite(Discounted) || (Cost > 0.0 && Discounted <= 0.0)) return {};
			Result.Add(Discounted);
		}
		return HasValidCosts(Result) ? Result : TArray<double>();
	}

	inline int32 MaxQuantity(const TArray<double>& Points, const TArray<double>& Costs)
	{
		if (Points.Num() != 3 || !HasValidCosts(Costs)) return 0;
		int32 MaxAffordable = MAX_int32;
		for (int32 Index = 0; Index < Points.Num(); ++Index)
		{
			if (!FMath::IsFinite(Points[Index]) || Points[Index] < 0.0) return 0;
			if (Costs[Index] <= 0.0) continue;
			const double Affordable = FMath::FloorToDouble(Points[Index] / Costs[Index]);
			if (Affordable < 1.0) return 0;
			if (FMath::IsFinite(Affordable) && Affordable < static_cast<double>(MaxAffordable))
			{
				MaxAffordable = static_cast<int32>(Affordable);
			}
		}
		return MaxAffordable;
	}

	inline bool CanReserve(const TArray<double>& Points, const TArray<double>& Costs, int32 Quantity)
	{
		if (Quantity <= 0 || Points.Num() != 3 || !HasValidCosts(Costs)) return false;
		for (int32 Index = 0; Index < Points.Num(); ++Index)
		{
			if (!FMath::IsFinite(Points[Index]) || Points[Index] < 0.0) return false;
			const double Total = Costs[Index] * static_cast<double>(Quantity);
			if (!FMath::IsFinite(Total) || Total > Points[Index]) return false;
		}
		return true;
	}
}
