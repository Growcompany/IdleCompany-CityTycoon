#include "Misc/AutomationTest.h"
#include "Player/Components/MovementInputHandler.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROfficeMovementBoundaryCenterTest,
	"CGR.Office.Camera.MovementBoundary.CenterRelativeOffset",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGROfficeMovementBoundaryCenterTest::RunTest(const FString& Parameters)
{
	// Mutation caught: subtracting the world origin instead of the configured Office floor center shifts the pan boundary.
	UMovementInputHandler* MovementHandler = NewObject<UMovementInputHandler>();
	if (!TestNotNull(TEXT("movement input handler"), MovementHandler))
	{
		return false;
	}

	const FVector OfficeFloorCenterWorld(5104.f, -3209.f, 275.f);
	MovementHandler->SetBoundaryCenterWorld(OfficeFloorCenterWorld);

	TestEqual(
		TEXT("boundary offset is planar and relative to the current Office floor center"),
		MovementHandler->CalculatePlanarBoundaryOffset(FVector(8104.f, -2709.f, 925.f)),
		FVector(3000.f, 500.f, 0.f));
	TestEqual(
		TEXT("the opposite side of the same center produces the opposite signed planar offset"),
		MovementHandler->CalculatePlanarBoundaryOffset(FVector(2104.f, -3709.f, -450.f)),
		FVector(-3000.f, -500.f, 0.f));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
