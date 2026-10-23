#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Entity/Door/InteractiveDoorActor.h"
#include "Misc/AutomationTest.h"
#include "Office/OfficeInterior.h"
#include "Office/OfficeTelevision.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace OfficeEntrancePlacementTests
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
			if (GEngine) { GEngine->DestroyWorldContext(TestWorld); }
			TestWorld->DestroyWorld(false);
		}
	}

	UWorld* Get() const { return TestWorld; }

private:
	UWorld* TestWorld = nullptr;
};

FVector GetDoorOfficeLocal(const AOfficeInterior* Interior)
{
	return Interior->GetActorTransform().InverseTransformPosition(
		Interior->GetRecruitmentDoor()->GetActorLocation());
}

void CheckDoorwayCases(FAutomationTestBase& Test, AOfficeInterior* Interior, const TCHAR* Phase)
{
	const FVector DoorLocal = GetDoorOfficeLocal(Interior);
	const FBox Reserved = Interior->GetRecruitmentEntranceExclusionBoundsLocal();
	Test.TestTrue(FString::Printf(TEXT("%s: real door has valid exclusion"), Phase), Reserved.IsValid != 0);
	if (!Reserved.IsValid) { return; }

	// The door actor supplies the fixture center; the reserved-box edge supplies a separate overlap boundary.
	const FBox PictureAtDoor(DoorLocal + FVector(-18, -8, 75), DoorLocal + FVector(18, 8, 155));
	Test.TestTrue(FString::Printf(TEXT("%s: picture on the doorway wall is blocked"), Phase),
		Interior->IsRecruitmentEntranceBlocked(PictureAtDoor));

	const double OutsideX = Reserved.Max.X + 6.0;
	const FBox EdgePicture(FVector(OutsideX - 14.0, DoorLocal.Y - 8.0, DoorLocal.Z + 75.0),
		FVector(OutsideX + 14.0, DoorLocal.Y + 8.0, DoorLocal.Z + 155.0));
	Test.TestTrue(FString::Printf(TEXT("%s: picture with pivot outside but edge inside is blocked"), Phase),
		Interior->IsRecruitmentEntranceBlocked(EdgePicture));

	const FBox ClearPicture(FVector(Reserved.Max.X + 55.0, DoorLocal.Y - 8.0, DoorLocal.Z + 75.0),
		FVector(Reserved.Max.X + 85.0, DoorLocal.Y + 8.0, DoorLocal.Z + 155.0));
	Test.TestFalse(FString::Printf(TEXT("%s: picture farther along wall is clear"), Phase),
		Interior->IsRecruitmentEntranceBlocked(ClearPicture));

	const FBox2D DoorwayFootprint(FVector2D(DoorLocal.X - 20.0, DoorLocal.Y - 75.0),
		FVector2D(DoorLocal.X + 20.0, DoorLocal.Y - 35.0));
	Test.TestTrue(FString::Printf(TEXT("%s: floor object near doorway is blocked"), Phase),
		Interior->IsRecruitmentEntranceFloorBlocked(DoorwayFootprint));

	const FBox2D ClearFootprint(FVector2D(Reserved.Max.X + 55.0, DoorLocal.Y - 75.0),
		FVector2D(Reserved.Max.X + 105.0, DoorLocal.Y - 35.0));
	Test.TestFalse(FString::Printf(TEXT("%s: floor object away from doorway is clear"), Phase),
		Interior->IsRecruitmentEntranceFloorBlocked(ClearFootprint));
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGROfficeEntrancePlacementTest,
	"CGR.Office.Entrance.PlacementExclusion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCGROfficeEntrancePlacementTest::RunTest(const FString&)
{
	OfficeEntrancePlacementTests::FScopedWorld WorldScope;
	UWorld* TestWorld = WorldScope.Get();
	if (!TestNotNull(TEXT("transient game world"), TestWorld)) { return false; }

	const TCHAR* OfficeClassPath = TEXT("/Game/CompanyGrowth/Blueprint/Office/BP_OfficeInterior.BP_OfficeInterior_C");
	UClass* OfficeClass = LoadClass<AOfficeInterior>(nullptr, OfficeClassPath);
	if (!TestNotNull(TEXT("real BP_OfficeInterior class"), OfficeClass)) { return false; }
	AOfficeInterior* Interior = TestWorld->SpawnActor<AOfficeInterior>(OfficeClass);
	if (!TestNotNull(TEXT("real office interior"), Interior)) { return false; }
	if (!TestNotNull(TEXT("real recruitment door child actor"), Interior->GetRecruitmentDoor())) { return false; }

	OfficeEntrancePlacementTests::CheckDoorwayCases(*this, Interior, TEXT("initial office"));
	const FBox InitialReserved = Interior->GetRecruitmentEntranceExclusionBoundsLocal();
	TArray<USceneComponent*> DoorComponents;
	Interior->GetRecruitmentDoor()->GetComponents<USceneComponent>(DoorComponents);
	const FNameProperty* HingeNameProperty = FindFProperty<FNameProperty>(AInteractiveDoorActor::StaticClass(), TEXT("HingeComponentName"));
	if (!TestNotNull(TEXT("door hinge configuration"), HingeNameProperty)) { return false; }
	const FName HingeName = HingeNameProperty->GetPropertyValue_InContainer(Interior->GetRecruitmentDoor());
	USceneComponent* DoorHinge = nullptr;
	for (USceneComponent* DoorComponent : DoorComponents)
	{
		if (DoorComponent && DoorComponent->GetFName() == HingeName)
		{
			DoorHinge = DoorComponent;
			break;
		}
	}
	if (!TestNotNull(TEXT("authored door hinge"), DoorHinge)) { return false; }
	const FRotator ClosedHingeRotation = DoorHinge->GetRelativeRotation();
	DoorHinge->SetRelativeRotation(ClosedHingeRotation + FRotator(0, 75, 0));
	const FBox OpenLeafReserved = Interior->GetRecruitmentEntranceExclusionBoundsLocal();
	TestTrue(TEXT("moving the actual door leaf keeps fixed entrance reservation"),
		InitialReserved.IsValid && OpenLeafReserved.IsValid
		&& InitialReserved.Min.Equals(OpenLeafReserved.Min, 0.01)
		&& InitialReserved.Max.Equals(OpenLeafReserved.Max, 0.01));
	DoorHinge->SetRelativeRotation(ClosedHingeRotation);

	Interior->SetActorLocationAndRotation(FVector(1230, -740, 95), FRotator(0, 31, 0));
	Interior->ApplyPresentationScale();
	TestTrue(TEXT("presentation scale is applied"),
		Interior->GetActorScale3D().Equals(FVector(AOfficeInterior::PresentationScale), 0.001));
	OfficeEntrancePlacementTests::CheckDoorwayCases(*this, Interior, TEXT("rotated scaled office"));
	const FBox RotatedReserved = Interior->GetRecruitmentEntranceExclusionBoundsLocal();
	TestTrue(TEXT("world rotation and presentation scale do not change local entrance bounds"),
		InitialReserved.IsValid && RotatedReserved.IsValid
		&& InitialReserved.Min.Equals(RotatedReserved.Min, 0.01)
		&& InitialReserved.Max.Equals(RotatedReserved.Max, 0.01));

	FOfficeSaveData ExpandedSave;
	ExpandedSave.TileCountX = 5;
	ExpandedSave.TileCountY = 6;
	Interior->ApplyOfficeSaveData(ExpandedSave);
	TestEqual(TEXT("expanded width restored"), Interior->GetTileCount().X, 5);
	TestEqual(TEXT("expanded depth restored"), Interior->GetTileCount().Y, 6);
	OfficeEntrancePlacementTests::CheckDoorwayCases(*this, Interior, TEXT("expanded office"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGROfficeEntranceMissingDoorTest,
	"CGR.Office.Entrance.MissingDoorDoesNotReserveSpace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCGROfficeEntranceMissingDoorTest::RunTest(const FString&)
{
	OfficeEntrancePlacementTests::FScopedWorld WorldScope;
	UWorld* TestWorld = WorldScope.Get();
	if (!TestNotNull(TEXT("transient game world"), TestWorld)) { return false; }
	AOfficeInterior* Interior = TestWorld->SpawnActor<AOfficeInterior>();
	if (!TestNotNull(TEXT("native office interior"), Interior)) { return false; }
	TestNull(TEXT("native office has no door child"), Interior->GetRecruitmentDoor());
	TestFalse(TEXT("native office has no entrance reservation"),
		Interior->GetRecruitmentEntranceExclusionBoundsLocal().IsValid != 0);
	TestFalse(TEXT("invalid entrance does not block a wall object"),
		Interior->IsRecruitmentEntranceBlocked(FBox(FVector(100, -10, 70), FVector(140, 10, 180))));
	TestFalse(TEXT("invalid entrance does not block a floor object"),
		Interior->IsRecruitmentEntranceFloorBlocked(FBox2D(FVector2D(100, -100), FVector2D(140, -60))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGROfficeTelevisionPlacementTest,
	"CGR.Office.Television.FixtureBounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCGROfficeTelevisionPlacementTest::RunTest(const FString&)
{
	OfficeEntrancePlacementTests::FScopedWorld WorldScope;
	UWorld* TestWorld = WorldScope.Get();
	if (!TestNotNull(TEXT("transient game world"), TestWorld)) { return false; }
	UClass* OfficeClass = LoadClass<AOfficeInterior>(nullptr,
		TEXT("/Game/CompanyGrowth/Blueprint/Office/BP_OfficeInterior.BP_OfficeInterior_C"));
	if (!TestNotNull(TEXT("authored office class"), OfficeClass)) { return false; }
	AOfficeInterior* Interior = TestWorld->SpawnActor<AOfficeInterior>(OfficeClass);
	if (!TestNotNull(TEXT("authored office"), Interior)) { return false; }
	for (const FIntPoint Size : { FIntPoint(1, 2), FIntPoint(2, 3), FIntPoint(5, 6) })
	{
		FOfficeSaveData OfficeData;
		OfficeData.TileCountX = Size.X;
		OfficeData.TileCountY = Size.Y;
		Interior->ApplyOfficeSaveData(OfficeData);
		const FBox ScreenBounds = AOfficeTelevision::GetScreenBoundsLocal(Interior);
		const FBox Reserved = AOfficeTelevision::GetExclusionBoundsLocal(Interior);
		const FBox2D FloorBounds = Interior->GetCurrentFloorBoundsLocal();
		TestTrue(TEXT("TV exists at every supported footprint"), ScreenBounds.IsValid != 0);
		TestFalse(TEXT("screen does not cover the door or its approach"), Interior->IsRecruitmentEntranceBlocked(Reserved));
		TestTrue(TEXT("screen retains visible wall margins"), Reserved.Min.X > FloorBounds.Min.X && Reserved.Max.X < FloorBounds.Max.X
			&& Reserved.Min.Z > Interior->GetStructuralFloorZLocal()
			&& Reserved.Max.Z < Interior->GetStructuralFloorZLocal() + Interior->GetWallHeight());
		TestTrue(TEXT("art covering the TV is rejected"), Interior->IsTelevisionPlacementBlocked(ScreenBounds));
		const FBox LowFurniture(FVector(ScreenBounds.Min.X, ScreenBounds.Min.Y, Interior->GetStructuralFloorZLocal()),
			FVector(ScreenBounds.Max.X, ScreenBounds.Max.Y, Reserved.Min.Z - 1.0));
		TestFalse(TEXT("furniture below screen remains allowed"), Interior->IsTelevisionPlacementBlocked(LowFurniture));
		Interior->SetActorLocationAndRotation(FVector(1200, -700, 80), FRotator(0, 31, 0));
		Interior->ApplyPresentationScale();
		TestTrue(TEXT("world scale and rotation preserve local reservation"), Reserved.Equals(AOfficeTelevision::GetExclusionBoundsLocal(Interior), 0.01));
	}
	return true;
}

#endif
