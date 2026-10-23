#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Entity/Building/BuildingBaseActor.h"
#include "Misc/AutomationTest.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace BuildingGrowthVFXTests
{
struct FFixture
{
	UWorld* World = nullptr;
	ABuildingBaseActor* Building = nullptr;
	UStaticMeshComponent* Base = nullptr;
	UInstancedStaticMeshComponent* Body = nullptr;
	UBoxComponent* Box = nullptr;
	UNiagaraSystem* System = nullptr;

	FFixture()
	{
		World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
		if (!World) { return; }
		if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::EditorPreview).SetCurrentWorld(World); }
		Building = World->SpawnActor<ABuildingBaseActor>();
		UStaticMesh* Cube = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube"))).LoadSynchronous();
		if (!Building || !Cube) { return; }
		Base = Cast<UStaticMeshComponent>(GetObjectProperty(TEXT("MainMeshComponent")));
		Body = Cast<UInstancedStaticMeshComponent>(GetObjectProperty(TEXT("Body_Module")));
		Box = Cast<UBoxComponent>(GetObjectProperty(TEXT("BoxComponent")));
		System = Cast<UNiagaraSystem>(GetObjectProperty(TEXT("BuildCompleteVFX")));
		if (!Base || !Body) { return; }

		UStaticMesh* BaseMesh = DuplicateObject<UStaticMesh>(Cube, GetTransientPackage());
		UStaticMesh* BodyMesh = DuplicateObject<UStaticMesh>(Cube, GetTransientPackage());
		BaseMesh->SetExtendedBounds(FBoxSphereBounds(FVector(-135.f, 85.f, 100.f), FVector(65.f, 75.f, 100.f), 200.f));
		BodyMesh->SetExtendedBounds(FBoxSphereBounds(FVector(190.f, -120.f, 400.f), FVector(70.f, 85.f, 400.f), 800.f));
		Base->SetStaticMesh(BaseMesh);
		Body->SetStaticMesh(BodyMesh);
		Body->ClearInstances();
		Body->AddInstance(FTransform(FVector(0.f, 0.f, 200.f)));

		Building->SetActorTransform(FTransform(FRotator(0.f, 43.f, 0.f),
			FVector(540.f, -320.f, 780.f), FVector(0.7f, 0.85f, 1.2f)));
		Base->SetRelativeLocation(FVector(-18.f, 26.f, 9.f));
		Base->SetRelativeScale3D(FVector(0.62f, 0.72f, 0.8f));
		Body->SetRelativeLocation(FVector(42.f, -31.f, 25.f));
		Body->SetRelativeScale3D(FVector(0.55f, 0.68f, 0.75f));
	}

	~FFixture()
	{
		DestroySpawnedVFX();
		if (World)
		{
			if (GEngine) { GEngine->DestroyWorldContext(World); }
			World->DestroyWorld(false);
		}
	}

	bool IsReady() const { return World && Building && Base && Body && Box && System; }

	UObject* GetObjectProperty(const FName Name) const
	{
		const FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(ABuildingBaseActor::StaticClass(), Name);
		return Property ? Property->GetObjectPropertyValue_InContainer(Building) : nullptr;
	}

	UNiagaraComponent* FindSpawnedVFX() const
	{
		for (TObjectIterator<UNiagaraComponent> It; It; ++It)
		{
			if (IsValid(*It) && It->GetWorld() == World && It->GetAsset() == System)
			{
				return *It;
			}
		}
		return nullptr;
	}

	void DestroySpawnedVFX() const
	{
		if (UNiagaraComponent* Spawned = FindSpawnedVFX())
		{
			Spawned->DestroyComponent();
		}
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGRBuildingGrowthVFXPositionTest,
	"CGR.Building.GrowthVFX.PositionFromVisualMesh",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCGRBuildingGrowthVFXPositionTest::RunTest(const FString& Parameters)
{
	BuildingGrowthVFXTests::FFixture Fixture;
	if (!TestTrue(TEXT("building, collider, and authored Niagara asset are available"), Fixture.IsReady())) { return false; }

	FVector BodyMin, BodyMax;
	Fixture.Body->GetLocalBounds(BodyMin, BodyMax);
	if (!TestTrue(TEXT("synthetic Body bounds are offset from actor origin"),
		BodyMin.X > 0.f && BodyMax.Y < 0.f)) { return false; }
	const float SeamHeight = 1000.f;
	const FVector ExpectedSeam = Fixture.Body->GetComponentTransform().TransformPosition(
		FVector((BodyMin.X + BodyMax.X) * 0.5f, (BodyMin.Y + BodyMax.Y) * 0.5f, SeamHeight));
	Fixture.Building->PlayBuildCompleteVFX(SeamHeight);
	UNiagaraComponent* Spawned = Fixture.FindSpawnedVFX();
	if (!TestNotNull(TEXT("growth spawns the authored Niagara system"), Spawned)) { return false; }
	TestTrue(TEXT("growth dust follows the Body center and transformed seam"),
		Spawned->GetComponentLocation().Equals(ExpectedSeam, 0.1f));
	Fixture.DestroySpawnedVFX();

	Fixture.Box->SetRelativeLocation(FVector(440.f, -270.f, 900.f));
	Fixture.Box->SetBoxExtent(FVector(900.f, 1250.f, 1700.f));
	Fixture.Building->PlayBuildCompleteVFX(SeamHeight);
	Spawned = Fixture.FindSpawnedVFX();
	if (!TestNotNull(TEXT("growth still spawns after collider changes"), Spawned)) { return false; }
	TestTrue(TEXT("collider position, extent, and padding do not move growth dust"),
		Spawned->GetComponentLocation().Equals(ExpectedSeam, 0.1f));
	Fixture.DestroySpawnedVFX();

	FVector BaseMin, BaseMax;
	Fixture.Base->GetLocalBounds(BaseMin, BaseMax);
	if (!TestTrue(TEXT("synthetic Base bounds are offset from actor origin"),
		BaseMax.X < 0.f && BaseMin.Y > 0.f)) { return false; }
	const FVector ExpectedGround = Fixture.Base->GetComponentTransform().TransformPosition(
		FVector((BaseMin.X + BaseMax.X) * 0.5f, (BaseMin.Y + BaseMax.Y) * 0.5f, BaseMin.Z));
	Fixture.Building->PlayBuildCompleteVFX(0.f);
	Spawned = Fixture.FindSpawnedVFX();
	if (!TestNotNull(TEXT("construction spawns the authored Niagara system"), Spawned)) { return false; }
	TestTrue(TEXT("ground dust follows the Base footprint center and lower bound"),
		Spawned->GetComponentLocation().Equals(ExpectedGround, 0.1f));
	return true;
}
#endif
