#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"
#include "Entity/Plot/CityPlotStreetSurface.h"

class AActor;
class ULevel;
class UInstancedStaticMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;

/** Runtime-only relocation of authored lamps and generation of matching lamps beside road cuts. */
class COMPANYGROWTHRENEWAL_API FCityPlotStreetFurniture
{
public:
	void Initialize(AActor* InOwner);
	static bool IsStreetLampMesh(const UStaticMesh* Mesh);
	void Rebuild(TConstArrayView<FCityStreetSurfaceRegion> Regions,
		const TFunction<bool(const FBox&, float OriginalGroundZ)>& CanPlace);
	void Restore();
	void InvalidateSources();

private:
	struct FSource
	{
		TWeakObjectPtr<UStaticMeshComponent> Component;
		int32 InstanceIndex = INDEX_NONE;
		FTransform OriginalLocal = FTransform::Identity;
		FTransform OriginalWorld = FTransform::Identity;
		FVector2D RowDirection = FVector2D(1.0, 0.0);
		float GroundZ = 0.f;
		bool bOriginallyVisible = true;
	};
	struct FGeneratedGroup
	{
		TWeakObjectPtr<UStaticMeshComponent> Template;
		TWeakObjectPtr<UInstancedStaticMeshComponent> Instances;
		TArray<FTransform> WorldTransforms;
	};

	void CollectSources();
	void Apply(const FSource& Source, const FTransform& DesiredWorld, bool bVisible);
	void RebuildGenerated(TConstArrayView<FCityStreetSurfaceRegion> Regions,
		const TArray<FVector>& AuthoredGroundBases,
		const TFunction<bool(const FBox&, float OriginalGroundZ)>& CanPlace);
	void ClearGenerated();
	void RefreshLampPools();
	TWeakObjectPtr<AActor> Owner;
	TArray<FSource> Sources;
	TArray<FGeneratedGroup> GeneratedGroups;
	TArray<ULevel*> ChangedLevels;
	bool bSourcesDirty = true;
};
