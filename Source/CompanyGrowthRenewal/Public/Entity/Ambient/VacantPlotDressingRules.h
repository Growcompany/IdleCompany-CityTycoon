#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"
#include "Containers/Set.h"
#include "Misc/Crc.h"
#include "Entity/Plot/CityPlotActor.h"

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

struct FVacantPlotBoundaryPresetParams
{
	FVector2D HalfExtent = FVector2D::ZeroVector;
	float Spacing = 0.f;
	float Inset = 0.f;
	int32 MaxPatches = 0;
};

struct FVacantPlotDressingRules
{
	static constexpr int32 MaxCandidateCount = 512;
	static constexpr int32 MaxPatchesPerPlotWorkBound = 12;
	static constexpr int32 MaxBoundaryTreesPerPlot = 96;
	static constexpr int32 MaxBoundaryTreesHardBound = 128;

	static FVacantPlotBoundaryPresetParams CombineBoundaryPresets(
		TConstArrayView<FVacantPlotBoundaryPresetParams> Presets)
	{
		FVacantPlotBoundaryPresetParams Combined;
		if (Presets.IsEmpty())
		{
			return Combined;
		}
		Combined.MaxPatches = Presets[0].MaxPatches;
		for (const FVacantPlotBoundaryPresetParams& Preset : Presets)
		{
			Combined.HalfExtent.X = FMath::Max(Combined.HalfExtent.X, Preset.HalfExtent.X);
			Combined.HalfExtent.Y = FMath::Max(Combined.HalfExtent.Y, Preset.HalfExtent.Y);
			Combined.Spacing = FMath::Max(Combined.Spacing, Preset.Spacing);
			Combined.Inset = FMath::Max(Combined.Inset, Preset.Inset);
			Combined.MaxPatches = FMath::Min(Combined.MaxPatches, Preset.MaxPatches);
		}
		return Combined;
	}

