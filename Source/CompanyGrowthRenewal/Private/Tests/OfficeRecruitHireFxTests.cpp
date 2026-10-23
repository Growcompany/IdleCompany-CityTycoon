#include "Misc/AutomationTest.h"
#include "Utils/OfficeRecruitHireFxMath.h"
#include "Office/RecruitEntranceHold.h"

#if WITH_DEV_AUTOMATION_TESTS

// 유니티 빌드에서 using 지시문이 이웃 파일로 새지 않도록 테스트를 수학 네임스페이스 안에 둔다.
namespace OfficeRecruitHireFx
{

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROfficeRecruitStaleRequestTest,
	"CGR.OfficeRecruitment.StaleRequestRelease",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGROfficeRecruitStaleRequestTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("연출도 흐름도 끝났는데 대기 → 푼다"), ShouldReleaseStaleRequest(true, true, false));
	TestFalse(TEXT("연출 중이면 그대로"), ShouldReleaseStaleRequest(true, false, false));
	TestFalse(TEXT("흐름이 아직 바쁘면 그대로"), ShouldReleaseStaleRequest(true, true, true));
	TestFalse(TEXT("대기 중이 아니면 할 일 없음"), ShouldReleaseStaleRequest(false, true, false));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROfficeRecruitHireBlockTest,
	"CGR.OfficeRecruitment.HireBlockReason",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGROfficeRecruitHireBlockTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("채용권이 있으면 막힘 없음"), SingleHireBlock(3, 2, false, 0, 50) == EHireBlock::None);
	TestTrue(TEXT("빈 좌석 0 은 버튼을 열어 두고 토스트로 안내 → 문구 없음"), SingleHireBlock(0, 0, true, 0, 50) == EHireBlock::None);
	TestTrue(TEXT("일반 채용권 0장 → 채용권 부족"), SingleHireBlock(0, 3, false, 999, 50) == EHireBlock::NoTickets);
	TestTrue(TEXT("고급 0장 + 다이아 부족 → 다이아 부족"), SingleHireBlock(0, 3, true, 10, 50) == EHireBlock::NoDiamonds);
	TestTrue(TEXT("고급 0장 + 다이아 충분 → 다이아 단건 가능"), SingleHireBlock(0, 3, true, 50, 50) == EHireBlock::None);
	TestTrue(TEXT("일괄은 채용권 2장부터"), BatchNeedsTickets(1) && !BatchNeedsTickets(2));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROfficeRecruitSeatCardPoseTest,
	"CGR.OfficeRecruitment.HireFx.SeatCardPose",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGROfficeRecruitSeatCardPoseTest::RunTest(const FString& Parameters)
{
	for (const FVector2D Viewport : { FVector2D(2304.f, 1440.f), FVector2D(3120.f, 1440.f), FVector2D(1280.f, 720.f) })
	{
		for (const FVector2D CanvasScale : { FVector2D(1.f, 1.f), FVector2D(0.5f, 0.5f), FVector2D(2.f, 2.f), FVector2D(0.75f, 1.f) })
		{
			const float Margin = FMath::Max(12.f, Viewport.Y * 0.02f);
			const FVector2D DeskA(Viewport.X * 0.40f, Viewport.Y * 0.80f);
			const FVector2D DeskB(Viewport.X * 0.60f, Viewport.Y * 0.80f);
			const FCardPose A = SeatCardPose(Viewport, DeskA, true, CanvasScale);
			const FCardPose B = SeatCardPose(Viewport, DeskB, true, CanvasScale);
			const FVector2D Size(CardW * A.Scale / CanvasScale.X, CardH * A.Scale / CanvasScale.Y);
			TestTrue(TEXT("원본 사원증 배율은 양의 유한값"), FMath::IsFinite(A.Scale) && A.Scale > 0.f);
			TestTrue(TEXT("카드 높이는 화면 28% 이하"), Size.Y <= Viewport.Y * 0.28f + 0.01f);
			TestTrue(TEXT("카드 폭은 화면 20% 이하"), Size.X <= Viewport.X * 0.20f + 0.01f);
			TestTrue(TEXT("좌석을 바꾸면 카드 X도 동일하게 이동"), FMath::IsNearlyEqual(B.Center.X - A.Center.X, DeskB.X - DeskA.X, 0.01f));
			TestTrue(TEXT("책상 위 8px에 카드 아랫변"), FMath::IsNearlyEqual(A.Center.Y + Size.Y * 0.5f, DeskA.Y - 8.f, 0.01f));
			for (const FVector2D Seat : { FVector2D(-100.f, -100.f), FVector2D(Viewport.X + 100.f, Viewport.Y + 100.f),
				FVector2D(0.f, Viewport.Y), FVector2D(Viewport.X, 0.f) })
			{
				const FCardPose Edge = SeatCardPose(Viewport, Seat, true, CanvasScale);
				TestTrue(TEXT("좌석이 화면 밖이어도 카드 왼쪽/오른쪽 안"), Edge.Center.X - Size.X * 0.5f >= Margin - 0.01f
					&& Edge.Center.X + Size.X * 0.5f <= Viewport.X - Margin + 0.01f);
				TestTrue(TEXT("좌석이 화면 밖이어도 카드 위/아래 안"), Edge.Center.Y - Size.Y * 0.5f >= Margin - 0.01f
					&& Edge.Center.Y + Size.Y * 0.5f <= Viewport.Y - Margin + 0.01f);
			}
			const FCardPose Fallback = SeatCardPose(Viewport, FVector2D(-99999.f, 99999.f), false, CanvasScale);
			TestTrue(TEXT("투영 실패는 화면 중앙"), Fallback.Center.Equals(Viewport * 0.5f, 0.01f));
			TestTrue(TEXT("투영 실패도 같은 카드 크기"), FMath::IsNearlyEqual(Fallback.Scale, A.Scale, 0.001f));
		}
	}
	const FCardPose Tiny = SeatCardPose(FVector2D::ZeroVector, FVector2D::ZeroVector, false);
	TestTrue(TEXT("퇴화한 viewport도 유한 포즈"), FMath::IsFinite(Tiny.Center.X) && FMath::IsFinite(Tiny.Center.Y)
		&& FMath::IsFinite(Tiny.Scale) && Tiny.Scale >= 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROfficeRecruitHireFxCurvesTest,
	"CGR.OfficeRecruitment.HireFx.Curves",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGROfficeRecruitHireFxCurvesTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("누르기 전 배율 1"), PressScaleAt(0.f), 1.f);
	TestTrue(TEXT("0.06초 최저 0.94"), FMath::IsNearlyEqual(PressScaleAt(0.06f), 0.94f, 0.001f));
	TestEqual(TEXT("0.18초 복귀"), PressScaleAt(0.18f), 1.f);
	TestEqual(TEXT("흔들림 시작 0"), ShakeOffsetAt(0.f), 0.f);
	TestEqual(TEXT("흔들림 끝 0"), ShakeOffsetAt(ShakeDuration), 0.f);
	TestTrue(TEXT("흔들림 첫 꼭짓점 -12"), FMath::IsNearlyEqual(ShakeOffsetAt(ShakeDuration / 6.f), -12.f, 0.01f));
	TestTrue(TEXT("딤 0.2초에 목표"), FMath::IsNearlyEqual(DimOpacityAt(0.2f), DimTarget, 0.001f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROfficeRecruitEntranceHoldTest,
	"CGR.OfficeRecruitment.EntranceHold.Rules",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGROfficeRecruitEntranceHoldTest::RunTest(const FString& Parameters)
{
	{
		FRecruitEntranceHold Hold;
		TestTrue(TEXT("보류 요청 없음 → 커밋 즉시 입장"), Hold.OnCommitted());
		TestFalse(TEXT("보류 아님"), Hold.IsHeld());
		TestFalse(TEXT("보류 아닌 Tick 은 시작 안 함"), Hold.Tick(1.f));
	}
	{
		FRecruitEntranceHold Hold;
		Hold.Request();
		TestFalse(TEXT("보류 → 커밋 때 시작 안 함"), Hold.OnCommitted());
		TestTrue(TEXT("보류 중"), Hold.IsHeld());
		TestFalse(TEXT("허락 전 Tick"), Hold.Tick(0.1f));
		Hold.Release();
		TestTrue(TEXT("허락 다음 Tick 에 시작"), Hold.Tick(0.016f));
		TestFalse(TEXT("시한 아님"), Hold.DidTimeOut());
		TestFalse(TEXT("한 번만 시작"), Hold.Tick(0.016f));
	}
	{
		FRecruitEntranceHold Hold;
		Hold.Request();
		Hold.OnCommitted();
		TestFalse(TEXT("1.49초까지 대기"), Hold.Tick(1.49f));
		TestTrue(TEXT("1.5초 시한에 시작"), Hold.Tick(0.02f));
		TestTrue(TEXT("시한 발동 기록"), Hold.DidTimeOut());
	}
	{
		FRecruitEntranceHold Hold;
		Hold.Request();
		Hold.Release();
		TestTrue(TEXT("커밋 전에 허락(모션 줄이기) → 즉시 시작"), Hold.OnCommitted());
	}
	{
		FRecruitEntranceHold Hold;
		Hold.Request();
		Hold.OnCommitted();
		Hold.Reset();
		TestFalse(TEXT("Reset 뒤 보류 아님"), Hold.IsHeld());
		TestTrue(TEXT("Reset 뒤 재사용은 요청 없음 상태"), Hold.OnCommitted());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROfficeRecruitSeatBlockTest,
	"CGR.OfficeRecruitment.SeatBlock",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGROfficeRecruitSeatBlockTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("책상·수용 모두 여유 → 막힘 없음"), SeatBlock(3, 2) == ESeatBlock::None);
	TestTrue(TEXT("빈 책상은 있는데 수용 인원이 가득 → 수용 인원 안내"), SeatBlock(4, 0) == ESeatBlock::Capacity);
	TestTrue(TEXT("수용 여유인데 빈 책상 없음 → 책상 안내"), SeatBlock(0, 3) == ESeatBlock::Desks);
	TestTrue(TEXT("둘 다 0 이면 더 근본인 수용 인원부터"), SeatBlock(0, 0) == ESeatBlock::Capacity);
	// 빈 자리 0 안내는 쉬는 창에서만 — 채용이 막 성공하면 자리가 0 이 되어 연출 도중 안내가 튀어나왔다(2026-10-13 캡처).
	TestTrue(TEXT("쉬는 창 + 빈 자리 0 → 안내"), ShouldShowSeatGuide(ESeatBlock::Desks, false, true));
	TestFalse(TEXT("자리 있음 → 안내 없음"), ShouldShowSeatGuide(ESeatBlock::None, false, true));
	TestFalse(TEXT("요청 중이면 안내 없음"), ShouldShowSeatGuide(ESeatBlock::Desks, true, true));
	TestFalse(TEXT("연출 중이면 안내 없음"), ShouldShowSeatGuide(ESeatBlock::Capacity, false, false));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROfficeRecruitSeatRevealTest,
	"CGR.OfficeRecruitment.HireFx.SeatRevealTiming",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGROfficeRecruitSeatRevealTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("첫 카드 요청 시 즉시 공개"), CanRevealCard(true, true, -1.f));
	TestFalse(TEXT("요청 전 공개 불가"), CanRevealCard(false, true, -1.f));
	TestFalse(TEXT("앞 카드 공개 전 대기"), CanRevealCard(true, false, -1.f));
	TestFalse(TEXT("앞 카드와 0.30초 간격"), CanRevealCard(true, false, 0.29f));
	TestTrue(TEXT("0.30초가 되면 다음 카드 공개"), CanRevealCard(true, false, 0.30f));
	TestEqual(TEXT("공개 순간 얼굴은 불투명"), SeatCardOpacity(0.f), 1.f);
	TestEqual(TEXT("2초가 지나도 불투명"), SeatCardOpacity(2.5f), 1.f);
	TestEqual(TEXT("3초까지 읽을 수 있음"), SeatCardOpacity(3.f), 1.f);
	TestTrue(TEXT("읽기 뒤 페이드 진행"), SeatCardOpacity(3.15f) > 0.f && SeatCardOpacity(3.15f) < 1.f);
	TestEqual(TEXT("3.3초에 사라짐"), SeatCardOpacity(3.3f), 0.f);
	TestEqual(TEXT("더 지난 뒤에도 사라짐"), SeatCardOpacity(4.f), 0.f);
	TestTrue(TEXT("공개 배율은 원 크기에 단조 접근"), RevealScale(0.f) < RevealScale(0.5f)
		&& RevealScale(0.5f) < RevealScale(1.f) && RevealScale(1.f) == 1.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROfficeRecruitSeatRockTest,
	"CGR.OfficeRecruitment.HireFx.SeatRock",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGROfficeRecruitSeatRockTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("좌우 흔들림은 양 방향"), SeatCardRockAt(0.6f, false).Shear.X > 0.f
		&& SeatCardRockAt(1.8f, false).Shear.X < 0.f);
	TestTrue(TEXT("정면 텍스트는 뒤집히지 않음"), SeatCardRockAt(0.6f, false).Scale.X > 0.85f
		&& SeatCardRockAt(1.8f, false).Scale.X > 0.85f);
	TestTrue(TEXT("종료 시 변환은 정면"), SeatCardRockAt(SeatCardReadDuration + SeatCardFadeDuration, false).IsIdentity());
	for (int32 Sample = 0; Sample <= 66; ++Sample)
	{
		const float Age = Sample * 0.05f;
		const FWidgetTransform Rock = SeatCardRockAt(Age, false);
		TestTrue(TEXT("흔들림은 카드 중심에서만 발생"), Rock.Translation.IsNearlyZero() && Rock.Angle == 0.f);
		TestTrue(TEXT("모션 줄이기는 흔들림만 끔"), SeatCardRockAt(Age, true).IsIdentity());
		for (const FVector2D CanvasScale : { FVector2D(1.f, 1.f), FVector2D(0.75f, 1.f), FVector2D(2.f, 0.5f) })
		{
			const FVector2D Viewport(1280.f, 720.f);
			for (const FVector2D Seat : { FVector2D(-100.f, -100.f), FVector2D(1380.f, 820.f) })
			{
				const FCardPose Pose = SeatCardPose(Viewport, Seat, true, CanvasScale);
				FWidgetTransform Transform = Rock;
				Transform.Scale *= Pose.Scale;
				const FSlateRenderTransform SlateTransform = Transform.ToSlateRenderTransform();
				for (const FVector2D Corner : { FVector2D(-CardW * 0.5f, -CardH * 0.5f),
					FVector2D(-CardW * 0.5f, CardH * 0.5f), FVector2D(CardW * 0.5f, -CardH * 0.5f),
					FVector2D(CardW * 0.5f, CardH * 0.5f) })
				{
					const FVector2D CornerCanvas = SlateTransform.TransformPoint(Corner);
					const FVector2D CornerViewport = Pose.Center + FVector2D(CornerCanvas.X / CanvasScale.X,
						CornerCanvas.Y / CanvasScale.Y);
					TestTrue(TEXT("변환된 네 모서리가 비균등 캔버스에서도 화면 안"),
						CornerViewport.X >= 12.f - 0.01f && CornerViewport.X <= Viewport.X - 12.f + 0.01f
						&& CornerViewport.Y >= 12.f - 0.01f && CornerViewport.Y <= Viewport.Y - 12.f + 0.01f);
				}
			}
		}
	}
	return true;
}

} // namespace OfficeRecruitHireFx

#endif
