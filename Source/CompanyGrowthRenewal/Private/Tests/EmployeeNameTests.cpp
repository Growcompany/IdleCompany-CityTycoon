#include "Misc/AutomationTest.h"
#include "Table/EmployeeNameTable.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGREmployeeNameComposeTest,
	"CGR.EmployeeName.Compose",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGREmployeeNameComposeTest::RunTest(const FString& Parameters)
{
	// 별명 핸들은 성이 앞에 붙는다
	TestEqual(TEXT("한국어 조합"),
		ComposeEmployeeName(TEXT("김"), TEXT("졸림"), false, TEXT("Korean")),
		FString(TEXT("김졸림")));

	// 통짜 이름에 성을 또 붙이면 "김황금손"이 된다 — 이 작업의 핵심 회귀 지점
	TestEqual(TEXT("통짜는 성이 안 붙는다"),
		ComposeEmployeeName(TEXT("김"), TEXT("황금손"), true, TEXT("Korean")),
		FString(TEXT("황금손")));

	// 통짜는 성 인자가 비어 있어도 같은 결과
	TestEqual(TEXT("통짜는 성 인자를 무시한다"),
		ComposeEmployeeName(FString(), TEXT("유니콘"), true, TEXT("Korean")),
		FString(TEXT("유니콘")));

	// 한국어가 아니면 성이 뒤로 간다
	TestEqual(TEXT("영어 조합"),
		ComposeEmployeeName(TEXT("Smith"), TEXT("Ace"), false, TEXT("English")),
		FString(TEXT("Ace Smith")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGREmployeeNameSurnameGuardTest,
	"CGR.EmployeeName.SurnameGuard",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGREmployeeNameSurnameGuardTest::RunTest(const FString& Parameters)
{
	const TArray<FString> Surnames = { TEXT("정"), TEXT("김"), TEXT("박"), TEXT("우"), TEXT("차") };

	// 첫 글자가 겹치면 "정정석"이 된다
	{
		const TArray<FString> Filtered = FilterSurnamesForHandle(Surnames, TEXT("정석"));
		TestFalse(TEXT("정석 -> 정 제외"), Filtered.Contains(TEXT("정")));
		TestTrue(TEXT("정석 -> 김 유지"), Filtered.Contains(TEXT("김")));
	}

	// 끝 글자가 겹쳐도 "우우수"가 된다 — 첫 글자 비교로는 못 잡는 유형
	{
		const TArray<FString> Filtered = FilterSurnamesForHandle(Surnames, TEXT("우수"));
		TestFalse(TEXT("우수 -> 우 제외"), Filtered.Contains(TEXT("우")));
		TestTrue(TEXT("우수 -> 박 유지"), Filtered.Contains(TEXT("박")));
	}

	// 3글자 핸들의 끝 글자도 잡힌다
	{
		const TArray<FString> Filtered = FilterSurnamesForHandle(Surnames, TEXT("초격차"));
		TestFalse(TEXT("초격차 -> 차 제외"), Filtered.Contains(TEXT("차")));
	}

	// 안 겹치면 하나도 안 빠진다
	{
		const TArray<FString> Filtered = FilterSurnamesForHandle(Surnames, TEXT("졸림"));
		TestEqual(TEXT("졸림 -> 전원 유지"), Filtered.Num(), Surnames.Num());
	}

	// 후보가 전부 걸리면 원본 폴백 — 이름을 못 만드는 것보다 낫다
	{
		const TArray<FString> Single = { TEXT("정") };
		const TArray<FString> Filtered = FilterSurnamesForHandle(Single, TEXT("정석"));
		TestEqual(TEXT("전부 걸리면 원본 폴백"), Filtered.Num(), 1);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
