#pragma once

#include "CoreMinimal.h"
#include "LiveOpsSaveData.generated.h"

USTRUCT(BlueprintType)
struct FLiveOpsEventProgress
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	int64 Points = 0;

	UPROPERTY(SaveGame)
	TArray<int32> ClaimedMilestones;

	// AttendanceEvent: 이벤트를 처음 관측한 시점의 출석 누계 (진행 = 현재 누계 − 이 값)
	UPROPERTY(SaveGame)
	int32 StartAttendanceTotal = -1;

	// 종료 후 미수령 보상 회수를 한 번만
	UPROPERTY(SaveGame)
	bool bExpiredHandled = false;
};

USTRUCT(BlueprintType)
struct FLiveOpsSaveData
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	TMap<FString, FLiveOpsEventProgress> Events;

	UPROPERTY(SaveGame)
	TArray<FString> ReadNoticeIds;
};

// 접속·하루 첫 한가한 순간에 스스로 여는 창을 「그날 이미 띄웠나」 ― 닫고 안 받아도 같은 날 다시 밀지 않는다
USTRUCT(BlueprintType)
struct FEntryPopupSaveData
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FString LastAttendanceDayKey;

	UPROPERTY(SaveGame)
	FString LastNoticeDayKey;
};
