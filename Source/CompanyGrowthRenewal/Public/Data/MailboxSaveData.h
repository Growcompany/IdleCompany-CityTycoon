#pragma once

#include "CoreMinimal.h"
#include "Table/MissionTable.h"
#include "MailboxSaveData.generated.h"

USTRUCT(BlueprintType)
struct FMailEntry
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FGuid Id;

	// 글로벌 메일 ID (시스템 메일은 빈값) — 1회 수신 판정 키
	UPROPERTY(SaveGame)
	FString SourceKey;

	UPROPERTY(SaveGame)
	FText Title;

	UPROPERTY(SaveGame)
	FText Body;

	UPROPERTY(SaveGame)
	TArray<FMissionReward> Rewards;

	UPROPERTY(SaveGame)
	int64 ReceivedUnix = 0;

	UPROPERTY(SaveGame)
	int64 ExpireUnix = 0;
};

USTRUCT(BlueprintType)
struct FMailboxSaveData
{
	GENERATED_BODY()

	// 수령 즉시 제거되므로 여기 있는 것 = 전부 미수령
	UPROPERTY(SaveGame)
	TArray<FMailEntry> Mails;

	UPROPERTY(SaveGame)
	TArray<FString> ConsumedGlobalMailIds;
};
