#include "Entity/Plot/CityPlotStreetFurniture.h"
#include "Entity/Ambient/StreetLampSpotPool.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Level.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "EngineUtils.h"

namespace CityPlotStreetFurniturePrivate
{
constexpr float BaseRadius = 70.f;
constexpr float LampSpacing = 500.f;
constexpr float GeneratedLampSpacing = 1000.f;
constexpr float GeneratedLongitudinalSpacing = 2500.f;
constexpr float GeneratedPhase = 1250.f;
constexpr float GeneratedEndInset = 750.f;
constexpr float GeneratedRoadEdgeInset = 150.f;
constexpr float RoadHeightTolerance = 100.f;
constexpr float SearchStep = 250.f;
constexpr int32 SearchSteps = 8;

bool CrossesVehicleSurface(const FVector& Base, TConstArrayView<FCityStreetSurfaceRegion> Regions)
{
	const FBox2D Footprint(FVector2D(Base.X - BaseRadius, Base.Y - BaseRadius),
		FVector2D(Base.X + BaseRadius, Base.Y + BaseRadius));
	for (const FCityStreetSurfaceRegion& Region : Regions)
	{
		if (Region.bVehicle && Region.Bounds.bIsValid
			&& FMath::Abs(Region.SurfaceZ - Base.Z) <= RoadHeightTolerance
			&& Region.Bounds.Intersect(Footprint)) { return true; }
	}
	return false;
}

FBox BaseFootprint(const FVector& Base)
{
	return FBox(Base - FVector(BaseRadius, BaseRadius, 5.f),
		Base + FVector(BaseRadius, BaseRadius, 5.f));
}
}

void FCityPlotStreetFurniture::Initialize(AActor* InOwner)
{
	if (Owner.Get() == InOwner) { return; }
	Restore();
	Owner = InOwner;
	Sources.Reset();
	bSourcesDirty = true;
}

bool FCityPlotStreetFurniture::IsStreetLampMesh(const UStaticMesh* Mesh)
{
	if (!Mesh) { return false; }
	const FString Path = Mesh->GetPathName();
	return Path == TEXT("/Game/City_Pack/Meshes/SM_Street_Lamp.SM_Street_Lamp")
		|| Path == TEXT("/Game/City_Pack/Meshes/SM_Street_Lamp_Large.SM_Street_Lamp_Large");
}

void FCityPlotStreetFurniture::CollectSources()
{
	if (!bSourcesDirty) { return; }
	Sources.Reset();
	bSourcesDirty = false;
	AActor* OwningActor = Owner.Get();
	UWorld* LampWorld = OwningActor ? OwningActor->GetWorld() : nullptr;
	if (!LampWorld) { return; }
	for (ULevel* LampLevel : LampWorld->GetLevels())
	{
		if (!IsValid(LampLevel) || !LampLevel->bIsVisible || LampLevel->bIsBeingRemoved) { continue; }
		for (AActor* LampActor : LampLevel->Actors)
		{
			if (!IsValid(LampActor) || LampActor == OwningActor) { continue; }
			for (UStaticMeshComponent* Component : TInlineComponentArray<UStaticMeshComponent*>(LampActor))
			{
				const UStaticMesh* Mesh = Component ? Component->GetStaticMesh() : nullptr;
				if (!IsStreetLampMesh(Mesh)) { continue; }
				FVector ArmLocal = FVector::ZeroVector;
				for (const UStaticMeshSocket* Socket : Mesh->Sockets)
				{
					if (Socket && Socket->SocketName.ToString().StartsWith(TEXT("Lamp"), ESearchCase::IgnoreCase))
					{
						ArmLocal = Socket->RelativeLocation;
						break;
					}
				}
				if (ArmLocal.IsNearlyZero()) { ArmLocal = FVector::YAxisVector; }
				UInstancedStaticMeshComponent* Instances = Cast<UInstancedStaticMeshComponent>(Component);
				const int32 Count = Instances ? Instances->GetInstanceCount() : 1;
				for (int32 Index = 0; Index < Count; ++Index)
				{
					FSource Source;
					Source.Component = Component;
					Source.InstanceIndex = Instances ? Index : INDEX_NONE;
					Source.bOriginallyVisible = Component->IsVisible();
					if (Instances)
					{
						if (!Instances->GetInstanceTransform(Index, Source.OriginalLocal, false)
							|| !Instances->GetInstanceTransform(Index, Source.OriginalWorld, true)) { continue; }
					}
					else
					{
						Source.OriginalLocal = Component->GetRelativeTransform();
						Source.OriginalWorld = Component->GetComponentTransform();
					}
					if (Source.OriginalWorld.GetScale3D().IsNearlyZero()) { continue; }
					FVector ArmWorld = Source.OriginalWorld.TransformVector(ArmLocal);
					ArmWorld.Z = 0.f;
					const FVector2D Arm(ArmWorld.X, ArmWorld.Y);
					Source.RowDirection = Arm.IsNearlyZero() ? FVector2D(1.f, 0.f)
						: FVector2D(-Arm.Y, Arm.X).GetSafeNormal();
					Source.GroundZ = Source.OriginalWorld.TransformPosition(FVector(0.f, 0.f,
						Mesh->GetBoundingBox().Min.Z)).Z;
					Sources.Add(MoveTemp(Source));
				}
			}
		}
	}
}

