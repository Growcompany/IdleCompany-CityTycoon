#include "Player/Components/CameraFramingMath.h"

namespace
{
	// 잔차 부호가 바뀌는 구간을 [0,1]에서 이분. 양 끝 부호가 같으면(리그 범위 밖) 부호가 가리키는 끝점 — 계속 모자라면 1, 계속 남으면 0
	float BisectZoom(TFunctionRef<float(float)> Residual, int32 MaxIterations, float ToleranceCm)
	{
		float Lo = 0.f;
		float Hi = 1.f;
		float ResLo = Residual(Lo);
		const float ResHi = Residual(Hi);
		if (FMath::Abs(ResLo) <= ToleranceCm) return Lo;
		if (FMath::Abs(ResHi) <= ToleranceCm) return Hi;
		if ((ResLo > 0.f) == (ResHi > 0.f))
		{
			return ResLo < 0.f ? Hi : Lo;
		}
		for (int32 Iteration = 0; Iteration < MaxIterations; ++Iteration)
		{
			const float Mid = (Lo + Hi) * 0.5f;
			const float ResMid = Residual(Mid);
			if (FMath::Abs(ResMid) <= ToleranceCm) return Mid;
			if ((ResMid > 0.f) == (ResLo > 0.f))
			{
				Lo = Mid;
				ResLo = ResMid;
			}
			else
			{
				Hi = Mid;
			}
		}
		return (Lo + Hi) * 0.5f;
	}
}

FZoomRigSample CameraFramingMath::EvaluateRig(const FZoomRigParams& Params, float Key)
{
	const float K = FMath::Clamp(Key, 0.f, 1.f);
	FZoomRigSample Sample;
	Sample.ArmLength = FMath::Lerp(Params.MinArmLength, Params.MaxArmLength, K);
	Sample.PitchDeg = FMath::Lerp(Params.PitchInDeg, Params.PitchOutDeg, K);
	Sample.HorizontalFOVDeg = FMath::Lerp(Params.FOVInDeg, Params.FOVOutDeg, K);
	return Sample;
}

float CameraFramingMath::VerticalHalfTangent(float HorizontalFOVDeg, float CameraAspectRatio)
{
	const float HalfTanX = FMath::Tan(FMath::DegreesToRadians(FMath::Max(0.001f, HorizontalFOVDeg) * 0.5f));
	return HalfTanX / FMath::Max(CameraAspectRatio, UE_KINDA_SMALL_NUMBER);
}

float CameraFramingMath::HorizontalHalfTangent(float HorizontalFOVDeg, float CameraAspectRatio, float ViewportAspectRatio)
{
	return VerticalHalfTangent(HorizontalFOVDeg, CameraAspectRatio) * FMath::Max(ViewportAspectRatio, UE_KINDA_SMALL_NUMBER);
}

float CameraFramingMath::RequiredArmLengthForHeight(float Height, float PitchDeg, float HorizontalFOVDeg, float CameraAspectRatio, float ScreenHeightRatio, float ScreenCenterRatioY)
{
	const double PitchRad = FMath::DegreesToRadians(static_cast<double>(FMath::Abs(PitchDeg)));
	const double S = FMath::Sin(PitchRad);
	const double C = FMath::Cos(PitchRad);
	const double T = VerticalHalfTangent(HorizontalFOVDeg, CameraAspectRatio);
	const double Ratio = FMath::Max(static_cast<double>(ScreenHeightRatio), static_cast<double>(UE_KINDA_SMALL_NUMBER));
	const double CenterTan = (2.0 * static_cast<double>(ScreenCenterRatioY) - 1.0) * T;
	const double ShiftedCos = C + CenterTan * S;
	const double RatioTanSin = Ratio * T * S;
	// 목표 화면 구간의 위·아래 끝점을 역투영해 실제 카메라 거리를 구한다.
	const double Distance = static_cast<double>(Height) * (ShiftedCos * ShiftedCos - RatioTanSin * RatioTanSin) / (2.0 * Ratio * T * C);
	return static_cast<float>(FMath::Max(0.0, Distance));
}

