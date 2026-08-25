#include "Player/OfficeCameraFraming.h"

namespace
{
constexpr float CameraNearEpsilon = 0.01f;

bool IsSafeFrameValid(const FVector2D SafeMin, const FVector2D SafeMax)
{
	return SafeMin.X >= 0.f
		&& SafeMin.Y >= 0.f
		&& SafeMax.X <= 1.f
		&& SafeMax.Y <= 1.f
		&& SafeMin.X <= SafeMax.X
		&& SafeMin.Y <= SafeMax.Y;
}
}

FBox FOfficeCameraFraming::MakeHeroBounds(
	const FBox2D& FloorBounds,
	const float StructuralFloorZ,
	const float VisibleDepthCm)
{
	if (!FloorBounds.bIsValid
		|| !FMath::IsFinite(FloorBounds.Min.X)
		|| !FMath::IsFinite(FloorBounds.Min.Y)
		|| !FMath::IsFinite(FloorBounds.Max.X)
		|| !FMath::IsFinite(FloorBounds.Max.Y)
		|| !FMath::IsFinite(StructuralFloorZ)
		|| !FMath::IsFinite(VisibleDepthCm)
		|| VisibleDepthCm <= 0.f)
	{
		return FBox(ForceInit);
	}

	const FVector2D FloorCenter = FloorBounds.GetCenter();
	return FBox(
		FVector(FloorCenter.X, FloorBounds.Min.Y, StructuralFloorZ - VisibleDepthCm),
		FVector(FloorBounds.Max.X, FloorCenter.Y, StructuralFloorZ));
}

TStaticArray<FVector, 8> FOfficeCameraFraming::MakeBoxCorners(const FBox& Bounds)
{
	TStaticArray<FVector, 8> Corners;
	Corners[0] = FVector(Bounds.Min.X, Bounds.Min.Y, Bounds.Min.Z);
	Corners[1] = FVector(Bounds.Max.X, Bounds.Min.Y, Bounds.Min.Z);
	Corners[2] = FVector(Bounds.Min.X, Bounds.Max.Y, Bounds.Min.Z);
	Corners[3] = FVector(Bounds.Max.X, Bounds.Max.Y, Bounds.Min.Z);
	Corners[4] = FVector(Bounds.Min.X, Bounds.Min.Y, Bounds.Max.Z);
	Corners[5] = FVector(Bounds.Max.X, Bounds.Min.Y, Bounds.Max.Z);
	Corners[6] = FVector(Bounds.Min.X, Bounds.Max.Y, Bounds.Max.Z);
	Corners[7] = FVector(Bounds.Max.X, Bounds.Max.Y, Bounds.Max.Z);
	return Corners;
}

bool FOfficeCameraFraming::ProjectWorldPoint(
	const FVector& Point,
	const FTransform& CameraTransform,
	const float HorizontalFOVDegrees,
	const float AspectRatio,
	FVector2D& OutNormalizedPosition)
{
	if (!FMath::IsFinite(HorizontalFOVDegrees)
		|| HorizontalFOVDegrees <= 0.f
		|| HorizontalFOVDegrees >= 180.f
		|| !FMath::IsFinite(AspectRatio)
		|| AspectRatio <= 0.f)
	{
		return false;
	}

	const FVector CameraLocalPoint = CameraTransform.InverseTransformPositionNoScale(Point);
	if (CameraLocalPoint.X <= CameraNearEpsilon)
	{
		return false;
	}

	const float HorizontalTangent = FMath::Tan(FMath::DegreesToRadians(HorizontalFOVDegrees * 0.5f));
	const float VerticalTangent = HorizontalTangent / AspectRatio;
	if (!FMath::IsFinite(HorizontalTangent)
		|| !FMath::IsFinite(VerticalTangent)
		|| HorizontalTangent <= 0.f
		|| VerticalTangent <= 0.f)
	{
		return false;
	}

	const float NormalizedDeviceX = CameraLocalPoint.Y / (CameraLocalPoint.X * HorizontalTangent);
	const float NormalizedDeviceY = CameraLocalPoint.Z / (CameraLocalPoint.X * VerticalTangent);
	OutNormalizedPosition = FVector2D(
		0.5f + NormalizedDeviceX * 0.5f,
		0.5f - NormalizedDeviceY * 0.5f);
	return FMath::IsFinite(OutNormalizedPosition.X) && FMath::IsFinite(OutNormalizedPosition.Y);
}