	static void GenerateBoundaryCenters(
		TConstArrayView<FCityPlotBoundarySegment> Segments,
		const FVector2D& TreeHalfExtent,
		float RequestedSpacing,
		float BoundaryInset,
		int32 RequestedMaxCount,
		TArray<FVector2D>& OutCenters)
	{
		OutCenters.Reset();
		if (TreeHalfExtent.X <= 0.f || TreeHalfExtent.Y <= 0.f
			|| !FMath::IsFinite(RequestedSpacing) || RequestedSpacing <= 0.f
			|| !FMath::IsFinite(BoundaryInset) || BoundaryInset < 0.f)
		{
			return;
		}
		const int32 MaxCount = FMath::Clamp(RequestedMaxCount, 0, MaxBoundaryTreesHardBound);
		if (MaxCount == 0)
		{
			return;
		}
		const float CornerMargin = FMath::Max(TreeHalfExtent.X, TreeHalfExtent.Y)
			+ BoundaryInset;
		const float MinimumSpacing = RequestedSpacing;
		auto CountAtSpacing = [&](float Spacing)
		{
			int32 Count = 0;
			for (const FCityPlotBoundarySegment& Segment : Segments)
			{
				const FVector2D Delta = Segment.End - Segment.Start;
				const bool bAxisAligned = FMath::IsNearlyZero(Delta.X, 0.1f)
					!= FMath::IsNearlyZero(Delta.Y, 0.1f);
				const bool bNormalAligned = (FMath::IsNearlyEqual(FMath::Abs(Segment.InwardNormal.X), 1.f, 0.01f)
					&& FMath::IsNearlyZero(Segment.InwardNormal.Y, 0.01f))
					|| (FMath::IsNearlyEqual(FMath::Abs(Segment.InwardNormal.Y), 1.f, 0.01f)
					&& FMath::IsNearlyZero(Segment.InwardNormal.X, 0.01f));
				if (!bAxisAligned || !bNormalAligned || !FMath::IsFinite(Delta.X)
					|| !FMath::IsFinite(Delta.Y) || !FMath::IsFinite(Segment.Start.X)
					|| !FMath::IsFinite(Segment.Start.Y))
				{
					continue;
				}
				const float UsableLength = Delta.Size() - 2.f * CornerMargin;
				if (UsableLength >= 0.f)
				{
					Count += FMath::FloorToInt(UsableLength / Spacing) + 1;
				}
			}
			return Count;
		};

		float Spacing = MinimumSpacing;
		if (CountAtSpacing(Spacing) > MaxCount)
		{
			float UpperSpacing = Spacing;
			while (CountAtSpacing(UpperSpacing) > MaxCount && UpperSpacing < 10000000.f)
			{
				UpperSpacing *= 2.f;
			}
			float LowerSpacing = Spacing;
			for (int32 Iteration = 0; Iteration < 24; ++Iteration)
			{
				const float Midpoint = (LowerSpacing + UpperSpacing) * 0.5f;
				if (CountAtSpacing(Midpoint) > MaxCount)
				{
					LowerSpacing = Midpoint;
				}
				else
				{
					UpperSpacing = Midpoint;
				}
			}
			Spacing = UpperSpacing;
		}

		TArray<TArray<FVector2D>> CentersBySegment;
		for (const FCityPlotBoundarySegment& Segment : Segments)
		{
			const FVector2D Delta = Segment.End - Segment.Start;
			const bool bAxisAligned = FMath::IsNearlyZero(Delta.X, 0.1f)
				!= FMath::IsNearlyZero(Delta.Y, 0.1f);
			const float Length = Delta.Size();
			TArray<FVector2D>& SegmentCenters = CentersBySegment.AddDefaulted_GetRef();
			if (!bAxisAligned || Length < 2.f * CornerMargin
				|| !FMath::IsFinite(Length) || !FMath::IsFinite(Segment.InwardNormal.X)
				|| !FMath::IsFinite(Segment.InwardNormal.Y))
			{
				continue;
			}
			const FVector2D Direction = Delta / Length;
			if (!FMath::IsNearlyZero(FVector2D::DotProduct(Direction, Segment.InwardNormal), 0.01f)
				|| !FMath::IsNearlyEqual(Segment.InwardNormal.SizeSquared(), 1.f, 0.01f))
			{
				continue;
			}
			const float NormalHalfExtent = FMath::Abs(Segment.InwardNormal.X) * TreeHalfExtent.X
				+ FMath::Abs(Segment.InwardNormal.Y) * TreeHalfExtent.Y;
			const float Inset = NormalHalfExtent + BoundaryInset;
			const int32 Count = FMath::FloorToInt((Length - 2.f * CornerMargin) / Spacing) + 1;
			const float FirstDistance = (Length - (Count - 1) * Spacing) * 0.5f;
			for (int32 Index = 0; Index < Count; ++Index)
			{
				SegmentCenters.Add(Segment.Start + Direction * (FirstDistance + Index * Spacing)
					+ Segment.InwardNormal * Inset);
			}
		}
		for (int32 Index = 0; OutCenters.Num() < MaxCount; ++Index)
		{
			bool bAdded = false;
			for (const TArray<FVector2D>& SegmentCenters : CentersBySegment)
			{
				if (SegmentCenters.IsValidIndex(Index) && OutCenters.Num() < MaxCount)
				{
					const FVector2D& Candidate = SegmentCenters[Index];
					const bool bTooClose = OutCenters.ContainsByPredicate(
						[&](const FVector2D& Existing)
						{
							return FVector2D::DistSquared(Candidate, Existing)
								< FMath::Square(FMath::Max(0.f, RequestedSpacing - 0.1f));
						});
					if (!bTooClose)
					{
						OutCenters.Add(Candidate);
					}
					bAdded = true;
				}
			}
			if (!bAdded)
			{
				break;
			}
		}
	}

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

