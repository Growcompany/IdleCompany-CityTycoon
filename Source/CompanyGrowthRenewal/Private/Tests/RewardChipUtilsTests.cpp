#include "Misc/AutomationTest.h"
#include "UI/Element/Common/RewardChipUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRRewardChipMixedMissionRewardsTest,
	"CGR.UI.RewardChip.MixedMissionRewards",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRRewardChipMixedMissionRewardsTest::RunTest(const FString& Parameters)
{
	TArray<FMissionReward> Rewards;

	FMissionReward MoneyReward;
	MoneyReward.ResourceType = EResourceType::Money;
	MoneyReward.Amount = 200000;
	Rewards.Add(MoneyReward);

	FMissionReward DiamondReward;
	DiamondReward.ResourceType = EResourceType::Diamond;
	DiamondReward.Amount = 100;
	Rewards.Add(DiamondReward);

	FMissionReward SkinTicketReward;
	SkinTicketReward.ItemType = EItemType::SkinTicketNormal;
	SkinTicketReward.ItemAmount = 1;
	Rewards.Add(SkinTicketReward);

	const TArray<CGRewardChip::FDisplayEntry> Entries =
		CGRewardChip::BuildDisplayEntries(Rewards);
	if (!TestEqual(TEXT("M7의 자원 2개와 아이템 1개를 모두 표시한다"), Entries.Num(), 3))
	{
		return true;
	}

	const CGRewardChip::FDisplayEntry* MoneyEntry = Entries.FindByPredicate(
		[](const CGRewardChip::FDisplayEntry& Entry)
		{
			return Entry.ResourceType == EResourceType::Money;
		});
	const CGRewardChip::FDisplayEntry* DiamondEntry = Entries.FindByPredicate(
		[](const CGRewardChip::FDisplayEntry& Entry)
		{
			return Entry.ResourceType == EResourceType::Diamond;
		});
	const CGRewardChip::FDisplayEntry* ItemEntry = Entries.FindByPredicate(
		[](const CGRewardChip::FDisplayEntry& Entry)
		{
			return Entry.ItemType == EItemType::SkinTicketNormal;
		});

	if (TestNotNull(TEXT("Money 칩이 존재한다"), MoneyEntry))
	{
		TestEqual(TEXT("Money 수량은 200000이다"), MoneyEntry->Quantity, int64(200000));
	}
	if (TestNotNull(TEXT("Diamond 칩이 존재한다"), DiamondEntry))
	{
		TestEqual(TEXT("Diamond 수량은 100이다"), DiamondEntry->Quantity, int64(100));
	}
	if (TestNotNull(TEXT("SkinTicketNormal 칩이 존재한다"), ItemEntry))
	{
		TestEqual(TEXT("SkinTicketNormal 수량은 1이다"), ItemEntry->Quantity, int64(1));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRRewardChipItemOnlyMissionRewardTest,
	"CGR.UI.RewardChip.ItemOnlyMissionReward",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRRewardChipItemOnlyMissionRewardTest::RunTest(const FString& Parameters)
{
	FMissionReward SkinTicketReward;
	SkinTicketReward.ItemType = EItemType::SkinTicketNormal;
	SkinTicketReward.ItemAmount = 1;

	const TArray<CGRewardChip::FDisplayEntry> Entries =
		CGRewardChip::BuildDisplayEntries({ SkinTicketReward });
	if (!TestEqual(TEXT("아이템 전용 보상도 한 칩으로 해석한다"), Entries.Num(), 1))
	{
		return true;
	}

	TestEqual(TEXT("아이템 전용 칩 타입을 보존한다"),
		Entries[0].ItemType, EItemType::SkinTicketNormal);
	TestEqual(TEXT("아이템 전용 칩 수량을 보존한다"), Entries[0].Quantity, int64(1));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
