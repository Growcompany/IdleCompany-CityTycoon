#include "Entity/Environment/CityPlayableArea.h"

#include "Components/BoxComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Player/CityCameraBoundaryModifier.h"
#include "Player/PlayerCamera.h"
#include "Player/Components/MovementInputHandler.h"

ACityPlayableArea::ACityPlayableArea()
{
    PrimaryActorTick.bCanEverTick = false;
#if WITH_EDITORONLY_DATA
    bIsSpatiallyLoaded = false;
#endif

    Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("Bounds"));
    SetRootComponent(Bounds);
    Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Bounds->SetGenerateOverlapEvents(false);
    Bounds->SetCanEverAffectNavigation(false);
}

bool ACityPlayableArea::ApplyToCamera(APlayerCamera* Camera) const
{
    if (!IsValid(Camera) || !Camera->MovementInputHandler || !Bounds)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CityPlayableArea] Cannot apply bounds: camera or component is missing."));
        return false;
    }

    const FVector Extent = Bounds->GetUnscaledBoxExtent();
    if (!FMath::IsFinite(Extent.X) || !FMath::IsFinite(Extent.Y)
        || Extent.X <= 0.f || Extent.Y <= 0.f)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CityPlayableArea] Cannot apply invalid box extent on %s."), *GetName());
        return false;
    }

    const FBox2D LocalBounds(FVector2D(-Extent.X, -Extent.Y), FVector2D(Extent.X, Extent.Y));
    if (!Camera->MovementInputHandler->SetPlanarMovementBounds(LocalBounds, Bounds->GetComponentTransform(), true))
    {
        UE_LOG(LogTemp, Warning, TEXT("[CityPlayableArea] Cannot apply box transform on %s."), *GetName());
        return false;
    }
    Camera->MovementInputHandler->ApplyMovementBoundary();
    if (const APlayerController* ViewController = Cast<APlayerController>(Camera->GetController()))
    {
        APlayerCameraManager* CameraManager = ViewController->PlayerCameraManager;
        if (CameraManager && !CameraManager->FindCameraModifierByClass(UCityCameraBoundaryModifier::StaticClass()))
            CameraManager->AddNewCameraModifier(UCityCameraBoundaryModifier::StaticClass());
    }
    return true;
}
