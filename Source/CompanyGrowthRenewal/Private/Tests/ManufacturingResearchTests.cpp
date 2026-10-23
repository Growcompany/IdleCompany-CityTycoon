#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Data/ManufacturingResearchRules.h"
#include "Data/GameSaveData.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FManufacturingResearchWorkTest,
    "CGR.Manufacturing.Research.WorkAndCapability", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FManufacturingResearchWorkTest::RunTest(const FString& Parameters)
{
    using namespace ManufacturingResearchRules;
    FManufacturingResearchState Research;
    Research.ProjectIndex = 1;
    const TArray<int32> Required = {5, 4, 3};
    TestFalse(TEXT("Surplus in other fields cannot cover a capability shortage"),
        Advance(Research, {100, 100, 2}, Required, 10., 1000., 2));
    TestEqual(TEXT("Unqualified team does not earn progress"), Research.CompletedWorkSeconds, 0.);
    TestFalse(TEXT("No actual work cannot earn progress"), Advance(Research, Required, Required, 10., 0., 2));
    TestFalse(TEXT("One working member of two earns half time"), Advance(Research, Required, Required, 10., 10., 2));
    TestEqual(TEXT("Partial rest slows research"), Research.CompletedWorkSeconds, 5.);
    TestFalse(TEXT("Removing required staff pauses partial research"), Advance(Research, {0, 0, 0}, Required, 10., 20., 2));
    TestEqual(TEXT("Paused research retains work already earned"), Research.CompletedWorkSeconds, 5.);
    TestTrue(TEXT("Qualified team can resume and finish"), Advance(Research, Required, Required, 10., 10., 2));
    TestEqual(TEXT("Progress caps at the authored duration"), Research.CompletedWorkSeconds, 10.);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FManufacturingResearchSaveTest,
    "CGR.Manufacturing.Research.RestartAndUnlock", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FManufacturingResearchSaveTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<USaveGame_GameData> Save(NewObject<USaveGame_GameData>());
    auto& Office = Save->GameData.OfficeDataMap.FindOrAdd(7);
    Office.ManufacturingResearch.ProjectIndex = 1;
    Office.ManufacturingResearch.CompletedWorkSeconds = 4.75;
    TArray<uint8> Bytes;
    if (!TestTrue(TEXT("Save partial research"), UGameplayStatics::SaveGameToMemory(Save.Get(), Bytes))) return false;
    TStrongObjectPtr<USaveGame_GameData> Loaded(Cast<USaveGame_GameData>(UGameplayStatics::LoadGameFromMemory(Bytes)));
    if (!TestNotNull(TEXT("Reload partial research"), Loaded.Get())) return false;
    auto& Restored = Loaded->GameData.OfficeDataMap.FindChecked(7);
    TestEqual(TEXT("Active product survives restart"), Restored.ManufacturingResearch.ProjectIndex, 1);
    TestEqual(TEXT("Actual work survives restart"), Restored.ManufacturingResearch.CompletedWorkSeconds, 4.75);
    TestTrue(TEXT("Only the remaining work is needed after restart"), ManufacturingResearchRules::Advance(
        Restored.ManufacturingResearch, {5, 4, 3}, {5, 4, 3}, 10., 5.25, 1));
    Restored.TierProgress.RecordProjectClear(1);
    Restored.TierProgress.RecordProjectClear(1);
    TestEqual(TEXT("Same product cannot count twice toward tier unlock"), Restored.TierProgress.ClearedProjects.Num(), 1);
    Restored.PendingManufacturingResearchResults.AddUnique(1);
    TArray<uint8> CompletedBytes;
    if (!TestTrue(TEXT("Save completed research result"), UGameplayStatics::SaveGameToMemory(Loaded.Get(), CompletedBytes))) return false;
    TStrongObjectPtr<USaveGame_GameData> Completed(Cast<USaveGame_GameData>(UGameplayStatics::LoadGameFromMemory(CompletedBytes)));
    if (!TestNotNull(TEXT("Reload completed research result"), Completed.Get())) return false;
    auto& CompletedOffice = Completed->GameData.OfficeDataMap.FindChecked(7);
    TestEqual(TEXT("Pending result survives restart"), CompletedOffice.PendingManufacturingResearchResults.Num(), 1);
    TestEqual(TEXT("Pending result identifies the product"), CompletedOffice.PendingManufacturingResearchResults[0], 1);
    CompletedOffice.PendingManufacturingResearchResults.RemoveSingle(1);
    TestTrue(TEXT("Stored pending result can be removed"), CompletedOffice.PendingManufacturingResearchResults.IsEmpty());
    const int32 RequiredClears = FTierLayout::ClearToUnlock(1);
    for (int32 Project = 2; Project <= RequiredClears; ++Project) Restored.TierProgress.RecordProjectClear(Project);
    TestEqual(TEXT("Distinct completed products unlock the next tier"), Restored.TierProgress.CurrentTier, 2);
    return true;
}
#endif
