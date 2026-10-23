#include "Entity/Plot/CityPlotWalkwayManager.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Level.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Entity/Ambient/VacantPlotDressingManager.h"
#include "Entity/Building/BuildingBaseActor.h"
#include "Entity/Factory/BrickFactory.h"
#include "Entity/Plot/CityPlotActor.h"
#include "Manager/CityAcquisitionManager.h"
#include "Manager/CityCompanyDirector.h"
#include "Manager/EntityManager.h"
#include "Manager/SpawnManager.h"
#include "Materials/MaterialInterface.h"
#include "Misc/ScopeExit.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/BodySetup.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "WorldPartition/WorldPartitionSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

namespace CityPlotWalkwayPrivate
{
	bool Overlaps(const FBox2D& A, const FBox2D& B)
	{
		return A.Min.X < B.Max.X - 0.1 && A.Max.X > B.Min.X + 0.1
			&& A.Min.Y < B.Max.Y - 0.1 && A.Max.Y > B.Min.Y + 0.1;
	}

	FBox2D XYBounds(const FBox& Bounds)
	{
		return FBox2D(FVector2D(Bounds.Min.X, Bounds.Min.Y), FVector2D(Bounds.Max.X, Bounds.Max.Y));
	}

	bool GatherCollision(AActor* Actor, FCityWalkwayOccupant& OutOccupant)
	{
		OutOccupant.Actor = Actor;
		OutOccupant.CommittedTransform = Actor->GetActorTransform();
		OutOccupant.Target.Id = static_cast<int32>(Actor->GetUniqueID());
		const AInteractableBaseActor* Interactable = Cast<AInteractableBaseActor>(Actor);
		for (UStaticMeshComponent* Component : TInlineComponentArray<UStaticMeshComponent*>(Actor))
		{
			const bool bBase = Interactable ? Component == Interactable->GetMainMeshComponent()
				: Component->GetFName() == TEXT("Base_Module");
			if (!bBase || !Component->GetStaticMesh() || !Component->IsRegistered()
				|| !Component->IsPhysicsStateCreated()
				|| Component->GetCollisionEnabled() == ECollisionEnabled::NoCollision
				|| Component->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Block) { continue; }
			UInstancedStaticMeshComponent* Instances = Cast<UInstancedStaticMeshComponent>(Component);
			const int32 Count = Instances ? Instances->GetInstanceCount() : 1;
			for (int32 Index = 0; Index < Count; ++Index)
			{
				const int32 BodyIndex = Instances ? Index : INDEX_NONE;
				FBodyInstance* Body = Component->GetBodyInstance(NAME_None, false, BodyIndex);
				if (!Body || !Body->IsValidBodyInstance()) { continue; }
				const FBox Bounds = Body->GetBodyBounds();
				if (!Bounds.IsValid || Bounds.GetExtent().X <= 0 || Bounds.GetExtent().Y <= 0) { continue; }
				OutOccupant.CollisionBodies.Add({Component, BodyIndex, Bounds});
				OutOccupant.Target.Bounds += XYBounds(Bounds);
			}
		}
		return !OutOccupant.CollisionBodies.IsEmpty();
	}

	bool Occupies(const FCityWalkwayOccupant& Occupant, const FBox2D& Rectangle)
	{
		if (!Overlaps(Occupant.Target.Bounds, Rectangle)) { return false; }
		AActor* Actor = Occupant.Actor.Get();
		if (!IsValid(Actor)) { return false; }
		const FTransform Current = Actor->GetActorTransform();
		const FQuat Rotation = Current.GetRotation() * Occupant.CommittedTransform.GetRotation().Inverse();
		for (const FCityWalkwayCollisionBody& CachedBody : Occupant.CollisionBodies)
		{
			if (!Overlaps(XYBounds(CachedBody.Bounds), Rectangle)) { continue; }
			UStaticMeshComponent* Component = CachedBody.Component.Get();
			if (!IsValid(Component) || !Component->IsPhysicsStateCreated()) { return true; }
			if (const UInstancedStaticMeshComponent* Instances = Cast<UInstancedStaticMeshComponent>(Component))
			{
				if (!Instances->IsValidInstance(CachedBody.InstanceIndex)) { return true; }
			}
			FBodyInstance* Body = Component->GetBodyInstance(NAME_None, false, CachedBody.InstanceIndex);
			if (!Body || !Body->IsValidBodyInstance()) { return true; }
			const FVector2D Center = Rectangle.GetCenter(), Half = Rectangle.GetExtent();
			const double QueryHalfHeight = FMath::Min(150.0, CachedBody.Bounds.GetExtent().Z);
			const FVector SavedCenter(Center.X, Center.Y, CachedBody.Bounds.Min.Z + QueryHalfHeight);
			const FVector QueryCenter = Current.TransformPosition(
				Occupant.CommittedTransform.InverseTransformPosition(SavedCenter));
			const FCollisionShape Shape = FCollisionShape::MakeBox(FVector(
				FMath::Max(0.01, Half.X - 0.25), FMath::Max(0.01, Half.Y - 0.25),
				QueryHalfHeight + 1.0));
			const UBodySetup* Setup = Component->GetBodySetup();
			const bool bComplex = Setup && Setup->GetCollisionTraceFlag() == CTF_UseComplexAsSimple;
			if (Body->OverlapTest(QueryCenter, Rotation, Shape, nullptr, bComplex)) { return true; }
		}
		return false;
	}

	bool MakeCandidate(const AActor* Actor, const FVector2D& Center, FCityWalkwayOccupant& Candidate)
	{
		if (!IsValid(Actor) || !GatherCollision(const_cast<AActor*>(Actor), Candidate)) { return false; }
		FVector CurrentCenter = Actor->GetActorLocation();
		if (const ABuildingBaseActor* Building = Cast<ABuildingBaseActor>(Actor))
		{
			CurrentCenter = Building->GetPlacementFootprintCenter();
		}
		else if (const ABrickFactory* Factory = Cast<ABrickFactory>(Actor))
		{
			CurrentCenter = Factory->GetPlacementFootprintCenter(true);
		}
		const FVector2D Delta = Center - FVector2D(CurrentCenter.X, CurrentCenter.Y);
		Candidate.Target.Bounds = FBox2D(Candidate.Target.Bounds.Min + Delta, Candidate.Target.Bounds.Max + Delta);
		Candidate.CommittedTransform.AddToTranslation(FVector(Delta.X, Delta.Y, 0));
		for (FCityWalkwayCollisionBody& Body : Candidate.CollisionBodies)
		{
			Body.Bounds = Body.Bounds.ShiftBy(FVector(Delta.X, Delta.Y, 0));
		}
		return true;
	}

	bool IsOccupied(const FCityWalkwayPlot& State, const FBox2D& Rectangle,
		const AActor* IgnoredActor = nullptr, const FCityWalkwayOccupant* Proposed = nullptr)
	{
		for (const FBox2D& Blocker : State.FixedBlockers)
		{
			if (Overlaps(Blocker, Rectangle)) { return true; }
		}
		for (const FCityWalkwayOccupant& Occupant : State.Occupants)
		{
			if (Occupant.Actor.Get() != IgnoredActor && Occupies(Occupant, Rectangle)) { return true; }
		}
		return Proposed && Occupies(*Proposed, Rectangle);
	}

	FBox2D AvailableRoad(const FCityWalkwayPlot& State, const FCityWalkwayEntrance& Entrance,
		const AActor* IgnoredActor = nullptr, const FCityWalkwayOccupant* Proposed = nullptr)
	{
		auto Blocked = [&](const FBox2D& Bounds) { return IsOccupied(State, Bounds, IgnoredActor, Proposed); };
		if (Blocked(Entrance.Bounds)) { return FBox2D(ForceInit); }
		return FPlotWalkwayRules::TrimStraightRoad(Entrance.GroundedRoadBounds, Entrance.InwardNormal, Blocked);
	}

	int32 GGroundTraceCount = 0;

	bool TraceRectangle(const ACityPlotActor* Plot, const FBox2D& Bounds, float MaximumStep, float& OutZ,
		TMap<FIntVector, float>& GroundCache)
	{
		constexpr float MissedGround = TNumericLimits<float>::Max();
		const FVector2D Span = Bounds.GetSize();
		const int32 CountX = FMath::Max(1, FMath::CeilToInt(Span.X / 200.0));
		const int32 CountY = FMath::Max(1, FMath::CeilToInt(Span.Y / 200.0));
		float Lowest = FLT_MAX, Highest = -FLT_MAX;
		for (int32 Y = 0; Y <= CountY; ++Y)
		{
			for (int32 X = 0; X <= CountX; ++X)
			{
				const FVector2D XY = Bounds.Min + FVector2D(
					FMath::Lerp(1.0, Span.X - 1.0, static_cast<double>(X) / CountX),
					FMath::Lerp(1.0, Span.Y - 1.0, static_cast<double>(Y) / CountY));
				const FIntVector Key(FMath::RoundToInt32(XY.X * 10.0), FMath::RoundToInt32(XY.Y * 10.0),
					static_cast<int32>(Plot->GetUniqueID()));
				float Height = 0.f;
				if (const float* Cached = GroundCache.Find(Key)) { Height = *Cached; }
				else
				{
					++GGroundTraceCount;
					if (!Plot->TryTraceGroundZ(XY, Height)) { Height = MissedGround; }
					GroundCache.Add(Key, Height);
				}
				if (Height == MissedGround
					|| FMath::Abs(Height - Plot->GetCenter().Z) > 600.f) { return false; }
				Lowest = FMath::Min(Lowest, Height);
				Highest = FMath::Max(Highest, Height);
				if (Highest - Lowest > MaximumStep) { return false; }
			}
		}
		OutZ = Highest;
		return true;
	}

