#include "Misc/AutomationTest.h"
#include "UI/Element/WorldMap/TradeRouteWidget.h"
#include "UI/Panel/WorldMapPanelWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FProductionRouteInput OverseasRouteRow(ECountryType Country, double X, double Y, bool bFactory, bool bHub)
	{
		FProductionRouteInput Row;
		Row.Country = Country;
		Row.Anchor = FVector2D(X, Y);
		Row.bFactory = bFactory;
		Row.bHub = bHub;
		Row.bUnlocked = true;
		return Row;
	}

	// DT_CountryInfo_Import.csv 의 MapAnchor·bSupportsFactory·bIsTradeHub. 미국·캐나다는 1차 계획이 옮긴 자리 ―
	// 옛 미국 자리(0.14, 0.279)면 판 픽셀 거리로 독일이 싱가포르와 짝이 된다.
	TArray<FProductionRouteInput> OverseasRouteTable()
	{
		return {
			OverseasRouteRow(ECountryType::Korea,       0.886667, 0.286885, true,  false),
			OverseasRouteRow(ECountryType::China,       0.803333, 0.319672, true,  false),
			OverseasRouteRow(ECountryType::Japan,       0.950000, 0.327869, true,  false),
			OverseasRouteRow(ECountryType::Germany,     0.500000, 0.188525, true,  false),
			OverseasRouteRow(ECountryType::USA,         0.178000, 0.370000, true,  true),
			OverseasRouteRow(ECountryType::SouthAfrica, 0.550000, 0.852459, false, false),
			OverseasRouteRow(ECountryType::Australia,   0.913333, 0.811475, false, false),
			OverseasRouteRow(ECountryType::Canada,      0.070000, 0.150000, false, false),
			OverseasRouteRow(ECountryType::Brazil,      0.296667, 0.696721, false, false),
			OverseasRouteRow(ECountryType::Saudi,       0.620000, 0.418033, false, false),
			OverseasRouteRow(ECountryType::Singapore,   0.813333, 0.606557, false, true),
		};
	}

	void OverseasRouteSetUnlocked(TArray<FProductionRouteInput>& Table, ECountryType Country, bool bUnlocked)
	{
		for (FProductionRouteInput& Row : Table)
		{
			if (Row.Country == Country)
			{
				Row.bUnlocked = bUnlocked;
			}
		}
	}

	const FProductionRouteInput* OverseasRouteFind(const TArray<FProductionRouteInput>& Table, ECountryType Country)
	{
		return Table.FindByPredicate([Country](const FProductionRouteInput& Row) { return Row.Country == Country; });
	}

	ECountryType OverseasRouteHubOf(const TArray<FProductionRoute>& Found, ECountryType From)
	{
		for (const FProductionRoute& Route : Found)
		{
			if (Route.From == From)
			{
				return Route.Hub;
			}
		}
		return ECountryType::None;
	}

	TArray<FProductionRoute> OverseasRouteResolve(const TArray<FProductionRouteInput>& Table)
	{
		return UTradeRouteWidget::ResolveProductionRoutes(Table, UWorldMapPanelWidget::DesignBoardSize());
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverseasRouteNearestHubTest, "CGR.Overseas.Route.NearestHub",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FOverseasRouteNearestHubTest::RunTest(const FString& Parameters)
{
	const TArray<FProductionRouteInput> Table = OverseasRouteTable();
	const TArray<FProductionRoute> Found = OverseasRouteResolve(Table);

	TestEqual(TEXT("항로 수 = 무역항이 아닌 공장 나라 4곳"), Found.Num(), 4);
	TestEqual(TEXT("한국 → 싱가포르"), OverseasRouteHubOf(Found, ECountryType::Korea), ECountryType::Singapore);
	TestEqual(TEXT("중국 → 싱가포르"), OverseasRouteHubOf(Found, ECountryType::China), ECountryType::Singapore);
	TestEqual(TEXT("일본 → 싱가포르"), OverseasRouteHubOf(Found, ECountryType::Japan), ECountryType::Singapore);
	TestEqual(TEXT("독일 → 미국"), OverseasRouteHubOf(Found, ECountryType::Germany), ECountryType::USA);

	const FProductionRouteInput* KoreaRow = OverseasRouteFind(Table, ECountryType::Korea);
	const FProductionRouteInput* SingaporeRow = OverseasRouteFind(Table, ECountryType::Singapore);
	for (const FProductionRoute& Route : Found)
	{
		if (Route.From == ECountryType::Korea && KoreaRow && SingaporeRow)
		{
			TestTrue(TEXT("출발 앵커 = 한국 앵커"), Route.FromAnchor.Equals(KoreaRow->Anchor, 1e-9));
			TestTrue(TEXT("도착 앵커 = 싱가포르 앵커"), Route.HubAnchor.Equals(SingaporeRow->Anchor, 1e-9));
		}
	}

	// 수평 현 480px → 분할 20, 가운데 점(t=0.5) = 중점에서 위로 거리 × 0.22 / 2 = 52.8px.
	const FVector2D Start(0.0, 100.0);
	const FVector2D End(480.0, 100.0);
	const TArray<FVector2D> Curve = UTradeRouteWidget::BuildLiftedCurvePoints(Start, End, 0.22);
	TestEqual(TEXT("곡선 점 수 = 분할 20 + 1"), Curve.Num(), 21);
	if (Curve.Num() == 21)
	{
		TestTrue(TEXT("곡선 시작 = 출발점"), Curve[0].Equals(Start, 1e-6));
		TestTrue(TEXT("곡선 끝 = 도착점"), Curve.Last().Equals(End, 1e-6));
		TestTrue(FString::Printf(TEXT("가운데 점이 위로 솟는다 (%.3f, %.3f) ≈ (240, 47.2)"), Curve[10].X, Curve[10].Y),
			Curve[10].Equals(FVector2D(240.0, 47.2), 1e-6));
	}
	const TArray<FVector2D> Reverse = UTradeRouteWidget::BuildLiftedCurvePoints(End, Start, 0.22);
	TestTrue(TEXT("방향을 뒤집어도 위로 솟는다"), Reverse.Num() == 21 && Reverse[10].Equals(FVector2D(240.0, 47.2), 1e-6));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverseasRouteSkipsLockedAndHubsTest, "CGR.Overseas.Route.RouteSkipsLockedAndHubs",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FOverseasRouteSkipsLockedAndHubsTest::RunTest(const FString& Parameters)
{
	{
		const TArray<FProductionRoute> Found = OverseasRouteResolve(OverseasRouteTable());
		TestEqual(TEXT("공장 겸 무역항(미국)은 출발지가 아니다"), OverseasRouteHubOf(Found, ECountryType::USA), ECountryType::None);
		TestEqual(TEXT("무역항(싱가포르)은 출발지가 아니다"), OverseasRouteHubOf(Found, ECountryType::Singapore), ECountryType::None);
		TestEqual(TEXT("광산만 있는 캐나다는 출발지가 아니다"), OverseasRouteHubOf(Found, ECountryType::Canada), ECountryType::None);
		TestEqual(TEXT("광산만 있는 사우디는 출발지가 아니다"), OverseasRouteHubOf(Found, ECountryType::Saudi), ECountryType::None);
	}

	{
		TArray<FProductionRouteInput> Table = OverseasRouteTable();
		OverseasRouteSetUnlocked(Table, ECountryType::Germany, false);
		const TArray<FProductionRoute> Found = OverseasRouteResolve(Table);
		TestEqual(TEXT("잠긴 독일은 항로 없음"), OverseasRouteHubOf(Found, ECountryType::Germany), ECountryType::None);
		TestEqual(TEXT("잠긴 독일을 뺀 3곳"), Found.Num(), 3);
	}

	{
		TArray<FProductionRouteInput> Table = OverseasRouteTable();
		OverseasRouteSetUnlocked(Table, ECountryType::Singapore, false);
		const TArray<FProductionRoute> Found = OverseasRouteResolve(Table);
		TestEqual(TEXT("싱가포르가 잠기면 공장 4곳 모두 미국으로"), Found.Num(), 4);
		for (const FProductionRoute& Route : Found)
		{
			TestEqual(FString::Printf(TEXT("%d 출발 → 잠금 해제된 미국"), static_cast<int32>(Route.From)), Route.Hub, ECountryType::USA);
		}
		const FProductionRouteInput* SingaporeRow = OverseasRouteFind(Table, ECountryType::Singapore);
		TestTrue(TEXT("잠긴 싱가포르에는 무역항 점이 없다"), SingaporeRow && !UTradeRouteWidget::IsProductionHub(*SingaporeRow));
	}

	{
		// 게임 시작 = 한국만 풀림. 무역항이 없으니 항로도 점도 0.
		TArray<FProductionRouteInput> Table = OverseasRouteTable();
		for (FProductionRouteInput& Row : Table)
		{
			Row.bUnlocked = (Row.Country == ECountryType::Korea);
		}
		TestEqual(TEXT("한국만 풀린 시작 구간 = 항로 0개"), OverseasRouteResolve(Table).Num(), 0);
		int32 HubMarkers = 0;
		for (const FProductionRouteInput& Row : Table)
		{
			HubMarkers += UTradeRouteWidget::IsProductionHub(Row) ? 1 : 0;
		}
		TestEqual(TEXT("한국만 풀린 시작 구간 = 무역항 점 0개"), HubMarkers, 0);
	}

	return true;
}

#endif
