#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Enum/MissionTypes.h"
#include "Table/MissionTable.h"
#include "GoalTable.generated.h"

/**
 * 미션판(GoalBoard) 미션 1건 (DT_Goal 행, RowName = GoalID).
 * 튜토리얼 체인 종료 후 동시 노출·자유 순서 수령 (스펙 2026-08-02 §5).
 */
USTRUCT(BlueprintType)
struct FGoalTable : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText Title;

	// 위치 안내를 포함한 설명 — 스포트라이트 가이드가 없으므로 이 텍스트가 길 안내를 담당
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText Desc;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EMissionConditionType ConditionType = EMissionConditionType::None;

	// 도달형 목표 수치 (ReachHQLevel = 목표 레벨). 이벤트형은 0
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int64 ConditionAmount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FMissionReward> Rewards;

	// 이 미션이 수령되기 전까지 잠금 카드 표시 (None = 항상 노출). 인수 3부작 순서 표현용
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName PrereqGoalID;

	// 단계별 안내 문구. 인덱스 = GetTrackedGoalStep 이 도출한 단계 번호.
	// ⚠ **비어 있으면 안내 없는 미션** — [안내하기] 버튼 자체가 안 뜬다(반복 미션용).
	// 별도 플래그를 두지 않는 이유 = 플래그와 데이터가 어긋날 여지를 없애기 위함(빈 셀 = 의도적 없음)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FText> StepLines;
};
