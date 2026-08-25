#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "PanelIntroTable.generated.h"

/**
 * 말풍선 꼬리 방향 = 대상이 툴팁의 어느 쪽에 있는가 (Up = 대상이 위, 툴팁은 그 아래).
 * ⚠ UI/HUD/GuideTooltipPlacement.h 의 EGuideTooltipDir 과 값 순서가 같아야 한다 —
 *   그쪽은 UENUM 이 아니라 DT 컬럼으로 못 쓰므로 오버레이가 static_cast 로 넘긴다.
 */
UENUM(BlueprintType)
enum class EPanelIntroTailDir : uint8
{
	Up,
	Down,
	Left,
	Right,
};

/**
 * 패널 최초 진입 코치마크 1스텝 (DT_PanelIntro 행, RowName = <PanelKey>_<StepIndex>).
 * 설계 = docs/superpowers/specs/2026-08-12-panel-intro-coachmark-design.md
 */
USTRUCT(BlueprintType)
struct FPanelIntroTable : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName PanelKey;

	// 같은 PanelKey 안에서 이 값 오름차순으로 재생 (0-base)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 StepIndex = 0;

	// 소유 패널의 위젯 이름 (GetWidgetFromName 인자). 못 찾으면 그 스텝만 경고 후 스킵
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName AnchorName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText Eyebrow;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText Body;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EPanelIntroTailDir TailDir = EPanelIntroTailDir::Up;
};
