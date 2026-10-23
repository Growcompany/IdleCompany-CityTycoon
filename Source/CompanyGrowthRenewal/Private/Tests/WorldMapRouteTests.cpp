#include "Misc/AutomationTest.h"
#include "UI/Element/WorldMap/TradeRouteWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldMapRouteResolveTest,
	"CGR.WorldMap.RouteResolve",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FWorldMapRouteResolveTest::RunTest(const FString& Parameters)
{
	// 출발 = 가동 라인 최다, 도착 = 수요 비율 최고
	const TArray<FCountryProduction> Producers = {
		{ ECountryType::Korea, 3 }, { ECountryType::Japan, 1 }, { ECountryType::China, 2 } };
	const TArray<FCountryDemand> Demands = {
		{ ECountryType::USA, 0.91f }, { ECountryType::Germany, 0.40f }, { ECountryType::Brazil, 0.62f } };

	const FTradeRoute R = UTradeRouteWidget::ResolveRoute(ECompanyType::Semiconductor, Producers, Demands);
	TestTrue (TEXT("유효"),   R.bValid);
	TestEqual(TEXT("출발=라인 최다"), R.From, ECountryType::Korea);
	TestEqual(TEXT("도착=수요 최고"), R.To,   ECountryType::USA);

	// 동률은 enum 값이 작은 쪽 ― 프레임마다 선이 바뀌지 않도록 결정론을 고정한다
	const TArray<FCountryProduction> Tie = { { ECountryType::Japan, 2 }, { ECountryType::China, 2 } };
	TestEqual(TEXT("생산 동률은 enum 작은 쪽"),
		UTradeRouteWidget::ResolveRoute(ECompanyType::Electronics, Tie, Demands).From, ECountryType::China);

	// 후보가 비면 무효 ― 판 밖으로 뻗는 선을 만들지 않는다
	TestFalse(TEXT("생산국 없음"),
		UTradeRouteWidget::ResolveRoute(ECompanyType::Automobile, {}, Demands).bValid);
	TestFalse(TEXT("수요국 없음"),
		UTradeRouteWidget::ResolveRoute(ECompanyType::Automobile, Producers, {}).bValid);

	// 출발=도착이면 무효 ― 길이 0 베지어 방지
	const TArray<FCountryDemand> SelfDemand = { { ECountryType::Korea, 0.99f } };
	TestFalse(TEXT("출발=도착"),
		UTradeRouteWidget::ResolveRoute(ECompanyType::Semiconductor, Producers, SelfDemand).bValid);

	// ★ 가동 라인 0 = 그 산업 공장이 없는 나라. 지원 산업 목록에만 올라 있고 실제로는 놀고 있는 나라에서
	//   선이 출발하면 판이 "여기서 만들고 있다"고 거짓말한다.
	const TArray<FCountryProduction> Idle = { { ECountryType::Korea, 0 }, { ECountryType::China, 0 } };
	TestFalse(TEXT("가동 라인 0 뿐이면 무효"),
		UTradeRouteWidget::ResolveRoute(ECompanyType::Semiconductor, Idle, Demands).bValid);

	const TArray<FCountryProduction> OneRunning = { { ECountryType::Korea, 0 }, { ECountryType::Japan, 1 } };
	TestEqual(TEXT("놀고 있는 나라를 건너뛰고 가동 중인 나라에서 출발"),
		UTradeRouteWidget::ResolveRoute(ECompanyType::Semiconductor, OneRunning, Demands).From, ECountryType::Japan);

	// 산업 미설정은 무효 ― 주문의 CompanyType 이 비어 있으면 어느 산업의 경로인지 말할 수 없다
	TestFalse(TEXT("산업 미설정"),
		UTradeRouteWidget::ResolveRoute(ECompanyType::None, Producers, Demands).bValid);

	// ★★ 생산국이 수요 1등이어도 항로가 생겨야 한다.
	//   오프닝에는 모든 시장 셀이 만수요(1.0)라 전 국가가 동률이고, 출발지를 도착 후보에서 빼지 않으면
	//   1등이 출발지 자신이 되어 주문이 통째로 버려진다. 한국만 해금된 구간에서는 그 결과가
	//   활성 0 · 점선 0 ― 게임 내내 판에 선이 한 줄도 없는 상태다.
	const TArray<FCountryProduction> KoreaOnly = { { ECountryType::Korea, 2 } };
	const TArray<FCountryDemand> AllFullDemand = {
		{ ECountryType::Korea, 1.0f }, { ECountryType::China, 1.0f }, { ECountryType::Japan, 1.0f } };
	const FTradeRoute Opening = UTradeRouteWidget::ResolveRoute(ECompanyType::Semiconductor, KoreaOnly, AllFullDemand);
	TestTrue (TEXT("만수요 동률에서도 항로가 생긴다"), Opening.bValid);
	TestEqual(TEXT("출발은 유일한 생산국"), Opening.From, ECountryType::Korea);
	TestTrue (TEXT("도착은 출발지가 아니다"), Opening.To != Opening.From);

	// 생산국이 수요 목록의 맨 앞(= 그냥 고르면 1등이 되는 자리)에 있어도 마찬가지다.
	const TArray<FCountryDemand> ProducerFirst = {
		{ ECountryType::Korea, 1.0f }, { ECountryType::USA, 0.30f } };
	const FTradeRoute Skipped = UTradeRouteWidget::ResolveRoute(ECompanyType::Semiconductor, KoreaOnly, ProducerFirst);
	TestTrue (TEXT("출발지를 건너뛰고 차선을 고른다"), Skipped.bValid);
	TestEqual(TEXT("차선이 도착지가 된다"), Skipped.To, ECountryType::USA);
	return true;
}

