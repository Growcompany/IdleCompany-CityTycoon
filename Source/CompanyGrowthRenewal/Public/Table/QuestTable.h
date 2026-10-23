#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "Enum/QuestTypes.h"
#include "Table/MissionTable.h"
#include "QuestTable.generated.h"

// DT_Quest 행 (RowName = QuestID). 일일/주간은 Track/Tier 비움
USTRUCT(BlueprintType)
struct FQuestTable : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	EQuestPeriod Period = EQuestPeriod::Daily;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	EQuestCounter Counter = EQuestCounter::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	int64 Target = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	FText Desc;

	// Money 금지 — 로드 시 ensure
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	TArray<FMissionReward> Rewards;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	FName Track;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	int32 Tier = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	int32 SortOrder = 0;

	// 줄 왼쪽 종류 아이콘 (흰 글리프 ― 줄 위젯이 한 가지 잉크로 칠한다). 비면 아이콘 칸을 접는다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	TSoftObjectPtr<UTexture2D> Icon;
};
