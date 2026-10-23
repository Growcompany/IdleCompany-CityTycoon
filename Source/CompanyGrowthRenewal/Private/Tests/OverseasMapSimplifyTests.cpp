// 해외 생산 2차 P2-B1~B3 ― 지도 이름표·말풍선·모두 받기·화면 가득 판의 순수 규칙 시험.

#include "Misc/AutomationTest.h"
#include "Engine/DataTable.h"
#include "Table/CountryInfoTable.h"
#include "UI/Element/WorldMap/MapIndustryTagWidget.h"
#include "UI/Element/WorldMap/MapActionBubbleWidget.h"
#include "UI/Element/Trade/CountryNameWidget.h"
#include "UI/Panel/WorldMapPanelWidget.h"
#include "Entity/OverseasDiorama/OverseasDioramaTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace OverseasMapSimplifyTest
{
	// DT_CountryInfo_Import.csv 의 역할 칸을 그대로 옮긴 행. DT 대조는 아래 MatchDT 시험이 따로 한다.
	FCountryInfoTable MakeRow(ECountryType Country, bool bFactory, bool bMine, bool bHub,
		TArray<ECompanyType> Industries, ECompanyType Representative, TArray<EResourceType> Resources)
	{
		FCountryInfoTable Row;
		Row.CountryType = Country;
		Row.bSupportsFactory = bFactory;
		Row.bSupportsMine = bMine;
		Row.bIsTradeHub = bHub;
		Row.SupportedIndustries = MoveTemp(Industries);
		Row.RepresentativeIndustry = Representative;
		Row.MinableResources = MoveTemp(Resources);
		return Row;
	}

	FMapIndustryTagSpec Factory(ECompanyType Industry)
	{
		FMapIndustryTagSpec Spec;
		Spec.Kind = EMapIndustryTagKind::Factory;
		Spec.Industry = Industry;
		return Spec;
	}

	FMapIndustryTagSpec Mine(TArray<EResourceType> Resources, bool bWordOnly)
	{
		FMapIndustryTagSpec Spec;
		Spec.Kind = EMapIndustryTagKind::Mine;
		Spec.Resources = MoveTemp(Resources);
		Spec.bMineWordOnly = bWordOnly;
		return Spec;
	}

	FMapIndustryTagSpec Oil(bool bRefinery)
	{
		FMapIndustryTagSpec Spec;
		Spec.Kind = EMapIndustryTagKind::Oil;
		Spec.Resources = { EResourceType::Oil };
		Spec.bRefinery = bRefinery;
		return Spec;
	}

	FMapIndustryTagSpec Hub()
	{
		FMapIndustryTagSpec Spec;
		Spec.Kind = EMapIndustryTagKind::TradeHub;
		return Spec;
	}

	FString Describe(const TArray<FMapIndustryTagSpec>& Specs)
	{
		FString Out;
		for (const FMapIndustryTagSpec& Spec : Specs)
		{
			Out += FString::Printf(TEXT("[%s %s res%d word%d ref%d] "), *UEnum::GetValueAsString(Spec.Kind),
				*UEnum::GetValueAsString(Spec.Industry), Spec.Resources.Num(), Spec.bMineWordOnly ? 1 : 0, Spec.bRefinery ? 1 : 0);
		}
		return Out;
	}

	struct FExpectedTags
	{
		const TCHAR* RowName;
		FCountryInfoTable Row;
		TArray<FMapIndustryTagSpec> Tags;
	};

	TArray<FExpectedTags> ExpectedTable()
	{
		using ECT = ECompanyType;
		using ERT = EResourceType;
		return {
			{ TEXT("Korea"), MakeRow(ECountryType::Korea, true, false, false, { ECT::Electronics }, ECT::Electronics, {}),
				{ Factory(ECT::Electronics) } },
			{ TEXT("China"), MakeRow(ECountryType::China, true, false, false, { ECT::Electronics, ECT::Automobile }, ECT::Electronics,
				{}), { Factory(ECT::Electronics) } },
			{ TEXT("USA"), MakeRow(ECountryType::USA, false, false, true, {}, ECT::None,
				{}), { Hub() } },
			{ TEXT("Saudi"), MakeRow(ECountryType::Saudi, false, true, false, {}, ECT::None, { ERT::Oil }), { Oil(false) } },
			{ TEXT("Singapore"), MakeRow(ECountryType::Singapore, false, false, true, {}, ECT::None, {}), { Hub() } },
			// 광산만 있는 나라는 자원 이름을 늘어놓는다(석유·목재).
			{ TEXT("Canada"), MakeRow(ECountryType::Canada, false, true, false, {}, ECT::None, { ERT::Oil, ERT::Wood, ERT::Copper }),
				{ Mine({ ERT::Oil, ERT::Wood, ERT::Copper }, false) } },
		};
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverseasMapTagSpecsTest,
	"CGR.Overseas.Map.TagSpecs",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FOverseasMapTagSpecsTest::RunTest(const FString& Parameters)
{
	using namespace OverseasMapSimplifyTest;
	for (const FExpectedTags& Expected : ExpectedTable())
	{
		const TArray<FMapIndustryTagSpec> Got = MapIndustryTags::BuildSpecs(Expected.Row, false);
		TestTrue(FString::Printf(TEXT("%s 이름표 = %s (실제 %s)"), Expected.RowName, *Describe(Expected.Tags), *Describe(Got)),
			Got == Expected.Tags);
	}

	// 정유는 국가 이름이 아니라 외형 데이터(유전만 + 저장 3단계)가 정한다 ― 같은 사우디 행에 플래그만 바꿔 본다.
	const FCountryInfoTable Saudi = MakeRow(ECountryType::Saudi, false, true, false, {}, ECompanyType::None, { EResourceType::Oil });
	const TArray<FMapIndustryTagSpec> Refinery = MapIndustryTags::BuildSpecs(Saudi, true);
	TestTrue(TEXT("사우디 + 정유 외형 = 석유·정유 한 칸"), Refinery.Num() == 1 && Refinery[0] == Oil(true));

	// 섞인 광산에는 정유 플래그가 실리지 않는다(정유 탑은 유전만 있는 광산의 외형이다).
	const FCountryInfoTable Canada = MakeRow(ECountryType::Canada, false, true, false, {}, ECompanyType::None,
		{ EResourceType::Oil, EResourceType::Wood });
	const TArray<FMapIndustryTagSpec> Mixed = MapIndustryTags::BuildSpecs(Canada, true);
	TestTrue(TEXT("섞인 광산은 정유 표시가 없다"), Mixed.Num() == 1 && !Mixed[0].bRefinery && Mixed[0].Kind == EMapIndustryTagKind::Mine);

	// 지원 플래그가 꺼진 칸은 목록에 없다 ― 공장 플래그만 있고 산업이 비면 공장 칸도 없다(빈 칸 대신 생략).
	const FCountryInfoTable NoIndustry = MakeRow(ECountryType::Japan, true, false, false, {}, ECompanyType::None, {});
	TestEqual(TEXT("산업이 비면 공장 칸 없음"), MapIndustryTags::BuildSpecs(NoIndustry, false).Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverseasMapTagSpecsMatchDTTest,
	"CGR.Overseas.Map.TagSpecsMatchDT",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FOverseasMapTagSpecsMatchDTTest::RunTest(const FString& Parameters)
{
	using namespace OverseasMapSimplifyTest;
	// 위 손 행이 실제 DT 와 갈리면 시험이 초록인 채 화면만 틀린다 ― 실제 DT 행으로 한 번 더 돈다.
	const UDataTable* CountryTable = LoadObject<UDataTable>(nullptr,
		TEXT("/Game/CompanyGrowth/Table/WorldMap/DT_CountryInfo.DT_CountryInfo"));
	if (!CountryTable)
	{
		AddError(TEXT("DT_CountryInfo 를 읽지 못했습니다."));
		return false;
	}
	for (const FExpectedTags& Expected : ExpectedTable())
	{
		const FCountryInfoTable* Row = CountryTable->FindRow<FCountryInfoTable>(FName(Expected.RowName), TEXT("OverseasMapSimplifyTest"));
		if (!TestNotNull(FString::Printf(TEXT("DT 에 %s 행이 있다"), Expected.RowName), Row))
		{
			continue;
		}
		const TArray<FMapIndustryTagSpec> Got = MapIndustryTags::BuildSpecs(*Row, false);
		TestTrue(FString::Printf(TEXT("DT %s 이름표 = %s (실제 %s)"), Expected.RowName, *Describe(Expected.Tags), *Describe(Got)),
			Got == Expected.Tags);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverseasMapTagWordsTest,
	"CGR.Overseas.Map.TagWords",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FOverseasMapTagWordsTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("광산 단어"), MapIndustryTags::MineWord().ToString(), FString(TEXT("광산")));
	TestEqual(TEXT("석유"), MapIndustryTags::OilWord(false).ToString(), FString(TEXT("석유")));
	TestEqual(TEXT("석유·정유"), MapIndustryTags::OilWord(true).ToString(), FString(TEXT("석유·정유")));
	TestEqual(TEXT("무역항"), MapIndustryTags::TradeHubWord().ToString(), FString(TEXT("무역항")));
	TestEqual(TEXT("자원 이름은 가운뎃점으로 잇는다"),
		MapIndustryTags::JoinNames({ FText::FromString(TEXT("석유")), FText::FromString(TEXT("목재")) }).ToString(),
		FString(TEXT("석유·목재")));
	TestTrue(TEXT("유전만 = 석유"), MapIndustryTags::IsAllOil({ EResourceType::Oil }));
	TestFalse(TEXT("섞이면 석유 아님"), MapIndustryTags::IsAllOil({ EResourceType::Oil, EResourceType::Wood }));
	TestFalse(TEXT("빈 목록은 석유 아님"), MapIndustryTags::IsAllOil({}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverseasMapBubbleOffsetTest,
	"CGR.Overseas.Map.BubbleOffset",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FOverseasMapBubbleOffsetTest::RunTest(const FString& Parameters)
{
	// 미니어처: 찍은 그림 300x200, 땅 조각 중심 (150, 140) → 판 위 표시 0.5배 → 앵커 위로 70 솟는다.
	FDioramaStillInfo Info;
	Info.SizePx = FIntPoint(300, 200);
	Info.PivotPx = FVector2D(150.0, 140.0);
	const float PivotY = static_cast<float>(Info.PivotPx.Y) * UCountryNameWidget::kDioMiniatureDisplayScale;
	const float MiniatureHeight = MapBubbles::HeightAboveAnchor(true, PivotY, 0.0f);
	TestEqual(TEXT("미니어처 윗변 = 앵커 위 70"), MiniatureHeight, 70.0f, 1e-3f);
	const FVector2D MiniatureOffset = MapBubbles::RowOffset(MiniatureHeight);
	TestTrue(FString::Printf(TEXT("말풍선 줄 바닥 = 미니어처 윗변 + 6 (실제 %.2f)"), MiniatureOffset.Y),
		MiniatureOffset.Equals(FVector2D(0.0, -64.0), 1e-3));

	// 원반(88)이면 반지름 44 위에 얹힌다.
	const float DiscHeight = MapBubbles::HeightAboveAnchor(false, 999.0f, 88.0f);
	TestEqual(TEXT("원반은 반지름만큼"), DiscHeight, 44.0f, 1e-3f);
	TestTrue(TEXT("원반 위 말풍선 줄 바닥 = -38"), MapBubbles::RowOffset(DiscHeight).Equals(FVector2D(0.0, -38.0), 1e-3));

	// 음수 입력(레이아웃 전 0 크기 등)이 말풍선을 앵커 아래로 내리지 않는다.
	TestTrue(TEXT("음수 피벗은 0 으로"), MapBubbles::HeightAboveAnchor(true, -5.0f, 0.0f) == 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverseasMapClaimAllLabelTest,
	"CGR.Overseas.Map.ClaimAllLabel",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FOverseasMapClaimAllLabelTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("모두 받기 라벨"), MapBubbles::ClaimAllLabel(3).ToString(), FString(TEXT("모두 받기 3")));
	TestEqual(TEXT("받기 말풍선 = 「받기 」 + 개수"), MapBubbles::ClaimLabel(12).ToString(), FString(TEXT("받기 12")));
	TestEqual(TEXT("음수 개수는 0"), MapBubbles::ClaimLabel(-2).ToString(), FString(TEXT("받기 0")));
	const FString Sell = MapBubbles::SellLabel(120000000).ToString();
	TestTrue(FString::Printf(TEXT("판매 말풍선은 「판매 」 + 금액 (실제 %s)"), *Sell),
		Sell.StartsWith(TEXT("판매 ")) && Sell.Len() > 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverseasMapClaimAllVisibilityTest,
	"CGR.Overseas.Map.ClaimAllVisibility",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FOverseasMapClaimAllVisibilityTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("받을 것 3 · 서랍 닫힘 = 보인다"), MapBubbles::ShouldShowClaimAll(3, false));
	TestFalse(TEXT("받을 것 3 · 서랍 열림 = 숨긴다"), MapBubbles::ShouldShowClaimAll(3, true));
	TestFalse(TEXT("받을 것 0 · 서랍 닫힘 = 숨긴다"), MapBubbles::ShouldShowClaimAll(0, false));
	TestEqual(TEXT("받기 말풍선 라벨"), MapBubbles::ClaimLabel(2).ToString(), FString(TEXT("받기 2")));
	TestEqual(TEXT("모두 받기 버튼 글자"), MapBubbles::ClaimAllButtonLabel().ToString(), FString(TEXT("모두 받기")));
	TestEqual(TEXT("모두 받기 개수 칩"), MapBubbles::ClaimAllCountLabel(3).ToString(), FString(TEXT("3곳")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverseasMapFullScreenFocusTest,
	"CGR.Overseas.Map.FullScreenFocus",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FOverseasMapFullScreenFocusTest::RunTest(const FString& Parameters)
{
	const FVector2D Board = UWorldMapPanelWidget::DesignBoardSize();
	TestTrue(TEXT("대륙 사각형 = 2400x976"), Board.Equals(FVector2D(2400.0, 976.0), 1e-6));
	// 대륙 텍스처(2048x833)와 비율이 0.5% 안이어야 마스크·앵커가 늘어나 보이지 않는다.
	TestTrue(TEXT("대륙 텍스처와 같은 비율"), FMath::Abs(Board.X / Board.Y - 2048.0 / 833.0) / (2048.0 / 833.0) < 0.005);

	// 기본 인자(원점 0·덮개 1)는 이전 판 수식과 같은 답을 낸다.
	{
		const FSlateRect View(FVector2D::ZeroVector, Board);
		const FVector2D Anchor(0.886667, 0.286885);
		const FVector2D Point = Anchor * Board;
		FVector2D OldT, NewT;
		float OldS = 0.0f, NewS = 0.0f;
		UWorldMapPanelWidget::ComputeFocusTransform(Anchor, Board, View, FSlateRect(Point, Point), OldT, OldS);
		UWorldMapPanelWidget::ComputeFocusTransform(Anchor, Board, View, FSlateRect(Point, Point), NewT, NewS,
			FVector2D::ZeroVector, FVector2D(1.0, 1.0));
		TestTrue(TEXT("기본 인자 = 명시 기본값"), OldT.Equals(NewT, 1e-6) && FMath::IsNearlyEqual(OldS, NewS));
	}

	// 화면 가득: 판은 가로 가운데 + 상단 바 절반만큼 아래, 바다는 판 중심 기준 (1.6, 1.966867) 배.
	const FVector2D SeaScale(1.6, 1.966867);
	const double TopBar = UWorldMapPanelWidget::MapTopBarHeight;
	for (const double ScreenWidth : { 2304.0, 2560.0, 3120.0 })
	{
		const FVector2D Screen(ScreenWidth, 1440.0);
		const FVector2D Origin((Screen.X - Board.X) * 0.5, (Screen.Y - Board.Y) * 0.5 + TopBar * 0.5);
		const FVector2D Center = Board * 0.5;

		// 쉬는 상태(변형 없음)에서 바다가 화면을 덮는다.
		const FVector2D SeaMin = Origin + Center - Center * SeaScale;
		const FVector2D SeaMax = Origin + Center + Center * SeaScale;
		TestTrue(FString::Printf(TEXT("%.0f: 쉬는 판의 바다가 화면을 덮는다"), ScreenWidth),
			SeaMin.X <= 0.0 && SeaMin.Y <= 0.0 && SeaMax.X >= Screen.X && SeaMax.Y >= Screen.Y);

		for (const bool bDrawer : { false, true })
		{
			const FSlateRect Visible(FVector2D(0.0, TopBar), FVector2D(bDrawer ? Screen.X - 1160.0 : Screen.X, Screen.Y));
			for (const FVector2D Anchor : { FVector2D(0.07, 0.15), FVector2D(0.886667, 0.286885), FVector2D(0.95, 0.327869),
				FVector2D(0.913333, 0.811475), FVector2D(0.55, 0.852459) })
			{
				const FVector2D Point = Anchor * Board;
				const FSlateRect Bounds(Point - FVector2D(90.0, 110.0), Point + FVector2D(90.0, 120.0));
				FVector2D Translation;
				float Scale = 0.0f;
				UWorldMapPanelWidget::ComputeFocusTransform(Anchor, Board, Visible, Bounds, Translation, Scale, Origin, SeaScale);
				const auto ToScreen = [&](const FVector2D& P) { return Origin + Center + (P - Center) * Scale + Translation; };
				const FVector2D PinMin = ToScreen(FVector2D(Bounds.Left, Bounds.Top));
				const FVector2D PinMax = ToScreen(FVector2D(Bounds.Right, Bounds.Bottom));
				const FVector2D CoverMin = Origin + Center - Center * SeaScale * Scale + Translation;
				const FVector2D CoverMax = Origin + Center + Center * SeaScale * Scale + Translation;
				const FString Label = FString::Printf(TEXT("%.0f%s (%.2f,%.2f)"), ScreenWidth, bDrawer ? TEXT(" 서랍") : TEXT(""), Anchor.X, Anchor.Y);
				TestTrue(FString::Printf(TEXT("%s: 선택 국가가 상단 바 아래 가시 영역 안"), *Label),
					PinMin.X >= Visible.Left + 15.9 && PinMin.Y >= Visible.Top + 15.9
					&& PinMax.X <= Visible.Right - 15.9 && PinMax.Y <= Visible.Bottom - 15.9);
				TestTrue(FString::Printf(TEXT("%s: 확대해도 바다가 가시 영역을 덮는다"), *Label),
					CoverMin.X <= Visible.Left + 0.1 && CoverMin.Y <= Visible.Top + 0.1
					&& CoverMax.X >= Visible.Right - 0.1 && CoverMax.Y >= Visible.Bottom - 0.1);
			}
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
