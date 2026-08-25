#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/Material.h"
#include "Misc/AutomationTest.h"
#include "Office/OfficeExteriorLayout.h"
#include "Office/OfficeExteriorShell.h"
#include "Office/OfficeFootprintGeometry.h"
#include "Office/OfficeInterior.h"
#include "UObject/UnrealType.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

namespace OfficeExteriorLayoutTests
{
constexpr float TransformTolerance = 0.1f;

FOfficeExteriorLayoutInput MakeInput(
	const FIntPoint TileCount,
	const FIntPoint MaxTileCount = FIntPoint(5, 6))
{
	FOfficeExteriorLayoutInput Input;
	Input.TileCount = TileCount;
	Input.MaxTileCount = MaxTileCount;
	Input.CurrentFloorBounds = FOfficeFootprintGeometry::MakeFloorBounds(TileCount, 400.f);
	Input.MaxFloorBounds = FOfficeFootprintGeometry::MakeFloorBounds(MaxTileCount, 400.f);
	Input.TileSizeCm = 400.f;
	Input.StructuralFloorZ = 0.f;
	return Input;
}

bool ContainsTransform(const TArray<FTransform>& Transforms, const FTransform& Expected)
{
	return Transforms.ContainsByPredicate(
		[&Expected](const FTransform& Candidate)
		{
			return Candidate.Equals(Expected, TransformTolerance);
		});
}

bool IsIntegerWithinTolerance(const float Value)
{
	return FMath::IsNearlyEqual(Value, FMath::RoundToFloat(Value), KINDA_SMALL_NUMBER);
}

float DistanceBetweenBoxes(const FBox2D& A, const FBox2D& B)
{
	const double DistanceX = FMath::Max3(A.Min.X - B.Max.X, B.Min.X - A.Max.X, 0.0);
	const double DistanceY = FMath::Max3(A.Min.Y - B.Max.Y, B.Min.Y - A.Max.Y, 0.0);
	return FVector2D(DistanceX, DistanceY).Size();
}

class FScopedTestWorld
{
public:
	explicit FScopedTestWorld(const EWorldType::Type WorldType = EWorldType::Game)
	{
		TestWorld = UWorld::CreateWorld(WorldType, false);
		if (TestWorld && GEngine)
		{
			FWorldContext& TestWorldContext = GEngine->CreateNewWorldContext(WorldType);
			TestWorldContext.SetCurrentWorld(TestWorld);
		}
	}

	~FScopedTestWorld()
	{
		if (!TestWorld)
		{
			return;
		}

		if (GEngine)
		{
			GEngine->DestroyWorldContext(TestWorld);
		}
		TestWorld->DestroyWorld(false);
	}

