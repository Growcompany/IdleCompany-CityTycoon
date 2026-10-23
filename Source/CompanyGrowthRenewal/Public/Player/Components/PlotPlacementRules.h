#pragma once

#include "CoreMinimal.h"

struct FPlotPlacementRules
{
	// Plot geometry leaves one percent of the original full width beyond each outer edge.
	static double GetPlotCellSize(double PlotBaseCellSize)
	{
		return PlotBaseCellSize * 1.02;
	}

	static FVector ResolveInitialPlacementPosition(
		const FVector& CameraFocusPosition,
		const FVector& ProjectedGroundPosition,
		const FVector& FootprintCenterOffset)
	{
		// A raised camera focus sends the center ray behind the current area.
		// Search footprint centers at the focus XY while preserving the ground-plane Z.
		return FVector(
			CameraFocusPosition.X - FootprintCenterOffset.X,
			CameraFocusPosition.Y - FootprintCenterOffset.Y,
			ProjectedGroundPosition.Z);
	}

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

	// 레벨에 손으로 배치한 부지: 박스 반경 → 격자 칸 수. 셀에 못 미치는 잔여는 버린다(건물이 부지를 삐져나오면 안 됨).
	// Tolerance = 박스 extent 가 float 왕복(스냅 → 직렬화 → 로드)에서 한 틱 모자라게 돌아와 칸이 통째로
	// 날아가는 걸 막는다. 셀의 0.1%라 "거의 한 칸"을 한 칸으로 올려주지는 않는다.
	// FVector/FVector2D 성분이 double 이라 double 로 받는다(float 로 받으면 호출마다 축소변환 경고).
	static FIntPoint DeriveGridFromHalfExtent(double HalfX, double HalfY, double CellSize)
	{
		if (HalfX <= 0.0 || HalfY <= 0.0 || CellSize <= 0.0)
		{
			return FIntPoint(0, 0);
		}

		const double Tolerance = CellSize * 0.001;
		return FIntPoint(
			FMath::FloorToInt((2.0 * HalfX + Tolerance) / CellSize),
			FMath::FloorToInt((2.0 * HalfY + Tolerance) / CellSize));
	}

	// 격자 칸 수 → 수용량. 한 축이라도 0칸이면 건물이 안 들어가므로 0.
	static int32 DeriveCapacityFromGrid(FIntPoint Grid)
	{
		if (Grid.X <= 0 || Grid.Y <= 0)
		{
			return 0;
		}
		return Grid.X * Grid.Y;
	}

	// 드래그한 박스 반경을 셀 배수로 내림 스냅. 잔여 여백이 남아 수용량이 한 칸씩 흔들리는 걸 없앤다.
	// 0칸으로 줄면 스냅하지 않는다(사용자가 막 놓은 기본 크기 박스를 0으로 만들어 버리면 안 됨).
	static FVector2D SnapHalfExtentToCells(double HalfX, double HalfY, double CellSize)
	{
		const FIntPoint Grid = DeriveGridFromHalfExtent(HalfX, HalfY, CellSize);
		if (Grid.X <= 0 || Grid.Y <= 0)
		{
			return FVector2D(HalfX, HalfY);
		}
		return FVector2D(Grid.X * CellSize * 0.5, Grid.Y * CellSize * 0.5);
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
