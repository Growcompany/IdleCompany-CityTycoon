#include "Misc/AutomationTest.h"
#include "Manager/ContentGuideStep.h"
#include "Manager/MissionGuidePhases.h"
#include "Data/TutorialDerivedMissionRules.h"
#include "Data/EntryPopupRules.h"
#include "Data/SkipCostRules.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRTutorialDerivedStepsTest,
	"CGR.Tutorial.DerivedSteps",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace
{
	void TestPromptFlow(FAutomationTestBase& Test)
	{
		using namespace CGRGoalGuideStep;
		Test.TestTrue(TEXT("창이 없으면 어느 조건이든 흐름 안"),
			IsPromptInFlow(EMissionConditionType::UpgradeFactoryStats, EGuidePromptKind::None));
		Test.TestFalse(TEXT("M8 공장 강화 중 메뉴 창은 흐름 밖"),
			IsPromptInFlow(EMissionConditionType::UpgradeFactoryStats, EGuidePromptKind::MenuPanel));
		Test.TestTrue(TEXT("M11 특성 뽑기 창은 흐름 안"),
			IsPromptInFlow(EMissionConditionType::EquipFirstTrait, EGuidePromptKind::CompanyGacha));
		Test.TestFalse(TEXT("M11 중 상점 창은 흐름 밖"),
			IsPromptInFlow(EMissionConditionType::EquipFirstTrait, EGuidePromptKind::ShopPanel));
		Test.TestTrue(TEXT("M18 장비 뽑기 창은 흐름 안"),
			IsPromptInFlow(EMissionConditionType::EquipFirstEquipment, EGuidePromptKind::EquipmentGacha));
		Test.TestTrue(TEXT("별 강화 미션에서는 별 강화 창이 흐름 안"),
			IsPromptInFlow(EMissionConditionType::EnhanceFirstEmployee, EGuidePromptKind::StarforceModal));
		Test.TestFalse(TEXT("다음 미션까지 남은 별 강화 창은 먼저 닫는다"),
			IsPromptInFlow(EMissionConditionType::EquipFirstEquipment, EGuidePromptKind::StarforceModal));
		Test.TestFalse(TEXT("M18 중 도시 특성 뽑기 창은 흐름 밖"),
			IsPromptInFlow(EMissionConditionType::EquipFirstEquipment, EGuidePromptKind::CompanyGacha));
		Test.TestTrue(TEXT("M22 건설 모달은 흐름 안"),
			IsPromptInFlow(EMissionConditionType::BuildSecondBuilding, EGuidePromptKind::BuildModal));
		Test.TestTrue(TEXT("M30 IT 건설 모달은 흐름 안"),
			IsPromptInFlow(EMissionConditionType::BuildIndustry, EGuidePromptKind::BuildModal));
		Test.TestFalse(TEXT("M30 시작 때 남은 본사 창은 흐름 밖(닫기로 안내)"),
			IsPromptInFlow(EMissionConditionType::BuildIndustry, EGuidePromptKind::HQPanel));
		Test.TestFalse(TEXT("M30 시작 때 남은 레벨업 축하는 흐름 밖(닫기로 안내)"),
			IsPromptInFlow(EMissionConditionType::BuildIndustry, EGuidePromptKind::HQLevelUpCelebration));
		Test.TestTrue(TEXT("M25 인수 창은 흐름 안"),
			IsPromptInFlow(EMissionConditionType::AcquireFirstCompany, EGuidePromptKind::CompanyInfo));
		Test.TestFalse(TEXT("M25 중 확인 창은 흐름 밖"),
			IsPromptInFlow(EMissionConditionType::AcquireFirstCompany, EGuidePromptKind::ConfirmCancel));
		Test.TestTrue(TEXT("M26 인수 결과 창은 흐름 안"),
			IsPromptInFlow(EMissionConditionType::SkipCompanyRecovery, EGuidePromptKind::CompanyInfo));
		Test.TestTrue(TEXT("M26 즉시 완료 확인 창은 흐름 안"),
			IsPromptInFlow(EMissionConditionType::SkipCompanyRecovery, EGuidePromptKind::ConfirmCancel));
		Test.TestTrue(TEXT("M28 부지 창(확인 창)은 흐름 안"),
			IsPromptInFlow(EMissionConditionType::ExplainPlot, EGuidePromptKind::ConfirmCancel));
		Test.TestTrue(TEXT("M29 본사 창은 흐름 안"),
			IsPromptInFlow(EMissionConditionType::ReachHQLevel, EGuidePromptKind::HQPanel));
		Test.TestTrue(TEXT("M29 레벨업 축하는 흐름 안(관찰 걸음)"),
			IsPromptInFlow(EMissionConditionType::ReachHQLevel, EGuidePromptKind::HQLevelUpCelebration));
		Test.TestFalse(TEXT("M29 중 메뉴 창은 흐름 밖"),
			IsPromptInFlow(EMissionConditionType::ReachHQLevel, EGuidePromptKind::MenuPanel));
		Test.TestTrue(TEXT("M31 메뉴 창은 흐름 안"),
			IsPromptInFlow(EMissionConditionType::MenuTour, EGuidePromptKind::MenuPanel));
		Test.TestFalse(TEXT("M33 상점 창은 흐름 밖"),
			IsPromptInFlow(EMissionConditionType::MenuTour, EGuidePromptKind::ShopPanel));
		Test.TestFalse(TEXT("M31 중 본사 창은 흐름 밖"),
			IsPromptInFlow(EMissionConditionType::MenuTour, EGuidePromptKind::HQPanel));
		Test.TestTrue(TEXT("M14·M24 채용 창(기타 종류)은 흐름 안"),
			IsPromptInFlow(EMissionConditionType::RecruitGoalDiscipline, EGuidePromptKind::Other));
		Test.TestFalse(TEXT("M14·M24 중 도시 인수 창은 흐름 밖"),
			IsPromptInFlow(EMissionConditionType::RecruitGoalDiscipline, EGuidePromptKind::CompanyInfo));
		Test.TestFalse(TEXT("M13·M23 책상 배치 중 확인 창은 흐름 밖"),
			IsPromptInFlow(EMissionConditionType::PlaceSecondDesk, EGuidePromptKind::ConfirmCancel));
	}

	void TestCompanyGrowthSteps(FAutomationTestBase& Test)
	{
		using namespace CGRGoalGuideStep;
		Test.TestEqual(TEXT("M8 사무실이면 나가기"), ResolveFactoryStatStep(true, true, false, false), FactoryStatGuide::ExitOffice);
		Test.TestEqual(TEXT("M8 패널이 닫혀 있으면 도크 [공장]"), ResolveFactoryStatStep(false, false, false, false), FactoryStatGuide::OpenFactory);
		Test.TestEqual(TEXT("M8 작업속도가 모자라면 작업속도"), ResolveFactoryStatStep(false, true, false, false), FactoryStatGuide::UpgradeSpeed);
		Test.TestEqual(TEXT("M8 작업속도를 채웠으면 생산능력"), ResolveFactoryStatStep(false, true, true, false), FactoryStatGuide::UpgradeCapacity);
		Test.TestEqual(TEXT("M2 둘 다 올렸으면 패널 닫기"), ResolveFactoryStatStep(false, true, true, true), FactoryStatGuide::ClosePanel);

		Test.TestEqual(TEXT("M9 다른 회사 패널은 닫기부터"),
			ResolveForcedRaiseFloorStep(false, true, false, false, false), RaiseFloorGuide::CloseOtherPanel);
		Test.TestEqual(TEXT("M9 패널이 없으면 회사 탭"),
			ResolveForcedRaiseFloorStep(false, false, false, false, false), RaiseFloorGuide::TapCompany);
		Test.TestEqual(TEXT("M9 패널만 열렸으면 [강화] 탭"),
			ResolveForcedRaiseFloorStep(false, false, true, false, false), RaiseFloorGuide::OpenEnhanceTab);
		Test.TestEqual(TEXT("M9 강화 탭이면 [빌드업]"),
			ResolveForcedRaiseFloorStep(false, false, true, true, false), RaiseFloorGuide::PressBuildUp);
		Test.TestEqual(TEXT("M9 층이 오른 뒤에는 관찰 걸음"),
			ResolveForcedRaiseFloorStep(false, false, true, true, true), RaiseFloorGuide::WatchFloorRise);
		Test.TestEqual(TEXT("M9 사무실이면 나가기가 먼저"),
			ResolveForcedRaiseFloorStep(true, true, true, true, true), RaiseFloorGuide::ExitOffice);
		Test.TestTrue(TEXT("M9 관찰 걸음은 CSV 줄 안"), RaiseFloorGuide::WatchFloorRise < GetDerivedStepCount(EMissionConditionType::RaiseBuildingFloor));

		Test.TestEqual(TEXT("M10 회사 탭"), ResolveEnhanceStatStep(false, false, false), EnhanceStatGuide::TapCompany);
		Test.TestEqual(TEXT("M10 [강화] 탭"), ResolveEnhanceStatStep(false, true, false), EnhanceStatGuide::OpenEnhanceTab);
		Test.TestEqual(TEXT("M10 자금 강화 버튼"), ResolveEnhanceStatStep(false, true, true), EnhanceStatGuide::PressEnhance);
	}

	void TestCompanyGachaSteps(FAutomationTestBase& Test)
	{
		using namespace CGRGoalGuideStep;
		FCompanyGachaGuideFlags Flags;
		Test.TestEqual(TEXT("뽑은 것이 없으면 도크 [뽑기]"), ResolveCompanyGachaStep(Flags), TraitGuide::OpenGacha);

		Flags.bGachaOpen = true;
		Flags.bWrongCategory = true;
		Test.TestEqual(TEXT("다른 분류면 분류 탭"), ResolveCompanyGachaStep(Flags), TraitGuide::PickCategory);
		Flags.bWrongCategory = false;
		Test.TestEqual(TEXT("맞는 분류면 [뽑기]"), ResolveCompanyGachaStep(Flags), TraitGuide::PressPull);

		Flags.bRevealOpen = true;
		Flags.bOwnsResult = true;
		Test.TestEqual(TEXT("연출이 떠 있으면 [확인]"), ResolveCompanyGachaStep(Flags), TraitGuide::ConfirmReveal);
		Flags.bRevealOpen = false;
		Test.TestEqual(TEXT("연출을 닫고 뽑기 창이 남았으면 [닫기]"), ResolveCompanyGachaStep(Flags), TraitGuide::CloseGacha);

		Flags.bGachaOpen = false;
		Test.TestEqual(TEXT("뽑기 창을 닫았으면 회사 탭"), ResolveCompanyGachaStep(Flags), TraitGuide::OpenTraitTab);
		Flags.bCompanyPanelOpen = true;
		Test.TestEqual(TEXT("관리 패널만 열렸으면 [특성] 탭"), ResolveCompanyGachaStep(Flags), TraitGuide::OpenTraitTab);
		Flags.bTargetTabActive = true;
		Test.TestEqual(TEXT("특성 탭이면 카드·[장착]"), ResolveCompanyGachaStep(Flags), TraitGuide::EquipCard);

		Flags.bInOffice = true;
		Test.TestEqual(TEXT("사무실이면 나가기가 먼저"), ResolveCompanyGachaStep(Flags), TraitGuide::ExitOffice);

		Test.TestEqual(TEXT("스킨도 같은 번호 ― [닫기] 걸음"), SkinGuide::CloseGacha, TraitGuide::CloseGacha);
		Test.TestEqual(TEXT("[닫기]는 연출 [확인] 바로 뒤"), TraitGuide::CloseGacha, TraitGuide::ConfirmReveal + 1);
		Test.TestEqual(TEXT("특성 걸음 수 = 8"), GetDerivedStepCount(EMissionConditionType::EquipFirstTrait), 8);
		Test.TestEqual(TEXT("스킨 걸음 수 = 8"), GetDerivedStepCount(EMissionConditionType::EquipFirstSkin), 8);
	}

	void TestEquipmentSteps(FAutomationTestBase& Test)
	{
		using namespace CGRGoalGuideStep;
		FEquipmentGuideFlags Flags;
		Test.TestEqual(TEXT("다른 곳이면 사무실 입장"), ResolveEquipmentStep(Flags), EquipmentGuide::EnterOffice);

		Flags.bInTargetOffice = true;
		Flags.bPanelGachaReachable = true;
		Test.TestEqual(TEXT("패널 안 뽑기가 가능하면 도크 [직원]"), ResolveEquipmentStep(Flags), EquipmentGuide::OpenEmployee);
		Flags.bEmployeePanelOpen = true;
		Test.TestEqual(TEXT("장비 탭이 아니면 탭부터(슬롯 걸음)"), ResolveEquipmentStep(Flags), EquipmentGuide::PickSlot);
		Flags.bEquipmentTabActive = true;
		Test.TestEqual(TEXT("빈 칸을 눌러야 뽑기 진입이 뜬다"), ResolveEquipmentStep(Flags), EquipmentGuide::PickSlot);
		Flags.bPanelGachaVisible = true;
		Test.TestEqual(TEXT("패널 [장비 뽑기]"), ResolveEquipmentStep(Flags), EquipmentGuide::OpenGacha);

		Flags.bGachaOpen = true;
		Test.TestEqual(TEXT("뽑기 창이면 [뽑기]"), ResolveEquipmentStep(Flags), EquipmentGuide::PressPull);
		Flags.bRevealOpen = true;
		Flags.bOwnsUnequipped = true;
		Test.TestEqual(TEXT("연출이면 [확인]"), ResolveEquipmentStep(Flags), EquipmentGuide::ConfirmReveal);
		Flags.bRevealOpen = false;
		Test.TestEqual(TEXT("뽑은 뒤 뽑기 창이 남았으면 [닫기]"), ResolveEquipmentStep(Flags), EquipmentGuide::CloseGacha);
		Flags.bGachaOpen = false;
		Test.TestEqual(TEXT("직원 패널로 돌아오면 장착"), ResolveEquipmentStep(Flags), EquipmentGuide::Equip);

		FEquipmentGuideFlags NoPanelRoute;
		NoPanelRoute.bInTargetOffice = true;
		Test.TestEqual(TEXT("패널 안 진입이 없으면 도크 [장비]로 바로"), ResolveEquipmentStep(NoPanelRoute), EquipmentGuide::OpenGacha);
		NoPanelRoute.bEmployeePanelOpen = true;
		NoPanelRoute.bEquipmentTabActive = true;
		Test.TestEqual(TEXT("패널이 덮고 진입도 없으면 뽑기 걸음(대상 없음)"), ResolveEquipmentStep(NoPanelRoute), EquipmentGuide::OpenGacha);

		Test.TestEqual(TEXT("[닫기]는 연출 [확인] 바로 뒤"), EquipmentGuide::CloseGacha, EquipmentGuide::ConfirmReveal + 1);
		Test.TestEqual(TEXT("장비 걸음 수 = 8"), GetDerivedStepCount(EMissionConditionType::EquipFirstEquipment), 8);
	}

	void TestPlacementGating(FAutomationTestBase& Test)
	{
		using namespace CGRGoalGuideStep;
		using FRules = FTutorialDerivedMissionRules;
		Test.TestEqual(TEXT("M13 다른 곳이면 사무실 입장"), ResolveOfficePlacementStep(false, false, true), SecondDeskGuide::EnterOffice);
		Test.TestEqual(TEXT("M13 도크 [책상]"), ResolveOfficePlacementStep(true, false, false), SecondDeskGuide::OpenPlacement);
		Test.TestEqual(TEXT("M13 카탈로그"), ResolveOfficePlacementStep(true, true, false), SecondDeskGuide::PickDesk);
		Test.TestEqual(TEXT("M13 배치가 켜졌을 때만 [배치]"), ResolveOfficePlacementStep(true, false, true), SecondDeskGuide::Confirm);
		Test.TestEqual(TEXT("M20 그림도 같은 번호"), ResolveOfficePlacementStep(true, true, false), PaintingGuide::PickPainting);

		Test.TestEqual(TEXT("M23 두 번째 회사 사무실이 아니면 입장"), ResolveOfficePlacementStep(false, false, false), SecondDeskGuide::EnterOffice);

		Test.TestEqual(TEXT("M22 사무실이면 나가기"), ResolveSecondBuildingStep(true, false, false, false), SecondBuildingGuide::ExitOffice);
		Test.TestEqual(TEXT("M22 도크 [건설]"), ResolveSecondBuildingStep(false, false, false, false), SecondBuildingGuide::OpenBuild);
		Test.TestEqual(TEXT("M22 산업 타일"), ResolveSecondBuildingStep(false, true, true, false), SecondBuildingGuide::PickIndustry);
		Test.TestEqual(TEXT("M22 첫 카드"), ResolveSecondBuildingStep(false, true, false, false), SecondBuildingGuide::PickBuilding);
		Test.TestEqual(TEXT("M22 배치가 켜졌을 때만 [배치]"), ResolveSecondBuildingStep(false, false, false, true), SecondBuildingGuide::PlaceBuilding);
		// M30 ― 모달은 게임 타일을 자동 선택한다: IT 를 직접 누르기 전에는 카드로 넘어가지 않는다
		Test.TestEqual(TEXT("M30 모달이 열렸고 IT 를 아직 안 골랐으면 IT 타일"), ResolveSecondBuildingStep(false, true, true, false), SecondBuildingGuide::PickIndustry);
		Test.TestEqual(TEXT("M30 IT 를 골랐으면 첫 카드"), ResolveSecondBuildingStep(false, true, false, false), SecondBuildingGuide::PickBuilding);
		Test.TestEqual(TEXT("M30 걸음 수 = 두 번째 회사와 같은 5"), GetDerivedStepCount(EMissionConditionType::BuildIndustry),
			GetDerivedStepCount(EMissionConditionType::BuildSecondBuilding));

		// 게이트·딤 해제는 배치 확정 번호에 묶여 있다 ― 배치가 꺼진 상태에서는 어떤 조합도 그 번호를 내면 안 된다
		bool bLeaked = false;
		for (int32 Bits = 0; Bits < 8; ++Bits)
		{
			const bool bA = (Bits & 1) != 0;
			const bool bB = (Bits & 2) != 0;
			const bool bC = (Bits & 4) != 0;
			bLeaked |= FRules::IsPlacementConfirmStep(EMissionConditionType::PlaceSecondDesk, ResolveOfficePlacementStep(bA, bB, false));
			bLeaked |= FRules::IsPlacementConfirmStep(EMissionConditionType::PlaceFirstPainting, ResolveOfficePlacementStep(bA, bB, false));
			bLeaked |= FRules::IsPlacementConfirmStep(EMissionConditionType::BuildSecondBuilding, ResolveSecondBuildingStep(bA, bB, bC, false));
			bLeaked |= FRules::IsPlacementConfirmStep(EMissionConditionType::BuildIndustry, ResolveSecondBuildingStep(bA, bB, bC, false));
		}
		Test.TestFalse(TEXT("배치가 꺼져 있으면 배치 확정 걸음이 나오지 않는다"), bLeaked);
	}

	void TestCityAcquisitionSteps(FAutomationTestBase& Test)
	{
		using namespace CGRGoalGuideStep;
		Test.TestEqual(TEXT("M25 사무실이면 나가기"), ResolveAcquireStep(true, true), AcquireGuide::ExitOffice);
		Test.TestEqual(TEXT("M25 가장 싼 회사 탭"), ResolveAcquireStep(false, false), AcquireGuide::TapCompany);
		Test.TestEqual(TEXT("M25 그 회사 창이면 [인수]"), ResolveAcquireStep(false, true), AcquireGuide::PressAcquire);

		Test.TestEqual(TEXT("M28 인수 결과 창이 남았으면 결과 [확인] 걸음"),
			ResolveSkipRecoveryStep(false, true, false, false), SkipRecoveryGuide::ConfirmResult);
		Test.TestEqual(TEXT("M26 인수한 회사 탭"), ResolveSkipRecoveryStep(false, false, false, false), SkipRecoveryGuide::TapCompany);
		Test.TestEqual(TEXT("M26 관리 창이면 [즉시 완료]"), ResolveSkipRecoveryStep(false, false, true, false), SkipRecoveryGuide::PressSkip);
		Test.TestEqual(TEXT("M26 확인 창이면 [확인]"), ResolveSkipRecoveryStep(false, false, true, true), SkipRecoveryGuide::Confirm);

		Test.TestEqual(TEXT("M27 고갈 회사 탭"), ResolveDemolishStep(false, false, false, false), DemolishGuide::TapCompany);
		Test.TestEqual(TEXT("M27 관리 창이면 [철거]"), ResolveDemolishStep(false, false, true, false), DemolishGuide::PressDemolish);
		Test.TestEqual(TEXT("M27 확인 창이 있을 때만 [확인]"), ResolveDemolishStep(false, false, true, true), DemolishGuide::Confirm);
		Test.TestEqual(TEXT("M27 철거 뒤에는 관찰"), ResolveDemolishStep(false, true, false, false), DemolishGuide::WatchDemolish);
		Test.TestEqual(TEXT("M27 사무실이면 나가기"), ResolveDemolishStep(true, false, false, false), DemolishGuide::ExitOffice);

		Test.TestEqual(TEXT("M28 부지 탭"), ResolvePlotExplainStep(false, false, false), PlotExplainGuide::TapPlot);
		Test.TestEqual(TEXT("M28 부지 창이면 설명"), ResolvePlotExplainStep(false, true, false), PlotExplainGuide::Explain);
		Test.TestEqual(TEXT("M28 다 읽으면 [닫기]"), ResolvePlotExplainStep(false, true, true), PlotExplainGuide::CloseDialog);
	}

	// M29 ― 설명을 읽힌 뒤 [레벨업]을 실제로 누르고 축하 연출을 본다
	void TestHQLevelUpSteps(FAutomationTestBase& Test)
	{
		using namespace CGRGoalGuideStep;
		using FRules = FTutorialDerivedMissionRules;
		Test.TestEqual(TEXT("M29 사무실이면 나가기"), ResolveHQLevelUpStep(true, false, false, false, false), HQLevelUpGuide::ExitOffice);
		Test.TestEqual(TEXT("M29 도크 [본사]"), ResolveHQLevelUpStep(false, false, false, false, false), HQLevelUpGuide::OpenHQ);
		Test.TestEqual(TEXT("M29 [성장] 탭"), ResolveHQLevelUpStep(false, true, false, false, false), HQLevelUpGuide::OpenGrowthTab);
		Test.TestEqual(TEXT("M29 성장 탭이면 조건 설명부터"), ResolveHQLevelUpStep(false, true, true, false, false), HQLevelUpGuide::Explain);
		Test.TestEqual(TEXT("M29 설명을 읽었으면 [레벨업]"), ResolveHQLevelUpStep(false, true, true, true, false), HQLevelUpGuide::PressLevelUp);
		Test.TestEqual(TEXT("M29 설명 뒤 다른 탭으로 가면 [성장] 탭으로 돌아온다"), ResolveHQLevelUpStep(false, true, false, true, false), HQLevelUpGuide::OpenGrowthTab);
		Test.TestEqual(TEXT("M29 패널을 닫았다 열어도 설명은 다시 하지 않는다"), ResolveHQLevelUpStep(false, false, false, true, false), HQLevelUpGuide::OpenHQ);
		Test.TestEqual(TEXT("M29 레벨이 오르면 축하 관찰(축하 창이 패널을 덮어도)"), ResolveHQLevelUpStep(false, false, false, true, true), HQLevelUpGuide::WatchLevelUp);
		Test.TestEqual(TEXT("M29 레벨이 이미 올랐으면 사무실에서도 관찰"), ResolveHQLevelUpStep(true, false, false, false, true), HQLevelUpGuide::WatchLevelUp);

		// [레벨업]이 꺼져 있으면(조건 미달) 이 걸음이 구매 걸음이라 ShortfallLine 으로 바뀐다
		Test.TestTrue(TEXT("M29 [레벨업] 꺼짐 = 부족 문구 대상"), FRules::IsPurchaseStep(EMissionConditionType::ReachHQLevel, HQLevelUpGuide::PressLevelUp));
		Test.TestFalse(TEXT("M29 성장 탭 열기는 부족 문구 대상이 아니다"), FRules::IsPurchaseStep(EMissionConditionType::ReachHQLevel, HQLevelUpGuide::OpenGrowthTab));
		Test.TestFalse(TEXT("M29 [레벨업]은 배치 확정 걸음이 아니다(게이트 유지)"), FRules::IsPlacementConfirmStep(EMissionConditionType::ReachHQLevel, HQLevelUpGuide::PressLevelUp));
		Test.TestEqual(TEXT("M29 걸음 수 = 6"), GetDerivedStepCount(EMissionConditionType::ReachHQLevel), 6);
		Test.TestEqual(TEXT("M29 관찰은 마지막 줄"), HQLevelUpGuide::WatchLevelUp, GetDerivedStepCount(EMissionConditionType::ReachHQLevel) - 1);
	}

	void TestExplainTours(FAutomationTestBase& Test)
	{
		using namespace CGRGoalGuideStep;
		Test.TestEqual(TEXT("M33 도크 [메뉴]"), ResolveMenuTourStep(false, false, false), MenuTourGuide::OpenMenu);
		Test.TestEqual(TEXT("M33 메뉴 설명"), ResolveMenuTourStep(false, true, false), MenuTourGuide::ExplainMenu);
		Test.TestEqual(TEXT("M33 메뉴를 읽었으면 [닫기]"), ResolveMenuTourStep(false, true, true), MenuTourGuide::CloseMenu);
		Test.TestEqual(TEXT("M33 메뉴를 닫으면 추가 안내 없이 완료"), ResolveMenuTourStep(false, false, true), INDEX_NONE);
		Test.TestEqual(TEXT("M33 사무실이면 나가기"), ResolveMenuTourStep(true, true, false), MenuTourGuide::ExitOffice);

		Test.TestEqual(TEXT("M22 목표 프로젝트 개발 중이면 개발 관찰"), ResolveForcedBusyProjectStep(true), LaunchGoalGuide::WatchDevelop);
		Test.TestEqual(TEXT("M22 첫 게임 운영 중이면 기다림"), ResolveForcedBusyProjectStep(false), LaunchGoalGuide::WaitRunning);
	}
}

