#include "Misc/AutomationTest.h"
#include "Utils/LaunchRewardRevealMath.h"

#if WITH_DEV_AUTOMATION_TESTS

// 아이템 보상의 수량은 Amount 가 아니라 ItemAmount 다 (ULaunchLootManagerSubsystem::RollAndGrant 와 동일 계약)
static FMissionReward MakeItem(EItemType Type, int32 Amount = 1)
{
	FMissionReward R;
	R.ItemType = Type;
	R.ItemAmount = Amount;
	return R;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRRevealOrderTest,
	"CGR.Launch.RewardReveal.Order",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRRevealOrderTest::RunTest(const FString& Parameters)
{
	TArray<FMissionReward> In;
	In.Add(MakeItem(EItemType::RecruitTicketPremium));
	In.Add(MakeItem(EItemType::RecruitTicketNormal));
	In.Add(MakeItem(EItemType::SkinTicketNormal));
	In.Add(MakeItem(EItemType::RecruitTicketNormal));
	In.Add(MakeItem(EItemType::BuildingTraitTicketAdvanced));

	const TArray<FMissionReward> Out = LaunchRewardRevealMath::BuildRevealOrder(In);
	TestEqual(TEXT("같은 ItemType 병합 → 4장"), Out.Num(), 4);
	TestEqual(TEXT("첫 장 = 일반 채용권"), Out[0].ItemType, EItemType::RecruitTicketNormal);
	TestEqual(TEXT("병합 수량 2"), Out[0].ItemAmount, 2);
	TestEqual(TEXT("마지막 = 스킨N(서열 6, 최고가 마지막)"), Out.Last().ItemType, EItemType::SkinTicketNormal);
	TestEqual(TEXT("서열: 프리미엄(4) < 특성A(5) < 스킨N(6)"), Out[1].ItemType, EItemType::RecruitTicketPremium);
	TestEqual(TEXT("서열 3번째 = 특성A"), Out[2].ItemType, EItemType::BuildingTraitTicketAdvanced);
	TestEqual(TEXT("서열 4번째 = 스킨N"), Out[3].ItemType, EItemType::SkinTicketNormal);
	TestEqual(TEXT("빈 입력 → 빈 출력"), LaunchRewardRevealMath::BuildRevealOrder({}).Num(), 0);

	// 자원 보상은 수량이 Amount 에 있다 — 병합이 두 수량 축을 각각 합치는지 확인
	FMissionReward Cash;
	Cash.ResourceType = EResourceType::Money;
	Cash.Amount = 5;
	const TArray<FMissionReward> CashOut = LaunchRewardRevealMath::BuildRevealOrder({Cash, Cash});
	TestEqual(TEXT("같은 자원 병합 → 1장"), CashOut.Num(), 1);
	TestEqual(TEXT("자원 수량 합산 10"), CashOut[0].Amount, (int64)10);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRRevealStampRunTest,
	"CGR.Launch.RewardReveal.StampRun",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRRevealStampRunTest::RunTest(const FString& Parameters)
{
	const TArray<float> T = LaunchRewardRevealMath::StampRunTimes(5, 2.15f, 0.12f, 0.9f, 0.10f);
	TestEqual(TEXT("개수"), T.Num(), 5);
	TestTrue(TEXT("첫 장 = Start"), FMath::IsNearlyEqual(T[0], 2.15f));
	TestTrue(TEXT("2장 간격 0.12"), FMath::IsNearlyEqual(T[1] - T[0], 0.12f, 0.0001f));
	TestTrue(TEXT("3장 간격 0.108 (가속)"), FMath::IsNearlyEqual(T[2] - T[1], 0.108f, 0.0001f));
	TestTrue(TEXT("하한 0.10 적용 (4→5장: 0.12*0.9^3=0.087 → 0.10)"), FMath::IsNearlyEqual(T[4] - T[3], 0.10f, 0.0001f));
	TestEqual(TEXT("0장"), LaunchRewardRevealMath::StampRunTimes(0, 2.15f, 0.12f, 0.9f, 0.10f).Num(), 0);
	return true;
}

#endif
