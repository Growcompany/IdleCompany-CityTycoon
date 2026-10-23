#include "Misc/AutomationTest.h"
#include "Entity/OverseasDiorama/DioramaModules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	// 시안 조명탑 머리 발광면 색(MB(0xFFE7A3)) — 광산 모듈에서 이 색을 쓰는 건 그 판 하나뿐이고 판 1장 = 상자 12삼각형.
	constexpr uint32 kDioMineTestLampRGB = 0xFFE7A3;
	constexpr int32 kDioMineTestBoxTriangles = 12;

	FDioramaMineSpec DioMineTestSpec(const TArray<EResourceType>& Resources)
	{
		FDioramaMineSpec Spec;
		Spec.bPresent = true;
		for (const EResourceType Resource : Resources)
		{
			FDioramaMineUnit Unit;
			Unit.Resource = Resource;
			Unit.State = EDioramaSlotState::Running;
			Unit.Fill01 = 0.5f;
			Spec.Units.Add(Unit);
		}
		return Spec;
	}

	FDioramaModuleBuild DioMineTestBuild(const FDioramaMineSpec& Spec, bool bMiniature = false)
	{
		FDioramaModuleBuild Build;
		DioramaModules::BuildMine(Spec, bMiniature, Build);
		return Build;
	}

	int32 DioMineTestCountRigs(const FDioramaModuleBuild& Build, EDioramaRigKind Kind)
	{
		int32 Count = 0;
		for (const FDioramaRigDesc& RigDesc : Build.Rigs)
		{
			Count += RigDesc.Kind == Kind ? 1 : 0;
		}
		return Count;
	}

	int32 DioMineTestBucketTriangles(const FDioramaGeometry& Geo, uint32 RGB)
	{
		const FDioramaMeshBucket* Bucket = Geo.GetBuckets().Find(DioramaModules::Hex(RGB).ToPackedARGB());
		return Bucket ? Bucket->Triangles.Num() / 3 : 0;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROverseasModuleMineRectTest,
	"CGR.Overseas.Module.MineRect",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGROverseasModuleMineRectTest::RunTest(const FString& Parameters)
{
	const FDioramaMineSpec One = DioMineTestSpec({ EResourceType::IronOre });
	const FBox2D RectOne = DioramaModules::MineRect(One);
	TestTrue(TEXT("unit1 auto0 min"), RectOne.Min.Equals(FVector2D(-7.2, -8.2), 1e-4));
	TestTrue(TEXT("unit1 auto0 max"), RectOne.Max.Equals(FVector2D(7.2, 6.8), 1e-4));

	FDioramaMineSpec Five = DioMineTestSpec({ EResourceType::IronOre, EResourceType::IronOre, EResourceType::IronOre, EResourceType::IronOre, EResourceType::IronOre });
	Five.bAutoClaim = true;
	const FBox2D RectFive = DioramaModules::MineRect(Five);
	TestTrue(TEXT("unit5 auto1 min"), RectFive.Min.Equals(FVector2D(-22.4, -8.2), 1e-4));
	TestTrue(TEXT("unit5 auto1 max"), RectFive.Max.Equals(FVector2D(22.4, 10.0), 1e-4));

	const FDioramaModuleBuild Build = DioMineTestBuild(Five);
	TestTrue(TEXT("BuildMine RectXZ == MineRect"), Build.RectXZ.Min.Equals(RectFive.Min, 1e-6) && Build.RectXZ.Max.Equals(RectFive.Max, 1e-6));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROverseasModuleMinePumpOnlyTest,
	"CGR.Overseas.Module.MinePumpOnly",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGROverseasModuleMinePumpOnlyTest::RunTest(const FString& Parameters)
{
	FDioramaMineSpec Spec = DioMineTestSpec({ EResourceType::Oil, EResourceType::Oil });
	Spec.RateTier = 1;
	const FDioramaModuleBuild Build = DioMineTestBuild(Spec);
	TestEqual(TEXT("OilPump"), DioMineTestCountRigs(Build, EDioramaRigKind::OilPump), 2);
	TestEqual(TEXT("Excavator"), DioMineTestCountRigs(Build, EDioramaRigKind::Excavator), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROverseasModuleMineLumberHasPinesTest,
	"CGR.Overseas.Module.MineLumberHasPines",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGROverseasModuleMineLumberHasPinesTest::RunTest(const FString& Parameters)
{
	const FDioramaModuleBuild Build = DioMineTestBuild(DioMineTestSpec({ EResourceType::Wood }));
	TestTrue(TEXT("pine bucket"), Build.Static.GetBuckets().Contains(DioramaModules::Hex(DioramaPalette::Pine).ToPackedARGB()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROverseasModuleRefineryTest,
	"CGR.Overseas.Module.Refinery",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGROverseasModuleRefineryTest::RunTest(const FString& Parameters)
{
	FDioramaMineSpec OilS3 = DioMineTestSpec({ EResourceType::Oil });
	OilS3.StorageTier = 3;
	TestEqual(TEXT("all oil, storage 3"), DioMineTestCountRigs(DioMineTestBuild(OilS3), EDioramaRigKind::Flare), 1);

	FDioramaMineSpec OilS2 = DioMineTestSpec({ EResourceType::Oil });
	OilS2.StorageTier = 2;
	TestEqual(TEXT("all oil, storage 2"), DioMineTestCountRigs(DioMineTestBuild(OilS2), EDioramaRigKind::Flare), 0);

	FDioramaMineSpec MixedS3 = DioMineTestSpec({ EResourceType::Oil, EResourceType::IronOre });
	MixedS3.StorageTier = 3;
	TestEqual(TEXT("oil + iron ore, storage 3"), DioMineTestCountRigs(DioMineTestBuild(MixedS3), EDioramaRigKind::Flare), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROverseasModuleMineAutoLineTest,
	"CGR.Overseas.Module.MineAutoLine",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGROverseasModuleMineAutoLineTest::RunTest(const FString& Parameters)
{
	FDioramaMineSpec Spec = DioMineTestSpec({ EResourceType::IronOre, EResourceType::IronOre });
	TestEqual(TEXT("no auto claim"), DioMineTestCountRigs(DioMineTestBuild(Spec), EDioramaRigKind::BeltPackets), 0);

	Spec.bAutoClaim = true;
	TestTrue(TEXT("auto claim"), DioMineTestCountRigs(DioMineTestBuild(Spec), EDioramaRigKind::BeltPackets) >= 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROverseasModuleMineLightsByOfflineTest,
	"CGR.Overseas.Module.MineLightsByOffline",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGROverseasModuleMineLightsByOfflineTest::RunTest(const FString& Parameters)
{
	const int32 Expected[4] = { 0, 1, 2, 4 };
	for (int32 Tier = 0; Tier < 4; ++Tier)
	{
		FDioramaMineSpec Spec = DioMineTestSpec({ EResourceType::IronOre });
		Spec.OfflineTier = Tier;
		const int32 LampTriangles = DioMineTestBucketTriangles(DioMineTestBuild(Spec).Static, kDioMineTestLampRGB);
		TestEqual(*FString::Printf(TEXT("offline %d towers"), Tier), LampTriangles / kDioMineTestBoxTriangles, Expected[Tier]);
		TestEqual(*FString::Printf(TEXT("offline %d whole boxes"), Tier), LampTriangles % kDioMineTestBoxTriangles, 0);
	}
	return true;
}

#endif