bool FCGRTutorialDerivedStepsTest::RunTest(const FString& Parameters)
{
	TestPromptFlow(*this);
	TestCompanyGrowthSteps(*this);
	TestCompanyGachaSteps(*this);
	TestEquipmentSteps(*this);
	TestPlacementGating(*this);
	TestCityAcquisitionSteps(*this);
	TestHQLevelUpSteps(*this);
	TestExplainTours(*this);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRTutorialEntryPopupRulesTest,
	"CGR.Tutorial.EntryPopupRules",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRTutorialEntryPopupRulesTest::RunTest(const FString& Parameters)
{
	const FString Today(TEXT("2026-10-16"));
	FEntryPopupSaveData Save;
	TestEqual(TEXT("서버 날짜를 모르면 아무것도 열지 않는다"),
		FEntryPopupRules::ResolveNext(true, true, FString(), Save, false), EEntryPopup::None);
	TestEqual(TEXT("받을 출석이 먼저"), FEntryPopupRules::ResolveNext(true, true, Today, Save, false), EEntryPopup::Attendance);
	FEntryPopupRules::MarkShown(EEntryPopup::Attendance, Today, Save);
	TestEqual(TEXT("출석을 띄운 날은 다음이 새 공지"), FEntryPopupRules::ResolveNext(true, true, Today, Save, false), EEntryPopup::Notice);
	TestEqual(TEXT("튜토리얼을 막 끝낸 세션은 출석 하나만"), FEntryPopupRules::ResolveNext(true, true, Today, Save, true), EEntryPopup::None);
	FEntryPopupRules::MarkShown(EEntryPopup::Notice, Today, Save);
	TestEqual(TEXT("둘 다 띄운 날은 더 열지 않는다(닫고 안 받아도)"), FEntryPopupRules::ResolveNext(true, true, Today, Save, false), EEntryPopup::None);
	TestEqual(TEXT("다음 날은 다시 출석부터"), FEntryPopupRules::ResolveNext(true, true, TEXT("2026-10-17"), Save, false), EEntryPopup::Attendance);
	TestEqual(TEXT("이미 받은 출석은 열지 않는다"), FEntryPopupRules::ResolveNext(false, false, TEXT("2026-10-17"), Save, false), EEntryPopup::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRSkipCostRulesTest,
	"CGR.CityAcq.SkipCostRules",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRSkipCostRulesTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("처음부터 사면 티어 × 10"), FSkipCostRules::Diamonds(3, 1.0), 30);
	TestEqual(TEXT("티어 1 도 최소 10"), FSkipCostRules::Diamonds(1, 1.0), 10);
	TestEqual(TEXT("절반 남으면 절반"), FSkipCostRules::Diamonds(10, 0.5), 50);
	TestEqual(TEXT("조금이라도 남았으면 최소 1"), FSkipCostRules::Diamonds(10, 0.0001), 1);
	TestEqual(TEXT("다 끝났으면 0"), FSkipCostRules::Diamonds(10, 0.0), 0);
	TestEqual(TEXT("서버 상한을 넘지 않는다"), FSkipCostRules::Diamonds(12, 1.0), FSkipCostRules::MaxDiamonds);
	TestEqual(TEXT("남은 비율은 올림"), FSkipCostRules::Diamonds(2, 0.51), 11);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
