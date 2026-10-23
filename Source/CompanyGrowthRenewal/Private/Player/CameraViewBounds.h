#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraTypes.h"

namespace CameraViewBounds
{
    bool GetGroundFootprint(const FMinimalViewInfo& View, const FIntRect& ViewRect,
        EAspectRatioAxisConstraint AxisConstraint, const FTransform& BoundsTransform, FBox2D& OutFootprint);

    bool GetTranslationBounds(const FBox2D& Bounds, const FBox2D& Footprint, FBox2D& OutTranslation);
}
