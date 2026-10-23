#include "Entity/Ambient/VacantPlotDressingManager.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Containers/Set.h"
#include "Core/CGGameInstance.h"
#include "Engine/GameInstance.h"
#include "Engine/Level.h"
#include "Engine/LevelStreaming.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Entity/Building/BuildingBaseActor.h"
#include "Entity/Factory/BrickFactory.h"
#include "Entity/Plot/CityPlotActor.h"
#include "Entity/Plot/CityPlotWalkwayManager.h"
#include "Manager/CityAcquisitionManager.h"
#include "Manager/CityCompanyDirector.h"
#include "Manager/EntityManager.h"
#include "Manager/SpawnManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Misc/Crc.h"
#include "Table/BuildingData.h"
#include "Table/CityDressingData.h"
#include "Table/CityPlotData.h"
#include "TimerManager.h"
#include "WorldPartition/WorldPartitionSubsystem.h"

namespace
{
	struct FRuntimeDressingPreset
	{
		FName PresetId = NAME_None;
		bool bForAuthoredPlots = false;
		bool bPlaceAlongBoundary = false;
		float BoundarySpacing = 1200.f;
		float BoundaryInset = 75.f;
		int32 PlotCellsPerPatch = 0;
		int32 MaxPatchesPerPlot = 2;
		TArray<FCityDressingData> Slots;
		TArray<UStaticMesh*> LoadedMeshes;
		FVector2D BoundsCenter = FVector2D::ZeroVector;
		FVector2D BoundsHalfExtent = FVector2D::ZeroVector;
	};

	struct FFixedDressingBlocker
	{
		FVacantPlotFootprint Footprint;
		float MinimumZ = 0.f;
		float MaximumZ = 0.f;
	};

	constexpr float FixedBlockerMinimumHeight = 50.f;
	constexpr float FixedBlockerVerticalClearance = 1000.f;
	constexpr float FixedBlockerMaximumHalfExtent = 2000.f;
	constexpr int32 MaxGroundTraceCandidateCount = 24;

	FVector2D RotateVector2D(const FVector2D& Value, float YawDegrees)
	{
		const float Radians = FMath::DegreesToRadians(YawDegrees);
		float SinValue = 0.f;
		float CosValue = 1.f;
		FMath::SinCos(&SinValue, &CosValue, Radians);
		return FVector2D(
			Value.X * CosValue - Value.Y * SinValue,
			Value.X * SinValue + Value.Y * CosValue);
	}

	FVector2D RotateAabbHalfExtent(const FVector2D& HalfExtent, float YawDegrees)
	{
		const float Radians = FMath::DegreesToRadians(YawDegrees);
		const float AbsCos = FMath::Abs(FMath::Cos(Radians));
		const float AbsSin = FMath::Abs(FMath::Sin(Radians));
		return FVector2D(
			AbsCos * FMath::Abs(HalfExtent.X) + AbsSin * FMath::Abs(HalfExtent.Y),
			AbsSin * FMath::Abs(HalfExtent.X) + AbsCos * FMath::Abs(HalfExtent.Y));
	}

	uint32 MakeStableSeed(FName PlotId, int32 PatchIndex, FName SlotId = NAME_None)
	{
		FString SeedString = FString::Printf(TEXT("%s|%d"), *PlotId.ToString(), PatchIndex);
		if (!SlotId.IsNone())
		{
			SeedString += TEXT("|");
			SeedString += SlotId.ToString();
		}
		return FCrc::StrCrc32(*SeedString);
	}

	float ResolveStableScale(const FCityDressingData& Slot, FName PlotId, int32 PatchIndex)
	{
		const float Minimum = FMath::Max(0.01f, Slot.ScaleVariationMin);
		const float Maximum = FMath::Max(Minimum, Slot.ScaleVariationMax);
		const float UnitValue = static_cast<float>(MakeStableSeed(PlotId, PatchIndex, Slot.SlotId) % 10001u)
			/ 10000.f;
		return FMath::Lerp(Minimum, Maximum, UnitValue);
	}

	void ResolvePresetBounds(FRuntimeDressingPreset& Preset)
	{
		FVector2D Minimum(FLT_MAX, FLT_MAX);
		FVector2D Maximum(-FLT_MAX, -FLT_MAX);
		for (int32 SlotIndex = 0; SlotIndex < Preset.Slots.Num(); ++SlotIndex)
		{
			const FCityDressingData& Slot = Preset.Slots[SlotIndex];
			const UStaticMesh* LoadedMesh = Preset.LoadedMeshes.IsValidIndex(SlotIndex)
				? Preset.LoadedMeshes[SlotIndex] : nullptr;
			const float MinimumVariation = FMath::Max(0.01f, Slot.ScaleVariationMin);
			const float MaximumVariation = FMath::Max(MinimumVariation, Slot.ScaleVariationMax);
			for (const float Variation : { MinimumVariation, MaximumVariation })
			{
				const FTransform SlotTransform(
					Slot.LocalRotation, Slot.LocalLocation, Slot.LocalScale * Variation);
				const FBox MeshBox = LoadedMesh->GetBoundingBox().TransformBy(
					SlotTransform.ToMatrixWithScale());
				const FVector2D AuthoredHalfExtent = RotateAabbHalfExtent(
					FVector2D(FMath::Abs(Slot.HalfExtent.X * Slot.LocalScale.X * Variation),
						FMath::Abs(Slot.HalfExtent.Y * Slot.LocalScale.Y * Variation)),
						Slot.LocalRotation.Yaw);
				Minimum.X = FMath::Min(Minimum.X,
					FMath::Min(MeshBox.Min.X, Slot.LocalLocation.X - AuthoredHalfExtent.X));
				Minimum.Y = FMath::Min(Minimum.Y,
					FMath::Min(MeshBox.Min.Y, Slot.LocalLocation.Y - AuthoredHalfExtent.Y));
				Maximum.X = FMath::Max(Maximum.X,
					FMath::Max(MeshBox.Max.X, Slot.LocalLocation.X + AuthoredHalfExtent.X));
				Maximum.Y = FMath::Max(Maximum.Y,
					FMath::Max(MeshBox.Max.Y, Slot.LocalLocation.Y + AuthoredHalfExtent.Y));
			}
		}

		if (Preset.Slots.Num() == 0 || Minimum.X >= Maximum.X || Minimum.Y >= Maximum.Y)
		{
			Preset.BoundsCenter = FVector2D::ZeroVector;
			Preset.BoundsHalfExtent = FVector2D::ZeroVector;
			return;
		}
		Preset.BoundsCenter = (Minimum + Maximum) * 0.5f;
		Preset.BoundsHalfExtent = (Maximum - Minimum) * 0.5f;
	}

	bool PreloadPresetMeshes(FRuntimeDressingPreset& Preset)
	{
		Preset.LoadedMeshes.Reset();
		Preset.LoadedMeshes.Reserve(Preset.Slots.Num());
		bool bAllMeshesLoaded = true;
		for (const FCityDressingData& Slot : Preset.Slots)
		{
			UStaticMesh* LoadedMesh = Slot.Mesh.LoadSynchronous();
			Preset.LoadedMeshes.Add(LoadedMesh);
			if (!LoadedMesh)
			{
				bAllMeshesLoaded = false;
				UE_LOG(LogTemp, Warning,
					TEXT("[CityDressing] Incomplete preset skipped preset=%s slot=%s path=%s"),
					*Preset.PresetId.ToString(), *Slot.SlotId.ToString(), *Slot.Mesh.ToString());
			}
		}

		if (!bAllMeshesLoaded)
		{
			return false;
		}
		return true;
	}

	void RollbackCandidateTailInstances(FVacantDressingPatch& Patch)
	{
		for (int32 InstanceRefIndex = Patch.Instances.Num() - 1;
			InstanceRefIndex >= 0;
			--InstanceRefIndex)
		{
			const FVacantDressingInstanceRef& InstanceRef = Patch.Instances[InstanceRefIndex];
			UHierarchicalInstancedStaticMeshComponent* Component = InstanceRef.Component.Get();
			if (!Component || InstanceRef.InstanceIndex == INDEX_NONE)
			{
				continue;
			}

			const int32 TailInstanceIndex = Component->GetInstanceCount() - 1;
			if (InstanceRef.InstanceIndex != TailInstanceIndex)
			{
				UE_LOG(LogTemp, Error,
					TEXT("[CityDressing] Candidate rollback refused non-tail instance index=%d tail=%d"),
					InstanceRef.InstanceIndex, TailInstanceIndex);
				// 아래 Instances.Reset() 이 참조를 버리므로 그냥 두면 회수 불가능한 프롭이 남는다.
				// 제거는 못 해도 스케일 0 으로 눌러 보이지 않게 만든다.
				FTransform HiddenTransform = InstanceRef.VisibleTransform;
				HiddenTransform.SetScale3D(FVector::ZeroVector);
				Component->UpdateInstanceTransform(
					InstanceRef.InstanceIndex, HiddenTransform,
					/*bWorldSpace=*/true, /*bMarkRenderStateDirty=*/true, /*bTeleport=*/true);
				continue;
			}
			if (!Component->RemoveInstance(TailInstanceIndex))
			{
				UE_LOG(LogTemp, Error,
					TEXT("[CityDressing] Candidate rollback failed instance=%d"),
					TailInstanceIndex);
			}
		}
		Patch.Instances.Reset();
	}

	bool IsFixedBlockerVerticallyRelevant(
		const FFixedDressingBlocker& Blocker,
		float PlotSurfaceZ)
	{
		return Blocker.MaximumZ >= PlotSurfaceZ + FixedBlockerMinimumHeight
			&& Blocker.MinimumZ <= PlotSurfaceZ + FixedBlockerVerticalClearance;
	}

