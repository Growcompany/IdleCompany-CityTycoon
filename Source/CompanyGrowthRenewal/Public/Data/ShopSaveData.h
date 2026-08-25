#pragma once

#include "CoreMinimal.h"
#include "ShopSaveData.generated.h"

// 상점 구매 상태 (게임 전체 1개 — 한도 카운트 + 마지막 리셋 시각)
USTRUCT(BlueprintType)
struct FShopSaveData
{
	GENERATED_BODY()

	// DT_ShopItem RowName → 현재 주기 구매 횟수 (한도 있는 행만 기록)
	UPROPERTY(SaveGame)
	TMap<FName, int32> PurchaseCounts;

	UPROPERTY(SaveGame)
	FDateTime LastDailyReset = FDateTime::MinValue();

	UPROPERTY(SaveGame)
	FDateTime LastWeeklyReset = FDateTime::MinValue();
};
