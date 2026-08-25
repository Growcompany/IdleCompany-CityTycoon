#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Enum/ChoiceArchetype.h"
#include "EventChoiceIconTable.generated.h"

/**
 * 이벤트 선택지 행동 아키타입 → 아이콘 텍스처 매핑.
 * 단일 진실 원천: DT_EventChoiceIcon (CSV: DataImport/DT_EventChoiceIcon_Import.csv)
 * Icon 은 TSoftObjectPtr — 쿠킹 시 폴더 등록 필요(/Game/.../UI/Textures/EventIcon).
 */
USTRUCT(BlueprintType)
struct FEventChoiceIconRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EventIcon")
	EChoiceArchetype Archetype = EChoiceArchetype::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EventIcon")
	TSoftObjectPtr<UTexture2D> Icon;
};
