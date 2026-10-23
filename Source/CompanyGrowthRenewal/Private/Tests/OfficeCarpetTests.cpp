#include "Misc/AutomationTest.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Office/DecorationActor.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfficeCarpetCollisionTest,
	"CGR.Office.Carpet.CollisionAndFootprint", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOfficeCarpetCollisionTest::RunTest(const FString& Parameters)
{
	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Test world"), TestWorld)) return false;
	if (GEngine) GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(TestWorld);
	ADecorationActor* Carpet = TestWorld->SpawnActor<ADecorationActor>();
	ADecorationActor* OtherCarpet = TestWorld->SpawnActor<ADecorationActor>();
	ADecorationActor* Furniture = TestWorld->SpawnActor<ADecorationActor>();
	Carpet->Category = OtherCarpet->Category = EDecorationCategory::Carpet;
	Furniture->Category = EDecorationCategory::Furniture;
	TestFalse(TEXT("Carpet may be placed under furniture"), Carpet->BlocksPlacementWith(Furniture));
	TestFalse(TEXT("Furniture may be placed over carpet"), Furniture->BlocksPlacementWith(Carpet));
	TestTrue(TEXT("Carpet footprints block other carpets"), Carpet->BlocksPlacementWith(OtherCarpet));
	TestTrue(TEXT("Furniture remains blocking"), Furniture->BlocksPlacementWith(Furniture));
	TSoftObjectPtr<UStaticMesh> CubePath(FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube")));
	UStaticMesh* Cube = CubePath.LoadSynchronous();
	if (TestNotNull(TEXT("Fixture mesh"), Cube))
	{
		Carpet->MeshComponent->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block);
		Carpet->SetDecorationMesh(Cube);
		TestEqual(TEXT("Selection retained"), Carpet->MeshComponent->GetCollisionResponseToChannel(ECC_GameTraceChannel1), ECR_Block);
		TestEqual(TEXT("Active drag retained"), Carpet->MeshComponent->GetCollisionResponseToChannel(ECC_GameTraceChannel2), ECR_Block);
		TestEqual(TEXT("Workers not blocked"), Carpet->MeshComponent->GetCollisionResponseToChannel(ECC_Pawn), ECR_Ignore);
		TestFalse(TEXT("Mesh does not affect navigation"), Carpet->MeshComponent->CanEverAffectNavigation());
		TestFalse(TEXT("Box does not affect navigation"), Carpet->BoxComponent->CanEverAffectNavigation());
		TestEqual(TEXT("No furniture overlap volume"), Carpet->BoxComponent->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
		Carpet->MeshComponent->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Ignore);
		Carpet->SetDecorationMesh(Cube);
		TestEqual(TEXT("Mesh reassignment preserves inactive drag"), Carpet->MeshComponent->GetCollisionResponseToChannel(ECC_GameTraceChannel2), ECR_Ignore);
		Carpet->SetActorTransform(FTransform(FRotator(0, 90, 0), FVector(300, 400, 0), FVector(2, 1, 0.02)));
		const FBox2D Footprint = Carpet->GetFloorFootprint(FTransform::Identity);
		TestTrue(TEXT("Footprint includes rotated scaled extent"), Footprint.GetSize().Equals(FVector2D(100, 200), 0.01));
		TestTrue(TEXT("Footprint preserves center"), Footprint.GetCenter().Equals(FVector2D(300, 400), 0.01));
	}
	if (GEngine) GEngine->DestroyWorldContext(TestWorld);
	TestWorld->DestroyWorld(false);
	return true;
}
#endif
