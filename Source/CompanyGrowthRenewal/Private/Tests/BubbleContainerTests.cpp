#include "Misc/AutomationTest.h"
#include "UI/Element/Chat/BubbleContainerWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRBubbleContainerOffscreenAnimationTest,
	"CGR.BubbleContainer.OffscreenAnimation",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRBubbleContainerOffscreenAnimationTest::RunTest(const FString& Parameters)
{
	TestTrue(
		TEXT("Off-screen Disappearing continues"),
		ShouldAdvanceBubbleAnimation(false, EBubbleAnimState::Disappearing));
	TestFalse(
		TEXT("Off-screen Idle remains paused"),
		ShouldAdvanceBubbleAnimation(false, EBubbleAnimState::Idle));
	TestFalse(
		TEXT("Off-screen Appearing remains paused"),
		ShouldAdvanceBubbleAnimation(false, EBubbleAnimState::Appearing));
	TestTrue(
		TEXT("On-screen animation continues"),
		ShouldAdvanceBubbleAnimation(true, EBubbleAnimState::Appearing));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRBubbleContainerMissingSourceTransitionTest,
	"CGR.BubbleContainer.MissingSourceTransition",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRBubbleContainerMissingSourceTransitionTest::RunTest(const FString& Parameters)
{
	EBubbleAnimState AnimState = EBubbleAnimState::Idle;
	EBubbleType PendingType = EBubbleType::NoProject;
	TestTrue(TEXT("Projection failure preserves state"),
		!ApplyMissingBubbleSourcePolicy(false, AnimState, PendingType)
		&& AnimState == EBubbleAnimState::Idle && PendingType == EBubbleType::NoProject);
	TestTrue(TEXT("Missing source starts removal and clears replacement"),
		ApplyMissingBubbleSourcePolicy(true, AnimState, PendingType)
		&& AnimState == EBubbleAnimState::Disappearing && PendingType == EBubbleType::None);

	PendingType = EBubbleType::NoProject;
	TestTrue(TEXT("Active disappearance is not restarted"),
		!ApplyMissingBubbleSourcePolicy(true, AnimState, PendingType)
		&& AnimState == EBubbleAnimState::Disappearing && PendingType == EBubbleType::None);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRBubbleContainerLatestDesiredStateTest,
	"CGR.BubbleContainer.LatestDesiredState",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRBubbleContainerLatestDesiredStateTest::RunTest(const FString& Parameters)
{
	const auto ReappearCurrent = ResolveDisappearingBubbleTransition(
		EBubbleType::VaultFull, EBubbleType::VaultFull);
	TestEqual(
		TEXT("A -> None -> A reappears current type"),
		ReappearCurrent.NextAnimState,
		EBubbleAnimState::Appearing);
	TestEqual(
		TEXT("Reappearing current type clears replacement"),
		ReappearCurrent.PendingType,
		EBubbleType::None);
	TestTrue(
		TEXT("Reappearing current type restarts animation"),
		ReappearCurrent.bResetAnimElapsed);

	const auto ClearReplacement = ResolveDisappearingBubbleTransition(
		EBubbleType::VaultFull, EBubbleType::None);
	TestEqual(
		TEXT("A -> B -> None keeps removal active"),
		ClearReplacement.NextAnimState,
		EBubbleAnimState::Disappearing);
	TestEqual(
		TEXT("A -> B -> None clears pending B"),
		ClearReplacement.PendingType,
		EBubbleType::None);
	TestFalse(
		TEXT("Clearing replacement does not restart disappearance"),
		ClearReplacement.bResetAnimElapsed);

	const auto ReplaceWithLatest = ResolveDisappearingBubbleTransition(
		EBubbleType::VaultFull, EBubbleType::NoProject);
	TestEqual(
		TEXT("A -> B -> C keeps disappearance active"),
		ReplaceWithLatest.NextAnimState,
		EBubbleAnimState::Disappearing);
	TestEqual(
		TEXT("A -> B -> C stores latest C"),
		ReplaceWithLatest.PendingType,
		EBubbleType::NoProject);
	TestFalse(
		TEXT("Replacing pending type does not restart disappearance"),
		ReplaceWithLatest.bResetAnimElapsed);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
