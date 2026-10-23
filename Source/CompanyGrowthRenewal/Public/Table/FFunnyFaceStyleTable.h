#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "FFunnyFaceStyleTable.generated.h"

// StyleIndex 순서가 직원별 얼굴 배정의 기준이다.
USTRUCT(BlueprintType)
struct FFunnyFaceStyleTable : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	int32 StyleIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<UTexture2D> EyesAtlas;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<UTexture2D> MouthAtlas;
};
