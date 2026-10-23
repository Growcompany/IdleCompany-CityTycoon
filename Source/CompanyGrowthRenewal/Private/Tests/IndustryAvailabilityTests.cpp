#include "Misc/AutomationTest.h"
#include "Table/CompanyInfoTable.h"
#include "Enum/CompanyType.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIndustryAvailabilityDefaultTest,
	"CGR.Industry.Availability.Defaults",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FIndustryAvailabilityDefaultTest::RunTest(const FString& Parameters)
{
	// 리임포트 누락으로 칼럼이 비어도 업종이 사라지면 안 된다 — 여는 쪽으로 실패한다
	const FCompanyInfoTable Fresh;
	TestTrue(TEXT("기본값은 가용"), Fresh.bAvailableAtLaunch);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIndustryTileStateTest,
	"CGR.Industry.Availability.TileState",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FIndustryTileStateTest::RunTest(const FString& Parameters)
{
	// 잠김(갈망 티저)과 미가용(접기)은 다른 상태다. 섞이면 영원히 안 열리는 회색 타일이 남는다.
	TestEqual(TEXT("가용+해금 = 정상"),
		DecideIndustryTileState(/*bAvailable=*/true, /*bUnlocked=*/true), EIndustryTileState::Unlocked);
	TestEqual(TEXT("가용+미해금 = 잠김(딤+자물쇠)"),
		DecideIndustryTileState(true, false), EIndustryTileState::Locked);
	TestEqual(TEXT("미가용 = 접기"),
		DecideIndustryTileState(false, false), EIndustryTileState::Hidden);

	// 미가용이면 해금 여부와 무관하게 접는다 — RequiredHQLevel 을 넘겼어도 이번 빌드엔 없는 업종이다
	TestEqual(TEXT("미가용+해금이어도 접기"),
		DecideIndustryTileState(false, true), EIndustryTileState::Hidden);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIndustryManufacturingEnumNotEmptyTest,
	"CGR.Industry.Availability.ManufacturingEnumNotEmpty",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FIndustryManufacturingEnumNotEmptyTest::RunTest(const FString& Parameters)
{
	// enum 비교만 본다 ― 가용성 필터 결과는 검증하지 않는다(월드·DT 없이 도는 테스트다).
	int32 ManufacturingCount = 0;
	for (uint8 Raw = (uint8)ECompanyType::None + 1; Raw < (uint8)ECompanyType::Max; ++Raw)
	{
		if (IsManufacturingType((ECompanyType)Raw)) ++ManufacturingCount;
	}
	TestTrue(TEXT("제조업 enum 이 최소 1종"), ManufacturingCount >= 1);

	// 출시 로스터에서 반도체가 빠져도 전자·자동차가 남는다
	TestTrue(TEXT("전자는 제조업"), IsManufacturingType(ECompanyType::Electronics));
	TestTrue(TEXT("자동차는 제조업"), IsManufacturingType(ECompanyType::Automobile));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIndustryDisplayNameTest,
	"CGR.Industry.DisplayName.Electronics",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FIndustryDisplayNameTest::RunTest(const FString& Parameters)
{
	// DT 와 C++ 가 "가전"/"전자" 로 갈려 있었다 — 반도체 흡수 후 "전자" 가 맞는 이름이다
	TestEqual(TEXT("Electronics 표시명"), CompanyTypeToString(ECompanyType::Electronics), FString(TEXT("전자")));
	TestEqual(TEXT("왕복 변환"), StringToCompanyType(TEXT("전자")), ECompanyType::Electronics);
	return true;
}
#endif
