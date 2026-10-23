#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Data/EmployeePotentialData.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPotentialValueTextTest, "CGR.Employee.PotentialValueText",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPotentialValueTextTest::RunTest(const FString& Parameters)
{
	FPotentialOptionLine Line;
	Line.OptionType = EPotentialOptionType::WorkEfficiency; Line.Value = 2.4f;
	TestEqual(TEXT("일반 옵션은 정수 %"), UEmployeePotentialHelper::GetPotentialOptionValueText(Line).ToString(), FString(TEXT("+2%")));

	Line.OptionType = EPotentialOptionType::CriticalChance; Line.Value = 25.f;
	TestEqual(TEXT("크리 확률은 %p 환산"), UEmployeePotentialHelper::GetPotentialOptionValueText(Line).ToString(), FString(TEXT("+1.0%p")));

	Line.OptionType = EPotentialOptionType::CriticalDamage; Line.Value = 25.f;
	TestEqual(TEXT("크리 배율은 배 환산"), UEmployeePotentialHelper::GetPotentialOptionValueText(Line).ToString(), FString(TEXT("+0.5배")));

	TestEqual(TEXT("잠금 줄 1"), UEmployeePotentialHelper::GetLockedLineUnlockText(1), FString(TEXT("★3에 열림")));
	TestEqual(TEXT("잠금 줄 2"), UEmployeePotentialHelper::GetLockedLineUnlockText(2), FString(TEXT("★6에 열림")));
	TestEqual(TEXT("잠금 줄 0 은 없음"), UEmployeePotentialHelper::GetLockedLineUnlockText(0), FString());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPotentialUpgradeChanceTest, "CGR.Employee.PotentialUpgradeChance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPotentialUpgradeChanceTest::RunTest(const FString& Parameters)
{
	// 기본표: <0.5 Legendary / <2.5 Epic / <10 Rare / <30 Unusual / else Common
	TestEqual(TEXT("일반 직원 + 종이(상한 레어) = 30%"),
		UEmployeePotentialHelper::GetRarityUpgradeChancePercent(ELootBoxRarity::Common, ELootBoxRarity::Common, ELootBoxRarity::Rare), 30.f, 0.01f);
	TestEqual(TEXT("일반 직원 + 블랙(상한 레전드) 도 30% ― 명함은 상한만 바꾼다"),
		UEmployeePotentialHelper::GetRarityUpgradeChancePercent(ELootBoxRarity::Common, ELootBoxRarity::Common, ELootBoxRarity::Legendary), 30.f, 0.01f);
	TestEqual(TEXT("레어 달성 + 종이 = 0% (오르지 않음)"),
		UEmployeePotentialHelper::GetRarityUpgradeChancePercent(ELootBoxRarity::Rare, ELootBoxRarity::Rare, ELootBoxRarity::Rare), 0.f, 0.01f);
	TestEqual(TEXT("레어 달성 + 골드(상한 에픽) = 2.5%"),
		UEmployeePotentialHelper::GetRarityUpgradeChancePercent(ELootBoxRarity::Rare, ELootBoxRarity::Rare, ELootBoxRarity::Epic), 2.5f, 0.01f);

	const TArray<FPotentialRarityOdds> CommonResults = UEmployeePotentialHelper::GetEffectiveRarityOdds(
		ELootBoxRarity::Common, ELootBoxRarity::Legendary);
	TestEqual(TEXT("일반에서 재설정하면 일반·고급 두 결과만 가능"), CommonResults.Num(), 2);
	if (CommonResults.Num() == 2)
	{
		TestEqual(TEXT("일반의 상승 결과는 고급 한 단계"), CommonResults[0].Rarity, ELootBoxRarity::Unusual);
		TestEqual(TEXT("일반에서 고급으로 상승할 확률"), CommonResults[0].Percent, 30.f, 0.01f);
		TestEqual(TEXT("일반 등급 유지 확률"), CommonResults[1].Percent, 70.f, 0.01f);
	}

	const TArray<FPotentialRarityOdds> RareResults = UEmployeePotentialHelper::GetEffectiveRarityOdds(
		ELootBoxRarity::Rare, ELootBoxRarity::Legendary);
	TestEqual(TEXT("레어에서 재설정하면 레어·에픽 두 결과만 가능"), RareResults.Num(), 2);
	if (RareResults.Num() == 2)
	{
		TestEqual(TEXT("레어의 상승 결과는 에픽 한 단계"), RareResults[0].Rarity, ELootBoxRarity::Epic);
		TestEqual(TEXT("레어에서 에픽으로 상승할 확률"), RareResults[0].Percent, 2.5f, 0.01f);
	}
	return true;
}

#endif
