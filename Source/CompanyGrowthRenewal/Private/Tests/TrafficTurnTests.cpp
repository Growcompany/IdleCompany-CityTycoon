#if WITH_DEV_AUTOMATION_TESTS

#include "Entity/Ambient/TrafficManager.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"

struct FTrafficTurnFixture
{
	UWorld* TestWorld = nullptr;
	ATrafficManager* Manager = nullptr;
	ATrafficManager::FGraphCar Car;

	FTrafficTurnFixture(const FVector& Direction, float Lane, float Slope, float EdgeLength = 10000.f)
	{
		TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
		if (!TestWorld) { return; }
		if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(TestWorld); }
		TestWorld->InitializeActorsForPlay(FURL());
		Manager = TestWorld->SpawnActor<ATrafficManager>();
		if (!Manager) { return; }
		Manager->GraphNodes = { FVector::ZeroVector, Direction * EdgeLength + FVector(0.f, 0.f, Slope * EdgeLength) };
		Manager->Adjacency = { { 1 }, { 0 } };
		Car.FromNode = 0;
		Car.ToNode = 1;
		Car.NextNode = 0;
		Car.PrevNode = 0;
		Car.EdgeLen = EdgeLength;
		Car.Dist = EdgeLength * 0.85f;
		Car.Speed = 1600.f;
		Car.CruiseSpeed = 1600.f;
		Car.LaneOff = Lane;
		Manager->UpdateGraphCar(Car, 0.f);
	}

	~FTrafficTurnFixture()
	{
		if (!TestWorld) { return; }
		if (GEngine) { GEngine->DestroyWorldContext(TestWorld); }
		TestWorld->DestroyWorld(false);
	}

	void Step(float DeltaTime) { Manager->UpdateGraphCar(Car, DeltaTime); }
	float MaxYawStep(float DeltaTime) const { return Manager->MaxYawRate * DeltaTime; }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrafficDeadEndTurnTest,
	"CGR.City.Traffic.DeadEndTurnFollowsHeading",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTrafficDeadEndTurnTest::RunTest(const FString& Parameters)
{
	// A collapsed return curve moves backward while the yaw limiter is still turning the body.
	for (const FVector Direction : { FVector::ForwardVector, FVector::RightVector, -FVector::ForwardVector })
	{
		for (const float Lane : { 150.f, 450.f })
		{
			for (const float DeltaTime : { 1.f / 60.f, 0.0333f, 0.1f })
			{
				FTrafficTurnFixture Fixture(Direction, Lane, 0.1f);
				if (!TestNotNull(TEXT("traffic turn fixture is ready"), Fixture.Manager)) { return false; }
				float MinAlignment = 1.f;
				float MaxYawChange = 0.f;
				float MaxJump = 0.f;
				float MaxGroundError = 0.f;
				for (int32 StepIndex = 0; StepIndex < 1200; ++StepIndex)
				{
					const FVector PreviousLocation = Fixture.Car.LastLoc;
					const float PreviousYaw = Fixture.Car.Yaw;
					Fixture.Step(DeltaTime);
					const FVector Travel = (Fixture.Car.LastLoc - PreviousLocation).GetSafeNormal2D();
					const FVector Heading = FRotator(0.f, Fixture.Car.Yaw, 0.f).Vector();
					if (!Travel.IsNearlyZero())
					{
						MinAlignment = FMath::Min(MinAlignment, static_cast<float>(FVector::DotProduct(Travel, Heading)));
					}
					MaxYawChange = FMath::Max(MaxYawChange, FMath::Abs(FMath::FindDeltaAngleDegrees(PreviousYaw, Fixture.Car.Yaw)));
					MaxJump = FMath::Max(MaxJump, static_cast<float>(FVector::Dist2D(PreviousLocation, Fixture.Car.LastLoc)));
					MaxGroundError = FMath::Max(MaxGroundError, static_cast<float>(FMath::Abs(
						Fixture.Car.LastLoc.Z - FVector::DotProduct(Fixture.Car.LastLoc, Direction) * 0.1f)));
					if (Fixture.Car.FromNode == 1 && Fixture.Car.Dist > 1500.f) { break; }
				}
				const FString Context = FString::Printf(TEXT("direction=%s lane=%.0f dt=%.4f"), *Direction.ToString(), Lane, DeltaTime);
				TestTrue(*FString::Printf(TEXT("%s: car exits in the opposite lane"), *Context),
					Fixture.Car.FromNode == 1 && Fixture.Car.Dist > 1500.f);
				TestTrue(*FString::Printf(TEXT("%s: movement follows the nose (min dot %.3f)"), *Context, MinAlignment), MinAlignment > 0.97f);
				TestTrue(*FString::Printf(TEXT("%s: turn respects the yaw speed limit"), *Context),
					MaxYawChange <= Fixture.MaxYawStep(DeltaTime) + 0.01f);
				TestTrue(*FString::Printf(TEXT("%s: no position jump at the node (max %.1f cm)"), *Context, MaxJump),
					MaxJump <= 1700.f * DeltaTime);
				TestTrue(*FString::Printf(TEXT("%s: car stays on the sloped road"), *Context), MaxGroundError < 0.1f);
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrafficShortDeadEndTurnTest,
	"CGR.City.Traffic.ShortDeadEndTurnStaysWithinRoad",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTrafficShortDeadEndTurnTest::RunTest(const FString& Parameters)
{
	FTrafficTurnFixture Fixture(FVector::ForwardVector, 450.f, 0.f, 500.f);
	if (!TestNotNull(TEXT("short road fixture is ready"), Fixture.Manager)) { return false; }
	float MinX = 500.f;
	float MaxX = 0.f;
	float MinAlignment = 1.f;
	for (int32 StepIndex = 0; StepIndex < 400; ++StepIndex)
	{
		const FVector PreviousLocation = Fixture.Car.LastLoc;
		Fixture.Step(0.0333f);
		MinX = FMath::Min(MinX, static_cast<float>(Fixture.Car.LastLoc.X));
		MaxX = FMath::Max(MaxX, static_cast<float>(Fixture.Car.LastLoc.X));
		const FVector Travel = (Fixture.Car.LastLoc - PreviousLocation).GetSafeNormal2D();
		if (!Travel.IsNearlyZero())
		{
			const FVector Heading = FRotator(0.f, Fixture.Car.Yaw, 0.f).Vector();
			MinAlignment = FMath::Min(MinAlignment, static_cast<float>(FVector::DotProduct(Travel, Heading)));
		}
	}
	TestTrue(TEXT("U-turns stay between both dead-end nodes"), MinX >= -0.1f && MaxX <= 500.1f);
	TestTrue(TEXT("short road turn still follows the nose"), MinAlignment > 0.97f);
	return true;
}

#endif
