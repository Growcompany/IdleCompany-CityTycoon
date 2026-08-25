#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/DataTable.h"
#include "UObject/UnrealType.h"
#include "Table/WorkstationCardTable.h"

#include <type_traits>

template <typename T, typename = void>
struct THasWorkstationPlacementCosts : std::false_type
{
};

template <typename T>
struct THasWorkstationPlacementCosts<T, std::void_t<decltype(&T::ConstructionCosts)>> : std::true_type
{
};

static_assert(
	!THasWorkstationPlacementCosts<FWorkstationCardTable>::value,
	"Workstation placement costs must not be part of the workstation card schema");

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWorkstationPlacementCostContractTest,
	"CGR.Workstation.PlacementCostContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorkstationPlacementCostContractTest::RunTest(const FString& Parameters)
{
	TestNull(
		TEXT("FWorkstationCardTable has no placement-cost property"),
		FWorkstationCardTable::StaticStruct()->FindPropertyByName(TEXT("ConstructionCosts")));

	UDataTable* WorkstationTable = LoadObject<UDataTable>(
		nullptr,
		TEXT("/Game/CompanyGrowth/Table/Office/DT_WorkstationCard.DT_WorkstationCard"));
	if (!TestNotNull(TEXT("DT_WorkstationCard exists"), WorkstationTable))
	{
		return false;
	}

	const TArray<FName> RowNames = WorkstationTable->GetRowNames();
	TestTrue(TEXT("DT_WorkstationCard contains registered workstation rows"), RowNames.Num() > 0);

	for (const FName& RowName : RowNames)
	{
		const FWorkstationCardTable* Row = WorkstationTable->FindRow<FWorkstationCardTable>(
			RowName, TEXT("Workstation placement cost contract"));
		if (!TestNotNull(*FString::Printf(TEXT("%s exists"), *RowName.ToString()), Row))
		{
			continue;
		}

	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
