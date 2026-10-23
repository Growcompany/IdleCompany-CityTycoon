#include "Misc/AutomationTest.h"
#include "Player/PlayerCamera.h"
#include "UI/HUD/GuideTooltipPlacement.h"
#include "UI/HUD/MissionGuideOverlayWidget.h"
#include "UI/HUD/PanelIntroOverlayWidget.h"
#include "UI/Element/Common/GuideTooltipWidget.h"
#include "UI/HUD/GuideHoleTransition.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGuideHoleTransitionTest,
	"CGR.GuideTooltip.HoleTransition",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGuideHoleTransitionTest::RunTest(const FString& Parameters)
{
	FGuideHoleTransition Motion;
	FVector2D Center(100.f, 200.f);
	FVector2D Half(40.f, 60.f);
	Motion.Advance(Center, Half, 0.f);
	TestTrue(TEXT("first target appears without flying from origin"), Motion.IsSettled());
	Motion.Retarget();
	Center = FVector2D(900.f, 600.f);
	Half = FVector2D(160.f, 100.f);
	Motion.Advance(Center, Half, 0.f);
	TestEqual(TEXT("target change starts at visible center"), Center, FVector2D(100.f, 200.f));
	TestEqual(TEXT("target change starts at visible size"), Half, FVector2D(40.f, 60.f));
	TestFalse(TEXT("moving target is not yet actionable"), Motion.IsSettled());
	Center = FVector2D(900.f, 600.f);
	Half = FVector2D(160.f, 100.f);
	Motion.Advance(Center, Half, 0.12f);
	TestTrue(TEXT("center travels between targets"), Center.X > 100.f && Center.X < 900.f);
	TestTrue(TEXT("size morphs between targets"), Half.X > 40.f && Half.X < 160.f);
	const FVector2D VisibleCenter = Center;
	const FVector2D VisibleHalf = Half;
	Motion.Retarget();
	Center = FVector2D(300.f, 100.f);
	Half = FVector2D(70.f, 30.f);
	Motion.Advance(Center, Half, 0.f);
	TestEqual(TEXT("interrupted move continues from visible center"), Center, VisibleCenter);
	TestEqual(TEXT("interrupted move continues from visible size"), Half, VisibleHalf);
	Center = FVector2D(320.f, 120.f);
	Half = FVector2D(72.f, 32.f);
	Motion.Advance(Center, Half, 0.3f);
	TestTrue(TEXT("moving destination cannot prolong fixed transition"), Motion.IsSettled());
	TestEqual(TEXT("arrives at current destination"), Center, FVector2D(320.f, 120.f));
	TestEqual(TEXT("arrives at current size"), Half, FVector2D(72.f, 32.f));
	Motion.Reset();
	Center = FVector2D(1500.f, 800.f);
	Half = FVector2D(100.f, 80.f);
	Motion.Advance(Center, Half, 0.f);
	TestEqual(TEXT("reopening never flies from a hidden old target"), Center, FVector2D(1500.f, 800.f));
	TestTrue(TEXT("reopening is settled"), Motion.IsSettled());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGuideTooltipRevealTest,
	"CGR.GuideTooltip.RevealLifecycle",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGuideTooltipRevealTest::RunTest(const FString& Parameters)
{
	UGuideTooltipWidget* Tip = NewObject<UGuideTooltipWidget>();
	const FText Line = FText::FromString(TEXT("first hint"));
	Tip->SetContent(FText::GetEmpty(), Line);
	Tip->AdvanceReveal(0.21f);
	const float MidOpacity = Tip->GetRenderOpacity();
	TestTrue(TEXT("hint fades in"), MidOpacity > 0.f && MidOpacity < 1.f);
	TestFalse(TEXT("polling same content preserves reveal"), Tip->SetContent(FText::GetEmpty(), Line));
	Tip->AdvanceReveal(0.26f);
	TestEqual(TEXT("repeated polling can finish reveal"), Tip->GetRenderOpacity(), 1.f);
	TestEqual(TEXT("settled hint stops moving"), Tip->GetRenderTransform().Translation, FVector2D::ZeroVector);
	Tip->HideGuide();
	TestEqual(TEXT("unresolved target hides hint"), Tip->GetVisibility(), ESlateVisibility::Collapsed);
	Tip->AdvanceReveal(0.f);
	TestEqual(TEXT("reappearing hint starts transparent"), Tip->GetRenderOpacity(), 0.f);
	Tip->AdvanceReveal(0.5f);
	Tip->SetContent(FText::GetEmpty(), FText::FromString(TEXT("next hint")));
	Tip->AdvanceReveal(0.f);
	TestEqual(TEXT("new content gets its own reveal"), Tip->GetRenderOpacity(), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGuideTooltipPositionTest,
	"CGR.GuideTooltip.Position",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGuideTooltipPositionTest::RunTest(const FString& Parameters)
{
	const FVector2D Screen(2304.0f, 1440.0f);
	const FVector2D Tip(600.0f, 200.0f);
	const FVector2D AnchorSize(268.0f, 74.0f);

	{
		// 화면 중앙 대상 + 꼬리 위 = 툴팁이 대상 아래, 가로 중앙정렬
		const FVector2D P = ComputeGuideTooltipPosition(
			FVector2D(1152.0f, 400.0f), AnchorSize, Tip,
			EGuideTooltipDir::Up, Screen, 72.0f, 30.0f);
		TestEqual(TEXT("중앙정렬 X"), P.X, 852.0);            // 1152 - 300
		TestEqual(TEXT("대상 아래 Y"), P.Y, 467.0);           // 400 + 37 + 30
	}
	{
		// 좌단 대상 — 툴팁 좌변이 안전 여백으로 밀린다
		const FVector2D P = ComputeGuideTooltipPosition(
			FVector2D(120.0f, 400.0f), AnchorSize, Tip,
			EGuideTooltipDir::Up, Screen, 72.0f, 30.0f);
		TestEqual(TEXT("좌측 클램프"), P.X, 72.0);
	}
	{
		// 우단 대상 — 툴팁 우변이 안전 여백으로 밀린다
		const FVector2D P = ComputeGuideTooltipPosition(
			FVector2D(2200.0f, 400.0f), AnchorSize, Tip,
			EGuideTooltipDir::Up, Screen, 72.0f, 30.0f);
		TestEqual(TEXT("우측 클램프"), P.X, 1632.0);          // 2304 - 72 - 600
	}
	{
		// 아래꼬리 = 툴팁이 대상 위. 높이만큼 빼야 대상에 붙는다(목업에서 실제로 틀렸던 지점)
		const FVector2D P = ComputeGuideTooltipPosition(
			FVector2D(1152.0f, 1200.0f), FVector2D(150.0f, 144.0f), Tip,
			EGuideTooltipDir::Down, Screen, 72.0f, 30.0f);
		TestEqual(TEXT("대상 위에 바닥이 붙는다"), P.Y, 898.0);  // 1200 - 72 - 30 - 200
	}
	{
		// 툴팁이 화면보다 크면 좌상단 안전 여백 고정 (음수 좌표 금지)
		const FVector2D P = ComputeGuideTooltipPosition(
			FVector2D(1152.0f, 720.0f), AnchorSize, FVector2D(4000.0f, 3000.0f),
			EGuideTooltipDir::Up, Screen, 72.0f, 30.0f);
		TestEqual(TEXT("과대 툴팁 X"), P.X, 72.0);
		TestEqual(TEXT("과대 툴팁 Y"), P.Y, 72.0);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGRGuideTooltipAvoidanceTest,
	"CGR.GuideTooltip.ContentAvoidance",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGuideTooltipAvoidanceTest::RunTest(const FString& Parameters)
{
	const FVector2D Tip(580., 170.);
	const FVector2D Button(320., 100.);
	for (const double Width : {2304., 2880., 3120.})
	{
		const FVector2D Screen(Width, 1440.);
		const FVector2D Anchor(Width * 0.5, 1240.);
		const FSlateRect Rewards(Width * 0.5 - 720., 850., Width * 0.5 + 720., 1170.);
		const TArray<FSlateRect> Obstacles{Rewards};
		const FGuideTooltipLayout Result = ResolveGuideTooltipLayout(
			EGuideTooltipDir::Down, Anchor, Button, Tip, Screen, 72.f, 30.f, Obstacles);
		TestTrue(TEXT("launch reward row remains readable at each supported width"), Result.bAvoidsContent);
		TestTrue(TEXT("tooltip clears reward bottom or side"),
			Result.Position.Y >= Rewards.Bottom || Result.Position.X + Tip.X <= Rewards.Left || Result.Position.X >= Rewards.Right);
		TestTrue(TEXT("launch tooltip stays inside safe screen"), Result.Position.X >= 72. && Result.Position.Y >= 72.
			&& Result.Position.X + Tip.X <= Width - 72. && Result.Position.Y + Tip.Y <= 1368.);
	}
	{
		const FVector2D Anchor(1152., 800.);
		const FVector2D Screen(2304., 1440.);
		const TArray<FSlateRect> Obstacles{FSlateRect(900., 450., 1400., 740.)};
		const FGuideTooltipLayout Result = ResolveGuideTooltipLayout(
			EGuideTooltipDir::Down, Anchor, Button, Tip, Screen, 72.f, 30.f, Obstacles);
		TestTrue(TEXT("chooses unoccupied opposite side instead of covering price text"), Result.bAvoidsContent);
		TestEqual(TEXT("tail follows resolved side"), Result.Direction, EGuideTooltipDir::Up);
	}
	{
		const FGuideTooltipLayout Result = ResolveGuideTooltipLayout(EGuideTooltipDir::Down,
			FVector2D(200., 120.), Button, Tip, FVector2D(2304., 1440.), 72.f, 30.f, {});
		TestTrue(TEXT("screen-edge panel intro can flip without hiding its anchor"), Result.bAvoidsContent);
		TestEqual(TEXT("top-edge tail points up"), Result.Direction, EGuideTooltipDir::Up);
	}
	{
		// 우측 공장 패널 ― 인부 버튼이 든 자기 행과 위아래 다른 강화 행. 패널 밖 멀리 나가지 않고 자기 행 위에 놓인다
		const FVector2D FactoryButton(2110., 720.);
		const TArray<FSlateRect> FactoryRows{FSlateRect(1450., 170., 2260., 420.), FSlateRect(1450., 600., 2260., 840.),
			FSlateRect(1450., 1020., 2260., 1270.)};
		const FGuideTooltipLayout Result = ResolveGuideTooltipLayout(EGuideTooltipDir::Right,
			FactoryButton, Button, Tip, FVector2D(2304., 1440.), 72.f, 30.f, FactoryRows);
		const FSlateRect TipRect(Result.Position.X, Result.Position.Y, Result.Position.X + Tip.X, Result.Position.Y + Tip.Y);
		const FSlateRect ButtonRect(FactoryButton.X - Button.X * 0.5, FactoryButton.Y - Button.Y * 0.5,
			FactoryButton.X + Button.X * 0.5, FactoryButton.Y + Button.Y * 0.5);
		const double GapX = FMath::Max3(0., double(ButtonRect.Left - TipRect.Right), double(TipRect.Left - ButtonRect.Right));
		const double GapY = FMath::Max3(0., double(ButtonRect.Top - TipRect.Bottom), double(TipRect.Top - ButtonRect.Bottom));
		TestTrue(TEXT("dense factory content keeps the tooltip next to its button"), FMath::Max(GapX, GapY) <= 30. + 48. + 1.);
		TestTrue(TEXT("the button's own row is not content to avoid, other rows stay clear"), Result.bAvoidsContent);
		TestEqual(TEXT("factory tooltip sits beside its button"), Result.Direction, EGuideTooltipDir::Right);
	}
	{
		// 오른쪽 위 닫기(X)를 짚는데 아래가 행으로 꽉 차 있다 ― 마지막 행 밑까지 내려가지 않는다
		const FVector2D CloseCenter(2150., 150.);
		const FVector2D CloseSize(104., 104.);
		const FSlateRect Rows(1300., 230., 2260., 1300.);
		const FGuideTooltipLayout Result = ResolveGuideTooltipLayout(EGuideTooltipDir::Down,
			CloseCenter, CloseSize, Tip, FVector2D(2304., 1440.), 72.f, 30.f, {Rows});
		const double DistanceBelow = Result.Position.Y - (CloseCenter.Y + CloseSize.Y * 0.5);
		const double DistanceLeft = (CloseCenter.X - CloseSize.X * 0.5) - (Result.Position.X + Tip.X);
		TestTrue(TEXT("close-button tooltip stays beside the button"),
			FMath::Max(DistanceBelow, DistanceLeft) <= 30. + 48. + 1.);
	}
	{
		// 덮으면 안 되는 것(다른 창의 닫기 X·손 그림)은 읽는 내용보다 먼저 피한다
		const FVector2D Anchor(1152., 800.);
		const FVector2D Screen(2304., 1440.);
		FGuideTooltipAvoid Avoid;
		Avoid.Hard.Add(FSlateRect(1100., 520., 1204., 624.));   // 기본 자리(위쪽)를 지나는 닫기 X
		Avoid.Hard.Add(FSlateRect(1000., 880., 1300., 1100.));  // 아래쪽을 덮는 손 그림
		const FGuideTooltipLayout Result = ResolveGuideTooltipLayoutAvoiding(
			EGuideTooltipDir::Down, Anchor, Button, Tip, Screen, 72.f, 30.f, Avoid);
		TestEqual(TEXT("hard rects are never covered when a side is free"),
			SumGuideTooltipOverlap(Result.Position, Tip, Avoid.Hard, 0.), 0.);
		TestTrue(TEXT("free side is the left or right of the button"),
			Result.Direction == EGuideTooltipDir::Left || Result.Direction == EGuideTooltipDir::Right);
	}
	{
		// 같은 거리 후보 중에서는 다른 버튼보다 글자를 덮는 편을 고른다
		const FVector2D Anchor(1152., 800.);
		FGuideTooltipAvoid Avoid;
		Avoid.Controls.Add(FSlateRect(992., 890., 1312., 990.));  // 바로 아래 다른 버튼
		Avoid.Soft.Add(FSlateRect(700., 540., 1600., 735.));      // 위쪽 설명 글자
		const FGuideTooltipLayout Result = ResolveGuideTooltipLayoutAvoiding(
			EGuideTooltipDir::Up, Anchor, Button, Tip, FVector2D(2304., 1440.), 72.f, 30.f, Avoid);
		TestEqual(TEXT("neighbor button stays uncovered"), SumGuideTooltipOverlap(Result.Position, Tip, Avoid.Controls, 0.), 0.);
	}
	{
		// 버튼만 비워 두고 사방이 내용으로 찬 화면(버튼을 감싼 하나짜리 묶음은 대상 자신이라 피할 내용이 아니다)
		const TArray<FSlateRect> Full{FSlateRect(0., 0., 2304., 700.), FSlateRect(0., 900., 2304., 1440.),
			FSlateRect(0., 700., 900., 900.), FSlateRect(1404., 700., 2304., 900.)};
		const FGuideTooltipLayout Result = ResolveGuideTooltipLayout(EGuideTooltipDir::Down,
			FVector2D(1152., 800.), Button, Tip, FVector2D(2304., 1440.), 72.f, 30.f, Full);
		TestFalse(TEXT("crowded fallback is not reported as unobstructed"), Result.bAvoidsContent);
		TestTrue(TEXT("crowded fallback still keeps actionable button uncovered"),
			Result.Position.Y + Tip.Y <= 750. || Result.Position.Y >= 850.
			|| Result.Position.X + Tip.X <= 992. || Result.Position.X >= 1312.);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGuideTailOffsetTest,
	"CGR.GuideTooltip.TailOffset",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGuideTailOffsetTest::RunTest(const FString& Parameters)
{
	// 툴팁 폭 600, 시작 852, 대상 중심 1152 → 꼬리는 정중앙(300 - 15 = 285)
	TestEqual(TEXT("클램프 없으면 대상 정렬"),
		ComputeGuideTailOffset(1152.0f, 852.0f, 600.0f, 30.0f, 24.0f), 285.0f);

	// 툴팁이 오른쪽으로 밀린 경우 — 꼬리는 왼쪽 코너 인셋까지만
	TestEqual(TEXT("좌측 코너 인셋 클램프"),
		ComputeGuideTailOffset(100.0f, 72.0f, 600.0f, 30.0f, 24.0f), 24.0f);

	// 툴팁이 왼쪽으로 밀린 경우 — 꼬리는 오른쪽 코너 인셋까지만
	TestEqual(TEXT("우측 코너 인셋 클램프"),
		ComputeGuideTailOffset(2280.0f, 1632.0f, 600.0f, 30.0f, 24.0f), 546.0f);  // 600 - 24 - 30

	// 툴팁이 꼬리보다 좁은 병리 케이스에서도 인셋 아래로 안 내려간다
	TestEqual(TEXT("과소 툴팁 안전"),
		ComputeGuideTailOffset(100.0f, 90.0f, 40.0f, 30.0f, 24.0f), 24.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGuideTailPlacementTest,
	"CGR.GuideTooltip.TailPlacement",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGuideTailPlacementTest::RunTest(const FString& Parameters)
{
	// 각 방향의 기대 모서리는 ComputeGuideTooltipPosition 이 툴팁을 어디에 놓는지에서 나온다.
	// 툴팁이 대상 반대편에 놓이므로 꼬리는 대상 쪽 모서리에 붙어야 한다.
	{
		// Up = 대상이 위 → 툴팁은 아래 → 꼬리는 툴팁 위쪽 모서리, 가로로 미끄러짐
		const FGuideTailPlacement P = ResolveGuideTailPlacement(EGuideTooltipDir::Up);
		TestEqual(TEXT("Up 각도"), P.Angle, 0.0f);
		TestEqual(TEXT("Up 앵커 X"), P.AnchorPoint.X, 0.0);
		TestEqual(TEXT("Up 앵커 Y"), P.AnchorPoint.Y, 0.0);
		TestEqual(TEXT("Up 얼라인 X"), P.Alignment.X, 0.0);
		TestEqual(TEXT("Up 얼라인 Y"), P.Alignment.Y, 0.0);
		TestTrue(TEXT("Up 은 가로축 미끄러짐"), P.bAlongX);
	}
	{
		// Down = 대상이 아래 → 툴팁은 위 → 꼬리는 툴팁 **아래쪽** 모서리 (교차축 0 고정이 틀렸던 지점)
		const FGuideTailPlacement P = ResolveGuideTailPlacement(EGuideTooltipDir::Down);
		TestEqual(TEXT("Down 각도"), P.Angle, 180.0f);
		TestEqual(TEXT("Down 앵커 X"), P.AnchorPoint.X, 0.0);
		TestEqual(TEXT("Down 앵커 Y"), P.AnchorPoint.Y, 1.0);
		TestEqual(TEXT("Down 얼라인 X"), P.Alignment.X, 0.0);
		TestEqual(TEXT("Down 얼라인 Y"), P.Alignment.Y, 1.0);
		TestTrue(TEXT("Down 은 가로축 미끄러짐"), P.bAlongX);
	}
	{
		// Left = 대상이 왼쪽 → 툴팁은 오른쪽 → 꼬리는 툴팁 왼쪽 모서리, 세로로 미끄러짐
		const FGuideTailPlacement P = ResolveGuideTailPlacement(EGuideTooltipDir::Left);
		TestEqual(TEXT("Left 각도"), P.Angle, 270.0f);
		TestEqual(TEXT("Left 앵커 X"), P.AnchorPoint.X, 0.0);
		TestEqual(TEXT("Left 앵커 Y"), P.AnchorPoint.Y, 0.0);
		TestEqual(TEXT("Left 얼라인 X"), P.Alignment.X, 0.0);
		TestEqual(TEXT("Left 얼라인 Y"), P.Alignment.Y, 0.0);
		TestFalse(TEXT("Left 는 세로축 미끄러짐"), P.bAlongX);
	}
	{
		// Right = 대상이 오른쪽 → 툴팁은 왼쪽 → 꼬리는 툴팁 **오른쪽** 모서리 (교차축 0 고정이 틀렸던 지점)
		const FGuideTailPlacement P = ResolveGuideTailPlacement(EGuideTooltipDir::Right);
		TestEqual(TEXT("Right 각도"), P.Angle, 90.0f);
		TestEqual(TEXT("Right 앵커 X"), P.AnchorPoint.X, 1.0);
		TestEqual(TEXT("Right 앵커 Y"), P.AnchorPoint.Y, 0.0);
		TestEqual(TEXT("Right 얼라인 X"), P.Alignment.X, 1.0);
		TestEqual(TEXT("Right 얼라인 Y"), P.Alignment.Y, 0.0);
		TestFalse(TEXT("Right 는 세로축 미끄러짐"), P.bAlongX);
	}

	// 앵커와 얼라인먼트가 어긋나면 꼬리가 모서리 밖으로 튀어나온다 ― 네 방향 모두 같아야 한다
	const EGuideTooltipDir AllDirs[] = {
		EGuideTooltipDir::Up, EGuideTooltipDir::Down,
		EGuideTooltipDir::Left, EGuideTooltipDir::Right };
	for (const EGuideTooltipDir Dir : AllDirs)
	{
		const FGuideTailPlacement P = ResolveGuideTailPlacement(Dir);
		TestEqual(TEXT("앵커 X == 얼라인 X"), P.Alignment.X, P.AnchorPoint.X);
		TestEqual(TEXT("앵커 Y == 얼라인 Y"), P.Alignment.Y, P.AnchorPoint.Y);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGuideTooltipSideTest,
	"CGR.GuideTooltip.Side",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGuideTooltipSideTest::RunTest(const FString& Parameters)
{
	const FVector2D Screen(2304.0, 1440.0);
	const FVector2D Tip(600.0, 200.0);
	const FVector2D Anchor(206.0, 86.0);   // Bar 150x30 + 스포트라이트 패딩 28 사방 → half (103, 43)
	const float Margin = 72.0f;
	const float Gap = 30.0f;

	auto Side = [&](EGuideTooltipDir Preferred, const FVector2D& Center, const FVector2D& TipSize)
	{
		return static_cast<int32>(
			ResolveGuideTooltipSide(Preferred, Center, Anchor, TipSize, Screen, Margin, Gap));
	};
	const int32 Up = static_cast<int32>(EGuideTooltipDir::Up);
	const int32 Down = static_cast<int32>(EGuideTooltipDir::Down);
	const int32 Left = static_cast<int32>(EGuideTooltipDir::Left);
	const int32 Right = static_cast<int32>(EGuideTooltipDir::Right);

	// ===== 자리가 있으면 선호 유지 =====
	TestEqual(TEXT("중앙 Right 유지"),  Side(EGuideTooltipDir::Right, FVector2D(1152.0, 720.0), Tip), Right);
	TestEqual(TEXT("중앙 Left 유지"),   Side(EGuideTooltipDir::Left,  FVector2D(1152.0, 720.0), Tip), Left);
	TestEqual(TEXT("상단 Up 유지"),     Side(EGuideTooltipDir::Up,    FVector2D(1152.0, 100.0), Tip), Up);
	TestEqual(TEXT("하단 Down 유지"),   Side(EGuideTooltipDir::Down,  FVector2D(1152.0, 1300.0), Tip), Down);

	// ===== 자리가 없으면 반대편으로 =====
	// 좌단 건물 + Right(툴팁 왼쪽) = -583 → 클램프 72 라 Bar 를 덮는다
	TestEqual(TEXT("좌단이면 Right→Left"), Side(EGuideTooltipDir::Right, FVector2D(150.0, 720.0), Tip), Left);
	// 우단 건물 + Left(툴팁 오른쪽) = 2333+600 → 오른쪽 안전선 초과
	TestEqual(TEXT("우단이면 Left→Right"), Side(EGuideTooltipDir::Left,  FVector2D(2200.0, 720.0), Tip), Right);
	TestEqual(TEXT("하단이면 Up→Down"),    Side(EGuideTooltipDir::Up,    FVector2D(1300.0, 1300.0), Tip), Down);
	TestEqual(TEXT("상단이면 Down→Up"),    Side(EGuideTooltipDir::Down,  FVector2D(1152.0, 120.0), Tip), Up);

	// ===== 양쪽 다 없으면 선호 유지(뒤집어도 클램프는 같으므로 방향만 튄다) =====
	TestEqual(TEXT("과대 툴팁은 선호 유지"),
		Side(EGuideTooltipDir::Right, FVector2D(1152.0, 720.0), FVector2D(2400.0, 200.0)), Right);

	// ===== 미끄러짐 축의 클램프는 뒤집을 이유가 아니다 =====
	// 하단 대상 + Left 는 Y 가 클램프되지만 꼬리(Y축)가 보정하므로 방향은 그대로
	TestEqual(TEXT("교차축 클램프는 무시"), Side(EGuideTooltipDir::Left, FVector2D(1152.0, 1400.0), Tip), Left);

	// ===== 회귀 근거 — 뒤집기 전에는 실제로 대상을 덮었다 =====
	{
		const FVector2D Center(150.0, 720.0);
		const double HoleRight = Center.X + Anchor.X * 0.5;   // 253
		const FVector2D Bad = ComputeGuideTooltipPosition(
			Center, Anchor, Tip, EGuideTooltipDir::Right, Screen, Margin, Gap);
		TestEqual(TEXT("뒤집기 전엔 안전선에 박힌다"), Bad.X, 72.0);
		TestTrue(TEXT("그 자리는 구멍을 가로지른다"), Bad.X < HoleRight && (Bad.X + Tip.X) > Center.X);

		const FVector2D Good = ComputeGuideTooltipPosition(
			Center, Anchor, Tip, EGuideTooltipDir::Left, Screen, Margin, Gap);
		TestTrue(TEXT("뒤집으면 구멍 오른쪽 바깥"), Good.X >= HoleRight);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGuideExplainActionTest,
	"CGR.GuideTooltip.ExplainAction",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGuideExplainActionTest::RunTest(const FString& Parameters)
{
	const float T = 5.0f;

	// 페이즈가 아니면 무조건 Idle — 카메라/타겟 상태와 무관
	TestEqual(TEXT("비활성"), ResolveGuideExplainAction(false, true,  true,  0.0f, T), EGuideExplainAction::Idle);
	TestEqual(TEXT("비활성(타임아웃 초과여도)"), ResolveGuideExplainAction(false, false, false, 99.0f, T), EGuideExplainAction::Idle);

	// 타겟이 모여도 카메라가 아직 이동 중이면 기다린다 — 화면 밖 대상에 구멍을 뚫지 않기 위해
	TestEqual(TEXT("카메라 이동 중이면 Wait"), ResolveGuideExplainAction(true, true, false, 0.0f, T), EGuideExplainAction::Wait);

	// 카메라가 도착해도 타겟이 안 모였으면 기다린다
	TestEqual(TEXT("타겟 미해결이면 Wait"), ResolveGuideExplainAction(true, false, true, 0.0f, T), EGuideExplainAction::Wait);

	// 둘 다 만족해야 Show
	TestEqual(TEXT("도착 + 해결 = Show"), ResolveGuideExplainAction(true, true, true, 0.0f, T), EGuideExplainAction::Show);
	TestEqual(TEXT("도착 + 해결이면 경과 무관 Show"), ResolveGuideExplainAction(true, true, true, 99.0f, T), EGuideExplainAction::Show);

	// 타임아웃은 절대값 — 카메라가 5초 넘게 못 오면 진행 방향으로 탈출시킨다(소프트락 가드)
	TestEqual(TEXT("경계 5.0 = Timeout"), ResolveGuideExplainAction(true, false, true, 5.0f, T), EGuideExplainAction::Timeout);
	TestEqual(TEXT("경계 직전은 Wait"), ResolveGuideExplainAction(true, false, true, 4.999f, T), EGuideExplainAction::Wait);
	// 경계 초과도 Timeout — >= 를 == 로 바꾸는 회귀를 잡는다(실제 누적은 정확히 경계값이 되지 않는다)
	TestEqual(TEXT("경계 초과 = Timeout"), ResolveGuideExplainAction(true, false, true, 12.0f, T), EGuideExplainAction::Timeout);
	TestEqual(TEXT("카메라가 안 와도 타임아웃은 걸린다"), ResolveGuideExplainAction(true, true, false, 5.0f, T), EGuideExplainAction::Timeout);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGRGuideExplainStepTest,
	"CGR.GuideTooltip.ExplainStep",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCGRGuideExplainStepTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("0 -> 1"), ResolveNextExplainStep(0, 3), 1);
	TestEqual(TEXT("1 -> 2"), ResolveNextExplainStep(1, 3), 2);
	TestEqual(TEXT("마지막이면 끝"), ResolveNextExplainStep(2, 3), (int32)INDEX_NONE);
	TestEqual(TEXT("스텝 1개면 즉시 끝"), ResolveNextExplainStep(0, 1), (int32)INDEX_NONE);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGRGuideExplainReadGateTest,
	"CGR.GuideTooltip.ExplainReadGate",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGuideExplainReadGateTest::RunTest(const FString& Parameters)
{
	FGuideExplainReadGate Gate;
	constexpr float ReadSeconds = 1.2f;
	Gate.Update(0, true, 0.5f);
	TestFalse(TEXT("first visible frame cannot skip"), Gate.CanAdvance(0, ReadSeconds));
	Gate.Update(0, true, 0.7f);
	TestFalse(TEXT("rapid tap before reading time"), Gate.CanAdvance(0, ReadSeconds));
	Gate.Update(0, true, 0.51f);
	TestTrue(TEXT("visible reading time permits tap"), Gate.CanAdvance(0, ReadSeconds));
	Gate.Reset();
	TestFalse(TEXT("second tap in the same frame cannot advance"), Gate.CanAdvance(0, ReadSeconds));
	Gate.Update(1, true, 0.5f);
	Gate.Update(1, true, 1.21f);
	TestTrue(TEXT("next step earns its own reading time"), Gate.CanAdvance(1, ReadSeconds));
	TestFalse(TEXT("old step cannot use next step time"), Gate.CanAdvance(0, ReadSeconds));
	Gate.Update(2, true, 2.f);
	TestFalse(TEXT("external step change clears old reading time"), Gate.CanAdvance(2, ReadSeconds));
	Gate.Update(2, true, 1.21f);
	Gate.Update(2, false, 2.f);
	TestFalse(TEXT("hidden or moving explanation loses reading time"), Gate.CanAdvance(2, ReadSeconds));
	Gate.Update(2, true, 0.5f);
	TestFalse(TEXT("reappearing explanation starts over"), Gate.CanAdvance(2, ReadSeconds));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGRGuideExplainAtomicAdvanceTest,
	"CGR.GuideTooltip.AtomicAdvance",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGuideExplainAtomicAdvanceTest::RunTest(const FString& Parameters)
{
	// 첫 설명을 탭해도 다음 Bar가 아직 준비되지 않았으면 현재 설명을 유지해야 한다.
	TestEqual(TEXT("다음 타겟 미준비면 현재 설명 유지"),
		ResolveGuideExplainAdvanceAction(0, 2, false),
		EGuideExplainAdvanceAction::WaitForNextTarget);

	// Bar가 준비된 바로 그 프레임에 스텝을 커밋한다.
	TestEqual(TEXT("다음 타겟 준비 시 원자 전환"),
		ResolveGuideExplainAdvanceAction(0, 2, true),
		EGuideExplainAdvanceAction::CommitNextStep);

	// 마지막 설명은 다음 타겟이 없으므로 즉시 설명 페이즈를 끝낸다.
	TestEqual(TEXT("마지막 설명은 종료"),
		ResolveGuideExplainAdvanceAction(1, 2, false),
		EGuideExplainAdvanceAction::FinishExplainPhase);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGRCameraFocusSettleTest,
	"CGR.Camera.FocusSettle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCGRCameraFocusSettleTest::RunTest(const FString& Parameters)
{
	const float Settle = 0.6f;
	const float LocTol = 50.f;
	const float ZoomTol = 0.01f;

	// 이동 중이 아니면 무조건 도착 ― 취소(StopCameraTransition)·미시작 경로가 여기로 빠진다
	TestTrue(TEXT("미이동"), ShouldTreatFocusAsSettled(false, 0.f, Settle, 99999.f, LocTol, 1.f, ZoomTol));

	// 시간이 주 판정 ― 아직 멀어도 0.6초면 통과(83% 도착)
	TestTrue(TEXT("경계 0.6초"), ShouldTreatFocusAsSettled(true, 0.6f, Settle, 5000.f, LocTol, 0.5f, ZoomTol));
	TestTrue(TEXT("0.6초 초과"), ShouldTreatFocusAsSettled(true, 2.0f, Settle, 5000.f, LocTol, 0.5f, ZoomTol));
	TestFalse(TEXT("0.6초 직전 + 멀다"), ShouldTreatFocusAsSettled(true, 0.599f, Settle, 5000.f, LocTol, 0.5f, ZoomTol));

	// 거리 보조 탈출로 ― 짧은 이동은 시간을 안 기다린다
	TestTrue(TEXT("가까우면 즉시"), ShouldTreatFocusAsSettled(true, 0.f, Settle, 10.f, LocTol, 0.001f, ZoomTol));

	// 한 축만 만족하면 보조 탈출로는 안 열린다(둘 다여야 한다)
	TestFalse(TEXT("위치만 가까움"), ShouldTreatFocusAsSettled(true, 0.f, Settle, 10.f, LocTol, 0.5f, ZoomTol));
	TestFalse(TEXT("줌만 가까움"), ShouldTreatFocusAsSettled(true, 0.f, Settle, 5000.f, LocTol, 0.001f, ZoomTol));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRPanelIntroAnchorActionTest,
	"CGR.GuideTooltip.AnchorAction",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRPanelIntroAnchorActionTest::RunTest(const FString& Parameters)
{
	const float Timeout = 0.5f;

	// 해결됐고 사각형도 유효 = 그린다
	TestEqual(TEXT("정상 앵커"),
		ResolvePanelIntroAnchorAction(true, true, 0.0f, Timeout),
		EPanelIntroAnchorAction::Draw);

	// 레이아웃 전 1~2프레임 ― 진짜 앵커도 여기 걸린다. 기다려야 한다
	TestEqual(TEXT("레이아웃 전은 대기"),
		ResolvePanelIntroAnchorAction(true, false, 0.016f, Timeout),
		EPanelIntroAnchorAction::Wait);

	// 상한을 넘도록 못 잡으면 화면에 없는 앵커다 ― 건너뛴다
	TestEqual(TEXT("상한 초과는 스킵"),
		ResolvePanelIntroAnchorAction(true, false, 0.6f, Timeout),
		EPanelIntroAnchorAction::Skip);

	// 앵커 자체를 못 찾은 경우도 같은 상한을 탄다
	TestEqual(TEXT("미해결 앵커도 상한 적용"),
		ResolvePanelIntroAnchorAction(false, false, 0.6f, Timeout),
		EPanelIntroAnchorAction::Skip);

	// 경계값 ― 같으면 아직 대기(초과해야 스킵)
	TestEqual(TEXT("경계는 대기"),
		ResolvePanelIntroAnchorAction(true, false, Timeout, Timeout),
		EPanelIntroAnchorAction::Wait);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
