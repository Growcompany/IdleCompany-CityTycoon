#include "Misc/AutomationTest.h"
#include "Table/MissionTable.h"
#include "Table/BuildableCardTable.h"
#include "Manager/MissionGuidePhases.h"
#include "Data/TutorialDerivedMissionRules.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRTutorialChainOrderTest,
	"CGR.Tutorial.ChainOrder",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace
{
	void TestDerivedRules(FAutomationTestBase& Test)
	{
		using FRules = FTutorialDerivedMissionRules;

		Test.TestEqual(TEXT("상태·래치형 Amount 0 은 하나 이상으로 읽는다"), FRules::ResolveTarget(0), 1);
		Test.TestEqual(TEXT("레벨형 Amount 는 목표 레벨 그대로"), FRules::ResolveTarget(2), 2);

		Test.TestFalse(TEXT("공장 작업속도만 Lv2 면 미완"), FRules::AreFactoryStatsReached(2, 1, 2));
		Test.TestTrue(TEXT("공장 작업속도·생산능력 둘 다 Lv2 면 완료"), FRules::AreFactoryStatsReached(2, 2, 2));

		Test.TestFalse(TEXT("빌드업 직후 패널이 열려 있으면 층 상승 연출을 기다린다"),
			FRules::IsFloorRaiseDone(1, 1, /*SignalAt=*/10.0, /*Now=*/11.0, /*bManagePanelOpen=*/true));
		Test.TestTrue(TEXT("빌드업 2초 뒤에는 패널이 열려 있어도 완료"),
			FRules::IsFloorRaiseDone(1, 1, 10.0, 12.0, true));
		Test.TestTrue(TEXT("패널을 닫으면 층이 보이므로 바로 완료"),
			FRules::IsFloorRaiseDone(1, 1, 10.0, 10.5, false));
		Test.TestTrue(TEXT("재접속(신호 없음)은 기다리지 않는다"),
			FRules::IsFloorRaiseDone(1, 1, -1.0, 0.0, true));
		Test.TestFalse(TEXT("층이 모자라면 미완"),
			FRules::IsFloorRaiseDone(0, 1, -1.0, 0.0, false));
		Test.TestTrue(TEXT("맵 이동으로 월드 시계가 되돌아가도 갇히지 않는다"),
			FRules::HasPresentationElapsed(50.0, 3.0, 2.0));

		Test.TestFalse(TEXT("철거 직후는 붕괴 연출을 기다린다"), FRules::IsDemolishDone(true, 5.0, 7.0));
		Test.TestTrue(TEXT("철거 4초 뒤 완료"), FRules::IsDemolishDone(true, 5.0, 9.0));
		Test.TestFalse(TEXT("정리된 회사가 없으면 미완"), FRules::IsDemolishDone(false, -1.0, 100.0));

		Test.TestFalse(TEXT("부지 창을 막 열었으면 아직 읽지 않았다"), FRules::ShouldLatchPlotExplain(1.0, 1.5));
		Test.TestTrue(TEXT("부지 창을 2.5초 이상 열었으면 읽었다"), FRules::ShouldLatchPlotExplain(1.0, 3.5));
		Test.TestFalse(TEXT("창이 닫혀 있으면 래치하지 않는다"), FRules::ShouldLatchPlotExplain(-1.0, 99.0));
		Test.TestFalse(TEXT("읽었어도 창이 열려 있으면 미완"), FRules::IsPlotExplainDone(true, true));
		Test.TestTrue(TEXT("읽고 닫으면 완료"), FRules::IsPlotExplainDone(true, false));

		Test.TestFalse(TEXT("본사 레벨이 모자라면 미완"), FRules::IsHQLevelUpDone(1, 2, -1.0, 0.0));
		Test.TestFalse(TEXT("레벨업 직후는 축하 연출을 기다린다"), FRules::IsHQLevelUpDone(2, 2, 10.0, 11.0));
		Test.TestTrue(TEXT("레벨업 2초 뒤 완료"), FRules::IsHQLevelUpDone(2, 2, 10.0, 12.0));
		Test.TestTrue(TEXT("재접속(신호 없음)이면 이미 오른 레벨로 바로 완료"), FRules::IsHQLevelUpDone(3, 2, -1.0, 0.0));

		const TArray<int32> Owned = { 7, 2, 2, INDEX_NONE, 11 };
		Test.TestEqual(TEXT("첫 회사 = 가장 작은 BuildingIndex"), FRules::ResolveCompanyIndexByOrder(Owned, 1), 2);
		Test.TestEqual(TEXT("두 번째 회사 = 다음으로 지은 회사(중복 제외)"), FRules::ResolveCompanyIndexByOrder(Owned, 2), 7);
		Test.TestEqual(TEXT("세 번째 회사"), FRules::ResolveCompanyIndexByOrder(Owned, 3), 11);
		Test.TestEqual(TEXT("없는 순서는 INDEX_NONE"), FRules::ResolveCompanyIndexByOrder(Owned, 4), static_cast<int32>(INDEX_NONE));
		Test.TestEqual(TEXT("순서 0 이하는 첫 회사로 읽는다"), FRules::ResolveCompanyIndexByOrder(Owned, 0), 2);
		Test.TestEqual(TEXT("회사가 없으면 INDEX_NONE"), FRules::ResolveCompanyIndexByOrder(TArray<int32>(), 1), static_cast<int32>(INDEX_NONE));

		Test.TestFalse(TEXT("메뉴 설명을 읽기 전에는 미완"), FRules::IsMenuTourDone(false, false));
		Test.TestFalse(TEXT("메뉴를 읽어도 프롬프트가 열려 있으면 미완"), FRules::IsMenuTourDone(true, true));
		Test.TestTrue(TEXT("상점 방문 없이 메뉴만 읽고 닫으면 완료"), FRules::IsMenuTourDone(true, false));

		const TCHAR* HQLatch = FRules::ResolveIntroLatch(EMissionConditionType::ReachHQLevel, TEXT("HQManage"));
		Test.TestTrue(TEXT("M29 본사 성장 조건 설명 완료 키"),
			HQLatch && FCString::Strcmp(HQLatch, TutorialDerivedLatch::HQIntroDone) == 0);
		const TCHAR* MenuLatch = FRules::ResolveIntroLatch(EMissionConditionType::MenuTour, TEXT("MenuPanel"));
		Test.TestTrue(TEXT("M33 메뉴 설명 완료 키"),
			MenuLatch && FCString::Strcmp(MenuLatch, TutorialDerivedLatch::MenuIntroDone) == 0);
		Test.TestNull(TEXT("상점 설명은 튜토리얼 완료 조건이 아니다"),
			FRules::ResolveIntroLatch(EMissionConditionType::MenuTour, TEXT("ShopPanel")));
		Test.TestNull(TEXT("다른 미션의 설명은 래치하지 않는다"),
			FRules::ResolveIntroLatch(EMissionConditionType::ReachHQLevel, TEXT("MenuPanel")));
		Test.TestNull(TEXT("본사 설명은 메뉴 투어 래치가 아니다"),
			FRules::ResolveIntroLatch(EMissionConditionType::MenuTour, TEXT("HQManage")));

		Test.TestTrue(TEXT("M13 책상 [배치]는 배치 확정 걸음"),
			FRules::IsPlacementConfirmStep(EMissionConditionType::PlaceSecondDesk, SecondDeskGuide::Confirm));
		Test.TestTrue(TEXT("M20 그림 [배치]는 배치 확정 걸음"),
			FRules::IsPlacementConfirmStep(EMissionConditionType::PlaceFirstPainting, PaintingGuide::Confirm));
		Test.TestTrue(TEXT("M22 건물 [배치]는 배치 확정 걸음"),
			FRules::IsPlacementConfirmStep(EMissionConditionType::BuildSecondBuilding, SecondBuildingGuide::PlaceBuilding));
		Test.TestTrue(TEXT("M30 IT 건물 [배치]도 배치 확정 걸음"),
			FRules::IsPlacementConfirmStep(EMissionConditionType::BuildIndustry, SecondBuildingGuide::PlaceBuilding));
		const FGuid GiftHat = FGuid::NewGuid();
		const FGuid PulledHat = FGuid::NewGuid();
		const TArray<FName> WornAtStart = { FName(TutorialDerivedLatch::EquipBaseline), FRules::MakeEquipHadLatch(GiftHat) };
		Test.TestEqual(TEXT("M19 같은 부위 [교체]도 새 장비 장착"), FRules::CountNewlyWornEquipment({ PulledHat }, WornAtStart), 1);
		Test.TestEqual(TEXT("M19 시작 때 끼던 장비만이면 아직"), FRules::CountNewlyWornEquipment({ GiftHat }, WornAtStart), 0);
		Test.TestEqual(TEXT("M19 다른 부위에 더 끼면 새 장비 1"), FRules::CountNewlyWornEquipment({ GiftHat, PulledHat }, WornAtStart), 1);
		Test.TestTrue(TEXT("M13 마지막 하나 [배치]도 배치 확정 걸음"),
			FRules::IsPlacementConfirmStep(EMissionConditionType::PlaceSecondDesk, SecondDeskGuide::ConfirmMore));
		Test.TestTrue(TEXT("채용 중 빈 책상 [배치]도 배치 확정 걸음"),
			FRules::IsPlacementConfirmStep(EMissionConditionType::RecruitGoalDiscipline, RecruitDisciplineGuide::ConfirmVacantDesk));
		Test.TestFalse(TEXT("채용 중 책상 카드 고르기는 배치 확정이 아니다"),
			FRules::IsPlacementConfirmStep(EMissionConditionType::RecruitGoalDiscipline, RecruitDisciplineGuide::PickVacantDesk));
		Test.TestFalse(TEXT("카드 고르기는 배치 확정이 아니다"),
			FRules::IsPlacementConfirmStep(EMissionConditionType::PlaceSecondDesk, SecondDeskGuide::PickDesk));
		Test.TestFalse(TEXT("M4 페이즈 경로는 이 규칙을 쓰지 않는다"),
			FRules::IsPlacementConfirmStep(EMissionConditionType::PlaceDesks, DeskGuide::Confirm));

		Test.TestTrue(TEXT("M10 자금 강화 걸음은 구매 걸음"),
			FRules::IsPurchaseStep(EMissionConditionType::EnhanceBuildingStat, EnhanceStatGuide::PressEnhance));
		Test.TestFalse(TEXT("장소 이동 걸음은 구매 걸음이 아니다"),
			FRules::IsPurchaseStep(EMissionConditionType::EnhanceBuildingStat, EnhanceStatGuide::ExitOffice));
		Test.TestTrue(TEXT("M29 [레벨업]은 구매 걸음 ― 꺼져 있으면 부족 문구"),
			FRules::IsPurchaseStep(EMissionConditionType::ReachHQLevel, HQLevelUpGuide::PressLevelUp));
		Test.TestFalse(TEXT("M29 설명·축하 관찰은 구매 걸음이 아니다"),
			FRules::IsPurchaseStep(EMissionConditionType::ReachHQLevel, HQLevelUpGuide::Explain)
			|| FRules::IsPurchaseStep(EMissionConditionType::ReachHQLevel, HQLevelUpGuide::WatchLevelUp));
		Test.TestTrue(TEXT("M30 IT 건물 카드는 구매 걸음(벽돌)"),
			FRules::IsPurchaseStep(EMissionConditionType::BuildIndustry, SecondBuildingGuide::PickBuilding));

		const TArray<FName> Saved = { FName(TutorialDerivedLatch::MenuIntroDone) };
		Test.TestEqual(TEXT("같은 미션이면 래치 복원"),
			FRules::ResolveRestoredLatches(TEXT("M33_MenuTour"), TEXT("M33_MenuTour"), true, Saved).Num(), 1);
		Test.TestEqual(TEXT("다른 미션(점프)이면 래치 버림"),
			FRules::ResolveRestoredLatches(TEXT("M33_MenuTour"), TEXT("M31_LevelUpHQ"), true, Saved).Num(), 0);
		Test.TestEqual(TEXT("도출형이 아니면 래치 버림"),
			FRules::ResolveRestoredLatches(TEXT("M7_CollectRevenue"), TEXT("M7_CollectRevenue"), false, Saved).Num(), 0);
	}
}