	void TryAddFixedDressingBlocker(
		const FBox& WorldBounds,
		TArray<FFixedDressingBlocker>& OutBlockers)
	{
		if (!WorldBounds.IsValid)
		{
			return;
		}

		const FVector BoundsExtent = WorldBounds.GetExtent();
		if (BoundsExtent.X <= 0.f || BoundsExtent.Y <= 0.f
			|| BoundsExtent.Z < FixedBlockerMinimumHeight
			|| BoundsExtent.X > FixedBlockerMaximumHalfExtent
			|| BoundsExtent.Y > FixedBlockerMaximumHalfExtent)
		{
			return;
		}

		const FVector BoundsCenter = WorldBounds.GetCenter();
		FFixedDressingBlocker& Blocker = OutBlockers.AddDefaulted_GetRef();
		Blocker.Footprint = FVacantPlotFootprint(
			FVector2D(BoundsCenter.X, BoundsCenter.Y),
			FVector2D(BoundsExtent.X, BoundsExtent.Y));
		Blocker.MinimumZ = WorldBounds.Min.Z;
		Blocker.MaximumZ = WorldBounds.Max.Z;
	}

	void GatherFixedDressingBlockers(
		UWorld* World,
		const AActor* DressingOwner,
		TArray<FFixedDressingBlocker>& OutBlockers)
	{
		OutBlockers.Reset();
		if (!World)
		{
			return;
		}

		TSet<TWeakObjectPtr<AActor>> MappedCompanyActors;
		if (const UCityCompanyDirector* CityDirector =
			World->GetSubsystem<UCityCompanyDirector>())
		{
			for (const FCitySkylineEntry& SkylineEntry : CityDirector->GetSkylineEntries())
			{
				AActor* CompanyActor = SkylineEntry.Building.Get();
				if (IsValid(CompanyActor) && !CompanyActor->IsActorBeingDestroyed())
				{
					MappedCompanyActors.Add(CompanyActor);
				}
			}
		}
		const bool bHasAuthoritativeCompanyMapping = !MappedCompanyActors.IsEmpty();

		for (TActorIterator<AActor> ActorIterator(World); ActorIterator; ++ActorIterator)
		{
			AActor* MeshOwner = *ActorIterator;
			if (!IsValid(MeshOwner) || MeshOwner == DressingOwner
				|| MeshOwner->IsActorBeingDestroyed() || MeshOwner->IsHidden()
				|| MeshOwner->IsA<ABuildingBaseActor>() || MeshOwner->IsA<ABrickFactory>())
			{
				continue;
			}

			const bool bMappedCompanyActor = MappedCompanyActors.Contains(MeshOwner);
			const bool bPotentialCompanyActorWithoutMapping =
				!bHasAuthoritativeCompanyMapping
				&& MeshOwner->GetClass()
				&& UCityCompanyDirector::ParseBuildingKey(
					MeshOwner->GetClass()->GetName()) > 0;
			if (bMappedCompanyActor || bPotentialCompanyActorWithoutMapping)
			{
				continue;
			}

			TInlineComponentArray<UInstancedStaticMeshComponent*> InstancedComponents;
			MeshOwner->GetComponents(InstancedComponents);
			for (UInstancedStaticMeshComponent* Component : InstancedComponents)
			{
				if (!IsValid(Component) || !Component->IsRegistered() || !Component->IsVisible()
					|| Component->Mobility == EComponentMobility::Movable)
				{
					continue;
				}

				const UStaticMesh* Mesh = Component->GetStaticMesh();
				if (!Mesh)
				{
					continue;
				}

				const FBox LocalBounds = Mesh->GetBoundingBox();
				for (int32 InstanceIndex = 0;
					InstanceIndex < Component->GetInstanceCount();
					++InstanceIndex)
				{
					FTransform WorldTransform;
					if (Component->GetInstanceTransform(
						InstanceIndex, WorldTransform, /*bWorldSpace=*/true))
					{
						TryAddFixedDressingBlocker(
							LocalBounds.TransformBy(WorldTransform.ToMatrixWithScale()),
							OutBlockers);
					}
				}
			}
		}
	}
}

FVacantDressingPlacementDecision FVacantPlotDressingManagerStateRules::ResolveFixedAnchor(
	const FVacantPlotFootprint& AnchorFootprint,
	bool bAnchorAvailable)
{
	FVacantDressingPlacementDecision Decision;
	Decision.ResolvedFootprint = AnchorFootprint;
	Decision.bTargetVisible = bAnchorAvailable;
	return Decision;
}

FVacantDressingPlacementDecision FVacantPlotDressingManagerStateRules::ResolvePlacement(
	const FVacantPlotFootprint& CurrentFootprint,
	bool bCurrentPlacementValid,
	EVacantPlotDressingRefreshMode RefreshMode,
	TConstArrayView<FVacantPlotFootprint> RankedCandidates)
{
	FVacantDressingPlacementDecision Decision;
	Decision.ResolvedFootprint = CurrentFootprint;
	Decision.bTargetVisible = bCurrentPlacementValid;
	if (RefreshMode == EVacantPlotDressingRefreshMode::Preview
		|| bCurrentPlacementValid
		|| RankedCandidates.IsEmpty())
	{
		return Decision;
	}

	Decision.ResolvedFootprint = RankedCandidates[0];
	Decision.bTargetVisible = false;
	Decision.bQueueRelocation = true;
	return Decision;
}

void FVacantPlotDressingManagerStateRules::ApplyRelocationDelta(
	FVacantDressingPatch& Patch,
	const FVacantPlotFootprint& DestinationFootprint)
{
	const FVector2D Translation = DestinationFootprint.Center - Patch.Footprint.Center;
	for (FVacantDressingInstanceRef& InstanceRef : Patch.Instances)
	{
		FVector UpdatedLocation = InstanceRef.VisibleTransform.GetLocation();
		UpdatedLocation.X += Translation.X;
		UpdatedLocation.Y += Translation.Y;
		InstanceRef.VisibleTransform.SetLocation(UpdatedLocation);
	}
	Patch.Footprint = DestinationFootprint;
	Patch.bHasResolvedPlacement = true;
}

AVacantPlotDressingManager::AVacantPlotDressingManager()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	SetActorTickEnabled(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	SetActorEnableCollision(false);
}

void AVacantPlotDressingManager::BeginPlay()
{
	Super::BeginPlay();

	UWorld* DressingWorld = GetWorld();
	const UCGGameInstance* CGInstance = DressingWorld
		? Cast<UCGGameInstance>(DressingWorld->GetGameInstance())
		: nullptr;
	if (CGInstance && CGInstance->IsVisitMode())
	{
		UE_LOG(LogTemp, Log, TEXT("[CityDressing] Visit mode: vacant-plot dressing remains hidden"));
		return;
	}

	AttemptInitialize();
}

void AVacantPlotDressingManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* DressingWorld = GetWorld())
	{
		DressingWorld->GetTimerManager().ClearTimer(InitializationRetryTimer);
		DressingWorld->GetTimerManager().ClearTimer(PendingPlotRetryTimer);
	}
	if (EntityManager.IsValid() && BuildingCountChangedHandle.IsValid())
	{
		EntityManager->OnBuildingCountChanged.Remove(BuildingCountChangedHandle);
	}
	if (AcquisitionManager.IsValid() && CompanyVisualClearedHandle.IsValid())
	{
		AcquisitionManager->OnCompanyVisualCleared.Remove(CompanyVisualClearedHandle);
	}
	BuildingCountChangedHandle.Reset();
	CompanyVisualClearedHandle.Reset();

	Super::EndPlay(EndPlayReason);
}

void AVacantPlotDressingManager::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	bool bChangedAnyInstance = false;
	bool bAnyTransitionActive = false;
	bool bAnyPatchReachedFullScale = false;
	const float InterpSpeed = 1.f / TransitionDuration;
	for (int32 PatchIndex = 0; PatchIndex < Patches.Num(); ++PatchIndex)
	{
		FVacantDressingPatch& Patch = Patches[PatchIndex];
		const float TargetAlpha = Patch.bTargetVisible ? 1.f : 0.f;
		if (!FMath::IsNearlyEqual(Patch.VisibilityAlpha, TargetAlpha, KINDA_SMALL_NUMBER))
		{
			const float PreviousAlpha = Patch.VisibilityAlpha;
			Patch.VisibilityAlpha = FMath::FInterpConstantTo(
				Patch.VisibilityAlpha, TargetAlpha, DeltaSeconds, InterpSpeed);
			bAnyPatchReachedFullScale |= TargetAlpha > 0.f
				&& PreviousAlpha < 1.f
				&& FMath::IsNearlyEqual(Patch.VisibilityAlpha, 1.f, KINDA_SMALL_NUMBER);
			ApplyPatchTransform(PatchIndex);
			bChangedAnyInstance = true;
		}
		else
		{
			Patch.VisibilityAlpha = TargetAlpha;
		}

		if (Patch.bHasPendingRelocation
			&& FMath::IsNearlyZero(Patch.VisibilityAlpha, KINDA_SMALL_NUMBER))
		{
			CommitPatchRelocation(PatchIndex);
			bChangedAnyInstance = true;
		}

		const float UpdatedTargetAlpha = Patch.bTargetVisible ? 1.f : 0.f;
		if (!FMath::IsNearlyEqual(
			Patch.VisibilityAlpha,
			UpdatedTargetAlpha,
			KINDA_SMALL_NUMBER))
		{
			bAnyTransitionActive = true;
		}
	}

	if (bChangedAnyInstance)
	{
		MarkAllHISMRenderStatesDirty();
	}
	if (bAnyPatchReachedFullScale)
	{
		for (UHierarchicalInstancedStaticMeshComponent* Component : HISMComponents)
		{
			if (Component)
			{
				Component->BuildTreeIfOutdated(/*Async=*/false, /*ForceUpdate=*/true);
			}
		}
	}
	SetActorTickEnabled(bAnyTransitionActive);
}

