#include "Misc/AutomationTest.h"
#include "Manager/ManufacturingGuideCountryRules.h"
#include "Table/ProductRecipeTable.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FManufacturingGuideCountrySelectionTest,
	"CGR.GoalStep.ManufacturingCountrySelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FManufacturingGuideCountrySelectionTest::RunTest(const FString& Parameters)
{
	FProductRecipeTable Recipe;
	Recipe.Copper = 5;
	Recipe.Silicon = 3;
	Recipe.Oil = 3;
	TArray<TPair<EResourceType, int32>> Materials;
	Recipe.CollectMaterials(Materials);
	const TArray<EResourceType> Needed = { EResourceType::Copper, EResourceType::Silicon, EResourceType::Oil };

	FCountryInfoTable UnrelatedMine;
	UnrelatedMine.bSupportsMine = true;
	UnrelatedMine.MinableResources = { EResourceType::Gold, EResourceType::RareEarth, EResourceType::DiamondOre };
	TestEqual(TEXT("active line for unrelated ore cannot be guided"),
		ManufacturingGuideCountryRules::MinePriority(UnrelatedMine, Materials, Needed, { EResourceType::Gold }), INDEX_NONE);

	FCountryInfoTable RelevantMine;
	RelevantMine.bSupportsMine = true;
	RelevantMine.MinableResources = { EResourceType::Copper, EResourceType::Wood };
	const int32 EmptyLinePriority = ManufacturingGuideCountryRules::MinePriority(RelevantMine, Materials, Needed, {});
	const int32 ActiveLinePriority = ManufacturingGuideCountryRules::MinePriority(RelevantMine, Materials, Needed,
		{ EResourceType::Copper });
	TestTrue(TEXT("a relevant active mining line takes priority"), ActiveLinePriority > EmptyLinePriority);
	TestEqual(TEXT("an unrelated active line does not gain priority"),
		ManufacturingGuideCountryRules::MinePriority(RelevantMine, Materials, Needed, { EResourceType::Wood }), EmptyLinePriority);

	FCountryInfoTable OtherIndustryFactory;
	OtherIndustryFactory.bSupportsFactory = true;
	OtherIndustryFactory.SupportedIndustries = { ECompanyType::Automobile };
	TestFalse(TEXT("factory with another industry is excluded"),
		ManufacturingGuideCountryRules::SupportsFactoryIndustry(OtherIndustryFactory, ECompanyType::Electronics));
	OtherIndustryFactory.SupportedIndustries.Add(ECompanyType::Electronics);
	TestTrue(TEXT("factory with target industry is eligible"),
		ManufacturingGuideCountryRules::SupportsFactoryIndustry(OtherIndustryFactory, ECompanyType::Electronics));

	TMap<EResourceType, int64> Inventory;
	Inventory.Add(EResourceType::Copper, 2);
	Inventory.Add(EResourceType::Silicon, 2);
	Inventory.Add(EResourceType::Oil, 7);
	TArray<EResourceType> Missing = ManufacturingGuideCountryRules::MissingMaterials(Materials, Inventory, 1);
	TestEqual(TEXT("one product misses two materials despite ample oil"), Missing.Num(), 2);
	TestTrue(TEXT("copper is short"), Missing.Contains(EResourceType::Copper));
	TestTrue(TEXT("silicon is short"), Missing.Contains(EResourceType::Silicon));
	TestFalse(TEXT("oil is sufficient"), Missing.Contains(EResourceType::Oil));
	TestTrue(TEXT("pending product needs material recovery before starting a line"),
		ManufacturingGuideCountryRules::ShouldRecoverMaterials(false, true, true, Missing));
	TestFalse(TEXT("an active factory line stays on receipt even after spending materials"),
		ManufacturingGuideCountryRules::ShouldRecoverMaterials(true, true, true, Missing));
	Inventory.Add(EResourceType::Copper, 5);
	Missing = ManufacturingGuideCountryRules::MissingMaterials(Materials, Inventory, 1);
	TestEqual(TEXT("collecting copper leaves only silicon short"), Missing.Num(), 1);
	TestTrue(TEXT("silicon remains short"), Missing.Contains(EResourceType::Silicon));
	Inventory.Add(EResourceType::Silicon, 3);
	Missing = ManufacturingGuideCountryRules::MissingMaterials(Materials, Inventory, 1);
	TestFalse(TEXT("one product can return to the factory"),
		ManufacturingGuideCountryRules::ShouldRecoverMaterials(false, true, true, Missing));
	TestTrue(TEXT("the full reservation would still be short, so recovery uses one product"),
		!ManufacturingGuideCountryRules::MissingMaterials(Materials, Inventory, 9).IsEmpty());
	return true;
}

#endif
