#include "Misc/AutomationTest.h"
#include "Manager/RecruitmentManagerSubsystem.h"
#include "Data/GachaRecruitmentData.h"
#include "Enum/LootBoxRarity.h"
#include "Engine/GameInstance.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace RecruitmentOddsTestUtils
{
	float PercentOf(const TArray<FGachaRarityChance>& Rows, ELootBoxRarity Rarity)
	{
		const FGachaRarityChance* Row = Rows.FindByPredicate([Rarity](const FGachaRarityChance& Chance) { return Chance.Rarity == Rarity; });
		return Row ? Row->Percent : 0.f;
	}
}

// RECRUITMENT_SYSTEM.md 확률표(프리미엄 = 일반 0 · 고급 15 · 레어 40 · 에픽 30 · 레전드 15)와 화면·추첨이 같은 표를 읽는지 확인한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRRecruitmentPremiumOddsTest,
	"CGR.Recruitment.PremiumOddsMatchDesign",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRRecruitmentPremiumOddsTest::RunTest(const FString& Parameters)
{
	using namespace RecruitmentOddsTestUtils;
	UGameInstance* Instance = NewObject<UGameInstance>();
	const URecruitmentManagerSubsystem* Recruitment = NewObject<URecruitmentManagerSubsystem>(Instance);
	const TArray<FGachaRarityChance> Premium = Recruitment->GetProbabilityTableForUI(EGachaTier::Premium, 0);
	const TArray<FGachaRarityChance> Advanced = Recruitment->GetProbabilityTableForUI(EGachaTier::Advanced, 0);

	TestEqual(TEXT("프리미엄 일반 0%"), PercentOf(Premium, ELootBoxRarity::Common), 0.f, 0.01f);
	TestEqual(TEXT("프리미엄 고급 15%"), PercentOf(Premium, ELootBoxRarity::Unusual), 15.f, 0.01f);
	TestEqual(TEXT("프리미엄 레어 40%"), PercentOf(Premium, ELootBoxRarity::Rare), 40.f, 0.01f);
	TestEqual(TEXT("프리미엄 에픽 30%"), PercentOf(Premium, ELootBoxRarity::Epic), 30.f, 0.01f);
	TestEqual(TEXT("프리미엄 레전드 15%"), PercentOf(Premium, ELootBoxRarity::Legendary), 15.f, 0.01f);
	TestEqual(TEXT("프리미엄 신화 없음(큐브 전용)"), PercentOf(Premium, ELootBoxRarity::Mythic), 0.f, 0.01f);

	float Sum = 0.f;
	for (const FGachaRarityChance& Row : Premium) Sum += Row.Percent;
	TestEqual(TEXT("프리미엄 합계 100%"), Sum, 100.f, 0.01f);

	// 비싼 권종이 아래 권종보다 나쁜 결과를 내지 않는다(레어 이상 누적 확률).
	const auto RareOrBetter = [](const TArray<FGachaRarityChance>& Rows)
	{
		return PercentOf(Rows, ELootBoxRarity::Rare) + PercentOf(Rows, ELootBoxRarity::Epic) + PercentOf(Rows, ELootBoxRarity::Legendary);
	};
	TestTrue(TEXT("프리미엄 레어 이상 ≥ 고급 레어 이상"), RareOrBetter(Premium) >= RareOrBetter(Advanced));
	TestTrue(TEXT("프리미엄 레전드 ≥ 고급 레전드"),
		PercentOf(Premium, ELootBoxRarity::Legendary) >= PercentOf(Advanced, ELootBoxRarity::Legendary));
	return true;
}

#endif
