#include "Misc/AutomationTest.h"
#include "UI/Element/Common/HelpTableProvider.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRHelpTableAxisTest,
	"CGR.Help.TableAxis",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRHelpTableAxisTest::RunTest(const FString& Parameters)
{
	// 축이 비었거나 하나뿐이면 전환기를 내보내지 않는다 ― 고를 게 없는 컨트롤은 자리만 먹는다
	TestFalse(TEXT("페이저: 축 없음"), ShouldShowPager(0));
	TestFalse(TEXT("페이저: 한 장뿐"), ShouldShowPager(1));
	TestTrue (TEXT("페이저: 두 장"),   ShouldShowPager(2));

	TestFalse(TEXT("탭: 축 없음"),   ShouldShowAxisTabs(0));
	TestFalse(TEXT("탭: 한 항목뿐"), ShouldShowAxisTabs(1));
	TestTrue (TEXT("탭: 두 항목"),   ShouldShowAxisTabs(2));

	// 축이 없는 표도 표다 ― 둘 다 접힌 상태가 정상 경로여야 한다
	FHelpTableInfo Plain;
	TestFalse(TEXT("무축 표: 페이저 접힘"), ShouldShowPager(Plain.PageLabels.Num()));
	TestFalse(TEXT("무축 표: 탭 접힘"),     ShouldShowAxisTabs(Plain.AxisLabels.Num()));

	return true;
}

#endif
