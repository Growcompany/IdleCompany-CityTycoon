#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Data/CompanyDisplayNameRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCompanyDisplayNameRulesTest, "CGR.Office.CompanyDisplayName.Rules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCompanyDisplayNameRulesTest::RunTest(const FString& Parameters)
{
	using FEntry = FCompanyDisplayNameRules::FCompanyEntry;
	// 0 = 본사처럼 회사가 없는 건물, 그 뒤로 지은 순서대로 게임·게임·IT
	const TArray<FEntry> Built = {
		{ 0, ECompanyType::None }, { 3, ECompanyType::Game }, { 7, ECompanyType::Game }, { 9, ECompanyType::IT } };

	ECompanyType Type = ECompanyType::None;
	int32 Ordinal = 0;
	TestTrue(TEXT("첫 게임 회사를 찾는다"), FCompanyDisplayNameRules::FindOrdinal(Built, 3, Type, Ordinal));
	TestTrue(TEXT("첫 게임 회사 = 게임 1"), Type == ECompanyType::Game && Ordinal == 1);
	FCompanyDisplayNameRules::FindOrdinal(Built, 7, Type, Ordinal);
	TestTrue(TEXT("두 번째 게임 회사 = 게임 2"), Type == ECompanyType::Game && Ordinal == 2);
	FCompanyDisplayNameRules::FindOrdinal(Built, 9, Type, Ordinal);
	TestTrue(TEXT("그다음 IT 회사는 업종 안에서 다시 1"), Type == ECompanyType::IT && Ordinal == 1);

	// 첫 게임 회사를 철거하면 남은 게임 회사가 1로 당겨진다
	const TArray<FEntry> AfterDemolish = { { 0, ECompanyType::None }, { 7, ECompanyType::Game }, { 9, ECompanyType::IT } };
	FCompanyDisplayNameRules::FindOrdinal(AfterDemolish, 7, Type, Ordinal);
	TestTrue(TEXT("철거 뒤 남은 게임 회사 = 게임 1"), Type == ECompanyType::Game && Ordinal == 1);

	TestFalse(TEXT("회사가 없는 건물은 이름이 없다"), FCompanyDisplayNameRules::FindOrdinal(Built, 0, Type, Ordinal));
	TestFalse(TEXT("없는 건물 번호는 이름이 없다"), FCompanyDisplayNameRules::FindOrdinal(Built, 42, Type, Ordinal));
	TestFalse(TEXT("INDEX_NONE 은 이름이 없다"), FCompanyDisplayNameRules::FindOrdinal(Built, INDEX_NONE, Type, Ordinal));

	TestEqual(TEXT("표시 형식"), FCompanyDisplayNameRules::Format(FText::FromString(TEXT("게임")), 2).ToString(), FString(TEXT("게임회사 2")));
	TestEqual(TEXT("영문 업종도 붙여 쓴다"), FCompanyDisplayNameRules::Format(FText::FromString(TEXT("IT")), 1).ToString(), FString(TEXT("IT회사 1")));
	return true;
}

#endif
