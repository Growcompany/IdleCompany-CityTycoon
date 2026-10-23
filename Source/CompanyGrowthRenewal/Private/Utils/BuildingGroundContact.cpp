#include "Utils/BuildingGroundContact.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"

namespace
{
	const FName ContactComponentName(TEXT("GroundContactSMC"));
	const FName BaseComponentName(TEXT("Base_Module"));

	UStaticMesh* GetPlaneMesh()
	{
		static const TSoftObjectPtr<UStaticMesh> PlanePath(
			FSoftObjectPath(TEXT("/Engine/BasicShapes/Plane.Plane")));
		return PlanePath.LoadSynchronous();
	}

	UMaterialInterface* GetContactMaterial()
	{
		static const TSoftObjectPtr<UMaterialInterface> MaterialPath(
			FSoftObjectPath(TEXT("/Game/ModernBuildings/Materials/M_MB_GroundContact.M_MB_GroundContact")));
		return MaterialPath.LoadSynchronous();
	}
}

bool BuildingGroundContact::Configure(AActor* Owner, UStaticMeshComponent* BaseMesh, UStaticMeshComponent* ContactMesh)
{
	if (!Owner || !BaseMesh || !BaseMesh->GetStaticMesh() || !ContactMesh
		|| !Owner->GetWorld() || Owner->GetWorld()->GetFeatureLevel() != ERHIFeatureLevel::ES3_1)
	{
		if (ContactMesh) { ContactMesh->SetVisibility(false); }
		return false;
	}

	UStaticMesh* PlaneMesh = GetPlaneMesh();
	UMaterialInterface* ContactMaterial = GetContactMaterial();
	if (!PlaneMesh || !ContactMaterial)
	{
		ContactMesh->SetVisibility(false);
		return false;
	}

	FVector MeshMin;
	FVector MeshMax;
	BaseMesh->GetLocalBounds(MeshMin, MeshMax);
	if (MeshMin.ContainsNaN() || MeshMax.ContainsNaN())
	{
		ContactMesh->SetVisibility(false);
		return false;
	}

	const FTransform ActorTransform = Owner->GetActorTransform();
	const FTransform BaseToActor = BaseMesh->GetComponentTransform().GetRelativeTransform(ActorTransform);
	FBox BaseInActor(ForceInit);
	for (double X : {MeshMin.X, MeshMax.X})
	{
		for (double Y : {MeshMin.Y, MeshMax.Y})
		{
			for (double Z : {MeshMin.Z, MeshMax.Z})
			{
				BaseInActor += BaseToActor.TransformPosition(FVector(X, Y, Z));
			}
		}
	}
	if (!BaseInActor.IsValid)
	{
		ContactMesh->SetVisibility(false);
		return false;
	}

	const FVector ActorScale = ActorTransform.GetScale3D().GetAbs();
	const FVector BaseHalf = BaseInActor.GetExtent();
	if (BaseHalf.X <= 1.f || BaseHalf.Y <= 1.f || ActorScale.X <= UE_SMALL_NUMBER
		|| ActorScale.Y <= UE_SMALL_NUMBER || ActorScale.Z <= UE_SMALL_NUMBER)
	{
		ContactMesh->SetVisibility(false);
		return false;
	}

	// 최대 100cm, 바닥 축 길이의 25%까지만 바깥쪽으로 번지게 한다.
	const float HaloX = FMath::Min(100.f / ActorScale.X, BaseHalf.X * 0.5f);
	const float HaloY = FMath::Min(100.f / ActorScale.Y, BaseHalf.Y * 0.5f);
	const float PlaneHalfX = BaseHalf.X + HaloX;
	const float PlaneHalfY = BaseHalf.Y + HaloY;
	const FVector ActorLocalCenter(BaseInActor.GetCenter().X, BaseInActor.GetCenter().Y,
		BaseInActor.Min.Z + 2.f / ActorScale.Z);

	ContactMesh->SetStaticMesh(PlaneMesh);
	ContactMesh->SetMaterial(0, ContactMaterial);
	ContactMesh->SetCustomPrimitiveDataFloat(0, BaseHalf.X / PlaneHalfX);
	ContactMesh->SetCustomPrimitiveDataFloat(1, BaseHalf.Y / PlaneHalfY);
	ContactMesh->SetWorldTransform(FTransform(
		ActorTransform.GetRotation(),
		ActorTransform.TransformPosition(ActorLocalCenter),
		FVector(PlaneHalfX * ActorScale.X / 50.f, PlaneHalfY * ActorScale.Y / 50.f, 1.f)));
	return true;
}

void BuildingGroundContact::AddToCityBuilding(AActor* Owner)
{
	if (!Owner || !Owner->GetWorld() || Owner->GetWorld()->GetFeatureLevel() != ERHIFeatureLevel::ES3_1)
	{
		return;
	}

	UStaticMeshComponent* BaseMesh = nullptr;
	TArray<UStaticMeshComponent*> MeshComponents;
	Owner->GetComponents<UStaticMeshComponent>(MeshComponents);
	for (UStaticMeshComponent* MeshComponent : MeshComponents)
	{
		if (MeshComponent && MeshComponent->GetFName() == ContactComponentName)
		{
			return;
		}
		if (MeshComponent && MeshComponent->GetFName() == BaseComponentName)
		{
			BaseMesh = MeshComponent;
		}
	}
	if (!BaseMesh || !BaseMesh->GetStaticMesh())
	{
		return;
	}

	UStaticMeshComponent* ContactMesh = NewObject<UStaticMeshComponent>(Owner, ContactComponentName);
	ContactMesh->SetupAttachment(BaseMesh);
	ContactMesh->SetMobility(BaseMesh->Mobility);
	ContactMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ContactMesh->SetGenerateOverlapEvents(false);
	ContactMesh->SetCanEverAffectNavigation(false);
	ContactMesh->SetCastShadow(false);
	ContactMesh->bUseAsOccluder = false;
	ContactMesh->bUseAttachParentBound = true;
	ContactMesh->SetVisibility(false);
	Owner->AddInstanceComponent(ContactMesh);
	const bool bConfigured = Configure(Owner, BaseMesh, ContactMesh);
	ContactMesh->RegisterComponent();
	ContactMesh->SetVisibility(bConfigured);
}
