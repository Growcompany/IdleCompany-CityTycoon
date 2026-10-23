#include "Player/CameraViewBounds.h"

#include "SceneView.h"

bool CameraViewBounds::GetGroundFootprint(const FMinimalViewInfo& View, const FIntRect& ViewRect,
    EAspectRatioAxisConstraint AxisConstraint, const FTransform& BoundsTransform, FBox2D& OutFootprint)
{
    OutFootprint = FBox2D(ForceInit);
    if (ViewRect.Width() <= 0 || ViewRect.Height() <= 0 || View.Location.ContainsNaN()
        || View.Rotation.ContainsNaN() || View.ProjectionMode != ECameraProjectionMode::Perspective)
    {
        return false;
    }

    FMinimalViewInfo ProjectedView = View;
    FSceneViewProjectionData Projection;
    Projection.SetViewRectangle(ViewRect);
    Projection.ViewOrigin = View.Location;
    Projection.ViewRotationMatrix = FInverseRotationMatrix(View.Rotation) * FMatrix(
        FPlane(0, 0, 1, 0), FPlane(1, 0, 0, 0), FPlane(0, 1, 0, 0), FPlane(0, 0, 0, 1));
    FMinimalViewInfo::CalculateProjectionMatrixGivenViewRectangle(
        ProjectedView, AxisConstraint, ViewRect, Projection);
    const FMatrix Inverse = Projection.ComputeViewProjectionMatrix().Inverse();
    const FIntRect VisibleRect = Projection.GetConstrainedViewRect();

    // Including the camera also contains the ray segments above the ground plane.
    const FVector LocalCamera = BoundsTransform.InverseTransformPosition(View.Location);
    if (LocalCamera.Z <= 0.0) return false;
    OutFootprint += FVector2D(LocalCamera);
    for (const FVector2D Pixel : {
        FVector2D(VisibleRect.Min.X, VisibleRect.Min.Y), FVector2D(VisibleRect.Max.X, VisibleRect.Min.Y),
        FVector2D(VisibleRect.Min.X, VisibleRect.Max.Y), FVector2D(VisibleRect.Max.X, VisibleRect.Max.Y)})
    {
        FVector RayOrigin;
        FVector RayDirection;
        FSceneView::DeprojectScreenToWorld(Pixel, VisibleRect, Inverse, RayOrigin, RayDirection);
        const FVector LocalOrigin = BoundsTransform.InverseTransformPosition(RayOrigin);
        const FVector LocalDirection = BoundsTransform.InverseTransformVector(RayDirection);
        if (LocalDirection.ContainsNaN() || LocalDirection.Z >= -UE_SMALL_NUMBER) return false;
        const double Distance = -LocalOrigin.Z / LocalDirection.Z;
        const FVector Hit = LocalOrigin + LocalDirection * Distance;
        if (Distance < 0.0 || Hit.ContainsNaN()) return false;
        OutFootprint += FVector2D(Hit);
    }
    return true;
}

bool CameraViewBounds::GetTranslationBounds(const FBox2D& Bounds, const FBox2D& Footprint,
    FBox2D& OutTranslation)
{
    if (!Bounds.bIsValid || !Footprint.bIsValid) return false;
    // One local centimeter keeps floating-point rounding off the visible boundary.
    const FVector2D Minimum = Bounds.Min + FVector2D(1.0) - Footprint.Min;
    const FVector2D Maximum = Bounds.Max - FVector2D(1.0) - Footprint.Max;
    if (Minimum.X > Maximum.X || Minimum.Y > Maximum.Y) return false;
    OutTranslation = FBox2D(Minimum, Maximum);
    return true;
}
