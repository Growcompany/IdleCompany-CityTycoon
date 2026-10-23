#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Enum/LiveOpsTypes.h"
#include "Enum/QuestTypes.h"
#include "Table/MissionTable.h"
#include "LiveOpsTables.generated.h"

USTRUCT(BlueprintType)
struct FLiveOpsMilestone
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int64 Threshold = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FMissionReward> Rewards;
};

// KST 시각 창 [Start, End). End < Start 면 자정을 넘긴다(22~2); End == Start 는 금지(24h 는 두 창으로)
USTRUCT(BlueprintType)
struct FLiveOpsHotTimeWindow
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 StartHourKst = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 EndHourKst = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float Multiplier = 1.0f;
};

// DT_LiveOpsEvent 행 / TitleData "LiveOps.Events" JSON 배열 원소 (같은 스키마)
USTRUCT(BlueprintType)
struct FLiveOpsEventDef : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString Id;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ELiveOpsEventType Type = ELiveOpsEventType::CollectEvent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText Desc;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int64 StartUnix = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int64 EndUnix = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FLinearColor BannerColor = FLinearColor(0.2f, 0.3f, 0.6f, 1.0f);

	// CollectEvent 전용
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EQuestCounter Counter = EQuestCounter::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FLiveOpsMilestone> Milestones;

	// HotTimeEvent 전용
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FLiveOpsHotTimeWindow> HotTime;
};

USTRUCT(BlueprintType)
struct FLiveOpsNoticeDef : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString Id;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText Body;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int64 PostedUnix = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bPinned = false;
};

USTRUCT(BlueprintType)
struct FLiveOpsMailDef : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString Id;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText Body;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FMissionReward> Rewards;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int64 StartUnix = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int64 EndUnix = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 ExpireDays = 30;
};
