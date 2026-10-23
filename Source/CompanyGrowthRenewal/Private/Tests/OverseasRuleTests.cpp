#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Core/CGGameInstance.h"
#include "Data/WorldFactoryUpgradeData.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Manager/MineManager.h"
#include "Manager/OverseasActions.h"
#include "Manager/ProductionOrderManager.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/WorldFactoryManager.h"
#include "Manager/WorldMapManager.h"
#include "Misc/Paths.h"
#include "Table/ProductRecipeTable.h"
#include "Table/CountryInfoTable.h"
#include "Table/ResourceInfo.h"

namespace OverseasRuleTests
{
	constexpr ECountryType kRuleFactoryCountry = ECountryType::Korea;
	constexpr ECompanyType kRuleIndustry = ECompanyType::Electronics;
	constexpr int32 kRuleProductIndex = 3;
	constexpr EQualityGrade kRuleGrade = EQualityGrade::A;
	constexpr ECountryType kRuleMineCountry = ECountryType::Australia;
	constexpr EResourceType kRuleMineResource = EResourceType::IronOre;

	// 고정 GameInstance 가 닫히며 남기는 저장이 플레이어 세이브를 덮지 않게 격리 폴더에서만 돈다(FactoryExpansionEmployeeTests 와 같은 자세).
	bool ShouldRun(FAutomationTestBase& Test)
	{
		if (!FPaths::ProjectSavedDir().Contains(TEXT("Saved_OverseasDiorama")))
		{
			Test.AddWarning(TEXT("Skipped: use -saveddirsuffix=OverseasDiorama to isolate fixture saves"));
			return false;
		}
		return Test.TestNull(TEXT("No playable game instance is active"), UCGGameInstance::GetInstance());
	}

	struct FRuleFixture
	{
		UGameInstance* Instance = nullptr;
		UWorld* TestWorld = nullptr;
		UWorldFactoryManager* Factory = nullptr;
		UMineManager* Mine = nullptr;
		UWorldMapManager* WorldMap = nullptr;
		UResourceItemManager* Resources = nullptr;

		FRuleFixture()
		{
			if (!GEngine) return;
			Instance = NewObject<UGameInstance>(GEngine);
			Instance->InitializeStandalone();
			TestWorld = Instance->GetWorld();
			if (USaveLoadManager* SaveMgr = Instance->GetSubsystem<USaveLoadManager>())
			{
				SaveMgr->SuppressSaving();
			}
			Factory = Instance->GetSubsystem<UWorldFactoryManager>();
			Mine = Instance->GetSubsystem<UMineManager>();
			WorldMap = Instance->GetSubsystem<UWorldMapManager>();
			Resources = Instance->GetSubsystem<UResourceItemManager>();
			// 라인 수 상한(공장 3·광산 1)이 빡빡해 앞선 상태가 하나라도 남으면 시작이 막힌다 — 빈 상태로 못 박는다.
			if (Factory) Factory->LoadFromSave(TMap<ECountryType, FCountryFactoryData>());
			if (Mine) Mine->LoadFromSave(TMap<ECountryType, FCountryMineData>());
		}

		~FRuleFixture()
		{
			if (Instance) Instance->Shutdown();
			if (TestWorld)
			{
				TestWorld->SetGameInstance(nullptr);
				if (GEngine) GEngine->DestroyWorldContext(TestWorld);
				TestWorld->DestroyWorld(false);
			}
		}

		bool IsReady() const { return Factory && Mine && WorldMap && Resources; }
	};

	int32 StartFinishedLine(FAutomationTestBase& Test, UWorldFactoryManager* Factory, int64 TargetQty)
	{
		const TSoftObjectPtr<UTexture2D> NoIcon;
		const int32 LineId = Factory->StartProduction(kRuleFactoryCountry, kRuleIndustry, kRuleProductIndex,
			FText::FromString(TEXT("Rule")), NoIcon, /*BaseRatePerSecond=*/0.01f, TargetQty, kRuleGrade);
		if (!Test.TestTrue(TEXT("Factory line starts (DT_WorldFactoryUpgradeDefinition LineExpansion allows a line)"), LineId > 0))
		{
			return INDEX_NONE;
		}
		Test.TestTrue(TEXT("Instant finish completes the line"), Factory->InstantFinishLine(kRuleFactoryCountry, LineId));
		return LineId;
	}

	bool ClaimFinishedBatch(FAutomationTestBase& Test, FRuleFixture& Fixture, int32 ProductIndex,
		EQualityGrade Grade, int64 TargetQty, int64& OutAddedQty, int32* OutLineId = nullptr)
	{
		const FIntPoint ProductKey(static_cast<int32>(kRuleIndustry), ProductIndex);
		const int64 Before = Fixture.WorldMap->GetProductAmount(ProductKey);
		const TSoftObjectPtr<UTexture2D> NoIcon;
		const int32 LineId = Fixture.Factory->StartProduction(kRuleFactoryCountry, kRuleIndustry, ProductIndex,
			FText::FromString(TEXT("Rule")), NoIcon, 1.0f, TargetQty, Grade);
		if (!Test.TestTrue(TEXT("Batch starts"), LineId > 0)) return false;
		if (!Test.TestTrue(TEXT("Batch completes"), Fixture.Factory->InstantFinishLine(kRuleFactoryCountry, LineId))) return false;
		if (!Test.TestTrue(TEXT("Batch is claimed"), Fixture.Factory->ClaimLine(kRuleFactoryCountry, LineId))) return false;
		OutAddedQty = Fixture.WorldMap->GetProductAmount(ProductKey) - Before;
		if (OutLineId) *OutLineId = LineId;
		return true;
	}

	bool StartFullMineLine(FAutomationTestBase& Test, FRuleFixture& Fixture, FMineLineState& OutFull)
	{
		// 라인 생성 비용(자원 기준가 × 한도)을 치를 자금
		Fixture.Resources->StoreResource(EResourceType::Money, 1'000'000'000'000LL, /*bShouldSave=*/false);
		Fixture.Mine->EnsureCountryLines(kRuleMineCountry);
		if (!Test.TestTrue(TEXT("Mine line is created (DT_CountryInfo: Australia mines IronOre)"),
			Fixture.Mine->CreateMineLine(kRuleMineCountry, kRuleMineResource))) return false;

		if (!Test.TestTrue(TEXT("An unfinished mine can be instantly filled"),
			Fixture.Mine->InstantFinishLine(kRuleMineCountry, kRuleMineResource))) return false;
		Test.TestFalse(TEXT("An already full mine cannot be finished twice"),
			Fixture.Mine->InstantFinishLine(kRuleMineCountry, kRuleMineResource));

		if (!Test.TestTrue(TEXT("Full mine line state is readable"),
			Fixture.Mine->GetLineState(kRuleMineCountry, kRuleMineResource, OutFull))) return false;
		return Test.TestTrue(TEXT("Mine line is saturated with stock"), OutFull.bSaturated && OutFull.CurrentQty > 0);
	}