	static bool IsBlockedByReservedPatch(
		const FVacantPlotFootprint& Patch,
		bool bFixedAnchor,
		TConstArrayView<FVacantPlotFootprint> ReservedPatches,
		float SafetyPadding)
	{
		return !bFixedAnchor && OverlapsAny(Patch, ReservedPatches, SafetyPadding);
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

	static void GenerateCandidateCenters(
		const FVector2D& PlotCenter,
		const FVector2D& PlotHalfExtent,
		const FVector2D& PatchHalfExtent,
		TConstArrayView<FVacantPlotFootprint> Occupancies,
		float SafetyPadding,
		TArray<FVector2D>& OutCandidateCenters)
	{
		OutCandidateCenters.Reset();
		if (PlotHalfExtent.X <= 0.f || PlotHalfExtent.Y <= 0.f
			|| PatchHalfExtent.X <= 0.f || PatchHalfExtent.Y <= 0.f)
		{
			return;
		}

		TSet<FIntPoint> QuantizedCenters;
		auto TryAddCandidate = [&](const FVector2D& CandidateCenter)
		{
			if (OutCandidateCenters.Num() >= MaxCandidateCount
				|| !FMath::IsFinite(CandidateCenter.X)
				|| !FMath::IsFinite(CandidateCenter.Y))
			{
				return;
			}

			const FIntPoint QuantizedCenter(
				FMath::RoundToInt(CandidateCenter.X),
				FMath::RoundToInt(CandidateCenter.Y));
			if (QuantizedCenters.Contains(QuantizedCenter))
			{
				return;
			}

			const FVector2D SnappedCenter(
				static_cast<float>(QuantizedCenter.X),
				static_cast<float>(QuantizedCenter.Y));
			const FVacantPlotFootprint Candidate(SnappedCenter, PatchHalfExtent);
			if (!IsInsidePlot(Candidate, PlotCenter, PlotHalfExtent)
				|| OverlapsAny(Candidate, Occupancies, SafetyPadding))
			{
				return;
			}

			QuantizedCenters.Add(QuantizedCenter);
			OutCandidateCenters.Add(SnappedCenter);
		};

		TryAddCandidate(PlotCenter);
		const FVector2D AvailableHalfExtent(
			PlotHalfExtent.X - PatchHalfExtent.X,
			PlotHalfExtent.Y - PatchHalfExtent.Y);
		if (AvailableHalfExtent.X < 0.f || AvailableHalfExtent.Y < 0.f)
		{
			return;
		}

		constexpr int32 GridResolution = 7;
		for (int32 RowIndex = 0; RowIndex < GridResolution; ++RowIndex)
		{
			const double YAlpha = static_cast<double>(RowIndex)
				/ static_cast<double>(GridResolution - 1);
			const double CandidateY = PlotCenter.Y + FMath::Lerp(
				-AvailableHalfExtent.Y, AvailableHalfExtent.Y, YAlpha);
			for (int32 ColumnIndex = 0; ColumnIndex < GridResolution; ++ColumnIndex)
			{
				const double XAlpha = static_cast<double>(ColumnIndex)
					/ static_cast<double>(GridResolution - 1);
				TryAddCandidate(FVector2D(
					PlotCenter.X + FMath::Lerp(
						-AvailableHalfExtent.X, AvailableHalfExtent.X, XAlpha),
					CandidateY));
			}
		}

		TArray<FVacantPlotFootprint> SortedOccupancies;
		SortedOccupancies.Reserve(Occupancies.Num());
		for (const FVacantPlotFootprint& Occupancy : Occupancies)
		{
			SortedOccupancies.Add(Occupancy);
		}
		SortedOccupancies.Sort([](
			const FVacantPlotFootprint& Left,
			const FVacantPlotFootprint& Right)
		{
			if (Left.Center.X != Right.Center.X)
			{
				return Left.Center.X < Right.Center.X;
			}
			if (Left.Center.Y != Right.Center.Y)
			{
				return Left.Center.Y < Right.Center.Y;
			}
			if (Left.HalfExtent.X != Right.HalfExtent.X)
			{
				return Left.HalfExtent.X < Right.HalfExtent.X;
			}
			return Left.HalfExtent.Y < Right.HalfExtent.Y;
		});

		const float Padding = FMath::Max(0.f, SafetyPadding);
		for (const FVacantPlotFootprint& Occupancy : SortedOccupancies)
		{
			const double SeparationX = FMath::Abs(Occupancy.HalfExtent.X)
				+ PatchHalfExtent.X + Padding;
			const double SeparationY = FMath::Abs(Occupancy.HalfExtent.Y)
				+ PatchHalfExtent.Y + Padding;
			const FVector2D ObstacleOffsets[] = {
				FVector2D(-SeparationX, 0.f),
				FVector2D(SeparationX, 0.f),
				FVector2D(0.f, -SeparationY),
				FVector2D(0.f, SeparationY),
				FVector2D(-SeparationX, -SeparationY),
				FVector2D(SeparationX, -SeparationY),
				FVector2D(-SeparationX, SeparationY),
				FVector2D(SeparationX, SeparationY)
			};
			for (const FVector2D& Offset : ObstacleOffsets)
			{
				TryAddCandidate(Occupancy.Center + Offset);
			}
		}
	}

	static void RankFreeCandidateIndices(
		FName PlotId,
		int32 PatchOrdinal,
		const FVector2D& PlotCenter,
		const FVector2D& PlotHalfExtent,
		const FVector2D& PatchHalfExtent,
		TConstArrayView<FVacantPlotFootprint> Occupancies,
		float SafetyPadding,
		TConstArrayView<FVector2D> CandidateCenters,
		TArray<int32>& OutRankedIndices)
	{
		OutRankedIndices.Reset();
		if (PlotId.IsNone() || PatchOrdinal < 0
			|| PlotHalfExtent.X <= 0.f || PlotHalfExtent.Y <= 0.f
			|| PatchHalfExtent.X <= 0.f || PatchHalfExtent.Y <= 0.f)
		{
			return;
		}

		struct FRankedCandidate
		{
			int32 CandidateIndex = INDEX_NONE;
			double MinimumClearance = 0.0;
			uint32 TieBreak = 0u;
			FIntPoint QuantizedCenter = FIntPoint::ZeroValue;
		};

		TArray<FRankedCandidate> RankedCandidates;
		const int32 CandidateWorkCount = FMath::Min(
			CandidateCenters.Num(), MaxCandidateCount);
		RankedCandidates.Reserve(CandidateWorkCount);
		const float Padding = FMath::Max(0.f, SafetyPadding);
		for (int32 CandidateIndex = 0;
			CandidateIndex < CandidateWorkCount;
			++CandidateIndex)
		{
			const FVector2D& CandidateCenter = CandidateCenters[CandidateIndex];
			if (!FMath::IsFinite(CandidateCenter.X) || !FMath::IsFinite(CandidateCenter.Y))
			{
				continue;
			}

			const FVacantPlotFootprint Candidate(CandidateCenter, PatchHalfExtent);
			if (!IsInsidePlot(Candidate, PlotCenter, PlotHalfExtent)
				|| OverlapsAny(Candidate, Occupancies, Padding))
			{
				continue;
			}

			double MinimumClearance = FMath::Min(
				PlotHalfExtent.X - FMath::Abs(CandidateCenter.X - PlotCenter.X)
					- PatchHalfExtent.X,
				PlotHalfExtent.Y - FMath::Abs(CandidateCenter.Y - PlotCenter.Y)
					- PatchHalfExtent.Y);
			for (const FVacantPlotFootprint& Occupancy : Occupancies)
			{
				const double ClearanceX = FMath::Max(
					0.0,
					FMath::Abs(CandidateCenter.X - Occupancy.Center.X)
						- (PatchHalfExtent.X + Occupancy.HalfExtent.X + Padding));
				const double ClearanceY = FMath::Max(
					0.0,
					FMath::Abs(CandidateCenter.Y - Occupancy.Center.Y)
						- (PatchHalfExtent.Y + Occupancy.HalfExtent.Y + Padding));
				MinimumClearance = FMath::Min(
					MinimumClearance,
					FVector2D(ClearanceX, ClearanceY).Size());
			}

			FRankedCandidate& Ranked = RankedCandidates.AddDefaulted_GetRef();
			Ranked.CandidateIndex = CandidateIndex;
			Ranked.MinimumClearance = MinimumClearance;
			Ranked.QuantizedCenter = FIntPoint(
				FMath::RoundToInt(CandidateCenter.X),
				FMath::RoundToInt(CandidateCenter.Y));
			const FString TieBreakSeed = FString::Printf(
				TEXT("%s|%d|%d|%d"),
				*PlotId.ToString(),
				PatchOrdinal,
				Ranked.QuantizedCenter.X,
				Ranked.QuantizedCenter.Y);
			Ranked.TieBreak = FCrc::StrCrc32(*TieBreakSeed);
		}

		RankedCandidates.Sort([](
			const FRankedCandidate& Left,
			const FRankedCandidate& Right)
		{
			if (Left.MinimumClearance != Right.MinimumClearance)
			{
				return Left.MinimumClearance > Right.MinimumClearance;
			}
			if (Left.TieBreak != Right.TieBreak)
			{
				return Left.TieBreak < Right.TieBreak;
			}
			if (Left.QuantizedCenter.X != Right.QuantizedCenter.X)
			{
				return Left.QuantizedCenter.X < Right.QuantizedCenter.X;
			}
			if (Left.QuantizedCenter.Y != Right.QuantizedCenter.Y)
			{
				return Left.QuantizedCenter.Y < Right.QuantizedCenter.Y;
			}
			return false;
		});

		OutRankedIndices.Reserve(RankedCandidates.Num());
		for (const FRankedCandidate& Ranked : RankedCandidates)
		{
			OutRankedIndices.Add(Ranked.CandidateIndex);
		}
	}

	static int32 ResolvePatchCount(int32 BuildingCapacity)
	{
		if (BuildingCapacity <= 0)
		{
			return 0;
		}
		if (BuildingCapacity >= 48)
		{
			return 2;
		}
		return 1;
	}

	static int32 ResolvePatchCount(int32 BuildingCapacity, int32 PlotCellsPerPatch,
		int32 MaxPatchesPerPlot)
	{
		if (BuildingCapacity <= 0 || MaxPatchesPerPlot <= 0 || PlotCellsPerPatch < 0)
		{
			return 0;
		}
		return FMath::Min(FMath::Min(MaxPatchesPerPlot, MaxPatchesPerPlotWorkBound),
			PlotCellsPerPatch == 0
			? ResolvePatchCount(BuildingCapacity)
			: FMath::DivideAndRoundUp(BuildingCapacity, PlotCellsPerPatch));
	}

	static bool IsValidPresetSlotSet(FName PresetId, bool bForAuthoredPlots,
		int32 ExpectedSlotCount, TConstArrayView<FName> SlotIds)
	{
		if (PresetId.IsNone() || ExpectedSlotCount <= 0
			|| SlotIds.Num() != ExpectedSlotCount)
		{
			return false;
		}
		TSet<FName> UniqueSlotIds;
		for (const FName SlotId : SlotIds)
		{
			if (SlotId.IsNone() || UniqueSlotIds.Contains(SlotId))
			{
				return false;
			}
			UniqueSlotIds.Add(SlotId);
		}
		return bForAuthoredPlots || IsCompletePresetSlotSet(PresetId, SlotIds);
	}

	static bool IsCompletePresetSlotSet(
		FName PresetId,
		TConstArrayView<FName> SlotIds)
	{
		if (PresetId != FName(TEXT("CircularTreeLounge")) || SlotIds.Num() != 3)
		{
			return false;
		}

		bool bHasHardscape = false;
		bool bHasTree = false;
		bool bHasCommuter = false;
		for (const FName SlotId : SlotIds)
		{
			if (SlotId == FName(TEXT("Hardscape")))
			{
				if (bHasHardscape)
				{
					return false;
				}
				bHasHardscape = true;
			}
			else if (SlotId == FName(TEXT("Tree")))
			{
				if (bHasTree)
				{
					return false;
				}
				bHasTree = true;
			}
			else if (SlotId == FName(TEXT("Commuter")))
			{
				if (bHasCommuter)
				{
					return false;
				}
				bHasCommuter = true;
			}
			else
			{
				return false;
			}
		}
		return bHasHardscape && bHasTree && bHasCommuter;
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
