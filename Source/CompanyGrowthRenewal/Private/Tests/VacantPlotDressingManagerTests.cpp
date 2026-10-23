#include "Misc/AutomationTest.h"
#include "Entity/Ambient/VacantPlotDressingManager.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRVacantPlotDressingManagerTest,
	"CGR.CityDressing.ManagerVisibility",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRVacantPlotDressingNavigationDiagnosticTest,
	"CGR.CityDressing.NavigationDiagnostic",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRVacantPlotDressingCurrentPlacementRetentionTest,
	"CGR.CityDressing.Manager.CurrentValidPlacementIsRetained",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRVacantPlotDressingNoCandidateStateTest,
	"CGR.CityDressing.Manager.NoCandidateRemainsHidden",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRVacantPlotDressingCommittedRelocationGateTest,
	"CGR.CityDressing.Manager.RelocationQueuesOnlyForCommittedRefresh",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRVacantPlotDressingPreviewImmutabilityTest,
	"CGR.CityDressing.Manager.PreviewNeverChangesPatchXY",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRVacantPlotDressingAtomicRelocationTest,
	"CGR.CityDressing.Manager.RelocationMovesEveryInstanceRefAtomically",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRVacantPlotDressingSharedPaddingTest,
	"CGR.CityDressing.Manager.HundredCentimeterGapUsesSharedSafetyPadding",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRVacantPlotDressingFixedAnchorRestoreTest,
	"CGR.CityDressing.Manager.FixedAnchorRestoresWithoutMoving",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRVacantPlotDressingManagerTest::RunTest(const FString& Parameters)
{
	const FVacantPlotFootprint Patch(FVector2D(100.f, 100.f), FVector2D(120.f, 60.f));

	TestTrue(TEXT("A patch with no building or preview occupancy remains visible"),
		AVacantPlotDressingManager::ShouldPatchBeVisible(
			Patch, TConstArrayView<FVacantPlotFootprint>(), 75.f));

	const TArray<FVacantPlotFootprint> FarOccupancy = {
		FVacantPlotFootprint(FVector2D(1000.f, 1000.f), FVector2D(100.f, 100.f))
	};
	TestTrue(TEXT("A distant building leaves the patch visible"),
		AVacantPlotDressingManager::ShouldPatchBeVisible(Patch, FarOccupancy, 75.f));

	const TArray<FVacantPlotFootprint> PreviewOverlap = {
		FVacantPlotFootprint(FVector2D(330.f, 100.f), FVector2D(40.f, 40.f))
	};
	TestFalse(TEXT("The active preview hides only its intersecting patch"),
		AVacantPlotDressingManager::ShouldPatchBeVisible(Patch, PreviewOverlap, 75.f));

	return true;
}

bool FCGRVacantPlotDressingNavigationDiagnosticTest::RunTest(const FString& Parameters)
{
	const AVacantPlotDressingManager* Manager = GetDefault<AVacantPlotDressingManager>();
	if (!TestNotNull(TEXT("vacant plot dressing manager CDO"), Manager))
	{
		return false;
	}

	TestFalse(
		TEXT("an uninitialized manager fails closed when it has no dressing components"),
		Manager->AreDressingComponentsNavigationDisabled());

	const UFunction* DiagnosticFunction = AVacantPlotDressingManager::StaticClass()->FindFunctionByName(
		GET_FUNCTION_NAME_CHECKED(
			AVacantPlotDressingManager,
			AreDressingComponentsNavigationDisabled));
	if (!TestNotNull(TEXT("navigation diagnostic is reflected"), DiagnosticFunction))
	{
		return false;
	}
	TestTrue(
		TEXT("navigation diagnostic is exposed as a BlueprintPure query"),
		DiagnosticFunction->HasAnyFunctionFlags(FUNC_BlueprintPure));

	return true;
}

bool FCGRVacantPlotDressingCurrentPlacementRetentionTest::RunTest(const FString& Parameters)
{
	const FVacantPlotFootprint CurrentPlacement(
		FVector2D(1200.f, -800.f), FVector2D(900.f, 700.f));
	const TArray<FVacantPlotFootprint> RankedCandidates = {
		FVacantPlotFootprint(FVector2D(-2500.f, 2200.f), FVector2D(900.f, 700.f))
	};

	const FVacantDressingPlacementDecision Decision =
		FVacantPlotDressingManagerStateRules::ResolvePlacement(
			CurrentPlacement,
			/*bCurrentPlacementValid=*/true,
			EVacantPlotDressingRefreshMode::Committed,
			RankedCandidates);

	TestEqual(TEXT("A valid current placement keeps its exact XY"),
		Decision.ResolvedFootprint.Center, FVector2D(1200.f, -800.f));
	TestEqual(TEXT("A valid current placement keeps its exact footprint extent"),
		Decision.ResolvedFootprint.HalfExtent, FVector2D(900.f, 700.f));
	TestTrue(TEXT("A valid current placement remains visible"), Decision.bTargetVisible);
	TestFalse(TEXT("A valid current placement never queues a higher-ranked alternative"),
		Decision.bQueueRelocation);

	return true;
}

