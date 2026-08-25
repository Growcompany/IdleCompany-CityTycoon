#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Enum/ResourceType.h"
#include "Enum/ItemType.h"
#include "LaunchLootTable.generated.h"

/**
 * 출시 전리품 드랍 엔트리 (DT_LaunchLoot 행 1개 = 후보 1개).
 * TableKey(Low/Mid/High)로 묶이고, 슬롯마다 Weight 가중 랜덤으로 1개 선정된다.
 * 스펙 = docs/superpowers/specs/2026-08-02-launch-loot-drop-design.md §3
 */
USTRUCT(BlueprintType)
struct FLaunchLootTable : public FTableRowBase
{
	GENERATED_BODY()

	// 소속 드랍 테이블 (Low / Mid / High)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName TableKey;

	// 재화 드랍이면 지정 (None 이면 아이템 드랍)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EResourceType ResourceType = EResourceType::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EItemType ItemType = EItemType::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int64 AmountMin = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int64 AmountMax = 1;

	// true 면 Amount 를 만분율로 해석 (지급량 = ScaleBasis * Amount / 10000) — 재화용. 티켓은 false(개수 고정)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bScaleWithProject = false;

	// 등장 티어 구간 (0 = 무제한). 상위 아이템 게이트 + 저가치 엔트리 퇴장용
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 MinTier = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 MaxTier = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 Weight = 1;
};