bool AVacantPlotDressingManager::ShouldPatchBeVisible(
	const FVacantPlotFootprint& Patch,
	TConstArrayView<FVacantPlotFootprint> Occupancies,
	float SafetyPadding)
{
	if (Patch.HalfExtent.X <= 0.f || Patch.HalfExtent.Y <= 0.f)
	{
		return false;
	}
	return !FVacantPlotDressingRules::OverlapsAny(Patch, Occupancies, SafetyPadding);
}

void AVacantPlotDressingManager::AttemptInitialize()
{
	if (bInitialized)
	{
		return;
	}

	++InitializationAttemptCount;
	if (!ResolveRuntimeDependencies() || !IsCityMappingReady() || !IsWorldGeometryReady())
	{
		if (InitializationAttemptCount < MaxInitializationAttempts)
		{
			if (UWorld* DressingWorld = GetWorld())
			{
				DressingWorld->GetTimerManager().SetTimer(
					InitializationRetryTimer,
					this,
					&AVacantPlotDressingManager::AttemptInitialize,
					0.25f,
					false);
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[CityDressing] Initialization exhausted after %d attempts; dressing stays hidden"),
				InitializationAttemptCount);
		}
		return;
	}

	if (ACityPlotWalkwayManager* WalkwayManager = SpawnManager.IsValid()
		? SpawnManager->GetPlotWalkwayManager() : nullptr)
	{
		WalkwayManager->RefreshAllPlots();
	}
	BuildPatches();
	bInitialized = true;
	BindRuntimeEvents();
	RefreshAllPlotsInternal(/*bInstant=*/true);
	int32 InstanceCount = 0;
	for (const UHierarchicalInstancedStaticMeshComponent* Component : HISMComponents)
	{
		InstanceCount += Component ? Component->GetInstanceCount() : 0;
	}
	UE_LOG(LogTemp, Log,
		TEXT("[CityDressing] Initialized plots=%d patches=%d visible=%d HISM=%d instances=%d"),
		PlotIds.Num(), Patches.Num(), GetVisiblePatchCount(), HISMComponents.Num(), InstanceCount);
}

void AVacantPlotDressingManager::RetryPendingPlots()
{
	if (PendingPlotIds.IsEmpty())
	{
		return;
	}
	if (!IsWorldGeometryReady())
	{
		if (UWorld* DressingWorld = GetWorld())
		{
			DressingWorld->GetTimerManager().SetTimer(
				PendingPlotRetryTimer, this,
				&AVacantPlotDressingManager::RetryPendingPlots, 0.5f, false);
		}
		return;
	}
	TArray<FName> ReadyPlotIds = PendingPlotIds.Array();
	PendingPlotIds.Reset();
	for (const FName PlotId : ReadyPlotIds)
	{
		RefreshPlot(PlotId);
	}
}

bool AVacantPlotDressingManager::ResolveRuntimeDependencies()
{
	UWorld* DressingWorld = GetWorld();
	UGameInstance* GameInstanceLocal = DressingWorld ? DressingWorld->GetGameInstance() : nullptr;
	if (!DressingWorld || !GameInstanceLocal)
	{
		return false;
	}

	TableManager = GameInstanceLocal->GetSubsystem<UTableManagerSubsystem>();
	SpawnManager = DressingWorld->GetSubsystem<USpawnManager>();
	EntityManager = DressingWorld->GetSubsystem<UEntityManager>();
	AcquisitionManager = DressingWorld->GetSubsystem<UCityAcquisitionManager>();
	if (!TableManager.IsValid() || !SpawnManager.IsValid()
		|| !EntityManager.IsValid() || !AcquisitionManager.IsValid())
	{
		return false;
	}

	PlotIds.Reset();
	for (TActorIterator<ACityPlotActor> PlotIterator(DressingWorld); PlotIterator; ++PlotIterator)
	{
		ACityPlotActor* PlotActor = *PlotIterator;
		if (IsValid(PlotActor) && !PlotActor->IsActorBeingDestroyed()
			&& !PlotActor->GetPlotId().IsNone()
			&& SpawnManager->GetSpawnedPlotById(PlotActor->GetPlotId()) == PlotActor)
		{
			PlotIds.AddUnique(PlotActor->GetPlotId());
		}
	}
	PlotIds.Sort([](const FName& Left, const FName& Right)
	{
		return Left.ToString().Compare(Right.ToString(), ESearchCase::CaseSensitive) < 0;
	});
	if (PlotIds.Num() == 0)
	{
		return false;
	}

	return true;
}

bool AVacantPlotDressingManager::IsCityMappingReady() const
{
	const UWorld* DressingWorld = GetWorld();
	// Director는 WorldSubsystem::OnWorldBeginPlay에서 동기적으로 매핑한다.
	// 테이블의 회사가 실제 맵에 없을 수 있으므로 행별 액터 존재 여부는 준비 조건이 아니다.
	return AcquisitionManager.IsValid() && DressingWorld && DressingWorld->HasBegunPlay()
		&& DressingWorld->GetSubsystem<UCityCompanyDirector>() != nullptr;
}

bool AVacantPlotDressingManager::IsWorldGeometryReady() const
{
	UWorld* DressingWorld = GetWorld();
	if (!DressingWorld || !DressingWorld->HasBegunPlay()
		|| !DressingWorld->AreActorsInitialized()
		|| !DressingWorld->PersistentLevel
		|| !DressingWorld->PersistentLevel->bIsVisible)
	{
		return false;
	}
	for (const FName PlotId : PlotIds)
	{
		const ACityPlotActor* PlotActor = SpawnManager.IsValid()
			? SpawnManager->GetSpawnedPlotById(PlotId) : nullptr;
		if (IsValid(PlotActor) && (!PlotActor->GetLevel()
			|| !PlotActor->GetLevel()->bIsVisible))
		{
			return false;
		}
	}
	if (UWorldPartitionSubsystem* Partition =
		DressingWorld->GetSubsystem<UWorldPartitionSubsystem>())
	{
		if (!Partition->IsAllStreamingCompleted())
		{
			return false;
		}
	}

	for (const ULevelStreaming* StreamingLevel : DressingWorld->GetStreamingLevels())
	{
		if (!StreamingLevel || !StreamingLevel->ShouldBeLoaded())
		{
			continue;
		}

		if (!StreamingLevel->IsLevelLoaded()
			|| (StreamingLevel->ShouldBeVisible() && !StreamingLevel->IsLevelVisible()))
		{
			return false;
		}
	}

	// World Partition의 로드된 셀은 일반 LevelStreaming 목록에 나타나지 않을 수 있다.
	return true;
}

