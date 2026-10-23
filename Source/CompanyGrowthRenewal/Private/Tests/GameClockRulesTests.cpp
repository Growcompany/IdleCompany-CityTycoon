#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Data/GameClockRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameClockRulesTest, "CGR.LiveOps.ClockRules",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FGameClockRulesTest::RunTest(const FString& Parameters)
{
	// 2026-09-13 은 일요일. UTC 14:59:59 = KST 23:59:59 / UTC 15:00:00 = KST 다음날 00:00:00
	const int64 SunLate = FDateTime(2026, 9, 13, 14, 59, 59).ToUnixTimestamp();
	const int64 MonStart = FDateTime(2026, 9, 13, 15, 0, 0).ToUnixTimestamp();

	TestEqual(TEXT("day key before KST midnight"), FGameClockRules::DayKeyFromUnix(SunLate), FString(TEXT("2026-09-13")));
	TestEqual(TEXT("day key at KST midnight"), FGameClockRules::DayKeyFromUnix(MonStart), FString(TEXT("2026-09-14")));
	TestEqual(TEXT("week key on sunday = that week's monday"), FGameClockRules::WeekKeyFromUnix(SunLate), FString(TEXT("2026-09-07")));
	TestEqual(TEXT("week key on monday 00:00 = itself"), FGameClockRules::WeekKeyFromUnix(MonStart), FString(TEXT("2026-09-14")));
	TestEqual(TEXT("1 second to next day"), FGameClockRules::SecondsUntilNextDayKst(SunLate), (int64)1);
	TestEqual(TEXT("full day at midnight"), FGameClockRules::SecondsUntilNextDayKst(MonStart), (int64)86400);
	TestEqual(TEXT("1 second to next week"), FGameClockRules::SecondsUntilNextWeekKst(SunLate), (int64)1);
	TestEqual(TEXT("7 days at monday 00:00"), FGameClockRules::SecondsUntilNextWeekKst(MonStart), (int64)(7 * 86400));
	TestEqual(TEXT("hour kst"), FGameClockRules::HourKst(SunLate), 23);
	TestEqual(TEXT("countdown hh:mm:ss"), FGameClockRules::FormatCountdown(3 * 3600 + 12 * 60 + 44), FString(TEXT("03:12:44")));
	TestEqual(TEXT("countdown days"), FGameClockRules::FormatCountdown(2 * 86400 + 3 * 3600 + 12 * 60), FString(TEXT("2일 03:12")));
	TestEqual(TEXT("countdown zero"), FGameClockRules::FormatCountdown(0), FString(TEXT("00:00:00")));
	TestEqual(TEXT("countdown negative clamps"), FGameClockRules::FormatCountdown(-5), FString(TEXT("00:00:00")));

	// T3 — 연말 경계: UTC 2026-12-31 15:00 = KST 2027-01-01 00:00 (금요일, Monday=0 이므로 그 주 월요일은 2026-12-28)
	const int64 NewYearKst = FDateTime(2026, 12, 31, 15, 0, 0).ToUnixTimestamp();
	TestEqual(TEXT("day key rolls into the new year"), FGameClockRules::DayKeyFromUnix(NewYearKst), FString(TEXT("2027-01-01")));
	TestEqual(TEXT("week key stays in the old year"), FGameClockRules::WeekKeyFromUnix(NewYearKst), FString(TEXT("2026-12-28")));
	TestEqual(TEXT("day key one second earlier"), FGameClockRules::DayKeyFromUnix(NewYearKst - 1), FString(TEXT("2026-12-31")));
	TestEqual(TEXT("full day at new year midnight"), FGameClockRules::SecondsUntilNextDayKst(NewYearKst), (int64)86400);
	TestEqual(TEXT("hour 0 at new year midnight"), FGameClockRules::HourKst(NewYearKst), 0);

	// unix 0 = UTC 1970-01-01 00:00 = KST 09:00 — 같은 날
	TestEqual(TEXT("unix 0 day key"), FGameClockRules::DayKeyFromUnix(0), FString(TEXT("1970-01-01")));

	// 음수 unix 도 죽지 않고 형식이 맞는 키를 낸다 (관측값 고정)
	const int64 BeforeEpoch = -2 * 86400;
	const FString NegKey = FGameClockRules::DayKeyFromUnix(BeforeEpoch);
	TestEqual(TEXT("negative unix day key is well formed"), NegKey.Len(), 10);
	TestEqual(TEXT("negative unix day key value"), NegKey, FString(TEXT("1969-12-30")));
	TestTrue(TEXT("negative unix seconds into day is in range"),
		FGameClockRules::SecondsIntoKstDay(BeforeEpoch) >= 0 && FGameClockRules::SecondsIntoKstDay(BeforeEpoch) < 86400);
	return true;
}

#endif