bool FCGRVacantPlotDressingNoCandidateStateTest::RunTest(const FString& Parameters)
{
	const FVacantPlotFootprint CurrentPlacement(
		FVector2D(-900.f, 1400.f), FVector2D(800.f, 800.f));
	const FVacantDressingPlacementDecision Decision =
		FVacantPlotDressingManagerStateRules::ResolvePlacement(
			CurrentPlacement,
			/*bCurrentPlacementValid=*/false,
			EVacantPlotDressingRefreshMode::Committed,
			TConstArrayView<FVacantPlotFootprint>());

	TestEqual(TEXT("No-candidate state does not invent a replacement XY"),
		Decision.ResolvedFootprint.Center, FVector2D(-900.f, 1400.f));
	TestFalse(TEXT("An invalid placement with no candidate remains hidden"),
		Decision.bTargetVisible);
	TestFalse(TEXT("An invalid placement with no candidate has nothing to relocate"),
		Decision.bQueueRelocation);

	return true;
}

bool FCGRVacantPlotDressingCommittedRelocationGateTest::RunTest(const FString& Parameters)
{
	const FVacantPlotFootprint CurrentPlacement(
		FVector2D(500.f, 500.f), FVector2D(750.f, 750.f));
	const TArray<FVacantPlotFootprint> RankedCandidates = {
		FVacantPlotFootprint(FVector2D(3000.f, -2000.f), FVector2D(750.f, 750.f))
	};

	const FVacantDressingPlacementDecision PreviewDecision =
		FVacantPlotDressingManagerStateRules::ResolvePlacement(
			CurrentPlacement,
			/*bCurrentPlacementValid=*/false,
			EVacantPlotDressingRefreshMode::Preview,
			RankedCandidates);
	const FVacantDressingPlacementDecision CommittedDecision =
		FVacantPlotDressingManagerStateRules::ResolvePlacement(
			CurrentPlacement,
			/*bCurrentPlacementValid=*/false,
			EVacantPlotDressingRefreshMode::Committed,
			RankedCandidates);

	TestFalse(TEXT("Preview overlap never queues relocation"),
		PreviewDecision.bQueueRelocation);
	TestTrue(TEXT("A committed invalidation with a free candidate queues relocation"),
		CommittedDecision.bQueueRelocation);
	TestFalse(TEXT("A queued relocation remains hidden until commit"),
		CommittedDecision.bTargetVisible);
	TestEqual(TEXT("Committed relocation selects the first ranked free footprint"),
		CommittedDecision.ResolvedFootprint.Center, FVector2D(3000.f, -2000.f));

	return true;
}

bool FCGRVacantPlotDressingPreviewImmutabilityTest::RunTest(const FString& Parameters)
{
	const FVacantPlotFootprint CurrentPlacement(
		FVector2D(-1700.f, 600.f), FVector2D(650.f, 850.f));
	const TArray<FVacantPlotFootprint> RankedCandidates = {
		FVacantPlotFootprint(FVector2D(2600.f, 3100.f), FVector2D(650.f, 850.f))
	};

	const FVacantDressingPlacementDecision Decision =
		FVacantPlotDressingManagerStateRules::ResolvePlacement(
			CurrentPlacement,
			/*bCurrentPlacementValid=*/false,
			EVacantPlotDressingRefreshMode::Preview,
			RankedCandidates);

	TestEqual(TEXT("Preview-only refresh preserves the current X coordinate"),
		Decision.ResolvedFootprint.Center.X, -1700.0);
	TestEqual(TEXT("Preview-only refresh preserves the current Y coordinate"),
		Decision.ResolvedFootprint.Center.Y, 600.0);
	TestEqual(TEXT("Preview-only refresh preserves the current footprint extent"),
		Decision.ResolvedFootprint.HalfExtent, FVector2D(650.f, 850.f));
	TestFalse(TEXT("An overlapping preview only hides the current patch"),
		Decision.bTargetVisible);

	return true;
}

