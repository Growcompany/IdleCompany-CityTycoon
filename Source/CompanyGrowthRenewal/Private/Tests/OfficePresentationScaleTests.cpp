#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Office/OfficeInterior.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace OfficePresentationScaleTests
{
class FScopedWorld
{
public:
	FScopedWorld()
	{
		TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
		if (TestWorld && GEngine)
		{
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(TestWorld);
		}
	}
	~FScopedWorld()
	{
		if (TestWorld)
		{
			if (GEngine) GEngine->DestroyWorldContext(TestWorld);
			TestWorld->DestroyWorld(false);
		}
	}
	UWorld* TestWorld = nullptr;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGROfficePresentationRoundTripTest,
	"CGR.Office.Presentation.CanonicalTransformRoundTrip",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGROfficePresentationRoundTripTest::RunTest(const FString& Parameters)
{
	OfficePresentationScaleTests::FScopedWorld TestScope;
	if (!TestNotNull(TEXT("test world"), TestScope.TestWorld)) return false;
	AOfficeInterior* Interior = TestScope.TestWorld->SpawnActor<AOfficeInterior>();
	if (!TestNotNull(TEXT("interior"), Interior)) return false;
	Interior->SetActorLocationAndRotation(FVector(1700, -600, 85), FRotator(0, 90, 0));
	Interior->ApplyPresentationScale();
	Interior->ApplyPresentationScale();
	TestTrue(TEXT("presentation application is idempotent"), Interior->GetActorScale3D().Equals(FVector(1.2), 0.00001));

	const FTransform Canonical(FRotator(0, 35, 0), FVector(100, -200, 50), FVector(0.9, 1.1, 0.8));
	FTransform Saved = Canonical;
	const FTransform WorldTransform = Interior->OfficeLocalToWorldTransform(Canonical);
	TestTrue(TEXT("translation rotates and scales exactly once"), WorldTransform.GetLocation().Equals(FVector(1940, -480, 145), 0.001));
	TestTrue(TEXT("authored furniture scale is preserved"), WorldTransform.GetScale3D().Equals(FVector(1.08, 1.32, 0.96), 0.001));
	TestTrue(TEXT("rotation composes with the room"), WorldTransform.GetRotation().Equals(FRotator(0, 125, 0).Quaternion(), 0.001));
	for (int32 Index = 0; Index < 100; ++Index)
	{
		Saved = Interior->WorldToOfficeLocalTransform(Interior->OfficeLocalToWorldTransform(Saved));
	}
	TestTrue(TEXT("repeated save and restore has no scale or position drift"), Saved.Equals(Canonical, 0.001));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGROfficePresentationNavBoundsTest,
	"CGR.Office.Presentation.NavBoundsSingleScale",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGROfficePresentationNavBoundsTest::RunTest(const FString& Parameters)
{
	OfficePresentationScaleTests::FScopedWorld TestScope;
	if (!TestNotNull(TEXT("test world"), TestScope.TestWorld)) return false;
	AOfficeInterior* Interior = TestScope.TestWorld->SpawnActor<AOfficeInterior>();
	if (!TestNotNull(TEXT("interior"), Interior)) return false;
	Interior->SetActorLocationAndRotation(FVector(700, -300, 45), FRotator(0, 31, 0));
	Interior->ApplyPresentationScale();
	UBoxComponent* NavBounds = Interior->FindComponentByClass<UBoxComponent>();
	if (!TestNotNull(TEXT("navigation bounds"), NavBounds)) return false;
	const FBox2D FloorBounds = Interior->GetCurrentFloorBoundsLocal();
	const FVector2D HalfSize = FloorBounds.GetExtent();
	TestTrue(TEXT("box extent stays canonical"), NavBounds->GetUnscaledBoxExtent().Equals(FVector(HalfSize.X, HalfSize.Y, 150), 0.001));
	TestTrue(TEXT("world extent inherits one 1.2 factor"), NavBounds->GetScaledBoxExtent().Equals(FVector(HalfSize.X * 1.2, HalfSize.Y * 1.2, 180), 0.001));
	const FVector2D Center = FloorBounds.GetCenter();
	const FVector ExpectedCenter = Interior->GetActorTransform().TransformPosition(FVector(Center.X, Center.Y, Interior->GetStructuralFloorZLocal() + 150));
	TestTrue(TEXT("navigation follows translated and rotated room"), NavBounds->GetComponentLocation().Equals(ExpectedCenter, 0.001));
	for (int32 Index = 0; Index < 32; ++Index)
	{
		const FVector LocalSpawn = Interior->GetActorTransform().InverseTransformPosition(Interior->GetRandomFloorLocation());
		TestTrue(TEXT("spawn stays on rotated floor"), FloorBounds.IsInside(FVector2D(LocalSpawn.X, LocalSpawn.Y)) && FMath::IsNearlyEqual(LocalSpawn.Z, static_cast<double>(Interior->GetStructuralFloorZLocal()), 0.001));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGROfficePresentationWallCoordinatesTest,
	"CGR.Office.Presentation.WallCoordinatesSingleScale",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGROfficePresentationWallCoordinatesTest::RunTest(const FString& Parameters)
{
	OfficePresentationScaleTests::FScopedWorld TestScope;
	if (!TestNotNull(TEXT("test world"), TestScope.TestWorld)) return false;
	AOfficeInterior* Interior = TestScope.TestWorld->SpawnActor<AOfficeInterior>();
	if (!TestNotNull(TEXT("interior"), Interior)) return false;
	Interior->SetActorLocation(FVector(1000, 2000, 30));
	Interior->ApplyPresentationScale();
	FVector WallMin, WallMax;
	Interior->GetWallPlacementBounds(EWallSide::Left, WallMin, WallMax);
	TestTrue(TEXT("left wall lower margins use room translation and one scale"), WallMin.Equals(FVector(1064.8, 1986.8, 90), 0.001));
	TestTrue(TEXT("left wall upper margins use one scale"), WallMax.Equals(FVector(1424.8, 1986.8, 474), 0.001));
	Interior->GetWallPlacementBounds(EWallSide::Right, WallMin, WallMax);
	TestTrue(TEXT("right wall spans its Y axis"), WallMin.Equals(FVector(1004.8, 1089.2, 90), 0.001));
	TestTrue(TEXT("right wall upper margins use one scale"), WallMax.Equals(FVector(1004.8, 1929.2, 474), 0.001));

	Interior->SetActorRotation(FRotator(0, 90, 0));
	const FTransform RoomTransform = Interior->GetActorTransform();
	const FVector HighHit = RoomTransform.TransformPosition(FVector(102.4, 90, 999));
	const FVector LowHit = RoomTransform.TransformPosition(FVector(90, -203, 0));
	const FVector LeftPosition = Interior->ConstrainWallDecorationLocation(EWallSide::Left, HighHit);
	const FVector RightPosition = Interior->ConstrainWallDecorationLocation(EWallSide::Right, LowHit);
	TestTrue(TEXT("left wall snaps in local space and clamps upper height"), RoomTransform.InverseTransformPosition(LeftPosition).Equals(FVector(100, -11, 370), 0.001));
	TestTrue(TEXT("wall switch uses right fixed axis and lower height"), RoomTransform.InverseTransformPosition(RightPosition).Equals(FVector(4, -205, 50), 0.001));
	const FVector NextPosition = Interior->ConstrainWallDecorationLocation(EWallSide::Left,
		RoomTransform.TransformPosition(FVector(107.4, 90, 999)));
	TestEqual(TEXT("canonical 5cm snap becomes 6cm world spacing"), FVector::Distance(LeftPosition, NextPosition), 6.0, 0.001);
	TestTrue(TEXT("wall normal follows room rotation"), Interior->GetWallNormal(EWallSide::Left).Equals(FVector(-1, 0, 0), 0.001));
	return true;
}

#endif
