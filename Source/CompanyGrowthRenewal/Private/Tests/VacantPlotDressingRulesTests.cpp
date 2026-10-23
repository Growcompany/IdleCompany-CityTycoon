#include "Misc/AutomationTest.h"
#include "Entity/Ambient/VacantPlotDressingRules.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRVacantPlotDressingGeometryAndSelectionRulesTest,
	"CGR.CityDressing.Rules.GeometryAndSelection",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRVacantPlotDressingGeometryAndSelectionRulesTest::RunTest(const FString& Parameters)
{
	const FVacantPlotFootprint Patch(FVector2D::ZeroVector, FVector2D(100.f, 50.f));

	TestFalse(TEXT("Rectangles that only touch at an edge remain independently visible"),
		FVacantPlotDressingRules::Overlaps(
			Patch,
			FVacantPlotFootprint(FVector2D(200.f, 0.f), FVector2D(100.f, 50.f)),
			/*SafetyPadding=*/0.f));

	TestTrue(TEXT("The 75 cm visual safety margin hides a near building patch"),
		FVacantPlotDressingRules::Overlaps(
			Patch,
			FVacantPlotFootprint(FVector2D(250.f, 0.f), FVector2D(100.f, 50.f)),
			/*SafetyPadding=*/75.f));

	const TArray<FVacantPlotFootprint> Occupancies = {
		FVacantPlotFootprint(FVector2D(800.f, 800.f), FVector2D(50.f, 50.f)),
		FVacantPlotFootprint(FVector2D(250.f, 0.f), FVector2D(100.f, 50.f))
	};
	TestTrue(TEXT("Any overlapping building or preview footprint hides the patch"),
		FVacantPlotDressingRules::OverlapsAny(Patch, Occupancies, /*SafetyPadding=*/75.f));
	TestFalse(TEXT("An empty occupancy list leaves the patch visible"),
		FVacantPlotDressingRules::OverlapsAny(Patch, TConstArrayView<FVacantPlotFootprint>(), 75.f));

	TestTrue(TEXT("A patch fully inside both plot axes is accepted"),
		FVacantPlotDressingRules::IsInsidePlot(
			FVacantPlotFootprint(FVector2D(550.f, -250.f), FVector2D(100.f, 50.f)),
			FVector2D::ZeroVector,
			FVector2D(700.f, 400.f)));

	TestFalse(TEXT("A patch crossing either plot edge is rejected fail-closed"),
		FVacantPlotDressingRules::IsInsidePlot(
			FVacantPlotFootprint(FVector2D(650.f, -250.f), FVector2D(100.f, 50.f)),
			FVector2D::ZeroVector,
			FVector2D(700.f, 400.f)));

	const uint32 Plot01Hash = FVacantPlotDressingRules::StableHash(TEXT("Plot_01"));
	TestEqual(TEXT("The same PlotId produces the same stable seed"),
		FVacantPlotDressingRules::StableHash(TEXT("Plot_01")), Plot01Hash);
	TestNotEqual(TEXT("Different PlotIds do not collapse to one constant seed"),
		FVacantPlotDressingRules::StableHash(TEXT("Plot_02")), Plot01Hash);

	const int32 FirstSelection = FVacantPlotDressingRules::SelectPresetIndex(TEXT("Plot_01"), 2, 3);
	TestEqual(TEXT("Preset selection is repeatable for a PlotId and patch index"),
		FVacantPlotDressingRules::SelectPresetIndex(TEXT("Plot_01"), 2, 3), FirstSelection);
	TestEqual(TEXT("An empty preset list fails closed"),
		FVacantPlotDressingRules::SelectPresetIndex(TEXT("Plot_01"), 2, 0), INDEX_NONE);

	TestEqual(TEXT("A quarter turn swaps a non-square half extent"),
		FVacantPlotDressingRules::RotateHalfExtent90(FVector2D(120.f, 40.f), 1),
		FVector2D(40.f, 120.f));
	TestEqual(TEXT("A half turn preserves a non-square half extent"),
		FVacantPlotDressingRules::RotateHalfExtent90(FVector2D(120.f, 40.f), 2),
		FVector2D(120.f, 40.f));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRVacantPlotDressingPatchCountRulesTest,
	"CGR.CityDressing.Rules.PatchCountPolicy",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRVacantPlotDressingPatchCountRulesTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Negative BuildingCapacity produces no dressing anchor"),
		FVacantPlotDressingRules::ResolvePatchCount(/*BuildingCapacity=*/-1), 0);
	TestEqual(TEXT("BuildingCapacity 0 produces no dressing anchor"),
		FVacantPlotDressingRules::ResolvePatchCount(/*BuildingCapacity=*/0), 0);
	TestEqual(TEXT("BuildingCapacity 6 produces exactly one dressing anchor"),
		FVacantPlotDressingRules::ResolvePatchCount(/*BuildingCapacity=*/6), 1);
	TestEqual(TEXT("BuildingCapacity 9 produces exactly one dressing anchor"),
		FVacantPlotDressingRules::ResolvePatchCount(/*BuildingCapacity=*/9), 1);
	TestEqual(TEXT("BuildingCapacity 12 produces exactly one dressing anchor"),
		FVacantPlotDressingRules::ResolvePatchCount(/*BuildingCapacity=*/12), 1);
	TestEqual(TEXT("BuildingCapacity 47 stays below the mega-plot threshold"),
		FVacantPlotDressingRules::ResolvePatchCount(/*BuildingCapacity=*/47), 1);
	TestEqual(TEXT("BuildingCapacity 48 produces exactly two dressing anchors"),
		FVacantPlotDressingRules::ResolvePatchCount(/*BuildingCapacity=*/48), 2);
	TestEqual(TEXT("BuildingCapacity 49 stays in the two-anchor mega-plot tier"),
		FVacantPlotDressingRules::ResolvePatchCount(/*BuildingCapacity=*/49), 2);
	TestEqual(TEXT("A full plot with zero capacity never allocates a pending dressing patch"),
		FVacantPlotDressingRules::ResolvePatchCount(0, 2, 12), 0);
	TestEqual(TEXT("Compact profiles allocate one patch per two occupied cells"),
		FVacantPlotDressingRules::ResolvePatchCount(5, 2, 12), 3);
	TestEqual(TEXT("Bad table density cannot exceed the bounded patch workload"),
		FVacantPlotDressingRules::ResolvePatchCount(100, 1, 100), 12);
	TestEqual(TEXT("Zero cells per patch retains the Riverwalk count contract"),
		FVacantPlotDressingRules::ResolvePatchCount(48, 0, 2), 2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRVacantPlotDressingPresetSlotContractTest,
	"CGR.CityDressing.Rules.PresetSlotContract",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRVacantPlotDressingPresetSlotContractTest::RunTest(const FString& Parameters)
{
	const TArray<FName> CompleteSlots = {
		FName(TEXT("Hardscape")),
		FName(TEXT("Tree")),
		FName(TEXT("Commuter"))
	};
	TestTrue(TEXT("CircularTreeLounge accepts its three unique production slots"),
		FVacantPlotDressingRules::IsCompletePresetSlotSet(
			TEXT("CircularTreeLounge"), CompleteSlots));

	const TArray<FName> MissingSlot = {
		FName(TEXT("Hardscape")),
		FName(TEXT("Tree"))
	};
	TestFalse(TEXT("CircularTreeLounge rejects a missing production slot"),
		FVacantPlotDressingRules::IsCompletePresetSlotSet(
			TEXT("CircularTreeLounge"), MissingSlot));

	const TArray<FName> DuplicateSlot = {
		FName(TEXT("Hardscape")),
		FName(TEXT("Tree")),
		FName(TEXT("Tree"))
	};
	TestFalse(TEXT("CircularTreeLounge rejects duplicate slot identifiers"),
		FVacantPlotDressingRules::IsCompletePresetSlotSet(
			TEXT("CircularTreeLounge"), DuplicateSlot));

	const TArray<FName> ExtraSlot = {
		FName(TEXT("Hardscape")),
		FName(TEXT("Tree")),
		FName(TEXT("Commuter")),
		FName(TEXT("Decoration"))
	};
	TestFalse(TEXT("CircularTreeLounge rejects extra slot identifiers"),
		FVacantPlotDressingRules::IsCompletePresetSlotSet(
			TEXT("CircularTreeLounge"), ExtraSlot));

	const TArray<FName> InvalidSlot = {
		FName(TEXT("Hardscape")),
		FName(TEXT("Tree")),
		NAME_None
	};
	TestFalse(TEXT("CircularTreeLounge rejects an invalid slot identifier"),
		FVacantPlotDressingRules::IsCompletePresetSlotSet(
			TEXT("CircularTreeLounge"), InvalidSlot));
	const TArray<FName> CompactSlots = { FName(TEXT("Tree")), FName(TEXT("Bench")) };
	TestTrue(TEXT("An authored profile accepts unique named slots with its declared count"),
		FVacantPlotDressingRules::IsValidPresetSlotSet(
			TEXT("CompactGarden"), true, 2, CompactSlots));
	TestFalse(TEXT("An authored profile rejects a missing row"),
		FVacantPlotDressingRules::IsValidPresetSlotSet(
			TEXT("CompactGarden"), true, 3, CompactSlots));
	TestFalse(TEXT("An authored profile rejects duplicate slots"),
		FVacantPlotDressingRules::IsValidPresetSlotSet(
			TEXT("CompactGarden"), true, 3, DuplicateSlot));
	TestFalse(TEXT("A runtime profile cannot replace the CircularTreeLounge contract"),
		FVacantPlotDressingRules::IsValidPresetSlotSet(
			TEXT("CompactGarden"), false, 2, CompactSlots));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRVacantPlotDressingBlockedFormerAnchorTest,
	"CGR.CityDressing.Rules.BlockedFormerAnchorFindsFreeGap",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRVacantPlotDressingBlockedFormerAnchorTest::RunTest(const FString& Parameters)
{
	const FVector2D PlotCenter = FVector2D::ZeroVector;
	const FVector2D PlotHalfExtent(5000.f, 5000.f);
	const FVector2D PatchHalfExtent(1000.f, 1000.f);
	const FVector2D BlockedFormerAnchor(-3000.f, -3000.f);
	const TArray<FVacantPlotFootprint> Occupancies = {
		FVacantPlotFootprint(BlockedFormerAnchor, FVector2D(1000.f, 1000.f))
	};

	TArray<FVector2D> CandidateCenters;
	FVacantPlotDressingRules::GenerateCandidateCenters(
		PlotCenter,
		PlotHalfExtent,
		PatchHalfExtent,
		Occupancies,
		/*SafetyPadding=*/75.f,
		CandidateCenters);

	TArray<int32> RankedIndices;
	FVacantPlotDressingRules::RankFreeCandidateIndices(
		TEXT("Plot_17"),
		/*PatchOrdinal=*/0,
		PlotCenter,
		PlotHalfExtent,
		PatchHalfExtent,
		Occupancies,
		/*SafetyPadding=*/75.f,
		CandidateCenters,
		RankedIndices);

	if (!TestTrue(TEXT("A blocked former corner still leaves at least one ranked free gap"),
		RankedIndices.Num() > 0)
		|| !CandidateCenters.IsValidIndex(RankedIndices[0]))
	{
		return false;
	}

	const FVector2D SelectedCenter = CandidateCenters[RankedIndices[0]];
	TestFalse(TEXT("The blocked former corner is not reused"),
		SelectedCenter.Equals(BlockedFormerAnchor, 0.1f));
	TestTrue(TEXT("The fallback candidate remains inside the literal Plot bounds"),
		FMath::Abs(SelectedCenter.X) + PatchHalfExtent.X <= 5000.f
		&& FMath::Abs(SelectedCenter.Y) + PatchHalfExtent.Y <= 5000.f);
	TestTrue(TEXT("The fallback candidate clears the blocked corner plus 75 cm padding"),
		FMath::Abs(SelectedCenter.X - BlockedFormerAnchor.X) >= 2075.f
		|| FMath::Abs(SelectedCenter.Y - BlockedFormerAnchor.Y) >= 2075.f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRVacantPlotDressingOccupancyOrderTest,
	"CGR.CityDressing.Rules.OccupancyOrderDoesNotChangeRanking",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRVacantPlotDressingOccupancyOrderTest::RunTest(const FString& Parameters)
{
	const FVector2D PlotCenter = FVector2D::ZeroVector;
	const FVector2D PlotHalfExtent(5000.f, 5000.f);
	const FVector2D PatchHalfExtent(500.f, 500.f);
	const TArray<FVector2D> CandidateCenters = {
		FVector2D(-3500.f, -3500.f),
		FVector2D(3500.f, -3500.f),
		FVector2D(-3500.f, 3500.f),
		FVector2D(3500.f, 3500.f),
		FVector2D(0.f, 0.f)
	};
	const TArray<FVacantPlotFootprint> ForwardOccupancies = {
		FVacantPlotFootprint(FVector2D(-1000.f, 0.f), FVector2D(600.f, 600.f)),
		FVacantPlotFootprint(FVector2D(1500.f, 500.f), FVector2D(500.f, 500.f))
	};
	const TArray<FVacantPlotFootprint> ReverseOccupancies = {
		FVacantPlotFootprint(FVector2D(1500.f, 500.f), FVector2D(500.f, 500.f)),
		FVacantPlotFootprint(FVector2D(-1000.f, 0.f), FVector2D(600.f, 600.f))
	};

	TArray<int32> ForwardRanking;
	FVacantPlotDressingRules::RankFreeCandidateIndices(
		TEXT("MegaPlot_01"),
		/*PatchOrdinal=*/0,
		PlotCenter,
		PlotHalfExtent,
		PatchHalfExtent,
		ForwardOccupancies,
		/*SafetyPadding=*/75.f,
		CandidateCenters,
		ForwardRanking);

	TArray<int32> ReverseRanking;
	FVacantPlotDressingRules::RankFreeCandidateIndices(
		TEXT("MegaPlot_01"),
		/*PatchOrdinal=*/0,
		PlotCenter,
		PlotHalfExtent,
		PatchHalfExtent,
		ReverseOccupancies,
		/*SafetyPadding=*/75.f,
		CandidateCenters,
		ReverseRanking);

	TestTrue(TEXT("The fixture contains free candidates to rank"), ForwardRanking.Num() > 0);
	if (!TestEqual(TEXT("Reordering occupancies preserves the ranked candidate count"),
		ReverseRanking.Num(), ForwardRanking.Num()))
	{
		return false;
	}
	for (int32 RankIndex = 0; RankIndex < ForwardRanking.Num(); ++RankIndex)
	{
		TestEqual(
			FString::Printf(TEXT("Occupancy order preserves ranked index %d"), RankIndex),
			ReverseRanking[RankIndex],
			ForwardRanking[RankIndex]);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRVacantPlotDressingCandidateValidityTest,
	"CGR.CityDressing.Rules.InvalidAndOverlappingCandidatesAreRejected",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRVacantPlotDressingCandidateValidityTest::RunTest(const FString& Parameters)
{
	const TArray<FVector2D> CandidateCenters = {
		FVector2D(6000.f, 0.f),
		FVector2D(0.f, 0.f),
		FVector2D(3000.f, 3000.f),
		FVector2D(-3000.f, -3000.f)
	};
	const TArray<FVacantPlotFootprint> Occupancies = {
		FVacantPlotFootprint(FVector2D::ZeroVector, FVector2D(600.f, 600.f))
	};
	TArray<int32> RankedIndices;

	FVacantPlotDressingRules::RankFreeCandidateIndices(
		TEXT("Plot_08"),
		/*PatchOrdinal=*/0,
		FVector2D::ZeroVector,
		FVector2D(5000.f, 5000.f),
		FVector2D(1000.f, 1000.f),
		Occupancies,
		/*SafetyPadding=*/75.f,
		CandidateCenters,
		RankedIndices);

	TestEqual(TEXT("Only the two literal in-bounds free candidates survive"),
		RankedIndices.Num(), 2);
	TestFalse(TEXT("The Plot-edge crossing candidate is rejected"), RankedIndices.Contains(0));
	TestFalse(TEXT("The occupancy-overlapping candidate is rejected"), RankedIndices.Contains(1));
	TestTrue(TEXT("The positive-quadrant free candidate is retained"), RankedIndices.Contains(2));
	TestTrue(TEXT("The negative-quadrant free candidate is retained"), RankedIndices.Contains(3));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRVacantPlotDressingMegaPlotReservationTest,
	"CGR.CityDressing.Rules.MegaPlotReservationsDoNotOverlap",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRVacantPlotDressingMegaPlotReservationTest::RunTest(const FString& Parameters)
{
	const FVector2D PlotCenter = FVector2D::ZeroVector;
	const FVector2D PlotHalfExtent(6000.f, 6000.f);
	const FVector2D PatchHalfExtent(1000.f, 1000.f);
	const TArray<FVector2D> CandidateCenters = {
		FVector2D(-3500.f, -3500.f),
		FVector2D(3500.f, -3500.f),
		FVector2D(-3500.f, 3500.f),
		FVector2D(3500.f, 3500.f),
		FVector2D::ZeroVector
	};

	TArray<int32> FirstRanking;
	FVacantPlotDressingRules::RankFreeCandidateIndices(
		TEXT("MegaPlot_01"),
		/*PatchOrdinal=*/0,
		PlotCenter,
		PlotHalfExtent,
		PatchHalfExtent,
		TConstArrayView<FVacantPlotFootprint>(),
		/*SafetyPadding=*/75.f,
		CandidateCenters,
		FirstRanking);
	if (!TestTrue(TEXT("The first MegaPlot patch has a free candidate"), FirstRanking.Num() > 0)
		|| !CandidateCenters.IsValidIndex(FirstRanking[0]))
	{
		return false;
	}

	const FVector2D FirstCenter = CandidateCenters[FirstRanking[0]];
	const TArray<FVacantPlotFootprint> ReservedFootprints = {
		FVacantPlotFootprint(FirstCenter, PatchHalfExtent)
	};
	TArray<int32> SecondRanking;
	FVacantPlotDressingRules::RankFreeCandidateIndices(
		TEXT("MegaPlot_01"),
		/*PatchOrdinal=*/1,
		PlotCenter,
		PlotHalfExtent,
		PatchHalfExtent,
		ReservedFootprints,
		/*SafetyPadding=*/75.f,
		CandidateCenters,
		SecondRanking);
	if (!TestTrue(TEXT("The reserved first patch leaves a second MegaPlot candidate"),
		SecondRanking.Num() > 0)
		|| !CandidateCenters.IsValidIndex(SecondRanking[0]))
	{
		return false;
	}

	const FVector2D SecondCenter = CandidateCenters[SecondRanking[0]];
	TestTrue(TEXT("The two selected 20 m footprints clear each other plus 75 cm padding"),
		FMath::Abs(SecondCenter.X - FirstCenter.X) >= 2075.f
		|| FMath::Abs(SecondCenter.Y - FirstCenter.Y) >= 2075.f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRVacantPlotDressingCandidateWorkBoundTest,
	"CGR.CityDressing.Rules.RankedCandidateWorkIsBounded",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRVacantPlotDressingBoundaryRowsTest,
	"CGR.CityDressing.Rules.BoundaryRowsFollowActualLWithoutInventedDiagonal",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRVacantPlotDressingCandidateWorkBoundTest::RunTest(const FString& Parameters)
{
	TArray<FVector2D> CandidateCenters;
	CandidateCenters.Reserve(700);
	for (int32 RowIndex = 0; RowIndex < 28; ++RowIndex)
	{
		for (int32 ColumnIndex = 0; ColumnIndex < 25; ++ColumnIndex)
		{
			CandidateCenters.Emplace(
				-4800.f + static_cast<float>(ColumnIndex) * 400.f,
				-5400.f + static_cast<float>(RowIndex) * 400.f);
		}
	}

	TArray<int32> RankedIndices;
	FVacantPlotDressingRules::RankFreeCandidateIndices(
		TEXT("MegaPlot_01"),
		/*PatchOrdinal=*/1,
		FVector2D::ZeroVector,
		FVector2D(10000.f, 10000.f),
		FVector2D(50.f, 50.f),
		TConstArrayView<FVacantPlotFootprint>(),
		/*SafetyPadding=*/75.f,
		CandidateCenters,
		RankedIndices);

	TestEqual(TEXT("All valid input still stops at the 512-candidate mobile work bound"),
		RankedIndices.Num(), 512);

	return true;
}

bool FCGRVacantPlotDressingBoundaryRowsTest::RunTest(const FString& Parameters)
{
	const TArray<FCityPlotBoundarySegment> StraightRun = {
		{ FVector2D(0.f, 0.f), FVector2D(6000.f, 0.f), FVector2D(0.f, 1.f) }
	};
	const TArray<FVacantPlotBoundaryPresetParams> MixedTrees = {
		{ FVector2D(250.f, 250.f), 1800.f, 100.f, 96 },
		{ FVector2D(750.f, 650.f), 2400.f, 200.f, 80 },
		{ FVector2D(400.f, 500.f), 2100.f, 150.f, 90 }
	};
	const FVacantPlotBoundaryPresetParams SharedTrees =
		FVacantPlotDressingRules::CombineBoundaryPresets(MixedTrees);
	TArray<FVector2D> MixedCenters;
	FVacantPlotDressingRules::GenerateBoundaryCenters(
		StraightRun, SharedTrees.HalfExtent, SharedTrees.Spacing,
		SharedTrees.Inset, SharedTrees.MaxPatches, MixedCenters);
	if (!TestEqual(TEXT("Mixed tree species keep safe boundary anchors"), MixedCenters.Num(), 2))
	{
		return false;
	}
	TestTrue(TEXT("Largest canopy and inset determine the common inward row"),
		FMath::IsNearlyEqual(MixedCenters[0].Y, 850.f, 0.1f));
	TestTrue(TEXT("Largest requested spacing separates mixed species"),
		FMath::IsNearlyEqual(MixedCenters[1].X - MixedCenters[0].X, 2400.f, 0.1f));
	for (int32 PresetIndex = 0; PresetIndex < MixedTrees.Num(); ++PresetIndex)
	{
		const FVector2D& HalfExtent = MixedTrees[PresetIndex].HalfExtent;
		for (const FVector2D& Center : MixedCenters)
		{
			TestTrue(TEXT("Every species' full canopy fits within the straight edge"),
				Center.X - HalfExtent.X >= 0.f && Center.X + HalfExtent.X <= 6000.f
				&& Center.Y - HalfExtent.Y >= MixedTrees[PresetIndex].Inset);
		}
	}
	TSet<int32> SelectedSpecies;
	for (int32 PatchOrdinal = 0; PatchOrdinal < 64; ++PatchOrdinal)
	{
		SelectedSpecies.Add(FVacantPlotDressingRules::SelectPresetIndex(
			TEXT("Plot_01"), PatchOrdinal, MixedTrees.Num()));
	}
	TestEqual(TEXT("Deterministic anchor selection reaches every eligible species"),
		SelectedSpecies.Num(), MixedTrees.Num());
	TestEqual(TEXT("Mixed species use the strictest authored count cap"),
		SharedTrees.MaxPatches, 80);
	TArray<FVector2D> DenseRun;
	FVacantPlotDressingRules::GenerateBoundaryCenters(
		StraightRun, FVector2D(500.f, 500.f), 750.f, 150.f, 96, DenseRun);
	if (!TestEqual(TEXT("A 10m canopy still permits 7 anchors on a 60m run at 7.5m spacing"),
		DenseRun.Num(), 7))
	{
		return false;
	}
	for (int32 Index = 1; Index < DenseRun.Num(); ++Index)
	{
		TestTrue(TEXT("Adjacent centers follow the requested 750cm spacing despite canopy overlap"),
			FMath::IsNearlyEqual(DenseRun[Index].X - DenseRun[Index - 1].X, 750.f, 0.1f));
	}

	const TArray<FCityPlotBoundarySegment> Boundary = {
		{ FVector2D(0.f, 0.f), FVector2D(6000.f, 0.f), FVector2D(0.f, 1.f) },
		{ FVector2D(6000.f, 0.f), FVector2D(6000.f, 3000.f), FVector2D(-1.f, 0.f) },
		{ FVector2D(6000.f, 3000.f), FVector2D(3000.f, 3000.f), FVector2D(0.f, -1.f) },
		{ FVector2D(3000.f, 3000.f), FVector2D(3000.f, 6000.f), FVector2D(-1.f, 0.f) },
		{ FVector2D(3000.f, 6000.f), FVector2D(0.f, 6000.f), FVector2D(0.f, -1.f) },
		{ FVector2D(0.f, 6000.f), FVector2D(0.f, 0.f), FVector2D(1.f, 0.f) }
	};
	TArray<FCityPlotBoundarySegment> WideBoundary = Boundary;
	for (FCityPlotBoundarySegment& Segment : WideBoundary)
	{
		Segment.Start *= 2.f;
		Segment.End *= 2.f;
	}
	TArray<FVector2D> MixedLCenters;
	FVacantPlotDressingRules::GenerateBoundaryCenters(
		WideBoundary, SharedTrees.HalfExtent, SharedTrees.Spacing,
		SharedTrees.Inset, SharedTrees.MaxPatches, MixedLCenters);
	TestTrue(TEXT("Mixed species retain anchors around the wide L"),
		MixedLCenters.Num() >= 6);
	for (const FVector2D& Center : MixedLCenters)
	{
		for (const FVacantPlotBoundaryPresetParams& Species : MixedTrees)
		{
			const FVector2D Minimum = Center - Species.HalfExtent;
			const FVector2D Maximum = Center + Species.HalfExtent;
			TestTrue(TEXT("Every selected species' full canopy stays in the actual L"),
				Minimum.X >= 0.f && Minimum.Y >= 0.f
				&& Maximum.X <= 12000.f && Maximum.Y <= 12000.f
				&& (Maximum.X <= 6000.f || Maximum.Y <= 6000.f));
		}
	}
	TArray<FVector2D> First;
	TArray<FVector2D> Second;
	FVacantPlotDressingRules::GenerateBoundaryCenters(
		Boundary, FVector2D(250.f, 250.f), 1200.f, 75.f, 96, First);
	FVacantPlotDressingRules::GenerateBoundaryCenters(
		Boundary, FVector2D(250.f, 250.f), 1200.f, 75.f, 96, Second);
	TestTrue(TEXT("The actual L perimeter produces a row of tree anchors"), First.Num() > 6);
	TestEqual(TEXT("Repeated generation preserves anchor count"), Second.Num(), First.Num());
	for (int32 Index = 0; Index < First.Num(); ++Index)
	{
		TestEqual(TEXT("Repeated generation preserves each anchor XY"), Second[Index], First[Index]);
		TestFalse(TEXT("No tree appears in the missing L quadrant"),
			First[Index].X > 3000.f && First[Index].Y > 3000.f);
		bool bOnInwardBoundaryRow = false;
		for (const FCityPlotBoundarySegment& Segment : Boundary)
		{
			const FVector2D InnerStart = Segment.Start + Segment.InwardNormal * 325.f;
			const FVector2D InnerEnd = Segment.End + Segment.InwardNormal * 325.f;
			const FVector2D Along = InnerEnd - InnerStart;
			const FVector2D ToTree = First[Index] - InnerStart;
			bOnInwardBoundaryRow |= FMath::Abs(Along.X * ToTree.Y - Along.Y * ToTree.X) < 1.f
				&& FVector2D::DotProduct(ToTree, Along) >= 0.f
				&& FVector2D::DotProduct(ToTree, Along) <= Along.SizeSquared();
		}
		TestTrue(TEXT("Each anchor belongs to an actual straight boundary run"), bOnInwardBoundaryRow);
	}
	TArray<FCityPlotBoundarySegment> HugeBoundary = Boundary;
	for (FCityPlotBoundarySegment& Segment : HugeBoundary)
	{
		Segment.Start *= 20.f;
		Segment.End *= 20.f;
	}
	TArray<FVector2D> Capped;
	FVacantPlotDressingRules::GenerateBoundaryCenters(
		HugeBoundary, FVector2D(250.f, 250.f), 1200.f, 75.f, 96, Capped);
	TestTrue(TEXT("Long perimeters retain all sides within the mobile tree budget"),
		Capped.Num() <= 96 && Capped.Num() >= 6);
	for (const FCityPlotBoundarySegment& Segment : HugeBoundary)
	{
		const float InwardOffset = 325.f;
		const FVector2D InnerStart = Segment.Start + Segment.InwardNormal * InwardOffset;
		const FVector2D Along = Segment.End - Segment.Start;
		const bool bSideCovered = Capped.ContainsByPredicate([&](const FVector2D& Center)
		{
			const FVector2D Relative = Center - InnerStart;
			return FMath::Abs(Along.X * Relative.Y - Along.Y * Relative.X) < 1.f
				&& FVector2D::DotProduct(Relative, Along) >= 0.f
				&& FVector2D::DotProduct(Relative, Along) <= Along.SizeSquared();
		});
		TestTrue(TEXT("The count cap retains every actual side"), bSideCovered);
	}
	TArray<FVector2D> DenseL;
	TArray<FVector2D> RepeatedDenseL;
	FVacantPlotDressingRules::GenerateBoundaryCenters(
		Boundary, FVector2D(500.f, 500.f), 750.f, 150.f, 96, DenseL);
	FVacantPlotDressingRules::GenerateBoundaryCenters(
		Boundary, FVector2D(500.f, 500.f), 750.f, 150.f, 96, RepeatedDenseL);
	if (!TestEqual(TEXT("Dense L rows preserve candidate count"), RepeatedDenseL.Num(), DenseL.Num()))
	{
		return false;
	}
	for (int32 FirstIndex = 0; FirstIndex < DenseL.Num(); ++FirstIndex)
	{
		TestEqual(TEXT("Dense L rows preserve each anchor XY"),
			RepeatedDenseL[FirstIndex], DenseL[FirstIndex]);
		TestFalse(TEXT("Dense L rows do not enter the cutout"),
			DenseL[FirstIndex].X > 3000.f && DenseL[FirstIndex].Y > 3000.f);
		for (int32 SecondIndex = FirstIndex + 1; SecondIndex < DenseL.Num(); ++SecondIndex)
		{
			TestTrue(TEXT("Adjacent sides never create coincident or crowded trunks"),
				FVector2D::DistSquared(DenseL[FirstIndex], DenseL[SecondIndex]) >= FMath::Square(749.9f));
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