	bool HasNearbyParallelRoad(const FBox2D& Bounds, const FVector2D& Direction,
		const FBox2D& Existing, const FVector2D& ExistingDirection, double MinimumGap)
	{
		const bool bAlongX = FMath::Abs(Direction.X) > 0.5;
		if (FMath::Abs(FVector2D::DotProduct(Direction, ExistingDirection)) < 0.99) { return false; }
		const double AlongOverlap = bAlongX
			? FMath::Min(Bounds.Max.X, Existing.Max.X) - FMath::Max(Bounds.Min.X, Existing.Min.X)
			: FMath::Min(Bounds.Max.Y, Existing.Max.Y) - FMath::Max(Bounds.Min.Y, Existing.Min.Y);
		const double Gap = bAlongX
			? FMath::Max(Existing.Min.Y - Bounds.Max.Y, Bounds.Min.Y - Existing.Max.Y)
			: FMath::Max(Existing.Min.X - Bounds.Max.X, Bounds.Min.X - Existing.Max.X);
		return AlongOverlap > 1.0 && Gap < MinimumGap;
	}

	FBox2D NonParallelRoad(const FCityWalkwayPlot& State, const FBox2D& Road,
		const FVector2D& Direction)
	{
		const double Width = FMath::Abs(Direction.X) > 0.5 ? Road.GetSize().Y : Road.GetSize().X;
		return FPlotWalkwayRules::TrimStraightRoad(Road, Direction, [&](const FBox2D& Bounds)
		{
			for (const FCityWalkwayRoad& Existing : State.RoadStrips)
			{
				if (HasNearbyParallelRoad(Bounds, Direction, Existing.Bounds, Existing.Direction, Width * 2.0)) { return true; }
			}
			return false;
		});
	}

	void GatherFixedBounds(UWorld* WalkwayWorld, const AActor* WalkwayOwner,
		const FBox2D& Region, float LowestGround, float HighestGround, TArray<FBox2D>& OutBounds)
	{
		OutBounds.Reset();
		auto AddBounds = [&](const FBox& Bounds)
		{
			if (!Bounds.IsValid || Bounds.Max.Z < LowestGround + 35.f
				|| Bounds.Min.Z > HighestGround + 300.f) { return; }
			const FBox2D Flat = XYBounds(Bounds);
			if (Overlaps(Flat, Region)) { OutBounds.Add(Flat); }
		};
		for (TActorIterator<AActor> It(WalkwayWorld); It; ++It)
		{
			AActor* Candidate = *It;
			if (!IsValid(Candidate) || Candidate == WalkwayOwner || Candidate->IsHidden()
				|| Candidate->IsActorBeingDestroyed() || Candidate->IsA<ABuildingBaseActor>()
				|| Candidate->IsA<ABrickFactory>() || Candidate->IsA<AVacantPlotDressingManager>()
				|| UCityCompanyDirector::ParseBuildingKey(Candidate->GetClass()->GetName()) > 0) { continue; }
			for (UStaticMeshComponent* Component : TInlineComponentArray<UStaticMeshComponent*>(Candidate))
			{
				if (!Component || !Component->IsRegistered() || !Component->IsVisible()
					|| Component->Mobility == EComponentMobility::Movable || !Component->GetStaticMesh()
					|| !Component->IsQueryCollisionEnabled()
					|| Component->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Block) { continue; }
				// Ground continuity comes from surface traces, not the island's aggregate XY box.
				const FString MeshPath = Component->GetStaticMesh()->GetPathName();
				if (MeshPath.StartsWith(TEXT("/Engine/EngineSky/"))
					|| MeshPath.StartsWith(TEXT("/Game/CompanyGrowth/Environment/Terrain/"))
					|| FCityPlotStreetFurniture::IsStreetLampMesh(Component->GetStaticMesh())) { continue; }
				const FBox LocalBounds = Component->GetStaticMesh()->GetBoundingBox();
				if (UInstancedStaticMeshComponent* Instances = Cast<UInstancedStaticMeshComponent>(Component))
				{
					for (int32 Index = 0; Index < Instances->GetInstanceCount(); ++Index)
					{
						FTransform Transform;
						if (Instances->GetInstanceTransform(Index, Transform, true))
						{
							AddBounds(LocalBounds.TransformBy(Transform.ToMatrixWithScale()));
						}
					}
				}
				else { AddBounds(LocalBounds.TransformBy(Component->GetComponentTransform().ToMatrixWithScale())); }
			}
		}
	}

	void MakeInputs(const FCityWalkwayPlot& State, const AActor* IgnoredActor,
		TArray<FPlotWalkwayTarget>& Targets, TArray<FBox2D>& Blockers, FPlotWalkwayGrid& Grid,
		const FCityWalkwayOccupant* Proposed = nullptr)
	{
		Targets.Reset();
		Blockers = State.FixedBlockers;
		Grid = State.Grid;
		Grid.EntranceCells.Reset();
		Grid.EntranceDepth.Reset();
		for (const FCityWalkwayOccupant& Occupant : State.Occupants)
		{
			if (IgnoredActor && Occupant.Actor.Get() == IgnoredActor) { continue; }
			if (Occupant.bRequiresAccess) { Targets.Add(Occupant.Target); }
		}
		if (Proposed) { Targets.Add(Proposed->Target); }
		for (const FCityWalkwayEntrance& Entrance : State.Entrances)
		{
			const FBox2D Road = AvailableRoad(State, Entrance, IgnoredActor, Proposed);
			if (!Road.bIsValid) { continue; }
			const FBox2D Preferred = NonParallelRoad(State, Road, Entrance.InwardNormal);
			const double Depth = Preferred.bIsValid
				? FMath::Abs(FVector2D::DotProduct(Preferred.GetSize(), Entrance.InwardNormal)) : 0.0;
			Grid.EntranceCells.AddUnique(Entrance.Cell);
			double& BestDepth = Grid.EntranceDepth.FindOrAdd(Entrance.Cell);
			BestDepth = FMath::Max(BestDepth, Depth);
		}
	}
}

