#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Enum/ItemType.h"
#include "Enum/ResourceType.h"
#include "SideDungeonLootTable.generated.h"

/** 드랍 확률 표시 3단 (COMMON §3-4). 값이 아니라 표기 — 실제 확률은 Weight 가 정한다. */
UENUM(BlueprintType)
enum class ESideDropOdds : uint8
{
	High   = 0 UMETA(DisplayName = "높음"),
	Normal = 1 UMETA(DisplayName = "보통"),
	Low    = 2 UMETA(DisplayName = "낮음")
};

/** UMETA 는 쿠킹에서 스트립되므로 표시 문자열을 코드가 직접 쥔다 (ItemType.h 와 같은 이유). */
inline FText GetSideDropOddsDisplayName(ESideDropOdds Odds)
{
	switch (Odds)
	{
	case ESideDropOdds::High:   return FText::FromString(TEXT("높음"));
	case ESideDropOdds::Normal: return FText::FromString(TEXT("보통"));
	case ESideDropOdds::Low:    return FText::FromString(TEXT("낮음"));
	default:                    return FText::GetEmpty();
	}
}

/**
 * 사이드 컨텐츠 드랍 1행 (DT_SideDungeonLoot).
 * 연수원(TableKey=Training)·출장(TableKey=Trip)이 같은 표를 밴드로 나눠 쓴다(설계 §모듈1·4).
 */
USTRUCT(BlueprintType)
struct FSideDungeonLootRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName TableKey;

	/** 단계 밴드 [MinStage, MaxStage]. 출장은 카드 기간 티어(1=30m, 2=4h, 3=12h)를 단계로 쓴다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 MinStage = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 MaxStage = 99;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EItemType ItemType = EItemType::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 ItemAmountMin = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 ItemAmountMax = 1;

	/** 원자재용. 자금 금지는 FRewardGrant::Commit 이 ensure 로 잡는다(설계 §0-3). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EResourceType ResourceType = EResourceType::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int64 ResourceAmountMin = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int64 ResourceAmountMax = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float Weight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ESideDropOdds Odds = ESideDropOdds::Normal;

	/** 이 본사 레벨 미만이면 후보에서 빠진다 (출장 원자재 = HQ5). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 RequiredHQLevel = 0;

	/** 건물별 컨텐츠 드랍 조건. HQ와 별도로 현재 회사 레벨을 검사한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 RequiredCompanyLevel = 0;
};
