#if WITH_DEV_AUTOMATION_TESTS

#include "Entity/Ambient/TrafficManager.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"

struct FTrafficLightUpdateFixture
{
	UWorld* TestWorld = nullptr;
	ATrafficManager* Manager = nullptr;

	FTrafficLightUpdateFixture()
	{
		TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
		if (!TestWorld) { return; }
		if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(TestWorld); }
		TestWorld->InitializeActorsForPlay(FURL());
		Manager = TestWorld->SpawnActor<ATrafficManager>();
		UStaticMesh* BodyMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		UStaticMesh* CardMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
		if (!Manager || !BodyMesh || !CardMesh) { return; }

		UInstancedStaticMeshComponent* Body = NewObject<UInstancedStaticMeshComponent>(Manager);
		Body->SetupAttachment(Manager->Root);
		Body->SetStaticMesh(BodyMesh);
		Body->RegisterComponent();
		Manager->BodyISM.Add(Body);
		Body->AddInstance(FTransform::Identity);

		auto MakeCard = [&](UInstancedStaticMeshComponent*& Card)
		{
			Card = NewObject<UInstancedStaticMeshComponent>(Manager);
			Card->SetupAttachment(Manager->Root);
			Card->SetStaticMesh(CardMesh);
			Card->RegisterComponent();
			Card->AddInstance(FTransform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector));
		};
		MakeCard(Manager->FrontLightISM);
		MakeCard(Manager->RearLightISM);
		Manager->ModelHeadlights.SetNum(1);
		Manager->ModelHeadlights[0].Add(FVector(50.f, 0.f, 0.f));
		Manager->ModelTaillights.SetNum(1);
		Manager->ModelTaillights[0].Add(FVector(-50.f, 0.f, 0.f));
		Manager->CachedCameraLoc = FVector(0.f, 0.f, 1000.f);
		Manager->bGlowVisible = true;
	}

	~FTrafficLightUpdateFixture()
	{
		if (!TestWorld) { return; }
		if (GEngine) { GEngine->DestroyWorldContext(TestWorld); }
		TestWorld->DestroyWorld(false);
	}

	bool IsReady() const
	{
		return Manager && Manager->BodyISM.Num() == 1 && Manager->FrontLightISM && Manager->RearLightISM;
	}

	void Move(const FVector& Location, bool bVisible, bool bWasVisible)
	{
		const TArray<int32> OneCard = { 0 };
		Manager->ApplyCarInstances(0, 0, OneCard, OneCard, Location, 0.f, bVisible, bWasVisible);
	}

	void SetNight(bool bNight) { Manager->ApplyNightVisibility(bNight); }

	FTransform FrontTransform() const
	{
		FTransform Result;
		Manager->FrontLightISM->GetInstanceTransform(0, Result, true);
		return Result;
	}

	FTransform BodyTransform() const
	{
		FTransform Result;
		Manager->BodyISM[0]->GetInstanceTransform(0, Result, true);
		return Result;
	}

	FTransform RearTransform() const
	{
		FTransform Result;
		Manager->RearLightISM->GetInstanceTransform(0, Result, true);
		return Result;
	}

	bool LightsVisible() const
	{
		return Manager->FrontLightISM->IsVisible() && Manager->RearLightISM->IsVisible();
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrafficDaylightCardUpdateTest,
	"CGR.City.Traffic.DaylightCardsStayStillAndSunsetRestoresCurrentState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTrafficDaylightCardUpdateTest::RunTest(const FString& Parameters)
{
	FTrafficLightUpdateFixture Fixture;
	if (!TestTrue(TEXT("traffic light fixture is ready"), Fixture.IsReady())) { return false; }

	const FVector InitialPosition(1000.f, 0.f, 0.f);
	const FVector DayPosition(2200.f, 0.f, 0.f);
	const FVector ReappearedPosition(3600.f, 0.f, 0.f);
	Fixture.Move(InitialPosition, true, false);
	const FTransform NightFront = Fixture.FrontTransform();
	const FTransform NightRear = Fixture.RearTransform();
	if (!TestTrue(TEXT("night cards start visible"), NightFront.GetScale3D().X > 0.f && NightRear.GetScale3D().X > 0.f)) { return false; }

	Fixture.SetNight(false);
	Fixture.Move(DayPosition, true, true);
	TestFalse(TEXT("daylight hides both card components"), Fixture.LightsVisible());
	TestTrue(TEXT("the car body still moves during daylight"),
		FVector::Dist(Fixture.BodyTransform().GetLocation(), DayPosition) < 100.f);
	TestTrue(TEXT("daytime body movement leaves hidden front transform untouched"), Fixture.FrontTransform().Equals(NightFront));
	TestTrue(TEXT("daytime body movement leaves hidden rear transform untouched"), Fixture.RearTransform().Equals(NightRear));

	Fixture.Move(DayPosition, false, true);
	Fixture.SetNight(true);
	TestTrue(TEXT("sunset exposes the light components"), Fixture.LightsVisible());
	TestTrue(TEXT("a car hidden during the day has no front glow at sunset"), Fixture.FrontTransform().GetScale3D().IsNearlyZero());
	TestTrue(TEXT("a car hidden during the day has no rear glow at sunset"), Fixture.RearTransform().GetScale3D().IsNearlyZero());

	Fixture.SetNight(false);
	Fixture.Move(ReappearedPosition, true, false);
	TestTrue(TEXT("daytime reappearance does not upload the front card"), Fixture.FrontTransform().GetScale3D().IsNearlyZero());
	Fixture.SetNight(true);
	const FTransform RestoredFront = Fixture.FrontTransform();
	const FTransform RestoredRear = Fixture.RearTransform();
	TestTrue(TEXT("stationary-camera sunset restores the latest front position"),
		RestoredFront.GetScale3D().X > 0.f && FVector::Dist(RestoredFront.GetLocation(), ReappearedPosition) < 100.f);
	TestTrue(TEXT("stationary-camera sunset restores the latest rear position"),
		RestoredRear.GetScale3D().X > 0.f && FVector::Dist(RestoredRear.GetLocation(), ReappearedPosition) < 100.f);
	return true;
}

#endif
