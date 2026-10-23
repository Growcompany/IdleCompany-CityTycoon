#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Office/DecorationTypes.h"
#include "DecorationCatalogTable.generated.h"

/** 장식 카탈로그의 분류 이름, 소속, 표시 순서. */
USTRUCT(BlueprintType)
struct FDecorationCatalogRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) EDecorationCategory Category = EDecorationCategory::Furniture;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText CategoryLabel;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Group;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText GroupLabel;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 GroupOrder = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 CategoryOrder = 0;
};
