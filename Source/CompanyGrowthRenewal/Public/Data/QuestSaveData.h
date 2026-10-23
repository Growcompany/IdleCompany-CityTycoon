#pragma once

#include "CoreMinimal.h"
#include "QuestSaveData.generated.h"

// 퀘스트 원장 저장 블록 (게임 전체 1개). 주기 상태는 키가 바뀌면 롤오버로 비워진다
USTRUCT(BlueprintType)
struct FQuestSaveData
{
	GENERATED_BODY()

	UPROPERTY(SaveGame) FString DayKey;
	UPROPERTY(SaveGame) FString WeekKey;
	// LoginDay 카운터를 하루 1회만 올리기 위한 키
	UPROPERTY(SaveGame) FString LastLoginDayKey;

	// 일일/주간 증분형 행의 진행 (QuestID → 누적)
	UPROPERTY(SaveGame) TMap<FName, int64> Progress;
	UPROPERTY(SaveGame) TArray<FName> Claimed;

	// 업적 증분형 카운터 (EQuestCounter 이름 → 누적, 티어가 공유)
	UPROPERTY(SaveGame) TMap<FName, int64> LifetimeProgress;
	UPROPERTY(SaveGame) TArray<FName> ClaimedLifetime;
};