ACityPlotWalkwayManager::ACityPlotWalkwayManager()
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("WalkwayRoot")));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Road(
		TEXT("/Game/City_Pack/Meshes/Road-Sidewalk_Pieces/SM_Road_Square.SM_Road_Square"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> RoadDouble(
		TEXT("/Game/City_Pack/Meshes/Road-Sidewalk_Pieces/SM_Road_Double.SM_Road_Double"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Junction(
		TEXT("/Game/City_Pack/Materials/Tilables/MI_Road.MI_Road"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sidewalk(
		TEXT("/Game/City_Pack/Meshes/Road-Sidewalk_Pieces/SM_Sidewalk_Flat.SM_Sidewalk_Flat"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Curb(
		TEXT("/Game/City_Pack/Meshes/Road-Sidewalk_Pieces/SM_Sidewalk_Straight.SM_Sidewalk_Straight"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Pedestrian(
		TEXT("/Game/City_Pack/Materials/Tilables/MI_Sidewalk.MI_Sidewalk"));
	RoadMesh = Road.Object;
	RoadDoubleMesh = RoadDouble.Object;
	JunctionMaterial = Junction.Object;
	SidewalkMesh = Sidewalk.Object;
	CurbMesh = Curb.Object;
	PedestrianMaterial = Pedestrian.Object;
}

void ACityPlotWalkwayManager::BeginPlay()
{
	Super::BeginPlay();
	StreetSurface.Initialize(this, RoadMesh, SidewalkMesh, CurbMesh, PedestrianMaterial,
		RoadDoubleMesh, JunctionMaterial);
	StreetFurniture.Initialize(this);
	UWorld* WalkwayWorld = GetWorld();
	EntityManager = WalkwayWorld->GetSubsystem<UEntityManager>();
	AcquisitionManager = WalkwayWorld->GetSubsystem<UCityAcquisitionManager>();
	if (EntityManager.IsValid())
	{
		BuildingChangedHandle = EntityManager->OnBuildingCountChanged.AddUObject(this, &ThisClass::ScheduleRefresh);
	}
	if (AcquisitionManager.IsValid())
	{
		CompanyClearedHandle = AcquisitionManager->OnCompanyVisualCleared.AddUObject(this, &ThisClass::HandleCompanyCleared);
	}
	LevelAddedHandle = FWorldDelegates::LevelAddedToWorld.AddUObject(this, &ThisClass::HandleLevelChanged);
	LevelRemovedHandle = FWorldDelegates::LevelRemovedFromWorld.AddUObject(this, &ThisClass::HandleLevelChanged);
	ScheduleRefresh();
}

void ACityPlotWalkwayManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(RefreshTimer);
	GetWorldTimerManager().ClearTimer(PreviewTimer);
	PreviewPlots.Reset();
	StreetFurniture.Restore();
	StreetSurface.Restore();
	if (EntityManager.IsValid()) { EntityManager->OnBuildingCountChanged.Remove(BuildingChangedHandle); }
	if (AcquisitionManager.IsValid()) { AcquisitionManager->OnCompanyVisualCleared.Remove(CompanyClearedHandle); }
	FWorldDelegates::LevelAddedToWorld.Remove(LevelAddedHandle);
	FWorldDelegates::LevelRemovedFromWorld.Remove(LevelRemovedHandle);
	Super::EndPlay(EndPlayReason);
}

void ACityPlotWalkwayManager::HandleCompanyCleared(int32 CompanyKey)
{
	ScheduleRefresh();
}

void ACityPlotWalkwayManager::HandleLevelChanged(ULevel* ChangedLevel, UWorld* ChangedWorld)
{
	if (ChangedWorld != GetWorld()) { return; }
	GetWorldTimerManager().ClearTimer(PreviewTimer);
	PreviewPlots.Reset();
	StreetFurniture.InvalidateSources();
	AppliedSurfaces.Reset();
	RenderedSurfaces.Reset();
	bAppliedSurfacesValid = false;
	bGroundQueriesPrepared = false;
	GroundSampleCache.Reset();
	ExistingRoads.Reset();
	StreetSurface.Restore();
	InitializationRetries = 0;
	for (TPair<FName, FCityWalkwayPlot>& Pair : Plots)
	{
		Pair.Value.bGroundReady = false;
		Pair.Value.ReservedBounds.Reset();
		Pair.Value.Surfaces.Reset();
	}
	ScheduleRefresh();
}

void ACityPlotWalkwayManager::ScheduleRefresh()
{
	if (GetWorldTimerManager().IsTimerActive(RefreshTimer)) { return; }
	const float RetryDelay = ++InitializationRetries <= 20 ? 0.3f : 2.f;
	GetWorldTimerManager().SetTimer(RefreshTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		RefreshAllPlots();
		if (USpawnManager* Spawn = GetWorld()->GetSubsystem<USpawnManager>())
		{
			if (AVacantPlotDressingManager* Dressing = Spawn->GetVacantPlotDressingManager())
			{
				Dressing->RefreshAllPlots();
			}
		}
	}), RetryDelay, false);
}

bool ACityPlotWalkwayManager::IsGeometryReady() const
{
	const UWorld* WalkwayWorld = GetWorld();
	if (!WalkwayWorld || !WalkwayWorld->HasBegunPlay() || !WalkwayWorld->AreActorsInitialized()
		|| !WalkwayWorld->PersistentLevel || !WalkwayWorld->PersistentLevel->bIsVisible
		|| !WalkwayWorld->GetSubsystem<UCityCompanyDirector>()) { return false; }
	UWorldPartitionSubsystem* Partition = WalkwayWorld
		? WalkwayWorld->GetSubsystem<UWorldPartitionSubsystem>() : nullptr;
	return WalkwayWorld && (!Partition || Partition->IsAllStreamingCompleted());
}

void ACityPlotWalkwayManager::GatherExistingRoads()
{
	using namespace CityPlotWalkwayPrivate;
	ExistingRoads.Reset();
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* RoadActor = *It;
		if (RoadActor == this || RoadActor->IsHidden() || RoadActor->IsActorBeingDestroyed()) { continue; }
		for (UStaticMeshComponent* Component : TInlineComponentArray<UStaticMeshComponent*>(RoadActor))
		{
			UStaticMesh* Mesh = Component ? Component->GetStaticMesh() : nullptr;
			if (!Mesh || !Component->IsRegistered() || !Component->IsVisible()) { continue; }
			const FString Name = Mesh->GetName();
			if (Name != TEXT("SM_Road_Square") && Name != TEXT("SM_Road_Double")
				&& Name != TEXT("SM_Double_Road2") && Name != TEXT("SM_Road_Double_With_Edge")) { continue; }
			UMaterialInterface* Material = nullptr;
			for (int32 Index = 0; Index < Component->GetNumMaterials(); ++Index)
			{
				UMaterialInterface* CandidateMaterial = Component->GetMaterial(Index);
				if (CandidateMaterial && (CandidateMaterial->GetPathName().Contains(TEXT("/MI_Road"))
					|| CandidateMaterial->GetPathName() == TEXT("/Game/City_Pack/Materials/Tilables/MI_Asphalt.MI_Asphalt")))
				{
					Material = CandidateMaterial;
					break;
				}
			}
			if (!Material) { continue; }
			if (const USplineMeshComponent* Spline = Cast<USplineMeshComponent>(Component))
			{
				const FTransform Pose = Spline->GetComponentTransform();
				const FVector Delta = Pose.TransformPosition(Spline->GetEndPosition())
					- Pose.TransformPosition(Spline->GetStartPosition());
				const FVector2D Axis = FVector2D(Delta.X, Delta.Y).GetSafeNormal();
				const FBox Bounds = Component->Bounds.GetBox();
				if (!Bounds.IsValid || FMath::Max(FMath::Abs(Axis.X), FMath::Abs(Axis.Y)) < 0.95) { continue; }
				FCityWalkwayRoad& Road = ExistingRoads.AddDefaulted_GetRef();
				Road.Bounds = XYBounds(Bounds);
				Road.Direction = FMath::Abs(Axis.X) > FMath::Abs(Axis.Y)
					? FVector2D(1.0, 0.0) : FVector2D(0.0, 1.0);
				Road.Height = Bounds.Max.Z;
				Road.Material = Material;
				Road.bCanConnect = false;
				continue;
			}
			auto AddRoad = [&](const FTransform& Transform)
			{
				if (FVector::DotProduct(Transform.GetUnitAxis(EAxis::Z), FVector::UpVector) < 0.9999) { return; }
				const FVector Axis = Transform.GetUnitAxis(EAxis::X);
				if (FMath::Max(FMath::Abs(Axis.X), FMath::Abs(Axis.Y)) < 0.9999) { return; }
				const FBox Bounds = Mesh->GetBoundingBox().TransformBy(Transform.ToMatrixWithScale());
				if (!Bounds.IsValid) { return; }
				FCityWalkwayRoad& Road = ExistingRoads.AddDefaulted_GetRef();
				Road.Bounds = XYBounds(Bounds);
				Road.Direction = FMath::Abs(Axis.X) > 0.5 ? FVector2D(1.0, 0.0) : FVector2D(0.0, 1.0);
				Road.Height = Bounds.Max.Z;
				Road.Material = Material;
			};
			if (const UInstancedStaticMeshComponent* Instances = Cast<UInstancedStaticMeshComponent>(Component))
			{
				for (int32 Index = 0; Index < Instances->GetInstanceCount(); ++Index)
				{
					FTransform Transform;
					if (Instances->GetInstanceTransform(Index, Transform, true)) { AddRoad(Transform); }
				}
			}
			else { AddRoad(Component->GetComponentTransform()); }
		}
	}
}

bool ACityPlotWalkwayManager::BuildGround(ACityPlotActor* Plot, FCityWalkwayPlot& State)
{
	using namespace CityPlotWalkwayPrivate;
	State.Actor = Plot;
	State.bGroundReady = false;
	State.Result = FPlotWalkwayResult();
	State.ReservedBounds.Reset();
	State.Entrances.Reset();
	const FVector Location = Plot->GetCenter();
	const FVector2D Center(Location.X, Location.Y), Extent = Plot->GetExtent();
	State.Grid.Origin = Center - Extent;
	State.Grid.CellSize = WalkwayWidth;
	State.Grid.Size = FIntPoint(FMath::CeilToInt(Extent.X * 2.0 / WalkwayWidth),
		FMath::CeilToInt(Extent.Y * 2.0 / WalkwayWidth));
	const int64 CellCount64 = static_cast<int64>(State.Grid.Size.X) * State.Grid.Size.Y;
	if (CellCount64 <= 0 || CellCount64 > 8192) { return false; }
	const int32 CellCount = static_cast<int32>(CellCount64);
	State.Grid.Ground.Init(0, CellCount);
	State.GroundHeights.Init(0.f, CellCount);
	float LowestGround = FLT_MAX, HighestGround = -FLT_MAX;
	int32 GroundCount = 0;
	for (int32 Index = 0; Index < CellCount; ++Index)
	{
		const FBox2D Bounds = FPlotWalkwayRules::CellBounds(State.Grid, Index);
		float Height = 0.f;
		if (Plot->IsDressingFootprintAllowed(Bounds.GetCenter(), Bounds.GetExtent())
			&& TraceRectangle(Plot, Bounds, MaximumGroundStep, Height, GroundSampleCache))
		{
			State.Grid.Ground[Index] = 1;
			State.GroundHeights[Index] = Height;
			LowestGround = FMath::Min(LowestGround, Height);
			HighestGround = FMath::Max(HighestGround, Height);
			++GroundCount;
		}
	}
	if (GroundCount == 0) { return false; }
	// Adjacent flat samples can still lie on different terraces; reserve the upper rim.
	TArray<uint8> ContinuousGround = State.Grid.Ground;
	for (int32 Index = 0; Index < CellCount; ++Index)
	{
		if (!State.Grid.Ground[Index]) { continue; }
		const int32 X = Index % State.Grid.Size.X, Y = Index / State.Grid.Size.X;
		const int32 Adjacent[] = { X > 0 ? Index - 1 : INDEX_NONE,
			X + 1 < State.Grid.Size.X ? Index + 1 : INDEX_NONE,
			Y > 0 ? Index - State.Grid.Size.X : INDEX_NONE,
			Y + 1 < State.Grid.Size.Y ? Index + State.Grid.Size.X : INDEX_NONE };
		for (int32 Next : Adjacent)
		{
			if (Next != INDEX_NONE && State.Grid.Ground[Next]
				&& State.GroundHeights[Index] - State.GroundHeights[Next] > MaximumGroundStep)
			{
				ContinuousGround[Index] = 0;
			}
		}
	}
	State.Grid.Ground = MoveTemp(ContinuousGround);
	const FBox2D Region(Center - Extent - FVector2D(4000), Center + Extent + FVector2D(4000));
	GatherFixedBounds(GetWorld(), this, Region, LowestGround, HighestGround, State.FixedBlockers);

	TArray<FCityPlotBoundarySegment> Boundary;
	if (!Plot->GetDressingBoundarySegments(Boundary)) { return false; }
	TArray<FCityWalkwayRoad>& RoadStrips = State.RoadStrips;
	RoadStrips.Reset();
	const FBox2D RoadSearch(Center - Extent - FVector2D(3000), Center + Extent + FVector2D(3000));
	for (const FCityWalkwayRoad& Road : ExistingRoads)
	{
		if (Overlaps(RoadSearch, Road.Bounds) && Road.Material.IsValid()
			&& FMath::Abs(Road.Height - HighestGround) <= RoadWidth * 2.0)
		{
			FCityWalkwayRoad Combined = Road;
			Combined.bCanConnect &= FMath::Abs(Road.Height - HighestGround) <= MaximumGroundStep;
			bool bMerged = true;
			while (bMerged)
			{
				bMerged = false;
				for (int32 StripIndex = RoadStrips.Num() - 1; StripIndex >= 0; --StripIndex)
				{
					const FCityWalkwayRoad& Strip = RoadStrips[StripIndex];
					if (Strip.Material != Combined.Material || Strip.Direction != Combined.Direction
						|| Strip.bCanConnect != Combined.bCanConnect
						|| FMath::Abs(Strip.Height - Combined.Height) > 1) { continue; }
					const FBox2D& A = Strip.Bounds;
					const FBox2D& B = Combined.Bounds;
					const bool bJoinX = FMath::Abs(A.Min.Y - B.Min.Y) < 2 && FMath::Abs(A.Max.Y - B.Max.Y) < 2
						&& A.Min.X <= B.Max.X + 2 && A.Max.X >= B.Min.X - 2;
					const bool bJoinY = FMath::Abs(A.Min.X - B.Min.X) < 2 && FMath::Abs(A.Max.X - B.Max.X) < 2
						&& A.Min.Y <= B.Max.Y + 2 && A.Max.Y >= B.Min.Y - 2;
					if ((bJoinX && Combined.Direction.X > 0.5) || (bJoinY && Combined.Direction.Y > 0.5))
					{
						Combined.Bounds += Strip.Bounds;
						RoadStrips.RemoveAtSwap(StripIndex, 1, EAllowShrinking::No);
						bMerged = true;
					}
				}
			}
			RoadStrips.Add(Combined);
		}
	}
	for (int32 Index = 0; Index < CellCount; ++Index)
	{
		if (!State.Grid.Ground[Index]) { continue; }
		const FVector2D CellCenter = FPlotWalkwayRules::CellCenter(State.Grid, Index);
		for (const FCityPlotBoundarySegment& Edge : Boundary)
		{
			const FVector2D Along = (Edge.End - Edge.Start).GetSafeNormal();
			const double Distance = FVector2D::DotProduct(CellCenter - Edge.Start, Edge.InwardNormal);
			const double Projection = FVector2D::DotProduct(CellCenter - Edge.Start, Along);
			if (Distance < WalkwayWidth * 0.5 - 1 || Distance > WalkwayWidth * 3.5
				|| Projection < RoadWidth * 0.5
				|| Projection > (Edge.End - Edge.Start).Size() - RoadWidth * 0.5) { continue; }
			const FCityWalkwayRoad* NearestRoad = nullptr;
			double NearestDistance = 3000.0;
			const bool bAlongX = FMath::Abs(Edge.InwardNormal.X) > 0.5;
			const FVector2D Half = bAlongX ? FVector2D(WalkwayWidth * 0.5, RoadWidth * 0.5)
				: FVector2D(RoadWidth * 0.5, WalkwayWidth * 0.5);
			for (const FCityWalkwayRoad& RoadStrip : RoadStrips)
			{
				if (!RoadStrip.bCanConnect) { continue; }
				const FCityWalkwayRoad* Road = &RoadStrip;
				const bool bFullWidth = bAlongX
					? Road->Bounds.Min.Y <= CellCenter.Y - Half.Y + 1 && Road->Bounds.Max.Y >= CellCenter.Y + Half.Y - 1
					: Road->Bounds.Min.X <= CellCenter.X - Half.X + 1 && Road->Bounds.Max.X >= CellCenter.X + Half.X - 1;
				if (!bFullWidth) { continue; }
				const double RoadEdge = bAlongX
					? (Edge.InwardNormal.X > 0 ? Road->Bounds.Max.X : Road->Bounds.Min.X)
					: (Edge.InwardNormal.Y > 0 ? Road->Bounds.Max.Y : Road->Bounds.Min.Y);
				const double RoadDistance = bAlongX
					? (CellCenter.X - RoadEdge) * Edge.InwardNormal.X
					: (CellCenter.Y - RoadEdge) * Edge.InwardNormal.Y;
				if (RoadDistance >= WalkwayWidth * 0.5 && RoadDistance < NearestDistance)
				{
					NearestDistance = RoadDistance;
					NearestRoad = Road;
				}
			}
			if (!NearestRoad) { continue; }
			const FVector2D End = CellCenter - Edge.InwardNormal * NearestDistance;
			FBox2D Bounds(CellCenter - Half, CellCenter + Half);
			if (bAlongX) { Bounds += FVector2D(End.X, CellCenter.Y); }
			else { Bounds += FVector2D(CellCenter.X, End.Y); }
			bool bBlocked = false;
			for (const FBox2D& Blocker : State.FixedBlockers) { bBlocked |= Overlaps(Bounds, Blocker); }
			// The road endpoint is grounded by its original mesh. Probe the sidewalk
			// inside the authored tile seam, which can leave a few centimetres between meshes.
			FBox2D GroundBounds = Bounds;
			constexpr double TileSeamInset = 5.0;
			if (bAlongX)
			{
				if (Edge.InwardNormal.X > 0) { GroundBounds.Min.X += TileSeamInset; }
				else { GroundBounds.Max.X -= TileSeamInset; }
			}
			else
			{
				if (Edge.InwardNormal.Y > 0) { GroundBounds.Min.Y += TileSeamInset; }
				else { GroundBounds.Max.Y -= TileSeamInset; }
			}
			float Height = 0.f;
			if (!bBlocked && TraceRectangle(Plot, GroundBounds, MaximumGroundStep, Height, GroundSampleCache)
				&& FMath::Abs(Height - State.GroundHeights[Index]) <= MaximumGroundStep)
			{
				FCityWalkwayEntrance& Entrance = State.Entrances.AddDefaulted_GetRef();
				Entrance.Cell = Index;
				Entrance.Bounds = Bounds;
				Entrance.RoadZ = NearestRoad->Height;
				Entrance.InwardNormal = Edge.InwardNormal;
				Entrance.RoadMaterial = NearestRoad->Material;
				Entrance.GroundedRoadBounds = Bounds;
				FVector2D Tip = CellCenter + Edge.InwardNormal * (WalkwayWidth * 0.5);
				for (int32 Step = 0; Step < FMath::Max(State.Grid.Size.X, State.Grid.Size.Y); ++Step)
				{
					const FVector2D NextCenter = Tip + Edge.InwardNormal * (WalkwayWidth * 0.5);
					const FBox2D Strip(NextCenter - Half, NextCenter + Half);
					float StripZ = 0.f;
					if (!Plot->IsDressingFootprintAllowed(Strip.GetCenter(), Strip.GetExtent())
						|| !TraceRectangle(Plot, Strip, MaximumGroundStep, StripZ, GroundSampleCache)
						|| FMath::Abs(StripZ - Height) > MaximumGroundStep) { break; }
					Entrance.GroundedRoadBounds += Strip;
					Tip += Edge.InwardNormal * WalkwayWidth;
				}
				const FVector2D JunctionCenter = End - Edge.InwardNormal * (WalkwayWidth * 0.5);
				Entrance.JunctionBounds = FBox2D(JunctionCenter - Half, JunctionCenter + Half);
				Entrance.JunctionBounds.Min = FVector2D(
					FMath::Max(Entrance.JunctionBounds.Min.X, NearestRoad->Bounds.Min.X),
					FMath::Max(Entrance.JunctionBounds.Min.Y, NearestRoad->Bounds.Min.Y));
				Entrance.JunctionBounds.Max = FVector2D(
					FMath::Min(Entrance.JunctionBounds.Max.X, NearestRoad->Bounds.Max.X),
					FMath::Min(Entrance.JunctionBounds.Max.Y, NearestRoad->Bounds.Max.Y));
			}
		}
	}
	State.bGroundReady = true;
	UE_LOG(LogTemp, Log, TEXT("[PlotWalkways] Ground %s valid=%d/%d entrances=%d blockers=%d"),
		*Plot->GetPlotId().ToString(), GroundCount, CellCount, State.Entrances.Num(), State.FixedBlockers.Num());
	return true;
}

void ACityPlotWalkwayManager::GatherOccupants(ACityPlotActor* Plot, FCityWalkwayPlot& State,
	const AActor* CommittedActor) const
{
	using namespace CityPlotWalkwayPrivate;
	const FVector PlotLocation = Plot->GetCenter();
	const FVector2D Center(PlotLocation.X, PlotLocation.Y), Half = Plot->GetExtent();
	const FBox2D Region(Center - Half - FVector2D(4000), Center + Half + FVector2D(4000));
	TArray<FCityWalkwayOccupant> Occupants;
	TSet<const AActor*> AddedActors;
	auto AddActor = [&](AActor* Candidate)
	{
		if (!IsValid(Candidate) || Candidate->IsHidden() || Candidate->IsActorBeingDestroyed()
			|| AddedActors.Contains(Candidate)) { return; }
		FCityWalkwayOccupant NewOccupant;
		bool bUseCommittedCollision = false;
		if (Candidate != CommittedActor)
		{
			if (const ABuildingBaseActor* Building = Cast<ABuildingBaseActor>(Candidate))
			{
				if (Building->IsFloating())
				{
					bool bFoundCommittedBounds = false;
					for (const TPair<FName, FCityWalkwayPlot>& Pair : Plots)
					{
						for (const FCityWalkwayOccupant& Previous : Pair.Value.Occupants)
						{
							if (Previous.Actor.Get() == Candidate)
							{
								NewOccupant = Previous;
								bUseCommittedCollision = true;
								bFoundCommittedBounds = true;
								break;
							}
						}
						if (bFoundCommittedBounds) { break; }
					}
					if (!bFoundCommittedBounds) { return; }
				}
			}
		}
		if (!bUseCommittedCollision && !GatherCollision(Candidate, NewOccupant)) { return; }
		const FBox2D& Bounds = NewOccupant.Target.Bounds;
		if (!Bounds.bIsValid || !Overlaps(Bounds, Region)) { return; }
		NewOccupant.bRequiresAccess = Plot->IsDressingFootprintAllowed(Bounds.GetCenter(), FVector2D(1.0));
		Occupants.Add(MoveTemp(NewOccupant));
		AddedActors.Add(Candidate);
	};
	if (EntityManager.IsValid())
	{
		for (ABuildingBaseActor* Building : EntityManager->GetBuildings()) { AddActor(Building); }
	}
	for (TActorIterator<ABrickFactory> It(GetWorld()); It; ++It) { AddActor(*It); }
	if (const UCityCompanyDirector* Director = GetWorld()->GetSubsystem<UCityCompanyDirector>())
	{
		for (const FCitySkylineEntry& Entry : Director->GetSkylineEntries()) { AddActor(Entry.Building.Get()); }
	}
	State.Occupants = MoveTemp(Occupants);
}

void ACityPlotWalkwayManager::RefreshPlot(FName PlotId, const AActor* CommittedActor)
{
	if (PlotId.IsNone() || bRefreshing) { return; }
	if (CommittedActor && PreviewBuilding.Get() == CommittedActor) { ClearBuildingPreview(); }
	USpawnManager* Spawn = GetWorld()->GetSubsystem<USpawnManager>();
	ACityPlotActor* Plot = Spawn ? Spawn->GetSpawnedPlotById(PlotId) : nullptr;
	if (!IsValid(Plot) || !Plot->IsAuthoredInLevel()) { return; }
	if (!IsGeometryReady())
	{
		ScheduleRefresh();
		return;
	}
	TGuardValue<bool> RefreshGuard(bRefreshing, true);
	if (!bGroundQueriesPrepared)
	{
		GroundSampleCache.Reset();
		ACityPlotActor::PrepareCityGroundQueries(GetWorld());
		GatherExistingRoads();
		bGroundQueriesPrepared = true;
	}
	FCityWalkwayPlot& State = Plots.FindOrAdd(PlotId);
	const bool bNeedsGround = !State.bGroundReady || State.Actor.Get() != Plot;
	if (bNeedsGround && !BuildGround(Plot, State))
	{
		ScheduleRefresh();
		return;
	}
	const TArray<FCityWalkwayOccupant> Before = State.Occupants;
	GatherOccupants(Plot, State, CommittedActor);
	bool bChanged = bNeedsGround || !State.Result.bValid || Before.Num() != State.Occupants.Num();
	for (int32 Index = 0; !bChanged && Index < Before.Num(); ++Index)
	{
		bChanged = Before[Index].Actor != State.Occupants[Index].Actor
			|| !Before[Index].Target.Bounds.Min.Equals(State.Occupants[Index].Target.Bounds.Min, 0.1)
			|| !Before[Index].Target.Bounds.Max.Equals(State.Occupants[Index].Target.Bounds.Max, 0.1)
			|| !Before[Index].CommittedTransform.GetRotation().Equals(
				State.Occupants[Index].CommittedTransform.GetRotation(), 0.0001)
			|| Before[Index].CollisionBodies.Num() != State.Occupants[Index].CollisionBodies.Num()
			|| Before[Index].bRequiresAccess != State.Occupants[Index].bRequiresAccess;
	}
	if (!bChanged) { return; }
	bCommittedPlotChanged = true;
	TArray<FPlotWalkwayTarget> Targets;
	TArray<FBox2D> Blockers;
	FPlotWalkwayGrid Grid;
	CityPlotWalkwayPrivate::MakeInputs(State, nullptr, Targets, Blockers, Grid);
	State.Result = FPlotWalkwayRules::Solve(Grid, Targets, Blockers, true,
		[&State](const FBox2D& Bounds) { return CityPlotWalkwayPrivate::IsOccupied(State, Bounds); });
	State.Grid.EntranceCells = Grid.EntranceCells;
	RebuildSurface(State);
	UE_LOG(LogTemp, Log, TEXT("[PlotWalkways] %s targets=%d connected=%d cells=%d"),
		*PlotId.ToString(), Targets.Num(), State.Result.ReachableTargetIds.Num(), State.Result.PathCells.Num());
	if (State.Result.bValid && State.Result.ReachableTargetIds.Num() < Targets.Num())
	{
		UE_LOG(LogTemp, Warning, TEXT("[PlotWalkways] %s: %d existing occupants have no grounded 500cm access"),
			*PlotId.ToString(), Targets.Num() - State.Result.ReachableTargetIds.Num());
	}
}

void ACityPlotWalkwayManager::RefreshPlots(FName FirstPlotId, FName SecondPlotId, const AActor* CommittedActor)
{
	RefreshPlot(FirstPlotId, CommittedActor);
	if (SecondPlotId != FirstPlotId) { RefreshPlot(SecondPlotId, CommittedActor); }
}

void ACityPlotWalkwayManager::RefreshAllPlots()
{
	TGuardValue<bool> BatchGuard(bBatchRefreshing, true);
	bCommittedPlotChanged = false;
	for (TActorIterator<ACityPlotActor> It(GetWorld()); It; ++It)
	{
		if (It->IsAuthoredInLevel()) { RefreshPlot(It->GetPlotId()); }
	}
	// A preview rebuild costs hundreds of ms on phones; repeat refreshes (preview spawn/despawn) change nothing.
	if (PreviewBuilding.IsValid()) { if (bCommittedPlotChanged) { RequestPreviewRebuild(); } }
	else { ApplyStreetSurfaces(); }
}

bool ACityPlotWalkwayManager::CanPlaceBuilding(FName PlotId, const FVector2D& Center,
	const FVector2D& HalfExtent, const AActor* PreviewActor) const
{
	using namespace CityPlotWalkwayPrivate;
	const FCityWalkwayPlot* State = Plots.Find(PlotId);
	if (!State || !State->bGroundReady || !State->Result.bValid
		|| HalfExtent.X <= 0 || HalfExtent.Y <= 0) { return false; }
	TArray<FPlotWalkwayTarget> Targets;
	TArray<FBox2D> Blockers;
	FPlotWalkwayGrid Grid;
	FCityWalkwayOccupant Candidate;
	if (!MakeCandidate(PreviewActor, Center, Candidate)) { return false; }
	constexpr int32 ProposedId = MAX_int32;
	Candidate.Target.Id = ProposedId;
	MakeInputs(*State, PreviewActor, Targets, Blockers, Grid, &Candidate);
	const FPlotWalkwayResult Proposed = FPlotWalkwayRules::Solve(Grid, Targets, Blockers, false,
		[&](const FBox2D& Bounds) { return IsOccupied(*State, Bounds, PreviewActor, &Candidate); });
	if (!Proposed.bValid || !Proposed.ReachableTargetIds.Contains(ProposedId)) { return false; }
	for (const FCityWalkwayOccupant& Occupant : State->Occupants)
	{
		if (Occupant.Actor.Get() != PreviewActor && Occupant.bRequiresAccess
			&& State->Result.ReachableTargetIds.Contains(Occupant.Target.Id)
			&& !Proposed.ReachableTargetIds.Contains(Occupant.Target.Id)) { return false; }
	}
	return true;
}

void ACityPlotWalkwayManager::UpdateBuildingPreview(FName PlotId, const FVector2D& Center,
	const FVector2D& HalfExtent, AActor* PreviewActor)
{
	if (!IsValid(PreviewActor) || PlotId.IsNone() || HalfExtent.GetMin() <= 0
		|| Center.ContainsNaN() || HalfExtent.ContainsNaN())
	{
		ClearBuildingPreview();
		return;
	}
	const FQuat Rotation = PreviewActor->GetActorQuat();
	if (PreviewBuilding == PreviewActor && PreviewPlotId == PlotId
		&& PreviewCenter.Equals(Center, 0.1) && PreviewHalfExtent.Equals(HalfExtent, 0.1)
		&& PreviewRotation.Equals(Rotation, 0.0001) && !PreviewPlots.IsEmpty()) { return; }
	PreviewBuilding = PreviewActor;
	PreviewPlotId = PlotId;
	PreviewCenter = Center;
	PreviewHalfExtent = HalfExtent;
	PreviewRotation = Rotation;
	if (LastPreviewUpdateTime < 0.0)
	{
		RequestPreviewRebuild();
		return;
	}
	// While the footprint keeps moving only the placement verdict follows the finger; the street preview
	// regenerates surface meshes, so it waits until the drag settles.
	GetWorldTimerManager().SetTimer(PreviewTimer, this, &ThisClass::RebuildBuildingPreview, PreviewSettleSeconds, false);
}

void ACityPlotWalkwayManager::NotePreviewDragActivity(const FVector2D& FingerXY)
{
	if (LastDragFingerXY.Equals(FingerXY, 1.0)) { return; }
	LastDragFingerXY = FingerXY;
	// The footprint snaps to cells, so a slow drag can hold one cell longer than the settle time.
	if (GetWorldTimerManager().IsTimerActive(PreviewTimer))
	{
		GetWorldTimerManager().SetTimer(PreviewTimer, this, &ThisClass::RebuildBuildingPreview, PreviewSettleSeconds, false);
	}
}

void ACityPlotWalkwayManager::RequestPreviewRebuild()
{
	// Every caller shares one throttle so several requests in a frame collapse into a single rebuild.
	const double Now = GetWorld()->GetTimeSeconds();
	constexpr double UpdateInterval = 0.12;
	if (LastPreviewUpdateTime < 0.0 || Now - LastPreviewUpdateTime >= UpdateInterval)
	{
		GetWorldTimerManager().ClearTimer(PreviewTimer);
		RebuildBuildingPreview();
	}
	else if (!GetWorldTimerManager().IsTimerActive(PreviewTimer))
	{
		GetWorldTimerManager().SetTimer(PreviewTimer, this, &ThisClass::RebuildBuildingPreview,
			static_cast<float>(UpdateInterval - (Now - LastPreviewUpdateTime)), false);
	}
}

void ACityPlotWalkwayManager::RebuildBuildingPreview()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(PlotWalkway_BuildPreview);
	using namespace CityPlotWalkwayPrivate;
	const double PreviewStart = FPlatformTime::Seconds();
	const int32 TracesBefore = GGroundTraceCount;
	double SolveEnd = PreviewStart;
	ON_SCOPE_EXIT
	{
		const double End = FPlatformTime::Seconds();
		UE_LOG(LogTemp, Verbose, TEXT("[PlotWalkways] preview rebuild %.1f ms (solve %.1f, apply %.1f) traces %d"),
			(End - PreviewStart) * 1000.0, (SolveEnd - PreviewStart) * 1000.0, (End - SolveEnd) * 1000.0,
			GGroundTraceCount - TracesBefore);
	};
	AActor* Building = PreviewBuilding.Get();
	const FCityWalkwayPlot* Destination = Plots.Find(PreviewPlotId);
	FCityWalkwayOccupant Candidate;
	if (!Destination || !Destination->bGroundReady || !Destination->Actor.IsValid()
		|| !MakeCandidate(Building, PreviewCenter, Candidate))
	{
		ClearBuildingPreview();
		return;
	}
	TArray<FName> ChangedPlots;
	PreviewPlots.GetKeys(ChangedPlots);
	PreviewPlots.Reset();
	{
		TGuardValue<bool> BatchGuard(bBatchRefreshing, true);
		for (const TPair<FName, FCityWalkwayPlot>& Pair : Plots)
		{
			if (!Pair.Value.bGroundReady || !Pair.Value.Actor.IsValid()) { continue; }
			const bool bWasOccupied = Pair.Value.Occupants.ContainsByPredicate(
				[Building](const FCityWalkwayOccupant& Item) { return Item.Actor.Get() == Building; });
			const FBox2D NearbyBounds(Pair.Value.Grid.Origin - FVector2D(4000), Pair.Value.Grid.Origin
				+ FVector2D(Pair.Value.Grid.Size.X, Pair.Value.Grid.Size.Y) * Pair.Value.Grid.CellSize + FVector2D(4000));
			const bool bNearCandidate = Overlaps(NearbyBounds, Candidate.Target.Bounds);
			if (Pair.Key != PreviewPlotId && !bWasOccupied && !bNearCandidate) { continue; }
			FCityWalkwayPlot& Preview = PreviewPlots.Add(Pair.Key, Pair.Value);
			Preview.Occupants.RemoveAll([Building](const FCityWalkwayOccupant& Item)
			{
				return Item.Actor.Get() == Building;
			});
			if (Pair.Key == PreviewPlotId || bNearCandidate)
			{
				Candidate.bRequiresAccess = Pair.Key == PreviewPlotId;
				Preview.Occupants.Add(Candidate);
			}
			TArray<FPlotWalkwayTarget> Targets;
			TArray<FBox2D> Blockers;
			FPlotWalkwayGrid Grid;
			MakeInputs(Preview, nullptr, Targets, Blockers, Grid);
			Preview.Result = FPlotWalkwayRules::Solve(Grid, Targets, Blockers, true,
				[&Preview](const FBox2D& Bounds) { return IsOccupied(Preview, Bounds); });
			Preview.Grid.EntranceCells = Grid.EntranceCells;
			RebuildSurface(Preview);
			ChangedPlots.AddUnique(Pair.Key);
		}
	}
	LastPreviewUpdateTime = GetWorld()->GetTimeSeconds();
	SolveEnd = FPlatformTime::Seconds();
	bSurfacesDirty = true;
	ApplyStreetSurfaces();
	RefreshPreviewDressing(ChangedPlots);
}

void ACityPlotWalkwayManager::ClearBuildingPreview()
{
	GetWorldTimerManager().ClearTimer(PreviewTimer);
	PreviewBuilding.Reset();
	PreviewPlotId = NAME_None;
	LastPreviewUpdateTime = -1.0;
	if (PreviewPlots.IsEmpty()) { return; }
	TArray<FName> ChangedPlots;
	PreviewPlots.GetKeys(ChangedPlots);
	PreviewPlots.Reset();
	bSurfacesDirty = true;
	ApplyStreetSurfaces();
	RefreshPreviewDressing(ChangedPlots);
}

void ACityPlotWalkwayManager::RefreshPreviewDressing(TConstArrayView<FName> PlotIds)
{
	USpawnManager* Spawn = GetWorld()->GetSubsystem<USpawnManager>();
	AVacantPlotDressingManager* Dressing = Spawn ? Spawn->GetVacantPlotDressingManager() : nullptr;
	if (!IsValid(Dressing)) { return; }
	for (FName PlotId : PlotIds) { Dressing->RefreshPreviewVisibilityPlots(PlotId, NAME_None); }
}

void ACityPlotWalkwayManager::AppendReservedFootprints(FName PlotId, TArray<FVacantPlotFootprint>& OutFootprints,
	bool bIncludePreview) const
{
	const FCityWalkwayPlot* State = bIncludePreview ? PreviewPlots.Find(PlotId) : nullptr;
	if (!State) { State = Plots.Find(PlotId); }
	if (!State || !State->bGroundReady) { return; }
	for (const FBox2D& Bounds : State->ReservedBounds)
	{
		OutFootprints.Emplace(Bounds.GetCenter(), Bounds.GetExtent());
	}
}

bool ACityPlotWalkwayManager::IsPlotReady(FName PlotId) const
{
	const FCityWalkwayPlot* State = Plots.Find(PlotId);
	return State && State->bGroundReady && State->Result.bValid;
}

int32 ACityPlotWalkwayManager::GetPathCellCount(FName PlotId) const
{
	const FCityWalkwayPlot* State = Plots.Find(PlotId);
	return State && State->bGroundReady ? State->Result.PathCells.Num() : 0;
}

int32 ACityPlotWalkwayManager::GetConnectedBuildingCount(FName PlotId) const
{
	const FCityWalkwayPlot* State = Plots.Find(PlotId);
	return State && State->bGroundReady ? State->Result.ReachableTargetIds.Num() : 0;
}

void ACityPlotWalkwayManager::RebuildSurface(FCityWalkwayPlot& State)
{
	using namespace CityPlotWalkwayPrivate;
	State.ReservedBounds.Reset();
	State.Surfaces.Reset();
	TArray<const FCityWalkwayEntrance*> SelectedEntrances;
	TArray<FBox2D> SelectedRoads;
	TArray<int32> CandidateCells;
	if (!State.Result.ReachableTargetIds.IsEmpty())
	{
		CandidateCells = State.Grid.EntranceCells;
		for (const int32 RootCell : State.Result.UsedEntranceCells) { CandidateCells.AddUnique(RootCell); }
		// Sweep each frontage from an end so a central mouth cannot consume two valid spaces.
		CandidateCells.Sort();
	}
	for (const int32 RootCell : CandidateCells)
	{
		const FCityWalkwayEntrance* BestEntrance = nullptr;
		FBox2D BestRoad(ForceInit);
		double BestDepth = -1.0;
		for (const FCityWalkwayEntrance& Entrance : State.Entrances)
		{
			if (Entrance.Cell != RootCell || !Entrance.RoadMaterial.IsValid()) { continue; }
			const FBox2D Road = AvailableRoad(State, Entrance);
			if (!Road.bIsValid) { continue; }
			const FBox2D Preferred = NonParallelRoad(State, Road, Entrance.InwardNormal);
			const double Depth = Preferred.bIsValid
				? FMath::Abs(FVector2D::DotProduct(Preferred.GetSize(), Entrance.InwardNormal)) : 0.0;
			if (Depth > BestDepth)
			{
				BestEntrance = &Entrance;
				BestRoad = Preferred.bIsValid ? Preferred : Road;
				BestDepth = Depth;
			}
		}
		if (BestEntrance)
		{
			SelectedEntrances.Add(BestEntrance);
			SelectedRoads.Add(BestRoad);
		}
	}
	auto AddRegion = [&](const FBox2D& Bounds, bool bVehicle,
		const FCityWalkwayEntrance& Entrance, float GroundZ)
	{
		FCityStreetSurfaceRegion& Region = State.Surfaces.AddDefaulted_GetRef();
		Region.Bounds = Bounds;
		Region.SurfaceZ = bVehicle ? Entrance.RoadZ : GroundZ;
		Region.CurbBaseZ = Entrance.RoadZ;
		Region.bVehicle = bVehicle;
		Region.Direction = Entrance.InwardNormal;
		Region.RoadMaterial = Entrance.RoadMaterial.Get();
		State.ReservedBounds.Add(Bounds);
	};
	auto NearestEntrance = [&](const FVector2D& Point) -> const FCityWalkwayEntrance*
	{
		const FCityWalkwayEntrance* Selected = nullptr;
		double BestDistance = TNumericLimits<double>::Max();
		for (const FCityWalkwayEntrance* Entrance : SelectedEntrances)
		{
			if (!State.Result.UsedEntranceCells.Contains(Entrance->Cell)) { continue; }
			const double Distance = FVector2D::DistSquared(Point,
				FPlotWalkwayRules::CellCenter(State.Grid, Entrance->Cell));
			if (Distance < BestDistance) { BestDistance = Distance; Selected = Entrance; }
		}
		return Selected;
	};
	auto AddPedestrianEntrance = [&](const FCityWalkwayEntrance& Entrance)
	{
		if (!State.Result.UsedEntranceCells.Contains(Entrance.Cell)) { return; }
		FBox2D Bounds = Entrance.Bounds;
		if (FMath::Abs(Entrance.InwardNormal.X) > 0.5)
		{
			Bounds.Min.Y = Entrance.Bounds.GetCenter().Y - WalkwayWidth * 0.5;
			Bounds.Max.Y = Entrance.Bounds.GetCenter().Y + WalkwayWidth * 0.5;
		}
		else
		{
			Bounds.Min.X = Entrance.Bounds.GetCenter().X - WalkwayWidth * 0.5;
			Bounds.Max.X = Entrance.Bounds.GetCenter().X + WalkwayWidth * 0.5;
		}
		AddRegion(Bounds, false, Entrance, State.GroundHeights[Entrance.Cell]);
		State.Surfaces.Last().bOpenAtStart = true;
	};
	for (int32 Index = 0; Index < SelectedEntrances.Num(); ++Index)
	{
		const FCityWalkwayEntrance* Entrance = SelectedEntrances[Index];
		const FBox2D SearchBounds(State.Grid.Origin, State.Grid.Origin
			+ FVector2D(State.Grid.Size.X, State.Grid.Size.Y) * State.Grid.CellSize);
		const ACityPlotActor* Plot = State.Actor.Get();
		auto IsClear = [&](const FBox2D& Bounds)
		{
			if (IsOccupied(State, Bounds)
				|| !Plot->IsDressingFootprintAllowed(Bounds.GetCenter(), Bounds.GetExtent())) { return false; }
			float Height = 0.f;
			return TraceRectangle(Plot, Bounds, MaximumGroundStep, Height, GroundSampleCache)
				&& FMath::Abs(Height - Entrance->RoadZ) <= MaximumGroundStep;
		};
		const double StraightDepth = FMath::Abs(FVector2D::DotProduct(
			SelectedRoads[Index].GetSize(), Entrance->InwardNormal));
		const FVector2D Mouth = SelectedRoads[Index].GetCenter()
			- Entrance->InwardNormal * (StraightDepth * 0.5);
		auto EdgeAllowed = [&](const FBox2D& Bounds, const FVector2D& Direction)
		{
			for (const FCityWalkwayRoad& Road : State.RoadStrips)
			{
				if (HasNearbyParallelRoad(Bounds, Direction, Road.Bounds, Road.Direction, RoadWidth * 2.0)) { return false; }
			}
			for (const FCityStreetSurfaceRegion& Region : State.Surfaces)
			{
				if (Region.bVehicle && !Region.bJunction
					&& HasNearbyParallelRoad(Bounds, Direction, Region.Bounds, Region.Direction, RoadWidth * 2.0)) { return false; }
			}
			return true;
		};
		if (!EdgeAllowed(Entrance->Bounds, Entrance->InwardNormal))
		{
			AddPedestrianEntrance(*Entrance);
			continue;
		}
		auto FindConnection = [&](const FVector2D& Center, const FVector2D& Direction,
			FPlotRoadConnection& OutConnection)
		{
			// Only the last lattice row next to the plot edge needs an exterior bridge.
			if (Plot->IsDressingFootprintAllowed(Center + Direction * WalkwayWidth,
				FVector2D(RoadWidth * 0.5))) { return false; }
			const bool bAlongX = FMath::Abs(Direction.X) > 0.5;
			const double HalfWidth = RoadWidth * 0.5;
			double BestDistance = 3500.0;
			bool bFound = false;
			for (const FCityWalkwayRoad& Road : State.RoadStrips)
			{
				const FVector2D Span = Road.Bounds.GetSize();
				if (!Road.bCanConnect || FMath::Abs(Road.Height - Entrance->RoadZ) > 1.f
					|| Overlaps(Road.Bounds, Entrance->JunctionBounds)) { continue; }
				const double SideMin = bAlongX ? Road.Bounds.Min.Y : Road.Bounds.Min.X;
				const double SideMax = bAlongX ? Road.Bounds.Max.Y : Road.Bounds.Max.X;
				const double SideCenter = bAlongX ? Center.Y : Center.X;
				if (SideMin > SideCenter - HalfWidth + 1.0 || SideMax < SideCenter + HalfWidth - 1.0) { continue; }
				const double NearEdge = bAlongX
					? (Direction.X > 0 ? Road.Bounds.Min.X : Road.Bounds.Max.X)
					: (Direction.Y > 0 ? Road.Bounds.Min.Y : Road.Bounds.Max.Y);
				const double Distance = bAlongX ? (NearEdge - Center.X) * Direction.X
					: (NearEdge - Center.Y) * Direction.Y;
				if (Distance < HalfWidth - 1.0 || Distance >= BestDistance) { continue; }
				const FVector2D End = Center + Direction * Distance;
				FBox2D Bridge(Center - FVector2D(HalfWidth), Center + FVector2D(HalfWidth));
				Bridge += End;
				if (IsOccupied(State, Bridge) || !EdgeAllowed(Bridge, Direction)) { continue; }
				FBox2D GroundBounds = Bridge;
				if (Direction.X > 0.5) { GroundBounds.Max.X -= 5.0; }
				else if (Direction.X < -0.5) { GroundBounds.Min.X += 5.0; }
				else if (Direction.Y > 0.5) { GroundBounds.Max.Y -= 5.0; }
				else { GroundBounds.Min.Y += 5.0; }
				float Height = 0.f;
				if (!TraceRectangle(Plot, GroundBounds, MaximumGroundStep, Height, GroundSampleCache)
					|| FMath::Abs(Height - Entrance->RoadZ) > MaximumGroundStep) { continue; }
				const double JunctionDepth = FMath::Min(static_cast<double>(WalkwayWidth), bAlongX ? Span.X : Span.Y);
				const FVector2D JunctionCenter = End + Direction * (JunctionDepth * 0.5);
				const FVector2D JunctionHalf = bAlongX ? FVector2D(JunctionDepth * 0.5, HalfWidth)
					: FVector2D(HalfWidth, JunctionDepth * 0.5);
				OutConnection.Bounds = Bridge;
				OutConnection.JunctionBounds = FBox2D(JunctionCenter - JunctionHalf, JunctionCenter + JunctionHalf);
				OutConnection.Direction = Direction;
				BestDistance = Distance;
				bFound = true;
			}
			return bFound;
		};
		FPlotRoadNetwork Network;
		FBox2D EntryRoad = SelectedRoads[Index];
		bool bNetworkAttempted = false;
		// Start where a complete turn first fits inside the plot, before a deeper obstacle.
		for (double Distance = RoadWidth * 0.5; Distance <= StraightDepth - RoadWidth * 0.5; Distance += RoadWidth * 0.1)
		{
			const FVector2D Start = Mouth + Entrance->InwardNormal * Distance;
			const FBox2D TurnBounds(Start - FVector2D(RoadWidth * 0.5), Start + FVector2D(RoadWidth * 0.5));
			if (!IsClear(TurnBounds)) { continue; }
			FPlotRoadNetworkOptions Options;
			Options.MinimumDeadEndLength = RoadWidth * 2.0;
			Options.MinimumParallelGap = RoadWidth * 2.0;
			Options.InitialLength = Distance;
			Options.IsEdgeAllowed = EdgeAllowed;
			Options.FindConnection = FindConnection;
			Network = FPlotWalkwayRules::BuildRoadNetwork(SearchBounds, Start,
				Entrance->InwardNormal, RoadWidth, WalkwayWidth, IsClear, Options);
			bNetworkAttempted = true;
			if (!Network.Segments.IsEmpty() || !Network.Connections.IsEmpty())
			{
				if (Entrance->InwardNormal.X > 0.5) { EntryRoad.Max.X = TurnBounds.Max.X; }
				else if (Entrance->InwardNormal.X < -0.5) { EntryRoad.Min.X = TurnBounds.Min.X; }
				else if (Entrance->InwardNormal.Y > 0.5) { EntryRoad.Max.Y = TurnBounds.Max.Y; }
				else { EntryRoad.Min.Y = TurnBounds.Min.Y; }
			}
			break;
		}
		const bool bHasNetwork = !Network.Segments.IsEmpty() || !Network.Connections.IsEmpty();
		if ((!bHasNetwork && (bNetworkAttempted || StraightDepth < RoadWidth * 2.0))
			|| !EdgeAllowed(EntryRoad, Entrance->InwardNormal))
		{
			AddPedestrianEntrance(*Entrance);
			UE_LOG(LogTemp, Log, TEXT("[PlotWalkways] %s pedestrian entrance: short or nearby parallel carriageway"),
				*State.Actor->GetPlotId().ToString());
			continue;
		}
		AddRegion(EntryRoad, true, *Entrance, Entrance->RoadZ);
		State.Surfaces.Last().bOpenAtStart = true;
		FBox2D NetworkBounds = State.Surfaces.Last().Bounds;
		for (int32 SegmentIndex = 0; SegmentIndex < Network.Segments.Num(); ++SegmentIndex)
		{
			AddRegion(Network.Segments[SegmentIndex], true, *Entrance, Entrance->RoadZ);
			State.Surfaces.Last().Direction = Network.Directions[SegmentIndex];
			NetworkBounds += Network.Segments[SegmentIndex];
		}
		for (const FBox2D& Junction : Network.Junctions)
		{
			AddRegion(Junction, true, *Entrance, Entrance->RoadZ);
			State.Surfaces.Last().bJunction = true;
			State.Surfaces.Last().bInternalJunction = true;
		}
		for (const FPlotRoadConnection& Connection : Network.Connections)
		{
			AddRegion(Connection.Bounds, true, *Entrance, Entrance->RoadZ);
			State.Surfaces.Last().Direction = Connection.Direction;
			AddRegion(Connection.JunctionBounds, true, *Entrance, Entrance->RoadZ);
			State.Surfaces.Last().bJunction = true;
			NetworkBounds += Connection.Bounds;
		}
		if (Entrance->JunctionBounds.bIsValid && Entrance->JunctionBounds.GetSize().GetMin() > 1.0)
		{
			AddRegion(Entrance->JunctionBounds, true, *Entrance, Entrance->RoadZ);
			State.Surfaces.Last().bJunction = true;
		}
		UE_LOG(LogTemp, Log, TEXT("[PlotWalkways] %s road width=%.0f straightDepth=%.0f depth=%.0f cm reachable=%d segments=%d junctions=%d connections=%d"),
			*State.Actor->GetPlotId().ToString(), RoadWidth,
			FMath::Abs(FVector2D::DotProduct(SelectedRoads[Index].GetSize(), Entrance->InwardNormal)),
			FMath::Abs(FVector2D::DotProduct(NetworkBounds.GetSize(), Entrance->InwardNormal)),
			Network.ReachableNodeCount, Network.Segments.Num(), Network.Junctions.Num(), Network.Connections.Num());
	}
	const int32 RoadRegionCount = State.Surfaces.Num();
	for (int32 First = 0; First < RoadRegionCount; ++First)
	{
		for (int32 Second = First + 1; Second < RoadRegionCount; ++Second)
		{
			const FCityStreetSurfaceRegion A = State.Surfaces[First];
			const FCityStreetSurfaceRegion B = State.Surfaces[Second];
			if (!A.bVehicle || !B.bVehicle || A.bJunction || B.bJunction
				|| FMath::Abs(FVector2D::DotProduct(A.Direction, B.Direction)) > 0.1
				|| !FMath::IsNearlyEqual(A.SurfaceZ, B.SurfaceZ, 1.f)) { continue; }
			const FBox2D Crossing(FVector2D(FMath::Max(A.Bounds.Min.X, B.Bounds.Min.X), FMath::Max(A.Bounds.Min.Y, B.Bounds.Min.Y)),
				FVector2D(FMath::Min(A.Bounds.Max.X, B.Bounds.Max.X), FMath::Min(A.Bounds.Max.Y, B.Bounds.Max.Y)));
			if (Crossing.GetSize().GetMin() < RoadWidth - 0.1
				|| State.Surfaces.ContainsByPredicate([&](const FCityStreetSurfaceRegion& Existing)
				{ return Existing.bInternalJunction && Existing.Bounds.Equals(Crossing, 0.1); })) { continue; }
			FCityStreetSurfaceRegion Junction = A;
			Junction.Bounds = Crossing;
			Junction.bJunction = true;
			Junction.bInternalJunction = true;
			Junction.bOpenAtStart = false;
			State.Surfaces.Add(Junction);
			State.ReservedBounds.Add(Crossing);
		}
	}
	for (const int32 Cell : State.Result.PathCells)
	{
		const FBox2D Bounds = FPlotWalkwayRules::CellBounds(State.Grid, Cell);
		const FCityWalkwayEntrance* Entrance = NearestEntrance(Bounds.GetCenter());
		if (Entrance) { AddRegion(Bounds, false, *Entrance, State.GroundHeights[Cell]); }
	}
	for (const FPlotWalkwayConnector& Connector : State.Result.Connectors)
	{
		if (const FCityWalkwayEntrance* Entrance = NearestEntrance(Connector.Bounds.GetCenter()))
		{
			AddRegion(Connector.Bounds, false, *Entrance, State.GroundHeights[Connector.Cell]);
		}
	}
	bSurfacesDirty = true;
	if (!bBatchRefreshing) { ApplyStreetSurfaces(); }
}

void ACityPlotWalkwayManager::ApplyStreetSurfaces()
{
	if (!bSurfacesDirty) { return; }
	TArray<FCityStreetSurfaceRegion> Regions;
	for (const TPair<FName, FCityWalkwayPlot>& Pair : Plots)
	{
		const FCityWalkwayPlot* State = PreviewPlots.Find(Pair.Key);
		if (!State) { State = &Pair.Value; }
		if (State->bGroundReady && State->Result.bValid) { Regions.Append(State->Surfaces); }
	}
	bool bChanged = !bAppliedSurfacesValid || Regions.Num() != AppliedSurfaces.Num();
	for (int32 Index = 0; !bChanged && Index < Regions.Num(); ++Index)
	{
		const FCityStreetSurfaceRegion& A = Regions[Index];
		const FCityStreetSurfaceRegion& B = AppliedSurfaces[Index];
		bChanged = !A.Bounds.Equals(B.Bounds, 0.1) || !A.Direction.Equals(B.Direction, 0.0001)
			|| !FMath::IsNearlyEqual(A.SurfaceZ, B.SurfaceZ, 0.1f)
			|| !FMath::IsNearlyEqual(A.CurbBaseZ, B.CurbBaseZ, 0.1f)
			|| A.bVehicle != B.bVehicle || A.bJunction != B.bJunction
			|| A.bInternalJunction != B.bInternalJunction || A.bOpenAtStart != B.bOpenAtStart
			|| A.RoadMaterial != B.RoadMaterial;
	}
	if (bChanged)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PlotWalkway_ApplyPreviewSurface);
		bAppliedSurfacesValid = StreetSurface.Rebuild(Regions, &RenderedSurfaces);
		if (bAppliedSurfacesValid) { AppliedSurfaces = Regions; }
		else { AppliedSurfaces.Reset(); }
	}
	StreetFurniture.Rebuild(RenderedSurfaces, [this](const FBox& Bounds, float GroundZ)
	{
		return CanPlaceStreetFurniture(Bounds, GroundZ);
	});
	bSurfacesDirty = false;
}

bool ACityPlotWalkwayManager::CanPlaceStreetFurniture(const FBox& Bounds, float OriginalGroundZ) const
{
	using namespace CityPlotWalkwayPrivate;
	const FBox2D Flat = XYBounds(Bounds);
	bool bWithinManagedArea = false;
	for (const TPair<FName, FCityWalkwayPlot>& Pair : Plots)
	{
		const FCityWalkwayPlot* State = PreviewPlots.Find(Pair.Key);
		if (!State) { State = &Pair.Value; }
		if (!State->bGroundReady) { continue; }
		const FBox2D NearbyBounds(State->Grid.Origin - FVector2D(4000), State->Grid.Origin
			+ FVector2D(State->Grid.Size.X, State->Grid.Size.Y) * State->Grid.CellSize + FVector2D(4000));
		if (!NearbyBounds.IsInside(Flat.Min) || !NearbyBounds.IsInside(Flat.Max)) { continue; }
		bWithinManagedArea = true;
		if (IsOccupied(*State, Flat)) { return false; }
		for (const FCityWalkwayRoad& Road : State->RoadStrips)
		{
			if (FMath::Abs(Road.Height - OriginalGroundZ) <= 100.f && Overlaps(Road.Bounds, Flat)) { return false; }
		}
	}
	if (!bWithinManagedArea) { return false; }
	FCollisionQueryParams Query(SCENE_QUERY_STAT(StreetLampRelocationGround), true);
	for (const FVector2D& Point : {Flat.GetCenter(), Flat.Min, Flat.Max,
		FVector2D(Flat.Min.X, Flat.Max.Y), FVector2D(Flat.Max.X, Flat.Min.Y)})
	{
		FHitResult Hit;
		if (!GetWorld()->LineTraceSingleByChannel(Hit,
			FVector(Point, OriginalGroundZ + 100.f), FVector(Point, OriginalGroundZ - 100.f),
			ECC_GameTraceChannel3, Query) || FMath::Abs(Hit.ImpactPoint.Z - OriginalGroundZ) > MaximumGroundStep)
		{
			return false;
		}
	}
	return true;
}

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGRPlotRoadTileDirectionTest,
	"CGR.CityWalkways.Rules.AuthoredRoadTileDirection",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRPlotRoadTileDirectionTest::RunTest(const FString& Parameters)
{
	using namespace CityPlotWalkwayPrivate;
	const FBox2D Candidate(FVector2D(0.0, 0.0), FVector2D(4000.0, 1000.0));
	const FVector2D Along(1.0, 0.0), Across(0.0, 1.0);
	const FBox2D ShortTile(FVector2D(1000.0, 1500.0), FVector2D(1500.0, 2500.0));
	TestTrue(TEXT("A 500 cm long two-lane tile still blocks a nearby parallel road"),
		HasNearbyParallelRoad(Candidate, Along, ShortTile, Along, 2000.0));
	TestFalse(TEXT("A perpendicular existing tile remains eligible for a junction"),
		HasNearbyParallelRoad(Candidate, Along, ShortTile, Across, 2000.0));
	const FBox2D SquareTile(FVector2D(1000.0, 1500.0), FVector2D(1500.0, 2000.0));
	TestTrue(TEXT("Square tiles retain their authored road axis"),
		HasNearbyParallelRoad(Candidate, Along, SquareTile, Along, 2000.0));
	const FBox2D SpacedTile(FVector2D(1000.0, 3000.0), FVector2D(1500.0, 4000.0));
	TestFalse(TEXT("An exact 2000 cm edge gap leaves room for another road"),
		HasNearbyParallelRoad(Candidate, Along, SpacedTile, Along, 2000.0));
	return true;
}
#endif