	UWorld* Get() const
	{
		return TestWorld;
	}

private:
	UWorld* TestWorld = nullptr;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROfficeExteriorLayoutGeometryTest,
	"CGR.Office.Exterior.Layout.Geometry",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROfficeExteriorLayoutValidationTest,
	"CGR.Office.Exterior.Layout.Validation",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROfficeExteriorLayoutExpansionTest,
	"CGR.Office.Exterior.Layout.GridAndExpansionStability",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROfficeExteriorLayoutSkylineTest,
	"CGR.Office.Exterior.Layout.SkylineSeparation",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROfficeExteriorShellComponentContractTest,
	"CGR.Office.Exterior.Layout.ActorComponentContract",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROfficeExteriorCanonicalMaximumTest,
	"CGR.Office.Exterior.Layout.CanonicalMaximum",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROfficeExteriorSupportedRangeTest,
	"CGR.Office.Exterior.Layout.SupportedRange",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROfficeExteriorFacadeMaterialSlotsTest,
	"CGR.Office.Exterior.Layout.FacadeMaterialSlots",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROfficeExteriorTowerHeightPropertyTest,
	"CGR.Office.Exterior.Layout.TowerHeightProperty",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROfficeExteriorTowerHeightGeometryTest,
	"CGR.Office.Exterior.Layout.TowerHeightGeometry",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGROfficeExteriorLayoutGeometryTest::RunTest(const FString& Parameters)
{
	FOfficeExteriorLayoutResult MaximumResult;
	TestTrue(
		TEXT("maximum layout builds"),
		FOfficeExteriorLayoutBuilder::Build(
			OfficeExteriorLayoutTests::MakeInput(FIntPoint(5, 6)),
			MaximumResult));
	TestEqual(TEXT("44 facade bays"), MaximumResult.FacadeBayTransforms.Num(), 44);
	TestEqual(TEXT("8 floor bands"), MaximumResult.FloorBandTransforms.Num(), 8);
	TestEqual(TEXT("3 corners and 2 trims"), MaximumResult.CornerAndTrimTransforms.Num(), 5);
	TestEqual(TEXT("57 dynamic instances"), MaximumResult.GetDynamicInstanceCount(), 57);
	TestEqual(TEXT("tower min"), MaximumResult.TowerTopBounds.Min, FVector2D(4.f, -2009.f));
	TestEqual(TEXT("tower max"), MaximumResult.TowerTopBounds.Max, FVector2D(1604.f, -9.f));
	TestEqual(TEXT("apron bottom"), MaximumResult.ApronBottomZ, -1620.f);
	TestEqual(TEXT("transfer top"), MaximumResult.TransferTopZ, -1520.f);
	TestEqual(TEXT("transfer bottom"), MaximumResult.TransferBottomZ, -1920.f);
	TestEqual(TEXT("tower top"), MaximumResult.TowerTopZ, -1870.f);
	TestEqual(TEXT("twelve skyline blocks"), MaximumResult.SkylineTransforms.Num(), 12);

	const FVector SlabLocation = MaximumResult.ApronSlabTransform.GetLocation();
	const FVector SlabScale = MaximumResult.ApronSlabTransform.GetScale3D();
	TestEqual(TEXT("slab center Z"), SlabLocation.Z, -10.0);
	TestEqual(TEXT("slab height is 20cm"), SlabScale.Z, 0.2);

	const FVector TransferLocation = MaximumResult.TransferLevelTransform.GetLocation();
	const FVector TransferScale = MaximumResult.TransferLevelTransform.GetScale3D();
	TestEqual(TEXT("transfer center Z"), TransferLocation.Z, -1720.0);
	TestEqual(TEXT("transfer height is 400cm"), TransferScale.Z, 4.0);

	const FVector TowerLocation = MaximumResult.TowerBodyTransform.GetLocation();
	const FVector TowerScale = MaximumResult.TowerBodyTransform.GetScale3D();
	TestEqual(TEXT("tower center Z"), TowerLocation.Z, -5070.0);
	TestEqual(TEXT("tower height is 6400cm"), TowerScale.Z, 64.0);

	FOfficeExteriorLayoutResult MinimumResult;
	TestTrue(
		TEXT("minimum layout builds"),
		FOfficeExteriorLayoutBuilder::Build(
			OfficeExteriorLayoutTests::MakeInput(FIntPoint(1, 2)),
			MinimumResult));
	TestEqual(TEXT("12 minimum facade bays"), MinimumResult.FacadeBayTransforms.Num(), 12);
	TestEqual(TEXT("8 minimum floor bands"), MinimumResult.FloorBandTransforms.Num(), 8);
	TestEqual(TEXT("5 minimum corners and trims"), MinimumResult.CornerAndTrimTransforms.Num(), 5);
	TestEqual(TEXT("25 minimum dynamic instances"), MinimumResult.GetDynamicInstanceCount(), 25);

	return true;
}

bool FCGROfficeExteriorLayoutValidationTest::RunTest(const FString& Parameters)
{
	FOfficeExteriorLayoutResult SentinelResult;
	SentinelResult.ApronBottomZ = 12345.f;
	SentinelResult.FacadeBayTransforms.Add(FTransform(FVector(1.f, 2.f, 3.f)));

	FOfficeExteriorLayoutInput InvalidCountInput = OfficeExteriorLayoutTests::MakeInput(FIntPoint(1, 2));
	InvalidCountInput.TileCount = FIntPoint(0, 1);
	TestFalse(
		TEXT("count below structural minimum is rejected"),
		FOfficeExteriorLayoutBuilder::Build(InvalidCountInput, SentinelResult));
	TestEqual(TEXT("failed build preserves scalar presentation"), SentinelResult.ApronBottomZ, 12345.f);
	TestEqual(TEXT("failed build preserves transform presentation"), SentinelResult.FacadeBayTransforms.Num(), 1);

	FOfficeExteriorLayoutInput OversizedCountInput = OfficeExteriorLayoutTests::MakeInput(FIntPoint(1, 2));
	OversizedCountInput.TileCount = FIntPoint(6, 7);
	TestFalse(
		TEXT("count above configured maximum is rejected"),
		FOfficeExteriorLayoutBuilder::Build(OversizedCountInput, SentinelResult));

	FOfficeExteriorLayoutInput MismatchedBoundsInput = OfficeExteriorLayoutTests::MakeInput(FIntPoint(1, 2));
	MismatchedBoundsInput.CurrentFloorBounds =
		FOfficeFootprintGeometry::MakeFloorBounds(FIntPoint(5, 6), 400.f);
	TestFalse(
		TEXT("bounds that do not match tile count are rejected"),
		FOfficeExteriorLayoutBuilder::Build(MismatchedBoundsInput, SentinelResult));

	FOfficeExteriorLayoutInput InvalidTileSizeInput = OfficeExteriorLayoutTests::MakeInput(FIntPoint(1, 2));
	InvalidTileSizeInput.TileSizeCm = 0.f;
	TestFalse(
		TEXT("non-positive tile size is rejected"),
		FOfficeExteriorLayoutBuilder::Build(InvalidTileSizeInput, SentinelResult));

	FOfficeExteriorLayoutResult FirstResult;
	FOfficeExteriorLayoutResult SecondResult;
	const FOfficeExteriorLayoutInput StableInput = OfficeExteriorLayoutTests::MakeInput(FIntPoint(3, 4));
	TestTrue(TEXT("first deterministic build succeeds"), FOfficeExteriorLayoutBuilder::Build(StableInput, FirstResult));
	TestTrue(TEXT("second deterministic build succeeds"), FOfficeExteriorLayoutBuilder::Build(StableInput, SecondResult));
	TestEqual(TEXT("deterministic bay count"), FirstResult.FacadeBayTransforms.Num(), SecondResult.FacadeBayTransforms.Num());
	for (int32 Index = 0; Index < FirstResult.FacadeBayTransforms.Num(); ++Index)
	{
		TestTrue(
			*FString::Printf(TEXT("deterministic bay transform %d"), Index),
			FirstResult.FacadeBayTransforms[Index].Equals(
				SecondResult.FacadeBayTransforms[Index],
				OfficeExteriorLayoutTests::TransformTolerance));
	}
	TestTrue(
		TEXT("deterministic structural transforms"),
		FirstResult.ApronSlabTransform.Equals(
			SecondResult.ApronSlabTransform,
			OfficeExteriorLayoutTests::TransformTolerance)
		&& FirstResult.TransferLevelTransform.Equals(
			SecondResult.TransferLevelTransform,
			OfficeExteriorLayoutTests::TransformTolerance)
		&& FirstResult.TowerBodyTransform.Equals(
			SecondResult.TowerBodyTransform,
			OfficeExteriorLayoutTests::TransformTolerance));

	return true;
}

bool FCGROfficeExteriorLayoutExpansionTest::RunTest(const FString& Parameters)
{
	const FOfficeExteriorLayoutInput MaximumInput = OfficeExteriorLayoutTests::MakeInput(FIntPoint(5, 6));
	FOfficeExteriorLayoutResult MaximumResult;
	if (!TestTrue(TEXT("maximum grid layout builds"), FOfficeExteriorLayoutBuilder::Build(MaximumInput, MaximumResult)))
	{
		return false;
	}

	for (int32 Index = 0; Index < MaximumResult.FacadeBayTransforms.Num(); ++Index)
	{
		const FTransform& BayTransform = MaximumResult.FacadeBayTransforms[Index];
		const FVector Location = BayTransform.GetLocation();
		const float NormalizedFloor = (-220.f - Location.Z) / 400.f;
		TestTrue(
			*FString::Printf(TEXT("bay %d uses a 400cm floor grid"), Index),
			OfficeExteriorLayoutTests::IsIntegerWithinTolerance(NormalizedFloor));

		if (FMath::IsNearlyEqual(Location.Y, MaximumInput.CurrentFloorBounds.Min.Y - 20.f))
		{
			const float NormalizedX =
				(Location.X - (MaximumInput.CurrentFloorBounds.Min.X + 200.f)) / 400.f;
			TestTrue(
				*FString::Printf(TEXT("-Y bay %d uses the 400cm X grid"), Index),
				OfficeExteriorLayoutTests::IsIntegerWithinTolerance(NormalizedX));
			TestTrue(
				*FString::Printf(TEXT("-Y bay %d has zero yaw"), Index),
				FMath::IsNearlyZero(BayTransform.Rotator().Yaw, KINDA_SMALL_NUMBER));
		}
		else if (FMath::IsNearlyEqual(Location.X, MaximumInput.CurrentFloorBounds.Max.X + 20.f))
		{
			const float NormalizedY =
				((MaximumInput.CurrentFloorBounds.Max.Y - 200.f) - Location.Y) / 400.f;
			TestTrue(
				*FString::Printf(TEXT("+X bay %d uses the 400cm Y grid"), Index),
				OfficeExteriorLayoutTests::IsIntegerWithinTolerance(NormalizedY));
			TestTrue(
				*FString::Printf(TEXT("+X bay %d has 90 degree yaw"), Index),
				FMath::IsNearlyEqual(FMath::Abs(BayTransform.Rotator().Yaw), 90.f, KINDA_SMALL_NUMBER));
		}
		else
		{
			AddError(FString::Printf(TEXT("bay %d is not on an open facade"), Index));
		}

		for (int32 OtherIndex = Index + 1; OtherIndex < MaximumResult.FacadeBayTransforms.Num(); ++OtherIndex)
		{
			TestFalse(
				*FString::Printf(TEXT("bay transforms %d and %d are unique"), Index, OtherIndex),
				BayTransform.Equals(
					MaximumResult.FacadeBayTransforms[OtherIndex],
					OfficeExteriorLayoutTests::TransformTolerance));
		}
	}

	FOfficeExteriorLayoutResult OneByTwoResult;
	FOfficeExteriorLayoutResult TwoByTwoResult;
	FOfficeExteriorLayoutResult TwoByThreeResult;
	TestTrue(
		TEXT("1x2 expansion baseline builds"),
		FOfficeExteriorLayoutBuilder::Build(
			OfficeExteriorLayoutTests::MakeInput(FIntPoint(1, 2)),
			OneByTwoResult));
	TestTrue(
		TEXT("2x2 expansion result builds"),
		FOfficeExteriorLayoutBuilder::Build(
			OfficeExteriorLayoutTests::MakeInput(FIntPoint(2, 2)),
			TwoByTwoResult));
	TestTrue(
		TEXT("2x3 expansion result builds"),
		FOfficeExteriorLayoutBuilder::Build(
			OfficeExteriorLayoutTests::MakeInput(FIntPoint(2, 3)),
			TwoByThreeResult));

	int32 StableNegativeYBays = 0;
	for (const FTransform& OldBay : OneByTwoResult.FacadeBayTransforms)
	{
		if (FMath::IsNearlyEqual(OldBay.GetLocation().Y, -829.f))
		{
			++StableNegativeYBays;
			TestTrue(
				TEXT("+X expansion preserves each existing -Y bay"),
				OfficeExteriorLayoutTests::ContainsTransform(TwoByTwoResult.FacadeBayTransforms, OldBay));
		}
	}
	TestEqual(TEXT("four existing -Y floor bays were checked"), StableNegativeYBays, 4);

	int32 StablePositiveXBays = 0;
	for (const FTransform& OldBay : TwoByTwoResult.FacadeBayTransforms)
	{
		if (FMath::IsNearlyEqual(OldBay.GetLocation().X, 824.f))
		{
			++StablePositiveXBays;
			TestTrue(
				TEXT("-Y expansion preserves each existing +X bay"),
				OfficeExteriorLayoutTests::ContainsTransform(TwoByThreeResult.FacadeBayTransforms, OldBay));
		}
	}
	TestEqual(TEXT("eight existing +X floor bays were checked"), StablePositiveXBays, 8);

	return true;
}

bool FCGROfficeExteriorLayoutSkylineTest::RunTest(const FString& Parameters)
{
	const FOfficeExteriorLayoutInput Input = OfficeExteriorLayoutTests::MakeInput(FIntPoint(5, 6));
	FOfficeExteriorLayoutResult Result;
	if (!TestTrue(TEXT("skyline layout builds"), FOfficeExteriorLayoutBuilder::Build(Input, Result)))
	{
		return false;
	}

	TestEqual(TEXT("skyline block count"), Result.SkylineTransforms.Num(), 12);
	for (int32 Index = 0; Index < Result.SkylineTransforms.Num(); ++Index)
	{
		const FTransform& SkylineTransform = Result.SkylineTransforms[Index];
		const FVector Center = SkylineTransform.GetLocation();
		const FVector HalfExtent = SkylineTransform.GetScale3D().GetAbs() * 50.f;
		const float TopZ = Center.Z + HalfExtent.Z;
		const float BottomZ = Center.Z - HalfExtent.Z;
		const FBox2D SkylineBounds(
			FVector2D(Center.X - HalfExtent.X, Center.Y - HalfExtent.Y),
			FVector2D(Center.X + HalfExtent.X, Center.Y + HalfExtent.Y));

		TestTrue(
			*FString::Printf(TEXT("skyline %d top is no higher than -2200"), Index),
			TopZ <= -2200.f + KINDA_SMALL_NUMBER);
		TestTrue(
			*FString::Printf(TEXT("skyline %d top is no lower than -3600"), Index),
			TopZ >= -3600.f - KINDA_SMALL_NUMBER);
		TestTrue(
			*FString::Printf(TEXT("skyline %d reaches below -9000"), Index),
			BottomZ <= -9000.f + KINDA_SMALL_NUMBER);
		TestTrue(
			*FString::Printf(TEXT("skyline %d remains 1200cm outside max floor"), Index),
			OfficeExteriorLayoutTests::DistanceBetweenBoxes(Input.MaxFloorBounds, SkylineBounds)
				>= 1200.f - KINDA_SMALL_NUMBER);
	}

	return true;
}

bool FCGROfficeExteriorShellComponentContractTest::RunTest(const FString& Parameters)
{
	AOfficeExteriorShell* ShellCDO = GetMutableDefault<AOfficeExteriorShell>();
	if (!TestNotNull(TEXT("office exterior shell CDO"), ShellCDO))
	{
		return false;
	}

	TestFalse(TEXT("shell actor tick is disabled"), ShellCDO->PrimaryActorTick.bCanEverTick);

	const TArray<FName> ExpectedVisualComponents = {
		TEXT("TowerBody"),
		TEXT("TowerTransferLevel"),
		TEXT("ApronSlab"),
		TEXT("FacadeApronISM"),
		TEXT("FacadeFloorBandISM"),
		TEXT("FacadeCornerISM"),
		TEXT("SkylineBlocksISM")};

	for (const FName ComponentName : ExpectedVisualComponents)
	{
		UPrimitiveComponent* VisualComponent = Cast<UPrimitiveComponent>(
			ShellCDO->GetDefaultSubobjectByName(ComponentName));
		if (!TestNotNull(*FString::Printf(TEXT("%s component exists"), *ComponentName.ToString()), VisualComponent))
		{
			continue;
		}

		TestEqual(
			*FString::Printf(TEXT("%s collision is disabled"), *ComponentName.ToString()),
			VisualComponent->GetCollisionEnabled(),
			ECollisionEnabled::NoCollision);
		TestFalse(
			*FString::Printf(TEXT("%s overlap events are disabled"), *ComponentName.ToString()),
			VisualComponent->GetGenerateOverlapEvents());
		TestFalse(
			*FString::Printf(TEXT("%s navigation influence is disabled"), *ComponentName.ToString()),
			VisualComponent->CanEverAffectNavigation());
		TestFalse(
			*FString::Printf(TEXT("%s dynamic shadows are disabled"), *ComponentName.ToString()),
			VisualComponent->CastShadow);
	}

	UInstancedStaticMeshComponent* FacadeISM = Cast<UInstancedStaticMeshComponent>(
		ShellCDO->GetDefaultSubobjectByName(TEXT("FacadeApronISM")));
	UInstancedStaticMeshComponent* BandISM = Cast<UInstancedStaticMeshComponent>(
		ShellCDO->GetDefaultSubobjectByName(TEXT("FacadeFloorBandISM")));
	UInstancedStaticMeshComponent* CornerISM = Cast<UInstancedStaticMeshComponent>(
		ShellCDO->GetDefaultSubobjectByName(TEXT("FacadeCornerISM")));
	if (TestNotNull(TEXT("facade ISM"), FacadeISM)
		&& TestNotNull(TEXT("band ISM"), BandISM)
		&& TestNotNull(TEXT("corner ISM"), CornerISM))
	{
		TestEqual(
			TEXT("dynamic instance getter sums the three dynamic ISMs"),
			ShellCDO->GetDynamicInstanceCount(),
			FacadeISM->GetInstanceCount() + BandISM->GetInstanceCount() + CornerISM->GetInstanceCount());
	}

	return true;
}

bool FCGROfficeExteriorCanonicalMaximumTest::RunTest(const FString& Parameters)
{
	FOfficeExteriorLayoutResult LowMaxSentinel;
	LowMaxSentinel.ApronBottomZ = 101.f;
	LowMaxSentinel.FacadeBayTransforms.Add(FTransform(FVector(1.f, 0.f, 0.f)));
	TestFalse(
		TEXT("4x5 maximum is rejected rather than changing the fixed shell contract"),
		FOfficeExteriorLayoutBuilder::Build(
			OfficeExteriorLayoutTests::MakeInput(FIntPoint(1, 2), FIntPoint(4, 5)),
			LowMaxSentinel));
	TestEqual(TEXT("low malformed maximum preserves scalar output"), LowMaxSentinel.ApronBottomZ, 101.f);
	TestEqual(TEXT("low malformed maximum preserves transform output"), LowMaxSentinel.FacadeBayTransforms.Num(), 1);

	FOfficeExteriorLayoutResult HighMaxSentinel;
	HighMaxSentinel.ApronBottomZ = 202.f;
	HighMaxSentinel.FacadeBayTransforms.Add(FTransform(FVector(2.f, 0.f, 0.f)));
	TestFalse(
		TEXT("6x7 maximum is rejected before it can create 65 dynamic instances"),
		FOfficeExteriorLayoutBuilder::Build(
			OfficeExteriorLayoutTests::MakeInput(FIntPoint(6, 7), FIntPoint(6, 7)),
			HighMaxSentinel));
	TestEqual(TEXT("high malformed maximum preserves scalar output"), HighMaxSentinel.ApronBottomZ, 202.f);
	TestEqual(TEXT("high malformed maximum preserves transform output"), HighMaxSentinel.FacadeBayTransforms.Num(), 1);

	OfficeExteriorLayoutTests::FScopedTestWorld TestWorldScope;
	UWorld* TestWorld = TestWorldScope.Get();
	if (!TestNotNull(TEXT("transient game world"), TestWorld))
	{
		return false;
	}

	AOfficeInterior* TestInterior = TestWorld->SpawnActor<AOfficeInterior>();
	AOfficeExteriorShell* TestShell = TestWorld->SpawnActor<AOfficeExteriorShell>();
	if (!TestNotNull(TEXT("test OfficeInterior"), TestInterior)
		|| !TestNotNull(TEXT("test OfficeExteriorShell"), TestShell))
	{
		return false;
	}

	FObjectProperty* TargetInteriorProperty = FindFProperty<FObjectProperty>(
		AOfficeExteriorShell::StaticClass(),
		TEXT("TargetInterior"));
	FIntProperty* MaxTileCountXProperty = FindFProperty<FIntProperty>(
		AOfficeInterior::StaticClass(),
		TEXT("MaxTileCountX"));
	FIntProperty* MaxTileCountYProperty = FindFProperty<FIntProperty>(
		AOfficeInterior::StaticClass(),
		TEXT("MaxTileCountY"));
	if (!TestNotNull(TEXT("TargetInterior property"), TargetInteriorProperty)
		|| !TestNotNull(TEXT("MaxTileCountX property"), MaxTileCountXProperty)
		|| !TestNotNull(TEXT("MaxTileCountY property"), MaxTileCountYProperty))
	{
		return false;
	}

	TargetInteriorProperty->SetObjectPropertyValue_InContainer(TestShell, TestInterior);
	if (!TestTrue(TEXT("canonical shell synchronization succeeds"), TestShell->SynchronizeToInterior()))
	{
		return false;
	}

	const FIntPoint LastBuiltBeforeMalformedMax = TestShell->GetLastBuiltTileCount();
	const int32 DynamicCountBeforeMalformedMax = TestShell->GetDynamicInstanceCount();
	UStaticMeshComponent* TransferComponent = Cast<UStaticMeshComponent>(
		TestShell->GetDefaultSubobjectByName(TEXT("TowerTransferLevel")));
	if (!TestNotNull(TEXT("transfer component"), TransferComponent))
	{
		return false;
	}
	const FTransform TransferBeforeMalformedMax = TransferComponent->GetRelativeTransform();

	MaxTileCountXProperty->SetPropertyValue_InContainer(TestInterior, 6);
	MaxTileCountYProperty->SetPropertyValue_InContainer(TestInterior, 7);
	TestFalse(
		TEXT("shell rejects high malformed maximum even when tile count is unchanged"),
		TestShell->SynchronizeToInterior());
	TestEqual(
		TEXT("high malformed maximum preserves shell tile cache"),
		TestShell->GetLastBuiltTileCount(),
		LastBuiltBeforeMalformedMax);
	TestEqual(
		TEXT("high malformed maximum preserves dynamic presentation"),
		TestShell->GetDynamicInstanceCount(),
		DynamicCountBeforeMalformedMax);
	TestTrue(
		TEXT("high malformed maximum preserves transfer presentation"),
		TransferComponent->GetRelativeTransform().Equals(
			TransferBeforeMalformedMax,
			OfficeExteriorLayoutTests::TransformTolerance));

	MaxTileCountXProperty->SetPropertyValue_InContainer(TestInterior, 4);
	MaxTileCountYProperty->SetPropertyValue_InContainer(TestInterior, 5);
	TestFalse(
		TEXT("shell rejects low malformed maximum even when tile count is unchanged"),
		TestShell->SynchronizeToInterior());
	TestEqual(
		TEXT("low malformed maximum preserves shell tile cache"),
		TestShell->GetLastBuiltTileCount(),
		LastBuiltBeforeMalformedMax);
	TestEqual(
		TEXT("low malformed maximum preserves dynamic presentation"),
		TestShell->GetDynamicInstanceCount(),
		DynamicCountBeforeMalformedMax);
	TestTrue(
		TEXT("low malformed maximum preserves transfer presentation"),
		TransferComponent->GetRelativeTransform().Equals(
			TransferBeforeMalformedMax,
			OfficeExteriorLayoutTests::TransformTolerance));

	return true;
}

bool FCGROfficeExteriorSupportedRangeTest::RunTest(const FString& Parameters)
{
	FOfficeExteriorLayoutResult CanonicalMaxResult;
	if (!TestTrue(
		TEXT("canonical maximum layout builds"),
		FOfficeExteriorLayoutBuilder::Build(
			OfficeExteriorLayoutTests::MakeInput(FIntPoint(5, 6)),
			CanonicalMaxResult)))
	{
		return false;
	}

	for (int32 TileCountX = 1; TileCountX <= 5; ++TileCountX)
	{
		for (int32 TileCountY = 2; TileCountY <= 6; ++TileCountY)
		{
			const FIntPoint TileCount(TileCountX, TileCountY);
			FOfficeExteriorLayoutResult Result;
			if (!TestTrue(
				*FString::Printf(TEXT("supported footprint %dx%d builds"), TileCountX, TileCountY),
				FOfficeExteriorLayoutBuilder::Build(
					OfficeExteriorLayoutTests::MakeInput(TileCount),
					Result)))
			{
				continue;
			}

			TestEqual(
				*FString::Printf(TEXT("%dx%d uses the exact dynamic formula"), TileCountX, TileCountY),
				Result.GetDynamicInstanceCount(),
				4 * (TileCountX + TileCountY) + 13);
			TestTrue(
				*FString::Printf(TEXT("%dx%d stays within 57 dynamic instances"), TileCountX, TileCountY),
				Result.GetDynamicInstanceCount() <= 57);
			TestEqual(
				*FString::Printf(TEXT("%dx%d keeps the fixed tower minimum"), TileCountX, TileCountY),
				Result.TowerTopBounds.Min,
				FVector2D(4.f, -2009.f));
			TestEqual(
				*FString::Printf(TEXT("%dx%d keeps the fixed tower maximum"), TileCountX, TileCountY),
				Result.TowerTopBounds.Max,
				FVector2D(1604.f, -9.f));
			TestTrue(
				*FString::Printf(TEXT("%dx%d keeps the transfer transform fixed"), TileCountX, TileCountY),
				Result.TransferLevelTransform.Equals(
					CanonicalMaxResult.TransferLevelTransform,
					OfficeExteriorLayoutTests::TransformTolerance));
			TestTrue(
				*FString::Printf(TEXT("%dx%d keeps the lower tower transform fixed"), TileCountX, TileCountY),
				Result.TowerBodyTransform.Equals(
					CanonicalMaxResult.TowerBodyTransform,
					OfficeExteriorLayoutTests::TransformTolerance));
			TestEqual(
				*FString::Printf(TEXT("%dx%d keeps twelve skyline blocks"), TileCountX, TileCountY),
				Result.SkylineTransforms.Num(),
				12);
			for (int32 SkylineIndex = 0; SkylineIndex < Result.SkylineTransforms.Num(); ++SkylineIndex)
			{
				TestTrue(
					*FString::Printf(
						TEXT("%dx%d keeps skyline block %d fixed"),
						TileCountX,
						TileCountY,
						SkylineIndex),
					Result.SkylineTransforms[SkylineIndex].Equals(
						CanonicalMaxResult.SkylineTransforms[SkylineIndex],
						OfficeExteriorLayoutTests::TransformTolerance));
			}
		}
	}

	return true;
}

bool FCGROfficeExteriorFacadeMaterialSlotsTest::RunTest(const FString& Parameters)
{
	OfficeExteriorLayoutTests::FScopedTestWorld TestWorldScope;
	UWorld* TestWorld = TestWorldScope.Get();
	if (!TestNotNull(TEXT("transient game world"), TestWorld))
	{
		return false;
	}

	AOfficeInterior* TestInterior = TestWorld->SpawnActor<AOfficeInterior>();
	AOfficeExteriorShell* OrderedSlotShell = TestWorld->SpawnActor<AOfficeExteriorShell>();
	if (!TestNotNull(TEXT("test OfficeInterior"), TestInterior)
		|| !TestNotNull(TEXT("ordered-slot shell"), OrderedSlotShell))
	{
		return false;
	}

	FObjectProperty* TargetInteriorProperty = FindFProperty<FObjectProperty>(
		AOfficeExteriorShell::StaticClass(),
		TEXT("TargetInterior"));
	FObjectProperty* FacadeBayMeshProperty = FindFProperty<FObjectProperty>(
		AOfficeExteriorShell::StaticClass(),
		TEXT("FacadeBayMesh"));
	FObjectProperty* GlassTrimMaterialProperty = FindFProperty<FObjectProperty>(
		AOfficeExteriorShell::StaticClass(),
		TEXT("GlassTrimMaterial"));
	if (!TestNotNull(TEXT("TargetInterior property"), TargetInteriorProperty)
		|| !TestNotNull(TEXT("FacadeBayMesh property"), FacadeBayMeshProperty)
		|| !TestNotNull(TEXT("GlassTrimMaterial property"), GlassTrimMaterialProperty))
	{
		return false;
	}

	UInstancedStaticMeshComponent* SourceCubeComponent = Cast<UInstancedStaticMeshComponent>(
		OrderedSlotShell->GetDefaultSubobjectByName(TEXT("FacadeFloorBandISM")));
	if (!TestNotNull(TEXT("source cube component"), SourceCubeComponent)
		|| !TestNotNull(TEXT("source cube mesh"), SourceCubeComponent->GetStaticMesh().Get()))
	{
		return false;
	}

	UMaterial* FrameMaterial = NewObject<UMaterial>(GetTransientPackage());
	UMaterial* AccentMaterial = NewObject<UMaterial>(GetTransientPackage());
	UMaterial* AuthoredGlassMaterial = NewObject<UMaterial>(GetTransientPackage());
	UMaterial* GlassOverrideMaterial = NewObject<UMaterial>(GetTransientPackage());
	UMaterial* OtherMaterial = NewObject<UMaterial>(GetTransientPackage());

	UStaticMesh* OrderedSlotMesh = DuplicateObject<UStaticMesh>(
		SourceCubeComponent->GetStaticMesh(),
		GetTransientPackage());
	OrderedSlotMesh->GetStaticMaterials().Reset();
	OrderedSlotMesh->GetStaticMaterials().Add(
		FStaticMaterial(FrameMaterial, TEXT("OfficeFrame"), TEXT("LegacyFrame")));
	OrderedSlotMesh->GetStaticMaterials().Add(
		FStaticMaterial(AccentMaterial, TEXT("OfficeAccent"), TEXT("LegacyAccent")));
	OrderedSlotMesh->GetStaticMaterials().Add(
		FStaticMaterial(AuthoredGlassMaterial, TEXT("OfficeGlass"), TEXT("LegacyGlass")));

	TargetInteriorProperty->SetObjectPropertyValue_InContainer(OrderedSlotShell, TestInterior);
	FacadeBayMeshProperty->SetObjectPropertyValue_InContainer(OrderedSlotShell, OrderedSlotMesh);
	GlassTrimMaterialProperty->SetObjectPropertyValue_InContainer(OrderedSlotShell, GlassOverrideMaterial);
	if (!TestTrue(TEXT("ordered-slot shell synchronizes"), OrderedSlotShell->SynchronizeToInterior()))
	{
		return false;
	}

	UInstancedStaticMeshComponent* OrderedFacadeISM = Cast<UInstancedStaticMeshComponent>(
		OrderedSlotShell->GetDefaultSubobjectByName(TEXT("FacadeApronISM")));
	if (!TestNotNull(TEXT("ordered facade ISM"), OrderedFacadeISM))
	{
		return false;
	}
	TestTrue(
		TEXT("OfficeFrame authored assignment survives reordered slots"),
		OrderedFacadeISM->GetMaterial(0) == FrameMaterial);
	TestTrue(
		TEXT("OfficeAccent authored assignment survives reordered slots"),
		OrderedFacadeISM->GetMaterial(1) == AccentMaterial);
	TestTrue(
		TEXT("only runtime OfficeGlass receives GlassTrimMaterial"),
		OrderedFacadeISM->GetMaterial(2) == GlassOverrideMaterial);

	AOfficeExteriorShell* MissingGlassSlotShell = TestWorld->SpawnActor<AOfficeExteriorShell>();
	if (!TestNotNull(TEXT("missing-glass-slot shell"), MissingGlassSlotShell))
	{
		return false;
	}
	UStaticMesh* MissingGlassSlotMesh = DuplicateObject<UStaticMesh>(
		SourceCubeComponent->GetStaticMesh(),
		GetTransientPackage());
	MissingGlassSlotMesh->GetStaticMaterials().Reset();
	MissingGlassSlotMesh->GetStaticMaterials().Add(
		FStaticMaterial(FrameMaterial, TEXT("OfficeFrame"), TEXT("LegacyFrame")));
	MissingGlassSlotMesh->GetStaticMaterials().Add(
		FStaticMaterial(AccentMaterial, TEXT("OfficeAccent"), TEXT("OfficeGlass")));
	MissingGlassSlotMesh->GetStaticMaterials().Add(
		FStaticMaterial(OtherMaterial, TEXT("OfficeOther"), TEXT("LegacyOther")));
	TargetInteriorProperty->SetObjectPropertyValue_InContainer(MissingGlassSlotShell, TestInterior);
	FacadeBayMeshProperty->SetObjectPropertyValue_InContainer(MissingGlassSlotShell, MissingGlassSlotMesh);
	GlassTrimMaterialProperty->SetObjectPropertyValue_InContainer(MissingGlassSlotShell, GlassOverrideMaterial);
	if (!TestTrue(TEXT("missing-glass-slot shell synchronizes"), MissingGlassSlotShell->SynchronizeToInterior()))
	{
		return false;
	}
	UInstancedStaticMeshComponent* MissingGlassFacadeISM = Cast<UInstancedStaticMeshComponent>(
		MissingGlassSlotShell->GetDefaultSubobjectByName(TEXT("FacadeApronISM")));
	if (!TestNotNull(TEXT("missing-glass facade ISM"), MissingGlassFacadeISM))
	{
		return false;
	}
	TestTrue(
		TEXT("missing OfficeGlass never overwrites OfficeFrame"),
		MissingGlassFacadeISM->GetMaterial(0) == FrameMaterial);
	TestTrue(
		TEXT("missing OfficeGlass preserves OfficeAccent"),
		MissingGlassFacadeISM->GetMaterial(1) == AccentMaterial);
	TestTrue(
		TEXT("missing OfficeGlass preserves every authored fallback slot"),
		MissingGlassFacadeISM->GetMaterial(2) == OtherMaterial);

	AOfficeExteriorShell* NullOverrideShell = TestWorld->SpawnActor<AOfficeExteriorShell>();
	if (!TestNotNull(TEXT("null-override shell"), NullOverrideShell))
	{
		return false;
	}
	TargetInteriorProperty->SetObjectPropertyValue_InContainer(NullOverrideShell, TestInterior);
	FacadeBayMeshProperty->SetObjectPropertyValue_InContainer(NullOverrideShell, OrderedSlotMesh);
	if (!TestTrue(TEXT("null-override shell synchronizes"), NullOverrideShell->SynchronizeToInterior()))
	{
		return false;
	}
	UInstancedStaticMeshComponent* NullOverrideFacadeISM = Cast<UInstancedStaticMeshComponent>(
		NullOverrideShell->GetDefaultSubobjectByName(TEXT("FacadeApronISM")));
	if (!TestNotNull(TEXT("null-override facade ISM"), NullOverrideFacadeISM))
	{
		return false;
	}
	TestTrue(
		TEXT("null GlassTrimMaterial preserves authored frame"),
		NullOverrideFacadeISM->GetMaterial(0) == FrameMaterial);
	TestTrue(
		TEXT("null GlassTrimMaterial preserves authored accent"),
		NullOverrideFacadeISM->GetMaterial(1) == AccentMaterial);
	TestTrue(
		TEXT("null GlassTrimMaterial preserves authored glass"),
		NullOverrideFacadeISM->GetMaterial(2) == AuthoredGlassMaterial);

	return true;
}

bool FCGROfficeExteriorTowerHeightPropertyTest::RunTest(const FString& Parameters)
{
	FFloatProperty* TowerHeightProperty = FindFProperty<FFloatProperty>(
		AOfficeExteriorShell::StaticClass(),
		TEXT("TowerHeightCm"));
	if (!TestNotNull(TEXT("TowerHeightCm editor property exists"), TowerHeightProperty))
	{
		return false;
	}

	AOfficeExteriorShell* ShellCDO = GetMutableDefault<AOfficeExteriorShell>();
	TestEqual(
		TEXT("TowerHeightCm defaults to 6400cm"),
		TowerHeightProperty->GetPropertyValue_InContainer(ShellCDO),
		6400.f);
	TestTrue(
		TEXT("TowerHeightCm is editable on defaults"),
		TowerHeightProperty->HasAnyPropertyFlags(CPF_Edit));
	TestTrue(
		TEXT("TowerHeightCm is not editable per placed instance"),
		TowerHeightProperty->HasAnyPropertyFlags(CPF_DisableEditOnInstance));

	return true;
}

bool FCGROfficeExteriorTowerHeightGeometryTest::RunTest(const FString& Parameters)
{
	FOfficeExteriorLayoutInput CustomHeightInput =
		OfficeExteriorLayoutTests::MakeInput(FIntPoint(5, 6));
	CustomHeightInput.TowerHeightCm = 8000.f;
	FOfficeExteriorLayoutResult CustomHeightResult;
	if (!TestTrue(
		TEXT("custom positive tower height builds"),
		FOfficeExteriorLayoutBuilder::Build(CustomHeightInput, CustomHeightResult)))
	{
		return false;
	}

	const FVector CustomTowerCenter = CustomHeightResult.TowerBodyTransform.GetLocation();
	const FVector CustomTowerScale = CustomHeightResult.TowerBodyTransform.GetScale3D();
	const double CustomTowerHalfHeight = CustomTowerScale.Z * 50.0;
	TestEqual(TEXT("custom tower height scales the cube"), CustomTowerScale.Z, 80.0);
	TestEqual(
		TEXT("custom tower keeps the fixed top"),
		CustomTowerCenter.Z + CustomTowerHalfHeight,
		-1870.0);
	TestEqual(
		TEXT("custom tower extends only downward"),
		CustomTowerCenter.Z - CustomTowerHalfHeight,
		-9870.0);

	FOfficeExteriorLayoutResult ZeroHeightSentinel;
	ZeroHeightSentinel.TowerTopZ = 303.f;
	ZeroHeightSentinel.FacadeBayTransforms.Add(FTransform(FVector(3.f, 0.f, 0.f)));
	FOfficeExteriorLayoutInput ZeroHeightInput =
		OfficeExteriorLayoutTests::MakeInput(FIntPoint(1, 2));
	ZeroHeightInput.TowerHeightCm = 0.f;
	TestFalse(
		TEXT("zero tower height is rejected"),
		FOfficeExteriorLayoutBuilder::Build(ZeroHeightInput, ZeroHeightSentinel));
	TestEqual(TEXT("zero height preserves scalar output"), ZeroHeightSentinel.TowerTopZ, 303.f);
	TestEqual(TEXT("zero height preserves transform output"), ZeroHeightSentinel.FacadeBayTransforms.Num(), 1);

	FOfficeExteriorLayoutResult NonFiniteHeightSentinel;
	NonFiniteHeightSentinel.TowerTopZ = 404.f;
	NonFiniteHeightSentinel.FacadeBayTransforms.Add(FTransform(FVector(4.f, 0.f, 0.f)));
	FOfficeExteriorLayoutInput NonFiniteHeightInput =
		OfficeExteriorLayoutTests::MakeInput(FIntPoint(1, 2));
	NonFiniteHeightInput.TowerHeightCm = std::numeric_limits<float>::quiet_NaN();
	TestFalse(
		TEXT("non-finite tower height is rejected"),
		FOfficeExteriorLayoutBuilder::Build(NonFiniteHeightInput, NonFiniteHeightSentinel));
	TestEqual(TEXT("non-finite height preserves scalar output"), NonFiniteHeightSentinel.TowerTopZ, 404.f);
	TestEqual(TEXT("non-finite height preserves transform output"), NonFiniteHeightSentinel.FacadeBayTransforms.Num(), 1);

	FFloatProperty* TowerHeightProperty = FindFProperty<FFloatProperty>(
		AOfficeExteriorShell::StaticClass(),
		TEXT("TowerHeightCm"));
	FObjectProperty* TargetInteriorProperty = FindFProperty<FObjectProperty>(
		AOfficeExteriorShell::StaticClass(),
		TEXT("TargetInterior"));
	if (!TestNotNull(TEXT("TowerHeightCm property"), TowerHeightProperty)
		|| !TestNotNull(TEXT("TargetInterior property"), TargetInteriorProperty))
	{
		return false;
	}

	OfficeExteriorLayoutTests::FScopedTestWorld RuntimeWorldScope(EWorldType::Game);
	UWorld* RuntimeWorld = RuntimeWorldScope.Get();
	AOfficeInterior* RuntimeInterior = RuntimeWorld
		? RuntimeWorld->SpawnActor<AOfficeInterior>()
		: nullptr;
	AOfficeExteriorShell* RuntimeShell = RuntimeWorld
		? RuntimeWorld->SpawnActor<AOfficeExteriorShell>()
		: nullptr;
	if (!TestNotNull(TEXT("runtime OfficeInterior"), RuntimeInterior)
		|| !TestNotNull(TEXT("runtime shell"), RuntimeShell))
	{
		return false;
	}
	TowerHeightProperty->SetPropertyValue_InContainer(RuntimeShell, 8000.f);
	TargetInteriorProperty->SetObjectPropertyValue_InContainer(RuntimeShell, RuntimeInterior);
	if (!TestTrue(TEXT("runtime shell uses custom tower height"), RuntimeShell->SynchronizeToInterior()))
	{
		return false;
	}
	UStaticMeshComponent* RuntimeTower = Cast<UStaticMeshComponent>(
		RuntimeShell->GetDefaultSubobjectByName(TEXT("TowerBody")));
	if (!TestNotNull(TEXT("runtime tower component"), RuntimeTower))
	{
		return false;
	}

	OfficeExteriorLayoutTests::FScopedTestWorld PreviewWorldScope(EWorldType::EditorPreview);
	UWorld* PreviewWorld = PreviewWorldScope.Get();
	AOfficeExteriorShell* PreviewShell = PreviewWorld
		? PreviewWorld->SpawnActor<AOfficeExteriorShell>()
		: nullptr;
	if (!TestNotNull(TEXT("preview shell"), PreviewShell))
	{
		return false;
	}
	TowerHeightProperty->SetPropertyValue_InContainer(PreviewShell, 8000.f);
	PreviewShell->RerunConstructionScripts();
	UStaticMeshComponent* PreviewTower = Cast<UStaticMeshComponent>(
		PreviewShell->GetDefaultSubobjectByName(TEXT("TowerBody")));
	if (!TestNotNull(TEXT("preview tower component"), PreviewTower))
	{
		return false;
	}
	TestEqual(
		TEXT("preview instance receives the custom class default"),
		TowerHeightProperty->GetPropertyValue_InContainer(PreviewShell),
		8000.f);
	TestTrue(
		TEXT("preview and runtime use the same custom tower transform"),
		PreviewTower->GetRelativeTransform().Equals(
			RuntimeTower->GetRelativeTransform(),
			OfficeExteriorLayoutTests::TransformTolerance));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
