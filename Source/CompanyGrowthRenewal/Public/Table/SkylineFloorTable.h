#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "SkylineFloorTable.generated.h"

/**
 * 스카이라인 층 밴드 1행 (DT_SkylineFloor). 계정 하나 — 건물 축과 달리 진행도 공용이다(설계 v0.8 §모듈2).
 *
 * 행 = 밴드다. 층 f 의 요구치는 그 f 를 품는 밴드가 정한다:
 *     Required(f) = RequiredBase x RequiredMul^(f - 1) x (보스 층이면 BossMul)
 *
 * ⚠ BossMul 이 1 보다 크면 **보스 다음 층이 보스보다 싸다**(40층 729 > 41층 479).
 *    등반이 관문 단위로 끊기는 이 성질은 버그가 아니라 확정된 의도다(DECISION_RECORDS §13.3).
 *    평탄하게 만들고 싶으면 BossMul 을 1 로 두면 되고, 그러면 "다음 관문" 태그도 의미를 잃는다.
 */
USTRUCT(BlueprintType)
struct FSkylineFloorRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 MinFloor = 1;

	/** 마지막 행의 이 값이 곧 최고 층이다 — 코드에 100 을 박지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 MaxFloor = 100;

	/** 층 하나를 깰 때 굴리는 드랍 횟수. FSideContentRules::RollLoot 의 Rolls. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 Rolls = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float RequiredBase = 68.0f;

	/** 밴드마다 다른 값을 넣으면 경계에서 요구치가 튄다 — 의도한 게 아니면 전 행을 같게 둘 것. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float RequiredMul = 1.05f;

	/** 이 수의 배수 층이 보스. 0 = 이 밴드엔 보스 없음. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 BossEvery = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float BossMul = 1.6f;

	/** 보스 층에서만 굴리는 추가 드랍표 키. 비면 보스도 일반 표만 굴린다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName BossLootKey;

	/**
	 * 층 카드에 붙는 입주 기업 플레이버. 밴드가 범위라 같은 밴드는 같은 이름을 쓴다 —
	 * 층마다 다른 이름을 붙이고 싶으면 **1층짜리 밴드**를 그만큼 만들면 된다(CSV 편집으로 완결).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText TenantName;
};
