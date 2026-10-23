#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Utils/EquipmentTicketShopRules.h"
#include "Utils/EmployeeEquipmentTransactionRules.h"
#include "Data/GameSaveData.h"
#include "Manager/ShopManagerSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"

namespace
{
    FShopItemTable TicketRow(EItemType Item, int32 Quantity = 10)
    {
        FShopItemTable Row;
        Row.Tab = EShopTab::Diamond;
        Row.Item = Item;
        Row.Quantity = Quantity;
        Row.Currency = EShopCurrency::Diamond;
        Row.Price = 500;
        return Row;
    }

    FEquipmentTicketShopRequest TicketRequest(FName RowName, const FShopItemTable& Row)
    {
        FEquipmentTicketShopRequest Request;
        Request.OperationId = FGuid::NewGuid();
        Request.RowName = RowName;
        Request.CatalogFingerprint = EquipmentTicketShopRules::CatalogFingerprint(RowName, Row);
        return Request;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEquipmentTicketShopPurchaseTest, "CompanyGrowth.EmployeeEquipment.TicketShop.Purchase", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEquipmentTicketShopPurchaseTest::RunTest(const FString& Parameters)
{
    const EItemType TicketTypes[] = {EItemType::EmployeeEquipmentTicketNormal, EItemType::EmployeeEquipmentTicketAdvanced, EItemType::EmployeeEquipmentTicketPremium};
    for (EItemType Item : TicketTypes)
    {
        const FShopItemTable Row = TicketRow(Item);
        const FEquipmentTicketShopRequest Request = TicketRequest(TEXT("TicketSku"), Row);
        FGameSaveData Candidate;
        Candidate.ResourceBank.Add(EResourceType::Diamond, 900);
        Candidate.ResourceBank.Add(EResourceType::Money, 700);
        Candidate.ItemInventory.Add(Item, 3);
        FEmployeeEquipmentInstance Existing;
        Existing.InstanceId = FGuid::NewGuid();
        Candidate.EmployeeEquipmentState.Instances.Add(Existing.InstanceId, Existing);
        FEquipmentTicketShopResult Result;
        TestTrue(TEXT("Each ticket type purchases through the same candidate contract"), EquipmentTicketShopRules::BuildPurchaseCandidate(Request, Row, FDateTime(2026,9,8), Candidate, Result));
        TestEqual(TEXT("The displayed SKU grants exactly ten of its ticket"), Candidate.ItemInventory.FindRef(Item), 13);
        TestEqual(TEXT("Only quoted diamonds are spent"), Candidate.ResourceBank.FindRef(EResourceType::Diamond), int64(400));
        TestEqual(TEXT("Money is unchanged"), Candidate.ResourceBank.FindRef(EResourceType::Money), int64(700));
        TestEqual(TEXT("Purchasing tickets never grants equipment"), Candidate.EmployeeEquipmentState.Instances.Num(), 1);
        TestEqual(TEXT("Receipt records the exact quantity"), Result.Quantity, 10);
        TestTrue(TEXT("The receipt is persisted with the candidate"), Candidate.ShopSaveData.EquipmentTicketPurchases.Contains(Request.OperationId));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEquipmentTicketShopStaleQuoteTest, "CompanyGrowth.EmployeeEquipment.TicketShop.StaleQuote", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEquipmentTicketShopStaleQuoteTest::RunTest(const FString& Parameters)
{
    const FShopItemTable Original = TicketRow(EItemType::EmployeeEquipmentTicketNormal);
    const FEquipmentTicketShopRequest Request = TicketRequest(TEXT("TicketSku"), Original);
    for (int32 Change = 0; Change < 6; ++Change)
    {
        FShopItemTable Changed = Original;
        if (Change == 0) Changed.Price = 501;
        if (Change == 1) Changed.Quantity = 11;
        if (Change == 2) Changed.Item = EItemType::EmployeeEquipmentTicketAdvanced;
        if (Change == 3) Changed.Currency = EShopCurrency::Money;
        if (Change == 4) Changed.LimitCount = 1;
        if (Change == 5) Changed.Tab = EShopTab::Weekly;
        FGameSaveData Candidate;
        Candidate.ResourceBank.Add(EResourceType::Diamond, 900);
        FEquipmentTicketShopResult Result;
        TestFalse(TEXT("Changed SKU requires a fresh confirmation"), EquipmentTicketShopRules::BuildPurchaseCandidate(Request, Changed, FDateTime(2026,9,8), Candidate, Result));
        TestEqual(TEXT("Stale quotation is explicit"), Result.ErrorCode, FName(TEXT("CatalogChanged")));
        TestEqual(TEXT("Rejected quote cannot spend"), Candidate.ResourceBank.FindRef(EResourceType::Diamond), int64(900));
        TestTrue(TEXT("Rejected quote cannot grant or record a receipt"), Candidate.ItemInventory.IsEmpty() && Candidate.ShopSaveData.EquipmentTicketPurchases.IsEmpty());
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEquipmentTicketShopWeeklyTest, "CompanyGrowth.EmployeeEquipment.TicketShop.WeeklyLimit", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEquipmentTicketShopWeeklyTest::RunTest(const FString& Parameters)
{
    FShopItemTable Row = TicketRow(EItemType::EmployeeEquipmentTicketNormal, 14);
    Row.Tab = EShopTab::Weekly; Row.Currency = EShopCurrency::Money; Row.Price = 0; Row.LimitCount = 1;
    const FEquipmentTicketShopRequest Request = TicketRequest(TEXT("WeeklyTicket"), Row);
    FGameSaveData Candidate;
    FEquipmentTicketShopResult Result;
    const FDateTime Sunday(2026,9,6,14,59,59);
    const FDateTime Monday(2026,9,6,15,0,0);
    TestTrue(TEXT("Weekly free tickets are granted without any money balance"), EquipmentTicketShopRules::BuildPurchaseCandidate(Request, Row, Sunday, Candidate, Result));
    TestEqual(TEXT("Exactly fourteen free normal tickets"), Candidate.ItemInventory.FindRef(EItemType::EmployeeEquipmentTicketNormal), 14);
    TestEqual(TEXT("Current KST week is exhausted"), EquipmentTicketShopRules::RemainingCount(Request.RowName, Row, Candidate.ShopSaveData, Sunday), 0);
    TestFalse(TEXT("A second purchase in the week is rejected"), EquipmentTicketShopRules::BuildPurchaseCandidate(TicketRequest(Request.RowName, Row), Row, Sunday, Candidate, Result));
    TestEqual(TEXT("Limit error is explicit"), Result.ErrorCode, FName(TEXT("PurchaseLimit")));
    TestEqual(TEXT("Monday midnight KST restores one purchase"), EquipmentTicketShopRules::RemainingCount(Request.RowName, Row, Candidate.ShopSaveData, Monday), 1);
    TestTrue(TEXT("New week grants again"), EquipmentTicketShopRules::BuildPurchaseCandidate(TicketRequest(Request.RowName, Row), Row, Monday, Candidate, Result));
    TestEqual(TEXT("New week grants only one further bundle"), Candidate.ItemInventory.FindRef(EItemType::EmployeeEquipmentTicketNormal), 28);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEquipmentTicketShopReplayTest, "CompanyGrowth.EmployeeEquipment.TicketShop.Replay", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEquipmentTicketShopReplayTest::RunTest(const FString& Parameters)
{
    const FShopItemTable Row = TicketRow(EItemType::EmployeeEquipmentTicketNormal);
    const FEquipmentTicketShopRequest Request = TicketRequest(TEXT("TicketSku"), Row);
    FGameSaveData Candidate;
    Candidate.ResourceBank.Add(EResourceType::Diamond, 900);
    FEquipmentTicketShopResult Result;
    TestTrue(TEXT("First purchase creates a receipt"), EquipmentTicketShopRules::BuildPurchaseCandidate(Request, Row, FDateTime(2026,9,8), Candidate, Result));
    FEquipmentTicketShopResult Replay;
    TestTrue(TEXT("Same confirmed request is recognized"), EquipmentTicketShopRules::TryReplay(Request, Candidate.ShopSaveData, Replay));
    TestTrue(TEXT("Same request returns its committed receipt"), Replay.bCommitted);
    TestEqual(TEXT("Replay keeps the original cost"), Replay.SpentCurrency, int64(500));
    TestEqual(TEXT("Replay does not spend a second time"), Candidate.ResourceBank.FindRef(EResourceType::Diamond), int64(400));
    FEquipmentTicketShopRequest Tampered = Request;
    Tampered.RowName = TEXT("OtherSku");
    TestTrue(TEXT("Changed request with the same operation is recognized as conflict"), EquipmentTicketShopRules::TryReplay(Tampered, Candidate.ShopSaveData, Replay));
    TestFalse(TEXT("Conflicting request is not committed"), Replay.bCommitted);
    TestEqual(TEXT("Conflict is explicit"), Replay.ErrorCode, FName(TEXT("IdempotencyConflict")));
    USaveGame_GameData* SaveObject = NewObject<USaveGame_GameData>();
    SaveObject->GameData = Candidate;
    TArray<uint8> SaveBytes;
    TestTrue(TEXT("Receipt-bearing save serializes through the real save format"), UGameplayStatics::SaveGameToMemory(SaveObject, SaveBytes));
    USaveGame_GameData* Loaded = Cast<USaveGame_GameData>(UGameplayStatics::LoadGameFromMemory(SaveBytes));
    if (TestNotNull(TEXT("Receipt-bearing save loads"), Loaded))
    {
        UGameInstance* TestGame = NewObject<UGameInstance>();
        UShopManagerSubsystem* Shop = NewObject<UShopManagerSubsystem>(TestGame);
        Shop->SetShopData(Loaded->GameData.ShopSaveData);
        const FEquipmentTicketShopResult RestoredReplay = Shop->PurchaseEquipmentTickets(Request);
        TestTrue(TEXT("The actual shop manager replays a restored receipt before catalog access"), RestoredReplay.bCommitted);
        TestEqual(TEXT("Restored manager returns original ticket quantity"), RestoredReplay.Quantity, 10);
        TestEqual(TEXT("Replay leaves one durable operation"), Shop->GetShopData().EquipmentTicketPurchases.Num(), 1);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEquipmentTicketShopFailureTest, "CompanyGrowth.EmployeeEquipment.TicketShop.FailureRollback", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEquipmentTicketShopFailureTest::RunTest(const FString& Parameters)
{
    const FShopItemTable Row = TicketRow(EItemType::EmployeeEquipmentTicketNormal);
    const FEquipmentTicketShopRequest Request = TicketRequest(TEXT("TicketSku"), Row);
    FGameSaveData Live;
    Live.ResourceBank.Add(EResourceType::Diamond, 900);
    Live.ItemInventory.Add(Row.Item, MAX_int32 - 9);
    FGameSaveData Candidate = Live;
    FEquipmentTicketShopResult Result;
    TestFalse(TEXT("Overflowing a ticket balance is rejected"), EquipmentTicketShopRules::BuildPurchaseCandidate(Request, Row, FDateTime(2026,9,8), Candidate, Result));
    TestEqual(TEXT("Overflow does not spend"), Candidate.ResourceBank.FindRef(EResourceType::Diamond), int64(900));
    Live.ItemInventory.Add(Row.Item, 4);
    Candidate = Live;
    TestTrue(TEXT("Valid candidate is ready for atomic persistence"), EquipmentTicketShopRules::BuildPurchaseCandidate(Request, Row, FDateTime(2026,9,8), Candidate, Result));
    bool bCommitting = false;
    int32 Publications = 0;
    TestFalse(TEXT("Disk failure rejects the candidate"), EmployeeEquipmentRules::CommitCandidate(bCommitting, Candidate,
        [](const FGameSaveData&) { return false; },
        [&Live](const FGameSaveData& Saved) { Live = Saved; },
        [&Publications](const FGameSaveData&) { ++Publications; }));
    TestEqual(TEXT("Disk failure leaves live currency intact"), Live.ResourceBank.FindRef(EResourceType::Diamond), int64(900));
    TestEqual(TEXT("Disk failure leaves live ticket balance intact"), Live.ItemInventory.FindRef(Row.Item), 4);
    TestTrue(TEXT("Disk failure leaves no successful receipt or limit consumption"), Live.ShopSaveData.EquipmentTicketPurchases.IsEmpty() && Live.ShopSaveData.PurchaseCounts.IsEmpty());
    TestEqual(TEXT("Disk failure never publishes success"), Publications, 0);
    Candidate = Live;
    Candidate.ResourceBank.Add(EResourceType::Diamond, 499);
    TestFalse(TEXT("Insufficient diamonds cannot buy"), EquipmentTicketShopRules::BuildPurchaseCandidate(Request, Row, FDateTime(2026,9,8), Candidate, Result));
    TestEqual(TEXT("Insufficient funds leave tickets unchanged"), Candidate.ItemInventory.FindRef(Row.Item), 4);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEquipmentTicketShopInvalidProductTest, "CompanyGrowth.EmployeeEquipment.TicketShop.InvalidProduct", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEquipmentTicketShopInvalidProductTest::RunTest(const FString& Parameters)
{
    for (int32 InvalidCase = 0; InvalidCase < 5; ++InvalidCase)
    {
        FShopItemTable Row = TicketRow(EItemType::EmployeeEquipmentTicketNormal);
        if (InvalidCase == 0) Row.Item = EItemType::ProtectionScroll;
        if (InvalidCase == 1) Row.Currency = EShopCurrency::Mileage;
        if (InvalidCase == 2) Row.Quantity = 0;
        if (InvalidCase == 3) Row.Price = -1;
        if (InvalidCase == 4) Row.LimitCount = -1;
        FGameSaveData Candidate;
        Candidate.ResourceBank.Add(EResourceType::Diamond, 900);
        FEquipmentTicketShopResult Result;
        TestFalse(TEXT("Invalid SKU cannot enter ticket transaction path"), EquipmentTicketShopRules::BuildPurchaseCandidate(TicketRequest(TEXT("BadSku"), Row), Row, FDateTime(2026,9,8), Candidate, Result));
        TestEqual(TEXT("Invalid SKU failure is explicit"), Result.ErrorCode, FName(TEXT("InvalidProduct")));
        TestEqual(TEXT("Invalid SKU leaves currency intact"), Candidate.ResourceBank.FindRef(EResourceType::Diamond), int64(900));
        TestTrue(TEXT("Invalid SKU grants nothing"), Candidate.ItemInventory.IsEmpty());
    }
    return true;
}
#endif
