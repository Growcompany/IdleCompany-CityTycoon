#pragma once

#include "CoreMinimal.h"

struct FPlotPlacementRules
{
	static int32 FindNearestValidCandidateIndex(
		const FVector2D& DesiredPosition,
		TArrayView<const FVector2D> CandidatePositions,
		TArrayView<const uint8> CandidateValidity)
	{
		if (CandidatePositions.Num() != CandidateValidity.Num())
		{
			return INDEX_NONE;
		}

		int32 BestIndex = INDEX_NONE;
		double BestDistanceSq = TNumericLimits<double>::Max();
		for (int32 CandidateIndex = 0; CandidateIndex < CandidatePositions.Num(); ++CandidateIndex)
		{
			if (CandidateValidity[CandidateIndex] == 0)
			{
				continue;
			}

			const double DistanceSq = FVector2D::DistSquared(
				DesiredPosition,
				CandidatePositions[CandidateIndex]);
			if (DistanceSq < BestDistanceSq)
			{
				BestDistanceSq = DistanceSq;
				BestIndex = CandidateIndex;
			}
		}

		return BestIndex;
	}

	static bool HasCapacity(int32 CurrentBuildingCount, int32 BuildingCapacity, bool bExcludeMovingBuilding)
	{
		if (CurrentBuildingCount < 0 || BuildingCapacity <= 0)
		{
			return false;
		}

		const int32 EffectiveBuildingCount = FMath::Max(
			0,
			CurrentBuildingCount - (bExcludeMovingBuilding ? 1 : 0));
		return EffectiveBuildingCount < BuildingCapacity;
	}

	static bool CanFootprintFitPlot(
		int32 PlotCols,
		int32 PlotRows,
		int32 FootprintWidthCells,
		int32 FootprintDepthCells)
	{
		if (PlotCols <= 0 || PlotRows <= 0 || FootprintWidthCells <= 0 || FootprintDepthCells <= 0)
		{
			return false;
		}

		const bool bFitsCurrentOrientation =
			FootprintWidthCells <= PlotCols && FootprintDepthCells <= PlotRows;
		const bool bFitsRotatedOrientation =
			FootprintDepthCells <= PlotCols && FootprintWidthCells <= PlotRows;
		return bFitsCurrentOrientation || bFitsRotatedOrientation;
	}

	static bool DoesCurrentOrientationFitWithinPlot(
		float PlotHalfX,
		float PlotHalfY,
		float FootprintHalfX,
		float FootprintHalfY)
	{
		if (PlotHalfX <= 0.f || PlotHalfY <= 0.f || FootprintHalfX <= 0.f || FootprintHalfY <= 0.f)
		{
			return false;
		}

		return FootprintHalfX <= PlotHalfX && FootprintHalfY <= PlotHalfY;
	}
};
