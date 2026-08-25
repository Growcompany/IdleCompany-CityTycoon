#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "ProfileImageData.generated.h"

/**
 * 프로필 이미지 DataTable 행
 * RowName = 이미지 ID (예: "100", "200")
 */
USTRUCT(BlueprintType)
struct FProfileImageData : public FTableRowBase
{
	GENERATED_BODY()

	// 프로필 이미지 ID
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Profile")
	int32 ImageID = 0;

	// 표시 이름
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Profile")
	FText DisplayName;

	// 아이콘 텍스처
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Profile")
	TSoftObjectPtr<UTexture2D> Icon;

	// 해금 조건 (0이면 기본 제공)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Profile")
	int32 RequiredHQLevel = 0;
};
