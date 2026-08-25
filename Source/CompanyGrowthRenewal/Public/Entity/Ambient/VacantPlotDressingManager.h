#pragma once

#include "CoreMinimal.h"
#include "Entity/Ambient/VacantPlotDressingRules.h"
#include "GameFramework/Actor.h"
#include "VacantPlotDressingManager.generated.h"

class ABuildingBaseActor;
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
};

struct FVacantDressingPatch
{
	FName PlotId = NAME_None;
	FVacantPlotFootprint Footprint;
	TArray<FVacantDressingInstanceRef> Instances;
	float VisibilityAlpha = 0.f;
	bool bTargetVisible = false;
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

	void RefreshPlot(FName PlotId);
	void RefreshPlots(FName FirstPlotId, FName SecondPlotId);
	void RefreshAllPlots();

	int32 GetPatchCount() const { return Patches.Num(); }
	int32 GetHISMComponentCount() const { return HISMComponents.Num(); }
	int32 GetVisiblePatchCount() const;

	static bool ShouldPatchBeVisible(
		const FVacantPlotFootprint& Patch,
		TConstArrayView<FVacantPlotFootprint> Occupancies,
		float SafetyPadding = 75.f);

private:
	void AttemptInitialize();
	bool ResolveRuntimeDependencies();
	bool IsCityMappingReady() const;
	bool IsWorldGeometryReady() const;
	void BuildPatches();
	void BindRuntimeEvents();
	void RefreshPlotInternal(FName PlotId, bool bInstant);
	void RefreshAllPlotsInternal(bool bInstant);
	void GatherOccupancies(FName PlotId, TArray<FVacantPlotFootprint>& OutOccupancies) const;
	void AddPlayerBuildingFootprint(
		const ABuildingBaseActor* Building,
		const FVector2D& PlotCenter,
		const FVector2D& PlotHalfExtent,
		TArray<FVacantPlotFootprint>& OutOccupancies) const;
	void SetPatchVisibilityTarget(int32 PatchIndex, bool bVisible, bool bInstant);
	void ApplyPatchTransform(int32 PatchIndex);
	void MarkAllHISMRenderStatesDirty();
	UHierarchicalInstancedStaticMeshComponent* GetOrCreateHISM(UStaticMesh* Mesh, bool bCastShadow);

	void HandleBuildingCountChanged();
	void HandleCompanyVisualCleared(int32 CompanyKey);

	UPROPERTY(VisibleAnywhere, Category = "City Dressing")
	TObjectPtr<USceneComponent> SceneRoot = nullptr;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> HISMComponents;

	TMap<UStaticMesh*, UHierarchicalInstancedStaticMeshComponent*> MeshComponents;
	TArray<FVacantDressingPatch> Patches;
	TMultiMap<FName, int32> PatchIndicesByPlot;
	TArray<FName> PlotIds;

	TWeakObjectPtr<UTableManagerSubsystem> TableManager;
	TWeakObjectPtr<USpawnManager> SpawnManager;
	TWeakObjectPtr<UEntityManager> EntityManager;
	TWeakObjectPtr<UCityAcquisitionManager> AcquisitionManager;

	FDelegateHandle BuildingCountChangedHandle;
	FDelegateHandle CompanyVisualClearedHandle;
	FTimerHandle InitializationRetryTimer;

	FName PreviewPlotId = NAME_None;
	FVacantPlotFootprint PreviewFootprint;
	bool bHasPreview = false;
	bool bInitialized = false;
	int32 InitializationAttemptCount = 0;

	static constexpr float OverlapSafetyPadding = 75.f;
	static constexpr float TransitionDuration = 0.15f;
	static constexpr float HiddenDropDistance = 25.f;
	static constexpr int32 MaxInitializationAttempts = 60;
};
