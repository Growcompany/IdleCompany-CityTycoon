#include "Misc/AutomationTest.h"
#include "Data/TutorialEnhancementGateRules.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRTutorialEnhancementGateRulesTest,
	"CGR.Building.Enhancement.TutorialAuthorityGate",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRTutorialEnhancementGateRulesTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("튜토리얼 중에는 빌드업만 구매 가능"),
		FTutorialEnhancementGateRules::IsPurchaseAllowed(
			/*bTutorialCompleted=*/false,
			EBuildingEnhancementType::BuildingFloor,
			/*bUnlockedByTier=*/true,
			/*bIsKeystone=*/false));
	TestFalse(TEXT("튜토리얼 중에는 T1 Money 강화도 차단"),
		FTutorialEnhancementGateRules::IsPurchaseAllowed(
			/*bTutorialCompleted=*/false,
			EBuildingEnhancementType::VaultCapacity,
			/*bUnlockedByTier=*/true,
			/*bIsKeystone=*/false));
	TestFalse(TEXT("튜토리얼 중에는 키스톤 영향력도 차단"),
		FTutorialEnhancementGateRules::IsPurchaseAllowed(
			/*bTutorialCompleted=*/false,
			EBuildingEnhancementType::KeystoneAuraPower,
			/*bUnlockedByTier=*/true,
			/*bIsKeystone=*/true));

	TestTrue(TEXT("튜토리얼 완료 후 티어 해금 강화 허용"),
		FTutorialEnhancementGateRules::IsPurchaseAllowed(
			/*bTutorialCompleted=*/true,
			EBuildingEnhancementType::MarketingPower,
			/*bUnlockedByTier=*/true,
			/*bIsKeystone=*/false));
	TestFalse(TEXT("튜토리얼 완료 후에도 티어 미해금 강화 차단"),
		FTutorialEnhancementGateRules::IsPurchaseAllowed(
			/*bTutorialCompleted=*/true,
			EBuildingEnhancementType::ProjectGrade,
			/*bUnlockedByTier=*/false,
			/*bIsKeystone=*/false));
	TestTrue(TEXT("완료한 키스톤은 전용 영향력 강화 허용"),
		FTutorialEnhancementGateRules::IsPurchaseAllowed(
			/*bTutorialCompleted=*/true,
			EBuildingEnhancementType::KeystoneAuraPower,
			/*bUnlockedByTier=*/false,
			/*bIsKeystone=*/true));
	TestFalse(TEXT("일반 건물은 키스톤 전용 강화를 직접 호출해도 차단"),
		FTutorialEnhancementGateRules::IsPurchaseAllowed(
			/*bTutorialCompleted=*/true,
			EBuildingEnhancementType::KeystoneAuraPower,
			/*bUnlockedByTier=*/true,
			/*bIsKeystone=*/false));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