bool FOfficeCameraFraming::DoesViewFit(
	const TConstArrayView<FVector> Points,
	const FTransform& CameraTransform,
	const float HorizontalFOVDegrees,
	const float AspectRatio,
	const FVector2D SafeMin,
	const FVector2D SafeMax)
{
	if (Points.IsEmpty() || !IsSafeFrameValid(SafeMin, SafeMax))
	{
		return false;
	}

	for (const FVector& Point : Points)
	{
		FVector2D NormalizedPosition = FVector2D::ZeroVector;
		if (!ProjectWorldPoint(
			Point,
			CameraTransform,
			HorizontalFOVDegrees,
			AspectRatio,
			NormalizedPosition)
			|| NormalizedPosition.X < SafeMin.X
			|| NormalizedPosition.X > SafeMax.X
			|| NormalizedPosition.Y < SafeMin.Y
			|| NormalizedPosition.Y > SafeMax.Y)
		{
			return false;
		}
	}

	return true;
}

FOfficeCameraFitResult FOfficeCameraFraming::FindClosestFit(
	const TFunctionRef<FOfficeCameraSample(float)> SampleProvider,
	const TConstArrayView<FVector> Points,
	const FVector2D SafeMin,
	const FVector2D SafeMax,
	const int32 CoarseSteps,
	const int32 RefineIterations)
{
	FOfficeCameraFitResult Result;
	if (CoarseSteps <= 0
		|| CoarseSteps > MaxCoarseSteps
		|| RefineIterations < 0
		|| RefineIterations > MaxRefineIterations
		|| Points.IsEmpty()
		|| !IsSafeFrameValid(SafeMin, SafeMax))
	{
		return Result;
	}

	auto EvaluateSample = [&](const float ZoomValue, FOfficeCameraSample& OutSample)
	{
		OutSample = SampleProvider(ZoomValue);
		OutSample.ZoomValue = ZoomValue;
		++Result.SampleCount;
		return DoesViewFit(
			Points,
			OutSample.CameraTransform,
			OutSample.HorizontalFOVDegrees,
			OutSample.AspectRatio,
			SafeMin,
			SafeMax);
	};

	FOfficeCameraSample FarthestSample;
	const int32 CoarseSampleCount = CoarseSteps + 1;
	for (int32 SampleIndex = 0; SampleIndex < CoarseSampleCount; ++SampleIndex)
	{
		const float ZoomValue = static_cast<float>(SampleIndex) / static_cast<float>(CoarseSteps);
		FOfficeCameraSample CoarseSample;
		const bool bCoarseFits = EvaluateSample(ZoomValue, CoarseSample);
		FarthestSample = CoarseSample;
		if (!bCoarseFits)
		{
			continue;
		}

		FOfficeCameraSample BestFitSample = CoarseSample;
		if (SampleIndex > 0)
		{
			float FailingZoom = static_cast<float>(SampleIndex - 1) / static_cast<float>(CoarseSteps);
			float FittingZoom = ZoomValue;
			for (int32 RefineIndex = 0; RefineIndex < RefineIterations; ++RefineIndex)
			{
				const float CandidateZoom = (FailingZoom + FittingZoom) * 0.5f;
				FOfficeCameraSample RefinedSample;
				if (EvaluateSample(CandidateZoom, RefinedSample))
				{
					FittingZoom = CandidateZoom;
					BestFitSample = RefinedSample;
				}
				else
				{
					FailingZoom = CandidateZoom;
				}
			}
		}

		Result.bFits = true;
		Result.ZoomValue = BestFitSample.ZoomValue;
		Result.ArmLength = BestFitSample.ArmLength;
		return Result;
	}

	Result.ZoomValue = 1.f;
	Result.ArmLength = FarthestSample.ArmLength;
	return Result;
}
