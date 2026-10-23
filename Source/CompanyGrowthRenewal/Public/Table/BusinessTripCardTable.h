#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Enum/ProductionDiscipline.h"
#include "BusinessTripCardTable.generated.h"

/**
 * 출장 카드 1행 (DT_BusinessTripCard, RowName = 카드 키).
 * 카드는 건물마다 자정에 이 표에서 3장을 뽑는다(설계 §모듈4).
 */
USTRUCT(BlueprintType)
struct FBusinessTripCardTable : public FTableRowBase
{
	GENERATED_BODY()

	/** 목적지 표시명. 코드 분기 금지 — 이 문자열이 단일 진실. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DisplayName;

	/** 카드 아트 244x80. 비면 아트 칸을 접는다 — 티어별 공용 그림으로 대신 채우지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UTexture2D> CardArt;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 DurationSeconds = 1800;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 CrewCount = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EProductionDiscipline RequiredDiscipline = EProductionDiscipline::Plan;

	/** 고른 직원들의 해당 직능 점수 합이 이 값 이상이어야 출발한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 RequiredScoreSum = 0;

	/** 확정 보상 — 보낸 직원 각자가 받는 경험치. 직능 포인트를 직접 주지 않는다(사용자 확정 2026-09-24). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float ExperienceReward = 0.0f;

	/** DT_SideDungeonLoot 의 TableKey. 비면 드랍 없음. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName LootTableKey;

	/** 드랍 밴드 선택에 쓰는 단계 (1=30m · 2=4h · 3=12h). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 LootStage = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 LootRolls = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 RequiredCompanyLevel = 3;
};
