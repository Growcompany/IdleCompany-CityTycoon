#include "Misc/AutomationTest.h"
#include "Entity/OverseasDiorama/DioramaModules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr double kDioPortTestTolerance = 1e-6;

	FOverseasDioramaSpec DioPortTestSpec(bool bFactory, bool bMine, bool bPort)
	{
		FOverseasDioramaSpec Spec;
		Spec.Country = ECountryType::Korea;
		if (bFactory)
		{
			Spec.Factory.bPresent = true;
			Spec.Factory.Industry = ECompanyType::Electronics;
			Spec.Factory.Slots = { EDioramaSlotState::Empty, EDioramaSlotState::Empty, EDioramaSlotState::Empty };
		}
		if (bMine)
		{
			FDioramaMineUnit Unit;
			Unit.Resource = EResourceType::IronOre;
			Unit.State = EDioramaSlotState::Running;
			Unit.Fill01 = 0.4f;
			Spec.Mine.bPresent = true;
			Spec.Mine.Units.Add(Unit);
		}
		if (bPort)
		{
			Spec.Port.bPresent = true;
			Spec.Port.CraneCount = (bFactory || bMine) ? 2 : 3;
		}
		return Spec;
	}

	const FDioramaLayoutItem* DioPortTestFind(const TArray<FDioramaLayoutItem>& Items, EDioramaModule Module)
	{
		return Items.FindByPredicate([Module](const FDioramaLayoutItem& Item) { return Item.Module == Module; });
	}

	int32 DioPortTestCountRigs(const FDioramaModuleBuild& Build, EDioramaRigKind Kind)
	{
		int32 Count = 0;
		for (const FDioramaRigDesc& Rig : Build.Rigs)
		{
			if (Rig.Kind == Kind)
			{
				++Count;
			}
		}
		return Count;
	}

	int32 DioPortTestBucketTriangles(const FDioramaGeometry& G, uint32 RGB)
	{
		const FDioramaMeshBucket* Bucket = G.GetBuckets().Find(DioramaModules::Hex(RGB).ToPackedARGB());
		return Bucket ? Bucket->Triangles.Num() / 3 : 0;
	}

	int32 DioPortTestHedgeTriangles(const FOverseasDioramaSpec& Spec)
	{
		FDioramaGeometry Tile;
		DioramaModules::BuildTile(DioramaModules::Layout(Spec, true), Tile);
		return DioPortTestBucketTriangles(Tile, DioramaPalette::Hedge);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverseasModulePortCranesTest, "CGR.Overseas.Module.PortCranes",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FOverseasModulePortCranesTest::RunTest(const FString& Parameters)
{
	FDioramaPortSpec Spec;
	Spec.bPresent = true;

	Spec.CraneCount = 3;
	FDioramaModuleBuild ThreeCranes;
	DioramaModules::BuildPort(Spec, false, ThreeCranes);
	TestEqual(TEXT("3 cranes -> 3 trolleys"), DioPortTestCountRigs(ThreeCranes, EDioramaRigKind::CraneTrolley), 3);

	Spec.CraneCount = 2;
	FDioramaModuleBuild TwoCranes;
	DioramaModules::BuildPort(Spec, false, TwoCranes);
	TestEqual(TEXT("2 cranes -> 2 trolleys"), DioPortTestCountRigs(TwoCranes, EDioramaRigKind::CraneTrolley), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverseasModuleLayoutPrimaryAtOriginTest, "CGR.Overseas.Module.LayoutPrimaryAtOrigin",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FOverseasModuleLayoutPrimaryAtOriginTest::RunTest(const FString& Parameters)
{
	const FOverseasDioramaSpec Spec = DioPortTestSpec(true, true, true);
	const TArray<FDioramaLayoutItem> Items = DioramaModules::Layout(Spec, true);
	if (!TestEqual(TEXT("three modules"), Items.Num(), 3))
	{
		return false;
	}
	TestTrue(TEXT("first item = factory"), Items[0].Module == EDioramaModule::Factory);
	TestEqual(TEXT("factory offset x"), Items[0].OffsetXZ.X, 0.0, kDioPortTestTolerance);
	TestEqual(TEXT("factory offset z"), Items[0].OffsetXZ.Y, 0.0, kDioPortTestTolerance);

	const FDioramaLayoutItem* MineItem = DioPortTestFind(Items, EDioramaModule::Mine);
	const FDioramaLayoutItem* PortItem = DioPortTestFind(Items, EDioramaModule::Port);
	if (!TestNotNull(TEXT("mine item"), MineItem) || !TestNotNull(TEXT("port item"), PortItem))
	{
		return false;
	}

	const FBox2D FactoryBox = DioramaModules::FactoryRect(Spec.Factory, true);
	const FBox2D MineBox = DioramaModules::MineRect(Spec.Mine);
	const FBox2D PortBox = DioramaModules::PortRect();
	TestEqual(TEXT("mine behind factory"), MineItem->OffsetXZ.Y, FactoryBox.Min.Y - 0.6 - MineBox.Max.Y, kDioPortTestTolerance);
	TestEqual(TEXT("port in front of factory"), PortItem->OffsetXZ.Y, FactoryBox.Max.Y + 0.6 - PortBox.Min.Y, kDioPortTestTolerance);
	TestEqual(TEXT("mine offset x"), MineItem->OffsetXZ.X, 0.0, kDioPortTestTolerance);
	TestEqual(TEXT("port offset x"), PortItem->OffsetXZ.X, 0.0, kDioPortTestTolerance);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverseasModuleLayoutHubOnlyTest, "CGR.Overseas.Module.LayoutHubOnly",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FOverseasModuleLayoutHubOnlyTest::RunTest(const FString& Parameters)
{
	const TArray<FDioramaLayoutItem> Items = DioramaModules::Layout(DioPortTestSpec(false, false, true), true);
	if (!TestEqual(TEXT("one module"), Items.Num(), 1))
	{
		return false;
	}
	TestTrue(TEXT("item = port"), Items[0].Module == EDioramaModule::Port);
	TestEqual(TEXT("port offset x"), Items[0].OffsetXZ.X, 0.0, kDioPortTestTolerance);
	TestEqual(TEXT("port offset z"), Items[0].OffsetXZ.Y, 0.0, kDioPortTestTolerance);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverseasModuleTileCoversAllTest, "CGR.Overseas.Module.TileCoversAll",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FOverseasModuleTileCoversAllTest::RunTest(const FString& Parameters)
{
	const FOverseasDioramaSpec Spec = DioPortTestSpec(true, true, true);
	for (const bool bMiniature : { true, false })
	{
		const TArray<FDioramaLayoutItem> Items = DioramaModules::Layout(Spec, bMiniature);
		FDioramaGeometry Tile;
		const FBox2D TileBox = DioramaModules::BuildTile(Items, Tile);
		TestFalse(TEXT("tile geometry"), Tile.IsEmpty());
		for (const FDioramaLayoutItem& Item : Items)
		{
			const FBox2D Placed = Item.RectXZ.ShiftBy(Item.OffsetXZ);
			const bool bCovered = TileBox.Min.X <= Placed.Min.X + kDioPortTestTolerance
				&& TileBox.Min.Y <= Placed.Min.Y + kDioPortTestTolerance
				&& TileBox.Max.X >= Placed.Max.X - kDioPortTestTolerance
				&& TileBox.Max.Y >= Placed.Max.Y - kDioPortTestTolerance;
			TestTrue(FString::Printf(TEXT("tile covers module %d (mini=%d)"), static_cast<int32>(Item.Module), bMiniature ? 1 : 0), bCovered);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverseasModuleTileNoHedgeOnPortSideTest, "CGR.Overseas.Module.TileNoHedgeOnPortSide",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FOverseasModuleTileNoHedgeOnPortSideTest::RunTest(const FString& Parameters)
{
	const int32 WithPort = DioPortTestHedgeTriangles(DioPortTestSpec(true, false, true));
	const int32 WithoutPort = DioPortTestHedgeTriangles(DioPortTestSpec(true, false, false));
	TestTrue(TEXT("hedges exist"), WithPort > 0);
	TestEqual(TEXT("front hedge box (12 triangles) dropped with port"), WithPort, WithoutPort - 12);
	return true;
}

#endif