bool FCGRTutorialChainOrderTest::RunTest(const FString& Parameters)
{
	TestDerivedRules(*this);

	// 아래는 DT_Mission 재임포트 뒤에만 통과한다 — CSV 가 아니라 실제 에셋이 체인을 정한다
	const UDataTable* Missions = LoadObject<UDataTable>(nullptr, TEXT("/Game/CompanyGrowth/Table/Mission/DT_Mission.DT_Mission"));
	if (!TestNotNull(TEXT("DT_Mission"), Missions))
	{
		return false;
	}
	const UDataTable* Buildables = LoadObject<UDataTable>(nullptr,
		TEXT("/Game/CompanyGrowth/Table/Building/BuildableTable.BuildableTable"));
	if (!TestNotNull(TEXT("실제 건설비 DT_BuildableCard"), Buildables)) return false;
	const FMissionTable* Opening = Missions->FindRow<FMissionTable>(TEXT("M1_CollectBricks"), TEXT("TutorialChainOrderTest"), false);
	const FBuildableCardTable* Starter = Buildables->FindRow<FBuildableCardTable>(TEXT("B1"), TEXT("TutorialChainOrderTest"), false);
	if (!TestNotNull(TEXT("M1 벽돌 목표"), Opening) || !TestNotNull(TEXT("B1 첫 건물 카드"), Starter)) return false;
	TestTrue(TEXT("M1 직접 모은 벽돌로 B1 실제 건설비를 감당한다"),
		Opening->ConditionType == EMissionConditionType::CollectBricks
		&& Starter->ConstructionCosts.Num() == 1
		&& Starter->ConstructionCosts[0].ResourceType == EResourceType::Brick
		&& Opening->ConditionAmount >= Starter->ConstructionCosts[0].Cost);

	TArray<FName> Chain;
	TSet<FName> Visited;
	FName MissionID = TEXT("M1_CollectBricks");
	while (!MissionID.IsNone())
	{
		if (!TestFalse(*FString::Printf(TEXT("체인 순환 없음: %s"), *MissionID.ToString()), Visited.Contains(MissionID)))
		{
			return false;
		}
		Visited.Add(MissionID);
		const FMissionTable* Row = Missions->FindRow<FMissionTable>(MissionID, TEXT("TutorialChainOrderTest"));
		if (!TestNotNull(*FString::Printf(TEXT("체인 행 존재: %s"), *MissionID.ToString()), Row))
		{
			return false;
		}
		Chain.Add(MissionID);
		MissionID = Row->NextMissionID;
	}

	TestEqual(TEXT("M1 에서 None 까지 33미션"), Chain.Num(), 33);
	TestEqual(TEXT("DT 행이 전부 체인에 걸려 있다"), Missions->GetRowNames().Num(), Chain.Num());
	const FName PaintingID(TEXT("M21_PlacePainting"));
	const FName SecondBuildingID(TEXT("M23_BuildSecond"));
	const FName SecondDeskID(TEXT("M24_PlaceDeskSecond"));
	const FName SecondRecruitID(TEXT("M25_RecruitSecond"));
	const FName SecondLaunchID(TEXT("M26_LaunchSecond"));
	const FName FirstCompanyGameID(TEXT("M22_LaunchGame2"));
	const FName AcquisitionID(TEXT("M27_AcquireCompany"));
	const int32 PaintingIndex = Chain.IndexOfByKey(PaintingID);
	const int32 BuildingIndex = Chain.IndexOfByKey(SecondBuildingID);
	const int32 DeskIndex = Chain.IndexOfByKey(SecondDeskID);
	const int32 RecruitIndex = Chain.IndexOfByKey(SecondRecruitID);
	const int32 SecondLaunchIndex = Chain.IndexOfByKey(SecondLaunchID);
	const int32 FirstCompanyGameIndex = Chain.IndexOfByKey(FirstCompanyGameID);
	const int32 AcquisitionIndex = Chain.IndexOfByKey(AcquisitionID);
	const int32 DerivedGuideStartIndex = Chain.IndexOfByKey(FName(TEXT("M9_RaiseFloor")));
	TestTrue(TEXT("도출형 가이드 시작 미션 M9가 체인에 있다"), DerivedGuideStartIndex != INDEX_NONE);
	TestTrue(TEXT("두 번째 회사 건설·책상·채용·출시를 첫 회사 재출시 대기 전에 마친다"),
		PaintingIndex != INDEX_NONE && BuildingIndex == PaintingIndex + 1
		&& DeskIndex == BuildingIndex + 1 && RecruitIndex == DeskIndex + 1
		&& SecondLaunchIndex == RecruitIndex + 1 && FirstCompanyGameIndex == SecondLaunchIndex + 1
		&& AcquisitionIndex == FirstCompanyGameIndex + 1);
	const FMissionTable* SecondLaunch = Missions->FindRow<FMissionTable>(SecondLaunchID, TEXT("TutorialChainOrderTest"), false);
	const FMissionTable* FirstCompanyGame = Missions->FindRow<FMissionTable>(FirstCompanyGameID, TEXT("TutorialChainOrderTest"), false);
	if (!TestNotNull(TEXT("두 번째 회사 첫 출시 미션"), SecondLaunch)
		|| !TestNotNull(TEXT("첫 회사 두 번째 출시 미션"), FirstCompanyGame)) return false;
	TestTrue(TEXT("두 게임 출시는 서로 다른 회사에서 실제 출시를 요구한다"),
		SecondLaunch->ConditionType == EMissionConditionType::LaunchGoalProject
		&& FirstCompanyGame->ConditionType == EMissionConditionType::LaunchGoalProject
		&& SecondLaunch->TutorialCompanyOrder == 2 && FirstCompanyGame->TutorialCompanyOrder == 1);

	for (int32 ChainIndex = 0; ChainIndex < Chain.Num(); ++ChainIndex)
	{
		const FMissionTable* Row = Missions->FindRow<FMissionTable>(Chain[ChainIndex], TEXT("TutorialChainOrderTest"));
		if (!Row)
		{
			continue;
		}
		const FString Label = Chain[ChainIndex].ToString();
		const bool bShouldManualClaim = Chain[ChainIndex] == TEXT("M8_CollectRevenue")
			|| Chain[ChainIndex] == TEXT("M33_MenuTour");
		TestTrue(*FString::Printf(TEXT("%s 수동 수령은 M8·M33 만"), *Label), Row->bManualClaim == bShouldManualClaim);
		// 공장 강화(M2)는 돈이 귀한 M1 직후에 둔다 — 첫 수익 보상 뒤에는 비용이 하찮아진다
		const bool bShouldDerived = Chain[ChainIndex] == TEXT("M2_UpgradeFactory")
			|| (DerivedGuideStartIndex != INDEX_NONE && ChainIndex >= DerivedGuideStartIndex);
		TestTrue(*FString::Printf(TEXT("%s 도출형은 M2·M9 이후"), *Label), Row->bDerivedGuide == bShouldDerived);
		// 두 번째 회사 채우기(M24 책상·M25 채용·M26 첫 게임)만 두 번째 회사를 다룬다
		const bool bSecondCompany = Chain[ChainIndex] == SecondDeskID
			|| Chain[ChainIndex] == SecondRecruitID || Chain[ChainIndex] == SecondLaunchID;
		TestEqual(*FString::Printf(TEXT("%s 대상 회사 순서"), *Label), Row->TutorialCompanyOrder, bSecondCompany ? 2 : 1);
		const ECompanyType ExpectedIndustry = Chain[ChainIndex] == SecondBuildingID ? ECompanyType::Game
			: Chain[ChainIndex] == TEXT("M32_BuildIT") ? ECompanyType::IT : ECompanyType::None;
		TestTrue(*FString::Printf(TEXT("%s 건설 산업 지정"), *Label), Row->RequiredIndustry == ExpectedIndustry);
		if (!Row->bDerivedGuide)
		{
			continue;
		}
		if (Chain[ChainIndex] == TEXT("M31_LevelUpHQ"))
		{
			TestTrue(TEXT("M31 은 본사 Lv2 실제 레벨업"), Row->ConditionType == EMissionConditionType::ReachHQLevel && Row->ConditionAmount == 2);
			TestFalse(TEXT("M31 [레벨업]이 꺼졌을 때의 부족 문구가 있다"), Row->ShortfallLine.IsEmpty());
		}
		if (Chain[ChainIndex] == TEXT("M32_BuildIT"))
		{
			TestTrue(TEXT("M32 는 IT 회사 1채 건설"), Row->ConditionType == EMissionConditionType::BuildIndustry && Row->ConditionAmount == 1);
		}
		const int32 StepCount = GetDerivedStepCount(Row->ConditionType);
		TestTrue(*FString::Printf(TEXT("%s 조건에 걸음표가 있다"), *Label), StepCount > 0);
		TestEqual(*FString::Printf(TEXT("%s GuideLines 줄 수 = 걸음 수"), *Label), Row->GuideLines.Num(), StepCount);
		TestEqual(*FString::Printf(TEXT("%s MentorLines 줄 수 = 걸음 수"), *Label), Row->MentorLines.Num(), StepCount);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
