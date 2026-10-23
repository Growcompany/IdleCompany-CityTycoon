#include "Misc/AutomationTest.h"
#include "Data/TutorialEnhancementGateRules.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRTutorialEnhancementGateRulesTest,
	"CGR.Building.Enhancement.TutorialAuthorityGate",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRTutorialEnhancementGateRulesTest::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("M1~M7 진행 중(도출형 아님)에는 회사 강화 잠금"),
		FTutorialEnhancementGateRules::IsCompanyEnhancementUnlocked(
			/*bTutorialCompleted=*/false,
			/*bDerivedGuideActive=*/false));
	TestTrue(TEXT("M7 수령 뒤 도출형 체인(M8~M31) 진행 중에는 회사 강화 해금"),
		FTutorialEnhancementGateRules::IsCompanyEnhancementUnlocked(
			/*bTutorialCompleted=*/false,
			/*bDerivedGuideActive=*/true));
	TestTrue(TEXT("튜토리얼 완료 후에는 체인이 없어도 해금"),
		FTutorialEnhancementGateRules::IsCompanyEnhancementUnlocked(
			/*bTutorialCompleted=*/true,
			/*bDerivedGuideActive=*/false));

	TestTrue(TEXT("잠금 중에도 빌드업은 구매 가능"),
		FTutorialEnhancementGateRules::IsPurchaseAllowed(
			/*bCompanyEnhancementUnlocked=*/false,
			EBuildingEnhancementType::BuildingFloor,
			/*bUnlockedByTier=*/true));
	TestFalse(TEXT("잠금 중에는 T1 Money 강화도 차단"),
		FTutorialEnhancementGateRules::IsPurchaseAllowed(
			/*bCompanyEnhancementUnlocked=*/false,
			EBuildingEnhancementType::VaultCapacity,
			/*bUnlockedByTier=*/true));

	TestTrue(TEXT("해금 후 티어 해금 강화 허용"),
		FTutorialEnhancementGateRules::IsPurchaseAllowed(
			/*bCompanyEnhancementUnlocked=*/true,
			EBuildingEnhancementType::MarketingPower,
			/*bUnlockedByTier=*/true));
	TestFalse(TEXT("해금 후에도 티어 미해금 강화 차단"),
		FTutorialEnhancementGateRules::IsPurchaseAllowed(
			/*bCompanyEnhancementUnlocked=*/true,
			EBuildingEnhancementType::ProjectGrade,
			/*bUnlockedByTier=*/false));
	TestTrue(TEXT("M10 자금 강화 — 도출형 체인 중 티어 해금 강화 허용"),
		FTutorialEnhancementGateRules::IsPurchaseAllowed(
			FTutorialEnhancementGateRules::IsCompanyEnhancementUnlocked(false, true),
			EBuildingEnhancementType::VaultCapacity,
			/*bUnlockedByTier=*/true));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
