#include "Misc/AutomationTest.h"
#include "UI/Element/Common/IncomeStreamGate.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	// InGameLayerWidget 의 기본 노브와 같은 값 — 테스트가 노브 변경을 눈치채게 한다
	constexpr float NearSize = 64.0f;
	constexpr float FarSize = 42.0f;
	constexpr float NearZoom = 0.48f;
	constexpr float FarZoom = 0.90f;

	float CoinSize(float Zoom)
	{
		return ResolveStreamCoinDrawSize(Zoom, NearSize, FarSize, NearZoom, FarZoom);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRStreamCoinDrawSizeTest,
	"CGR.IncomePopup.CoinDrawSize",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRStreamCoinDrawSizeTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("줌인 경계 이하는 Near 고정"), CoinSize(0.0f), NearSize);
	TestEqual(TEXT("Near 경계 정확히는 Near"), CoinSize(NearZoom), NearSize);
	TestEqual(TEXT("Far 경계 정확히는 Far"), CoinSize(FarZoom), FarSize);
	TestEqual(TEXT("최대 줌아웃 초과도 Far 유지"), CoinSize(1.0f), FarSize);
	TestEqual(TEXT("구간 중앙은 두 값의 평균"), CoinSize((NearZoom + FarZoom) * 0.5f), 53.0f);

	// 방향이 뒤집히면 줌아웃에서 화면이 코인으로 덮인다 — 단조 감소를 직접 건다
	TestTrue(TEXT("줌아웃할수록 작아진다"), CoinSize(0.60f) > CoinSize(0.75f));

	// 작아지는 쪽으로 넘어지면 줌인 뷰의 코인이 조용히 쪼그라든다
	TestEqual(TEXT("NaN 은 Near 로 폴백"), CoinSize(std::numeric_limits<float>::quiet_NaN()), NearSize);
	TestEqual(TEXT("무한대도 Near 로 폴백"), CoinSize(std::numeric_limits<float>::infinity()), NearSize);

	TestEqual(TEXT("구간 폭 0 은 Near 로 폴백"),
		ResolveStreamCoinDrawSize(0.7f, NearSize, FarSize, 0.5f, 0.5f), NearSize);
	TestEqual(TEXT("뒤집힌 구간은 Near 로 폴백"),
		ResolveStreamCoinDrawSize(0.7f, NearSize, FarSize, 0.9f, 0.4f), NearSize);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRIncomePopupSourceVisibilityTest,
	"CGR.IncomePopup.SourceVisibility",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRIncomePopupSourceVisibilityTest::RunTest(const FString& Parameters)
{
	const FVector2D Canvas(1920.0, 1080.0);

	TestTrue(TEXT("화면 중앙은 보임"), IsIncomePopupSourceVisible(FVector2D(960.0, 540.0), Canvas));
	TestTrue(TEXT("좌상단 꼭지점 포함"), IsIncomePopupSourceVisible(FVector2D(0.0, 0.0), Canvas));
	TestTrue(TEXT("우하단 꼭지점 포함"), IsIncomePopupSourceVisible(Canvas, Canvas));

	// 게이지의 ±200px 마진을 복제하면 화면 가장자리 밖에서 유령 팝업이 떠오른다
	TestFalse(TEXT("좌측 1px 밖은 제외"), IsIncomePopupSourceVisible(FVector2D(-1.0, 540.0), Canvas));
	TestFalse(TEXT("우측 1px 밖은 제외"), IsIncomePopupSourceVisible(FVector2D(1921.0, 540.0), Canvas));
	TestFalse(TEXT("상단 밖은 제외"), IsIncomePopupSourceVisible(FVector2D(960.0, -1.0), Canvas));
	TestFalse(TEXT("하단 밖은 제외"), IsIncomePopupSourceVisible(FVector2D(960.0, 1081.0), Canvas));

	// NaN 은 범위 비교가 전부 false 라 통과시켜 버린다 — 이후 Sort 의 비교 전이성까지 깨진다
	const double NaNValue = std::numeric_limits<double>::quiet_NaN();
	TestFalse(TEXT("X NaN 은 제외"), IsIncomePopupSourceVisible(FVector2D(NaNValue, 540.0), Canvas));
	TestFalse(TEXT("Y NaN 은 제외"), IsIncomePopupSourceVisible(FVector2D(960.0, NaNValue), Canvas));
	TestFalse(TEXT("무한대는 제외"),
		IsIncomePopupSourceVisible(FVector2D(std::numeric_limits<double>::infinity(), 540.0), Canvas));

	// 페인트 전 캔버스 = 크기 0. 통과시키면 전 건물이 (0,0) 에서 금액을 띄운다
	TestFalse(TEXT("캔버스 크기 0 은 전부 제외"),
		IsIncomePopupSourceVisible(FVector2D(0.0, 0.0), FVector2D::ZeroVector));

	return true;
}

#endif
