#include "Player/Components/FocusOcclusionMath.h"

namespace FocusOcclusionMath
{
	void ComputeSamplePoints(const FBox& TargetBounds, const FVector& CameraRight, float Inset, TArray<FVector>& OutPoints)
	{
		OutPoints.Reset();
		if (!TargetBounds.IsValid)
		{
			return;
		}

		const FVector Center = TargetBounds.GetCenter();
		const FVector Extent = TargetBounds.GetExtent();
		const FVector Right = CameraRight.GetSafeNormal();
		const float ClampedInset = FMath::Clamp(Inset, 0.f, 1.f);

		// 가로는 짧은 축을 쓴다 — 긴 축 기준이면 비정사각 풋프린트에서 샘플이 바운드 밖으로 나간다
		const float HorizontalHalf = FMath::Min(Extent.X, Extent.Y) * ClampedInset;
		const float VerticalHalf = Extent.Z * ClampedInset;

		OutPoints.Add(Center);
		OutPoints.Add(Center + FVector::UpVector * VerticalHalf);
		OutPoints.Add(Center - FVector::UpVector * VerticalHalf);
		OutPoints.Add(Center + Right * HorizontalHalf);
		OutPoints.Add(Center - Right * HorizontalHalf);
	}

	float StepGhostAlpha(float Current, float Goal, float DeltaTime, float TransitionTime)
	{
		if (TransitionTime <= 0.f)
		{
			return Goal;
		}
		if (DeltaTime <= 0.f)
		{
			return Current;
		}
		// 등속 보간 — FInterpTo(지수)는 목표에 영원히 도달하지 않아 페이드 완료 판정이 안 선다
		return FMath::FInterpConstantTo(Current, Goal, DeltaTime, 1.f / TransitionTime);
	}
}