void AVacantPlotDressingManager::BuildPatches(FName OnlyPlotId)
{
	TArray<FCityDressingData> Rows;
	TableManager->GetAllCityDressingRows(Rows);
	TMap<FName, TArray<FCityDressingData>> GroupedRows;
	for (const FCityDressingData& Row : Rows)
	{
		if (Row.PresetId.IsNone())
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[CityDressing] Invalid slot skipped preset=%s slot=%s mesh=%s"),
				*Row.PresetId.ToString(), *Row.SlotId.ToString(), *Row.Mesh.ToString());
			continue;
		}
		GroupedRows.FindOrAdd(Row.PresetId).Add(Row);
	}

	TArray<FRuntimeDressingPreset> Presets;
	for (TPair<FName, TArray<FCityDressingData>>& Pair : GroupedRows)
	{
		FRuntimeDressingPreset Preset;
		Preset.PresetId = Pair.Key;
		Preset.Slots = MoveTemp(Pair.Value);
		Preset.Slots.Sort([](const FCityDressingData& Left, const FCityDressingData& Right)
		{
			return Left.SlotId.ToString().Compare(
				Right.SlotId.ToString(), ESearchCase::CaseSensitive) < 0;
		});

		TArray<FName> SlotIds;
		SlotIds.Reserve(Preset.Slots.Num());
		const FCityDressingData& FirstSlot = Preset.Slots[0];
		Preset.bForAuthoredPlots = FirstSlot.bForAuthoredPlots;
		Preset.bPlaceAlongBoundary = FirstSlot.bPlaceAlongBoundary;
		Preset.BoundarySpacing = FirstSlot.BoundarySpacing;
		Preset.BoundaryInset = FirstSlot.BoundaryInset;
		Preset.PlotCellsPerPatch = FirstSlot.PlotCellsPerPatch;
		Preset.MaxPatchesPerPlot = FirstSlot.MaxPatchesPerPlot;
		bool bMetadataConsistent = FirstSlot.CullDistance >= 0
			&& FirstSlot.PlotCellsPerPatch >= 0 && FirstSlot.MaxPatchesPerPlot > 0;
		bMetadataConsistent &= FMath::IsFinite(FirstSlot.BoundarySpacing)
			&& FirstSlot.BoundarySpacing > 0.f
			&& FMath::IsFinite(FirstSlot.BoundaryInset) && FirstSlot.BoundaryInset >= 0.f
			&& (!FirstSlot.bPlaceAlongBoundary || FirstSlot.bForAuthoredPlots);
		for (const FCityDressingData& Slot : Preset.Slots)
		{
			SlotIds.Add(Slot.SlotId);
			bMetadataConsistent &= Slot.bForAuthoredPlots == FirstSlot.bForAuthoredPlots
				&& Slot.bPlaceAlongBoundary == FirstSlot.bPlaceAlongBoundary
				&& Slot.BoundarySpacing == FirstSlot.BoundarySpacing
				&& Slot.BoundaryInset == FirstSlot.BoundaryInset
				&& Slot.ExpectedSlotCount == FirstSlot.ExpectedSlotCount
				&& Slot.PlotCellsPerPatch == FirstSlot.PlotCellsPerPatch
				&& Slot.MaxPatchesPerPlot == FirstSlot.MaxPatchesPerPlot
				&& Slot.CullDistance == FirstSlot.CullDistance;
		}
		if (!bMetadataConsistent || !FVacantPlotDressingRules::IsValidPresetSlotSet(
			Preset.PresetId, Preset.bForAuthoredPlots, FirstSlot.ExpectedSlotCount, SlotIds))
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[CityDressing] Invalid or inconsistent preset skipped preset=%s slotCount=%d"),
				*Preset.PresetId.ToString(), Preset.Slots.Num());
			continue;
		}

		if (!PreloadPresetMeshes(Preset))
		{
			continue;
		}

		ResolvePresetBounds(Preset);
		if (Preset.BoundsHalfExtent.X > 0.f && Preset.BoundsHalfExtent.Y > 0.f)
		{
			Presets.Add(MoveTemp(Preset));
		}
	}
	Presets.Sort([](const FRuntimeDressingPreset& Left, const FRuntimeDressingPreset& Right)
	{
		return Left.PresetId.ToString().Compare(
			Right.PresetId.ToString(), ESearchCase::CaseSensitive) < 0;
	});

	if (Presets.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CityDressing] No valid DT_CityDressing presets"));
		return;
	}

	ACityPlotActor::PrepareCityGroundQueries(GetWorld());

	TArray<FFixedDressingBlocker> FixedBlockers;
	GatherFixedDressingBlockers(GetWorld(), this, FixedBlockers);
	UE_LOG(LogTemp, Log,
		TEXT("[CityDressing] Fixed instanced blockers sampled=%d"),
		FixedBlockers.Num());
	for (UHierarchicalInstancedStaticMeshComponent* Component : HISMComponents)
	{
		if (Component)
		{
			Component->bAutoRebuildTreeOnInstanceChanges = false;
		}
	}

	for (const FName PlotId : PlotIds)
	{
		if (!OnlyPlotId.IsNone() && PlotId != OnlyPlotId)
		{
			continue;
		}
		ACityPlotActor* PlotActor = SpawnManager->GetSpawnedPlotById(PlotId);
		if (!IsValid(PlotActor))
		{
			continue;
		}

		TArray<int32> BoundaryPresetIndices;
		TArray<int32> InteriorPresetIndices;
		for (int32 PresetIndex = 0; PresetIndex < Presets.Num(); ++PresetIndex)
		{
			if (Presets[PresetIndex].bForAuthoredPlots == PlotActor->IsAuthoredInLevel())
			{
				(Presets[PresetIndex].bPlaceAlongBoundary
					? BoundaryPresetIndices : InteriorPresetIndices).Add(PresetIndex);
			}
		}
		if (BoundaryPresetIndices.IsEmpty() && InteriorPresetIndices.IsEmpty())
		{
			continue;
		}
		TArray<TArray<int32>> EligiblePresetGroups;
		if (!BoundaryPresetIndices.IsEmpty())
		{
			EligiblePresetGroups.Add(MoveTemp(BoundaryPresetIndices));
		}
		if (!InteriorPresetIndices.IsEmpty())
		{
			EligiblePresetGroups.Add(MoveTemp(InteriorPresetIndices));
		}
		const FVector PlotCenter3D = PlotActor->GetCenter();
		const FVector2D PlotCenter(PlotCenter3D.X, PlotCenter3D.Y);
		const FVector2D PlotHalfExtent = PlotActor->GetExtent();
		const FVacantPlotFootprint PlotFootprint(PlotCenter, PlotHalfExtent);
		float PlotSurfaceZ = PlotCenter3D.Z;
		const bool bHasGroundHeight = PlotActor->TryTraceGroundZ(PlotCenter, PlotSurfaceZ);
		TArray<FVacantPlotFootprint>& PlotFixedBlockers =
			FixedBlockersByPlot.FindOrAdd(PlotId);
		PlotFixedBlockers.Reset();
		for (const FFixedDressingBlocker& Blocker : FixedBlockers)
		{
			if ((!bHasGroundHeight && PlotActor->IsAuthoredInLevel()
					|| IsFixedBlockerVerticallyRelevant(Blocker, PlotSurfaceZ))
				&& FVacantPlotDressingRules::Overlaps(
					PlotFootprint, Blocker.Footprint, OverlapSafetyPadding))
			{
				PlotFixedBlockers.Add(Blocker.Footprint);
			}
		}
		PlotFixedBlockers.Sort([](
			const FVacantPlotFootprint& Left,
			const FVacantPlotFootprint& Right)
		{
			if (Left.Center.X != Right.Center.X)
			{
				return Left.Center.X < Right.Center.X;
			}
			if (Left.Center.Y != Right.Center.Y)
			{
				return Left.Center.Y < Right.Center.Y;
			}
			if (Left.HalfExtent.X != Right.HalfExtent.X)
			{
				return Left.HalfExtent.X < Right.HalfExtent.X;
			}
			return Left.HalfExtent.Y < Right.HalfExtent.Y;
		});
		CachedPlotActors.Add(PlotId, PlotActor);
		if (PatchIndicesByPlot.Contains(PlotId))
		{
			continue;
		}

		TArray<FVacantPlotFootprint> BaseOccupancies;
		GatherOccupancies(PlotId, BaseOccupancies, /*bIncludePreview=*/false);
		BaseOccupancies.Append(PlotFixedBlockers);
		TArray<FVacantPlotFootprint> AcceptedFootprints;
		int32 NextPatchOrdinal = 0;
		for (const TArray<int32>& EligiblePresetIndices : EligiblePresetGroups)
		{
			const FRuntimeDressingPreset& DensityPreset = Presets[EligiblePresetIndices[0]];
			int32 DesiredPatchCount = FVacantPlotDressingRules::ResolvePatchCount(
				PlotActor->GetBuildingCapacity(), DensityPreset.PlotCellsPerPatch,
				DensityPreset.MaxPatchesPerPlot);
			TArray<FVector2D> BoundaryCenters;
			if (DensityPreset.bPlaceAlongBoundary)
			{
				TArray<FVacantPlotBoundaryPresetParams> BoundaryPresetParams;
				BoundaryPresetParams.Reserve(EligiblePresetIndices.Num());
				for (const int32 PresetIndex : EligiblePresetIndices)
				{
					const FRuntimeDressingPreset& BoundaryPreset = Presets[PresetIndex];
					BoundaryPresetParams.Add(FVacantPlotBoundaryPresetParams{ BoundaryPreset.BoundsHalfExtent,
						BoundaryPreset.BoundarySpacing, BoundaryPreset.BoundaryInset,
						BoundaryPreset.MaxPatchesPerPlot });
				}
				const FVacantPlotBoundaryPresetParams SharedBoundary =
					FVacantPlotDressingRules::CombineBoundaryPresets(BoundaryPresetParams);
				TArray<FCityPlotBoundarySegment> BoundarySegments;
				if (!PlotActor->GetDressingBoundarySegments(BoundarySegments))
				{
					UE_LOG(LogTemp, Warning, TEXT("[CityDressing] Invalid dressing boundary plot=%s"),
						*PlotId.ToString());
					continue;
				}
				FVacantPlotDressingRules::GenerateBoundaryCenters(
					BoundarySegments, SharedBoundary.HalfExtent,
					SharedBoundary.Spacing, SharedBoundary.Inset,
					FMath::Min(SharedBoundary.MaxPatches,
						FVacantPlotDressingRules::MaxBoundaryTreesPerPlot), BoundaryCenters);
				DesiredPatchCount = BoundaryCenters.Num();
			}
			for (int32 GroupPatchOrdinal = 0; GroupPatchOrdinal < DesiredPatchCount; ++GroupPatchOrdinal)
			{
				const int32 PatchOrdinal = NextPatchOrdinal + GroupPatchOrdinal;
				const int32 EligibleIndex = FVacantPlotDressingRules::SelectPresetIndex(
					PlotId, PatchOrdinal, EligiblePresetIndices.Num());
				const int32 PresetIndex = EligiblePresetIndices.IsValidIndex(EligibleIndex)
					? EligiblePresetIndices[EligibleIndex] : INDEX_NONE;
				if (!Presets.IsValidIndex(PresetIndex))
				{
					break;
				}

				const FRuntimeDressingPreset& Preset = Presets[PresetIndex];
				const int32 QuarterTurns = Preset.bPlaceAlongBoundary ? 0
					: static_cast<int32>(MakeStableSeed(PlotId, PatchOrdinal) % 4u);
				const float PatchYaw = static_cast<float>(QuarterTurns * 90);
				const FVector2D RotatedHalfExtent = FVacantPlotDressingRules::RotateHalfExtent90(
					Preset.BoundsHalfExtent, QuarterTurns);

				TArray<FVector2D> CandidateCenters;
				TArray<int32> RankedCandidateIndices;
				if (Preset.bPlaceAlongBoundary)
				{
					CandidateCenters.Add(BoundaryCenters[GroupPatchOrdinal]);
					RankedCandidateIndices.Add(0);
				}
				else
				{
					TArray<FVacantPlotFootprint> CandidateOccupancies = BaseOccupancies;
					CandidateOccupancies.Append(AcceptedFootprints);
					FVacantPlotDressingRules::GenerateCandidateCenters(
						PlotCenter,
						PlotHalfExtent,
						RotatedHalfExtent,
						CandidateOccupancies,
						OverlapSafetyPadding,
						CandidateCenters);
					FVacantPlotDressingRules::RankFreeCandidateIndices(
						PlotId,
						PatchOrdinal,
						PlotCenter,
						PlotHalfExtent,
						RotatedHalfExtent,
						CandidateOccupancies,
						OverlapSafetyPadding,
						CandidateCenters,
						RankedCandidateIndices);
				}

				FVacantPlotFootprint PatchFootprint(
					Preset.bPlaceAlongBoundary ? BoundaryCenters[GroupPatchOrdinal] : PlotCenter,
					RotatedHalfExtent);
				bool bHasGroundedPlacement = false;
				const int32 GroundTraceCount = FMath::Min(
					RankedCandidateIndices.Num(), MaxGroundTraceCandidateCount);
				for (int32 RankIndex = 0; RankIndex < GroundTraceCount; ++RankIndex)
				{
					const int32 CandidateIndex = RankedCandidateIndices[RankIndex];
					if (!CandidateCenters.IsValidIndex(CandidateIndex))
					{
						continue;
					}

					const FVector2D& CandidateCenter = CandidateCenters[CandidateIndex];
					if (FVacantPlotDressingRules::IsInsidePlot(
						FVacantPlotFootprint(CandidateCenter, RotatedHalfExtent), PlotCenter, PlotHalfExtent)
						&& PlotActor->IsDressingFootprintAllowed(CandidateCenter, RotatedHalfExtent)
						&& PlotActor->IsFootprintOnGround(
						FVector(CandidateCenter.X, CandidateCenter.Y, PlotCenter3D.Z),
						RotatedHalfExtent.X,
						RotatedHalfExtent.Y))
					{
						PatchFootprint.Center = CandidateCenter;
						bHasGroundedPlacement = true;
						break;
					}
				}

				FVacantDressingPatch Patch;
				Patch.PlotId = PlotId;
				Patch.PatchOrdinal = PatchOrdinal;
				Patch.bFixedAnchor = Preset.bPlaceAlongBoundary;
				Patch.Footprint = PatchFootprint;
				Patch.bHasResolvedPlacement = bHasGroundedPlacement;
				const FVector2D RotatedBoundsCenter = RotateVector2D(
					Preset.BoundsCenter, PatchYaw);
				const FVector PatchPivot(
					PatchFootprint.Center.X - RotatedBoundsCenter.X,
					PatchFootprint.Center.Y - RotatedBoundsCenter.Y,
					PlotCenter3D.Z);
				const FQuat PatchRotation = FRotator(0.f, PatchYaw, 0.f).Quaternion();

				bool bPatchComplete = true;
				bool bAllSlotGrounded = true;
				for (int32 SlotIndex = 0; SlotIndex < Preset.Slots.Num(); ++SlotIndex)
				{
					const FCityDressingData& Slot = Preset.Slots[SlotIndex];
					UStaticMesh* Mesh = Preset.LoadedMeshes[SlotIndex];

					UHierarchicalInstancedStaticMeshComponent* HISM = GetOrCreateHISM(
						Mesh, Slot.bCastShadow, Slot.CullDistance);
					if (!HISM)
					{
						bPatchComplete = false;
						break;
					}

					const float StableScale = ResolveStableScale(Slot, PlotId, PatchOrdinal);
					const FVector LocalLocation = Slot.LocalLocation;
					const FVector RotatedLocation = PatchRotation.RotateVector(LocalLocation);
					FVector WorldLocation = PatchPivot + RotatedLocation;
					float SurfaceZ = PlotCenter3D.Z;
					const bool bGroundHit = PlotActor->TryTraceGroundZ(
						FVector2D(WorldLocation.X, WorldLocation.Y), SurfaceZ);
					bAllSlotGrounded &= bGroundHit || !PlotActor->IsAuthoredInLevel();
					const float GroundOffsetZ = Slot.LocalLocation.Z + 5.f;
					WorldLocation.Z = SurfaceZ + GroundOffsetZ;
					const FQuat WorldRotation = PatchRotation * Slot.LocalRotation.Quaternion();
					const FVector WorldScale = Slot.LocalScale * StableScale;
					const FTransform VisibleTransform(WorldRotation, WorldLocation, WorldScale);
					FTransform HiddenTransform = VisibleTransform;
					HiddenTransform.SetScale3D(FVector::ZeroVector);
					const int32 InstanceIndex = HISM->AddInstance(HiddenTransform, /*bWorldSpace=*/true);
					if (InstanceIndex == INDEX_NONE)
					{
						UE_LOG(LogTemp, Warning,
							TEXT("[CityDressing] AddInstance failed preset=%s slot=%s"),
							*Preset.PresetId.ToString(), *Slot.SlotId.ToString());
						bPatchComplete = false;
						break;
					}

					FVacantDressingInstanceRef& InstanceRef = Patch.Instances.AddDefaulted_GetRef();
					InstanceRef.Component = HISM;
					InstanceRef.InstanceIndex = InstanceIndex;
					InstanceRef.VisibleTransform = VisibleTransform;
					InstanceRef.GroundOffsetZ = GroundOffsetZ;
				}

				if (!bPatchComplete)
				{
					RollbackCandidateTailInstances(Patch);
					continue;
				}
				Patch.bHasResolvedPlacement = bHasGroundedPlacement && bAllSlotGrounded;
				const bool bResolvedPlacement = Patch.bHasResolvedPlacement;
				const int32 NewPatchIndex = Patches.Add(MoveTemp(Patch));
				PatchIndicesByPlot.Add(PlotId, NewPatchIndex);
				if (bResolvedPlacement)
				{
					AcceptedFootprints.Add(PatchFootprint);
				}
			}
			NextPatchOrdinal += DesiredPatchCount;
		}
	}
	for (UHierarchicalInstancedStaticMeshComponent* Component : HISMComponents)
	{
		if (Component)
		{
			Component->bAutoRebuildTreeOnInstanceChanges = true;
			Component->BuildTreeIfOutdated(/*Async=*/false, /*ForceUpdate=*/false);
		}
	}
}