void FCityPlotStreetFurniture::Apply(const FSource& Source, const FTransform& DesiredWorld, bool bVisible)
{
	UStaticMeshComponent* Component = Source.Component.Get();
	if (!Component) { return; }
	bool bChanged = false;
	if (UInstancedStaticMeshComponent* Instances = Cast<UInstancedStaticMeshComponent>(Component))
	{
		FTransform Current;
		if (Instances->GetInstanceTransform(Source.InstanceIndex, Current, true)
			&& !Current.Equals(DesiredWorld))
		{
			const bool bOriginal = DesiredWorld.Equals(Source.OriginalWorld);
			Instances->UpdateInstanceTransform(Source.InstanceIndex,
				bOriginal ? Source.OriginalLocal : DesiredWorld, !bOriginal, true, true);
			bChanged = true;
		}
	}
	else
	{
		if (!Component->GetComponentTransform().Equals(DesiredWorld))
		{
			if (DesiredWorld.Equals(Source.OriginalWorld)) { Component->SetRelativeTransform(Source.OriginalLocal); }
			else { Component->SetWorldTransform(DesiredWorld); }
			bChanged = true;
		}
		if (Component->IsVisible() != bVisible) { Component->SetVisibility(bVisible); bChanged = true; }
	}
	if (bChanged && Component->GetOwner()) { ChangedLevels.AddUnique(Component->GetOwner()->GetLevel()); }
}

void FCityPlotStreetFurniture::RefreshLampPools()
{
	if (ChangedLevels.IsEmpty()) { return; }
	if (const AActor* OwningActor = Owner.Get())
	{
		if (UWorld* LampWorld = OwningActor->GetWorld())
		{
			for (TActorIterator<AStreetLampSpotPool> It(LampWorld); It; ++It)
			{
				It->RefreshLampSources(ChangedLevels);
			}
		}
	}
	ChangedLevels.Reset();
}

