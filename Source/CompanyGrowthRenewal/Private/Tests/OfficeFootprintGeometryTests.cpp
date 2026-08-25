#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Office/OfficeFootprintGeometry.h"
#include "Office/OfficeInterior.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace OfficeFootprintGeometryTests
{
class FScopedTestWorld
{
public:
	FScopedTestWorld()
	{
		TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
		if (TestWorld && GEngine)
		{
			FWorldContext& TestWorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
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
	FCGROfficeFootprintGeometryTest,
	"CGR.Office.Exterior.Footprint.Geometry",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROfficeFootprintSaveNormalizationTest,
	"CGR.Office.Exterior.Footprint.InteriorSaveNormalization",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROfficeFootprintStarterWithinCurrentTest,
	"CGR.Office.Exterior.Footprint.StarterWithinCurrent",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROfficeFootprintInvalidMaximumTest,
	"CGR.Office.Exterior.Footprint.InvalidMaximumRejected",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROfficeRejectedExpansionPreservesStateTest,
	"CGR.Office.Exterior.Footprint.RejectedExpansionPreservesState",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGROfficeFootprintGeometryTest::RunTest(const FString& Parameters)
{
	const FIntPoint MaximumTileCount(5, 6);

	TestEqual(
		TEXT("invalid low count"),
		FOfficeFootprintGeometry::NormalizeTileCount(FIntPoint(0, 1), MaximumTileCount),
		FIntPoint(1, 2));
	TestEqual(
		TEXT("invalid high count"),
		FOfficeFootprintGeometry::NormalizeTileCount(FIntPoint(8, 9), MaximumTileCount),
		MaximumTileCount);
	TestTrue(
		TEXT("normalized starter count is valid"),
		FOfficeFootprintGeometry::IsTileCountValid(FIntPoint(1, 2), MaximumTileCount));
	TestFalse(
		TEXT("count below the structural minimum is invalid"),
		FOfficeFootprintGeometry::IsTileCountValid(FIntPoint(1, 1), MaximumTileCount));

	const FBox2D StarterBounds = FOfficeFootprintGeometry::MakeFloorBounds(FIntPoint(1, 2), 400.f);
	TestEqual(TEXT("starter min"), StarterBounds.Min, FVector2D(4.f, -809.f));
	TestEqual(TEXT("starter max"), StarterBounds.Max, FVector2D(404.f, -9.f));

	const FBox2D MaximumBounds = FOfficeFootprintGeometry::MakeFloorBounds(MaximumTileCount, 400.f);
	TestEqual(TEXT("maximum min"), MaximumBounds.Min, FVector2D(4.f, -2409.f));
	TestEqual(TEXT("maximum max"), MaximumBounds.Max, FVector2D(2004.f, -9.f));

	TestEqual(
		TEXT("tile 0,0"),
		FOfficeFootprintGeometry::MakeTileCenter(0, 0, 400.f),
		FVector(204.f, -209.f, -6.f));
	TestEqual(
		TEXT("tile 4,5"),
		FOfficeFootprintGeometry::MakeTileCenter(4, 5, 400.f),
		FVector(1804.f, -2209.f, -6.f));

	return true;
}

bool FCGROfficeFootprintSaveNormalizationTest::RunTest(const FString& Parameters)
{
	OfficeFootprintGeometryTests::FScopedTestWorld TestWorldScope;
	UWorld* TestWorld = TestWorldScope.Get();
	if (!TestNotNull(TEXT("transient test world"), TestWorld))
	{
		return false;
	}

	AOfficeInterior* TestInterior = TestWorld->SpawnActor<AOfficeInterior>();
	if (!TestNotNull(TEXT("office interior actor"), TestInterior))
	{
		return false;
	}

	int32 BroadcastCount = 0;
	FIntPoint LastBroadcastTileCount = FIntPoint::ZeroValue;
	const FDelegateHandle DelegateHandle = TestInterior->OnFootprintChanged.AddLambda(
		[&BroadcastCount, &LastBroadcastTileCount](const FIntPoint TileCount)
		{
			++BroadcastCount;
			LastBroadcastTileCount = TileCount;
		});

	FOfficeSaveData OfficeData;
	OfficeData.TileCountX = 0;
	OfficeData.TileCountY = 99;
	OfficeData.StarterTileCountX = 0;
	OfficeData.StarterTileCountY = 99;
	OfficeData.CurrentFloorTileRowName = NAME_None;
	TestInterior->ApplyOfficeSaveData(OfficeData);

	TestEqual(TEXT("invalid requested count is normalized"), TestInterior->GetTileCount(), FIntPoint(1, 6));
	FIntProperty* StarterTileCountXProperty = FindFProperty<FIntProperty>(
		AOfficeInterior::StaticClass(),
		TEXT("StarterTileCountX"));
	FIntProperty* StarterTileCountYProperty = FindFProperty<FIntProperty>(
		AOfficeInterior::StaticClass(),
		TEXT("StarterTileCountY"));
	if (TestNotNull(TEXT("StarterTileCountX property"), StarterTileCountXProperty)
		&& TestNotNull(TEXT("StarterTileCountY property"), StarterTileCountYProperty))
	{
		TestEqual(
			TEXT("invalid requested starter count is normalized"),
			FIntPoint(
				StarterTileCountXProperty->GetPropertyValue_InContainer(TestInterior),
				StarterTileCountYProperty->GetPropertyValue_InContainer(TestInterior)),
			FIntPoint(1, 6));
	}
	TestEqual(TEXT("normalized save broadcasts once"), BroadcastCount, 1);
	TestEqual(TEXT("broadcast payload matches applied count"), LastBroadcastTileCount, FIntPoint(1, 6));

	TestInterior->OnFootprintChanged.Remove(DelegateHandle);
	return true;
}

bool FCGROfficeFootprintStarterWithinCurrentTest::RunTest(const FString& Parameters)
{
	OfficeFootprintGeometryTests::FScopedTestWorld TestWorldScope;
	UWorld* TestWorld = TestWorldScope.Get();
	if (!TestNotNull(TEXT("transient test world"), TestWorld))
	{
		return false;
	}

	AOfficeInterior* TestInterior = TestWorld->SpawnActor<AOfficeInterior>();
	if (!TestNotNull(TEXT("office interior actor"), TestInterior))
	{
		return false;
	}

	FOfficeSaveData OfficeData;
	OfficeData.TileCountX = 1;
	OfficeData.TileCountY = 2;
	OfficeData.StarterTileCountX = 5;
	OfficeData.StarterTileCountY = 6;
	OfficeData.CurrentFloorTileRowName = NAME_None;
	TestInterior->ApplyOfficeSaveData(OfficeData);

	FIntProperty* StarterTileCountXProperty = FindFProperty<FIntProperty>(
		AOfficeInterior::StaticClass(),
		TEXT("StarterTileCountX"));
	FIntProperty* StarterTileCountYProperty = FindFProperty<FIntProperty>(
		AOfficeInterior::StaticClass(),
		TEXT("StarterTileCountY"));
	if (!TestNotNull(TEXT("StarterTileCountX property"), StarterTileCountXProperty)
		|| !TestNotNull(TEXT("StarterTileCountY property"), StarterTileCountYProperty))
	{
		return false;
	}

	TestEqual(TEXT("current footprint uses normalized save count"), TestInterior->GetTileCount(), FIntPoint(1, 2));
	TestEqual(
		TEXT("starter footprint is clamped within current footprint"),
		FIntPoint(
			StarterTileCountXProperty->GetPropertyValue_InContainer(TestInterior),
			StarterTileCountYProperty->GetPropertyValue_InContainer(TestInterior)),
		FIntPoint(1, 2));

	return true;
}

bool FCGROfficeFootprintInvalidMaximumTest::RunTest(const FString& Parameters)
{
	OfficeFootprintGeometryTests::FScopedTestWorld TestWorldScope;
	UWorld* TestWorld = TestWorldScope.Get();
	if (!TestNotNull(TEXT("transient test world"), TestWorld))
	{
		return false;
	}

	AOfficeInterior* TestInterior = TestWorld->SpawnActor<AOfficeInterior>();
	if (!TestNotNull(TEXT("office interior actor"), TestInterior))
	{
		return false;
	}

	FIntProperty* MaxTileCountXProperty = FindFProperty<FIntProperty>(AOfficeInterior::StaticClass(), TEXT("MaxTileCountX"));
	FIntProperty* MaxTileCountYProperty = FindFProperty<FIntProperty>(AOfficeInterior::StaticClass(), TEXT("MaxTileCountY"));
	if (!TestNotNull(TEXT("MaxTileCountX property"), MaxTileCountXProperty)
		|| !TestNotNull(TEXT("MaxTileCountY property"), MaxTileCountYProperty))
	{
		return false;
	}

	MaxTileCountXProperty->SetPropertyValue_InContainer(TestInterior, 0);
	MaxTileCountYProperty->SetPropertyValue_InContainer(TestInterior, 1);
	const FIntPoint PreviousTileCount = TestInterior->GetTileCount();
	FIntProperty* StarterTileCountXProperty = FindFProperty<FIntProperty>(
		AOfficeInterior::StaticClass(),
		TEXT("StarterTileCountX"));
	FIntProperty* StarterTileCountYProperty = FindFProperty<FIntProperty>(
		AOfficeInterior::StaticClass(),
		TEXT("StarterTileCountY"));
	if (!TestNotNull(TEXT("StarterTileCountX property"), StarterTileCountXProperty)
		|| !TestNotNull(TEXT("StarterTileCountY property"), StarterTileCountYProperty))
	{
		return false;
	}
	const FIntPoint PreviousStarterTileCount(
		StarterTileCountXProperty->GetPropertyValue_InContainer(TestInterior),
		StarterTileCountYProperty->GetPropertyValue_InContainer(TestInterior));

	int32 BroadcastCount = 0;
	const FDelegateHandle DelegateHandle = TestInterior->OnFootprintChanged.AddLambda(
		[&BroadcastCount](const FIntPoint TileCount)
		{
			++BroadcastCount;
		});

	FOfficeSaveData OfficeData;
	OfficeData.TileCountX = 5;
	OfficeData.TileCountY = 6;
	OfficeData.CurrentFloorTileRowName = NAME_None;
	TestInterior->ApplyOfficeSaveData(OfficeData);

	TestEqual(TEXT("invalid configured maximum preserves state"), TestInterior->GetTileCount(), PreviousTileCount);
	TestEqual(
		TEXT("invalid configured maximum preserves starter state"),
		FIntPoint(
			StarterTileCountXProperty->GetPropertyValue_InContainer(TestInterior),
			StarterTileCountYProperty->GetPropertyValue_InContainer(TestInterior)),
		PreviousStarterTileCount);
	TestEqual(TEXT("invalid configured maximum does not broadcast"), BroadcastCount, 0);

	TestInterior->OnFootprintChanged.Remove(DelegateHandle);
	return true;
}

bool FCGROfficeRejectedExpansionPreservesStateTest::RunTest(const FString& Parameters)
{
	OfficeFootprintGeometryTests::FScopedTestWorld TestWorldScope;
	UWorld* TestWorld = TestWorldScope.Get();
	if (!TestNotNull(TEXT("transient test world"), TestWorld))
	{
		return false;
	}

	AOfficeInterior* TestInterior = TestWorld->SpawnActor<AOfficeInterior>();
	if (!TestNotNull(TEXT("office interior actor"), TestInterior))
	{
		return false;
	}

	FOfficeSaveData OfficeData;
	OfficeData.TileCountX = 5;
	OfficeData.TileCountY = 6;
	OfficeData.StarterTileCountX = 5;
	OfficeData.StarterTileCountY = 6;
	OfficeData.CurrentFloorTileRowName = NAME_None;
	TestInterior->ApplyOfficeSaveData(OfficeData);

	FIntProperty* MaxTileCountXProperty = FindFProperty<FIntProperty>(AOfficeInterior::StaticClass(), TEXT("MaxTileCountX"));
	FIntProperty* MaxTileCountYProperty = FindFProperty<FIntProperty>(AOfficeInterior::StaticClass(), TEXT("MaxTileCountY"));
	FIntProperty* StarterTileCountXProperty = FindFProperty<FIntProperty>(
		AOfficeInterior::StaticClass(),
		TEXT("StarterTileCountX"));
	FIntProperty* StarterTileCountYProperty = FindFProperty<FIntProperty>(
		AOfficeInterior::StaticClass(),
		TEXT("StarterTileCountY"));
	if (!TestNotNull(TEXT("MaxTileCountX property"), MaxTileCountXProperty)
		|| !TestNotNull(TEXT("MaxTileCountY property"), MaxTileCountYProperty)
		|| !TestNotNull(TEXT("StarterTileCountX property"), StarterTileCountXProperty)
		|| !TestNotNull(TEXT("StarterTileCountY property"), StarterTileCountYProperty))
	{
		return false;
	}

	MaxTileCountXProperty->SetPropertyValue_InContainer(TestInterior, 4);
	MaxTileCountYProperty->SetPropertyValue_InContainer(TestInterior, 5);
	const FIntPoint PreviousTileCount = TestInterior->GetTileCount();
	const FIntPoint PreviousStarterTileCount(
		StarterTileCountXProperty->GetPropertyValue_InContainer(TestInterior),
		StarterTileCountYProperty->GetPropertyValue_InContainer(TestInterior));

	int32 BroadcastCount = 0;
	const FDelegateHandle DelegateHandle = TestInterior->OnFootprintChanged.AddLambda(
		[&BroadcastCount](const FIntPoint TileCount)
		{
			++BroadcastCount;
		});

	TestFalse(TEXT("left expansion is rejected after maximum shrinks"), TestInterior->ExpandLeft());
	TestFalse(TEXT("right expansion is rejected after maximum shrinks"), TestInterior->ExpandRight());
	TestEqual(TEXT("rejected expansions preserve current count"), TestInterior->GetTileCount(), PreviousTileCount);
	TestEqual(
		TEXT("rejected expansions preserve starter count"),
		FIntPoint(
			StarterTileCountXProperty->GetPropertyValue_InContainer(TestInterior),
			StarterTileCountYProperty->GetPropertyValue_InContainer(TestInterior)),
		PreviousStarterTileCount);
	TestEqual(TEXT("rejected expansions do not broadcast"), BroadcastCount, 0);

	TestInterior->OnFootprintChanged.Remove(DelegateHandle);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