void AVacantPlotDressingManager::RebuildPatchesForActorReplacement()
{
	SetActorTickEnabled(false);
	for (UHierarchicalInstancedStaticMeshComponent* Component : HISMComponents)
	{
		if (IsValid(Component))
		{
			Component->bAutoRebuildTreeOnInstanceChanges = false;
			Component->ClearInstances();
		}
	}
	Patches.Reset();
	PatchIndicesByPlot.Reset();
	FixedBlockersByPlot.Reset();
	CachedPlotActors.Reset();
	if (ACityPlotWalkwayManager* WalkwayManager = SpawnManager.IsValid()
		? SpawnManager->GetPlotWalkwayManager() : nullptr)
	{
		WalkwayManager->RefreshAllPlots();
	}
	BuildPatches();
	RefreshAllPlotsInternal(/*bInstant=*/true);
}

void AVacantPlotDressingManager::BindRuntimeEvents()
{
	if (EntityManager.IsValid() && !BuildingCountChangedHandle.IsValid())
	{
		BuildingCountChangedHandle = EntityManager->OnBuildingCountChanged.AddUObject(
			this, &AVacantPlotDressingManager::HandleBuildingCountChanged);
	}
	if (AcquisitionManager.IsValid() && !CompanyVisualClearedHandle.IsValid())
	{
		CompanyVisualClearedHandle = AcquisitionManager->OnCompanyVisualCleared.AddUObject(
			this, &AVacantPlotDressingManager::HandleCompanyVisualCleared);
	}
}

void AVacantPlotDressingManager::UpdateBuildingPreview(
	FName PlotId,
	const FVector2D& Center,
	const FVector2D& HalfExtent)
{
	if (PlotId.IsNone() || HalfExtent.X <= 0.f || HalfExtent.Y <= 0.f)
	{
		ClearBuildingPreview();
		return;
	}
	if (bHasPreview && PreviewPlotId == PlotId
		&& PreviewFootprint.Center.Equals(Center, 0.5f)
		&& PreviewFootprint.HalfExtent.Equals(HalfExtent, 0.1f))
	{
		return;
	}

	const FName PreviousPlotId = bHasPreview ? PreviewPlotId : NAME_None;
	PreviewPlotId = PlotId;
	PreviewFootprint = FVacantPlotFootprint(Center, HalfExtent);
	bHasPreview = true;
	RefreshPreviewVisibilityInternal(PreviousPlotId, /*bInstant=*/false);
	if (PreviewPlotId != PreviousPlotId)
	{
		RefreshPreviewVisibilityInternal(PreviewPlotId, /*bInstant=*/false);
	}
}

void AVacantPlotDressingManager::ClearBuildingPreview()
{
	if (!bHasPreview)
	{
		return;
	}
	const FName PreviousPlotId = PreviewPlotId;
	bHasPreview = false;
	PreviewPlotId = NAME_None;
	PreviewFootprint = FVacantPlotFootprint();
	RefreshPreviewVisibilityInternal(PreviousPlotId, /*bInstant=*/false);
}