	void ExpectMineLineRestarted(FAutomationTestBase& Test, UMineManager* Mine)
	{
		Test.TestEqual(TEXT("Claim keeps the mine line"), Mine->GetMineLines(kRuleMineCountry).Num(), 1);
		FMineLineState After;
		if (!Test.TestTrue(TEXT("Claimed mine line is still found"), Mine->GetLineState(kRuleMineCountry, kRuleMineResource, After))) return;
		Test.TestEqual(TEXT("Mine stock restarts at zero"), After.CurrentQty, int64(0));
		Test.TestFalse(TEXT("Restarted mine line is no longer saturated"), After.bSaturated);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverseasRuleMineLineExpansionCapTest,
	"CGR.Overseas.Rule.MineLineExpansionCap",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FOverseasRuleMineLineExpansionCapTest::RunTest(const FString& Parameters)
{
	namespace ORT = OverseasRuleTests;
	if (!ORT::ShouldRun(*this)) return true;
	ORT::FRuleFixture Fixture;
	if (!TestTrue(TEXT("Rule fixture is ready"), Fixture.IsReady())) return false;

	TestEqual(TEXT("Saudi has one distinct mineable resource"), Fixture.Mine->GetResourceLineCap(ECountryType::Saudi), 1);
	TestEqual(TEXT("Saudi starts with its only usable line"), Fixture.Mine->GetMaxLines(ECountryType::Saudi), 1);
	TestFalse(TEXT("Saudi cannot buy a line for a nonexistent second resource"),
		Fixture.Mine->UpgradeLevelUp(ECountryType::Saudi, EMineUpgradeType::LineExpansion));
	TestEqual(TEXT("Rejected expansion leaves Saudi at level zero"),
		Fixture.Mine->GetUpgradeLevel(ECountryType::Saudi, EMineUpgradeType::LineExpansion), 0);

	TestEqual(TEXT("Canada has three distinct mineable resources"), Fixture.Mine->GetResourceLineCap(ECountryType::Canada), 3);
	TestEqual(TEXT("Canada starts with one line"), Fixture.Mine->GetMaxLines(ECountryType::Canada), 1);
	TestTrue(TEXT("Canada can buy a second resource line"),
		Fixture.Mine->UpgradeLevelUp(ECountryType::Canada, EMineUpgradeType::LineExpansion));
	TestEqual(TEXT("Canada has two slots at level one"), Fixture.Mine->GetMaxLines(ECountryType::Canada), 2);
	TestTrue(TEXT("Canada can buy a third resource line"),
		Fixture.Mine->UpgradeLevelUp(ECountryType::Canada, EMineUpgradeType::LineExpansion));
	TestEqual(TEXT("Canada has three slots at level two"), Fixture.Mine->GetMaxLines(ECountryType::Canada), 3);
	TestFalse(TEXT("Canada cannot buy a fourth resource line"),
		Fixture.Mine->UpgradeLevelUp(ECountryType::Canada, EMineUpgradeType::LineExpansion));
	TestEqual(TEXT("Canada stays at level two"),
		Fixture.Mine->GetUpgradeLevel(ECountryType::Canada, EMineUpgradeType::LineExpansion), 2);

	TMap<ECountryType, FCountryMineData> PriorMineData;
	FCountryMineData& PriorSaudi = PriorMineData.Add(ECountryType::Saudi);
	PriorSaudi.UpgradeLevels.Add(EMineUpgradeType::LineExpansion, 1);
	Fixture.Mine->LoadFromSave(PriorMineData);
	TestEqual(TEXT("Existing Saudi level one still exposes only one usable line"),
		Fixture.Mine->GetMaxLines(ECountryType::Saudi), 1);
	TestFalse(TEXT("Existing Saudi level one cannot buy another empty slot"),
		Fixture.Mine->UpgradeLevelUp(ECountryType::Saudi, EMineUpgradeType::LineExpansion));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverseasRecipeMaterialSupplyTest,
	"CGR.Overseas.Rule.RecipeMaterialsHaveMineSource",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FOverseasRecipeMaterialSupplyTest::RunTest(const FString& Parameters)
{
	const UDataTable* CountryTable = LoadObject<UDataTable>(nullptr,
		TEXT("/Game/CompanyGrowth/Table/WorldMap/DT_CountryInfo.DT_CountryInfo"));
	if (!TestNotNull(TEXT("DT_CountryInfo is loaded"), CountryTable)) return false;

	TSet<EResourceType> MineableMaterials;
	for (const FName RowName : CountryTable->GetRowNames())
	{
		const FCountryInfoTable* Country = CountryTable->FindRow<FCountryInfoTable>(RowName, TEXT("RecipeMaterialSupply"));
		if (!Country || !Country->bSupportsMine) continue;
		for (const EResourceType Material : Country->MinableResources)
		{
			MineableMaterials.Add(Material);
		}
	}
	if (!TestTrue(TEXT("DT_CountryInfo has at least one mineable material"), !MineableMaterials.IsEmpty())) return false;
	const UDataTable* ResourceTable = LoadObject<UDataTable>(nullptr,
		TEXT("/Game/CompanyGrowth/Table/DT_Resource.DT_Resource"));
	if (!TestNotNull(TEXT("DT_Resource is loaded"), ResourceTable)) return false;
	bool bAllCovered = true;
	for (const EResourceType Material : MineableMaterials)
	{
		const FString MaterialName = UEnum::GetValueAsString(Material);
		const FName ResourceRow(*StaticEnum<EResourceType>()->GetNameStringByValue(static_cast<int64>(Material)));
		const FResourceInfo* Resource = ResourceTable->FindRow<FResourceInfo>(ResourceRow, TEXT("MineBaseRate"), false);
		if (!TestNotNull(*FString::Printf(TEXT("DT_Resource has %s"), *MaterialName), Resource))
		{
			bAllCovered = false;
			continue;
		}
		if (!TestTrue(*FString::Printf(TEXT("DT_Resource.%s has a finite positive mining rate"), *MaterialName),
			Resource->ResourceType == Material && FMath::IsFinite(Resource->MiningRatePerMinute)
				&& Resource->MiningRatePerMinute > 0.0f)) bAllCovered = false;
	}

	const UEnum* CompanyEnum = StaticEnum<ECompanyType>();
	if (!TestNotNull(TEXT("ECompanyType enum is available"), CompanyEnum)) return false;
	int32 CheckedRecipes = 0;
	for (int32 Value = 0; Value < static_cast<int32>(ECompanyType::Max); ++Value)
	{
		const ECompanyType Industry = static_cast<ECompanyType>(Value);
		if (!IsManufacturingType(Industry)) continue;

		const FString IndustryName = CompanyEnum->GetNameStringByValue(Value);
		const FString TableName = FString::Printf(TEXT("DT_Recipe_%s"), *IndustryName);
		const FString TablePath = FString::Printf(TEXT("/Game/CompanyGrowth/Table/Company/Recipe/%s.%s"),
			*TableName, *TableName);
		const UDataTable* RecipeTable = LoadObject<UDataTable>(nullptr, *TablePath);
		if (!TestNotNull(*FString::Printf(TEXT("%s is loaded"), *TableName), RecipeTable))
		{
			bAllCovered = false;
			continue;
		}
		const TArray<FName> RecipeNames = RecipeTable->GetRowNames();
		if (!TestTrue(*FString::Printf(TEXT("%s has recipes"), *TableName), !RecipeNames.IsEmpty()))
		{
			bAllCovered = false;
			continue;
		}
		for (const FName RecipeName : RecipeNames)
		{
			const FProductRecipeTable* Recipe = RecipeTable->FindRow<FProductRecipeTable>(RecipeName, TEXT("RecipeMaterialSupply"));
			if (!TestNotNull(*FString::Printf(TEXT("%s/%s is readable"), *TableName, *RecipeName.ToString()), Recipe))
			{
				bAllCovered = false;
				continue;
			}
			++CheckedRecipes;
			TArray<TPair<EResourceType, int32>> Materials;
			Recipe->CollectMaterials(Materials);
			for (const TPair<EResourceType, int32>& Material : Materials)
			{
				const FString MaterialName = UEnum::GetValueAsString(Material.Key);
				if (!TestTrue(*FString::Printf(TEXT("%s/%s requires %s (%d), but no mine country supplies it"),
					*TableName, *RecipeName.ToString(), *MaterialName, Material.Value), MineableMaterials.Contains(Material.Key)))
				{
					bAllCovered = false;
				}
			}
		}
	}
	return TestTrue(TEXT("At least one manufacturing recipe was checked"), CheckedRecipes > 0) && bAllCovered;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverseasRuleClaimFreesLineTest, "CGR.Overseas.Rule.ClaimFreesLine",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FOverseasRuleClaimFreesLineTest::RunTest(const FString& Parameters)
{
	namespace ORT = OverseasRuleTests;
	if (!ORT::ShouldRun(*this)) return true;
	ORT::FRuleFixture Fixture;
	if (!TestTrue(TEXT("Rule fixture has the overseas managers"), Fixture.IsReady())) return false;

	const int32 LineId = ORT::StartFinishedLine(*this, Fixture.Factory, 5);
	if (LineId == INDEX_NONE) return false;
	if (!TestTrue(TEXT("Completed line can be claimed"), Fixture.Factory->ClaimLine(ORT::kRuleFactoryCountry, LineId))) return false;
	TestEqual(TEXT("Claim frees the factory slot"), Fixture.Factory->GetActiveLineCount(ORT::kRuleFactoryCountry), 0);

	FWorldFactoryLineState After;
	TestFalse(TEXT("Claimed line cannot be read again"), Fixture.Factory->GetLineState(ORT::kRuleFactoryCountry, LineId, After));
	TestFalse(TEXT("Claimed line cannot finish again"), Fixture.Factory->InstantFinishLine(ORT::kRuleFactoryCountry, LineId));
	TestFalse(TEXT("Claimed line cannot be claimed again"), Fixture.Factory->ClaimLine(ORT::kRuleFactoryCountry, LineId));
	const TSoftObjectPtr<UTexture2D> NoIcon;
	const int32 NextLineId = Fixture.Factory->StartProduction(ORT::kRuleFactoryCountry, ORT::kRuleIndustry,
		ORT::kRuleProductIndex + 1, FText::FromString(TEXT("Next")), NoIcon, 1.0f, 1, ORT::kRuleGrade);
	TestTrue(TEXT("Freed slot accepts a different product"), NextLineId > LineId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverseasRuleClaimAddsGoodsTest, "CGR.Overseas.Rule.ClaimAddsGoods",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FOverseasRuleClaimAddsGoodsTest::RunTest(const FString& Parameters)
{
	namespace ORT = OverseasRuleTests;
	if (!ORT::ShouldRun(*this)) return true;
	ORT::FRuleFixture Fixture;
	if (!TestTrue(TEXT("Rule fixture has the overseas managers"), Fixture.IsReady())) return false;

	const FIntPoint Key(static_cast<int32>(ORT::kRuleIndustry), ORT::kRuleProductIndex);
	const FIntPoint NeighborKey(static_cast<int32>(ORT::kRuleIndustry), ORT::kRuleProductIndex + 1);
	const int64 Before = Fixture.WorldMap->GetProductAmount(Key);
	const int64 NeighborBefore = Fixture.WorldMap->GetProductAmount(NeighborKey);

	const int32 LineId = ORT::StartFinishedLine(*this, Fixture.Factory, 7);
	if (LineId == INDEX_NONE) return false;
	if (!TestTrue(TEXT("Completed line can be claimed"), Fixture.Factory->ClaimLine(ORT::kRuleFactoryCountry, LineId))) return false;
	TestEqual(TEXT("Warehouse grows by the claimed quantity under (CompanyType, ProductIndex)"),
		Fixture.WorldMap->GetProductAmount(Key) - Before, int64(7));
	TestEqual(TEXT("Goods carry the line's order grade"), Fixture.WorldMap->GetProductGrade(Key), ORT::kRuleGrade);
	TestEqual(TEXT("Other product keys are untouched"), Fixture.WorldMap->GetProductAmount(NeighborKey), NeighborBefore);

	TestFalse(TEXT("Duplicate claim cannot pay the same batch again"), Fixture.Factory->ClaimLine(ORT::kRuleFactoryCountry, LineId));
	TestEqual(TEXT("Duplicate claim leaves warehouse quantity unchanged"), Fixture.WorldMap->GetProductAmount(Key) - Before, int64(7));
	TMap<ECountryType, FCountryFactoryData> Saved;
	Fixture.Factory->SerializeForSave(Saved);
	Fixture.Factory->LoadFromSave(Saved);
	TestFalse(TEXT("Reloaded state cannot claim the consumed batch"), Fixture.Factory->ClaimLine(ORT::kRuleFactoryCountry, LineId));
	TestEqual(TEXT("Reloaded state cannot pay the batch again"), Fixture.WorldMap->GetProductAmount(Key) - Before, int64(7));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverseasRuleQualityBonusPaysGoodsTest,
	"CGR.Overseas.Rule.QualityBonusPaysGoods",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FOverseasRuleQualityBonusPaysGoodsTest::RunTest(const FString& Parameters)
{
	namespace ORT = OverseasRuleTests;
	if (!ORT::ShouldRun(*this)) return true;
	ORT::FRuleFixture Fixture;
	if (!TestTrue(TEXT("Rule fixture has the overseas managers"), Fixture.IsReady())) return false;
	UTableManagerSubsystem* Tables = Fixture.Instance->GetSubsystem<UTableManagerSubsystem>();
	FWorldFactoryUpgradeDefinition QualityDef;
	if (!TestTrue(TEXT("Quality upgrade definition is loaded"), Tables &&
		Tables->GetWorldFactoryUpgradeDefinition(EWorldFactoryUpgradeType::QualityControl, QualityDef))) return false;
	if (!TestTrue(TEXT("Quality upgrade has a positive quantity bonus"), QualityDef.PerLevel > 0.0f)) return false;
	if (!TestTrue(TEXT("Quality level 1 is bought"),
		Fixture.Factory->UpgradeLevelUp(ORT::kRuleFactoryCountry, EWorldFactoryUpgradeType::QualityControl))) return false;

	constexpr int64 BatchQty = 1000;
	const int64 ExpectedBonus = static_cast<int64>(FMath::FloorToDouble(
		static_cast<double>(BatchQty) * static_cast<double>(QualityDef.PerLevel) + 1e-6));
	if (!TestTrue(TEXT("The selected batch yields at least one bonus unit"), ExpectedBonus > 0)) return false;
	int64 AddedQty = 0;
	int32 LineId = INDEX_NONE;
	if (!ORT::ClaimFinishedBatch(*this, Fixture, ORT::kRuleProductIndex, ORT::kRuleGrade,
		BatchQty, AddedQty, &LineId)) return false;
	TestEqual(TEXT("Claimed warehouse goods include the DT quality bonus"), AddedQty, BatchQty + ExpectedBonus);
	TestFalse(TEXT("A second claim cannot pay the quality bonus again"),
		Fixture.Factory->ClaimLine(ORT::kRuleFactoryCountry, LineId));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverseasRuleQualityCarrySurvivesSaveAndStaysWithProductGradeTest,
	"CGR.Overseas.Rule.QualityCarrySurvivesSaveAndStaysWithProductGrade",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FOverseasRuleQualityCarrySurvivesSaveAndStaysWithProductGradeTest::RunTest(const FString& Parameters)
{
	namespace ORT = OverseasRuleTests;
	if (!ORT::ShouldRun(*this)) return true;
	ORT::FRuleFixture Fixture;
	if (!TestTrue(TEXT("Rule fixture has the overseas managers"), Fixture.IsReady())) return false;
	UTableManagerSubsystem* Tables = Fixture.Instance->GetSubsystem<UTableManagerSubsystem>();
	FWorldFactoryUpgradeDefinition QualityDef;
	if (!TestTrue(TEXT("Quality upgrade definition is loaded"), Tables &&
		Tables->GetWorldFactoryUpgradeDefinition(EWorldFactoryUpgradeType::QualityControl, QualityDef))) return false;
	if (!TestTrue(TEXT("Fixture quality bonus is fractional per item"),
		QualityDef.PerLevel > 0.0f && QualityDef.PerLevel < 1.0f)) return false;
	const int32 UnitsForBonus = FMath::CeilToInt((1.0 - 1e-6) / static_cast<double>(QualityDef.PerLevel));
	if (!TestTrue(TEXT("A few split batches can accumulate one bonus unit"),
		UnitsForBonus >= 4 && UnitsForBonus <= 1000)) return false;
	if (!TestTrue(TEXT("Quality level 1 is bought"),
		Fixture.Factory->UpgradeLevelUp(ORT::kRuleFactoryCountry, EWorldFactoryUpgradeType::QualityControl))) return false;

	const int32 FirstQty = UnitsForBonus / 2;
	const int32 SecondQty = UnitsForBonus - FirstQty - 1;
	int64 AddedQty = 0;
	if (!ORT::ClaimFinishedBatch(*this, Fixture, ORT::kRuleProductIndex, ORT::kRuleGrade,
		FirstQty, AddedQty)) return false;
	TestEqual(TEXT("First partial batch pays its base units"), AddedQty, int64(FirstQty));
	TMap<ECountryType, FCountryFactoryData> Saved;
	Fixture.Factory->SerializeForSave(Saved);
	Fixture.Factory->LoadFromSave(Saved);
	if (!ORT::ClaimFinishedBatch(*this, Fixture, ORT::kRuleProductIndex, ORT::kRuleGrade,
		SecondQty, AddedQty)) return false;
	TestEqual(TEXT("Second partial batch pays its base units"), AddedQty, int64(SecondQty));
	if (!ORT::ClaimFinishedBatch(*this, Fixture, ORT::kRuleProductIndex + 1, ORT::kRuleGrade,
		1, AddedQty)) return false;
	TestEqual(TEXT("Another product cannot consume this product's carry"), AddedQty, int64(1));
	if (!ORT::ClaimFinishedBatch(*this, Fixture, ORT::kRuleProductIndex, EQualityGrade::B,
		1, AddedQty)) return false;
	TestEqual(TEXT("Another grade cannot consume this grade's carry"), AddedQty, int64(1));
	int32 LastLineId = INDEX_NONE;
	if (!ORT::ClaimFinishedBatch(*this, Fixture, ORT::kRuleProductIndex, ORT::kRuleGrade,
		1, AddedQty, &LastLineId)) return false;
	TestEqual(TEXT("Saved fractional carry pays exactly one unit on the matching batch"), AddedQty, int64(2));
	TestFalse(TEXT("Claimed carry cannot be paid twice"),
		Fixture.Factory->ClaimLine(ORT::kRuleFactoryCountry, LastLineId));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverseasRuleAutomationSpeedsInstalledLinesTest,
	"CGR.Overseas.Rule.AutomationSpeedsInstalledLines",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FOverseasRuleAutomationSpeedsInstalledLinesTest::RunTest(const FString& Parameters)
{
	namespace ORT = OverseasRuleTests;
	if (!ORT::ShouldRun(*this)) return true;
	ORT::FRuleFixture Fixture;
	if (!TestTrue(TEXT("Rule fixture has the overseas managers"), Fixture.IsReady())) return false;
	UTableManagerSubsystem* Tables = Fixture.Instance->GetSubsystem<UTableManagerSubsystem>();
	FWorldFactoryUpgradeDefinition AutomationDef;
	if (!TestTrue(TEXT("Automation upgrade definition is loaded"), Tables &&
		Tables->GetWorldFactoryUpgradeDefinition(EWorldFactoryUpgradeType::Automation, AutomationDef))) return false;
	if (!TestTrue(TEXT("Automation has a positive speed bonus"), AutomationDef.PerLevel > 0.0f)) return false;

	const TSoftObjectPtr<UTexture2D> NoIcon;
	const int32 ActiveId = Fixture.Factory->StartProduction(ORT::kRuleFactoryCountry, ORT::kRuleIndustry,
		ORT::kRuleProductIndex, FText::FromString(TEXT("Active")), NoIcon, 0.2f, 100, ORT::kRuleGrade);
	const int32 FinishedId = Fixture.Factory->StartProduction(ORT::kRuleFactoryCountry, ORT::kRuleIndustry,
		ORT::kRuleProductIndex + 1, FText::FromString(TEXT("Finished")), NoIcon, 0.2f, 3, ORT::kRuleGrade);
	if (!TestTrue(TEXT("Both existing lines start"), ActiveId > 0 && FinishedId > 0)) return false;
	if (!TestTrue(TEXT("Second line completes before the upgrade"),
		Fixture.Factory->InstantFinishLine(ORT::kRuleFactoryCountry, FinishedId))) return false;
	TMap<ECountryType, FCountryFactoryData> Saved;
	Fixture.Factory->SerializeForSave(Saved);
	FCountryFactoryData* CountryData = Saved.Find(ORT::kRuleFactoryCountry);
	if (!TestTrue(TEXT("Existing lines are in save state"), CountryData && CountryData->ActiveLines.Num() == 2)) return false;
	CountryData->ActiveLines[0].StartWallUtc = FDateTime::UtcNow() - FTimespan::FromSeconds(12.5);
	Fixture.Factory->LoadFromSave(Saved);
	FWorldFactoryLineState Before;
	FWorldFactoryLineState FinishedBefore;
	if (!TestTrue(TEXT("Both line states are readable before upgrade"),
		Fixture.Factory->GetLineState(ORT::kRuleFactoryCountry, ActiveId, Before) &&
		Fixture.Factory->GetLineState(ORT::kRuleFactoryCountry, FinishedId, FinishedBefore))) return false;
	const double RawBefore = Before.LineElapsedSec * static_cast<double>(Before.RatePerSecond);
	if (!TestTrue(TEXT("Existing line has fractional progress before upgrade"),
		RawBefore > 2.25 && RawBefore < 2.75 && !Before.bCompleted)) return false;
	if (!TestTrue(TEXT("Automation level 1 is bought"),
		Fixture.Factory->UpgradeLevelUp(ORT::kRuleFactoryCountry, EWorldFactoryUpgradeType::Automation))) return false;
	FWorldFactoryLineState After;
	FWorldFactoryLineState FinishedAfter;
	if (!TestTrue(TEXT("Both line states remain readable after upgrade"),
		Fixture.Factory->GetLineState(ORT::kRuleFactoryCountry, ActiveId, After) &&
		Fixture.Factory->GetLineState(ORT::kRuleFactoryCountry, FinishedId, FinishedAfter))) return false;
	TestTrue(TEXT("Existing active line gains the DT speed bonus"),
		FMath::IsNearlyEqual(After.RatePerSecond,
			Before.RatePerSecond * (1.0f + AutomationDef.PerLevel), 0.0001f));
	TestEqual(TEXT("Upgrade does not retroactively add whole units"), After.CurrentQty, Before.CurrentQty);
	const double RawAfter = After.LineElapsedSec * static_cast<double>(After.RatePerSecond);
	TestTrue(TEXT("Upgrade preserves fractional progress"), FMath::IsNearlyEqual(RawAfter, RawBefore, 0.02));
	TestTrue(TEXT("Completed line stays completed"), FinishedAfter.bCompleted);
	TestEqual(TEXT("Completed line keeps its finished quantity"), FinishedAfter.CurrentQty, FinishedBefore.CurrentQty);

	Fixture.Factory->SerializeForSave(Saved);
	CountryData = Saved.Find(ORT::kRuleFactoryCountry);
	if (!TestTrue(TEXT("Upgraded active line is serialized"), CountryData && CountryData->ActiveLines.Num() == 2)) return false;
	CountryData->ActiveLines[0].StartWallUtc -= FTimespan::FromSeconds(10.0);
	Fixture.Factory->LoadFromSave(Saved);
	FWorldFactoryLineState Later;
	if (!TestTrue(TEXT("Aged upgraded line is readable"),
		Fixture.Factory->GetLineState(ORT::kRuleFactoryCountry, ActiveId, Later))) return false;
	const double RawLater = Later.LineElapsedSec * static_cast<double>(Later.RatePerSecond);
	TestTrue(TEXT("Following ten seconds use the new rate"),
		FMath::IsNearlyEqual(RawLater - RawAfter, 10.0 * static_cast<double>(After.RatePerSecond), 0.02));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverseasRuleAutomationPreservesFutureStartTest,
	"CGR.Overseas.Rule.AutomationPreservesFutureStart",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FOverseasRuleAutomationPreservesFutureStartTest::RunTest(const FString& Parameters)
{
	namespace ORT = OverseasRuleTests;
	if (!ORT::ShouldRun(*this)) return true;
	ORT::FRuleFixture Fixture;
	if (!TestTrue(TEXT("Rule fixture has the overseas managers"), Fixture.IsReady())) return false;
	const TSoftObjectPtr<UTexture2D> NoIcon;
	const int32 LineId = Fixture.Factory->StartProduction(ORT::kRuleFactoryCountry, ORT::kRuleIndustry,
		ORT::kRuleProductIndex, FText::FromString(TEXT("Future")), NoIcon, 0.2f, 100, ORT::kRuleGrade);
	if (!TestTrue(TEXT("Delayed line starts"), LineId > 0)) return false;
	TMap<ECountryType, FCountryFactoryData> Saved;
	Fixture.Factory->SerializeForSave(Saved);
	FCountryFactoryData* CountryData = Saved.Find(ORT::kRuleFactoryCountry);
	if (!TestTrue(TEXT("Delayed line is serialized"), CountryData && CountryData->ActiveLines.Num() == 1)) return false;
	const FDateTime FutureStart = FDateTime::UtcNow() + FTimespan::FromHours(1);
	CountryData->ActiveLines[0].StartWallUtc = FutureStart;
	Fixture.Factory->LoadFromSave(Saved);
	FWorldFactoryLineState Before;
	if (!TestTrue(TEXT("Delayed line is readable"),
		Fixture.Factory->GetLineState(ORT::kRuleFactoryCountry, LineId, Before))) return false;
	TestEqual(TEXT("Future start has no progress"), Before.CurrentQty, int64(0));
	if (!TestTrue(TEXT("Automation level 1 is bought"),
		Fixture.Factory->UpgradeLevelUp(ORT::kRuleFactoryCountry, EWorldFactoryUpgradeType::Automation))) return false;
	FWorldFactoryLineState After;
	if (!TestTrue(TEXT("Upgraded delayed line is readable"),
		Fixture.Factory->GetLineState(ORT::kRuleFactoryCountry, LineId, After))) return false;
	TestEqual(TEXT("Automation preserves the remaining offline delay"), After.StartWallUtc.GetTicks(), FutureStart.GetTicks());
	TestEqual(TEXT("Upgraded future start still has no progress"), After.CurrentQty, int64(0));
	TestTrue(TEXT("Delayed line gets the new rate"), After.RatePerSecond > Before.RatePerSecond);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverseasRuleInvalidIndustryClaimTest, "CGR.Overseas.Rule.InvalidIndustryClaimPreservesLine",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FOverseasRuleInvalidIndustryClaimTest::RunTest(const FString& Parameters)
{
	namespace ORT = OverseasRuleTests;
	if (!ORT::ShouldRun(*this)) return true;
	ORT::FRuleFixture Fixture;
	if (!TestTrue(TEXT("Rule fixture has the overseas managers"), Fixture.IsReady())) return false;
	const TSoftObjectPtr<UTexture2D> NoIcon;
	const int32 LineId = Fixture.Factory->StartProduction(ORT::kRuleFactoryCountry, ECompanyType::None,
		ORT::kRuleProductIndex, FText::FromString(TEXT("Invalid")), NoIcon, 1.0f, 1);
	if (!TestTrue(TEXT("Invalid-industry batch exists"), LineId > 0)) return false;
	if (!TestTrue(TEXT("Invalid-industry batch completes"), Fixture.Factory->InstantFinishLine(ORT::kRuleFactoryCountry, LineId))) return false;
	AddExpectedError(TEXT("ClaimLine: 라인"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("A batch with no warehouse key cannot be claimed"), Fixture.Factory->ClaimLine(ORT::kRuleFactoryCountry, LineId));
	FWorldFactoryLineState StillReady;
	if (!TestTrue(TEXT("Failed payout preserves the line"), Fixture.Factory->GetLineState(ORT::kRuleFactoryCountry, LineId, StillReady))) return false;
	TestTrue(TEXT("Failed payout preserves its completed batch"), StillReady.bCompleted);
	TestEqual(TEXT("Failed payout preserves its quantity"), StillReady.CurrentQty, int64(1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverseasRuleMineClaimKeepsLineTest, "CGR.Overseas.Rule.MineClaimKeepsLine",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FOverseasRuleMineClaimKeepsLineTest::RunTest(const FString& Parameters)
{
	namespace ORT = OverseasRuleTests;
	if (!ORT::ShouldRun(*this)) return true;
	ORT::FRuleFixture Fixture;
	if (!TestTrue(TEXT("Rule fixture has the overseas managers"), Fixture.IsReady())) return false;

	FMineLineState Full;
	if (!ORT::StartFullMineLine(*this, Fixture, Full)) return false;
	const int64 StockBefore = Fixture.Resources->GetResourceAmount(ORT::kRuleMineResource);

	if (!TestTrue(TEXT("Full mine line can be claimed"), Fixture.Mine->ClaimResource(ORT::kRuleMineCountry, ORT::kRuleMineResource))) return false;
	ORT::ExpectMineLineRestarted(*this, Fixture.Mine);
	TestEqual(TEXT("Claimed stock moves to the resource inventory"),
		Fixture.Resources->GetResourceAmount(ORT::kRuleMineResource) - StockBefore, Full.CurrentQty);
	TestFalse(TEXT("An empty mine line has nothing to claim"), Fixture.Mine->ClaimResource(ORT::kRuleMineCountry, ORT::kRuleMineResource));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverseasRuleMineAutoClaimKeepsLineTest, "CGR.Overseas.Rule.MineAutoClaimKeepsLine",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FOverseasRuleMineAutoClaimKeepsLineTest::RunTest(const FString& Parameters)
{
	namespace ORT = OverseasRuleTests;
	if (!ORT::ShouldRun(*this)) return true;
	ORT::FRuleFixture Fixture;
	if (!TestTrue(TEXT("Rule fixture has the overseas managers"), Fixture.IsReady())) return false;

	FMineLineState Full;
	if (!ORT::StartFullMineLine(*this, Fixture, Full)) return false;
	if (!TestTrue(TEXT("Auto claim upgrade is bought"), Fixture.Mine->UpgradeLevelUp(ORT::kRuleMineCountry, EMineUpgradeType::AutoClaim))) return false;
	if (!TestTrue(TEXT("Auto claim is enabled"), Fixture.Mine->IsAutoClaimEnabled(ORT::kRuleMineCountry))) return false;
	const int64 StockBefore = Fixture.Resources->GetResourceAmount(ORT::kRuleMineResource);

	Fixture.Mine->ProcessAutoClaims();
	ORT::ExpectMineLineRestarted(*this, Fixture.Mine);
	TestEqual(TEXT("Auto claim moves the full stock to the resource inventory"),
		Fixture.Resources->GetResourceAmount(ORT::kRuleMineResource) - StockBefore, Full.CurrentQty);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverseasRuleMineStorageAffectsInstalledLinesTest,
	"CGR.Overseas.Rule.MineStorageAffectsInstalledLines",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FOverseasRuleMineStorageAffectsInstalledLinesTest::RunTest(const FString& Parameters)
{
	namespace ORT = OverseasRuleTests;
	if (!ORT::ShouldRun(*this)) return true;
	ORT::FRuleFixture Fixture;
	if (!TestTrue(TEXT("Rule fixture has the overseas managers"), Fixture.IsReady())) return false;
	Fixture.Resources->StoreResource(EResourceType::Money, 1'000'000'000'000LL, /*bShouldSave=*/false);
	Fixture.Mine->EnsureCountryLines(ORT::kRuleMineCountry);
	if (!TestTrue(TEXT("A 10-capacity mine line is created"),
		Fixture.Mine->CreateMineLine(ORT::kRuleMineCountry, ORT::kRuleMineResource, 10))) return false;
	TestEqual(TEXT("Installed line begins at selected capacity"),
		Fixture.Mine->GetEffectiveMaxStorage(ORT::kRuleMineCountry, ORT::kRuleMineResource), int64(10));
	TMap<ECountryType, FCountryMineData> Saved;
	Fixture.Mine->SerializeForSave(Saved);
	FCountryMineData* CountryData = Saved.Find(ORT::kRuleMineCountry);
	if (!TestTrue(TEXT("Installed line can be aged"), CountryData && CountryData->ActiveLines.Num() == 1)) return false;
	CountryData->ActiveLines[0].StartWallUtc = FDateTime::UtcNow() - FTimespan::FromMinutes(30);
	Fixture.Mine->LoadFromSave(Saved);
	FMineLineState BeforeStorageUpgrade;
	if (!TestTrue(TEXT("Old capacity saturates after a long wait"),
		Fixture.Mine->GetLineState(ORT::kRuleMineCountry, ORT::kRuleMineResource, BeforeStorageUpgrade) &&
		BeforeStorageUpgrade.bSaturated && BeforeStorageUpgrade.CurrentQty == 10)) return false;
	if (!TestTrue(TEXT("Storage level 1 is bought"),
		Fixture.Mine->UpgradeLevelUp(ORT::kRuleMineCountry, EMineUpgradeType::MiningStorage))) return false;
	TestEqual(TEXT("Existing 10-capacity line gains 100 after level 1"),
		Fixture.Mine->GetEffectiveMaxStorage(ORT::kRuleMineCountry, ORT::kRuleMineResource), int64(110));
	FMineLineState AfterStorageUpgrade;
	if (!TestTrue(TEXT("Upgraded line remains readable"),
		Fixture.Mine->GetLineState(ORT::kRuleMineCountry, ORT::kRuleMineResource, AfterStorageUpgrade))) return false;
	TestEqual(TEXT("Storage upgrade does not retroactively mine past the old cap"), AfterStorageUpgrade.CurrentQty, int64(10));
	TestFalse(TEXT("Higher cap lets the old line resume mining"), AfterStorageUpgrade.bSaturated);

	Fixture.Mine->SerializeForSave(Saved);
	CountryData = Saved.Find(ORT::kRuleMineCountry);
	if (!TestTrue(TEXT("Installed line is serialized"), CountryData && CountryData->ActiveLines.Num() == 1)) return false;
	TestEqual(TEXT("Original line records storage level at installation"),
		CountryData->ActiveLines[0].StorageLevelAtCreation, 0);
	CountryData->ActiveLines[0].StartWallUtc -= FTimespan::FromMinutes(1);
	Fixture.Mine->LoadFromSave(Saved);
	FMineLineState ResumedLine;
	if (!TestTrue(TEXT("Resumed line is readable"),
		Fixture.Mine->GetLineState(ORT::kRuleMineCountry, ORT::kRuleMineResource, ResumedLine))) return false;
	TestTrue(TEXT("Line mines new units after storage upgrade"), ResumedLine.CurrentQty > 10);
	TestEqual(TEXT("Original line gain survives save/load"),
		Fixture.Mine->GetEffectiveMaxStorage(ORT::kRuleMineCountry, ORT::kRuleMineResource), int64(110));

	CountryData->ActiveLines.Reset();
	Fixture.Mine->LoadFromSave(Saved);
	if (!TestTrue(TEXT("A new line can be installed after level 1"),
		Fixture.Mine->CreateMineLine(ORT::kRuleMineCountry, ORT::kRuleMineResource, 10))) return false;
	TestEqual(TEXT("New line still begins at selected capacity"),
		Fixture.Mine->GetEffectiveMaxStorage(ORT::kRuleMineCountry, ORT::kRuleMineResource), int64(10));
	Fixture.Mine->SerializeForSave(Saved);
	CountryData = Saved.Find(ORT::kRuleMineCountry);
	if (!TestTrue(TEXT("New line is serialized"), CountryData && CountryData->ActiveLines.Num() == 1)) return false;
	TestEqual(TEXT("New line records current storage level at installation"),
		CountryData->ActiveLines[0].StorageLevelAtCreation, 1);
	CountryData->ActiveLines[0].StartWallUtc = FDateTime::UtcNow() - FTimespan::FromSeconds(40);
	Fixture.Mine->LoadFromSave(Saved);
	FMineLineState BeforeRateUpgrade;
	if (!TestTrue(TEXT("Partially mined line is readable"),
		Fixture.Mine->GetLineState(ORT::kRuleMineCountry, ORT::kRuleMineResource, BeforeRateUpgrade))) return false;
	if (!TestTrue(TEXT("Rate level 1 is bought"),
		Fixture.Mine->UpgradeLevelUp(ORT::kRuleMineCountry, EMineUpgradeType::MiningRate))) return false;
	FMineLineState AfterRateUpgrade;
	if (!TestTrue(TEXT("Rate-upgraded line is readable"),
		Fixture.Mine->GetLineState(ORT::kRuleMineCountry, ORT::kRuleMineResource, AfterRateUpgrade))) return false;
	TestEqual(TEXT("Rate upgrade does not retroactively increase mined units"),
		AfterRateUpgrade.CurrentQty, BeforeRateUpgrade.CurrentQty);
	TestTrue(TEXT("Rate upgrade preserves fractional progress"),
		FMath::IsNearlyEqual(AfterRateUpgrade.PendingFraction, BeforeRateUpgrade.PendingFraction, 0.01f));
	if (!TestTrue(TEXT("Storage level 2 is bought"),
		Fixture.Mine->UpgradeLevelUp(ORT::kRuleMineCountry, EMineUpgradeType::MiningStorage))) return false;
	TestEqual(TEXT("Later upgrade increases the new line by 100"),
		Fixture.Mine->GetEffectiveMaxStorage(ORT::kRuleMineCountry, ORT::kRuleMineResource), int64(110));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverseasRuleOrderStartTest, "CGR.Overseas.Rule.OrderStartBudgetAndSpeed",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FOverseasRuleOrderStartTest::RunTest(const FString& Parameters)
{
	namespace ORT = OverseasRuleTests;
	if (!ORT::ShouldRun(*this)) return true;
	ORT::FRuleFixture Fixture;
	if (!TestTrue(TEXT("Rule fixture has the overseas managers"), Fixture.IsReady())) return false;
	UProductionOrderManager* Orders = Fixture.Instance->GetSubsystem<UProductionOrderManager>();
	UTableManagerSubsystem* Tables = Fixture.Instance->GetSubsystem<UTableManagerSubsystem>();
	if (!TestTrue(TEXT("Order and table managers are available"), Orders && Tables)) return false;

	bool bRecipeFound = false;
	const FProductRecipeTable Recipe = Tables->GetProductRecipe(ORT::kRuleIndustry, ORT::kRuleProductIndex, bRecipeFound);
	if (!TestTrue(TEXT("Fixture has a timed raw-material recipe"), bRecipeFound && Recipe.ProductionTimeSec > 0.0f)) return false;
	TArray<TPair<EResourceType, int32>> Materials;
	Recipe.CollectMaterials(Materials);
	if (!TestTrue(TEXT("Fixture recipe needs raw materials"), !Materials.IsEmpty())) return false;
	for (const auto& Material : Materials)
	{
		Fixture.Resources->StoreResource(Material.Key,
			static_cast<int64>(Material.Value) * 2 - Fixture.Resources->GetResourceAmount(Material.Key), false);
	}
	Fixture.Resources->StoreResource(EResourceType::Money, 1'000'000, false);
	if (!TestTrue(TEXT("Production speed upgrade succeeds"),
		Fixture.Factory->UpgradeLevelUp(ORT::kRuleFactoryCountry, EWorldFactoryUpgradeType::Automation))) return false;
	const int32 OrderId = Orders->CreateTestOrder(ORT::kRuleIndustry, ORT::kRuleProductIndex, TEXT("Rule"), 10);
	FText Failure;
	TestTrue(TEXT("A quantity exceeding available materials cannot start"),
		OverseasActions::StartLineFromOrder(Fixture.Instance, ORT::kRuleFactoryCountry, OrderId, 3, Failure) <= 0);
	TestEqual(TEXT("Rejected start creates no line"), Fixture.Factory->GetActiveLineCount(ORT::kRuleFactoryCountry), 0);
	TestEqual(TEXT("Rejected start consumes no order quantity"), Orders->GetOrderByID(OrderId)->RemainingQuantity, 10);
	for (const auto& Material : Materials)
	{
		TestEqual(TEXT("Rejected start consumes no material"), Fixture.Resources->GetResourceAmount(Material.Key),
			static_cast<int64>(Material.Value) * 2);
	}

	const int32 LineId = OverseasActions::StartLineFromOrder(Fixture.Instance, ORT::kRuleFactoryCountry, OrderId, 2, Failure);
	FWorldFactoryLineState Started;
	if (!TestTrue(TEXT("Affordable quantity starts"), LineId > 0 &&
		Fixture.Factory->GetLineState(ORT::kRuleFactoryCountry, LineId, Started))) return false;
	const float Speed = Fixture.Factory->GetSpeedMultiplier(ORT::kRuleFactoryCountry);
	TestTrue(TEXT("The upgrade actually changes the base speed"), Speed > 1.0f);
	TestTrue(TEXT("Actual duration matches recipe duration with the upgrade applied once"),
		FMath::IsNearlyEqual(2.0f / Started.RatePerSecond, 2.0f * Recipe.ProductionTimeSec / Speed, 0.0001f));
	TestEqual(TEXT("Only the selected quantity is consumed"), Orders->GetOrderByID(OrderId)->RemainingQuantity, 8);
	for (const auto& Material : Materials)
	{
		TestEqual(TEXT("Selected quantity consumes its recipe materials"), Fixture.Resources->GetResourceAmount(Material.Key), int64(0));
	}
	const FIntPoint ProductKey(static_cast<int32>(ORT::kRuleIndustry), ORT::kRuleProductIndex);
	const int64 StockBeforeClaim = Fixture.WorldMap->GetProductAmount(ProductKey);
	if (!TestTrue(TEXT("Paid batch completes"), Fixture.Factory->InstantFinishLine(ORT::kRuleFactoryCountry, LineId))) return false;
	if (!TestTrue(TEXT("Paid batch can be claimed"), Fixture.Factory->ClaimLine(ORT::kRuleFactoryCountry, LineId))) return false;
	TestEqual(TEXT("Paid batch adds only its selected quantity"), Fixture.WorldMap->GetProductAmount(ProductKey) - StockBeforeClaim, int64(2));
	TestFalse(TEXT("Consumed order batch cannot be claimed twice"), Fixture.Factory->ClaimLine(ORT::kRuleFactoryCountry, LineId));
	TestEqual(TEXT("Claim does not consume another order quantity"), Orders->GetOrderByID(OrderId)->RemainingQuantity, 8);
	for (const auto& Material : Materials)
	{
		TestEqual(TEXT("Claim does not consume another batch of materials"), Fixture.Resources->GetResourceAmount(Material.Key), int64(0));
	}
	return true;
}

#endif
