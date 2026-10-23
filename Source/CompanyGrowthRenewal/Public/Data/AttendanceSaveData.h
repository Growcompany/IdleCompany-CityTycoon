#pragma once

#include "CoreMinimal.h"
#include "AttendanceSaveData.generated.h"

USTRUCT(BlueprintType)
struct FAttendanceSaveData
{
	GENERATED_BODY()

	// 0 = 아직 없음, 1~28 = 마지막으로 받은 일차
	UPROPERTY(SaveGame)
	int32 CycleDay = 0;

	UPROPERTY(SaveGame)
	FString LastClaimDayKey;

	// 이벤트(누적 출석)용 누계
	UPROPERTY(SaveGame)
	int32 TotalDays = 0;
};
