#include "Misc/AutomationTest.h"
#include "Data/TutorialMissionRestoreRules.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRTutorialMissionRestoreRulesTest,
	"CGR.Mission.TutorialClaimReadyRestorePolicy",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRTutorialMissionRestoreRulesTest::RunTest(const FString& Parameters)
{
	const FName RewardMission(TEXT("M7_CollectRevenue"));
	FShippedProjectRecord Shipped;
	Shipped.BuildingIndex = 4;
	Shipped.Industry = ECompanyType::Game;
	Shipped.ProjectIndex = 2;
	TArray<FShippedProjectRecord> Records = { Shipped };
	TestFalse(TEXT("Another operating project cannot complete the required first launch"),
		FTutorialMissionRestoreRules::HasRequiredLaunch(Records, 4, ECompanyType::Game, 1));
	Records[0].ProjectIndex = 1;
	TestFalse(TEXT("A launch at another company cannot restore this company's lesson"),
		FTutorialMissionRestoreRules::HasRequiredLaunch(Records, 5, ECompanyType::Game, 1));
	TestTrue(TEXT("The configured project restores even after its operation has ended"),
		FTutorialMissionRestoreRules::HasRequiredLaunch(Records, 4, ECompanyType::Game, 1));

	TestTrue(TEXT("Saved claim-ready state restores for the final M7 reward"),
		FTutorialMissionRestoreRules::ShouldRestoreClaimReady(
			true, RewardMission, RewardMission, /*bManualClaim=*/true));
	TestFalse(TEXT("A saved ready flag never restores onto an automatic mission"),
		FTutorialMissionRestoreRules::ShouldRestoreClaimReady(
			true, TEXT("M6_LaunchFirstProject"), TEXT("M6_LaunchFirstProject"), /*bManualClaim=*/false));
	TestFalse(TEXT("A dev override cannot inherit another mission's ready flag"),
		FTutorialMissionRestoreRules::ShouldRestoreClaimReady(
			true, RewardMission, TEXT("M6_LaunchFirstProject"), /*bManualClaim=*/false));
	TestFalse(TEXT("A completed chain cannot restore claim-ready without an active mission"),
		FTutorialMissionRestoreRules::ShouldRestoreClaimReady(
			true, NAME_None, NAME_None, /*bManualClaim=*/false));
	TestFalse(TEXT("An unfinished final mission remains unfinished after restore"),
		FTutorialMissionRestoreRules::ShouldRestoreClaimReady(
			false, RewardMission, RewardMission, /*bManualClaim=*/true));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
