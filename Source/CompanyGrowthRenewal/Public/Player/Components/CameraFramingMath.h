#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"

// 줌 리그 파라미터 — 팔길이·피치·FOV가 줌 key(=C_Zoom(ZoomValue)) 하나에 전부 선형. MovementInputHandler Zoom Settings 복사본
struct COMPANYGROWTHRENEWAL_API FZoomRigParams
{
	float MinArmLength = 1600.f;
	float MaxArmLength = 320000.f;
	float PitchInDeg = -40.f;
	float PitchOutDeg = -55.f;
	float FOVInDeg = 30.f;   // UCameraComponent::FieldOfView = 수평 FOV
	float FOVOutDeg = 20.f;
	float CameraAspectRatio = 1.777778f; // UCameraComponent::AspectRatio — MaintainYFOV에서 수직 FOV 변환 기준
	// SpringArm::SocketOffset.X 가 카메라를 팔 끝에서 한 번 더 뒤로 민 양(양수 = 더 뒤). APlayerCamera 는 1200.
	// 팔길이를 그대로 피사체 거리로 쓰면 실제 카메라가 이만큼 더 멀어 피사체가 목표 비율보다 작게 잡힌다
	// (근거리일수록 오차가 크다 — arm 7641 에서 약 16%). 기본 0 = 소켓 오프셋 없는 순수 리그.
	float CameraPullBackCm = 0.f;
};

struct COMPANYGROWTHRENEWAL_API FZoomRigSample
{
	float ArmLength = 0.f;
	float PitchDeg = 0.f;
	float HorizontalFOVDeg = 0.f;
};

struct COMPANYGROWTHRENEWAL_API FVerticalFramingResult
{
	float PivotHeight = 0.f;
	float TopScreenPosition = 0.5f;
	float BottomScreenPosition = 0.5f;
	bool bFitsInFrame = true;
};

// 건물 포커스 프레이밍의 순수 계산부. 월드/컴포넌트에 의존하지 않아 자동화 테스트로 고정한다.
namespace CameraFramingMath
{
	// key ∈ [0,1] (클램프). 리그 세 값 전부 Lerp
	COMPANYGROWTHRENEWAL_API FZoomRigSample EvaluateRig(const FZoomRigParams& Params, float Key);

	// 엔진 기본 제약(MaintainYFOV): tan(vFOV/2) = tan(hFOV/2) / CameraAspectRatio (CameraStackTypes.cpp 281~288행과 동일)
	COMPANYGROWTHRENEWAL_API float VerticalHalfTangent(float HorizontalFOVDeg, float CameraAspectRatio);

	// 수직 FOV는 고정, 실제 수평 반각은 뷰포트 종횡비에 따라 변함 → 수직 반각 × 뷰포트 종횡비
	COMPANYGROWTHRENEWAL_API float HorizontalHalfTangent(float HorizontalFOVDeg, float CameraAspectRatio, float ViewportAspectRatio);

	// 피벗을 조정해 높이 H를 ScreenCenterRatioY 중심의 화면 세로 Ratio에 담는 실제 카메라 거리.
	// 팔길이를 구하려면 반환값에서 CameraPullBackCm를 뺀다.
	COMPANYGROWTHRENEWAL_API float RequiredArmLengthForHeight(float Height, float PitchDeg, float HorizontalFOVDeg, float CameraAspectRatio, float ScreenHeightRatio, float ScreenCenterRatioY = 0.5f);

	// 바닥 기준 피벗 높이를 계산한다. 들어오면 ScreenCenterRatioY 중심, 넘치면 꼭대기를 OverflowTopScreenPosition에 고정한다.
	// 반환 화면 좌표는 0=화면 아래, 1=화면 위이며 프레임 밖이면 그 범위를 벗어난다.
	// bAnchorOverflowToTop=false면 점유율을 넘겨도 지정한 화면 중심을 유지한다.
	COMPANYGROWTHRENEWAL_API FVerticalFramingResult SolveVerticalFraming(float Height, float CameraDistance, float PitchDeg,
		float HorizontalFOVDeg, float CameraAspectRatio, float ScreenHeightRatio, float OverflowTopScreenPosition = 0.97f, bool bAnchorOverflowToTop = true, float ScreenCenterRatioY = 0.5f);

	// 잔차 f(z) = Arm(curve(z)) − Required(curve(z)) 의 근을 [0,1] 이분법으로. CurveEval = ZoomValue→key.
	// 양 끝 부호가 같으면(요청이 리그 범위 밖) 부호가 가리키는 끝점 반환 — 계속 모자라면 1, 계속 남으면 0
	COMPANYGROWTHRENEWAL_API float SolveZoomForHeight(const FZoomRigParams& Params, TFunctionRef<float(float)> CurveEval, float Height, float ScreenHeightRatio, float ScreenCenterRatioY = 0.5f, int32 MaxIterations = 24, float ToleranceCm = 1.f);

	// 팔길이가 DesiredArmLength가 되는 줌값 (FocusOnLocation/FocusOnAreaRadius용). 같은 이분법
	COMPANYGROWTHRENEWAL_API float SolveZoomForArmLength(const FZoomRigParams& Params, TFunctionRef<float(float)> CurveEval, float DesiredArmLength, int32 MaxIterations = 24, float ToleranceCm = 1.f);
}
