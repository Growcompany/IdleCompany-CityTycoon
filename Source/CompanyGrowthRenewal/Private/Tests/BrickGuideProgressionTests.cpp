#include "Misc/AutomationTest.h"
#include "Manager/MissionGuidePhases.h"
#include "Data/FactorySaveData.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRBrickGuideProgressionTest,
	"CGR.Tutorial.BrickGuideProgression",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRBrickGuideProgressionTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Passive collection cannot skip observation before first delivery"),
		BrickGuide::PhaseAfterDelivery(BrickGuide::ObserveLaborers, 0, 2, 5, 1), BrickGuide::ObserveLaborers);
	TestEqual(TEXT("First delivery guides the factory panel"),
		BrickGuide::PhaseAfterDelivery(BrickGuide::ObserveLaborers, 3, 2, 5, 1), BrickGuide::OpenFactory);
	TestEqual(TEXT("Earlier laborer purchase still waits for first delivery"),
		BrickGuide::PhaseAfterDelivery(BrickGuide::ObserveLaborers, 0, 5, 5, 1), BrickGuide::ObserveLaborers);
	TestEqual(TEXT("Early purchase cannot skip observation"),
		BrickGuide::PhaseAfterPurchase(BrickGuide::ObserveLaborers, EFactoryUpgradeType::LaborerCount, 5, 5), BrickGuide::ObserveLaborers);
	TestEqual(TEXT("Partial crew reload returns to factory opening after delivery"),
		BrickGuide::PhaseAfterDelivery(BrickGuide::ObserveLaborers, 3, 3, 5, 1), BrickGuide::OpenFactory);
	TestEqual(TEXT("Partial crew reload stays in laborer lesson"),
		BrickGuide::PhaseAfterDelivery(BrickGuide::AddLaborer, 100, 4, 5, 1), BrickGuide::AddLaborer);
	TestEqual(TEXT("Full crew reload resumes at expansion after delivery"),
		BrickGuide::PhaseAfterDelivery(BrickGuide::ObserveLaborers, 3, 5, 5, 1), BrickGuide::ExpandFactory);
	TestEqual(TEXT("Late factory restore cannot strand a full crew panel"),
		BrickGuide::PhaseAfterDelivery(BrickGuide::AddLaborer, 100, 5, 5, 1), BrickGuide::ExpandFactory);
	TestEqual(TEXT("Bulk purchase over capacity counts as a full crew on reload"),
		BrickGuide::PhaseAfterDelivery(BrickGuide::AddLaborer, 100, 7, 5, 1), BrickGuide::ExpandFactory);
	TestEqual(TEXT("Invalid capacity cannot advance laborer lesson on reload"),
		BrickGuide::PhaseAfterDelivery(BrickGuide::AddLaborer, 100, 5, 0, 1), BrickGuide::AddLaborer);
	TestEqual(TEXT("Saved factory expansion resumes at boost"),
		BrickGuide::PhaseAfterDelivery(BrickGuide::ObserveLaborers, 100, 2, 5, 2), BrickGuide::BoostLaborers);
	TestEqual(TEXT("Speed upgrade cannot advance laborer lesson"),
		BrickGuide::PhaseAfterPurchase(BrickGuide::AddLaborer, EFactoryUpgradeType::HoldProductionSpeed, 5, 5), BrickGuide::AddLaborer);
	TestEqual(TEXT("Output upgrade cannot advance laborer lesson"),
		BrickGuide::PhaseAfterPurchase(BrickGuide::AddLaborer, EFactoryUpgradeType::HoldProductionAmount, 5, 5), BrickGuide::AddLaborer);
	TestEqual(TEXT("First partial hire stays in laborer lesson"),
		BrickGuide::PhaseAfterPurchase(BrickGuide::AddLaborer, EFactoryUpgradeType::LaborerCount, 3, 5), BrickGuide::AddLaborer);
	TestEqual(TEXT("Second partial hire stays in laborer lesson"),
		BrickGuide::PhaseAfterPurchase(BrickGuide::AddLaborer, EFactoryUpgradeType::LaborerCount, 4, 5), BrickGuide::AddLaborer);
	TestEqual(TEXT("Hire reaching capacity advances to expansion"),
		BrickGuide::PhaseAfterPurchase(BrickGuide::AddLaborer, EFactoryUpgradeType::LaborerCount, 5, 5), BrickGuide::ExpandFactory);
	TestEqual(TEXT("Bulk hire over capacity advances to expansion"),
		BrickGuide::PhaseAfterPurchase(BrickGuide::AddLaborer, EFactoryUpgradeType::LaborerCount, 7, 5), BrickGuide::ExpandFactory);
	TestEqual(TEXT("Invalid capacity cannot advance laborer lesson after purchase"),
		BrickGuide::PhaseAfterPurchase(BrickGuide::AddLaborer, EFactoryUpgradeType::LaborerCount, 5, 0), BrickGuide::AddLaborer);
	TestEqual(TEXT("Unrelated upgrade cannot advance expansion lesson"),
		BrickGuide::PhaseAfterPurchase(BrickGuide::ExpandFactory, EFactoryUpgradeType::HoldProductionSpeed, 5, 5), BrickGuide::ExpandFactory);
	TestEqual(TEXT("Successful expansion purchase starts observation"),
		BrickGuide::PhaseAfterPurchase(BrickGuide::ExpandFactory, EFactoryUpgradeType::FactoryExpansion, 5, 5), BrickGuide::WatchExpansion);
	TestEqual(TEXT("Expansion appearance remains visible before boost guidance"),
		BrickGuide::PhaseAfterExpansionWatch(BrickGuide::WatchExpansion, 1.9), BrickGuide::WatchExpansion);
	TestEqual(TEXT("Expansion observation ends after two seconds"),
		BrickGuide::PhaseAfterExpansionWatch(BrickGuide::WatchExpansion, 2.0), BrickGuide::BoostLaborers);
	TestFalse(TEXT("Passive brick target cannot skip onboarding"),
		BrickGuide::CanComplete(BrickGuide::ObserveLaborers, 100, 100));
	TestFalse(TEXT("Brick target cannot complete before boost lesson"),
		BrickGuide::CanComplete(BrickGuide::BoostLaborers, 100, 100));
	TestEqual(TEXT("Brief boost stays in the hold lesson"),
		BrickGuide::PhaseAfterBoost(BrickGuide::BoostLaborers, 0.2), BrickGuide::BoostLaborers);
	TestEqual(TEXT("Hold just below threshold stays in the hold lesson"),
		BrickGuide::PhaseAfterBoost(BrickGuide::BoostLaborers, 1.24), BrickGuide::BoostLaborers);
	TestEqual(TEXT("Continuous hold enters free collection at the threshold"),
		BrickGuide::PhaseAfterBoost(BrickGuide::BoostLaborers, 1.25), BrickGuide::CollectBricks);
	TestEqual(TEXT("Other phases do not advance from a long hold"),
		BrickGuide::PhaseAfterBoost(BrickGuide::WatchExpansion, 2.0), BrickGuide::WatchExpansion);
	TestTrue(TEXT("Brick target completes after actual boost"),
		BrickGuide::CanComplete(BrickGuide::CollectBricks, 100, 100));
	return true;
}

#endif
