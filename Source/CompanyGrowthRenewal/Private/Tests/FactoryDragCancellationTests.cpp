#include "Misc/AutomationTest.h"
#include "Player/Components/InteractableDragPolicy.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRFactoryDragCancellationPolicyTest,
	"CGR.Input.FactoryDragCancellationPolicy",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRFactoryDragCancellationPolicyTest::RunTest(const FString& Parameters)
{
	constexpr float Threshold = 30.0f;

	TestTrue(TEXT("Active factory hold cancels when drag crosses threshold"),
		CGRInteractableDragPolicy::ShouldCancelFactoryHold(
			false, 31.0f, Threshold, EInputMode::Factory, true));
	TestFalse(TEXT("Distance at threshold is not a drag"),
		CGRInteractableDragPolicy::ShouldCancelFactoryHold(
			false, 30.0f, Threshold, EInputMode::Factory, true));
	TestFalse(TEXT("Normal interaction is not factory hold"),
		CGRInteractableDragPolicy::ShouldCancelFactoryHold(
			false, 31.0f, Threshold, EInputMode::Normal, true));
	TestFalse(TEXT("Non-factory actor is not canceled"),
		CGRInteractableDragPolicy::ShouldCancelFactoryHold(
			false, 31.0f, Threshold, EInputMode::Factory, false));
	TestFalse(TEXT("Already-started drag does not cancel twice"),
		CGRInteractableDragPolicy::ShouldCancelFactoryHold(
			true, 31.0f, Threshold, EInputMode::Factory, true));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRFactoryDragCancellationTransitionTest,
	"CGR.Input.FactoryDragCancellationTransition",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRFactoryDragCancellationTransitionTest::RunTest(const FString& Parameters)
{
	UObject* HitObject = GetTransientPackage();
	int32 CleanupCallCount = 0;
	const auto CountCleanup = [&CleanupCallCount](UObject*)
	{
		++CleanupCallCount;
	};

	TestTrue(TEXT("Eligible cancellation succeeds"),
		CGRInteractableDragPolicy::TryCancelFactoryHold(true, HitObject, true, CountCleanup));
	TestEqual(TEXT("Eligible cancellation invokes cleanup exactly once"), CleanupCallCount, 1);
	TestTrue(TEXT("Eligible cancellation clears the hit pointer"), HitObject == nullptr);

	TestFalse(TEXT("Cleared hit pointer makes a second cancellation a no-op"),
		CGRInteractableDragPolicy::TryCancelFactoryHold(true, HitObject, true, CountCleanup));
	TestEqual(TEXT("Second cancellation does not invoke cleanup twice"), CleanupCallCount, 1);

	UObject* NonInterfaceHitObject = GetTransientPackage();
	UObject* ExpectedNonInterfaceHitObject = NonInterfaceHitObject;
	int32 NonInterfaceCleanupCallCount = 0;
	TestFalse(TEXT("Non-interface hit does not cancel"),
		CGRInteractableDragPolicy::TryCancelFactoryHold(
			true,
			NonInterfaceHitObject,
			false,
			[&NonInterfaceCleanupCallCount](UObject*) { ++NonInterfaceCleanupCallCount; }));
	TestEqual(TEXT("Non-interface hit does not invoke cleanup"), NonInterfaceCleanupCallCount, 0);
	TestTrue(TEXT("Non-interface hit remains tracked"),
		NonInterfaceHitObject == ExpectedNonInterfaceHitObject);

	UObject* InactivePolicyHitObject = GetTransientPackage();
	UObject* ExpectedInactivePolicyHitObject = InactivePolicyHitObject;
	int32 InactivePolicyCleanupCallCount = 0;
	TestFalse(TEXT("False cancellation policy does not cancel"),
		CGRInteractableDragPolicy::TryCancelFactoryHold(
			false,
			InactivePolicyHitObject,
			true,
			[&InactivePolicyCleanupCallCount](UObject*) { ++InactivePolicyCleanupCallCount; }));
	TestEqual(TEXT("False cancellation policy does not invoke cleanup"), InactivePolicyCleanupCallCount, 0);
	TestTrue(TEXT("False cancellation policy leaves the hit tracked"),
		InactivePolicyHitObject == ExpectedInactivePolicyHitObject);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
