#pragma once

#include "CoreMinimal.h"
#include "Enum/ItemType.h"
#include "Enum/ShopTypes.h"
#include "EquipmentTicketShopData.generated.h"

USTRUCT(BlueprintType)
struct FEquipmentTicketShopQuote
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FName RowName;
    UPROPERTY(BlueprintReadOnly) EItemType Item = EItemType::None;
    UPROPERTY(BlueprintReadOnly) int32 Quantity = 0;
    UPROPERTY(BlueprintReadOnly) EShopCurrency Currency = EShopCurrency::Diamond;
    UPROPERTY(BlueprintReadOnly) int64 Price = 0;
    UPROPERTY(BlueprintReadOnly) int32 LimitCount = 0;
    UPROPERTY(BlueprintReadOnly) EShopTab Tab = EShopTab::None;
    UPROPERTY(BlueprintReadOnly) int32 RemainingCount = 0;
    UPROPERTY(BlueprintReadOnly) bool bCanAfford = false;
    UPROPERTY(BlueprintReadOnly) FString CatalogFingerprint;
};

USTRUCT(BlueprintType)
struct FEquipmentTicketShopRequest
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadWrite) FGuid OperationId;
    UPROPERTY(BlueprintReadWrite) FName RowName;
    UPROPERTY(BlueprintReadWrite) FString CatalogFingerprint;
};

USTRUCT(BlueprintType)
struct FEquipmentTicketShopResult
{
    GENERATED_BODY()
    UPROPERTY(SaveGame, BlueprintReadOnly) FGuid OperationId;
    UPROPERTY(SaveGame, BlueprintReadOnly) bool bCommitted = false;
    UPROPERTY(SaveGame, BlueprintReadOnly) FName ErrorCode;
    UPROPERTY(SaveGame, BlueprintReadOnly) EItemType Item = EItemType::None;
    UPROPERTY(SaveGame, BlueprintReadOnly) int32 Quantity = 0;
    UPROPERTY(SaveGame, BlueprintReadOnly) EShopCurrency Currency = EShopCurrency::Diamond;
    UPROPERTY(SaveGame, BlueprintReadOnly) int64 SpentCurrency = 0;
    UPROPERTY(SaveGame, BlueprintReadOnly) int32 RemainingCount = 0;
};

USTRUCT()
struct FEquipmentTicketShopReceipt
{
    GENERATED_BODY()
    UPROPERTY(SaveGame) FString RequestFingerprint;
    UPROPERTY(SaveGame) FEquipmentTicketShopResult Result;
};
