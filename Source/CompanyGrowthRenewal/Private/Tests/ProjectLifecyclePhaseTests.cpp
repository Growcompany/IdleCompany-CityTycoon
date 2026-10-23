#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Enum/ProjectLifecycle.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGRFocusLockedPhaseTest, "CGR.Office.FocusLockedPhase",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRFocusLockedPhaseTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("개발 중"), IsFocusLockedPhase(EProjectLifecycle::Developing));
	// 컨티뉴(추가 개발)로 개발에 되돌아갈 수 있어 출시 대기까지가 한 구간이다
	TestTrue(TEXT("출시 대기"), IsFocusLockedPhase(EProjectLifecycle::LaunchPending));
	// 아래 셋은 개발이 끝난 상태 — 배속이 남으면 직원만 2배속으로 움직인다
	TestFalse(TEXT("출시 후 운영"), IsFocusLockedPhase(EProjectLifecycle::Operating));
	TestFalse(TEXT("운영 리포트"), IsFocusLockedPhase(EProjectLifecycle::ReportPending));
	TestFalse(TEXT("대기(폐기 직후 포함)"), IsFocusLockedPhase(EProjectLifecycle::Idle));
	return true;
}

#endif
