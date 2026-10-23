#include "Misc/AutomationTest.h"
#include "Layout/Geometry.h"
#include "UI/Panel/BuildingManageCameraFraming.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRBuildingManageCameraFramingTest,
	"CGR.Camera.BuildingManage.VisibleCenter",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRBuildingManageCameraFramingTest::RunTest(const FString& Parameters)
{
	float Ratio = -1.f;
	const FSlateRect WideViewport(FVector2D(0.f, 0.f), FVector2D(2400.f, 1080.f));
	const FSlateRect WideRightUI(FVector2D(1440.f, 0.f), FVector2D(2400.f, 1080.f));
	TestTrue(TEXT("2400px 화면의 왼쪽 1440px는 유효"),
		BuildingManageCameraFraming::TryGetScreenCenterRatioX(WideViewport, WideRightUI, Ratio));
	TestEqual(TEXT("왼쪽 가시 구간 중앙은 화면 폭의 30%"), Ratio, 0.3f, 0.0001f);

	const FSlateRect NarrowViewport(FVector2D(0.f, 0.f), FVector2D(1920.f, 1080.f));
	const FSlateRect NarrowRightUI(FVector2D(960.f, 0.f), FVector2D(1920.f, 1080.f));
	TestTrue(TEXT("1920px 화면의 왼쪽 960px는 유효"),
		BuildingManageCameraFraming::TryGetScreenCenterRatioX(NarrowViewport, NarrowRightUI, Ratio));
	TestEqual(TEXT("좁은 화면에서는 고정 0.3이 아닌 0.25"), Ratio, 0.25f, 0.0001f);

	const FSlateRect OffsetViewport(FVector2D(100.f, 40.f), FVector2D(2020.f, 1120.f));
	const FSlateRect OffsetRightUI(FVector2D(1252.f, 40.f), FVector2D(2020.f, 1120.f));
	TestTrue(TEXT("데스크톱 뷰포트의 절대 원점을 반영"),
		BuildingManageCameraFraming::TryGetScreenCenterRatioX(OffsetViewport, OffsetRightUI, Ratio));
	TestEqual(TEXT("뷰포트 원점을 뺀 가시 중앙"), Ratio, 0.3f, 0.0001f);

	const FSlateRect ScaledViewport(FVector2D(50.f, 20.f), FVector2D(1010.f, 560.f));
	const FSlateRect ScaledRightUI(FVector2D(626.f, 20.f), FVector2D(1010.f, 560.f));
	TestTrue(TEXT("같은 Slate 배율의 UI와 뷰포트"),
		BuildingManageCameraFraming::TryGetScreenCenterRatioX(ScaledViewport, ScaledRightUI, Ratio));
	TestEqual(TEXT("DPI가 달라도 비율은 같음"), Ratio, 0.3f, 0.0001f);

	// 등장 애니메이션은 렌더 위치만 움직인다. 포커스는 레이아웃상 가시 영역에 고정한다.
	const FGeometry ViewportGeometry = FGeometry::MakeRoot(
		FVector2D(1920.f, 1080.f), FSlateLayoutTransform(0.5f, FVector2D(50.f, 20.f)));
	const FGeometry AnimatedRightUIGeometry = ViewportGeometry.MakeChild(
		FVector2D(768.f, 1080.f), FSlateLayoutTransform(FVector2D(1152.f, 0.f)),
		FSlateRenderTransform(FVector2f(400.f, 0.f)), FVector2f::ZeroVector);
	const FSlateRect LayoutRightUI = AnimatedRightUIGeometry.GetLayoutBoundingRect();
	const FSlateRect RenderRightUI = AnimatedRightUIGeometry.GetRenderBoundingRect();
	TestEqual(TEXT("애니메이션 중 오른쪽 UI의 레이아웃 시작점"), LayoutRightUI.Left, 626.f, 0.0001f);
	TestTrue(TEXT("렌더 이동이 레이아웃 경계와 구별됨"), RenderRightUI.Left > LayoutRightUI.Left);
	TestTrue(TEXT("렌더 이동 중에도 레이아웃 가시폭 유효"),
		BuildingManageCameraFraming::TryGetScreenCenterRatioX(
			ViewportGeometry.GetLayoutBoundingRect(), LayoutRightUI, Ratio));
	TestEqual(TEXT("등장 애니메이션 중 목표 중앙은 0.3에 고정"), Ratio, 0.3f, 0.0001f);

	const float PreviousRatio = Ratio;
	const FSlateRect ZeroViewport(FVector2D::ZeroVector, FVector2D::ZeroVector);
	TestFalse(TEXT("레이아웃 미준비: 크기 0"),
		BuildingManageCameraFraming::TryGetScreenCenterRatioX(ZeroViewport, WideRightUI, Ratio));
	TestEqual(TEXT("실패 시 출력 보존"), Ratio, PreviousRatio);

	const FSlateRect CoveringUI(FVector2D(-20.f, 0.f), FVector2D(2400.f, 1080.f));
	TestFalse(TEXT("오른쪽 UI가 전체 화면을 덮으면 가시 구간 없음"),
		BuildingManageCameraFraming::TryGetScreenCenterRatioX(WideViewport, CoveringUI, Ratio));
	const FSlateRect OutsideUI(FVector2D(2400.f, 0.f), FVector2D(2700.f, 1080.f));
	TestFalse(TEXT("뷰포트와 교차하지 않는 UI"),
		BuildingManageCameraFraming::TryGetScreenCenterRatioX(WideViewport, OutsideUI, Ratio));
	const FSlateRect AboveUI(FVector2D(1440.f, -200.f), FVector2D(2400.f, 0.f));
	TestFalse(TEXT("수직으로 교차하지 않는 UI"),
		BuildingManageCameraFraming::TryGetScreenCenterRatioX(WideViewport, AboveUI, Ratio));
	const FSlateRect ZeroUI(FVector2D(1440.f, 0.f), FVector2D(1440.f, 1080.f));
	TestFalse(TEXT("레이아웃 미준비: 오른쪽 UI 폭 0"),
		BuildingManageCameraFraming::TryGetScreenCenterRatioX(WideViewport, ZeroUI, Ratio));
	const FSlateRect NonFiniteUI(FVector2D(std::numeric_limits<float>::quiet_NaN(), 0.f), FVector2D(2400.f, 1080.f));
	TestFalse(TEXT("유한하지 않은 UI 좌표"),
		BuildingManageCameraFraming::TryGetScreenCenterRatioX(WideViewport, NonFiniteUI, Ratio));
	TestEqual(TEXT("모든 실패 후 출력 보존"), Ratio, PreviousRatio);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
