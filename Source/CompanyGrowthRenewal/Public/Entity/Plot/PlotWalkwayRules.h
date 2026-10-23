#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Templates/Function.h"

// Ground and entrance indices use row-major order: X + Y * Size.X.
struct FPlotWalkwayGrid
{
	FVector2D Origin = FVector2D::ZeroVector;
	float CellSize = 400.f;
	FIntPoint Size = FIntPoint::ZeroValue;
	TArray<uint8> Ground;
	TArray<int32> EntranceCells;
	// Verified inward clear-road depth by entrance cell; absent entries keep distance ordering.
	TMap<int32, double> EntranceDepth;
};

struct FPlotWalkwayTarget
{
	int32 Id = 0;
	FBox2D Bounds = FBox2D(ForceInit);
};

struct FPlotWalkwayConnector
{
	int32 TargetId = 0;
	int32 Cell = INDEX_NONE;
	FBox2D Bounds = FBox2D(ForceInit); // Nonzero-gap, cell-width passage to the target face.
};

struct FPlotWalkwayResult
{
	bool bValid = false;
	TArray<int32> PathCells;
	TSet<int32> ReachableTargetIds;
	TArray<FPlotWalkwayConnector> Connectors; // Sorted by TargetId; zero-gap contacts need no rect.
	TArray<int32> UsedEntranceCells; // Only entrances selected for a rendered route; sorted and unique.
};

struct FPlotRoadConnection
{
	FBox2D Bounds = FBox2D(ForceInit);
	FBox2D JunctionBounds = FBox2D(ForceInit);
	FVector2D Direction = FVector2D::ZeroVector;
};

struct FPlotRoadNetworkOptions
{
	double MinimumDeadEndLength = 0.0;
	double InitialLength = 0.0;
	double MinimumParallelGap = 0.0;
	TFunction<bool(const FBox2D&, const FVector2D&)> IsEdgeAllowed;
	TFunction<bool(const FVector2D&, const FVector2D&, FPlotRoadConnection&)> FindConnection;
};

struct FPlotRoadNetwork
{
	TArray<FBox2D> Segments;
	TArray<FVector2D> Directions;
	TArray<FBox2D> Junctions;
	TArray<FPlotRoadConnection> Connections;
	int32 ReachableNodeCount = 0;
};

struct COMPANYGROWTHRENEWAL_API FPlotWalkwayRules
{
	static FVector2D CellCenter(const FPlotWalkwayGrid& Grid, int32 Index);
	static FBox2D CellBounds(const FPlotWalkwayGrid& Grid, int32 Index);
	// Keeps the entrance face and full width, shortening only the inward end to a clear prefix.
	static FBox2D TrimStraightRoad(const FBox2D& GroundedRoad, const FVector2D& Inward,
		const TFunction<bool(const FBox2D&)>& IsOccupied);
	// Finds a sparse, full-width road union from the entrance toward forward/left/right extrema.
	static FPlotRoadNetwork BuildRoadNetwork(const FBox2D& SearchBounds, const FVector2D& Start,
		const FVector2D& Inward, double Width, double Step,
		const TFunction<bool(const FBox2D&)>& IsClear,
		const FPlotRoadNetworkOptions& Options = {});
	// When supplied, actual collision replaces target-box exclusion; target boxes remain contact goals.
	static FPlotWalkwayResult Solve(const FPlotWalkwayGrid& Grid,
		TConstArrayView<FPlotWalkwayTarget> Targets,
		TConstArrayView<FBox2D> FixedBlockers,
		bool bBuildPaths = true,
		const TFunction<bool(const FBox2D&)>& IsOccupied = {});
};
