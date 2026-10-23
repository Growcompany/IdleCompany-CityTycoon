#pragma once

#include "CoreMinimal.h"
#include "Data/EquipmentTicketShopData.h"
#include "ShopSaveData.generated.h"

// 상점 구매 상태 (게임 전체 1개 — 한도 카운트 + 마지막 리셋 키)
USTRUCT(BlueprintType)
struct FShopSaveData
{
	GENERATED_BODY()

	// DT_ShopItem RowName → 현재 주기 구매 횟수 (한도 있는 행만 기록)
	UPROPERTY(SaveGame)
	TMap<FName, int32> PurchaseCounts;

	UPROPERTY(SaveGame)
	TMap<FName, FString> EquipmentTicketResetPeriods;

	UPROPERTY(SaveGame)
	TMap<FGuid, FEquipmentTicketShopReceipt> EquipmentTicketPurchases;

	// 마지막으로 리셋을 적용한 게임 하루/주 키 (UGameClockSubsystem::GetDayKey 형식 "YYYY-MM-DD"). 빈값 = 아직 없음
	UPROPERTY(SaveGame)
	FString LastDailyKey;

	UPROPERTY(SaveGame)
	FString LastWeeklyKey;
};
