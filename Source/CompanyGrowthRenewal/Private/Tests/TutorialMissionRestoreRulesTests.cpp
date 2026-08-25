#include "Misc/AutomationTest.h"
#include "Data/TutorialMissionRestoreRules.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRTutorialMissionRestoreRulesTest,
	"CGR.Mission.TutorialClaimReadyRestorePolicy",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRTutorialMissionRestoreRulesTest::RunTest(const FString& Parameters)
{
	const FName FinalMission(TEXT("M7_CollectRevenue"));

	TestTrue(TEXT("Saved final claim-ready state restores for the same final mission"),
		FTutorialMissionRestoreRules::ShouldRestoreClaimReady(
			true, FinalMission, FinalMission, /*bHasNextMission=*/false));
	TestFalse(TEXT("A saved ready flag never restores onto an intermediate mission"),
		FTutorialMissionRestoreRules::ShouldRestoreClaimReady(
			true, TEXT("M6_InHouseProject"), TEXT("M6_InHouseProject"), /*bHasNextMission=*/true));
	TestFalse(TEXT("A dev override cannot inherit another mission's ready flag"),
		FTutorialMissionRestoreRules::ShouldRestoreClaimReady(
			true, FinalMission, TEXT("M1_CollectBricks"), /*bHasNextMission=*/true));
	TestFalse(TEXT("A completed chain cannot restore claim-ready without an active mission"),
		FTutorialMissionRestoreRules::ShouldRestoreClaimReady(
			true, NAME_None, NAME_None, /*bHasNextMission=*/false));
	TestFalse(TEXT("An unfinished final mission remains unfinished after restore"),
		FTutorialMissionRestoreRules::ShouldRestoreClaimReady(
			false, FinalMission, FinalMission, /*bHasNextMission=*/false));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
