#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Enum/MissionTypes.h"
#include "Table/MissionTable.h"
#include "Enum/CompanyType.h"
#include "Enum/ProductionDiscipline.h"
#include "GoalTable.generated.h"

/**
 * 튜토리얼 이후 성장 안내 1건 (DT_Goal 행, RowName = GoalID).
 */
USTRUCT(BlueprintType)
struct FGoalTable : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText Title;

	// 행동의 목적을 설명하는 짧은 문장. 위치 안내는 StepLines가 담당한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText Desc;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EMissionConditionType ConditionType = EMissionConditionType::None;

	// 도달형 목표 수치 (ReachHQLevel = 목표 레벨). 이벤트형은 0
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int64 ConditionAmount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 RepeatLimit = 1;

	// 행동 수량을 누적한다. 도달형 목표는 현재 게임 상태에서 판정한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bCountActions = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int64 RepeatTargetIncrement = 0;

	// 0이면 별도 상한 없음. RepeatLimit의 0은 계속 반복한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int64 RepeatTargetCap = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int64 RewardMoney = 0;

	// 선행 안내의 첫 성공 이후 해제한다. None이면 선행 제한이 없다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName PrereqGoalID;

	// 추천 순서. 같은 값이면 DataTable 행 순서를 유지한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 RecommendationPriority = 100;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 RequiredHQLevel = 1;

	// 산업별 목표 및 선행 출시작 조회에 사용하는 산업.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ECompanyType CompanyType = ECompanyType::Game;

	// 해당 산업의 같은 회사에서 먼저 출시해야 하는 프로젝트. 0은 제한 없음.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 RequiredProjectIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 TargetProjectIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EProductionDiscipline RequiredDiscipline = EProductionDiscipline::Count;

	// 단계별 안내 문구. 인덱스 = GetTrackedGoalStep 이 도출한 단계 번호.
	// ⚠ **비어 있으면 안내 없는 미션** — [안내하기] 버튼 자체가 안 뜬다(반복 미션용).
	// 별도 플래그를 두지 않는 이유 = 플래그와 데이터가 어긋날 여지를 없애기 위함(빈 셀 = 의도적 없음)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FText> StepLines;
};
