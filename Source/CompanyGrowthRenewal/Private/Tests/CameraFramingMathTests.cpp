#include "Misc/AutomationTest.h"
#include "Player/Components/CameraFramingMath.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	// 기본 리그 = MainMap APlayerCamera 값 (MovementInputHandler.h Zoom Settings)
	FZoomRigParams DefaultRig()
	{
		FZoomRigParams P;
		P.MinArmLength = 1600.f;
		P.MaxArmLength = 320000.f;
		P.PitchInDeg = -40.f;
		P.PitchOutDeg = -55.f;
		P.FOVInDeg = 30.f;
		P.FOVOutDeg = 20.f;
		P.CameraAspectRatio = 1.777778f;
		return P;
	}
	float IdentityCurve(float Zoom) { return Zoom; }

	FZoomRigParams MainMapRig()
	{
		FZoomRigParams P;
		P.MinArmLength = 3000.f;
		P.MaxArmLength = 20000.f;
		P.PitchInDeg = -40.f;
		P.PitchOutDeg = -48.f;
		P.FOVInDeg = 52.f;
		P.FOVOutDeg = 58.f;
		P.CameraAspectRatio = 1.777778f;
		P.CameraPullBackCm = 1200.f;
		return P;
	}

	// 카메라의 forward/up 기저에 월드 수직축의 끝점을 투영한다.
	float ProjectWorldHeight(float EndpointHeight, float PivotHeight, float Distance, float PitchDeg, float HorizontalFOVDeg, float AspectRatio)
	{
		const float PitchRad = FMath::DegreesToRadians(FMath::Abs(PitchDeg));
		const FVector CameraForward(FMath::Cos(PitchRad), 0.f, -FMath::Sin(PitchRad));
		const FVector CameraUp(FMath::Sin(PitchRad), 0.f, FMath::Cos(PitchRad));
		const FVector CameraPosition = -CameraForward * Distance;
		const FVector ToEndpoint = FVector(0.f, 0.f, EndpointHeight - PivotHeight) - CameraPosition;
		const float VerticalHalfTan = FMath::Tan(FMath::DegreesToRadians(HorizontalFOVDeg * 0.5f)) / AspectRatio;
		return 0.5f + 0.5f * FVector::DotProduct(ToEndpoint, CameraUp)
			/ (FVector::DotProduct(ToEndpoint, CameraForward) * VerticalHalfTan);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRCameraFramingEvaluateRigTest,
	"CGR.CameraFraming.EvaluateRig",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRCameraFramingEvaluateRigTest::RunTest(const FString& Parameters)
{
	const FZoomRigParams P = DefaultRig();

	const FZoomRigSample Near = CameraFramingMath::EvaluateRig(P, 0.f);
	TestEqual(TEXT("key 0 팔길이 = Min"), Near.ArmLength, 1600.f, 0.01f);
	TestEqual(TEXT("key 0 피치 = In"), Near.PitchDeg, -40.f, 0.01f);
	TestEqual(TEXT("key 0 FOV = In"), Near.HorizontalFOVDeg, 30.f, 0.01f);

	const FZoomRigSample Far = CameraFramingMath::EvaluateRig(P, 1.f);
	TestEqual(TEXT("key 1 팔길이 = Max"), Far.ArmLength, 320000.f, 0.01f);
	TestEqual(TEXT("key 1 피치 = Out"), Far.PitchDeg, -55.f, 0.01f);
	TestEqual(TEXT("key 1 FOV = Out"), Far.HorizontalFOVDeg, 20.f, 0.01f);

	const FZoomRigSample Mid = CameraFramingMath::EvaluateRig(P, 0.5f);
	TestEqual(TEXT("key 0.5 팔길이 = 중간"), Mid.ArmLength, 160800.f, 0.01f);
	TestEqual(TEXT("key 0.5 피치 = 중간"), Mid.PitchDeg, -47.5f, 0.01f);
	TestEqual(TEXT("key 0.5 FOV = 중간"), Mid.HorizontalFOVDeg, 25.f, 0.01f);

	// 범위 밖 key는 클램프 — 탐색이 [0,1] 밖을 넘겨도 리그가 외삽하지 않는다
	TestEqual(TEXT("key 2 → key 1"), CameraFramingMath::EvaluateRig(P, 2.f).ArmLength, 320000.f, 0.01f);
	TestEqual(TEXT("key -1 → key 0"), CameraFramingMath::EvaluateRig(P, -1.f).ArmLength, 1600.f, 0.01f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRCameraFramingVerticalHalfTangentTest,
	"CGR.CameraFraming.VerticalHalfTangent",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRCameraFramingVerticalHalfTangentTest::RunTest(const FString& Parameters)
{
	// hFOV 30°, 카메라 종횡비 16:9 → tan(15°)/1.777778 = 0.150721 (수직 FOV ≈ 17.1°). 30/2=15° 그대로 쓰면 0.267949로 1.78배 틀린다
	TestEqual(TEXT("30° @16:9 수직 반각 탄젠트"), CameraFramingMath::VerticalHalfTangent(30.f, 1.777778f), 0.150721f, 0.0005f);
	// 종횡비 1이면 수평=수직
	TestEqual(TEXT("30° @1:1 = tan(15°)"), CameraFramingMath::VerticalHalfTangent(30.f, 1.f), 0.267949f, 0.0005f);
	// 수평 반각 = 수직 반각 × 뷰포트 종횡비. 16:9 뷰포트면 원래 tan(hFOV/2)로 돌아온다
	TestEqual(TEXT("수평 반각 @16:9 뷰포트 = tan(15°)"), CameraFramingMath::HorizontalHalfTangent(30.f, 1.777778f, 1.777778f), 0.267949f, 0.0005f);
	// 19.5:9 폰(2.1667)은 수직 고정·수평만 넓어진다
	TestEqual(TEXT("수평 반각 @19.5:9 뷰포트"), CameraFramingMath::HorizontalHalfTangent(30.f, 1.777778f, 2.166667f), 0.326563f, 0.0005f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRCameraFramingRequiredArmLengthTest,
	"CGR.CameraFraming.RequiredArmLength",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRCameraFramingRequiredArmLengthTest::RunTest(const FString& Parameters)
{
	// 원근 투영에서는 위쪽 끝이 카메라에 더 가까워 같은 점유율의 필요 거리가 근삿값과 다르다.
	TestEqual(TEXT("H1000 피치-40 FOV30 50%"), CameraFramingMath::RequiredArmLengthForHeight(1000.f, -40.f, 30.f, 1.777778f, 0.5f), 5062.2f, 1.f);
	// 피치 0(보정 없음), 100% → 1000 / (2·0.150721) = 3317.4
	TestEqual(TEXT("피치 0 100%"), CameraFramingMath::RequiredArmLengthForHeight(1000.f, 0.f, 30.f, 1.777778f, 1.f), 3317.4f, 1.f);
	// 원근 효과 때문에 점유율을 두 배로 키워도 거리는 정확히 절반이 아니다.
	const float Half = CameraFramingMath::RequiredArmLengthForHeight(1000.f, -40.f, 30.f, 1.777778f, 0.25f);
	const float Full = CameraFramingMath::RequiredArmLengthForHeight(1000.f, -40.f, 30.f, 1.777778f, 0.5f);
	TestTrue(TEXT("점유율 반 → 거리는 두 배보다 약간 멀다"), Half > Full * 2.f);
	// 피치 부호는 무시(내려다보는 각도의 절댓값)
	TestEqual(TEXT("피치 +40 == -40"),
		CameraFramingMath::RequiredArmLengthForHeight(1000.f, 40.f, 30.f, 1.777778f, 0.5f),
		CameraFramingMath::RequiredArmLengthForHeight(1000.f, -40.f, 30.f, 1.777778f, 0.5f), 0.01f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRCameraFramingSolveArmLengthTest,
	"CGR.CameraFraming.SolveZoomForArmLength",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRCameraFramingSolveArmLengthTest::RunTest(const FString& Parameters)
{
	const FZoomRigParams P = DefaultRig();

	// 항등 커브: 팔길이는 z에 선형 → 중간값은 z=0.5
	TestEqual(TEXT("160800 → z 0.5"), CameraFramingMath::SolveZoomForArmLength(P, &IdentityCurve, 160800.f), 0.5f, 0.001f);
	TestTrue(TEXT("Min → z≈0"), CameraFramingMath::SolveZoomForArmLength(P, &IdentityCurve, 1600.f) <= 0.001f);
	TestTrue(TEXT("Max → z≈1"), CameraFramingMath::SolveZoomForArmLength(P, &IdentityCurve, 320000.f) >= 0.999f);

	// 범위 밖 요청은 가까운 끝점으로 클램프
	TestEqual(TEXT("100cm → z 0"), CameraFramingMath::SolveZoomForArmLength(P, &IdentityCurve, 100.f), 0.f, 0.0001f);
	TestEqual(TEXT("1e7cm → z 1"), CameraFramingMath::SolveZoomForArmLength(P, &IdentityCurve, 10000000.f), 1.f, 0.0001f);

	// 비선형 커브(key = z²): key 0.25 의 팔길이 81200 은 z=0.5 에서 나온다 — 커브 역산이 솔버 안에 있음을 고정
	const float Solved = CameraFramingMath::SolveZoomForArmLength(P, [](float Z) { return Z * Z; }, 81200.f);
	TestEqual(TEXT("z² 커브 81200 → z 0.5"), Solved, 0.5f, 0.001f);

	// 결과 팔길이 오차 ≤ 1cm (기본 허용치)
	const float Z = CameraFramingMath::SolveZoomForArmLength(P, &IdentityCurve, 23456.f);
	TestEqual(TEXT("잔차 ≤ 1cm"), CameraFramingMath::EvaluateRig(P, Z).ArmLength, 23456.f, 1.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRCameraFramingSolveHeightTest,
	"CGR.CameraFraming.SolveZoomForHeight",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRCameraFramingSolveHeightTest::RunTest(const FString& Parameters)
{
	const FZoomRigParams P = DefaultRig();

	// 자기일관성: 구한 z의 리그(피치·FOV)로 다시 계산한 필요 거리와 팔길이가 1cm 안에서 같다 (= 고정점)
	const float Z = CameraFramingMath::SolveZoomForHeight(P, &IdentityCurve, 1000.f, 0.5f);
	TestTrue(TEXT("z 는 (0,1) 내부"), Z > 0.f && Z < 1.f);
	const FZoomRigSample S = CameraFramingMath::EvaluateRig(P, Z);
	const float Required = CameraFramingMath::RequiredArmLengthForHeight(1000.f, S.PitchDeg, S.HorizontalFOVDeg, P.CameraAspectRatio, 0.5f);
	TestEqual(TEXT("팔길이 == 필요 거리"), S.ArmLength, Required, 1.f);

	// 단조: 더 높은 건물 → 더 멀리(z 증가), 더 큰 점유율 → 더 가까이(z 감소)
	const float ZTall = CameraFramingMath::SolveZoomForHeight(P, &IdentityCurve, 5000.f, 0.5f);
	const float ZBig = CameraFramingMath::SolveZoomForHeight(P, &IdentityCurve, 1000.f, 0.8f);
	TestTrue(TEXT("H 5배 → z 증가"), ZTall > Z);
	TestTrue(TEXT("점유율 80% → z 감소"), ZBig < Z);

	// 비선형 커브에서도 자기일관성 유지
	const auto Curve = [](float Zoom) { return Zoom * Zoom; };
	const float Zc = CameraFramingMath::SolveZoomForHeight(P, Curve, 3000.f, 0.6f);
	const FZoomRigSample Sc = CameraFramingMath::EvaluateRig(P, Curve(Zc));
	TestEqual(TEXT("z² 커브 팔길이 == 필요 거리"), Sc.ArmLength,
		CameraFramingMath::RequiredArmLengthForHeight(3000.f, Sc.PitchDeg, Sc.HorizontalFOVDeg, P.CameraAspectRatio, 0.6f), 1.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRCameraFramingSolveClampsTest,
	"CGR.CameraFraming.SolveClamps",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRCameraFramingSolveClampsTest::RunTest(const FString& Parameters)
{
	const FZoomRigParams P = DefaultRig();
	// 1cm 건물: 필요 거리(≈5)가 Min 팔길이(1600)보다 작아 양 끝 잔차 모두 양수 → 가장 가까운 끝 z=0
	TestEqual(TEXT("H 1cm → z 0"), CameraFramingMath::SolveZoomForHeight(P, &IdentityCurve, 1.f, 0.5f), 0.f, 0.0001f);
	// 1e9cm 건물: 필요 거리가 Max(320000)보다 커 양 끝 잔차 모두 음수 → z=1
	TestEqual(TEXT("H 1e9cm → z 1"), CameraFramingMath::SolveZoomForHeight(P, &IdentityCurve, 1000000000.f, 0.5f), 1.f, 0.0001f);
	// 반복 0회면 끝점 판정만으로 중간값 반환(발산 아님)
	const float Z0 = CameraFramingMath::SolveZoomForHeight(P, &IdentityCurve, 1000.f, 0.5f, 0.5f, 0, 1.f);
	TestTrue(TEXT("반복 0회 → 0.5"), FMath::IsNearlyEqual(Z0, 0.5f, 0.0001f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRCameraFramingPerspectiveTest,
	"CGR.CameraFraming.PerspectiveProjection",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRCameraFramingPerspectiveTest::RunTest(const FString& Parameters)
{
	const FZoomRigParams P = MainMapRig();
	const float Heights[] = { 5000.f, 10000.f, 15000.f };
	for (const float Height : Heights)
	{
		const float Zoom = CameraFramingMath::SolveZoomForHeight(P, &IdentityCurve, Height, 0.94f);
		const FZoomRigSample Sample = CameraFramingMath::EvaluateRig(P, Zoom);
		const float Distance = Sample.ArmLength + P.CameraPullBackCm;
		const FVerticalFramingResult Framing = CameraFramingMath::SolveVerticalFraming(
			Height, Distance, Sample.PitchDeg, Sample.HorizontalFOVDeg, P.CameraAspectRatio, 0.94f);
		const float Bottom = ProjectWorldHeight(0.f, Framing.PivotHeight, Distance, Sample.PitchDeg, Sample.HorizontalFOVDeg, P.CameraAspectRatio);
		const float Top = ProjectWorldHeight(Height, Framing.PivotHeight, Distance, Sample.PitchDeg, Sample.HorizontalFOVDeg, P.CameraAspectRatio);
		TestTrue(TEXT("중간 높이의 건물은 목표 프레임에 들어온다"), Framing.bFitsInFrame);
		TestEqual(TEXT("바닥 투영은 화면 3%"), Bottom, 0.03f, 0.001f);
		TestEqual(TEXT("옥상 투영은 화면 97%"), Top, 0.97f, 0.001f);
		TestEqual(TEXT("반환 화면 좌표는 실제 투영과 일치"), Framing.TopScreenPosition, Top, 0.0001f);
		TestEqual(TEXT("반환 바닥 좌표는 실제 투영과 일치"), Framing.BottomScreenPosition, Bottom, 0.0001f);
	}

	const float ShortHeight = 1200.f;
	const float ShortZoom = CameraFramingMath::SolveZoomForHeight(P, &IdentityCurve, ShortHeight, 0.94f);
	TestEqual(TEXT("저층 건물은 최근접 줌"), ShortZoom, 0.f, 0.0001f);
	const FZoomRigSample Near = CameraFramingMath::EvaluateRig(P, ShortZoom);
	const float NearDistance = Near.ArmLength + P.CameraPullBackCm;
	const FVerticalFramingResult ShortFraming = CameraFramingMath::SolveVerticalFraming(
		ShortHeight, NearDistance, Near.PitchDeg, Near.HorizontalFOVDeg, P.CameraAspectRatio, 0.94f);
	const float ShortBottom = ProjectWorldHeight(0.f, ShortFraming.PivotHeight, NearDistance, Near.PitchDeg, Near.HorizontalFOVDeg, P.CameraAspectRatio);
	const float ShortTop = ProjectWorldHeight(ShortHeight, ShortFraming.PivotHeight, NearDistance, Near.PitchDeg, Near.HorizontalFOVDeg, P.CameraAspectRatio);
	TestTrue(TEXT("저층 건물도 중앙에 둔다"), ShortFraming.bFitsInFrame);
	TestEqual(TEXT("저층 상하 여백 대칭"), ShortTop + ShortBottom, 1.f, 0.001f);
	TestTrue(TEXT("최근접 줌의 저층 점유율은 목표보다 작다"), ShortTop - ShortBottom < 0.94f);

	const float TallHeight = 25000.f;
	const float TallZoom = CameraFramingMath::SolveZoomForHeight(P, &IdentityCurve, TallHeight, 0.94f);
	TestEqual(TEXT("초고층은 최대 줌"), TallZoom, 1.f, 0.0001f);
	const FZoomRigSample Far = CameraFramingMath::EvaluateRig(P, TallZoom);
	const float FarDistance = Far.ArmLength + P.CameraPullBackCm;
	const FVerticalFramingResult TallFraming = CameraFramingMath::SolveVerticalFraming(
		TallHeight, FarDistance, Far.PitchDeg, Far.HorizontalFOVDeg, P.CameraAspectRatio, 0.94f);
	const float TallBottom = ProjectWorldHeight(0.f, TallFraming.PivotHeight, FarDistance, Far.PitchDeg, Far.HorizontalFOVDeg, P.CameraAspectRatio);
	const float TallTop = ProjectWorldHeight(TallHeight, TallFraming.PivotHeight, FarDistance, Far.PitchDeg, Far.HorizontalFOVDeg, P.CameraAspectRatio);
	TestFalse(TEXT("초고층은 프레임을 넘친다"), TallFraming.bFitsInFrame);
	TestEqual(TEXT("초고층 옥상은 97%에 고정"), TallTop, 0.97f, 0.001f);
	TestTrue(TEXT("초고층 바닥은 화면 아래로 흐른다"), TallBottom < 0.f);

	const float Height = 10000.f;
	const float WithPullBack = CameraFramingMath::SolveZoomForHeight(P, &IdentityCurve, Height, 0.94f);
	FZoomRigParams NoPullBackRig = P;
	NoPullBackRig.CameraPullBackCm = 0.f;
	const float WithoutPullBack = CameraFramingMath::SolveZoomForHeight(NoPullBackRig, &IdentityCurve, Height, 0.94f);
	TestTrue(TEXT("소켓 후퇴가 있으면 팔길이를 더 짧게 잡는다"), WithPullBack < WithoutPullBack);

	const FVerticalFramingResult FixedDistanceFraming = CameraFramingMath::SolveVerticalFraming(
		5000.f, 10000.f, -40.f, 52.f, P.CameraAspectRatio, 0.2f, 0.6f, false);
	const float FixedBottom = ProjectWorldHeight(0.f, FixedDistanceFraming.PivotHeight, 10000.f, -40.f, 52.f, P.CameraAspectRatio);
	const float FixedTop = ProjectWorldHeight(5000.f, FixedDistanceFraming.PivotHeight, 10000.f, -40.f, 52.f, P.CameraAspectRatio);
	TestFalse(TEXT("고정 거리에서는 목표 점유율을 넘을 수 있다"), FixedDistanceFraming.bFitsInFrame);
	TestTrue(TEXT("목표 점유율을 넘겨도 전체는 화면 안에 있다"), FixedBottom > 0.f && FixedTop < 1.f);
	TestEqual(TEXT("범용 Actor는 고정 거리에서 상하 중심을 유지한다"), FixedTop + FixedBottom, 1.f, 0.001f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRCameraFramingAsymmetricMarginsTest,
	"CGR.CameraFraming.AsymmetricMargins",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRCameraFramingAsymmetricMarginsTest::RunTest(const FString& Parameters)
{
	const FZoomRigParams P = MainMapRig();
	for (const float Height : {1200.f, 5000.f, 10000.f, 25000.f})
	{
		const float Zoom = CameraFramingMath::SolveZoomForHeight(P, &IdentityCurve, Height, 0.7f, 0.45f);
		const FZoomRigSample Sample = CameraFramingMath::EvaluateRig(P, Zoom);
		const float Distance = Sample.ArmLength + P.CameraPullBackCm;
		const FVerticalFramingResult Framing = CameraFramingMath::SolveVerticalFraming(
			Height, Distance, Sample.PitchDeg, Sample.HorizontalFOVDeg, P.CameraAspectRatio, 0.7f, 0.8f, true, 0.45f);
		const float Bottom = ProjectWorldHeight(0.f, Framing.PivotHeight, Distance, Sample.PitchDeg, Sample.HorizontalFOVDeg, P.CameraAspectRatio);
		const float Top = ProjectWorldHeight(Height, Framing.PivotHeight, Distance, Sample.PitchDeg, Sample.HorizontalFOVDeg, P.CameraAspectRatio);
		TestEqual(TEXT("asymmetric roof result matches independent projection"), Framing.TopScreenPosition, Top, 0.0001f);
		TestEqual(TEXT("asymmetric base result matches independent projection"), Framing.BottomScreenPosition, Bottom, 0.0001f);
		if (Height == 1200.f)
		{
			TestEqual(TEXT("short building stops at minimum zoom"), Zoom, 0.f, 0.0001f);
			TestTrue(TEXT("short building fits the asymmetric frame"), Framing.bFitsInFrame);
			TestTrue(TEXT("short building leaves extra space inside both margins"), Top < 0.8f && Bottom > 0.1f);
			TestEqual(TEXT("short building centers in the requested frame"), (Top + Bottom) * 0.5f, 0.45f, 0.001f);
		}
		else if (Height == 25000.f)
		{
			TestEqual(TEXT("tall building stops at maximum zoom"), Zoom, 1.f, 0.0001f);
			TestFalse(TEXT("tall building exceeds the asymmetric frame"), Framing.bFitsInFrame);
			TestEqual(TEXT("overflow preserves the 20 percent top margin"), Top, 0.8f, 0.001f);
			TestTrue(TEXT("overflow lets the base leave the screen"), Bottom < 0.f);
		}
		else
		{
			TestTrue(TEXT("mid-height building fits the asymmetric frame"), Framing.bFitsInFrame);
			TestEqual(TEXT("roof leaves 20 percent top margin"), Top, 0.8f, 0.001f);
			TestEqual(TEXT("base leaves 10 percent bottom margin"), Bottom, 0.1f, 0.001f);
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
