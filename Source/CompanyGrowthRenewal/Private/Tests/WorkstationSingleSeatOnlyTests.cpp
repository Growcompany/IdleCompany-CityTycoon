#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/DataTable.h"
#include "Table/WorkstationCardTable.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWorkstationSingleSeatOnlyDataContractTest,
	"CGR.Workstation.SingleSeatOnlyDataContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorkstationSingleSeatOnlyDataContractTest::RunTest(const FString& Parameters)
{
	UDataTable* WorkstationTable = LoadObject<UDataTable>(
		nullptr,
		TEXT("/Game/CompanyGrowth/Table/Office/DT_WorkstationCard.DT_WorkstationCard"));
	if (!TestNotNull(TEXT("DT_WorkstationCard exists"), WorkstationTable))
	{
		return false;
	}

	const TArray<FName> RowNames = WorkstationTable->GetRowNames();
	TestTrue(TEXT("DT_WorkstationCard contains runtime-purchasable workstation rows"), RowNames.Num() > 0);

	const TSet<FName> ExpectedLiveRows = {
		FName(TEXT("WS_A")),
		FName(TEXT("WS_B")),
		FName(TEXT("WS_D")),
		FName(TEXT("WS_E")),
		FName(TEXT("WS_F")),
		FName(TEXT("WS_G")),
		FName(TEXT("WS_I")),
		FName(TEXT("WS_J")),
		FName(TEXT("WS_L")),
		FName(TEXT("WS_N")),
		FName(TEXT("WS_Q")),
		FName(TEXT("WS_S")),
		FName(TEXT("WS_T")),
		FName(TEXT("WS_U")),
		FName(TEXT("WS_W")),
		FName(TEXT("WS_X"))
	};
	TSet<FName> ActualRows;
	ActualRows.Reserve(RowNames.Num());
	for (const FName& RowName : RowNames)
	{
		ActualRows.Add(RowName);
	}

	const TArray<FName> RemovedDoubleRows = {
		FName(TEXT("WD_A")),
		FName(TEXT("WD_B")),
		FName(TEXT("WD_C")),
		FName(TEXT("WD_D")),
		FName(TEXT("WD_E")),
		FName(TEXT("WD_F")),
		FName(TEXT("WD_G"))
	};
	for (const FName& RemovedRow : RemovedDoubleRows)
	{
		TestFalse(
			*FString::Printf(TEXT("Removed Double row %s is absent"), *RemovedRow.ToString()),
			ActualRows.Contains(RemovedRow));
	}

	TestEqual(TEXT("DT_WorkstationCard contains exactly 16 live Single rows"), ActualRows.Num(), ExpectedLiveRows.Num());
	for (const FName& ExpectedRow : ExpectedLiveRows)
	{
		TestTrue(
			*FString::Printf(TEXT("Expected live Single row %s exists"), *ExpectedRow.ToString()),
			ActualRows.Contains(ExpectedRow));
	}
	for (const FName& ActualRow : ActualRows)
	{
		TestTrue(
			*FString::Printf(TEXT("Unexpected workstation row %s is absent"), *ActualRow.ToString()),
			ExpectedLiveRows.Contains(ActualRow));
	}

	for (const FName& RowName : RowNames)
	{
		const FWorkstationCardTable* Row = WorkstationTable->FindRow<FWorkstationCardTable>(
			RowName, TEXT("Single-seat-only workstation data contract"));
		if (!TestNotNull(*FString::Printf(TEXT("%s exists"), *RowName.ToString()), Row))
		{
			continue;
		}

		TestTrue(
			*FString::Printf(TEXT("%s is a Single workstation"), *RowName.ToString()),
			Row->WorkstationType == EWorkstationType::Single);

		const FString BlueprintPath = Row->BlueprintClass.ToSoftObjectPath().ToString();
		TestFalse(
			*FString::Printf(TEXT("%s BlueprintClass is configured"), *RowName.ToString()),
			Row->BlueprintClass.IsNull());
		TestFalse(
			*FString::Printf(TEXT("%s BlueprintClass path is not empty"), *RowName.ToString()),
			BlueprintPath.IsEmpty());
		TestTrue(
			*FString::Printf(TEXT("%s BlueprintClass uses a /Single/ asset path"), *RowName.ToString()),
			BlueprintPath.Contains(TEXT("/Single/"), ESearchCase::IgnoreCase));
		TestFalse(
			*FString::Printf(TEXT("%s BlueprintClass does not use a /Double/ asset path"), *RowName.ToString()),
			BlueprintPath.Contains(TEXT("/Double/"), ESearchCase::IgnoreCase));
		TestFalse(
			*FString::Printf(TEXT("%s BlueprintClass does not reference BP_WorkStation_Double"), *RowName.ToString()),
			BlueprintPath.Contains(TEXT("BP_WorkStation_Double"), ESearchCase::IgnoreCase));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
