#include "Misc/AutomationTest.h"
#include "Manager/GuideGestureRules.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGuideGestureRulesTest,
	"CGR.Tutorial.GuideGestureRules",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGuideGestureRulesTest::RunTest(const FString& Parameters)
{
	using namespace GuideGestureRules;
	using EC = EMissionConditionType;

	TestEqual(TEXT("M1 TapFactory = Hold"), ResolveGesture(EC::CollectBricks, BrickGuide::TapFactory), EGestureHintKind::Hold);
	TestEqual(TEXT("M1 Grind = Hold"), ResolveGesture(EC::CollectBricks, BrickGuide::Grind), EGestureHintKind::Hold);
	TestEqual(TEXT("M1 OpenFactory = None"), ResolveGesture(EC::CollectBricks, BrickGuide::OpenFactory), EGestureHintKind::None);
	TestEqual(TEXT("M2 PlaceBuilding = Drag"), ResolveGesture(EC::BuildFirstBuilding, BuildGuide::PlaceBuilding), EGestureHintKind::Drag);
	TestEqual(TEXT("M2 PressBuild = None"), ResolveGesture(EC::BuildFirstBuilding, BuildGuide::PressBuild), EGestureHintKind::None);
	TestEqual(TEXT("M4 Confirm = Drag"), ResolveGesture(EC::PlaceDesks, DeskGuide::Confirm), EGestureHintKind::Drag);
	TestEqual(TEXT("M4 PlaceMore = Drag"), ResolveGesture(EC::PlaceDesks, DeskGuide::PlaceMore), EGestureHintKind::Drag);
	TestEqual(TEXT("M4 OpenPlacement = None"), ResolveGesture(EC::PlaceDesks, DeskGuide::OpenPlacement), EGestureHintKind::None);
	TestEqual(TEXT("M5 = None"), ResolveGesture(EC::RecruitEmployees, RecruitGuide::PressPull), EGestureHintKind::None);

	TestEqual(TEXT("Factory anchor"), ResolveAnchor(EC::CollectBricks, BrickGuide::Grind), EGestureAnchorKind::BrickFactory);
	TestEqual(TEXT("Preview anchor"), ResolveAnchor(EC::PlaceDesks, DeskGuide::PlaceMore), EGestureAnchorKind::PlacementPreview);
	TestEqual(TEXT("No anchor"), ResolveAnchor(EC::EnterOffice, EnterOfficeGuide::PressEnter), EGestureAnchorKind::None);

	TestEqual(TEXT("TapFactory keeps dim"), ResolveDimScale(EC::CollectBricks, BrickGuide::TapFactory), 1.f);
	TestEqual(TEXT("Grind dim 0"), ResolveDimScale(EC::CollectBricks, BrickGuide::Grind), 0.f);
	TestEqual(TEXT("Placement dim 0"), ResolveDimScale(EC::BuildFirstBuilding, BuildGuide::PlaceBuilding), 0.f);
	TestEqual(TEXT("Desk dim 0"), ResolveDimScale(EC::PlaceDesks, DeskGuide::Confirm), 0.f);
	TestEqual(TEXT("Other dim 1"), ResolveDimScale(EC::LaunchFirstInHouseProject, InHouseGuide::WatchStrip), 1.f);
	return true;
}

#endif
