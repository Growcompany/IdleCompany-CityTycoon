#pragma once

#include "CoreMinimal.h"

/** 코치마크 구멍의 위치·크기를 현재 표시값에서 새 대상으로 감속 이동시킨다. */
struct FGuideHoleTransition
{
	void Retarget() { bRetargetPending = bInitialized; }
	void Reset() { *this = FGuideHoleTransition(); }
	bool IsSettled() const { return !bRetargetPending && Remaining <= 0.f; }

	void Advance(FVector2D& InOutCenter, FVector2D& InOutHalf, float DeltaSeconds)
	{
		if (bRetargetPending && bInitialized)
		{
			FromCenter = DrawnCenter;
			FromHalf = DrawnHalf;
			Remaining = Duration;
		}
		bRetargetPending = false;
		bInitialized = true;
		if (Remaining > 0.f)
		{
			Remaining = FMath::Max(Remaining - FMath::Max(DeltaSeconds, 0.f), 0.f);
			const float Alpha = FMath::Clamp(1.f - FMath::Pow(Remaining / Duration, 3.f), 0.f, 1.f);
			DrawnCenter = FMath::Lerp(FromCenter, InOutCenter, Alpha);
			DrawnHalf = FMath::Lerp(FromHalf, InOutHalf, Alpha);
		}
		else
		{
			DrawnCenter = InOutCenter;
			DrawnHalf = InOutHalf;
		}
		InOutCenter = DrawnCenter;
		InOutHalf = DrawnHalf;
	}

private:
	static constexpr float Duration = 0.24f;
	float Remaining = 0.f;
	bool bRetargetPending = false;
	bool bInitialized = false;
	FVector2D FromCenter = FVector2D::ZeroVector;
	FVector2D FromHalf = FVector2D::ZeroVector;
	FVector2D DrawnCenter = FVector2D::ZeroVector;
	FVector2D DrawnHalf = FVector2D::ZeroVector;
};