bool FCGRVacantPlotDressingAtomicRelocationTest::RunTest(const FString& Parameters)
{
	FVacantDressingPatch Patch;
	Patch.Footprint = FVacantPlotFootprint(
		FVector2D(1000.f, 1000.f), FVector2D(700.f, 900.f));
	TestFalse(TEXT("A startup patch with no grounded space begins unresolved and hidden"),
		Patch.bHasResolvedPlacement);

	FVacantDressingInstanceRef& FirstRef = Patch.Instances.AddDefaulted_GetRef();
	FirstRef.VisibleTransform = FTransform(
		FRotator(0.f, 15.f, 0.f),
		FVector(1100.f, 900.f, 30.f),
		FVector(1.5f, 2.f, 0.75f));
	FirstRef.GroundOffsetZ = 35.f;
	FVacantDressingInstanceRef& SecondRef = Patch.Instances.AddDefaulted_GetRef();
	SecondRef.VisibleTransform = FTransform(
		FRotator(0.f, -35.f, 0.f),
		FVector(800.f, 1300.f, 60.f),
		FVector(0.8f, 1.2f, 1.1f));
	SecondRef.GroundOffsetZ = 65.f;

	FVacantPlotDressingManagerStateRules::ApplyRelocationDelta(
		Patch,
		FVacantPlotFootprint(
			FVector2D(2400.f, -300.f), FVector2D(700.f, 900.f)));

	TestEqual(TEXT("Relocation commits the destination footprint center"),
		Patch.Footprint.Center, FVector2D(2400.f, -300.f));
	TestTrue(TEXT("A later free space resolves the pending patch in place"),
		Patch.bHasResolvedPlacement);
	TestEqual(TEXT("Relocation commits the destination footprint extent"),
		Patch.Footprint.HalfExtent, FVector2D(700.f, 900.f));
	TestEqual(TEXT("The first instance receives the common +1400,-1300 XY delta"),
		Patch.Instances[0].VisibleTransform.GetLocation(), FVector(2500.f, -400.f, 30.f));
	TestEqual(TEXT("The second instance receives the same +1400,-1300 XY delta"),
		Patch.Instances[1].VisibleTransform.GetLocation(), FVector(2200.f, 0.f, 60.f));
	TestTrue(TEXT("Relocation preserves the first instance rotation"),
		Patch.Instances[0].VisibleTransform.GetRotation().Equals(
			FRotator(0.f, 15.f, 0.f).Quaternion()));
	TestTrue(TEXT("Relocation preserves the second instance scale"),
		Patch.Instances[1].VisibleTransform.GetScale3D().Equals(
			FVector(0.8f, 1.2f, 1.1f)));
	TestEqual(TEXT("Relocation retains the authored ground offset for Z resampling"),
		Patch.Instances[0].GroundOffsetZ, 35.f);

	return true;
}

bool FCGRVacantPlotDressingSharedPaddingTest::RunTest(const FString& Parameters)
{
	const FVacantPlotFootprint Patch(
		FVector2D::ZeroVector, FVector2D(100.f, 100.f));
	const TArray<FVacantPlotFootprint> Occupancies = {
		FVacantPlotFootprint(FVector2D(300.f, 0.f), FVector2D(100.f, 100.f))
	};

	TestTrue(TEXT("A literal 100 cm edge gap clears the shared 75 cm safety margin"),
		AVacantPlotDressingManager::ShouldPatchBeVisible(Patch, Occupancies));
	TestFalse(TEXT("The same fixture distinguishes the obsolete 150 cm committed margin"),
		AVacantPlotDressingManager::ShouldPatchBeVisible(
			Patch, Occupancies, /*SafetyPadding=*/150.f));

	return true;
}

bool FCGRVacantPlotDressingFixedAnchorRestoreTest::RunTest(const FString& Parameters)
{
	const FVacantPlotFootprint Anchor(FVector2D(1700.f, -2300.f), FVector2D(300.f, 250.f));
	const FVacantPlotFootprint Building(FVector2D(1650.f, -2200.f), FVector2D(600.f, 600.f));
	const TArray<FVacantPlotFootprint> Blocked = { Building };
	const TArray<FVacantPlotFootprint> NeighborTrees = {
		FVacantPlotFootprint(FVector2D(2450.f, -2300.f), FVector2D(500.f, 500.f))
	};
	TestFalse(TEXT("A neighboring canopy may overlap a fixed tree without hiding it"),
		FVacantPlotDressingRules::IsBlockedByReservedPatch(
			Anchor, true, NeighborTrees, 75.f));
	TestTrue(TEXT("A moving decoration still respects another patch reservation"),
		FVacantPlotDressingRules::IsBlockedByReservedPatch(
			Anchor, false, NeighborTrees, 75.f));
	const bool bAvailableWhileBuilt = AVacantPlotDressingManager::ShouldPatchBeVisible(
		Anchor, Blocked);
	const FVacantDressingPlacementDecision Hidden =
		FVacantPlotDressingManagerStateRules::ResolveFixedAnchor(Anchor, bAvailableWhileBuilt);
	const FVacantDressingPlacementDecision Restored =
		FVacantPlotDressingManagerStateRules::ResolveFixedAnchor(Anchor,
			AVacantPlotDressingManager::ShouldPatchBeVisible(
				Anchor, TConstArrayView<FVacantPlotFootprint>()));
	TestFalse(TEXT("An overlapping building hides the anchored tree"), Hidden.bTargetVisible);
	TestFalse(TEXT("Fixed anchor never schedules relocation"), Hidden.bQueueRelocation);
	TestTrue(TEXT("Removal restores visibility"), Restored.bTargetVisible);
	TestEqual(TEXT("The restored tree keeps its authored XY"),
		Restored.ResolvedFootprint.Center, Anchor.Center);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