// ★ 타이브레이크 ― 동률 구간은 게임 초반 내내다. 여기서 enum 순서로 고르면 화면이 "이 나라가 수요 1등"
//   이라고 말하지만 실제로는 수익이 같다. 의미 있는 축(PriceMul)이 먼저 가르는지 못 박는다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldMapRouteTiebreakTest,
	"CGR.WorldMap.RouteTiebreak",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FWorldMapRouteTiebreakTest::RunTest(const FString& Parameters)
{
	const TArray<FCountryProduction> Producers = { { ECountryType::Korea, 2 } };

	// 수요가 같으면 실제로 더 비싸게 팔리는 쪽. enum 순서(China 2 < Japan 3)를 일부러 거스르게 배치했다 ―
	// enum 으로 고르면 China 가 나오므로 이 단언이 두 규칙을 갈라 준다.
	const TArray<FCountryDemand> SameRatio = {
		{ ECountryType::China, 1.0f, 0.75f },
		{ ECountryType::Japan, 1.0f, 1.40f } };
	TestEqual(TEXT("수요 동률이면 PriceMul 높은 쪽"),
		UTradeRouteWidget::ResolveRoute(ECompanyType::Electronics, Producers, SameRatio).To, ECountryType::Japan);

	// 1순위는 어디까지나 수요다 ― PriceMul 이 높아도 수요가 낮으면 진다.
	const TArray<FCountryDemand> RatioWins = {
		{ ECountryType::China, 0.90f, 0.75f },
		{ ECountryType::Japan, 0.40f, 1.40f } };
	TestEqual(TEXT("수요가 다르면 수요가 이긴다"),
		UTradeRouteWidget::ResolveRoute(ECompanyType::Electronics, Producers, RatioWins).To, ECountryType::China);

	// 둘 다 같으면 enum ― 의미 축이 아니라 "같은 입력에 같은 답" 을 보장하는 최후 장치다.
	const TArray<FCountryDemand> FullTie = {
		{ ECountryType::Japan, 1.0f, 1.0f },
		{ ECountryType::China, 1.0f, 1.0f } };
	const ECountryType FirstAnswer = UTradeRouteWidget::ResolveRoute(ECompanyType::Electronics, Producers, FullTie).To;
	TestEqual(TEXT("완전 동률은 결정론적"), FirstAnswer,
		UTradeRouteWidget::ResolveRoute(ECompanyType::Electronics, Producers, FullTie).To);
	TestTrue(TEXT("완전 동률이어도 유효한 도착지가 나온다"), FirstAnswer != ECountryType::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldMapRouteFilterTest,
	"CGR.WorldMap.RouteFilter",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FWorldMapRouteFilterTest::RunTest(const FString& Parameters)
{
	auto MakeOrder = [](ECompanyType Industry, bool bAccepted)
	{
		FTradeOrder O;
		O.CompanyType = Industry;
		O.bAccepted   = bAccepted;
		return O;
	};

	TArray<FTradeOrder> Orders;
	Orders.Add(MakeOrder(ECompanyType::Semiconductor, true));   // 수락 ― 그린다
	Orders.Add(MakeOrder(ECompanyType::Electronics,   false));  // 미수락 ― 안 그린다
	Orders.Add(MakeOrder(ECompanyType::None,          true));   // 산업 미설정 ― 안 그린다

	auto GetProducers = [](ECompanyType Industry) -> TArray<FCountryProduction>
	{
		if (Industry == ECompanyType::Semiconductor) { return { { ECountryType::Korea, 3 } }; }
		if (Industry == ECompanyType::Electronics)   { return { { ECountryType::Japan, 2 } }; }
		return {};
	};
	auto GetDemands = [](ECompanyType Industry) -> TArray<FCountryDemand>
	{
		if (Industry == ECompanyType::Semiconductor) { return { { ECountryType::USA, 0.8f } }; }
		if (Industry == ECompanyType::Electronics)   { return { { ECountryType::China, 0.7f } }; }
		return {};
	};

	const TArray<FTradeRoute> Routes = UTradeRouteWidget::BuildRoutes(Orders, GetProducers, GetDemands);
	TestEqual(TEXT("수락+해결 가능 1건만"), Routes.Num(), 1);
	TestEqual(TEXT("출발"), Routes[0].From, ECountryType::Korea);
	TestEqual(TEXT("도착"), Routes[0].To,   ECountryType::USA);

	// ★ 비활성(점선) = 아직 수락하지 않은 주문. 활성과 정확히 반대편만 가져간다 ―
	//   한쪽이 상대의 여집합이 아니면 같은 주문이 파란 선과 점선으로 동시에 그려진다.
	const TArray<FTradeRoute> Idle = UTradeRouteWidget::BuildIdleRoutes(Orders, GetProducers, GetDemands);
	TestEqual(TEXT("미수락+해결 가능 1건만"), Idle.Num(), 1);
	TestEqual(TEXT("점선 출발"), Idle[0].From, ECountryType::Japan);
	TestEqual(TEXT("점선 도착"), Idle[0].To,   ECountryType::China);
	TestFalse(TEXT("수락한 주문은 점선에 없다"),
		UTradeRouteWidget::ContainsPair(Idle, ECountryType::Korea, ECountryType::USA));

	// ★ 같은 쌍을 두 번 그리면 알파가 겹쳐 더 진해진다 ― 화면이 항로 수를 거짓으로 말한다.
	TArray<FTradeOrder> Doubled;
	Doubled.Add(MakeOrder(ECompanyType::Semiconductor, true));
	Doubled.Add(MakeOrder(ECompanyType::Semiconductor, true));
	TestEqual(TEXT("같은 쌍 두 주문은 선 하나"),
		UTradeRouteWidget::BuildRoutes(Doubled, GetProducers, GetDemands).Num(), 1);

	// 방향 무관 비교 ― 점선을 파란 선 위에 겹쳐 그리지 않으려면 왕복도 같은 쌍으로 봐야 한다.
	TestTrue(TEXT("역방향도 같은 쌍"),
		UTradeRouteWidget::ContainsPair(Routes, ECountryType::USA, ECountryType::Korea));
	TestFalse(TEXT("무관한 쌍은 없다"),
		UTradeRouteWidget::ContainsPair(Routes, ECountryType::USA, ECountryType::Japan));
	return true;
}

// ★ 분할 수의 **실제** 계약을 못 박는다. 미터 한계를 여기서 지켜 주는 게 아니다 ―
//   이차 베지어의 최악 꺾임각이 18.2° 라 76.5° 한계에서 멀고, 굵기 13px 에 필요한 최소 세그먼트가
//   2.4px 라 본선 길이 구간은 애초에 안전하다. 여기서 지키는 건 하한 2(0 나눗셈)와 상한 32(드로우 예산)다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldMapRouteCurveTest,
	"CGR.WorldMap.RouteCurve",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FWorldMapRouteCurveTest::RunTest(const FString& Parameters)
{
	// 48px 이상에서만 "간격 24px 이상" 이 성립한다. 그 아래는 하한 2 가 이긴다.
	for (const double SampleLength : { 48.0, 120.0, 400.0, 824.6, 1500.0 })
	{
		const int32 Segments = UTradeRouteWidget::ResolveSegmentCount(SampleLength);
		TestTrue(TEXT("48px 이상이면 간격 24px 이상"), SampleLength / Segments >= 24.0 - 1e-9);
	}

	// ★ 48px 미만 구간의 **실제** 동작을 적는다 ― 주석이 보장하지 못하는 것을 보장한다고 쓰지 않기 위해서다.
	TestEqual(TEXT("48px 미만은 하한 2 가 이긴다"), UTradeRouteWidget::ResolveSegmentCount(30.0), 2);
	TestTrue(TEXT("그 구간의 간격은 24px 아래다"), 30.0 / 2.0 < 24.0);

	// 아주 짧아도 분할 수가 0/1 로 떨어지면 BuildCurvePoints 가 0 으로 나눈다.
	TestTrue(TEXT("길이 0 이어도 분할 수 하한"), UTradeRouteWidget::ResolveSegmentCount(0.0) >= 2);
	TestTrue(TEXT("긴 항로도 분할 수 상한"), UTradeRouteWidget::ResolveSegmentCount(100000.0) <= 32);

	// 곡선의 양 끝은 넘긴 끝점 그대로여야 한다 ― 어긋나면 선이 메달리온에 안 꽂힌다.
	const FVector2D Start(100.0, 200.0);
	const FVector2D End(900.0, 400.0);
	const TArray<FVector2D> Curve = UTradeRouteWidget::BuildCurvePoints(Start, End, 0.16);
	TestTrue(TEXT("곡선 점 2개 이상"), Curve.Num() >= 3);
	TestTrue(TEXT("시작점 일치"), Curve[0].Equals(Start, 1e-6));
	TestTrue(TEXT("끝점 일치"), Curve.Last().Equals(End, 1e-6));

	// 휨은 진행 방향에 묶인다 ― 왕복 항로가 한 줄로 뭉치면 두 방향을 구별할 수 없다.
	const TArray<FVector2D> Reverse = UTradeRouteWidget::BuildCurvePoints(End, Start, 0.16);
	TestTrue(TEXT("역방향은 반대쪽으로 휜다"), !Curve[Curve.Num() / 2].Equals(Reverse[Reverse.Num() / 2], 1.0));

	// 휨 0 이면 직선 ― 중간점이 현의 중점과 같다.
	const TArray<FVector2D> Straight = UTradeRouteWidget::BuildCurvePoints(Start, End, 0.0);
	TestTrue(TEXT("휨 0 은 직선"),
		Straight[Straight.Num() / 2].Equals((Start + End) * 0.5, 1e-3));

	// 길이 0 은 점을 만들지 않는다 ― 빈 폴리라인이면 드로잉이 통째로 건너뛴다.
	TestEqual(TEXT("길이 0 곡선은 비어 있다"),
		UTradeRouteWidget::BuildCurvePoints(Start, Start, 0.16).Num(), 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
