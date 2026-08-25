#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Office/WorkstationTypes.h"
#include "Table/WorkstationTable.h"
#include "Entity/Officeworker/EmployeeBehaviorComponent.h"
#include "Engine/DataTable.h"

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
	ValidRow.UpgradeCost = 0;
	ValidRow.WorkSpeedBonusRate = 0.0f;
	ValidRow.LaptopPlacement = ELaptopPlacement::Center;
	ValidRow.bMouseActive = true;
	ValidRow.LaptopMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/Office/Meshes/SM_Laptop_White.SM_Laptop_White")));
	ValidRow.MouseMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/Office/Meshes/SM_Mouse_Black.SM_Mouse_Black")));

	TestTrue(TEXT("A complete active-equipment row is valid"), ValidRow.IsValidConfiguration());

	FComputerSetupLevelData NegativeCostRow = ValidRow;
	NegativeCostRow.UpgradeCost = -1;
	TestFalse(TEXT("Negative Diamond cost is rejected"), NegativeCostRow.IsValidConfiguration());

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

	const int32 ExpectedCosts[] = { 0, 30, 60, 120, 240, 480 };
	const float ExpectedRates[] = { 0.00f, 0.05f, 0.10f, 0.15f, 0.20f, 0.25f };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(ExpectedCosts); ++Index)
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
		TestEqual(*FString::Printf(TEXT("%s has the configured Diamond entry cost"), *RowName.ToString()),
			Row->UpgradeCost, ExpectedCosts[Index]);
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
		TestEqual(TEXT("Both max variants share one swap cost"),
			TwinRow->VariantSwapCost, MaxRow->VariantSwapCost);
		TestTrue(TEXT("The swap cost is configured"), TwinRow->VariantSwapCost > 0);
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
			TestFalse(*FString::Printf(TEXT("%s: a centre laptop leaves no room for a mouse"), *Label),
				Row->bMouseActive);
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

#endif
