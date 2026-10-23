#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "TrainingCourseTable.generated.h"

/**
 * 기술 연구소 과정 밴드 1행 (DT_TrainingCourse). 건물 공통 ― 과정 진행만 건물마다 따로다(설계 §모듈1).
 *
 * 행 = 밴드다. 과정 k 의 요구치는 그 k 를 품는 밴드가 정한다:
 *     Required(k) = RequiredBase x RequiredMul^(k - 1)
 * 전 밴드가 같은 Base/Mul(80 / 1.10)을 쓰면 스펙의 `80 x 1.10^(k-1)` 과 정확히 같고,
 * 후반만 가파르게 하고 싶으면 그 밴드 행의 Mul 만 올리면 된다(기획자 독립 ― CSV 편집으로 완결).
 */
USTRUCT(BlueprintType)
struct FTrainingCourseRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 MinCourse = 1;

	/** 마지막 행의 이 값이 곧 최종 과정 수다 ― 코드에 50 을 박지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 MaxCourse = 10;

	/** 이 밴드에서 도전·소탕 1회가 굴리는 드랍 횟수. FSideContentRules::RollLoot 의 Rolls. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 Rolls = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float RequiredBase = 80.0f;

	/** 밴드마다 다른 값을 넣으면 경계에서 요구치가 튄다 ― 의도한 게 아니면 전 행을 같게 둘 것. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float RequiredMul = 1.10f;
};
