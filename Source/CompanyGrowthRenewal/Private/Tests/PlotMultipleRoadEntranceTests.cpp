#include "Entity/Plot/CityPlotWalkwayManager.h"
#include "Entity/Plot/CityPlotActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Misc/AutomationTest.h"
#include "PhysicsEngine/BodyInstance.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGRPlotMultipleRoadEntrancesTest,
	"CGR.CityWalkways.MultipleIndependentRoadEntrances",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCGRPlotMultipleRoadEntrancesTest::RunTest(const FString& Parameters)
{
	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::EditorPreview, false);
	if (!TestNotNull(TEXT("isolated road entrance world"), TestWorld)) { return false; }
	if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::EditorPreview).SetCurrentWorld(TestWorld); }
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!TestNotNull(TEXT("cube fixture mesh"), Cube))
	{
		if (GEngine) { GEngine->DestroyWorldContext(TestWorld); }
		TestWorld->DestroyWorld(false);
		return false;
	}
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
	UStaticMeshComponent* Ground = MakeBox(FVector(2500, 2500, -50), FVector(100, 100, 1), TEXT("Ground"));
	Ground->SetCollisionResponseToChannel(ECC_GameTraceChannel3, ECR_Block);
	UStaticMeshComponent* BuildingMesh = MakeBox(FVector(4500, 2750, 500), FVector(4, 4, 10), TEXT("Base_Module"));
	ACityPlotActor* Plot = TestWorld->SpawnActor<ACityPlotActor>();
	Plot->SetActorLocation(FVector(2500, 2500, 0));
	const FArrayProperty* BoundaryProperty = FindFProperty<FArrayProperty>(
		ACityPlotActor::StaticClass(), TEXT("DressingBoundaryLocalVertices"));
	if (!TestNotNull(TEXT("authored plot boundary property"), BoundaryProperty))
	{
		if (GEngine) { GEngine->DestroyWorldContext(TestWorld); }
		TestWorld->DestroyWorld(false);
		return false;
	}
	Plot->InitFromAuthoredBounds();
	*BoundaryProperty->ContainerPtrToValuePtr<TArray<FVector2D>>(Plot) = {
		FVector2D(-2500, -2500), FVector2D(2500, -2500),
		FVector2D(2500, 2500), FVector2D(-2500, 2500)
	};
	TestFalse(TEXT("a turn straddling the plot edge is outside the authored boundary"),
		Plot->IsDressingFootprintAllowed(FVector2D(0, 1250), FVector2D(500, 500)));
	TestTrue(TEXT("the first full turn inside the plot is allowed"),
		Plot->IsDressingFootprintAllowed(FVector2D(500, 1250), FVector2D(500, 500)));
	ACityPlotWalkwayManager* Manager = TestWorld->SpawnActor<ACityPlotWalkwayManager>();
	if (!TestNotNull(TEXT("walkway manager"), Manager)
		|| !TestNotNull(TEXT("road material"), Manager->JunctionMaterial.Get()))
	{
		if (GEngine) { GEngine->DestroyWorldContext(TestWorld); }
		TestWorld->DestroyWorld(false);
		return false;
	}
	UStaticMesh* CityRoadMesh = LoadObject<UStaticMesh>(nullptr,
		TEXT("/Game/City_Pack/Meshes/Road-Sidewalk_Pieces/SM_Road_Square.SM_Road_Square"));
	UMaterialInterface* AsphaltMaterial = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Game/City_Pack/Materials/Tilables/MI_Asphalt.MI_Asphalt"));
	UMaterialInterface* SidewalkMaterial = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Game/City_Pack/Materials/Tilables/MI_Sidewalk.MI_Sidewalk"));
	if (!TestNotNull(TEXT("City Pack square road mesh"), CityRoadMesh)
		|| !TestNotNull(TEXT("City Pack asphalt material"), AsphaltMaterial)
		|| !TestNotNull(TEXT("City Pack sidewalk material"), SidewalkMaterial))
	{
		Manager->Destroy();
		if (GEngine) { GEngine->DestroyWorldContext(TestWorld); }
		TestWorld->DestroyWorld(false);
		return false;
	}
	UStaticMeshComponent* AsphaltRoad = MakeBox(FVector(9000, 0, 0), FVector::OneVector, TEXT("AsphaltRoad"));
	AsphaltRoad->SetStaticMesh(CityRoadMesh);
	UStaticMeshComponent* SidewalkRoad = MakeBox(FVector(11000, 0, 0), FVector::OneVector, TEXT("SidewalkRoad"));
	SidewalkRoad->SetStaticMesh(CityRoadMesh);
	for (int32 MaterialIndex = 0; MaterialIndex < AsphaltRoad->GetNumMaterials(); ++MaterialIndex)
	{
		AsphaltRoad->SetMaterial(MaterialIndex, AsphaltMaterial);
		SidewalkRoad->SetMaterial(MaterialIndex, SidewalkMaterial);
	}
	UStaticMeshComponent* AsphaltCube = MakeBox(FVector(13000, 0, 0), FVector::OneVector, TEXT("AsphaltCube"));
	AsphaltCube->SetMaterial(0, AsphaltMaterial);
	Manager->GatherExistingRoads();
	TestEqual(TEXT("only the square road with asphalt is gathered"), Manager->ExistingRoads.Num(), 1);
	if (Manager->ExistingRoads.Num() == 1)
	{
		TestTrue(TEXT("gathered road retains the asphalt material"),
			Manager->ExistingRoads[0].Material.Get() == AsphaltMaterial);
	}
	// The fixture inspects the solver's region output before any authored floor is cut.
	Manager->bBatchRefreshing = true;
	FCityWalkwayPlot State;
	State.Actor = Plot;
	State.bGroundReady = true;
	State.Grid.Origin = FVector2D::ZeroVector;
	State.Grid.CellSize = 500.f;
	State.Grid.Size = FIntPoint(10, 10);
	State.Grid.Ground.Init(1, 100);
	State.GroundHeights.Init(0.f, 100);
	const int32 EntranceCells[] = { 20, 50, 80 };
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const int32 Cell = EntranceCells[Index];
		const double CenterY = 1250.0 + Index * 1500.0;
		State.Grid.EntranceCells.Add(Cell);
		FCityWalkwayEntrance& Entrance = State.Entrances.AddDefaulted_GetRef();
		Entrance.Cell = Cell;
		Entrance.Bounds = FBox2D(FVector2D(-500, CenterY - 500), FVector2D(500, CenterY + 500));
		Entrance.GroundedRoadBounds = FBox2D(FVector2D(-500, CenterY - 500),
			FVector2D(3500, CenterY + 500));
		Entrance.InwardNormal = FVector2D(1, 0);
		Entrance.RoadZ = 0.f;
		Entrance.RoadMaterial = Manager->JunctionMaterial.Get();
	}
	// Two physical gaps keep road growth in separate corridors; each mouth remains clear.
	State.FixedBlockers.Add(FBox2D(FVector2D(0, 1800), FVector2D(5000, 2200)));
	State.FixedBlockers.Add(FBox2D(FVector2D(0, 3300), FVector2D(5000, 3700)));
	FCityWalkwayOccupant& Occupant = State.Occupants.AddDefaulted_GetRef();
	Occupant.Actor = BuildingMesh->GetOwner();
	Occupant.CommittedTransform = BuildingMesh->GetOwner()->GetActorTransform();
	Occupant.Target.Id = 1;
	const FBox BodyBounds = BuildingMesh->GetBodyInstance()->GetBodyBounds();
	Occupant.Target.Bounds = FBox2D(FVector2D(BodyBounds.Min), FVector2D(BodyBounds.Max));
	Occupant.CollisionBodies.Add({ BuildingMesh, INDEX_NONE, BodyBounds });
	Occupant.bRequiresAccess = true;
	State.Result.bValid = true;
	State.Result.ReachableTargetIds.Add(Occupant.Target.Id);
	State.Result.UsedEntranceCells.Add(50); // The existing pedestrian route uses only the middle mouth.

	auto RoadMouths = [&State]()
	{
		TArray<double> Centers;
		for (const FCityStreetSurfaceRegion& Region : State.Surfaces)
		{
			if (Region.bVehicle && Region.bOpenAtStart && !Region.bJunction)
			{
				Centers.Add(Region.Bounds.GetCenter().Y);
			}
		}
		Centers.Sort();
		return Centers;
	};
	Manager->RebuildSurface(State);
	const TArray<double> WideRoads = RoadMouths();
	TestEqual(TEXT("one connected target permits two independent road mouths"), WideRoads.Num(), 2);
	if (WideRoads.Num() == 2)
	{
		TestTrue(TEXT("side-order chooses the two mouths with a 2000 cm edge gap"),
			FMath::IsNearlyEqual(WideRoads[0], 1250.0, 1.0)
			&& FMath::IsNearlyEqual(WideRoads[1], 4250.0, 1.0));
	}
	State.Grid.EntranceCells.Remove(80);
	State.Entrances.RemoveAt(2);
	Manager->RebuildSurface(State);
	TestEqual(TEXT("two close candidates leave only one vehicle entrance"), RoadMouths().Num(), 1);
	State.Result.ReachableTargetIds.Reset();
	State.Result.UsedEntranceCells.Reset();
	Manager->RebuildSurface(State);
	TestEqual(TEXT("no reachable building creates no vehicle entrance"), RoadMouths().Num(), 0);
	Manager->Destroy();
	if (GEngine) { GEngine->DestroyWorldContext(TestWorld); }
	TestWorld->DestroyWorld(false);
	return true;
}
#endif
