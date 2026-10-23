#pragma once

#include "CoreMinimal.h"
#include "Containers/Set.h"
#include "Entity/Ambient/VacantPlotDressingRules.h"
#include "GameFramework/Actor.h"
#include "VacantPlotDressingManager.generated.h"

class ABuildingBaseActor;
class ACityPlotActor;
class UCityAcquisitionManager;
class UEntityManager;
class UHierarchicalInstancedStaticMeshComponent;
class USceneComponent;
class USpawnManager;
class UStaticMesh;
class UTableManagerSubsystem;

struct FVacantDressingInstanceRef
{
	TWeakObjectPtr<UHierarchicalInstancedStaticMeshComponent> Component;
	int32 InstanceIndex = INDEX_NONE;
	FTransform VisibleTransform = FTransform::Identity;
	float GroundOffsetZ = 0.f;
};

struct FVacantDressingPatch
{
	FName PlotId = NAME_None;
	int32 PatchOrdinal = INDEX_NONE;
	FVacantPlotFootprint Footprint;
	FVacantPlotFootprint PendingFootprint;
	TArray<FVacantDressingInstanceRef> Instances;
	float VisibilityAlpha = 0.f;
	bool bTargetVisible = false;
	bool bHasPendingRelocation = false;
	bool bVisibleAfterRelocation = false;
	bool bHasResolvedPlacement = false;
	bool bFixedAnchor = false;
};

enum class EVacantPlotDressingRefreshMode : uint8
{
	Preview,
	Committed
};

struct FVacantDressingPlacementDecision
{
	FVacantPlotFootprint ResolvedFootprint;
	bool bTargetVisible = false;
	bool bQueueRelocation = false;
};

struct FVacantPlotDressingManagerStateRules
{
	static FVacantDressingPlacementDecision ResolveFixedAnchor(
		const FVacantPlotFootprint& AnchorFootprint,
		bool bAnchorAvailable);

	static FVacantDressingPlacementDecision ResolvePlacement(
		const FVacantPlotFootprint& CurrentFootprint,
		bool bCurrentPlacementValid,
		EVacantPlotDressingRefreshMode RefreshMode,
		TConstArrayView<FVacantPlotFootprint> RankedCandidates);

	static void ApplyRelocationDelta(
		FVacantDressingPatch& Patch,
		const FVacantPlotFootprint& DestinationFootprint);
};

/**
 * 도시 부지 전체의 작은 생활 소품을 메시별 HISM으로 일괄 관리한다.
 * 배치 상태는 저장하지 않고 부지, 건물, 인수회사 상태에서 매번 파생한다.
 */
UCLASS(Transient, NotBlueprintable)
class COMPANYGROWTHRENEWAL_API AVacantPlotDressingManager : public AActor
{
	GENERATED_BODY()

public:
	AVacantPlotDressingManager();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	// 배치 프리뷰가 바뀐 부지만 갱신한다. HalfExtent는 회전이 반영된 월드 XY AABB 반경이다.
	void UpdateBuildingPreview(FName PlotId, const FVector2D& Center, const FVector2D& HalfExtent);
	void ClearBuildingPreview();
	// 가시성만 재평가하며 후보 탐색, 재배치 큐, Patch XY 변경은 하지 않는다.
	void RefreshPreviewVisibilityPlots(FName FirstPlotId, FName SecondPlotId);

	void RefreshPlot(FName PlotId);
	void RefreshPlots(FName FirstPlotId, FName SecondPlotId);
	void RefreshAllPlots();

	int32 GetPatchCount() const { return Patches.Num(); }
	int32 GetHISMComponentCount() const { return HISMComponents.Num(); }
	int32 GetVisiblePatchCount() const;

	UFUNCTION(BlueprintPure, Category = "City Dressing|Diagnostics")
	bool AreDressingComponentsNavigationDisabled() const;

	static bool ShouldPatchBeVisible(
		const FVacantPlotFootprint& Patch,
		TConstArrayView<FVacantPlotFootprint> Occupancies,
		float SafetyPadding = 75.f);

private:
	void AttemptInitialize();
	void RetryPendingPlots();
	bool ResolveRuntimeDependencies();
	bool IsCityMappingReady() const;
	bool IsWorldGeometryReady() const;
	void BuildPatches(FName OnlyPlotId = NAME_None);
	void RebuildPatchesForActorReplacement();
	void BindRuntimeEvents();
	void RefreshPreviewVisibilityInternal(FName PlotId, bool bInstant);
	void RefreshPlotInternal(FName PlotId, bool bInstant);
	void RefreshAllPlotsInternal(bool bInstant);
	bool TryResolveCommittedPatchPlacement(
		int32 PatchIndex,
		ACityPlotActor* PlotActor,
		TConstArrayView<FVacantPlotFootprint> Occupancies,
		FVacantPlotFootprint& OutFootprint) const;
	void QueuePatchRelocation(
		int32 PatchIndex,
		const FVacantPlotFootprint& DestinationFootprint,
		bool bVisibleAfterRelocation);
	void CommitPatchRelocation(int32 PatchIndex);
	void GatherOccupancies(
		FName PlotId,
		TArray<FVacantPlotFootprint>& OutOccupancies,
		bool bIncludePreview) const;
	void AddPlayerBuildingFootprint(
		const ABuildingBaseActor* Building,
		const FVector2D& PlotCenter,
		const FVector2D& PlotHalfExtent,
		TArray<FVacantPlotFootprint>& OutOccupancies) const;
	void SetPatchVisibilityTarget(int32 PatchIndex, bool bVisible, bool bInstant);
	void ApplyPatchTransform(int32 PatchIndex);
	void MarkAllHISMRenderStatesDirty();
	UHierarchicalInstancedStaticMeshComponent* GetOrCreateHISM(UStaticMesh* Mesh,
		bool bCastShadow, int32 CullDistance);

	void HandleBuildingCountChanged();
	void HandleCompanyVisualCleared(int32 CompanyKey);

	UPROPERTY(VisibleAnywhere, Category = "City Dressing")
	TObjectPtr<USceneComponent> SceneRoot = nullptr;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> HISMComponents;

	TMap<UStaticMesh*, UHierarchicalInstancedStaticMeshComponent*> MeshComponents;
	TArray<FVacantDressingPatch> Patches;
	TMultiMap<FName, int32> PatchIndicesByPlot;
	TMap<FName, TArray<FVacantPlotFootprint>> FixedBlockersByPlot;
	TMap<FName, TWeakObjectPtr<ACityPlotActor>> CachedPlotActors;
	TArray<FName> PlotIds;
	TSet<FName> PendingPlotIds;

	TWeakObjectPtr<UTableManagerSubsystem> TableManager;
	TWeakObjectPtr<USpawnManager> SpawnManager;
	TWeakObjectPtr<UEntityManager> EntityManager;
	TWeakObjectPtr<UCityAcquisitionManager> AcquisitionManager;

	FDelegateHandle BuildingCountChangedHandle;
	FDelegateHandle CompanyVisualClearedHandle;
	FTimerHandle InitializationRetryTimer;
	FTimerHandle PendingPlotRetryTimer;

	FName PreviewPlotId = NAME_None;
	FVacantPlotFootprint PreviewFootprint;
	bool bHasPreview = false;
	bool bInitialized = false;
	int32 InitializationAttemptCount = 0;

	static constexpr float OverlapSafetyPadding = 75.f;
	static constexpr float TransitionDuration = 0.15f;
	static constexpr int32 MaxInitializationAttempts = 60;
};
