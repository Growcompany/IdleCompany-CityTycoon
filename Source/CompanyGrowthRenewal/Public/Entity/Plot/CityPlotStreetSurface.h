#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"

class AActor;
class UActorComponent;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

struct FCityStreetSurfaceRegion
{
	FBox2D Bounds = FBox2D(ForceInit);
	float SurfaceZ = 0.f;
	float CurbBaseZ = 0.f;
	bool bVehicle = false;
	bool bJunction = false;
	bool bInternalJunction = false;
	bool bOpenAtStart = false;
	FVector2D Direction = FVector2D(1.0, 0.0);
	UMaterialInterface* RoadMaterial = nullptr;
};

/** Runtime-only replacement of the original City_Pack floor inside committed street cuts. */
class COMPANYGROWTHRENEWAL_API FCityPlotStreetSurface
{
public:
	bool Initialize(AActor* InOwner, UStaticMesh* InRoadSquare, UStaticMesh* InSidewalkFlat,
		UStaticMesh* InSidewalkStraight, UMaterialInterface* InPedestrianMaterial,
		UStaticMesh* InRoadDouble, UMaterialInterface* InJunctionMaterial);
	bool Rebuild(TConstArrayView<FCityStreetSurfaceRegion> Regions,
		TArray<FCityStreetSurfaceRegion>* OutAppliedRegions = nullptr);
	void Restore();

private:
	struct FModifiedSource
	{
		TWeakObjectPtr<UStaticMeshComponent> Component;
		int32 InstanceIndex = INDEX_NONE;
		FTransform OriginalLocal = FTransform::Identity;
		FTransform OriginalWorld = FTransform::Identity;
		TWeakObjectPtr<UStaticMesh> Mesh;
		TWeakObjectPtr<UStaticMeshComponent> CollisionProxy;
		bool bInstanced = false;
		bool bRoadSource = false;
	};

	TWeakObjectPtr<AActor> Owner;
	TWeakObjectPtr<UStaticMesh> RoadSquare;
	TWeakObjectPtr<UStaticMesh> RoadDouble;
	TWeakObjectPtr<UStaticMesh> SidewalkFlat;
	TWeakObjectPtr<UStaticMesh> SidewalkStraight;
	TWeakObjectPtr<UMaterialInterface> PedestrianMaterial;
	TWeakObjectPtr<UMaterialInterface> JunctionMaterial;
	TArray<FModifiedSource> ModifiedSources;
	TArray<TWeakObjectPtr<UActorComponent>> GeneratedComponents;
};
