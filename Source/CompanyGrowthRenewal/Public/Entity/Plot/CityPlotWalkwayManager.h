#pragma once

#include "CoreMinimal.h"
#include "Entity/Plot/CityPlotStreetSurface.h"
#include "Entity/Plot/CityPlotStreetFurniture.h"
#include "Entity/Plot/PlotWalkwayRules.h"
#include "GameFramework/Actor.h"
#include "CityPlotWalkwayManager.generated.h"

class ACityPlotActor;
class UCityAcquisitionManager;
class UEntityManager;
class ULevel;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;
struct FVacantPlotFootprint;

struct FCityWalkwayCollisionBody
{
	TWeakObjectPtr<UStaticMeshComponent> Component;
	int32 InstanceIndex = INDEX_NONE;
	FBox Bounds = FBox(ForceInit);
};

struct FCityWalkwayOccupant
{
	TWeakObjectPtr<AActor> Actor;
	FPlotWalkwayTarget Target;
	FTransform CommittedTransform = FTransform::Identity;
	TArray<FCityWalkwayCollisionBody> CollisionBodies;
	bool bRequiresAccess = true;
};

struct FCityWalkwayEntrance
{
	int32 Cell = INDEX_NONE;
	FBox2D Bounds;
	FBox2D GroundedRoadBounds;
	FBox2D JunctionBounds;
	float RoadZ = 0.f;
	FVector2D InwardNormal = FVector2D::ZeroVector;
	TWeakObjectPtr<UMaterialInterface> RoadMaterial;
};

struct FCityWalkwayRoad
{
	FBox2D Bounds;
	FVector2D Direction = FVector2D::ZeroVector;
	float Height = 0.f;
	TWeakObjectPtr<UMaterialInterface> Material;
	bool bCanConnect = true;
};

struct FCityWalkwayPlot
{
	TWeakObjectPtr<ACityPlotActor> Actor;
	FPlotWalkwayGrid Grid;
	TArray<float> GroundHeights;
	TArray<FBox2D> FixedBlockers;
	TArray<FCityWalkwayEntrance> Entrances;
	TArray<FCityWalkwayRoad> RoadStrips;
	TArray<FCityWalkwayOccupant> Occupants;
	FPlotWalkwayResult Result;
	TArray<FBox2D> ReservedBounds;
	TArray<FCityStreetSurfaceRegion> Surfaces;
	bool bGroundReady = false;
};

/** Actual ground and building collision determine committed and preview street networks. */
UCLASS(Transient, NotBlueprintable)
class COMPANYGROWTHRENEWAL_API ACityPlotWalkwayManager : public AActor
{
	GENERATED_BODY()

public:
	ACityPlotWalkwayManager();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void RefreshPlot(FName PlotId, const AActor* CommittedActor = nullptr);
	void RefreshPlots(FName FirstPlotId, FName SecondPlotId, const AActor* CommittedActor = nullptr);
	void RefreshAllPlots();

	// Evaluates the proposed footprint without moving buildings or changing the rendered network.
	bool CanPlaceBuilding(FName PlotId, const FVector2D& Center, const FVector2D& HalfExtent,
		const AActor* PreviewActor) const;
	void UpdateBuildingPreview(FName PlotId, const FVector2D& Center, const FVector2D& HalfExtent,
		AActor* PreviewActor);
	// The raw finger position while dragging; a pending street rebuild waits until it stops moving.
	void NotePreviewDragActivity(const FVector2D& FingerXY);
	void ClearBuildingPreview();
	void AppendReservedFootprints(FName PlotId, TArray<FVacantPlotFootprint>& OutFootprints,
		bool bIncludePreview = false) const;

	UFUNCTION(BlueprintPure, Category = "City|Walkways")
	bool IsPlotReady(FName PlotId) const;

	UFUNCTION(BlueprintPure, Category = "City|Walkways")
	int32 GetPathCellCount(FName PlotId) const;

	UFUNCTION(BlueprintPure, Category = "City|Walkways")
	int32 GetConnectedBuildingCount(FName PlotId) const;

private:
	friend class FCGRPlotWalkwayPreviewLifecycleTest;
	friend class FCGRPlotMultipleRoadEntrancesTest;
	void RebuildBuildingPreview();
	void RequestPreviewRebuild();
	void RefreshPreviewDressing(TConstArrayView<FName> PlotIds);
	bool CanPlaceStreetFurniture(const FBox& Bounds, float OriginalGroundZ) const;
	void ScheduleRefresh();
	void HandleLevelChanged(ULevel* ChangedLevel, UWorld* ChangedWorld);
	void HandleCompanyCleared(int32 CompanyKey);
	bool IsGeometryReady() const;
	bool BuildGround(ACityPlotActor* Plot, FCityWalkwayPlot& State);
	void GatherOccupants(ACityPlotActor* Plot, FCityWalkwayPlot& State, const AActor* CommittedActor) const;
	void GatherExistingRoads();
	void RebuildSurface(FCityWalkwayPlot& State);
	void ApplyStreetSurfaces();

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> RoadMesh = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> RoadDoubleMesh = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> JunctionMaterial = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> SidewalkMesh = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> CurbMesh = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> PedestrianMaterial = nullptr;

	TMap<FName, FCityWalkwayPlot> Plots;
	TArray<FCityWalkwayRoad> ExistingRoads;
	FCityPlotStreetSurface StreetSurface;
	FCityPlotStreetFurniture StreetFurniture;
	TMap<FName, FCityWalkwayPlot> PreviewPlots;
	TArray<FCityStreetSurfaceRegion> AppliedSurfaces;
	TArray<FCityStreetSurfaceRegion> RenderedSurfaces;
	TWeakObjectPtr<AActor> PreviewBuilding;
	FName PreviewPlotId = NAME_None;
	FVector2D PreviewCenter = FVector2D::ZeroVector;
	FVector2D PreviewHalfExtent = FVector2D::ZeroVector;
	FQuat PreviewRotation = FQuat::Identity;
	FTimerHandle PreviewTimer;
	double LastPreviewUpdateTime = -1.0;
	FVector2D LastDragFingerXY = FVector2D::ZeroVector;
	bool bAppliedSurfacesValid = false;
	TWeakObjectPtr<UEntityManager> EntityManager;
	TWeakObjectPtr<UCityAcquisitionManager> AcquisitionManager;
	FDelegateHandle BuildingChangedHandle;
	FDelegateHandle CompanyClearedHandle;
	FDelegateHandle LevelAddedHandle;
	FDelegateHandle LevelRemovedHandle;
	FTimerHandle RefreshTimer;
	int32 InitializationRetries = 0;
	bool bGroundQueriesPrepared = false;
	// StreetSurface-channel heights only change with level streaming; a placement preview re-probes the same points.
	TMap<FIntVector, float> GroundSampleCache;
	bool bCommittedPlotChanged = false;
	bool bRefreshing = false;
	bool bBatchRefreshing = false;
	bool bSurfacesDirty = false;

	static constexpr float WalkwayWidth = 500.f;
	static constexpr float RoadWidth = 1000.f;
	static constexpr float MaximumGroundStep = 20.f;
	static constexpr float PreviewSettleSeconds = 0.25f;
};
