#include "Misc/AutomationTest.h"
#include "Player/Components/FactoryTapHoldPolicy.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRFactoryTapHoldPolicyTest,
	"CGR.Input.FactoryTapHoldPolicy",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRFactoryTapHoldPolicyTest::RunTest(const FString& Parameters)
{
	using namespace CGRFactoryTapHoldPolicy;

	TestEqual(TEXT("Hold threshold is 0.2 seconds"), HoldThresholdSeconds, 0.2f);

	TestEqual(TEXT("Pending release closes the panel"),
		ResolveRelease(EPhase::Pending, false), EReleaseAction::ClosePanel);
	TestEqual(TEXT("Holding release stops production"),
		ResolveRelease(EPhase::Holding, false), EReleaseAction::StopProduction);
	TestEqual(TEXT("None release has no action"),
		ResolveRelease(EPhase::None, false), EReleaseAction::None);
	TestEqual(TEXT("Dragging pending release has no action"),
		ResolveRelease(EPhase::Pending, true), EReleaseAction::None);
	TestEqual(TEXT("Dragging holding release has no action"),
		ResolveRelease(EPhase::Holding, true), EReleaseAction::None);
	TestEqual(TEXT("Dragging none release has no action"),
		ResolveRelease(EPhase::None, true), EReleaseAction::None);

	TestEqual(TEXT("Pending drag cancels the pending press"),
		ResolveDrag(EPhase::Pending), EDragAction::CancelPending);
	TestEqual(TEXT("Holding drag stops production"),
		ResolveDrag(EPhase::Holding), EDragAction::StopProduction);
	TestEqual(TEXT("None drag has no action"),
		ResolveDrag(EPhase::None), EDragAction::None);

	TestTrue(TEXT("Pending stationary press with active panel and valid factory starts hold"),
		ShouldStartHold(EPhase::Pending, false, true, true));
	TestFalse(TEXT("Dragging press cannot start hold"),
		ShouldStartHold(EPhase::Pending, true, true, true));
	TestFalse(TEXT("Inactive panel cannot start hold"),
		ShouldStartHold(EPhase::Pending, false, false, true));
	TestFalse(TEXT("Invalid factory cannot start hold"),
		ShouldStartHold(EPhase::Pending, false, true, false));
	TestFalse(TEXT("None phase cannot start hold"),
		ShouldStartHold(EPhase::None, false, true, true));
	TestFalse(TEXT("Holding phase cannot start hold again"),
		ShouldStartHold(EPhase::Holding, false, true, true));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
