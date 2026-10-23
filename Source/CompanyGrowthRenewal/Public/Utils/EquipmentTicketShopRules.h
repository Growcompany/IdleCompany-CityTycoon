#pragma once

#include "CoreMinimal.h"
#include "Data/EquipmentTicketShopData.h"
#include "Table/ShopItemTable.h"

struct FGameSaveData;
struct FShopSaveData;

namespace EquipmentTicketShopRules
{
    bool IsTicket(EItemType Item);
    bool ValidateRow(FName RowName, const FShopItemTable& Row, FName& OutError);
    FString CatalogFingerprint(FName RowName, const FShopItemTable& Row);
    FString RequestFingerprint(const FEquipmentTicketShopRequest& Request);
    int32 RemainingCount(FName RowName, const FShopItemTable& Row, const FShopSaveData& ShopData, const FDateTime& UtcNow);
    bool TryReplay(const FEquipmentTicketShopRequest& Request, const FShopSaveData& ShopData, FEquipmentTicketShopResult& OutResult);
    bool BuildPurchaseCandidate(const FEquipmentTicketShopRequest& Request, const FShopItemTable& Row, const FDateTime& UtcNow, FGameSaveData& Candidate, FEquipmentTicketShopResult& OutResult);
}
