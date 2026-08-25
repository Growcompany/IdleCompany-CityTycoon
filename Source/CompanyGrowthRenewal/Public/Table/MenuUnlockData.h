#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "MenuUnlockData.generated.h"

/**
 * 메뉴 버튼 해금 조건 DataTable 행
 * RowName = 버튼 이름 (예: "RankBtn", "SkinBtn")
 */
USTRUCT(BlueprintType)
struct FMenuUnlockData : public FTableRowBase
{
	GENERATED_BODY()

	// 해금에 필요한 최소 HQ 레벨 (0이면 항상 해금)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Menu")
	int32 RequiredHQLevel = 0;

	// 잠금 시 표시할 텍스트 (예: "HQ Lv.10 필요")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Menu")
	FText LockReasonText;
};
