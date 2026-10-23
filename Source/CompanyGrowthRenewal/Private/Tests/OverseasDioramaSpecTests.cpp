#include "Misc/AutomationTest.h"
#include "Entity/OverseasDiorama/OverseasDioramaTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FDioramaSpecInputs DioSpecFactoryInputs(int32 MaxLines, TArray<bool> Completed)
	{
		FDioramaSpecInputs In;
		In.Country = ECountryType::Korea;
		In.bFactory = true;
		In.Industry = ECompanyType::Electronics;
		In.FactoryMaxLines = MaxLines;
		In.FactoryLineCompleted = MoveTemp(Completed);
		return In;
	}

	FDioramaMineUnit DioSpecMineUnit(EResourceType Resource, EDioramaSlotState State, float Fill)
	{
		FDioramaMineUnit Unit;
		Unit.Resource = Resource;
		Unit.State = State;
		Unit.Fill01 = Fill;
		return Unit;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverseasSpecAppearanceTierTest, "CGR.Overseas.Spec.AppearanceTier",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FOverseasSpecAppearanceTierTest::RunTest(const FString& Parameters)
{
	const TArray<int32> Levels = { 1, 25, 100 };
	TestEqual(TEXT("lv0"), OverseasDiorama::AppearanceTier(0, Levels), 0);
	TestEqual(TEXT("lv1"), OverseasDiorama::AppearanceTier(1, Levels), 1);
	TestEqual(TEXT("lv24"), OverseasDiorama::AppearanceTier(24, Levels), 1);
	TestEqual(TEXT("lv25"), OverseasDiorama::AppearanceTier(25, Levels), 2);
	TestEqual(TEXT("lv999"), OverseasDiorama::AppearanceTier(999, Levels), 3);
	TestEqual(TEXT("empty"), OverseasDiorama::AppearanceTier(50, TArray<int32>()), 0);
	TestEqual(TEXT("autoclaim"), OverseasDiorama::AppearanceTier(1, TArray<int32>{ 1 }), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverseasSpecMineKindTest, "CGR.Overseas.Spec.MineKind",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FOverseasSpecMineKindTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("oil"), OverseasDiorama::MineKindFor(EResourceType::Oil) == EDioramaMineKind::Pump);
	TestTrue(TEXT("wood"), OverseasDiorama::MineKindFor(EResourceType::Wood) == EDioramaMineKind::Lumber);
	TestTrue(TEXT("iron"), OverseasDiorama::MineKindFor(EResourceType::IronOre) == EDioramaMineKind::Pit);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverseasSpecFactorySlotsTest, "CGR.Overseas.Spec.FactorySlots",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FOverseasSpecFactorySlotsTest::RunTest(const FString& Parameters)
{
	const FOverseasDioramaSpec Spec = OverseasDiorama::AssembleSpec(DioSpecFactoryInputs(4, { false, true }));
	TestTrue(TEXT("present"), Spec.Factory.bPresent);
	TestEqual(TEXT("slots"), Spec.Factory.Slots.Num(), 4);
	if (Spec.Factory.Slots.Num() == 4)
	{
		TestTrue(TEXT("s0 running"), Spec.Factory.Slots[0] == EDioramaSlotState::Running);
		TestTrue(TEXT("s1 completed"), Spec.Factory.Slots[1] == EDioramaSlotState::Completed);
		TestTrue(TEXT("s2 empty"), Spec.Factory.Slots[2] == EDioramaSlotState::Empty);
		TestTrue(TEXT("s3 empty"), Spec.Factory.Slots[3] == EDioramaSlotState::Empty);
	}
	TestEqual(TEXT("line tier"), Spec.Factory.LineTier(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverseasSpecFactorySlotClampTest, "CGR.Overseas.Spec.FactorySlotClamp",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FOverseasSpecFactorySlotClampTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("min 3"), OverseasDiorama::AssembleSpec(DioSpecFactoryInputs(0, {})).Factory.Slots.Num(), 3);
	TestEqual(TEXT("max 6"), OverseasDiorama::AssembleSpec(DioSpecFactoryInputs(9, {})).Factory.Slots.Num(), 6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverseasSpecMineUnitsTest, "CGR.Overseas.Spec.MineUnits",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FOverseasSpecMineUnitsTest::RunTest(const FString& Parameters)
{
	FDioramaSpecInputs In;
	In.Country = ECountryType::China;
	In.bMine = true;
	In.MinableResources = { EResourceType::RareEarth, EResourceType::IronOre };
	In.MineMaxLines = 3;
	In.MineActiveLines = { DioSpecMineUnit(EResourceType::IronOre, EDioramaSlotState::Running, 0.4f) };
	const FOverseasDioramaSpec Spec = OverseasDiorama::AssembleSpec(In);
	TestEqual(TEXT("units"), Spec.Mine.Units.Num(), 3);
	if (Spec.Mine.Units.Num() == 3)
	{
		TestTrue(TEXT("u0 iron running"), Spec.Mine.Units[0].Resource == EResourceType::IronOre && Spec.Mine.Units[0].State == EDioramaSlotState::Running);
		TestTrue(TEXT("u1 rare earth idle"), Spec.Mine.Units[1].Resource == EResourceType::RareEarth && Spec.Mine.Units[1].State == EDioramaSlotState::Empty);
		TestTrue(TEXT("u2 iron idle"), Spec.Mine.Units[2].Resource == EResourceType::IronOre && Spec.Mine.Units[2].State == EDioramaSlotState::Empty);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverseasSpecHubOnlyTest, "CGR.Overseas.Spec.HubOnly",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FOverseasSpecHubOnlyTest::RunTest(const FString& Parameters)
{
	FDioramaSpecInputs In;
	In.Country = ECountryType::Singapore;
	In.bPort = true;
	const FOverseasDioramaSpec Spec = OverseasDiorama::AssembleSpec(In);
	TestTrue(TEXT("port"), Spec.Port.bPresent);
	TestEqual(TEXT("cranes"), Spec.Port.CraneCount, 3);
	TestTrue(TEXT("primary port"), Spec.PrimaryModule() == EDioramaModule::Port);
	TestFalse(TEXT("no factory"), Spec.HasModule(EDioramaModule::Factory));
	TestTrue(TEXT("has any"), Spec.HasAny());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverseasSpecMultiCraneTest, "CGR.Overseas.Spec.MultiCraneCount",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FOverseasSpecMultiCraneTest::RunTest(const FString& Parameters)
{
	FDioramaSpecInputs In = DioSpecFactoryInputs(3, {});
	In.bPort = true;
	const FOverseasDioramaSpec Spec = OverseasDiorama::AssembleSpec(In);
	TestEqual(TEXT("cranes"), Spec.Port.CraneCount, 2);
	TestTrue(TEXT("primary factory"), Spec.PrimaryModule() == EDioramaModule::Factory);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverseasSpecMineNoResourcesTest, "CGR.Overseas.Spec.MineNoResources",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FOverseasSpecMineNoResourcesTest::RunTest(const FString& Parameters)
{
	FDioramaSpecInputs In;
	In.Country = ECountryType::Australia;
	In.bMine = true;
	In.MineMaxLines = 2;
	const FOverseasDioramaSpec Spec = OverseasDiorama::AssembleSpec(In);
	TestFalse(TEXT("no mine"), Spec.Mine.bPresent);
	TestFalse(TEXT("nothing"), Spec.HasAny());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverseasSpecLookHashTest, "CGR.Overseas.Spec.LookHashIgnoresFill",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FOverseasSpecLookHashTest::RunTest(const FString& Parameters)
{
	FDioramaSpecInputs In = DioSpecFactoryInputs(4, { false });
	In.bMine = true;
	In.MinableResources = { EResourceType::Oil };
	In.MineMaxLines = 2;
	In.MineActiveLines = { DioSpecMineUnit(EResourceType::Oil, EDioramaSlotState::Running, 0.1f) };
	const uint32 Low = OverseasDiorama::AssembleSpec(In).GetLookHash();

	In.MineActiveLines[0].Fill01 = 0.9f;
	TestEqual(TEXT("fill ignored"), OverseasDiorama::AssembleSpec(In).GetLookHash(), Low);

	In.FactoryLineCompleted[0] = true;
	TestNotEqual(TEXT("slot state counts"), OverseasDiorama::AssembleSpec(In).GetLookHash(), Low);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverseasSpecAllOilTest, "CGR.Overseas.Spec.AllOil",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FOverseasSpecAllOilTest::RunTest(const FString& Parameters)
{
	FDioramaMineSpec Mine;
	Mine.Units = { DioSpecMineUnit(EResourceType::Oil, EDioramaSlotState::Empty, 0.f), DioSpecMineUnit(EResourceType::Oil, EDioramaSlotState::Running, 0.f) };
	TestTrue(TEXT("all oil"), Mine.IsAllOil());
	Mine.Units.Add(DioSpecMineUnit(EResourceType::Wood, EDioramaSlotState::Empty, 0.f));
	TestFalse(TEXT("oil + wood"), Mine.IsAllOil());
	return true;
}

#endif
