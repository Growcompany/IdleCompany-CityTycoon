#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Entity/Building/BuildingBaseActor.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Misc/AutomationTest.h"
#include "Player/PlayerCamera.h"
#include "SceneView.h"
#include "UObject/UnrealType.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
namespace OfficeBuildingFloorTests
{
struct FFixture
{
	UWorld* TestWorld = nullptr;
	ABuildingBaseActor* Building = nullptr;
	UStaticMeshComponent* Base = nullptr;
	UStaticMeshComponent* Body = nullptr;
	UStaticMeshComponent* Top = nullptr;
	UStaticMesh* BodyMesh = nullptr;
	UStaticMesh* TopMesh = nullptr;

	FFixture()
	{
		TestWorld = UWorld::CreateWorld(EWorldType::EditorPreview, false);
		if (!TestWorld) { return; }
		if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::EditorPreview).SetCurrentWorld(TestWorld); }
		Building = TestWorld->SpawnActor<ABuildingBaseActor>();
		UStaticMesh* Cube = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube"))).LoadSynchronous();
		if (!Building || !Cube) { return; }
		Base = FindMesh(TEXT("MainMeshComponent"));
		Body = FindMesh(TEXT("Body_Module"));
		Top = FindMesh(TEXT("Top_Module"));
		if (!Base || !Body || !Top) { return; }
		UStaticMesh* BaseMesh = DuplicateObject<UStaticMesh>(Cube, GetTransientPackage());
		BodyMesh = DuplicateObject<UStaticMesh>(Cube, GetTransientPackage());
		TopMesh = DuplicateObject<UStaticMesh>(Cube, GetTransientPackage());
		SetHeight(BaseMesh, 100.f);
		SetHeight(BodyMesh, 1200.f);
		SetHeight(TopMesh, 600.f);
		Base->SetStaticMesh(BaseMesh);
		Body->SetStaticMesh(BodyMesh);
		Top->SetStaticMesh(TopMesh);
		SetCopies(0);
		SetFloat(TEXT("Floor_Height_BodyModuleScale"), 1.f);
		SetFloat(TEXT("Standard_FloorHeight"), 4.f);
	}

	~FFixture()
	{
		if (TestWorld)
		{
			if (GEngine) { GEngine->DestroyWorldContext(TestWorld); }
			TestWorld->DestroyWorld(false);
		}
	}

	bool IsReady() const { return Building && Base && Body && Top && BodyMesh && TopMesh; }

	UStaticMeshComponent* FindMesh(const FName Name) const
	{
		const FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(ABuildingBaseActor::StaticClass(), Name);
		return Property ? Cast<UStaticMeshComponent>(Property->GetObjectPropertyValue_InContainer(Building)) : nullptr;
	}

	static void SetHeight(UStaticMesh* Mesh, const float Height)
	{
		Mesh->SetExtendedBounds(FBoxSphereBounds(FVector(0.f, 0.f, Height * 0.5f), FVector(50.f, 50.f, Height * 0.5f), Height));
	}

	void SetCopies(const int32 Copies) const
	{
		FindFProperty<FIntProperty>(ABuildingBaseActor::StaticClass(), TEXT("Body_Module_Copies"))->SetPropertyValue_InContainer(Building, Copies);
	}

	void SetFloat(const FName Name, const float Value) const
	{
		FindFProperty<FFloatProperty>(ABuildingBaseActor::StaticClass(), Name)->SetPropertyValue_InContainer(Building, Value);
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGROfficeBuildingFloorRoofTest, "CGR.Office.BuildingFloor.RoofIndependentLocalHeight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCGROfficeBuildingFloorRoofTest::RunTest(const FString& Parameters)
{
	OfficeBuildingFloorTests::FFixture Fixture;
	if (!TestTrue(TEXT("isolated building fixture"), Fixture.IsReady())) { return false; }
	float Height = -1.f;
	TestTrue(TEXT("normal Body floor is available"), Fixture.Building->TryGetOfficeFloorHeight(Height));
	TestEqual(TEXT("100cm base plus 1200cm Body minus 400cm floor"), Height, 900.f);
	TestEqual(TEXT("whole silhouette still includes roof"), Fixture.Building->GetTotalBuildingHeight(), 1900.f);
	Fixture.SetHeight(Fixture.TopMesh, 2400.f);
	TestTrue(TEXT("larger rooftop equipment keeps floor available"), Fixture.Building->TryGetOfficeFloorHeight(Height));
	TestEqual(TEXT("equipment height cannot raise Office floor"), Height, 900.f);
	TestEqual(TEXT("whole silhouette still grows with equipment"), Fixture.Building->GetTotalBuildingHeight(), 3700.f);
	Fixture.Building->SetActorTransform(FTransform(FRotator(0.f, 71.f, 0.f), FVector(300.f, -500.f, 1200.f), FVector(2.f, 3.f, 4.f)));
	TestTrue(TEXT("transformed building keeps local floor available"), Fixture.Building->TryGetOfficeFloorHeight(Height));
	TestEqual(TEXT("actor elevation and scale are left to Snapshot"), Height, 900.f);
	Fixture.Top->SetStaticMesh(nullptr);
	TestTrue(TEXT("roof mesh is not needed to identify the Body floor"), Fixture.Building->TryGetOfficeFloorHeight(Height));
	TestEqual(TEXT("missing roof does not change Body floor"), Height, 900.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGROfficeBuildingFloorGrowthTest, "CGR.Office.BuildingFloor.GrowthScaleAndShortBody",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCGROfficeBuildingFloorGrowthTest::RunTest(const FString& Parameters)
{
	OfficeBuildingFloorTests::FFixture Fixture;
	if (!TestTrue(TEXT("isolated building fixture"), Fixture.IsReady())) { return false; }
	float Height = -1.f;
	Fixture.SetCopies(2);
	TestTrue(TEXT("three stacked Body modules have a representative floor"), Fixture.Building->TryGetOfficeFloorHeight(Height));
	TestEqual(TEXT("two extensions each add one 1200cm Body"), Height, 3300.f);
	Fixture.SetFloat(TEXT("Floor_Height_BodyModuleScale"), 0.5f);
	TestTrue(TEXT("scaled Body has a representative floor"), Fixture.Building->TryGetOfficeFloorHeight(Height));
	TestEqual(TEXT("Body stack and floor height both use Body scale"), Height, 1700.f);
	Fixture.SetCopies(0);
	Fixture.SetHeight(Fixture.BodyMesh, 200.f);
	TestTrue(TEXT("short Body clamps the general floor height"), Fixture.Building->TryGetOfficeFloorHeight(Height));
	TestEqual(TEXT("short single Body floor begins at the base top"), Height, 100.f);
	Fixture.SetFloat(TEXT("Standard_FloorHeight"), 1.f);
	TestTrue(TEXT("explicit shorter standard floor is honored"), Fixture.Building->TryGetOfficeFloorHeight(Height));
	TestEqual(TEXT("one meter standard floor is scaled to 50cm"), Height, 150.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGROfficeBuildingFloorInvalidTest, "CGR.Office.BuildingFloor.InvalidPreservesOutput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCGROfficeBuildingFloorInvalidTest::RunTest(const FString& Parameters)
{
	OfficeBuildingFloorTests::FFixture Fixture;
	if (!TestTrue(TEXT("isolated building fixture"), Fixture.IsReady())) { return false; }
	auto CheckInvalid = [this, &Fixture](const TCHAR* Label)
	{
		float Height = 12345.f;
		TestFalse(Label, Fixture.Building->TryGetOfficeFloorHeight(Height));
		TestEqual(TEXT("invalid input preserves caller output"), Height, 12345.f);
	};
	for (const FName PropertyName : {FName(TEXT("Floor_Height_BodyModuleScale")), FName(TEXT("Standard_FloorHeight"))})
	{
		for (const float Invalid : {0.f, -1.f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()})
		{
			Fixture.SetFloat(PropertyName, Invalid);
			CheckInvalid(TEXT("non-positive or non-finite floor configuration is rejected"));
		}
		Fixture.SetFloat(PropertyName, PropertyName == FName(TEXT("Standard_FloorHeight")) ? 4.f : 1.f);
	}
	Fixture.SetCopies(-1);
	CheckInvalid(TEXT("at least one Body module is required"));
	Fixture.SetCopies(0);
	Fixture.SetHeight(Fixture.BodyMesh, 0.f);
	CheckInvalid(TEXT("zero height Body is rejected"));
	Fixture.SetHeight(Fixture.BodyMesh, 1200.f);
	Fixture.Body->SetStaticMesh(nullptr);
	CheckInvalid(TEXT("missing Body mesh is rejected"));
	Fixture.Body->SetStaticMesh(Fixture.BodyMesh);
	Fixture.Base->SetStaticMesh(nullptr);
	CheckInvalid(TEXT("missing base mesh is rejected"));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGRBuildingWorldHeightTest, "CGR.Building.WorldHeight.TransformedMeshBounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCGRBuildingWorldHeightTest::RunTest(const FString& Parameters)
{
	OfficeBuildingFloorTests::FFixture Fixture;
	if (!TestTrue(TEXT("isolated building fixture"), Fixture.IsReady())) { return false; }
	UInstancedStaticMeshComponent* BodyInstances = CastChecked<UInstancedStaticMeshComponent>(Fixture.Body);
	UInstancedStaticMeshComponent* RoofInstances = CastChecked<UInstancedStaticMeshComponent>(Fixture.Top);
	BodyInstances->ClearInstances();
	RoofInstances->ClearInstances();
	BodyInstances->AddInstance(FTransform(FVector(0.f, 0.f, 100.f)));
	RoofInstances->AddInstance(FTransform(FVector(0.f, 0.f, 1300.f)));
	Fixture.Building->SetActorTransform(FTransform(FRotator(0.f, 37.f, 0.f), FVector(300.f, -500.f, 1200.f), FVector(0.7f)));
	Fixture.Base->UpdateBounds();
	BodyInstances->UpdateBounds();
	RoofInstances->UpdateBounds();

	TestEqual(TEXT("actor scale applies to the complete 1900cm silhouette"),
		Fixture.Building->GetTotalBuildingHeightWorld(), 1330.f, 0.01f);
	TestEqual(TEXT("bubble rests on the roof above the elevated base"),
		Fixture.Building->GetBubbleAnchorPosition().Z, 2530.0, 0.01);

	for (UStaticMeshComponent* MeshComponent : {Fixture.Base, Fixture.Body, Fixture.Top})
	{
		MeshComponent->SetRelativeScale3D(FVector(0.5f));
		MeshComponent->UpdateBounds();
	}
	TestEqual(TEXT("component and actor scale are each applied once"),
		Fixture.Building->GetTotalBuildingHeightWorld(), 665.f, 0.01f);
	TestEqual(TEXT("bubble follows the transformed roof"),
		Fixture.Building->GetBubbleAnchorPosition().Z, 1865.0, 0.01);
	RoofInstances->ClearInstances();
	RoofInstances->AddInstance(FTransform(FVector(0.f, 0.f, 2200.f)));
	TestEqual(TEXT("same-frame roof restack does not wait for render bounds"),
		Fixture.Building->GetTotalBuildingHeightWorld(), 980.f, 0.01f);
	TestEqual(TEXT("bubble follows a restacked roof in the same frame"),
		Fixture.Building->GetBubbleAnchorPosition().Z, 2180.0, 0.01);

	UStaticMeshComponent* Ghost = Fixture.FindMesh(TEXT("GhostBoxSMC"));
	if (!TestNotNull(TEXT("ghost component"), Ghost)) { return false; }
	Ghost->SetRelativeLocation(FVector(0.f, 0.f, 100000.f));
	Ghost->SetRelativeScale3D(FVector(100.f));
	Ghost->UpdateBounds();
	TestEqual(TEXT("auxiliary ghost bounds cannot raise the roof"),
		Fixture.Building->GetTotalBuildingHeightWorld(), 980.f, 0.01f);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGRBuildingCameraProjectionTest, "CGR.Camera.BuildingFocus.ActualRigProjection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCGRBuildingCameraProjectionTest::RunTest(const FString& Parameters)
{
	OfficeBuildingFloorTests::FFixture Fixture;
	if (!TestTrue(TEXT("isolated building fixture"), Fixture.IsReady())) { return false; }
	APlayerController* TestController = Fixture.TestWorld->SpawnActor<APlayerController>();
	APlayerCamera* CameraPawn = Fixture.TestWorld->SpawnActor<APlayerCamera>();
	if (!TestNotNull(TEXT("controller"), TestController) || !TestNotNull(TEXT("camera pawn"), CameraPawn)) { return false; }
	Fixture.TestWorld->AddController(TestController);
	UInstancedStaticMeshComponent* BodyInstances = CastChecked<UInstancedStaticMeshComponent>(Fixture.Body);
	UInstancedStaticMeshComponent* RoofInstances = CastChecked<UInstancedStaticMeshComponent>(Fixture.Top);
	BodyInstances->ClearInstances();
	RoofInstances->ClearInstances();
	Fixture.Building->SetActorLocation(FVector(300.f, -500.f, 1200.f));
	const auto ProjectPoint = [this, CameraPawn](const FVector& WorldPoint, const FIntRect& ViewRect)
	{
		FMinimalViewInfo ViewInfo;
		CameraPawn->CalcCamera(0.f, ViewInfo);
		FSceneViewProjectionData ProjectionData;
		ProjectionData.SetViewRectangle(ViewRect);
		ProjectionData.ViewOrigin = ViewInfo.Location;
		ProjectionData.ViewRotationMatrix = FInverseRotationMatrix(ViewInfo.Rotation) * FMatrix(
			FPlane(0, 0, 1, 0), FPlane(1, 0, 0, 0),
			FPlane(0, 1, 0, 0), FPlane(0, 0, 0, 1));
		FMinimalViewInfo::CalculateProjectionMatrixGivenViewRectangle(
			ViewInfo, AspectRatio_MaintainYFOV, ViewRect, ProjectionData);
		FVector2D ScreenPosition = FVector2D::ZeroVector;
		TestTrue(TEXT("world point projects through the engine view matrix"),
			FSceneView::ProjectWorldToScreen(WorldPoint, ProjectionData.GetConstrainedViewRect(),
				ProjectionData.ComputeViewProjectionMatrix(), ScreenPosition, true));
		return FVector2D(
			(ScreenPosition.X - ViewRect.Min.X) / ViewRect.Width(),
			(ScreenPosition.Y - ViewRect.Min.Y) / ViewRect.Height());
	};
	const auto ProjectForwardRay = [&ProjectPoint, CameraPawn](const FIntRect& ViewRect)
	{
		FMinimalViewInfo ViewInfo;
		CameraPawn->CalcCamera(0.f, ViewInfo);
		return ProjectPoint(ViewInfo.Location + ViewInfo.Rotation.Vector() * 10000.f, ViewRect);
	};
	const auto AdvanceNaturalTicks = [CameraPawn]()
	{
		for (int32 Frame = 0; Frame < 180; ++Frame)
		{
			CameraPawn->Tick(1.f / 60.f);
		}
	};
	float PreviousArmLength = 0.f;
	for (const float Height : {5000.f, 10000.f, 25000.f})
	{
		BodyInstances->ClearInstances();
		RoofInstances->ClearInstances();
		BodyInstances->AddInstance(FTransform(FQuat::Identity, FVector(0.f, 0.f, 100.f), FVector(1.f, 1.f, (Height - 700.f) / 1200.f)));
		RoofInstances->AddInstance(FTransform(FVector(0.f, 0.f, Height - 600.f)));
		if (!TestTrue(TEXT("building focus settles"), CameraPawn->SnapFocusOnBuilding(Fixture.Building, 0.3f))) { return false; }
		FMinimalViewInfo ReusedViewInfo;
		CameraPawn->CalcCamera(0.f, ReusedViewInfo);
		const FMinimalViewInfo FirstViewInfo = ReusedViewInfo;
		CameraPawn->CalcCamera(0.f, ReusedViewInfo);
		TestEqual(TEXT("repeated CalcCamera preserves lens offset"),
			ReusedViewInfo.OffCenterProjectionOffset.X, FirstViewInfo.OffCenterProjectionOffset.X);
		TestTrue(TEXT("repeated CalcCamera preserves camera location"),
			ReusedViewInfo.Location.Equals(FirstViewInfo.Location, 0.001));
		TestTrue(TEXT("repeated CalcCamera preserves camera rotation"),
			ReusedViewInfo.Rotation.Equals(FirstViewInfo.Rotation, 0.001));
		TestEqual(TEXT("repeated CalcCamera preserves FOV"), ReusedViewInfo.FOV, FirstViewInfo.FOV);
		const FVector BasePoint = Fixture.Building->GetActorLocation();
		const FVector RoofPoint = BasePoint + FVector(0.f, 0.f, Height);
		for (const FIntRect& ViewRect : {FIntRect(0, 0, 1920, 1080), FIntRect(0, 0, 2400, 1080)})
		{
			const FVector2D ProjectedRoof = ProjectPoint(RoofPoint, ViewRect);
			const FVector2D ProjectedBase = ProjectPoint(BasePoint, ViewRect);
			TestEqual(TEXT("roof occupies requested horizontal coordinate"), ProjectedRoof.X, 0.3, 0.001);
			TestEqual(TEXT("base occupies requested horizontal coordinate"), ProjectedBase.X, 0.3, 0.001);
			TestEqual(TEXT("roof has 20 percent top margin"), ProjectedRoof.Y, 0.20, 0.001);
			if (Height < 25000.f)
			{
				TestEqual(TEXT("base has 10 percent bottom margin"), ProjectedBase.Y, 0.90, 0.001);
			}
			else
			{
				TestTrue(TEXT("overflow keeps roof visible and lets base leave screen"), ProjectedBase.Y > 1.0);
			}
		}
		TestTrue(TEXT("new building height recalculates the target zoom"), CameraPawn->SpringArm->TargetArmLength > PreviousArmLength);
		PreviousArmLength = CameraPawn->SpringArm->TargetArmLength;
	}
	constexpr float OffPivotHeight = 5000.f;
	const FVector MeshOffset(800.f, -350.f, 0.f);
	Fixture.Base->SetRelativeLocation(MeshOffset);
	BodyInstances->ClearInstances();
	RoofInstances->ClearInstances();
	BodyInstances->AddInstance(FTransform(FQuat::Identity, MeshOffset + FVector(0.f, 0.f, 100.f),
		FVector(1.f, 1.f, (OffPivotHeight - 700.f) / 1200.f)));
	RoofInstances->AddInstance(FTransform(MeshOffset + FVector(0.f, 0.f, OffPivotHeight - 600.f)));
	const FVector VisualBase = Fixture.Building->GetPlacementFootprintCenter();
	TestTrue(TEXT("off-pivot building has a visual center distinct from actor origin"),
		FVector::Dist2D(VisualBase, Fixture.Building->GetActorLocation()) > 200.f);
	if (!TestTrue(TEXT("off-pivot building focus settles"), CameraPawn->SnapFocusOnBuilding(Fixture.Building, 0.3f))) { return false; }
	const FVector VisualRoof = VisualBase + FVector(0.f, 0.f, OffPivotHeight);
	for (const FIntRect& ViewRect : {FIntRect(0, 0, 1920, 1080), FIntRect(0, 0, 2400, 1080)})
	{
		TestEqual(TEXT("off-pivot visual base occupies requested horizontal coordinate"),
			ProjectPoint(VisualBase, ViewRect).X, 0.3, 0.001);
		TestEqual(TEXT("off-pivot visual roof occupies requested horizontal coordinate"),
			ProjectPoint(VisualRoof, ViewRect).X, 0.3, 0.001);
		TestTrue(TEXT("off-pivot actor origin stays distinct from visual focus"),
			FMath::Abs(ProjectPoint(Fixture.Building->GetActorLocation(), ViewRect).X - 0.3) > 0.01);
	}
	const FIntRect WideViewRect(0, 0, 2400, 1080);
	const FVector FocusedPawnLocation = CameraPawn->GetActorLocation();
	const float FocusedArmLength = CameraPawn->SpringArm->TargetArmLength;
	CameraPawn->ClearFocusTarget();
	AdvanceNaturalTicks();
	TestEqual(TEXT("closing focus restores neutral projection"), ProjectForwardRay(WideViewRect).X, 0.5, 0.001);
	TestTrue(TEXT("closing focus does not move the pawn"), CameraPawn->GetActorLocation().Equals(FocusedPawnLocation, 0.001));
	TestEqual(TEXT("closing focus does not change zoom"), CameraPawn->SpringArm->TargetArmLength, FocusedArmLength, 0.001f);
	CameraPawn->RestoreFocusTarget(Fixture.Building);
	AdvanceNaturalTicks();
	TestEqual(TEXT("modal return restores building projection"), ProjectForwardRay(WideViewRect).X, 0.3, 0.001);
	TestTrue(TEXT("modal return does not move the pawn"), CameraPawn->GetActorLocation().Equals(FocusedPawnLocation, 0.001));
	TestEqual(TEXT("modal return does not change zoom"), CameraPawn->SpringArm->TargetArmLength, FocusedArmLength, 0.001f);
	CameraPawn->FocusOnActor(Fixture.Building, 0.5f, false);
	AdvanceNaturalTicks();
	TestEqual(TEXT("generic actor focus clears building projection"), ProjectForwardRay(WideViewRect).X, 0.5, 0.001);
	CameraPawn->SnapFocusOnBuilding(Fixture.Building, 0.3f);
	CameraPawn->FocusOnLocation(Fixture.Building->GetActorLocation());
	AdvanceNaturalTicks();
	TestEqual(TEXT("generic location focus clears building projection"), ProjectForwardRay(WideViewRect).X, 0.5, 0.001);
	CameraPawn->SnapFocusOnBuilding(Fixture.Building, 0.3f);
	CameraPawn->FocusOnBuildingForPlacement(Fixture.Building, 15000.f);
	AdvanceNaturalTicks();
	TestEqual(TEXT("placement focus clears building projection"), ProjectForwardRay(WideViewRect).X, 0.5, 0.001);
	return true;
}
#endif
