#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraTypes.h"

namespace CityOpeningCamera
{
    constexpr float Duration = 6.f;
    constexpr float FactoryCut = 4.f;

    inline int32 Section(float Seconds)
    {
        return Seconds < FactoryCut ? 0 : 1;
    }

    inline float SmoothAlpha(float Alpha)
    {
        const float A = FMath::Clamp(Alpha, 0.f, 1.f);
        return A * A * (3.f - 2.f * A);
    }

    // 컷 뒤 잠깐 완전한 암전을 유지해 10Hz 도시 조명이 바뀐 뒤 화면을 연다.
    inline float FadeAmount(float Seconds)
    {
        const float Fade = 1.f - SmoothAlpha(Seconds / .18f);
        const float Out = SmoothAlpha((Seconds - (FactoryCut - .20f)) / .18f);
        const float In = 1.f - SmoothAlpha((Seconds - (FactoryCut + .12f)) / .20f);
        return FMath::Max(Fade, FMath::Min(Out, In));
    }

    inline FMinimalViewInfo MountedView(const FMinimalViewInfo& Template, const FTransform& Travel,
        const FVector& CameraOffset, const FVector& LookAtOffset, float FOV)
    {
        FMinimalViewInfo View = Template;
        View.Location = Travel.TransformPositionNoScale(CameraOffset);
        View.Rotation = (Travel.TransformPositionNoScale(LookAtOffset) - View.Location).Rotation();
        View.FOV = FOV;
        View.OffCenterProjectionOffset = FVector2D::ZeroVector;
        return View;
    }
}
