#include "Misc/AutomationTest.h"
#include "UI/Panel/BuildingEnhancementLockPolicy.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRBuildingEnhancementLockPolicyTest,
	"CGR.UI.BuildingManage.EnhancementLockPolicy",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRBuildingEnhancementLockPolicyTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("튜토리얼 중 비-빌드업은 튜토리얼 잠금 사유를 우선 표시"),
		CGR::UI::ResolveEnhancementLockReason(
			EBuildingEnhancementType::VaultCapacity,
			/*bTutorialCompleted=*/false,
			/*bUnlockedByTier=*/true,
			/*bAuthorityAllowsPurchase=*/false)
		== CGR::UI::EBuildingEnhancementLockReason::Tutorial);

	TestTrue(TEXT("튜토리얼 중에도 빌드업은 Actor가 허용하면 활성"),
		CGR::UI::ResolveEnhancementLockReason(
			EBuildingEnhancementType::BuildingFloor,
			/*bTutorialCompleted=*/false,
			/*bUnlockedByTier=*/true,
			/*bAuthorityAllowsPurchase=*/true)
		== CGR::UI::EBuildingEnhancementLockReason::None);

	TestTrue(TEXT("튜토리얼 완료 후 기존 티어 잠금 사유 유지"),
		CGR::UI::ResolveEnhancementLockReason(
			EBuildingEnhancementType::ProjectGrade,
			/*bTutorialCompleted=*/true,
			/*bUnlockedByTier=*/false,
			/*bAuthorityAllowsPurchase=*/false)
		== CGR::UI::EBuildingEnhancementLockReason::Tier);

	TestTrue(TEXT("튜토리얼과 티어를 통과해도 Actor 거부는 잠금"),
		CGR::UI::ResolveEnhancementLockReason(
			EBuildingEnhancementType::MarketingPower,
			/*bTutorialCompleted=*/true,
			/*bUnlockedByTier=*/true,
			/*bAuthorityAllowsPurchase=*/false)
		== CGR::UI::EBuildingEnhancementLockReason::Authority);

	TestTrue(TEXT("모든 게이트 통과 시 활성"),
		CGR::UI::ResolveEnhancementLockReason(
			EBuildingEnhancementType::EmployeeGrowth,
			/*bTutorialCompleted=*/true,
			/*bUnlockedByTier=*/true,
			/*bAuthorityAllowsPurchase=*/true)
		== CGR::UI::EBuildingEnhancementLockReason::None);

	const CGR::UI::EBuildingEnhancementLockReason BeforeCompletion =
		CGR::UI::ResolveEnhancementLockReason(
			EBuildingEnhancementType::VaultCapacity,
			/*bTutorialCompleted=*/false,
			/*bUnlockedByTier=*/true,
			/*bAuthorityAllowsPurchase=*/false);
	TestTrue(TEXT("열린 패널의 완료 전 슬롯은 잠금"),
		BeforeCompletion == CGR::UI::EBuildingEnhancementLockReason::Tutorial);

	TestTrue(TEXT("튜토리얼 완료 상태 변화는 열린 패널 잠금 재평가를 요구"),
		CGR::UI::ShouldRefreshEnhancementLocksOnTutorialStateChange(
			/*bPreviousTutorialCompleted=*/false,
			/*bCurrentTutorialCompleted=*/true));

	const CGR::UI::EBuildingEnhancementLockReason AfterCompletion =
		CGR::UI::ResolveEnhancementLockReason(
			EBuildingEnhancementType::VaultCapacity,
			/*bTutorialCompleted=*/true,
			/*bUnlockedByTier=*/true,
			/*bAuthorityAllowsPurchase=*/true);
	TestTrue(TEXT("완료 상태로 재평가한 슬롯은 해금"),
		AfterCompletion == CGR::UI::EBuildingEnhancementLockReason::None);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
