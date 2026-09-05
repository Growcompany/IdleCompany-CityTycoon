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

float CameraFramingMath::RequiredArmLengthForHeight(float Height, float PitchDeg, float HorizontalFOVDeg, float CameraAspectRatio, float ScreenHeightRatio)
{
	const float VisibleHeight = Height * FMath::Cos(FMath::DegreesToRadians(FMath::Abs(PitchDeg)));
	const float Denominator = 2.f * VerticalHalfTangent(HorizontalFOVDeg, CameraAspectRatio) * FMath::Max(ScreenHeightRatio, UE_KINDA_SMALL_NUMBER);
	return VisibleHeight / Denominator;
}

float CameraFramingMath::SolveZoomForHeight(const FZoomRigParams& Params, TFunctionRef<float(float)> CurveEval, float Height, float ScreenHeightRatio, int32 MaxIterations, float ToleranceCm)
{
	return BisectZoom([&](float Zoom)
	{
		const FZoomRigSample Sample = EvaluateRig(Params, CurveEval(Zoom));
		return Sample.ArmLength - RequiredArmLengthForHeight(Height, Sample.PitchDeg, Sample.HorizontalFOVDeg, Params.CameraAspectRatio, ScreenHeightRatio);
	}, MaxIterations, ToleranceCm);
}

float CameraFramingMath::SolveZoomForArmLength(const FZoomRigParams& Params, TFunctionRef<float(float)> CurveEval, float DesiredArmLength, int32 MaxIterations, float ToleranceCm)
{
	return BisectZoom([&](float Zoom)
	{
		return EvaluateRig(Params, CurveEval(Zoom)).ArmLength - DesiredArmLength;
	}, MaxIterations, ToleranceCm);
}
