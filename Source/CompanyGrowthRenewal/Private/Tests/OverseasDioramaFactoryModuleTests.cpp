#include "Misc/AutomationTest.h"
#include "Entity/OverseasDiorama/DioramaModules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr double kDioFacTestTolerance = 1e-6;

	FDioramaFactorySpec DioFacTestSpec(int32 SlotCount, int32 Automation = 0, int32 Quality = 0, int32 Storage = 0, int32 Rush = 0)
	{
		FDioramaFactorySpec Spec;
		Spec.bPresent = true;
		Spec.Industry = ECompanyType::Electronics;
		Spec.Slots.Init(EDioramaSlotState::Running, SlotCount);
		Spec.AutomationTier = Automation;
		Spec.QualityTier = Quality;
		Spec.StorageTier = Storage;
		Spec.RushTier = Rush;
		return Spec;
	}

	FDioramaModuleBuild DioFacTestBuild(const FDioramaFactorySpec& Spec, bool bMiniature)
	{
		FDioramaModuleBuild Build;
		DioramaModules::BuildFactory(Spec, bMiniature, Build);
		return Build;
	}

	// 로봇팔은 리그 형상이라 정지 형상과 합쳐 센다.
	int32 DioFacTestTriangles(const FDioramaFactorySpec& Spec)
	{
		const FDioramaModuleBuild Build = DioFacTestBuild(Spec, false);
		int32 Count = Build.Static.NumTriangles();
		for (const FDioramaRigDesc& Rig : Build.Rigs)
		{
			Count += Rig.Geometry.NumTriangles();
		}
		return Count;
	}

	TArray<const FDioramaRigDesc*> DioFacTestRigs(const FDioramaModuleBuild& Build, EDioramaRigKind Kind)
	{
		TArray<const FDioramaRigDesc*> Found;
		for (const FDioramaRigDesc& Rig : Build.Rigs)
		{
			if (Rig.Kind == Kind)
			{
				Found.Add(&Rig);
			}
		}
		return Found;
	}

	bool DioFacTestRectEquals(const FBox2D& Actual, const FBox2D& Expected)
	{
		return Actual.bIsValid
			&& Actual.Min.Equals(Expected.Min, kDioFacTestTolerance)
			&& Actual.Max.Equals(Expected.Max, kDioFacTestTolerance);
	}

	FString DioFacTestRectText(const FBox2D& Rect)
	{
		return FString::Printf(TEXT("((%.3f,%.3f),(%.3f,%.3f))"), Rect.Min.X, Rect.Min.Y, Rect.Max.X, Rect.Max.Y);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROverseasModuleFactoryRectLiveTest,
	"CGR.Overseas.Module.FactoryRectLive",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGROverseasModuleFactoryRectLiveTest::RunTest(const FString& Parameters)
{
	const FBox2D Expected(FVector2D(-23.0, -15.0), FVector2D(23.0, 15.0));

	const FBox2D Rect3 = DioramaModules::FactoryRect(DioFacTestSpec(3), false);
	TestTrue(FString::Printf(TEXT("slot3 live rect %s"), *DioFacTestRectText(Rect3)), DioFacTestRectEquals(Rect3, Expected));

	const FBox2D Rect6 = DioramaModules::FactoryRect(DioFacTestSpec(6, 3, 3, 3, 3), false);
	TestTrue(FString::Printf(TEXT("slot6 tier3 live rect %s"), *DioFacTestRectText(Rect6)), DioFacTestRectEquals(Rect6, Expected));

	const FDioramaModuleBuild Build = DioFacTestBuild(DioFacTestSpec(3), false);
	TestTrue(TEXT("build module = Factory"), Build.Module == EDioramaModule::Factory);
	TestTrue(FString::Printf(TEXT("build rect %s"), *DioFacTestRectText(Build.RectXZ)), DioFacTestRectEquals(Build.RectXZ, Expected));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROverseasModuleFactoryRectMiniTest,
	"CGR.Overseas.Module.FactoryRectMini",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGROverseasModuleFactoryRectMiniTest::RunTest(const FString& Parameters)
{
	const FBox2D Rect3 = DioramaModules::FactoryRect(DioFacTestSpec(3), true);
	TestTrue(FString::Printf(TEXT("slot3 rush0 %s"), *DioFacTestRectText(Rect3)),
		DioFacTestRectEquals(Rect3, FBox2D(FVector2D(-22.6, -14.2), FVector2D(17.4, 1.0))));

	const FBox2D Rect6 = DioramaModules::FactoryRect(DioFacTestSpec(6), true);
	TestTrue(FString::Printf(TEXT("slot6 z1 %.4f"), Rect6.Max.Y), FMath::IsNearlyEqual(Rect6.Max.Y, 10.6, kDioFacTestTolerance));
	TestTrue(FString::Printf(TEXT("slot6 x1 %.4f"), Rect6.Max.X), FMath::IsNearlyEqual(Rect6.Max.X, 17.4, kDioFacTestTolerance));

	const FBox2D RectRush = DioramaModules::FactoryRect(DioFacTestSpec(3, 0, 0, 0, 1), true);
	TestTrue(FString::Printf(TEXT("rush1 x1 %.4f"), RectRush.Max.X), FMath::IsNearlyEqual(RectRush.Max.X, 21.8, kDioFacTestTolerance));
	TestTrue(FString::Printf(TEXT("rush1 z1 %.4f"), RectRush.Max.Y), FMath::IsNearlyEqual(RectRush.Max.Y, 1.0, kDioFacTestTolerance));

	const FDioramaModuleBuild Build = DioFacTestBuild(DioFacTestSpec(6), true);
	TestTrue(FString::Printf(TEXT("mini build rect %s"), *DioFacTestRectText(Build.RectXZ)), DioFacTestRectEquals(Build.RectXZ, Rect6));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROverseasModuleFactoryGrowsWithTiersTest,
	"CGR.Overseas.Module.FactoryGrowsWithTiers",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGROverseasModuleFactoryGrowsWithTiersTest::RunTest(const FString& Parameters)
{
	const int32 Base = DioFacTestTriangles(DioFacTestSpec(3));
	const int32 Automation1 = DioFacTestTriangles(DioFacTestSpec(3, 1));
	const int32 Automation3 = DioFacTestTriangles(DioFacTestSpec(3, 3));
	const int32 Slot6 = DioFacTestTriangles(DioFacTestSpec(6));
	const int32 Quality3 = DioFacTestTriangles(DioFacTestSpec(3, 0, 3));
	const int32 Storage3 = DioFacTestTriangles(DioFacTestSpec(3, 0, 0, 3));
	const int32 Rush3 = DioFacTestTriangles(DioFacTestSpec(3, 0, 0, 0, 3));

	TestTrue(FString::Printf(TEXT("base %d > 0"), Base), Base > 0);
	TestTrue(FString::Printf(TEXT("base %d < automation1 %d"), Base, Automation1), Base < Automation1);
	TestTrue(FString::Printf(TEXT("automation1 %d < automation3 %d"), Automation1, Automation3), Automation1 < Automation3);
	TestTrue(FString::Printf(TEXT("slot3 %d < slot6 %d"), Base, Slot6), Base < Slot6);
	TestTrue(FString::Printf(TEXT("quality0 %d < quality3 %d"), Base, Quality3), Base < Quality3);
	TestTrue(FString::Printf(TEXT("storage0 %d < storage3 %d"), Base, Storage3), Base < Storage3);
	TestTrue(FString::Printf(TEXT("rush0 %d < rush3 %d"), Base, Rush3), Base < Rush3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROverseasModuleFactoryRigsTest,
	"CGR.Overseas.Module.FactoryRigs",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGROverseasModuleFactoryRigsTest::RunTest(const FString& Parameters)
{
	FDioramaFactorySpec Spec = DioFacTestSpec(4);
	Spec.Slots = { EDioramaSlotState::Running, EDioramaSlotState::Completed, EDioramaSlotState::Empty, EDioramaSlotState::Empty };

	const FDioramaModuleBuild Build = DioFacTestBuild(Spec, false);
	const TArray<const FDioramaRigDesc*> Belts = DioFacTestRigs(Build, EDioramaRigKind::BeltPackets);
	TestEqual(TEXT("belt packet rigs"), Belts.Num(), 4);
	const bool ExpectedActive[4] = { true, false, false, false };
	for (int32 Index = 0; Index < FMath::Min(Belts.Num(), 4); ++Index)
	{
		const FDioramaRigDesc& Belt = *Belts[Index];
		TestEqual(FString::Printf(TEXT("belt %d slot"), Index), Belt.Slot, Index);
		TestTrue(FString::Printf(TEXT("belt %d active == %d"), Index, ExpectedActive[Index] ? 1 : 0), Belt.bActive == ExpectedActive[Index]);
		TestTrue(FString::Printf(TEXT("belt %d runs along +x (UE +Y)"), Index), Belt.PathEndUE.Y > Belt.PathStartUE.Y);
		TestTrue(FString::Printf(TEXT("belt %d speed %.2f"), Index, Belt.Speed), FMath::IsNearlyEqual(Belt.Speed, 1.7f));
		TestTrue(FString::Printf(TEXT("belt %d packet size"), Index), Belt.PacketSizeUE.GetMin() > 0.0);
	}

	TestEqual(TEXT("automation0 arms"), DioFacTestRigs(Build, EDioramaRigKind::RobotArm).Num(), 0);

	Spec.AutomationTier = 1;
	const TArray<const FDioramaRigDesc*> Arms1 = DioFacTestRigs(DioFacTestBuild(Spec, false), EDioramaRigKind::RobotArm);
	TestEqual(TEXT("automation1 arms = 1 per line"), Arms1.Num(), 4);

	Spec.AutomationTier = 2;
	const FDioramaModuleBuild Build2 = DioFacTestBuild(Spec, false);
	const TArray<const FDioramaRigDesc*> Arms2 = DioFacTestRigs(Build2, EDioramaRigKind::RobotArm);
	TestEqual(TEXT("automation2 arms = 2 per line"), Arms2.Num(), 8);
	for (const FDioramaRigDesc* Arm : Arms2)
	{
		TestTrue(TEXT("arm has geometry"), Arm->Geometry.NumTriangles() > 0);
		TestTrue(FString::Printf(TEXT("arm slot %d in range"), Arm->Slot), Arm->Slot >= 0 && Arm->Slot < 4);
		TestTrue(FString::Printf(TEXT("arm sway %.2f deg / %.2f s"), Arm->Amplitude, Arm->Period), Arm->Amplitude > 0.f && Arm->Period > 0.f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROverseasModuleFactoryMiniNoYardTest,
	"CGR.Overseas.Module.FactoryMiniNoYard",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGROverseasModuleFactoryMiniNoYardTest::RunTest(const FString& Parameters)
{
	const FDioramaFactorySpec Spec = DioFacTestSpec(3);
	const FDioramaModuleBuild Mini = DioFacTestBuild(Spec, true);
	const FDioramaModuleBuild Live = DioFacTestBuild(Spec, false);

	TestEqual(TEXT("mini trucks"), DioFacTestRigs(Mini, EDioramaRigKind::Truck).Num(), 0);
	const int32 LiveTrucks = DioFacTestRigs(Live, EDioramaRigKind::Truck).Num();
	TestTrue(FString::Printf(TEXT("live trucks %d >= 1"), LiveTrucks), LiveTrucks >= 1);
	for (const FDioramaRigDesc* Truck : DioFacTestRigs(Live, EDioramaRigKind::Truck))
	{
		TestTrue(TEXT("truck geometry empty (stage uses SM_Truck_Large)"), Truck->Geometry.IsEmpty());
		TestTrue(TEXT("truck has a drive path"), !Truck->PathStartUE.Equals(Truck->PathEndUE));
	}
	TestTrue(FString::Printf(TEXT("mini static %d < live static %d"), Mini.Static.NumTriangles(), Live.Static.NumTriangles()),
		Mini.Static.NumTriangles() < Live.Static.NumTriangles());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROverseasModuleFactoryBoundsTest,
	"CGR.Overseas.Module.FactoryBounds",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGROverseasModuleFactoryBoundsTest::RunTest(const FString& Parameters)
{
	auto CheckInside = [this](const TCHAR* Label, const FDioramaFactorySpec& Spec, bool bMiniature)
	{
		const FDioramaModuleBuild Build = DioFacTestBuild(Spec, bMiniature);
		const FBox Bounds = Build.Static.GetBoundsUE();
		TestTrue(FString::Printf(TEXT("%s bounds valid"), Label), Bounds.IsValid != 0);

		// 시안 (x, z) → UE (X = −z, Y = x) × 100, 여유 1m
		const FBox2D& Rect = Build.RectXZ;
		const double MarginCm = 100.0;
		const double MinX = -Rect.Max.Y * 100.0 - MarginCm;
		const double MaxX = -Rect.Min.Y * 100.0 + MarginCm;
		const double MinY = Rect.Min.X * 100.0 - MarginCm;
		const double MaxY = Rect.Max.X * 100.0 + MarginCm;
		TestTrue(FString::Printf(TEXT("%s UE X [%.1f, %.1f] within [%.1f, %.1f]"), Label, Bounds.Min.X, Bounds.Max.X, MinX, MaxX),
			Bounds.Min.X >= MinX && Bounds.Max.X <= MaxX);
		TestTrue(FString::Printf(TEXT("%s UE Y [%.1f, %.1f] within [%.1f, %.1f]"), Label, Bounds.Min.Y, Bounds.Max.Y, MinY, MaxY),
			Bounds.Min.Y >= MinY && Bounds.Max.Y <= MaxY);
	};

	CheckInside(TEXT("live slot3 tier0"), DioFacTestSpec(3), false);

	FDioramaFactorySpec Full = DioFacTestSpec(6, 3, 3, 3, 3);
	Full.Industry = ECompanyType::Automobile;
	Full.Slots = { EDioramaSlotState::Completed, EDioramaSlotState::Running, EDioramaSlotState::Completed,
		EDioramaSlotState::Empty, EDioramaSlotState::Running, EDioramaSlotState::Completed };
	CheckInside(TEXT("live slot6 tier3 auto"), Full, false);

	FDioramaFactorySpec Five = DioFacTestSpec(5, 2, 2, 2, 2);
	Five.Slots[1] = EDioramaSlotState::Completed;
	CheckInside(TEXT("live slot5 tier2 elec"), Five, false);

	CheckInside(TEXT("mini slot6 tier3 auto"), Full, true);
	CheckInside(TEXT("mini slot3 tier0"), DioFacTestSpec(3), true);
	return true;
}

#endif