void AVacantPlotDressingManager::RefreshPreviewVisibilityPlots(
	FName FirstPlotId,
	FName SecondPlotId)
{
	if (!FirstPlotId.IsNone())
	{
		RefreshPreviewVisibilityInternal(FirstPlotId, /*bInstant=*/false);
	}
	if (!SecondPlotId.IsNone() && SecondPlotId != FirstPlotId)
	{
		RefreshPreviewVisibilityInternal(SecondPlotId, /*bInstant=*/false);
	}
}

void AVacantPlotDressingManager::RefreshPlot(FName PlotId)
{
	if (ACityPlotWalkwayManager* WalkwayManager = SpawnManager.IsValid()
		? SpawnManager->GetPlotWalkwayManager() : nullptr)
	{
		WalkwayManager->RefreshPlot(PlotId);
	}
	RefreshPlotInternal(PlotId, /*bInstant=*/false);
}

void AVacantPlotDressingManager::RefreshPlots(FName FirstPlotId, FName SecondPlotId)
{
	if (!FirstPlotId.IsNone())
	{
		RefreshPlot(FirstPlotId);
	}
	if (!SecondPlotId.IsNone() && SecondPlotId != FirstPlotId)
	{
		RefreshPlot(SecondPlotId);
	}
}

void AVacantPlotDressingManager::RefreshAllPlots()
{
	if (ACityPlotWalkwayManager* WalkwayManager = SpawnManager.IsValid()
		? SpawnManager->GetPlotWalkwayManager() : nullptr)
	{
		WalkwayManager->RefreshAllPlots();
	}
	RefreshAllPlotsInternal(/*bInstant=*/false);
}

void AVacantPlotDressingManager::RefreshAllPlotsInternal(bool bInstant)
{
	if (!bInitialized)
	{
		return;
	}
	for (const FName PlotId : PlotIds)
	{
		RefreshPlotInternal(PlotId, bInstant);
	}
	if (bInstant)
	{
		MarkAllHISMRenderStatesDirty();
		for (UHierarchicalInstancedStaticMeshComponent* Component : HISMComponents)
		{
			if (Component)
			{
				Component->BuildTreeIfOutdated(/*Async=*/false, /*ForceUpdate=*/true);
			}
		}
	}
}

void AVacantPlotDressingManager::RefreshPreviewVisibilityInternal(
	FName PlotId,
	bool bInstant)
{
	if (!bInitialized || PlotId.IsNone())
	{
		return;
	}

	TArray<int32> PatchIndices;
	PatchIndicesByPlot.MultiFind(PlotId, PatchIndices);
	PatchIndices.Sort();
	if (PatchIndices.IsEmpty())
	{
		return;
	}

	ACityPlotActor* PlotActor = SpawnManager.IsValid()
		? SpawnManager->GetSpawnedPlotById(PlotId)
		: nullptr;
	TArray<FVacantPlotFootprint> VisibilityOccupancies;
	GatherOccupancies(PlotId, VisibilityOccupancies, /*bIncludePreview=*/true);
	if (const TArray<FVacantPlotFootprint>* FixedBlockers = FixedBlockersByPlot.Find(PlotId))
	{
		VisibilityOccupancies.Append(*FixedBlockers);
	}

	for (const int32 PatchIndex : PatchIndices)
	{
		if (!Patches.IsValidIndex(PatchIndex))
		{
			continue;
		}

		FVacantDressingPatch& Patch = Patches[PatchIndex];
		if (Patch.bHasPendingRelocation)
		{
			SetPatchVisibilityTarget(PatchIndex, false, bInstant);
			continue;
		}

		const bool bCurrentPlacementValid = Patch.bHasResolvedPlacement && IsValid(PlotActor)
			&& FVacantPlotDressingRules::IsInsidePlot(
				Patch.Footprint,
				FVector2D(PlotActor->GetCenter().X, PlotActor->GetCenter().Y),
				PlotActor->GetExtent())
			&& PlotActor->IsDressingFootprintAllowed(
				Patch.Footprint.Center, Patch.Footprint.HalfExtent)
			&& ShouldPatchBeVisible(
				Patch.Footprint,
				VisibilityOccupancies,
				OverlapSafetyPadding);
		const FVacantDressingPlacementDecision Decision =
			FVacantPlotDressingManagerStateRules::ResolvePlacement(
				Patch.Footprint,
				bCurrentPlacementValid,
				EVacantPlotDressingRefreshMode::Preview,
				TConstArrayView<FVacantPlotFootprint>());
		SetPatchVisibilityTarget(PatchIndex, Decision.bTargetVisible, bInstant);
	}
}

void AVacantPlotDressingManager::RefreshPlotInternal(FName PlotId, bool bInstant)
{
	if (!bInitialized || PlotId.IsNone())
	{
		return;
	}
	ACityPlotActor* PlotActor = SpawnManager.IsValid()
		? SpawnManager->GetSpawnedPlotById(PlotId)
		: nullptr;
	TArray<int32> PatchIndices;
	PatchIndicesByPlot.MultiFind(PlotId, PatchIndices);
	PatchIndices.Sort();
	if (!IsValid(PlotActor))
	{
		for (const int32 PatchIndex : PatchIndices)
		{
			SetPatchVisibilityTarget(PatchIndex, false, bInstant);
		}
		return;
	}
	PlotIds.AddUnique(PlotId);
	if (!IsWorldGeometryReady() || !IsCityMappingReady())
	{
		PendingPlotIds.Add(PlotId);
		for (const int32 PatchIndex : PatchIndices)
		{
			SetPatchVisibilityTarget(PatchIndex, false, bInstant);
		}
		if (UWorld* DressingWorld = GetWorld())
		{
			if (!DressingWorld->GetTimerManager().IsTimerActive(PendingPlotRetryTimer))
			{
				DressingWorld->GetTimerManager().SetTimer(
					PendingPlotRetryTimer, this,
					&AVacantPlotDressingManager::RetryPendingPlots, 0.5f, false);
			}
		}
		return;
	}
	PendingPlotIds.Remove(PlotId);
	if (const TWeakObjectPtr<ACityPlotActor>* CachedActor = CachedPlotActors.Find(PlotId))
	{
		if (CachedActor->Get() != PlotActor)
		{
			RebuildPatchesForActorReplacement();
			return;
		}
	}
	if (!PatchIndicesByPlot.Contains(PlotId)
		|| CachedPlotActors.FindRef(PlotId).Get() != PlotActor)
	{
		BuildPatches(PlotId);
		PatchIndices.Reset();
		PatchIndicesByPlot.MultiFind(PlotId, PatchIndices);
		PatchIndices.Sort();
	}
	if (PatchIndices.IsEmpty())
	{
		return;
	}

	TArray<FVacantPlotFootprint> CommittedOccupancies;
	GatherOccupancies(PlotId, CommittedOccupancies, /*bIncludePreview=*/false);
	if (const TArray<FVacantPlotFootprint>* FixedBlockers = FixedBlockersByPlot.Find(PlotId))
	{
		CommittedOccupancies.Append(*FixedBlockers);
	}
	TArray<FVacantPlotFootprint> ReservedFootprints;
	TSet<int32> ValidPatchIndices;
	const FVector PlotCenter3D = PlotActor->GetCenter();
	const FVector2D PlotCenter(PlotCenter3D.X, PlotCenter3D.Y);
	for (const int32 PatchIndex : PatchIndices)
	{
		if (!Patches.IsValidIndex(PatchIndex))
		{
			continue;
		}
		const FVacantPlotFootprint& Footprint = Patches[PatchIndex].Footprint;
		if (Patches[PatchIndex].bHasResolvedPlacement
			&& FVacantPlotDressingRules::IsInsidePlot(Footprint, PlotCenter, PlotActor->GetExtent())
			&& !FVacantPlotDressingRules::OverlapsAny(
				Footprint, CommittedOccupancies, OverlapSafetyPadding)
			&& !FVacantPlotDressingRules::IsBlockedByReservedPatch(
				Footprint, Patches[PatchIndex].bFixedAnchor,
				ReservedFootprints, OverlapSafetyPadding)
			&& PlotActor->IsDressingFootprintAllowed(
				Footprint.Center, Footprint.HalfExtent)
			&& (Patches[PatchIndex].bFixedAnchor || PlotActor->IsFootprintOnGround(
				FVector(Footprint.Center.X, Footprint.Center.Y, PlotCenter3D.Z),
				Footprint.HalfExtent.X, Footprint.HalfExtent.Y)))
		{
			ValidPatchIndices.Add(PatchIndex);
			ReservedFootprints.Add(Footprint);
		}
	}
	for (const int32 PatchIndex : PatchIndices)
	{
		if (!Patches.IsValidIndex(PatchIndex))
		{
			continue;
		}

		FVacantDressingPatch& Patch = Patches[PatchIndex];
		const bool bCurrentPlacementValid = ValidPatchIndices.Contains(PatchIndex);
		if (Patch.bFixedAnchor)
		{
			Patch.bHasPendingRelocation = false;
			Patch.PendingFootprint = FVacantPlotFootprint();
			Patch.bVisibleAfterRelocation = false;
			const bool bPreviewAllowsVisibility = !bHasPreview
				|| PreviewPlotId != PlotId
				|| !FVacantPlotDressingRules::Overlaps(
					Patch.Footprint, PreviewFootprint, OverlapSafetyPadding);
			const FVacantDressingPlacementDecision Decision =
				FVacantPlotDressingManagerStateRules::ResolveFixedAnchor(
					Patch.Footprint, bCurrentPlacementValid && bPreviewAllowsVisibility);
			SetPatchVisibilityTarget(PatchIndex, Decision.bTargetVisible, bInstant);
			continue;
		}
		FVacantPlotFootprint ResolvedFootprint = Patch.Footprint;
		bool bHasResolvedPlacement = bCurrentPlacementValid;
		if (!bCurrentPlacementValid)
		{
			TArray<FVacantPlotFootprint> EvaluationOccupancies = CommittedOccupancies;
			EvaluationOccupancies.Append(ReservedFootprints);
			bHasResolvedPlacement = TryResolveCommittedPatchPlacement(
				PatchIndex, PlotActor, EvaluationOccupancies, ResolvedFootprint);
		}
		TArray<FVacantPlotFootprint> RankedCandidates;
		if (bHasResolvedPlacement && !bCurrentPlacementValid)
		{
			RankedCandidates.Add(ResolvedFootprint);
		}

		const FVacantDressingPlacementDecision Decision =
			FVacantPlotDressingManagerStateRules::ResolvePlacement(
				Patch.Footprint,
				bCurrentPlacementValid,
				EVacantPlotDressingRefreshMode::Committed,
				RankedCandidates);
		const bool bPreviewAllowsVisibility = !bHasPreview
			|| PreviewPlotId != PlotId
			|| !FVacantPlotDressingRules::Overlaps(
				Decision.ResolvedFootprint,
				PreviewFootprint,
				OverlapSafetyPadding);

		if (Decision.bQueueRelocation)
		{
			QueuePatchRelocation(
				PatchIndex,
				Decision.ResolvedFootprint,
				bPreviewAllowsVisibility);
			if (bInstant && !Patch.bHasPendingRelocation)
			{
				SetPatchVisibilityTarget(
					PatchIndex, bPreviewAllowsVisibility && Patch.bHasResolvedPlacement,
					/*bInstant=*/true);
			}
			ReservedFootprints.Add(Decision.ResolvedFootprint);
			continue;
		}

		Patch.bHasPendingRelocation = false;
		Patch.PendingFootprint = FVacantPlotFootprint();
		Patch.bVisibleAfterRelocation = false;
		const bool bVisible = Decision.bTargetVisible && bPreviewAllowsVisibility;
		SetPatchVisibilityTarget(PatchIndex, bVisible, bInstant);
	}
}

