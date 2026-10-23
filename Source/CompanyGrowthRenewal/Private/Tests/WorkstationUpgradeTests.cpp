#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Office/WorkstationTypes.h"
#include "Table/WorkstationTable.h"
#include "Office/WorkstationUpgradeRules.h"
#include "Entity/Officeworker/EmployeeBehaviorComponent.h"
#include "Engine/DataTable.h"
#include "Office/WorkstationActorBase.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWorkstationLinearLevelTest,
	"CGR.Workstation.LinearLevelSuccessor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorkstationLinearLevelTest::RunTest(const FString& Parameters)
{
	struct FTransitionCase
	{
		EComputerSetupLevel Current;
		EComputerSetupLevel ExpectedNext;
	};

	const FTransitionCase Cases[] =
	{
		{ EComputerSetupLevel::Level1, EComputerSetupLevel::Level2 },
		{ EComputerSetupLevel::Level2, EComputerSetupLevel::Level3 },
		{ EComputerSetupLevel::Level3, EComputerSetupLevel::Level4 },
		{ EComputerSetupLevel::Level4, EComputerSetupLevel::Level5 },
		{ EComputerSetupLevel::Level5, EComputerSetupLevel::Level6 },
	};

	for (const FTransitionCase& TestCase : Cases)
	{
		EComputerSetupLevel ActualNext = EComputerSetupLevel::Level1;
		TestTrue(TEXT("Each non-max level has a successor"),
			TryGetNextComputerSetupLevel(TestCase.Current, ActualNext));
		TestEqual(TEXT("Successor advances exactly one level"), ActualNext, TestCase.ExpectedNext);
	}

	EComputerSetupLevel Unchanged = EComputerSetupLevel::Level6;
	TestFalse(TEXT("Level 6 has no successor"),
		TryGetNextComputerSetupLevel(EComputerSetupLevel::Level6, Unchanged));
	TestEqual(TEXT("Failed successor lookup does not invent another level"),
		Unchanged, EComputerSetupLevel::Level6);

	// 외형 변형은 승급 경로 밖이다 — enum 순번상 Level6 다음이라 자동 승급되면 안 된다
	EComputerSetupLevel FromTwin = EComputerSetupLevel::Level6Twin;
	TestFalse(TEXT("The twin variant is outside the upgrade path"),
		TryGetNextComputerSetupLevel(EComputerSetupLevel::Level6Twin, FromTwin));
	TestEqual(TEXT("Both max variants report the same display number"),
		GetComputerSetupLevelNumber(EComputerSetupLevel::Level6Twin),
		GetComputerSetupLevelNumber(EComputerSetupLevel::Level6));
	TestTrue(TEXT("Both max variants count as max"),
		IsMaxComputerSetupLevel(EComputerSetupLevel::Level6)
			&& IsMaxComputerSetupLevel(EComputerSetupLevel::Level6Twin));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWorkstationSpeedCompositionTest,
	"CGR.Workstation.SpeedComposition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorkstationSpeedCompositionTest::RunTest(const FString& Parameters)
{
	// 단정밀도 곱셈 반올림 오차가 IsNearlyEqual 기본값(UE_SMALL_NUMBER=1e-8)보다 크다
	TestTrue(TEXT("Zero workstation rate preserves employee speed"),
		FMath::IsNearlyEqual(
			UEmployeeBehaviorComponent::ComposeWorkstationSpeedFactor(1.4f, 0.0f), 1.4f,
			UE_KINDA_SMALL_NUMBER));
	TestTrue(TEXT("Five percent workstation rate multiplies speed once"),
		FMath::IsNearlyEqual(
			UEmployeeBehaviorComponent::ComposeWorkstationSpeedFactor(1.4f, 0.05f), 1.47f,
			UE_KINDA_SMALL_NUMBER));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWorkstationSetupRowValidationTest,
	"CGR.Workstation.SetupRowValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorkstationSetupRowValidationTest::RunTest(const FString& Parameters)
{
	FComputerSetupLevelData ValidRow;
	ValidRow.Level = EComputerSetupLevel::Level1;
	ValidRow.DisplayName = FText::FromString(TEXT("Level 1"));
	ValidRow.WorkSpeedBonusRate = 0.0f;
	ValidRow.LaptopPlacement = ELaptopPlacement::Center;
	ValidRow.bMouseActive = true;
	ValidRow.LaptopMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/Office/Meshes/SM_Laptop_White.SM_Laptop_White")));
	ValidRow.MouseMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/Office/Meshes/SM_Mouse_Black.SM_Mouse_Black")));

	TestTrue(TEXT("A complete active-equipment row is valid"), ValidRow.IsValidConfiguration());

	FComputerSetupLevelData NegativeRateRow = ValidRow;
	NegativeRateRow.WorkSpeedBonusRate = -0.01f;
	TestFalse(TEXT("Negative work-speed rate is rejected"), NegativeRateRow.IsValidConfiguration());

	FComputerSetupLevelData MissingActiveMeshRow = ValidRow;
	MissingActiveMeshRow.MouseMesh.Reset();
	TestFalse(TEXT("An active item without a mesh path is rejected"), MissingActiveMeshRow.IsValidConfiguration());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWorkstationSetupTableContractTest,
	"CGR.Workstation.SetupTableContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorkstationSetupTableContractTest::RunTest(const FString& Parameters)
{
	UDataTable* SetupTable = LoadObject<UDataTable>(
		nullptr,
		TEXT("/Game/CompanyGrowth/Table/Office/DT_WorkstationSetupLevel.DT_WorkstationSetupLevel"));
	if (!TestNotNull(TEXT("The constructor-loaded setup DataTable exists"), SetupTable))
	{
		return false;
	}

	const float ExpectedRates[] = { 0.00f, 0.05f, 0.10f, 0.15f, 0.20f, 0.25f };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(ExpectedRates); ++Index)
	{
		const FName RowName(*FString::Printf(TEXT("Level%d"), Index + 1));
		const FComputerSetupLevelData* Row =
			SetupTable->FindRow<FComputerSetupLevelData>(RowName, TEXT("Workstation automation"));
		if (!TestNotNull(*FString::Printf(TEXT("%s exists"), *RowName.ToString()), Row))
		{
			continue;
		}

		TestEqual(*FString::Printf(TEXT("%s has its matching level"), *RowName.ToString()),
			static_cast<int32>(Row->Level), Index);
		TestTrue(*FString::Printf(TEXT("%s has the configured cumulative speed rate"), *RowName.ToString()),
			FMath::IsNearlyEqual(Row->WorkSpeedBonusRate, ExpectedRates[Index]));
		TestTrue(*FString::Printf(TEXT("%s is a complete equipment configuration"), *RowName.ToString()),
			Row->IsValidConfiguration());
	}

	// TableManagerSubsystem 캐시는 행 이름 == enum 이름일 때만 행을 받는다.
	// 어긋나면 조용히 버려져 그 레벨이 통째로 사라진다 (Level6Twin 이 "Level7" 로 역산돼 탈락했던 사고).
	if (const UEnum* LevelEnum = StaticEnum<EComputerSetupLevel>())
	{
		for (const FName& RowName : SetupTable->GetRowNames())
		{
			const FComputerSetupLevelData* Row =
				SetupTable->FindRow<FComputerSetupLevelData>(RowName, TEXT("Workstation row naming"));
			if (!Row)
			{
				continue;
			}

			TestEqual(*FString::Printf(TEXT("%s is named after its enum value"), *RowName.ToString()),
				RowName.ToString(),
				LevelEnum->GetNameStringByValue(static_cast<int64>(Row->Level)));
		}

		TestEqual(TEXT("Every declared setup level has a row"),
			SetupTable->GetRowNames().Num(), static_cast<int32>(EComputerSetupLevel::Max));
	}

	// 최대 레벨 외형 변형 — 승급 경로 밖이지만 성능은 Lv6 과 같아야 한다. 다르면 기각된 성능 분기가 된다.
	const FComputerSetupLevelData* TwinRow =
		SetupTable->FindRow<FComputerSetupLevelData>(TEXT("Level6Twin"), TEXT("Workstation automation"));
	const FComputerSetupLevelData* MaxRow =
		SetupTable->FindRow<FComputerSetupLevelData>(TEXT("Level6"), TEXT("Workstation automation"));
	if (TestNotNull(TEXT("Level6Twin exists"), TwinRow) && TestNotNull(TEXT("Level6 exists"), MaxRow))
	{
		TestEqual(TEXT("The twin row carries the twin enum value"),
			TwinRow->Level, EComputerSetupLevel::Level6Twin);
		TestTrue(TEXT("Both max variants share one work-speed rate"),
			FMath::IsNearlyEqual(TwinRow->WorkSpeedBonusRate, MaxRow->WorkSpeedBonusRate, UE_KINDA_SMALL_NUMBER));
		TestEqual(TEXT("The twin variant uses the curved twin layout"),
			TwinRow->SecondMonitorType, ESecondMonitorType::Curved);
		TestTrue(TEXT("The twin variant is a complete equipment configuration"),
			TwinRow->IsValidConfiguration());
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWorkstationSocketRuleTest,
	"CGR.Workstation.SetupSocketRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorkstationSocketRuleTest::RunTest(const FString& Parameters)
{
	// 소켓은 구 설계가 저작한 조합만 전제한다. 스키마가 표현 가능한 조합 중 실제로 설 수 있는 것만 통과시킨다.
	UDataTable* SetupTable = LoadObject<UDataTable>(
		nullptr,
		TEXT("/Game/CompanyGrowth/Table/Office/DT_WorkstationSetupLevel.DT_WorkstationSetupLevel"));
	if (!TestNotNull(TEXT("The constructor-loaded setup DataTable exists"), SetupTable))
	{
		return false;
	}

	for (const FName& RowName : SetupTable->GetRowNames())
	{
		const FComputerSetupLevelData* Row =
			SetupTable->FindRow<FComputerSetupLevelData>(RowName, TEXT("Workstation socket rules"));
		if (!Row)
		{
			continue;
		}

		const FString Label = RowName.ToString();

		if (Row->LaptopPlacement == ELaptopPlacement::Center)
		{
			TestFalse(*FString::Printf(TEXT("%s: a centre laptop leaves no room for a keyboard"), *Label),
				Row->bKeyboardActive);
			TestEqual(*FString::Printf(TEXT("%s: a centre laptop leaves no room for a monitor"), *Label),
				Row->PrimaryMonitorType, EPrimaryMonitorType::None);
		}

		if (Row->bKeyboardActive)
		{
			TestNotEqual(*FString::Printf(TEXT("%s: a keyboard needs the centre free of the laptop"), *Label),
				Row->LaptopPlacement, ELaptopPlacement::Center);
		}

		if (Row->SecondMonitorType != ESecondMonitorType::None)
		{
			TestEqual(*FString::Printf(TEXT("%s: a second monitor claims the side slot the laptop would use"), *Label),
				Row->LaptopPlacement, ELaptopPlacement::None);
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWorkstationComponentRulesTest,
	"CGR.Workstation.ComponentRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorkstationComponentRulesTest::RunTest(const FString& Parameters)
{
	FWorkstationUpgradeDefinition Definition;
	Definition.Type = EWorkstationUpgradeType::Processing;
	Definition.DisplayName = FText::FromString(TEXT("Fixture"));
	Definition.Description = FText::FromString(TEXT("Fixture effect"));
	Definition.MaxLevel = 4;
	Definition.BaseCost = 7;
	Definition.CostGrowthRate = 2.0;
	Definition.GradeCostMultiplier = 3.0;
	Definition.EffectPerLevel = 0.01f;
	TestTrue(TEXT("A complete component definition is valid"), Definition.IsValidConfiguration());
	FWorkstationUpgradeDefinition BadDefinition = Definition;
	BadDefinition.BaseCost = -1;
	TestFalse(TEXT("Negative component cost is rejected"), BadDefinition.IsValidConfiguration());
	BadDefinition = Definition;
	BadDefinition.EffectPerLevel = -0.01f;
	TestFalse(TEXT("Negative component effect is rejected"), BadDefinition.IsValidConfiguration());
	BadDefinition = Definition;
	BadDefinition.MaxLevel = 1;
	TestFalse(TEXT("A component cap must allow at least one purchase"), BadDefinition.IsValidConfiguration());
	BadDefinition.MaxLevel = MAX_int32;
	TestFalse(TEXT("An unbounded component quote is rejected"), BadDefinition.IsValidConfiguration());

	const float BeforePromotion = WorkstationUpgradeRules::GetCumulativeBonus(
		Definition, EComputerSetupLevel::Level1, Definition.MaxLevel);
	const float AfterPromotion = WorkstationUpgradeRules::GetCumulativeBonus(
		Definition, EComputerSetupLevel::Level2, 1);
	TestTrue(TEXT("Promoting and resetting the visible level keeps completed bonuses"),
		FMath::IsNearlyEqual(BeforePromotion, AfterPromotion));
	TestTrue(TEXT("A new grade's first component upgrade increases the cumulative bonus"),
		WorkstationUpgradeRules::GetCumulativeBonus(Definition, EComputerSetupLevel::Level2, 2) > AfterPromotion);
	TestTrue(TEXT("The final twin variant preserves the same component effect"),
		FMath::IsNearlyEqual(
			WorkstationUpgradeRules::GetCumulativeBonus(Definition, EComputerSetupLevel::Level6, 3),
			WorkstationUpgradeRules::GetCumulativeBonus(Definition, EComputerSetupLevel::Level6Twin, 3)));

	FWorkstationUpgradeQuote Quote;
	TestTrue(TEXT("Exact quote is computable with insufficient funds"),
		WorkstationUpgradeRules::BuildQuote(Definition, EComputerSetupLevel::Level1, 1, 2, false, 10, Quote));
	TestEqual(TEXT("Exact quote retains full requested count"), Quote.Count, 2);
	TestEqual(TEXT("Exact quote retains full cost for shortage display"), Quote.TotalCost, 21LL);
	TestTrue(TEXT("Exact quote exposes shortage"), Quote.TotalCost > Quote.Available);

	TestTrue(TEXT("Affordable max quote is computable"),
		WorkstationUpgradeRules::BuildQuote(Definition, EComputerSetupLevel::Level1, 1, 0, true, 10, Quote));
	TestEqual(TEXT("Affordable max stops before the unaffordable level"), Quote.Count, 1);
	TestEqual(TEXT("Affordable max sums only bought levels"), Quote.TotalCost, 7LL);
	TestTrue(TEXT("Zero affordable levels still quotes the next price"),
		WorkstationUpgradeRules::BuildQuote(Definition, EComputerSetupLevel::Level1, 1, 0, true, 0, Quote));
	TestEqual(TEXT("Zero affordable levels buys nothing"), Quote.Count, 0);
	TestEqual(TEXT("Zero affordable quote shows next level cost"), Quote.TotalCost, 7LL);

	TestTrue(TEXT("A request is bounded by the current grade"),
		WorkstationUpgradeRules::BuildQuote(Definition, EComputerSetupLevel::Level1, 2, 100, false, 1000, Quote));
	TestEqual(TEXT("The quote stops at the component cap"), Quote.Count, 2);
	TestTrue(TEXT("The component cap reports maxed"),
		WorkstationUpgradeRules::BuildQuote(Definition, EComputerSetupLevel::Level6, 4, 1, false, 1000, Quote)
		&& Quote.bMaxed && Quote.Count == 0);

	FWorkstationUpgradeDefinition Overflow = Definition;
	Overflow.BaseCost = MAX_int64;
	TestFalse(TEXT("Overflowing level cost is rejected"),
		WorkstationUpgradeRules::BuildQuote(Overflow, EComputerSetupLevel::Level1, 1, 1, false, MAX_int64, Quote));

	FWorkstationUpgradeDefinition LongProgression = Definition;
	LongProgression.MaxLevel = 500;
	LongProgression.CostGrowthRate = 1.0;
	TestTrue(TEXT("Data-defined caps beyond one hundred remain purchasable"),
		WorkstationUpgradeRules::BuildQuote(LongProgression, EComputerSetupLevel::Level1, 100, 50, false, 1000, Quote));
	TestEqual(TEXT("Bulk purchase crosses the old cap"), Quote.Count, 50);
	TestEqual(TEXT("Crossing the old cap charges each purchased level"), Quote.TotalCost, 350LL);
	TestTrue(TEXT("Max purchase stops at the configured cap"),
		WorkstationUpgradeRules::BuildQuote(LongProgression, EComputerSetupLevel::Level1, 490, 0, true, 1000, Quote));
	TestEqual(TEXT("Max purchase buys the ten remaining levels"), Quote.Count, 10);
	TestTrue(TEXT("A high-cap promotion retains cumulative bonuses"),
		FMath::IsNearlyEqual(
			WorkstationUpgradeRules::GetCumulativeBonus(LongProgression, EComputerSetupLevel::Level1, 500),
			WorkstationUpgradeRules::GetCumulativeBonus(LongProgression, EComputerSetupLevel::Level2, 1)));

	FWorkstationUpgradeDefinition Compound = Definition;
	Compound.Type = EWorkstationUpgradeType::Tools;
	Compound.EffectCurve = EWorkstationEffectCurve::Compound;
	Compound.EffectPerLevel = 0.1f;
	TestTrue(TEXT("Compound effect is valid"), Compound.IsValidConfiguration());
	const float ThreePurchaseBonus = WorkstationUpgradeRules::GetCumulativeBonus(
		Compound, EComputerSetupLevel::Level1, 4);
	TestTrue(TEXT("Three tool purchases multiply score by 1.331 rather than 1.3"),
		FMath::IsNearlyEqual(1.0f + ThreePurchaseBonus, 1.331f, 0.0001f));
	TestTrue(TEXT("Compound bonus continues unchanged across grade promotion"),
		FMath::IsNearlyEqual(ThreePurchaseBonus,
			WorkstationUpgradeRules::GetCumulativeBonus(Compound, EComputerSetupLevel::Level2, 1)));
	TestTrue(TEXT("Compound bonus is unchanged by the twin appearance"),
		FMath::IsNearlyEqual(
			WorkstationUpgradeRules::GetCumulativeBonus(Compound, EComputerSetupLevel::Level6, 3),
			WorkstationUpgradeRules::GetCumulativeBonus(Compound, EComputerSetupLevel::Level6Twin, 3)));
	TestTrue(TEXT("A bulk compound quote is available"),
		WorkstationUpgradeRules::BuildQuote(Compound, EComputerSetupLevel::Level1, 1, 3, false, 1000, Quote));
	TestEqual(TEXT("Bulk quote buys three levels"), Quote.Count, 3);
	TestTrue(TEXT("Bulk quote ends at the same score multiplier as three single purchases"),
		FMath::IsNearlyEqual(1.0f + Quote.NextBonus, 1.331f, 0.0001f));
	const FWorkstationUpgradeQuote BulkQuote = Quote;
	int64 SinglePurchaseTotal = 0;
	for (int32 CurrentLevel = 1; CurrentLevel < 4; ++CurrentLevel)
	{
		FWorkstationUpgradeQuote SingleQuote;
		TestTrue(TEXT("Single compound quote is available"),
			WorkstationUpgradeRules::BuildQuote(Compound, EComputerSetupLevel::Level1,
				CurrentLevel, 1, false, 1000, SingleQuote));
		SinglePurchaseTotal += SingleQuote.TotalCost;
		if (CurrentLevel == 3)
		{
			TestTrue(TEXT("The final single purchase gives the same effect as bulk"),
				FMath::IsNearlyEqual(SingleQuote.NextBonus, BulkQuote.NextBonus));
		}
	}
	TestEqual(TEXT("Bulk and single purchases cost the same total"), BulkQuote.TotalCost, SinglePurchaseTotal);

	FWorkstationUpgradeDefinition EffectOverflow = Compound;
	EffectOverflow.MaxLevel = 500;
	EffectOverflow.EffectPerLevel = 1.0f;
	TestFalse(TEXT("A finite per-level compound rate is rejected if the six-grade bonus overflows float"),
		EffectOverflow.IsValidConfiguration());
	TestFalse(TEXT("An overflowing effect cannot produce a quote"),
		WorkstationUpgradeRules::BuildQuote(EffectOverflow, EComputerSetupLevel::Level1, 1, 1, false, 1000, Quote));

	return true;
}

// 화면 검수용: 이 월드의 첫 책상 하나의 강화 항목을 실제 구매 경로(자금 차감)로 최대까지 올려 「책상 강화 준비」 상태를 만든다.
// -game 파이썬은 UENUM 인자를 못 넘기므로 콘솔 명령으로 둔다.
static FAutoConsoleCommandWithWorld MaxFirstDeskComponents(
	TEXT("cg.Workstation.MaxFirstDeskComponents"),
	TEXT("Buy every upgrade component of the first desk in this world up to its max level (QA capture helper)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (!World) return;
		for (TActorIterator<AWorkstationActorBase> It(World); It; ++It)
		{
			for (uint8 Index = 0; Index < static_cast<uint8>(EWorkstationUpgradeType::Count); ++Index)
			{
				const EWorkstationUpgradeType Type = static_cast<EWorkstationUpgradeType>(Index);
				const bool bBought = It->TryUpgradeComponent(Type, MAX_int32, true);
				UE_LOG(LogTemp, Display, TEXT("[MaxFirstDeskComponents] %s type %d bought=%d level=%d/%d"), *It->GetName(),
					Index, bBought ? 1 : 0, It->GetUpgradeLevel(Type), It->GetUpgradeMaxLevel(Type));
			}
			return;
		}
	}));

#endif
