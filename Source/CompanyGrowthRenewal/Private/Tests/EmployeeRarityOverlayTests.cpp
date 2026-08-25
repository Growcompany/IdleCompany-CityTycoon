#include "Misc/AutomationTest.h"
#include "Entity/Officeworker/StickOfficeworker.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGREmployeeRarityOverlayContextTest,
	"CGR.Employee.Cosmetics.RarityOverlayContext",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGREmployeeRarityOverlayContextTest::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("Office Premium 직원은 희귀도 Overlay를 표시하지 않는다"),
		AStickOfficeworker::ShouldShowRarityOverlay(EGachaTier::Premium, false));
	TestTrue(TEXT("가챠 공개 중인 Premium 직원만 희귀도 Overlay를 표시한다"),
		AStickOfficeworker::ShouldShowRarityOverlay(EGachaTier::Premium, true));
	TestFalse(TEXT("가챠 공개 중이어도 Advanced 직원은 희귀도 Overlay를 표시하지 않는다"),
		AStickOfficeworker::ShouldShowRarityOverlay(EGachaTier::Advanced, true));
	TestFalse(TEXT("가챠 공개 중이어도 Normal 직원은 희귀도 Overlay를 표시하지 않는다"),
		AStickOfficeworker::ShouldShowRarityOverlay(EGachaTier::Normal, true));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
