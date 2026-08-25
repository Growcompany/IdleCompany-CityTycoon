#include "Misc/AutomationTest.h"
#include "UI/Element/Common/RevenueRatePulse.h"

#include <cfloat>
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRRevenueRatePulseTest,
	"CGR.RevenueRateChip.Pulse",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRRevenueRatePulseTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("음수는 표시 0"), GetRevenueRateDisplayKey(-7.0), int64{0});
	TestEqual(TEXT("1 미만은 표시 0"), GetRevenueRateDisplayKey(0.99), int64{0});
	TestEqual(TEXT("양수는 정수 절삭"), GetRevenueRateDisplayKey(12.99), int64{12});
	TestEqual(TEXT("음의 DBL_MAX는 표시 0"), GetRevenueRateDisplayKey(-DBL_MAX), int64{0});
	TestEqual(TEXT("DBL_MAX는 int64 상한 포화"), GetRevenueRateDisplayKey(DBL_MAX), MAX_int64);
	TestEqual(TEXT("양의 무한대는 표시 0"),
		GetRevenueRateDisplayKey(std::numeric_limits<double>::infinity()), int64{0});
	TestEqual(TEXT("음의 무한대는 표시 0"),
		GetRevenueRateDisplayKey(-std::numeric_limits<double>::infinity()), int64{0});
	TestEqual(TEXT("NaN은 표시 0"),
		GetRevenueRateDisplayKey(std::numeric_limits<double>::quiet_NaN()), int64{0});
	TestEqual(TEXT("double MAX_int64 경계는 2의 63승"),
		static_cast<double>(MAX_int64), 9223372036854775808.0);
	TestEqual(TEXT("2의 63승 경계는 int64 상한 포화"),
		GetRevenueRateDisplayKey(static_cast<double>(MAX_int64)), MAX_int64);
	TestEqual(TEXT("2의 63승 바로 아래 값은 절삭 유지"),
		GetRevenueRateDisplayKey(9223372036854774784.0), int64{9223372036854774784LL});
	TestFalse(TEXT("0 은 비활성"), IsRevenueRateActive(0.99));
	TestTrue(TEXT("1 부터 활성"), IsRevenueRateActive(1.0));

	TestEqual(TEXT("0 표기에는 증가 부호 없음"),
		FormatRevenueRateText(0, FText::GetEmpty()).ToString(), FString(TEXT("0/s")));
	TestEqual(TEXT("양수 표기는 증가 부호와 /s"),
		FormatRevenueRateText(12000, FText::FromString(TEXT("1.2만"))).ToString(),
		FString(TEXT("+1.2만/s")));

	TestFalse(TEXT("0 수입은 펄스 없음"), ShouldPulseRevenueRate(-1.0, 0.0, 0.05));
	TestTrue(TEXT("0 에서 첫 수입"), ShouldPulseRevenueRate(0.0, 100.0, 0.05));
	TestFalse(TEXT("4.9% 변화"), ShouldPulseRevenueRate(100.0, 104.9, 0.05));
	TestTrue(TEXT("5% 상승"), ShouldPulseRevenueRate(100.0, 105.0, 0.05));
	TestTrue(TEXT("5% 하락"), ShouldPulseRevenueRate(100.0, 95.0, 0.05));
	TestTrue(TEXT("Same display key raw-rate drop above 5% pulses"),
		ShouldPulseRevenueRate(1.99, 1.89, 0.05));
	TestFalse(TEXT("수입 종료는 펄스 없음"), ShouldPulseRevenueRate(100.0, 0.0, 0.05));

	TestEqual(TEXT("펄스 시작 1"), ComputeRevenuePulseStrength(0.0f, 0.45f), 1.0f);
	TestEqual(TEXT("펄스 종료 0"), ComputeRevenuePulseStrength(0.45f, 0.45f), 0.0f);
	TestTrue(TEXT("중간은 범위 안"),
		FMath::IsWithinInclusive(ComputeRevenuePulseStrength(0.225f, 0.45f), 0.0f, 1.0f));

	TestFalse(TEXT("음수도 표시 0 유지"), ShouldRefreshRevenueRateText(0, -3.0));
	TestFalse(TEXT("1 미만은 표시 0 유지"), ShouldRefreshRevenueRateText(0, 0.9));
	TestTrue(TEXT("0 에서 1 은 갱신"), ShouldRefreshRevenueRateText(0, 1.0));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
