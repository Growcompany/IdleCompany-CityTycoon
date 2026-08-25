#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"
#include "Misc/Crc.h"

struct FVacantPlotFootprint
{
	FVector2D Center = FVector2D::ZeroVector;
	FVector2D HalfExtent = FVector2D::ZeroVector;

	FVacantPlotFootprint() = default;

	FVacantPlotFootprint(const FVector2D& InCenter, const FVector2D& InHalfExtent)
		: Center(InCenter)
		, HalfExtent(InHalfExtent)
	{
	}
};

struct FVacantPlotDressingRules
{
	static bool Overlaps(
		const FVacantPlotFootprint& A,
		const FVacantPlotFootprint& B,
		float SafetyPadding)
	{
		const float Padding = FMath::Max(0.f, SafetyPadding);
		return FMath::Abs(A.Center.X - B.Center.X) < (A.HalfExtent.X + B.HalfExtent.X + Padding)
			&& FMath::Abs(A.Center.Y - B.Center.Y) < (A.HalfExtent.Y + B.HalfExtent.Y + Padding);
	}

	static bool OverlapsAny(
		const FVacantPlotFootprint& Patch,
		TConstArrayView<FVacantPlotFootprint> Occupancies,
		float SafetyPadding)
	{
		for (const FVacantPlotFootprint& Occupancy : Occupancies)
		{
			if (Overlaps(Patch, Occupancy, SafetyPadding))
			{
				return true;
			}
		}
		return false;
	}

	static bool IsInsidePlot(
		const FVacantPlotFootprint& Patch,
		const FVector2D& PlotCenter,
		const FVector2D& PlotHalfExtent)
	{
		if (Patch.HalfExtent.X <= 0.f || Patch.HalfExtent.Y <= 0.f
			|| PlotHalfExtent.X <= 0.f || PlotHalfExtent.Y <= 0.f)
		{
			return false;
		}

		return FMath::Abs(Patch.Center.X - PlotCenter.X) + Patch.HalfExtent.X <= PlotHalfExtent.X
			&& FMath::Abs(Patch.Center.Y - PlotCenter.Y) + Patch.HalfExtent.Y <= PlotHalfExtent.Y;
	}

	static int32 ResolvePatchCount(int32 BuildingCapacity)
	{
		if (BuildingCapacity <= 0)
		{
			return 0;
		}
		if (BuildingCapacity <= 6)
		{
			return 2;
		}
		if (BuildingCapacity <= 12)
		{
			return 3;
		}
		return 4;
	}

	static uint32 StableHash(FName PlotId)
	{
		return PlotId.IsNone() ? 0u : FCrc::StrCrc32(*PlotId.ToString());
	}

	static int32 SelectPresetIndex(FName PlotId, int32 PatchIndex, int32 PresetCount)
	{
		if (PlotId.IsNone() || PatchIndex < 0 || PresetCount <= 0)
		{
			return INDEX_NONE;
		}

		uint32 Seed = StableHash(PlotId);
		Seed ^= 0x9e3779b9u + static_cast<uint32>(PatchIndex) + (Seed << 6u) + (Seed >> 2u);
		return static_cast<int32>(Seed % static_cast<uint32>(PresetCount));
	}

	static FVector2D RotateHalfExtent90(const FVector2D& HalfExtent, int32 QuarterTurns)
	{
		const int32 NormalizedTurns = ((QuarterTurns % 4) + 4) % 4;
		return (NormalizedTurns % 2) == 0
			? FVector2D(FMath::Abs(HalfExtent.X), FMath::Abs(HalfExtent.Y))
			: FVector2D(FMath::Abs(HalfExtent.Y), FMath::Abs(HalfExtent.X));
	}
};