void FCityPlotStreetFurniture::Rebuild(TConstArrayView<FCityStreetSurfaceRegion> Regions,
	const TFunction<bool(const FBox&, float OriginalGroundZ)>& CanPlace)
{
	using namespace CityPlotStreetFurniturePrivate;
	CollectSources();
	TArray<FVector> Occupied;
	Occupied.Reserve(Sources.Num());
	TArray<bool> NeedsMove;
	NeedsMove.Reserve(Sources.Num());
	for (const FSource& Source : Sources)
	{
		const bool bMove = CrossesVehicleSurface(FVector(Source.OriginalWorld.GetLocation().X,
			Source.OriginalWorld.GetLocation().Y, Source.GroundZ), Regions);
		NeedsMove.Add(bMove);
		if (!bMove && Source.Component.IsValid()) { Occupied.Add(Source.OriginalWorld.GetLocation()); }
	}
	for (int32 SourceIndex = 0; SourceIndex < Sources.Num(); ++SourceIndex)
	{
		const FSource& Source = Sources[SourceIndex];
		if (!Source.Component.IsValid()) { continue; }
		if (!NeedsMove[SourceIndex]) { Apply(Source, Source.OriginalWorld, Source.bOriginallyVisible); continue; }
		bool bFound = false;
		FTransform Desired = Source.OriginalWorld;
		for (int32 Step = 1; Step <= SearchSteps && !bFound; ++Step)
		{
			for (int32 Sign : { -1, 1 })
			{
				const FVector Shift(Source.RowDirection.X * Sign * Step * SearchStep,
					Source.RowDirection.Y * Sign * Step * SearchStep, 0.f);
				const FVector CandidateBase = Source.OriginalWorld.GetLocation() + Shift;
				const FVector CandidateGround(CandidateBase.X, CandidateBase.Y, Source.GroundZ);
				if (CrossesVehicleSurface(CandidateGround, Regions)) { continue; }
				bool bTooClose = false;
				for (const FVector& Existing : Occupied)
				{
					if (FVector::DistSquared2D(Existing, CandidateBase) < FMath::Square(LampSpacing)
						&& FMath::Abs(Existing.Z - CandidateBase.Z) <= RoadHeightTolerance)
						{ bTooClose = true; break; }
				}
				if (bTooClose || !CanPlace(BaseFootprint(CandidateGround), Source.GroundZ)) { continue; }
				Desired.AddToTranslation(Shift);
				Occupied.Add(CandidateBase);
				bFound = true;
				break;
			}
		}
		if (!bFound) { Desired.SetScale3D(FVector::ZeroVector); }
		Apply(Source, Desired, bFound && Source.bOriginallyVisible);
	}
	TArray<FVector> AuthoredGroundBases;
	for (const FSource& Source : Sources)
	{
		UStaticMeshComponent* Component = Source.Component.Get();
		if (!Component || !Source.bOriginallyVisible || !Component->IsVisible()) { continue; }
		FTransform WorldTransform;
		if (const UInstancedStaticMeshComponent* Instances = Cast<UInstancedStaticMeshComponent>(Component))
		{
			if (!Instances->GetInstanceTransform(Source.InstanceIndex, WorldTransform, true)) { continue; }
		}
		else { WorldTransform = Component->GetComponentTransform(); }
		if (WorldTransform.GetScale3D().IsNearlyZero()) { continue; }
		const FVector Location = WorldTransform.GetLocation();
		AuthoredGroundBases.Add(FVector(Location.X, Location.Y, Source.GroundZ));
	}
	RebuildGenerated(Regions, AuthoredGroundBases, CanPlace);
	RefreshLampPools();
}

