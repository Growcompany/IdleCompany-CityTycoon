#include "Entity/Ambient/VacantPlotDressingManager.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Core/CGGameInstance.h"
#include "Engine/GameInstance.h"
#include "Engine/LevelStreaming.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Entity/Building/BuildingBaseActor.h"
#include "Entity/Plot/CityPlotActor.h"
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

namespace
{
	struct FRuntimeDressingPreset
	{
		FName PresetId = NAME_None;
		TArray<FCityDressingData> Slots;
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
	constexpr float FixedBlockerPadding = 150.f;

	const FVector2D CandidateDirections[] = {
		FVector2D(-1.f, -1.f),
		FVector2D(1.f, 1.f),
		FVector2D(-1.f, 1.f),
		FVector2D(1.f, -1.f),
		FVector2D(-1.f, 0.f),
		FVector2D(1.f, 0.f),
		FVector2D(0.f, -1.f),
		FVector2D(0.f, 1.f)
	};

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
		for (const FCityDressingData& Slot : Preset.Slots)
		{
			const float MaxVariation = FMath::Max(
				0.01f, FMath::Max(Slot.ScaleVariationMin, Slot.ScaleVariationMax));
			const FVector2D ScaledHalfExtent(
				FMath::Abs(Slot.HalfExtent.X * Slot.LocalScale.X) * MaxVariation,
				FMath::Abs(Slot.HalfExtent.Y * Slot.LocalScale.Y) * MaxVariation);
			const FVector2D RotatedHalfExtent = RotateAabbHalfExtent(
				ScaledHalfExtent, Slot.LocalRotation.Yaw);
			const FVector2D SlotCenter(Slot.LocalLocation.X, Slot.LocalLocation.Y);
			Minimum.X = FMath::Min(Minimum.X, SlotCenter.X - RotatedHalfExtent.X);
			Minimum.Y = FMath::Min(Minimum.Y, SlotCenter.Y - RotatedHalfExtent.Y);
			Maximum.X = FMath::Max(Maximum.X, SlotCenter.X + RotatedHalfExtent.X);
			Maximum.Y = FMath::Max(Maximum.Y, SlotCenter.Y + RotatedHalfExtent.Y);
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

		for (TActorIterator<AActor> ActorIterator(World); ActorIterator; ++ActorIterator)
		{
			AActor* MeshOwner = *ActorIterator;
			if (!IsValid(MeshOwner) || MeshOwner == DressingOwner
				|| MeshOwner->IsHidden() || MeshOwner->IsA<ABuildingBaseActor>())
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
	const float InterpSpeed = 1.f / TransitionDuration;
	for (int32 PatchIndex = 0; PatchIndex < Patches.Num(); ++PatchIndex)
	{
		FVacantDressingPatch& Patch = Patches[PatchIndex];
		const float TargetAlpha = Patch.bTargetVisible ? 1.f : 0.f;
		if (FMath::IsNearlyEqual(Patch.VisibilityAlpha, TargetAlpha, KINDA_SMALL_NUMBER))
		{
			Patch.VisibilityAlpha = TargetAlpha;
			continue;
		}

		Patch.VisibilityAlpha = FMath::FInterpConstantTo(
			Patch.VisibilityAlpha, TargetAlpha, DeltaSeconds, InterpSpeed);
		ApplyPatchTransform(PatchIndex);
		bChangedAnyInstance = true;
		if (!FMath::IsNearlyEqual(Patch.VisibilityAlpha, TargetAlpha, KINDA_SMALL_NUMBER))
		{
			bAnyTransitionActive = true;
		}
	}

	if (bChangedAnyInstance)
	{
		MarkAllHISMRenderStatesDirty();
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

	TableManager->GetAllCityPlotRows(PlotIds);
	PlotIds.Sort([](const FName& Left, const FName& Right)
	{
		return Left.ToString().Compare(Right.ToString(), ESearchCase::CaseSensitive) < 0;
	});
	if (PlotIds.Num() == 0)
	{
		return false;
	}

	for (const FName PlotId : PlotIds)
	{
		if (!IsValid(SpawnManager->GetSpawnedPlotById(PlotId)))
		{
			return false;
		}
	}
	return true;
}

bool AVacantPlotDressingManager::IsCityMappingReady() const
{
	bool bHasAnyOccupant = false;
	for (const FName PlotId : PlotIds)
	{
		TArray<int32> CompanyKeys;
		AcquisitionManager->GetOccupantKeysForPlot(PlotId, CompanyKeys);
		if (CompanyKeys.Num() > 0)
		{
			bHasAnyOccupant = true;
			break;
		}
	}
	if (!bHasAnyOccupant)
	{
		return true;
	}

	UWorld* DressingWorld = GetWorld();
	const UCityCompanyDirector* CityDirector = DressingWorld
		? DressingWorld->GetSubsystem<UCityCompanyDirector>()
		: nullptr;
	return CityDirector && CityDirector->GetSkylineEntries().Num() > 0;
}

bool AVacantPlotDressingManager::IsWorldGeometryReady() const
{
	const UWorld* DressingWorld = GetWorld();
	if (!DressingWorld)
	{
		return false;
	}

	bool bHasRequestedStreamingLevel = false;
	for (const ULevelStreaming* StreamingLevel : DressingWorld->GetStreamingLevels())
	{
		if (!StreamingLevel || !StreamingLevel->ShouldBeLoaded())
		{
			continue;
		}

		bHasRequestedStreamingLevel = true;
		if (!StreamingLevel->IsLevelLoaded()
			|| (StreamingLevel->ShouldBeVisible() && !StreamingLevel->IsLevelVisible()))
		{
			return false;
		}
	}

	// MainMap의 Riverwalk LevelInstance가 아직 등록조차 되지 않은 첫 틱도 준비 전으로 본다.
	return bHasRequestedStreamingLevel;
}

void AVacantPlotDressingManager::BuildPatches()
{
	TArray<FCityDressingData> Rows;
	TableManager->GetAllCityDressingRows(Rows);
	TMap<FName, TArray<FCityDressingData>> GroupedRows;
	for (const FCityDressingData& Row : Rows)
	{
		if (Row.PresetId.IsNone() || Row.SlotId.IsNone() || Row.Mesh.IsNull())
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

	TArray<FFixedDressingBlocker> FixedBlockers;
	GatherFixedDressingBlockers(GetWorld(), this, FixedBlockers);
	UE_LOG(LogTemp, Log,
		TEXT("[CityDressing] Fixed instanced blockers sampled=%d"),
		FixedBlockers.Num());

	constexpr float PlotEdgeInset = 1000.f;
	constexpr float InterPatchPadding = 150.f;
	for (const FName PlotId : PlotIds)
	{
		ACityPlotActor* PlotActor = SpawnManager->GetSpawnedPlotById(PlotId);
		bool bPlotDataFound = false;
		const FCityPlotData PlotData = TableManager->GetCityPlotData(PlotId, bPlotDataFound);
		if (!IsValid(PlotActor) || !bPlotDataFound)
		{
			continue;
		}

		const int32 DesiredPatchCount = FVacantPlotDressingRules::ResolvePatchCount(
			PlotData.BuildingCapacity);
		const FVector PlotCenter3D = PlotActor->GetCenter();
		const FVector2D PlotCenter(PlotCenter3D.X, PlotCenter3D.Y);
		const FVector2D PlotHalfExtent = PlotActor->GetExtent();
		TArray<FVacantPlotFootprint> AcceptedFootprints;
		const int32 DirectionOffset = static_cast<int32>(
			FVacantPlotDressingRules::StableHash(PlotId) % UE_ARRAY_COUNT(CandidateDirections));

		for (int32 CandidateStep = 0;
			CandidateStep < UE_ARRAY_COUNT(CandidateDirections)
				&& AcceptedFootprints.Num() < DesiredPatchCount;
			++CandidateStep)
		{
			const int32 PatchOrdinal = AcceptedFootprints.Num();
			const int32 PresetIndex = FVacantPlotDressingRules::SelectPresetIndex(
				PlotId, PatchOrdinal, Presets.Num());
			if (!Presets.IsValidIndex(PresetIndex))
			{
				break;
			}

			const FRuntimeDressingPreset& Preset = Presets[PresetIndex];
			const int32 QuarterTurns = static_cast<int32>(
				MakeStableSeed(PlotId, PatchOrdinal) % 4u);
			const float PatchYaw = static_cast<float>(QuarterTurns * 90);
			const FVector2D RotatedHalfExtent = FVacantPlotDressingRules::RotateHalfExtent90(
				Preset.BoundsHalfExtent, QuarterTurns);
			const FVector2D Direction = CandidateDirections[
				(CandidateStep + DirectionOffset) % UE_ARRAY_COUNT(CandidateDirections)];
			const FVector2D AvailableOffset(
				PlotHalfExtent.X - RotatedHalfExtent.X - PlotEdgeInset,
				PlotHalfExtent.Y - RotatedHalfExtent.Y - PlotEdgeInset);
			if (AvailableOffset.X < 0.f || AvailableOffset.Y < 0.f)
			{
				continue;
			}

			const FVector2D PatchCenter = PlotCenter + FVector2D(
				Direction.X * AvailableOffset.X,
				Direction.Y * AvailableOffset.Y);
			const FVacantPlotFootprint PatchFootprint(PatchCenter, RotatedHalfExtent);
			bool bOverlapsFixedBlocker = false;
			for (const FFixedDressingBlocker& Blocker : FixedBlockers)
			{
				if (IsFixedBlockerVerticallyRelevant(Blocker, PlotCenter3D.Z)
					&& FVacantPlotDressingRules::Overlaps(
						PatchFootprint, Blocker.Footprint, FixedBlockerPadding))
				{
					bOverlapsFixedBlocker = true;
					break;
				}
			}
			if (!FVacantPlotDressingRules::IsInsidePlot(
				PatchFootprint, PlotCenter, PlotHalfExtent)
				|| FVacantPlotDressingRules::OverlapsAny(
					PatchFootprint, AcceptedFootprints, InterPatchPadding)
				|| bOverlapsFixedBlocker
				|| !PlotActor->IsFootprintOnGround(
					FVector(PatchCenter.X, PatchCenter.Y, PlotCenter3D.Z),
					RotatedHalfExtent.X,
					RotatedHalfExtent.Y))
			{
				continue;
			}

			FVacantDressingPatch Patch;
			Patch.PlotId = PlotId;
			Patch.Footprint = PatchFootprint;
			const FVector2D RotatedBoundsCenter = RotateVector2D(
				Preset.BoundsCenter, PatchYaw);
			const FVector PatchPivot(
				PatchCenter.X - RotatedBoundsCenter.X,
				PatchCenter.Y - RotatedBoundsCenter.Y,
				PlotCenter3D.Z + 5.f);
			const FQuat PatchRotation = FRotator(0.f, PatchYaw, 0.f).Quaternion();

			for (const FCityDressingData& Slot : Preset.Slots)
			{
				UStaticMesh* Mesh = Slot.Mesh.LoadSynchronous();
				if (!Mesh)
				{
					UE_LOG(LogTemp, Warning,
						TEXT("[CityDressing] Mesh load failed preset=%s slot=%s path=%s"),
						*Preset.PresetId.ToString(), *Slot.SlotId.ToString(), *Slot.Mesh.ToString());
					continue;
				}

				UHierarchicalInstancedStaticMeshComponent* HISM = GetOrCreateHISM(
					Mesh, Slot.bCastShadow);
				if (!HISM)
				{
					continue;
				}

				const float StableScale = ResolveStableScale(Slot, PlotId, PatchOrdinal);
				const FVector LocalLocation = Slot.LocalLocation;
				const FVector RotatedLocation = PatchRotation.RotateVector(LocalLocation);
				const FVector WorldLocation = PatchPivot + RotatedLocation;
				const FQuat WorldRotation = PatchRotation * Slot.LocalRotation.Quaternion();
				const FVector WorldScale = Slot.LocalScale * StableScale;
				const FTransform VisibleTransform(WorldRotation, WorldLocation, WorldScale);
				const int32 InstanceIndex = HISM->AddInstance(VisibleTransform, /*bWorldSpace=*/true);
				if (InstanceIndex == INDEX_NONE)
				{
					UE_LOG(LogTemp, Warning,
						TEXT("[CityDressing] AddInstance failed preset=%s slot=%s"),
						*Preset.PresetId.ToString(), *Slot.SlotId.ToString());
					continue;
				}

				FVacantDressingInstanceRef& InstanceRef = Patch.Instances.AddDefaulted_GetRef();
				InstanceRef.Component = HISM;
				InstanceRef.InstanceIndex = InstanceIndex;
				InstanceRef.VisibleTransform = VisibleTransform;
			}

			if (Patch.Instances.Num() == 0)
			{
				continue;
			}
			const int32 NewPatchIndex = Patches.Add(MoveTemp(Patch));
			PatchIndicesByPlot.Add(PlotId, NewPatchIndex);
			AcceptedFootprints.Add(PatchFootprint);
		}
	}
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
	RefreshPlots(PreviousPlotId, PreviewPlotId);
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
	RefreshPlot(PreviousPlotId);
}

void AVacantPlotDressingManager::RefreshPlot(FName PlotId)
{
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
	}
}

void AVacantPlotDressingManager::RefreshPlotInternal(FName PlotId, bool bInstant)
{
	if (!bInitialized || PlotId.IsNone())
	{
		return;
	}

	TArray<int32> PatchIndices;
	PatchIndicesByPlot.MultiFind(PlotId, PatchIndices);
	if (PatchIndices.Num() == 0)
	{
		return;
	}

	ACityPlotActor* PlotActor = SpawnManager.IsValid()
		? SpawnManager->GetSpawnedPlotById(PlotId)
		: nullptr;
	if (!IsValid(PlotActor))
	{
		for (const int32 PatchIndex : PatchIndices)
		{
			SetPatchVisibilityTarget(PatchIndex, false, bInstant);
		}
		return;
	}

	TArray<FVacantPlotFootprint> Occupancies;
	GatherOccupancies(PlotId, Occupancies);
	for (const int32 PatchIndex : PatchIndices)
	{
		if (!Patches.IsValidIndex(PatchIndex))
		{
			continue;
		}
		const bool bVisible = ShouldPatchBeVisible(
			Patches[PatchIndex].Footprint, Occupancies, OverlapSafetyPadding);
		SetPatchVisibilityTarget(PatchIndex, bVisible, bInstant);
	}
}

void AVacantPlotDressingManager::GatherOccupancies(
	FName PlotId,
	TArray<FVacantPlotFootprint>& OutOccupancies) const
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

	if (!AcquisitionManager.IsValid())
	{
		OutOccupancies.Emplace(PlotCenter, PlotHalfExtent);
	}
	else
	{
		TArray<int32> CompanyKeys;
		AcquisitionManager->GetOccupantKeysForPlot(PlotId, CompanyKeys);
		for (const int32 CompanyKey : CompanyKeys)
		{
			AActor* CompanyActor = AcquisitionManager->GetCompanyActor(CompanyKey);
			if (IsValid(CompanyActor) && !CompanyActor->IsHidden()
				&& !CompanyActor->IsActorBeingDestroyed())
			{
				FVector BoundsOrigin;
				FVector BoundsExtent;
				CompanyActor->GetActorBounds(false, BoundsOrigin, BoundsExtent);
				OutOccupancies.Emplace(
					FVector2D(BoundsOrigin.X, BoundsOrigin.Y),
					FVector2D(BoundsExtent.X, BoundsExtent.Y));
			}
			else if (AcquisitionManager->GetState(CompanyKey) != EAcqState::Cleared)
			{
				// 매핑되지 않은 미철거 회사는 부지 전체를 점유한 것으로 처리해 겹침을 fail-closed로 막는다.
				OutOccupancies.Emplace(PlotCenter, PlotHalfExtent);
			}
		}
	}

	if (bHasPreview && PreviewPlotId == PlotId)
	{
		OutOccupancies.Add(PreviewFootprint);
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
	const FBuildingData BuildingData = TableManager->GetBuildingData(
		Building->GetBuildingID(), bBuildingDataFound);
	if (!bBuildingDataFound)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[CityDressing] Missing building footprint row for %s; plot hidden fail-closed"),
			*Building->GetBuildingID().ToString());
		OutOccupancies.Emplace(PlotCenter, PlotHalfExtent);
		return;
	}

	const FVector2D LocalHalfExtent(
		FMath::Max(1, BuildingData.FootprintWidthCells) * FootprintCellSize * 0.5f,
		FMath::Max(1, BuildingData.FootprintDepthCells) * FootprintCellSize * 0.5f);
	const FVector2D WorldHalfExtent = RotateAabbHalfExtent(
		LocalHalfExtent, Building->GetActorRotation().Yaw);
	const FVector BuildingLocation = Building->GetActorLocation();
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
		FVector UpdatedLocation = UpdatedTransform.GetLocation();
		UpdatedLocation.Z -= (1.f - ClampedAlpha) * HiddenDropDistance;
		UpdatedTransform.SetLocation(UpdatedLocation);
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
	bool bCastShadow)
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
	if (!AcquisitionManager.IsValid())
	{
		return;
	}
	for (const FName PlotId : PlotIds)
	{
		TArray<int32> CompanyKeys;
		AcquisitionManager->GetOccupantKeysForPlot(PlotId, CompanyKeys);
		if (CompanyKeys.Contains(CompanyKey))
		{
			RefreshPlot(PlotId);
		}
	}
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
