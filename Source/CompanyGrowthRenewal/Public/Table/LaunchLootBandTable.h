#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "LaunchLootBandTable.generated.h"

/**
 * 출시 평점 판정 밴드 표시 행 (DT_LaunchLootBand, 행 이름 = Low/Mid/High = DT_LaunchLoot.TableKey).
 * 임계(24/32)는 ULaunchLootManagerSubsystem 상수가 SOT — 여기엔 표시명/색만 둔다.
 */
USTRUCT(BlueprintType)
struct FLaunchLootBandRow : public FTableRowBase
{
	GENERATED_BODY()

	// 판정 도장/텍스트 라벨 (범작/수작/명작)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DisplayName;

	// 도장 테두리/판정 텍스트 색 (linear)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FLinearColor Color = FLinearColor::White;
};
