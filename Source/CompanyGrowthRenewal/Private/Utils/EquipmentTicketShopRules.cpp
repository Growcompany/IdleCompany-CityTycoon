#include "Utils/EquipmentTicketShopRules.h"
#include "Utils/EmployeeEquipmentTransactionRules.h"
#include "Data/GameSaveData.h"
#include "Misc/SecureHash.h"

namespace
{
    FString TicketPeriod(const FShopItemTable& Row, const FDateTime& UtcNow)
    {
        if (Row.Tab == EShopTab::Weekly) return EmployeeEquipmentRules::WeeklyPeriodId(UtcNow);
        if (Row.Tab == EShopTab::Daily) return (UtcNow + FTimespan::FromHours(9)).ToString(TEXT("%Y-%m-%d"));
        return TEXT("Permanent");
    }

    bool Reject(FEquipmentTicketShopResult& Result, FName ErrorCode)
    {
        Result.ErrorCode = ErrorCode;
        return false;
    }
}

bool EquipmentTicketShopRules::IsTicket(EItemType Item)
{
    return EmployeeEquipmentRules::IsEquipmentTicket(Item);
}

bool EquipmentTicketShopRules::ValidateRow(FName RowName, const FShopItemTable& Row, FName& OutError)
{
    OutError = NAME_None;
    if (RowName.IsNone() || !IsTicket(Row.Item) || Row.Quantity <= 0 || Row.Price < 0 || Row.LimitCount < 0 ||
        Row.Tab <= EShopTab::None || Row.Tab >= EShopTab::Count || (Row.Currency != EShopCurrency::Money && Row.Currency != EShopCurrency::Diamond))
    {
        OutError = TEXT("InvalidProduct");
        return false;
    }
    return true;
}

FString EquipmentTicketShopRules::CatalogFingerprint(FName RowName, const FShopItemTable& Row)
{
    const FString Canonical = FString::Printf(TEXT("%s|%d|%d|%d|%lld|%d|%d"),
        *RowName.ToString().ToLower(), int32(Row.Item), Row.Quantity, int32(Row.Currency), Row.Price, Row.LimitCount, int32(Row.Tab));
    return FMD5::HashAnsiString(*Canonical);
}

FString EquipmentTicketShopRules::RequestFingerprint(const FEquipmentTicketShopRequest& Request)
{
    const FString Canonical = Request.RowName.ToString().ToLower() + TEXT("|") + Request.CatalogFingerprint;
    return FMD5::HashAnsiString(*Canonical);
}

int32 EquipmentTicketShopRules::RemainingCount(FName RowName, const FShopItemTable& Row, const FShopSaveData& ShopData, const FDateTime& UtcNow)
{
    if (Row.LimitCount <= 0) return -1;
    if (ShopData.EquipmentTicketResetPeriods.FindRef(RowName) != TicketPeriod(Row, UtcNow)) return Row.LimitCount;
    const int32 Purchased = ShopData.PurchaseCounts.FindRef(RowName);
    return Purchased < 0 ? 0 : FMath::Max(0, Row.LimitCount - Purchased);
}

bool EquipmentTicketShopRules::TryReplay(const FEquipmentTicketShopRequest& Request, const FShopSaveData& ShopData, FEquipmentTicketShopResult& OutResult)
{
    const FEquipmentTicketShopReceipt* Receipt = ShopData.EquipmentTicketPurchases.Find(Request.OperationId);
    if (!Receipt) return false;
    if (Receipt->RequestFingerprint == RequestFingerprint(Request)) OutResult = Receipt->Result;
    else
    {
        OutResult = FEquipmentTicketShopResult();
        OutResult.OperationId = Request.OperationId;
        OutResult.ErrorCode = TEXT("IdempotencyConflict");
    }
    return true;
}

bool EquipmentTicketShopRules::BuildPurchaseCandidate(const FEquipmentTicketShopRequest& Request, const FShopItemTable& Row, const FDateTime& UtcNow, FGameSaveData& Candidate, FEquipmentTicketShopResult& OutResult)
{
    OutResult = FEquipmentTicketShopResult();
    OutResult.OperationId = Request.OperationId;
    if (!Request.OperationId.IsValid() || Request.RowName.IsNone() || Request.CatalogFingerprint.IsEmpty()) return Reject(OutResult, TEXT("InvalidRequest"));
    if (Request.CatalogFingerprint != CatalogFingerprint(Request.RowName, Row)) return Reject(OutResult, TEXT("CatalogChanged"));
    if (!ValidateRow(Request.RowName, Row, OutResult.ErrorCode)) return false;
    if (RemainingCount(Request.RowName, Row, Candidate.ShopSaveData, UtcNow) == 0) return Reject(OutResult, TEXT("PurchaseLimit"));
    const EResourceType Resource = Row.Currency == EShopCurrency::Money ? EResourceType::Money : EResourceType::Diamond;
    const int64 Balance = Candidate.ResourceBank.FindRef(Resource);
    const int32 Tickets = Candidate.ItemInventory.FindRef(Row.Item);
    if (Balance < Row.Price) return Reject(OutResult, TEXT("InsufficientCurrency"));
    if (Tickets < 0 || Tickets > MAX_int32 - Row.Quantity) return Reject(OutResult, TEXT("BalanceOverflow"));

    Candidate.ResourceBank.Add(Resource, Balance - Row.Price);
    Candidate.ItemInventory.Add(Row.Item, Tickets + Row.Quantity);
    if (Row.LimitCount > 0)
    {
        const FString Period = TicketPeriod(Row, UtcNow);
        const int32 Purchased = Candidate.ShopSaveData.EquipmentTicketResetPeriods.FindRef(Request.RowName) == Period
            ? Candidate.ShopSaveData.PurchaseCounts.FindRef(Request.RowName) : 0;
        Candidate.ShopSaveData.PurchaseCounts.Add(Request.RowName, Purchased + 1);
        Candidate.ShopSaveData.EquipmentTicketResetPeriods.Add(Request.RowName, Period);
    }
    OutResult.bCommitted = true;
    OutResult.Item = Row.Item;
    OutResult.Quantity = Row.Quantity;
    OutResult.Currency = Row.Currency;
    OutResult.SpentCurrency = Row.Price;
    OutResult.RemainingCount = RemainingCount(Request.RowName, Row, Candidate.ShopSaveData, UtcNow);
    FEquipmentTicketShopReceipt Receipt;
    Receipt.RequestFingerprint = RequestFingerprint(Request);
    Receipt.Result = OutResult;
    Candidate.ShopSaveData.EquipmentTicketPurchases.Add(Request.OperationId, Receipt);
    return true;
}
