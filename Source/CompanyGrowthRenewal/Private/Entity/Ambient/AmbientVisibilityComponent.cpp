#include "Entity/Ambient/AmbientVisibilityComponent.h"

#include "Components/PrimitiveComponent.h"
#include "ConvexVolume.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "SceneView.h"
#include "TimerManager.h"

UAmbientVisibilityComponent::UAmbientVisibilityComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UAmbientVisibilityComponent::StartMonitoring(UPrimitiveComponent* Target)
{
	StopMonitoring();
	ObservedMesh = Target;
	if (!IsValid(Target) || !GetWorld())
	{
		return;
	}
	UpdateVisibility();
	GetWorld()->GetTimerManager().SetTimer(VisibilityTimer, this,
		&UAmbientVisibilityComponent::UpdateVisibility, 0.25f, true);
}

void UAmbientVisibilityComponent::StopMonitoring()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(VisibilityTimer);
	}
	ObservedMesh.Reset();
	bMotionActive = false;
	if (AActor* OwningActor = GetOwner())
	{
		OwningActor->SetActorTickEnabled(false);
	}
}

void UAmbientVisibilityComponent::UpdateVisibility()
{
	const UPrimitiveComponent* Target = ObservedMesh.Get();
	const APlayerController* Controller = UGameplayStatics::GetPlayerController(this, 0);
	const ULocalPlayer* LocalPlayer = Controller && Controller->IsLocalController()
		? Controller->GetLocalPlayer() : nullptr;
	bool bShouldAnimate = false;
	if (Target && Target->IsVisible() && !Target->bHiddenInGame
		&& LocalPlayer && LocalPlayer->ViewportClient && LocalPlayer->ViewportClient->Viewport)
	{
		FSceneViewProjectionData Projection;
		if (LocalPlayer->GetProjectionData(LocalPlayer->ViewportClient->Viewport, Projection))
		{
			const FBoxSphereBounds& MeshBounds = Target->Bounds;
			const float Radius = bMotionActive
				? FMath::Max(ActivationRadiusCm, DeactivationRadiusCm) : ActivationRadiusCm;
			const double SurfaceDistance = FMath::Max(0.0,
				FVector::Distance(Projection.ViewOrigin, MeshBounds.Origin) - MeshBounds.SphereRadius);
			if (SurfaceDistance <= Radius)
			{
				FConvexVolume ViewFrustum;
				GetViewFrustumBounds(ViewFrustum, Projection.ComputeViewProjectionMatrix(), true);
				bShouldAnimate = ViewFrustum.IntersectBox(MeshBounds.Origin, MeshBounds.BoxExtent);
			}
		}
	}
	bMotionActive = bShouldAnimate;
	if (AActor* OwningActor = GetOwner())
	{
		OwningActor->SetActorTickEnabled(bMotionActive);
	}
}

void UAmbientVisibilityComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopMonitoring();
	Super::EndPlay(EndPlayReason);
}