void FCityPlotStreetFurniture::RebuildGenerated(TConstArrayView<FCityStreetSurfaceRegion> Regions,
	const TArray<FVector>& AuthoredGroundBases,
	const TFunction<bool(const FBox&, float OriginalGroundZ)>& CanPlace)
{
	using namespace CityPlotStreetFurniturePrivate;
	AActor* OwningActor = Owner.Get();
	if (!OwningActor) { ClearGenerated(); return; }
	struct FDesiredGroup
	{
		UStaticMeshComponent* Template = nullptr;
		TArray<FTransform> Transforms;
	};
	TArray<FDesiredGroup> DesiredGroups;
	TArray<FVector> Reserved = AuthoredGroundBases;
	TArray<int32> RoadIndices;
	for (int32 Index = 0; Index < Regions.Num(); ++Index)
	{
		if (Regions[Index].bVehicle && !Regions[Index].bJunction && Regions[Index].Bounds.bIsValid)
		{
			RoadIndices.Add(Index);
		}
	}
	RoadIndices.Sort([&Regions](int32 FirstIndex, int32 SecondIndex)
	{
		const FBox2D& First = Regions[FirstIndex].Bounds;
		const FBox2D& Second = Regions[SecondIndex].Bounds;
		if (First.Min.X != Second.Min.X) { return First.Min.X < Second.Min.X; }
		if (First.Min.Y != Second.Min.Y) { return First.Min.Y < Second.Min.Y; }
		if (First.Max.X != Second.Max.X) { return First.Max.X < Second.Max.X; }
		if (First.Max.Y != Second.Max.Y) { return First.Max.Y < Second.Max.Y; }
		return FirstIndex < SecondIndex;
	});
	for (const int32 RoadIndex : RoadIndices)
	{
		const FCityStreetSurfaceRegion& Road = Regions[RoadIndex];
		const bool bAlongX = FMath::Abs(Road.Direction.X) > 0.5
			|| (Road.Direction.IsNearlyZero() && Road.Bounds.GetSize().X >= Road.Bounds.GetSize().Y);
		const double AlongMin = bAlongX ? Road.Bounds.Min.X : Road.Bounds.Min.Y;
		const double AlongMax = bAlongX ? Road.Bounds.Max.X : Road.Bounds.Max.Y;
		const double First = FMath::CeilToInt((AlongMin + GeneratedEndInset - GeneratedPhase)
			/ GeneratedLongitudinalSpacing) * GeneratedLongitudinalSpacing + GeneratedPhase;
		for (double Along = First; Along <= AlongMax - GeneratedEndInset; Along += GeneratedLongitudinalSpacing)
		{
			for (int32 Side : { -1, 1 })
			{
				const FVector2D XY = bAlongX
					? FVector2D(Along, Side < 0 ? Road.Bounds.Min.Y - GeneratedRoadEdgeInset
						: Road.Bounds.Max.Y + GeneratedRoadEdgeInset)
					: FVector2D(Side < 0 ? Road.Bounds.Min.X - GeneratedRoadEdgeInset
						: Road.Bounds.Max.X + GeneratedRoadEdgeInset, Along);
				const FVector2D Inward = bAlongX ? FVector2D(0.f, Side < 0 ? 1.f : -1.f)
					: FVector2D(Side < 0 ? 1.f : -1.f, 0.f);
				const FSource* Nearest = nullptr;
				double BestDistance = TNumericLimits<double>::Max();
				for (const FSource& Source : Sources)
				{
					const UStaticMeshComponent* SourceComponent = Source.Component.Get();
					if (!Source.bOriginallyVisible || !SourceComponent
						|| !IsStreetLampMesh(SourceComponent->GetStaticMesh())
						|| FMath::Abs(Source.GroundZ - Road.SurfaceZ) > RoadHeightTolerance) { continue; }
					const FVector Location = Source.OriginalWorld.GetLocation();
					const double Distance = FVector2D::DistSquared(XY, FVector2D(Location.X, Location.Y));
					if (Distance < BestDistance) { BestDistance = Distance; Nearest = &Source; }
				}
				if (!Nearest) { continue; }
				const FVector Ground(XY.X, XY.Y, Nearest->GroundZ);
				if (CrossesVehicleSurface(Ground, Regions)
					|| !CanPlace(BaseFootprint(Ground), Nearest->GroundZ)) { continue; }
				const FVector Pivot(XY.X, XY.Y, Nearest->OriginalWorld.GetLocation().Z);
				bool bTooClose = false;
				for (const FVector& Existing : Reserved)
				{
					if (FVector::DistSquared2D(Existing, Ground) < FMath::Square(GeneratedLampSpacing)
						&& FMath::Abs(Existing.Z - Ground.Z) <= RoadHeightTolerance)
					{
						bTooClose = true;
						break;
					}
				}
				if (bTooClose) { continue; }
				UStaticMeshComponent* Template = Nearest->Component.Get();
				const UStaticMesh* Mesh = Template->GetStaticMesh();
				FVector ArmLocal = FVector::YAxisVector;
				for (const UStaticMeshSocket* Socket : Mesh->Sockets)
				{
					if (Socket && Socket->SocketName.ToString().StartsWith(TEXT("Lamp"), ESearchCase::IgnoreCase))
					{
						ArmLocal = Socket->RelativeLocation;
						break;
					}
				}
				if (ArmLocal.IsNearlyZero()) { ArmLocal = FVector::YAxisVector; }
				FVector ArmWorld = Nearest->OriginalWorld.TransformVector(ArmLocal);
				ArmWorld.Z = 0.f;
				if (ArmWorld.IsNearlyZero()) { ArmWorld = Nearest->OriginalWorld.GetRotation().RotateVector(FVector::YAxisVector); }
				const double CurrentYaw = FMath::Atan2(ArmWorld.Y, ArmWorld.X);
				const double DesiredYaw = FMath::Atan2(Inward.Y, Inward.X);
				FQuat Rotation = FQuat(FVector::UpVector, DesiredYaw - CurrentYaw)
					* Nearest->OriginalWorld.GetRotation();
				Rotation.Normalize();
				const FTransform Transform(Rotation, Pivot, Nearest->OriginalWorld.GetScale3D());
				FDesiredGroup* Group = DesiredGroups.FindByPredicate(
					[Template](const FDesiredGroup& Existing) { return Existing.Template == Template; });
				if (!Group) { Group = &DesiredGroups.AddDefaulted_GetRef(); Group->Template = Template; }
				Group->Transforms.Add(Transform);
				Reserved.Add(Ground);
			}
		}
	}

	TArray<FGeneratedGroup> NextGroups;
	TSet<UInstancedStaticMeshComponent*> Retained;
	for (const FDesiredGroup& Desired : DesiredGroups)
	{
		const FGeneratedGroup* Previous = GeneratedGroups.FindByPredicate(
			[&Desired](const FGeneratedGroup& Group) { return Group.Template.Get() == Desired.Template; });
		UInstancedStaticMeshComponent* Instances = Previous ? Previous->Instances.Get() : nullptr;
		if (Instances && (!IsValid(Instances) || !Instances->IsRegistered()
			|| Instances->GetOwner() != OwningActor)) { Instances = nullptr; }
		const bool bReusedInstance = Instances != nullptr;
		if (!Instances)
		{
			Instances = NewObject<UInstancedStaticMeshComponent>(OwningActor, NAME_None, RF_Transient);
			Instances->SetMobility(EComponentMobility::Movable);
			Instances->SetStaticMesh(Desired.Template->GetStaticMesh());
			Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Instances->SetCanEverAffectNavigation(false);
			for (int32 Index = 0; Index < Desired.Template->GetNumMaterials(); ++Index)
			{
				Instances->SetMaterial(Index, Desired.Template->GetMaterial(Index));
			}
			OwningActor->AddInstanceComponent(Instances);
			Instances->RegisterComponent();
			ChangedLevels.AddUnique(OwningActor->GetLevel());
		}
		bool bSame = bReusedInstance && Previous
			&& Instances->GetInstanceCount() == Desired.Transforms.Num()
			&& Previous->WorldTransforms.Num() == Desired.Transforms.Num();
		if (bSame)
		{
			for (int32 Index = 0; Index < Desired.Transforms.Num(); ++Index)
			{
				if (!Previous->WorldTransforms[Index].Equals(Desired.Transforms[Index])) { bSame = false; break; }
			}
		}
		if (!bSame)
		{
			Instances->ClearInstances();
			for (const FTransform& Transform : Desired.Transforms) { Instances->AddInstance(Transform, true); }
			ChangedLevels.AddUnique(OwningActor->GetLevel());
		}
		FGeneratedGroup& Next = NextGroups.AddDefaulted_GetRef();
		Next.Template = Desired.Template;
		Next.Instances = Instances;
		Next.WorldTransforms = Desired.Transforms;
		Retained.Add(Instances);
	}
	for (const FGeneratedGroup& Previous : GeneratedGroups)
	{
		if (UInstancedStaticMeshComponent* Instances = Previous.Instances.Get())
		{
			if (IsValid(Instances) && !Retained.Contains(Instances))
			{
				if (AActor* LampActor = Instances->GetOwner()) { ChangedLevels.AddUnique(LampActor->GetLevel()); }
				Instances->DestroyComponent();
			}
		}
	}
	GeneratedGroups = MoveTemp(NextGroups);
}

void FCityPlotStreetFurniture::ClearGenerated()
{
	for (const FGeneratedGroup& Group : GeneratedGroups)
	{
		if (UInstancedStaticMeshComponent* Instances = Group.Instances.Get())
		{
			if (IsValid(Instances))
			{
				if (AActor* LampActor = Instances->GetOwner()) { ChangedLevels.AddUnique(LampActor->GetLevel()); }
				Instances->DestroyComponent();
			}
		}
	}
	GeneratedGroups.Reset();
}

void FCityPlotStreetFurniture::Restore()
{
	ClearGenerated();
	for (const FSource& Source : Sources)
	{
		Apply(Source, Source.OriginalWorld, Source.bOriginallyVisible);
	}
	RefreshLampPools();
}

void FCityPlotStreetFurniture::InvalidateSources()
{
	Restore();
	Sources.Reset();
	bSourcesDirty = true;
}
