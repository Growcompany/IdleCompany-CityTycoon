#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Enum/ShopTypes.h"
#include "Enum/ItemType.h"
#include "ShopItemTable.generated.h"

// 상점 상품 정의 (DT_ShopItem — 단일 진실. 품목/가격/한도는 CSV 리임포트만으로 수정)
USTRUCT(BlueprintType)
struct FShopItemTable : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EShopTab Tab = EShopTab::Daily;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EItemType Item = EItemType::None;

	// 1회 구매 시 지급 수량
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 Quantity = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EShopCurrency Currency = EShopCurrency::Money;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int64 Price = 0;

	// 리셋 주기당 구매 한도 (0 = 무제한)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 LimitCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 SortOrder = 0;
};