bool AVacantPlotDressingManager::TryResolveCommittedPatchPlacement(
	int32 PatchIndex,
	ACityPlotActor* PlotActor,
	TConstArrayView<FVacantPlotFootprint> Occupancies,
	FVacantPlotFootprint& OutFootprint) const
{
	if (!Patches.IsValidIndex(PatchIndex) || !IsValid(PlotActor))
	{
		return false;
	}

	const FVacantDressingPatch& Patch = Patches[PatchIndex];
	OutFootprint = Patch.Footprint;
	const FVector PlotCenter3D = PlotActor->GetCenter();
	const FVector2D PlotCenter(PlotCenter3D.X, PlotCenter3D.Y);
	const FVector2D PlotHalfExtent = PlotActor->GetExtent();
	const bool bCurrentPlacementValid = Patch.bHasResolvedPlacement
		&& FVacantPlotDressingRules::IsInsidePlot(
		Patch.Footprint, PlotCenter, PlotHalfExtent)
		&& !FVacantPlotDressingRules::OverlapsAny(
			Patch.Footprint, Occupancies, OverlapSafetyPadding)
		&& PlotActor->IsDressingFootprintAllowed(
			Patch.Footprint.Center, Patch.Footprint.HalfExtent)
		&& PlotActor->IsFootprintOnGround(
			FVector(Patch.Footprint.Center.X, Patch.Footprint.Center.Y, PlotCenter3D.Z),
			Patch.Footprint.HalfExtent.X,
			Patch.Footprint.HalfExtent.Y);
	if (bCurrentPlacementValid)
	{
		return true;
	}

	TArray<FVector2D> CandidateCenters;
	FVacantPlotDressingRules::GenerateCandidateCenters(
		PlotCenter,
		PlotHalfExtent,
		Patch.Footprint.HalfExtent,
		Occupancies,
		OverlapSafetyPadding,
		CandidateCenters);
	TArray<int32> RankedCandidateIndices;
	FVacantPlotDressingRules::RankFreeCandidateIndices(
		Patch.PlotId,
		Patch.PatchOrdinal,
		PlotCenter,
		PlotHalfExtent,
		Patch.Footprint.HalfExtent,
		Occupancies,
		OverlapSafetyPadding,
		CandidateCenters,
		RankedCandidateIndices);

	const int32 GroundTraceCount = FMath::Min(
		RankedCandidateIndices.Num(), MaxGroundTraceCandidateCount);
	for (int32 RankIndex = 0; RankIndex < GroundTraceCount; ++RankIndex)
	{
		const int32 CandidateIndex = RankedCandidateIndices[RankIndex];
		if (!CandidateCenters.IsValidIndex(CandidateIndex))
		{
			continue;
		}

		const FVector2D& CandidateCenter = CandidateCenters[CandidateIndex];
		if (PlotActor->IsDressingFootprintAllowed(
			CandidateCenter, Patch.Footprint.HalfExtent)
			&& PlotActor->IsFootprintOnGround(
			FVector(CandidateCenter.X, CandidateCenter.Y, PlotCenter3D.Z),
			Patch.Footprint.HalfExtent.X,
			Patch.Footprint.HalfExtent.Y))
		{
			OutFootprint = FVacantPlotFootprint(
				CandidateCenter, Patch.Footprint.HalfExtent);
			return true;
		}
	}

	return false;
}

void AVacantPlotDressingManager::QueuePatchRelocation(
	int32 PatchIndex,
	const FVacantPlotFootprint& DestinationFootprint,
	bool bVisibleAfterRelocation)
{
	if (!Patches.IsValidIndex(PatchIndex))
	{
		return;
	}

	FVacantDressingPatch& Patch = Patches[PatchIndex];
	Patch.PendingFootprint = DestinationFootprint;
	Patch.bHasPendingRelocation = true;
	Patch.bVisibleAfterRelocation = bVisibleAfterRelocation;
	SetPatchVisibilityTarget(PatchIndex, false, /*bInstant=*/false);
	if (FMath::IsNearlyZero(Patch.VisibilityAlpha, KINDA_SMALL_NUMBER))
	{
		CommitPatchRelocation(PatchIndex);
	}
}

void AVacantPlotDressingManager::CommitPatchRelocation(int32 PatchIndex)
{
	if (!Patches.IsValidIndex(PatchIndex)
		|| !Patches[PatchIndex].bHasPendingRelocation)
	{
		return;
	}

	FVacantDressingPatch& Patch = Patches[PatchIndex];
	const FVacantPlotFootprint DestinationFootprint = Patch.PendingFootprint;
	const bool bVisibleAfterRelocation = Patch.bVisibleAfterRelocation;
	FVacantPlotDressingManagerStateRules::ApplyRelocationDelta(
		Patch, DestinationFootprint);
	const ACityPlotActor* PlotActor = SpawnManager.IsValid()
		? SpawnManager->GetSpawnedPlotById(Patch.PlotId) : nullptr;
	bool bAllSlotGrounded = IsValid(PlotActor);
	for (FVacantDressingInstanceRef& InstanceRef : Patch.Instances)
	{
		FVector WorldLocation = InstanceRef.VisibleTransform.GetLocation();
		float SurfaceZ = IsValid(PlotActor) ? PlotActor->GetCenter().Z : WorldLocation.Z - InstanceRef.GroundOffsetZ;
		const bool bGroundHit = IsValid(PlotActor) && PlotActor->TryTraceGroundZ(
			FVector2D(WorldLocation.X, WorldLocation.Y), SurfaceZ);
		bAllSlotGrounded &= bGroundHit || (IsValid(PlotActor) && !PlotActor->IsAuthoredInLevel());
		WorldLocation.Z = SurfaceZ + InstanceRef.GroundOffsetZ;
		InstanceRef.VisibleTransform.SetLocation(WorldLocation);
	}
	Patch.bHasResolvedPlacement = bAllSlotGrounded;
	Patch.PendingFootprint = FVacantPlotFootprint();
	Patch.bHasPendingRelocation = false;
	Patch.bVisibleAfterRelocation = false;
	ApplyPatchTransform(PatchIndex);
	SetPatchVisibilityTarget(
		PatchIndex, bVisibleAfterRelocation && bAllSlotGrounded, /*bInstant=*/false);
	RefreshPreviewVisibilityInternal(Patch.PlotId, /*bInstant=*/false);
	MarkAllHISMRenderStatesDirty();
}

