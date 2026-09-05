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
	// H=1000, 피치 -40°(cos=0.766044), hFOV 30° @16:9, 화면 50% → 766.044 / (2·0.150721·0.5) = 5082.5
	TestEqual(TEXT("H1000 피치-40 FOV30 50%"), CameraFramingMath::RequiredArmLengthForHeight(1000.f, -40.f, 30.f, 1.777778f, 0.5f), 5082.5f, 1.f);
	// 피치 0(보정 없음), 100% → 1000 / (2·0.150721) = 3317.4
	TestEqual(TEXT("피치 0 100%"), CameraFramingMath::RequiredArmLengthForHeight(1000.f, 0.f, 30.f, 1.777778f, 1.f), 3317.4f, 1.f);
	// 점유율 2배 → 거리 절반
	const float Half = CameraFramingMath::RequiredArmLengthForHeight(1000.f, -40.f, 30.f, 1.777778f, 0.25f);
	const float Full = CameraFramingMath::RequiredArmLengthForHeight(1000.f, -40.f, 30.f, 1.777778f, 0.5f);
	TestEqual(TEXT("점유율 반 → 거리 2배"), Half, Full * 2.f, 0.5f);
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
	const float Z0 = CameraFramingMath::SolveZoomForHeight(P, &IdentityCurve, 1000.f, 0.5f, 0, 1.f);
	TestTrue(TEXT("반복 0회 → 0.5"), FMath::IsNearlyEqual(Z0, 0.5f, 0.0001f));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
