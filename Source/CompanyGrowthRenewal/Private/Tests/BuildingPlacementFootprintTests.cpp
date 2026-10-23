#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Entity/Building/BuildingBaseActor.h"
#include "Misc/AutomationTest.h"
#include "Table/BuildingData.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace BuildingPlacementFootprintTests
{
struct FFixture
{
	UWorld* TestWorld = nullptr;
	ABuildingBaseActor* Building = nullptr;
	UStaticMeshComponent* Base = nullptr;
	UInstancedStaticMeshComponent* Body = nullptr;
	UInstancedStaticMeshComponent* Top = nullptr;
	UInstancedStaticMeshComponent* EmptyTop = nullptr;
	UStaticMesh* Cube = nullptr;

	FFixture()
	{
		TestWorld = UWorld::CreateWorld(EWorldType::EditorPreview, false);
		if (!TestWorld) { return; }
		if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::EditorPreview).SetCurrentWorld(TestWorld); }
		Building = TestWorld->SpawnActor<ABuildingBaseActor>();
		Cube = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube"))).LoadSynchronous();
		if (!Building || !Cube) { return; }
		Base = FindMesh<UStaticMeshComponent>(TEXT("MainMeshComponent"));
		Body = FindMesh<UInstancedStaticMeshComponent>(TEXT("Body_Module"));
		Top = FindMesh<UInstancedStaticMeshComponent>(TEXT("Top_Module"));
		EmptyTop = FindMesh<UInstancedStaticMeshComponent>(TEXT("Top_Empty_Module"));
	}

	~FFixture()
	{
		if (TestWorld)
		{
			if (GEngine) { GEngine->DestroyWorldContext(TestWorld); }
			TestWorld->DestroyWorld(false);
		}
	}

	bool IsReady() const { return Building && Base && Body && Top && EmptyTop && Cube; }

	template <typename T>
	T* FindMesh(const FName Name) const
	{
		const FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(ABuildingBaseActor::StaticClass(), Name);
		return Property ? Cast<T>(Property->GetObjectPropertyValue_InContainer(Building)) : nullptr;
	}

	UDataTable* GetBuildingTable() const
	{
		return FindMesh<UDataTable>(TEXT("BuildingDataTable"));
	}

	void SetNominalRow(UDataTable* Table, const FName RowName) const
	{
		FindFProperty<FObjectPropertyBase>(ABuildingBaseActor::StaticClass(), TEXT("BuildingDataTable"))
			->SetObjectPropertyValue_InContainer(Building, Table);
		FindFProperty<FNameProperty>(ABuildingBaseActor::StaticClass(), TEXT("BuildingID"))
			->SetPropertyValue_InContainer(Building, RowName);
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGRBuildingTightFootprintMeshTest, "CGR.Building.CommonFootprint.LogicalShapeAndYaw",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCGRBuildingTightFootprintMeshTest::RunTest(const FString& Parameters)
{
	AddInfo(TEXT("Common footprint: logical DT shape and yaw at 1863 cm per cell"));
	BuildingPlacementFootprintTests::FFixture Fixture;
	if (!TestTrue(TEXT("isolated building fixture"), Fixture.IsReady())) { return false; }
	UDataTable* Table = NewObject<UDataTable>(Fixture.Building);
	Table->RowStruct = FBuildingData::StaticStruct();
	FBuildingData Row;
	Row.FootprintWidthCells = 2;
	Row.FootprintDepthCells = 1;
	const FName RowName(TEXT("TestRectangle"));
	Table->AddRow(RowName, Row);
	Fixture.SetNominalRow(Table, RowName);
	const FVector2D LocalHalf(BuildingFootprintCellSize, BuildingFootprintCellSize * 0.5);
	TestEqual(TEXT("two by one DT shape uses the common cell"), Fixture.Building->GetPlacementFootprintHalfExtent(false), LocalHalf);
	Fixture.Base->SetStaticMesh(Fixture.Cube);
	Fixture.Base->SetRelativeScale3D(FVector(100.f, 100.f, 1.f));
	TestEqual(TEXT("visual mesh size does not change the shared footprint"), Fixture.Building->GetPlacementFootprintHalfExtent(false), LocalHalf);
	Fixture.Building->SetActorRotation(FRotator(0.f, 90.f, 0.f));
	TestEqual(TEXT("local shape stays unrotated"), Fixture.Building->GetPlacementFootprintHalfExtent(false), LocalHalf);
	TestTrue(TEXT("world AABB swaps the rectangle axes"), Fixture.Building->GetPlacementFootprintHalfExtent(true)
		.Equals(FVector2D(LocalHalf.Y, LocalHalf.X), 0.01));
	Fixture.Building->SetActorRotation(FRotator(0.f, 45.f, 0.f));
	const double DiagonalHalf = (LocalHalf.X + LocalHalf.Y) / FMath::Sqrt(2.0);
	TestTrue(TEXT("arbitrary yaw encloses the rotated rectangle"), Fixture.Building->GetPlacementFootprintHalfExtent(true)
		.Equals(FVector2D(DiagonalHalf, DiagonalHalf), 0.01));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGRBuildingTightFootprintBoundsTest, "CGR.Building.CommonFootprint.VisualCenter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCGRBuildingTightFootprintBoundsTest::RunTest(const FString& Parameters)
{
	AddInfo(TEXT("Common footprint: displaced visual center, active ISM bounds and ghost exclusion"));
	BuildingPlacementFootprintTests::FFixture Fixture;
	if (!TestTrue(TEXT("isolated building fixture"), Fixture.IsReady())) { return false; }
	Fixture.Building->SetActorLocation(FVector(300.f, -500.f, 1200.f));
	TestEqual(TEXT("missing body falls back to actor origin"), Fixture.Building->GetPlacementFootprintCenter(),
		FVector(300.f, -500.f, 1200.f));
	Fixture.Base->SetStaticMesh(Fixture.Cube);
	Fixture.Base->SetRelativeScale3D(FVector(10.f, 20.f, 1.f));
	Fixture.Base->SetRelativeLocation(FVector(200.f, -300.f, 0.f));
	TestTrue(TEXT("off-pivot body moves XY reservation center"), Fixture.Building->GetPlacementFootprintCenter()
		.Equals(FVector(500.f, -800.f, 1200.f), 0.01));
	Fixture.Body->SetStaticMesh(Fixture.Cube);
	Fixture.Body->AddInstance(FTransform(FVector(1000.f, 0.f, 0.f)));
	TestTrue(TEXT("active Body instance joins the visual bounds"), Fixture.Building->GetPlacementFootprintCenter()
		.Equals(FVector(675.f, -800.f, 1200.f), 0.01));
	Fixture.Top->SetStaticMesh(Fixture.Cube);
	Fixture.Top->SetRelativeLocation(FVector(100000.f, 0.f, 0.f));
	Fixture.EmptyTop->SetStaticMesh(Fixture.Cube);
	Fixture.EmptyTop->SetRelativeLocation(FVector(0.f, 100000.f, 0.f));
	UStaticMeshComponent* Ghost = Fixture.FindMesh<UStaticMeshComponent>(TEXT("GhostBoxSMC"));
	if (!TestNotNull(TEXT("ghost mesh component"), Ghost)) { return false; }
	Ghost->SetRelativeScale3D(FVector(10000.f));
	TestTrue(TEXT("empty ISMs and ghost do not shift the visual center"), Fixture.Building->GetPlacementFootprintCenter()
		.Equals(FVector(675.f, -800.f, 1200.f), 0.01));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGRBuildingTightFootprintFallbackTest, "CGR.Building.CommonFootprint.AllBuildingMeshesFit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCGRBuildingTightFootprintFallbackTest::RunTest(const FString& Parameters)
{
	AddInfo(TEXT("Common footprint: all 40 live DT mesh unions fit the shared 1863 cm cell"));
	BuildingPlacementFootprintTests::FFixture Fixture;
	if (!TestTrue(TEXT("isolated building fixture"), Fixture.IsReady())) { return false; }
	UDataTable* Table = Fixture.GetBuildingTable();
	if (!TestNotNull(TEXT("live BuildingDataTable"), Table)) { return false; }
	const TArray<FName> RowNames = Table->GetRowNames();
	if (!TestEqual(TEXT("all 40 live building rows are covered"), RowNames.Num(), 40)) { return false; }
	double MaximumRequiredCell = 0.0;
	for (const FName RowName : RowNames)
	{
		const FBuildingData* Row = Table->FindRow<FBuildingData>(RowName, TEXT("CommonFootprintAssetTest"));
		if (!TestNotNull(*RowName.ToString(), Row)) { continue; }
		Fixture.SetNominalRow(Table, RowName);
		Fixture.Base->SetStaticMesh(Row->BaseMesh.LoadSynchronous());
		Fixture.Body->ClearInstances();
		Fixture.Top->ClearInstances();
		Fixture.EmptyTop->ClearInstances();
		UInstancedStaticMeshComponent* Modules[] = {Fixture.Body, Fixture.Top, Fixture.EmptyTop};
		UStaticMesh* Meshes[] = {Row->BodyMesh.LoadSynchronous(), Row->TopMesh.LoadSynchronous(),
			Row->TopEmptyMesh.LoadSynchronous()};
		const FVector MeshScale(FMath::Max(0.05f, Row->MeshScale));
		Fixture.Base->SetRelativeScale3D(MeshScale);
		for (int32 Index = 0; Index < 3; ++Index)
		{
			Modules[Index]->SetStaticMesh(Meshes[Index]);
			Modules[Index]->SetRelativeScale3D(MeshScale);
			if (Meshes[Index]) { Modules[Index]->AddInstance(FTransform::Identity); }
		}
		const FBox VisualBounds = Fixture.Building->GetBuildingWorldBounds();
		if (!TestTrue(*FString::Printf(TEXT("%s body meshes loaded"), *RowName.ToString()), VisualBounds.IsValid != 0)) { continue; }
		const FVector ActualSize = VisualBounds.GetSize();
		MaximumRequiredCell = FMath::Max(MaximumRequiredCell,
			FMath::Max(ActualSize.X / FMath::Max(1, Row->FootprintWidthCells),
				ActualSize.Y / FMath::Max(1, Row->FootprintDepthCells)));
		const FVector2D Reservation = Fixture.Building->GetPlacementFootprintHalfExtent(false);
		const FString Label = FString::Printf(TEXT("%s four-module XY union fits its logical cells"), *RowName.ToString());
		TestTrue(*Label, ActualSize.X <= 2.0 * Reservation.X + 0.01
			&& ActualSize.Y <= 2.0 * Reservation.Y + 0.01);
		if (RowName == FName(TEXT("B12")))
		{
			TestTrue(TEXT("B12 visual center follows its off-pivot geometry"),
				Fixture.Building->GetPlacementFootprintCenter().X > 800.0);
		}
	}
	TestEqual(TEXT("the shared cell is the smallest whole centimetre enclosing every model"),
		BuildingFootprintCellSize, static_cast<float>(FMath::CeilToInt(MaximumRequiredCell)));
	return true;
}
#endif
