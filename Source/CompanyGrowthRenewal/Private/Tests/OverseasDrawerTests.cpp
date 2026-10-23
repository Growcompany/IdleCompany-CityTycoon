#include "UI/Panel/CountryDrawerModel.h"
#include "Table/CountryInfoTable.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace OverseasDrawerTests
{
	// DT_CountryInfo_Import.csv 의 시설 칸(bSupportsFactory / bSupportsMine+MinableResources / bIsTradeHub)을 그대로 옮긴 행
	FCountryInfoTable MakeRow(bool bFactory, bool bMine, TArray<EResourceType> Resources, bool bHub)
	{
		FCountryInfoTable Row;
		Row.bSupportsFactory = bFactory;
		Row.bSupportsMine = bMine;
		Row.MinableResources = MoveTemp(Resources);
		Row.bIsTradeHub = bHub;
		return Row;
	}

	FString Str(const FText& Text)
	{
		return Text.ToString();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverseasDrawerFacilityTabsTest, "CGR.Overseas.Drawer.FacilityTabs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FOverseasDrawerFacilityTabsTest::RunTest(const FString& Parameters)
{
	using namespace OverseasDrawerTests;

	const TArray<EDioramaModule> Korea = CountryDrawer::FacilitiesOf(MakeRow(true, false, {}, false));
	TestEqual(TEXT("Korea = factory only"), Korea.Num(), 1);
	TestTrue(TEXT("Korea factory"), Korea.Num() == 1 && Korea[0] == EDioramaModule::Factory);
	TestFalse(TEXT("Korea hides tab row"), CountryDrawer::ShouldShowTabs(Korea));

	const TArray<EDioramaModule> Usa = CountryDrawer::FacilitiesOf(MakeRow(false, false, {}, true));
	TestTrue(TEXT("USA = port only"), Usa.Num() == 1 && Usa[0] == EDioramaModule::Port);
	TestFalse(TEXT("USA hides tab row"), CountryDrawer::ShouldShowTabs(Usa));
	TestEqual(TEXT("oil resource = oil field"), Str(CountryDrawer::MineTabLabel(CountryDrawer::IsAllOil({ EResourceType::Oil }))),
		FString(TEXT("유전")));

	const TArray<EDioramaModule> Singapore = CountryDrawer::FacilitiesOf(MakeRow(false, false, {}, true));
	TestTrue(TEXT("Singapore = port only"), Singapore.Num() == 1 && Singapore[0] == EDioramaModule::Port);
	TestFalse(TEXT("Singapore hides tab row"), CountryDrawer::ShouldShowTabs(Singapore));

	const TArray<EDioramaModule> China = CountryDrawer::FacilitiesOf(MakeRow(true, false, {}, false));
	TestTrue(TEXT("China = factory only"), China.Num() == 1 && China[0] == EDioramaModule::Factory);
	TestFalse(TEXT("China hides tab row"), CountryDrawer::ShouldShowTabs(China));
	TestEqual(TEXT("mixed resources = mine"), Str(CountryDrawer::MineTabLabel(CountryDrawer::IsAllOil({ EResourceType::Oil, EResourceType::Wood }))),
		FString(TEXT("광산")));

	const TArray<EDioramaModule> NoResources = CountryDrawer::FacilitiesOf(MakeRow(false, true, {}, false));
	TestEqual(TEXT("mine flag without resources is not a facility"), NoResources.Num(), 0);
	TestFalse(TEXT("no resources is not oil"), CountryDrawer::IsAllOil({}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverseasDrawerTabIndexTest, "CGR.Overseas.Drawer.TabIndex",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FOverseasDrawerTabIndexTest::RunTest(const FString& Parameters)
{
	// 월드맵 ModuleForTab 이 기대하는 번호(1=공장, 2=광산)와 같아야 한다
	TestEqual(TEXT("factory tab"), CountryDrawer::TabIndexOf(EDioramaModule::Factory), 1);
	TestEqual(TEXT("mine tab"), CountryDrawer::TabIndexOf(EDioramaModule::Mine), 2);
	TestEqual(TEXT("port tab"), CountryDrawer::TabIndexOf(EDioramaModule::Port), 3);

	const TArray<EDioramaModule> Usa = { EDioramaModule::Port };
	const TArray<EDioramaModule> China = { EDioramaModule::Factory };
	EDioramaModule Out = EDioramaModule::Port;
	TestTrue(TEXT("primary tab resolves to USA port"), CountryDrawer::FacilityForTab(0, Usa, Out) && Out == EDioramaModule::Port);
	TestTrue(TEXT("mine tab on USA resolves to port"), CountryDrawer::FacilityForTab(2, Usa, Out) && Out == EDioramaModule::Port);
	TestTrue(TEXT("port tab on China falls back to first"), CountryDrawer::FacilityForTab(3, China, Out) && Out == EDioramaModule::Factory);
	TestFalse(TEXT("no facilities"), CountryDrawer::FacilityForTab(1, {}, Out));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverseasDrawerHeroSummaryTest, "CGR.Overseas.Drawer.HeroSummary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FOverseasDrawerHeroSummaryTest::RunTest(const FString& Parameters)
{
	using namespace OverseasDrawerTests;

	TestEqual(TEXT("factory title"), Str(CountryDrawer::FactoryTitle(FText::FromString(TEXT("전자")))), FString(TEXT("전자 공장")));
	TestEqual(TEXT("factory summary with running and ready"), Str(CountryDrawer::FactorySummary(3, 1, 1)),
		FString(TEXT("라인 3개 · 생산 중 1 · 완료 1")));
	TestEqual(TEXT("factory summary hides zero parts"), Str(CountryDrawer::FactorySummary(3, 0, 0)),
		FString(TEXT("라인 3개")));
	TestEqual(TEXT("mine hero summary with full"), Str(CountryDrawer::MineHeroSummary(1, 1)),
		FString(TEXT("라인 1개 · 가득 참 1")));
	TestEqual(TEXT("mine hero summary hides zero full"), Str(CountryDrawer::MineHeroSummary(2, 0)),
		FString(TEXT("라인 2개")));
	TestEqual(TEXT("mine hero summary without lines"), Str(CountryDrawer::MineHeroSummary(0, 0)),
		FString(TEXT("채광 라인을 설치해 주세요")));

	TestEqual(TEXT("mine title"), Str(CountryDrawer::MineTitle(false, 2)), FString(TEXT("광산 2곳")));
	TestEqual(TEXT("oil title"), Str(CountryDrawer::MineTitle(true, 1)), FString(TEXT("유전 1곳")));
	const TArray<FText> Names = { FText::FromString(TEXT("희토류")), FText::FromString(TEXT("철광석")) };
	TestEqual(TEXT("mine summary storage"), Str(CountryDrawer::MineSummary(Names, false, 200)),
		FString(TEXT("희토류 · 철광석 · 200개씩 보관")));
	TestEqual(TEXT("mine summary auto"), Str(CountryDrawer::MineSummary(Names, true, 200)),
		FString(TEXT("희토류 · 철광석 · 가득 차면 자동으로 받습니다")));

	TestEqual(TEXT("port title"), Str(CountryDrawer::PortTitle(FText::FromString(TEXT("미국")))), FString(TEXT("미국 무역항")));
	TestEqual(TEXT("port summary with orders"), Str(CountryDrawer::PortSummary(1.35f, 2)),
		FString(TEXT("시세 ×1.35 · 주문을 먼저 채웁니다")));
	TestEqual(TEXT("port summary without orders"), Str(CountryDrawer::PortSummary(1.0f, 0)), FString(TEXT("시세 ×1")));
	TestEqual(TEXT("port order name (spec §2.8)"), Str(CountryDrawer::OrderName(FText::FromString(TEXT("B")), FText::FromString(TEXT("에어프라이어")))),
		FString(TEXT("B 주문 · 에어프라이어")));
	TestEqual(TEXT("port order name without product"), Str(CountryDrawer::OrderName(FText::FromString(TEXT("B")), FText::GetEmpty())),
		FString(TEXT("B 주문")));
	TestEqual(TEXT("port order detail (spec §2.8)"), Str(CountryDrawer::OrderDetail(80, 1.5f)), FString(TEXT("80개 납품 · 보상 ×1.5")));

	TestEqual(TEXT("factory CTA"), Str(CountryDrawer::ClaimFactoryLabel()), FString(TEXT("모두 받기")));
	TestEqual(TEXT("mine CTA"), Str(CountryDrawer::ClaimMineLabel()), FString(TEXT("모두 받기")));
	TestEqual(TEXT("port CTA"), Str(CountryDrawer::SellAllLabel()), FString(TEXT("모두 판매")));
	TestEqual(TEXT("empty line CTA"), Str(CountryDrawer::StartLineLabel()), FString(TEXT("생산 시작")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverseasDrawerPageToggleRulesTest, "CGR.Overseas.Drawer.PageToggleRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FOverseasDrawerPageToggleRulesTest::RunTest(const FString& Parameters)
{
	using CountryDrawer::EDrawerPage;
	TestTrue(TEXT("factory shows toggle"), CountryDrawer::ShowsPageToggle(EDioramaModule::Factory));
	TestTrue(TEXT("mine shows toggle"), CountryDrawer::ShowsPageToggle(EDioramaModule::Mine));
	TestFalse(TEXT("port hides toggle"), CountryDrawer::ShowsPageToggle(EDioramaModule::Port));

	TestTrue(TEXT("dot on upgrade page with pending"), CountryDrawer::ShowsProductionDot(EDrawerPage::Upgrade, 1));
	TestFalse(TEXT("no dot on production page"), CountryDrawer::ShowsProductionDot(EDrawerPage::Production, 5));
	TestFalse(TEXT("no dot without pending"), CountryDrawer::ShowsProductionDot(EDrawerPage::Upgrade, 0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverseasDrawerLineProgressTest, "CGR.Overseas.Drawer.LineProgress",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FOverseasDrawerLineProgressTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("whole progress includes current unit"), CountryDrawer::LineProgress01(38, 81, 0.5), 38.5f / 81.f);
	TestEqual(TEXT("finished line is full"), CountryDrawer::LineProgress01(81, 81, 0.0), 1.f);
	TestEqual(TEXT("zero target is empty"), CountryDrawer::LineProgress01(0, 0, 0.0), 0.f);
	TestEqual(TEXT("near the end"), CountryDrawer::LineProgress01(80, 81, 0.9), 80.9f / 81.f);
	TestTrue(TEXT("finishing one unit does not reset the bar"),
		CountryDrawer::LineProgress01(39, 81, 0.0) > CountryDrawer::LineProgress01(38, 81, 0.99));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverseasDrawerDurationsTest, "CGR.Overseas.Drawer.Durations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FOverseasDrawerDurationsTest::RunTest(const FString& Parameters)
{
	using namespace OverseasDrawerTests;

	TestEqual(TEXT("minutes and seconds"), Str(CountryDrawer::DurationLabel(FTimespan(0, 4, 59))), FString(TEXT("4분 59초")));
	TestEqual(TEXT("seconds only"), Str(CountryDrawer::DurationLabel(FTimespan::FromSeconds(12))), FString(TEXT("12초")));
	TestEqual(TEXT("hours drop seconds"), Str(CountryDrawer::DurationLabel(FTimespan(1, 2, 30))), FString(TEXT("1시간 2분")));
	TestEqual(TEXT("zero"), Str(CountryDrawer::DurationLabel(FTimespan::Zero())), FString(TEXT("0초")));
	TestEqual(TEXT("negative = zero"), Str(CountryDrawer::DurationLabel(FTimespan::FromSeconds(-3))), FString(TEXT("0초")));
	TestEqual(TEXT("no rounding up"), Str(CountryDrawer::DurationLabel(FTimespan::FromSeconds(12.9))), FString(TEXT("12초")));

	TestEqual(TEXT("factory remaining"), Str(CountryDrawer::FactoryRemainingLabel(FTimespan(0, 4, 59))), FString(TEXT("남은 4분 59초")));
	TestEqual(TEXT("mine full eta"), Str(CountryDrawer::MineFullEtaLabel(FTimespan::FromSeconds(72))), FString(TEXT("가득까지 1분 12초")));
	TestEqual(TEXT("completed"), Str(CountryDrawer::CompletedStatus()), FString(TEXT("완료")));
	TestEqual(TEXT("full"), Str(CountryDrawer::FullStatus()), FString(TEXT("가득 참")));
	TestEqual(TEXT("bar count"), Str(CountryDrawer::BarCountLabel(38, 81)), FString(TEXT("38 / 81개")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverseasDrawerLockedLineTest, "CGR.Overseas.Drawer.LockedLine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FOverseasDrawerLockedLineTest::RunTest(const FString& Parameters)
{
	using namespace OverseasDrawerTests;

	TestTrue(TEXT("below max shows locked line"), CountryDrawer::ShowsLockedLine(3, 6));
	TestFalse(TEXT("at max hides locked line"), CountryDrawer::ShowsLockedLine(6, 6));
	TestEqual(TEXT("locked line name"), Str(CountryDrawer::LockedLineName(4)), FString(TEXT("4번 라인")));
	TestEqual(TEXT("locked line detail"), Str(CountryDrawer::LockedLineDetail()), FString(TEXT("강화의 라인 증설로 열립니다")));
	TestEqual(TEXT("empty line detail"), Str(CountryDrawer::EmptyLineDetail()), FString(TEXT("제품을 고르면 생산을 시작합니다")));
	TestEqual(TEXT("empty mine detail"), Str(CountryDrawer::EmptyMineDetail()), FString(TEXT("캘 자원을 고릅니다")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverseasDrawerUpgradeSlotMaxNotLockedTest, "CGR.Overseas.Drawer.UpgradeSlotMaxNotLocked",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FOverseasDrawerUpgradeSlotMaxNotLockedTest::RunTest(const FString& Parameters)
{
	using namespace OverseasDrawerTests;

	TestFalse(TEXT("max level is not locked"), CountryDrawer::UpgradeSlotLocked(true));
	TestFalse(TEXT("below max is not locked"), CountryDrawer::UpgradeSlotLocked(false));
	TestEqual(TEXT("max label"), Str(CountryDrawer::MaxLevelLabel()), FString(TEXT("최대")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverseasDrawerModalCopyTest, "CGR.Overseas.Drawer.ModalCopy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FOverseasDrawerModalCopyTest::RunTest(const FString& Parameters)
{
	using namespace OverseasDrawerTests;

	TestEqual(TEXT("instant finish factory"), Str(CountryDrawer::InstantFinishMessage(5, 43, false)),
		FString(TEXT("다이아 5개로 남은 43개를 지금 바로 만듭니다")));
	TestTrue(TEXT("instant finish mine digs"), Str(CountryDrawer::InstantFinishMessage(2, 3, true)).EndsWith(TEXT("캡니다")));
	TestEqual(TEXT("instant finish title"), Str(CountryDrawer::InstantFinishTitle()), FString(TEXT("즉시 완료")));
	TestEqual(TEXT("instant finish cancel"), Str(CountryDrawer::CancelLabel()), FString(TEXT("취소")));

	TestEqual(TEXT("limit band without batchim"),
		Str(CountryDrawer::ProductionLimitBand(FText::FromString(TEXT("석유")), 3, 1)),
		FString(TEXT("석유가 3개뿐이라 최대 1개까지 만들 수 있습니다")));
	TestTrue(TEXT("limit band with batchim"),
		Str(CountryDrawer::ProductionLimitBand(FText::FromString(TEXT("실리콘")), 4, 1)).StartsWith(TEXT("실리콘이")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverseasDrawerQuickStartTest, "CGR.Overseas.Drawer.QuickStartPick",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FOverseasDrawerQuickStartTest::RunTest(const FString& Parameters)
{
	using FCandidate = CountryDrawer::FQuickStartCandidate;
	auto Make = [](int32 OrderID, ECompanyType Company, int32 Project, int64 Value, int32 MaxProducible)
	{
		FCandidate C;
		C.OrderID = OrderID;
		C.Company = Company;
		C.ProjectIndex = Project;
		C.UnitValue = Value;
		C.MaxProducible = MaxProducible;
		return C;
	};

	const TArray<FCandidate> Candidates = {
		Make(5, ECompanyType::Electronics, 2, 100, 3),
		Make(3, ECompanyType::Electronics, 4, 500, 2),
		Make(7, ECompanyType::Automobile, 1, 900, 0),
	};
	TestEqual(TEXT("last product wins over pricier"),
		CountryDrawer::PickQuickStart(Candidates, ECompanyType::Electronics, 2), 0);
	TestEqual(TEXT("no last product = most valuable producible"),
		CountryDrawer::PickQuickStart(Candidates, ECompanyType::None, 0), 1);
	TestEqual(TEXT("last product without materials falls back"),
		CountryDrawer::PickQuickStart(Candidates, ECompanyType::Automobile, 1), 1);

	const TArray<FCandidate> Tie = {
		Make(9, ECompanyType::Electronics, 1, 300, 1),
		Make(4, ECompanyType::Electronics, 3, 300, 1),
	};
	TestEqual(TEXT("tie = older order"), CountryDrawer::PickQuickStart(Tie, ECompanyType::None, 0), 1);

	TestEqual(TEXT("nothing producible"),
		CountryDrawer::PickQuickStart({ Make(1, ECompanyType::Electronics, 1, 50, 0) }, ECompanyType::None, 0), INDEX_NONE);
	TestEqual(TEXT("empty list"), CountryDrawer::PickQuickStart({}, ECompanyType::None, 0), INDEX_NONE);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverseasDrawerNextMineResourceTest, "CGR.Overseas.Drawer.NextMineResource",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FOverseasDrawerNextMineResourceTest::RunTest(const FString& Parameters)
{
	const TArray<EResourceType> Canada = { EResourceType::Oil, EResourceType::Wood };
	TestTrue(TEXT("no lines = first in DT order"), CountryDrawer::NextMineResource(Canada, {}) == EResourceType::Oil);
	TestTrue(TEXT("first taken = next"), CountryDrawer::NextMineResource(Canada, { EResourceType::Oil }) == EResourceType::Wood);
	TestTrue(TEXT("taken out of order"), CountryDrawer::NextMineResource(Canada, { EResourceType::Wood }) == EResourceType::Oil);
	TestTrue(TEXT("pool exhausted"), CountryDrawer::NextMineResource(Canada, { EResourceType::Wood, EResourceType::Oil }) == EResourceType::None);
	TestTrue(TEXT("empty pool"), CountryDrawer::NextMineResource({}, {}) == EResourceType::None);
	TestEqual(TEXT("StartMineLabel"), CountryDrawer::StartMineLabel().ToString(), FString(TEXT("자원 선택")));
	return true;
}

#endif
