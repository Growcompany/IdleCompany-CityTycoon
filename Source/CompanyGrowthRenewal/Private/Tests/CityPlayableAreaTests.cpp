#include "Misc/AutomationTest.h"
#include "Entity/Environment/CityPlayableArea.h"
#include "Player/PlayerCamera.h"
#include "Player/CameraViewBounds.h"
#include "Player/Components/MovementInputHandler.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Entity/Building/BuildingBaseActor.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "SceneView.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace CityPlayableAreaTests
{
    bool IsViewInside(const FMinimalViewInfo& CameraView, const FBox2D& Bounds, double GroundZ,
        const FIntRect& ViewRect = FIntRect(0, 0, 1920, 1080),
        EAspectRatioAxisConstraint AxisConstraint = AspectRatio_MaintainYFOV)
    {
        FMinimalViewInfo View = CameraView;
        FSceneViewProjectionData Projection;
        Projection.SetViewRectangle(ViewRect);
        Projection.ViewOrigin = View.Location;
        Projection.ViewRotationMatrix = FInverseRotationMatrix(View.Rotation) * FMatrix(
            FPlane(0, 0, 1, 0), FPlane(1, 0, 0, 0), FPlane(0, 1, 0, 0), FPlane(0, 0, 0, 1));
        FMinimalViewInfo::CalculateProjectionMatrixGivenViewRectangle(
            View, AxisConstraint, ViewRect, Projection);
        const FMatrix Inverse = Projection.ComputeViewProjectionMatrix().Inverse();
        const FIntRect VisibleRect = Projection.GetConstrainedViewRect();
        for (const FVector2D Pixel : {
            FVector2D(VisibleRect.Min.X, VisibleRect.Min.Y), FVector2D(VisibleRect.Max.X, VisibleRect.Min.Y),
            FVector2D(VisibleRect.Min.X, VisibleRect.Max.Y), FVector2D(VisibleRect.Max.X, VisibleRect.Max.Y)})
        {
            FVector RayOrigin;
            FVector RayDirection;
            FSceneView::DeprojectScreenToWorld(Pixel, VisibleRect, Inverse, RayOrigin, RayDirection);
            if (RayDirection.Z >= -UE_SMALL_NUMBER) return false;
            const FVector Hit = RayOrigin + RayDirection * ((GroundZ - RayOrigin.Z) / RayDirection.Z);
            if (!Bounds.ExpandBy(1.0).IsInside(FVector2D(Hit))) return false;
        }
        return true;
    }

    const FBox2D CityBounds(FVector2D(-80000, -114000), FVector2D(11000, 46000));

    bool ApplyCityBounds(ACityPlayableArea* PlayableArea, APlayerCamera* Camera)
    {
        PlayableArea->SetActorLocation(FVector(-34500, -34000, 0));
        PlayableArea->Bounds->SetBoxExtent(FVector(45500, 80000, 100));
        return PlayableArea->ApplyToCamera(Camera);
    }

    FMinimalViewInfo CurrentView(APlayerCamera* Camera, float DeltaTime = 1.f / 60.f)
    {
        Camera->SpringArm->TickComponent(DeltaTime, LEVELTICK_All, nullptr);
        FMinimalViewInfo View;
        Camera->CalcCamera(DeltaTime, View);
        return View;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityPlayableAreaProjectionBoundsTest,
    "CGR.Camera.CityPlayableArea.ProjectionTranslationBounds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCityPlayableAreaProjectionBoundsTest::RunTest(const FString& Parameters)
{
    FMinimalViewInfo View;
    View.ProjectionMode = ECameraProjectionMode::Perspective;
    View.Location = FVector(11000.f, 46000.f, 11200.f);
    View.Rotation = FRotator(-45.f, 45.f, 0.f);
    View.FOV = 52.f;
    View.AspectRatio = 16.f / 9.f;
    View.bConstrainAspectRatio = false;

    for (const FIntPoint Size : {FIntPoint(1440, 1080), FIntPoint(1920, 1080),
        FIntPoint(2400, 1080), FIntPoint(2520, 1080)})
    {
        const FIntRect ViewRect(0, 0, Size.X, Size.Y);
        for (const float LensOffset : {-0.6f, 0.f, 0.6f})
        {
            View.OffCenterProjectionOffset.X = LensOffset;
            FBox2D Footprint;
            FBox2D Translation;
            const FString Case = FString::Printf(TEXT("%dx%d offset %.1f"), Size.X, Size.Y, LensOffset);
            if (!TestTrue(Case + TEXT(" footprint"), CameraViewBounds::GetGroundFootprint(
                View, ViewRect, AspectRatio_MaintainYFOV, FTransform::Identity, Footprint))) continue;
            if (!TestTrue(Case + TEXT(" translation"), CameraViewBounds::GetTranslationBounds(
                CityPlayableAreaTests::CityBounds, Footprint, Translation))) continue;

            FMinimalViewInfo Corrected = View;
            Corrected.Location += FVector(
                FMath::Clamp(0.0, Translation.Min.X, Translation.Max.X),
                FMath::Clamp(0.0, Translation.Min.Y, Translation.Max.Y), 0.0);
            TestTrue(Case + TEXT(" independent projected corners inside"), CityPlayableAreaTests::IsViewInside(
                Corrected, CityPlayableAreaTests::CityBounds, 0.0, ViewRect, AspectRatio_MaintainYFOV));
        }
    }

    FBox2D Footprint;
    FBox2D Translation;
    TestTrue(TEXT("reference footprint exists"), CameraViewBounds::GetGroundFootprint(View,
        FIntRect(0, 0, 1920, 1080), AspectRatio_MaintainYFOV, FTransform::Identity, Footprint));
    TestFalse(TEXT("footprint wider than bounds has no translation"), CameraViewBounds::GetTranslationBounds(
        FBox2D(FVector2D(-100.f, -100.f), FVector2D(100.f, 100.f)), Footprint, Translation));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityPlayableAreaViewBoundaryTest,
    "CGR.Camera.CityPlayableArea.ZoomedViewStaysInside",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCityPlayableAreaViewBoundaryTest::RunTest(const FString& Parameters)
{
    UWorld* TestWorld = UWorld::CreateWorld(EWorldType::EditorPreview, false);
    if (!TestNotNull(TEXT("world"), TestWorld)) return false;
    ACityPlayableArea* PlayableArea = TestWorld->SpawnActor<ACityPlayableArea>();
    APlayerCamera* Camera = TestWorld->SpawnActor<APlayerCamera>();
    if (!TestNotNull(TEXT("area"), PlayableArea) || !TestNotNull(TEXT("camera"), Camera))
    {
        TestWorld->DestroyWorld(false);
        return false;
    }
    TestTrue(TEXT("city boundary applies"), CityPlayableAreaTests::ApplyCityBounds(PlayableArea, Camera));
    Camera->SetActorLocationAndRotation(FVector(11000, -34000, 11200), FRotator::ZeroRotator);
    Camera->MovementInputHandler->SetZoomValue(1.f);
    Camera->MovementInputHandler->ApplyZoomSettings();
    Camera->SpringArm->bEnableCameraLag = false;
    Camera->SpringArm->bEnableCameraRotationLag = false;
    const FMinimalViewInfo View = CityPlayableAreaTests::CurrentView(Camera);
    TestTrue(TEXT("zoomed-out screen corners cannot reveal outside the city boundary"),
        CityPlayableAreaTests::IsViewInside(View, CityPlayableAreaTests::CityBounds, 0.0));
    TestWorld->DestroyWorld(false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityPlayableAreaPoseSweepTest,
    "CGR.Camera.CityPlayableArea.ZoomRotationAspectSweep",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCityPlayableAreaPoseSweepTest::RunTest(const FString& Parameters)
{
    UWorld* TestWorld = UWorld::CreateWorld(EWorldType::EditorPreview, false);
    if (!TestNotNull(TEXT("world"), TestWorld)) return false;
    ACityPlayableArea* PlayableArea = TestWorld->SpawnActor<ACityPlayableArea>();
    APlayerCamera* Camera = TestWorld->SpawnActor<APlayerCamera>();
    if (!TestNotNull(TEXT("area"), PlayableArea) || !TestNotNull(TEXT("camera"), Camera))
    {
        TestWorld->DestroyWorld(false);
        return false;
    }
    TestTrue(TEXT("city boundary applies"), CityPlayableAreaTests::ApplyCityBounds(PlayableArea, Camera));
    Camera->SpringArm->bEnableCameraLag = false;
    Camera->SpringArm->bEnableCameraRotationLag = false;
    Camera->CameraComponent->bConstrainAspectRatio = true;

    for (const float Aspect : {4.f / 3.f, 16.f / 9.f, 20.f / 9.f, 21.f / 9.f})
    {
        Camera->CameraComponent->AspectRatio = Aspect;
        for (const float Yaw : {0.f, 45.f, 90.f, 180.f, 270.f})
        {
            Camera->SetActorLocationAndRotation(FVector(10500.f, 45000.f, 11200.f), FRotator(0.f, Yaw, 0.f));
            for (const float Zoom : {0.f, 1.f})
            {
                Camera->MovementInputHandler->SetZoomValue(Zoom);
                Camera->MovementInputHandler->ApplyZoomSettings();
                const FMinimalViewInfo View = CityPlayableAreaTests::CurrentView(Camera);
                TestTrue(FString::Printf(TEXT("view inside: aspect %.3f yaw %.0f zoom %.0f"), Aspect, Yaw, Zoom),
                    CityPlayableAreaTests::IsViewInside(View, CityPlayableAreaTests::CityBounds, 0.0));
            }
        }
    }

    Camera->CameraComponent->AspectRatio = 16.f / 9.f;
    Camera->SetActorLocationAndRotation(FVector(11000.f, -34000.f, 11200.f), FRotator::ZeroRotator);
    Camera->MovementInputHandler->SetZoomValue(0.f);
    Camera->MovementInputHandler->ApplyZoomSettings();
    const FVector BeforeZoomOut = Camera->GetActorLocation();
    Camera->MovementInputHandler->SetZoomValue(1.f);
    Camera->MovementInputHandler->ApplyZoomSettings();
    const FVector AfterZoomOut = Camera->GetActorLocation();
    TestTrue(TEXT("zoom out moves the pawn inward when the view would escape"),
        FVector::Dist2D(BeforeZoomOut, AfterZoomOut) > 1.f);
    TestTrue(TEXT("zoom out view inside"), CityPlayableAreaTests::IsViewInside(
        CityPlayableAreaTests::CurrentView(Camera), CityPlayableAreaTests::CityBounds, 0.0));
    Camera->MovementInputHandler->SetZoomValue(0.f);
    Camera->MovementInputHandler->ApplyZoomSettings();
    TestTrue(TEXT("zoom back preserves the current pawn position"),
        Camera->GetActorLocation().Equals(AfterZoomOut, 0.1f));
    TestWorld->DestroyWorld(false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityPlayableAreaFocusViewTest,
    "CGR.Camera.CityPlayableArea.FocusOutsideCompletesInside",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCityPlayableAreaFocusViewTest::RunTest(const FString& Parameters)
{
    UWorld* TestWorld = UWorld::CreateWorld(EWorldType::EditorPreview, false);
    if (!TestNotNull(TEXT("world"), TestWorld)) return false;
    ACityPlayableArea* PlayableArea = TestWorld->SpawnActor<ACityPlayableArea>();
    APlayerCamera* Camera = TestWorld->SpawnActor<APlayerCamera>();
    if (!TestNotNull(TEXT("area"), PlayableArea) || !TestNotNull(TEXT("camera"), Camera))
    {
        TestWorld->DestroyWorld(false);
        return false;
    }
    Camera->SetActorLocation(FVector(-34500.f, -34000.f, 11200.f));
    TestTrue(TEXT("city boundary applies"), CityPlayableAreaTests::ApplyCityBounds(PlayableArea, Camera));
    Camera->SpringArm->bEnableCameraLag = false;
    Camera->SpringArm->bEnableCameraRotationLag = false;
    Camera->MovementInputHandler->SetZoomValue(1.f);
    Camera->MovementInputHandler->ApplyZoomSettings();
    Camera->FocusOnLocation(FVector(90000.f, 90000.f, 11200.f));
    int32 FirstOutsideFrame = INDEX_NONE;
    for (int32 Frame = 0; Frame < 600 && Camera->IsTransitioningCamera(); ++Frame)
    {
        Camera->TickActor(1.f / 60.f, LEVELTICK_All, Camera->PrimaryActorTick);
        if (FirstOutsideFrame == INDEX_NONE && !CityPlayableAreaTests::IsViewInside(
            CityPlayableAreaTests::CurrentView(Camera), CityPlayableAreaTests::CityBounds, 0.0))
        {
            FirstOutsideFrame = Frame;
        }
    }
    TestTrue(FString::Printf(TEXT("focus view remains inside; first outside frame %d"), FirstOutsideFrame),
        FirstOutsideFrame == INDEX_NONE);
    TestFalse(TEXT("out-of-bounds focus completes"), Camera->IsTransitioningCamera());
    TestTrue(TEXT("finished focus view inside"), CityPlayableAreaTests::IsViewInside(
        CityPlayableAreaTests::CurrentView(Camera), CityPlayableAreaTests::CityBounds, 0.0));
    TestTrue(TEXT("finished focus stays inside the authored pawn bounds"),
        CityPlayableAreaTests::CityBounds.IsInside(FVector2D(Camera->GetActorLocation())));

    Camera->SetActorLocation(FVector(-34500.f, -34000.f, 11200.f));
    Camera->MovementInputHandler->SetZoomValue(1.f);
    Camera->MovementInputHandler->ApplyZoomSettings();
    Camera->FocusOnLocation(FVector(90000.f, 90000.f, 11200.f), Camera->MovementInputHandler->MinZoomDistance);
    for (int32 Frame = 0; Frame < 600 && Camera->IsTransitioningCamera(); ++Frame)
    {
        Camera->TickActor(1.f / 60.f, LEVELTICK_All, Camera->PrimaryActorTick);
    }
    TestFalse(TEXT("out-of-bounds focus with minimum arm completes"), Camera->IsTransitioningCamera());
    TestTrue(TEXT("minimum requested arm remains selected"), FMath::IsNearlyEqual(
        Camera->SpringArm->TargetArmLength, Camera->MovementInputHandler->MinZoomDistance, 2.f));
    TestTrue(TEXT("minimum-arm focus finishes with view inside"), CityPlayableAreaTests::IsViewInside(
        CityPlayableAreaTests::CurrentView(Camera), CityPlayableAreaTests::CityBounds, 0.0));

    Camera->SetActorLocation(FVector(-34500.f, -34000.f, 11200.f));
    Camera->MovementInputHandler->SetZoomValue(0.f);
    Camera->MovementInputHandler->ApplyZoomSettings();
    Camera->FocusOnLocation(FVector(90000.f, 90000.f, 11200.f), Camera->MovementInputHandler->MaxZoomDistance);
    for (int32 Frame = 0; Frame < 600 && Camera->IsTransitioningCamera(); ++Frame)
    {
        Camera->TickActor(1.f / 60.f, LEVELTICK_All, Camera->PrimaryActorTick);
    }
    TestFalse(TEXT("out-of-bounds focus with maximum arm completes from minimum zoom"), Camera->IsTransitioningCamera());
    TestTrue(TEXT("maximum requested arm remains selected"), FMath::IsNearlyEqual(
        Camera->SpringArm->TargetArmLength, Camera->MovementInputHandler->MaxZoomDistance, 2.f));
    TestTrue(TEXT("maximum-arm focus finishes with view inside"), CityPlayableAreaTests::IsViewInside(
        CityPlayableAreaTests::CurrentView(Camera), CityPlayableAreaTests::CityBounds, 0.0));
    TestWorld->DestroyWorld(false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityPlayableAreaSnapFocusTest,
    "CGR.Camera.CityPlayableArea.SnapFocusRejectsUnsafeAndIgnoresStartHeight",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCityPlayableAreaSnapFocusTest::RunTest(const FString& Parameters)
{
    UWorld* TestWorld = UWorld::CreateWorld(EWorldType::EditorPreview, false);
    if (!TestNotNull(TEXT("world"), TestWorld)) return false;
    ACityPlayableArea* PlayableArea = TestWorld->SpawnActor<ACityPlayableArea>();
    APlayerCamera* Camera = TestWorld->SpawnActor<APlayerCamera>();
    ABuildingBaseActor* Building = TestWorld->SpawnActor<ABuildingBaseActor>();
    APlayerController* Controller = TestWorld->SpawnActor<APlayerController>();
    if (!TestNotNull(TEXT("area"), PlayableArea) || !TestNotNull(TEXT("camera"), Camera)
        || !TestNotNull(TEXT("building"), Building) || !TestNotNull(TEXT("controller"), Controller))
    {
        TestWorld->DestroyWorld(false);
        return false;
    }
    // EditorPreview does not run the controller's normal PostInitializeComponents registration.
    TestWorld->AddController(Controller);
    const FObjectPropertyBase* MainMeshProperty = FindFProperty<FObjectPropertyBase>(
        ABuildingBaseActor::StaticClass(), TEXT("MainMeshComponent"));
    UStaticMeshComponent* MainMesh = MainMeshProperty
        ? Cast<UStaticMeshComponent>(MainMeshProperty->GetObjectPropertyValue_InContainer(Building)) : nullptr;
    UStaticMesh* Cube = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube"))).LoadSynchronous();
    if (!TestNotNull(TEXT("main mesh"), MainMesh) || !TestNotNull(TEXT("cube mesh"), Cube))
    {
        TestWorld->DestroyWorld(false);
        return false;
    }
    MainMesh->SetStaticMesh(Cube);
    MainMesh->SetRelativeScale3D(FVector(10.f, 10.f, 250.f));
    MainMesh->SetRelativeLocation(FVector(0.f, 0.f, 12500.f));
    Building->SetActorLocation(FVector(-34500.f, -34000.f, 0.f));
    Camera->SetActorLocationAndRotation(FVector(-34500.f, -34000.f, 11200.f), FRotator(0.f, 90.f, 0.f));
    TestTrue(TEXT("city boundary applies"), CityPlayableAreaTests::ApplyCityBounds(PlayableArea, Camera));
    Camera->MovementInputHandler->SetZoomValue(0.f);
    Camera->MovementInputHandler->ApplyZoomSettings();
    const FVector BeforeReject = Camera->GetActorLocation();
    const float ZoomBeforeReject = Camera->MovementInputHandler->GetZoomValue();
    Building->SetActorLocation(FVector(-34500.f, -34000.f, 10000000.f));
    TestFalse(TEXT("impossible high building snap is rejected"), Camera->SnapFocusOnBuilding(Building));
    TestTrue(TEXT("rejected snap preserves pawn"), Camera->GetActorLocation().Equals(BeforeReject, 0.1f));
    TestTrue(TEXT("rejected snap preserves zoom"), FMath::IsNearlyEqual(
        Camera->MovementInputHandler->GetZoomValue(), ZoomBeforeReject, 0.0001f));

    Building->SetActorLocation(FVector(-34500.f, -34000.f, 0.f));
    if (TestTrue(TEXT("safe start snaps to building"), Camera->SnapFocusOnBuilding(Building)))
    {
        const FVector ExpectedLocation = Camera->GetActorLocation();
        const float ExpectedZoom = Camera->MovementInputHandler->GetZoomValue();
        const float ExpectedArm = Camera->SpringArm->TargetArmLength;
        TestTrue(TEXT("tall building requests the maximum arm"), FMath::IsNearlyEqual(
            ExpectedArm, Camera->MovementInputHandler->MaxZoomDistance, 2.f));
        Camera->SetActorLocation(FVector(-34500.f, -34000.f, 30000.f));
        Camera->MovementInputHandler->SetZoomValue(1.f);
        Camera->MovementInputHandler->ApplyZoomSettings();
        TestTrue(TEXT("high starting height constrains zoom before the snap"),
            Camera->MovementInputHandler->GetZoomValue() > 0.f && Camera->MovementInputHandler->GetZoomValue() < 0.999f);
        TestTrue(TEXT("high starting pawn snaps to the same safe building"), Camera->SnapFocusOnBuilding(Building));
        TestTrue(TEXT("snap destination is independent of starting height"),
            Camera->GetActorLocation().Equals(ExpectedLocation, 0.1f));
        TestTrue(TEXT("snap zoom is independent of starting height"), FMath::IsNearlyEqual(
            Camera->MovementInputHandler->GetZoomValue(), ExpectedZoom, 0.001f));
        TestTrue(TEXT("snap arm is independent of starting height"), FMath::IsNearlyEqual(
            Camera->SpringArm->TargetArmLength, ExpectedArm, 2.f));
    }
    TestWorld->DestroyWorld(false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityPlayableAreaLagViewTest,
    "CGR.Camera.CityPlayableArea.LagAndRepeatedCalcCamera",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCityPlayableAreaLagViewTest::RunTest(const FString& Parameters)
{
    UWorld* TestWorld = UWorld::CreateWorld(EWorldType::EditorPreview, false);
    if (!TestNotNull(TEXT("world"), TestWorld)) return false;
    ACityPlayableArea* PlayableArea = TestWorld->SpawnActor<ACityPlayableArea>();
    APlayerCamera* Camera = TestWorld->SpawnActor<APlayerCamera>();
    if (!TestNotNull(TEXT("area"), PlayableArea) || !TestNotNull(TEXT("camera"), Camera))
    {
        TestWorld->DestroyWorld(false);
        return false;
    }
    Camera->SetActorLocation(FVector(-34500.f, -34000.f, 11200.f));
    TestTrue(TEXT("city boundary applies"), CityPlayableAreaTests::ApplyCityBounds(PlayableArea, Camera));
    Camera->MovementInputHandler->SetZoomValue(1.f);
    Camera->MovementInputHandler->ApplyZoomSettings();
    Camera->SpringArm->bEnableCameraLag = false;
    Camera->SpringArm->bEnableCameraRotationLag = false;
    CityPlayableAreaTests::CurrentView(Camera);
    Camera->SpringArm->bEnableCameraLag = true;
    Camera->SpringArm->bEnableCameraRotationLag = true;
    Camera->SetActorLocationAndRotation(FVector(11000.f, 46000.f, 11200.f), FRotator(0.f, 90.f, 0.f));
    Camera->MovementInputHandler->ApplyMovementBoundary();
    int32 FirstOutsideFrame = INDEX_NONE;
    for (int32 Frame = 0; Frame < 120; ++Frame)
    {
        const FMinimalViewInfo View = CityPlayableAreaTests::CurrentView(Camera);
        if (FirstOutsideFrame == INDEX_NONE && !CityPlayableAreaTests::IsViewInside(
            View, CityPlayableAreaTests::CityBounds, 0.0))
        {
            FirstOutsideFrame = Frame;
        }
    }
    TestTrue(FString::Printf(TEXT("lag view remains inside; first outside frame %d"), FirstOutsideFrame),
        FirstOutsideFrame == INDEX_NONE);
    const FVector PawnBefore = Camera->GetActorLocation();
    const FMinimalViewInfo First = CityPlayableAreaTests::CurrentView(Camera, 0.f);
    for (int32 Repeat = 0; Repeat < 4; ++Repeat)
    {
        FMinimalViewInfo Repeated;
        Camera->CalcCamera(0.f, Repeated);
        TestTrue(TEXT("repeated CalcCamera does not accumulate view translation"),
            Repeated.Location.Equals(First.Location, 0.1f));
        TestTrue(TEXT("repeated CalcCamera does not move pawn"), Camera->GetActorLocation().Equals(PawnBefore, 0.1f));
    }
    TestWorld->DestroyWorld(false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityPlayableAreaCameraBoundaryTest,
    "CGR.Camera.CityPlayableArea.RotatedBoundaryAndFocus",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCityPlayableAreaCameraBoundaryTest::RunTest(const FString& Parameters)
{
    UWorld* TestWorld = UWorld::CreateWorld(EWorldType::EditorPreview, false);
    if (!TestNotNull(TEXT("world"), TestWorld)) return false;

    ACityPlayableArea* PlayableArea = TestWorld->SpawnActor<ACityPlayableArea>();
    APlayerCamera* Camera = TestWorld->SpawnActor<APlayerCamera>();
    if (!TestNotNull(TEXT("playable area"), PlayableArea) || !TestNotNull(TEXT("camera"), Camera))
    {
        TestWorld->DestroyWorld(false);
        return false;
    }

    PlayableArea->Bounds->SetBoxExtent(FVector(1000.f, 500.f, 100.f));
    PlayableArea->SetActorLocationAndRotation(FVector(4000.f, -3000.f, 0.f), FRotator(0.f, 90.f, 0.f));
    TestTrue(TEXT("authored area applies to camera"), PlayableArea->ApplyToCamera(Camera));
    TestTrue(TEXT("point boundary remains available"), Camera->MovementInputHandler->SetPlanarMovementBounds(
        FBox2D(FVector2D(-1000.f, -500.f), FVector2D(1000.f, 500.f)), PlayableArea->Bounds->GetComponentTransform()));

    const FVector Outside(4000.f, 2000.f, 700.f);
    const FVector ExpectedBoundary(4000.f, -2000.f, 700.f);
    TestTrue(TEXT("rotated box clamps XY and preserves camera height"),
        Camera->MovementInputHandler->ClampToMovementBoundary(Outside).Equals(ExpectedBoundary, 0.01f));

    Camera->SetActorLocation(FVector(4000.f, -3000.f, 700.f));
    Camera->FocusOnLocation(Outside);
    for (int32 Frame = 0; Frame < 600 && Camera->IsTransitioningCamera(); ++Frame)
    {
        Camera->TickActor(1.f / 60.f, LEVELTICK_All, Camera->PrimaryActorTick);
    }
    TestFalse(TEXT("out-of-bounds focus completes"), Camera->IsTransitioningCamera());
    TestTrue(TEXT("focus finishes on playable side of rotated boundary"),
        Camera->GetActorLocation().Equals(ExpectedBoundary, 0.1f));

    TestWorld->DestroyWorld(false);
    return true;
}
#endif