FVerticalFramingResult CameraFramingMath::SolveVerticalFraming(float Height, float CameraDistance, float PitchDeg,
	float HorizontalFOVDeg, float CameraAspectRatio, float ScreenHeightRatio, float OverflowTopScreenPosition, bool bAnchorOverflowToTop, float ScreenCenterRatioY)
{
	const double H = FMath::Max(static_cast<double>(Height), 0.0);
	const double D = FMath::Max(static_cast<double>(CameraDistance), static_cast<double>(UE_KINDA_SMALL_NUMBER));
	const double PitchRad = FMath::DegreesToRadians(static_cast<double>(FMath::Abs(PitchDeg)));
	const double S = FMath::Sin(PitchRad);
	const double C = FMath::Cos(PitchRad);
	const double T = VerticalHalfTangent(HorizontalFOVDeg, CameraAspectRatio);

	const double CenterTan = (2.0 * static_cast<double>(ScreenCenterRatioY) - 1.0) * T;
	const double ShiftedCos = C + CenterTan * S;
	// 두 끝점의 화면상 평균을 지정한 중심에 맞춘다. 유리화한 근은 작은 높이와 수평 피치에서도 안정적이다.
	const double DepthRoot = FMath::Sqrt(C * C * D * D + ShiftedCos * ShiftedCos * S * S * H * H);
	const double CenteredPivot = H * 0.5 - D * CenterTan / ShiftedCos
		+ ShiftedCos * S * H * H / (2.0 * (DepthRoot + C * D));
	const auto Project = [D, S, C, T](double RelativeHeight)
	{
		return 0.5 + 0.5 * (C * RelativeHeight / (D - S * RelativeHeight)) / T;
	};
	const double CenteredTop = Project(H - CenteredPivot);
	const double CenteredBottom = Project(-CenteredPivot);

	FVerticalFramingResult Result;
	Result.bFitsInFrame = CenteredTop - CenteredBottom <= static_cast<double>(ScreenHeightRatio) + 0.001;
	double Pivot = CenteredPivot;
	if (!Result.bFitsInFrame && bAnchorOverflowToTop)
	{
		const double TargetNdc = 2.0 * static_cast<double>(OverflowTopScreenPosition) - 1.0;
		const double A = TargetNdc * T;
		const double TopRelativeHeight = A * D / (C + A * S);
		Pivot = H - TopRelativeHeight;
	}
	Result.PivotHeight = static_cast<float>(Pivot);
	Result.TopScreenPosition = static_cast<float>(Project(H - Pivot));
	Result.BottomScreenPosition = static_cast<float>(Project(-Pivot));
	return Result;
}

float CameraFramingMath::SolveZoomForHeight(const FZoomRigParams& Params, TFunctionRef<float(float)> CurveEval, float Height, float ScreenHeightRatio, float ScreenCenterRatioY, int32 MaxIterations, float ToleranceCm)
{
	return BisectZoom([&](float Zoom)
	{
		const FZoomRigSample Sample = EvaluateRig(Params, CurveEval(Zoom));
		// 실제 피사체 거리 = 팔길이 + 소켓 후퇴. RequiredArmLengthForHeight 는 "필요한 카메라 거리"를 돌려주므로
		// 같은 축에서 비교해야 한다 — 후퇴를 빼먹으면 늘 그만큼 멀리 서서 피사체가 작게 잡힌다.
		return Sample.ArmLength + Params.CameraPullBackCm
			- RequiredArmLengthForHeight(Height, Sample.PitchDeg, Sample.HorizontalFOVDeg, Params.CameraAspectRatio, ScreenHeightRatio, ScreenCenterRatioY);
	}, MaxIterations, ToleranceCm);
}

float CameraFramingMath::SolveZoomForArmLength(const FZoomRigParams& Params, TFunctionRef<float(float)> CurveEval, float DesiredArmLength, int32 MaxIterations, float ToleranceCm)
{
	return BisectZoom([&](float Zoom)
	{
		return EvaluateRig(Params, CurveEval(Zoom)).ArmLength - DesiredArmLength;
	}, MaxIterations, ToleranceCm);
}
