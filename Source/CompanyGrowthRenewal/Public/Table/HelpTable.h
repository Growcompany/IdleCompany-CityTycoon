#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Enum/WidgetType.h"
#include "HelpTable.generated.h"

/**
 * 도움말 "?" 한 장 (DT_Help 행, RowName = HelpKey).
 * 수치 컬럼은 없다 ― 확률·비용·배율을 여기 적으면 밸런스를 고칠 때마다 도움말만 조용히 틀린다.
 * 숫자는 소유 시스템의 DT 에서 IHelpTableProvider 로 읽어 그린다.
 * 설계 = docs/superpowers/specs/2026-09-23-help-button-design.md §9
 */
USTRUCT(BlueprintType)
struct FHelpTable : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName HelpKey;

	// 대상 이름 (명사)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText Eyebrow;

	// 안내문 ("~습니다" 안내체). 표가 있는 화면에서는 표 위 캡션으로 같이 보인다
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText Body;

	// 이 화면의 숫자 화면이 이미 따로 있으면 그 위젯. 같은 표를 두 벌 만들면 둘이 서로 어긋난다
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EWidgetType LinkWidget = EWidgetType::None;
};
