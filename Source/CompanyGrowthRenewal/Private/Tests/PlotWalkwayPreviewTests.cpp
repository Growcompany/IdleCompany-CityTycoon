#include "Entity/Plot/CityPlotWalkwayManager.h"
#include "Entity/Plot/CityPlotActor.h"
#include "Entity/Ambient/VacantPlotDressingRules.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "PhysicsEngine/BodyInstance.h"
#include "TimerManager.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGRPlotWalkwayPreviewLifecycleTest,
	"CGR.CityWalkways.Preview.MoveCancelAndPendingRefresh",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCGRPlotWalkwayPreviewLifecycleTest::RunTest(const FString& Parameters)
{
	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::EditorPreview, false);
	if (!TestNotNull(TEXT("Preview world"), TestWorld)) { return false; }
	if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::EditorPreview).SetCurrentWorld(TestWorld); }
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	auto MakeBox = [&](const FVector& Location, const FVector& Scale, FName Name)
	{
		AActor* BoxActor = TestWorld->SpawnActor<AActor>();
		UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(BoxActor, Name);
		BoxActor->SetRootComponent(Mesh);
		Mesh->SetStaticMesh(Cube);
		Mesh->SetMobility(EComponentMobility::Movable);
		Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
		Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		Mesh->RegisterComponent();
		BoxActor->SetActorLocation(Location);
		BoxActor->SetActorScale3D(Scale);
		return Mesh;
	};
	UStaticMeshComponent* Ground = MakeBox(FVector(2000, 2000, -50), FVector(100, 100, 1), TEXT("Ground"));
	Ground->SetCollisionResponseToChannel(ECC_GameTraceChannel3, ECR_Block);
	UStaticMeshComponent* BuildingMesh = MakeBox(FVector(3000, 3000, 500), FVector(10, 10, 10), TEXT("Base_Module"));
	AActor* Building = BuildingMesh->GetOwner();
	ACityPlotActor* Plot = TestWorld->SpawnActor<ACityPlotActor>();
	Plot->SetActorLocation(FVector(2000, 2000, 0));
	ACityPlotWalkwayManager* Manager = TestWorld->SpawnActor<ACityPlotWalkwayManager>();
	const FName PlotId(TEXT("PreviewFixture"));
	FCityWalkwayPlot& State = Manager->Plots.Add(PlotId);
	State.Actor = Plot;
	State.bGroundReady = true;
	State.Grid.Origin = FVector2D::ZeroVector;
	State.Grid.CellSize = 500.0;
	State.Grid.Size = FIntPoint(8, 8);
	State.Grid.Ground.Init(1, 64);
	State.GroundHeights.Init(0, 64);
	State.Grid.EntranceCells.Add(16);
	FCityWalkwayEntrance& Entrance = State.Entrances.AddDefaulted_GetRef();
	Entrance.Cell = 16;
	Entrance.Bounds = FBox2D(FVector2D(-500, 750), FVector2D(500, 1750));
	Entrance.GroundedRoadBounds = FBox2D(FVector2D(-500, 750), FVector2D(4000, 1750));
	Entrance.InwardNormal = FVector2D(1, 0);
	Entrance.RoadMaterial = Manager->JunctionMaterial;
	FCityWalkwayOccupant& Occupant = State.Occupants.AddDefaulted_GetRef();
	Occupant.Actor = Building;
	Occupant.CommittedTransform = Building->GetActorTransform();
	Occupant.Target.Id = static_cast<int32>(Building->GetUniqueID());
	const FBox BodyBounds = BuildingMesh->GetBodyInstance()->GetBodyBounds();
	Occupant.Target.Bounds = FBox2D(FVector2D(BodyBounds.Min), FVector2D(BodyBounds.Max));
	Occupant.CollisionBodies.Add({BuildingMesh, INDEX_NONE, BodyBounds});
	TArray<FPlotWalkwayTarget> Targets{Occupant.Target};
	State.Result = FPlotWalkwayRules::Solve(State.Grid, Targets, {});
	TestTrue(TEXT("The building fixture has a reachable 500 cm entrance"),
		State.Result.ReachableTargetIds.Contains(Occupant.Target.Id));
	Manager->RebuildSurface(State);
	TestFalse(TEXT("A path without authored street sources is not cached as rendered"), Manager->bAppliedSurfacesValid);
	TestTrue(TEXT("Missing street sources cannot displace lamps along an invisible road"), Manager->RenderedSurfaces.IsEmpty());
	UStaticMeshComponent* GroundProxy = NewObject<UStaticMeshComponent>(Manager, TEXT("RetainedGroundCollision"));
	GroundProxy->SetupAttachment(Manager->GetRootComponent());
	GroundProxy->SetStaticMesh(Cube);
	GroundProxy->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	GroundProxy->SetCollisionResponseToAllChannels(ECR_Ignore);
	GroundProxy->SetCollisionResponseToChannel(ECC_GameTraceChannel3, ECR_Block);
	GroundProxy->RegisterComponent();
	GroundProxy->SetWorldTransform(Ground->GetComponentTransform());
	Ground->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TestTrue(TEXT("Lamp relocation can use ground collision retained by the road manager"),
		Manager->CanPlaceStreetFurniture(FBox(FVector(430, 430, 0), FVector(570, 570, 100)), 0.f));
	GroundProxy->DestroyComponent();
	Ground->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	const FBox2D CommittedBuildingBounds = Occupant.Target.Bounds;
	const TArray<FBox2D> CommittedRoads = State.ReservedBounds;
	TestFalse(TEXT("Committed fixture has a real path"), CommittedRoads.IsEmpty());
	Building->SetActorLocation(FVector(2500, 1250, 1000));
	Manager->UpdateBuildingPreview(PlotId, FVector2D(2500, 1250), FVector2D(500), Building);
	TestTrue(TEXT("Moving a building creates a displayed preview"), Manager->PreviewPlots.Contains(PlotId));
	TestTrue(TEXT("Preview does not overwrite committed occupancy"),
		Manager->Plots[PlotId].Occupants[0].Target.Bounds.Equals(CommittedBuildingBounds, 0.1));
	TArray<FVacantPlotFootprint> CommittedReservations, PreviewReservations;
	Manager->AppendReservedFootprints(PlotId, CommittedReservations);
	Manager->AppendReservedFootprints(PlotId, PreviewReservations, true);
	TestEqual(TEXT("Default reservation remains committed"), CommittedReservations.Num(), CommittedRoads.Num());
	if (const FCityWalkwayPlot* Preview = Manager->PreviewPlots.Find(PlotId))
	{
		TestTrue(TEXT("Displayed footprint follows current XY despite floating Z"),
			Preview->Occupants[0].Target.Bounds.GetCenter().Equals(FVector2D(2500, 1250), 0.1));
		bool bChanged = Preview->ReservedBounds.Num() != CommittedRoads.Num();
		for (int32 Index = 0; !bChanged && Index < CommittedRoads.Num(); ++Index)
		{
			bChanged = !Preview->ReservedBounds[Index].Equals(CommittedRoads[Index], 0.1);
		}
		TestTrue(TEXT("The rendered road layout changes before confirmation"), bChanged);
	}
	Building->SetActorLocation(FVector(1750, 2500, 1000));
	Manager->UpdateBuildingPreview(PlotId, FVector2D(1750, 2500), FVector2D(500), Building);
	TestTrue(TEXT("Rapid movement coalesces to one pending update"),
		TestWorld->GetTimerManager().IsTimerActive(Manager->PreviewTimer));
	Manager->ClearBuildingPreview();
	TestTrue(TEXT("Cancel clears displayed overrides"), Manager->PreviewPlots.IsEmpty());
	TestFalse(TEXT("Cancel removes pending road work"), TestWorld->GetTimerManager().IsTimerActive(Manager->PreviewTimer));
	TestWorld->GetTimerManager().Tick(1.0f);
	TestTrue(TEXT("A canceled timer cannot resurrect a preview"), Manager->PreviewPlots.IsEmpty());
	TestEqual(TEXT("Cancel preserves committed reservations"), Manager->Plots[PlotId].ReservedBounds.Num(), CommittedRoads.Num());
	Manager->Destroy();
	if (GEngine) { GEngine->DestroyWorldContext(TestWorld); }
	TestWorld->DestroyWorld(false);
	return true;
}
#endif
