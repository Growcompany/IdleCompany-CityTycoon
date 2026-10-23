#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Entity/Factory/BrickFactory.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Player/Components/MovementInputHandler.h"
#include "Player/PlayerCamera.h"
#include "SceneView.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace FactoryCameraGroundFocusTests
{
bool ProjectGroundCenter(APlayerCamera* Camera, const FVector& GroundCenter, FVector2D& OutScreenRatio)
{
    FMinimalViewInfo View;
    Camera->CalcCamera(0.f, View);
    const FIntRect ViewRect(0, 0, 1920, 1080);
    FSceneViewProjectionData Projection;
    Projection.SetViewRectangle(ViewRect);
    Projection.ViewOrigin = View.Location;
    Projection.ViewRotationMatrix = FInverseRotationMatrix(View.Rotation) * FMatrix(
        FPlane(0, 0, 1, 0), FPlane(1, 0, 0, 0),
        FPlane(0, 1, 0, 0), FPlane(0, 0, 0, 1));
    FMinimalViewInfo::CalculateProjectionMatrixGivenViewRectangle(
        View, AspectRatio_MaintainYFOV, ViewRect, Projection);
    FVector2D ScreenPosition = FVector2D::ZeroVector;
    if (!FSceneView::ProjectWorldToScreen(GroundCenter, Projection.GetConstrainedViewRect(),
        Projection.ComputeViewProjectionMatrix(), ScreenPosition, true)) return false;
    OutScreenRatio = FVector2D(ScreenPosition.X / ViewRect.Width(), ScreenPosition.Y / ViewRect.Height());
    return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGRFactoryCameraGroundFocusTest,
    "CGR.Camera.Factory.GroundCenterAcrossStagesAndRotations",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCGRFactoryCameraGroundFocusTest::RunTest(const FString& Parameters)
{
    if (!FPaths::ProjectSavedDir().Contains(TEXT("Saved_FactoryEmployeeVerify")))
    {
        AddWarning(TEXT("Skipped: use -saveddirsuffix=FactoryEmployeeVerify to isolate fixture shutdown saves"));
        return true;
    }
    if (!TestNotNull(TEXT("engine"), GEngine)) return false;
    UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
    Instance->InitializeStandalone();
    UWorld* TestWorld = Instance->GetWorld();
    if (!TestNotNull(TEXT("world"), TestWorld)) return false;
    TestWorld->InitializeActorsForPlay(FURL());
    const auto Cleanup = [Instance, TestWorld]()
    {
        Instance->Shutdown();
        TestWorld->SetGameInstance(nullptr);
        if (GEngine) GEngine->DestroyWorldContext(TestWorld);
        TestWorld->DestroyWorld(false);
    };

    UClass* FactoryClass = LoadClass<ABrickFactory>(nullptr,
        TEXT("/Game/CompanyGrowth/Blueprint/Factory/BP_BrickFactory.BP_BrickFactory_C"));
    APlayerController* Controller = TestWorld->SpawnActor<APlayerController>();
    APlayerCamera* Camera = TestWorld->SpawnActor<APlayerCamera>();
    ABrickFactory* Factory = FactoryClass ? TestWorld->SpawnActor<ABrickFactory>(FactoryClass) : nullptr;
    if (!TestNotNull(TEXT("factory Blueprint"), FactoryClass)
        || !TestNotNull(TEXT("controller"), Controller)
        || !TestNotNull(TEXT("camera"), Camera)
        || !TestNotNull(TEXT("factory"), Factory))
    {
        Cleanup();
        return false;
    }
    UStaticMeshComponent* Body = Cast<UStaticMeshComponent>(Factory->GetMainMeshComponent());
    if (!TestNotNull(TEXT("factory body"), Body))
    {
        Cleanup();
        return false;
    }
    Camera->SpringArm->bEnableCameraLag = false;
    Camera->SpringArm->bEnableCameraRotationLag = false;

    float InitialDistance = 0.f;
    float PreviousDistance = 0.f;
    for (int32 Stage = 1; Stage <= 8; ++Stage)
    {
        FFactorySaveData FactoryData = Factory->GetFactoryData();
        FactoryData.UpgradeData.FindOrAdd(EFactoryUpgradeType::FactoryExpansion).Level = Stage;
        Factory->SetFactoryData(FactoryData);
        const float FocusDistance = Factory->GetCameraFocusDistanceCm();
        if (!TestTrue(TEXT("Displayed factory has a valid focus distance"), FocusDistance > 0.f)) continue;
        UStaticMesh* StageMesh = Body->GetStaticMesh();
        if (!TestNotNull(FString::Printf(TEXT("stage %d mesh"), Stage), StageMesh)) continue;
        Body->UpdateBounds();
        const FBox LocalBounds = StageMesh->GetBoundingBox();
        const FVector LocalGroundCenter(LocalBounds.GetCenter().X, LocalBounds.GetCenter().Y, LocalBounds.Min.Z);
        for (const float Yaw : {0.f, 90.f, 180.f, 270.f})
        {
            Factory->SetActorLocationAndRotation(FVector(5000.f, -12000.f, 10900.f), FRotator(0.f, Yaw, 0.f));
            const FVector GroundCenter = Body->GetComponentTransform().TransformPosition(LocalGroundCenter);
            Camera->SetActorLocationAndRotation(FVector::ZeroVector, FRotator(0.f, Yaw, 0.f));
            for (const float ScreenRatioX : {0.5f, 0.35f})
            {
                const FString Case = FString::Printf(TEXT("stage %d yaw %.0f screen X %.2f"),
                    Stage, Yaw, ScreenRatioX);
                if (ScreenRatioX == 0.5f)
                {
                    if (!TestTrue(Case + TEXT(" snap succeeds"),
                        Camera->SnapFocusOnActor(Factory, ScreenRatioX, FocusDistance))) continue;
                }
                else
                {
                    Camera->FocusOnActor(Factory, ScreenRatioX, false, FocusDistance);
                    for (int32 Frame = 0; Frame < 300 && Camera->IsTransitioningCamera(); ++Frame)
                    {
                        Camera->Tick(1.f / 60.f);
                    }
                    if (!TestFalse(Case + TEXT(" glide finishes"), Camera->IsTransitioningCamera())) continue;
                }

                FVector2D Projected;
                if (TestTrue(Case + TEXT(" ground is visible"),
                    FactoryCameraGroundFocusTests::ProjectGroundCenter(Camera, GroundCenter, Projected)))
                {
                    TestEqual(Case + TEXT(" current outline ground is vertically centered"),
                        Projected.Y, 0.5, 0.005);
                    TestEqual(Case + TEXT(" horizontal composition is preserved"),
                        Projected.X, static_cast<double>(ScreenRatioX), 0.005);
                }
                const FZoomRigParams Rig = Camera->MovementInputHandler->GetZoomRigParams();
                TestEqual(Case + TEXT(" camera distance includes socket pullback"),
                    Camera->SpringArm->TargetArmLength + Rig.CameraPullBackCm,
                    FocusDistance, 2.f);
            }
        }
        const float StageDistance = Camera->SpringArm->TargetArmLength
            + Camera->MovementInputHandler->GetZoomRigParams().CameraPullBackCm;
        if (Stage == 1) InitialDistance = StageDistance;
        TestTrue(TEXT("Larger factory stages do not bring the focus closer"), StageDistance >= PreviousDistance - 2.f);
        PreviousDistance = StageDistance;
    }
    TestTrue(TEXT("The starter is framed meaningfully closer than the final factory"),
        InitialDistance < PreviousDistance * 0.85f);
    Cleanup();
    return true;
}
#endif
