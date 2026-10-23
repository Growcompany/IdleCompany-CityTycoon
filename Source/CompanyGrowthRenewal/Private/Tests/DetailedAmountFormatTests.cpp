#include "Misc/AutomationTest.h"
#include "Global/GlobalUtilFunctions.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDetailedAmountGroupingTest,
	"CGR.Number.DetailedAmount.Grouping",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDetailedAmountFitTest,
	"CGR.Number.DetailedAmount.FitToWidth",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace
{
	// 가짜 측정기 — ASCII 10px, 한글 단위 20px. 실제 폰트/Slate 없이 사다리 선택만 검증한다.
	float FakeMeasure(const FString& Candidate)
	{
		float Width = 0.f;
		for (const TCHAR Ch : Candidate)
		{
			Width += (Ch > 127) ? 20.f : 10.f;
		}
		return Width;
	}

	FString FitResult(int64 Value, float Budget)
	{
		const int32 Groups = UGlobalUtilFunctions::SelectAmountDetailGroups(Value, Budget, &FakeMeasure);
		return (Groups > 0)
			? UGlobalUtilFunctions::FormatKoreanUnitsDetailed(Value, Groups).ToString()
			: UGlobalUtilFunctions::AbbreviateNumber(Value, ENumberAbbrevStyle::KoreanUnit).ToString();
	}
}

bool FDetailedAmountGroupingTest::RunTest(const FString& Parameters)
{
	auto Fmt = [](int64 Value, int32 Groups)
	{
		return UGlobalUtilFunctions::FormatKoreanUnitsDetailed(Value, Groups).ToString();
	};

	TestEqual(TEXT("9자리 3그룹"), Fmt(134422342, 3), TEXT("1억 3442만 2342"));
	TestEqual(TEXT("9자리 2그룹"), Fmt(134422342, 2), TEXT("1억 3442만"));
	TestEqual(TEXT("9자리 1그룹"), Fmt(134422342, 1), TEXT("1억"));

	// 빈 그룹은 자리만 비운다 — "1억 0만 2342" 같은 표기를 만들지 않는다.
	TestEqual(TEXT("중간 0 그룹 생략"), Fmt(100002342, 3), TEXT("1억 2342"));
	TestEqual(TEXT("하위 전부 0"), Fmt(100000000, 3), TEXT("1억"));

	TestEqual(TEXT("조 단위 4그룹"), Fmt(1234567890123, 4), TEXT("1조 2345억 6789만 123"));
	TestEqual(TEXT("조 단위 2그룹"), Fmt(1234567890123, 2), TEXT("1조 2345억"));

	// 단위 미만은 기존 축약과 같은 콤마 원본 — 컬처 의존이라 FText 로 비교한다.
	TestEqual(TEXT("만 미만"), Fmt(9999, 3), FText::AsNumber(9999).ToString());
	TestEqual(TEXT("0"), Fmt(0, 3), FText::AsNumber(0).ToString());

	TestEqual(TEXT("음수"), Fmt(-134422342, 3), TEXT("-1억 3442만 2342"));

	TestEqual(TEXT("그룹 0 클램프"), Fmt(134422342, 0), TEXT("1억"));
	TestEqual(TEXT("그룹 과대 클램프"), Fmt(134422342, 99), TEXT("1억 3442만 2342"));

	// 부호 반전이 오버플로하는 유일한 입력.
	TestEqual(TEXT("int64 최솟값"), Fmt(MIN_int64, 5), TEXT("-922경 3372조 368억 5477만 5808"));

	return true;
}

bool FDetailedAmountFitTest::RunTest(const FString& Parameters)
{
	// 134,422,342 의 최악폭 후보: 3그룹 "8억 8888만 8888"=150, 2그룹 "8억 8888만"=100, 축약 "1.34억"=60
	TestEqual(TEXT("넓은 칸 = 3그룹"), UGlobalUtilFunctions::SelectAmountDetailGroups(134422342, 160.f, &FakeMeasure), 3);
	TestEqual(TEXT("중간 칸 = 2그룹"), UGlobalUtilFunctions::SelectAmountDetailGroups(134422342, 120.f, &FakeMeasure), 2);
	TestEqual(TEXT("좁은 칸 = 축약 폴백"), UGlobalUtilFunctions::SelectAmountDetailGroups(134422342, 80.f, &FakeMeasure), 0);

	// 1그룹("1억")은 축약("1.34억")보다 정보가 적어 사다리에 없다.
	TestNotEqual(TEXT("1그룹은 고르지 않는다"), UGlobalUtilFunctions::SelectAmountDetailGroups(134422342, 90.f, &FakeMeasure), 1);

	// 레이아웃 전(폭 0)은 축약으로 폴백한다.
	TestEqual(TEXT("폭 0 = 폴백"), UGlobalUtilFunctions::SelectAmountDetailGroups(134422342, 0.f, &FakeMeasure), 0);

	// 카운트업 떨림 방지 — 자릿수가 같으면 값이 달라도 같은 상세도를 고른다.
	const int32 DenseDigits = UGlobalUtilFunctions::SelectAmountDetailGroups(134422342, 120.f, &FakeMeasure);
	const int32 RoundDigits = UGlobalUtilFunctions::SelectAmountDetailGroups(100000000, 120.f, &FakeMeasure);
	TestEqual(TEXT("같은 자릿수 = 같은 상세도"), RoundDigits, DenseDigits);

	// 고른 결과는 예산을 넘지 않는다 — 최악폭 기준이라 실제 문자열은 더 좁다.
	const float Budget = 120.f;
	const int64 Samples[] = { 12345, 134422342, 100000000, 1234567890123, -134422342, MIN_int64 };
	for (const int64 Value : Samples)
	{
		const FString Fitted = FitResult(Value, Budget);
		TestTrue(FString::Printf(TEXT("%lld -> \"%s\" 는 예산 내"), Value, *Fitted), FakeMeasure(Fitted) <= Budget);
	}

	return true;
}

#endif
