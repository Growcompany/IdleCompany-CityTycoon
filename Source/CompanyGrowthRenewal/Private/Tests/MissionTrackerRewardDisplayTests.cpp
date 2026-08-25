#include "Misc/AutomationTest.h"
#include "UI/HUD/MissionTrackerDisplayRules.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRMissionTrackerRewardDisplayTest,
	"CGR.Mission.Tracker.RewardPresentation.FinalMissionOnly",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRMissionTrackerRewardDisplayTest::RunTest(const FString& Parameters)
{
	const FName IntermediateNextMissionIDs[] = {
		TEXT("M2_BuildFirstCompany"),
		TEXT("M3_EnterOffice"),
		TEXT("M4_PlaceDesk"),
		TEXT("M5_FirstRecruit"),
		TEXT("M6_InHouseProject"),
		TEXT("M7_CollectRevenue"),
	};
	for (int32 MissionIndex = 0;
		MissionIndex < static_cast<int32>(UE_ARRAY_COUNT(IntermediateNextMissionIDs));
		++MissionIndex)
	{
		FMissionTable IntermediateMission;
		IntermediateMission.NextMissionID = IntermediateNextMissionIDs[MissionIndex];
		FMissionReward IntermediateSupply;
		IntermediateSupply.ResourceType = EResourceType::Brick;
		IntermediateSupply.Amount = 400;
		IntermediateMission.Rewards.Add(IntermediateSupply);
		const FMissionTrackerRewardDisplayState IntermediateState =
			FMissionTrackerDisplayRules::ResolveRewardDisplay(IntermediateMission);
		TestFalse(*FString::Printf(TEXT("M%d 보상 구역은 숨긴다"), MissionIndex + 1),
			IntermediateState.bShowSection);
		TestTrue(*FString::Printf(TEXT("M%d 보상 라벨은 비운다"), MissionIndex + 1),
			IntermediateState.Label.IsEmpty());
	}

	FMissionTable FinalMission;
	FinalMission.NextMissionID = NAME_None;
	FMissionReward FinalReward;
	FinalReward.ResourceType = EResourceType::Brick;
	FinalReward.Amount = 20000;
	FinalMission.Rewards.Add(FinalReward);
	const FMissionTrackerRewardDisplayState FinalState =
		FMissionTrackerDisplayRules::ResolveRewardDisplay(FinalMission);
	TestTrue(TEXT("마지막 행의 피날레 보상만 표시한다"),
		FinalState.bShowSection);
	TestEqual(TEXT("피날레 라벨은 튜토리얼 완료 보상으로 표시한다"),
		FinalState.Label.ToString(), FString(TEXT("튜토리얼 완료 보상")));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
