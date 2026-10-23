#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Data/AttendanceRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttendanceRulesTest, "CGR.LiveOps.AttendanceRules",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FAttendanceRulesTest::RunTest(const FString& Parameters)
{
	FAttendanceSaveData Save;
	TestTrue(TEXT("fresh can claim"), FAttendanceRules::CanClaim(Save, TEXT("2026-09-13")));
	TestEqual(TEXT("fresh next day = 1"), FAttendanceRules::NextDay(Save), 1);

	FAttendanceRules::ApplyClaim(Save, TEXT("2026-09-13"));
	TestEqual(TEXT("cycle day 1"), Save.CycleDay, 1);
	TestEqual(TEXT("total 1"), Save.TotalDays, 1);
	TestFalse(TEXT("same day twice not allowed"), FAttendanceRules::CanClaim(Save, TEXT("2026-09-13")));
	TestTrue(TEXT("next day allowed"), FAttendanceRules::CanClaim(Save, TEXT("2026-09-14")));

	// 놓친 날은 건너뛰지 않는다 — 9/16 에 와도 2일차
	TestEqual(TEXT("missed days do not skip"), FAttendanceRules::NextDay(Save), 2);
	FAttendanceRules::ApplyClaim(Save, TEXT("2026-09-16"));
	TestEqual(TEXT("cycle day 2"), Save.CycleDay, 2);

	Save.CycleDay = 28;
	TestEqual(TEXT("wraps to 1 after 28"), FAttendanceRules::NextDay(Save), 1);
	FAttendanceRules::ApplyClaim(Save, TEXT("2026-09-17"));
	TestEqual(TEXT("cycle day wrapped"), Save.CycleDay, 1);
	TestEqual(TEXT("total keeps counting"), Save.TotalDays, 3);

	// 시계가 권위를 못 잡으면 하루 키가 비어 온다 — 그 상태로 수령되면 안 된다
	TestFalse(TEXT("empty day key cannot claim"), FAttendanceRules::CanClaim(Save, FString()));
	FAttendanceSaveData Fresh;
	TestFalse(TEXT("empty day key cannot claim even when fresh"), FAttendanceRules::CanClaim(Fresh, FString()));
	return true;
}

#endif