void AVacantPlotDressingManager::GatherOccupancies(
	FName PlotId,
	TArray<FVacantPlotFootprint>& OutOccupancies,
	bool bIncludePreview) const
{
	OutOccupancies.Reset();
	if (!SpawnManager.IsValid() || !TableManager.IsValid() || !EntityManager.IsValid())
	{
		return;
	}

	ACityPlotActor* PlotActor = SpawnManager->GetSpawnedPlotById(PlotId);
	if (!IsValid(PlotActor))
	{
		return;
	}
	const FVector PlotCenter3D = PlotActor->GetCenter();
	const FVector2D PlotCenter(PlotCenter3D.X, PlotCenter3D.Y);
	const FVector2D PlotHalfExtent = PlotActor->GetExtent();

	for (const ABuildingBaseActor* Building : EntityManager->GetBuildingsOnPlot(PlotId))
	{
		if (IsValid(Building) && !Building->IsHidden() && !Building->IsActorBeingDestroyed())
		{
			AddPlayerBuildingFootprint(Building, PlotCenter, PlotHalfExtent, OutOccupancies);
		}
	}
	for (TActorIterator<ABrickFactory> FactoryIt(GetWorld()); FactoryIt; ++FactoryIt)
	{
		const ABrickFactory* Factory = *FactoryIt;
		if (!IsValid(Factory) || Factory->IsActorBeingDestroyed()) continue;
		const FVector Center = Factory->GetPlacementFootprintCenter(true);
		const FVector2D HalfExtent = Factory->GetPlacementFootprintHalfExtent(true, true);
		OutOccupancies.Emplace(FVector2D(Center.X, Center.Y), HalfExtent);
	}

	if (!AcquisitionManager.IsValid())
	{
		OutOccupancies.Emplace(PlotCenter, PlotHalfExtent);
	}
	else
	{
		// 저작 건물의 실제 위치가 DT 부지 표기와 다르거나 이웃 부지에 걸쳐 있어도 겹침을 피한다.
		if (const UCityCompanyDirector* CityDirector = GetWorld()->GetSubsystem<UCityCompanyDirector>())
		{
			const FVacantPlotFootprint PlotFootprint(PlotCenter, PlotHalfExtent);
			for (const FCitySkylineEntry& SkylineEntry : CityDirector->GetSkylineEntries())
			{
				const AActor* CompanyActor = SkylineEntry.Building.Get();
				if (!IsValid(CompanyActor) || CompanyActor->IsHidden() || CompanyActor->IsActorBeingDestroyed())
				{
					continue;
				}
				FVector BoundsOrigin;
				FVector BoundsExtent;
				CompanyActor->GetActorBounds(false, BoundsOrigin, BoundsExtent);
				const FVacantPlotFootprint CompanyFootprint(
					FVector2D(BoundsOrigin.X, BoundsOrigin.Y), FVector2D(BoundsExtent.X, BoundsExtent.Y));
				if (FVacantPlotDressingRules::Overlaps(PlotFootprint, CompanyFootprint, OverlapSafetyPadding))
				{
					OutOccupancies.Add(CompanyFootprint);
				}
			}
		}
		TArray<int32> CompanyKeys;
		AcquisitionManager->GetOccupantKeysForPlot(PlotId, CompanyKeys);
		for (const int32 CompanyKey : CompanyKeys)
		{
			AActor* CompanyActor = AcquisitionManager->GetCompanyActor(CompanyKey);
			if ((!IsValid(CompanyActor) || CompanyActor->IsHidden() || CompanyActor->IsActorBeingDestroyed())
				&& AcquisitionManager->GetState(CompanyKey) != EAcqState::Cleared)
			{
				// 매핑되지 않은 미철거 회사는 부지 전체를 점유한 것으로 처리해 겹침을 fail-closed로 막는다.
				OutOccupancies.Emplace(PlotCenter, PlotHalfExtent);
			}
		}
	}

	if (bIncludePreview && bHasPreview && PreviewPlotId == PlotId)
	{
		OutOccupancies.Add(PreviewFootprint);
	}
	if (const ACityPlotWalkwayManager* WalkwayManager = SpawnManager->GetPlotWalkwayManager())
	{
		WalkwayManager->AppendReservedFootprints(PlotId, OutOccupancies, bIncludePreview);
	}
}

void AVacantPlotDressingManager::AddPlayerBuildingFootprint(
	const ABuildingBaseActor* Building,
	const FVector2D& PlotCenter,
	const FVector2D& PlotHalfExtent,
	TArray<FVacantPlotFootprint>& OutOccupancies) const
{
	if (!Building || !TableManager.IsValid())
	{
		OutOccupancies.Emplace(PlotCenter, PlotHalfExtent);
		return;
	}

	bool bBuildingDataFound = false;
	TableManager->GetBuildingData(Building->GetBuildingID(), bBuildingDataFound);
	if (!bBuildingDataFound)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[CityDressing] Missing building footprint row for %s; plot hidden fail-closed"),
			*Building->GetBuildingID().ToString());
		OutOccupancies.Emplace(PlotCenter, PlotHalfExtent);
		return;
	}

	const FVector2D WorldHalfExtent = Building->GetPlacementFootprintHalfExtent();
	const FVector BuildingLocation = Building->GetPlacementFootprintCenter();
	OutOccupancies.Emplace(
		FVector2D(BuildingLocation.X, BuildingLocation.Y), WorldHalfExtent);
}

void AVacantPlotDressingManager::SetPatchVisibilityTarget(
	int32 PatchIndex,
	bool bVisible,
	bool bInstant)
{
	if (!Patches.IsValidIndex(PatchIndex))
	{
		return;
	}

	FVacantDressingPatch& Patch = Patches[PatchIndex];
	Patch.bTargetVisible = bVisible;
	if (bInstant)
	{
		Patch.VisibilityAlpha = bVisible ? 1.f : 0.f;
		ApplyPatchTransform(PatchIndex);
		return;
	}

	const float TargetAlpha = bVisible ? 1.f : 0.f;
	if (!FMath::IsNearlyEqual(Patch.VisibilityAlpha, TargetAlpha, KINDA_SMALL_NUMBER))
	{
		SetActorTickEnabled(true);
	}
}

void AVacantPlotDressingManager::ApplyPatchTransform(int32 PatchIndex)
{
	if (!Patches.IsValidIndex(PatchIndex))
	{
		return;
	}

	const FVacantDressingPatch& Patch = Patches[PatchIndex];
	const float ClampedAlpha = FMath::Clamp(Patch.VisibilityAlpha, 0.f, 1.f);
	for (const FVacantDressingInstanceRef& InstanceRef : Patch.Instances)
	{
		UHierarchicalInstancedStaticMeshComponent* Component = InstanceRef.Component.Get();
		if (!Component || InstanceRef.InstanceIndex == INDEX_NONE)
		{
			continue;
		}

		FTransform UpdatedTransform = InstanceRef.VisibleTransform;
		UpdatedTransform.SetScale3D(
			InstanceRef.VisibleTransform.GetScale3D() * ClampedAlpha);
		Component->UpdateInstanceTransform(
			InstanceRef.InstanceIndex,
			UpdatedTransform,
			/*bWorldSpace=*/true,
			/*bMarkRenderStateDirty=*/false,
			/*bTeleport=*/true);
	}
}

void AVacantPlotDressingManager::MarkAllHISMRenderStatesDirty()
{
	for (UHierarchicalInstancedStaticMeshComponent* Component : HISMComponents)
	{
		if (Component)
		{
			Component->MarkRenderStateDirty();
		}
	}
}

UHierarchicalInstancedStaticMeshComponent* AVacantPlotDressingManager::GetOrCreateHISM(
	UStaticMesh* Mesh,
	bool bCastShadow,
	int32 CullDistance)
{
	if (!Mesh)
	{
		return nullptr;
	}
	if (UHierarchicalInstancedStaticMeshComponent** ExistingComponent = MeshComponents.Find(Mesh))
	{
		if (bCastShadow && *ExistingComponent)
		{
			(*ExistingComponent)->SetCastShadow(true);
		}
		return *ExistingComponent;
	}

	const FName ComponentName = MakeUniqueObjectName(
		this,
		UHierarchicalInstancedStaticMeshComponent::StaticClass(),
		FName(*FString::Printf(TEXT("HISM_%s"), *Mesh->GetName())));
	UHierarchicalInstancedStaticMeshComponent* NewComponent =
		NewObject<UHierarchicalInstancedStaticMeshComponent>(this, ComponentName);
	if (!NewComponent)
	{
		return nullptr;
	}

	NewComponent->SetupAttachment(SceneRoot);
	NewComponent->SetStaticMesh(Mesh);
	NewComponent->SetMobility(EComponentMobility::Movable);
	NewComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	NewComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	NewComponent->SetGenerateOverlapEvents(false);
	NewComponent->SetCanEverAffectNavigation(false);
	NewComponent->SetCastShadow(bCastShadow);
	NewComponent->SetCullDistances(0, CullDistance);
	NewComponent->bAutoRebuildTreeOnInstanceChanges = false;
	NewComponent->SetReceivesDecals(false);
	NewComponent->bCastDynamicShadow = bCastShadow;
	NewComponent->bCastStaticShadow = false;
	NewComponent->bAffectDistanceFieldLighting = false;
	NewComponent->bAffectDynamicIndirectLighting = false;
	NewComponent->bUseAsOccluder = false;
	NewComponent->PrimaryComponentTick.bCanEverTick = false;
	NewComponent->SetComponentTickEnabled(false);
	AddInstanceComponent(NewComponent);
	NewComponent->RegisterComponent();

	HISMComponents.Add(NewComponent);
	MeshComponents.Add(Mesh, NewComponent);
	return NewComponent;
}

void AVacantPlotDressingManager::HandleBuildingCountChanged()
{
	RefreshAllPlots();
}

void AVacantPlotDressingManager::HandleCompanyVisualCleared(int32 CompanyKey)
{
	// 철거된 건물이 실제로 닿았던 부지는 DT 표기와 다를 수 있다.
	RefreshAllPlots();
}

int32 AVacantPlotDressingManager::GetVisiblePatchCount() const
{
	int32 VisibleCount = 0;
	for (const FVacantDressingPatch& Patch : Patches)
	{
		if (Patch.bTargetVisible)
		{
			++VisibleCount;
		}
	}
	return VisibleCount;
}

bool AVacantPlotDressingManager::AreDressingComponentsNavigationDisabled() const
{
	if (HISMComponents.IsEmpty())
	{
		return false;
	}

	for (const UHierarchicalInstancedStaticMeshComponent* Component : HISMComponents)
	{
		if (!IsValid(Component) || Component->CanEverAffectNavigation())
		{
			return false;
		}
	}
	return true;
}
