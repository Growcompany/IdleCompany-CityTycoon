#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Data/WorldMapTypes.h"
#include "TradeOrderTierDisplayTable.generated.h"

/**
 * 무역 주문 Tier (긴급/VIP/일반) → 한국어 라벨 + 표시 색상 매핑
 * 단일 진실 원천: DT_TradeOrderTierDisplay (DataImport/DT_TradeOrderTierDisplay_Import.csv)
 */
USTRUCT(BlueprintType)
struct FTradeOrderTierDisplayRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	ETradeOrderTier Tier = ETradeOrderTier::Normal;

	// 라벨 텍스트 ("[긴급]", "[VIP]", "[일반]")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	FText Label;

	// Border 색상
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	FLinearColor Color = FLinearColor::White;
};
