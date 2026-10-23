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

	TestEqual(TEXT("M1 observation has no hold hint"), ResolveGesture(EC::CollectBricks, BrickGuide::ObserveLaborers), EGestureHintKind::None);
	TestEqual(TEXT("M1 expansion observation has no hold hint"), ResolveGesture(EC::CollectBricks, BrickGuide::WatchExpansion), EGestureHintKind::None);
	TestEqual(TEXT("M1 boost = Hold"), ResolveGesture(EC::CollectBricks, BrickGuide::BoostLaborers), EGestureHintKind::Hold);
	TestEqual(TEXT("M1 collection has no hold hint"), ResolveGesture(EC::CollectBricks, BrickGuide::CollectBricks), EGestureHintKind::None);
	TestEqual(TEXT("M1 OpenFactory = None"), ResolveGesture(EC::CollectBricks, BrickGuide::OpenFactory), EGestureHintKind::None);
	TestEqual(TEXT("M2 PlaceBuilding = Drag"), ResolveGesture(EC::BuildFirstBuilding, BuildGuide::PlaceBuilding), EGestureHintKind::Drag);
	TestEqual(TEXT("M2 PressBuild = None"), ResolveGesture(EC::BuildFirstBuilding, BuildGuide::PressBuild), EGestureHintKind::None);
	TestEqual(TEXT("M4 Confirm = Drag"), ResolveGesture(EC::PlaceDesks, DeskGuide::Confirm), EGestureHintKind::Drag);
	TestEqual(TEXT("M4 PlaceMore = Drag"), ResolveGesture(EC::PlaceDesks, DeskGuide::PlaceMore), EGestureHintKind::Drag);
	TestEqual(TEXT("M4 OpenPlacement = None"), ResolveGesture(EC::PlaceDesks, DeskGuide::OpenPlacement), EGestureHintKind::None);
	TestEqual(TEXT("M4 PickDesk = None"), ResolveGesture(EC::PlaceDesks, DeskGuide::PickDesk), EGestureHintKind::None);
	TestEqual(TEXT("M5 = None"), ResolveGesture(EC::RecruitEmployees, RecruitGuide::PressPull), EGestureHintKind::None);
	TestEqual(TEXT("Company selection teaches Tap"), ResolveGesture(EC::EnterOffice, EnterOfficeGuide::ClickBuilding), EGestureHintKind::Tap);
	TestEqual(TEXT("Office entry button has no world Tap"), ResolveGesture(EC::EnterOffice, EnterOfficeGuide::PressEnter), EGestureHintKind::None);

	TestEqual(TEXT("Factory boost anchor"), ResolveAnchor(EC::CollectBricks, BrickGuide::BoostLaborers), EGestureAnchorKind::BrickFactory);
	TestEqual(TEXT("Observation has no anchor"), ResolveAnchor(EC::CollectBricks, BrickGuide::ObserveLaborers), EGestureAnchorKind::None);
	TestEqual(TEXT("Collection has no anchor"), ResolveAnchor(EC::CollectBricks, BrickGuide::CollectBricks), EGestureAnchorKind::None);
	TestEqual(TEXT("Preview anchor"), ResolveAnchor(EC::PlaceDesks, DeskGuide::PlaceMore), EGestureAnchorKind::PlacementPreview);
	TestEqual(TEXT("No anchor"), ResolveAnchor(EC::EnterOffice, EnterOfficeGuide::PressEnter), EGestureAnchorKind::None);
	TestEqual(TEXT("Company Tap follows the spotlight actor"), ResolveAnchor(EC::EnterOffice, EnterOfficeGuide::ClickBuilding), EGestureAnchorKind::SpotlightActor);

	TestEqual(TEXT("Observation keeps factory spotlight dim"), ResolveDimScale(EC::CollectBricks, BrickGuide::ObserveLaborers), 1.f);
	TestEqual(TEXT("Expansion observation keeps factory spotlight dim"), ResolveDimScale(EC::CollectBricks, BrickGuide::WatchExpansion), 1.f);
	TestEqual(TEXT("Boost keeps dim"), ResolveDimScale(EC::CollectBricks, BrickGuide::BoostLaborers), 1.f);
	TestEqual(TEXT("Collection is free"), ResolveDimScale(EC::CollectBricks, BrickGuide::CollectBricks), 0.f);
	TestEqual(TEXT("Placement dim 0"), ResolveDimScale(EC::BuildFirstBuilding, BuildGuide::PlaceBuilding), 0.f);
	TestEqual(TEXT("Desk dim 0"), ResolveDimScale(EC::PlaceDesks, DeskGuide::Confirm), 0.f);
	TestEqual(TEXT("Desk catalog keeps dim policy"), ResolveDimScale(EC::PlaceDesks, DeskGuide::PickDesk), 1.f);
	TestEqual(TEXT("M6 open board keeps dim"), ResolveDimScale(EC::LaunchFirstInHouseProject, InHouseGuide::OpenBoard), 1.f);
	TestEqual(TEXT("M6 pick uses ring only"), ResolveDimScale(EC::LaunchFirstInHouseProject, InHouseGuide::PickInHouse), 0.f);
	TestEqual(TEXT("M6 watch uses ring only"), ResolveDimScale(EC::LaunchFirstInHouseProject, InHouseGuide::WatchStrip), 0.f);
	TestEqual(TEXT("M6 launch uses ring only"), ResolveDimScale(EC::LaunchFirstInHouseProject, InHouseGuide::PressLaunch), 0.f);
	TestEqual(TEXT("Other dim 1"), ResolveDimScale(EC::RecruitEmployees, RecruitGuide::PressPull), 1.f);
	return true;
}

#endif
