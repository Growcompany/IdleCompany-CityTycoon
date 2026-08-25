#pragma once

#include "CoreMinimal.h"

struct FWorkstationCapacityDecision
{
	bool bCanPlace = false;
	bool bReachesCapacity = false;
};

struct FWorkstationCapacityRules
{
	static FWorkstationCapacityDecision Evaluate(
		int32 CurrentSeats,
		int32 CandidateSeats,
		int32 EmployeeCapacity)
	{
		if (CurrentSeats < 0 || CandidateSeats <= 0 || EmployeeCapacity <= 0
			|| CurrentSeats >= EmployeeCapacity)
		{
			return {};
		}

		const int32 RemainingSeats = EmployeeCapacity - CurrentSeats;
		if (CandidateSeats > RemainingSeats)
		{
			return {};
		}

		return { true, CandidateSeats == RemainingSeats };
	}
};

struct FWorkstationMissionProgressRules
{
	static int32 GetProgressIncrement(int32 Count)
	{
		return FMath::Max(1, Count);
	}

	static int32 ResolveRestoredProgress(
		FName SavedMissionID,
		FName ActiveMissionID,
		bool bIsPlaceDesksMission,
		int32 SavedProgress,
		int64 TargetProgress)
	{
		if (!bIsPlaceDesksMission || SavedMissionID.IsNone() || SavedMissionID != ActiveMissionID)
		{
			return 0;
		}

		const int32 ClampedTarget = static_cast<int32>(FMath::Clamp(
			TargetProgress,
			static_cast<int64>(0),
			static_cast<int64>(MAX_int32)));
		return FMath::Clamp(SavedProgress, 0, ClampedTarget);
	}

	static bool ShouldSaveAfterPlacement(
		bool bWasPlaceDesksMission,
		bool bIsPlaceDesksMission)
	{
		return !bWasPlaceDesksMission || bIsPlaceDesksMission;
	}
};
