#include "Misc/AutomationTest.h"
#include "Data/TutorialMissionAtomicCommitRules.h"
#include "Table/MissionTable.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRTutorialMissionAtomicCommitRulesTest,
	"CGR.Mission.TutorialAtomicCommitPolicy",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRTutorialMissionAtomicCommitRulesTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("The final mission prepares GoalBoard unlock before the completion snapshot"),
		FTutorialMissionAtomicCommitRules::ShouldPrepareGoalBoardUnlock(NAME_None));
	TestFalse(TEXT("An intermediate mission never unlocks GoalBoard in its completion snapshot"),
		FTutorialMissionAtomicCommitRules::ShouldPrepareGoalBoardUnlock(TEXT("M7_CollectRevenue")));
	TestTrue(TEXT("The final mission marks tutorial completion in the same atomic snapshot"),
		FTutorialMissionAtomicCommitRules::ShouldMarkTutorialCompleted(NAME_None));
	TestFalse(TEXT("An intermediate mission cannot mark tutorial completion"),
		FTutorialMissionAtomicCommitRules::ShouldMarkTutorialCompleted(TEXT("M7_CollectRevenue")));
	// M7 now continues into the derived chain: its manual claim grants rewards but must not finish the tutorial
	TestFalse(TEXT("The M7 manual claim (next = M8) does not mark tutorial completion"),
		FTutorialMissionAtomicCommitRules::ShouldMarkTutorialCompleted(TEXT("M8_UpgradeFactory")));
	TestFalse(TEXT("The M7 manual claim (next = M8) does not unlock GoalBoard"),
		FTutorialMissionAtomicCommitRules::ShouldPrepareGoalBoardUnlock(TEXT("M8_UpgradeFactory")));
	TestTrue(TEXT("The M31 final claim (next = None) marks completion and unlocks GoalBoard in one snapshot"),
		FTutorialMissionAtomicCommitRules::ShouldMarkTutorialCompleted(NAME_None)
		&& FTutorialMissionAtomicCommitRules::ShouldPrepareGoalBoardUnlock(NAME_None));

	TArray<FMissionReward> EmptyRewards;
	TestTrue(TEXT("A terminal mission with no rewards can commit"),
		FTutorialMissionAtomicCommitRules::AreRewardsGrantable(
			EmptyRewards,
			/*bResourceManagerAvailable=*/true,
			/*bItemManagerAvailable=*/true));

	FMissionReward MalformedReward;
	MalformedReward.ResourceType = EResourceType::Money;
	MalformedReward.Amount = 0;
	MalformedReward.ItemType = EItemType::RecruitTicketNormal;
	MalformedReward.ItemAmount = 0;
	TestFalse(TEXT("A malformed final reward cannot silently complete the tutorial"),
		FTutorialMissionAtomicCommitRules::AreRewardsGrantable(
			{MalformedReward},
			/*bResourceManagerAvailable=*/true,
			/*bItemManagerAvailable=*/true));

	FMissionReward MoneyReward;
	MoneyReward.ResourceType = EResourceType::Money;
	MoneyReward.Amount = 100;
	TestFalse(TEXT("A valid resource row is not grantable without its manager"),
		FTutorialMissionAtomicCommitRules::AreRewardsGrantable(
			{MoneyReward},
			/*bResourceManagerAvailable=*/false,
			/*bItemManagerAvailable=*/true));
	TestTrue(TEXT("A positive resource reward with its manager is grantable"),
		FTutorialMissionAtomicCommitRules::AreRewardsGrantable(
			{MoneyReward},
			/*bResourceManagerAvailable=*/true,
			/*bItemManagerAvailable=*/false));

	FMissionReward ItemReward;
	ItemReward.ItemType = EItemType::RecruitTicketNormal;
	ItemReward.ItemAmount = 1;
	TestTrue(TEXT("A positive item reward with its manager is grantable"),
		FTutorialMissionAtomicCommitRules::AreRewardsGrantable(
			{ItemReward},
			/*bResourceManagerAvailable=*/false,
			/*bItemManagerAvailable=*/true));
	TestFalse(TEXT("Every configured final reward manager must be available"),
		FTutorialMissionAtomicCommitRules::AreRewardsGrantable(
			{MoneyReward, ItemReward},
			/*bResourceManagerAvailable=*/true,
			/*bItemManagerAvailable=*/false));
	TestTrue(TEXT("All configured final rewards are grantable when every manager is available"),
		FTutorialMissionAtomicCommitRules::AreRewardsGrantable(
			{MoneyReward, ItemReward},
			/*bResourceManagerAvailable=*/true,
			/*bItemManagerAvailable=*/true));

	FMissionReward PartiallyMalformedReward;
	PartiallyMalformedReward.ResourceType = EResourceType::Money;
	PartiallyMalformedReward.Amount = 100;
	PartiallyMalformedReward.ItemType = EItemType::SkinTicketNormal;
	PartiallyMalformedReward.ItemAmount = 0;
	TestFalse(TEXT("A valid component cannot hide a malformed component in the same reward row"),
		FTutorialMissionAtomicCommitRules::AreRewardsGrantable(
			{PartiallyMalformedReward},
			/*bResourceManagerAvailable=*/true,
			/*bItemManagerAvailable=*/true));

	const FTutorialRewardBroadcastDecision TerminalPrepareBroadcast =
		FTutorialMissionAtomicCommitRules::ResolveRewardBroadcastDecision(
			/*bIsTerminalMission=*/true,
			/*bSaveSucceeded=*/false);
	TestFalse(TEXT("Terminal reward prepare never broadcasts before durable save"),
		TerminalPrepareBroadcast.bBroadcastDuringPrepare);
	TestFalse(TEXT("Terminal rollback never broadcasts transient restored values"),
		TerminalPrepareBroadcast.bBroadcastDuringRollback);
	TestFalse(TEXT("A failed terminal save publishes no reward changes"),
		TerminalPrepareBroadcast.bPublishRewardsAfterCommit);
	const FTutorialRewardBroadcastDecision ManualM7Prepare =
		FTutorialMissionAtomicCommitRules::ResolveRewardBroadcastDecision(true, false);
	TestFalse(TEXT("M7 reward stays private until its intermediate claim save succeeds"),
		ManualM7Prepare.bBroadcastDuringPrepare);

	const FTutorialRewardBroadcastDecision TerminalCommitBroadcast =
		FTutorialMissionAtomicCommitRules::ResolveRewardBroadcastDecision(
			/*bIsTerminalMission=*/true,
			/*bSaveSucceeded=*/true);
	TestFalse(TEXT("Successful terminal prepare is still silent"),
		TerminalCommitBroadcast.bBroadcastDuringPrepare);
	TestTrue(TEXT("Successful terminal save publishes each committed reward"),
		TerminalCommitBroadcast.bPublishRewardsAfterCommit);

	const FTutorialRewardBroadcastDecision IntermediateBroadcast =
		FTutorialMissionAtomicCommitRules::ResolveRewardBroadcastDecision(
			/*bIsTerminalMission=*/false,
			/*bSaveSucceeded=*/false);
	TestTrue(TEXT("Intermediate mission rewards preserve their existing immediate broadcasts"),
		IntermediateBroadcast.bBroadcastDuringPrepare);
	TestFalse(TEXT("Intermediate rewards are not published a second time after save"),
		IntermediateBroadcast.bPublishRewardsAfterCommit);

	const FTutorialMissionSaveDecision FailedSaveDecision =
		FTutorialMissionAtomicCommitRules::ResolveSaveDecision(
			/*bIsTerminalMission=*/true,
			/*bSaveSucceeded=*/false);
	TestFalse(TEXT("A failed save never commits prepared mutations"), FailedSaveDecision.bCommitPreparedState);
	TestTrue(TEXT("A failed save restores the retryable Ready state"), FailedSaveDecision.bRestoreReadyToClaim);
	TestFalse(TEXT("A failed save never publishes completion or GoalBoard unlock"), FailedSaveDecision.bPublishCompletionEvents);
	const FTutorialMissionSaveDecision FailedM7Claim =
		FTutorialMissionAtomicCommitRules::ResolveSaveDecision(true, false);
	TestFalse(TEXT("Failed M7 claim cannot consume the reward or finish the tutorial"), FailedM7Claim.bCommitPreparedState);
	TestTrue(TEXT("Failed M7 claim remains retryable"), FailedM7Claim.bRestoreReadyToClaim);

	const FTutorialMissionSaveDecision FailedIntermediateSaveDecision =
		FTutorialMissionAtomicCommitRules::ResolveSaveDecision(
			/*bIsTerminalMission=*/false,
			/*bSaveSucceeded=*/false);
	TestTrue(TEXT("An intermediate mission keeps its in-memory advance when save fails"),
		FailedIntermediateSaveDecision.bCommitPreparedState);
	TestFalse(TEXT("An intermediate mission has no Ready state to restore"),
		FailedIntermediateSaveDecision.bRestoreReadyToClaim);
	TestTrue(TEXT("An intermediate mission still publishes its in-memory transition"),
		FailedIntermediateSaveDecision.bPublishCompletionEvents);

	const FTutorialMissionSaveDecision SuccessfulSaveDecision =
		FTutorialMissionAtomicCommitRules::ResolveSaveDecision(
			/*bIsTerminalMission=*/true,
			/*bSaveSucceeded=*/true);
	TestTrue(TEXT("A successful save commits prepared mutations"), SuccessfulSaveDecision.bCommitPreparedState);
	TestFalse(TEXT("A successful save clears the Ready state"), SuccessfulSaveDecision.bRestoreReadyToClaim);
	TestTrue(TEXT("A successful save publishes completion and GoalBoard unlock"), SuccessfulSaveDecision.bPublishCompletionEvents);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
